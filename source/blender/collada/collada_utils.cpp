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
 * Contributor(s): Chingiz Dyussenov, Arystanbek Dyussenov, Nathan Letwory.
 *
 * ***** END GPL LICENSE BLOCK *****
 */

/** \file blender/collada/collada_utils.cpp
 *  \ingroup collada
 */


/* COLLADABU_ASSERT, may be able to remove later */
#include "COLLADABUPlatform.h"

#include "COLLADAFWGeometry.h"
#include "COLLADAFWMeshPrimitive.h"
#include "COLLADAFWMeshVertexData.h"

#include "collada_utils.h"

#include "DNA_modifier_types.h"
#include "DNA_customdata_types.h"
#include "DNA_object_types.h"
#include "DNA_mesh_types.h"
#include "DNA_scene_types.h"

#include "BLI_math.h"

#include "BKE_context.h"
#include "BKE_customdata.h"
#include "BKE_depsgraph.h"
#include "BKE_object.h"
#include "BKE_mesh.h"
#include "BKE_scene.h"

extern "C" {
#include "BKE_DerivedMesh.h"
#include "BLI_linklist.h"
}

#include "WM_api.h" // XXX hrm, see if we can do without this
#include "WM_types.h"

float bc_get_float_value(const COLLADAFW::FloatOrDoubleArray& array, unsigned int index)
{
	if (index >= array.getValuesCount())
		return 0.0f;

	if (array.getType() == COLLADAFW::MeshVertexData::DATA_TYPE_FLOAT)
		return array.getFloatValues()->getData()[index];
	else 
		return array.getDoubleValues()->getData()[index];
}

// copied from /editors/object/object_relations.c
int bc_test_parent_loop(Object *par, Object *ob)
{
	/* test if 'ob' is a parent somewhere in par's parents */
	
	if (par == NULL) return 0;
	if (ob == par) return 1;
	
	return bc_test_parent_loop(par->parent, ob);
}

// a shortened version of parent_set_exec()
// if is_parent_space is true then ob->obmat will be multiplied by par->obmat before parenting
int bc_set_parent(Object *ob, Object *par, bContext *C, bool is_parent_space)
{
	Object workob;
	Main *bmain = CTX_data_main(C);
	Scene *sce = CTX_data_scene(C);
	
	if (!par || bc_test_parent_loop(par, ob))
		return false;

	ob->parent = par;
	ob->partype = PAROBJECT;

	ob->parsubstr[0] = 0;

	if (is_parent_space) {
		float mat[4][4];
		// calc par->obmat
		BKE_object_where_is_calc(sce, par);

		// move child obmat into world space
		mult_m4_m4m4(mat, par->obmat, ob->obmat);
		copy_m4_m4(ob->obmat, mat);
	}
	
	// apply child obmat (i.e. decompose it into rot/loc/size)
	BKE_object_apply_mat4(ob, ob->obmat, 0, 0);

	// compute parentinv
	BKE_object_workob_calc_parent(sce, ob, &workob);
	invert_m4_m4(ob->parentinv, workob.obmat);

	ob->recalc |= OB_RECALC_OB | OB_RECALC_DATA;
	par->recalc |= OB_RECALC_OB;

	DAG_scene_sort(bmain, sce);
	DAG_ids_flush_update(bmain, 0);
	WM_event_add_notifier(C, NC_OBJECT | ND_TRANSFORM, NULL);

	return true;
}

Object *bc_add_object(Scene *scene, int type, const char *name)
{
	Object *ob = BKE_object_add_only_object(type, name);

	ob->data = BKE_object_obdata_add_from_type(type);
	ob->lay = scene->lay;
	ob->recalc |= OB_RECALC_OB | OB_RECALC_DATA | OB_RECALC_TIME;

	BKE_scene_base_select(scene, BKE_scene_base_add(scene, ob));

	return ob;
}

Mesh *bc_to_mesh_apply_modifiers(Scene *scene, Object *ob)
{
	Mesh *tmpmesh;
	CustomDataMask mask = CD_MASK_MESH;
	DerivedMesh *dm     = mesh_create_derived_view(scene, ob, mask);
	tmpmesh             = BKE_mesh_add("ColladaMesh"); // name is not important here
	DM_to_mesh(dm, tmpmesh, ob);
	dm->release(dm);
	return tmpmesh;
}

Object *bc_get_assigned_armature(Object *ob)
{
	Object *ob_arm = NULL;

	if (ob->parent && ob->partype == PARSKEL && ob->parent->type == OB_ARMATURE) {
		ob_arm = ob->parent;
	}
	else {
		ModifierData *mod;
		for (mod = (ModifierData *)ob->modifiers.first; mod; mod = mod->next) {
			if (mod->type == eModifierType_Armature) {
				ob_arm = ((ArmatureModifierData *)mod)->object;
			}
		}
	}

	return ob_arm;
}

// Returns the highest selected ancestor
// returns NULL if no ancestor is selected
// IMPORTANT: This function expects that
// all exported objects have set:
// ob->id.flag & LIB_DOIT
Object *bc_get_highest_selected_ancestor_or_self(LinkNode *export_set, Object *ob) 
{
	Object *ancestor = ob;
	while (ob->parent && bc_is_marked(ob->parent))
	{
		ob = ob->parent;
		ancestor = ob;
	}
	return ancestor;
}

bool bc_is_base_node(LinkNode *export_set, Object *ob)
{
	Object *root = bc_get_highest_selected_ancestor_or_self(export_set, ob);
	return (root == ob);
}

bool bc_is_in_Export_set(LinkNode *export_set, Object *ob)
{
	LinkNode *node = export_set;
	
	while (node) {
		Object *element = (Object *)node->link;
	
		if (element == ob)
			return true;
		
		node= node->next;
	}
	return false;
}

bool bc_has_object_type(LinkNode *export_set, short obtype)
{
	LinkNode *node = export_set;
	
	while (node) {
		Object *ob = (Object *)node->link;
			
		if (ob->type == obtype && ob->data) {
			return true;
		}
		node= node->next;
	}
	return false;
}

int bc_is_marked(Object *ob)
{
	return ob && (ob->id.flag & LIB_DOIT);
}

void bc_remove_mark(Object *ob)
{
	ob->id.flag &= ~LIB_DOIT;
}
