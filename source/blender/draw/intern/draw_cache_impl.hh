/* SPDX-FileCopyrightText: 2016 Blender Authors
 *
 * SPDX-License-Identifier: GPL-2.0-or-later */

/** \file
 * \ingroup draw
 */

#pragma once

struct GPUBatch;
struct GPUMaterial;
struct GPUVertBuf;
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

/* -------------------------------------------------------------------- */
/** \name Expose via BKE callbacks
 * \{ */

void DRW_curve_batch_cache_dirty_tag(Curve *cu, int mode);
void DRW_curve_batch_cache_validate(Curve *cu);
void DRW_curve_batch_cache_free(Curve *cu);

void DRW_mesh_batch_cache_dirty_tag(Mesh *me, eMeshBatchDirtyMode mode);
void DRW_mesh_batch_cache_validate(Object *object, Mesh *me);
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
void DRW_mesh_batch_cache_free_old(Mesh *me, int ctime);
void DRW_curves_batch_cache_free_old(Curves *curves, int ctime);
void DRW_pointcloud_batch_cache_free_old(PointCloud *pointcloud, int ctime);

/** \} */

/* -------------------------------------------------------------------- */
/** \name Generic
 * \{ */

void DRW_vertbuf_create_wiredata(GPUVertBuf *vbo, int vert_len);

/** \} */

/* -------------------------------------------------------------------- */
/** \name Curve
 * \{ */

void DRW_curve_batch_cache_create_requested(Object *ob, const Scene *scene);

int DRW_curve_material_count_get(Curve *cu);

GPUBatch *DRW_curve_batch_cache_get_wire_edge(Curve *cu);
GPUBatch *DRW_curve_batch_cache_get_wire_edge_viewer_attribute(Curve *cu);
GPUBatch *DRW_curve_batch_cache_get_normal_edge(Curve *cu);
GPUBatch *DRW_curve_batch_cache_get_edit_edges(Curve *cu);
GPUBatch *DRW_curve_batch_cache_get_edit_verts(Curve *cu);

/** \} */

/* -------------------------------------------------------------------- */
/** \name Lattice
 * \{ */

GPUBatch *DRW_lattice_batch_cache_get_all_edges(Lattice *lt, bool use_weight, int actdef);
GPUBatch *DRW_lattice_batch_cache_get_all_verts(Lattice *lt);
GPUBatch *DRW_lattice_batch_cache_get_edit_verts(Lattice *lt);

/** \} */

/* -------------------------------------------------------------------- */
/** \name Curves
 * \{ */

int DRW_curves_material_count_get(Curves *curves);

/**
 * Provide GPU access to a specific evaluated attribute on curves.
 *
 * \return A pointer to location where the texture will be
 * stored, which will be filled by #DRW_shgroup_curves_create_sub.
 */
GPUVertBuf **DRW_curves_texture_for_evaluated_attribute(Curves *curves,
                                                        const char *name,
                                                        bool *r_is_point_domain);

GPUBatch *DRW_curves_batch_cache_get_edit_points(Curves *curves);
GPUBatch *DRW_curves_batch_cache_get_edit_lines(Curves *curves);

void DRW_curves_batch_cache_create_requested(Object *ob);

/** \} */

/* -------------------------------------------------------------------- */
/** \name PointCloud
 * \{ */

int DRW_pointcloud_material_count_get(PointCloud *pointcloud);

GPUVertBuf *DRW_pointcloud_position_and_radius_buffer_get(Object *ob);

GPUVertBuf **DRW_pointcloud_evaluated_attribute(PointCloud *pointcloud, const char *name);
GPUBatch *DRW_pointcloud_batch_cache_get_dots(Object *ob);

void DRW_pointcloud_batch_cache_create_requested(Object *ob);

/** \} */

/* -------------------------------------------------------------------- */
/** \name Volume
 * \{ */

int DRW_volume_material_count_get(Volume *volume);

GPUBatch *DRW_volume_batch_cache_get_wireframes_face(Volume *volume);
GPUBatch *DRW_volume_batch_cache_get_selection_surface(Volume *volume);

/** \} */

/* -------------------------------------------------------------------- */
/** \name Mesh
 * \{ */

/**
 * Can be called for any surface type. Mesh *me is the final mesh.
 */
void DRW_mesh_batch_cache_create_requested(TaskGraph *task_graph,
                                           Object *ob,
                                           Mesh *me,
                                           const Scene *scene,
                                           bool is_paint_mode,
                                           bool use_hide);

GPUBatch *DRW_mesh_batch_cache_get_all_verts(Mesh *me);
GPUBatch *DRW_mesh_batch_cache_get_all_edges(Mesh *me);
GPUBatch *DRW_mesh_batch_cache_get_loose_edges(Mesh *me);
GPUBatch *DRW_mesh_batch_cache_get_edge_detection(Mesh *me, bool *r_is_manifold);
GPUBatch *DRW_mesh_batch_cache_get_surface(Mesh *me);
GPUBatch *DRW_mesh_batch_cache_get_surface_edges(Object *object, Mesh *me);
GPUBatch **DRW_mesh_batch_cache_get_surface_shaded(Object *object,
                                                   Mesh *me,
                                                   GPUMaterial **gpumat_array,
                                                   uint gpumat_array_len);

GPUBatch **DRW_mesh_batch_cache_get_surface_texpaint(Object *object, Mesh *me);
GPUBatch *DRW_mesh_batch_cache_get_surface_texpaint_single(Object *object, Mesh *me);
GPUBatch *DRW_mesh_batch_cache_get_surface_vertpaint(Object *object, Mesh *me);
GPUBatch *DRW_mesh_batch_cache_get_surface_sculpt(Object *object, Mesh *me);
GPUBatch *DRW_mesh_batch_cache_get_surface_weights(Mesh *me);
GPUBatch *DRW_mesh_batch_cache_get_sculpt_overlays(Mesh *me);
GPUBatch *DRW_mesh_batch_cache_get_surface_viewer_attribute(Mesh *me);

/** \} */

/* -------------------------------------------------------------------- */
/** \name Edit-Mesh Drawing
 * \{ */

GPUBatch *DRW_mesh_batch_cache_get_edit_triangles(Mesh *me);
GPUBatch *DRW_mesh_batch_cache_get_edit_vertices(Mesh *me);
GPUBatch *DRW_mesh_batch_cache_get_edit_edges(Mesh *me);
GPUBatch *DRW_mesh_batch_cache_get_edit_vert_normals(Mesh *me);
GPUBatch *DRW_mesh_batch_cache_get_edit_loop_normals(Mesh *me);
GPUBatch *DRW_mesh_batch_cache_get_edit_facedots(Mesh *me);
GPUBatch *DRW_mesh_batch_cache_get_edit_skin_roots(Mesh *me);

/** \} */

/* -------------------------------------------------------------------- */
/** \name Edit-mesh Selection
 * \{ */

GPUBatch *DRW_mesh_batch_cache_get_triangles_with_select_id(Mesh *me);
GPUBatch *DRW_mesh_batch_cache_get_facedots_with_select_id(Mesh *me);
GPUBatch *DRW_mesh_batch_cache_get_edges_with_select_id(Mesh *me);
GPUBatch *DRW_mesh_batch_cache_get_verts_with_select_id(Mesh *me);

/** \} */

/* -------------------------------------------------------------------- */
/** \name Object Mode Wireframe Overlays
 * \{ */

GPUBatch *DRW_mesh_batch_cache_get_wireframes_face(Mesh *me);

/** \} */

/* -------------------------------------------------------------------- */
/** \name Edit-mesh UV Editor
 * \{ */

/**
 * Creates the #GPUBatch for drawing the UV Stretching Area Overlay.
 * Optional retrieves the total area or total uv area of the mesh.
 *
 * The `cache->tot_area` and cache->tot_uv_area` update are calculation are
 * only valid after calling `DRW_mesh_batch_cache_create_requested`.
 */
GPUBatch *DRW_mesh_batch_cache_get_edituv_faces_stretch_area(Object *object,
                                                             Mesh *me,
                                                             float **tot_area,
                                                             float **tot_uv_area);
GPUBatch *DRW_mesh_batch_cache_get_edituv_faces_stretch_angle(Object *object, Mesh *me);
GPUBatch *DRW_mesh_batch_cache_get_edituv_faces(Object *object, Mesh *me);
GPUBatch *DRW_mesh_batch_cache_get_edituv_edges(Object *object, Mesh *me);
GPUBatch *DRW_mesh_batch_cache_get_edituv_verts(Object *object, Mesh *me);
GPUBatch *DRW_mesh_batch_cache_get_edituv_facedots(Object *object, Mesh *me);

/** \} */

/* -------------------------------------------------------------------- */
/** \name For Image UV Editor
 * \{ */

GPUBatch *DRW_mesh_batch_cache_get_uv_edges(Object *object, Mesh *me);
GPUBatch *DRW_mesh_batch_cache_get_edit_mesh_analysis(Mesh *me);

/** \} */

/* -------------------------------------------------------------------- */
/** \name For Direct Data Access
 * \{ */

GPUVertBuf *DRW_mesh_batch_cache_pos_vertbuf_get(Mesh *me);

int DRW_mesh_material_count_get(const Object *object, const Mesh *me);

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

GPUBatch *DRW_particles_batch_cache_get_hair(Object *object,
                                             ParticleSystem *psys,
                                             ModifierData *md);
GPUBatch *DRW_particles_batch_cache_get_dots(Object *object, ParticleSystem *psys);
GPUBatch *DRW_particles_batch_cache_get_edit_strands(Object *object,
                                                     ParticleSystem *psys,
                                                     PTCacheEdit *edit,
                                                     bool use_weight);
GPUBatch *DRW_particles_batch_cache_get_edit_inner_points(Object *object,
                                                          ParticleSystem *psys,
                                                          PTCacheEdit *edit);
GPUBatch *DRW_particles_batch_cache_get_edit_tip_points(Object *object,
                                                        ParticleSystem *psys,
                                                        PTCacheEdit *edit);

/** \} */
