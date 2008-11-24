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
struct ID;
struct Main;

/* Types */

extern BlenderRNA BLENDER_RNA;

extern StructRNA RNA_ID;
extern StructRNA RNA_IDProperty;
extern StructRNA RNA_IDPropertyGroup;
extern StructRNA RNA_Main;
extern StructRNA RNA_Mesh;
extern StructRNA RNA_MVert;
extern StructRNA RNA_MVertGroup;
extern StructRNA RNA_MEdge;
extern StructRNA RNA_MFace;
extern StructRNA RNA_MTFace;
extern StructRNA RNA_MTFaceLayer;
extern StructRNA RNA_MSticky;
extern StructRNA RNA_MCol;
extern StructRNA RNA_MColLayer;
extern StructRNA RNA_MFloatProperty;
extern StructRNA RNA_MFloatPropertyLayer;
extern StructRNA RNA_MIntProperty;
extern StructRNA RNA_MIntPropertyLayer;
extern StructRNA RNA_MStringProperty;
extern StructRNA RNA_MStringPropertyLayer;
extern StructRNA RNA_MMultires;
extern StructRNA RNA_Object;
extern StructRNA RNA_Struct;
extern StructRNA RNA_Property;
extern StructRNA RNA_BooleanProperty;
extern StructRNA RNA_IntProperty;
extern StructRNA RNA_FloatProperty;
extern StructRNA RNA_StringProperty;
extern StructRNA RNA_EnumProperty;
extern StructRNA RNA_EnumPropertyItem;
extern StructRNA RNA_PointerProperty;
extern StructRNA RNA_CollectionProperty;
extern StructRNA RNA_Scene;
extern StructRNA RNA_Lamp;
extern StructRNA RNA_Operator;
extern StructRNA RNA_WindowManager;

/* Pointer
 *
 * These functions will fill in RNA pointers, this can be done in three ways:
 * - a pointer Main is created by just passing the data pointer
 * - a pointer to a datablock can be created with the type and id data pointer
 * - a pointer to data contained in a datablock can be created with the id type
 *   and id data pointer, and the data type and pointer to the struct itself.
 */

void RNA_main_pointer_create(struct Main *main, PointerRNA *r_ptr);
void RNA_id_pointer_create(StructRNA *idtype, struct ID *id, PointerRNA *r_ptr);
void RNA_pointer_create(StructRNA *idtype, struct ID *id, StructRNA *type, void *data, PointerRNA *r_ptr);

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

const char *RNA_property_identifier(PointerRNA *ptr, PropertyRNA *prop);
PropertyType RNA_property_type(PointerRNA *ptr, PropertyRNA *prop);
PropertySubType RNA_property_subtype(PointerRNA *ptr, PropertyRNA *prop);

int RNA_property_array_length(PointerRNA *ptr, PropertyRNA *prop);

void RNA_property_int_range(PointerRNA *ptr, PropertyRNA *prop, int *hardmin, int *hardmax);
void RNA_property_int_ui_range(PointerRNA *ptr, PropertyRNA *prop, int *softmin, int *softmax, int *step);

void RNA_property_float_range(PointerRNA *ptr, PropertyRNA *prop, float *hardmin, float *hardmax);
void RNA_property_float_ui_range(PointerRNA *ptr, PropertyRNA *prop, float *softmin, float *softmax, float *step, float *precision);

int RNA_property_string_maxlength(PointerRNA *ptr, PropertyRNA *prop);

void RNA_property_enum_items(PointerRNA *ptr, PropertyRNA *prop, const EnumPropertyItem **item, int *totitem);

const char *RNA_property_ui_name(PointerRNA *ptr, PropertyRNA *prop);
const char *RNA_property_ui_description(PointerRNA *ptr, PropertyRNA *prop);

int RNA_property_editable(PointerRNA *ptr, PropertyRNA *prop);
int RNA_property_evaluated(PointerRNA *ptr, PropertyRNA *prop);

void RNA_property_notify(PropertyRNA *prop, struct bContext *C, PointerRNA *ptr);

/* Property Data */

int RNA_property_boolean_get(PointerRNA *ptr, PropertyRNA *prop);
void RNA_property_boolean_set(PointerRNA *ptr, PropertyRNA *prop, int value);
int RNA_property_boolean_get_array(PointerRNA *ptr, PropertyRNA *prop, int index);
void RNA_property_boolean_set_array(PointerRNA *ptr, PropertyRNA *prop, int index, int value);

int RNA_property_int_get(PointerRNA *ptr, PropertyRNA *prop);
void RNA_property_int_set(PointerRNA *ptr, PropertyRNA *prop, int value);
int RNA_property_int_get_array(PointerRNA *ptr, PropertyRNA *prop, int index);
void RNA_property_int_set_array(PointerRNA *ptr, PropertyRNA *prop, int index, int value);

float RNA_property_float_get(PointerRNA *ptr, PropertyRNA *prop);
void RNA_property_float_set(PointerRNA *ptr, PropertyRNA *prop, float value);
float RNA_property_float_get_array(PointerRNA *ptr, PropertyRNA *prop, int index);
void RNA_property_float_set_array(PointerRNA *ptr, PropertyRNA *prop, int index, float value);

void RNA_property_string_get(PointerRNA *ptr, PropertyRNA *prop, char *value);
char *RNA_property_string_get_alloc(PointerRNA *ptr, PropertyRNA *prop, char *fixedbuf, int fixedlen);
int RNA_property_string_length(PointerRNA *ptr, PropertyRNA *prop);
void RNA_property_string_set(PointerRNA *ptr, PropertyRNA *prop, const char *value);

int RNA_property_enum_get(PointerRNA *ptr, PropertyRNA *prop);
void RNA_property_enum_set(PointerRNA *ptr, PropertyRNA *prop, int value);

void RNA_property_pointer_get(PointerRNA *ptr, PropertyRNA *prop, PointerRNA *r_ptr);
void RNA_property_pointer_set(PointerRNA *ptr, PropertyRNA *prop, PointerRNA *ptr_value);

void RNA_property_collection_begin(PointerRNA *ptr, PropertyRNA *prop, CollectionPropertyIterator *iter);
void RNA_property_collection_next(CollectionPropertyIterator *iter);
void RNA_property_collection_end(CollectionPropertyIterator *iter);
int RNA_property_collection_length(PointerRNA *ptr, PropertyRNA *prop);
int RNA_property_collection_lookup_int(PointerRNA *ptr, PropertyRNA *prop, int key, PointerRNA *r_ptr);
int RNA_property_collection_lookup_string(PointerRNA *ptr, PropertyRNA *prop, const char *key, PointerRNA *r_ptr);

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
 * There is no support for pointers and collections here yet, these can be 
 * added when ID properties support them. */

int RNA_boolean_get(PointerRNA *ptr, const char *name);
void RNA_boolean_set(PointerRNA *ptr, const char *name, int value);
void RNA_boolean_get_array(PointerRNA *ptr, const char *name, int *values);
void RNA_boolean_set_array(PointerRNA *ptr, const char *name, const int *values);

int RNA_int_get(PointerRNA *ptr, const char *name);
void RNA_int_set(PointerRNA *ptr, const char *name, int value);
void RNA_int_get_array(PointerRNA *ptr, const char *name, int *values);
void RNA_int_set_array(PointerRNA *ptr, const char *name, const int *values);

float RNA_float_get(PointerRNA *ptr, const char *name);
void RNA_float_set(PointerRNA *ptr, const char *name, float value);
void RNA_float_get_array(PointerRNA *ptr, const char *name, float *values);
void RNA_float_set_array(PointerRNA *ptr, const char *name, const float *values);

int RNA_enum_get(PointerRNA *ptr, const char *name);
void RNA_enum_set(PointerRNA *ptr, const char *name, int value);

void RNA_string_get(PointerRNA *ptr, const char *name, char *value);
char *RNA_string_get_alloc(PointerRNA *ptr, const char *name, char *fixedbuf, int fixedlen);
int RNA_string_length(PointerRNA *ptr, const char *name);
void RNA_string_set(PointerRNA *ptr, const char *name, const char *value);

/* check if the idproperty exists, for operators */
int RNA_property_is_set(PointerRNA *ptr, const char *name);

#endif /* RNA_ACCESS */

