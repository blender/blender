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

#include "RNA_define.h"
#include "RNA_types.h"

#include "DNA_ID.h"

#ifdef RNA_RUNTIME

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

#else

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

#endif

