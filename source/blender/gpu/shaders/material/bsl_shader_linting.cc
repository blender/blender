/* SPDX-FileCopyrightText: 2026 Blender Authors
 *
 * SPDX-License-Identifier: GPL-2.0-or-later */

/**
 * Compile shader files as C++ inside one compilation unit to lint syntax and get IDE integration.
 */

#include "gpu_shader_material_add_shader.bsl.hh"               /* IWYU pragma: export */
#include "gpu_shader_material_align_rotation_to_vector.bsl.hh" /* IWYU pragma: export */
#include "gpu_shader_material_ambient_occlusion.bsl.hh"        /* IWYU pragma: export */
#include "gpu_shader_material_attribute.bsl.hh"                /* IWYU pragma: export */
#include "gpu_shader_material_axes_to_rotation.bsl.hh"         /* IWYU pragma: export */
#include "gpu_shader_material_axis_angle_to_rotation.bsl.hh"   /* IWYU pragma: export */
#include "gpu_shader_material_background.bsl.hh"               /* IWYU pragma: export */
#include "gpu_shader_material_bevel.bsl.hh"                    /* IWYU pragma: export */
#include "gpu_shader_material_bit_math.bsl.hh"                 /* IWYU pragma: export */
#include "gpu_shader_material_blackbody.bsl.hh"                /* IWYU pragma: export */
#include "gpu_shader_material_boolean_math.bsl.hh"             /* IWYU pragma: export */
#include "gpu_shader_material_bright_contrast.bsl.hh"          /* IWYU pragma: export */
#include "gpu_shader_material_clamp.bsl.hh"                    /* IWYU pragma: export */
#include "gpu_shader_material_combine_color.bsl.hh"            /* IWYU pragma: export */
#include "gpu_shader_material_combine_transform.bsl.hh"        /* IWYU pragma: export */
#include "gpu_shader_material_combine_xyz.bsl.hh"              /* IWYU pragma: export */
#include "gpu_shader_material_compare.bsl.hh"                  /* IWYU pragma: export */
#include "gpu_shader_material_diffuse.bsl.hh"                  /* IWYU pragma: export */
#include "gpu_shader_material_emission.bsl.hh"                 /* IWYU pragma: export */
#include "gpu_shader_material_euler_to_rotation.bsl.hh"        /* IWYU pragma: export */
#include "gpu_shader_material_float_to_int.bsl.hh"             /* IWYU pragma: export */
#include "gpu_shader_material_fractal_noise.bsl.hh"            /* IWYU pragma: export */
#include "gpu_shader_material_fractal_voronoi.bsl.hh"          /* IWYU pragma: export */
#include "gpu_shader_material_fresnel.bsl.hh"                  /* IWYU pragma: export */
#include "gpu_shader_material_gamma.bsl.hh"                    /* IWYU pragma: export */
#include "gpu_shader_material_get_vector_component.bsl.hh"     /* IWYU pragma: export */
#include "gpu_shader_material_glass.bsl.hh"                    /* IWYU pragma: export */
#include "gpu_shader_material_hair.bsl.hh"                     /* IWYU pragma: export */
#include "gpu_shader_material_hair_info.bsl.hh"                /* IWYU pragma: export */
#include "gpu_shader_material_hash_value.bsl.hh"               /* IWYU pragma: export */
#include "gpu_shader_material_holdout.bsl.hh"                  /* IWYU pragma: export */
#include "gpu_shader_material_hue_sat_val.bsl.hh"              /* IWYU pragma: export */
#include "gpu_shader_material_implicit_defaults.bsl.hh"        /* IWYU pragma: export */
#include "gpu_shader_material_integer_math.bsl.hh"             /* IWYU pragma: export */
#include "gpu_shader_material_invert.bsl.hh"                   /* IWYU pragma: export */
#include "gpu_shader_material_invert_rotation.bsl.hh"          /* IWYU pragma: export */
#include "gpu_shader_material_layer_weight.bsl.hh"             /* IWYU pragma: export */
#include "gpu_shader_material_light_accumulation.bsl.hh"       /* IWYU pragma: export */
#include "gpu_shader_material_light_evaluation.bsl.hh"         /* IWYU pragma: export */
#include "gpu_shader_material_light_falloff.bsl.hh"            /* IWYU pragma: export */
#include "gpu_shader_material_light_info.bsl.hh"               /* IWYU pragma: export */
#include "gpu_shader_material_light_iter_internal.bsl.hh"      /* IWYU pragma: export */
#include "gpu_shader_material_map_range.bsl.hh"                /* IWYU pragma: export */
#include "gpu_shader_material_mapping.bsl.hh"                  /* IWYU pragma: export */
#include "gpu_shader_material_matrix_svd.bsl.hh"               /* IWYU pragma: export */
#include "gpu_shader_material_metallic.bsl.hh"                 /* IWYU pragma: export */
#include "gpu_shader_material_mix_color.bsl.hh"                /* IWYU pragma: export */
#include "gpu_shader_material_mix_shader.bsl.hh"               /* IWYU pragma: export */
#include "gpu_shader_material_noise.bsl.hh"                    /* IWYU pragma: export */
#include "gpu_shader_material_normal.bsl.hh"                   /* IWYU pragma: export */
#include "gpu_shader_material_open_pbr_util.bsl.hh"            /* IWYU pragma: export */
#include "gpu_shader_material_particle_info.bsl.hh"            /* IWYU pragma: export */
#include "gpu_shader_material_point_info.bsl.hh"               /* IWYU pragma: export */
#include "gpu_shader_material_principled.bsl.hh"               /* IWYU pragma: export */
#include "gpu_shader_material_quaternion_to_rotation.bsl.hh"   /* IWYU pragma: export */
#include "gpu_shader_material_random_value.bsl.hh"             /* IWYU pragma: export */
#include "gpu_shader_material_ray_portal.bsl.hh"               /* IWYU pragma: export */
#include "gpu_shader_material_raycast.bsl.hh"                  /* IWYU pragma: export */
#include "gpu_shader_material_refraction.bsl.hh"               /* IWYU pragma: export */
#include "gpu_shader_material_repeat_zone.bsl.hh"              /* IWYU pragma: export */
#include "gpu_shader_material_rgb_to_bw.bsl.hh"                /* IWYU pragma: export */
#include "gpu_shader_material_rotate_rotation.bsl.hh"          /* IWYU pragma: export */
#include "gpu_shader_material_rotate_vector.bsl.hh"            /* IWYU pragma: export */
#include "gpu_shader_material_rotation_to_axis_angle.bsl.hh"   /* IWYU pragma: export */
#include "gpu_shader_material_rotation_to_euler.bsl.hh"        /* IWYU pragma: export */
#include "gpu_shader_material_rotation_to_quaternion.bsl.hh"   /* IWYU pragma: export */
#include "gpu_shader_material_scene_time.bsl.hh"               /* IWYU pragma: export */
#include "gpu_shader_material_separate_color.bsl.hh"           /* IWYU pragma: export */
#include "gpu_shader_material_separate_transform.bsl.hh"       /* IWYU pragma: export */
#include "gpu_shader_material_separate_xyz.bsl.hh"             /* IWYU pragma: export */
#include "gpu_shader_material_set.bsl.hh"                      /* IWYU pragma: export */
#include "gpu_shader_material_shader_to_rgba.bsl.hh"           /* IWYU pragma: export */
#include "gpu_shader_material_shadow_raycast.bsl.hh"           /* IWYU pragma: export */
#include "gpu_shader_material_sheen.bsl.hh"                    /* IWYU pragma: export */
#include "gpu_shader_material_squeeze.bsl.hh"                  /* IWYU pragma: export */
#include "gpu_shader_material_subsurface_scattering.bsl.hh"    /* IWYU pragma: export */
#include "gpu_shader_material_tex_brick.bsl.hh"                /* IWYU pragma: export */
#include "gpu_shader_material_tex_checker.bsl.hh"              /* IWYU pragma: export */
#include "gpu_shader_material_tex_environment.bsl.hh"          /* IWYU pragma: export */
#include "gpu_shader_material_tex_gabor.bsl.hh"                /* IWYU pragma: export */
#include "gpu_shader_material_tex_gradient.bsl.hh"             /* IWYU pragma: export */
#include "gpu_shader_material_tex_image.bsl.hh"                /* IWYU pragma: export */
#include "gpu_shader_material_tex_magic.bsl.hh"                /* IWYU pragma: export */
#include "gpu_shader_material_tex_noise.bsl.hh"                /* IWYU pragma: export */
#include "gpu_shader_material_tex_sky.bsl.hh"                  /* IWYU pragma: export */
#include "gpu_shader_material_tex_voronoi.bsl.hh"              /* IWYU pragma: export */
#include "gpu_shader_material_tex_wave.bsl.hh"                 /* IWYU pragma: export */
#include "gpu_shader_material_tex_white_noise.bsl.hh"          /* IWYU pragma: export */
#include "gpu_shader_material_toon.bsl.hh"                     /* IWYU pragma: export */
#include "gpu_shader_material_translucent.bsl.hh"              /* IWYU pragma: export */
#include "gpu_shader_material_transparent.bsl.hh"              /* IWYU pragma: export */
#include "gpu_shader_material_uv_map.bsl.hh"                   /* IWYU pragma: export */
#include "gpu_shader_material_vector_math.bsl.hh"              /* IWYU pragma: export */
#include "gpu_shader_material_vector_rotate.bsl.hh"            /* IWYU pragma: export */
#include "gpu_shader_material_vertex_color.bsl.hh"             /* IWYU pragma: export */
#include "gpu_shader_material_volume_absorption.bsl.hh"        /* IWYU pragma: export */
#include "gpu_shader_material_volume_coefficients.bsl.hh"      /* IWYU pragma: export */
#include "gpu_shader_material_volume_principled.bsl.hh"        /* IWYU pragma: export */
#include "gpu_shader_material_volume_scatter.bsl.hh"           /* IWYU pragma: export */
#include "gpu_shader_material_voronoi.bsl.hh"                  /* IWYU pragma: export */
#include "gpu_shader_material_wavelength.bsl.hh"               /* IWYU pragma: export */
#include "gpu_shader_material_wireframe.bsl.hh"                /* IWYU pragma: export */
// #include "gpu_shader_material_geometry.bsl.hh"                 /* IWYU pragma: export */

void main() {}
