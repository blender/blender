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

#include <float.h>
#include <stdlib.h>

#include "RNA_define.h"
#include "RNA_types.h"

#include "rna_internal.h"

#include "DNA_material_types.h"
#include "DNA_texture_types.h"

#include "WM_types.h"

#ifdef RNA_RUNTIME

static void *rna_Material_self_get(PointerRNA *ptr)
{
	return ptr->id.data;
}

static void rna_Material_mode_halo_set(PointerRNA *ptr, int value)
{
	Material *mat= (Material*)ptr->data;
	
	if(value)
		mat->mode |= MA_HALO;
	else
		mat->mode &= ~(MA_HALO|MA_STAR|MA_HALO_XALPHA|MA_ZINV|MA_ENV);
}

static void rna_Material_mtex_begin(CollectionPropertyIterator *iter, PointerRNA *ptr)
{
	Material *mat= (Material*)ptr->data;
	rna_iterator_array_begin(iter, (void*)mat->mtex, sizeof(MTex*), MAX_MTEX, NULL);
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
	RNA_def_property_update(prop, NC_MATERIAL|ND_SHADING_DRAW, NULL);
	
	prop= RNA_def_property(srna, "specular_color", PROP_FLOAT, PROP_COLOR);
	RNA_def_property_float_sdna(prop, NULL, "specr");
	RNA_def_property_array(prop, 3);
	RNA_def_property_ui_text(prop, "Specular Color", "");
	RNA_def_property_update(prop, NC_MATERIAL|ND_SHADING_DRAW, NULL);
	
	prop= RNA_def_property(srna, "mirror_color", PROP_FLOAT, PROP_COLOR);
	RNA_def_property_float_sdna(prop, NULL, "mirr");
	RNA_def_property_array(prop, 3);
	RNA_def_property_ui_text(prop, "Mirror Color", "");
	RNA_def_property_update(prop, NC_MATERIAL|ND_SHADING, NULL);
	
	prop= RNA_def_property(srna, "ambient_color", PROP_FLOAT, PROP_COLOR);
	RNA_def_property_float_sdna(prop, NULL, "ambr");
	RNA_def_property_array(prop, 3);
	RNA_def_property_ui_text(prop, "Ambient Color", "");
	RNA_def_property_update(prop, NC_MATERIAL|ND_SHADING, NULL);
	
	prop= RNA_def_property(srna, "alpha", PROP_FLOAT, PROP_NONE);
	RNA_def_property_range(prop, 0.0f, 1.0f);
	RNA_def_property_ui_text(prop, "Alpha", "");
	RNA_def_property_update(prop, NC_MATERIAL|ND_SHADING_DRAW, NULL);
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
	RNA_def_property_update(prop, NC_MATERIAL|ND_SHADING, NULL);
	
	prop= RNA_def_property(srna, "diffuse_reflection", PROP_FLOAT, PROP_NONE);
	RNA_def_property_float_sdna(prop, NULL, "ref");
	RNA_def_property_range(prop, 0.0f, 1.0f);
	RNA_def_property_ui_text(prop, "Diffuse Reflection", "Amount of diffuse reflection.");
	RNA_def_property_update(prop, NC_MATERIAL|ND_SHADING_DRAW, NULL);
	
	prop= RNA_def_property(srna, "roughness", PROP_FLOAT, PROP_NONE);
	RNA_def_property_range(prop, 0.0f, 3.14f);
	RNA_def_property_ui_text(prop, "Roughness", "Oren-Nayar Roughness");
	RNA_def_property_update(prop, NC_MATERIAL|ND_SHADING, NULL);
	
	prop= RNA_def_property(srna, "params1_4", PROP_FLOAT, PROP_NONE);
	RNA_def_property_float_sdna(prop, NULL, "param");
	RNA_def_property_array(prop, 4);
	RNA_def_property_range(prop, 0.0f, 5.0f);
	RNA_def_property_ui_text(prop, "Params 1-4", "Parameters used for diffuse and specular Toon, and diffuse Fresnel shaders. Check documentation for details.");
	RNA_def_property_update(prop, NC_MATERIAL|ND_SHADING, NULL);
	
	prop= RNA_def_property(srna, "darkness", PROP_FLOAT, PROP_NONE);
	RNA_def_property_range(prop, 0.0f, 2.0f);
	RNA_def_property_ui_text(prop, "Darkness", "Minnaert darkness.");
	RNA_def_property_update(prop, NC_MATERIAL|ND_SHADING, NULL);
}

static void rna_def_material_raymirror(BlenderRNA *brna, StructRNA *parent)
{
	StructRNA *srna;
	PropertyRNA *prop;

	static EnumPropertyItem prop_fadeto_mir_items[] = {
		{MA_RAYMIR_FADETOSKY, "RAYMIR_FADETOSKY", "Fade to sky color", ""},
		{MA_RAYMIR_FADETOMAT, "RAYMIR_FADETOMAT", "Fade to material color", ""},
		{0, NULL, NULL, NULL}};

	srna= RNA_def_struct(brna, "MaterialRaytraceMirror", NULL);
	RNA_def_struct_sdna(srna, "Material");
	RNA_def_struct_ui_text(srna, "Raytrace Mirror", "");

	prop= RNA_def_property(parent, "raytrace_mirror", PROP_POINTER, PROP_NONE);
	RNA_def_property_struct_type(prop, "MaterialRaytraceMirror");
	RNA_def_property_pointer_funcs(prop, "rna_Material_self_get", NULL, NULL);
	RNA_def_property_ui_text(prop, "Raytrace Mirror", "");
	
	prop= RNA_def_property(srna, "mode_ray_mirror", PROP_BOOLEAN, PROP_NONE);
	RNA_def_property_boolean_sdna(prop, NULL, "mode", MA_RAYMIRROR); /* use bitflags */
	RNA_def_property_ui_text(prop, "Ray Mirror Mode", "Toggle raytrace mirror.");
	RNA_def_property_update(prop, NC_MATERIAL|ND_SHADING, NULL);
		
	prop= RNA_def_property(srna, "ray_mirror", PROP_FLOAT, PROP_NONE);
	RNA_def_property_range(prop, 0.0f, 1.0f);
	RNA_def_property_ui_text(prop, "Ray Mirror", "Sets the amount mirror reflection for raytrace.");
	RNA_def_property_update(prop, NC_MATERIAL|ND_SHADING, NULL);
	
	prop= RNA_def_property(srna, "ray_mirror_fresnel", PROP_FLOAT, PROP_NONE);
	RNA_def_property_float_sdna(prop, NULL, "fresnel_mir");
	RNA_def_property_range(prop, 0.0f, 5.0f);
	RNA_def_property_ui_text(prop, "Ray Mirror Fresnel", "Power of Fresnel for mirror reflection.");
	RNA_def_property_update(prop, NC_MATERIAL|ND_SHADING, NULL);
	
	prop= RNA_def_property(srna, "ray_mirror_fresnel_fac", PROP_FLOAT, PROP_NONE);
	RNA_def_property_float_sdna(prop, NULL, "fresnel_mir_i");
	RNA_def_property_range(prop, 0.0f, 1.0f);
	RNA_def_property_ui_text(prop, "Ray Mirror Fresnel Factor", "Blending factor for Fresnel.");
	RNA_def_property_update(prop, NC_MATERIAL|ND_SHADING, NULL);
	
	prop= RNA_def_property(srna, "ray_mirror_gloss", PROP_FLOAT, PROP_NONE);
	RNA_def_property_float_sdna(prop, NULL, "gloss_mir");
	RNA_def_property_range(prop, 0.0f, 1.0f);
	RNA_def_property_ui_text(prop, "Ray Mirror Gloss", "The shininess of the reflection. Values < 1.0 give diffuse, blurry reflections.");
	RNA_def_property_update(prop, NC_MATERIAL|ND_SHADING, NULL);
	
	prop= RNA_def_property(srna, "ray_mirror_gloss_aniso", PROP_FLOAT, PROP_NONE);
	RNA_def_property_float_sdna(prop, NULL, "aniso_gloss_mir");
	RNA_def_property_range(prop, 0.0f, 1.0f);
	RNA_def_property_ui_text(prop, "Ray Mirror Gloss Aniso", "The shape of the reflection, from 0.0 (circular) to 1.0 (fully stretched along the tangent.");
	RNA_def_property_update(prop, NC_MATERIAL|ND_SHADING, NULL);
		
	prop= RNA_def_property(srna, "ray_mirror_gloss_samples", PROP_INT, PROP_NONE);
	RNA_def_property_int_sdna(prop, NULL, "samp_gloss_mir");
	RNA_def_property_range(prop, 0, 1024);
	RNA_def_property_ui_text(prop, "Ray Mirror Gloss Samples", "Number of cone samples averaged for blurry reflections.");
	RNA_def_property_update(prop, NC_MATERIAL|ND_SHADING, NULL);
	
	prop= RNA_def_property(srna, "ray_mirror_adapt_thresh", PROP_FLOAT, PROP_NONE);
	RNA_def_property_float_sdna(prop, NULL, "adapt_thresh_mir");
	RNA_def_property_range(prop, 0.0f, 1.0f);
	RNA_def_property_ui_text(prop, "Ray Mirror Gloss Threshold", "Threshold for adaptive sampling. If a sample contributes less than this amount (as a percentage), sampling is stopped.");
	RNA_def_property_update(prop, NC_MATERIAL|ND_SHADING, NULL);
	
	prop= RNA_def_property(srna, "ray_mirror_depth", PROP_INT, PROP_NONE);
	RNA_def_property_int_sdna(prop, NULL, "ray_depth");
	RNA_def_property_range(prop, 0, 10);
	RNA_def_property_ui_text(prop, "Ray Mirror Depth", "Maximum allowed number of light inter-reflections.");
	RNA_def_property_update(prop, NC_MATERIAL|ND_SHADING, NULL);
	
	prop= RNA_def_property(srna, "ray_mirror_distance", PROP_FLOAT, PROP_NONE);
	RNA_def_property_float_sdna(prop, NULL, "dist_mir");
	RNA_def_property_range(prop, 0.0f, 10000.0f);
	RNA_def_property_ui_text(prop, "Ray Mirror Max Dist", "Maximum distance of reflected rays. Reflections further than this range fade to sky color or material color.");
	RNA_def_property_update(prop, NC_MATERIAL|ND_SHADING, NULL);
	
	prop= RNA_def_property(srna, "ray_mirror_fadeto", PROP_ENUM, PROP_NONE);
	RNA_def_property_enum_sdna(prop, NULL, "fadeto_mir");
	RNA_def_property_enum_items(prop, prop_fadeto_mir_items);
	RNA_def_property_ui_text(prop, "Ray Mirror End Fade-out", "The color that rays with no intersection within the Max Distance take. Material color can be best for indoor scenes, sky color for outdoor.");
	RNA_def_property_update(prop, NC_MATERIAL|ND_SHADING, NULL);
}

static void rna_def_material_raytra(BlenderRNA *brna, StructRNA *parent)
{
	StructRNA *srna;
	PropertyRNA *prop;

	srna= RNA_def_struct(brna, "MaterialRaytraceTransparency", NULL);
	RNA_def_struct_sdna(srna, "Material");
	RNA_def_struct_ui_text(srna, "Raytrace Transparency", "");

	prop= RNA_def_property(parent, "raytrace_transparency", PROP_POINTER, PROP_NONE);
	RNA_def_property_struct_type(prop, "MaterialRaytraceTransparency");
	RNA_def_property_pointer_funcs(prop, "rna_Material_self_get", NULL, NULL);
	RNA_def_property_ui_text(prop, "Raytrace Transparency", "");

	prop= RNA_def_property(srna, "mode_ray_transparency", PROP_BOOLEAN, PROP_NONE);
	RNA_def_property_boolean_sdna(prop, NULL, "mode", MA_RAYTRANSP); /* use bitflags */
	RNA_def_property_ui_text(prop, "Ray Transparency Mode", "Enables raytracing for transparent refraction rendering.");
	RNA_def_property_update(prop, NC_MATERIAL|ND_SHADING, NULL);
	
	prop= RNA_def_property(srna, "ray_transparency_ior", PROP_FLOAT, PROP_NONE);
	RNA_def_property_float_sdna(prop, NULL, "ang");
	RNA_def_property_range(prop, 1.0f, 3.0f);
	RNA_def_property_ui_text(prop, "Ray Transparency IOR", "Sets angular index of refraction for raytraced refraction.");
	RNA_def_property_update(prop, NC_MATERIAL|ND_SHADING, NULL);
	
	prop= RNA_def_property(srna, "ray_transparency_fresnel", PROP_FLOAT, PROP_NONE);
	RNA_def_property_float_sdna(prop, NULL, "fresnel_tra");
	RNA_def_property_range(prop, 0.0f, 5.0f);
	RNA_def_property_ui_text(prop, "Ray Transparency Fresnel", "Power of Fresnel for transparency (Ray or ZTransp).");
	RNA_def_property_update(prop, NC_MATERIAL|ND_SHADING, NULL);
	
	prop= RNA_def_property(srna, "ray_transparency_fresnel_fac", PROP_FLOAT, PROP_NONE);
	RNA_def_property_float_sdna(prop, NULL, "fresnel_tra_i");
	RNA_def_property_range(prop, 1.0f, 5.0f);
	RNA_def_property_ui_text(prop, "Ray Transparency Fresnel Factor", "Blending factor for Fresnel.");
	RNA_def_property_update(prop, NC_MATERIAL|ND_SHADING, NULL);
	
	prop= RNA_def_property(srna, "ray_transparency_gloss", PROP_FLOAT, PROP_NONE);
	RNA_def_property_float_sdna(prop, NULL, "gloss_tra");
	RNA_def_property_range(prop, 0.0f, 1.0f);
	RNA_def_property_ui_text(prop, "Ray Transparency Gloss", "The clarity of the refraction. Values < 1.0 give diffuse, blurry refractions.");
	RNA_def_property_update(prop, NC_MATERIAL|ND_SHADING, NULL);
	
	prop= RNA_def_property(srna, "ray_transparency_gloss_samples", PROP_INT, PROP_NONE);
	RNA_def_property_int_sdna(prop, NULL, "samp_gloss_tra");
	RNA_def_property_range(prop, 0, 1024);
	RNA_def_property_ui_text(prop, "Ray Transparency Gloss Samples", "Number of cone samples averaged for blurry refractions.");
	RNA_def_property_update(prop, NC_MATERIAL|ND_SHADING, NULL);
	
	prop= RNA_def_property(srna, "ray_transparency_adapt_thresh", PROP_FLOAT, PROP_NONE);
	RNA_def_property_float_sdna(prop, NULL, "adapt_thresh_tra");
	RNA_def_property_range(prop, 0.0f, 1.0f);
	RNA_def_property_ui_text(prop, "Ray Transparency Gloss Threshold", "Threshold for adaptive sampling. If a sample contributes less than this amount (as a percentage), sampling is stopped.");
	RNA_def_property_update(prop, NC_MATERIAL|ND_SHADING, NULL);
	
	prop= RNA_def_property(srna, "ray_transparency_depth", PROP_INT, PROP_NONE);
	RNA_def_property_int_sdna(prop, NULL, "ray_depth_tra");
	RNA_def_property_range(prop, 0, 10);
	RNA_def_property_ui_text(prop, "Ray Transparency Depth", "Maximum allowed number of light inter-refractions.");
	RNA_def_property_update(prop, NC_MATERIAL|ND_SHADING, NULL);
	
	prop= RNA_def_property(srna, "ray_transparency_filter", PROP_FLOAT, PROP_NONE);
	RNA_def_property_float_sdna(prop, NULL, "filter");
	RNA_def_property_range(prop, 0.0f, 1.0f);
	RNA_def_property_ui_text(prop, "Ray Transparency Filter", "Amount to blend in the material's diffuse color in raytraced transparency (simulating absorption).");
	RNA_def_property_update(prop, NC_MATERIAL|ND_SHADING, NULL);
	
	prop= RNA_def_property(srna, "ray_transparency_limit", PROP_FLOAT, PROP_NONE);
	RNA_def_property_float_sdna(prop, NULL, "tx_limit");
	RNA_def_property_range(prop, 0.0f, 100.0f);
	RNA_def_property_ui_text(prop, "Ray Transparency Limit", "Maximum depth for light to travel through the transparent material before becoming fully filtered (0.0 is disabled).");
	RNA_def_property_update(prop, NC_MATERIAL|ND_SHADING, NULL);
	
	prop= RNA_def_property(srna, "ray_transparency_falloff", PROP_FLOAT, PROP_NONE);
	RNA_def_property_float_sdna(prop, NULL, "tx_falloff");
	RNA_def_property_range(prop, 0.1f, 10.0f);
	RNA_def_property_ui_text(prop, "Ray Transparency Falloff", "Falloff power for transmissivity filter effect (1.0 is linear).");
	RNA_def_property_update(prop, NC_MATERIAL|ND_SHADING, NULL);
	
	prop= RNA_def_property(srna, "ray_transparency_specular_opacity", PROP_FLOAT, PROP_NONE);
	RNA_def_property_float_sdna(prop, NULL, "spectra");
	RNA_def_property_range(prop, 0.0f, 1.0f);
	RNA_def_property_ui_text(prop, "Ray Transparency Specular Opacity", "Makes specular areas opaque on transparent materials.");
	RNA_def_property_update(prop, NC_MATERIAL|ND_SHADING, NULL);
}

static void rna_def_material_halo(BlenderRNA *brna, StructRNA *parent)
{
	StructRNA *srna;
	PropertyRNA *prop;

	srna= RNA_def_struct(brna, "MaterialHalo", NULL);
	RNA_def_struct_sdna(srna, "Material");
	RNA_def_struct_ui_text(srna, "Halo", "");

	prop= RNA_def_property(parent, "halo", PROP_POINTER, PROP_NONE);
	RNA_def_property_struct_type(prop, "MaterialHalo");
	RNA_def_property_pointer_funcs(prop, "rna_Material_self_get", NULL, NULL);
	RNA_def_property_ui_text(prop, "Halo", "");

	prop= RNA_def_property(srna, "mode_halo", PROP_BOOLEAN, PROP_NONE);
	RNA_def_property_boolean_sdna(prop, NULL, "mode", MA_HALO); /* use bitflags */
	RNA_def_property_ui_text(prop, "Halo Mode", "Enables halo rendering of material.");
	RNA_def_property_boolean_funcs(prop, NULL, "rna_Material_mode_halo_set");
	RNA_def_property_update(prop, NC_MATERIAL|ND_SHADING_DRAW, NULL);
	
	prop= RNA_def_property(srna, "halo_size", PROP_FLOAT, PROP_NONE);
	RNA_def_property_float_sdna(prop, NULL, "hasize");
	RNA_def_property_range(prop, 0.0f, 100.0f);
	RNA_def_property_ui_text(prop, "Halo Size", "Sets the dimension of the halo.");
	RNA_def_property_update(prop, NC_MATERIAL|ND_SHADING, NULL);
	
	prop= RNA_def_property(srna, "halo_hardness", PROP_INT, PROP_NONE);
	RNA_def_property_int_sdna(prop, NULL, "har");
	RNA_def_property_range(prop, 0, 127);
	RNA_def_property_ui_text(prop, "Halo Hardness", "Sets the hardness of the halo.");
	RNA_def_property_update(prop, NC_MATERIAL|ND_SHADING, NULL);
	
	prop= RNA_def_property(srna, "halo_add", PROP_FLOAT, PROP_NONE);
	RNA_def_property_float_sdna(prop, NULL, "add");
	RNA_def_property_range(prop, 0.0f, 1.0f);
	RNA_def_property_ui_text(prop, "Halo Add", "Sets the strength of the add effect.");
	RNA_def_property_update(prop, NC_MATERIAL|ND_SHADING, NULL);
	
	prop= RNA_def_property(srna, "halo_rings", PROP_INT, PROP_NONE);
	RNA_def_property_int_sdna(prop, NULL, "ringc");
	RNA_def_property_range(prop, 0, 24);
	RNA_def_property_ui_text(prop, "Halo Rings", "Sets the number of rings rendered over the halo.");
	RNA_def_property_update(prop, NC_MATERIAL|ND_SHADING, NULL);
	
	prop= RNA_def_property(srna, "halo_lines", PROP_INT, PROP_NONE);
	RNA_def_property_int_sdna(prop, NULL, "linec");
	RNA_def_property_range(prop, 0, 250);
	RNA_def_property_ui_text(prop, "Halo Lines", "Sets the number of star shaped lines rendered over the halo.");
	RNA_def_property_update(prop, NC_MATERIAL|ND_SHADING, NULL);
	
	prop= RNA_def_property(srna, "halo_star_tips", PROP_INT, PROP_NONE);
	RNA_def_property_int_sdna(prop, NULL, "starc");
	RNA_def_property_range(prop, 3, 50);
	RNA_def_property_ui_text(prop, "Halo Star Tips", "Sets the number of points on the star shaped halo.");
	RNA_def_property_update(prop, NC_MATERIAL|ND_SHADING, NULL);
	
	prop= RNA_def_property(srna, "halo_seed", PROP_INT, PROP_NONE);
	RNA_def_property_int_sdna(prop, NULL, "seed1");
	RNA_def_property_range(prop, 0, 255);
	RNA_def_property_ui_text(prop, "Halo Seed", "Randomizes ring dimension and line location.");
	RNA_def_property_update(prop, NC_MATERIAL|ND_SHADING, NULL);
	
	prop= RNA_def_property(srna, "halo_flare_mode", PROP_BOOLEAN, PROP_NONE);
	RNA_def_property_boolean_sdna(prop, NULL, "mode", MA_HALO_FLARE); /* use bitflags */
	RNA_def_property_ui_text(prop, "Halo Mode Flare", "Renders halo as a lensflare.");
	RNA_def_property_update(prop, NC_MATERIAL|ND_SHADING, NULL);
	
	prop= RNA_def_property(srna, "halo_flare_size", PROP_FLOAT, PROP_NONE);
	RNA_def_property_float_sdna(prop, NULL, "flaresize");
	RNA_def_property_range(prop, 0.1f, 25.0f);
	RNA_def_property_ui_text(prop, "Halo Flare Size", "Sets the factor by which the flare is larger than the halo.");
	RNA_def_property_update(prop, NC_MATERIAL|ND_SHADING, NULL);
	
	prop= RNA_def_property(srna, "halo_flare_subsize", PROP_FLOAT, PROP_NONE);
	RNA_def_property_float_sdna(prop, NULL, "subsize");
	RNA_def_property_range(prop, 0.1f, 25.0f);
	RNA_def_property_ui_text(prop, "Halo Flare Subsize", "Sets the dimension of the subflares, dots and circles.");
	RNA_def_property_update(prop, NC_MATERIAL|ND_SHADING, NULL);
	
	prop= RNA_def_property(srna, "halo_flare_boost", PROP_FLOAT, PROP_NONE);
	RNA_def_property_float_sdna(prop, NULL, "flareboost");
	RNA_def_property_range(prop, 0.1f, 10.0f);
	RNA_def_property_ui_text(prop, "Halo Flare Boost", "Gives the flare extra strength.");
	RNA_def_property_update(prop, NC_MATERIAL|ND_SHADING, NULL);
	
	prop= RNA_def_property(srna, "halo_flare_seed", PROP_INT, PROP_NONE);
	RNA_def_property_int_sdna(prop, NULL, "seed2");
	RNA_def_property_range(prop, 0, 255);
	RNA_def_property_ui_text(prop, "Halo Flare Seed", "Specifies an offset in the flare seed table.");
	RNA_def_property_update(prop, NC_MATERIAL|ND_SHADING, NULL);
	
	prop= RNA_def_property(srna, "halo_flares_sub", PROP_INT, PROP_NONE);
	RNA_def_property_int_sdna(prop, NULL, "flarec");
	RNA_def_property_range(prop, 1, 32);
	RNA_def_property_ui_text(prop, "Halo Flares Sub", "Sets the number of subflares.");
	RNA_def_property_update(prop, NC_MATERIAL|ND_SHADING, NULL);
	
	prop= RNA_def_property(srna, "halo_rings_mode", PROP_BOOLEAN, PROP_NONE);
	RNA_def_property_boolean_sdna(prop, NULL, "mode", MA_HALO_RINGS); /* use bitflags */
	RNA_def_property_ui_text(prop, "Halo Mode Rings", "Renders rings over halo.");
	RNA_def_property_update(prop, NC_MATERIAL|ND_SHADING, NULL);
	
	prop= RNA_def_property(srna, "halo_lines_mode", PROP_BOOLEAN, PROP_NONE);
	RNA_def_property_boolean_sdna(prop, NULL, "mode", MA_HALO_LINES); /* use bitflags */
	RNA_def_property_ui_text(prop, "Halo Mode Lines", "Renders star shaped lines over halo.");
	RNA_def_property_update(prop, NC_MATERIAL|ND_SHADING, NULL);
	
	prop= RNA_def_property(srna, "halo_star_mode", PROP_BOOLEAN, PROP_NONE);
	RNA_def_property_boolean_sdna(prop, NULL, "mode", MA_STAR); /* use bitflags */
	RNA_def_property_ui_text(prop, "Halo Mode Star", "Renders halo as a star.");
	RNA_def_property_update(prop, NC_MATERIAL|ND_SHADING, NULL);
	
	prop= RNA_def_property(srna, "halo_texture_mode", PROP_BOOLEAN, PROP_NONE);
	RNA_def_property_boolean_sdna(prop, NULL, "mode", MA_HALOTEX); /* use bitflags */
	RNA_def_property_ui_text(prop, "Halo Mode Texture", "Gives halo a texture.");
	RNA_def_property_update(prop, NC_MATERIAL|ND_SHADING, NULL);
	
	prop= RNA_def_property(srna, "halo_puno_mode", PROP_BOOLEAN, PROP_NONE);
	RNA_def_property_boolean_sdna(prop, NULL, "mode", MA_HALOPUNO); /* use bitflags */
	RNA_def_property_ui_text(prop, "Halo Mode Puno", "Uses the vertex normal to specify the dimension of the halo.");
	RNA_def_property_update(prop, NC_MATERIAL|ND_SHADING, NULL);
	
	prop= RNA_def_property(srna, "halo_xalpha_mode", PROP_BOOLEAN, PROP_NONE);
	RNA_def_property_boolean_sdna(prop, NULL, "mode", MA_HALO_XALPHA); /* use bitflags */
	RNA_def_property_ui_text(prop, "Halo Mode Extreme Alpha", "Uses extreme alpha.");
	RNA_def_property_update(prop, NC_MATERIAL|ND_SHADING, NULL);
	
	prop= RNA_def_property(srna, "halo_shaded_mode", PROP_BOOLEAN, PROP_NONE);
	RNA_def_property_boolean_sdna(prop, NULL, "mode", MA_HALO_SHADE); /* use bitflags */
	RNA_def_property_ui_text(prop, "Halo Mode Shaded", "Lets halo receive light and shadows.");
	RNA_def_property_update(prop, NC_MATERIAL|ND_SHADING, NULL);
	
	prop= RNA_def_property(srna, "halo_soft_mode", PROP_BOOLEAN, PROP_NONE);
	RNA_def_property_boolean_sdna(prop, NULL, "mode", MA_HALO_SOFT); /* use bitflags */
	RNA_def_property_ui_text(prop, "Halo Mode Soft", "Softens the halo.");
	RNA_def_property_update(prop, NC_MATERIAL|ND_SHADING, NULL);
}

static void rna_def_material_sss(BlenderRNA *brna, StructRNA *parent)
{
	StructRNA *srna;
	PropertyRNA *prop;

	srna= RNA_def_struct(brna, "MaterialSubsurfaceScattering", NULL);
	RNA_def_struct_sdna(srna, "Material");
	RNA_def_struct_ui_text(srna, "Subsurface Scattering", "");

	prop= RNA_def_property(parent, "subsurface_scattering", PROP_POINTER, PROP_NONE);
	RNA_def_property_struct_type(prop, "MaterialSubsurfaceScattering");
	RNA_def_property_pointer_funcs(prop, "rna_Material_self_get", NULL, NULL);
	RNA_def_property_ui_text(prop, "Subsurface Scattering", "");

	/* XXX: The labels for the array elements should really be R, G, B */
	prop= RNA_def_property(srna, "radius", PROP_FLOAT, PROP_NONE);
	RNA_def_property_float_sdna(prop, NULL, "sss_radius");
	RNA_def_property_range(prop, 0.001, FLT_MAX);
	RNA_def_property_ui_range(prop, 0.001, 10000, 1, 3);
	RNA_def_property_ui_text(prop, "Radius", "Mean red/green/blue scattering path length.");

	prop= RNA_def_property(srna, "color", PROP_FLOAT, PROP_COLOR);
	RNA_def_property_float_sdna(prop, NULL, "sss_col");
	RNA_def_property_ui_text(prop, "Color", "Scattering color.");

	prop= RNA_def_property(srna, "error_tolerance", PROP_FLOAT, PROP_NONE);
	RNA_def_property_float_sdna(prop, NULL, "sss_error");
	RNA_def_property_ui_range(prop, 0.0001, 10, 1, 3);
	RNA_def_property_ui_text(prop, "Error tolerance", "");

	prop= RNA_def_property(srna, "object_scale", PROP_FLOAT, PROP_NONE);
	RNA_def_property_float_sdna(prop, NULL, "sss_scale");
	RNA_def_property_ui_range(prop, 0.001, 1000, 1, 3);
	RNA_def_property_ui_text(prop, "Object Scale", "");

	prop= RNA_def_property(srna, "index_of_refraction", PROP_FLOAT, PROP_NONE);
	RNA_def_property_float_sdna(prop, NULL, "sss_ior");
	RNA_def_property_ui_range(prop, 0.1, 2, 1, 3);
	RNA_def_property_ui_text(prop, "Index of Refraction", "");

	prop= RNA_def_property(srna, "blend_factor", PROP_FLOAT, PROP_NONE);
	RNA_def_property_float_sdna(prop, NULL, "sss_colfac");
	RNA_def_property_range(prop, 0, 1);
	RNA_def_property_ui_text(prop, "Blend Factor", "");

	prop= RNA_def_property(srna, "texture_scattering_factor", PROP_FLOAT, PROP_NONE);
	RNA_def_property_float_sdna(prop, NULL, "sss_texfac");
	RNA_def_property_range(prop, 0, 1);
	RNA_def_property_ui_text(prop, "Texture Scattering Factor", "");

	prop= RNA_def_property(srna, "front_weight", PROP_FLOAT, PROP_NONE);
	RNA_def_property_float_sdna(prop, NULL, "sss_front");
	RNA_def_property_range(prop, 0, 2);
	RNA_def_property_ui_text(prop, "Front Weight", "");

	prop= RNA_def_property(srna, "back_weight", PROP_FLOAT, PROP_NONE);
	RNA_def_property_float_sdna(prop, NULL, "sss_back");
	RNA_def_property_range(prop, 0, 10);
	RNA_def_property_ui_text(prop, "Back Weight", "");

	prop= RNA_def_property(srna, "enable", PROP_BOOLEAN, PROP_NONE);
	RNA_def_property_boolean_sdna(prop, NULL, "sss_flag", MA_DIFF_SSS);
	RNA_def_property_ui_text(prop, "Enable", "");
}

void RNA_def_material(BlenderRNA *brna)
{
	StructRNA *srna= NULL;
	PropertyRNA *prop= NULL;

	srna= RNA_def_struct(brna, "Material", "ID");
	RNA_def_struct_ui_text(srna, "Material", "DOC_BROKEN");

	/* mtex */
	prop= RNA_def_property(srna, "textures", PROP_COLLECTION, PROP_NONE);
	RNA_def_property_struct_type(prop, "TextureSlot");
	RNA_def_property_collection_funcs(prop, "rna_Material_mtex_begin", "rna_iterator_array_next", "rna_iterator_array_end", "rna_iterator_array_dereference_get", 0, 0, 0, 0);
	RNA_def_property_ui_text(prop, "Textures", "");
	
	/* colors */
	rna_def_material_colors(srna, prop);
	/* diffuse shaders */
	rna_def_material_diffuse(srna, prop);
	/* raytrace mirror */
	rna_def_material_raymirror(brna, srna);
	/* raytrace transparency */
	rna_def_material_raytra(brna, srna);
	/* Halo settings */
	rna_def_material_halo(brna, srna);
	/* Subsurface scattering */
	rna_def_material_sss(brna, srna);

	/* nodetree */
	prop= RNA_def_property(srna, "nodetree", PROP_POINTER, PROP_NONE);
	RNA_def_property_ui_text(prop, "Node Tree", "");
}

#endif


