/*
 * ***** BEGIN GPL LICENSE BLOCK *****
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
 * Contributor(s): Geoffrey Bantle, Levi Schooley.
 *
 * ***** END GPL LICENSE BLOCK *****
 */

#ifndef __BMESH_H__
#define __BMESH_H__

/** \file blender/bmesh/bmesh.h
 *  \ingroup bmesh
 *
 * \addtogroup bmesh BMesh
 *
 * \brief BMesh is a non-manifold boundary representation designed to replace the current, limited EditMesh structure,
 * solving many of the design limitations and maintenance issues of EditMesh.
 *
 *
 * \section bm_structure The Structure
 *
 * BMesh stores topology in four main element structures:
 *
 * - Faces - BMFace
 * - Loops - BMLoop, (stores per-face-vertex data, UV's, vertex-colors, etc)
 * - Edges - BMEdge
 * - Verts - BMVert
 *
 *
 * \subsection bm_header_flags Header Flags
 * Each element (vertex/edge/face/loop) in a mesh has an associated bit-field called "header flags".
 *
 * BMHeader flags should <b>never</b> be read or written to by bmesh operators (see Operators below).
 *
 * Access to header flags is done with BM_elem_flag_*() functions.
 *
 *
 * \subsection bm_faces Faces
 *
 * Faces in BMesh are stored as a circular linked list of loops. Loops store per-face-vertex data
 * (amongst other things outlined later in this document), and define the face boundary.
 *
 *
 * \subsection bm_loop The Loop
 *
 * Loops define the boundary loop of a face. Each loop logically corresponds to an edge,
 * which is defined by the loop and next loop's vertices.
 *
 * Loops store several handy pointers:
 *
 * - BMLoop#v - pointer to the vertex associated with this loop.
 * - BMLoop#e - pointer to the edge associated with this loop.
 * - BMLoop#f - pointer to the face associated with this loop.
 *
 *
 * \subsection bm_two_side_face 2-Sided Faces
 *
 * There are some situations where you need 2-sided faces (e.g. a face of two vertices).
 * This is supported by BMesh, but note that such faces should only be used as intermediary steps,
 * and should not end up in the final mesh.
 *
 *
 * \subsection bm_edges_and_verts Edges and Vertices
 *
 * Edges and Vertices in BMesh are much like their counterparts in EditMesh,
 * except for some members private to the BMesh api.
 *
 * \note There can be more then one edge between two vertices in bmesh,
 * though the rest of blender (e.g. DerivedMesh, CDDM, CCGSubSurf, etc) does not support this.
 *
 *
 * \subsection bm_queries Queries
 *
 * The following topological queries are available:
 *
 * - Edges/Faces/Loops around a vertex.
 * - Faces around an edge.
 * - Loops around an edge.
 *
 * These are accessible through the iterator api, which is covered later in this document
 *
 * See source/blender/bmesh/bmesh_queries.h for more misc. queries.
 *
 *
 * \section bm_api The BMesh API
 *
 * One of the goals of the BMesh API is to make it easy and natural to produce highly maintainable code.
 * Code duplication, etc are avoided where possible.
 *
 *
 * \subsection bm_iter_api Iterator API
 *
 * Most topological queries in BMesh go through an iterator API (see Queries above).
 * These are defined in bmesh_iterators.h.  If you can, please use the #BM_ITER macro in bmesh_iterators.h
 *
 *
 * \subsection bm_walker_api Walker API
 *
 * Topological queries that require a stack (e.g. recursive queries) go through the Walker API,
 * which is defined in bmesh_walkers.h. Currently the "walkers" are hard-coded into the API,
 * though a mechanism for plugging in new walkers needs to be added at some point.
 *
 * Most topological queries should go through these two APIs;
 * there are additional functions you can use for topological iteration, but their meant for internal bmesh code.
 *
 * Note that the walker API supports delimiter flags, to allow the caller to flag elements not to walk past.
 *
 *
 * \subsection bm_ops Operators
 *
 * Operators are an integral part of BMesh. Unlike regular blender operators,
 * BMesh operators <b>bmo's</b> are designed to be nested (e.g. call other operators).
 *
 * Each operator has a number of input/output "slots" which are used to pass settings & data into/out of the operator
 * (and allows for chaining operators together).
 *
 * These slots are identified by name, using strings.
 *
 * Access to slots is done with BMO_slot_*() functions.
 *
 *
 * \subsection bm_tool_flags Tool Flags
 *
 * The BMesh API provides a set of flags for faces, edges and vertices, which are private to an operator.
 * These flags may be used by the client operator code as needed
 * (a common example is flagging elements for use in another operator).
 * Each call to an operator allocates it's own set of tool flags when it's executed,
 * avoiding flag conflicts between operators.
 *
 * These flags should not be confused with header flags, which are used to store persistent flags
 * (e.g. selection, hide status, etc).
 *
 * Access to tool flags is done with BMO_elem_flag_*() functions.
 *
 * \warning Operators are never allowed to read or write to header flags.
 * They act entirely on the data inside their input slots.
 * For example an operator should not check the selected state of an element,
 * there are some exceptions to this - some operators check of a face is smooth.
 *
 *
 * \subsection bm_slot_types Slot Types
 *
 * The following slot types are available:
 *
 * - integer - #BMO_OP_SLOT_INT
 * - boolean - #BMO_OP_SLOT_BOOL
 * - float   - #BMO_OP_SLOT_FLT
 * - pointer - #BMO_OP_SLOT_PNT
 * - element buffer - #BMO_OP_SLOT_ELEMENT_BUF - a list of verts/edges/faces
 * - map     - BMO_OP_SLOT_MAPPING - simple hash map
 *
 *
 * \subsection bm_slot_iter Slot Iterators
 *
 * Access to element buffers or maps must go through the slot iterator api, defined in bmesh_operators.h.
 * Use #BMO_ITER where ever possible.
 *
 *
 * \subsection bm_elem_buf Element Buffers
 *
 * The element buffer slot type is used to feed elements (verts/edges/faces) to operators.
 * Internally they are stored as pointer arrays (which happily has not caused any problems so far).
 * Many operators take in a buffer of elements, process it,
 * then spit out a new one; this allows operators to be chained together.
 *
 * \note Element buffers may have elements of different types within the same buffer (this is supported by the API.
 */

#ifdef __cplusplus
extern "C" {
#endif

#include "DNA_listBase.h"
#include "DNA_customdata_types.h"

#include "BLI_utildefines.h"

#include "bmesh_class.h"

/*forward declarations*/
struct Mesh;

/*
 * BMHeader
 *
 * All mesh elements begin with a BMHeader. This structure
 * hold several types of data
 *
 * 1: The type of the element (vert, edge, loop or face)
 * 2: Persistant "header" flags/markings (sharp, seam, select, hidden, ect)
      note that this is different from the "tool" flags.
 * 3: Unique ID in the bmesh.
 * 4: some elements for internal record keeping.
 *
*/

/* BMHeader->htype (char) */
enum {
	BM_VERT = 1,
	BM_EDGE = 2,
	BM_LOOP = 4,
	BM_FACE = 8
};

#define BM_ALL (BM_VERT | BM_EDGE | BM_LOOP | BM_FACE)

/* BMHeader->hflag (char) */
enum {
	BM_ELEM_SELECT  = (1 << 0),
	BM_ELEM_HIDDEN  = (1 << 1),
	BM_ELEM_SEAM    = (1 << 2),
	BM_ELEM_SMOOTH  = (1 << 3), /* used for faces and edges, note from the user POV,
                                  * this is a sharp edge when disabled */

	BM_ELEM_TAG     = (1 << 4), /* internal flag, used for ensuring correct normals
                                 * during multires interpolation, and any other time
                                 * when temp tagging is handy.
                                 * always assume dirty & clear before use. */

	/* we have 2 spare flags which is awesome but since we're limited to 8
	 * only add new flags with care! - campbell */
	/* BM_ELEM_SPARE  = (1 << 5), */
	/* BM_ELEM_SPARE  = (1 << 6), */

	BM_ELEM_INTERNAL_TAG = (1 << 7) /* for low level internal API tagging,
                                     * since tools may want to tag verts and
                                     * not have functions clobber them */
};

/* Mesh Level Ops */
extern int bm_mesh_allocsize_default[4];

/* ob is needed by multires */
BMesh *BM_mesh_create(struct Object *ob, const int allocsize[4]);
BMesh *BM_mesh_copy(BMesh *bmold);
void   BM_mesh_free(BMesh *bm);

/* frees mesh, but not actual BMesh struct */
void BM_mesh_data_free(BMesh *bm);
void BM_mesh_normals_update(BMesh *bm, const short skip_hidden);

/* Construction */
BMVert *BM_vert_create(BMesh *bm, const float co[3], const BMVert *example);
BMEdge *BM_edge_create(BMesh *bm, BMVert *v1, BMVert *v2, const BMEdge *example, int nodouble);
BMFace *BM_face_create(BMesh *bm, BMVert **verts, BMEdge **edges, const int len, int nodouble);

BMFace *BM_face_create_quad_tri_v(BMesh *bm,
                                  BMVert **verts, int len,
                                  const BMFace *example, const int nodouble);

/* easier to use version of BM_face_create_quad_tri_v.
 * creates edges if necassary. */
BMFace *BM_face_create_quad_tri(BMesh *bm, BMVert *v1, BMVert *v2, BMVert *v3, BMVert *v4,
                                const BMFace *example, const int nodouble);

/* makes an ngon from an unordered list of edges.  v1 and v2 must be the verts
 * defining edges[0], and define the winding of the new face. */
BMFace *BM_face_create_ngon(BMesh *bm, BMVert *v1, BMVert *v2, BMEdge **edges, int len, int nodouble);

/* stuff for dealing with header flags */
#define BM_elem_flag_test(   ele, hflag)      _bm_elem_flag_test    (&(ele)->head, hflag)
#define BM_elem_flag_enable( ele, hflag)      _bm_elem_flag_enable  (&(ele)->head, hflag)
#define BM_elem_flag_disable(ele, hflag)      _bm_elem_flag_disable (&(ele)->head, hflag)
#define BM_elem_flag_set(    ele, hflag, val) _bm_elem_flag_set     (&(ele)->head, hflag, val)
#define BM_elem_flag_toggle( ele, hflag)      _bm_elem_flag_toggle  (&(ele)->head, hflag)
#define BM_elem_flag_merge(  ele_a, ele_b)    _bm_elem_flag_merge   (&(ele_a)->head, &(ele_b)->head)

BM_INLINE char _bm_elem_flag_test(const BMHeader *head, const char hflag);
BM_INLINE void _bm_elem_flag_enable(BMHeader *head, const char hflag);
BM_INLINE void _bm_elem_flag_disable(BMHeader *head, const char hflag);
BM_INLINE void _bm_elem_flag_set(BMHeader *head, const char hflag, const int val);
BM_INLINE void _bm_elem_flag_toggle(BMHeader *head, const char hflag);
BM_INLINE void _bm_elem_flag_merge(BMHeader *head_a, BMHeader *head_b);

/* notes on BM_elem_index_set(...) usage,
 * Set index is sometimes abused as temp storage, other times we cant be
 * sure if the index values are valid because certain operations have modified
 * the mesh structure.
 *
 * To set the elements to valid indicies 'BM_mesh_elem_index_ensure' should be used
 * rather then adding inline loops, however there are cases where we still
 * set the index directly
 *
 * In an attempt to manage this, here are 3 tags Im adding to uses of
 * 'BM_elem_index_set'
 *
 * - 'set_inline'  -- since the data is already being looped over set to a
 *                    valid value inline.
 *
 * - 'set_dirty!'  -- intentionally sets the index to an invalid value,
 *                    flagging 'bm->elem_index_dirty' so we dont use it.
 *
 * - 'set_ok'      -- this is valid use since the part of the code is low level.
 *
 * - 'set_ok_invalid'  -- set to -1 on purpose since this should not be
 *                    used without a full array re-index, do this on
 *                    adding new vert/edge/faces since they may be added at
 *                    the end of the array.
 *
 * - 'set_loop'    -- currently loop index values are not used used much so
 *                    assume each case they are dirty.
 * - campbell */

#define BM_elem_index_get(ele)           _bm_elem_index_get(&(ele)->head)
#define BM_elem_index_set(ele, index)    _bm_elem_index_set(&(ele)->head, index)
BM_INLINE int  _bm_elem_index_get(const BMHeader *ele);
BM_INLINE void _bm_elem_index_set(BMHeader *ele, const int index);

/* todo */
BMFace *BM_face_copy(BMesh *bm, BMFace *f, const short copyverts, const short copyedges);

/* copies loop data from adjacent faces */
void BM_face_copy_shared(BMesh *bm, BMFace *f);

/* copies attributes, e.g. customdata, header flags, etc, from one element
 * to another of the same type.*/
void BM_elem_attrs_copy(BMesh *source_mesh, BMesh *target_mesh, const void *source, void *target);

/* Modification */
/* join two adjacent faces together along an edge.  note that
 * the faces must only be joined by on edge.  e is the edge you
 * wish to dissolve.*/
BMFace *BM_faces_join_pair(BMesh *bm, BMFace *f1, BMFace *f2, BMEdge *e);

/* generic, flexible join faces function; note that most everything uses
 * this, including BM_faces_join_pair */
BMFace *BM_faces_join(BMesh *bm, BMFace **faces, int totface);

/* split a face along two vertices.  returns the newly made face, and sets
 * the nl member to a loop in the newly created edge.*/
BMFace *BM_face_split(BMesh *bm, BMFace *f,
                      BMVert *v1, BMVert *v2,
                      BMLoop **r_l, BMEdge *example);

/* these 2 functions are very similar */
BMEdge* BM_vert_collapse_faces(BMesh *bm, BMEdge *ke, BMVert *kv, float fac,
                               const short join_faces, const short kill_degenerate_faces);
BMEdge* BM_vert_collapse_edge(BMesh *bm, BMEdge *ke, BMVert *kv,
                              const short kill_degenerate_faces);


/* splits an edge.  ne is set to the new edge created. */
BMVert *BM_edge_split(BMesh *bm, BMEdge *e, BMVert *v, BMEdge **r_e, float percent);

/* split an edge multiple times evenly */
BMVert  *BM_edge_split_n(BMesh *bm, BMEdge *e, int numcuts);

/* connect two verts together, through a face they share.  this function may
 * be removed in the future. */
BMEdge *BM_verts_connect(BMesh *bm, BMVert *v1, BMVert *v2, BMFace **r_f);

/* rotates an edge topologically, either clockwise (if ccw=0) or counterclockwise
 * (if ccw is 1). */
BMEdge *BM_edge_rotate(BMesh *bm, BMEdge *e, int ccw);

/* Rip a single face from a vertex fan */
BMVert *BM_vert_rip(BMesh *bm, BMFace *sf, BMVert *sv);

/*updates a face normal*/
void BM_face_normal_update(BMesh *bm, BMFace *f);
void BM_face_normal_update_vcos(BMesh *bm, BMFace *f, float no[3], float (*vertexCos)[3]);

/*updates face and vertex normals incident on an edge*/
void BM_edge_normals_update(BMesh *bm, BMEdge *e);

/*update a vert normal (but not the faces incident on it)*/
void BM_vert_normal_update(BMesh *bm, BMVert *v);
void BM_vert_normal_update_all(BMesh *bm, BMVert *v);

void BM_face_normal_flip(BMesh *bm, BMFace *f);

/*dissolves all faces around a vert, and removes it.*/
int BM_disk_dissolve(BMesh *bm, BMVert *v);

/* dissolves vert, in more situations then BM_disk_dissolve
 * (e.g. if the vert is part of a wire edge, etc).*/
int BM_vert_dissolve(BMesh *bm, BMVert *v);

/* Projects co onto face f, and returns true if it is inside
 * the face bounds.  Note that this uses a best-axis projection
 * test, instead of projecting co directly into f's orientation
 * space, so there might be accuracy issues.*/
int BM_face_point_inside_test(BMesh *bm, BMFace *f, const float co[3]);

/* Interpolation */

/* projects target onto source for customdata interpolation.  note: only
 * does loop customdata.  multires is handled.  */
void BM_face_interp_from_face(BMesh *bm, BMFace *target, BMFace *source);

/* projects a single loop, target, onto source for customdata interpolation. multires is handled.
 * if do_vertex is true, target's vert data will also get interpolated.*/
void BM_loop_interp_from_face(BMesh *bm, BMLoop *target, BMFace *source,
                              int do_vertex, int do_multires);

/* smoothes boundaries between multires grids, including some borders in adjacent faces */
void BM_face_multires_bounds_smooth(BMesh *bm, BMFace *f);

/* project the multires grid in target onto source's set of multires grids */
void BM_loop_interp_multires(BMesh *bm, BMLoop *target, BMFace *source);
void BM_vert_interp_from_face(BMesh *bm, BMVert *v, BMFace *source);

void  BM_data_interp_from_verts(BMesh *bm, BMVert *v1, BMVert *v2, BMVert *v, const float fac);
void  BM_data_interp_face_vert_edge(BMesh *bm, BMVert *v1, BMVert *v2, BMVert *v, BMEdge *e1, const float fac);
void  BM_data_layer_add(BMesh *em, CustomData *data, int type);
void  BM_data_layer_add_named(BMesh *bm, CustomData *data, int type, const char *name);
void  BM_data_layer_free(BMesh *em, CustomData *data, int type);
void  BM_data_layer_free_n(BMesh *bm, CustomData *data, int type, int n);
float BM_elem_float_data_get(CustomData *cd, void *element, int type);
void  BM_elem_float_data_set(CustomData *cd, void *element, int type, const float val);

/* get the area of the face */
float BM_face_area_calc(BMesh *bm, BMFace *f);
/* computes the centroid of a face, using the center of the bounding box */
void BM_face_center_bounds_calc(BMesh *bm, BMFace *f, float center[3]);
/* computes the centroid of a face, using the mean average */
void BM_face_center_mean_calc(BMesh *bm, BMFace *f, float center[3]);

void BM_mesh_select_mode_flush(BMesh *bm);

/* mode independent flushing up/down */
void BM_mesh_deselect_flush(BMesh *bm);
void BM_mesh_select_flush(BMesh *bm);

/* flag conversion funcs */
char BM_face_flag_from_mflag(const char  mflag);
char BM_edge_flag_from_mflag(const short mflag);
char BM_vert_flag_from_mflag(const char  mflag);
/* reverse */
char  BM_face_flag_to_mflag(BMFace *f);
short BM_edge_flag_to_mflag(BMEdge *e);
char  BM_vert_flag_to_mflag(BMVert *v);


/* convert MLoop*** in a bmface to mtface and mcol in
 * an MFace*/
void BM_loops_to_corners(BMesh *bm, struct Mesh *me, int findex,
                         BMFace *f, int numTex, int numCol);

void BM_loop_kill(BMesh *bm, BMLoop *l);
void BM_face_kill(BMesh *bm, BMFace *f);
void BM_edge_kill(BMesh *bm, BMEdge *e);
void BM_vert_kill(BMesh *bm, BMVert *v);

/* kills all edges associated with f, along with any other faces containing
 * those edges*/
void BM_face_edges_kill(BMesh *bm, BMFace *f);

/* kills all verts associated with f, along with any other faces containing
 * those vertices*/
void BM_face_verts_kill(BMesh *bm, BMFace *f);

/*clear all data in bm*/
void BM_mesh_clear(BMesh *bm);

void BM_mesh_elem_index_ensure(BMesh *bm, const char hflag);

void BM_mesh_elem_index_validate(BMesh *bm, const char *location, const char *func,
                                 const char *msg_a, const char *msg_b);

BMVert *BM_vert_at_index(BMesh *bm, const int index);
BMEdge *BM_edge_at_index(BMesh *bm, const int index);
BMFace *BM_face_at_index(BMesh *bm, const int index);

/*start/stop edit*/
void bmesh_edit_begin(BMesh *bm, int flag);
void bmesh_edit_end(BMesh *bm, int flag);


#ifdef USE_BMESH_HOLES
#  define BM_FACE_FIRST_LOOP(p) (((BMLoopList *)((p)->loops.first))->first)
#else
#  define BM_FACE_FIRST_LOOP(p) ((p)->l_first)
#endif

/* size to use for static arrays when dealing with NGons,
 * alloc after this limit is reached.
 * this value is rather arbitrary */
#define BM_NGON_STACK_SIZE 32

/* avoid inf loop, this value is arbtrary
 * but should not error on valid cases */
#define BM_LOOP_RADIAL_MAX 10000
#define BM_NGON_MAX 100000

/* include the rest of the API */
#include "bmesh_marking.h"
#include "bmesh_operator_api.h"
#include "bmesh_operators.h"
#include "bmesh_error.h"
#include "bmesh_queries.h"
#include "bmesh_iterators.h"
#include "bmesh_walkers.h"
#include "intern/bmesh_inline.c"
#include "intern/bmesh_operator_api_inline.c"

#ifdef __cplusplus
}
#endif

#endif /* __BMESH_H__ */
