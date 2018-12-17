/*
 * Copyright 2016, Blender Foundation.
 *
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
 * Contributor(s): Blender Institute
 *
 */

/** \file draw_cache_impl.h
 *  \ingroup draw
 */

#ifndef __DRAW_CACHE_IMPL_H__
#define __DRAW_CACHE_IMPL_H__

struct CurveCache;
struct GPUMaterial;
struct GPUTexture;
struct GPUBatch;
struct GPUIndexBuf;
struct GPUVertBuf;
struct ListBase;
struct ModifierData;
struct ParticleSystem;
struct PTCacheEdit;
struct SpaceImage;

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

/* Curve */
void DRW_curve_batch_cache_create_requested(struct Object *ob);

struct GPUBatch *DRW_curve_batch_cache_get_wire_edge(struct Curve *cu);
struct GPUBatch *DRW_curve_batch_cache_get_normal_edge(struct Curve *cu);
struct GPUBatch *DRW_curve_batch_cache_get_edit_edges(struct Curve *cu);
struct GPUBatch *DRW_curve_batch_cache_get_edit_verts(struct Curve *cu, bool handles);

struct GPUBatch *DRW_curve_batch_cache_get_triangles_with_normals(struct Curve *cu);
struct GPUBatch **DRW_curve_batch_cache_get_surface_shaded(
        struct Curve *cu, struct GPUMaterial **gpumat_array, uint gpumat_array_len);
struct GPUBatch *DRW_curve_batch_cache_get_wireframes_face(struct Curve *cu);

/* Metaball */
struct GPUBatch *DRW_metaball_batch_cache_get_triangles_with_normals(struct Object *ob);
struct GPUBatch **DRW_metaball_batch_cache_get_surface_shaded(struct Object *ob, struct MetaBall *mb, struct GPUMaterial **gpumat_array, uint gpumat_array_len);
struct GPUBatch *DRW_metaball_batch_cache_get_wireframes_face(struct Object *ob);

/* DispList */
void DRW_displist_vertbuf_create_pos_and_nor(struct ListBase *lb, struct GPUVertBuf *vbo);
void DRW_displist_vertbuf_create_pos_and_nor_and_uv_tess(
        struct ListBase *lb, struct GPUVertBuf *vbo_pos_nor, struct GPUVertBuf *vbo_uv);
void DRW_displist_vertbuf_create_wireframe_data_tess(struct ListBase *lb, struct GPUVertBuf *vbo);
void DRW_displist_indexbuf_create_triangles_in_order(struct ListBase *lb, struct GPUIndexBuf *vbo);
void DRW_displist_indexbuf_create_triangles_tess_split_by_material(
        struct ListBase *lb, struct GPUIndexBuf **ibo_mat, uint mat_len);

/* Lattice */
struct GPUBatch *DRW_lattice_batch_cache_get_all_edges(struct Lattice *lt, bool use_weight, const int actdef);
struct GPUBatch *DRW_lattice_batch_cache_get_all_verts(struct Lattice *lt);
struct GPUBatch *DRW_lattice_batch_cache_get_edit_verts(struct Lattice *lt);

/* Vertex Group Selection and display options */
struct DRW_MeshWeightState {
	int defgroup_active;
	int defgroup_len;

	short flags;
	char alert_mode;

	/* Set of all selected bones for Multipaint. */
	bool *defgroup_sel; /* [defgroup_len] */
	int   defgroup_sel_count;
};

/* DRW_MeshWeightState.flags */
enum {
	DRW_MESH_WEIGHT_STATE_MULTIPAINT          = (1 << 0),
	DRW_MESH_WEIGHT_STATE_AUTO_NORMALIZE      = (1 << 1),
};

void DRW_mesh_weight_state_clear(struct DRW_MeshWeightState *wstate);
void DRW_mesh_weight_state_copy(struct DRW_MeshWeightState *wstate_dst, const struct DRW_MeshWeightState *wstate_src);
bool DRW_mesh_weight_state_compare(const struct DRW_MeshWeightState *a, const struct DRW_MeshWeightState *b);

/* Mesh */
void DRW_mesh_batch_cache_create_requested(struct Object *ob, struct Mesh *me);

struct GPUBatch **DRW_mesh_batch_cache_get_surface_shaded(
        struct Mesh *me, struct GPUMaterial **gpumat_array, uint gpumat_array_len,
        char **auto_layer_names, int **auto_layer_is_srgb, int *auto_layer_count);
struct GPUBatch **DRW_mesh_batch_cache_get_surface_texpaint(struct Mesh *me);
struct GPUBatch *DRW_mesh_batch_cache_get_surface_texpaint_single(struct Mesh *me);
struct GPUBatch *DRW_mesh_batch_cache_get_wire_loops(struct Mesh *me);
struct GPUBatch *DRW_mesh_batch_cache_get_weight_overlay_faces(struct Mesh *me);
struct GPUBatch *DRW_mesh_batch_cache_get_weight_overlay_verts(struct Mesh *me);
struct GPUBatch *DRW_mesh_batch_cache_get_all_edges(struct Mesh *me);
struct GPUBatch *DRW_mesh_batch_cache_get_all_triangles(struct Mesh *me);
struct GPUBatch *DRW_mesh_batch_cache_get_triangles_with_normals(struct Mesh *me);
struct GPUBatch *DRW_mesh_batch_cache_get_triangles_with_normals_and_weights(struct Mesh *me, const struct DRW_MeshWeightState *wstate);
struct GPUBatch *DRW_mesh_batch_cache_get_triangles_with_normals_and_vert_colors(struct Mesh *me);
struct GPUBatch *DRW_mesh_batch_cache_get_triangles_with_select_id(struct Mesh *me, bool use_hide, uint select_id_offset);
struct GPUBatch *DRW_mesh_batch_cache_get_triangles_with_select_mask(struct Mesh *me, bool use_hide);
struct GPUBatch *DRW_mesh_batch_cache_get_loose_edges_with_normals(struct Mesh *me);
struct GPUBatch *DRW_mesh_batch_cache_get_points_with_normals(struct Mesh *me);
struct GPUBatch *DRW_mesh_batch_cache_get_all_verts(struct Mesh *me);
struct GPUBatch *DRW_mesh_batch_cache_get_fancy_edges(struct Mesh *me);
struct GPUBatch *DRW_mesh_batch_cache_get_edge_detection(struct Mesh *me, bool *r_is_manifold);
struct GPUBatch *DRW_mesh_batch_cache_get_edit_triangles(struct Mesh *me);
struct GPUBatch *DRW_mesh_batch_cache_get_edit_triangles_nor(struct Mesh *me);
struct GPUBatch *DRW_mesh_batch_cache_get_edit_triangles_lnor(struct Mesh *me);
struct GPUBatch *DRW_mesh_batch_cache_get_edit_vertices(struct Mesh *me);
struct GPUBatch *DRW_mesh_batch_cache_get_edit_loose_edges(struct Mesh *me);
struct GPUBatch *DRW_mesh_batch_cache_get_edit_loose_edges_nor(struct Mesh *me);
struct GPUBatch *DRW_mesh_batch_cache_get_edit_loose_verts(struct Mesh *me);
struct GPUBatch *DRW_mesh_batch_cache_get_edit_facedots(struct Mesh *me);
/* edit-mesh selection (use generic function for faces) */
struct GPUBatch *DRW_mesh_batch_cache_get_facedots_with_select_id(struct Mesh *me, uint select_id_offset);
struct GPUBatch *DRW_mesh_batch_cache_get_edges_with_select_id(struct Mesh *me, uint select_id_offset);
struct GPUBatch *DRW_mesh_batch_cache_get_verts_with_select_id(struct Mesh *me, uint select_id_offset);
/* Object mode Wireframe overlays */
struct GPUBatch *DRW_mesh_batch_cache_get_wireframes_face(struct Mesh *me);

void DRW_mesh_cache_sculpt_coords_ensure(struct Mesh *me);

enum {
	UVEDIT_EDGES          = (1 << 0),
	UVEDIT_DATA           = (1 << 1),
	UVEDIT_FACEDOTS       = (1 << 2),
	UVEDIT_FACES          = (1 << 3),
	UVEDIT_STRETCH_ANGLE  = (1 << 4),
	UVEDIT_STRETCH_AREA   = (1 << 5),
	UVEDIT_SYNC_SEL       = (1 << 6),
};

/* For Image UV editor. */
struct GPUBatch *DRW_mesh_batch_cache_get_texpaint_loop_wire(struct Mesh *me);
void DRW_mesh_cache_uvedit(
        struct Object *me, struct SpaceImage *sima, struct Scene *scene, uchar state,
        struct GPUBatch **faces, struct GPUBatch **edges, struct GPUBatch **verts, struct GPUBatch **facedots);

/* Edit mesh bitflags (is this the right place?) */

enum {
	VFLAG_VERTEX_ACTIVE   = 1 << 0,
	VFLAG_VERTEX_SELECTED = 1 << 1,
	VFLAG_VERTEX_EXISTS   = 1 << 2,
	VFLAG_FACE_ACTIVE     = 1 << 3,
	VFLAG_FACE_SELECTED   = 1 << 4,
	VFLAG_FACE_FREESTYLE  = 1 << 5,
	/* Beware to not go over 1 << 7 (it's a byte flag)
	 * (see gpu_shader_edit_mesh_overlay_geom.glsl) */
};

enum {
	VFLAG_EDGE_EXISTS   = 1 << 0,
	VFLAG_EDGE_ACTIVE   = 1 << 1,
	VFLAG_EDGE_SELECTED = 1 << 2,
	VFLAG_EDGE_SEAM     = 1 << 3,
	VFLAG_EDGE_SHARP    = 1 << 4,
	VFLAG_EDGE_FREESTYLE = 1 << 5,
	/* Beware to not go over 1 << 7 (it's a byte flag)
	 * (see gpu_shader_edit_mesh_overlay_geom.glsl) */
};

/* Particles */
struct GPUBatch *DRW_particles_batch_cache_get_hair(
        struct Object *object, struct ParticleSystem *psys, struct ModifierData *md);
struct GPUBatch *DRW_particles_batch_cache_get_dots(
        struct Object *object, struct ParticleSystem *psys);
struct GPUBatch *DRW_particles_batch_cache_get_edit_strands(
        struct Object *object, struct ParticleSystem *psys, struct PTCacheEdit *edit, bool use_weight);
struct GPUBatch *DRW_particles_batch_cache_get_edit_inner_points(
        struct Object *object, struct ParticleSystem *psys, struct PTCacheEdit *edit);
struct GPUBatch *DRW_particles_batch_cache_get_edit_tip_points(
        struct Object *object, struct ParticleSystem *psys, struct PTCacheEdit *edit);

/* Common */
#define DRW_ADD_FLAG_FROM_VBO_REQUEST(flag, vbo, value) (flag |= DRW_vbo_requested(vbo) ? value : 0)
#define DRW_ADD_FLAG_FROM_IBO_REQUEST(flag, ibo, value) (flag |= DRW_ibo_requested(ibo) ? value : 0)

/* Test and assign NULL if test fails */
#define DRW_TEST_ASSIGN_VBO(v) (v = (DRW_vbo_requested(v) ? v : NULL))
#define DRW_TEST_ASSIGN_IBO(v) (v = (DRW_ibo_requested(v) ? v : NULL))

struct GPUBatch *DRW_batch_request(struct GPUBatch **batch);
bool DRW_batch_requested(struct GPUBatch *batch, int prim_type);
void DRW_ibo_request(struct GPUBatch *batch, struct GPUIndexBuf **ibo);
bool DRW_ibo_requested(struct GPUIndexBuf *ibo);
void DRW_vbo_request(struct GPUBatch *batch, struct GPUVertBuf **vbo);
bool DRW_vbo_requested(struct GPUVertBuf *vbo);

#endif /* __DRAW_CACHE_IMPL_H__ */
