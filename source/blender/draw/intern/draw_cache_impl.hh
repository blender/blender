/* SPDX-FileCopyrightText: 2016 Blender Foundation
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

#include "BKE_mesh_types.hh"

#ifdef __cplusplus
extern "C" {
#endif

/* -------------------------------------------------------------------- */
/** \name Expose via BKE callbacks
 * \{ */

void DRW_curve_batch_cache_dirty_tag(struct Curve *cu, int mode);
void DRW_curve_batch_cache_validate(struct Curve *cu);
void DRW_curve_batch_cache_free(struct Curve *cu);

void DRW_mesh_batch_cache_dirty_tag(struct Mesh *me, eMeshBatchDirtyMode mode);
void DRW_mesh_batch_cache_validate(struct Object *object, struct Mesh *me);
void DRW_mesh_batch_cache_free(void *batch_cache);

void DRW_lattice_batch_cache_dirty_tag(struct Lattice *lt, int mode);
void DRW_lattice_batch_cache_validate(struct Lattice *lt);
void DRW_lattice_batch_cache_free(struct Lattice *lt);

void DRW_particle_batch_cache_dirty_tag(struct ParticleSystem *psys, int mode);
void DRW_particle_batch_cache_free(struct ParticleSystem *psys);

void DRW_gpencil_batch_cache_dirty_tag(struct bGPdata *gpd);
void DRW_gpencil_batch_cache_free(struct bGPdata *gpd);

void DRW_curves_batch_cache_dirty_tag(struct Curves *curves, int mode);
void DRW_curves_batch_cache_validate(struct Curves *curves);
void DRW_curves_batch_cache_free(struct Curves *curves);

void DRW_pointcloud_batch_cache_dirty_tag(struct PointCloud *pointcloud, int mode);
void DRW_pointcloud_batch_cache_validate(struct PointCloud *pointcloud);
void DRW_pointcloud_batch_cache_free(struct PointCloud *pointcloud);

void DRW_volume_batch_cache_dirty_tag(struct Volume *volume, int mode);
void DRW_volume_batch_cache_validate(struct Volume *volume);
void DRW_volume_batch_cache_free(struct Volume *volume);

void DRW_grease_pencil_batch_cache_dirty_tag(struct GreasePencil *grase_pencil, int mode);
void DRW_grease_pencil_batch_cache_validate(struct GreasePencil *grase_pencil);
void DRW_grease_pencil_batch_cache_free(struct GreasePencil *grase_pencil);

/** \} */

/* -------------------------------------------------------------------- */
/** \name Garbage Collection
 * \{ */

void DRW_batch_cache_free_old(struct Object *ob, int ctime);

/**
 * Thread safety need to be assured by caller. Don't call this during drawing.
 * \note For now this only free the shading batches / VBO if any cd layers is not needed anymore.
 */
void DRW_mesh_batch_cache_free_old(struct Mesh *me, int ctime);
void DRW_curves_batch_cache_free_old(struct Curves *curves, int ctime);
void DRW_pointcloud_batch_cache_free_old(struct PointCloud *pointcloud, int ctime);

/** \} */

/* -------------------------------------------------------------------- */
/** \name Generic
 * \{ */

void DRW_vertbuf_create_wiredata(struct GPUVertBuf *vbo, int vert_len);

/** \} */

/* -------------------------------------------------------------------- */
/** \name Curve
 * \{ */

void DRW_curve_batch_cache_create_requested(struct Object *ob, const struct Scene *scene);

int DRW_curve_material_count_get(struct Curve *cu);

struct GPUBatch *DRW_curve_batch_cache_get_wire_edge(struct Curve *cu);
struct GPUBatch *DRW_curve_batch_cache_get_wire_edge_viewer_attribute(struct Curve *cu);
struct GPUBatch *DRW_curve_batch_cache_get_normal_edge(struct Curve *cu);
struct GPUBatch *DRW_curve_batch_cache_get_edit_edges(struct Curve *cu);
struct GPUBatch *DRW_curve_batch_cache_get_edit_verts(struct Curve *cu);

/** \} */

/* -------------------------------------------------------------------- */
/** \name Lattice
 * \{ */

struct GPUBatch *DRW_lattice_batch_cache_get_all_edges(struct Lattice *lt,
                                                       bool use_weight,
                                                       int actdef);
struct GPUBatch *DRW_lattice_batch_cache_get_all_verts(struct Lattice *lt);
struct GPUBatch *DRW_lattice_batch_cache_get_edit_verts(struct Lattice *lt);

/** \} */

/* -------------------------------------------------------------------- */
/** \name Curves
 * \{ */

int DRW_curves_material_count_get(struct Curves *curves);

/**
 * Provide GPU access to a specific evaluated attribute on curves.
 *
 * \return A pointer to location where the texture will be
 * stored, which will be filled by #DRW_shgroup_curves_create_sub.
 */
struct GPUVertBuf **DRW_curves_texture_for_evaluated_attribute(struct Curves *curves,
                                                               const char *name,
                                                               bool *r_is_point_domain);

struct GPUBatch *DRW_curves_batch_cache_get_edit_points(struct Curves *curves);
struct GPUBatch *DRW_curves_batch_cache_get_edit_lines(struct Curves *curves);

void DRW_curves_batch_cache_create_requested(struct Object *ob);

/** \} */

/* -------------------------------------------------------------------- */
/** \name PointCloud
 * \{ */

int DRW_pointcloud_material_count_get(struct PointCloud *pointcloud);

struct GPUVertBuf *DRW_pointcloud_position_and_radius_buffer_get(struct Object *ob);

struct GPUVertBuf **DRW_pointcloud_evaluated_attribute(struct PointCloud *pointcloud,
                                                       const char *name);
struct GPUBatch *DRW_pointcloud_batch_cache_get_dots(struct Object *ob);

void DRW_pointcloud_batch_cache_create_requested(struct Object *ob);

/** \} */

/* -------------------------------------------------------------------- */
/** \name Volume
 * \{ */

int DRW_volume_material_count_get(struct Volume *volume);

struct GPUBatch *DRW_volume_batch_cache_get_wireframes_face(struct Volume *volume);
struct GPUBatch *DRW_volume_batch_cache_get_selection_surface(struct Volume *volume);

/** \} */

/* -------------------------------------------------------------------- */
/** \name Mesh
 * \{ */

/**
 * Can be called for any surface type. Mesh *me is the final mesh.
 */
void DRW_mesh_batch_cache_create_requested(struct TaskGraph *task_graph,
                                           struct Object *ob,
                                           struct Mesh *me,
                                           const struct Scene *scene,
                                           bool is_paint_mode,
                                           bool use_hide);

struct GPUBatch *DRW_mesh_batch_cache_get_all_verts(struct Mesh *me);
struct GPUBatch *DRW_mesh_batch_cache_get_all_edges(struct Mesh *me);
struct GPUBatch *DRW_mesh_batch_cache_get_loose_edges(struct Mesh *me);
struct GPUBatch *DRW_mesh_batch_cache_get_edge_detection(struct Mesh *me, bool *r_is_manifold);
struct GPUBatch *DRW_mesh_batch_cache_get_surface(struct Mesh *me);
struct GPUBatch *DRW_mesh_batch_cache_get_surface_edges(struct Object *object, struct Mesh *me);
struct GPUBatch **DRW_mesh_batch_cache_get_surface_shaded(struct Object *object,
                                                          struct Mesh *me,
                                                          struct GPUMaterial **gpumat_array,
                                                          uint gpumat_array_len);

struct GPUBatch **DRW_mesh_batch_cache_get_surface_texpaint(struct Object *object,
                                                            struct Mesh *me);
struct GPUBatch *DRW_mesh_batch_cache_get_surface_texpaint_single(struct Object *object,
                                                                  struct Mesh *me);
struct GPUBatch *DRW_mesh_batch_cache_get_surface_vertpaint(struct Object *object,
                                                            struct Mesh *me);
struct GPUBatch *DRW_mesh_batch_cache_get_surface_sculpt(struct Object *object, struct Mesh *me);
struct GPUBatch *DRW_mesh_batch_cache_get_surface_weights(struct Mesh *me);
struct GPUBatch *DRW_mesh_batch_cache_get_sculpt_overlays(struct Mesh *me);
struct GPUBatch *DRW_mesh_batch_cache_get_surface_viewer_attribute(struct Mesh *me);

/** \} */

/* -------------------------------------------------------------------- */
/** \name Edit-Mesh Drawing
 * \{ */

struct GPUBatch *DRW_mesh_batch_cache_get_edit_triangles(struct Mesh *me);
struct GPUBatch *DRW_mesh_batch_cache_get_edit_vertices(struct Mesh *me);
struct GPUBatch *DRW_mesh_batch_cache_get_edit_edges(struct Mesh *me);
struct GPUBatch *DRW_mesh_batch_cache_get_edit_vert_normals(struct Mesh *me);
struct GPUBatch *DRW_mesh_batch_cache_get_edit_loop_normals(struct Mesh *me);
struct GPUBatch *DRW_mesh_batch_cache_get_edit_facedots(struct Mesh *me);
struct GPUBatch *DRW_mesh_batch_cache_get_edit_skin_roots(struct Mesh *me);

/** \} */

/* -------------------------------------------------------------------- */
/** \name Edit-mesh Selection
 * \{ */

struct GPUBatch *DRW_mesh_batch_cache_get_triangles_with_select_id(struct Mesh *me);
struct GPUBatch *DRW_mesh_batch_cache_get_facedots_with_select_id(struct Mesh *me);
struct GPUBatch *DRW_mesh_batch_cache_get_edges_with_select_id(struct Mesh *me);
struct GPUBatch *DRW_mesh_batch_cache_get_verts_with_select_id(struct Mesh *me);

/** \} */

/* -------------------------------------------------------------------- */
/** \name Object Mode Wireframe Overlays
 * \{ */

struct GPUBatch *DRW_mesh_batch_cache_get_wireframes_face(struct Mesh *me);

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
struct GPUBatch *DRW_mesh_batch_cache_get_edituv_faces_stretch_area(struct Object *object,
                                                                    struct Mesh *me,
                                                                    float **tot_area,
                                                                    float **tot_uv_area);
struct GPUBatch *DRW_mesh_batch_cache_get_edituv_faces_stretch_angle(struct Object *object,
                                                                     struct Mesh *me);
struct GPUBatch *DRW_mesh_batch_cache_get_edituv_faces(struct Object *object, struct Mesh *me);
struct GPUBatch *DRW_mesh_batch_cache_get_edituv_edges(struct Object *object, struct Mesh *me);
struct GPUBatch *DRW_mesh_batch_cache_get_edituv_verts(struct Object *object, struct Mesh *me);
struct GPUBatch *DRW_mesh_batch_cache_get_edituv_facedots(struct Object *object, struct Mesh *me);

/** \} */

/* -------------------------------------------------------------------- */
/** \name For Image UV Editor
 * \{ */

struct GPUBatch *DRW_mesh_batch_cache_get_uv_edges(struct Object *object, struct Mesh *me);
struct GPUBatch *DRW_mesh_batch_cache_get_edit_mesh_analysis(struct Mesh *me);

/** \} */

/* -------------------------------------------------------------------- */
/** \name For Direct Data Access
 * \{ */

struct GPUVertBuf *DRW_mesh_batch_cache_pos_vertbuf_get(struct Mesh *me);

int DRW_mesh_material_count_get(const struct Object *object, const struct Mesh *me);

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

struct GPUBatch *DRW_particles_batch_cache_get_hair(struct Object *object,
                                                    struct ParticleSystem *psys,
                                                    struct ModifierData *md);
struct GPUBatch *DRW_particles_batch_cache_get_dots(struct Object *object,
                                                    struct ParticleSystem *psys);
struct GPUBatch *DRW_particles_batch_cache_get_edit_strands(struct Object *object,
                                                            struct ParticleSystem *psys,
                                                            struct PTCacheEdit *edit,
                                                            bool use_weight);
struct GPUBatch *DRW_particles_batch_cache_get_edit_inner_points(struct Object *object,
                                                                 struct ParticleSystem *psys,
                                                                 struct PTCacheEdit *edit);
struct GPUBatch *DRW_particles_batch_cache_get_edit_tip_points(struct Object *object,
                                                               struct ParticleSystem *psys,
                                                               struct PTCacheEdit *edit);

/** \} */

#ifdef __cplusplus
}
#endif
