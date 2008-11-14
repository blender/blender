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

static void rna_StructRNA_cname_get(PointerRNA *ptr, char *value)
{
	strcpy(value, ((StructRNA*)ptr->data)->cname);
}

static int rna_StructRNA_cname_length(PointerRNA *ptr)
{
	return strlen(((StructRNA*)ptr->data)->cname);
}

static void rna_StructRNA_name_get(PointerRNA *ptr, char *value)
{
	strcpy(value, ((StructRNA*)ptr->data)->name);
}

static int rna_StructRNA_name_length(PointerRNA *ptr)
{
	return strlen(((StructRNA*)ptr->data)->name);
}

static void *rna_StructRNA_name_property_get(PointerRNA *ptr)
{
	return ((StructRNA*)ptr->data)->nameproperty;
}

static void rna_StructRNA_properties_next(CollectionPropertyIterator *iter)
{
	do {
		rna_iterator_listbase_next(iter);
	} while(iter->valid && (((PropertyRNA*)iter->internal)->flag & PROP_BUILTIN));
}

static void rna_StructRNA_properties_begin(CollectionPropertyIterator *iter, PointerRNA *ptr)
{
	rna_iterator_listbase_begin(iter, &((StructRNA*)ptr->data)->properties);

	if(iter->valid && (((PropertyRNA*)iter->internal)->flag & PROP_BUILTIN))
		rna_StructRNA_properties_next(iter);
}

static void *rna_StructRNA_properties_get(CollectionPropertyIterator *iter)
{
	return rna_iterator_listbase_get(iter);
}

static StructRNA *rna_StructRNA_properties_type(CollectionPropertyIterator *iter)
{
	PropertyRNA *prop= iter->internal;

	switch(prop->type) {
		case PROP_BOOLEAN: return &RNA_BooleanPropertyRNA;
		case PROP_INT: return &RNA_IntPropertyRNA;
		case PROP_FLOAT: return &RNA_FloatPropertyRNA;
		case PROP_STRING: return &RNA_StringPropertyRNA;
		case PROP_ENUM: return &RNA_EnumPropertyRNA;
		case PROP_POINTER: return &RNA_PointerPropertyRNA;
		case PROP_COLLECTION: return &RNA_CollectionPropertyRNA;
		default: return NULL;
	}
}

static void rna_builtin_properties_begin(CollectionPropertyIterator *iter, PointerRNA *ptr)
{
	PointerRNA newptr;

	/* we create a new with the type as the data */
	newptr.type= &RNA_StructRNA;
	newptr.data= ptr->type;

	if(ptr->type->flag & STRUCT_ID) {
		newptr.id.type= ptr->type;
		newptr.id.data= ptr->data;
	}
	else {
		newptr.id.type= NULL;
		newptr.id.data= NULL;
	}

	rna_StructRNA_properties_begin(iter, &newptr);
}

static void rna_builtin_properties_next(CollectionPropertyIterator *iter)
{
	rna_StructRNA_properties_next(iter);
}

static void *rna_builtin_properties_get(CollectionPropertyIterator *iter)
{
	return rna_StructRNA_properties_get(iter);
}

static StructRNA *rna_builtin_properties_type(CollectionPropertyIterator *iter)
{
	return rna_StructRNA_properties_type(iter);
}

static void *rna_builtin_type_get(PointerRNA *ptr)
{
	return ptr->type;
}

/* Property */

static void rna_PropertyRNA_cname_get(PointerRNA *ptr, char *value)
{
	strcpy(value, ((PropertyRNA*)ptr->data)->cname);
}

static int rna_PropertyRNA_cname_length(PointerRNA *ptr)
{
	return strlen(((PropertyRNA*)ptr->data)->cname);
}

static void rna_PropertyRNA_name_get(PointerRNA *ptr, char *value)
{
	strcpy(value, ((PropertyRNA*)ptr->data)->name);
}

static int rna_PropertyRNA_name_length(PointerRNA *ptr)
{
	return strlen(((PropertyRNA*)ptr->data)->name);
}

static void rna_PropertyRNA_description_get(PointerRNA *ptr, char *value)
{
	strcpy(value, ((PropertyRNA*)ptr->data)->description);
}

static int rna_PropertyRNA_description_length(PointerRNA *ptr)
{
	return strlen(((PropertyRNA*)ptr->data)->description);
}

static int rna_PropertyRNA_type_get(PointerRNA *ptr)
{
	return ((PropertyRNA*)ptr->data)->type;
}

static int rna_PropertyRNA_subtype_get(PointerRNA *ptr)
{
	return ((PropertyRNA*)ptr->data)->subtype;
}

static int rna_PropertyRNA_array_length_get(PointerRNA *ptr)
{
	return ((PropertyRNA*)ptr->data)->arraylength;
}

static int rna_PropertyRNA_max_length_get(PointerRNA *ptr)
{
	return ((StringPropertyRNA*)ptr->data)->maxlength;
}

#else

static void rna_def_property(StructRNA *srna)
{
	PropertyRNA *prop;
	static PropertyEnumItem type_items[] = {
		{PROP_BOOLEAN, "BOOLEAN", "Boolean"},
		{PROP_INT, "INT", "Integer"},
		{PROP_FLOAT, "FLOAT", "Float"},
		{PROP_STRING, "STRING", "String"},
		{PROP_ENUM, "ENUM", "Enumeration"},
		{PROP_POINTER, "POINTER", "Pointer"},
		{PROP_COLLECTION, "COLLECTION", "Collection"},
		{0, NULL, NULL}};
	static PropertyEnumItem subtype_items[] = {
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
	RNA_def_property_string_funcs(prop, "rna_PropertyRNA_name_get", "rna_PropertyRNA_name_length", NULL);
	RNA_def_property_ui_text(prop, "Name", "Human readable name.");
	RNA_def_struct_name_property(srna, prop);

	prop= RNA_def_property(srna, "cname", PROP_STRING, PROP_NONE);
	RNA_def_property_flag(prop, PROP_NOT_EDITABLE);
	RNA_def_property_string_funcs(prop, "rna_PropertyRNA_cname_get", "rna_PropertyRNA_cname_length", NULL);
	RNA_def_property_ui_text(prop, "Identifier", "Unique name used in the code and scripting.");

	prop= RNA_def_property(srna, "description", PROP_STRING, PROP_NONE);
	RNA_def_property_flag(prop, PROP_NOT_EDITABLE);
	RNA_def_property_string_funcs(prop, "rna_PropertyRNA_description_get", "rna_PropertyRNA_description_length", NULL);
	RNA_def_property_ui_text(prop, "Description", "Description of the property for tooltips.");

	prop= RNA_def_property(srna, "type", PROP_ENUM, PROP_NONE);
	RNA_def_property_flag(prop, PROP_NOT_EDITABLE);
	RNA_def_property_enum_items(prop, type_items);
	RNA_def_property_enum_funcs(prop, "rna_PropertyRNA_type_get", NULL);
	RNA_def_property_ui_text(prop, "Type", "Data type of the property.");

	prop= RNA_def_property(srna, "subtype", PROP_ENUM, PROP_NONE);
	RNA_def_property_flag(prop, PROP_NOT_EDITABLE);
	RNA_def_property_enum_items(prop, subtype_items);
	RNA_def_property_enum_funcs(prop, "rna_PropertyRNA_subtype_get", NULL);
	RNA_def_property_ui_text(prop, "Sub Type", "Sub type indicating the interpretation of the property.");
}

void RNA_def_rna(BlenderRNA *brna)
{
	StructRNA *srna;
	PropertyRNA *prop;

	/* StructRNA */
	srna= RNA_def_struct(brna, "StructRNA", "Struct RNA");

	prop= RNA_def_property(srna, "name", PROP_STRING, PROP_NONE);
	RNA_def_property_flag(prop, PROP_NOT_EDITABLE);
	RNA_def_property_string_funcs(prop, "rna_StructRNA_name_get", "rna_StructRNA_name_length", NULL);
	RNA_def_property_ui_text(prop, "Name", "Human readable name.");
	RNA_def_struct_name_property(srna, prop);

	prop= RNA_def_property(srna, "cname", PROP_STRING, PROP_NONE);
	RNA_def_property_flag(prop, PROP_NOT_EDITABLE);
	RNA_def_property_string_funcs(prop, "rna_StructRNA_cname_get", "rna_StructRNA_cname_length", NULL);
	RNA_def_property_ui_text(prop, "Identifier", "Unique name used in the code and scripting.");

	prop= RNA_def_property(srna, "name_property", PROP_POINTER, PROP_NONE);
	RNA_def_property_flag(prop, PROP_NOT_EDITABLE);
	RNA_def_property_struct_type(prop, "StringPropertyRNA");
	RNA_def_property_pointer_funcs(prop, "rna_StructRNA_name_property_get", NULL, NULL);
	RNA_def_property_ui_text(prop, "Name Property", "Property that gives the name of the struct.");

	prop= RNA_def_property(srna, "properties", PROP_COLLECTION, PROP_NONE);
	RNA_def_property_flag(prop, PROP_NOT_EDITABLE);
	RNA_def_property_collection_funcs(prop, "rna_StructRNA_properties_begin", "rna_StructRNA_properties_next", 0, "rna_StructRNA_properties_get", "rna_StructRNA_properties_type", 0, 0, 0);
	RNA_def_property_ui_text(prop, "Properties", "Properties in the struct.");

	/* BooleanPropertyRNA */
	srna= RNA_def_struct(brna, "BooleanPropertyRNA", "Boolean Property");
	rna_def_property(srna);

	prop= RNA_def_property(srna, "array_length", PROP_INT, PROP_NONE);
	RNA_def_property_flag(prop, PROP_NOT_EDITABLE);
	RNA_def_property_int_funcs(prop, "rna_PropertyRNA_array_length_get", NULL);
	RNA_def_property_ui_text(prop, "Array Length", "Maximum length of the array, 0 means unlimited.");

	/* IntPropertyRNA */
	srna= RNA_def_struct(brna, "IntPropertyRNA", "Int Property");
	rna_def_property(srna);

	prop= RNA_def_property(srna, "array_length", PROP_INT, PROP_NONE);
	RNA_def_property_flag(prop, PROP_NOT_EDITABLE);
	RNA_def_property_int_funcs(prop, "rna_PropertyRNA_array_length_get", NULL);
	RNA_def_property_ui_text(prop, "Array Length", "Maximum length of the array, 0 means unlimited.");

	/* FloatPropertyRNA */
	srna= RNA_def_struct(brna, "FloatPropertyRNA", "Float Property");
	rna_def_property(srna);

	prop= RNA_def_property(srna, "array_length", PROP_INT, PROP_NONE);
	RNA_def_property_flag(prop, PROP_NOT_EDITABLE);
	RNA_def_property_int_funcs(prop, "rna_PropertyRNA_array_length_get", NULL);
	RNA_def_property_ui_text(prop, "Array Length", "Maximum length of the array, 0 means unlimited.");

	/* StringPropertyRNA */
	srna= RNA_def_struct(brna, "StringPropertyRNA", "String Property");
	rna_def_property(srna);

	prop= RNA_def_property(srna, "max_length", PROP_INT, PROP_NONE);
	RNA_def_property_flag(prop, PROP_NOT_EDITABLE);
	RNA_def_property_int_funcs(prop, "rna_PropertyRNA_max_length_get", NULL);
	RNA_def_property_ui_text(prop, "Maximum Length", "Maximum length of the string, 0 means unlimited.");

	/* EnumPropertyRNA */
	srna= RNA_def_struct(brna, "EnumPropertyRNA", "Enum Property");
	rna_def_property(srna);

	/* PointerPropertyRNA */
	srna= RNA_def_struct(brna, "PointerPropertyRNA", "Pointer Property");
	rna_def_property(srna);

	/* CollectionPropertyRNA */
	srna= RNA_def_struct(brna, "CollectionPropertyRNA", "Collection Property");
	rna_def_property(srna);
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
	RNA_def_property_struct_type(prop, "StructRNA");
	RNA_def_property_pointer_funcs(prop, "rna_builtin_type_get", NULL, NULL);
	RNA_def_property_ui_text(prop, "Type", "RNA type definition.");
}

#endif

