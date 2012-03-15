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

#include "bmesh.h"
#include "intern/bmesh_private.h"

/* forward declarations */
static void bmo_flag_layer_alloc(BMesh *bm);
static void bmo_flag_layer_free(BMesh *bm);
static void bmo_flag_layer_clear(BMesh *bm);
static int bmo_name_to_slotcode(BMOpDefine *def, const char *name);
static int bmo_name_to_slotcode_check(BMOpDefine *def, const char *name);
static int bmo_opname_to_opcode(const char *opname);

static const char *bmo_error_messages[] = {
	NULL,
	"Self intersection error",
	"Could not dissolve vert",
	"Could not connect vertices",
	"Could not traverse mesh",
	"Could not dissolve faces",
	"Could not dissolve vertices",
	"Tessellation error",
	"Can not deal with non-manifold geometry",
	"Invalid selection",
	"Internal mesh error",
};


/* operator slot type information - size of one element of the type given. */
const int BMO_OPSLOT_TYPEINFO[BMO_OP_SLOT_TOTAL_TYPES] = {
	0,
	sizeof(int),
	sizeof(int),
	sizeof(float),
	sizeof(void *),
	0, /* unused */
	0, /* unused */
	0, /* unused */
	sizeof(void *),	/* pointer buffer */
	sizeof(BMOElemMapping)
};

/* Dummy slot so there is something to return when slot name lookup fails */
static BMOpSlot BMOpEmptySlot = {0};

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

/**
 * \brief BMESH OPSTACK INIT OP
 *
 * Initializes an operator structure to a certain type
 */
void BMO_op_init(BMesh *bm, BMOperator *op, const char *opname)
{
	int i, opcode = bmo_opname_to_opcode(opname);

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
	op->flag = opdefines[opcode]->flag;
	
	/* initialize the operator slot types */
	for (i = 0; opdefines[opcode]->slottypes[i].type; i++) {
		op->slots[i].slottype = opdefines[opcode]->slottypes[i].type;
		op->slots[i].index = i;
	}

	/* callback */
	op->exec = opdefines[opcode]->exec;

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
	
	BMO_push(bm, op);

	if (bm->stackdepth == 2)
		bmesh_edit_begin(bm, op->flag);
	op->exec(bm, op);
	
	if (bm->stackdepth == 2)
		bmesh_edit_end(bm, op->flag);
	
	BMO_pop(bm);
}

/**
 * \brief BMESH OPSTACK FINISH OP
 *
 * Does housekeeping chores related to finishing up an operator.
 */
void BMO_op_finish(BMesh *bm, BMOperator *op)
{
	BMOpSlot *slot;
	int i;

	for (i = 0; opdefines[op->type]->slottypes[i].type; i++) {
		slot = &op->slots[i];
		if (slot->slottype == BMO_OP_SLOT_MAPPING) {
			if (slot->data.ghash)
				BLI_ghash_free(slot->data.ghash, NULL, NULL);
		}
	}

	BLI_memarena_free(op->arena);

#ifdef DEBUG
	BM_ELEM_INDEX_VALIDATE(bm, "post bmo", opdefines[op->type]->name);
#else
	(void)bm;
#endif
}

/**
 * \brief BMESH OPSTACK HAS SLOT
 *
 * \return Success if the slot if found.
 */
int BMO_slot_exists(BMOperator *op, const char *slotname)
{
	int slotcode = bmo_name_to_slotcode(opdefines[op->type], slotname);
	return (slotcode >= 0);
}

/**
 * \brief BMESH OPSTACK GET SLOT
 *
 * Returns a pointer to the slot of type 'slotcode'
 */
BMOpSlot *BMO_slot_get(BMOperator *op, const char *slotname)
{
	int slotcode = bmo_name_to_slotcode_check(opdefines[op->type], slotname);

	if (slotcode < 0) {
		return &BMOpEmptySlot;
	}

	return &(op->slots[slotcode]);
}

/**
 * \brief BMESH OPSTACK COPY SLOT
 *
 * Copies data from one slot to another.
 */
void BMO_slot_copy(BMOperator *source_op, BMOperator *dest_op, const char *src, const char *dst)
{
	BMOpSlot *source_slot = BMO_slot_get(source_op, src);
	BMOpSlot *dest_slot = BMO_slot_get(dest_op, dst);

	if (source_slot == dest_slot)
		return;

	if (source_slot->slottype != dest_slot->slottype) {
		/* possibly assert here? */
		return;
	}

	if (dest_slot->slottype  == BMO_OP_SLOT_ELEMENT_BUF) {
		/* do buffer copy */
		dest_slot->data.buf = NULL;
		dest_slot->len = source_slot->len;
		if (dest_slot->len) {
			const int slot_alloc_size = BMO_OPSLOT_TYPEINFO[dest_slot->slottype] * dest_slot->len;
			dest_slot->data.buf = BLI_memarena_alloc(dest_op->arena, slot_alloc_size);
			memcpy(dest_slot->data.buf, source_slot->data.buf, slot_alloc_size);
		}
	}
	else if (dest_slot->slottype  == BMO_OP_SLOT_MAPPING) {
		GHashIterator it;
		BMOElemMapping *srcmap, *dstmap;

		/* sanity check */
		if (!source_slot->data.ghash) {
			return;
		}

		if (!dest_slot->data.ghash) {
			dest_slot->data.ghash = BLI_ghash_new(BLI_ghashutil_ptrhash,
												  BLI_ghashutil_ptrcmp, "bmesh operator 2");
		}

		BLI_ghashIterator_init(&it, source_slot->data.ghash);
		for ( ; (srcmap = BLI_ghashIterator_getValue(&it));
			  BLI_ghashIterator_step(&it))
		{
			dstmap = BLI_memarena_alloc(dest_op->arena, sizeof(*dstmap) + srcmap->len);

			dstmap->element = srcmap->element;
			dstmap->len = srcmap->len;
			memcpy(dstmap + 1, srcmap + 1, srcmap->len);

			BLI_ghash_insert(dest_slot->data.ghash, dstmap->element, dstmap);
		}
	}
	else {
		dest_slot->data = source_slot->data;
	}
}

/*
 * BMESH OPSTACK SET XXX
 *
 * Sets the value of a slot depending on it's type
 */

void BMO_slot_float_set(BMOperator *op, const char *slotname, const float f)
{
	BMOpSlot *slot = BMO_slot_get(op, slotname);
	BLI_assert(slot->slottype == BMO_OP_SLOT_FLT);
	if (!(slot->slottype == BMO_OP_SLOT_FLT))
		return;

	slot->data.f = f;
}

void BMO_slot_int_set(BMOperator *op, const char *slotname, const int i)
{
	BMOpSlot *slot = BMO_slot_get(op, slotname);
	BLI_assert(slot->slottype == BMO_OP_SLOT_INT);
	if (!(slot->slottype == BMO_OP_SLOT_INT))
		return;

	slot->data.i = i;
}

void BMO_slot_bool_set(BMOperator *op, const char *slotname, const int i)
{
	BMOpSlot *slot = BMO_slot_get(op, slotname);
	BLI_assert(slot->slottype == BMO_OP_SLOT_BOOL);
	if (!(slot->slottype == BMO_OP_SLOT_BOOL))
		return;

	slot->data.i = i;
}

/* only supports square mats */
void BMO_slot_mat_set(BMOperator *op, const char *slotname, const float *mat, int size)
{
	BMOpSlot *slot = BMO_slot_get(op, slotname);
	BLI_assert(slot->slottype == BMO_OP_SLOT_MAT);
	if (!(slot->slottype == BMO_OP_SLOT_MAT))
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

		memset(slot->data.p, 0, sizeof(float) * 4 * 4);
	}
}

void BMO_slot_mat4_get(BMOperator *op, const char *slotname, float r_mat[4][4])
{
	BMOpSlot *slot = BMO_slot_get(op, slotname);
	BLI_assert(slot->slottype == BMO_OP_SLOT_MAT);
	if (!(slot->slottype == BMO_OP_SLOT_MAT))
		return;

	copy_m4_m4(r_mat, (float (*)[4])slot->data.p);
}

void BMO_slot_mat3_set(BMOperator *op, const char *slotname, float r_mat[3][3])
{
	BMOpSlot *slot = BMO_slot_get(op, slotname);
	BLI_assert(slot->slottype == BMO_OP_SLOT_MAT);
	if (!(slot->slottype == BMO_OP_SLOT_MAT))
		return;

	copy_m3_m4(r_mat, slot->data.p);
}

void BMO_slot_ptr_set(BMOperator *op, const char *slotname, void *p)
{
	BMOpSlot *slot = BMO_slot_get(op, slotname);
	BLI_assert(slot->slottype == BMO_OP_SLOT_PNT);
	if (!(slot->slottype == BMO_OP_SLOT_PNT))
		return;

	slot->data.p = p;
}

void BMO_slot_vec_set(BMOperator *op, const char *slotname, const float vec[3])
{
	BMOpSlot *slot = BMO_slot_get(op, slotname);
	BLI_assert(slot->slottype == BMO_OP_SLOT_VEC);
	if (!(slot->slottype == BMO_OP_SLOT_VEC))
		return;

	copy_v3_v3(slot->data.vec, vec);
}


float BMO_slot_float_get(BMOperator *op, const char *slotname)
{
	BMOpSlot *slot = BMO_slot_get(op, slotname);
	BLI_assert(slot->slottype == BMO_OP_SLOT_FLT);
	if (!(slot->slottype == BMO_OP_SLOT_FLT))
		return 0.0f;

	return slot->data.f;
}

int BMO_slot_int_get(BMOperator *op, const char *slotname)
{
	BMOpSlot *slot = BMO_slot_get(op, slotname);
	BLI_assert(slot->slottype == BMO_OP_SLOT_INT);
	if (!(slot->slottype == BMO_OP_SLOT_INT))
		return 0;

	return slot->data.i;
}

int BMO_slot_bool_get(BMOperator *op, const char *slotname)
{
	BMOpSlot *slot = BMO_slot_get(op, slotname);
	BLI_assert(slot->slottype == BMO_OP_SLOT_BOOL);
	if (!(slot->slottype == BMO_OP_SLOT_BOOL))
		return 0;

	return slot->data.i;
}


void *BMO_slot_ptr_get(BMOperator *op, const char *slotname)
{
	BMOpSlot *slot = BMO_slot_get(op, slotname);
	BLI_assert(slot->slottype == BMO_OP_SLOT_PNT);
	if (!(slot->slottype == BMO_OP_SLOT_PNT))
		return NULL;

	return slot->data.p;
}

void BMO_slot_vec_get(BMOperator *op, const char *slotname, float r_vec[3])
{
	BMOpSlot *slot = BMO_slot_get(op, slotname);
	BLI_assert(slot->slottype == BMO_OP_SLOT_VEC);
	if (!(slot->slottype == BMO_OP_SLOT_VEC))
		return;

	copy_v3_v3(r_vec, slot->data.vec);
}

/*
 * BMO_COUNTFLAG
 *
 * Counts the number of elements of a certain type that
 * have a specific flag set.
 *
 */

int BMO_mesh_flag_count(BMesh *bm, const short oflag, const char htype)
{
	BMIter elements;
	int count = 0;
	BMElemF *ele_f;

	if (htype & BM_VERT) {
		for (ele_f = BM_iter_new(&elements, bm, BM_VERTS_OF_MESH, bm); ele_f; ele_f = BM_iter_step(&elements)) {
			if (BMO_elem_flag_test(bm, ele_f, oflag))
				count++;
		}
	}
	if (htype & BM_EDGE) {
		for (ele_f = BM_iter_new(&elements, bm, BM_EDGES_OF_MESH, bm); ele_f; ele_f = BM_iter_step(&elements)) {
			if (BMO_elem_flag_test(bm, ele_f, oflag))
				count++;
		}
	}
	if (htype & BM_FACE) {
		for (ele_f = BM_iter_new(&elements, bm, BM_FACES_OF_MESH, bm); ele_f; ele_f = BM_iter_step(&elements)) {
			if (BMO_elem_flag_test(bm, ele_f, oflag))
				count++;
		}
	}

	return count;
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
			BM_ITER(ele, &iter, bm, iter_types[i], NULL) {
				BMO_elem_flag_disable(bm, ele, oflag);
			}
		}
	}
}

int BMO_slot_buffer_count(BMesh *UNUSED(bm), BMOperator *op, const char *slotname)
{
	BMOpSlot *slot = BMO_slot_get(op, slotname);
	BLI_assert(slot->slottype == BMO_OP_SLOT_ELEMENT_BUF);
	
	/* check if its actually a buffer */
	if (slot->slottype != BMO_OP_SLOT_ELEMENT_BUF)
		return 0;

	return slot->len;
}

int BMO_slot_map_count(BMesh *UNUSED(bm), BMOperator *op, const char *slotname)
{
	BMOpSlot *slot = BMO_slot_get(op, slotname);
	BLI_assert(slot->slottype == BMO_OP_SLOT_MAPPING);
	
	/* check if its actually a buffer */
	if (!(slot->slottype == BMO_OP_SLOT_MAPPING))
		return 0;

	return slot->data.ghash ? BLI_ghash_size(slot->data.ghash) : 0;
}

/* inserts a key/value mapping into a mapping slot.  note that it copies the
 * value, it doesn't store a reference to it. */

void BMO_slot_map_insert(BMesh *UNUSED(bm), BMOperator *op, const char *slotname,
                         void *element, void *data, int len)
{
	BMOElemMapping *mapping;
	BMOpSlot *slot = BMO_slot_get(op, slotname);
	BLI_assert(slot->slottype == BMO_OP_SLOT_MAPPING);

	mapping = (BMOElemMapping *) BLI_memarena_alloc(op->arena, sizeof(*mapping) + len);

	mapping->element = (BMHeader *) element;
	mapping->len = len;
	memcpy(mapping + 1, data, len);

	if (!slot->data.ghash) {
		slot->data.ghash = BLI_ghash_new(BLI_ghashutil_ptrhash,
		                                 BLI_ghashutil_ptrcmp, "bmesh op");
	}

	BLI_ghash_insert(slot->data.ghash, element, mapping);
}

#if 0
void *bmo_slot_buffer_grow(BMesh *bm, BMOperator *op, int slotcode, int totadd)
{
	BMOpSlot *slot = &op->slots[slotcode];
	void *tmp;
	ssize_t allocsize;
	
	BLI_assert(slot->slottype == BMO_OP_SLOT_ELEMENT_BUF);

	/* check if its actually a buffer */
	if (slot->slottype != BMO_OP_SLOT_ELEMENT_BUF)
		return NULL;

	if (slot->flag & BMOS_DYNAMIC_ARRAY) {
		if (slot->len >= slot->size) {
			slot->size = (slot->size + 1 + totadd) * 2;

			allocsize = BMO_OPSLOT_TYPEINFO[opdefines[op->type]->slottypes[slotcode].type] * slot->size;

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

		allocsize = BMO_OPSLOT_TYPEINFO[opdefines[op->type]->slottypes[slotcode].type] * slot->len;

		tmp = slot->data.buf;
		slot->data.buf = MEM_callocN(allocsize, "opslot dynamic array");
		memcpy(slot->data.buf, tmp, allocsize);
	}

	return slot->data.buf;
}
#endif

void BMO_slot_map_to_flag(BMesh *bm, BMOperator *op, const char *slotname,
                          const short oflag, const char htype)
{
	GHashIterator it;
	BMOpSlot *slot = BMO_slot_get(op, slotname);
	BMElemF *ele_f;

	BLI_assert(slot->slottype == BMO_OP_SLOT_MAPPING);

	/* sanity check */
	if (!slot->data.ghash) return;

	BLI_ghashIterator_init(&it, slot->data.ghash);
	for ( ; (ele_f = BLI_ghashIterator_getKey(&it)); BLI_ghashIterator_step(&it)) {
		if (ele_f->head.htype & htype) {
			BMO_elem_flag_enable(bm, ele_f, oflag);
		}
	}
}

static void *bmo_slot_buffer_alloc(BMOperator *op, const char *slotname, int len)
{
	BMOpSlot *slot = BMO_slot_get(op, slotname);
	BLI_assert(slot->slottype == BMO_OP_SLOT_ELEMENT_BUF);

	/* check if its actually a buffer */
	if (slot->slottype != BMO_OP_SLOT_ELEMENT_BUF)
		return NULL;
	
	slot->len = len;
	if (len)
		slot->data.buf = BLI_memarena_alloc(op->arena, BMO_OPSLOT_TYPEINFO[slot->slottype] * len);
	return slot->data.buf;
}

/**
 * \brief BMO_ALL_TO_SLOT
 *
 * Copies all elements of a certain type into an operator slot.
 */
static void BMO_slot_buffer_from_all(BMesh *bm, BMOperator *op, const char *slotname, const char htype)
{
	BMIter elements;
	BMHeader *e;
	BMOpSlot *output = BMO_slot_get(op, slotname);
	int totelement = 0, i = 0;
	
	if (htype & BM_VERT) totelement += bm->totvert;
	if (htype & BM_EDGE) totelement += bm->totedge;
	if (htype & BM_FACE) totelement += bm->totface;

	if (totelement) {
		bmo_slot_buffer_alloc(op, slotname, totelement);

		if (htype & BM_VERT) {
			for (e = BM_iter_new(&elements, bm, BM_VERTS_OF_MESH, bm); e; e = BM_iter_step(&elements)) {
				((BMHeader **)output->data.p)[i] = e;
				i++;
			}
		}

		if (htype & BM_EDGE) {
			for (e = BM_iter_new(&elements, bm, BM_EDGES_OF_MESH, bm); e; e = BM_iter_step(&elements)) {
				((BMHeader **)output->data.p)[i] = e;
				i++;
			}
		}

		if (htype & BM_FACE) {
			for (e = BM_iter_new(&elements, bm, BM_FACES_OF_MESH, bm); e; e = BM_iter_step(&elements)) {
				((BMHeader **)output->data.p)[i] = e;
				i++;
			}
		}
	}
}

/**
 * \brief BMO_HEADERFLAG_TO_SLOT
 *
 * Copies elements of a certain type, which have a certain header flag set
 * into a slot for an operator.
 */
void BMO_slot_buffer_from_hflag(BMesh *bm, BMOperator *op, const char *slotname,
                                const char hflag, const char htype)
{
	BMIter elements;
	BMElem *ele;
	BMOpSlot *output = BMO_slot_get(op, slotname);
	int totelement = 0, i = 0;
	
	totelement = BM_mesh_count_flag(bm, htype, hflag, TRUE);

	if (totelement) {
		bmo_slot_buffer_alloc(op, slotname, totelement);

		if (htype & BM_VERT) {
			for (ele = BM_iter_new(&elements, bm, BM_VERTS_OF_MESH, bm); ele; ele = BM_iter_step(&elements)) {
				if (!BM_elem_flag_test(ele, BM_ELEM_HIDDEN) && BM_elem_flag_test(ele, hflag)) {
					((BMElem **)output->data.p)[i] = ele;
					i++;
				}
			}
		}

		if (htype & BM_EDGE) {
			for (ele = BM_iter_new(&elements, bm, BM_EDGES_OF_MESH, bm); ele; ele = BM_iter_step(&elements)) {
				if (!BM_elem_flag_test(ele, BM_ELEM_HIDDEN) && BM_elem_flag_test(ele, hflag)) {
					((BMElem **)output->data.p)[i] = ele;
					i++;
				}
			}
		}

		if (htype & BM_FACE) {
			for (ele = BM_iter_new(&elements, bm, BM_FACES_OF_MESH, bm); ele; ele = BM_iter_step(&elements)) {
				if (!BM_elem_flag_test(ele, BM_ELEM_HIDDEN) && BM_elem_flag_test(ele, hflag)) {
					((BMElem **)output->data.p)[i] = ele;
					i++;
				}
			}
		}
	}
	else {
		output->len = 0;
	}
}

/**
 * \brief BMO_FLAG_TO_SLOT
 *
 * Copies elements of a certain type, which have a certain flag set
 * into an output slot for an operator.
 */
void BMO_slot_buffer_from_flag(BMesh *bm, BMOperator *op, const char *slotname,
                               const short oflag, const char htype)
{
	BMIter elements;
	BMOpSlot *slot = BMO_slot_get(op, slotname);
	int totelement = BMO_mesh_flag_count(bm, oflag, htype), i = 0;

	BLI_assert(slot->slottype == BMO_OP_SLOT_ELEMENT_BUF);

	if (totelement) {
		BMHeader *ele;
		BMHeader **ele_array;

		bmo_slot_buffer_alloc(op, slotname, totelement);

		ele_array = (BMHeader **)slot->data.p;

		if (htype & BM_VERT) {
			for (ele = BM_iter_new(&elements, bm, BM_VERTS_OF_MESH, bm); ele; ele = BM_iter_step(&elements)) {
				if (BMO_elem_flag_test(bm, (BMElemF *)ele, oflag)) {
					ele_array[i] = ele;
					i++;
				}
			}
		}

		if (htype & BM_EDGE) {
			for (ele = BM_iter_new(&elements, bm, BM_EDGES_OF_MESH, bm); ele; ele = BM_iter_step(&elements)) {
				if (BMO_elem_flag_test(bm, (BMElemF *)ele, oflag)) {
					ele_array[i] = ele;
					i++;
				}
			}
		}

		if (htype & BM_FACE) {
			for (ele = BM_iter_new(&elements, bm, BM_FACES_OF_MESH, bm); ele; ele = BM_iter_step(&elements)) {
				if (BMO_elem_flag_test(bm, (BMElemF *)ele, oflag)) {
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

/**
 * \brief BMO_FLAG_BUFFER
 *
 * Header Flags elements in a slots buffer, automatically
 * using the selection API where appropriate.
 */
void BMO_slot_buffer_hflag_enable(BMesh *bm, BMOperator *op, const char *slotname,
                                  const char hflag, const char htype, const char do_flush)
{
	BMOpSlot *slot = BMO_slot_get(op, slotname);
	BMElem **data =  slot->data.p;
	int i;
	const char do_flush_select = (do_flush && (hflag & BM_ELEM_SELECT));
	const char do_flush_hide = (do_flush && (hflag & BM_ELEM_HIDDEN));

	BLI_assert(slot->slottype == BMO_OP_SLOT_ELEMENT_BUF);

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
void BMO_slot_buffer_hflag_disable(BMesh *bm, BMOperator *op, const char *slotname,
                                   const char hflag, const char htype, const char do_flush)
{
	BMOpSlot *slot = BMO_slot_get(op, slotname);
	BMElem **data =  slot->data.p;
	int i;
	const char do_flush_select = (do_flush && (hflag & BM_ELEM_SELECT));
	const char do_flush_hide = (do_flush && (hflag & BM_ELEM_HIDDEN));

	BLI_assert(slot->slottype == BMO_OP_SLOT_ELEMENT_BUF);

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
void BMO_slot_buffer_flag_enable(BMesh *bm, BMOperator *op, const char *slotname,
                                 const short oflag, const char htype)
{
	BMOpSlot *slot = BMO_slot_get(op, slotname);
	BMHeader **data =  slot->data.p;
	int i;

	BLI_assert(slot->slottype == BMO_OP_SLOT_ELEMENT_BUF);

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
void BMO_slot_buffer_flag_disable(BMesh *bm, BMOperator *op, const char *slotname,
                                  const short oflag, const char htype)
{
	BMOpSlot *slot = BMO_slot_get(op, slotname);
	BMHeader **data =  slot->data.p;
	int i;

	BLI_assert(slot->slottype == BMO_OP_SLOT_ELEMENT_BUF);

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
	BLI_mempool *oldpool = bm->toolflagpool; 		/* old flag pool */
	BLI_mempool *newpool;
	void *oldflags;

	/* store memcpy size for reuse */
	const size_t old_totflags_size = (bm->totflags * sizeof(BMFlagLayer));
	
	bm->totflags++;

	/* allocate new flag poo */
	bm->toolflagpool = newpool = BLI_mempool_create(sizeof(BMFlagLayer) * bm->totflags, 512, 512, 0);
	
	/* now go through and memcpy all the flags. Loops don't get a flag layer at this time.. */
	for (ele = BM_iter_new(&iter, bm, BM_VERTS_OF_MESH, bm), i = 0; ele; ele = BM_iter_step(&iter), i++) {
		oldflags = ele->oflags;
		ele->oflags = BLI_mempool_calloc(newpool);
		memcpy(ele->oflags, oldflags, old_totflags_size);
		BM_elem_index_set(ele, i); /* set_inline */
	}
	for (ele = BM_iter_new(&iter, bm, BM_EDGES_OF_MESH, bm), i = 0; ele; ele = BM_iter_step(&iter), i++) {
		oldflags = ele->oflags;
		ele->oflags = BLI_mempool_calloc(newpool);
		memcpy(ele->oflags, oldflags, old_totflags_size);
		BM_elem_index_set(ele, i); /* set_inline */
	}
	for (ele = BM_iter_new(&iter, bm, BM_FACES_OF_MESH, bm), i = 0; ele; ele = BM_iter_step(&iter), i++) {
		oldflags = ele->oflags;
		ele->oflags = BLI_mempool_calloc(newpool);
		memcpy(ele->oflags, oldflags, old_totflags_size);
		BM_elem_index_set(ele, i); /* set_inline */
	}

	bm->elem_index_dirty &= ~(BM_VERT|BM_EDGE|BM_FACE);

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
	for (ele = BM_iter_new(&iter, bm, BM_VERTS_OF_MESH, bm), i = 0; ele; ele = BM_iter_step(&iter), i++) {
		oldflags = ele->oflags;
		ele->oflags = BLI_mempool_calloc(newpool);
		memcpy(ele->oflags, oldflags, new_totflags_size);
		BM_elem_index_set(ele, i); /* set_inline */
	}
	for (ele = BM_iter_new(&iter, bm, BM_EDGES_OF_MESH, bm), i = 0; ele; ele = BM_iter_step(&iter), i++) {
		oldflags = ele->oflags;
		ele->oflags = BLI_mempool_calloc(newpool);
		memcpy(ele->oflags, oldflags, new_totflags_size);
		BM_elem_index_set(ele, i); /* set_inline */
	}
	for (ele = BM_iter_new(&iter, bm, BM_FACES_OF_MESH, bm), i = 0; ele; ele = BM_iter_step(&iter), i++) {
		oldflags = ele->oflags;
		ele->oflags = BLI_mempool_calloc(newpool);
		memcpy(ele->oflags, oldflags, new_totflags_size);
		BM_elem_index_set(ele, i); /* set_inline */
	}

	bm->elem_index_dirty &= ~(BM_VERT|BM_EDGE|BM_FACE);

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
	for (ele = BM_iter_new(&iter, bm, BM_VERTS_OF_MESH, bm), i = 0; ele; ele = BM_iter_step(&iter), i++) {
		memset(ele->oflags + totflags_offset, 0, sizeof(BMFlagLayer));
		BM_elem_index_set(ele, i); /* set_inline */
	}
	for (ele = BM_iter_new(&iter, bm, BM_EDGES_OF_MESH, bm), i = 0; ele; ele = BM_iter_step(&iter), i++) {
		memset(ele->oflags + totflags_offset, 0, sizeof(BMFlagLayer));
		BM_elem_index_set(ele, i); /* set_inline */
	}
	for (ele = BM_iter_new(&iter, bm, BM_FACES_OF_MESH, bm), i = 0; ele; ele = BM_iter_step(&iter), i++) {
		memset(ele->oflags + totflags_offset, 0, sizeof(BMFlagLayer));
		BM_elem_index_set(ele, i); /* set_inline */
	}

	bm->elem_index_dirty &= ~(BM_VERT|BM_EDGE|BM_FACE);
}

void *BMO_slot_buffer_elem_first(BMOperator *op, const char *slotname)
{
	BMOpSlot *slot = BMO_slot_get(op, slotname);
	
	if (slot->slottype != BMO_OP_SLOT_ELEMENT_BUF)
		return NULL;

	return slot->data.buf ? *(void **)slot->data.buf : NULL;
}

/**
 * \brief New Iterator
 *
 * \param restrictmask restricts the iteration to certain element types
 * (e.g. combination of BM_VERT, BM_EDGE, BM_FACE), if iterating
 * over an element buffer (not a mapping). */
void *BMO_iter_new(BMOIter *iter, BMesh *UNUSED(bm), BMOperator *op,
                   const char *slotname, const char restrictmask)
{
	BMOpSlot *slot = BMO_slot_get(op, slotname);

	memset(iter, 0, sizeof(BMOIter));

	iter->slot = slot;
	iter->cur = 0;
	iter->restrictmask = restrictmask;

	if (iter->slot->slottype == BMO_OP_SLOT_MAPPING) {
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
	if (iter->slot->slottype == BMO_OP_SLOT_ELEMENT_BUF) {
		BMHeader *h;

		if (iter->cur >= iter->slot->len) {
			return NULL;
		}

		h = ((void **)iter->slot->data.buf)[iter->cur++];
		while (!(iter->restrictmask & h->htype)) {
			if (iter->cur >= iter->slot->len) {
				return NULL;
			}

			h = ((void **)iter->slot->data.buf)[iter->cur++];
		}

		return h;
	}
	else if (iter->slot->slottype == BMO_OP_SLOT_MAPPING) {
		BMOElemMapping *map;
		void *ret = BLI_ghashIterator_getKey(&iter->giter);
		map = BLI_ghashIterator_getValue(&iter->giter);
		
		iter->val = map + 1;

		BLI_ghashIterator_step(&iter->giter);

		return ret;
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
	if (!msg) msg = bmo_error_messages[errcode];
	err->msg = msg;
	err->op = owner;
	
	BLI_addhead(&bm->errorstack, err);
}

int BMO_error_occurred(BMesh *bm)
{
	return bm->errorstack.first != NULL;
}

/* returns error code or 0 if no erro */
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

static int bmo_name_to_slotcode(BMOpDefine *def, const char *name)
{
	int i;

	for (i = 0; def->slottypes[i].type; i++) {
		if (!strncmp(name, def->slottypes[i].name, MAX_SLOTNAME)) {
			return i;
		}
	}

	return -1;
}

static int bmo_name_to_slotcode_check(BMOpDefine *def, const char *name)
{
	int i = bmo_name_to_slotcode(def, name);
	if (i < 0) {
		fprintf(stderr, "%s: ! could not find bmesh slot for name %s! (bmesh internal error)\n", __func__, name);
	}

	return i;
}

static int bmo_opname_to_opcode(const char *opname)
{
	int i;

	for (i = 0; i < bmesh_total_ops; i++) {
		if (!strcmp(opname, opdefines[i]->name)) {
			return i;
		}
	}

	fprintf(stderr, "%s: ! could not find bmesh slot for name %s! (bmesh internal error)\n", __func__, opname);
	return -1;
}

/* Example:
 * BMO_op_callf(bm, "del %i %hv", DEL_ONLYFACES, BM_ELEM_SELECT);
 *
 *  i - int
 *  b - boolean (same as int but 1/0 only)
 *  f - float
 *  hv - header flagged verts (hflag)
 *  he - header flagged edges (hflag)
 *  hf - header flagged faces (hflag)
 *  fv - flagged verts (oflag)
 *  fe - flagged edges (oflag)
 *  ff - flagged faces (oflag)
 */

int BMO_op_vinitf(BMesh *bm, BMOperator *op, const char *_fmt, va_list vlist)
{
	BMOpDefine *def;
	char *opname, *ofmt, *fmt;
	char slotname[64] = {0};
	int i /*, n = strlen(fmt) */, stop /*, slotcode = -1 */, ret, type, state;
	int noslot = 0;


	/* basic useful info to help find where bmop formatting strings fail */
	int lineno = -1;
#   define GOTO_ERROR { lineno = __LINE__; goto error; }


	/* we muck around in here, so dup i */
	fmt = ofmt = BLI_strdup(_fmt);
	
	/* find operator nam */
	i = strcspn(fmt, " \t");

	opname = fmt;
	if (!opname[i]) noslot = 1;
	opname[i] = '\0';

	fmt += i + (noslot ? 0 : 1);
	
	i = bmo_opname_to_opcode(opname);

	if (i == -1) {
		MEM_freeN(ofmt);
		return FALSE;
	}

	BMO_op_init(bm, op, opname);
	def = opdefines[i];
	
	i = 0;
	state = 1; /* 0: not inside slotcode name, 1: inside slotcode name */

	while (*fmt) {
		if (state) {
			/* jump past leading whitespac */
			i = strspn(fmt, " \t");
			fmt += i;
			
			/* ignore trailing whitespac */
			if (!fmt[i])
				break;

			/* find end of slot name.  currently this is
			 * a little flexible, allowing "slot=%f",
			 * "slot %f", "slot%f", and "slot\t%f". */
			i = strcspn(fmt, "= \t%");
			if (!fmt[i]) GOTO_ERROR;

			fmt[i] = 0;

			if (bmo_name_to_slotcode_check(def, fmt) < 0) GOTO_ERROR;
			
			BLI_strncpy(slotname, fmt, sizeof(slotname));
			
			state = 0;
			fmt += i;
		}
		else {
			switch (*fmt) {
				case ' ':
				case '\t':
				case '=':
				case '%':
					break;
				case 'm': {
					int size, c;

					c = NEXT_CHAR(fmt);
					fmt++;

					if (c == '3') size = 3;
					else if (c == '4') size = 4;
					else GOTO_ERROR;

					BMO_slot_mat_set(op, slotname, va_arg(vlist, void *), size);
					state = 1;
					break;
				}
				case 'v': {
					BMO_slot_vec_set(op, slotname, va_arg(vlist, float *));
					state = 1;
					break;
				}
				case 'e': {
					BMHeader *ele = va_arg(vlist, void *);
					BMOpSlot *slot = BMO_slot_get(op, slotname);

					slot->data.buf = BLI_memarena_alloc(op->arena, sizeof(void *) * 4);
					slot->len = 1;
					*((void **)slot->data.buf) = ele;

					state = 1;
					break;
				}
				case 's': {
					BMOperator *op2 = va_arg(vlist, void *);
					const char *slotname2 = va_arg(vlist, char *);

					BMO_slot_copy(op2, op, slotname2, slotname);
					state = 1;
					break;
				}
				case 'i':
					BMO_slot_int_set(op, slotname, va_arg(vlist, int));
					state = 1;
					break;
				case 'b':
					BMO_slot_bool_set(op, slotname, va_arg(vlist, int));
					state = 1;
					break;
				case 'p':
					BMO_slot_ptr_set(op, slotname, va_arg(vlist, void *));
					state = 1;
					break;
				case 'f':
				case 'h':
				case 'a':
					type = *fmt;

					if (NEXT_CHAR(fmt) == ' ' || NEXT_CHAR(fmt) == '\t' || NEXT_CHAR(fmt) == '\0') {
						BMO_slot_float_set(op, slotname, va_arg(vlist, double));
					}
					else {
						ret = 0;
						stop = 0;
						while (1) {
							switch (NEXT_CHAR(fmt)) {
								case 'f': ret |= BM_FACE; break;
								case 'e': ret |= BM_EDGE; break;
								case 'v': ret |= BM_VERT; break;
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
							BMO_slot_buffer_from_hflag(bm, op, slotname, va_arg(vlist, int), ret);
						}
						else if (type == 'a') {
							BMO_slot_buffer_from_all(bm, op, slotname, ret);
						}
						else {
							BMO_slot_buffer_from_flag(bm, op, slotname, va_arg(vlist, int), ret);
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
	fprintf(stderr,
	        "%s: error parsing formatting string, %d in '%s'\n    see - %s:%d\n",
	        __func__, (int)(fmt - ofmt), _fmt, __FILE__, lineno);
	MEM_freeN(ofmt);

	BMO_op_finish(bm, op);
	return FALSE;

#undef GOTO_ERROR

}


int BMO_op_initf(BMesh *bm, BMOperator *op, const char *fmt, ...)
{
	va_list list;

	va_start(list, fmt);
	if (!BMO_op_vinitf(bm, op, fmt, list)) {
		printf("%s: failed\n", __func__);
		va_end(list);
		return FALSE;
	}
	va_end(list);

	return TRUE;
}

int BMO_op_callf(BMesh *bm, const char *fmt, ...)
{
	va_list list;
	BMOperator op;

	va_start(list, fmt);
	if (!BMO_op_vinitf(bm, &op, fmt, list)) {
		printf("%s: failed, format is:\n    \"%s\"\n", __func__, fmt);
		va_end(list);
		return FALSE;
	}

	BMO_op_exec(bm, &op);
	BMO_op_finish(bm, &op);

	va_end(list);
	return TRUE;
}
