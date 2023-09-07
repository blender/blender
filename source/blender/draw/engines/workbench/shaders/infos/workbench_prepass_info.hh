/* SPDX-FileCopyrightText: 2023 Blender Authors
 *
 * SPDX-License-Identifier: GPL-2.0-or-later */

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
    .additional_info("draw_modelmat_new_with_custom_id", "draw_resource_handle_new");

GPU_SHADER_CREATE_INFO(workbench_curves)
    .sampler(WB_CURVES_COLOR_SLOT, ImageType::FLOAT_BUFFER, "ac", Frequency::BATCH)
    .sampler(WB_CURVES_UV_SLOT, ImageType::FLOAT_BUFFER, "au", Frequency::BATCH)
    .push_constant(Type::INT, "emitter_object_id")
    .vertex_source("workbench_prepass_hair_vert.glsl")
    .additional_info("draw_modelmat_new_with_custom_id",
                     "draw_resource_handle_new",
                     "draw_hair_new");

GPU_SHADER_CREATE_INFO(workbench_pointcloud)
    .vertex_source("workbench_prepass_pointcloud_vert.glsl")
    .additional_info("draw_modelmat_new_with_custom_id",
                     "draw_resource_handle_new",
                     "draw_pointcloud_new");

/** \} */

/* -------------------------------------------------------------------- */
/** \name Lighting Type (only for transparent)
 * \{ */

GPU_SHADER_CREATE_INFO(workbench_lighting_flat).define("WORKBENCH_LIGHTING_FLAT");
GPU_SHADER_CREATE_INFO(workbench_lighting_studio).define("WORKBENCH_LIGHTING_STUDIO");
GPU_SHADER_CREATE_INFO(workbench_lighting_matcap)
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

GPU_SHADER_CREATE_INFO(workbench_color_material)
    .define("WORKBENCH_COLOR_MATERIAL")
    .storage_buf(WB_MATERIAL_SLOT, Qualifier::READ, "vec4", "materials_data[]");

GPU_SHADER_CREATE_INFO(workbench_color_texture)
    .define("WORKBENCH_COLOR_TEXTURE")
    .define("WORKBENCH_TEXTURE_IMAGE_ARRAY")
    .define("WORKBENCH_COLOR_MATERIAL")
    .storage_buf(WB_MATERIAL_SLOT, Qualifier::READ, "vec4", "materials_data[]")
    .sampler(2, ImageType::FLOAT_2D, "imageTexture", Frequency::BATCH)
    .sampler(3, ImageType::FLOAT_2D_ARRAY, "imageTileArray", Frequency::BATCH)
    .sampler(4, ImageType::FLOAT_1D_ARRAY, "imageTileData", Frequency::BATCH)
    .push_constant(Type::BOOL, "isImageTile")
    .push_constant(Type::BOOL, "imagePremult")
    .push_constant(Type::FLOAT, "imageTransparencyCutoff");

GPU_SHADER_CREATE_INFO(workbench_color_vertex).define("WORKBENCH_COLOR_VERTEX");

GPU_SHADER_CREATE_INFO(workbench_prepass)
    .define("WORKBENCH_NEXT")
    .uniform_buf(WB_WORLD_SLOT, "WorldData", "world_data")
    .vertex_out(workbench_material_iface)
    .additional_info("draw_view");

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
  WORKBENCH_COLOR_VARIATIONS(prefix##_matcap, "workbench_lighting_matcap", __VA_ARGS__)

#define WORKBENCH_PIPELINE_VARIATIONS(prefix, ...) \
  WORKBENCH_SHADING_VARIATIONS(prefix##_transparent, "workbench_transparent_accum", __VA_ARGS__) \
  WORKBENCH_SHADING_VARIATIONS(prefix##_opaque, "workbench_opaque", __VA_ARGS__)

#define WORKBENCH_GEOMETRY_VARIATIONS(prefix, ...) \
  WORKBENCH_PIPELINE_VARIATIONS(prefix##_mesh, "workbench_mesh", __VA_ARGS__) \
  WORKBENCH_PIPELINE_VARIATIONS(prefix##_curves, "workbench_curves", __VA_ARGS__) \
  WORKBENCH_PIPELINE_VARIATIONS(prefix##_ptcloud, "workbench_pointcloud", __VA_ARGS__)

WORKBENCH_GEOMETRY_VARIATIONS(workbench_prepass, "workbench_prepass");

#undef WORKBENCH_FINAL_VARIATION
#undef WORKBENCH_CLIPPING_VARIATIONS
#undef WORKBENCH_TEXTURE_VARIATIONS
#undef WORKBENCH_DATATYPE_VARIATIONS
#undef WORKBENCH_PIPELINE_VARIATIONS

/** \} */
