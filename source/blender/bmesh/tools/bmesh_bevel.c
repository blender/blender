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
 * Contributor(s):
 *         Joseph Eagar,
 *         Aleksandr Mokhov,
 *         Howard Trickey,
 *         Campbell Barton
 *
 * ***** END GPL LICENSE BLOCK *****
 */

/** \file blender/bmesh/tools/bmesh_bevel.c
 *  \ingroup bmesh
 *
 * Main functions for beveling a BMesh (used by the tool and modifier)
 */

#include "MEM_guardedalloc.h"

#include "DNA_object_types.h"
#include "DNA_meshdata_types.h"

#include "BLI_array.h"
#include "BLI_alloca.h"
#include "BLI_gsqueue.h"
#include "BLI_math.h"
#include "BLI_memarena.h"

#include "BKE_customdata.h"
#include "BKE_deform.h"

#include "bmesh.h"
#include "bmesh_bevel.h"  /* own include */

#include "./intern/bmesh_private.h"

#define BEVEL_EPSILON_D  1e-6
#define BEVEL_EPSILON    1e-6f
#define BEVEL_EPSILON_SQ 1e-12f
#define BEVEL_EPSILON_BIG 1e-4f
#define BEVEL_EPSILON_BIG_SQ 1e-8f
#define BEVEL_EPSILON_ANG DEG2RADF(2.0f)
#define BEVEL_SMALL_ANG DEG2RADF(10.0f)
#define BEVEL_MAX_ADJUST_PCT 10.0f
#define BEVEL_MAX_AUTO_ADJUST_PCT 300.0f

/* happens far too often, uncomment for development */
// #define BEVEL_ASSERT_PROJECT

/* for testing */
// #pragma GCC diagnostic error "-Wpadded"

/* Constructed vertex, sometimes later instantiated as BMVert */
typedef struct NewVert {
	BMVert *v;
	float co[3];
//	int _pad;
} NewVert;

struct BoundVert;

/* Data for one end of an edge involved in a bevel */
typedef struct EdgeHalf {
	struct EdgeHalf *next, *prev;   /* in CCW order */
	BMEdge *e;                  /* original mesh edge */
	BMFace *fprev;              /* face between this edge and previous, if any */
	BMFace *fnext;              /* face between this edge and next, if any */
	struct BoundVert *leftv;    /* left boundary vert (looking along edge to end) */
	struct BoundVert *rightv;   /* right boundary vert, if beveled */
	int   seg;                  /* how many segments for the bevel */
	float offset_l;             /* offset for this edge, on left side */
	float offset_r;             /* offset for this edge, on right side */
	float offset_l_spec;        /* user specification for offset_l */
	float offset_r_spec;        /* user specification for offset_r */
	bool is_bev;                /* is this edge beveled? */
	bool is_rev;                /* is e->v2 the vertex at this end? */
	bool is_seam;               /* is e a seam for custom loopdata (e.g., UVs)? */
//	int _pad;
} EdgeHalf;

/* Profile specification.
 * Many interesting profiles are in family of superellipses:
 *     (abs(x/a))^r + abs(y/b))^r = 1
 * r==2 => ellipse; r==1 => line; r < 1 => concave; r > 1 => bulging out.
 * Special cases: let r==0 mean straight-inward, and r==4 mean straight outward.
 * The profile is an arc with control points coa, midco,
 * projected onto a plane (plane_no is normal, plane_co is a point on it)
 * via lines in a given direction (proj_dir).
 * After the parameters are all set, the actual profile points are calculated
 * and point in prof_co. We also may need profile points for a higher resolution
 * number of segments, in order to make the vertex mesh pattern, and that goes
 * in prof_co_2.
 */
typedef struct Profile {
	float super_r;       /* superellipse r parameter */
	float coa[3];        /* start control point for profile */
	float midco[3];      /* mid control point for profile */
	float cob[3];        /* end control point for profile */
	float plane_no[3];   /* normal of plane to project to */
	float plane_co[3];   /* coordinate on plane to project to */
	float proj_dir[3];   /* direction of projection line */
	float *prof_co;      /* seg+1 profile coordinates (triples of floats) */
	float *prof_co_2;    /* like prof_co, but for seg power of 2 >= seg */
} Profile;
#define PRO_SQUARE_R 4.0f
#define PRO_CIRCLE_R 2.0f
#define PRO_LINE_R 1.0f
#define PRO_SQUARE_IN_R 0.0f

/* Cache result of expensive calculation of u parameter values to
 * get even spacing on superellipse for current BevelParams seg
 * and pro_super_r. */
typedef struct ProfileSpacing {
	float *uvals;       /* seg+1 u values */
	float *uvals_2;     /* seg_2+1 u values, seg_2 = power of 2 >= seg */
	int seg_2;          /* the seg_2 value */
} ProfileSpacing;

/* An element in a cyclic boundary of a Vertex Mesh (VMesh) */
typedef struct BoundVert {
	struct BoundVert *next, *prev;  /* in CCW order */
	NewVert nv;
	EdgeHalf *efirst;   /* first of edges attached here: in CCW order */
	EdgeHalf *elast;
	EdgeHalf *ebev;     /* beveled edge whose left side is attached here, if any */
	int index;          /* used for vmesh indexing */
	Profile profile;    /* edge profile between this and next BoundVert */
	bool any_seam;      /* are any of the edges attached here seams? */
//	int _pad;
} BoundVert;	

/* Mesh structure replacing a vertex */
typedef struct VMesh {
	NewVert *mesh;           /* allocated array - size and structure depends on kind */
	BoundVert *boundstart;   /* start of boundary double-linked list */
	int count;               /* number of vertices in the boundary */
	int seg;                 /* common # of segments for segmented edges */
	enum {
		M_NONE,         /* no polygon mesh needed */
		M_POLY,         /* a simple polygon */
		M_ADJ,          /* "adjacent edges" mesh pattern */
		M_TRI_FAN,      /* a simple polygon - fan filled */
		M_QUAD_STRIP,   /* a simple polygon - cut into parallel strips */
	} mesh_kind;
//	int _pad;
} VMesh;

/* Data for a vertex involved in a bevel */
typedef struct BevVert {
	BMVert *v;          /* original mesh vertex */
	int edgecount;          /* total number of edges around the vertex (excluding wire edges if edge beveling) */
	int selcount;           /* number of selected edges around the vertex */
	int wirecount;			/* count of wire edges */
	float offset;           /* offset for this vertex, if vertex_only bevel */
	bool any_seam;			/* any seams on attached edges? */
	bool visited;           /* used in graph traversal */
	EdgeHalf *edges;        /* array of size edgecount; CCW order from vertex normal side */
	BMEdge **wire_edges;	/* array of size wirecount of wire edges */
	VMesh *vmesh;           /* mesh structure for replacing vertex */
} BevVert;

/* Bevel parameters and state */
typedef struct BevelParams {
	/* hash of BevVert for each vertex involved in bevel
	 * GHash: (key=(BMVert *), value=(BevVert *)) */
	GHash    *vert_hash;
	MemArena *mem_arena;    /* use for all allocs while bevel runs, if we need to free we can switch to mempool */
	ProfileSpacing pro_spacing; /* parameter values for evenly spaced profiles */

	float offset;           /* blender units to offset each side of a beveled edge */
	int offset_type;        /* how offset is measured; enum defined in bmesh_operators.h */
	int seg;                /* number of segments in beveled edge profile */
	float pro_super_r;      /* superellipse parameter for edge profile */
	bool vertex_only;       /* bevel vertices only */
	bool use_weights;       /* bevel amount affected by weights on edges or verts */
	bool loop_slide;	    /* should bevel prefer to slide along edges rather than keep widths spec? */
	bool limit_offset;      /* should offsets be limited by collisions? */
	const struct MDeformVert *dvert; /* vertex group array, maybe set if vertex_only */
	int vertex_group;       /* vertex group index, maybe set if vertex_only */
	int mat_nr;             /* if >= 0, material number for bevel; else material comes from adjacent faces */
} BevelParams;

// #pragma GCC diagnostic ignored "-Wpadded"

// #include "bevdebug.c"

/* some flags to re-enable old behavior for a while, in case fixes broke things not caught by regression tests */
static int bev_debug_flags = 0;
#define DEBUG_OLD_PLANE_SPECIAL (bev_debug_flags & 1)
#define DEBUG_OLD_PROJ_TO_PERP_PLANE (bev_debug_flags & 2)
#define DEBUG_OLD_FLAT_MID (bev_debug_flags & 4)

/* this flag values will get set on geom we want to return in 'out' slots for edges and verts */
#define EDGE_OUT 4
#define VERT_OUT 8

/* If we're called from the modifier, tool flags aren't available, but don't need output geometry */
static void flag_out_edge(BMesh *bm, BMEdge *bme)
{
	if (bm->use_toolflags)
		BMO_edge_flag_enable(bm, bme, EDGE_OUT);
}

static void flag_out_vert(BMesh *bm, BMVert *bmv)
{
	if (bm->use_toolflags)
		BMO_vert_flag_enable(bm, bmv, VERT_OUT);
}

static void disable_flag_out_edge(BMesh *bm, BMEdge *bme)
{
	if (bm->use_toolflags)
		BMO_edge_flag_disable(bm, bme, EDGE_OUT);
}

/* Are d1 and d2 parallel or nearly so? */
static bool nearly_parallel(const float d1[3], const float d2[3])
{
	float ang;

	ang = angle_v3v3(d1, d2);
	return (fabsf(ang) < BEVEL_EPSILON_ANG) || (fabsf(ang - (float)M_PI) < BEVEL_EPSILON_ANG);
}

/* Make a new BoundVert of the given kind, insert it at the end of the circular linked
 * list with entry point bv->boundstart, and return it. */
static BoundVert *add_new_bound_vert(MemArena *mem_arena, VMesh *vm, const float co[3])
{
	BoundVert *ans = (BoundVert *)BLI_memarena_alloc(mem_arena, sizeof(BoundVert));

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
	ans->profile.super_r = PRO_LINE_R;
	vm->count++;
	return ans;
}

BLI_INLINE void adjust_bound_vert(BoundVert *bv, const float co[3])
{
	copy_v3_v3(bv->nv.co, co);
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
	nv->v = BM_vert_create(bm, nv->co, eg, BM_CREATE_NOP);
	BM_elem_flag_disable(nv->v, BM_ELEM_TAG);
	flag_out_vert(bm, nv->v);
}

static void copy_mesh_vert(
        VMesh *vm, int ito, int jto, int kto,
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

/* find the BevVert corresponding to BMVert bmv */
static BevVert *find_bevvert(BevelParams *bp, BMVert *bmv)
{
	return BLI_ghash_lookup(bp->vert_hash, bmv);
}

/* Find the EdgeHalf representing the other end of e->e.
 * Return other end's BevVert in *bvother, if r_bvother is provided.
 * That may not have been constructed yet, in which case return NULL. */
static EdgeHalf *find_other_end_edge_half(BevelParams *bp, EdgeHalf *e, BevVert **r_bvother)
{
	BevVert *bvo;
	EdgeHalf *eother;

	bvo = find_bevvert(bp, e->is_rev ? e->e->v1 : e->e->v2);
	if (bvo) {
		if (r_bvother)
			*r_bvother = bvo;
		eother = find_edge_half(bvo, e->e);
		BLI_assert(eother != NULL);
		return eother;
	}
	else if (r_bvother) {
		*r_bvother = NULL;
	}
	return NULL;
}

static bool other_edge_half_visited(BevelParams *bp, EdgeHalf *e)
{
	BevVert *bvo;

	bvo = find_bevvert(bp, e->is_rev ? e->e->v1 : e->e->v2);
	if (bvo)
		return bvo->visited;
	else
		return false;
}

static bool edge_half_offset_changed(EdgeHalf *e)
{
	return e->offset_l != e->offset_l_spec ||
	       e->offset_r != e->offset_r_spec;
}

static float adjusted_rel_change(float val, float spec)
{
	float relchg;

	relchg = 0.0f;
	if (val != spec) {
		if (spec == 0)
			relchg = 1000.0f;  /* arbitrary large value */
		else
			relchg = fabsf((val - spec) / spec);
	}
	return relchg;
}

static float max_edge_half_offset_rel_change(BevVert *bv)
{
	int i;
	float max_rel_change;
	EdgeHalf *e;

	max_rel_change = 0.0f;
	for (i = 0; i < bv->edgecount; i++) {
		e = &bv->edges[i];
		max_rel_change = max_ff(max_rel_change, adjusted_rel_change(e->offset_l, e->offset_l_spec));
		max_rel_change = max_ff(max_rel_change, adjusted_rel_change(e->offset_r, e->offset_r_spec));
	}
	return max_rel_change;
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
		if (e->is_bev) {
			return e;
		}
	} while ((e = e->next) != from_e);
	return NULL;
}

/* return count of edges between e1 and e2 when going around bv CCW */
static int count_ccw_edges_between(EdgeHalf *e1, EdgeHalf *e2)
{
	int cnt = 0;
	EdgeHalf *e = e1;

	do {
		if (e == e2)
			break;
		e = e->next;
		cnt++;
	} while (e != e1);
	return cnt;
}

/* Assume bme1 and bme2 both share some vert. Do they share a face?
 * If they share a face then there is some loop around bme1 that is in a face
 * where the next or previous edge in the face must be bme2. */
static bool edges_face_connected_at_vert(BMEdge *bme1, BMEdge *bme2)
{
	BMLoop *l;
	BMIter iter;

	BM_ITER_ELEM(l, &iter, bme1, BM_LOOPS_OF_EDGE) {
		if (l->prev->e == bme2 || l->next->e == bme2)
			return true;
	}
	return false;
}

/* Return a good representative face (for materials, etc.) for faces
 * created around/near BoundVert v.
 * Sometimes care about a second choice, if there is one.
 * If r_fother parameter is non-NULL and there is another, different,
 * possible frep, return the other one in that parameter. */
static BMFace *boundvert_rep_face(BoundVert *v, BMFace **r_fother)
{
	BMFace *frep, *frep2;

	frep2 = NULL;
	if (v->ebev) {
		frep = v->ebev->fprev;
		if (v->efirst->fprev != frep)
			frep2 = v->efirst->fprev;
	}
	else {
		frep = v->efirst->fprev;
		if (frep) {
			if (v->elast->fnext != frep)
				frep2 = v->elast->fnext;
			else if (v->efirst->fnext != frep)
				frep2 = v->efirst->fnext;
			else if (v->elast->fprev != frep)
				frep2 = v->efirst->fprev;
		}
		else if (v->efirst->fnext) {
			frep = v->efirst->fnext;
			if (v->elast->fnext != frep)
				frep2 = v->elast->fnext;
		}
		else if (v->elast->fprev) {
			frep = v->elast->fprev;
		}
	}
	if (r_fother)
		*r_fother = frep2;
	return frep;
}

/**
 * Make ngon from verts alone.
 * Make sure to properly copy face attributes and do custom data interpolation from
 * corresponding elements of face_arr, if that is non-NULL, else from facerep.
 * If edge_arr is non-NULL, then for interpolation purposes only, the corresponding
 * elements of vert_arr are snapped to any non-NULL edges in that array.
 * If mat_nr >= 0 then the material of the face is set to that.
 *
 * \note ALL face creation goes through this function, this is important to keep!
 */
static BMFace *bev_create_ngon(
        BMesh *bm, BMVert **vert_arr, const int totv,
        BMFace **face_arr, BMFace *facerep, BMEdge **edge_arr,
        int mat_nr, bool do_interp)
{
	BMIter iter;
	BMLoop *l;
	BMFace *f, *interp_f;
	BMEdge *bme;
	float save_co[3];
	int i;

	f = BM_face_create_verts(bm, vert_arr, totv, facerep, BM_CREATE_NOP, true);

	if ((facerep || (face_arr && face_arr[0])) && f) {
		BM_elem_attrs_copy(bm, bm, facerep ? facerep : face_arr[0], f);
		if (do_interp) {
			i = 0;
			BM_ITER_ELEM (l, &iter, f, BM_LOOPS_OF_FACE) {
				if (face_arr) {
					/* assume loops of created face are in same order as verts */
					BLI_assert(l->v == vert_arr[i]);
					interp_f = face_arr[i];
				}
				else {
					interp_f = facerep;
				}
				if (interp_f) {
					bme = NULL;
					if (edge_arr)
						bme = edge_arr[i];
					if (bme) {
						copy_v3_v3(save_co, l->v->co);
						closest_to_line_segment_v3(l->v->co, save_co, bme->v1->co, bme->v2->co);
					}
					BM_loop_interp_from_face(bm, l, interp_f, true, true);
					if (bme) {
						copy_v3_v3(l->v->co, save_co);
					}
				}
				i++;
			}
		}
	}

	/* not essential for bevels own internal logic,
	 * this is done so the operator can select newly created geometry */
	if (f) {
		BM_elem_flag_enable(f, BM_ELEM_TAG);
		BM_ITER_ELEM(bme, &iter, f, BM_EDGES_OF_FACE) {
			flag_out_edge(bm, bme);
		}
	}

	if (mat_nr >= 0)
		f->mat_nr = mat_nr;
	return f;
}

static BMFace *bev_create_quad(
        BMesh *bm, BMVert *v1, BMVert *v2, BMVert *v3, BMVert *v4,
        BMFace *f1, BMFace *f2, BMFace *f3, BMFace *f4, 
        int mat_nr)
{
	BMVert *varr[4] = {v1, v2, v3, v4};
	BMFace *farr[4] = {f1, f2, f3, f4};
	return bev_create_ngon(bm, varr, 4, farr, f1, NULL, mat_nr, true);
}

static BMFace *bev_create_quad_ex(
        BMesh *bm, BMVert *v1, BMVert *v2, BMVert *v3, BMVert *v4,
        BMFace *f1, BMFace *f2, BMFace *f3, BMFace *f4, 
        BMEdge *e1, BMEdge *e2, BMEdge *e3, BMEdge *e4,
        int mat_nr)
{
	BMVert *varr[4] = {v1, v2, v3, v4};
	BMFace *farr[4] = {f1, f2, f3, f4};
	BMEdge *earr[4] = {e1, e2, e3, e4};
	return bev_create_ngon(bm, varr, 4, farr, f1, earr, mat_nr, true);
}

/* Is Loop layer layer_index contiguous across shared vertex of l1 and l2? */
static bool contig_ldata_across_loops(
        BMesh *bm, BMLoop *l1, BMLoop *l2,
        int layer_index)
{
	const int offset = bm->ldata.layers[layer_index].offset;
	const int type = bm->ldata.layers[layer_index].type;

	return CustomData_data_equals(type,
	                              (char *)l1->head.data + offset,
	                              (char *)l2->head.data + offset);
}

/* Are all loop layers with have math (e.g., UVs) contiguous from face f1 to face f2 across edge e? */
static bool contig_ldata_across_edge(BMesh *bm, BMEdge *e, BMFace *f1, BMFace *f2)
{
	BMLoop *lef1, *lef2;
	BMLoop *lv1f1, *lv1f2, *lv2f1, *lv2f2;
	BMVert *v1, *v2;
	int i;

	if (bm->ldata.totlayer == 0)
		return true;

	v1 = e->v1;
	v2 = e->v2;
	if (!BM_edge_loop_pair(e, &lef1, &lef2))
		return false;
	if (lef1->f == f2) {
		SWAP(BMLoop *, lef1, lef2);
	}

	if (lef1->v == v1) {
		lv1f1 = lef1;
		lv2f1 = BM_face_other_edge_loop(f1, e, v2);
	}
	else {
		lv2f1 = lef1;
		lv1f1 = BM_face_other_edge_loop(f1, e, v1);
	}

	if (lef2->v == v1) {
		lv1f2 = lef2;
		lv2f2 = BM_face_other_edge_loop(f2, e, v2);
	}
	else {
		lv2f2 = lef2;
		lv1f2 = BM_face_other_edge_loop(f2, e, v1);
	}

	for (i = 0; i < bm->ldata.totlayer; i++) {
		if (CustomData_layer_has_math(&bm->ldata, i) &&
		    (!contig_ldata_across_loops(bm, lv1f1, lv1f2, i) ||
		     !contig_ldata_across_loops(bm, lv2f1, lv2f2, i)))
		{
			return false;
		}
	}
	return true;
}

/* Merge (using average) all the UV values for loops of v's faces.
 * Caller should ensure that no seams are violated by doing this. */
static void bev_merge_uvs(BMesh *bm, BMVert *v)
{
	BMIter iter;
	MLoopUV *luv;
	BMLoop *l;
	float uv[2];
	int n;
	int num_of_uv_layers = CustomData_number_of_layers(&bm->ldata, CD_MLOOPUV);
	int i;

	for (i = 0; i < num_of_uv_layers; i++) {
		int cd_loop_uv_offset = CustomData_get_n_offset(&bm->ldata, CD_MLOOPUV, i);

		if (cd_loop_uv_offset == -1)
			return;

		n = 0;
		zero_v2(uv);
		BM_ITER_ELEM (l, &iter, v, BM_LOOPS_OF_VERT) {
			luv = BM_ELEM_CD_GET_VOID_P(l, cd_loop_uv_offset);
			add_v2_v2(uv, luv->uv);
			n++;
		}
		if (n > 1) {
			mul_v2_fl(uv, 1.0f / (float)n);
			BM_ITER_ELEM (l, &iter, v, BM_LOOPS_OF_VERT) {
				luv = BM_ELEM_CD_GET_VOID_P(l, cd_loop_uv_offset);
				copy_v2_v2(luv->uv, uv);
			}
		}
	}
}

/* Merge (using average) the UV values for two specific loops of v: those for faces containing v,
 * and part of faces that share edge bme */
static void bev_merge_edge_uvs(BMesh *bm, BMEdge *bme, BMVert *v)
{
	BMIter iter;
	MLoopUV *luv;
	BMLoop *l, *l1, *l2;
	float uv[2];
	int num_of_uv_layers = CustomData_number_of_layers(&bm->ldata, CD_MLOOPUV);
	int i;

	l1 = NULL;
	l2 = NULL;
	BM_ITER_ELEM (l, &iter, v, BM_LOOPS_OF_VERT) {
		if (l->e == bme)
			l1 = l;
		else if (l->prev->e == bme)
			l2 = l;
	}
	if (l1 == NULL || l2 == NULL)
		return;

	for (i = 0; i < num_of_uv_layers; i++) {
		int cd_loop_uv_offset = CustomData_get_n_offset(&bm->ldata, CD_MLOOPUV, i);

		if (cd_loop_uv_offset == -1)
			return;

		zero_v2(uv);
		luv = BM_ELEM_CD_GET_VOID_P(l1, cd_loop_uv_offset);
		add_v2_v2(uv, luv->uv);
		luv = BM_ELEM_CD_GET_VOID_P(l2, cd_loop_uv_offset);
		add_v2_v2(uv, luv->uv);
		mul_v2_fl(uv, 0.5f);
		luv = BM_ELEM_CD_GET_VOID_P(l1, cd_loop_uv_offset);
		copy_v2_v2(luv->uv, uv);
		luv = BM_ELEM_CD_GET_VOID_P(l2, cd_loop_uv_offset);
		copy_v2_v2(luv->uv, uv);
	}
}

/* Calculate coordinates of a point a distance d from v on e->e and return it in slideco */
static void slide_dist(EdgeHalf *e, BMVert *v, float d, float slideco[3])
{
	float dir[3], len;

	sub_v3_v3v3(dir, v->co, BM_edge_other_vert(e->e, v)->co);
	len = normalize_v3(dir);
	if (d > len)
		d = len - (float)(50.0 * BEVEL_EPSILON_D);
	copy_v3_v3(slideco, v->co);
	madd_v3_v3fl(slideco, dir, -d);
}

/* Is co not on the edge e? if not, return the closer end of e in ret_closer_v */
static bool is_outside_edge(EdgeHalf *e, const float co[3], BMVert **ret_closer_v)
{
	float d_squared;

	d_squared = dist_squared_to_line_segment_v3(co, e->e->v1->co, e->e->v2->co);
	if (d_squared > BEVEL_EPSILON_BIG * BEVEL_EPSILON_BIG) {
		if (len_squared_v3v3(co, e->e->v1->co) > len_squared_v3v3(co, e->e->v2->co))
			*ret_closer_v = e->e->v2;
		else
			*ret_closer_v = e->e->v1;
		return true;
	}
	else {
		return false;
	}
}

/* co should be approximately on the plane between e1 and e2, which share common vert v
 * and common face f (which cannot be NULL).
 * Is it between those edges, sweeping CCW? */
static bool point_between_edges(float co[3], BMVert *v, BMFace *f, EdgeHalf *e1, EdgeHalf *e2)
{
	BMVert *v1, *v2;
	float dir1[3], dir2[3], dirco[3], no[3];
	float ang11, ang1co;

	v1 = BM_edge_other_vert(e1->e, v);
	v2 = BM_edge_other_vert(e2->e, v);
	sub_v3_v3v3(dir1, v->co, v1->co);
	sub_v3_v3v3(dir2, v->co, v2->co);
	sub_v3_v3v3(dirco, v->co, co);
	normalize_v3(dir1);
	normalize_v3(dir2);
	normalize_v3(dirco);
	ang11 = angle_normalized_v3v3(dir1, dir2);
	ang1co = angle_normalized_v3v3(dir1, dirco);
	/* angles are in [0,pi]. need to compare cross product with normal to see if they are reflex */
	cross_v3_v3v3(no, dir1, dir2);
	if (dot_v3v3(no, f->no) < 0.0f)
		ang11 = (float)(M_PI * 2.0) - ang11;
	cross_v3_v3v3(no, dir1, dirco);
	if (dot_v3v3(no, f->no) < 0.0f)
		ang1co = (float)(M_PI * 2.0) - ang1co;
	return (ang11 - ang1co > -BEVEL_EPSILON_ANG);
}

/*
 * Calculate the meeting point between the offset edges for e1 and e2, putting answer in meetco.
 * e1 and e2 share vertex v and face f (may be NULL) and viewed from the normal side of
 * the bevel vertex,  e1 precedes e2 in CCW order.
 * Except: if edges_between is true, there are edges between e1 and e2 in CCW order so they
 * don't share a common face. We want the meeting point to be on an existing face so it
 * should be dropped onto one of the intermediate faces, if possible.
 * Offset edge is on right of both edges, where e1 enters v and e2 leave it.
 * When offsets are equal, the new point is on the edge bisector, with length offset/sin(angle/2),
 * but if the offsets are not equal (allowing for this, as bevel modifier has edge weights that may
 * lead to different offsets) then meeting point can be found be intersecting offset lines.
 * If making the meeting point significantly changes the left or right offset from the user spec,
 * record the change in offset_l (or offset_r); later we can tell that a change has happened because
 * the offset will differ from its original value in offset_l_spec (or offset_r_spec).
 */
static void offset_meet(EdgeHalf *e1, EdgeHalf *e2, BMVert *v, BMFace *f, bool edges_between, float meetco[3])
{
	float dir1[3], dir2[3], dir1n[3], dir2p[3], norm_v[3], norm_v1[3], norm_v2[3],
		norm_perp1[3], norm_perp2[3], off1a[3], off1b[3], off2a[3], off2b[3],
		isect2[3], dropco[3], plane[4], ang, d;
	BMVert *closer_v;
	EdgeHalf *e, *e1next, *e2prev;
	BMFace *ff;
	int isect_kind;

	/* get direction vectors for two offset lines */
	sub_v3_v3v3(dir1, v->co, BM_edge_other_vert(e1->e, v)->co);
	sub_v3_v3v3(dir2, BM_edge_other_vert(e2->e, v)->co, v->co);

	if (edges_between) {
		e1next = e1->next;
		e2prev = e2->prev;
		sub_v3_v3v3(dir1n, BM_edge_other_vert(e1next->e, v)->co, v->co);
		sub_v3_v3v3(dir2p, v->co, BM_edge_other_vert(e2prev->e, v)->co);
	}

	ang = angle_v3v3(dir1, dir2);
	if (ang < BEVEL_EPSILON_ANG) {
		/* special case: e1 and e2 are parallel; put offset point perp to both, from v.
		 * need to find a suitable plane.
		 * if offsets are different, we're out of luck:
		 * use the max of the two (so get consistent looking results if the same situation
		 * arises elsewhere in the object but with opposite roles for e1 and e2 */
		if (f)
			copy_v3_v3(norm_v, f->no);
		else
			copy_v3_v3(norm_v, v->no);
		cross_v3_v3v3(norm_perp1, dir1, norm_v);
		normalize_v3(norm_perp1);
		copy_v3_v3(off1a, v->co);
		d = max_ff(e1->offset_r, e2->offset_l);
		madd_v3_v3fl(off1a, norm_perp1, d);
		if (e1->offset_r != d)
			e1->offset_r = d;
		else if (e2->offset_l != d)
			e2->offset_l = d;
		copy_v3_v3(meetco, off1a);
	}
	else if (fabsf(ang - (float)M_PI) < BEVEL_EPSILON_ANG) {
		/* special case e1 and e2 are antiparallel, so bevel is into
		 * a zero-area face.  Just make the offset point on the
		 * common line, at offset distance from v. */
		d = max_ff(e1->offset_r, e2->offset_l);
		slide_dist(e2, v, d, meetco);
		if (e1->offset_r != d)
			e1->offset_r = d;
		else if (e2->offset_l != d)
			e2->offset_l = d;
	}
	else {
		/* Get normal to plane where meet point should be,
		 * using cross product instead of f->no in case f is non-planar.
		 * Except: sometimes locally there can be a small angle
		 * between dir1 and dir2 that leads to a normal that is actually almost
		 * perpendicular to the face normal; in this case it looks wrong to use
		 * the local (cross-product) normal, so use the face normal if the angle
		 * between dir1 and dir2 is smallish.
		 * If e1-v-e2 is a reflex angle (viewed from vertex normal side), need to flip.
		 * Use f->no to figure out which side to look at angle from, as even if
		 * f is non-planar, will be more accurate than vertex normal */
		if (f && ang < BEVEL_SMALL_ANG) {
			copy_v3_v3(norm_v1, f->no);
			copy_v3_v3(norm_v2, f->no);
		}
		else if (!edges_between) {
			cross_v3_v3v3(norm_v1, dir2, dir1);
			normalize_v3(norm_v1);
			if (dot_v3v3(norm_v1, f ? f->no : v->no) < 0.0f)
				negate_v3(norm_v1);
			copy_v3_v3(norm_v2, norm_v1);
		}
		else {
			/* separate faces; get face norms at corners for each separately */
			cross_v3_v3v3(norm_v1, dir1n, dir1);
			normalize_v3(norm_v1);
			f = e1->fnext;
			if (dot_v3v3(norm_v1, f ? f->no : v->no) < 0.0f)
				negate_v3(norm_v1);
			cross_v3_v3v3(norm_v2, dir2, dir2p);
			normalize_v3(norm_v2);
			f = e2->fprev;
			if (dot_v3v3(norm_v2, f ? f->no : v->no) < 0.0f)
				negate_v3(norm_v2);
		}


		/* get vectors perp to each edge, perp to norm_v, and pointing into face */
		cross_v3_v3v3(norm_perp1, dir1, norm_v1);
		cross_v3_v3v3(norm_perp2, dir2, norm_v2);
		normalize_v3(norm_perp1);
		normalize_v3(norm_perp2);

		/* get points that are offset distances from each line, then another point on each line */
		copy_v3_v3(off1a, v->co);
		madd_v3_v3fl(off1a, norm_perp1, e1->offset_r);
		add_v3_v3v3(off1b, off1a, dir1);
		copy_v3_v3(off2a, v->co);
		madd_v3_v3fl(off2a, norm_perp2, e2->offset_l);
		add_v3_v3v3(off2b, off2a, dir2);

		/* intersect the lines */
		isect_kind = isect_line_line_v3(off1a, off1b, off2a, off2b, meetco, isect2);
		if (isect_kind == 0) {
			/* lines are collinear: we already tested for this, but this used a different epsilon */
			copy_v3_v3(meetco, off1a);  /* just to do something */
			d = dist_to_line_v3(meetco, v->co, BM_edge_other_vert(e2->e, v)->co);
			if (fabsf(d - e2->offset_l) > BEVEL_EPSILON)
				e2->offset_l = d;
		}
		else {
			/* The lines intersect, but is it at a reasonable place?
			 * One problem to check: if one of the offsets is 0, then don't
			 * want an intersection that is outside that edge itself.
			 * This can happen if angle between them is > 180 degrees,
			 * or if the offset amount is > the edge length*/
			if (e1->offset_r == 0.0f && is_outside_edge(e1, meetco, &closer_v)) {
				copy_v3_v3(meetco, closer_v->co);
				e2->offset_l = len_v3v3(meetco, v->co);
			}
			if (e2->offset_l == 0.0f && is_outside_edge(e2, meetco, &closer_v)) {
				copy_v3_v3(meetco, closer_v->co);
				e1->offset_r = len_v3v3(meetco, v->co);
			}
			if (edges_between && e1->offset_r > 0.0f && e2->offset_l > 0.0f) {
				/* Try to drop meetco to a face between e1 and e2 */
				if (isect_kind == 2) {
					/* lines didn't meet in 3d: get average of meetco and isect2 */
					mid_v3_v3v3(meetco, meetco, isect2);
				}
				for (e = e1; e != e2; e = e->next) {
					ff = e->fnext;
					if (!ff)
						continue;
					plane_from_point_normal_v3(plane, v->co, ff->no);
					closest_to_plane_normalized_v3(dropco, plane, meetco);
					if (point_between_edges(dropco, v, ff, e, e->next)) {
						copy_v3_v3(meetco, dropco);
						break;
					}
				}
				e1->offset_r = dist_to_line_v3(meetco, v->co, BM_edge_other_vert(e1->e, v)->co);
				e2->offset_l = dist_to_line_v3(meetco, v->co, BM_edge_other_vert(e2->e, v)->co);
			}
		}
	}
}

/* chosen so that 1/sin(BEVEL_GOOD_ANGLE) is about 4, giving that expansion factor to bevel width */
#define BEVEL_GOOD_ANGLE 0.25f

/* Calculate the meeting point between e1 and e2 (one of which should have zero offsets),
 * where e1 precedes e2 in CCW order around their common vertex v (viewed from normal side).
 * If r_angle is provided, return the angle between e and emeet in *r_angle.
 * If the angle is 0, or it is 180 degrees or larger, there will be no meeting point;
 * return false in that case, else true. */
static bool offset_meet_edge(EdgeHalf *e1, EdgeHalf *e2, BMVert *v,  float meetco[3], float *r_angle)
{
	float dir1[3], dir2[3], fno[3], ang, sinang;

	sub_v3_v3v3(dir1, BM_edge_other_vert(e1->e, v)->co, v->co);
	sub_v3_v3v3(dir2, BM_edge_other_vert(e2->e, v)->co, v->co);
	normalize_v3(dir1);
	normalize_v3(dir2);

	/* find angle from dir1 to dir2 as viewed from vertex normal side */
	ang = angle_normalized_v3v3(dir1, dir2);
	if (fabsf(ang) < BEVEL_GOOD_ANGLE) {
		if (r_angle)
			*r_angle = 0.0f;
		return false;
	}
	cross_v3_v3v3(fno, dir1, dir2);
	if (dot_v3v3(fno, v->no) < 0.0f) {
		ang = 2.0f * (float)M_PI - ang;  /* angle is reflex */
		if (r_angle)
			*r_angle = ang;
		return false;
	}
	if (r_angle)
		*r_angle = ang;

	if (fabsf(ang - (float)M_PI) < BEVEL_GOOD_ANGLE)
		return false;

	sinang = sinf(ang);

	copy_v3_v3(meetco, v->co);
	if (e1->offset_r == 0.0f)
		madd_v3_v3fl(meetco, dir1, e2->offset_l / sinang);
	else
		madd_v3_v3fl(meetco, dir2, e1->offset_r / sinang);
	return true;
}

/* Return true if it will look good to put the meeting point where offset_on_edge_between
 * would put it. This means that neither side sees a reflex angle */
static bool good_offset_on_edge_between(EdgeHalf *e1, EdgeHalf *e2, EdgeHalf *emid, BMVert *v)
{
	float ang;
	float meet[3];

	return offset_meet_edge(e1, emid, v, meet, &ang) &&
	       offset_meet_edge(emid, e2, v, meet, &ang);
}

/* Calculate the best place for a meeting point for the offsets from edges e1 and e2
 * on the in-between edge emid.  Viewed from the vertex normal side, the CCW
 * order of these edges is e1, emid, e2.
 * The offsets probably do not meet at a common point on emid, so need to pick
 * one that causes the least problems. If the other end of one of e1 or e2 has been visited
 * already, prefer to keep the offset the same on this end.
 * Otherwise, pick a point between the two intersection points on emid that minimizes
 * the sum of squares of errors from desired offset. */
static void offset_on_edge_between(
        BevelParams *bp, EdgeHalf *e1, EdgeHalf *e2, EdgeHalf *emid,
        BMVert *v, float meetco[3])
{
	float d, ang1, ang2, sina1, sina2, lambda;
	float meet1[3], meet2[3];
	bool visited1, visited2, ok1, ok2;

	BLI_assert(e1->is_bev && e2->is_bev && !emid->is_bev);

	visited1 = other_edge_half_visited(bp, e1);
	visited2 = other_edge_half_visited(bp, e2);

	ok1 = offset_meet_edge(e1, emid, v, meet1, &ang1);
	ok2 = offset_meet_edge(emid, e2, v, meet2, &ang2);
	if (ok1 && ok2) {
		if (visited1 && !visited2) {
			copy_v3_v3(meetco, meet1);
		}
		else if (!visited1 && visited2) {
			copy_v3_v3(meetco, meet2);
		}
		else {
			/* find best compromise meet point */
			sina1 = sinf(ang1);
			sina2 = sinf(ang2);
			lambda = sina2 * sina2 / (sina1 * sina1 + sina2 * sina2);
			interp_v3_v3v3(meetco, meet1, meet2, lambda);
		}
	}
	else if (ok1 && !ok2) {
		copy_v3_v3(meetco, meet1);
	}
	else if (!ok1 && ok2) {
		copy_v3_v3(meetco, meet2);
	}
	else {
		/* Neither offset line met emid.
		 * This should only happen if all three lines are on top of each other */
		slide_dist(emid, v, e1->offset_r, meetco);
	}

	/* offsets may have changed now */
	d = dist_to_line_v3(meetco, v->co, BM_edge_other_vert(e1->e, v)->co);
	if (fabsf(d - e1->offset_r) > BEVEL_EPSILON)
		e1->offset_r = d;
	d = dist_to_line_v3(meetco, v->co, BM_edge_other_vert(e2->e, v)->co);
	if (fabsf(d - e2->offset_l) > BEVEL_EPSILON)
		e2->offset_l = d;
}

/* Offset by e->offset in plane with normal plane_no, on left if left==true,
 * else on right.  If no is NULL, choose an arbitrary plane different
 * from eh's direction. */
static void offset_in_plane(EdgeHalf *e, const float plane_no[3], bool left, float r[3])
{
	float dir[3], no[3], fdir[3];
	BMVert *v;

	v = e->is_rev ? e->e->v2 : e->e->v1;

	sub_v3_v3v3(dir, BM_edge_other_vert(e->e, v)->co, v->co);
	normalize_v3(dir);
	if (plane_no) {
		copy_v3_v3(no, plane_no);
	}
	else {
		zero_v3(no);
		if (fabsf(dir[0]) < fabsf(dir[1]))
			no[0] = 1.0f;
		else
			no[1] = 1.0f;
	}
	if (left)
		cross_v3_v3v3(fdir, dir, no);
	else
		cross_v3_v3v3(fdir, no, dir);
	normalize_v3(fdir);
	copy_v3_v3(r, v->co);
	madd_v3_v3fl(r, fdir, left ? e->offset_l : e->offset_r);
}

/* Calculate the point on e where line (co_a, co_b) comes closest to and return it in projco */
static void project_to_edge(BMEdge *e, const float co_a[3], const float co_b[3], float projco[3])
{
	float otherco[3];

	if (!isect_line_line_v3(e->v1->co, e->v2->co, co_a, co_b, projco, otherco)) {
#ifdef BEVEL_ASSERT_PROJECT
		BLI_assert(!"project meet failure");
#endif
		copy_v3_v3(projco, e->v1->co);
	}
}

/* If there is a bndv->ebev edge, find the mid control point if necessary.
 * It is the closest point on the beveled edge to the line segment between
 * bndv and bndv->next.  */
static void set_profile_params(BevelParams *bp, BevVert *bv, BoundVert *bndv)
{
	EdgeHalf *e;
	Profile *pro;
	float co1[3], co2[3], co3[3], d1[3], d2[3];
	bool do_linear_interp;

	copy_v3_v3(co1, bndv->nv.co);
	copy_v3_v3(co2, bndv->next->nv.co);
	pro = &bndv->profile;
	e = bndv->ebev;
	do_linear_interp = true;
	if (e) {
		do_linear_interp = false;
		pro->super_r = bp->pro_super_r;
		/* projection direction is direction of the edge */
		sub_v3_v3v3(pro->proj_dir, e->e->v1->co, e->e->v2->co);
		normalize_v3(pro->proj_dir);
		project_to_edge(e->e, co1, co2, pro->midco);
		if (DEBUG_OLD_PROJ_TO_PERP_PLANE) {
			/* put arc endpoints on plane with normal proj_dir, containing midco */
			add_v3_v3v3(co3, co1, pro->proj_dir);
			if (!isect_line_plane_v3(pro->coa, co1, co3, pro->midco, pro->proj_dir)) {
				/* shouldn't happen */
				copy_v3_v3(pro->coa, co1);
			}
			add_v3_v3v3(co3, co2, pro->proj_dir);
			if (!isect_line_plane_v3(pro->cob, co2, co3, pro->midco, pro->proj_dir)) {
				/* shouldn't happen */
				copy_v3_v3(pro->cob, co2);
			}
		}
		else {
			copy_v3_v3(pro->coa, co1);
			copy_v3_v3(pro->cob, co2);
		}
		/* default plane to project onto is the one with triangle co1 - midco - co2 in it */
		sub_v3_v3v3(d1, pro->midco, co1);
		sub_v3_v3v3(d2, pro->midco, co2);
		normalize_v3(d1);
		normalize_v3(d2);
		cross_v3_v3v3(pro->plane_no, d1, d2);
		normalize_v3(pro->plane_no);
		if (nearly_parallel(d1, d2)) {
			/* co1 - midco -co2 are collinear.
			 * Should be case that beveled edge is coplanar with two boundary verts.
			 * We want to move the profile to that common plane, if possible.
			 * That makes the multi-segment bevels curve nicely in that plane, as users expect.
			 * The new midco should be either v (when neighbor edges are unbeveled)
			 * or the intersection of the offset lines (if they are).
			 * If the profile is going to lead into unbeveled edges on each side
			 * (that is, both BoundVerts are "on-edge" points on non-beveled edges)
			 */
			if (DEBUG_OLD_PLANE_SPECIAL && (e->prev->is_bev || e->next->is_bev)) {
				do_linear_interp = true;
			}
			else {
				if (DEBUG_OLD_PROJ_TO_PERP_PLANE) {
					copy_v3_v3(pro->coa, co1);
					copy_v3_v3(pro->cob, co2);
				}
				if (DEBUG_OLD_FLAT_MID) {
					copy_v3_v3(pro->midco, bv->v->co);
				}
				else {
					copy_v3_v3(pro->midco, bv->v->co);
					if (e->prev->is_bev && e->next->is_bev && bv->selcount >= 3) {
						/* want mid at the meet point of next and prev offset edges */
						float d3[3], d4[3], co4[3], meetco[3], isect2[3];
						int isect_kind;

						sub_v3_v3v3(d3, e->prev->e->v1->co, e->prev->e->v2->co);
						sub_v3_v3v3(d4, e->next->e->v1->co, e->next->e->v2->co);
						normalize_v3(d3);
						normalize_v3(d4);
						if (nearly_parallel(d3, d4)) {
							/* offset lines are collinear - want linear interpolation */
							mid_v3_v3v3(pro->midco, co1, co2);
							do_linear_interp = true;
						}
						else {
							add_v3_v3v3(co3, co1, d3);
							add_v3_v3v3(co4, co2, d4);
							isect_kind = isect_line_line_v3(co1, co3, co2, co4, meetco, isect2);
							if (isect_kind != 0) {
								copy_v3_v3(pro->midco, meetco);
							}
							else {
								/* offset lines don't intersect - want linear interpolation */
								mid_v3_v3v3(pro->midco, co1, co2);
								do_linear_interp = true;
							}
						}
					}
				}
				copy_v3_v3(pro->cob, co2);
				sub_v3_v3v3(d1, pro->midco, co1);
				normalize_v3(d1);
				sub_v3_v3v3(d2, pro->midco, co2);
				normalize_v3(d2);
				cross_v3_v3v3(pro->plane_no, d1, d2);
				normalize_v3(pro->plane_no);
				if (nearly_parallel(d1, d2)) {
					/* whole profile is collinear with edge: just interpolate */
					do_linear_interp = true;
				}
				else {
					copy_v3_v3(pro->plane_co, bv->v->co);
					copy_v3_v3(pro->proj_dir, pro->plane_no);
				}
			}
		}
		copy_v3_v3(pro->plane_co, co1);
	}
	if (do_linear_interp) {
		pro->super_r = PRO_LINE_R;
		copy_v3_v3(pro->coa, co1);
		copy_v3_v3(pro->cob, co2);
		mid_v3_v3v3(pro->midco, co1, co2);
		/* won't use projection for this line profile */
		zero_v3(pro->plane_co);
		zero_v3(pro->plane_no);
		zero_v3(pro->proj_dir);
	}
}

/* Move the profile plane for bndv to the plane containing e1 and e2, which share a vert */
static void move_profile_plane(BoundVert *bndv, EdgeHalf *e1, EdgeHalf *e2)
{
	float d1[3], d2[3], no[3], no2[3], dot;

	/* only do this if projecting, and e1, e2, and proj_dir are not coplanar */
	if (is_zero_v3(bndv->profile.proj_dir))
		return;
	sub_v3_v3v3(d1, e1->e->v1->co, e1->e->v2->co);
	sub_v3_v3v3(d2, e2->e->v1->co, e2->e->v2->co);
	cross_v3_v3v3(no, d1, d2);
	cross_v3_v3v3(no2, d1, bndv->profile.proj_dir);
	if (normalize_v3(no) > BEVEL_EPSILON_BIG && normalize_v3(no2) > BEVEL_EPSILON_BIG) {
		dot = fabsf(dot_v3v3(no, no2));
		if (fabsf(dot - 1.0f) > BEVEL_EPSILON_BIG)
			copy_v3_v3(bndv->profile.plane_no, no);
	}
}

/* Move the profile plane for the two BoundVerts involved in a weld.
 * We want the plane that is most likely to have the intersections of the
 * two edges' profile projections on it. bndv1 and bndv2 are by
 * construction the intersection points of the outside parts of the profiles.
 * The original vertex should form a third point of the desired plane. */
static void move_weld_profile_planes(BevVert *bv, BoundVert *bndv1, BoundVert *bndv2)
{
	float d1[3], d2[3], no[3], no2[3], no3[3], dot1, dot2, l1, l2, l3;

	/* only do this if projecting, and d1, d2, and proj_dir are not coplanar */
	if (is_zero_v3(bndv1->profile.proj_dir) || is_zero_v3(bndv2->profile.proj_dir))
		return;
	sub_v3_v3v3(d1, bv->v->co, bndv1->nv.co);
	sub_v3_v3v3(d2, bv->v->co, bndv2->nv.co);
	cross_v3_v3v3(no, d1, d2);
	l1 = normalize_v3(no);
	/* "no" is new normal projection plane, but don't move if
	 * it is coplanar with both of the projection dirs */
	cross_v3_v3v3(no2, d1, bndv1->profile.proj_dir);
	l2 = normalize_v3(no2);
	cross_v3_v3v3(no3, d2, bndv2->profile.proj_dir);
	l3 = normalize_v3(no3);
	if (l1 > BEVEL_EPSILON && (l2 > BEVEL_EPSILON || l3 > BEVEL_EPSILON)) {
		dot1 = fabsf(dot_v3v3(no, no2));
		dot2 = fabsf(dot_v3v3(no, no3));
		if (fabsf(dot1 - 1.0f) > BEVEL_EPSILON)
			copy_v3_v3(bndv1->profile.plane_no, no);
		if (fabsf(dot2 - 1.0f) > BEVEL_EPSILON)
			copy_v3_v3(bndv2->profile.plane_no, no);
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

/* Fill matrix r_mat so that a point in the sheared parallelogram with corners
 * va, vmid, vb (and the 4th that is implied by it being a parallelogram)
 * is the result of transforming the unit square by multiplication with r_mat.
 * If it can't be done because the parallelogram is degenerate, return false
 * else return true.
 * Method:
 * Find vo, the origin of the parallelogram with other three points va, vmid, vb.
 * Also find vd, which is in direction normal to parallelogram and 1 unit away
 * from the origin.
 * The quarter circle in first quadrant of unit square will be mapped to the
 * quadrant of a sheared ellipse in the parallelogram, using a matrix.
 * The matrix mat is calculated to map:
 *    (0,1,0) -> va
 *    (1,1,0) -> vmid
 *    (1,0,0) -> vb
 *    (0,1,1) -> vd
 * We want M to make M*A=B where A has the left side above, as columns
 * and B has the right side as columns - both extended into homogeneous coords.
 * So M = B*(Ainverse).  Doing Ainverse by hand gives the code below.
 */
static bool make_unit_square_map(
        const float va[3], const float vmid[3], const float vb[3],
        float r_mat[4][4])
{
	float vo[3], vd[3], vb_vmid[3], va_vmid[3], vddir[3];

	sub_v3_v3v3(va_vmid, vmid, va);
	sub_v3_v3v3(vb_vmid, vmid, vb);
	if (fabsf(angle_v3v3(va_vmid, vb_vmid) - (float)M_PI) > BEVEL_EPSILON_ANG) {
		sub_v3_v3v3(vo, va, vb_vmid);
		cross_v3_v3v3(vddir, vb_vmid, va_vmid);
		normalize_v3(vddir);
		add_v3_v3v3(vd, vo, vddir);

		/* The cols of m are: {vmid - va, vmid - vb, vmid + vd - va -vb, va + vb - vmid;
		 * blender transform matrices are stored such that m[i][*] is ith column;
		 * the last elements of each col remain as they are in unity matrix */
		sub_v3_v3v3(&r_mat[0][0], vmid, va);
		r_mat[0][3] = 0.0f;
		sub_v3_v3v3(&r_mat[1][0], vmid, vb);
		r_mat[1][3] = 0.0f;
		add_v3_v3v3(&r_mat[2][0], vmid, vd);
		sub_v3_v3(&r_mat[2][0], va);
		sub_v3_v3(&r_mat[2][0], vb);
		r_mat[2][3] = 0.0f;
		add_v3_v3v3(&r_mat[3][0], va, vb);
		sub_v3_v3(&r_mat[3][0], vmid);
		r_mat[3][3] = 1.0f;

		return true;
	}
	else
		return false;
}

/* Like make_unit_square_map, but this one makes a matrix that transforms the
 * (1,1,1) corner of a unit cube into an arbitrary corner with corner vert d
 * and verts around it a, b, c (in ccw order, viewed from d normal dir).
 * The matrix mat is calculated to map:
 *    (1,0,0) -> va
 *    (0,1,0) -> vb
 *    (0,0,1) -> vc
 *    (1,1,1) -> vd
 * We want M to make M*A=B where A has the left side above, as columns
 * and B has the right side as columns - both extended into homogeneous coords.
 * So M = B*(Ainverse).  Doing Ainverse by hand gives the code below.
 * The cols of M are 1/2{va-vb+vc-vd}, 1/2{-va+vb-vc+vd}, 1/2{-va-vb+vc+vd},
 * and 1/2{va+vb+vc-vd}
 * and Blender matrices have cols at m[i][*].
 */
static void make_unit_cube_map(
        const float va[3], const float vb[3], const float vc[3],
        const float vd[3], float r_mat[4][4])
{
	copy_v3_v3(r_mat[0], va);
	sub_v3_v3(r_mat[0], vb);
	sub_v3_v3(r_mat[0], vc);
	add_v3_v3(r_mat[0], vd);
	mul_v3_fl(r_mat[0], 0.5f);
	r_mat[0][3] = 0.0f;
	copy_v3_v3(r_mat[1], vb);
	sub_v3_v3(r_mat[1], va);
	sub_v3_v3(r_mat[1], vc);
	add_v3_v3(r_mat[1], vd);
	mul_v3_fl(r_mat[1], 0.5f);
	r_mat[1][3] = 0.0f;
	copy_v3_v3(r_mat[2], vc);
	sub_v3_v3(r_mat[2], va);
	sub_v3_v3(r_mat[2], vb);
	add_v3_v3(r_mat[2], vd);
	mul_v3_fl(r_mat[2], 0.5f);
	r_mat[2][3] = 0.0f;
	copy_v3_v3(r_mat[3], va);
	add_v3_v3(r_mat[3], vb);
	add_v3_v3(r_mat[3], vc);
	sub_v3_v3(r_mat[3], vd);
	mul_v3_fl(r_mat[3], 0.5f);
	r_mat[3][3] = 1.0f;
}

/* Get the coordinate on the superellipse (exponent r),
 * at parameter value u.  u goes from u to 2 as the
 * superellipse moves on the quadrant (0,1) to (1,0). */
static void superellipse_co(float u, float r, float r_co[2])
{
	float t;
	
	if (u <= 0.0f) {
		r_co[0] = 0.0f;
		r_co[1] = 1.0f;
	}
	else if (u >= 2.0f) {
		r_co[0] = 1.0f;
		r_co[1] = 0.0f;
	}
	else if (r == PRO_LINE_R) {
		t = u / 2.0f;
		r_co[0] = t;
		r_co[1] = 1.0f - t;
		
	}
	else if (r == PRO_SQUARE_IN_R) {
		if (u < 1.0f) {
			r_co[0] = 0.0f;
			r_co[1] = 1.0f - u;
		}
		else {
			r_co[0] = u - 1.0f;
			r_co[1] = 0.0f;
		}
	}
	else if (r == PRO_SQUARE_R) {
		if (u < 1.0f) {
			r_co[0] = u;
			r_co[1] = 1.0f;
		}
		else {
			r_co[0] = 1.0f;
			r_co[1] = 2.0f - u;
		}
		
	}
	else {
		t = u * (float)M_PI / 4.0f;  /* angle from y axis */
		r_co[0] = sinf(t);
		r_co[1] = cosf(t);
		if (r != PRO_SQUARE_R) {
			r_co[0] = pow(r_co[0], 2.0f / r);
			r_co[1] = pow(r_co[1], 2.0f / r);
		}
	}
}

/* Find the point on given profile at parameter i which goes from 0 to n as
 * the profile is moved from pro->coa to pro->cob.
 * We assume that n is either the global seg number or a power of 2 less than
 * or equal to the power of 2 >= seg.
 * In the latter case, we subsample the profile for seg_2, which will not necessarily
 * give equal spaced chords, but is in fact more what is desired by the cubic subdivision
 * method used to make the vmesh pattern. */
static void get_profile_point(BevelParams *bp, const Profile *pro, int i, int n, float r_co[3])
{
	int d;

	if (bp->seg == 1) {
		if (i == 0)
			copy_v3_v3(r_co, pro->coa);
		else
			copy_v3_v3(r_co, pro->cob);
	}
	
	else {
		if (n == bp->seg) {
			BLI_assert(pro->prof_co != NULL);
			copy_v3_v3(r_co, pro->prof_co + 3 * i);
		}
		else {
			BLI_assert(is_power_of_2_i(n) && n <= bp->pro_spacing.seg_2);
			/* set d to spacing in prof_co_2 between subsamples */
			d = bp->pro_spacing.seg_2 / n;
			copy_v3_v3(r_co, pro->prof_co_2 + 3 * i * d);
		}
	}
}

/* Calculate the actual coordinate values for bndv's profile.
 * This is only needed if bp->seg > 1.
 * Allocate the space for them if that hasn't been done already.
 * If bp->seg is not a power of 2, also need to calculate
 * the coordinate values for the power of 2 >= bp->seg,
 * because the ADJ pattern needs power-of-2 boundaries
 * during construction. */
static void calculate_profile(BevelParams *bp, BoundVert *bndv)
{
	int i, k, ns;
	const float *uvals;
	float co[3], co2[3], p[3], m[4][4];
	float *prof_co, *prof_co_k;
	float r;
	bool need_2, map_ok;
	Profile *pro = &bndv->profile;

	if (bp->seg == 1)
		return;

	need_2 = bp->seg != bp->pro_spacing.seg_2;
	if (!pro->prof_co) {
		pro->prof_co = (float *)BLI_memarena_alloc(bp->mem_arena, (bp->seg + 1) * 3 * sizeof(float));
		if (need_2)
			pro->prof_co_2 = (float *)BLI_memarena_alloc(bp->mem_arena, (bp->pro_spacing.seg_2 + 1) * 3 *sizeof(float));
		else
			pro->prof_co_2 = pro->prof_co;
	}
	r = pro->super_r;
	if (r == PRO_LINE_R)
		map_ok = false;
	else
		map_ok = make_unit_square_map(pro->coa, pro->midco, pro->cob, m);
	for (i = 0; i < 2; i++) {
		if (i == 0) {
			ns = bp->seg;
			uvals = bp->pro_spacing.uvals;
			prof_co = pro->prof_co;
		}
		else {
			if (!need_2)
				break;  /* shares coords with pro->prof_co */
			ns = bp->pro_spacing.seg_2;
			uvals = bp->pro_spacing.uvals_2;
			prof_co = pro->prof_co_2;
		}
		BLI_assert((r == PRO_LINE_R || uvals != NULL) && prof_co != NULL);
		for (k = 0; k <= ns; k++) {
			if (k == 0)
				copy_v3_v3(co, pro->coa);
			else if (k == ns)
				copy_v3_v3(co, pro->cob);
			else {
				if (map_ok) {
					superellipse_co(uvals[k], r, p);
					p[2] = 0.0f;
					mul_v3_m4v3(co, m, p);
				}
				else {
					interp_v3_v3v3(co, pro->coa, pro->cob, (float)k / (float)ns);
				}
			}
			/* project co onto final profile plane */
			prof_co_k = prof_co + 3 * k;
			if (!is_zero_v3(pro->proj_dir)) {
				add_v3_v3v3(co2, co, pro->proj_dir);
				if (!isect_line_plane_v3(prof_co_k, co, co2, pro->plane_co, pro->plane_no)) {
					/* shouldn't happen */
					copy_v3_v3(prof_co_k, co);
				}
			}
			else {
				copy_v3_v3(prof_co_k, co);
			}
		}
	}
}

/* Snap a direction co to a superellipsoid with parameter super_r.
 * For square profiles, midline says whether or not to snap to both planes. */
static void snap_to_superellipsoid(float co[3], const float super_r, bool midline)
{
	float a, b, c, x, y, z, r, rinv, dx, dy;

	r = super_r;
	if (r == PRO_CIRCLE_R) {
		normalize_v3(co);
		return;
	}

	x = a = max_ff(0.0f, co[0]);
	y = b = max_ff(0.0f, co[1]);
	z = c = max_ff(0.0f, co[2]);
	if (r == PRO_SQUARE_R || r == PRO_SQUARE_IN_R) {
		/* will only be called for 2d profile */
		BLI_assert(fabsf(z) < BEVEL_EPSILON);
		z = 0.0f;
		x = min_ff(1.0f, x);
		y = min_ff(1.0f, y);
		if (r == PRO_SQUARE_R) {
			/* snap to closer of x==1 and y==1 lines, or maybe both */
			dx = 1.0f - x;
			dy = 1.0f - y;
			if (dx < dy) {
				x = 1.0f;
				y = midline ? 1.0f : y;
			}
			else {
				y = 1.0f;
				x = midline ? 1.0f : x;
			}
		}
		else {
			/* snap to closer of x==0 and y==0 lines, or maybe both */
			if (x < y) {
				x = 0.0f;
				y = midline ? 0.0f : y;
			}
			else {
				y = 0.0f;
				x = midline ? 0.0f : x;
			}
		}
	}
	else {
		rinv = 1.0f / r;
		if (a == 0.0f) {
			if (b == 0.0f) {
				x = 0.0f;
				y = 0.0f;
				z = powf(c, rinv);
			}
			else {
				x = 0.0f;
				y = powf(1.0f / (1.0f + powf(c / b, r)), rinv);
				z = c * y / b;
			}
		}
		else {
			x = powf(1.0f / (1.0f + powf(b / a, r) + powf(c / a, r)), rinv);
			y = b * x / a;
			z = c * x / a;
		}
	}
	co[0] = x;
	co[1] = y;
	co[2] = z;
}

/* Set the any_seam property for a BevVert and all its BoundVerts */
static void set_bound_vert_seams(BevVert *bv)
{
	BoundVert *v;
	EdgeHalf *e;

	bv->any_seam = false;
	v = bv->vmesh->boundstart;
	do {
		v->any_seam = false;
		for (e = v->efirst; e; e = e->next) {
			v->any_seam |= e->is_seam;
			if (e == v->elast)
				break;
		}
		bv->any_seam |= v->any_seam;
	} while ((v = v->next) != bv->vmesh->boundstart);
}

static int count_bound_vert_seams(BevVert *bv)
{
	int ans, i;

	if (!bv->any_seam)
		return 0;

	ans = 0;
	for (i = 0; i < bv->edgecount; i++)
		if (bv->edges[i].is_seam)
			ans++;
	return ans;
}

/* Is e between two planes where angle between is 180? */
static bool eh_on_plane(EdgeHalf *e)
{
	float dot;

	if (e->fprev && e->fnext) {
		dot = dot_v3v3(e->fprev->no, e->fnext->no);
		if (fabsf(dot) <= BEVEL_EPSILON_BIG ||
		    fabsf(dot - 1.0f) <= BEVEL_EPSILON_BIG)
		{
			return true;
		}
	}
	return false;
}

/* Calculate the profiles for all the BoundVerts of VMesh vm */
static void calculate_vm_profiles(BevelParams *bp, BevVert *bv, VMesh *vm)
{
	BoundVert *v;

	v = vm->boundstart;
	do {
		set_profile_params(bp, bv, v);
		calculate_profile(bp, v);
	} while ((v = v->next) != vm->boundstart);
}

/* Implements build_boundary for vertex-only case */
static void build_boundary_vertex_only(BevelParams *bp, BevVert *bv, bool construct)
{
	VMesh *vm = bv->vmesh;
	EdgeHalf *efirst, *e;
	BoundVert *v;
	float co[3];

	BLI_assert(bp->vertex_only);

	e = efirst = &bv->edges[0];
	do {
		slide_dist(e, bv->v, e->offset_l, co);
		if (construct) {
			v = add_new_bound_vert(bp->mem_arena, vm, co);
			v->efirst = v->elast = e;
			e->leftv = e->rightv = v;
		}
		else {
			adjust_bound_vert(e->leftv, co);
		}
	} while ((e = e->next) != efirst);

	calculate_vm_profiles(bp, bv, vm);

	if (construct) {
		set_bound_vert_seams(bv);
		if (vm->count == 2)
			vm->mesh_kind = M_NONE;
		else if (bp->seg == 1)
			vm->mesh_kind = M_POLY;
		else
			vm->mesh_kind = M_ADJ;
	}
}

/**
 * Special case of build_boundary when a single edge is beveled.
 * The 'width adjust' part of build_boundary has been done already,
 * and \a efirst is the first beveled edge at vertex \a bv.
*/
static void build_boundary_terminal_edge(BevelParams *bp, BevVert *bv, EdgeHalf *efirst, bool construct)
{
	MemArena *mem_arena = bp->mem_arena;
	VMesh *vm = bv->vmesh;
	BoundVert *v;
	EdgeHalf *e;
	const float *no;
	float co[3], d;

	e = efirst;
	if (bv->edgecount == 2) {
		/* only 2 edges in, so terminate the edge with an artificial vertex on the unbeveled edge */
		no = e->fprev ? e->fprev->no : (e->fnext ? e->fnext->no : NULL);
		offset_in_plane(e, no, true, co);
		if (construct) {
			v = add_new_bound_vert(mem_arena, vm, co);
			v->efirst = v->elast = v->ebev = e;
			e->leftv = v;
		}
		else {
			adjust_bound_vert(e->leftv, co);
		}
		no = e->fnext ? e->fnext->no : (e->fprev ? e->fprev->no : NULL);
		offset_in_plane(e, no, false, co);
		if (construct) {
			v = add_new_bound_vert(mem_arena, vm, co);
			v->efirst = v->elast = e;
			e->rightv = v;
		}
		else {
			adjust_bound_vert(e->rightv, co);
		}
		/* make artifical extra point along unbeveled edge, and form triangle */
		slide_dist(e->next, bv->v, e->offset_l, co);
		if (construct) {
			v = add_new_bound_vert(mem_arena, vm, co);
			v->efirst = v->elast = e->next;
			e->next->leftv = e->next->rightv = v;
			/* could use M_POLY too, but tri-fan looks nicer)*/
			vm->mesh_kind = M_TRI_FAN;
			set_bound_vert_seams(bv);
		}
		else {
			adjust_bound_vert(e->next->leftv, co);
		}
	}
	else {
		/* More than 2 edges in. Put on-edge verts on all the other edges
		 * and join with the beveled edge to make a poly or adj mesh,
		 * Because e->prev has offset 0, offset_meet will put co on that edge. */
		/* TODO: should do something else if angle between e and e->prev > 180 */
		offset_meet(e->prev, e, bv->v, e->fprev, false, co);
		if (construct) {
			v = add_new_bound_vert(mem_arena, vm, co);
			v->efirst = e->prev;
			v->elast = v->ebev = e;
			e->leftv = v;
			e->prev->leftv = e->prev->rightv = v;
		}
		else {
			adjust_bound_vert(e->leftv, co);
		}
		e = e->next;
		offset_meet(e->prev, e, bv->v, e->fprev, false, co);
		if (construct) {
			v = add_new_bound_vert(mem_arena, vm, co);
			v->efirst = e->prev;
			v->elast = e;
			e->leftv = e->rightv = v;
			e->prev->rightv = v;
		}
		else {
			adjust_bound_vert(e->leftv, co);
		}
		/* For the edges not adjacent to the beveled edge, slide the bevel amount along. */
		d = efirst->offset_l_spec;
		for (e = e->next; e->next != efirst; e = e->next) {
			slide_dist(e, bv->v, d, co);
			if (construct) {
				v = add_new_bound_vert(mem_arena, vm, co);
				v->efirst = v->elast = e;
				e->leftv = e->rightv = v;
			}
			else {
				adjust_bound_vert(e->leftv, co);
			}
		}
	}
	calculate_vm_profiles(bp, bv, vm);

	if (bv->edgecount >= 3) {
		/* special case: snap profile to plane of adjacent two edges */
		v = vm->boundstart;
		BLI_assert(v->ebev != NULL);
		move_profile_plane(v, v->efirst, v->next->elast);
		calculate_profile(bp, v);
	}

	if (construct) {
		set_bound_vert_seams(bv);

		if (vm->count == 2 && bv->edgecount == 3) {
			vm->mesh_kind = M_NONE;
		}
		else if (vm->count == 3) {
			vm->mesh_kind = M_TRI_FAN;
		}
		else {
			vm->mesh_kind = M_POLY;
		}
	}
}

/* Return a value that is v if v is within BEVEL_MAX_ADJUST_PCT of the spec (assumed positive),
 * else clamp to make it at most that far away from spec */
static float clamp_adjust(float v, float spec)
{
	float allowed_delta = spec * (BEVEL_MAX_ADJUST_PCT / 100.0f);

	if (v - spec > allowed_delta)
		return spec + allowed_delta;
	else if (spec - v > allowed_delta)
		return spec - allowed_delta;
	else
		return v;
}

/* Make a circular list of BoundVerts for bv, each of which has the coordinates
 * of a vertex on the boundary of the beveled vertex bv->v.
 * This may adjust some EdgeHalf widths, and there might have to be
 * a subsequent pass to make the widths as consistent as possible.
 * The first time through, construct will be true and we are making the BoundVerts
 * and setting up the BoundVert and EdgeHalf pointers appropriately.
 * For a width consistency path, we just recalculate the coordinates of the
 * BoundVerts. If the other ends have been (re)built already, then we
 * copy the offsets from there to match, else we use the ideal (user-specified)
 * widths.
 * Also, if construct, decide on the mesh pattern that will be used inside the boundary.
 * Doesn't make the actual BMVerts */
static void build_boundary(BevelParams *bp, BevVert *bv, bool construct)
{
	MemArena *mem_arena = bp->mem_arena;
	EdgeHalf *efirst, *e, *e2, *e3, *enip, *eip, *eother;
	BoundVert *v;
	BevVert *bvother;
	VMesh *vm;
	float co[3];
	int nip, nnip;

	/* Current bevel does nothing if only one edge into a vertex */
	if (bv->edgecount <= 1)
		return;

	if (bp->vertex_only) {
		build_boundary_vertex_only(bp, bv, construct);
		return;
	}

	vm = bv->vmesh;

	/* Find a beveled edge to be efirst. Then for each edge, try matching widths to other end. */
	e = efirst = next_bev(bv, NULL);
	BLI_assert(e->is_bev);
	do {
		eother = find_other_end_edge_half(bp, e, &bvother);
		if (eother && bvother->visited && bp->offset_type != BEVEL_AMT_PERCENT) {
			/* try to keep bevel even by matching other end offsets */
			/* sometimes, adjustment can accumulate errors so use the bp->limit_offset to
			 * let user limit the adjustment to within a reasonable range around spec */
			if (bp->limit_offset) {
				e->offset_l = clamp_adjust(eother->offset_r, e->offset_l_spec);
				e->offset_r = clamp_adjust(eother->offset_l, e->offset_r_spec);
			}
			else {
				e->offset_l = eother->offset_r;
				e->offset_r = eother->offset_l;
			}
		}
		else {
			/* reset to user spec */
			e->offset_l = e->offset_l_spec;
			e->offset_r = e->offset_r_spec;
		}
	} while ((e = e->next) != efirst);

	if (bv->selcount == 1) {
		/* special case: only one beveled edge in */
		build_boundary_terminal_edge(bp, bv, efirst, construct);
		return;
	}

	/* Here: there is more than one beveled edge.
	 * We make BoundVerts to connect the sides of the beveled edges.
	 * Non-beveled edges in between will just join to the appropriate juncture point. */
	e = efirst;
	do {
		BLI_assert(e->is_bev);
		/* Make the BoundVert for the right side of e; other side will be made
		 * when the beveled edge to the left of e is handled.
		 * Analyze edges until next beveled edge.
		 * They are either "in plane" (preceding and subsequent faces are coplanar)
		 * or not. The "non-in-plane" edges effect silhouette and we prefer to slide
		 * along one of those if possible. */
		nip = nnip = 0;        /* counts of in-plane / not-in-plane */
		enip = eip = NULL;     /* representatives of each */
		for (e2 = e->next; !e2->is_bev; e2 = e2->next) {
			if (eh_on_plane(e2)) {
				nip++;
				eip = e2;
			}
			else {
				nnip++;
				enip = e2;
			}
		}
		if (nip == 0 && nnip == 0) {
			offset_meet(e, e2, bv->v, e->fnext, false, co);
		}
		else if (nnip > 0) {
			if (bp->loop_slide && nnip == 1 && good_offset_on_edge_between(e, e2, enip, bv->v)) {
				offset_on_edge_between(bp, e, e2, enip, bv->v, co);
			}
			else {
				offset_meet(e, e2, bv->v, NULL, true, co);
			}
		}
		else {
			/* nip > 0 and nnip == 0 */
			if (bp->loop_slide && nip == 1 && good_offset_on_edge_between(e, e2, eip, bv->v)) {
				offset_on_edge_between(bp, e, e2, eip, bv->v, co);
			}
			else {
				offset_meet(e, e2, bv->v, e->fnext, true, co);
			}
		}
		if (construct) {
			v = add_new_bound_vert(mem_arena, vm, co);
			v->efirst = e;
			v->elast = e2;
			v->ebev = e2;
			e->rightv = v;
			e2->leftv = v;
			for (e3 = e->next; e3 != e2; e3 = e3->next) {
				e3->leftv = e3->rightv = v;
			}
		}
		else {
			adjust_bound_vert(e->rightv, co);
		}
		e = e2;
	} while (e != efirst);

	calculate_vm_profiles(bp, bv, vm);

	if (construct) {
		set_bound_vert_seams(bv);

		if (vm->count == 2) {
			vm->mesh_kind = M_NONE;
		}
		else if (efirst->seg == 1) {
			vm->mesh_kind = M_POLY;
		}
		else {
			vm->mesh_kind = M_ADJ;
		}
	}
}

/* Do a global pass to try to make offsets as even as possible.
 * Consider this graph:
 *   nodes = BevVerts
 *   edges = { (u,v) } where u and v are nodes such that u and v
 *        are connected by a mesh edge that has at least one end
 *        whose offset does not match the user spec.
 *
 * Do a breadth-first search on this graph, starting from nodes
 * that have any_adjust=true, and changing all
 * not-already-changed offsets on EdgeHalfs to match the
 * corresponding ones that changed on the other end.
 * The graph is dynamic in the sense that having an offset that
 * doesn't meet the user spec can be added as the search proceeds.
 * We want this search to be deterministic (not dependent
 * on order of processing through hash table), so as to avoid
 * flicker to to different decisions made if search is different
 * while dragging the offset number in the UI.  So look for the
 * lower vertex number when there is a choice of where to start.
 *
 * Note that this might not process all BevVerts, only the ones
 * that need adjustment.
 */
static void adjust_offsets(BevelParams *bp)
{
	BevVert *bv, *searchbv, *bvother;
	int i, searchi;
	GHashIterator giter;
	EdgeHalf *e, *efirst, *eother;
	GSQueue *q;
	float max_rel_adj;

	BLI_assert(!bp->vertex_only);
	GHASH_ITER(giter, bp->vert_hash) {
		bv = BLI_ghashIterator_getValue(&giter);
		bv->visited = false;
	}

	q = BLI_gsqueue_new(sizeof(BevVert *));
	/* the following loop terminates because at least one node is visited each time */
	for (;;) {
		/* look for root of a connected component in search graph */
		searchbv = NULL;
		searchi = -1;
		GHASH_ITER(giter, bp->vert_hash) {
			bv = BLI_ghashIterator_getValue(&giter);
			if (!bv->visited && max_edge_half_offset_rel_change(bv) > 0.0f) {
				i = BM_elem_index_get(bv->v);
				if (!searchbv || i < searchi) {
					searchbv = bv;
					searchi = i;
				}
			}
		}
		if (searchbv == NULL)
			break;

		BLI_gsqueue_push(q, &searchbv);
		while (!BLI_gsqueue_is_empty(q)) {
			BLI_gsqueue_pop(q, &bv);
			/* If do this check, don't have to check for already-on-queue before push, below */
			if (bv->visited)
				continue;
			bv->visited = true;
			build_boundary(bp, bv, false);

			e = efirst = &bv->edges[0];
			do {
				eother = find_other_end_edge_half(bp, e, &bvother);
				if (eother && !bvother->visited && edge_half_offset_changed(e)) {
					BLI_gsqueue_push(q, &bvother);
				}
			} while ((e = e->next) != efirst);
		}
	}
	BLI_gsqueue_free(q);

	/* Should we auto-limit the error accumulation? Typically, spirals can lead to 100x relative adjustments,
	 * and somewhat hacky mechanism of using bp->limit_offset to indicate "clamp the adjustments" is not
	 * obvious to users, who almost certainaly want clamping in this situation.
	 * The reason not to clamp always is that some models work better without it (e.g., Bent_test in regression
	 * suite, where relative adjust maximum is about .6). */
	if (!bp->limit_offset) {
		max_rel_adj = 0.0f;
		GHASH_ITER(giter, bp->vert_hash) {
			bv = BLI_ghashIterator_getValue(&giter);
			max_rel_adj = max_ff(max_rel_adj, max_edge_half_offset_rel_change(bv));
		}
		if (max_rel_adj > BEVEL_MAX_AUTO_ADJUST_PCT / 100.0f) {
			bp->limit_offset = true;
			adjust_offsets(bp);
			bp->limit_offset = false;
		}
	}
}

/* Do the edges at bv form a "pipe"?
 * Current definition: 3 or 4 beveled edges, 2 in line with each other,
 * with other edges on opposite sides of the pipe if there are 4.
 * Also, the vertex boundary should have 3 or 4 vertices in it,
 * and all of the faces involved should be parallel to the pipe edges.
 * Return the boundary vert whose ebev is one of the pipe edges, and
 * whose next boundary vert has a beveled, non-pipe edge. */
static BoundVert *pipe_test(BevVert *bv)
{
	EdgeHalf *e, *epipe;
	VMesh *vm;
	BoundVert *v1, *v2, *v3;
	float dir1[3], dir3[3];

	vm = bv->vmesh;
	if (vm->count < 3 || vm->count > 4 || bv->selcount < 3 || bv->selcount > 4)
		return NULL;

	/* find v1, v2, v3 all with beveled edges, where v1 and v3 have collinear edges */
	epipe = NULL;
	v1 = vm->boundstart;
	do {
		v2 = v1->next;
		v3 = v2->next;
		if (v1->ebev && v2->ebev && v3->ebev) {
			sub_v3_v3v3(dir1, bv->v->co, BM_edge_other_vert(v1->ebev->e, bv->v)->co);
			sub_v3_v3v3(dir3, BM_edge_other_vert(v3->ebev->e, bv->v)->co, bv->v->co);
			normalize_v3(dir1);
			normalize_v3(dir3);
			if (angle_normalized_v3v3(dir1, dir3) < BEVEL_EPSILON_ANG) {
				epipe =  v1->ebev;
				break;
			}
		}
	} while ((v1 = v1->next) != vm->boundstart);

	if (!epipe)
		return NULL;

	/* check face planes: all should have normals perpendicular to epipe */
	for (e = &bv->edges[0]; e != &bv->edges[bv->edgecount]; e++) {
		if (e->fnext) {
			if (dot_v3v3(dir1, e->fnext->no) > BEVEL_EPSILON_BIG)
				return NULL;
		}
	}
	return v1;
}

static VMesh *new_adj_vmesh(MemArena *mem_arena, int count, int seg, BoundVert *bounds)
{
	VMesh *vm;

	vm = (VMesh *)BLI_memarena_alloc(mem_arena, sizeof(VMesh));
	vm->count = count;
	vm->seg = seg;
	vm->boundstart = bounds;
	vm->mesh = (NewVert *)BLI_memarena_alloc(mem_arena, count * (1 + seg / 2) * (1 + seg) * sizeof(NewVert));
	vm->mesh_kind = M_ADJ;
	return vm;
}

/* VMesh verts for vertex i have data for (i, 0 <= j <= ns2, 0 <= k <= ns), where ns2 = floor(nseg / 2).
 * But these overlap data from previous and next i: there are some forced equivalences.
 * Let's call these indices the canonical ones: we will just calculate data for these
 *    0 <= j <= ns2, 0 <= k < ns2  (for odd ns2)
 *    0 <= j < ns2, 0 <= k <= ns2  (for even ns2)
 *        also (j=ns2, k=ns2) at i=0 (for even ns2)
 * This function returns the canonical one for any i, j, k in [0,n],[0,ns],[0,ns] */
static NewVert *mesh_vert_canon(VMesh *vm, int i, int j, int k)
{
	int n, ns, ns2, odd;
	NewVert *ans;

	n = vm->count;
	ns = vm->seg;
	ns2 = ns / 2;
	odd = ns % 2;
	BLI_assert(0 <= i && i <= n && 0 <= j && j <= ns && 0 <= k && k <= ns);

	if (!odd && j == ns2 && k == ns2)
		ans = mesh_vert(vm, 0, j, k);
	else if (j <= ns2 - 1 + odd && k <= ns2)
		ans = mesh_vert(vm, i, j, k);
	else if (k <= ns2)
		ans = mesh_vert(vm, (i + n - 1) % n, k, ns - j);
	else
		ans = mesh_vert(vm, (i + 1) % n, ns - k, j);
	return ans;
}

static bool is_canon(VMesh *vm, int i, int j, int k)
{
	int ns2 = vm->seg / 2;
	if (vm->seg % 2 == 1)
		return (j <= ns2 && k <= ns2);
	else
		return ((j < ns2 && k <= ns2) || (j == ns2 && k == ns2 && i == 0));
}

/* Copy the vertex data to all of vm verts from canonical ones */
static void vmesh_copy_equiv_verts(VMesh *vm)
{
	int n, ns, ns2, i, j, k;
	NewVert *v0, *v1;

	n = vm->count;
	ns = vm->seg;
	ns2 = ns / 2;
	for (i = 0; i < n; i++) {
		for (j = 0; j <= ns2; j++) {
			for (k = 0; k <= ns; k++) {
				if (is_canon(vm, i, j, k))
					continue;
				v1 = mesh_vert(vm, i, j, k);
				v0 = mesh_vert_canon(vm, i, j, k);
				copy_v3_v3(v1->co, v0->co);
				v1->v = v0->v;
			}
		}
	}
}

/* Calculate and return in r_cent the centroid of the center poly */
static void vmesh_center(VMesh *vm, float r_cent[3])
{
	int n, ns2, i;

	n = vm->count;
	ns2 = vm->seg / 2;
	if (vm->seg % 2) {
		zero_v3(r_cent);
		for (i = 0; i < n; i++) {
			add_v3_v3(r_cent, mesh_vert(vm, i, ns2, ns2)->co);
		}
		mul_v3_fl(r_cent, 1.0f / (float) n);
	}
	else {
		copy_v3_v3(r_cent, mesh_vert(vm, 0, ns2, ns2)->co);
	}
}

static void avg4(
        float co[3],
        const NewVert *v0, const NewVert *v1,
        const NewVert *v2, const NewVert *v3)
{
	add_v3_v3v3(co, v0->co, v1->co);
	add_v3_v3(co, v2->co);
	add_v3_v3(co, v3->co);
	mul_v3_fl(co, 0.25f);
}

/* gamma needed for smooth Catmull-Clark, Sabin modification */
static float sabin_gamma(int n)
{
	double ans, k, k2, k4, k6, x, y;

	/* precalculated for common cases of n */
	if (n < 3)
		return 0.0f;
	else if (n == 3)
		ans = 0.065247584f;
	else if (n == 4)
		ans = 0.25f;
	else if (n == 5)
		ans = 0.401983447f;
	else if (n == 6)
		ans = 0.523423277f;
	else {
		k = cos(M_PI / (double)n);
		/* need x, real root of x^3 + (4k^2 - 3)x - 2k = 0.
		 * answer calculated via Wolfram Alpha */
		k2 = k * k;
		k4 = k2 * k2;
		k6 = k4 * k2;
		y = pow(M_SQRT3 * sqrt(64.0 * k6 - 144.0 * k4 + 135.0 * k2 - 27.0) + 9.0 * k,
		        1.0 / 3.0);
		x = 0.480749856769136 * y - (0.231120424783545 * (12.0 * k2 - 9.0)) / y;
		ans = (k * x + 2.0 * k2 - 1.0) / (x * x * (k * x + 1.0));
	}
	return (float)ans;
}

/* Fill frac with fractions of way along ring 0 for vertex i, for use with interp_range function */
static void fill_vmesh_fracs(VMesh *vm, float *frac, int i)
{
	int k, ns;
	float total = 0.0f;

	ns = vm->seg;
	frac[0] = 0.0f;
	for (k = 0; k < ns; k++) {
		total += len_v3v3(mesh_vert(vm, i, 0, k)->co, mesh_vert(vm, i, 0, k + 1)->co);
		frac[k + 1] = total;
	}
	if (total > 0.0f) {
		for (k = 1; k <= ns; k++)
			frac[k] /= total;
	}
	else {
		frac[ns] = 1.0f;
	}
}

/* Like fill_vmesh_fracs but want fractions for profile points of bndv, with ns segments */
static void fill_profile_fracs(BevelParams *bp, BoundVert *bndv, float *frac, int ns)
{
	int k;
	float co[3], nextco[3];
	float total = 0.0f;

	frac[0] = 0.0f;
	copy_v3_v3(co, bndv->nv.co);
	for (k = 0; k < ns; k++) {
		get_profile_point(bp, &bndv->profile, k + 1, ns, nextco);
		total += len_v3v3(co, nextco);
		frac[k + 1] = total;
		copy_v3_v3(co, nextco);
	}
	if (total > 0.0f) {
		for (k = 1; k <= ns; k++) {
			frac[k] /= total;
		}
	}
	else {
		frac[ns] = 1.0f;
	}
}

/* Return i such that frac[i] <= f <= frac[i + 1], where frac[n] == 1.0
 * and put fraction of rest of way between frac[i] and frac[i + 1] into r_rest */
static int interp_range(const float *frac, int n, const float f, float *r_rest)
{
	int i;
	float rest;

	/* could binary search in frac, but expect n to be reasonably small */
	for (i = 0; i < n; i++) {
		if (f <= frac[i + 1]) {
			rest = f - frac[i];
			if (rest == 0)
				*r_rest = 0.0f;
			else
				*r_rest = rest / (frac[i + 1] - frac[i]);
			if (i == n - 1 && *r_rest == 1.0f) {
				i = n;
				*r_rest = 0.0f;
			}
			return i;
		}
	}
	*r_rest = 0.0f;
	return n;
}

/* Interpolate given vmesh to make one with target nseg border vertices on the profiles */
static VMesh *interp_vmesh(BevelParams *bp, VMesh *vm0, int nseg)
{
	int n, ns0, nseg2, odd, i, j, k, j0, k0, k0prev, j0inc, k0inc;
	float *prev_frac, *frac, *new_frac, *prev_new_frac;
	float f, restj, restk, restkprev;
	float quad[4][3], co[3], center[3];
	VMesh *vm1;
	BoundVert *bndv;

	n = vm0->count;
	ns0 = vm0->seg;
	nseg2 = nseg / 2;
	odd = nseg % 2;
	vm1 = new_adj_vmesh(bp->mem_arena, n, nseg, vm0->boundstart);

	prev_frac = BLI_array_alloca(prev_frac, (ns0 + 1));
	frac = BLI_array_alloca(frac, (ns0 + 1));
	new_frac = BLI_array_alloca(new_frac, (nseg + 1));
	prev_new_frac = BLI_array_alloca(prev_new_frac, (nseg + 1));

	fill_vmesh_fracs(vm0, prev_frac, n - 1);
	bndv = vm0->boundstart;
	fill_profile_fracs(bp, bndv->prev, prev_new_frac, nseg);
	for (i = 0; i < n; i++) {
		fill_vmesh_fracs(vm0, frac, i);
		fill_profile_fracs(bp, bndv, new_frac, nseg);
		for (j = 0; j <= nseg2 - 1 + odd; j++) {
			for (k = 0; k <= nseg2; k++) {
				f = new_frac[k];
				k0 = interp_range(frac, ns0, f, &restk);
				f = prev_new_frac[nseg - j];
				k0prev = interp_range(prev_frac, ns0, f, &restkprev);
				j0 = ns0 - k0prev;
				restj = -restkprev;
				if (restj > -BEVEL_EPSILON) {
					restj = 0.0f;
				}
				else {
					j0 = j0 - 1;
					restj = 1.0f + restj;
				}
				/* Use bilinear interpolation within the source quad; could be smarter here */
				if (restj < BEVEL_EPSILON && restk < BEVEL_EPSILON) {
					copy_v3_v3(co, mesh_vert_canon(vm0, i, j0, k0)->co);
				}
				else {
					j0inc = (restj < BEVEL_EPSILON || j0 == ns0) ? 0 : 1;
					k0inc = (restk < BEVEL_EPSILON || k0 == ns0) ? 0 : 1;
					copy_v3_v3(quad[0], mesh_vert_canon(vm0, i, j0, k0)->co);
					copy_v3_v3(quad[1], mesh_vert_canon(vm0, i, j0, k0 + k0inc)->co);
					copy_v3_v3(quad[2], mesh_vert_canon(vm0, i, j0 + j0inc, k0 + k0inc)->co);
					copy_v3_v3(quad[3], mesh_vert_canon(vm0, i, j0 + j0inc, k0)->co);
					interp_bilinear_quad_v3(quad, restk, restj, co);
				}
				copy_v3_v3(mesh_vert(vm1, i, j, k)->co, co);
			}
		}
		bndv = bndv->next;
		memcpy(prev_frac, frac, (ns0 + 1) * sizeof(float));
		memcpy(prev_new_frac, new_frac, (nseg + 1) * sizeof(float));
	}
	if (!odd) {
		vmesh_center(vm0, center);
		copy_v3_v3(mesh_vert(vm1, 0, nseg2, nseg2)->co, center);
	}
	vmesh_copy_equiv_verts(vm1);
	return vm1;
}

/* Do one step of cubic subdivision (Catmull-Clark), with special rules at boundaries.
 * For now, this is written assuming vm0->nseg is even and > 0.
 * We are allowed to modify vm0, as it will not be used after this call.
 * See Levin 1999 paper: "Filling an N-sided hole using combined subdivision schemes". */
static VMesh *cubic_subdiv(BevelParams *bp, VMesh *vm0)
{
	int n, ns0, ns20, ns1;
	int i, j, k, inext;
	float co[3], co1[3], co2[3], acc[3];
	float beta, gamma;
	VMesh *vm1;
	BoundVert *bndv;
	
	n = vm0->count;
	ns0 = vm0->seg;
	ns20 = ns0 / 2;
	BLI_assert(ns0 % 2 == 0);
	ns1 = 2 * ns0;
	vm1 = new_adj_vmesh(bp->mem_arena, n, ns1, vm0->boundstart);

	/* First we adjust the boundary vertices of the input mesh, storing in output mesh */
	for (i = 0; i < n; i++) {
		copy_v3_v3(mesh_vert(vm1, i, 0, 0)->co, mesh_vert(vm0, i, 0, 0)->co);
		for (k = 1; k < ns0; k++) {
			/* smooth boundary rule */
			copy_v3_v3(co, mesh_vert(vm0, i, 0, k)->co);
			copy_v3_v3(co1, mesh_vert(vm0, i, 0, k - 1)->co);
			copy_v3_v3(co2, mesh_vert(vm0, i, 0, k + 1)->co);

			add_v3_v3v3(acc, co1, co2);
			madd_v3_v3fl(acc, co, -2.0f);
			madd_v3_v3fl(co, acc, -1.0f / 6.0f);
			
			copy_v3_v3(mesh_vert_canon(vm1, i, 0, 2 * k)->co, co);
		}
	}
	/* now do odd ones in output mesh, based on even ones */
	bndv = vm1->boundstart;
	for (i = 0; i < n; i++) {
		for (k = 1; k < ns1; k += 2) {
			get_profile_point(bp, &bndv->profile, k, ns1, co);
			copy_v3_v3(co1, mesh_vert_canon(vm1, i, 0, k - 1)->co);
			copy_v3_v3(co2, mesh_vert_canon(vm1, i, 0, k + 1)->co);

			add_v3_v3v3(acc, co1, co2);
			madd_v3_v3fl(acc, co, -2.0f);
			madd_v3_v3fl(co, acc, -1.0f / 6.0f);
			
			copy_v3_v3(mesh_vert_canon(vm1, i, 0, k)->co, co);
		}
		bndv = bndv->next;
	}
	vmesh_copy_equiv_verts(vm1);

	/* Copy adjusted verts back into vm0 */
	for (i = 0; i < n; i++) {
		for (k = 0; k < ns0; k++) {
			copy_v3_v3(mesh_vert(vm0, i, 0, k)->co,
			           mesh_vert(vm1, i, 0, 2 * k)->co);
		}
	}

	vmesh_copy_equiv_verts(vm0);

	/* Now we do the internal vertices, using standard Catmull-Clark
	 * and assuming all boundary vertices have valence 4 */
	
	/* The new face vertices */
	for (i = 0; i < n; i++) {
		for (j = 0; j < ns20; j++) {
			for (k = 0; k < ns20; k++) {
				/* face up and right from (j, k) */
				avg4(co,
				     mesh_vert(vm0, i, j, k),
				     mesh_vert(vm0, i, j, k + 1),
				     mesh_vert(vm0, i, j + 1, k),
				     mesh_vert(vm0, i, j + 1, k + 1));
				copy_v3_v3(mesh_vert(vm1, i, 2 * j + 1, 2 * k + 1)->co, co);
			}
		}
	}

	/* The new vertical edge vertices  */
	for (i = 0; i < n; i++) {
		for (j = 0; j < ns20; j++) {
			for (k = 1; k <= ns20; k++) {
				/* vertical edge between (j, k) and (j+1, k) */
				avg4(co, mesh_vert(vm0, i, j, k),
				         mesh_vert(vm0, i, j + 1, k),
				         mesh_vert_canon(vm1, i, 2 * j + 1, 2 * k - 1),
				         mesh_vert_canon(vm1, i, 2 * j + 1, 2 * k + 1));
				copy_v3_v3(mesh_vert(vm1, i, 2 * j + 1, 2 * k)->co, co);
			}
		}
	}

	/* The new horizontal edge vertices  */
	for (i = 0; i < n; i++) {
		for (j = 1; j < ns20; j++) {
			for (k = 0; k < ns20; k++) {
				/* horizontal edge between (j, k) and (j, k+1) */
				avg4(co, mesh_vert(vm0, i, j, k),
				         mesh_vert(vm0, i, j, k + 1),
				         mesh_vert_canon(vm1, i, 2 * j - 1, 2 * k + 1),
				         mesh_vert_canon(vm1, i, 2 * j + 1, 2 * k + 1));
				copy_v3_v3(mesh_vert(vm1, i, 2 * j, 2 * k + 1)->co, co);
			}
		}
	}

	/* The new vertices, not on border */
	gamma = 0.25f;
	beta = -gamma;
	for (i = 0; i < n; i++) {
		for (j = 1; j < ns20; j++) {
			for (k = 1; k <= ns20; k++) {
				/* co1 = centroid of adjacent new edge verts */
				avg4(co1, mesh_vert_canon(vm1, i, 2 * j, 2 * k - 1),
				          mesh_vert_canon(vm1, i, 2 * j, 2 * k + 1),
				          mesh_vert_canon(vm1, i, 2 * j - 1, 2 * k),
				          mesh_vert_canon(vm1, i, 2 * j + 1, 2 * k));
				/* co2 = centroid of adjacent new face verts */
				avg4(co2, mesh_vert_canon(vm1, i, 2 * j - 1, 2 * k - 1),
				          mesh_vert_canon(vm1, i, 2 * j + 1, 2 * k - 1),
				          mesh_vert_canon(vm1, i, 2 * j - 1, 2 * k + 1),
				          mesh_vert_canon(vm1, i, 2 * j + 1, 2 * k + 1));
				/* combine with original vert with alpha, beta, gamma factors */
				copy_v3_v3(co, co1);  /* alpha = 1.0 */
				madd_v3_v3fl(co, co2, beta);
				madd_v3_v3fl(co, mesh_vert(vm0, i, j, k)->co, gamma);
				copy_v3_v3(mesh_vert(vm1, i, 2 * j, 2 * k)->co, co);
			}
		}
	}

	vmesh_copy_equiv_verts(vm1);

	/* The center vertex is special */
	gamma = sabin_gamma(n);
	beta = -gamma;
	/* accumulate edge verts in co1, face verts in co2 */
	zero_v3(co1);
	zero_v3(co2);
	for (i = 0; i < n; i++) {
		add_v3_v3(co1, mesh_vert(vm1, i, ns0, ns0 - 1)->co);
		add_v3_v3(co2, mesh_vert(vm1, i, ns0 - 1, ns0 - 1)->co);
		add_v3_v3(co2, mesh_vert(vm1, i, ns0 - 1, ns0 + 1)->co);
	}
	copy_v3_v3(co, co1);
	mul_v3_fl(co, 1.0f / (float)n);
	madd_v3_v3fl(co, co2, beta / (2.0f * (float)n));
	madd_v3_v3fl(co, mesh_vert(vm0, 0, ns20, ns20)->co, gamma);
	for (i = 0; i < n; i++)
		copy_v3_v3(mesh_vert(vm1, i, ns0, ns0)->co, co);

	/* Final step: sample the boundary vertices at even parameter spacing */
	bndv = vm1->boundstart;
	for (i = 0; i < n; i++) {
		inext = (i + 1) % n;
		for (k = 0; k <= ns1; k++) {
			get_profile_point(bp, &bndv->profile, k, ns1, co);
			copy_v3_v3(mesh_vert(vm1, i, 0, k)->co, co);
			if (k >= ns0 && k < ns1) {
				copy_v3_v3(mesh_vert(vm1, inext, ns1 - k, 0)->co, co);
			}
		}
		bndv = bndv->next;
	}

	return vm1;
}

/* Special case for cube corner, when r is PRO_SQUARE_R,
 * meaning straight sides */
static VMesh *make_cube_corner_straight(MemArena *mem_arena, int nseg)
{
	VMesh *vm;
	float co[3];
	int i, j, k, ns2;

	ns2 = nseg / 2;
	vm = new_adj_vmesh(mem_arena, 3, nseg, NULL);
	vm->count = 0;  // reset, so following loop will end up with correct count
	for (i = 0; i < 3; i++) {
		zero_v3(co);
		co[i] = 1.0f;
		add_new_bound_vert(mem_arena, vm, co);
	}
	for (i = 0; i < 3; i++) {
		for (j = 0; j <= ns2; j++) {
			for (k = 0; k <= ns2; k++) {
				if (!is_canon(vm, i, j, k))
					continue;
				co[i] = 1.0f;
				co[(i + 1) % 3] = (float)k * 2.0f / (float)nseg;
				co[(i + 2) % 3] = (float)j * 2.0f / (float)nseg;
				copy_v3_v3(mesh_vert(vm, i, j, k)->co, co);
			}
		}
	}
	vmesh_copy_equiv_verts(vm);
	return vm;
}

/* Make a VMesh with nseg segments that covers the unit radius sphere octant
 * with center at (0,0,0).
 * This has BoundVerts at (1,0,0), (0,1,0) and (0,0,1), with quarter circle arcs
 * on the faces for the orthogonal planes through the origin.
 */
static VMesh *make_cube_corner_adj_vmesh(BevelParams *bp)
{
	MemArena *mem_arena = bp->mem_arena;
	int nseg = bp->seg;
	float r = bp->pro_super_r;
	VMesh *vm0, *vm1;
	BoundVert *bndv;
	int i, j, k, ns2;
	float co[3], coc[3];

	if (r == PRO_SQUARE_R)
		return make_cube_corner_straight(mem_arena, nseg);

	/* initial mesh has 3 sides, 2 segments */
	vm0 = new_adj_vmesh(mem_arena, 3, 2, NULL);
	vm0->count = 0;  // reset, so following loop will end up with correct count
	for (i = 0; i < 3; i++) {
		zero_v3(co);
		co[i] = 1.0f;
		add_new_bound_vert(mem_arena, vm0, co);
	}
	bndv = vm0->boundstart;
	for (i = 0; i < 3; i++) {
		/* Get point, 1/2 of the way around profile, on arc between this and next */
		coc[i] = 1.0f;
		coc[(i + 1) % 3] = 1.0f;
		coc[(i + 2) % 3] = 0.0f;
		bndv->profile.super_r = r;
		copy_v3_v3(bndv->profile.coa, bndv->nv.co);
		copy_v3_v3(bndv->profile.cob, bndv->next->nv.co);
		copy_v3_v3(bndv->profile.midco, coc);
		copy_v3_v3(mesh_vert(vm0, i, 0, 0)->co, bndv->profile.coa);
		copy_v3_v3(bndv->profile.plane_co, bndv->profile.coa);
		cross_v3_v3v3(bndv->profile.plane_no, bndv->profile.coa, bndv->profile.cob);
		copy_v3_v3(bndv->profile.proj_dir, bndv->profile.plane_no);
		calculate_profile(bp, bndv);
		get_profile_point(bp, &bndv->profile, 1, 2, mesh_vert(vm0, i, 0, 1)->co);
		
		bndv = bndv->next;
	}
	/* center vertex */
	copy_v3_fl(co, M_SQRT1_3);

	if (nseg > 2) {
		if (r > 1.5f)
			mul_v3_fl(co, 1.4f);
		else if (r < 0.75f)
			mul_v3_fl(co, 0.6f);
	}
	copy_v3_v3(mesh_vert(vm0, 0, 1, 1)->co, co);

	vmesh_copy_equiv_verts(vm0);

	vm1 = vm0;
	while (vm1->seg < nseg) {
		vm1 = cubic_subdiv(bp, vm1);
	}
	if (vm1->seg != nseg)
		vm1 = interp_vmesh(bp, vm1, nseg);

	/* Now snap each vertex to the superellipsoid */
	ns2 = nseg / 2;
	for (i = 0; i < 3; i++) {
		for (j = 0; j <= ns2; j++) {
			for (k = 0; k <= nseg; k++) {
				snap_to_superellipsoid(mesh_vert(vm1, i, j, k)->co, r, false);
			}
		}
	}
	return vm1;
}

/* Is this a good candidate for using tri_corner_adj_vmesh? */
static bool tri_corner_test(BevelParams *bp, BevVert *bv)
{
	float ang, totang, angdiff;
	EdgeHalf *e;
	int i;

	if (bv->edgecount != 3 || bv->selcount != 3)
		return false;
	totang = 0.0f;
	for (i = 0; i < 3; i++) {
		e = &bv->edges[i];
		ang = BM_edge_calc_face_angle_signed_ex(e->e, 0.0f);
		if (ang <= (float) M_PI_4 || ang >= 3.0f * (float) M_PI_4)
			return false;
		totang += ang;
	}
	angdiff = fabsf(totang - 3.0f * (float)M_PI_2);
	if ((bp->pro_super_r == PRO_SQUARE_R && angdiff > (float)M_PI / 16.0f) ||
	    (angdiff > (float)M_PI_4))
	{
		return false;
	}
	return true;
}

static VMesh *tri_corner_adj_vmesh(BevelParams *bp, BevVert *bv)
{
	int i, j, k, ns, ns2;
	float co0[3], co1[3], co2[3];
	float mat[4][4], v[4];
	VMesh *vm;
	BoundVert *bndv;

	BLI_assert(bv->edgecount == 3 && bv->selcount == 3);
	bndv = bv->vmesh->boundstart;
	copy_v3_v3(co0, bndv->nv.co);
	bndv = bndv->next;
	copy_v3_v3(co1, bndv->nv.co);
	bndv = bndv->next;
	copy_v3_v3(co2, bndv->nv.co);
	make_unit_cube_map(co0, co1, co2, bv->v->co, mat);
	ns = bp->seg;
	ns2 = ns / 2;
	vm = make_cube_corner_adj_vmesh(bp);
	for (i = 0; i < 3; i++) {
		for (j = 0; j <= ns2; j++) {
			for (k = 0; k <= ns; k++) {
				copy_v3_v3(v, mesh_vert(vm, i, j, k)->co);
				v[3] = 1.0f;
				mul_m4_v4(mat, v);
				copy_v3_v3(mesh_vert(vm, i, j, k)->co, v);
			}
		}
	}

	return vm;
}

static VMesh *adj_vmesh(BevelParams *bp, BevVert *bv)
{
	int n, ns, i;
	VMesh *vm0, *vm1;
	float co[3], coa[3], cob[3], dir[3];
	BoundVert *bndv;
	MemArena *mem_arena = bp->mem_arena;
	float r, fac, fullness;

	/* First construct an initial control mesh, with nseg==2 */
	n = bv->vmesh->count;
	ns = bv->vmesh->seg;
	vm0 = new_adj_vmesh(mem_arena, n, 2, bv->vmesh->boundstart);

	bndv = vm0->boundstart;
	zero_v3(co);
	for (i = 0; i < n; i++) {
		/* Boundaries just divide input polygon edges into 2 even segments */
		copy_v3_v3(mesh_vert(vm0, i, 0, 0)->co, bndv->nv.co);
		get_profile_point(bp, &bndv->profile, 1, 2, mesh_vert(vm0, i, 0, 1)->co);
		add_v3_v3(co, bndv->nv.co);
		bndv = bndv->next;
	}
	/* To place center vertex:
	 * coa is original vertex
	 * co is centroid of boundary corners
	 * cob is reflection of coa in across co.
	 * Calculate 'fullness' = fraction of way
	 * from co to coa (if positive) or to cob (if negative).
	 */
	copy_v3_v3(coa, bv->v->co);
	mul_v3_fl(co, 1.0f / (float)n);
	sub_v3_v3v3(cob, co, coa);
	add_v3_v3(cob, co);
	r = bp->pro_super_r;
	if (r == 1.0f)
		fullness = 0.0f;
	else if (r > 1.0f) {
		if (bp->vertex_only)
			fac = 0.25f;
		else if (r == PRO_SQUARE_R)
			fac = -2.0;
		else
			fac = 0.5f;
		fullness = 1.0f - fac / r;
	}
	else {
		fullness = r - 1.0f;
	}
	sub_v3_v3v3(dir, coa, co);
	if (len_squared_v3(dir) > BEVEL_EPSILON_SQ)
		madd_v3_v3fl(co, dir, fullness);
	copy_v3_v3(mesh_vert(vm0, 0, 1, 1)->co, co);
	vmesh_copy_equiv_verts(vm0);

	vm1 = vm0;
	do {
		vm1 = cubic_subdiv(bp, vm1);
	} while (vm1->seg < ns);
	if (vm1->seg != ns)
		vm1 = interp_vmesh(bp, vm1, ns);
	return vm1;
}

/* Snap co to the closest point on the profile for vpipe projected onto the plane
 * containing co with normal in the direction of edge vpipe->ebev.
 * For the square profiles, need to decide whether to snap to just one plane
 * or to the midpoint of the profile; do so if midline is true. */
static void snap_to_pipe_profile(BoundVert *vpipe, bool midline, float co[3])
{
	float va[3], vb[3], edir[3], va0[3], vb0[3], vmid0[3];
	float plane[4], m[4][4], minv[4][4], p[3], snap[3];
	Profile *pro = &vpipe->profile;
	EdgeHalf *e = vpipe->ebev;

	copy_v3_v3(va, pro->coa);
	copy_v3_v3(vb, pro->cob);

	sub_v3_v3v3(edir, e->e->v1->co, e->e->v2->co);

	plane_from_point_normal_v3(plane, co, edir);
	closest_to_plane_v3(va0, plane, va);
	closest_to_plane_v3(vb0, plane, vb);
	closest_to_plane_v3(vmid0, plane, pro->midco);
	if (make_unit_square_map(va0, vmid0, vb0, m)) {
		/* Transform co and project it onto superellipse */
		if (!invert_m4_m4(minv, m)) {
			/* shouldn't happen */
			BLI_assert(!"failed inverse during pipe profile snap");
			return;
		}
		mul_v3_m4v3(p, minv, co);
		snap_to_superellipsoid(p, pro->super_r, midline);
		mul_v3_m4v3(snap, m, p);
		copy_v3_v3(co, snap);
	}
	else {
		/* planar case: just snap to line va0--vb0 */
		closest_to_line_segment_v3(p, co, va0, vb0);
		copy_v3_v3(co, p);
	}
}

/* See pipe_test for conditions that make 'pipe'; vpipe is the return value from that.
 * We want to make an ADJ mesh but then snap the vertices to the profile in a plane
 * perpendicular to the pipes.
 * A tricky case is for the 'square' profiles and an even nseg: we want certain vertices
 * to snap to the midline on the pipe, not just to one plane or the other. */
static VMesh *pipe_adj_vmesh(BevelParams *bp, BevVert *bv, BoundVert *vpipe)
{
	int i, j, k, n, ns, ns2, ipipe1, ipipe2;
	VMesh *vm;
	bool even, midline;

	vm = adj_vmesh(bp, bv);

	/* Now snap all interior coordinates to be on the epipe profile */
	n = bv->vmesh->count;
	ns = bv->vmesh->seg;
	ns2 = ns / 2;
	even = (ns % 2) == 0;
	ipipe1 = vpipe->index;
	ipipe2 = vpipe->next->next->index;
	for (i = 0; i < n; i++) {
		for (j = 1; j <= ns2; j++) {
			for (k = 0; k <= ns2; k++) {
				if (!is_canon(vm, i, j, k))
					continue;
				midline = even && k == ns2 &&
				          ((i == 0 && j == ns2) || (i == ipipe1 || i == ipipe2));
				snap_to_pipe_profile(vpipe, midline, mesh_vert(vm, i, j, k)->co);
			}
		}
	}

	return vm;
}

static void get_incident_edges(BMFace *f, BMVert *v, BMEdge **r_e1, BMEdge **r_e2)
{
	BMIter iter;
	BMEdge *e;

	*r_e1 = NULL;
	*r_e2 = NULL;
	if (!f)
		return;
	BM_ITER_ELEM (e, &iter, f, BM_EDGES_OF_FACE) {
		if (e->v1 == v || e->v2 == v) {
			if (*r_e1 == NULL)
				*r_e1 = e;
			else if (*r_e2 == NULL)
				*r_e2 = e;
		}
	}
}

static BMEdge *find_closer_edge(float *co, BMEdge *e1, BMEdge *e2)
{
	float dsq1, dsq2;

	BLI_assert(e1 != NULL && e2 != NULL);
	dsq1 = dist_squared_to_line_segment_v3(co, e1->v1->co, e1->v2->co);
	dsq2 = dist_squared_to_line_segment_v3(co, e2->v1->co, e2->v2->co);
	if (dsq1 < dsq2)
		return e1;
	else
		return e2;
}

/* Snap co to the closest edge of face f. Return the edge in *r_snap_e,
 * the coordinates of snap point in r_ snap_co,
 * and the distance squared to the snap point as function return */
static float snap_face_dist_squared(float *co, BMFace *f, BMEdge **r_snap_e, float *r_snap_co)
{
	BMIter iter;
	BMEdge *beste = NULL;
	float d2, beste_d2;
	BMEdge *e;
	float closest[3];

	beste_d2 = 1e20;
	BM_ITER_ELEM(e, &iter, f, BM_EDGES_OF_FACE) {
		closest_to_line_segment_v3(closest, co, e->v1->co, e->v2->co);
		d2 = len_squared_v3v3(closest, co);
		if (d2 < beste_d2) {
			beste_d2 = d2;
			beste = e;
			copy_v3_v3(r_snap_co, closest);
		}
	}
	*r_snap_e = beste;
	return beste_d2;
}

/*
 * Given that the boundary is built and the boundary BMVerts have been made,
 * calculate the positions of the interior mesh points for the M_ADJ pattern,
 * using cubic subdivision, then make the BMVerts and the new faces. */
static void bevel_build_rings(BevelParams *bp, BMesh *bm, BevVert *bv)
{
	int n, ns, ns2, odd, i, j, k, ring;
	VMesh *vm1, *vm;
	BoundVert *v;
	BMVert *bmv1, *bmv2, *bmv3, *bmv4;
	BMFace *f, *f2;
	BMEdge *bme, *bme1, *bme2, *bme3;
	EdgeHalf *e;
	BoundVert *vpipe;
	int mat_nr = bp->mat_nr;

	n = bv->vmesh->count;
	ns = bv->vmesh->seg;
	ns2 = ns / 2;
	odd = ns % 2;
	BLI_assert(n >= 3 && ns > 1);

	vpipe = pipe_test(bv);

	if (vpipe)
		vm1 = pipe_adj_vmesh(bp, bv, vpipe);
	else if (tri_corner_test(bp, bv))
		vm1 = tri_corner_adj_vmesh(bp, bv);
	else
		vm1 = adj_vmesh(bp, bv);

	/* copy final vmesh into bv->vmesh, make BMVerts and BMFaces */
	vm = bv->vmesh;
	for (i = 0; i < n; i++) {
		for (j = 0; j <= ns2; j++) {
			for (k = 0; k <= ns; k++) {
				if (j == 0 && (k == 0 || k == ns))
					continue;  /* boundary corners already made */
				if (!is_canon(vm, i, j, k))
					continue;
				copy_v3_v3(mesh_vert(vm, i, j, k)->co, mesh_vert(vm1, i, j, k)->co);
				create_mesh_bmvert(bm, vm, i, j, k, bv->v);
			}
		}
	}
	vmesh_copy_equiv_verts(vm);
	/* make the polygons */
	v = vm->boundstart;
	do {
		i = v->index;
		f = boundvert_rep_face(v, NULL);
		f2 = boundvert_rep_face(v->next, NULL);
		if (bp->vertex_only)
			e = v->efirst;
		else
			e = v->ebev;
		BLI_assert(e != NULL);
		bme = e->e;
		/* For odd ns, make polys with lower left corner at (i,j,k) for
		 *    j in [0, ns2-1], k in [0, ns2].  And then the center ngon.
		 * For even ns,
		 *    j in [0, ns2-1], k in [0, ns2-1] */
		for (j = 0; j < ns2; j++) {
			for (k = 0; k < ns2 + odd; k++) {
				bmv1 = mesh_vert(vm, i, j, k)->v;
				bmv2 = mesh_vert(vm, i, j, k + 1)->v;
				bmv3 = mesh_vert(vm, i, j + 1, k + 1)->v;
				bmv4 = mesh_vert(vm, i, j + 1, k)->v;
				BLI_assert(bmv1 && bmv2 && bmv3 && bmv4);
				if (bp->vertex_only) {
					if (j < k) {
						if (k == ns2 && j == ns2 - 1) {
							bev_create_quad_ex(bm, bmv1, bmv2, bmv3, bmv4, f2, f2, f2, f2,
							                   NULL, NULL, v->next->efirst->e, bme, mat_nr);
						}
						else {
							bev_create_quad(bm, bmv1, bmv2, bmv3, bmv4, f2, f2, f2, f2, mat_nr);
						}
					}
					else if (j > k) {
						bev_create_quad(bm, bmv1, bmv2, bmv3, bmv4, f2, f2, f2, f2, mat_nr);
					}
					else { /* j == k */
						/* only one edge attached to v, since vertex_only */
						if (e->is_seam) {
							bev_create_quad_ex(bm, bmv1, bmv2, bmv3, bmv4, f2, f2, f2, f2,
							                   bme, NULL, bme, NULL, mat_nr);
						}
						else {
							bev_create_quad_ex(bm, bmv1, bmv2, bmv3, bmv4, f2, f2, f2, f,
							                   bme, NULL, bme, NULL, mat_nr);
						}
					}
				}
				else { /* edge bevel */
					if (odd) {
						if (k == ns2) {
							if (e->is_seam) {
								bev_create_quad_ex(bm, bmv1, bmv2, bmv3, bmv4, f, f, f, f,
								                   NULL, bme, bme, NULL, mat_nr);
							}
							else {
								bev_create_quad(bm, bmv1, bmv2, bmv3, bmv4, f, f2, f2, f, mat_nr);
							}
						}
						else {
							bev_create_quad(bm, bmv1, bmv2, bmv3, bmv4, f, f, f, f, mat_nr);
						}
					}
					else {
						bme1 = k == ns2 - 1 ? bme : NULL;
						bme3 = j == ns2 - 1 ? v->prev->ebev->e : NULL;
						bme2 = bme1 != NULL ? bme1 : bme3;
						bev_create_quad_ex(bm, bmv1, bmv2, bmv3, bmv4, f, f, f, f,
						                   NULL, bme1, bme2, bme3, mat_nr);
					}
				}
			}
		}
	} while ((v = v->next) != vm->boundstart);

	/* Fix UVs along center lines if even number of segments */
	if (!odd) {
		v = vm->boundstart;
		do {
			i = v->index;
			if (!v->any_seam) {
				for (ring = 1; ring < ns2; ring++) {
					BMVert *v_uv = mesh_vert(vm, i, ring, ns2)->v;
					if (v_uv) {
						bev_merge_uvs(bm, v_uv);
					}
				}
			}
		} while ((v = v->next) != vm->boundstart);
		bmv1 = mesh_vert(vm, 0, ns2, ns2)->v;
		if (bp->vertex_only || count_bound_vert_seams(bv) <= 1)
			bev_merge_uvs(bm, bmv1);
	}

	/* center ngon */
	if (odd) {
		BMFace *frep;
		BMEdge *frep_e1, *frep_e2, *frep_e;
		BMVert **vv = NULL;
		BMFace **vf = NULL;
		BMEdge **ve = NULL;
		BLI_array_staticdeclare(vv, BM_DEFAULT_NGON_STACK_SIZE);
		BLI_array_staticdeclare(vf, BM_DEFAULT_NGON_STACK_SIZE);
		BLI_array_staticdeclare(ve, BM_DEFAULT_NGON_STACK_SIZE);

		if (bv->any_seam) {
			frep = boundvert_rep_face(vm->boundstart, NULL);
			get_incident_edges(frep, bv->v, &frep_e1, &frep_e2);
		}
		else {
			frep = NULL;
			frep_e1 = frep_e2 = NULL;
		}
		v = vm->boundstart;
		do {
			i = v->index;
			BLI_array_append(vv, mesh_vert(vm, i, ns2, ns2)->v);
			if (frep) {
				BLI_array_append(vf, frep);
				frep_e = find_closer_edge(mesh_vert(vm, i, ns2, ns2)->v->co, frep_e1, frep_e2);
				BLI_array_append(ve, v == vm->boundstart ? NULL : frep_e);
			}
			else {
				BLI_array_append(vf, boundvert_rep_face(v, NULL));
				BLI_array_append(ve, NULL);
			}
		} while ((v = v->next) != vm->boundstart);
		bev_create_ngon(bm, vv, BLI_array_count(vv), vf, frep, ve, mat_nr, true);

		BLI_array_free(vv);
		BLI_array_free(vf);
		BLI_array_free(ve);
	}
}

/* If we make a poly out of verts around bv, snapping to rep frep, will uv poly have zero area?
 * The uv poly is made by snapping all outside-of-frep vertices to the closest edge in frep.
 * Assume that this funciton is called when the only inside-of-frep vertex is vm->boundstart.
 * The poly will have zero area if the distance of that first vertex to some edge e is zero, and all
 * the other vertices snap to e or snap to an edge at a point that is essentially on e too.  */
static bool is_bad_uv_poly(BevVert *bv, BMFace *frep)
{
	BoundVert *v;
	BMEdge *snape, *firste;
	float co[3];
	VMesh *vm = bv->vmesh;
	float d2;

	v = vm->boundstart;
	d2 = snap_face_dist_squared(v->nv.v->co, frep, &firste, co);
	if (d2 > BEVEL_EPSILON_BIG_SQ || firste == NULL)
		return false;

	for (v = v->next; v != vm->boundstart; v = v->next) {
		snap_face_dist_squared(v->nv.v->co, frep, &snape, co);
		if (snape  != firste) {
			d2 = dist_to_line_v3(co, firste->v1->co, firste->v2->co);
			if (d2 > BEVEL_EPSILON_BIG_SQ)
				return false;
		}
	}
	return true;
}

static BMFace *bevel_build_poly(BevelParams *bp, BMesh *bm, BevVert *bv)
{
	BMFace *f, *frep, *frep2;
	int n, k;
	VMesh *vm = bv->vmesh;
	BoundVert *v;
	BMEdge *frep_e1, *frep_e2, *frep_e;
	BMVert **vv = NULL;
	BMFace **vf = NULL;
	BMEdge **ve = NULL;
	BLI_array_staticdeclare(vv, BM_DEFAULT_NGON_STACK_SIZE);
	BLI_array_staticdeclare(vf, BM_DEFAULT_NGON_STACK_SIZE);
	BLI_array_staticdeclare(ve, BM_DEFAULT_NGON_STACK_SIZE);

	if (bv->any_seam) {
		frep = boundvert_rep_face(vm->boundstart, &frep2);
		if (frep2 && frep && is_bad_uv_poly(bv, frep)) {
			frep = frep2;
		}
		get_incident_edges(frep, bv->v, &frep_e1, &frep_e2);
	}
	else {
		frep = NULL;
		frep_e1 = frep_e2 = NULL;
	}
	v = vm->boundstart;
	n = 0;
	do {
		/* accumulate vertices for vertex ngon */
		/* also accumulate faces in which uv interpolation is to happen for each */
		BLI_array_append(vv, v->nv.v);
		if (frep) {
			BLI_array_append(vf, frep);
			frep_e = find_closer_edge(v->nv.v->co, frep_e1, frep_e2);
			BLI_array_append(ve, n > 0 ? frep_e : NULL);
		}
		else {
			BLI_array_append(vf, boundvert_rep_face(v, NULL));
			BLI_array_append(ve, NULL);
		}
		n++;
		if (v->ebev && v->ebev->seg > 1) {
			for (k = 1; k < v->ebev->seg; k++) {
				BLI_array_append(vv, mesh_vert(vm, v->index, 0, k)->v);
				if (frep) {
					BLI_array_append(vf, frep);
					frep_e = find_closer_edge(mesh_vert(vm, v->index, 0, k)->v->co, frep_e1, frep_e2);
					BLI_array_append(ve, k < v->ebev->seg / 2 ? NULL : frep_e);
				}
				else {
					BLI_array_append(vf, boundvert_rep_face(v, NULL));
					BLI_array_append(ve, NULL);
				}
				n++;
			}
		}
	} while ((v = v->next) != vm->boundstart);
	if (n > 2) {
		f = bev_create_ngon(bm, vv, n, vf, frep, ve, bp->mat_nr, true);
	}
	else {
		f = NULL;
	}
	BLI_array_free(vv);
	BLI_array_free(vf);
	BLI_array_free(ve);
	return f;
}

static void bevel_build_trifan(BevelParams *bp, BMesh *bm, BevVert *bv)
{
	BMFace *f;
	BLI_assert(next_bev(bv, NULL)->seg == 1 || bv->selcount == 1);

	f = bevel_build_poly(bp, bm, bv);

	if (f) {
		/* we have a polygon which we know starts at the previous vertex, make it into a fan */
		BMLoop *l_fan = BM_FACE_FIRST_LOOP(f)->prev;
		BMVert *v_fan = l_fan->v;

		while (f->len > 3) {
			BMLoop *l_new;
			BMFace *f_new;
			BLI_assert(v_fan == l_fan->v);
			f_new = BM_face_split(bm, f, l_fan, l_fan->next->next, &l_new, NULL, false);
			flag_out_edge(bm, l_new->e);

			if (f_new->len > f->len) {
				f = f_new;
				if      (l_new->v       == v_fan) { l_fan = l_new; }
				else if (l_new->next->v == v_fan) { l_fan = l_new->next; }
				else if (l_new->prev->v == v_fan) { l_fan = l_new->prev; }
				else { BLI_assert(0); }
			}
			else {
				if      (l_fan->v       == v_fan) { /* l_fan = l_fan; */ }
				else if (l_fan->next->v == v_fan) { l_fan = l_fan->next; }
				else if (l_fan->prev->v == v_fan) { l_fan = l_fan->prev; }
				else { BLI_assert(0); }
			}
		}
	}
}

static void bevel_build_quadstrip(BevelParams *bp, BMesh *bm, BevVert *bv)
{
	BMFace *f;
	BLI_assert(bv->selcount == 2);

	f = bevel_build_poly(bp, bm, bv);

	if (f) {
		/* we have a polygon which we know starts at this vertex, make it into strips */
		EdgeHalf *eh_a = bv->vmesh->boundstart->elast;
		EdgeHalf *eh_b = next_bev(bv, eh_a->next);  /* since (selcount == 2) we know this is valid */
		BMLoop *l_a = BM_face_vert_share_loop(f, eh_a->rightv->nv.v);
		BMLoop *l_b = BM_face_vert_share_loop(f, eh_b->leftv->nv.v);
		int split_count = bv->vmesh->seg + 1;  /* ensure we don't walk past the segments */

		while (f->len > 4 && split_count > 0) {
			BMLoop *l_new;
			BLI_assert(l_a->f == f);
			BLI_assert(l_b->f == f);

			if (l_a-> v == l_b->v || l_a->next == l_b) {
				/* l_a->v and l_b->v can be the same or such that we'd make a 2-vertex poly */
				l_a = l_a->prev;
				l_b = l_b->next;
			}
			else {
				BM_face_split(bm, f, l_a, l_b, &l_new, NULL, false);
				f = l_new->f;
				flag_out_edge(bm, l_new->e);

				/* walk around the new face to get the next verts to split */
				l_a = l_new->prev;
				l_b = l_new->next->next;
			}
			split_count--;
		}
	}
}

/* Special case: vertex bevel with only two boundary verts.
 * Want to make a curved edge if seg > 0.
 * If there are no faces in the original mesh at the original vertex,
 * there will be no rebuilt face to make the edge between the boundary verts,
 * we have to make it here. */
static void bevel_vert_two_edges(BevelParams *bp, BMesh *bm, BevVert *bv)
{
	VMesh *vm = bv->vmesh;
	BMVert *v1, *v2;
	BMEdge *e_eg, *bme;
	Profile *pro;
	float co[3];
	BoundVert *bndv;
	int ns, k;

	BLI_assert(vm->count == 2 && bp->vertex_only);

	v1 = mesh_vert(vm, 0, 0, 0)->v;
	v2 = mesh_vert(vm, 1, 0, 0)->v;

	ns = vm->seg;
	if (ns > 1) {
		/* Set up profile parameters */
		bndv = vm->boundstart;
		pro = &bndv->profile;
		pro->super_r = bp->pro_super_r;
		copy_v3_v3(pro->coa, v1->co);
		copy_v3_v3(pro->cob, v2->co);
		copy_v3_v3(pro->midco, bv->v->co);
		/* don't use projection */
		zero_v3(pro->plane_co);
		zero_v3(pro->plane_no);
		zero_v3(pro->proj_dir);
		calculate_profile(bp, bndv);
		for (k = 1; k < ns; k++) {
			get_profile_point(bp, pro, k, ns, co);
			copy_v3_v3(mesh_vert(vm, 0, 0, k)->co, co);
			create_mesh_bmvert(bm, vm, 0, 0, k, bv->v);
		}
		copy_v3_v3(mesh_vert(vm, 0, 0, ns)->co, v2->co);
		for (k = 1; k < ns; k++)
			copy_mesh_vert(vm, 1, 0, ns - k, 0, 0, k);
	}

	if (BM_vert_face_check(bv->v) == false) {
		e_eg = bv->edges[0].e;
		BLI_assert(e_eg != NULL);
		for (k = 0; k < ns; k++) {
			v1 = mesh_vert(vm, 0, 0, k)->v;
			v2 = mesh_vert(vm, 0, 0, k + 1)->v;
			BLI_assert(v1 != NULL && v2 != NULL);
			bme = BM_edge_create(bm, v1, v2, e_eg, BM_CREATE_NO_DOUBLE);
			if (bme)
				flag_out_edge(bm, bme);
		}
	}
}

/* Given that the boundary is built, now make the actual BMVerts
 * for the boundary and the interior of the vertex mesh. */
static void build_vmesh(BevelParams *bp, BMesh *bm, BevVert *bv)
{
	MemArena *mem_arena = bp->mem_arena;
	VMesh *vm = bv->vmesh;
	BoundVert *v, *weld1, *weld2;
	int n, ns, ns2, i, k, weld;
	float *va, *vb, co[3];

	n = vm->count;
	ns = vm->seg;
	ns2 = ns / 2;

	vm->mesh = (NewVert *)BLI_memarena_alloc(mem_arena, n * (ns2 + 1) * (ns + 1) * sizeof(NewVert));

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
			else {
				weld2 = v;
				move_weld_profile_planes(bv, weld1, weld2);
				calculate_profile(bp, weld1);
				calculate_profile(bp, weld2);
			}
		}
	} while ((v = v->next) != vm->boundstart);

	/* copy other ends to (i, 0, ns) for all i, and fill in profiles for edges */
	v = vm->boundstart;
	do {
		i = v->index;
		copy_mesh_vert(vm, i, 0, ns, v->next->index, 0, 0);
		for (k = 1; k < ns; k++) {
			if (v->ebev && vm->mesh_kind != M_ADJ) {
				get_profile_point(bp, &v->profile, k, ns, co);
				copy_v3_v3(mesh_vert(vm, i, 0, k)->co, co);
				if (!weld)
					create_mesh_bmvert(bm, vm, i, 0, k, bv->v);
			}
			else if (n == 2 && !v->ebev && vm->mesh_kind != M_ADJ) {
				/* case of one edge beveled and this is the v without ebev */
				/* want to copy the verts from other v, in reverse order */
				copy_mesh_vert(vm, i, 0, k, 1 - i, 0, ns - k);
			}
		}
	} while ((v = v->next) != vm->boundstart);

	if (weld) {
		vm->mesh_kind = M_NONE;
		for (k = 1; k < ns; k++) {
			va = mesh_vert(vm, weld1->index, 0, k)->co;
			vb = mesh_vert(vm, weld2->index, 0, ns - k)->co;
			/* if one of the profiles is on a flat plane,
			 * just use the boundary point of the other */
			if (weld1->profile.super_r == PRO_LINE_R &&
			    weld2->profile.super_r != PRO_LINE_R)
			{
				copy_v3_v3(co, vb);
			}
			else if (weld2->profile.super_r == PRO_LINE_R &&
			         weld1->profile.super_r != PRO_LINE_R)
			{
				copy_v3_v3(co, va);
			}
			else {
				mid_v3_v3v3(co, va, vb);
			}
			copy_v3_v3(mesh_vert(vm, weld1->index, 0, k)->co, co);
			create_mesh_bmvert(bm, vm, weld1->index, 0, k, bv->v);
		}
		for (k = 1; k < ns; k++)
			copy_mesh_vert(vm, weld2->index, 0, ns - k, weld1->index, 0, k);
	}

	switch (vm->mesh_kind) {
		case M_NONE:
			if  (n == 2 && bp->vertex_only)
				bevel_vert_two_edges(bp, bm, bv);
			break;
		case M_POLY:
			bevel_build_poly(bp, bm, bv);
			break;
		case M_ADJ:
			bevel_build_rings(bp, bm, bv);
			break;
		case M_TRI_FAN:
			bevel_build_trifan(bp, bm, bv);
			break;
		case M_QUAD_STRIP:
			bevel_build_quadstrip(bp, bm, bv);
			break;
	}
}

/* Return the angle between the two faces adjacent to e.
 * If there are not two, return 0. */
static float edge_face_angle(EdgeHalf *e)
{
	if (e->fprev && e->fnext) {
		/* angle between faces is supplement of angle between face normals */
		return (float)M_PI - angle_normalized_v3v3(e->fprev->no, e->fnext->no);
	}
	else {
		return 0.0f;
	}
}

/* take care, this flag isn't cleared before use, it just so happens that its not set */
#define BM_BEVEL_EDGE_TAG_ENABLE(bme)  BM_ELEM_API_FLAG_ENABLE(  (bme), _FLAG_OVERLAP)
#define BM_BEVEL_EDGE_TAG_DISABLE(bme) BM_ELEM_API_FLAG_DISABLE( (bme), _FLAG_OVERLAP)
#define BM_BEVEL_EDGE_TAG_TEST(bme)    BM_ELEM_API_FLAG_TEST(    (bme), _FLAG_OVERLAP)

/* Try to extend the bv->edges[] array beyond i by finding more successor edges.
 * This is a possibly exponential-time search, but it is only exponential in the number
 * of "internal faces" at a vertex -- i.e., faces that bridge between the edges that naturally
 * form a manifold cap around bv. It is rare to have more than one of these, so unlikely
 * that the exponential time case will be hit in practice.
 * Returns the new index i' where bv->edges[i'] ends the best path found.
 * The path will have the tags of all of its edges set. */
static int bevel_edge_order_extend(BMesh *bm, BevVert *bv, int i)
{
	BMEdge *bme, *bme2, *nextbme;
	BMLoop *l;
	BMIter iter;
	int j, tryj, bestj, nsucs, sucindex, k;
	BMEdge **sucs = NULL;
	BMEdge **save_path = NULL;
	BLI_array_staticdeclare(sucs, 4);  /* likely very few faces attached to same edge */
	BLI_array_staticdeclare(save_path, BM_DEFAULT_NGON_STACK_SIZE);

	bme = bv->edges[i].e;
	/* fill sucs with all unmarked edges of bmes */
	BM_ITER_ELEM(l, &iter, bme, BM_LOOPS_OF_EDGE) {
		bme2 = (l->v == bv->v) ? l->prev->e : l->next->e;
		if (!BM_BEVEL_EDGE_TAG_TEST(bme2)) {
			BLI_array_append(sucs, bme2);
		}
	}
	nsucs = BLI_array_count(sucs);

	bestj = j = i;
	for (sucindex = 0; sucindex < nsucs; sucindex++) {
		nextbme = sucs[sucindex];
		BLI_assert(nextbme != NULL);
		BLI_assert(!BM_BEVEL_EDGE_TAG_TEST(nextbme));
		BLI_assert(j + 1 < bv->edgecount);
		bv->edges[j + 1].e = nextbme;
		BM_BEVEL_EDGE_TAG_ENABLE(nextbme);
		tryj = bevel_edge_order_extend(bm, bv, j + 1);
		if (tryj > bestj || (tryj == bestj && edges_face_connected_at_vert(bv->edges[tryj].e, bv->edges[0].e))) {
			bestj = tryj;
			BLI_array_empty(save_path);
			for (k = j + 1; k <= bestj; k++) {
				BLI_array_append(save_path, bv->edges[k].e);
			}
		}
		/* now reset to path only-going-to-j state */
		for (k = j + 1; k <= tryj; k++) {
			BM_BEVEL_EDGE_TAG_DISABLE(bv->edges[k].e);
			bv->edges[k].e = NULL;
		}
	}
	/* at this point we should be back at invariant on entrance: path up to j */
	if (bestj > j) {
		/* save_path should have from j + 1 to bestj inclusive edges to add to edges[] before returning */
		for (k = j + 1; k <= bestj; k++) {
			BLI_assert(save_path[k - (j + 1)] != NULL);
			bv->edges[k].e = save_path[k - (j + 1)];
			BM_BEVEL_EDGE_TAG_ENABLE(bv->edges[k].e);
		}
	}
	BLI_array_free(sucs);
	BLI_array_free(save_path);
	return bestj;
}

/* See if we have usual case for bevel edge order:
 * there is an ordering such that all the faces are between
 * successive edges and form a manifold "cap" at bv.
 * If this is the case, set bv->edges to such an order
 * and return true; else return unmark any partial path and return false.
 * Assume the first edge is already in bv->edges[0].e and it is tagged. */
#ifdef FASTER_FASTORDER
/* The alternative older code is O(n^2) where n = # of edges incident to bv->v.
 * This implementation is O(n * m) where m = average number of faces attached to an edge incident to bv->v,
 * which is almost certainly a small constant except in very strange cases. But this code produces different
 * choices of ordering than the legacy system, leading to differences in vertex orders etc. in user models,
 * so for now will continue to use the legacy code. */
static bool fast_bevel_edge_order(BevVert *bv)
{
	int j, k, nsucs;
	BMEdge *bme, *bme2, *bmenext;
	BMIter iter;
	BMLoop *l;

	for (j = 1; j < bv->edgecount; j++) {
		bme = bv->edges[j - 1].e;
		bmenext = NULL;
		nsucs = 0;
		BM_ITER_ELEM(l, &iter, bme, BM_LOOPS_OF_EDGE) {
			bme2 = (l->v == bv->v) ? l->prev->e : l->next->e;
			if (!BM_BEVEL_EDGE_TAG_TEST(bme2)) {
				nsucs++;
				if (bmenext == NULL)
					bmenext = bme2;
			}
		}
		if (nsucs == 0 || (nsucs == 2 && j != 1) || nsucs > 2 ||
		    (j == bv->edgecount - 1 && !edges_face_connected_at_vert(bmenext, bv->edges[0].e)))
		{
			for (k = 1; k < j; k++) {
				BM_BEVEL_EDGE_TAG_DISABLE(bv->edges[k].e);
				bv->edges[k].e = NULL;
			}
			return false;
		}
		bv->edges[j].e = bmenext;
		BM_BEVEL_EDGE_TAG_ENABLE(bmenext);
	}
	return true;
}
#else
static bool fast_bevel_edge_order(BevVert *bv)
{
	BMEdge *bme, *bme2, *first_suc;
	BMIter iter, iter2;
	BMFace *f;
	EdgeHalf *e;
	int i, k, ntot, num_shared_face;

	ntot = bv->edgecount;

	/* add edges to bv->edges in order that keeps adjacent edges sharing
	 * a unique face, if possible */
	e = &bv->edges[0];
	bme = e->e;
	if (!bme->l)
		return false;
	for (i = 1; i < ntot; i++) {
		/* find an unflagged edge bme2 that shares a face f with previous bme */
		num_shared_face = 0;
		first_suc = NULL;  /* keep track of first successor to match legacy behavior */
		BM_ITER_ELEM (bme2, &iter, bv->v, BM_EDGES_OF_VERT) {
			if (BM_BEVEL_EDGE_TAG_TEST(bme2))
				continue;
			BM_ITER_ELEM (f, &iter2, bme2, BM_FACES_OF_EDGE) {
				if (BM_face_edge_share_loop(f, bme)) {
					num_shared_face++;
					if (first_suc == NULL)
						first_suc = bme2;
				}
			}
			if (num_shared_face >= 3)
				break;
		}
		if (num_shared_face == 1 || (i == 1 && num_shared_face == 2)) {
			e = &bv->edges[i];
			e->e = bme = first_suc;
			BM_BEVEL_EDGE_TAG_ENABLE(bme);
		}
		else {
			for (k = 1; k < i; k++) {
				BM_BEVEL_EDGE_TAG_DISABLE(bv->edges[k].e);
				bv->edges[k].e = NULL;
			}
			return false;
		}
	}
	return true;
}
#endif

/* Fill in bv->edges with a good ordering of non-wire edges around bv->v.
 * Use only edges where BM_BEVEL_EDGE_TAG is disabled so far
 * (if edge beveling, others are wire).
 * first_bme is a good edge to start with.*/
static void find_bevel_edge_order(BMesh *bm, BevVert *bv, BMEdge *first_bme)
{
	BMEdge *bme, *bme2;
	BMIter iter;
	BMFace *f, *bestf;
	EdgeHalf *e;
	EdgeHalf *e2;
	BMLoop *l;
	int i, ntot;

	ntot = bv->edgecount;
	i = 0;
	for (;;) {
		BLI_assert(first_bme != NULL);
		bv->edges[i].e = first_bme;
		BM_BEVEL_EDGE_TAG_ENABLE(first_bme);
		if (i == 0 && fast_bevel_edge_order(bv))
			break;
		i = bevel_edge_order_extend(bm, bv, i);
		i++;
		if (i >= bv->edgecount)
			break;
		/* Not done yet: find a new first_bme */
		first_bme = NULL;
		BM_ITER_ELEM(bme, &iter, bv->v, BM_EDGES_OF_VERT) {
			if (BM_BEVEL_EDGE_TAG_TEST(bme))
				continue;
			if (!first_bme)
				first_bme = bme;
			if (BM_edge_face_count(bme) == 1) {
				first_bme = bme;
				break;
			}
		}
	}
	/* now fill in the faces ... */
	for (i = 0; i < ntot; i++) {
		e = &bv->edges[i];
		e2 = (i == bv->edgecount - 1) ? &bv->edges[0] : &bv->edges[i + 1];
		bme = e->e;
		bme2 = e2->e;
		BLI_assert(bme != NULL);
		if (e->fnext != NULL || e2->fprev != NULL)
			continue;
		/* Which faces have successive loops that are for bme and bme2?
		 * There could be more than one. E.g., in manifold ntot==2 case.
		 * Prefer one that has loop in same direction as e. */
		bestf = NULL;
		BM_ITER_ELEM(l, &iter, bme, BM_LOOPS_OF_EDGE) {
			f = l->f;
			if ((l->prev->e == bme2 || l->next->e == bme2)) {
				if (!bestf || l->v == bv->v)
					bestf = f;
			}
			if (bestf) {
				e->fnext = e2->fprev = bestf;
			}
		}
	}
}

/*
 * Construction around the vertex
 */
static BevVert *bevel_vert_construct(BMesh *bm, BevelParams *bp, BMVert *v)
{
	BMEdge *bme;
	BevVert *bv;
	BMEdge *first_bme;
	BMVert *v1, *v2;
	BMIter iter;
	EdgeHalf *e;
	float weight, z;
	int i, ccw_test_sum;
	int nsel = 0;
	int ntot = 0;
	int nwire = 0;
	int fcnt;

	/* Gather input selected edges.
	 * Only bevel selected edges that have exactly two incident faces.
	 * Want edges to be ordered so that they share faces.
	 * There may be one or more chains of shared faces broken by
	 * gaps where there are no faces.
	 * Want to ignore wire edges completely for edge beveling.
	 * TODO: make following work when more than one gap.
	 */

	first_bme = NULL;
	BM_ITER_ELEM (bme, &iter, v, BM_EDGES_OF_VERT) {
		fcnt = BM_edge_face_count(bme);
		BM_BEVEL_EDGE_TAG_DISABLE(bme);
		if (BM_elem_flag_test(bme, BM_ELEM_TAG) && !bp->vertex_only) {
			BLI_assert(fcnt == 2);
			nsel++;
			if (!first_bme)
				first_bme = bme;
		}
		if (fcnt == 1) {
			/* good to start face chain from this edge */
			first_bme = bme;
		}
		if (fcnt > 0 || bp->vertex_only)
			ntot++;
		if (BM_edge_is_wire(bme)) {
			nwire++;
			/* If edge beveling, exclude wire edges from edges array.
			 * Mark this edge as "chosen" so loop below won't choose it. */
			if (!bp->vertex_only) {
				BM_BEVEL_EDGE_TAG_ENABLE(bme);
			}
		}
	}
	if (!first_bme)
		first_bme = v->e;

	if ((nsel == 0 && !bp->vertex_only) || (ntot < 2 && bp->vertex_only)) {
		/* signal this vert isn't being beveled */
		BM_elem_flag_disable(v, BM_ELEM_TAG);
		return NULL;
	}

	bv = (BevVert *)BLI_memarena_alloc(bp->mem_arena, (sizeof(BevVert)));
	bv->v = v;
	bv->edgecount = ntot;
	bv->selcount = nsel;
	bv->wirecount = nwire;
	bv->offset = bp->offset;
	bv->edges = (EdgeHalf *)BLI_memarena_alloc(bp->mem_arena, ntot * sizeof(EdgeHalf));
	if (nwire)
		bv->wire_edges = (BMEdge **)BLI_memarena_alloc(bp->mem_arena, nwire * sizeof(BMEdge *));
	else
		bv->wire_edges = NULL;
	bv->vmesh = (VMesh *)BLI_memarena_alloc(bp->mem_arena, sizeof(VMesh));
	bv->vmesh->seg = bp->seg;

	if (bp->vertex_only) {
		/* if weighted, modify offset by weight */
		if (bp->dvert != NULL && bp->vertex_group != -1) {
			weight = defvert_find_weight(bp->dvert + BM_elem_index_get(v), bp->vertex_group);
			if (weight <= 0.0f) {
				BM_elem_flag_disable(v, BM_ELEM_TAG);
				return NULL;
			}
			bv->offset *= weight;
		}
		else if (bp->use_weights) {
			weight = BM_elem_float_data_get(&bm->vdata, v, CD_BWEIGHT);
			bv->offset *= weight;
		}
	}
	BLI_ghash_insert(bp->vert_hash, v, bv);

	find_bevel_edge_order(bm, bv, first_bme);

	/* fill in other attributes of EdgeHalfs */
	for (i = 0; i < ntot; i++) {
		e = &bv->edges[i];
		bme = e->e;
		if (BM_elem_flag_test(bme, BM_ELEM_TAG) && !bp->vertex_only) {
			e->is_bev = true;
			e->seg = bp->seg;
		}
		else {
			e->is_bev = false;
			e->seg = 0;
		}
		e->is_rev = (bme->v2 == v);
	}

	/* now done with tag flag */
	BM_ITER_ELEM (bme, &iter, v, BM_EDGES_OF_VERT) {
		BM_BEVEL_EDGE_TAG_DISABLE(bme);
	}

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

	for (i = 0, e = bv->edges; i < ntot; i++, e++) {
		e->next = &bv->edges[(i + 1) % ntot];
		e->prev = &bv->edges[(i + ntot - 1) % ntot];

		/* set offsets  */
		if (e->is_bev) {
			/* Convert distance as specified by user into offsets along
			 * faces on left side and right side of this edgehalf.
			 * Except for percent method, offset will be same on each side. */

			switch (bp->offset_type) {
				case BEVEL_AMT_OFFSET:
					e->offset_l_spec = bp->offset;
					break;
				case BEVEL_AMT_WIDTH:
					z = fabsf(2.0f * sinf(edge_face_angle(e) / 2.0f));
					if (z < BEVEL_EPSILON)
						e->offset_l_spec = 0.01f * bp->offset; /* undefined behavior, so tiny bevel */
					else
						e->offset_l_spec = bp->offset / z;
					break;
				case BEVEL_AMT_DEPTH:
					z = fabsf(cosf(edge_face_angle(e) / 2.0f));
					if (z < BEVEL_EPSILON)
						e->offset_l_spec = 0.01f * bp->offset; /* undefined behavior, so tiny bevel */
					else
						e->offset_l_spec = bp->offset / z;
					break;
				case BEVEL_AMT_PERCENT:
					/* offset needs to be such that it meets adjacent edges at percentage of their lengths */
					v1 = BM_edge_other_vert(e->prev->e, v);
					v2 = BM_edge_other_vert(e->e, v);
					z = sinf(angle_v3v3v3(v1->co, v->co, v2->co));
					e->offset_l_spec = BM_edge_calc_length(e->prev->e) * bp->offset * z / 100.0f;
					v1 = BM_edge_other_vert(e->e, v);
					v2 = BM_edge_other_vert(e->next->e, v);
					z = sinf(angle_v3v3v3(v1->co, v->co, v2->co));
					e->offset_r_spec = BM_edge_calc_length(e->next->e) * bp->offset * z / 100.0f;
					break;
				default:
					BLI_assert(!"bad bevel offset kind");
					e->offset_l_spec = bp->offset;
					break;
			}
			if (bp->offset_type != BEVEL_AMT_PERCENT)
				e->offset_r_spec = e->offset_l_spec;
			if (bp->use_weights) {
				weight = BM_elem_float_data_get(&bm->edata, e->e, CD_BWEIGHT);
				e->offset_l_spec *= weight;
				e->offset_r_spec *= weight;
			}
		}
		else if (bp->vertex_only) {
			/* Weight has already been applied to bv->offset, if present.
			 * Transfer to e->offset_[lr]_spec and treat percent as special case */
			if (bp->offset_type == BEVEL_AMT_PERCENT) {
				v2 = BM_edge_other_vert(e->e, bv->v);
				e->offset_l_spec = BM_edge_calc_length(e->e) * bv->offset / 100.0f;
			}
			else {
				e->offset_l_spec = bv->offset;
			}
			e->offset_r_spec = e->offset_l_spec;
		}
		else {
			e->offset_l_spec = e->offset_r_spec = 0.0f;
		}
		e->offset_l = e->offset_l_spec;
		e->offset_r = e->offset_r_spec;

		if (e->fprev && e->fnext)
			e->is_seam = !contig_ldata_across_edge(bm, e->e, e->fprev, e->fnext);
		else
			e->is_seam = true;
	}

	if (nwire) {
		i = 0;
		BM_ITER_ELEM (bme, &iter, v, BM_EDGES_OF_VERT) {
			if (BM_edge_is_wire(bme)) {
				BLI_assert(i < bv->wirecount);
				bv->wire_edges[i++] = bme;
			}
		}
		BLI_assert(i == bv->wirecount);
	}

	return bv;
}

/* Face f has at least one beveled vertex.  Rebuild f */
static bool bev_rebuild_polygon(BMesh *bm, BevelParams *bp, BMFace *f)
{
	BMIter liter, eiter, fiter;
	BMLoop *l, *lprev;
	BevVert *bv;
	BoundVert *v, *vstart, *vend;
	EdgeHalf *e, *eprev;
	VMesh *vm;
	int i, k, n;
	bool do_rebuild = false;
	bool go_ccw, corner3special, keep;
	BMVert *bmv;
	BMEdge *bme, *bme_new, *bme_prev;
	BMFace *f_new, *f_other;
	BMVert **vv = NULL;
	BMVert **vv_fix = NULL;
	BMEdge **ee = NULL;
	BLI_array_staticdeclare(vv, BM_DEFAULT_NGON_STACK_SIZE);
	BLI_array_staticdeclare(vv_fix, BM_DEFAULT_NGON_STACK_SIZE);
	BLI_array_staticdeclare(ee, BM_DEFAULT_NGON_STACK_SIZE);

	BM_ITER_ELEM (l, &liter, f, BM_LOOPS_OF_FACE) {
		if (BM_elem_flag_test(l->v, BM_ELEM_TAG)) {
			lprev = l->prev;
			bv = find_bevvert(bp, l->v);
			vm = bv->vmesh;
			e = find_edge_half(bv, l->e);
			BLI_assert(e != NULL);
			bme = e->e;
			eprev = find_edge_half(bv, lprev->e);
			BLI_assert(eprev != NULL);

			/* which direction around our vertex do we travel to match orientation of f? */
			if (e->prev == eprev) {
				if (eprev->prev == e) {
					/* valence 2 vertex: use f is one of e->fnext or e->fprev  to break tie */
					go_ccw = (e->fnext != f);
				}
				else {
					go_ccw = true;  /* going ccw around bv to trace this corner */
				}
			}
			else if (eprev->prev == e) {
				go_ccw = false;  /* going cw around bv to trace this corner */
			}
			else {
				/* edges in face are non-contiguous in our ordering around bv.
				 * Which way should we go when going from eprev to e? */
				if (count_ccw_edges_between(eprev, e) < count_ccw_edges_between(e, eprev)) {
					/* go counterclockewise from eprev to e */
					go_ccw = true;
				}
				else {
					/* go clockwise from eprev to e */
					go_ccw = false;
				}
			}
			if (go_ccw) {
				vstart = eprev->rightv;
				vend = e->leftv;
			}
			else {
				vstart = eprev->leftv;
				vend = e->rightv;
			}
			BLI_assert(vstart != NULL && vend != NULL);
			v = vstart;
			BLI_array_append(vv, v->nv.v);
			BLI_array_append(ee, bme);
			/* check for special case: multisegment 3rd face opposite a beveled edge with no vmesh */
			corner3special = (vm->mesh_kind == M_NONE && v->ebev != e && v->ebev != eprev);
			while (v != vend) {
				if (go_ccw) {
					if (vm->seg > 1) {
						if (vm->mesh_kind == M_ADJ || bp->vertex_only || corner3special) {
							i = v->index;
							for (k = 1; k < vm->seg; k++) {
								bmv = mesh_vert(vm, i, 0, k)->v;
								BLI_array_append(vv, bmv);
								BLI_array_append(ee, bme); /* TODO: maybe better edge here */
								if (corner3special && v->ebev && !v->ebev->is_seam)
									BLI_array_append(vv_fix, bmv);
							}
						}
					}
					v = v->next;
				}
				else {
					/* going cw */
					if (vm->seg > 1) {
						if (vm->mesh_kind == M_ADJ || bp->vertex_only ||
						    (vm->mesh_kind == M_NONE && v->ebev != e && v->ebev != eprev))
						{
							i = v->prev->index;
							for (k = vm->seg - 1; k > 0; k--) {
								bmv = mesh_vert(vm, i, 0, k)->v;
								BLI_array_append(vv, bmv);
								BLI_array_append(ee, bme);
								if (corner3special && v->ebev && !v->ebev->is_seam)
									BLI_array_append(vv_fix, bmv);
							}
						}
					}
					v = v->prev;
				}
				BLI_array_append(vv, v->nv.v);
				BLI_array_append(ee, bme);
			}
			do_rebuild = true;
		}
		else {
			BLI_array_append(vv, l->v);
			BLI_array_append(ee, l->e);
		}
	}
	if (do_rebuild) {
		n = BLI_array_count(vv);
		f_new = bev_create_ngon(bm, vv, n, NULL, f, NULL, -1, true);

		for (k = 0; k < BLI_array_count(vv_fix); k++) {
			bev_merge_uvs(bm, vv_fix[k]);
		}

		/* copy attributes from old edges */
		BLI_assert(n == BLI_array_count(ee));
		bme_prev = ee[n - 1];
		for (k = 0; k < n; k++) {
			bme_new = BM_edge_exists(vv[k], vv[(k + 1) % n]);
			BLI_assert(ee[k] && bme_new);
			if (ee[k] != bme_new) {
				BM_elem_attrs_copy(bm, bm, ee[k], bme_new);
				/* want to undo seam and smooth for corner segments
				 * if those attrs aren't contiguous around face */
				if (k < n - 1 && ee[k] == ee[k + 1]) {
					if (BM_elem_flag_test(ee[k], BM_ELEM_SEAM) &&
					    !BM_elem_flag_test(bme_prev, BM_ELEM_SEAM))
					{
						BM_elem_flag_disable(bme_new, BM_ELEM_SEAM);
					}
					/* actually want "sharp" to be contiguous, so reverse the test */
					if (!BM_elem_flag_test(ee[k], BM_ELEM_SMOOTH) &&
					    BM_elem_flag_test(bme_prev, BM_ELEM_SMOOTH))
					{
						BM_elem_flag_enable(bme_new, BM_ELEM_SMOOTH);
					}
				}
				else
					bme_prev = ee[k];
			}
		}

		/* don't select newly or return created boundary faces... */
		if (f_new) {
			BM_elem_flag_disable(f_new, BM_ELEM_TAG);
			/* Also don't want new edges that aren't part of a new bevel face */
			BM_ITER_ELEM(bme, &eiter, f_new, BM_EDGES_OF_FACE) {
				keep = false;
				BM_ITER_ELEM(f_other, &fiter, bme, BM_FACES_OF_EDGE) {
					if (BM_elem_flag_test(f_other, BM_ELEM_TAG)) {
						keep = true;
						break;
					}
				}
				if (!keep)
					disable_flag_out_edge(bm, bme);
			}
		}
	}

	BLI_array_free(vv);
	BLI_array_free(vv_fix);
	BLI_array_free(ee);
	return do_rebuild;
}

/* All polygons touching v need rebuilding because beveling v has made new vertices */
static void bevel_rebuild_existing_polygons(BMesh *bm, BevelParams *bp, BMVert *v)
{
	void    *faces_stack[BM_DEFAULT_ITER_STACK_SIZE];
	int      faces_len, f_index;
	BMFace **faces = BM_iter_as_arrayN(bm, BM_FACES_OF_VERT, v, &faces_len,
	                                   faces_stack, BM_DEFAULT_ITER_STACK_SIZE);

	if (LIKELY(faces != NULL)) {
		for (f_index = 0; f_index < faces_len; f_index++) {
			BMFace *f = faces[f_index];
			if (bev_rebuild_polygon(bm, bp, f)) {
				BM_face_kill(bm, f);
			}
		}

		if (faces != (BMFace **)faces_stack) {
			MEM_freeN(faces);
		}
	}
}

/* If there were any wire edges, they need to be reattached somewhere */
static void bevel_reattach_wires(BMesh *bm, BevelParams *bp, BMVert *v)
{
	BMEdge *e;
	BMVert *vclosest, *vother, *votherclosest;
	BevVert *bv, *bvother;
	BoundVert *bndv, *bndvother;
	float d, dclosest;
	int i;

	bv = find_bevvert(bp, v);
	if (!bv || bv->wirecount == 0 || !bv->vmesh)
		return;

	for (i = 0; i < bv->wirecount; i++) {
		e = bv->wire_edges[i];
		/* look for the new vertex closest to the other end of e */
		vclosest = NULL;
		dclosest = FLT_MAX;
		votherclosest = NULL;
		vother = BM_edge_other_vert(e, v);
		bvother = NULL;
		if (BM_elem_flag_test(vother, BM_ELEM_TAG)) {
			bvother = find_bevvert(bp, vother);
			if (!bvother || !bvother->vmesh)
				return;  /* shouldn't happen */
		}
		bndv = bv->vmesh->boundstart;
		do {
			if (bvother) {
				bndvother = bvother->vmesh->boundstart;
				do {
					d = len_squared_v3v3(bndvother->nv.co, bndv->nv.co);
					if (d < dclosest) {
						vclosest = bndv->nv.v;
						votherclosest = bndvother->nv.v;
						dclosest = d;
						
					}
				} while ((bndvother = bndvother->next) != bvother->vmesh->boundstart);
			}
			else {
				d = len_squared_v3v3(vother->co, bndv->nv.co);
				if (d < dclosest) {
					vclosest = bndv->nv.v;
					votherclosest = vother;
					dclosest = d;
				}
			}
		} while ((bndv = bndv->next) != bv->vmesh->boundstart);
		if (vclosest) {
			BM_edge_create(bm, vclosest, votherclosest, e, BM_CREATE_NO_DOUBLE);
		}
	}
}

static void bev_merge_end_uvs(BMesh *bm, BevVert *bv, EdgeHalf *e)
{
	VMesh *vm = bv->vmesh;
	int i, k, nseg;

	nseg = e->seg;
	i = e->leftv->index;
	for (k = 1; k < nseg; k++) {
		bev_merge_uvs(bm, mesh_vert(vm, i, 0, k)->v);
	}
}

/*
 * Is this BevVert the special case of a weld (no vmesh) where there are
 * four edges total, two are beveled, and the other two are on opposite sides?
 */
static bool bevvert_is_weld_cross(BevVert *bv)
{
	return (bv->edgecount == 4 && bv->selcount == 2 &&
	        ((bv->edges[0].is_bev && bv->edges[2].is_bev) ||
	         (bv->edges[1].is_bev && bv->edges[3].is_bev)));
}

/*
 * Copy edge attribute data across the non-beveled crossing edges of a cross weld.
 *
 * Situation looks like this:
 *
 *      e->next
 *        |
 * -------3-------
 * -------2-------
 * -------1------- e
 * -------0------
 *        |
 *      e->prev
 *
 * where e is the EdgeHalf of one of the beveled edges,
 * e->next and e->prev are EdgeHalfs for the unbeveled edges of the cross
 * and their attributes are to be copied to the edges 01, 12, 23.
 * The vert i is mesh_vert(vm, vmindex, 0, i)->v
 */
static void weld_cross_attrs_copy(BMesh *bm, BevVert *bv, VMesh *vm, int vmindex, EdgeHalf *e)
{
	BMEdge *bme_prev, *bme_next, *bme;
	int i, nseg;
	bool disable_seam, enable_smooth;

	bme_prev = bme_next = NULL;
	for (i = 0; i < 4; i++) {
		if (&bv->edges[i] == e) {
			bme_prev = bv->edges[(i + 3) % 4].e;
			bme_next = bv->edges[(i + 1) % 4].e;
			break;
		}
	}
	BLI_assert(bme_prev && bme_next);

	/* want seams and sharp edges to cross only if that way on both sides */
	disable_seam = BM_elem_flag_test(bme_prev, BM_ELEM_SEAM) != BM_elem_flag_test(bme_next, BM_ELEM_SEAM);
	enable_smooth = BM_elem_flag_test(bme_prev, BM_ELEM_SMOOTH) != BM_elem_flag_test(bme_next, BM_ELEM_SMOOTH);

	nseg = e->seg;
	for (i = 0; i < nseg; i++) {
		bme = BM_edge_exists(mesh_vert(vm, vmindex, 0, i)->v,
		                     mesh_vert(vm, vmindex, 0, i + 1)->v);
		BLI_assert(bme);
		BM_elem_attrs_copy(bm, bm, bme_prev, bme);
		if (disable_seam)
			BM_elem_flag_disable(bme, BM_ELEM_SEAM);
		if (enable_smooth)
			BM_elem_flag_enable(bme, BM_ELEM_SMOOTH);
	}
}

/*
 * Build the polygons along the selected Edge
 */
static void bevel_build_edge_polygons(BMesh *bm, BevelParams *bp, BMEdge *bme)
{
	BevVert *bv1, *bv2;
	BMVert *bmv1, *bmv2, *bmv3, *bmv4;
	VMesh *vm1, *vm2;
	EdgeHalf *e1, *e2;
	BMEdge *bme1, *bme2, *center_bme;
	BMFace *f1, *f2, *f;
	BMVert *verts[4];
	BMFace *faces[4];
	BMEdge *edges[4];
	int k, nseg, i1, i2, odd, mid;
	int mat_nr = bp->mat_nr;

	if (!BM_edge_is_manifold(bme))
		return;

	bv1 = find_bevvert(bp, bme->v1);
	bv2 = find_bevvert(bp, bme->v2);

	BLI_assert(bv1 && bv2);

	e1 = find_edge_half(bv1, bme);
	e2 = find_edge_half(bv2, bme);

	BLI_assert(e1 && e2);

	/*
	 *      bme->v1
	 *     / | \
	 *   v1--|--v4
	 *   |   |   |
	 *   |   |   |
	 *   v2--|--v3
	 *     \ | /
	 *      bme->v2
	 */
	nseg = e1->seg;
	BLI_assert(nseg > 0 && nseg == e2->seg);

	bmv1 = e1->leftv->nv.v;
	bmv4 = e1->rightv->nv.v;
	bmv2 = e2->rightv->nv.v;
	bmv3 = e2->leftv->nv.v;

	BLI_assert(bmv1 && bmv2 && bmv3 && bmv4);

	f1 = e1->fprev;
	f2 = e1->fnext;
	faces[0] = faces[1] = f1;
	faces[2] = faces[3] = f2;
	i1 = e1->leftv->index;
	i2 = e2->leftv->index;
	vm1 = bv1->vmesh;
	vm2 = bv2->vmesh;

	verts[0] = bmv1;
	verts[1] = bmv2;
	odd = nseg % 2;
	mid = nseg / 2;
	center_bme = NULL;
	for (k = 1; k <= nseg; k++) {
		verts[3] = mesh_vert(vm1, i1, 0, k)->v;
		verts[2] = mesh_vert(vm2, i2, 0, nseg - k)->v;
		if (odd && k == mid + 1) {
			if (e1->is_seam) {
				/* straddles a seam: choose to interpolate in f1 and snap right edge to bme */
				edges[0] = edges[1] = NULL;
				edges[2] = edges[3] = bme;
				bev_create_ngon(bm, verts, 4, NULL, f1, edges, mat_nr, true);
			}
			else {
				/* straddles but not a seam: interpolate left half in f1, right half in f2 */
				bev_create_ngon(bm, verts, 4, faces, NULL, NULL, mat_nr, true);
			}
		}
		else if (!odd && k == mid) {
			/* left poly that touches an even center line on right */
			edges[0] = edges[1] = NULL;
			edges[2] = edges[3] = bme;
			bev_create_ngon(bm, verts, 4, NULL, f1, edges, mat_nr, true);
			center_bme = BM_edge_exists(verts[2], verts[3]);
			BLI_assert(center_bme != NULL);
		}
		else if (!odd && k == mid + 1) {
			/* right poly that touches an even center line on left */
			edges[0] = edges[1] = bme;
			edges[2] = edges[3] = NULL;
			bev_create_ngon(bm, verts, 4, NULL, f2, edges, mat_nr, true);
		}
		else {
			/* doesn't cross or touch the center line, so interpolate in appropriate f1 or f2 */
			f = (k <= mid) ? f1 : f2;
			bev_create_ngon(bm, verts, 4, NULL, f, NULL, mat_nr, true);
		}
		verts[0] = verts[3];
		verts[1] = verts[2];
	}
	if (!odd) {
		if (!e1->is_seam)
			bev_merge_edge_uvs(bm, center_bme, mesh_vert(vm1, i1, 0, mid)->v);
		if (!e2->is_seam)
			bev_merge_edge_uvs(bm, center_bme, mesh_vert(vm2, i2, 0, mid)->v);
	}

	/* Fix UVs along end edge joints.  A nop unless other side built already. */
	/* TODO: if some seam, may want to do selective merge */
	if (!bv1->any_seam && bv1->vmesh->mesh_kind == M_NONE)
		bev_merge_end_uvs(bm, bv1, e1);
	if (!bv2->any_seam && bv2->vmesh->mesh_kind == M_NONE)
		bev_merge_end_uvs(bm, bv2, e2);

	/* Copy edge data to first and last edge */
	bme1 = BM_edge_exists(bmv1, bmv2);
	bme2 = BM_edge_exists(bmv3, bmv4);
	BLI_assert(bme1 && bme2);
	BM_elem_attrs_copy(bm, bm, bme, bme1);
	BM_elem_attrs_copy(bm, bm, bme, bme2);

	/* If either end is a "weld cross", want continuity of edge attributes across end edge(s) */
	if (bevvert_is_weld_cross(bv1)) {
		weld_cross_attrs_copy(bm, bv1, vm1, i1, e1);
	}
	if (bevvert_is_weld_cross(bv2)) {
		weld_cross_attrs_copy(bm, bv2, vm2, i2, e2);
	}
}

/* Returns the square of the length of the chord from parameter u0 to parameter u1
 * of superellipse_co. */
static float superellipse_chord_length_squared(float u0, float u1, float r)
{
	float a[2], b[2];

	BLI_assert(u0 >= 0.0f && u1 >= u0 && u1 <= 2.0f);
	superellipse_co(u0, r, a);
	superellipse_co(u1, r, b);
	return len_squared_v2v2(a, b);
}

/* Find parameter u >= u0 to make chord of squared length d2goal,
 * from u0 to u on superellipse with parameter r.
 * If it cannot be found, return -1.0f. */
static float find_superellipse_chord_u(float u0, float d2goal, float r)
{
	float ulow, uhigh, u, d2, d2max;
	const float dtol = 1e-4f;
	const float utol = 1e-6f;
	const float umax = 2.0f;

	if (d2goal == 0.0f)
		return u0;
	d2max = superellipse_chord_length_squared(u0, umax, r);
	if (fabsf(d2goal - d2max) <= dtol)
		return umax;
	if (d2goal - d2max > dtol)
		return -1.0f;

	/* binary search for good u value */
	ulow = u0;
	uhigh = umax;
	do {
		u = 0.5f * (ulow + uhigh);
		d2 = superellipse_chord_length_squared(u0, u, r);
		if (fabsf(d2goal - d2) <= dtol)
			break;
		if (d2 < d2goal)
			ulow = u;
		else
			uhigh = u;
	} while (fabsf(uhigh - ulow) > utol);
	return u;
}

/* Find parameters u0, u1, ..., un that divide the quarter-arc superellipse
 * with parameter r into n even chords.
 * There is no closed form way of doing this except for a few special
 * values of r, so this uses binary search to find a chord length that works.
 * Return the u's in *r_params, which should point to an array of size n+1. */
static void find_even_superellipse_params(int n, float r, float *r_params)
{
	float d2low, d2high, d2 = 0.0f, d2final, u;
	int i, j, n2;
	const int maxiters = 40;
	const float d2tol = 1e-6f;
	const float umax = 2.0f;

	if (r == PRO_CIRCLE_R || r == PRO_LINE_R ||
	    ((n % 2 == 0) && (r == PRO_SQUARE_IN_R || r == PRO_SQUARE_R)))
	{
		/* even parameter spacing works for these cases */
		for (i = 0; i <= n; i++)
			r_params[i] = i * 2.0f / (float) n;
		return;
	}
	if (r == PRO_SQUARE_IN_R || r == PRO_SQUARE_R) {
		/* n is odd, so get one corner-cut chord.
		 * Solve u == sqrt(2*(1-n2*u)^2) where n2 = floor(n/2) */
		n2 = n / 2;
		u = (2.0f * n2 - (float)M_SQRT2) / (2.0f * n2 * n2 - 1.0f);
		for (i = 0; i < n; i++)
			r_params[i] = i * u;
		r_params[n] = umax;
	}
	d2low = 2.0f / (n * n);  /* (sqrt(2)/n)**2 */
	d2high = 2 * d2low;      /* (2/n)**2 */
	for (i = 0; i < maxiters && fabsf(d2high - d2low) > d2tol; i++) {
		d2 = 0.5f * (d2low + d2high);

		/* find where we are after n-1 chords of squared length d2 */
		u = 0.0f;
		for (j = 0; j < n - 1; j++) {
			u = find_superellipse_chord_u(u, d2, r);
			if (u == -1.0f)
				break;  /* d2 is too big to go n-1 chords */
		}
		if (u == -1.0f) {
			d2high = d2;
			continue;
		}
		d2final = superellipse_chord_length_squared(u, umax, r);
		if (fabsf(d2final - d2) <= d2tol)
			break;
		if (d2final < d2)
			d2high = d2;
		else
			d2low = d2;
	}
	u = 0.0f;
	for (i = 0; i < n; i++) {
		r_params[i] = u;
		u = find_superellipse_chord_u(u, d2, r);
	}
	r_params[n] = umax;
}

/* The superellipse used for multisegment profiles does not
 * have a closed-form way to generate evenly spaced points
 * along an arc. We use an expensive search procedure to find
 * the parameter values that lead to bp->seg even chords.
 * We also want spacing for a number of segments that is
 * a power of 2 >= bp->seg (but at least 4). */
static void set_profile_spacing(BevelParams *bp)
{
	int seg, seg_2;

	seg = bp->seg;
	if (seg > 1) {
		bp->pro_spacing.uvals = (float *)BLI_memarena_alloc(bp->mem_arena, (seg + 1) * sizeof(float));
		find_even_superellipse_params(seg, bp->pro_super_r, bp->pro_spacing.uvals);
		seg_2 = power_of_2_max_i(bp->seg);
		if (seg_2 == 2)
			seg_2 = 4;
		bp->pro_spacing.seg_2 = seg_2;
		if (seg_2 == seg) {
			bp->pro_spacing.uvals_2 = bp->pro_spacing.uvals;
		}
		else {
			bp->pro_spacing.uvals_2 = (float *)BLI_memarena_alloc(bp->mem_arena, (seg_2 + 1) * sizeof(float));
			find_even_superellipse_params(seg_2, bp->pro_super_r, bp->pro_spacing.uvals_2);
		}
	}
	else {
		bp->pro_spacing.uvals = NULL;
		bp->pro_spacing.uvals_2 = NULL;
		bp->pro_spacing.seg_2 = 0;
	}
}

/*
 * Calculate and return an offset that is the lesser of the current
 * bp.offset and the maximum possible offset before geometry
 * collisions happen.
 * Currently this is a quick and dirty estimate of the max
 * possible: half the minimum edge length of any vertex involved
 * in a bevel. This is usually conservative.
 * The correct calculation is quite complicated.
 * TODO: implement this correctly.
 */
static float bevel_limit_offset(BMesh *bm, BevelParams *bp)
{
	BMVert *v;
	BMEdge *e;
	BMIter v_iter, e_iter;
	float limited_offset, half_elen;
	bool vbeveled;

	limited_offset = bp->offset;
	if (bp->offset_type == BEVEL_AMT_PERCENT) {
		if (limited_offset > 50.0f)
			limited_offset = 50.0f;
		return limited_offset;
	}
	BM_ITER_MESH (v, &v_iter, bm, BM_VERTS_OF_MESH) {
		if (BM_elem_flag_test(v, BM_ELEM_TAG)) {
			if (bp->vertex_only) {
				vbeveled = true;
			}
			else {
				vbeveled = false;
				BM_ITER_ELEM (e, &e_iter, v, BM_EDGES_OF_VERT) {
					if (BM_elem_flag_test(BM_edge_other_vert(e, v), BM_ELEM_TAG)) {
						vbeveled = true;
						break;
					}
				}
			}
			if (vbeveled) {
				BM_ITER_ELEM (e, &e_iter, v, BM_EDGES_OF_VERT) {
					half_elen = 0.5f * BM_edge_calc_length(e);
					if (half_elen < limited_offset)
						limited_offset = half_elen;
				}
			}
		}
	}
	return limited_offset;
}

/**
 * - Currently only bevels BM_ELEM_TAG'd verts and edges.
 *
 * - Newly created faces, edges, and verts are BM_ELEM_TAG'd too,
 *   the caller needs to ensure these are cleared before calling
 *   if its going to use this tag.
 *
 * - If limit_offset is set, adjusts offset down if necessary
 *   to avoid geometry collisions.
 *
 * \warning all tagged edges _must_ be manifold.
 */
void BM_mesh_bevel(
        BMesh *bm, const float offset, const int offset_type,
        const float segments, const float profile,
        const bool vertex_only, const bool use_weights, const bool limit_offset,
        const struct MDeformVert *dvert, const int vertex_group, const int mat,
        const bool loop_slide)
{
	BMIter iter;
	BMVert *v, *v_next;
	BMEdge *e;
	BevVert *bv;
	BevelParams bp = {NULL};

	bp.offset = offset;
	bp.offset_type = offset_type;
	bp.seg    = segments;
	bp.pro_super_r = 4.0f * profile;  /* convert to superellipse exponent */
	bp.vertex_only = vertex_only;
	bp.use_weights = use_weights;
	bp.loop_slide = loop_slide;
	bp.limit_offset = limit_offset;
	bp.dvert = dvert;
	bp.vertex_group = vertex_group;
	bp.mat_nr = mat;

	if (bp.pro_super_r < 0.60f)
		bp.pro_super_r = 0.60f;  /* TODO: implement 0 case properly */

	if (bp.offset > 0) {
		/* primary alloc */
		bp.vert_hash = BLI_ghash_ptr_new(__func__);
		bp.mem_arena = BLI_memarena_new(MEM_SIZE_OPTIMAL(1 << 16), __func__);
		BLI_memarena_use_calloc(bp.mem_arena);
		set_profile_spacing(&bp);

		if (limit_offset)
			bp.offset = bevel_limit_offset(bm, &bp);

		/* Analyze input vertices, sorting edges and assigning initial new vertex positions */
		BM_ITER_MESH (v, &iter, bm, BM_VERTS_OF_MESH) {
			if (BM_elem_flag_test(v, BM_ELEM_TAG)) {
				bv = bevel_vert_construct(bm, &bp, v);
				if (bv)
					build_boundary(&bp, bv, true);
			}
		}

		/* Perhaps do a pass to try to even out widths */
		if (!bp.vertex_only) {
			adjust_offsets(&bp);
		}

		/* Build the meshes around vertices, now that positions are final */
		BM_ITER_MESH (v, &iter, bm, BM_VERTS_OF_MESH) {
			if (BM_elem_flag_test(v, BM_ELEM_TAG)) {
				bv = find_bevvert(&bp, v);
				if (bv)
					build_vmesh(&bp, bm, bv);
			}
		}

		/* Build polygons for edges */
		if (!bp.vertex_only) {
			BM_ITER_MESH (e, &iter, bm, BM_EDGES_OF_MESH) {
				if (BM_elem_flag_test(e, BM_ELEM_TAG)) {
					bevel_build_edge_polygons(bm, &bp, e);
				}
			}
		}

		/* Rebuild face polygons around affected vertices */
		BM_ITER_MESH (v, &iter, bm, BM_VERTS_OF_MESH) {
			if (BM_elem_flag_test(v, BM_ELEM_TAG)) {
				bevel_rebuild_existing_polygons(bm, &bp, v);
				bevel_reattach_wires(bm, &bp, v);
			}
		}

		BM_ITER_MESH_MUTABLE (v, v_next, &iter, bm, BM_VERTS_OF_MESH) {
			if (BM_elem_flag_test(v, BM_ELEM_TAG)) {
				BLI_assert(find_bevvert(&bp, v) != NULL);
				BM_vert_kill(bm, v);
			}
		}

		/* When called from operator (as opposed to modifier), bm->use_toolflags
		 * will be set, and we to transfer the oflags to BM_ELEM_TAGs */
		if (bm->use_toolflags) {
			BM_ITER_MESH (v, &iter, bm, BM_VERTS_OF_MESH) {
				if (BMO_vert_flag_test(bm, v, VERT_OUT))
					BM_elem_flag_enable(v, BM_ELEM_TAG);
			}
			BM_ITER_MESH (e, &iter, bm, BM_EDGES_OF_MESH) {
				if (BMO_edge_flag_test(bm, e, EDGE_OUT)) {
					BM_elem_flag_enable(e, BM_ELEM_TAG);
				}
			}
		}

		/* primary free */
		BLI_ghash_free(bp.vert_hash, NULL, NULL);
		BLI_memarena_free(bp.mem_arena);
	}
}
