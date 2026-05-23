/* SPDX-FileCopyrightText: 2023 Blender Authors
 *
 * SPDX-License-Identifier: GPL-2.0-or-later */

#ifdef GPU_SHADER
#  pragma once
#  include "BLI_utildefines_variadic.h"

#  include "gpu_shader_compat.hh"

#  include "draw_object_infos_infos.hh"
#  include "draw_view_infos.hh"

#  include "eevee_common_infos.hh"
#  include "eevee_lightprobe_shared.hh"
#  include "eevee_sampling_infos.hh"
#  include "eevee_shadow_shared.hh"
#  include "eevee_uniform_infos.hh"
#endif

#ifdef GLSL_CPP_STUBS
#  define CURVES_SHADER
#  define DRW_HAIR_INFO

#  define POINTCLOUD_SHADER
#  define DRW_POINTCLOUD_INFO

#  define MAT_TRANSPARENT
#  define MAT_SHADER_TO_RGBA
#endif

#include "eevee_defines.hh"
#include "gpu_shader_create_info.hh"

/* Common interface */
GPU_SHADER_NAMED_INTERFACE_INFO(eevee_surf_iface, interp)
/* World Position. */
SMOOTH(float3, P)
/* World Normal. */
SMOOTH(float3, N)
GPU_SHADER_NAMED_INTERFACE_END(interp)

GPU_SHADER_NAMED_INTERFACE_INFO(eevee_surf_shadow_iface, shadow_iface)
FLAT(int, shadow_view_id)
GPU_SHADER_NAMED_INTERFACE_END(shadow_iface)

GPU_SHADER_NAMED_INTERFACE_INFO(eevee_surf_shadow_clipping_iface, shadow_clip)
SMOOTH(float3, position)
SMOOTH(float3, vector)
GPU_SHADER_NAMED_INTERFACE_END(shadow_clip)

GPU_SHADER_NAMED_INTERFACE_INFO(eevee_surf_curve_iface, curve_interp)
SMOOTH(float3, tangent)
SMOOTH(float3, binormal)
SMOOTH(float, time)
SMOOTH(float, time_width)
SMOOTH(float, radius)
SMOOTH(float, point_id) /* Smooth to be used for barycentric. */
GPU_SHADER_NAMED_INTERFACE_END(curve_interp)

GPU_SHADER_NAMED_INTERFACE_INFO(eevee_surf_curve_flat_iface, curve_interp_flat)
FLAT(int, strand_id)
GPU_SHADER_NAMED_INTERFACE_END(curve_interp_flat)

GPU_SHADER_NAMED_INTERFACE_INFO(eevee_surf_pointcloud_iface, pointcloud_interp)
SMOOTH(float, radius)
SMOOTH(float3, position)
GPU_SHADER_NAMED_INTERFACE_END(pointcloud_interp)

GPU_SHADER_NAMED_INTERFACE_INFO(eevee_surf_pointcloud_flat_iface, pointcloud_interp_flat)
FLAT(int, id)
GPU_SHADER_NAMED_INTERFACE_END(pointcloud_interp_flat)

/* WORKAROUND: Until we get condition support for interfaces. */
GPU_SHADER_CREATE_INFO(eevee_shadow_iface_info)
VERTEX_OUT(eevee_surf_shadow_iface)
VERTEX_OUT(eevee_surf_shadow_clipping_iface)
GPU_SHADER_CREATE_END()

/* WORKAROUND: Until we get condition support for interfaces. */
GPU_SHADER_CREATE_INFO(eevee_geom_curves_iface_info)
VERTEX_OUT(eevee_surf_curve_iface)
VERTEX_OUT(eevee_surf_curve_flat_iface)
GPU_SHADER_CREATE_END()

/* WORKAROUND: Until we get condition support for interfaces. */
GPU_SHADER_CREATE_INFO(eevee_geom_pointcloud_iface_info)
VERTEX_OUT(eevee_surf_pointcloud_iface)
VERTEX_OUT(eevee_surf_pointcloud_flat_iface)
GPU_SHADER_CREATE_END()

/* WORKAROUND: Until we remove global accesses to the interface. */
GPU_SHADER_CREATE_INFO(eevee_geom_iface_info)
VERTEX_OUT(eevee_surf_iface)
GPU_SHADER_CREATE_END()
