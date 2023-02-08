/* SPDX-License-Identifier: GPL-2.0-or-later */

#include "gpu_shader_create_info.hh"
#include "workbench_defines.hh"

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
/** \name Object Type
 * \{ */

GPU_SHADER_CREATE_INFO(workbench_next_mesh)
    .vertex_in(0, Type::VEC3, "pos")
    .vertex_in(1, Type::VEC3, "nor")
    .vertex_in(2, Type::VEC4, "ac")
    .vertex_in(3, Type::VEC2, "au")
    .vertex_source("workbench_prepass_vert.glsl")
    .additional_info("draw_modelmat_new")
    .additional_info("draw_resource_handle_new");

GPU_SHADER_CREATE_INFO(workbench_next_curves)
    /* TODO Adding workbench_next_mesh to avoid shader compilation errors */
    .additional_info("workbench_next_mesh");

GPU_SHADER_CREATE_INFO(workbench_next_pointcloud)
    /* TODO Adding workbench_next_mesh to avoid shader compilation errors */
    .additional_info("workbench_next_mesh");

/** \} */

/* -------------------------------------------------------------------- */
/** \name Texture Type
 * \{ */

GPU_SHADER_CREATE_INFO(workbench_texture_none).define("TEXTURE_NONE");

GPU_SHADER_CREATE_INFO(workbench_texture_single)
    .sampler(2, ImageType::FLOAT_2D, "imageTexture", Frequency::BATCH)
    .push_constant(Type::BOOL, "imagePremult")
    .push_constant(Type::FLOAT, "imageTransparencyCutoff")
    .define("WORKBENCH_COLOR_TEXTURE");

GPU_SHADER_CREATE_INFO(workbench_texture_tile)
    .sampler(2, ImageType::FLOAT_2D_ARRAY, "imageTileArray", Frequency::BATCH)
    .sampler(3, ImageType::FLOAT_1D_ARRAY, "imageTileData", Frequency::BATCH)
    .push_constant(Type::BOOL, "imagePremult")
    .push_constant(Type::FLOAT, "imageTransparencyCutoff")
    .define("WORKBENCH_COLOR_TEXTURE")
    .define("WORKBENCH_TEXTURE_IMAGE_ARRAY");

/** \} */

/* -------------------------------------------------------------------- */
/** \name Lighting Type (only for transparent)
 * \{ */

GPU_SHADER_CREATE_INFO(workbench_lighting_flat).define("WORKBENCH_LIGHTING_FLAT");
GPU_SHADER_CREATE_INFO(workbench_lighting_studio).define("WORKBENCH_LIGHTING_STUDIO");
GPU_SHADER_CREATE_INFO(workbench_lighting_matcap)
    .define("WORKBENCH_LIGHTING_MATCAP")
    .sampler(4, ImageType::FLOAT_2D, "matcap_diffuse_tx")
    .sampler(5, ImageType::FLOAT_2D, "matcap_specular_tx");

GPU_SHADER_CREATE_INFO(workbench_next_lighting_matcap)
    .define("WORKBENCH_LIGHTING_MATCAP")
    .sampler(WB_MATCAP_SLOT, ImageType::FLOAT_2D_ARRAY, "matcap_tx");

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
    .flat(Type::FLOAT, "_roughness")
    .flat(Type::FLOAT, "metallic");

GPU_SHADER_CREATE_INFO(workbench_material)
    .uniform_buf(WB_WORLD_SLOT, "WorldData", "world_data")
    .uniform_buf(5, "vec4", "materials_data[4096]")
    .push_constant(Type::INT, "materialIndex")
    .push_constant(Type::BOOL, "useMatcap")
    .vertex_out(workbench_material_iface);

GPU_SHADER_CREATE_INFO(workbench_next_prepass)
    .define("WORKBENCH_NEXT")
    .uniform_buf(WB_WORLD_SLOT, "WorldData", "world_data")
    .vertex_out(workbench_material_iface)
    .additional_info("draw_view");

/** \} */

/* -------------------------------------------------------------------- */
/** \name Material Interface
 * \{ */

GPU_SHADER_CREATE_INFO(workbench_color_material)
    .define("WORKBENCH_COLOR_MATERIAL")
    .storage_buf(WB_MATERIAL_SLOT, Qualifier::READ, "vec4", "materials_data[]");

GPU_SHADER_CREATE_INFO(workbench_color_texture)
    .define("WORKBENCH_COLOR_TEXTURE")
    .define("WORKBENCH_TEXTURE_IMAGE_ARRAY")
    .define("WORKBENCH_COLOR_MATERIAL")
    .storage_buf(WB_MATERIAL_SLOT, Qualifier::READ, "vec4", "materials_data[]")
    .sampler(1, ImageType::FLOAT_2D, "imageTexture", Frequency::BATCH)
    .sampler(2, ImageType::FLOAT_2D_ARRAY, "imageTileArray", Frequency::BATCH)
    .sampler(3, ImageType::FLOAT_1D_ARRAY, "imageTileData", Frequency::BATCH)
    .push_constant(Type::BOOL, "isImageTile")
    .push_constant(Type::BOOL, "imagePremult")
    .push_constant(Type::FLOAT, "imageTransparencyCutoff");

GPU_SHADER_CREATE_INFO(workbench_color_vertex).define("WORKBENCH_COLOR_VERTEX");

/** \} */

/* -------------------------------------------------------------------- */
/** \name Pipeline Type
 * \{ */

GPU_SHADER_CREATE_INFO(workbench_transparent_accum)
    /* NOTE: Blending will be skipped on objectId because output is a
     * non-normalized integer buffer. */
    .fragment_out(0, Type::VEC4, "out_transparent_accum")
    .fragment_out(1, Type::VEC4, "out_revealage_accum")
    .fragment_out(2, Type::UINT, "out_object_id")
    .push_constant(Type::BOOL, "forceShadowing")
    .typedef_source("workbench_shader_shared.h")
    .fragment_source("workbench_transparent_accum_frag.glsl");

GPU_SHADER_CREATE_INFO(workbench_opaque)
    .fragment_out(0, Type::VEC4, "out_material")
    .fragment_out(1, Type::VEC2, "out_normal")
    .fragment_out(2, Type::UINT, "out_object_id")
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

#undef WORKBENCH_FINAL_VARIATION
#undef WORKBENCH_CLIPPING_VARIATIONS
#undef WORKBENCH_TEXTURE_VARIATIONS
#undef WORKBENCH_DATATYPE_VARIATIONS
#undef WORKBENCH_PIPELINE_VARIATIONS

/** \} */

/* -------------------------------------------------------------------- */
/** \name Variations Declaration
 * \{ */

GPU_SHADER_CREATE_INFO(workbench_flat).define("WORKBENCH_SHADING_FLAT");
GPU_SHADER_CREATE_INFO(workbench_studio).define("WORKBENCH_SHADING_STUDIO");
GPU_SHADER_CREATE_INFO(workbench_matcap).define("WORKBENCH_SHADING_MATCAP");

#define WORKBENCH_FINAL_VARIATION(name, ...) \
  GPU_SHADER_CREATE_INFO(name).additional_info(__VA_ARGS__).do_static_compilation(true);

#define WORKBENCH_CLIPPING_VARIATIONS(prefix, ...) \
  WORKBENCH_FINAL_VARIATION(prefix##_clip, "drw_clipped", __VA_ARGS__) \
  WORKBENCH_FINAL_VARIATION(prefix##_no_clip, __VA_ARGS__)

#define WORKBENCH_COLOR_VARIATIONS(prefix, ...) \
  WORKBENCH_CLIPPING_VARIATIONS(prefix##_material, "workbench_color_material", __VA_ARGS__) \
  WORKBENCH_CLIPPING_VARIATIONS(prefix##_texture, "workbench_color_texture", __VA_ARGS__) \
  WORKBENCH_CLIPPING_VARIATIONS(prefix##_vertex, "workbench_color_vertex", __VA_ARGS__)

#define WORKBENCH_SHADING_VARIATIONS(prefix, ...) \
  WORKBENCH_COLOR_VARIATIONS(prefix##_flat, "workbench_lighting_flat", __VA_ARGS__) \
  WORKBENCH_COLOR_VARIATIONS(prefix##_studio, "workbench_lighting_studio", __VA_ARGS__) \
  WORKBENCH_COLOR_VARIATIONS(prefix##_matcap, "workbench_next_lighting_matcap", __VA_ARGS__)

#define WORKBENCH_PIPELINE_VARIATIONS(prefix, ...) \
  WORKBENCH_SHADING_VARIATIONS(prefix##_transparent, "workbench_transparent_accum", __VA_ARGS__) \
  WORKBENCH_SHADING_VARIATIONS(prefix##_opaque, "workbench_opaque", __VA_ARGS__)

#define WORKBENCH_GEOMETRY_VARIATIONS(prefix, ...) \
  WORKBENCH_PIPELINE_VARIATIONS(prefix##_mesh, "workbench_next_mesh", __VA_ARGS__) \
  WORKBENCH_PIPELINE_VARIATIONS(prefix##_curves, "workbench_next_curves", __VA_ARGS__) \
  WORKBENCH_PIPELINE_VARIATIONS(prefix##_ptcloud, "workbench_next_pointcloud", __VA_ARGS__)

WORKBENCH_GEOMETRY_VARIATIONS(workbench_next_prepass, "workbench_next_prepass");

#undef WORKBENCH_FINAL_VARIATION
#undef WORKBENCH_CLIPPING_VARIATIONS
#undef WORKBENCH_TEXTURE_VARIATIONS
#undef WORKBENCH_DATATYPE_VARIATIONS
#undef WORKBENCH_PIPELINE_VARIATIONS

/** \} */
