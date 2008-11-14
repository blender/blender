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

#include <stdlib.h>

#include "RNA_access.h"
#include "RNA_define.h"
#include "RNA_types.h"

#ifdef RNA_RUNTIME

/* Struct */

static void rna_Struct_identifier_get(PointerRNA *ptr, char *value)
{
	strcpy(value, ((StructRNA*)ptr->data)->identifier);
}

static int rna_Struct_identifier_length(PointerRNA *ptr)
{
	return strlen(((StructRNA*)ptr->data)->identifier);
}

static void rna_Struct_name_get(PointerRNA *ptr, char *value)
{
	strcpy(value, ((StructRNA*)ptr->data)->name);
}

static int rna_Struct_name_length(PointerRNA *ptr)
{
	return strlen(((StructRNA*)ptr->data)->name);
}

static void *rna_Struct_name_property_get(PointerRNA *ptr)
{
	return ((StructRNA*)ptr->data)->nameproperty;
}

static void rna_Struct_properties_next(CollectionPropertyIterator *iter)
{
	do {
		rna_iterator_listbase_next(iter);
	} while(iter->valid && (((PropertyRNA*)iter->internal)->flag & PROP_BUILTIN));
}

static void rna_Struct_properties_begin(CollectionPropertyIterator *iter, PointerRNA *ptr)
{
	rna_iterator_listbase_begin(iter, &((StructRNA*)ptr->data)->properties);

	if(iter->valid && (((PropertyRNA*)iter->internal)->flag & PROP_BUILTIN))
		rna_Struct_properties_next(iter);
}

static void *rna_Struct_properties_get(CollectionPropertyIterator *iter)
{
	return rna_iterator_listbase_get(iter);
}

static StructRNA *rna_Struct_properties_type(CollectionPropertyIterator *iter)
{
	PropertyRNA *prop= iter->internal;

	switch(prop->type) {
		case PROP_BOOLEAN: return &RNA_BooleanProperty;
		case PROP_INT: return &RNA_IntProperty;
		case PROP_FLOAT: return &RNA_FloatProperty;
		case PROP_STRING: return &RNA_StringProperty;
		case PROP_ENUM: return &RNA_EnumProperty;
		case PROP_POINTER: return &RNA_PointerProperty;
		case PROP_COLLECTION: return &RNA_CollectionProperty;
		default: return NULL;
	}
}

static void rna_builtin_properties_begin(CollectionPropertyIterator *iter, PointerRNA *ptr)
{
	PointerRNA newptr;

	/* we create a new with the type as the data */
	newptr.type= &RNA_Struct;
	newptr.data= ptr->type;

	if(ptr->type->flag & STRUCT_ID) {
		newptr.id.type= ptr->type;
		newptr.id.data= ptr->data;
	}
	else {
		newptr.id.type= NULL;
		newptr.id.data= NULL;
	}

	rna_Struct_properties_begin(iter, &newptr);
}

static void rna_builtin_properties_next(CollectionPropertyIterator *iter)
{
	rna_Struct_properties_next(iter);
}

static void *rna_builtin_properties_get(CollectionPropertyIterator *iter)
{
	return rna_Struct_properties_get(iter);
}

static StructRNA *rna_builtin_properties_type(CollectionPropertyIterator *iter)
{
	return rna_Struct_properties_type(iter);
}

static void *rna_builtin_type_get(PointerRNA *ptr)
{
	return ptr->type;
}

/* Property */

static void rna_Property_identifier_get(PointerRNA *ptr, char *value)
{
	strcpy(value, ((PropertyRNA*)ptr->data)->identifier);
}

static int rna_Property_identifier_length(PointerRNA *ptr)
{
	return strlen(((PropertyRNA*)ptr->data)->identifier);
}

static void rna_Property_name_get(PointerRNA *ptr, char *value)
{
	strcpy(value, ((PropertyRNA*)ptr->data)->name);
}

static int rna_Property_name_length(PointerRNA *ptr)
{
	return strlen(((PropertyRNA*)ptr->data)->name);
}

static void rna_Property_description_get(PointerRNA *ptr, char *value)
{
	strcpy(value, ((PropertyRNA*)ptr->data)->description);
}

static int rna_Property_description_length(PointerRNA *ptr)
{
	return strlen(((PropertyRNA*)ptr->data)->description);
}

static int rna_Property_type_get(PointerRNA *ptr)
{
	return ((PropertyRNA*)ptr->data)->type;
}

static int rna_Property_subtype_get(PointerRNA *ptr)
{
	return ((PropertyRNA*)ptr->data)->subtype;
}

static int rna_Property_array_length_get(PointerRNA *ptr)
{
	return ((PropertyRNA*)ptr->data)->arraylength;
}

static int rna_IntProperty_hard_min_get(PointerRNA *ptr)
{
	return ((IntPropertyRNA*)ptr->data)->hardmin;
}

static int rna_IntProperty_hard_max_get(PointerRNA *ptr)
{
	return ((IntPropertyRNA*)ptr->data)->hardmax;
}

static int rna_IntProperty_soft_min_get(PointerRNA *ptr)
{
	return ((IntPropertyRNA*)ptr->data)->softmin;
}

static int rna_IntProperty_soft_max_get(PointerRNA *ptr)
{
	return ((IntPropertyRNA*)ptr->data)->softmax;
}

static int rna_IntProperty_step_get(PointerRNA *ptr)
{
	return ((IntPropertyRNA*)ptr->data)->step;
}

static float rna_FloatProperty_hard_min_get(PointerRNA *ptr)
{
	return ((FloatPropertyRNA*)ptr->data)->hardmin;
}

static float rna_FloatProperty_hard_max_get(PointerRNA *ptr)
{
	return ((FloatPropertyRNA*)ptr->data)->hardmax;
}

static float rna_FloatProperty_soft_min_get(PointerRNA *ptr)
{
	return ((FloatPropertyRNA*)ptr->data)->softmin;
}

static float rna_FloatProperty_soft_max_get(PointerRNA *ptr)
{
	return ((FloatPropertyRNA*)ptr->data)->softmax;
}

static float rna_FloatProperty_step_get(PointerRNA *ptr)
{
	return ((FloatPropertyRNA*)ptr->data)->step;
}

static int rna_FloatProperty_precision_get(PointerRNA *ptr)
{
	return ((FloatPropertyRNA*)ptr->data)->precision;
}

static int rna_StringProperty_max_length_get(PointerRNA *ptr)
{
	return ((StringPropertyRNA*)ptr->data)->maxlength;
}

static void rna_EnumProperty_items_begin(CollectionPropertyIterator *iter, PointerRNA *ptr)
{
	EnumPropertyRNA *eprop= (EnumPropertyRNA*)ptr->data;
	rna_iterator_array_begin(iter, (void*)eprop->item, sizeof(eprop->item[0]), eprop->totitem);
}

static void rna_EnumPropertyItem_identifier_get(PointerRNA *ptr, char *value)
{
	strcpy(value, ((EnumPropertyItem*)ptr->data)->identifier);
}

static int rna_EnumPropertyItem_identifier_length(PointerRNA *ptr)
{
	return strlen(((EnumPropertyItem*)ptr->data)->identifier);
}

static void rna_EnumPropertyItem_name_get(PointerRNA *ptr, char *value)
{
	strcpy(value, ((EnumPropertyItem*)ptr->data)->name);
}

static int rna_EnumPropertyItem_name_length(PointerRNA *ptr)
{
	return strlen(((EnumPropertyItem*)ptr->data)->name);
}

static int rna_EnumPropertyItem_value_get(PointerRNA *ptr)
{
	return ((EnumPropertyItem*)ptr->data)->value;
}

static void *rna_PointerProperty_fixed_type_get(PointerRNA *ptr)
{
	return ((PointerPropertyRNA*)ptr->data)->structtype;
}

static void *rna_CollectionProperty_fixed_type_get(PointerRNA *ptr)
{
	return ((CollectionPropertyRNA*)ptr->data)->structtype;
}

#else

static void rna_def_property(StructRNA *srna)
{
	PropertyRNA *prop;
	static EnumPropertyItem type_items[] = {
		{PROP_BOOLEAN, "BOOLEAN", "Boolean"},
		{PROP_INT, "INT", "Integer"},
		{PROP_FLOAT, "FLOAT", "Float"},
		{PROP_STRING, "STRING", "String"},
		{PROP_ENUM, "ENUM", "Enumeration"},
		{PROP_POINTER, "POINTER", "Pointer"},
		{PROP_COLLECTION, "COLLECTION", "Collection"},
		{0, NULL, NULL}};
	static EnumPropertyItem subtype_items[] = {
		{PROP_NONE, "NONE", "None"},
		{PROP_UNSIGNED, "UNSIGNED", "Unsigned Number"},
		{PROP_FILEPATH, "FILEPATH", "File Path"},
		{PROP_COLOR, "COLOR", "Color"},
		{PROP_VECTOR, "VECTOR", "Vector"},
		{PROP_MATRIX, "MATRIX", "Matrix"},
		{PROP_ROTATION, "ROTATION", "Rotation"},
		{0, NULL, NULL}};

	prop= RNA_def_property(srna, "name", PROP_STRING, PROP_NONE);
	RNA_def_property_flag(prop, PROP_NOT_EDITABLE);
	RNA_def_property_string_funcs(prop, "rna_Property_name_get", "rna_Property_name_length", NULL);
	RNA_def_property_ui_text(prop, "Name", "Human readable name.");
	RNA_def_struct_name_property(srna, prop);

	prop= RNA_def_property(srna, "identifier", PROP_STRING, PROP_NONE);
	RNA_def_property_flag(prop, PROP_NOT_EDITABLE);
	RNA_def_property_string_funcs(prop, "rna_Property_identifier_get", "rna_Property_identifier_length", NULL);
	RNA_def_property_ui_text(prop, "Identifier", "Unique name used in the code and scripting.");

	prop= RNA_def_property(srna, "description", PROP_STRING, PROP_NONE);
	RNA_def_property_flag(prop, PROP_NOT_EDITABLE);
	RNA_def_property_string_funcs(prop, "rna_Property_description_get", "rna_Property_description_length", NULL);
	RNA_def_property_ui_text(prop, "Description", "Description of the property for tooltips.");

	prop= RNA_def_property(srna, "type", PROP_ENUM, PROP_NONE);
	RNA_def_property_flag(prop, PROP_NOT_EDITABLE);
	RNA_def_property_enum_items(prop, type_items);
	RNA_def_property_enum_funcs(prop, "rna_Property_type_get", NULL);
	RNA_def_property_ui_text(prop, "Type", "Data type of the property.");

	prop= RNA_def_property(srna, "subtype", PROP_ENUM, PROP_NONE);
	RNA_def_property_flag(prop, PROP_NOT_EDITABLE);
	RNA_def_property_enum_items(prop, subtype_items);
	RNA_def_property_enum_funcs(prop, "rna_Property_subtype_get", NULL);
	RNA_def_property_ui_text(prop, "Sub Type", "Sub type indicating the interpretation of the property.");
}

static void rna_def_number_property(StructRNA *srna, PropertyType type)
{
	PropertyRNA *prop;

	prop= RNA_def_property(srna, "array_length", PROP_INT, PROP_NONE);
	RNA_def_property_flag(prop, PROP_NOT_EDITABLE);
	RNA_def_property_int_funcs(prop, "rna_Property_array_length_get", NULL);
	RNA_def_property_ui_text(prop, "Array Length", "Maximum length of the array, 0 means unlimited.");

	if(type == PROP_BOOLEAN)
		return;

	prop= RNA_def_property(srna, "hard_min", type, PROP_NONE);
	RNA_def_property_flag(prop, PROP_NOT_EDITABLE);
	if(type == PROP_INT) RNA_def_property_int_funcs(prop, "rna_IntProperty_hard_min_get", NULL);
	else RNA_def_property_float_funcs(prop, "rna_FloatProperty_hard_min_get", NULL);
	RNA_def_property_ui_text(prop, "Hard Minimum", "Minimum value used by buttons.");

	prop= RNA_def_property(srna, "hard_max", type, PROP_NONE);
	RNA_def_property_flag(prop, PROP_NOT_EDITABLE);
	if(type == PROP_INT) RNA_def_property_int_funcs(prop, "rna_IntProperty_hard_max_get", NULL);
	else RNA_def_property_float_funcs(prop, "rna_FloatProperty_hard_max_get", NULL);
	RNA_def_property_ui_text(prop, "Hard Maximum", "Maximum value used by buttons.");

	prop= RNA_def_property(srna, "soft_min", type, PROP_NONE);
	RNA_def_property_flag(prop, PROP_NOT_EDITABLE);
	if(type == PROP_INT) RNA_def_property_int_funcs(prop, "rna_IntProperty_soft_min_get", NULL);
	else RNA_def_property_float_funcs(prop, "rna_FloatProperty_soft_min_get", NULL);
	RNA_def_property_ui_text(prop, "Soft Minimum", "Minimum value used by buttons.");

	prop= RNA_def_property(srna, "soft_max", type, PROP_NONE);
	RNA_def_property_flag(prop, PROP_NOT_EDITABLE);
	if(type == PROP_INT) RNA_def_property_int_funcs(prop, "rna_IntProperty_soft_max_get", NULL);
	else RNA_def_property_float_funcs(prop, "rna_FloatProperty_soft_max_get", NULL);
	RNA_def_property_ui_text(prop, "Soft Maximum", "Maximum value used by buttons.");

	prop= RNA_def_property(srna, "step", type, PROP_NONE);
	RNA_def_property_flag(prop, PROP_NOT_EDITABLE);
	if(type == PROP_INT) RNA_def_property_int_funcs(prop, "rna_IntProperty_step_get", NULL);
	else RNA_def_property_float_funcs(prop, "rna_FloatProperty_step_get", NULL);
	RNA_def_property_ui_text(prop, "Step", "Step size used by number buttons, for floats 1/100th of the step size.");

	if(type == PROP_FLOAT) {
		prop= RNA_def_property(srna, "precision", PROP_INT, PROP_NONE);
		RNA_def_property_flag(prop, PROP_NOT_EDITABLE);
		RNA_def_property_int_funcs(prop, "rna_FloatProperty_precision_get", NULL);
		RNA_def_property_ui_text(prop, "Precision", "Number of digits after the dot used by buttons.");
	}
}

static void rna_def_enum_property(BlenderRNA *brna, StructRNA *srna)
{
	PropertyRNA *prop;

	prop= RNA_def_property(srna, "items", PROP_COLLECTION, PROP_NONE);
	RNA_def_property_flag(prop, PROP_NOT_EDITABLE);
	RNA_def_property_struct_type(prop, "EnumPropertyItem");
	RNA_def_property_collection_funcs(prop, "rna_EnumProperty_items_begin", "rna_iterator_array_next", "rna_iterator_array_end", "rna_iterator_array_get", 0, 0, 0, 0);
	RNA_def_property_ui_text(prop, "Items", "Possible values for the property.");

	srna= RNA_def_struct(brna, "EnumPropertyItem", "Enum Item Definition");

	prop= RNA_def_property(srna, "name", PROP_STRING, PROP_NONE);
	RNA_def_property_flag(prop, PROP_NOT_EDITABLE);
	RNA_def_property_string_funcs(prop, "rna_EnumPropertyItem_name_get", "rna_EnumPropertyItem_name_length", NULL);
	RNA_def_property_ui_text(prop, "Name", "Human readable name.");
	RNA_def_struct_name_property(srna, prop);

	prop= RNA_def_property(srna, "identifier", PROP_STRING, PROP_NONE);
	RNA_def_property_flag(prop, PROP_NOT_EDITABLE);
	RNA_def_property_string_funcs(prop, "rna_EnumPropertyItem_identifier_get", "rna_EnumPropertyItem_identifier_length", NULL);
	RNA_def_property_ui_text(prop, "Identifier", "Unique name used in the code and scripting.");

	prop= RNA_def_property(srna, "value", PROP_INT, PROP_NONE);
	RNA_def_property_flag(prop, PROP_NOT_EDITABLE);
	RNA_def_property_int_funcs(prop, "rna_EnumPropertyItem_value_get", NULL);
	RNA_def_property_ui_text(prop, "Value", "Value of the item.");
}

static void rna_def_pointer_property(StructRNA *srna, PropertyType type)
{
	PropertyRNA *prop;

	prop= RNA_def_property(srna, "fixed_type", PROP_POINTER, PROP_NONE);
	RNA_def_property_flag(prop, PROP_NOT_EDITABLE);
	RNA_def_property_struct_type(prop, "Struct");
	if(type == PROP_POINTER)
		RNA_def_property_pointer_funcs(prop, "rna_PointerProperty_fixed_type_get", NULL, NULL);
	else
		RNA_def_property_pointer_funcs(prop, "rna_CollectionProperty_fixed_type_get", NULL, NULL);
	RNA_def_property_ui_text(prop, "Pointer Type", "Fixed pointer type, empty if variable type.");
}

void RNA_def_rna(BlenderRNA *brna)
{
	StructRNA *srna;
	PropertyRNA *prop;

	/* StructRNA */
	srna= RNA_def_struct(brna, "Struct", "Struct Definition");

	prop= RNA_def_property(srna, "name", PROP_STRING, PROP_NONE);
	RNA_def_property_flag(prop, PROP_NOT_EDITABLE);
	RNA_def_property_string_funcs(prop, "rna_Struct_name_get", "rna_Struct_name_length", NULL);
	RNA_def_property_ui_text(prop, "Name", "Human readable name.");
	RNA_def_struct_name_property(srna, prop);

	prop= RNA_def_property(srna, "identifier", PROP_STRING, PROP_NONE);
	RNA_def_property_flag(prop, PROP_NOT_EDITABLE);
	RNA_def_property_string_funcs(prop, "rna_Struct_identifier_get", "rna_Struct_identifier_length", NULL);
	RNA_def_property_ui_text(prop, "Identifier", "Unique name used in the code and scripting.");

	prop= RNA_def_property(srna, "name_property", PROP_POINTER, PROP_NONE);
	RNA_def_property_flag(prop, PROP_NOT_EDITABLE);
	RNA_def_property_struct_type(prop, "StringProperty");
	RNA_def_property_pointer_funcs(prop, "rna_Struct_name_property_get", NULL, NULL);
	RNA_def_property_ui_text(prop, "Name Property", "Property that gives the name of the struct.");

	prop= RNA_def_property(srna, "properties", PROP_COLLECTION, PROP_NONE);
	RNA_def_property_flag(prop, PROP_NOT_EDITABLE);
	RNA_def_property_collection_funcs(prop, "rna_Struct_properties_begin", "rna_Struct_properties_next", 0, "rna_Struct_properties_get", "rna_Struct_properties_type", 0, 0, 0);
	RNA_def_property_ui_text(prop, "Properties", "Properties in the struct.");

	/* BooleanProperty */
	srna= RNA_def_struct(brna, "BooleanProperty", "Boolean Definition");
	rna_def_property(srna);
	rna_def_number_property(srna, PROP_BOOLEAN);

	/* IntProperty */
	srna= RNA_def_struct(brna, "IntProperty", "Int Definition");
	rna_def_property(srna);
	rna_def_number_property(srna, PROP_INT);

	/* FloatProperty */
	srna= RNA_def_struct(brna, "FloatProperty", "Float Definition");
	rna_def_property(srna);
	rna_def_number_property(srna, PROP_FLOAT);

	/* StringProperty */
	srna= RNA_def_struct(brna, "StringProperty", "String Definition");
	rna_def_property(srna);

	prop= RNA_def_property(srna, "max_length", PROP_INT, PROP_NONE);
	RNA_def_property_flag(prop, PROP_NOT_EDITABLE);
	RNA_def_property_int_funcs(prop, "rna_StringProperty_max_length_get", NULL);
	RNA_def_property_ui_text(prop, "Maximum Length", "Maximum length of the string, 0 means unlimited.");

	/* EnumProperty */
	srna= RNA_def_struct(brna, "EnumProperty", "Enum Definition");
	rna_def_property(srna);
	rna_def_enum_property(brna, srna);

	/* PointerProperty */
	srna= RNA_def_struct(brna, "PointerProperty", "Pointer Definition");
	rna_def_property(srna);
	rna_def_pointer_property(srna, PROP_POINTER);

	/* CollectionProperty */
	srna= RNA_def_struct(brna, "CollectionProperty", "Collection Definition");
	rna_def_property(srna);
	rna_def_pointer_property(srna, PROP_COLLECTION);
}

void rna_def_builtin_properties(StructRNA *srna)
{
	PropertyRNA *prop;

	prop= RNA_def_property(srna, "rna_properties", PROP_COLLECTION, PROP_NONE);
	RNA_def_property_flag(prop, PROP_NOT_EDITABLE|PROP_BUILTIN);
	RNA_def_property_collection_funcs(prop, "rna_builtin_properties_begin", "rna_builtin_properties_next", 0, "rna_builtin_properties_get", "rna_builtin_properties_type", 0, 0, 0);
	RNA_def_property_ui_text(prop, "Properties", "RNA property collection.");

	prop= RNA_def_property(srna, "rna_type", PROP_POINTER, PROP_NONE);
	RNA_def_property_flag(prop, PROP_NOT_EDITABLE);
	RNA_def_property_struct_type(prop, "Struct");
	RNA_def_property_pointer_funcs(prop, "rna_builtin_type_get", NULL, NULL);
	RNA_def_property_ui_text(prop, "Type", "RNA type definition.");
}

#endif

