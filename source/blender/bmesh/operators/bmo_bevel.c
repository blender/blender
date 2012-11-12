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
 * Contributor(s): Joseph Eagar, Aleksandr Mokhov, Howard Trickey
 *
 * ***** END GPL LICENSE BLOCK *****
 */

/** \file blender/bmesh/operators/bmo_bevel.c
 *  \ingroup bmesh
 */

#include "MEM_guardedalloc.h"

#include "BLI_listbase.h"
#include "BLI_array.h"
#include "BLI_math.h"
#include "BLI_smallhash.h"

#include "BKE_customdata.h"

#include "bmesh.h"

#include "intern/bmesh_operators_private.h" /* own include */

#define NEW_BEVEL 1

#ifdef NEW_BEVEL
#define BEVEL_FLAG      1
#define EDGE_SELECTED   2

#define BEVEL_EPSILON  1e-6

/* Constructed vertex, sometimes later instantiated as BMVert */
typedef struct NewVert {
	float co[3];
	BMVert *v;
} NewVert;

struct BoundVert;

/* Data for one end of an edge involved in a bevel */
typedef struct EdgeHalf {
	struct EdgeHalf *next, *prev;   /* in CCW order */
	BMEdge *e;                  /* original mesh edge */
	int isbev;                  /* is this edge beveled? */
	int isrev;                  /* is e->v2 the vertex at this end? */
	int seg;                    /* how many segments for the bevel */
	float offset;               /* offset for this edge */
	BMFace *fprev;              /* face between this edge and previous, if any */
	BMFace *fnext;              /* face between this edge and next, if any */
	struct BoundVert *leftv;    /* left boundary vert (looking along edge to end) */
	struct BoundVert *rightv;   /* right boundary vert, if beveled */
} EdgeHalf;

/* An element in a cyclic boundary of a Vertex Mesh (VMesh) */
typedef struct BoundVert {
	struct BoundVert *next, *prev;  /* in CCW order */
	int index;          /* used for vmesh indexing */
	NewVert nv;
	EdgeHalf *efirst;   /* first of edges attached here: in CCW order */
	EdgeHalf *elast;
	EdgeHalf *ebev;     /* beveled edge whose left side is attached here, if any */
} BoundVert;

/* Mesh structure replacing a vertex */
typedef struct VMesh {
	enum {
		M_NONE,         /* no polygon mesh needed */
		M_POLY,         /* a simple polygon */
		M_ADJ,          /* "adjacent edges" mesh pattern */
		M_CROSS,        /* "cross edges" mesh pattern */
	} mesh_kind;
	int count;          /* number of vertices in the boundary */
	int seg;            /* common # of segments for segmented edges */
	BoundVert *boundstart;      /* start of boundary double-linked list */
	NewVert *mesh;          /* allocated array - size and structure depends on kind */
} VMesh;

/* Data for a vertex involved in a bevel */
typedef struct BevVert {
	struct BevVert *next, *prev;
	BMVert *v;          /* original mesh vertex */
	int edgecount;          /* total number of edges around the vertex */
	int selcount;           /* number of selected edges around the vertex */
	EdgeHalf *edges;        /* array of size edgecount; CCW order from vertex normal side */
	VMesh *vmesh;           /* mesh structure for replacing vertex */
} BevVert;

/*
 * Bevel parameters and state
 */
typedef struct BevelParams {
	ListBase vertList;      /* list of BevVert for each vertex involved in bevel */
	float offset;           /* blender units to offset each side of a beveled edge */
	int seg;                /* number of segments in beveled edge profile */

	BMOperator *op;
} BevelParams;

/* Make a new BoundVert of the given kind, insert it at the end of the circular linked
 * list with entry point bv->boundstart, and return it. */
static BoundVert *add_new_bound_vert(VMesh *vm, float co[3])
{
	BoundVert *ans = (BoundVert *) MEM_callocN(sizeof(BoundVert), "BoundVert");
	copy_v3_v3(ans->nv.co, co);
	if (!vm->boundstart) {
		ans->index = 0;
		vm->boundstart = ans;
		ans->next = ans->prev = ans;
	}
	else {
		BoundVert *tail = vm->boundstart->prev;
		ans->index = tail->index + 1;
		ans->prev = tail;
		ans->next = vm->boundstart;
		tail->next = ans;
		vm->boundstart->prev = ans;
	}
	vm->count++;
	return ans;
}

/* Mesh verts are indexed (i, j, k) where
 * i = boundvert index (0 <= i < nv)
 * j = ring index (0 <= j <= ns2)
 * k = segment index (0 <= k <= ns)
 * Not all of these are used, and some will share BMVerts */
static NewVert *mesh_vert(VMesh *vm, int i, int j, int k)
{
	int nj = (vm->seg / 2) + 1;
	int nk = vm->seg + 1;

	return &vm->mesh[i * nk * nj  + j * nk + k];
}

static void create_mesh_bmvert(BMesh *bm, VMesh *vm, int i, int j, int k, BMVert *eg)
{
	NewVert *nv = mesh_vert(vm, i, j, k);
	nv->v = BM_vert_create(bm, nv->co, eg);
}

static void copy_mesh_vert(VMesh *vm, int ito, int jto, int kto,
                           int ifrom, int jfrom, int kfrom)
{
	NewVert *nvto, *nvfrom;

	nvto = mesh_vert(vm, ito, jto, kto);
	nvfrom = mesh_vert(vm, ifrom, jfrom, kfrom);
	nvto->v = nvfrom->v;
	copy_v3_v3(nvto->co, nvfrom->co);
}

/* find the EdgeHalf in bv's array that has edge bme */
static EdgeHalf *find_edge_half(BevVert *bv, BMEdge *bme)
{
	int i;

	for (i = 0; i < bv->edgecount; i++) {
		if (bv->edges[i].e == bme)
			return &bv->edges[i];
	}
	return NULL;
}

/* Return the next EdgeHalf after from_e that is beveled.
 * If from_e is NULL, find the first beveled edge. */
static EdgeHalf *next_bev(BevVert *bv, EdgeHalf *from_e)
{
	EdgeHalf *e;

	if (from_e == NULL)
		from_e = &bv->edges[bv->edgecount - 1];
	e = from_e;
	do {
		if (e->isbev)
			return e;
		e = e->next;
	} while (e != from_e);
	return NULL;
}

/* find the BevVert corresponding to BMVert bmv */
static BevVert *find_bevvert(BevelParams *bp, BMVert *bmv)
{
	BevVert *bv;

	for (bv = bp->vertList.first; bv; bv = bv->next) {
		if (bv->v == bmv)
			return bv;
	}
	return NULL;
}

/* Return a good respresentative face (for materials, etc.) for faces
 * created around/near BoundVert v */
static BMFace *boundvert_rep_face(BoundVert *v)
{
	BMFace *fans = NULL;
	BMFace *firstf = NULL;
	BMEdge *e1, *e2;
	BMFace *f1, *f2;
	BMIter iter1, iter2;

	BLI_assert(v->efirst != NULL && v->elast != NULL);
	e1 = v->efirst->e;
	e2 = v->elast->e;
	BM_ITER_ELEM (f1, &iter1, e1, BM_FACES_OF_EDGE) {
		if (!firstf)
			firstf = f1;
		BM_ITER_ELEM (f2, &iter2, e2, BM_FACES_OF_EDGE) {
			if (f1 == f2) {
				fans = f1;
				break;
			}
		}
	}
	if (!fans)
		fans = firstf;

	return fans;
}

/* Make ngon from verts alone.
 * Make sure to properly copy face attributes and do custom data interpolation from
 * example face, facerep. */
static BMFace *bev_create_ngon(BMesh *bm, BMVert **vert_arr, int totv, BMFace *facerep)
{
	BMIter iter;
	BMLoop *l;
	BMFace *f;

	if (totv == 3) {
		f = BM_face_create_quad_tri(bm,
		                            vert_arr[0], vert_arr[1], vert_arr[2], NULL, facerep, 0);
	}
	else if (totv == 4) {
		f = BM_face_create_quad_tri(bm,
		                            vert_arr[0], vert_arr[1], vert_arr[2], vert_arr[3], facerep, 0);
	}
	else {
		int i;
		BMEdge *e;
		BMEdge **ee = NULL;
		BLI_array_staticdeclare(ee, 30);

		for (i = 0; i < totv; i++) {
			e = BM_edge_create(bm, vert_arr[i], vert_arr[(i + 1) % totv], NULL, TRUE);
			BLI_array_append(ee, e);
		}
		f = BM_face_create_ngon(bm, vert_arr[0], vert_arr[1], ee, totv, FALSE);
		BLI_array_free(ee);
	}
	if (facerep && f) {
		int has_mdisps = CustomData_has_layer(&bm->ldata, CD_MDISPS);
		BM_elem_attrs_copy(bm, bm, facerep, f);
		BM_ITER_ELEM (l, &iter, f, BM_LOOPS_OF_FACE) {
			BM_loop_interp_from_face(bm, l, facerep, TRUE, TRUE);
			if (has_mdisps)
				BM_loop_interp_multires(bm, l, facerep);
		}
	}
	return f;
}

static BMFace *bev_create_quad_tri(BMesh *bm, BMVert *v1, BMVert *v2, BMVert *v3, BMVert *v4,
                                   BMFace *facerep)
{
	BMVert *varr[4];

	varr[0] = v1;
	varr[1] = v2;
	varr[2] = v3;
	varr[3] = v4;
	return bev_create_ngon(bm, varr, v4 ? 4 : 3, facerep);
}

/*
 * Calculate the meeting point between the offset edges for e1 and e2, putting answer in meetco.
 * e1 and e2 share vertex v and face f (may be NULL) and viewed from the normal side of
 * the bevel vertex,  e1 precedes e2 in CCW order.
 * If on_right is true, offset edge is on right of both edges, where e1 enters v and
 * e2 leave it. If on_right is false, then the offset edge is on the left.
 * When offsets are equal, the new point is on the edge bisector, with length offset/sin(angle/2),
 * but if the offsets are not equal (allowing for this, as bevel modifier has edge weights that may
 * lead to different offsets) then meeting point can be found be intersecting offset lines.
 */
static void offset_meet(EdgeHalf *e1, EdgeHalf *e2, BMVert *v, BMFace *f,
                        int on_right, float meetco[3])
{
	float dir1[3], dir2[3], norm_v[3], norm_perp1[3], norm_perp2[3],
	      off1a[3], off1b[3], off2a[3], off2b[3], isect2[3];

	/* get direction vectors for two offset lines */
	sub_v3_v3v3(dir1, v->co, BM_edge_other_vert(e1->e, v)->co);
	sub_v3_v3v3(dir2, BM_edge_other_vert(e2->e, v)->co, v->co);

	/* get normal to plane where meet point should be */
	cross_v3_v3v3(norm_v, dir2, dir1);
	normalize_v3(norm_v);
	if (!on_right)
		negate_v3(norm_v);
	if (is_zero_v3(norm_v)) {
		/* special case: e1 and e2 are parallel; put offset point perp to both, from v.
		 * need to find a suitable plane.
		 * if offsets are different, we're out of luck: just use e1->offset */
		if (f)
			copy_v3_v3(norm_v, f->no);
		else
			copy_v3_v3(norm_v, v->no);
		cross_v3_v3v3(norm_perp1, dir1, norm_v);
		normalize_v3(norm_perp1);
		copy_v3_v3(off1a, v->co);
		madd_v3_v3fl(off1a, norm_perp1, e1->offset);
		copy_v3_v3(meetco, off1a);
	}
	else {
		/* get vectors perp to each edge, perp to norm_v, and pointing into face */
		if (f) {
			copy_v3_v3(norm_v, f->no);
			normalize_v3(norm_v);
		}
		cross_v3_v3v3(norm_perp1, dir1, norm_v);
		cross_v3_v3v3(norm_perp2, dir2, norm_v);
		normalize_v3(norm_perp1);
		normalize_v3(norm_perp2);

		/* get points that are offset distances from each line, then another point on each line */
		copy_v3_v3(off1a, v->co);
		madd_v3_v3fl(off1a, norm_perp1, e1->offset);
		add_v3_v3v3(off1b, off1a, dir1);
		copy_v3_v3(off2a, v->co);
		madd_v3_v3fl(off2a, norm_perp2, e2->offset);
		add_v3_v3v3(off2b, off2a, dir2);

		/* intersect the lines; by construction they should be on the same plane and not parallel */
		if (!isect_line_line_v3(off1a, off1b, off2a, off2b, meetco, isect2)) {
			BLI_assert(!"offset_meet failure");
			copy_v3_v3(meetco, off1a);  /* just to do something */
		}
	}
}

/* Like offset_meet, but here f1 and f2 must not be NULL and give the
 * planes in which to run the offset lines.  They may not meet exactly,
 * but the line intersection routine will find the closest approach point. */
static void offset_in_two_planes(EdgeHalf *e1, EdgeHalf *e2, BMVert *v,
                                 BMFace *f1, BMFace *f2, float meetco[3])
{
	float dir1[3], dir2[3], norm_perp1[3], norm_perp2[3],
	      off1a[3], off1b[3], off2a[3], off2b[3], isect2[3];

	BLI_assert(f1 != NULL && f2 != NULL);

	/* get direction vectors for two offset lines */
	sub_v3_v3v3(dir1, v->co, BM_edge_other_vert(e1->e, v)->co);
	sub_v3_v3v3(dir2, BM_edge_other_vert(e2->e, v)->co, v->co);

	/* get directions into offset planes */
	cross_v3_v3v3(norm_perp1, dir1, f1->no);
	normalize_v3(norm_perp1);
	cross_v3_v3v3(norm_perp2, dir2, f2->no);
	normalize_v3(norm_perp2);

	/* get points that are offset distances from each line, then another point on each line */
	copy_v3_v3(off1a, v->co);
	madd_v3_v3fl(off1a, norm_perp1, e1->offset);
	add_v3_v3v3(off1b, off1a, dir1);
	copy_v3_v3(off2a, v->co);
	madd_v3_v3fl(off2a, norm_perp2, e2->offset);
	add_v3_v3v3(off2b, off2a, dir2);

	if (!isect_line_line_v3(off1a, off1b, off2a, off2b, meetco, isect2)) {
		/* lines are parallel; off1a is a good meet point */
		copy_v3_v3(meetco, off1a);
	}
}

/* Offset by e->offset in plane with normal plane_no, on left if left==TRUE,
 * else on right.  If no is NULL, choose an arbitrary plane different
 * from eh's direction. */
static void offset_in_plane(EdgeHalf *e, float plane_no[3], int left, float r[3])
{
	float dir[3], no[3];
	BMVert *v;

	v = e->isrev ? e->e->v1 : e->e->v2;

	sub_v3_v3v3(dir, BM_edge_other_vert(e->e, v)->co, v->co);
	normalize_v3(dir);
	if (plane_no) {
		copy_v3_v3(no, plane_no);
	}
	else {
		zero_v3(no);
		if (fabs(dir[0]) < fabs(dir[1]))
			no[0] = 1.0f;
		else
			no[1] = 1.0f;
	}
	if (left)
		cross_v3_v3v3(r, no, dir);
	else
		cross_v3_v3v3(r, dir, no);
	normalize_v3(r);
	mul_v3_fl(r, e->offset);
}

/* Calculate coordinates of a point a distance d from v on e->e and return it in slideco */
static void slide_dist(EdgeHalf *e, BMVert *v, float d, float slideco[3])
{
	float dir[3], len;

	sub_v3_v3v3(dir, v->co, BM_edge_other_vert(e->e, v)->co);
	len = len_v3(dir);
	normalize_v3(dir);
	if (d > len)
		d = len - (float)(50 * BEVEL_EPSILON);
	copy_v3_v3(slideco, v->co);
	madd_v3_v3fl(slideco, dir, -d);
}

/* Calculate the point on e where line (co_a, co_b) comes closest to and return it in projco */
static void project_to_edge(BMEdge *e, float co_a[3], float co_b[3], float projco[3])
{
	float otherco[3];

	if (!isect_line_line_v3(e->v1->co, e->v2->co, co_a, co_b,
	                        projco, otherco)) {
		BLI_assert(!"project meet failure");
		copy_v3_v3(projco, e->v1->co);
	}
}


/* return 1 if a and b are in CCW order on the normal side of f,
 * and -1 if they are reversed, and 0 if there is no shared face f */
static int bev_ccw_test(BMEdge *a, BMEdge *b, BMFace *f)
{
	BMLoop *la, *lb;

	if (!f)
		return 0;
	la = BM_face_edge_share_loop(f, a);
	lb = BM_face_edge_share_loop(f, b);
	if (!la || !lb)
		return 0;
	return lb->next == la ? 1 : -1;
}

/*
 * calculation of points on the round profile
 * r - result, coordinate of point on round profile
 * method:
 * Inscribe a circle in angle va - v -vb
 * such that it touches the arms at offset from v.
 * Rotate the center-va segment by (i/n) of the
 * angle va - center -vb, and put the endpoint
 * of that segment in r.
 */
static void get_point_on_round_profile(float r[3], float offset, int i, int count,
                                       float va[3], float v[3], float vb[3])
{
	float vva[3], vvb[3], angle, center[3], rv[3], axis[3], co[3];

	sub_v3_v3v3(vva, va, v);
	sub_v3_v3v3(vvb, vb, v);
	normalize_v3(vva);
	normalize_v3(vvb);
	angle = angle_v3v3(vva, vvb);

	add_v3_v3v3(center, vva, vvb);
	normalize_v3(center);
	mul_v3_fl(center, offset * (1.0f / cosf(0.5f * angle)));
	add_v3_v3(center, v);           /* coordinates of the center of the inscribed circle */


	sub_v3_v3v3(rv, va, center);    /* radius vector */


	sub_v3_v3v3(co, v, center);
	cross_v3_v3v3(axis, rv, co);    /* calculate axis */

	sub_v3_v3v3(vva, va, center);
	sub_v3_v3v3(vvb, vb, center);
	angle = angle_v3v3(vva, vvb);

	rotate_v3_v3v3fl(co, rv, axis, angle * (float)(i) / (float)(count));

	add_v3_v3(co, center);
	copy_v3_v3(r, co);
}

/*
 * Find the point (i/n) of the way around the round profile for e,
 * where start point is va, midarc point is vmid, and end point is vb.
 * Return the answer in profileco.
 * Method:
 * Adjust va and vb (along edge direction) so that they are perpendicular
 * to edge at v, then use get_point_on_round_profile, then project
 * back onto original va - vmid - vb plane.
 * If va, vmid, and vb are all on the same plane, just interpolate between va and vb.
 */
static void get_point_on_round_edge(EdgeHalf *e, int i,
                                    float va[3], float vmid[3], float vb[3], float profileco[3])
{
	float vva[3], vvb[3],  point[3], dir[3], vaadj[3], vbadj[3], p2[3], pn[3];
	int n = e->seg;

	sub_v3_v3v3(vva, va, vmid);
	sub_v3_v3v3(vvb, vb, vmid);
	if (e->isrev)
		sub_v3_v3v3(dir, e->e->v1->co, e->e->v2->co);
	else
		sub_v3_v3v3(dir, e->e->v2->co, e->e->v1->co);
	normalize_v3(dir);
	if (fabsf(angle_v3v3(vva, vvb) - (float)M_PI) > (float)BEVEL_EPSILON) {
		copy_v3_v3(vaadj, va);
		madd_v3_v3fl(vaadj, dir, -len_v3(vva) * cosf(angle_v3v3(vva, dir)));
		copy_v3_v3(vbadj, vb);
		madd_v3_v3fl(vbadj, dir, -len_v3(vvb) * cosf(angle_v3v3(vvb, dir)));

		get_point_on_round_profile(point, e->offset, i, n, vaadj, vmid, vbadj);

		add_v3_v3v3(p2, point, dir);
		cross_v3_v3v3(pn, vva, vvb);
		if (!isect_line_plane_v3(profileco, point, p2, vmid, pn, 0)) {
			/* TODO: track down why this sometimes fails */
			copy_v3_v3(profileco, point);
		}
	}
	else {
		/* planar case */
		interp_v3_v3v3(profileco, va, vb, (float) i / (float) n);
	}
}

static void mid_v3_v3v3v3(float v[3], const float v1[3], const float v2[3], const float v3[3])
{
	v[0] = (v1[0] + v2[0] + v3[0]) / 3.0f;
	v[1] = (v1[1] + v2[1] + v3[1]) / 3.0f;
	v[2] = (v1[2] + v2[2] + v3[2]) / 3.0f;
}

/* Make a circular list of BoundVerts for bv, each of which has the coordinates
 * of a vertex on the the boundary of the beveled vertex bv->v.
 * Also decide on the mesh pattern that will be used inside the boundary.
 * Doesn't make the actual BMVerts */
static void build_boundary(BevVert *bv)
{
	EdgeHalf *efirst, *e;
	BoundVert *v;
	VMesh *vm;
	float co[3], *no;
	float lastd;

	e = efirst = next_bev(bv, NULL);
	vm = bv->vmesh;

	BLI_assert(bv->edgecount >= 2);  /* since bevel edges incident to 2 faces */

	if (bv->edgecount == 2 && bv->selcount == 1) {
		/* special case: beveled edge meets non-beveled one at valence 2 vert */
		no = e->fprev ? e->fprev->no : (e->fnext ? e->fnext->no : NULL);
		offset_in_plane(e, no, TRUE, co);
		v = add_new_bound_vert(vm, co);
		v->efirst = v->elast = v->ebev = e;
		e->leftv = v;
		no = e->fnext ? e->fnext->no : (e->fprev ? e->fprev->no : NULL);
		offset_in_plane(e, no, FALSE, co);
		v = add_new_bound_vert(vm, co);
		v->efirst = v->elast = e;
		e->rightv = v;
		/* make artifical extra point along unbeveled edge, and form triangle */
		slide_dist(e->next, bv->v, e->offset, co);
		v = add_new_bound_vert(vm, co);
		v->efirst = v->elast = e->next;
		vm->mesh_kind = M_POLY;
		return;
	}

	lastd = e->offset;
	vm->boundstart = NULL;
	do {
		if (e->isbev) {
			/* handle only left side of beveled edge e here: next iteration should do right side */
			if (e->prev->isbev) {
				BLI_assert(e->prev != e);  /* see: wire edge special case */
				offset_meet(e->prev, e, bv->v, e->fprev, TRUE, co);
				v = add_new_bound_vert(vm, co);
				v->efirst = e->prev;
				v->elast = v->ebev = e;
				e->leftv = v;
				e->prev->rightv = v;
			}
			else {
				/* e->prev is not beveled */
				if (e->prev->prev->isbev) {
					BLI_assert(e->prev->prev != e); /* see: edgecount 2, selcount 1 case */
					/* find meet point between e->prev->prev and e and attach e->prev there */
					/* TODO: fix case when one or both faces in following are NULL */
					offset_in_two_planes(e->prev->prev, e, bv->v,
					                     e->prev->prev->fnext, e->fprev, co);
					v = add_new_bound_vert(vm, co);
					v->efirst = e->prev->prev;
					v->elast = v->ebev = e;
					e->leftv = v;
					e->prev->leftv = v;
					e->prev->prev->rightv = v;
				}
				else {
					/* neither e->prev nor e->prev->prev are beveled: make on-edge on e->prev */
					offset_meet(e->prev, e, bv->v, e->fprev, TRUE, co);
					v = add_new_bound_vert(vm, co);
					v->efirst = e->prev;
					v->elast = v->ebev = e;
					e->leftv = v;
					e->prev->leftv = v;
				}
			}
			lastd = len_v3v3(bv->v->co, v->nv.co);
		}
		else {
			/* e is not beveled */
			if (e->next->isbev) {
				/* next iteration will place e between beveled previous and next edges */
				e = e->next;
				continue;
			}
			if (e->prev->isbev) {
				/* on-edge meet between e->prev and e */
				offset_meet(e->prev, e, bv->v, e->fprev, TRUE, co);
				v = add_new_bound_vert(vm, co);
				v->efirst = e->prev;
				v->elast = e;
				e->leftv = v;
				e->prev->rightv = v;
			}
			else {
				/* None of e->prev, e, e->next are beveled.
				 * could either leave alone or add slide points to make
				 * one polygon around bv->v.  For now, we choose latter.
				 * Could slide to make an even bevel plane but for now will
				 * just use last distance a meet point moved from bv->v. */
				slide_dist(e, bv->v, lastd, co);
				v = add_new_bound_vert(vm, co);
				v->efirst = v->elast = e;
				e->leftv = v;
			}
		}
		e = e->next;
	} while (e != efirst);

	BLI_assert(vm->count >= 2);
	if (vm->count == 2 && bv->edgecount == 3)
		vm->mesh_kind = M_NONE;
	else if (efirst->seg == 1 || bv->selcount == 1)
		vm->mesh_kind = M_POLY;
	else
		vm->mesh_kind = M_ADJ;
	/* TODO: if vm->count == 4 and bv->selcount == 4, use M_CROSS pattern */
}

/*
 * Given that the boundary is built and the boundary BMVerts have been made,
 * calculate the positions of the interior mesh points for the M_ADJ pattern,
 * then make the BMVerts and the new faces. */
static void bevel_build_rings(BMesh *bm, BevVert *bv)
{
	int k, ring, i, n, ns, ns2, nn;
	VMesh *vm = bv->vmesh;
	BoundVert *v, *vprev, *vnext;
	NewVert *nv, *nvprev, *nvnext;
	BMVert *bmv, *bmv1, *bmv2, *bmv3, *bmv4;
	BMFace *f;
	float co[3], coa[3], cob[3], midco[3];

	n = vm->count;
	ns = vm->seg;
	ns2 = ns / 2;
	BLI_assert(n > 2 && ns > 1);
	/* Make initial rings, going between points on neighbors.
	 * After this loop, will have coords for all (i, r, k) where
	 * BoundVert for i has a bevel, 0 <= r <= ns2, 0 <= k <= ns */
	for (ring = 1; ring <= ns2; ring++) {
		v = vm->boundstart;
		do {
			i = v->index;
			if (v->ebev) {
				/* get points coords of points a and b, on outer rings
				 * of prev and next edges, k away from this edge */
				vprev = v->prev;
				vnext = v->next;

				if (vprev->ebev)
					nvprev = mesh_vert(vm, vprev->index, 0, ns - ring);
				else
					nvprev = mesh_vert(vm, vprev->index, 0, ns);
				copy_v3_v3(coa, nvprev->co);
				nv = mesh_vert(vm, i, ring, 0);
				copy_v3_v3(nv->co, coa);
				nv->v = nvprev->v;

				if (vnext->ebev)
					nvnext = mesh_vert(vm, vnext->index, 0, ring);
				else
					nvnext = mesh_vert(vm, vnext->index, 0, 0);
				copy_v3_v3(cob, nvnext->co);
				nv = mesh_vert(vm, i, ring, ns);
				copy_v3_v3(nv->co, cob);
				nv->v = nvnext->v;

				/* TODO: better calculation of new midarc point? */
				project_to_edge(v->ebev->e, coa, cob, midco);

				for (k = 1; k < ns; k++) {
					get_point_on_round_edge(v->ebev, k, coa, midco, cob, co);
					copy_v3_v3(mesh_vert(vm, i, ring, k)->co, co);
				}
			}
			v = v->next;
		} while (v != vm->boundstart);
	}

	/* Now make sure cross points of rings share coordinates and vertices.
	 * After this loop, will have BMVerts for all (i, r, k) where
	 * i is for a BoundVert that is beveled and has either a predecessor or
	 * successor BoundVert beveled too, and
	 * for odd ns: 0 <= r <= ns2, 0 <= k <= ns
	 * for even ns: 0 <= r < ns2, 0 <= k <= ns except k=ns2 */
	v = vm->boundstart;
	do {
		i = v->index;
		if (v->ebev) {
			vprev = v->prev;
			vnext = v->next;
			if (vprev->ebev) {
				for (ring = 1; ring <= ns2; ring++) {
					for (k = 1; k <= ns2; k++) {
						if (ns % 2 == 0 && (k == ns2 || ring == ns2))
							continue;  /* center line is special case: do after the rest are done */
						nv = mesh_vert(vm, i, ring, k);
						nvprev = mesh_vert(vm, vprev->index, k, ns - ring);
						mid_v3_v3v3(co, nv->co, nvprev->co);
						copy_v3_v3(nv->co, co);
						BLI_assert(nv->v == NULL && nvprev->v == NULL);
						create_mesh_bmvert(bm, vm, i, ring, k, bv->v);
						copy_mesh_vert(vm, vprev->index, k, ns - ring, i, ring, k);
					}
				}
				if (!vprev->prev->ebev) {
					for (ring = 1; ring <= ns2; ring++) {
						for (k = 1; k <= ns2; k++) {
							if (ns % 2 == 0 && (k == ns2 || ring == ns2))
								continue;
							create_mesh_bmvert(bm, vm, vprev->index, ring, k, bv->v);
						}
					}
				}
				if (!vnext->ebev) {
					for (ring = 1; ring <= ns2; ring++) {
						for (k = ns - ns2; k < ns; k++) {
							if (ns % 2 == 0 && (k == ns2 || ring == ns2))
								continue;
							create_mesh_bmvert(bm, vm, i, ring, k, bv->v);
						}
					}
				}
			}
		}
		v = v->next;
	} while (v != vm->boundstart);

	if (ns % 2 == 0) {
		/* Do special case center lines.
		 * This loop makes verts for (i, ns2, k) for 1 <= k <= ns-1, k!=ns2
		 * and for (i, r, ns2) for 1 <= r <= ns2-1,
		 * whenever i is in a sequence of at least two beveled verts */
		v = vm->boundstart;
		do {
			i = v->index;
			if (v->ebev) {
				vprev = v->prev;
				vnext = v->next;
				for (k = 1; k < ns2; k++) {
					nv = mesh_vert(vm, i, k, ns2);
					if (vprev->ebev)
						nvprev = mesh_vert(vm, vprev->index, ns2, ns - k);
					if (vnext->ebev)
						nvnext = mesh_vert(vm, vnext->index, ns2, k);
					if (vprev->ebev && vnext->ebev) {
						mid_v3_v3v3v3(co, nvprev->co, nv->co, nvnext->co);
						copy_v3_v3(nv->co, co);
						create_mesh_bmvert(bm, vm, i, k, ns2, bv->v);
						copy_mesh_vert(vm, vprev->index, ns2, ns - k, i, k, ns2);
						copy_mesh_vert(vm, vnext->index, ns2, k, i, k, ns2);

					}
					else if (vprev->ebev) {
						mid_v3_v3v3(co, nvprev->co, nv->co);
						copy_v3_v3(nv->co, co);
						create_mesh_bmvert(bm, vm, i, k, ns2, bv->v);
						copy_mesh_vert(vm, vprev->index, ns2, ns - k, i, k, ns2);

						create_mesh_bmvert(bm, vm, i, ns2, ns - k, bv->v);
					}
					else if (vnext->ebev) {
						mid_v3_v3v3(co, nv->co, nvnext->co);
						copy_v3_v3(nv->co, co);
						create_mesh_bmvert(bm, vm, i, k, ns2, bv->v);
						copy_mesh_vert(vm, vnext->index, ns2, k, i, k, ns2);

						create_mesh_bmvert(bm, vm, i, ns2, k, bv->v);
					}
				}
			}
			v = v->next;
		} while (v != vm->boundstart);

		/* center point need to be average of all centers of rings */
		/* TODO: this is wrong if not all verts have ebev: could have
		 * several disconnected sections of mesh. */
		zero_v3(midco);
		nn = 0;
		v = vm->boundstart;
		do {
			i = v->index;
			if (v->ebev) {
				nv = mesh_vert(vm, i, ns2, ns2);
				add_v3_v3(midco, nv->co);
				nn++;
			}
			v = v->next;
		} while (v != vm->boundstart);
		mul_v3_fl(midco, 1.0f / nn);
		bmv = BM_vert_create(bm, midco, NULL);
		v = vm->boundstart;
		do {
			i = v->index;
			if (v->ebev) {
				nv = mesh_vert(vm, i, ns2, ns2);
				copy_v3_v3(nv->co, midco);
				nv->v = bmv;
			}
			v = v->next;
		} while (v != vm->boundstart);
	}

	/* Make the ring quads */
	for (ring = 0; ring < ns2; ring++) {
		v = vm->boundstart;
		do {
			i = v->index;
			f = boundvert_rep_face(v);
			if (v->ebev && (v->prev->ebev || v->next->ebev)) {
				for (k = 0; k < ns2 + (ns % 2); k++) {
					bmv1 = mesh_vert(vm, i, ring, k)->v;
					bmv2 = mesh_vert(vm, i, ring, k + 1)->v;
					bmv3 = mesh_vert(vm, i, ring + 1, k + 1)->v;
					bmv4 = mesh_vert(vm, i, ring + 1, k)->v;
					BLI_assert(bmv1 && bmv2 && bmv3 && bmv4);
					if (bmv3 == bmv4 || bmv1 == bmv4)
						bmv4 = NULL;
					bev_create_quad_tri(bm, bmv1, bmv2, bmv3, bmv4, f);
				}
			}
			else if (v->prev->ebev && v->prev->prev->ebev) {
				/* finish off a sequence of beveled edges */
				i = v->prev->index;
				f = boundvert_rep_face(v->prev);
				for (k = ns2 + (ns % 2); k < ns; k++) {
					bmv1 = mesh_vert(vm, i, ring, k)->v;
					bmv2 = mesh_vert(vm, i, ring, k + 1)->v;
					bmv3 = mesh_vert(vm, i, ring + 1, k + 1)->v;
					bmv4 = mesh_vert(vm, i, ring + 1, k)->v;
					BLI_assert(bmv1 && bmv2 && bmv3 && bmv4);
					if (bmv2 == bmv3) {
						bmv3 = bmv4;
						bmv4 = NULL;
					}
					bev_create_quad_tri(bm, bmv1, bmv2, bmv3, bmv4, f);
				}
			}
			v = v->next;
		} while (v != vm->boundstart);
	}

	/* Make center ngon if odd number of segments and fully beveled */
	if (ns % 2 == 1 && vm->count == bv->selcount) {
		BMVert **vv = NULL;
		BLI_array_declare(vv);

		v = vm->boundstart;
		do {
			i = v->index;
			BLI_assert(v->ebev);
			BLI_array_append(vv, mesh_vert(vm, i, ns2, ns2)->v);
			v = v->next;
		} while (v != vm->boundstart);
		f = boundvert_rep_face(vm->boundstart);
		bev_create_ngon(bm, vv, BLI_array_count(vv), f);

		BLI_array_free(vv);
	}

	/* Make 'rest-of-vmesh' polygon if not fully beveled */
	if (vm->count > bv->selcount) {
		int j;
		BMVert **vv = NULL;
		BLI_array_declare(vv);

		v = vm->boundstart;
		f = boundvert_rep_face(v);
		j = 0;
		do {
			i = v->index;
			if (v->ebev) {
				if (!v->prev->ebev) {
					for (k = 0; k < ns2; k++) {
						bmv1 = mesh_vert(vm, i, ns2, k)->v;
						if (!bmv1)
							bmv1 = mesh_vert(vm, i, 0, k)->v;
						if (!(j > 0 && bmv1 == vv[j - 1])) {
							BLI_assert(bmv1 != NULL);
							BLI_array_append(vv, bmv1);
							j++;
						}
					}
				}
				bmv1 = mesh_vert(vm, i, ns2, ns2)->v;
				if (!bmv1)
					bmv1 = mesh_vert(vm, i, 0, ns2)->v;
				if (!(j > 0 && bmv1 == vv[j - 1])) {
					BLI_assert(bmv1 != NULL);
					BLI_array_append(vv, bmv1);
					j++;
				}
				if (!v->next->ebev) {
					for (k = ns - ns2; k < ns; k++) {
						bmv1 = mesh_vert(vm, i, ns2, k)->v;
						if (!bmv1)
							bmv1 = mesh_vert(vm, i, 0, k)->v;
						if (!(j > 0 && bmv1 == vv[j - 1])) {
							BLI_assert(bmv1 != NULL);
							BLI_array_append(vv, bmv1);
							j++;
						}
					}
				}
			}
			else {
				BLI_assert(mesh_vert(vm, i, 0, 0)->v != NULL);
				BLI_array_append(vv, mesh_vert(vm, i, 0, 0)->v);
				j++;
			}
			v = v->next;
		} while (v != vm->boundstart);
		if (vv[0] == vv[j - 1])
			j--;
		bev_create_ngon(bm, vv, j, f);

		BLI_array_free(vv);
	}
}

static void bevel_build_poly(BMesh *bm, BevVert *bv)
{
	int n, k;
	VMesh *vm = bv->vmesh;
	BoundVert *v;
	BMVert **vv = NULL;
	BLI_array_declare(vv);

	v = vm->boundstart;
	n = 0;
	do {
		/* accumulate vertices for vertex ngon */
		BLI_array_append(vv, v->nv.v);
		n++;
		if (v->ebev && v->ebev->seg > 1) {
			for (k = 1; k < v->ebev->seg; k++) {
				BLI_array_append(vv, mesh_vert(vm, v->index, 0, k)->v);
				n++;
			}
		}
		v = v->next;
	} while (v != vm->boundstart);
	if (n > 2)
		bev_create_ngon(bm, vv, n, boundvert_rep_face(v));
	BLI_array_free(vv);
}

/* Given that the boundary is built, now make the actual BMVerts
 * for the boundary and the interior of the vertex mesh. */
static void build_vmesh(BMesh *bm, BevVert *bv)
{
	VMesh *vm = bv->vmesh;
	BoundVert *v, *weld1, *weld2;
	int n, ns, ns2, i, k, weld;
	float *va, *vb, co[3], midco[3];

	n = vm->count;
	ns = vm->seg;
	ns2 = ns / 2;

	vm->mesh = (NewVert *)MEM_callocN(n * (ns2 + 1) * (ns + 1) * sizeof(NewVert), "NewVert");

	/* special case: two beveled ends welded together */
	weld = (bv->selcount == 2) && (vm->count == 2);
	weld1 = weld2 = NULL;   /* will hold two BoundVerts involved in weld */

	/* make (i, 0, 0) mesh verts for all i */
	v = vm->boundstart;
	do {
		i = v->index;
		copy_v3_v3(mesh_vert(vm, i, 0, 0)->co, v->nv.co);
		create_mesh_bmvert(bm, vm, i, 0, 0, bv->v);
		v->nv.v = mesh_vert(vm, i, 0, 0)->v;
		if (weld && v->ebev) {
			if (!weld1)
				weld1 = v;
			else
				weld2 = v;
		}
		v = v->next;
	} while (v != vm->boundstart);

	/* copy other ends to (i, 0, ns) for all i, and fill in profiles for beveled edges */
	v = vm->boundstart;
	do {
		i = v->index;
		copy_mesh_vert(vm, i, 0, ns, v->next->index, 0, 0);
		if (v->ebev) {
			va = mesh_vert(vm, i, 0, 0)->co;
			vb = mesh_vert(vm, i, 0, ns)->co;
			project_to_edge(v->ebev->e, va, vb, midco);
			for (k = 1; k < ns; k++) {
				get_point_on_round_edge(v->ebev, k, va, midco, vb, co);
				copy_v3_v3(mesh_vert(vm, i, 0, k)->co, co);
				if (!weld)
					create_mesh_bmvert(bm, vm, i, 0, k, bv->v);
			}
		}
		v = v->next;
	} while (v != vm->boundstart);

	if (weld) {
		vm->mesh_kind = M_NONE;
		for (k = 1; k < ns; k++) {
			va = mesh_vert(vm, weld1->index, 0, k)->co;
			vb = mesh_vert(vm, weld2->index, 0, ns - k)->co;
			mid_v3_v3v3(co, va, vb);
			copy_v3_v3(mesh_vert(vm, weld1->index, 0, k)->co, co);
			create_mesh_bmvert(bm, vm, weld1->index, 0, k, bv->v);
		}
		for (k = 1; k < ns; k++)
			copy_mesh_vert(vm, weld2->index, 0, ns - k, weld1->index, 0, k);
	}

	if (vm->mesh_kind == M_ADJ)
		bevel_build_rings(bm, bv);
	else if (vm->mesh_kind == M_POLY)
		bevel_build_poly(bm, bv);
}

/*
 * Construction around the vertex
 */
static void bevel_vert_construct(BMesh *bm, BevelParams *bp, BMOperator *op, BMVert *v)
{

	BMOIter siter;
	BMEdge *bme;
	BevVert *bv;
	BMEdge *bme2, *unflagged_bme;
	BMFace *f;
	BMIter iter, iter2;
	EdgeHalf *e;
	int i, ntot, found_shared_face, ccw_test_sum;
	int nsel = 0;

	/* Gather input selected edges.
	 * Only bevel selected edges that have exactly two incident faces. */
	BMO_ITER (bme, &siter, bm, op, "geom", BM_EDGE) {
		if ((bme->v1 == v) || (BM_edge_other_vert(bme, bme->v1) == v)) {
			if (BM_edge_is_manifold(bme)) {
				BMO_elem_flag_enable(bm, bme, EDGE_SELECTED);
				nsel++;
			}
		}
	}

	if (nsel == 0)
		return;

	ntot = BM_vert_edge_count(v);
	bv = (BevVert *)MEM_callocN(sizeof(BevVert), "BevVert");
	bv->v = v;
	bv->edgecount = ntot;
	bv->selcount = nsel;
	bv->edges = (EdgeHalf *)MEM_callocN(ntot * sizeof(EdgeHalf), "EdgeHalf");
	bv->vmesh = (VMesh *)MEM_callocN(sizeof(VMesh), "VMesh");
	bv->vmesh->seg = bp->seg;
	BLI_addtail(&bp->vertList, bv);

	/* add edges to bv->edges in order that keeps adjacent edges sharing
	 * a face, if possible */
	i = 0;
	bme = v->e;
	BMO_elem_flag_enable(bm, bme, BEVEL_FLAG);
	e = &bv->edges[0];
	e->e = bme;
	for (i = 0; i < ntot; i++) {
		if (i > 0) {
			/* find an unflagged edge bme2 that shares a face f with previous bme */
			found_shared_face = 0;
			unflagged_bme = NULL;
			BM_ITER_ELEM (bme2, &iter, v, BM_EDGES_OF_VERT) {
				if (BMO_elem_flag_test(bm, bme2, BEVEL_FLAG))
					continue;
				if (!unflagged_bme)
					unflagged_bme = bme2;
				BM_ITER_ELEM (f, &iter2, bme2, BM_FACES_OF_EDGE) {
					if (BM_face_edge_share_loop(f, bme)) {
						found_shared_face = 1;
						break;
					}
				}
				if (found_shared_face)
					break;
			}
			e = &bv->edges[i];
			if (found_shared_face) {
				e->e = bme2;
				e->fprev = f;
				bv->edges[i - 1].fnext = f;
			}
			else {
				e->e = unflagged_bme;
			}
		}
		bme = e->e;
		BMO_elem_flag_enable(bm, bme, BEVEL_FLAG);
		if (BMO_elem_flag_test(bm, bme, EDGE_SELECTED)) {
			e->isbev = 1;
			e->seg = bp->seg;
		}
		else {
			e->isbev = 0;
			e->seg = 0;
		}
		e->isrev = (bme->v2 == v);
		e->offset = e->isbev ? bp->offset : 0.0f;
	}
	/* find wrap-around shared face */
	BM_ITER_ELEM (f, &iter2, bme, BM_FACES_OF_EDGE) {
		if (BM_face_edge_share_loop(f, bv->edges[0].e)) {
			if (bv->edges[0].fnext == f)
				continue;   /* if two shared faces, want the other one now */
			bv->edges[ntot - 1].fnext = f;
			bv->edges[0].fprev = f;
			break;
		}
	}

	/* remove BEVEL_FLAG now that we are finished with it*/
	for (i = 0; i < ntot; i++)
		BMO_elem_flag_disable(bm, bv->edges[i].e, BEVEL_FLAG);

	/* if edge array doesn't go CCW around vertex from average normal side,
	 * reverse the array, being careful to reverse face pointers too */
	if (ntot > 1) {
		ccw_test_sum = 0;
		for (i = 0; i < ntot; i++)
			ccw_test_sum += bev_ccw_test(bv->edges[i].e, bv->edges[(i + 1) % ntot].e,
			                             bv->edges[i].fnext);
		if (ccw_test_sum < 0) {
			for (i = 0; i <= (ntot / 2) - 1; i++) {
				SWAP(EdgeHalf, bv->edges[i], bv->edges[ntot - i - 1]);
				SWAP(BMFace *, bv->edges[i].fprev, bv->edges[i].fnext);
				SWAP(BMFace *, bv->edges[ntot - i - 1].fprev, bv->edges[ntot - i - 1].fnext);
			}
			if (ntot % 2 == 1) {
				i = ntot / 2;
				SWAP(BMFace *, bv->edges[i].fprev,  bv->edges[i].fnext);
			}
		}
	}

	for (i = 0; i < ntot; i++) {
		e = &bv->edges[i];
		e->next = &bv->edges[(i + 1) % ntot];
		e->prev = &bv->edges[(i + ntot - 1) % ntot];
	}

	build_boundary(bv);
	build_vmesh(bm, bv);
}

/* Face f has at least one beveled vertex.  Rebuild f */
static void rebuild_polygon(BMesh *bm, BevelParams *bp, BMFace *f)
{
	BMIter liter;
	BMLoop *l, *lprev;
	BevVert *bv;
	BoundVert *v, *vstart, *vend;
	EdgeHalf *e, *eprev;
	VMesh *vm;
	int i, k;
	BMVert *bmv;
	BMVert **vv = NULL;
	BLI_array_declare(vv);

	BM_ITER_ELEM (l, &liter, f, BM_LOOPS_OF_FACE) {
		bv = find_bevvert(bp, l->v);
		if (bv) {
			lprev = l->prev;
			e = find_edge_half(bv, l->e);
			eprev = find_edge_half(bv, lprev->e);
			BLI_assert(e != NULL && eprev != NULL);
			vstart = eprev->leftv;
			if (e->isbev)
				vend = e->rightv;
			else
				vend = e->leftv;
			v = vstart;
			vm = bv->vmesh;
			BLI_array_append(vv, v->nv.v);
			while (v != vend) {
				if (vm->mesh_kind == M_NONE && v->ebev && v->ebev->seg > 1 && v->ebev != e && v->ebev != eprev) {
					/* case of 3rd face opposite a beveled edge, with no vmesh */
					i = v->index;
					e = v->ebev;
					for (k = 1; k < e->seg; k++) {
						bmv = mesh_vert(vm, i, 0, k)->v;
						BLI_array_append(vv, bmv);
					}
				}
				v = v->prev;
				BLI_array_append(vv, v->nv.v);
			}
		}
		else {
			BLI_array_append(vv, l->v);
		}
	}
	bev_create_ngon(bm, vv, BLI_array_count(vv), f);
	BLI_array_free(vv);
}

/* All polygons touching v need rebuilding because beveling v has made new vertices */
static void bevel_rebuild_existing_polygons(BMesh *bm, BevelParams *bp, BMVert *v)
{
	BMFace *f;
	BMIter iter;

	/* TODO: don't iterate through all faces, but just local geometry around v */
	BM_ITER_MESH (f, &iter, bm, BM_FACES_OF_MESH) {
		BMLoop *l = f->l_first;
		do {
			if (l->v == v) {
				rebuild_polygon(bm, bp, f);
				BM_face_kill(bm, f);
			}
			l = l->next;
		} while (l != f->l_first);
	}
}



/*
 * Build the polygons along the selected Edge
 */
static void bevel_build_edge_polygons(BMesh *bm, BevelParams *bp, BMEdge *bme)
{
	BevVert *bv1, *bv2;
	BMVert *bmv1, *bmv2, *bmv3, *bmv4, *bmv1i, *bmv2i, *bmv3i, *bmv4i;
	VMesh *vm1, *vm2;
	EdgeHalf *e1, *e2;
	BMFace *f1, *f2, *f;
	int k, nseg, i1, i2;

	if (!BM_edge_is_manifold(bme))
		return;

	bv1 = find_bevvert(bp, bme->v1);
	bv2 = find_bevvert(bp, bme->v2);

	BLI_assert(bv1 && bv2);

	e1 = find_edge_half(bv1, bme);
	e2 = find_edge_half(bv2, bme);

	BLI_assert(e1 && e2);

	/*	v4                       v3
	 *       \                      /
	 *        e->v1 - e->v2
	 *       /                      \
	 *      v1                       v2 */

	nseg = e1->seg;
	BLI_assert(nseg > 0 && nseg == e2->seg);

	bmv1 = e1->leftv->nv.v;
	bmv4 = e1->rightv->nv.v;
	bmv2 = e2->rightv->nv.v;
	bmv3 = e2->leftv->nv.v;

	BLI_assert(bmv1 && bmv2 && bmv3 && bmv4);

	f1 = boundvert_rep_face(e1->leftv);
	f2 = boundvert_rep_face(e1->rightv);

	if (nseg == 1) {
		bev_create_quad_tri(bm, bmv1, bmv2, bmv3, bmv4, f1);
	}
	else {
		i1 = e1->leftv->index;
		i2 = e2->leftv->index;
		vm1 = bv1->vmesh;
		vm2 = bv2->vmesh;
		bmv1i = bmv1;
		bmv2i = bmv2;
		for (k = 1; k <= nseg; k++) {
			bmv4i = mesh_vert(vm1, i1, 0, k)->v;
			bmv3i = mesh_vert(vm2, i2, 0, nseg - k)->v;
			f = (k <= nseg / 2 + (nseg % 2)) ? f1 : f2;
			bev_create_quad_tri(bm, bmv1i, bmv2i, bmv3i, bmv4i, f);
			bmv1i = bmv4i;
			bmv2i = bmv3i;
		}
	}
}


static void free_bevel_params(BevelParams *bp)
{
	BevVert *bv;
	VMesh *vm;
	BoundVert *v, *vnext;

	for (bv = bp->vertList.first; bv; bv = bv->next) {
		MEM_freeN(bv->edges);
		vm = bv->vmesh;
		v = vm->boundstart;
		if (v) {
			do {
				vnext = v->next;
				MEM_freeN(v);
				v = vnext;
			} while (v != vm->boundstart);
		}
		if (vm->mesh)
			MEM_freeN(vm->mesh);
		MEM_freeN(vm);
	}
	BLI_freelistN(&bp->vertList);
}

void bmo_bevel_exec(BMesh *bm, BMOperator *op)
{
	BMOIter siter;
	BMVert *v;
	BMEdge *e;
	BevelParams bp;

	bp.offset = BMO_slot_float_get(op, "offset");
	bp.op = op;
	bp.seg = BMO_slot_int_get(op, "segments");

	if (bp.offset > 0) {
		bp.vertList.first = bp.vertList.last = NULL;

		/* The analysis of the input vertices and execution additional constructions */
		BMO_ITER (v, &siter, bm, op, "geom", BM_VERT) {
			bevel_vert_construct(bm, &bp, op, v);
		}
		/* Build polygons for edges */
		BMO_ITER (e, &siter, bm, op, "geom", BM_EDGE) {
			bevel_build_edge_polygons(bm, &bp, e);
		}

		BMO_ITER (v, &siter, bm, op, "geom", BM_VERT) {
			bevel_rebuild_existing_polygons(bm, &bp, v);
		}

		BMO_ITER (v, &siter, bm, op, "geom", BM_VERT) {
			if (find_bevvert(&bp, v))
				BM_vert_kill(bm, v);
		}
		free_bevel_params(&bp);
	}

}

#else
#define BEVEL_FLAG  1
#define BEVEL_DEL   2
#define FACE_NEW    4
#define EDGE_OLD    8
#define FACE_OLD    16
#define VERT_OLD    32
#define FACE_SPAN   64
#define FACE_HOLE   128

typedef struct LoopTag {
	BMVert *newv;
} LoopTag;

typedef struct EdgeTag {
	BMVert *newv1, *newv2;
} EdgeTag;

static void calc_corner_co(BMLoop *l, const float fac, float r_co[3],
                           const short do_dist, const short do_even)
{
	float no[3], l_vec_prev[3], l_vec_next[3], l_co_prev[3], l_co[3], l_co_next[3], co_ofs[3];
	int is_concave;

	/* first get the prev/next verts */
	if (l->f->len > 2) {
		copy_v3_v3(l_co_prev, l->prev->v->co);
		copy_v3_v3(l_co, l->v->co);
		copy_v3_v3(l_co_next, l->next->v->co);

		/* calculate normal */
		sub_v3_v3v3(l_vec_prev, l_co_prev, l_co);
		sub_v3_v3v3(l_vec_next, l_co_next, l_co);

		cross_v3_v3v3(no, l_vec_prev, l_vec_next);
		is_concave = dot_v3v3(no, l->f->no) > 0.0f;
	}
	else {
		BMIter iter;
		BMLoop *l2;
		float up[3] = {0.0f, 0.0f, 1.0f};

		copy_v3_v3(l_co_prev, l->prev->v->co);
		copy_v3_v3(l_co, l->v->co);
		
		BM_ITER_ELEM (l2, &iter, l->v, BM_LOOPS_OF_VERT) {
			if (l2->f != l->f) {
				copy_v3_v3(l_co_next, BM_edge_other_vert(l2->e, l2->next->v)->co);
				break;
			}
		}
		
		sub_v3_v3v3(l_vec_prev, l_co_prev, l_co);
		sub_v3_v3v3(l_vec_next, l_co_next, l_co);

		cross_v3_v3v3(no, l_vec_prev, l_vec_next);
		if (dot_v3v3(no, no) == 0.0f) {
			no[0] = no[1] = 0.0f; no[2] = -1.0f;
		}
		
		is_concave = dot_v3v3(no, up) < 0.0f;
	}


	/* now calculate the new location */
	if (do_dist) { /* treat 'fac' as distance */

		normalize_v3(l_vec_prev);
		normalize_v3(l_vec_next);

		add_v3_v3v3(co_ofs, l_vec_prev, l_vec_next);
		if (UNLIKELY(normalize_v3(co_ofs) == 0.0f)) {  /* edges form a straight line */
			cross_v3_v3v3(co_ofs, l_vec_prev, l->f->no);
		}

		if (do_even) {
			negate_v3(l_vec_next);
			mul_v3_fl(co_ofs, fac * shell_angle_to_dist(0.5f * angle_normalized_v3v3(l_vec_prev, l_vec_next)));
			/* negate_v3(l_vec_next); */ /* no need unless we use again */
		}
		else {
			mul_v3_fl(co_ofs, fac);
		}
	}
	else { /* treat as 'fac' as a factor (0 - 1) */

		/* not strictly necessary, balance vectors
		 * so the longer edge doesn't skew the result,
		 * gives nicer, move even output.
		 *
		 * Use the minimum rather then the middle value so skinny faces don't flip along the short axis */
		float min_fac = min_ff(normalize_v3(l_vec_prev), normalize_v3(l_vec_next));
		float angle;

		if (do_even) {
			negate_v3(l_vec_next);
			angle = angle_normalized_v3v3(l_vec_prev, l_vec_next);
			negate_v3(l_vec_next); /* no need unless we use again */
		}
		else {
			angle = 0.0f;
		}

		mul_v3_fl(l_vec_prev, min_fac);
		mul_v3_fl(l_vec_next, min_fac);

		add_v3_v3v3(co_ofs, l_vec_prev, l_vec_next);

		if (UNLIKELY(is_zero_v3(co_ofs))) {
			cross_v3_v3v3(co_ofs, l_vec_prev, l->f->no);
			normalize_v3(co_ofs);
			mul_v3_fl(co_ofs, min_fac);
		}

		/* done */
		if (do_even) {
			mul_v3_fl(co_ofs, (fac * 0.5f) * shell_angle_to_dist(0.5f * angle));
		}
		else {
			mul_v3_fl(co_ofs, fac * 0.5f);
		}
	}

	/* apply delta vec */
	if (is_concave)
		negate_v3(co_ofs);

	add_v3_v3v3(r_co, co_ofs, l->v->co);
}


#define ETAG_SET(e, v, nv)  (                                                 \
	(v) == (e)->v1 ?                                                          \
		(etags[BM_elem_index_get((e))].newv1 = (nv)) :                        \
		(etags[BM_elem_index_get((e))].newv2 = (nv))                          \
	)

#define ETAG_GET(e, v)  (                                                     \
	(v) == (e)->v1 ?                                                          \
		(etags[BM_elem_index_get((e))].newv1) :                               \
		(etags[BM_elem_index_get((e))].newv2)                                 \
	)

void bmo_bevel_exec(BMesh *bm, BMOperator *op)
{
	BMOIter siter;
	BMIter iter;
	BMEdge *e;
	BMVert *v;
	BMFace **faces = NULL, *f;
	LoopTag *tags = NULL, *tag;
	EdgeTag *etags = NULL;
	BMVert **verts = NULL;
	BMEdge **edges = NULL;
	BLI_array_declare(faces);
	BLI_array_declare(tags);
	BLI_array_declare(etags);
	BLI_array_declare(verts);
	BLI_array_declare(edges);
	SmallHash hash;
	float fac = BMO_slot_float_get(op, "percent");
	const short do_even = BMO_slot_bool_get(op, "use_even");
	const short do_dist = BMO_slot_bool_get(op, "use_dist");
	int i, li, has_elens, HasMDisps = CustomData_has_layer(&bm->ldata, CD_MDISPS);
	
	has_elens = CustomData_has_layer(&bm->edata, CD_PROP_FLT) && BMO_slot_bool_get(op, "use_lengths");
	if (has_elens) {
		li = BMO_slot_int_get(op, "lengthlayer");
	}
	
	BLI_smallhash_init(&hash);
	
	BMO_ITER (e, &siter, bm, op, "geom", BM_EDGE) {
		BMO_elem_flag_enable(bm, e, BEVEL_FLAG | BEVEL_DEL);
		BMO_elem_flag_enable(bm, e->v1, BEVEL_FLAG | BEVEL_DEL);
		BMO_elem_flag_enable(bm, e->v2, BEVEL_FLAG | BEVEL_DEL);
		
		if (BM_edge_face_count(e) < 2) {
			BMO_elem_flag_disable(bm, e, BEVEL_DEL);
			BMO_elem_flag_disable(bm, e->v1, BEVEL_DEL);
			BMO_elem_flag_disable(bm, e->v2, BEVEL_DEL);
		}
#if 0
		if (BM_edge_is_wire(e)) {
			BMVert *verts[2] = {e->v1, e->v2};
			BMEdge *edges[2] = {e, BM_edge_create(bm, e->v1, e->v2, e, 0)};
			
			BMO_elem_flag_enable(bm, edges[1], BEVEL_FLAG);
			BM_face_create(bm, verts, edges, 2, FALSE);
		}
#endif
	}
	
	BM_ITER_MESH (v, &iter, bm, BM_VERTS_OF_MESH) {
		BMO_elem_flag_enable(bm, v, VERT_OLD);
	}

#if 0
	/* a bit of cleaner code that, alas, doens't work. */
	/* build edge tag */
	BM_ITER_MESH (e, &iter, bm, BM_EDGES_OF_MESH) {
		if (BMO_elem_flag_test(bm, e->v1, BEVEL_FLAG) || BMO_elem_flag_test(bm, e->v2, BEVEL_FLAG)) {
			BMIter liter;
			BMLoop *l;
			
			if (!BMO_elem_flag_test(bm, e, EDGE_OLD)) {
				BM_elem_index_set(e, BLI_array_count(etags)); /* set_dirty! */
				BLI_array_grow_one(etags);
				
				BMO_elem_flag_enable(bm, e, EDGE_OLD);
			}
			
			BM_ITER_ELEM (l, &liter, e, BM_LOOPS_OF_EDGE) {
				BMLoop *l2;
				BMIter liter2;
				
				if (BMO_elem_flag_test(bm, l->f, BEVEL_FLAG))
					continue;

				BM_ITER_ELEM (l2, &liter2, l->f, BM_LOOPS_OF_FACE) {
					BM_elem_index_set(l2, BLI_array_count(tags)); /* set_loop */
					BLI_array_grow_one(tags);

					if (!BMO_elem_flag_test(bm, l2->e, EDGE_OLD)) {
						BM_elem_index_set(l2->e, BLI_array_count(etags)); /* set_dirty! */
						BLI_array_grow_one(etags);
						
						BMO_elem_flag_enable(bm, l2->e, EDGE_OLD);
					}
				}

				BMO_elem_flag_enable(bm, l->f, BEVEL_FLAG);
				BLI_array_append(faces, l->f);
			}
		}
		else {
			BM_elem_index_set(e, -1); /* set_dirty! */
		}
	}
#endif
	
	/* create and assign looptag structure */
	BMO_ITER (e, &siter, bm, op, "geom", BM_EDGE) {
		BMLoop *l;
		BMIter liter;

		BMO_elem_flag_enable(bm, e->v1, BEVEL_FLAG | BEVEL_DEL);
		BMO_elem_flag_enable(bm, e->v2, BEVEL_FLAG | BEVEL_DEL);
		
		if (BM_edge_face_count(e) < 2) {
			BMO_elem_flag_disable(bm, e, BEVEL_DEL);
			BMO_elem_flag_disable(bm, e->v1, BEVEL_DEL);
			BMO_elem_flag_disable(bm, e->v2, BEVEL_DEL);
		}
		
		if (!BLI_smallhash_haskey(&hash, (intptr_t)e)) {
			BLI_array_grow_one(etags);
			BM_elem_index_set(e, BLI_array_count(etags) - 1); /* set_dirty! */
			BLI_smallhash_insert(&hash, (intptr_t)e, NULL);
			BMO_elem_flag_enable(bm, e, EDGE_OLD);
		}
		
		/* find all faces surrounding e->v1 and, e->v2 */
		for (i = 0; i < 2; i++) {
			BM_ITER_ELEM (l, &liter, i ? e->v2 : e->v1, BM_LOOPS_OF_VERT) {
				BMLoop *l2;
				BMIter liter2;
				
				/* see if we've already processed this loop's fac */
				if (BLI_smallhash_haskey(&hash, (intptr_t)l->f))
					continue;
				
				/* create tags for all loops in l-> */
				BM_ITER_ELEM (l2, &liter2, l->f, BM_LOOPS_OF_FACE) {
					BLI_array_grow_one(tags);
					BM_elem_index_set(l2, BLI_array_count(tags) - 1); /* set_loop */
					
					if (!BLI_smallhash_haskey(&hash, (intptr_t)l2->e)) {
						BLI_array_grow_one(etags);
						BM_elem_index_set(l2->e, BLI_array_count(etags) - 1); /* set_dirty! */
						BLI_smallhash_insert(&hash, (intptr_t)l2->e, NULL);
						BMO_elem_flag_enable(bm, l2->e, EDGE_OLD);
					}
				}

				BLI_smallhash_insert(&hash, (intptr_t)l->f, NULL);
				BMO_elem_flag_enable(bm, l->f, BEVEL_FLAG);
				BLI_array_append(faces, l->f);
			}
		}
	}

	bm->elem_index_dirty |= BM_EDGE;
	
	BM_ITER_MESH (v, &iter, bm, BM_VERTS_OF_MESH) {
		BMIter eiter;
		
		if (!BMO_elem_flag_test(bm, v, BEVEL_FLAG))
			continue;
		
		BM_ITER_ELEM (e, &eiter, v, BM_EDGES_OF_VERT) {
			if (!BMO_elem_flag_test(bm, e, BEVEL_FLAG) && !ETAG_GET(e, v)) {
				BMVert *v2;
				float co[3];
				
				v2 = BM_edge_other_vert(e, v);
				sub_v3_v3v3(co, v2->co, v->co);
				if (has_elens) {
					float elen = *(float *)CustomData_bmesh_get_n(&bm->edata, e->head.data, CD_PROP_FLT, li);

					normalize_v3(co);
					mul_v3_fl(co, elen);
				}
				
				mul_v3_fl(co, fac);
				add_v3_v3(co, v->co);
				
				v2 = BM_vert_create(bm, co, v);
				ETAG_SET(e, v, v2);
			}
		}
	}
	
	for (i = 0; i < BLI_array_count(faces); i++) {
		BMLoop *l;
		BMIter liter;
		
		BMO_elem_flag_enable(bm, faces[i], FACE_OLD);
		
		BM_ITER_ELEM (l, &liter, faces[i], BM_LOOPS_OF_FACE) {
			float co[3];

			if (BMO_elem_flag_test(bm, l->e, BEVEL_FLAG)) {
				if (BMO_elem_flag_test(bm, l->prev->e, BEVEL_FLAG)) {
					tag = tags + BM_elem_index_get(l);
					calc_corner_co(l, fac, co, do_dist, do_even);
					tag->newv = BM_vert_create(bm, co, l->v);
				}
				else {
					tag = tags + BM_elem_index_get(l);
					tag->newv = ETAG_GET(l->prev->e, l->v);
					
					if (!tag->newv) {
						sub_v3_v3v3(co, l->prev->v->co, l->v->co);
						if (has_elens) {
							float elen = *(float *)CustomData_bmesh_get_n(&bm->edata, l->prev->e->head.data,
							                                              CD_PROP_FLT, li);

							normalize_v3(co);
							mul_v3_fl(co, elen);
						}

						mul_v3_fl(co, fac);
						add_v3_v3(co, l->v->co);

						tag->newv = BM_vert_create(bm, co, l->v);
						
						ETAG_SET(l->prev->e, l->v, tag->newv);
					}
				}
			}
			else if (BMO_elem_flag_test(bm, l->v, BEVEL_FLAG)) {
				tag = tags + BM_elem_index_get(l);
				tag->newv = ETAG_GET(l->e, l->v);

				if (!tag->newv) {
					sub_v3_v3v3(co, l->next->v->co, l->v->co);
					if (has_elens) {
						float elen = *(float *)CustomData_bmesh_get_n(&bm->edata, l->e->head.data, CD_PROP_FLT, li);

						normalize_v3(co);
						mul_v3_fl(co, elen);
					}
					
					mul_v3_fl(co, fac);
					add_v3_v3(co, l->v->co);

					tag = tags + BM_elem_index_get(l);
					tag->newv = BM_vert_create(bm, co, l->v);
					
					ETAG_SET(l->e, l->v, tag->newv);
				}
			}
			else {
				tag = tags + BM_elem_index_get(l);
				tag->newv = l->v;
				BMO_elem_flag_disable(bm, l->v, BEVEL_DEL);
			}
		}
	}
	
	/* create new faces inset from original face */
	for (i = 0; i < BLI_array_count(faces); i++) {
		BMLoop *l;
		BMIter liter;
		BMFace *f;
		BMVert *lastv = NULL, *firstv = NULL;

		BMO_elem_flag_enable(bm, faces[i], BEVEL_DEL);
		
		BLI_array_empty(verts);
		BLI_array_empty(edges);
		
		BM_ITER_ELEM (l, &liter, faces[i], BM_LOOPS_OF_FACE) {
			BMVert *v2;
			
			tag = tags + BM_elem_index_get(l);
			BLI_array_append(verts, tag->newv);
			
			if (!firstv)
				firstv = tag->newv;
			
			if (lastv) {
				e = BM_edge_create(bm, lastv, tag->newv, l->e, TRUE);
				BM_elem_attrs_copy(bm, bm, l->prev->e, e);
				BLI_array_append(edges, e);
			}
			lastv = tag->newv;
			
			v2 = ETAG_GET(l->e, l->next->v);
			
			tag = &tags[BM_elem_index_get(l->next)];
			if (!BMO_elem_flag_test(bm, l->e, BEVEL_FLAG) && v2 && v2 != tag->newv) {
				BLI_array_append(verts, v2);
				
				e = BM_edge_create(bm, lastv, v2, l->e, TRUE);
				BM_elem_attrs_copy(bm, bm, l->e, e);
				
				BLI_array_append(edges, e);
				lastv = v2;
			}
		}
		
		e = BM_edge_create(bm, firstv, lastv, BM_FACE_FIRST_LOOP(faces[i])->e, TRUE);
		if (BM_FACE_FIRST_LOOP(faces[i])->prev->e != e) {
			BM_elem_attrs_copy(bm, bm, BM_FACE_FIRST_LOOP(faces[i])->prev->e, e);
		}
		BLI_array_append(edges, e);
		
		f = BM_face_create_ngon(bm, verts[0], verts[1], edges, BLI_array_count(edges), FALSE);
		if (UNLIKELY(f == NULL)) {
			printf("%s: could not make face!\n", __func__);
			continue;
		}

		BMO_elem_flag_enable(bm, f, FACE_NEW);
	}

	for (i = 0; i < BLI_array_count(faces); i++) {
		BMLoop *l;
		BMIter liter;
		int j;
		
		/* create quad spans between split edge */
		BM_ITER_ELEM (l, &liter, faces[i], BM_LOOPS_OF_FACE) {
			BMVert *v1 = NULL, *v2 = NULL, *v3 = NULL, *v4 = NULL;
			
			if (!BMO_elem_flag_test(bm, l->e, BEVEL_FLAG))
				continue;
			
			v1 = tags[BM_elem_index_get(l)].newv;
			v2 = tags[BM_elem_index_get(l->next)].newv;
			if (l->radial_next != l) {
				v3 = tags[BM_elem_index_get(l->radial_next)].newv;
				if (l->radial_next->next->v == l->next->v) {
					v4 = v3;
					v3 = tags[BM_elem_index_get(l->radial_next->next)].newv;
				}
				else {
					v4 = tags[BM_elem_index_get(l->radial_next->next)].newv;
				}
			}
			else {
				/* the loop is on a boundar */
				v3 = l->next->v;
				v4 = l->v;
				
				for (j = 0; j < 2; j++) {
					BMIter eiter;
					BMVert *v = j ? v4 : v3;

					BM_ITER_ELEM (e, &eiter, v, BM_EDGES_OF_VERT) {
						if (!BM_vert_in_edge(e, v3) || !BM_vert_in_edge(e, v4))
							continue;
						
						if (!BMO_elem_flag_test(bm, e, BEVEL_FLAG) && BMO_elem_flag_test(bm, e, EDGE_OLD)) {
							BMVert *vv;
							
							vv = ETAG_GET(e, v);
							if (!vv || BMO_elem_flag_test(bm, vv, BEVEL_FLAG))
								continue;
							
							if (j) {
								v1 = vv;
							}
							else {
								v2 = vv;
							}
							break;
						}
					}
				}

				BMO_elem_flag_disable(bm, v3, BEVEL_DEL);
				BMO_elem_flag_disable(bm, v4, BEVEL_DEL);
			}
			
			if (v1 != v2 && v2 != v3 && v3 != v4) {
				BMIter liter2;
				BMLoop *l2, *l3;
				BMEdge *e1, *e2;
				float d1, d2, *d3;
				
				f = BM_face_create_quad_tri(bm, v4, v3, v2, v1, l->f, TRUE);

				e1 = BM_edge_exists(v4, v3);
				e2 = BM_edge_exists(v2, v1);
				BM_elem_attrs_copy(bm, bm, l->e, e1);
				BM_elem_attrs_copy(bm, bm, l->e, e2);
				
				/* set edge lengths of cross edges as the average of the cross edges they're based o */
				if (has_elens) {
					/* angle happens not to be used. why? - not sure it just isn't - campbell.
					 * leave this in in case we need to use it later */
#if 0
					float ang;
#endif
					e1 = BM_edge_exists(v1, v4);
					e2 = BM_edge_exists(v2, v3);
					
					if (l->radial_next->v == l->v) {
						l2 = l->radial_next->prev;
						l3 = l->radial_next->next;
					}
					else {
						l2 = l->radial_next->next;
						l3 = l->radial_next->prev;
					}
					
					d3 = CustomData_bmesh_get_n(&bm->edata, e1->head.data, CD_PROP_FLT, li);
					d1 = *(float *)CustomData_bmesh_get_n(&bm->edata, l->prev->e->head.data, CD_PROP_FLT, li);
					d2 = *(float *)CustomData_bmesh_get_n(&bm->edata, l2->e->head.data, CD_PROP_FLT, li);
#if 0
					ang = angle_v3v3v3(l->prev->v->co, l->v->co, BM_edge_other_vert(l2->e, l->v)->co);
#endif
					*d3 = (d1 + d2) * 0.5f;
					
					d3 = CustomData_bmesh_get_n(&bm->edata, e2->head.data, CD_PROP_FLT, li);
					d1 = *(float *)CustomData_bmesh_get_n(&bm->edata, l->next->e->head.data, CD_PROP_FLT, li);
					d2 = *(float *)CustomData_bmesh_get_n(&bm->edata, l3->e->head.data, CD_PROP_FLT, li);
#if 0
					ang = angle_v3v3v3(BM_edge_other_vert(l->next->e, l->next->v)->co, l->next->v->co,
					                   BM_edge_other_vert(l3->e, l->next->v)->co);
#endif
					*d3 = (d1 + d2) * 0.5f;
				}

				if (UNLIKELY(f == NULL)) {
					fprintf(stderr, "%s: face index out of range! (bmesh internal error)\n", __func__);
					continue;
				}
				
				BMO_elem_flag_enable(bm, f, FACE_NEW | FACE_SPAN);
				
				/* un-tag edges in f for deletio */
				BM_ITER_ELEM (l2, &liter2, f, BM_LOOPS_OF_FACE) {
					BMO_elem_flag_disable(bm, l2->e, BEVEL_DEL);
				}
			}
			else {
				f = NULL;
			}
		}
	}
	
	/* fill in holes at vertices */
	BM_ITER_MESH (v, &iter, bm, BM_VERTS_OF_MESH) {
		BMIter eiter;
		BMVert *vv, *vstart = NULL, *lastv = NULL;
		SmallHash tmphash;
		int rad, insorig = 0, err = 0;

		BLI_smallhash_init(&tmphash);

		if (!BMO_elem_flag_test(bm, v, BEVEL_FLAG))
			continue;
		
		BLI_array_empty(verts);
		BLI_array_empty(edges);
		
		BM_ITER_ELEM (e, &eiter, v, BM_EDGES_OF_VERT) {
			BMIter liter;
			BMVert *v1 = NULL, *v2 = NULL;
			BMLoop *l;
			
			if (BM_edge_face_count(e) < 2)
				insorig = 1;
			
			if (BM_elem_index_get(e) == -1)
				continue;
			
			rad = 0;
			BM_ITER_ELEM (l, &liter, e, BM_LOOPS_OF_EDGE) {
				if (!BMO_elem_flag_test(bm, l->f, FACE_OLD))
					continue;
				
				rad++;
				
				tag = tags + BM_elem_index_get((l->v == v) ? l : l->next);
				
				if (!v1)
					v1 = tag->newv;
				else if (!v2)
					v2 = tag->newv;
			}
			
			if (rad < 2)
				insorig = 1;
			
			if (!v1)
				v1 = ETAG_GET(e, v);
			if (!v2 || v1 == v2)
				v2 = ETAG_GET(e, v);
			
			if (v1) {
				if (!BLI_smallhash_haskey(&tmphash, (intptr_t)v1)) {
					BLI_array_append(verts, v1);
					BLI_smallhash_insert(&tmphash, (intptr_t)v1, NULL);
				}
				
				if (v2 && v1 != v2 && !BLI_smallhash_haskey(&tmphash, (intptr_t)v2)) {
					BLI_array_append(verts, v2);
					BLI_smallhash_insert(&tmphash, (intptr_t)v2, NULL);
				}
			}
		}
		
		if (!BLI_array_count(verts))
			continue;
		
		if (insorig) {
			BLI_array_append(verts, v);
			BLI_smallhash_insert(&tmphash, (intptr_t)v, NULL);
		}
		
		/* find edges that exist between vertices in verts.  this is basically
		 * a topological walk of the edges connecting them */
		vstart = vstart ? vstart : verts[0];
		vv = vstart;
		do {
			BM_ITER_ELEM (e, &eiter, vv, BM_EDGES_OF_VERT) {
				BMVert *vv2 = BM_edge_other_vert(e, vv);
				
				if (vv2 != lastv && BLI_smallhash_haskey(&tmphash, (intptr_t)vv2)) {
					/* if we've go over the same vert twice, break out of outer loop */
					if (BLI_smallhash_lookup(&tmphash, (intptr_t)vv2) != NULL) {
						e = NULL;
						err = 1;
						break;
					}
					
					/* use self pointer as ta */
					BLI_smallhash_remove(&tmphash, (intptr_t)vv2);
					BLI_smallhash_insert(&tmphash, (intptr_t)vv2, vv2);
					
					lastv = vv;
					BLI_array_append(edges, e);
					vv = vv2;
					break;
				}
			}

			if (e == NULL) {
				break;
			}
		} while (vv != vstart);
		
		if (err) {
			continue;
		}

		/* there may not be a complete loop of edges, so start again and make
		 * final edge afterwards.  in this case, the previous loop worked to
		 * find one of the two edges at the extremes. */
		if (vv != vstart) {
			/* undo previous taggin */
			for (i = 0; i < BLI_array_count(verts); i++) {
				BLI_smallhash_remove(&tmphash, (intptr_t)verts[i]);
				BLI_smallhash_insert(&tmphash, (intptr_t)verts[i], NULL);
			}

			vstart = vv;
			lastv = NULL;
			BLI_array_empty(edges);
			do {
				BM_ITER_ELEM (e, &eiter, vv, BM_EDGES_OF_VERT) {
					BMVert *vv2 = BM_edge_other_vert(e, vv);
					
					if (vv2 != lastv && BLI_smallhash_haskey(&tmphash, (intptr_t)vv2)) {
						/* if we've go over the same vert twice, break out of outer loo */
						if (BLI_smallhash_lookup(&tmphash, (intptr_t)vv2) != NULL) {
							e = NULL;
							err = 1;
							break;
						}
						
						/* use self pointer as ta */
						BLI_smallhash_remove(&tmphash, (intptr_t)vv2);
						BLI_smallhash_insert(&tmphash, (intptr_t)vv2, vv2);
						
						lastv = vv;
						BLI_array_append(edges, e);
						vv = vv2;
						break;
					}
				}
				if (e == NULL)
					break;
			} while (vv != vstart);
			
			if (!err) {
				e = BM_edge_create(bm, vv, vstart, NULL, TRUE);
				BLI_array_append(edges, e);
			}
		}
		
		if (err)
			continue;
		
		if (BLI_array_count(edges) >= 3) {
			BMFace *f;
			
			if (BM_face_exists(bm, verts, BLI_array_count(verts), &f))
				continue;
			
			f = BM_face_create_ngon(bm, lastv, vstart, edges, BLI_array_count(edges), FALSE);
			if (UNLIKELY(f == NULL)) {
				fprintf(stderr, "%s: in bevel vert fill! (bmesh internal error)\n", __func__);
			}
			else {
				BMO_elem_flag_enable(bm, f, FACE_NEW | FACE_HOLE);
			}
		}
		BLI_smallhash_release(&tmphash);
	}
	
	/* copy over customdat */
	for (i = 0; i < BLI_array_count(faces); i++) {
		BMLoop *l;
		BMIter liter;
		BMFace *f = faces[i];
		
		BM_ITER_ELEM (l, &liter, f, BM_LOOPS_OF_FACE) {
			BMLoop *l2;
			BMIter liter2;
			
			tag = tags + BM_elem_index_get(l);
			if (!tag->newv)
				continue;
			
			BM_ITER_ELEM (l2, &liter2, tag->newv, BM_LOOPS_OF_VERT) {
				if (!BMO_elem_flag_test(bm, l2->f, FACE_NEW) || (l2->v != tag->newv && l2->v != l->v))
					continue;
				
				if (tag->newv != l->v || HasMDisps) {
					BM_elem_attrs_copy(bm, bm, l->f, l2->f);
					BM_loop_interp_from_face(bm, l2, l->f, TRUE, TRUE);
				}
				else {
					BM_elem_attrs_copy(bm, bm, l->f, l2->f);
					BM_elem_attrs_copy(bm, bm, l, l2);
				}
				
				if (HasMDisps) {
					BMLoop *l3;
					BMIter liter3;
					
					BM_ITER_ELEM (l3, &liter3, l2->f, BM_LOOPS_OF_FACE) {
						BM_loop_interp_multires(bm, l3, l->f);
					}
				}
			}
		}
	}
	
	/* handle vertices along boundary edge */
	BM_ITER_MESH (v, &iter, bm, BM_VERTS_OF_MESH) {
		if (BMO_elem_flag_test(bm, v, VERT_OLD) &&
		    BMO_elem_flag_test(bm, v, BEVEL_FLAG) &&
		    !BMO_elem_flag_test(bm, v, BEVEL_DEL))
		{
			BMLoop *l;
			BMLoop *lorig = NULL;
			BMIter liter;
			
			BM_ITER_ELEM (l, &liter, v, BM_LOOPS_OF_VERT) {
				// BMIter liter2;
				// BMLoop *l2 = l->v == v ? l : l->next, *l3;
				
				if (BMO_elem_flag_test(bm, l->f, FACE_OLD)) {
					lorig = l;
					break;
				}
			}
			
			if (!lorig)
				continue;
			
			BM_ITER_ELEM (l, &liter, v, BM_LOOPS_OF_VERT) {
				BMLoop *l2 = l->v == v ? l : l->next;
				
				BM_elem_attrs_copy(bm, bm, lorig->f, l2->f);
				BM_elem_attrs_copy(bm, bm, lorig, l2);
			}
		}
	}
#if 0
	/* clean up any remaining 2-edged face */
	BM_ITER_MESH (f, &iter, bm, BM_FACES_OF_MESH) {
		if (f->len == 2) {
			BMFace *faces[2] = {f, BM_FACE_FIRST_LOOP(f)->radial_next->f};
			
			if (faces[0] == faces[1])
				BM_face_kill(bm, f);
			else
				BM_faces_join(bm, faces, 2);
		}
	}
#endif

	BMO_op_callf(bm, op->flag, "delete geom=%fv context=%i", BEVEL_DEL, DEL_VERTS);

	/* clean up any edges that might not get properly delete */
	BM_ITER_MESH (e, &iter, bm, BM_EDGES_OF_MESH) {
		if (BMO_elem_flag_test(bm, e, EDGE_OLD) && !e->l)
			BMO_elem_flag_enable(bm, e, BEVEL_DEL);
	}

	BMO_op_callf(bm, op->flag, "delete geom=%fe context=%i", BEVEL_DEL, DEL_EDGES);
	BMO_op_callf(bm, op->flag, "delete geom=%ff context=%i", BEVEL_DEL, DEL_FACES);
	
	BLI_smallhash_release(&hash);
	BLI_array_free(tags);
	BLI_array_free(etags);
	BLI_array_free(verts);
	BLI_array_free(edges);
	BLI_array_free(faces);
	
	BMO_slot_buffer_from_enabled_flag(bm, op, "face_spans", BM_FACE, FACE_SPAN);
	BMO_slot_buffer_from_enabled_flag(bm, op, "face_holes", BM_FACE, FACE_HOLE);
}
#endif  /* NEW_BEVEL */
