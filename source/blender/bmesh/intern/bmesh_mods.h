/* SPDX-FileCopyrightText: 2023 Blender Foundation
 *
 * SPDX-License-Identifier: GPL-2.0-or-later */

#pragma once

/** \file
 * \ingroup bmesh
 */

#ifdef __cplusplus
extern "C" {
#endif

/**
 * \brief Dissolve Vert
 *
 * Turns the face region surrounding a manifold vertex into a single polygon.
 *
 * \par Example:
 * <pre>
 *              +---------+             +---------+
 *              |  \   /  |             |         |
 *     Before:  |    v    |      After: |         |
 *              |  /   \  |             |         |
 *              +---------+             +---------+
 * </pre>
 *
 * This function can also collapse edges too
 * in cases when it can't merge into faces.
 *
 * \par Example:
 * <pre>
 *     Before:  +----v----+      After: +---------+
 * </pre>
 *
 * \note dissolves vert, in more situations than BM_disk_dissolve
 * (e.g. if the vert is part of a wire edge, etc).
 */
bool BM_vert_dissolve(BMesh *bm, BMVert *v);

/**
 * dissolves all faces around a vert, and removes it.
 */
bool BM_disk_dissolve(BMesh *bm, BMVert *v);

/**
 * \brief Faces Join Pair
 *
 * Joins two adjacent faces together.
 *
 * \note This method calls to #BM_faces_join to do its work.
 * This means connected edges which also share the two faces will be joined.
 *
 * If the windings do not match the winding of the new face will follow
 * \a l_a's winding (i.e. \a l_b will be reversed before the join).
 *
 * \return The combined face or NULL on failure.
 */
BMFace *BM_faces_join_pair(BMesh *bm, BMLoop *l_a, BMLoop *l_b, bool do_del);

/** see: bmesh_polygon_edgenet.h for #BM_face_split_edgenet */

/**
 * \brief Face Split
 *
 * Split a face along two vertices. returns the newly made face, and sets
 * the \a r_l member to a loop in the newly created edge.
 *
 * \param bm: The bmesh
 * \param f: the original face
 * \param l_a, l_b: Loops of this face, their vertices define
 * the split edge to be created (must be differ and not can't be adjacent in the face).
 * \param r_l: pointer which will receive the BMLoop for the split edge in the new face
 * \param example: Edge used for attributes of splitting edge, if non-NULL
 * \param no_double: Use an existing edge if found
 *
 * \return Pointer to the newly created face representing one side of the split
 * if the split is successful (and the original face will be the other side).
 * NULL if the split fails.
 */
BMFace *BM_face_split(
    BMesh *bm, BMFace *f, BMLoop *l_a, BMLoop *l_b, BMLoop **r_l, BMEdge *example, bool no_double);

/**
 * \brief Face Split with intermediate points
 *
 * Like BM_face_split, but with an edge split by \a n intermediate points with given coordinates.
 *
 * \param bm: The bmesh.
 * \param f: the original face.
 * \param l_a, l_b: Vertices which define the split edge, must be different.
 * \param cos: Array of coordinates for intermediate points.
 * \param n: Length of \a cos (must be > 0).
 * \param r_l: pointer which will receive the BMLoop.
 * for the first split edge (from \a l_a) in the new face.
 * \param example: Edge used for attributes of splitting edge, if non-NULL.
 *
 * \return Pointer to the newly created face representing one side of the split
 * if the split is successful (and the original face will be the other side).
 * NULL if the split fails.
 */
BMFace *BM_face_split_n(BMesh *bm,
                        BMFace *f,
                        BMLoop *l_a,
                        BMLoop *l_b,
                        float cos[][3],
                        int n,
                        BMLoop **r_l,
                        BMEdge *example);

/**
 * \brief Vert Collapse Faces
 *
 * Collapses vertex \a v_kill that has only two manifold edges
 * onto a vertex it shares an edge with.
 * \a fac defines the amount of interpolation for Custom Data.
 *
 * \note that this is not a general edge collapse function.
 *
 * \note this function is very close to #BM_vert_collapse_edge,
 * both collapse a vertex and return a new edge.
 * Except this takes a factor and merges custom data.
 *
 * \param bm: The bmesh
 * \param e_kill: The edge to collapse
 * \param v_kill: The vertex  to collapse into the edge
 * \param fac: The factor along the edge
 * \param join_faces: When true the faces around the vertex will be joined
 * otherwise collapse the vertex by merging the 2 edges this vert touches into one.
 * \param kill_degenerate_faces: Removes faces with less than 3 verts after collapsing.
 *
 * \returns The New Edge
 */
BMEdge *BM_vert_collapse_faces(BMesh *bm,
                               BMEdge *e_kill,
                               BMVert *v_kill,
                               float fac,
                               bool do_del,
                               bool join_faces,
                               bool kill_degenerate_faces,
                               bool kill_duplicate_faces);
/**
 * \brief Vert Collapse Faces
 *
 * Collapses a vertex onto another vertex it shares an edge with.
 *
 * \return The New Edge
 */
BMEdge *BM_vert_collapse_edge(BMesh *bm,
                              BMEdge *e_kill,
                              BMVert *v_kill,
                              bool do_del,
                              bool kill_degenerate_faces,
                              bool kill_duplicate_faces);

/**
 * Collapse and edge into a single vertex.
 */
BMVert *BM_edge_collapse(BMesh *bm,
                         BMEdge *e_kill,
                         BMVert *v_kill,
                         const bool do_del,
                         const bool kill_degenerate_faces,
                         const bool combine_flags,
                         const bool full_non_manifold_collapse);

/**
 * \brief Edge Split
 *
 * <pre>
 * Before: v
 *         +-----------------------------------+
 *                           e
 *
 * After:  v                 v_new (returned)
 *         +-----------------+-----------------+
 *                 r_e                e
 * </pre>
 *
 * \param e: The edge to split.
 * \param v: One of the vertices in \a e and defines the "from" end of the splitting operation,
 * the new vertex will be \a fac of the way from \a v to the other end.
 * \param r_e: The newly created edge.
 * \return  The new vertex.
 */
BMVert *BM_edge_split(BMesh *bm, BMEdge *e, BMVert *v, BMEdge **r_e, float fac);

/**
 * \brief Split an edge multiple times evenly
 *
 * \param r_varr: Optional array, verts in between (v1 -> v2)
 */
BMVert *BM_edge_split_n(BMesh *bm, BMEdge *e, int numcuts, BMVert **r_varr);

/**
 * Swap v1 & v2
 *
 * \note Typically we shouldn't care about this, however it's used when extruding wire edges.
 */
void BM_edge_verts_swap(BMEdge *e);

bool BM_face_validate(BMFace *face, FILE *err);

/**
 * Calculate the 2 loops which _would_ make up the newly rotated Edge
 * but don't actually change anything.
 *
 * Use this to further inspect if the loops to be connected have issues:
 *
 * Examples:
 * - the newly formed edge already exists
 * - the new face would be degenerate (zero area / concave /  bow-tie)
 * - may want to measure if the new edge gives improved results topology.
 *   over the old one, as with beauty fill.
 *
 * \note #BM_edge_rotate_check must have already run.
 */
void BM_edge_calc_rotate(BMEdge *e, bool ccw, BMLoop **r_l1, BMLoop **r_l2);
/**
 * \brief Check if Rotate Edge is OK
 *
 * Quick check to see if we could rotate the edge,
 * use this to avoid calling exceptions on common cases.
 */
bool BM_edge_rotate_check(BMEdge *e);
/**
 * \brief Check if Edge Rotate Gives Degenerate Faces
 *
 * Check 2 cases
 * 1) does the newly forms edge form a flipped face (compare with previous cross product)
 * 2) does the newly formed edge cause a zero area corner (or close enough to be almost zero)
 *
 * \param e: The edge to test rotation.
 * \param l1, l2: are the loops of the proposed verts to rotate too and should
 * be the result of calling #BM_edge_calc_rotate
 */
bool BM_edge_rotate_check_degenerate(BMEdge *e, BMLoop *l1, BMLoop *l2);
bool BM_edge_rotate_check_beauty(BMEdge *e, BMLoop *l1, BMLoop *l2);
/**
 * \brief Rotate Edge
 *
 * Spins an edge topologically,
 * either counter-clockwise or clockwise depending on \a ccw.
 *
 * \return The spun edge, NULL on error
 * (e.g., if the edge isn't surrounded by exactly two faces).
 *
 * \note This works by dissolving the edge then re-creating it,
 * so the returned edge won't have the same pointer address as the original one.
 *
 * \see header definition for \a check_flag enum.
 */
BMEdge *BM_edge_rotate(BMesh *bm, BMEdge *e, bool ccw, short check_flag);

/** Flags for #BM_edge_rotate */
enum {
  /** Disallow rotating when the new edge matches an existing one. */
  BM_EDGEROT_CHECK_EXISTS = (1 << 0),
  /** Overrides existing check, if the edge already, rotate and merge them. */
  BM_EDGEROT_CHECK_SPLICE = (1 << 1),
  /** Disallow creating bow-tie, concave or zero area faces */
  BM_EDGEROT_CHECK_DEGENERATE = (1 << 2),
  /** Disallow rotating into ugly topology. */
  BM_EDGEROT_CHECK_BEAUTY = (1 << 3),
};

/**
 * \brief Rip a single face from a vertex fan
 */
BMVert *BM_face_loop_separate(BMesh *bm, BMLoop *l_sep);
BMVert *BM_face_loop_separate_multi_isolated(BMesh *bm, BMLoop *l_sep);
BMVert *BM_face_loop_separate_multi(BMesh *bm, BMLoop **larr, int larr_len);

#ifdef __cplusplus
}
#endif
