/* SPDX-FileCopyrightText: 2023 Blender Authors
 *
 * SPDX-License-Identifier: GPL-2.0-or-later */

/**
 * Common public resources to use the light-probes.
 */

#ifdef GPU_SHADER
#  pragma once
#  include "gpu_shader_compat.hh"

#  include "eevee_lightprobe_shared.hh"
#endif

#include "eevee_defines.hh"
#include "gpu_shader_create_info.hh"

GPU_SHADER_CREATE_INFO(eevee_lightprobe_sphere_data)
DEFINE("SPHERE_PROBE")
TYPEDEF_SOURCE("eevee_lightprobe_shared.hh")
UNIFORM_BUF(SPHERE_PROBE_BUF_SLOT, SphereProbeData, lightprobe_sphere_buf[SPHERE_PROBE_MAX])
SAMPLER(SPHERE_PROBE_TEX_SLOT, sampler2DArray, lightprobe_spheres_tx)
GPU_SHADER_CREATE_END()

GPU_SHADER_CREATE_INFO(eevee_volume_probe_data)
TYPEDEF_SOURCE("eevee_lightprobe_shared.hh")
UNIFORM_BUF(IRRADIANCE_GRID_BUF_SLOT, VolumeProbeData, grids_infos_buf[IRRADIANCE_GRID_MAX])
/* NOTE: Use uint instead of IrradianceBrickPacked because Metal needs to know the exact type.
 */
STORAGE_BUF(IRRADIANCE_BRICK_BUF_SLOT, read, uint, bricks_infos_buf[])
SAMPLER(VOLUME_PROBE_TEX_SLOT, sampler3D, irradiance_atlas_tx)
DEFINE("IRRADIANCE_GRID_SAMPLING")
GPU_SHADER_CREATE_END()

GPU_SHADER_CREATE_INFO(eevee_lightprobe_planar_data)
DEFINE("SPHERE_PROBE")
TYPEDEF_SOURCE("eevee_lightprobe_shared.hh")
UNIFORM_BUF(PLANAR_PROBE_BUF_SLOT, PlanarProbeData, probe_planar_buf[PLANAR_PROBE_MAX])
SAMPLER(PLANAR_PROBE_RADIANCE_TEX_SLOT, sampler2DArray, planar_radiance_tx)
SAMPLER(PLANAR_PROBE_DEPTH_TEX_SLOT, sampler2DArrayDepth, planar_depth_tx)
GPU_SHADER_CREATE_END()

GPU_SHADER_CREATE_INFO(eevee_surfel_common)
TYPEDEF_SOURCE("eevee_lightprobe_shared.hh")
STORAGE_BUF(SURFEL_BUF_SLOT, read_write, Surfel, surfel_buf[])
STORAGE_BUF(CAPTURE_BUF_SLOT, read, CaptureInfoData, capture_info_buf)
GPU_SHADER_CREATE_END()

GPU_SHADER_CREATE_INFO(eevee_lightprobe_data)
ADDITIONAL_INFO(eevee_lightprobe_sphere_data)
ADDITIONAL_INFO(eevee_volume_probe_data)
GPU_SHADER_CREATE_END()
