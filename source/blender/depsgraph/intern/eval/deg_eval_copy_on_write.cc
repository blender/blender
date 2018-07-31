/*
 * ***** BEGIN GPL LICENSE BLOCK *****
 *
 * This program is free software; you can redistribute it and/or
 * modify it under the terms of the GNU General Public License
 * as published by the Free Software Foundation; either version 2
 * of the License, or (at your option) any later version.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with this program; if not, write to the Free Software Foundation,
 * Inc., 51 Franklin Street, Fifth Floor, Boston, MA 02110-1301, USA.
 *
 * The Original Code is Copyright (C) 20137Blender Foundation.
 * All rights reserved.
 *
 * Original Author: Sergey Sharybin
 * Contributor(s): None Yet
 *
 * ***** END GPL LICENSE BLOCK *****
 */


/** \file blender/depsgraph/intern/eval/deg_eval_copy_on_write.cc
 *  \ingroup depsgraph
 */

/* Enable special; trickery to treat nested owned IDs (such as nodetree of
 * material) to be handled in same way as "real" datablocks, even tho some
 * internal BKE routines doesn't treat them like that.
 *
 * TODO(sergey): Re-evaluate that after new ID handling is in place.
 */
#define NESTED_ID_NASTY_WORKAROUND

/* Silence warnings from copying deprecated fields. */
#define DNA_DEPRECATED_ALLOW

#include "intern/eval/deg_eval_copy_on_write.h"

#include <cstring>

#include "BLI_utildefines.h"
#include "BLI_listbase.h"
#include "BLI_threads.h"
#include "BLI_string.h"

#include "BKE_curve.h"
#include "BKE_global.h"
#include "BKE_idprop.h"
#include "BKE_layer.h"
#include "BKE_library.h"
#include "BKE_main.h"
#include "BKE_scene.h"

#include "DEG_depsgraph.h"

#include "MEM_guardedalloc.h"

extern "C" {
#include "DNA_ID.h"
#include "DNA_anim_types.h"
#include "DNA_armature_types.h"
#include "DNA_mesh_types.h"
#include "DNA_scene_types.h"
#include "DNA_object_types.h"
#include "DNA_particle_types.h"

#include "DRW_engine.h"

#ifdef NESTED_ID_NASTY_WORKAROUND
#  include "DNA_curve_types.h"
#  include "DNA_key_types.h"
#  include "DNA_lamp_types.h"
#  include "DNA_lattice_types.h"
#  include "DNA_linestyle_types.h"
#  include "DNA_material_types.h"
#  include "DNA_meta_types.h"
#  include "DNA_node_types.h"
#  include "DNA_texture_types.h"
#  include "DNA_world_types.h"
#endif

#include "BKE_action.h"
#include "BKE_animsys.h"
#include "BKE_armature.h"
#include "BKE_editmesh.h"
#include "BKE_library_query.h"
#include "BKE_object.h"
}

#include "intern/depsgraph.h"
#include "intern/builder/deg_builder_nodes.h"
#include "intern/nodes/deg_node.h"
#include "intern/nodes/deg_node_id.h"

namespace DEG {

#define DEBUG_PRINT if (G.debug & G_DEBUG_DEPSGRAPH_EVAL) printf

namespace {

#ifdef NESTED_ID_NASTY_WORKAROUND
union NestedIDHackTempStorage {
	Curve curve;
	FreestyleLineStyle linestyle;
	Lamp lamp;
	Lattice lattice;
	Material material;
	Mesh mesh;
	Scene scene;
	Tex tex;
	World world;
};

/* Set nested owned ID pointers to NULL. */
void nested_id_hack_discard_pointers(ID *id_cow)
{
	switch (GS(id_cow->name)) {
#  define SPECIAL_CASE(id_type, dna_type, field)  \
		case id_type:                             \
		{                                         \
			((dna_type *)id_cow)->field = NULL;   \
			break;                                \
		}

		SPECIAL_CASE(ID_LS, FreestyleLineStyle, nodetree)
		SPECIAL_CASE(ID_LA, Lamp, nodetree)
		SPECIAL_CASE(ID_MA, Material, nodetree)
		SPECIAL_CASE(ID_SCE, Scene, nodetree)
		SPECIAL_CASE(ID_TE, Tex, nodetree)
		SPECIAL_CASE(ID_WO, World, nodetree)

		SPECIAL_CASE(ID_CU, Curve, key)
		SPECIAL_CASE(ID_LT, Lattice, key)
		SPECIAL_CASE(ID_ME, Mesh, key)

		case ID_OB:
		{
			/* Clear the ParticleSettings pointer to prevent doubly-freeing it. */
			Object *ob = (Object *)id_cow;
			LISTBASE_FOREACH(ParticleSystem *, psys, &ob->particlesystem) {
				psys->part = NULL;
			}
			break;
		}
#  undef SPECIAL_CASE

		default:
			break;
	}
}

/* Set ID pointer of nested owned IDs (nodetree, key) to NULL.
 *
 * Return pointer to a new ID to be used.
 */
const ID *nested_id_hack_get_discarded_pointers(NestedIDHackTempStorage *storage,
                                                const ID *id)
{
	switch (GS(id->name)) {
#  define SPECIAL_CASE(id_type, dna_type, field, variable)  \
		case id_type:                                       \
		{                                                   \
			storage->variable = *(dna_type *)id;            \
			storage->variable.field = NULL;                 \
			return &storage->variable.id;                   \
		}

		SPECIAL_CASE(ID_LS, FreestyleLineStyle, nodetree, linestyle)
		SPECIAL_CASE(ID_LA, Lamp, nodetree, lamp)
		SPECIAL_CASE(ID_MA, Material, nodetree, material)
		SPECIAL_CASE(ID_SCE, Scene, nodetree, scene)
		SPECIAL_CASE(ID_TE, Tex, nodetree, tex)
		SPECIAL_CASE(ID_WO, World, nodetree, world)

		SPECIAL_CASE(ID_CU, Curve, key, curve)
		SPECIAL_CASE(ID_LT, Lattice, key, lattice)
		SPECIAL_CASE(ID_ME, Mesh, key, mesh)

#  undef SPECIAL_CASE

		default:
			break;
	}
	return id;
}

/* Set ID pointer of nested owned IDs (nodetree, key) to the original value. */
void nested_id_hack_restore_pointers(const ID *old_id, ID *new_id)
{
	if (new_id == NULL) {
		return;
	}
	switch (GS(old_id->name)) {
#  define SPECIAL_CASE(id_type, dna_type, field)    \
		case id_type:                               \
		{                                           \
			((dna_type *)(new_id))->field =         \
			        ((dna_type *)(old_id))->field;  \
			break;                                  \
		}

		SPECIAL_CASE(ID_LS, FreestyleLineStyle, nodetree)
		SPECIAL_CASE(ID_LA, Lamp, nodetree)
		SPECIAL_CASE(ID_MA, Material, nodetree)
		SPECIAL_CASE(ID_SCE, Scene, nodetree)
		SPECIAL_CASE(ID_TE, Tex, nodetree)
		SPECIAL_CASE(ID_WO, World, nodetree)

		SPECIAL_CASE(ID_CU, Curve, key)
		SPECIAL_CASE(ID_LT, Lattice, key)
		SPECIAL_CASE(ID_ME, Mesh, key)

#undef SPECIAL_CASE
		default:
			break;
	}
}

/* Remap pointer of nested owned IDs (nodetree. key) to the new ID values. */
void ntree_hack_remap_pointers(const Depsgraph *depsgraph, ID *id_cow)
{
	switch (GS(id_cow->name)) {
#  define SPECIAL_CASE(id_type, dna_type, field, field_type)                   \
		case id_type:                                                          \
		{                                                                      \
			dna_type *data = (dna_type *)id_cow;                               \
			if (data->field != NULL) {                                         \
				ID *ntree_id_cow = depsgraph->get_cow_id(&data->field->id);    \
				if (ntree_id_cow != NULL) {                                    \
					DEG_COW_PRINT("    Remapping datablock for %s: id_orig=%p id_cow=%p\n", \
					              data->field->id.name,                        \
					              data->field,                                 \
					              ntree_id_cow);                               \
					data->field = (field_type *)ntree_id_cow;                  \
				}                                                              \
			}                                                                  \
			break;                                                             \
		}

		SPECIAL_CASE(ID_LS, FreestyleLineStyle, nodetree, bNodeTree)
		SPECIAL_CASE(ID_LA, Lamp, nodetree, bNodeTree)
		SPECIAL_CASE(ID_MA, Material, nodetree, bNodeTree)
		SPECIAL_CASE(ID_SCE, Scene, nodetree, bNodeTree)
		SPECIAL_CASE(ID_TE, Tex, nodetree, bNodeTree)
		SPECIAL_CASE(ID_WO, World, nodetree, bNodeTree)

		SPECIAL_CASE(ID_CU, Curve, key, Key)
		SPECIAL_CASE(ID_LT, Lattice, key, Key)
		SPECIAL_CASE(ID_ME, Mesh, key, Key)

#undef SPECIAL_CASE
		default:
			break;
	}
}
#endif  /* NODETREE_NASTY_WORKAROUND */

struct ValidateData {
	bool is_valid;
};

/* Similar to generic id_copy() but does not require main and assumes pointer
 * is already allocated,
 */
bool id_copy_inplace_no_main(const ID *id, ID *newid)
{
	const ID *id_for_copy = id;

#ifdef NESTED_ID_NASTY_WORKAROUND
	NestedIDHackTempStorage id_hack_storage;
	id_for_copy = nested_id_hack_get_discarded_pointers(&id_hack_storage, id);
#endif

	bool result = BKE_id_copy_ex(NULL,
	                             (ID *)id_for_copy,
	                             &newid,
	                             (LIB_ID_CREATE_NO_MAIN |
	                              LIB_ID_CREATE_NO_USER_REFCOUNT |
	                              LIB_ID_CREATE_NO_ALLOCATE |
	                              LIB_ID_CREATE_NO_DEG_TAG |
	                              LIB_ID_COPY_CACHES),
	                             false);

#ifdef NESTED_ID_NASTY_WORKAROUND
	if (result) {
		nested_id_hack_restore_pointers(id, newid);
	}
#endif

	return result;
}

/* Similar to BKE_scene_copy() but does not require main and assumes pointer
 * is already allocated.
 */
bool scene_copy_inplace_no_main(const Scene *scene, Scene *new_scene)
{
	const ID *id_for_copy = &scene->id;

#ifdef NESTED_ID_NASTY_WORKAROUND
	NestedIDHackTempStorage id_hack_storage;
	id_for_copy = nested_id_hack_get_discarded_pointers(&id_hack_storage,
	                                                    &scene->id);
#endif

	bool result = BKE_id_copy_ex(NULL,
	                             id_for_copy,
	                             (ID **)&new_scene,
	                             LIB_ID_CREATE_NO_MAIN |
	                             LIB_ID_CREATE_NO_USER_REFCOUNT |
	                             LIB_ID_CREATE_NO_ALLOCATE |
	                             LIB_ID_CREATE_NO_DEG_TAG,
	                             false);

#ifdef NESTED_ID_NASTY_WORKAROUND
	if (result) {
		nested_id_hack_restore_pointers(&scene->id, &new_scene->id);
	}
#endif

	return result;
}

/* Check whether given ID is expanded or still a shallow copy. */
BLI_INLINE bool check_datablock_expanded(const ID *id_cow)
{
	return (id_cow->name[0] != '\0');
}

/* Those are datablocks which are not covered by dependency graph and hence
 * does not need any remapping or anything.
 *
 * TODO(sergey): How to make it more robust for the future, so we don't have
 * to maintain exception lists all over the code?
 */
static bool check_datablocks_copy_on_writable(const ID *id_orig)
{
	const ID_Type id_type = GS(id_orig->name);
	/* We shouldn't bother if copied ID is same as original one. */
	if (!deg_copy_on_write_is_needed(id_orig)) {
		return false;
	}
	return !ELEM(id_type, ID_BR,
	                      ID_LS,
	                      ID_PAL);
}

/* Callback for BKE_library_foreach_ID_link which remaps original ID pointer
 * with the one created by CoW system.
 */

struct RemapCallbackUserData {
	/* Dependency graph for which remapping is happening. */
	const Depsgraph *depsgraph;
	/* Create placeholder for ID nodes for cases when we need to remap original
	 * ID to it[s CoW version but we don't have required ID node yet.
	 *
	 * This happens when expansion happens a ta construction time.
	 */
	DepsgraphNodeBuilder *node_builder;
	bool create_placeholders;
};

int foreach_libblock_remap_callback(void *user_data_v,
                                    ID *id_self,
                                    ID **id_p,
                                    int /*cb_flag*/)
{
	if (*id_p == NULL) {
		return IDWALK_RET_NOP;
	}
	RemapCallbackUserData *user_data = (RemapCallbackUserData *)user_data_v;
	const Depsgraph *depsgraph = user_data->depsgraph;
	ID *id_orig = *id_p;
	if (check_datablocks_copy_on_writable(id_orig)) {
		ID *id_cow;
		if (user_data->create_placeholders) {
			/* Special workaround to stop creating temp datablocks for
			 * objects which are coming from scene's collection and which
			 * are never linked to any of layers.
			 *
			 * TODO(sergey): Ideally we need to tell ID looper to ignore
			 * those or at least make it more reliable check where the
			 * pointer is coming from.
			 */
			const ID_Type id_type = GS(id_orig->name);
			const ID_Type id_type_self = GS(id_self->name);
			if (id_type == ID_OB && id_type_self == ID_SCE) {
				IDDepsNode *id_node = depsgraph->find_id_node(id_orig);
				if (id_node == NULL) {
					id_cow = id_orig;
				}
				else {
					id_cow = id_node->id_cow;
				}
			}
			else {
				id_cow = user_data->node_builder->ensure_cow_id(id_orig);
			}
		}
		else {
			id_cow = depsgraph->get_cow_id(id_orig);
		}
		BLI_assert(id_cow != NULL);
		DEG_COW_PRINT("    Remapping datablock for %s: id_orig=%p id_cow=%p\n",
		              id_orig->name, id_orig, id_cow);
		*id_p = id_cow;
	}
	return IDWALK_RET_NOP;
}

void update_armature_edit_mode_pointers(const Depsgraph * /*depsgraph*/,
                                        const ID *id_orig, ID *id_cow)
{
	const bArmature *armature_orig = (const bArmature *)id_orig;
	bArmature *armature_cow = (bArmature *)id_cow;
	armature_cow->edbo = armature_orig->edbo;
}

void update_curve_edit_mode_pointers(const Depsgraph * /*depsgraph*/,
                                     const ID *id_orig, ID *id_cow)
{
	const Curve *curve_orig = (const Curve *)id_orig;
	Curve *curve_cow = (Curve *)id_cow;
	curve_cow->editnurb = curve_orig->editnurb;
	curve_cow->editfont = curve_orig->editfont;
}

void update_mball_edit_mode_pointers(const Depsgraph * /*depsgraph*/,
                                     const ID *id_orig, ID *id_cow)
{
	const MetaBall *mball_orig = (const MetaBall *)id_orig;
	MetaBall *mball_cow = (MetaBall *)id_cow;
	mball_cow->editelems = mball_orig->editelems;
}

void update_lattice_edit_mode_pointers(const Depsgraph * /*depsgraph*/,
                                     const ID *id_orig, ID *id_cow)
{
	const Lattice *lt_orig = (const Lattice *)id_orig;
	Lattice *lt_cow = (Lattice *)id_cow;
	lt_cow->editlatt = lt_orig->editlatt;
}

void update_mesh_edit_mode_pointers(const Depsgraph *depsgraph,
                                    const ID *id_orig, ID *id_cow)
{
	/* For meshes we need to update edit_btmesh to make it to point
	 * to the CoW version of object.
	 *
	 * This is kind of confusing, because actual bmesh is not owned by
	 * the CoW object, so need to be accurate about using link from
	 * edit_btmesh to object.
	 */
	const Mesh *mesh_orig = (const Mesh *)id_orig;
	Mesh *mesh_cow = (Mesh *)id_cow;
	if (mesh_orig->edit_btmesh == NULL) {
		return;
	}
	mesh_cow->edit_btmesh = (BMEditMesh *)MEM_dupallocN(mesh_orig->edit_btmesh);
	mesh_cow->edit_btmesh->ob =
	    (Object *)depsgraph->get_cow_id(&mesh_orig->edit_btmesh->ob->id);
	mesh_cow->edit_btmesh->derivedFinal = NULL;
	mesh_cow->edit_btmesh->derivedCage = NULL;
}

/* Edit data is stored and owned by original datablocks, copied ones
 * are simply referencing to them.
 */
void update_edit_mode_pointers(const Depsgraph *depsgraph,
                               const ID *id_orig, ID *id_cow)
{
	const ID_Type type = GS(id_orig->name);
	switch (type) {
		case ID_AR:
			update_armature_edit_mode_pointers(depsgraph, id_orig, id_cow);
			break;
		case ID_ME:
			update_mesh_edit_mode_pointers(depsgraph, id_orig, id_cow);
			break;
		case ID_CU:
			update_curve_edit_mode_pointers(depsgraph, id_orig, id_cow);
			break;
		case ID_MB:
			update_mball_edit_mode_pointers(depsgraph, id_orig, id_cow);
			break;
		case ID_LT:
			update_lattice_edit_mode_pointers(depsgraph, id_orig, id_cow);
			break;
		default:
			break;
	}
}

void update_particle_system_orig_pointers(const Object *object_orig,
                                          Object *object_cow)
{
	ParticleSystem *psys_cow =
	        (ParticleSystem *) object_cow->particlesystem.first;
	ParticleSystem *psys_orig =
	        (ParticleSystem *) object_orig->particlesystem.first;
	while (psys_orig != NULL) {
		psys_cow->orig_psys = psys_orig;
		psys_cow = psys_cow->next;
		psys_orig = psys_orig->next;
	}
}

void update_pose_orig_pointers(const bPose *pose_orig, bPose *pose_cow)
{
	bPoseChannel *pchan_cow = (bPoseChannel *) pose_cow->chanbase.first;
	bPoseChannel *pchan_orig = (bPoseChannel *) pose_orig->chanbase.first;
	while (pchan_orig != NULL) {
		pchan_cow->orig_pchan = pchan_orig;
		pchan_cow = pchan_cow->next;
		pchan_orig = pchan_orig->next;
	}
}

/* Do some special treatment of data transfer from original ID to it's
 * CoW complementary part.
 *
 * Only use for the newly created CoW datablocks.
 */
void update_special_pointers(const Depsgraph *depsgraph,
                             const ID *id_orig, ID *id_cow)
{
	const ID_Type type = GS(id_orig->name);
	switch (type) {
		case ID_OB:
		{
			/* Ensure we don't drag someone's else derived mesh to the
			 * new copy of the object.
			 */
			Object *object_cow = (Object *)id_cow;
			const Object *object_orig = (const Object *)id_orig;
			BLI_assert(object_cow->derivedFinal == NULL);
			BLI_assert(object_cow->derivedDeform == NULL);
			object_cow->mode = object_orig->mode;
			object_cow->sculpt = object_orig->sculpt;
			if (object_cow->type == OB_MESH) {
				object_cow->runtime.mesh_orig = (Mesh *)object_cow->data;
			}
			if (object_cow->type == OB_ARMATURE) {
				const bArmature *armature_orig = (bArmature *)object_orig->data;
				bArmature *armature_cow = (bArmature *)object_cow->data;
				BKE_pose_remap_bone_pointers(armature_cow, object_cow->pose);
				if (armature_orig->edbo == NULL) {
					update_pose_orig_pointers(object_orig->pose,
					                          object_cow->pose);
				}
			}
			update_particle_system_orig_pointers(object_orig, object_cow);
			break;
		}
		case ID_SCE:
		{
			Scene *scene_cow = (Scene *)id_cow;
			const Scene *scene_orig = (const Scene *)id_orig;
			scene_cow->eevee.light_cache = scene_orig->eevee.light_cache;
			break;
		}
		default:
			break;
	}
	update_edit_mode_pointers(depsgraph, id_orig, id_cow);
	BKE_animsys_update_driver_array(id_cow);
}

/* This callback is used to validate that all nested ID datablocks are
 * properly expanded.
 */
int foreach_libblock_validate_callback(void *user_data,
                                       ID * /*id_self*/,
                                       ID **id_p,
                                       int /*cb_flag*/)
{
	ValidateData *data = (ValidateData *)user_data;
	if (*id_p != NULL) {
		if (!check_datablock_expanded(*id_p)) {
			data->is_valid = false;
			/* TODO(sergey): Store which is is not valid? */
		}
	}
	return IDWALK_RET_NOP;
}

}  // namespace

/* Actual implementation of logic which "expands" all the data which was not
 * yet copied-on-write.
 *
 * NOTE: Expects that CoW datablock is empty.
 */
ID *deg_expand_copy_on_write_datablock(const Depsgraph *depsgraph,
                                       const IDDepsNode *id_node,
                                       DepsgraphNodeBuilder *node_builder,
                                       bool create_placeholders)
{
	const ID *id_orig = id_node->id_orig;
	ID *id_cow = id_node->id_cow;
	const int id_cow_recalc = id_cow->recalc;
	/* No need to expand such datablocks, their copied ID is same as original
	 * one already.
	 */
	if (!deg_copy_on_write_is_needed(id_orig)) {
		return id_cow;
	}
	DEG_COW_PRINT("Expanding datablock for %s: id_orig=%p id_cow=%p\n",
	              id_orig->name, id_orig, id_cow);
	/* Sanity checks. */
	/* NOTE: Disabled for now, conflicts when re-using evaluated datablock when
	 * rebuilding dependencies.
	 */
	if (check_datablock_expanded(id_cow) && create_placeholders) {
		deg_free_copy_on_write_datablock(id_cow);
	}
	// BLI_assert(check_datablock_expanded(id_cow) == false);
	/* Copy data from original ID to a copied version. */
	/* TODO(sergey): Avoid doing full ID copy somehow, make Mesh to reference
	 * original geometry arrays for until those are modified.
	 */
	/* TODO(sergey): We do some trickery with temp bmain and extra ID pointer
	 * just to be able to use existing API. Ideally we need to replace this with
	 * in-place copy from existing datablock to a prepared memory.
	 *
	 * NOTE: We don't use BKE_main_{new,free} because:
	 * - We don't want heap-allocations here.
	 * - We don't want bmain's content to be freed when main is freed.
	 */
	bool done = false;
	/* First we handle special cases which are not covered by id_copy() yet.
	 * or cases where we want to do something smarter than simple datablock
	 * copy.
	 */
	const ID_Type id_type = GS(id_orig->name);
	switch (id_type) {
		case ID_SCE:
		{
			done = scene_copy_inplace_no_main((Scene *)id_orig, (Scene *)id_cow);
			break;
		}
		case ID_ME:
		{
			/* TODO(sergey): Ideally we want to handle meshes in a special
			 * manner here to avoid initial copy of all the geometry arrays.
			 */
			break;
		}
		default:
			break;
	}
	if (!done) {
		done = id_copy_inplace_no_main(id_orig, id_cow);
	}
	if (!done) {
		BLI_assert(!"No idea how to perform CoW on datablock");
	}
	/* Update pointers to nested ID datablocks. */
	DEG_COW_PRINT("  Remapping ID links for %s: id_orig=%p id_cow=%p\n",
	              id_orig->name, id_orig, id_cow);

#ifdef NESTED_ID_NASTY_WORKAROUND
	ntree_hack_remap_pointers(depsgraph, id_cow);
#endif
	/* Do it now, so remapping will understand that possibly remapped self ID
	 * is not to be remapped again.
	 */
	deg_tag_copy_on_write_id(id_cow, id_orig);
	/* Perform remapping of the nodes. */
	RemapCallbackUserData user_data = {NULL};
	user_data.depsgraph = depsgraph;
	user_data.node_builder = node_builder;
	user_data.create_placeholders = create_placeholders;
	BKE_library_foreach_ID_link(NULL,
	                            id_cow,
	                            foreach_libblock_remap_callback,
	                            (void *)&user_data,
	                            IDWALK_NOP);
	/* Correct or tweak some pointers which are not taken care by foreach
	 * from above.
	 */
	update_special_pointers(depsgraph, id_orig, id_cow);
	id_cow->recalc = id_orig->recalc | id_cow_recalc;
	return id_cow;
}

/* NOTE: Depsgraph is supposed to have ID node already. */
ID *deg_expand_copy_on_write_datablock(const Depsgraph *depsgraph,
                                       ID *id_orig,
                                       DepsgraphNodeBuilder *node_builder,
                                       bool create_placeholders)
{
	DEG::IDDepsNode *id_node = depsgraph->find_id_node(id_orig);
	BLI_assert(id_node != NULL);
	return deg_expand_copy_on_write_datablock(depsgraph,
	                                          id_node,
	                                          node_builder,
	                                          create_placeholders);
}

static void deg_update_copy_on_write_animation(const Depsgraph *depsgraph,
                                               const IDDepsNode *id_node)
{
	DEG_debug_print_eval((::Depsgraph *)depsgraph,
	                     __func__,
	                     id_node->id_orig->name,
	                     id_node->id_cow);
	BKE_animdata_copy_id(NULL, id_node->id_cow, id_node->id_orig, false, false);
	RemapCallbackUserData user_data = {NULL};
	user_data.depsgraph = depsgraph;
	BKE_library_foreach_ID_link(NULL,
	                            id_node->id_cow,
	                            foreach_libblock_remap_callback,
	                            (void *)&user_data,
	                            IDWALK_NOP);
}

typedef struct ObjectRuntimeBackup {
	Object_Runtime runtime;
	short base_flag;
} ObjectRuntimeBackup;

/* Make a backup of object's evaluation runtime data, additionally
 * make object to be safe for free without invalidating backed up
 * pointers.
 */
static void deg_backup_object_runtime(
        Object *object,
        ObjectRuntimeBackup *object_runtime_backup)
{
	/* Store evaluated mesh and curve_cache, and make sure we don't free it. */
	Mesh *mesh_eval = object->runtime.mesh_eval;
	object_runtime_backup->runtime = object->runtime;
	BKE_object_runtime_reset(object);
	/* Object update will override actual object->data to an evaluated version.
	 * Need to make sure we don't have data set to evaluated one before free
	 * anything.
	 */
	if (mesh_eval != NULL && object->data == mesh_eval) {
		object->data = object->runtime.mesh_orig;
	}
	/* Make a backup of base flags. */
	object_runtime_backup->base_flag = object->base_flag;
}

static void deg_restore_object_runtime(
        Object *object,
        const ObjectRuntimeBackup *object_runtime_backup)
{
	Mesh *mesh_orig = object->runtime.mesh_orig;
	object->runtime = object_runtime_backup->runtime;
	object->runtime.mesh_orig = mesh_orig;
	if (object->type == OB_MESH && object->runtime.mesh_eval != NULL) {
		if (object->id.recalc & ID_RECALC_GEOMETRY) {
			/* If geometry is tagged for update it means, that part of
			 * evaluated mesh are not valid anymore. In this case we can not
			 * have any "persistent" pointers to point to an invalid data.
			 *
			 * We restore object's data datablock to an original copy of
			 * that datablock.
			 */
			object->data = mesh_orig;
		}
		else {
			Mesh *mesh_eval = object->runtime.mesh_eval;
			/* Do same thing as object update: override actual object data
			 * pointer with evaluated datablock.
			 */
			object->data = mesh_eval;
			/* Evaluated mesh simply copied edit_btmesh pointer from
			 * original mesh during update, need to make sure no dead
			 * pointers are left behind.
			*/
			mesh_eval->edit_btmesh = mesh_orig->edit_btmesh;
		}
	}
	object->base_flag = object_runtime_backup->base_flag;
}

ID *deg_update_copy_on_write_datablock(const Depsgraph *depsgraph,
                                       const IDDepsNode *id_node)
{
	const ID *id_orig = id_node->id_orig;
	const ID_Type id_type = GS(id_orig->name);
	ID *id_cow = id_node->id_cow;
	/* Similar to expansion, no need to do anything here. */
	if (!deg_copy_on_write_is_needed(id_orig)) {
		return id_cow;
	}
	/* For the rest if datablock types we use simple logic:
	 * - Free previously expanded data, if any.
	 * - Perform full datablock copy.
	 *
	 * Note that we never free GPU materials from here since that's not
	 * safe for threading and GPU materials are likely to be re-used.
	 */
	/* TODO(sergey): Either move this to an utility function or redesign
	 * Copy-on-Write components in a way that only needed parts are being
	 * copied over.
	 */
	/* TODO(sergey): Wrap GPU material backup and object runtime backup to a
	 * generic backup structure.
	 */
	ListBase gpumaterial_backup;
	ListBase *gpumaterial_ptr = NULL;
	DrawDataList drawdata_backup;
	DrawDataList *drawdata_ptr = NULL;
	ObjectRuntimeBackup object_runtime_backup = {NULL};
	if (check_datablock_expanded(id_cow)) {
		switch (id_type) {
			case ID_MA:
			{
				Material *material = (Material *)id_cow;
				gpumaterial_ptr = &material->gpumaterial;
				break;
			}
			case ID_WO:
			{
				World *world = (World *)id_cow;
				gpumaterial_ptr = &world->gpumaterial;
				break;
			}
			case ID_NT:
			{
				/* Node trees should try to preserve their socket pointers
				 * as much as possible. This is due to UBOs code in GPU,
				 * which references sockets from trees.
				 *
				 * These flags CURRENTLY don't need full datablock update,
				 * everything is done by node tree update function which
				 * only copies socket values.
				 */
				const int ignore_flag = (ID_RECALC_DRAW |
				                         ID_RECALC_ANIMATION |
				                         ID_RECALC_COPY_ON_WRITE);
				if ((id_cow->recalc & ~ignore_flag) == 0) {
					deg_update_copy_on_write_animation(depsgraph, id_node);
					return id_cow;
				}
				break;
			}
			case ID_OB:
			{
				Object *ob = (Object *)id_cow;
				deg_backup_object_runtime(ob, &object_runtime_backup);
				break;
			}
			default:
				break;
		}
		if (gpumaterial_ptr != NULL) {
			gpumaterial_backup = *gpumaterial_ptr;
			gpumaterial_ptr->first = gpumaterial_ptr->last = NULL;
		}
		drawdata_ptr = DRW_drawdatalist_from_id(id_cow);
		if (drawdata_ptr != NULL) {
			drawdata_backup = *drawdata_ptr;
			drawdata_ptr->first = drawdata_ptr->last = NULL;
		}
	}
	deg_free_copy_on_write_datablock(id_cow);
	deg_expand_copy_on_write_datablock(depsgraph, id_node);
	/* Restore GPU materials. */
	if (gpumaterial_ptr != NULL) {
		*gpumaterial_ptr = gpumaterial_backup;
	}
	/* Restore DrawData. */
	if (drawdata_ptr != NULL) {
		*drawdata_ptr = drawdata_backup;
	}
	if (id_type == ID_OB) {
		deg_restore_object_runtime((Object *)id_cow, &object_runtime_backup);
	}
	return id_cow;
}

/* NOTE: Depsgraph is supposed to have ID node already. */
ID *deg_update_copy_on_write_datablock(const Depsgraph *depsgraph,
                                       ID *id_orig)
{
	DEG::IDDepsNode *id_node = depsgraph->find_id_node(id_orig);
	BLI_assert(id_node != NULL);
	return deg_update_copy_on_write_datablock(depsgraph, id_node);
}

namespace {

void discard_armature_edit_mode_pointers(ID *id_cow)
{
	bArmature *armature_cow = (bArmature *)id_cow;
	armature_cow->edbo = NULL;
}

void discard_curve_edit_mode_pointers(ID *id_cow)
{
	Curve *curve_cow = (Curve *)id_cow;
	curve_cow->editnurb = NULL;
	curve_cow->editfont = NULL;
}

void discard_mball_edit_mode_pointers(ID *id_cow)
{
	MetaBall *mball_cow = (MetaBall *)id_cow;
	mball_cow->editelems = NULL;
}

void discard_lattice_edit_mode_pointers(ID *id_cow)
{
	Lattice *lt_cow = (Lattice *)id_cow;
	lt_cow->editlatt = NULL;
}

void discard_mesh_edit_mode_pointers(ID *id_cow)
{
	Mesh *mesh_cow = (Mesh *)id_cow;
	if (mesh_cow->edit_btmesh == NULL) {
		return;
	}
	BKE_editmesh_free_derivedmesh(mesh_cow->edit_btmesh);
	MEM_freeN(mesh_cow->edit_btmesh);
	mesh_cow->edit_btmesh = NULL;
}

void discard_scene_pointers(ID *id_cow)
{
	Scene *scene_cow = (Scene *)id_cow;
	scene_cow->eevee.light_cache = NULL;
}

/* NULL-ify all edit mode pointers which points to data from
 * original object.
 */
void discard_edit_mode_pointers(ID *id_cow)
{
	const ID_Type type = GS(id_cow->name);
	switch (type) {
		case ID_AR:
			discard_armature_edit_mode_pointers(id_cow);
			break;
		case ID_ME:
			discard_mesh_edit_mode_pointers(id_cow);
			break;
		case ID_CU:
			discard_curve_edit_mode_pointers(id_cow);
			break;
		case ID_MB:
			discard_mball_edit_mode_pointers(id_cow);
			break;
		case ID_LT:
			discard_lattice_edit_mode_pointers(id_cow);
			break;
		case ID_SCE:
			/* Not really edit mode but still needs to run before
			 * BKE_libblock_free_datablock() */
			discard_scene_pointers(id_cow);
			break;
		default:
			break;
	}
}

}  // namespace

/* Free content of the CoW datablock
 * Notes:
 * - Does not recurs into nested ID datablocks.
 * - Does not free datablock itself.
 */
void deg_free_copy_on_write_datablock(ID *id_cow)
{
	if (!check_datablock_expanded(id_cow)) {
		/* Actual content was never copied on top of CoW block, we have
		 * nothing to free.
		 */
		return;
	}
	const ID_Type type = GS(id_cow->name);
#ifdef NESTED_ID_NASTY_WORKAROUND
	nested_id_hack_discard_pointers(id_cow);
#endif
	switch (type) {
		case ID_OB:
		{
			/* TODO(sergey): This workaround is only to prevent free derived
			 * caches from modifying object->data. This is currently happening
			 * due to mesh/curve datablock boundbox tagging dirty.
			 */
			Object *ob_cow = (Object *)id_cow;
			ob_cow->data = NULL;
			ob_cow->sculpt = NULL;
			break;
		}
		default:
			break;
	}
	discard_edit_mode_pointers(id_cow);
	BKE_libblock_free_datablock(id_cow, 0);
	BKE_libblock_free_data(id_cow, false);
	/* Signal datablock as not being expanded. */
	id_cow->name[0] = '\0';
}

void deg_evaluate_copy_on_write(struct ::Depsgraph *graph,
                                const IDDepsNode *id_node)
{
	const DEG::Depsgraph *depsgraph = reinterpret_cast<const DEG::Depsgraph *>(graph);
	DEG_debug_print_eval(graph, __func__, id_node->id_orig->name, id_node->id_cow);
	if (id_node->id_orig == &depsgraph->scene->id) {
		/* NOTE: This is handled by eval_ctx setup routines, which
		 * ensures scene and view layer pointers are valid.
		 */
		return;
	}
	deg_update_copy_on_write_datablock(depsgraph, id_node);
}

bool deg_validate_copy_on_write_datablock(ID *id_cow)
{
	if (id_cow == NULL) {
		return false;
	}
	ValidateData data;
	data.is_valid = true;
	BKE_library_foreach_ID_link(NULL,
	                            id_cow,
	                            foreach_libblock_validate_callback,
	                            &data,
	                            IDWALK_NOP);
	return data.is_valid;
}

void deg_tag_copy_on_write_id(ID *id_cow, const ID *id_orig)
{
	BLI_assert(id_cow != id_orig);
	BLI_assert((id_orig->tag & LIB_TAG_COPIED_ON_WRITE) == 0);
	id_cow->tag |= LIB_TAG_COPIED_ON_WRITE;
	id_cow->orig_id = (ID *)id_orig;
}

bool deg_copy_on_write_is_expanded(const ID *id_cow)
{
	return check_datablock_expanded(id_cow);
}

bool deg_copy_on_write_is_needed(const ID *id_orig)
{
	const ID_Type id_type = GS(id_orig->name);
	return !ELEM(id_type, ID_IM);
}

}  // namespace DEG
