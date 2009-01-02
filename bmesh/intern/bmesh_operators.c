#include "MEM_guardedalloc.h"

#include "BLI_memarena.h"
#include "BLI_mempool.h"

#include "BKE_utildefines.h"

#include "bmesh.h"
#include "bmesh_private.h"

#include <string.h>

/*forward declarations*/
static void alloc_flag_layer(BMesh *bm);
static void free_flag_layer(BMesh *bm);

/*function pointer table*/
typedef void (*opexec)(struct BMesh *bm, struct BMOperator *op);
const opexec BMOP_OPEXEC[BMOP_TOTAL_OPS] = {
	splitop_exec,
	dupeop_exec,
	delop_exec
};

/*operator slot type information - size of one element of the type given.*/
const int BMOP_OPSLOT_TYPEINFO[BMOP_OPSLOT_TYPES] = {
	sizeof(int),
	sizeof(float),
	sizeof(void*),
	sizeof(float)*3,
	sizeof(int),
	sizeof(float),
	sizeof(void*)
};

/*
 * BMESH OPSTACK PUSH
 *
 * Pushes the opstack down one level 
 * and allocates a new flag layer if
 * appropriate.
 *
*/

void BMO_push(BMesh *bm, BMOperator *op)
{
	bm->stackdepth++;

	/*add flag layer, if appropriate*/
	if(bm->stackdepth > 1)
		alloc_flag_layer(bm);
}

/*
 * BMESH OPSTACK POP
 *
 * Pops the opstack one level  
 * and frees a flag layer if appropriate
 * TODO: investigate NOT freeing flag
 * layers.
 *
*/
void BMO_pop(BMesh *bm)
{
	bm->stackdepth--;
	if(bm->stackdepth > 1)
		free_flag_layer(bm);
}

/*
 * BMESH OPSTACK INIT OP
 *
 * Initializes an operator structure  
 * to a certain type
 *
*/

void BMO_Init_Op(BMOperator *op, int opcode)
{
	int i;

	memset(op, 0, sizeof(BMOperator));
	op->type = opcode;

	if (BMOP_OPTIONS[opcode] & NEEDFLAGS) op->needflag = 1;

	/*initialize the operator slot types*/
	for(i = 0; i < BMOP_TYPETOTALS[opcode]; i++) {
		op->slots[i].slottype = BMOP_TYPEINFO[opcode][i];
		op->slots[i].index = i;
	}

	/*callback*/
	op->exec = BMOP_OPEXEC[opcode];

	/*memarena, used for operator's slot buffers*/
	op->arena = BLI_memarena_new(BLI_MEMARENA_STD_BUFSIZE);
	BLI_memarena_use_calloc (op->arena);
}

/*
 *	BMESH OPSTACK EXEC OP
 *
 *  Executes a passed in operator. This handles
 *  the allocation and freeing of temporary flag
 *  layers and starting/stopping the modelling
 *  loop. Can be called from other operators
 *  exec callbacks as well.
 * 
*/

void BMO_Exec_Op(BMesh *bm, BMOperator *op)
{
	
	BMO_push(bm, op);

	if(bm->stackdepth == 1)
		bmesh_begin_edit(bm);
	op->exec(bm, op);
	
	if(bm->stackdepth == 1)
		bmesh_end_edit(bm,0);
	
	BMO_pop(bm);	
}

/*
 *  BMESH OPSTACK FINISH OP
 *
 *  Does housekeeping chores related to finishing
 *  up an operator.
 *
*/

void BMO_Finish_Op(BMesh *bm, BMOperator *op)
{
	BLI_memarena_free(op->arena);
}

/*
 * BMESH OPSTACK GET SLOT
 *
 * Returns a pointer to the slot of  
 * type 'slotcode'
 *
*/

BMOpSlot *BMO_GetSlot(BMOperator *op, int slotcode)
{
	return &(op->slots[slotcode]);
}

/*
 * BMESH OPSTACK COPY SLOT
 *
 * Copies data from one slot to another 
 *
*/

void BMO_CopySlot(BMOperator *source_op, BMOperator *dest_op, int src, int dst)
{
	BMOpSlot *source_slot = &source_op->slots[src];
	BMOpSlot *dest_slot = &dest_op->slots[dst];

	if(source_slot == dest_slot)
		return;

	if(source_slot->slottype != dest_slot->slottype)
		return;
	
	if(dest_slot->slottype > BMOP_OPSLOT_VEC){
		/*do buffer copy*/
		dest_slot->data.buf = NULL;
		dest_slot->len = source_slot->len;
		if(dest_slot->len){
			dest_slot->data.buf = BLI_memarena_alloc(dest_op->arena, BMOP_TYPETOTALS[dest_slot->slottype] * dest_slot->len);
			memcpy(dest_slot->data.buf, source_slot->data.buf, BMOP_TYPETOTALS[dest_slot->slottype] * dest_slot->len);
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


void BMO_Set_Float(BMOperator *op, int slotcode, float f)
{
	if( !(op->slots[slotcode].slottype == BMOP_OPSLOT_FLT) )
		return;
	op->slots[slotcode].data.f = f;
}

void BMO_Set_Int(BMOperator *op, int slotcode, int i)
{
	if( !(op->slots[slotcode].slottype == BMOP_OPSLOT_INT) )
		return;
	op->slots[slotcode].data.i = i;

}
void BMO_Set_Pnt(BMOperator *op, int slotcode, void *p)
{
	if( !(op->slots[slotcode].slottype == BMOP_OPSLOT_PNT) )
		return;
	op->slots[slotcode].data.p = p;
}
void BMO_Set_Vec(BMOperator *op, int slotcode, float *vec)
{
	if( !(op->slots[slotcode].slottype == BMOP_OPSLOT_VEC) )
		return;

	VECCOPY(op->slots[slotcode].data.vec, vec);
}

/*
 * BMO_SETFLAG
 *
 * Sets a flag for a certain element
 *
*/
void BMO_SetFlag(BMesh *bm, void *element, int flag)
{
	BMHeader *head = element;
	head->flags[bm->stackdepth].mask |= flag;
}

/*
 * BMO_CLEARFLAG
 *
 * Clears a specific flag from a given element
 *
*/

void BMO_ClearFlag(BMesh *bm, void *element, int flag)
{
	BMHeader *head = element;
	head->flags[bm->stackdepth].mask &= ~flag;
}

/*
 * BMO_TESTFLAG
 *
 * Tests whether or not a flag is set for a specific element
 *
 *
*/

int BMO_TestFlag(BMesh *bm, void *element, int flag)
{
	BMHeader *head = element;
	if(head->flags[bm->stackdepth].mask & flag)
		return 1;
	return 0;
}

/*
 * BMO_COUNTFLAG
 *
 * Counts the number of elements of a certain type that
 * have a specific flag set.
 *
*/

int BMO_CountFlag(BMesh *bm, int flag, int type)
{
	BMIter elements;
	BMHeader *e;
	int count = 0;

	if(type & BM_VERT){
		for(e = BMIter_New(&elements, bm, BM_VERTS, bm); e; e = BMIter_Step(&elements)){
			if(BMO_TestFlag(bm, e, flag))
				count++;
		}
	}
	if(type & BM_EDGE){
		for(e = BMIter_New(&elements, bm, BM_EDGES, bm); e; e = BMIter_Step(&elements)){
			if(BMO_TestFlag(bm, e, flag))
				count++;
		}
	}
	if(type & BM_FACE){
		for(e = BMIter_New(&elements, bm, BM_FACES, bm); e; e = BMIter_Step(&elements)){
			if(BMO_TestFlag(bm, e, flag))
				count++;
		}
	}

	return count;	
}

static void *alloc_slot_buffer(BMOperator *op, int slotcode, int len){

	/*check if its actually a buffer*/
	if( !(op->slots[slotcode].slottype > BMOP_OPSLOT_VEC) )
		return NULL;
	
	op->slots[slotcode].len = len;
	if(len)
		op->slots[slotcode].data.buf = BLI_memarena_alloc(op->arena, BMOP_TYPEINFO[op->type][slotcode] * len);
	return op->slots[slotcode].data.buf;
}

/*
 *
 * BMO_FLAG_TO_SLOT
 *
 * Copies elements of a certain type, which have a certain flag set 
 * into an output slot for an operator.
 *
*/

void BMO_Flag_To_Slot(BMesh *bm, BMOperator *op, int slotcode, int flag, int type)
{
	BMIter elements;
	BMHeader *e;
	BMOpSlot *output = BMO_GetSlot(op, slotcode);
	int totelement = BMO_CountFlag(bm, flag, type), i=0;

	if(totelement){
		alloc_slot_buffer(op, slotcode, totelement);

		if (type & BM_VERT) {
			for (e = BMIter_New(&elements, bm, BM_VERTS, bm); e; e = BMIter_Step(&elements)) {
				if(BMO_TestFlag(bm, e, flag)){
					((BMHeader**)output->data.p)[i] = e;
					i++;
				}
			}
		}

		if (type & BM_EDGE) {
			for (e = BMIter_New(&elements, bm, BM_EDGES, bm); e; e = BMIter_Step(&elements)) {
				if(BMO_TestFlag(bm, e, flag)){
					((BMHeader**)output->data.p)[i] = e;
					i++;
				}
			}
		}

		if (type & BM_FACE) {
			for (e = BMIter_New(&elements, bm, BM_FACES, bm); e; e = BMIter_Step(&elements)) {
				if(BMO_TestFlag(bm, e, flag)){
					((BMHeader**)output->data.p)[i] = e;
					i++;
				}
			}
		}
	}
}

/*
 *
 * BMO_FLAG_BUFFER
 *
 * Flags elements in a slots buffer
 *
*/

void BMO_Flag_Buffer(BMesh *bm, BMOperator *op, int slotcode, int flag)
{
	BMOpSlot *slot = BMO_GetSlot(op, slotcode);
	BMHeader **data =  slot->data.p;
	int i;
	
	for(i = 0; i < slot->len; i++)
		BMO_SetFlag(bm, data[i], flag);
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
 *  TODO:
 *	Investigate not freeing flag layers until
 *  all operators have been executed. This would
 *  save a lot of realloc potentially.
 *
*/

static void alloc_flag_layer(BMesh *bm)
{
	BMVert *v;
	BMEdge *e;
	BMFace *f;

	BMIter verts;
	BMIter edges;
	BMIter faces;
	BLI_mempool *oldpool = bm->flagpool; 		/*old flag pool*/
	void *oldflags;
	
	/*allocate new flag pool*/
	bm->flagpool = BLI_mempool_create(sizeof(BMFlagLayer)*(bm->totflags+1), 512, 512 );
	
	/*now go through and memcpy all the flags. Loops don't get a flag layer at this time...*/
	for(v = BMIter_New(&verts, bm, BM_VERTS, bm); v; v = BMIter_Step(&verts)){
		oldflags = v->head.flags;
		v->head.flags = BLI_mempool_alloc(bm->flagpool);
		memcpy(v->head.flags, oldflags, sizeof(BMFlagLayer)*bm->totflags); /*dont know if this memcpy usage is correct*/
	}
	for(e = BMIter_New(&edges, bm, BM_EDGES, bm); e; e = BMIter_Step(&edges)){
		oldflags = e->head.flags;
		e->head.flags = BLI_mempool_alloc(bm->flagpool);
		memcpy(e->head.flags, oldflags, sizeof(BMFlagLayer)*bm->totflags);
	}
	for(f = BMIter_New(&faces, bm, BM_FACES, bm); f; f = BMIter_Step(&faces)){
		oldflags = f->head.flags;
		f->head.flags = BLI_mempool_alloc(bm->flagpool);
		memcpy(f->head.flags, oldflags, sizeof(BMFlagLayer)*bm->totflags);
	}
	bm->totflags++;
	BLI_mempool_destroy(oldpool);
}

static void free_flag_layer(BMesh *bm)
{
	BMVert *v;
	BMEdge *e;
	BMFace *f;

	BMIter verts;
	BMIter edges;
	BMIter faces;
	BLI_mempool *oldpool = bm->flagpool;
	void *oldflags;
	
	/*de-increment the totflags first...*/
	bm->totflags--;
	/*allocate new flag pool*/
	bm->flagpool = BLI_mempool_create(sizeof(BMFlagLayer)*bm->totflags, 512, 512);
	
	/*now go through and memcpy all the flags*/
	for(v = BMIter_New(&verts, bm, BM_VERTS, bm); v; v = BMIter_Step(&verts)){
		oldflags = v->head.flags;
		v->head.flags = BLI_mempool_alloc(bm->flagpool);
		memcpy(v->head.flags, oldflags, sizeof(BMFlagLayer)*bm->totflags);  /*correct?*/
	}
	for(e = BMIter_New(&edges, bm, BM_EDGES, bm); e; e = BMIter_Step(&edges)){
		oldflags = e->head.flags;
		e->head.flags = BLI_mempool_alloc(bm->flagpool);
		memcpy(e->head.flags, oldflags, sizeof(BMFlagLayer)*bm->totflags);
	}
	for(f = BMIter_New(&faces, bm, BM_FACES, bm); f; f = BMIter_Step(&faces)){
		oldflags = f->head.flags;
		f->head.flags = BLI_mempool_alloc(bm->flagpool);
		memcpy(f->head.flags, oldflags, sizeof(BMFlagLayer)*bm->totflags);
	}

	BLI_mempool_destroy(oldpool);
}
