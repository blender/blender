
#include "gpu_shader_create_info.hh"

/* -------------------------------------------------------------------- */
/** \name Volume shader base
 * \{ */

GPU_SHADER_CREATE_INFO(workbench_volume)
    .vertex_in(0, Type::VEC3, "pos")
    .fragment_out(0, Type::VEC4, "fragColor")
    .sampler(0, ImageType::DEPTH_2D, "depthBuffer")
    .sampler(1, ImageType::FLOAT_3D, "densityTexture")
    .push_constant(28, Type::INT, "samplesLen")
    .push_constant(29, Type::FLOAT, "noiseOfs")
    .push_constant(30, Type::FLOAT, "stepLength")
    .push_constant(31, Type::FLOAT, "densityScale")
    .vertex_source("workbench_volume_vert.glsl")
    .fragment_source("workbench_volume_frag.glsl")
    .additional_info("draw_object_infos");

/** \} */

/* -------------------------------------------------------------------- */
/** \name Smoke variation
 * \{ */

GPU_SHADER_CREATE_INFO(workbench_volume_smoke)
    .define("VOLUME_SMOKE")
    .sampler(2, ImageType::FLOAT_3D, "flameTexture")
    .sampler(3, ImageType::FLOAT_1D, "flameColorTexture")
    .additional_info("draw_mesh", "draw_resource_id_varying");

GPU_SHADER_CREATE_INFO(workbench_volume_object)
    .define("VOLUME_OBJECT")
    .push_constant(0, Type::MAT4, "volumeTextureToObject")
    /* FIXME(fclem): This overflow the push_constant limit. */
    .push_constant(16, Type::MAT4, "volumeObjectToTexture")
    .additional_info("draw_volume", "draw_resource_id_varying");

/** \} */

/* -------------------------------------------------------------------- */
/** \name Color Band variation
 * \{ */

GPU_SHADER_CREATE_INFO(workbench_volume_coba)
    .define("USE_COBA")
    .sampler(4, ImageType::UINT_3D, "flagTexture")
    .sampler(5, ImageType::FLOAT_1D, "transferTexture")
    .push_constant(18, Type::BOOL, "showPhi")
    .push_constant(19, Type::BOOL, "showFlags")
    .push_constant(20, Type::BOOL, "showPressure")
    .push_constant(21, Type::FLOAT, "gridScale");

GPU_SHADER_CREATE_INFO(workbench_volume_no_coba)
    .sampler(4, ImageType::FLOAT_3D, "shadowTexture")
    .sampler(5, ImageType::UINT_2D, "transferTexture")
    .push_constant(18, Type::VEC3, "activeColor");

/** \} */

/* -------------------------------------------------------------------- */
/** \name Sampling variation
 * \{ */

GPU_SHADER_CREATE_INFO(workbench_volume_linear).define("USE_TRILINEAR");
GPU_SHADER_CREATE_INFO(workbench_volume_cubic).define("USE_TRICUBIC");
GPU_SHADER_CREATE_INFO(workbench_volume_closest).define("USE_CLOSEST");

/** \} */

/* -------------------------------------------------------------------- */
/** \name Slice variation
 * \{ */

GPU_SHADER_INTERFACE_INFO(workbench_volume_iface, "").smooth(Type::VEC3, "localPos");

GPU_SHADER_CREATE_INFO(workbench_volume_slice)
    .define("VOLUME_SLICE")
    .vertex_in(1, Type::VEC3, "uvs")
    .vertex_out(workbench_volume_iface)
    .push_constant(32, Type::INT, "sliceAxis") /* -1 is no slice. */
    .push_constant(33, Type::FLOAT, "slicePosition");

/** \} */

/* -------------------------------------------------------------------- */
/** \name Variations Declaration
 * \{ */

#define WORKBENCH_VOLUME_SLICE_VARIATIONS(prefix, ...) \
  GPU_SHADER_CREATE_INFO(prefix##_slice) \
      .additional_info("workbench_volume_slice", __VA_ARGS__) \
      .do_static_compilation(true); \
  GPU_SHADER_CREATE_INFO(prefix##_no_slice) \
      .additional_info(__VA_ARGS__) \
      .do_static_compilation(true);

#define WORKBENCH_VOLUME_COBA_VARIATIONS(prefix, ...) \
  WORKBENCH_VOLUME_SLICE_VARIATIONS(prefix##_coba, "workbench_volume_coba", __VA_ARGS__) \
  WORKBENCH_VOLUME_SLICE_VARIATIONS(prefix##_no_coba, "workbench_volume_no_coba", __VA_ARGS__)

#define WORKBENCH_VOLUME_INTERP_VARIATIONS(prefix, ...) \
  WORKBENCH_VOLUME_COBA_VARIATIONS(prefix##_linear, "workbench_volume_linear", __VA_ARGS__) \
  WORKBENCH_VOLUME_COBA_VARIATIONS(prefix##_cubic, "workbench_volume_cubic", __VA_ARGS__) \
  WORKBENCH_VOLUME_COBA_VARIATIONS(prefix##_closest, "workbench_volume_closest", __VA_ARGS__)

#define WORKBENCH_VOLUME_SMOKE_VARIATIONS(prefix, ...) \
  WORKBENCH_VOLUME_INTERP_VARIATIONS(prefix##_smoke, "workbench_volume_smoke", __VA_ARGS__) \
  WORKBENCH_VOLUME_INTERP_VARIATIONS(prefix##_object, "workbench_volume_object", __VA_ARGS__)

WORKBENCH_VOLUME_SMOKE_VARIATIONS(workbench_volume, "workbench_volume")

/** \} */
