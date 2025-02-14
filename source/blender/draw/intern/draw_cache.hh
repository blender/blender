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
struct GPUTexture;
namespace blender::gpu {
class Batch;
class VertBuf;
}  // namespace blender::gpu
struct ModifierData;
struct Object;
struct PTCacheEdit;
struct ParticleSystem;
struct Volume;
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
blender::gpu::Batch *DRW_cache_cursor_get(bool crosshair_lines);
/* Common Object */

blender::gpu::Batch *DRW_cache_object_all_edges_get(Object *ob);
blender::gpu::Batch *DRW_cache_object_edge_detection_get(Object *ob, bool *r_is_manifold);
blender::gpu::Batch *DRW_cache_object_surface_get(Object *ob);
blender::gpu::Batch *DRW_cache_object_loose_edges_get(Object *ob);
blender::Span<blender::gpu::Batch *> DRW_cache_object_surface_material_get(
    Object *ob, blender::Span<const GPUMaterial *> materials);
blender::gpu::Batch *DRW_cache_object_face_wireframe_get(const Scene *scene, Object *ob);

/**
 * Returns the vertbuf used by shaded surface batch.
 */
blender::gpu::VertBuf *DRW_cache_object_pos_vertbuf_get(Object *ob);

/* Meshes */

blender::gpu::Batch *DRW_cache_mesh_all_verts_get(Object *ob);
blender::gpu::Batch *DRW_cache_mesh_all_edges_get(Object *ob);
blender::gpu::Batch *DRW_cache_mesh_loose_edges_get(Object *ob);
blender::gpu::Batch *DRW_cache_mesh_edge_detection_get(Object *ob, bool *r_is_manifold);
blender::gpu::Batch *DRW_cache_mesh_surface_get(Object *ob);
blender::gpu::Batch *DRW_cache_mesh_surface_edges_get(Object *ob);
/**
 * Return list of batches with length equal to `max(1, totcol)`.
 */
blender::Span<blender::gpu::Batch *> DRW_cache_mesh_surface_shaded_get(
    Object *ob, blender::Span<const GPUMaterial *> materials);
/**
 * Return list of batches with length equal to `max(1, totcol)`.
 */
blender::Span<blender::gpu::Batch *> DRW_cache_mesh_surface_texpaint_get(Object *ob);
blender::gpu::Batch *DRW_cache_mesh_surface_texpaint_single_get(Object *ob);
blender::gpu::Batch *DRW_cache_mesh_surface_vertpaint_get(Object *ob);
blender::gpu::Batch *DRW_cache_mesh_surface_sculptcolors_get(Object *ob);
blender::gpu::Batch *DRW_cache_mesh_surface_weights_get(Object *ob);
blender::gpu::Batch *DRW_cache_mesh_surface_mesh_analysis_get(Object *ob);
blender::gpu::Batch *DRW_cache_mesh_face_wireframe_get(Object *ob);
blender::gpu::Batch *DRW_cache_mesh_surface_viewer_attribute_get(Object *ob);

/* Curve */

blender::gpu::Batch *DRW_cache_curve_edge_wire_get(Object *ob);
blender::gpu::Batch *DRW_cache_curve_edge_wire_viewer_attribute_get(Object *ob);

/* edit-mode */

blender::gpu::Batch *DRW_cache_curve_edge_normal_get(Object *ob);
blender::gpu::Batch *DRW_cache_curve_edge_overlay_get(Object *ob);
blender::gpu::Batch *DRW_cache_curve_vert_overlay_get(Object *ob);

/* Font */

blender::gpu::Batch *DRW_cache_text_edge_wire_get(Object *ob);

/* Surface */

blender::gpu::Batch *DRW_cache_surf_edge_wire_get(Object *ob);

/* Lattice */

blender::gpu::Batch *DRW_cache_lattice_verts_get(Object *ob);
blender::gpu::Batch *DRW_cache_lattice_wire_get(Object *ob, bool use_weight);
blender::gpu::Batch *DRW_cache_lattice_vert_overlay_get(Object *ob);

/* Particles */

blender::gpu::Batch *DRW_cache_particles_get_hair(Object *object,
                                                  ParticleSystem *psys,
                                                  ModifierData *md);
blender::gpu::Batch *DRW_cache_particles_get_dots(Object *object, ParticleSystem *psys);
blender::gpu::Batch *DRW_cache_particles_get_edit_strands(Object *object,
                                                          ParticleSystem *psys,
                                                          PTCacheEdit *edit,
                                                          bool use_weight);
blender::gpu::Batch *DRW_cache_particles_get_edit_inner_points(Object *object,
                                                               ParticleSystem *psys,
                                                               PTCacheEdit *edit);
blender::gpu::Batch *DRW_cache_particles_get_edit_tip_points(Object *object,
                                                             ParticleSystem *psys,
                                                             PTCacheEdit *edit);

/* Volume */

struct DRWVolumeGrid {
  DRWVolumeGrid *next, *prev;

  /* Grid name. */
  char *name;

  /* 3D texture. */
  GPUTexture *texture;

  /* Transform between 0..1 texture space and object space. */
  blender::float4x4 texture_to_object;
  blender::float4x4 object_to_texture;

  /* Transform from bounds to texture space. */
  blender::float4x4 object_to_bounds;
  blender::float4x4 bounds_to_texture;
};

namespace blender::draw {

DRWVolumeGrid *DRW_volume_batch_cache_get_grid(Volume *volume,
                                               const bke::VolumeGridData *volume_grid);
blender::gpu::Batch *DRW_cache_volume_face_wireframe_get(Object *ob);
blender::gpu::Batch *DRW_cache_volume_selection_surface_get(Object *ob);

/* Grease Pencil */

/* When there's no visible drawings in this grease pencil object, the returned `Batch` could be
 * nullptr as `grease_pencil_edit_batch_ensure` won't do anything in those cases. */
blender::gpu::Batch *DRW_cache_grease_pencil_get(const Scene *scene, Object *ob);
blender::gpu::Batch *DRW_cache_grease_pencil_edit_points_get(const Scene *scene, Object *ob);
blender::gpu::Batch *DRW_cache_grease_pencil_edit_lines_get(const Scene *scene, Object *ob);
gpu::VertBuf *DRW_cache_grease_pencil_position_buffer_get(const Scene *scene, Object *ob);
gpu::VertBuf *DRW_cache_grease_pencil_color_buffer_get(const Scene *scene, Object *ob);
blender::gpu::Batch *DRW_cache_grease_pencil_weight_points_get(const Scene *scene, Object *ob);
blender::gpu::Batch *DRW_cache_grease_pencil_weight_lines_get(const Scene *scene, Object *ob);
blender::gpu::Batch *DRW_cache_grease_pencil_face_wireframe_get(const Scene *scene, Object *ob);

}  // namespace blender::draw
