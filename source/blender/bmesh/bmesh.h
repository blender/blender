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
 *
 *
 * \section bm_fname Function Naming Conventions
 *
 * These conventions should be used throughout the bmesh module.
 *
 * - BM_xxx() -     High level BMesh API function for use anywhere.
 * - bmesh_xxx() -  Low level API function.
 * - bm_xxx() -     'static' functions, not apart of the API at all, but use prefix since they operate on BMesh data.
 * - BMO_xxx() -    High level operator API function for use anywhere.
 * - bmo_xxx() -    Low level / internal operator API functions.
 * - _bm_xxx() -    Functions which are called via macros only.
 *
 */

#ifdef __cplusplus
extern "C" {
#endif

#include "DNA_listBase.h"
#include "DNA_customdata_types.h"

#include "BLI_utildefines.h"

#include "bmesh_class.h"

/*forward declarations*/

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


/* ------------------------------------------------------------------------- */
/* bmesh_inline.c */

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
#include "bmesh_operator_api.h"
#include "bmesh_error.h"

#include "intern/bmesh_construct.h"
#include "intern/bmesh_core.h"
#include "intern/bmesh_interp.h"
#include "intern/bmesh_iterators.h"
#include "intern/bmesh_marking.h"
#include "intern/bmesh_mesh.h"
#include "intern/bmesh_mods.h"
#include "intern/bmesh_operators.h"
#include "intern/bmesh_polygon.h"
#include "intern/bmesh_queries.h"
#include "intern/bmesh_walkers.h"
#include "intern/bmesh_walkers.h"

#include "intern/bmesh_inline.c"
#include "intern/bmesh_operator_api_inline.c"

#ifdef __cplusplus
}
#endif

#endif /* __BMESH_H__ */
