/* SPDX-FileCopyrightText: 2023 Blender Authors
 *
 * SPDX-License-Identifier: GPL-2.0-or-later */

#include "draw_defines.hh"
#include "gpu_shader_create_info.hh"

/* -------------------------------------------------------------------- */
/** \name Resource ID
 *
 * This is used to fetch per object data in drw_matrices and other object indexed
 * buffers. There is multiple possibilities depending on how we are drawing the object.
 *
 * \{ */

/* Standard way. Use gpu_InstanceIndex to index the object data. */
GPU_SHADER_CREATE_INFO(draw_resource_id)
DEFINE("DYNAMIC_RESOURCE_ID")
GPU_SHADER_CREATE_END()

/**
 * Used if the resource index needs to be passed to the fragment shader.
 * IMPORTANT: Vertex and Geometry shaders need to use PASS_RESOURCE_ID in main().
 */
GPU_SHADER_NAMED_INTERFACE_INFO(draw_resource_id_iface, drw_ResourceID_iface)
FLAT(INT, resource_index)
GPU_SHADER_NAMED_INTERFACE_END(drw_ResourceID_iface)

GPU_SHADER_CREATE_INFO(draw_resource_id_varying)
VERTEX_OUT(draw_resource_id_iface)
GEOMETRY_OUT(draw_resource_id_iface)
GPU_SHADER_CREATE_END() /* Used if needed. */

/* Variation used when drawing multiple instances for one object. */
GPU_SHADER_CREATE_INFO(draw_resource_id_uniform)
DEFINE("UNIFORM_RESOURCE_ID")
PUSH_CONSTANT(INT, drw_ResourceID)
GPU_SHADER_CREATE_END()

/**
 * Declare a resource handle that identify a unique object.
 * Requires draw_resource_id[_uniform].
 */
GPU_SHADER_CREATE_INFO(draw_resource_handle)
DEFINE_VALUE("resource_handle", "(drw_resourceChunk * DRW_RESOURCE_CHUNK_LEN + resource_id)")
PUSH_CONSTANT(INT, drw_resourceChunk)
GPU_SHADER_CREATE_END()

/** \} */

/* -------------------------------------------------------------------- */
/** \name Draw View
 * \{ */

GPU_SHADER_CREATE_INFO(draw_view)
UNIFORM_BUF_FREQ(DRW_VIEW_UBO_SLOT, ViewMatrices, drw_view_[DRW_VIEW_LEN], PASS)
DEFINE("DRAW_VIEW_CREATE_INFO")
DEFINE_VALUE("drw_view", "drw_view_[drw_view_id]")
TYPEDEF_SOURCE("draw_shader_shared.hh")
GPU_SHADER_CREATE_END()

GPU_SHADER_CREATE_INFO(draw_view_culling)
UNIFORM_BUF(DRW_VIEW_CULLING_UBO_SLOT, ViewCullingData, drw_view_culling_[DRW_VIEW_LEN])
DEFINE("DRW_VIEW_CULLING_INFO")
DEFINE_VALUE("drw_view_culling", "drw_view_culling_[drw_view_id]")
TYPEDEF_SOURCE("draw_shader_shared.hh")
GPU_SHADER_CREATE_END()

GPU_SHADER_CREATE_INFO(draw_modelmat)
UNIFORM_BUF_FREQ(DRW_OBJ_MAT_UBO_SLOT, ObjectMatrices, drw_matrices[DRW_RESOURCE_CHUNK_LEN], BATCH)
DEFINE_VALUE("ModelMatrix", "(drw_matrices[resource_id].model)")
DEFINE_VALUE("ModelMatrixInverse", "(drw_matrices[resource_id].model_inverse)")
ADDITIONAL_INFO(draw_view)
GPU_SHADER_CREATE_END()

GPU_SHADER_CREATE_INFO(draw_modelmat_legacy)
DEFINE("DRW_LEGACY_MODEL_MATRIX")
PUSH_CONSTANT(MAT4, ModelMatrix)
PUSH_CONSTANT(MAT4, ModelMatrixInverse)
ADDITIONAL_INFO(draw_view)
GPU_SHADER_CREATE_END()

GPU_SHADER_CREATE_INFO(draw_modelmat_instanced_attr)
PUSH_CONSTANT(MAT4, ModelMatrix)
PUSH_CONSTANT(MAT4, ModelMatrixInverse)
ADDITIONAL_INFO(draw_view)
GPU_SHADER_CREATE_END()

/** \} */

/* -------------------------------------------------------------------- */
/** \name Draw View
 * \{ */

GPU_SHADER_CREATE_INFO(drw_clipped)
/* TODO(fclem): Move to engine side. */
UNIFORM_BUF_FREQ(DRW_CLIPPING_UBO_SLOT, vec4, drw_clipping_[6], PASS)
DEFINE("USE_WORLD_CLIP_PLANES")
GPU_SHADER_CREATE_END()

/** \} */

/* -------------------------------------------------------------------- */
/** \name Draw Globals
 * \{ */

GPU_SHADER_CREATE_INFO(draw_globals)
TYPEDEF_SOURCE("draw_common_shader_shared.hh")
UNIFORM_BUF_FREQ(7, GlobalsUboStorage, globalsBlock, PASS)
GPU_SHADER_CREATE_END()

/** \} */

/* -------------------------------------------------------------------- */
/** \name Geometry Type
 * \{ */

GPU_SHADER_CREATE_INFO(draw_mesh)
ADDITIONAL_INFO(draw_modelmat)
ADDITIONAL_INFO(draw_resource_id)
GPU_SHADER_CREATE_END()

GPU_SHADER_CREATE_INFO(draw_mesh_new)
ADDITIONAL_INFO(draw_modelmat_new)
ADDITIONAL_INFO(draw_resource_id)
GPU_SHADER_CREATE_END()

GPU_SHADER_CREATE_INFO(draw_hair)
DEFINE("HAIR_SHADER")
DEFINE("DRW_HAIR_INFO")
SAMPLER(15, FLOAT_BUFFER, hairPointBuffer)
/* TODO(@fclem): Pack these into one UBO. */
PUSH_CONSTANT(INT, hairStrandsRes)
PUSH_CONSTANT(INT, hairThicknessRes)
PUSH_CONSTANT(FLOAT, hairRadRoot)
PUSH_CONSTANT(FLOAT, hairRadTip)
PUSH_CONSTANT(FLOAT, hairRadShape)
PUSH_CONSTANT(BOOL, hairCloseTip)
PUSH_CONSTANT(INT, hairStrandOffset)
PUSH_CONSTANT(MAT4, hairDupliMatrix)
ADDITIONAL_INFO(draw_modelmat)
ADDITIONAL_INFO(draw_resource_id)
GPU_SHADER_CREATE_END()

GPU_SHADER_CREATE_INFO(draw_hair_new)
DEFINE("HAIR_SHADER")
DEFINE("DRW_HAIR_INFO")
SAMPLER(0, FLOAT_BUFFER, hairPointBuffer)
/* TODO(@fclem): Pack these into one UBO. */
PUSH_CONSTANT(INT, hairStrandsRes)
PUSH_CONSTANT(INT, hairThicknessRes)
PUSH_CONSTANT(FLOAT, hairRadRoot)
PUSH_CONSTANT(FLOAT, hairRadTip)
PUSH_CONSTANT(FLOAT, hairRadShape)
PUSH_CONSTANT(BOOL, hairCloseTip)
PUSH_CONSTANT(INT, hairStrandOffset)
PUSH_CONSTANT(MAT4, hairDupliMatrix)
GPU_SHADER_CREATE_END()

GPU_SHADER_CREATE_INFO(draw_pointcloud)
SAMPLER_FREQ(0, FLOAT_BUFFER, ptcloud_pos_rad_tx, BATCH)
DEFINE("POINTCLOUD_SHADER")
DEFINE("DRW_POINTCLOUD_INFO")
ADDITIONAL_INFO(draw_modelmat_instanced_attr)
ADDITIONAL_INFO(draw_resource_id_uniform)
GPU_SHADER_CREATE_END()

GPU_SHADER_CREATE_INFO(draw_pointcloud_new)
SAMPLER_FREQ(0, FLOAT_BUFFER, ptcloud_pos_rad_tx, BATCH)
DEFINE("POINTCLOUD_SHADER")
DEFINE("DRW_POINTCLOUD_INFO")
GPU_SHADER_CREATE_END()

GPU_SHADER_CREATE_INFO(draw_volume)
ADDITIONAL_INFO(draw_modelmat)
ADDITIONAL_INFO(draw_resource_id_uniform)
GPU_SHADER_CREATE_END()

GPU_SHADER_CREATE_INFO(draw_volume_new)
ADDITIONAL_INFO(draw_modelmat_new)
ADDITIONAL_INFO(draw_resource_handle_new)
GPU_SHADER_CREATE_END()

GPU_SHADER_CREATE_INFO(draw_gpencil)
TYPEDEF_SOURCE("gpencil_shader_shared.h")
DEFINE("DRW_GPENCIL_INFO")
SAMPLER(0, FLOAT_BUFFER, gp_pos_tx)
SAMPLER(1, FLOAT_BUFFER, gp_col_tx)
/* Per Object */
PUSH_CONSTANT(FLOAT, gpThicknessScale)      /* TODO(fclem): Replace with object info. */
PUSH_CONSTANT(FLOAT, gpThicknessWorldScale) /* TODO(fclem): Same as above. */
DEFINE_VALUE("gpThicknessIsScreenSpace", "(gpThicknessWorldScale < 0.0)")
/* Per Layer */
PUSH_CONSTANT(FLOAT, gpThicknessOffset)
ADDITIONAL_INFO(draw_modelmat)
ADDITIONAL_INFO(draw_object_infos)
GPU_SHADER_CREATE_END()

GPU_SHADER_CREATE_INFO(draw_gpencil_new)
TYPEDEF_SOURCE("gpencil_shader_shared.h")
DEFINE("DRW_GPENCIL_INFO")
SAMPLER(0, FLOAT_BUFFER, gp_pos_tx)
SAMPLER(1, FLOAT_BUFFER, gp_col_tx)
/* Per Object */
PUSH_CONSTANT(FLOAT, gpThicknessScale)      /* TODO(fclem): Replace with object info. */
PUSH_CONSTANT(FLOAT, gpThicknessWorldScale) /* TODO(fclem): Same as above. */
DEFINE_VALUE("gpThicknessIsScreenSpace", "(gpThicknessWorldScale < 0.0)")
/* Per Layer */
PUSH_CONSTANT(FLOAT, gpThicknessOffset)
ADDITIONAL_INFO(draw_resource_id_varying)
ADDITIONAL_INFO(draw_view)
ADDITIONAL_INFO(draw_object_infos_new)
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
STORAGE_BUF(0, READ, ObjectMatrices, matrix_buf[])
STORAGE_BUF(1, READ_WRITE, ObjectBounds, bounds_buf[])
STORAGE_BUF(2, READ_WRITE, ObjectInfos, infos_buf[])
PUSH_CONSTANT(INT, resource_len)
COMPUTE_SOURCE("draw_resource_finalize_comp.glsl")
GPU_SHADER_CREATE_END()

GPU_SHADER_CREATE_INFO(draw_view_finalize)
DO_STATIC_COMPILATION()
LOCAL_GROUP_SIZE(DRW_VIEW_MAX)
DEFINE_VALUE("DRW_VIEW_LEN", STRINGIFY(DRW_VIEW_MAX))
STORAGE_BUF(0, READ_WRITE, ViewCullingData, view_culling_buf[DRW_VIEW_LEN])
COMPUTE_SOURCE("draw_view_finalize_comp.glsl")
ADDITIONAL_INFO(draw_view)
GPU_SHADER_CREATE_END()

GPU_SHADER_CREATE_INFO(draw_visibility_compute)
DO_STATIC_COMPILATION()
LOCAL_GROUP_SIZE(DRW_VISIBILITY_GROUP_SIZE)
DEFINE_VALUE("DRW_VIEW_LEN", STRINGIFY(DRW_VIEW_MAX))
STORAGE_BUF(0, READ, ObjectBounds, bounds_buf[])
STORAGE_BUF(1, READ_WRITE, uint, visibility_buf[])
PUSH_CONSTANT(INT, resource_len)
PUSH_CONSTANT(INT, view_len)
PUSH_CONSTANT(INT, visibility_word_per_draw)
COMPUTE_SOURCE("draw_visibility_comp.glsl")
ADDITIONAL_INFO(draw_view)
ADDITIONAL_INFO(draw_view_culling)
GPU_SHADER_CREATE_END()

GPU_SHADER_CREATE_INFO(draw_command_generate)
DO_STATIC_COMPILATION()
TYPEDEF_SOURCE("draw_shader_shared.hh")
TYPEDEF_SOURCE("draw_command_shared.hh")
LOCAL_GROUP_SIZE(DRW_COMMAND_GROUP_SIZE)
STORAGE_BUF(0, READ_WRITE, DrawGroup, group_buf[])
STORAGE_BUF(1, READ, uint, visibility_buf[])
STORAGE_BUF(2, READ, DrawPrototype, prototype_buf[])
STORAGE_BUF(3, WRITE, DrawCommand, command_buf[])
STORAGE_BUF(DRW_RESOURCE_ID_SLOT, WRITE, uint, resource_id_buf[])
PUSH_CONSTANT(INT, prototype_len)
PUSH_CONSTANT(INT, visibility_word_per_draw)
PUSH_CONSTANT(INT, view_shift)
PUSH_CONSTANT(INT, view_len)
PUSH_CONSTANT(BOOL, use_custom_ids)
COMPUTE_SOURCE("draw_command_generate_comp.glsl")
GPU_SHADER_CREATE_END()

/** \} */

/* -------------------------------------------------------------------- */
/** \name Draw Resource ID
 * New implementation using gl_BaseInstance and storage buffers.
 * \{ */

GPU_SHADER_CREATE_INFO(draw_resource_id_new)
DEFINE("UNIFORM_RESOURCE_ID_NEW")
/* TODO (Miguel Pozo): This is an int for compatibility.
 * It should become uint once the "Next" ports are complete. */
STORAGE_BUF(DRW_RESOURCE_ID_SLOT, READ, int, resource_id_buf[])
DEFINE_VALUE("drw_ResourceID", "resource_id_buf[gpu_BaseInstance + gl_InstanceID]")
GPU_SHADER_CREATE_END()

GPU_SHADER_CREATE_INFO(draw_resource_with_custom_id_new)
DEFINE("UNIFORM_RESOURCE_ID_NEW")
DEFINE("WITH_CUSTOM_IDS")
STORAGE_BUF(DRW_RESOURCE_ID_SLOT, READ, int2, resource_id_buf[])
DEFINE_VALUE("drw_ResourceID", "resource_id_buf[gpu_BaseInstance + gl_InstanceID].x")
DEFINE_VALUE("drw_CustomID", "resource_id_buf[gpu_BaseInstance + gl_InstanceID].y")
GPU_SHADER_CREATE_END()

/**
 * Workaround the lack of gl_BaseInstance by binding the resource_id_buf as vertex buf.
 */
GPU_SHADER_CREATE_INFO(draw_resource_id_fallback)
DEFINE("UNIFORM_RESOURCE_ID_NEW")
VERTEX_IN(15, INT, drw_ResourceID)
GPU_SHADER_CREATE_END()

GPU_SHADER_CREATE_INFO(draw_resource_with_custom_id_fallback)
DEFINE("UNIFORM_RESOURCE_ID_NEW")
DEFINE("WITH_CUSTOM_IDS")
VERTEX_IN(15, IVEC2, vertex_in_drw_ResourceID)
DEFINE_VALUE("drw_ResourceID", "vertex_in_drw_ResourceID.x")
DEFINE_VALUE("drw_CustomID", "vertex_in_drw_ResourceID.y")
GPU_SHADER_CREATE_END()

/** TODO mask view id bits. */
GPU_SHADER_CREATE_INFO(draw_resource_handle_new)
DEFINE_VALUE("resource_handle", "drw_ResourceID")
GPU_SHADER_CREATE_END()

/** \} */

/* -------------------------------------------------------------------- */
/** \name Draw Object Resources
 * \{ */

GPU_SHADER_CREATE_INFO(draw_modelmat_new_common)
TYPEDEF_SOURCE("draw_shader_shared.hh")
STORAGE_BUF(DRW_OBJ_MAT_SLOT, READ, ObjectMatrices, drw_matrix_buf[])
DEFINE("DRAW_MODELMAT_CREATE_INFO")
DEFINE_VALUE("drw_ModelMatrixInverse", "drw_matrix_buf[resource_id].model_inverse")
DEFINE_VALUE("drw_ModelMatrix", "drw_matrix_buf[resource_id].model")
/* TODO For compatibility with old shaders. To be removed. */
DEFINE_VALUE("ModelMatrixInverse", "drw_ModelMatrixInverse")
DEFINE_VALUE("ModelMatrix", "drw_ModelMatrix")
GPU_SHADER_CREATE_END()

GPU_SHADER_CREATE_INFO(draw_modelmat_new)
ADDITIONAL_INFO(draw_modelmat_new_common)
ADDITIONAL_INFO(draw_resource_id_new)
GPU_SHADER_CREATE_END()

GPU_SHADER_CREATE_INFO(draw_modelmat_new_with_custom_id)
ADDITIONAL_INFO(draw_modelmat_new_common)
ADDITIONAL_INFO(draw_resource_with_custom_id_new)
GPU_SHADER_CREATE_END()

/** \} */
