#include "gpu_shader_create_info.hh"

GPU_SHADER_INTERFACE_INFO(image_engine_iface, "")
    .smooth(Type::VEC2, "uv_screen")
    .smooth(Type::VEC2, "uv_image");

GPU_SHADER_CREATE_INFO(image_engine_shader)
    .vertex_in(0, Type::VEC2, "pos")
    .vertex_in(1, Type::VEC2, "uv")
    .vertex_out(image_engine_iface)
    .fragment_out(0, Type::VEC4, "fragColor")
    .push_constant(Type::VEC4, "shuffle")
    .push_constant(Type::VEC2, "maxUv")
    .push_constant(Type::VEC2, "farNearDistances")
    .push_constant(Type::INT, "drawFlags")
    .push_constant(Type::BOOL, "imgPremultiplied")
    .sampler(0, ImageType::FLOAT_2D, "imageTexture")
    .vertex_source("image_engine_vert.glsl")
    .fragment_source("image_engine_frag.glsl")
    .additional_info("draw_modelmat")
    .do_static_compilation(true);
