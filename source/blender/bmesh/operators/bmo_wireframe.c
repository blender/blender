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
 * Contributor(s): Campbell Barton
 *
 * ***** END GPL LICENSE BLOCK *****
 */

/** \file blender/bmesh/operators/bmo_wireframe.c
 *  \ingroup bmesh
 */

#include "MEM_guardedalloc.h"

#include "BLI_math.h"

#include "BKE_customdata.h"

#include "bmesh.h"

#include "intern/bmesh_operators_private.h" /* own include */

BMLoop *bm_edge_tag_faceloop(BMEdge *e)
{
	BMLoop *l, *l_first;

	l = l_first = e->l;
	do {
		if (BM_elem_flag_test(l->f, BM_ELEM_TAG)) {
			return l;
		}
	} while ((l = l->radial_next) != l_first);

	/* in the case this is used, we know this will never happen */
	return NULL;
}

static void bm_vert_boundary_tangent(BMVert *v, float r_no[3], float r_no_face[3],
                                     BMVert **r_va_other, BMVert **r_vb_other)
{
	BMIter iter;
	BMEdge *e_iter;

	BMEdge *e_a = NULL, *e_b = NULL;
	BMVert *v_a, *v_b;

	BMLoop *l_a, *l_b;

	float no_face[3], no_edge[3];
	float tvec_a[3], tvec_b[3];

	/* get 2 boundary edges, there should only _be_ 2,
	 * in case there are more - results wont be valid of course */
	BM_ITER_ELEM (e_iter, &iter, v, BM_EDGES_OF_VERT) {
		if (BM_elem_flag_test(e_iter, BM_ELEM_TAG)) {
			if (e_a == NULL) {
				e_a = e_iter;
			}
			else {
				e_b = e_iter;
				break;
			}
		}
	}

	l_a = bm_edge_tag_faceloop(e_a);
	l_b = bm_edge_tag_faceloop(e_b);

	/* average edge face normal */
	add_v3_v3v3(no_face, l_a->f->no, l_b->f->no);

	/* average edge direction */
	v_a = BM_edge_other_vert(e_a, v);
	v_b = BM_edge_other_vert(e_b, v);

	sub_v3_v3v3(tvec_a, v->co, v_a->co);
	sub_v3_v3v3(tvec_b, v_b->co, v->co);
	normalize_v3(tvec_a);
	normalize_v3(tvec_b);
	add_v3_v3v3(no_edge, tvec_a, tvec_b); /* not unit length but this is ok */


	/* find the normal */
	cross_v3_v3v3(r_no, no_edge, no_face);
	normalize_v3(r_no);

	/* check are we flipped the right way */
	BM_edge_calc_face_tangent(e_a, l_a, tvec_a);
	BM_edge_calc_face_tangent(e_b, l_b, tvec_b);
	add_v3_v3(tvec_a, tvec_b);

	if (dot_v3v3(r_no, tvec_a) > 0.0) {
		negate_v3(r_no);
	}

	copy_v3_v3(r_no_face, no_face);
	*r_va_other = v_a;
	*r_vb_other = v_b;
}

/* check if we are the only tagged loop-face around this edge */
static int bm_loop_is_radial_boundary(BMLoop *l_first)
{
	BMLoop *l = l_first->radial_next;

	if (l == l_first) {
		return TRUE; /* a real boundary */
	}
	else {
		do {
			if (BM_elem_flag_test(l->f, BM_ELEM_TAG)) {
				return FALSE;
			}
		} while ((l = l->radial_next) != l_first);
	}
	return TRUE;
}

extern float BM_vert_calc_mean_tagged_edge_length(BMVert *v);

void bmo_wireframe_exec(BMesh *bm, BMOperator *op)
{
	const int use_boundary        = BMO_slot_bool_get(op,  "use_boundary");
	const int use_even_offset     = BMO_slot_bool_get(op,  "use_even_offset");
	const int use_relative_offset = BMO_slot_bool_get(op,  "use_relative_offset");
	const int use_crease          = (BMO_slot_bool_get(op,  "use_crease") &&
	                                 CustomData_has_layer(&bm->edata, CD_CREASE));
	const float depth             = BMO_slot_float_get(op, "thickness");
	const float inset             = depth;

	const int totvert_orig = bm->totvert;

	BMOIter oiter;
	BMIter iter;
	BMIter itersub;

	/* filled only with boundary verts */
	BMVert **verts_src      = MEM_mallocN(sizeof(BMVert **) * totvert_orig, __func__);
	BMVert **verts_neg      = MEM_mallocN(sizeof(BMVert **) * totvert_orig, __func__);
	BMVert **verts_pos      = MEM_mallocN(sizeof(BMVert **) * totvert_orig, __func__);

	/* will over-alloc, but makes for easy lookups by index to keep aligned  */
	BMVert **verts_boundary = use_boundary ?
	                          MEM_mallocN(sizeof(BMVert **) * totvert_orig, __func__) : NULL;

	float  *verts_relfac    = use_relative_offset ?
	                          MEM_mallocN(sizeof(float) * totvert_orig, __func__) : NULL;

	/* may over-alloc if not all faces have wire */
	BMVert **verts_loop;
	int verts_loop_tot = 0;

	BMVert *v_src;

	BMFace *f_src;
	BMLoop *l;

	float tvec[3];
	float fac;

	int i;

	BM_mesh_elem_index_ensure(bm, BM_VERT);

	BM_ITER_MESH_INDEX (v_src, &iter, bm, BM_VERTS_OF_MESH, i) {
		BM_elem_flag_disable(v_src, BM_ELEM_TAG);
		verts_src[i] = v_src;
	}

	/* setup tags, all faces and verts will be tagged which will be duplicated */
	BM_mesh_elem_hflag_disable_all(bm, BM_FACE, BM_ELEM_TAG, FALSE);

	BMO_ITER (f_src, &oiter, bm, op, "faces", BM_FACE) {
		verts_loop_tot += f_src->len;
		BM_elem_flag_enable(f_src, BM_ELEM_TAG);
		BM_ITER_ELEM (l, &itersub, f_src, BM_LOOPS_OF_FACE) {
			BM_elem_flag_enable(l->v, BM_ELEM_TAG);

			/* also tag boundary edges */
			BM_elem_flag_set(l->e, BM_ELEM_TAG, bm_loop_is_radial_boundary(l));
		}
	}

	/* duplicate tagged verts */
	for (i = 0, v_src = verts_src[i]; i < totvert_orig; i++, v_src = verts_src[i]) {
		if (BM_elem_flag_test(v_src, BM_ELEM_TAG)) {
			fac = depth;

			if (use_relative_offset) {
				verts_relfac[i] = BM_vert_calc_mean_tagged_edge_length(v_src);
				fac *= verts_relfac[i];
			}

			madd_v3_v3v3fl(tvec, v_src->co, v_src->no, -fac);
			verts_neg[i] = BM_vert_create(bm, tvec, v_src);
			madd_v3_v3v3fl(tvec, v_src->co, v_src->no,  fac);
			verts_pos[i] = BM_vert_create(bm, tvec, v_src);
		}
		else {
			/* could skip this */
			verts_src[i] = NULL;
			verts_neg[i] = NULL;
			verts_pos[i] = NULL;
		}

		/* conflicts with BM_vert_calc_mean_tagged_edge_length */
		if (use_relative_offset == FALSE) {
			BM_elem_flag_disable(v_src, BM_ELEM_TAG);
		}
	}

	if (use_relative_offset) {
		BM_mesh_elem_hflag_disable_all(bm, BM_VERT, BM_ELEM_TAG, FALSE);
	}

	verts_loop = MEM_mallocN(sizeof(BMVert **) * verts_loop_tot, __func__);
	verts_loop_tot = 0; /* count up again */

	BMO_ITER (f_src, &oiter, bm, op, "faces", BM_FACE) {
		BM_ITER_ELEM (l, &itersub, f_src, BM_LOOPS_OF_FACE) {
			BM_elem_index_set(l, verts_loop_tot); /* set_loop */

			BM_loop_calc_face_tangent(l, tvec);

			/* create offset vert */
			fac = inset;
			if (use_even_offset) {
				fac *= shell_angle_to_dist((M_PI - BM_loop_calc_face_angle(l)) * 0.5f);
			}
			if (use_relative_offset) {
				fac *= verts_relfac[BM_elem_index_get(l->v)];
			}

			madd_v3_v3v3fl(tvec, l->v->co, tvec, fac);
			verts_loop[verts_loop_tot] = BM_vert_create(bm, tvec, l->v);


			if (use_boundary) {
				if (BM_elem_flag_test(l->e, BM_ELEM_TAG)) {  /* is this a boundary? */

					BMLoop *l_pair[2] = {l, l->next};

					BM_elem_flag_enable(l->e, BM_ELEM_TAG);
					for (i = 0; i < 2; i++) {
						if (!BM_elem_flag_test(l_pair[i]->v, BM_ELEM_TAG)) {
							float no_face[3];
							BMVert *va_other;
							BMVert *vb_other;

							BM_elem_flag_enable(l_pair[i]->v, BM_ELEM_TAG);

							bm_vert_boundary_tangent(l_pair[i]->v, tvec, no_face, &va_other, &vb_other);

							/* create offset vert */
							/* similar to code above but different angle calc */
							fac = inset;
							if (use_even_offset) {
								fac *= shell_angle_to_dist((M_PI - angle_on_axis_v3v3v3_v3(va_other->co,
								                                                           l_pair[i]->v->co,
								                                                           vb_other->co,
								                                                           no_face)) * 0.5f);
							}
							if (use_relative_offset) {
								fac *= verts_relfac[BM_elem_index_get(l_pair[i]->v)];
							}
							madd_v3_v3v3fl(tvec, l_pair[i]->v->co, tvec, fac);
							verts_boundary[BM_elem_index_get(l_pair[i]->v)] = BM_vert_create(bm, tvec, l_pair[i]->v);
						}
					}
				}
			}

			verts_loop_tot++;
		}
	}

	BMO_ITER (f_src, &oiter, bm, op, "faces", BM_FACE) {
		BM_elem_flag_disable(f_src, BM_ELEM_TAG);
		BM_ITER_ELEM (l, &itersub, f_src, BM_LOOPS_OF_FACE) {
			BMFace *f_new;
			BMLoop *l_new;
			BMLoop *l_next = l->next;
			BMVert *v_l1 = verts_loop[BM_elem_index_get(l)];
			BMVert *v_l2 = verts_loop[BM_elem_index_get(l_next)];

			BMVert *v_src_l1 = l->v;
			BMVert *v_src_l2 = l_next->v;

			const int i_1 = BM_elem_index_get(v_src_l1);
			const int i_2 = BM_elem_index_get(v_src_l2);

			BMVert *v_neg1 = verts_neg[i_1];
			BMVert *v_neg2 = verts_neg[i_2];

			BMVert *v_pos1 = verts_pos[i_1];
			BMVert *v_pos2 = verts_pos[i_2];

			f_new = BM_face_create_quad_tri(bm, v_l1, v_l2, v_neg2, v_neg1, f_src, FALSE);
			BM_elem_flag_enable(f_new, BM_ELEM_TAG);
			l_new = BM_FACE_FIRST_LOOP(f_new);

			BM_elem_attrs_copy(bm, bm, l,      l_new);
			BM_elem_attrs_copy(bm, bm, l,      l_new->prev);
			BM_elem_attrs_copy(bm, bm, l_next, l_new->next);
			BM_elem_attrs_copy(bm, bm, l_next, l_new->next->next);

			f_new = BM_face_create_quad_tri(bm, v_l2, v_l1, v_pos1, v_pos2, f_src, FALSE);
			BM_elem_flag_enable(f_new, BM_ELEM_TAG);
			l_new = BM_FACE_FIRST_LOOP(f_new);

			BM_elem_attrs_copy(bm, bm, l_next, l_new);
			BM_elem_attrs_copy(bm, bm, l_next, l_new->prev);
			BM_elem_attrs_copy(bm, bm, l,      l_new->next);
			BM_elem_attrs_copy(bm, bm, l,      l_new->next->next);

			if (use_boundary) {
				if (BM_elem_flag_test(l->e, BM_ELEM_TAG)) {
					/* we know its a boundary and this is the only face user (which is being wire'd) */
					/* we know we only touch this edge/face once */
					BMVert *v_b1 = verts_boundary[i_1];
					BMVert *v_b2 = verts_boundary[i_2];

					f_new = BM_face_create_quad_tri(bm, v_b2, v_b1, v_neg1, v_neg2, f_src, FALSE);
					BM_elem_flag_enable(f_new, BM_ELEM_TAG);
					l_new = BM_FACE_FIRST_LOOP(f_new);

					BM_elem_attrs_copy(bm, bm, l_next, l_new);
					BM_elem_attrs_copy(bm, bm, l_next, l_new->prev);
					BM_elem_attrs_copy(bm, bm, l,      l_new->next);
					BM_elem_attrs_copy(bm, bm, l,      l_new->next->next);

					f_new = BM_face_create_quad_tri(bm, v_b1, v_b2, v_pos2, v_pos1, f_src, FALSE);
					BM_elem_flag_enable(f_new, BM_ELEM_TAG);
					l_new = BM_FACE_FIRST_LOOP(f_new);

					BM_elem_attrs_copy(bm, bm, l,      l_new);
					BM_elem_attrs_copy(bm, bm, l,      l_new->prev);
					BM_elem_attrs_copy(bm, bm, l_next, l_new->next);
					BM_elem_attrs_copy(bm, bm, l_next, l_new->next->next);

					if (use_crease) {
						BMEdge *e_new;
						e_new = BM_edge_exists(v_pos1, v_b1);
						BM_elem_float_data_set(&bm->edata, e_new, CD_CREASE, 1.0f);

						e_new = BM_edge_exists(v_pos2, v_b2);
						BM_elem_float_data_set(&bm->edata, e_new, CD_CREASE, 1.0f);

						e_new = BM_edge_exists(v_neg1, v_b1);
						BM_elem_float_data_set(&bm->edata, e_new, CD_CREASE, 1.0f);

						e_new = BM_edge_exists(v_neg2, v_b2);
						BM_elem_float_data_set(&bm->edata, e_new, CD_CREASE, 1.0f);
					}
				}
			}

			if (use_crease) {
				BMEdge *e_new;
				e_new = BM_edge_exists(v_pos1, v_l1);
				BM_elem_float_data_set(&bm->edata, e_new, CD_CREASE, 1.0f);

				e_new = BM_edge_exists(v_pos2, v_l2);
				BM_elem_float_data_set(&bm->edata, e_new, CD_CREASE, 1.0f);

				e_new = BM_edge_exists(v_neg1, v_l1);
				BM_elem_float_data_set(&bm->edata, e_new, CD_CREASE, 1.0f);

				e_new = BM_edge_exists(v_neg2, v_l2);
				BM_elem_float_data_set(&bm->edata, e_new, CD_CREASE, 1.0f);
			}

		}
	}

	if (use_boundary) {
		MEM_freeN(verts_boundary);
	}

	if (use_relative_offset) {
		MEM_freeN(verts_relfac);
	}

	MEM_freeN(verts_src);
	MEM_freeN(verts_neg);
	MEM_freeN(verts_pos);
	MEM_freeN(verts_loop);

	BMO_slot_buffer_from_enabled_hflag(bm, op, "faceout", BM_FACE, BM_ELEM_TAG);
}
