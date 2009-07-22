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
 * Contributor(s): Campbell Barton
 *
 * ***** END GPL LICENSE BLOCK *****
 */

#include <stdlib.h>

#include "RNA_define.h"
#include "RNA_types.h"

#include "rna_internal.h"

#include "DNA_scene_types.h"

#ifdef RNA_RUNTIME

#else

void RNA_def_vpaint(BlenderRNA *brna)
{
	StructRNA *srna;
	PropertyRNA *prop;
	static EnumPropertyItem prop_mode_items[] = {
		{0, "MIX", 0, "Mix", "Use mix blending mode while painting."},
		{1, "ADD", 0, "Add", "Use add blending mode while painting."},
		{2, "SUB", 0, "Subtract", "Use subtract blending mode while painting."},
		{3, "MUL", 0, "Multiply", "Use multiply blending mode while painting."},
		{4, "BLUR", 0, "Blur", "Blur the color with surrounding values"},
		{5, "LIGHTEN", 0, "Lighten", "Use lighten blending mode while painting."},
		{6, "DARKEN", 0, "Darken", "Use darken blending mode while painting."},
		{0, NULL, 0, NULL, NULL}};
	
	srna= RNA_def_struct(brna, "VPaint", NULL);
	RNA_def_struct_ui_text(srna, "Vertex Paint", "Properties of the Vpaint tool.");
    
	prop= RNA_def_property(srna, "brush", PROP_POINTER, PROP_NONE);
	RNA_def_property_struct_type(prop, "Brush");
	RNA_def_property_ui_text(prop, "Brush", "");

	prop= RNA_def_property(srna, "mode", PROP_ENUM, PROP_NONE);
	RNA_def_property_enum_items(prop, prop_mode_items);
	RNA_def_property_ui_text(prop, "Brush Mode", "The Mode in which color is painted.");
	
	prop= RNA_def_property(srna, "all_faces", PROP_BOOLEAN, PROP_NONE);
	RNA_def_property_boolean_sdna(prop, NULL, "flag", VP_AREA);
	RNA_def_property_ui_text(prop, "All Faces", "Paint on all faces inside brush.");
	
	prop= RNA_def_property(srna, "vertex_dist", PROP_BOOLEAN, PROP_NONE);
	RNA_def_property_boolean_sdna(prop, NULL, "flag", VP_SOFT);
	RNA_def_property_ui_text(prop, "Vertex Dist", "Use distances to vertices (instead of paint entire faces).");
	
	prop= RNA_def_property(srna, "normals", PROP_BOOLEAN, PROP_NONE);
	RNA_def_property_boolean_sdna(prop, NULL, "flag", VP_NORMALS);
	RNA_def_property_ui_text(prop, "Normals", "Applies the vertex normal before painting.");
	
	prop= RNA_def_property(srna, "spray", PROP_BOOLEAN, PROP_NONE);
	RNA_def_property_boolean_sdna(prop, NULL, "flag", VP_SPRAY);
	RNA_def_property_ui_text(prop, "Spray", "Keep applying paint effect while holding mouse.");
	
	prop= RNA_def_property(srna, "gamma", PROP_FLOAT, PROP_NONE);
	RNA_def_property_range(prop, 0.1f, 5.0f);
	RNA_def_property_ui_text(prop, "Gamma", "Vpaint Gamma.");
	
	prop= RNA_def_property(srna, "mul", PROP_FLOAT, PROP_NONE);
	RNA_def_property_range(prop, 0.1f, 50.0f);
	RNA_def_property_ui_text(prop, "Mul", "Vpaint Mul.");

}

#endif

