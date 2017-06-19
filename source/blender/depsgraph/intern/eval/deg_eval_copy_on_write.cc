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


/** \file blender/depsgraph/intern/eval/deg_eval_copy_on_write.h
 *  \ingroup depsgraph
 */

/* Enable special; trickery to treat nested owned IDs (such as nodetree of
 * material) to be handled in same way as "real" datablocks, even tho some
 * internal BKE routines doesn't treat them like that.
 *
 * TODO(sergey): Re-evaluate that after new ID handling is in place.
 */
#define NESTED_ID_NASTY_WORKAROUND

#include "intern/eval/deg_eval_copy_on_write.h"

#include <cstring>

#include "BLI_utildefines.h"
#include "BLI_threads.h"

#include "BKE_global.h"
#include "BKE_layer.h"
#include "BKE_library.h"
#include "BKE_main.h"
#include "BKE_scene.h"

#include "DEG_depsgraph.h"

#include "MEM_guardedalloc.h"

extern "C" {
#include "DNA_ID.h"
#include "DNA_mesh_types.h"
#include "DNA_scene_types.h"
#include "DNA_object_types.h"

#ifdef NESTED_ID_NASTY_WORKAROUND
#  include "DNA_key_types.h"
#  include "DNA_lamp_types.h"
#  include "DNA_linestyle_types.h"
#  include "DNA_material_types.h"
#  include "DNA_mesh_types.h"
#  include "DNA_node_types.h"
#  include "DNA_scene_types.h"
#  include "DNA_texture_types.h"
#  include "DNA_world_types.h"
#endif

#include "BKE_editmesh.h"
#include "BKE_library_query.h"
}

#include "intern/depsgraph.h"
#include "intern/nodes/deg_node.h"

namespace DEG {

#define DEBUG_PRINT if (G.debug & G_DEBUG_DEPSGRAPH) printf

namespace {

#ifdef NESTED_ID_NASTY_WORKAROUND
union NestedIDHackTempStorage {
	FreestyleLineStyle linestyle;
	Lamp lamp;
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

		SPECIAL_CASE(ID_ME, Mesh, key)

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

/* Similar to generic id_copy() but does not require main.
 *
 * TODO(sergey): Get rid of this once T51804 is handled.
 */
bool id_copy_no_main(const ID *id, ID **newid)
{
	const ID *id_for_copy = id;
	Main temp_bmain = {0};
	SpinLock lock;
	temp_bmain.lock = (MainLock *)&lock;
	BLI_spin_init(&lock);

#ifdef NESTED_ID_NASTY_WORKAROUND
	NestedIDHackTempStorage id_hack_storage;
	id_for_copy = nested_id_hack_get_discarded_pointers(&id_hack_storage, id);
#endif

	bool result = id_copy(&temp_bmain, (ID *)id_for_copy, newid, false);

#ifdef NESTED_ID_NASTY_WORKAROUND
	if (result) {
		nested_id_hack_restore_pointers(id, *newid);
	}
#endif

	BLI_spin_end(&lock);
	return result;
}

/* Similar to BKE_scene_copy() but does not require main.
 *
 * TODO(sergey): Get rid of this once T51804 is handled.
 */
Scene *scene_copy_no_main(Scene *scene)
{
	const ID *id_for_copy = &scene->id;
	Main temp_bmain = {0};
	SpinLock lock;
	temp_bmain.lock = (MainLock *)&lock;
	BLI_spin_init(&lock);

#ifdef NESTED_ID_NASTY_WORKAROUND
	NestedIDHackTempStorage id_hack_storage;
	id_for_copy = nested_id_hack_get_discarded_pointers(&id_hack_storage,
	                                                    &scene->id);
#endif

	Scene *new_scene = BKE_scene_copy(&temp_bmain,
	                                  (Scene *)id_for_copy,
	                                  SCE_COPY_LINK_OB);

#ifdef NESTED_ID_NASTY_WORKAROUND
	nested_id_hack_restore_pointers(&scene->id, &new_scene->id);
#endif

	BLI_spin_end(&lock);
	return new_scene;
}

/* Callback for BKE_library_foreach_ID_link which remaps original ID pointer
 * with the one created by CoW system.
 */
int foreach_libblock_remap_callback(void *user_data,
                                    ID * /*id_self*/,
                                    ID **id_p,
                                    int /*cb_flag*/)
{
	Depsgraph *depsgraph = (Depsgraph *)user_data;
	if (*id_p != NULL) {
		const ID *id_orig = *id_p;
		ID *id_cow = depsgraph->get_cow_id(id_orig);
		if (id_cow != NULL) {
			DEG_COW_PRINT("    Remapping datablock for %s: id_orig=%p id_cow=%p\n",
			              id_orig->name, id_orig, id_cow);
			*id_p = id_cow;
		}
	}
	return IDWALK_RET_NOP;
}

/* Check whether given ID is expanded or still a shallow copy. */
BLI_INLINE bool check_datablock_expanded(ID *id_cow)
{
	return (id_cow->name[0] != '\0');
}

/* Do some special treatment of data transfer from original ID to it's
 * CoW complementary part.
 *
 * Only use for the newly created CoW datablocks.
 */
void update_special_pointers(const Depsgraph *depsgraph,
                             const ID *id_orig, ID *id_cow)
{
	const short type = GS(id_orig->name);
	switch (type) {
		case ID_OB:
		{
			/* Ensure we don't drag someone's else derived mesh to the
			 * new copy of the object.
			 */
			Object *object_cow = (Object *)id_cow;
			(void) object_cow;  /* Ignored for release builds. */
			BLI_assert(object_cow->derivedFinal == NULL);
			BLI_assert(object_cow->derivedDeform == NULL);
			break;
		}
		case ID_ME:
		{
			/* For meshes we need to update edit_brtmesh to make it to point
			 * to the CoW version of object.
			 *
			 * This is kind of confusing, because actual bmesh is not owned by
			 * the CoW object, so need to be accurate about using link from
			 * edit_btmesh to object.
			 */
			const Mesh *mesh_orig = (const Mesh *)id_orig;
			Mesh *mesh_cow = (Mesh *)id_cow;
			if (mesh_orig->edit_btmesh != NULL) {
				mesh_cow->edit_btmesh = (BMEditMesh *)MEM_dupallocN(mesh_orig->edit_btmesh);
				mesh_cow->edit_btmesh->ob =
				        (Object *)depsgraph->get_cow_id(&mesh_orig->edit_btmesh->ob->id);
				mesh_cow->edit_btmesh->derivedFinal = NULL;
				mesh_cow->edit_btmesh->derivedCage = NULL;
			}
			break;
		}
		case ID_SCE:
		{
			const Scene *scene_orig = (const Scene *)id_orig;
			Scene *scene_cow = (Scene *)id_cow;
			if (scene_orig->obedit != NULL) {
				scene_cow->obedit = (Object *)depsgraph->get_cow_id(&scene_orig->obedit->id);
			}
			else {
				scene_cow->obedit = NULL;
			}
			break;
		}
	}
}

/* Update copy-on-write version of datablock from it's original ID without re-building
 * the whole datablock from scratch.
 *
 * Used for such special cases as scene collections and armatures, which can not use full
 * re-alloc due to pointers used as function bindings.
 */
void update_copy_on_write_datablock(const Depsgraph *depsgraph,
                                    const ID *id_orig, ID *id_cow)
{
	if (GS(id_orig->name) == ID_SCE) {
		const Scene *scene_orig = (const Scene *)id_orig;
		Scene *scene_cow = (Scene *)id_cow;
		// Some non-pointer data sync, current frame for now.
		// TODO(sergey): Are we missing something here?
		scene_cow->r.cfra = scene_orig->r.cfra;
		scene_cow->r.subframe = scene_orig->r.subframe;
		// Update bases.
		const SceneLayer *sl_orig = (SceneLayer *)scene_orig->render_layers.first;
		SceneLayer *sl_cow = (SceneLayer *)scene_cow->render_layers.first;
		while (sl_orig != NULL) {
			// Update pointers to active base.
			if (sl_orig->basact == NULL) {
				sl_cow->basact = NULL;
			}
			else {
				const Object *obact_orig = sl_orig->basact->object;
				Object *obact_cow = (Object *)depsgraph->get_cow_id(&obact_orig->id);
				sl_cow->basact = BKE_scene_layer_base_find(sl_cow, obact_cow);
			}
			// Update base flags.
			//
			// TODO(sergey): We should probably check visibled/selectabled
			// flag here?
			const Base *base_orig = (Base *)sl_orig->object_bases.first;
			Base *base_cow = (Base *)sl_cow->object_bases.first;;
			while (base_orig != NULL) {
				base_cow->flag = base_orig->flag;
				base_orig = base_orig->next;
				base_cow = base_cow->next;
			}
			sl_orig = sl_orig->next;
			sl_cow = sl_cow->next;
		}
		// Update edit object pointer.
		if (scene_orig->obedit != NULL) {
			scene_cow->obedit = (Object *)depsgraph->get_cow_id(&scene_orig->obedit->id);
		}
		else {
			scene_cow->obedit = NULL;
		}
		// TODO(sergey): Things which are still missing here:
		// - Active render engine.
		// - Something else?
	}
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
			/* TODO(sergey_: Store which is is not valid? */
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
                                       const IDDepsNode *id_node)
{
	const ID *id_orig = id_node->id_orig;
	ID *id_cow = id_node->id_cow;
	DEG_COW_PRINT("Expanding datablock for %s: id_orig=%p id_cow=%p\n",
	              id_orig->name, id_orig, id_cow);
	/* Sanity checks. */
	BLI_assert(check_datablock_expanded(id_cow) == false);
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
	const short type = GS(id_orig->name);
	switch (type) {
		case ID_SCE:
		{
			Scene *new_scene = scene_copy_no_main((Scene *)id_orig);
			*(Scene *)id_cow = *new_scene;
			MEM_freeN(new_scene);
			done = true;
			break;
		}
		case ID_ME:
		{
			/* TODO(sergey): Ideally we want to handle meshes in a special
			 * manner here to avoid initial copy of all the geometry arrays.
			 */
			break;
		}
	}
	if (!done) {
		ID *newid;
		if (id_copy_no_main(id_orig, &newid)) {
			/* We copy contents of new ID to our CoW placeholder and free ID memory
			 * returned by id_copy().
			 *
			 * TODO(sergey): We can avoid having extra ID allocation here if we'll
			 * have some smarter id_copy() which can use externally allocated memory.
			 */
			const size_t size = BKE_libblock_get_alloc_info(GS(newid->name), NULL);
			memcpy(id_cow, newid, size);
			MEM_freeN(newid);
			done = true;
		}
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

	BKE_library_foreach_ID_link(NULL,
	                            id_cow,
	                            foreach_libblock_remap_callback,
	                            (void *)depsgraph,
	                            IDWALK_NOP);
	/* Correct or tweak some pointers which are not taken care by foreach
	 * from above.
	 */
	update_special_pointers(depsgraph, id_orig, id_cow);
	return id_cow;
}

/* NOTE: Depsgraph is supposed to have ID node already. */
ID *deg_expand_copy_on_write_datablock(const Depsgraph *depsgraph, ID *id_orig)
{
	DEG::IDDepsNode *id_node = depsgraph->find_id_node(id_orig);
	BLI_assert(id_node != NULL);
	return deg_expand_copy_on_write_datablock(depsgraph, id_node);
}

ID *deg_update_copy_on_write_datablock(const Depsgraph *depsgraph,
                                       const IDDepsNode *id_node)
{
	const ID *id_orig = id_node->id_orig;
	ID *id_cow = id_node->id_cow;
	/* Special case for datablocks which are expanded at the dependency graph
	 * construction time. This datablocks must never change pointers of their
	 * nested data since it is used for function bindings.
	 */
	if (GS(id_orig->name) == ID_SCE) {
		BLI_assert(check_datablock_expanded(id_cow) == true);
		update_copy_on_write_datablock(depsgraph, id_orig, id_cow);
		return id_cow;
	}
	/* For the rest if datablock types we use simple logic:
	 * - Free previously expanded data, if any.
	 * - Perform full datablock copy.
	 */
	deg_free_copy_on_write_datablock(id_cow);
	deg_expand_copy_on_write_datablock(depsgraph, id_node);
	return id_cow;
}

/* NOTE: Depsgraph is supposed to have ID node already. */
ID *deg_update_copy_on_write_datablock(const Depsgraph *depsgraph, ID *id_orig)
{
	DEG::IDDepsNode *id_node = depsgraph->find_id_node(id_orig);
	BLI_assert(id_node != NULL);
	return deg_update_copy_on_write_datablock(depsgraph, id_node);
}

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
	const short type = GS(id_cow->name);
	switch (type) {
		case ID_OB:
		{
			/* TODO(sergey): This workaround is only to prevent free derived
			 * caches from modifying object->data. This is currently happening
			 * due to mesh/curve datablock boundbox tagging dirty.
			 */
			Object *ob_cow = (Object *)id_cow;
			ob_cow->data = NULL;
			break;
		}
		case ID_ME:
		{
			Mesh *mesh_cow = (Mesh *)id_cow;
			if (mesh_cow->edit_btmesh != NULL) {
				BKE_editmesh_free_derivedmesh(mesh_cow->edit_btmesh);
				MEM_freeN(mesh_cow->edit_btmesh);
				mesh_cow->edit_btmesh = NULL;
			}
			break;
		}
		case ID_SCE:
		{
			/* Special case for scene: we use explicit function call which
			 * ensures no access to other datablocks is done.
			 */
			BKE_scene_free_ex((Scene *)id_cow, false);
			BKE_libblock_free_data(id_cow, false);
			id_cow->name[0] = '\0';
			return;
		}
	}
#ifdef NESTED_ID_NASTY_WORKAROUND
	nested_id_hack_discard_pointers(id_cow);
#endif
	BKE_libblock_free_datablock(id_cow);
	BKE_libblock_free_data(id_cow, false);
	/* Signal datablock as not being expanded. */
	id_cow->name[0] = '\0';
}

void deg_evaluate_copy_on_write(EvaluationContext * /*eval_ctx*/,
                                const Depsgraph *depsgraph,
                                const IDDepsNode *id_node)
{
	DEBUG_PRINT("%s on %s\n", __func__, id_node->id_orig->name);
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

}  // namespace DEG
