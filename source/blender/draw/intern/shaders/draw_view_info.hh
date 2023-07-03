/* SPDX-FileCopyrightText: 2023 Blender Foundation
 *
 * SPDX-License-Identifier: GPL-2.0-or-later */

#include "draw_defines.h"
#include "gpu_shader_create_info.hh"

/* -------------------------------------------------------------------- */
/** \name Resource ID
 *
 * This is used to fetch per object data in drw_matrices and other object indexed
 * buffers. There is multiple possibilities depending on how we are drawing the object.
 *
 * \{ */

/* Standard way. Use gpu_InstanceIndex to index the object data. */
GPU_SHADER_CREATE_INFO(draw_resource_id).define("DYNAMIC_RESOURCE_ID");

/**
 * Used if the resource index needs to be passed to the fragment shader.
 * IMPORTANT: Vertex and Geometry shaders need to use PASS_RESOURCE_ID in main().
 */
GPU_SHADER_INTERFACE_INFO(draw_resource_id_iface, "drw_ResourceID_iface")
    .flat(Type::INT, "resource_index");

GPU_SHADER_CREATE_INFO(draw_resource_id_varying)
    .vertex_out(draw_resource_id_iface)
    .geometry_out(draw_resource_id_iface); /* Used if needed. */

/* Variation used when drawing multiple instances for one object. */
GPU_SHADER_CREATE_INFO(draw_resource_id_uniform)
    .define("UNIFORM_RESOURCE_ID")
    .push_constant(Type::INT, "drw_ResourceID");

/**
 * Declare a resource handle that identify a unique object.
 * Requires draw_resource_id[_uniform].
 */
GPU_SHADER_CREATE_INFO(draw_resource_handle)
    .define("resource_handle (drw_resourceChunk * DRW_RESOURCE_CHUNK_LEN + resource_id)")
    .push_constant(Type::INT, "drw_resourceChunk");

/** \} */

/* -------------------------------------------------------------------- */
/** \name Draw View
 * \{ */

GPU_SHADER_CREATE_INFO(draw_view)
    .uniform_buf(DRW_VIEW_UBO_SLOT, "ViewMatrices", "drw_view_[DRW_VIEW_LEN]", Frequency::PASS)
    .define("drw_view", "drw_view_[drw_view_id]")
    .typedef_source("draw_shader_shared.h");

GPU_SHADER_CREATE_INFO(draw_view_culling)
    .uniform_buf(DRW_VIEW_CULLING_UBO_SLOT, "ViewCullingData", "drw_view_culling_[DRW_VIEW_LEN]")
    .define("drw_view_culling", "drw_view_culling_[drw_view_id]")
    .typedef_source("draw_shader_shared.h");

GPU_SHADER_CREATE_INFO(draw_modelmat)
    .uniform_buf(DRW_OBJ_MAT_UBO_SLOT,
                 "ObjectMatrices",
                 "drw_matrices[DRW_RESOURCE_CHUNK_LEN]",
                 Frequency::BATCH)
    .define("ModelMatrix", "(drw_matrices[resource_id].model)")
    .define("ModelMatrixInverse", "(drw_matrices[resource_id].model_inverse)")
    .additional_info("draw_view");

GPU_SHADER_CREATE_INFO(draw_modelmat_legacy)
    .define("DRW_LEGACY_MODEL_MATRIX")
    .push_constant(Type::MAT4, "ModelMatrix")
    .push_constant(Type::MAT4, "ModelMatrixInverse")
    .additional_info("draw_view");

GPU_SHADER_CREATE_INFO(draw_modelmat_instanced_attr)
    .push_constant(Type::MAT4, "ModelMatrix")
    .push_constant(Type::MAT4, "ModelMatrixInverse")
    .additional_info("draw_view");

/** \} */

/* -------------------------------------------------------------------- */
/** \name Draw View
 * \{ */

GPU_SHADER_CREATE_INFO(drw_clipped)
    /* TODO(fclem): Move to engine side. */
    .uniform_buf(DRW_CLIPPING_UBO_SLOT, "vec4", "drw_clipping_[6]", Frequency::PASS)
    .define("USE_WORLD_CLIP_PLANES");

/** \} */

/* -------------------------------------------------------------------- */
/** \name Draw Globals
 * \{ */

GPU_SHADER_CREATE_INFO(draw_globals)
    .typedef_source("draw_common_shader_shared.h")
    .uniform_buf(7, "GlobalsUboStorage", "globalsBlock", Frequency::PASS);

/** \} */

/* -------------------------------------------------------------------- */
/** \name Geometry Type
 * \{ */

GPU_SHADER_CREATE_INFO(draw_mesh).additional_info("draw_modelmat", "draw_resource_id");

GPU_SHADER_CREATE_INFO(draw_hair)
    .define("HAIR_SHADER")
    .define("DRW_HAIR_INFO")
    .sampler(15, ImageType::FLOAT_BUFFER, "hairPointBuffer")
    /* TODO(@fclem): Pack these into one UBO. */
    .push_constant(Type::INT, "hairStrandsRes")
    .push_constant(Type::INT, "hairThicknessRes")
    .push_constant(Type::FLOAT, "hairRadRoot")
    .push_constant(Type::FLOAT, "hairRadTip")
    .push_constant(Type::FLOAT, "hairRadShape")
    .push_constant(Type::BOOL, "hairCloseTip")
    .push_constant(Type::INT, "hairStrandOffset")
    .push_constant(Type::MAT4, "hairDupliMatrix")
    .additional_info("draw_modelmat", "draw_resource_id");

GPU_SHADER_CREATE_INFO(draw_hair_new)
    .define("HAIR_SHADER")
    .define("DRW_HAIR_INFO")
    .sampler(0, ImageType::FLOAT_BUFFER, "hairPointBuffer")
    /* TODO(@fclem): Pack these into one UBO. */
    .push_constant(Type::INT, "hairStrandsRes")
    .push_constant(Type::INT, "hairThicknessRes")
    .push_constant(Type::FLOAT, "hairRadRoot")
    .push_constant(Type::FLOAT, "hairRadTip")
    .push_constant(Type::FLOAT, "hairRadShape")
    .push_constant(Type::BOOL, "hairCloseTip")
    .push_constant(Type::INT, "hairStrandOffset")
    .push_constant(Type::MAT4, "hairDupliMatrix");

GPU_SHADER_CREATE_INFO(draw_pointcloud)
    .sampler(0, ImageType::FLOAT_BUFFER, "ptcloud_pos_rad_tx", Frequency::BATCH)
    .define("POINTCLOUD_SHADER")
    .define("DRW_POINTCLOUD_INFO")
    .vertex_in(0, Type::VEC4, "pos")
    .vertex_in(1, Type::VEC3, "pos_inst")
    .vertex_in(2, Type::VEC3, "nor")
    .additional_info("draw_modelmat_instanced_attr", "draw_resource_id_uniform");

GPU_SHADER_CREATE_INFO(draw_pointcloud_new)
    .sampler(0, ImageType::FLOAT_BUFFER, "ptcloud_pos_rad_tx", Frequency::BATCH)
    .define("POINTCLOUD_SHADER")
    .define("DRW_POINTCLOUD_INFO")
    .vertex_in(0, Type::VEC4, "pos")
    .vertex_in(1, Type::VEC3, "pos_inst")
    .vertex_in(2, Type::VEC3, "nor");

GPU_SHADER_CREATE_INFO(draw_volume).additional_info("draw_modelmat", "draw_resource_id_uniform");

GPU_SHADER_CREATE_INFO(draw_volume_new)
    .additional_info("draw_modelmat_new", "draw_resource_handle_new");

GPU_SHADER_CREATE_INFO(draw_gpencil)
    .typedef_source("gpencil_shader_shared.h")
    .define("DRW_GPENCIL_INFO")
    .sampler(0, ImageType::FLOAT_BUFFER, "gp_pos_tx")
    .sampler(1, ImageType::FLOAT_BUFFER, "gp_col_tx")
    /* Per Object */
    .push_constant(Type::FLOAT, "gpThicknessScale") /* TODO(fclem): Replace with object info. */
    .push_constant(Type::FLOAT, "gpThicknessWorldScale") /* TODO(fclem): Same as above. */
    .define("gpThicknessIsScreenSpace", "(gpThicknessWorldScale < 0.0)")
    /* Per Layer */
    .push_constant(Type::FLOAT, "gpThicknessOffset")
    .additional_info("draw_modelmat", "draw_object_infos");

GPU_SHADER_CREATE_INFO(draw_gpencil_new)
    .typedef_source("gpencil_shader_shared.h")
    .define("DRW_GPENCIL_INFO")
    .sampler(0, ImageType::FLOAT_BUFFER, "gp_pos_tx")
    .sampler(1, ImageType::FLOAT_BUFFER, "gp_col_tx")
    /* Per Object */
    .define("gpThicknessScale", "1.0")               /* TODO(fclem): Replace with object info. */
    .define("gpThicknessWorldScale", "1.0 / 2000.0") /* TODO(fclem): Same as above. */
    .define("gpThicknessIsScreenSpace", "(gpThicknessWorldScale < 0.0)")
    /* Per Layer */
    .define("gpThicknessOffset", "0.0") /* TODO(fclem): Remove. */
    .additional_info("draw_modelmat_new",
                     "draw_resource_id_varying",
                     "draw_view",
                     "draw_object_infos_new");

/** \} */

/* -------------------------------------------------------------------- */
/** \name Internal Draw Manager usage
 * \{ */

GPU_SHADER_CREATE_INFO(draw_resource_finalize)
    .do_static_compilation(true)
    .typedef_source("draw_shader_shared.h")
    .define("DRAW_FINALIZE_SHADER")
    .local_group_size(DRW_FINALIZE_GROUP_SIZE)
    .storage_buf(0, Qualifier::READ, "ObjectMatrices", "matrix_buf[]")
    .storage_buf(1, Qualifier::READ_WRITE, "ObjectBounds", "bounds_buf[]")
    .storage_buf(2, Qualifier::READ_WRITE, "ObjectInfos", "infos_buf[]")
    .push_constant(Type::INT, "resource_len")
    .compute_source("draw_resource_finalize_comp.glsl");

GPU_SHADER_CREATE_INFO(draw_view_finalize)
    .do_static_compilation(true)
    .local_group_size(64) /* DRW_VIEW_MAX */
    .define("DRW_VIEW_LEN", "64")
    .storage_buf(0, Qualifier::READ_WRITE, "ViewCullingData", "view_culling_buf[DRW_VIEW_LEN]")
    .compute_source("draw_view_finalize_comp.glsl")
    .additional_info("draw_view");

GPU_SHADER_CREATE_INFO(draw_visibility_compute)
    .do_static_compilation(true)
    .local_group_size(DRW_VISIBILITY_GROUP_SIZE)
    .define("DRW_VIEW_LEN", "64")
    .storage_buf(0, Qualifier::READ, "ObjectBounds", "bounds_buf[]")
    .storage_buf(1, Qualifier::READ_WRITE, "uint", "visibility_buf[]")
    .push_constant(Type::INT, "resource_len")
    .push_constant(Type::INT, "view_len")
    .push_constant(Type::INT, "visibility_word_per_draw")
    .compute_source("draw_visibility_comp.glsl")
    .additional_info("draw_view", "draw_view_culling");

GPU_SHADER_CREATE_INFO(draw_command_generate)
    .do_static_compilation(true)
    .typedef_source("draw_shader_shared.h")
    .typedef_source("draw_command_shared.hh")
    .local_group_size(DRW_COMMAND_GROUP_SIZE)
    .storage_buf(0, Qualifier::READ_WRITE, "DrawGroup", "group_buf[]")
    .storage_buf(1, Qualifier::READ, "uint", "visibility_buf[]")
    .storage_buf(2, Qualifier::READ, "DrawPrototype", "prototype_buf[]")
    .storage_buf(3, Qualifier::WRITE, "DrawCommand", "command_buf[]")
    .storage_buf(DRW_RESOURCE_ID_SLOT, Qualifier::WRITE, "uint", "resource_id_buf[]")
    .push_constant(Type::INT, "prototype_len")
    .push_constant(Type::INT, "visibility_word_per_draw")
    .push_constant(Type::INT, "view_shift")
    .push_constant(Type::BOOL, "use_custom_ids")
    .compute_source("draw_command_generate_comp.glsl");

/** \} */

/* -------------------------------------------------------------------- */
/** \name Draw Resource ID
 * New implementation using gl_BaseInstance and storage buffers.
 * \{ */

GPU_SHADER_CREATE_INFO(draw_resource_id_new)
    .define("UNIFORM_RESOURCE_ID_NEW")
    /* TODO (Miguel Pozo): This is an int for compatibility.
     * It should become uint once the "Next" ports are complete. */
    .storage_buf(DRW_RESOURCE_ID_SLOT, Qualifier::READ, "int", "resource_id_buf[]")
    .define("drw_ResourceID", "resource_id_buf[gpu_BaseInstance + gl_InstanceID]");

GPU_SHADER_CREATE_INFO(draw_resource_with_custom_id_new)
    .define("UNIFORM_RESOURCE_ID_NEW")
    .define("WITH_CUSTOM_IDS")
    .storage_buf(DRW_RESOURCE_ID_SLOT, Qualifier::READ, "int2", "resource_id_buf[]")
    .define("drw_ResourceID", "resource_id_buf[gpu_BaseInstance + gl_InstanceID].x")
    .define("drw_CustomID", "resource_id_buf[gpu_BaseInstance + gl_InstanceID].y");

/**
 * Workaround the lack of gl_BaseInstance by binding the resource_id_buf as vertex buf.
 */
GPU_SHADER_CREATE_INFO(draw_resource_id_fallback)
    .define("UNIFORM_RESOURCE_ID_NEW")
    .vertex_in(15, Type::INT, "drw_ResourceID");

GPU_SHADER_CREATE_INFO(draw_resource_with_custom_id_fallback)
    .define("UNIFORM_RESOURCE_ID_NEW")
    .define("WITH_CUSTOM_IDS")
    .vertex_in(15, Type::IVEC2, "vertex_in_drw_ResourceID")
    .define("drw_ResourceID", "vertex_in_drw_ResourceID.x")
    .define("drw_CustomID", "vertex_in_drw_ResourceID.y");

/** TODO mask view id bits. */
GPU_SHADER_CREATE_INFO(draw_resource_handle_new).define("resource_handle", "drw_ResourceID");

/** \} */

/* -------------------------------------------------------------------- */
/** \name Draw Object Resources
 * \{ */

GPU_SHADER_CREATE_INFO(draw_modelmat_new_common)
    .typedef_source("draw_shader_shared.h")
    .storage_buf(DRW_OBJ_MAT_SLOT, Qualifier::READ, "ObjectMatrices", "drw_matrix_buf[]")
    .define("drw_ModelMatrixInverse", "drw_matrix_buf[resource_id].model_inverse")
    .define("drw_ModelMatrix", "drw_matrix_buf[resource_id].model")
    /* TODO For compatibility with old shaders. To be removed. */
    .define("ModelMatrixInverse", "drw_ModelMatrixInverse")
    .define("ModelMatrix", "drw_ModelMatrix");

GPU_SHADER_CREATE_INFO(draw_modelmat_new)
    .additional_info("draw_modelmat_new_common", "draw_resource_id_new");

GPU_SHADER_CREATE_INFO(draw_modelmat_new_with_custom_id)
    .additional_info("draw_modelmat_new_common", "draw_resource_with_custom_id_new");

/** \} */
