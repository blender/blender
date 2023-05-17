/* SPDX-License-Identifier: GPL-2.0-or-later
 * Copyright 2016 Blender Foundation. */

/** \file
 * \ingroup draw_engine
 */

#include "DRW_render.h"

#include "BKE_lib_id.h"
#include "BKE_node.hh"

#include "BLI_dynstr.h"
#include "BLI_string_utils.h"

#include "DNA_world_types.h"

#include "MEM_guardedalloc.h"

#include "GPU_capabilities.h"
#include "GPU_context.h"
#include "GPU_material.h"
#include "GPU_shader.h"

#include "NOD_shader.h"

#include "eevee_engine.h"
#include "eevee_private.h"

static struct {
  /* Lookdev */
  struct GPUShader *studiolight_probe_sh;
  struct GPUShader *studiolight_background_sh;

  /* Probes */
  struct GPUShader *probe_grid_display_sh;
  struct GPUShader *probe_cube_display_sh;
  struct GPUShader *probe_planar_display_sh;
  struct GPUShader *probe_filter_glossy_sh;
  struct GPUShader *probe_filter_diffuse_sh;
  struct GPUShader *probe_filter_visibility_sh;
  struct GPUShader *probe_grid_fill_sh;
  struct GPUShader *probe_planar_downsample_sh;

  /* Velocity Resolve */
  struct GPUShader *velocity_resolve_sh;

  /* Temporal Anti Aliasing */
  struct GPUShader *taa_resolve_sh;
  struct GPUShader *taa_resolve_reproject_sh;

  /* Bloom */
  struct GPUShader *bloom_blit_sh[2];
  struct GPUShader *bloom_downsample_sh[2];
  struct GPUShader *bloom_upsample_sh[2];
  struct GPUShader *bloom_resolve_sh[2];

  /* Depth Of Field */
  struct GPUShader *dof_bokeh_sh;
  struct GPUShader *dof_setup_sh;
  struct GPUShader *dof_flatten_tiles_sh;
  struct GPUShader *dof_dilate_tiles_sh[2];
  struct GPUShader *dof_downsample_sh;
  struct GPUShader *dof_reduce_sh[2];
  struct GPUShader *dof_gather_sh[DOF_GATHER_MAX_PASS][2];
  struct GPUShader *dof_filter_sh;
  struct GPUShader *dof_scatter_sh[2][2];
  struct GPUShader *dof_resolve_sh[2][2];

  /* General purpose Shaders. */
  struct GPUShader *lookdev_background;
  struct GPUShader *update_noise_sh;

  /* Down-sample Depth */
  struct GPUShader *minz_downlevel_sh;
  struct GPUShader *maxz_downlevel_sh;
  struct GPUShader *minz_downdepth_sh;
  struct GPUShader *maxz_downdepth_sh;
  struct GPUShader *minz_downdepth_layer_sh;
  struct GPUShader *maxz_downdepth_layer_sh;
  struct GPUShader *maxz_copydepth_layer_sh;
  struct GPUShader *minz_copydepth_sh;
  struct GPUShader *maxz_copydepth_sh;

  /* Simple Down-sample. */
  struct GPUShader *color_copy_sh;
  struct GPUShader *downsample_sh;
  struct GPUShader *downsample_cube_sh;

  /* Mist */
  struct GPUShader *mist_sh;

  /* Motion Blur */
  struct GPUShader *motion_blur_sh;
  struct GPUShader *motion_blur_object_sh;
  struct GPUShader *motion_blur_hair_sh;
  struct GPUShader *velocity_tiles_sh;
  struct GPUShader *velocity_tiles_expand_sh;

  /* Ground Truth Ambient Occlusion */
  struct GPUShader *gtao_sh;
  struct GPUShader *gtao_layer_sh;
  struct GPUShader *gtao_debug_sh;

  /* GGX LUT */
  struct GPUShader *ggx_lut_sh;
  struct GPUShader *ggx_refraction_lut_sh;

  /* Render Passes */
  struct GPUShader *rpass_accumulate_sh;
  struct GPUShader *postprocess_sh;
  struct GPUShader *cryptomatte_sh[2];

  /* Screen Space Reflection */
  struct GPUShader *reflection_trace;
  struct GPUShader *reflection_resolve;
  struct GPUShader *reflection_resolve_probe;
  struct GPUShader *reflection_resolve_raytrace;

  /* Shadows */
  struct GPUShader *shadow_sh;
  struct GPUShader *shadow_accum_sh;

  /* Subsurface */
  struct GPUShader *sss_sh[3];

  /* Volume */
  struct GPUShader *volumetric_clear_sh;
  struct GPUShader *scatter_sh;
  struct GPUShader *scatter_with_lights_sh;
  struct GPUShader *volumetric_integration_sh;
  struct GPUShader *volumetric_resolve_sh[2];
  struct GPUShader *volumetric_accum_sh;

  /* Shader strings */
  char *surface_lit_frag;
  char *surface_prepass_frag;
  char *surface_geom_barycentric;

  DRWShaderLibrary *lib;

  /* LookDev Materials */
  Material *glossy_mat;
  Material *diffuse_mat;

  Material *error_mat;

  World *default_world;

  /* Default Material */
  struct {
    bNodeTree *ntree;
    bNodeSocketValueRGBA *color_socket;
    bNodeSocketValueFloat *metallic_socket;
    bNodeSocketValueFloat *roughness_socket;
    bNodeSocketValueFloat *specular_socket;
  } surface;

  struct {
    bNodeTree *ntree;
    bNodeSocketValueRGBA *color_socket;
  } world;
} e_data = {nullptr}; /* Engine data */

extern "C" char datatoc_engine_eevee_legacy_shared_h[];
extern "C" char datatoc_common_hair_lib_glsl[];
extern "C" char datatoc_common_math_lib_glsl[];
extern "C" char datatoc_common_math_geom_lib_glsl[];
extern "C" char datatoc_common_view_lib_glsl[];
extern "C" char datatoc_gpu_shader_codegen_lib_glsl[];

extern "C" char datatoc_ambient_occlusion_lib_glsl[];
extern "C" char datatoc_bsdf_common_lib_glsl[];
extern "C" char datatoc_bsdf_sampling_lib_glsl[];
extern "C" char datatoc_closure_type_lib_glsl[];
extern "C" char datatoc_closure_eval_volume_lib_glsl[];
extern "C" char datatoc_common_uniforms_lib_glsl[];
extern "C" char datatoc_common_utiltex_lib_glsl[];
extern "C" char datatoc_cubemap_lib_glsl[];
extern "C" char datatoc_effect_dof_lib_glsl[];
extern "C" char datatoc_effect_reflection_lib_glsl[];
extern "C" char datatoc_irradiance_lib_glsl[];
extern "C" char datatoc_lightprobe_lib_glsl[];
extern "C" char datatoc_lights_lib_glsl[];
extern "C" char datatoc_closure_eval_lib_glsl[];
extern "C" char datatoc_closure_eval_surface_lib_glsl[];
extern "C" char datatoc_closure_eval_diffuse_lib_glsl[];
extern "C" char datatoc_closure_eval_glossy_lib_glsl[];
extern "C" char datatoc_closure_eval_refraction_lib_glsl[];
extern "C" char datatoc_closure_eval_translucent_lib_glsl[];
extern "C" char datatoc_ltc_lib_glsl[];
extern "C" char datatoc_octahedron_lib_glsl[];
extern "C" char datatoc_prepass_frag_glsl[];
extern "C" char datatoc_random_lib_glsl[];
extern "C" char datatoc_raytrace_lib_glsl[];
extern "C" char datatoc_renderpass_lib_glsl[];
extern "C" char datatoc_ssr_lib_glsl[];
extern "C" char datatoc_surface_frag_glsl[];
extern "C" char datatoc_surface_geom_glsl[];
extern "C" char datatoc_surface_lib_glsl[];
extern "C" char datatoc_surface_vert_glsl[];
extern "C" char datatoc_volumetric_frag_glsl[];
extern "C" char datatoc_volumetric_geom_glsl[];
extern "C" char datatoc_volumetric_lib_glsl[];
extern "C" char datatoc_volumetric_vert_glsl[];
extern "C" char datatoc_world_vert_glsl[];

/* *********** FUNCTIONS *********** */

static void eevee_shader_library_ensure()
{
  if (e_data.lib == nullptr) {
    e_data.lib = DRW_shader_library_create();
    /* NOTE: These need to be ordered by dependencies. */
    DRW_SHADER_LIB_ADD_SHARED(e_data.lib, engine_eevee_legacy_shared);
    DRW_SHADER_LIB_ADD(e_data.lib, common_math_lib);
    DRW_SHADER_LIB_ADD(e_data.lib, common_math_geom_lib);
    DRW_SHADER_LIB_ADD(e_data.lib, common_hair_lib);
    DRW_SHADER_LIB_ADD(e_data.lib, common_view_lib);
    DRW_SHADER_LIB_ADD(e_data.lib, common_uniforms_lib);
    DRW_SHADER_LIB_ADD(e_data.lib, gpu_shader_codegen_lib);
    DRW_SHADER_LIB_ADD(e_data.lib, random_lib);
    DRW_SHADER_LIB_ADD(e_data.lib, renderpass_lib);
    DRW_SHADER_LIB_ADD(e_data.lib, bsdf_common_lib);
    DRW_SHADER_LIB_ADD(e_data.lib, common_utiltex_lib);
    DRW_SHADER_LIB_ADD(e_data.lib, bsdf_sampling_lib);
    DRW_SHADER_LIB_ADD(e_data.lib, cubemap_lib);
    DRW_SHADER_LIB_ADD(e_data.lib, raytrace_lib);
    DRW_SHADER_LIB_ADD(e_data.lib, ambient_occlusion_lib);
    DRW_SHADER_LIB_ADD(e_data.lib, octahedron_lib);
    DRW_SHADER_LIB_ADD(e_data.lib, irradiance_lib);
    DRW_SHADER_LIB_ADD(e_data.lib, lightprobe_lib);
    DRW_SHADER_LIB_ADD(e_data.lib, ltc_lib);
    DRW_SHADER_LIB_ADD(e_data.lib, lights_lib);
    DRW_SHADER_LIB_ADD(e_data.lib, surface_lib);
    DRW_SHADER_LIB_ADD(e_data.lib, volumetric_lib);
    DRW_SHADER_LIB_ADD(e_data.lib, ssr_lib);
    DRW_SHADER_LIB_ADD(e_data.lib, effect_dof_lib);
    DRW_SHADER_LIB_ADD(e_data.lib, effect_reflection_lib);
    DRW_SHADER_LIB_ADD(e_data.lib, closure_type_lib);
    DRW_SHADER_LIB_ADD(e_data.lib, closure_eval_lib);
    DRW_SHADER_LIB_ADD(e_data.lib, closure_eval_diffuse_lib);
    DRW_SHADER_LIB_ADD(e_data.lib, closure_eval_glossy_lib);
    DRW_SHADER_LIB_ADD(e_data.lib, closure_eval_translucent_lib);
    DRW_SHADER_LIB_ADD(e_data.lib, closure_eval_refraction_lib);
    DRW_SHADER_LIB_ADD(e_data.lib, closure_eval_surface_lib);
    DRW_SHADER_LIB_ADD(e_data.lib, closure_eval_volume_lib);
    DRW_SHADER_LIB_ADD(e_data.lib, surface_vert);

    e_data.surface_lit_frag = DRW_shader_library_create_shader_string(e_data.lib,
                                                                      datatoc_surface_frag_glsl);

    e_data.surface_prepass_frag = DRW_shader_library_create_shader_string(
        e_data.lib, datatoc_prepass_frag_glsl);

    e_data.surface_geom_barycentric = DRW_shader_library_create_shader_string(
        e_data.lib, datatoc_surface_geom_glsl);
  }
}

void EEVEE_shaders_material_shaders_init(void)
{
  eevee_shader_library_ensure();
}

DRWShaderLibrary *EEVEE_shader_lib_get(void)
{
  eevee_shader_library_ensure();
  return e_data.lib;
}

GPUShader *EEVEE_shaders_probe_filter_glossy_sh_get(void)
{
  if (e_data.probe_filter_glossy_sh == nullptr) {
    e_data.probe_filter_glossy_sh = DRW_shader_create_from_info_name(
        "eevee_legacy_probe_filter_glossy");
  }
  return e_data.probe_filter_glossy_sh;
}

GPUShader *EEVEE_shaders_probe_filter_diffuse_sh_get(void)
{
  if (e_data.probe_filter_diffuse_sh == nullptr) {
    const char *create_info_name =
#if defined(IRRADIANCE_SH_L2)
        "eevee_legacy_probe_filter_diffuse_sh_l2";
#elif defined(IRRADIANCE_HL2)
        "eevee_legacy_probe_filter_diffuse_hl2";
#else
        nullptr;
    /* Should not reach this case. Either mode above should be defined. */
    BLI_assert_unreachable();
#endif
    e_data.probe_filter_diffuse_sh = DRW_shader_create_from_info_name(create_info_name);
  }
  return e_data.probe_filter_diffuse_sh;
}

GPUShader *EEVEE_shaders_probe_filter_visibility_sh_get(void)
{
  if (e_data.probe_filter_visibility_sh == nullptr) {
    e_data.probe_filter_visibility_sh = DRW_shader_create_from_info_name(
        "eevee_legacy_probe_filter_visiblity");
  }
  return e_data.probe_filter_visibility_sh;
}

GPUShader *EEVEE_shaders_probe_grid_fill_sh_get(void)
{
  if (e_data.probe_grid_fill_sh == nullptr) {
    const char *create_info_name =
#if defined(IRRADIANCE_SH_L2)
        "eevee_legacy_probe_grid_fill_sh_l2";
#elif defined(IRRADIANCE_HL2)
        "eevee_legacy_probe_grid_fill_hl2";
#else
        nullptr;
    /* Should not reach this case. `data_size` will not be defined otherwise. */
    BLI_assert_unreachable();
#endif
    e_data.probe_grid_fill_sh = DRW_shader_create_from_info_name(create_info_name);
  }
  return e_data.probe_grid_fill_sh;
}

GPUShader *EEVEE_shaders_probe_planar_downsample_sh_get(void)
{
  if (e_data.probe_planar_downsample_sh == nullptr) {
    e_data.probe_planar_downsample_sh = DRW_shader_create_from_info_name(
        "eevee_legacy_lightprobe_planar_downsample");
  }
  return e_data.probe_planar_downsample_sh;
}

GPUShader *EEVEE_shaders_studiolight_probe_sh_get(void)
{
  if (e_data.studiolight_probe_sh == nullptr) {
    e_data.studiolight_probe_sh = DRW_shader_create_from_info_name(
        "eevee_legacy_studiolight_probe");
  }
  return e_data.studiolight_probe_sh;
}

GPUShader *EEVEE_shaders_studiolight_background_sh_get(void)
{
  if (e_data.studiolight_background_sh == nullptr) {
    e_data.studiolight_background_sh = DRW_shader_create_from_info_name(
        "eevee_legacy_studiolight_background");
  }
  return e_data.studiolight_background_sh;
}

GPUShader *EEVEE_shaders_probe_cube_display_sh_get(void)
{
  if (e_data.probe_cube_display_sh == nullptr) {
    e_data.probe_cube_display_sh = DRW_shader_create_from_info_name(
        "eevee_legacy_lightprobe_cube_display");
  }
  return e_data.probe_cube_display_sh;
}

GPUShader *EEVEE_shaders_probe_grid_display_sh_get(void)
{
  if (e_data.probe_grid_display_sh == nullptr) {
    const char *probe_display_grid_info_name = nullptr;
#if defined(IRRADIANCE_SH_L2)
    probe_display_grid_info_name = "eevee_legacy_lightprobe_grid_display_common_sh_l2";
#elif defined(IRRADIANCE_HL2)
    probe_display_grid_info_name = "eevee_legacy_lightprobe_grid_display_common_hl2";
#endif
    BLI_assert(probe_display_grid_info_name != nullptr);

    e_data.probe_grid_display_sh = DRW_shader_create_from_info_name(probe_display_grid_info_name);
  }
  return e_data.probe_grid_display_sh;
}

GPUShader *EEVEE_shaders_probe_planar_display_sh_get(void)
{
  if (e_data.probe_planar_display_sh == nullptr) {
    e_data.probe_planar_display_sh = DRW_shader_create_from_info_name(
        "eevee_legacy_probe_planar_display");
  }
  return e_data.probe_planar_display_sh;
}

/* -------------------------------------------------------------------- */
/** \name Down-sampling
 * \{ */

GPUShader *EEVEE_shaders_effect_color_copy_sh_get(void)
{
  if (e_data.color_copy_sh == nullptr) {
    e_data.color_copy_sh = DRW_shader_create_from_info_name("eevee_legacy_color_copy");
  }
  return e_data.color_copy_sh;
}

GPUShader *EEVEE_shaders_effect_downsample_sh_get(void)
{
  if (e_data.downsample_sh == nullptr) {
    e_data.downsample_sh = DRW_shader_create_from_info_name("eevee_legacy_downsample");
  }
  return e_data.downsample_sh;
}

GPUShader *EEVEE_shaders_effect_downsample_cube_sh_get(void)
{
  if (e_data.downsample_cube_sh == nullptr) {
    e_data.downsample_cube_sh = DRW_shader_create_from_info_name(
        "eevee_legacy_effect_downsample_cube");
  }
  return e_data.downsample_cube_sh;
}

GPUShader *EEVEE_shaders_effect_minz_downlevel_sh_get(void)
{
  if (e_data.minz_downlevel_sh == nullptr) {
    e_data.minz_downlevel_sh = DRW_shader_create_from_info_name("eevee_legacy_minz_downlevel");
  }
  return e_data.minz_downlevel_sh;
}

GPUShader *EEVEE_shaders_effect_maxz_downlevel_sh_get(void)
{
  if (e_data.maxz_downlevel_sh == nullptr) {
    e_data.maxz_downlevel_sh = DRW_shader_create_from_info_name("eevee_legacy_maxz_downlevel");
  }
  return e_data.maxz_downlevel_sh;
}

GPUShader *EEVEE_shaders_effect_minz_downdepth_sh_get(void)
{
  if (e_data.minz_downdepth_sh == nullptr) {
    e_data.minz_downdepth_sh = DRW_shader_create_from_info_name("eevee_legacy_minz_downdepth");
  }
  return e_data.minz_downdepth_sh;
}

GPUShader *EEVEE_shaders_effect_maxz_downdepth_sh_get(void)
{
  if (e_data.maxz_downdepth_sh == nullptr) {
    e_data.maxz_downdepth_sh = DRW_shader_create_from_info_name("eevee_legacy_maxz_downdepth");
  }
  return e_data.maxz_downdepth_sh;
}

GPUShader *EEVEE_shaders_effect_minz_downdepth_layer_sh_get(void)
{
  if (e_data.minz_downdepth_layer_sh == nullptr) {
    e_data.minz_downdepth_layer_sh = DRW_shader_create_from_info_name(
        "eevee_legacy_minz_downdepth_layer");
  }
  return e_data.minz_downdepth_layer_sh;
}

GPUShader *EEVEE_shaders_effect_maxz_downdepth_layer_sh_get(void)
{
  if (e_data.maxz_downdepth_layer_sh == nullptr) {
    e_data.maxz_downdepth_layer_sh = DRW_shader_create_from_info_name(
        "eevee_legacy_maxz_downdepth_layer");
  }
  return e_data.maxz_downdepth_layer_sh;
}

GPUShader *EEVEE_shaders_effect_maxz_copydepth_layer_sh_get(void)
{
  if (e_data.maxz_copydepth_layer_sh == nullptr) {
    e_data.maxz_copydepth_layer_sh = DRW_shader_create_from_info_name(
        "eevee_legacy_maxz_copydepth_layer");
  }
  return e_data.maxz_copydepth_layer_sh;
}

GPUShader *EEVEE_shaders_effect_minz_copydepth_sh_get(void)
{
  if (e_data.minz_copydepth_sh == nullptr) {
    e_data.minz_copydepth_sh = DRW_shader_create_from_info_name("eevee_legacy_minz_copydepth");
  }
  return e_data.minz_copydepth_sh;
}

GPUShader *EEVEE_shaders_effect_maxz_copydepth_sh_get(void)
{
  if (e_data.maxz_copydepth_sh == nullptr) {
    e_data.maxz_copydepth_sh = DRW_shader_create_from_info_name("eevee_legacy_maxz_copydepth");
  }
  return e_data.maxz_copydepth_sh;
}

/** \} */

/* -------------------------------------------------------------------- */
/** \name GGX LUT
 * \{ */

GPUShader *EEVEE_shaders_ggx_lut_sh_get(void)
{
  if (e_data.ggx_lut_sh == nullptr) {
    e_data.ggx_lut_sh = DRW_shader_create_from_info_name("eevee_legacy_ggx_lut_bsdf");
  }
  return e_data.ggx_lut_sh;
}

GPUShader *EEVEE_shaders_ggx_refraction_lut_sh_get(void)
{
  if (e_data.ggx_refraction_lut_sh == nullptr) {
    e_data.ggx_refraction_lut_sh = DRW_shader_create_from_info_name("eevee_legacy_ggx_lut_btdf");
  }
  return e_data.ggx_refraction_lut_sh;
}

/** \} */

/* -------------------------------------------------------------------- */
/** \name Mist
 * \{ */

GPUShader *EEVEE_shaders_effect_mist_sh_get(void)
{
  if (e_data.mist_sh == nullptr) {
    e_data.mist_sh = DRW_shader_create_from_info_name("eevee_legacy_effect_mist_FIRST_PASS");
  }
  return e_data.mist_sh;
}

/** \} */

/* -------------------------------------------------------------------- */
/** \name Motion Blur
 * \{ */

#define TILE_SIZE_STR "#define EEVEE_VELOCITY_TILE_SIZE " STRINGIFY(EEVEE_VELOCITY_TILE_SIZE) "\n"
GPUShader *EEVEE_shaders_effect_motion_blur_sh_get(void)
{
  if (e_data.motion_blur_sh == nullptr) {
    e_data.motion_blur_sh = DRW_shader_create_from_info_name("eevee_legacy_effect_motion_blur");
  }
  return e_data.motion_blur_sh;
}

GPUShader *EEVEE_shaders_effect_motion_blur_object_sh_get(void)
{
  if (e_data.motion_blur_object_sh == nullptr) {
    e_data.motion_blur_object_sh = DRW_shader_create_from_info_name(
        "eevee_legacy_effect_motion_blur_object");
  }
  return e_data.motion_blur_object_sh;
}

GPUShader *EEVEE_shaders_effect_motion_blur_hair_sh_get(void)
{
  if (e_data.motion_blur_hair_sh == nullptr) {
    e_data.motion_blur_hair_sh = DRW_shader_create_from_info_name(
        "eevee_legacy_effect_motion_blur_object_hair");
  }
  return e_data.motion_blur_hair_sh;
}

GPUShader *EEVEE_shaders_effect_motion_blur_velocity_tiles_sh_get(void)
{
  if (e_data.velocity_tiles_sh == nullptr) {
    e_data.velocity_tiles_sh = DRW_shader_create_from_info_name(
        "eevee_legacy_effect_motion_blur_velocity_tiles_GATHER");
  }
  return e_data.velocity_tiles_sh;
}

GPUShader *EEVEE_shaders_effect_motion_blur_velocity_tiles_expand_sh_get(void)
{
  if (e_data.velocity_tiles_expand_sh == nullptr) {
    e_data.velocity_tiles_expand_sh = DRW_shader_create_from_info_name(
        "eevee_legacy_effect_motion_blur_velocity_tiles_EXPANSION");
  }
  return e_data.velocity_tiles_expand_sh;
}

#undef TILE_SIZE_STR

/** \} */

/* -------------------------------------------------------------------- */
/** \name Ambient Occlusion
 * \{ */

GPUShader *EEVEE_shaders_effect_ambient_occlusion_sh_get(void)
{
  if (e_data.gtao_sh == nullptr) {
    e_data.gtao_sh = DRW_shader_create_from_info_name("eevee_legacy_ambient_occlusion");
  }
  return e_data.gtao_sh;
}

GPUShader *EEVEE_shaders_effect_ambient_occlusion_debug_sh_get(void)
{
  if (e_data.gtao_debug_sh == nullptr) {
    e_data.gtao_debug_sh = DRW_shader_create_from_info_name(
        "eevee_legacy_ambient_occlusion_debug");
  }
  return e_data.gtao_debug_sh;
}

/** \} */

/* -------------------------------------------------------------------- */
/** \name Render Passes
 * \{ */

GPUShader *EEVEE_shaders_renderpasses_accumulate_sh_get(void)
{
  if (e_data.rpass_accumulate_sh == nullptr) {
    e_data.rpass_accumulate_sh = DRW_shader_create_from_info_name(
        "eevee_legacy_renderpass_accumulate");
  }
  return e_data.rpass_accumulate_sh;
}

GPUShader *EEVEE_shaders_renderpasses_post_process_sh_get(void)
{
  if (e_data.postprocess_sh == nullptr) {
    e_data.postprocess_sh = DRW_shader_create_from_info_name("eevee_legacy_post_process");
  }
  return e_data.postprocess_sh;
}

/** \} */

/* -------------------------------------------------------------------- */
/** \name Cryptomatte
 * \{ */

GPUShader *EEVEE_shaders_cryptomatte_sh_get(bool is_hair)
{
  const int index = is_hair ? 1 : 0;
  if (e_data.cryptomatte_sh[index] == nullptr) {
    const char *crytomatte_sh_info_name = nullptr;
    if (is_hair) {
      crytomatte_sh_info_name = "eevee_legacy_cryptomatte_hair";
    }
    else {
      crytomatte_sh_info_name = "eevee_legacy_cryptomatte_mesh";
    }

    e_data.cryptomatte_sh[index] = DRW_shader_create_from_info_name(crytomatte_sh_info_name);
  }
  return e_data.cryptomatte_sh[index];
}

/** \} */

/* -------------------------------------------------------------------- */
/** \name Raytraced Reflections
 * \{ */

struct GPUShader *EEVEE_shaders_effect_reflection_trace_sh_get(void)
{
  if (e_data.reflection_trace == nullptr) {
    e_data.reflection_trace = DRW_shader_create_from_info_name(
        "eevee_legacy_effect_reflection_trace");
  }
  return e_data.reflection_trace;
}

struct GPUShader *EEVEE_shaders_effect_reflection_resolve_sh_get(void)
{
  if (e_data.reflection_resolve == nullptr) {
    e_data.reflection_resolve = DRW_shader_create_from_info_name(
        "eevee_legacy_effect_reflection_resolve");
  }
  return e_data.reflection_resolve;
}

struct GPUShader *EEVEE_shaders_effect_reflection_resolve_probe_sh_get(void)
{
  if (e_data.reflection_resolve_probe == nullptr) {
    e_data.reflection_resolve_probe = DRW_shader_create_from_info_name(
        "eevee_legacy_effect_reflection_resolve_probe");
  }
  return e_data.reflection_resolve_probe;
}

struct GPUShader *EEVEE_shaders_effect_reflection_resolve_refl_sh_get(void)
{
  if (e_data.reflection_resolve_raytrace == nullptr) {
    e_data.reflection_resolve_raytrace = DRW_shader_create_from_info_name(
        "eevee_legacy_effect_reflection_resolve_ssr");
  }
  return e_data.reflection_resolve_raytrace;
}

/** \} */

/* -------------------------------------------------------------------- */
/** \name Shadows
 * \{ */

struct GPUShader *EEVEE_shaders_shadow_sh_get()
{
  if (e_data.shadow_sh == nullptr) {
    e_data.shadow_sh = DRW_shader_create_from_info_name("eevee_legacy_shader_shadow");
  }
  return e_data.shadow_sh;
}

struct GPUShader *EEVEE_shaders_shadow_accum_sh_get()
{
  if (e_data.shadow_accum_sh == nullptr) {
    e_data.shadow_accum_sh = DRW_shader_create_from_info_name("eevee_legacy_shader_shadow_accum");
  }
  return e_data.shadow_accum_sh;
}

/** \} */

/* -------------------------------------------------------------------- */
/** \name Subsurface
 * \{ */

struct GPUShader *EEVEE_shaders_subsurface_first_pass_sh_get()
{
  if (e_data.sss_sh[0] == nullptr) {
    e_data.sss_sh[0] = DRW_shader_create_from_info_name(
        "eevee_legacy_shader_effect_subsurface_common_FIRST_PASS");
  }
  return e_data.sss_sh[0];
}

struct GPUShader *EEVEE_shaders_subsurface_second_pass_sh_get()
{
  if (e_data.sss_sh[1] == nullptr) {

    e_data.sss_sh[1] = DRW_shader_create_from_info_name(
        "eevee_legacy_shader_effect_subsurface_common_SECOND_PASS");
  }
  return e_data.sss_sh[1];
}

struct GPUShader *EEVEE_shaders_subsurface_translucency_sh_get()
{
  if (e_data.sss_sh[2] == nullptr) {
    e_data.sss_sh[2] = DRW_shader_create_from_info_name(
        "eevee_legacy_shader_effect_subsurface_translucency");
  }
  return e_data.sss_sh[2];
}

/** \} */

/* -------------------------------------------------------------------- */
/** \name Volumes
 * \{ */

struct GPUShader *EEVEE_shaders_volumes_clear_sh_get()
{
  if (e_data.volumetric_clear_sh == nullptr) {
    e_data.volumetric_clear_sh = DRW_shader_create_from_info_name("eevee_legacy_volumes_clear");
  }
  return e_data.volumetric_clear_sh;
}

struct GPUShader *EEVEE_shaders_volumes_scatter_sh_get()
{
  if (e_data.scatter_sh == nullptr) {
    e_data.scatter_sh = DRW_shader_create_from_info_name("eevee_legacy_volumes_scatter");
  }
  return e_data.scatter_sh;
}

struct GPUShader *EEVEE_shaders_volumes_scatter_with_lights_sh_get()
{
  if (e_data.scatter_with_lights_sh == nullptr) {
    e_data.scatter_with_lights_sh = DRW_shader_create_from_info_name(
        "eevee_legacy_volumes_scatter_with_lights");
  }
  return e_data.scatter_with_lights_sh;
}

struct GPUShader *EEVEE_shaders_volumes_integration_sh_get()
{
  if (e_data.volumetric_integration_sh == nullptr) {
    e_data.volumetric_integration_sh = DRW_shader_create_from_info_name(
        (USE_VOLUME_OPTI) ? "eevee_legacy_volumes_integration_OPTI" :
                            "eevee_legacy_volumes_integration");
  }
  return e_data.volumetric_integration_sh;
}

struct GPUShader *EEVEE_shaders_volumes_resolve_sh_get(bool accum)
{
  const int index = accum ? 1 : 0;
  if (e_data.volumetric_resolve_sh[index] == nullptr) {
    e_data.volumetric_resolve_sh[index] = DRW_shader_create_from_info_name(
        (accum) ? "eevee_legacy_volumes_resolve_accum" : "eevee_legacy_volumes_resolve");
  }
  return e_data.volumetric_resolve_sh[index];
}

struct GPUShader *EEVEE_shaders_volumes_accum_sh_get()
{
  if (e_data.volumetric_accum_sh == nullptr) {
    e_data.volumetric_accum_sh = DRW_shader_create_from_info_name("eevee_legacy_volumes_accum");
  }
  return e_data.volumetric_accum_sh;
}

/** \} */

GPUShader *EEVEE_shaders_velocity_resolve_sh_get(void)
{
  if (e_data.velocity_resolve_sh == nullptr) {
    e_data.velocity_resolve_sh = DRW_shader_create_from_info_name("eevee_legacy_velocity_resolve");
  }
  return e_data.velocity_resolve_sh;
}

GPUShader *EEVEE_shaders_update_noise_sh_get(void)
{
  if (e_data.update_noise_sh == nullptr) {
    e_data.update_noise_sh = DRW_shader_create_from_info_name("eevee_legacy_update_noise");
  }
  return e_data.update_noise_sh;
}

GPUShader *EEVEE_shaders_taa_resolve_sh_get(EEVEE_EffectsFlag enabled_effects)
{
  if (enabled_effects & EFFECT_TAA_REPROJECT) {
    if (e_data.taa_resolve_reproject_sh == nullptr) {
      e_data.taa_resolve_reproject_sh = DRW_shader_create_from_info_name(
          "eevee_legacy_taa_resolve_reprojection");
    }
    return e_data.taa_resolve_reproject_sh;
  }
  if (e_data.taa_resolve_sh == nullptr) {
    e_data.taa_resolve_sh = DRW_shader_create_from_info_name("eevee_legacy_taa_resolve_basic");
  }
  return e_data.taa_resolve_sh;
}

/* -------------------------------------------------------------------- */
/** \name Bloom
 * \{ */

GPUShader *EEVEE_shaders_bloom_blit_get(bool high_quality)
{
  int index = high_quality ? 1 : 0;

  if (e_data.bloom_blit_sh[index] == nullptr) {
    e_data.bloom_blit_sh[index] = DRW_shader_create_from_info_name(
        high_quality ? "eevee_legacy_bloom_blit_hq" : "eevee_legacy_bloom_blit");
  }
  return e_data.bloom_blit_sh[index];
}

GPUShader *EEVEE_shaders_bloom_downsample_get(bool high_quality)
{
  int index = high_quality ? 1 : 0;

  if (e_data.bloom_downsample_sh[index] == nullptr) {
    e_data.bloom_downsample_sh[index] = DRW_shader_create_from_info_name(
        high_quality ? "eevee_legacy_bloom_downsample_hq" : "eevee_legacy_bloom_downsample");
  }
  return e_data.bloom_downsample_sh[index];
}

GPUShader *EEVEE_shaders_bloom_upsample_get(bool high_quality)
{
  int index = high_quality ? 1 : 0;

  if (e_data.bloom_upsample_sh[index] == nullptr) {
    e_data.bloom_upsample_sh[index] = DRW_shader_create_from_info_name(
        high_quality ? "eevee_legacy_bloom_upsample_hq" : "eevee_legacy_bloom_upsample");
  }
  return e_data.bloom_upsample_sh[index];
}

GPUShader *EEVEE_shaders_bloom_resolve_get(bool high_quality)
{
  int index = high_quality ? 1 : 0;

  if (e_data.bloom_resolve_sh[index] == nullptr) {
    e_data.bloom_resolve_sh[index] = DRW_shader_create_from_info_name(
        high_quality ? "eevee_legacy_bloom_resolve_hq" : "eevee_legacy_bloom_resolve");
  }
  return e_data.bloom_resolve_sh[index];
}

/** \} */

/* -------------------------------------------------------------------- */
/** \name Depth of field
 * \{ */

GPUShader *EEVEE_shaders_depth_of_field_bokeh_get(void)
{
  if (e_data.dof_bokeh_sh == nullptr) {
    e_data.dof_bokeh_sh = DRW_shader_create_from_info_name("eevee_legacy_depth_of_field_bokeh");
  }
  return e_data.dof_bokeh_sh;
}

GPUShader *EEVEE_shaders_depth_of_field_setup_get(void)
{
  if (e_data.dof_setup_sh == nullptr) {
    e_data.dof_setup_sh = DRW_shader_create_from_info_name("eevee_legacy_depth_of_field_setup");
  }
  return e_data.dof_setup_sh;
}

GPUShader *EEVEE_shaders_depth_of_field_flatten_tiles_get(void)
{
  if (e_data.dof_flatten_tiles_sh == nullptr) {
    e_data.dof_flatten_tiles_sh = DRW_shader_create_from_info_name(
        "eevee_legacy_depth_of_field_flatten_tiles");
  }
  return e_data.dof_flatten_tiles_sh;
}

GPUShader *EEVEE_shaders_depth_of_field_dilate_tiles_get(bool b_pass)
{
  int pass = b_pass;
  if (e_data.dof_dilate_tiles_sh[pass] == nullptr) {

    e_data.dof_dilate_tiles_sh[pass] = DRW_shader_create_from_info_name(
        (pass == 0) ? "eevee_legacy_depth_of_field_dilate_tiles_MINMAX" :
                      "eevee_legacy_depth_of_field_dilate_tiles_MINABS");
  }
  return e_data.dof_dilate_tiles_sh[pass];
}

GPUShader *EEVEE_shaders_depth_of_field_downsample_get(void)
{
  if (e_data.dof_downsample_sh == nullptr) {
    e_data.dof_downsample_sh = DRW_shader_create_from_info_name(
        "eevee_legacy_depth_of_field_downsample");
  }
  return e_data.dof_downsample_sh;
}

GPUShader *EEVEE_shaders_depth_of_field_reduce_get(bool b_is_copy_pass)
{
  int is_copy_pass = b_is_copy_pass;
  if (e_data.dof_reduce_sh[is_copy_pass] == nullptr) {
    e_data.dof_reduce_sh[is_copy_pass] = DRW_shader_create_from_info_name(
        (is_copy_pass) ? "eevee_legacy_depth_of_field_reduce_COPY_PASS" :
                         "eevee_legacy_depth_of_field_reduce_REDUCE_PASS");
  }
  return e_data.dof_reduce_sh[is_copy_pass];
}

GPUShader *EEVEE_shaders_depth_of_field_gather_get(EEVEE_DofGatherPass pass, bool b_use_bokeh_tx)
{
  int use_bokeh_tx = b_use_bokeh_tx;
  if (e_data.dof_gather_sh[pass][use_bokeh_tx] == nullptr) {
    const char *dof_gather_info_name = nullptr;
    switch (pass) {
      case DOF_GATHER_FOREGROUND:
        dof_gather_info_name = (b_use_bokeh_tx) ?
                                   "eevee_legacy_depth_of_field_gather_FOREGROUND_BOKEH" :
                                   "eevee_legacy_depth_of_field_gather_FOREGROUND";
        break;
      case DOF_GATHER_BACKGROUND:
        dof_gather_info_name = (b_use_bokeh_tx) ?
                                   "eevee_legacy_depth_of_field_gather_BACKGROUND_BOKEH" :
                                   "eevee_legacy_depth_of_field_gather_BACKGROUND";
        break;
      case DOF_GATHER_HOLEFILL:
        dof_gather_info_name = (b_use_bokeh_tx) ?
                                   "eevee_legacy_depth_of_field_gather_HOLEFILL_BOKEH" :
                                   "eevee_legacy_depth_of_field_gather_HOLEFILL";
        break;
      default:
        BLI_assert_unreachable();
        break;
    }
    BLI_assert(dof_gather_info_name != nullptr);
    e_data.dof_gather_sh[pass][use_bokeh_tx] = DRW_shader_create_from_info_name(
        dof_gather_info_name);
  }
  return e_data.dof_gather_sh[pass][use_bokeh_tx];
}

GPUShader *EEVEE_shaders_depth_of_field_filter_get(void)
{
  if (e_data.dof_filter_sh == nullptr) {
    e_data.dof_filter_sh = DRW_shader_create_from_info_name("eevee_legacy_depth_of_field_filter");
  }
  return e_data.dof_filter_sh;
}

GPUShader *EEVEE_shaders_depth_of_field_scatter_get(bool b_is_foreground, bool b_use_bokeh_tx)
{
  int is_foreground = b_is_foreground;
  int use_bokeh_tx = b_use_bokeh_tx;
  if (e_data.dof_scatter_sh[is_foreground][use_bokeh_tx] == nullptr) {
    const char *dof_filter_info_name = nullptr;
    if (b_is_foreground) {
      dof_filter_info_name = (b_use_bokeh_tx) ?
                                 "eevee_legacy_depth_of_field_scatter_FOREGROUND_BOKEH" :
                                 "eevee_legacy_depth_of_field_scatter_FOREGROUND";
    }
    else {
      dof_filter_info_name = (b_use_bokeh_tx) ?
                                 "eevee_legacy_depth_of_field_scatter_BACKGROUND_BOKEH" :
                                 "eevee_legacy_depth_of_field_scatter_BACKGROUND";
    }
    BLI_assert(dof_filter_info_name != nullptr);
    e_data.dof_scatter_sh[is_foreground][use_bokeh_tx] = DRW_shader_create_from_info_name(
        dof_filter_info_name);
  }
  return e_data.dof_scatter_sh[is_foreground][use_bokeh_tx];
}

GPUShader *EEVEE_shaders_depth_of_field_resolve_get(bool b_use_bokeh_tx, bool b_use_hq_gather)
{
  int use_hq_gather = b_use_hq_gather;
  int use_bokeh_tx = b_use_bokeh_tx;
  if (e_data.dof_resolve_sh[use_bokeh_tx][use_hq_gather] == nullptr) {
    const char *dof_resolve_info_name = nullptr;

    if (b_use_hq_gather) {
      dof_resolve_info_name = (b_use_bokeh_tx) ? "eevee_legacy_depth_of_field_resolve_HQ_BOKEH" :
                                                 "eevee_legacy_depth_of_field_resolve_HQ";
    }
    else {
      dof_resolve_info_name = (b_use_bokeh_tx) ? "eevee_legacy_depth_of_field_resolve_LQ_BOKEH" :
                                                 "eevee_legacy_depth_of_field_resolve_LQ";
    }
    BLI_assert(dof_resolve_info_name != nullptr);
    e_data.dof_resolve_sh[use_bokeh_tx][use_hq_gather] = DRW_shader_create_from_info_name(
        dof_resolve_info_name);
  }
  return e_data.dof_resolve_sh[use_bokeh_tx][use_hq_gather];
}

/** \} */

Material *EEVEE_material_default_diffuse_get(void)
{
  if (!e_data.diffuse_mat) {
    Material *ma = static_cast<Material *>(BKE_id_new_nomain(ID_MA, "EEVEEE default diffuse"));

    bNodeTree *ntree = blender::bke::ntreeAddTreeEmbedded(
        nullptr, &ma->id, "Shader Nodetree", ntreeType_Shader->idname);
    ma->use_nodes = true;

    bNode *bsdf = nodeAddStaticNode(nullptr, ntree, SH_NODE_BSDF_DIFFUSE);
    bNodeSocket *base_color = nodeFindSocket(bsdf, SOCK_IN, "Color");
    copy_v3_fl(((bNodeSocketValueRGBA *)base_color->default_value)->value, 0.8f);

    bNode *output = nodeAddStaticNode(nullptr, ntree, SH_NODE_OUTPUT_MATERIAL);

    nodeAddLink(ntree,
                bsdf,
                nodeFindSocket(bsdf, SOCK_OUT, "BSDF"),
                output,
                nodeFindSocket(output, SOCK_IN, "Surface"));

    nodeSetActive(ntree, output);
    e_data.diffuse_mat = ma;
  }
  return e_data.diffuse_mat;
}

Material *EEVEE_material_default_glossy_get(void)
{
  if (!e_data.glossy_mat) {
    Material *ma = static_cast<Material *>(BKE_id_new_nomain(ID_MA, "EEVEEE default metal"));

    bNodeTree *ntree = blender::bke::ntreeAddTreeEmbedded(
        nullptr, &ma->id, "Shader Nodetree", ntreeType_Shader->idname);
    ma->use_nodes = true;

    bNode *bsdf = nodeAddStaticNode(nullptr, ntree, SH_NODE_BSDF_GLOSSY);
    bNodeSocket *base_color = nodeFindSocket(bsdf, SOCK_IN, "Color");
    copy_v3_fl(((bNodeSocketValueRGBA *)base_color->default_value)->value, 1.0f);
    bNodeSocket *roughness = nodeFindSocket(bsdf, SOCK_IN, "Roughness");
    ((bNodeSocketValueFloat *)roughness->default_value)->value = 0.0f;

    bNode *output = nodeAddStaticNode(nullptr, ntree, SH_NODE_OUTPUT_MATERIAL);

    nodeAddLink(ntree,
                bsdf,
                nodeFindSocket(bsdf, SOCK_OUT, "BSDF"),
                output,
                nodeFindSocket(output, SOCK_IN, "Surface"));

    nodeSetActive(ntree, output);
    e_data.glossy_mat = ma;
  }
  return e_data.glossy_mat;
}

Material *EEVEE_material_default_error_get(void)
{
  if (!e_data.error_mat) {
    Material *ma = static_cast<Material *>(BKE_id_new_nomain(ID_MA, "EEVEEE default error"));

    bNodeTree *ntree = blender::bke::ntreeAddTreeEmbedded(
        nullptr, &ma->id, "Shader Nodetree", ntreeType_Shader->idname);
    ma->use_nodes = true;

    /* Use emission and output material to be compatible with both World and Material. */
    bNode *bsdf = nodeAddStaticNode(nullptr, ntree, SH_NODE_EMISSION);
    bNodeSocket *color = nodeFindSocket(bsdf, SOCK_IN, "Color");
    copy_v3_fl3(((bNodeSocketValueRGBA *)color->default_value)->value, 1.0f, 0.0f, 1.0f);

    bNode *output = nodeAddStaticNode(nullptr, ntree, SH_NODE_OUTPUT_MATERIAL);

    nodeAddLink(ntree,
                bsdf,
                nodeFindSocket(bsdf, SOCK_OUT, "Emission"),
                output,
                nodeFindSocket(output, SOCK_IN, "Surface"));

    nodeSetActive(ntree, output);
    e_data.error_mat = ma;
  }
  return e_data.error_mat;
}

struct bNodeTree *EEVEE_shader_default_surface_nodetree(Material *ma)
{
  /* WARNING: This function is not threadsafe. Which is not a problem for the moment. */
  if (!e_data.surface.ntree) {
    bNodeTree *ntree = ntreeAddTree(nullptr, "Shader Nodetree", ntreeType_Shader->idname);
    bNode *bsdf = nodeAddStaticNode(nullptr, ntree, SH_NODE_BSDF_PRINCIPLED);
    bNode *output = nodeAddStaticNode(nullptr, ntree, SH_NODE_OUTPUT_MATERIAL);
    bNodeSocket *bsdf_out = nodeFindSocket(bsdf, SOCK_OUT, "BSDF");
    bNodeSocket *output_in = nodeFindSocket(output, SOCK_IN, "Surface");
    nodeAddLink(ntree, bsdf, bsdf_out, output, output_in);
    nodeSetActive(ntree, output);

    e_data.surface.color_socket = static_cast<bNodeSocketValueRGBA *>(
        nodeFindSocket(bsdf, SOCK_IN, "Base Color")->default_value);
    e_data.surface.metallic_socket = static_cast<bNodeSocketValueFloat *>(
        nodeFindSocket(bsdf, SOCK_IN, "Metallic")->default_value);
    e_data.surface.roughness_socket = static_cast<bNodeSocketValueFloat *>(
        nodeFindSocket(bsdf, SOCK_IN, "Roughness")->default_value);
    e_data.surface.specular_socket = static_cast<bNodeSocketValueFloat *>(
        nodeFindSocket(bsdf, SOCK_IN, "Specular")->default_value);
    e_data.surface.ntree = ntree;
  }
  /* Update */
  copy_v3_fl3(e_data.surface.color_socket->value, ma->r, ma->g, ma->b);
  e_data.surface.metallic_socket->value = ma->metallic;
  e_data.surface.roughness_socket->value = ma->roughness;
  e_data.surface.specular_socket->value = ma->spec;

  return e_data.surface.ntree;
}

struct bNodeTree *EEVEE_shader_default_world_nodetree(World *wo)
{
  /* WARNING: This function is not threadsafe. Which is not a problem for the moment. */
  if (!e_data.world.ntree) {
    bNodeTree *ntree = ntreeAddTree(nullptr, "Shader Nodetree", ntreeType_Shader->idname);
    bNode *bg = nodeAddStaticNode(nullptr, ntree, SH_NODE_BACKGROUND);
    bNode *output = nodeAddStaticNode(nullptr, ntree, SH_NODE_OUTPUT_WORLD);
    bNodeSocket *bg_out = nodeFindSocket(bg, SOCK_OUT, "Background");
    bNodeSocket *output_in = nodeFindSocket(output, SOCK_IN, "Surface");
    nodeAddLink(ntree, bg, bg_out, output, output_in);
    nodeSetActive(ntree, output);

    e_data.world.color_socket = static_cast<bNodeSocketValueRGBA *>(
        nodeFindSocket(bg, SOCK_IN, "Color")->default_value);
    e_data.world.ntree = ntree;
  }

  copy_v3_fl3(e_data.world.color_socket->value, wo->horr, wo->horg, wo->horb);

  return e_data.world.ntree;
}

World *EEVEE_world_default_get(void)
{
  if (e_data.default_world == nullptr) {
    e_data.default_world = static_cast<World *>(BKE_id_new_nomain(ID_WO, "EEVEEE default world"));
    copy_v3_fl(&e_data.default_world->horr, 0.0f);
    e_data.default_world->use_nodes = 0;
    e_data.default_world->nodetree = nullptr;
    BLI_listbase_clear(&e_data.default_world->gpumaterial);
  }
  return e_data.default_world;
}

/* Select create info configuration and base source for given material types.
 * Each configuration has an associated base source and create-info.
 * Source is provided separately, rather than via create-info as source is manipulated
 * by `eevee_shader_material_create_info_amend`.
 *
 * We also retain the previous behavior for ensuring library includes occur in the
 * correct order. */
static const char *eevee_get_vert_info(int options, char **r_src)
{
  const char *info_name = nullptr;

  /* Permutations */
  const bool is_hair = (options & (VAR_MAT_HAIR)) != 0;
  const bool is_point_cloud = (options & (VAR_MAT_POINTCLOUD)) != 0;

  if ((options & VAR_MAT_VOLUME) != 0) {
    *r_src = DRW_shader_library_create_shader_string(e_data.lib, datatoc_volumetric_vert_glsl);
    info_name = "eevee_legacy_material_volumetric_vert";
  }
  else if ((options & (VAR_WORLD_PROBE | VAR_WORLD_BACKGROUND)) != 0) {
    *r_src = DRW_shader_library_create_shader_string(e_data.lib, datatoc_world_vert_glsl);
    info_name = "eevee_legacy_material_world_vert";
  }
  else {
    *r_src = DRW_shader_library_create_shader_string(e_data.lib, datatoc_surface_vert_glsl);
    if (is_hair) {
      info_name = "eevee_legacy_mateiral_surface_vert_hair";
    }
    else if (is_point_cloud) {
      info_name = "eevee_legacy_mateiral_surface_vert_pointcloud";
    }
    else {
      info_name = "eevee_legacy_material_surface_vert";
    }
  }

  return info_name;
}

static const char *eevee_get_geom_info(int options, char **r_src)
{
  const char *info_name = nullptr;
  if (GPU_backend_get_type() == GPU_BACKEND_METAL) {
    /* Geometry shading is unsupported in Metal. Vertex-shader based workarounds will instead
     * be injected where appropriate. For volumetric rendering, volumetric_vert_no_geom replaces
     * the default volumetric_vert + volumetric_geom combination.
     * See: `source/blender/gpu/intern/gpu_shader_create_info.cc` for further details. */
    *r_src = nullptr;
    return nullptr;
  }

  if ((options & VAR_MAT_VOLUME) != 0) {
    *r_src = DRW_shader_library_create_shader_string(e_data.lib, datatoc_volumetric_geom_glsl);
    info_name = "eevee_legacy_material_volumetric_geom";
  }

  return info_name;
}

static const char *eevee_get_frag_info(int options, char **r_src)
{
  const char *info_name = nullptr;

  const bool is_alpha_hash = ((options & VAR_MAT_HASH) != 0);
  bool is_alpha_blend = ((options & VAR_MAT_BLEND) != 0);
  const bool is_hair = (options & (VAR_MAT_HAIR)) != 0;
  const bool is_point_cloud = (options & (VAR_MAT_POINTCLOUD)) != 0;

  if ((options & VAR_MAT_VOLUME) != 0) {
    /* -- VOLUME FRAG -
     * Select create info permutation for `volume_frag`. */
    info_name = "eevee_legacy_material_volumetric_frag";
    *r_src = DRW_shader_library_create_shader_string(e_data.lib, datatoc_volumetric_frag_glsl);
  }
  else if ((options & VAR_MAT_DEPTH) != 0) {
    /* -- PREPASS FRAG -
     * Select create info permutation for `prepass_frag`. */

    if (is_alpha_hash) {
      /* Alpha hash material variants. */
      if (is_hair) {
        info_name = "eevee_legacy_material_prepass_frag_alpha_hash_hair";
      }
      else if (is_point_cloud) {
        info_name = "eevee_legacy_material_prepass_frag_alpha_hash_pointcloud";
      }
      else {
        info_name = "eevee_legacy_material_prepass_frag_alpha_hash";
      }
    }
    else {
      /* Opaque material variants. */
      if (is_hair) {
        info_name = "eevee_legacy_material_prepass_frag_opaque_hair";
      }
      else if (is_point_cloud) {
        info_name = "eevee_legacy_material_prepass_frag_opaque_pointcloud";
      }
      else {
        info_name = "eevee_legacy_material_prepass_frag_opaque";
      }
    }
    *r_src = BLI_strdup(e_data.surface_prepass_frag);
  }
  else {
    /* -- SURFACE FRAG --
     * Select create info permutation for `surface_frag`. */
    if (is_alpha_blend) {
      info_name = "eevee_legacy_material_surface_frag_alpha_blend";
    }
    else {
      info_name = "eevee_legacy_material_surface_frag_opaque";
    }
    *r_src = BLI_strdup(e_data.surface_lit_frag);
  }

  return info_name;
}

static char *eevee_get_defines(int options)
{
  char *str = nullptr;

  DynStr *ds = BLI_dynstr_new();

  /* Global EEVEE defines included for CreateInfo shaders via `engine_eevee_shared_defines.h` in
   * eevee_legacy_common_lib CreateInfo. */

  /* Defines which affect bindings are instead injected via CreateInfo permutations. To specify
   * new permutations, references to GPUShaderCreateInfo variants should be fetched in:
   * `eevee_get_vert/geom/frag_info(..)`
   *
   * CreateInfo's for EEVEE materials are declared in:
   * `eevee/shaders/infos/eevee_legacy_material_info.hh`
   *
   * This function should only contain defines which alter behavior, but do not affect shader
   * resources. */

  if ((options & VAR_WORLD_BACKGROUND) != 0) {
    BLI_dynstr_append(ds, "#define WORLD_BACKGROUND\n");
  }
  if ((options & VAR_MAT_VOLUME) != 0) {
    BLI_dynstr_append(ds, "#define VOLUMETRICS\n");
  }
  if ((options & VAR_MAT_MESH) != 0) {
    BLI_dynstr_append(ds, "#define MESH_SHADER\n");
  }
  if ((options & VAR_MAT_DEPTH) != 0) {
    BLI_dynstr_append(ds, "#define DEPTH_SHADER\n");
  }
  if ((options & VAR_WORLD_PROBE) != 0) {
    BLI_dynstr_append(ds, "#define PROBE_CAPTURE\n");
  }
  if ((options & VAR_MAT_REFRACT) != 0) {
    BLI_dynstr_append(ds, "#define USE_REFRACTION\n");
  }
  if ((options & VAR_MAT_HOLDOUT) != 0) {
    BLI_dynstr_append(ds, "#define HOLDOUT\n");
  }

  str = BLI_dynstr_get_cstring(ds);
  BLI_dynstr_free(ds);

  return str;
}

static void eevee_material_post_eval(void * /*thunk*/, GPUMaterial *mat, GPUCodegenOutput *codegen)
{
  /* Fetch material-specific Create-info's and source. */
  uint64_t options = GPU_material_uuid_get(mat);
  char *vert = nullptr;
  char *geom = nullptr;
  char *frag = nullptr;

  const char *vert_info_name = eevee_get_vert_info(options, &vert);
  const char *geom_info_name = eevee_get_geom_info(options, &geom);
  const char *frag_info_name = eevee_get_frag_info(options, &frag);
  char *defines = eevee_get_defines(options);

  eevee_shader_material_create_info_amend(
      mat, codegen, vert, geom, frag, vert_info_name, geom_info_name, frag_info_name, defines);

  MEM_SAFE_FREE(defines);
  MEM_SAFE_FREE(vert);
  MEM_SAFE_FREE(geom);
  MEM_SAFE_FREE(frag);
}

static struct GPUMaterial *eevee_material_get_ex(
    struct Scene * /*scene*/, Material *ma, World *wo, int options, bool deferred)
{
  BLI_assert(ma || wo);
  const bool is_volume = (options & VAR_MAT_VOLUME) != 0;
  const bool is_default = (options & VAR_DEFAULT) != 0;

  GPUMaterial *mat = nullptr;
  GPUCodegenCallbackFn cbfn = &eevee_material_post_eval;

  if (ma) {
    bNodeTree *ntree = !is_default ? ma->nodetree : EEVEE_shader_default_surface_nodetree(ma);
    mat = DRW_shader_from_material(ma, ntree, options, is_volume, deferred, cbfn, nullptr);
  }
  else {
    bNodeTree *ntree = !is_default ? wo->nodetree : EEVEE_shader_default_world_nodetree(wo);
    mat = DRW_shader_from_world(wo, ntree, options, is_volume, deferred, cbfn, nullptr);
  }
  return mat;
}

struct GPUMaterial *EEVEE_material_default_get(struct Scene *scene, Material *ma, int options)
{
  Material *def_ma = (ma && (options & VAR_MAT_VOLUME)) ? BKE_material_default_volume() :
                                                          BKE_material_default_surface();
  BLI_assert(def_ma->use_nodes && def_ma->nodetree);

  return eevee_material_get_ex(scene, def_ma, nullptr, options, false);
}

struct GPUMaterial *EEVEE_material_get(
    EEVEE_Data *vedata, struct Scene *scene, Material *ma, World *wo, int options)
{
  if ((ma && (!ma->use_nodes || !ma->nodetree)) || (wo && (!wo->use_nodes || !wo->nodetree))) {
    options |= VAR_DEFAULT;
  }

  /* Meh, implicit option. World probe cannot be deferred because they need
   * to be rendered immediately. */
  const bool deferred = (options & VAR_WORLD_PROBE) == 0;

  GPUMaterial *mat = eevee_material_get_ex(scene, ma, wo, options, deferred);

  int status = GPU_material_status(mat);
  /* Return null material and bypass drawing for volume shaders. */
  if ((options & VAR_MAT_VOLUME) && status != GPU_MAT_SUCCESS) {
    return nullptr;
  }
  switch (status) {
    case GPU_MAT_SUCCESS: {
      /* Determine optimization status for remaining compilations counter. */
      int optimization_status = GPU_material_optimization_status(mat);
      if (optimization_status == GPU_MAT_OPTIMIZATION_QUEUED) {
        vedata->stl->g_data->queued_optimise_shaders_count++;
      }
    } break;
    case GPU_MAT_QUEUED: {
      vedata->stl->g_data->queued_shaders_count++;
      GPUMaterial *default_mat = EEVEE_material_default_get(scene, ma, options);
      /* Mark pending material with its default material for future cache warming. */
      GPU_material_set_default(mat, default_mat);
      /* Return default material. */
      mat = default_mat;
    } break;
    case GPU_MAT_FAILED:
    default:
      ma = EEVEE_material_default_error_get();
      mat = eevee_material_get_ex(scene, ma, nullptr, options, false);
      break;
  }
  /* Returned material should be ready to be drawn. */
  BLI_assert(GPU_material_status(mat) == GPU_MAT_SUCCESS);
  return mat;
}

void EEVEE_shaders_free(void)
{
  MEM_SAFE_FREE(e_data.surface_prepass_frag);
  MEM_SAFE_FREE(e_data.surface_lit_frag);
  MEM_SAFE_FREE(e_data.surface_geom_barycentric);
  DRW_SHADER_FREE_SAFE(e_data.lookdev_background);
  DRW_SHADER_FREE_SAFE(e_data.update_noise_sh);
  DRW_SHADER_FREE_SAFE(e_data.color_copy_sh);
  DRW_SHADER_FREE_SAFE(e_data.downsample_sh);
  DRW_SHADER_FREE_SAFE(e_data.downsample_cube_sh);
  DRW_SHADER_FREE_SAFE(e_data.minz_downlevel_sh);
  DRW_SHADER_FREE_SAFE(e_data.maxz_downlevel_sh);
  DRW_SHADER_FREE_SAFE(e_data.minz_downdepth_sh);
  DRW_SHADER_FREE_SAFE(e_data.maxz_downdepth_sh);
  DRW_SHADER_FREE_SAFE(e_data.minz_downdepth_layer_sh);
  DRW_SHADER_FREE_SAFE(e_data.maxz_downdepth_layer_sh);
  DRW_SHADER_FREE_SAFE(e_data.maxz_copydepth_layer_sh);
  DRW_SHADER_FREE_SAFE(e_data.minz_copydepth_sh);
  DRW_SHADER_FREE_SAFE(e_data.maxz_copydepth_sh);
  DRW_SHADER_FREE_SAFE(e_data.ggx_lut_sh);
  DRW_SHADER_FREE_SAFE(e_data.ggx_refraction_lut_sh);
  DRW_SHADER_FREE_SAFE(e_data.mist_sh);
  DRW_SHADER_FREE_SAFE(e_data.motion_blur_sh);
  DRW_SHADER_FREE_SAFE(e_data.motion_blur_object_sh);
  DRW_SHADER_FREE_SAFE(e_data.motion_blur_hair_sh);
  DRW_SHADER_FREE_SAFE(e_data.velocity_tiles_sh);
  DRW_SHADER_FREE_SAFE(e_data.velocity_tiles_expand_sh);
  DRW_SHADER_FREE_SAFE(e_data.gtao_sh);
  DRW_SHADER_FREE_SAFE(e_data.gtao_layer_sh);
  DRW_SHADER_FREE_SAFE(e_data.gtao_debug_sh);
  DRW_SHADER_FREE_SAFE(e_data.velocity_resolve_sh);
  DRW_SHADER_FREE_SAFE(e_data.rpass_accumulate_sh);
  DRW_SHADER_FREE_SAFE(e_data.postprocess_sh);
  DRW_SHADER_FREE_SAFE(e_data.shadow_sh);
  DRW_SHADER_FREE_SAFE(e_data.shadow_accum_sh);
  DRW_SHADER_FREE_SAFE(e_data.sss_sh[0]);
  DRW_SHADER_FREE_SAFE(e_data.sss_sh[1]);
  DRW_SHADER_FREE_SAFE(e_data.sss_sh[2]);
  DRW_SHADER_FREE_SAFE(e_data.volumetric_clear_sh);
  DRW_SHADER_FREE_SAFE(e_data.scatter_sh);
  DRW_SHADER_FREE_SAFE(e_data.scatter_with_lights_sh);
  DRW_SHADER_FREE_SAFE(e_data.volumetric_integration_sh);
  DRW_SHADER_FREE_SAFE(e_data.volumetric_resolve_sh[0]);
  DRW_SHADER_FREE_SAFE(e_data.volumetric_resolve_sh[1]);
  DRW_SHADER_FREE_SAFE(e_data.volumetric_accum_sh);
  DRW_SHADER_FREE_SAFE(e_data.probe_filter_glossy_sh);
  DRW_SHADER_FREE_SAFE(e_data.probe_filter_diffuse_sh);
  DRW_SHADER_FREE_SAFE(e_data.probe_filter_visibility_sh);
  DRW_SHADER_FREE_SAFE(e_data.probe_grid_fill_sh);
  DRW_SHADER_FREE_SAFE(e_data.probe_planar_downsample_sh);
  DRW_SHADER_FREE_SAFE(e_data.studiolight_probe_sh);
  DRW_SHADER_FREE_SAFE(e_data.studiolight_background_sh);
  DRW_SHADER_FREE_SAFE(e_data.probe_grid_display_sh);
  DRW_SHADER_FREE_SAFE(e_data.probe_cube_display_sh);
  DRW_SHADER_FREE_SAFE(e_data.probe_planar_display_sh);
  DRW_SHADER_FREE_SAFE(e_data.velocity_resolve_sh);
  DRW_SHADER_FREE_SAFE(e_data.taa_resolve_sh);
  DRW_SHADER_FREE_SAFE(e_data.taa_resolve_reproject_sh);
  DRW_SHADER_FREE_SAFE(e_data.dof_bokeh_sh);
  DRW_SHADER_FREE_SAFE(e_data.dof_setup_sh);
  DRW_SHADER_FREE_SAFE(e_data.dof_flatten_tiles_sh);
  DRW_SHADER_FREE_SAFE(e_data.dof_dilate_tiles_sh[0]);
  DRW_SHADER_FREE_SAFE(e_data.dof_dilate_tiles_sh[1]);
  DRW_SHADER_FREE_SAFE(e_data.dof_downsample_sh);
  DRW_SHADER_FREE_SAFE(e_data.dof_reduce_sh[0]);
  DRW_SHADER_FREE_SAFE(e_data.dof_reduce_sh[1]);
  for (int i = 0; i < DOF_GATHER_MAX_PASS; i++) {
    DRW_SHADER_FREE_SAFE(e_data.dof_gather_sh[i][0]);
    DRW_SHADER_FREE_SAFE(e_data.dof_gather_sh[i][1]);
  }
  DRW_SHADER_FREE_SAFE(e_data.dof_filter_sh);
  DRW_SHADER_FREE_SAFE(e_data.dof_scatter_sh[0][0]);
  DRW_SHADER_FREE_SAFE(e_data.dof_scatter_sh[0][1]);
  DRW_SHADER_FREE_SAFE(e_data.dof_scatter_sh[1][0]);
  DRW_SHADER_FREE_SAFE(e_data.dof_scatter_sh[1][1]);
  DRW_SHADER_FREE_SAFE(e_data.dof_resolve_sh[0][0]);
  DRW_SHADER_FREE_SAFE(e_data.dof_resolve_sh[0][1]);
  DRW_SHADER_FREE_SAFE(e_data.dof_resolve_sh[1][0]);
  DRW_SHADER_FREE_SAFE(e_data.dof_resolve_sh[1][1]);
  DRW_SHADER_FREE_SAFE(e_data.cryptomatte_sh[0]);
  DRW_SHADER_FREE_SAFE(e_data.cryptomatte_sh[1]);
  for (int i = 0; i < 2; i++) {
    DRW_SHADER_FREE_SAFE(e_data.bloom_blit_sh[i]);
    DRW_SHADER_FREE_SAFE(e_data.bloom_downsample_sh[i]);
    DRW_SHADER_FREE_SAFE(e_data.bloom_upsample_sh[i]);
    DRW_SHADER_FREE_SAFE(e_data.bloom_resolve_sh[i]);
  }
  DRW_SHADER_FREE_SAFE(e_data.reflection_trace);
  DRW_SHADER_FREE_SAFE(e_data.reflection_resolve);
  DRW_SHADER_FREE_SAFE(e_data.reflection_resolve_probe);
  DRW_SHADER_FREE_SAFE(e_data.reflection_resolve_raytrace);
  DRW_SHADER_LIB_FREE_SAFE(e_data.lib);

  if (e_data.default_world) {
    BKE_id_free(nullptr, e_data.default_world);
    e_data.default_world = nullptr;
  }
  if (e_data.glossy_mat) {
    BKE_id_free(nullptr, e_data.glossy_mat);
    e_data.glossy_mat = nullptr;
  }
  if (e_data.diffuse_mat) {
    BKE_id_free(nullptr, e_data.diffuse_mat);
    e_data.diffuse_mat = nullptr;
  }
  if (e_data.error_mat) {
    BKE_id_free(nullptr, e_data.error_mat);
    e_data.error_mat = nullptr;
  }
  if (e_data.surface.ntree) {
    ntreeFreeEmbeddedTree(e_data.surface.ntree);
    MEM_freeN(e_data.surface.ntree);
    e_data.surface.ntree = nullptr;
  }
  if (e_data.world.ntree) {
    ntreeFreeEmbeddedTree(e_data.world.ntree);
    MEM_freeN(e_data.world.ntree);
    e_data.world.ntree = nullptr;
  }
}
