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
 */

#ifdef __cplusplus
extern "C" {
#endif

#include "DNA_listBase.h"
#include "DNA_customdata_types.h"

#include "BLI_utildefines.h"

#include "bmesh_class.h"

/*
 * short introduction:
 *
 * the bmesh structure is a boundary representation, supporting non-manifold
 * locally modifiable topology. the API is designed to allow clean, maintainable
 * code, that never (or almost never) directly inspects the underlying structure.
 *
 * The API includes iterators, including many useful topological iterators;
 * walkers, which walk over a mesh, without the risk of hitting the recursion
 * limit; operators, which are logical, reusable mesh modules; topological
 * modification functions (like split face, join faces, etc), which are used for
 * topological manipulations; and some (not yet finished) geometric utility
 * functions.
 *
 * some definitions:
 *
 * tool flags: private flags for tools.  each operator has it's own private
 *             tool flag "layer", which it can use to flag elements.
 *             tool flags are also used by various other parts of the api.
 * header flags: stores persistent flags, such as selection state, hide state,
 *               etc.  be careful of touching these.
 */

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
#define BM_VERT 	1
#define BM_EDGE 	2
#define BM_LOOP 	4
#define BM_FACE 	8
#define BM_ALL		(BM_VERT | BM_EDGE | BM_LOOP | BM_FACE)

/* BMHeader->hflag (char) */
#define BM_ELEM_SELECT	(1 << 0)
#define BM_ELEM_HIDDEN	(1 << 1)
#define BM_ELEM_SEAM	(1 << 2)
#define BM_ELEM_SMOOTH	(1 << 3) /* used for faces and edges, note from the user POV,
                                  * this is a sharp edge when disabled */

#define BM_ELEM_TAG     (1 << 4) /* internal flag, used for ensuring correct normals
                                  * during multires interpolation, and any other time
                                  * when temp tagging is handy.
                                  * always assume dirty & clear before use. */

/* we have 3 spare flags which is awesome but since we're limited to 8
 * only add new flags with care! - campbell */
/* #define BM_ELEM_SPARE	 (1 << 5) */
/* #define BM_ELEM_SPARE	 (1 << 6) */

#define BM_ELEM_INTERNAL_TAG (1 << 7) /* for low level internal API tagging,
                                       * since tools may want to tag verts and
                                       * not have functions clobber them */

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

BM_INLINE char _bm_elem_flag_test(const BMHeader *element, const char hflag);
BM_INLINE void _bm_elem_flag_enable(BMHeader *element, const char hflag);
BM_INLINE void _bm_elem_flag_disable(BMHeader *element, const char hflag);
BM_INLINE void _bm_elem_flag_set(BMHeader *ele, const char hflag, const int val);
BM_INLINE void _bm_elem_flag_toggle(BMHeader *ele, const char hflag);
BM_INLINE void _bm_elem_flag_merge(BMHeader *ele_a, BMHeader *ele_b);

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
                      BMLoop **nl, BMEdge *example);

/* these 2 functions are very similar */
BMEdge* BM_vert_collapse_faces(BMesh *bm, BMEdge *ke, BMVert *kv, float fac,
                               const short join_faces, const short kill_degenerate_faces);
BMEdge* BM_vert_collapse_edge(BMesh *bm, BMEdge *ke, BMVert *kv,
                              const short kill_degenerate_faces);


/* splits an edge.  ne is set to the new edge created. */
BMVert *BM_edge_split(BMesh *bm, BMEdge *e, BMVert *v, BMEdge **ne, float percent);

/* split an edge multiple times evenly */
BMVert  *BM_edge_split_n(BMesh *bm, BMEdge *e, int numcuts);

/* connect two verts together, through a face they share.  this function may
 * be removed in the future. */
BMEdge *BM_verts_connect(BMesh *bm, BMVert *v1, BMVert *v2, BMFace **nf);

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
void bmesh_begin_edit(BMesh *bm, int flag);
void bmesh_end_edit(BMesh *bm, int flag);


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
