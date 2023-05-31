/* SPDX-License-Identifier: GPL-2.0-or-later */

#pragma once

/** \file
 * \ingroup bmesh
 *
 * #BMesh data structures, used for mesh editing operations
 * that benefit from accessing connectivity information.
 */

#include "BLI_assert.h"

#ifdef __cplusplus
extern "C" {
#endif

/* disable holes for now,
 * these are ifdef'd because they use more memory and can't be saved in DNA currently */
// #define USE_BMESH_HOLES

struct BMEdge;
struct BMFace;
struct BMLoop;
struct BMVert;
struct BMesh;

struct MLoopNorSpaceArray;

struct BLI_mempool;

/* NOTE: it is very important for BMHeader to start with two
 * pointers. this is a requirement of mempool's method of
 * iteration.
 *
 * hrm. it doesn't but still works ok, remove the comment above? - campbell.
 */

// #pragma GCC diagnostic error "-Wpadded"

/**
 * #BMHeader
 *
 * All mesh elements begin with a #BMHeader. This structure
 * hold several types of data
 *
 * 1: The type of the element (vert, edge, loop or face)
 * 2: Persistent "header" flags/markings (smooth, seam, select, hidden, etc)
 *    note that this is different from the "tool" flags.
 * 3: Unique ID in the #BMesh.
 * 4: some elements for internal record keeping.
 */
typedef struct BMHeader {
  /** Customdata layers. */
  void *data;

  /**
   * \note
   * - Use BM_elem_index_get/set macros for index
   * - Uninitialized to -1 so we can easily tell its not set.
   * - Used for edge/vert/face/loop, check BMesh.elem_index_dirty for valid index values,
   *   this is abused by various tools which set it dirty.
   * - For loops this is used for sorting during tessellation.
   */
  int index;

  /** Element geometric type (verts/edges/loops/faces). */
  char htype;
  /** This would be a CD layer, see below. */
  char hflag;

  /**
   * Internal use only!
   * \note We are very picky about not bloating this struct
   * but in this case its padded up to 16 bytes anyway,
   * so adding a flag here gives no increase in size.
   */
  char api_flag;
  // char _pad;
} BMHeader;

BLI_STATIC_ASSERT((sizeof(BMHeader) <= 16), "BMHeader size has grown!");

/* NOTE: need some way to specify custom locations for custom data layers.  so we can
 * make them point directly into structs.  and some way to make it only happen to the
 * active layer, and properly update when switching active layers. */

typedef struct BMVert {
  BMHeader head;

  float co[3]; /* vertex coordinates */
  float no[3]; /* vertex normal */

  /**
   * Pointer to (any) edge using this vertex (for disk cycles).
   *
   * \note Some higher level functions set this to different edges that use this vertex,
   * which is a bit of an abuse of internal #BMesh data but also works OK for now
   * (use with care!).
   */
  struct BMEdge *e;
} BMVert;

typedef struct BMVert_OFlag {
  BMVert base;
  struct BMFlagLayer *oflags;
} BMVert_OFlag;

/* disk link structure, only used by edges */
typedef struct BMDiskLink {
  struct BMEdge *next, *prev;
} BMDiskLink;

typedef struct BMEdge {
  BMHeader head;

  /**
   * Vertices (unordered),
   *
   * Although the order can be used at times,
   * when extruding a face from a wire-edge for example.
   *
   * Operations that create/subdivide edges shouldn't flip the order
   * unless there is a good reason to do so.
   */
  BMVert *v1, *v2;

  /**
   * The list of loops around the edge, see doc-string for #BMLoop.radial_next
   * for an example of using this to loop over all faces used by an edge.
   */
  struct BMLoop *l;

  /**
   * Disk Cycle Pointers
   *
   * relative data: d1 indicates the next/prev
   * edge around vertex v1 and d2 does the same for v2.
   */
  BMDiskLink v1_disk_link, v2_disk_link;
} BMEdge;

typedef struct BMEdge_OFlag {
  BMEdge base;
  struct BMFlagLayer *oflags;
} BMEdge_OFlag;

typedef struct BMLoop {
  BMHeader head;
  /* notice no flags layer */

  /**
   * The vertex this loop points to.
   *
   * - This vertex must be unique within the cycle.
   */
  struct BMVert *v;

  /**
   * The edge this loop uses.
   *
   * Vertices (#BMLoop.v & #BMLoop.next.v) always contain vertices from (#BMEdge.v1 & #BMEdge.v2).
   * Although no assumptions can be made about the order,
   * as this isn't meaningful for mesh topology.
   *
   * - This edge must be unique within the cycle (defined by #BMLoop.next & #BMLoop.prev links).
   */
  struct BMEdge *e;
  /**
   * The face this loop is part of.
   *
   * - This face must be shared by all within the cycle.
   *   Used as a back-pointer so loops can know the face they define.
   */
  struct BMFace *f;

  /**
   * Other loops connected to this edge.
   *
   * This is typically use for accessing an edges faces,
   * however this is done by stepping over it's loops.
   *
   * - This is a circular list, so there are no first/last storage of the "radial" data.
   *   Instead #BMEdge.l points to any one of the loops that use it.
   *
   * - Since the list is circular, the particular loop referenced doesn't matter,
   *   as all other loops can be accessed from it.
   *
   * - Every loop in this radial list has the same value for #BMLoop.e.
   *
   * - The value for #BMLoop.v might not match the radial next/previous
   *   as this depends on the face-winding.
   *   You can be sure #BMLoop.v will either #BMEdge.v1 or #BMEdge.v2 of #BMLoop.e,
   *
   * - Unlike face-winding (which defines if the direction the face points),
   *   next and previous are insignificant. The list could be reversed for example,
   *   without any impact on the topology.
   *
   * This is an example of looping over an edges faces using #BMLoop.radial_next.
   *
   * \code{.c}
   * BMLoop *l_iter = edge->l;
   * do {
   *   operate_on_face(l_iter->f);
   * } while ((l_iter = l_iter->radial_next) != edge->l);
   * \endcode
   */
  struct BMLoop *radial_next, *radial_prev;

  /**
   * Other loops that are part of this face.
   *
   * This is typically used for accessing all vertices/edges in a faces.
   *
   * - This is a circular list, so there are no first/last storage of the "cycle" data.
   *   Instead #BMFace.l_first points to any one of the loops that are part of this face.
   *
   * - Since the list is circular, the particular loop referenced doesn't matter,
   *   as all other loops can be accessed from it.
   *
   * - Every loop in this "cycle" list has the same value for #BMLoop.f.
   *
   * - The direction of this list defines the face winding.
   *   Reversing the list flips the face.
   *
   * This is an example loop over all vertices and edges of a face.
   *
   * \code{.c}
   * BMLoop *l_first, *l_iter;
   * l_iter = l_first = BM_FACE_FIRST_LOOP(f);
   * do {
   *   operate_on_vert(l_iter->v);
   *   operate_on_edge(l_iter->e);
   * } while ((l_iter = l_iter->next) != l_first);
   * \endcode
   */
  struct BMLoop *next, *prev;
} BMLoop;

/* can cast BMFace/BMEdge/BMVert, but NOT BMLoop, since these don't have a flag layer */
typedef struct BMElemF {
  BMHeader head;
} BMElemF;

/* can cast anything to this, including BMLoop */
typedef struct BMElem {
  BMHeader head;
} BMElem;

#ifdef USE_BMESH_HOLES
/* eventually, this structure will be used for supporting holes in faces */
typedef struct BMLoopList {
  struct BMLoopList *next, *prev;
  struct BMLoop *first, *last;
} BMLoopList;
#endif

typedef struct BMFace {
  BMHeader head;

#ifdef USE_BMESH_HOLES
  int totbounds; /* Total boundaries, is one plus the number of holes in the face. */
  ListBase loops;
#else
  BMLoop *l_first;
#endif
  /**
   * Number of vertices in the face
   * (the length of #BMFace.l_first circular linked list).
   */
  int len;
  /**
   * Face normal, see #BM_face_calc_normal.
   */
  float no[3];
  /**
   * Material index, typically >= 0 and < #Mesh.totcol although this isn't enforced
   * Python for e.g. can set this to any positive value since scripts may create
   * mesh data first and setup material slots later.
   *
   * When using to index into a material array it's range should be checked first,
   * values exceeding the range should be ignored or treated as zero
   * (if a material slot needs to be used - when drawing for e.g.)
   */
  short mat_nr;
  //  short _pad[3];
} BMFace;

typedef struct BMFace_OFlag {
  BMFace base;
  struct BMFlagLayer *oflags;
} BMFace_OFlag;

typedef struct BMFlagLayer {
  short f; /* flags */
} BMFlagLayer;

// #pragma GCC diagnostic ignored "-Wpadded"

typedef struct BMesh {
  int totvert, totedge, totloop, totface;
  int totvertsel, totedgesel, totfacesel;

  /**
   * Flag index arrays as being dirty so we can check if they are clean and
   * avoid looping over the entire vert/edge/face/loop array in those cases.
   * valid flags are: `(BM_VERT | BM_EDGE | BM_FACE | BM_LOOP)`
   */
  char elem_index_dirty;

  /**
   * Flag array table as being dirty so we know when its safe to use it,
   * or when it needs to be re-created.
   */
  char elem_table_dirty;

  /* element pools */
  struct BLI_mempool *vpool, *epool, *lpool, *fpool;

  /* mempool lookup tables (optional)
   * index tables, to map indices to elements via
   * BM_mesh_elem_table_ensure and associated functions.  don't
   * touch this or read it directly.\
   * Use BM_mesh_elem_table_ensure(), BM_vert/edge/face_at_index() */
  BMVert **vtable;
  BMEdge **etable;
  BMFace **ftable;

  /* size of allocated tables */
  int vtable_tot;
  int etable_tot;
  int ftable_tot;

  /* operator api stuff (must be all NULL or all alloc'd) */
  struct BLI_mempool *vtoolflagpool, *etoolflagpool, *ftoolflagpool;

  uint use_toolflags : 1;

  int toolflag_index;

  CustomData vdata, edata, ldata, pdata;

#ifdef USE_BMESH_HOLES
  struct BLI_mempool *looplistpool;
#endif

  struct MLoopNorSpaceArray *lnor_spacearr;
  char spacearr_dirty;

  /* Should be copy of scene select mode. */
  /* Stored in #BMEditMesh too, this is a bit confusing,
   * make sure they're in sync!
   * Only use when the edit mesh can't be accessed - campbell */
  short selectmode;

  /* ID of the shape key this bmesh came from */
  int shapenr;

  int totflags;
  ListBase selected;

  /**
   * The active face.
   * This is kept even when unselected, mainly so UV editing can keep showing the
   * active faces image while the selection is being modified in the 3D viewport.
   *
   * Without this the active image in the UV editor would flicker in a distracting way
   * while changing selection in the 3D viewport.
   */
  BMFace *act_face;

  /** List of #BMOpError, used for operator error handling. */
  ListBase errorstack;

  /**
   * Keep a single reference to the Python instance of this #BMesh (if any exists).
   *
   * This allows save invalidation of a #BMesh when it's freed,
   * so the Python object will report it as having been removed,
   * instead of crashing on invalid memory access.
   *
   * Doesn't hold a #PyObject reference, cleared when the last object is de-referenced.
   */
  void *py_handle;
} BMesh;

/** #BMHeader.htype (char) */
enum {
  BM_VERT = 1,
  BM_EDGE = 2,
  BM_LOOP = 4,
  BM_FACE = 8,
};

typedef struct BMLoopNorEditData {
  int loop_index;
  BMLoop *loop;
  float niloc[3];
  float nloc[3];
  float *loc;
  short *clnors_data;
} BMLoopNorEditData;

typedef struct BMLoopNorEditDataArray {
  BMLoopNorEditData *lnor_editdata;
  /**
   * This one has full amount of loops,
   * used to map loop index to actual #BMLoopNorEditData struct.
   */
  BMLoopNorEditData **lidx_to_lnor_editdata;

  int cd_custom_normal_offset;
  int totloop;
} BMLoopNorEditDataArray;

#define BM_ALL (BM_VERT | BM_EDGE | BM_LOOP | BM_FACE)
#define BM_ALL_NOLOOP (BM_VERT | BM_EDGE | BM_FACE)

/** #BMesh.spacearr_dirty */
enum {
  BM_SPACEARR_DIRTY = 1 << 0,
  BM_SPACEARR_DIRTY_ALL = 1 << 1,
  BM_SPACEARR_BMO_SET = 1 << 2,
};

/* args for _Generic */
#define _BM_GENERIC_TYPE_ELEM_NONCONST \
  void *, BMVert *, BMEdge *, BMLoop *, BMFace *, BMVert_OFlag *, BMEdge_OFlag *, BMFace_OFlag *, \
      BMElem *, BMElemF *, BMHeader *

#define _BM_GENERIC_TYPE_ELEM_CONST \
  const void *, const BMVert *, const BMEdge *, const BMLoop *, const BMFace *, \
      const BMVert_OFlag *, const BMEdge_OFlag *, const BMFace_OFlag *, const BMElem *, \
      const BMElemF *, const BMHeader *

#define BM_CHECK_TYPE_ELEM_CONST(ele) CHECK_TYPE_ANY(ele, _BM_GENERIC_TYPES_CONST)

#define BM_CHECK_TYPE_ELEM_NONCONST(ele) CHECK_TYPE_ANY(ele, _BM_GENERIC_TYPE_ELEM_NONCONST)

#define BM_CHECK_TYPE_ELEM(ele) \
  CHECK_TYPE_ANY(ele, _BM_GENERIC_TYPE_ELEM_NONCONST, _BM_GENERIC_TYPE_ELEM_CONST)

/* vert */
#define _BM_GENERIC_TYPE_VERT_NONCONST BMVert *, BMVert_OFlag *
#define _BM_GENERIC_TYPE_VERT_CONST const BMVert *, const BMVert_OFlag *
#define BM_CHECK_TYPE_VERT_CONST(ele) CHECK_TYPE_ANY(ele, _BM_GENERIC_TYPE_VERT_CONST)
#define BM_CHECK_TYPE_VERT_NONCONST(ele) CHECK_TYPE_ANY(ele, _BM_GENERIC_TYPE_ELEM_NONCONST)
#define BM_CHECK_TYPE_VERT(ele) \
  CHECK_TYPE_ANY(ele, _BM_GENERIC_TYPE_VERT_NONCONST, _BM_GENERIC_TYPE_VERT_CONST)
/* edge */
#define _BM_GENERIC_TYPE_EDGE_NONCONST BMEdge *, BMEdge_OFlag *
#define _BM_GENERIC_TYPE_EDGE_CONST const BMEdge *, const BMEdge_OFlag *
#define BM_CHECK_TYPE_EDGE_CONST(ele) CHECK_TYPE_ANY(ele, _BM_GENERIC_TYPE_EDGE_CONST)
#define BM_CHECK_TYPE_EDGE_NONCONST(ele) CHECK_TYPE_ANY(ele, _BM_GENERIC_TYPE_ELEM_NONCONST)
#define BM_CHECK_TYPE_EDGE(ele) \
  CHECK_TYPE_ANY(ele, _BM_GENERIC_TYPE_EDGE_NONCONST, _BM_GENERIC_TYPE_EDGE_CONST)
/* face */
#define _BM_GENERIC_TYPE_FACE_NONCONST BMFace *, BMFace_OFlag *
#define _BM_GENERIC_TYPE_FACE_CONST const BMFace *, const BMFace_OFlag *
#define BM_CHECK_TYPE_FACE_CONST(ele) CHECK_TYPE_ANY(ele, _BM_GENERIC_TYPE_FACE_CONST)
#define BM_CHECK_TYPE_FACE_NONCONST(ele) CHECK_TYPE_ANY(ele, _BM_GENERIC_TYPE_ELEM_NONCONST)
#define BM_CHECK_TYPE_FACE(ele) \
  CHECK_TYPE_ANY(ele, _BM_GENERIC_TYPE_FACE_NONCONST, _BM_GENERIC_TYPE_FACE_CONST)

/* Assignment from a void* to a typed pointer is not allowed in C++,
 * casting the LHS to void works fine though.
 */
#ifdef __cplusplus
#  define BM_CHECK_TYPE_ELEM_ASSIGN(ele) (BM_CHECK_TYPE_ELEM(ele)), *((void **)&ele)
#else
#  define BM_CHECK_TYPE_ELEM_ASSIGN(ele) (BM_CHECK_TYPE_ELEM(ele)), ele
#endif

/** #BMHeader.hflag (char) */
enum {
  BM_ELEM_SELECT = (1 << 0),
  BM_ELEM_HIDDEN = (1 << 1),
  BM_ELEM_SEAM = (1 << 2),
  /** Used for faces and edges, note from the user POV, this is a sharp edge when disabled. */
  BM_ELEM_SMOOTH = (1 << 3),
  /**
   * Internal flag, used for ensuring correct normals
   * during multi-resolution interpolation, and any other time
   * when temp tagging is handy.
   * always assume dirty & clear before use.
   */
  BM_ELEM_TAG = (1 << 4),

  BM_ELEM_DRAW = (1 << 5), /* edge display */

  /** Spare tag, assumed dirty, use define in each function to name based on use. */
  BM_ELEM_TAG_ALT = (1 << 6),

  /**
   * For low level internal API tagging,
   * since tools may want to tag verts and not have functions clobber them.
   * Leave cleared!
   */
  BM_ELEM_INTERNAL_TAG = (1 << 7),
};

struct BPy_BMGeneric;
extern void bpy_bm_generic_invalidate(struct BPy_BMGeneric *self);

typedef bool (*BMElemFilterFunc)(const BMElem *, void *user_data);
typedef bool (*BMVertFilterFunc)(const BMVert *, void *user_data);
typedef bool (*BMEdgeFilterFunc)(const BMEdge *, void *user_data);
typedef bool (*BMFaceFilterFunc)(const BMFace *, void *user_data);
typedef bool (*BMLoopFilterFunc)(const BMLoop *, void *user_data);
typedef bool (*BMLoopPairFilterFunc)(const BMLoop *, const BMLoop *, void *user_data);

/* defines */
#define BM_ELEM_CD_SET_INT(ele, offset, f) \
  { \
    CHECK_TYPE_NONCONST(ele); \
    BLI_assert(offset != -1); \
    *((int *)((char *)(ele)->head.data + (offset))) = (f); \
  } \
  (void)0

#define BM_ELEM_CD_GET_INT(ele, offset) \
  (BLI_assert(offset != -1), *((int *)((char *)(ele)->head.data + (offset))))

#define BM_ELEM_CD_SET_BOOL(ele, offset, f) \
  { \
    CHECK_TYPE_NONCONST(ele); \
    BLI_assert(offset != -1); \
    *((bool *)((char *)(ele)->head.data + (offset))) = (f); \
  } \
  (void)0

#define BM_ELEM_CD_GET_BOOL(ele, offset) \
  (BLI_assert(offset != -1), *((bool *)((char *)(ele)->head.data + (offset))))

#if defined(__STDC_VERSION__) && (__STDC_VERSION__ >= 201112L)
#  define BM_ELEM_CD_GET_BOOL_P(ele, offset) \
    (BLI_assert(offset != -1), \
     _Generic(ele, \
              GENERIC_TYPE_ANY((bool *)POINTER_OFFSET((ele)->head.data, offset), \
                               _BM_GENERIC_TYPE_ELEM_NONCONST), \
              GENERIC_TYPE_ANY((const bool *)POINTER_OFFSET((ele)->head.data, offset), \
                               _BM_GENERIC_TYPE_ELEM_CONST)))
#else
#  define BM_ELEM_CD_GET_BOOL_P(ele, offset) \
    (BLI_assert(offset != -1), (bool *)((char *)(ele)->head.data + (offset)))
#endif

#if defined(__STDC_VERSION__) && (__STDC_VERSION__ >= 201112L)
#  define BM_ELEM_CD_GET_VOID_P(ele, offset) \
    (BLI_assert(offset != -1), \
     _Generic(ele, \
              GENERIC_TYPE_ANY(POINTER_OFFSET((ele)->head.data, offset), \
                               _BM_GENERIC_TYPE_ELEM_NONCONST), \
              GENERIC_TYPE_ANY((const void *)POINTER_OFFSET((ele)->head.data, offset), \
                               _BM_GENERIC_TYPE_ELEM_CONST)))
#else
#  define BM_ELEM_CD_GET_VOID_P(ele, offset) \
    (BLI_assert(offset != -1), (void *)((char *)(ele)->head.data + (offset)))
#endif

#define BM_ELEM_CD_SET_FLOAT(ele, offset, f) \
  { \
    CHECK_TYPE_NONCONST(ele); \
    BLI_assert(offset != -1); \
    *((float *)((char *)(ele)->head.data + (offset))) = (f); \
  } \
  (void)0

#define BM_ELEM_CD_GET_FLOAT(ele, offset) \
  (BLI_assert(offset != -1), *((float *)((char *)(ele)->head.data + (offset))))

#if defined(__STDC_VERSION__) && (__STDC_VERSION__ >= 201112L)

#  define BM_ELEM_CD_GET_FLOAT_P(ele, offset) \
    (BLI_assert(offset != -1), \
     _Generic(ele, \
              GENERIC_TYPE_ANY((float *)POINTER_OFFSET((ele)->head.data, offset), \
                               _BM_GENERIC_TYPE_ELEM_NONCONST), \
              GENERIC_TYPE_ANY((const float *)POINTER_OFFSET((ele)->head.data, offset), \
                               _BM_GENERIC_TYPE_ELEM_CONST)))

#  define BM_ELEM_CD_GET_FLOAT2_P(ele, offset) \
    (BLI_assert(offset != -1), \
     _Generic(ele, \
              GENERIC_TYPE_ANY((float(*)[2])POINTER_OFFSET((ele)->head.data, offset), \
                               _BM_GENERIC_TYPE_ELEM_NONCONST), \
              GENERIC_TYPE_ANY((const float(*)[2])POINTER_OFFSET((ele)->head.data, offset), \
                               _BM_GENERIC_TYPE_ELEM_CONST)))

#  define BM_ELEM_CD_GET_FLOAT3_P(ele, offset) \
    (BLI_assert(offset != -1), \
     _Generic(ele, \
              GENERIC_TYPE_ANY((float(*)[3])POINTER_OFFSET((ele)->head.data, offset), \
                               _BM_GENERIC_TYPE_ELEM_NONCONST), \
              GENERIC_TYPE_ANY((const float(*)[3])POINTER_OFFSET((ele)->head.data, offset), \
                               _BM_GENERIC_TYPE_ELEM_CONST)))

#else

#  define BM_ELEM_CD_GET_FLOAT_P(ele, offset) \
    (BLI_assert(offset != -1), (float *)((char *)(ele)->head.data + (offset)))

#  define BM_ELEM_CD_GET_FLOAT2_P(ele, offset) \
    (BLI_assert(offset != -1), (float(*)[2])((char *)(ele)->head.data + (offset)))

#  define BM_ELEM_CD_GET_FLOAT3_P(ele, offset) \
    (BLI_assert(offset != -1), (float(*)[3])((char *)(ele)->head.data + (offset)))

#endif

#define BM_ELEM_CD_SET_FLOAT2(ele, offset, f) \
  { \
    CHECK_TYPE_NONCONST(ele); \
    BLI_assert(offset != -1); \
    ((float *)((char *)(ele)->head.data + (offset)))[0] = (f)[0]; \
    ((float *)((char *)(ele)->head.data + (offset)))[1] = (f)[1]; \
  } \
  (void)0

#define BM_ELEM_CD_SET_FLOAT3(ele, offset, f) \
  { \
    CHECK_TYPE_NONCONST(ele); \
    BLI_assert(offset != -1); \
    ((float *)((char *)(ele)->head.data + (offset)))[0] = (f)[0]; \
    ((float *)((char *)(ele)->head.data + (offset)))[1] = (f)[1]; \
    ((float *)((char *)(ele)->head.data + (offset)))[2] = (f)[2]; \
  } \
  (void)0

#define BM_ELEM_CD_GET_FLOAT_AS_UCHAR(ele, offset) \
  (BLI_assert(offset != -1), (uchar)(BM_ELEM_CD_GET_FLOAT(ele, offset) * 255.0f))

/* Forward declarations. */

#ifdef USE_BMESH_HOLES
#  define BM_FACE_FIRST_LOOP(p) (((BMLoopList *)((p)->loops.first))->first)
#else
#  define BM_FACE_FIRST_LOOP(p) ((p)->l_first)
#endif

#define BM_DISK_EDGE_NEXT(e, v) \
  (CHECK_TYPE_INLINE(e, BMEdge *), \
   CHECK_TYPE_INLINE(v, BMVert *), \
   BLI_assert(BM_vert_in_edge(e, v)), \
   (((&e->v1_disk_link)[v == e->v2]).next))
#define BM_DISK_EDGE_PREV(e, v) \
  (CHECK_TYPE_INLINE(e, BMEdge *), \
   CHECK_TYPE_INLINE(v, BMVert *), \
   BLI_assert(BM_vert_in_edge(e, v)), \
   (((&e->v1_disk_link)[v == e->v2]).prev))

/**
 * Size to use for stack arrays when dealing with NGons, allocate after this limit is reached.
 * this value is rather arbitrary.
 */
#define BM_DEFAULT_NGON_STACK_SIZE 32
/**
 * Size to use for stack arrays dealing with connected mesh data
 * verts of faces, edges of vert - etc.
 * often used with #BM_iter_as_arrayN().
 */
#define BM_DEFAULT_ITER_STACK_SIZE 16

/* avoid inf loop, this value is arbitrary
 * but should not error on valid cases */
#define BM_LOOP_RADIAL_MAX 10000
#define BM_NGON_MAX 100000

/* setting zero so we can catch bugs in OpenMP/BMesh */
#ifdef DEBUG
#  define BM_OMP_LIMIT 0
#else
#  define BM_OMP_LIMIT 10000
#endif

#ifdef __cplusplus
}
#endif
