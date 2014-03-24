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

/** \file blender/bmesh/operators/bmesh_wireframe.c
 *  \ingroup bmesh
 *
 * Creates a solid wireframe from connected faces.
 */

#include "MEM_guardedalloc.h"

#include "DNA_object_types.h"
#include "DNA_meshdata_types.h"

#include "BLI_math.h"

#include "bmesh.h"

#include "BKE_deform.h"
#include "BKE_customdata.h"

#include "bmesh_wireframe.h"

static BMLoop *bm_edge_tag_faceloop(BMEdge *e)
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

	if (e_a && e_b) {
		/* note, with an incorrectly flushed selection this can crash */
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

		/* check are we flipped the right way */
		BM_edge_calc_face_tangent(e_a, l_a, tvec_a);
		BM_edge_calc_face_tangent(e_b, l_b, tvec_b);
		add_v3_v3(tvec_a, tvec_b);

		*r_va_other = v_a;
		*r_vb_other = v_b;
	}
	else {
		/* degenerate case - vertex connects a boundary edged face to other faces,
		 * so we have only one boundary face - only use it for calculations */
		l_a = bm_edge_tag_faceloop(e_a);

		copy_v3_v3(no_face, l_a->f->no);

		/* edge direction */
		v_a = BM_edge_other_vert(e_a, v);
		v_b = NULL;

		sub_v3_v3v3(no_edge, v->co, v_a->co);

		/* check are we flipped the right way */
		BM_edge_calc_face_tangent(e_a, l_a, tvec_a);

		*r_va_other = NULL;
		*r_vb_other = NULL;
	}

	/* find the normal */
	cross_v3_v3v3(r_no, no_edge, no_face);
	normalize_v3(r_no);

	if (dot_v3v3(r_no, tvec_a) > 0.0f) {
		negate_v3(r_no);
	}

	copy_v3_v3(r_no_face, no_face);
}

/* check if we are the only tagged loop-face around this edge */
static bool bm_loop_is_radial_boundary(BMLoop *l_first)
{
	BMLoop *l = l_first->radial_next;

	if (l == l_first) {
		return true; /* a real boundary */
	}
	else {
		do {
			if (BM_elem_flag_test(l->f, BM_ELEM_TAG)) {
				return false;
			}
		} while ((l = l->radial_next) != l_first);
	}
	return true;
}

/**
 * \param def_nr  -1 for no vertex groups.
 *
 * \note All edge tags must be cleared.
 * \note Behavior matches MOD_solidify.c
 */
void BM_mesh_wireframe(
        BMesh *bm,
        const float offset,
        const float offset_fac,
        const float offset_fac_vg,
        const bool use_replace,
        const bool use_boundary,
        const bool use_even_offset,
        const bool use_relative_offset,
        const bool use_crease,
        const float crease_weight,
        const int defgrp_index,
        const bool defgrp_invert,
        const short mat_offset,
        const short mat_max,
        /* for operators */
        const bool use_tag
        )
{
	const float ofs_orig = -(((-offset_fac + 1.0f) * 0.5f) * offset);
	const float ofs_new  = offset + ofs_orig;
	const float ofs_mid  = (ofs_orig + ofs_new) / 2.0f;
	const float inset = offset / 2.0f;
	int cd_edge_crease_offset = use_crease ? CustomData_get_offset(&bm->edata, CD_CREASE) : -1;
	const int cd_dvert_offset = (defgrp_index != -1) ? CustomData_get_offset(&bm->vdata, CD_MDEFORMVERT) : -1;
	const float offset_fac_vg_inv = 1.0f - offset_fac_vg;

	const int totvert_orig = bm->totvert;

	BMIter iter;
	BMIter itersub;

	/* filled only with boundary verts */
	BMVert **verts_src      = MEM_mallocN(sizeof(BMVert *) * totvert_orig, __func__);
	BMVert **verts_neg      = MEM_mallocN(sizeof(BMVert *) * totvert_orig, __func__);
	BMVert **verts_pos      = MEM_mallocN(sizeof(BMVert *) * totvert_orig, __func__);

	/* will over-alloc, but makes for easy lookups by index to keep aligned  */
	BMVert **verts_boundary = use_boundary ?
	                          MEM_mallocN(sizeof(BMVert *) * totvert_orig, __func__) : NULL;

	float  *verts_relfac    = (use_relative_offset || (cd_dvert_offset != -1)) ?
	                          MEM_mallocN(sizeof(float) * totvert_orig, __func__) : NULL;

	/* may over-alloc if not all faces have wire */
	BMVert **verts_loop;
	int verts_loop_tot = 0;

	BMVert *v_src;

	BMFace *f_src;
	BMLoop *l;

	float tvec[3];
	float fac, fac_shell;

	int i;

	if (use_crease && cd_edge_crease_offset == -1) {
		BM_data_layer_add(bm, &bm->edata, CD_CREASE);
		cd_edge_crease_offset = CustomData_get_offset(&bm->edata, CD_CREASE);
	}

	BM_ITER_MESH_INDEX (v_src, &iter, bm, BM_VERTS_OF_MESH, i) {
		BM_elem_index_set(v_src, i); /* set_inline */

		verts_src[i] = v_src;
		BM_elem_flag_disable(v_src, BM_ELEM_TAG);
	}
	bm->elem_index_dirty &= ~BM_VERT;

	/* setup tags, all faces and verts will be tagged which will be duplicated */

	BM_ITER_MESH_INDEX (f_src, &iter, bm, BM_FACES_OF_MESH, i) {
		BM_elem_index_set(f_src, i); /* set_inline */

		if (use_tag) {
			if (!BM_elem_flag_test(f_src, BM_ELEM_TAG)) {
				continue;
			}
		}
		else {
			BM_elem_flag_enable(f_src, BM_ELEM_TAG);
		}


		verts_loop_tot += f_src->len;
		BM_ITER_ELEM (l, &itersub, f_src, BM_LOOPS_OF_FACE) {
			BM_elem_flag_enable(l->v, BM_ELEM_TAG);

			/* also tag boundary edges */
			BM_elem_flag_set(l->e, BM_ELEM_TAG, bm_loop_is_radial_boundary(l));
		}
	}
	bm->elem_index_dirty &= ~BM_FACE;

	/* duplicate tagged verts */
	for (i = 0; i < totvert_orig; i++) {
		v_src = verts_src[i];
		if (BM_elem_flag_test(v_src, BM_ELEM_TAG)) {
			fac = 1.0f;

			if (verts_relfac) {
				if (use_relative_offset) {
					verts_relfac[i] = BM_vert_calc_mean_tagged_edge_length(v_src);
				}
				else {
					verts_relfac[i] = 1.0f;
				}


				if (cd_dvert_offset != -1) {
					MDeformVert *dvert = BM_ELEM_CD_GET_VOID_P(v_src, cd_dvert_offset);
					float defgrp_fac = defvert_find_weight(dvert, defgrp_index);

					if (defgrp_invert) {
						defgrp_fac = 1.0f - defgrp_fac;
					}

					if (offset_fac_vg > 0.0f) {
						defgrp_fac = (offset_fac_vg + (defgrp_fac * offset_fac_vg_inv));
					}

					verts_relfac[i] *= defgrp_fac;
				}

				fac *= verts_relfac[i];
			}


			verts_neg[i] = BM_vert_create(bm, NULL, v_src, BM_CREATE_NOP);
			verts_pos[i] = BM_vert_create(bm, NULL, v_src, BM_CREATE_NOP);

			if (offset == 0.0f) {
				madd_v3_v3v3fl(verts_neg[i]->co, v_src->co, v_src->no, ofs_orig * fac);
				madd_v3_v3v3fl(verts_pos[i]->co, v_src->co, v_src->no,  ofs_new * fac);
			}
			else {
				madd_v3_v3v3fl(tvec, v_src->co, v_src->no, ofs_mid * fac);

				madd_v3_v3v3fl(verts_neg[i]->co, tvec, v_src->no, (ofs_orig - ofs_mid) * fac);
				madd_v3_v3v3fl(verts_pos[i]->co, tvec, v_src->no,  (ofs_new - ofs_mid) * fac);
			}
		}
		else {
			/* could skip this */
			verts_neg[i] = NULL;
			verts_pos[i] = NULL;
		}

		/* conflicts with BM_vert_calc_mean_tagged_edge_length */
		if (use_relative_offset == false) {
			BM_elem_flag_disable(v_src, BM_ELEM_TAG);
		}
	}

	if (use_relative_offset) {
		BM_mesh_elem_hflag_disable_all(bm, BM_VERT, BM_ELEM_TAG, false);
	}

	verts_loop = MEM_mallocN(sizeof(BMVert *) * verts_loop_tot, __func__);
	verts_loop_tot = 0; /* count up again */

	BM_ITER_MESH (f_src, &iter, bm, BM_FACES_OF_MESH) {

		if (use_tag && !BM_elem_flag_test(f_src, BM_ELEM_TAG)) {
			continue;
		}

		BM_ITER_ELEM (l, &itersub, f_src, BM_LOOPS_OF_FACE) {
			BM_elem_index_set(l, verts_loop_tot); /* set_loop */

			BM_loop_calc_face_tangent(l, tvec);

			/* create offset vert */
			fac = 1.0f;

			if (verts_relfac) {
				fac *= verts_relfac[BM_elem_index_get(l->v)];
			}

			fac_shell = fac;
			if (use_even_offset) {
				fac_shell *= shell_angle_to_dist(((float)M_PI - BM_loop_calc_face_angle(l)) * 0.5f);
			}


			madd_v3_v3v3fl(tvec, l->v->co, tvec, inset * fac_shell);
			if (offset != 0.0f) {
				madd_v3_v3fl(tvec, l->v->no, ofs_mid * fac);
			}
			verts_loop[verts_loop_tot] = BM_vert_create(bm, tvec, l->v, BM_CREATE_NOP);


			if (use_boundary) {
				if (BM_elem_flag_test(l->e, BM_ELEM_TAG)) {  /* is this a boundary? */
					BMVert *v_pair[2] = {l->v, l->next->v};

					for (i = 0; i < 2; i++) {
						BMVert *v_boundary = v_pair[i];
						if (!BM_elem_flag_test(v_boundary, BM_ELEM_TAG)) {
							const int v_boundary_index = BM_elem_index_get(v_boundary);
							float no_face[3];
							BMVert *va_other;
							BMVert *vb_other;

							BM_elem_flag_enable(v_boundary, BM_ELEM_TAG);

							bm_vert_boundary_tangent(v_boundary, tvec, no_face, &va_other, &vb_other);

							/* create offset vert */
							/* similar to code above but different angle calc */
							fac = 1.0f;

							if (verts_relfac) {
								fac *= verts_relfac[v_boundary_index];
							}

							fac_shell = fac;
							if (use_even_offset) {
								if (va_other) {  /* for verts with only one boundary edge - this will be NULL */
									fac_shell *= shell_angle_to_dist(((float)M_PI -
									                                  angle_on_axis_v3v3v3_v3(va_other->co,
									                                                          v_boundary->co,
									                                                          vb_other->co,
									                                                          no_face)) * 0.5f);
								}
							}


							madd_v3_v3v3fl(tvec, v_boundary->co, tvec, inset * fac_shell);
							if (offset != 0.0f) {
								madd_v3_v3fl(tvec, v_boundary->no, ofs_mid * fac);
							}
							verts_boundary[v_boundary_index] = BM_vert_create(bm, tvec, v_boundary, BM_CREATE_NOP);
						}
					}
				}
			}

			verts_loop_tot++;
		}
	}

	BM_ITER_MESH (f_src, &iter, bm, BM_FACES_OF_MESH) {

		/* skip recently added faces */
		if (BM_elem_index_get(f_src) == -1) {
			continue;
		}

		if (use_tag && !BM_elem_flag_test(f_src, BM_ELEM_TAG)) {
			continue;
		}

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

			f_new = BM_face_create_quad_tri(bm, v_l1, v_l2, v_neg2, v_neg1, f_src, false);
			if (mat_offset) f_new->mat_nr = CLAMPIS(f_new->mat_nr + mat_offset, 0, mat_max);
			BM_elem_flag_enable(f_new, BM_ELEM_TAG);
			l_new = BM_FACE_FIRST_LOOP(f_new);

			BM_elem_attrs_copy(bm, bm, l,      l_new);
			BM_elem_attrs_copy(bm, bm, l,      l_new->prev);
			BM_elem_attrs_copy(bm, bm, l_next, l_new->next);
			BM_elem_attrs_copy(bm, bm, l_next, l_new->next->next);

			f_new = BM_face_create_quad_tri(bm, v_l2, v_l1, v_pos1, v_pos2, f_src, false);

			if (mat_offset) f_new->mat_nr = CLAMPIS(f_new->mat_nr + mat_offset, 0, mat_max);
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

					f_new = BM_face_create_quad_tri(bm, v_b2, v_b1, v_neg1, v_neg2, f_src, false);
					if (mat_offset) f_new->mat_nr = CLAMPIS(f_new->mat_nr + mat_offset, 0, mat_max);
					BM_elem_flag_enable(f_new, BM_ELEM_TAG);
					l_new = BM_FACE_FIRST_LOOP(f_new);

					BM_elem_attrs_copy(bm, bm, l_next, l_new);
					BM_elem_attrs_copy(bm, bm, l_next, l_new->prev);
					BM_elem_attrs_copy(bm, bm, l,      l_new->next);
					BM_elem_attrs_copy(bm, bm, l,      l_new->next->next);

					f_new = BM_face_create_quad_tri(bm, v_b1, v_b2, v_pos2, v_pos1, f_src, false);
					if (mat_offset) f_new->mat_nr = CLAMPIS(f_new->mat_nr + mat_offset, 0, mat_max);
					BM_elem_flag_enable(f_new, BM_ELEM_TAG);
					l_new = BM_FACE_FIRST_LOOP(f_new);

					BM_elem_attrs_copy(bm, bm, l,      l_new);
					BM_elem_attrs_copy(bm, bm, l,      l_new->prev);
					BM_elem_attrs_copy(bm, bm, l_next, l_new->next);
					BM_elem_attrs_copy(bm, bm, l_next, l_new->next->next);

					if (use_crease) {
						BMEdge *e_new;
						e_new = BM_edge_exists(v_pos1, v_b1);
						BM_ELEM_CD_SET_FLOAT(e_new, cd_edge_crease_offset, crease_weight);

						e_new = BM_edge_exists(v_pos2, v_b2);
						BM_ELEM_CD_SET_FLOAT(e_new, cd_edge_crease_offset, crease_weight);

						e_new = BM_edge_exists(v_neg1, v_b1);
						BM_ELEM_CD_SET_FLOAT(e_new, cd_edge_crease_offset, crease_weight);

						e_new = BM_edge_exists(v_neg2, v_b2);
						BM_ELEM_CD_SET_FLOAT(e_new, cd_edge_crease_offset, crease_weight);
					}
				}
			}

			if (use_crease) {
				BMEdge *e_new;
				e_new = BM_edge_exists(v_pos1, v_l1);
				BM_ELEM_CD_SET_FLOAT(e_new, cd_edge_crease_offset, crease_weight);

				e_new = BM_edge_exists(v_pos2, v_l2);
				BM_ELEM_CD_SET_FLOAT(e_new, cd_edge_crease_offset, crease_weight);

				e_new = BM_edge_exists(v_neg1, v_l1);
				BM_ELEM_CD_SET_FLOAT(e_new, cd_edge_crease_offset, crease_weight);

				e_new = BM_edge_exists(v_neg2, v_l2);
				BM_ELEM_CD_SET_FLOAT(e_new, cd_edge_crease_offset, crease_weight);
			}

		}
	}

	if (use_boundary) {
		MEM_freeN(verts_boundary);
	}

	if (verts_relfac) {
		MEM_freeN(verts_relfac);
	}

	if (use_replace) {

		if (use_tag) {
			/* only remove faces which are original and used to make wire,
			 * use 'verts_pos' and 'verts_neg' to avoid a feedback loop. */

			/* vertex must be from 'verts_src' */
#define VERT_DUPE_TEST_ORIG(v)  (verts_neg[BM_elem_index_get(v)] != NULL)
#define VERT_DUPE_TEST(v)       (verts_pos[BM_elem_index_get(v)] != NULL)
#define VERT_DUPE_CLEAR(v)     { verts_pos[BM_elem_index_get(v)]  = NULL; } (void)0

			/* first ensure we keep all verts which are used in faces that weren't
			 * entirely made into wire. */
			BM_ITER_MESH (f_src, &iter, bm, BM_FACES_OF_MESH) {
				int mix_flag = 0;
				BMLoop *l_iter, *l_first;

				/* skip new faces */
				if (BM_elem_index_get(f_src) == -1) {
					continue;
				}

				l_iter = l_first = BM_FACE_FIRST_LOOP(f_src);
				do {
					mix_flag |= (VERT_DUPE_TEST_ORIG(l_iter->v) ? 1 : 2);
					if (mix_flag == (1 | 2)) {
						break;
					}
				} while ((l_iter = l_iter->next) != l_first);

				if (mix_flag == (1 | 2)) {
					l_iter = l_first = BM_FACE_FIRST_LOOP(f_src);
					do {
						VERT_DUPE_CLEAR(l_iter->v);
					} while ((l_iter = l_iter->next) != l_first);
				}
			}

			/* now remove any verts which were made into wire by all faces */
			for (i = 0; i < totvert_orig; i++) {
				v_src = verts_src[i];
				BLI_assert(i == BM_elem_index_get(v_src));
				if (VERT_DUPE_TEST(v_src)) {
					BM_vert_kill(bm, v_src);
				}
			}

#undef VERT_DUPE_TEST_ORIG
#undef VERT_DUPE_TEST
#undef VERT_DUPE_CLEAR

		}
		else {
			/* simple case, no tags - replace all */
			for (i = 0; i < totvert_orig; i++) {
				BM_vert_kill(bm, verts_src[i]);
			}
		}
	}

	MEM_freeN(verts_src);
	MEM_freeN(verts_neg);
	MEM_freeN(verts_pos);
	MEM_freeN(verts_loop);
}
