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
#include "BLI_array.h"
#include "BLI_blenlib.h"
#include "BLI_edgehash.h"

#include "BLI_heap.h"

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

void bmesh_translate_exec(BMesh *bm, BMOperator *op)
{
	float mat[4][4], vec[3];
	
	BMO_Get_Vec(op, "vec", vec);

	Mat4One(mat);
	VECCOPY(mat[3], vec);

	BMO_CallOpf(bm, "transform mat=%m4 verts=%s", mat, op, "verts");
}

void bmesh_scale_exec(BMesh *bm, BMOperator *op)
{
	float mat[3][3], vec[3];
	
	BMO_Get_Vec(op, "vec", vec);

	Mat3One(mat);
	mat[0][0] = vec[0];
	mat[1][1] = vec[1];
	mat[2][2] = vec[2];

	BMO_CallOpf(bm, "transform mat=%m3 verts=%s", mat, op, "verts");
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
	BLI_array_declare(fstack);
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

	BLI_array_growone(fstack);
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
						BLI_array_growone(fstack);
						maxi++;
					}

					fstack[i] = l2->f;
				}
			}
		}
	}

	BLI_array_free(fstack);

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
	BLI_array_declare(cos);
	float (*cos)[3] = NULL;
	float *co, *co2, clipdist = BMO_Get_Float(op, "clipdist");
	int i, j, clipx, clipy, clipz;
	
	clipx = BMO_Get_Int(op, "mirror_clip_x");
	clipy = BMO_Get_Int(op, "mirror_clip_y");
	clipz = BMO_Get_Int(op, "mirror_clip_z");

	i = 0;
	BMO_ITER(v, &siter, bm, op, "verts", BM_VERT) {
		BLI_array_growone(cos);
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

	BLI_array_free(cos);
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
	int num_sels = 0, num_total = 0, i = 0, idx = 0;
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
	indices	= (int*)MEM_callocN(sizeof(int) * num_sels, "face indices util.c");
	f_ext = (tmp_face_ext*)MEM_callocN(sizeof(tmp_face_ext) * num_total, "f_ext util.c");

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
	if( type == SIMFACE_PERIMETER || type == SIMFACE_AREA || type == SIMFACE_COPLANAR || type == SIMFACE_IMAGE ) {
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
	for( i = 0; i < num_total; i++ ) {
		fm = f_ext[i].f;
		if( !BMO_TestFlag(bm, fm, FACE_MARK)  && !BM_TestHFlag(fm, BM_HIDDEN) ) {
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

	MEM_freeN(f_ext);
	MEM_freeN(indices);

	/* transfer all marked faces to the output slot */
	BMO_Flag_To_Slot(bm, op, "faceout", FACE_MARK, BM_FACE);
}

/******************************************************************************
** Similar Edges
******************************************************************************/
#define EDGE_MARK	1

/*
** compute the angle of an edge (i.e. the angle between two faces)
*/
static float edge_angle(BMesh *bm, BMEdge *e)
{
	BMIter	fiter;
	BMFace	*f;
	int		num_faces = 0;
	float	n1[3], n2[3];
	float	angle = 0.0f;

	BM_ITER(f, &fiter, bm, BM_FACES_OF_EDGE, e) {
		if( num_faces == 0 ) {
			n1[0] = f->no[0];
			n1[1] = f->no[1];
			n1[2] = f->no[2];
			num_faces++;
		} else {
			n2[0] = f->no[0];
			n2[1] = f->no[1];
			n2[2] = f->no[2];
			num_faces++;
		}
	}

	angle = VecAngle2(n1, n2) / 180.0;

	return angle;
}
/*
** extra edge information
*/
typedef struct tmp_edge_ext {
	BMEdge		*e;
	union {
		float		dir[3];
		float		angle;			/* angle between the faces*/
	};

	union {
		float		length;			/* edge length */
		int			faces;			/* faces count */
	};
} tmp_edge_ext;

/*
** select similar edges: the choices are in the enum in source/blender/bmesh/bmesh_operators.h
** choices are length, direction, face, ...
*/
void bmesh_similaredges_exec(BMesh *bm, BMOperator *op)
{
	BMOIter es_iter;	/* selected edges iterator */
	BMIter	e_iter;		/* mesh edges iterator */
	BMEdge	*es;		/* selected edge */
	BMEdge	*e;		/* mesh edge */
	int idx = 0, i = 0, f = 0;
	int *indices = NULL;
	tmp_edge_ext *e_ext = NULL;
	float *angles = NULL;
	float angle;

	int num_sels = 0, num_total = 0;
	int type = BMO_Get_Int(op, "type");
	float thresh = BMO_Get_Float(op, "thresh");

	num_total = BM_Count_Element(bm, BM_EDGE);

	/* iterate through all selected edges and mark them */
	BMO_ITER(es, &es_iter, bm, op, "edges", BM_EDGE) {
			BMO_SetFlag(bm, es, EDGE_MARK);
			num_sels++;
	}

	/* allocate memory for the selected edges indices and for all temporary edges */
	indices	= (int*)MEM_callocN(sizeof(int) * num_sels, "indices util.c");
	e_ext = (tmp_edge_ext*)MEM_callocN(sizeof(tmp_edge_ext) * num_total, "e_ext util.c");

	/* loop through all the edges and fill the edges/indices structure */
	BM_ITER(e, &e_iter, bm, BM_EDGES_OF_MESH, NULL) {
		e_ext[i].e = e;
		if (BMO_TestFlag(bm, e, EDGE_MARK)) {
			indices[idx] = i;
			idx++;
		}
		i++;
	}

	/* save us some computation time by doing heavy computation once */
	if( type == SIMEDGE_LENGTH || type == SIMEDGE_FACE || type == SIMEDGE_DIR ||
		type == SIMEDGE_FACE_ANGLE ) {
		for( i = 0; i < num_total; i++ ) {
			switch( type ) {
			case SIMEDGE_LENGTH:	/* compute the length of the edge */
				e_ext[i].length	= VecLenf(e_ext[i].e->v1->co, e_ext[i].e->v2->co);
				break;

			case SIMEDGE_DIR:		/* compute the direction */
				VecSubf(e_ext[i].dir, e_ext[i].e->v1->co, e_ext[i].e->v2->co);
				break;

			case SIMEDGE_FACE:		/* count the faces around the edge */
				e_ext[i].faces	= BM_Edge_FaceCount(e_ext[i].e);
				break;

			case SIMEDGE_FACE_ANGLE:
				e_ext[i].faces	= BM_Edge_FaceCount(e_ext[i].e);
				if( e_ext[i].faces == 2 )
					e_ext[i].angle = edge_angle(bm, e_ext[i].e);
				break;
			}
		}
	}

	/* select the edges if any */
	for( i = 0; i < num_total; i++ ) {
		e = e_ext[i].e;
		if( !BMO_TestFlag(bm, e, EDGE_MARK) && !BM_TestHFlag(e, BM_HIDDEN) ) {
			int cont = 1;
			for( idx = 0; idx < num_sels && cont == 1; idx++ ) {
				es = e_ext[indices[idx]].e;
				switch( type ) {
				case SIMEDGE_LENGTH:
					if( fabs(e_ext[i].length - e_ext[indices[idx]].length) <= thresh ) {
						BMO_SetFlag(bm, e, EDGE_MARK);
						cont = 0;
					}
					break;

				case SIMEDGE_DIR:
					/* compute the angle between the two edges */
					angle = VecAngle2(e_ext[i].dir, e_ext[indices[idx]].dir);

					if( angle > 90.0 ) /* use the smallest angle between the edges */
						angle = fabs(angle - 180.0f);

					if( angle / 90.0 <= thresh ) {
						BMO_SetFlag(bm, e, EDGE_MARK);
						cont = 0;
					}
					break;

				case SIMEDGE_FACE:
					if( e_ext[i].faces == e_ext[indices[idx]].faces ) {
						BMO_SetFlag(bm, e, EDGE_MARK);
						cont = 0;
					}
					break;

				case SIMEDGE_FACE_ANGLE:
					if( e_ext[i].faces == 2 ) {
						if( e_ext[indices[idx]].faces == 2 ) {
							if( fabs(e_ext[i].angle - e_ext[indices[idx]].angle) <= thresh ) {
								BMO_SetFlag(bm, e, EDGE_MARK);
								cont = 0;
							}
						}
					} else cont = 0;
					break;

				case SIMEDGE_CREASE:
					if( fabs(e->crease - es->crease) <= thresh ) {
						BMO_SetFlag(bm, e, EDGE_MARK);
						cont = 0;
					}
					break;

				case SIMEDGE_SEAM:
					if( BM_TestHFlag(e, BM_SEAM) == BM_TestHFlag(es, BM_SEAM) ) {
						BMO_SetFlag(bm, e, EDGE_MARK);
						cont = 0;
					}
					break;

				case SIMEDGE_SHARP:
					if( BM_TestHFlag(e, BM_SHARP) == BM_TestHFlag(es, BM_SHARP) ) {
						BMO_SetFlag(bm, e, EDGE_MARK);
						cont = 0;
					}
					break;
				}
			}
		}
	}

	MEM_freeN(e_ext);
	MEM_freeN(indices);

	/* transfer all marked edges to the output slot */
	BMO_Flag_To_Slot(bm, op, "edgeout", EDGE_MARK, BM_EDGE);
}

/******************************************************************************
** Similar Vertices
******************************************************************************/
#define VERT_MARK	1

typedef struct tmp_vert_ext {
	BMVert *v;
	union {
		int num_faces; /* adjacent faces */
		MDeformVert *dvert; /* deform vertex */
	};
} tmp_vert_ext;

/*
** select similar vertices: the choices are in the enum in source/blender/bmesh/bmesh_operators.h
** choices are normal, face, vertex group...
*/
void bmesh_similarverts_exec(BMesh *bm, BMOperator *op)
{
	BMOIter vs_iter;	/* selected verts iterator */
	BMIter v_iter;		/* mesh verts iterator */
	BMVert *vs;		/* selected vertex */
	BMVert *v;			/* mesh vertex */
	tmp_vert_ext *v_ext = NULL;
	int *indices = NULL;
	int num_total = 0, num_sels = 0, i = 0, idx = 0;
	int type = BMO_Get_Int(op, "type");
	float thresh = BMO_Get_Float(op, "thresh");

	num_total = BM_Count_Element(bm, BM_VERT);

	/* iterate through all selected edges and mark them */
	BMO_ITER(vs, &vs_iter, bm, op, "verts", BM_VERT) {
		BMO_SetFlag(bm, vs, VERT_MARK);
		num_sels++;
	}

	/* allocate memory for the selected vertices indices and for all temporary vertices */
	indices	= (int*)MEM_mallocN(sizeof(int) * num_sels, "vertex indices");
	v_ext = (tmp_vert_ext*)MEM_mallocN(sizeof(tmp_vert_ext) * num_total, "vertex extra");

	/* loop through all the vertices and fill the vertices/indices structure */
	BM_ITER(v, &v_iter, bm, BM_VERTS_OF_MESH, NULL) {
		v_ext[i].v = v;
		if (BMO_TestFlag(bm, v, VERT_MARK)) {
			indices[idx] = i;
			idx++;
		}

		switch( type ) {
		case SIMVERT_FACE:
			/* calling BM_Vert_FaceCount every time is time consumming, so call it only once per vertex */
			v_ext[i].num_faces	= BM_Vert_FaceCount(v);
			break;

		case SIMVERT_VGROUP:
			if( CustomData_has_layer(&(bm->vdata),CD_MDEFORMVERT) ) {
				v_ext[i].dvert = CustomData_bmesh_get(&bm->vdata, v_ext[i].v->head.data, CD_MDEFORMVERT);
			} else v_ext[i].dvert = NULL;
			break;
		}

		i++;
	}

	/* select the vertices if any */
	for( i = 0; i < num_total; i++ ) {
		v = v_ext[i].v;
		if( !BMO_TestFlag(bm, v, VERT_MARK) && !BM_TestHFlag(v, BM_HIDDEN) ) {
			int cont = 1;
			for( idx = 0; idx < num_sels && cont == 1; idx++ ) {
				vs = v_ext[indices[idx]].v;
				switch( type ) {
				case SIMVERT_NORMAL:
					/* compare the angle between the normals */
					if( VecAngle2(v->no, vs->no) / 180.0 <= thresh ) {
						BMO_SetFlag(bm, v, VERT_MARK);
						cont = 0;

					}
					break;
				case SIMVERT_FACE:
					/* number of adjacent faces */
					if( v_ext[i].num_faces == v_ext[indices[idx]].num_faces ) {
						BMO_SetFlag(bm, v, VERT_MARK);
						cont = 0;
					}
					break;

				case SIMVERT_VGROUP:
					if( v_ext[i].dvert != NULL && v_ext[indices[idx]].dvert != NULL ) {
						int v1, v2;
						for( v1 = 0; v1 < v_ext[i].dvert->totweight && cont == 1; v1++ ) {
							for( v2 = 0; v2 < v_ext[indices[idx]].dvert->totweight; v2++ ) {
								if( v_ext[i].dvert->dw[v1].def_nr == v_ext[indices[idx]].dvert->dw[v2].def_nr ) {
									BMO_SetFlag(bm, v, VERT_MARK);
									cont = 0;
									break;
								}
							}
						}
					}
					break;
				}
			}
		}
	}

	MEM_freeN(indices);
	MEM_freeN(v_ext);

	BMO_Flag_To_Slot(bm, op, "vertout", VERT_MARK, BM_VERT);
}

/******************************************************************************
** Cycle UVs for a face
******************************************************************************/

void bmesh_rotateuvs_exec(BMesh *bm, BMOperator *op)
{
	BMOIter fs_iter;	/* selected faces iterator */
	BMFace *fs;	/* current face */
	BMIter l_iter;	/* iteration loop */
	int n;

	int dir = BMO_Get_Int(op, "dir");

	BMO_ITER(fs, &fs_iter, bm, op, "faces", BM_FACE) {
		if( CustomData_has_layer(&(bm->ldata), CD_MLOOPUV) ) {
			if( dir == DIRECTION_CW ) { /* same loops direction */
				BMLoop *lf;	/* current face loops */
				MLoopUV *f_luv; /* first face loop uv */
				float p_uv[2];	/* previous uvs */
				float t_uv[2];	/* tmp uvs */

				int n = 0;
				BM_ITER(lf, &l_iter, bm, BM_LOOPS_OF_FACE, fs) {
					/* current loop uv is the previous loop uv */
					MLoopUV *luv = CustomData_bmesh_get(&bm->ldata, lf->head.data, CD_MLOOPUV);
					if( n == 0 ) {
						f_luv = luv;
						p_uv[0] = luv->uv[0];
						p_uv[1] = luv->uv[1];
					} else {
						t_uv[0] = luv->uv[0];
						t_uv[1] = luv->uv[1];
						luv->uv[0] = p_uv[0];
						luv->uv[1] = p_uv[1];
						p_uv[0] = t_uv[0];
						p_uv[1] = t_uv[1];
					}
					n++;
				}

				f_luv->uv[0] = p_uv[0];
				f_luv->uv[1] = p_uv[1];
			} else if( dir == DIRECTION_CCW ) { /* counter loop direction */
				BMLoop *lf;	/* current face loops */
				MLoopUV *p_luv; /*previous loop uv */
				MLoopUV *luv;
				float t_uv[2];	/* current uvs */

				int n = 0;
				BM_ITER(lf, &l_iter, bm, BM_LOOPS_OF_FACE, fs) {
					/* previous loop uv is the current loop uv */
					luv = CustomData_bmesh_get(&bm->ldata, lf->head.data, CD_MLOOPUV);
					if( n == 0 ) {
						p_luv = luv;
						t_uv[0] = luv->uv[0];
						t_uv[1] = luv->uv[1];
					} else {
						p_luv->uv[0] = luv->uv[0];
						p_luv->uv[1] = luv->uv[1];
						p_luv = luv;
					}
					n++;
				}

				luv->uv[0] = t_uv[0];
				luv->uv[1] = t_uv[1];
			}
		}
	}

}

/******************************************************************************
** Reverse UVs for a face
******************************************************************************/

void bmesh_reverseuvs_exec(BMesh *bm, BMOperator *op)
{
	BMOIter fs_iter;	/* selected faces iterator */
	BMFace *fs;		/* current face */
	BMIter l_iter;		/* iteration loop */
	BLI_array_declare(uvs);
	float (*uvs)[2] = NULL;
	int max_vert_count = 0;

	BMO_ITER(fs, &fs_iter, bm, op, "faces", BM_FACE) {
		if( CustomData_has_layer(&(bm->ldata), CD_MLOOPUV) ) {
			BMLoop *lf;	/* current face loops */
			MLoopUV *f_luv; /* first face loop uv */
			int num_verts = fs->len;
			int i = 0;

			BLI_array_empty(uvs);
			BM_ITER(lf, &l_iter, bm, BM_LOOPS_OF_FACE, fs) {
				MLoopUV *luv = CustomData_bmesh_get(&bm->ldata, lf->head.data, CD_MLOOPUV);

				/* current loop uv is the previous loop uv */
				BLI_array_growone(uvs);
				uvs[i][0] = luv->uv[0];
				uvs[i][1] = luv->uv[1];
				i++;
			}

			/* now that we have the uvs in the array, reverse! */
			i = 0;
			BM_ITER(lf, &l_iter, bm, BM_LOOPS_OF_FACE, fs) {
				/* current loop uv is the previous loop uv */
				MLoopUV *luv = CustomData_bmesh_get(&bm->ldata, lf->head.data, CD_MLOOPUV);
				luv->uv[0] = uvs[(fs->len - i - 1)][0];
				luv->uv[1] = uvs[(fs->len - i - 1)][1];
				i++;
			}
		}
	}

	BLI_array_free(uvs);
}

/******************************************************************************
** Cycle colors for a face
******************************************************************************/

void bmesh_rotatecolors_exec(BMesh *bm, BMOperator *op)
{
	BMOIter fs_iter;	/* selected faces iterator */
	BMFace *fs;	/* current face */
	BMIter l_iter;	/* iteration loop */
	int n;

	int dir = BMO_Get_Int(op, "dir");

	BMO_ITER(fs, &fs_iter, bm, op, "faces", BM_FACE) {
		if( CustomData_has_layer(&(bm->ldata), CD_MLOOPCOL) ) {
			if( dir == DIRECTION_CW ) { /* same loops direction */
				BMLoop *lf;	/* current face loops */
				MLoopCol *f_lcol; /* first face loop color */
				MLoopCol p_col;	/* previous color */
				MLoopCol t_col;	/* tmp color */

				int n = 0;
				BM_ITER(lf, &l_iter, bm, BM_LOOPS_OF_FACE, fs) {
					/* current loop color is the previous loop color */
					MLoopCol *luv = CustomData_bmesh_get(&bm->ldata, lf->head.data, CD_MLOOPCOL);
					if( n == 0 ) {
						f_lcol = luv;
						p_col = *luv;
					} else {
						t_col = *luv;
						*luv = p_col;
						p_col = t_col;
					}
					n++;
				}

				*f_lcol = p_col;
			} else if( dir == DIRECTION_CCW ) { /* counter loop direction */
				BMLoop *lf;	/* current face loops */
				MLoopCol *p_lcol; /*previous loop color */
				MLoopCol *lcol;
				MLoopCol t_col;	/* current color */

				int n = 0;
				BM_ITER(lf, &l_iter, bm, BM_LOOPS_OF_FACE, fs) {
					/* previous loop color is the current loop color */
					lcol = CustomData_bmesh_get(&bm->ldata, lf->head.data, CD_MLOOPCOL);
					if( n == 0 ) {
						p_lcol = lcol;
						t_col = *lcol;
					} else {
						*p_lcol = *lcol;
						p_lcol = lcol;
					}
					n++;
				}

				*lcol = t_col;
			}
		}
	}
}

/******************************************************************************
** Reverse colors for a face
******************************************************************************/

void bmesh_reversecolors_exec(BMesh *bm, BMOperator *op)
{
	BMOIter fs_iter;	/* selected faces iterator */
	BMFace *fs;		/* current face */
	BMIter l_iter;		/* iteration loop */
	BLI_array_declare(cols);
	MLoopCol *cols = NULL;
	int max_vert_count = 0;


	BMO_ITER(fs, &fs_iter, bm, op, "faces", BM_FACE) {
		if( CustomData_has_layer(&(bm->ldata), CD_MLOOPCOL) ) {
			BMLoop *lf;	/* current face loops */
			MLoopCol *f_lcol; /* first face loop color */
			int num_verts = fs->len;
			int i = 0;

			BLI_array_empty(cols);
			BM_ITER(lf, &l_iter, bm, BM_LOOPS_OF_FACE, fs) {
				MLoopCol *lcol = CustomData_bmesh_get(&bm->ldata, lf->head.data, CD_MLOOPCOL);

				/* current loop uv is the previous loop color */
				BLI_array_growone(cols);
				cols[i] = *lcol;
				i++;
			}

			/* now that we have the uvs in the array, reverse! */
			i = 0;
			BM_ITER(lf, &l_iter, bm, BM_LOOPS_OF_FACE, fs) {
				/* current loop uv is the previous loop color */
				MLoopCol *lcol = CustomData_bmesh_get(&bm->ldata, lf->head.data, CD_MLOOPCOL);
				*lcol = cols[(fs->len - i - 1)];
				i++;
			}
		}
	}

	BLI_array_free(cols);
}


/******************************************************************************
** shortest vertex path select
******************************************************************************/

typedef struct element_node {
	BMVert *v;	/* vertex */
	BMVert *parent;	/* node parent id */
	float weight;	/* node weight */
	HeapNode *hn;	/* heap node */
} element_node;

void bmesh_vertexshortestpath_exec(BMesh *bm, BMOperator *op)
{
	BMOIter vs_iter, vs2_iter;	/* selected verts iterator */
	BMIter v_iter;		/* mesh verts iterator */
	BMVert *vs, *sv, *ev;	/* starting vertex, ending vertex */
	BMVert *v;		/* mesh vertex */
	Heap *h = NULL;

	element_node *vert_list = NULL;

	int num_total = 0, num_sels = 0, i = 0;
	int type = BMO_Get_Int(op, "type");

	BMO_ITER(vs, &vs_iter, bm, op, "startv", BM_VERT)
			sv = vs;
	BMO_ITER(vs, &vs_iter, bm, op, "endv", BM_VERT)
			ev = vs;

	num_total = BM_Count_Element(bm, BM_VERT);

	/* allocate memory for the nodes */
	vert_list = (element_node*)MEM_mallocN(sizeof(element_node) * num_total, "vertex nodes");

	/* iterate through all the mesh vertices */
	/* loop through all the vertices and fill the vertices/indices structure */
	i = 0;
	BM_ITER(v, &v_iter, bm, BM_VERTS_OF_MESH, NULL) {
		vert_list[i].v = v;
		vert_list[i].parent = NULL;
		vert_list[i].weight = FLT_MAX;
		BMINDEX_SET(v, i);
		i++;
	}

	/*
	** we now have everything we need, start Dijkstra path finding algorithm
	*/

	/* set the distance/weight of the start vertex to 0 */
	vert_list[BMINDEX_GET(sv)].weight = 0.0f;

	h = BLI_heap_new();

	for( i = 0; i < num_total; i++ )
		vert_list[i].hn = BLI_heap_insert(h, vert_list[i].weight, vert_list[i].v);

	while( !BLI_heap_empty(h) ) {
		BMEdge *e;
		BMIter e_i;
		float v_weight;

		/* take the vertex with the lowest weight out of the heap */
		BMVert *v = (BMVert*)BLI_heap_popmin(h);

		if( vert_list[BMINDEX_GET(v)].weight == FLT_MAX ) /* this means that there is no path */
			break;

		v_weight = vert_list[BMINDEX_GET(v)].weight;

		BM_ITER(e, &e_i, bm, BM_EDGES_OF_VERT, v) {
			BMVert *u;
			float e_weight = v_weight;

			if( type == VPATH_SELECT_EDGE_LENGTH )
				e_weight += VecLenf(e->v1->co, e->v2->co);
			else e_weight += 1.0f;

			u = ( e->v1 == v ) ? e->v2 : e->v1;

			if( e_weight < vert_list[BMINDEX_GET(u)].weight ) { /* is this path shorter ? */
				/* add it if so */
				vert_list[BMINDEX_GET(u)].parent = v;
				vert_list[BMINDEX_GET(u)].weight = e_weight;

				/* we should do a heap update node function!!! :-/ */
				BLI_heap_remove(h, vert_list[BMINDEX_GET(u)].hn);
				BLI_heap_insert(h, e_weight, u);
			}
		}
	}

	/* now we trace the path (if it exists) */
	v = ev;

	while( vert_list[BMINDEX_GET(v)].parent != NULL ) {
		BMO_SetFlag(bm, v, VERT_MARK);
		v = vert_list[BMINDEX_GET(v)].parent;
	}

	BLI_heap_free(h, NULL);
	MEM_freeN(vert_list);

	BMO_Flag_To_Slot(bm, op, "vertout", VERT_MARK, BM_VERT);
}
