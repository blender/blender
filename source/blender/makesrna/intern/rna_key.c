/**
 * $Id$
 *
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

#include <stdlib.h>

#include "RNA_access.h"
#include "RNA_define.h"

#include "rna_internal.h"

#include "DNA_ID.h"
#include "DNA_curve_types.h"
#include "DNA_key_types.h"
#include "DNA_lattice_types.h"
#include "DNA_mesh_types.h"

#ifdef RNA_RUNTIME

#include <stddef.h>

#include "DNA_object_types.h"
#include "DNA_scene_types.h"

#include "BKE_animsys.h"
#include "BKE_depsgraph.h"
#include "BKE_key.h"
#include "BKE_main.h"

#include "WM_api.h"
#include "WM_types.h"

static Key *rna_ShapeKey_find_key(ID *id)
{
	switch(GS(id->name)) {
		case ID_CU: return ((Curve*)id)->key;
		case ID_KE: return (Key*)id;
		case ID_LT: return ((Lattice*)id)->key;
		case ID_ME: return ((Mesh*)id)->key;
		default: return NULL;
	}
}

void rna_ShapeKey_name_set(PointerRNA *ptr, const char *value)
{
	KeyBlock *kb= ptr->data;
	char oldname[sizeof(kb->name)];
	
	/* make a copy of the old name first */
	BLI_strncpy(oldname, kb->name, sizeof(kb->name));
	
	/* copy the new name into the name slot */
	BLI_strncpy(kb->name, value, sizeof(kb->name));
	
	/* make sure the name is truly unique */
	if (ptr->id.data) {
		Key *key= rna_ShapeKey_find_key(ptr->id.data);
		BLI_uniquename(&key->block, kb, "Key", '.', offsetof(KeyBlock, name), sizeof(kb->name));
	}
	
	/* fix all the animation data which may link to this */
	BKE_all_animdata_fix_paths_rename("keys", oldname, kb->name);
}

static void rna_ShapeKey_value_set(PointerRNA *ptr, float value)
{
	KeyBlock *data= (KeyBlock*)ptr->data;
	CLAMP(value, data->slidermin, data->slidermax);
	data->curval= value;
}

static void rna_ShapeKey_value_range(PointerRNA *ptr, float *min, float *max)
{
	KeyBlock *data= (KeyBlock*)ptr->data;

	*min= data->slidermin;
	*max= data->slidermax;
}

PointerRNA rna_object_shapekey_index_get(ID *id, int value)
{
	Key *key= rna_ShapeKey_find_key(id);
	KeyBlock *kb= NULL;
	PointerRNA ptr;
	int a;

	if(key && value < key->totkey)
		for(a=0, kb=key->block.first; kb; kb=kb->next, a++)
			if(a == value)
				break;
	
	RNA_pointer_create(id, &RNA_ShapeKey, kb, &ptr);

	return ptr;
}

int rna_object_shapekey_index_set(ID *id, PointerRNA value, int current)
{
	Key *key= rna_ShapeKey_find_key(id);
	KeyBlock *kb;
	int a;

	if(key)
		for(a=0, kb=key->block.first; kb; kb=kb->next, a++)
			if(kb == value.data)
				return a;
	
	return current;
}

static PointerRNA rna_ShapeKey_relative_key_get(PointerRNA *ptr)
{
	KeyBlock *kb= (KeyBlock*)ptr->data;

	return rna_object_shapekey_index_get(ptr->id.data, kb->relative);
}

static void rna_ShapeKey_relative_key_set(PointerRNA *ptr, PointerRNA value)
{
	KeyBlock *kb= (KeyBlock*)ptr->data;

	kb->relative= rna_object_shapekey_index_set(ptr->id.data, value, kb->relative);
}

static void rna_ShapeKeyPoint_co_get(PointerRNA *ptr, float *values)
{
	float *vec= (float*)ptr->data;

	values[0]= vec[0];
	values[1]= vec[1];
	values[2]= vec[2];
}

static void rna_ShapeKeyPoint_co_set(PointerRNA *ptr, const float *values)
{
	float *vec= (float*)ptr->data;

	vec[0]= values[0];
	vec[1]= values[1];
	vec[2]= values[2];
}

static float rna_ShapeKeyCurvePoint_tilt_get(PointerRNA *ptr)
{
	float *vec= (float*)ptr->data;
	return vec[3];
}

static void rna_ShapeKeyCurvePoint_tilt_set(PointerRNA *ptr, float value)
{
	float *vec= (float*)ptr->data;
	vec[3]= value;
}

static void rna_ShapeKeyBezierPoint_co_get(PointerRNA *ptr, float *values)
{
	float *vec= (float*)ptr->data;

	values[0]= vec[0+3];
	values[1]= vec[1+3];
	values[2]= vec[2+3];
}

static void rna_ShapeKeyBezierPoint_co_set(PointerRNA *ptr, const float *values)
{
	float *vec= (float*)ptr->data;

	vec[0+3]= values[0];
	vec[1+3]= values[1];
	vec[2+3]= values[2];
}

static void rna_ShapeKeyBezierPoint_handle_1_co_get(PointerRNA *ptr, float *values)
{
	float *vec= (float*)ptr->data;

	values[0]= vec[0];
	values[1]= vec[1];
	values[2]= vec[2];
}

static void rna_ShapeKeyBezierPoint_handle_1_co_set(PointerRNA *ptr, const float *values)
{
	float *vec= (float*)ptr->data;

	vec[0]= values[0];
	vec[1]= values[1];
	vec[2]= values[2];
}

static void rna_ShapeKeyBezierPoint_handle_2_co_get(PointerRNA *ptr, float *values)
{
	float *vec= (float*)ptr->data;

	values[0]= vec[6+0];
	values[1]= vec[6+1];
	values[2]= vec[6+2];
}

static void rna_ShapeKeyBezierPoint_handle_2_co_set(PointerRNA *ptr, const float *values)
{
	float *vec= (float*)ptr->data;

	vec[6+0]= values[0];
	vec[6+1]= values[1];
	vec[6+2]= values[2];
}

/*static float rna_ShapeKeyBezierPoint_tilt_get(PointerRNA *ptr)
{
	float *vec= (float*)ptr->data;
	return vec[10];
}

static void rna_ShapeKeyBezierPoint_tilt_set(PointerRNA *ptr, float value)
{
	float *vec= (float*)ptr->data;
	vec[10]= value;
}*/

static void rna_ShapeKey_data_begin(CollectionPropertyIterator *iter, PointerRNA *ptr)
{
	Key *key= rna_ShapeKey_find_key(ptr->id.data);
	KeyBlock *kb= (KeyBlock*)ptr->data;
	Curve *cu;
	Nurb *nu;
	int tot= kb->totelem, size= key->elemsize;

	if(GS(key->from->name) == ID_CU) {
		cu= (Curve*)key->from;
		nu= cu->nurb.first;

		if(nu->bezt) {
			tot /= 3;
			size *= 3;
		}
	}

	rna_iterator_array_begin(iter, (void*)kb->data, size, tot, 0, NULL);
}

static int rna_ShapeKey_data_length(PointerRNA *ptr)
{
	Key *key= rna_ShapeKey_find_key(ptr->id.data);
	KeyBlock *kb= (KeyBlock*)ptr->data;
	Curve *cu;
	Nurb *nu;
	int tot= kb->totelem;

	if(GS(key->from->name) == ID_CU) {
		cu= (Curve*)key->from;
		nu= cu->nurb.first;

		if(nu->bezt)
			tot /= 3;
	}

	return tot;
}

static PointerRNA rna_ShapeKey_data_get(CollectionPropertyIterator *iter)
{
	Key *key= rna_ShapeKey_find_key(iter->parent.id.data);
	StructRNA *type;
	Curve *cu;
	Nurb *nu;

	if(GS(key->from->name) == ID_CU) {
		cu= (Curve*)key->from;
		nu= cu->nurb.first;

		if(nu->bezt)
			type= &RNA_ShapeKeyBezierPoint;
		else
			type= &RNA_ShapeKeyCurvePoint;
	}
	else
		type= &RNA_ShapeKeyPoint;
	
	return rna_pointer_inherit_refine(&iter->parent, type, rna_iterator_array_get(iter));
}

static char *rna_ShapeKey_path(PointerRNA *ptr)
{
	KeyBlock *kb= (KeyBlock *)ptr->data;
	ID *id= ptr->id.data;
	
	if ((id) && (GS(id->name) != ID_KE))
		return BLI_sprintfN("shape_keys.keys[\"%s\"]", kb->name);
	else
		return BLI_sprintfN("keys[\"%s\"]", kb->name);
}

static void rna_Key_update_data(Main *bmain, Scene *scene, PointerRNA *ptr)
{
	Key *key= ptr->id.data;
	Object *ob;

	for(ob=bmain->object.first; ob; ob= ob->id.next) {
		if(ob_get_key(ob) == key) {
			DAG_id_flush_update(&ob->id, OB_RECALC_DATA);
			WM_main_add_notifier(NC_OBJECT|ND_MODIFIER, ob);
		}
	}
}

#else

static void rna_def_keydata(BlenderRNA *brna)
{
	StructRNA *srna;
	PropertyRNA *prop;

	srna= RNA_def_struct(brna, "ShapeKeyPoint", NULL);
	RNA_def_struct_ui_text(srna, "Shape Key Point", "Point in a shape key");

	prop= RNA_def_property(srna, "co", PROP_FLOAT, PROP_TRANSLATION);
	RNA_def_property_array(prop, 3);
	RNA_def_property_float_funcs(prop, "rna_ShapeKeyPoint_co_get", "rna_ShapeKeyPoint_co_set", NULL);
	RNA_def_property_ui_text(prop, "Location", "");
	RNA_def_property_update(prop, 0, "rna_Key_update_data");

	srna= RNA_def_struct(brna, "ShapeKeyCurvePoint", NULL);
	RNA_def_struct_ui_text(srna, "Shape Key Curve Point", "Point in a shape key for curves");

	prop= RNA_def_property(srna, "co", PROP_FLOAT, PROP_TRANSLATION);
	RNA_def_property_array(prop, 3);
	RNA_def_property_float_funcs(prop, "rna_ShapeKeyPoint_co_get", "rna_ShapeKeyPoint_co_set", NULL);
	RNA_def_property_ui_text(prop, "Location", "");
	RNA_def_property_update(prop, 0, "rna_Key_update_data");

	prop= RNA_def_property(srna, "tilt", PROP_FLOAT, PROP_NONE);
	RNA_def_property_float_funcs(prop, "rna_ShapeKeyCurvePoint_tilt_get", "rna_ShapeKeyCurvePoint_tilt_set", NULL);
	RNA_def_property_ui_text(prop, "Tilt", "");
	RNA_def_property_update(prop, 0, "rna_Key_update_data");

	srna= RNA_def_struct(brna, "ShapeKeyBezierPoint", NULL);
	RNA_def_struct_ui_text(srna, "Shape Key Bezier Point", "Point in a shape key for bezier curves");

	prop= RNA_def_property(srna, "co", PROP_FLOAT, PROP_TRANSLATION);
	RNA_def_property_array(prop, 3);
	RNA_def_property_float_funcs(prop, "rna_ShapeKeyBezierPoint_co_get", "rna_ShapeKeyBezierPoint_co_set", NULL);
	RNA_def_property_ui_text(prop, "Location", "");
	RNA_def_property_update(prop, 0, "rna_Key_update_data");

	prop= RNA_def_property(srna, "handle_1_co", PROP_FLOAT, PROP_TRANSLATION);
	RNA_def_property_array(prop, 3);
	RNA_def_property_float_funcs(prop, "rna_ShapeKeyBezierPoint_handle_1_co_get", "rna_ShapeKeyBezierPoint_handle_1_co_set", NULL);
	RNA_def_property_ui_text(prop, "Handle 1 Location", "");
	RNA_def_property_update(prop, 0, "rna_Key_update_data");

	prop= RNA_def_property(srna, "handle_2_co", PROP_FLOAT, PROP_TRANSLATION);
	RNA_def_property_array(prop, 3);
	RNA_def_property_float_funcs(prop, "rna_ShapeKeyBezierPoint_handle_2_co_get", "rna_ShapeKeyBezierPoint_handle_2_co_set", NULL);
	RNA_def_property_ui_text(prop, "Handle 2 Location", "");
	RNA_def_property_update(prop, 0, "rna_Key_update_data");

	/* appears to be unused currently
	prop= RNA_def_property(srna, "tilt", PROP_FLOAT, PROP_NONE);
	RNA_def_property_float_funcs(prop, "rna_ShapeKeyBezierPoint_tilt_get", "rna_ShapeKeyBezierPoint_tilt_set", NULL);
	RNA_def_property_ui_text(prop, "Tilt", "");
	RNA_def_property_update(prop, 0, "rna_Key_update_data"); */
}

static void rna_def_keyblock(BlenderRNA *brna)
{
	StructRNA *srna;
	PropertyRNA *prop;

	static EnumPropertyItem prop_keyblock_type_items[] = {
		{KEY_LINEAR, "KEY_LINEAR", 0, "Linear", ""},
		{KEY_CARDINAL, "KEY_CARDINAL", 0, "Cardinal", ""},
		{KEY_BSPLINE, "KEY_BSPLINE", 0, "BSpline", ""},
		{0, NULL, 0, NULL, NULL}};

	srna= RNA_def_struct(brna, "ShapeKey", NULL);
	RNA_def_struct_ui_text(srna, "Shape Key", "Shape key in a shape keys datablock");
	RNA_def_struct_sdna(srna, "KeyBlock");
	RNA_def_struct_path_func(srna, "rna_ShapeKey_path");
	RNA_def_struct_ui_icon(srna, ICON_SHAPEKEY_DATA);

	prop= RNA_def_property(srna, "name", PROP_STRING, PROP_NONE);
	RNA_def_property_ui_text(prop, "Name", "");
	RNA_def_property_string_funcs(prop, NULL, NULL, "rna_ShapeKey_name_set");
	RNA_def_struct_name_property(srna, prop);

	/* keys need to be sorted to edit this */
	prop= RNA_def_property(srna, "frame", PROP_FLOAT, PROP_TIME);
	RNA_def_property_clear_flag(prop, PROP_EDITABLE);
	RNA_def_property_float_sdna(prop, NULL, "pos");
	RNA_def_property_ui_text(prop, "Frame", "Frame for absolute keys");
	RNA_def_property_update(prop, 0, "rna_Key_update_data");
	
	/* for now, this is editable directly, as users can set this even if they're not animating them (to test results) */
	prop= RNA_def_property(srna, "value", PROP_FLOAT, PROP_FACTOR);
	RNA_def_property_float_sdna(prop, NULL, "curval");
	RNA_def_property_float_funcs(prop, NULL, "rna_ShapeKey_value_set", "rna_ShapeKey_value_range");
	RNA_def_property_ui_range(prop, -10.0f, 10.0f, 10, 3);
	RNA_def_property_ui_text(prop, "Value", "Value of shape key at the current frame");
	RNA_def_property_update(prop, 0, "rna_Key_update_data");

	prop= RNA_def_property(srna, "interpolation", PROP_ENUM, PROP_NONE);
	RNA_def_property_enum_sdna(prop, NULL, "type");
	RNA_def_property_enum_items(prop, prop_keyblock_type_items);
	RNA_def_property_ui_text(prop, "Interpolation", "Interpolation type");
	RNA_def_property_update(prop, 0, "rna_Key_update_data");

	prop= RNA_def_property(srna, "vertex_group", PROP_STRING, PROP_NONE);
	RNA_def_property_string_sdna(prop, NULL, "vgroup");
	RNA_def_property_ui_text(prop, "Vertex Group", "Vertex weight group, to blend with basis shape");
	RNA_def_property_update(prop, 0, "rna_Key_update_data");

	prop= RNA_def_property(srna, "relative_key", PROP_POINTER, PROP_NONE);
	RNA_def_property_struct_type(prop, "ShapeKey");
	RNA_def_property_flag(prop, PROP_EDITABLE);
	RNA_def_property_pointer_funcs(prop, "rna_ShapeKey_relative_key_get", "rna_ShapeKey_relative_key_set", NULL);
	RNA_def_property_ui_text(prop, "Relative Key", "Shape used as a relative key");
	RNA_def_property_update(prop, 0, "rna_Key_update_data");

	prop= RNA_def_property(srna, "mute", PROP_BOOLEAN, PROP_NONE);
	RNA_def_property_boolean_sdna(prop, NULL, "flag", KEYBLOCK_MUTE);
	RNA_def_property_ui_text(prop, "Mute", "Mute this shape key");
	RNA_def_property_ui_icon(prop, ICON_MUTE_IPO_OFF, 1);
	RNA_def_property_update(prop, 0, "rna_Key_update_data");

	prop= RNA_def_property(srna, "slider_min", PROP_FLOAT, PROP_NONE);
	RNA_def_property_float_sdna(prop, NULL, "slidermin");
	RNA_def_property_range(prop, -10.0f, 10.0f);
	RNA_def_property_ui_text(prop, "Slider Min", "Minimum for slider");

	prop= RNA_def_property(srna, "slider_max", PROP_FLOAT, PROP_NONE);
	RNA_def_property_float_sdna(prop, NULL, "slidermax");
	RNA_def_property_range(prop, -10.0f, 10.0f);
	RNA_def_property_float_default(prop, 1.0f);
	RNA_def_property_ui_text(prop, "Slider Max", "Maximum for slider");

	prop= RNA_def_property(srna, "data", PROP_COLLECTION, PROP_NONE);
	RNA_def_property_collection_sdna(prop, NULL, "data", "totelem");
	RNA_def_property_struct_type(prop, "UnknownType");
	RNA_def_property_ui_text(prop, "Data", "");
	RNA_def_property_collection_funcs(prop, "rna_ShapeKey_data_begin", 0, 0, "rna_ShapeKey_data_get", "rna_ShapeKey_data_length", 0, 0);
}

static void rna_def_key(BlenderRNA *brna)
{
	StructRNA *srna;
	PropertyRNA *prop;

	srna= RNA_def_struct(brna, "Key", "ID");
	RNA_def_struct_ui_text(srna, "Key", "Shape keys datablock containing different shapes of geometric datablocks");
	RNA_def_struct_ui_icon(srna, ICON_SHAPEKEY_DATA);

	prop= RNA_def_property(srna, "reference_key", PROP_POINTER, PROP_NONE);
	RNA_def_property_flag(prop, PROP_NEVER_NULL);
	RNA_def_property_clear_flag(prop, PROP_EDITABLE);
	RNA_def_property_pointer_sdna(prop, NULL, "refkey");
	RNA_def_property_ui_text(prop, "Reference Key", "");

	prop= RNA_def_property(srna, "keys", PROP_COLLECTION, PROP_NONE);
	RNA_def_property_collection_sdna(prop, NULL, "block", NULL);
	RNA_def_property_struct_type(prop, "ShapeKey");
	RNA_def_property_ui_text(prop, "Keys", "Shape keys");

	rna_def_animdata_common(srna);

	prop= RNA_def_property(srna, "user", PROP_POINTER, PROP_NONE);
	RNA_def_property_flag(prop, PROP_NEVER_NULL);
	RNA_def_property_pointer_sdna(prop, NULL, "from");
	RNA_def_property_ui_text(prop, "User", "Datablock using these shape keys");

	prop= RNA_def_property(srna, "relative", PROP_BOOLEAN, PROP_NONE);
	RNA_def_property_boolean_sdna(prop, NULL, "type", KEY_RELATIVE);
	RNA_def_property_ui_text(prop, "Relative", "Makes shape keys relative");
	RNA_def_property_update(prop, 0, "rna_Key_update_data");

	prop= RNA_def_property(srna, "slurph", PROP_INT, PROP_UNSIGNED);
	RNA_def_property_int_sdna(prop, NULL, "slurph");
	RNA_def_property_range(prop, -500, 500);
	RNA_def_property_ui_text(prop, "Slurph", "Creates a delay in amount of frames in applying keypositions, first vertex goes first");
	RNA_def_property_update(prop, 0, "rna_Key_update_data");
}

void RNA_def_key(BlenderRNA *brna)
{
	rna_def_key(brna);
	rna_def_keyblock(brna);
	rna_def_keydata(brna);
}

#endif

