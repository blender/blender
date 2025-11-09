/* SPDX-FileCopyrightText: 2023 Blender Authors
 *
 * SPDX-License-Identifier: GPL-2.0-or-later */

#include "draw_defines.hh"

#ifdef GPU_SHADER
#  pragma once
#  include "gpu_shader_compat.hh"

#  include "draw_command_shared.hh"
#  include "draw_shader_shared.hh"

/* Define stub defines for C++ test compilation. */
#  define DRAW_VIEW_CREATE_INFO
#  define DRW_VIEW_CULLING_INFO
#  define USE_WORLD_CLIP_PLANES

#  define DRW_VIEW_LEN DRW_VIEW_MAX
#endif

#include "gpu_shader_create_info.hh"

/* -------------------------------------------------------------------- */
/** \name Resource ID
 * This is used to fetch per object data in drw_matrices and other object indexed buffers.
 * \{ */

/**
 * Used if the resource index needs to be passed to the fragment shader.
 * IMPORTANT: Vertex shader need to write `drw_ResourceID_iface.resource_index` in main().
 */
GPU_SHADER_NAMED_INTERFACE_INFO(draw_resource_id_iface, drw_ResourceID_iface)
FLAT(uint, resource_index)
GPU_SHADER_NAMED_INTERFACE_END(drw_ResourceID_iface)

GPU_SHADER_CREATE_INFO(draw_resource_id_varying)
VERTEX_OUT(draw_resource_id_iface)
GEOMETRY_OUT(draw_resource_id_iface)
GPU_SHADER_CREATE_END()

GPU_SHADER_CREATE_INFO(draw_resource_id)
STORAGE_BUF(DRW_RESOURCE_ID_SLOT, read, uint, resource_id_buf[])
GPU_SHADER_CREATE_END()

GPU_SHADER_CREATE_INFO(draw_resource_with_custom_id)
DEFINE("WITH_CUSTOM_IDS")
STORAGE_BUF(DRW_RESOURCE_ID_SLOT, read, uint2, resource_id_buf[])
GPU_SHADER_CREATE_END()

/** \} */

/* -------------------------------------------------------------------- */
/** \name Draw Object Resources
 * \{ */

GPU_SHADER_CREATE_INFO(draw_modelmat_common)
TYPEDEF_SOURCE("draw_shader_shared.hh")
STORAGE_BUF(DRW_OBJ_MAT_SLOT, read, ObjectMatrices, drw_matrix_buf[])
DEFINE("DRAW_MODELMAT_CREATE_INFO")
GPU_SHADER_CREATE_END()

GPU_SHADER_CREATE_INFO(draw_modelmat)
ADDITIONAL_INFO(draw_modelmat_common)
ADDITIONAL_INFO(draw_resource_id)
GPU_SHADER_CREATE_END()

GPU_SHADER_CREATE_INFO(draw_modelmat_with_custom_id)
ADDITIONAL_INFO(draw_modelmat_common)
ADDITIONAL_INFO(draw_resource_with_custom_id)
GPU_SHADER_CREATE_END()

/** \} */

/* -------------------------------------------------------------------- */
/** \name Draw View
 * \{ */

GPU_SHADER_CREATE_INFO(draw_view)
UNIFORM_BUF_FREQ(DRW_VIEW_UBO_SLOT, ViewMatrices, drw_view_buf[DRW_VIEW_LEN], PASS)
DEFINE("DRAW_VIEW_CREATE_INFO")
TYPEDEF_SOURCE("draw_shader_shared.hh")
GPU_SHADER_CREATE_END()

GPU_SHADER_CREATE_INFO(draw_view_culling)
UNIFORM_BUF(DRW_VIEW_CULLING_UBO_SLOT, ViewCullingData, drw_view_culling_buf[DRW_VIEW_LEN])
DEFINE("DRW_VIEW_CULLING_INFO")
TYPEDEF_SOURCE("draw_shader_shared.hh")
GPU_SHADER_CREATE_END()

/** \} */

/* -------------------------------------------------------------------- */
/** \name Draw View
 * \{ */

GPU_SHADER_CREATE_INFO(drw_clipped)
/* TODO(fclem): Move to engine side. */
UNIFORM_BUF_FREQ(DRW_CLIPPING_UBO_SLOT, float4, drw_clipping_[6], PASS)
BUILTINS(BuiltinBits::CLIP_DISTANCES)
DEFINE("USE_WORLD_CLIP_PLANES")
GPU_SHADER_CREATE_END()

/** \} */

/* -------------------------------------------------------------------- */
/** \name Internal Draw Manager usage
 * \{ */

GPU_SHADER_CREATE_INFO(draw_resource_finalize)
DO_STATIC_COMPILATION()
TYPEDEF_SOURCE("draw_shader_shared.hh")
DEFINE("DRAW_FINALIZE_SHADER")
LOCAL_GROUP_SIZE(DRW_FINALIZE_GROUP_SIZE)
STORAGE_BUF(0, read, ObjectMatrices, matrix_buf[])
STORAGE_BUF(1, read_write, ObjectBounds, bounds_buf[])
STORAGE_BUF(2, read_write, ObjectInfos, infos_buf[])
PUSH_CONSTANT(int, resource_len)
COMPUTE_SOURCE("draw_resource_finalize_comp.glsl")
GPU_SHADER_CREATE_END()

GPU_SHADER_CREATE_INFO(draw_view_finalize)
DO_STATIC_COMPILATION()
LOCAL_GROUP_SIZE(DRW_VIEW_MAX)
DEFINE_VALUE("DRW_VIEW_LEN", STRINGIFY(DRW_VIEW_MAX))
STORAGE_BUF(0, read_write, ViewCullingData, view_culling_buf[DRW_VIEW_LEN])
COMPUTE_SOURCE("draw_view_finalize_comp.glsl")
ADDITIONAL_INFO(draw_view)
GPU_SHADER_CREATE_END()

GPU_SHADER_CREATE_INFO(draw_visibility_compute)
DO_STATIC_COMPILATION()
LOCAL_GROUP_SIZE(DRW_VISIBILITY_GROUP_SIZE)
DEFINE_VALUE("DRW_VIEW_LEN", STRINGIFY(DRW_VIEW_MAX))
STORAGE_BUF(0, read, ObjectBounds, bounds_buf[])
STORAGE_BUF(1, read_write, uint, visibility_buf[])
PUSH_CONSTANT(int, resource_len)
PUSH_CONSTANT(int, view_len)
PUSH_CONSTANT(int, visibility_word_per_draw)
COMPUTE_SOURCE("draw_visibility_comp.glsl")
ADDITIONAL_INFO(draw_view)
ADDITIONAL_INFO(draw_view_culling)
GPU_SHADER_CREATE_END()

GPU_SHADER_CREATE_INFO(draw_command_generate)
DO_STATIC_COMPILATION()
TYPEDEF_SOURCE("draw_shader_shared.hh")
TYPEDEF_SOURCE("draw_command_shared.hh")
LOCAL_GROUP_SIZE(DRW_COMMAND_GROUP_SIZE)
STORAGE_BUF(0, read_write, DrawGroup, group_buf[])
STORAGE_BUF(1, read, uint, visibility_buf[])
STORAGE_BUF(2, read, DrawPrototype, prototype_buf[])
STORAGE_BUF(3, write, DrawCommand, command_buf[])
STORAGE_BUF(DRW_RESOURCE_ID_SLOT, write, uint, resource_id_buf[])
PUSH_CONSTANT(int, prototype_len)
PUSH_CONSTANT(int, visibility_word_per_draw)
PUSH_CONSTANT(int, view_shift)
PUSH_CONSTANT(int, view_len)
PUSH_CONSTANT(bool, use_custom_ids)
COMPUTE_SOURCE("draw_command_generate_comp.glsl")
GPU_SHADER_CREATE_END()

/** \} */

/* Stub needs to be after all definitions to avoid conflict with legacy definitions. */
#ifdef GPU_SHADER
/* Make it work for both draw_resource_id and draw_resource_with_custom_id. */
#  define resource_id_buf float2(0)
#endif
