/* SPDX-FileCopyrightText: 2023 Blender Foundation
 *
 * SPDX-License-Identifier: GPL-2.0-or-later */

#include "gpu_shader_create_info.hh"

/* -------------------------------------------------------------------- */
/** \name Volume Velocity
 * \{ */

GPU_SHADER_INTERFACE_INFO(overlay_volume_velocity_iface, "").smooth(Type::VEC4, "finalColor");

GPU_SHADER_CREATE_INFO(overlay_volume_velocity)
    .do_static_compilation(true)
    .sampler(0, ImageType::FLOAT_3D, "velocityX")
    .sampler(1, ImageType::FLOAT_3D, "velocityY")
    .sampler(2, ImageType::FLOAT_3D, "velocityZ")
    .push_constant(Type::FLOAT, "displaySize")
    .push_constant(Type::FLOAT, "slicePosition")
    .push_constant(Type::INT, "sliceAxis")
    .push_constant(Type::BOOL, "scaleWithMagnitude")
    .push_constant(Type::BOOL, "isCellCentered")
    /* FluidDomainSettings.cell_size */
    .push_constant(Type::VEC3, "cellSize")
    /* FluidDomainSettings.p0 */
    .push_constant(Type::VEC3, "domainOriginOffset")
    /* FluidDomainSettings.res_min */
    .push_constant(Type::IVEC3, "adaptiveCellOffset")
    .vertex_out(overlay_volume_velocity_iface)
    .fragment_out(0, Type::VEC4, "fragColor")
    .vertex_source("overlay_volume_velocity_vert.glsl")
    .fragment_source("overlay_varying_color.glsl")
    .additional_info("draw_volume");

GPU_SHADER_CREATE_INFO(overlay_volume_velocity_mac)
    .do_static_compilation(true)
    .define("USE_MAC")
    .push_constant(Type::BOOL, "drawMACX")
    .push_constant(Type::BOOL, "drawMACY")
    .push_constant(Type::BOOL, "drawMACZ")
    .additional_info("overlay_volume_velocity");

GPU_SHADER_CREATE_INFO(overlay_volume_velocity_needle)
    .do_static_compilation(true)
    .define("USE_NEEDLE")
    .additional_info("overlay_volume_velocity");

/** \} */

/* -------------------------------------------------------------------- */
/** \name Volume Grid-Lines
 * \{ */

GPU_SHADER_INTERFACE_INFO(overlay_volume_gridlines_iface, "").flat(Type::VEC4, "finalColor");

GPU_SHADER_CREATE_INFO(overlay_volume_gridlines)
    .do_static_compilation(true)
    .push_constant(Type::FLOAT, "slicePosition")
    .push_constant(Type::INT, "sliceAxis")
    /* FluidDomainSettings.res */
    .push_constant(Type::IVEC3, "volumeSize")
    /* FluidDomainSettings.cell_size */
    .push_constant(Type::VEC3, "cellSize")
    /* FluidDomainSettings.p0 */
    .push_constant(Type::VEC3, "domainOriginOffset")
    /* FluidDomainSettings.res_min */
    .push_constant(Type::IVEC3, "adaptiveCellOffset")
    .vertex_out(overlay_volume_gridlines_iface)
    .fragment_out(0, Type::VEC4, "fragColor")
    .vertex_source("overlay_volume_gridlines_vert.glsl")
    .fragment_source("overlay_varying_color.glsl")
    .additional_info("draw_volume");

GPU_SHADER_CREATE_INFO(overlay_volume_gridlines_flags)
    .do_static_compilation(true)
    .define("SHOW_FLAGS")
    .sampler(0, ImageType::UINT_3D, "flagTexture")
    .additional_info("overlay_volume_gridlines");

GPU_SHADER_CREATE_INFO(overlay_volume_gridlines_range)
    .do_static_compilation(true)
    .define("SHOW_RANGE")
    .push_constant(Type::FLOAT, "lowerBound")
    .push_constant(Type::FLOAT, "upperBound")
    .push_constant(Type::VEC4, "rangeColor")
    .push_constant(Type::INT, "cellFilter")
    .sampler(0, ImageType::UINT_3D, "flagTexture")
    .sampler(1, ImageType::FLOAT_3D, "fieldTexture")
    .additional_info("overlay_volume_gridlines");

/** \} */
