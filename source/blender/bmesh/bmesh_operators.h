#ifndef BM_OPERATORS_H
#define BM_OPERATORS_H

#include "BLI_memarena.h"

#define BMOP_OPSLOT_INT			0
#define BMOP_OPSLOT_FLT			1
#define BMOP_OPSLOT_PNT			2
#define BMOP_OPSLOT_VEC			6

/*after BMOP_OPSLOT_VEC, everything is 
  dynamically allocated arrays.  we
  leave a space in the identifiers
  for future growth.*/
#define BMOP_OPSLOT_INT_BUF		7
#define BMOP_OPSLOT_FLT_BUF		8		
#define BMOP_OPSLOT_PNT_BUF		9
#define BMOP_OPSLOT_TYPES		10

typedef struct BMOpSlot{
	int slottype;
	int len;
	int index; /*index within slot array*/
	union {
		int i;
		float f;					
		void *p;					
		float vec[3];				/*vector*/
		void *buf;				/*buffer*/
	} data;
}BMOpSlot;

/*operators represent logical, executable mesh modules.*/
#define BMOP_MAX_SLOTS			16		/*way more than probably needed*/

typedef struct BMOperator{
	int type;
	int slottype;
	int needflag;
	struct BMOpSlot slots[BMOP_MAX_SLOTS];
	void (*exec)(struct BMesh *bm, struct BMOperator *op);
	MemArena *arena;
}BMOperator;

/*need to refactor code to use this*/
typedef struct BMOpDefine {
	int slottypes[BMOP_MAX_SLOTS];
	void (*exec)(BMesh *bm, BMOperator *op);
	int totslot;
	int flag;
} BMOpDefine;

/*BMOpDefine->flag*/
//doesn't do anything at the moment.

/*API for operators*/
void BMO_Init_Op(struct BMOperator *op, int opcode);
void BMO_Exec_Op(struct BMesh *bm, struct BMOperator *op);
void BMO_Finish_Op(struct BMesh *bm, struct BMOperator *op);
BMOpSlot *BMO_GetSlot(struct BMOperator *op, int slotcode);
void BMO_CopySlot(struct BMOperator *source_op, struct BMOperator *dest_op, int src, int dst);
void BMO_Set_Float(struct BMOperator *op, int slotcode, float f);
void BMO_Set_Int(struct BMOperator *op, int slotcode, int i);
void BMO_Set_PntBuf(struct BMOperator *op, int slotcode, void *p, int len);
void BMO_Set_FltBuf(BMOperator *op, int slotcode, float *p, int len);
void BMO_Set_Pnt(struct BMOperator *op, int slotcode, void *p);
void BMO_Set_Vec(struct BMOperator *op, int slotcode, float *vec);
void BMO_SetFlag(struct BMesh *bm, void *element, int flag);
void BMO_ClearFlag(struct BMesh *bm, void *element, int flag);
int BMO_TestFlag(struct BMesh *bm, void *element, int flag);
int BMO_CountFlag(struct BMesh *bm, int flag, int type);
void BMO_Flag_To_Slot(struct BMesh *bm, struct BMOperator *op, int slotcode, int flag, int type);
void BMO_Flag_Buffer(struct BMesh *bm, struct BMOperator *op, int slotcode, int flag);
void BMO_HeaderFlag_To_Slot(struct BMesh *bm, struct BMOperator *op, int slotcode, int flag, int type);

/*if msg is null, then the default message for the errorcode is used*/
void BMOP_RaiseError(BMesh *bm, int errcode, char *msg);
/*returns error code or 0 if no error*/
int BMOP_GetError(BMesh *bm, char **msg);
/*returns 1 if there was an error*/
int BMOP_CheckError(BMesh *bm);
int BMOP_PopError(BMesh *bm, char **msg);

/*------ error code defines -------*/

/*error messages*/
#define BMERR_SELF_INTERSECTING        1

static char *bmop_error_messages[] = {
       0,
       "Self intersection error",
};

/*------------begin operator defines (see bmesh_opdefines.c too)------------*/
/*split op*/
#define BMOP_SPLIT				0

enum {
	BMOP_SPLIT_MULTIN,
	BMOP_SPLIT_MULTOUT,
	BMOP_SPLIT_TOTSLOT,
};

/*dupe op*/
#define BMOP_DUPE	1

enum {
	BMOP_DUPE_MULTIN,
	BMOP_DUPE_ORIG,
	BMOP_DUPE_NEW,
	BMOP_DUPE_TOTSLOT
};

/*delete op*/
#define BMOP_DEL	2

enum {
	BMOP_DEL_MULTIN,
	BMOP_DEL_CONTEXT,
	BMOP_DEL_TOTSLOT,
};

#define DEL_VERTS		1
#define DEL_EDGES		2
#define DEL_ONLYFACES	3
#define DEL_EDGESFACES	4
#define DEL_FACES		5
#define DEL_ALL			6

/*BMOP_DEL_CONTEXT*/
enum {
	BMOP_DEL_VERTS = 1,
	BMOP_DEL_EDGESFACES,
	BMOP_DEL_ONLYFACES,
	BMOP_DEL_FACES,
	BMOP_DEL_ALL,
};

/*editmesh->bmesh op*/
#define BMOP_FROM_EDITMESH		3
enum {
	BMOP_FROM_EDITMESH_EM,
	BMOP_FROM_EDITMESH_TOTSLOT,
};

#define BMOP_TO_EDITMESH		4
/*bmesh->editmesh op*/
enum {
	BMOP_TO_EDITMESH_EMOUT,
	BMOP_TO_EDITMESH_TOTSLOT,
};

/*edge subdivide op*/
#define BMOP_ESUBDIVIDE			5
enum {
	BMOP_ESUBDIVIDE_EDGES,
	BMOP_ESUBDIVIDE_NUMCUTS,
	BMOP_ESUBDIVIDE_FLAG, //beauty flag in esubdivide
	BMOP_ESUBDIVIDE_RADIUS,
	BMOP_ESUBDIVIDE_SELACTION,

	BMOP_ESUBDIVIDE_CUSTOMFILL_FACES,
	BMOP_ESUBDIVIDE_CUSTOMFILL_PATTERNS,

	BMOP_ESUBDIVIDE_PERCENT_EDGES,
	BMOP_ESUBDIVIDE_PERCENT_VALUES,

	BMOP_ESUBDIVIDE_TOTSLOT,
};
/*
SUBDIV_SELECT_INNER
SUBDIV_SELECT_ORIG
SUBDIV_SELECT_INNER_SEL
SUBDIV_SELECT_LOOPCUT
DOUBLEOPFILL
*/

/*triangulate*/
#define BMOP_TRIANGULATE		6

enum {
	BMOP_TRIANG_FACEIN,
	BMOP_TRIANG_NEW_EDGES,
	BMOP_TRIANG_NEW_FACES,
	BMOP_TRIANG_TOTSLOT,
};

/*dissolve faces*/
#define BMOP_DISSOLVE_FACES		7

#define BMOP_DISFACES_FACEIN	0
#define BMOP_DISFACES_TOTSLOT	1

/*dissolve verts*/
#define BMOP_DISSOLVE_VERTS		8

#define BMOP_DISVERTS_VERTIN	0
#define BMOP_DISVERTS_TOTSLOT	1

#define BMOP_MAKE_FGONS			9
#define BMOP_MAKE_FGONS_TOTSLOT	0

/*keep this updated!*/
#define BMOP_TOTAL_OPS			10
/*-------------------------------end operator defines-------------------------------*/

extern BMOpDefine *opdefines[];
extern int bmesh_total_ops;

/*------specific operator helper functions-------*/

/*executes the duplicate operation, feeding elements of 
  type flag etypeflag and header flag flag to it.  note,
  to get more useful information (such as the mapping from
  original to new elements) you should run the dupe op manually.*/
struct Object;

void BMOP_DupeFromFlag(struct BMesh *bm, int etypeflag, int flag);
void BM_esubdivideflag(struct Object *obedit, struct BMesh *bm, int selflag, float rad, 
		       int flag, int numcuts, int seltype);
#endif
