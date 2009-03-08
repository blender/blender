#ifndef BM_OPERATORS_H
#define BM_OPERATORS_H

#include "BLI_memarena.h"
#include "BLI_ghash.h"

#include <stdarg.h>

#define BMOP_OPSLOT_INT			0
#define BMOP_OPSLOT_FLT			1
#define BMOP_OPSLOT_PNT			2
#define BMOP_OPSLOT_VEC			6

/*after BMOP_OPSLOT_VEC, everything is 
  dynamically allocated arrays.  we
  leave a space in the identifiers
  for future growth.*/
#define BMOP_OPSLOT_PNT_BUF		7

#define BMOP_OPSLOT_MAPPING		8
#define BMOP_OPSLOT_TYPES		9

typedef struct BMOpSlot{
	int slottype;
	int len;
	int flag;
	int index; /*index within slot array*/
	union {
		int i;
		float f;					
		void *p;					
		float vec[3];				/*vector*/
		void *buf;				/*buffer*/
		GHash *ghash;
	} data;
}BMOpSlot;

/*BMOpSlot->flag*/
/*is a dynamically-allocated array.  set at runtime.*/
#define BMOS_DYNAMIC_ARRAY	1

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

#define MAX_SLOTNAME	32

typedef struct slottype {
	int type;
	char name[MAX_SLOTNAME];
} slottype;

/*need to refactor code to use this*/
typedef struct BMOpDefine {
	char *name;
	slottype slottypes[BMOP_MAX_SLOTS];
	void (*exec)(BMesh *bm, BMOperator *op);
	int totslot;
	int flag;
} BMOpDefine;

/*BMOpDefine->flag*/
//doesn't do anything at the moment.

/*API for operators*/

/*data types that use pointers (arrays, etc) should never
  have it set directly.  and never use BMO_Set_Pnt to
  pass in a list of edges or any arrays, really.*/
void BMO_Init_Op(struct BMOperator *op, int opcode);
void BMO_Exec_Op(struct BMesh *bm, struct BMOperator *op);
void BMO_Finish_Op(struct BMesh *bm, struct BMOperator *op);
BMOpSlot *BMO_GetSlot(struct BMOperator *op, int slotcode);
void BMO_CopySlot(struct BMOperator *source_op, struct BMOperator *dest_op, int src, int dst);
void BMO_Set_Float(struct BMOperator *op, int slotcode, float f);
void BMO_Set_Int(struct BMOperator *op, int slotcode, int i);
void BMO_Set_Pnt(struct BMOperator *op, int slotcode, void *p);
void BMO_Set_Vec(struct BMOperator *op, int slotcode, float *vec);
void BMO_SetFlag(struct BMesh *bm, void *element, int flag);
void BMO_ClearFlag(struct BMesh *bm, void *element, int flag);
int BMO_TestFlag(struct BMesh *bm, void *element, int flag);
int BMO_CountFlag(struct BMesh *bm, int flag, int type);
void BMO_Flag_To_Slot(struct BMesh *bm, struct BMOperator *op, int slotcode, int flag, int type);
void BMO_Flag_Buffer(struct BMesh *bm, struct BMOperator *op, int slotcode, int flag);
void BMO_Unflag_Buffer(struct BMesh *bm, struct BMOperator *op, int slotcode, int flag);
void BMO_HeaderFlag_To_Slot(struct BMesh *bm, struct BMOperator *op, int slotcode, int flag, int type);
int BMO_CountSlotBuf(struct BMesh *bm, struct BMOperator *op, int slotcode);

/*copies data, doesn't store a reference to it.*/
void BMO_Insert_Mapping(BMesh *bm, BMOperator *op, int slotcode, 
			void *element, void *data, int len);
void BMO_Insert_MapFloat(BMesh *bm, BMOperator *op, int slotcode, 
			void *element, float val);

//returns 1 if the specified element is in the map.
int BMO_InMap(BMesh *bm, BMOperator *op, int slotcode, void *element);
void *BMO_Get_MapData(BMesh *bm, BMOperator *op, int slotcode,
		      void *element);
float BMO_Get_MapFloat(BMesh *bm, BMOperator *op, int slotcode,
		       void *element);
void BMO_Mapping_To_Flag(struct BMesh *bm, struct BMOperator *op, 
			 int slotcode, int flag);

/*do NOT use these for non-operator-api-allocated memory! instead
  use BMO_Get_MapData, which copies the data.*/
void BMO_Insert_MapPointer(BMesh *bm, BMOperator *op, int slotcode, 
			void *element, void *val);
void *BMO_Get_MapPointer(BMesh *bm, BMOperator *op, int slotcode,
		       void *element);

struct GHashIterator;
typedef struct BMOIter {
	BMOpSlot *slot;
	int cur; //for arrays
	struct GHashIterator giter;
	void *val;
} BMOIter;

void *BMO_IterNew(BMOIter *iter, BMesh *bm, BMOperator *op, 
		  int slotcode);
void *BMO_IterStep(BMOIter *iter);

/*returns a pointer to the key value when iterating over mappings.
  remember for pointer maps this will be a pointer to a pointer.*/
void *BMO_IterMapVal(BMOIter *iter);

/*formatted operator initialization/execution*/
int BMO_CallOpf(BMesh *bm, char *fmt, ...);
int BMO_InitOpf(BMesh *bm, BMOperator *op, char *fmt, ...);
int BMO_VInitOpf(BMesh *bm, BMOperator *op, char *fmt, va_list vlist);

/*----------- bmop error system ----------*/

/*pushes an error onto the bmesh error stack.
  if msg is null, then the default message for the errorcode is used.*/
void BMO_RaiseError(BMesh *bm, BMOperator *owner, int errcode, char *msg);

/*gets the topmost error from the stack.
  returns error code or 0 if no error.*/
int BMO_GetError(BMesh *bm, char **msg, BMOperator **op);
int BMO_HasError(BMesh *bm);

/*same as geterror, only pops the error off the stack as well*/
int BMO_PopError(BMesh *bm, char **msg, BMOperator **op);
void BMO_ClearStack(BMesh *bm);

#if 0
//this is meant for handling errors, like self-intersection test failures.
//it's dangerous to handle errors in general though, so disabled for now.

/*catches an error raised by the op pointed to by catchop.
  errorcode is either the errorcode, or BMERR_ALL for any 
  error.*/
int BMO_CatchOpError(BMesh *bm, BMOperator *catchop, int errorcode, char **msg);
#endif

/*------ error code defines -------*/

/*error messages*/
#define BMERR_SELF_INTERSECTING			1
#define BMERR_DISSOLVEDISK_FAILED		2
#define BMERR_CONNECTVERT_FAILED		3
#define BMERR_WALKER_FAILED			4
#define BMERR_DISSOLVEFACES_FAILED		5
#define BMERR_DISSOLVEVERTS_FAILED		6

static char *bmop_error_messages[] = {
       0,
       "Self intersection error",
       "Could not dissolve vert",
       "Could not connect verts",
       "Could not traverse mesh",
       "Could not dissolve faces",
       "Could not dissolve vertices",
};

/*------------begin operator defines (see bmesh_opdefines.c too)------------*/
/*split op*/
#define BMOP_SPLIT				0

enum {
	BMOP_SPLIT_MULTIN,
	BMOP_SPLIT_MULTOUT,
	BMOP_SPLIT_BOUNDS_EDGEMAP, //bounding edges of split faces
	BMOP_SPLIT_TOTSLOT,
};

/*dupe op*/
#define BMOP_DUPE	1

enum {
	BMOP_DUPE_MULTIN,
	BMOP_DUPE_ORIG,
	BMOP_DUPE_NEW,
	/*we need a map for verts duplicated not connected
	  to any faces, too.*/	
	BMOP_DUPE_BOUNDS_EDGEMAP,
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
#define DEL_ONLYTAGGED		7

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

	BMOP_ESUBDIVIDE_CUSTOMFILL_FACEMAP,
	BMOP_ESUBDIVIDE_PERCENT_EDGEMAP,

	/*inner verts/new faces of completely filled faces, e.g.
	  fully selected face.*/
	BMOP_ESUBDIVIDE_INNER_MULTOUT,

	/*new edges and vertices from splitting original edges,
	  doesn't include edges creating by connecting verts.*/
	BMOP_ESUBDIVIDE_SPLIT_MULTOUT,	
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
enum {
	BMOP_DISFACES_FACEIN,
	//list of faces that comprise regions of split faces
	BMOP_DISFACES_REGIONOUT,
	BMOP_DISFACES_TOTSLOT,
};

/*dissolve verts*/
#define BMOP_DISSOLVE_VERTS		8

enum {
	BMOP_DISVERTS_VERTIN,
	BMOP_DISVERTS_TOTSLOT,
};

#define BMOP_MAKE_FGONS			9
#define BMOP_MAKE_FGONS_TOTSLOT	0

#define BMOP_EXTRUDE_EDGECONTEXT	10
enum {
	BMOP_EXFACE_EDGEFACEIN,
	BMOP_EXFACE_EXCLUDEMAP, //exclude edges from skirt connecting
	BMOP_EXFACE_MULTOUT, //new geometry
	BMOP_EXFACE_TOTSLOT,
};

#define BMOP_CONNECT_VERTS		11
enum {
	BM_CONVERTS_VERTIN,
	BM_CONVERTS_EDGEOUT,
	BM_CONVERTS_TOTSLOT
};

/*keep this updated!*/
#define BMOP_TOTAL_OPS			12
/*-------------------------------end operator defines-------------------------------*/

extern BMOpDefine *opdefines[];
extern int bmesh_total_ops;

/*------specific operator helper functions-------*/

/*executes the duplicate operation, feeding elements of 
  type flag etypeflag and header flag flag to it.  note,
  to get more useful information (such as the mapping from
  original to new elements) you should run the dupe op manually.*/
struct Object;
struct EditMesh;

void BMOP_DupeFromFlag(struct BMesh *bm, int etypeflag, int flag);
void BM_esubdivideflag(struct Object *obedit, struct BMesh *bm, int selflag, float rad, 
	       int flag, int numcuts, int seltype);
void BM_extrudefaceflag(BMesh *bm, int flag);

/*these next two return 1 if they did anything, or zero otherwise.
  they're kindof a hackish way to integrate with fkey, until
  such time as fkey is completely bmeshafied.*/
int BM_DissolveFaces(struct EditMesh *em, int flag);
/*this doesn't display errors to the user, btw*/
int BM_ConnectVerts(struct EditMesh *em, int flag);

#endif
