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

BMFace *BM_face_copy(BMesh *bm_dst, BMesh *bm_src, BMFace *f, bool copy_verts, bool copy_edges);

typedef enum eBMCreateFlag {
  BM_CREATE_NOP = 0,
  /** Faces and edges only. */
  BM_CREATE_NO_DOUBLE = (1 << 1),
  /**
   * Skip custom-data - for all element types data,
   * use if we immediately write custom-data into the element so this skips copying from 'example'
   * arguments or setting defaults, speeds up conversion when data is converted all at once.
   */
  BM_CREATE_SKIP_CD = (1 << 2),
} eBMCreateFlag;

/**
 * \brief Main function for creating a new vertex.
 */
BMVert *BM_vert_create(BMesh *bm,
                       const float co[3],
                       const BMVert *v_example,
                       eBMCreateFlag create_flag);
/**
 * \brief Main function for creating a new edge.
 *
 * \note Duplicate edges are supported by the API however users should _never_ see them.
 * so unless you need a unique edge or know the edge won't exist,
 * you should call with \a no_double = true.
 */
BMEdge *BM_edge_create(
    BMesh *bm, BMVert *v1, BMVert *v2, const BMEdge *e_example, eBMCreateFlag create_flag);
/**
 * Main face creation function
 *
 * \param bm: The mesh
 * \param verts: A sorted array of verts size of len
 * \param edges: A sorted array of edges size of len
 * \param len: Length of the face
 * \param create_flag: Options for creating the face
 */
BMFace *BM_face_create(BMesh *bm,
                       BMVert **verts,
                       BMEdge **edges,
                       int len,
                       const BMFace *f_example,
                       eBMCreateFlag create_flag);
/**
 * Wrapper for #BM_face_create when you don't have an edge array
 */
BMFace *BM_face_create_verts(BMesh *bm,
                             BMVert **vert_arr,
                             int len,
                             const BMFace *f_example,
                             eBMCreateFlag create_flag,
                             bool create_edges);

/**
 * Kills all edges associated with \a f, along with any other faces containing those edges.
 */
void BM_face_edges_kill(BMesh *bm, BMFace *f);
/**
 * kills all verts associated with \a f, along with any other faces containing
 * those vertices
 */
void BM_face_verts_kill(BMesh *bm, BMFace *f);

/**
 * A version of #BM_face_kill which removes edges and verts
 * which have no remaining connected geometry.
 */
void BM_face_kill_loose(BMesh *bm, BMFace *f);

/**
 * Kills \a f and its loops.
 */
void BM_face_kill(BMesh *bm, BMFace *f);
/**
 * Kills \a e and all faces that use it.
 */
void BM_edge_kill(BMesh *bm, BMEdge *e);
/**
 * Kills \a v and all edges that use it.
 */
void BM_vert_kill(BMesh *bm, BMVert *v);

/**
 * \brief Splice Edge
 *
 * Splice two unique edges which share the same two vertices into one edge.
 *  (\a e_src into \a e_dst, removing e_src).
 *
 * \return Success
 *
 * \note Edges must already have the same vertices.
 */
bool BM_edge_splice(BMesh *bm, BMEdge *e_dst, BMEdge *e_src);
/**
 * \brief Splice Vert
 *
 * Merges two verts into one
 * (\a v_src into \a v_dst, removing \a v_src).
 *
 * \return Success
 *
 * \warning This doesn't work for collapsing edges,
 * where \a v and \a vtarget are connected by an edge
 * (assert checks for this case).
 */
bool BM_vert_splice(BMesh *bm, BMVert *v_dst, BMVert *v_src);
/**
 * Check if splicing vertices would create any double edges.
 *
 * \note assume caller will handle case where verts share an edge.
 */
bool BM_vert_splice_check_double(BMVert *v_a, BMVert *v_b);

/**
 * \brief Loop Reverse
 *
 * Changes the winding order of a face from CW to CCW or vice versa.
 *
 * \param cd_loop_mdisp_offset: Cached result of `CustomData_get_offset(&bm->ldata, CD_MDISPS)`.
 * \param use_loop_mdisp_flip: When set, flip the Z-depth of the mdisp,
 * (use when flipping normals, disable when mirroring, eg: symmetrize).
 */
void bmesh_kernel_loop_reverse(BMesh *bm,
                               BMFace *f,
                               int cd_loop_mdisp_offset,
                               bool use_loop_mdisp_flip);

/**
 * Avoid calling this where possible,
 * low level function so both face pointers remain intact but point to swapped data.
 * \note must be from the same bmesh.
 */
void bmesh_face_swap_data(BMFace *f_a, BMFace *f_b);

/**
 * \brief Join Connected Faces
 *
 * Joins a collected group of faces into one. Only restriction on
 * the input data is that the faces must be connected to each other.
 *
 * \return The newly created combine BMFace.
 *
 * \note If a pair of faces share multiple edges,
 * the pair of faces will be joined at every edge.
 *
 * \note this is a generic, flexible join faces function,
 * almost everything uses this, including #BM_faces_join_pair
 */
BMFace *BM_faces_join(BMesh *bm, BMFace **faces, int totface, bool do_del);
/**
 * High level function which wraps both #bmesh_kernel_vert_separate and #bmesh_kernel_edge_separate
 */
void BM_vert_separate(BMesh *bm,
                      BMVert *v,
                      BMEdge **e_in,
                      int e_in_len,
                      bool copy_select,
                      BMVert ***r_vout,
                      int *r_vout_len);
/**
 * A version of #BM_vert_separate which takes a flag.
 */
void BM_vert_separate_hflag(
    BMesh *bm, BMVert *v, char hflag, bool copy_select, BMVert ***r_vout, int *r_vout_len);
void BM_vert_separate_tested_edges(
    BMesh *bm, BMVert *v_dst, BMVert *v_src, bool (*testfn)(BMEdge *, void *arg), void *arg);

/**
 * BMesh Kernel: For modifying structure.
 *
 * Names are on the verbose side but these are only for low-level access.
 */
/**
 * \brief Separate Vert
 *
 * Separates all disjoint fans that meet at a vertex, making a unique
 * vertex for each region. returns an array of all resulting vertices.
 *
 * \note this is a low level function, bm_edge_separate needs to run on edges first
 * or, the faces sharing verts must not be sharing edges for them to split at least.
 *
 * \return Success
 */
void bmesh_kernel_vert_separate(
    BMesh *bm, BMVert *v, BMVert ***r_vout, int *r_vout_len, bool copy_select);
/**
 * \brief Separate Edge
 *
 * Separates a single edge into two edge: the original edge and
 * a new edge that has only \a l_sep in its radial.
 *
 * \return Success
 *
 * \note Does nothing if \a l_sep is already the only loop in the
 * edge radial.
 */
void bmesh_kernel_edge_separate(BMesh *bm, BMEdge *e, BMLoop *l_sep, bool copy_select);

/**
 * \brief Split Face Make Edge (SFME)
 *
 * \warning this is a low level function, most likely you want to use #BM_face_split()
 *
 * Takes as input two vertices in a single face.
 * An edge is created which divides the original face into two distinct regions.
 * One of the regions is assigned to the original face and it is closed off.
 * The second region has a new face assigned to it.
 *
 * \par Examples:
 * <pre>
 *     Before:               After:
 *      +--------+           +--------+
 *      |        |           |        |
 *      |        |           |   f1   |
 *     v1   f1   v2          v1======v2
 *      |        |           |   f2   |
 *      |        |           |        |
 *      +--------+           +--------+
 * </pre>
 *
 * \note the input vertices can be part of the same edge. This will
 * result in a two edged face. This is desirable for advanced construction
 * tools and particularly essential for edge bevel. Because of this it is
 * up to the caller to decide what to do with the extra edge.
 *
 * \note If \a holes is NULL, then both faces will lose
 * all holes from the original face.  Also, you cannot split between
 * a hole vert and a boundary vert; that case is handled by higher-
 * level wrapping functions (when holes are fully implemented, anyway).
 *
 * \note that holes represents which holes goes to the new face, and of
 * course this requires removing them from the existing face first, since
 * you cannot have linked list links inside multiple lists.
 *
 * \return A BMFace pointer
 */
BMFace *bmesh_kernel_split_face_make_edge(BMesh *bm,
                                          BMFace *f,
                                          BMLoop *l_v1,
                                          BMLoop *l_v2,
                                          BMLoop **r_l,
#ifdef USE_BMESH_HOLES
                                          ListBase *holes,
#endif
                                          BMEdge *example,
                                          bool no_double);

/**
 * \brief Split Edge Make Vert (SEMV)
 *
 * Takes \a e edge and splits it into two, creating a new vert.
 * \a tv should be one end of \a e : the newly created edge
 * will be attached to that end and is returned in \a r_e.
 *
 * \par Examples:
 *
 * <pre>
 *                     E
 *     Before: OV-------------TV
 *                 E       RE
 *     After:  OV------NV-----TV
 * </pre>
 *
 * \return The newly created BMVert pointer.
 */
BMVert *bmesh_kernel_split_edge_make_vert(BMesh *bm, BMVert *tv, BMEdge *e, BMEdge **r_e);
/**
 * \brief Join Edge Kill Vert (JEKV)
 *
 * Takes an edge \a e_kill and pointer to one of its vertices \a v_kill
 * and collapses the edge on that vertex.
 *
 * \par Examples:
 *
 * <pre>
 *     Before:    e_old  e_kill
 *              +-------+-------+
 *              |       |       |
 *              v_old   v_kill  v_target
 *
 *     After:           e_old
 *              +---------------+
 *              |               |
 *              v_old           v_target
 * </pre>
 *
 * \par Restrictions:
 * KV is a vertex that must have a valance of exactly two. Furthermore
 * both edges in KV's disk cycle (OE and KE) must be unique (no double edges).
 *
 * \return The resulting edge, NULL for failure.
 *
 * \note This euler has the possibility of creating
 * faces with just 2 edges. It is up to the caller to decide what to do with
 * these faces.
 */
BMEdge *bmesh_kernel_join_edge_kill_vert(BMesh *bm,
                                         BMEdge *e_kill,
                                         BMVert *v_kill,
                                         bool do_del,
                                         bool check_edge_exists,
                                         bool kill_degenerate_faces,
                                         bool kill_duplicate_faces);
/**
 * \brief Join Vert Kill Edge (JVKE)
 *
 * Collapse an edge, merging surrounding data.
 *
 * Unlike #BM_vert_collapse_edge & #bmesh_kernel_join_edge_kill_vert
 * which only handle 2 valence verts,
 * this can handle any number of connected edges/faces.
 *
 * <pre>
 * Before: -> After:
 * +-+-+-+    +-+-+-+
 * | | | |    | \ / |
 * +-+-+-+    +--+--+
 * | | | |    | / \ |
 * +-+-+-+    +-+-+-+
 * </pre>
 */
BMVert *bmesh_kernel_join_vert_kill_edge(BMesh *bm,
                                         BMEdge *e_kill,
                                         BMVert *v_kill,
                                         bool do_del,
                                         bool check_edge_exists,
                                         bool kill_degenerate_faces);
/**
 * \brief Join Face Kill Edge (JFKE)
 *
 * Takes two faces joined by a single 2-manifold edge and fuses them together.
 * The edge shared by the faces must not be connected to any other edges which have
 * Both faces in its radial cycle
 *
 * \par Examples:
 * <pre>
 *           A                   B
 *      +--------+           +--------+
 *      |        |           |        |
 *      |   f1   |           |   f1   |
 *     v1========v2 = Ok!    v1==V2==v3 == Wrong!
 *      |   f2   |           |   f2   |
 *      |        |           |        |
 *      +--------+           +--------+
 * </pre>
 *
 * In the example A, faces \a f1 and \a f2 are joined by a single edge,
 * and the euler can safely be used.
 * In example B however, \a f1 and \a f2 are joined by multiple edges and will produce an error.
 * The caller in this case should call #bmesh_kernel_join_edge_kill_vert on the extra edges
 * before attempting to fuse \a f1 and \a f2.
 *
 * \note The order of arguments decides whether or not certain per-face attributes are present
 * in the resultant face. For instance vertex winding, material index, smooth flags,
 * etc are inherited from \a f1, not \a f2.
 *
 * \return A BMFace pointer
 */
BMFace *bmesh_kernel_join_face_kill_edge(BMesh *bm, BMFace *f1, BMFace *f2, BMEdge *e);

/**
 * \brief Un-glue Region Make Vert (URMV)
 *
 * Disconnects a face from its vertex fan at loop \a l_sep
 *
 * \return The newly created BMVert
 *
 * \note Will be a no-op and return original vertex if only two edges at that vertex.
 */
BMVert *bmesh_kernel_unglue_region_make_vert(BMesh *bm, BMLoop *l_sep);
/**
 * A version of #bmesh_kernel_unglue_region_make_vert that disconnects multiple loops at once.
 * The loops must all share the same vertex, can be in any order
 * and are all moved to use a single new vertex - which is returned.
 *
 * This function handles the details of finding fans boundaries.
 */
BMVert *bmesh_kernel_unglue_region_make_vert_multi(BMesh *bm, BMLoop **larr, int larr_len);
/**
 * This function assumes l_sep is a part of a larger fan which has already been
 * isolated by calling #bmesh_kernel_edge_separate to segregate it radially.
 */
BMVert *bmesh_kernel_unglue_region_make_vert_multi_isolated(BMesh *bm, BMLoop *l_sep);
