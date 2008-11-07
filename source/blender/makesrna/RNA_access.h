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

struct bContext;
struct BlenderRNA;
struct StructRNA;
struct PropertyRNA;
struct PointerRNA;
struct CollectionPropertyIterator;
struct Main;

/* Pointer
 *
 * Currently only an RNA pointer to Main can be obtained, this
 * should  be extended to allow making other pointers as well. */

void RNA_pointer_main_get(struct Main *main, struct PointerRNA *r_ptr);

/* Property
 *
 * Access to struct properties. All this works with RNA pointers rather than
 * direct pointers to the data. */

int RNA_property_editable(struct PropertyRNA *prop, struct PointerRNA *ptr);
int RNA_property_evaluated(struct PropertyRNA *prop, struct PointerRNA *ptr);

void RNA_property_notify(struct PropertyRNA *prop, struct bContext *C, struct PointerRNA *ptr);

int RNA_property_boolean_get(struct PropertyRNA *prop, struct PointerRNA *ptr);
void RNA_property_boolean_set(struct PropertyRNA *prop, struct PointerRNA *ptr, int value);
int RNA_property_boolean_get_array(struct PropertyRNA *prop, struct PointerRNA *ptr, int index);
void RNA_property_boolean_set_array(struct PropertyRNA *prop, struct PointerRNA *ptr, int index, int value);

int RNA_property_int_get(struct PropertyRNA *prop, struct PointerRNA *ptr);
void RNA_property_int_set(struct PropertyRNA *prop, struct PointerRNA *ptr, int value);
int RNA_property_int_get_array(struct PropertyRNA *prop, struct PointerRNA *ptr, int index);
void RNA_property_int_set_array(struct PropertyRNA *prop, struct PointerRNA *ptr, int index, int value);

float RNA_property_float_get(struct PropertyRNA *prop, struct PointerRNA *ptr);
void RNA_property_float_set(struct PropertyRNA *prop, struct PointerRNA *ptr, float value);
float RNA_property_float_get_array(struct PropertyRNA *prop, struct PointerRNA *ptr, int index);
void RNA_property_float_set_array(struct PropertyRNA *prop, struct PointerRNA *ptr, int index, float value);

void RNA_property_string_get(struct PropertyRNA *prop, struct PointerRNA *ptr, char *value);
int RNA_property_string_length(struct PropertyRNA *prop, struct PointerRNA *ptr);
void RNA_property_string_set(struct PropertyRNA *prop, struct PointerRNA *ptr, const char *value);

int RNA_property_enum_get(struct PropertyRNA *prop, struct PointerRNA *ptr);
void RNA_property_enum_set(struct PropertyRNA *prop, struct PointerRNA *ptr, int value);

void RNA_property_pointer_get(struct PropertyRNA *prop, struct PointerRNA *ptr, struct PointerRNA *r_ptr);
void RNA_property_pointer_set(struct PropertyRNA *prop, struct PointerRNA *ptr, struct PointerRNA *ptr_value);
struct StructRNA *RNA_property_pointer_type(struct PropertyRNA *prop, struct PointerRNA *ptr);

void RNA_property_collection_begin(struct PropertyRNA *prop, struct CollectionPropertyIterator *iter, struct PointerRNA *ptr);
void RNA_property_collection_next(struct PropertyRNA *prop, struct CollectionPropertyIterator *iter);
void RNA_property_collection_end(struct PropertyRNA *prop, struct CollectionPropertyIterator *iter);
void RNA_property_collection_get(struct PropertyRNA *prop, struct CollectionPropertyIterator *iter, struct PointerRNA *r_ptr);
struct StructRNA *RNA_property_collection_type(struct PropertyRNA *prop, struct CollectionPropertyIterator *iter);
int RNA_property_collection_length(struct PropertyRNA *prop, struct PointerRNA *ptr);
int RNA_property_collection_lookup_int(struct PropertyRNA *prop, struct PointerRNA *ptr, int key, struct PointerRNA *r_ptr);
int RNA_property_collection_lookup_string(struct PropertyRNA *prop, struct PointerRNA *ptr, const char *key, struct PointerRNA *r_ptr);

/* Path
 *
 * Experimental method to refer to structs and properties with a string,
 * using a syntax like: scenes[0].objects["Cube"].data.verts[7].co
 *
 * This provides a way to refer to RNA data while being detached from any
 * particular pointers, which is useful in a number of applications, like
 * UI code or Actions, though efficiency is a concern. */

char *RNA_path_append(const char *path, struct PropertyRNA *prop, int intkey, const char *strkey);
char *RNA_path_back(const char *path);

int RNA_path_resolve(struct PointerRNA *ptr, const char *path,
	struct PointerRNA *r_ptr, struct PropertyRNA **r_prop);

/* Dependency
 *
 * Experimental code that will generate callbacks for each dependency
 * between ID types. This may end up being useful for UI
 * and evaluation code that needs to know such dependencies for correct
 * redraws and re-evaluations. */

typedef void (*PropDependencyCallback)(void *udata, struct PointerRNA *from, struct PointerRNA *to);
void RNA_test_dependencies_cb(void *udata, struct PointerRNA *from, struct PointerRNA *to);

void RNA_generate_dependencies(struct PointerRNA *mainptr, void *udata, PropDependencyCallback cb);

#endif /* RNA_ACCESS */

