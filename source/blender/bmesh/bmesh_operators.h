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
		       float fractal, int beauty, int numcuts, int seltype);
void BM_extrudefaceflag(BMesh *bm, int flag);

/*this next one return 1 if they did anything, or zero otherwise.
  they're kindof a hackish way to integrate with fkey, until
  such time as fkey is completely bmeshafied.*/
/*this doesn't display errors to the user, btw*/
int BM_ConnectVerts(struct EditMesh *em, int flag);

#endif
