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
 * Inc., 51 Franklin Street, Fifth Floor, Boston, MA 02110-1301, USA.
 *
 * Contributor(s): Blender Foundation (2008).
 *
 * ***** END GPL LICENSE BLOCK *****
 */

#include <stdio.h>
#include <stdlib.h>

#include "RNA_define.h"

#include "rna_internal.h"

#include "DNA_linestyle_types.h"
#include "DNA_material_types.h"
#include "DNA_texture_types.h"

#include "WM_types.h"
#include "WM_api.h"

EnumPropertyItem linestyle_color_modifier_type_items[] ={
	{LS_MODIFIER_ALONG_STROKE, "ALONG_STROKE", ICON_MODIFIER, "Along Stroke", ""},
	{LS_MODIFIER_DISTANCE_FROM_CAMERA, "DISTANCE_FROM_CAMERA", ICON_MODIFIER, "Distance from Camera", ""},
	{LS_MODIFIER_DISTANCE_FROM_OBJECT, "DISTANCE_FROM_OBJECT", ICON_MODIFIER, "Distance from Object", ""},
	{0, NULL, 0, NULL, NULL}};

EnumPropertyItem linestyle_alpha_modifier_type_items[] ={
	{LS_MODIFIER_ALONG_STROKE, "ALONG_STROKE", ICON_MODIFIER, "Along Stroke", ""},
	{LS_MODIFIER_DISTANCE_FROM_CAMERA, "DISTANCE_FROM_CAMERA", ICON_MODIFIER, "Distance from Camera", ""},
	{LS_MODIFIER_DISTANCE_FROM_OBJECT, "DISTANCE_FROM_OBJECT", ICON_MODIFIER, "Distance from Object", ""},
	{0, NULL, 0, NULL, NULL}};

EnumPropertyItem linestyle_thickness_modifier_type_items[] ={
	{LS_MODIFIER_ALONG_STROKE, "ALONG_STROKE", ICON_MODIFIER, "Along Stroke", ""},
	{LS_MODIFIER_DISTANCE_FROM_CAMERA, "DISTANCE_FROM_CAMERA", ICON_MODIFIER, "Distance from Camera", ""},
	{LS_MODIFIER_DISTANCE_FROM_OBJECT, "DISTANCE_FROM_OBJECT", ICON_MODIFIER, "Distance from Object", ""},
	{0, NULL, 0, NULL, NULL}};

#ifdef RNA_RUNTIME

static StructRNA *rna_LineStyle_color_modifier_refine(struct PointerRNA *ptr)
{
	LineStyleModifier *m = (LineStyleModifier *)ptr->data;

	switch(m->type) {
		case LS_MODIFIER_ALONG_STROKE:
			return &RNA_LineStyleColorModifier_AlongStroke;
		case LS_MODIFIER_DISTANCE_FROM_CAMERA:
			return &RNA_LineStyleColorModifier_DistanceFromCamera;
		case LS_MODIFIER_DISTANCE_FROM_OBJECT:
			return &RNA_LineStyleColorModifier_DistanceFromObject;
		default:
			return &RNA_LineStyleColorModifier;
	}
}

static StructRNA *rna_LineStyle_alpha_modifier_refine(struct PointerRNA *ptr)
{
	LineStyleModifier *m = (LineStyleModifier *)ptr->data;

	switch(m->type) {
		case LS_MODIFIER_ALONG_STROKE:
			return &RNA_LineStyleAlphaModifier_AlongStroke;
		case LS_MODIFIER_DISTANCE_FROM_CAMERA:
			return &RNA_LineStyleAlphaModifier_DistanceFromCamera;
		case LS_MODIFIER_DISTANCE_FROM_OBJECT:
			return &RNA_LineStyleAlphaModifier_DistanceFromObject;
		default:
			return &RNA_LineStyleAlphaModifier;
	}
}

static StructRNA *rna_LineStyle_thickness_modifier_refine(struct PointerRNA *ptr)
{
	LineStyleModifier *m = (LineStyleModifier *)ptr->data;

	switch(m->type) {
		case LS_MODIFIER_ALONG_STROKE:
			return &RNA_LineStyleThicknessModifier_AlongStroke;
		case LS_MODIFIER_DISTANCE_FROM_CAMERA:
			return &RNA_LineStyleThicknessModifier_DistanceFromCamera;
		case LS_MODIFIER_DISTANCE_FROM_OBJECT:
			return &RNA_LineStyleThicknessModifier_DistanceFromObject;
		default:
			return &RNA_LineStyleThicknessModifier;
	}
}

static char *rna_LineStyle_color_modifier_path(PointerRNA *ptr)
{
	return BLI_sprintfN("color_modifiers[\"%s\"]", ((LineStyleModifier*)ptr->data)->name);
}

static char *rna_LineStyle_alpha_modifier_path(PointerRNA *ptr)
{
	return BLI_sprintfN("alpha_modifiers[\"%s\"]", ((LineStyleModifier*)ptr->data)->name);
}

static char *rna_LineStyle_thickness_modifier_path(PointerRNA *ptr)
{
	return BLI_sprintfN("thickness_modifiers[\"%s\"]", ((LineStyleModifier*)ptr->data)->name);
}

#else

#include "DNA_material_types.h"

static void rna_def_modifier_type_common(StructRNA *srna, EnumPropertyItem *modifier_type_items)
{
	PropertyRNA *prop;

	prop= RNA_def_property(srna, "type", PROP_ENUM, PROP_NONE);
	RNA_def_property_enum_sdna(prop, NULL, "modifier.type");
	RNA_def_property_enum_items(prop, modifier_type_items);
	RNA_def_property_clear_flag(prop, PROP_EDITABLE);
	RNA_def_property_ui_text(prop, "Modifier Type", "Type of the modifier.");

	prop= RNA_def_property(srna, "name", PROP_STRING, PROP_NONE);
	RNA_def_property_string_sdna(prop, NULL, "modifier.name");
	RNA_def_property_ui_text(prop, "Modifier Name", "Name of the modifier.");
	RNA_def_property_update(prop, NC_SCENE, NULL);
	RNA_def_struct_name_property(srna, prop);

	prop= RNA_def_property(srna, "influence", PROP_FLOAT, PROP_FACTOR);
	RNA_def_property_float_sdna(prop, NULL, "modifier.influence");
	RNA_def_property_range(prop, 0.0f, 1.0f);
	RNA_def_property_ui_text(prop, "Influence", "Influence factor by which the modifier changes the property.");
	RNA_def_property_update(prop, NC_SCENE, NULL);

	prop= RNA_def_property(srna, "enabled", PROP_BOOLEAN, PROP_NONE);
	RNA_def_property_boolean_sdna(prop, NULL, "modifier.flags", LS_MODIFIER_ENABLED);
	RNA_def_property_ui_text(prop, "Enabled", "True if the modifier is enabled.");

	prop= RNA_def_property(srna, "expanded", PROP_BOOLEAN, PROP_NONE);
	RNA_def_property_boolean_sdna(prop, NULL, "modifier.flags", LS_MODIFIER_EXPANDED);
	RNA_def_property_ui_text(prop, "Expanded", "True if the modifier tab is expanded.");
}

static void rna_def_color_modifier(StructRNA *srna)
{
	rna_def_modifier_type_common(srna, linestyle_color_modifier_type_items);
}

static void rna_def_alpha_modifier(StructRNA *srna)
{
	rna_def_modifier_type_common(srna, linestyle_alpha_modifier_type_items);
}

static void rna_def_thickness_modifier(StructRNA *srna)
{
	rna_def_modifier_type_common(srna, linestyle_thickness_modifier_type_items);
}

static void rna_def_modifier_color_common(StructRNA *srna, int range)
{
	PropertyRNA *prop;

	static EnumPropertyItem ramp_blend_items[] = {
		{MA_RAMP_BLEND, "MIX", 0, "Mix", ""},
		{MA_RAMP_ADD, "ADD", 0, "Add", ""},
		{MA_RAMP_MULT, "MULTIPLY", 0, "Multiply", ""},
		{MA_RAMP_SUB, "SUBTRACT", 0, "Subtract", ""},
		{MA_RAMP_SCREEN, "SCREEN", 0, "Screen", ""},
		{MA_RAMP_DIV, "DIVIDE", 0, "Divide", ""},
		{MA_RAMP_DIFF, "DIFFERENCE", 0, "Difference", ""},
		{MA_RAMP_DARK, "DARKEN", 0, "Darken", ""},
		{MA_RAMP_LIGHT, "LIGHTEN", 0, "Lighten", ""},
		{MA_RAMP_OVERLAY, "OVERLAY", 0, "Overlay", ""},
		{MA_RAMP_DODGE, "DODGE", 0, "Dodge", ""},
		{MA_RAMP_BURN, "BURN", 0, "Burn", ""},
		{MA_RAMP_HUE, "HUE", 0, "Hue", ""},
		{MA_RAMP_SAT, "SATURATION", 0, "Saturation", ""},
		{MA_RAMP_VAL, "VALUE", 0, "Value", ""},
		{MA_RAMP_COLOR, "COLOR", 0, "Color", ""},
		{MA_RAMP_SOFT, "SOFT LIGHT", 0, "Soft Light", ""}, 
		{MA_RAMP_LINEAR, "LINEAR LIGHT", 0, "Linear Light", ""}, 
		{0, NULL, 0, NULL, NULL}};

	prop= RNA_def_property(srna, "color_ramp", PROP_POINTER, PROP_NONE);
	RNA_def_property_pointer_sdna(prop, NULL, "color_ramp");
	RNA_def_property_struct_type(prop, "ColorRamp");
	RNA_def_property_ui_text(prop, "Color Ramp", "Color ramp used to change line color.");
	RNA_def_property_update(prop, NC_SCENE, NULL);

	prop= RNA_def_property(srna, "blend", PROP_ENUM, PROP_NONE);
	RNA_def_property_enum_sdna(prop, NULL, "blend");
	RNA_def_property_enum_items(prop, ramp_blend_items);
	RNA_def_property_ui_text(prop, "Ramp Blend", "Specify how the color ramp and line color are blended.");
	RNA_def_property_update(prop, NC_SCENE, NULL);

	if (range) {
		prop= RNA_def_property(srna, "range_min", PROP_FLOAT, PROP_NONE);
		RNA_def_property_float_sdna(prop, NULL, "range_min");
		RNA_def_property_ui_text(prop, "Range Min", "Lower bound of the input range the mapping is applied.");
		RNA_def_property_update(prop, NC_SCENE, NULL);

		prop= RNA_def_property(srna, "range_max", PROP_FLOAT, PROP_NONE);
		RNA_def_property_float_sdna(prop, NULL, "range_max");
		RNA_def_property_ui_text(prop, "Range Max", "Upper bound of the input range the mapping is applied.");
		RNA_def_property_update(prop, NC_SCENE, NULL);
	}
}

static void rna_def_modifier_curve_common(StructRNA *srna, int range, int value)
{
	PropertyRNA *prop;

	static EnumPropertyItem mapping_items[] = {
		{0, "LINEAR", 0, "Linear", "Use linear mapping."},
		{LS_MODIFIER_USE_CURVE, "CURVE", 0, "Curve", "Use curve mapping."},
		{0, NULL, 0, NULL, NULL}};

	static EnumPropertyItem value_blend_items[] = {
		{LS_VALUE_BLEND, "MIX", 0, "Mix", ""},
		{LS_VALUE_ADD, "ADD", 0, "Add", ""},
		{LS_VALUE_SUB, "SUBTRACT", 0, "Subtract", ""},
		{LS_VALUE_MULT, "MULTIPLY", 0, "Multiply", ""},
		{LS_VALUE_DIV, "DIVIDE", 0, "Divide", ""},
		{LS_VALUE_DIFF, "DIFFERENCE", 0, "Divide", ""},
		{LS_VALUE_MIN, "MININUM", 0, "Minimum", ""}, 
		{LS_VALUE_MAX, "MAXIMUM", 0, "Maximum", ""}, 
		{0, NULL, 0, NULL, NULL}};

	prop= RNA_def_property(srna, "mapping", PROP_ENUM, PROP_NONE);
	RNA_def_property_enum_bitflag_sdna(prop, NULL, "flags");
	RNA_def_property_enum_items(prop, mapping_items);
	RNA_def_property_ui_text(prop, "Mapping", "Select the mapping type.");
	RNA_def_property_update(prop, NC_SCENE, NULL);

	prop= RNA_def_property(srna, "invert", PROP_BOOLEAN, PROP_NONE);
	RNA_def_property_boolean_sdna(prop, NULL, "flags", LS_MODIFIER_INVERT);
	RNA_def_property_ui_text(prop, "Invert", "Invert the fade-out direction of the linear mapping.");
	RNA_def_property_update(prop, NC_SCENE, NULL);

	prop = RNA_def_property(srna, "curve", PROP_POINTER, PROP_NONE);
	RNA_def_property_pointer_sdna(prop, NULL, "curve");
	RNA_def_property_struct_type(prop, "CurveMapping");
	RNA_def_property_ui_text(prop, "Curve", "Curve used for the curve mapping.");
	RNA_def_property_update(prop, NC_SCENE, NULL);

	prop= RNA_def_property(srna, "blend", PROP_ENUM, PROP_NONE);
	RNA_def_property_enum_sdna(prop, NULL, "blend");
	RNA_def_property_enum_items(prop, value_blend_items);
	RNA_def_property_ui_text(prop, "Blend", "Specify how the mapping value and property value are blended.");
	RNA_def_property_update(prop, NC_SCENE, NULL);

	if (range) {
		prop= RNA_def_property(srna, "range_min", PROP_FLOAT, PROP_NONE);
		RNA_def_property_float_sdna(prop, NULL, "range_min");
		RNA_def_property_ui_text(prop, "Range Min", "Lower bound of the input range the mapping is applied.");
		RNA_def_property_update(prop, NC_SCENE, NULL);

		prop= RNA_def_property(srna, "range_max", PROP_FLOAT, PROP_NONE);
		RNA_def_property_float_sdna(prop, NULL, "range_max");
		RNA_def_property_ui_text(prop, "Range Max", "Upper bound of the input range the mapping is applied.");
		RNA_def_property_update(prop, NC_SCENE, NULL);
	}

	if (value) {
		prop= RNA_def_property(srna, "value_min", PROP_FLOAT, PROP_NONE);
		RNA_def_property_float_sdna(prop, NULL, "value_min");
		RNA_def_property_ui_text(prop, "Value Min", "Minimum output value of the mapping.");
		RNA_def_property_update(prop, NC_SCENE, NULL);

		prop= RNA_def_property(srna, "value_max", PROP_FLOAT, PROP_NONE);
		RNA_def_property_float_sdna(prop, NULL, "value_max");
		RNA_def_property_ui_text(prop, "Value Max", "Maximum output value of the mapping.");
		RNA_def_property_update(prop, NC_SCENE, NULL);
	}
}

static void rna_def_linestyle_modifiers(BlenderRNA *brna)
{
	StructRNA *srna;
	PropertyRNA *prop;

	srna= RNA_def_struct(brna, "LineStyleModifier", NULL);
	RNA_def_struct_ui_text(srna, "Line Style Modifier", "Base type to define modifiers.");

	/* line color modifiers */

	srna= RNA_def_struct(brna, "LineStyleColorModifier", "LineStyleModifier");
	RNA_def_struct_sdna(srna, "LineStyleModifier");
	RNA_def_struct_refine_func(srna, "rna_LineStyle_color_modifier_refine");
	RNA_def_struct_path_func(srna, "rna_LineStyle_color_modifier_path");
	RNA_def_struct_ui_text(srna, "Line Style Color Modifier", "Base type to define line color modifiers.");

	srna= RNA_def_struct(brna, "LineStyleColorModifier_AlongStroke", "LineStyleColorModifier");
	RNA_def_struct_ui_text(srna, "Along Stroke", "Change line color along stroke.");
	rna_def_color_modifier(srna);
	rna_def_modifier_color_common(srna, 0);

	srna= RNA_def_struct(brna, "LineStyleColorModifier_DistanceFromCamera", "LineStyleColorModifier");
	RNA_def_struct_ui_text(srna, "Distance from Camera", "Change line color based on the distance from the camera.");
	rna_def_color_modifier(srna);
	rna_def_modifier_color_common(srna, 1);

	srna= RNA_def_struct(brna, "LineStyleColorModifier_DistanceFromObject", "LineStyleColorModifier");
	RNA_def_struct_ui_text(srna, "Distance from Object", "Change line color based on the distance from an object.");
	rna_def_color_modifier(srna);
	rna_def_modifier_color_common(srna, 1);

	prop= RNA_def_property(srna, "target", PROP_POINTER, PROP_NONE);
	RNA_def_property_pointer_sdna(prop, NULL, "target");
	RNA_def_property_struct_type(prop, "Object");
	RNA_def_property_flag(prop, PROP_EDITABLE);
	RNA_def_property_ui_text(prop, "Target", "Target object from which the distance is measured.");
	RNA_def_property_update(prop, NC_SCENE, NULL);

	/* alpha transparency modifiers */

	srna= RNA_def_struct(brna, "LineStyleAlphaModifier", "LineStyleModifier");
	RNA_def_struct_sdna(srna, "LineStyleModifier");
	RNA_def_struct_refine_func(srna, "rna_LineStyle_alpha_modifier_refine");
	RNA_def_struct_path_func(srna, "rna_LineStyle_alpha_modifier_path");
	RNA_def_struct_ui_text(srna, "Line Style Alpha Modifier", "Base type to define alpha transparency modifiers.");

	srna= RNA_def_struct(brna, "LineStyleAlphaModifier_AlongStroke", "LineStyleAlphaModifier");
	RNA_def_struct_ui_text(srna, "Along Stroke", "Change alpha transparency along stroke.");
	rna_def_alpha_modifier(srna);
	rna_def_modifier_curve_common(srna, 0, 0);

	srna= RNA_def_struct(brna, "LineStyleAlphaModifier_DistanceFromCamera", "LineStyleAlphaModifier");
	RNA_def_struct_ui_text(srna, "Distance from Camera", "Change alpha transparency based on the distance from the camera.");
	rna_def_alpha_modifier(srna);
	rna_def_modifier_curve_common(srna, 1, 0);

	srna= RNA_def_struct(brna, "LineStyleAlphaModifier_DistanceFromObject", "LineStyleAlphaModifier");
	RNA_def_struct_ui_text(srna, "Distance from Object", "Change alpha transparency based on the distance from an object.");
	rna_def_alpha_modifier(srna);
	rna_def_modifier_curve_common(srna, 1, 0);

	prop= RNA_def_property(srna, "target", PROP_POINTER, PROP_NONE);
	RNA_def_property_pointer_sdna(prop, NULL, "target");
	RNA_def_property_struct_type(prop, "Object");
	RNA_def_property_flag(prop, PROP_EDITABLE);
	RNA_def_property_ui_text(prop, "Target", "Target object from which the distance is measured.");
	RNA_def_property_update(prop, NC_SCENE, NULL);

	/* line thickness modifiers */

	srna= RNA_def_struct(brna, "LineStyleThicknessModifier", "LineStyleModifier");
	RNA_def_struct_sdna(srna, "LineStyleModifier");
	RNA_def_struct_refine_func(srna, "rna_LineStyle_thickness_modifier_refine");
	RNA_def_struct_path_func(srna, "rna_LineStyle_thickness_modifier_path");
	RNA_def_struct_ui_text(srna, "Line Style Thickness Modifier", "Base type to define line thickness modifiers.");

	srna= RNA_def_struct(brna, "LineStyleThicknessModifier_AlongStroke", "LineStyleThicknessModifier");
	RNA_def_struct_ui_text(srna, "Along Stroke", "Change line thickness along stroke.");
	rna_def_thickness_modifier(srna);
	rna_def_modifier_curve_common(srna, 0, 1);

	srna= RNA_def_struct(brna, "LineStyleThicknessModifier_DistanceFromCamera", "LineStyleThicknessModifier");
	RNA_def_struct_ui_text(srna, "Distance from Camera", "Change line thickness based on the distance from the camera.");
	rna_def_thickness_modifier(srna);
	rna_def_modifier_curve_common(srna, 1, 1);

	srna= RNA_def_struct(brna, "LineStyleThicknessModifier_DistanceFromObject", "LineStyleThicknessModifier");
	RNA_def_struct_ui_text(srna, "Distance from Object", "Change line thickness based on the distance from an object.");
	rna_def_thickness_modifier(srna);
	rna_def_modifier_curve_common(srna, 1, 1);

	prop= RNA_def_property(srna, "target", PROP_POINTER, PROP_NONE);
	RNA_def_property_pointer_sdna(prop, NULL, "target");
	RNA_def_property_struct_type(prop, "Object");
	RNA_def_property_flag(prop, PROP_EDITABLE);
	RNA_def_property_ui_text(prop, "Target", "Target object from which the distance is measured.");
	RNA_def_property_update(prop, NC_SCENE, NULL);

}

static void rna_def_linestyle(BlenderRNA *brna)
{
	StructRNA *srna;
	PropertyRNA *prop;

	static EnumPropertyItem panel_items[] = {
		{LS_PANEL_COLOR, "COLOR", 0, "Color", "Show the panel for line color options."},
		{LS_PANEL_ALPHA, "ALPHA", 0, "Alpha", "Show the panel for alpha transparency options."},
		{LS_PANEL_THICKNESS, "THICKNESS", 0, "Thickness", "Show the panel for line thickness options."},
		{LS_PANEL_STROKES, "STROKES", 0, "Strokes", "Show the panel for stroke construction."},
		{LS_PANEL_DISTORT, "DISTORT", 0, "Distort", "Show the panel for stroke distortion."},
		{LS_PANEL_MISC, "MISC", 0, "Misc", "Show the panel for miscellaneous options."},
		{0, NULL, 0, NULL, NULL}};

	srna= RNA_def_struct(brna, "FreestyleLineStyle", "ID");
	RNA_def_struct_ui_text(srna, "Freestyle Line Style", "Freestyle line style, reusable by multiple line sets");
	RNA_def_struct_ui_icon(srna, ICON_BRUSH_DATA); /* FIXME: use a proper icon */

	prop= RNA_def_property(srna, "panel", PROP_ENUM, PROP_NONE);
	RNA_def_property_enum_bitflag_sdna(prop, NULL, "panel");
	RNA_def_property_enum_items(prop, panel_items);
	RNA_def_property_ui_text(prop, "Panel", "Select the property panel to be shown.");
	RNA_def_property_update(prop, NC_SCENE, NULL);

	prop= RNA_def_property(srna, "color", PROP_FLOAT, PROP_COLOR);
	RNA_def_property_float_sdna(prop, NULL, "r");
	RNA_def_property_array(prop, 3);
	RNA_def_property_ui_text(prop, "Color", "Base line color, possibly modified by line color modifiers.");
	RNA_def_property_update(prop, NC_SCENE, NULL);

	prop= RNA_def_property(srna, "alpha", PROP_FLOAT, PROP_FACTOR);
	RNA_def_property_float_sdna(prop, NULL, "alpha");
	RNA_def_property_range(prop, 0.0f, 1.0f);
	RNA_def_property_ui_text(prop, "Alpha", "Base alpha transparency, possibly modified by alpha transparency modifiers.");
	RNA_def_property_update(prop, NC_SCENE, NULL);

	prop= RNA_def_property(srna, "thickness", PROP_FLOAT, PROP_NONE);
	RNA_def_property_float_sdna(prop, NULL, "thickness");
	RNA_def_property_range(prop, 0.0f, 10000.0f);
	RNA_def_property_ui_text(prop, "Thickness", "Base line thickness, possibly modified by line thickness modifiers.");
	RNA_def_property_update(prop, NC_SCENE, NULL);

	prop= RNA_def_property(srna, "color_modifiers", PROP_COLLECTION, PROP_NONE);
	RNA_def_property_collection_sdna(prop, NULL, "color_modifiers", NULL);
	RNA_def_property_struct_type(prop, "LineStyleColorModifier");
	RNA_def_property_ui_text(prop, "Color Modifiers", "List of line color modifiers.");

	prop= RNA_def_property(srna, "alpha_modifiers", PROP_COLLECTION, PROP_NONE);
	RNA_def_property_collection_sdna(prop, NULL, "alpha_modifiers", NULL);
	RNA_def_property_struct_type(prop, "LineStyleAlphaModifier");
	RNA_def_property_ui_text(prop, "Alpha Modifiers", "List of alpha trancparency modifiers.");

	prop= RNA_def_property(srna, "thickness_modifiers", PROP_COLLECTION, PROP_NONE);
	RNA_def_property_collection_sdna(prop, NULL, "thickness_modifiers", NULL);
	RNA_def_property_struct_type(prop, "LineStyleThicknessModifier");
	RNA_def_property_ui_text(prop, "Thickness Modifiers", "List of line thickness modifiers.");

}

void RNA_def_linestyle(BlenderRNA *brna)
{
	rna_def_linestyle_modifiers(brna);
	rna_def_linestyle(brna);
}

#endif
