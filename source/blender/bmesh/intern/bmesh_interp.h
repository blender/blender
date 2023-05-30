/* SPDX-License-Identifier: GPL-2.0-or-later */

#pragma once

/** \file
 * \ingroup bmesh
 */

struct LinkNode;
struct MemArena;

#ifdef __cplusplus
extern "C" {
#endif

void BM_loop_interp_multires_ex(BMesh *bm,
                                BMLoop *l_dst,
                                const BMFace *f_src,
                                const float f_dst_center[3],
                                const float f_src_center[3],
                                int cd_loop_mdisp_offset);
/**
 * Project the multi-resolution grid in target onto f_src's set of multi-resolution grids.
 */
void BM_loop_interp_multires(BMesh *bm, BMLoop *l_dst, const BMFace *f_src);

void BM_face_interp_multires_ex(BMesh *bm,
                                BMFace *f_dst,
                                const BMFace *f_src,
                                const float f_dst_center[3],
                                const float f_src_center[3],
                                int cd_loop_mdisp_offset);
void BM_face_interp_multires(BMesh *bm, BMFace *f_dst, const BMFace *f_src);

void BM_vert_interp_from_face(BMesh *bm, BMVert *v_dst, const BMFace *f_src);

/**
 * \brief Data, Interpolate From Verts
 *
 * Interpolates per-vertex data from two sources to \a v_dst
 *
 * \note This is an exact match to #BM_data_interp_from_edges.
 */
void BM_data_interp_from_verts(
    BMesh *bm, const BMVert *v_src_1, const BMVert *v_src_2, BMVert *v_dst, float fac);
/**
 * \brief Data, Interpolate From Edges
 *
 * Interpolates per-edge data from two sources to \a e_dst.
 *
 * \note This is an exact match to #BM_data_interp_from_verts.
 */
void BM_data_interp_from_edges(
    BMesh *bm, const BMEdge *e_src_1, const BMEdge *e_src_2, BMEdge *e_dst, float fac);
/**
 * \brief Data Face-Vert Edge Interpolate
 *
 * Walks around the faces of \a e and interpolates
 * the loop data between two sources.
 */
void BM_data_interp_face_vert_edge(
    BMesh *bm, const BMVert *v_src_1, const BMVert *v_src_2, BMVert *v, BMEdge *e, float fac);

typedef struct BMCustomLayerReq {
  int type;
  const char *name;  /* Can be NULL. */
  int flag;
} BMCustomLayerReq;
void BM_data_layers_ensure(BMesh *bm, CustomData *data, BMCustomLayerReq *layers, int totlayer);

void BM_data_layer_add(BMesh *bm, CustomData *data, int type);
void BM_data_layer_add_named(BMesh *bm, CustomData *data, int type, const char *name);
void BM_data_layer_ensure_named(BMesh *bm, CustomData *data, int type, const char *name);
void BM_data_layer_free(BMesh *bm, CustomData *data, int type);

/** Ensure the dependent boolean layers exist for all face corner #CD_PROP_FLOAT2 layers. */
void BM_uv_map_ensure_select_and_pin_attrs(BMesh *bm);

void BM_uv_map_ensure_vert_select_attr(BMesh *bm, const char *uv_map_name);
void BM_uv_map_ensure_edge_select_attr(BMesh *bm, const char *uv_map_name);
void BM_uv_map_ensure_pin_attr(BMesh *bm, const char *uv_map_name);

/**
 * Remove a named custom data layer, if it existed. Return true if the layer was removed.
 */
bool BM_data_layer_free_named(BMesh *bm, CustomData *data, const char *name);
void BM_data_layer_free_n(BMesh *bm, CustomData *data, int type, int n);
void BM_data_layer_copy(BMesh *bm, CustomData *data, int type, int src_n, int dst_n);

float BM_elem_float_data_get(CustomData *cd, void *element, int type);
void BM_elem_float_data_set(CustomData *cd, void *element, int type, float val);

/**
 * \brief Data Interpolate From Face
 *
 * Projects target onto source, and pulls interpolated custom-data from source.
 *
 * \note Only handles loop custom-data. multi-res is handled.
 */
void BM_face_interp_from_face_ex(BMesh *bm,
                                 BMFace *f_dst,
                                 const BMFace *f_src,
                                 bool do_vertex,
                                 const void **blocks,
                                 const void **blocks_v,
                                 float (*cos_2d)[2],
                                 float axis_mat[3][3]);
void BM_face_interp_from_face(BMesh *bm, BMFace *f_dst, const BMFace *f_src, bool do_vertex);
/**
 * Projects a single loop, target, onto f_src for custom-data interpolation.
 * multi-resolution is handled.
 * \param do_vertex: When true the target's vert data will also get interpolated.
 */
void BM_loop_interp_from_face(
    BMesh *bm, BMLoop *l_dst, const BMFace *f_src, bool do_vertex, bool do_multires);

/**
 * Smooths boundaries between multi-res grids,
 * including some borders in adjacent faces.
 */
void BM_face_multires_bounds_smooth(BMesh *bm, BMFace *f);

struct LinkNode *BM_vert_loop_groups_data_layer_create(
    BMesh *bm, BMVert *v, int layer_n, const float *loop_weights, struct MemArena *arena);
/**
 * Take existing custom data and merge each fan's data.
 */
void BM_vert_loop_groups_data_layer_merge(BMesh *bm, struct LinkNode *groups, int layer_n);
/**
 * A version of #BM_vert_loop_groups_data_layer_merge
 * that takes an array of loop-weights (aligned with #BM_LOOPS_OF_VERT iterator).
 */
void BM_vert_loop_groups_data_layer_merge_weights(BMesh *bm,
                                                  struct LinkNode *groups,
                                                  int layer_n,
                                                  const float *loop_weights);

#ifdef __cplusplus
}
#endif
