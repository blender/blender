#ifndef BMESH_OPERATORS_H
#define BMESH_OPERATORS_H

#include "BLI_memarena.h"

#define BMOP_OPSLOT_INT			0
#define BMOP_OPSLOT_FLT			1
#define BMOP_OPSLOT_PNT			2
#define BMOP_OPSLOT_VEC			3

/*after BMOP_OPSLOT_VEC, everything is 
  dynamically allocated arrays*/
#define BMOP_OPSLOT_INT_BUF		10	
#define BMOP_OPSLOT_FLT_BUF		11		
#define BMOP_OPSLOT_PNT_BUF		12	
#define BMOP_OPSLOT_TYPES		13

typedef struct BMOpSlot{
	int slottype;
	int len;
	int index; /*index within slot array*/
	union {
		int	i;
		float f;					
		void *p;					
		float vec[3];				/*vector*/
		void *buf;				/*buffer*/
	} data;
}BMOpSlot;

/*not sure if this is correct, which is why it's a macro, nicely
  visible here in the header.*/
#define BMO_Connect(s1, s2) (s2->data = s1->data)

/*operators represent logical, executable mesh modules.*/
#define BMOP_MAX_SLOTS			16		/*way more than probably needed*/

typedef struct BMOperator{
	int type;
	int slottype;							
	struct BMOpSlot slots[BMOP_MAX_SLOTS];
	void (*exec)(struct BMesh *bm, struct BMOperator *op);
	MemArena *arena;
}BMOperator;

/*API for operators*/
void BMO_Init_Op(struct BMOperator *op, int opcode);
void BMO_Exec_Op(struct BMesh *bm, struct BMOperator *op);
void BMO_Finish_Op(struct BMesh *bm, struct BMOperator *op);
void BMO_Init_Op(struct BMOpSlot *s1, struct BMOpSlot *s2);
BMOpSlot *BMO_GetSlot(struct BMOperator *op, int slotcode);
void BMO_CopySlot(struct BMOperator *source_op, struct BMOperator *dest_op, int src, int dst);
void BMO_Set_Float(struct BMOperator *op, int slotcode, float f);
void BMO_Set_Int(struct BMOperator *op, int slotcode, int i);
void BMO_Set_Pnt(struct BMOperator *op, int slotcode, void *p);
void BMO_Set_Vec(struct BMOperator *op, int slotcode, float *vec);
void BMO_SetFlag(struct BMesh *bm, struct BMHeader *head, int flag);
void BMO_ClearFlag(struct BMesh *bm, struct BMHeader *head, int flag);
int BMO_TestFlag(struct BMesh *bm, struct BMHeader *head, int flag);
int BMO_CountFlag(struct BMesh *bm, int flag, int type);
void BMO_Flag_To_Slot(struct BMesh *bm, struct BMOperator *op, int slotcode, int flag, int type);
void BMO_Flag_Buffer(struct BMesh *bm, struct BMOperator *op, int slotcode, int flag);

#define BMOP_OPSLOT_INT			0
#define BMOP_OPSLOT_FLT			1
#define BMOP_OPSLOT_PNT			2
#define BMOP_OPSLOT_VEC			3
#define BMOP_OPSLOT_INT_BUF		4		
#define BMOP_OPSLOT_FLT_BUF		5		
#define BMOP_OPSLOT_PNT_BUF		6		
#define BMOP_OPSLOT_TYPES		7

/*defines for individual operators*/
/*split op*/
#define BMOP_SPLIT				0
#define BMOP_SPLIT_MULTINPUT	0
#define BMOP_SPLIT_MULTOUTPUT	1
#define BMOP_SPLIT_TOTSLOT		2

const int BMOP_SPLIT_TYPEINFO[BMOP_SPLIT_TOTSLOT] = {
	BMOP_OPSLOT_PNT_BUF,
	BMOP_OPSLOT_PNT_BUF
};

/*----------begin operator defines--------*/
/*dupe op*/
#define BMOP_DUPE				1
#define BMOP_DUPE_MULTINPUT		0
#define BMOP_DUPE_ORIGINAL		1
#define BMOP_DUPE_NEW			2
#define BMOP_DUPE_TOTSLOT		3

const int BMOP_DUPE_TYPEINFO[BMOP_DUPE_TOTSLOT] = {
	BMOP_OPSLOT_PNT_BUF,
	BMOP_OPSLOT_PNT_BUF,
	BMOP_OPSLOT_PNT_BUF
};

/*delete op*/
#define BMOP_DEL			2
#define BMOP_DEL_MULTINPUT	0
#define BMOP_DEL_CONTEXT	1
#define BMOP_DEL_TOTSLOT	2

/*BMOP_DEL_CONTEXT*/
#define BMOP_DEL_VERTS				1
#define BMOP_DEL_EDGESFACES			2
#define BMOP_DEL_ONLYFACES			3
#define BMOP_DEL_FACES				4
#define BMOP_DEL_ALL				5

const int BMOP_DEL_TYPEINFO[BMOP_DEL_TOTSLOT] = {
	BMOP_OPSLOT_PNT_BUF,
	BMOP_OPSLOT_INT
};

/*editmesh->bmesh op*/
#define BMOP_FROM_EDITMESH		3
#define BMOP_FROM_EDITMESH_EM	0
#define BMOP_FROM_EDITMESH_TOTSLOT 1

const int BMOP_FROM_EDITMESH_TYPEINFO[BMOP_FROM_EDITMESH_TOTSLOT] = {
	BMOP_OPSLOT_PNT
};

/*bmesh->editmesh op*/
#define BMOP_TO_EDITMESH		4
#define BMOP_TO_EDITMESH_EM		0
#define BMOP_TO_EDITMESH_TOTSLOT 1

/*keep this updated!*/
#define BMOP_TOTAL_OPS				5
/*----------end operator defines--------*/

const int BMOP_TO_EDITMESH_TYPEINFO[BMOP_TO_EDITMESH_TOTSLOT] = {
	BMOP_OPSLOT_PNT
};

/*Following arrays are used by the init functions to init operator slot typecodes*/
const int *BMOP_TYPEINFO[BMOP_TOTAL_OPS] = {
	BMOP_SPLIT_TYPEINFO,
	BMOP_DEL_TYPEINFO,
	BMOP_FROM_EDITMESH_TYPEINFO,
	BMOP_TO_EDITMESH_TYPEINFO
};

const int BMOP_TYPETOTALS[BMOP_TOTAL_OPS] = {
	BMOP_SPLIT_TOTSLOT,
	BMOP_DEL_TOTSLOT,
	BMOP_FROM_EDITMESH_TOTSLOT,
	BMOP_TO_EDITMESH_TOTSLOT
};

#endif
