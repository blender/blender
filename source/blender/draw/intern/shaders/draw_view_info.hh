/* SPDX-License-Identifier: GPL-2.0-or-later */

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
    .uniform_buf(0, "ViewInfos", "drw_view", Frequency::PASS)
    .typedef_source("draw_shader_shared.h");

GPU_SHADER_CREATE_INFO(draw_modelmat)
    .uniform_buf(8, "ObjectMatrices", "drw_matrices[DRW_RESOURCE_CHUNK_LEN]", Frequency::BATCH)
    .define("ModelMatrix", "(drw_matrices[resource_id].drw_modelMatrix)")
    .define("ModelMatrixInverse", "(drw_matrices[resource_id].drw_modelMatrixInverse)")
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

GPU_SHADER_CREATE_INFO(drw_clipped).define("USE_WORLD_CLIP_PLANES");

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

GPU_SHADER_CREATE_INFO(draw_pointcloud)
    .vertex_in(0, Type::VEC4, "pos")
    .vertex_in(1, Type::VEC3, "pos_inst")
    .vertex_in(2, Type::VEC3, "nor")
    .additional_info("draw_modelmat_instanced_attr", "draw_resource_id_uniform");

GPU_SHADER_CREATE_INFO(draw_volume).additional_info("draw_modelmat", "draw_resource_id_uniform");

GPU_SHADER_CREATE_INFO(draw_gpencil)
    .typedef_source("gpencil_shader_shared.h")
    .define("DRW_GPENCIL_INFO")
    .vertex_in(0, Type::IVEC4, "ma")
    .vertex_in(1, Type::IVEC4, "ma1")
    .vertex_in(2, Type::IVEC4, "ma2")
    .vertex_in(3, Type::IVEC4, "ma3")
    .vertex_in(4, Type::VEC4, "pos")
    .vertex_in(5, Type::VEC4, "pos1")
    .vertex_in(6, Type::VEC4, "pos2")
    .vertex_in(7, Type::VEC4, "pos3")
    .vertex_in(8, Type::VEC4, "uv1")
    .vertex_in(9, Type::VEC4, "uv2")
    .vertex_in(10, Type::VEC4, "col1")
    .vertex_in(11, Type::VEC4, "col2")
    .vertex_in(12, Type::VEC4, "fcol1")
    /* Per Object */
    .push_constant(Type::FLOAT, "gpThicknessScale") /* TODO(fclem): Replace with object info. */
    .push_constant(Type::FLOAT, "gpThicknessWorldScale") /* TODO(fclem): Same as above. */
    .define("gpThicknessIsScreenSpace", "(gpThicknessWorldScale < 0.0)")
    /* Per Layer */
    .push_constant(Type::FLOAT, "gpThicknessOffset")
    .additional_info("draw_modelmat", "draw_resource_id_uniform", "draw_object_infos");

/** \} */
