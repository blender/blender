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

	BMO_Flag_Buffer(bm, op, "geom", SEL_ORIG, BM_ALL);

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
	
	BMO_Flag_Buffer(bm, op, "faces", FACE_FLAG, BM_FACE);

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

	/*make sure the starting face has the correct winding*/
	if (cent[0]*startf->no[0] + cent[1]*startf->no[1] + cent[2]*startf->no[2] < 0.0)
		BM_flip_normal(bm, startf);
	
	/*now that we've found our starting face, make all connected faces
	  have the same winding.  this is done recursively, using a manual
	  stack (if we use simple function recursion, we'd end up overloading
	  the stack on large meshes).*/

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

void bmesh_vertexsmooth_exec(BMesh *bm, BMOperator *op)
{
	BMOIter siter;
	BMIter iter;
	BMVert *v;
	BMEdge *e;
	V_DECLARE(cos);
	float (*cos)[3] = NULL;
	float *co, *co2, clipdist = BMO_Get_Float(op, "clipdist");
	int i, j, clipx, clipy, clipz;
	
	clipx = BMO_Get_Int(op, "mirror_clip_x");
	clipy = BMO_Get_Int(op, "mirror_clip_y");
	clipz = BMO_Get_Int(op, "mirror_clip_z");

	i = 0;
	BMO_ITER(v, &siter, bm, op, "verts", BM_VERT) {
		V_GROW(cos);
		co = cos[i];
		
		j  = 0;
		BM_ITER(e, &iter, bm, BM_EDGES_OF_VERT, v) {
			co2 = BM_OtherEdgeVert(e, v)->co;
			VECADD(co, co, co2);
			j += 1;
		}
		
		if (!j) {
			VECCOPY(co, v->co);
			i++;
			continue;
		}

		co[0] /= (float)j;
		co[1] /= (float)j;
		co[2] /= (float)j;

		co[0] = v->co[0]*0.5 + co[0]*0.5;
		co[1] = v->co[1]*0.5 + co[1]*0.5;
		co[2] = v->co[2]*0.5 + co[2]*0.5;
		
		if (clipx && fabs(v->co[0]) < clipdist)
			co[0] = 0.0f;
		if (clipy && fabs(v->co[1]) < clipdist)
			co[1] = 0.0f;
		if (clipz && fabs(v->co[2]) < clipdist)
			co[2] = 0.0f;

		i++;
	}

	i = 0;
	BMO_ITER(v, &siter, bm, op, "verts", BM_VERT) {
		VECCOPY(v->co, cos[i]);
		i++;
	}

	V_FREE(cos);
}

/*
** compute the centroid of an ngon
**
** NOTE: This should probably go to bmesh_polygon.c and replace the function that compute its center
** basing on bounding box
*/
static void ngon_center(float *v, BMesh *bm, BMFace *f)
{
	BMIter	liter;
	BMLoop	*l;
	v[0] = v[1] = v[2] = 0;

	BM_ITER(l, &liter, bm, BM_LOOPS_OF_FACE, f) {
		VecAddf(v, v, l->v->co);
	}

	if( f->len )
	{
		v[0] /= f->len;
		v[1] /= f->len;
		v[2] /= f->len;
	}
}

/*
** compute the perimeter of an ngon
**
** NOTE: This should probably go to bmesh_polygon.c
*/
static float ngon_perimeter(BMesh *bm, BMFace *f)
{
	BMIter	liter;
	BMLoop	*l;
	int		num_verts = 0;
	float	v[3], sv[3];
	float	perimeter = 0.0f;

	BM_ITER(l, &liter, bm, BM_LOOPS_OF_FACE, f) {
		if( num_verts == 0 ) {
			sv[0] = v[0] = l->v->co[0];
			sv[1] = v[1] = l->v->co[1];
			sv[2] = v[2] = l->v->co[2];
			num_verts++;
		} else {
			perimeter += VecLenf(v, l->v->co);
			v[0] = l->v->co[0];
			v[1] = l->v->co[1];
			v[2] = l->v->co[2];
			num_verts++;
		}
	}

	perimeter += VecLenf(v, sv);

	return perimeter;
}

/*
** compute the fake surface of an ngon
** This is done by decomposing the ngon into triangles who share the centroid of the ngon
** while this method is far from being exact, it should garantee an invariance.
**
** NOTE: This should probably go to bmesh_polygon.c
*/
static float ngon_fake_area(BMesh *bm, BMFace *f)
{
	BMIter	liter;
	BMLoop	*l;
	int		num_verts = 0;
	float	v[3], sv[3], c[3];
	float	area = 0.0f;

	ngon_center(c, bm, f);

	BM_ITER(l, &liter, bm, BM_LOOPS_OF_FACE, f) {
		if( num_verts == 0 ) {
			sv[0] = v[0] = l->v->co[0];
			sv[1] = v[1] = l->v->co[1];
			sv[2] = v[2] = l->v->co[2];
			num_verts++;
		} else {
			area += AreaT3Dfl(v, c, l->v->co);
			v[0] = l->v->co[0];
			v[1] = l->v->co[1];
			v[2] = l->v->co[2];
			num_verts++;
		}
	}

	area += AreaT3Dfl(v, c, sv);

	return area;
}

/*
** extra face data (computed data)
*/
typedef struct tmp_face_ext {
	BMFace		*f;			/* the face */
	float	c[3];			/* center */
	union {
		float	area;		/* area */
		float	perim;		/* perimeter */
		float	d;			/* 4th component of plane (the first three being the normal) */
		struct Image	*t;	/* image pointer */
	};
} tmp_face_ext;

/*
** Select similar faces, the choices are in the enum in source/blender/bmesh/bmesh_operators.h
** We select either similar faces based on material, image, area, perimeter, normal, or the coplanar faces
*/
void bmesh_similarfaces_exec(BMesh *bm, BMOperator *op)
{
	BMIter fm_iter;
	BMFace *fs, *fm;
	BMOIter fs_iter;
	int num_tex, num_sels = 0, num_total = 0, i = 0, idx = 0;
	float angle = 0.0f;
	tmp_face_ext *f_ext = NULL;
	int *indices = NULL;
	float t_no[3];	/* temporary normal */
	int type = BMO_Get_Int(op, "type");
	float thresh = BMO_Get_Float(op, "thresh");

	num_total = BM_Count_Element(bm, BM_FACE);

	/*
	** The first thing to do is to iterate through all the the selected items and mark them since
	** they will be in the selection anyway.
	** This will increase performance, (especially when the number of originaly selected faces is high)
	** so the overall complexity will be less than $O(mn)$ where is the total number of selected faces,
	** and n is the total number of faces
	*/
	BMO_ITER(fs, &fs_iter, bm, op, "faces", BM_FACE) {
		if (!BMO_TestFlag(bm, fs, FACE_MARK)) {	/* is this really needed ? */
			BMO_SetFlag(bm, fs, FACE_MARK);
			num_sels++;
		}
	}

	/* allocate memory for the selected faces indices and for all temporary faces */
	indices	= (int*)malloc(sizeof(int) * num_sels);
	f_ext = (tmp_face_ext*)malloc(sizeof(tmp_face_ext) * num_total);

	/* loop through all the faces and fill the faces/indices structure */
	BM_ITER(fm, &fm_iter, bm, BM_FACES_OF_MESH, NULL) {
		f_ext[i].f = fm;
		if (BMO_TestFlag(bm, fm, FACE_MARK)) {
			indices[idx] = i;
			idx++;
		}
		i++;
	}

	/*
	** Save us some computation burden: In case of perimeter/area/coplanar selection we compute
	** only once.
	*/
	if( type == SIMFACE_PERIMETER || type == SIMFACE_AREA || type == SIMFACE_COPLANAR || type == SIMFACE_IMAGE )	{
		for( i = 0; i < num_total; i++ ) {
			switch( type ) {
			case SIMFACE_PERIMETER:
				/* set the perimeter */
				f_ext[i].perim = ngon_perimeter(bm, f_ext[i].f);
				break;

			case SIMFACE_COPLANAR:
				/* compute the center of the polygon */
				ngon_center(f_ext[i].c, bm, f_ext[i].f);

				/* normalize the polygon normal */
				VecCopyf(t_no, f_ext[i].f->no);
				Normalize(t_no);

				/* compute the plane distance */
				f_ext[i].d = Inpf(t_no, f_ext[i].c);
				break;

			case SIMFACE_AREA:
				f_ext[i].area = ngon_fake_area(bm, f_ext[i].f);
				break;

			case SIMFACE_IMAGE:
				f_ext[i].t = NULL;
				if( CustomData_has_layer(&(bm->pdata), CD_MTEXPOLY) ) {
					MTexPoly *mtpoly = CustomData_bmesh_get(&bm->pdata, f_ext[i].f->head.data, CD_MTEXPOLY);
					f_ext[i].t = mtpoly->tpage;
				}
				break;
			}
		}
	}

	/* now select the rest (if any) */
	//BM_ITER(fm, &fm_iter, bm, BM_FACES_OF_MESH, NULL) {
	for( i = 0; i < num_total; i++ ) {
		fm = f_ext[i].f;
		if (!BMO_TestFlag(bm, fm, FACE_MARK)) {
			//BMO_ITER(fs, &fs_iter, bm, op, "faces", BM_FACE) {
			int cont = 1;
			for( idx = 0; idx < num_sels && cont == 1; idx++ ) {
				fs = f_ext[indices[idx]].f;
				switch( type ) {
				case SIMFACE_MATERIAL:
					if( fm->mat_nr == fs->mat_nr ) {
						BMO_SetFlag(bm, fm, FACE_MARK);
						cont = 0;
					}
					break;

				case SIMFACE_IMAGE:
					if( f_ext[i].t == f_ext[indices[idx]].t ) {
						BMO_SetFlag(bm, fm, FACE_MARK);
						cont = 0;
					}
					break;

				case SIMFACE_NORMAL:
					angle = VecAngle2(fs->no, fm->no);	/* if the angle between the normals -> 0 */
					if( angle / 180.0 <= thresh ) {
						BMO_SetFlag(bm, fm, FACE_MARK);
						cont = 0;
					}
					break;

				case SIMFACE_COPLANAR:
					angle = VecAngle2(fs->no, fm->no); /* angle -> 0 */
					if( angle / 180.0 <= thresh ) { /* and dot product difference -> 0 */
						if( fabs(f_ext[i].d - f_ext[indices[idx]].d) <= thresh ) {
							BMO_SetFlag(bm, fm, FACE_MARK);
							cont = 0;
						}
					}
					break;

				case SIMFACE_AREA:
					if( fabs(f_ext[i].area - f_ext[indices[idx]].area) <= thresh ) {
						BMO_SetFlag(bm, fm, FACE_MARK);
						cont = 0;
					}
					break;

				case SIMFACE_PERIMETER:
					if( fabs(f_ext[i].perim - f_ext[indices[idx]].perim) <= thresh ) {
						BMO_SetFlag(bm, fm, FACE_MARK);
						cont = 0;
					}
					break;
				}
			}
		}
	}

	free(f_ext);
	free(indices);

	/* transfer all marked faces to the output slot */
	BMO_Flag_To_Slot(bm, op, "faceout", FACE_MARK, BM_FACE);
}
