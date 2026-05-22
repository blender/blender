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
#endif

#ifdef GLSL_CPP_STUBS
#  define SPHERE_PROBE
#endif

#include "eevee_defines.hh"
#include "gpu_shader_create_info.hh"

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
