#include "MEM_guardedalloc.h"
#include "BKE_customdata.h" 
#include "DNA_listBase.h"
#include "DNA_customdata_types.h"
#include "DNA_mesh_types.h"
#include "DNA_meshdata_types.h"
#include "DNA_object_types.h"
#include "DNA_scene_types.h"
#include <string.h>
#include "BKE_utildefines.h"
#include "BKE_mesh.h"
#include "BKE_global.h"
#include "BKE_DerivedMesh.h"
#include "BKE_cdderivedmesh.h"

#include "BLI_editVert.h"
#include "mesh_intern.h"
#include "ED_mesh.h"

#include "BLI_arithb.h"
#include "BLI_blenlib.h"
#include "BLI_edgehash.h"

#include "bmesh.h"

/*
 * UTILS.C
 *
 * utility bmesh operators, e.g. transform, 
 * translate, rotate, scale, etc.
 *
*/

void bmesh_makevert_exec(BMesh *bm, BMOperator *op)
{
	float vec[3];

	BMO_Get_Vec(op, "co", vec);

	BMO_SetFlag(bm, BM_Make_Vert(bm, vec, NULL), 1);	
	BMO_Flag_To_Slot(bm, op, "newvertout", 1, BM_VERT);
}

void bmesh_transform_exec(BMesh *bm, BMOperator *op)
{
	BMOIter iter;
	BMVert *v;
	float mat[4][4];

	BMO_Get_Mat4(op, "mat", mat);

	BMO_ITER(v, &iter, bm, op, "verts", BM_VERT) {
		Mat4MulVecfl(mat, v->co);
	}
}

/*this operator calls the transform operator, which
  is a little complex, but makes it easier to make
  sure the transform op is working, since initially
  only this one will be used.*/
void bmesh_translate_exec(BMesh *bm, BMOperator *op)
{
	float mat[4][4], vec[3];
	
	BMO_Get_Vec(op, "vec", vec);

	Mat4One(mat);
	VECCOPY(mat[3], vec);

	BMO_CallOpf(bm, "transform mat=%m4 verts=%s", mat, op, "verts");
}

void bmesh_rotate_exec(BMesh *bm, BMOperator *op)
{
	float vec[3];
	
	BMO_Get_Vec(op, "cent", vec);
	
	/*there has to be a proper matrix way to do this, but
	  this is how editmesh did it and I'm too tired to think
	  through the math right now.*/
	VecMulf(vec, -1);
	BMO_CallOpf(bm, "translate verts=%s vec=%v", op, "verts", vec);

	BMO_CallOpf(bm, "transform mat=%s verts=%s", op, "mat", op, "verts");

	VecMulf(vec, -1);
	BMO_CallOpf(bm, "translate verts=%s vec=%v", op, "verts", vec);
}

void bmesh_reversefaces_exec(BMesh *bm, BMOperator *op)
{
	BMOIter siter;
	BMFace *f;

	BMO_ITER(f, &siter, bm, op, "faces", BM_FACE) {
		BM_flip_normal(bm, f);
	}
}

void bmesh_edgerotate_exec(BMesh *bm, BMOperator *op)
{
	BMOIter siter;
	BMEdge *e, *e2;
	int ccw = BMO_Get_Int(op, "ccw");

	BMO_ITER(e, &siter, bm, op, "edges", BM_EDGE) {
		if (!(e2 = BM_Rotate_Edge(bm, e, ccw))) {
			BMO_RaiseError(bm, op, BMERR_INVALID_SELECTION, "Could not rotate edge");
			return;
		}

		BMO_SetFlag(bm, e2, 1);
	}

	BMO_Flag_To_Slot(bm, op, "edgeout", 1, BM_EDGE);
}

#define SEL_FLAG	1
#define SEL_ORIG	2

static void bmesh_regionextend_extend(BMesh *bm, BMOperator *op, int usefaces)
{
	BMVert *v;
	BMEdge *e;
	BMIter eiter;
	BMOIter siter;

	if (!usefaces) {
		BMO_ITER(v, &siter, bm, op, "geom", BM_VERT) {
			BM_ITER(e, &eiter, bm, BM_EDGES_OF_VERT, v) {
				if (!BMO_TestFlag(bm, e, SEL_ORIG))
					break;
			}

			if (e) {
				BM_ITER(e, &eiter, bm, BM_EDGES_OF_VERT, v) {
					BMO_SetFlag(bm, e, SEL_FLAG);
					BMO_SetFlag(bm, BM_OtherEdgeVert(e, v), SEL_FLAG);
				}
			}
		}
	} else {
		BMIter liter, fiter;
		BMFace *f, *f2;
		BMLoop *l;

		BMO_ITER(f, &siter, bm, op, "geom", BM_FACE) {
			BM_ITER(l, &liter, bm, BM_LOOPS_OF_FACE, f) {
				BM_ITER(f2, &fiter, bm, BM_FACES_OF_EDGE, l->e) {
					if (!BMO_TestFlag(bm, f2, SEL_ORIG))
						BMO_SetFlag(bm, f2, SEL_FLAG);
				}
			}
		}
	}
}

static void bmesh_regionextend_constrict(BMesh *bm, BMOperator *op, int usefaces)
{
	BMVert *v;
	BMEdge *e;
	BMIter eiter;
	BMOIter siter;

	if (!usefaces) {
		BMO_ITER(v, &siter, bm, op, "geom", BM_VERT) {
			BM_ITER(e, &eiter, bm, BM_EDGES_OF_VERT, v) {
				if (!BMO_TestFlag(bm, e, SEL_ORIG))
					break;
			}

			if (e) {
				BMO_SetFlag(bm, v, SEL_FLAG);

				BM_ITER(e, &eiter, bm, BM_EDGES_OF_VERT, v) {
					BMO_SetFlag(bm, e, SEL_FLAG);
				}
			}
		}
	} else {
		BMIter liter, fiter;
		BMFace *f, *f2;
		BMLoop *l;

		BMO_ITER(f, &siter, bm, op, "geom", BM_FACE) {
			BM_ITER(l, &liter, bm, BM_LOOPS_OF_FACE, f) {
				BM_ITER(f2, &fiter, bm, BM_FACES_OF_EDGE, l->e) {
					if (!BMO_TestFlag(bm, f2, SEL_ORIG)) {
						BMO_SetFlag(bm, f, SEL_FLAG);
						break;
					}
				}
			}
		}
	}
}

void bmesh_regionextend_exec(BMesh *bm, BMOperator *op)
{
	int usefaces = BMO_Get_Int(op, "usefaces");
	int constrict = BMO_Get_Int(op, "constrict");

	BMO_Flag_Buffer(bm, op, "geom", SEL_ORIG);

	if (constrict)
		bmesh_regionextend_constrict(bm, op, usefaces);
	else
		bmesh_regionextend_extend(bm, op, usefaces);

	BMO_Flag_To_Slot(bm, op, "geomout", SEL_FLAG, BM_ALL);
}