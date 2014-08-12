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

/** \file blender/bmesh/tools/bmesh_bisect_plane.c
 *  \ingroup bmesh
 *
 * Cut the geometry in half using a plane.
 *
 * \par Implementation
 * This simply works by splitting tagged edges whos verts span either side of
 * the plane, then splitting faces along their dividing verts.
 * The only complex case is when a ngon spans the axis multiple times,
 * in this case we need to do some extra checks to correctly bisect the ngon.
 * see: #bm_face_bisect_verts
 */

#include <limits.h>

#include "MEM_guardedalloc.h"

#include "BLI_utildefines.h"
#include "BLI_stackdefines.h"
#include "BLI_alloca.h"
#include "BLI_linklist.h"
#include "BLI_linklist_stack.h"
#include "BLI_math.h"

#include "bmesh.h"
#include "bmesh_bisect_plane.h"  /* own include */

#include "BLI_strict_flags.h"  /* keep last */


/* -------------------------------------------------------------------- */
/* Math utils */

static int plane_point_test_v3(const float plane[4], const float co[3], const float eps, float *r_depth)
{
	const float f = plane_point_side_v3(plane, co);
	*r_depth = f;

	if      (f <= -eps) return -1;
	else if (f >=  eps) return  1;
	else                return  0;
}


/* -------------------------------------------------------------------- */
/* Wrappers to hide internal data-structure abuse,
 * later we may want to move this into some hash lookup
 * to a separate struct, but for now we can store in BMesh data */

#define BM_VERT_DIR(v)     ((v)->head.index)    /* Direction -1/0/1 */
#define BM_VERT_DIST(v)    ((v)->no[0])         /* Distance from the plane. */
#define BM_VERT_SORTVAL(v) ((v)->no[1])         /* Temp value for sorting. */
#define BM_VERT_LOOPINDEX(v)                    /* The verts index within a face (temp var) */ \
	(*((unsigned int *)(&(v)->no[2])))

/**
 * Hide flag access
 * (for more readable code since same flag is used differently for vert/edgeface)...
 */

/* enable when vertex is in the center and its faces have been added to the stack */
BLI_INLINE void vert_is_center_enable(BMVert *v) { BM_elem_flag_enable(v, BM_ELEM_TAG); }
BLI_INLINE void vert_is_center_disable(BMVert *v) { BM_elem_flag_disable(v, BM_ELEM_TAG); }
BLI_INLINE bool vert_is_center_test(BMVert *v) { return (BM_elem_flag_test(v, BM_ELEM_TAG) != 0); }

/* enable when the edge can be cut */
BLI_INLINE void edge_is_cut_enable(BMEdge *e) { BM_elem_flag_enable(e, BM_ELEM_TAG); }
BLI_INLINE void edge_is_cut_disable(BMEdge *e) { BM_elem_flag_disable(e, BM_ELEM_TAG); }
BLI_INLINE bool edge_is_cut_test(BMEdge *e) { return (BM_elem_flag_test(e, BM_ELEM_TAG) != 0); }

/* enable when the faces are added to the stack */
BLI_INLINE void face_in_stack_enable(BMFace *f) { BM_elem_flag_disable(f, BM_ELEM_TAG); }
BLI_INLINE void face_in_stack_disable(BMFace *f) { BM_elem_flag_enable(f, BM_ELEM_TAG); }
BLI_INLINE bool face_in_stack_test(BMFace *f) { return (BM_elem_flag_test(f, BM_ELEM_TAG) == 0); }

/* -------------------------------------------------------------------- */
/* BMesh utils */

static int bm_vert_sortval_cb(const void *v_a_v, const void *v_b_v)
{
	const float val_a = BM_VERT_SORTVAL(*((BMVert **)v_a_v));
	const float val_b = BM_VERT_SORTVAL(*((BMVert **)v_b_v));

	if      (val_a > val_b) return  1;
	else if (val_a < val_b) return -1;
	                        return  0;
}


static void bm_face_bisect_verts(BMesh *bm, BMFace *f, const float plane[4], const short oflag_center)
{
	/* unlikely more then 2 verts are needed */
	const unsigned int f_len_orig = (unsigned int)f->len;
	BMVert **vert_split_arr = BLI_array_alloca(vert_split_arr, f_len_orig);
	STACK_DECLARE(vert_split_arr);
	BMLoop *l_iter, *l_first;
	bool use_dirs[3] = {false, false, false};

	STACK_INIT(vert_split_arr, f_len_orig);

	l_first = BM_FACE_FIRST_LOOP(f);

	/* add plane-aligned verts to the stack
	 * and check we have verts from both sides in this face,
	 * ... that the face doesn't only have boundary verts on the plane for eg. */
	l_iter = l_first;
	do {
		if (vert_is_center_test(l_iter->v)) {
			BLI_assert(BM_VERT_DIR(l_iter->v) == 0);
			STACK_PUSH(vert_split_arr, l_iter->v);
		}
		use_dirs[BM_VERT_DIR(l_iter->v) + 1] = true;
	} while ((l_iter = l_iter->next) != l_first);

	if ((STACK_SIZE(vert_split_arr) > 1) &&
	    (use_dirs[0] && use_dirs[2]))
	{
		if (LIKELY(STACK_SIZE(vert_split_arr) == 2)) {
			BMLoop *l_new;
			BMLoop *l_a, *l_b;

			l_a = BM_face_vert_share_loop(f, vert_split_arr[0]);
			l_b = BM_face_vert_share_loop(f, vert_split_arr[1]);

			/* common case, just cut the face once */
			BM_face_split(bm, f, l_a, l_b, &l_new, NULL, true);
			if (l_new) {
				if (oflag_center) {
					BMO_elem_flag_enable(bm, l_new->e, oflag_center);
					BMO_elem_flag_enable(bm, l_new->f, oflag_center);
					BMO_elem_flag_enable(bm, f,        oflag_center);
				}
			}
		}
		else {
			/* less common case, _complicated_ we need to calculate how to do multiple cuts */
			float (*face_verts_proj_2d)[2] = BLI_array_alloca(face_verts_proj_2d, f_len_orig);
			float axis_mat[3][3];

			BMFace **face_split_arr = BLI_array_alloca(face_split_arr, STACK_SIZE(vert_split_arr));
			STACK_DECLARE(face_split_arr);

			float sort_dir[3];
			unsigned int i;


			/* ---- */
			/* Calculate the direction to sort verts in the face intersecting the plane */

			/* exact dir isn't so important,
			 * just need a dir for sorting verts across face,
			 * 'sort_dir' could be flipped either way, it not important, we only need to order the array
			 */
			cross_v3_v3v3(sort_dir, f->no, plane);
			if (UNLIKELY(normalize_v3(sort_dir) == 0.0f)) {
				/* find any 2 verts and get their direction */
				for (i = 0; i < STACK_SIZE(vert_split_arr); i++) {
					if (!equals_v3v3(vert_split_arr[0]->co, vert_split_arr[i]->co)) {
						sub_v3_v3v3(sort_dir, vert_split_arr[0]->co, vert_split_arr[i]->co);
						normalize_v3(sort_dir);
					}
				}
				if (UNLIKELY(i == STACK_SIZE(vert_split_arr))) {
					/* ok, we can't do anything useful here,
					 * face has no area or so, bail out, this is highly unlikely but not impossible */
					goto finally;
				}
			}


			/* ---- */
			/* Calculate 2d coords to use for intersection checks */

			/* get the faces 2d coords */
			BLI_assert(BM_face_is_normal_valid(f));
			axis_dominant_v3_to_m3(axis_mat, f->no);

			l_iter = l_first;
			i = 0;
			do {
				BM_VERT_LOOPINDEX(l_iter->v) = i;
				mul_v2_m3v3(face_verts_proj_2d[i], axis_mat, l_iter->v->co);
				i++;
			} while ((l_iter = l_iter->next) != l_first);


			/* ---- */
			/* Sort the verts across the face from one side to another */
			for (i = 0; i < STACK_SIZE(vert_split_arr); i++) {
				BMVert *v = vert_split_arr[i];
				BM_VERT_SORTVAL(v) = dot_v3v3(sort_dir, v->co);
			}

			qsort(vert_split_arr, STACK_SIZE(vert_split_arr), sizeof(*vert_split_arr), bm_vert_sortval_cb);


			/* ---- */
			/* Split the face across sorted splits */

			/* note: we don't know which face gets which splits,
			 * so at the moment we have to search all faces for the vert pair,
			 * while not all that nice, typically there are < 5 resulting faces,
			 * so its not _that_ bad. */

			STACK_INIT(face_split_arr, STACK_SIZE(vert_split_arr));
			STACK_PUSH(face_split_arr, f);

			for (i = 0; i < STACK_SIZE(vert_split_arr) - 1; i++) {
				BMVert *v_a = vert_split_arr[i];
				BMVert *v_b = vert_split_arr[i + 1];
				float co_mid[2];

				/* geometric test before doing face lookups,
				 * find if the split spans a filled region of the polygon. */
				mid_v2_v2v2(co_mid,
				            face_verts_proj_2d[BM_VERT_LOOPINDEX(v_a)],
				            face_verts_proj_2d[BM_VERT_LOOPINDEX(v_b)]);

				if (isect_point_poly_v2(co_mid, (const float (*)[2])face_verts_proj_2d, f_len_orig, false)) {
					BMLoop *l_a, *l_b;
					bool found = false;
					unsigned int j;

					for (j = 0; j < STACK_SIZE(face_split_arr); j++) {
						/* would be nice to avoid loop lookup here,
						 * but we need to know which face the verts are in */
						if ((l_a = BM_face_vert_share_loop(face_split_arr[j], v_a)) &&
						    (l_b = BM_face_vert_share_loop(face_split_arr[j], v_b)))
						{
							found = true;
							break;
						}
					}

					BLI_assert(found == true);

					/* in fact this simple test is good enough,
					 * test if the loops are adjacent */
					if (found && !BM_loop_is_adjacent(l_a, l_b)) {
						BMFace *f_tmp;
						f_tmp = BM_face_split(bm, face_split_arr[j], l_a, l_b, NULL, NULL, true);
						if (f_tmp) {
							if (f_tmp != face_split_arr[j]) {
								STACK_PUSH(face_split_arr, f_tmp);
								BLI_assert(STACK_SIZE(face_split_arr) <= STACK_SIZE(vert_split_arr));
							}
						}
					}
				}
				else {
					// printf("no intersect\n");
				}
			}
		}
	}


finally:
	(void)vert_split_arr;
}

/* -------------------------------------------------------------------- */
/* Main logic */

/**
 * \param use_snap_center  Snap verts onto the plane.
 * \param use_tag  Only bisect tagged edges and faces.
 * \param oflag_center  Operator flag, enabled for geometry on the axis (existing and created)
 */
void BM_mesh_bisect_plane(BMesh *bm, float plane[4],
                          const bool use_snap_center, const bool use_tag,
                          const short oflag_center, const float eps)
{
	unsigned int einput_len;
	unsigned int i;
	BMEdge **edges_arr = MEM_mallocN(sizeof(*edges_arr) * (size_t)bm->totedge, __func__);

	BLI_LINKSTACK_DECLARE(face_stack, BMFace *);

	BMVert *v;
	BMFace *f;

	BMIter iter;

	if (use_tag) {
		/* build tagged edge array */
		BMEdge *e;
		einput_len = 0;

		/* flush edge tags to verts */
		BM_mesh_elem_hflag_disable_all(bm, BM_VERT, BM_ELEM_TAG, false);

		/* keep face tags as is */
		BM_ITER_MESH_INDEX (e, &iter, bm, BM_EDGES_OF_MESH, i) {
			if (edge_is_cut_test(e)) {
				edges_arr[einput_len++] = e;

				/* flush edge tags to verts */
				BM_elem_flag_enable(e->v1, BM_ELEM_TAG);
				BM_elem_flag_enable(e->v2, BM_ELEM_TAG);
			}
		}

		/* face tags are set by caller */
	}
	else {
		BMEdge *e;
		einput_len = (unsigned int)bm->totedge;
		BM_ITER_MESH_INDEX (e, &iter, bm, BM_EDGES_OF_MESH, i) {
			edge_is_cut_enable(e);
			edges_arr[i] = e;
		}

		BM_ITER_MESH (f, &iter, bm, BM_FACES_OF_MESH) {
			face_in_stack_disable(f);
		}
	}


	BM_ITER_MESH (v, &iter, bm, BM_VERTS_OF_MESH) {

		if (use_tag && !BM_elem_flag_test(v, BM_ELEM_TAG)) {
			vert_is_center_disable(v);

			/* these should never be accessed */
			BM_VERT_DIR(v) = 0;
			BM_VERT_DIST(v) = 0.0f;

			continue;
		}

		vert_is_center_disable(v);
		BM_VERT_DIR(v) = plane_point_test_v3(plane, v->co, eps, &(BM_VERT_DIST(v)));

		if (BM_VERT_DIR(v) == 0) {
			if (oflag_center) {
				BMO_elem_flag_enable(bm, v, oflag_center);
			}
			if (use_snap_center) {
				closest_to_plane_v3(v->co, plane, v->co);
			}
		}
	}

	/* store a stack of faces to be evaluated for splitting */
	BLI_LINKSTACK_INIT(face_stack);

	for (i = 0; i < einput_len; i++) {
		/* we could check edge_is_cut_test(e) but there is no point */
		BMEdge *e = edges_arr[i];
		const int side[2] = {BM_VERT_DIR(e->v1), BM_VERT_DIR(e->v2)};
		const float dist[2] = {BM_VERT_DIST(e->v1), BM_VERT_DIST(e->v2)};

		if (side[0] && side[1] && (side[0] != side[1])) {
			const float e_fac = fabsf(dist[0]) / fabsf(dist[0] - dist[1]);
			BMVert *v_new;

			if (e->l) {
				BMLoop *l_iter, *l_first;
				l_iter = l_first = e->l;
				do {
					if (!face_in_stack_test(l_iter->f)) {
						face_in_stack_enable(l_iter->f);
						BLI_LINKSTACK_PUSH(face_stack, l_iter->f);
					}
				} while ((l_iter = l_iter->radial_next) != l_first);
			}

			v_new = BM_edge_split(bm, e, e->v1, NULL, e_fac);
			vert_is_center_enable(v_new);
			if (oflag_center) {
				BMO_elem_flag_enable(bm, v_new, oflag_center);
			}

			BM_VERT_DIR(v_new) = 0;
			BM_VERT_DIST(v_new) = 0.0f;
		}
		else if (side[0] == 0 || side[1] == 0) {
			/* check if either edge verts are aligned,
			 * if so - tag and push all faces that use it into the stack */
			unsigned int j;
			BM_ITER_ELEM_INDEX (v, &iter, e, BM_VERTS_OF_EDGE, j) {
				if (side[j] == 0) {
					if (vert_is_center_test(v) == 0) {
						BMIter itersub;
						BMLoop *l_iter;

						vert_is_center_enable(v);

						BM_ITER_ELEM (l_iter, &itersub, v, BM_LOOPS_OF_VERT) {
							if (!face_in_stack_test(l_iter->f)) {
								face_in_stack_enable(l_iter->f);
								BLI_LINKSTACK_PUSH(face_stack, l_iter->f);
							}
						}

					}
				}
			}

			/* if both verts are on the center - tag it */
			if (oflag_center) {
				if (side[0] == 0 && side[1] == 0) {
					BMO_elem_flag_enable(bm, e, oflag_center);
				}
			}
		}
	}

	MEM_freeN(edges_arr);

	while ((f = BLI_LINKSTACK_POP(face_stack))) {
		bm_face_bisect_verts(bm, f, plane, oflag_center);
	}

	/* now we have all faces to split in the stack */
	BLI_LINKSTACK_FREE(face_stack);
}
