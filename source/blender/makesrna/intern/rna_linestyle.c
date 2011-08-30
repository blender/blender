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
	{LS_MODIFIER_MATERIAL, "MATERIAL", ICON_MODIFIER, "Material", ""},
	{0, NULL, 0, NULL, NULL}};

EnumPropertyItem linestyle_alpha_modifier_type_items[] ={
	{LS_MODIFIER_ALONG_STROKE, "ALONG_STROKE", ICON_MODIFIER, "Along Stroke", ""},
	{LS_MODIFIER_DISTANCE_FROM_CAMERA, "DISTANCE_FROM_CAMERA", ICON_MODIFIER, "Distance from Camera", ""},
	{LS_MODIFIER_DISTANCE_FROM_OBJECT, "DISTANCE_FROM_OBJECT", ICON_MODIFIER, "Distance from Object", ""},
	{LS_MODIFIER_MATERIAL, "MATERIAL", ICON_MODIFIER, "Material", ""},
	{0, NULL, 0, NULL, NULL}};

EnumPropertyItem linestyle_thickness_modifier_type_items[] ={
	{LS_MODIFIER_ALONG_STROKE, "ALONG_STROKE", ICON_MODIFIER, "Along Stroke", ""},
	{LS_MODIFIER_DISTANCE_FROM_CAMERA, "DISTANCE_FROM_CAMERA", ICON_MODIFIER, "Distance from Camera", ""},
	{LS_MODIFIER_DISTANCE_FROM_OBJECT, "DISTANCE_FROM_OBJECT", ICON_MODIFIER, "Distance from Object", ""},
	{LS_MODIFIER_MATERIAL, "MATERIAL", ICON_MODIFIER, "Material", ""},
	{0, NULL, 0, NULL, NULL}};

EnumPropertyItem linestyle_geometry_modifier_type_items[] ={
	{LS_MODIFIER_SAMPLING, "SAMPLING", ICON_MODIFIER, "Sampling", ""},
	{LS_MODIFIER_BEZIER_CURVE, "BEZIER_CURVE", ICON_MODIFIER, "Bezier Curve", ""},
	{LS_MODIFIER_SINUS_DISPLACEMENT, "SINUS_DISPLACEMENT", ICON_MODIFIER, "Sinus Displacement", ""},
	{LS_MODIFIER_SPATIAL_NOISE, "SPATIAL_NOISE", ICON_MODIFIER, "Spatial Noise", ""},
	{LS_MODIFIER_PERLIN_NOISE_1D, "PERLIN_NOISE_1D", ICON_MODIFIER, "Perlin Noise 1D", ""},
	{LS_MODIFIER_PERLIN_NOISE_2D, "PERLIN_NOISE_2D", ICON_MODIFIER, "Perlin Noise 2D", ""},
	{LS_MODIFIER_BACKBONE_STRETCHER, "BACKBONE_STRETCHER", ICON_MODIFIER, "Backbone Stretcher", ""},
	{LS_MODIFIER_TIP_REMOVER, "TIP_REMOVER", ICON_MODIFIER, "Tip Remover", ""},
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
		case LS_MODIFIER_MATERIAL:
			return &RNA_LineStyleColorModifier_Material;
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
		case LS_MODIFIER_MATERIAL:
			return &RNA_LineStyleAlphaModifier_Material;
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
		case LS_MODIFIER_MATERIAL:
			return &RNA_LineStyleThicknessModifier_Material;
		default:
			return &RNA_LineStyleThicknessModifier;
	}
}

static StructRNA *rna_LineStyle_geometry_modifier_refine(struct PointerRNA *ptr)
{
	LineStyleModifier *m = (LineStyleModifier *)ptr->data;

	switch(m->type) {
		case LS_MODIFIER_SAMPLING:
			return &RNA_LineStyleGeometryModifier_Sampling;
		case LS_MODIFIER_BEZIER_CURVE:
			return &RNA_LineStyleGeometryModifier_BezierCurve;
		case LS_MODIFIER_SINUS_DISPLACEMENT:
			return &RNA_LineStyleGeometryModifier_SinusDisplacement;
		case LS_MODIFIER_SPATIAL_NOISE:
			return &RNA_LineStyleGeometryModifier_SpatialNoise;
		case LS_MODIFIER_PERLIN_NOISE_1D:
			return &RNA_LineStyleGeometryModifier_PerlinNoise1D;
		case LS_MODIFIER_PERLIN_NOISE_2D:
			return &RNA_LineStyleGeometryModifier_PerlinNoise2D;
		case LS_MODIFIER_BACKBONE_STRETCHER:
			return &RNA_LineStyleGeometryModifier_BackboneStretcher;
		case LS_MODIFIER_TIP_REMOVER:
			return &RNA_LineStyleGeometryModifier_TipRemover;
		default:
			return &RNA_LineStyleGeometryModifier;
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

static char *rna_LineStyle_geometry_modifier_path(PointerRNA *ptr)
{
	return BLI_sprintfN("geometry_modifiers[\"%s\"]", ((LineStyleModifier*)ptr->data)->name);
}

#else

#include "DNA_material_types.h"

static void rna_def_modifier_type_common(StructRNA *srna, EnumPropertyItem *modifier_type_items, int blend, int color)
{
	PropertyRNA *prop;

	static EnumPropertyItem color_blend_items[] = {
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

	if (blend) {
		prop= RNA_def_property(srna, "blend", PROP_ENUM, PROP_NONE);
		RNA_def_property_enum_sdna(prop, NULL, "modifier.blend");
		RNA_def_property_enum_items(prop, (color) ? color_blend_items : value_blend_items);
		RNA_def_property_ui_text(prop, "Blend", "Specify how the modifier value is blended into the base value.");
		RNA_def_property_update(prop, NC_SCENE, NULL);

		prop= RNA_def_property(srna, "influence", PROP_FLOAT, PROP_FACTOR);
		RNA_def_property_float_sdna(prop, NULL, "modifier.influence");
		RNA_def_property_range(prop, 0.0f, 1.0f);
		RNA_def_property_ui_text(prop, "Influence", "Influence factor by which the modifier changes the property.");
		RNA_def_property_update(prop, NC_SCENE, NULL);
	}

	prop= RNA_def_property(srna, "use", PROP_BOOLEAN, PROP_NONE);
	RNA_def_property_boolean_sdna(prop, NULL, "modifier.flags", LS_MODIFIER_ENABLED);
	RNA_def_property_ui_text(prop, "Use", "Enable or disable this modifier during stroke rendering.");

	prop= RNA_def_property(srna, "expanded", PROP_BOOLEAN, PROP_NONE);
	RNA_def_property_boolean_sdna(prop, NULL, "modifier.flags", LS_MODIFIER_EXPANDED);
	RNA_def_property_ui_text(prop, "Expanded", "True if the modifier tab is expanded.");
}

static void rna_def_color_modifier(StructRNA *srna)
{
	rna_def_modifier_type_common(srna, linestyle_color_modifier_type_items, 1, 1);
}

static void rna_def_alpha_modifier(StructRNA *srna)
{
	rna_def_modifier_type_common(srna, linestyle_alpha_modifier_type_items, 1, 0);
}

static void rna_def_thickness_modifier(StructRNA *srna)
{
	rna_def_modifier_type_common(srna, linestyle_thickness_modifier_type_items, 1, 0);
}

static void rna_def_geometry_modifier(StructRNA *srna)
{
	rna_def_modifier_type_common(srna, linestyle_geometry_modifier_type_items, 0, 0);
}

static void rna_def_modifier_color_ramp_common(StructRNA *srna, int range)
{
	PropertyRNA *prop;

	prop= RNA_def_property(srna, "color_ramp", PROP_POINTER, PROP_NONE);
	RNA_def_property_pointer_sdna(prop, NULL, "color_ramp");
	RNA_def_property_struct_type(prop, "ColorRamp");
	RNA_def_property_ui_text(prop, "Color Ramp", "Color ramp used to change line color.");
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

static void rna_def_modifier_material_common(StructRNA *srna)
{
	PropertyRNA *prop;

	static EnumPropertyItem mat_attr_items[] = {
		{LS_MODIFIER_MATERIAL_DIFF, "DIFF", 0, "Diffuse", ""},
		{LS_MODIFIER_MATERIAL_DIFF_R, "DIFF_R", 0, "Diffuse Red", ""},
		{LS_MODIFIER_MATERIAL_DIFF_G, "DIFF_G", 0, "Diffuse Green", ""},
		{LS_MODIFIER_MATERIAL_DIFF_B, "DIFF_B", 0, "Diffuse Blue", ""},
		{LS_MODIFIER_MATERIAL_SPEC, "SPEC", 0, "Specular", ""},
		{LS_MODIFIER_MATERIAL_SPEC_R, "SPEC_R", 0, "Specular Red", ""},
		{LS_MODIFIER_MATERIAL_SPEC_G, "SPEC_G", 0, "Specular Green", ""},
		{LS_MODIFIER_MATERIAL_SPEC_B, "SPEC_B", 0, "Specular Blue", ""},
		{LS_MODIFIER_MATERIAL_SPEC_HARD, "SPEC_HARD", 0, "Specular Hardness", ""},
		{LS_MODIFIER_MATERIAL_ALPHA, "ALPHA", 0, "Alpha", ""},
		{0, NULL, 0, NULL, NULL}};

	prop= RNA_def_property(srna, "material_attr", PROP_ENUM, PROP_NONE);
	RNA_def_property_enum_sdna(prop, NULL, "mat_attr");
	RNA_def_property_enum_items(prop, mat_attr_items);
	RNA_def_property_ui_text(prop, "Material Attribute", "Specify which material attribute is used.");
	RNA_def_property_update(prop, NC_SCENE, NULL);

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
	rna_def_modifier_color_ramp_common(srna, 0);

	srna= RNA_def_struct(brna, "LineStyleColorModifier_DistanceFromCamera", "LineStyleColorModifier");
	RNA_def_struct_ui_text(srna, "Distance from Camera", "Change line color based on the distance from the camera.");
	rna_def_color_modifier(srna);
	rna_def_modifier_color_ramp_common(srna, 1);

	srna= RNA_def_struct(brna, "LineStyleColorModifier_DistanceFromObject", "LineStyleColorModifier");
	RNA_def_struct_ui_text(srna, "Distance from Object", "Change line color based on the distance from an object.");
	rna_def_color_modifier(srna);
	rna_def_modifier_color_ramp_common(srna, 1);

	prop= RNA_def_property(srna, "target", PROP_POINTER, PROP_NONE);
	RNA_def_property_pointer_sdna(prop, NULL, "target");
	RNA_def_property_struct_type(prop, "Object");
	RNA_def_property_flag(prop, PROP_EDITABLE);
	RNA_def_property_ui_text(prop, "Target", "Target object from which the distance is measured.");
	RNA_def_property_update(prop, NC_SCENE, NULL);

	srna= RNA_def_struct(brna, "LineStyleColorModifier_Material", "LineStyleColorModifier");
	RNA_def_struct_ui_text(srna, "Material", "Change line color based on a material attribute.");
	rna_def_color_modifier(srna);
	rna_def_modifier_material_common(srna);
	rna_def_modifier_color_ramp_common(srna, 0);

	prop= RNA_def_property(srna, "use_ramp", PROP_BOOLEAN, PROP_NONE);
	RNA_def_property_boolean_sdna(prop, NULL, "flags", LS_MODIFIER_USE_RAMP);
	RNA_def_property_ui_text(prop, "Ramp", "Use color ramp to map the BW average into an RGB color.");
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

	srna= RNA_def_struct(brna, "LineStyleAlphaModifier_Material", "LineStyleAlphaModifier");
	RNA_def_struct_ui_text(srna, "Material", "Change alpha transparency based on a material attribute.");
	rna_def_alpha_modifier(srna);
	rna_def_modifier_material_common(srna);
	rna_def_modifier_curve_common(srna, 0, 0);

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

	srna= RNA_def_struct(brna, "LineStyleThicknessModifier_Material", "LineStyleThicknessModifier");
	RNA_def_struct_ui_text(srna, "Material", "Change line thickness based on a material attribute.");
	rna_def_thickness_modifier(srna);
	rna_def_modifier_material_common(srna);
	rna_def_modifier_curve_common(srna, 0, 1);

	/* geometry modifiers */

	srna= RNA_def_struct(brna, "LineStyleGeometryModifier", "LineStyleModifier");
	RNA_def_struct_sdna(srna, "LineStyleModifier");
	RNA_def_struct_refine_func(srna, "rna_LineStyle_geometry_modifier_refine");
	RNA_def_struct_path_func(srna, "rna_LineStyle_geometry_modifier_path");
	RNA_def_struct_ui_text(srna, "Line Style Geometry Modifier", "Base type to define stroke geometry modifiers.");

	srna= RNA_def_struct(brna, "LineStyleGeometryModifier_Sampling", "LineStyleGeometryModifier");
	RNA_def_struct_ui_text(srna, "Sampling", "Specify a new sampling value that determines the resolution of stroke polylines.");
	rna_def_geometry_modifier(srna);

	prop= RNA_def_property(srna, "sampling", PROP_FLOAT, PROP_NONE);
	RNA_def_property_float_sdna(prop, NULL, "sampling");
	RNA_def_property_ui_text(prop, "Sampling", "New sampling value to be used for subsequent modifiers.");
	RNA_def_property_update(prop, NC_SCENE, NULL);

	srna= RNA_def_struct(brna, "LineStyleGeometryModifier_BezierCurve", "LineStyleGeometryModifier");
	RNA_def_struct_ui_text(srna, "Bezier Curve", "Replace stroke backbone geometry by a Bezier curve approximation of the original backbone geometry.");
	rna_def_geometry_modifier(srna);

	prop= RNA_def_property(srna, "error", PROP_FLOAT, PROP_NONE);
	RNA_def_property_float_sdna(prop, NULL, "error");
	RNA_def_property_ui_text(prop, "Error", "Maximum distance allowed between the new Bezier curve and the original backbone geometry).");
	RNA_def_property_update(prop, NC_SCENE, NULL);

	srna= RNA_def_struct(brna, "LineStyleGeometryModifier_SinusDisplacement", "LineStyleGeometryModifier");
	RNA_def_struct_ui_text(srna, "Sinus Displacement", "Add sinus displacement to stroke backbone geometry.");
	rna_def_geometry_modifier(srna);

	prop= RNA_def_property(srna, "wavelength", PROP_FLOAT, PROP_NONE);
	RNA_def_property_float_sdna(prop, NULL, "wavelength");
	RNA_def_property_ui_text(prop, "Wavelength", "Wavelength of the sinus displacement.");
	RNA_def_property_update(prop, NC_SCENE, NULL);

	prop= RNA_def_property(srna, "amplitude", PROP_FLOAT, PROP_NONE);
	RNA_def_property_float_sdna(prop, NULL, "amplitude");
	RNA_def_property_ui_text(prop, "Amplitude", "Amplitude of the sinus displacement.");
	RNA_def_property_update(prop, NC_SCENE, NULL);

	prop= RNA_def_property(srna, "phase", PROP_FLOAT, PROP_NONE);
	RNA_def_property_float_sdna(prop, NULL, "phase");
	RNA_def_property_ui_text(prop, "Phase", "Phase of the sinus displacement.");
	RNA_def_property_update(prop, NC_SCENE, NULL);

	srna= RNA_def_struct(brna, "LineStyleGeometryModifier_SpatialNoise", "LineStyleGeometryModifier");
	RNA_def_struct_ui_text(srna, "Spatial Noise", "Add spatial noise to stroke backbone geometry.");
	rna_def_geometry_modifier(srna);

	prop= RNA_def_property(srna, "amplitude", PROP_FLOAT, PROP_NONE);
	RNA_def_property_float_sdna(prop, NULL, "amplitude");
	RNA_def_property_ui_text(prop, "Amplitude", "Amplitude of the spatial noise.");
	RNA_def_property_update(prop, NC_SCENE, NULL);

	prop= RNA_def_property(srna, "scale", PROP_FLOAT, PROP_NONE);
	RNA_def_property_float_sdna(prop, NULL, "scale");
	RNA_def_property_ui_text(prop, "Scale", "Scale of the spatial noise.");
	RNA_def_property_update(prop, NC_SCENE, NULL);

	prop= RNA_def_property(srna, "octaves", PROP_INT, PROP_UNSIGNED);
	RNA_def_property_int_sdna(prop, NULL, "octaves");
	RNA_def_property_ui_text(prop, "Octaves", "Number of octaves (i.e., the amount of detail of the spatial noise).");
	RNA_def_property_update(prop, NC_SCENE, NULL);

	prop= RNA_def_property(srna, "smooth", PROP_BOOLEAN, PROP_NONE);
	RNA_def_property_boolean_sdna(prop, NULL, "flags", LS_MODIFIER_SPATIAL_NOISE_SMOOTH);
	RNA_def_property_ui_text(prop, "Smooth", "If true, the spatial noise is smooth.");
	RNA_def_property_update(prop, NC_SCENE, NULL);

	prop= RNA_def_property(srna, "pure_random", PROP_BOOLEAN, PROP_NONE);
	RNA_def_property_boolean_sdna(prop, NULL, "flags", LS_MODIFIER_SPATIAL_NOISE_PURERANDOM);
	RNA_def_property_ui_text(prop, "Pure Random", "If true, the spatial noise does not show any coherence.");
	RNA_def_property_update(prop, NC_SCENE, NULL);

	srna= RNA_def_struct(brna, "LineStyleGeometryModifier_PerlinNoise1D", "LineStyleGeometryModifier");
	RNA_def_struct_ui_text(srna, "Perlin Noise 1D", "Add one-dimensional Perlin noise to stroke backbone geometry.");
	rna_def_geometry_modifier(srna);

	prop= RNA_def_property(srna, "frequency", PROP_FLOAT, PROP_NONE);
	RNA_def_property_float_sdna(prop, NULL, "frequency");
	RNA_def_property_ui_text(prop, "Frequency", "Frequency of the Perlin noise.");
	RNA_def_property_update(prop, NC_SCENE, NULL);

	prop= RNA_def_property(srna, "amplitude", PROP_FLOAT, PROP_NONE);
	RNA_def_property_float_sdna(prop, NULL, "amplitude");
	RNA_def_property_ui_text(prop, "Amplitude", "Amplitude of the Perlin noise.");
	RNA_def_property_update(prop, NC_SCENE, NULL);

	prop= RNA_def_property(srna, "octaves", PROP_INT, PROP_UNSIGNED);
	RNA_def_property_int_sdna(prop, NULL, "octaves");
	RNA_def_property_ui_text(prop, "Octaves", "Number of octaves (i.e., the amount of detail of the Perlin noise).");
	RNA_def_property_update(prop, NC_SCENE, NULL);

	prop= RNA_def_property(srna, "angle", PROP_FLOAT, PROP_NONE);
	RNA_def_property_float_sdna(prop, NULL, "angle");
	RNA_def_property_ui_text(prop, "Angle", "Displacement direction in degrees.");
	RNA_def_property_update(prop, NC_SCENE, NULL);

	prop= RNA_def_property(srna, "seed", PROP_INT, PROP_NONE);
	RNA_def_property_int_sdna(prop, NULL, "seed");
	RNA_def_property_ui_text(prop, "Seed", "Seed for random number generation.  If negative, time is used as a seed instead.");
	RNA_def_property_update(prop, NC_SCENE, NULL);

	srna= RNA_def_struct(brna, "LineStyleGeometryModifier_PerlinNoise2D", "LineStyleGeometryModifier");
	RNA_def_struct_ui_text(srna, "Perlin Noise 2D", "Add two-dimensional Perlin noise to stroke backbone geometry.");
	rna_def_geometry_modifier(srna);

	prop= RNA_def_property(srna, "frequency", PROP_FLOAT, PROP_NONE);
	RNA_def_property_float_sdna(prop, NULL, "frequency");
	RNA_def_property_ui_text(prop, "Frequency", "Frequency of the Perlin noise.");
	RNA_def_property_update(prop, NC_SCENE, NULL);

	prop= RNA_def_property(srna, "amplitude", PROP_FLOAT, PROP_NONE);
	RNA_def_property_float_sdna(prop, NULL, "amplitude");
	RNA_def_property_ui_text(prop, "Amplitude", "Amplitude of the Perlin noise.");
	RNA_def_property_update(prop, NC_SCENE, NULL);

	prop= RNA_def_property(srna, "octaves", PROP_INT, PROP_UNSIGNED);
	RNA_def_property_int_sdna(prop, NULL, "octaves");
	RNA_def_property_ui_text(prop, "Octaves", "Number of octaves (i.e., the amount of detail of the Perlin noise).");
	RNA_def_property_update(prop, NC_SCENE, NULL);

	prop= RNA_def_property(srna, "angle", PROP_FLOAT, PROP_NONE);
	RNA_def_property_float_sdna(prop, NULL, "angle");
	RNA_def_property_ui_text(prop, "Angle", "Displacement direction in degrees.");
	RNA_def_property_update(prop, NC_SCENE, NULL);

	prop= RNA_def_property(srna, "seed", PROP_INT, PROP_NONE);
	RNA_def_property_int_sdna(prop, NULL, "seed");
	RNA_def_property_ui_text(prop, "Seed", "Seed for random number generation.  If negative, time is used as a seed instead.");
	RNA_def_property_update(prop, NC_SCENE, NULL);

	srna= RNA_def_struct(brna, "LineStyleGeometryModifier_BackboneStretcher", "LineStyleGeometryModifier");
	RNA_def_struct_ui_text(srna, "Backbone Stretcher", "Stretch the beginning and the end of stroke backbone.");
	rna_def_geometry_modifier(srna);

	prop= RNA_def_property(srna, "amount", PROP_FLOAT, PROP_NONE);
	RNA_def_property_float_sdna(prop, NULL, "amount");
	RNA_def_property_ui_text(prop, "Amount", "Amount of stretching.");
	RNA_def_property_update(prop, NC_SCENE, NULL);

	srna= RNA_def_struct(brna, "LineStyleGeometryModifier_TipRemover", "LineStyleGeometryModifier");
	RNA_def_struct_ui_text(srna, "Tip Remover", "Remove a piece of stroke at the beginning and the end of stroke backbone.");
	rna_def_geometry_modifier(srna);

	prop= RNA_def_property(srna, "tip_length", PROP_FLOAT, PROP_NONE);
	RNA_def_property_float_sdna(prop, NULL, "tip_length");
	RNA_def_property_ui_text(prop, "Tip Length", "Length of tips to be removed.");
	RNA_def_property_update(prop, NC_SCENE, NULL);

}

static void rna_def_linestyle(BlenderRNA *brna)
{
	StructRNA *srna;
	PropertyRNA *prop;

	static EnumPropertyItem panel_items[] = {
		{LS_PANEL_STROKES, "STROKES", 0, "Strokes", "Show the panel for stroke construction."},
		{LS_PANEL_COLOR, "COLOR", 0, "Color", "Show the panel for line color options."},
		{LS_PANEL_ALPHA, "ALPHA", 0, "Alpha", "Show the panel for alpha transparency options."},
		{LS_PANEL_THICKNESS, "THICKNESS", 0, "Thickness", "Show the panel for line thickness options."},
		{LS_PANEL_GEOMETRY, "GEOMETRY", 0, "Geometry", "Show the panel for stroke geometry options."},
		{LS_PANEL_MISC, "MISC", 0, "Misc", "Show the panel for miscellaneous options."},
		{0, NULL, 0, NULL, NULL}};
	static EnumPropertyItem cap_items[] = {
		{LS_CAPS_BUTT, "BUTT", 0, "Butt", "Butt cap (flat)."},
		{LS_CAPS_ROUND, "ROUND", 0, "Round", "Round cap (half-circle)."},
		{LS_CAPS_SQUARE, "SQUARE", 0, "Square", "Square cap (flat and extended)."},
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

	prop= RNA_def_property(srna, "geometry_modifiers", PROP_COLLECTION, PROP_NONE);
	RNA_def_property_collection_sdna(prop, NULL, "geometry_modifiers", NULL);
	RNA_def_property_struct_type(prop, "LineStyleGeometryModifier");
	RNA_def_property_ui_text(prop, "Geometry Modifiers", "List of stroke geometry modifiers.");

	prop= RNA_def_property(srna, "same_object", PROP_BOOLEAN, PROP_NONE);
	RNA_def_property_boolean_sdna(prop, NULL, "flag", LS_SAME_OBJECT);
	RNA_def_property_ui_text(prop, "Same Object", "If true, only feature edges of the same object are joined.");
	RNA_def_property_update(prop, NC_SCENE, NULL);

	prop= RNA_def_property(srna, "material_boundary", PROP_BOOLEAN, PROP_NONE);
	RNA_def_property_boolean_sdna(prop, NULL, "flag", LS_MATERIAL_BOUNDARY);
	RNA_def_property_ui_text(prop, "Material Boundary", "If true, chains of feature edges are split at material boundaries.");
	RNA_def_property_update(prop, NC_SCENE, NULL);

	prop= RNA_def_property(srna, "use_dashed_line", PROP_BOOLEAN, PROP_NONE);
	RNA_def_property_boolean_sdna(prop, NULL, "flag", LS_DASHED_LINE);
	RNA_def_property_ui_text(prop, "Dashed Line", "Enable or disable dashed line.");
	RNA_def_property_update(prop, NC_SCENE, NULL);

	prop= RNA_def_property(srna, "caps", PROP_ENUM, PROP_NONE);
	RNA_def_property_enum_bitflag_sdna(prop, NULL, "caps");
	RNA_def_property_enum_items(prop, cap_items);
	RNA_def_property_ui_text(prop, "Cap", "Select the shape of both ends of strokes.");
	RNA_def_property_update(prop, NC_SCENE, NULL);

	prop= RNA_def_property(srna, "dash1", PROP_INT, PROP_UNSIGNED);
	RNA_def_property_int_sdna(prop, NULL, "dash1");
	RNA_def_property_range(prop, 0, USHRT_MAX);
	RNA_def_property_ui_text(prop, "Dash #1", "Length of the 1st dash.");
	RNA_def_property_update(prop, NC_SCENE, NULL);

	prop= RNA_def_property(srna, "gap1", PROP_INT, PROP_UNSIGNED);
	RNA_def_property_int_sdna(prop, NULL, "gap1");
	RNA_def_property_range(prop, 0, USHRT_MAX);
	RNA_def_property_ui_text(prop, "Gap #1", "Length of the 1st gap.");
	RNA_def_property_update(prop, NC_SCENE, NULL);

	prop= RNA_def_property(srna, "dash2", PROP_INT, PROP_UNSIGNED);
	RNA_def_property_int_sdna(prop, NULL, "dash2");
	RNA_def_property_range(prop, 0, USHRT_MAX);
	RNA_def_property_ui_text(prop, "Dash #2", "Length of the 2nd dash.");
	RNA_def_property_update(prop, NC_SCENE, NULL);

	prop= RNA_def_property(srna, "gap2", PROP_INT, PROP_UNSIGNED);
	RNA_def_property_int_sdna(prop, NULL, "gap2");
	RNA_def_property_range(prop, 0, USHRT_MAX);
	RNA_def_property_ui_text(prop, "Gap #2", "Length of the 2nd gap.");
	RNA_def_property_update(prop, NC_SCENE, NULL);

	prop= RNA_def_property(srna, "dash3", PROP_INT, PROP_UNSIGNED);
	RNA_def_property_int_sdna(prop, NULL, "dash3");
	RNA_def_property_range(prop, 0, USHRT_MAX);
	RNA_def_property_ui_text(prop, "Dash #3", "Length of the 3rd dash.");
	RNA_def_property_update(prop, NC_SCENE, NULL);

	prop= RNA_def_property(srna, "gap3", PROP_INT, PROP_UNSIGNED);
	RNA_def_property_int_sdna(prop, NULL, "gap3");
	RNA_def_property_range(prop, 0, USHRT_MAX);
	RNA_def_property_ui_text(prop, "Gap #3", "Length of the 3rd gap.");
	RNA_def_property_update(prop, NC_SCENE, NULL);

}

void RNA_def_linestyle(BlenderRNA *brna)
{
	rna_def_linestyle_modifiers(brna);
	rna_def_linestyle(brna);
}

#endif
