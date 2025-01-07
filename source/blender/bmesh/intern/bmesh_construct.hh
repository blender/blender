/* SPDX-FileCopyrightText: 2023 Blender Authors
 *
 * SPDX-License-Identifier: GPL-2.0-or-later */

#pragma once

/** \file
 * \ingroup bmesh
 */

#include "bmesh_class.hh"
#include "bmesh_core.hh"

struct BMAllocTemplate;
struct BMCustomDataCopyMap;
struct Mesh;

/**
 * Fill in a vertex array from an edge array.
 *
 * \returns false if any verts aren't found.
 */
bool BM_verts_from_edges(BMVert **vert_arr, BMEdge **edge_arr, int len);

/**
 * Fill in an edge array from a vertex array (connected polygon loop).
 *
 * \returns false if any edges aren't found.
 */
bool BM_edges_from_verts(BMEdge **edge_arr, BMVert **vert_arr, int len);
/**
 * Fill in an edge array from a vertex array (connected polygon loop).
 * Creating edges as-needed.
 */
void BM_edges_from_verts_ensure(BMesh *bm, BMEdge **edge_arr, BMVert **vert_arr, int len);

/**
 * Makes an NGon from an un-ordered set of verts.
 *
 * Assumes:
 * - that verts are only once in the list.
 * - that the verts have roughly planer bounds
 * - that the verts are roughly circular
 *
 * There can be concave areas but overlapping folds from the center point will fail.
 *
 * A brief explanation of the method used
 * - find the center point
 * - find the normal of the vertex-cloud
 * - order the verts around the face based on their angle to the normal vector at the center point.
 *
 * \note Since this is a vertex-cloud there is no direction.
 */
void BM_verts_sort_radial_plane(BMVert **vert_arr, int len);

/**
 * \brief Make Quad/Triangle
 *
 * Creates a new quad or triangle from a list of 3 or 4 vertices.
 * If \a no_double is true, then a check is done to see if a face
 * with these vertices already exists and returns it instead.
 *
 * If a pointer to an example face is provided, its custom data
 * and properties will be copied to the new face.
 *
 * \note The winding of the face is determined by the order
 * of the vertices in the vertex array.
 */
BMFace *BM_face_create_quad_tri(BMesh *bm,
                                BMVert *v1,
                                BMVert *v2,
                                BMVert *v3,
                                BMVert *v4,
                                const BMFace *f_example,
                                eBMCreateFlag create_flag);

/**
 * \brief copies face loop data from shared adjacent faces.
 *
 * \param filter_fn: A function that filters the source loops before copying
 * (don't always want to copy all).
 *
 * \note when a matching edge is found, both loops of that edge are copied
 * this is done since the face may not be completely surrounded by faces,
 * this way: a quad with 2 connected quads on either side will still get all 4 loops updated
 */
void BM_face_copy_shared(BMesh *bm, BMFace *f, BMLoopFilterFunc filter_fn, void *user_data);

/**
 * \brief Make NGon
 *
 * Makes an ngon from an unordered list of edges.
 * Verts \a v1 and \a v2 define the winding of the new face.
 *
 * \a edges are not required to be ordered, simply to form
 * a single closed loop as a whole.
 *
 * \note While this function will work fine when the edges
 * are already sorted, if the edges are always going to be sorted,
 * #BM_face_create should be considered over this function as it
 * avoids some unnecessary work.
 */
BMFace *BM_face_create_ngon(BMesh *bm,
                            BMVert *v1,
                            BMVert *v2,
                            BMEdge **edges,
                            int len,
                            const BMFace *f_example,
                            eBMCreateFlag create_flag);
/**
 * Create an ngon from an array of sorted verts
 *
 * Special features this has over other functions.
 * - Optionally calculate winding based on surrounding edges.
 * - Optionally create edges between vertices.
 * - Uses verts so no need to find edges (handy when you only have verts)
 */
BMFace *BM_face_create_ngon_verts(BMesh *bm,
                                  BMVert **vert_arr,
                                  int len,
                                  const BMFace *f_example,
                                  eBMCreateFlag create_flag,
                                  bool calc_winding,
                                  bool create_edges);

/**
 * Copy attributes between elements with a precalculated map of copy operations. This significantly
 * improves performance when copying, since all the work of finding common layers doesn't have to
 * be done for every element.
 */
void BM_elem_attrs_copy(BMesh *bm,
                        const BMCustomDataCopyMap &cd_map,
                        const BMVert *src,
                        BMVert *dst);
void BM_elem_attrs_copy(BMesh *bm,
                        const BMCustomDataCopyMap &cd_map,
                        const BMEdge *src,
                        BMEdge *dst);
void BM_elem_attrs_copy(BMesh *bm,
                        const BMCustomDataCopyMap &cd_map,
                        const BMFace *src,
                        BMFace *dst);
void BM_elem_attrs_copy(BMesh *bm,
                        const BMCustomDataCopyMap &cd_map,
                        const BMLoop *src,
                        BMLoop *dst);

/** Copy attributes between elements in the same BMesh. */
void BM_elem_attrs_copy(BMesh *bm, const BMVert *src, BMVert *dst);
void BM_elem_attrs_copy(BMesh *bm, const BMEdge *src, BMEdge *dst);
void BM_elem_attrs_copy(BMesh *bm, const BMFace *src, BMFace *dst);
void BM_elem_attrs_copy(BMesh *bm, const BMLoop *src, BMLoop *dst);

void BM_elem_select_copy(BMesh *bm_dst, void *ele_dst_v, const void *ele_src_v);

/**
 * Initialize the `bm_dst` layers in preparation for populating its contents with multiple meshes.
 * Typically done using multiple calls to #BM_mesh_bm_from_me with the same `bm` argument).
 *
 * \note While the custom-data layers of all meshes are created, the active layers are set
 * by the first instance mesh containing that layer type.
 * This means the first mesh should always be the main mesh (from the user perspective),
 * as this is the mesh they have control over (active UV layer for rendering for example).
 */
void BM_mesh_copy_init_customdata_from_mesh_array(BMesh *bm_dst,
                                                  const Mesh *me_src_array[],
                                                  int me_src_array_len,
                                                  const BMAllocTemplate *allocsize);
void BM_mesh_copy_init_customdata_from_mesh(BMesh *bm_dst,
                                            const Mesh *me_src,
                                            const BMAllocTemplate *allocsize);
void BM_mesh_copy_init_customdata(BMesh *bm_dst, BMesh *bm_src, const BMAllocTemplate *allocsize);
/**
 * Similar to #BM_mesh_copy_init_customdata but copies all layers ignoring
 * flags like #CD_FLAG_NOCOPY.
 *
 * \param bm_dst: BMesh whose custom-data layers will be added.
 * \param bm_src: BMesh whose custom-data layers will be copied.
 * \param htype: Specifies which custom-data layers will be initiated.
 * \param allocsize: Initialize the memory-pool before use (may be an estimate).
 */
void BM_mesh_copy_init_customdata_all_layers(BMesh *bm_dst,
                                             BMesh *bm_src,
                                             char htype,
                                             const BMAllocTemplate *allocsize);
BMesh *BM_mesh_copy(BMesh *bm_old);
