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
 * Contributor(s): Joseph Eagar.
 *
 * ***** END GPL LICENSE BLOCK *****
 */

#ifndef __BMESH_OPERATOR_API_H__
#define __BMESH_OPERATOR_API_H__

/** \file blender/bmesh/bmesh_operator_api.h
 *  \ingroup bmesh
 */

#ifdef __cplusplus
extern "C" {
#endif

#include "BLI_memarena.h"
#include "BLI_ghash.h"

#include "BKE_utildefines.h"

#include <stdarg.h>
#include <string.h> /* for memcpy */

/*
 * operators represent logical, executable mesh modules.  all topological
 * operations involving a bmesh has to go through them.
 *
 * operators are nested, as are tool flags, which are private to an operator
 * when it's executed.  tool flags are allocated in layers, one per operator
 * execution, and are used for all internal flagging a tool needs to do.
 *
 * each operator has a series of "slots," which can be of the following types:
 * - simple numerical types
 * - arrays of elements (e.g. arrays of faces).
 * - hash mappings.
 *
 * each slot is identified by a slot code, as are each operator.
 * operators, and their slots, are defined in bmesh_opdefines.c (with their
 * execution functions prototyped in bmesh_operators_private.h), with all their
 * operator code and slot codes defined in bmesh_operators.h.  see
 * bmesh_opdefines.c and the BMOpDefine struct for how to define new operators.
 *
 * in general, operators are fed arrays of elements, created using either
 * BM_HeaderFlag_To_Slot or BM_Flag_To_Slot (or through one of the format
 * specifyers in BMO_op_callf or BMO_op_initf).  Note that multiple element
 * types (e.g. faces and edges) can be fed to the same slot array.  Operators
 * act on this data, and possibly spit out data into output slots.
 *
 * some notes:
 * - operators should never read from header flags (e.g. element->head.flag). for
 *   example, if you want an operator to only operate on selected faces, you
 *   should use BM_HeaderFlag_To_Slot to put the selected elements into a slot.
 * - when you read from an element slot array or mapping, you can either tool-flag
 *   all the elements in it, or read them using an iterator APi (which is
 *   semantically similar to the iterator api in bmesh_iterators.h).
 */

struct GHashIterator;

#define BMO_elem_flag_test(   bm, ele, oflag)      _bmo_elem_flag_test    (bm, (ele)->oflags, oflag)
#define BMO_elem_flag_enable( bm, ele, oflag)      _bmo_elem_flag_enable  (bm, (ele)->oflags, oflag)
#define BMO_elem_flag_disable(bm, ele, oflag)      _bmo_elem_flag_disable (bm, (ele)->oflags, oflag)
#define BMO_elem_flag_set(    bm, ele, oflag, val) _bmo_elem_flag_set     (bm, (ele)->oflags, oflag, val)
#define BMO_elem_flag_toggle( bm, ele, oflag)      _bmo_elem_flag_toggle  (bm, (ele)->oflags, oflag)

BM_INLINE short _bmo_elem_flag_test(BMesh *bm, BMFlagLayer *oflags, const short oflag);
BM_INLINE void  _bmo_elem_flag_enable(BMesh *bm, BMFlagLayer *oflags, const short oflag);
BM_INLINE void  _bmo_elem_flag_disable(BMesh *bm, BMFlagLayer *oflags, const short oflag);
BM_INLINE void  _bmo_elem_flag_set(BMesh *bm, BMFlagLayer *oflags, const short oflag, int val);
BM_INLINE void  _bmo_elem_flag_toggle(BMesh *bm, BMFlagLayer *oflags, const short oflag);

/* slot type arrays are terminated by the last member
 * having a slot type of 0.*/
enum {
	BMO_OP_SLOT_SENTINEL = 0,
	BMO_OP_SLOT_BOOL = 1,
	BMO_OP_SLOT_INT = 2,
	BMO_OP_SLOT_FLT = 3,

	/* normally store pointers to object, scene,
	 * _never_ store arrays corresponding to mesh elements with this */
	BMO_OP_SLOT_PNT = 4,
	BMO_OP_SLOT_MAT = 5,
	BMO_OP_SLOT_VEC = 8,

	/* after BMO_OP_SLOT_VEC, everything is dynamically allocated arrays.
	 * We leave a space in the identifiers for future growth.
	 *
	 * it's very important this remain a power of two */
	BMO_OP_SLOT_ELEMENT_BUF = 9, /* list of verts/edges/faces */
	BMO_OP_SLOT_MAPPING = 10 /* simple hash map */
};
#define BMO_OP_SLOT_TOTAL_TYPES 11

/* please ignore all these structures, don't touch them in tool code, except
 * for when your defining an operator with BMOpDefine.*/

typedef struct BMOpSlot {
	int slottype;
	int len;
	int flag;
	int index; /* index within slot array */
	union {
		int i;
		float f;
		void *p;
		float vec[3];
		void *buf;
		GHash *ghash;
	} data;
} BMOpSlot;

#define BMO_OP_MAX_SLOTS 16 /* way more than probably needed */

#ifdef slots
#undef slots
#endif

typedef struct BMOperator {
	int type;
	int slottype;
	int needflag;
	int flag;
	struct BMOpSlot slots[BMO_OP_MAX_SLOTS]; void (*exec)(BMesh *bm, struct BMOperator *op);
	MemArena *arena;
} BMOperator;

#define MAX_SLOTNAME	32

typedef struct BMOSlotType {
	int type;
	char name[MAX_SLOTNAME];
} BMOSlotType;

typedef struct BMOpDefine {
	const char *name;
	BMOSlotType slottypes[BMO_OP_MAX_SLOTS];
	void (*exec)(BMesh *bm, BMOperator *op);
	int flag;
} BMOpDefine;

/* BMOpDefine->flag */
#define BMO_OP_FLAG_UNTAN_MULTIRES		1 /*switch from multires tangent space to absolute coordinates*/

/* ensures consistent normals before operator execution,
 * restoring the original ones windings/normals afterwards.
 * keep in mind, this won't work if the input mesh isn't
 * manifold.*/
#define BMO_OP_FLAG_RATIONALIZE_NORMALS 2

/*------------- Operator API --------------*/

/* data types that use pointers (arrays, etc) should never
 * have it set directly.  and never use BMO_slot_ptr_set to
 * pass in a list of edges or any arrays, really.*/

void BMO_op_init(BMesh *bm, BMOperator *op, const char *opname);

/* executes an operator, pushing and popping a new tool flag
 * layer as appropriate.*/
void BMO_op_exec(BMesh *bm, BMOperator *op);

/* finishes an operator (though note the operator's tool flag is removed
 * after it finishes executing in BMO_op_exec).*/
void BMO_op_finish(BMesh *bm, BMOperator *op);

/* count the number of elements with a specific flag.
 * type can be a bitmask of BM_FACE, BM_EDGE, or BM_FACE. */
int BMO_mesh_flag_count(BMesh *bm, const short oflag, const char htype);

/*---------formatted operator initialization/execution-----------*/
/*
 * this system is used to execute or initialize an operator,
 * using a formatted-string system.
 *
 * for example, BMO_op_callf(bm, "del geom=%hf context=%i", BM_ELEM_SELECT, DEL_FACES);
 * . . .will execute the delete operator, feeding in selected faces, deleting them.
 *
 * the basic format for the format string is:
 *   [operatorname] [slotname]=%[code] [slotname]=%[code]
 *
 * as in printf, you pass in one additional argument to the function
 * for every code.
 *
 * the formatting codes are:
 *    %d - put int in slot
 *    %f - put float in slot
 *    %p - put pointer in slot
 *    %h[f/e/v] - put elements with a header flag in slot.
 *                 the letters after %h define which element types to use,
 *             so e.g. %hf will do faces, %hfe will do faces and edges,
 *             %hv will do verts, etc.  must pass in at least one
 *             element type letter.
 *    %f[f/e/v] - same as %h, except it deals with tool flags instead of
 *                 header flags.
 *    %a[f/e/v] - pass all elements (of types specified by f/e/v) to the
 *                 slot.
 *    %e        - pass in a single element.
 *    %v - pointer to a float vector of length 3.
 *    %m[3/4] - matrix, 3/4 refers to the matrix size, 3 or 4.  the
 *              corresponding argument must be a pointer to
 *          a float matrix.
 *    %s - copy a slot from another op, instead of mapping to one
 *         argument, it maps to two, a pointer to an operator and
 *     a slot name.
 */
void BMO_push(BMesh *bm, BMOperator *op);
void BMO_pop(BMesh *bm);

/*executes an operator*/
int BMO_op_callf(BMesh *bm, const char *fmt, ...);

/* initializes, but doesn't execute an operator.  this is so you can
 * gain access to the outputs of the operator.  note that you have
 * to execute/finitsh (BMO_op_exec and BMO_op_finish) yourself. */
int BMO_op_initf(BMesh *bm, BMOperator *op, const char *fmt, ...);

/* va_list version, used to implement the above two functions,
 * plus EDBM_CallOpf in bmeshutils.c. */
int BMO_op_vinitf(BMesh *bm, BMOperator *op, const char *fmt, va_list vlist);

/* test whether a named slot exists */
int BMO_slot_exists(BMOperator *op, const char *slotname);

/* get a pointer to a slot.  this may be removed layer on from the public API. */
BMOpSlot *BMO_slot_get(BMOperator *op, const char *slotname);

/* copies the data of a slot from one operator to another.  src and dst are the
 * source/destination slot codes, respectively. */
void BMO_slot_copy(BMOperator *source_op, BMOperator *dest_op,
                   const char *src, const char *dst);

/* del "context" slot values, used for operator too */
enum {
	DEL_VERTS = 1,
	DEL_EDGES,
	DEL_ONLYFACES,
	DEL_EDGESFACES,
	DEL_FACES,
	DEL_ALL,
	DEL_ONLYTAGGED
};

void BMO_op_flag_enable(BMesh *bm, BMOperator *op, const int op_flag);
void BMO_op_flag_disable(BMesh *bm, BMOperator *op, const int op_flag);

void  BMO_slot_float_set(BMOperator *op, const char *slotname, const float f);
float BMO_slot_float_get(BMOperator *op, const char *slotname);
void  BMO_slot_int_set(BMOperator *op, const char *slotname, const int i);
int   BMO_slot_int_get(BMOperator *op, const char *slotname);
void  BMO_slot_bool_set(BMOperator *op, const char *slotname, const int i);
int   BMO_slot_bool_get(BMOperator *op, const char *slotname);

/* don't pass in arrays that are supposed to map to elements this way.
 *
 * so, e.g. passing in list of floats per element in another slot is bad.
 * passing in, e.g. pointer to an editmesh for the conversion operator is fine
 * though. */
void  BMO_slot_ptr_set(BMOperator *op, const char *slotname, void *p);
void *BMO_slot_ptr_get(BMOperator *op, const char *slotname);
void  BMO_slot_vec_set(BMOperator *op, const char *slotname, const float vec[3]);
void  BMO_slot_vec_get(BMOperator *op, const char *slotname, float r_vec[3]);

/* only supports square mats */
/* size must be 3 or 4; this api is meant only for transformation matrices.
 * note that internally the matrix is stored in 4x4 form, and it's safe to
 * call whichever BMO_Get_Mat* function you want. */
void BMO_slot_mat_set(BMOperator *op, const char *slotname, const float *mat, int size);
void BMO_slot_mat4_get(BMOperator *op, const char *slotname, float r_mat[4][4]);
void BMO_slot_mat3_set(BMOperator *op, const char *slotname, float r_mat[3][3]);

void BMO_mesh_flag_disable_all(BMesh *bm, BMOperator *op, const char htype, const short oflag);

/* puts every element of type type (which is a bitmask) with tool flag flag,
 * into a slot. */
void BMO_slot_from_flag(BMesh *bm, BMOperator *op, const char *slotname,
                        const short oflag, const char htype);

/* tool-flags all elements inside an element slot array with flag flag. */
void BMO_slot_buffer_flag_enable(BMesh *bm, BMOperator *op, const char *slotname,
                                 const short oflag, const char htype);
/* clears tool-flag flag from all elements inside a slot array. */
void BMO_slot_buffer_flag_disable(BMesh *bm, BMOperator *op, const char *slotname,
                                  const short oflag, const char htype);

/* tool-flags all elements inside an element slot array with flag flag. */
void BMO_slot_buffer_hflag_enable(BMesh *bm, BMOperator *op, const char *slotname,
                                  const char hflag, const char htype, char do_flush_select);
/* clears tool-flag flag from all elements inside a slot array. */
void BMO_slot_buffer_hflag_disable(BMesh *bm, BMOperator *op, const char *slotname,
                                   const char hflag, const char htype, char do_flush_select);

/* puts every element of type type (which is a bitmask) with header flag
 * flag, into a slot.  note: ignores hidden elements (e.g. elements with
 * header flag BM_ELEM_HIDDEN set).*/
void BMO_slot_from_hflag(BMesh *bm, BMOperator *op, const char *slotname,
                         const char hflag, const char htype);

/* counts number of elements inside a slot array. */
int BMO_slot_buf_count(BMesh *bm, BMOperator *op, const char *slotname);
int BMO_slot_map_count(BMesh *bm, BMOperator *op, const char *slotname);

void BMO_slot_map_insert(BMesh *UNUSED(bm), BMOperator *op, const char *slotname,
                         void *element, void *data, int len);

/* Counts the number of edges with tool flag toolflag around
 */
int BMO_vert_edge_flags_count(BMesh *bm, BMVert *v, const short oflag);

/* flags all elements in a mapping.  note that the mapping must only have
 * bmesh elements in it.*/
void BMO_slot_map_to_flag(BMesh *bm, BMOperator *op, const char *slotname,
                          const short oflag, const char hflag);

/* this part of the API is used to iterate over element buffer or
 * mapping slots.
 *
 * for example, iterating over the faces in a slot is:
 *
 *    BMOIter oiter;
 *    BMFace *f;
 *
 *    f = BMO_iter_new(&oiter, bm, some_operator, "slotname", BM_FACE);
 *    for (; f; f=BMO_iter_step(&oiter)) {
 *        /do something with the face
 *    }
 *
 * another example, iterating over a mapping:
 *    BMOIter oiter;
 *    void *key;
 *    void *val;
 *
 *    key = BMO_iter_new(&oiter, bm, some_operator, "slotname", 0);
 *    for (; key; key=BMO_iter_step(&oiter)) {
 *        val = BMO_iter_map_value(&oiter);
 *        //do something with the key/val pair
 *        //note that val is a pointer to the val data,
 *        //whether it's a float, pointer, whatever.
 *        //
 *        // so to get a pointer, for example, use:
 *        //  *((void**)BMO_iter_map_value(&oiter));
 *        //or something like that.
 *    }
 */

/* contents of this structure are private,
 * don't directly access. */
typedef struct BMOIter {
	BMOpSlot *slot;
	int cur; //for arrays
	GHashIterator giter;
	void *val;
	char restrictmask; /* bitwise '&' with BMHeader.htype */
} BMOIter;

void *BMO_slot_elem_first(BMOperator *op, const char *slotname);

/* restrictmask restricts the iteration to certain element types
 * (e.g. combination of BM_VERT, BM_EDGE, BM_FACE), if iterating
 * over an element buffer (not a mapping).*/
void *BMO_iter_new(BMOIter *iter, BMesh *bm, BMOperator *op,
                   const char *slotname, const char restrictmask);
void *BMO_iter_step(BMOIter *iter);

/* returns a pointer to the key value when iterating over mappings.
 * remember for pointer maps this will be a pointer to a pointer.*/
void *BMO_iter_map_value(BMOIter *iter);

/* use this for pointer mappings */
void *BMO_iter_map_value_p(BMOIter *iter);

/* use this for float mappings */
float BMO_iter_map_value_f(BMOIter *iter);

#define BMO_ITER(ele, iter, bm, op, slotname, restrict)   \
	ele = BMO_iter_new(iter, bm, op, slotname, restrict); \
	for ( ; ele; ele=BMO_iter_step(iter))

/******************* Inlined Functions********************/
typedef void (*opexec)(BMesh *bm, BMOperator *op);

/* mappings map elements to data, which
 * follows the mapping struct in memory. */
typedef struct BMOElemMapping {
	BMHeader *element;
	int len;
} BMOElemMapping;

extern const int BMO_OPSLOT_TYPEINFO[BMO_OP_SLOT_TOTAL_TYPES];

#ifdef __cplusplus
}
#endif

#endif /* __BMESH_OPERATOR_API_H__ */
