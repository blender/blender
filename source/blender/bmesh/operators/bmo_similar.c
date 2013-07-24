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
 * Contributor(s): Joseph Eagar, Campbell Barton
 *
 * ***** END GPL LICENSE BLOCK *****
 */

/** \file blender/bmesh/operators/bmo_similar.c
 *  \ingroup bmesh
 *
 * bmesh operators to select based on
 * comparisons with the existing selection.
 */

#include "MEM_guardedalloc.h"

#include "DNA_object_types.h"
#include "DNA_meshdata_types.h"

#include "BLI_math.h"

#include "BKE_customdata.h"
#include "BKE_deform.h"

#include "bmesh.h"

#include "intern/bmesh_operators_private.h"  /* own include */

/* in fact these could all be the same */

/*
 * extra face data (computed data)
 */
typedef struct SimSel_FaceExt {
	BMFace  *f;             /* the face */
	float    c[3];          /* center */
	union {
		float   area;       /* area */
		float   perim;      /* perimeter */
		float   d;          /* 4th component of plane (the first three being the normal) */
		struct Image *t;    /* image pointer */
	};
} SimSel_FaceExt;

static int bm_sel_similar_cmp_fl(const float delta, const float thresh, const int compare)
{
	switch (compare) {
		case SIM_CMP_EQ:
			return (fabsf(delta) <= thresh);
		case SIM_CMP_GT:
			return ((delta + thresh) >= 0.0f);
		case SIM_CMP_LT:
			return ((delta - thresh) <= 0.0f);
		default:
			BLI_assert(0);
			return 0;
	}
}

static int bm_sel_similar_cmp_i(const int delta, const int compare)
{
	switch (compare) {
		case SIM_CMP_EQ:
			return (delta == 0);
		case SIM_CMP_GT:
			return (delta > 0);
		case SIM_CMP_LT:
			return (delta < 0);
		default:
			BLI_assert(0);
			return 0;
	}
}

/*
 * Select similar faces, the choices are in the enum in source/blender/bmesh/bmesh_operators.h
 * We select either similar faces based on material, image, area, perimeter, normal, or the coplanar faces
 */
void bmo_similar_faces_exec(BMesh *bm, BMOperator *op)
{
#define FACE_MARK	1

	BMIter fm_iter;
	BMFace *fs, *fm;
	BMOIter fs_iter;
	int num_sels = 0, num_total = 0, i = 0, idx = 0;
	float angle = 0.0f;
	SimSel_FaceExt *f_ext = NULL;
	int *indices = NULL;
	float t_no[3];	/* temporary normal */
	const int type = BMO_slot_int_get(op->slots_in, "type");
	const float thresh = BMO_slot_float_get(op->slots_in, "thresh");
	const float thresh_radians = thresh * (float)M_PI;
	const int compare = BMO_slot_int_get(op->slots_in, "compare");

	/* initial_elem - other_elem */
	float delta_fl;
	int   delta_i;

	num_total = BM_mesh_elem_count(bm, BM_FACE);

	/*
	 * The first thing to do is to iterate through all the the selected items and mark them since
	 * they will be in the selection anyway.
	 * This will increase performance, (especially when the number of originally selected faces is high)
	 * so the overall complexity will be less than $O(mn)$ where is the total number of selected faces,
	 * and n is the total number of faces
	 */
	BMO_ITER (fs, &fs_iter, op->slots_in, "faces", BM_FACE) {
		if (!BMO_elem_flag_test(bm, fs, FACE_MARK)) {	/* is this really needed ? */
			BMO_elem_flag_enable(bm, fs, FACE_MARK);
			num_sels++;
		}
	}

	/* allocate memory for the selected faces indices and for all temporary faces */
	indices = (int *)MEM_callocN(sizeof(int) * num_sels, "face indices util.c");
	f_ext = (SimSel_FaceExt *)MEM_callocN(sizeof(SimSel_FaceExt) * num_total, "f_ext util.c");

	/* loop through all the faces and fill the faces/indices structure */
	BM_ITER_MESH (fm, &fm_iter, bm, BM_FACES_OF_MESH) {
		f_ext[i].f = fm;
		if (BMO_elem_flag_test(bm, fm, FACE_MARK)) {
			indices[idx] = i;
			idx++;
		}
		i++;
	}

	/*
	 * Save us some computation burden: In case of perimeter/area/coplanar selection we compute
	 * only once.
	 */
	if (type == SIMFACE_PERIMETER || type == SIMFACE_AREA || type == SIMFACE_COPLANAR || type == SIMFACE_IMAGE) {
		for (i = 0; i < num_total; i++) {
			switch (type) {
				case SIMFACE_PERIMETER:
					/* set the perimeter */
					f_ext[i].perim = BM_face_calc_perimeter(f_ext[i].f);
					break;

				case SIMFACE_COPLANAR:
					/* compute the center of the polygon */
					BM_face_calc_center_mean(f_ext[i].f, f_ext[i].c);

					/* normalize the polygon normal */
					copy_v3_v3(t_no, f_ext[i].f->no);
					normalize_v3(t_no);

					/* compute the plane distance */
					f_ext[i].d = dot_v3v3(t_no, f_ext[i].c);
					break;

				case SIMFACE_AREA:
					f_ext[i].area = BM_face_calc_area(f_ext[i].f);
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
		if (!BMO_elem_flag_test(bm, fm, FACE_MARK) && !BM_elem_flag_test(fm, BM_ELEM_HIDDEN)) {
			bool cont = true;
			for (idx = 0; idx < num_sels && cont == true; idx++) {
				fs = f_ext[indices[idx]].f;
				switch (type) {
					case SIMFACE_MATERIAL:
						if (fm->mat_nr == fs->mat_nr) {
							BMO_elem_flag_enable(bm, fm, FACE_MARK);
							cont = false;
						}
						break;

					case SIMFACE_IMAGE:
						if (f_ext[i].t == f_ext[indices[idx]].t) {
							BMO_elem_flag_enable(bm, fm, FACE_MARK);
							cont = false;
						}
						break;

					case SIMFACE_NORMAL:
						angle = angle_normalized_v3v3(fs->no, fm->no);	/* if the angle between the normals -> 0 */
						if (angle <= thresh_radians) {
							BMO_elem_flag_enable(bm, fm, FACE_MARK);
							cont = false;
						}
						break;

					case SIMFACE_COPLANAR:
						angle = angle_normalized_v3v3(fs->no, fm->no); /* angle -> 0 */
						if (angle <= thresh_radians) { /* and dot product difference -> 0 */
							delta_fl = f_ext[i].d - f_ext[indices[idx]].d;
							if (bm_sel_similar_cmp_fl(delta_fl, thresh, compare)) {
								BMO_elem_flag_enable(bm, fm, FACE_MARK);
								cont = false;
							}
						}
						break;

					case SIMFACE_AREA:
						delta_fl = f_ext[i].area - f_ext[indices[idx]].area;
						if (bm_sel_similar_cmp_fl(delta_fl, thresh, compare)) {
							BMO_elem_flag_enable(bm, fm, FACE_MARK);
							cont = false;
						}
						break;

					case SIMFACE_SIDES:
						delta_i = fm->len - fs->len;
						if (bm_sel_similar_cmp_i(delta_i, compare)) {
							BMO_elem_flag_enable(bm, fm, FACE_MARK);
							cont = false;
						}
						break;

					case SIMFACE_PERIMETER:
						delta_fl = f_ext[i].perim - f_ext[indices[idx]].perim;
						if (bm_sel_similar_cmp_fl(delta_fl, thresh, compare)) {
							BMO_elem_flag_enable(bm, fm, FACE_MARK);
							cont = false;
						}
						break;
#ifdef WITH_FREESTYLE
					case SIMFACE_FREESTYLE:
						if (CustomData_has_layer(&bm->pdata, CD_FREESTYLE_FACE)) {
							FreestyleEdge *ffa1, *ffa2;

							ffa1 = CustomData_bmesh_get(&bm->pdata, fs->head.data, CD_FREESTYLE_FACE);
							ffa2 = CustomData_bmesh_get(&bm->pdata, fm->head.data, CD_FREESTYLE_FACE);

							if (ffa1 && ffa2 && (ffa1->flag & FREESTYLE_FACE_MARK) == (ffa2->flag & FREESTYLE_FACE_MARK)) {
								BMO_elem_flag_enable(bm, fm, FACE_MARK);
								cont = false;
							}
						}
						break;
#endif
					default:
						BLI_assert(0);
						break;
				}
			}
		}
	}

	MEM_freeN(f_ext);
	MEM_freeN(indices);

	/* transfer all marked faces to the output slot */
	BMO_slot_buffer_from_enabled_flag(bm, op, op->slots_out, "faces.out", BM_FACE, FACE_MARK);
#undef FACE_MARK
}

/**************************************************************************** *
 * Similar Edges
 **************************************************************************** */

/*
 * extra edge information
 */
typedef struct SimSel_EdgeExt {
	BMEdge *e;
	union {
		float dir[3];
		float angle;            /* angle between the face */
	};

	union {
		float length;           /* edge length */
		int   faces;            /* faces count */
	};
} SimSel_EdgeExt;

/*
 * select similar edges: the choices are in the enum in source/blender/bmesh/bmesh_operators.h
 * choices are length, direction, face, ...
 */
void bmo_similar_edges_exec(BMesh *bm, BMOperator *op)
{
#define EDGE_MARK	1

	BMOIter es_iter;	/* selected edges iterator */
	BMIter e_iter;		/* mesh edges iterator */
	BMEdge *es;		/* selected edge */
	BMEdge *e;		/* mesh edge */
	int idx = 0, i = 0 /* , f = 0 */;
	int *indices = NULL;
	SimSel_EdgeExt *e_ext = NULL;
	// float *angles = NULL;
	float angle;

	int num_sels = 0, num_total = 0;
	const int type = BMO_slot_int_get(op->slots_in, "type");
	const float thresh = BMO_slot_float_get(op->slots_in, "thresh");
	const int compare = BMO_slot_int_get(op->slots_in, "compare");

	/* initial_elem - other_elem */
	float delta_fl;
	int   delta_i;

	/* sanity checks that the data we need is available */
	switch (type) {
		case SIMEDGE_CREASE:
			if (!CustomData_has_layer(&bm->edata, CD_CREASE)) {
				return;
			}
			break;
		case SIMEDGE_BEVEL:
			if (!CustomData_has_layer(&bm->edata, CD_BWEIGHT)) {
				return;
			}
			break;
	}

	num_total = BM_mesh_elem_count(bm, BM_EDGE);

	/* iterate through all selected edges and mark them */
	BMO_ITER (es, &es_iter, op->slots_in, "edges", BM_EDGE) {
		BMO_elem_flag_enable(bm, es, EDGE_MARK);
		num_sels++;
	}

	/* allocate memory for the selected edges indices and for all temporary edges */
	indices = (int *)MEM_callocN(sizeof(int) * num_sels, __func__);
	e_ext = (SimSel_EdgeExt *)MEM_callocN(sizeof(SimSel_EdgeExt) * num_total, __func__);

	/* loop through all the edges and fill the edges/indices structure */
	BM_ITER_MESH (e, &e_iter, bm, BM_EDGES_OF_MESH) {
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
					e_ext[i].length = len_v3v3(e_ext[i].e->v1->co, e_ext[i].e->v2->co);
					break;

				case SIMEDGE_DIR:		/* compute the direction */
					sub_v3_v3v3(e_ext[i].dir, e_ext[i].e->v1->co, e_ext[i].e->v2->co);
					normalize_v3(e_ext[i].dir);
					break;

				case SIMEDGE_FACE:		/* count the faces around the edge */
					e_ext[i].faces = BM_edge_face_count(e_ext[i].e);
					break;

				case SIMEDGE_FACE_ANGLE:
					e_ext[i].faces = BM_edge_face_count(e_ext[i].e);
					if (e_ext[i].faces == 2)
						e_ext[i].angle = BM_edge_calc_face_angle(e_ext[i].e);
					break;
			}
		}
	}

	/* select the edges if any */
	for (i = 0; i < num_total; i++) {
		e = e_ext[i].e;
		if (!BMO_elem_flag_test(bm, e, EDGE_MARK) && !BM_elem_flag_test(e, BM_ELEM_HIDDEN)) {
			bool cont = true;
			for (idx = 0; idx < num_sels && cont == true; idx++) {
				es = e_ext[indices[idx]].e;
				switch (type) {
					case SIMEDGE_LENGTH:
						delta_fl = e_ext[i].length - e_ext[indices[idx]].length;
						if (bm_sel_similar_cmp_fl(delta_fl, thresh, compare)) {
							BMO_elem_flag_enable(bm, e, EDGE_MARK);
							cont = false;
						}
						break;

					case SIMEDGE_DIR:
						/* compute the angle between the two edges */
						angle = angle_normalized_v3v3(e_ext[i].dir, e_ext[indices[idx]].dir);

						if (angle > (float)(M_PI / 2.0)) /* use the smallest angle between the edges */
							angle = fabsf(angle - (float)M_PI);

						if (angle / (float)(M_PI / 2.0) <= thresh) {
							BMO_elem_flag_enable(bm, e, EDGE_MARK);
							cont = false;
						}
						break;

					case SIMEDGE_FACE:
						delta_i = e_ext[i].faces - e_ext[indices[idx]].faces;
						if (bm_sel_similar_cmp_i(delta_i, compare)) {
							BMO_elem_flag_enable(bm, e, EDGE_MARK);
							cont = false;
						}
						break;

					case SIMEDGE_FACE_ANGLE:
						if (e_ext[i].faces == 2) {
							if (e_ext[indices[idx]].faces == 2) {
								if (fabsf(e_ext[i].angle - e_ext[indices[idx]].angle) <= thresh) {
									BMO_elem_flag_enable(bm, e, EDGE_MARK);
									cont = false;
								}
							}
						}
						else {
							cont = false;
						}
						break;

					case SIMEDGE_CREASE:
						{
							float *c1, *c2;

							c1 = CustomData_bmesh_get(&bm->edata, e->head.data, CD_CREASE);
							c2 = CustomData_bmesh_get(&bm->edata, es->head.data, CD_CREASE);
							delta_fl = *c1 - *c2;

							if (bm_sel_similar_cmp_fl(delta_fl, thresh, compare)) {
								BMO_elem_flag_enable(bm, e, EDGE_MARK);
								cont = false;
							}
						}
						break;

					case SIMEDGE_BEVEL:
						{
							float *c1, *c2;

							c1 = CustomData_bmesh_get(&bm->edata, e->head.data, CD_BWEIGHT);
							c2 = CustomData_bmesh_get(&bm->edata, es->head.data, CD_BWEIGHT);
							delta_fl = *c1 - *c2;

							if (bm_sel_similar_cmp_fl(delta_fl, thresh, compare)) {
								BMO_elem_flag_enable(bm, e, EDGE_MARK);
								cont = false;
							}
						}
						break;

					case SIMEDGE_SEAM:
						if (BM_elem_flag_test(e, BM_ELEM_SEAM) == BM_elem_flag_test(es, BM_ELEM_SEAM)) {
							BMO_elem_flag_enable(bm, e, EDGE_MARK);
							cont = false;
						}
						break;

					case SIMEDGE_SHARP:
						if (BM_elem_flag_test(e, BM_ELEM_SMOOTH) == BM_elem_flag_test(es, BM_ELEM_SMOOTH)) {
							BMO_elem_flag_enable(bm, e, EDGE_MARK);
							cont = false;
						}
						break;
#ifdef WITH_FREESTYLE
					case SIMEDGE_FREESTYLE:
						if (CustomData_has_layer(&bm->edata, CD_FREESTYLE_EDGE)) {
							FreestyleEdge *fed1, *fed2;

							fed1 = CustomData_bmesh_get(&bm->edata, e->head.data, CD_FREESTYLE_EDGE);
							fed2 = CustomData_bmesh_get(&bm->edata, es->head.data, CD_FREESTYLE_EDGE);

							if (fed1 && fed2 && (fed1->flag & FREESTYLE_EDGE_MARK) == (fed2->flag & FREESTYLE_EDGE_MARK)) {
								BMO_elem_flag_enable(bm, e, EDGE_MARK);
								cont = false;
							}
						}
						break;
#endif
					default:
						BLI_assert(0);
						break;
				}
			}
		}
	}

	MEM_freeN(e_ext);
	MEM_freeN(indices);

	/* transfer all marked edges to the output slot */
	BMO_slot_buffer_from_enabled_flag(bm, op, op->slots_out, "edges.out", BM_EDGE, EDGE_MARK);

#undef EDGE_MARK
}

/**************************************************************************** *
 * Similar Vertices
 **************************************************************************** */

typedef struct SimSel_VertExt {
	BMVert *v;
	union {
		int num_faces; /* adjacent faces */
		int num_edges; /* adjacent edges */
		MDeformVert *dvert; /* deform vertex */
	};
} SimSel_VertExt;

/*
 * select similar vertices: the choices are in the enum in source/blender/bmesh/bmesh_operators.h
 * choices are normal, face, vertex group...
 */
void bmo_similar_verts_exec(BMesh *bm, BMOperator *op)
{
#define VERT_MARK	1

	const int cd_dvert_offset = CustomData_get_offset(&bm->vdata, CD_MDEFORMVERT);
	BMOIter vs_iter;	/* selected verts iterator */
	BMIter v_iter;		/* mesh verts iterator */
	BMVert *vs;		/* selected vertex */
	BMVert *v;			/* mesh vertex */
	SimSel_VertExt *v_ext = NULL;
	int *indices = NULL;
	int num_total = 0, num_sels = 0, i = 0, idx = 0;
	const int type = BMO_slot_int_get(op->slots_in, "type");
	const float thresh = BMO_slot_float_get(op->slots_in, "thresh");
	const float thresh_radians = thresh * (float)M_PI;
	const int compare = BMO_slot_int_get(op->slots_in, "compare");

	/* initial_elem - other_elem */
//	float delta_fl;
	int   delta_i;

	num_total = BM_mesh_elem_count(bm, BM_VERT);

	/* iterate through all selected edges and mark them */
	BMO_ITER (vs, &vs_iter, op->slots_in, "verts", BM_VERT) {
		BMO_elem_flag_enable(bm, vs, VERT_MARK);
		num_sels++;
	}

	/* allocate memory for the selected vertices indices and for all temporary vertices */
	indices = (int *)MEM_mallocN(sizeof(int) * num_sels, "vertex indices");
	v_ext = (SimSel_VertExt *)MEM_mallocN(sizeof(SimSel_VertExt) * num_total, "vertex extra");

	/* loop through all the vertices and fill the vertices/indices structure */
	BM_ITER_MESH (v, &v_iter, bm, BM_VERTS_OF_MESH) {
		v_ext[i].v = v;
		if (BMO_elem_flag_test(bm, v, VERT_MARK)) {
			indices[idx] = i;
			idx++;
		}

		switch (type) {
			case SIMVERT_FACE:
				/* calling BM_vert_face_count every time is time consumming, so call it only once per vertex */
				v_ext[i].num_faces = BM_vert_face_count(v);
				break;

			case SIMVERT_VGROUP:
				v_ext[i].dvert = (cd_dvert_offset != -1) ? BM_ELEM_CD_GET_VOID_P(v_ext[i].v, cd_dvert_offset) : NULL;
				break;

			case SIMVERT_EDGE:
				v_ext[i].num_edges = BM_vert_edge_count(v);
				break;
		}

		i++;
	}

	/* select the vertices if any */
	for (i = 0; i < num_total; i++) {
		v = v_ext[i].v;
		if (!BMO_elem_flag_test(bm, v, VERT_MARK) && !BM_elem_flag_test(v, BM_ELEM_HIDDEN)) {
			bool cont = true;
			for (idx = 0; idx < num_sels && cont == true; idx++) {
				vs = v_ext[indices[idx]].v;
				switch (type) {
					case SIMVERT_NORMAL:
						/* compare the angle between the normals */
						if (angle_normalized_v3v3(v->no, vs->no) <= thresh_radians) {
							BMO_elem_flag_enable(bm, v, VERT_MARK);
							cont = false;
						}
						break;
					case SIMVERT_FACE:
						/* number of adjacent faces */
						delta_i = v_ext[i].num_faces - v_ext[indices[idx]].num_faces;
						if (bm_sel_similar_cmp_i(delta_i, compare)) {
							BMO_elem_flag_enable(bm, v, VERT_MARK);
							cont = false;
						}
						break;

					case SIMVERT_VGROUP:
						if (v_ext[i].dvert != NULL && v_ext[indices[idx]].dvert != NULL) {
							if (defvert_find_shared(v_ext[i].dvert, v_ext[indices[idx]].dvert) != -1) {
								BMO_elem_flag_enable(bm, v, VERT_MARK);
								cont = false;
							}
						}
						break;
					case SIMVERT_EDGE:
						/* number of adjacent edges */
						delta_i = v_ext[i].num_edges - v_ext[indices[idx]].num_edges;
						if (bm_sel_similar_cmp_i(delta_i, compare)) {
							BMO_elem_flag_enable(bm, v, VERT_MARK);
							cont = false;
						}
						break;
					default:
						BLI_assert(0);
						break;
				}
			}
		}
	}

	MEM_freeN(indices);
	MEM_freeN(v_ext);

	BMO_slot_buffer_from_enabled_flag(bm, op, op->slots_out, "verts.out", BM_VERT, VERT_MARK);

#undef VERT_MARK
}
