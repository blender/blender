
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
 * Requires draw_resource_id[_constant].
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
/** \name Geometry Type
 * \{ */

GPU_SHADER_CREATE_INFO(draw_mesh).additional_info("draw_modelmat", "draw_resource_id");

GPU_SHADER_CREATE_INFO(draw_hair)
    .sampler(15, ImageType::FLOAT_BUFFER, "hairPointBuffer")
    .sampler(14, ImageType::UINT_BUFFER, "hairStrandBuffer")
    .sampler(13, ImageType::UINT_BUFFER, "hairStrandSegBuffer")
    /* TODO(fclem) Pack thoses into one UBO. */
    .push_constant(Type::INT, "hairStrandsRes")
    .push_constant(Type::INT, "hairThicknessRes")
    .push_constant(Type::FLOAT, "hairRadRoot")
    .push_constant(Type::FLOAT, "hairRadTip")
    .push_constant(Type::FLOAT, "hairRadShape")
    .push_constant(Type::BOOL, "hairCloseTip")
    .push_constant(Type::INT, "hairStrandOffset")
    .push_constant(Type::VEC4, "hairDupliMatrix", 4)
    .additional_info("draw_modelmat", "draw_resource_id");

GPU_SHADER_CREATE_INFO(draw_pointcloud)
    .vertex_in(0, Type::VEC4, "pos")
    .vertex_in(1, Type::VEC3, "pos_inst")
    .vertex_in(2, Type::VEC3, "nor")
    .additional_info("draw_modelmat_instanced_attr", "draw_resource_id_uniform");

GPU_SHADER_CREATE_INFO(draw_volume).additional_info("draw_modelmat", "draw_resource_id_uniform");

/** \} */
