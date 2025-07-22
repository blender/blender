/* SPDX-FileCopyrightText: 2016 Blender Authors
 *
 * SPDX-License-Identifier: GPL-2.0-or-later */

/** \file
 * \ingroup draw
 */

#pragma once

#include "BLI_math_matrix_types.hh"
#include "BLI_span.hh"

#include "BKE_volume_grid_fwd.hh"

struct GPUMaterial;
namespace blender::gpu {
class Texture;
class Batch;
class VertBuf;
}  // namespace blender::gpu
struct ModifierData;
struct Object;
struct PTCacheEdit;
struct ParticleSystem;
struct Volume;
struct Scene;

namespace blender::draw {

/**
 * Shape resolution level of detail.
 */
enum eDRWLevelOfDetail {
  DRW_LOD_LOW = 0,
  DRW_LOD_MEDIUM = 1,
  DRW_LOD_HIGH = 2,

  DRW_LOD_MAX, /* Max number of level of detail */
};

/* Common Object */

gpu::Batch *DRW_cache_object_all_edges_get(Object *ob);
gpu::Batch *DRW_cache_object_edge_detection_get(Object *ob, bool *r_is_manifold);
gpu::Batch *DRW_cache_object_surface_get(Object *ob);
gpu::Batch *DRW_cache_object_loose_edges_get(Object *ob);
Span<gpu::Batch *> DRW_cache_object_surface_material_get(Object *ob,
                                                         Span<const GPUMaterial *> materials);
gpu::Batch *DRW_cache_object_face_wireframe_get(const Scene *scene, Object *ob);

/* Meshes */

gpu::Batch *DRW_cache_mesh_all_verts_get(Object *ob);
gpu::Batch *DRW_cache_mesh_paint_overlay_verts_get(Object *ob);
gpu::Batch *DRW_cache_mesh_all_edges_get(Object *ob);
gpu::Batch *DRW_cache_mesh_loose_edges_get(Object *ob);
gpu::Batch *DRW_cache_mesh_edge_detection_get(Object *ob, bool *r_is_manifold);
gpu::Batch *DRW_cache_mesh_surface_get(Object *ob);
gpu::Batch *DRW_cache_mesh_paint_overlay_surface_get(Object *ob);
gpu::Batch *DRW_cache_mesh_paint_overlay_edges_get(Object *ob);
/**
 * Return list of batches with length equal to `max(1, totcol)`.
 */
Span<gpu::Batch *> DRW_cache_mesh_surface_shaded_get(Object *ob,
                                                     Span<const GPUMaterial *> materials);
/**
 * Return list of batches with length equal to `max(1, totcol)`.
 */
Span<gpu::Batch *> DRW_cache_mesh_surface_texpaint_get(Object *ob);
gpu::Batch *DRW_cache_mesh_surface_texpaint_single_get(Object *ob);
gpu::Batch *DRW_cache_mesh_surface_vertpaint_get(Object *ob);
gpu::Batch *DRW_cache_mesh_surface_sculptcolors_get(Object *ob);
gpu::Batch *DRW_cache_mesh_surface_weights_get(Object *ob);
gpu::Batch *DRW_cache_mesh_surface_mesh_analysis_get(Object *ob);
gpu::Batch *DRW_cache_mesh_face_wireframe_get(Object *ob);
gpu::Batch *DRW_cache_mesh_surface_viewer_attribute_get(Object *ob);

/* Curve */

gpu::Batch *DRW_cache_curve_edge_wire_get(Object *ob);
gpu::Batch *DRW_cache_curve_edge_wire_viewer_attribute_get(Object *ob);

/* edit-mode */

gpu::Batch *DRW_cache_curve_edge_normal_get(Object *ob);
gpu::Batch *DRW_cache_curve_edge_overlay_get(Object *ob);
gpu::Batch *DRW_cache_curve_vert_overlay_get(Object *ob);

/* Font */

gpu::Batch *DRW_cache_text_edge_wire_get(Object *ob);

/* Surface */

gpu::Batch *DRW_cache_surf_edge_wire_get(Object *ob);

/* Lattice */

gpu::Batch *DRW_cache_lattice_verts_get(Object *ob);
gpu::Batch *DRW_cache_lattice_wire_get(Object *ob, bool use_weight);
gpu::Batch *DRW_cache_lattice_vert_overlay_get(Object *ob);

/* Point Cloud */

gpu::Batch *DRW_cache_pointcloud_vert_overlay_get(Object *ob);

/* Particles */

gpu::Batch *DRW_cache_particles_get_hair(Object *object, ParticleSystem *psys, ModifierData *md);
gpu::Batch *DRW_cache_particles_get_dots(Object *object, ParticleSystem *psys);
gpu::Batch *DRW_cache_particles_get_edit_strands(Object *object,
                                                 ParticleSystem *psys,
                                                 PTCacheEdit *edit,
                                                 bool use_weight);
gpu::Batch *DRW_cache_particles_get_edit_inner_points(Object *object,
                                                      ParticleSystem *psys,
                                                      PTCacheEdit *edit);
gpu::Batch *DRW_cache_particles_get_edit_tip_points(Object *object,
                                                    ParticleSystem *psys,
                                                    PTCacheEdit *edit);

/* Volume */

struct DRWVolumeGrid {
  DRWVolumeGrid *next, *prev;

  /* Grid name. */
  char *name;

  /* 3D texture. */
  gpu::Texture *texture;

  /* Transform between 0..1 texture space and object space. */
  float4x4 texture_to_object;
  float4x4 object_to_texture;

  /* Transform from bounds to texture space. */
  float4x4 object_to_bounds;
  float4x4 bounds_to_texture;
};

DRWVolumeGrid *DRW_volume_batch_cache_get_grid(Volume *volume,
                                               const bke::VolumeGridData *volume_grid);
gpu::Batch *DRW_cache_volume_face_wireframe_get(Object *ob);
gpu::Batch *DRW_cache_volume_selection_surface_get(Object *ob);

/* Grease Pencil */

/* When there's no visible drawings in this grease pencil object, the returned `Batch` could be
 * nullptr as `grease_pencil_edit_batch_ensure` won't do anything in those cases. */
gpu::Batch *DRW_cache_grease_pencil_get(const Scene *scene, Object *ob);
gpu::Batch *DRW_cache_grease_pencil_edit_points_get(const Scene *scene, Object *ob);
gpu::Batch *DRW_cache_grease_pencil_edit_lines_get(const Scene *scene, Object *ob);
gpu::VertBuf *DRW_cache_grease_pencil_position_buffer_get(const Scene *scene, Object *ob);
gpu::VertBuf *DRW_cache_grease_pencil_color_buffer_get(const Scene *scene, Object *ob);
gpu::Batch *DRW_cache_grease_pencil_weight_points_get(const Scene *scene, Object *ob);
gpu::Batch *DRW_cache_grease_pencil_weight_lines_get(const Scene *scene, Object *ob);
gpu::Batch *DRW_cache_grease_pencil_face_wireframe_get(const Scene *scene, Object *ob);

}  // namespace blender::draw
