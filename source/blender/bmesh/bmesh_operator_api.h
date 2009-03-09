#ifndef _BMESH_OPERATOR_H
#define _BMESH_OPERATOR_H

#include "BLI_memarena.h"
#include "BLI_ghash.h"

#include <stdarg.h>

/*
operators represent logical, executable mesh modules.  all operations
involving a bmesh has to go through them.

operators are nested, and certain data (e.g. tool flags) are also nested,
and are private to an operator when it's executed.
*/

struct GHashIterator;

#define BMOP_OPSLOT_INT			0
#define BMOP_OPSLOT_FLT			1
#define BMOP_OPSLOT_PNT			2
#define BMOP_OPSLOT_VEC			6

/*after BMOP_OPSLOT_VEC, everything is 
  dynamically allocated arrays.  we
  leave a space in the identifiers
  for future growth.*/
#define BMOP_OPSLOT_ELEMENT_BUF		7
#define BMOP_OPSLOT_MAPPING		8
#define BMOP_OPSLOT_TYPES		9

/*don't access the contents of this directly*/
typedef struct BMOpSlot{
	int slottype;
	int len;
	int flag;
	int index; /*index within slot array*/
	union {
		int i;
		float f;					
		void *p;					
		float vec[3];
		void *buf;
		GHash *ghash;
	} data;
}BMOpSlot;

#define BMOP_MAX_SLOTS			16 /*way more than probably needed*/

typedef struct BMOperator {
	int type;
	int slottype;
	int needflag;
	struct BMOpSlot slots[BMOP_MAX_SLOTS];
	void (*exec)(struct BMesh *bm, struct BMOperator *op);
	MemArena *arena;
} BMOperator;

#define MAX_SLOTNAME	32

typedef struct slottype {
	int type;
	char name[MAX_SLOTNAME];
} slottype;

typedef struct BMOpDefine {
	char *name;
	slottype slottypes[BMOP_MAX_SLOTS];
	void (*exec)(BMesh *bm, BMOperator *op);
	int totslot;
	int flag; /*doesn't do anything right now*/
} BMOpDefine;

/*------------- Operator API --------------*/

/*data types that use pointers (arrays, etc) should never
  have it set directly.  and never use BMO_Set_Pnt to
  pass in a list of edges or any arrays, really.*/
void BMO_Init_Op(struct BMOperator *op, int opcode);
void BMO_Exec_Op(struct BMesh *bm, struct BMOperator *op);
void BMO_Finish_Op(struct BMesh *bm, struct BMOperator *op);

/*tool flag API. never, ever ever should tool code put junk in 
  header flags (element->head.flag), nor should they use 
  element->head.eflag1/eflag2.  instead, use this api to set
  flags.  
  
  if you need to store a value per element, use a 
  ghash or a mapping slot to do it.*/
/*flags 15 and 16 (1<<14 and 1<<15) are reserved for bmesh api use*/
void BMO_SetFlag(struct BMesh *bm, void *element, int flag);
void BMO_ClearFlag(struct BMesh *bm, void *element, int flag);
int BMO_TestFlag(struct BMesh *bm, void *element, int flag);
int BMO_CountFlag(struct BMesh *bm, int flag, int type);

/*---------formatted operator initialization/execution-----------*/
/*
  this system is used to execute or initialize an operator,
  using a formatted-string system.

  for example, BMO_CallOpf(bm, "del geom=%hf context=%d", BM_SELECT, DEL_FACES);
  . . .will execute the delete operator, feeding in selected faces, deleting them.

  the basic format for the format string is:
    [operatorname] [slotname]=%[code] [slotname]=%[code]
  
  as in printf, you pass in one additional argument to the function
  for every code.

  the formatting codes are:
     %d - put int in slot
     %f - put float in float
     %h[f/e/v] - put elements with a header flag in slot.
          the letters after %h define which element types to use,
	  so e.g. %hf will do faces, %hfe will do faces and edges,
	  %hv will do verts, etc.  must pass in at least one
	  element type letter.
     %f[f/e/v] - same as %h.
*/
/*executes an operator*/
int BMO_CallOpf(BMesh *bm, char *fmt, ...);

/*initializes, but doesn't execute an operator*/
int BMO_InitOpf(BMesh *bm, BMOperator *op, char *fmt, ...);

/*va_list version, used to implement the above two functions,
   plus EDBM_CallOpf in bmeshutils.c.*/
int BMO_VInitOpf(BMesh *bm, BMOperator *op, char *fmt, va_list vlist);
/*------end of formatted op system -------*/

BMOpSlot *BMO_GetSlot(struct BMOperator *op, int slotcode);
void BMO_CopySlot(struct BMOperator *source_op, struct BMOperator *dest_op, int src, int dst);

void BMO_Set_Float(struct BMOperator *op, int slotcode, float f);
void BMO_Set_Int(struct BMOperator *op, int slotcode, int i);
/*don't pass in arrays that are supposed to map to elements this way.
  
  so, e.g. passing in list of floats per element in another slot is bad.
  passing in, e.g. pointer to an editmesh for the conversion operator is fine
  though.*/
void BMO_Set_Pnt(struct BMOperator *op, int slotcode, void *p);
void BMO_Set_Vec(struct BMOperator *op, int slotcode, float *vec);

/*puts every element of type type (which is a bitmask) with tool flag flag,
  into a slot.*/
void BMO_Flag_To_Slot(struct BMesh *bm, struct BMOperator *op, int slotcode, int flag, int type);
void BMO_Flag_Buffer(struct BMesh *bm, struct BMOperator *op, int slotcode, int flag);
void BMO_Unflag_Buffer(struct BMesh *bm, struct BMOperator *op, int slotcode, int flag);

/*puts every element of type type (which is a bitmask) with header flag 
  flag, into a slot.*/
void BMO_HeaderFlag_To_Slot(struct BMesh *bm, struct BMOperator *op, int slotcode, int flag, int type);
int BMO_CountSlotBuf(struct BMesh *bm, struct BMOperator *op, int slotcode);

/*copies data, doesn't store a reference to it.*/
void BMO_Insert_Mapping(BMesh *bm, BMOperator *op, int slotcode, 
			void *element, void *data, int len);
void BMO_Insert_MapFloat(BMesh *bm, BMOperator *op, int slotcode, 
			void *element, float val);
//returns 1 if the specified element is in the map.
int BMO_InMap(BMesh *bm, BMOperator *op, int slotcode, void *element);
void *BMO_Get_MapData(BMesh *bm, BMOperator *op, int slotcode, void *element);
float BMO_Get_MapFloat(BMesh *bm, BMOperator *op, int slotcode, void *element);
void BMO_Mapping_To_Flag(struct BMesh *bm, struct BMOperator *op, 
			 int slotcode, int flag);

/*do NOT use these for non-operator-api-allocated memory! instead
  use BMO_Get_MapData and BMO_Insert_Mapping, which copies the data.*/
void BMO_Insert_MapPointer(BMesh *bm, BMOperator *op, int slotcode, 
			void *element, void *val);
void *BMO_Get_MapPointer(BMesh *bm, BMOperator *op, int slotcode,
		       void *element);

/*contents of this structure are private,
  don't directly access.*/
typedef struct BMOIter {
	BMOpSlot *slot;
	int cur; //for arrays
	struct GHashIterator giter;
	void *val;
} BMOIter;

/*this part of the API is used to iterate over element buffer or
  mapping slots.
  
  for example, iterating over the faces in a slot is:

	  BMOIter oiter;
	  BMFace *f;

	  f = BMO_IterNew(&oiter, bm, some_operator, SOME_SLOT_CODE);
	  for (; f; f=BMO_IterStep) {
		/do something with the face
	  }

  another example, iterating over a mapping:
	  BMOIter oiter;
	  void *key;
	  void *val;

	  key = BMO_IterNew(&oiter, bm, some_operator, SOME_SLOT_CODE);
	  for (; key; key=BMO_IterStep) {
		val = BMO_IterMapVal(&oiter);
		//do something with the key/val pair
	  }

  */

void *BMO_IterNew(BMOIter *iter, BMesh *bm, BMOperator *op, 
		  int slotcode);
void *BMO_IterStep(BMOIter *iter);

/*returns a pointer to the key value when iterating over mappings.
  remember for pointer maps this will be a pointer to a pointer.*/
void *BMO_IterMapVal(BMOIter *iter);

#endif /* _BMESH_OPERATOR_H */