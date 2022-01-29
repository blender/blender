
#include "gpu_shader_create_info.hh"

/* -------------------------------------------------------------------- */
/** \name TAA
 * \{ */

GPU_SHADER_CREATE_INFO(workbench_taa)
    .sampler(0, ImageType::FLOAT_2D, "colorBuffer")
    .push_constant(Type::FLOAT, "samplesWeights", 9)
    .fragment_out(0, Type::VEC4, "fragColor")
    .fragment_source("workbench_effect_taa_frag.glsl")
    .additional_info("draw_fullscreen")
    .do_static_compilation(true);

/** \} */

/* -------------------------------------------------------------------- */
/** \name SMAA
 * \{ */

GPU_SHADER_INTERFACE_INFO(workbench_smaa_iface, "")
    .smooth(Type::VEC2, "uvs")
    .smooth(Type::VEC2, "pixcoord")
    .smooth(Type::VEC4, "offset[3]");

GPU_SHADER_CREATE_INFO(workbench_smaa)
    .define("SMAA_GLSL_3")
    .define("SMAA_RT_METRICS", "viewportMetrics")
    .define("SMAA_PRESET_HIGH")
    .define("SMAA_LUMA_WEIGHT", "float4(1.0, 1.0, 1.0, 1.0)")
    .define("SMAA_NO_DISCARD")
    .vertex_out(workbench_smaa_iface)
    .push_constant(Type::VEC4, "viewportMetrics")
    .vertex_source("workbench_effect_smaa_vert.glsl")
    .fragment_source("workbench_effect_smaa_frag.glsl");

GPU_SHADER_CREATE_INFO(workbench_smaa_stage_0)
    .define("SMAA_STAGE", "0")
    .sampler(0, ImageType::FLOAT_2D, "colorTex")
    .fragment_out(0, Type::VEC2, "out_edges")
    .additional_info("workbench_smaa")
    .do_static_compilation(true);

GPU_SHADER_CREATE_INFO(workbench_smaa_stage_1)
    .define("SMAA_STAGE", "1")
    .sampler(0, ImageType::FLOAT_2D, "edgesTex")
    .sampler(1, ImageType::FLOAT_2D, "areaTex")
    .sampler(2, ImageType::FLOAT_2D, "searchTex")
    .fragment_out(0, Type::VEC4, "out_weights")
    .additional_info("workbench_smaa")
    .do_static_compilation(true);

GPU_SHADER_CREATE_INFO(workbench_smaa_stage_2)
    .define("SMAA_STAGE", "2")
    .sampler(0, ImageType::FLOAT_2D, "colorTex")
    .sampler(1, ImageType::FLOAT_2D, "blendTex")
    .push_constant(Type::FLOAT, "mixFactor")
    .push_constant(Type::FLOAT, "taaAccumulatedWeight")
    .fragment_out(0, Type::VEC4, "out_color")
    .additional_info("workbench_smaa")
    .do_static_compilation(true);

/** \} */
