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

/********* righthand faces implementation ********/

#define FACE_VIS	1
#define FACE_FLAG	2
#define FACE_MARK	4

/* NOTE: these are the original righthandfaces comment in editmesh_mods.c,
         copied here for reference.
*/
       /* based at a select-connected to witness loose objects */

	/* count per edge the amount of faces */

	/* find the ultimate left, front, upper face (not manhattan dist!!) */
	/* also evaluate both triangle cases in quad, since these can be non-flat */

	/* put normal to the outside, and set the first direction flags in edges */

	/* then check the object, and set directions / direction-flags: but only for edges with 1 or 2 faces */
	/* this is in fact the 'select connected' */
	
	/* in case (selected) faces were not done: start over with 'find the ultimate ...' */

/*note: this function uses recursion, which is a little unusual for a bmop
        function, but acceptable I think.*/
void bmesh_righthandfaces_exec(BMesh *bm, BMOperator *op)
{
	BMIter liter, liter2;
	BMOIter siter;
	BMFace *f, *startf, **fstack = NULL;
	V_DECLARE(fstack);
	BMLoop *l, *l2;
	float maxx, cent[3];
	int i, maxi;

	startf= NULL;
	maxx= -1.0e10;
	
	BMO_Flag_Buffer(bm, op, "faces", FACE_FLAG);

	/*find a starting face*/
	BMO_ITER(f, &siter, bm, op, "faces", BM_FACE) {
		if (BMO_TestFlag(bm, f, FACE_VIS))
			continue;

		if (!startf) startf = f;

		BM_Compute_Face_Center(bm, f, cent);

		cent[0] = cent[0]*cent[0] + cent[1]*cent[1] + cent[2]*cent[2];
		if (cent[0] > maxx) {
			maxx = cent[0];
			startf = f;
		}
	}

	if (!startf) return;

	BM_Compute_Face_Center(bm, startf, cent);

	if (cent[0]*startf->no[0] + cent[1]*startf->no[1] + cent[2]*startf->no[2] < 0.0)
		BM_flip_normal(bm, startf);
	
	V_GROW(fstack);
	fstack[0] = startf;
	BMO_SetFlag(bm, startf, FACE_VIS);

	i = 0;
	maxi = 1;
	while (i >= 0) {
		f = fstack[i];
		i--;

		BM_ITER(l, &liter, bm, BM_LOOPS_OF_FACE, f) {
			BM_ITER(l2, &liter2, bm, BM_LOOPS_OF_LOOP, l) {
				if (!BMO_TestFlag(bm, l2->f, FACE_FLAG) || l2 == l)
					continue;

				if (!BMO_TestFlag(bm, l2->f, FACE_VIS)) {
					BMO_SetFlag(bm, l2->f, FACE_VIS);
					i++;
					
					if (l2->v == l->v)
						BM_flip_normal(bm, l2->f);

					if (i == maxi) {
						V_GROW(fstack);
						maxi++;
					}

					fstack[i] = l2->f;
				}
			}
		}
	}

	V_FREE(fstack);

	/*check if we have faces yet to do.  if so, recurse.*/
	BMO_ITER(f, &siter, bm, op, "faces", BM_FACE) {
		if (!BMO_TestFlag(bm, f, FACE_VIS)) {
			bmesh_righthandfaces_exec(bm, op);
			break;
		}
	}
}
