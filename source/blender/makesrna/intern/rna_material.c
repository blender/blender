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
 * Contributor(s): Blender Foundation (2008), Nathan Letwory
 *
 * ***** END GPL LICENSE BLOCK *****
 */

#include <stdlib.h>

#include "RNA_define.h"
#include "RNA_types.h"

#include "rna_internal.h"

#include "DNA_material_types.h"

#ifdef RNA_RUNTIME

static void rna_Material_mode_halo_set(PointerRNA *ptr, int value)
{
	Material *mat= (Material*)ptr->data;
	
	if(value)
		mat->mode |= MA_HALO;
	else
		mat->mode &= ~(MA_HALO|MA_STAR|MA_HALO_XALPHA|MA_ZINV|MA_ENV);
}

#else

static void rna_def_material_colors(StructRNA *srna, PropertyRNA *prop)
{
	static EnumPropertyItem prop_type_items[] = {
		{MA_RGB, "RGB", "RGB", ""},
		/* not used in blender yet
		{MA_CMYK, "CMYK", "CMYK", ""}, 
		{MA_YUV, "YUV", "YUV", ""}, */
		{MA_HSV, "HSV", "HSV", ""},
		{0, NULL, NULL, NULL}};
	
	prop= RNA_def_property(srna, "color_model", PROP_ENUM, PROP_NONE);
	RNA_def_property_enum_sdna(prop, NULL, "colormodel");
	RNA_def_property_enum_items(prop, prop_type_items);
	RNA_def_property_ui_text(prop, "Color Model", "");
	
	prop= RNA_def_property(srna, "diffuse_color", PROP_FLOAT, PROP_COLOR);
	RNA_def_property_float_sdna(prop, NULL, "r");
	RNA_def_property_array(prop, 3);
	RNA_def_property_ui_text(prop, "Diffuse Color", "");
	RNA_def_property_ui_range(prop, 0.0f , 1.0f, 10.0f, 3);
	
	prop= RNA_def_property(srna, "specular_color", PROP_FLOAT, PROP_COLOR);
	RNA_def_property_float_sdna(prop, NULL, "specr");
	RNA_def_property_array(prop, 3);
	RNA_def_property_ui_text(prop, "Specular Color", "");
	RNA_def_property_ui_range(prop, 0.0f , 1.0f, 10.0f, 3);
	
	prop= RNA_def_property(srna, "mirror_color", PROP_FLOAT, PROP_COLOR);
	RNA_def_property_float_sdna(prop, NULL, "mirr");
	RNA_def_property_array(prop, 3);
	RNA_def_property_ui_text(prop, "Mirror Color", "");
	RNA_def_property_ui_range(prop, 0.0f , 1.0f, 10.0f, 3);
		
	prop= RNA_def_property(srna, "ambient_color", PROP_FLOAT, PROP_COLOR);
	RNA_def_property_float_sdna(prop, NULL, "ambr");
	RNA_def_property_array(prop, 3);
	RNA_def_property_ui_text(prop, "Ambient Color", "");
	RNA_def_property_ui_range(prop, 0.0f , 1.0f, 10.0f, 3);
	
	prop= RNA_def_property(srna, "alpha", PROP_FLOAT, PROP_NONE);
	RNA_def_property_range(prop, 0.0f, 1.0f);
	RNA_def_property_ui_text(prop, "Alpha", "");
}

static void rna_def_material_diffuse(StructRNA *srna, PropertyRNA *prop)
{
	static EnumPropertyItem prop_diff_shader_items[] = {
		{MA_DIFF_LAMBERT, "DIFF_LAMBERT", "Lambert", ""},
		{MA_DIFF_ORENNAYAR, "DIFF_ORENNAYAR", "Oren-Nayar", ""},
		{MA_DIFF_TOON, "DIFF_TOON", "Toon", ""},
		{MA_DIFF_MINNAERT, "DIFF_MINNAERT", "Minnaert", ""},
		{MA_DIFF_FRESNEL, "DIFF_FRESNEL", "Fresnel", ""},
		{0, NULL, NULL, NULL}};
	
	prop= RNA_def_property(srna, "diffuse_shader", PROP_ENUM, PROP_NONE);
	RNA_def_property_enum_sdna(prop, NULL, "diff_shader");
	RNA_def_property_enum_items(prop, prop_diff_shader_items);
	RNA_def_property_ui_text(prop, "Diffuse Shader Model", "");
	
	prop= RNA_def_property(srna, "diffuse_reflection", PROP_FLOAT, PROP_NONE);
	RNA_def_property_float_sdna(prop, NULL, "ref");
	RNA_def_property_range(prop, 0.0f, 1.0f);
	RNA_def_property_ui_text(prop, "Diffuse Reflection", "Amount of diffuse reflection.");
	
	prop= RNA_def_property(srna, "roughness", PROP_FLOAT, PROP_NONE);
	RNA_def_property_range(prop, 0.0f, 3.14f);
	RNA_def_property_ui_text(prop, "Roughness", "Oren-Nayar Roughness");
	
	prop= RNA_def_property(srna, "params1_4", PROP_FLOAT, PROP_NONE);
	RNA_def_property_float_sdna(prop, NULL, "param");
	RNA_def_property_array(prop, 4);
	RNA_def_property_range(prop, 0.0f, 5.0f);
	RNA_def_property_ui_text(prop, "Params 1-4", "Parameters used for diffuse and specular Toon, and diffuse Fresnel shaders. Check documentation for details.");
	
	prop= RNA_def_property(srna, "darkness", PROP_FLOAT, PROP_NONE);
	RNA_def_property_range(prop, 0.0f, 2.0f);
	RNA_def_property_ui_text(prop, "Darkness", "Minnaert darkness.");
}

static void rna_def_material_raymirror(StructRNA *srna, PropertyRNA *prop)
{
	static EnumPropertyItem prop_fadeto_mir_items[] = {
		{MA_RAYMIR_FADETOSKY, "RAYMIR_FADETOSKY", "Fade to sky color", ""},
		{MA_RAYMIR_FADETOMAT, "RAYMIR_FADETOMAT", "Fade to material color", ""},
		{0, NULL, NULL, NULL}};
	
	prop= RNA_def_property(srna, "mode_ray_mirror", PROP_BOOLEAN, PROP_NONE);
	RNA_def_property_boolean_sdna(prop, NULL, "mode", MA_RAYMIRROR); /* use bitflags */
	RNA_def_property_ui_text(prop, "Ray Mirror Mode", "Toggle raytrace mirror.");
	
	prop= RNA_def_property(srna, "ray_mirror", PROP_FLOAT, PROP_NONE);
	RNA_def_property_range(prop, 0.0f, 1.0f);
	RNA_def_property_ui_text(prop, "Ray Mirror", "Sets the amount mirror reflection for raytrace.");
	
	prop= RNA_def_property(srna, "ray_mirror_fresnel", PROP_FLOAT, PROP_NONE);
	RNA_def_property_float_sdna(prop, NULL, "fresnel_mir");
	RNA_def_property_range(prop, 0.0f, 5.0f);
	RNA_def_property_ui_text(prop, "Ray Mirror Fresnel", "Power of Fresnel for mirror reflection.");
	
	prop= RNA_def_property(srna, "ray_mirror_fresnel_fac", PROP_FLOAT, PROP_NONE);
	RNA_def_property_float_sdna(prop, NULL, "fresnel_mir_i");
	RNA_def_property_range(prop, 0.0f, 1.0f);
	RNA_def_property_ui_text(prop, "Ray Mirror Fresnel Factor", "Blending factor for Fresnel.");
	
	prop= RNA_def_property(srna, "ray_mirror_gloss", PROP_FLOAT, PROP_NONE);
	RNA_def_property_float_sdna(prop, NULL, "gloss_mir");
	RNA_def_property_range(prop, 0.0f, 1.0f);
	RNA_def_property_ui_text(prop, "Ray Mirror Gloss", "The shininess of the reflection. Values < 1.0 give diffuse, blurry reflections.");
	
	prop= RNA_def_property(srna, "ray_mirror_gloss_aniso", PROP_FLOAT, PROP_NONE);
	RNA_def_property_float_sdna(prop, NULL, "aniso_gloss_mir");
	RNA_def_property_range(prop, 0.0f, 1.0f);
	RNA_def_property_ui_text(prop, "Ray Mirror Gloss Aniso", "The shape of the reflection, from 0.0 (circular) to 1.0 (fully stretched along the tangent.");
		
	prop= RNA_def_property(srna, "ray_mirror_gloss_samples", PROP_FLOAT, PROP_NONE);
	RNA_def_property_float_sdna(prop, NULL, "samp_gloss_mir");
	RNA_def_property_range(prop, 0.0f, 1024.0f);
	RNA_def_property_ui_text(prop, "Ray Mirror Gloss Samples", "Number of cone samples averaged for blurry reflections.");
	
	prop= RNA_def_property(srna, "ray_mirror_adapt_thresh", PROP_FLOAT, PROP_NONE);
	RNA_def_property_float_sdna(prop, NULL, "adapt_thresh_mir");
	RNA_def_property_range(prop, 0.0f, 1.0f);
	RNA_def_property_ui_text(prop, "Ray Mirror Gloss Threshold", "Threshold for adaptive sampling. If a sample contributes less than this amount (as a percentage), sampling is stopped.");
	
	prop= RNA_def_property(srna, "ray_mirror_depth", PROP_INT, PROP_NONE);
	RNA_def_property_int_sdna(prop, NULL, "ray_depth");
	RNA_def_property_range(prop, 0, 10);
	RNA_def_property_ui_text(prop, "Ray Mirror Depth", "Maximum allowed number of light inter-reflections.");
	
	prop= RNA_def_property(srna, "ray_mirror_distance", PROP_FLOAT, PROP_NONE);
	RNA_def_property_float_sdna(prop, NULL, "dist_mir");
	RNA_def_property_range(prop, 0.0f, 10000.0f);
	RNA_def_property_ui_text(prop, "Ray Mirror Max Dist", "Maximum distance of reflected rays. Reflections further than this range fade to sky color or material color.");
	
	prop= RNA_def_property(srna, "ray_mirror_fadeto", PROP_ENUM, PROP_NONE);
	RNA_def_property_enum_sdna(prop, NULL, "fadeto_mir");
	RNA_def_property_enum_items(prop, prop_fadeto_mir_items);
	RNA_def_property_ui_text(prop, "Ray Mirror End Fade-out", "The color that rays with no intersection within the Max Distance take. Material color can be best for indoor scenes, sky color for outdoor.");
}

static void rna_def_material_raytra(StructRNA *srna, PropertyRNA *prop)
{
	prop= RNA_def_property(srna, "mode_ray_transparency", PROP_BOOLEAN, PROP_NONE);
	RNA_def_property_boolean_sdna(prop, NULL, "mode", MA_RAYTRANSP); /* use bitflags */
	RNA_def_property_ui_text(prop, "Ray Transparency Mode", "Enables raytracing for transparent refraction rendering.");
	
	prop= RNA_def_property(srna, "ray_transparency_ior", PROP_FLOAT, PROP_NONE);
	RNA_def_property_float_sdna(prop, NULL, "ang");
	RNA_def_property_range(prop, 1.0f, 3.0f);
	RNA_def_property_ui_text(prop, "Ray Transparency IOR", "Sets angular index of refraction for raytraced refraction.");
	
	prop= RNA_def_property(srna, "ray_transparency_fresnel", PROP_FLOAT, PROP_NONE);
	RNA_def_property_float_sdna(prop, NULL, "fresnel_tra");
	RNA_def_property_range(prop, 0.0f, 5.0f);
	RNA_def_property_ui_text(prop, "Ray Transparency Fresnel", "Power of Fresnel for transparency (Ray or ZTransp).");
	
	prop= RNA_def_property(srna, "ray_transparency_fresnel_fac", PROP_FLOAT, PROP_NONE);
	RNA_def_property_float_sdna(prop, NULL, "fresnel_tra_i");
	RNA_def_property_range(prop, 1.0f, 5.0f);
	RNA_def_property_ui_text(prop, "Ray Transparency Fresnel Factor", "Blending factor for Fresnel.");
	
	prop= RNA_def_property(srna, "ray_transparency_gloss", PROP_FLOAT, PROP_NONE);
	RNA_def_property_float_sdna(prop, NULL, "gloss_tra");
	RNA_def_property_range(prop, 0.0f, 1.0f);
	RNA_def_property_ui_text(prop, "Ray Transparency Gloss", "The clarity of the refraction. Values < 1.0 give diffuse, blurry refractions.");
	
	prop= RNA_def_property(srna, "ray_transparency_gloss_samples", PROP_FLOAT, PROP_NONE);
	RNA_def_property_float_sdna(prop, NULL, "samp_gloss_tra");
	RNA_def_property_range(prop, 0.0f, 1024.0f);
	RNA_def_property_ui_text(prop, "Ray Transparency Gloss Samples", "Number of cone samples averaged for blurry refractions.");
	
	prop= RNA_def_property(srna, "ray_transparency_adapt_thresh", PROP_FLOAT, PROP_NONE);
	RNA_def_property_float_sdna(prop, NULL, "adapt_thresh_tra");
	RNA_def_property_range(prop, 0.0f, 1.0f);
	RNA_def_property_ui_text(prop, "Ray Transparency Gloss Threshold", "Threshold for adaptive sampling. If a sample contributes less than this amount (as a percentage), sampling is stopped.");
	
	prop= RNA_def_property(srna, "ray_transparency_depth", PROP_INT, PROP_NONE);
	RNA_def_property_int_sdna(prop, NULL, "ray_depth_tra");
	RNA_def_property_range(prop, 0, 10);
	RNA_def_property_ui_text(prop, "Ray Transparency Depth", "Maximum allowed number of light inter-refractions.");
	
	prop= RNA_def_property(srna, "ray_transparency_filter", PROP_FLOAT, PROP_NONE);
	RNA_def_property_float_sdna(prop, NULL, "filter");
	RNA_def_property_range(prop, 0.0f, 1.0f);
	RNA_def_property_ui_text(prop, "Ray Transparency Filter", "Amount to blend in the material's diffuse color in raytraced transparency (simulating absorption).");
	
	prop= RNA_def_property(srna, "ray_transparency_limit", PROP_FLOAT, PROP_NONE);
	RNA_def_property_float_sdna(prop, NULL, "tx_limit");
	RNA_def_property_range(prop, 0.0f, 100.0f);
	RNA_def_property_ui_text(prop, "Ray Transparency Limit", "Maximum depth for light to travel through the transparent material before becoming fully filtered (0.0 is disabled).");
	
	prop= RNA_def_property(srna, "ray_transparency_falloff", PROP_FLOAT, PROP_NONE);
	RNA_def_property_float_sdna(prop, NULL, "tx_falloff");
	RNA_def_property_range(prop, 0.1f, 10.0f);
	RNA_def_property_ui_text(prop, "Ray Transparency Falloff", "Falloff power for transmissivity filter effect (1.0 is linear).");
	
	prop= RNA_def_property(srna, "ray_transparency_specular_opacity", PROP_FLOAT, PROP_NONE);
	RNA_def_property_float_sdna(prop, NULL, "spectra");
	RNA_def_property_range(prop, 0.0f, 1.0f);
	RNA_def_property_ui_text(prop, "Ray Transparency Specular Opacity", "Makes specular areas opaque on transparent materials.");
}

static void rna_def_material_halo(StructRNA *srna, PropertyRNA *prop)
{
	prop= RNA_def_property(srna, "mode_halo", PROP_BOOLEAN, PROP_NONE);
	RNA_def_property_boolean_sdna(prop, NULL, "mode", MA_HALO); /* use bitflags */
	RNA_def_property_ui_text(prop, "Halo Mode", "Enables halo rendering of material.");
	RNA_def_property_boolean_funcs(prop, NULL, "rna_Material_mode_halo_set");
	
	prop= RNA_def_property(srna, "halo_size", PROP_FLOAT, PROP_NONE);
	RNA_def_property_float_sdna(prop, NULL, "hasize");
	RNA_def_property_range(prop, 0.0f, 100.0f);
	RNA_def_property_ui_text(prop, "Halo Size", "Sets the dimension of the halo.");
	
	prop= RNA_def_property(srna, "halo_hardness", PROP_INT, PROP_NONE);
	RNA_def_property_int_sdna(prop, NULL, "har");
	RNA_def_property_range(prop, 0, 127);
	RNA_def_property_ui_text(prop, "Halo Hardness", "Sets the hardness of the halo.");
	
	prop= RNA_def_property(srna, "halo_add", PROP_FLOAT, PROP_NONE);
	RNA_def_property_float_sdna(prop, NULL, "add");
	RNA_def_property_range(prop, 0.0f, 1.0f);
	RNA_def_property_ui_text(prop, "Halo Add", "Sets the strength of the add effect.");
	
	prop= RNA_def_property(srna, "halo_rings", PROP_INT, PROP_NONE);
	RNA_def_property_int_sdna(prop, NULL, "ringc");
	RNA_def_property_range(prop, 0, 24);
	RNA_def_property_ui_text(prop, "Halo Rings", "Sets the number of rings rendered over the halo.");
	
	prop= RNA_def_property(srna, "halo_lines", PROP_INT, PROP_NONE);
	RNA_def_property_int_sdna(prop, NULL, "linec");
	RNA_def_property_range(prop, 0, 250);
	RNA_def_property_ui_text(prop, "Halo Lines", "Sets the number of star shaped lines rendered over the halo.");
	
	prop= RNA_def_property(srna, "halo_star_tips", PROP_INT, PROP_NONE);
	RNA_def_property_int_sdna(prop, NULL, "starc");
	RNA_def_property_range(prop, 3, 50);
	RNA_def_property_ui_text(prop, "Halo Star Tips", "Sets the number of points on the star shaped halo.");
	
	prop= RNA_def_property(srna, "halo_seed", PROP_INT, PROP_NONE);
	RNA_def_property_int_sdna(prop, NULL, "seed1");
	RNA_def_property_range(prop, 0, 255);
	RNA_def_property_ui_text(prop, "Halo Seed", "Randomizes ring dimension and line location.");
	
	prop= RNA_def_property(srna, "halo_flare_mode", PROP_BOOLEAN, PROP_NONE);
	RNA_def_property_boolean_sdna(prop, NULL, "mode", MA_HALO_FLARE); /* use bitflags */
	RNA_def_property_ui_text(prop, "Halo Mode Flare", "Renders halo as a lensflare.");
	
	prop= RNA_def_property(srna, "halo_flare_size", PROP_FLOAT, PROP_NONE);
	RNA_def_property_float_sdna(prop, NULL, "flaresize");
	RNA_def_property_range(prop, 0.1f, 25.0f);
	RNA_def_property_ui_text(prop, "Halo Flare Size", "Sets the factor by which the flare is larger than the halo.");
	
	prop= RNA_def_property(srna, "halo_flare_subsize", PROP_FLOAT, PROP_NONE);
	RNA_def_property_float_sdna(prop, NULL, "subsize");
	RNA_def_property_range(prop, 0.1f, 25.0f);
	RNA_def_property_ui_text(prop, "Halo Flare Subsize", "Sets the dimension of the subflares, dots and circles.");
	
	prop= RNA_def_property(srna, "halo_flare_boost", PROP_FLOAT, PROP_NONE);
	RNA_def_property_float_sdna(prop, NULL, "flareboost");
	RNA_def_property_range(prop, 0.1f, 10.0f);
	RNA_def_property_ui_text(prop, "Halo Flare Boost", "Gives the flare extra strength.");
	
	prop= RNA_def_property(srna, "halo_flare_seed", PROP_INT, PROP_NONE);
	RNA_def_property_int_sdna(prop, NULL, "seed2");
	RNA_def_property_range(prop, 0, 255);
	RNA_def_property_ui_text(prop, "Halo Flare Seed", "Specifies an offset in the flare seed table.");
	
	prop= RNA_def_property(srna, "halo_flares_sub", PROP_INT, PROP_NONE);
	RNA_def_property_int_sdna(prop, NULL, "flarec");
	RNA_def_property_range(prop, 1, 32);
	RNA_def_property_ui_text(prop, "Halo Flares Sub", "Sets the number of subflares.");
	
	prop= RNA_def_property(srna, "halo_rings_mode", PROP_BOOLEAN, PROP_NONE);
	RNA_def_property_boolean_sdna(prop, NULL, "mode", MA_HALO_RINGS); /* use bitflags */
	RNA_def_property_ui_text(prop, "Halo Mode Rings", "Renders rings over halo.");
	
	prop= RNA_def_property(srna, "halo_lines_mode", PROP_BOOLEAN, PROP_NONE);
	RNA_def_property_boolean_sdna(prop, NULL, "mode", MA_HALO_LINES); /* use bitflags */
	RNA_def_property_ui_text(prop, "Halo Mode Lines", "Renders star shaped lines over halo.");
	
	prop= RNA_def_property(srna, "halo_star_mode", PROP_BOOLEAN, PROP_NONE);
	RNA_def_property_boolean_sdna(prop, NULL, "mode", MA_STAR); /* use bitflags */
	RNA_def_property_ui_text(prop, "Halo Mode Star", "Renders halo as a star.");
	
	prop= RNA_def_property(srna, "halo_texture_mode", PROP_BOOLEAN, PROP_NONE);
	RNA_def_property_boolean_sdna(prop, NULL, "mode", MA_HALOTEX); /* use bitflags */
	RNA_def_property_ui_text(prop, "Halo Mode Texture", "Gives halo a texture.");
	
	prop= RNA_def_property(srna, "halo_puno_mode", PROP_BOOLEAN, PROP_NONE);
	RNA_def_property_boolean_sdna(prop, NULL, "mode", MA_HALOPUNO); /* use bitflags */
	RNA_def_property_ui_text(prop, "Halo Mode Puno", "Uses the vertex normal to specify the dimension of the halo.");
	
	prop= RNA_def_property(srna, "halo_xalpha_mode", PROP_BOOLEAN, PROP_NONE);
	RNA_def_property_boolean_sdna(prop, NULL, "mode", MA_HALO_XALPHA); /* use bitflags */
	RNA_def_property_ui_text(prop, "Halo Mode Extreme Alpha", "Uses extreme alpha.");
	
	prop= RNA_def_property(srna, "halo_shaded_mode", PROP_BOOLEAN, PROP_NONE);
	RNA_def_property_boolean_sdna(prop, NULL, "mode", MA_HALO_SHADE); /* use bitflags */
	RNA_def_property_ui_text(prop, "Halo Mode Shaded", "Lets halo receive light and shadows.");
	
	prop= RNA_def_property(srna, "halo_soft_mode", PROP_BOOLEAN, PROP_NONE);
	RNA_def_property_boolean_sdna(prop, NULL, "mode", MA_HALO_SOFT); /* use bitflags */
	RNA_def_property_ui_text(prop, "Halo Mode Soft", "Softens the halo.");
	
}

void RNA_def_material(BlenderRNA *brna)
{
	StructRNA *srna= NULL;
	PropertyRNA *prop= NULL;
	
	srna= RNA_def_struct(brna, "Material", "ID");
	RNA_def_struct_ui_text(srna, "Material", "DOC_BROKEN");
	
	/* colors */
	rna_def_material_colors(srna, prop);
	/* diffuse shaders */
	rna_def_material_diffuse(srna, prop);
	/* raytrace mirror */
	rna_def_material_raymirror(srna, prop);
	/* raytrace transparency */
	rna_def_material_raytra(srna, prop);
	/* Halo settings */
	rna_def_material_halo(srna, prop);
	
	/* nodetree */
	prop= RNA_def_property(srna, "nodetree", PROP_POINTER, PROP_NONE);
	RNA_def_property_ui_text(prop, "Node Tree", "");
}

#endif


