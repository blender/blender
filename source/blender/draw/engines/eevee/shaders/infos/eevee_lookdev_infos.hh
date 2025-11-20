/* SPDX-FileCopyrightText: 2023 Blender Authors
 *
 * SPDX-License-Identifier: GPL-2.0-or-later */

#ifdef GPU_SHADER
#  pragma once
#  include "gpu_shader_compat.hh"

#  include "draw_view_infos.hh"
#  include "eevee_common_infos.hh"

#  include "eevee_light_shared.hh"
#  include "eevee_lightprobe_shared.hh"
#endif

#ifdef GLSL_CPP_STUBS
#  define SPHERE_PROBE
#endif

#include "eevee_defines.hh"
#include "gpu_shader_create_info.hh"

GPU_SHADER_INTERFACE_INFO(eevee_lookdev_display_iface)
SMOOTH(float2, uv_coord)
FLAT(uint, sphere_id)
GPU_SHADER_INTERFACE_END()

GPU_SHADER_CREATE_INFO(eevee_lookdev_display)
VERTEX_SOURCE("eevee_lookdev_display_vert.glsl")
VERTEX_OUT(eevee_lookdev_display_iface)
PUSH_CONSTANT(float2, viewportSize)
PUSH_CONSTANT(float2, invertedViewportSize)
PUSH_CONSTANT(int2, anchor)
SAMPLER(0, sampler2D, metallic_tx)
SAMPLER(1, sampler2D, diffuse_tx)
FRAGMENT_OUT(0, float4, out_color)
FRAGMENT_SOURCE("eevee_lookdev_display_frag.glsl")
DEPTH_WRITE(DepthWrite::ANY)
DO_STATIC_COMPILATION()
GPU_SHADER_CREATE_END()

GPU_SHADER_CREATE_INFO(eevee_lookdev_copy_world)
LOCAL_GROUP_SIZE(SPHERE_PROBE_REMAP_GROUP_SIZE, SPHERE_PROBE_REMAP_GROUP_SIZE)
TYPEDEF_SOURCE("eevee_lightprobe_shared.hh")
TYPEDEF_SOURCE("eevee_light_shared.hh")
PUSH_CONSTANT(int4, read_coord_packed)
PUSH_CONSTANT(int4, write_coord_mip0_packed)
PUSH_CONSTANT(int4, write_coord_mip1_packed)
PUSH_CONSTANT(int4, write_coord_mip2_packed)
PUSH_CONSTANT(int4, write_coord_mip3_packed)
PUSH_CONSTANT(int4, write_coord_mip4_packed)
PUSH_CONSTANT(float4x4, lookdev_rotation)
SAMPLER(0, sampler2DArray, in_sphere_tx)
IMAGE(0, SPHERE_PROBE_FORMAT, write, image2DArray, out_sphere_mip0)
IMAGE(1, SPHERE_PROBE_FORMAT, write, image2DArray, out_sphere_mip1)
IMAGE(2, SPHERE_PROBE_FORMAT, write, image2DArray, out_sphere_mip2)
IMAGE(3, SPHERE_PROBE_FORMAT, write, image2DArray, out_sphere_mip3)
IMAGE(4, SPHERE_PROBE_FORMAT, write, image2DArray, out_sphere_mip4)
STORAGE_BUF(0, read, SphereProbeHarmonic, in_sh)
STORAGE_BUF(1, write, SphereProbeHarmonic, out_sh)
/* WORKAROUND: The no_restrict flag is only here to workaround an NVidia linker bug. */
#ifdef GLSL_CPP_STUBS
STORAGE_BUF(2, read, LightData, in_sun)
#else
/* WORKAROUND: The no_restrict flag is only here to workaround an NVidia linker bug. */
STORAGE_BUF(2, no_restrict | Qualifier::read, LightData, in_sun)
#endif
STORAGE_BUF(3, write, LightData, out_sun)
COMPUTE_SOURCE("eevee_lookdev_copy_world_comp.glsl")
DO_STATIC_COMPILATION()
GPU_SHADER_CREATE_END()
