/* Apache License, Version 2.0 */

#include "testing/testing.h"

#include "draw_testing.hh"

#include "GPU_context.h"
#include "GPU_index_buffer.h"
#include "GPU_init_exit.h"
#include "GPU_shader.h"
#include "GPU_texture.h"
#include "GPU_vertex_buffer.h"

#include "intern/draw_manager_testing.h"

#include "engines/basic/basic_private.h"
#include "engines/eevee/eevee_private.h"
#include "engines/gpencil/gpencil_engine.h"
#include "engines/image/image_private.hh"
#include "engines/overlay/overlay_private.h"
#include "engines/workbench/workbench_private.h"
#include "intern/draw_shader.h"

namespace blender::draw {

using namespace blender::draw::image_engine;

static void test_workbench_glsl_shaders()
{
  workbench_shader_library_ensure();

  const int MAX_WPD = 6;
  WORKBENCH_PrivateData wpds[MAX_WPD];

  wpds[0].sh_cfg = GPU_SHADER_CFG_DEFAULT;
  wpds[0].shading.light = V3D_LIGHTING_FLAT;
  wpds[1].sh_cfg = GPU_SHADER_CFG_DEFAULT;
  wpds[1].shading.light = V3D_LIGHTING_MATCAP;
  wpds[2].sh_cfg = GPU_SHADER_CFG_DEFAULT;
  wpds[2].shading.light = V3D_LIGHTING_STUDIO;
  wpds[3].sh_cfg = GPU_SHADER_CFG_CLIPPED;
  wpds[3].shading.light = V3D_LIGHTING_FLAT;
  wpds[4].sh_cfg = GPU_SHADER_CFG_CLIPPED;
  wpds[4].shading.light = V3D_LIGHTING_MATCAP;
  wpds[5].sh_cfg = GPU_SHADER_CFG_CLIPPED;
  wpds[5].shading.light = V3D_LIGHTING_STUDIO;

  for (int wpd_index = 0; wpd_index < MAX_WPD; wpd_index++) {
    WORKBENCH_PrivateData *wpd = &wpds[wpd_index];
    EXPECT_NE(workbench_shader_opaque_get(wpd, WORKBENCH_DATATYPE_MESH), nullptr);
    EXPECT_NE(workbench_shader_opaque_get(wpd, WORKBENCH_DATATYPE_HAIR), nullptr);
    EXPECT_NE(workbench_shader_opaque_get(wpd, WORKBENCH_DATATYPE_POINTCLOUD), nullptr);
    EXPECT_NE(workbench_shader_opaque_image_get(wpd, WORKBENCH_DATATYPE_MESH, false), nullptr);
    EXPECT_NE(workbench_shader_opaque_image_get(wpd, WORKBENCH_DATATYPE_MESH, true), nullptr);
    EXPECT_NE(workbench_shader_opaque_image_get(wpd, WORKBENCH_DATATYPE_HAIR, false), nullptr);
    EXPECT_NE(workbench_shader_opaque_image_get(wpd, WORKBENCH_DATATYPE_HAIR, true), nullptr);
    EXPECT_NE(workbench_shader_opaque_image_get(wpd, WORKBENCH_DATATYPE_POINTCLOUD, false),
              nullptr);
    EXPECT_NE(workbench_shader_opaque_image_get(wpd, WORKBENCH_DATATYPE_POINTCLOUD, true),
              nullptr);
    EXPECT_NE(workbench_shader_composite_get(wpd), nullptr);
    EXPECT_NE(workbench_shader_merge_infront_get(wpd), nullptr);

    EXPECT_NE(workbench_shader_transparent_get(wpd, WORKBENCH_DATATYPE_MESH), nullptr);
    EXPECT_NE(workbench_shader_transparent_get(wpd, WORKBENCH_DATATYPE_HAIR), nullptr);
    EXPECT_NE(workbench_shader_transparent_get(wpd, WORKBENCH_DATATYPE_POINTCLOUD), nullptr);
    EXPECT_NE(workbench_shader_transparent_image_get(wpd, WORKBENCH_DATATYPE_MESH, false),
              nullptr);
    EXPECT_NE(workbench_shader_transparent_image_get(wpd, WORKBENCH_DATATYPE_MESH, true), nullptr);
    EXPECT_NE(workbench_shader_transparent_image_get(wpd, WORKBENCH_DATATYPE_HAIR, false),
              nullptr);
    EXPECT_NE(workbench_shader_transparent_image_get(wpd, WORKBENCH_DATATYPE_HAIR, true), nullptr);
    EXPECT_NE(workbench_shader_transparent_image_get(wpd, WORKBENCH_DATATYPE_POINTCLOUD, false),
              nullptr);
    EXPECT_NE(workbench_shader_transparent_image_get(wpd, WORKBENCH_DATATYPE_POINTCLOUD, true),
              nullptr);
    EXPECT_NE(workbench_shader_transparent_resolve_get(wpd), nullptr);
  }

  EXPECT_NE(workbench_shader_shadow_pass_get(false), nullptr);
  EXPECT_NE(workbench_shader_shadow_pass_get(true), nullptr);
  EXPECT_NE(workbench_shader_shadow_fail_get(false, false), nullptr);
  EXPECT_NE(workbench_shader_shadow_fail_get(false, true), nullptr);
  EXPECT_NE(workbench_shader_shadow_fail_get(true, false), nullptr);
  EXPECT_NE(workbench_shader_shadow_fail_get(true, true), nullptr);

  /* NOTE: workbench_shader_cavity_get(false, false) isn't a valid option. */
  EXPECT_NE(workbench_shader_cavity_get(false, true), nullptr);
  EXPECT_NE(workbench_shader_cavity_get(true, false), nullptr);
  EXPECT_NE(workbench_shader_cavity_get(true, true), nullptr);
  EXPECT_NE(workbench_shader_outline_get(), nullptr);

  EXPECT_NE(workbench_shader_antialiasing_accumulation_get(), nullptr);
  EXPECT_NE(workbench_shader_antialiasing_get(0), nullptr);
  EXPECT_NE(workbench_shader_antialiasing_get(1), nullptr);
  EXPECT_NE(workbench_shader_antialiasing_get(2), nullptr);

  EXPECT_NE(workbench_shader_volume_get(false, false, WORKBENCH_VOLUME_INTERP_LINEAR, false),
            nullptr);
  EXPECT_NE(workbench_shader_volume_get(false, false, WORKBENCH_VOLUME_INTERP_LINEAR, true),
            nullptr);
  EXPECT_NE(workbench_shader_volume_get(false, false, WORKBENCH_VOLUME_INTERP_CUBIC, false),
            nullptr);
  EXPECT_NE(workbench_shader_volume_get(false, false, WORKBENCH_VOLUME_INTERP_CUBIC, true),
            nullptr);
  EXPECT_NE(workbench_shader_volume_get(false, false, WORKBENCH_VOLUME_INTERP_CLOSEST, false),
            nullptr);
  EXPECT_NE(workbench_shader_volume_get(false, false, WORKBENCH_VOLUME_INTERP_CLOSEST, true),
            nullptr);
  EXPECT_NE(workbench_shader_volume_get(false, true, WORKBENCH_VOLUME_INTERP_LINEAR, false),
            nullptr);
  EXPECT_NE(workbench_shader_volume_get(false, true, WORKBENCH_VOLUME_INTERP_LINEAR, true),
            nullptr);
  EXPECT_NE(workbench_shader_volume_get(false, true, WORKBENCH_VOLUME_INTERP_CUBIC, false),
            nullptr);
  EXPECT_NE(workbench_shader_volume_get(false, true, WORKBENCH_VOLUME_INTERP_CUBIC, true),
            nullptr);
  EXPECT_NE(workbench_shader_volume_get(false, true, WORKBENCH_VOLUME_INTERP_CLOSEST, false),
            nullptr);
  EXPECT_NE(workbench_shader_volume_get(false, true, WORKBENCH_VOLUME_INTERP_CLOSEST, true),
            nullptr);
  EXPECT_NE(workbench_shader_volume_get(true, false, WORKBENCH_VOLUME_INTERP_LINEAR, false),
            nullptr);
  EXPECT_NE(workbench_shader_volume_get(true, false, WORKBENCH_VOLUME_INTERP_LINEAR, true),
            nullptr);
  EXPECT_NE(workbench_shader_volume_get(true, false, WORKBENCH_VOLUME_INTERP_CUBIC, false),
            nullptr);
  EXPECT_NE(workbench_shader_volume_get(true, false, WORKBENCH_VOLUME_INTERP_CUBIC, true),
            nullptr);
  EXPECT_NE(workbench_shader_volume_get(true, false, WORKBENCH_VOLUME_INTERP_CLOSEST, false),
            nullptr);
  EXPECT_NE(workbench_shader_volume_get(true, false, WORKBENCH_VOLUME_INTERP_CLOSEST, true),
            nullptr);
  EXPECT_NE(workbench_shader_volume_get(true, true, WORKBENCH_VOLUME_INTERP_LINEAR, false),
            nullptr);
  EXPECT_NE(workbench_shader_volume_get(true, true, WORKBENCH_VOLUME_INTERP_LINEAR, true),
            nullptr);
  EXPECT_NE(workbench_shader_volume_get(true, true, WORKBENCH_VOLUME_INTERP_CUBIC, false),
            nullptr);
  EXPECT_NE(workbench_shader_volume_get(true, true, WORKBENCH_VOLUME_INTERP_CUBIC, true), nullptr);
  EXPECT_NE(workbench_shader_volume_get(true, true, WORKBENCH_VOLUME_INTERP_CLOSEST, false),
            nullptr);
  EXPECT_NE(workbench_shader_volume_get(true, true, WORKBENCH_VOLUME_INTERP_CLOSEST, true),
            nullptr);

  GPUShader *dof_prepare_sh;
  GPUShader *dof_downsample_sh;
  GPUShader *dof_blur1_sh;
  GPUShader *dof_blur2_sh;
  GPUShader *dof_resolve_sh;
  workbench_shader_depth_of_field_get(
      &dof_prepare_sh, &dof_downsample_sh, &dof_blur1_sh, &dof_blur2_sh, &dof_resolve_sh);
  EXPECT_NE(dof_prepare_sh, nullptr);
  EXPECT_NE(dof_downsample_sh, nullptr);
  EXPECT_NE(dof_blur1_sh, nullptr);
  EXPECT_NE(dof_blur2_sh, nullptr);
  EXPECT_NE(dof_resolve_sh, nullptr);

  workbench_shader_free();
}
DRAW_TEST(workbench_glsl_shaders)

static void test_gpencil_glsl_shaders()
{
  EXPECT_NE(GPENCIL_shader_antialiasing(0), nullptr);
  EXPECT_NE(GPENCIL_shader_antialiasing(1), nullptr);
  EXPECT_NE(GPENCIL_shader_antialiasing(2), nullptr);

  EXPECT_NE(GPENCIL_shader_geometry_get(), nullptr);
  EXPECT_NE(GPENCIL_shader_layer_blend_get(), nullptr);
  EXPECT_NE(GPENCIL_shader_mask_invert_get(), nullptr);
  EXPECT_NE(GPENCIL_shader_depth_merge_get(), nullptr);
  EXPECT_NE(GPENCIL_shader_fx_blur_get(), nullptr);
  EXPECT_NE(GPENCIL_shader_fx_colorize_get(), nullptr);
  EXPECT_NE(GPENCIL_shader_fx_composite_get(), nullptr);
  EXPECT_NE(GPENCIL_shader_fx_transform_get(), nullptr);
  EXPECT_NE(GPENCIL_shader_fx_glow_get(), nullptr);
  EXPECT_NE(GPENCIL_shader_fx_pixelize_get(), nullptr);
  EXPECT_NE(GPENCIL_shader_fx_rim_get(), nullptr);
  EXPECT_NE(GPENCIL_shader_fx_shadow_get(), nullptr);

  GPENCIL_shader_free();
}
DRAW_TEST(gpencil_glsl_shaders)

static void test_image_glsl_shaders()
{
  IMAGE_shader_library_ensure();

  EXPECT_NE(IMAGE_shader_image_get(false), nullptr);
  EXPECT_NE(IMAGE_shader_image_get(true), nullptr);

  IMAGE_shader_free();
}
DRAW_TEST(image_glsl_shaders)

static void test_overlay_glsl_shaders()
{
  OVERLAY_shader_library_ensure();

  for (int i = 0; i < 2; i++) {
    eGPUShaderConfig sh_cfg = i == 0 ? GPU_SHADER_CFG_DEFAULT : GPU_SHADER_CFG_CLIPPED;
    DRW_draw_state_init_gtests(sh_cfg);
    EXPECT_NE(OVERLAY_shader_antialiasing(), nullptr);
    EXPECT_NE(OVERLAY_shader_armature_degrees_of_freedom_wire(), nullptr);
    EXPECT_NE(OVERLAY_shader_armature_degrees_of_freedom_solid(), nullptr);
    EXPECT_NE(OVERLAY_shader_armature_envelope(false), nullptr);
    EXPECT_NE(OVERLAY_shader_armature_envelope(true), nullptr);
    EXPECT_NE(OVERLAY_shader_armature_shape(false), nullptr);
    EXPECT_NE(OVERLAY_shader_armature_shape(true), nullptr);
    EXPECT_NE(OVERLAY_shader_armature_shape_wire(), nullptr);
    EXPECT_NE(OVERLAY_shader_armature_sphere(false), nullptr);
    EXPECT_NE(OVERLAY_shader_armature_sphere(true), nullptr);
    EXPECT_NE(OVERLAY_shader_armature_stick(), nullptr);
    EXPECT_NE(OVERLAY_shader_armature_wire(), nullptr);
    EXPECT_NE(OVERLAY_shader_background(), nullptr);
    EXPECT_NE(OVERLAY_shader_clipbound(), nullptr);
    EXPECT_NE(OVERLAY_shader_depth_only(), nullptr);
    EXPECT_NE(OVERLAY_shader_edit_curve_handle(), nullptr);
    EXPECT_NE(OVERLAY_shader_edit_curve_point(), nullptr);
    EXPECT_NE(OVERLAY_shader_edit_curve_wire(), nullptr);
    EXPECT_NE(OVERLAY_shader_edit_gpencil_guide_point(), nullptr);
    EXPECT_NE(OVERLAY_shader_edit_gpencil_point(), nullptr);
    EXPECT_NE(OVERLAY_shader_edit_gpencil_wire(), nullptr);
    EXPECT_NE(OVERLAY_shader_edit_lattice_point(), nullptr);
    EXPECT_NE(OVERLAY_shader_edit_lattice_wire(), nullptr);
    EXPECT_NE(OVERLAY_shader_edit_mesh_analysis(), nullptr);
    EXPECT_NE(OVERLAY_shader_edit_mesh_edge(false), nullptr);
    EXPECT_NE(OVERLAY_shader_edit_mesh_edge(true), nullptr);
    EXPECT_NE(OVERLAY_shader_edit_mesh_face(), nullptr);
    EXPECT_NE(OVERLAY_shader_edit_mesh_facedot(), nullptr);
    EXPECT_NE(OVERLAY_shader_edit_mesh_normal(), nullptr);
    EXPECT_NE(OVERLAY_shader_edit_mesh_skin_root(), nullptr);
    EXPECT_NE(OVERLAY_shader_edit_mesh_vert(), nullptr);
    EXPECT_NE(OVERLAY_shader_edit_particle_strand(), nullptr);
    EXPECT_NE(OVERLAY_shader_edit_particle_point(), nullptr);
    EXPECT_NE(OVERLAY_shader_edit_uv_edges_get(), nullptr);
    EXPECT_NE(OVERLAY_shader_edit_uv_face_get(), nullptr);
    EXPECT_NE(OVERLAY_shader_edit_uv_face_dots_get(), nullptr);
    EXPECT_NE(OVERLAY_shader_edit_uv_verts_get(), nullptr);
    EXPECT_NE(OVERLAY_shader_edit_uv_stretching_area_get(), nullptr);
    EXPECT_NE(OVERLAY_shader_edit_uv_stretching_angle_get(), nullptr);
    EXPECT_NE(OVERLAY_shader_edit_uv_tiled_image_borders_get(), nullptr);
    EXPECT_NE(OVERLAY_shader_edit_uv_stencil_image(), nullptr);
    EXPECT_NE(OVERLAY_shader_edit_uv_mask_image(), nullptr);
    EXPECT_NE(OVERLAY_shader_extra(false), nullptr);
    EXPECT_NE(OVERLAY_shader_extra(true), nullptr);
    EXPECT_NE(OVERLAY_shader_extra_groundline(), nullptr);
    EXPECT_NE(OVERLAY_shader_extra_wire(false, false), nullptr);
    EXPECT_NE(OVERLAY_shader_extra_wire(false, true), nullptr);
    EXPECT_NE(OVERLAY_shader_extra_wire(true, false), nullptr);
    EXPECT_NE(OVERLAY_shader_extra_wire(true, true), nullptr);
    EXPECT_NE(OVERLAY_shader_extra_loose_point(), nullptr);
    EXPECT_NE(OVERLAY_shader_extra_point(), nullptr);
    EXPECT_NE(OVERLAY_shader_facing(), nullptr);
    EXPECT_NE(OVERLAY_shader_gpencil_canvas(), nullptr);
    EXPECT_NE(OVERLAY_shader_grid(), nullptr);
    EXPECT_NE(OVERLAY_shader_grid_image(), nullptr);
    EXPECT_NE(OVERLAY_shader_image(), nullptr);
    EXPECT_NE(OVERLAY_shader_motion_path_line(), nullptr);
    EXPECT_NE(OVERLAY_shader_motion_path_vert(), nullptr);
    EXPECT_NE(OVERLAY_shader_uniform_color(), nullptr);
    EXPECT_NE(OVERLAY_shader_outline_prepass(false), nullptr);
    EXPECT_NE(OVERLAY_shader_outline_prepass(true), nullptr);
    EXPECT_NE(OVERLAY_shader_outline_prepass_gpencil(), nullptr);
    EXPECT_NE(OVERLAY_shader_outline_prepass_pointcloud(), nullptr);
    EXPECT_NE(OVERLAY_shader_extra_grid(), nullptr);
    EXPECT_NE(OVERLAY_shader_outline_detect(), nullptr);
    EXPECT_NE(OVERLAY_shader_paint_face(), nullptr);
    EXPECT_NE(OVERLAY_shader_paint_point(), nullptr);
    EXPECT_NE(OVERLAY_shader_paint_texture(), nullptr);
    EXPECT_NE(OVERLAY_shader_paint_vertcol(), nullptr);
    EXPECT_NE(OVERLAY_shader_paint_weight(false), nullptr);
    EXPECT_NE(OVERLAY_shader_paint_weight(true), nullptr);
    EXPECT_NE(OVERLAY_shader_paint_wire(), nullptr);
    EXPECT_NE(OVERLAY_shader_particle_dot(), nullptr);
    EXPECT_NE(OVERLAY_shader_particle_shape(), nullptr);
    EXPECT_NE(OVERLAY_shader_sculpt_mask(), nullptr);
    EXPECT_NE(OVERLAY_shader_volume_velocity(false, false), nullptr);
    EXPECT_NE(OVERLAY_shader_volume_velocity(false, true), nullptr);
    EXPECT_NE(OVERLAY_shader_volume_velocity(true, false), nullptr);
    EXPECT_NE(OVERLAY_shader_wireframe(false), nullptr);
    EXPECT_NE(OVERLAY_shader_wireframe(true), nullptr);
    EXPECT_NE(OVERLAY_shader_wireframe_select(), nullptr);
    EXPECT_NE(OVERLAY_shader_xray_fade(), nullptr);
  }

  OVERLAY_shader_free();
}
DRAW_TEST(overlay_glsl_shaders)

static void test_eevee_glsl_shaders_static()
{
  EEVEE_shaders_material_shaders_init();

  EXPECT_NE(EEVEE_shaders_bloom_blit_get(false), nullptr);
  EXPECT_NE(EEVEE_shaders_bloom_blit_get(true), nullptr);
  EXPECT_NE(EEVEE_shaders_bloom_downsample_get(false), nullptr);
  EXPECT_NE(EEVEE_shaders_bloom_downsample_get(true), nullptr);
  EXPECT_NE(EEVEE_shaders_bloom_upsample_get(false), nullptr);
  EXPECT_NE(EEVEE_shaders_bloom_upsample_get(true), nullptr);
  EXPECT_NE(EEVEE_shaders_bloom_resolve_get(false), nullptr);
  EXPECT_NE(EEVEE_shaders_bloom_resolve_get(true), nullptr);
  EXPECT_NE(EEVEE_shaders_depth_of_field_bokeh_get(), nullptr);
  EXPECT_NE(EEVEE_shaders_depth_of_field_setup_get(), nullptr);
  EXPECT_NE(EEVEE_shaders_depth_of_field_flatten_tiles_get(), nullptr);
  EXPECT_NE(EEVEE_shaders_depth_of_field_dilate_tiles_get(false), nullptr);
  EXPECT_NE(EEVEE_shaders_depth_of_field_dilate_tiles_get(true), nullptr);
  EXPECT_NE(EEVEE_shaders_depth_of_field_downsample_get(), nullptr);
  EXPECT_NE(EEVEE_shaders_depth_of_field_reduce_get(true), nullptr);
  EXPECT_NE(EEVEE_shaders_depth_of_field_reduce_get(false), nullptr);
  EXPECT_NE(EEVEE_shaders_depth_of_field_gather_get(DOF_GATHER_FOREGROUND, false), nullptr);
  EXPECT_NE(EEVEE_shaders_depth_of_field_gather_get(DOF_GATHER_FOREGROUND, true), nullptr);
  EXPECT_NE(EEVEE_shaders_depth_of_field_gather_get(DOF_GATHER_BACKGROUND, false), nullptr);
  EXPECT_NE(EEVEE_shaders_depth_of_field_gather_get(DOF_GATHER_BACKGROUND, true), nullptr);
  EXPECT_NE(EEVEE_shaders_depth_of_field_gather_get(DOF_GATHER_HOLEFILL, false), nullptr);
  EXPECT_NE(EEVEE_shaders_depth_of_field_gather_get(DOF_GATHER_HOLEFILL, true), nullptr);
  EXPECT_NE(EEVEE_shaders_depth_of_field_filter_get(), nullptr);
  EXPECT_NE(EEVEE_shaders_depth_of_field_scatter_get(false, false), nullptr);
  EXPECT_NE(EEVEE_shaders_depth_of_field_scatter_get(false, true), nullptr);
  EXPECT_NE(EEVEE_shaders_depth_of_field_scatter_get(true, false), nullptr);
  EXPECT_NE(EEVEE_shaders_depth_of_field_scatter_get(true, true), nullptr);
  EXPECT_NE(EEVEE_shaders_depth_of_field_resolve_get(false, true), nullptr);
  EXPECT_NE(EEVEE_shaders_depth_of_field_resolve_get(false, false), nullptr);
  EXPECT_NE(EEVEE_shaders_depth_of_field_resolve_get(true, true), nullptr);
  EXPECT_NE(EEVEE_shaders_depth_of_field_resolve_get(true, false), nullptr);
  EXPECT_NE(EEVEE_shaders_effect_downsample_sh_get(), nullptr);
  EXPECT_NE(EEVEE_shaders_effect_downsample_cube_sh_get(), nullptr);
  EXPECT_NE(EEVEE_shaders_effect_minz_downlevel_sh_get(), nullptr);
  EXPECT_NE(EEVEE_shaders_effect_maxz_downlevel_sh_get(), nullptr);
  EXPECT_NE(EEVEE_shaders_effect_minz_downdepth_sh_get(), nullptr);
  EXPECT_NE(EEVEE_shaders_effect_maxz_downdepth_sh_get(), nullptr);
  EXPECT_NE(EEVEE_shaders_effect_minz_downdepth_layer_sh_get(), nullptr);
  EXPECT_NE(EEVEE_shaders_effect_maxz_downdepth_layer_sh_get(), nullptr);
  EXPECT_NE(EEVEE_shaders_effect_maxz_copydepth_layer_sh_get(), nullptr);
  EXPECT_NE(EEVEE_shaders_effect_minz_copydepth_sh_get(), nullptr);
  EXPECT_NE(EEVEE_shaders_effect_maxz_copydepth_sh_get(), nullptr);
  EXPECT_NE(EEVEE_shaders_effect_mist_sh_get(), nullptr);
  EXPECT_NE(EEVEE_shaders_effect_motion_blur_sh_get(), nullptr);
  EXPECT_NE(EEVEE_shaders_effect_motion_blur_object_sh_get(), nullptr);
  EXPECT_NE(EEVEE_shaders_effect_motion_blur_hair_sh_get(), nullptr);
  EXPECT_NE(EEVEE_shaders_effect_motion_blur_velocity_tiles_sh_get(), nullptr);
  EXPECT_NE(EEVEE_shaders_effect_motion_blur_velocity_tiles_expand_sh_get(), nullptr);
  EXPECT_NE(EEVEE_shaders_effect_ambient_occlusion_sh_get(), nullptr);
  EXPECT_NE(EEVEE_shaders_effect_ambient_occlusion_debug_sh_get(), nullptr);
  EXPECT_NE(EEVEE_shaders_ggx_lut_sh_get(), nullptr);
  EXPECT_NE(EEVEE_shaders_ggx_refraction_lut_sh_get(), nullptr);
  EXPECT_NE(EEVEE_shaders_probe_filter_glossy_sh_get(), nullptr);
  EXPECT_NE(EEVEE_shaders_probe_filter_diffuse_sh_get(), nullptr);
  EXPECT_NE(EEVEE_shaders_probe_filter_visibility_sh_get(), nullptr);
  EXPECT_NE(EEVEE_shaders_probe_grid_fill_sh_get(), nullptr);
  EXPECT_NE(EEVEE_shaders_probe_planar_downsample_sh_get(), nullptr);
  EXPECT_NE(EEVEE_shaders_renderpasses_post_process_sh_get(), nullptr);
  EXPECT_NE(EEVEE_shaders_cryptomatte_sh_get(false), nullptr);
  EXPECT_NE(EEVEE_shaders_cryptomatte_sh_get(true), nullptr);
  EXPECT_NE(EEVEE_shaders_shadow_sh_get(), nullptr);
  EXPECT_NE(EEVEE_shaders_shadow_accum_sh_get(), nullptr);
  EXPECT_NE(EEVEE_shaders_subsurface_first_pass_sh_get(), nullptr);
  EXPECT_NE(EEVEE_shaders_subsurface_second_pass_sh_get(), nullptr);
  EXPECT_NE(EEVEE_shaders_volumes_clear_sh_get(), nullptr);
  EXPECT_NE(EEVEE_shaders_volumes_clear_sh_get(), nullptr);
  EXPECT_NE(EEVEE_shaders_volumes_scatter_sh_get(), nullptr);
  EXPECT_NE(EEVEE_shaders_volumes_scatter_with_lights_sh_get(), nullptr);
  EXPECT_NE(EEVEE_shaders_volumes_integration_sh_get(), nullptr);
  EXPECT_NE(EEVEE_shaders_volumes_resolve_sh_get(false), nullptr);
  EXPECT_NE(EEVEE_shaders_volumes_resolve_sh_get(true), nullptr);
  EXPECT_NE(EEVEE_shaders_volumes_accum_sh_get(), nullptr);
  EXPECT_NE(EEVEE_shaders_studiolight_probe_sh_get(), nullptr);
  EXPECT_NE(EEVEE_shaders_studiolight_background_sh_get(), nullptr);
  EXPECT_NE(EEVEE_shaders_probe_cube_display_sh_get(), nullptr);
  EXPECT_NE(EEVEE_shaders_probe_grid_display_sh_get(), nullptr);
  EXPECT_NE(EEVEE_shaders_probe_planar_display_sh_get(), nullptr);
  EXPECT_NE(EEVEE_shaders_update_noise_sh_get(), nullptr);
  EXPECT_NE(EEVEE_shaders_velocity_resolve_sh_get(), nullptr);
  EXPECT_NE(EEVEE_shaders_taa_resolve_sh_get(EFFECT_TAA), nullptr);
  EXPECT_NE(EEVEE_shaders_taa_resolve_sh_get(EFFECT_TAA_REPROJECT), nullptr);
  EXPECT_NE(EEVEE_shaders_effect_reflection_trace_sh_get(), nullptr);
  EXPECT_NE(EEVEE_shaders_effect_reflection_resolve_sh_get(), nullptr);
  EEVEE_shaders_free();
}
DRAW_TEST(eevee_glsl_shaders_static)

static void test_draw_shaders(eParticleRefineShaderType sh_type)
{
  DRW_shaders_free();
  EXPECT_NE(DRW_shader_hair_refine_get(PART_REFINE_CATMULL_ROM, sh_type), nullptr);
  DRW_shaders_free();
}

static void test_draw_glsl_shaders()
{
#ifndef __APPLE__
  test_draw_shaders(PART_REFINE_SHADER_TRANSFORM_FEEDBACK);
  test_draw_shaders(PART_REFINE_SHADER_COMPUTE);
#endif
  test_draw_shaders(PART_REFINE_SHADER_TRANSFORM_FEEDBACK_WORKAROUND);
}
DRAW_TEST(draw_glsl_shaders)

static void test_basic_glsl_shaders()
{
  for (int i = 0; i < GPU_SHADER_CFG_LEN; i++) {
    eGPUShaderConfig sh_cfg = static_cast<eGPUShaderConfig>(i);
    BASIC_shaders_depth_sh_get(sh_cfg);
    BASIC_shaders_pointcloud_depth_sh_get(sh_cfg);
    BASIC_shaders_depth_conservative_sh_get(sh_cfg);
    BASIC_shaders_pointcloud_depth_conservative_sh_get(sh_cfg);
  }
  BASIC_shaders_free();
}
DRAW_TEST(basic_glsl_shaders)

}  // namespace blender::draw
