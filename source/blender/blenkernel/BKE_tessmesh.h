#include "bmesh.h"

struct BMesh;
struct BMLoop;
struct DerivedMesh;
struct BMFace;

/*
ok: the EDBM module is for editmode bmesh stuff.  in contrast, the 
    BMEdit module is for code shared with blenkernel that concerns
    the BMEditMesh structure.
*/

/*this structure replaces EditMesh.
 
  through this, you get access to both the edit bmesh,
  it's tesselation, and various stuff that doesn't belong in the BMesh
  struct itself.
  
  the entire derivedmesh and modifier system works with this structure,
  and not BMesh.  Mesh->edit_bmesh stores a pointer to this structure.*/
typedef struct BMEditMesh {
	struct BMesh *bm;

	/*this is for undoing failed operations*/
	struct BMEditMesh *emcopy;
	int emcopyusers;
	
	/*we store tesselations as triplets of three loops,
	  which each define a triangle.*/
	struct BMLoop *(*looptris)[3];
	int tottri;

	/*derivedmesh stuff*/
	struct DerivedMesh *derivedFinal, *derivedCage;
	int lastDataMask;
	
	/*retopo data pointer*/
	struct RetopoPaintData *retopo_paint_data;

	/*index tables, to map indices to elements via
	  EDBM_init_index_arrays and associated functions.  don't
	  touch this or read it directly.*/
	struct BMVert **vert_index;
	struct BMEdge **edge_index;
	struct BMFace **face_index;

	/*selection mode*/
	int selectmode;

	int mat_nr;
} BMEditMesh;

void BMEdit_RecalcTesselation(BMEditMesh *tm);
BMEditMesh *BMEdit_Create(BMesh *bm);
BMEditMesh *BMEdit_Copy(BMEditMesh *tm);
void BMEdit_Free(BMEditMesh *em);
void BMEdit_UpdateLinkedCustomData(BMEditMesh *em);
