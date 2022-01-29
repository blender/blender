
#include "gpu_shader_create_info.hh"

/* -------------------------------------------------------------------- */
/** \name Object Type
 * \{ */

GPU_SHADER_CREATE_INFO(workbench_mesh)
    .vertex_in(0, Type::VEC3, "pos")
    .vertex_in(1, Type::VEC3, "nor")
    .vertex_in(2, Type::VEC4, "ac")
    .vertex_in(3, Type::VEC2, "au")
    .vertex_source("workbench_prepass_vert.glsl")
    .additional_info("draw_mesh")
    .additional_info("draw_resource_handle");

GPU_SHADER_CREATE_INFO(workbench_hair)
    .sampler(0, ImageType::FLOAT_BUFFER, "ac", Frequency::BATCH)
    .sampler(1, ImageType::FLOAT_BUFFER, "au", Frequency::BATCH)
    .vertex_source("workbench_prepass_hair_vert.glsl")
    .additional_info("draw_hair")
    .additional_info("draw_resource_handle");

GPU_SHADER_CREATE_INFO(workbench_pointcloud)
    .vertex_source("workbench_prepass_pointcloud_vert.glsl")
    .additional_info("draw_pointcloud")
    .additional_info("draw_resource_handle");

/** \} */

/* -------------------------------------------------------------------- */
/** \name Texture Type
 * \{ */

GPU_SHADER_CREATE_INFO(workbench_texture_none).define("TEXTURE_NONE");

GPU_SHADER_CREATE_INFO(workbench_texture_single)
    .sampler(2, ImageType::FLOAT_2D, "imageTexture", Frequency::BATCH)
    .push_constant(Type::BOOL, "imagePremult")
    .push_constant(Type::FLOAT, "imageTransparencyCutoff")
    .define("V3D_SHADING_TEXTURE_COLOR");

GPU_SHADER_CREATE_INFO(workbench_texture_tile)
    .sampler(2, ImageType::FLOAT_2D_ARRAY, "imageTileArray", Frequency::BATCH)
    .sampler(3, ImageType::FLOAT_1D_ARRAY, "imageTileData", Frequency::BATCH)
    .push_constant(Type::BOOL, "imagePremult")
    .push_constant(Type::FLOAT, "imageTransparencyCutoff")
    .define("V3D_SHADING_TEXTURE_COLOR")
    .define("TEXTURE_IMAGE_ARRAY");

/** \} */

/* -------------------------------------------------------------------- */
/** \name Lighting Type (only for transparent)
 * \{ */

GPU_SHADER_CREATE_INFO(workbench_lighting_flat).define("V3D_LIGHTING_FLAT");
GPU_SHADER_CREATE_INFO(workbench_lighting_studio).define("V3D_LIGHTING_STUDIO");
GPU_SHADER_CREATE_INFO(workbench_lighting_matcap)
    .define("V3D_LIGHTING_MATCAP")
    .sampler(4, ImageType::FLOAT_2D, "matcap_diffuse_tx", Frequency::PASS)
    .sampler(5, ImageType::FLOAT_2D, "matcap_specular_tx", Frequency::PASS);

/** \} */

/* -------------------------------------------------------------------- */
/** \name Material Interface
 * \{ */

GPU_SHADER_INTERFACE_INFO(workbench_material_iface, "")
    .smooth(Type::VEC3, "normal_interp")
    .smooth(Type::VEC3, "color_interp")
    .smooth(Type::FLOAT, "alpha_interp")
    .smooth(Type::VEC2, "uv_interp")
    .flat(Type::INT, "object_id")
    .flat(Type::FLOAT, "roughness")
    .flat(Type::FLOAT, "metallic");

GPU_SHADER_CREATE_INFO(workbench_material)
    .uniform_buf(4, "WorldData", "world_data", Frequency::PASS)
    .uniform_buf(5, "vec4", "materials_data[4096]", Frequency::PASS)
    .push_constant(Type::INT, "materialIndex")
    .push_constant(Type::BOOL, "useMatcap")
    .vertex_out(workbench_material_iface);

/** \} */

/* -------------------------------------------------------------------- */
/** \name Pipeline Type
 * \{ */

GPU_SHADER_CREATE_INFO(workbench_transparent_accum)
    /* Note: Blending will be skipped on objectId because output is a
       non-normalized integer buffer. */
    .fragment_out(0, Type::VEC4, "transparentAccum")
    .fragment_out(1, Type::VEC4, "revealageAccum")
    .fragment_out(2, Type::UINT, "objectId")
    .push_constant(Type::BOOL, "forceShadowing")
    .typedef_source("workbench_shader_shared.h")
    .fragment_source("workbench_transparent_accum_frag.glsl");

GPU_SHADER_CREATE_INFO(workbench_opaque)
    .fragment_out(0, Type::VEC4, "materialData")
    .fragment_out(1, Type::VEC2, "normalData")
    .fragment_out(2, Type::UINT, "objectId")
    .typedef_source("workbench_shader_shared.h")
    .fragment_source("workbench_prepass_frag.glsl");

/** \} */

/* -------------------------------------------------------------------- */
/** \name Variations Declaration
 * \{ */

#define WORKBENCH_FINAL_VARIATION(name, ...) \
  GPU_SHADER_CREATE_INFO(name).additional_info(__VA_ARGS__).do_static_compilation(true);

#define WORKBENCH_CLIPPING_VARIATIONS(prefix, ...) \
  WORKBENCH_FINAL_VARIATION(prefix##_clip, "drw_clipped", __VA_ARGS__) \
  WORKBENCH_FINAL_VARIATION(prefix##_no_clip, __VA_ARGS__)

#define WORKBENCH_TEXTURE_VARIATIONS(prefix, ...) \
  WORKBENCH_CLIPPING_VARIATIONS(prefix##_tex_none, "workbench_texture_none", __VA_ARGS__) \
  WORKBENCH_CLIPPING_VARIATIONS(prefix##_tex_single, "workbench_texture_single", __VA_ARGS__) \
  WORKBENCH_CLIPPING_VARIATIONS(prefix##_tex_tile, "workbench_texture_tile", __VA_ARGS__)

#define WORKBENCH_DATATYPE_VARIATIONS(prefix, ...) \
  WORKBENCH_TEXTURE_VARIATIONS(prefix##_mesh, "workbench_mesh", __VA_ARGS__) \
  WORKBENCH_TEXTURE_VARIATIONS(prefix##_hair, "workbench_hair", __VA_ARGS__) \
  WORKBENCH_TEXTURE_VARIATIONS(prefix##_ptcloud, "workbench_pointcloud", __VA_ARGS__)

#define WORKBENCH_PIPELINE_VARIATIONS(prefix, ...) \
  WORKBENCH_DATATYPE_VARIATIONS(prefix##_transp_studio, \
                                "workbench_transparent_accum", \
                                "workbench_lighting_studio", \
                                __VA_ARGS__) \
  WORKBENCH_DATATYPE_VARIATIONS(prefix##_transp_matcap, \
                                "workbench_transparent_accum", \
                                "workbench_lighting_matcap", \
                                __VA_ARGS__) \
  WORKBENCH_DATATYPE_VARIATIONS(prefix##_transp_flat, \
                                "workbench_transparent_accum", \
                                "workbench_lighting_flat", \
                                __VA_ARGS__) \
  WORKBENCH_DATATYPE_VARIATIONS(prefix##_opaque, "workbench_opaque", __VA_ARGS__)

WORKBENCH_PIPELINE_VARIATIONS(workbench, "workbench_material");

/** \} */
