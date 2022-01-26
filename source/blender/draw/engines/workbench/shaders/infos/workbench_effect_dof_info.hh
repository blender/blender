
#include "gpu_shader_create_info.hh"

GPU_SHADER_CREATE_INFO(workbench_effect_dof)
    /* TODO(fclem): Split resources per stage. */
    .sampler(0, ImageType::FLOAT_2D, "inputCocTex")
    .sampler(1, ImageType::FLOAT_2D, "maxCocTilesTex")
    .sampler(2, ImageType::FLOAT_2D, "sceneColorTex")
    .sampler(3, ImageType::FLOAT_2D, "sceneDepthTex")
    .sampler(4, ImageType::FLOAT_2D, "backgroundTex")
    .sampler(5, ImageType::FLOAT_2D, "halfResColorTex")
    .sampler(6, ImageType::FLOAT_2D, "blurTex")
    .sampler(7, ImageType::FLOAT_2D, "noiseTex")
    .push_constant(0, Type::VEC2, "invertedViewportSize")
    .push_constant(1, Type::VEC2, "nearFar")
    .push_constant(2, Type::VEC3, "dofParams")
    .push_constant(3, Type::FLOAT, "noiseOffset")
    .fragment_source("workbench_effect_dof_frag.glsl")
    .additional_info("draw_fullscreen")
    .additional_info("draw_view");

GPU_SHADER_CREATE_INFO(workbench_effect_dof_prepare)
    .define("PREPARE")
    .fragment_out(0, Type::VEC4, "halfResColor")
    .fragment_out(1, Type::VEC2, "normalizedCoc")
    .additional_info("workbench_effect_dof")
    .do_static_compilation(true);

GPU_SHADER_CREATE_INFO(workbench_effect_dof_downsample)
    .define("DOWNSAMPLE")
    .fragment_out(0, Type::VEC4, "outColor")
    .fragment_out(1, Type::VEC2, "outCocs")
    .additional_info("workbench_effect_dof")
    .do_static_compilation(true);

GPU_SHADER_CREATE_INFO(workbench_effect_dof_blur1)
    .define("BLUR1")
    .define("NUM_SAMPLES", "49")
    .uniform_buf(1, "vec4", "samples[49]")
    .fragment_out(0, Type::VEC4, "blurColor")
    .additional_info("workbench_effect_dof")
    .do_static_compilation(true);

GPU_SHADER_CREATE_INFO(workbench_effect_dof_blur2)
    .define("BLUR2")
    .fragment_out(0, Type::VEC4, "finalColor")
    .additional_info("workbench_effect_dof")
    .do_static_compilation(true);

GPU_SHADER_CREATE_INFO(workbench_effect_dof_resolve)
    .define("RESOLVE")
    .fragment_out(0, Type::VEC4, "finalColorAdd", DualBlend::SRC_0)
    .fragment_out(0, Type::VEC4, "finalColorMul", DualBlend::SRC_1)
    .additional_info("workbench_effect_dof")
    .do_static_compilation(true);