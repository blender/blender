/* SPDX-FileCopyrightText: 2016 Blender Authors
 *
 * SPDX-License-Identifier: GPL-2.0-or-later */

/** \file
 * \ingroup draw
 */

#pragma once

#ifdef __cplusplus
extern "C" {
#endif

struct GPUBatch;
struct GPUMaterial;
struct GPUVertBuf;
struct ModifierData;
struct Object;
struct PTCacheEdit;
struct ParticleSystem;
struct Volume;
struct VolumeGrid;
struct bGPDstroke;
struct bGPdata;

/**
 * Shape resolution level of detail.
 */
typedef enum eDRWLevelOfDetail {
  DRW_LOD_LOW = 0,
  DRW_LOD_MEDIUM = 1,
  DRW_LOD_HIGH = 2,

  DRW_LOD_MAX, /* Max number of level of detail */
} eDRWLevelOfDetail;

void DRW_shape_cache_free(void);

/* 3D cursor */
struct GPUBatch *DRW_cache_cursor_get(bool crosshair_lines);

/* Common Shapes */
struct GPUBatch *DRW_cache_groundline_get(void);
/* Grid */
struct GPUBatch *DRW_cache_grid_get(void);
/**
 * Use this one for rendering full-screen passes. For 3D objects use #DRW_cache_quad_get().
 */
struct GPUBatch *DRW_cache_fullscreen_quad_get(void);
/* Just a regular quad with 4 vertices. */
struct GPUBatch *DRW_cache_quad_get(void);
/* Just a regular quad with 4 vertices - wires. */
struct GPUBatch *DRW_cache_quad_wires_get(void);
struct GPUBatch *DRW_cache_cube_get(void);
struct GPUBatch *DRW_cache_normal_arrow_get(void);

struct GPUBatch *DRW_cache_sphere_get(eDRWLevelOfDetail level_of_detail);

/* Dummy VBOs */

struct GPUBatch *DRW_gpencil_dummy_buffer_get(void);

/* Common Object */

struct GPUBatch *DRW_cache_object_all_edges_get(struct Object *ob);
struct GPUBatch *DRW_cache_object_edge_detection_get(struct Object *ob, bool *r_is_manifold);
struct GPUBatch *DRW_cache_object_surface_get(struct Object *ob);
struct GPUBatch *DRW_cache_object_loose_edges_get(struct Object *ob);
struct GPUBatch **DRW_cache_object_surface_material_get(struct Object *ob,
                                                        struct GPUMaterial **gpumat_array,
                                                        uint gpumat_array_len);
struct GPUBatch *DRW_cache_object_face_wireframe_get(struct Object *ob);
int DRW_cache_object_material_count_get(struct Object *ob);

/**
 * Returns the vertbuf used by shaded surface batch.
 */
struct GPUVertBuf *DRW_cache_object_pos_vertbuf_get(struct Object *ob);

/* Empties */
struct GPUBatch *DRW_cache_plain_axes_get(void);
struct GPUBatch *DRW_cache_single_arrow_get(void);
struct GPUBatch *DRW_cache_empty_cube_get(void);
struct GPUBatch *DRW_cache_circle_get(void);
struct GPUBatch *DRW_cache_empty_sphere_get(void);
struct GPUBatch *DRW_cache_empty_cylinder_get(void);
struct GPUBatch *DRW_cache_empty_cone_get(void);
struct GPUBatch *DRW_cache_empty_capsule_cap_get(void);
struct GPUBatch *DRW_cache_empty_capsule_body_get(void);

/* Force Field */

struct GPUBatch *DRW_cache_field_wind_get(void);
struct GPUBatch *DRW_cache_field_force_get(void);
struct GPUBatch *DRW_cache_field_vortex_get(void);

/* Screen-aligned circle. */

struct GPUBatch *DRW_cache_field_curve_get(void);
struct GPUBatch *DRW_cache_field_tube_limit_get(void);
struct GPUBatch *DRW_cache_field_cone_limit_get(void);

/* Screen-aligned dashed circle */

struct GPUBatch *DRW_cache_field_sphere_limit_get(void);

/* Lights */

struct GPUBatch *DRW_cache_light_icon_inner_lines_get(void);
struct GPUBatch *DRW_cache_light_icon_outer_lines_get(void);
struct GPUBatch *DRW_cache_light_icon_sun_rays_get(void);
struct GPUBatch *DRW_cache_light_point_lines_get(void);
struct GPUBatch *DRW_cache_light_sun_lines_get(void);
struct GPUBatch *DRW_cache_light_spot_lines_get(void);
struct GPUBatch *DRW_cache_light_area_disk_lines_get(void);
struct GPUBatch *DRW_cache_light_area_square_lines_get(void);
struct GPUBatch *DRW_cache_light_spot_volume_get(void);

/* Camera */

struct GPUBatch *DRW_cache_camera_frame_get(void);
struct GPUBatch *DRW_cache_camera_volume_get(void);
struct GPUBatch *DRW_cache_camera_volume_wire_get(void);
struct GPUBatch *DRW_cache_camera_tria_wire_get(void);
struct GPUBatch *DRW_cache_camera_tria_get(void);
struct GPUBatch *DRW_cache_camera_distances_get(void);

/* Speaker */

struct GPUBatch *DRW_cache_speaker_get(void);

/* Probe */

struct GPUBatch *DRW_cache_lightprobe_cube_get(void);
struct GPUBatch *DRW_cache_lightprobe_grid_get(void);
struct GPUBatch *DRW_cache_lightprobe_planar_get(void);

/* Bones */

struct GPUBatch *DRW_cache_bone_octahedral_get(void);
struct GPUBatch *DRW_cache_bone_octahedral_wire_get(void);
struct GPUBatch *DRW_cache_bone_box_get(void);
struct GPUBatch *DRW_cache_bone_box_wire_get(void);
struct GPUBatch *DRW_cache_bone_envelope_solid_get(void);
struct GPUBatch *DRW_cache_bone_envelope_outline_get(void);
struct GPUBatch *DRW_cache_bone_point_get(void);
struct GPUBatch *DRW_cache_bone_point_wire_outline_get(void);
struct GPUBatch *DRW_cache_bone_stick_get(void);
struct GPUBatch *DRW_cache_bone_arrows_get(void);
struct GPUBatch *DRW_cache_bone_dof_sphere_get(void);
struct GPUBatch *DRW_cache_bone_dof_lines_get(void);

/* Meshes */

struct GPUBatch *DRW_cache_mesh_all_verts_get(struct Object *ob);
struct GPUBatch *DRW_cache_mesh_all_edges_get(struct Object *ob);
struct GPUBatch *DRW_cache_mesh_loose_edges_get(struct Object *ob);
struct GPUBatch *DRW_cache_mesh_edge_detection_get(struct Object *ob, bool *r_is_manifold);
struct GPUBatch *DRW_cache_mesh_surface_get(struct Object *ob);
struct GPUBatch *DRW_cache_mesh_surface_edges_get(struct Object *ob);
/**
 * Return list of batches with length equal to `max(1, totcol)`.
 */
struct GPUBatch **DRW_cache_mesh_surface_shaded_get(struct Object *ob,
                                                    struct GPUMaterial **gpumat_array,
                                                    uint gpumat_array_len);
/**
 * Return list of batches with length equal to `max(1, totcol)`.
 */
struct GPUBatch **DRW_cache_mesh_surface_texpaint_get(struct Object *ob);
struct GPUBatch *DRW_cache_mesh_surface_texpaint_single_get(struct Object *ob);
struct GPUBatch *DRW_cache_mesh_surface_vertpaint_get(struct Object *ob);
struct GPUBatch *DRW_cache_mesh_surface_sculptcolors_get(struct Object *ob);
struct GPUBatch *DRW_cache_mesh_surface_weights_get(struct Object *ob);
struct GPUBatch *DRW_cache_mesh_surface_mesh_analysis_get(struct Object *ob);
struct GPUBatch *DRW_cache_mesh_face_wireframe_get(struct Object *ob);
struct GPUBatch *DRW_cache_mesh_surface_viewer_attribute_get(struct Object *ob);

/* Curve */

struct GPUBatch *DRW_cache_curve_edge_wire_get(struct Object *ob);
struct GPUBatch *DRW_cache_curve_edge_wire_viewer_attribute_get(struct Object *ob);

/* edit-mode */

struct GPUBatch *DRW_cache_curve_edge_normal_get(struct Object *ob);
struct GPUBatch *DRW_cache_curve_edge_overlay_get(struct Object *ob);
struct GPUBatch *DRW_cache_curve_vert_overlay_get(struct Object *ob);

/* Font */

struct GPUBatch *DRW_cache_text_edge_wire_get(struct Object *ob);

/* Surface */

struct GPUBatch *DRW_cache_surf_edge_wire_get(struct Object *ob);

/* Lattice */

struct GPUBatch *DRW_cache_lattice_verts_get(struct Object *ob);
struct GPUBatch *DRW_cache_lattice_wire_get(struct Object *ob, bool use_weight);
struct GPUBatch *DRW_cache_lattice_vert_overlay_get(struct Object *ob);

/* Particles */

struct GPUBatch *DRW_cache_particles_get_hair(struct Object *object,
                                              struct ParticleSystem *psys,
                                              struct ModifierData *md);
struct GPUBatch *DRW_cache_particles_get_dots(struct Object *object, struct ParticleSystem *psys);
struct GPUBatch *DRW_cache_particles_get_edit_strands(struct Object *object,
                                                      struct ParticleSystem *psys,
                                                      struct PTCacheEdit *edit,
                                                      bool use_weight);
struct GPUBatch *DRW_cache_particles_get_edit_inner_points(struct Object *object,
                                                           struct ParticleSystem *psys,
                                                           struct PTCacheEdit *edit);
struct GPUBatch *DRW_cache_particles_get_edit_tip_points(struct Object *object,
                                                         struct ParticleSystem *psys,
                                                         struct PTCacheEdit *edit);
struct GPUBatch *DRW_cache_particles_get_prim(int type);

/* Curves */

struct GPUBatch *DRW_cache_curves_surface_get(struct Object *ob);
struct GPUBatch **DRW_cache_curves_surface_shaded_get(struct Object *ob,
                                                      struct GPUMaterial **gpumat_array,
                                                      uint gpumat_array_len);
struct GPUBatch *DRW_cache_curves_face_wireframe_get(struct Object *ob);
struct GPUBatch *DRW_cache_curves_edge_detection_get(struct Object *ob, bool *r_is_manifold);

/* Volume */

typedef struct DRWVolumeGrid {
  struct DRWVolumeGrid *next, *prev;

  /* Grid name. */
  char *name;

  /* 3D texture. */
  struct GPUTexture *texture;

  /* Transform between 0..1 texture space and object space. */
  float texture_to_object[4][4];
  float object_to_texture[4][4];

  /* Transform from bounds to texture space. */
  float object_to_bounds[4][4];
  float bounds_to_texture[4][4];
} DRWVolumeGrid;

DRWVolumeGrid *DRW_volume_batch_cache_get_grid(struct Volume *volume,
                                               const struct VolumeGrid *grid);
struct GPUBatch *DRW_cache_volume_face_wireframe_get(struct Object *ob);
struct GPUBatch *DRW_cache_volume_selection_surface_get(struct Object *ob);

/* GPencil (legacy) */

struct GPUBatch *DRW_cache_gpencil_get(struct Object *ob, int cfra);
struct GPUVertBuf *DRW_cache_gpencil_position_buffer_get(struct Object *ob, int cfra);
struct GPUVertBuf *DRW_cache_gpencil_color_buffer_get(struct Object *ob, int cfra);
struct GPUBatch *DRW_cache_gpencil_edit_lines_get(struct Object *ob, int cfra);
struct GPUBatch *DRW_cache_gpencil_edit_points_get(struct Object *ob, int cfra);
struct GPUBatch *DRW_cache_gpencil_edit_curve_handles_get(struct Object *ob, int cfra);
struct GPUBatch *DRW_cache_gpencil_edit_curve_points_get(struct Object *ob, int cfra);
struct GPUBatch *DRW_cache_gpencil_sbuffer_get(struct Object *ob, bool show_fill);
struct GPUVertBuf *DRW_cache_gpencil_sbuffer_position_buffer_get(struct Object *ob,
                                                                 bool show_fill);
struct GPUVertBuf *DRW_cache_gpencil_sbuffer_color_buffer_get(struct Object *ob, bool show_fill);
int DRW_gpencil_material_count_get(struct bGPdata *gpd);

struct GPUBatch *DRW_cache_gpencil_face_wireframe_get(struct Object *ob);

struct bGPDstroke *DRW_cache_gpencil_sbuffer_stroke_data_get(struct Object *ob);
/**
 * Sbuffer batches are temporary. We need to clear it after drawing.
 */
void DRW_cache_gpencil_sbuffer_clear(struct Object *ob);

/* Grease Pencil */

struct GPUBatch *DRW_cache_grease_pencil_get(struct Object *ob, int cfra);
struct GPUBatch *DRW_cache_grease_pencil_edit_points_get(struct Object *ob, int cfra);
struct GPUVertBuf *DRW_cache_grease_pencil_position_buffer_get(struct Object *ob, int cfra);
struct GPUVertBuf *DRW_cache_grease_pencil_color_buffer_get(struct Object *ob, int cfra);

#ifdef __cplusplus
}
#endif
