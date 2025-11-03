/* SPDX-FileCopyrightText: 2023 Blender Authors
 *
 * SPDX-License-Identifier: GPL-2.0-or-later */

#ifdef GPU_SHADER
#  pragma once
#  include "BLI_utildefines_variadic.h"

#  include "gpu_shader_compat.hh"

#  include "draw_view_infos.hh"
#  include "eevee_common_infos.hh"
#  include "eevee_depth_of_field_shared.hh"
#  include "eevee_sampling_infos.hh"
#  include "eevee_velocity_infos.hh"

#  define DOF_BOKEH_TEXTURE true
#  define DILATE_MODE_MIN_MAX true
#endif

#include "eevee_defines.hh"
#include "gpu_shader_create_info.hh"

GPU_SHADER_CREATE_INFO(eevee_depth_of_field_tiles_common)
IMAGE(0, UFLOAT_11_11_10, read, image2D, in_tiles_fg_img)
IMAGE(1, UFLOAT_11_11_10, read, image2D, in_tiles_bg_img)
GPU_SHADER_CREATE_END()

/* -------------------------------------------------------------------- */
/** \name Setup
 * \{ */

GPU_SHADER_CREATE_INFO(eevee_depth_of_field_bokeh_lut)
DO_STATIC_COMPILATION()
LOCAL_GROUP_SIZE(DOF_BOKEH_LUT_SIZE, DOF_BOKEH_LUT_SIZE)
TYPEDEF_SOURCE("eevee_defines.hh")
TYPEDEF_SOURCE("eevee_depth_of_field_shared.hh")
ADDITIONAL_INFO(draw_view)
UNIFORM_BUF(6, DepthOfFieldData, dof_buf)
IMAGE(0, SFLOAT_16_16, write, image2D, out_gather_lut_img)
IMAGE(1, SFLOAT_16, write, image2D, out_scatter_lut_img)
IMAGE(2, SFLOAT_16, write, image2D, out_resolve_lut_img)
COMPUTE_SOURCE("eevee_depth_of_field_bokeh_lut_comp.glsl")
GPU_SHADER_CREATE_END()

GPU_SHADER_CREATE_INFO(eevee_depth_of_field_setup)
DO_STATIC_COMPILATION()
LOCAL_GROUP_SIZE(DOF_DEFAULT_GROUP_SIZE, DOF_DEFAULT_GROUP_SIZE)
TYPEDEF_SOURCE("eevee_defines.hh")
TYPEDEF_SOURCE("eevee_depth_of_field_shared.hh")
ADDITIONAL_INFO(draw_view)
UNIFORM_BUF(6, DepthOfFieldData, dof_buf)
SAMPLER(0, sampler2D, color_tx)
SAMPLER(1, sampler2DDepth, depth_tx)
IMAGE(0, SFLOAT_16_16_16_16, write, image2D, out_color_img)
IMAGE(1, SFLOAT_16, write, image2D, out_coc_img)
COMPUTE_SOURCE("eevee_depth_of_field_setup_comp.glsl")
GPU_SHADER_CREATE_END()

GPU_SHADER_CREATE_INFO(eevee_depth_of_field_stabilize)
DO_STATIC_COMPILATION()
LOCAL_GROUP_SIZE(DOF_STABILIZE_GROUP_SIZE, DOF_STABILIZE_GROUP_SIZE)
TYPEDEF_SOURCE("eevee_defines.hh")
TYPEDEF_SOURCE("eevee_depth_of_field_shared.hh")
ADDITIONAL_INFO(draw_view)
ADDITIONAL_INFO(eevee_velocity_camera)
UNIFORM_BUF(6, DepthOfFieldData, dof_buf)
SAMPLER(0, sampler2D, coc_tx)
SAMPLER(1, sampler2D, color_tx)
SAMPLER(2, sampler2D, velocity_tx)
SAMPLER(3, sampler2D, in_history_tx)
SAMPLER(4, sampler2DDepth, depth_tx)
PUSH_CONSTANT(bool, u_use_history)
IMAGE(0, SFLOAT_16_16_16_16, write, image2D, out_color_img)
IMAGE(1, SFLOAT_16, write, image2D, out_coc_img)
IMAGE(2, SFLOAT_16_16_16_16, write, image2D, out_history_img)
COMPUTE_SOURCE("eevee_depth_of_field_stabilize_comp.glsl")
GPU_SHADER_CREATE_END()

GPU_SHADER_CREATE_INFO(eevee_depth_of_field_downsample)
DO_STATIC_COMPILATION()
LOCAL_GROUP_SIZE(DOF_DEFAULT_GROUP_SIZE, DOF_DEFAULT_GROUP_SIZE)
TYPEDEF_SOURCE("eevee_defines.hh")
TYPEDEF_SOURCE("eevee_depth_of_field_shared.hh")
ADDITIONAL_INFO(draw_view)
SAMPLER(0, sampler2D, color_tx)
SAMPLER(1, sampler2D, coc_tx)
IMAGE(0, SFLOAT_16_16_16_16, write, image2D, out_color_img)
COMPUTE_SOURCE("eevee_depth_of_field_downsample_comp.glsl")
GPU_SHADER_CREATE_END()

GPU_SHADER_CREATE_INFO(eevee_depth_of_field_reduce)
DO_STATIC_COMPILATION()
LOCAL_GROUP_SIZE(DOF_REDUCE_GROUP_SIZE, DOF_REDUCE_GROUP_SIZE)
TYPEDEF_SOURCE("eevee_defines.hh")
TYPEDEF_SOURCE("eevee_depth_of_field_shared.hh")
ADDITIONAL_INFO(draw_view)
UNIFORM_BUF(6, DepthOfFieldData, dof_buf)
SAMPLER(0, sampler2D, downsample_tx)
STORAGE_BUF(0, write, ScatterRect, scatter_fg_list_buf[])
STORAGE_BUF(1, write, ScatterRect, scatter_bg_list_buf[])
STORAGE_BUF(2, read_write, DrawCommand, scatter_fg_indirect_buf)
STORAGE_BUF(3, read_write, DrawCommand, scatter_bg_indirect_buf)
IMAGE(0, SFLOAT_16_16_16_16, read_write, image2D, inout_color_lod0_img)
IMAGE(1, SFLOAT_16_16_16_16, write, image2D, out_color_lod1_img)
IMAGE(2, SFLOAT_16_16_16_16, write, image2D, out_color_lod2_img)
IMAGE(3, SFLOAT_16_16_16_16, write, image2D, out_color_lod3_img)
IMAGE(4, SFLOAT_16, read, image2D, in_coc_lod0_img)
IMAGE(5, SFLOAT_16, write, image2D, out_coc_lod1_img)
IMAGE(6, SFLOAT_16, write, image2D, out_coc_lod2_img)
IMAGE(7, SFLOAT_16, write, image2D, out_coc_lod3_img)
COMPUTE_SOURCE("eevee_depth_of_field_reduce_comp.glsl")
GPU_SHADER_CREATE_END()

/** \} */

/* -------------------------------------------------------------------- */
/** \name Circle-Of-Confusion Tiles
 * \{ */

GPU_SHADER_CREATE_INFO(eevee_depth_of_field_tiles_flatten)
DO_STATIC_COMPILATION()
LOCAL_GROUP_SIZE(DOF_TILES_FLATTEN_GROUP_SIZE, DOF_TILES_FLATTEN_GROUP_SIZE)
TYPEDEF_SOURCE("eevee_defines.hh")
TYPEDEF_SOURCE("eevee_depth_of_field_shared.hh")
ADDITIONAL_INFO(draw_view)
SAMPLER(0, sampler2D, coc_tx)
IMAGE(2, UFLOAT_11_11_10, write, image2D, out_tiles_fg_img)
IMAGE(3, UFLOAT_11_11_10, write, image2D, out_tiles_bg_img)
COMPUTE_SOURCE("eevee_depth_of_field_tiles_flatten_comp.glsl")
GPU_SHADER_CREATE_END()

GPU_SHADER_CREATE_INFO(eevee_depth_of_field_tiles_dilate)
TYPEDEF_SOURCE("eevee_defines.hh")
TYPEDEF_SOURCE("eevee_depth_of_field_shared.hh")
ADDITIONAL_INFO(draw_view)
ADDITIONAL_INFO(eevee_depth_of_field_tiles_common)
LOCAL_GROUP_SIZE(DOF_TILES_DILATE_GROUP_SIZE, DOF_TILES_DILATE_GROUP_SIZE)
IMAGE(2, UFLOAT_11_11_10, write, image2D, out_tiles_fg_img)
IMAGE(3, UFLOAT_11_11_10, write, image2D, out_tiles_bg_img)
PUSH_CONSTANT(int, ring_count)
PUSH_CONSTANT(int, ring_width_multiplier)
COMPUTE_SOURCE("eevee_depth_of_field_tiles_dilate_comp.glsl")
GPU_SHADER_CREATE_END()

GPU_SHADER_CREATE_INFO(eevee_depth_of_field_tiles_dilate_minabs)
DO_STATIC_COMPILATION()
DEFINE_VALUE("DILATE_MODE_MIN_MAX", "false")
ADDITIONAL_INFO(eevee_depth_of_field_tiles_dilate)
GPU_SHADER_CREATE_END()

GPU_SHADER_CREATE_INFO(eevee_depth_of_field_tiles_dilate_minmax)
DO_STATIC_COMPILATION()
DEFINE_VALUE("DILATE_MODE_MIN_MAX", "true")
ADDITIONAL_INFO(eevee_depth_of_field_tiles_dilate)
GPU_SHADER_CREATE_END()

/** \} */

/* -------------------------------------------------------------------- */
/** \name Variations
 * \{ */

GPU_SHADER_CREATE_INFO(eevee_depth_of_field_no_lut)
DEFINE_VALUE("DOF_BOKEH_TEXTURE", "false")
/**
 * WORKAROUND(@fclem): This is to keep the code as is for now. The bokeh_lut_tx is referenced
 * even if not used after optimization. But we don't want to include it in the create information.
 */
DEFINE_VALUE("bokeh_lut_tx", "color_tx")
GPU_SHADER_CREATE_END()

GPU_SHADER_CREATE_INFO(eevee_depth_of_field_lut)
DEFINE_VALUE("DOF_BOKEH_TEXTURE", "true")
SAMPLER(5, sampler2D, bokeh_lut_tx)
GPU_SHADER_CREATE_END()

GPU_SHADER_CREATE_INFO(eevee_depth_of_field_background)
DEFINE_VALUE("DOF_FOREGROUND_PASS", "false")
GPU_SHADER_CREATE_END()
GPU_SHADER_CREATE_INFO(eevee_depth_of_field_foreground)
DEFINE_VALUE("DOF_FOREGROUND_PASS", "true")
GPU_SHADER_CREATE_END()

/** \} */

/* -------------------------------------------------------------------- */
/** \name Gather
 * \{ */

GPU_SHADER_CREATE_INFO(eevee_depth_of_field_gather_common)
TYPEDEF_SOURCE("eevee_defines.hh")
TYPEDEF_SOURCE("eevee_depth_of_field_shared.hh")
ADDITIONAL_INFO(draw_view)
ADDITIONAL_INFO(eevee_depth_of_field_tiles_common)
ADDITIONAL_INFO(eevee_sampling_data)
UNIFORM_BUF(6, DepthOfFieldData, dof_buf)
LOCAL_GROUP_SIZE(DOF_GATHER_GROUP_SIZE, DOF_GATHER_GROUP_SIZE)
SAMPLER(0, sampler2D, color_tx)
SAMPLER(1, sampler2D, color_bilinear_tx)
SAMPLER(2, sampler2D, coc_tx)
IMAGE(2, SFLOAT_16_16_16_16, write, image2D, out_color_img)
IMAGE(3, SFLOAT_16, write, image2D, out_weight_img)
GPU_SHADER_CREATE_END()

GPU_SHADER_CREATE_INFO(eevee_depth_of_field_gather)
IMAGE(4, SFLOAT_16_16, write, image2D, out_occlusion_img)
COMPUTE_SOURCE("eevee_depth_of_field_gather_comp.glsl")
ADDITIONAL_INFO(eevee_depth_of_field_gather_common)
GPU_SHADER_CREATE_END()

CREATE_INFO_VARIANT(eevee_depth_of_field_gather_background_lut,
                    eevee_depth_of_field_lut,
                    eevee_depth_of_field_background,
                    eevee_depth_of_field_gather)
CREATE_INFO_VARIANT(eevee_depth_of_field_gather_background_no_lut,
                    eevee_depth_of_field_no_lut,
                    eevee_depth_of_field_background,
                    eevee_depth_of_field_gather)
CREATE_INFO_VARIANT(eevee_depth_of_field_gather_foreground_lut,
                    eevee_depth_of_field_lut,
                    eevee_depth_of_field_foreground,
                    eevee_depth_of_field_gather)
CREATE_INFO_VARIANT(eevee_depth_of_field_gather_foreground_no_lut,
                    eevee_depth_of_field_no_lut,
                    eevee_depth_of_field_foreground,
                    eevee_depth_of_field_gather)

GPU_SHADER_CREATE_INFO(eevee_depth_of_field_hole_fill)
DO_STATIC_COMPILATION()
COMPUTE_SOURCE("eevee_depth_of_field_hole_fill_comp.glsl")
ADDITIONAL_INFO(eevee_depth_of_field_gather_common)
ADDITIONAL_INFO(eevee_depth_of_field_no_lut)
GPU_SHADER_CREATE_END()

GPU_SHADER_CREATE_INFO(eevee_depth_of_field_filter)
DO_STATIC_COMPILATION()
LOCAL_GROUP_SIZE(DOF_FILTER_GROUP_SIZE, DOF_FILTER_GROUP_SIZE)
TYPEDEF_SOURCE("eevee_defines.hh")
TYPEDEF_SOURCE("eevee_depth_of_field_shared.hh")
SAMPLER(0, sampler2D, color_tx)
SAMPLER(1, sampler2D, weight_tx)
IMAGE(0, SFLOAT_16_16_16_16, write, image2D, out_color_img)
IMAGE(1, SFLOAT_16, write, image2D, out_weight_img)
COMPUTE_SOURCE("eevee_depth_of_field_filter_comp.glsl")
GPU_SHADER_CREATE_END()

/** \} */

/* -------------------------------------------------------------------- */
/** \name Scatter
 * \{ */

GPU_SHADER_NAMED_INTERFACE_INFO(eevee_depth_of_field_scatter_flat_iface, interp_flat)
/** Colors, weights, and Circle of confusion radii for the 4 pixels to scatter. */
FLAT(float4, color_and_coc1)
FLAT(float4, color_and_coc2)
FLAT(float4, color_and_coc3)
FLAT(float4, color_and_coc4)
/** Scaling factor for the bokeh distance. */
FLAT(float, distance_scale)
GPU_SHADER_NAMED_INTERFACE_END(interp_flat)
GPU_SHADER_NAMED_INTERFACE_INFO(eevee_depth_of_field_scatter_noperspective_iface,
                                interp_noperspective)
/** Sprite pixel position with origin at sprite center. In pixels. */
NO_PERSPECTIVE(float2, rect_uv1)
NO_PERSPECTIVE(float2, rect_uv2)
NO_PERSPECTIVE(float2, rect_uv3)
NO_PERSPECTIVE(float2, rect_uv4)
GPU_SHADER_NAMED_INTERFACE_END(interp_noperspective)

GPU_SHADER_CREATE_INFO(eevee_depth_of_field_scatter)
DO_STATIC_COMPILATION()
TYPEDEF_SOURCE("eevee_defines.hh")
TYPEDEF_SOURCE("eevee_depth_of_field_shared.hh")
ADDITIONAL_INFO(draw_view)
SAMPLER(0, sampler2D, occlusion_tx)
SAMPLER(1, sampler2D, bokeh_lut_tx)
UNIFORM_BUF(6, DepthOfFieldData, dof_buf)
STORAGE_BUF(0, read, ScatterRect, scatter_list_buf[])
FRAGMENT_OUT(0, float4, out_color)
PUSH_CONSTANT(bool, use_bokeh_lut)
VERTEX_OUT(eevee_depth_of_field_scatter_flat_iface)
VERTEX_OUT(eevee_depth_of_field_scatter_noperspective_iface)
VERTEX_SOURCE("eevee_depth_of_field_scatter_vert.glsl")
FRAGMENT_SOURCE("eevee_depth_of_field_scatter_frag.glsl")
GPU_SHADER_CREATE_END()

/** \} */

/* -------------------------------------------------------------------- */
/** \name Resolve
 * \{ */

GPU_SHADER_CREATE_INFO(eevee_depth_of_field_resolve)
DEFINE_VALUE("DOF_RESOLVE_PASS", "true")
LOCAL_GROUP_SIZE(DOF_RESOLVE_GROUP_SIZE, DOF_RESOLVE_GROUP_SIZE)
SPECIALIZATION_CONSTANT(bool, do_debug_color, false)
TYPEDEF_SOURCE("eevee_defines.hh")
TYPEDEF_SOURCE("eevee_depth_of_field_shared.hh")
ADDITIONAL_INFO(draw_view)
ADDITIONAL_INFO(eevee_depth_of_field_tiles_common)
ADDITIONAL_INFO(eevee_sampling_data)
UNIFORM_BUF(6, DepthOfFieldData, dof_buf)
SAMPLER(0, sampler2DDepth, depth_tx)
SAMPLER(1, sampler2D, color_tx)
SAMPLER(2, sampler2D, color_bg_tx)
SAMPLER(3, sampler2D, color_fg_tx)
SAMPLER(4, sampler2D, color_hole_fill_tx)
SAMPLER(7, sampler2D, weight_bg_tx)
SAMPLER(8, sampler2D, weight_fg_tx)
SAMPLER(9, sampler2D, weight_hole_fill_tx)
SAMPLER(10, sampler2D, stable_color_tx)
IMAGE(2, SFLOAT_16_16_16_16, write, image2D, out_color_img)
COMPUTE_SOURCE("eevee_depth_of_field_resolve_comp.glsl")
GPU_SHADER_CREATE_END()

CREATE_INFO_VARIANT(eevee_depth_of_field_resolve_lut,
                    eevee_depth_of_field_lut,
                    eevee_depth_of_field_resolve)
CREATE_INFO_VARIANT(eevee_depth_of_field_resolve_no_lut,
                    eevee_depth_of_field_no_lut,
                    eevee_depth_of_field_resolve)

/** \} */
