/* SPDX-FileCopyrightText: 2023 Blender Authors
 *
 * SPDX-License-Identifier: GPL-2.0-or-later */

#pragma once

/** \file
 * \ingroup bmesh
 */

#include "BLI_enum_flags.hh"
#include "BLI_ghash.h"

#include <cstdarg>

#include "bmesh_class.hh"

/**
 * operators represent logical, executable mesh modules.  all topological
 * operations involving a bmesh has to go through them.
 *
 * operators are nested, as are tool flags, which are private to an operator
 * when it's executed.  tool flags are allocated in layers, one per operator
 * execution, and are used for all internal flagging a tool needs to do.
 *
 * each operator has a series of "slots" which can be of the following types:
 * - simple numerical types
 * - arrays of elements (e.g. arrays of faces).
 * - hash mappings.
 *
 * each slot is identified by a slot code, as are each operator.
 * operators, and their slots, are defined in bmesh_opdefines.cc (with their
 * execution functions prototyped in bmesh_operators_private.hh), with all their
 * operator code and slot codes defined in bmesh_operators.hh.  see
 * bmesh_opdefines.cc and the BMOpDefine struct for how to define new operators.
 *
 * in general, operators are fed arrays of elements, created using either
 * #BMO_slot_buffer_from_hflag or #BMO_slot_buffer_from_flag
 * (or through one of the format specifiers in #BMO_op_callf or #BMO_op_initf).
 *
 * \note multiple element types (e.g. faces and edges)
 * can be fed to the same slot array.  Operators act on this data,
 * and possibly spit out data into output slots.
 *
 * \note operators should never read from header flags (e.g. element->head.flag).
 * For example, if you want an operator to only operate on selected faces, you
 * should use #BMO_slot_buffer_from_hflag to put the selected elements into a slot.
 *
 * \note when you read from an element slot array or mapping, you can either tool-flag
 * all the elements in it, or read them using an iterator API (which is semantically
 * similar to the iterator API in bmesh_iterators.hh).
 *
 * \note only #BMLoop items can't be put into slots as with verts, edges & faces.
 */

struct GHashIterator;

BLI_INLINE BMFlagLayer *BMO_elem_flag_from_header(BMHeader *ele_head)
{
  switch (ele_head->htype) {
    case BM_VERT:
      return ((BMVert_OFlag *)ele_head)->oflags;
    case BM_EDGE:
      return ((BMEdge_OFlag *)ele_head)->oflags;
    default:
      return ((BMFace_OFlag *)ele_head)->oflags;
  }
}

#define BMO_elem_flag_test(bm, ele, oflag) \
  _bmo_elem_flag_test(bm, BMO_elem_flag_from_header(&(ele)->head), oflag)
#define BMO_elem_flag_test_bool(bm, ele, oflag) \
  _bmo_elem_flag_test_bool(bm, BMO_elem_flag_from_header(&(ele)->head), oflag)
#define BMO_elem_flag_enable(bm, ele, oflag) \
  _bmo_elem_flag_enable( \
      bm, (BM_CHECK_TYPE_ELEM_NONCONST(ele), BMO_elem_flag_from_header(&(ele)->head)), oflag)
#define BMO_elem_flag_disable(bm, ele, oflag) \
  _bmo_elem_flag_disable( \
      bm, (BM_CHECK_TYPE_ELEM_NONCONST(ele), BMO_elem_flag_from_header(&(ele)->head)), oflag)
#define BMO_elem_flag_set(bm, ele, oflag, val) \
  _bmo_elem_flag_set(bm, \
                     (BM_CHECK_TYPE_ELEM_NONCONST(ele), BMO_elem_flag_from_header(&(ele)->head)), \
                     oflag, \
                     val)
#define BMO_elem_flag_toggle(bm, ele, oflag) \
  _bmo_elem_flag_toggle( \
      bm, (BM_CHECK_TYPE_ELEM_NONCONST(ele), BMO_elem_flag_from_header(&(ele)->head)), oflag)

/* take care not to instantiate args multiple times */
#ifdef __GNUC___
#  define _BMO_CAST_V_CONST(e) \
    ({ \
      typeof(e) _e = e; \
      (BM_CHECK_TYPE_VERT(_e), \
       BLI_assert(((const BMHeader *)_e)->htype == BM_VERT), \
       (const BMVert_OFlag *)_e); \
    })
#  define _BMO_CAST_E_CONST(e) \
    ({ \
      typeof(e) _e = e; \
      (BM_CHECK_TYPE_EDGE(_e), \
       BLI_assert(((const BMHeader *)_e)->htype == BM_EDGE), \
       (const BMEdge_OFlag *)_e); \
    })
#  define _BMO_CAST_F_CONST(e) \
    ({ \
      typeof(e) _e = e; \
      (BM_CHECK_TYPE_FACE(_e), \
       BLI_assert(((const BMHeader *)_e)->htype == BM_FACE), \
       (const BMFace_OFlag *)_e); \
    })
#  define _BMO_CAST_V(e) \
    ({ \
      typeof(e) _e = e; \
      (BM_CHECK_TYPE_VERT_NONCONST(_e), \
       BLI_assert(((BMHeader *)_e)->htype == BM_VERT), \
       (BMVert_OFlag *)_e); \
    })
#  define _BMO_CAST_E(e) \
    ({ \
      typeof(e) _e = e; \
      (BM_CHECK_TYPE_EDGE_NONCONST(_e), \
       BLI_assert(((BMHeader *)_e)->htype == BM_EDGE), \
       (BMEdge_OFlag *)_e); \
    })
#  define _BMO_CAST_F(e) \
    ({ \
      typeof(e) _e = e; \
      (BM_CHECK_TYPE_FACE_NONCONST(_e), \
       BLI_assert(((BMHeader *)_e)->htype == BM_FACE), \
       (BMFace_OFlag *)_e); \
    })
#else
#  define _BMO_CAST_V_CONST(e) (BM_CHECK_TYPE_VERT(e), (const BMVert_OFlag *)e)
#  define _BMO_CAST_E_CONST(e) (BM_CHECK_TYPE_EDGE(e), (const BMEdge_OFlag *)e)
#  define _BMO_CAST_F_CONST(e) (BM_CHECK_TYPE_FACE(e), (const BMFace_OFlag *)e)
#  define _BMO_CAST_V(e) (BM_CHECK_TYPE_VERT_NONCONST(e), (BMVert_OFlag *)e)
#  define _BMO_CAST_E(e) (BM_CHECK_TYPE_EDGE_NONCONST(e), (BMEdge_OFlag *)e)
#  define _BMO_CAST_F(e) (BM_CHECK_TYPE_FACE_NONCONST(e), (BMFace_OFlag *)e)
#endif

#define BMO_vert_flag_test(bm, e, oflag) \
  _bmo_elem_flag_test(bm, _BMO_CAST_V_CONST(e)->oflags, oflag)
#define BMO_vert_flag_test_bool(bm, e, oflag) \
  _bmo_elem_flag_test_bool(bm, _BMO_CAST_V_CONST(e)->oflags, oflag)
#define BMO_vert_flag_enable(bm, e, oflag) _bmo_elem_flag_enable(bm, _BMO_CAST_V(e)->oflags, oflag)
#define BMO_vert_flag_disable(bm, e, oflag) \
  _bmo_elem_flag_disable(bm, _BMO_CAST_V(e)->oflags, oflag)
#define BMO_vert_flag_set(bm, e, oflag, val) \
  _bmo_elem_flag_set(bm, _BMO_CAST_V(e)->oflags, oflag, val)
#define BMO_vert_flag_toggle(bm, e, oflag) _bmo_elem_flag_toggle(bm, _BMO_CAST_V(e)->oflags, oflag)

#define BMO_edge_flag_test(bm, e, oflag) \
  _bmo_elem_flag_test(bm, _BMO_CAST_E_CONST(e)->oflags, oflag)
#define BMO_edge_flag_test_bool(bm, e, oflag) \
  _bmo_elem_flag_test_bool(bm, _BMO_CAST_E_CONST(e)->oflags, oflag)
#define BMO_edge_flag_enable(bm, e, oflag) _bmo_elem_flag_enable(bm, _BMO_CAST_E(e)->oflags, oflag)
#define BMO_edge_flag_disable(bm, e, oflag) \
  _bmo_elem_flag_disable(bm, _BMO_CAST_E(e)->oflags, oflag)
#define BMO_edge_flag_set(bm, e, oflag, val) \
  _bmo_elem_flag_set(bm, _BMO_CAST_E(e)->oflags, oflag, val)
#define BMO_edge_flag_toggle(bm, e, oflag) _bmo_elem_flag_toggle(bm, _BMO_CAST_E(e)->oflags, oflag)

#define BMO_face_flag_test(bm, e, oflag) \
  _bmo_elem_flag_test(bm, _BMO_CAST_F_CONST(e)->oflags, oflag)
#define BMO_face_flag_test_bool(bm, e, oflag) \
  _bmo_elem_flag_test_bool(bm, _BMO_CAST_F_CONST(e)->oflags, oflag)
#define BMO_face_flag_enable(bm, e, oflag) _bmo_elem_flag_enable(bm, _BMO_CAST_F(e)->oflags, oflag)
#define BMO_face_flag_disable(bm, e, oflag) \
  _bmo_elem_flag_disable(bm, _BMO_CAST_F(e)->oflags, oflag)
#define BMO_face_flag_set(bm, e, oflag, val) \
  _bmo_elem_flag_set(bm, _BMO_CAST_F(e)->oflags, oflag, val)
#define BMO_face_flag_toggle(bm, e, oflag) _bmo_elem_flag_toggle(bm, _BMO_CAST_F(e)->oflags, oflag)

BLI_INLINE short _bmo_elem_flag_test(BMesh *bm, const BMFlagLayer *oflags, short oflag);
BLI_INLINE bool _bmo_elem_flag_test_bool(BMesh *bm, const BMFlagLayer *oflags, short oflag);
BLI_INLINE void _bmo_elem_flag_enable(BMesh *bm, BMFlagLayer *oflags, short oflag);
BLI_INLINE void _bmo_elem_flag_disable(BMesh *bm, BMFlagLayer *oflags, short oflag);
BLI_INLINE void _bmo_elem_flag_set(BMesh *bm, BMFlagLayer *oflags, short oflag, int val);
BLI_INLINE void _bmo_elem_flag_toggle(BMesh *bm, BMFlagLayer *oflags, short oflag);

/* slot type arrays are terminated by the last member
 * having a slot type of 0 */
enum eBMOpSlotType {
  /* BMO_OP_SLOT_SENTINEL = 0, */
  BMO_OP_SLOT_BOOL = 1,
  BMO_OP_SLOT_INT = 2,
  BMO_OP_SLOT_FLT = 3,

  /* normally store pointers to object, scene,
   * _never_ store arrays corresponding to mesh elements with this */
  BMO_OP_SLOT_PTR = 4, /* requires subtype BMO_OP_SLOT_SUBTYPE_PTR_xxx */
  BMO_OP_SLOT_MAT = 5,
  BMO_OP_SLOT_VEC = 8,

  /* after BMO_OP_SLOT_VEC, everything is dynamically allocated arrays.
   * We leave a space in the identifiers for future growth.
   *
   * it's very important this remain a power of two */
  BMO_OP_SLOT_ELEMENT_BUF = 9, /* list of verts/edges/faces */
  BMO_OP_SLOT_MAPPING = 10     /* simple hash map, requires subtype BMO_OP_SLOT_SUBTYPE_MAP_xxx */
};
#define BMO_OP_SLOT_TOTAL_TYPES 11

/* don't overlap values to avoid confusion */
enum eBMOpSlotSubType_Elem {
  /* use as flags */
  BMO_OP_SLOT_SUBTYPE_ELEM_VERT = BM_VERT,
  BMO_OP_SLOT_SUBTYPE_ELEM_EDGE = BM_EDGE,
  BMO_OP_SLOT_SUBTYPE_ELEM_FACE = BM_FACE,
  BMO_OP_SLOT_SUBTYPE_ELEM_IS_SINGLE = (BM_FACE << 1),
};
ENUM_OPERATORS(eBMOpSlotSubType_Elem)

enum eBMOpSlotSubType_Map {
  BMO_OP_SLOT_SUBTYPE_MAP_EMPTY = 64, /* use as a set(), unused value */
  BMO_OP_SLOT_SUBTYPE_MAP_ELEM = 65,
  BMO_OP_SLOT_SUBTYPE_MAP_FLT = 66,
  BMO_OP_SLOT_SUBTYPE_MAP_INT = 67,
  BMO_OP_SLOT_SUBTYPE_MAP_BOOL = 68,
  BMO_OP_SLOT_SUBTYPE_MAP_INTERNAL = 69, /* python can't convert these */
};
enum eBMOpSlotSubType_Ptr {
  BMO_OP_SLOT_SUBTYPE_PTR_BMESH = 100,
  BMO_OP_SLOT_SUBTYPE_PTR_SCENE = 101,
  BMO_OP_SLOT_SUBTYPE_PTR_OBJECT = 102,
  BMO_OP_SLOT_SUBTYPE_PTR_MESH = 103,
  BMO_OP_SLOT_SUBTYPE_PTR_STRUCT = 104,
};
enum eBMOpSlotSubType_Int {
  BMO_OP_SLOT_SUBTYPE_INT_ENUM = 200,
  BMO_OP_SLOT_SUBTYPE_INT_FLAG = 201,
};

union eBMOpSlotSubType_Union {
  eBMOpSlotSubType_Elem elem;
  eBMOpSlotSubType_Ptr ptr;
  eBMOpSlotSubType_Map map;
  eBMOpSlotSubType_Int intg;
};

struct BMO_FlagSet {
  int value;
  const char *identifier;
};

/* please ignore all these structures, don't touch them in tool code, except
 * for when your defining an operator with BMOpDefine. */

struct BMOpSlot {
  const char *slot_name; /* pointer to BMOpDefine.slot_args */
  eBMOpSlotType slot_type;
  eBMOpSlotSubType_Union slot_subtype;

  int len;
  //  int flag;  /* UNUSED */
  //  int index; /* index within slot array */  /* UNUSED */
  union {
    int i;
    float f;
    void *p;
    float vec[3];
    void **buf;
    GHash *ghash;
    struct {
      /** Don't clobber (i) when assigning flags, see #eBMOpSlotSubType_Int. */
      int _i;
      BMO_FlagSet *flags;
    } enum_data;
  } data;
};

/* mainly for use outside bmesh internal code */
#define BMO_SLOT_AS_BOOL(slot) ((slot)->data.i)
#define BMO_SLOT_AS_INT(slot) ((slot)->data.i)
#define BMO_SLOT_AS_FLOAT(slot) ((slot)->data.f)
#define BMO_SLOT_AS_VECTOR(slot) ((slot)->data.vec)
#define BMO_SLOT_AS_MATRIX(slot) ((float (*)[4])((slot)->data.p))
#define BMO_SLOT_AS_BUFFER(slot) ((slot)->data.buf)
#define BMO_SLOT_AS_GHASH(slot) ((slot)->data.ghash)

#define BMO_ASSERT_SLOT_IN_OP(slot, op) \
  BLI_assert(((slot >= (op)->slots_in) && (slot < &(op)->slots_in[BMO_OP_MAX_SLOTS])) || \
             ((slot >= (op)->slots_out) && (slot < &(op)->slots_out[BMO_OP_MAX_SLOTS])))

/* Limit hit, so expanded for bevel operator. Compiler complains if limit is hit. */
#define BMO_OP_MAX_SLOTS 21

/* BMOpDefine->type_flag */
enum BMOpTypeFlag {
  BMO_OPTYPE_FLAG_NOP = 0,
  /** Switch from multires tangent space to absolute coordinates. */
  BMO_OPTYPE_FLAG_UNTAN_MULTIRES = (1 << 0),
  BMO_OPTYPE_FLAG_NORMALS_CALC = (1 << 1),
  BMO_OPTYPE_FLAG_SELECT_FLUSH = (1 << 2),
  BMO_OPTYPE_FLAG_SELECT_VALIDATE = (1 << 3),
  BMO_OPTYPE_FLAG_INVALIDATE_CLNOR_ALL = (1 << 4),
};
ENUM_OPERATORS(BMOpTypeFlag)

struct BMOperator {
  struct BMOpSlot slots_in[BMO_OP_MAX_SLOTS];
  struct BMOpSlot slots_out[BMO_OP_MAX_SLOTS];
  void (*exec)(BMesh *bm, struct BMOperator *op);
  struct MemArena *arena;
  int type;
  BMOpTypeFlag type_flag;
  int flag; /* runtime options */
};

enum {
  BMO_FLAG_RESPECT_HIDE = 1,
};

#define BMO_FLAG_DEFAULTS BMO_FLAG_RESPECT_HIDE

#define MAX_SLOTNAME 32

struct BMOSlotType {
  char name[MAX_SLOTNAME];
  eBMOpSlotType type;
  eBMOpSlotSubType_Union subtype;
  BMO_FlagSet *enum_flags;
};

struct BMOpDefine {
  const char *opname;
  BMOSlotType slot_types_in[BMO_OP_MAX_SLOTS];
  BMOSlotType slot_types_out[BMO_OP_MAX_SLOTS];
  /**
   * Optional initialize function.
   * Can be used for setting defaults.
   */
  void (*init)(BMOperator *op);
  void (*exec)(BMesh *bm, BMOperator *op);
  BMOpTypeFlag type_flag;
};

/* -------------------------------------------------------------------- */
/** \name BMesh Operator API
 *
 * \note data types that use pointers (arrays, etc) must _never_ have it set directly.
 * Don't #BMO_slot_ptr_set to pass in a list of edges or any arrays.
 * \{ */

/**
 * \brief BMESH OPSTACK INIT OP
 *
 * Initializes an operator structure to a certain type
 */
void BMO_op_init(BMesh *bm, BMOperator *op, int flag, const char *opname);

/**
 * \brief BMESH OPSTACK EXEC OP
 *
 * Executes a passed in operator.
 *
 * This handles the allocation and freeing of temporary tool flag
 * layers and starting/stopping the modeling loop.
 * Can be called from other operators exec callbacks as well.
 */
void BMO_op_exec(BMesh *bm, BMOperator *op);

/**
 * \brief BMESH OPSTACK FINISH OP
 *
 * Does housekeeping chores related to finishing up an operator.
 *
 * \note the operator's tool flag is removed after it finishes executing in #BMO_op_exec.
 */
void BMO_op_finish(BMesh *bm, BMOperator *op);

/**
 * Count the number of elements with the specified flag enabled.
 * type can be a bit-mask of #BM_FACE, #BM_EDGE, or #BM_FACE.
 */
int BMO_mesh_enabled_flag_count(BMesh *bm, char htype, short oflag);

/**
 * Count the number of elements with the specified flag disabled.
 * type can be a bit-mask of #BM_FACE, #BM_EDGE, or #BM_FACE.
 */
int BMO_mesh_disabled_flag_count(BMesh *bm, char htype, short oflag);

/**
 * \brief BMESH OPSTACK PUSH
 *
 * Pushes the operator-stack down one level and allocates a new flag layer if appropriate.
 */
void BMO_push(BMesh *bm, BMOperator *op);
/**
 * \brief BMESH OPSTACK POP
 *
 * Pops the operator-stack one level and frees a flag layer if appropriate
 *
 * BMESH_TODO: investigate NOT freeing flag layers.
 */
void BMO_pop(BMesh *bm);

/** \} */

/* -------------------------------------------------------------------- */
/** \name Formatted Operator Initialization/Execution
 *
 * Format Strings for #BMOperator Initialization.
 *
 * This system is used to execute or initialize an operator,
 * using a formatted-string system.
 *
 * The basic format for the format string is:
 * `[operatorname] [slot_name]=%[code] [slot_name]=%[code]`
 *
 * Example:
 *
 * \code{.c}
 *     BMO_op_callf(bm, BMO_FLAG_DEFAULTS,
 *                  "delete context=%i geom=%hv",
 *                  DEL_ONLYFACES, BM_ELEM_SELECT);
 * \endcode
 * **Primitive Types**
 * - `b` - boolean (same as int but 1/0 only). #BMO_OP_SLOT_BOOL
 * - `i` - int. #BMO_OP_SLOT_INT
 * - `f` - float. #BMO_OP_SLOT_FLT
 * - `p` - pointer (normally to a Scene/Mesh/Object/BMesh). #BMO_OP_SLOT_PTR
 * - `m3` - 3x3 matrix of floats. #BMO_OP_SLOT_MAT
 * - `m4` - 4x4 matrix of floats. #BMO_OP_SLOT_MAT
 * - `v` - 3D vector of floats. #BMO_OP_SLOT_VEC
 * **Utility**
 *
 * Pass an existing slot which is copied to either an input or output slot.
 * Taking the operator and slot-name pair of args (BMOperator *, const char *).
 * - `s` - slot_in (lower case)
 * - `S` - slot_out (upper case)
 * **Element Buffer** (#BMO_OP_SLOT_ELEMENT_BUF)
 * - `e` - single element vert/edge/face (use with #BMO_OP_SLOT_SUBTYPE_ELEM_IS_SINGLE).
 * - `eb` - elem buffer, take an array and a length.
 * - `av` - all verts
 * - `ae` - all edges
 * - `af` - all faces
 * - `hv` - header flagged verts (hflag)
 * - `he` - header flagged edges (hflag)
 * - `hf` - header flagged faces (hflag)
 * - `Hv` - header flagged verts (hflag off)
 * - `He` - header flagged edges (hflag off)
 * - `Hf` - header flagged faces (hflag off)
 * - `fv` - flagged verts (oflag)
 * - `fe` - flagged edges (oflag)
 * - `ff` - flagged faces (oflag)
 * - `Fv` - flagged verts (oflag off)
 * - `Fe` - flagged edges (oflag off)
 * - `Ff` - flagged faces (oflag off)
 *
 * \note The common v/e/f suffix can be mixed,
 * so `avef` is can be used for all verts, edges and faces.
 * Order is not important so `Hfev` is also valid (all un-flagged verts, edges and faces).
 *
 * \{ */

/** Executes an operator. */
bool BMO_op_callf(BMesh *bm, int flag, const char *fmt, ...);

/**
 * Initializes, but doesn't execute an operator.  this is so you can
 * gain access to the outputs of the operator.  note that you have
 * to execute/finish (BMO_op_exec and BMO_op_finish) yourself.
 */
bool BMO_op_initf(BMesh *bm, BMOperator *op, int flag, const char *fmt, ...);

/**
 * A `va_list` version, used to implement the above two functions,
 * plus #EDBM_op_callf in editmesh_utils.cc.
 */
bool BMO_op_vinitf(BMesh *bm, BMOperator *op, int flag, const char *fmt, va_list vlist);

/** \} */

/* -------------------------------------------------------------------- */
/** \name BMesh Operator Slot Access
 * \{ */

/**
 * \brief BMESH OPSTACK HAS SLOT
 *
 * \return Success if the slot if found.
 */
bool BMO_slot_exists(BMOpSlot slot_args[BMO_OP_MAX_SLOTS], const char *identifier);

/* get a pointer to a slot.  this may be removed layer on from the public API. */
/**
 * \brief BMESH OPSTACK GET SLOT
 *
 * Returns a pointer to the slot of type 'slot_code'
 */
BMOpSlot *BMO_slot_get(BMOpSlot slot_args[BMO_OP_MAX_SLOTS], const char *identifier);

/* copies the data of a slot from one operator to another.  src and dst are the
 * source/destination slot codes, respectively. */
#define BMO_slot_copy(op_src, slots_src, slot_name_src, op_dst, slots_dst, slot_name_dst) \
  _bmo_slot_copy( \
      (op_src)->slots_src, slot_name_src, (op_dst)->slots_dst, slot_name_dst, (op_dst)->arena)

/**
 * \brief BMESH OPSTACK COPY SLOT
 *
 * define used.
 * Copies data from one slot to another.
 */
void _bmo_slot_copy(BMOpSlot slot_args_src[BMO_OP_MAX_SLOTS],
                    const char *slot_name_src,
                    BMOpSlot slot_args_dst[BMO_OP_MAX_SLOTS],
                    const char *slot_name_dst,
                    struct MemArena *arena_dst);

/** \} */

/** Delete "context" slot values, used for operator too. */
enum {
  DEL_VERTS = 1,
  DEL_EDGES,
  DEL_ONLYFACES,
  DEL_EDGESFACES,
  DEL_FACES,
  /* A version of 'DEL_FACES' that keeps edges on face boundaries,
   * allowing the surrounding edge-loop to be kept from removed face regions. */
  DEL_FACES_KEEP_BOUNDARY,
  DEL_ONLYTAGGED,
};

enum BMO_SymmDirection {
  BMO_SYMMETRIZE_NEGATIVE_X,
  BMO_SYMMETRIZE_NEGATIVE_Y,
  BMO_SYMMETRIZE_NEGATIVE_Z,

  BMO_SYMMETRIZE_POSITIVE_X,
  BMO_SYMMETRIZE_POSITIVE_Y,
  BMO_SYMMETRIZE_POSITIVE_Z,
};

enum BMO_Delimit {
  BMO_DELIM_NORMAL = 1 << 0,
  BMO_DELIM_MATERIAL = 1 << 1,
  BMO_DELIM_SEAM = 1 << 2,
  BMO_DELIM_SHARP = 1 << 3,
  BMO_DELIM_UV = 1 << 4,
};
ENUM_OPERATORS(BMO_Delimit)

void BMO_op_flag_enable(BMesh *bm, BMOperator *op, int op_flag);
void BMO_op_flag_disable(BMesh *bm, BMOperator *op, int op_flag);

/* -------------------------------------------------------------------- */
/** \name BMesh Operator Slot Get/Set
 * \{ */

void BMO_slot_float_set(BMOpSlot slot_args[BMO_OP_MAX_SLOTS], const char *slot_name, float f);
float BMO_slot_float_get(BMOpSlot slot_args[BMO_OP_MAX_SLOTS], const char *slot_name);
void BMO_slot_int_set(BMOpSlot slot_args[BMO_OP_MAX_SLOTS], const char *slot_name, int i);
int BMO_slot_int_get(BMOpSlot slot_args[BMO_OP_MAX_SLOTS], const char *slot_name);
void BMO_slot_bool_set(BMOpSlot slot_args[BMO_OP_MAX_SLOTS], const char *slot_name, bool i);
bool BMO_slot_bool_get(BMOpSlot slot_args[BMO_OP_MAX_SLOTS], const char *slot_name);
/**
 * Return a copy of the element buffer.
 */
void *BMO_slot_as_arrayN(BMOpSlot slot_args[BMO_OP_MAX_SLOTS], const char *slot_name, int *len);

/**
 * Don't pass in arrays that are supposed to map to elements this way.
 *
 * so, e.g. passing in list of floats per element in another slot is bad.
 * passing in, e.g. pointer to an edit-mesh for the conversion operator is fine though.
 */
void BMO_slot_ptr_set(BMOpSlot slot_args[BMO_OP_MAX_SLOTS], const char *slot_name, void *p);
void *BMO_slot_ptr_get(BMOpSlot slot_args[BMO_OP_MAX_SLOTS], const char *slot_name);
void BMO_slot_vec_set(BMOpSlot slot_args[BMO_OP_MAX_SLOTS],
                      const char *slot_name,
                      const float vec[3]);
void BMO_slot_vec_get(BMOpSlot slot_args[BMO_OP_MAX_SLOTS], const char *slot_name, float r_vec[3]);

/**
 * Only supports square matrices.
 * size must be 3 or 4; this API is meant only for transformation matrices.
 *
 * \note the matrix is stored in 4x4 form, and it's safe to call whichever function you want.
 */
void BMO_slot_mat_set(BMOperator *op,
                      BMOpSlot slot_args[BMO_OP_MAX_SLOTS],
                      const char *slot_name,
                      const float *mat,
                      int size);
void BMO_slot_mat4_get(BMOpSlot slot_args[BMO_OP_MAX_SLOTS],
                       const char *slot_name,
                       float r_mat[4][4]);
void BMO_slot_mat3_get(BMOpSlot slot_args[BMO_OP_MAX_SLOTS],
                       const char *slot_name,
                       float r_mat[3][3]);

/** \} */

void BMO_mesh_flag_disable_all(BMesh *bm, BMOperator *op, char htype, short oflag);

void BMO_mesh_selected_remap(BMesh *bm,
                             BMOpSlot *slot_vert_map,
                             BMOpSlot *slot_edge_map,
                             BMOpSlot *slot_face_map,
                             bool check_select);

/**
 * Copies the values from another slot to the end of the output slot.
 */
#define BMO_slot_buffer_append( \
    op_src, slots_src, slot_name_src, op_dst, slots_dst, slot_name_dst) \
  _bmo_slot_buffer_append( \
      (op_src)->slots_src, slot_name_src, (op_dst)->slots_dst, slot_name_dst, (op_dst)->arena)
/**
 * Copies the values from another slot to the end of the output slot.
 */
void _bmo_slot_buffer_append(BMOpSlot slot_args_dst[BMO_OP_MAX_SLOTS],
                             const char *slot_name_dst,
                             BMOpSlot slot_args_src[BMO_OP_MAX_SLOTS],
                             const char *slot_name_src,
                             struct MemArena *arena_dst);

/**
 * Puts every element of type 'type' (which is a bit-mask) with tool flag 'flag', into a slot.
 */
void BMO_slot_buffer_from_enabled_flag(BMesh *bm,
                                       BMOperator *op,
                                       BMOpSlot slot_args[BMO_OP_MAX_SLOTS],
                                       const char *slot_name,
                                       char htype,
                                       short oflag);

/**
 * Puts every element of type 'type' (which is a bit-mask) without tool flag 'flag', into a slot.
 */
void BMO_slot_buffer_from_disabled_flag(BMesh *bm,
                                        BMOperator *op,
                                        BMOpSlot slot_args[BMO_OP_MAX_SLOTS],
                                        const char *slot_name,
                                        char htype,
                                        short oflag);

/**
 * \brief BMO_FLAG_BUFFER
 *
 * Flags elements in a slots buffer
 */
void BMO_slot_buffer_flag_enable(BMesh *bm,
                                 BMOpSlot slot_args[BMO_OP_MAX_SLOTS],
                                 const char *slot_name,
                                 char htype,
                                 short oflag);
/**
 * \brief BMO_FLAG_BUFFER
 *
 * Removes flags from elements in a slots buffer
 */
void BMO_slot_buffer_flag_disable(BMesh *bm,
                                  BMOpSlot slot_args[BMO_OP_MAX_SLOTS],
                                  const char *slot_name,
                                  char htype,
                                  short oflag);

/**
 * \brief BMO_FLAG_BUFFER
 *
 * Header Flags elements in a slots buffer, automatically
 * using the selection API where appropriate.
 */
void BMO_slot_buffer_hflag_enable(BMesh *bm,
                                  BMOpSlot slot_args[BMO_OP_MAX_SLOTS],
                                  const char *slot_name,
                                  char htype,
                                  char hflag,
                                  bool do_flush);
/**
 * \brief BMO_FLAG_BUFFER
 *
 * Removes flags from elements in a slots buffer, automatically
 * using the selection API where appropriate.
 */
void BMO_slot_buffer_hflag_disable(BMesh *bm,
                                   BMOpSlot slot_args[BMO_OP_MAX_SLOTS],
                                   const char *slot_name,
                                   char htype,
                                   char hflag,
                                   bool do_flush);

/**
 * Puts every element of type 'type' (which is a bit-mask) with header flag 'flag', into a slot.
 * \note ignores hidden elements (e.g. elements with header flag BM_ELEM_HIDDEN set).
 */
void BMO_slot_buffer_from_enabled_hflag(BMesh *bm,
                                        BMOperator *op,
                                        BMOpSlot slot_args[BMO_OP_MAX_SLOTS],
                                        const char *slot_name,
                                        char htype,
                                        char hflag);
/**
 * Puts every element of type 'type' (which is a bit-mask) without header flag 'flag', into a slot.
 * \note ignores hidden elements (e.g. elements with header flag BM_ELEM_HIDDEN set).
 */
void BMO_slot_buffer_from_disabled_hflag(BMesh *bm,
                                         BMOperator *op,
                                         BMOpSlot slot_args[BMO_OP_MAX_SLOTS],
                                         const char *slot_name,
                                         char htype,
                                         char hflag);

void BMO_slot_buffer_from_array(BMOperator *op,
                                BMOpSlot *slot,
                                BMHeader **ele_buffer,
                                int ele_buffer_len);

void BMO_slot_buffer_from_single(BMOperator *op, BMOpSlot *slot, BMHeader *ele);
void *BMO_slot_buffer_get_single(BMOpSlot *slot);

/** Return the number of elements inside a slot array. */
int BMO_slot_buffer_len(BMOpSlot slot_args[BMO_OP_MAX_SLOTS], const char *slot_name);
/** Return the number of elements inside a slot map. */
int BMO_slot_map_len(BMOpSlot slot_args[BMO_OP_MAX_SLOTS], const char *slot_name);

/**
 * Inserts a key/value mapping into a mapping slot.  note that it copies the
 * value, it doesn't store a reference to it.
 */
void BMO_slot_map_insert(BMOperator *op, BMOpSlot *slot, const void *element, const void *data);

/**
 * Flags all elements in a mapping.
 * \note that the mapping must only have #BMesh elements in it.
 */
void BMO_slot_map_to_flag(BMesh *bm,
                          BMOpSlot slot_args[BMO_OP_MAX_SLOTS],
                          const char *slot_name,
                          char htype,
                          short oflag);

void *BMO_slot_buffer_alloc(BMOperator *op,
                            BMOpSlot slot_args[BMO_OP_MAX_SLOTS],
                            const char *slot_name,
                            int len);

/**
 * \brief BMO_ALL_TO_SLOT
 *
 * Copies all elements of a certain type into an operator slot.
 */
void BMO_slot_buffer_from_all(BMesh *bm,
                              BMOperator *op,
                              BMOpSlot slot_args[BMO_OP_MAX_SLOTS],
                              const char *slot_name,
                              char htype);

/**
 * This part of the API is used to iterate over element buffer or
 * mapping slots.
 *
 * for example, iterating over the faces in a slot is:
 *
 * \code{.c}
 *
 *    BMOIter oiter;
 *    BMFace *f;
 *
 *    f = BMO_iter_new(&oiter, some_operator, "slot_name", BM_FACE);
 *    for (; f; f = BMO_iter_step(&oiter)) {
 *        // do something with the face
 *    }
 *
 * another example, iterating over a mapping:
 *    BMOIter oiter;
 *    void *key;
 *    void *val;
 *
 *    key = BMO_iter_new(&oiter, bm, some_operator, "slot_name", 0);
 *    for (; key; key = BMO_iter_step(&oiter)) {
 *        val = BMO_iter_map_value(&oiter);
 *        //do something with the key/val pair
 *        //note that val is a pointer to the val data,
 *        //whether it's a float, pointer, whatever.
 *        //
 *        // so to get a pointer, for example, use:
 *        //  *((void **)BMO_iter_map_value(&oiter));
 *        //or something like that.
 *    }
 * \endcode
 */

/* contents of this structure are private,
 * don't directly access. */
struct BMOIter {
  BMOpSlot *slot;
  int cur;  // for arrays
  GHashIterator giter;
  void **val;
  /** Bit-wise '&' with #BMHeader.htype */
  char restrictmask;
};

void *BMO_slot_buffer_get_first(BMOpSlot slot_args[BMO_OP_MAX_SLOTS], const char *slot_name);

/**
 * \brief New Iterator
 *
 * \param restrictmask: restricts the iteration to certain element types
 * (e.g. combination of BM_VERT, BM_EDGE, BM_FACE), if iterating
 * over an element buffer (not a mapping). */
void *BMO_iter_new(BMOIter *iter,
                   BMOpSlot slot_args[BMO_OP_MAX_SLOTS],
                   const char *slot_name,
                   char restrictmask);
void *BMO_iter_step(BMOIter *iter);

/**
 * Returns a pointer to the key-value when iterating over mappings.
 * remember for pointer maps this will be a pointer to a pointer.
 */
void **BMO_iter_map_value_p(BMOIter *iter);
void *BMO_iter_map_value_ptr(BMOIter *iter);

float BMO_iter_map_value_float(BMOIter *iter);
int BMO_iter_map_value_int(BMOIter *iter);
bool BMO_iter_map_value_bool(BMOIter *iter);

#define BMO_ITER(ele, iter, slot_args, slot_name, restrict_flag) \
  for (BM_CHECK_TYPE_ELEM_ASSIGN(ele) = BMO_iter_new(iter, slot_args, slot_name, restrict_flag); \
       ele; \
       BM_CHECK_TYPE_ELEM_ASSIGN(ele) = BMO_iter_step(iter))

#define BMO_ITER_INDEX(ele, iter, slot_args, slot_name, restrict_flag, i_) \
  for (BM_CHECK_TYPE_ELEM_ASSIGN(ele) = BMO_iter_new(iter, slot_args, slot_name, restrict_flag), \
      i_ = 0; \
       ele; \
       BM_CHECK_TYPE_ELEM_ASSIGN(ele) = BMO_iter_step(iter), i_++)

/* operator slot type information - size of one element of the type given. */
extern const int BMO_OPSLOT_TYPEINFO[BMO_OP_SLOT_TOTAL_TYPES];

int BMO_opcode_from_opname(const char *opname);
