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
#include "bmesh_private.h"
#include "stdarg.h"

#include <string.h>

/*forward declarations*/
static void alloc_flag_layer(BMesh *bm);
static void free_flag_layer(BMesh *bm);
static void clear_flag_layer(BMesh *bm);
static int bmesh_name_to_slotcode(BMOpDefine *def, const char *name);
static int bmesh_name_to_slotcode_check(BMOpDefine *def, const char *name);
static int bmesh_opname_to_opcode(const char *opname);

static const char *bmop_error_messages[] = {
       NULL,
       "Self intersection error",
       "Could not dissolve vert",
       "Could not connect vertices",
       "Could not traverse mesh",
       "Could not dissolve faces",
       "Could not dissolve vertices",
       "Tesselation error",
       "Can not deal with non-manifold geometry",
       "Invalid selection",
	   "Internal mesh error",
};


/*operator slot type information - size of one element of the type given.*/
const int BMOP_OPSLOT_TYPEINFO[] = {
	0,
	sizeof(int),
	sizeof(float),
	sizeof(void*),
	0, /* unused */
	0, /* unused */
	0, /* unused */
	sizeof(void*),	/* pointer buffer */
	sizeof(element_mapping)
};

/* Dummy slot so there is something to return when slot name lookup fails */
static BMOpSlot BMOpEmptySlot = { 0 };

void BMO_Set_OpFlag(BMesh *UNUSED(bm), BMOperator *op, int flag)
{
	op->flag |= flag;
}

void BMO_Clear_OpFlag(BMesh *UNUSED(bm), BMOperator *op, int flag)
{
	op->flag &= ~flag;
}

/*
 * BMESH OPSTACK PUSH
 *
 * Pushes the opstack down one level 
 * and allocates a new flag layer if
 * appropriate.
 */
void BMO_push(BMesh *bm, BMOperator *UNUSED(op))
{
	bm->stackdepth++;

	/*add flag layer, if appropriate*/
	if (bm->stackdepth > 1)
		alloc_flag_layer(bm);
	else
		clear_flag_layer(bm);
}

/*
 * BMESH OPSTACK POP
 *
 * Pops the opstack one level  
 * and frees a flag layer if appropriate
 * BMESH_TODO: investigate NOT freeing flag
 * layers.
 */
void BMO_pop(BMesh *bm)
{
	if(bm->stackdepth > 1)
		free_flag_layer(bm);

	bm->stackdepth--;
}

/*
 * BMESH OPSTACK INIT OP
 *
 * Initializes an operator structure  
 * to a certain type
 */
void BMO_Init_Op(BMesh *bm, BMOperator *op, const char *opname)
{
	int i, opcode = bmesh_opname_to_opcode(opname);

#ifdef DEBUG
	BM_ELEM_INDEX_VALIDATE(bm, "pre bmo", opname);
#else
	(void)bm;
#endif

	if (opcode == -1) {
		opcode= 0; /* error!, already printed, have a better way to handle this? */
	}

	memset(op, 0, sizeof(BMOperator));
	op->type = opcode;
	op->flag = opdefines[opcode]->flag;
	
	/*initialize the operator slot types*/
	for(i = 0; opdefines[opcode]->slottypes[i].type; i++) {
		op->slots[i].slottype = opdefines[opcode]->slottypes[i].type;
		op->slots[i].index = i;
	}

	/*callback*/
	op->exec = opdefines[opcode]->exec;

	/*memarena, used for operator's slot buffers*/
	op->arena = BLI_memarena_new(BLI_MEMARENA_STD_BUFSIZE, "bmesh operator");
	BLI_memarena_use_calloc (op->arena);
}

/*
 * BMESH OPSTACK EXEC OP
 *
 * Executes a passed in operator. This handles
 * the allocation and freeing of temporary flag
 * layers and starting/stopping the modelling
 * loop. Can be called from other operators
 * exec callbacks as well.
 */
void BMO_Exec_Op(BMesh *bm, BMOperator *op)
{
	
	BMO_push(bm, op);

	if(bm->stackdepth == 2)
		bmesh_begin_edit(bm, op->flag);
	op->exec(bm, op);
	
	if(bm->stackdepth == 2)
		bmesh_end_edit(bm, op->flag);
	
	BMO_pop(bm);	
}

/*
 * BMESH OPSTACK FINISH OP
 *
 * Does housekeeping chores related to finishing
 * up an operator.
 */
void BMO_Finish_Op(BMesh *bm, BMOperator *op)
{
	BMOpSlot *slot;
	int i;

	for (i=0; opdefines[op->type]->slottypes[i].type; i++) {
		slot = &op->slots[i];
		if (slot->slottype == BMOP_OPSLOT_MAPPING) {
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

/*
 * BMESH OPSTACK HAS SLOT
 *
 * Returns 1 if the named slot exists on the given operator,
 * otherwise returns 0.
 */
int BMO_HasSlot(BMOperator *op, const char *slotname)
{
	int slotcode = bmesh_name_to_slotcode(opdefines[op->type], slotname);
	return (slotcode >= 0);
}

/*
 * BMESH OPSTACK GET SLOT
 *
 * Returns a pointer to the slot of  
 * type 'slotcode'
 */
BMOpSlot *BMO_GetSlot(BMOperator *op, const char *slotname)
{
	int slotcode = bmesh_name_to_slotcode_check(opdefines[op->type], slotname);

	if (slotcode < 0) {
		return &BMOpEmptySlot;
	}

	return &(op->slots[slotcode]);
}

/*
 * BMESH OPSTACK COPY SLOT
 *
 * Copies data from one slot to another
 */
void BMO_CopySlot(BMOperator *source_op, BMOperator *dest_op, const char *src, const char *dst)
{
	BMOpSlot *source_slot = BMO_GetSlot(source_op, src);
	BMOpSlot *dest_slot = BMO_GetSlot(dest_op, dst);

	if(source_slot == dest_slot)
		return;

	if(source_slot->slottype != dest_slot->slottype)
		return;
	
	if (dest_slot->slottype > BMOP_OPSLOT_VEC) {
		if (dest_slot->slottype != BMOP_OPSLOT_MAPPING) {
			/*do buffer copy*/
			dest_slot->data.buf = NULL;
			dest_slot->len = source_slot->len;
			if(dest_slot->len){
				dest_slot->data.buf = BLI_memarena_alloc(dest_op->arena, BMOP_OPSLOT_TYPEINFO[dest_slot->slottype] * dest_slot->len);
				memcpy(dest_slot->data.buf, source_slot->data.buf, BMOP_OPSLOT_TYPEINFO[dest_slot->slottype] * dest_slot->len);
			}
		} else {
			GHashIterator it;
			element_mapping *srcmap, *dstmap;

			/*sanity check*/
			if (!source_slot->data.ghash) return;
			
			if (!dest_slot->data.ghash) {
				dest_slot->data.ghash = 
				      BLI_ghash_new(BLI_ghashutil_ptrhash, 
					  BLI_ghashutil_ptrcmp, "bmesh operator 2");
			}

			BLI_ghashIterator_init(&it, source_slot->data.ghash);
			for (; (srcmap=BLI_ghashIterator_getValue(&it));
			      BLI_ghashIterator_step(&it))
			{
				dstmap = BLI_memarena_alloc(dest_op->arena, 
				            sizeof(*dstmap) + srcmap->len);

				dstmap->element = srcmap->element;
				dstmap->len = srcmap->len;
				memcpy(dstmap+1, srcmap+1, srcmap->len);
				
				BLI_ghash_insert(dest_slot->data.ghash,
				                dstmap->element, dstmap);				
			}
		}
	} else {
		dest_slot->data = source_slot->data;
	}
}

/*
 * BMESH OPSTACK SET XXX
 *
 * Sets the value of a slot depending on it's type
 *
*/


void BMO_Set_Float(BMOperator *op, const char *slotname, float f)
{
	BMOpSlot *slot = BMO_GetSlot(op, slotname);
	if( !(slot->slottype == BMOP_OPSLOT_FLT) )
		return;

	slot->data.f = f;
}

void BMO_Set_Int(BMOperator *op, const char *slotname, int i)
{
	BMOpSlot *slot = BMO_GetSlot(op, slotname);
	if( !(slot->slottype == BMOP_OPSLOT_INT) )
		return;

	slot->data.i = i;
}

/*only supports square mats*/
void BMO_Set_Mat(struct BMOperator *op, const char *slotname, float *mat, int size)
{
	BMOpSlot *slot = BMO_GetSlot(op, slotname);
	if( !(slot->slottype == BMOP_OPSLOT_MAT) )
		return;

	slot->len = 4;
	slot->data.p = BLI_memarena_alloc(op->arena, sizeof(float)*4*4);
	
	if (size == 4) {
		memcpy(slot->data.p, mat, sizeof(float)*4*4);
	} else if (size == 3) {
		copy_m4_m3(slot->data.p, (float (*)[3])mat);
	} else {
		fprintf(stderr, "%s: invalid size argument %d (bmesh internal error)\n", __func__, size);

		memset(slot->data.p, 0, sizeof(float)*4*4);
		return;
	}
}

void BMO_Get_Mat4(struct BMOperator *op, const char *slotname, float mat[4][4])
{
	BMOpSlot *slot = BMO_GetSlot(op, slotname);
	if( !(slot->slottype == BMOP_OPSLOT_MAT) )
		return;

	memcpy(mat, slot->data.p, sizeof(float)*4*4);
}

void BMO_Get_Mat3(struct BMOperator *op, const char *slotname, float mat[3][3])
{
	BMOpSlot *slot = BMO_GetSlot(op, slotname);
	if( !(slot->slottype == BMOP_OPSLOT_MAT) )
		return;

	copy_m3_m4(mat, slot->data.p);
}

void BMO_Set_Pnt(BMOperator *op, const char *slotname, void *p)
{
	BMOpSlot *slot = BMO_GetSlot(op, slotname);
	if( !(slot->slottype == BMOP_OPSLOT_PNT) )
		return;

	slot->data.p = p;
}

void BMO_Set_Vec(BMOperator *op, const char *slotname, float *vec)
{
	BMOpSlot *slot = BMO_GetSlot(op, slotname);
	if( !(slot->slottype == BMOP_OPSLOT_VEC) )
		return;

	copy_v3_v3(slot->data.vec, vec);
}


float BMO_Get_Float(BMOperator *op, const char *slotname)
{
	BMOpSlot *slot = BMO_GetSlot(op, slotname);
	if( !(slot->slottype == BMOP_OPSLOT_FLT) )
		return 0.0f;

	return slot->data.f;
}

int BMO_Get_Int(BMOperator *op, const char *slotname)
{
	BMOpSlot *slot = BMO_GetSlot(op, slotname);
	if( !(slot->slottype == BMOP_OPSLOT_INT) )
		return 0;

	return slot->data.i;
}


void *BMO_Get_Pnt(BMOperator *op, const char *slotname)
{
	BMOpSlot *slot = BMO_GetSlot(op, slotname);
	if( !(slot->slottype == BMOP_OPSLOT_PNT) )
		return NULL;

	return slot->data.p;
}

void BMO_Get_Vec(BMOperator *op, const char *slotname, float *vec_out)
{
	BMOpSlot *slot = BMO_GetSlot(op, slotname);
	if( !(slot->slottype == BMOP_OPSLOT_VEC) )
		return;

	copy_v3_v3(vec_out, slot->data.vec);
}

/*
 * BMO_COUNTFLAG
 *
 * Counts the number of elements of a certain type that
 * have a specific flag set.
 *
*/

int BMO_CountFlag(BMesh *bm, int flag, const char htype)
{
	BMIter elements;
	BMHeader *e;
	int count = 0;

	if(htype & BM_VERT){
		for(e = BMIter_New(&elements, bm, BM_VERTS_OF_MESH, bm); e; e = BMIter_Step(&elements)){
			if(BMO_TestFlag(bm, e, flag))
				count++;
		}
	}
	if(htype & BM_EDGE){
		for(e = BMIter_New(&elements, bm, BM_EDGES_OF_MESH, bm); e; e = BMIter_Step(&elements)){
			if(BMO_TestFlag(bm, e, flag))
				count++;
		}
	}
	if(htype & BM_FACE){
		for(e = BMIter_New(&elements, bm, BM_FACES_OF_MESH, bm); e; e = BMIter_Step(&elements)){
			if(BMO_TestFlag(bm, e, flag))
				count++;
		}
	}

	return count;	
}

void BMO_Clear_Flag_All(BMesh *bm, BMOperator *UNUSED(op), const char htype, int flag)
{
	BMIter iter;
	BMHeader *ele;
	int i=0, types[3] = {BM_VERTS_OF_MESH, BM_EDGES_OF_MESH, BM_FACES_OF_MESH};

	for (i=0; i<3; i++) {
		if (i==0 && !(htype & BM_VERT))
			continue;
		if (i==1 && !(htype & BM_EDGE))
			continue;
		if (i==2 && !(htype & BM_FACE))
			continue;

		BM_ITER(ele, &iter, bm, types[i], NULL) {
			BMO_ClearFlag(bm, ele, flag);
		}
	}
}

int BMO_CountSlotBuf(struct BMesh *UNUSED(bm), struct BMOperator *op, const char *slotname)
{
	BMOpSlot *slot = BMO_GetSlot(op, slotname);
	
	/*check if its actually a buffer*/
	if( !(slot->slottype > BMOP_OPSLOT_VEC) )
		return 0;

	return slot->len;
}

int BMO_CountSlotMap(BMesh *UNUSED(bm), BMOperator *op, const char *slotname)
{
	BMOpSlot *slot = BMO_GetSlot(op, slotname);
	
	/*check if its actually a buffer*/
	if( !(slot->slottype == BMOP_OPSLOT_MAPPING) )
		return 0;

	return slot->data.ghash ? BLI_ghash_size(slot->data.ghash) : 0;
}

#if 0
void *BMO_Grow_Array(BMesh *bm, BMOperator *op, int slotcode, int totadd)
{
	BMOpSlot *slot = &op->slots[slotcode];
	void *tmp;
	
	/*check if its actually a buffer*/
	if( !(slot->slottype > BMOP_OPSLOT_VEC) )
		return NULL;

	if (slot->flag & BMOS_DYNAMIC_ARRAY) {
		if (slot->len >= slot->size) {
			slot->size = (slot->size+1+totadd)*2;

			tmp = slot->data.buf;
			slot->data.buf = MEM_callocN(BMOP_OPSLOT_TYPEINFO[opdefines[op->type]->slottypes[slotcode].type] * slot->size, "opslot dynamic array");
			memcpy(slot->data.buf, tmp, BMOP_OPSLOT_TYPEINFO[opdefines[op->type]->slottypes[slotcode].type] * slot->size);
			MEM_freeN(tmp);
		}

		slot->len += totadd;
	} else {
		slot->flag |= BMOS_DYNAMIC_ARRAY;
		slot->len += totadd;
		slot->size = slot->len+2;
		tmp = slot->data.buf;
		slot->data.buf = MEM_callocN(BMOP_OPSLOT_TYPEINFO[opdefines[op->type]->slottypes[slotcode].type] * slot->len, "opslot dynamic array");
		memcpy(slot->data.buf, tmp, BMOP_OPSLOT_TYPEINFO[opdefines[op->type]->slottypes[slotcode].type] * slot->len);
	}

	return slot->data.buf;
}
#endif

void BMO_Mapping_To_Flag(struct BMesh *bm, struct BMOperator *op, 
			 const char *slotname, int flag)
{
	GHashIterator it;
	BMOpSlot *slot = BMO_GetSlot(op, slotname);
	BMHeader *ele;

	/*sanity check*/
	if (slot->slottype != BMOP_OPSLOT_MAPPING) return;
	if (!slot->data.ghash) return;

	BLI_ghashIterator_init(&it, slot->data.ghash);
	for ( ; (ele=BLI_ghashIterator_getKey(&it)); BLI_ghashIterator_step(&it)) {
		BMO_SetFlag(bm, ele, flag);
	}
}

static void *alloc_slot_buffer(BMOperator *op, const char *slotname, int len)
{
	BMOpSlot *slot = BMO_GetSlot(op, slotname);

	/*check if its actually a buffer*/
	if( !(slot->slottype > BMOP_OPSLOT_VEC) )
		return NULL;
	
	slot->len = len;
	if(len)
		slot->data.buf = BLI_memarena_alloc(op->arena, BMOP_OPSLOT_TYPEINFO[slot->slottype] * len);
	return slot->data.buf;
}


/*
 *
 * BMO_ALL_TO_SLOT
 *
 * Copies all elements of a certain type into an operator slot.
 *
*/

static void BMO_All_To_Slot(BMesh *bm, BMOperator *op, const char *slotname, const char htype)
{
	BMIter elements;
	BMHeader *e;
	BMOpSlot *output = BMO_GetSlot(op, slotname);
	int totelement=0, i=0;
	
	if (htype & BM_VERT) totelement += bm->totvert;
	if (htype & BM_EDGE) totelement += bm->totedge;
	if (htype & BM_FACE) totelement += bm->totface;

	if(totelement){
		alloc_slot_buffer(op, slotname, totelement);

		if (htype & BM_VERT) {
			for (e = BMIter_New(&elements, bm, BM_VERTS_OF_MESH, bm); e; e = BMIter_Step(&elements)) {
				((BMHeader**)output->data.p)[i] = e;
				i++;
			}
		}

		if (htype & BM_EDGE) {
			for (e = BMIter_New(&elements, bm, BM_EDGES_OF_MESH, bm); e; e = BMIter_Step(&elements)) {
				((BMHeader**)output->data.p)[i] = e;
				i++;
			}
		}

		if (htype & BM_FACE) {
			for (e = BMIter_New(&elements, bm, BM_FACES_OF_MESH, bm); e; e = BMIter_Step(&elements)) {
				((BMHeader**)output->data.p)[i] = e;
				i++;
			}
		}
	}
}

/*
 *
 * BMO_HEADERFLAG_TO_SLOT
 *
 * Copies elements of a certain type, which have a certain header flag set 
 * into a slot for an operator.
 *
*/

void BMO_HeaderFlag_To_Slot(BMesh *bm, BMOperator *op, const char *slotname,
                            const char hflag, const char htype)
{
	BMIter elements;
	BMHeader *e;
	BMOpSlot *output = BMO_GetSlot(op, slotname);
	int totelement=0, i=0;
	
	totelement = BM_CountFlag(bm, htype, hflag, 1);

	if(totelement){
		alloc_slot_buffer(op, slotname, totelement);

		if (htype & BM_VERT) {
			for (e = BMIter_New(&elements, bm, BM_VERTS_OF_MESH, bm); e; e = BMIter_Step(&elements)) {
				if(!BM_TestHFlag(e, BM_HIDDEN) && BM_TestHFlag(e, hflag)) {
					((BMHeader**)output->data.p)[i] = e;
					i++;
				}
			}
		}

		if (htype & BM_EDGE) {
			for (e = BMIter_New(&elements, bm, BM_EDGES_OF_MESH, bm); e; e = BMIter_Step(&elements)) {
				if(!BM_TestHFlag(e, BM_HIDDEN) && BM_TestHFlag(e, hflag)) {
					((BMHeader**)output->data.p)[i] = e;
					i++;
				}
			}
		}

		if (htype & BM_FACE) {
			for (e = BMIter_New(&elements, bm, BM_FACES_OF_MESH, bm); e; e = BMIter_Step(&elements)) {
				if(!BM_TestHFlag(e, BM_HIDDEN) && BM_TestHFlag(e, hflag)) {
					((BMHeader**)output->data.p)[i] = e;
					i++;
				}
			}
		}
	} else {
		output->len = 0;
	}
}

/*
 *
 * BMO_FLAG_TO_SLOT
 *
 * Copies elements of a certain type, which have a certain flag set 
 * into an output slot for an operator.
 */
void BMO_Flag_To_Slot(BMesh *bm, BMOperator *op, const char *slotname,
                      const int flag, const char htype)
{
	BMIter elements;
	BMHeader *e;
	BMOpSlot *output = BMO_GetSlot(op, slotname);
	int totelement = BMO_CountFlag(bm, flag, htype), i=0;

	if(totelement){
		alloc_slot_buffer(op, slotname, totelement);

		if (htype & BM_VERT) {
			for (e = BMIter_New(&elements, bm, BM_VERTS_OF_MESH, bm); e; e = BMIter_Step(&elements)) {
				if(BMO_TestFlag(bm, e, flag)){
					((BMHeader**)output->data.p)[i] = e;
					i++;
				}
			}
		}

		if (htype & BM_EDGE) {
			for (e = BMIter_New(&elements, bm, BM_EDGES_OF_MESH, bm); e; e = BMIter_Step(&elements)) {
				if(BMO_TestFlag(bm, e, flag)){
					((BMHeader**)output->data.p)[i] = e;
					i++;
				}
			}
		}

		if (htype & BM_FACE) {
			for (e = BMIter_New(&elements, bm, BM_FACES_OF_MESH, bm); e; e = BMIter_Step(&elements)) {
				if(BMO_TestFlag(bm, e, flag)){
					((BMHeader**)output->data.p)[i] = e;
					i++;
				}
			}
		}
	} else {
		output->len = 0;
	}
}

/*
 *
 * BMO_FLAG_BUFFER
 *
 * Header Flags elements in a slots buffer, automatically
 * using the selection API where appropriate.
 */
void BMO_HeaderFlag_Buffer(BMesh *bm, BMOperator *op, const char *slotname,
                           const char hflag, const char htype)
{
	BMOpSlot *slot = BMO_GetSlot(op, slotname);
	BMHeader **data =  slot->data.p;
	int i;
	
	for(i = 0; i < slot->len; i++) {
		if (!(htype & data[i]->htype))
			continue;

		if (hflag & BM_SELECT) {
			BM_Select(bm, data[i], 1);
		}
		BM_SetHFlag(data[i], hflag);
	}
}

/*
 *
 * BMO_FLAG_BUFFER
 *
 * Removes flags from elements in a slots buffer, automatically
 * using the selection API where appropriate.
 */
void BMO_UnHeaderFlag_Buffer(BMesh *bm, BMOperator *op, const char *slotname,
                             const char hflag, const char htype)
{
	BMOpSlot *slot = BMO_GetSlot(op, slotname);
	BMHeader **data =  slot->data.p;
	int i;
	
	for(i = 0; i < slot->len; i++) {
		if (!(htype & data[i]->htype))
			continue;

		if (hflag & BM_SELECT)
			BM_Select(bm, data[i], 0);
		BM_ClearHFlag(data[i], hflag);
	}
}
int BMO_Vert_CountEdgeFlags(BMesh *bm, BMVert *v, int toolflag)
{
	int count= 0;

	if(v->e) {
		BMEdge *curedge;
		const int len= bmesh_disk_count(v);
		int i;
		
		for(i = 0, curedge=v->e; i<len; i++){
			if (BMO_TestFlag(bm, curedge, toolflag))
				count++;
			curedge = bmesh_disk_nextedge(curedge, v);
		}
	}

	return count;
}

/*
 *
 * BMO_FLAG_BUFFER
 *
 * Flags elements in a slots buffer
 */
void BMO_Flag_Buffer(BMesh *bm, BMOperator *op, const char *slotname,
                     const int hflag, const char htype)
{
	BMOpSlot *slot = BMO_GetSlot(op, slotname);
	BMHeader **data =  slot->data.p;
	int i;
	
	for(i = 0; i < slot->len; i++) {
		if (!(htype & data[i]->htype))
			continue;

		BMO_SetFlag(bm, data[i], hflag);
	}
}

/*
 *
 * BMO_FLAG_BUFFER
 *
 * Removes flags from elements in a slots buffer
 */
void BMO_Unflag_Buffer(BMesh *bm, BMOperator *op, const char *slotname,
                       const int flag, const char htype)
{
	BMOpSlot *slot = BMO_GetSlot(op, slotname);
	BMHeader **data =  slot->data.p;
	int i;
	
	for(i = 0; i < slot->len; i++) {
		if (!(htype & data[i]->htype))
			continue;

		BMO_ClearFlag(bm, data[i], flag);
	}
}


/*
 *
 *	ALLOC/FREE FLAG LAYER
 *
 *  Used by operator stack to free/allocate 
 *  private flag data. This is allocated
 *  using a mempool so the allocation/frees
 *  should be quite fast.
 *
 *  BMESH_TODO:
 *	Investigate not freeing flag layers until
 *  all operators have been executed. This would
 *  save a lot of realloc potentially.
 */
static void alloc_flag_layer(BMesh *bm)
{
	BMHeader *ele;
	/* set the index values since we are looping over all data anyway,
	 * may save time later on */
	int i;

	BMIter iter;
	BLI_mempool *oldpool = bm->toolflagpool; 		/*old flag pool*/
	BLI_mempool *newpool;
	void *oldflags;

	/* store memcpy size for reuse */
	const size_t old_totflags_size= (bm->totflags * sizeof(BMFlagLayer));
	
	bm->totflags++;

	/*allocate new flag pool*/
	bm->toolflagpool= newpool= BLI_mempool_create(sizeof(BMFlagLayer)*bm->totflags, 512, 512, FALSE, FALSE);
	
	/*now go through and memcpy all the flags. Loops don't get a flag layer at this time...*/
	for (ele = BMIter_New(&iter, bm, BM_VERTS_OF_MESH, bm), i = 0; ele; ele = BMIter_Step(&iter), i++) {
		oldflags = ele->flags;
		ele->flags = BLI_mempool_calloc(newpool);
		memcpy(ele->flags, oldflags, old_totflags_size);
		BM_SetIndex(ele, i); /* set_inline */
	}
	for (ele = BMIter_New(&iter, bm, BM_EDGES_OF_MESH, bm), i = 0; ele; ele = BMIter_Step(&iter), i++) {
		oldflags = ele->flags;
		ele->flags = BLI_mempool_calloc(newpool);
		memcpy(ele->flags, oldflags, old_totflags_size);
		BM_SetIndex(ele, i); /* set_inline */
	}
	for (ele = BMIter_New(&iter, bm, BM_FACES_OF_MESH, bm), i = 0; ele; ele = BMIter_Step(&iter), i++) {
		oldflags = ele->flags;
		ele->flags = BLI_mempool_calloc(newpool);
		memcpy(ele->flags, oldflags, old_totflags_size);
		BM_SetIndex(ele, i); /* set_inline */
	}

	bm->elem_index_dirty &= ~(BM_VERT|BM_EDGE|BM_FACE);

	BLI_mempool_destroy(oldpool);
}

static void free_flag_layer(BMesh *bm)
{
	BMHeader *ele;
	/* set the index values since we are looping over all data anyway,
	 * may save time later on */
	int i;

	BMIter iter;
	BLI_mempool *oldpool = bm->toolflagpool;
	BLI_mempool *newpool;
	void *oldflags;
	
	/* store memcpy size for reuse */
	const size_t new_totflags_size= ((bm->totflags-1) * sizeof(BMFlagLayer));

	/*de-increment the totflags first...*/
	bm->totflags--;
	/*allocate new flag pool*/
	bm->toolflagpool= newpool= BLI_mempool_create(new_totflags_size, 512, 512, TRUE, FALSE);
	
	/*now go through and memcpy all the flags*/
	for (ele = BMIter_New(&iter, bm, BM_VERTS_OF_MESH, bm), i = 0; ele; ele = BMIter_Step(&iter), i++) {
		oldflags = ele->flags;
		ele->flags = BLI_mempool_calloc(newpool);
		memcpy(ele->flags, oldflags, new_totflags_size);
		BM_SetIndex(ele, i); /* set_inline */
	}
	for (ele = BMIter_New(&iter, bm, BM_EDGES_OF_MESH, bm), i = 0; ele; ele = BMIter_Step(&iter), i++) {
		oldflags = ele->flags;
		ele->flags = BLI_mempool_calloc(newpool);
		memcpy(ele->flags, oldflags, new_totflags_size);
		BM_SetIndex(ele, i); /* set_inline */
	}
	for (ele = BMIter_New(&iter, bm, BM_FACES_OF_MESH, bm), i = 0; ele; ele = BMIter_Step(&iter), i++) {
		oldflags = ele->flags;
		ele->flags = BLI_mempool_calloc(newpool);
		memcpy(ele->flags, oldflags, new_totflags_size);
		BM_SetIndex(ele, i); /* set_inline */
	}

	bm->elem_index_dirty &= ~(BM_VERT|BM_EDGE|BM_FACE);

	BLI_mempool_destroy(oldpool);
}

static void clear_flag_layer(BMesh *bm)
{
	BMVert *v;
	BMEdge *e;
	BMFace *f;
	
	BMIter verts;
	BMIter edges;
	BMIter faces;
	
	/*now go through and memcpy all the flags*/
	for(v = BMIter_New(&verts, bm, BM_VERTS_OF_MESH, bm); v; v = BMIter_Step(&verts)){
		memset(v->head.flags+(bm->totflags-1), 0, sizeof(BMFlagLayer));
	}
	for(e = BMIter_New(&edges, bm, BM_EDGES_OF_MESH, bm); e; e = BMIter_Step(&edges)){
		memset(e->head.flags+(bm->totflags-1), 0, sizeof(BMFlagLayer));
	}
	for(f = BMIter_New(&faces, bm, BM_FACES_OF_MESH, bm); f; f = BMIter_Step(&faces)){
		memset(f->head.flags+(bm->totflags-1), 0, sizeof(BMFlagLayer));
	}
}

void *BMO_FirstElem(BMOperator *op, const char *slotname)
{
	BMOpSlot *slot = BMO_GetSlot(op, slotname);
	
	if (slot->slottype != BMOP_OPSLOT_ELEMENT_BUF)
		return NULL;

	return slot->data.buf ? *(void**)slot->data.buf : NULL;
}

void *BMO_IterNew(BMOIter *iter, BMesh *UNUSED(bm), BMOperator *op,
		  const char *slotname, const char restrictmask)
{
	BMOpSlot *slot = BMO_GetSlot(op, slotname);

	memset(iter, 0, sizeof(BMOIter));

	iter->slot = slot;
	iter->cur = 0;
	iter->restrictmask = restrictmask;

	if (iter->slot->slottype == BMOP_OPSLOT_MAPPING) {
		if (iter->slot->data.ghash)
			BLI_ghashIterator_init(&iter->giter, slot->data.ghash);
		else return NULL;
	}

	return BMO_IterStep(iter);
}

void *BMO_IterStep(BMOIter *iter)
{
	if (iter->slot->slottype == BMOP_OPSLOT_ELEMENT_BUF) {
		BMHeader *h;

		if (iter->cur >= iter->slot->len) return NULL;

		h = ((void**)iter->slot->data.buf)[iter->cur++];
		while (!(iter->restrictmask & h->htype)) {
			if (iter->cur >= iter->slot->len) return NULL;
			h = ((void**)iter->slot->data.buf)[iter->cur++];
		}

		return h;
	} else if (iter->slot->slottype == BMOP_OPSLOT_MAPPING) {
		struct element_mapping *map; 
		void *ret = BLI_ghashIterator_getKey(&iter->giter);
		map = BLI_ghashIterator_getValue(&iter->giter);
		
		iter->val = map + 1;

		BLI_ghashIterator_step(&iter->giter);

		return ret;
	}

	return NULL;
}

/*used for iterating over mappings*/
void *BMO_IterMapVal(BMOIter *iter)
{
	return iter->val;
}

void *BMO_IterMapValp(BMOIter *iter)
{
	return *((void**)iter->val);
}

float BMO_IterMapValf(BMOIter *iter)
{
	return *((float*)iter->val);
}

/*error system*/
typedef struct bmop_error {
       struct bmop_error *next, *prev;
       int errorcode;
       BMOperator *op;
       const char *msg;
} bmop_error;

void BMO_ClearStack(BMesh *bm)
{
	while (BMO_PopError(bm, NULL, NULL));
}

void BMO_RaiseError(BMesh *bm, BMOperator *owner, int errcode, const char *msg)
{
	bmop_error *err = MEM_callocN(sizeof(bmop_error), "bmop_error");
	
	err->errorcode = errcode;
	if (!msg) msg = bmop_error_messages[errcode];
	err->msg = msg;
	err->op = owner;
	
	BLI_addhead(&bm->errorstack, err);
}

int BMO_HasError(BMesh *bm)
{
	return bm->errorstack.first != NULL;
}

/*returns error code or 0 if no error*/
int BMO_GetError(BMesh *bm, const char **msg, BMOperator **op)
{
	bmop_error *err = bm->errorstack.first;
	if (!err) return 0;

	if (msg) *msg = err->msg;
	if (op) *op = err->op;
	
	return err->errorcode;
}

int BMO_PopError(BMesh *bm, const char **msg, BMOperator **op)
{
	int errorcode = BMO_GetError(bm, msg, op);
	
	if (errorcode) {
		bmop_error *err = bm->errorstack.first;
		
		BLI_remlink(&bm->errorstack, bm->errorstack.first);
		MEM_freeN(err);
	}

	return errorcode;
}

/*
typedef struct bflag {
	const char *str;
	int flag;
} bflag;

#define b(f) {#f, f},
static const char *bmesh_flags = {
	b(BM_SELECT);
	b(BM_SEAM);
	b(BM_FGON);
	b(BM_HIDDEN);
	b(BM_SHARP);
	b(BM_SMOOTH);
	{NULL, 0};
};

int bmesh_str_to_flag(const char *str)
{
	int i;

	while (bmesh_flags[i]->name) {
		if (!strcmp(bmesh_flags[i]->name, str))
			return bmesh_flags[i]->flag;
	}

	return -1;
}
*/

//example:
//BMO_CallOp(bm, "del %d %hv", DEL_ONLYFACES, BM_SELECT);
/*
  d - int
  i - int
  f - float
  hv - header flagged verts
  he - header flagged edges
  hf - header flagged faces
  fv - flagged verts
  fe - flagged edges
  ff - flagged faces
  
*/

#define nextc(fmt) ((fmt)[0] != 0 ? (fmt)[1] : 0)

static int bmesh_name_to_slotcode(BMOpDefine *def, const char *name)
{
	int i;

	for (i=0; def->slottypes[i].type; i++) {
		if (!strncmp(name, def->slottypes[i].name, MAX_SLOTNAME)) return i;
	}

	return -1;
}

static int bmesh_name_to_slotcode_check(BMOpDefine *def, const char *name)
{
	int i = bmesh_name_to_slotcode(def, name);
	if (i < 0) {
		fprintf(stderr, "%s: ! could not find bmesh slot for name %s! (bmesh internal error)\n", __func__, name);
	}

	return i;
}

static int bmesh_opname_to_opcode(const char *opname)
{
	int i;

	for (i=0; i<bmesh_total_ops; i++) {
		if (!strcmp(opname, opdefines[i]->name)) return i;
	}

	fprintf(stderr, "%s: ! could not find bmesh slot for name %s! (bmesh internal error)\n", __func__, opname);
	return -1;
}

int BMO_VInitOpf(BMesh *bm, BMOperator *op, const char *_fmt, va_list vlist)
{
	BMOpDefine *def;
	char *opname, *ofmt, *fmt;
	char slotname[64] = {0};
	int i /*, n=strlen(fmt) */, stop /*, slotcode = -1 */, ret, type, state;
	int noslot=0;


	/* basic useful info to help find where bmop formatting strings fail */
	int lineno= -1;
#   define GOTO_ERROR { lineno= __LINE__; goto error; }


	/*we muck around in here, so dup it*/
	fmt = ofmt = BLI_strdup(_fmt);
	
	/*find operator name*/
	i = strcspn(fmt, " \t");

	opname = fmt;
	if (!opname[i]) noslot = 1;
	opname[i] = '\0';

	fmt += i + (noslot ? 0 : 1);
	
	i= bmesh_opname_to_opcode(opname);

	if (i == -1) {
		MEM_freeN(ofmt);
		return 0;
	}

	BMO_Init_Op(bm, op, opname);
	def = opdefines[i];
	
	i = 0;
	state = 1; //0: not inside slotcode name, 1: inside slotcode name

	while (*fmt) {
		if (state) {
			/*jump past leading whitespace*/
			i = strspn(fmt, " \t");
			fmt += i;
			
			/*ignore trailing whitespace*/
			if (!fmt[i])
				break;

			/*find end of slot name.  currently this is
			  a little flexible, allowing "slot=%f", 
			  "slot %f", "slot%f", and "slot\t%f". */
			i = strcspn(fmt, "= \t%");
			if (!fmt[i]) GOTO_ERROR;

			fmt[i] = 0;

			if (bmesh_name_to_slotcode_check(def, fmt) < 0) GOTO_ERROR;
			
			BLI_strncpy(slotname, fmt, sizeof(slotname));
			
			state = 0;
			fmt += i;
		} else {
			switch (*fmt) {
			case ' ':
			case '\t':
			case '=':
			case '%':
				break;
			case 'm': {
				int size, c;
				
				c = nextc(fmt);
				fmt++;

				if (c == '3') size = 3;
				else if (c == '4') size = 4;
				else GOTO_ERROR;

				BMO_Set_Mat(op, slotname, va_arg(vlist, void*), size);
				state = 1;
				break;
			}
			case 'v': {
				BMO_Set_Vec(op, slotname, va_arg(vlist, float*));
				state = 1;
				break;
			}
			case 'e': {
				BMHeader *ele = va_arg(vlist, void*);
				BMOpSlot *slot = BMO_GetSlot(op, slotname);

				slot->data.buf = BLI_memarena_alloc(op->arena, sizeof(void*)*4);
				slot->len = 1;
				*((void**)slot->data.buf) = ele;

				state = 1;
				break;
			}
			case 's': {
				BMOperator *op2 = va_arg(vlist, void*);
				const char *slotname2 = va_arg(vlist, char*);

				BMO_CopySlot(op2, op, slotname2, slotname);
				state = 1;
				break;
			}
			case 'i':
			case 'd':
				BMO_Set_Int(op, slotname, va_arg(vlist, int));
				state = 1;
				break;
			case 'p':
				BMO_Set_Pnt(op, slotname, va_arg(vlist, void*));
				state = 1;
				break;
			case 'f':
			case 'h':
			case 'a':
				type = *fmt;

				if (nextc(fmt) == ' ' || nextc(fmt) == '\t' || 
				    nextc(fmt)==0) 
				{
					BMO_Set_Float(op,slotname,va_arg(vlist,double));
				} else {
					ret = 0;
					stop = 0;
					while (1) {
					switch (nextc(fmt)) {
						case 'f': ret |= BM_FACE;break;
						case 'e': ret |= BM_EDGE;break;
						case 'v': ret |= BM_VERT;break;
						default:
							stop = 1;
							break;
					}
					if (stop) break;
					fmt++;
					}
					
					if (type == 'h')
						BMO_HeaderFlag_To_Slot(bm, op, 
						   slotname, va_arg(vlist, int), ret);
					else if (type == 'a')
						BMO_All_To_Slot(bm, op, slotname, ret);
					else
						BMO_Flag_To_Slot(bm, op, slotname, 
							     va_arg(vlist, int), ret);
				}

				state = 1;
				break;
			default:
				fprintf(stderr,
				        "%s: unrecognized bmop format char: %c, %d in '%s'\n",
				        __func__, *fmt, (int)(fmt-ofmt), ofmt);
				break;
			}
		}
		fmt++;
	}

	MEM_freeN(ofmt);
	return 1;
error:

	/* non urgent todo - explain exactly what is failing */
	fprintf(stderr,
	        "%s: error parsing formatting string, %d in '%s'\n    see - %s:%d\n",
	        __func__, (int)(fmt-ofmt), _fmt, __FILE__, lineno);
	MEM_freeN(ofmt);

	BMO_Finish_Op(bm, op);
	return 0;

#undef GOTO_ERROR

}


int BMO_InitOpf(BMesh *bm, BMOperator *op, const char *fmt, ...)
{
	va_list list;

	va_start(list, fmt);
	if (!BMO_VInitOpf(bm, op, fmt, list)) {
		printf("%s: failed\n", __func__);
		va_end(list);
		return 0;
	}
	va_end(list);

	return 1;
}

int BMO_CallOpf(BMesh *bm, const char *fmt, ...)
{
	va_list list;
	BMOperator op;

	va_start(list, fmt);
	if (!BMO_VInitOpf(bm, &op, fmt, list)) {
		printf("%s: failed, format is:\n    \"%s\"\n", __func__, fmt);
		va_end(list);
		return 0;
	}

	BMO_Exec_Op(bm, &op);
	BMO_Finish_Op(bm, &op);

	va_end(list);
	return 1;
}

/*
 * BMO_TOGGLEFLAG
 *
 * Toggles a flag for a certain element
 */
#ifdef BMO_ToggleFlag
#undef BMO_ToggleFlag
#endif
static void BMO_ToggleFlag(BMesh *bm, void *element, int flag)
{
	BMHeader *head = element;
	head->flags[bm->stackdepth-1].f ^= flag;
}

/*
 * BMO_SETFLAG
 *
 * Sets a flag for a certain element
 */
#ifdef BMO_SetFlag
#undef BMO_SetFlag
#endif
static void BMO_SetFlag(BMesh *bm, void *element, const int flag)
{
	BMHeader *head= element;
	head->flags[bm->stackdepth-1].f |= flag;
}

/*
 * BMO_CLEARFLAG
 *
 * Clears a specific flag from a given element
 */
#ifdef BMO_ClearFlag
#undef BMO_ClearFlag
#endif
static void BMO_ClearFlag(BMesh *bm, void *element, const int flag)
{
	BMHeader *head= element;
	head->flags[bm->stackdepth-1].f &= ~flag;
}

/*
 * BMO_TESTFLAG
 *
 * Tests whether or not a flag is set for a specific element
 *
 */
#ifdef BMO_TestFlag
#undef BMO_TestFlag
#endif
static int BMO_TestFlag(BMesh *bm, void *element, int flag)
{
	BMHeader *head = element;
	if(head->flags[bm->stackdepth-1].f & flag)
		return 1;
	return 0;
}
