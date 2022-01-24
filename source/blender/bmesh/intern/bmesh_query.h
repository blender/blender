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
 */

#pragma once

/** \file
 * \ingroup bmesh
 */

/**
 * Returns true if the vertex is used in a given face.
 */
bool BM_vert_in_face(BMVert *v, BMFace *f) ATTR_WARN_UNUSED_RESULT ATTR_NONNULL();
/**
 * Compares the number of vertices in an array
 * that appear in a given face
 */
int BM_verts_in_face_count(BMVert **varr, int len, BMFace *f) ATTR_WARN_UNUSED_RESULT
    ATTR_NONNULL();
/**
 * Return true if all verts are in the face.
 */
bool BM_verts_in_face(BMVert **varr, int len, BMFace *f) ATTR_WARN_UNUSED_RESULT ATTR_NONNULL();

/**
 * Returns whether or not a given edge is part of a given face.
 */
bool BM_edge_in_face(const BMEdge *e, const BMFace *f) ATTR_WARN_UNUSED_RESULT ATTR_NONNULL();
BLI_INLINE bool BM_edge_in_loop(const BMEdge *e, const BMLoop *l) ATTR_WARN_UNUSED_RESULT
    ATTR_NONNULL();

BLI_INLINE bool BM_vert_in_edge(const BMEdge *e, const BMVert *v) ATTR_WARN_UNUSED_RESULT
    ATTR_NONNULL();
BLI_INLINE bool BM_verts_in_edge(const BMVert *v1,
                                 const BMVert *v2,
                                 const BMEdge *e) ATTR_WARN_UNUSED_RESULT ATTR_NONNULL();

/**
 * Returns edge length
 */
float BM_edge_calc_length(const BMEdge *e) ATTR_WARN_UNUSED_RESULT ATTR_NONNULL();
/**
 * Returns edge length squared (for comparisons)
 */
float BM_edge_calc_length_squared(const BMEdge *e) ATTR_WARN_UNUSED_RESULT ATTR_NONNULL();
/**
 * Utility function, since enough times we have an edge
 * and want to access 2 connected faces.
 *
 * \return true when only 2 faces are found.
 */
bool BM_edge_face_pair(BMEdge *e, BMFace **r_fa, BMFace **r_fb) ATTR_NONNULL();
/**
 * Utility function, since enough times we have an edge
 * and want to access 2 connected loops.
 *
 * \return true when only 2 faces are found.
 */
bool BM_edge_loop_pair(BMEdge *e, BMLoop **r_la, BMLoop **r_lb) ATTR_NONNULL();
BLI_INLINE BMVert *BM_edge_other_vert(BMEdge *e, const BMVert *v) ATTR_WARN_UNUSED_RESULT
    ATTR_NONNULL();
/**
 * Given a edge and a loop (assumes the edge is manifold). returns
 * the other faces loop, sharing the same vertex.
 *
 * <pre>
 * +-------------------+
 * |                   |
 * |                   |
 * |l_other <-- return |
 * +-------------------+ <-- A manifold edge between 2 faces
 * |l    e  <-- edge   |
 * |^ <-------- loop   |
 * |                   |
 * +-------------------+
 * </pre>
 */
BMLoop *BM_edge_other_loop(BMEdge *e, BMLoop *l) ATTR_WARN_UNUSED_RESULT ATTR_NONNULL();
/**
 * \brief Other Loop in Face Sharing an Edge
 *
 * Finds the other loop that shares \a v with \a e loop in \a f.
 * <pre>
 *     +----------+
 *     |          |
 *     |    f     |
 *     |          |
 *     +----------+ <-- return the face loop of this vertex.
 *     v --> e
 *     ^     ^ <------- These vert args define direction
 *                      in the face to check.
 *                      The faces loop direction is ignored.
 * </pre>
 *
 * \note caller must ensure \a e is used in \a f
 */
BMLoop *BM_face_other_edge_loop(BMFace *f, BMEdge *e, BMVert *v) ATTR_WARN_UNUSED_RESULT
    ATTR_NONNULL();
/**
 * See #BM_face_other_edge_loop This is the same functionality
 * to be used when the edges loop is already known.
 */
BMLoop *BM_loop_other_edge_loop(BMLoop *l, BMVert *v) ATTR_WARN_UNUSED_RESULT ATTR_NONNULL();
/**
 * \brief Other Loop in Face Sharing a Vertex
 *
 * Finds the other loop in a face.
 *
 * This function returns a loop in \a f that shares an edge with \a v
 * The direction is defined by \a v_prev, where the return value is
 * the loop of what would be 'v_next'
 * <pre>
 *     +----------+ <-- return the face loop of this vertex.
 *     |          |
 *     |    f     |
 *     |          |
 *     +----------+
 *     v_prev --> v
 *     ^^^^^^     ^ <-- These vert args define direction
 *                      in the face to check.
 *                      The faces loop direction is ignored.
 * </pre>
 *
 * \note \a v_prev and \a v _implicitly_ define an edge.
 */
BMLoop *BM_face_other_vert_loop(BMFace *f, BMVert *v_prev, BMVert *v) ATTR_WARN_UNUSED_RESULT
    ATTR_NONNULL();
/**
 * Return the other loop that uses this edge.
 *
 * In this case the loop defines the vertex,
 * the edge passed in defines the direction to step.
 *
 * <pre>
 *     +----------+ <-- Return the face-loop of this vertex.
 *     |          |
 *     |        e | <-- This edge defines the direction.
 *     |          |
 *     +----------+ <-- This loop defines the face and vertex..
 *                l
 * </pre>
 *
 */
BMLoop *BM_loop_other_vert_loop_by_edge(BMLoop *l, BMEdge *e) ATTR_WARN_UNUSED_RESULT
    ATTR_NONNULL();
/**
 * \brief Other Loop in Face Sharing a Vert
 *
 * Finds the other loop that shares \a v with \a e loop in \a f.
 * <pre>
 *     +----------+ <-- return the face loop of this vertex.
 *     |          |
 *     |          |
 *     |          |
 *     +----------+ <-- This vertex defines the direction.
 *           l    v
 *           ^ <------- This loop defines both the face to search
 *                      and the edge, in combination with 'v'
 *                      The faces loop direction is ignored.
 * </pre>
 */
BMLoop *BM_loop_other_vert_loop(BMLoop *l, BMVert *v) ATTR_WARN_UNUSED_RESULT ATTR_NONNULL();
/**
 * Utility function to step around a fan of loops,
 * using an edge to mark the previous side.
 *
 * \note all edges must be manifold,
 * once a non manifold edge is hit, return NULL.
 *
 * \code{.unparsed}
 *                ,.,-->|
 *            _,-'      |
 *          ,'          | (notice how 'e_step'
 *         /            |  and 'l' define the
 *        /             |  direction the arrow
 *       |     return   |  points).
 *       |     loop --> |
 * ---------------------+---------------------
 *         ^      l --> |
 *         |            |
 *  assign e_step       |
 *                      |
 *   begin e_step ----> |
 *                      |
 * \endcode
 */
BMLoop *BM_vert_step_fan_loop(BMLoop *l, BMEdge **e_step) ATTR_WARN_UNUSED_RESULT ATTR_NONNULL();

/**
 * Get the first loop of a vert. Uses the same initialization code for the first loop of the
 * iterator API
 */
BMLoop *BM_vert_find_first_loop(BMVert *v) ATTR_WARN_UNUSED_RESULT ATTR_NONNULL();
/**
 * A version of #BM_vert_find_first_loop that ignores hidden loops.
 */
BMLoop *BM_vert_find_first_loop_visible(BMVert *v) ATTR_WARN_UNUSED_RESULT ATTR_NONNULL();
/**
 * Only #BMEdge.l access us needed, however when we want the first visible loop,
 * a utility function is needed.
 */
BMLoop *BM_edge_find_first_loop_visible(BMEdge *e) ATTR_WARN_UNUSED_RESULT ATTR_NONNULL();

/**
 * Check if verts share a face.
 */
bool BM_vert_pair_share_face_check(BMVert *v_a, BMVert *v_b) ATTR_WARN_UNUSED_RESULT
    ATTR_NONNULL();
bool BM_vert_pair_share_face_check_cb(BMVert *v_a,
                                      BMVert *v_b,
                                      bool (*test_fn)(BMFace *f, void *user_data),
                                      void *user_data) ATTR_WARN_UNUSED_RESULT
    ATTR_NONNULL(1, 2, 3);
BMFace *BM_vert_pair_shared_face_cb(BMVert *v_a,
                                    BMVert *v_b,
                                    bool allow_adjacent,
                                    bool (*callback)(BMFace *, BMLoop *, BMLoop *, void *userdata),
                                    void *user_data,
                                    BMLoop **r_l_a,
                                    BMLoop **r_l_b) ATTR_NONNULL(1, 2, 4, 6, 7);
/**
 * Given 2 verts, find the smallest face they share and give back both loops.
 */
BMFace *BM_vert_pair_share_face_by_len(
    BMVert *v_a, BMVert *v_b, BMLoop **r_l_a, BMLoop **r_l_b, bool allow_adjacent) ATTR_NONNULL();
/**
 * Given 2 verts,
 * find a face they share that has the lowest angle across these verts and give back both loops.
 *
 * This can be better than #BM_vert_pair_share_face_by_len
 * because concave splits are ranked lowest.
 */
BMFace *BM_vert_pair_share_face_by_angle(
    BMVert *v_a, BMVert *v_b, BMLoop **r_l_a, BMLoop **r_l_b, bool allow_adjacent) ATTR_NONNULL();

BMFace *BM_edge_pair_share_face_by_len(
    BMEdge *e_a, BMEdge *e_b, BMLoop **r_l_a, BMLoop **r_l_b, bool allow_adjacent) ATTR_NONNULL();

int BM_vert_edge_count_nonwire(const BMVert *v) ATTR_WARN_UNUSED_RESULT ATTR_NONNULL();
#define BM_vert_edge_count_is_equal(v, n) (BM_vert_edge_count_at_most(v, (n) + 1) == n)
#define BM_vert_edge_count_is_over(v, n) (BM_vert_edge_count_at_most(v, (n) + 1) == (n) + 1)
int BM_vert_edge_count_at_most(const BMVert *v, int count_max) ATTR_WARN_UNUSED_RESULT
    ATTR_NONNULL();
/**
 * Returns the number of edges around this vertex.
 */
int BM_vert_edge_count(const BMVert *v) ATTR_WARN_UNUSED_RESULT ATTR_NONNULL();
#define BM_edge_face_count_is_equal(e, n) (BM_edge_face_count_at_most(e, (n) + 1) == n)
#define BM_edge_face_count_is_over(e, n) (BM_edge_face_count_at_most(e, (n) + 1) == (n) + 1)
int BM_edge_face_count_at_most(const BMEdge *e, int count_max) ATTR_WARN_UNUSED_RESULT
    ATTR_NONNULL();
/**
 * Returns the number of faces around this edge
 */
int BM_edge_face_count(const BMEdge *e) ATTR_WARN_UNUSED_RESULT ATTR_NONNULL();
#define BM_vert_face_count_is_equal(v, n) (BM_vert_face_count_at_most(v, (n) + 1) == n)
#define BM_vert_face_count_is_over(v, n) (BM_vert_face_count_at_most(v, (n) + 1) == (n) + 1)
int BM_vert_face_count_at_most(const BMVert *v, int count_max) ATTR_WARN_UNUSED_RESULT
    ATTR_NONNULL();
/**
 * Returns the number of faces around this vert
 * length matches #BM_LOOPS_OF_VERT iterator
 */
int BM_vert_face_count(const BMVert *v) ATTR_WARN_UNUSED_RESULT ATTR_NONNULL();
/**
 * The function takes a vertex at the center of a fan and returns the opposite edge in the fan.
 * All edges in the fan must be manifold, otherwise return NULL.
 *
 * \note This could (probably) be done more efficiently.
 */
BMEdge *BM_vert_other_disk_edge(BMVert *v, BMEdge *e) ATTR_WARN_UNUSED_RESULT ATTR_NONNULL();

/**
 * Fast alternative to `(BM_vert_edge_count(v) == 2)`.
 */
bool BM_vert_is_edge_pair(const BMVert *v) ATTR_WARN_UNUSED_RESULT ATTR_NONNULL();
/**
 * Fast alternative to `(BM_vert_edge_count(v) == 2)`
 * that checks both edges connect to the same faces.
 */
bool BM_vert_is_edge_pair_manifold(const BMVert *v) ATTR_WARN_UNUSED_RESULT ATTR_NONNULL();
/**
 * Access a verts 2 connected edges.
 *
 * \return true when only 2 verts are found.
 */
bool BM_vert_edge_pair(BMVert *v, BMEdge **r_e_a, BMEdge **r_e_b);
/**
 * Return true if the vertex is connected to _any_ faces.
 *
 * same as `BM_vert_face_count(v) != 0` or `BM_vert_find_first_loop(v) == NULL`.
 */
bool BM_vert_face_check(const BMVert *v) ATTR_WARN_UNUSED_RESULT ATTR_NONNULL();
/**
 * Tests whether or not the vertex is part of a wire edge.
 * (ie: has no faces attached to it)
 */
bool BM_vert_is_wire(const BMVert *v) ATTR_WARN_UNUSED_RESULT ATTR_NONNULL();
BLI_INLINE bool BM_edge_is_wire(const BMEdge *e) ATTR_WARN_UNUSED_RESULT ATTR_NONNULL();

/**
 * A vertex is non-manifold if it meets the following conditions:
 * 1: Loose - (has no edges/faces incident upon it).
 * 2: Joins two distinct regions - (two pyramids joined at the tip).
 * 3: Is part of an edge with more than 2 faces.
 * 4: Is part of a wire edge.
 */
bool BM_vert_is_manifold(const BMVert *v) ATTR_WARN_UNUSED_RESULT ATTR_NONNULL();
/**
 * A version of #BM_vert_is_manifold
 * which only checks if we're connected to multiple isolated regions.
 */
bool BM_vert_is_manifold_region(const BMVert *v) ATTR_WARN_UNUSED_RESULT ATTR_NONNULL();
BLI_INLINE bool BM_edge_is_manifold(const BMEdge *e) ATTR_WARN_UNUSED_RESULT ATTR_NONNULL();
bool BM_vert_is_boundary(const BMVert *v) ATTR_WARN_UNUSED_RESULT ATTR_NONNULL();
BLI_INLINE bool BM_edge_is_boundary(const BMEdge *e) ATTR_WARN_UNUSED_RESULT ATTR_NONNULL();
BLI_INLINE bool BM_edge_is_contiguous(const BMEdge *e) ATTR_WARN_UNUSED_RESULT ATTR_NONNULL();
/**
 * Check if the edge is convex or concave
 * (depends on face winding)
 */
bool BM_edge_is_convex(const BMEdge *e) ATTR_WARN_UNUSED_RESULT ATTR_NONNULL();
/**
 * \return true when loop customdata is contiguous.
 */
bool BM_edge_is_contiguous_loop_cd(const BMEdge *e,
                                   int cd_loop_type,
                                   int cd_loop_offset) ATTR_WARN_UNUSED_RESULT ATTR_NONNULL();

/**
 * The number of loops connected to this loop (not including disconnected regions).
 */
int BM_loop_region_loops_count_at_most(BMLoop *l, int *r_loop_total) ATTR_WARN_UNUSED_RESULT
    ATTR_NONNULL(1);
int BM_loop_region_loops_count(BMLoop *l) ATTR_WARN_UNUSED_RESULT ATTR_NONNULL(1);
/**
 * Check if the loop is convex or concave
 * (depends on face normal)
 */
bool BM_loop_is_convex(const BMLoop *l) ATTR_WARN_UNUSED_RESULT ATTR_NONNULL();
BLI_INLINE bool BM_loop_is_adjacent(const BMLoop *l_a, const BMLoop *l_b) ATTR_WARN_UNUSED_RESULT
    ATTR_NONNULL();
/**
 * Check if a point is inside the corner defined by a loop
 * (within the 2 planes defined by the loops corner & face normal).
 *
 * \return signed, squared distance to the loops planes, less than 0.0 when outside.
 */
float BM_loop_point_side_of_loop_test(const BMLoop *l, const float co[3]) ATTR_WARN_UNUSED_RESULT
    ATTR_NONNULL();
/**
 * Check if a point is inside the edge defined by a loop
 * (within the plane defined by the loops edge & face normal).
 *
 * \return signed, squared distance to the edge plane, less than 0.0 when outside.
 */
float BM_loop_point_side_of_edge_test(const BMLoop *l, const float co[3]) ATTR_WARN_UNUSED_RESULT
    ATTR_NONNULL();

/**
 * \return The previous loop, over \a eps_sq distance from \a l (or \a NULL if l_stop is reached).
 */
BMLoop *BM_loop_find_prev_nodouble(BMLoop *l, BMLoop *l_stop, float eps_sq);
/**
 * \return The next loop, over \a eps_sq distance from \a l (or \a NULL if l_stop is reached).
 */
BMLoop *BM_loop_find_next_nodouble(BMLoop *l, BMLoop *l_stop, float eps_sq);

/**
 * Calculates the angle between the previous and next loops
 * (angle at this loops face corner).
 *
 * \return angle in radians
 */
float BM_loop_calc_face_angle(const BMLoop *l) ATTR_WARN_UNUSED_RESULT ATTR_NONNULL();
/**
 * \brief BM_loop_calc_face_normal
 *
 * Calculate the normal at this loop corner or fallback to the face normal on straight lines.
 *
 * \param l: The loop to calculate the normal at
 * \param r_normal: Resulting normal
 * \return The length of the cross product (double the area).
 */
float BM_loop_calc_face_normal(const BMLoop *l, float r_normal[3]) ATTR_NONNULL();
/**
 * #BM_loop_calc_face_normal_safe_ex with predefined sane epsilon.
 *
 * Since this doesn't scale based on triangle size, fixed value works well.
 */
float BM_loop_calc_face_normal_safe(const BMLoop *l, float r_normal[3]) ATTR_NONNULL();
/**
 * \brief BM_loop_calc_face_normal
 *
 * Calculate the normal at this loop corner or fallback to the face normal on straight lines.
 *
 * \param l: The loop to calculate the normal at.
 * \param epsilon_sq: Value to avoid numeric errors (1e-5f works well).
 * \param r_normal: Resulting normal.
 */
float BM_loop_calc_face_normal_safe_ex(const BMLoop *l, float epsilon_sq, float r_normal[3])
    ATTR_NONNULL();
/**
 * A version of BM_loop_calc_face_normal_safe_ex which takes vertex coordinates.
 */
float BM_loop_calc_face_normal_safe_vcos_ex(const BMLoop *l,
                                            const float normal_fallback[3],
                                            float const (*vertexCos)[3],
                                            float epsilon_sq,
                                            float r_normal[3]) ATTR_NONNULL();
float BM_loop_calc_face_normal_safe_vcos(const BMLoop *l,
                                         const float normal_fallback[3],
                                         float const (*vertexCos)[3],
                                         float r_normal[3]) ATTR_NONNULL();

/**
 * \brief BM_loop_calc_face_direction
 *
 * Calculate the direction a loop is pointing.
 *
 * \param l: The loop to calculate the direction at
 * \param r_dir: Resulting direction
 */
void BM_loop_calc_face_direction(const BMLoop *l, float r_dir[3]);
/**
 * \brief BM_loop_calc_face_tangent
 *
 * Calculate the tangent at this loop corner or fallback to the face normal on straight lines.
 * This vector always points inward into the face.
 *
 * \param l: The loop to calculate the tangent at
 * \param r_tangent: Resulting tangent
 */
void BM_loop_calc_face_tangent(const BMLoop *l, float r_tangent[3]);

/**
 * \brief BMESH EDGE/FACE ANGLE
 *
 * Calculates the angle between two faces.
 * Assumes the face normals are correct.
 *
 * \return angle in radians
 */
float BM_edge_calc_face_angle_ex(const BMEdge *e, float fallback) ATTR_WARN_UNUSED_RESULT
    ATTR_NONNULL();
float BM_edge_calc_face_angle(const BMEdge *e) ATTR_WARN_UNUSED_RESULT ATTR_NONNULL();
/**
 * \brief BMESH EDGE/FACE ANGLE
 *
 * Calculates the angle between two faces.
 * Assumes the face normals are correct.
 *
 * \return angle in radians
 */
float BM_edge_calc_face_angle_signed_ex(const BMEdge *e, float fallback) ATTR_WARN_UNUSED_RESULT
    ATTR_NONNULL();
/**
 * \brief BMESH EDGE/FACE ANGLE
 *
 * Calculates the angle between two faces in world space.
 * Assumes the face normals are correct.
 *
 * \return angle in radians
 */
float BM_edge_calc_face_angle_with_imat3_ex(const BMEdge *e,
                                            const float imat3[3][3],
                                            float fallback) ATTR_WARN_UNUSED_RESULT ATTR_NONNULL();
float BM_edge_calc_face_angle_with_imat3(const BMEdge *e,
                                         const float imat3[3][3]) ATTR_WARN_UNUSED_RESULT
    ATTR_NONNULL();
float BM_edge_calc_face_angle_signed(const BMEdge *e) ATTR_WARN_UNUSED_RESULT ATTR_NONNULL();
/**
 * \brief BMESH EDGE/FACE TANGENT
 *
 * Calculate the tangent at this loop corner or fallback to the face normal on straight lines.
 * This vector always points inward into the face.
 *
 * \brief BM_edge_calc_face_tangent
 * \param e:
 * \param e_loop: The loop to calculate the tangent at,
 * used to get the face and winding direction.
 * \param r_tangent: The loop corner tangent to set
 */
void BM_edge_calc_face_tangent(const BMEdge *e, const BMLoop *e_loop, float r_tangent[3])
    ATTR_NONNULL();
float BM_vert_calc_edge_angle(const BMVert *v) ATTR_WARN_UNUSED_RESULT ATTR_NONNULL();
/**
 * \brief BMESH VERT/EDGE ANGLE
 *
 * Calculates the angle a verts 2 edges.
 *
 * \returns the angle in radians
 */
float BM_vert_calc_edge_angle_ex(const BMVert *v, float fallback) ATTR_WARN_UNUSED_RESULT
    ATTR_NONNULL();
/**
 * \note this isn't optimal to run on an array of verts,
 * see 'solidify_add_thickness' for a function which runs on an array.
 */
float BM_vert_calc_shell_factor(const BMVert *v) ATTR_WARN_UNUSED_RESULT ATTR_NONNULL();
/* alternate version of #BM_vert_calc_shell_factor which only
 * uses 'hflag' faces, but falls back to all if none found. */
float BM_vert_calc_shell_factor_ex(const BMVert *v,
                                   const float no[3],
                                   char hflag) ATTR_WARN_UNUSED_RESULT ATTR_NONNULL();
/**
 * \note quite an obscure function.
 * used in bmesh operators that have a relative scale options,
 */
float BM_vert_calc_median_tagged_edge_length(const BMVert *v) ATTR_WARN_UNUSED_RESULT
    ATTR_NONNULL();

/**
 * Returns the loop of the shortest edge in f.
 */
BMLoop *BM_face_find_shortest_loop(BMFace *f) ATTR_WARN_UNUSED_RESULT ATTR_NONNULL();
/**
 * Returns the loop of the longest edge in f.
 */
BMLoop *BM_face_find_longest_loop(BMFace *f) ATTR_WARN_UNUSED_RESULT ATTR_NONNULL();

BMEdge *BM_edge_exists(BMVert *v_a, BMVert *v_b) ATTR_WARN_UNUSED_RESULT ATTR_NONNULL();
/**
 * Returns an edge sharing the same vertices as this one.
 * This isn't an invalid state but tools should clean up these cases before
 * returning the mesh to the user.
 */
BMEdge *BM_edge_find_double(BMEdge *e) ATTR_WARN_UNUSED_RESULT ATTR_NONNULL();

/**
 * Given a set of vertices (varr), find out if
 * there is a face with exactly those vertices
 * (and only those vertices).
 *
 * \note there used to be a BM_face_exists_overlap function that checks for partial overlap.
 */
BMFace *BM_face_exists(BMVert **varr, int len) ATTR_NONNULL(1);
/**
 * Check if the face has an exact duplicate (both winding directions).
 */
BMFace *BM_face_find_double(BMFace *f) ATTR_WARN_UNUSED_RESULT ATTR_NONNULL();

/**
 * Given a set of vertices and edges (\a varr, \a earr), find out if
 * all those vertices are filled in by existing faces that _only_ use those vertices.
 *
 * This is for use in cases where creating a face is possible but would result in
 * many overlapping faces.
 *
 * An example of how this is used: when 2 tri's are selected that share an edge,
 * pressing F-key would make a new overlapping quad (without a check like this)
 *
 * \a earr and \a varr can be in any order, however they _must_ form a closed loop.
 */
bool BM_face_exists_multi(BMVert **varr, BMEdge **earr, int len) ATTR_WARN_UNUSED_RESULT
    ATTR_NONNULL();
/* same as 'BM_face_exists_multi' but built vert array from edges */
bool BM_face_exists_multi_edge(BMEdge **earr, int len) ATTR_WARN_UNUSED_RESULT ATTR_NONNULL();

/**
 * Given a set of vertices (varr), find out if
 * all those vertices overlap an existing face.
 *
 * \note The face may contain other verts \b not in \a varr.
 *
 * \note Its possible there are more than one overlapping faces,
 * in this case the first one found will be returned.
 *
 * \param varr: Array of unordered verts.
 * \param len: \a varr array length.
 * \return The face or NULL.
 */
BMFace *BM_face_exists_overlap(BMVert **varr, int len) ATTR_WARN_UNUSED_RESULT;
/**
 * Given a set of vertices (varr), find out if
 * there is a face that uses vertices only from this list
 * (that the face is a subset or made from the vertices given).
 *
 * \param varr: Array of unordered verts.
 * \param len: varr array length.
 */
bool BM_face_exists_overlap_subset(BMVert **varr, int len) ATTR_WARN_UNUSED_RESULT ATTR_NONNULL();

/**
 * Returns the number of faces that are adjacent to both f1 and f2,
 * \note Could be sped up a bit by not using iterators and by tagging
 * faces on either side, then count the tags rather then searching.
 */
int BM_face_share_face_count(BMFace *f_a, BMFace *f_b) ATTR_WARN_UNUSED_RESULT ATTR_NONNULL();
/**
 * Counts the number of edges two faces share (if any)
 */
int BM_face_share_edge_count(BMFace *f_a, BMFace *f_b) ATTR_WARN_UNUSED_RESULT ATTR_NONNULL();
/**
 * Counts the number of verts two faces share (if any).
 */
int BM_face_share_vert_count(BMFace *f_a, BMFace *f_b) ATTR_WARN_UNUSED_RESULT ATTR_NONNULL();

/**
 * same as #BM_face_share_face_count but returns a bool
 */
bool BM_face_share_face_check(BMFace *f_a, BMFace *f_b) ATTR_WARN_UNUSED_RESULT ATTR_NONNULL();
/**
 * Returns true if the faces share an edge
 */
bool BM_face_share_edge_check(BMFace *f_a, BMFace *f_b) ATTR_WARN_UNUSED_RESULT ATTR_NONNULL();
/**
 * Returns true if the faces share a vert.
 */
bool BM_face_share_vert_check(BMFace *f_a, BMFace *f_b) ATTR_WARN_UNUSED_RESULT ATTR_NONNULL();

/**
 * Returns true when 2 loops share an edge (are adjacent in the face-fan)
 */
bool BM_loop_share_edge_check(BMLoop *l_a, BMLoop *l_b) ATTR_WARN_UNUSED_RESULT ATTR_NONNULL();

/**
 * Test if e1 shares any faces with e2
 */
bool BM_edge_share_face_check(BMEdge *e1, BMEdge *e2) ATTR_WARN_UNUSED_RESULT ATTR_NONNULL();
/**
 * Test if e1 shares any quad faces with e2
 */
bool BM_edge_share_quad_check(BMEdge *e1, BMEdge *e2) ATTR_WARN_UNUSED_RESULT ATTR_NONNULL();
/**
 * Tests to see if e1 shares a vertex with e2
 */
bool BM_edge_share_vert_check(BMEdge *e1, BMEdge *e2) ATTR_WARN_UNUSED_RESULT ATTR_NONNULL();

/**
 * Return the shared vertex between the two edges or NULL
 */
BMVert *BM_edge_share_vert(BMEdge *e1, BMEdge *e2) ATTR_WARN_UNUSED_RESULT ATTR_NONNULL();
/**
 * \brief Return the Loop Shared by Edge and Vert
 *
 * Finds the loop used which uses \a  in face loop \a l
 *
 * \note this function takes a loop rather than an edge
 * so we can select the face that the loop should be from.
 */
BMLoop *BM_edge_vert_share_loop(BMLoop *l, BMVert *v) ATTR_WARN_UNUSED_RESULT ATTR_NONNULL();
/**
 * \brief Return the Loop Shared by Face and Vertex
 *
 * Finds the loop used which uses \a v in face loop \a l
 *
 * \note currently this just uses simple loop in future may be sped up
 * using radial vars
 */
BMLoop *BM_face_vert_share_loop(BMFace *f, BMVert *v) ATTR_WARN_UNUSED_RESULT ATTR_NONNULL();
/**
 * \brief Return the Loop Shared by Face and Edge
 *
 * Finds the loop used which uses \a e in face loop \a l
 *
 * \note currently this just uses simple loop in future may be sped up
 * using radial vars
 */
BMLoop *BM_face_edge_share_loop(BMFace *f, BMEdge *e) ATTR_WARN_UNUSED_RESULT ATTR_NONNULL();

void BM_edge_ordered_verts(const BMEdge *edge, BMVert **r_v1, BMVert **r_v2) ATTR_NONNULL();
/**
 * Returns the verts of an edge as used in a face
 * if used in a face at all, otherwise just assign as used in the edge.
 *
 * Useful to get a deterministic winding order when calling
 * BM_face_create_ngon() on an arbitrary array of verts,
 * though be sure to pick an edge which has a face.
 *
 * \note This is in fact quite a simple check,
 * mainly include this function so the intent is more obvious.
 * We know these 2 verts will _always_ make up the loops edge
 */
void BM_edge_ordered_verts_ex(const BMEdge *edge,
                              BMVert **r_v1,
                              BMVert **r_v2,
                              const BMLoop *edge_loop) ATTR_NONNULL();

bool BM_vert_is_all_edge_flag_test(const BMVert *v,
                                   char hflag,
                                   bool respect_hide) ATTR_WARN_UNUSED_RESULT ATTR_NONNULL();
bool BM_vert_is_all_face_flag_test(const BMVert *v,
                                   char hflag,
                                   bool respect_hide) ATTR_WARN_UNUSED_RESULT ATTR_NONNULL();
bool BM_edge_is_all_face_flag_test(const BMEdge *e,
                                   char hflag,
                                   bool respect_hide) ATTR_WARN_UNUSED_RESULT ATTR_NONNULL();

/* convenience functions for checking flags */
bool BM_edge_is_any_vert_flag_test(const BMEdge *e, char hflag) ATTR_WARN_UNUSED_RESULT
    ATTR_NONNULL();
bool BM_edge_is_any_face_flag_test(const BMEdge *e, char hflag) ATTR_WARN_UNUSED_RESULT
    ATTR_NONNULL();
bool BM_face_is_any_vert_flag_test(const BMFace *f, char hflag) ATTR_WARN_UNUSED_RESULT
    ATTR_NONNULL();
bool BM_face_is_any_edge_flag_test(const BMFace *f, char hflag) ATTR_WARN_UNUSED_RESULT
    ATTR_NONNULL();

bool BM_edge_is_any_face_len_test(const BMEdge *e, int len) ATTR_WARN_UNUSED_RESULT ATTR_NONNULL();

/**
 * Use within assert's to check normals are valid.
 */
bool BM_face_is_normal_valid(const BMFace *f) ATTR_WARN_UNUSED_RESULT ATTR_NONNULL();

double BM_mesh_calc_volume(BMesh *bm, bool is_signed) ATTR_WARN_UNUSED_RESULT ATTR_NONNULL();

/**
 * Calculate isolated groups of faces with optional filtering.
 *
 * \param bm: the BMesh.
 * \param r_groups_array: Array of ints to fill in, length of bm->totface
 *        (or when hflag_test is set, the number of flagged faces).
 * \param r_group_index: index, length pairs into \a r_groups_array, size of return value
 *        int pairs: (array_start, array_length).
 * \param filter_fn: Filter the edge-loops or vert-loops we step over (depends on \a htype_step).
 * \param user_data: Optional user data for \a filter_fn, can be NULL.
 * \param hflag_test: Optional flag to test faces,
 *        use to exclude faces from the calculation, 0 for all faces.
 * \param htype_step: BM_VERT to walk over face-verts, BM_EDGE to walk over faces edges
 *        (having both set is supported too).
 * \return The number of groups found.
 */
int BM_mesh_calc_face_groups(BMesh *bm,
                             int *r_groups_array,
                             int (**r_group_index)[2],
                             BMLoopFilterFunc filter_fn,
                             BMLoopPairFilterFunc filter_pair_fn,
                             void *user_data,
                             char hflag_test,
                             char htype_step) ATTR_WARN_UNUSED_RESULT ATTR_NONNULL(1, 2, 3);
/**
 * Calculate isolated groups of edges with optional filtering.
 *
 * \param bm: the BMesh.
 * \param r_groups_array: Array of ints to fill in, length of `bm->totedge`
 *        (or when hflag_test is set, the number of flagged edges).
 * \param r_group_index: index, length pairs into \a r_groups_array, size of return value
 *        int pairs: (array_start, array_length).
 * \param filter_fn: Filter the edges or verts we step over (depends on \a htype_step)
 *        as to which types we deal with.
 * \param user_data: Optional user data for \a filter_fn, can be NULL.
 * \param hflag_test: Optional flag to test edges,
 *        use to exclude edges from the calculation, 0 for all edges.
 * \return The number of groups found.
 *
 * \note Unlike #BM_mesh_calc_face_groups there is no 'htype_step' argument,
 *       since we always walk over verts.
 */
int BM_mesh_calc_edge_groups(BMesh *bm,
                             int *r_groups_array,
                             int (**r_group_index)[2],
                             BMVertFilterFunc filter_fn,
                             void *user_data,
                             char hflag_test) ATTR_WARN_UNUSED_RESULT ATTR_NONNULL(1, 2, 3);

/**
 * This is an alternative to #BM_mesh_calc_edge_groups.
 *
 * While we could call this, then create vertex & face arrays,
 * it requires looping over geometry connectivity twice,
 * this slows down edit-mesh separate by loose parts, see: T70864.
 */
int BM_mesh_calc_edge_groups_as_arrays(BMesh *bm,
                                       BMVert **verts,
                                       BMEdge **edges,
                                       BMFace **faces,
                                       int (**r_groups)[3]) ATTR_WARN_UNUSED_RESULT
    ATTR_NONNULL(1, 2, 3, 4, 5);

/* Not really any good place to put this. */
float bmesh_subd_falloff_calc(int falloff, float val) ATTR_WARN_UNUSED_RESULT;

#include "bmesh_query_inline.h"
