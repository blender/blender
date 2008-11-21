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

static StructRNA *rna_ID_refine(PointerRNA *ptr)
{
	ID *id= (ID*)ptr->data;

	switch(GS(id->name)) {
		case ID_LA: return &RNA_Lamp;
		case ID_ME: return &RNA_Mesh;
		case ID_OB: return &RNA_Object;
		case ID_SCE: return &RNA_Scene;
		case ID_WM: return &RNA_WindowManager;
		default: return &RNA_ID;
	}
}

#else

static void rna_def_ID_properties(BlenderRNA *brna)
{
	StructRNA *srna;
	PropertyRNA *prop;

	/* this is struct is used for holding the virtual
	 * PropertyRNA's for ID properties */
	srna= RNA_def_struct(brna, "IDProperty", NULL, "ID Property");

	/* IDP_STRING */
	prop= RNA_def_property(srna, "string", PROP_STRING, PROP_NONE);
	RNA_def_property_flag(prop, PROP_EXPORT|PROP_IDPROPERTY);

	/* IDP_INT */
	prop= RNA_def_property(srna, "int", PROP_INT, PROP_NONE);
	RNA_def_property_flag(prop, PROP_EXPORT|PROP_IDPROPERTY);

	prop= RNA_def_property(srna, "intarray", PROP_INT, PROP_NONE);
	RNA_def_property_flag(prop, PROP_EXPORT|PROP_IDPROPERTY);
	RNA_def_property_array(prop, 1);

	/* IDP_FLOAT */
	prop= RNA_def_property(srna, "float", PROP_FLOAT, PROP_NONE);
	RNA_def_property_flag(prop, PROP_EXPORT|PROP_IDPROPERTY);

	prop= RNA_def_property(srna, "floatarray", PROP_FLOAT, PROP_NONE);
	RNA_def_property_flag(prop, PROP_EXPORT|PROP_IDPROPERTY);
	RNA_def_property_array(prop, 1);

	/* IDP_DOUBLE */
	prop= RNA_def_property(srna, "double", PROP_FLOAT, PROP_NONE);
	RNA_def_property_flag(prop, PROP_EXPORT|PROP_IDPROPERTY);

	prop= RNA_def_property(srna, "doublearray", PROP_FLOAT, PROP_NONE);
	RNA_def_property_flag(prop, PROP_EXPORT|PROP_IDPROPERTY);
	RNA_def_property_array(prop, 1);

	/* IDP_GROUP */
	prop= RNA_def_property(srna, "group", PROP_POINTER, PROP_NONE);
	RNA_def_property_flag(prop, PROP_EXPORT|PROP_NOT_EDITABLE|PROP_IDPROPERTY);
	RNA_def_property_struct_type(prop, "IDPropertyGroup");

	/* IDP_ID -- not implemented yet in id properties */

	/* ID property groups > level 0, since level 0 group is merged
	 * with native RNA properties. the builtin_properties will take
	 * care of the properties here */
	srna= RNA_def_struct(brna, "IDPropertyGroup", NULL, "ID Property Group");
}

void rna_def_ID(BlenderRNA *brna)
{
	StructRNA *srna;
	PropertyRNA *prop;

	srna= RNA_def_struct(brna, "ID", NULL, "ID");
	RNA_def_struct_flag(srna, STRUCT_ID);
	RNA_def_struct_funcs(srna, NULL, "rna_ID_refine");

	prop= RNA_def_property(srna, "name", PROP_STRING, PROP_NONE);
	RNA_def_property_string_sdna(prop, "ID", "name");
	RNA_def_property_ui_text(prop, "Name", "Object ID name.");
	RNA_def_property_string_funcs(prop, "rna_ID_name_get", "rna_ID_name_length", "rna_ID_name_set");
	RNA_def_property_string_maxlength(prop, 22);
	RNA_def_struct_name_property(srna, prop);
}

void RNA_def_ID(BlenderRNA *brna)
{
	/* ID */
	rna_def_ID(brna);

	/* ID Properties */
	rna_def_ID_properties(brna);
}

#endif

