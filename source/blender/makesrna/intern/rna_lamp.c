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

#include "DNA_lamp_types.h"

#include "WM_types.h"

#ifdef RNA_RUNTIME

static void rna_Lamp_buffer_size_set(PointerRNA *ptr, int value)
{
	Lamp *la= (Lamp*)ptr->data;

	CLAMP(value, 512, 10240);
	la->bufsize= value;
	la->bufsize &= (~15); /* round to multiple of 16 */
}

static void *rna_Lamp_sunsky_settings_get(PointerRNA *ptr)
{
	return ptr->id.data;
}


#else

static void rna_def_lamp_sunsky_settings(BlenderRNA *brna)
{
	StructRNA *srna;
	PropertyRNA *prop;

	static EnumPropertyItem prop_skycolorspace_items[] = {
		{0, "SMPTE", "SMPTE", ""},
		{1, "REC709", "REC709", ""},
		{2, "CIE", "CIE", ""},
		{0, NULL, NULL, NULL}};
		
	static EnumPropertyItem prop_blendmode_items[] = {
		{0, "MIX", "Mix", ""},
		{1, "ADD", "Add", ""},
		{2, "MULTIPLY", "Multiply", ""},
		{3, "SUBTRACT", "Subtract", ""},
		{4, "SCREEN", "Screen", ""},
		{5, "DIVIDE", "Divide", ""},
		{6, "DIFFERENCE", "Difference", ""},
		{7, "DARKEN", "Darken", ""},
		{8, "LIGHTEN", "Lighten", ""},
		{9, "OVERLAY", "Overlay", ""},
		{10, "DODGE", "Dodge", ""},
		{11, "BURN", "Burn", ""},
		{12, "HUE", "Hue", ""},
		{13, "SATURATION", "Saturation", ""},
		{14, "VALUE", "Value", ""},
		{15, "COLOR", "Color", ""},
		{0, NULL, NULL, NULL}};
		
	srna= RNA_def_struct(brna, "SunskySettings", NULL);
	RNA_def_struct_sdna(srna, "Lamp");
	RNA_def_struct_ui_text(srna, "Sun/Sky Settings", "Sun/Sky related settings for the lamp.");
		
	prop= RNA_def_property(srna, "sky_colorspace", PROP_ENUM, PROP_NONE);
	RNA_def_property_enum_items(prop, prop_skycolorspace_items);
	RNA_def_property_ui_text(prop, "Sky Color Space", "");
	RNA_def_property_update(prop, NC_LAMP|ND_SKY, NULL);

	prop= RNA_def_property(srna, "sky_blend_type", PROP_ENUM, PROP_NONE);
	RNA_def_property_enum_sdna(prop, NULL, "skyblendtype");
	RNA_def_property_enum_items(prop, prop_blendmode_items);
	RNA_def_property_ui_text(prop, "Sky Blend Mode", "Blend mode for combining sun sky with world sky");
	RNA_def_property_update(prop, NC_LAMP|ND_SKY, NULL);
	
	/* Number values */
	
	prop= RNA_def_property(srna, "horizon_brightness", PROP_FLOAT, PROP_NONE);
	RNA_def_property_range(prop, 0.0f, 20.0f);
	RNA_def_property_ui_text(prop, "Horizon Brightness", "horizon brightness");
	RNA_def_property_update(prop, NC_LAMP|ND_SKY, NULL);

	prop= RNA_def_property(srna, "spread", PROP_FLOAT, PROP_NONE);
	RNA_def_property_range(prop, 0.0f, 10.0f);
	RNA_def_property_ui_text(prop, "Horizon Spread", "horizon Spread");
	RNA_def_property_update(prop, NC_LAMP|ND_SKY, NULL);

	prop= RNA_def_property(srna, "sun_brightness", PROP_FLOAT, PROP_NONE);
	RNA_def_property_range(prop, 0.0f, 10.0f);
	RNA_def_property_ui_text(prop, "Sun Brightness", "Sun Brightness");
	RNA_def_property_update(prop, NC_LAMP|ND_SKY, NULL);

	prop= RNA_def_property(srna, "sun_size", PROP_FLOAT, PROP_NONE);
	RNA_def_property_range(prop, 0.0f, 10.0f);
	RNA_def_property_ui_text(prop, "Sun Size", "Sun Size");
	RNA_def_property_update(prop, NC_LAMP|ND_SKY, NULL);

  	prop= RNA_def_property(srna, "backscattered_light", PROP_FLOAT, PROP_NONE);
	RNA_def_property_range(prop, 0.0f, 1.0f);
	RNA_def_property_ui_text(prop, "Back Light", "Backscatter Light");
	RNA_def_property_update(prop, NC_LAMP|ND_SKY, NULL);

	prop= RNA_def_property(srna, "sun_intensity", PROP_FLOAT, PROP_NONE);
	RNA_def_property_range(prop, 0.0f, 10.0f);
	RNA_def_property_ui_text(prop, "Sun Intensity", "Sun Intensity");
	RNA_def_property_update(prop, NC_LAMP|ND_SKY, NULL);

	prop= RNA_def_property(srna, "atm_turbidity", PROP_FLOAT, PROP_NONE);
	RNA_def_property_range(prop, 0.0f, 30.0f);
	RNA_def_property_ui_text(prop, "Turbidity", "Sky Turbidity");
	RNA_def_property_update(prop, NC_LAMP|ND_SKY, NULL);

	prop= RNA_def_property(srna, "atm_inscattering_factor", PROP_FLOAT, PROP_NONE);
	RNA_def_property_range(prop, 0.0f, 1.0f);
	RNA_def_property_ui_text(prop, "Inscatter", "Scatter contribution factor");
	RNA_def_property_update(prop, NC_LAMP|ND_SKY, NULL);

	prop= RNA_def_property(srna, "atm_extinction_factor", PROP_FLOAT, PROP_NONE);
	RNA_def_property_range(prop, 0.0f, 1.0f);
	RNA_def_property_ui_text(prop, "Extinction", "Extinction scattering contribution factor");
	RNA_def_property_update(prop, NC_LAMP|ND_SKY, NULL);

	prop= RNA_def_property(srna, "atm_distance_factor", PROP_FLOAT, PROP_NONE);
	RNA_def_property_range(prop, 0.0f, 500.0f);
	RNA_def_property_ui_text(prop, "Distance", "Multiplier to convert blender units to physical distance");
	RNA_def_property_update(prop, NC_LAMP|ND_SKY, NULL);

	prop= RNA_def_property(srna, "sky_blend_factor", PROP_FLOAT, PROP_NONE);
	RNA_def_property_float_sdna(prop, NULL, "skyblendfac");
	RNA_def_property_range(prop, 0.0f, 2.0f);
	RNA_def_property_ui_text(prop, "Sky Blend Factor", "Blend factor with sky");
	RNA_def_property_update(prop, NC_LAMP|ND_SKY, NULL);

	prop= RNA_def_property(srna, "sky_exposure", PROP_FLOAT, PROP_NONE);
	RNA_def_property_range(prop, 0.0f, 20.0f);
	RNA_def_property_ui_text(prop, "Sky Exposure", "Strength of sky shading exponential exposure correction.");
	RNA_def_property_update(prop, NC_LAMP|ND_SKY, NULL);

	/* boolean */
	
	prop= RNA_def_property(srna, "sky", PROP_BOOLEAN, PROP_NONE);
	RNA_def_property_boolean_sdna(prop, NULL, "sun_effect_type", LA_SUN_EFFECT_SKY);
	RNA_def_property_ui_text(prop, "Sky", "Apply sun effect on sky");
	RNA_def_property_update(prop, NC_LAMP|ND_SKY, NULL);

	prop= RNA_def_property(srna, "atmosphere", PROP_BOOLEAN, PROP_NONE);
	RNA_def_property_boolean_sdna(prop, NULL, "sun_effect_type", LA_SUN_EFFECT_AP);
	RNA_def_property_ui_text(prop, "Atmosphere", "Apply sun effect on Atmosphere");
	RNA_def_property_update(prop, NC_LAMP|ND_SKY, NULL);
}

void rna_def_lamp(BlenderRNA *brna)
{
	StructRNA *srna;
	PropertyRNA *prop;
	static EnumPropertyItem prop_type_items[] = {
		{LA_LOCAL, "OMNI", "Omni", ""},
		{LA_SUN, "SUN", "Sun", ""},
		{LA_SPOT, "SPOT", "Spot", ""},
		{LA_HEMI, "HEMI", "Hemi", ""},
		{LA_AREA, "AREA", "Area", ""},
		{0, NULL, NULL, NULL}};
	static EnumPropertyItem prop_shadow_items[] = {
		{0, "NOSHADOW", "No Shadow", ""},
		{LA_SHAD_BUF, "BUFSHADOW", "Buffer Shadow", "Lets spotlight produce shadows using shadow buffer."},
		{LA_SHAD_RAY, "RAYSHADOW", "Ray Shadow", "Use ray tracing for shadow."},
		{0, NULL, NULL, NULL}};
	static EnumPropertyItem prop_raysampmethod_items[] = {
		{LA_SAMP_CONSTANT, "CONSTANT_JITTERED", "Constant Jittered", ""},
		{LA_SAMP_HALTON, "ADAPTIVE_QMC", "Adaptive QMC", ""},
		{LA_SAMP_HAMMERSLEY, "CONSTANT_QMC", "Constant QMC", ""},
		{0, NULL, NULL, NULL}};
	static EnumPropertyItem prop_areashape_items[] = {
		{LA_AREA_SQUARE, "SQUARE", "Square", ""},
		{LA_AREA_RECT, "RECTANGLE", "Rectangle", ""},
		{0, NULL, NULL, NULL}};
	static EnumPropertyItem prop_shadbuftype_items[] = {
		{LA_SHADBUF_REGULAR	, "REGULAR", "Classical", "Use the Classic Buffer type"},
		{LA_SHADBUF_IRREGULAR, "IRREGULAR", "Irregular", "Use the Irregular Shadow Buffer type."},
		{LA_SHADBUF_HALFWAY, "HALFWAY", "Classic-Halfway", "Use the Classic-Halfway Buffer type."},
		{0, NULL, NULL, NULL}};
	static EnumPropertyItem prop_shadbuffiltertype_items[] = {
		{LA_SHADBUF_BOX	, "BOX", "Box", "Use the Box filter"},
		{LA_SHADBUF_TENT, "TENT", "Tent", "Use the Tent Filter."},
		{LA_SHADBUF_GAUSS, "GAUSS", "Gauss", "Use the Gauss filter."},
		{0, NULL, NULL, NULL}};
	static EnumPropertyItem prop_numbuffer_items[] = {
		{1, "1BUFF", "1", "Sample 1 Shadow Buffer."},
		{4, "4BUFF", "4", "Sample 4 Shadow Buffers."},
		{9, "9BUFF", "9", "Sample 9 Shadow Buffers."},
		{0, NULL, NULL, NULL}};
	static EnumPropertyItem prop_fallofftype_items[] = {
		{LA_FALLOFF_CONSTANT, "CONSTANT", "Constant", ""},
		{LA_FALLOFF_INVLINEAR, "INVLINEAR", "Inverse Linear", ""},
		{LA_FALLOFF_INVSQUARE, "INVSQUARE", "Inverse Square", ""},
		{LA_FALLOFF_CURVE, "CURVE", "Custom Curve", ""},
		{LA_FALLOFF_SLIDERS, "SLIDERS", "Lin/Quad Weighted", ""},
		{0, NULL, NULL, NULL}};

	srna= RNA_def_struct(brna, "Lamp", "ID");
	RNA_def_struct_ui_text(srna, "Lamp", "DOC_BROKEN");

	/* Enums */
	prop= RNA_def_property(srna, "type", PROP_ENUM, PROP_NONE);
	RNA_def_property_enum_items(prop, prop_type_items);
	RNA_def_property_ui_text(prop, "Type", "Type of Lamp.");
	RNA_def_property_update(prop, NC_LAMP|ND_LIGHTING_DRAW, NULL);

	prop= RNA_def_property(srna, "area_shape", PROP_ENUM, PROP_NONE);
	RNA_def_property_enum_items(prop, prop_areashape_items);
	RNA_def_property_ui_text(prop, "Area Shape", "Shape of the Area lamp");
	RNA_def_property_update(prop, NC_LAMP|ND_LIGHTING_DRAW, NULL);

	prop= RNA_def_property(srna, "ray_samp_method", PROP_ENUM, PROP_NONE);
	RNA_def_property_enum_items(prop, prop_raysampmethod_items);
	RNA_def_property_ui_text(prop, "Ray Sample Method", "The Method in how rays are sampled");
	RNA_def_property_update(prop, NC_LAMP|ND_LIGHTING, NULL);

	prop= RNA_def_property(srna, "buffer_type", PROP_ENUM, PROP_NONE);
	RNA_def_property_enum_sdna(prop, NULL, "buftype");
	RNA_def_property_enum_items(prop, prop_shadbuftype_items);
	RNA_def_property_ui_text(prop, "Buffer Type", "Type of Shadow Buffer.");
	RNA_def_property_update(prop, NC_LAMP|ND_LIGHTING, NULL);

	prop= RNA_def_property(srna, "filter_type", PROP_ENUM, PROP_NONE);
	RNA_def_property_enum_sdna(prop, NULL, "filtertype");
	RNA_def_property_enum_items(prop, prop_shadbuffiltertype_items);
	RNA_def_property_ui_text(prop, "Filter Type", "Type of Shadow Buffer Filter.");
	RNA_def_property_update(prop, NC_LAMP|ND_LIGHTING, NULL);

	prop= RNA_def_property(srna, "buffers", PROP_ENUM, PROP_NONE);
	RNA_def_property_enum_items(prop, prop_numbuffer_items);
	RNA_def_property_ui_text(prop, "Sample Buffers", "Number of Buffers to sample.");
	RNA_def_property_update(prop, NC_LAMP|ND_LIGHTING, NULL);

	prop= RNA_def_property(srna, "falloff_type", PROP_ENUM, PROP_NONE);
	RNA_def_property_flag(prop, PROP_NOT_EDITABLE); /* needs to be able to create curve mapping */
	RNA_def_property_enum_items(prop, prop_fallofftype_items);
	RNA_def_property_ui_text(prop, "Falloff Type", "Intensity Decay with distance.");
	RNA_def_property_update(prop, NC_LAMP|ND_LIGHTING, NULL);
	
	prop= RNA_def_property(srna, "falloff_curve", PROP_POINTER, PROP_NONE);
	RNA_def_property_pointer_sdna(prop, NULL, "curfalloff");
	RNA_def_property_ui_text(prop, "Falloff Curve", "Custom Lamp Falloff Curve");
	RNA_def_property_update(prop, NC_LAMP|ND_LIGHTING, NULL);

	/* Number values */
	prop= RNA_def_property(srna, "distance", PROP_FLOAT, PROP_NONE);
	RNA_def_property_float_sdna(prop, NULL, "dist");
	RNA_def_property_range(prop, 0.0f, 9999.0f);
	RNA_def_property_ui_text(prop, "Distance", "Distance that the lamp emits light.");
	RNA_def_property_update(prop, NC_LAMP|ND_LIGHTING_DRAW, NULL);

	prop= RNA_def_property(srna, "linear_attenuation", PROP_FLOAT, PROP_NONE);
	RNA_def_property_float_sdna(prop, NULL, "att1");
	RNA_def_property_range(prop, 0.0f, 1.0f);
	RNA_def_property_ui_text(prop, "Linear Attenuation", "Linear distance attentuation.");
	RNA_def_property_update(prop, NC_LAMP|ND_LIGHTING, NULL);

	prop= RNA_def_property(srna, "quadratic_attenuation", PROP_FLOAT, PROP_NONE);
	RNA_def_property_float_sdna(prop, NULL, "att2");
	RNA_def_property_range(prop, 0.0f, 1.0f);
	RNA_def_property_ui_text(prop, "Quadratic Attenuation", "Quadratic distance attentuation.");
	RNA_def_property_update(prop, NC_LAMP|ND_LIGHTING, NULL);

	prop= RNA_def_property(srna, "spot_blend", PROP_FLOAT, PROP_NONE);
	RNA_def_property_float_sdna(prop, NULL, "spotblend");
	RNA_def_property_range(prop, 0.0f ,1.0f);
	RNA_def_property_ui_text(prop, "Spot Blend", "The softeness of the spotlight edge.");
	RNA_def_property_update(prop, NC_LAMP|ND_LIGHTING_DRAW, NULL);

	prop= RNA_def_property(srna, "spot_size", PROP_FLOAT, PROP_NONE);
	RNA_def_property_float_sdna(prop, NULL, "spotsize");
	RNA_def_property_range(prop, 1.0f ,180.0f);
	RNA_def_property_ui_text(prop, "Spot Size", "The angle of the spotlight beam in degrees.");
	RNA_def_property_update(prop, NC_LAMP|ND_LIGHTING_DRAW, NULL);

	prop= RNA_def_property(srna, "clip_start", PROP_FLOAT, PROP_NONE);
	RNA_def_property_float_sdna(prop, NULL, "clipsta");
	RNA_def_property_range(prop, 0.0f, 9999.0f);
	RNA_def_property_ui_text(prop, "Clip Start", "Distance that the buffer calculations start.");
	RNA_def_property_update(prop, NC_LAMP|ND_LIGHTING_DRAW, NULL);

	prop= RNA_def_property(srna, "clip_end", PROP_FLOAT, PROP_NONE);
	RNA_def_property_float_sdna(prop, NULL, "clipend");
	RNA_def_property_range(prop, 0.0f, 9999.0f);
	RNA_def_property_ui_text(prop, "Clip End", "Distance that the buffer calculations finish.");
	RNA_def_property_update(prop, NC_LAMP|ND_LIGHTING_DRAW, NULL);

	prop= RNA_def_property(srna, "bias", PROP_FLOAT, PROP_NONE);
	RNA_def_property_range(prop, 0.0f, 5.0f);
	RNA_def_property_ui_text(prop, "Bias", "Shadow Map sampling bias.");
	RNA_def_property_update(prop, NC_LAMP|ND_LIGHTING, NULL);

	prop= RNA_def_property(srna, "soft", PROP_FLOAT, PROP_NONE);
	RNA_def_property_range(prop, 0.0f, 100.0f);
	RNA_def_property_ui_text(prop, "Soft", "Size of shadow sampling area.");
	RNA_def_property_update(prop, NC_LAMP|ND_LIGHTING, NULL);

	prop= RNA_def_property(srna, "samples", PROP_INT, PROP_NONE);
	RNA_def_property_int_sdna(prop, NULL, "samp");
	RNA_def_property_range(prop, 1,16);
	RNA_def_property_ui_text(prop, "Samples", "Number of shadow map samples.");
	RNA_def_property_update(prop, NC_LAMP|ND_LIGHTING, NULL);

	prop= RNA_def_property(srna, "energy", PROP_FLOAT, PROP_NONE);
	RNA_def_property_range(prop, 0.0f, 10.0f);
	RNA_def_property_ui_text(prop, "Energy", "Amount of light that the lamp emits.");
	RNA_def_property_update(prop, NC_LAMP|ND_LIGHTING, NULL);

	prop= RNA_def_property(srna, "ray_samples", PROP_INT, PROP_NONE);
	RNA_def_property_int_sdna(prop, NULL, "ray_samp");
	RNA_def_property_range(prop, 1, 16);
	RNA_def_property_ui_text(prop, "Ray Samples", "Amount of samples taken extra (samples x samples).");
	RNA_def_property_update(prop, NC_LAMP|ND_LIGHTING, NULL);

	prop= RNA_def_property(srna, "ray_sampy", PROP_INT, PROP_NONE);
	RNA_def_property_range(prop, 1,16);
	RNA_def_property_ui_text(prop, "Ray Samples Y", "Amount of samples taken extra (samples x samples).");
	RNA_def_property_update(prop, NC_LAMP|ND_LIGHTING, NULL);

	prop= RNA_def_property(srna, "area_size", PROP_FLOAT, PROP_NONE);
	RNA_def_property_range(prop, 0.0f, 100.0f);
	RNA_def_property_ui_text(prop, "Area Size", "Size of the area of the Area Lamp.");
	RNA_def_property_update(prop, NC_LAMP|ND_LIGHTING_DRAW, NULL);

	prop= RNA_def_property(srna, "area_sizey", PROP_FLOAT, PROP_NONE);
	RNA_def_property_range(prop, 0.0f, 100.0f);
	RNA_def_property_ui_text(prop, "Area Size Y", "Size of the area of the Area Lamp.");
	RNA_def_property_update(prop, NC_LAMP|ND_LIGHTING_DRAW, NULL);

	prop= RNA_def_property(srna, "adapt_thresh", PROP_FLOAT, PROP_NONE);
	RNA_def_property_range(prop, 0.0f, 1.0f);
	RNA_def_property_ui_text(prop, "Adapt Threshold", "Threshold for Adaptive Sampling.");
	RNA_def_property_update(prop, NC_LAMP|ND_LIGHTING, NULL);

	prop= RNA_def_property(srna, "buffer_size", PROP_INT, PROP_NONE);
	RNA_def_property_int_sdna(prop, NULL, "bufsize");
	RNA_def_property_range(prop, 512, 10240);
	RNA_def_property_ui_text(prop, "Buffer Size", "Sets the size of the shadow buffer to nearest multiple of 16");
	RNA_def_property_int_funcs(prop, NULL, "rna_Lamp_buffer_size_set", NULL);
	RNA_def_property_update(prop, NC_LAMP|ND_LIGHTING, NULL);

	prop= RNA_def_property(srna, "halo_intensity", PROP_FLOAT, PROP_NONE);
	RNA_def_property_float_sdna(prop, NULL, "haint");
	RNA_def_property_range(prop, 0.0f, 5.0f);
	RNA_def_property_ui_text(prop, "Halo Intensity", "Intensity of Spot Halo");
	RNA_def_property_update(prop, NC_LAMP|ND_LIGHTING, NULL);

	/*short sky_colorspace, pad4;*/

	/* Colors */
	prop= RNA_def_property(srna, "color", PROP_FLOAT, PROP_COLOR);
	RNA_def_property_float_sdna(prop, NULL, "r");
	RNA_def_property_array(prop, 3);
	RNA_def_property_ui_text(prop, "Color", "");
	RNA_def_property_update(prop, NC_LAMP|ND_LIGHTING, NULL);

	prop= RNA_def_property(srna, "shadow_color", PROP_FLOAT, PROP_COLOR);
	RNA_def_property_float_sdna(prop, NULL, "shdwr");
	RNA_def_property_array(prop, 3);
	RNA_def_property_ui_text(prop, "Shadow Color", "");
	RNA_def_property_update(prop, NC_LAMP|ND_LIGHTING, NULL);

	/* Booleans */
	prop= RNA_def_property(srna, "auto_clip_start", PROP_BOOLEAN, PROP_NONE);
	RNA_def_property_boolean_sdna(prop, NULL, "bufflag", LA_SHADBUF_AUTO_START);
	RNA_def_property_ui_text(prop, "Autoclip Start", "Automatically Sets Clip start to the nearest pixel.");
	RNA_def_property_update(prop, NC_LAMP|ND_LIGHTING_DRAW, NULL);

	prop= RNA_def_property(srna, "auto_clip_end", PROP_BOOLEAN, PROP_NONE);
	RNA_def_property_boolean_sdna(prop, NULL, "bufflag", LA_SHADBUF_AUTO_END);
	RNA_def_property_ui_text(prop, "Autoclip End", "Automatically Sets Clip end to the farthest away pixel.");
	RNA_def_property_update(prop, NC_LAMP|ND_LIGHTING_DRAW, NULL);

	prop= RNA_def_property(srna, "umbra", PROP_BOOLEAN, PROP_NONE);
	RNA_def_property_boolean_sdna(prop, NULL, "ray_samp_type", LA_SAMP_UMBRA);
	RNA_def_property_ui_text(prop, "Umbra", "Emphasise parts in full shadow.");
	RNA_def_property_update(prop, NC_LAMP|ND_LIGHTING, NULL);

	prop= RNA_def_property(srna, "dither", PROP_BOOLEAN, PROP_NONE);
	RNA_def_property_boolean_sdna(prop, NULL, "ray_samp_type", LA_SAMP_DITHER);
	RNA_def_property_ui_text(prop, "Dither", "Use 2x2 dithering for sampling.");
	RNA_def_property_update(prop, NC_LAMP|ND_LIGHTING, NULL);

	prop= RNA_def_property(srna, "jitter", PROP_BOOLEAN, PROP_NONE);
	RNA_def_property_boolean_sdna(prop, NULL, "ray_samp_type", LA_SAMP_JITTER);
	RNA_def_property_ui_text(prop, "Jitter", "Use noise for sampling.");
	RNA_def_property_update(prop, NC_LAMP|ND_LIGHTING, NULL);

	/* mode */
	prop= RNA_def_property(srna, "halo", PROP_BOOLEAN, PROP_NONE);
	RNA_def_property_boolean_sdna(prop, NULL, "mode", LA_HALO);
	RNA_def_property_ui_text(prop, "Halo", "Lamp creates a halo.");
	RNA_def_property_update(prop, NC_LAMP|ND_LIGHTING, NULL);

	prop= RNA_def_property(srna, "layer", PROP_BOOLEAN, PROP_NONE);
	RNA_def_property_boolean_sdna(prop, NULL, "mode", LA_LAYER);
	RNA_def_property_ui_text(prop, "Layer", "Lamp is only used on the Scene layer the lamp is on.");
	RNA_def_property_update(prop, NC_LAMP|ND_LIGHTING, NULL);

	prop= RNA_def_property(srna, "negative", PROP_BOOLEAN, PROP_NONE);
	RNA_def_property_boolean_sdna(prop, NULL, "mode", LA_NEG);
	RNA_def_property_ui_text(prop, "Negative", "Lamp casts negative light.");
	RNA_def_property_update(prop, NC_LAMP|ND_LIGHTING, NULL);

	prop= RNA_def_property(srna, "specular", PROP_BOOLEAN, PROP_NONE);
	RNA_def_property_boolean_negative_sdna(prop, NULL, "mode", LA_NO_SPEC);
	RNA_def_property_ui_text(prop, "Specular", "Lamp creates specular highlights.");
	RNA_def_property_update(prop, NC_LAMP|ND_LIGHTING, NULL);

	prop= RNA_def_property(srna, "diffuse", PROP_BOOLEAN, PROP_NONE);
	RNA_def_property_boolean_negative_sdna(prop, NULL, "mode", LA_NO_DIFF);
	RNA_def_property_ui_text(prop, "Diffuse", "Lamp does diffuse shading.");
	RNA_def_property_update(prop, NC_LAMP|ND_LIGHTING, NULL);

	prop= RNA_def_property(srna, "only_shadow", PROP_BOOLEAN, PROP_NONE);
	RNA_def_property_boolean_sdna(prop, NULL, "mode", LA_ONLYSHADOW);
	RNA_def_property_ui_text(prop, "Only Shadow", "Lamp only creates shadows.");
	RNA_def_property_update(prop, NC_LAMP|ND_LIGHTING, NULL);

	prop= RNA_def_property(srna, "shadow", PROP_ENUM, PROP_NONE);
	RNA_def_property_enum_bitflag_sdna(prop, NULL, "mode");
	RNA_def_property_enum_items(prop, prop_shadow_items);
	RNA_def_property_ui_text(prop, "Shadow", "Method to compute lamp shadow.");
	RNA_def_property_update(prop, NC_LAMP|ND_LIGHTING, NULL);

	prop= RNA_def_property(srna, "sphere", PROP_BOOLEAN, PROP_NONE);
	RNA_def_property_boolean_sdna(prop, NULL, "mode", LA_SPHERE);
	RNA_def_property_ui_text(prop, "Sphere", "Sets light intensity to zero beyond lamp distance.");
	RNA_def_property_update(prop, NC_LAMP|ND_LIGHTING_DRAW, NULL);

	prop= RNA_def_property(srna, "square", PROP_BOOLEAN, PROP_NONE);
	RNA_def_property_boolean_sdna(prop, NULL, "mode", LA_SQUARE);
	RNA_def_property_ui_text(prop, "Square", "Casts a square spot light shape");
	RNA_def_property_update(prop, NC_LAMP|ND_LIGHTING_DRAW, NULL);
	
	/* sun/sky */
	prop= RNA_def_property(srna, "sunsky", PROP_POINTER, PROP_NONE);
	RNA_def_property_struct_type(prop, "SunskySettings");
	RNA_def_property_pointer_funcs(prop, "rna_Lamp_sunsky_settings_get", NULL, NULL);
	RNA_def_property_ui_text(prop, "Sun/Sky Settings", "Sun/Sky related settings for the lamp.");

}

void RNA_def_lamp(BlenderRNA *brna)
{
	rna_def_lamp(brna);
	rna_def_lamp_sunsky_settings(brna);
}

#endif

