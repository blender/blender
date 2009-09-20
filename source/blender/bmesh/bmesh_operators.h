#ifndef BM_OPERATORS_H
#define BM_OPERATORS_H

/*--------defines/enumerations for specific operators-------*/

/*del operator "context" slot values*/
enum {
	DEL_VERTS = 1,
	DEL_EDGES,
	DEL_ONLYFACES,
	DEL_EDGESFACES,
	DEL_FACES,
	DEL_ALL	,
	DEL_ONLYTAGGED,
};

/*quad innervert values*/
enum {
	SUBD_INNERVERT,
	SUBD_PATH,
	SUBD_FAN,
	SUBD_STRAIGHT_CUT,
};

/* similar face selection slot values */
enum {
	SIMFACE_MATERIAL = 201,
	SIMFACE_IMAGE,
	SIMFACE_AREA,
	SIMFACE_PERIMETER,
	SIMFACE_NORMAL,
	SIMFACE_COPLANAR,
};

/* similar edge selection slot values */
enum {
	SIMEDGE_LENGTH = 101,
	SIMEDGE_DIR,
	SIMEDGE_FACE,
	SIMEDGE_FACE_ANGLE,
	SIMEDGE_CREASE,
	SIMEDGE_SEAM,
	SIMEDGE_SHARP,
};

/* similar vertex selection slot values */
enum {
	SIMVERT_NORMAL = 0,
	SIMVERT_FACE,
	SIMVERT_VGROUP,
};

enum {
	OPUVC_AXIS_X = 1,
	OPUVC_AXIS_Y
};

enum {
	DIRECTION_CW = 1,
	DIRECTION_CCW
};

/* vertex path selection values */
enum {
	VPATH_SELECT_EDGE_LENGTH = 0,
	VPATH_SELECT_TOPOLOGICAL
};

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
void BM_esubdivideflag(struct Object *obedit, BMesh *bm, int flag, float smooth, 
		       float fractal, int beauty, int numcuts, int seltype,
		       int cornertype, int singleedge, int gridfill);
void BM_extrudefaceflag(BMesh *bm, int flag);

/*this next one return 1 if they did anything, or zero otherwise.
  they're kindof a hackish way to integrate with fkey, until
  such time as fkey is completely bmeshafied.*/
/*this doesn't display errors to the user, btw*/
int BM_ConnectVerts(struct EditMesh *em, int flag);

#endif
