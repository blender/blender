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

/* Pointer
 *
 * Currently only an RNA pointer to Main can be obtained, this
 * should  be extended to allow making other pointers as well. */

void RNA_pointer_main_get(struct Main *main, PointerRNA *r_ptr);

/* Structs */

const char *RNA_struct_cname(PointerRNA *ptr);
const char *RNA_struct_ui_name(PointerRNA *ptr);

PropertyRNA *RNA_struct_name_property(PointerRNA *ptr);
PropertyRNA *RNA_struct_iterator_property(PointerRNA *ptr);

/* Properties
 *
 * Access to struct properties. All this works with RNA pointers rather than
 * direct pointers to the data. */

/* Property Information */

const char *RNA_property_cname(PropertyRNA *prop, PointerRNA *ptr);
PropertyType RNA_property_type(PropertyRNA *prop, PointerRNA *ptr);
PropertySubType RNA_property_subtype(PropertyRNA *prop, PointerRNA *ptr);

int RNA_property_array_length(PropertyRNA *prop, PointerRNA *ptr);

void RNA_property_int_range(PropertyRNA *prop, PointerRNA *ptr, int *hardmin, int *hardmax);
void RNA_property_int_ui_range(PropertyRNA *prop, PointerRNA *ptr, int *softmin, int *softmax, int *step);

void RNA_property_float_range(PropertyRNA *prop, PointerRNA *ptr, float *hardmin, float *hardmax);
void RNA_property_float_ui_range(PropertyRNA *prop, PointerRNA *ptr, float *softmin, float *softmax, float *step, float *precision);

int RNA_property_string_maxlength(PropertyRNA *prop, PointerRNA *ptr);

void RNA_property_enum_items(PropertyRNA *prop, PointerRNA *ptr, const PropertyEnumItem **item, int *totitem);

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
StructRNA *RNA_property_pointer_type(PropertyRNA *prop, PointerRNA *ptr);

void RNA_property_collection_begin(PropertyRNA *prop, CollectionPropertyIterator *iter, PointerRNA *ptr);
void RNA_property_collection_next(PropertyRNA *prop, CollectionPropertyIterator *iter);
void RNA_property_collection_end(PropertyRNA *prop, CollectionPropertyIterator *iter);
void RNA_property_collection_get(PropertyRNA *prop, CollectionPropertyIterator *iter, PointerRNA *r_ptr);
StructRNA *RNA_property_collection_type(PropertyRNA *prop, CollectionPropertyIterator *iter);
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

char *RNA_path_append(const char *path, PropertyRNA *prop, int intkey, const char *strkey);
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

#endif /* RNA_ACCESS */

