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

#ifndef __RNA_DEFINE_H__
#define __RNA_DEFINE_H__

/** \file RNA_define.h
 *  \ingroup RNA
 *  Functions used during preprocess and runtime, for defining the RNA. */

#include <float.h>
#include <limits.h>

#include "DNA_listBase.h"
#include "RNA_types.h"

#ifdef __cplusplus
extern "C" {
#endif

/* Blender RNA */

BlenderRNA *RNA_create(void);
void RNA_define_free(BlenderRNA *brna);
void RNA_free(BlenderRNA *brna);
void RNA_define_verify_sdna(bool verify);
void RNA_define_animate_sdna(bool animate);

void RNA_init(void);
void RNA_exit(void);

/* Struct */

StructRNA *RNA_def_struct_ptr(BlenderRNA *brna, const char *identifier, StructRNA *srnafrom);
StructRNA *RNA_def_struct(BlenderRNA *brna, const char *identifier, const char *from);
void RNA_def_struct_sdna(StructRNA *srna, const char *structname);
void RNA_def_struct_sdna_from(StructRNA *srna, const char *structname, const char *propname);
void RNA_def_struct_name_property(StructRNA *srna, PropertyRNA *prop);
void RNA_def_struct_nested(BlenderRNA *brna, StructRNA *srna, const char *structname);
void RNA_def_struct_flag(StructRNA *srna, int flag);
void RNA_def_struct_clear_flag(StructRNA *srna, int flag);
void RNA_def_struct_refine_func(StructRNA *srna, const char *refine);
void RNA_def_struct_idprops_func(StructRNA *srna, const char *refine);
void RNA_def_struct_register_funcs(StructRNA *srna, const char *reg, const char *unreg, const char *instance);
void RNA_def_struct_path_func(StructRNA *srna, const char *path);
void RNA_def_struct_identifier(StructRNA *srna, const char *identifier);
void RNA_def_struct_ui_text(StructRNA *srna, const char *name, const char *description);
void RNA_def_struct_ui_icon(StructRNA *srna, int icon);
void RNA_struct_free_extension(StructRNA *srna, ExtensionRNA *ext);
void RNA_struct_free(BlenderRNA *brna, StructRNA *srna);

void RNA_def_struct_translation_context(StructRNA *srna, const char *context);

/* Compact Property Definitions */

typedef void StructOrFunctionRNA;

PropertyRNA *RNA_def_boolean(StructOrFunctionRNA *cont, const char *identifier, int default_value, const char *ui_name, const char *ui_description);
PropertyRNA *RNA_def_boolean_array(StructOrFunctionRNA *cont, const char *identifier, int len, int *default_value, const char *ui_name, const char *ui_description);
PropertyRNA *RNA_def_boolean_layer(StructOrFunctionRNA *cont, const char *identifier, int len, int *default_value, const char *ui_name, const char *ui_description);
PropertyRNA *RNA_def_boolean_layer_member(StructOrFunctionRNA *cont, const char *identifier, int len, int *default_value, const char *ui_name, const char *ui_description);
PropertyRNA *RNA_def_boolean_vector(StructOrFunctionRNA *cont, const char *identifier, int len, int *default_value, const char *ui_name, const char *ui_description);

PropertyRNA *RNA_def_int(StructOrFunctionRNA *cont, const char *identifier, int default_value, int hardmin, int hardmax, const char *ui_name, const char *ui_description, int softmin, int softmax);
PropertyRNA *RNA_def_int_vector(StructOrFunctionRNA *cont, const char *identifier, int len, const int *default_value, int hardmin, int hardmax, const char *ui_name, const char *ui_description, int softmin, int softmax);
PropertyRNA *RNA_def_int_array(StructOrFunctionRNA *cont, const char *identifier, int len, const int *default_value, int hardmin, int hardmax, const char *ui_name, const char *ui_description, int softmin, int softmax);

PropertyRNA *RNA_def_string(StructOrFunctionRNA *cont, const char *identifier, const char *default_value, int maxlen, const char *ui_name, const char *ui_description);
PropertyRNA *RNA_def_string_file_path(StructOrFunctionRNA *cont, const char *identifier, const char *default_value, int maxlen, const char *ui_name, const char *ui_description);
PropertyRNA *RNA_def_string_dir_path(StructOrFunctionRNA *cont, const char *identifier, const char *default_value, int maxlen, const char *ui_name, const char *ui_description);
PropertyRNA *RNA_def_string_file_name(StructOrFunctionRNA *cont, const char *identifier, const char *default_value, int maxlen, const char *ui_name, const char *ui_description);

PropertyRNA *RNA_def_enum(StructOrFunctionRNA *cont, const char *identifier, const EnumPropertyItem *items, int default_value, const char *ui_name, const char *ui_description);
PropertyRNA *RNA_def_enum_flag(StructOrFunctionRNA *cont, const char *identifier, const EnumPropertyItem *items, int default_value, const char *ui_name, const char *ui_description);
void RNA_def_enum_funcs(PropertyRNA *prop, EnumPropertyItemFunc itemfunc);

PropertyRNA *RNA_def_float(StructOrFunctionRNA *cont, const char *identifier, float default_value, float hardmin, float hardmax, const char *ui_name, const char *ui_description, float softmin, float softmax);
PropertyRNA *RNA_def_float_vector(StructOrFunctionRNA *cont, const char *identifier, int len, const float *default_value, float hardmin, float hardmax, const char *ui_name, const char *ui_description, float softmin, float softmax);
PropertyRNA *RNA_def_float_vector_xyz(StructOrFunctionRNA *cont, const char *identifier, int len, const float *default_value, float hardmin, float hardmax, const char *ui_name, const char *ui_description, float softmin, float softmax);
PropertyRNA *RNA_def_float_color(StructOrFunctionRNA *cont, const char *identifier, int len, const float *default_value, float hardmin, float hardmax, const char *ui_name, const char *ui_description, float softmin, float softmax);
PropertyRNA *RNA_def_float_matrix(StructOrFunctionRNA *cont, const char *identifier, int rows, int columns, const float *default_value, float hardmin, float hardmax, const char *ui_name, const char *ui_description, float softmin, float softmax);
PropertyRNA *RNA_def_float_rotation(StructOrFunctionRNA *cont, const char *identifier, int len, const float *default_value,
	float hardmin, float hardmax, const char *ui_name, const char *ui_description, float softmin, float softmax);
PropertyRNA *RNA_def_float_array(StructOrFunctionRNA *cont, const char *identifier, int len, const float *default_value,
	float hardmin, float hardmax, const char *ui_name, const char *ui_description, float softmin, float softmax);

//PropertyRNA *RNA_def_float_dynamic_array(StructOrFunctionRNA *cont, const char *identifier, float hardmin, float hardmax,
//	const char *ui_name, const char *ui_description, float softmin, float softmax, unsigned int dimension, unsigned short dim_size[]);

PropertyRNA *RNA_def_float_percentage(StructOrFunctionRNA *cont, const char *identifier, float default_value, float hardmin, float hardmax,
	const char *ui_name, const char *ui_description, float softmin, float softmax);
PropertyRNA *RNA_def_float_factor(StructOrFunctionRNA *cont, const char *identifier, float default_value, float hardmin, float hardmax,
	const char *ui_name, const char *ui_description, float softmin, float softmax);

PropertyRNA *RNA_def_pointer(StructOrFunctionRNA *cont, const char *identifier, const char *type,
	const char *ui_name, const char *ui_description);
PropertyRNA *RNA_def_pointer_runtime(StructOrFunctionRNA *cont, const char *identifier, StructRNA *type,
	const char *ui_name, const char *ui_description);

PropertyRNA *RNA_def_collection(StructOrFunctionRNA *cont, const char *identifier, const char *type,
	const char *ui_name, const char *ui_description);
PropertyRNA *RNA_def_collection_runtime(StructOrFunctionRNA *cont, const char *identifier, StructRNA *type,
	const char *ui_name, const char *ui_description);

/* Extended Property Definitions */

PropertyRNA *RNA_def_property(StructOrFunctionRNA *cont, const char *identifier, int type, int subtype);

void RNA_def_property_boolean_sdna(PropertyRNA *prop, const char *structname, const char *propname, int bit);
void RNA_def_property_boolean_negative_sdna(PropertyRNA *prop, const char *structname, const char *propname, int bit);
void RNA_def_property_int_sdna(PropertyRNA *prop, const char *structname, const char *propname);
void RNA_def_property_float_sdna(PropertyRNA *prop, const char *structname, const char *propname);
void RNA_def_property_string_sdna(PropertyRNA *prop, const char *structname, const char *propname);
void RNA_def_property_enum_sdna(PropertyRNA *prop, const char *structname, const char *propname);
void RNA_def_property_enum_bitflag_sdna(PropertyRNA *prop, const char *structname, const char *propname);
void RNA_def_property_pointer_sdna(PropertyRNA *prop, const char *structname, const char *propname);
void RNA_def_property_collection_sdna(PropertyRNA *prop, const char *structname, const char *propname, const char *lengthpropname);

void RNA_def_property_flag(PropertyRNA *prop, int flag);
void RNA_def_property_clear_flag(PropertyRNA *prop, int flag);
void RNA_def_property_subtype(PropertyRNA *prop, PropertySubType subtype);
void RNA_def_property_array(PropertyRNA *prop, int length);
void RNA_def_property_multi_array(PropertyRNA *prop, int dimension, const int length[]);
void RNA_def_property_range(PropertyRNA *prop, double min, double max);

void RNA_def_property_enum_items(PropertyRNA *prop, const EnumPropertyItem *item);
void RNA_def_property_string_maxlength(PropertyRNA *prop, int maxlength);
void RNA_def_property_struct_type(PropertyRNA *prop, const char *type);
void RNA_def_property_struct_runtime(PropertyRNA *prop, StructRNA *type);

void RNA_def_property_boolean_default(PropertyRNA *prop, int value);
void RNA_def_property_boolean_array_default(PropertyRNA *prop, const int *array);
void RNA_def_property_int_default(PropertyRNA *prop, int value);
void RNA_def_property_int_array_default(PropertyRNA *prop, const int *array);
void RNA_def_property_float_default(PropertyRNA *prop, float value);
void RNA_def_property_float_array_default(PropertyRNA *prop, const float *array);
void RNA_def_property_enum_default(PropertyRNA *prop, int value);
void RNA_def_property_string_default(PropertyRNA *prop, const char *value);

void RNA_def_property_ui_text(PropertyRNA *prop, const char *name, const char *description);
void RNA_def_property_ui_range(PropertyRNA *prop, double min, double max, double step, int precision);
void RNA_def_property_ui_icon(PropertyRNA *prop, int icon, bool consecutive);

void RNA_def_property_update(PropertyRNA *prop, int noteflag, const char *updatefunc);
void RNA_def_property_editable_func(PropertyRNA *prop, const char *editable);
void RNA_def_property_editable_array_func(PropertyRNA *prop, const char *editable);

void RNA_def_property_update_runtime(PropertyRNA *prop, const void *func);

void RNA_def_property_dynamic_array_funcs(PropertyRNA *prop, const char *getlength);
void RNA_def_property_boolean_funcs(PropertyRNA *prop, const char *get, const char *set);
void RNA_def_property_int_funcs(PropertyRNA *prop, const char *get, const char *set, const char *range);
void RNA_def_property_float_funcs(PropertyRNA *prop, const char *get, const char *set, const char *range);
void RNA_def_property_enum_funcs(PropertyRNA *prop, const char *get, const char *set, const char *item);
void RNA_def_property_string_funcs(PropertyRNA *prop, const char *get, const char *length, const char *set);
void RNA_def_property_pointer_funcs(PropertyRNA *prop, const char *get, const char *set, const char *typef, const char *poll);
void RNA_def_property_collection_funcs(PropertyRNA *prop, const char *begin, const char *next, const char *end, const char *get, const char *length, const char *lookupint, const char *lookupstring, const char *assignint);
void RNA_def_property_srna(PropertyRNA *prop, const char *type);
void RNA_def_py_data(PropertyRNA *prop, void *py_data);

void RNA_def_property_boolean_funcs_runtime(PropertyRNA *prop, BooleanPropertyGetFunc getfunc, BooleanPropertySetFunc setfunc);
void RNA_def_property_boolean_array_funcs_runtime(PropertyRNA *prop, BooleanArrayPropertyGetFunc getfunc, BooleanArrayPropertySetFunc setfunc);
void RNA_def_property_int_funcs_runtime(PropertyRNA *prop, IntPropertyGetFunc getfunc, IntPropertySetFunc setfunc, IntPropertyRangeFunc rangefunc);
void RNA_def_property_int_array_funcs_runtime(PropertyRNA *prop, IntArrayPropertyGetFunc getfunc, IntArrayPropertySetFunc setfunc, IntPropertyRangeFunc rangefunc);
void RNA_def_property_float_funcs_runtime(PropertyRNA *prop, FloatPropertyGetFunc getfunc, FloatPropertySetFunc setfunc, FloatPropertyRangeFunc rangefunc);
void RNA_def_property_float_array_funcs_runtime(PropertyRNA *prop, FloatArrayPropertyGetFunc getfunc, FloatArrayPropertySetFunc setfunc, FloatPropertyRangeFunc rangefunc);
void RNA_def_property_enum_funcs_runtime(PropertyRNA *prop, EnumPropertyGetFunc getfunc, EnumPropertySetFunc setfunc, EnumPropertyItemFunc itemfunc);
void RNA_def_property_string_funcs_runtime(PropertyRNA *prop, StringPropertyGetFunc getfunc, StringPropertyLengthFunc lengthfunc, StringPropertySetFunc setfunc);

void RNA_def_property_enum_py_data(PropertyRNA *prop, void *py_data);

void RNA_def_property_translation_context(PropertyRNA *prop, const char *context);

/* Function */

FunctionRNA *RNA_def_function(StructRNA *srna, const char *identifier, const char *call);
FunctionRNA *RNA_def_function_runtime(StructRNA *srna, const char *identifier, CallFunc call);
void RNA_def_function_return(FunctionRNA *func, PropertyRNA *ret);
void RNA_def_function_output(FunctionRNA *func, PropertyRNA *ret);
void RNA_def_function_flag(FunctionRNA *func, int flag);
void RNA_def_function_ui_description(FunctionRNA *func, const char *description);

/* Dynamic Enums
 * strings are not freed, assumed pointing to static location. */

void RNA_enum_item_add(EnumPropertyItem **items, int *totitem, const EnumPropertyItem *item);
void RNA_enum_item_add_separator(EnumPropertyItem **items, int *totitem);
void RNA_enum_items_add(EnumPropertyItem **items, int *totitem, EnumPropertyItem *item);
void RNA_enum_items_add_value(EnumPropertyItem **items, int *totitem, EnumPropertyItem *item, int value);
void RNA_enum_item_end(EnumPropertyItem **items, int *totitem);

/* Memory management */

void RNA_def_struct_duplicate_pointers(StructRNA *srna);
void RNA_def_struct_free_pointers(StructRNA *srna);
void RNA_def_func_duplicate_pointers(FunctionRNA *func);
void RNA_def_func_free_pointers(FunctionRNA *func);
void RNA_def_property_duplicate_pointers(StructOrFunctionRNA *cont_, PropertyRNA *prop);
void RNA_def_property_free_pointers(PropertyRNA *prop);
int RNA_def_property_free_identifier(StructOrFunctionRNA *cont_, const char *identifier);

/* utilities */
const char *RNA_property_typename(PropertyType type);
#define IS_DNATYPE_FLOAT_COMPAT(_str) (strcmp(_str, "float") == 0 || strcmp(_str, "double") == 0)
#define IS_DNATYPE_INT_COMPAT(_str) (strcmp(_str, "int") == 0 || strcmp(_str, "short") == 0 || strcmp(_str, "char") == 0)

void RNA_identifier_sanitize(char *identifier, int property);

extern const int rna_matrix_dimsize_3x3[];
extern const int rna_matrix_dimsize_4x4[];
extern const int rna_matrix_dimsize_4x2[];

/* max size for dynamic defined type descriptors,
 * this value is arbitrary */
#define RNA_DYN_DESCR_MAX 240

#ifdef __cplusplus
}
#endif

#endif /* __RNA_DEFINE_H__ */

