#include "gpu_interface_info.hh"
#include "gpu_shader_create_info.hh"

GPU_SHADER_CREATE_INFO(gpu_shader_2D_image_multi_rect_color)
    .vertex_in(0, Type::VEC2, "pos")
    .vertex_out(flat_color_smooth_tex_coord_interp_iface)
    .fragment_out(0, Type::VEC4, "fragColor")
    .uniform_buf(0, "MultiRectCallData", "multi_rect_data")
    .sampler(0, ImageType::FLOAT_2D, "image")
    .typedef_source("GPU_shader_shared.h")
    .vertex_source("gpu_shader_2D_image_multi_rect_vert.glsl")
    .fragment_source("gpu_shader_image_varying_color_frag.glsl")
    .do_static_compilation(true);
