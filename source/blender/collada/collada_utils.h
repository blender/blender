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

/** \file collada_utils.h
 *  \ingroup collada
 */

#ifndef __COLLADA_UTILS_H__
#define __COLLADA_UTILS_H__

#include "COLLADAFWMeshPrimitive.h"
#include "COLLADAFWGeometry.h"
#include "COLLADAFWFloatOrDoubleArray.h"
#include "COLLADAFWTypes.h"

#include <vector>
#include <map>

extern "C" {
#include "DNA_object_types.h"
#include "DNA_mesh_types.h"
#include "DNA_customdata_types.h"
#include "DNA_texture_types.h"
#include "DNA_scene_types.h"

#include "RNA_access.h"

#include "BLI_linklist.h"
#include "BLI_utildefines.h"

#include "BKE_context.h"
#include "BKE_object.h"
#include "BKE_DerivedMesh.h"
#include "BKE_scene.h"
}

#include "ExportSettings.h"
#include "collada_internal.h"

typedef std::map<COLLADAFW::TextureMapId, std::vector<MTex *> > TexIndexTextureArrayMap;

extern float bc_get_float_value(const COLLADAFW::FloatOrDoubleArray& array, unsigned int index);
extern int bc_test_parent_loop(Object *par, Object *ob);
extern int bc_set_parent(Object *ob, Object *par, bContext *C, bool is_parent_space = true);
extern Object *bc_add_object(Scene *scene, int type, const char *name);
extern Mesh *bc_get_mesh_copy(Scene *scene, Object *ob, BC_export_mesh_type export_mesh_type, bool apply_modifiers, bool triangulate);

extern Object *bc_get_assigned_armature(Object *ob);
extern Object *bc_get_highest_selected_ancestor_or_self(LinkNode *export_set, Object *ob);
extern bool bc_is_base_node(LinkNode *export_set, Object *ob);
extern bool bc_is_in_Export_set(LinkNode *export_set, Object *ob);
extern bool bc_has_object_type(LinkNode *export_set, short obtype);

extern int bc_is_marked(Object *ob);
extern void bc_remove_mark(Object *ob);
extern void bc_set_mark(Object *ob);

extern char *bc_CustomData_get_layer_name(const CustomData *data, int type, int n);
extern char *bc_CustomData_get_active_layer_name(const CustomData *data, int type);

extern void bc_bubble_sort_by_Object_name(LinkNode *export_set);
extern bool bc_is_root_bone(Bone *aBone, bool deform_bones_only);
extern int  bc_get_active_UVLayer(Object *ob);

extern std::string bc_replace_string(std::string data, const std::string& pattern, const std::string& replacement); 
extern std::string bc_url_encode(std::string data); 
extern void bc_match_scale(Object *ob, UnitConverter &bc_unit, bool scale_to_scene);
extern void bc_match_scale(std::vector<Object *> *objects_done, UnitConverter &unit_converter, bool scale_to_scene);

extern void bc_triangulate_mesh(Mesh *me);


class BCPolygonNormalsIndices
{
	std::vector<unsigned int> normal_indices;

	public:

	void add_index(unsigned int index) {
		normal_indices.push_back(index);
	}

	unsigned int operator[](unsigned int i) { 
		return normal_indices[i]; 
	}

};

#endif
