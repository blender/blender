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

struct CurveCache;
struct GPUBatch;
struct GPUIndexBuf;
struct GPUMaterial;
struct GPUTexture;
struct GPUVertBuf;
struct ListBase;
struct ModifierData;
struct PTCacheEdit;
struct ParticleSystem;
struct SpaceImage;
struct ToolSettings;

struct Curve;
struct Lattice;
struct Mesh;
struct MetaBall;
struct bGPdata;

/* Expose via BKE callbacks */
void DRW_mball_batch_cache_dirty_tag(struct MetaBall *mb, int mode);
void DRW_mball_batch_cache_free(struct MetaBall *mb);

void DRW_curve_batch_cache_dirty_tag(struct Curve *cu, int mode);
void DRW_curve_batch_cache_free(struct Curve *cu);

void DRW_mesh_batch_cache_dirty_tag(struct Mesh *me, int mode);
void DRW_mesh_batch_cache_free(struct Mesh *me);

void DRW_lattice_batch_cache_dirty_tag(struct Lattice *lt, int mode);
void DRW_lattice_batch_cache_free(struct Lattice *lt);

void DRW_particle_batch_cache_dirty_tag(struct ParticleSystem *psys, int mode);
void DRW_particle_batch_cache_free(struct ParticleSystem *psys);

void DRW_gpencil_batch_cache_dirty_tag(struct bGPdata *gpd);
void DRW_gpencil_batch_cache_free(struct bGPdata *gpd);

/* Garbage collection */
void DRW_batch_cache_free_old(struct Object *ob, int ctime);

void DRW_mesh_batch_cache_free_old(struct Mesh *me, int ctime);

/* Curve */
void DRW_curve_batch_cache_create_requested(struct Object *ob);

struct GPUBatch *DRW_curve_batch_cache_get_wire_edge(struct Curve *cu);
struct GPUBatch *DRW_curve_batch_cache_get_normal_edge(struct Curve *cu);
struct GPUBatch *DRW_curve_batch_cache_get_edge_detection(struct Curve *cu, bool *r_is_manifold);
struct GPUBatch *DRW_curve_batch_cache_get_edit_edges(struct Curve *cu);
struct GPUBatch *DRW_curve_batch_cache_get_edit_verts(struct Curve *cu, bool handles);

struct GPUBatch *DRW_curve_batch_cache_get_triangles_with_normals(struct Curve *cu);
struct GPUBatch **DRW_curve_batch_cache_get_surface_shaded(struct Curve *cu,
                                                           struct GPUMaterial **gpumat_array,
                                                           uint gpumat_array_len);
struct GPUBatch *DRW_curve_batch_cache_get_wireframes_face(struct Curve *cu);
/* Metaball */
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
void DRW_displist_vertbuf_create_loop_pos_and_nor_and_uv(struct ListBase *lb,
                                                         struct GPUVertBuf *vbo_pos_nor,
                                                         struct GPUVertBuf *vbo_uv);
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

/* Mesh */
void DRW_mesh_batch_cache_create_requested(struct Object *ob,
                                           struct Mesh *me,
                                           const struct ToolSettings *ts,
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
                                                          uint gpumat_array_len,
                                                          char **auto_layer_names,
                                                          int **auto_layer_is_srgb,
                                                          int *auto_layer_count);
struct GPUBatch **DRW_mesh_batch_cache_get_surface_texpaint(struct Mesh *me);
struct GPUBatch *DRW_mesh_batch_cache_get_surface_texpaint_single(struct Mesh *me);
struct GPUBatch *DRW_mesh_batch_cache_get_surface_vertpaint(struct Mesh *me);
struct GPUBatch *DRW_mesh_batch_cache_get_surface_weights(struct Mesh *me);
/* edit-mesh drawing */
struct GPUBatch *DRW_mesh_batch_cache_get_edit_triangles(struct Mesh *me);
struct GPUBatch *DRW_mesh_batch_cache_get_edit_vertices(struct Mesh *me);
struct GPUBatch *DRW_mesh_batch_cache_get_edit_edges(struct Mesh *me);
struct GPUBatch *DRW_mesh_batch_cache_get_edit_lnors(struct Mesh *me);
struct GPUBatch *DRW_mesh_batch_cache_get_edit_facedots(struct Mesh *me);
/* edit-mesh selection */
struct GPUBatch *DRW_mesh_batch_cache_get_triangles_with_select_id(struct Mesh *me);
struct GPUBatch *DRW_mesh_batch_cache_get_facedots_with_select_id(struct Mesh *me);
struct GPUBatch *DRW_mesh_batch_cache_get_edges_with_select_id(struct Mesh *me);
struct GPUBatch *DRW_mesh_batch_cache_get_verts_with_select_id(struct Mesh *me);
/* Object mode Wireframe overlays */
struct GPUBatch *DRW_mesh_batch_cache_get_wireframes_face(struct Mesh *me);
/* edit-mesh UV editor */
struct GPUBatch *DRW_mesh_batch_cache_get_edituv_faces_strech_area(struct Mesh *me);
struct GPUBatch *DRW_mesh_batch_cache_get_edituv_faces_strech_angle(struct Mesh *me);
struct GPUBatch *DRW_mesh_batch_cache_get_edituv_faces(struct Mesh *me);
struct GPUBatch *DRW_mesh_batch_cache_get_edituv_edges(struct Mesh *me);
struct GPUBatch *DRW_mesh_batch_cache_get_edituv_verts(struct Mesh *me);
struct GPUBatch *DRW_mesh_batch_cache_get_edituv_facedots(struct Mesh *me);
/* For Image UV editor. */
struct GPUBatch *DRW_mesh_batch_cache_get_uv_edges(struct Mesh *me);
struct GPUBatch *DRW_mesh_batch_cache_get_edit_mesh_analysis(struct Mesh *me);

void DRW_mesh_cache_sculpt_coords_ensure(struct Mesh *me);

/* Edit mesh bitflags (is this the right place?) */
enum {
  VFLAG_VERT_ACTIVE = 1 << 0,
  VFLAG_VERT_SELECTED = 1 << 1,
  VFLAG_EDGE_ACTIVE = 1 << 2,
  VFLAG_EDGE_SELECTED = 1 << 3,
  VFLAG_EDGE_SEAM = 1 << 4,
  VFLAG_EDGE_SHARP = 1 << 5,
  VFLAG_EDGE_FREESTYLE = 1 << 6,
  /* Beware to not go over 1 << 7 (it's a byte flag)
   * (see gpu_shader_edit_mesh_overlay_geom.glsl) */
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
  /* Beware to not go over 1 << 7 (it's a byte flag)
   * (see gpu_shader_edit_mesh_overlay_geom.glsl) */
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

/* Common */
// #define DRW_DEBUG_MESH_CACHE_REQUEST

#ifdef DRW_DEBUG_MESH_CACHE_REQUEST
#  define DRW_ADD_FLAG_FROM_VBO_REQUEST(flag, vbo, value) \
    (flag |= DRW_vbo_requested(vbo) ? (printf("  VBO requested " #vbo "\n") ? value : value) : 0)
#  define DRW_ADD_FLAG_FROM_IBO_REQUEST(flag, ibo, value) \
    (flag |= DRW_ibo_requested(ibo) ? (printf("  IBO requested " #ibo "\n") ? value : value) : 0)
#else
#  define DRW_ADD_FLAG_FROM_VBO_REQUEST(flag, vbo, value) \
    (flag |= DRW_vbo_requested(vbo) ? (value) : 0)
#  define DRW_ADD_FLAG_FROM_IBO_REQUEST(flag, ibo, value) \
    (flag |= DRW_ibo_requested(ibo) ? (value) : 0)
#endif

/* Test and assign NULL if test fails */
#define DRW_TEST_ASSIGN_VBO(v) (v = (DRW_vbo_requested(v) ? (v) : NULL))
#define DRW_TEST_ASSIGN_IBO(v) (v = (DRW_ibo_requested(v) ? (v) : NULL))

struct GPUBatch *DRW_batch_request(struct GPUBatch **batch);
bool DRW_batch_requested(struct GPUBatch *batch, int prim_type);
void DRW_ibo_request(struct GPUBatch *batch, struct GPUIndexBuf **ibo);
bool DRW_ibo_requested(struct GPUIndexBuf *ibo);
void DRW_vbo_request(struct GPUBatch *batch, struct GPUVertBuf **vbo);
bool DRW_vbo_requested(struct GPUVertBuf *vbo);

#endif /* __DRAW_CACHE_IMPL_H__ */
