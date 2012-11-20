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

/** \file blender/bmesh/operators/bmo_utils.c
 *  \ingroup bmesh
 *
 * utility bmesh operators, e.g. transform,
 * translate, rotate, scale, etc.
 */

#include "MEM_guardedalloc.h"

#include "DNA_meshdata_types.h"

#include "BLI_math.h"
#include "BLI_array.h"
#include "BLI_heap.h"

#include "BKE_customdata.h"

#include "bmesh.h"

#include "intern/bmesh_operators_private.h" /* own include */

void bmo_create_vert_exec(BMesh *bm, BMOperator *op)
{
	float vec[3];

	BMO_slot_vec_get(op->slots_in, "co", vec);

	BMO_elem_flag_enable(bm, BM_vert_create(bm, vec, NULL), 1);
	BMO_slot_buffer_from_enabled_flag(bm, op, op->slots_out, "vert.out", BM_VERT, 1);
}

void bmo_transform_exec(BMesh *UNUSED(bm), BMOperator *op)
{
	BMOIter iter;
	BMVert *v;
	float mat[4][4];

	BMO_slot_mat4_get(op->slots_in, "mat", mat);

	BMO_ITER (v, &iter, op->slots_in, "verts", BM_VERT) {
		mul_m4_v3(mat, v->co);
	}
}

void bmo_translate_exec(BMesh *bm, BMOperator *op)
{
	float mat[4][4], vec[3];
	
	BMO_slot_vec_get(op->slots_in, "vec", vec);

	unit_m4(mat);
	copy_v3_v3(mat[3], vec);

	BMO_op_callf(bm, op->flag, "transform mat=%m4 verts=%s", mat, op, "verts");
}

void bmo_scale_exec(BMesh *bm, BMOperator *op)
{
	float mat[3][3], vec[3];
	
	BMO_slot_vec_get(op->slots_in, "vec", vec);

	unit_m3(mat);
	mat[0][0] = vec[0];
	mat[1][1] = vec[1];
	mat[2][2] = vec[2];

	BMO_op_callf(bm, op->flag, "transform mat=%m3 verts=%s", mat, op, "verts");
}

void bmo_rotate_exec(BMesh *bm, BMOperator *op)
{
	float vec[3];
	
	BMO_slot_vec_get(op->slots_in, "cent", vec);
	
	/* there has to be a proper matrix way to do this, but
	 * this is how editmesh did it and I'm too tired to think
	 * through the math right now. */
	mul_v3_fl(vec, -1.0f);
	BMO_op_callf(bm, op->flag, "translate verts=%s vec=%v", op, "verts", vec);

	BMO_op_callf(bm, op->flag, "transform mat=%s verts=%s", op, "mat", op, "verts");

	mul_v3_fl(vec, -1.0f);
	BMO_op_callf(bm, op->flag, "translate verts=%s vec=%v", op, "verts", vec);
}

void bmo_reverse_faces_exec(BMesh *bm, BMOperator *op)
{
	BMOIter siter;
	BMFace *f;

	BMO_ITER (f, &siter, op->slots_in, "faces", BM_FACE) {
		BM_face_normal_flip(bm, f);
	}
}

void bmo_rotate_edges_exec(BMesh *bm, BMOperator *op)
{
	BMOIter siter;
	BMEdge *e, *e2;
	const int use_ccw = BMO_slot_bool_get(op->slots_in, "use_ccw");
	const int is_single = BMO_slot_buffer_count(op->slots_in, "edges") == 1;
	short check_flag = is_single ?
	            BM_EDGEROT_CHECK_EXISTS :
	            BM_EDGEROT_CHECK_EXISTS | BM_EDGEROT_CHECK_DEGENERATE;

#define EDGE_OUT   1
#define FACE_TAINT 1

	BMO_ITER (e, &siter, op->slots_in, "edges", BM_EDGE) {
		/**
		 * this ends up being called twice, could add option to not to call check in
		 * #BM_edge_rotate to get some extra speed */
		if (BM_edge_rotate_check(e)) {
			BMFace *fa, *fb;
			if (BM_edge_face_pair(e, &fa, &fb)) {

				/* check we're untouched */
				if (BMO_elem_flag_test(bm, fa, FACE_TAINT) == FALSE &&
				    BMO_elem_flag_test(bm, fb, FACE_TAINT) == FALSE)
				{

					if (!(e2 = BM_edge_rotate(bm, e, use_ccw, check_flag))) {
#if 0
						BMO_error_raise(bm, op, BMERR_INVALID_SELECTION, "Could not rotate edge");
						return;
#endif
						continue;
					}

					BMO_elem_flag_enable(bm, e2, EDGE_OUT);

					/* don't touch again */
					BMO_elem_flag_enable(bm, fa, FACE_TAINT);
					BMO_elem_flag_enable(bm, fb, FACE_TAINT);
				}
			}
		}
	}

	BMO_slot_buffer_from_enabled_flag(bm, op, op->slots_out, "edges.out", BM_EDGE, EDGE_OUT);

#undef EDGE_OUT
#undef FACE_TAINT

}

#define SEL_FLAG	1
#define SEL_ORIG	2

static void bmo_region_extend_extend(BMesh *bm, BMOperator *op, int usefaces)
{
	BMVert *v;
	BMEdge *e;
	BMIter eiter;
	BMOIter siter;

	if (!usefaces) {
		BMO_ITER (v, &siter, op->slots_in, "geom", BM_VERT) {
			BM_ITER_ELEM (e, &eiter, v, BM_EDGES_OF_VERT) {
				if (!BM_elem_flag_test(e, BM_ELEM_HIDDEN))
					if (!BMO_elem_flag_test(bm, e, SEL_ORIG))
						break;
			}

			if (e) {
				BM_ITER_ELEM (e, &eiter, v, BM_EDGES_OF_VERT) {
					if (!BM_elem_flag_test(e, BM_ELEM_HIDDEN)) {
						BMO_elem_flag_enable(bm, e, SEL_FLAG);
						BMO_elem_flag_enable(bm, BM_edge_other_vert(e, v), SEL_FLAG);
					}
				}
			}
		}
	}
	else {
		BMIter liter, fiter;
		BMFace *f, *f2;
		BMLoop *l;

		BMO_ITER (f, &siter, op->slots_in, "geom", BM_FACE) {
			BM_ITER_ELEM (l, &liter, f, BM_LOOPS_OF_FACE) {
				BM_ITER_ELEM (f2, &fiter, l->e, BM_FACES_OF_EDGE) {
					if (!BM_elem_flag_test(f2, BM_ELEM_HIDDEN)) {
						if (!BMO_elem_flag_test(bm, f2, SEL_ORIG)) {
							BMO_elem_flag_enable(bm, f2, SEL_FLAG);
						}
					}
				}
			}
		}
	}
}

static void bmo_region_extend_constrict(BMesh *bm, BMOperator *op, int usefaces)
{
	BMVert *v;
	BMEdge *e;
	BMIter eiter;
	BMOIter siter;

	if (!usefaces) {
		BMO_ITER (v, &siter, op->slots_in, "geom", BM_VERT) {
			BM_ITER_ELEM (e, &eiter, v, BM_EDGES_OF_VERT) {
				if (!BM_elem_flag_test(e, BM_ELEM_HIDDEN))
					if (!BMO_elem_flag_test(bm, e, SEL_ORIG))
						break;
			}

			if (e) {
				BMO_elem_flag_enable(bm, v, SEL_FLAG);

				BM_ITER_ELEM (e, &eiter, v, BM_EDGES_OF_VERT) {
					if (!BM_elem_flag_test(e, BM_ELEM_HIDDEN)) {
						BMO_elem_flag_enable(bm, e, SEL_FLAG);
					}
				}

			}
		}
	}
	else {
		BMIter liter, fiter;
		BMFace *f, *f2;
		BMLoop *l;

		BMO_ITER (f, &siter, op->slots_in, "geom", BM_FACE) {
			BM_ITER_ELEM (l, &liter, f, BM_LOOPS_OF_FACE) {
				BM_ITER_ELEM (f2, &fiter, l->e, BM_FACES_OF_EDGE) {
					if (!BM_elem_flag_test(f2, BM_ELEM_HIDDEN)) {
						if (!BMO_elem_flag_test(bm, f2, SEL_ORIG)) {
							BMO_elem_flag_enable(bm, f, SEL_FLAG);
							break;
						}
					}
				}
			}
		}
	}
}

void bmo_region_extend_exec(BMesh *bm, BMOperator *op)
{
	int use_faces = BMO_slot_bool_get(op->slots_in, "use_faces");
	int constrict = BMO_slot_bool_get(op->slots_in, "use_constrict");

	BMO_slot_buffer_flag_enable(bm, op->slots_in, "geom", BM_ALL, SEL_ORIG);

	if (constrict)
		bmo_region_extend_constrict(bm, op, use_faces);
	else
		bmo_region_extend_extend(bm, op, use_faces);

	BMO_slot_buffer_from_enabled_flag(bm, op, op->slots_out, "geom.out", BM_ALL, SEL_FLAG);
}

/********* righthand faces implementation ****** */

#define FACE_VIS	1
#define FACE_FLAG	2
// #define FACE_MARK	4  /* UNUSED */
#define FACE_FLIP	8

/* NOTE: these are the original recalc_face_normals comment in editmesh_mods.c,
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

void bmo_recalc_face_normals_exec(BMesh *bm, BMOperator *op)
{
	BMIter liter, liter2;
	BMOIter siter;
	BMFace *f, *startf, **fstack = NULL;
	BLI_array_declare(fstack);
	BMLoop *l, *l2;
	float maxx, maxx_test, cent[3];
	int i, i_max, flagflip = BMO_slot_bool_get(op->slots_in, "use_flip");

	startf = NULL;
	maxx = -1.0e10;
	
	BMO_slot_buffer_flag_enable(bm, op->slots_in, "faces", BM_FACE, FACE_FLAG);

	/* find a starting face */
	BMO_ITER (f, &siter, op->slots_in, "faces", BM_FACE) {

		/* clear dirty flag */
		BM_elem_flag_disable(f, BM_ELEM_TAG);

		if (BMO_elem_flag_test(bm, f, FACE_VIS))
			continue;

		if (!startf) startf = f;

		BM_face_calc_center_bounds(f, cent);

		if ((maxx_test = dot_v3v3(cent, cent)) > maxx) {
			maxx = maxx_test;
			startf = f;
		}
	}

	if (!startf) return;

	BM_face_calc_center_bounds(startf, cent);

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

	BLI_array_grow_one(fstack);
	fstack[0] = startf;
	BMO_elem_flag_enable(bm, startf, FACE_VIS);

	i = 0;
	i_max = 1;
	while (i >= 0) {
		f = fstack[i];
		i--;

		BM_ITER_ELEM (l, &liter, f, BM_LOOPS_OF_FACE) {
			BM_ITER_ELEM (l2, &liter2, l, BM_LOOPS_OF_LOOP) {
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
					
					if (i == i_max) {
						BLI_array_grow_one(fstack);
						i_max++;
					}

					fstack[i] = l2->f;
				}
			}
		}
	}

	BLI_array_free(fstack);

	/* check if we have faces yet to do.  if so, recurse */
	BMO_ITER (f, &siter, op->slots_in, "faces", BM_FACE) {
		if (!BMO_elem_flag_test(bm, f, FACE_VIS)) {
			bmo_recalc_face_normals_exec(bm, op);
			break;
		}
	}
}

void bmo_smooth_vert_exec(BMesh *UNUSED(bm), BMOperator *op)
{
	BMOIter siter;
	BMIter iter;
	BMVert *v;
	BMEdge *e;
	BLI_array_declare(cos);
	float (*cos)[3] = NULL;
	float *co, *co2, clip_dist = BMO_slot_float_get(op->slots_in, "clip_dist");
	int i, j, clipx, clipy, clipz;
	int xaxis, yaxis, zaxis;
	
	clipx = BMO_slot_bool_get(op->slots_in, "mirror_clip_x");
	clipy = BMO_slot_bool_get(op->slots_in, "mirror_clip_y");
	clipz = BMO_slot_bool_get(op->slots_in, "mirror_clip_z");

	xaxis = BMO_slot_bool_get(op->slots_in, "use_axis_x");
	yaxis = BMO_slot_bool_get(op->slots_in, "use_axis_y");
	zaxis = BMO_slot_bool_get(op->slots_in, "use_axis_z");

	i = 0;
	BMO_ITER (v, &siter, op->slots_in, "verts", BM_VERT) {
		BLI_array_grow_one(cos);
		co = cos[i];
		
		j  = 0;
		BM_ITER_ELEM (e, &iter, v, BM_EDGES_OF_VERT) {
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

		if (clipx && fabsf(v->co[0]) <= clip_dist)
			co[0] = 0.0f;
		if (clipy && fabsf(v->co[1]) <= clip_dist)
			co[1] = 0.0f;
		if (clipz && fabsf(v->co[2]) <= clip_dist)
			co[2] = 0.0f;

		i++;
	}

	i = 0;
	BMO_ITER (v, &siter, op->slots_in, "verts", BM_VERT) {
		if (xaxis)
			v->co[0] = cos[i][0];
		if (yaxis)
			v->co[1] = cos[i][1];
		if (zaxis)
			v->co[2] = cos[i][2];

		i++;
	}

	BLI_array_free(cos);
}

/**************************************************************************** *
 * Cycle UVs for a face
 **************************************************************************** */

void bmo_rotate_uvs_exec(BMesh *bm, BMOperator *op)
{
	BMOIter fs_iter;  /* selected faces iterator */
	BMFace *fs;       /* current face */
	BMIter l_iter;    /* iteration loop */

	const int use_ccw = BMO_slot_bool_get(op->slots_in, "use_ccw");

	BMO_ITER (fs, &fs_iter, op->slots_in, "faces", BM_FACE) {
		if (CustomData_has_layer(&(bm->ldata), CD_MLOOPUV)) {
			if (use_ccw == FALSE) {  /* same loops direction */
				BMLoop *lf;	/* current face loops */
				MLoopUV *f_luv; /* first face loop uv */
				float p_uv[2];	/* previous uvs */
				float t_uv[2];	/* tmp uvs */

				int n = 0;
				BM_ITER_ELEM (lf, &l_iter, fs, BM_LOOPS_OF_FACE) {
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
			else { /* counter loop direction */
				BMLoop *lf;	/* current face loops */
				MLoopUV *p_luv; /* previous loop uv */
				MLoopUV *luv;
				float t_uv[2];	/* current uvs */

				int n = 0;
				BM_ITER_ELEM (lf, &l_iter, fs, BM_LOOPS_OF_FACE) {
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

/**************************************************************************** *
 * Reverse UVs for a face
 **************************************************************************** */

void bmo_reverse_uvs_exec(BMesh *bm, BMOperator *op)
{
	BMOIter fs_iter;	/* selected faces iterator */
	BMFace *fs;		/* current face */
	BMIter l_iter;		/* iteration loop */
	BLI_array_declare(uvs);
	float (*uvs)[2] = NULL;

	BMO_ITER (fs, &fs_iter, op->slots_in, "faces", BM_FACE) {
		if (CustomData_has_layer(&(bm->ldata), CD_MLOOPUV)) {
			BMLoop *lf;	/* current face loops */
			int i;

			BLI_array_empty(uvs);
			BLI_array_grow_items(uvs, fs->len);

			BM_ITER_ELEM_INDEX (lf, &l_iter, fs, BM_LOOPS_OF_FACE, i) {
				MLoopUV *luv = CustomData_bmesh_get(&bm->ldata, lf->head.data, CD_MLOOPUV);

				/* current loop uv is the previous loop uv */
				copy_v2_v2(uvs[i], luv->uv);
			}

			/* now that we have the uvs in the array, reverse! */
			i = 0;
			BM_ITER_ELEM_INDEX (lf, &l_iter, fs, BM_LOOPS_OF_FACE, i) {
				/* current loop uv is the previous loop uv */
				MLoopUV *luv = CustomData_bmesh_get(&bm->ldata, lf->head.data, CD_MLOOPUV);
				copy_v2_v2(luv->uv, uvs[(fs->len - i - 1)]);
			}
		}
	}

	BLI_array_free(uvs);
}

/**************************************************************************** *
 * Cycle colors for a face
 **************************************************************************** */

void bmo_rotate_colors_exec(BMesh *bm, BMOperator *op)
{
	BMOIter fs_iter;  /* selected faces iterator */
	BMFace *fs;       /* current face */
	BMIter l_iter;    /* iteration loop */

	const int use_ccw = BMO_slot_bool_get(op->slots_in, "use_ccw");

	BMO_ITER (fs, &fs_iter, op->slots_in, "faces", BM_FACE) {
		if (CustomData_has_layer(&(bm->ldata), CD_MLOOPCOL)) {
			if (use_ccw == FALSE) {  /* same loops direction */
				BMLoop *lf;	/* current face loops */
				MLoopCol *f_lcol; /* first face loop color */
				MLoopCol p_col;	/* previous color */
				MLoopCol t_col;	/* tmp color */

				int n = 0;
				BM_ITER_ELEM (lf, &l_iter, fs, BM_LOOPS_OF_FACE) {
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
			else {  /* counter loop direction */
				BMLoop *lf;	/* current face loops */
				MLoopCol *p_lcol; /* previous loop color */
				MLoopCol *lcol;
				MLoopCol t_col;	/* current color */

				int n = 0;
				BM_ITER_ELEM (lf, &l_iter, fs, BM_LOOPS_OF_FACE) {
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

/*************************************************************************** *
 * Reverse colors for a face
 *************************************************************************** */

void bmo_reverse_colors_exec(BMesh *bm, BMOperator *op)
{
	BMOIter fs_iter;	/* selected faces iterator */
	BMFace *fs;		/* current face */
	BMIter l_iter;		/* iteration loop */
	BLI_array_declare(cols);
	MLoopCol *cols = NULL;

	BMO_ITER (fs, &fs_iter, op->slots_in, "faces", BM_FACE) {
		if (CustomData_has_layer(&(bm->ldata), CD_MLOOPCOL)) {
			BMLoop *lf;	/* current face loops */
			int i;

			BLI_array_empty(cols);
			BLI_array_grow_items(cols, fs->len);

			BM_ITER_ELEM_INDEX (lf, &l_iter, fs, BM_LOOPS_OF_FACE, i) {
				cols[i] = *((MLoopCol *)CustomData_bmesh_get(&bm->ldata, lf->head.data, CD_MLOOPCOL));
			}

			/* now that we have the uvs in the array, reverse! */
			BM_ITER_ELEM_INDEX (lf, &l_iter, fs, BM_LOOPS_OF_FACE, i) {
				/* current loop uv is the previous loop color */
				MLoopCol *lcol = CustomData_bmesh_get(&bm->ldata, lf->head.data, CD_MLOOPCOL);
				*lcol = cols[(fs->len - i - 1)];
			}
		}
	}

	BLI_array_free(cols);
}


/*************************************************************************** *
 * shortest vertex path select
 *************************************************************************** */

typedef struct ElemNode {
	BMVert *v;	/* vertex */
	BMVert *parent;	/* node parent id */
	float weight;	/* node weight */
	HeapNode *hn;	/* heap node */
} ElemNode;

#define VERT_MARK	1

void bmo_shortest_path_exec(BMesh *bm, BMOperator *op)
{
	BMOIter vs_iter /* , vs2_iter */;	/* selected verts iterator */
	BMIter v_iter;		/* mesh verts iterator */
	BMVert *vs, *sv, *ev;	/* starting vertex, ending vertex */
	BMVert *v;		/* mesh vertex */
	Heap *h = NULL;

	ElemNode *vert_list = NULL;

	int num_total = 0 /*, num_sels = 0 */, i = 0;
	const int type = BMO_slot_int_get(op->slots_in, "type");

	/* BMESH_TODO use BMO_slot_buffer_elem_first here? */
	BMO_ITER (vs, &vs_iter, op->slots_in, "startv", BM_VERT) {
		sv = vs;
	}
	BMO_ITER (vs, &vs_iter, op->slots_in, "endv", BM_VERT) {
		ev = vs;
	}

	num_total = BM_mesh_elem_count(bm, BM_VERT);

	/* allocate memory for the nodes */
	vert_list = (ElemNode *)MEM_mallocN(sizeof(ElemNode) * num_total, "vertex nodes");

	/* iterate through all the mesh vertices */
	/* loop through all the vertices and fill the vertices/indices structure */
	i = 0;
	BM_ITER_MESH (v, &v_iter, bm, BM_VERTS_OF_MESH) {
		vert_list[i].v = v;
		vert_list[i].parent = NULL;
		vert_list[i].weight = FLT_MAX;
		BM_elem_index_set(v, i); /* set_inline */
		i++;
	}
	bm->elem_index_dirty &= ~BM_VERT;

	/*
	 * we now have everything we need, start Dijkstra path finding algorithm
	 */

	/* set the distance/weight of the start vertex to 0 */
	vert_list[BM_elem_index_get(sv)].weight = 0.0f;

	h = BLI_heap_new();

	for (i = 0; i < num_total; i++) {
		vert_list[i].hn = BLI_heap_insert(h, vert_list[i].weight, vert_list[i].v);
	}

	while (!BLI_heap_is_empty(h)) {
		BMEdge *e;
		BMIter e_i;
		float v_weight;

		/* take the vertex with the lowest weight out of the heap */
		BMVert *v = (BMVert *)BLI_heap_popmin(h);

		if (vert_list[BM_elem_index_get(v)].weight == FLT_MAX) /* this means that there is no path */
			break;

		v_weight = vert_list[BM_elem_index_get(v)].weight;

		BM_ITER_ELEM (e, &e_i, v, BM_EDGES_OF_VERT) {
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

	BMO_slot_buffer_from_enabled_flag(bm, op, op->slots_out, "verts.out", BM_VERT, VERT_MARK);
}
