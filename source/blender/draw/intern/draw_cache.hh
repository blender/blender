/* SPDX-FileCopyrightText: 2016 Blender Authors
 *
 * SPDX-License-Identifier: GPL-2.0-or-later */

/** \file
 * \ingroup draw
 */

#pragma once

#include "BKE_volume_grid_fwd.hh"

struct GPUBatch;
struct GPUMaterial;
namespace blender::gpu {
class VertBuf;
}
struct ModifierData;
struct Object;
struct PTCacheEdit;
struct ParticleSystem;
struct Volume;
struct bGPDstroke;
struct bGPdata;
struct Scene;

/**
 * Shape resolution level of detail.
 */
enum eDRWLevelOfDetail {
  DRW_LOD_LOW = 0,
  DRW_LOD_MEDIUM = 1,
  DRW_LOD_HIGH = 2,

  DRW_LOD_MAX, /* Max number of level of detail */
};

void DRW_shape_cache_free();

/* 3D cursor */
GPUBatch *DRW_cache_cursor_get(bool crosshair_lines);

/* Common Shapes */
GPUBatch *DRW_cache_groundline_get();
/* Grid */
GPUBatch *DRW_cache_grid_get();
/**
 * Use this one for rendering full-screen passes. For 3D objects use #DRW_cache_quad_get().
 */
GPUBatch *DRW_cache_fullscreen_quad_get();
/* Just a regular quad with 4 vertices. */
GPUBatch *DRW_cache_quad_get();
/* Just a regular quad with 4 vertices - wires. */
GPUBatch *DRW_cache_quad_wires_get();
GPUBatch *DRW_cache_cube_get();
GPUBatch *DRW_cache_normal_arrow_get();

GPUBatch *DRW_cache_sphere_get(eDRWLevelOfDetail level_of_detail);

/* Dummy VBOs */

GPUBatch *DRW_gpencil_dummy_buffer_get();

/* Common Object */

GPUBatch *DRW_cache_object_all_edges_get(Object *ob);
GPUBatch *DRW_cache_object_edge_detection_get(Object *ob, bool *r_is_manifold);
GPUBatch *DRW_cache_object_surface_get(Object *ob);
GPUBatch *DRW_cache_object_loose_edges_get(Object *ob);
GPUBatch **DRW_cache_object_surface_material_get(Object *ob,
                                                 GPUMaterial **gpumat_array,
                                                 uint gpumat_array_len);
GPUBatch *DRW_cache_object_face_wireframe_get(Object *ob);
int DRW_cache_object_material_count_get(const Object *ob);

/**
 * Returns the vertbuf used by shaded surface batch.
 */
blender::gpu::VertBuf *DRW_cache_object_pos_vertbuf_get(Object *ob);

/* Empties */
GPUBatch *DRW_cache_plain_axes_get();
GPUBatch *DRW_cache_single_arrow_get();
GPUBatch *DRW_cache_empty_cube_get();
GPUBatch *DRW_cache_circle_get();
GPUBatch *DRW_cache_empty_sphere_get();
GPUBatch *DRW_cache_empty_cylinder_get();
GPUBatch *DRW_cache_empty_cone_get();
GPUBatch *DRW_cache_empty_capsule_cap_get();
GPUBatch *DRW_cache_empty_capsule_body_get();

/* Force Field */

GPUBatch *DRW_cache_field_wind_get();
GPUBatch *DRW_cache_field_force_get();
GPUBatch *DRW_cache_field_vortex_get();

/* Screen-aligned circle. */

GPUBatch *DRW_cache_field_curve_get();
GPUBatch *DRW_cache_field_tube_limit_get();
GPUBatch *DRW_cache_field_cone_limit_get();

/* Screen-aligned dashed circle */

GPUBatch *DRW_cache_field_sphere_limit_get();

/* Lights */

GPUBatch *DRW_cache_light_icon_inner_lines_get();
GPUBatch *DRW_cache_light_icon_outer_lines_get();
GPUBatch *DRW_cache_light_icon_sun_rays_get();
GPUBatch *DRW_cache_light_point_lines_get();
GPUBatch *DRW_cache_light_sun_lines_get();
GPUBatch *DRW_cache_light_spot_lines_get();
GPUBatch *DRW_cache_light_area_disk_lines_get();
GPUBatch *DRW_cache_light_area_square_lines_get();
GPUBatch *DRW_cache_light_spot_volume_get();

/* Camera */

GPUBatch *DRW_cache_camera_frame_get();
GPUBatch *DRW_cache_camera_volume_get();
GPUBatch *DRW_cache_camera_volume_wire_get();
GPUBatch *DRW_cache_camera_tria_wire_get();
GPUBatch *DRW_cache_camera_tria_get();
GPUBatch *DRW_cache_camera_distances_get();

/* Speaker */

GPUBatch *DRW_cache_speaker_get();

/* Probe */

GPUBatch *DRW_cache_lightprobe_cube_get();
GPUBatch *DRW_cache_lightprobe_grid_get();
GPUBatch *DRW_cache_lightprobe_planar_get();

/* Bones */

GPUBatch *DRW_cache_bone_octahedral_get();
GPUBatch *DRW_cache_bone_octahedral_wire_get();
GPUBatch *DRW_cache_bone_box_get();
GPUBatch *DRW_cache_bone_box_wire_get();
GPUBatch *DRW_cache_bone_envelope_solid_get();
GPUBatch *DRW_cache_bone_envelope_outline_get();
GPUBatch *DRW_cache_bone_point_get();
GPUBatch *DRW_cache_bone_point_wire_outline_get();
GPUBatch *DRW_cache_bone_stick_get();
GPUBatch *DRW_cache_bone_arrows_get();
GPUBatch *DRW_cache_bone_dof_sphere_get();
GPUBatch *DRW_cache_bone_dof_lines_get();

/* Meshes */

GPUBatch *DRW_cache_mesh_all_verts_get(Object *ob);
GPUBatch *DRW_cache_mesh_all_edges_get(Object *ob);
GPUBatch *DRW_cache_mesh_loose_edges_get(Object *ob);
GPUBatch *DRW_cache_mesh_edge_detection_get(Object *ob, bool *r_is_manifold);
GPUBatch *DRW_cache_mesh_surface_get(Object *ob);
GPUBatch *DRW_cache_mesh_surface_edges_get(Object *ob);
/**
 * Return list of batches with length equal to `max(1, totcol)`.
 */
GPUBatch **DRW_cache_mesh_surface_shaded_get(Object *ob,
                                             GPUMaterial **gpumat_array,
                                             uint gpumat_array_len);
/**
 * Return list of batches with length equal to `max(1, totcol)`.
 */
GPUBatch **DRW_cache_mesh_surface_texpaint_get(Object *ob);
GPUBatch *DRW_cache_mesh_surface_texpaint_single_get(Object *ob);
GPUBatch *DRW_cache_mesh_surface_vertpaint_get(Object *ob);
GPUBatch *DRW_cache_mesh_surface_sculptcolors_get(Object *ob);
GPUBatch *DRW_cache_mesh_surface_weights_get(Object *ob);
GPUBatch *DRW_cache_mesh_surface_mesh_analysis_get(Object *ob);
GPUBatch *DRW_cache_mesh_face_wireframe_get(Object *ob);
GPUBatch *DRW_cache_mesh_surface_viewer_attribute_get(Object *ob);

/* Curve */

GPUBatch *DRW_cache_curve_edge_wire_get(Object *ob);
GPUBatch *DRW_cache_curve_edge_wire_viewer_attribute_get(Object *ob);

/* edit-mode */

GPUBatch *DRW_cache_curve_edge_normal_get(Object *ob);
GPUBatch *DRW_cache_curve_edge_overlay_get(Object *ob);
GPUBatch *DRW_cache_curve_vert_overlay_get(Object *ob);

/* Font */

GPUBatch *DRW_cache_text_edge_wire_get(Object *ob);

/* Surface */

GPUBatch *DRW_cache_surf_edge_wire_get(Object *ob);

/* Lattice */

GPUBatch *DRW_cache_lattice_verts_get(Object *ob);
GPUBatch *DRW_cache_lattice_wire_get(Object *ob, bool use_weight);
GPUBatch *DRW_cache_lattice_vert_overlay_get(Object *ob);

/* Particles */

GPUBatch *DRW_cache_particles_get_hair(Object *object, ParticleSystem *psys, ModifierData *md);
GPUBatch *DRW_cache_particles_get_dots(Object *object, ParticleSystem *psys);
GPUBatch *DRW_cache_particles_get_edit_strands(Object *object,
                                               ParticleSystem *psys,
                                               PTCacheEdit *edit,
                                               bool use_weight);
GPUBatch *DRW_cache_particles_get_edit_inner_points(Object *object,
                                                    ParticleSystem *psys,
                                                    PTCacheEdit *edit);
GPUBatch *DRW_cache_particles_get_edit_tip_points(Object *object,
                                                  ParticleSystem *psys,
                                                  PTCacheEdit *edit);
GPUBatch *DRW_cache_particles_get_prim(int type);

/* Curves */

GPUBatch *DRW_cache_curves_surface_get(Object *ob);
GPUBatch **DRW_cache_curves_surface_shaded_get(Object *ob,
                                               GPUMaterial **gpumat_array,
                                               uint gpumat_array_len);
GPUBatch *DRW_cache_curves_face_wireframe_get(Object *ob);
GPUBatch *DRW_cache_curves_edge_detection_get(Object *ob, bool *r_is_manifold);

/* Volume */

struct DRWVolumeGrid {
  DRWVolumeGrid *next, *prev;

  /* Grid name. */
  char *name;

  /* 3D texture. */
  GPUTexture *texture;

  /* Transform between 0..1 texture space and object space. */
  float texture_to_object[4][4];
  float object_to_texture[4][4];

  /* Transform from bounds to texture space. */
  float object_to_bounds[4][4];
  float bounds_to_texture[4][4];
};

namespace blender::draw {

DRWVolumeGrid *DRW_volume_batch_cache_get_grid(Volume *volume, const bke::VolumeGridData *grid);
GPUBatch *DRW_cache_volume_face_wireframe_get(Object *ob);
GPUBatch *DRW_cache_volume_selection_surface_get(Object *ob);

/* GPencil (legacy) */

GPUBatch *DRW_cache_gpencil_get(Object *ob, int cfra);
gpu::VertBuf *DRW_cache_gpencil_position_buffer_get(Object *ob, int cfra);
gpu::VertBuf *DRW_cache_gpencil_color_buffer_get(Object *ob, int cfra);
GPUBatch *DRW_cache_gpencil_edit_lines_get(Object *ob, int cfra);
GPUBatch *DRW_cache_gpencil_edit_points_get(Object *ob, int cfra);
GPUBatch *DRW_cache_gpencil_edit_curve_handles_get(Object *ob, int cfra);
GPUBatch *DRW_cache_gpencil_edit_curve_points_get(Object *ob, int cfra);
GPUBatch *DRW_cache_gpencil_sbuffer_get(Object *ob, bool show_fill);
gpu::VertBuf *DRW_cache_gpencil_sbuffer_position_buffer_get(Object *ob, bool show_fill);
gpu::VertBuf *DRW_cache_gpencil_sbuffer_color_buffer_get(Object *ob, bool show_fill);
int DRW_gpencil_material_count_get(const bGPdata *gpd);

GPUBatch *DRW_cache_gpencil_face_wireframe_get(Object *ob);

bGPDstroke *DRW_cache_gpencil_sbuffer_stroke_data_get(Object *ob);
/**
 * Sbuffer batches are temporary. We need to clear it after drawing.
 */
void DRW_cache_gpencil_sbuffer_clear(Object *ob);

/* Grease Pencil */

GPUBatch *DRW_cache_grease_pencil_get(const Scene *scene, Object *ob);
GPUBatch *DRW_cache_grease_pencil_edit_points_get(const Scene *scene, Object *ob);
GPUBatch *DRW_cache_grease_pencil_edit_lines_get(const Scene *scene, Object *ob);
gpu::VertBuf *DRW_cache_grease_pencil_position_buffer_get(const Scene *scene, Object *ob);
gpu::VertBuf *DRW_cache_grease_pencil_color_buffer_get(const Scene *scene, Object *ob);
}  // namespace blender::draw
