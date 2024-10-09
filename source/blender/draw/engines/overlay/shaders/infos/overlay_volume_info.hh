/* SPDX-FileCopyrightText: 2023 Blender Authors
 *
 * SPDX-License-Identifier: GPL-2.0-or-later */

#include "gpu_shader_create_info.hh"

/* -------------------------------------------------------------------- */
/** \name Volume Velocity
 * \{ */

GPU_SHADER_INTERFACE_INFO(overlay_volume_velocity_iface)
SMOOTH(VEC4, finalColor)
GPU_SHADER_INTERFACE_END()

GPU_SHADER_CREATE_INFO(overlay_volume_velocity)
SAMPLER(0, FLOAT_3D, velocityX)
SAMPLER(1, FLOAT_3D, velocityY)
SAMPLER(2, FLOAT_3D, velocityZ)
PUSH_CONSTANT(FLOAT, displaySize)
PUSH_CONSTANT(FLOAT, slicePosition)
PUSH_CONSTANT(INT, sliceAxis)
PUSH_CONSTANT(BOOL, scaleWithMagnitude)
PUSH_CONSTANT(BOOL, isCellCentered)
/* FluidDomainSettings.cell_size */
PUSH_CONSTANT(VEC3, cellSize)
/* FluidDomainSettings.p0 */
PUSH_CONSTANT(VEC3, domainOriginOffset)
/* FluidDomainSettings.res_min */
PUSH_CONSTANT(IVEC3, adaptiveCellOffset)
PUSH_CONSTANT(INT, in_select_id)
VERTEX_OUT(overlay_volume_velocity_iface)
FRAGMENT_OUT(0, VEC4, fragColor)
VERTEX_SOURCE("overlay_volume_velocity_vert.glsl")
FRAGMENT_SOURCE("overlay_varying_color.glsl")
GPU_SHADER_CREATE_END()

GPU_SHADER_CREATE_INFO(overlay_volume_velocity_streamline)
DO_STATIC_COMPILATION()
ADDITIONAL_INFO(draw_volume)
ADDITIONAL_INFO(overlay_volume_velocity)
GPU_SHADER_CREATE_END()

GPU_SHADER_CREATE_INFO(overlay_volume_velocity_mac)
DO_STATIC_COMPILATION()
DEFINE("USE_MAC")
PUSH_CONSTANT(BOOL, drawMACX)
PUSH_CONSTANT(BOOL, drawMACY)
PUSH_CONSTANT(BOOL, drawMACZ)
ADDITIONAL_INFO(draw_volume)
ADDITIONAL_INFO(overlay_volume_velocity)
GPU_SHADER_CREATE_END()

GPU_SHADER_CREATE_INFO(overlay_volume_velocity_needle)
DO_STATIC_COMPILATION()
DEFINE("USE_NEEDLE")
ADDITIONAL_INFO(draw_volume)
ADDITIONAL_INFO(overlay_volume_velocity)
GPU_SHADER_CREATE_END()

/** \} */

/* -------------------------------------------------------------------- */
/** \name Volume Grid-Lines
 * \{ */

GPU_SHADER_INTERFACE_INFO(overlay_volume_gridlines_iface)
FLAT(VEC4, finalColor)
GPU_SHADER_INTERFACE_END()

GPU_SHADER_CREATE_INFO(overlay_volume_gridlines)
PUSH_CONSTANT(FLOAT, slicePosition)
PUSH_CONSTANT(INT, sliceAxis)
/* FluidDomainSettings.res */
PUSH_CONSTANT(IVEC3, volumeSize)
/* FluidDomainSettings.cell_size */
PUSH_CONSTANT(VEC3, cellSize)
/* FluidDomainSettings.p0 */
PUSH_CONSTANT(VEC3, domainOriginOffset)
/* FluidDomainSettings.res_min */
PUSH_CONSTANT(IVEC3, adaptiveCellOffset)
PUSH_CONSTANT(INT, in_select_id)
VERTEX_OUT(overlay_volume_gridlines_iface)
FRAGMENT_OUT(0, VEC4, fragColor)
VERTEX_SOURCE("overlay_volume_gridlines_vert.glsl")
FRAGMENT_SOURCE("overlay_varying_color.glsl")
GPU_SHADER_CREATE_END()

GPU_SHADER_CREATE_INFO(overlay_volume_gridlines_flat)
DO_STATIC_COMPILATION()
ADDITIONAL_INFO(draw_volume)
ADDITIONAL_INFO(overlay_volume_gridlines)
GPU_SHADER_CREATE_END()

GPU_SHADER_CREATE_INFO(overlay_volume_gridlines_flags)
DO_STATIC_COMPILATION()
DEFINE("SHOW_FLAGS")
SAMPLER(0, UINT_3D, flagTexture)
ADDITIONAL_INFO(draw_volume)
ADDITIONAL_INFO(overlay_volume_gridlines)
GPU_SHADER_CREATE_END()

GPU_SHADER_CREATE_INFO(overlay_volume_gridlines_range)
DO_STATIC_COMPILATION()
DEFINE("SHOW_RANGE")
PUSH_CONSTANT(FLOAT, lowerBound)
PUSH_CONSTANT(FLOAT, upperBound)
PUSH_CONSTANT(VEC4, rangeColor)
PUSH_CONSTANT(INT, cellFilter)
SAMPLER(0, UINT_3D, flagTexture)
SAMPLER(1, FLOAT_3D, fieldTexture)
ADDITIONAL_INFO(draw_volume)
ADDITIONAL_INFO(overlay_volume_gridlines)
GPU_SHADER_CREATE_END()

/** \} */
