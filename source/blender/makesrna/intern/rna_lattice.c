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

#include "rna_internal.h"

#include "DNA_lattice_types.h"
#include "DNA_key_types.h"

#ifdef RNA_RUNTIME
#else

void RNA_def_lattice(BlenderRNA *brna)
{
	StructRNA *srna;
	PropertyRNA *prop;

	static EnumPropertyItem prop_keyblock_type_items[] = {
		{KEY_LINEAR, "KEY_LINEAR", "Linear", ""},
		{KEY_CARDINAL, "KEY_CARDINAL", "Cardinal", ""},
		{KEY_BSPLINE, "KEY_BSPLINE", "BSpline", ""},
		{0, NULL, NULL, NULL}};

	srna= RNA_def_struct(brna, "Lattice", "ID", "Lattice");

	prop= RNA_def_property(srna, "points_u", PROP_INT, PROP_NONE);
	RNA_def_property_int_sdna(prop, NULL, "pntsu");
	RNA_def_property_flag(prop, PROP_NOT_EDITABLE);
	RNA_def_property_ui_text(prop, "U", "Points in U direction.");

	prop= RNA_def_property(srna, "points_v", PROP_INT, PROP_NONE);
	RNA_def_property_int_sdna(prop, NULL, "pntsv");
	RNA_def_property_flag(prop, PROP_NOT_EDITABLE);
	RNA_def_property_ui_text(prop, "V", "Points in V direction.");

	prop= RNA_def_property(srna, "points_w", PROP_INT, PROP_NONE);
	RNA_def_property_int_sdna(prop, NULL, "pntsw");
	RNA_def_property_flag(prop, PROP_NOT_EDITABLE);
	RNA_def_property_ui_text(prop, "W", "Points in W direction.");

	prop= RNA_def_property(srna, "interpolation_type_u", PROP_ENUM, PROP_NONE);
	RNA_def_property_enum_sdna(prop, NULL, "typeu");
	RNA_def_property_enum_items(prop, prop_keyblock_type_items);
	RNA_def_property_ui_text(prop, "Interpolation Type U", "");

	prop= RNA_def_property(srna, "interpolation_type_v", PROP_ENUM, PROP_NONE);
	RNA_def_property_enum_sdna(prop, NULL, "typev");
	RNA_def_property_enum_items(prop, prop_keyblock_type_items);
	RNA_def_property_ui_text(prop, "Interpolation Type V", "");

	prop= RNA_def_property(srna, "interpolation_type_w", PROP_ENUM, PROP_NONE);
	RNA_def_property_enum_sdna(prop, NULL, "typew");
	RNA_def_property_enum_items(prop, prop_keyblock_type_items);
	RNA_def_property_ui_text(prop, "Interpolation Type W", "");

	prop= RNA_def_property(srna, "outside", PROP_BOOLEAN, PROP_NONE);
	RNA_def_property_boolean_sdna(prop, NULL, "flag", LT_OUTSIDE);
	RNA_def_property_ui_text(prop, "Outside", "Only draw, and take into account, the outer vertices.");
}

#endif

