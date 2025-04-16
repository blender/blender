/* SPDX-FileCopyrightText: 2023 Blender Authors
 *
 * SPDX-License-Identifier: GPL-2.0-or-later */

#ifdef GPU_SHADER
#  pragma once
#  include "gpu_glsl_cpp_stubs.hh"

#  include "draw_common_shader_shared.hh"
#  include "draw_object_infos_info.hh"
#  include "draw_view_info.hh"
#  include "overlay_common_info.hh"

#  define USE_MAC
#  define SHOW_RANGE
#endif

#include "gpu_shader_create_info.hh"

/* -------------------------------------------------------------------- */
/** \name Volume Velocity
 * \{ */

GPU_SHADER_INTERFACE_INFO(overlay_volume_velocity_iface)
SMOOTH(float4, finalColor)
GPU_SHADER_INTERFACE_END()

GPU_SHADER_CREATE_INFO(overlay_volume_velocity)
SAMPLER(0, FLOAT_3D, velocityX)
SAMPLER(1, FLOAT_3D, velocityY)
SAMPLER(2, FLOAT_3D, velocityZ)
PUSH_CONSTANT(float, displaySize)
PUSH_CONSTANT(float, slicePosition)
PUSH_CONSTANT(int, sliceAxis)
PUSH_CONSTANT(bool, scaleWithMagnitude)
PUSH_CONSTANT(bool, isCellCentered)
/* FluidDomainSettings.cell_size */
PUSH_CONSTANT(float3, cellSize)
/* FluidDomainSettings.p0 */
PUSH_CONSTANT(float3, domainOriginOffset)
/* FluidDomainSettings.res_min */
PUSH_CONSTANT(int3, adaptiveCellOffset)
PUSH_CONSTANT(int, in_select_id)
VERTEX_OUT(overlay_volume_velocity_iface)
FRAGMENT_OUT(0, float4, fragColor)
VERTEX_SOURCE("overlay_volume_velocity_vert.glsl")
FRAGMENT_SOURCE("overlay_varying_color.glsl")
GPU_SHADER_CREATE_END()

GPU_SHADER_CREATE_INFO(overlay_volume_velocity_streamline)
DO_STATIC_COMPILATION()
ADDITIONAL_INFO(draw_volume)
ADDITIONAL_INFO(draw_view)
ADDITIONAL_INFO(overlay_volume_velocity)
GPU_SHADER_CREATE_END()

OVERLAY_INFO_SELECT_VARIATION(overlay_volume_velocity_streamline)

GPU_SHADER_CREATE_INFO(overlay_volume_velocity_mac)
DO_STATIC_COMPILATION()
DEFINE("USE_MAC")
PUSH_CONSTANT(bool, drawMACX)
PUSH_CONSTANT(bool, drawMACY)
PUSH_CONSTANT(bool, drawMACZ)
ADDITIONAL_INFO(draw_volume)
ADDITIONAL_INFO(draw_view)
ADDITIONAL_INFO(overlay_volume_velocity)
GPU_SHADER_CREATE_END()

OVERLAY_INFO_SELECT_VARIATION(overlay_volume_velocity_mac)

GPU_SHADER_CREATE_INFO(overlay_volume_velocity_needle)
DO_STATIC_COMPILATION()
DEFINE("USE_NEEDLE")
ADDITIONAL_INFO(draw_volume)
ADDITIONAL_INFO(draw_view)
ADDITIONAL_INFO(overlay_volume_velocity)
GPU_SHADER_CREATE_END()

OVERLAY_INFO_SELECT_VARIATION(overlay_volume_velocity_needle)

/** \} */

/* -------------------------------------------------------------------- */
/** \name Volume Grid-Lines
 * \{ */

GPU_SHADER_INTERFACE_INFO(overlay_volume_gridlines_iface)
FLAT(float4, finalColor)
GPU_SHADER_INTERFACE_END()

GPU_SHADER_CREATE_INFO(overlay_volume_gridlines)
PUSH_CONSTANT(float, slicePosition)
PUSH_CONSTANT(int, sliceAxis)
/* FluidDomainSettings.res */
PUSH_CONSTANT(int3, volumeSize)
/* FluidDomainSettings.cell_size */
PUSH_CONSTANT(float3, cellSize)
/* FluidDomainSettings.p0 */
PUSH_CONSTANT(float3, domainOriginOffset)
/* FluidDomainSettings.res_min */
PUSH_CONSTANT(int3, adaptiveCellOffset)
PUSH_CONSTANT(int, in_select_id)
VERTEX_OUT(overlay_volume_gridlines_iface)
FRAGMENT_OUT(0, float4, fragColor)
VERTEX_SOURCE("overlay_volume_gridlines_vert.glsl")
FRAGMENT_SOURCE("overlay_varying_color.glsl")
GPU_SHADER_CREATE_END()

GPU_SHADER_CREATE_INFO(overlay_volume_gridlines_flat)
DO_STATIC_COMPILATION()
ADDITIONAL_INFO(draw_volume)
ADDITIONAL_INFO(draw_view)
ADDITIONAL_INFO(overlay_volume_gridlines)
GPU_SHADER_CREATE_END()

OVERLAY_INFO_SELECT_VARIATION(overlay_volume_gridlines_flat)

GPU_SHADER_CREATE_INFO(overlay_volume_gridlines_flags)
DO_STATIC_COMPILATION()
DEFINE("SHOW_FLAGS")
SAMPLER(0, UINT_3D, flagTexture)
ADDITIONAL_INFO(draw_volume)
ADDITIONAL_INFO(draw_view)
ADDITIONAL_INFO(overlay_volume_gridlines)
GPU_SHADER_CREATE_END()

OVERLAY_INFO_SELECT_VARIATION(overlay_volume_gridlines_flags)

GPU_SHADER_CREATE_INFO(overlay_volume_gridlines_range)
DO_STATIC_COMPILATION()
DEFINE("SHOW_RANGE")
PUSH_CONSTANT(float, lowerBound)
PUSH_CONSTANT(float, upperBound)
PUSH_CONSTANT(float4, rangeColor)
PUSH_CONSTANT(int, cellFilter)
SAMPLER(0, UINT_3D, flagTexture)
SAMPLER(1, FLOAT_3D, fieldTexture)
ADDITIONAL_INFO(draw_volume)
ADDITIONAL_INFO(draw_view)
ADDITIONAL_INFO(overlay_volume_gridlines)
GPU_SHADER_CREATE_END()

OVERLAY_INFO_SELECT_VARIATION(overlay_volume_gridlines_range)

/** \} */
