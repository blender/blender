/**
 * ***** BEGIN GPL LICENSE BLOCK *****
 *
 * This program is free software; you can redistribute it and/or
 * modify it under the terms of the GNU General Public License
 * as published by the Free Software Foundation; either version 2
 * of the License, or (at your option) any later version.
 *
 * Contributor(s): Miika Hämäläinen
 *
 * ***** END GPL LICENSE BLOCK *****
 */

#include <stdlib.h>
#include <limits.h>

#include "RNA_define.h"

#include "rna_internal.h"

#include "BKE_modifier.h"
#include "BKE_dynamicpaint.h"

#include "DNA_modifier_types.h"
#include "DNA_object_types.h"
#include "DNA_scene_types.h"
#include "DNA_dynamicpaint_types.h"

#include "WM_types.h"


#ifdef RNA_RUNTIME

#include "BKE_context.h"
#include "BKE_depsgraph.h"
#include "BKE_particle.h"


static char *rna_DynamicPaintCanvasSettings_path(PointerRNA *ptr)
{
	DynamicPaintCanvasSettings *settings = (DynamicPaintCanvasSettings*)ptr->data;
	ModifierData *md= (ModifierData *)settings->pmd;

	return BLI_sprintfN("modifiers[\"%s\"].canvas_settings", md->name);
}

static char *rna_DynamicPaintPainterSettings_path(PointerRNA *ptr)
{
	DynamicPaintPainterSettings *settings = (DynamicPaintPainterSettings*)ptr->data;
	ModifierData *md= (ModifierData *)settings->pmd;

	return BLI_sprintfN("modifiers[\"%s\"].paint_settings", md->name);
}

static void rna_DynamicPaint_uvlayer_set(PointerRNA *ptr, const char *value)
{
	DynamicPaintCanvasSettings *canvas= (DynamicPaintCanvasSettings*)ptr->data;
	rna_object_uvlayer_name_set(ptr, value, canvas->uvlayer_name, sizeof(canvas->uvlayer_name));
}


#else

static void rna_def_dynamic_paint_canvas_settings(BlenderRNA *brna)
{
	StructRNA *srna;
	PropertyRNA *prop;

	/*
	*   Effect type
	*   Only used by ui to view per effect settings
	*/
	static EnumPropertyItem prop_dynamicpaint_effecttype[] = {
			{1, "SPREAD", 0, "Spread", ""},
			{2, "DRIP", 0, "Drip", ""},
			{3, "SHRINK", 0, "Shrink", ""},
			{0, NULL, 0, NULL, NULL}};

	/*
	*   Displacemap file format
	*/
	static EnumPropertyItem prop_dynamicpaint_disp_format[] = {
			{MOD_DPAINT_DISPFOR_PNG, "PNG", 0, "PNG", ""},
#ifdef WITH_OPENEXR
			{MOD_DPAINT_DISPFOR_OPENEXR, "OPENEXR", 0, "OpenEXR", ""},
#endif
			{0, NULL, 0, NULL, NULL}};

	/*
	*   Displacemap type
	*/
	static EnumPropertyItem prop_dynamicpaint_disp_type[] = {
			{MOD_DPAINT_DISP_DISPLACE, "DISPLACE", 0, "Displacement", ""},
			{MOD_DPAINT_DISP_DEPTH, "DEPTH", 0, "Depth", ""},
			{0, NULL, 0, NULL, NULL}};

	srna = RNA_def_struct(brna, "DynamicPaintCanvasSettings", NULL);
	RNA_def_struct_ui_text(srna, "Canvas Settings", "Dynamic Paint canvas settings");
	RNA_def_struct_sdna(srna, "DynamicPaintCanvasSettings");
	RNA_def_struct_path_func(srna, "rna_DynamicPaintCanvasSettings_path");

	/*
	*   Paint, wet and disp
	*/
	prop= RNA_def_property(srna, "use_dissolve_paint", PROP_BOOLEAN, PROP_NONE);
	RNA_def_property_boolean_sdna(prop, NULL, "flags", MOD_DPAINT_DISSOLVE);
	RNA_def_property_ui_text(prop, "Dissolve Paint", "Enable paint to disappear over time.");
	
	prop= RNA_def_property(srna, "dissolve_speed", PROP_INT, PROP_NONE);
	RNA_def_property_int_sdna(prop, NULL, "diss_speed");
	RNA_def_property_range(prop, 1.0, 10000.0);
	RNA_def_property_ui_range(prop, 1.0, 10000.0, 5, 0);
	RNA_def_property_ui_text(prop, "Dissolve Speed", "Dissolve Speed");
	
	prop= RNA_def_property(srna, "use_flatten_disp", PROP_BOOLEAN, PROP_NONE);
	RNA_def_property_boolean_sdna(prop, NULL, "flags", MOD_DPAINT_FLATTEN);
	RNA_def_property_ui_text(prop, "Time Flatten", "Makes displacement map to flatten over time.");
	
	prop= RNA_def_property(srna, "flatten_speed", PROP_INT, PROP_NONE);
	RNA_def_property_int_sdna(prop, NULL, "dflat_speed");
	RNA_def_property_range(prop, 1.0, 10000.0);
	RNA_def_property_ui_range(prop, 1.0, 10000.0, 5, 0);
	RNA_def_property_ui_text(prop, "Flatten Speed", "Flatten Speed");
	
	prop= RNA_def_property(srna, "dry_speed", PROP_INT, PROP_NONE);
	RNA_def_property_int_sdna(prop, NULL, "dry_speed");
	RNA_def_property_range(prop, 1.0, 10000.0);
	RNA_def_property_ui_range(prop, 1.0, 10000.0, 5, 0);
	RNA_def_property_ui_text(prop, "Dry Speed", "Dry Speed");
	
	/*
	*   Simulation settings
	*/
	prop= RNA_def_property(srna, "resolution", PROP_INT, PROP_NONE);
	RNA_def_property_clear_flag(prop, PROP_ANIMATABLE);
	RNA_def_property_int_sdna(prop, NULL, "resolution");
	RNA_def_property_range(prop, 16.0, 4096.0);
	RNA_def_property_ui_range(prop, 16.0, 4096.0, 1, 0);
	RNA_def_property_ui_text(prop, "Resolution", "Texture resolution");
	
	prop= RNA_def_property(srna, "uv_layer", PROP_STRING, PROP_NONE);
	RNA_def_property_string_sdna(prop, NULL, "uvlayer_name");
	RNA_def_property_ui_text(prop, "UV Layer", "UV layer name");
	RNA_def_property_string_funcs(prop, NULL, NULL, "rna_DynamicPaint_uvlayer_set");
	
	prop= RNA_def_property(srna, "start_frame", PROP_INT, PROP_NONE);
	RNA_def_property_clear_flag(prop, PROP_ANIMATABLE);
	RNA_def_property_int_sdna(prop, NULL, "start_frame");
	RNA_def_property_range(prop, 1.0, 9999.0);
	RNA_def_property_ui_range(prop, 1.0, 9999, 1, 0);
	RNA_def_property_ui_text(prop, "Start Frame", "Simulation start frame");
	
	prop= RNA_def_property(srna, "end_frame", PROP_INT, PROP_NONE);
	RNA_def_property_clear_flag(prop, PROP_ANIMATABLE);
	RNA_def_property_int_sdna(prop, NULL, "end_frame");
	RNA_def_property_range(prop, 1.0, 9999.0);
	RNA_def_property_ui_range(prop, 1.0, 9999.0, 1, 0);
	RNA_def_property_ui_text(prop, "End Frame", "Simulation end frame");
	
	prop= RNA_def_property(srna, "substeps", PROP_INT, PROP_NONE);
	RNA_def_property_int_sdna(prop, NULL, "substeps");
	RNA_def_property_range(prop, 0.0, 10.0);
	RNA_def_property_ui_range(prop, 0.0, 10, 1, 0);
	RNA_def_property_ui_text(prop, "Sub-Steps", "Do extra frames between scene frames to ensure smooth motion.");
	
	prop= RNA_def_property(srna, "use_anti_aliasing", PROP_BOOLEAN, PROP_NONE);
	RNA_def_property_boolean_sdna(prop, NULL, "flags", MOD_DPAINT_ANTIALIAS);
	RNA_def_property_ui_text(prop, "Anti-aliasing", "Uses 5x multisampling to smoothen paint edges.");

	prop= RNA_def_property(srna, "ui_info", PROP_STRING, PROP_NONE);
	RNA_def_property_string_sdna(prop, NULL, "ui_info");
	RNA_def_property_clear_flag(prop, PROP_EDITABLE);
	RNA_def_property_ui_text(prop, "Bake Info", "Info on bake status");

	/*
	*   Effect Settings
	*/
	prop= RNA_def_property(srna, "effect_ui", PROP_ENUM, PROP_NONE);
	RNA_def_property_clear_flag(prop, PROP_ANIMATABLE);
	RNA_def_property_enum_sdna(prop, NULL, "effect_ui");
	RNA_def_property_enum_items(prop, prop_dynamicpaint_effecttype);
	RNA_def_property_ui_text(prop, "Effect Type", "");
	
	prop= RNA_def_property(srna, "use_dry_log", PROP_BOOLEAN, PROP_NONE);
	RNA_def_property_boolean_sdna(prop, NULL, "flags", MOD_DPAINT_DRY_LOG);
	RNA_def_property_ui_text(prop, "Slow", "Use 1/x instead of linear drying.");
	
	prop= RNA_def_property(srna, "use_spread", PROP_BOOLEAN, PROP_NONE);
	RNA_def_property_boolean_sdna(prop, NULL, "effect", MOD_DPAINT_EFFECT_DO_SPREAD);
	RNA_def_property_ui_text(prop, "Use Spread", "Processes spread effect. Spreads wet paint around surface.");
	
	prop= RNA_def_property(srna, "spread_speed", PROP_FLOAT, PROP_NONE);
	RNA_def_property_float_sdna(prop, NULL, "spread_speed");
	RNA_def_property_range(prop, 0.1, 5.0);
	RNA_def_property_ui_range(prop, 0.1, 5.0, 1, 2);
	RNA_def_property_ui_text(prop, "Spread Speed", "How fast spread effect moves on the canvas surface.");
	
	prop= RNA_def_property(srna, "use_drip", PROP_BOOLEAN, PROP_NONE);
	RNA_def_property_boolean_sdna(prop, NULL, "effect", MOD_DPAINT_EFFECT_DO_DRIP);
	RNA_def_property_ui_text(prop, "Use Drip", "Processes drip effect. Drips wet paint to gravity direction.");
	
	prop= RNA_def_property(srna, "drip_speed", PROP_FLOAT, PROP_NONE);
	RNA_def_property_float_sdna(prop, NULL, "drip_speed");
	RNA_def_property_range(prop, 0.1, 5.0);
	RNA_def_property_ui_range(prop, 0.1, 5.0, 1, 2);
	RNA_def_property_ui_text(prop, "Drip Speed", "How fast drip effect moves on the canvas surface.");
	
	prop= RNA_def_property(srna, "use_shrink", PROP_BOOLEAN, PROP_NONE);
	RNA_def_property_boolean_sdna(prop, NULL, "effect", MOD_DPAINT_EFFECT_DO_SHRINK);
	RNA_def_property_ui_text(prop, "Use Shrink", "Processes shrink effect. Shrinks paint areas.");
	
	prop= RNA_def_property(srna, "shrink_speed", PROP_FLOAT, PROP_NONE);
	RNA_def_property_float_sdna(prop, NULL, "shrink_speed");
	RNA_def_property_range(prop, 0.1, 5.0);
	RNA_def_property_ui_range(prop, 0.1, 5.0, 1, 2);
	RNA_def_property_ui_text(prop, "Shrink Speed", "How fast shrink effect moves on the canvas surface.");

	/*
	*   Output settings
	*/
	prop= RNA_def_property(srna, "premultiply", PROP_BOOLEAN, PROP_NONE);
	RNA_def_property_clear_flag(prop, PROP_ANIMATABLE);
	RNA_def_property_boolean_sdna(prop, NULL, "flags", MOD_DPAINT_MULALPHA);
	RNA_def_property_ui_text(prop, "Premultiply alpha", "Multiplies color by alpha. (Recommended for Blender input.)");
	
	prop= RNA_def_property(srna, "paint_output_path", PROP_STRING, PROP_DIRPATH);
	RNA_def_property_string_sdna(prop, NULL, "paint_output_path");
	RNA_def_property_ui_text(prop, "Output Path", "Directory/name to save color textures");
	
	prop= RNA_def_property(srna, "wet_output_path", PROP_STRING, PROP_DIRPATH);
	RNA_def_property_string_sdna(prop, NULL, "wet_output_path");
	RNA_def_property_ui_text(prop, "Output Path", "Directory/name to save wetmap textures");
	
	prop= RNA_def_property(srna, "displace_output_path", PROP_STRING, PROP_DIRPATH);
	RNA_def_property_string_sdna(prop, NULL, "displace_output_path");
	RNA_def_property_ui_text(prop, "Output Path", "Directory/name to save displace textures");
	
	prop= RNA_def_property(srna, "output_paint", PROP_BOOLEAN, PROP_NONE);
	RNA_def_property_boolean_sdna(prop, NULL, "output", MOD_DPAINT_OUT_PAINT);
	RNA_def_property_ui_text(prop, "Ouput Paintmaps", "Generates paint textures.");
	
	prop= RNA_def_property(srna, "output_wet", PROP_BOOLEAN, PROP_NONE);
	RNA_def_property_boolean_sdna(prop, NULL, "output", MOD_DPAINT_OUT_WET);
	RNA_def_property_ui_text(prop, "Ouput Wetmaps", "Generates wetmaps.");
	
	prop= RNA_def_property(srna, "output_disp", PROP_BOOLEAN, PROP_NONE);
	RNA_def_property_boolean_sdna(prop, NULL, "output", MOD_DPAINT_OUT_DISP);
	RNA_def_property_ui_text(prop, "Output Displacement", "Generates displacement textures.");
	
	prop= RNA_def_property(srna, "displacement", PROP_FLOAT, PROP_NONE);
	RNA_def_property_clear_flag(prop, PROP_ANIMATABLE);
	RNA_def_property_float_sdna(prop, NULL, "disp_depth");
	RNA_def_property_range(prop, 0.01, 5.0);
	RNA_def_property_ui_range(prop, 0.01, 5.0, 1, 2);
	RNA_def_property_ui_text(prop, "Displace Strength", "Maximum level of intersection to store in the texture. Use same value as the displace method strength.");
	
	prop= RNA_def_property(srna, "disp_format", PROP_ENUM, PROP_NONE);
	RNA_def_property_clear_flag(prop, PROP_ANIMATABLE);
	RNA_def_property_enum_sdna(prop, NULL, "disp_format");
	RNA_def_property_enum_items(prop, prop_dynamicpaint_disp_format);
	RNA_def_property_ui_text(prop, "File Format", "");
	
	prop= RNA_def_property(srna, "disp_type", PROP_ENUM, PROP_NONE);
	RNA_def_property_clear_flag(prop, PROP_ANIMATABLE);
	RNA_def_property_enum_sdna(prop, NULL, "disp_type");
	RNA_def_property_enum_items(prop, prop_dynamicpaint_disp_type);
	RNA_def_property_ui_text(prop, "Data Type", "");
}

static void rna_def_dynamic_paint_painter_settings(BlenderRNA *brna)
{
	StructRNA *srna;
	PropertyRNA *prop;

	/* paint collision type */
	static EnumPropertyItem prop_dynamicpaint_collisiontype[] = {
			{MOD_DPAINT_COL_PSYS, "PSYS", 0, "Particle System", ""},
			{MOD_DPAINT_COL_DIST, "DISTANCE", 0, "Proximity", ""},
			{MOD_DPAINT_COL_VOLDIST, "VOLDIST", 0, "Mesh Volume + Proximity", ""},
			{MOD_DPAINT_COL_VOLUME, "VOLUME", 0, "Mesh Volume", ""},
			{0, NULL, 0, NULL, NULL}};

	static EnumPropertyItem prop_dynamicpaint_prox_falloff[] = {
			{MOD_DPAINT_PRFALL_SMOOTH, "SMOOTH", 0, "Smooth", ""},
			{MOD_DPAINT_PRFALL_SHARP, "SHARP", 0, "Sharp", ""},
			{MOD_DPAINT_PRFALL_RAMP, "RAMP", 0, "Color Ramp", ""},
			{0, NULL, 0, NULL, NULL}};

	srna = RNA_def_struct(brna, "DynamicPaintPainterSettings", NULL);
	RNA_def_struct_ui_text(srna, "Paint Settings", "Paint settings");
	RNA_def_struct_sdna(srna, "DynamicPaintPainterSettings");
	RNA_def_struct_path_func(srna, "rna_DynamicPaintPainterSettings_path");

	/*
	*   Paint
	*/
	prop= RNA_def_property(srna, "paint_color", PROP_FLOAT, PROP_COLOR);
	RNA_def_property_float_sdna(prop, NULL, "r");
	RNA_def_property_array(prop, 3);
	RNA_def_property_ui_text(prop, "Paint Color", "Color of the paint.");
	RNA_def_property_update(prop, NC_MATERIAL|ND_SHADING_DRAW, NULL);

	prop= RNA_def_property(srna, "paint_alpha", PROP_FLOAT, PROP_NONE);
	RNA_def_property_float_sdna(prop, NULL, "alpha");
	RNA_def_property_range(prop, 0.0, 10.0);
	RNA_def_property_ui_range(prop, 0.0, 10.0, 1, 2);
	RNA_def_property_ui_text(prop, "Paint Alpha", "Paint alpha.");
	
	prop= RNA_def_property(srna, "use_material", PROP_BOOLEAN, PROP_NONE);
	RNA_def_property_boolean_sdna(prop, NULL, "flags", MOD_DPAINT_USE_MATERIAL);
	RNA_def_property_ui_text(prop, "Use object material", "Use object material to define color and alpha.");

	prop= RNA_def_property(srna, "material", PROP_POINTER, PROP_NONE);
	RNA_def_property_pointer_sdna(prop, NULL, "mat");
	RNA_def_property_ui_text(prop, "Material", "Material to use. If not defined, material linked to the mesh is used.");
	RNA_def_property_flag(prop, PROP_EDITABLE);
	
	prop= RNA_def_property(srna, "absolute_alpha", PROP_BOOLEAN, PROP_NONE);
	RNA_def_property_boolean_sdna(prop, NULL, "flags", MOD_DPAINT_ABS_ALPHA);
	RNA_def_property_ui_text(prop, "Absolute Alpha", "Only increase alpha value if paint alpha is higher than existing.");
	
	prop= RNA_def_property(srna, "paint_wetness", PROP_FLOAT, PROP_NONE);
	RNA_def_property_float_sdna(prop, NULL, "wetness");
	RNA_def_property_range(prop, 0.0, 1.0);
	RNA_def_property_ui_range(prop, 0.0, 1.0, 5, 3);
	RNA_def_property_ui_text(prop, "Paint Wetness", "Paint Wetness. Visible in wet map. Some effects only affect wet paint.");
	
	prop= RNA_def_property(srna, "paint_erase", PROP_BOOLEAN, PROP_NONE);
	RNA_def_property_boolean_sdna(prop, NULL, "flags", MOD_DPAINT_ERASE);
	RNA_def_property_ui_text(prop, "Erase Paint", "Erase / remove paint instead of adding it.");
	
	/*
	*   Paint Area / Collision
	*/
	prop= RNA_def_property(srna, "paint_source", PROP_ENUM, PROP_NONE);
	RNA_def_property_clear_flag(prop, PROP_ANIMATABLE);
	RNA_def_property_enum_sdna(prop, NULL, "collision");
	RNA_def_property_enum_items(prop, prop_dynamicpaint_collisiontype);
	RNA_def_property_ui_text(prop, "Paint Source", "");
	
	prop= RNA_def_property(srna, "paint_distance", PROP_FLOAT, PROP_NONE);
	RNA_def_property_float_sdna(prop, NULL, "paint_distance");
	RNA_def_property_range(prop, 0.0, 500.0);
	RNA_def_property_ui_range(prop, 0.0, 500.0, 10, 3);
	RNA_def_property_ui_text(prop, "Proximity Distance", "Maximum distance to mesh surface to affect paint.");
	
	prop= RNA_def_property(srna, "prox_ramp_alpha", PROP_BOOLEAN, PROP_NONE);
	RNA_def_property_boolean_sdna(prop, NULL, "flags", MOD_DPAINT_RAMP_ALPHA);
	RNA_def_property_ui_text(prop, "Only Use Alpha", "Only reads color ramp alpha.");
	
	prop= RNA_def_property(srna, "edge_displace", PROP_BOOLEAN, PROP_NONE);
	RNA_def_property_boolean_sdna(prop, NULL, "flags", MOD_DPAINT_EDGE_DISP);
	RNA_def_property_ui_text(prop, "Edge Displace", "Add displacement to intersection edges too.");
	
	prop= RNA_def_property(srna, "displace_distance", PROP_FLOAT, PROP_NONE);
	RNA_def_property_float_sdna(prop, NULL, "displace_distance");
	RNA_def_property_range(prop, 0.0, 10.0);
	RNA_def_property_ui_range(prop, 0.0, 10.0, 5, 3);
	RNA_def_property_ui_text(prop, "Displace Distance", "Maximum distance to mesh surface to displace.");
	
	prop= RNA_def_property(srna, "prox_displace_strength", PROP_FLOAT, PROP_NONE);
	RNA_def_property_float_sdna(prop, NULL, "prox_displace_strength");
	RNA_def_property_range(prop, 0.0, 1.0);
	RNA_def_property_ui_range(prop, 0.0, 1.0, 5, 3);
	RNA_def_property_ui_text(prop, "Strength", "How much of maximum intersection will be used in edges.");
	
	prop= RNA_def_property(srna, "prox_falloff", PROP_ENUM, PROP_NONE);
	RNA_def_property_clear_flag(prop, PROP_ANIMATABLE);
	RNA_def_property_enum_sdna(prop, NULL, "proximity_falloff");
	RNA_def_property_enum_items(prop, prop_dynamicpaint_prox_falloff);
	RNA_def_property_ui_text(prop, "Paint Falloff", "");
	
	prop= RNA_def_property(srna, "prox_facealigned", PROP_BOOLEAN, PROP_NONE);
	RNA_def_property_boolean_sdna(prop, NULL, "flags", MOD_DPAINT_PROX_FACEALIGNED);
	RNA_def_property_ui_text(prop, "Face Aligned", "Check proximity in face normal direction only.");
	

	/*
	*   Particle
	*/
	prop= RNA_def_property(srna, "psys", PROP_POINTER, PROP_NONE);
	RNA_def_property_pointer_sdna(prop, NULL, "psys");
	RNA_def_property_struct_type(prop, "ParticleSystem");
	RNA_def_property_flag(prop, PROP_EDITABLE);
	RNA_def_property_ui_text(prop, "Particle Systems", "The particle system to paint with.");
	
	prop= RNA_def_property(srna, "use_part_radius", PROP_BOOLEAN, PROP_NONE);
	RNA_def_property_boolean_sdna(prop, NULL, "flags", MOD_DPAINT_PART_RAD);
	RNA_def_property_ui_text(prop, "Use Particle Radius", "Uses radius from particle settings.");
	
	prop= RNA_def_property(srna, "solid_radius", PROP_FLOAT, PROP_NONE);
	RNA_def_property_float_sdna(prop, NULL, "particle_radius");
	RNA_def_property_range(prop, 0.01, 10.0);
	RNA_def_property_ui_range(prop, 0.01, 2.0, 5, 3);
	RNA_def_property_ui_text(prop, "Solid Radius", "Radius that will be painted solid.");

	prop= RNA_def_property(srna, "smooth_radius", PROP_FLOAT, PROP_NONE);
	RNA_def_property_float_sdna(prop, NULL, "particle_smooth");
	RNA_def_property_range(prop, 0.0, 10.0);
	RNA_def_property_ui_range(prop, 0.0, 1.0, 5, 0);
	RNA_def_property_ui_text(prop, "Smooth Radius", "Smooth falloff added after solid paint area.");

	/*
	*   Effect
	*/
	prop= RNA_def_property(srna, "do_paint", PROP_BOOLEAN, PROP_NONE);
	RNA_def_property_clear_flag(prop, PROP_ANIMATABLE);
	RNA_def_property_boolean_sdna(prop, NULL, "flags", MOD_DPAINT_DO_PAINT);
	RNA_def_property_ui_text(prop, "Affect Paint", "Makes this painter to affect paint data.");

	prop= RNA_def_property(srna, "do_wet", PROP_BOOLEAN, PROP_NONE);
	RNA_def_property_clear_flag(prop, PROP_ANIMATABLE);
	RNA_def_property_boolean_sdna(prop, NULL, "flags", MOD_DPAINT_DO_WETNESS);
	RNA_def_property_ui_text(prop, "Affect Wetness", "Makes this painter to affect wetness data.");

	prop= RNA_def_property(srna, "do_displace", PROP_BOOLEAN, PROP_NONE);
	RNA_def_property_clear_flag(prop, PROP_ANIMATABLE);
	RNA_def_property_boolean_sdna(prop, NULL, "flags", MOD_DPAINT_DO_DISPLACE);
	RNA_def_property_ui_text(prop, "Affect Displace", "Makes this painter to affect displace data.");
	

	/*
	* Color ramps
	*/
	prop= RNA_def_property(srna, "paint_ramp", PROP_POINTER, PROP_NONE);
	RNA_def_property_pointer_sdna(prop, NULL, "paint_ramp");
	RNA_def_property_struct_type(prop, "ColorRamp");
	RNA_def_property_ui_text(prop, "Paint Color Ramp", "Color ramp used to define proximity falloff.");
}

void RNA_def_dynamic_paint(BlenderRNA *brna)
{
	rna_def_dynamic_paint_canvas_settings(brna);
	rna_def_dynamic_paint_painter_settings(brna);
}

#endif
