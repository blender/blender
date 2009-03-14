#ifndef BM_OPERATORS_H
#define BM_OPERATORS_H

/*slot names and operator names appear in comments next to/above tbeir
  enumeration/define value*/

/*------------begin operator defines (see bmesh_opdefines.c too)------------*/

/*split*/
#define BMOP_SPLIT				0

/*the split operator.  splits geometry from the mesh.*/
enum {
	BMOP_SPLIT_MULTIN, /*geom*/
	BMOP_SPLIT_MULTOUT, /*geomout*/

	//bounding edges of split faces
	BMOP_SPLIT_BOUNDS_EDGEMAP, /*boundarymap*/
	BMOP_SPLIT_TOTSLOT,
};

/*dupe*/
#define BMOP_DUPE	1

/*duplicates input geometry, and creates a bounds mapping between old/new
  edges along the boundary.*/
enum {
	BMOP_DUPE_MULTIN, /*geom*/
	BMOP_DUPE_ORIG, /*origout*/
	BMOP_DUPE_NEW, /*newout*/
	/*we need a map for verts duplicated not connected
	  to any faces, too.*/	
	BMOP_DUPE_BOUNDS_EDGEMAP, /*boundarymap*/
	BMOP_DUPE_TOTSLOT
};

/*del*/
#define BMOP_DEL	2

/*deletes input geometry, using on of several deletion methods
  specified by context.*/
enum {
	BMOP_DEL_MULTIN, /*geom*/
	BMOP_DEL_CONTEXT, /*context*/
	BMOP_DEL_TOTSLOT,
};

/*context slot values*/
#define DEL_VERTS		1
#define DEL_EDGES		2
#define DEL_ONLYFACES	3
#define DEL_EDGESFACES	4
#define DEL_FACES		5
#define DEL_ALL			6
#define DEL_ONLYTAGGED		7

/*editmesh_to_bmesh*/
#define BMOP_FROM_EDITMESH		3

/*editmesh->bmesh op*/
enum {
	BMOP_FROM_EDITMESH_EM, /*em*/
	
	/*maps old elements to new ones.
	 coud do new elements to old too,
	 in the future*/
	BMOP_FROM_EDITMESH_MAP,
	BMOP_FROM_EDITMESH_TOTSLOT,
};

/*bmesh_to_editmesh*/
#define BMOP_TO_EDITMESH		4

/*bmesh->editmesh op*/
enum {
	BMOP_TO_EDITMESH_EMOUT, /*emout*/
	BMOP_TO_EDITMESH_TOTSLOT,
};

/*esubd*/
#define BMOP_ESUBDIVIDE			5

/*edge subdivide op*/
enum {
	BMOP_ESUBDIVIDE_EDGES, /*edges*/
	BMOP_ESUBDIVIDE_NUMCUTS, /*numcuts*/

	//beauty flag in esubdivide
	BMOP_ESUBDIVIDE_FLAG, /*flag*/
	BMOP_ESUBDIVIDE_RADIUS, /*radius*/

	BMOP_ESUBDIVIDE_CUSTOMFILL_FACEMAP, /*custompatterns*/
	BMOP_ESUBDIVIDE_PERCENT_EDGEMAP, /*edgepercents*/

	/*inner verts/new faces of completely filled faces, e.g.
	  fully selected face.*/
	BMOP_ESUBDIVIDE_INNER_MULTOUT, /*outinner*/

	/*new edges and vertices from splitting original edges,
	  doesn't include edges creating by connecting verts.*/
	BMOP_ESUBDIVIDE_SPLIT_MULTOUT, /*outsplit*/	
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

/*triangle tesselator op*/
enum {
	BMOP_TRIANG_FACEIN, /*faces*/
	BMOP_TRIANG_NEW_EDGES, /*edgeout*/
	BMOP_TRIANG_NEW_FACES, /*faceout*/
	BMOP_TRIANG_TOTSLOT,
};

/*dissolvefaces*/
#define BMOP_DISSOLVE_FACES		7

/*dissolve faces*/
enum {
	BMOP_DISFACES_FACEIN,
	//list of faces that comprise regions of split faces
	BMOP_DISFACES_REGIONOUT,
	BMOP_DISFACES_TOTSLOT,
};

/*dissolveverts*/
#define BMOP_DISSOLVE_VERTS		8

/*dissolve verts*/
enum {
	BMOP_DISVERTS_VERTIN, /*verts*/
	BMOP_DISVERTS_TOTSLOT,
};

/*makefgon*/
#define BMOP_MAKE_FGONS			9

#define BMOP_MAKE_FGONS_TOTSLOT	0

/*extrudefaceregion*/
#define BMOP_EXTRUDE_EDGECONTEXT	10
enum {
	BMOP_EXFACE_EDGEFACEIN, /*edgefacein*/
	
	//exclude edges from skirt connecting
	BMOP_EXFACE_EXCLUDEMAP,  /*exclude*/
	
	//new geometry
	BMOP_EXFACE_MULTOUT,  /*geomout*/
	BMOP_EXFACE_TOTSLOT,
};

/*connectverts*/
#define BMOP_CONNECT_VERTS		11
enum {
	BM_CONVERTS_VERTIN, /*verts*/
	BM_CONVERTS_EDGEOUT, /*edgeout*/
	BM_CONVERTS_TOTSLOT
};

/*keep this updated!*/
#define BMOP_TOTAL_OPS			12
/*-------------------------------end operator defines------------------------*/

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

/*this next one return 1 if they did anything, or zero otherwise.
  they're kindof a hackish way to integrate with fkey, until
  such time as fkey is completely bmeshafied.*/
/*this doesn't display errors to the user, btw*/
int BM_ConnectVerts(struct EditMesh *em, int flag);

#endif
