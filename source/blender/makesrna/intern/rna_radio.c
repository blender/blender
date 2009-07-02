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

#include "DNA_radio_types.h"

#ifdef RNA_RUNTIME

#else

void RNA_def_radio(BlenderRNA *brna)
{
	StructRNA *srna;
	PropertyRNA *prop;
	static EnumPropertyItem prop_drawtype_items[] = { 
		{RAD_WIREFRAME, "WIREFRAME", 0, "Wireframe", "Enables Wireframe draw mode"},
		{RAD_SOLID, "SOLID", 0, "Solid", "Enables Solid draw mode"},
		{RAD_GOURAUD, "GOURAUD", 0, "Gouraud", "Enables Gouraud draw mode"},
		{0, NULL, 0, NULL, NULL}};

	srna= RNA_def_struct(brna, "Radiosity", NULL);
	RNA_def_struct_ui_text(srna, "Radiosity", "Settings for radiosity simulation of indirect diffuse lighting.");
	RNA_def_struct_sdna(srna, "Radio");

	/* Enums */
	prop= RNA_def_property(srna, "draw_mode", PROP_ENUM, PROP_NONE);
	RNA_def_property_enum_sdna(prop, NULL, "drawtype");
	RNA_def_property_enum_items(prop, prop_drawtype_items);
	RNA_def_property_ui_text(prop, "Draw Mode", "Radiosity draw modes.");

	/* Number values */
	prop= RNA_def_property(srna, "hemi_resolution", PROP_INT, PROP_NONE);
	RNA_def_property_int_sdna(prop, NULL, "hemires");
	RNA_def_property_range(prop, 100, 1000);
	RNA_def_property_ui_text(prop, "Hemi Resolution", "Sets the size of a hemicube.");

	prop= RNA_def_property(srna, "max_iterations", PROP_INT, PROP_NONE);
	RNA_def_property_int_sdna(prop, NULL, "maxiter");
	RNA_def_property_range(prop, 0, 10000);
	RNA_def_property_ui_text(prop, "Max Iterations", "Limits the maximum number of radiosity rounds.");

	prop= RNA_def_property(srna, "multiplier", PROP_FLOAT, PROP_NONE);
	RNA_def_property_float_sdna(prop, NULL, "radfac");
	RNA_def_property_range(prop, 0.001f, 250.0f);
	RNA_def_property_ui_text(prop, "Multiplier", "Multiplies the energy values.");

	prop= RNA_def_property(srna, "gamma", PROP_FLOAT, PROP_NONE);
	RNA_def_property_float_sdna(prop, NULL, "gamma");
	RNA_def_property_range(prop, 0.2f, 10.0f);
	RNA_def_property_ui_text(prop, "Gamma", "Changes the contrast of the energy values.");

	prop= RNA_def_property(srna, "convergence", PROP_FLOAT, PROP_NONE);
	RNA_def_property_float_sdna(prop, NULL, "convergence");
	RNA_def_property_range(prop, 0.0f, 1.0f);
	RNA_def_property_ui_text(prop, "Convergence", "Sets the lower threshold of unshot energy.");

	prop= RNA_def_property(srna, "element_max", PROP_INT, PROP_NONE);
	RNA_def_property_int_sdna(prop, NULL, "elma");
	RNA_def_property_range(prop, 1, 500);
	RNA_def_property_ui_text(prop, "Element Max", "Sets maximum size of an element");

	prop= RNA_def_property(srna, "element_min", PROP_INT, PROP_NONE);
	RNA_def_property_int_sdna(prop, NULL, "elmi");
	RNA_def_property_range(prop, 1, 100);
	RNA_def_property_ui_text(prop, "Element Min", "Sets minimum size of an element");

	prop= RNA_def_property(srna, "patch_max", PROP_INT, PROP_NONE);
	RNA_def_property_int_sdna(prop, NULL, "pama");
	RNA_def_property_range(prop, 10, 1000);
	RNA_def_property_ui_text(prop, "Patch Max", "Sets maximum size of a patch.");

	prop= RNA_def_property(srna, "patch_min", PROP_INT, PROP_NONE);
	RNA_def_property_int_sdna(prop, NULL, "pami");
	RNA_def_property_range(prop, 10, 1000);
	RNA_def_property_ui_text(prop, "Patch Min", "Sets minimum size of a patch.");

	prop= RNA_def_property(srna, "subshoot_patch", PROP_INT, PROP_NONE);
	RNA_def_property_int_sdna(prop, NULL, "subshootp");
	RNA_def_property_range(prop, 0, 10);
	RNA_def_property_ui_text(prop, "SubShoot Patch", "Sets the number of times the environment is tested to detect paths.");

	prop= RNA_def_property(srna, "subshoot_element", PROP_INT, PROP_NONE);
	RNA_def_property_int_sdna(prop, NULL, "subshoote");
	RNA_def_property_range(prop, 0, 10);
	RNA_def_property_ui_text(prop, "SubShoot Element", "Sets the number of times the environment is tested to detect elements.");

	prop= RNA_def_property(srna, "max_elements", PROP_INT, PROP_NONE);
	RNA_def_property_int_sdna(prop, NULL, "maxnode");
	RNA_def_property_range(prop, 1, 250000);
	RNA_def_property_ui_text(prop, "Max Elements", "Sets the maximum allowed number of elements.");

	prop= RNA_def_property(srna, "max_subdiv_shoot", PROP_INT, PROP_NONE);
	RNA_def_property_int_sdna(prop, NULL, "maxsublamp");
	RNA_def_property_range(prop, 1, 250);
	RNA_def_property_ui_text(prop, "Max Subdiv Shoot", "Sets the maximum number of initial shoot patches that are evaluated");

	prop= RNA_def_property(srna, "remove_doubles_limit", PROP_INT, PROP_NONE);
	RNA_def_property_int_sdna(prop, NULL, "nodelim");
	RNA_def_property_range(prop, 0, 50);
	RNA_def_property_ui_text(prop, "Remove Doubles Limit", "Sets the range for removing doubles");

	/* flag */
	prop= RNA_def_property(srna, "show_limits", PROP_BOOLEAN, PROP_NONE);
	RNA_def_property_boolean_sdna(prop, NULL, "flag", RAD_SHOWLIMITS);
	RNA_def_property_ui_text(prop, "Show Limits", "Draws patch and element limits");

	prop= RNA_def_property(srna, "show_z", PROP_BOOLEAN, PROP_NONE);
	RNA_def_property_boolean_sdna(prop, NULL, "flag", RAD_SHOWZ);
	RNA_def_property_ui_text(prop, "Show Z", "Draws limits differently");
}

#endif

