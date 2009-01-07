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

#include <float.h>
#include <stdlib.h>

#include "RNA_define.h"
#include "RNA_types.h"

#include "rna_internal.h"

#include "DNA_world_types.h"

#ifdef RNA_RUNTIME

static void *rna_World_ambient_occlusion_get(PointerRNA *ptr)
{
	return ptr->id.data;
}

#else

void rna_def_ambient_occlusion(BlenderRNA *brna, StructRNA *parent)
{
	StructRNA *srna;
	PropertyRNA *prop;

	static EnumPropertyItem prop_mode_items[] = {
		{WO_AOADD, "ADD", "Add", "Add light and shadow."},
		{WO_AOSUB, "SUBTRACT", "Subtract", "Subtract light and shadow (needs a normal light to make anything visible.)"},
		{WO_AOADDSUB, "BOTH", "Both", "Both lighten and darken."},
		{0, NULL, NULL, NULL}};

	static EnumPropertyItem prop_color_items[] = {
		{WO_AOPLAIN, "PLAIN", "White", "Plain diffuse energy (white.)"},
		{WO_AOSKYCOL, "SKY_COLOR", "Sky Color", "Use horizon and zenith color for diffuse energy."},
		{WO_AOSKYTEX, "SKY_TEXTURE", "Sky Texture", "Does full Sky texture render for diffuse energy."},
		{0, NULL, NULL, NULL}};

	static EnumPropertyItem prop_sample_method_items[] = {
		{WO_AOSAMP_CONSTANT, "CONSTANT_JITTERED", "Constant Jittered", ""},
		{WO_AOSAMP_HALTON, "ADAPTIVE_QMC", "Adaptive QMC", "Fast in high-contrast areas."},
		{WO_AOSAMP_HAMMERSLEY, "ADAPTIVE_QMC", "Constant QMC", "Best quality."},
		{0, NULL, NULL, NULL}};

	static EnumPropertyItem prop_gather_method_items[] = {
		{WO_AOGATHER_RAYTRACE, "RAYTRACE", "Raytrace", "Accurate, but slow when noise-free results are required."},
		{WO_AOGATHER_APPROX, "APPROXIMATE", "Approximate", "Inaccurate, but faster and without noise."},
		{0, NULL, NULL, NULL}};

	srna= RNA_def_struct(brna, "WorldAmbientOcclusion", NULL);
	RNA_def_struct_sdna(srna, "World");
	RNA_def_struct_ui_text(srna, "Ambient Occlusion", "DOC_BROKEN");

	prop= RNA_def_property(parent, "ambient_occlusion", PROP_POINTER, PROP_NONE);
	RNA_def_property_struct_type(prop, "WorldAmbientOcclusion");
	RNA_def_property_pointer_funcs(prop, "rna_World_ambient_occlusion_get", NULL, NULL);
	RNA_def_property_ui_text(prop, "Ambient Occlusion", "");

	prop= RNA_def_property(srna, "enabled", PROP_BOOLEAN, PROP_NONE);
	RNA_def_property_boolean_sdna(prop, NULL, "mode", WO_AMB_OCC);
	RNA_def_property_ui_text(prop, "Enabled", "");

	prop= RNA_def_property(srna, "distance", PROP_FLOAT, PROP_NONE);
	RNA_def_property_float_sdna(prop, NULL, "aodist");
	RNA_def_property_range(prop, 0.001, 5000);
	RNA_def_property_ui_text(prop, "Distance", "Length of rays, defines how far away other faces give occlusion effect.");

	prop= RNA_def_property(srna, "strength", PROP_FLOAT, PROP_NONE);
	RNA_def_property_float_sdna(prop, NULL, "aodistfac");
	RNA_def_property_range(prop, 0.00001, 10);
	RNA_def_property_ui_text(prop, "Strength", "Distance attenuation factor, the higher, the 'shorter' the shadows.");

	prop= RNA_def_property(srna, "energy", PROP_FLOAT, PROP_NONE);
	RNA_def_property_float_sdna(prop, NULL, "aoenergy");
	RNA_def_property_range(prop, 0.01, 3);
	RNA_def_property_ui_text(prop, "Energy", "Global energy scale for ambient occlusion.");

	prop= RNA_def_property(srna, "bias", PROP_FLOAT, PROP_NONE);
	RNA_def_property_float_sdna(prop, NULL, "aobias");
	RNA_def_property_range(prop, 0, 0.5);
	RNA_def_property_ui_text(prop, "Bias", "Bias (in radians) to prevent smoothed faces from showing banding.");

	prop= RNA_def_property(srna, "threshold", PROP_FLOAT, PROP_NONE);
	RNA_def_property_float_sdna(prop, NULL, "ao_adapt_thresh");
	RNA_def_property_range(prop, 0, 1);
	RNA_def_property_ui_text(prop, "Threshold", "Samples below this threshold will be considered fully shadowed/unshadowed and skipped.");

	prop= RNA_def_property(srna, "adapt_to_speed", PROP_FLOAT, PROP_NONE);
	RNA_def_property_float_sdna(prop, NULL, "ao_adapt_speed_fac");
	RNA_def_property_range(prop, 0, 1);
	RNA_def_property_ui_text(prop, "Adapt To Speed", "Use the speed vector pass to reduce AO samples in fast moving pixels. Higher values result in more aggressive sample reduction. Requires Vec pass enabled.");

	prop= RNA_def_property(srna, "error_tolerance", PROP_FLOAT, PROP_NONE);
	RNA_def_property_float_sdna(prop, NULL, "ao_approx_error");
	RNA_def_property_range(prop, 0.0001, 1);
	RNA_def_property_ui_text(prop, "Error Tolerance", "Low values are slower and higher quality.");

	prop= RNA_def_property(srna, "correction", PROP_FLOAT, PROP_NONE);
	RNA_def_property_float_sdna(prop, NULL, "ao_approx_correction");
	RNA_def_property_range(prop, 0, 1);
	RNA_def_property_ui_text(prop, "Correction", "Ad-hoc correction for over-occlusion due to the approximation.");

	prop= RNA_def_property(srna, "falloff", PROP_BOOLEAN, PROP_NONE);
	RNA_def_property_boolean_sdna(prop, NULL, "aomode", WO_AODIST);
	RNA_def_property_ui_text(prop, "Falloff", "");

	prop= RNA_def_property(srna, "pixel_cache", PROP_BOOLEAN, PROP_NONE);
	RNA_def_property_boolean_sdna(prop, NULL, "aomode", WO_AOCACHE);
	RNA_def_property_ui_text(prop, "Pixel Cache", "Cache AO results in pixels and interpolate over neighbouring pixels for speedup.");

	prop= RNA_def_property(srna, "samples", PROP_INT, PROP_NONE);
	RNA_def_property_int_sdna(prop, NULL, "aosamp");
	RNA_def_property_range(prop, 0, 1);
	RNA_def_property_ui_text(prop, "Samples", "");

	prop= RNA_def_property(srna, "mode", PROP_ENUM, PROP_NONE);
	RNA_def_property_enum_sdna(prop, NULL, "aomix");
	RNA_def_property_enum_items(prop, prop_mode_items);
	RNA_def_property_ui_text(prop, "Mode", "");

	prop= RNA_def_property(srna, "color", PROP_ENUM, PROP_NONE);
	RNA_def_property_enum_sdna(prop, NULL, "aocolor");
	RNA_def_property_enum_items(prop, prop_color_items);
	RNA_def_property_ui_text(prop, "Color", "");

	prop= RNA_def_property(srna, "sample_method", PROP_ENUM, PROP_NONE);
	RNA_def_property_enum_sdna(prop, NULL, "ao_samp_method");
	RNA_def_property_enum_items(prop, prop_sample_method_items);
	RNA_def_property_ui_text(prop, "Sample Method", "Method for generating shadow samples.");

	prop= RNA_def_property(srna, "gather_method", PROP_ENUM, PROP_NONE);
	RNA_def_property_enum_sdna(prop, NULL, "ao_gather_method");
	RNA_def_property_enum_items(prop, prop_gather_method_items);
	RNA_def_property_ui_text(prop, "Gather Method", "");

	prop= RNA_def_property(srna, "passes", PROP_INT, PROP_NONE);
	RNA_def_property_int_sdna(prop, NULL, "ao_approx_passes");
	RNA_def_property_range(prop, 0, 10);
	RNA_def_property_ui_text(prop, "Passes", "Number of preprocessing passes to reduce overocclusion.");
}

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
	srna= RNA_def_struct(brna, "World", "ID");
	RNA_def_struct_ui_text(srna, "World", "DOC_BROKEN");

	rna_def_ipo_common(srna);

	rna_def_ambient_occlusion(brna, srna);

/*
	prop= RNA_def_property(srna, "mtex", PROP_POINTER, PROP_NONE);
	RNA_def_property_struct_type(prop, "MTex");
	RNA_def_property_ui_text(prop, "MTex", "MTex associated with this world setting.");
*/

	/* colors */
	prop= RNA_def_property(srna, "horizon_color", PROP_FLOAT, PROP_COLOR);
	RNA_def_property_float_sdna(prop, NULL, "horr");
	RNA_def_property_array(prop, 3);
	RNA_def_property_ui_text(prop, "Horizon Color", "Color at the horizon.");

	prop= RNA_def_property(srna, "zenith_color", PROP_FLOAT, PROP_COLOR);
	RNA_def_property_float_sdna(prop, NULL, "zenr");
	RNA_def_property_array(prop, 3);
	RNA_def_property_ui_text(prop, "Zenith Color", "Color at the zenith.");

	prop= RNA_def_property(srna, "ambient_color", PROP_FLOAT, PROP_COLOR);
	RNA_def_property_float_sdna(prop, NULL, "ambr");
	RNA_def_property_array(prop, 3);
	RNA_def_property_ui_text(prop, "Ambient Color", "");

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

	/* mist */
	prop= RNA_def_property(srna, "mist_intensity", PROP_FLOAT, PROP_NONE);
	RNA_def_property_float_sdna(prop, NULL, "misi");
	RNA_def_property_range(prop, 0, 1);
	RNA_def_property_ui_text(prop, "Mist Intensity", "");

	prop= RNA_def_property(srna, "mist_start", PROP_FLOAT, PROP_NONE);
	RNA_def_property_float_sdna(prop, NULL, "miststa");
	RNA_def_property_range(prop, 0, FLT_MAX);
	RNA_def_property_ui_range(prop, 0, 10000, 10, 2);
	RNA_def_property_ui_text(prop, "Mist Intensity", "");

	prop= RNA_def_property(srna, "mist_depth", PROP_FLOAT, PROP_NONE);
	RNA_def_property_float_sdna(prop, NULL, "mistdist");
	RNA_def_property_range(prop, 0, FLT_MAX);
	RNA_def_property_ui_range(prop, 0, 10000, 10, 2);
	RNA_def_property_ui_text(prop, "Mist Depth", "");

	prop= RNA_def_property(srna, "mist_height", PROP_FLOAT, PROP_NONE);
	RNA_def_property_float_sdna(prop, NULL, "misthi");
	RNA_def_property_range(prop, 0, 100);
	RNA_def_property_ui_text(prop, "Mist Height", "");

	/* stars */
	prop= RNA_def_property(srna, "star_size", PROP_FLOAT, PROP_NONE);
	RNA_def_property_float_sdna(prop, NULL, "starsize");
	RNA_def_property_range(prop, 0, 10);
	RNA_def_property_ui_text(prop, "Star Size", "");

	prop= RNA_def_property(srna, "star_minimum_distance", PROP_FLOAT, PROP_NONE);
	RNA_def_property_float_sdna(prop, NULL, "starmindist");
	RNA_def_property_range(prop, 0, 1000);
	RNA_def_property_ui_text(prop, "Star Minimum Distance", "");

	prop= RNA_def_property(srna, "star_average_separation", PROP_FLOAT, PROP_NONE);
	RNA_def_property_float_sdna(prop, NULL, "stardist");
	RNA_def_property_range(prop, 2, 1000);
	RNA_def_property_ui_text(prop, "Star Average Separation", "");

	prop= RNA_def_property(srna, "star_color_randomization", PROP_FLOAT, PROP_NONE);
	RNA_def_property_float_sdna(prop, NULL, "starcolnoise");
	RNA_def_property_range(prop, 0, 1);
	RNA_def_property_ui_text(prop, "Star Color Randomization", "");

	prop= RNA_def_property(srna, "star_color", PROP_FLOAT, PROP_COLOR);
	RNA_def_property_float_sdna(prop, NULL, "starr");
	RNA_def_property_array(prop, 3);
	RNA_def_property_ui_text(prop, "Star Color", "");

	/* modes */
	prop= RNA_def_property(srna, "mist", PROP_BOOLEAN, PROP_NONE);
	RNA_def_property_boolean_sdna(prop, NULL, "mode", WO_MIST);
	RNA_def_property_ui_text(prop, "Mist", "");

	prop= RNA_def_property(srna, "stars", PROP_BOOLEAN, PROP_NONE);
	RNA_def_property_boolean_sdna(prop, NULL, "mode", WO_STARS);
	RNA_def_property_ui_text(prop, "Stars", "");
}

#endif

