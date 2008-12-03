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

#include "DNA_key_types.h"

#ifdef RNA_RUNTIME
#else

void RNA_def_keyblock(BlenderRNA *brna)
{
	StructRNA *srna;
	PropertyRNA *prop;

	static EnumPropertyItem prop_keyblock_type_items[] = {
		{KEY_LINEAR, "KEY_LINEAR", "Linear", ""},
		{KEY_CARDINAL, "KEY_CARDINAL", "Cardinal", ""},
		{KEY_BSPLINE, "KEY_BSPLINE", "BSpline", ""},
		{0, NULL, NULL, NULL}};

	srna= RNA_def_struct(brna, "KeyBlock", NULL, "KeyBlock");

	prop= RNA_def_property(srna, "current_pos", PROP_FLOAT, PROP_NONE);
	RNA_def_property_float_sdna(prop, NULL, "pos");
	RNA_def_property_ui_text(prop, "CurrentPosition", "Current Position.");

	prop= RNA_def_property(srna, "current_val", PROP_FLOAT, PROP_NONE);
	RNA_def_property_float_sdna(prop, NULL, "curval");
	RNA_def_property_ui_text(prop, "CurrentValue", "Current Value.");

	prop= RNA_def_property(srna, "type", PROP_ENUM, PROP_NONE);
	RNA_def_property_enum_sdna(prop, NULL, "type", 0);
	RNA_def_property_enum_items(prop, prop_keyblock_type_items);
	RNA_def_property_ui_text(prop, "Type", "");

	prop= RNA_def_property(srna, "name", PROP_STRING, PROP_NONE);
	RNA_def_property_string_sdna(prop, NULL, "name");
	RNA_def_property_ui_text(prop, "Name", "Current Shape Key name.");
	RNA_def_property_string_maxlength(prop, 32);

	prop= RNA_def_property(srna, "vertex_group", PROP_STRING, PROP_NONE);
	RNA_def_property_string_sdna(prop, NULL, "vgroup");
	RNA_def_property_ui_text(prop, "Vertex Group", "");
	RNA_def_property_string_maxlength(prop, 32);

	/* XXX couldn't quite figure this one out: shape key number, channel code? */
	prop= RNA_def_property(srna, "channel", PROP_INT, PROP_NONE);
	RNA_def_property_int_sdna(prop, NULL, "adrcode");
	RNA_def_property_flag(prop, PROP_NOT_EDITABLE);
	RNA_def_property_ui_text(prop, "Channel", "");

	prop= RNA_def_property(srna, "relative", PROP_BOOLEAN, PROP_NONE);
	RNA_def_property_boolean_sdna(prop, NULL, "relative", 1);
	RNA_def_property_ui_text(prop, "Relative", "Makes Shape Keys relative.");

	prop= RNA_def_property(srna, "slidermin", PROP_FLOAT, PROP_NONE);
	RNA_def_property_float_sdna(prop, NULL, "slidermin");
	RNA_def_property_ui_text(prop, "SliderMin", "Minimum for Slider.");

	prop= RNA_def_property(srna, "slidermax", PROP_FLOAT, PROP_NONE);
	RNA_def_property_float_sdna(prop, NULL, "slidermax");
	RNA_def_property_ui_text(prop, "SliderMax", "Maximum for Slider.");

}

void RNA_def_key(BlenderRNA *brna)
{
	StructRNA *srna;
	PropertyRNA *prop;

	RNA_def_keyblock(brna);

	srna= RNA_def_struct(brna, "Key", "ID", "Key");

	prop= RNA_def_property(srna, "refkey", PROP_POINTER, PROP_NONE);
	RNA_def_property_struct_type(prop, "KeyBlock");
	RNA_def_property_ui_text(prop, "Reference Key", "");

	prop= RNA_def_property(srna, "keyblocks", PROP_COLLECTION, PROP_NONE);
	RNA_def_property_collection_sdna(prop, NULL, "block", NULL);
	RNA_def_property_struct_type(prop, "KeyBlock");
	RNA_def_property_ui_text(prop, "KeyBlocks", "Key Blocks.");

	prop= RNA_def_property(srna, "num_keyblocks", PROP_INT, PROP_NONE);
	RNA_def_property_int_sdna(prop, NULL, "totkey");
	RNA_def_property_ui_text(prop, "NumKeyBlocks", "Number of KeyBlocks.");
	
	prop= RNA_def_property(srna, "ipo", PROP_POINTER, PROP_NONE);
	RNA_def_property_struct_type(prop, "Ipo");
	RNA_def_property_ui_text(prop, "Ipo", "");

	prop= RNA_def_property(srna, "from", PROP_POINTER, PROP_NONE);
	RNA_def_property_struct_type(prop, "ID");
	RNA_def_property_ui_text(prop, "From", "");

	prop= RNA_def_property(srna, "relative", PROP_BOOLEAN, PROP_NONE);
	RNA_def_property_boolean_sdna(prop, NULL, "type", 1);
	RNA_def_property_ui_text(prop, "Relative", "");

	prop= RNA_def_property(srna, "slurph", PROP_INT, PROP_NONE);
	RNA_def_property_int_sdna(prop, NULL, "slurph");
	RNA_def_property_ui_text(prop, "Slurph", "");


}

#endif

