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
  ../blentranslation
  ../bmesh
  ../depsgraph
  ../editors/include
  ../editors/space_view3d
  ../gpu
  ../imbuf
  ../makesdna
  ../makesrna
  ../nodes
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
  intern/draw_cache.c
  intern/draw_cache_extract_mesh.c
  intern/draw_cache_impl_curve.c
  intern/draw_cache_impl_displist.c
  intern/draw_cache_impl_gpencil.c
  intern/draw_cache_impl_hair.c
  intern/draw_cache_impl_lattice.c
  intern/draw_cache_impl_mesh.c
  intern/draw_cache_impl_metaball.c
  intern/draw_cache_impl_particles.c
  intern/draw_cache_impl_pointcloud.c
  intern/draw_cache_impl_volume.c
  intern/draw_color_management.c
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
  intern/draw_select_buffer.c
  intern/draw_view.c
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
  engines/eevee/eevee_lut.c
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
  engines/workbench/workbench_shader.c
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
  engines/overlay/overlay_engine.c
  engines/overlay/overlay_extra.c
  engines/overlay/overlay_facing.c
  engines/overlay/overlay_gpencil.c
  engines/overlay/overlay_grid.c
  engines/overlay/overlay_image.c
  engines/overlay/overlay_lattice.c
  engines/overlay/overlay_metaball.c
  engines/overlay/overlay_motion_path.c
  engines/overlay/overlay_outline.c
  engines/overlay/overlay_paint.c
  engines/overlay/overlay_particle.c
  engines/overlay/overlay_pointcloud.c
  engines/overlay/overlay_sculpt.c
  engines/overlay/overlay_shader.c
  engines/overlay/overlay_wireframe.c

  DRW_engine.h
  DRW_select_buffer.h
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
  intern/draw_manager_text.h
  intern/draw_view.h
  intern/smaa_textures.h
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
  engines/overlay/overlay_engine.h
  engines/overlay/overlay_private.h
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
data_to_c_simple(engines/eevee/shaders/effect_velocity_tile_frag.glsl SRC)
data_to_c_simple(engines/eevee/shaders/effect_minmaxz_frag.glsl SRC)
data_to_c_simple(engines/eevee/shaders/effect_mist_frag.glsl SRC)
data_to_c_simple(engines/eevee/shaders/effect_motion_blur_frag.glsl SRC)
data_to_c_simple(engines/eevee/shaders/effect_ssr_frag.glsl SRC)
data_to_c_simple(engines/eevee/shaders/effect_subsurface_frag.glsl SRC)
data_to_c_simple(engines/eevee/shaders/effect_translucency_frag.glsl SRC)
data_to_c_simple(engines/eevee/shaders/effect_temporal_aa.glsl SRC)
data_to_c_simple(engines/eevee/shaders/lightprobe_planar_downsample_frag.glsl SRC)
data_to_c_simple(engines/eevee/shaders/lightprobe_planar_downsample_geom.glsl SRC)
data_to_c_simple(engines/eevee/shaders/lightprobe_planar_downsample_vert.glsl SRC)
data_to_c_simple(engines/eevee/shaders/object_motion_frag.glsl SRC)
data_to_c_simple(engines/eevee/shaders/object_motion_vert.glsl SRC)
data_to_c_simple(engines/eevee/shaders/prepass_frag.glsl SRC)
data_to_c_simple(engines/eevee/shaders/prepass_vert.glsl SRC)
data_to_c_simple(engines/eevee/shaders/shadow_accum_frag.glsl SRC)

data_to_c_simple(engines/eevee/shaders/shadow_frag.glsl SRC)
data_to_c_simple(engines/eevee/shaders/shadow_vert.glsl SRC)
data_to_c_simple(engines/eevee/shaders/bsdf_lut_frag.glsl SRC)
data_to_c_simple(engines/eevee/shaders/btdf_lut_frag.glsl SRC)
data_to_c_simple(engines/eevee/shaders/bsdf_common_lib.glsl SRC)
data_to_c_simple(engines/eevee/shaders/irradiance_lib.glsl SRC)
data_to_c_simple(engines/eevee/shaders/octahedron_lib.glsl SRC)
data_to_c_simple(engines/eevee/shaders/cubemap_lib.glsl SRC)
data_to_c_simple(engines/eevee/shaders/bsdf_sampling_lib.glsl SRC)
data_to_c_simple(engines/eevee/shaders/raytrace_lib.glsl SRC)
data_to_c_simple(engines/eevee/shaders/renderpass_postprocess_frag.glsl SRC)
data_to_c_simple(engines/eevee/shaders/ltc_lib.glsl SRC)
data_to_c_simple(engines/eevee/shaders/ssr_lib.glsl SRC)
data_to_c_simple(engines/eevee/shaders/update_noise_frag.glsl SRC)
data_to_c_simple(engines/eevee/shaders/volumetric_accum_frag.glsl SRC)
data_to_c_simple(engines/eevee/shaders/volumetric_lib.glsl SRC)
data_to_c_simple(engines/eevee/shaders/volumetric_frag.glsl SRC)
data_to_c_simple(engines/eevee/shaders/volumetric_geom.glsl SRC)
data_to_c_simple(engines/eevee/shaders/volumetric_vert.glsl SRC)
data_to_c_simple(engines/eevee/shaders/volumetric_resolve_frag.glsl SRC)
data_to_c_simple(engines/eevee/shaders/volumetric_scatter_frag.glsl SRC)
data_to_c_simple(engines/eevee/shaders/volumetric_integration_frag.glsl SRC)

data_to_c_simple(engines/workbench/shaders/workbench_cavity_lib.glsl SRC)
data_to_c_simple(engines/workbench/shaders/workbench_common_lib.glsl SRC)
data_to_c_simple(engines/workbench/shaders/workbench_composite_frag.glsl SRC)
data_to_c_simple(engines/workbench/shaders/workbench_curvature_lib.glsl SRC)
data_to_c_simple(engines/workbench/shaders/workbench_data_lib.glsl SRC)
data_to_c_simple(engines/workbench/shaders/workbench_effect_cavity_frag.glsl SRC)
data_to_c_simple(engines/workbench/shaders/workbench_effect_dof_frag.glsl SRC)
data_to_c_simple(engines/workbench/shaders/workbench_effect_outline_frag.glsl SRC)
data_to_c_simple(engines/workbench/shaders/workbench_effect_smaa_frag.glsl SRC)
data_to_c_simple(engines/workbench/shaders/workbench_effect_smaa_vert.glsl SRC)
data_to_c_simple(engines/workbench/shaders/workbench_effect_taa_frag.glsl SRC)
data_to_c_simple(engines/workbench/shaders/workbench_image_lib.glsl SRC)
data_to_c_simple(engines/workbench/shaders/workbench_matcap_lib.glsl SRC)
data_to_c_simple(engines/workbench/shaders/workbench_material_lib.glsl SRC)
data_to_c_simple(engines/workbench/shaders/workbench_merge_infront_frag.glsl SRC)
data_to_c_simple(engines/workbench/shaders/workbench_prepass_frag.glsl SRC)
data_to_c_simple(engines/workbench/shaders/workbench_prepass_hair_vert.glsl SRC)
data_to_c_simple(engines/workbench/shaders/workbench_prepass_vert.glsl SRC)
data_to_c_simple(engines/workbench/shaders/workbench_shader_interface_lib.glsl SRC)
data_to_c_simple(engines/workbench/shaders/workbench_shadow_caps_geom.glsl SRC)
data_to_c_simple(engines/workbench/shaders/workbench_shadow_debug_frag.glsl SRC)
data_to_c_simple(engines/workbench/shaders/workbench_shadow_geom.glsl SRC)
data_to_c_simple(engines/workbench/shaders/workbench_shadow_vert.glsl SRC)
data_to_c_simple(engines/workbench/shaders/workbench_transparent_accum_frag.glsl SRC)
data_to_c_simple(engines/workbench/shaders/workbench_transparent_resolve_frag.glsl SRC)
data_to_c_simple(engines/workbench/shaders/workbench_volume_frag.glsl SRC)
data_to_c_simple(engines/workbench/shaders/workbench_volume_vert.glsl SRC)
data_to_c_simple(engines/workbench/shaders/workbench_world_light_lib.glsl SRC)

data_to_c_simple(intern/shaders/common_colormanagement_lib.glsl SRC)
data_to_c_simple(intern/shaders/common_globals_lib.glsl SRC)
data_to_c_simple(intern/shaders/common_hair_lib.glsl SRC)
data_to_c_simple(intern/shaders/common_hair_refine_vert.glsl SRC)
data_to_c_simple(intern/shaders/common_view_lib.glsl SRC)
data_to_c_simple(intern/shaders/common_fxaa_lib.glsl SRC)
data_to_c_simple(intern/shaders/common_smaa_lib.glsl SRC)
data_to_c_simple(intern/shaders/common_fullscreen_vert.glsl SRC)

data_to_c_simple(engines/gpencil/shaders/gpencil_frag.glsl SRC)
data_to_c_simple(engines/gpencil/shaders/gpencil_vert.glsl SRC)
data_to_c_simple(engines/gpencil/shaders/gpencil_antialiasing_frag.glsl SRC)
data_to_c_simple(engines/gpencil/shaders/gpencil_antialiasing_vert.glsl SRC)
data_to_c_simple(engines/gpencil/shaders/gpencil_common_lib.glsl SRC)
data_to_c_simple(engines/gpencil/shaders/gpencil_layer_blend_frag.glsl SRC)
data_to_c_simple(engines/gpencil/shaders/gpencil_mask_invert_frag.glsl SRC)
data_to_c_simple(engines/gpencil/shaders/gpencil_depth_merge_frag.glsl SRC)
data_to_c_simple(engines/gpencil/shaders/gpencil_depth_merge_vert.glsl SRC)
data_to_c_simple(engines/gpencil/shaders/gpencil_vfx_frag.glsl SRC)

data_to_c_simple(engines/select/shaders/selection_id_3D_vert.glsl SRC)
data_to_c_simple(engines/select/shaders/selection_id_frag.glsl SRC)

data_to_c_simple(engines/basic/shaders/conservative_depth_geom.glsl SRC)
data_to_c_simple(engines/basic/shaders/depth_vert.glsl SRC)
data_to_c_simple(engines/basic/shaders/depth_frag.glsl SRC)

data_to_c_simple(engines/overlay/shaders/antialiasing_frag.glsl SRC)
data_to_c_simple(engines/overlay/shaders/antialiasing_vert.glsl SRC)
data_to_c_simple(engines/overlay/shaders/armature_dof_vert.glsl SRC)
data_to_c_simple(engines/overlay/shaders/armature_envelope_outline_vert.glsl SRC)
data_to_c_simple(engines/overlay/shaders/armature_envelope_solid_frag.glsl SRC)
data_to_c_simple(engines/overlay/shaders/armature_envelope_solid_vert.glsl SRC)
data_to_c_simple(engines/overlay/shaders/armature_shape_outline_geom.glsl SRC)
data_to_c_simple(engines/overlay/shaders/armature_shape_outline_vert.glsl SRC)
data_to_c_simple(engines/overlay/shaders/armature_shape_solid_frag.glsl SRC)
data_to_c_simple(engines/overlay/shaders/armature_shape_solid_vert.glsl SRC)
data_to_c_simple(engines/overlay/shaders/armature_shape_wire_vert.glsl SRC)
data_to_c_simple(engines/overlay/shaders/armature_sphere_outline_vert.glsl SRC)
data_to_c_simple(engines/overlay/shaders/armature_sphere_solid_frag.glsl SRC)
data_to_c_simple(engines/overlay/shaders/armature_sphere_solid_vert.glsl SRC)
data_to_c_simple(engines/overlay/shaders/armature_stick_frag.glsl SRC)
data_to_c_simple(engines/overlay/shaders/armature_stick_vert.glsl SRC)
data_to_c_simple(engines/overlay/shaders/armature_wire_frag.glsl SRC)
data_to_c_simple(engines/overlay/shaders/armature_wire_vert.glsl SRC)
data_to_c_simple(engines/overlay/shaders/background_frag.glsl SRC)
data_to_c_simple(engines/overlay/shaders/clipbound_vert.glsl SRC)
data_to_c_simple(engines/overlay/shaders/depth_only_vert.glsl SRC)
data_to_c_simple(engines/overlay/shaders/edit_curve_handle_geom.glsl SRC)
data_to_c_simple(engines/overlay/shaders/edit_curve_handle_vert.glsl SRC)
data_to_c_simple(engines/overlay/shaders/edit_curve_point_vert.glsl SRC)
data_to_c_simple(engines/overlay/shaders/edit_curve_wire_vert.glsl SRC)
data_to_c_simple(engines/overlay/shaders/edit_gpencil_canvas_vert.glsl SRC)
data_to_c_simple(engines/overlay/shaders/edit_gpencil_guide_vert.glsl SRC)
data_to_c_simple(engines/overlay/shaders/edit_gpencil_vert.glsl SRC)
data_to_c_simple(engines/overlay/shaders/edit_lattice_point_vert.glsl SRC)
data_to_c_simple(engines/overlay/shaders/edit_lattice_wire_vert.glsl SRC)
data_to_c_simple(engines/overlay/shaders/edit_mesh_common_lib.glsl SRC)
data_to_c_simple(engines/overlay/shaders/edit_mesh_facefill_frag.glsl SRC)
data_to_c_simple(engines/overlay/shaders/edit_mesh_facefill_vert.glsl SRC)
data_to_c_simple(engines/overlay/shaders/edit_mesh_frag.glsl SRC)
data_to_c_simple(engines/overlay/shaders/edit_mesh_geom.glsl SRC)
data_to_c_simple(engines/overlay/shaders/edit_mesh_normal_vert.glsl SRC)
data_to_c_simple(engines/overlay/shaders/edit_mesh_analysis_frag.glsl SRC)
data_to_c_simple(engines/overlay/shaders/edit_mesh_analysis_vert.glsl SRC)
data_to_c_simple(engines/overlay/shaders/edit_mesh_skin_root_vert.glsl SRC)
data_to_c_simple(engines/overlay/shaders/edit_mesh_vert.glsl SRC)
data_to_c_simple(engines/overlay/shaders/edit_particle_strand_vert.glsl SRC)
data_to_c_simple(engines/overlay/shaders/edit_particle_point_vert.glsl SRC)
data_to_c_simple(engines/overlay/shaders/extra_frag.glsl SRC)
data_to_c_simple(engines/overlay/shaders/extra_vert.glsl SRC)
data_to_c_simple(engines/overlay/shaders/extra_groundline_vert.glsl SRC)
data_to_c_simple(engines/overlay/shaders/extra_lightprobe_grid_vert.glsl SRC)
data_to_c_simple(engines/overlay/shaders/extra_loose_point_frag.glsl SRC)
data_to_c_simple(engines/overlay/shaders/extra_loose_point_vert.glsl SRC)
data_to_c_simple(engines/overlay/shaders/extra_point_vert.glsl SRC)
data_to_c_simple(engines/overlay/shaders/extra_wire_frag.glsl SRC)
data_to_c_simple(engines/overlay/shaders/extra_wire_vert.glsl SRC)
data_to_c_simple(engines/overlay/shaders/facing_frag.glsl SRC)
data_to_c_simple(engines/overlay/shaders/facing_vert.glsl SRC)
data_to_c_simple(engines/overlay/shaders/grid_frag.glsl SRC)
data_to_c_simple(engines/overlay/shaders/grid_vert.glsl SRC)
data_to_c_simple(engines/overlay/shaders/image_vert.glsl SRC)
data_to_c_simple(engines/overlay/shaders/image_frag.glsl SRC)
data_to_c_simple(engines/overlay/shaders/motion_path_line_geom.glsl SRC)
data_to_c_simple(engines/overlay/shaders/motion_path_line_vert.glsl SRC)
data_to_c_simple(engines/overlay/shaders/motion_path_point_vert.glsl SRC)
data_to_c_simple(engines/overlay/shaders/outline_detect_frag.glsl SRC)
data_to_c_simple(engines/overlay/shaders/outline_prepass_frag.glsl SRC)
data_to_c_simple(engines/overlay/shaders/outline_prepass_geom.glsl SRC)
data_to_c_simple(engines/overlay/shaders/outline_prepass_vert.glsl SRC)
data_to_c_simple(engines/overlay/shaders/paint_face_vert.glsl SRC)
data_to_c_simple(engines/overlay/shaders/paint_point_vert.glsl SRC)
data_to_c_simple(engines/overlay/shaders/paint_texture_frag.glsl SRC)
data_to_c_simple(engines/overlay/shaders/paint_texture_vert.glsl SRC)
data_to_c_simple(engines/overlay/shaders/paint_vertcol_frag.glsl SRC)
data_to_c_simple(engines/overlay/shaders/paint_vertcol_vert.glsl SRC)
data_to_c_simple(engines/overlay/shaders/paint_weight_frag.glsl SRC)
data_to_c_simple(engines/overlay/shaders/paint_weight_vert.glsl SRC)
data_to_c_simple(engines/overlay/shaders/paint_wire_vert.glsl SRC)
data_to_c_simple(engines/overlay/shaders/particle_vert.glsl SRC)
data_to_c_simple(engines/overlay/shaders/particle_frag.glsl SRC)
data_to_c_simple(engines/overlay/shaders/pointcloud_vert.glsl SRC)
data_to_c_simple(engines/overlay/shaders/pointcloud_frag.glsl SRC)
data_to_c_simple(engines/overlay/shaders/sculpt_mask_vert.glsl SRC)
data_to_c_simple(engines/overlay/shaders/sculpt_mask_frag.glsl SRC)
data_to_c_simple(engines/overlay/shaders/volume_velocity_vert.glsl SRC)
data_to_c_simple(engines/overlay/shaders/wireframe_vert.glsl SRC)
data_to_c_simple(engines/overlay/shaders/wireframe_frag.glsl SRC)
data_to_c_simple(engines/overlay/shaders/xray_fade_frag.glsl SRC)

list(APPEND INC
)

if(WITH_FREESTYLE)
  add_definitions(-DWITH_FREESTYLE)
endif()

if(WITH_XR_OPENXR)
  add_definitions(-DWITH_XR_OPENXR)
endif()

add_definitions(${GL_DEFINITIONS})

blender_add_lib(bf_draw "${SRC}" "${INC}" "${INC_SYS}" "${LIB}")
