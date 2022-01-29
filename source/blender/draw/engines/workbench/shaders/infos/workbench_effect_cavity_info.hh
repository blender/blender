
#include "gpu_shader_create_info.hh"

GPU_SHADER_CREATE_INFO(workbench_effect_cavity_common)
    .fragment_out(0, Type::VEC4, "fragColor")
    .sampler(0, ImageType::DEPTH_2D, "depthBuffer")
    .sampler(1, ImageType::FLOAT_2D, "normalBuffer")
    .sampler(2, ImageType::UINT_2D, "objectIdBuffer")
    .sampler(3, ImageType::FLOAT_2D, "cavityJitter")
    .uniform_buf(3, "vec4", "samples_coords[512]")
    .uniform_buf(4, "WorldData", "world_data", Frequency::PASS)
    .typedef_source("workbench_shader_shared.h")
    .fragment_source("workbench_effect_cavity_frag.glsl")
    .additional_info("draw_fullscreen")
    .additional_info("draw_view");

GPU_SHADER_CREATE_INFO(workbench_effect_cavity)
    .do_static_compilation(true)
    .define("USE_CAVITY")
    .additional_info("workbench_effect_cavity_common");

GPU_SHADER_CREATE_INFO(workbench_effect_curvature)
    .do_static_compilation(true)
    .define("USE_CURVATURE")
    .additional_info("workbench_effect_cavity_common");

GPU_SHADER_CREATE_INFO(workbench_effect_cavity_curvature)
    .do_static_compilation(true)
    .define("USE_CAVITY")
    .define("USE_CURVATURE")
    .additional_info("workbench_effect_cavity_common");
