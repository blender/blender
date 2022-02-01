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

  ../blenfont
  ../blenkernel
  ../blenlib
  ../blenloader
  ../blentranslation
  ../bmesh
  ../depsgraph
  ../editors/include
  ../editors/space_view3d
  ../functions
  ../gpu
  ../imbuf
  ../makesdna
  ../makesrna
  ../nodes
  ../render
  ../render/intern
  ../windowmanager

  ../../../intern/atomic
  ../../../intern/glew-mx
  ../../../intern/guardedalloc
  ../../../intern/opensubdiv
  ../../../intern/clog

  # dna_type_offsets.h
  ${CMAKE_CURRENT_BINARY_DIR}/../makesdna/intern
  ${OPENSUBDIV_INCLUDE_DIRS}
)

set(SRC
  intern/draw_cache.c
  intern/draw_cache_extract_mesh.cc
  intern/draw_cache_extract_mesh_render_data.c
  intern/mesh_extractors/extract_mesh.c
  intern/mesh_extractors/extract_mesh_ibo_edituv.cc
  intern/mesh_extractors/extract_mesh_ibo_fdots.cc
  intern/mesh_extractors/extract_mesh_ibo_lines.cc
  intern/mesh_extractors/extract_mesh_ibo_lines_adjacency.cc
  intern/mesh_extractors/extract_mesh_ibo_lines_paint_mask.cc
  intern/mesh_extractors/extract_mesh_ibo_points.cc
  intern/mesh_extractors/extract_mesh_ibo_tris.cc
  intern/mesh_extractors/extract_mesh_vbo_attributes.cc
  intern/mesh_extractors/extract_mesh_vbo_edge_fac.cc
  intern/mesh_extractors/extract_mesh_vbo_edit_data.cc
  intern/mesh_extractors/extract_mesh_vbo_edituv_data.cc
  intern/mesh_extractors/extract_mesh_vbo_edituv_stretch_angle.cc
  intern/mesh_extractors/extract_mesh_vbo_edituv_stretch_area.cc
  intern/mesh_extractors/extract_mesh_vbo_fdots_edituv_data.cc
  intern/mesh_extractors/extract_mesh_vbo_fdots_nor.cc
  intern/mesh_extractors/extract_mesh_vbo_fdots_pos.cc
  intern/mesh_extractors/extract_mesh_vbo_fdots_uv.cc
  intern/mesh_extractors/extract_mesh_vbo_lnor.cc
  intern/mesh_extractors/extract_mesh_vbo_mesh_analysis.cc
  intern/mesh_extractors/extract_mesh_vbo_orco.cc
  intern/mesh_extractors/extract_mesh_vbo_pos_nor.cc
  intern/mesh_extractors/extract_mesh_vbo_sculpt_data.cc
  intern/mesh_extractors/extract_mesh_vbo_select_idx.cc
  intern/mesh_extractors/extract_mesh_vbo_skin_roots.cc
  intern/mesh_extractors/extract_mesh_vbo_tan.cc
  intern/mesh_extractors/extract_mesh_vbo_uv.cc
  intern/mesh_extractors/extract_mesh_vbo_vcol.cc
  intern/mesh_extractors/extract_mesh_vbo_weights.cc
  intern/draw_cache_impl_curve.cc
  intern/draw_cache_impl_displist.c
  intern/draw_cache_impl_gpencil.c
  intern/draw_cache_impl_hair.cc
  intern/draw_cache_impl_lattice.c
  intern/draw_cache_impl_mesh.c
  intern/draw_cache_impl_metaball.c
  intern/draw_cache_impl_particles.c
  intern/draw_cache_impl_pointcloud.c
  intern/draw_cache_impl_subdivision.cc
  intern/draw_cache_impl_volume.c
  intern/draw_color_management.cc
  intern/draw_common.c
  intern/draw_debug.c
  intern/draw_fluid.c
  intern/draw_hair.c
  intern/draw_instance_data.c
  intern/draw_manager.c
  intern/draw_manager_data.c
  intern/draw_manager_exec.c
  intern/draw_manager_profiling.c
  intern/draw_manager_shader.c
  intern/draw_manager_text.c
  intern/draw_manager_texture.c
  intern/draw_select_buffer.c
  intern/draw_shader.c
  intern/draw_texture_pool.cc
  intern/draw_view.c
  intern/draw_view_data.cc
  intern/smaa_textures.c
  engines/basic/basic_engine.c
  engines/basic/basic_shader.c
  engines/image/image_engine.cc
  engines/image/image_shader.cc
  engines/eevee/eevee_bloom.c
  engines/eevee/eevee_cryptomatte.c
  engines/eevee/eevee_data.c
  engines/eevee/eevee_depth_of_field.c
  engines/eevee/eevee_effects.c
  engines/eevee/eevee_engine.c
  engines/eevee/eevee_lightcache.c
  engines/eevee/eevee_lightprobes.c
  engines/eevee/eevee_lights.c
  engines/eevee/eevee_lookdev.c
  engines/eevee/eevee_lut.c
  engines/eevee/eevee_lut_gen.c
  engines/eevee/eevee_materials.c
  engines/eevee/eevee_mist.c
  engines/eevee/eevee_motion_blur.c
  engines/eevee/eevee_occlusion.c
  engines/eevee/eevee_render.c
  engines/eevee/eevee_renderpasses.c
  engines/eevee/eevee_sampling.c
  engines/eevee/eevee_screen_raytrace.c
  engines/eevee/eevee_shaders.c
  engines/eevee/eevee_shadows.c
  engines/eevee/eevee_shadows_cascade.c
  engines/eevee/eevee_shadows_cube.c
  engines/eevee/eevee_subsurface.c
  engines/eevee/eevee_temporal_sampling.c
  engines/eevee/eevee_volumes.c
  engines/workbench/workbench_data.c
  engines/workbench/workbench_effect_antialiasing.c
  engines/workbench/workbench_effect_cavity.c
  engines/workbench/workbench_effect_dof.c
  engines/workbench/workbench_effect_outline.c
  engines/workbench/workbench_engine.c
  engines/workbench/workbench_materials.c
  engines/workbench/workbench_opaque.c
  engines/workbench/workbench_render.c
  engines/workbench/workbench_shader.cc
  engines/workbench/workbench_shadow.c
  engines/workbench/workbench_transparent.c
  engines/workbench/workbench_volume.c
  engines/external/external_engine.c
  engines/gpencil/gpencil_antialiasing.c
  engines/gpencil/gpencil_cache_utils.c
  engines/gpencil/gpencil_draw_data.c
  engines/gpencil/gpencil_engine.c
  engines/gpencil/gpencil_engine.h
  engines/gpencil/gpencil_render.c
  engines/gpencil/gpencil_shader.c
  engines/gpencil/gpencil_shader_fx.c
  engines/select/select_draw_utils.c
  engines/select/select_engine.c
  engines/overlay/overlay_antialiasing.c
  engines/overlay/overlay_armature.c
  engines/overlay/overlay_background.c
  engines/overlay/overlay_edit_curve.c
  engines/overlay/overlay_edit_mesh.c
  engines/overlay/overlay_edit_text.c
  engines/overlay/overlay_edit_uv.c
  engines/overlay/overlay_engine.c
  engines/overlay/overlay_extra.c
  engines/overlay/overlay_facing.c
  engines/overlay/overlay_fade.c
  engines/overlay/overlay_gpencil.c
  engines/overlay/overlay_grid.c
  engines/overlay/overlay_image.c
  engines/overlay/overlay_lattice.c
  engines/overlay/overlay_metaball.c
  engines/overlay/overlay_mode_transfer.c
  engines/overlay/overlay_motion_path.c
  engines/overlay/overlay_outline.c
  engines/overlay/overlay_paint.c
  engines/overlay/overlay_particle.c
  engines/overlay/overlay_sculpt.c
  engines/overlay/overlay_shader.c
  engines/overlay/overlay_volume.c
  engines/overlay/overlay_wireframe.c

  DRW_engine.h
  DRW_select_buffer.h
  intern/DRW_gpu_wrapper.hh
  intern/DRW_render.h
  intern/draw_cache.h
  intern/draw_cache_extract.h
  intern/draw_cache_impl.h
  intern/draw_cache_inline.h
  intern/draw_color_management.h
  intern/draw_common.h
  intern/draw_debug.h
  intern/draw_hair_private.h
  intern/draw_instance_data.h
  intern/draw_manager.h
  intern/draw_manager_profiling.h
  intern/draw_manager_testing.h
  intern/draw_manager_text.h
  intern/draw_shader.h
  intern/draw_shader_shared.h
  intern/draw_subdivision.h
  intern/draw_texture_pool.h
  intern/draw_view.h
  intern/draw_view_data.h
  intern/mesh_extractors/extract_mesh.h
  intern/smaa_textures.h
  engines/basic/basic_engine.h
  engines/basic/basic_private.h
  engines/eevee/eevee_engine.h
  engines/eevee/eevee_lightcache.h
  engines/eevee/eevee_lut.h
  engines/eevee/eevee_private.h
  engines/external/external_engine.h
  engines/image/image_batches.hh
  engines/image/image_drawing_mode.hh
  engines/image/image_engine.h
  engines/image/image_instance_data.hh
  engines/image/image_partial_updater.hh
  engines/image/image_private.hh
  engines/image/image_shader_params.hh
  engines/image/image_space_image.hh
  engines/image/image_space_node.hh
  engines/image/image_space.hh
  engines/image/image_wrappers.hh
  engines/workbench/workbench_engine.h
  engines/workbench/workbench_private.h
  engines/workbench/workbench_shader_shared.h
  engines/select/select_engine.h
  engines/select/select_private.h
  engines/overlay/overlay_engine.h
  engines/overlay/overlay_private.h
)

set(LIB
  bf_blenkernel
  bf_blenlib
  bf_windowmanager
)

set(GLSL_SRC
  engines/eevee/shaders/ambient_occlusion_lib.glsl
  engines/eevee/shaders/background_vert.glsl
  engines/eevee/shaders/common_uniforms_lib.glsl
  engines/eevee/shaders/common_utiltex_lib.glsl
  engines/eevee/shaders/lights_lib.glsl
  engines/eevee/shaders/lightprobe_lib.glsl
  engines/eevee/shaders/lightprobe_filter_glossy_frag.glsl
  engines/eevee/shaders/lightprobe_filter_diffuse_frag.glsl
  engines/eevee/shaders/lightprobe_filter_visibility_frag.glsl
  engines/eevee/shaders/lightprobe_geom.glsl
  engines/eevee/shaders/lightprobe_vert.glsl
  engines/eevee/shaders/lightprobe_cube_display_frag.glsl
  engines/eevee/shaders/lightprobe_cube_display_vert.glsl
  engines/eevee/shaders/lightprobe_grid_display_frag.glsl
  engines/eevee/shaders/lightprobe_grid_display_vert.glsl
  engines/eevee/shaders/lightprobe_grid_fill_frag.glsl
  engines/eevee/shaders/lightprobe_planar_display_frag.glsl
  engines/eevee/shaders/lightprobe_planar_display_vert.glsl
  engines/eevee/shaders/lookdev_world_frag.glsl
  engines/eevee/shaders/closure_eval_lib.glsl
  engines/eevee/shaders/closure_eval_diffuse_lib.glsl
  engines/eevee/shaders/closure_eval_glossy_lib.glsl
  engines/eevee/shaders/closure_eval_refraction_lib.glsl
  engines/eevee/shaders/closure_eval_translucent_lib.glsl
  engines/eevee/shaders/closure_type_lib.glsl
  engines/eevee/shaders/effect_bloom_frag.glsl
  engines/eevee/shaders/effect_dof_bokeh_frag.glsl
  engines/eevee/shaders/effect_dof_dilate_tiles_frag.glsl
  engines/eevee/shaders/effect_dof_downsample_frag.glsl
  engines/eevee/shaders/effect_dof_filter_frag.glsl
  engines/eevee/shaders/effect_dof_flatten_tiles_frag.glsl
  engines/eevee/shaders/effect_dof_gather_frag.glsl
  engines/eevee/shaders/effect_dof_lib.glsl
  engines/eevee/shaders/effect_dof_reduce_frag.glsl
  engines/eevee/shaders/effect_dof_resolve_frag.glsl
  engines/eevee/shaders/effect_dof_scatter_frag.glsl
  engines/eevee/shaders/effect_dof_scatter_vert.glsl
  engines/eevee/shaders/effect_dof_setup_frag.glsl
  engines/eevee/shaders/effect_reflection_lib.glsl
  engines/eevee/shaders/effect_reflection_resolve_frag.glsl
  engines/eevee/shaders/effect_reflection_trace_frag.glsl
  engines/eevee/shaders/effect_downsample_frag.glsl
  engines/eevee/shaders/effect_downsample_cube_frag.glsl
  engines/eevee/shaders/effect_gtao_frag.glsl
  engines/eevee/shaders/effect_velocity_resolve_frag.glsl
  engines/eevee/shaders/effect_velocity_tile_frag.glsl
  engines/eevee/shaders/effect_minmaxz_frag.glsl
  engines/eevee/shaders/effect_mist_frag.glsl
  engines/eevee/shaders/effect_motion_blur_frag.glsl
  engines/eevee/shaders/effect_subsurface_frag.glsl
  engines/eevee/shaders/effect_translucency_frag.glsl
  engines/eevee/shaders/effect_temporal_aa.glsl
  engines/eevee/shaders/lightprobe_planar_downsample_frag.glsl
  engines/eevee/shaders/lightprobe_planar_downsample_geom.glsl
  engines/eevee/shaders/lightprobe_planar_downsample_vert.glsl
  engines/eevee/shaders/object_motion_frag.glsl
  engines/eevee/shaders/object_motion_vert.glsl
  engines/eevee/shaders/prepass_frag.glsl
  engines/eevee/shaders/prepass_vert.glsl
  engines/eevee/shaders/shadow_accum_frag.glsl
  engines/eevee/shaders/shadow_frag.glsl
  engines/eevee/shaders/shadow_vert.glsl
  engines/eevee/shaders/bsdf_lut_frag.glsl
  engines/eevee/shaders/btdf_lut_frag.glsl
  engines/eevee/shaders/bsdf_common_lib.glsl
  engines/eevee/shaders/irradiance_lib.glsl
  engines/eevee/shaders/octahedron_lib.glsl
  engines/eevee/shaders/cubemap_lib.glsl
  engines/eevee/shaders/bsdf_sampling_lib.glsl
  engines/eevee/shaders/random_lib.glsl
  engines/eevee/shaders/raytrace_lib.glsl
  engines/eevee/shaders/renderpass_lib.glsl
  engines/eevee/shaders/renderpass_postprocess_frag.glsl
  engines/eevee/shaders/cryptomatte_frag.glsl
  engines/eevee/shaders/ltc_lib.glsl
  engines/eevee/shaders/ssr_lib.glsl
  engines/eevee/shaders/surface_frag.glsl
  engines/eevee/shaders/surface_geom.glsl
  engines/eevee/shaders/surface_lib.glsl
  engines/eevee/shaders/surface_vert.glsl
  engines/eevee/shaders/update_noise_frag.glsl
  engines/eevee/shaders/volumetric_accum_frag.glsl
  engines/eevee/shaders/volumetric_lib.glsl
  engines/eevee/shaders/volumetric_frag.glsl
  engines/eevee/shaders/volumetric_geom.glsl
  engines/eevee/shaders/volumetric_vert.glsl
  engines/eevee/shaders/volumetric_resolve_frag.glsl
  engines/eevee/shaders/volumetric_scatter_frag.glsl
  engines/eevee/shaders/volumetric_integration_frag.glsl

  engines/workbench/shaders/workbench_cavity_lib.glsl
  engines/workbench/shaders/workbench_common_lib.glsl
  engines/workbench/shaders/workbench_composite_frag.glsl
  engines/workbench/shaders/workbench_curvature_lib.glsl
  engines/workbench/shaders/workbench_effect_cavity_frag.glsl
  engines/workbench/shaders/workbench_effect_dof_frag.glsl
  engines/workbench/shaders/workbench_effect_outline_frag.glsl
  engines/workbench/shaders/workbench_effect_smaa_frag.glsl
  engines/workbench/shaders/workbench_effect_smaa_vert.glsl
  engines/workbench/shaders/workbench_effect_taa_frag.glsl
  engines/workbench/shaders/workbench_image_lib.glsl
  engines/workbench/shaders/workbench_matcap_lib.glsl
  engines/workbench/shaders/workbench_material_lib.glsl
  engines/workbench/shaders/workbench_merge_infront_frag.glsl
  engines/workbench/shaders/workbench_prepass_frag.glsl
  engines/workbench/shaders/workbench_prepass_hair_vert.glsl
  engines/workbench/shaders/workbench_prepass_pointcloud_vert.glsl
  engines/workbench/shaders/workbench_prepass_vert.glsl
  engines/workbench/shaders/workbench_shadow_caps_geom.glsl
  engines/workbench/shaders/workbench_shadow_debug_frag.glsl
  engines/workbench/shaders/workbench_shadow_geom.glsl
  engines/workbench/shaders/workbench_shadow_vert.glsl
  engines/workbench/shaders/workbench_transparent_accum_frag.glsl
  engines/workbench/shaders/workbench_transparent_resolve_frag.glsl
  engines/workbench/shaders/workbench_volume_frag.glsl
  engines/workbench/shaders/workbench_volume_vert.glsl
  engines/workbench/shaders/workbench_world_light_lib.glsl

  engines/workbench/workbench_shader_shared.h

  intern/shaders/common_colormanagement_lib.glsl
  intern/shaders/common_globals_lib.glsl
  intern/shaders/common_pointcloud_lib.glsl
  intern/shaders/common_hair_lib.glsl
  intern/shaders/common_hair_refine_vert.glsl
  intern/shaders/common_hair_refine_comp.glsl
  intern/shaders/common_math_lib.glsl
  intern/shaders/common_math_geom_lib.glsl
  intern/shaders/common_view_clipping_lib.glsl
  intern/shaders/common_view_lib.glsl
  intern/shaders/common_fxaa_lib.glsl
  intern/shaders/common_smaa_lib.glsl
  intern/shaders/common_fullscreen_vert.glsl

  intern/shaders/common_subdiv_custom_data_interp_comp.glsl
  intern/shaders/common_subdiv_ibo_lines_comp.glsl
  intern/shaders/common_subdiv_ibo_tris_comp.glsl
  intern/shaders/common_subdiv_lib.glsl
  intern/shaders/common_subdiv_normals_accumulate_comp.glsl
  intern/shaders/common_subdiv_normals_finalize_comp.glsl
  intern/shaders/common_subdiv_patch_evaluation_comp.glsl
  intern/shaders/common_subdiv_vbo_edge_fac_comp.glsl
  intern/shaders/common_subdiv_vbo_edituv_strech_angle_comp.glsl
  intern/shaders/common_subdiv_vbo_edituv_strech_area_comp.glsl
  intern/shaders/common_subdiv_vbo_lnor_comp.glsl
  intern/shaders/common_subdiv_vbo_sculpt_data_comp.glsl

  intern/draw_shader_shared.h

  engines/gpencil/shaders/gpencil_frag.glsl
  engines/gpencil/shaders/gpencil_vert.glsl
  engines/gpencil/shaders/gpencil_antialiasing_frag.glsl
  engines/gpencil/shaders/gpencil_antialiasing_vert.glsl
  engines/gpencil/shaders/gpencil_common_lib.glsl
  engines/gpencil/shaders/gpencil_layer_blend_frag.glsl
  engines/gpencil/shaders/gpencil_mask_invert_frag.glsl
  engines/gpencil/shaders/gpencil_depth_merge_frag.glsl
  engines/gpencil/shaders/gpencil_depth_merge_vert.glsl
  engines/gpencil/shaders/gpencil_vfx_frag.glsl

  engines/select/shaders/selection_id_3D_vert.glsl
  engines/select/shaders/selection_id_frag.glsl

  engines/basic/shaders/conservative_depth_geom.glsl
  engines/basic/shaders/depth_vert.glsl
  engines/basic/shaders/depth_frag.glsl

  engines/overlay/shaders/common_overlay_lib.glsl
  engines/overlay/shaders/antialiasing_frag.glsl
  engines/overlay/shaders/antialiasing_vert.glsl
  engines/overlay/shaders/armature_dof_vert.glsl
  engines/overlay/shaders/armature_dof_solid_frag.glsl
  engines/overlay/shaders/armature_envelope_outline_vert.glsl
  engines/overlay/shaders/armature_envelope_solid_frag.glsl
  engines/overlay/shaders/armature_envelope_solid_vert.glsl
  engines/overlay/shaders/armature_shape_outline_geom.glsl
  engines/overlay/shaders/armature_shape_outline_vert.glsl
  engines/overlay/shaders/armature_shape_solid_frag.glsl
  engines/overlay/shaders/armature_shape_solid_vert.glsl
  engines/overlay/shaders/armature_shape_wire_vert.glsl
  engines/overlay/shaders/armature_sphere_outline_vert.glsl
  engines/overlay/shaders/armature_sphere_solid_frag.glsl
  engines/overlay/shaders/armature_sphere_solid_vert.glsl
  engines/overlay/shaders/armature_stick_frag.glsl
  engines/overlay/shaders/armature_stick_vert.glsl
  engines/overlay/shaders/armature_wire_frag.glsl
  engines/overlay/shaders/armature_wire_vert.glsl
  engines/overlay/shaders/background_frag.glsl
  engines/overlay/shaders/clipbound_vert.glsl
  engines/overlay/shaders/depth_only_vert.glsl
  engines/overlay/shaders/edit_curve_handle_geom.glsl
  engines/overlay/shaders/edit_curve_handle_vert.glsl
  engines/overlay/shaders/edit_curve_point_vert.glsl
  engines/overlay/shaders/edit_curve_wire_vert.glsl
  engines/overlay/shaders/edit_gpencil_canvas_vert.glsl
  engines/overlay/shaders/edit_gpencil_guide_vert.glsl
  engines/overlay/shaders/edit_gpencil_vert.glsl
  engines/overlay/shaders/edit_lattice_point_vert.glsl
  engines/overlay/shaders/edit_lattice_wire_vert.glsl
  engines/overlay/shaders/edit_mesh_common_lib.glsl
  engines/overlay/shaders/edit_mesh_facefill_frag.glsl
  engines/overlay/shaders/edit_mesh_facefill_vert.glsl
  engines/overlay/shaders/edit_mesh_frag.glsl
  engines/overlay/shaders/edit_mesh_geom.glsl
  engines/overlay/shaders/edit_mesh_normal_vert.glsl
  engines/overlay/shaders/edit_mesh_analysis_frag.glsl
  engines/overlay/shaders/edit_mesh_analysis_vert.glsl
  engines/overlay/shaders/edit_mesh_skin_root_vert.glsl
  engines/overlay/shaders/edit_mesh_vert.glsl
  engines/overlay/shaders/edit_particle_strand_vert.glsl
  engines/overlay/shaders/edit_particle_point_vert.glsl
  engines/overlay/shaders/edit_uv_edges_vert.glsl
  engines/overlay/shaders/edit_uv_edges_geom.glsl
  engines/overlay/shaders/edit_uv_edges_frag.glsl
  engines/overlay/shaders/edit_uv_verts_vert.glsl
  engines/overlay/shaders/edit_uv_verts_frag.glsl
  engines/overlay/shaders/edit_uv_faces_vert.glsl
  engines/overlay/shaders/edit_uv_face_dots_vert.glsl
  engines/overlay/shaders/edit_uv_image_vert.glsl
  engines/overlay/shaders/edit_uv_image_mask_frag.glsl
  engines/overlay/shaders/edit_uv_stretching_vert.glsl
  engines/overlay/shaders/edit_uv_tiled_image_borders_vert.glsl
  engines/overlay/shaders/extra_frag.glsl
  engines/overlay/shaders/extra_vert.glsl
  engines/overlay/shaders/extra_groundline_vert.glsl
  engines/overlay/shaders/extra_lightprobe_grid_vert.glsl
  engines/overlay/shaders/extra_loose_point_frag.glsl
  engines/overlay/shaders/extra_loose_point_vert.glsl
  engines/overlay/shaders/extra_point_vert.glsl
  engines/overlay/shaders/extra_wire_frag.glsl
  engines/overlay/shaders/extra_wire_vert.glsl
  engines/overlay/shaders/facing_frag.glsl
  engines/overlay/shaders/facing_vert.glsl
  engines/overlay/shaders/grid_background_frag.glsl
  engines/overlay/shaders/grid_frag.glsl
  engines/overlay/shaders/grid_vert.glsl
  engines/overlay/shaders/image_vert.glsl
  engines/overlay/shaders/image_frag.glsl
  engines/overlay/shaders/motion_path_line_geom.glsl
  engines/overlay/shaders/motion_path_line_vert.glsl
  engines/overlay/shaders/motion_path_point_vert.glsl
  engines/overlay/shaders/outline_detect_frag.glsl
  engines/overlay/shaders/outline_prepass_frag.glsl
  engines/overlay/shaders/outline_prepass_geom.glsl
  engines/overlay/shaders/outline_prepass_vert.glsl
  engines/overlay/shaders/paint_face_vert.glsl
  engines/overlay/shaders/paint_point_vert.glsl
  engines/overlay/shaders/paint_texture_frag.glsl
  engines/overlay/shaders/paint_texture_vert.glsl
  engines/overlay/shaders/paint_vertcol_frag.glsl
  engines/overlay/shaders/paint_vertcol_vert.glsl
  engines/overlay/shaders/paint_weight_frag.glsl
  engines/overlay/shaders/paint_weight_vert.glsl
  engines/overlay/shaders/paint_wire_vert.glsl
  engines/overlay/shaders/particle_vert.glsl
  engines/overlay/shaders/particle_frag.glsl
  engines/overlay/shaders/sculpt_mask_vert.glsl
  engines/overlay/shaders/sculpt_mask_frag.glsl
  engines/overlay/shaders/volume_velocity_vert.glsl
  engines/overlay/shaders/volume_gridlines_vert.glsl
  engines/overlay/shaders/wireframe_vert.glsl
  engines/overlay/shaders/wireframe_frag.glsl
  engines/overlay/shaders/xray_fade_frag.glsl

  engines/image/shaders/image_engine_color_frag.glsl
  engines/image/shaders/image_engine_color_vert.glsl
  engines/image/shaders/image_engine_depth_frag.glsl
  engines/image/shaders/image_engine_depth_vert.glsl
)

set(GLSL_C)
foreach(GLSL_FILE ${GLSL_SRC})
  data_to_c_simple(${GLSL_FILE} GLSL_C)
endforeach()

blender_add_lib(bf_draw_shaders "${GLSL_C}" "" "" "")

list(APPEND LIB
  bf_draw_shaders
)

set(GLSL_SOURCE_CONTENT "")
foreach(GLSL_FILE ${GLSL_SRC})
  get_filename_component(GLSL_FILE_NAME ${GLSL_FILE} NAME)
  string(REPLACE "." "_" GLSL_FILE_NAME_UNDERSCORES ${GLSL_FILE_NAME})
  string(APPEND GLSL_SOURCE_CONTENT "SHADER_SOURCE\(datatoc_${GLSL_FILE_NAME_UNDERSCORES}, \"${GLSL_FILE_NAME}\", \"${GLSL_FILE}\"\)\n")
endforeach()

set(glsl_source_list_file "${CMAKE_CURRENT_BINARY_DIR}/glsl_draw_source_list.h")
file(GENERATE OUTPUT ${glsl_source_list_file} CONTENT "${GLSL_SOURCE_CONTENT}")
list(APPEND SRC ${glsl_source_list_file})
list(APPEND INC ${CMAKE_CURRENT_BINARY_DIR})

target_include_directories(bf_draw_shaders PUBLIC ${CMAKE_CURRENT_BINARY_DIR})

list(APPEND INC
)

if(WITH_DRAW_DEBUG)
  list(APPEND SRC
    engines/select/select_debug_engine.c
  )
  add_definitions(-DWITH_DRAW_DEBUG)
endif()

if(WITH_MOD_FLUID)
  list(APPEND INC
    ../../../intern/mantaflow/extern
  )
  add_definitions(-DWITH_FLUID)
endif()

if(WITH_FREESTYLE)
  add_definitions(-DWITH_FREESTYLE)
endif()

if(WITH_XR_OPENXR)
  add_definitions(-DWITH_XR_OPENXR)
endif()

if(WITH_GTESTS)
  if(WITH_OPENGL_DRAW_TESTS)
    add_definitions(-DWITH_OPENGL_DRAW_TESTS)
  endif()
endif()


blender_add_lib(bf_draw "${SRC}" "${INC}" "${INC_SYS}" "${LIB}")

# Needed so we can use dna_type_offsets.h for defaults initialization.
add_dependencies(bf_draw bf_dna)

if(WITH_GTESTS)
  if(WITH_OPENGL_DRAW_TESTS)
    set(TEST_SRC
      tests/draw_testing.cc
      tests/shaders_test.cc

      tests/draw_testing.hh
    )
    set(TEST_INC
      ../../../intern/ghost
      ../gpu/tests
    )
    set(TEST_LIB
    )
    include(GTestTesting)
    blender_add_test_lib(bf_draw_tests "${TEST_SRC}" "${INC};${TEST_INC}" "${INC_SYS}" "${LIB};${TEST_LIB}")
  endif()
endif()
