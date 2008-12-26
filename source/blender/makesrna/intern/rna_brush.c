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
 * Contributor(s): Blender Foundation (2008), Juho Vepsäläinen
 *
 * ***** END GPL LICENSE BLOCK *****
 */

#include <stdlib.h>

#include "RNA_define.h"
#include "RNA_types.h"

#include "rna_internal.h"

#include "DNA_brush_types.h"

#ifdef RNA_RUNTIME

#else

void rna_def_brush(BlenderRNA *brna)
{
	StructRNA *srna;
	PropertyRNA *prop;
	static EnumPropertyItem prop_blend_items[] = {
		{BRUSH_BLEND_MIX, "MIX", "Mix", "Use mix blending mode while painting."},
		{BRUSH_BLEND_ADD, "ADD", "Add", "Use add blending mode while painting."},
		{BRUSH_BLEND_SUB, "SUB", "Subtract", "Use subtract blending mode while painting."},
		{BRUSH_BLEND_MUL, "MUL", "Multiply", "Use multiply blending mode while painting."},
		{BRUSH_BLEND_LIGHTEN, "LIGHTEN", "Lighten", "Use lighten blending mode while painting."},
		{BRUSH_BLEND_DARKEN, "DARKEN", "Darken", "Use darken blending mode while painting."},
		{BRUSH_BLEND_ERASE_ALPHA, "ERASE_ALPHA", "Erase Alpha", "Erase alpha while painting."},
		{BRUSH_BLEND_ADD_ALPHA, "ADD_ALPHA", "Add Alpha", "Add alpha while painting."},
		{0, NULL, NULL, NULL}};
	
	srna= RNA_def_struct(brna, "Brush", "ID");
	RNA_def_struct_ui_text(srna, "Brush", "stores brush setting for painting in the image view, sculpting and projection painting");
	
	/* enums */
	prop= RNA_def_property(srna, "blend", PROP_ENUM, PROP_NONE);
	RNA_def_property_enum_items(prop, prop_blend_items);
	RNA_def_property_ui_text(prop, "Blending mode", "Brush blending mode.");
	
	/* number values */
	prop= RNA_def_property(srna, "size", PROP_INT, PROP_NONE);
	RNA_def_property_range(prop, 1, 200);
	RNA_def_property_ui_text(prop, "Size", "Diameter of the brush.");
	
	prop= RNA_def_property(srna, "falloff", PROP_FLOAT, PROP_NONE);
	RNA_def_property_float_sdna(prop, NULL, "innerradius");
	RNA_def_property_range(prop, 0.0f, 1.0f);
	RNA_def_property_ui_text(prop, "Falloff", "Falloff radius of the brush.");
	
	prop= RNA_def_property(srna, "spacing", PROP_FLOAT, PROP_NONE);
	RNA_def_property_float_sdna(prop, NULL, "spacing");
	RNA_def_property_range(prop, 1.0f, 100.0f);
	RNA_def_property_ui_text(prop, "Spacing", "Spacing between brush stamps.");
	
	prop= RNA_def_property(srna, "rate", PROP_FLOAT, PROP_NONE);
	RNA_def_property_float_sdna(prop, NULL, "rate");
	RNA_def_property_range(prop, 0.010f, 1.0f);
	RNA_def_property_ui_text(prop, "Rate", "Number of paints per second for Airbrush.");
	
	prop= RNA_def_property(srna, "color", PROP_FLOAT, PROP_COLOR);
	RNA_def_property_float_sdna(prop, NULL, "rgb");
	RNA_def_property_ui_text(prop, "Color", "");
	RNA_def_property_ui_range(prop, 0.0f , 1.0f, 10.0f, 3);
	
	prop= RNA_def_property(srna, "opacity", PROP_FLOAT, PROP_NONE);
	RNA_def_property_float_sdna(prop, NULL, "alpha");
	RNA_def_property_range(prop, 0.0f, 1.0f);
	RNA_def_property_ui_text(prop, "Opacity", "The amount of pressure on the brush.");
	
	/* flag */
	prop= RNA_def_property(srna, "airbrush", PROP_BOOLEAN, PROP_NONE);
	RNA_def_property_boolean_sdna(prop, NULL, "flag", BRUSH_AIRBRUSH);
	RNA_def_property_ui_text(prop, "Airbrush", "Keep applying paint effect while holding mouse (spray).");
	
	prop= RNA_def_property(srna, "wrap", PROP_BOOLEAN, PROP_NONE);
	RNA_def_property_boolean_sdna(prop, NULL, "flag", BRUSH_TORUS);
	RNA_def_property_ui_text(prop, "Wrap", "Enable torus wrapping while painting.");
	
	prop= RNA_def_property(srna, "alpha_pressure", PROP_BOOLEAN, PROP_NONE);
	RNA_def_property_boolean_sdna(prop, NULL, "flag", BRUSH_ALPHA_PRESSURE);
	RNA_def_property_ui_text(prop, "Opacity Pressure", "Enable tablet pressure sensitivity for opacity.");
	
	prop= RNA_def_property(srna, "size_pressure", PROP_BOOLEAN, PROP_NONE);
	RNA_def_property_boolean_sdna(prop, NULL, "flag", BRUSH_SIZE_PRESSURE);
	RNA_def_property_ui_text(prop, "Size Pressure", "Enable tablet pressure sensitivity for size.");
	
	prop= RNA_def_property(srna, "falloff_pressure", PROP_BOOLEAN, PROP_NONE);
	RNA_def_property_boolean_sdna(prop, NULL, "flag", BRUSH_RAD_PRESSURE);
	RNA_def_property_ui_text(prop, "Falloff Pressure", "Enable tablet pressure sensitivity for falloff.");
	
	prop= RNA_def_property(srna, "spacing_pressure", PROP_BOOLEAN, PROP_NONE);
	RNA_def_property_boolean_sdna(prop, NULL, "flag", BRUSH_SPACING_PRESSURE);
	RNA_def_property_ui_text(prop, "Spacing Pressure", "Enable tablet pressure sensitivity for spacing.");
	
	/* not exposed in the interface yet
	prop= RNA_def_property(srna, "fixed_tex", PROP_BOOLEAN, PROP_NONE);
	RNA_def_property_boolean_sdna(prop, NULL, "flag", BRUSH_FIXED_TEX);
	RNA_def_property_ui_text(prop, "Fixed Texture", "Keep texture origin in fixed position.");*/

	/* texture */
	prop= RNA_def_property(srna, "texture_slots", PROP_COLLECTION, PROP_NONE);
	RNA_def_property_collection_sdna(prop, NULL, "mtex", "");
	RNA_def_property_struct_type(prop, "UnknownType");
	RNA_def_property_ui_text(prop, "Textures Slots", "");

	prop= RNA_def_property(srna, "active_texture", PROP_INT, PROP_NONE);
	RNA_def_property_int_sdna(prop, NULL, "texact");
	RNA_def_property_range(prop, 0, MAX_MTEX-1);
	RNA_def_property_ui_text(prop, "Active Texture", "Active texture index.");

	/* clone tool */
	prop= RNA_def_property(srna, "clone_image", PROP_POINTER, PROP_NONE);
	RNA_def_property_pointer_sdna(prop, NULL, "clone.image");
	RNA_def_property_flag(prop, PROP_NOT_EDITABLE);
	RNA_def_property_ui_text(prop, "Clone Image", "Image for clone tool.");
	
	prop= RNA_def_property(srna, "clone_opacity", PROP_FLOAT, PROP_NONE);
	RNA_def_property_float_sdna(prop, NULL, "clone.alpha");
	RNA_def_property_range(prop, 0.0f, 1.0f);
	RNA_def_property_ui_text(prop, "Clone Opacity", "Opacity of clone image display.");

	prop= RNA_def_property(srna, "clone_offset", PROP_FLOAT, PROP_VECTOR);
	RNA_def_property_float_sdna(prop, NULL, "clone.offset");
	RNA_def_property_ui_text(prop, "Clone Offset", "");
	RNA_def_property_ui_range(prop, -1.0f , 1.0f, 10.0f, 3);
}

void RNA_def_brush(BlenderRNA *brna)
{
	rna_def_brush(brna);
}

#endif
