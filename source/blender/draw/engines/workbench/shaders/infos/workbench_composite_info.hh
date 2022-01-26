
#include "gpu_shader_create_info.hh"

/* -------------------------------------------------------------------- */
/** \name Base Composite
 * \{ */

GPU_SHADER_CREATE_INFO(workbench_composite)
    .sampler(0, ImageType::FLOAT_2D, "normalBuffer", Frequency::PASS)
    .sampler(1, ImageType::FLOAT_2D, "materialBuffer", Frequency::PASS)
    .uniform_buf(4, "WorldData", "world_data", Frequency::PASS)
    .push_constant(0, Type::BOOL, "forceShadowing")
    .fragment_out(0, Type::VEC4, "fragColor")
    .typedef_source("workbench_shader_shared.h")
    .fragment_source("workbench_composite_frag.glsl")
    .additional_info("draw_fullscreen");

/** \} */

/* -------------------------------------------------------------------- */
/** \name Lighting Type
 * \{ */

GPU_SHADER_CREATE_INFO(workbench_composite_studio)
    .define("V3D_LIGHTING_STUDIO")
    .additional_info("workbench_composite")
    .do_static_compilation(true);

GPU_SHADER_CREATE_INFO(workbench_composite_matcap)
    .define("V3D_LIGHTING_MATCAP")
    .sampler(2, ImageType::FLOAT_2D, "matcap_diffuse_tx", Frequency::PASS)
    .sampler(3, ImageType::FLOAT_2D, "matcap_specular_tx", Frequency::PASS)
    .additional_info("workbench_composite")
    .do_static_compilation(true);

GPU_SHADER_CREATE_INFO(workbench_composite_flat)
    .define("V3D_LIGHTING_FLAT")
    .additional_info("workbench_composite")
    .do_static_compilation(true);

/** \} */
