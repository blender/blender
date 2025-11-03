/* SPDX-FileCopyrightText: 2023 Blender Authors
 *
 * SPDX-License-Identifier: GPL-2.0-or-later */

#ifdef GPU_SHADER
#  pragma once
#  include "gpu_shader_compat.hh"

#  include "draw_object_infos_infos.hh"
#  include "draw_view_infos.hh"
#  include "eevee_common_infos.hh"
#  include "eevee_lightprobe_shared.hh"
#  include "eevee_lightprobe_volume_infos.hh"
#  include "eevee_sampling_infos.hh"
#  include "eevee_uniform_infos.hh"

#  define SPHERE_PROBE
#endif

#include "eevee_defines.hh"
#include "gpu_shader_create_info.hh"

/* Sample cubemap and remap into an octahedral texture. */
GPU_SHADER_CREATE_INFO(eevee_lightprobe_sphere_remap)
LOCAL_GROUP_SIZE(SPHERE_PROBE_REMAP_GROUP_SIZE, SPHERE_PROBE_REMAP_GROUP_SIZE)
SPECIALIZATION_CONSTANT(bool, extract_sh, true)
SPECIALIZATION_CONSTANT(bool, extract_sun, true)
PUSH_CONSTANT(int4, probe_coord_packed)
PUSH_CONSTANT(int4, write_coord_packed)
PUSH_CONSTANT(int4, world_coord_packed)
SAMPLER(0, samplerCube, cubemap_tx)
SAMPLER(1, sampler2DArray, atlas_tx)
STORAGE_BUF(0, write, SphereProbeHarmonic, out_sh[])
STORAGE_BUF(1, write, SphereProbeSunLight, out_sun[])
IMAGE(0, SFLOAT_16_16_16_16, write, image2DArray, atlas_img)
COMPUTE_SOURCE("eevee_lightprobe_sphere_remap_comp.glsl")
TYPEDEF_SOURCE("eevee_defines.hh")
TYPEDEF_SOURCE("eevee_lightprobe_shared.hh")
ADDITIONAL_INFO(eevee_global_ubo)
DO_STATIC_COMPILATION()
GPU_SHADER_CREATE_END()

GPU_SHADER_CREATE_INFO(eevee_lightprobe_sphere_irradiance)
LOCAL_GROUP_SIZE(SPHERE_PROBE_SH_GROUP_SIZE)
PUSH_CONSTANT(int3, probe_remap_dispatch_size)
STORAGE_BUF(0, read, SphereProbeHarmonic, in_sh[])
STORAGE_BUF(1, write, SphereProbeHarmonic, out_sh)
TYPEDEF_SOURCE("eevee_defines.hh")
TYPEDEF_SOURCE("eevee_lightprobe_shared.hh")
DO_STATIC_COMPILATION()
COMPUTE_SOURCE("eevee_lightprobe_sphere_irradiance_comp.glsl")
GPU_SHADER_CREATE_END()

GPU_SHADER_CREATE_INFO(eevee_lightprobe_sphere_sunlight)
LOCAL_GROUP_SIZE(SPHERE_PROBE_SH_GROUP_SIZE)
PUSH_CONSTANT(int3, probe_remap_dispatch_size)
TYPEDEF_SOURCE("eevee_defines.hh")
TYPEDEF_SOURCE("eevee_lightprobe_shared.hh")
TYPEDEF_SOURCE("eevee_light_shared.hh")
TYPEDEF_SOURCE("eevee_uniform_shared.hh")
STORAGE_BUF(0, read, SphereProbeSunLight, in_sun[])
STORAGE_BUF(1, write, LightData, sunlight_buf)
DO_STATIC_COMPILATION()
COMPUTE_SOURCE("eevee_lightprobe_sphere_sunlight_comp.glsl")
GPU_SHADER_CREATE_END()

GPU_SHADER_CREATE_INFO(eevee_lightprobe_sphere_select)
LOCAL_GROUP_SIZE(SPHERE_PROBE_SELECT_GROUP_SIZE)
STORAGE_BUF(0, read_write, SphereProbeData, lightprobe_sphere_buf[SPHERE_PROBE_MAX])
PUSH_CONSTANT(int, lightprobe_sphere_count)
TYPEDEF_SOURCE("eevee_defines.hh")
TYPEDEF_SOURCE("eevee_lightprobe_shared.hh")
ADDITIONAL_INFO(eevee_sampling_data)
ADDITIONAL_INFO(eevee_global_ubo)
ADDITIONAL_INFO(eevee_volume_probe_data)
COMPUTE_SOURCE("eevee_lightprobe_sphere_select_comp.glsl")
DO_STATIC_COMPILATION()
GPU_SHADER_CREATE_END()

GPU_SHADER_CREATE_INFO(eevee_lightprobe_sphere_convolve)
LOCAL_GROUP_SIZE(SPHERE_PROBE_GROUP_SIZE, SPHERE_PROBE_GROUP_SIZE)
TYPEDEF_SOURCE("eevee_defines.hh")
TYPEDEF_SOURCE("eevee_lightprobe_shared.hh")
PUSH_CONSTANT(int4, probe_coord_packed)
PUSH_CONSTANT(int4, write_coord_packed)
PUSH_CONSTANT(int4, read_coord_packed)
PUSH_CONSTANT(int, read_lod)
SAMPLER(0, samplerCube, cubemap_tx)
SAMPLER(1, sampler2DArray, in_atlas_mip_tx)
IMAGE(1, SFLOAT_16_16_16_16, write, image2DArray, out_atlas_mip_img)
COMPUTE_SOURCE("eevee_lightprobe_sphere_convolve_comp.glsl")
DO_STATIC_COMPILATION()
GPU_SHADER_CREATE_END()

GPU_SHADER_INTERFACE_INFO(eevee_display_lightprobe_sphere_iface)
SMOOTH(float3, P)
SMOOTH(float2, lP)
FLAT(int, probe_index)
GPU_SHADER_INTERFACE_END()

GPU_SHADER_CREATE_INFO(eevee_display_lightprobe_sphere)
TYPEDEF_SOURCE("eevee_defines.hh")
TYPEDEF_SOURCE("eevee_lightprobe_shared.hh")
ADDITIONAL_INFO(draw_view)
ADDITIONAL_INFO(eevee_lightprobe_sphere_data)
STORAGE_BUF(0, read, SphereProbeDisplayData, display_data_buf[])
VERTEX_SOURCE("eevee_display_lightprobe_sphere_vert.glsl")
VERTEX_OUT(eevee_display_lightprobe_sphere_iface)
FRAGMENT_SOURCE("eevee_display_lightprobe_sphere_frag.glsl")
FRAGMENT_OUT(0, float4, out_color)
BUILTINS(BuiltinBits::CLIP_CONTROL)
DO_STATIC_COMPILATION()
GPU_SHADER_CREATE_END()

GPU_SHADER_INTERFACE_INFO(eevee_display_lightprobe_planar_iface)
FLAT(float3, probe_normal)
FLAT(int, probe_index)
GPU_SHADER_INTERFACE_END()

GPU_SHADER_CREATE_INFO(eevee_display_lightprobe_planar)
PUSH_CONSTANT(int4, world_coord_packed)
TYPEDEF_SOURCE("eevee_defines.hh")
TYPEDEF_SOURCE("eevee_lightprobe_shared.hh")
ADDITIONAL_INFO(draw_view)
ADDITIONAL_INFO(eevee_lightprobe_planar_data)
ADDITIONAL_INFO(eevee_lightprobe_sphere_data)
STORAGE_BUF(0, read, PlanarProbeDisplayData, display_data_buf[])
VERTEX_SOURCE("eevee_display_lightprobe_planar_vert.glsl")
VERTEX_OUT(eevee_display_lightprobe_planar_iface)
FRAGMENT_SOURCE("eevee_display_lightprobe_planar_frag.glsl")
FRAGMENT_OUT(0, float4, out_color)
BUILTINS(BuiltinBits::CLIP_CONTROL)
DO_STATIC_COMPILATION()
GPU_SHADER_CREATE_END()

/** \} */
