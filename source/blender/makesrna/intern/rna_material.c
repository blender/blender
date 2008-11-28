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

#else

void RNA_def_material(BlenderRNA *brna)
{
	StructRNA *srna;
	PropertyRNA *prop;
	
	static EnumPropertyItem prop_type_items[] = {
		{MA_RGB, "RGB", "RGB", ""},
		/*{MA_CMYK, "CMYK", "CMYK", ""}, 
		{MA_YUV, "YUV", "YUV", ""},  XXX: blender code doesn't support this yet. Commented out */
		{MA_HSV, "HSV", "HSV", ""},
		{0, NULL, NULL, NULL}};
	static EnumPropertyItem prop_fadeto_mir_items[] = {
		{MA_RAYMIR_FADETOSKY, "RAYMIR_FADETOSKY", "Fade to sky color", ""},
		{MA_RAYMIR_FADETOMAT, "RAYMIR_FADETOMAT", "Fade to material color", ""},
		{0, NULL, NULL, NULL}};
	static EnumPropertyItem prop_diff_shader_items[] = {
		{MA_DIFF_LAMBERT, "DIFF_LAMBERT", "Lambert", ""},
		{MA_DIFF_ORENNAYAR, "DIFF_ORENNAYAR", "Orennayar", ""},
		{MA_DIFF_TOON, "DIFF_TOON", "Toon", ""},
		{MA_DIFF_MINNAERT, "DIFF_MINNAERT", "Minnaert", ""},
		{MA_DIFF_FRESNEL, "DIFF_FRESNEL", "Fresnel", ""},
		{0, NULL, NULL, NULL}};
	
	
	srna= RNA_def_struct(brna, "Material", "ID", "Material");
		
	prop= RNA_def_property(srna, "colormodel", PROP_ENUM, PROP_NONE);
	RNA_def_property_enum_items(prop, prop_type_items);
	RNA_def_property_ui_text(prop, "Color Model", "Color model.");
	
	/* colors */
	prop= RNA_def_property(srna, "color", PROP_FLOAT, PROP_COLOR);
	RNA_def_property_float_sdna(prop, "Material", "r");
	RNA_def_property_array(prop, 3);
	RNA_def_property_ui_text(prop, "Color", "Diffuse color.");
	RNA_def_property_ui_range(prop, 0.0f , 1.0f, 10.0f, 3.0f);
	
	prop= RNA_def_property(srna, "specular", PROP_FLOAT, PROP_COLOR);
	RNA_def_property_float_sdna(prop, NULL, "specr");
	RNA_def_property_array(prop, 3);
	RNA_def_property_ui_text(prop, "Specular Color", "Specular color.");
	RNA_def_property_ui_range(prop, 0.0f , 1.0f, 10.0f, 3.0f);
	
	prop= RNA_def_property(srna, "mirror", PROP_FLOAT, PROP_COLOR);
	RNA_def_property_float_sdna(prop, NULL, "mirr");
	RNA_def_property_array(prop, 3);
	RNA_def_property_ui_text(prop, "Mirror Color", "Mirror color.");
	RNA_def_property_ui_range(prop, 0.0f , 1.0f, 10.0f, 3.0f);
		
	prop= RNA_def_property(srna, "ambient", PROP_FLOAT, PROP_COLOR);
	RNA_def_property_float_sdna(prop, NULL, "ambr");
	RNA_def_property_array(prop, 3);
	RNA_def_property_ui_text(prop, "Ambient Color", "Ambient color.");
	RNA_def_property_ui_range(prop, 0.0f , 1.0f, 10.0f, 3.0f);
	
	prop= RNA_def_property(srna, "alpha", PROP_FLOAT, PROP_NONE);
	RNA_def_property_range(prop, 0.0f, 1.0f);
	RNA_def_property_ui_text(prop, "Alpha", "Alpha");
	
	/* diffuse shaders */
	
	prop= RNA_def_property(srna, "diff_shader", PROP_ENUM, PROP_NONE);
	RNA_def_property_enum_items(prop, prop_diff_shader_items);
	RNA_def_property_ui_text(prop, "Diffuse Shader Model", "Diffuse shader model.");
	
	prop= RNA_def_property(srna, "ref", PROP_FLOAT, PROP_NONE);
	RNA_def_property_range(prop, 0.0f, 1.0f);
	RNA_def_property_ui_text(prop, "Reflection", "Sets the amount of reflection.");
	
	prop= RNA_def_property(srna, "roughness", PROP_FLOAT, PROP_NONE);
	RNA_def_property_range(prop, 0.0f, 3.14f);
	RNA_def_property_ui_text(prop, "Roughness", "Sets Oren Nayar Roughness");
	
	prop= RNA_def_property(srna, "params1_4", PROP_FLOAT, PROP_NONE);
	RNA_def_property_float_sdna(prop, NULL, "param");
	RNA_def_property_array(prop, 4);
	RNA_def_property_range(prop, 0.0f, 5.0f);
	RNA_def_property_ui_text(prop, "Params 1-4", "Parameters used for diffuse and specular Toon, and diffuse Fresnel shaders. Check documentation for details.");
	
	prop= RNA_def_property(srna, "darkness", PROP_FLOAT, PROP_NONE);
	RNA_def_property_range(prop, 0.0f, 2.0f);
	RNA_def_property_ui_text(prop, "Darkness", "Sets Minnaert darkness.");
	
	/* raytrace mirror */
	prop= RNA_def_property(srna, "mode_ray_mirror", PROP_BOOLEAN, PROP_NONE);
	RNA_def_property_boolean_sdna(prop, NULL, "mode", MA_RAYMIRROR); /* use bitflags */
	RNA_def_property_ui_text(prop, "Ray Mirror Mode", "Toggle raytrace mirror.");
	
	prop= RNA_def_property(srna, "ray_mirror", PROP_FLOAT, PROP_NONE);
	RNA_def_property_range(prop, 0.0f, 1.0f);
	RNA_def_property_ui_text(prop, "Ray Mirror", "Sets the amount mirror reflection for raytrace.");
	
	prop= RNA_def_property(srna, "fresnel_mir", PROP_FLOAT, PROP_NONE);
	RNA_def_property_range(prop, 0.0f, 5.0f);
	RNA_def_property_ui_text(prop, "Fresnel", "Power of Fresnel for mirror reflection.");
	
	prop= RNA_def_property(srna, "fresnel_mir_i", PROP_FLOAT, PROP_NONE);
	RNA_def_property_range(prop, 0.0f, 1.0f);
	RNA_def_property_ui_text(prop, "Fresnel Factor", "Blending factor for Fresnel.");
	
	prop= RNA_def_property(srna, "gloss_mir", PROP_FLOAT, PROP_NONE);
	RNA_def_property_range(prop, 0.0f, 1.0f);
	RNA_def_property_ui_text(prop, "Gloss", "The shininess of the reflection. Values < 1.0 give diffuse, blurry reflections.");
	
	prop= RNA_def_property(srna, "aniso_gloss_mir", PROP_FLOAT, PROP_NONE);
	RNA_def_property_range(prop, 0.0f, 1.0f);
	RNA_def_property_ui_text(prop, "Aniso", "The shape of the reflection, from 0.0 (circular) to 1.0 (fully stretched along the tangent.");
		
	prop= RNA_def_property(srna, "samp_gloss_mir", PROP_FLOAT, PROP_NONE);
	RNA_def_property_range(prop, 0.0f, 1024.0f);
	RNA_def_property_ui_text(prop, "Gloss Samples", "Number of cone samples averaged for blurry reflections.");
	
	prop= RNA_def_property(srna, "adapt_thresh_mir", PROP_FLOAT, PROP_NONE);
	RNA_def_property_range(prop, 0.0f, 1.0f);
	RNA_def_property_ui_text(prop, "Gloss Threshold", "Threshold for adaptive sampling. If a sample contributes less than this amount (as a percentage), sampling is stopped.");
	
	prop= RNA_def_property(srna, "ray_depth", PROP_FLOAT, PROP_NONE);
	RNA_def_property_range(prop, 0.0f, 10.0f);
	RNA_def_property_ui_text(prop, "Ray Mirror Depth", "Maximum allowed number of light inter-reflections.");
	
	prop= RNA_def_property(srna, "dist_mir", PROP_FLOAT, PROP_NONE);
	RNA_def_property_range(prop, 0.0f, 10000.0f);
	RNA_def_property_ui_text(prop, "Ray Mirror Max Dist", "Maximum distance of reflected rays. Reflections further than this range fade to sky color or material color.");
	
	prop= RNA_def_property(srna, "fadeto_mir", PROP_ENUM, PROP_NONE);
	RNA_def_property_enum_items(prop, prop_fadeto_mir_items);
	RNA_def_property_ui_text(prop, "Ray end fade-out", "The color that rays with no intersection within the Max Distance take. Material color can be best for indoor scenes, sky color for outdoor.");
	
	/* nodetree */
	prop= RNA_def_property(srna, "nodetree", PROP_POINTER, PROP_NONE);
	RNA_def_property_struct_type(prop, "bNodeTree");
	RNA_def_property_ui_text(prop, "Nodetree", "Nodetree");

}

#endif


