
#include "gpu_shader_create_info.hh"

GPU_SHADER_CREATE_INFO(workbench_merge_infront)
    .fragment_out(0, Type::VEC4, "fragColor")
    .sampler(0, ImageType::DEPTH_2D, "depthBuffer")
    .fragment_source("workbench_merge_infront_frag.glsl")
    .additional_info("draw_fullscreen")
    .do_static_compilation(true);
