/* SPDX-FileCopyrightText: 2023 Blender Authors
 *
 * SPDX-License-Identifier: GPL-2.0-or-later */

#include "draw_defines.hh"

#include "gpu_shader_create_info.hh"

/* -------------------------------------------------------------------- */
/** \name Common
 * \{ */

GPU_SHADER_CREATE_INFO(workbench_shadow_common)
    .storage_buf(3, Qualifier::READ, "float", "pos[]", Frequency::GEOMETRY)
    /* WORKAROUND: Needed to support OpenSubdiv vertex format. Should be removed. */
    .push_constant(Type::IVEC2, "gpu_attr_3")
    .uniform_buf(1, "ShadowPassData", "pass_data")
    .typedef_source("workbench_shader_shared.h")
    .additional_info("gpu_index_load")
    .additional_info("draw_view")
    .additional_info("draw_modelmat_new")
    .additional_info("draw_resource_handle_new");

GPU_SHADER_CREATE_INFO(workbench_shadow_visibility_compute_common)
    .local_group_size(DRW_VISIBILITY_GROUP_SIZE)
    .define("DRW_VIEW_LEN", "64")
    .storage_buf(0, Qualifier::READ, "ObjectBounds", "bounds_buf[]")
    .uniform_buf(2, "ExtrudedFrustum", "extruded_frustum")
    .push_constant(Type::INT, "resource_len")
    .push_constant(Type::INT, "view_len")
    .push_constant(Type::INT, "visibility_word_per_draw")
    .push_constant(Type::BOOL, "force_fail_method")
    .push_constant(Type::VEC3, "shadow_direction")
    .typedef_source("workbench_shader_shared.h")
    .compute_source("workbench_shadow_visibility_comp.glsl")
    .additional_info("draw_view", "draw_view_culling");

GPU_SHADER_CREATE_INFO(workbench_shadow_visibility_compute_dynamic_pass_type)
    .additional_info("workbench_shadow_visibility_compute_common")
    .define("DYNAMIC_PASS_SELECTION")
    .storage_buf(1, Qualifier::READ_WRITE, "uint", "pass_visibility_buf[]")
    .storage_buf(2, Qualifier::READ_WRITE, "uint", "fail_visibility_buf[]")
    .do_static_compilation(true);

GPU_SHADER_CREATE_INFO(workbench_shadow_visibility_compute_static_pass_type)
    .additional_info("workbench_shadow_visibility_compute_common")
    .storage_buf(1, Qualifier::READ_WRITE, "uint", "visibility_buf[]")
    .do_static_compilation(true);

/** \} */

/* -------------------------------------------------------------------- */
/** \name Debug Type
 * \{ */

GPU_SHADER_CREATE_INFO(workbench_shadow_no_debug)
    .fragment_source("gpu_shader_depth_only_frag.glsl");

GPU_SHADER_CREATE_INFO(workbench_shadow_debug)
    .fragment_out(0, Type::VEC4, "out_debug_color")
    .fragment_source("workbench_shadow_debug_frag.glsl");

/** \} */

/* -------------------------------------------------------------------- */
/** \name Variations Declaration
 * \{ */

#define WORKBENCH_SHADOW_VARIATIONS(common, prefix, suffix, ...) \
  GPU_SHADER_CREATE_INFO(prefix##_pass_manifold_no_caps##suffix) \
      .define("SHADOW_PASS") \
      .vertex_source("workbench_shadow_vert.glsl") \
      .additional_info(common, __VA_ARGS__) \
      .do_static_compilation(true); \
  GPU_SHADER_CREATE_INFO(prefix##_pass_no_manifold_no_caps##suffix) \
      .define("SHADOW_PASS") \
      .define("DOUBLE_MANIFOLD") \
      .vertex_source("workbench_shadow_vert.glsl") \
      .additional_info(common, __VA_ARGS__) \
      .do_static_compilation(true); \
  GPU_SHADER_CREATE_INFO(prefix##_fail_manifold_caps##suffix) \
      .define("SHADOW_FAIL") \
      .vertex_source("workbench_shadow_caps_vert.glsl") \
      .additional_info(common, __VA_ARGS__) \
      .do_static_compilation(true); \
  GPU_SHADER_CREATE_INFO(prefix##_fail_manifold_no_caps##suffix) \
      .define("SHADOW_FAIL") \
      .vertex_source("workbench_shadow_vert.glsl") \
      .additional_info(common, __VA_ARGS__) \
      .do_static_compilation(true); \
  GPU_SHADER_CREATE_INFO(prefix##_fail_no_manifold_caps##suffix) \
      .define("SHADOW_FAIL") \
      .define("DOUBLE_MANIFOLD") \
      .vertex_source("workbench_shadow_caps_vert.glsl") \
      .additional_info(common, __VA_ARGS__) \
      .do_static_compilation(true); \
  GPU_SHADER_CREATE_INFO(prefix##_fail_no_manifold_no_caps##suffix) \
      .define("SHADOW_FAIL") \
      .define("DOUBLE_MANIFOLD") \
      .vertex_source("workbench_shadow_vert.glsl") \
      .additional_info(common, __VA_ARGS__) \
      .do_static_compilation(true);

WORKBENCH_SHADOW_VARIATIONS("workbench_shadow_common",
                            workbench_shadow,
                            ,
                            "workbench_shadow_no_debug")

WORKBENCH_SHADOW_VARIATIONS("workbench_shadow_common",
                            workbench_shadow,
                            _debug,
                            "workbench_shadow_debug")

/** \} */
