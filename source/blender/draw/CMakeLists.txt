# ***** BEGIN GPL LICENSE BLOCK *****
#
# This program is free software; you can redistribute it and/or
# modify it under the terms of the GNU General Public License
# as published by the Free Software Foundation; either version 2
# of the License, or (at your option) any later version.
#
# This program is distributed in the hope that it will be useful,
# but WITHOUT ANY WARRANTY; without even the implied warranty of
# MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
# GNU General Public License for more details.
#
# You should have received a copy of the GNU General Public License
# along with this program; if not, write to the Free Software Foundation,
# Inc., 51 Franklin Street, Fifth Floor, Boston, MA 02110-1301, USA.
#
# The Original Code is Copyright (C) 2016, Blender Foundation
# All rights reserved.
# ***** END GPL LICENSE BLOCK *****

set(INC
  .
  intern
  modes

  ../blenfont
  ../blenkernel
  ../blenlib
  ../blentranslation
  ../bmesh
  ../depsgraph
  ../editors/include
  ../editors/space_view3d
  ../gpu
  ../imbuf
  ../makesdna
  ../makesrna
  ../render/extern/include
  ../render/intern/include
  ../windowmanager

  ../../../intern/atomic
  ../../../intern/glew-mx
  ../../../intern/guardedalloc
)

set(INC_SYS
  ${GLEW_INCLUDE_PATH}
)

set(SRC
  intern/draw_anim_viz.c
  intern/draw_armature.c
  intern/draw_cache.c
  intern/draw_cache_impl_curve.c
  intern/draw_cache_impl_displist.c
  intern/draw_cache_impl_lattice.c
  intern/draw_cache_impl_mesh.c
  intern/draw_cache_impl_metaball.c
  intern/draw_cache_impl_particles.c
  intern/draw_common.c
  intern/draw_debug.c
  intern/draw_hair.c
  intern/draw_instance_data.c
  intern/draw_manager.c
  intern/draw_manager_data.c
  intern/draw_manager_exec.c
  intern/draw_manager_profiling.c
  intern/draw_manager_shader.c
  intern/draw_manager_text.c
  intern/draw_manager_texture.c
  intern/draw_view.c
  modes/edit_armature_mode.c
  modes/edit_curve_mode.c
  modes/edit_lattice_mode.c
  modes/edit_mesh_mode.c
  modes/edit_mesh_mode_text.c
  modes/edit_metaball_mode.c
  modes/edit_text_mode.c
  modes/object_mode.c
  modes/overlay_mode.c
  modes/paint_texture_mode.c
  modes/paint_vertex_mode.c
  modes/particle_mode.c
  modes/pose_mode.c
  modes/sculpt_mode.c
  engines/basic/basic_engine.c
  engines/eevee/eevee_bloom.c
  engines/eevee/eevee_data.c
  engines/eevee/eevee_depth_of_field.c
  engines/eevee/eevee_effects.c
  engines/eevee/eevee_engine.c
  engines/eevee/eevee_lightcache.c
  engines/eevee/eevee_lightprobes.c
  engines/eevee/eevee_lights.c
  engines/eevee/eevee_lookdev.c
  engines/eevee/eevee_materials.c
  engines/eevee/eevee_mist.c
  engines/eevee/eevee_motion_blur.c
  engines/eevee/eevee_occlusion.c
  engines/eevee/eevee_render.c
  engines/eevee/eevee_screen_raytrace.c
  engines/eevee/eevee_shaders.c
  engines/eevee/eevee_subsurface.c
  engines/eevee/eevee_temporal_sampling.c
  engines/eevee/eevee_volumes.c
  engines/workbench/solid_mode.c
  engines/workbench/transparent_mode.c
  engines/workbench/workbench_data.c
  engines/workbench/workbench_deferred.c
  engines/workbench/workbench_effect_aa.c
  engines/workbench/workbench_effect_dof.c
  engines/workbench/workbench_effect_fxaa.c
  engines/workbench/workbench_effect_taa.c
  engines/workbench/workbench_engine.c
  engines/workbench/workbench_forward.c
  engines/workbench/workbench_materials.c
  engines/workbench/workbench_render.c
  engines/workbench/workbench_studiolight.c
  engines/workbench/workbench_volume.c
  engines/external/external_engine.c
  engines/gpencil/gpencil_cache_utils.c
  engines/gpencil/gpencil_draw_cache_impl.c
  engines/gpencil/gpencil_draw_utils.c
  engines/gpencil/gpencil_engine.c
  engines/gpencil/gpencil_engine.h
  engines/gpencil/gpencil_render.c
  engines/gpencil/gpencil_shader_fx.c
  engines/select/select_engine.c
  engines/select/select_draw_utils.c

  DRW_engine.h
  intern/DRW_render.h
  intern/draw_cache.h
  intern/draw_cache_impl.h
  intern/draw_cache_inline.h
  intern/draw_common.h
  intern/draw_debug.h
  intern/draw_hair_private.h
  intern/draw_instance_data.h
  intern/draw_manager.h
  intern/draw_manager_profiling.h
  intern/draw_manager_text.h
  intern/draw_view.h
  modes/draw_mode_engines.h
  modes/edit_mesh_mode_intern.h
  engines/basic/basic_engine.h
  engines/eevee/eevee_engine.h
  engines/eevee/eevee_lightcache.h
  engines/eevee/eevee_lut.h
  engines/eevee/eevee_private.h
  engines/external/external_engine.h
  engines/workbench/workbench_engine.h
  engines/workbench/workbench_private.h
  engines/select/select_engine.h
  engines/select/select_private.h
)

set(LIB
  bf_blenkernel
  bf_blenlib
  bf_windowmanager
)

data_to_c_simple(engines/eevee/shaders/ambient_occlusion_lib.glsl SRC)
data_to_c_simple(engines/eevee/shaders/default_frag.glsl SRC)
data_to_c_simple(engines/eevee/shaders/default_world_frag.glsl SRC)
data_to_c_simple(engines/eevee/shaders/background_vert.glsl SRC)
data_to_c_simple(engines/eevee/shaders/concentric_samples_lib.glsl SRC)
data_to_c_simple(engines/eevee/shaders/common_uniforms_lib.glsl SRC)
data_to_c_simple(engines/eevee/shaders/lights_lib.glsl SRC)
data_to_c_simple(engines/eevee/shaders/lightprobe_lib.glsl SRC)
data_to_c_simple(engines/eevee/shaders/lightprobe_filter_glossy_frag.glsl SRC)
data_to_c_simple(engines/eevee/shaders/lightprobe_filter_diffuse_frag.glsl SRC)
data_to_c_simple(engines/eevee/shaders/lightprobe_filter_visibility_frag.glsl SRC)
data_to_c_simple(engines/eevee/shaders/lightprobe_geom.glsl SRC)
data_to_c_simple(engines/eevee/shaders/lightprobe_vert.glsl SRC)
data_to_c_simple(engines/eevee/shaders/lightprobe_cube_display_frag.glsl SRC)
data_to_c_simple(engines/eevee/shaders/lightprobe_cube_display_vert.glsl SRC)
data_to_c_simple(engines/eevee/shaders/lightprobe_grid_display_frag.glsl SRC)
data_to_c_simple(engines/eevee/shaders/lightprobe_grid_display_vert.glsl SRC)
data_to_c_simple(engines/eevee/shaders/lightprobe_grid_fill_frag.glsl SRC)
data_to_c_simple(engines/eevee/shaders/lightprobe_planar_display_frag.glsl SRC)
data_to_c_simple(engines/eevee/shaders/lightprobe_planar_display_vert.glsl SRC)
data_to_c_simple(engines/eevee/shaders/lit_surface_frag.glsl SRC)
data_to_c_simple(engines/eevee/shaders/lit_surface_vert.glsl SRC)
data_to_c_simple(engines/eevee/shaders/effect_bloom_frag.glsl SRC)
data_to_c_simple(engines/eevee/shaders/effect_dof_vert.glsl SRC)
data_to_c_simple(engines/eevee/shaders/effect_dof_frag.glsl SRC)
data_to_c_simple(engines/eevee/shaders/effect_downsample_frag.glsl SRC)
data_to_c_simple(engines/eevee/shaders/effect_downsample_cube_frag.glsl SRC)
data_to_c_simple(engines/eevee/shaders/effect_gtao_frag.glsl SRC)
data_to_c_simple(engines/eevee/shaders/effect_velocity_resolve_frag.glsl SRC)
data_to_c_simple(engines/eevee/shaders/effect_minmaxz_frag.glsl SRC)
data_to_c_simple(engines/eevee/shaders/effect_mist_frag.glsl SRC)
data_to_c_simple(engines/eevee/shaders/effect_motion_blur_frag.glsl SRC)
data_to_c_simple(engines/eevee/shaders/effect_ssr_frag.glsl SRC)
data_to_c_simple(engines/eevee/shaders/effect_subsurface_frag.glsl SRC)
data_to_c_simple(engines/eevee/shaders/effect_temporal_aa.glsl SRC)
data_to_c_simple(engines/eevee/shaders/lightprobe_planar_downsample_frag.glsl SRC)
data_to_c_simple(engines/eevee/shaders/lightprobe_planar_downsample_geom.glsl SRC)
data_to_c_simple(engines/eevee/shaders/lightprobe_planar_downsample_vert.glsl SRC)
data_to_c_simple(engines/eevee/shaders/prepass_frag.glsl SRC)
data_to_c_simple(engines/eevee/shaders/prepass_vert.glsl SRC)
data_to_c_simple(engines/eevee/shaders/shadow_frag.glsl SRC)
data_to_c_simple(engines/eevee/shaders/shadow_vert.glsl SRC)
data_to_c_simple(engines/eevee/shaders/shadow_process_vert.glsl SRC)
data_to_c_simple(engines/eevee/shaders/shadow_process_geom.glsl SRC)
data_to_c_simple(engines/eevee/shaders/shadow_store_frag.glsl SRC)
data_to_c_simple(engines/eevee/shaders/shadow_copy_frag.glsl SRC)
data_to_c_simple(engines/eevee/shaders/bsdf_lut_frag.glsl SRC)
data_to_c_simple(engines/eevee/shaders/btdf_lut_frag.glsl SRC)
data_to_c_simple(engines/eevee/shaders/bsdf_common_lib.glsl SRC)
data_to_c_simple(engines/eevee/shaders/irradiance_lib.glsl SRC)
data_to_c_simple(engines/eevee/shaders/octahedron_lib.glsl SRC)
data_to_c_simple(engines/eevee/shaders/bsdf_sampling_lib.glsl SRC)
data_to_c_simple(engines/eevee/shaders/raytrace_lib.glsl SRC)
data_to_c_simple(engines/eevee/shaders/ltc_lib.glsl SRC)
data_to_c_simple(engines/eevee/shaders/ssr_lib.glsl SRC)
data_to_c_simple(engines/eevee/shaders/update_noise_frag.glsl SRC)
data_to_c_simple(engines/eevee/shaders/volumetric_lib.glsl SRC)
data_to_c_simple(engines/eevee/shaders/volumetric_frag.glsl SRC)
data_to_c_simple(engines/eevee/shaders/volumetric_geom.glsl SRC)
data_to_c_simple(engines/eevee/shaders/volumetric_vert.glsl SRC)
data_to_c_simple(engines/eevee/shaders/volumetric_resolve_frag.glsl SRC)
data_to_c_simple(engines/eevee/shaders/volumetric_scatter_frag.glsl SRC)
data_to_c_simple(engines/eevee/shaders/volumetric_integration_frag.glsl SRC)

data_to_c_simple(engines/workbench/shaders/workbench_background_lib.glsl SRC)
data_to_c_simple(engines/workbench/shaders/workbench_cavity_lib.glsl SRC)
data_to_c_simple(engines/workbench/shaders/workbench_cavity_frag.glsl SRC)
data_to_c_simple(engines/workbench/shaders/workbench_checkerboard_depth_frag.glsl SRC)
data_to_c_simple(engines/workbench/shaders/workbench_common_lib.glsl SRC)
data_to_c_simple(engines/workbench/shaders/workbench_data_lib.glsl SRC)
data_to_c_simple(engines/workbench/shaders/workbench_deferred_composite_frag.glsl SRC)
data_to_c_simple(engines/workbench/shaders/workbench_deferred_background_frag.glsl SRC)
data_to_c_simple(engines/workbench/shaders/workbench_effect_dof_frag.glsl SRC)
data_to_c_simple(engines/workbench/shaders/workbench_effect_fxaa_frag.glsl SRC)
data_to_c_simple(engines/workbench/shaders/workbench_effect_taa_frag.glsl SRC)
data_to_c_simple(engines/workbench/shaders/workbench_forward_composite_frag.glsl SRC)
data_to_c_simple(engines/workbench/shaders/workbench_forward_depth_frag.glsl SRC)
data_to_c_simple(engines/workbench/shaders/workbench_forward_transparent_accum_frag.glsl SRC)
data_to_c_simple(engines/workbench/shaders/workbench_ghost_resolve_frag.glsl SRC)
data_to_c_simple(engines/workbench/shaders/workbench_object_outline_lib.glsl SRC)
data_to_c_simple(engines/workbench/shaders/workbench_curvature_lib.glsl SRC)
data_to_c_simple(engines/workbench/shaders/workbench_prepass_vert.glsl SRC)
data_to_c_simple(engines/workbench/shaders/workbench_prepass_frag.glsl SRC)
data_to_c_simple(engines/workbench/shaders/workbench_shadow_vert.glsl SRC)
data_to_c_simple(engines/workbench/shaders/workbench_shadow_geom.glsl SRC)
data_to_c_simple(engines/workbench/shaders/workbench_shadow_caps_geom.glsl SRC)
data_to_c_simple(engines/workbench/shaders/workbench_shadow_debug_frag.glsl SRC)
data_to_c_simple(engines/workbench/shaders/workbench_volume_vert.glsl SRC)
data_to_c_simple(engines/workbench/shaders/workbench_volume_frag.glsl SRC)
data_to_c_simple(engines/workbench/shaders/workbench_world_light_lib.glsl SRC)

data_to_c_simple(modes/shaders/common_colormanagement_lib.glsl SRC)
data_to_c_simple(modes/shaders/common_globals_lib.glsl SRC)
data_to_c_simple(modes/shaders/common_hair_lib.glsl SRC)
data_to_c_simple(modes/shaders/common_hair_refine_vert.glsl SRC)
data_to_c_simple(modes/shaders/common_view_lib.glsl SRC)
data_to_c_simple(modes/shaders/common_fxaa_lib.glsl SRC)
data_to_c_simple(modes/shaders/common_fullscreen_vert.glsl SRC)
data_to_c_simple(modes/shaders/animviz_mpath_lines_vert.glsl SRC)
data_to_c_simple(modes/shaders/animviz_mpath_lines_geom.glsl SRC)
data_to_c_simple(modes/shaders/animviz_mpath_points_vert.glsl SRC)
data_to_c_simple(modes/shaders/armature_axes_vert.glsl SRC)
data_to_c_simple(modes/shaders/armature_sphere_solid_vert.glsl SRC)
data_to_c_simple(modes/shaders/armature_sphere_solid_frag.glsl SRC)
data_to_c_simple(modes/shaders/armature_sphere_outline_vert.glsl SRC)
data_to_c_simple(modes/shaders/armature_envelope_solid_vert.glsl SRC)
data_to_c_simple(modes/shaders/armature_envelope_solid_frag.glsl SRC)
data_to_c_simple(modes/shaders/armature_envelope_outline_vert.glsl SRC)
data_to_c_simple(modes/shaders/armature_envelope_distance_frag.glsl SRC)
data_to_c_simple(modes/shaders/armature_shape_solid_vert.glsl SRC)
data_to_c_simple(modes/shaders/armature_shape_solid_frag.glsl SRC)
data_to_c_simple(modes/shaders/armature_shape_outline_vert.glsl SRC)
data_to_c_simple(modes/shaders/armature_shape_outline_geom.glsl SRC)
data_to_c_simple(modes/shaders/armature_stick_vert.glsl SRC)
data_to_c_simple(modes/shaders/armature_stick_frag.glsl SRC)
data_to_c_simple(modes/shaders/armature_dof_vert.glsl SRC)
data_to_c_simple(modes/shaders/edit_mesh_overlay_common_lib.glsl SRC)
data_to_c_simple(modes/shaders/edit_mesh_overlay_frag.glsl SRC)
data_to_c_simple(modes/shaders/edit_mesh_overlay_vert.glsl SRC)
data_to_c_simple(modes/shaders/edit_mesh_overlay_geom.glsl SRC)
data_to_c_simple(modes/shaders/edit_mesh_overlay_mix_frag.glsl SRC)
data_to_c_simple(modes/shaders/edit_mesh_overlay_facefill_vert.glsl SRC)
data_to_c_simple(modes/shaders/edit_mesh_overlay_facefill_frag.glsl SRC)
data_to_c_simple(modes/shaders/edit_mesh_overlay_mesh_analysis_frag.glsl SRC)
data_to_c_simple(modes/shaders/edit_mesh_overlay_mesh_analysis_vert.glsl SRC)
data_to_c_simple(modes/shaders/edit_curve_overlay_handle_vert.glsl SRC)
data_to_c_simple(modes/shaders/edit_curve_overlay_handle_geom.glsl SRC)
data_to_c_simple(modes/shaders/edit_curve_overlay_loosevert_vert.glsl SRC)
data_to_c_simple(modes/shaders/edit_curve_overlay_normals_vert.glsl SRC)
data_to_c_simple(modes/shaders/edit_lattice_overlay_frag.glsl SRC)
data_to_c_simple(modes/shaders/edit_lattice_overlay_loosevert_vert.glsl SRC)
data_to_c_simple(modes/shaders/edit_normals_vert.glsl SRC)
data_to_c_simple(modes/shaders/edit_normals_geom.glsl SRC)
data_to_c_simple(modes/shaders/overlay_face_orientation_frag.glsl SRC)
data_to_c_simple(modes/shaders/overlay_face_orientation_vert.glsl SRC)
data_to_c_simple(modes/shaders/overlay_face_wireframe_vert.glsl SRC)
data_to_c_simple(modes/shaders/overlay_face_wireframe_geom.glsl SRC)
data_to_c_simple(modes/shaders/overlay_face_wireframe_frag.glsl SRC)
data_to_c_simple(modes/shaders/object_camera_image_frag.glsl SRC)
data_to_c_simple(modes/shaders/object_camera_image_vert.glsl SRC)
data_to_c_simple(modes/shaders/object_empty_axes_vert.glsl SRC)
data_to_c_simple(modes/shaders/object_empty_image_frag.glsl SRC)
data_to_c_simple(modes/shaders/object_empty_image_vert.glsl SRC)
data_to_c_simple(modes/shaders/object_outline_resolve_frag.glsl SRC)
data_to_c_simple(modes/shaders/object_outline_expand_frag.glsl SRC)
data_to_c_simple(modes/shaders/object_outline_detect_frag.glsl SRC)
data_to_c_simple(modes/shaders/object_outline_prepass_vert.glsl SRC)
data_to_c_simple(modes/shaders/object_outline_prepass_geom.glsl SRC)
data_to_c_simple(modes/shaders/object_outline_prepass_frag.glsl SRC)
data_to_c_simple(modes/shaders/object_grid_frag.glsl SRC)
data_to_c_simple(modes/shaders/object_grid_vert.glsl SRC)
data_to_c_simple(modes/shaders/object_lightprobe_grid_vert.glsl SRC)
data_to_c_simple(modes/shaders/object_mball_handles_vert.glsl SRC)
data_to_c_simple(modes/shaders/object_particle_prim_vert.glsl SRC)
data_to_c_simple(modes/shaders/object_particle_dot_vert.glsl SRC)
data_to_c_simple(modes/shaders/object_particle_dot_frag.glsl SRC)
data_to_c_simple(modes/shaders/object_loose_points_frag.glsl SRC)
data_to_c_simple(modes/shaders/paint_texture_frag.glsl SRC)
data_to_c_simple(modes/shaders/paint_texture_vert.glsl SRC)
data_to_c_simple(modes/shaders/paint_vertex_frag.glsl SRC)
data_to_c_simple(modes/shaders/paint_vertex_vert.glsl SRC)
data_to_c_simple(modes/shaders/paint_weight_frag.glsl SRC)
data_to_c_simple(modes/shaders/paint_weight_vert.glsl SRC)
data_to_c_simple(modes/shaders/paint_face_selection_vert.glsl SRC)
data_to_c_simple(modes/shaders/paint_face_vert.glsl SRC)
data_to_c_simple(modes/shaders/paint_wire_frag.glsl SRC)
data_to_c_simple(modes/shaders/paint_wire_vert.glsl SRC)
data_to_c_simple(modes/shaders/paint_vert_frag.glsl SRC)
data_to_c_simple(modes/shaders/particle_strand_frag.glsl SRC)
data_to_c_simple(modes/shaders/particle_strand_vert.glsl SRC)
data_to_c_simple(modes/shaders/pose_selection_vert.glsl SRC)
data_to_c_simple(modes/shaders/sculpt_mask_vert.glsl SRC)
data_to_c_simple(modes/shaders/volume_velocity_vert.glsl SRC)

data_to_c_simple(engines/gpencil/shaders/gpencil_fill_vert.glsl SRC)
data_to_c_simple(engines/gpencil/shaders/gpencil_fill_frag.glsl SRC)
data_to_c_simple(engines/gpencil/shaders/gpencil_stroke_vert.glsl SRC)
data_to_c_simple(engines/gpencil/shaders/gpencil_stroke_geom.glsl SRC)
data_to_c_simple(engines/gpencil/shaders/gpencil_stroke_frag.glsl SRC)
data_to_c_simple(engines/gpencil/shaders/gpencil_zdepth_mix_frag.glsl SRC)
data_to_c_simple(engines/gpencil/shaders/gpencil_simple_mix_frag.glsl SRC)
data_to_c_simple(engines/gpencil/shaders/gpencil_blend_frag.glsl SRC)
data_to_c_simple(engines/gpencil/shaders/gpencil_point_vert.glsl SRC)
data_to_c_simple(engines/gpencil/shaders/gpencil_point_geom.glsl SRC)
data_to_c_simple(engines/gpencil/shaders/gpencil_point_frag.glsl SRC)
data_to_c_simple(engines/gpencil/shaders/gpencil_background_frag.glsl SRC)
data_to_c_simple(engines/gpencil/shaders/gpencil_paper_frag.glsl SRC)
data_to_c_simple(engines/gpencil/shaders/gpencil_edit_point_vert.glsl SRC)
data_to_c_simple(engines/gpencil/shaders/gpencil_edit_point_geom.glsl SRC)
data_to_c_simple(engines/gpencil/shaders/gpencil_edit_point_frag.glsl SRC)

data_to_c_simple(engines/gpencil/shaders/fx/gpencil_fx_blur_frag.glsl SRC)
data_to_c_simple(engines/gpencil/shaders/fx/gpencil_fx_colorize_frag.glsl SRC)
data_to_c_simple(engines/gpencil/shaders/fx/gpencil_fx_flip_frag.glsl SRC)
data_to_c_simple(engines/gpencil/shaders/fx/gpencil_fx_glow_prepare_frag.glsl SRC)
data_to_c_simple(engines/gpencil/shaders/fx/gpencil_fx_glow_resolve_frag.glsl SRC)
data_to_c_simple(engines/gpencil/shaders/fx/gpencil_fx_light_frag.glsl SRC)
data_to_c_simple(engines/gpencil/shaders/fx/gpencil_fx_pixel_frag.glsl SRC)
data_to_c_simple(engines/gpencil/shaders/fx/gpencil_fx_rim_prepare_frag.glsl SRC)
data_to_c_simple(engines/gpencil/shaders/fx/gpencil_fx_rim_resolve_frag.glsl SRC)
data_to_c_simple(engines/gpencil/shaders/fx/gpencil_fx_shadow_prepare_frag.glsl SRC)
data_to_c_simple(engines/gpencil/shaders/fx/gpencil_fx_shadow_resolve_frag.glsl SRC)
data_to_c_simple(engines/gpencil/shaders/fx/gpencil_fx_swirl_frag.glsl SRC)
data_to_c_simple(engines/gpencil/shaders/fx/gpencil_fx_wave_frag.glsl SRC)

data_to_c_simple(engines/select/shaders/selection_id_3D_vert.glsl SRC)
data_to_c_simple(engines/select/shaders/selection_id_frag.glsl SRC)


list(APPEND INC
)

if(WITH_FREESTYLE)
  add_definitions(-DWITH_FREESTYLE)
endif()

blender_add_lib(bf_draw "${SRC}" "${INC}" "${INC_SYS}" "${LIB}")
