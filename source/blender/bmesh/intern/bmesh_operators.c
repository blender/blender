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
 * Contributor(s): Joseph Eagar, Geoffrey Bantle, Campbell Barton
 *
 * ***** END GPL LICENSE BLOCK *****
 */

/** \file blender/bmesh/intern/bmesh_operators.c
 *  \ingroup bmesh
 *
 * BMesh operator access.
 */

#include "MEM_guardedalloc.h"

#include "BLI_utildefines.h"
#include "BLI_string.h"
#include "BLI_math.h"
#include "BLI_memarena.h"
#include "BLI_mempool.h"
#include "BLI_listbase.h"
#include "BLI_array.h"

#include "BLF_translation.h"

#include "bmesh.h"
#include "intern/bmesh_private.h"

/* forward declarations */
static void bmo_flag_layer_alloc(BMesh *bm);
static void bmo_flag_layer_free(BMesh *bm);
static void bmo_flag_layer_clear(BMesh *bm);
static int bmo_name_to_slotcode(BMOpSlot slot_args[BMO_OP_MAX_SLOTS], const char *identifier);
static int bmo_name_to_slotcode_check(BMOpSlot slot_args[BMO_OP_MAX_SLOTS], const char *identifier);
static int bmo_opname_to_opcode(const char *opname);

static const char *bmo_error_messages[] = {
	NULL,
	N_("Self intersection error"),
	N_("Could not dissolve vert"),
	N_("Could not connect vertices"),
	N_("Could not traverse mesh"),
	N_("Could not dissolve faces"),
	N_("Could not dissolve vertices"),
	N_("Tessellation error"),
	N_("Cannot deal with non-manifold geometry"),
	N_("Invalid selection"),
	N_("Internal mesh error"),
};


/* operator slot type information - size of one element of the type given. */
const int BMO_OPSLOT_TYPEINFO[BMO_OP_SLOT_TOTAL_TYPES] = {
	0,                      /*  0: BMO_OP_SLOT_SENTINEL */
	sizeof(int),            /*  1: BMO_OP_SLOT_BOOL */ 
	sizeof(int),            /*  2: BMO_OP_SLOT_INT */ 
	sizeof(float),          /*  3: BMO_OP_SLOT_FLT */ 
	sizeof(void *),         /*  4: BMO_OP_SLOT_PNT */ 
	sizeof(void *),         /*  5: BMO_OP_SLOT_PNT */
	0,                      /*  6: unused */
	0,                      /*  7: unused */
	sizeof(float) * 3,      /*  8: BMO_OP_SLOT_VEC */
	sizeof(void *),	        /*  9: BMO_OP_SLOT_ELEMENT_BUF */
	sizeof(BMOElemMapping)  /* 10: BMO_OP_SLOT_MAPPING */
};

/* Dummy slot so there is something to return when slot name lookup fails */
// static BMOpSlot BMOpEmptySlot = {0};

void BMO_op_flag_enable(BMesh *UNUSED(bm), BMOperator *op, const int op_flag)
{
	op->flag |= op_flag;
}

void BMO_op_flag_disable(BMesh *UNUSED(bm), BMOperator *op, const int op_flag)
{
	op->flag &= ~op_flag;
}

/**
 * \brief BMESH OPSTACK PUSH
 *
 * Pushes the opstack down one level and allocates a new flag layer if appropriate.
 */
void BMO_push(BMesh *bm, BMOperator *UNUSED(op))
{
	bm->stackdepth++;

	BLI_assert(bm->totflags > 0);

	/* add flag layer, if appropriate */
	if (bm->stackdepth > 1)
		bmo_flag_layer_alloc(bm);
	else
		bmo_flag_layer_clear(bm);
}

/**
 * \brief BMESH OPSTACK POP
 *
 * Pops the opstack one level and frees a flag layer if appropriate
 *
 * BMESH_TODO: investigate NOT freeing flag layers.
 */
void BMO_pop(BMesh *bm)
{
	if (bm->stackdepth > 1)
		bmo_flag_layer_free(bm);

	bm->stackdepth--;
}


/* use for both slot_types_in and slot_types_out */
static void bmo_op_slots_init(const BMOSlotType *slot_types, BMOpSlot *slot_args)
{
	unsigned int i;
	for (i = 0; slot_types[i].type; i++) {
		slot_args[i].slot_name    = slot_types[i].name;
		slot_args[i].slot_type    = slot_types[i].type;
		slot_args[i].slot_subtype = slot_types[i].subtype;
		// slot_args[i].index = i;  // UNUSED
	}
}

/**
 * \brief BMESH OPSTACK INIT OP
 *
 * Initializes an operator structure to a certain type
 */
void BMO_op_init(BMesh *bm, BMOperator *op, const int flag, const char *opname)
{
	int opcode = bmo_opname_to_opcode(opname);

#ifdef DEBUG
	BM_ELEM_INDEX_VALIDATE(bm, "pre bmo", opname);
#else
	(void)bm;
#endif

	if (opcode == -1) {
		opcode = 0; /* error!, already printed, have a better way to handle this? */
	}

	memset(op, 0, sizeof(BMOperator));
	op->type = opcode;
	op->type_flag = bmo_opdefines[opcode]->type_flag;
	op->flag = flag;
	
	/* initialize the operator slot types */
	bmo_op_slots_init(bmo_opdefines[opcode]->slot_types_in,  op->slots_in);
	bmo_op_slots_init(bmo_opdefines[opcode]->slot_types_out, op->slots_out);

	/* callback */
	op->exec = bmo_opdefines[opcode]->exec;

	/* memarena, used for operator's slot buffers */
	op->arena = BLI_memarena_new(BLI_MEMARENA_STD_BUFSIZE, __func__);
	BLI_memarena_use_calloc(op->arena);
}

/**
 * \brief BMESH OPSTACK EXEC OP
 *
 * Executes a passed in operator.
 *
 * This handles the allocation and freeing of temporary flag
 * layers and starting/stopping the modeling loop.
 * Can be called from other operators exec callbacks as well.
 */
void BMO_op_exec(BMesh *bm, BMOperator *op)
{
	/* allocate tool flags on demand */
	BM_mesh_elem_toolflags_ensure(bm);

	BMO_push(bm, op);

	if (bm->stackdepth == 2)
		bmesh_edit_begin(bm, op->type_flag);
	op->exec(bm, op);
	
	if (bm->stackdepth == 2)
		bmesh_edit_end(bm, op->type_flag);
	
	BMO_pop(bm);
}

static void bmo_op_slots_free(const BMOSlotType *slot_types, BMOpSlot *slot_args)
{
	BMOpSlot *slot;
	unsigned int i;
	for (i = 0; slot_types[i].type; i++) {
		slot = &slot_args[i];
		if (slot->slot_type == BMO_OP_SLOT_MAPPING) {
			if (slot->data.ghash) {
				BLI_ghash_free(slot->data.ghash, NULL, NULL);
			}
		}
	}
}

/**
 * \brief BMESH OPSTACK FINISH OP
 *
 * Does housekeeping chores related to finishing up an operator.
 */
void BMO_op_finish(BMesh *bm, BMOperator *op)
{
	bmo_op_slots_free(bmo_opdefines[op->type]->slot_types_in,  op->slots_in);
	bmo_op_slots_free(bmo_opdefines[op->type]->slot_types_out, op->slots_out);

	BLI_memarena_free(op->arena);

#ifdef DEBUG
	BM_ELEM_INDEX_VALIDATE(bm, "post bmo", bmo_opdefines[op->type]->opname);
#else
	(void)bm;
#endif
}

/**
 * \brief BMESH OPSTACK HAS SLOT
 *
 * \return Success if the slot if found.
 */
int BMO_slot_exists(BMOpSlot slot_args[BMO_OP_MAX_SLOTS], const char *identifier)
{
	int slot_code = bmo_name_to_slotcode(slot_args, identifier);
	return (slot_code >= 0);
}

/**
 * \brief BMESH OPSTACK GET SLOT
 *
 * Returns a pointer to the slot of type 'slot_code'
 */
BMOpSlot *BMO_slot_get(BMOpSlot slot_args[BMO_OP_MAX_SLOTS], const char *identifier)
{
	int slot_code = bmo_name_to_slotcode_check(slot_args, identifier);

	if (UNLIKELY(slot_code < 0)) {
		//return &BMOpEmptySlot;
		BLI_assert(0);
		NULL;  /* better crash */
	}

	return &slot_args[slot_code];
}

/**
 * \brief BMESH OPSTACK COPY SLOT
 *
 * define used.
 * Copies data from one slot to another.
 */
void _bmo_slot_copy(BMOpSlot slot_args_src[BMO_OP_MAX_SLOTS], const char *slot_name_src,
                    BMOpSlot slot_args_dst[BMO_OP_MAX_SLOTS], const char *slot_name_dst,
                    struct MemArena *arena_dst)
{
	BMOpSlot *slot_src = BMO_slot_get(slot_args_src, slot_name_src);
	BMOpSlot *slot_dst = BMO_slot_get(slot_args_dst, slot_name_dst);

	if (slot_src == slot_dst)
		return;

	BLI_assert(slot_src->slot_type == slot_dst->slot_type);
	if (slot_src->slot_type != slot_dst->slot_type) {
		return;
	}

	if (slot_dst->slot_type == BMO_OP_SLOT_ELEMENT_BUF) {
		/* do buffer copy */
		slot_dst->data.buf = NULL;
		slot_dst->len = slot_src->len;
		if (slot_dst->len) {
			/* check dest has all flags enabled that the source has */
			const eBMOpSlotSubType_Elem src_elem_flag = (slot_src->slot_subtype.elem & BM_ALL_NOLOOP);
			const eBMOpSlotSubType_Elem dst_elem_flag = (slot_dst->slot_subtype.elem & BM_ALL_NOLOOP);

			if ((src_elem_flag | dst_elem_flag) == dst_elem_flag) {
				/* pass */
			}
			else {
				/* check types */
				const unsigned int tot = slot_src->len;
				unsigned int i;
				unsigned int out = 0;
				BMElem **ele_src = (BMElem **)slot_src->data.buf;
				for (i = 0; i < tot; i++, ele_src++) {
					if ((*ele_src)->head.htype & dst_elem_flag) {
						out++;
					}
				}
				if (out != tot) {
					slot_dst->len = out;
				}
			}

			if (slot_dst->len) {
				const int slot_alloc_size = BMO_OPSLOT_TYPEINFO[slot_dst->slot_type] * slot_dst->len;
				slot_dst->data.buf = BLI_memarena_alloc(arena_dst, slot_alloc_size);
				if (slot_src->len == slot_dst->len) {
					memcpy(slot_dst->data.buf, slot_src->data.buf, slot_alloc_size);
				}
				else {
					/* only copy compatible elements */
					const unsigned int tot = slot_src->len;
					unsigned int i;
					BMElem **ele_src = (BMElem **)slot_src->data.buf;
					BMElem **ele_dst = (BMElem **)slot_dst->data.buf;
					for (i = 0; i < tot; i++, ele_src++) {
						if ((*ele_src)->head.htype & dst_elem_flag) {
							*ele_dst = *ele_src;
							ele_dst++;
						}
					}
				}
			}
		}
	}
	else if (slot_dst->slot_type == BMO_OP_SLOT_MAPPING) {
		GHashIterator it;
		BMOElemMapping *srcmap, *dstmap;

		/* sanity check */
		if (!slot_src->data.ghash) {
			return;
		}

		if (!slot_dst->data.ghash) {
			slot_dst->data.ghash = BLI_ghash_ptr_new("bmesh operator 2");
		}

		for (BLI_ghashIterator_init(&it, slot_src->data.ghash);
		     (srcmap = BLI_ghashIterator_getValue(&it));
		     BLI_ghashIterator_step(&it))
		{
			dstmap = BLI_memarena_alloc(arena_dst, sizeof(*dstmap) + srcmap->len);

			dstmap->element = srcmap->element;
			dstmap->len = srcmap->len;
			memcpy(BMO_OP_SLOT_MAPPING_DATA(dstmap), BMO_OP_SLOT_MAPPING_DATA(srcmap), srcmap->len);

			BLI_ghash_insert(slot_dst->data.ghash, dstmap->element, dstmap);
		}
	}
	else {
		slot_dst->data = slot_src->data;
	}
}

/*
 * BMESH OPSTACK SET XXX
 *
 * Sets the value of a slot depending on it's type
 */

void BMO_slot_float_set(BMOpSlot slot_args[BMO_OP_MAX_SLOTS], const char *slot_name, const float f)
{
	BMOpSlot *slot = BMO_slot_get(slot_args, slot_name);
	BLI_assert(slot->slot_type == BMO_OP_SLOT_FLT);
	if (!(slot->slot_type == BMO_OP_SLOT_FLT))
		return;

	slot->data.f = f;
}

void BMO_slot_int_set(BMOpSlot slot_args[BMO_OP_MAX_SLOTS], const char *slot_name, const int i)
{
	BMOpSlot *slot = BMO_slot_get(slot_args, slot_name);
	BLI_assert(slot->slot_type == BMO_OP_SLOT_INT);
	if (!(slot->slot_type == BMO_OP_SLOT_INT))
		return;

	slot->data.i = i;
}

void BMO_slot_bool_set(BMOpSlot slot_args[BMO_OP_MAX_SLOTS], const char *slot_name, const int i)
{
	BMOpSlot *slot = BMO_slot_get(slot_args, slot_name);
	BLI_assert(slot->slot_type == BMO_OP_SLOT_BOOL);
	if (!(slot->slot_type == BMO_OP_SLOT_BOOL))
		return;

	slot->data.i = i;
}

/* only supports square mats */
void BMO_slot_mat_set(BMOperator *op, BMOpSlot slot_args[BMO_OP_MAX_SLOTS], const char *slot_name, const float *mat, int size)
{
	BMOpSlot *slot = BMO_slot_get(slot_args, slot_name);
	BLI_assert(slot->slot_type == BMO_OP_SLOT_MAT);
	if (!(slot->slot_type == BMO_OP_SLOT_MAT))
		return;

	slot->len = 4;
	slot->data.p = BLI_memarena_alloc(op->arena, sizeof(float) * 4 * 4);
	
	if (size == 4) {
		memcpy(slot->data.p, mat, sizeof(float) * 4 * 4);
	}
	else if (size == 3) {
		copy_m4_m3(slot->data.p, (float (*)[3])mat);
	}
	else {
		fprintf(stderr, "%s: invalid size argument %d (bmesh internal error)\n", __func__, size);

		zero_m4(slot->data.p);
	}
}

void BMO_slot_mat4_get(BMOpSlot slot_args[BMO_OP_MAX_SLOTS], const char *slot_name, float r_mat[4][4])
{
	BMOpSlot *slot = BMO_slot_get(slot_args, slot_name);
	BLI_assert(slot->slot_type == BMO_OP_SLOT_MAT);
	if (!(slot->slot_type == BMO_OP_SLOT_MAT))
		return;

	if (slot->data.p) {
		copy_m4_m4(r_mat, BMO_SLOT_AS_MATRIX(slot));
	}
	else {
		unit_m4(r_mat);
	}
}

void BMO_slot_mat3_get(BMOpSlot slot_args[BMO_OP_MAX_SLOTS], const char *slot_name, float r_mat[3][3])
{
	BMOpSlot *slot = BMO_slot_get(slot_args, slot_name);
	BLI_assert(slot->slot_type == BMO_OP_SLOT_MAT);
	if (!(slot->slot_type == BMO_OP_SLOT_MAT))
		return;

	if (slot->data.p) {
		copy_m3_m4(r_mat, BMO_SLOT_AS_MATRIX(slot));
	}
	else {
		unit_m3(r_mat);
	}
}

void BMO_slot_ptr_set(BMOpSlot slot_args[BMO_OP_MAX_SLOTS], const char *slot_name, void *p)
{
	BMOpSlot *slot = BMO_slot_get(slot_args, slot_name);
	BLI_assert(slot->slot_type == BMO_OP_SLOT_PTR);
	if (!(slot->slot_type == BMO_OP_SLOT_PTR))
		return;

	slot->data.p = p;
}

void BMO_slot_vec_set(BMOpSlot slot_args[BMO_OP_MAX_SLOTS], const char *slot_name, const float vec[3])
{
	BMOpSlot *slot = BMO_slot_get(slot_args, slot_name);
	BLI_assert(slot->slot_type == BMO_OP_SLOT_VEC);
	if (!(slot->slot_type == BMO_OP_SLOT_VEC))
		return;

	copy_v3_v3(slot->data.vec, vec);
}


float BMO_slot_float_get(BMOpSlot slot_args[BMO_OP_MAX_SLOTS], const char *slot_name)
{
	BMOpSlot *slot = BMO_slot_get(slot_args, slot_name);
	BLI_assert(slot->slot_type == BMO_OP_SLOT_FLT);
	if (!(slot->slot_type == BMO_OP_SLOT_FLT))
		return 0.0f;

	return slot->data.f;
}

int BMO_slot_int_get(BMOpSlot slot_args[BMO_OP_MAX_SLOTS], const char *slot_name)
{
	BMOpSlot *slot = BMO_slot_get(slot_args, slot_name);
	BLI_assert(slot->slot_type == BMO_OP_SLOT_INT);
	if (!(slot->slot_type == BMO_OP_SLOT_INT))
		return 0;

	return slot->data.i;
}

int BMO_slot_bool_get(BMOpSlot slot_args[BMO_OP_MAX_SLOTS], const char *slot_name)
{
	BMOpSlot *slot = BMO_slot_get(slot_args, slot_name);
	BLI_assert(slot->slot_type == BMO_OP_SLOT_BOOL);
	if (!(slot->slot_type == BMO_OP_SLOT_BOOL))
		return 0;

	return slot->data.i;
}

/* if you want a copy of the elem buffer */
void *BMO_slot_as_arrayN(BMOpSlot slot_args[BMO_OP_MAX_SLOTS], const char *slot_name, int *len)
{
	BMOpSlot *slot = BMO_slot_get(slot_args, slot_name);
	void *ret;

	/* could add support for mapping type */
	BLI_assert(slot->slot_type == BMO_OP_SLOT_ELEMENT_BUF);

	ret = MEM_mallocN(sizeof(void **) * slot->len, __func__);
	memcpy(ret, slot->data.buf, sizeof(void **) * slot->len);
	*len = slot->len;
	return ret;
}

void *BMO_slot_ptr_get(BMOpSlot slot_args[BMO_OP_MAX_SLOTS], const char *slot_name)
{
	BMOpSlot *slot = BMO_slot_get(slot_args, slot_name);
	BLI_assert(slot->slot_type == BMO_OP_SLOT_PTR);
	if (!(slot->slot_type == BMO_OP_SLOT_PTR))
		return NULL;

	return slot->data.p;
}

void BMO_slot_vec_get(BMOpSlot slot_args[BMO_OP_MAX_SLOTS], const char *slot_name, float r_vec[3])
{
	BMOpSlot *slot = BMO_slot_get(slot_args, slot_name);
	BLI_assert(slot->slot_type == BMO_OP_SLOT_VEC);
	if (!(slot->slot_type == BMO_OP_SLOT_VEC))
		return;

	copy_v3_v3(r_vec, slot->data.vec);
}

/*
 * BMO_COUNTFLAG
 *
 * Counts the number of elements of a certain type that have a
 * specific flag enabled (or disabled if test_for_enabled is false).
 *
 */

static int bmo_mesh_flag_count(BMesh *bm, const char htype, const short oflag,
                               const short test_for_enabled)
{
	const char iter_types[3] = {BM_VERTS_OF_MESH,
	                            BM_EDGES_OF_MESH,
	                            BM_FACES_OF_MESH};

	const char flag_types[3] = {BM_VERT, BM_EDGE, BM_FACE};

	BMIter iter;
	int count = 0;
	BMElemF *ele_f;
	int i;

	BLI_assert(ELEM(TRUE, FALSE, test_for_enabled));

	for (i = 0; i < 3; i++) {
		if (htype & flag_types[i]) {
			BM_ITER_MESH (ele_f, &iter, bm, iter_types[i]) {
				if (BMO_elem_flag_test_bool(bm, ele_f, oflag) == test_for_enabled)
					count++;
			}
		}
	}

	return count;
}


int BMO_mesh_enabled_flag_count(BMesh *bm, const char htype, const short oflag)
{
	return bmo_mesh_flag_count(bm, htype, oflag, TRUE);
}

int BMO_mesh_disabled_flag_count(BMesh *bm, const char htype, const short oflag)
{
	return bmo_mesh_flag_count(bm, htype, oflag, FALSE);
}

void BMO_mesh_flag_disable_all(BMesh *bm, BMOperator *UNUSED(op), const char htype, const short oflag)
{
	const char iter_types[3] = {BM_VERTS_OF_MESH,
	                            BM_EDGES_OF_MESH,
	                            BM_FACES_OF_MESH};

	const char flag_types[3] = {BM_VERT, BM_EDGE, BM_FACE};

	BMIter iter;
	BMElemF *ele;
	int i;

	for (i = 0; i < 3; i++) {
		if (htype & flag_types[i]) {
			BM_ITER_MESH (ele, &iter, bm, iter_types[i]) {
				BMO_elem_flag_disable(bm, ele, oflag);
			}
		}
	}
}

int BMO_slot_buffer_count(BMOpSlot slot_args[BMO_OP_MAX_SLOTS], const char *slot_name)
{
	BMOpSlot *slot = BMO_slot_get(slot_args, slot_name);
	BLI_assert(slot->slot_type == BMO_OP_SLOT_ELEMENT_BUF);
	
	/* check if its actually a buffer */
	if (slot->slot_type != BMO_OP_SLOT_ELEMENT_BUF)
		return 0;

	return slot->len;
}

int BMO_slot_map_count(BMOpSlot slot_args[BMO_OP_MAX_SLOTS], const char *slot_name)
{
	BMOpSlot *slot = BMO_slot_get(slot_args, slot_name);
	BLI_assert(slot->slot_type == BMO_OP_SLOT_MAPPING);
	
	/* check if its actually a buffer */
	if (!(slot->slot_type == BMO_OP_SLOT_MAPPING))
		return 0;

	return slot->data.ghash ? BLI_ghash_size(slot->data.ghash) : 0;
}

/* inserts a key/value mapping into a mapping slot.  note that it copies the
 * value, it doesn't store a reference to it. */

void BMO_slot_map_insert(BMOperator *op, BMOpSlot *slot,
                         const void *element, const void *data, const int len)
{
	BMOElemMapping *mapping;
	BLI_assert(slot->slot_type == BMO_OP_SLOT_MAPPING);
	BMO_ASSERT_SLOT_IN_OP(slot, op);

	mapping = (BMOElemMapping *) BLI_memarena_alloc(op->arena, sizeof(*mapping) + len);

	mapping->element = (BMHeader *) element;
	mapping->len = len;
	memcpy(BMO_OP_SLOT_MAPPING_DATA(mapping), data, len);

	if (!slot->data.ghash) {
		slot->data.ghash = BLI_ghash_ptr_new("bmesh slot map hash");
	}
	else {
		BLI_assert(slot->data.ghash);
	}

	BLI_ghash_insert(slot->data.ghash, (void *)element, mapping);
}

#if 0
void *bmo_slot_buffer_grow(BMesh *bm, BMOperator *op, int slot_code, int totadd)
{
	BMOpSlot *slot = &op->slots[slot_code];
	void *tmp;
	ssize_t allocsize;
	
	BLI_assert(slot->slottype == BMO_OP_SLOT_ELEMENT_BUF);

	/* check if its actually a buffer */
	if (slot->slottype != BMO_OP_SLOT_ELEMENT_BUF)
		return NULL;

	if (slot->flag & BMOS_DYNAMIC_ARRAY) {
		if (slot->len >= slot->size) {
			slot->size = (slot->size + 1 + totadd) * 2;

			allocsize = BMO_OPSLOT_TYPEINFO[bmo_opdefines[op->type]->slot_types[slot_code].type] * slot->size;

			tmp = slot->data.buf;
			slot->data.buf = MEM_callocN(allocsize, "opslot dynamic array");
			memcpy(slot->data.buf, tmp, allocsize);
			MEM_freeN(tmp);
		}

		slot->len += totadd;
	}
	else {
		slot->flag |= BMOS_DYNAMIC_ARRAY;
		slot->len += totadd;
		slot->size = slot->len + 2;

		allocsize = BMO_OPSLOT_TYPEINFO[bmo_opdefines[op->type]->slot_types[slot_code].type] * slot->len;

		tmp = slot->data.buf;
		slot->data.buf = MEM_callocN(allocsize, "opslot dynamic array");
		memcpy(slot->data.buf, tmp, allocsize);
	}

	return slot->data.buf;
}
#endif

void BMO_slot_map_to_flag(BMesh *bm, BMOpSlot slot_args[BMO_OP_MAX_SLOTS], const char *slot_name,
                          const char htype, const short oflag)
{
	GHashIterator it;
	BMOpSlot *slot = BMO_slot_get(slot_args, slot_name);
	BMElemF *ele_f;

	BLI_assert(slot->slot_type == BMO_OP_SLOT_MAPPING);

	/* sanity check */
	if (!slot->data.ghash) return;

	BLI_ghashIterator_init(&it, slot->data.ghash);
	for ( ; (ele_f = BLI_ghashIterator_getKey(&it)); BLI_ghashIterator_step(&it)) {
		if (ele_f->head.htype & htype) {
			BMO_elem_flag_enable(bm, ele_f, oflag);
		}
	}
}

void *BMO_slot_buffer_alloc(BMOperator *op, BMOpSlot slot_args[BMO_OP_MAX_SLOTS], const char *slot_name, const int len)
{
	BMOpSlot *slot = BMO_slot_get(slot_args, slot_name);

	/* check if its actually a buffer */
	if (slot->slot_type != BMO_OP_SLOT_ELEMENT_BUF)
		return NULL;
	
	slot->len = len;
	if (len) {
		slot->data.buf = BLI_memarena_alloc(op->arena, BMO_OPSLOT_TYPEINFO[slot->slot_type] * len);
	}
	else {
		slot->data.buf = NULL;
	}

	return slot->data.buf;
}

/**
 * \brief BMO_ALL_TO_SLOT
 *
 * Copies all elements of a certain type into an operator slot.
 */
void BMO_slot_buffer_from_all(BMesh *bm, BMOperator *op, BMOpSlot slot_args[BMO_OP_MAX_SLOTS],
                              const char *slot_name, const char htype)
{
	BMOpSlot *output = BMO_slot_get(slot_args, slot_name);
	int totelement = 0, i = 0;
	
	if (htype & BM_VERT) totelement += bm->totvert;
	if (htype & BM_EDGE) totelement += bm->totedge;
	if (htype & BM_FACE) totelement += bm->totface;

	if (totelement) {
		BMIter iter;
		BMHeader *ele;

		BMO_slot_buffer_alloc(op, slot_args, slot_name, totelement);

		/* TODO - collapse these loops into one */

		if (htype & BM_VERT) {
			BM_ITER_MESH (ele, &iter, bm, BM_VERTS_OF_MESH) {
				output->data.buf[i] = ele;
				i++;
			}
		}

		if (htype & BM_EDGE) {
			BM_ITER_MESH (ele, &iter, bm, BM_EDGES_OF_MESH) {
				output->data.buf[i] = ele;
				i++;
			}
		}

		if (htype & BM_FACE) {
			BM_ITER_MESH (ele, &iter, bm, BM_FACES_OF_MESH) {
				output->data.buf[i] = ele;
				i++;
			}
		}
	}
}

/**
 * \brief BMO_HEADERFLAG_TO_SLOT
 *
 * Copies elements of a certain type, which have a certain header flag
 * enabled/disabled into a slot for an operator.
 */
static void bmo_slot_buffer_from_hflag(BMesh *bm, BMOperator *op, BMOpSlot slot_args[BMO_OP_MAX_SLOTS], const char *slot_name,
                                       const char htype, const char hflag,
                                       const short test_for_enabled)
{
	BMOpSlot *output = BMO_slot_get(slot_args, slot_name);
	int totelement = 0, i = 0;
	const int respecthide = (op->flag & BMO_FLAG_RESPECT_HIDE) != 0;

	BLI_assert(ELEM(test_for_enabled, TRUE, FALSE));

	if (test_for_enabled)
		totelement = BM_mesh_elem_hflag_count_enabled(bm, htype, hflag, respecthide);
	else
		totelement = BM_mesh_elem_hflag_count_disabled(bm, htype, hflag, respecthide);

	if (totelement) {
		BMIter iter;
		BMElem *ele;

		BMO_slot_buffer_alloc(op, slot_args, slot_name, totelement);

		/* TODO - collapse these loops into one */

		if (htype & BM_VERT) {
			BM_ITER_MESH (ele, &iter, bm, BM_VERTS_OF_MESH) {
				if ((!respecthide || !BM_elem_flag_test(ele, BM_ELEM_HIDDEN)) &&
				    BM_elem_flag_test_bool(ele, hflag) == test_for_enabled)
				{
					output->data.buf[i] = ele;
					i++;
				}
			}
		}

		if (htype & BM_EDGE) {
			BM_ITER_MESH (ele, &iter, bm, BM_EDGES_OF_MESH) {
				if ((!respecthide || !BM_elem_flag_test(ele, BM_ELEM_HIDDEN)) &&
				    BM_elem_flag_test_bool(ele, hflag) == test_for_enabled)
				{
					output->data.buf[i] = ele;
					i++;
				}
			}
		}

		if (htype & BM_FACE) {
			BM_ITER_MESH (ele, &iter, bm, BM_FACES_OF_MESH) {
				if ((!respecthide || !BM_elem_flag_test(ele, BM_ELEM_HIDDEN)) &&
				    BM_elem_flag_test_bool(ele, hflag) == test_for_enabled)
				{
					output->data.buf[i] = ele;
					i++;
				}
			}
		}
	}
	else {
		output->len = 0;
	}
}

void BMO_slot_buffer_from_enabled_hflag(BMesh *bm, BMOperator *op,
                                        BMOpSlot slot_args[BMO_OP_MAX_SLOTS], const char *slot_name,
                                        const char htype, const char hflag)
{
	bmo_slot_buffer_from_hflag(bm, op, slot_args, slot_name, htype, hflag, TRUE);
}

void BMO_slot_buffer_from_disabled_hflag(BMesh *bm, BMOperator *op,
                                         BMOpSlot slot_args[BMO_OP_MAX_SLOTS], const char *slot_name,
                                         const char htype, const char hflag)
{
	bmo_slot_buffer_from_hflag(bm, op, slot_args, slot_name, htype, hflag, FALSE);
}

void BMO_slot_buffer_from_single(BMOperator *op, BMOpSlot *slot, BMHeader *ele)
{
	BMO_ASSERT_SLOT_IN_OP(slot, op);
	BLI_assert(slot->slot_type == BMO_OP_SLOT_ELEMENT_BUF);
	BLI_assert(slot->slot_subtype.elem & BMO_OP_SLOT_SUBTYPE_ELEM_IS_SINGLE);
	BLI_assert(slot->len == 0 || slot->len == 1);

	BLI_assert(slot->slot_subtype.elem & ele->htype);

	slot->data.buf = BLI_memarena_alloc(op->arena, sizeof(void *) * 4);  /* XXX, why 'x4' ? */
	slot->len = 1;
	*slot->data.buf = ele;
}

void *BMO_slot_buffer_get_single(BMOpSlot *slot)
{
	BLI_assert(slot->slot_type == BMO_OP_SLOT_ELEMENT_BUF);
	BLI_assert(slot->slot_subtype.elem & BMO_OP_SLOT_SUBTYPE_ELEM_IS_SINGLE);
	BLI_assert(slot->len == 0 || slot->len == 1);

	return slot->len ? (BMHeader *)slot->data.buf[0] : NULL;
}

/**
 * Copies the values from another slot to the end of the output slot.
 */
void _bmo_slot_buffer_append(BMOpSlot slot_args_dst[BMO_OP_MAX_SLOTS], const char *slot_name_dst,
                             BMOpSlot slot_args_src[BMO_OP_MAX_SLOTS], const char *slot_name_src,
                             struct MemArena *arena_dst)
{
	BMOpSlot *slot_dst = BMO_slot_get(slot_args_dst, slot_name_dst);
	BMOpSlot *slot_src  = BMO_slot_get(slot_args_src,  slot_name_src);

	BLI_assert(slot_dst->slot_type == BMO_OP_SLOT_ELEMENT_BUF &&
	           slot_src->slot_type == BMO_OP_SLOT_ELEMENT_BUF);

	if (slot_dst->len == 0) {
		/* output slot is empty, copy rather than append */
		_bmo_slot_copy(slot_args_src, slot_name_src,
		               slot_args_dst, slot_name_dst,
		               arena_dst);
	}
	else if (slot_src->len != 0) {
		int elem_size = BMO_OPSLOT_TYPEINFO[slot_dst->slot_type];
		int alloc_size = elem_size * (slot_dst->len + slot_src->len);
		/* allocate new buffer */
		void *buf = BLI_memarena_alloc(arena_dst, alloc_size);

		/* copy slot data */
		memcpy(buf, slot_dst->data.buf, elem_size * slot_dst->len);
		memcpy(((char *)buf) + elem_size * slot_dst->len, slot_src->data.buf, elem_size * slot_src->len);

		slot_dst->data.buf = buf;
		slot_dst->len += slot_src->len;
	}
}

/**
 * \brief BMO_FLAG_TO_SLOT
 *
 * Copies elements of a certain type, which have a certain flag set
 * into an output slot for an operator.
 */
static void bmo_slot_buffer_from_flag(BMesh *bm, BMOperator *op,
                                      BMOpSlot slot_args[BMO_OP_MAX_SLOTS], const char *slot_name,
                                      const char htype, const short oflag,
                                      const short test_for_enabled)
{
	BMOpSlot *slot = BMO_slot_get(slot_args, slot_name);
	int totelement, i = 0;

	BLI_assert(op->slots_in == slot_args || op->slots_out == slot_args);
	BLI_assert(ELEM(TRUE, FALSE, test_for_enabled));

	if (test_for_enabled)
		totelement = BMO_mesh_enabled_flag_count(bm, htype, oflag);
	else
		totelement = BMO_mesh_disabled_flag_count(bm, htype, oflag);

	BLI_assert(slot->slot_type == BMO_OP_SLOT_ELEMENT_BUF);
	BLI_assert(((slot->slot_subtype.elem & BM_ALL_NOLOOP) & htype) == htype);

	if (totelement) {
		BMIter iter;
		BMHeader *ele;
		BMHeader **ele_array;

		BMO_slot_buffer_alloc(op, slot_args, slot_name, totelement);

		ele_array = (BMHeader **)slot->data.buf;

		/* TODO - collapse these loops into one */

		if (htype & BM_VERT) {
			BM_ITER_MESH (ele, &iter, bm, BM_VERTS_OF_MESH) {
				if (BMO_elem_flag_test_bool(bm, (BMElemF *)ele, oflag) == test_for_enabled) {
					ele_array[i] = ele;
					i++;
				}
			}
		}

		if (htype & BM_EDGE) {
			BM_ITER_MESH (ele, &iter, bm, BM_EDGES_OF_MESH) {
				if (BMO_elem_flag_test_bool(bm, (BMElemF *)ele, oflag) == test_for_enabled) {
					ele_array[i] = ele;
					i++;
				}
			}
		}

		if (htype & BM_FACE) {
			BM_ITER_MESH (ele, &iter, bm, BM_FACES_OF_MESH) {
				if (BMO_elem_flag_test_bool(bm, (BMElemF *)ele, oflag) == test_for_enabled) {
					ele_array[i] = ele;
					i++;
				}
			}
		}
	}
	else {
		slot->len = 0;
	}
}

void BMO_slot_buffer_from_enabled_flag(BMesh *bm, BMOperator *op,
                                       BMOpSlot slot_args[BMO_OP_MAX_SLOTS], const char *slot_name,
                                       const char htype, const short oflag)
{
	bmo_slot_buffer_from_flag(bm, op, slot_args, slot_name, htype, oflag, TRUE);
}

void BMO_slot_buffer_from_disabled_flag(BMesh *bm, BMOperator *op,
                                        BMOpSlot slot_args[BMO_OP_MAX_SLOTS], const char *slot_name,
                                        const char htype, const short oflag)
{
	bmo_slot_buffer_from_flag(bm, op, slot_args, slot_name, htype, oflag, FALSE);
}

/**
 * \brief BMO_FLAG_BUFFER
 *
 * Header Flags elements in a slots buffer, automatically
 * using the selection API where appropriate.
 */
void BMO_slot_buffer_hflag_enable(BMesh *bm,
                                  BMOpSlot slot_args[BMO_OP_MAX_SLOTS], const char *slot_name,
                                  const char htype, const char hflag, const char do_flush)
{
	BMOpSlot *slot = BMO_slot_get(slot_args, slot_name);
	BMElem **data =  (BMElem **)slot->data.buf;
	int i;
	const char do_flush_select = (do_flush && (hflag & BM_ELEM_SELECT));
	const char do_flush_hide = (do_flush && (hflag & BM_ELEM_HIDDEN));

	BLI_assert(slot->slot_type == BMO_OP_SLOT_ELEMENT_BUF);
	BLI_assert(((slot->slot_subtype.elem & BM_ALL_NOLOOP) & htype) == htype);

	for (i = 0; i < slot->len; i++, data++) {
		if (!(htype & (*data)->head.htype))
			continue;

		if (do_flush_select) {
			BM_elem_select_set(bm, *data, TRUE);
		}

		if (do_flush_hide) {
			BM_elem_hide_set(bm, *data, FALSE);
		}

		BM_elem_flag_enable(*data, hflag);
	}
}

/**
 * \brief BMO_FLAG_BUFFER
 *
 * Removes flags from elements in a slots buffer, automatically
 * using the selection API where appropriate.
 */
void BMO_slot_buffer_hflag_disable(BMesh *bm,
                                   BMOpSlot slot_args[BMO_OP_MAX_SLOTS], const char *slot_name,
                                   const char htype, const char hflag, const char do_flush)
{
	BMOpSlot *slot = BMO_slot_get(slot_args, slot_name);
	BMElem **data =  (BMElem **)slot->data.buf;
	int i;
	const char do_flush_select = (do_flush && (hflag & BM_ELEM_SELECT));
	const char do_flush_hide = (do_flush && (hflag & BM_ELEM_HIDDEN));

	BLI_assert(slot->slot_type == BMO_OP_SLOT_ELEMENT_BUF);
	BLI_assert(((slot->slot_subtype.elem & BM_ALL_NOLOOP) & htype) == htype);

	for (i = 0; i < slot->len; i++, data++) {
		if (!(htype & (*data)->head.htype))
			continue;

		if (do_flush_select) {
			BM_elem_select_set(bm, *data, FALSE);
		}

		if (do_flush_hide) {
			BM_elem_hide_set(bm, *data, FALSE);
		}

		BM_elem_flag_disable(*data, hflag);
	}
}

int BMO_vert_edge_flags_count(BMesh *bm, BMVert *v, const short oflag)
{
	int count = 0;

	if (v->e) {
		BMEdge *curedge;
		const int len = bmesh_disk_count(v);
		int i;
		
		for (i = 0, curedge = v->e; i < len; i++) {
			if (BMO_elem_flag_test(bm, curedge, oflag))
				count++;
			curedge = bmesh_disk_edge_next(curedge, v);
		}
	}

	return count;
}

/**
 * \brief BMO_FLAG_BUFFER
 *
 * Flags elements in a slots buffer
 */
void BMO_slot_buffer_flag_enable(BMesh *bm,
                                 BMOpSlot slot_args[BMO_OP_MAX_SLOTS], const char *slot_name,
                                 const char htype, const short oflag)
{
	BMOpSlot *slot = BMO_slot_get(slot_args, slot_name);
	BMHeader **data =  slot->data.p;
	int i;

	BLI_assert(slot->slot_type == BMO_OP_SLOT_ELEMENT_BUF);
	BLI_assert(((slot->slot_subtype.elem & BM_ALL_NOLOOP) & htype) == htype);

	for (i = 0; i < slot->len; i++) {
		if (!(htype & data[i]->htype))
			continue;

		BMO_elem_flag_enable(bm, (BMElemF *)data[i], oflag);
	}
}

/**
 * \brief BMO_FLAG_BUFFER
 *
 * Removes flags from elements in a slots buffer
 */
void BMO_slot_buffer_flag_disable(BMesh *bm,
                                  BMOpSlot slot_args[BMO_OP_MAX_SLOTS], const char *slot_name,
                                  const char htype, const short oflag)
{
	BMOpSlot *slot = BMO_slot_get(slot_args, slot_name);
	BMHeader **data = (BMHeader **)slot->data.buf;
	int i;

	BLI_assert(slot->slot_type == BMO_OP_SLOT_ELEMENT_BUF);
	BLI_assert(((slot->slot_subtype.elem & BM_ALL_NOLOOP) & htype) == htype);

	for (i = 0; i < slot->len; i++) {
		if (!(htype & data[i]->htype))
			continue;

		BMO_elem_flag_disable(bm, (BMElemF *)data[i], oflag);
	}
}


/**
 * \brief ALLOC/FREE FLAG LAYER
 *
 * Used by operator stack to free/allocate
 * private flag data. This is allocated
 * using a mempool so the allocation/frees
 * should be quite fast.
 *
 * BMESH_TODO:
 * Investigate not freeing flag layers until
 * all operators have been executed. This would
 * save a lot of realloc potentially.
 */
static void bmo_flag_layer_alloc(BMesh *bm)
{
	BMElemF *ele;
	/* set the index values since we are looping over all data anyway,
	 * may save time later on */
	int i;

	BMIter iter;
	BLI_mempool *oldpool = bm->toolflagpool;  /* old flag pool */
	BLI_mempool *newpool;
	void *oldflags;

	/* store memcpy size for reuse */
	const size_t old_totflags_size = (bm->totflags * sizeof(BMFlagLayer));

	BLI_assert(oldpool != NULL);

	bm->totflags++;

	/* allocate new flag poo */
	bm->toolflagpool = newpool = BLI_mempool_create(sizeof(BMFlagLayer) * bm->totflags, 512, 512, 0);
	
	/* now go through and memcpy all the flags. Loops don't get a flag layer at this time.. */
	BM_ITER_MESH_INDEX (ele, &iter, bm, BM_VERTS_OF_MESH, i) {
		oldflags = ele->oflags;
		ele->oflags = BLI_mempool_calloc(newpool);
		memcpy(ele->oflags, oldflags, old_totflags_size);
		BM_elem_index_set(ele, i); /* set_inline */
		BM_ELEM_API_FLAG_CLEAR((BMElemF *)ele);
	}
	BM_ITER_MESH_INDEX (ele, &iter, bm, BM_EDGES_OF_MESH, i) {
		oldflags = ele->oflags;
		ele->oflags = BLI_mempool_calloc(newpool);
		memcpy(ele->oflags, oldflags, old_totflags_size);
		BM_elem_index_set(ele, i); /* set_inline */
		BM_ELEM_API_FLAG_CLEAR((BMElemF *)ele);
	}
	BM_ITER_MESH_INDEX (ele, &iter, bm, BM_FACES_OF_MESH, i) {
		oldflags = ele->oflags;
		ele->oflags = BLI_mempool_calloc(newpool);
		memcpy(ele->oflags, oldflags, old_totflags_size);
		BM_elem_index_set(ele, i); /* set_inline */
		BM_ELEM_API_FLAG_CLEAR((BMElemF *)ele);
	}

	bm->elem_index_dirty &= ~(BM_VERT | BM_EDGE | BM_FACE);

	BLI_mempool_destroy(oldpool);
}

static void bmo_flag_layer_free(BMesh *bm)
{
	BMElemF *ele;
	/* set the index values since we are looping over all data anyway,
	 * may save time later on */
	int i;

	BMIter iter;
	BLI_mempool *oldpool = bm->toolflagpool;
	BLI_mempool *newpool;
	void *oldflags;
	
	/* store memcpy size for reuse */
	const size_t new_totflags_size = ((bm->totflags - 1) * sizeof(BMFlagLayer));

	/* de-increment the totflags first.. */
	bm->totflags--;
	/* allocate new flag poo */
	bm->toolflagpool = newpool = BLI_mempool_create(new_totflags_size, 512, 512, BLI_MEMPOOL_SYSMALLOC);
	
	/* now go through and memcpy all the flag */
	BM_ITER_MESH_INDEX (ele, &iter, bm, BM_VERTS_OF_MESH, i) {
		oldflags = ele->oflags;
		ele->oflags = BLI_mempool_calloc(newpool);
		memcpy(ele->oflags, oldflags, new_totflags_size);
		BM_elem_index_set(ele, i); /* set_inline */
		BM_ELEM_API_FLAG_CLEAR((BMElemF *)ele);
	}
	BM_ITER_MESH_INDEX (ele, &iter, bm, BM_EDGES_OF_MESH, i) {
		oldflags = ele->oflags;
		ele->oflags = BLI_mempool_calloc(newpool);
		memcpy(ele->oflags, oldflags, new_totflags_size);
		BM_elem_index_set(ele, i); /* set_inline */
		BM_ELEM_API_FLAG_CLEAR((BMElemF *)ele);
	}
	BM_ITER_MESH_INDEX (ele, &iter, bm, BM_FACES_OF_MESH, i) {
		oldflags = ele->oflags;
		ele->oflags = BLI_mempool_calloc(newpool);
		memcpy(ele->oflags, oldflags, new_totflags_size);
		BM_elem_index_set(ele, i); /* set_inline */
		BM_ELEM_API_FLAG_CLEAR((BMElemF *)ele);
	}

	bm->elem_index_dirty &= ~(BM_VERT | BM_EDGE | BM_FACE);

	BLI_mempool_destroy(oldpool);
}

static void bmo_flag_layer_clear(BMesh *bm)
{
	BMElemF *ele;
	/* set the index values since we are looping over all data anyway,
	 * may save time later on */
	int i;

	BMIter iter;
	const int totflags_offset = bm->totflags - 1;

	/* now go through and memcpy all the flag */
	BM_ITER_MESH_INDEX (ele, &iter, bm, BM_VERTS_OF_MESH, i) {
		memset(ele->oflags + totflags_offset, 0, sizeof(BMFlagLayer));
		BM_elem_index_set(ele, i); /* set_inline */
	}
	BM_ITER_MESH_INDEX (ele, &iter, bm, BM_EDGES_OF_MESH, i) {
		memset(ele->oflags + totflags_offset, 0, sizeof(BMFlagLayer));
		BM_elem_index_set(ele, i); /* set_inline */
	}
	BM_ITER_MESH_INDEX (ele, &iter, bm, BM_FACES_OF_MESH, i) {
		memset(ele->oflags + totflags_offset, 0, sizeof(BMFlagLayer));
		BM_elem_index_set(ele, i); /* set_inline */
	}

	bm->elem_index_dirty &= ~(BM_VERT | BM_EDGE | BM_FACE);
}

void *BMO_slot_buffer_elem_first(BMOpSlot slot_args[BMO_OP_MAX_SLOTS], const char *slot_name)
{
	BMOpSlot *slot = BMO_slot_get(slot_args, slot_name);
	
	if (slot->slot_type != BMO_OP_SLOT_ELEMENT_BUF)
		return NULL;

	return slot->data.buf ? *slot->data.buf : NULL;
}

/**
 * \brief New Iterator
 *
 * \param restrictmask restricts the iteration to certain element types
 * (e.g. combination of BM_VERT, BM_EDGE, BM_FACE), if iterating
 * over an element buffer (not a mapping). */
void *BMO_iter_new(BMOIter *iter,
                   BMOpSlot slot_args[BMO_OP_MAX_SLOTS], const char *slot_name,
                   const char restrictmask)
{
	BMOpSlot *slot = BMO_slot_get(slot_args, slot_name);

	memset(iter, 0, sizeof(BMOIter));

	iter->slot = slot;
	iter->cur = 0;
	iter->restrictmask = restrictmask;

	if (iter->slot->slot_type == BMO_OP_SLOT_MAPPING) {
		if (iter->slot->data.ghash) {
			BLI_ghashIterator_init(&iter->giter, slot->data.ghash);
		}
		else {
			return NULL;
		}
	}

	return BMO_iter_step(iter);
}

void *BMO_iter_step(BMOIter *iter)
{
	BMOpSlot *slot = iter->slot;
	if (slot->slot_type == BMO_OP_SLOT_ELEMENT_BUF) {
		BMHeader *ele;

		if (iter->cur >= slot->len) {
			return NULL;
		}

		ele = slot->data.buf[iter->cur++];
		while (!(iter->restrictmask & ele->htype)) {
			if (iter->cur >= slot->len) {
				return NULL;
			}

			ele = slot->data.buf[iter->cur++];
			BLI_assert((ele == NULL) || (slot->slot_subtype.elem & ele->htype));
		}

		BLI_assert((ele == NULL) || (slot->slot_subtype.elem & ele->htype));

		return ele;
	}
	else if (slot->slot_type == BMO_OP_SLOT_MAPPING) {
		BMOElemMapping *map;
		void *ret = BLI_ghashIterator_getKey(&iter->giter);
		map = BLI_ghashIterator_getValue(&iter->giter);
		
		iter->val = BMO_OP_SLOT_MAPPING_DATA(map);

		BLI_ghashIterator_step(&iter->giter);

		return ret;
	}
	else {
		BLI_assert(0);
	}

	return NULL;
}

/* used for iterating over mapping */
void *BMO_iter_map_value(BMOIter *iter)
{
	return iter->val;
}

void *BMO_iter_map_value_p(BMOIter *iter)
{
	return *((void **)iter->val);
}

float BMO_iter_map_value_f(BMOIter *iter)
{
	return *((float *)iter->val);
}

/* error syste */
typedef struct BMOpError {
	struct BMOpError *next, *prev;
	int errorcode;
	BMOperator *op;
	const char *msg;
} BMOpError;

void BMO_error_clear(BMesh *bm)
{
	while (BMO_error_pop(bm, NULL, NULL));
}

void BMO_error_raise(BMesh *bm, BMOperator *owner, int errcode, const char *msg)
{
	BMOpError *err = MEM_callocN(sizeof(BMOpError), "bmop_error");
	
	err->errorcode = errcode;
	if (!msg) {
		msg = bmo_error_messages[errcode];
	}
	err->msg = msg;
	err->op = owner;
	
	BLI_addhead(&bm->errorstack, err);
}

int BMO_error_occurred(BMesh *bm)
{
	return bm->errorstack.first != NULL;
}

/* returns error code or 0 if no error */
int BMO_error_get(BMesh *bm, const char **msg, BMOperator **op)
{
	BMOpError *err = bm->errorstack.first;
	if (!err) {
		return 0;
	}

	if (msg) *msg = err->msg;
	if (op) *op = err->op;
	
	return err->errorcode;
}

int BMO_error_pop(BMesh *bm, const char **msg, BMOperator **op)
{
	int errorcode = BMO_error_get(bm, msg, op);
	
	if (errorcode) {
		BMOpError *err = bm->errorstack.first;
		
		BLI_remlink(&bm->errorstack, bm->errorstack.first);
		MEM_freeN(err);
	}

	return errorcode;
}


#define NEXT_CHAR(fmt) ((fmt)[0] != 0 ? (fmt)[1] : 0)

static int bmo_name_to_slotcode(BMOpSlot slot_args[BMO_OP_MAX_SLOTS], const char *identifier)
{
	int i = 0;

	while (slot_args->slot_name) {
		if (strncmp(identifier, slot_args->slot_name, MAX_SLOTNAME) == 0) {
			return i;
		}
		slot_args++;
		i++;
	}

	return -1;
}

static int bmo_name_to_slotcode_check(BMOpSlot slot_args[BMO_OP_MAX_SLOTS], const char *identifier)
{
	int i = bmo_name_to_slotcode(slot_args, identifier);
	if (i < 0) {
		fprintf(stderr, "%s: ! could not find bmesh slot for name %s! (bmesh internal error)\n", __func__, identifier);
	}

	return i;
}

static int bmo_opname_to_opcode(const char *opname)
{
	int i;

	for (i = 0; i < bmo_opdefines_total; i++) {
		if (!strcmp(opname, bmo_opdefines[i]->opname)) {
			return i;
		}
	}

	fprintf(stderr, "%s: could not find bmesh slot for name %s! (bmesh internal error)\n", __func__, opname);
	return -1;
}

/**
 * \brief Format Strings for #BMOperator Initialization.
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
 *
 *
 * **Primitive Types**
 * - `b` - boolean (same as int but 1/0 only). #BMO_OP_SLOT_BOOL
 * - `i` - int. #BMO_OP_SLOT_INT
 * - `f` - float. #BMO_OP_SLOT_FLT
 * - `p` - pointer (normally to a Scene/Mesh/Object/BMesh). #BMO_OP_SLOT_PTR
 * - `m3` - 3x3 matrix of floats. #BMO_OP_SLOT_MAT
 * - `m4` - 4x4 matrix of floats. #BMO_OP_SLOT_MAT
 * - `v` - 3D vector of floats. #BMO_OP_SLOT_VEC
 *
 *
 * **Utility**
 *
 * Pass an existing slot which is copied to either an input or output slot.
 * Taking the operator and slot-name pair of args.
 * - `s` - slot_in (lower case)
 * - `S` - slot_out (upper case)
 *
 *
 * **Element Buffer** (#BMO_OP_SLOT_ELEMENT_BUF)
 * - `e` - single element vert/edge/face (use with #BMO_OP_SLOT_SUBTYPE_ELEM_IS_SINGLE).
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
 * Order is not important so `Hfev` is also valid (all unflagged verts, edges and faces).
 */

int BMO_op_vinitf(BMesh *bm, BMOperator *op, const int flag, const char *_fmt, va_list vlist)
{
//	BMOpDefine *def;
	char *opname, *ofmt, *fmt;
	char slot_name[64] = {0};
	int i /*, n = strlen(fmt) */, stop /*, slot_code = -1 */, type, state;
	char htype;
	int noslot = 0;


	/* basic useful info to help find where bmop formatting strings fail */
	const char *err_reason = "Unknown";
	int lineno = -1;

#define GOTO_ERROR(reason)   \
	{                        \
		err_reason = reason; \
		lineno = __LINE__;   \
		goto error;          \
	} (void)0

	/* we muck around in here, so dup i */
	fmt = ofmt = BLI_strdup(_fmt);
	
	/* find operator name */
	i = strcspn(fmt, " ");

	opname = fmt;
	if (!opname[i]) noslot = 1;
	opname[i] = '\0';

	fmt += i + (noslot ? 0 : 1);
	
	i = bmo_opname_to_opcode(opname);

	if (i == -1) {
		MEM_freeN(ofmt);
		return FALSE;
	}

	BMO_op_init(bm, op, flag, opname);
//	def = bmo_opdefines[i];
	
	i = 0;
	state = 1; /* 0: not inside slot_code name, 1: inside slot_code name */

	while (*fmt) {
		if (state) {
			/* jump past leading whitespac */
			i = strspn(fmt, " ");
			fmt += i;
			
			/* ignore trailing whitespac */
			if (!fmt[i])
				break;

			/* find end of slot name, only "slot=%f", can be used */
			i = strcspn(fmt, "=");
			if (!fmt[i]) {
				GOTO_ERROR("could not match end of slot name");
			}

			fmt[i] = 0;

			if (bmo_name_to_slotcode_check(op->slots_in, fmt) < 0) {
				GOTO_ERROR("name to slot code check failed");
			}
			
			BLI_strncpy(slot_name, fmt, sizeof(slot_name));
			
			state = 0;
			fmt += i;
		}
		else {
			switch (*fmt) {
				case ' ':
				case '=':
				case '%':
					break;
				case 'm':
				{
					int size, c;

					c = NEXT_CHAR(fmt);
					fmt++;

					if      (c == '3') size = 3;
					else if (c == '4') size = 4;
					else GOTO_ERROR("matrix size was not 3 or 4");

					BMO_slot_mat_set(op, op->slots_in, slot_name, va_arg(vlist, void *), size);
					state = 1;
					break;
				}
				case 'v':
				{
					BMO_slot_vec_set(op->slots_in, slot_name, va_arg(vlist, float *));
					state = 1;
					break;
				}
				case 'e':  /* single vert/edge/face */
				{
					BMHeader *ele = va_arg(vlist, void *);
					BMOpSlot *slot = BMO_slot_get(op->slots_in, slot_name);

					BMO_slot_buffer_from_single(op, slot, ele);

					state = 1;
					break;
				}
				case 's':
				case 'S':
				{
					BMOperator *op_other        = va_arg(vlist, void *);
					const char *slot_name_other = va_arg(vlist, char *);

					if (*fmt == 's') {
						BLI_assert(bmo_name_to_slotcode_check(op_other->slots_in, slot_name_other) != -1);
						BMO_slot_copy(op_other, slots_in, slot_name_other,
						              op,       slots_in, slot_name);
					}
					else {
						BLI_assert(bmo_name_to_slotcode_check(op_other->slots_out, slot_name_other) != -1);
						BMO_slot_copy(op_other, slots_out, slot_name_other,
						              op,       slots_in, slot_name);
					}
					state = 1;
					break;
				}
				case 'i':
					BMO_slot_int_set(op->slots_in, slot_name, va_arg(vlist, int));
					state = 1;
					break;
				case 'b':
					BMO_slot_bool_set(op->slots_in, slot_name, va_arg(vlist, int));
					state = 1;
					break;
				case 'p':
					BMO_slot_ptr_set(op->slots_in, slot_name, va_arg(vlist, void *));
					state = 1;
					break;
				case 'f':
				case 'F':
				case 'h':
				case 'H':
				case 'a':
					type = *fmt;

					if (NEXT_CHAR(fmt) == ' ' || NEXT_CHAR(fmt) == '\0') {
						BMO_slot_float_set(op->slots_in, slot_name, va_arg(vlist, double));
					}
					else {
						htype = 0;
						stop = 0;
						while (1) {
							switch (NEXT_CHAR(fmt)) {
								case 'f': htype |= BM_FACE; break;
								case 'e': htype |= BM_EDGE; break;
								case 'v': htype |= BM_VERT; break;
								default:
									stop = 1;
									break;
							}
							if (stop) {
								break;
							}

							fmt++;
						}

						if (type == 'h') {
							BMO_slot_buffer_from_enabled_hflag(bm, op, op->slots_in, slot_name, htype, va_arg(vlist, int));
						}
						else if (type == 'H') {
							BMO_slot_buffer_from_disabled_hflag(bm, op, op->slots_in, slot_name, htype, va_arg(vlist, int));
						}
						else if (type == 'a') {
							BMO_slot_buffer_from_all(bm, op, op->slots_in, slot_name, htype);
						}
						else if (type == 'f') {
							BMO_slot_buffer_from_enabled_flag(bm, op, op->slots_in, slot_name, htype, va_arg(vlist, int));
						}
						else if (type == 'F') {
							BMO_slot_buffer_from_disabled_flag(bm, op, op->slots_in, slot_name, htype, va_arg(vlist, int));
						}
					}

					state = 1;
					break;
				default:
					fprintf(stderr,
					        "%s: unrecognized bmop format char: %c, %d in '%s'\n",
					        __func__, *fmt, (int)(fmt - ofmt), ofmt);
					break;
			}
		}
		fmt++;
	}

	MEM_freeN(ofmt);
	return TRUE;
error:

	/* non urgent todo - explain exactly what is failing */
	fprintf(stderr, "%s: error parsing formatting string\n", __func__);

	fprintf(stderr, "string: '%s', position %d\n", _fmt, (int)(fmt - ofmt));
	fprintf(stderr, "         ");
	{
		int pos = (int)(fmt - ofmt);
		int i;
		for (i = 0; i < pos; i++) {
			fprintf(stderr, " ");
		}
		fprintf(stderr, "^\n");
	}

	fprintf(stderr, "source code:  %s:%d\n", __FILE__, lineno);

	fprintf(stderr, "reason: %s\n", err_reason);


	MEM_freeN(ofmt);

	BMO_op_finish(bm, op);
	return FALSE;

#undef GOTO_ERROR

}


int BMO_op_initf(BMesh *bm, BMOperator *op, const int flag, const char *fmt, ...)
{
	va_list list;

	va_start(list, fmt);
	if (!BMO_op_vinitf(bm, op, flag, fmt, list)) {
		printf("%s: failed\n", __func__);
		va_end(list);
		return FALSE;
	}
	va_end(list);

	return TRUE;
}

int BMO_op_callf(BMesh *bm, const int flag, const char *fmt, ...)
{
	va_list list;
	BMOperator op;

	va_start(list, fmt);
	if (!BMO_op_vinitf(bm, &op, flag, fmt, list)) {
		printf("%s: failed, format is:\n    \"%s\"\n", __func__, fmt);
		va_end(list);
		return FALSE;
	}

	BMO_op_exec(bm, &op);
	BMO_op_finish(bm, &op);

	va_end(list);
	return TRUE;
}
