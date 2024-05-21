/* SPDX-FileCopyrightText: 2016 Blender Authors
 *
 * SPDX-License-Identifier: GPL-2.0-or-later */

/** \file
 * \ingroup draw
 */

#pragma once

struct GPUMaterial;
namespace blender::gpu {
class Batch;
class VertBuf;
}  // namespace blender::gpu
struct GPUUniformBuf;
struct ModifierData;
struct PTCacheEdit;
struct ParticleSystem;
struct TaskGraph;

struct Curve;
struct Curves;
struct Lattice;
struct Mesh;
struct PointCloud;
struct Volume;
struct bGPdata;
struct GreasePencil;

#include "BKE_mesh.h"

namespace blender::draw {

/* -------------------------------------------------------------------- */
/** \name Expose via BKE callbacks
 * \{ */

void DRW_curve_batch_cache_dirty_tag(Curve *cu, int mode);
void DRW_curve_batch_cache_validate(Curve *cu);
void DRW_curve_batch_cache_free(Curve *cu);

void DRW_mesh_batch_cache_dirty_tag(Mesh *mesh, eMeshBatchDirtyMode mode);
void DRW_mesh_batch_cache_validate(Object &object, Mesh &mesh);
void DRW_mesh_batch_cache_free(void *batch_cache);

void DRW_lattice_batch_cache_dirty_tag(Lattice *lt, int mode);
void DRW_lattice_batch_cache_validate(Lattice *lt);
void DRW_lattice_batch_cache_free(Lattice *lt);

void DRW_particle_batch_cache_dirty_tag(ParticleSystem *psys, int mode);
void DRW_particle_batch_cache_free(ParticleSystem *psys);

void DRW_gpencil_batch_cache_dirty_tag(bGPdata *gpd);
void DRW_gpencil_batch_cache_free(bGPdata *gpd);

void DRW_curves_batch_cache_dirty_tag(Curves *curves, int mode);
void DRW_curves_batch_cache_validate(Curves *curves);
void DRW_curves_batch_cache_free(Curves *curves);

void DRW_pointcloud_batch_cache_dirty_tag(PointCloud *pointcloud, int mode);
void DRW_pointcloud_batch_cache_validate(PointCloud *pointcloud);
void DRW_pointcloud_batch_cache_free(PointCloud *pointcloud);

void DRW_volume_batch_cache_dirty_tag(Volume *volume, int mode);
void DRW_volume_batch_cache_validate(Volume *volume);
void DRW_volume_batch_cache_free(Volume *volume);

void DRW_grease_pencil_batch_cache_dirty_tag(GreasePencil *grase_pencil, int mode);
void DRW_grease_pencil_batch_cache_validate(GreasePencil *grase_pencil);
void DRW_grease_pencil_batch_cache_free(GreasePencil *grase_pencil);

/** \} */

/* -------------------------------------------------------------------- */
/** \name Garbage Collection
 * \{ */

void DRW_batch_cache_free_old(Object *ob, int ctime);

/**
 * Thread safety need to be assured by caller. Don't call this during drawing.
 * \note For now this only free the shading batches / VBO if any cd layers is not needed anymore.
 */
void DRW_mesh_batch_cache_free_old(Mesh *mesh, int ctime);
void DRW_curves_batch_cache_free_old(Curves *curves, int ctime);
void DRW_pointcloud_batch_cache_free_old(PointCloud *pointcloud, int ctime);

/** \} */

/* -------------------------------------------------------------------- */
/** \name Generic
 * \{ */

void DRW_vertbuf_create_wiredata(gpu::VertBuf *vbo, int vert_len);

/** \} */

/* -------------------------------------------------------------------- */
/** \name Curve
 * \{ */

void DRW_curve_batch_cache_create_requested(Object *ob, const Scene *scene);

int DRW_curve_material_count_get(const Curve *cu);

blender::gpu::Batch *DRW_curve_batch_cache_get_wire_edge(Curve *cu);
blender::gpu::Batch *DRW_curve_batch_cache_get_wire_edge_viewer_attribute(Curve *cu);
blender::gpu::Batch *DRW_curve_batch_cache_get_normal_edge(Curve *cu);
blender::gpu::Batch *DRW_curve_batch_cache_get_edit_edges(Curve *cu);
blender::gpu::Batch *DRW_curve_batch_cache_get_edit_verts(Curve *cu);

/** \} */

/* -------------------------------------------------------------------- */
/** \name Lattice
 * \{ */

blender::gpu::Batch *DRW_lattice_batch_cache_get_all_edges(Lattice *lt,
                                                           bool use_weight,
                                                           int actdef);
blender::gpu::Batch *DRW_lattice_batch_cache_get_all_verts(Lattice *lt);
blender::gpu::Batch *DRW_lattice_batch_cache_get_edit_verts(Lattice *lt);

/** \} */

/* -------------------------------------------------------------------- */
/** \name Curves
 * \{ */

int DRW_curves_material_count_get(const Curves *curves);

/**
 * Provide GPU access to a specific evaluated attribute on curves.
 *
 * \return A pointer to location where the texture will be
 * stored, which will be filled by #DRW_shgroup_curves_create_sub.
 */
gpu::VertBuf **DRW_curves_texture_for_evaluated_attribute(Curves *curves,
                                                          const char *name,
                                                          bool *r_is_point_domain);

GPUUniformBuf *DRW_curves_batch_cache_ubo_storage(Curves *curves);
blender::gpu::Batch *DRW_curves_batch_cache_get_edit_points(Curves *curves);
blender::gpu::Batch *DRW_curves_batch_cache_get_sculpt_curves_cage(Curves *curves);
blender::gpu::Batch *DRW_curves_batch_cache_get_edit_curves_handles(Curves *curves);
blender::gpu::Batch *DRW_curves_batch_cache_get_edit_curves_lines(Curves *curves);

void DRW_curves_batch_cache_create_requested(Object *ob);

/** \} */

/* -------------------------------------------------------------------- */
/** \name PointCloud
 * \{ */

int DRW_pointcloud_material_count_get(const PointCloud *pointcloud);

gpu::VertBuf *DRW_pointcloud_position_and_radius_buffer_get(Object *ob);

gpu::VertBuf **DRW_pointcloud_evaluated_attribute(PointCloud *pointcloud, const char *name);
blender::gpu::Batch *DRW_pointcloud_batch_cache_get_dots(Object *ob);

void DRW_pointcloud_batch_cache_create_requested(Object *ob);

/** \} */

/* -------------------------------------------------------------------- */
/** \name Volume
 * \{ */

int DRW_volume_material_count_get(const Volume *volume);

blender::gpu::Batch *DRW_volume_batch_cache_get_wireframes_face(Volume *volume);
blender::gpu::Batch *DRW_volume_batch_cache_get_selection_surface(Volume *volume);

/** \} */

/* -------------------------------------------------------------------- */
/** \name Mesh
 * \{ */

/**
 * Can be called for any surface type. Mesh *mesh is the final mesh.
 */
void DRW_mesh_batch_cache_create_requested(TaskGraph &task_graph,
                                           Object &ob,
                                           Mesh &mesh,
                                           const Scene &scene,
                                           bool is_paint_mode,
                                           bool use_hide);

blender::gpu::Batch *DRW_mesh_batch_cache_get_all_verts(Mesh &mesh);
blender::gpu::Batch *DRW_mesh_batch_cache_get_all_edges(Mesh &mesh);
blender::gpu::Batch *DRW_mesh_batch_cache_get_loose_edges(Mesh &mesh);
blender::gpu::Batch *DRW_mesh_batch_cache_get_edge_detection(Mesh &mesh, bool *r_is_manifold);
blender::gpu::Batch *DRW_mesh_batch_cache_get_surface(Mesh &mesh);
blender::gpu::Batch *DRW_mesh_batch_cache_get_surface_edges(Object &object, Mesh &mesh);
blender::gpu::Batch **DRW_mesh_batch_cache_get_surface_shaded(Object &object,
                                                              Mesh &mesh,
                                                              GPUMaterial **gpumat_array,
                                                              uint gpumat_array_len);

blender::gpu::Batch **DRW_mesh_batch_cache_get_surface_texpaint(Object &object, Mesh &mesh);
blender::gpu::Batch *DRW_mesh_batch_cache_get_surface_texpaint_single(Object &object, Mesh &mesh);
blender::gpu::Batch *DRW_mesh_batch_cache_get_surface_vertpaint(Object &object, Mesh &mesh);
blender::gpu::Batch *DRW_mesh_batch_cache_get_surface_sculpt(Object &object, Mesh &mesh);
blender::gpu::Batch *DRW_mesh_batch_cache_get_surface_weights(Mesh &mesh);
blender::gpu::Batch *DRW_mesh_batch_cache_get_sculpt_overlays(Mesh &mesh);
blender::gpu::Batch *DRW_mesh_batch_cache_get_surface_viewer_attribute(Mesh &mesh);

/** \} */

/* -------------------------------------------------------------------- */
/** \name Edit-Mesh Drawing
 * \{ */

blender::gpu::Batch *DRW_mesh_batch_cache_get_edit_triangles(Mesh &mesh);
blender::gpu::Batch *DRW_mesh_batch_cache_get_edit_vertices(Mesh &mesh);
blender::gpu::Batch *DRW_mesh_batch_cache_get_edit_edges(Mesh &mesh);
blender::gpu::Batch *DRW_mesh_batch_cache_get_edit_vert_normals(Mesh &mesh);
blender::gpu::Batch *DRW_mesh_batch_cache_get_edit_loop_normals(Mesh &mesh);
blender::gpu::Batch *DRW_mesh_batch_cache_get_edit_facedots(Mesh &mesh);
blender::gpu::Batch *DRW_mesh_batch_cache_get_edit_skin_roots(Mesh &mesh);

/** \} */

/* -------------------------------------------------------------------- */
/** \name Edit-mesh Selection
 * \{ */

blender::gpu::Batch *DRW_mesh_batch_cache_get_triangles_with_select_id(Mesh &mesh);
blender::gpu::Batch *DRW_mesh_batch_cache_get_facedots_with_select_id(Mesh &mesh);
blender::gpu::Batch *DRW_mesh_batch_cache_get_edges_with_select_id(Mesh &mesh);
blender::gpu::Batch *DRW_mesh_batch_cache_get_verts_with_select_id(Mesh &mesh);

/** \} */

/* -------------------------------------------------------------------- */
/** \name Object Mode Wireframe Overlays
 * \{ */

blender::gpu::Batch *DRW_mesh_batch_cache_get_wireframes_face(Mesh &mesh);

/** \} */

/* -------------------------------------------------------------------- */
/** \name Edit-mesh UV Editor
 * \{ */

/**
 * Creates the #blender::gpu::Batch for drawing the UV Stretching Area Overlay.
 * Optional retrieves the total area or total uv area of the mesh.
 *
 * The `cache->tot_area` and cache->tot_uv_area` update are calculation are
 * only valid after calling `DRW_mesh_batch_cache_create_requested`.
 */
blender::gpu::Batch *DRW_mesh_batch_cache_get_edituv_faces_stretch_area(Object &object,
                                                                        Mesh &mesh,
                                                                        float **tot_area,
                                                                        float **tot_uv_area);
blender::gpu::Batch *DRW_mesh_batch_cache_get_edituv_faces_stretch_angle(Object &object,
                                                                         Mesh &mesh);
blender::gpu::Batch *DRW_mesh_batch_cache_get_edituv_faces(Object &object, Mesh &mesh);
blender::gpu::Batch *DRW_mesh_batch_cache_get_edituv_edges(Object &object, Mesh &mesh);
blender::gpu::Batch *DRW_mesh_batch_cache_get_edituv_verts(Object &object, Mesh &mesh);
blender::gpu::Batch *DRW_mesh_batch_cache_get_edituv_facedots(Object &object, Mesh &mesh);

/** \} */

/* -------------------------------------------------------------------- */
/** \name For Image UV Editor
 * \{ */

blender::gpu::Batch *DRW_mesh_batch_cache_get_uv_edges(Object &object, Mesh &mesh);
blender::gpu::Batch *DRW_mesh_batch_cache_get_edit_mesh_analysis(Mesh &mesh);

/** \} */

/* -------------------------------------------------------------------- */
/** \name For Direct Data Access
 * \{ */

gpu::VertBuf *DRW_mesh_batch_cache_pos_vertbuf_get(Mesh &mesh);

int DRW_mesh_material_count_get(const Object &object, const Mesh &mesh);

/* Edit mesh bitflags (is this the right place?) */
enum {
  VFLAG_VERT_ACTIVE = 1 << 0,
  VFLAG_VERT_SELECTED = 1 << 1,
  VFLAG_VERT_SELECTED_BEZT_HANDLE = 1 << 2,
  VFLAG_EDGE_ACTIVE = 1 << 3,
  VFLAG_EDGE_SELECTED = 1 << 4,
  VFLAG_EDGE_SEAM = 1 << 5,
  VFLAG_EDGE_SHARP = 1 << 6,
  VFLAG_EDGE_FREESTYLE = 1 << 7,
  /* Beware to not go over 1 << 7 (it's a byte flag). */
  /* NOTE: Grease pencil edit curve use another type of data format that allows for this value. */
  VFLAG_VERT_GPENCIL_BEZT_HANDLE = 1 << 30,
};

enum {
  VFLAG_FACE_ACTIVE = 1 << 0,
  VFLAG_FACE_SELECTED = 1 << 1,
  VFLAG_FACE_FREESTYLE = 1 << 2,
  VFLAG_VERT_UV_SELECT = 1 << 3,
  VFLAG_VERT_UV_PINNED = 1 << 4,
  VFLAG_EDGE_UV_SELECT = 1 << 5,
  VFLAG_FACE_UV_ACTIVE = 1 << 6,
  VFLAG_FACE_UV_SELECT = 1 << 7,
  /* Beware to not go over 1 << 7 (it's a byte flag). */
};

/** \} */

/* -------------------------------------------------------------------- */
/** \name Particles
 * \{ */

blender::gpu::Batch *DRW_particles_batch_cache_get_hair(Object *object,
                                                        ParticleSystem *psys,
                                                        ModifierData *md);
blender::gpu::Batch *DRW_particles_batch_cache_get_dots(Object *object, ParticleSystem *psys);
blender::gpu::Batch *DRW_particles_batch_cache_get_edit_strands(Object *object,
                                                                ParticleSystem *psys,
                                                                PTCacheEdit *edit,
                                                                bool use_weight);
blender::gpu::Batch *DRW_particles_batch_cache_get_edit_inner_points(Object *object,
                                                                     ParticleSystem *psys,
                                                                     PTCacheEdit *edit);
blender::gpu::Batch *DRW_particles_batch_cache_get_edit_tip_points(Object *object,
                                                                   ParticleSystem *psys,
                                                                   PTCacheEdit *edit);

/** \} */

}  // namespace blender::draw
