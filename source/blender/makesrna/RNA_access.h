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
struct PropertyRNA;
struct StructRNA;
struct CollectionPropertyIterator;

/* Property */

void RNA_property_notify(struct PropertyRNA *prop, struct bContext *C, void *data);
int RNA_property_readonly(struct PropertyRNA *prop, struct bContext *C, void *data);

int RNA_property_boolean_get(struct PropertyRNA *prop, void *data);
void RNA_property_boolean_set(struct PropertyRNA *prop, void *data, int value);
int RNA_property_boolean_get_array(struct PropertyRNA *prop, void *data, int index);
void RNA_property_boolean_set_array(struct PropertyRNA *prop, void *data, int index, int value);

int RNA_property_int_get(struct PropertyRNA *prop, void *data);
void RNA_property_int_set(struct PropertyRNA *prop, void *data, int value);
int RNA_property_int_get_array(struct PropertyRNA *prop, void *data, int index);
void RNA_property_int_set_array(struct PropertyRNA *prop, void *data, int index, int value);

float RNA_property_float_get(struct PropertyRNA *prop, void *data);
void RNA_property_float_set(struct PropertyRNA *prop, void *data, float value);
float RNA_property_float_get_array(struct PropertyRNA *prop, void *data, int index);
void RNA_property_float_set_array(struct PropertyRNA *prop, void *data, int index, float value);

void RNA_property_string_get(struct PropertyRNA *prop, void *data, char *value);
int RNA_property_string_length(struct PropertyRNA *prop, void *data);
void RNA_property_string_set(struct PropertyRNA *prop, void *data, const char *value);

int RNA_property_enum_get(struct PropertyRNA *prop, void *data);
void RNA_property_enum_set(struct PropertyRNA *prop, void *data, int value);

void *RNA_property_pointer_get(struct PropertyRNA *prop, void *data);
void RNA_property_pointer_set(struct PropertyRNA *prop, void *data, void *value);
struct StructRNA *RNA_property_pointer_type(struct PropertyRNA *prop, void *data);

void RNA_property_collection_begin(struct PropertyRNA *prop, struct CollectionPropertyIterator *iter, void *data);
void RNA_property_collection_next(struct PropertyRNA *prop, struct CollectionPropertyIterator *iter);
void RNA_property_collection_end(struct PropertyRNA *prop, struct CollectionPropertyIterator *iter);
void *RNA_property_collection_get(struct PropertyRNA *prop, struct CollectionPropertyIterator *iter);
struct StructRNA *RNA_property_collection_type(struct PropertyRNA *prop, struct CollectionPropertyIterator *iter);
int RNA_property_collection_length(struct PropertyRNA *prop, void *data);
void *RNA_property_collection_lookup_int(struct PropertyRNA *prop, void *data, int key, struct StructRNA **type);
void *RNA_property_collection_lookup_string(struct PropertyRNA *prop, void *data, const char *key, struct StructRNA **type);

#endif /* RNA_ACCESS */

