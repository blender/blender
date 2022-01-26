
#include "gpu_shader_create_info.hh"

/* -------------------------------------------------------------------- */
/** \name Common
 * \{ */

GPU_SHADER_INTERFACE_INFO(workbench_shadow_iface, "vData")
    .smooth(Type::VEC3, "pos")
    .smooth(Type::VEC4, "frontPosition")
    .smooth(Type::VEC4, "backPosition");

GPU_SHADER_CREATE_INFO(workbench_shadow_common)
    .vertex_in(0, Type::VEC3, "pos")
    .vertex_out(workbench_shadow_iface)
    .push_constant(0, Type::FLOAT, "lightDistance")
    .push_constant(1, Type::VEC3, "lightDirection")
    .vertex_source("workbench_shadow_vert.glsl")
    .additional_info("draw_mesh");

/** \} */

/* -------------------------------------------------------------------- */
/** \name Manifold Type
 * \{ */

GPU_SHADER_CREATE_INFO(workbench_shadow_manifold)
    .geometry_layout(PrimitiveIn::LINES_ADJACENCY, PrimitiveOut::TRIANGLE_STRIP, 4, 1)
    .geometry_source("workbench_shadow_geom.glsl");

GPU_SHADER_CREATE_INFO(workbench_shadow_no_manifold)
    .geometry_layout(PrimitiveIn::LINES_ADJACENCY, PrimitiveOut::TRIANGLE_STRIP, 4, 2)
    .geometry_source("workbench_shadow_geom.glsl");

/** \} */

/* -------------------------------------------------------------------- */
/** \name Caps Type
 * \{ */

GPU_SHADER_CREATE_INFO(workbench_shadow_caps)
    .geometry_layout(PrimitiveIn::TRIANGLES, PrimitiveOut::TRIANGLE_STRIP, 3, 2)
    .geometry_source("workbench_shadow_caps_geom.glsl");

/** \} */

/* -------------------------------------------------------------------- */
/** \name Debug Type
 * \{ */

GPU_SHADER_CREATE_INFO(workbench_shadow_no_debug)
    .fragment_source("gpu_shader_depth_only_frag.glsl");

GPU_SHADER_CREATE_INFO(workbench_shadow_debug)
    .fragment_out(0, Type::VEC4, "materialData")
    .fragment_out(1, Type::VEC4, "normalData")
    .fragment_out(2, Type::UINT, "objectId")
    .fragment_source("workbench_shadow_debug_frag.glsl");

/** \} */

/* -------------------------------------------------------------------- */
/** \name Variations Declaration
 * \{ */

#define WORKBENCH_SHADOW_VARIATIONS(suffix, ...) \
  GPU_SHADER_CREATE_INFO(workbench_shadow_pass_manifold_no_caps##suffix) \
      .define("SHADOW_PASS") \
      .additional_info("workbench_shadow_common", "workbench_shadow_manifold", __VA_ARGS__) \
      .do_static_compilation(true); \
  GPU_SHADER_CREATE_INFO(workbench_shadow_pass_no_manifold_no_caps##suffix) \
      .define("SHADOW_PASS") \
      .define("DOUBLE_MANIFOLD") \
      .additional_info("workbench_shadow_common", "workbench_shadow_no_manifold", __VA_ARGS__) \
      .do_static_compilation(true); \
  GPU_SHADER_CREATE_INFO(workbench_shadow_fail_manifold_caps##suffix) \
      .define("SHADOW_FAIL") \
      .additional_info("workbench_shadow_common", "workbench_shadow_caps", __VA_ARGS__) \
      .do_static_compilation(true); \
  GPU_SHADER_CREATE_INFO(workbench_shadow_fail_manifold_no_caps##suffix) \
      .define("SHADOW_FAIL") \
      .additional_info("workbench_shadow_common", "workbench_shadow_manifold", __VA_ARGS__) \
      .do_static_compilation(true); \
  GPU_SHADER_CREATE_INFO(workbench_shadow_fail_no_manifold_caps##suffix) \
      .define("SHADOW_FAIL") \
      .define("DOUBLE_MANIFOLD") \
      .additional_info("workbench_shadow_common", "workbench_shadow_caps", __VA_ARGS__) \
      .do_static_compilation(true); \
  GPU_SHADER_CREATE_INFO(workbench_shadow_fail_no_manifold_no_caps##suffix) \
      .define("SHADOW_FAIL") \
      .define("DOUBLE_MANIFOLD") \
      .additional_info("workbench_shadow_common", "workbench_shadow_no_manifold", __VA_ARGS__) \
      .do_static_compilation(true);

WORKBENCH_SHADOW_VARIATIONS(, "workbench_shadow_no_debug")
WORKBENCH_SHADOW_VARIATIONS(_debug, "workbench_shadow_debug")

/** \} */
