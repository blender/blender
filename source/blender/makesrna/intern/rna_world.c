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

#include "DNA_world_types.h"

#ifdef RNA_RUNTIME

#else

void RNA_def_world(BlenderRNA *brna)
{
	StructRNA *srna;
	PropertyRNA *prop;
/*
	float horr, horg, horb, hork;
	float zenr, zeng, zenb, zenk;
	float ambr, ambg, ambb, ambk;

	static EnumPropertyItem gameproperty_types_items[] ={
		{PROP_BOOL, "BOOL", "Boolean", ""},
		{PROP_INT, "INT", "Integer", ""},
		{PROP_FLOAT, "FLOAT", "Float", ""},
		{PROP_STRING, "STRING", "String", ""},
		{PROP_TIME, "TIME", "Time", ""},
		{0, NULL, NULL, NULL}};
*/
	srna= RNA_def_struct(brna, "World", "ID" , "World");
	RNA_def_struct_sdna(srna, "World");

/*	When MTex and IPO was defined uncomment this section:
	prop= RNA_def_property(srna, "ipo", PROP_POINTER, PROP_NONE);
	RNA_def_property_struct_type(prop, "ID");
	RNA_def_property_ui_text(prop, "IPO", "IPO associated with this world setting.");


	prop= RNA_def_property(srna, "mtex", PROP_POINTER, PROP_NONE);
	RNA_def_property_struct_type(prop, "MTex");
	RNA_def_property_ui_text(prop, "MTex", "MTex associated with this world setting.");
*/

	/* Horizontal Color */
	prop= RNA_def_property(srna, "horizontal_color_red", PROP_FLOAT, PROP_NONE);
	RNA_def_property_float_sdna(prop, NULL, "horr");
	RNA_def_property_range(prop, 0.0, 1.0);
	RNA_def_property_ui_text(prop, "Horizontal Red Color", "Horizontal red color of this world.");
	
	prop= RNA_def_property(srna, "horizontal_color_green", PROP_FLOAT, PROP_NONE);
	RNA_def_property_float_sdna(prop, NULL, "horg");
	RNA_def_property_range(prop, 0.0, 1.0);
	RNA_def_property_ui_text(prop, "Horizontal Green Color", "Horizontal green color of this world.");

	prop= RNA_def_property(srna, "horizontal_color_blue", PROP_FLOAT, PROP_NONE);
	RNA_def_property_float_sdna(prop, NULL, "horb");
	RNA_def_property_range(prop, 0.0, 1.0);
	RNA_def_property_ui_text(prop, "Horizontal Blue Color", "Horizontal blue color of this world.");

	/* Zenith Color */
	prop= RNA_def_property(srna, "zenith_color_red", PROP_FLOAT, PROP_NONE);
	RNA_def_property_float_sdna(prop, NULL, "horr");
	RNA_def_property_range(prop, 0.0, 1.0);
	RNA_def_property_ui_text(prop, "Zenith Red Color", "Zenith red color of this world.");
	
	prop= RNA_def_property(srna, "zenith_color_green", PROP_FLOAT, PROP_NONE);
	RNA_def_property_float_sdna(prop, NULL, "horg");
	RNA_def_property_range(prop, 0.0, 1.0);
	RNA_def_property_ui_text(prop, "Zenith Green Color", "Zenith green color of this world.");

	prop= RNA_def_property(srna, "zenith_color_blue", PROP_FLOAT, PROP_NONE);
	RNA_def_property_float_sdna(prop, NULL, "horb");
	RNA_def_property_range(prop, 0.0, 1.0);
	RNA_def_property_ui_text(prop, "Zenith Blue Color", "Zenith blue color of this world.");

	/* Ambiant Color */
	prop= RNA_def_property(srna, "ambiant_color_red", PROP_FLOAT, PROP_NONE);
	RNA_def_property_float_sdna(prop, NULL, "horr");
	RNA_def_property_range(prop, 0.0, 1.0);
	RNA_def_property_ui_text(prop, "Ambiant Red Color", "Ambiant red color of this world.");
	
	prop= RNA_def_property(srna, "ambiant_color_green", PROP_FLOAT, PROP_NONE);
	RNA_def_property_float_sdna(prop, NULL, "horg");
	RNA_def_property_range(prop, 0.0, 1.0);
	RNA_def_property_ui_text(prop, "Ambiant Green Color", "Ambiant green color of this world.");

	prop= RNA_def_property(srna, "ambiant_color_blue", PROP_FLOAT, PROP_NONE);
	RNA_def_property_float_sdna(prop, NULL, "horb");
	RNA_def_property_range(prop, 0.0, 1.0);
	RNA_def_property_ui_text(prop, "Ambiant Blue Color", "Ambiant blue color of this world.");

	/* exp, range */
	prop= RNA_def_property(srna, "exposure", PROP_FLOAT, PROP_NONE);
	RNA_def_property_float_sdna(prop, NULL, "exp");
	RNA_def_property_range(prop, 0.0, 1.0);
	RNA_def_property_ui_text(prop, "Exposure", "Amount of exponential color correction for light.");

	prop= RNA_def_property(srna, "range", PROP_FLOAT, PROP_NONE);
	RNA_def_property_float_sdna(prop, NULL, "range");
	RNA_def_property_range(prop, 0.2, 5.0);
	RNA_def_property_ui_text(prop, "Range", "The color amount that will be mapped on color 1.0.");

	/* sky type */
	prop= RNA_def_property(srna, "blend_sky", PROP_BOOLEAN, PROP_NONE);
	RNA_def_property_boolean_sdna(prop, NULL, "skytype", WO_SKYBLEND);
	RNA_def_property_ui_text(prop, "Blend Sky", "Renders background with natural progression from horizon to zenith.");

	prop= RNA_def_property(srna, "paper_sky", PROP_BOOLEAN, PROP_NONE);
	RNA_def_property_boolean_sdna(prop, NULL, "skytype", WO_SKYPAPER);
	RNA_def_property_ui_text(prop, "Paper Sky", "Flattens blend or texture coordinates.");

	prop= RNA_def_property(srna, "real_sky", PROP_BOOLEAN, PROP_NONE);
	RNA_def_property_boolean_sdna(prop, NULL, "skytype", WO_SKYREAL);
	RNA_def_property_ui_text(prop, "Real Sky", "Renders background with a real horizon.");

}

#endif

