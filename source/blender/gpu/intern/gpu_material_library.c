/*
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
 * The Original Code is Copyright (C) 2005 Blender Foundation.
 * All rights reserved.
 */

/** \file
 * \ingroup gpu
 *
 * GPU material library parsing and code generation.
 */

#include <stdio.h>
#include <string.h>

#include "MEM_guardedalloc.h"

#include "BLI_dynstr.h"
#include "BLI_ghash.h"
#include "BLI_string.h"
#include "BLI_utildefines.h"

#include "gpu_material_library.h"

/* List of all gpu_shader_material_*.glsl files used by GLSL materials. These
 * will be parsed to make all functions in them available to use for GPU_link().
 *
 * If a file uses functions from another file, it must be added to the list of
 * dependencies, and be placed after that file in the list. */

extern char datatoc_gpu_shader_material_add_shader_glsl[];
extern char datatoc_gpu_shader_material_ambient_occlusion_glsl[];
extern char datatoc_gpu_shader_material_anisotropic_glsl[];
extern char datatoc_gpu_shader_material_attribute_glsl[];
extern char datatoc_gpu_shader_material_background_glsl[];
extern char datatoc_gpu_shader_material_bevel_glsl[];
extern char datatoc_gpu_shader_material_blackbody_glsl[];
extern char datatoc_gpu_shader_material_bright_contrast_glsl[];
extern char datatoc_gpu_shader_material_bump_glsl[];
extern char datatoc_gpu_shader_material_camera_glsl[];
extern char datatoc_gpu_shader_material_clamp_glsl[];
extern char datatoc_gpu_shader_material_color_ramp_glsl[];
extern char datatoc_gpu_shader_material_color_util_glsl[];
extern char datatoc_gpu_shader_material_combine_hsv_glsl[];
extern char datatoc_gpu_shader_material_combine_rgb_glsl[];
extern char datatoc_gpu_shader_material_combine_xyz_glsl[];
extern char datatoc_gpu_shader_material_diffuse_glsl[];
extern char datatoc_gpu_shader_material_displacement_glsl[];
extern char datatoc_gpu_shader_material_eevee_specular_glsl[];
extern char datatoc_gpu_shader_material_emission_glsl[];
extern char datatoc_gpu_shader_material_fractal_noise_glsl[];
extern char datatoc_gpu_shader_material_fresnel_glsl[];
extern char datatoc_gpu_shader_material_gamma_glsl[];
extern char datatoc_gpu_shader_material_geometry_glsl[];
extern char datatoc_gpu_shader_material_glass_glsl[];
extern char datatoc_gpu_shader_material_glossy_glsl[];
extern char datatoc_gpu_shader_material_hair_info_glsl[];
extern char datatoc_gpu_shader_material_hash_glsl[];
extern char datatoc_gpu_shader_material_holdout_glsl[];
extern char datatoc_gpu_shader_material_hue_sat_val_glsl[];
extern char datatoc_gpu_shader_material_invert_glsl[];
extern char datatoc_gpu_shader_material_layer_weight_glsl[];
extern char datatoc_gpu_shader_material_light_falloff_glsl[];
extern char datatoc_gpu_shader_material_light_path_glsl[];
extern char datatoc_gpu_shader_material_mapping_glsl[];
extern char datatoc_gpu_shader_material_map_range_glsl[];
extern char datatoc_gpu_shader_material_math_glsl[];
extern char datatoc_gpu_shader_material_math_util_glsl[];
extern char datatoc_gpu_shader_material_mix_rgb_glsl[];
extern char datatoc_gpu_shader_material_mix_shader_glsl[];
extern char datatoc_gpu_shader_material_noise_glsl[];
extern char datatoc_gpu_shader_material_normal_glsl[];
extern char datatoc_gpu_shader_material_normal_map_glsl[];
extern char datatoc_gpu_shader_material_object_info_glsl[];
extern char datatoc_gpu_shader_material_output_aov_glsl[];
extern char datatoc_gpu_shader_material_output_material_glsl[];
extern char datatoc_gpu_shader_material_output_world_glsl[];
extern char datatoc_gpu_shader_material_particle_info_glsl[];
extern char datatoc_gpu_shader_material_principled_glsl[];
extern char datatoc_gpu_shader_material_refraction_glsl[];
extern char datatoc_gpu_shader_material_rgb_curves_glsl[];
extern char datatoc_gpu_shader_material_rgb_to_bw_glsl[];
extern char datatoc_gpu_shader_material_separate_hsv_glsl[];
extern char datatoc_gpu_shader_material_separate_rgb_glsl[];
extern char datatoc_gpu_shader_material_separate_xyz_glsl[];
extern char datatoc_gpu_shader_material_set_glsl[];
extern char datatoc_gpu_shader_material_shader_to_rgba_glsl[];
extern char datatoc_gpu_shader_material_squeeze_glsl[];
extern char datatoc_gpu_shader_material_subsurface_scattering_glsl[];
extern char datatoc_gpu_shader_material_tangent_glsl[];
extern char datatoc_gpu_shader_material_tex_brick_glsl[];
extern char datatoc_gpu_shader_material_tex_checker_glsl[];
extern char datatoc_gpu_shader_material_tex_environment_glsl[];
extern char datatoc_gpu_shader_material_tex_gradient_glsl[];
extern char datatoc_gpu_shader_material_tex_image_glsl[];
extern char datatoc_gpu_shader_material_tex_magic_glsl[];
extern char datatoc_gpu_shader_material_tex_musgrave_glsl[];
extern char datatoc_gpu_shader_material_tex_noise_glsl[];
extern char datatoc_gpu_shader_material_tex_sky_glsl[];
extern char datatoc_gpu_shader_material_texture_coordinates_glsl[];
extern char datatoc_gpu_shader_material_tex_voronoi_glsl[];
extern char datatoc_gpu_shader_material_tex_wave_glsl[];
extern char datatoc_gpu_shader_material_tex_white_noise_glsl[];
extern char datatoc_gpu_shader_material_toon_glsl[];
extern char datatoc_gpu_shader_material_translucent_glsl[];
extern char datatoc_gpu_shader_material_transparent_glsl[];
extern char datatoc_gpu_shader_material_uv_map_glsl[];
extern char datatoc_gpu_shader_material_vector_curves_glsl[];
extern char datatoc_gpu_shader_material_vector_displacement_glsl[];
extern char datatoc_gpu_shader_material_vector_math_glsl[];
extern char datatoc_gpu_shader_material_vector_rotate_glsl[];
extern char datatoc_gpu_shader_material_velvet_glsl[];
extern char datatoc_gpu_shader_material_vertex_color_glsl[];
extern char datatoc_gpu_shader_material_volume_absorption_glsl[];
extern char datatoc_gpu_shader_material_volume_info_glsl[];
extern char datatoc_gpu_shader_material_volume_principled_glsl[];
extern char datatoc_gpu_shader_material_volume_scatter_glsl[];
extern char datatoc_gpu_shader_material_wireframe_glsl[];
extern char datatoc_gpu_shader_material_world_normals_glsl[];

static GPUMaterialLibrary gpu_shader_material_math_util_library = {
    .code = datatoc_gpu_shader_material_math_util_glsl,
    .dependencies = {NULL},
};

static GPUMaterialLibrary gpu_shader_material_color_util_library = {
    .code = datatoc_gpu_shader_material_color_util_glsl,
    .dependencies = {NULL},
};

static GPUMaterialLibrary gpu_shader_material_hash_library = {
    .code = datatoc_gpu_shader_material_hash_glsl,
    .dependencies = {NULL},
};

static GPUMaterialLibrary gpu_shader_material_noise_library = {
    .code = datatoc_gpu_shader_material_noise_glsl,
    .dependencies = {&gpu_shader_material_hash_library, NULL},
};

static GPUMaterialLibrary gpu_shader_material_fractal_noise_library = {
    .code = datatoc_gpu_shader_material_fractal_noise_glsl,
    .dependencies = {&gpu_shader_material_noise_library, NULL},
};

static GPUMaterialLibrary gpu_shader_material_add_shader_library = {
    .code = datatoc_gpu_shader_material_add_shader_glsl,
    .dependencies = {NULL},
};

static GPUMaterialLibrary gpu_shader_material_ambient_occlusion_library = {
    .code = datatoc_gpu_shader_material_ambient_occlusion_glsl,
    .dependencies = {NULL},
};

static GPUMaterialLibrary gpu_shader_material_glossy_library = {
    .code = datatoc_gpu_shader_material_glossy_glsl,
    .dependencies = {NULL},
};

static GPUMaterialLibrary gpu_shader_material_anisotropic_library = {
    .code = datatoc_gpu_shader_material_anisotropic_glsl,
    .dependencies = {&gpu_shader_material_glossy_library, NULL},
};

static GPUMaterialLibrary gpu_shader_material_attribute_library = {
    .code = datatoc_gpu_shader_material_attribute_glsl,
    .dependencies = {NULL},
};

static GPUMaterialLibrary gpu_shader_material_background_library = {
    .code = datatoc_gpu_shader_material_background_glsl,
    .dependencies = {NULL},
};

static GPUMaterialLibrary gpu_shader_material_bevel_library = {
    .code = datatoc_gpu_shader_material_bevel_glsl,
    .dependencies = {NULL},
};

static GPUMaterialLibrary gpu_shader_material_blackbody_library = {
    .code = datatoc_gpu_shader_material_blackbody_glsl,
    .dependencies = {NULL},
};

static GPUMaterialLibrary gpu_shader_material_bright_contrast_library = {
    .code = datatoc_gpu_shader_material_bright_contrast_glsl,
    .dependencies = {NULL},
};

static GPUMaterialLibrary gpu_shader_material_bump_library = {
    .code = datatoc_gpu_shader_material_bump_glsl,
    .dependencies = {NULL},
};

static GPUMaterialLibrary gpu_shader_material_camera_library = {
    .code = datatoc_gpu_shader_material_camera_glsl,
    .dependencies = {NULL},
};

static GPUMaterialLibrary gpu_shader_material_clamp_library = {
    .code = datatoc_gpu_shader_material_clamp_glsl,
    .dependencies = {NULL},
};

static GPUMaterialLibrary gpu_shader_material_color_ramp_library = {
    .code = datatoc_gpu_shader_material_color_ramp_glsl,
    .dependencies = {NULL},
};

static GPUMaterialLibrary gpu_shader_material_combine_hsv_library = {
    .code = datatoc_gpu_shader_material_combine_hsv_glsl,
    .dependencies = {&gpu_shader_material_color_util_library, NULL},
};

static GPUMaterialLibrary gpu_shader_material_combine_rgb_library = {
    .code = datatoc_gpu_shader_material_combine_rgb_glsl,
    .dependencies = {NULL},
};

static GPUMaterialLibrary gpu_shader_material_combine_xyz_library = {
    .code = datatoc_gpu_shader_material_combine_xyz_glsl,
    .dependencies = {NULL},
};

static GPUMaterialLibrary gpu_shader_material_diffuse_library = {
    .code = datatoc_gpu_shader_material_diffuse_glsl,
    .dependencies = {NULL},
};

static GPUMaterialLibrary gpu_shader_material_displacement_library = {
    .code = datatoc_gpu_shader_material_displacement_glsl,
    .dependencies = {NULL},
};

static GPUMaterialLibrary gpu_shader_material_eevee_specular_library = {
    .code = datatoc_gpu_shader_material_eevee_specular_glsl,
    .dependencies = {NULL},
};

static GPUMaterialLibrary gpu_shader_material_emission_library = {
    .code = datatoc_gpu_shader_material_emission_glsl,
    .dependencies = {NULL},
};

static GPUMaterialLibrary gpu_shader_material_fresnel_library = {
    .code = datatoc_gpu_shader_material_fresnel_glsl,
    .dependencies = {NULL},
};

static GPUMaterialLibrary gpu_shader_material_gamma_library = {
    .code = datatoc_gpu_shader_material_gamma_glsl,
    .dependencies = {&gpu_shader_material_math_util_library, NULL},
};

static GPUMaterialLibrary gpu_shader_material_tangent_library = {
    .code = datatoc_gpu_shader_material_tangent_glsl,
    .dependencies = {NULL},
};

static GPUMaterialLibrary gpu_shader_material_geometry_library = {
    .code = datatoc_gpu_shader_material_geometry_glsl,
    .dependencies = {&gpu_shader_material_tangent_library, NULL},
};

static GPUMaterialLibrary gpu_shader_material_glass_library = {
    .code = datatoc_gpu_shader_material_glass_glsl,
    .dependencies = {NULL},
};

static GPUMaterialLibrary gpu_shader_material_hair_info_library = {
    .code = datatoc_gpu_shader_material_hair_info_glsl,
    .dependencies = {NULL},
};

static GPUMaterialLibrary gpu_shader_material_holdout_library = {
    .code = datatoc_gpu_shader_material_holdout_glsl,
    .dependencies = {NULL},
};

static GPUMaterialLibrary gpu_shader_material_hue_sat_val_library = {
    .code = datatoc_gpu_shader_material_hue_sat_val_glsl,
    .dependencies = {&gpu_shader_material_color_util_library, NULL},
};

static GPUMaterialLibrary gpu_shader_material_invert_library = {
    .code = datatoc_gpu_shader_material_invert_glsl,
    .dependencies = {NULL},
};

static GPUMaterialLibrary gpu_shader_material_layer_weight_library = {
    .code = datatoc_gpu_shader_material_layer_weight_glsl,
    .dependencies = {&gpu_shader_material_fresnel_library, NULL},
};

static GPUMaterialLibrary gpu_shader_material_light_falloff_library = {
    .code = datatoc_gpu_shader_material_light_falloff_glsl,
    .dependencies = {NULL},
};

static GPUMaterialLibrary gpu_shader_material_light_path_library = {
    .code = datatoc_gpu_shader_material_light_path_glsl,
    .dependencies = {NULL},
};

static GPUMaterialLibrary gpu_shader_material_mapping_library = {
    .code = datatoc_gpu_shader_material_mapping_glsl,
    .dependencies = {&gpu_shader_material_math_util_library, NULL},
};

static GPUMaterialLibrary gpu_shader_material_map_range_library = {
    .code = datatoc_gpu_shader_material_map_range_glsl,
    .dependencies = {&gpu_shader_material_math_util_library, NULL},
};

static GPUMaterialLibrary gpu_shader_material_math_library = {
    .code = datatoc_gpu_shader_material_math_glsl,
    .dependencies = {&gpu_shader_material_math_util_library, NULL},
};

static GPUMaterialLibrary gpu_shader_material_mix_rgb_library = {
    .code = datatoc_gpu_shader_material_mix_rgb_glsl,
    .dependencies = {&gpu_shader_material_color_util_library, NULL},
};

static GPUMaterialLibrary gpu_shader_material_mix_shader_library = {
    .code = datatoc_gpu_shader_material_mix_shader_glsl,
    .dependencies = {NULL},
};

static GPUMaterialLibrary gpu_shader_material_normal_library = {
    .code = datatoc_gpu_shader_material_normal_glsl,
    .dependencies = {NULL},
};

static GPUMaterialLibrary gpu_shader_material_normal_map_library = {
    .code = datatoc_gpu_shader_material_normal_map_glsl,
    .dependencies = {NULL},
};

static GPUMaterialLibrary gpu_shader_material_object_info_library = {
    .code = datatoc_gpu_shader_material_object_info_glsl,
    .dependencies = {NULL},
};

static GPUMaterialLibrary gpu_shader_material_output_aov_library = {
    .code = datatoc_gpu_shader_material_output_aov_glsl,
    .dependencies = {NULL},
};

static GPUMaterialLibrary gpu_shader_material_output_material_library = {
    .code = datatoc_gpu_shader_material_output_material_glsl,
    .dependencies = {NULL},
};

static GPUMaterialLibrary gpu_shader_material_output_world_library = {
    .code = datatoc_gpu_shader_material_output_world_glsl,
    .dependencies = {NULL},
};

static GPUMaterialLibrary gpu_shader_material_particle_info_library = {
    .code = datatoc_gpu_shader_material_particle_info_glsl,
    .dependencies = {NULL},
};

static GPUMaterialLibrary gpu_shader_material_principled_library = {
    .code = datatoc_gpu_shader_material_principled_glsl,
    .dependencies = {NULL},
};

static GPUMaterialLibrary gpu_shader_material_refraction_library = {
    .code = datatoc_gpu_shader_material_refraction_glsl,
    .dependencies = {NULL},
};

static GPUMaterialLibrary gpu_shader_material_rgb_curves_library = {
    .code = datatoc_gpu_shader_material_rgb_curves_glsl,
    .dependencies = {NULL},
};

static GPUMaterialLibrary gpu_shader_material_rgb_to_bw_library = {
    .code = datatoc_gpu_shader_material_rgb_to_bw_glsl,
    .dependencies = {NULL},
};

static GPUMaterialLibrary gpu_shader_material_separate_hsv_library = {
    .code = datatoc_gpu_shader_material_separate_hsv_glsl,
    .dependencies = {&gpu_shader_material_color_util_library, NULL},
};

static GPUMaterialLibrary gpu_shader_material_separate_rgb_library = {
    .code = datatoc_gpu_shader_material_separate_rgb_glsl,
    .dependencies = {NULL},
};

static GPUMaterialLibrary gpu_shader_material_separate_xyz_library = {
    .code = datatoc_gpu_shader_material_separate_xyz_glsl,
    .dependencies = {NULL},
};

static GPUMaterialLibrary gpu_shader_material_set_library = {
    .code = datatoc_gpu_shader_material_set_glsl,
    .dependencies = {NULL},
};

static GPUMaterialLibrary gpu_shader_material_shader_to_rgba_library = {
    .code = datatoc_gpu_shader_material_shader_to_rgba_glsl,
    .dependencies = {NULL},
};

static GPUMaterialLibrary gpu_shader_material_squeeze_library = {
    .code = datatoc_gpu_shader_material_squeeze_glsl,
    .dependencies = {NULL},
};

static GPUMaterialLibrary gpu_shader_material_subsurface_scattering_library = {
    .code = datatoc_gpu_shader_material_subsurface_scattering_glsl,
    .dependencies = {&gpu_shader_material_diffuse_library, NULL},
};

static GPUMaterialLibrary gpu_shader_material_tex_brick_library = {
    .code = datatoc_gpu_shader_material_tex_brick_glsl,
    .dependencies = {&gpu_shader_material_math_util_library,
                     &gpu_shader_material_hash_library,
                     NULL},
};

static GPUMaterialLibrary gpu_shader_material_tex_checker_library = {
    .code = datatoc_gpu_shader_material_tex_checker_glsl,
    .dependencies = {NULL},
};

static GPUMaterialLibrary gpu_shader_material_tex_environment_library = {
    .code = datatoc_gpu_shader_material_tex_environment_glsl,
    .dependencies = {&gpu_shader_material_math_util_library, NULL},
};

static GPUMaterialLibrary gpu_shader_material_tex_gradient_library = {
    .code = datatoc_gpu_shader_material_tex_gradient_glsl,
    .dependencies = {NULL},
};

static GPUMaterialLibrary gpu_shader_material_tex_image_library = {
    .code = datatoc_gpu_shader_material_tex_image_glsl,
    .dependencies = {NULL},
};

static GPUMaterialLibrary gpu_shader_material_tex_magic_library = {
    .code = datatoc_gpu_shader_material_tex_magic_glsl,
    .dependencies = {NULL},
};

static GPUMaterialLibrary gpu_shader_material_tex_musgrave_library = {
    .code = datatoc_gpu_shader_material_tex_musgrave_glsl,
    .dependencies = {&gpu_shader_material_noise_library, NULL},
};

static GPUMaterialLibrary gpu_shader_material_tex_noise_library = {
    .code = datatoc_gpu_shader_material_tex_noise_glsl,
    .dependencies = {&gpu_shader_material_fractal_noise_library, NULL},
};

static GPUMaterialLibrary gpu_shader_material_tex_sky_library = {
    .code = datatoc_gpu_shader_material_tex_sky_glsl,
    .dependencies = {NULL},
};

static GPUMaterialLibrary gpu_shader_material_texture_coordinates_library = {
    .code = datatoc_gpu_shader_material_texture_coordinates_glsl,
    .dependencies = {NULL},
};

static GPUMaterialLibrary gpu_shader_material_tex_voronoi_library = {
    .code = datatoc_gpu_shader_material_tex_voronoi_glsl,
    .dependencies = {&gpu_shader_material_math_util_library,
                     &gpu_shader_material_hash_library,
                     NULL},
};

static GPUMaterialLibrary gpu_shader_material_tex_wave_library = {
    .code = datatoc_gpu_shader_material_tex_wave_glsl,
    .dependencies = {&gpu_shader_material_fractal_noise_library, NULL},
};

static GPUMaterialLibrary gpu_shader_material_tex_white_noise_library = {
    .code = datatoc_gpu_shader_material_tex_white_noise_glsl,
    .dependencies = {&gpu_shader_material_hash_library, NULL},
};

static GPUMaterialLibrary gpu_shader_material_toon_library = {
    .code = datatoc_gpu_shader_material_toon_glsl,
    .dependencies = {&gpu_shader_material_diffuse_library, NULL},
};

static GPUMaterialLibrary gpu_shader_material_translucent_library = {
    .code = datatoc_gpu_shader_material_translucent_glsl,
    .dependencies = {&gpu_shader_material_diffuse_library, NULL},
};

static GPUMaterialLibrary gpu_shader_material_transparent_library = {
    .code = datatoc_gpu_shader_material_transparent_glsl,
    .dependencies = {NULL},
};

static GPUMaterialLibrary gpu_shader_material_uv_map_library = {
    .code = datatoc_gpu_shader_material_uv_map_glsl,
    .dependencies = {NULL},
};

static GPUMaterialLibrary gpu_shader_material_vector_curves_library = {
    .code = datatoc_gpu_shader_material_vector_curves_glsl,
    .dependencies = {NULL},
};

static GPUMaterialLibrary gpu_shader_material_vector_displacement_library = {
    .code = datatoc_gpu_shader_material_vector_displacement_glsl,
    .dependencies = {NULL},
};

static GPUMaterialLibrary gpu_shader_material_vector_math_library = {
    .code = datatoc_gpu_shader_material_vector_math_glsl,
    .dependencies = {&gpu_shader_material_math_util_library, NULL},
};

static GPUMaterialLibrary gpu_shader_material_vector_rotate_library = {
    .code = datatoc_gpu_shader_material_vector_rotate_glsl,
    .dependencies = {&gpu_shader_material_math_util_library, NULL},
};

static GPUMaterialLibrary gpu_shader_material_velvet_library = {
    .code = datatoc_gpu_shader_material_velvet_glsl,
    .dependencies = {&gpu_shader_material_diffuse_library, NULL},
};

static GPUMaterialLibrary gpu_shader_material_vertex_color_library = {
    .code = datatoc_gpu_shader_material_vertex_color_glsl,
    .dependencies = {NULL},
};

static GPUMaterialLibrary gpu_shader_material_volume_absorption_library = {
    .code = datatoc_gpu_shader_material_volume_absorption_glsl,
    .dependencies = {NULL},
};

static GPUMaterialLibrary gpu_shader_material_volume_info_library = {
    .code = datatoc_gpu_shader_material_volume_info_glsl,
    .dependencies = {NULL},
};

static GPUMaterialLibrary gpu_shader_material_volume_principled_library = {
    .code = datatoc_gpu_shader_material_volume_principled_glsl,
    .dependencies = {&gpu_shader_material_blackbody_library, NULL},
};

static GPUMaterialLibrary gpu_shader_material_volume_scatter_library = {
    .code = datatoc_gpu_shader_material_volume_scatter_glsl,
    .dependencies = {NULL},
};

static GPUMaterialLibrary gpu_shader_material_wireframe_library = {
    .code = datatoc_gpu_shader_material_wireframe_glsl,
    .dependencies = {NULL},
};

static GPUMaterialLibrary gpu_shader_material_world_normals_library = {
    .code = datatoc_gpu_shader_material_world_normals_glsl,
    .dependencies = {&gpu_shader_material_texture_coordinates_library, NULL},
};

static GPUMaterialLibrary *gpu_material_libraries[] = {
    &gpu_shader_material_math_util_library,
    &gpu_shader_material_color_util_library,
    &gpu_shader_material_hash_library,
    &gpu_shader_material_noise_library,
    &gpu_shader_material_fractal_noise_library,
    &gpu_shader_material_add_shader_library,
    &gpu_shader_material_ambient_occlusion_library,
    &gpu_shader_material_glossy_library,
    &gpu_shader_material_anisotropic_library,
    &gpu_shader_material_attribute_library,
    &gpu_shader_material_background_library,
    &gpu_shader_material_bevel_library,
    &gpu_shader_material_blackbody_library,
    &gpu_shader_material_bright_contrast_library,
    &gpu_shader_material_bump_library,
    &gpu_shader_material_camera_library,
    &gpu_shader_material_clamp_library,
    &gpu_shader_material_color_ramp_library,
    &gpu_shader_material_combine_hsv_library,
    &gpu_shader_material_combine_rgb_library,
    &gpu_shader_material_combine_xyz_library,
    &gpu_shader_material_diffuse_library,
    &gpu_shader_material_displacement_library,
    &gpu_shader_material_eevee_specular_library,
    &gpu_shader_material_emission_library,
    &gpu_shader_material_fresnel_library,
    &gpu_shader_material_gamma_library,
    &gpu_shader_material_tangent_library,
    &gpu_shader_material_geometry_library,
    &gpu_shader_material_glass_library,
    &gpu_shader_material_hair_info_library,
    &gpu_shader_material_holdout_library,
    &gpu_shader_material_hue_sat_val_library,
    &gpu_shader_material_invert_library,
    &gpu_shader_material_layer_weight_library,
    &gpu_shader_material_light_falloff_library,
    &gpu_shader_material_light_path_library,
    &gpu_shader_material_mapping_library,
    &gpu_shader_material_map_range_library,
    &gpu_shader_material_math_library,
    &gpu_shader_material_mix_rgb_library,
    &gpu_shader_material_mix_shader_library,
    &gpu_shader_material_normal_library,
    &gpu_shader_material_normal_map_library,
    &gpu_shader_material_object_info_library,
    &gpu_shader_material_output_aov_library,
    &gpu_shader_material_output_material_library,
    &gpu_shader_material_output_world_library,
    &gpu_shader_material_particle_info_library,
    &gpu_shader_material_principled_library,
    &gpu_shader_material_refraction_library,
    &gpu_shader_material_rgb_curves_library,
    &gpu_shader_material_rgb_to_bw_library,
    &gpu_shader_material_separate_hsv_library,
    &gpu_shader_material_separate_rgb_library,
    &gpu_shader_material_separate_xyz_library,
    &gpu_shader_material_set_library,
    &gpu_shader_material_shader_to_rgba_library,
    &gpu_shader_material_squeeze_library,
    &gpu_shader_material_subsurface_scattering_library,
    &gpu_shader_material_tex_brick_library,
    &gpu_shader_material_tex_checker_library,
    &gpu_shader_material_tex_environment_library,
    &gpu_shader_material_tex_gradient_library,
    &gpu_shader_material_tex_image_library,
    &gpu_shader_material_tex_magic_library,
    &gpu_shader_material_tex_musgrave_library,
    &gpu_shader_material_tex_noise_library,
    &gpu_shader_material_tex_sky_library,
    &gpu_shader_material_texture_coordinates_library,
    &gpu_shader_material_tex_voronoi_library,
    &gpu_shader_material_tex_wave_library,
    &gpu_shader_material_tex_white_noise_library,
    &gpu_shader_material_toon_library,
    &gpu_shader_material_translucent_library,
    &gpu_shader_material_transparent_library,
    &gpu_shader_material_uv_map_library,
    &gpu_shader_material_vector_curves_library,
    &gpu_shader_material_vector_displacement_library,
    &gpu_shader_material_vector_math_library,
    &gpu_shader_material_vector_rotate_library,
    &gpu_shader_material_velvet_library,
    &gpu_shader_material_vertex_color_library,
    &gpu_shader_material_volume_absorption_library,
    &gpu_shader_material_volume_info_library,
    &gpu_shader_material_volume_principled_library,
    &gpu_shader_material_volume_scatter_library,
    &gpu_shader_material_wireframe_library,
    &gpu_shader_material_world_normals_library,
    NULL};

/* GLSL code parsing for finding function definitions.
 * These are stored in a hash for lookup when creating a material. */

static GHash *FUNCTION_HASH = NULL;

char *gpu_str_skip_token(char *str, char *token, int max)
{
  int len = 0;

  /* skip a variable/function name */
  while (*str) {
    if (ELEM(*str, ' ', '(', ')', ',', ';', '\t', '\n', '\r')) {
      break;
    }

    if (token && len < max - 1) {
      *token = *str;
      token++;
      len++;
    }
    str++;
  }

  if (token) {
    *token = '\0';
  }

  /* skip the next special characters:
   * note the missing ')' */
  while (*str) {
    if (ELEM(*str, ' ', '(', ',', ';', '\t', '\n', '\r')) {
      str++;
    }
    else {
      break;
    }
  }

  return str;
}

/* Indices match the eGPUType enum */
static const char *GPU_DATATYPE_STR[17] = {
    "",
    "float",
    "vec2",
    "vec3",
    "vec4",
    NULL,
    NULL,
    NULL,
    NULL,
    "mat3",
    NULL,
    NULL,
    NULL,
    NULL,
    NULL,
    NULL,
    "mat4",
};

const char *gpu_data_type_to_string(const eGPUType type)
{
  return GPU_DATATYPE_STR[type];
}

static void gpu_parse_material_library(GHash *hash, GPUMaterialLibrary *library)
{
  GPUFunction *function;
  eGPUType type;
  GPUFunctionQual qual;
  int i;
  char *code = library->code;

  while ((code = strstr(code, "void "))) {
    function = MEM_callocN(sizeof(GPUFunction), "GPUFunction");
    function->library = library;

    code = gpu_str_skip_token(code, NULL, 0);
    code = gpu_str_skip_token(code, function->name, MAX_FUNCTION_NAME);

    /* get parameters */
    while (*code && *code != ')') {
      if (BLI_str_startswith(code, "const ")) {
        code = gpu_str_skip_token(code, NULL, 0);
      }

      /* test if it's an input or output */
      qual = FUNCTION_QUAL_IN;
      if (BLI_str_startswith(code, "out ")) {
        qual = FUNCTION_QUAL_OUT;
      }
      if (BLI_str_startswith(code, "inout ")) {
        qual = FUNCTION_QUAL_INOUT;
      }
      if ((qual != FUNCTION_QUAL_IN) || BLI_str_startswith(code, "in ")) {
        code = gpu_str_skip_token(code, NULL, 0);
      }

      /* test for type */
      type = GPU_NONE;
      for (i = 1; i < ARRAY_SIZE(GPU_DATATYPE_STR); i++) {
        if (GPU_DATATYPE_STR[i] && BLI_str_startswith(code, GPU_DATATYPE_STR[i])) {
          type = i;
          break;
        }
      }

      if (!type && BLI_str_startswith(code, "samplerCube")) {
        type = GPU_TEXCUBE;
      }
      if (!type && BLI_str_startswith(code, "sampler2DShadow")) {
        type = GPU_SHADOW2D;
      }
      if (!type && BLI_str_startswith(code, "sampler1DArray")) {
        type = GPU_TEX1D_ARRAY;
      }
      if (!type && BLI_str_startswith(code, "sampler2DArray")) {
        type = GPU_TEX2D_ARRAY;
      }
      if (!type && BLI_str_startswith(code, "sampler2D")) {
        type = GPU_TEX2D;
      }
      if (!type && BLI_str_startswith(code, "sampler3D")) {
        type = GPU_TEX3D;
      }

      if (!type && BLI_str_startswith(code, "Closure")) {
        type = GPU_CLOSURE;
      }

      if (type) {
        /* add parameter */
        code = gpu_str_skip_token(code, NULL, 0);
        code = gpu_str_skip_token(code, NULL, 0);
        function->paramqual[function->totparam] = qual;
        function->paramtype[function->totparam] = type;
        function->totparam++;
      }
      else {
        fprintf(stderr, "GPU invalid function parameter in %s.\n", function->name);
        break;
      }
    }

    if (function->name[0] == '\0' || function->totparam == 0) {
      fprintf(stderr, "GPU functions parse error.\n");
      MEM_freeN(function);
      break;
    }

    BLI_ghash_insert(hash, function->name, function);
  }
}

/* Module */

void gpu_material_library_init(void)
{
  /* Only parse GLSL shader files once. */
  if (FUNCTION_HASH) {
    return;
  }

  FUNCTION_HASH = BLI_ghash_str_new("GPU_lookup_function gh");
  for (int i = 0; gpu_material_libraries[i]; i++) {
    gpu_parse_material_library(FUNCTION_HASH, gpu_material_libraries[i]);
  }
}

void gpu_material_library_exit(void)
{
  if (FUNCTION_HASH) {
    BLI_ghash_free(FUNCTION_HASH, NULL, MEM_freeN);
    FUNCTION_HASH = NULL;
  }
}

/* Code Generation */

static void gpu_material_use_library_with_dependencies(GSet *used_libraries,
                                                       GPUMaterialLibrary *library)
{
  if (BLI_gset_add(used_libraries, library->code)) {
    for (int i = 0; library->dependencies[i]; i++) {
      gpu_material_use_library_with_dependencies(used_libraries, library->dependencies[i]);
    }
  }
}

GPUFunction *gpu_material_library_use_function(GSet *used_libraries, const char *name)
{
  GPUFunction *function = BLI_ghash_lookup(FUNCTION_HASH, (const void *)name);
  if (function) {
    gpu_material_use_library_with_dependencies(used_libraries, function->library);
  }
  return function;
}

char *gpu_material_library_generate_code(GSet *used_libraries, const char *frag_lib)
{
  DynStr *ds = BLI_dynstr_new();

  if (frag_lib) {
    BLI_dynstr_append(ds, frag_lib);
  }

  /* Always include those because they may be needed by the execution function. */
  gpu_material_use_library_with_dependencies(used_libraries,
                                             &gpu_shader_material_world_normals_library);

  /* Add library code in order, for dependencies. */
  for (int i = 0; gpu_material_libraries[i]; i++) {
    GPUMaterialLibrary *library = gpu_material_libraries[i];
    if (BLI_gset_haskey(used_libraries, library->code)) {
      BLI_dynstr_append(ds, library->code);
    }
  }

  char *result = BLI_dynstr_get_cstring(ds);
  BLI_dynstr_free(ds);

  return result;
}
