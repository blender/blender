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

#include <float.h>
#include <stdlib.h>

#include "RNA_define.h"

#include "rna_internal.h"

#include "DNA_material_types.h"
#include "DNA_texture_types.h"
#include "DNA_world_types.h"

#ifdef RNA_RUNTIME

#include "MEM_guardedalloc.h"

#include "BKE_depsgraph.h"
#include "BKE_main.h"
#include "BKE_texture.h"

#include "WM_api.h"
#include "WM_types.h"

static PointerRNA rna_World_lighting_get(PointerRNA *ptr)
{
	return rna_pointer_inherit_refine(ptr, &RNA_WorldLighting, ptr->id.data);
}

static PointerRNA rna_World_stars_get(PointerRNA *ptr)
{
	return rna_pointer_inherit_refine(ptr, &RNA_WorldStarsSettings, ptr->id.data);
}

static PointerRNA rna_World_mist_get(PointerRNA *ptr)
{
	return rna_pointer_inherit_refine(ptr, &RNA_WorldMistSettings, ptr->id.data);
}

static void rna_World_mtex_begin(CollectionPropertyIterator *iter, PointerRNA *ptr)
{
	World *wo= (World*)ptr->data;
	rna_iterator_array_begin(iter, (void*)wo->mtex, sizeof(MTex*), MAX_MTEX, 0, NULL);
}

static PointerRNA rna_World_active_texture_get(PointerRNA *ptr)
{
	World *wo= (World*)ptr->data;
	Tex *tex;

	tex= give_current_world_texture(wo);
	return rna_pointer_inherit_refine(ptr, &RNA_Texture, tex);
}

static void rna_World_active_texture_set(PointerRNA *ptr, PointerRNA value)
{
	World *wo= (World*)ptr->data;

	set_current_world_texture(wo, value.data);
}

static void rna_World_update(Main *bmain, Scene *scene, PointerRNA *ptr)
{
	World *wo= ptr->id.data;

	DAG_id_flush_update(&wo->id, 0);
	WM_main_add_notifier(NC_WORLD, wo);
}

static void rna_World_draw_update(Main *bmain, Scene *scene, PointerRNA *ptr)
{
	World *wo= ptr->id.data;

	DAG_id_flush_update(&wo->id, 0);
	WM_main_add_notifier(NC_WORLD|ND_WORLD_DRAW, wo);
}

#else

static void rna_def_world_mtex(BlenderRNA *brna)
{
	StructRNA *srna;
	PropertyRNA *prop;

	static EnumPropertyItem texco_items[] = {
		{TEXCO_VIEW, "VIEW", 0, "View", "Uses view vector for the texture coordinates"},
		{TEXCO_GLOB, "GLOBAL", 0, "Global", "Uses global coordinates for the texture coordinates (interior mist)"},
		{TEXCO_ANGMAP, "ANGMAP", 0, "AngMap", "Uses 360 degree angular coordinates, e.g. for spherical light probes"},
		{TEXCO_H_SPHEREMAP, "SPHERE", 0, "Sphere", "For 360 degree panorama sky, spherical mapped, only top half"},
		{TEXCO_H_TUBEMAP, "TUBE", 0, "Tube", "For 360 degree panorama sky, cylindrical mapped, only top half"},
		{TEXCO_OBJECT, "OBJECT", 0, "Object", "Uses linked object's coordinates for texture coordinates"},
		{0, NULL, 0, NULL, NULL}};

	srna= RNA_def_struct(brna, "WorldTextureSlot", "TextureSlot");
	RNA_def_struct_sdna(srna, "MTex");
	RNA_def_struct_ui_text(srna, "World Texture Slot", "Texture slot for textures in a World datablock");

	/* map to */
	prop= RNA_def_property(srna, "map_blend", PROP_BOOLEAN, PROP_NONE);
	RNA_def_property_boolean_sdna(prop, NULL, "mapto", WOMAP_BLEND);
	RNA_def_property_ui_text(prop, "Blend", "Affect the color progression of the background");
	RNA_def_property_update(prop, 0, "rna_World_update");

	prop= RNA_def_property(srna, "map_horizon", PROP_BOOLEAN, PROP_NONE);
	RNA_def_property_boolean_sdna(prop, NULL, "mapto", WOMAP_HORIZ);
	RNA_def_property_ui_text(prop, "Horizon", "Affect the color of the horizon");
	RNA_def_property_update(prop, 0, "rna_World_update");

	prop= RNA_def_property(srna, "map_zenith_up", PROP_BOOLEAN, PROP_NONE);
	RNA_def_property_boolean_sdna(prop, NULL, "mapto", WOMAP_ZENUP);
	RNA_def_property_ui_text(prop, "Zenith Up", "Affect the color of the zenith above");
	RNA_def_property_update(prop, 0, "rna_World_update");

	prop= RNA_def_property(srna, "map_zenith_down", PROP_BOOLEAN, PROP_NONE);
	RNA_def_property_boolean_sdna(prop, NULL, "mapto", WOMAP_ZENDOWN);
	RNA_def_property_ui_text(prop, "Zenith Down", "Affect the color of the zenith below");
	RNA_def_property_update(prop, 0, "rna_World_update");

	/* unused
	prop= RNA_def_property(srna, "map_mist", PROP_BOOLEAN, PROP_NONE);
	RNA_def_property_boolean_sdna(prop, NULL, "mapto", WOMAP_MIST);
	RNA_def_property_ui_text(prop, "Mist", "Causes the texture to affect the intensity of the mist");*/

	prop= RNA_def_property(srna, "texture_coordinates", PROP_ENUM, PROP_NONE);
	RNA_def_property_enum_sdna(prop, NULL, "texco");
	RNA_def_property_enum_items(prop, texco_items);
	RNA_def_property_ui_text(prop, "Texture Coordinates", "Texture coordinates used to map the texture onto the background");
	RNA_def_property_update(prop, 0, "rna_World_update");

	prop= RNA_def_property(srna, "object", PROP_POINTER, PROP_NONE);
	RNA_def_property_pointer_sdna(prop, NULL, "object");
	RNA_def_property_struct_type(prop, "Object");
	RNA_def_property_flag(prop, PROP_EDITABLE);
	RNA_def_property_ui_text(prop, "Object", "Object to use for mapping with Object texture coordinates");
	RNA_def_property_update(prop, 0, "rna_World_update");

	prop= RNA_def_property(srna, "blend_factor", PROP_FLOAT, PROP_NONE);
	RNA_def_property_float_sdna(prop, NULL, "blendfac");
	RNA_def_property_ui_range(prop, 0, 1, 10, 3);
	RNA_def_property_ui_text(prop, "Blend Factor", "Amount texture affects color progression of the background");
	RNA_def_property_update(prop, 0, "rna_World_update");

	prop= RNA_def_property(srna, "horizon_factor", PROP_FLOAT, PROP_NONE);
	RNA_def_property_float_sdna(prop, NULL, "colfac");
	RNA_def_property_ui_range(prop, 0, 1, 10, 3);
	RNA_def_property_ui_text(prop, "Horizon Factor", "Amount texture affects color of the horizon");
	RNA_def_property_update(prop, 0, "rna_World_update");

	prop= RNA_def_property(srna, "zenith_up_factor", PROP_FLOAT, PROP_NONE);
	RNA_def_property_float_sdna(prop, NULL, "zenupfac");
	RNA_def_property_ui_range(prop, 0, 1, 10, 3);
	RNA_def_property_ui_text(prop, "Zenith Up Factor", "Amount texture affects color of the zenith above");
	RNA_def_property_update(prop, 0, "rna_World_update");

	prop= RNA_def_property(srna, "zenith_down_factor", PROP_FLOAT, PROP_NONE);
	RNA_def_property_float_sdna(prop, NULL, "zendownfac");
	RNA_def_property_ui_range(prop, 0, 1, 10, 3);
	RNA_def_property_ui_text(prop, "Zenith Down Factor", "Amount texture affects color of the zenith below");
	RNA_def_property_update(prop, 0, "rna_World_update");
}

static void rna_def_lighting(BlenderRNA *brna)
{
	StructRNA *srna;
	PropertyRNA *prop;

	static EnumPropertyItem blend_mode_items[] = {
		{WO_AOMUL, "MULTIPLY", 0, "Multiply", "Multiply direct lighting with ambient occlusion, darkening the result"},
		{WO_AOADD, "ADD", 0, "Add", "Add light and shadow"},
		{0, NULL, 0, NULL, NULL}};

	static EnumPropertyItem prop_color_items[] = {
		{WO_AOPLAIN, "PLAIN", 0, "White", "Plain diffuse energy (white.)"},
		{WO_AOSKYCOL, "SKY_COLOR", 0, "Sky Color", "Use horizon and zenith color for diffuse energy"},
		{WO_AOSKYTEX, "SKY_TEXTURE", 0, "Sky Texture", "Does full Sky texture render for diffuse energy"},
		{0, NULL, 0, NULL, NULL}};

	static EnumPropertyItem prop_sample_method_items[] = {
		{WO_AOSAMP_CONSTANT, "CONSTANT_JITTERED", 0, "Constant Jittered", ""},
		{WO_AOSAMP_HALTON, "ADAPTIVE_QMC", 0, "Adaptive QMC", "Fast in high-contrast areas"},
		{WO_AOSAMP_HAMMERSLEY, "CONSTANT_QMC", 0, "Constant QMC", "Best quality"},
		{0, NULL, 0, NULL, NULL}};

	static EnumPropertyItem prop_gather_method_items[] = {
		{WO_AOGATHER_RAYTRACE, "RAYTRACE", 0, "Raytrace", "Accurate, but slow when noise-free results are required"},
		{WO_AOGATHER_APPROX, "APPROXIMATE", 0, "Approximate", "Inaccurate, but faster and without noise"},
		{0, NULL, 0, NULL, NULL}};

	srna= RNA_def_struct(brna, "WorldLighting", NULL);
	RNA_def_struct_sdna(srna, "World");
	RNA_def_struct_nested(brna, srna, "World");
	RNA_def_struct_ui_text(srna, "Lighting", "Lighting for a World datablock");

	/* ambient occlusion */
	prop= RNA_def_property(srna, "use_ambient_occlusion", PROP_BOOLEAN, PROP_NONE);
	RNA_def_property_boolean_sdna(prop, NULL, "mode", WO_AMB_OCC);
	RNA_def_property_ui_text(prop, "Use Ambient Occlusion", "Use Ambient Occlusion to add shadowing based on distance between objects");
	RNA_def_property_update(prop, 0, "rna_World_update");

	prop= RNA_def_property(srna, "ao_factor", PROP_FLOAT, PROP_FACTOR);
	RNA_def_property_float_sdna(prop, NULL, "aoenergy");
	RNA_def_property_range(prop, 0, INT_MAX);
	RNA_def_property_ui_range(prop, 0, 1, 0.1, 2);
	RNA_def_property_ui_text(prop, "Factor", "Factor for ambient occlusion blending");
	RNA_def_property_update(prop, 0, "rna_World_update");

	prop= RNA_def_property(srna, "ao_blend_mode", PROP_ENUM, PROP_NONE);
	RNA_def_property_enum_sdna(prop, NULL, "aomix");
	RNA_def_property_enum_items(prop, blend_mode_items);
	RNA_def_property_ui_text(prop, "Blend Mode", "Defines how AO mixes with material shading");
	RNA_def_property_update(prop, 0, "rna_World_update");

	/* environment lighting */
	prop= RNA_def_property(srna, "use_environment_lighting", PROP_BOOLEAN, PROP_NONE);
	RNA_def_property_boolean_sdna(prop, NULL, "mode", WO_ENV_LIGHT);
	RNA_def_property_ui_text(prop, "Use Environment Lighting", "Add light coming from the environment");
	RNA_def_property_update(prop, 0, "rna_World_update");

	prop= RNA_def_property(srna, "environment_energy", PROP_FLOAT, PROP_NONE);
	RNA_def_property_float_sdna(prop, NULL, "ao_env_energy");
	RNA_def_property_ui_range(prop, 0, FLT_MAX, 0.1, 2);
	RNA_def_property_ui_text(prop, "Environment Color", "Defines the strength of environment light");
	RNA_def_property_update(prop, 0, "rna_World_update");

	prop= RNA_def_property(srna, "environment_color", PROP_ENUM, PROP_NONE);
	RNA_def_property_enum_sdna(prop, NULL, "aocolor");
	RNA_def_property_enum_items(prop, prop_color_items);
	RNA_def_property_ui_text(prop, "Environment Color", "Defines where the color of the environment light comes from");
	RNA_def_property_update(prop, 0, "rna_World_update");

	/* indirect lighting */
	prop= RNA_def_property(srna, "use_indirect_lighting", PROP_BOOLEAN, PROP_NONE);
	RNA_def_property_boolean_sdna(prop, NULL, "mode", WO_INDIRECT_LIGHT);
	RNA_def_property_ui_text(prop, "Use Indirect Lighting", "Add indirect light bouncing of surrounding objects");
	RNA_def_property_update(prop, 0, "rna_World_update");

	prop= RNA_def_property(srna, "indirect_factor", PROP_FLOAT, PROP_FACTOR);
	RNA_def_property_float_sdna(prop, NULL, "ao_indirect_energy");
	RNA_def_property_range(prop, 0, INT_MAX);
	RNA_def_property_ui_range(prop, 0, 1, 0.1, 2);
	RNA_def_property_ui_text(prop, "Indirect Factor", "Factor for how much surrounding objects contribute to light");
	RNA_def_property_update(prop, 0, "rna_World_update");

	prop= RNA_def_property(srna, "indirect_bounces", PROP_INT, PROP_UNSIGNED);
	RNA_def_property_int_sdna(prop, NULL, "ao_indirect_bounces");
	RNA_def_property_range(prop, 1, INT_MAX);
	RNA_def_property_ui_text(prop, "Bounces", "Number of indirect diffuse light bounces to use for approximate ambient occlusion");
	RNA_def_property_update(prop, 0, "rna_World_update");

	/* gathering parameters */
	prop= RNA_def_property(srna, "gather_method", PROP_ENUM, PROP_NONE);
	RNA_def_property_enum_sdna(prop, NULL, "ao_gather_method");
	RNA_def_property_enum_items(prop, prop_gather_method_items);
	RNA_def_property_ui_text(prop, "Gather Method", "");
	RNA_def_property_update(prop, 0, "rna_World_update");

	prop= RNA_def_property(srna, "passes", PROP_INT, PROP_NONE);
	RNA_def_property_int_sdna(prop, NULL, "ao_approx_passes");
	RNA_def_property_range(prop, 0, 10);
	RNA_def_property_ui_text(prop, "Passes", "Number of preprocessing passes to reduce overocclusion (for approximate ambient occlusion)");
	RNA_def_property_update(prop, 0, "rna_World_update");

	prop= RNA_def_property(srna, "distance", PROP_FLOAT, PROP_DISTANCE);
	RNA_def_property_float_sdna(prop, NULL, "aodist");
	RNA_def_property_ui_text(prop, "Distance", "Length of rays, defines how far away other faces give occlusion effect");
	RNA_def_property_update(prop, 0, "rna_World_update");

	prop= RNA_def_property(srna, "falloff_strength", PROP_FLOAT, PROP_NONE);
	RNA_def_property_float_sdna(prop, NULL, "aodistfac");
	RNA_def_property_ui_text(prop, "Strength", "Distance attenuation factor, the higher, the 'shorter' the shadows");
	RNA_def_property_update(prop, 0, "rna_World_update");

	prop= RNA_def_property(srna, "bias", PROP_FLOAT, PROP_NONE);
	RNA_def_property_float_sdna(prop, NULL, "aobias");
	RNA_def_property_range(prop, 0, 0.5);
	RNA_def_property_ui_text(prop, "Bias", "Bias (in radians) to prevent smoothed faces from showing banding (for Raytrace Constant Jittered)");
	RNA_def_property_update(prop, 0, "rna_World_update");

	prop= RNA_def_property(srna, "threshold", PROP_FLOAT, PROP_NONE);
	RNA_def_property_float_sdna(prop, NULL, "ao_adapt_thresh");
	RNA_def_property_range(prop, 0, 1);
	RNA_def_property_ui_text(prop, "Threshold", "Samples below this threshold will be considered fully shadowed/unshadowed and skipped (for Raytrace Adaptive QMC)");
	RNA_def_property_update(prop, 0, "rna_World_update");

	prop= RNA_def_property(srna, "adapt_to_speed", PROP_FLOAT, PROP_NONE);
	RNA_def_property_float_sdna(prop, NULL, "ao_adapt_speed_fac");
	RNA_def_property_range(prop, 0, 1);
	RNA_def_property_ui_text(prop, "Adapt To Speed", "Use the speed vector pass to reduce AO samples in fast moving pixels. Higher values result in more aggressive sample reduction. Requires Vec pass enabled (for Raytrace Adaptive QMC)");
	RNA_def_property_update(prop, 0, "rna_World_update");

	prop= RNA_def_property(srna, "error_tolerance", PROP_FLOAT, PROP_NONE);
	RNA_def_property_float_sdna(prop, NULL, "ao_approx_error");
	RNA_def_property_range(prop, 0.0001, 10);
	RNA_def_property_ui_text(prop, "Error Tolerance", "Low values are slower and higher quality (for Approximate)");
	RNA_def_property_update(prop, 0, "rna_World_update");

	prop= RNA_def_property(srna, "correction", PROP_FLOAT, PROP_NONE);
	RNA_def_property_float_sdna(prop, NULL, "ao_approx_correction");
	RNA_def_property_range(prop, 0, 1);
	RNA_def_property_ui_range(prop, 0, 1, 0.1, 2);
	RNA_def_property_ui_text(prop, "Correction", "Ad-hoc correction for over-occlusion due to the approximation (for Approximate)");
	RNA_def_property_update(prop, 0, "rna_World_update");

	prop= RNA_def_property(srna, "falloff", PROP_BOOLEAN, PROP_NONE);
	RNA_def_property_boolean_sdna(prop, NULL, "aomode", WO_AODIST);
	RNA_def_property_ui_text(prop, "Falloff", "");
	RNA_def_property_update(prop, 0, "rna_World_update");

	prop= RNA_def_property(srna, "pixel_cache", PROP_BOOLEAN, PROP_NONE);
	RNA_def_property_boolean_sdna(prop, NULL, "aomode", WO_AOCACHE);
	RNA_def_property_ui_text(prop, "Pixel Cache", "Cache AO results in pixels and interpolate over neighbouring pixels for speedup (for Approximate)");
	RNA_def_property_update(prop, 0, "rna_World_update");

	prop= RNA_def_property(srna, "samples", PROP_INT, PROP_NONE);
	RNA_def_property_int_sdna(prop, NULL, "aosamp");
	RNA_def_property_range(prop, 1, 32);
	RNA_def_property_ui_text(prop, "Samples", "Amount of ray samples. Higher values give smoother results and longer rendering times");
	RNA_def_property_update(prop, 0, "rna_World_update");

	prop= RNA_def_property(srna, "sample_method", PROP_ENUM, PROP_NONE);
	RNA_def_property_enum_sdna(prop, NULL, "ao_samp_method");
	RNA_def_property_enum_items(prop, prop_sample_method_items);
	RNA_def_property_ui_text(prop, "Sample Method", "Method for generating shadow samples (for Raytrace)");
	RNA_def_property_update(prop, 0, "rna_World_update");
}

static void rna_def_world_mist(BlenderRNA *brna)
{
	StructRNA *srna;
	PropertyRNA *prop;
	
	static EnumPropertyItem falloff_items[] = {
		{0, "QUADRATIC", 0, "Quadratic", "Mist uses quadratic progression"},
		{1, "LINEAR", 0, "Linear", "Mist uses linear progression"},
		{2, "INVERSE_QUADRATIC", 0, "Inverse Quadratic", "Mist uses inverse quadratic progression"},
		{0, NULL, 0, NULL, NULL}};

	srna= RNA_def_struct(brna, "WorldMistSettings", NULL);
	RNA_def_struct_sdna(srna, "World");
	RNA_def_struct_nested(brna, srna, "World");
	RNA_def_struct_ui_text(srna, "World Mist", "Mist settings for a World data-block");

	prop= RNA_def_property(srna, "enabled", PROP_BOOLEAN, PROP_NONE);
	RNA_def_property_boolean_sdna(prop, NULL, "mode", WO_MIST);
	RNA_def_property_ui_text(prop, "Enabled", "Occlude objects with the environment color as they are further away");
	RNA_def_property_update(prop, 0, "rna_World_draw_update");

	prop= RNA_def_property(srna, "intensity", PROP_FLOAT, PROP_NONE);
	RNA_def_property_float_sdna(prop, NULL, "misi");
	RNA_def_property_range(prop, 0, 1);
	RNA_def_property_ui_text(prop, "Intensity", "Intensity of the mist effect");
	RNA_def_property_update(prop, 0, "rna_World_update");

	prop= RNA_def_property(srna, "start", PROP_FLOAT, PROP_DISTANCE);
	RNA_def_property_float_sdna(prop, NULL, "miststa");
	RNA_def_property_range(prop, 0, FLT_MAX);
	RNA_def_property_ui_range(prop, 0, 10000, 10, 2);
	RNA_def_property_ui_text(prop, "Start", "Starting distance of the mist, measured from the camera");
	RNA_def_property_update(prop, 0, "rna_World_draw_update");

	prop= RNA_def_property(srna, "depth", PROP_FLOAT, PROP_DISTANCE);
	RNA_def_property_float_sdna(prop, NULL, "mistdist");
	RNA_def_property_range(prop, 0, FLT_MAX);
	RNA_def_property_ui_range(prop, 0, 10000, 10, 2);
	RNA_def_property_ui_text(prop, "Depth", "The distance over which the mist effect fades in");
	RNA_def_property_update(prop, 0, "rna_World_draw_update");

	prop= RNA_def_property(srna, "height", PROP_FLOAT, PROP_DISTANCE);
	RNA_def_property_float_sdna(prop, NULL, "misthi");
	RNA_def_property_range(prop, 0, 100);
	RNA_def_property_ui_text(prop, "Height", "Control how much mist density decreases with height");
	RNA_def_property_update(prop, 0, "rna_World_update");
	
	prop= RNA_def_property(srna, "falloff", PROP_ENUM, PROP_NONE);
	RNA_def_property_enum_sdna(prop, NULL, "mistype");
	RNA_def_property_enum_items(prop, falloff_items);
	RNA_def_property_ui_text(prop, "Falloff", "Type of transition used to fade mist");
	RNA_def_property_update(prop, 0, "rna_World_update");
}

static void rna_def_world_stars(BlenderRNA *brna)
{
	StructRNA *srna;
	PropertyRNA *prop;

	srna= RNA_def_struct(brna, "WorldStarsSettings", NULL);
	RNA_def_struct_sdna(srna, "World");
	RNA_def_struct_nested(brna, srna, "World");
	RNA_def_struct_ui_text(srna, "World Stars", "Stars setting for a World data-block");

	prop= RNA_def_property(srna, "enabled", PROP_BOOLEAN, PROP_NONE);
	RNA_def_property_boolean_sdna(prop, NULL, "mode", WO_STARS);
	RNA_def_property_ui_text(prop, "Enabled", "Enable starfield generation");
	RNA_def_property_update(prop, 0, "rna_World_update");

	prop= RNA_def_property(srna, "size", PROP_FLOAT, PROP_NONE);
	RNA_def_property_float_sdna(prop, NULL, "starsize");
	RNA_def_property_range(prop, 0, 10);
	RNA_def_property_ui_text(prop, "Size", "Average screen dimension of stars");
	RNA_def_property_update(prop, 0, "rna_World_update");

	prop= RNA_def_property(srna, "min_distance", PROP_FLOAT, PROP_DISTANCE);
	RNA_def_property_float_sdna(prop, NULL, "starmindist");
	RNA_def_property_range(prop, 0, 1000);
	RNA_def_property_ui_text(prop, "Minimum Distance", "Minimum distance to the camera for stars");
	RNA_def_property_update(prop, 0, "rna_World_update");

	prop= RNA_def_property(srna, "average_separation", PROP_FLOAT, PROP_NONE);
	RNA_def_property_float_sdna(prop, NULL, "stardist");
	RNA_def_property_range(prop, 2, 1000);
	RNA_def_property_ui_text(prop, "Average Separation", "Average distance between any two stars");
	RNA_def_property_update(prop, 0, "rna_World_update");

	prop= RNA_def_property(srna, "color_randomization", PROP_FLOAT, PROP_NONE);
	RNA_def_property_float_sdna(prop, NULL, "starcolnoise");
	RNA_def_property_range(prop, 0, 1);
	RNA_def_property_ui_text(prop, "Color Randomization", "Randomize star colors");
	RNA_def_property_update(prop, 0, "rna_World_update");
	
	/* unused
	prop= RNA_def_property(srna, "color", PROP_FLOAT, PROP_COLOR);
	RNA_def_property_float_sdna(prop, NULL, "starr");
	RNA_def_property_array(prop, 3);
	RNA_def_property_ui_text(prop, "Color", "Stars color");
	RNA_def_property_update(prop, 0, "rna_World_update");*/
}

void RNA_def_world(BlenderRNA *brna)
{
	StructRNA *srna;
	PropertyRNA *prop;

/*
	static EnumPropertyItem physics_engine_items[] = {
		{WOPHY_NONE, "NONE", 0, "None", ""},
		//{WOPHY_ENJI, "ENJI", 0, "Enji", ""},
		//{WOPHY_SUMO, "SUMO", 0, "Sumo (Deprecated)", ""},
		//{WOPHY_DYNAMO, "DYNAMO", 0, "Dynamo", ""},
		//{WOPHY_ODE, "ODE", 0, "ODE", ""},
		{WOPHY_BULLET, "BULLET", 0, "Bullet", ""},
		{0, NULL, 0, NULL, NULL}};
*/

	srna= RNA_def_struct(brna, "World", "ID");
	RNA_def_struct_ui_text(srna, "World", "World datablock describing the environment and ambient lighting of a scene");
	RNA_def_struct_ui_icon(srna, ICON_WORLD_DATA);

	rna_def_animdata_common(srna);
	rna_def_mtex_common(srna, "rna_World_mtex_begin", "rna_World_active_texture_get",
		"rna_World_active_texture_set", "WorldTextureSlot", "rna_World_update");

	/* colors */
	prop= RNA_def_property(srna, "horizon_color", PROP_FLOAT, PROP_COLOR);
	RNA_def_property_float_sdna(prop, NULL, "horr");
	RNA_def_property_array(prop, 3);
	RNA_def_property_ui_text(prop, "Horizon Color", "Color at the horizon");
	RNA_def_property_update(prop, 0, "rna_World_update");
	
	prop= RNA_def_property(srna, "zenith_color", PROP_FLOAT, PROP_COLOR);
	RNA_def_property_float_sdna(prop, NULL, "zenr");
	RNA_def_property_array(prop, 3);
	RNA_def_property_ui_text(prop, "Zenith Color", "Color at the zenith");
	RNA_def_property_update(prop, 0, "rna_World_update");

	prop= RNA_def_property(srna, "ambient_color", PROP_FLOAT, PROP_COLOR);
	RNA_def_property_float_sdna(prop, NULL, "ambr");
	RNA_def_property_array(prop, 3);
	RNA_def_property_ui_text(prop, "Ambient Color", "");
	RNA_def_property_update(prop, 0, "rna_World_update");

	/* exp, range */
	prop= RNA_def_property(srna, "exposure", PROP_FLOAT, PROP_NONE);
	RNA_def_property_float_sdna(prop, NULL, "exp");
	RNA_def_property_range(prop, 0.0, 1.0);
	RNA_def_property_ui_text(prop, "Exposure", "Amount of exponential color correction for light");
	RNA_def_property_update(prop, 0, "rna_World_update");

	prop= RNA_def_property(srna, "range", PROP_FLOAT, PROP_NONE);
	RNA_def_property_float_sdna(prop, NULL, "range");
	RNA_def_property_range(prop, 0.2, 5.0);
	RNA_def_property_ui_text(prop, "Range", "The color range that will be mapped to 0-1");
	RNA_def_property_update(prop, 0, "rna_World_update");

	/* sky type */
	prop= RNA_def_property(srna, "blend_sky", PROP_BOOLEAN, PROP_NONE);
	RNA_def_property_boolean_sdna(prop, NULL, "skytype", WO_SKYBLEND);
	RNA_def_property_ui_text(prop, "Blend Sky", "Render background with natural progression from horizon to zenith");
	RNA_def_property_update(prop, 0, "rna_World_update");

	prop= RNA_def_property(srna, "paper_sky", PROP_BOOLEAN, PROP_NONE);
	RNA_def_property_boolean_sdna(prop, NULL, "skytype", WO_SKYPAPER);
	RNA_def_property_ui_text(prop, "Paper Sky", "Flatten blend or texture coordinates");
	RNA_def_property_update(prop, 0, "rna_World_update");

	prop= RNA_def_property(srna, "real_sky", PROP_BOOLEAN, PROP_NONE);
	RNA_def_property_boolean_sdna(prop, NULL, "skytype", WO_SKYREAL);
	RNA_def_property_ui_text(prop, "Real Sky", "Render background with a real horizon, relative to the camera angle");
	RNA_def_property_update(prop, 0, "rna_World_update");

	/* nested structs */
	prop= RNA_def_property(srna, "lighting", PROP_POINTER, PROP_NONE);
	RNA_def_property_flag(prop, PROP_NEVER_NULL);
	RNA_def_property_struct_type(prop, "WorldLighting");
	RNA_def_property_pointer_funcs(prop, "rna_World_lighting_get", NULL, NULL);
	RNA_def_property_ui_text(prop, "Lighting", "World lighting settings");

	prop= RNA_def_property(srna, "mist", PROP_POINTER, PROP_NONE);
	RNA_def_property_flag(prop, PROP_NEVER_NULL);
	RNA_def_property_struct_type(prop, "WorldMistSettings");
	RNA_def_property_pointer_funcs(prop, "rna_World_mist_get", NULL, NULL);
	RNA_def_property_ui_text(prop, "Mist", "World mist settings");

	prop= RNA_def_property(srna, "stars", PROP_POINTER, PROP_NONE);
	RNA_def_property_flag(prop, PROP_NEVER_NULL);
	RNA_def_property_struct_type(prop, "WorldStarsSettings");
	RNA_def_property_pointer_funcs(prop, "rna_World_stars_get", NULL, NULL);
	RNA_def_property_ui_text(prop, "Stars", "World stars settings");

	rna_def_lighting(brna);
	rna_def_world_mist(brna);
	rna_def_world_stars(brna);
	rna_def_world_mtex(brna);
}

#endif

