/*
 * ***** BEGIN GPL LICENSE BLOCK *****
 *
 * This program is free software; you can redistribute it and/or
 * modify it under the terms of the GNU General Public License
 * as published by the Free Software Foundation; either version 2
 * of the License, or (at your option) any later version.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with this program; if not, write to the Free Software Foundation,
 * Inc., 51 Franklin Street, Fifth Floor, Boston, MA 02110-1301, USA.
 *
 * Contributor(s): Joseph Eagar.
 *
 * ***** END GPL LICENSE BLOCK *****
 */

#include "MEM_guardedalloc.h"

#include "DNA_meshdata_types.h"

#include "BLI_math.h"
#include "BLI_array.h"
#include "BLI_heap.h"

#include "BKE_customdata.h"

#include "bmesh.h"

#include "bmesh_operators_private.h" /* own include */

/*
 * UTILS.C
 *
 * utility bmesh operators, e.g. transform,
 * translate, rotate, scale, etc.
 *
 */

void bmo_makevert_exec(BMesh *bm, BMOperator *op)
{
	float vec[3];

	BMO_slot_vec_get(op, "co", vec);

	BMO_elem_flag_enable(bm, BM_vert_create(bm, vec, NULL), 1);
	BMO_slot_buffer_from_flag(bm, op, "newvertout", 1, BM_VERT);
}

void bmo_transform_exec(BMesh *bm, BMOperator *op)
{
	BMOIter iter;
	BMVert *v;
	float mat[4][4];

	BMO_slot_mat4_get(op, "mat", mat);

	BMO_ITER(v, &iter, bm, op, "verts", BM_VERT) {
		mul_m4_v3(mat, v->co);
	}
}

void bmo_translate_exec(BMesh *bm, BMOperator *op)
{
	float mat[4][4], vec[3];
	
	BMO_slot_vec_get(op, "vec", vec);

	unit_m4(mat);
	copy_v3_v3(mat[3], vec);

	BMO_op_callf(bm, "transform mat=%m4 verts=%s", mat, op, "verts");
}

void bmo_scale_exec(BMesh *bm, BMOperator *op)
{
	float mat[3][3], vec[3];
	
	BMO_slot_vec_get(op, "vec", vec);

	unit_m3(mat);
	mat[0][0] = vec[0];
	mat[1][1] = vec[1];
	mat[2][2] = vec[2];

	BMO_op_callf(bm, "transform mat=%m3 verts=%s", mat, op, "verts");
}

void bmo_rotate_exec(BMesh *bm, BMOperator *op)
{
	float vec[3];
	
	BMO_slot_vec_get(op, "cent", vec);
	
	/* there has to be a proper matrix way to do this, but
	 * this is how editmesh did it and I'm too tired to think
	 * through the math right now. */
	mul_v3_fl(vec, -1.0f);
	BMO_op_callf(bm, "translate verts=%s vec=%v", op, "verts", vec);

	BMO_op_callf(bm, "transform mat=%s verts=%s", op, "mat", op, "verts");

	mul_v3_fl(vec, -1.0f);
	BMO_op_callf(bm, "translate verts=%s vec=%v", op, "verts", vec);
}

void bmo_reversefaces_exec(BMesh *bm, BMOperator *op)
{
	BMOIter siter;
	BMFace *f;

	BMO_ITER(f, &siter, bm, op, "faces", BM_FACE) {
		BM_face_normal_flip(bm, f);
	}
}

void bmo_edgerotate_exec(BMesh *bm, BMOperator *op)
{
	BMOIter siter;
	BMEdge *e, *e2;
	int ccw = BMO_slot_bool_get(op, "ccw");

#define EDGE_OUT   1
#define FACE_TAINT 1

	BMO_ITER(e, &siter, bm, op, "edges", BM_EDGE) {
		/**
		 * this ends up being called twice, could add option to not to call check in
		 * #BM_edge_rotate to get some extra speed */
		if (BM_edge_rotate_check(bm, e)) {
			BMFace *fa, *fb;
			if (BM_edge_face_pair(e, &fa, &fb)) {

				/* check we're untouched */
				if (BMO_elem_flag_test(bm, fa, FACE_TAINT) == FALSE &&
				    BMO_elem_flag_test(bm, fb, FACE_TAINT) == FALSE)
				{

					if (!(e2 = BM_edge_rotate(bm, e, ccw))) {
						BMO_error_raise(bm, op, BMERR_INVALID_SELECTION, "Could not rotate edge");
						return;
					}

					BMO_elem_flag_enable(bm, e2, EDGE_OUT);

					/* dont touch again */
					BMO_elem_flag_enable(bm, fa, FACE_TAINT);
					BMO_elem_flag_enable(bm, fb, FACE_TAINT);
				}
			}
		}
	}

	BMO_slot_buffer_from_flag(bm, op, "edgeout", EDGE_OUT, BM_EDGE);

#undef EDGE_OUT
#undef FACE_TAINT

}

#define SEL_FLAG	1
#define SEL_ORIG	2

static void bmo_regionextend_extend(BMesh *bm, BMOperator *op, int usefaces)
{
	BMVert *v;
	BMEdge *e;
	BMIter eiter;
	BMOIter siter;

	if (!usefaces) {
		BMO_ITER(v, &siter, bm, op, "geom", BM_VERT) {
			BM_ITER(e, &eiter, bm, BM_EDGES_OF_VERT, v) {
				if (!BMO_elem_flag_test(bm, e, SEL_ORIG))
					break;
			}

			if (e) {
				BM_ITER(e, &eiter, bm, BM_EDGES_OF_VERT, v) {
					BMO_elem_flag_enable(bm, e, SEL_FLAG);
					BMO_elem_flag_enable(bm, BM_edge_other_vert(e, v), SEL_FLAG);
				}
			}
		}
	}
	else {
		BMIter liter, fiter;
		BMFace *f, *f2;
		BMLoop *l;

		BMO_ITER(f, &siter, bm, op, "geom", BM_FACE) {
			BM_ITER(l, &liter, bm, BM_LOOPS_OF_FACE, f) {
				BM_ITER(f2, &fiter, bm, BM_FACES_OF_EDGE, l->e) {
					if (!BMO_elem_flag_test(bm, f2, SEL_ORIG)) {
						BMO_elem_flag_enable(bm, f2, SEL_FLAG);
					}
				}
			}
		}
	}
}

static void bmo_regionextend_constrict(BMesh *bm, BMOperator *op, int usefaces)
{
	BMVert *v;
	BMEdge *e;
	BMIter eiter;
	BMOIter siter;

	if (!usefaces) {
		BMO_ITER(v, &siter, bm, op, "geom", BM_VERT) {
			BM_ITER(e, &eiter, bm, BM_EDGES_OF_VERT, v) {
				if (!BMO_elem_flag_test(bm, e, SEL_ORIG))
					break;
			}

			if (e) {
				BMO_elem_flag_enable(bm, v, SEL_FLAG);

				BM_ITER(e, &eiter, bm, BM_EDGES_OF_VERT, v) {
					BMO_elem_flag_enable(bm, e, SEL_FLAG);
				}

			}
		}
	}
	else {
		BMIter liter, fiter;
		BMFace *f, *f2;
		BMLoop *l;

		BMO_ITER(f, &siter, bm, op, "geom", BM_FACE) {
			BM_ITER(l, &liter, bm, BM_LOOPS_OF_FACE, f) {
				BM_ITER(f2, &fiter, bm, BM_FACES_OF_EDGE, l->e) {
					if (!BMO_elem_flag_test(bm, f2, SEL_ORIG)) {
						BMO_elem_flag_enable(bm, f, SEL_FLAG);
						break;
					}
				}
			}
		}
	}
}

void bmo_regionextend_exec(BMesh *bm, BMOperator *op)
{
	int use_faces = BMO_slot_bool_get(op, "use_faces");
	int constrict = BMO_slot_bool_get(op, "constrict");

	BMO_slot_buffer_flag_enable(bm, op, "geom", SEL_ORIG, BM_ALL);

	if (constrict)
		bmo_regionextend_constrict(bm, op, use_faces);
	else
		bmo_regionextend_extend(bm, op, use_faces);

	BMO_slot_buffer_from_flag(bm, op, "geomout", SEL_FLAG, BM_ALL);
}

/********* righthand faces implementation ****** */

#define FACE_VIS	1
#define FACE_FLAG	2
#define FACE_MARK	4
#define FACE_FLIP	8

/* NOTE: these are the original righthandfaces comment in editmesh_mods.c,
 *       copied here for reference. */

/* based at a select-connected to witness loose objects */

/* count per edge the amount of faces
 * find the ultimate left, front, upper face (not manhattan dist!!)
 * also evaluate both triangle cases in quad, since these can be non-flat
 *
 * put normal to the outside, and set the first direction flags in edges
 *
 * then check the object, and set directions / direction-flags: but only for edges with 1 or 2 faces
 * this is in fact the 'select connected'
 *
 * in case (selected) faces were not done: start over with 'find the ultimate ...' */

/* NOTE: this function uses recursion, which is a little unusual for a bmop
 *       function, but acceptable I think. */

/* NOTE: BM_ELEM_TAG is used on faces to tell if they are flipped. */

void bmo_righthandfaces_exec(BMesh *bm, BMOperator *op)
{
	BMIter liter, liter2;
	BMOIter siter;
	BMFace *f, *startf, **fstack = NULL;
	BLI_array_declare(fstack);
	BMLoop *l, *l2;
	float maxx, maxx_test, cent[3];
	int i, maxi, flagflip = BMO_slot_bool_get(op, "do_flip");

	startf = NULL;
	maxx = -1.0e10;
	
	BMO_slot_buffer_flag_enable(bm, op, "faces", FACE_FLAG, BM_FACE);

	/* find a starting face */
	BMO_ITER(f, &siter, bm, op, "faces", BM_FACE) {

		/* clear dirty flag */
		BM_elem_flag_disable(f, BM_ELEM_TAG);

		if (BMO_elem_flag_test(bm, f, FACE_VIS))
			continue;

		if (!startf) startf = f;

		BM_face_center_bounds_calc(bm, f, cent);

		if ((maxx_test = dot_v3v3(cent, cent)) > maxx) {
			maxx = maxx_test;
			startf = f;
		}
	}

	if (!startf) return;

	BM_face_center_bounds_calc(bm, startf, cent);

	/* make sure the starting face has the correct winding */
	if (dot_v3v3(cent, startf->no) < 0.0f) {
		BM_face_normal_flip(bm, startf);
		BMO_elem_flag_toggle(bm, startf, FACE_FLIP);

		if (flagflip)
			BM_elem_flag_toggle(startf, BM_ELEM_TAG);
	}
	
	/* now that we've found our starting face, make all connected faces
	 * have the same winding.  this is done recursively, using a manual
	 * stack (if we use simple function recursion, we'd end up overloading
	 * the stack on large meshes). */

	BLI_array_growone(fstack);
	fstack[0] = startf;
	BMO_elem_flag_enable(bm, startf, FACE_VIS);

	i = 0;
	maxi = 1;
	while (i >= 0) {
		f = fstack[i];
		i--;

		BM_ITER(l, &liter, bm, BM_LOOPS_OF_FACE, f) {
			BM_ITER(l2, &liter2, bm, BM_LOOPS_OF_LOOP, l) {
				if (!BMO_elem_flag_test(bm, l2->f, FACE_FLAG) || l2 == l)
					continue;

				if (!BMO_elem_flag_test(bm, l2->f, FACE_VIS)) {
					BMO_elem_flag_enable(bm, l2->f, FACE_VIS);
					i++;
					
					if (l2->v == l->v) {
						BM_face_normal_flip(bm, l2->f);
						
						BMO_elem_flag_toggle(bm, l2->f, FACE_FLIP);
						if (flagflip)
							BM_elem_flag_toggle(l2->f, BM_ELEM_TAG);
					}
					else if (BM_elem_flag_test(l2->f, BM_ELEM_TAG) || BM_elem_flag_test(l->f, BM_ELEM_TAG)) {
						if (flagflip) {
							BM_elem_flag_disable(l->f, BM_ELEM_TAG);
							BM_elem_flag_disable(l2->f, BM_ELEM_TAG);
						}
					}
					
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

	/* check if we have faces yet to do.  if so, recurse */
	BMO_ITER(f, &siter, bm, op, "faces", BM_FACE) {
		if (!BMO_elem_flag_test(bm, f, FACE_VIS)) {
			bmo_righthandfaces_exec(bm, op);
			break;
		}
	}
}

void bmo_vertexsmooth_exec(BMesh *bm, BMOperator *op)
{
	BMOIter siter;
	BMIter iter;
	BMVert *v;
	BMEdge *e;
	BLI_array_declare(cos);
	float (*cos)[3] = NULL;
	float *co, *co2, clipdist = BMO_slot_float_get(op, "clipdist");
	int i, j, clipx, clipy, clipz;
	
	clipx = BMO_slot_bool_get(op, "mirror_clip_x");
	clipy = BMO_slot_bool_get(op, "mirror_clip_y");
	clipz = BMO_slot_bool_get(op, "mirror_clip_z");

	i = 0;
	BMO_ITER(v, &siter, bm, op, "verts", BM_VERT) {
		BLI_array_growone(cos);
		co = cos[i];
		
		j  = 0;
		BM_ITER(e, &iter, bm, BM_EDGES_OF_VERT, v) {
			co2 = BM_edge_other_vert(e, v)->co;
			add_v3_v3v3(co, co, co2);
			j += 1;
		}
		
		if (!j) {
			copy_v3_v3(co, v->co);
			i++;
			continue;
		}

		mul_v3_fl(co, 1.0f / (float)j);
		mid_v3_v3v3(co, co, v->co);

		if (clipx && fabsf(v->co[0]) <= clipdist)
			co[0] = 0.0f;
		if (clipy && fabsf(v->co[1]) <= clipdist)
			co[1] = 0.0f;
		if (clipz && fabsf(v->co[2]) <= clipdist)
			co[2] = 0.0f;

		i++;
	}

	i = 0;
	BMO_ITER(v, &siter, bm, op, "verts", BM_VERT) {
		copy_v3_v3(v->co, cos[i]);
		i++;
	}

	BLI_array_free(cos);
}

/*
 * compute the perimeter of an ngon
 *
 * NOTE: This should probably go to bmesh_polygon.c
 */
static float ngon_perimeter(BMesh *bm, BMFace *f)
{
	BMIter	liter;
	BMLoop	*l;
	int		num_verts = 0;
	float	v[3], sv[3];
	float	perimeter = 0.0f;

	BM_ITER(l, &liter, bm, BM_LOOPS_OF_FACE, f) {
		if (num_verts == 0) {
			copy_v3_v3(v, l->v->co);
			copy_v3_v3(sv, l->v->co);
		}
		else {
			perimeter += len_v3v3(v, l->v->co);
			copy_v3_v3(v, l->v->co);
		}
		num_verts++;
	}

	perimeter += len_v3v3(v, sv);

	return perimeter;
}

/*
 * compute the fake surface of an ngon
 * This is done by decomposing the ngon into triangles who share the centroid of the ngon
 * while this method is far from being exact, it should garantee an invariance.
 *
 * NOTE: This should probably go to bmesh_polygon.c
 */
static float ngon_fake_area(BMesh *bm, BMFace *f)
{
	BMIter	liter;
	BMLoop	*l;
	int		num_verts = 0;
	float	v[3], sv[3], c[3];
	float	area = 0.0f;

	BM_face_center_mean_calc(bm, f, c);

	BM_ITER(l, &liter, bm, BM_LOOPS_OF_FACE, f) {
		if (num_verts == 0) {
			copy_v3_v3(v, l->v->co);
			copy_v3_v3(sv, l->v->co);
			num_verts++;
		}
		else {
			area += area_tri_v3(v, c, l->v->co);
			copy_v3_v3(v, l->v->co);
			num_verts++;
		}
	}

	area += area_tri_v3(v, c, sv);

	return area;
}

/*
 * extra face data (computed data)
 */
typedef struct SimSel_FaceExt {
	BMFace		*f;			/* the face */
	float	c[3];			/* center */
	union {
		float	area;		/* area */
		float	perim;		/* perimeter */
		float	d;			/* 4th component of plane (the first three being the normal) */
		struct Image	*t;	/* image pointer */
	};
} SimSel_FaceExt;

/*
 * Select similar faces, the choices are in the enum in source/blender/bmesh/bmesh_operators.h
 * We select either similar faces based on material, image, area, perimeter, normal, or the coplanar faces
 */
void bmo_similarfaces_exec(BMesh *bm, BMOperator *op)
{
	BMIter fm_iter;
	BMFace *fs, *fm;
	BMOIter fs_iter;
	int num_sels = 0, num_total = 0, i = 0, idx = 0;
	float angle = 0.0f;
	SimSel_FaceExt *f_ext = NULL;
	int *indices = NULL;
	float t_no[3];	/* temporary normal */
	int type = BMO_slot_int_get(op, "type");
	float thresh = BMO_slot_float_get(op, "thresh");

	num_total = BM_mesh_elem_count(bm, BM_FACE);

	/*
	** The first thing to do is to iterate through all the the selected items and mark them since
	** they will be in the selection anyway.
	** This will increase performance, (especially when the number of originaly selected faces is high)
	** so the overall complexity will be less than $O(mn)$ where is the total number of selected faces,
	** and n is the total number of faces
	*/
	BMO_ITER(fs, &fs_iter, bm, op, "faces", BM_FACE) {
		if (!BMO_elem_flag_test(bm, fs, FACE_MARK)) {	/* is this really needed ? */
			BMO_elem_flag_enable(bm, fs, FACE_MARK);
			num_sels++;
		}
	}

	/* allocate memory for the selected faces indices and for all temporary faces */
	indices	= (int *)MEM_callocN(sizeof(int) * num_sels, "face indices util.c");
	f_ext = (SimSel_FaceExt *)MEM_callocN(sizeof(SimSel_FaceExt) * num_total, "f_ext util.c");

	/* loop through all the faces and fill the faces/indices structure */
	BM_ITER(fm, &fm_iter, bm, BM_FACES_OF_MESH, NULL) {
		f_ext[i].f = fm;
		if (BMO_elem_flag_test(bm, fm, FACE_MARK)) {
			indices[idx] = i;
			idx++;
		}
		i++;
	}

	/*
	** Save us some computation burden: In case of perimeter/area/coplanar selection we compute
	** only once.
	*/
	if (type == SIMFACE_PERIMETER || type == SIMFACE_AREA || type == SIMFACE_COPLANAR || type == SIMFACE_IMAGE) {
		for (i = 0; i < num_total; i++) {
			switch (type) {
				case SIMFACE_PERIMETER:
					/* set the perimeter */
					f_ext[i].perim = ngon_perimeter(bm, f_ext[i].f);
					break;

				case SIMFACE_COPLANAR:
					/* compute the center of the polygon */
					BM_face_center_mean_calc(bm, f_ext[i].f, f_ext[i].c);

					/* normalize the polygon normal */
					copy_v3_v3(t_no, f_ext[i].f->no);
					normalize_v3(t_no);

					/* compute the plane distance */
					f_ext[i].d = dot_v3v3(t_no, f_ext[i].c);
					break;

				case SIMFACE_AREA:
					f_ext[i].area = ngon_fake_area(bm, f_ext[i].f);
					break;

				case SIMFACE_IMAGE:
					f_ext[i].t = NULL;
					if (CustomData_has_layer(&(bm->pdata), CD_MTEXPOLY)) {
						MTexPoly *mtpoly = CustomData_bmesh_get(&bm->pdata, f_ext[i].f->head.data, CD_MTEXPOLY);
						f_ext[i].t = mtpoly->tpage;
					}
					break;
			}
		}
	}

	/* now select the rest (if any) */
	for (i = 0; i < num_total; i++) {
		fm = f_ext[i].f;
		if (!BMO_elem_flag_test(bm, fm, FACE_MARK)  && !BM_elem_flag_test(fm, BM_ELEM_HIDDEN)) {
			int cont = TRUE;
			for (idx = 0; idx < num_sels && cont == TRUE; idx++) {
				fs = f_ext[indices[idx]].f;
				switch (type) {
					case SIMFACE_MATERIAL:
						if (fm->mat_nr == fs->mat_nr) {
							BMO_elem_flag_enable(bm, fm, FACE_MARK);
							cont = FALSE;
						}
						break;

					case SIMFACE_IMAGE:
						if (f_ext[i].t == f_ext[indices[idx]].t) {
							BMO_elem_flag_enable(bm, fm, FACE_MARK);
							cont = FALSE;
						}
						break;

					case SIMFACE_NORMAL:
						angle = RAD2DEGF(angle_v3v3(fs->no, fm->no));	/* if the angle between the normals -> 0 */
						if (angle / 180.0f <= thresh) {
							BMO_elem_flag_enable(bm, fm, FACE_MARK);
							cont = FALSE;
						}
						break;

					case SIMFACE_COPLANAR:
						angle = RAD2DEGF(angle_v3v3(fs->no, fm->no)); /* angle -> 0 */
						if (angle / 180.0f <= thresh) { /* and dot product difference -> 0 */
							if (fabsf(f_ext[i].d - f_ext[indices[idx]].d) <= thresh) {
								BMO_elem_flag_enable(bm, fm, FACE_MARK);
								cont = FALSE;
							}
						}
						break;

					case SIMFACE_AREA:
						if (fabsf(f_ext[i].area - f_ext[indices[idx]].area) <= thresh) {
							BMO_elem_flag_enable(bm, fm, FACE_MARK);
							cont = FALSE;
						}
						break;

					case SIMFACE_PERIMETER:
						if (fabsf(f_ext[i].perim - f_ext[indices[idx]].perim) <= thresh) {
							BMO_elem_flag_enable(bm, fm, FACE_MARK);
							cont = FALSE;
						}
						break;
				}
			}
		}
	}

	MEM_freeN(f_ext);
	MEM_freeN(indices);

	/* transfer all marked faces to the output slot */
	BMO_slot_buffer_from_flag(bm, op, "faceout", FACE_MARK, BM_FACE);
}

/******************************************************************************
** Similar Edges
**************************************************************************** */
#define EDGE_MARK	1

/*
 * compute the angle of an edge (i.e. the angle between two faces)
 */
static float edge_angle(BMesh *bm, BMEdge *e)
{
	BMIter	fiter;
	BMFace	*f, *f_prev = NULL;

	/* first edge faces, dont account for 3+ */

	BM_ITER(f, &fiter, bm, BM_FACES_OF_EDGE, e) {
		if (f_prev == NULL) {
			f_prev = f;
		}
		else {
			return angle_v3v3(f_prev->no, f->no);
		}
	}

	return 0.0f;
}
/*
 * extra edge information
 */
typedef struct SimSel_EdgeExt {
	BMEdge		*e;
	union {
		float		dir[3];
		float		angle;			/* angle between the face */
	};

	union {
		float		length;			/* edge length */
		int			faces;			/* faces count */
	};
} SimSel_EdgeExt;

/*
 * select similar edges: the choices are in the enum in source/blender/bmesh/bmesh_operators.h
 * choices are length, direction, face, ...
 */
void bmo_similaredges_exec(BMesh *bm, BMOperator *op)
{
	BMOIter es_iter;	/* selected edges iterator */
	BMIter	e_iter;		/* mesh edges iterator */
	BMEdge	*es;		/* selected edge */
	BMEdge	*e;		/* mesh edge */
	int idx = 0, i = 0 /* , f = 0 */;
	int *indices = NULL;
	SimSel_EdgeExt *e_ext = NULL;
	// float *angles = NULL;
	float angle;

	int num_sels = 0, num_total = 0;
	int type = BMO_slot_int_get(op, "type");
	float thresh = BMO_slot_float_get(op, "thresh");

	num_total = BM_mesh_elem_count(bm, BM_EDGE);

	/* iterate through all selected edges and mark them */
	BMO_ITER(es, &es_iter, bm, op, "edges", BM_EDGE) {
		BMO_elem_flag_enable(bm, es, EDGE_MARK);
		num_sels++;
	}

	/* allocate memory for the selected edges indices and for all temporary edges */
	indices	= (int *)MEM_callocN(sizeof(int) * num_sels, "indices util.c");
	e_ext = (SimSel_EdgeExt *)MEM_callocN(sizeof(SimSel_EdgeExt) * num_total, "e_ext util.c");

	/* loop through all the edges and fill the edges/indices structure */
	BM_ITER(e, &e_iter, bm, BM_EDGES_OF_MESH, NULL) {
		e_ext[i].e = e;
		if (BMO_elem_flag_test(bm, e, EDGE_MARK)) {
			indices[idx] = i;
			idx++;
		}
		i++;
	}

	/* save us some computation time by doing heavy computation once */
	if (type == SIMEDGE_LENGTH || type == SIMEDGE_FACE || type == SIMEDGE_DIR || type == SIMEDGE_FACE_ANGLE) {
		for (i = 0; i < num_total; i++) {
			switch (type) {
				case SIMEDGE_LENGTH:	/* compute the length of the edge */
					e_ext[i].length	= len_v3v3(e_ext[i].e->v1->co, e_ext[i].e->v2->co);
					break;

				case SIMEDGE_DIR:		/* compute the direction */
					sub_v3_v3v3(e_ext[i].dir, e_ext[i].e->v1->co, e_ext[i].e->v2->co);
					break;

				case SIMEDGE_FACE:		/* count the faces around the edge */
					e_ext[i].faces	= BM_edge_face_count(e_ext[i].e);
					break;

				case SIMEDGE_FACE_ANGLE:
					e_ext[i].faces	= BM_edge_face_count(e_ext[i].e);
					if (e_ext[i].faces == 2)
						e_ext[i].angle = edge_angle(bm, e_ext[i].e);
					break;
			}
		}
	}

	/* select the edges if any */
	for (i = 0; i < num_total; i++) {
		e = e_ext[i].e;
		if (!BMO_elem_flag_test(bm, e, EDGE_MARK) && !BM_elem_flag_test(e, BM_ELEM_HIDDEN)) {
			int cont = TRUE;
			for (idx = 0; idx < num_sels && cont == TRUE; idx++) {
				es = e_ext[indices[idx]].e;
				switch (type) {
					case SIMEDGE_LENGTH:
						if (fabsf(e_ext[i].length - e_ext[indices[idx]].length) <= thresh) {
							BMO_elem_flag_enable(bm, e, EDGE_MARK);
							cont = FALSE;
						}
						break;

					case SIMEDGE_DIR:
						/* compute the angle between the two edges */
						angle = RAD2DEGF(angle_v3v3(e_ext[i].dir, e_ext[indices[idx]].dir));

						if (angle > 90.0f) /* use the smallest angle between the edges */
							angle = fabsf(angle - 180.0f);

						if (angle / 90.0f <= thresh) {
							BMO_elem_flag_enable(bm, e, EDGE_MARK);
							cont = FALSE;
						}
						break;

					case SIMEDGE_FACE:
						if (e_ext[i].faces == e_ext[indices[idx]].faces) {
							BMO_elem_flag_enable(bm, e, EDGE_MARK);
							cont = FALSE;
						}
						break;

					case SIMEDGE_FACE_ANGLE:
						if (e_ext[i].faces == 2) {
							if (e_ext[indices[idx]].faces == 2) {
								if (fabsf(e_ext[i].angle - e_ext[indices[idx]].angle) <= thresh) {
									BMO_elem_flag_enable(bm, e, EDGE_MARK);
									cont = FALSE;
								}
							}
						}
						else {
							cont = FALSE;
						}
						break;

					case SIMEDGE_CREASE:
						if (CustomData_has_layer(&bm->edata, CD_CREASE)) {
							float *c1, *c2;

							c1 = CustomData_bmesh_get(&bm->edata, e->head.data, CD_CREASE);
							c2 = CustomData_bmesh_get(&bm->edata, es->head.data, CD_CREASE);

							if (c1 && c2 && fabsf(*c1 - *c2) <= thresh) {
								BMO_elem_flag_enable(bm, e, EDGE_MARK);
								cont = FALSE;
							}
						}
						break;

					case SIMEDGE_SEAM:
						if (BM_elem_flag_test(e, BM_ELEM_SEAM) == BM_elem_flag_test(es, BM_ELEM_SEAM)) {
							BMO_elem_flag_enable(bm, e, EDGE_MARK);
							cont = FALSE;
						}
						break;

					case SIMEDGE_SHARP:
						if (BM_elem_flag_test(e, BM_ELEM_SMOOTH) == BM_elem_flag_test(es, BM_ELEM_SMOOTH)) {
							BMO_elem_flag_enable(bm, e, EDGE_MARK);
							cont = FALSE;
						}
						break;
				}
			}
		}
	}

	MEM_freeN(e_ext);
	MEM_freeN(indices);

	/* transfer all marked edges to the output slot */
	BMO_slot_buffer_from_flag(bm, op, "edgeout", EDGE_MARK, BM_EDGE);
}

/******************************************************************************
** Similar Vertices
**************************************************************************** */
#define VERT_MARK	1

typedef struct SimSel_VertExt {
	BMVert *v;
	union {
		int num_faces; /* adjacent faces */
		MDeformVert *dvert; /* deform vertex */
	};
} SimSel_VertExt;

/*
 * select similar vertices: the choices are in the enum in source/blender/bmesh/bmesh_operators.h
 * choices are normal, face, vertex group...
 */
void bmo_similarverts_exec(BMesh *bm, BMOperator *op)
{
	BMOIter vs_iter;	/* selected verts iterator */
	BMIter v_iter;		/* mesh verts iterator */
	BMVert *vs;		/* selected vertex */
	BMVert *v;			/* mesh vertex */
	SimSel_VertExt *v_ext = NULL;
	int *indices = NULL;
	int num_total = 0, num_sels = 0, i = 0, idx = 0;
	int type = BMO_slot_int_get(op, "type");
	float thresh = BMO_slot_float_get(op, "thresh");

	num_total = BM_mesh_elem_count(bm, BM_VERT);

	/* iterate through all selected edges and mark them */
	BMO_ITER(vs, &vs_iter, bm, op, "verts", BM_VERT) {
		BMO_elem_flag_enable(bm, vs, VERT_MARK);
		num_sels++;
	}

	/* allocate memory for the selected vertices indices and for all temporary vertices */
	indices	= (int *)MEM_mallocN(sizeof(int) * num_sels, "vertex indices");
	v_ext = (SimSel_VertExt *)MEM_mallocN(sizeof(SimSel_VertExt) * num_total, "vertex extra");

	/* loop through all the vertices and fill the vertices/indices structure */
	BM_ITER(v, &v_iter, bm, BM_VERTS_OF_MESH, NULL) {
		v_ext[i].v = v;
		if (BMO_elem_flag_test(bm, v, VERT_MARK)) {
			indices[idx] = i;
			idx++;
		}

		switch (type) {
			case SIMVERT_FACE:
				/* calling BM_vert_face_count every time is time consumming, so call it only once per vertex */
				v_ext[i].num_faces	= BM_vert_face_count(v);
				break;

			case SIMVERT_VGROUP:
				if (CustomData_has_layer(&(bm->vdata), CD_MDEFORMVERT)) {
					v_ext[i].dvert = CustomData_bmesh_get(&bm->vdata, v_ext[i].v->head.data, CD_MDEFORMVERT);
				}
				else {
					v_ext[i].dvert = NULL;
				}
				break;
		}

		i++;
	}

	/* select the vertices if any */
	for (i = 0; i < num_total; i++) {
		v = v_ext[i].v;
		if (!BMO_elem_flag_test(bm, v, VERT_MARK) && !BM_elem_flag_test(v, BM_ELEM_HIDDEN)) {
			int cont = TRUE;
			for (idx = 0; idx < num_sels && cont == TRUE; idx++) {
				vs = v_ext[indices[idx]].v;
				switch (type) {
					case SIMVERT_NORMAL:
						/* compare the angle between the normals */
						if (RAD2DEGF(angle_v3v3(v->no, vs->no)) / 180.0f <= thresh) {
							BMO_elem_flag_enable(bm, v, VERT_MARK);
							cont = FALSE;
						}
						break;
					case SIMVERT_FACE:
						/* number of adjacent faces */
						if (v_ext[i].num_faces == v_ext[indices[idx]].num_faces) {
							BMO_elem_flag_enable(bm, v, VERT_MARK);
							cont = FALSE;
						}
						break;

					case SIMVERT_VGROUP:
						if (v_ext[i].dvert != NULL && v_ext[indices[idx]].dvert != NULL) {
							int v1, v2;
							for (v1 = 0; v1 < v_ext[i].dvert->totweight && cont == 1; v1++) {
								for (v2 = 0; v2 < v_ext[indices[idx]].dvert->totweight; v2++) {
									if (v_ext[i].dvert->dw[v1].def_nr == v_ext[indices[idx]].dvert->dw[v2].def_nr) {
										BMO_elem_flag_enable(bm, v, VERT_MARK);
										cont = FALSE;
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

	BMO_slot_buffer_from_flag(bm, op, "vertout", VERT_MARK, BM_VERT);
}

/******************************************************************************
** Cycle UVs for a face
**************************************************************************** */

void bmo_face_rotateuvs_exec(BMesh *bm, BMOperator *op)
{
	BMOIter fs_iter;	/* selected faces iterator */
	BMFace *fs;	/* current face */
	BMIter l_iter;	/* iteration loop */
	// int n;

	int dir = BMO_slot_int_get(op, "dir");

	BMO_ITER(fs, &fs_iter, bm, op, "faces", BM_FACE) {
		if (CustomData_has_layer(&(bm->ldata), CD_MLOOPUV)) {
			if (dir == DIRECTION_CW) { /* same loops direction */
				BMLoop *lf;	/* current face loops */
				MLoopUV *f_luv; /* first face loop uv */
				float p_uv[2];	/* previous uvs */
				float t_uv[2];	/* tmp uvs */

				int n = 0;
				BM_ITER(lf, &l_iter, bm, BM_LOOPS_OF_FACE, fs) {
					/* current loop uv is the previous loop uv */
					MLoopUV *luv = CustomData_bmesh_get(&bm->ldata, lf->head.data, CD_MLOOPUV);
					if (n == 0) {
						f_luv = luv;
						copy_v2_v2(p_uv, luv->uv);
					}
					else {
						copy_v2_v2(t_uv, luv->uv);
						copy_v2_v2(luv->uv, p_uv);
						copy_v2_v2(p_uv, t_uv);
					}
					n++;
				}

				copy_v2_v2(f_luv->uv, p_uv);
			}
			else if (dir == DIRECTION_CCW) { /* counter loop direction */
				BMLoop *lf;	/* current face loops */
				MLoopUV *p_luv; /* previous loop uv */
				MLoopUV *luv;
				float t_uv[2];	/* current uvs */

				int n = 0;
				BM_ITER(lf, &l_iter, bm, BM_LOOPS_OF_FACE, fs) {
					/* previous loop uv is the current loop uv */
					luv = CustomData_bmesh_get(&bm->ldata, lf->head.data, CD_MLOOPUV);
					if (n == 0) {
						p_luv = luv;
						copy_v2_v2(t_uv, luv->uv);
					}
					else {
						copy_v2_v2(p_luv->uv, luv->uv);
						p_luv = luv;
					}
					n++;
				}

				copy_v2_v2(luv->uv, t_uv);
			}
		}
	}

}

/******************************************************************************
** Reverse UVs for a face
**************************************************************************** */

void bmo_face_reverseuvs_exec(BMesh *bm, BMOperator *op)
{
	BMOIter fs_iter;	/* selected faces iterator */
	BMFace *fs;		/* current face */
	BMIter l_iter;		/* iteration loop */
	BLI_array_declare(uvs);
	float (*uvs)[2] = NULL;

	BMO_ITER(fs, &fs_iter, bm, op, "faces", BM_FACE) {
		if (CustomData_has_layer(&(bm->ldata), CD_MLOOPUV)) {
			BMLoop *lf;	/* current face loops */
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
**************************************************************************** */

void bmo_rotatecolors_exec(BMesh *bm, BMOperator *op)
{
	BMOIter fs_iter;	/* selected faces iterator */
	BMFace *fs;	/* current face */
	BMIter l_iter;	/* iteration loop */
	// int n;

	int dir = BMO_slot_int_get(op, "dir");

	BMO_ITER(fs, &fs_iter, bm, op, "faces", BM_FACE) {
		if (CustomData_has_layer(&(bm->ldata), CD_MLOOPCOL)) {
			if (dir == DIRECTION_CW) { /* same loops direction */
				BMLoop *lf;	/* current face loops */
				MLoopCol *f_lcol; /* first face loop color */
				MLoopCol p_col;	/* previous color */
				MLoopCol t_col;	/* tmp color */

				int n = 0;
				BM_ITER(lf, &l_iter, bm, BM_LOOPS_OF_FACE, fs) {
					/* current loop color is the previous loop color */
					MLoopCol *luv = CustomData_bmesh_get(&bm->ldata, lf->head.data, CD_MLOOPCOL);
					if (n == 0) {
						f_lcol = luv;
						p_col = *luv;
					}
					else {
						t_col = *luv;
						*luv = p_col;
						p_col = t_col;
					}
					n++;
				}

				*f_lcol = p_col;
			}
			else if (dir == DIRECTION_CCW) { /* counter loop direction */
				BMLoop *lf;	/* current face loops */
				MLoopCol *p_lcol; /* previous loop color */
				MLoopCol *lcol;
				MLoopCol t_col;	/* current color */

				int n = 0;
				BM_ITER(lf, &l_iter, bm, BM_LOOPS_OF_FACE, fs) {
					/* previous loop color is the current loop color */
					lcol = CustomData_bmesh_get(&bm->ldata, lf->head.data, CD_MLOOPCOL);
					if (n == 0) {
						p_lcol = lcol;
						t_col = *lcol;
					}
					else {
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
**************************************************************************** */

void bmo_face_reversecolors_exec(BMesh *bm, BMOperator *op)
{
	BMOIter fs_iter;	/* selected faces iterator */
	BMFace *fs;		/* current face */
	BMIter l_iter;		/* iteration loop */
	BLI_array_declare(cols);
	MLoopCol *cols = NULL;

	BMO_ITER(fs, &fs_iter, bm, op, "faces", BM_FACE) {
		if (CustomData_has_layer(&(bm->ldata), CD_MLOOPCOL)) {
			BMLoop *lf;	/* current face loops */
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
**************************************************************************** */

typedef struct ElemNode {
	BMVert *v;	/* vertex */
	BMVert *parent;	/* node parent id */
	float weight;	/* node weight */
	HeapNode *hn;	/* heap node */
} ElemNode;

void bmo_vertexshortestpath_exec(BMesh *bm, BMOperator *op)
{
	BMOIter vs_iter /* , vs2_iter */;	/* selected verts iterator */
	BMIter v_iter;		/* mesh verts iterator */
	BMVert *vs, *sv, *ev;	/* starting vertex, ending vertex */
	BMVert *v;		/* mesh vertex */
	Heap *h = NULL;

	ElemNode *vert_list = NULL;

	int num_total = 0 /*, num_sels = 0 */, i = 0;
	int type = BMO_slot_int_get(op, "type");

	BMO_ITER(vs, &vs_iter, bm, op, "startv", BM_VERT) {
		sv = vs;
	}
	BMO_ITER(vs, &vs_iter, bm, op, "endv", BM_VERT) {
		ev = vs;
	}

	num_total = BM_mesh_elem_count(bm, BM_VERT);

	/* allocate memory for the nodes */
	vert_list = (ElemNode *)MEM_mallocN(sizeof(ElemNode) * num_total, "vertex nodes");

	/* iterate through all the mesh vertices */
	/* loop through all the vertices and fill the vertices/indices structure */
	i = 0;
	BM_ITER(v, &v_iter, bm, BM_VERTS_OF_MESH, NULL) {
		vert_list[i].v = v;
		vert_list[i].parent = NULL;
		vert_list[i].weight = FLT_MAX;
		BM_elem_index_set(v, i); /* set_inline */
		i++;
	}
	bm->elem_index_dirty &= ~BM_VERT;

	/*
	** we now have everything we need, start Dijkstra path finding algorithm
	*/

	/* set the distance/weight of the start vertex to 0 */
	vert_list[BM_elem_index_get(sv)].weight = 0.0f;

	h = BLI_heap_new();

	for (i = 0; i < num_total; i++) {
		vert_list[i].hn = BLI_heap_insert(h, vert_list[i].weight, vert_list[i].v);
	}

	while (!BLI_heap_empty(h)) {
		BMEdge *e;
		BMIter e_i;
		float v_weight;

		/* take the vertex with the lowest weight out of the heap */
		BMVert *v = (BMVert *)BLI_heap_popmin(h);

		if (vert_list[BM_elem_index_get(v)].weight == FLT_MAX) /* this means that there is no path */
			break;

		v_weight = vert_list[BM_elem_index_get(v)].weight;

		BM_ITER(e, &e_i, bm, BM_EDGES_OF_VERT, v) {
			BMVert *u;
			float e_weight = v_weight;

			if (type == VPATH_SELECT_EDGE_LENGTH)
				e_weight += len_v3v3(e->v1->co, e->v2->co);
			else e_weight += 1.0f;

			u = (e->v1 == v) ? e->v2 : e->v1;

			if (e_weight < vert_list[BM_elem_index_get(u)].weight) { /* is this path shorter ? */
				/* add it if so */
				vert_list[BM_elem_index_get(u)].parent = v;
				vert_list[BM_elem_index_get(u)].weight = e_weight;

				/* we should do a heap update node function!!! :-/ */
				BLI_heap_remove(h, vert_list[BM_elem_index_get(u)].hn);
				BLI_heap_insert(h, e_weight, u);
			}
		}
	}

	/* now we trace the path (if it exists) */
	v = ev;

	while (vert_list[BM_elem_index_get(v)].parent != NULL) {
		BMO_elem_flag_enable(bm, v, VERT_MARK);
		v = vert_list[BM_elem_index_get(v)].parent;
	}

	BLI_heap_free(h, NULL);
	MEM_freeN(vert_list);

	BMO_slot_buffer_from_flag(bm, op, "vertout", VERT_MARK, BM_VERT);
}
