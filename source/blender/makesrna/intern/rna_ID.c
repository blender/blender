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
#include <stdio.h>

#include "RNA_define.h"
#include "RNA_types.h"

#include "DNA_ID.h"

#ifdef RNA_RUNTIME

#include "BKE_idprop.h"

/* name functions that ignore the first two ID characters */
static void rna_ID_name_get(PointerRNA *ptr, char *value)
{
	ID *id= (ID*)ptr->data;
	BLI_strncpy(value, id->name+2, sizeof(id->name)-2);
}

static int rna_ID_name_length(PointerRNA *ptr)
{
	ID *id= (ID*)ptr->data;
	return strlen(id->name+2);
}

static void rna_ID_name_set(PointerRNA *ptr, const char *value)
{
	ID *id= (ID*)ptr->data;
	BLI_strncpy(id->name+2, value, sizeof(id->name)-2);
}

/* ID properties */

static void rna_IDProperty_string_get(PointerRNA *ptr, char *value)
{
	IDProperty *prop= (IDProperty*)ptr->data;
	strcpy(value, IDP_String(prop));
}

static int rna_IDProperty_string_length(PointerRNA *ptr)
{
	IDProperty *prop= (IDProperty*)ptr->data;
	return strlen(IDP_String(prop));
}

static void rna_IDProperty_string_set(PointerRNA *ptr, const char *value)
{
	IDProperty *prop= (IDProperty*)ptr->data;
	IDP_AssignString(prop, (char*)value);
}

static int rna_IDProperty_int_get(PointerRNA *ptr)
{
	IDProperty *prop= (IDProperty*)ptr->data;
	return IDP_Int(prop);
}

static void rna_IDProperty_int_set(PointerRNA *ptr, int value)
{
	IDProperty *prop= (IDProperty*)ptr->data;
	IDP_Int(prop)= value;
}

static int rna_IDProperty_intarray_get(PointerRNA *ptr, int index)
{
	IDProperty *prop= (IDProperty*)ptr->data;
	return ((int*)IDP_Array(prop))[index];
}

static void rna_IDProperty_intarray_set(PointerRNA *ptr, int index, int value)
{
	IDProperty *prop= (IDProperty*)ptr->data;
	((int*)IDP_Array(prop))[index]= value;
}

static float rna_IDProperty_float_get(PointerRNA *ptr)
{
	IDProperty *prop= (IDProperty*)ptr->data;
	return IDP_Float(prop);
}

static void rna_IDProperty_float_set(PointerRNA *ptr, float value)
{
	IDProperty *prop= (IDProperty*)ptr->data;
	IDP_Float(prop)= value;
}

static float rna_IDProperty_floatarray_get(PointerRNA *ptr, int index)
{
	IDProperty *prop= (IDProperty*)ptr->data;
	return ((float*)IDP_Array(prop))[index];
}

static void rna_IDProperty_floatarray_set(PointerRNA *ptr, int index, float value)
{
	IDProperty *prop= (IDProperty*)ptr->data;
	((float*)IDP_Array(prop))[index]= value;
}

static float rna_IDProperty_double_get(PointerRNA *ptr)
{
	IDProperty *prop= (IDProperty*)ptr->data;
	return (float)IDP_Double(prop);
}

static void rna_IDProperty_double_set(PointerRNA *ptr, float value)
{
	IDProperty *prop= (IDProperty*)ptr->data;
	IDP_Double(prop)= value;
}

static float rna_IDProperty_doublearray_get(PointerRNA *ptr, int index)
{
	IDProperty *prop= (IDProperty*)ptr->data;
	return (float)(((double*)IDP_Array(prop))[index]);
}

static void rna_IDProperty_doublearray_set(PointerRNA *ptr, int index, float value)
{
	IDProperty *prop= (IDProperty*)ptr->data;
	((double*)IDP_Array(prop))[index]= value;
}

static void* rna_IDProperty_group_get(PointerRNA *ptr)
{
	IDProperty *prop= (IDProperty*)ptr->data;
	return prop;
}

#else

static void RNA_def_ID_property(BlenderRNA *brna)
{
	StructRNA *srna;
	PropertyRNA *prop;

	/* this is struct is used for holding the virtual
	 * PropertyRNA's for ID properties */
	srna= RNA_def_struct(brna, "IDProperty", "ID Property");

	/* IDP_STRING */
	prop= RNA_def_property(srna, "string", PROP_STRING, PROP_NONE);
	RNA_def_property_flag(prop, PROP_EXPORT);
	RNA_def_property_string_funcs(prop, "rna_IDProperty_string_get", "rna_IDProperty_string_length", "rna_IDProperty_string_set");

	/* IDP_INT */
	prop= RNA_def_property(srna, "int", PROP_INT, PROP_NONE);
	RNA_def_property_flag(prop, PROP_EXPORT);
	RNA_def_property_int_funcs(prop, "rna_IDProperty_int_get", "rna_IDProperty_int_set");

	prop= RNA_def_property(srna, "intarray", PROP_INT, PROP_NONE);
	RNA_def_property_flag(prop, PROP_EXPORT);
	RNA_def_property_array(prop, 1);
	RNA_def_property_int_funcs(prop, "rna_IDProperty_intarray_get", "rna_IDProperty_intarray_set");

	/* IDP_FLOAT */
	prop= RNA_def_property(srna, "float", PROP_FLOAT, PROP_NONE);
	RNA_def_property_flag(prop, PROP_EXPORT);
	RNA_def_property_float_funcs(prop, "rna_IDProperty_float_get", "rna_IDProperty_float_set");

	prop= RNA_def_property(srna, "floatarray", PROP_FLOAT, PROP_NONE);
	RNA_def_property_flag(prop, PROP_EXPORT);
	RNA_def_property_array(prop, 1);
	RNA_def_property_float_funcs(prop, "rna_IDProperty_floatarray_get", "rna_IDProperty_floatarray_set");

	/* IDP_DOUBLE */
	prop= RNA_def_property(srna, "double", PROP_FLOAT, PROP_NONE);
	RNA_def_property_flag(prop, PROP_EXPORT);
	RNA_def_property_float_funcs(prop, "rna_IDProperty_double_get", "rna_IDProperty_double_set");

	prop= RNA_def_property(srna, "doublearray", PROP_FLOAT, PROP_NONE);
	RNA_def_property_flag(prop, PROP_EXPORT);
	RNA_def_property_array(prop, 1);
	RNA_def_property_float_funcs(prop, "rna_IDProperty_doublearray_get", "rna_IDProperty_doublearray_set");

	/* IDP_GROUP */
	prop= RNA_def_property(srna, "group", PROP_POINTER, PROP_NONE);
	RNA_def_property_flag(prop, PROP_EXPORT|PROP_NOT_EDITABLE);
	RNA_def_property_struct_type(prop, "IDPropertyGroup");
	RNA_def_property_pointer_funcs(prop, "rna_IDProperty_group_get", 0, 0);

	/* IDP_ID -- not implemented yet in id properties */

	/* ID property groups > level 0, since level 0 group is merged
	 * with native RNA properties. the builtin_properties will take
	 * care of the properties here */
	srna= RNA_def_struct(brna, "IDPropertyGroup", "ID Property Group");
}

void RNA_def_ID(StructRNA *srna)
{
	PropertyRNA *prop;

	RNA_def_struct_flag(srna, STRUCT_ID);

	prop= RNA_def_property(srna, "name", PROP_STRING, PROP_NONE);
	RNA_def_property_string_sdna(prop, "ID", "name");
	RNA_def_property_ui_text(prop, "Name", "Object ID name.");
	RNA_def_property_string_funcs(prop, "rna_ID_name_get", "rna_ID_name_length", "rna_ID_name_set");
	RNA_def_struct_name_property(srna, prop);
}

void RNA_def_ID_types(BlenderRNA *brna)
{
	RNA_def_ID_property(brna);
}

#endif

