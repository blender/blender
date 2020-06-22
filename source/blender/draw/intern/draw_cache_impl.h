/*
 * This program is free software; you can redistribute it and/or
 * modify it under the terms of the GNU General Public License
 * as published by the Free Software Foundation; either version 2
 * of the License, or (at your option) any later version.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with this program; if not, write to the Free Software Foundation,
 * Inc., 51 Franklin Street, Fifth Floor, Boston, MA 02110-1301, USA.
 *
 * Copyright 2016, Blender Foundation.
 */

/** \file
 * \ingroup draw
 */

#ifndef __DRAW_CACHE_IMPL_H__
#define __DRAW_CACHE_IMPL_H__

struct GPUBatch;
struct GPUIndexBuf;
struct GPUMaterial;
struct GPUVertBuf;
struct ListBase;
struct ModifierData;
struct PTCacheEdit;
struct ParticleSystem;
struct TaskGraph;

struct Curve;
struct Hair;
struct Lattice;
struct Mesh;
struct MetaBall;
struct PointCloud;
struct Volume;
struct bGPdata;

/* Expose via BKE callbacks */
void DRW_mball_batch_cache_dirty_tag(struct MetaBall *mb, int mode);
void DRW_mball_batch_cache_validate(struct MetaBall *mb);
void DRW_mball_batch_cache_free(struct MetaBall *mb);

void DRW_curve_batch_cache_dirty_tag(struct Curve *cu, int mode);
void DRW_curve_batch_cache_validate(struct Curve *cu);
void DRW_curve_batch_cache_free(struct Curve *cu);

void DRW_mesh_batch_cache_dirty_tag(struct Mesh *me, int mode);
void DRW_mesh_batch_cache_validate(struct Mesh *me);
void DRW_mesh_batch_cache_free(struct Mesh *me);

void DRW_lattice_batch_cache_dirty_tag(struct Lattice *lt, int mode);
void DRW_lattice_batch_cache_validate(struct Lattice *lt);
void DRW_lattice_batch_cache_free(struct Lattice *lt);

void DRW_particle_batch_cache_dirty_tag(struct ParticleSystem *psys, int mode);
void DRW_particle_batch_cache_free(struct ParticleSystem *psys);

void DRW_gpencil_batch_cache_dirty_tag(struct bGPdata *gpd);
void DRW_gpencil_batch_cache_free(struct bGPdata *gpd);

void DRW_hair_batch_cache_dirty_tag(struct Hair *hair, int mode);
void DRW_hair_batch_cache_validate(struct Hair *hair);
void DRW_hair_batch_cache_free(struct Hair *hair);

void DRW_pointcloud_batch_cache_dirty_tag(struct PointCloud *pointcloud, int mode);
void DRW_pointcloud_batch_cache_validate(struct PointCloud *pointcloud);
void DRW_pointcloud_batch_cache_free(struct PointCloud *pointcloud);

void DRW_volume_batch_cache_dirty_tag(struct Volume *volume, int mode);
void DRW_volume_batch_cache_validate(struct Volume *volume);
void DRW_volume_batch_cache_free(struct Volume *volume);

/* Garbage collection */
void DRW_batch_cache_free_old(struct Object *ob, int ctime);

void DRW_mesh_batch_cache_free_old(struct Mesh *me, int ctime);

/* Generic */
void DRW_vertbuf_create_wiredata(struct GPUVertBuf *vbo, const int vert_len);

/* Curve */
void DRW_curve_batch_cache_create_requested(struct Object *ob);

int DRW_curve_material_count_get(struct Curve *cu);

struct GPUBatch *DRW_curve_batch_cache_get_wire_edge(struct Curve *cu);
struct GPUBatch *DRW_curve_batch_cache_get_normal_edge(struct Curve *cu);
struct GPUBatch *DRW_curve_batch_cache_get_edge_detection(struct Curve *cu, bool *r_is_manifold);
struct GPUBatch *DRW_curve_batch_cache_get_edit_edges(struct Curve *cu);
struct GPUBatch *DRW_curve_batch_cache_get_edit_verts(struct Curve *cu);

struct GPUBatch *DRW_curve_batch_cache_get_triangles_with_normals(struct Curve *cu);
struct GPUBatch **DRW_curve_batch_cache_get_surface_shaded(struct Curve *cu,
                                                           struct GPUMaterial **gpumat_array,
                                                           uint gpumat_array_len);
struct GPUBatch *DRW_curve_batch_cache_get_wireframes_face(struct Curve *cu);

/* Metaball */
int DRW_metaball_material_count_get(struct MetaBall *mb);

struct GPUBatch *DRW_metaball_batch_cache_get_triangles_with_normals(struct Object *ob);
struct GPUBatch **DRW_metaball_batch_cache_get_surface_shaded(struct Object *ob,
                                                              struct MetaBall *mb,
                                                              struct GPUMaterial **gpumat_array,
                                                              uint gpumat_array_len);
struct GPUBatch *DRW_metaball_batch_cache_get_wireframes_face(struct Object *ob);
struct GPUBatch *DRW_metaball_batch_cache_get_edge_detection(struct Object *ob,
                                                             bool *r_is_manifold);

/* DispList */
void DRW_displist_vertbuf_create_pos_and_nor(struct ListBase *lb, struct GPUVertBuf *vbo);
void DRW_displist_vertbuf_create_wiredata(struct ListBase *lb, struct GPUVertBuf *vbo);
void DRW_displist_vertbuf_create_loop_pos_and_nor_and_uv_and_tan(struct ListBase *lb,
                                                                 struct GPUVertBuf *vbo_pos_nor,
                                                                 struct GPUVertBuf *vbo_uv,
                                                                 struct GPUVertBuf *vbo_tan);
void DRW_displist_indexbuf_create_lines_in_order(struct ListBase *lb, struct GPUIndexBuf *ibo);
void DRW_displist_indexbuf_create_triangles_in_order(struct ListBase *lb, struct GPUIndexBuf *ibo);
void DRW_displist_indexbuf_create_triangles_loop_split_by_material(struct ListBase *lb,
                                                                   struct GPUIndexBuf **ibo_mat,
                                                                   uint mat_len);
void DRW_displist_indexbuf_create_edges_adjacency_lines(struct ListBase *lb,
                                                        struct GPUIndexBuf *ibo,
                                                        bool *r_is_manifold);

/* Lattice */
struct GPUBatch *DRW_lattice_batch_cache_get_all_edges(struct Lattice *lt,
                                                       bool use_weight,
                                                       const int actdef);
struct GPUBatch *DRW_lattice_batch_cache_get_all_verts(struct Lattice *lt);
struct GPUBatch *DRW_lattice_batch_cache_get_edit_verts(struct Lattice *lt);

/* Hair */
int DRW_hair_material_count_get(struct Hair *hair);

/* PointCloud */
int DRW_pointcloud_material_count_get(struct PointCloud *pointcloud);

struct GPUBatch *DRW_pointcloud_batch_cache_get_dots(struct Object *ob);

/* Volume */
int DRW_volume_material_count_get(struct Volume *volume);

struct GPUBatch *DRW_volume_batch_cache_get_wireframes_face(struct Volume *volume);

/* Mesh */
void DRW_mesh_batch_cache_create_requested(struct TaskGraph *task_graph,
                                           struct Object *ob,
                                           struct Mesh *me,
                                           const struct Scene *scene,
                                           const bool is_paint_mode,
                                           const bool use_hide);

struct GPUBatch *DRW_mesh_batch_cache_get_all_verts(struct Mesh *me);
struct GPUBatch *DRW_mesh_batch_cache_get_all_edges(struct Mesh *me);
struct GPUBatch *DRW_mesh_batch_cache_get_loose_edges(struct Mesh *me);
struct GPUBatch *DRW_mesh_batch_cache_get_edge_detection(struct Mesh *me, bool *r_is_manifold);
struct GPUBatch *DRW_mesh_batch_cache_get_surface(struct Mesh *me);
struct GPUBatch *DRW_mesh_batch_cache_get_surface_edges(struct Mesh *me);
struct GPUBatch **DRW_mesh_batch_cache_get_surface_shaded(struct Mesh *me,
                                                          struct GPUMaterial **gpumat_array,
                                                          uint gpumat_array_len);
struct GPUBatch **DRW_mesh_batch_cache_get_surface_texpaint(struct Mesh *me);
struct GPUBatch *DRW_mesh_batch_cache_get_surface_texpaint_single(struct Mesh *me);
struct GPUBatch *DRW_mesh_batch_cache_get_surface_vertpaint(struct Mesh *me);
struct GPUBatch *DRW_mesh_batch_cache_get_surface_sculpt(struct Mesh *me);
struct GPUBatch *DRW_mesh_batch_cache_get_surface_weights(struct Mesh *me);
/* edit-mesh drawing */
struct GPUBatch *DRW_mesh_batch_cache_get_edit_triangles(struct Mesh *me);
struct GPUBatch *DRW_mesh_batch_cache_get_edit_vertices(struct Mesh *me);
struct GPUBatch *DRW_mesh_batch_cache_get_edit_edges(struct Mesh *me);
struct GPUBatch *DRW_mesh_batch_cache_get_edit_vnors(struct Mesh *me);
struct GPUBatch *DRW_mesh_batch_cache_get_edit_lnors(struct Mesh *me);
struct GPUBatch *DRW_mesh_batch_cache_get_edit_facedots(struct Mesh *me);
struct GPUBatch *DRW_mesh_batch_cache_get_edit_skin_roots(struct Mesh *me);
/* edit-mesh selection */
struct GPUBatch *DRW_mesh_batch_cache_get_triangles_with_select_id(struct Mesh *me);
struct GPUBatch *DRW_mesh_batch_cache_get_facedots_with_select_id(struct Mesh *me);
struct GPUBatch *DRW_mesh_batch_cache_get_edges_with_select_id(struct Mesh *me);
struct GPUBatch *DRW_mesh_batch_cache_get_verts_with_select_id(struct Mesh *me);
/* Object mode Wireframe overlays */
struct GPUBatch *DRW_mesh_batch_cache_get_wireframes_face(struct Mesh *me);
/* edit-mesh UV editor */
struct GPUBatch *DRW_mesh_batch_cache_get_edituv_faces_stretch_area(struct Mesh *me,
                                                                    float **tot_area,
                                                                    float **tot_uv_area);
struct GPUBatch *DRW_mesh_batch_cache_get_edituv_faces_stretch_angle(struct Mesh *me);
struct GPUBatch *DRW_mesh_batch_cache_get_edituv_faces(struct Mesh *me);
struct GPUBatch *DRW_mesh_batch_cache_get_edituv_edges(struct Mesh *me);
struct GPUBatch *DRW_mesh_batch_cache_get_edituv_verts(struct Mesh *me);
struct GPUBatch *DRW_mesh_batch_cache_get_edituv_facedots(struct Mesh *me);
/* For Image UV editor. */
struct GPUBatch *DRW_mesh_batch_cache_get_uv_edges(struct Mesh *me);
struct GPUBatch *DRW_mesh_batch_cache_get_edit_mesh_analysis(struct Mesh *me);

/* For direct data access. */
struct GPUVertBuf *DRW_mesh_batch_cache_pos_vertbuf_get(struct Mesh *me);
struct GPUVertBuf *DRW_curve_batch_cache_pos_vertbuf_get(struct Curve *cu);
struct GPUVertBuf *DRW_mball_batch_cache_pos_vertbuf_get(struct Object *ob);

int DRW_mesh_material_count_get(struct Mesh *me);

/* See 'common_globals_lib.glsl' for duplicate defines. */

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

/* Particles */
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

#endif /* __DRAW_CACHE_IMPL_H__ */
