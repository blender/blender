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
 * Inc., 59 Temple Place - Suite 330, Boston, MA  02111-1307, USA.
 *
 * Contributor(s): Blender Foundation (2008).
 *
 * ***** END GPL LICENSE BLOCK *****
 */

#ifndef RNA_ACCESS
#define RNA_ACCESS

#include "RNA_types.h"

struct bContext;
struct Main;

extern BlenderRNA BLENDER_RNA;

/* Pointer
 *
 * Currently only an RNA pointer to Main can be obtained, this
 * should  be extended to allow making other pointers as well. */

void RNA_pointer_main_get(struct Main *main, PointerRNA *r_ptr);

/* Structs */

const char *RNA_struct_identifier(PointerRNA *ptr);
const char *RNA_struct_ui_name(PointerRNA *ptr);

PropertyRNA *RNA_struct_name_property(PointerRNA *ptr);
PropertyRNA *RNA_struct_iterator_property(PointerRNA *ptr);

PropertyRNA *RNA_struct_find_property(PointerRNA *ptr, const char *identifier);

/* Properties
 *
 * Access to struct properties. All this works with RNA pointers rather than
 * direct pointers to the data. */

/* Property Information */

const char *RNA_property_identifier(PropertyRNA *prop, PointerRNA *ptr);
PropertyType RNA_property_type(PropertyRNA *prop, PointerRNA *ptr);
PropertySubType RNA_property_subtype(PropertyRNA *prop, PointerRNA *ptr);

int RNA_property_array_length(PropertyRNA *prop, PointerRNA *ptr);

void RNA_property_int_range(PropertyRNA *prop, PointerRNA *ptr, int *hardmin, int *hardmax);
void RNA_property_int_ui_range(PropertyRNA *prop, PointerRNA *ptr, int *softmin, int *softmax, int *step);

void RNA_property_float_range(PropertyRNA *prop, PointerRNA *ptr, float *hardmin, float *hardmax);
void RNA_property_float_ui_range(PropertyRNA *prop, PointerRNA *ptr, float *softmin, float *softmax, float *step, float *precision);

int RNA_property_string_maxlength(PropertyRNA *prop, PointerRNA *ptr);

void RNA_property_enum_items(PropertyRNA *prop, PointerRNA *ptr, const EnumPropertyItem **item, int *totitem);

const char *RNA_property_ui_name(PropertyRNA *prop, PointerRNA *ptr);
const char *RNA_property_ui_description(PropertyRNA *prop, PointerRNA *ptr);

int RNA_property_editable(PropertyRNA *prop, PointerRNA *ptr);
int RNA_property_evaluated(PropertyRNA *prop, PointerRNA *ptr);

void RNA_property_notify(PropertyRNA *prop, struct bContext *C, PointerRNA *ptr);

/* Property Data */

int RNA_property_boolean_get(PropertyRNA *prop, PointerRNA *ptr);
void RNA_property_boolean_set(PropertyRNA *prop, PointerRNA *ptr, int value);
int RNA_property_boolean_get_array(PropertyRNA *prop, PointerRNA *ptr, int index);
void RNA_property_boolean_set_array(PropertyRNA *prop, PointerRNA *ptr, int index, int value);

int RNA_property_int_get(PropertyRNA *prop, PointerRNA *ptr);
void RNA_property_int_set(PropertyRNA *prop, PointerRNA *ptr, int value);
int RNA_property_int_get_array(PropertyRNA *prop, PointerRNA *ptr, int index);
void RNA_property_int_set_array(PropertyRNA *prop, PointerRNA *ptr, int index, int value);

float RNA_property_float_get(PropertyRNA *prop, PointerRNA *ptr);
void RNA_property_float_set(PropertyRNA *prop, PointerRNA *ptr, float value);
float RNA_property_float_get_array(PropertyRNA *prop, PointerRNA *ptr, int index);
void RNA_property_float_set_array(PropertyRNA *prop, PointerRNA *ptr, int index, float value);

void RNA_property_string_get(PropertyRNA *prop, PointerRNA *ptr, char *value);
char *RNA_property_string_get_alloc(PropertyRNA *prop, PointerRNA *ptr, char *fixedbuf, int fixedlen);
int RNA_property_string_length(PropertyRNA *prop, PointerRNA *ptr);
void RNA_property_string_set(PropertyRNA *prop, PointerRNA *ptr, const char *value);

int RNA_property_enum_get(PropertyRNA *prop, PointerRNA *ptr);
void RNA_property_enum_set(PropertyRNA *prop, PointerRNA *ptr, int value);

void RNA_property_pointer_get(PropertyRNA *prop, PointerRNA *ptr, PointerRNA *r_ptr);
void RNA_property_pointer_set(PropertyRNA *prop, PointerRNA *ptr, PointerRNA *ptr_value);

void RNA_property_collection_begin(PropertyRNA *prop, CollectionPropertyIterator *iter, PointerRNA *ptr);
void RNA_property_collection_next(PropertyRNA *prop, CollectionPropertyIterator *iter);
void RNA_property_collection_end(PropertyRNA *prop, CollectionPropertyIterator *iter);
int RNA_property_collection_length(PropertyRNA *prop, PointerRNA *ptr);
int RNA_property_collection_lookup_int(PropertyRNA *prop, PointerRNA *ptr, int key, PointerRNA *r_ptr);
int RNA_property_collection_lookup_string(PropertyRNA *prop, PointerRNA *ptr, const char *key, PointerRNA *r_ptr);

/* Path
 *
 * Experimental method to refer to structs and properties with a string,
 * using a syntax like: scenes[0].objects["Cube"].data.verts[7].co
 *
 * This provides a way to refer to RNA data while being detached from any
 * particular pointers, which is useful in a number of applications, like
 * UI code or Actions, though efficiency is a concern. */

char *RNA_path_append(const char *path, PointerRNA *ptr, PropertyRNA *prop,
	int intkey, const char *strkey);
char *RNA_path_back(const char *path);

int RNA_path_resolve(PointerRNA *ptr, const char *path,
	PointerRNA *r_ptr, PropertyRNA **r_prop);

#if 0
/* Dependency
 *
 * Experimental code that will generate callbacks for each dependency
 * between ID types. This may end up being useful for UI
 * and evaluation code that needs to know such dependencies for correct
 * redraws and re-evaluations. */

typedef void (*PropDependencyCallback)(void *udata, PointerRNA *from, PointerRNA *to);
void RNA_test_dependencies_cb(void *udata, PointerRNA *from, PointerRNA *to);

void RNA_generate_dependencies(PointerRNA *mainptr, void *udata, PropDependencyCallback cb);
#endif

/* Quick name based property access
 *
 * These are just an easier way to access property values without having to
 * call RNA_struct_find_property. The names have to exist as RNA properties
 * for the type in the pointer, if they do not exist an error will be printed.
 *
 * The get and set functions work like the corresponding functions above, the
 * default functions are intended to be used for runtime / id properties
 * specifically. They will set the value only if the id property does not yet
 * exist, and return the current value. This is useful to set inputs in an
 * operator, avoiding to overwrite them if they were specified by the caller.
 *
 * There is no support for pointers and collections here yet, these can be 
 * added when ID properties support them. */

int RNA_boolean_get(PointerRNA *ptr, const char *name);
void RNA_boolean_set(PointerRNA *ptr, const char *name, int value);
int RNA_boolean_default(PointerRNA *ptr, const char *name, int value);
void RNA_boolean_get_array(PointerRNA *ptr, const char *name, int *values);
void RNA_boolean_set_array(PointerRNA *ptr, const char *name, const int *values);
void RNA_boolean_default_array(PointerRNA *ptr, const char *name, int *values);

int RNA_int_get(PointerRNA *ptr, const char *name);
void RNA_int_set(PointerRNA *ptr, const char *name, int value);
int RNA_int_default(PointerRNA *ptr, const char *name, int value);
void RNA_int_get_array(PointerRNA *ptr, const char *name, int *values);
void RNA_int_set_array(PointerRNA *ptr, const char *name, const int *values);
void RNA_int_default_array(PointerRNA *ptr, const char *name, int *values);

float RNA_float_get(PointerRNA *ptr, const char *name);
void RNA_float_set(PointerRNA *ptr, const char *name, float value);
float RNA_float_default(PointerRNA *ptr, const char *name, float value);
void RNA_float_get_array(PointerRNA *ptr, const char *name, float *values);
void RNA_float_set_array(PointerRNA *ptr, const char *name, const float *values);
void RNA_float_default_array(PointerRNA *ptr, const char *name, float *values);

int RNA_enum_get(PointerRNA *ptr, const char *name);
void RNA_enum_set(PointerRNA *ptr, const char *name, int value);
int RNA_enum_default(PointerRNA *ptr, const char *name, int value);

void RNA_string_get(PointerRNA *ptr, const char *name, char *value);
char *RNA_string_get_alloc(PointerRNA *ptr, const char *name, char *fixedbuf, int fixedlen);
int RNA_string_length(PointerRNA *ptr, const char *name);
void RNA_string_set(PointerRNA *ptr, const char *name, const char *value);
void RNA_string_default(PointerRNA *ptr, const char *name, const char *value);

/* check if the idproperty exists, for operators */
int RNA_property_is_set(PointerRNA *ptr, const char *name);

#endif /* RNA_ACCESS */

