
#include "gpu_shader_create_info.hh"

/* -------------------------------------------------------------------- */
/** \name Draw View
 * \{ */

GPU_SHADER_CREATE_INFO(draw_view)
    .uniform_buf(0, "ViewInfos", "drw_view", Frequency::PASS)
    .typedef_source("draw_shader_shared.h");

GPU_SHADER_CREATE_INFO(draw_view_instanced_attr)
    .push_constant(0, Type::MAT4, "ModelMatrix")
    .push_constant(16, Type::MAT4, "ModelMatrixInverse")
    .additional_info("draw_view");

/** \} */

/* -------------------------------------------------------------------- */
/** \name Geometry Type
 * \{ */

GPU_SHADER_CREATE_INFO(draw_mesh)
    .uniform_buf(8, "ObjectMatrices", "drw_matrices[DRW_RESOURCE_CHUNK_LEN]", Frequency::BATCH)
    .additional_info("draw_view");

GPU_SHADER_CREATE_INFO(draw_hair)
    /* TODO(fclem) Finish */
    .uniform_buf(8, "ObjectMatrices", "drw_matrices[DRW_RESOURCE_CHUNK_LEN]", Frequency::BATCH)
    .additional_info("draw_view");

GPU_SHADER_CREATE_INFO(draw_pointcloud)
    .vertex_in(0, Type::VEC4, "pos")
    .vertex_in(1, Type::VEC3, "pos_inst")
    .vertex_in(2, Type::VEC3, "nor")
    .define("UNIFORM_RESOURCE_ID")
    .define("INSTANCED_ATTR")
    .additional_info("draw_view_instanced_attr");

/** \} */
