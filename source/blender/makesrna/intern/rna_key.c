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
 * Contributor(s): Blender Foundation (2008).
 *
 * ***** END GPL LICENSE BLOCK *****
 */

/** \file blender/makesrna/intern/rna_key.c
 *  \ingroup RNA
 */

#include <stdlib.h>

#include "DNA_ID.h"
#include "DNA_scene_types.h"
#include "DNA_curve_types.h"
#include "DNA_key_types.h"
#include "DNA_lattice_types.h"
#include "DNA_mesh_types.h"

#include "BLI_string_utils.h"
#include "BLI_utildefines.h"

#include "BLT_translation.h"

#include "RNA_access.h"
#include "RNA_define.h"
#include "RNA_enum_types.h"

#include "rna_internal.h"

#ifdef RNA_RUNTIME

#include <stddef.h>

#include "DNA_object_types.h"

#include "BKE_animsys.h"
#include "BKE_depsgraph.h"
#include "BKE_key.h"
#include "BKE_main.h"

#include "WM_api.h"
#include "WM_types.h"

static Key *rna_ShapeKey_find_key(ID *id)
{
	switch (GS(id->name)) {
		case ID_CU: return ((Curve *)id)->key;
		case ID_KE: return (Key *)id;
		case ID_LT: return ((Lattice *)id)->key;
		case ID_ME: return ((Mesh *)id)->key;
		case ID_OB: return BKE_key_from_object((Object *)id);
		default: return NULL;
	}
}

static void rna_ShapeKey_name_set(PointerRNA *ptr, const char *value)
{
	KeyBlock *kb = ptr->data;
	char oldname[sizeof(kb->name)];
	
	/* make a copy of the old name first */
	BLI_strncpy(oldname, kb->name, sizeof(kb->name));
	
	/* copy the new name into the name slot */
	BLI_strncpy_utf8(kb->name, value, sizeof(kb->name));
	
	/* make sure the name is truly unique */
	if (ptr->id.data) {
		Key *key = rna_ShapeKey_find_key(ptr->id.data);
		BLI_uniquename(&key->block, kb, CTX_DATA_(BLT_I18NCONTEXT_ID_SHAPEKEY, "Key"), '.',
		               offsetof(KeyBlock, name), sizeof(kb->name));
	}
	
	/* fix all the animation data which may link to this */
	BKE_animdata_fix_paths_rename_all(NULL, "key_blocks", oldname, kb->name);
}

static float rna_ShapeKey_frame_get(PointerRNA *ptr)
{
	KeyBlock *kb = (KeyBlock *)ptr->data;
	return kb->pos * 100.0f;  /* Because pos is ctime/100... */
}

static void rna_ShapeKey_value_set(PointerRNA *ptr, float value)
{
	KeyBlock *data = (KeyBlock *)ptr->data;
	CLAMP(value, data->slidermin, data->slidermax);
	data->curval = value;
}

static void rna_ShapeKey_value_range(PointerRNA *ptr, float *min, float *max,
                                     float *UNUSED(softmin), float *UNUSED(softmax))
{
	KeyBlock *data = (KeyBlock *)ptr->data;

	*min = data->slidermin;
	*max = data->slidermax;
}

/* epsilon for how close one end of shapekey range can get to the other */
#define SHAPEKEY_SLIDER_TOL 0.001f

static void rna_ShapeKey_slider_min_range(PointerRNA *ptr, float *min, float *max,
                                          float *UNUSED(softmin), float *UNUSED(softmax))
{
	KeyBlock *data = (KeyBlock *)ptr->data;

	*min = -10.0f;
	*max = data->slidermax - SHAPEKEY_SLIDER_TOL;
}

static void rna_ShapeKey_slider_min_set(PointerRNA *ptr, float value)
{
	KeyBlock *data = (KeyBlock *)ptr->data;
	float min, max, softmin, softmax;
	
	rna_ShapeKey_slider_min_range(ptr, &min, &max, &softmin, &softmax);
	CLAMP(value, min, max);
	data->slidermin = value;
}

static void rna_ShapeKey_slider_max_range(PointerRNA *ptr, float *min, float *max,
                                          float *UNUSED(softmin), float *UNUSED(softmax))
{
	KeyBlock *data = (KeyBlock *)ptr->data;

	*min = data->slidermin + SHAPEKEY_SLIDER_TOL;
	*max = 10.0f;
}

static void rna_ShapeKey_slider_max_set(PointerRNA *ptr, float value)
{
	KeyBlock *data = (KeyBlock *)ptr->data;
	float min, max, softmin, softmax;
	
	rna_ShapeKey_slider_max_range(ptr, &min, &max, &softmin, &softmax);
	CLAMP(value, min, max);
	data->slidermax = value;
}

#undef SHAPEKEY_SLIDER_TOL

/* ***** Normals accessors for shapekeys. ***** */
/* Note: with this we may recompute several times the same data, should we want to access verts, then polys, then loops
 *       normals... However, such case looks rather unlikely - and not worth adding some kind of caching in KeyBlocks.
 */

static Mesh *rna_KeyBlock_normals_get_mesh(PointerRNA *ptr, ID *id)
{
	Key *key = rna_ShapeKey_find_key((id == NULL && ptr != NULL) ? ptr->id.data : id);
	id = key ? key->from : NULL;

	if (id != NULL) {
		switch (GS(id->name)) {
			case ID_ME:
				return (Mesh *)id;
			case ID_OB:
			{
				Object *ob = (Object *)id;
				if (ob->type == OB_MESH) {
					return ob->data;
				}
			}
		}
	}

	return NULL;
}

static int rna_KeyBlock_normals_vert_len(PointerRNA *ptr, int length[RNA_MAX_ARRAY_DIMENSION])
{
	Mesh *me = rna_KeyBlock_normals_get_mesh(ptr, NULL);

	length[0] = me ? me->totvert : 0;
	length[1] = 3;

	return (length[0] * length[1]);
}

static void rna_KeyBlock_normals_vert_calc(ID *id, KeyBlock *data, int *normals_len, float **normals)
{
	Mesh *me = rna_KeyBlock_normals_get_mesh(NULL, id);

	*normals_len = (me ? me->totvert : 0) * 3;

	if (ELEM(NULL, me, data) || (me->totvert == 0)) {
		*normals = NULL;
		return;
	}

	*normals = MEM_mallocN(sizeof(**normals) * (size_t)(*normals_len), __func__);

	BKE_keyblock_mesh_calc_normals(data, me, (float (*)[3])(*normals), NULL, NULL);
}

static int rna_KeyBlock_normals_poly_len(PointerRNA *ptr, int length[RNA_MAX_ARRAY_DIMENSION])
{
	Mesh *me = rna_KeyBlock_normals_get_mesh(ptr, NULL);

	length[0] = me ? me->totpoly : 0;
	length[1] = 3;

	return (length[0] * length[1]);
}

static void rna_KeyBlock_normals_poly_calc(ID *id, KeyBlock *data, int *normals_len, float **normals)
{
	Mesh *me = rna_KeyBlock_normals_get_mesh(NULL, id);

	*normals_len = (me ? me->totpoly : 0) * 3;

	if (ELEM(NULL, me, data) || (me->totpoly == 0)) {
		*normals = NULL;
		return;
	}

	*normals = MEM_mallocN(sizeof(**normals) * (size_t)(*normals_len), __func__);

	BKE_keyblock_mesh_calc_normals(data, me, NULL, (float (*)[3])(*normals), NULL);
}

static int rna_KeyBlock_normals_loop_len(PointerRNA *ptr, int length[RNA_MAX_ARRAY_DIMENSION])
{
	Mesh *me = rna_KeyBlock_normals_get_mesh(ptr, NULL);

	length[0] = me ? me->totloop : 0;
	length[1] = 3;

	return (length[0] * length[1]);
}

static void rna_KeyBlock_normals_loop_calc(ID *id, KeyBlock *data, int *normals_len, float **normals)
{
	Mesh *me = rna_KeyBlock_normals_get_mesh(NULL, id);

	*normals_len = (me ? me->totloop : 0) * 3;

	if (ELEM(NULL, me, data) || (me->totloop == 0)) {
		*normals = NULL;
		return;
	}

	*normals = MEM_mallocN(sizeof(**normals) * (size_t)(*normals_len), __func__);

	BKE_keyblock_mesh_calc_normals(data, me, NULL, NULL, (float (*)[3])(*normals));
}


PointerRNA rna_object_shapekey_index_get(ID *id, int value)
{
	Key *key = rna_ShapeKey_find_key(id);
	KeyBlock *kb = NULL;
	PointerRNA ptr;

	if (key && value < key->totkey)
		kb = BLI_findlink(&key->block, value);
	
	RNA_pointer_create(id, &RNA_ShapeKey, kb, &ptr);

	return ptr;
}

int rna_object_shapekey_index_set(ID *id, PointerRNA value, int current)
{
	Key *key = rna_ShapeKey_find_key(id);

	if (key) {
		int a = BLI_findindex(&key->block, value.data);
		if (a != -1) return a;
	}
	
	return current;
}

static PointerRNA rna_ShapeKey_relative_key_get(PointerRNA *ptr)
{
	KeyBlock *kb = (KeyBlock *)ptr->data;

	return rna_object_shapekey_index_get(ptr->id.data, kb->relative);
}

static void rna_ShapeKey_relative_key_set(PointerRNA *ptr, PointerRNA value)
{
	KeyBlock *kb = (KeyBlock *)ptr->data;

	kb->relative = rna_object_shapekey_index_set(ptr->id.data, value, kb->relative);
}

static void rna_ShapeKeyPoint_co_get(PointerRNA *ptr, float *values)
{
	float *vec = (float *)ptr->data;

	values[0] = vec[0];
	values[1] = vec[1];
	values[2] = vec[2];
}

static void rna_ShapeKeyPoint_co_set(PointerRNA *ptr, const float *values)
{
	float *vec = (float *)ptr->data;

	vec[0] = values[0];
	vec[1] = values[1];
	vec[2] = values[2];
}

static float rna_ShapeKeyCurvePoint_tilt_get(PointerRNA *ptr)
{
	float *vec = (float *)ptr->data;
	return vec[3];
}

static void rna_ShapeKeyCurvePoint_tilt_set(PointerRNA *ptr, float value)
{
	float *vec = (float *)ptr->data;
	vec[3] = value;
}

static void rna_ShapeKeyBezierPoint_co_get(PointerRNA *ptr, float *values)
{
	float *vec = (float *)ptr->data;

	values[0] = vec[0 + 3];
	values[1] = vec[1 + 3];
	values[2] = vec[2 + 3];
}

static void rna_ShapeKeyBezierPoint_co_set(PointerRNA *ptr, const float *values)
{
	float *vec = (float *)ptr->data;

	vec[0 + 3] = values[0];
	vec[1 + 3] = values[1];
	vec[2 + 3] = values[2];
}

static void rna_ShapeKeyBezierPoint_handle_1_co_get(PointerRNA *ptr, float *values)
{
	float *vec = (float *)ptr->data;

	values[0] = vec[0];
	values[1] = vec[1];
	values[2] = vec[2];
}

static void rna_ShapeKeyBezierPoint_handle_1_co_set(PointerRNA *ptr, const float *values)
{
	float *vec = (float *)ptr->data;

	vec[0] = values[0];
	vec[1] = values[1];
	vec[2] = values[2];
}

static void rna_ShapeKeyBezierPoint_handle_2_co_get(PointerRNA *ptr, float *values)
{
	float *vec = (float *)ptr->data;

	values[0] = vec[6 + 0];
	values[1] = vec[6 + 1];
	values[2] = vec[6 + 2];
}

static void rna_ShapeKeyBezierPoint_handle_2_co_set(PointerRNA *ptr, const float *values)
{
	float *vec = (float *)ptr->data;

	vec[6 + 0] = values[0];
	vec[6 + 1] = values[1];
	vec[6 + 2] = values[2];
}

#if 0
static float rna_ShapeKeyBezierPoint_tilt_get(PointerRNA *ptr)
{
	float *vec = (float *)ptr->data;
	return vec[10];
}

static void rna_ShapeKeyBezierPoint_tilt_set(PointerRNA *ptr, float value)
{
	float *vec = (float *)ptr->data;
	vec[10] = value;
}
#endif

static void rna_ShapeKey_data_begin(CollectionPropertyIterator *iter, PointerRNA *ptr)
{
	Key *key = rna_ShapeKey_find_key(ptr->id.data);
	KeyBlock *kb = (KeyBlock *)ptr->data;
	Curve *cu;
	Nurb *nu;
	int tot = kb->totelem, size = key->elemsize;
	
	if (GS(key->from->name) == ID_CU) {
		cu = (Curve *)key->from;
		nu = cu->nurb.first;
		
		if (nu->bezt) {
			tot /= 3;
			size *= 3;
		}
	}
	
	rna_iterator_array_begin(iter, (void *)kb->data, size, tot, 0, NULL);
}

static int rna_ShapeKey_data_length(PointerRNA *ptr)
{
	Key *key = rna_ShapeKey_find_key(ptr->id.data);
	KeyBlock *kb = (KeyBlock *)ptr->data;
	Curve *cu;
	Nurb *nu;
	int tot = kb->totelem;
	
	if (GS(key->from->name) == ID_CU) {
		cu = (Curve *)key->from;
		nu = cu->nurb.first;
		
		if (nu->bezt)
			tot /= 3;
	}
	
	return tot;
}

static PointerRNA rna_ShapeKey_data_get(CollectionPropertyIterator *iter)
{
	Key *key = rna_ShapeKey_find_key(iter->parent.id.data);
	StructRNA *type;
	Curve *cu;
	Nurb *nu;
	
	if (GS(key->from->name) == ID_CU) {
		cu = (Curve *)key->from;
		nu = cu->nurb.first;
		
		if (nu->bezt)
			type = &RNA_ShapeKeyBezierPoint;
		else
			type = &RNA_ShapeKeyCurvePoint;
	}
	else
		type = &RNA_ShapeKeyPoint;
	
	return rna_pointer_inherit_refine(&iter->parent, type, rna_iterator_array_get(iter));
}

static char *rna_ShapeKey_path(PointerRNA *ptr)
{
	KeyBlock *kb = (KeyBlock *)ptr->data;
	ID *id = ptr->id.data;
	char name_esc[sizeof(kb->name) * 2];

	BLI_strescape(name_esc, kb->name, sizeof(name_esc));

	if ((id) && (GS(id->name) != ID_KE))
		return BLI_sprintfN("shape_keys.key_blocks[\"%s\"]", name_esc);
	else
		return BLI_sprintfN("key_blocks[\"%s\"]", name_esc);
}

static void rna_Key_update_data(Main *bmain, Scene *UNUSED(scene), PointerRNA *ptr)
{
	Key *key = ptr->id.data;
	Object *ob;

	for (ob = bmain->object.first; ob; ob = ob->id.next) {
		if (BKE_key_from_object(ob) == key) {
			DAG_id_tag_update(&ob->id, OB_RECALC_DATA);
			WM_main_add_notifier(NC_OBJECT | ND_MODIFIER, ob);
		}
	}
}

static KeyBlock *rna_ShapeKeyData_find_keyblock(Key *key, float *point)
{
	KeyBlock *kb;
	
	/* sanity checks */
	if (ELEM(NULL, key, point))
		return NULL;
	
	/* we'll need to manually search through the keyblocks and check
	 * if the point is somewhere in the middle of each block's data
	 */
	for (kb = key->block.first; kb; kb = kb->next) {
		if (kb->data) {
			float *start = (float *)kb->data;
			float *end;
			
			/* easy cases first */
			if ((start == NULL) || (start > point)) {
				/* there's no chance point is in array */
				continue;
			}
			else if (start == point) {
				/* exact match - point is first in array */
				return kb;
			}
			
			/* determine where end of array is
			 *	- elemsize is in bytes, so use (char *) cast to get array in terms of bytes
			 */
			end = (float *)((char *)start + (key->elemsize * kb->totelem));
			
			/* if point's address is less than the end, then it is somewhere between start and end, so in array */
			if (end > point) {
				/* we've found the owner of the point data */
				return kb;
			}
		}
	}
	
	return NULL;
}

static int rna_ShapeKeyPoint_get_index(Key *key, KeyBlock *kb, float *point)
{
	/* if we frame the data array and point pointers as (char *), then the difference between
	 * them will be in bytes. Thus, dividing through by key->elemsize (number of bytes per point)
	 * gives us the offset of point from start of array.
	 */
	char *start = (char *)kb->data;
	char *pt = (char *)point;
	
	return (int)(pt - start) / key->elemsize;
}

static int rna_ShapeKeyBezierPoint_get_index(KeyBlock *kb, float *point)
{
	float *start = (float *)kb->data;
	
	/* Unlike with rna_ShapeKeyPoint_get_index(), we cannot use key->elemsize here
	 * since the default value for curves (16) is actually designed for BPoints
	 * (i.e. NURBS Surfaces). The magic number "12" here was found by empirical
	 * testing on a 64-bit system, and is similar to what's used for meshes and 
	 * lattices. For more details, see T38013
	 */
	return (int)(point - start) / 12;
}

static char *rna_ShapeKeyPoint_path(PointerRNA *ptr)
{
	ID *id = (ID *)ptr->id.data;
	Key *key = rna_ShapeKey_find_key(ptr->id.data);
	KeyBlock *kb;
	float *point = (float *)ptr->data;
	
	/* if we can get a key block, we can construct a path */
	kb = rna_ShapeKeyData_find_keyblock(key, point);
	
	if (kb) {
		char name_esc_kb[sizeof(kb->name) * 2];
		int index;
		
		if (ptr->type == &RNA_ShapeKeyBezierPoint)
			index = rna_ShapeKeyBezierPoint_get_index(kb, point);
		else
			index = rna_ShapeKeyPoint_get_index(key, kb, point);

		BLI_strescape(name_esc_kb, kb->name, sizeof(name_esc_kb));
		
		if (GS(id->name) == ID_KE)
			return BLI_sprintfN("key_blocks[\"%s\"].data[%d]", name_esc_kb, index);
		else
			return BLI_sprintfN("shape_keys.key_blocks[\"%s\"].data[%d]", name_esc_kb, index);
	}
	else
		return NULL;  /* XXX: there's really no way to resolve this... */
}

#else

EnumPropertyItem rna_enum_keyblock_type_items[] = {
	{KEY_LINEAR, "KEY_LINEAR", 0, "Linear", ""},
	{KEY_CARDINAL, "KEY_CARDINAL", 0, "Cardinal", ""},
	{KEY_CATMULL_ROM, "KEY_CATMULL_ROM", 0, "Catmull-Rom", ""},
	{KEY_BSPLINE, "KEY_BSPLINE", 0, "BSpline", ""},
	{0, NULL, 0, NULL, NULL}
};

static void rna_def_keydata(BlenderRNA *brna)
{
	StructRNA *srna;
	PropertyRNA *prop;

	srna = RNA_def_struct(brna, "ShapeKeyPoint", NULL);
	RNA_def_struct_ui_text(srna, "Shape Key Point", "Point in a shape key");
	RNA_def_struct_path_func(srna, "rna_ShapeKeyPoint_path");

	prop = RNA_def_property(srna, "co", PROP_FLOAT, PROP_TRANSLATION);
	RNA_def_property_array(prop, 3);
	RNA_def_property_float_funcs(prop, "rna_ShapeKeyPoint_co_get", "rna_ShapeKeyPoint_co_set", NULL);
	RNA_def_property_ui_text(prop, "Location", "");
	RNA_def_property_update(prop, 0, "rna_Key_update_data");

	srna = RNA_def_struct(brna, "ShapeKeyCurvePoint", NULL);
	RNA_def_struct_ui_text(srna, "Shape Key Curve Point", "Point in a shape key for curves");
	/* there's nothing type specific here, so this is fine for now */
	RNA_def_struct_path_func(srna, "rna_ShapeKeyPoint_path");

	prop = RNA_def_property(srna, "co", PROP_FLOAT, PROP_TRANSLATION);
	RNA_def_property_array(prop, 3);
	RNA_def_property_float_funcs(prop, "rna_ShapeKeyPoint_co_get", "rna_ShapeKeyPoint_co_set", NULL);
	RNA_def_property_ui_text(prop, "Location", "");
	RNA_def_property_update(prop, 0, "rna_Key_update_data");

	prop = RNA_def_property(srna, "tilt", PROP_FLOAT, PROP_NONE);
	RNA_def_property_float_funcs(prop, "rna_ShapeKeyCurvePoint_tilt_get", "rna_ShapeKeyCurvePoint_tilt_set", NULL);
	RNA_def_property_ui_text(prop, "Tilt", "");
	RNA_def_property_update(prop, 0, "rna_Key_update_data");

	srna = RNA_def_struct(brna, "ShapeKeyBezierPoint", NULL);
	RNA_def_struct_ui_text(srna, "Shape Key Bezier Point", "Point in a shape key for Bezier curves");
	/* there's nothing type specific here, so this is fine for now */
	RNA_def_struct_path_func(srna, "rna_ShapeKeyPoint_path");

	prop = RNA_def_property(srna, "co", PROP_FLOAT, PROP_TRANSLATION);
	RNA_def_property_array(prop, 3);
	RNA_def_property_float_funcs(prop, "rna_ShapeKeyBezierPoint_co_get", "rna_ShapeKeyBezierPoint_co_set", NULL);
	RNA_def_property_ui_text(prop, "Location", "");
	RNA_def_property_update(prop, 0, "rna_Key_update_data");

	prop = RNA_def_property(srna, "handle_left", PROP_FLOAT, PROP_TRANSLATION);
	RNA_def_property_array(prop, 3);
	RNA_def_property_float_funcs(prop, "rna_ShapeKeyBezierPoint_handle_1_co_get",
	                             "rna_ShapeKeyBezierPoint_handle_1_co_set", NULL);
	RNA_def_property_ui_text(prop, "Handle 1 Location", "");
	RNA_def_property_update(prop, 0, "rna_Key_update_data");

	prop = RNA_def_property(srna, "handle_right", PROP_FLOAT, PROP_TRANSLATION);
	RNA_def_property_array(prop, 3);
	RNA_def_property_float_funcs(prop, "rna_ShapeKeyBezierPoint_handle_2_co_get",
	                             "rna_ShapeKeyBezierPoint_handle_2_co_set", NULL);
	RNA_def_property_ui_text(prop, "Handle 2 Location", "");
	RNA_def_property_update(prop, 0, "rna_Key_update_data");

	/* appears to be unused currently */
#if 0
	prop = RNA_def_property(srna, "tilt", PROP_FLOAT, PROP_NONE);
	RNA_def_property_float_funcs(prop, "rna_ShapeKeyBezierPoint_tilt_get", "rna_ShapeKeyBezierPoint_tilt_set", NULL);
	RNA_def_property_ui_text(prop, "Tilt", "");
	RNA_def_property_update(prop, 0, "rna_Key_update_data");
#endif
}

static void rna_def_keyblock(BlenderRNA *brna)
{
	StructRNA *srna;
	PropertyRNA *prop, *parm;
	FunctionRNA *func;

	srna = RNA_def_struct(brna, "ShapeKey", NULL);
	RNA_def_struct_ui_text(srna, "Shape Key", "Shape key in a shape keys data-block");
	RNA_def_struct_sdna(srna, "KeyBlock");
	RNA_def_struct_path_func(srna, "rna_ShapeKey_path");
	RNA_def_struct_ui_icon(srna, ICON_SHAPEKEY_DATA);

	prop = RNA_def_property(srna, "name", PROP_STRING, PROP_NONE);
	RNA_def_property_ui_text(prop, "Name", "Name of Shape Key");
	RNA_def_property_string_funcs(prop, NULL, NULL, "rna_ShapeKey_name_set");
	RNA_def_property_update(prop, 0, "rna_Key_update_data");
	RNA_def_struct_name_property(srna, prop);

	/* keys need to be sorted to edit this */
	prop = RNA_def_property(srna, "frame", PROP_FLOAT, PROP_TIME);
	RNA_def_property_clear_flag(prop, PROP_EDITABLE);
	RNA_def_property_float_sdna(prop, NULL, "pos");
	RNA_def_property_float_funcs(prop, "rna_ShapeKey_frame_get", NULL, NULL);
	RNA_def_property_ui_text(prop, "Frame", "Frame for absolute keys");
	RNA_def_property_update(prop, 0, "rna_Key_update_data");
	
	/* for now, this is editable directly, as users can set this even if they're not animating them
	 * (to test results) */
	prop = RNA_def_property(srna, "value", PROP_FLOAT, PROP_FACTOR);
	RNA_def_property_float_sdna(prop, NULL, "curval");
	RNA_def_property_float_funcs(prop, NULL, "rna_ShapeKey_value_set", "rna_ShapeKey_value_range");
	RNA_def_property_ui_range(prop, -10.0f, 10.0f, 10, 3);
	RNA_def_property_ui_text(prop, "Value", "Value of shape key at the current frame");
	RNA_def_property_update(prop, 0, "rna_Key_update_data");

	prop = RNA_def_property(srna, "interpolation", PROP_ENUM, PROP_NONE);
	RNA_def_property_enum_sdna(prop, NULL, "type");
	RNA_def_property_enum_items(prop, rna_enum_keyblock_type_items);
	RNA_def_property_ui_text(prop, "Interpolation", "Interpolation type for absolute shape keys");
	RNA_def_property_update(prop, 0, "rna_Key_update_data");

	prop = RNA_def_property(srna, "vertex_group", PROP_STRING, PROP_NONE);
	RNA_def_property_string_sdna(prop, NULL, "vgroup");
	RNA_def_property_ui_text(prop, "Vertex Group", "Vertex weight group, to blend with basis shape");
	RNA_def_property_update(prop, 0, "rna_Key_update_data");

	prop = RNA_def_property(srna, "relative_key", PROP_POINTER, PROP_NONE);
	RNA_def_property_struct_type(prop, "ShapeKey");
	RNA_def_property_flag(prop, PROP_EDITABLE | PROP_NEVER_NULL);
	RNA_def_property_pointer_funcs(prop, "rna_ShapeKey_relative_key_get",
	                               "rna_ShapeKey_relative_key_set", NULL, NULL);
	RNA_def_property_ui_text(prop, "Relative Key", "Shape used as a relative key");
	RNA_def_property_update(prop, 0, "rna_Key_update_data");

	prop = RNA_def_property(srna, "mute", PROP_BOOLEAN, PROP_NONE);
	RNA_def_property_boolean_sdna(prop, NULL, "flag", KEYBLOCK_MUTE);
	RNA_def_property_ui_text(prop, "Mute", "Mute this shape key");
	RNA_def_property_ui_icon(prop, ICON_RESTRICT_VIEW_OFF, 1);
	RNA_def_property_update(prop, 0, "rna_Key_update_data");

	prop = RNA_def_property(srna, "slider_min", PROP_FLOAT, PROP_NONE);
	RNA_def_property_float_sdna(prop, NULL, "slidermin");
	RNA_def_property_range(prop, -10.0f, 10.0f);
	RNA_def_property_float_funcs(prop, NULL, "rna_ShapeKey_slider_min_set", "rna_ShapeKey_slider_min_range");
	RNA_def_property_ui_text(prop, "Slider Min", "Minimum for slider");

	prop = RNA_def_property(srna, "slider_max", PROP_FLOAT, PROP_NONE);
	RNA_def_property_float_sdna(prop, NULL, "slidermax");
	RNA_def_property_range(prop, -10.0f, 10.0f);
	RNA_def_property_float_default(prop, 1.0f);
	RNA_def_property_float_funcs(prop, NULL, "rna_ShapeKey_slider_max_set", "rna_ShapeKey_slider_max_range");
	RNA_def_property_ui_text(prop, "Slider Max", "Maximum for slider");

	prop = RNA_def_property(srna, "data", PROP_COLLECTION, PROP_NONE);
	RNA_def_property_collection_sdna(prop, NULL, "data", "totelem");
	RNA_def_property_struct_type(prop, "UnknownType");
	RNA_def_property_ui_text(prop, "Data", "");
	RNA_def_property_collection_funcs(prop, "rna_ShapeKey_data_begin", NULL, NULL, "rna_ShapeKey_data_get",
	                                  "rna_ShapeKey_data_length", NULL, NULL, NULL);

	/* XXX multi-dim dynamic arrays are very badly supported by (py)rna currently, those are defined for the day
	 *     it works better, for now user will get a 1D tuple...
	 **/
	func = RNA_def_function(srna, "normals_vertex_get", "rna_KeyBlock_normals_vert_calc");
	RNA_def_function_ui_description(func, "Compute local space vertices' normals for this shape key");
	RNA_def_function_flag(func, FUNC_USE_SELF_ID);
	parm = RNA_def_property(func, "normals", PROP_FLOAT, /* PROP_DIRECTION */ PROP_NONE);
	RNA_def_parameter_flags(parm, PROP_DYNAMIC, PARM_OUTPUT);
	RNA_def_property_multi_array(parm, 2, NULL);
	RNA_def_property_range(parm, -1.0f, 1.0f);
	RNA_def_property_dynamic_array_funcs(parm, "rna_KeyBlock_normals_vert_len");

	func = RNA_def_function(srna, "normals_polygon_get", "rna_KeyBlock_normals_poly_calc");
	RNA_def_function_ui_description(func, "Compute local space faces' normals for this shape key");
	RNA_def_function_flag(func, FUNC_USE_SELF_ID);
	parm = RNA_def_property(func, "normals", PROP_FLOAT, /* PROP_DIRECTION */ PROP_NONE);
	RNA_def_parameter_flags(parm, PROP_DYNAMIC, PARM_OUTPUT);
	RNA_def_property_multi_array(parm, 2, NULL);
	RNA_def_property_range(parm, -1.0f, 1.0f);
	RNA_def_property_dynamic_array_funcs(parm, "rna_KeyBlock_normals_poly_len");

	func = RNA_def_function(srna, "normals_split_get", "rna_KeyBlock_normals_loop_calc");
	RNA_def_function_ui_description(func, "Compute local space face corners' normals for this shape key");
	RNA_def_function_flag(func, FUNC_USE_SELF_ID);
	parm = RNA_def_property(func, "normals", PROP_FLOAT, /* PROP_DIRECTION */ PROP_NONE);
	RNA_def_parameter_flags(parm, PROP_DYNAMIC, PARM_OUTPUT);
	RNA_def_property_multi_array(parm, 2, NULL);
	RNA_def_property_range(parm, -1.0f, 1.0f);
	RNA_def_property_dynamic_array_funcs(parm, "rna_KeyBlock_normals_loop_len");
}

static void rna_def_key(BlenderRNA *brna)
{
	StructRNA *srna;
	PropertyRNA *prop;

	srna = RNA_def_struct(brna, "Key", "ID");
	RNA_def_struct_ui_text(srna, "Key", "Shape keys data-block containing different shapes of geometric data-blocks");
	RNA_def_struct_ui_icon(srna, ICON_SHAPEKEY_DATA);

	prop = RNA_def_property(srna, "reference_key", PROP_POINTER, PROP_NONE);
	RNA_def_property_flag(prop, PROP_NEVER_NULL);
	RNA_def_property_clear_flag(prop, PROP_EDITABLE);
	RNA_def_property_pointer_sdna(prop, NULL, "refkey");
	RNA_def_property_ui_text(prop, "Reference Key", "");

	prop = RNA_def_property(srna, "key_blocks", PROP_COLLECTION, PROP_NONE);
	RNA_def_property_collection_sdna(prop, NULL, "block", NULL);
	RNA_def_property_struct_type(prop, "ShapeKey");
	RNA_def_property_ui_text(prop, "Key Blocks", "Shape keys");

	rna_def_animdata_common(srna);

	prop = RNA_def_property(srna, "user", PROP_POINTER, PROP_NONE);
	RNA_def_property_flag(prop, PROP_NEVER_NULL);
	RNA_def_property_pointer_sdna(prop, NULL, "from");
	RNA_def_property_ui_text(prop, "User", "Data-block using these shape keys");

	prop = RNA_def_property(srna, "use_relative", PROP_BOOLEAN, PROP_NONE);
	RNA_def_property_boolean_sdna(prop, NULL, "type", KEY_RELATIVE);
	RNA_def_property_ui_text(prop, "Relative",
	                         "Make shape keys relative, "
	                         "otherwise play through shapes as a sequence using the evaluation time");
	RNA_def_property_update(prop, 0, "rna_Key_update_data");

	prop = RNA_def_property(srna, "eval_time", PROP_FLOAT, PROP_NONE);
	RNA_def_property_float_sdna(prop, NULL, "ctime");
	RNA_def_property_range(prop, MINFRAME, MAXFRAME);
	RNA_def_property_ui_text(prop, "Evaluation Time", "Evaluation time for absolute shape keys");
	RNA_def_property_update(prop, 0, "rna_Key_update_data");
}

void RNA_def_key(BlenderRNA *brna)
{
	rna_def_key(brna);
	rna_def_keyblock(brna);
	rna_def_keydata(brna);
}

#endif
