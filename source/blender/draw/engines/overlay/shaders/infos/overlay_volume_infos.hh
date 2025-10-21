/* SPDX-FileCopyrightText: 2023 Blender Authors
 *
 * SPDX-License-Identifier: GPL-2.0-or-later */

#ifdef GPU_SHADER
#  pragma once
#  include "gpu_shader_compat.hh"

#  include "draw_object_infos_infos.hh"
#  include "draw_view_infos.hh"
#  include "overlay_common_infos.hh"

#  define USE_MAC
#  define SHOW_RANGE
#endif

#include "gpu_shader_create_info.hh"

/* -------------------------------------------------------------------- */
/** \name Volume Velocity
 * \{ */

GPU_SHADER_INTERFACE_INFO(overlay_volume_velocity_iface)
SMOOTH(float4, final_color)
GPU_SHADER_INTERFACE_END()

GPU_SHADER_CREATE_INFO(overlay_volume_velocity)
SAMPLER(0, sampler3D, velocity_x)
SAMPLER(1, sampler3D, velocity_y)
SAMPLER(2, sampler3D, velocity_z)
PUSH_CONSTANT(float, display_size)
PUSH_CONSTANT(float, slice_position)
PUSH_CONSTANT(int, slice_axis)
PUSH_CONSTANT(bool, scale_with_magnitude)
PUSH_CONSTANT(bool, is_cell_centered)
/* FluidDomainSettings.cell_size */
PUSH_CONSTANT(float3, cell_size)
/* FluidDomainSettings.p0 */
PUSH_CONSTANT(float3, domain_origin_offset)
/* FluidDomainSettings.res_min */
PUSH_CONSTANT(int3, adaptive_cell_offset)
PUSH_CONSTANT(int, in_select_id)
VERTEX_OUT(overlay_volume_velocity_iface)
FRAGMENT_OUT(0, float4, frag_color)
VERTEX_SOURCE("overlay_volume_velocity_vert.glsl")
FRAGMENT_SOURCE("overlay_varying_color.glsl")
GPU_SHADER_CREATE_END()

GPU_SHADER_CREATE_INFO(overlay_volume_velocity_streamline)
DO_STATIC_COMPILATION()
ADDITIONAL_INFO(draw_volume)
ADDITIONAL_INFO(draw_view)
ADDITIONAL_INFO(overlay_volume_velocity)
GPU_SHADER_CREATE_END()

CREATE_INFO_VARIANT(overlay_volume_velocity_streamline_selectable,
                    overlay_volume_velocity_streamline,
                    overlay_select)

GPU_SHADER_CREATE_INFO(overlay_volume_velocity_mac)
DO_STATIC_COMPILATION()
DEFINE("USE_MAC")
PUSH_CONSTANT(bool, draw_macx)
PUSH_CONSTANT(bool, draw_macy)
PUSH_CONSTANT(bool, draw_macz)
ADDITIONAL_INFO(draw_volume)
ADDITIONAL_INFO(draw_view)
ADDITIONAL_INFO(overlay_volume_velocity)
GPU_SHADER_CREATE_END()

CREATE_INFO_VARIANT(overlay_volume_velocity_mac_selectable,
                    overlay_volume_velocity_mac,
                    overlay_select)

GPU_SHADER_CREATE_INFO(overlay_volume_velocity_needle)
DO_STATIC_COMPILATION()
DEFINE("USE_NEEDLE")
ADDITIONAL_INFO(draw_volume)
ADDITIONAL_INFO(draw_view)
ADDITIONAL_INFO(overlay_volume_velocity)
GPU_SHADER_CREATE_END()

CREATE_INFO_VARIANT(overlay_volume_velocity_needle_selectable,
                    overlay_volume_velocity_needle,
                    overlay_select)

/** \} */

/* -------------------------------------------------------------------- */
/** \name Volume Grid-Lines
 * \{ */

GPU_SHADER_INTERFACE_INFO(overlay_volume_gridlines_iface)
FLAT(float4, final_color)
GPU_SHADER_INTERFACE_END()

GPU_SHADER_CREATE_INFO(overlay_volume_gridlines)
PUSH_CONSTANT(float, slice_position)
PUSH_CONSTANT(int, slice_axis)
/* FluidDomainSettings.res */
PUSH_CONSTANT(int3, volume_size)
/* FluidDomainSettings.cell_size */
PUSH_CONSTANT(float3, cell_size)
/* FluidDomainSettings.p0 */
PUSH_CONSTANT(float3, domain_origin_offset)
/* FluidDomainSettings.res_min */
PUSH_CONSTANT(int3, adaptive_cell_offset)
PUSH_CONSTANT(int, in_select_id)
VERTEX_OUT(overlay_volume_gridlines_iface)
FRAGMENT_OUT(0, float4, frag_color)
VERTEX_SOURCE("overlay_volume_gridlines_vert.glsl")
FRAGMENT_SOURCE("overlay_varying_color.glsl")
GPU_SHADER_CREATE_END()

GPU_SHADER_CREATE_INFO(overlay_volume_gridlines_flat)
DO_STATIC_COMPILATION()
ADDITIONAL_INFO(draw_volume)
ADDITIONAL_INFO(draw_view)
ADDITIONAL_INFO(overlay_volume_gridlines)
GPU_SHADER_CREATE_END()

CREATE_INFO_VARIANT(overlay_volume_gridlines_flat_selectable,
                    overlay_volume_gridlines_flat,
                    overlay_select)

GPU_SHADER_CREATE_INFO(overlay_volume_gridlines_flags)
DO_STATIC_COMPILATION()
DEFINE("SHOW_FLAGS")
SAMPLER(0, usampler3D, flag_tx)
ADDITIONAL_INFO(draw_volume)
ADDITIONAL_INFO(draw_view)
ADDITIONAL_INFO(overlay_volume_gridlines)
GPU_SHADER_CREATE_END()

CREATE_INFO_VARIANT(overlay_volume_gridlines_flags_selectable,
                    overlay_volume_gridlines_flags,
                    overlay_select)

GPU_SHADER_CREATE_INFO(overlay_volume_gridlines_range)
DO_STATIC_COMPILATION()
DEFINE("SHOW_RANGE")
PUSH_CONSTANT(float, lower_bound)
PUSH_CONSTANT(float, upper_bound)
PUSH_CONSTANT(float4, range_color)
PUSH_CONSTANT(int, cell_filter)
SAMPLER(0, usampler3D, flag_tx)
SAMPLER(1, sampler3D, field_tx)
ADDITIONAL_INFO(draw_volume)
ADDITIONAL_INFO(draw_view)
ADDITIONAL_INFO(overlay_volume_gridlines)
GPU_SHADER_CREATE_END()

CREATE_INFO_VARIANT(overlay_volume_gridlines_range_selectable,
                    overlay_volume_gridlines_range,
                    overlay_select)

/** \} */
