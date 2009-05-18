#include "bmesh.h"

struct BMesh;
struct BMLoop;
struct DerivedMesh;
struct BMFace;

typedef struct BMEditSelection
{
	struct BMEditSelection *next, *prev;
	short type;
	void *data;
} BMEditSelection;

/*this structure replaces EditMesh.
 
  through this, you get access to both the edit bmesh,
  it's tesselation, and various stuff that doesn't belong in the BMesh
  struct itself.
  
  the entire derivedmesh and modifier system works with this structure,
  and not BMesh.  Mesh->editbmesh will store a pointer to this structure.*/
typedef struct BMEditMesh {
	struct BMesh *bm;
	
	/*we store tesselations as triplets of three loops,
	  which each define a triangle.*/
	struct BMLoop *(*looptris)[3];
	int tottri;

	/*derivedmesh stuff*/
	struct DerivedMesh *derivedFinal, *derivedCage;
	int lastDataMask;
	
	/*retopo data pointer*/
	struct RetopoPaintData *retopo_paint_data;

	/*active face pointer*/
	struct BMFace *act_face; 

	/*index tables, to map indices to elements via
	  EDBM_init_index_arrays and associated functions.  don't
	  touch this or read it directly.*/
	struct BMVert **vert_index;
	struct BMEdge **edge_index;
	struct BMFace **face_index;
	
	/*selection order list*/
	ListBase selected;

	/*selection mode*/
	int selectmode, totfacesel, totvertsel, totedgesel;

	int mat_nr;
} BMEditMesh;

void TM_RecalcTesselation(BMEditMesh *tm);
BMEditMesh *TM_Create(BMesh *bm);
BMEditMesh *TM_Copy(BMEditMesh *tm);
void TM_Free(BMEditMesh *em);
