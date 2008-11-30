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

#include "rna_internal.h"

#include "DNA_property_types.h"

#ifdef RNA_RUNTIME

#else

void RNA_def_gameproperty(BlenderRNA *brna)
{
	StructRNA *srna;
	PropertyRNA *prop;

	static EnumPropertyItem gameproperty_types_items[] ={
		{PROP_BOOL, "BOOL", "Boolean", ""},
		{PROP_INT, "INT", "Integer", ""},
		{PROP_FLOAT, "FLOAT", "Float", ""},
		{PROP_STRING, "STRING", "String", ""},
		{PROP_TIME, "TIME", "Time", ""},
		{0, NULL, NULL, NULL}};

	srna= RNA_def_struct(brna, "GameProperty", NULL , "GameProperty");
	RNA_def_struct_sdna(srna, "bProperty");

	prop= RNA_def_property(srna, "gameproperty_name", PROP_STRING, PROP_NONE);
	RNA_def_property_string_sdna(prop, NULL, "name");
	RNA_def_property_string_maxlength(prop, 31);
	RNA_def_property_ui_text(prop, "Name", "Game Property name.");

	/* type is not editable, would need to do proper data free/alloc */
	prop= RNA_def_property(srna, "type", PROP_ENUM, PROP_NONE);
	RNA_def_property_flag(prop, PROP_NOT_EDITABLE);
	RNA_def_property_enum_items(prop, gameproperty_types_items);
	RNA_def_property_ui_text(prop, "Game Property Types", "Game Property types.");

}

#endif

