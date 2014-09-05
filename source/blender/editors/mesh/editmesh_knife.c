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
 * The Original Code is Copyright (C) 2007 Blender Foundation.
 * All rights reserved.
 *
 * 
 * Contributor(s): Joseph Eagar, Joshua Leung, Howard Trickey,
 *                 Campbell Barton
 *
 * ***** END GPL LICENSE BLOCK *****
 */

/** \file blender/editors/mesh/editmesh_knife.c
 *  \ingroup edmesh
 *
 * Interactive editmesh knife tool.
 */

#ifdef _MSC_VER
#  define _USE_MATH_DEFINES
#endif

#include "MEM_guardedalloc.h"

#include "BLI_listbase.h"
#include "BLI_string.h"
#include "BLI_array.h"
#include "BLI_alloca.h"
#include "BLI_linklist.h"
#include "BLI_math.h"
#include "BLI_smallhash.h"
#include "BLI_memarena.h"

#include "BLF_translation.h"

#include "BKE_DerivedMesh.h"
#include "BKE_context.h"
#include "BKE_editmesh.h"
#include "BKE_editmesh_bvh.h"
#include "BKE_report.h"

#include "BIF_gl.h"
#include "BIF_glutil.h" /* for paint cursor */

#include "ED_screen.h"
#include "ED_space_api.h"
#include "ED_view3d.h"
#include "ED_mesh.h"

#include "WM_api.h"
#include "WM_types.h"

#include "DNA_object_types.h"
#include "UI_resources.h"

#include "RNA_access.h"
#include "RNA_define.h"

#include "mesh_intern.h"  /* own include */

#define KMAXDIST    10  /* max mouse distance from edge before not detecting it */

#define KNIFE_FLT_EPS          0.00001f
#define KNIFE_FLT_EPS_SQUARED  (KNIFE_FLT_EPS * KNIFE_FLT_EPS)
#define KNIFE_FLT_EPSBIG       0.0005f
#define KNIFE_FLT_EPS_PX       0.2f

typedef struct KnifeColors {
	unsigned char line[3];
	unsigned char edge[3];
	unsigned char curpoint[3];
	unsigned char curpoint_a[4];
	unsigned char point[3];
	unsigned char point_a[4];
} KnifeColors;

/* knifetool operator */
typedef struct KnifeVert {
	BMVert *v; /* non-NULL if this is an original vert */
	ListBase edges;
	ListBase faces;

	float co[3], cageco[3], sco[2]; /* sco is screen coordinates for cageco */
	bool is_face, in_space;
	bool draw;
} KnifeVert;

typedef struct Ref {
	struct Ref *next, *prev;
	void *ref;
} Ref;

typedef struct KnifeEdge {
	KnifeVert *v1, *v2;
	BMFace *basef; /* face to restrict face fill to */
	ListBase faces;

	BMEdge *e /* , *e_old */; /* non-NULL if this is an original edge */
	bool draw;
} KnifeEdge;

typedef struct KnifeLineHit {
	float hit[3], cagehit[3];
	float schit[2];  /* screen coordinates for cagehit */
	float l; /* lambda along cut line */
	float perc; /* lambda along hit line */
	float m; /* depth front-to-back */

	/* Exactly one of kfe, v, or f should be non-NULL,
	 * saying whether cut line crosses and edge,
	 * is snapped to a vert, or is in the middle of some face. */
	KnifeEdge *kfe;
	KnifeVert *v;
	BMFace *f;
} KnifeLineHit;

typedef struct KnifePosData {
	float co[3];
	float cage[3];

	/* At most one of vert, edge, or bmface should be non-NULL,
	 * saying whether the point is snapped to a vertex, edge, or in a face.
	 * If none are set, this point is in space and is_space should be true. */
	KnifeVert *vert;
	KnifeEdge *edge;
	BMFace *bmface;
	bool is_space;

	float mval[2]; /* mouse screen position (may be non-integral if snapped to something) */
} KnifePosData;

/* struct for properties used while drawing */
typedef struct KnifeTool_OpData {
	ARegion *ar;        /* region that knifetool was activated in */
	void *draw_handle;  /* for drawing preview loop */
	ViewContext vc;     /* note: _don't_ use 'mval', instead use the one we define below */
	float mval[2];      /* mouse value with snapping applied */
	//bContext *C;

	Object *ob;
	BMEditMesh *em;

	MemArena *arena;

	GHash *origvertmap;
	GHash *origedgemap;
	GHash *kedgefacemap;
	GHash *facetrimap;

	BMBVHTree *bmbvh;

	BLI_mempool *kverts;
	BLI_mempool *kedges;

	float vthresh;
	float ethresh;

	/* used for drag-cutting */
	KnifeLineHit *linehits;
	int totlinehit;

	/* Data for mouse-position-derived data (cur) and previous click (prev) */
	KnifePosData curr, prev;

	int totkedge, totkvert;

	BLI_mempool *refs;

	float projmat[4][4];

	KnifeColors colors;

	/* run by the UI or not */
	bool is_interactive;

	/* operatpr options */
	bool cut_through;    /* preference, can be modified at runtime (that feature may go) */
	bool only_select;    /* set on initialization */
	bool select_result;  /* set on initialization */

	bool is_ortho;
	float ortho_extent;
	float clipsta, clipend;

	enum {
		MODE_IDLE,
		MODE_DRAGGING,
		MODE_CONNECT,
		MODE_PANNING
	} mode;

	int prevmode;
	bool snap_midpoints;
	bool ignore_edge_snapping;
	bool ignore_vert_snapping;

	/* use to check if we're currently dragging an angle snapped line */
	bool is_angle_snapping;

	enum {
		ANGLE_FREE,
		ANGLE_0,
		ANGLE_45,
		ANGLE_90,
		ANGLE_135
	} angle_snapping;

	const float (*cagecos)[3];
} KnifeTool_OpData;

static ListBase *knife_get_face_kedges(KnifeTool_OpData *kcd, BMFace *f);

static void knife_input_ray_segment(KnifeTool_OpData *kcd, const float mval[2], const float ofs,
                                    float r_origin[3], float r_dest[3]);

static bool knife_verts_edge_in_face(KnifeVert *v1, KnifeVert *v2, BMFace *f);

static void knife_update_header(bContext *C, KnifeTool_OpData *kcd)
{
#define HEADER_LENGTH 256
	char header[HEADER_LENGTH];

	BLI_snprintf(header, HEADER_LENGTH, IFACE_("LMB: define cut lines, Return/Spacebar: confirm, Esc or RMB: cancel, "
	                                           "E: new cut, Ctrl: midpoint snap (%s), Shift: ignore snap (%s), "
	                                           "C: angle constrain (%s), Z: cut through (%s)"),
	             WM_bool_as_string(kcd->snap_midpoints),
	             WM_bool_as_string(kcd->ignore_edge_snapping),
	             WM_bool_as_string(kcd->angle_snapping),
	             WM_bool_as_string(kcd->cut_through));
	ED_area_headerprint(CTX_wm_area(C), header);
#undef HEADER_LENGTH
}

static void knife_project_v2(const KnifeTool_OpData *kcd, const float co[3], float sco[2])
{
	ED_view3d_project_float_v2_m4(kcd->ar, co, sco, (float (*)[4])kcd->projmat);
}

static void knife_pos_data_clear(KnifePosData *kpd)
{
	zero_v3(kpd->co);
	zero_v3(kpd->cage);
	kpd->vert = NULL;
	kpd->edge = NULL;
	kpd->bmface = NULL;
	zero_v2(kpd->mval);
}

static ListBase *knife_empty_list(KnifeTool_OpData *kcd)
{
	ListBase *lst;

	lst = BLI_memarena_alloc(kcd->arena, sizeof(ListBase));
	BLI_listbase_clear(lst);
	return lst;
}

static void knife_append_list(KnifeTool_OpData *kcd, ListBase *lst, void *elem)
{
	Ref *ref;

	ref = BLI_mempool_calloc(kcd->refs);
	ref->ref = elem;
	BLI_addtail(lst, ref);
}

static Ref *find_ref(ListBase *lb, void *ref)
{
	Ref *ref1;

	for (ref1 = lb->first; ref1; ref1 = ref1->next) {
		if (ref1->ref == ref)
			return ref1;
	}

	return NULL;
}

static void knife_append_list_no_dup(KnifeTool_OpData *kcd, ListBase *lst, void *elem)
{
	if (!find_ref(lst, elem))
		knife_append_list(kcd, lst, elem);
}

static KnifeEdge *new_knife_edge(KnifeTool_OpData *kcd)
{
	kcd->totkedge++;
	return BLI_mempool_calloc(kcd->kedges);
}

static void knife_add_to_vert_edges(KnifeTool_OpData *kcd, KnifeEdge *kfe)
{
	knife_append_list(kcd, &kfe->v1->edges, kfe);
	knife_append_list(kcd, &kfe->v2->edges, kfe);
}

/* Add faces of an edge to a KnifeVert's faces list.  No checks for dups. */
static void knife_add_edge_faces_to_vert(KnifeTool_OpData *kcd, KnifeVert *kfv, BMEdge *e)
{
	BMIter bmiter;
	BMFace *f;

	BM_ITER_ELEM (f, &bmiter, e, BM_FACES_OF_EDGE) {
		knife_append_list(kcd, &kfv->faces, f);
	}
}

/* Find a face in common in the two faces lists.
 * If more than one, return the first; if none, return NULL */
static BMFace *knife_find_common_face(ListBase *faces1, ListBase *faces2)
{
	Ref *ref1, *ref2;

	for (ref1 = faces1->first; ref1; ref1 = ref1->next) {
		for (ref2 = faces2->first; ref2; ref2 = ref2->next) {
			if (ref1->ref == ref2->ref)
				return (BMFace *)(ref1->ref);
		}
	}
	return NULL;
}

static KnifeVert *new_knife_vert(KnifeTool_OpData *kcd, const float co[3], const float cageco[3])
{
	KnifeVert *kfv = BLI_mempool_calloc(kcd->kverts);

	kcd->totkvert++;

	copy_v3_v3(kfv->co, co);
	copy_v3_v3(kfv->cageco, cageco);

	knife_project_v2(kcd, kfv->cageco, kfv->sco);

	return kfv;
}

/* get a KnifeVert wrapper for an existing BMVert */
static KnifeVert *get_bm_knife_vert(KnifeTool_OpData *kcd, BMVert *v)
{
	KnifeVert *kfv = BLI_ghash_lookup(kcd->origvertmap, v);
	const float *cageco;

	if (!kfv) {
		BMIter bmiter;
		BMFace *f;

		if (BM_elem_index_get(v) >= 0)
			cageco = kcd->cagecos[BM_elem_index_get(v)];
		else
			cageco = v->co;
		kfv = new_knife_vert(kcd, v->co, cageco);
		kfv->v = v;
		BLI_ghash_insert(kcd->origvertmap, v, kfv);
		BM_ITER_ELEM (f, &bmiter, v, BM_FACES_OF_VERT) {
			knife_append_list(kcd, &kfv->faces, f);
		}
	}

	return kfv;
}

/* get a KnifeEdge wrapper for an existing BMEdge */
static KnifeEdge *get_bm_knife_edge(KnifeTool_OpData *kcd, BMEdge *e)
{
	KnifeEdge *kfe = BLI_ghash_lookup(kcd->origedgemap, e);
	if (!kfe) {
		BMIter bmiter;
		BMFace *f;

		kfe = new_knife_edge(kcd);
		kfe->e = e;
		kfe->v1 = get_bm_knife_vert(kcd, e->v1);
		kfe->v2 = get_bm_knife_vert(kcd, e->v2);

		knife_add_to_vert_edges(kcd, kfe);

		BLI_ghash_insert(kcd->origedgemap, e, kfe);

		BM_ITER_ELEM (f, &bmiter, e, BM_FACES_OF_EDGE) {
			knife_append_list(kcd, &kfe->faces, f);
		}
	}

	return kfe;
}

/* Record the index in kcd->em->looptris of first looptri triple for a given face,
 * given an index for some triple in that array.
 * This assumes that all of the triangles for a given face are contiguous
 * in that array (as they are by the current tesselation routines).
 * Actually store index + 1 in the hash, because 0 looks like "no entry"
 * to hash lookup routine; will reverse this in the get routine.
 * Doing this lazily rather than all at once for all faces.
 */
static void set_lowest_face_tri(KnifeTool_OpData *kcd, BMFace *f, int index)
{
	int i;

	if (BLI_ghash_lookup(kcd->facetrimap, f))
		return;

	BLI_assert(index >= 0 && index < kcd->em->tottri);
	BLI_assert(kcd->em->looptris[index][0]->f == f);
	for (i = index - 1; i >= 0; i--) {
		if (kcd->em->looptris[i][0]->f != f) {
			i++;
			break;
		}
	}
	if (i == -1)
		i++;

	BLI_ghash_insert(kcd->facetrimap, f, SET_INT_IN_POINTER(i + 1));
}

/* This should only be called for faces that have had a lowest face tri set by previous function */
static int get_lowest_face_tri(KnifeTool_OpData *kcd, BMFace *f)
{
	int ans;

	ans = GET_INT_FROM_POINTER(BLI_ghash_lookup(kcd->facetrimap, f));
	BLI_assert(ans != 0);
	return ans - 1;
}

/* User has just clicked for first time or first time after a restart (E key).
 * Copy the current position data into prev. */
static void knife_start_cut(KnifeTool_OpData *kcd)
{
	kcd->prev = kcd->curr;
	kcd->curr.is_space = 0; /*TODO: why do we do this? */

	if (kcd->prev.vert == NULL && kcd->prev.edge == NULL && is_zero_v3(kcd->prev.cage)) {
		/* Make prevcage a point on the view ray to mouse closest to a point on model: choose vertex 0 */
		float origin[3], origin_ofs[3];
		BMVert *v0;

		knife_input_ray_segment(kcd, kcd->curr.mval, 1.0f, origin, origin_ofs);
		v0 = BM_vert_at_index_find(kcd->em->bm, 0);
		if (v0) {
			closest_to_line_v3(kcd->prev.cage, v0->co, origin_ofs, origin);
			copy_v3_v3(kcd->prev.co, kcd->prev.cage); /*TODO: do we need this? */
			copy_v3_v3(kcd->curr.cage, kcd->prev.cage);
			copy_v3_v3(kcd->curr.co, kcd->prev.co);
		}
	}
}

static ListBase *knife_get_face_kedges(KnifeTool_OpData *kcd, BMFace *f)
{
	ListBase *lst = BLI_ghash_lookup(kcd->kedgefacemap, f);

	if (!lst) {
		BMIter bmiter;
		BMEdge *e;

		lst = knife_empty_list(kcd);

		BM_ITER_ELEM (e, &bmiter, f, BM_EDGES_OF_FACE) {
			knife_append_list(kcd, lst, get_bm_knife_edge(kcd, e));
		}

		BLI_ghash_insert(kcd->kedgefacemap, f, lst);
	}

	return lst;
}

static void knife_edge_append_face(KnifeTool_OpData *kcd, KnifeEdge *kfe, BMFace *f)
{
	knife_append_list(kcd, knife_get_face_kedges(kcd, f), kfe);
	knife_append_list(kcd, &kfe->faces, f);
}

static KnifeVert *knife_split_edge(
        KnifeTool_OpData *kcd, KnifeEdge *kfe,
        const float co[3], const float cageco[3],
        KnifeEdge **r_kfe)
{
	KnifeEdge *newkfe = new_knife_edge(kcd);
	Ref *ref;
	BMFace *f;

	newkfe->v1 = kfe->v1;
	newkfe->v2 = new_knife_vert(kcd, co, cageco);
	newkfe->v2->draw = 1;
	if (kfe->e) {
		knife_add_edge_faces_to_vert(kcd, newkfe->v2, kfe->e);
	}
	else {
		/* kfe cuts across an existing face.
		 * If v1 and v2 are in multiple faces together (e.g., if they
		 * are in doubled polys) then this arbitrarily chooses one of them */
		f = knife_find_common_face(&kfe->v1->faces, &kfe->v2->faces);
		if (f)
			knife_append_list(kcd, &newkfe->v2->faces, f);
	}
	newkfe->basef = kfe->basef;

	ref = find_ref(&kfe->v1->edges, kfe);
	BLI_remlink(&kfe->v1->edges, ref);

	kfe->v1 = newkfe->v2;
	BLI_addtail(&kfe->v1->edges, ref);

	for (ref = kfe->faces.first; ref; ref = ref->next)
		knife_edge_append_face(kcd, newkfe, ref->ref);

	knife_add_to_vert_edges(kcd, newkfe);

	newkfe->draw = kfe->draw;
	newkfe->e = kfe->e;

	*r_kfe = newkfe;

	return newkfe->v2;
}

/* primary key: lambda along cut
 * secondary key: lambda along depth
 * tertiary key: pointer comparisons of verts if both snapped to verts
 */
static int linehit_compare(const void *vlh1, const void *vlh2)
{
	const KnifeLineHit *lh1 = vlh1;
	const KnifeLineHit *lh2 = vlh2;

	if      (lh1->l < lh2->l) return -1;
	else if (lh1->l > lh2->l) return  1;
	else {
		if      (lh1->m < lh2->m) return -1;
		else if (lh1->m > lh2->m) return  1;
		else {
			if      (lh1->v < lh2->v) return -1;
			else if (lh1->v > lh2->v) return  1;
			else return 0;
		}
	}
}

/*
 * Sort linehits by distance along cut line, and secondarily from
 * front to back (from eye), and tertiarily by snap vertex,
 * and remove any duplicates.
 */
static void prepare_linehits_for_cut(KnifeTool_OpData *kcd)
{
	KnifeLineHit *linehits, *lhi, *lhj;
	int i, j, n;

	n = kcd->totlinehit;
	linehits = kcd->linehits;
	if (n == 0)
		return;

	qsort(linehits, n, sizeof(KnifeLineHit), linehit_compare);

	/* Remove any edge hits that are preceded or followed
	 * by a vertex hit that is very near. Mark such edge hits using
	 * l == -1 and then do another pass to actually remove.
	 * Also remove all but one of a series of vertex hits for the same vertex. */
	for (i = 0; i < n; i++) {
		lhi = &linehits[i];
		if (lhi->v) {
			for (j = i - 1; j >= 0; j--) {
				lhj = &linehits[j];
				if (!lhj->kfe ||
				    fabsf(lhi->l - lhj->l) > KNIFE_FLT_EPSBIG ||
				    fabsf(lhi->m - lhj->m) > KNIFE_FLT_EPSBIG)
				{
					break;
				}
				lhj->l = -1.0f;
			}
			for (j = i + 1; j < n; j++) {
				lhj = &linehits[j];
				if (fabsf(lhi->l - lhj->l) > KNIFE_FLT_EPSBIG ||
				    fabsf(lhi->m - lhj->m) > KNIFE_FLT_EPSBIG)
				{
					break;
				}
				if (lhj->kfe || lhi->v == lhj->v) {
					lhj->l = -1.0f;
				}
			}
		}
	}

	/* delete-in-place loop: copying from pos j to pos i+1 */
	i = 0;
	j = 1;
	while (j < n) {
		lhi = &linehits[i];
		lhj = &linehits[j];
		if (lhj->l == -1.0f) {
			j++; /* skip copying this one */
		}
		else {
			/* copy unless a no-op */
			if (lhi->l == -1.0f) {
				/* could happen if linehits[0] is being deleted */
				memcpy(&linehits[i], &linehits[j], sizeof(KnifeLineHit));
			}
			else {
				if (i + 1 != j)
					memcpy(&linehits[i + 1], &linehits[j], sizeof(KnifeLineHit));
				i++;
			}
			j++;
		}
	}
	kcd->totlinehit = i + 1;
}

/* Add hit to list of hits in facehits[f], where facehits is a map, if not already there */
static void add_hit_to_facehits(KnifeTool_OpData *kcd, GHash *facehits, BMFace *f, KnifeLineHit *hit)
{
	ListBase *lst = BLI_ghash_lookup(facehits, f);

	if (!lst) {
		lst = knife_empty_list(kcd);
		BLI_ghash_insert(facehits, f, lst);
	}
	knife_append_list_no_dup(kcd, lst, hit);
}

static void knife_add_single_cut(KnifeTool_OpData *kcd, KnifeLineHit *lh1, KnifeLineHit *lh2, BMFace *f)
{
	KnifeEdge *kfe, *kfe2;

	if ((lh1->v && lh1->v == lh2->v) ||
	    (lh1->kfe && lh1->kfe == lh2->kfe))
	{
		return;
	}

	/* Check if edge actually lies within face (might not, if this face is concave) */
	if ((lh1->v && !lh1->kfe) && (lh2->v && !lh2->kfe)) {
		if (!knife_verts_edge_in_face(lh1->v, lh2->v, f)) {
			return;
		}
	}

	kfe = new_knife_edge(kcd);
	kfe->draw = true;
	kfe->basef = f;

	if (lh1->v) {
		kfe->v1 = lh1->v;
	}
	else if (lh1->kfe) {
		kfe->v1 = knife_split_edge(kcd, lh1->kfe, lh1->hit, lh1->cagehit, &kfe2);
		lh1->v = kfe->v1;  /* record the KnifeVert for this hit  */
	}
	else {
		BLI_assert(lh1->f);
		kfe->v1 = new_knife_vert(kcd, lh1->hit, lh1->cagehit);
		kfe->v1->draw = true;
		kfe->v1->is_face = true;
		knife_append_list(kcd, &kfe->v1->faces, lh1->f);
		lh1->v = kfe->v1;  /* record the KnifeVert for this hit */
	}

	if (lh2->v) {
		kfe->v2 = lh2->v;
	}
	else if (lh2->kfe) {
		kfe->v2 = knife_split_edge(kcd, lh2->kfe, lh2->hit, lh2->cagehit, &kfe2);
		lh2->v = kfe->v2;  /* future uses of lh2 won't split again */
	}
	else {
		BLI_assert(lh2->f);
		kfe->v2 = new_knife_vert(kcd, lh2->hit, lh2->cagehit);
		kfe->v2->draw = true;
		kfe->v2->is_face = true;
		knife_append_list(kcd, &kfe->v2->faces, lh2->f);
		lh2->v = kfe->v2;  /* record the KnifeVert for this hit */
	}

	knife_add_to_vert_edges(kcd, kfe);

	/* TODO: check if this is ever needed */
	if (kfe->basef && !find_ref(&kfe->faces, kfe->basef))
		knife_edge_append_face(kcd, kfe, kfe->basef);

}

/* Given a list of KnifeLineHits for one face, sorted by l
 * and then by m, make the required KnifeVerts and
 * KnifeEdges.
 */
static void knife_cut_face(KnifeTool_OpData *kcd, BMFace *f, ListBase *hits)
{
	Ref *r;
	KnifeLineHit *lh, *prevlh;
	int n;

	(void) kcd;

	n = BLI_countlist(hits);
	if (n < 2)
		return;

	prevlh = NULL;
	for (r = hits->first; r; r = r->next) {
		lh = (KnifeLineHit *)r->ref;
		if (prevlh)
			knife_add_single_cut(kcd, prevlh, lh, f);
		prevlh = lh;
	}

}

/* User has just left-clicked after the first time.
 * Add all knife cuts implied by line from prev to curr.
 * If that line crossed edges then kcd->linehits will be non-NULL.
 * Make all of the KnifeVerts and KnifeEdges implied by this cut.
 */
static void knife_add_cut(KnifeTool_OpData *kcd)
{
	int i;
	KnifeLineHit *lh;
	GHash *facehits;
	BMFace *f;
	Ref *r;
	GHashIterator giter;
	ListBase *lst;

	prepare_linehits_for_cut(kcd);
	if (kcd->totlinehit == 0) {
		kcd->prev = kcd->curr;
		return;
	}

	/* make facehits: map face -> list of linehits touching it */
	facehits = BLI_ghash_ptr_new("knife facehits");
	for (i = 0; i < kcd->totlinehit; i++) {
		lh = &kcd->linehits[i];
		if (lh->f) {
			add_hit_to_facehits(kcd, facehits, lh->f, lh);
		}
		if (lh->v) {
			for (r = lh->v->faces.first; r; r = r->next) {
				add_hit_to_facehits(kcd, facehits, r->ref, lh);
			}
		}
		if (lh->kfe) {
			for (r = lh->kfe->faces.first; r; r = r->next) {
				add_hit_to_facehits(kcd, facehits, r->ref, lh);
			}
		}
	}

	/* Note: as following loop progresses, the 'v' fields of
	 * the linehits will be filled in (as edges are split or
	 * in-face verts are made), so it may be true that both
	 * the v and the kfe or f fields will be non-NULL. */
	GHASH_ITER (giter, facehits) {
		f = (BMFace *)BLI_ghashIterator_getKey(&giter);
		lst = (ListBase *)BLI_ghashIterator_getValue(&giter);
		knife_cut_face(kcd, f, lst);
	}

	/* set up for next cut */
	kcd->prev = kcd->curr;
	if (kcd->prev.bmface) {
		/* was "in face" but now we have a KnifeVert it is snapped to */
		kcd->prev.bmface = NULL;
		kcd->prev.vert = kcd->linehits[kcd->totlinehit - 1].v;
	}

	BLI_ghash_free(facehits, NULL, NULL);
	MEM_freeN(kcd->linehits);
	kcd->linehits = NULL;
	kcd->totlinehit = 0;
}

static void knife_finish_cut(KnifeTool_OpData *kcd)
{
	if (kcd->linehits) {
		MEM_freeN(kcd->linehits);
		kcd->linehits = NULL;
		kcd->totlinehit = 0;
	}
}

static void knifetool_draw_angle_snapping(const KnifeTool_OpData *kcd)
{
	bglMats mats;
	double u[3], u1[2], u2[2], v1[3], v2[3], dx, dy;
	double wminx, wminy, wmaxx, wmaxy;

	/* make u the window coords of prevcage */
	view3d_get_transformation(kcd->ar, kcd->vc.rv3d, kcd->ob, &mats);
	gluProject(kcd->prev.cage[0], kcd->prev.cage[1], kcd->prev.cage[2],
	           mats.modelview, mats.projection, mats.viewport,
	           &u[0], &u[1], &u[2]);

	/* make u1, u2 the points on window going through u at snap angle */
	wminx = kcd->ar->winrct.xmin;
	wmaxx = kcd->ar->winrct.xmin + kcd->ar->winx;
	wminy = kcd->ar->winrct.ymin;
	wmaxy = kcd->ar->winrct.ymin + kcd->ar->winy;

	switch (kcd->angle_snapping) {
		case ANGLE_0:
			u1[0] = wminx;
			u2[0] = wmaxx;
			u1[1] = u2[1] = u[1];
			break;
		case ANGLE_90:
			u1[0] = u2[0] = u[0];
			u1[1] = wminy;
			u2[1] = wmaxy;
			break;
		case ANGLE_45:
			/* clip against left or bottom */
			dx = u[0] - wminx;
			dy = u[1] - wminy;
			if (dy > dx) {
				u1[0] = wminx;
				u1[1] = u[1] - dx;
			}
			else {
				u1[0] = u[0] - dy;
				u1[1] = wminy;
			}
			/* clip against right or top */
			dx = wmaxx - u[0];
			dy = wmaxy - u[1];
			if (dy > dx) {
				u2[0] = wmaxx;
				u2[1] = u[1] + dx;
			}
			else {
				u2[0] = u[0] + dy;
				u2[1] = wmaxy;
			}
			break;
		case ANGLE_135:
			/* clip against right or bottom */
			dx = wmaxx - u[0];
			dy = u[1] - wminy;
			if (dy > dx) {
				u1[0] = wmaxx;
				u1[1] = u[1] - dx;
			}
			else {
				u1[0] = u[0] + dy;
				u1[1] = wminy;
			}
			/* clip against left or top */
			dx = u[0] - wminx;
			dy = wmaxy - u[1];
			if (dy > dx) {
				u2[0] = wminx;
				u2[1] = u[1] + dx;
			}
			else {
				u2[0] = u[0] - dy;
				u2[1] = wmaxy;
			}
			break;
		default:
			return;
	}

	/* unproject u1 and u2 back into object space */
	gluUnProject(u1[0], u1[1], 0.0,
	             mats.modelview, mats.projection, mats.viewport,
	             &v1[0], &v1[1], &v1[2]);
	gluUnProject(u2[0], u2[1], 0.0,
	             mats.modelview, mats.projection, mats.viewport,
	             &v2[0], &v2[1], &v2[2]);

	UI_ThemeColor(TH_TRANSFORM);
	glLineWidth(2.0);
	glBegin(GL_LINES);
	glVertex3dv(v1);
	glVertex3dv(v2);
	glEnd();
}

static void knife_init_colors(KnifeColors *colors)
{
	/* possible BMESH_TODO: add explicit themes or calculate these by
	 * figuring out contrasting colors with grid / edges / verts
	 * a la UI_make_axis_color */
	UI_GetThemeColor3ubv(TH_NURB_VLINE, colors->line);
	UI_GetThemeColor3ubv(TH_NURB_ULINE, colors->edge);
	UI_GetThemeColor3ubv(TH_HANDLE_SEL_VECT, colors->curpoint);
	UI_GetThemeColor3ubv(TH_HANDLE_SEL_VECT, colors->curpoint_a);
	colors->curpoint_a[3] = 102;
	UI_GetThemeColor3ubv(TH_ACTIVE_SPLINE, colors->point);
	UI_GetThemeColor3ubv(TH_ACTIVE_SPLINE, colors->point_a);
	colors->point_a[3] = 102;
}

/* modal loop selection drawing callback */
static void knifetool_draw(const bContext *C, ARegion *UNUSED(ar), void *arg)
{
	View3D *v3d = CTX_wm_view3d(C);
	const KnifeTool_OpData *kcd = arg;

	if (v3d->zbuf) glDisable(GL_DEPTH_TEST);

	glPolygonOffset(1.0f, 1.0f);

	glPushMatrix();
	glMultMatrixf(kcd->ob->obmat);

	if (kcd->mode == MODE_DRAGGING) {
		if (kcd->angle_snapping != ANGLE_FREE)
			knifetool_draw_angle_snapping(kcd);

		glColor3ubv(kcd->colors.line);
		
		glLineWidth(2.0);

		glBegin(GL_LINES);
		glVertex3fv(kcd->prev.cage);
		glVertex3fv(kcd->curr.cage);
		glEnd();

		glLineWidth(1.0);
	}

	if (kcd->prev.vert) {
		glColor3ubv(kcd->colors.point);
		glPointSize(11);

		glBegin(GL_POINTS);
		glVertex3fv(kcd->prev.cage);
		glEnd();
	}

	if (kcd->prev.bmface) {
		glColor3ubv(kcd->colors.curpoint);
		glPointSize(9);

		glBegin(GL_POINTS);
		glVertex3fv(kcd->prev.cage);
		glEnd();
	}

	if (kcd->curr.edge) {
		glColor3ubv(kcd->colors.edge);
		glLineWidth(2.0);

		glBegin(GL_LINES);
		glVertex3fv(kcd->curr.edge->v1->cageco);
		glVertex3fv(kcd->curr.edge->v2->cageco);
		glEnd();

		glLineWidth(1.0);
	}
	else if (kcd->curr.vert) {
		glColor3ubv(kcd->colors.point);
		glPointSize(11);

		glBegin(GL_POINTS);
		glVertex3fv(kcd->curr.cage);
		glEnd();
	}

	if (kcd->curr.bmface) {
		glColor3ubv(kcd->colors.curpoint);
		glPointSize(9);

		glBegin(GL_POINTS);
		glVertex3fv(kcd->curr.cage);
		glEnd();
	}

	if (kcd->totlinehit > 0) {
		KnifeLineHit *lh;
		int i;

		glEnable(GL_BLEND);
		glBlendFunc(GL_SRC_ALPHA, GL_ONE_MINUS_SRC_ALPHA);

		/* draw any snapped verts first */
		glColor4ubv(kcd->colors.point_a);
		glPointSize(11);
		glBegin(GL_POINTS);
		lh = kcd->linehits;
		for (i = 0; i < kcd->totlinehit; i++, lh++) {
			if (lh->v)
				glVertex3fv(lh->cagehit);
		}
		glEnd();

		/* now draw the rest */
		glColor4ubv(kcd->colors.curpoint_a);
		glPointSize(7);
		glBegin(GL_POINTS);
		lh = kcd->linehits;
		for (i = 0; i < kcd->totlinehit; i++, lh++) {
			if (!lh->v)
				glVertex3fv(lh->cagehit);
		}
		glEnd();
		glDisable(GL_BLEND);
	}

	if (kcd->totkedge > 0) {
		BLI_mempool_iter iter;
		KnifeEdge *kfe;

		glLineWidth(1.0);
		glBegin(GL_LINES);

		BLI_mempool_iternew(kcd->kedges, &iter);
		for (kfe = BLI_mempool_iterstep(&iter); kfe; kfe = BLI_mempool_iterstep(&iter)) {
			if (!kfe->draw)
				continue;

			glColor3ubv(kcd->colors.line);

			glVertex3fv(kfe->v1->cageco);
			glVertex3fv(kfe->v2->cageco);
		}

		glEnd();
		glLineWidth(1.0);
	}

	if (kcd->totkvert > 0) {
		BLI_mempool_iter iter;
		KnifeVert *kfv;

		glPointSize(5.0);

		glBegin(GL_POINTS);
		BLI_mempool_iternew(kcd->kverts, &iter);
		for (kfv = BLI_mempool_iterstep(&iter); kfv; kfv = BLI_mempool_iterstep(&iter)) {
			if (!kfv->draw)
				continue;

			glColor3ubv(kcd->colors.point);

			glVertex3fv(kfv->cageco);
		}

		glEnd();
	}

	glPopMatrix();

	if (v3d->zbuf) glEnable(GL_DEPTH_TEST);
}

/**
 * Find intersection of v1-v2 with face f.
 * Only take intersections that are at least \a face_tol_sq (in screen space) away
 * from other intersection elements.
 * If v1-v2 is coplanar with f, call that "no intersection though
 * it really means "infinite number of intersections".
 * In such a case we should have gotten hits on edges or verts of the face.
 */
static bool knife_ray_intersect_face(
        KnifeTool_OpData *kcd,
        const float s[2], const float v1[3], const float v2[3],
        BMFace *f, const float face_tol_sq,
        float hit_co[3], float hit_cageco[3])
{
	int tottri, tri_i;
	float raydir[3];
	float tri_norm[3], tri_plane[4];
	float se1[2], se2[2];
	float d, lambda;
	BMLoop **tri;
	ListBase *lst;
	Ref *ref;
	KnifeEdge *kfe;

	sub_v3_v3v3(raydir, v2, v1);
	normalize_v3(raydir);
	tri_i = get_lowest_face_tri(kcd, f);
	tottri = kcd->em->tottri;
	BLI_assert(tri_i >= 0 && tri_i < tottri);

	for (; tri_i < tottri; tri_i++) {
		const float *lv1, *lv2, *lv3;

		tri = kcd->em->looptris[tri_i];
		if (tri[0]->f != f)
			break;
		lv1 = kcd->cagecos[BM_elem_index_get(tri[0]->v)];
		lv2 = kcd->cagecos[BM_elem_index_get(tri[1]->v)];
		lv3 = kcd->cagecos[BM_elem_index_get(tri[2]->v)];
		/* using epsilon test in case ray is directly through an internal
		 * tesselation edge and might not hit either tesselation tri with
		 * an exact test;
		 * we will exclude hits near real edges by a later test */
		if (isect_ray_tri_epsilon_v3(v1, raydir, lv1, lv2, lv3, &lambda, NULL, KNIFE_FLT_EPS)) {
			/* check if line coplanar with tri */
			normal_tri_v3(tri_norm, lv1, lv2, lv3);
			plane_from_point_normal_v3(tri_plane, lv1, tri_norm);
			if ((dist_squared_to_plane_v3(v1, tri_plane) < KNIFE_FLT_EPS) &&
			    (dist_squared_to_plane_v3(v2, tri_plane) < KNIFE_FLT_EPS))
			{
				return false;
			}
			copy_v3_v3(hit_cageco, v1);
			madd_v3_v3fl(hit_cageco, raydir, lambda);
			/* Now check that far enough away from verts and edges */
			lst = knife_get_face_kedges(kcd, f);
			for (ref = lst->first; ref; ref = ref->next) {
				kfe = ref->ref;
				knife_project_v2(kcd, kfe->v1->cageco, se1);
				knife_project_v2(kcd, kfe->v2->cageco, se2);
				d = dist_squared_to_line_segment_v2(s, se1, se2);
				if (d < face_tol_sq) {
					return false;
				}
			}

			transform_point_by_tri_v3(
			        hit_co, hit_cageco,
			        tri[0]->v->co, tri[1]->v->co, tri[2]->v->co,
			        lv1, lv2, lv3);
			return true;
		}
	}
	return false;
}

/* Calculate maximum excursion from (0,0,0) of mesh */
static void calc_ortho_extent(KnifeTool_OpData *kcd)
{
	BMIter iter;
	BMVert *v;
	BMesh *bm = kcd->em->bm;
	float max_xyz = 0.0f;
	int i;

	BM_ITER_MESH (v, &iter, bm, BM_VERTS_OF_MESH) {
		for (i = 0; i < 3; i++)
			max_xyz = max_ff(max_xyz, fabsf(v->co[i]));
	}
	kcd->ortho_extent = max_xyz;
}

/* Check if p is visible (not clipped, not occluded by another face).
 * s in screen projection of p. */
static bool point_is_visible(KnifeTool_OpData *kcd, const float p[3], const float s[2], bglMats *mats)
{
	BMFace *f_hit;

	/* If box clipping on, make sure p is not clipped */
	if (kcd->vc.rv3d->rflag & RV3D_CLIPPING &&
	    ED_view3d_clipping_test(kcd->vc.rv3d, p, true))
	{
		return false;
	}

	/* If not cutting through, make sure no face is in front of p */
	if (!kcd->cut_through) {
		float dist;
		float view[3], p_ofs[3];

		/* TODO: I think there's a simpler way to get the required raycast ray */
		ED_view3d_unproject(mats, view, s[0], s[1], 0.0f);

		mul_m4_v3(kcd->ob->imat, view);

		/* make p_ofs a little towards view, so ray doesn't hit p's face. */
		sub_v3_v3(view, p);
		dist = normalize_v3(view);
		madd_v3_v3v3fl(p_ofs, p, view, KNIFE_FLT_EPSBIG * 3.0f);

		/* avoid projecting behind the viewpoint */
		if (kcd->is_ortho && (kcd->vc.rv3d->persp != RV3D_CAMOB)) {
			dist = kcd->vc.v3d->far * 2.0f;
		}

		if (kcd->vc.rv3d->rflag & RV3D_CLIPPING) {
			float view_clip[2][3];
			/* note: view_clip[0] should never get clipped */
			copy_v3_v3(view_clip[0], p_ofs);
			madd_v3_v3v3fl(view_clip[1], p_ofs, view, dist);

			if (clip_segment_v3_plane_n(view_clip[0], view_clip[1], kcd->vc.rv3d->clip_local, 6)) {
				dist = len_v3v3(p_ofs, view_clip[1]);
			}
		}

		/* see if there's a face hit between p1 and the view */
		f_hit = BKE_bmbvh_ray_cast(kcd->bmbvh, p_ofs, view, KNIFE_FLT_EPS, &dist, NULL, NULL);
		if (f_hit)
			return false;
	}

	return true;
}

/* Clip the line (v1, v2) to planes perpendicular to it and distances d from
 * the closest point on the line to the origin */
static void clip_to_ortho_planes(float v1[3], float v2[3], float d)
{
	float closest[3];
	const float origin[3] = {0.0f, 0.0f, 0.0f};

	closest_to_line_v3(closest, origin, v1, v2);
	dist_ensure_v3_v3fl(v1, closest, d);
	dist_ensure_v3_v3fl(v2, closest, d);
}

static void set_linehit_depth(KnifeTool_OpData *kcd, KnifeLineHit *lh)
{
	lh->m = dot_m4_v3_row_z(kcd->vc.rv3d->persmatob, lh->cagehit);
}

/* Finds visible (or all, if cutting through) edges that intersects the current screen drag line */
static void knife_find_line_hits(KnifeTool_OpData *kcd)
{
	bglMats mats;
	SmallHash faces, kfes, kfvs;
	float v1[3], v2[3], v3[3], v4[3], s1[2], s2[2];
	BVHTree *planetree, *tree;
	BVHTreeOverlap *results, *result;
	BMLoop **ls;
	BMFace *f;
	KnifeEdge *kfe;
	KnifeVert *v;
	ListBase *lst;
	Ref *ref;
	KnifeLineHit *linehits = NULL;
	BLI_array_declare(linehits);
	SmallHashIter hiter;
	KnifeLineHit hit;
	void *val;
	float plane_cos[12];
	float s[2], se1[2], se2[2], sint[2];
	float r1[3], r2[3];
	float d, d1, d2, lambda;
	float vert_tol, vert_tol_sq;
	float line_tol, line_tol_sq;
	float face_tol, face_tol_sq;
	float eps_scale;
	int isect_kind;
	unsigned int tot;
	int i;

	bgl_get_mats(&mats);

	if (kcd->linehits) {
		MEM_freeN(kcd->linehits);
		kcd->linehits = NULL;
		kcd->totlinehit = 0;
	}

	copy_v3_v3(v1, kcd->prev.cage);
	copy_v3_v3(v2, kcd->curr.cage);

	/* project screen line's 3d coordinates back into 2d */
	knife_project_v2(kcd, v1, s1);
	knife_project_v2(kcd, v2, s2);

	if (kcd->is_interactive) {
		if (len_squared_v2v2(s1, s2) < 1.0f) {
			return;
		}
	}
	else {
		if (len_squared_v2v2(s1, s2) < KNIFE_FLT_EPS_SQUARED) {
			return;
		}
	}

	/* unproject screen line */
	ED_view3d_win_to_segment(kcd->ar, kcd->vc.v3d, s1, v1, v3, true);
	ED_view3d_win_to_segment(kcd->ar, kcd->vc.v3d, s2, v2, v4, true);

	mul_m4_v3(kcd->ob->imat, v1);
	mul_m4_v3(kcd->ob->imat, v2);
	mul_m4_v3(kcd->ob->imat, v3);
	mul_m4_v3(kcd->ob->imat, v4);

	/* numeric error, 'v1' -> 'v2', 'v2' -> 'v4' can end up being ~2000 units apart in otho mode
	 * (from ED_view3d_win_to_segment_clip() above)
	 * this gives precision error; rather then solving properly
	 * (which may involve using doubles everywhere!),
	 * limit the distance between these points */
	if (kcd->is_ortho && (kcd->vc.rv3d->persp != RV3D_CAMOB)) {
		if (kcd->ortho_extent == 0.0f)
			calc_ortho_extent(kcd);
		clip_to_ortho_planes(v1, v3, kcd->ortho_extent + 10.0f);
		clip_to_ortho_planes(v2, v4, kcd->ortho_extent + 10.0f);
	}

	/* First use bvh tree to find faces, knife edges, and knife verts that might
	 * intersect the cut plane with rays v1-v3 and v2-v4.
	 * This deduplicates the candidates before doing more expensive intersection tests. */

	tree = BKE_bmbvh_tree_get(kcd->bmbvh);
	planetree = BLI_bvhtree_new(4, FLT_EPSILON * 4, 8, 8);
	copy_v3_v3(plane_cos + 0, v1);
	copy_v3_v3(plane_cos + 3, v2);
	copy_v3_v3(plane_cos + 6, v3);
	copy_v3_v3(plane_cos + 9, v4);
	BLI_bvhtree_insert(planetree, 0, plane_cos, 4);
	BLI_bvhtree_balance(planetree);

	results = BLI_bvhtree_overlap(tree, planetree, &tot);
	if (!results) {
		BLI_bvhtree_free(planetree);
		return;
	}

	BLI_smallhash_init(&faces);
	BLI_smallhash_init(&kfes);
	BLI_smallhash_init(&kfvs);

	for (i = 0, result = results; i < tot; i++, result++) {
		ls = (BMLoop **)kcd->em->looptris[result->indexA];
		f = ls[0]->f;
		set_lowest_face_tri(kcd, f, result->indexA);
		/* for faces, store index of lowest hit looptri in hash */
		if (BLI_smallhash_haskey(&faces, (uintptr_t)f)) {
			continue;
		}
		/* don't care what the value is except that it is non-NULL, for iterator */
		BLI_smallhash_insert(&faces, (uintptr_t)f, f);

		lst = knife_get_face_kedges(kcd, f);
		for (ref = lst->first; ref; ref = ref->next) {
			kfe = ref->ref;
			if (BLI_smallhash_haskey(&kfes, (uintptr_t)kfe))
				continue;
			BLI_smallhash_insert(&kfes, (uintptr_t)kfe, kfe);
			v = kfe->v1;
			if (!BLI_smallhash_haskey(&kfvs, (uintptr_t)v))
				BLI_smallhash_insert(&kfvs, (uintptr_t)v, v);
			v = kfe->v2;
			if (!BLI_smallhash_haskey(&kfvs, (uintptr_t)v))
				BLI_smallhash_insert(&kfvs, (uintptr_t)v, v);
		}
	}

	/* Now go through the candidates and find intersections */
	/* These tolerances, in screen space, are for intermediate hits, as ends are already snapped to screen */
	{
		/* Scale the epsilon by the zoom level
		 * to compensate for projection imprecision, see T41164 */
		float zoom_xy[2] = {kcd->vc.rv3d->winmat[0][0],
		                    kcd->vc.rv3d->winmat[1][1]};
		eps_scale = len_v2(zoom_xy);
	}

	vert_tol = KNIFE_FLT_EPS_PX * eps_scale;
	line_tol = KNIFE_FLT_EPS_PX * eps_scale;
	face_tol = max_ff(vert_tol, line_tol);

	vert_tol_sq = vert_tol * vert_tol;
	line_tol_sq = line_tol * line_tol;
	face_tol_sq = face_tol * face_tol;

	/* Assume these tolerances swamp floating point rounding errors in calculations below */

	/* first look for vertex hits */
	for (val = BLI_smallhash_iternew(&kfvs, &hiter, (uintptr_t *)&v); val;
	     val = BLI_smallhash_iternext(&hiter, (uintptr_t *)&v))
	{
		knife_project_v2(kcd, v->cageco, s);
		d = dist_squared_to_line_segment_v2(s, s1, s2);
		if (d <= vert_tol_sq) {
			if (point_is_visible(kcd, v->cageco, s, &mats)) {
				memset(&hit, 0, sizeof(hit));
				hit.v = v;
				copy_v3_v3(hit.hit, v->co);
				copy_v3_v3(hit.cagehit, v->cageco);
				copy_v2_v2(hit.schit, s);
				set_linehit_depth(kcd, &hit);
				BLI_array_append(linehits, hit);
			}
		}
	}
	/* now edge hits; don't add if a vertex at end of edge should have hit */
	for (val = BLI_smallhash_iternew(&kfes, &hiter, (uintptr_t *)&kfe); val;
	     val = BLI_smallhash_iternext(&hiter, (uintptr_t *)&kfe))
	{
		knife_project_v2(kcd, kfe->v1->cageco, se1);
		knife_project_v2(kcd, kfe->v2->cageco, se2);
		isect_kind = isect_seg_seg_v2_point(s1, s2, se1, se2, sint);
		if (isect_kind == -1) {
			/* isect_seg_seg_v2 doesn't do tolerance test around ends of s1-s2 */
			closest_to_line_segment_v2(sint, s1, se1, se2);
			if (len_squared_v2v2(sint, s1) <= line_tol_sq)
				isect_kind = 1;
			else {
				closest_to_line_segment_v2(sint, s2, se1, se2);
				if (len_squared_v2v2(sint, s2) <= line_tol_sq)
					isect_kind = 1;
			}
		}
		if (isect_kind == 1) {
			d1 = len_v2v2(sint, se1);
			d2 = len_v2v2(se2, se1);
			if (!(d1 <= line_tol || d2 <= line_tol || fabsf(d1 - d2) <= line_tol)) {
				float p_cage[3], p_cage_tmp[3];
				lambda = d1 / d2;
				/* Can't just interpolate between ends of kfe because
				 * that doesn't work with perspective transformation.
				 * Need to find 3d intersection of ray through sint */
				knife_input_ray_segment(kcd, sint, 1.0f, r1, r2);
				isect_kind = isect_line_line_v3(kfe->v1->cageco, kfe->v2->cageco, r1, r2, p_cage, p_cage_tmp);
				if (isect_kind >= 1 && point_is_visible(kcd, p_cage, sint, &mats)) {
					memset(&hit, 0, sizeof(hit));
					if (kcd->snap_midpoints) {
						/* choose intermediate point snap too */
						mid_v3_v3v3(p_cage, kfe->v1->cageco, kfe->v2->cageco);
						mid_v2_v2v2(sint, se1, se2);
						lambda = 0.5f;
					}
					hit.kfe = kfe;
					transform_point_by_seg_v3(
					        hit.hit, p_cage,
					        kfe->v1->co, kfe->v2->co,
					        kfe->v1->cageco, kfe->v2->cageco);
					copy_v3_v3(hit.cagehit, p_cage);
					copy_v2_v2(hit.schit, sint);
					hit.perc = lambda;
					set_linehit_depth(kcd, &hit);
					BLI_array_append(linehits, hit);
				}
			}
		}
	}
	/* now face hits; don't add if a vertex or edge in face should have hit */
	for (val = BLI_smallhash_iternew(&faces, &hiter, (uintptr_t *)&f); val;
	     val = BLI_smallhash_iternext(&hiter, (uintptr_t *)&f))
	{
		float p[3], p_cage[3];

		if (knife_ray_intersect_face(kcd, s1, v1, v3, f, face_tol_sq, p, p_cage)) {
			if (point_is_visible(kcd, p_cage, s1, &mats)) {
				memset(&hit, 0, sizeof(hit));
				hit.f = f;
				copy_v3_v3(hit.hit, p);
				copy_v3_v3(hit.cagehit, p_cage);
				copy_v2_v2(hit.schit, s1);
				set_linehit_depth(kcd, &hit);
				BLI_array_append(linehits, hit);
			}
		}
		if (knife_ray_intersect_face(kcd, s2, v2, v4, f, face_tol_sq, p, p_cage)) {
			if (point_is_visible(kcd, p_cage, s2, &mats)) {
				memset(&hit, 0, sizeof(hit));
				hit.f = f;
				copy_v3_v3(hit.hit, p);
				copy_v3_v3(hit.cagehit, p_cage);
				copy_v2_v2(hit.schit, s2);
				set_linehit_depth(kcd, &hit);
				BLI_array_append(linehits, hit);
			}
		}
	}

	kcd->linehits = linehits;
	kcd->totlinehit = BLI_array_count(linehits);

	/* find position along screen line, used for sorting */
	for (i = 0; i < kcd->totlinehit; i++) {
		KnifeLineHit *lh = kcd->linehits + i;

		lh->l = len_v2v2(lh->schit, s1) / len_v2v2(s2, s1);
	}

	BLI_smallhash_release(&faces);
	BLI_smallhash_release(&kfes);
	BLI_smallhash_release(&kfvs);
	BLI_bvhtree_free(planetree);
	if (results)
		MEM_freeN(results);
}

static void knife_input_ray_segment(KnifeTool_OpData *kcd, const float mval[2], const float ofs,
                                    float r_origin[3], float r_origin_ofs[3])
{
	bglMats mats;

	bgl_get_mats(&mats);

	/* unproject to find view ray */
	ED_view3d_unproject(&mats, r_origin,     mval[0], mval[1], 0.0f);
	ED_view3d_unproject(&mats, r_origin_ofs, mval[0], mval[1], ofs);

	/* transform into object space */
	invert_m4_m4(kcd->ob->imat, kcd->ob->obmat); 

	mul_m4_v3(kcd->ob->imat, r_origin);
	mul_m4_v3(kcd->ob->imat, r_origin_ofs);
}

static BMFace *knife_find_closest_face(KnifeTool_OpData *kcd, float co[3], float cageco[3], bool *is_space)
{
	BMFace *f;
	float dist = KMAXDIST;
	float origin[3];
	float origin_ofs[3];
	float ray[3];

	/* unproject to find view ray */
	knife_input_ray_segment(kcd, kcd->curr.mval, 1.0f, origin, origin_ofs);
	sub_v3_v3v3(ray, origin_ofs, origin);

	f = BKE_bmbvh_ray_cast(kcd->bmbvh, origin, ray, 0.0f, NULL, co, cageco);

	if (is_space)
		*is_space = !f;

	if (!f) {
		if (kcd->is_interactive) {
			/* try to use backbuffer selection method if ray casting failed */
			f = EDBM_face_find_nearest(&kcd->vc, &dist);

			/* cheat for now; just put in the origin instead
			 * of a true coordinate on the face.
			 * This just puts a point 1.0f infront of the view. */
			add_v3_v3v3(co, origin, ray);
		}
	}

	return f;
}

/* find the 2d screen space density of vertices within a radius.  used to scale snapping
 * distance for picking edges/verts.*/
static int knife_sample_screen_density(KnifeTool_OpData *kcd, const float radius)
{
	BMFace *f;
	bool is_space;
	float co[3], cageco[3], sco[2];

	BLI_assert(kcd->is_interactive == true);

	f = knife_find_closest_face(kcd, co, cageco, &is_space);

	if (f && !is_space) {
		const float radius_sq = radius * radius;
		ListBase *lst;
		Ref *ref;
		float dis_sq;
		int c = 0;

		knife_project_v2(kcd, cageco, sco);

		lst = knife_get_face_kedges(kcd, f);
		for (ref = lst->first; ref; ref = ref->next) {
			KnifeEdge *kfe = ref->ref;
			int i;

			for (i = 0; i < 2; i++) {
				KnifeVert *kfv = i ? kfe->v2 : kfe->v1;

				knife_project_v2(kcd, kfv->cageco, kfv->sco);

				dis_sq = len_squared_v2v2(kfv->sco, sco);
				if (dis_sq < radius_sq) {
					if (kcd->vc.rv3d->rflag & RV3D_CLIPPING) {
						if (ED_view3d_clipping_test(kcd->vc.rv3d, kfv->cageco, true) == 0) {
							c++;
						}
					}
					else {
						c++;
					}
				}
			}
		}

		return c;
	}

	return 0;
}

/* returns snapping distance for edges/verts, scaled by the density of the
 * surrounding mesh (in screen space)*/
static float knife_snap_size(KnifeTool_OpData *kcd, float maxsize)
{
	float density;

	if (kcd->is_interactive) {
		density = (float)knife_sample_screen_density(kcd, maxsize * 2.0f);
	}
	else {
		density = 1.0f;
	}

	if (density < 1.0f)
		density = 1.0f;

	return min_ff(maxsize / (density * 0.5f), maxsize);
}

/* p is closest point on edge to the mouse cursor */
static KnifeEdge *knife_find_closest_edge(KnifeTool_OpData *kcd, float p[3], float cagep[3],
                                          BMFace **fptr, bool *is_space)
{
	BMFace *f;
	float co[3], cageco[3], sco[2];
	float maxdist = knife_snap_size(kcd, kcd->ethresh);

	if (kcd->ignore_vert_snapping)
		maxdist *= 0.5f;

	f = knife_find_closest_face(kcd, co, cageco, NULL);
	*is_space = !f;

	/* set p to co, in case we don't find anything, means a face cut */
	copy_v3_v3(p, co);
	copy_v3_v3(cagep, cageco);

	kcd->curr.bmface = f;

	if (f) {
		const float maxdist_sq = maxdist * maxdist;
		KnifeEdge *cure = NULL;
		float cur_cagep[3];
		ListBase *lst;
		Ref *ref;
		float dis_sq, curdis_sq = FLT_MAX;

		knife_project_v2(kcd, cageco, sco);

		/* look through all edges associated with this face */
		lst = knife_get_face_kedges(kcd, f);
		for (ref = lst->first; ref; ref = ref->next) {
			KnifeEdge *kfe = ref->ref;
			float test_cagep[3];
			float lambda;

			/* project edge vertices into screen space */
			knife_project_v2(kcd, kfe->v1->cageco, kfe->v1->sco);
			knife_project_v2(kcd, kfe->v2->cageco, kfe->v2->sco);

			/* check if we're close enough and calculate 'lambda' */
			if (kcd->is_angle_snapping) {
			/* if snapping, check we're in bounds */
				float sco_snap[2];
				isect_line_line_v2_point(kfe->v1->sco, kfe->v2->sco, kcd->prev.mval, kcd->curr.mval, sco_snap);
				lambda = line_point_factor_v2(sco_snap, kfe->v1->sco, kfe->v2->sco);

				/* be strict about angle-snapping within edge */
				if ((lambda < 0.0f - KNIFE_FLT_EPSBIG) || (lambda > 1.0f + KNIFE_FLT_EPSBIG)) {
					continue;
				}

				dis_sq = len_squared_v2v2(sco, sco_snap);
				if (dis_sq < curdis_sq && dis_sq < maxdist_sq) {
					/* we already have 'lambda' */
				}
				else {
					continue;
				}
			}
			else {
				dis_sq = dist_squared_to_line_segment_v2(sco, kfe->v1->sco, kfe->v2->sco);
				if (dis_sq < curdis_sq && dis_sq < maxdist_sq) {
					lambda = line_point_factor_v2(sco, kfe->v1->sco, kfe->v2->sco);
				}
				else {
					continue;
				}
			}

			/* now we have 'lambda' calculated */
			interp_v3_v3v3(test_cagep, kfe->v1->cageco, kfe->v2->cageco, lambda);

			if (kcd->vc.rv3d->rflag & RV3D_CLIPPING) {
				/* check we're in the view */
				if (ED_view3d_clipping_test(kcd->vc.rv3d, test_cagep, true)) {
					continue;
				}
			}

			cure = kfe;
			curdis_sq = dis_sq;
			copy_v3_v3(cur_cagep, test_cagep);
		}

		if (fptr)
			*fptr = f;

		if (cure) {
			if (!kcd->ignore_edge_snapping || !(cure->e)) {
				KnifeVert *edgesnap = NULL;

				if (kcd->snap_midpoints) {
					mid_v3_v3v3(p, cure->v1->co, cure->v2->co);
					mid_v3_v3v3(cagep, cure->v1->cageco, cure->v2->cageco);
				}
				else {
					float lambda = line_point_factor_v3(cur_cagep, cure->v1->cageco, cure->v2->cageco);
					copy_v3_v3(cagep, cur_cagep);
					interp_v3_v3v3(p, cure->v1->co, cure->v2->co, lambda);
				}

				/* update mouse coordinates to the snapped-to edge's screen coordinates
				 * this is important for angle snap, which uses the previous mouse position */
				edgesnap = new_knife_vert(kcd, p, cagep);
				kcd->curr.mval[0] = edgesnap->sco[0];
				kcd->curr.mval[1] = edgesnap->sco[1];

			}
			else {
				return NULL;
			}
		}

		return cure;
	}

	if (fptr)
		*fptr = NULL;

	return NULL;
}

/* find a vertex near the mouse cursor, if it exists */
static KnifeVert *knife_find_closest_vert(KnifeTool_OpData *kcd, float p[3], float cagep[3], BMFace **fptr,
                                          bool *is_space)
{
	BMFace *f;
	float co[3], cageco[3], sco[2], maxdist = knife_snap_size(kcd, kcd->vthresh);

	if (kcd->ignore_vert_snapping)
		maxdist *= 0.5f;

	f = knife_find_closest_face(kcd, co, cageco, is_space);

	/* set p to co, in case we don't find anything, means a face cut */
	copy_v3_v3(p, co);
	copy_v3_v3(cagep, cageco);
	kcd->curr.bmface = f;

	if (f) {
		const float maxdist_sq = maxdist * maxdist;
		ListBase *lst;
		Ref *ref;
		KnifeVert *curv = NULL;
		float dis_sq, curdis_sq = FLT_MAX;

		knife_project_v2(kcd, cageco, sco);

		lst = knife_get_face_kedges(kcd, f);
		for (ref = lst->first; ref; ref = ref->next) {
			KnifeEdge *kfe = ref->ref;
			int i;

			for (i = 0; i < 2; i++) {
				KnifeVert *kfv = i ? kfe->v2 : kfe->v1;

				knife_project_v2(kcd, kfv->cageco, kfv->sco);

				/* be strict about angle snapping, the vertex needs to be very close to the angle, or we ignore */
				if (kcd->is_angle_snapping) {
					if (dist_squared_to_line_segment_v2(kfv->sco, kcd->prev.mval, kcd->curr.mval) > KNIFE_FLT_EPSBIG) {
						continue;
					}
				}

				dis_sq = len_squared_v2v2(kfv->sco, sco);
				if (dis_sq < curdis_sq && dis_sq < maxdist_sq) {
					if (kcd->vc.rv3d->rflag & RV3D_CLIPPING) {
						if (ED_view3d_clipping_test(kcd->vc.rv3d, kfv->cageco, true) == 0) {
							curv = kfv;
							curdis_sq = dis_sq;
						}
					}
					else {
						curv = kfv;
						curdis_sq = dis_sq;
					}
				}
			}
		}

		if (!kcd->ignore_vert_snapping || !(curv && curv->v)) {
			if (fptr)
				*fptr = f;

			if (curv) {
				copy_v3_v3(p, curv->co);
				copy_v3_v3(cagep, curv->cageco);

				/* update mouse coordinates to the snapped-to vertex's screen coordinates
				 * this is important for angle snap, which uses the previous mouse position */
				kcd->curr.mval[0] = curv->sco[0];
				kcd->curr.mval[1] = curv->sco[1];
			}

			return curv;
		}
		else {
			if (fptr)
				*fptr = f;

			return NULL;
		}
	}

	if (fptr)
		*fptr = NULL;

	return NULL;
}

/* update both kcd->curr.mval and kcd->mval to snap to required angle */
static bool knife_snap_angle(KnifeTool_OpData *kcd)
{
	float dx, dy;
	float w, abs_tan;

	dx = kcd->curr.mval[0] - kcd->prev.mval[0];
	dy = kcd->curr.mval[1] - kcd->prev.mval[1];
	if (dx == 0.0f && dy == 0.0f)
		return false;

	if (dx == 0.0f) {
		kcd->angle_snapping = ANGLE_90;
		kcd->curr.mval[0] = kcd->prev.mval[0];
	}

	w = dy / dx;
	abs_tan = fabsf(w);
	if (abs_tan <= 0.4142f) { /* tan(22.5 degrees) = 0.4142 */
		kcd->angle_snapping = ANGLE_0;
		kcd->curr.mval[1] = kcd->prev.mval[1];
	}
	else if (abs_tan < 2.4142f) { /* tan(67.5 degrees) = 2.4142 */
		if (w > 0) {
			kcd->angle_snapping = ANGLE_45;
			kcd->curr.mval[1] = kcd->prev.mval[1] + dx;
		}
		else {
			kcd->angle_snapping = ANGLE_135;
			kcd->curr.mval[1] = kcd->prev.mval[1] - dx;
		}
	}
	else {
		kcd->angle_snapping = ANGLE_90;
		kcd->curr.mval[0] = kcd->prev.mval[0];
	}

	copy_v2_v2(kcd->mval, kcd->curr.mval);

	return true;
}

/* update active knife edge/vert pointers */
static int knife_update_active(KnifeTool_OpData *kcd)
{
	knife_pos_data_clear(&kcd->curr);
	copy_v2_v2(kcd->curr.mval, kcd->mval);

	/* view matrix may have changed, reproject */
	knife_project_v2(kcd, kcd->prev.cage, kcd->prev.mval);

	if (kcd->angle_snapping != ANGLE_FREE && kcd->mode == MODE_DRAGGING) {
		kcd->is_angle_snapping = knife_snap_angle(kcd);
	}
	else {
		kcd->is_angle_snapping = false;
	}

	kcd->curr.vert = knife_find_closest_vert(kcd, kcd->curr.co, kcd->curr.cage, &kcd->curr.bmface, &kcd->curr.is_space);

	if (!kcd->curr.vert) {
		kcd->curr.edge = knife_find_closest_edge(kcd, kcd->curr.co, kcd->curr.cage,
		                                         &kcd->curr.bmface, &kcd->curr.is_space);
	}

	/* if no hits are found this would normally default to (0, 0, 0) so instead
	 * get a point at the mouse ray closest to the previous point.
	 * Note that drawing lines in `free-space` isn't properly supported
	 * but theres no guarantee (0, 0, 0) has any geometry either - campbell */
	if (kcd->curr.vert == NULL && kcd->curr.edge == NULL && kcd->curr.bmface == NULL) {
		float origin[3];
		float origin_ofs[3];

		knife_input_ray_segment(kcd, kcd->curr.mval, 1.0f, origin, origin_ofs);

		closest_to_line_v3(kcd->curr.cage, kcd->prev.cage, origin_ofs, origin);
		copy_v3_v3(kcd->curr.co, kcd->curr.cage);
	}

	if (kcd->mode == MODE_DRAGGING) {
		knife_find_line_hits(kcd);
	}
	return 1;
}

/* sort list of kverts by fraction along edge e */
static void sort_by_frac_along(ListBase *lst, BMEdge *e)
{
	/* note, since we know the point is along the edge, sort from distance to v1co */
	const float *v1co = e->v1->co;
	Ref *cur = NULL, *prev = NULL, *next = NULL;

	if (lst->first == lst->last)
		return;

	for (cur = ((Ref *)lst->first)->next; cur; cur = next) {
		KnifeVert *vcur = cur->ref;
		const float vcur_fac_sq = len_squared_v3v3(v1co, vcur->co);

		next = cur->next;
		prev = cur->prev;

		BLI_remlink(lst, cur);

		while (prev) {
			KnifeVert *vprev = prev->ref;
			if (len_squared_v3v3(v1co, vprev->co) <= vcur_fac_sq)
				break;
			prev = prev->prev;
		}

		BLI_insertlinkafter(lst, prev, cur);
	}
}

/* The chain so far goes from an instantiated vertex to kfv (some may be reversed).
 * If possible, complete the chain to another instantiated vertex and return 1, else return 0.
 * The visited hash says which KnifeVert's have already been tried, not including kfv. */
static bool find_chain_search(KnifeTool_OpData *kcd, KnifeVert *kfv, ListBase *fedges, SmallHash *visited,
                              ListBase *chain)
{
	Ref *r;
	KnifeEdge *kfe;
	KnifeVert *kfv_other;

	if (kfv->v)
		return true;

	BLI_smallhash_insert(visited, (uintptr_t)kfv, NULL);
	/* Try all possible next edges. Could either go through fedges
	 * (all the KnifeEdges for the face being cut) or could go through
	 * kve->edges and restrict to cutting face and uninstantiated edges.
	 * Not clear which is better. Let's do the first. */
	for (r = fedges->first; r; r = r->next) {
		kfe = r->ref;
		kfv_other = NULL;
		if (kfe->v1 == kfv)
			kfv_other = kfe->v2;
		else if (kfe->v2 == kfv)
			kfv_other = kfe->v1;
		if (kfv_other && !BLI_smallhash_haskey(visited, (uintptr_t)kfv_other)) {
			knife_append_list(kcd, chain, kfe);
			if (find_chain_search(kcd, kfv_other, fedges, visited, chain))
				return true;
			BLI_remlink(chain, chain->last);
		}
	}
	return false;
}

static ListBase *find_chain_from_vertex(KnifeTool_OpData *kcd, KnifeEdge *kfe, BMVert *v, ListBase *fedges)
{
	SmallHash visited_, *visited = &visited_;
	ListBase *ans;
	bool found;

	ans = knife_empty_list(kcd);
	knife_append_list(kcd, ans, kfe);
	found = false;
	BLI_smallhash_init(visited);
	if (kfe->v1->v == v) {
		BLI_smallhash_insert(visited, (uintptr_t)(kfe->v1), NULL);
		found = find_chain_search(kcd, kfe->v2, fedges, visited, ans);
	}
	else {
		BLI_assert(kfe->v2->v == v);
		BLI_smallhash_insert(visited, (uintptr_t)(kfe->v2), NULL);
		found = find_chain_search(kcd, kfe->v1, fedges, visited, ans);
	}

	BLI_smallhash_release(visited);

	if (found)
		return ans;
	else
		return NULL;
}

/* Find a chain in fedges from one instantiated vertex to another.
 * Remove the edges in the chain from fedges and return a separate list of the chain. */
static ListBase *find_chain(KnifeTool_OpData *kcd, ListBase *fedges)
{
	Ref *r, *ref;
	KnifeEdge *kfe;
	BMVert *v1, *v2;
	ListBase *ans;

	ans = NULL;

	for (r = fedges->first; r; r = r->next) {
		kfe = r->ref;
		v1 = kfe->v1->v;
		v2 = kfe->v2->v;
		if (v1 && v2) {
			ans = knife_empty_list(kcd);
			knife_append_list(kcd, ans, kfe);
			break;
		}
		if (v1)
			ans = find_chain_from_vertex(kcd, kfe, v1, fedges);
		else if (v2)
			ans = find_chain_from_vertex(kcd, kfe, v2, fedges);
		if (ans)
			break;
	}
	if (ans) {
		BLI_assert(BLI_countlist(ans) > 0);
		for (r = ans->first; r; r = r->next) {
			ref = find_ref(fedges, r->ref);
			BLI_assert(ref != NULL);
			BLI_remlink(fedges, ref);
		}
	}
	return ans;
}

/* The hole so far goes from kfvfirst to kfv (some may be reversed).
 * If possible, complete the hole back to kfvfirst and return 1, else return 0.
 * The visited hash says which KnifeVert's have already been tried, not including kfv or kfvfirst. */
static bool find_hole_search(KnifeTool_OpData *kcd, KnifeVert *kfvfirst, KnifeVert *kfv, ListBase *fedges,
                             SmallHash *visited, ListBase *hole)
{
	Ref *r;
	KnifeEdge *kfe, *kfelast;
	KnifeVert *kfv_other;

	if (kfv == kfvfirst)
		return true;

	BLI_smallhash_insert(visited, (uintptr_t)kfv, NULL);
	kfelast = ((Ref *)hole->last)->ref;
	for (r = fedges->first; r; r = r->next) {
		kfe = r->ref;
		if (kfe == kfelast)
			continue;
		if (kfe->v1->v || kfe->v2->v)
			continue;
		kfv_other = NULL;
		if (kfe->v1 == kfv)
			kfv_other = kfe->v2;
		else if (kfe->v2 == kfv)
			kfv_other = kfe->v1;
		if (kfv_other && !BLI_smallhash_haskey(visited, (uintptr_t)kfv_other)) {
			knife_append_list(kcd, hole, kfe);
			if (find_hole_search(kcd, kfvfirst, kfv_other, fedges, visited, hole))
				return true;
			BLI_remlink(hole, hole->last);
		}
	}
	return false;
}

/* Find a hole (simple cycle with no instantiated vertices).
 * Remove the edges in the cycle from fedges and return a separate list of the cycle */
static ListBase *find_hole(KnifeTool_OpData *kcd, ListBase *fedges)
{
	ListBase *ans;
	Ref *r, *ref;
	KnifeEdge *kfe;
	SmallHash visited_, *visited = &visited_;
	bool found;

	ans = NULL;
	found = false;

	for (r = fedges->first; r && !found; r = r->next) {
		kfe = r->ref;
		if (kfe->v1->v || kfe->v2->v || kfe->v1 == kfe->v2)
			continue;

		BLI_smallhash_init(visited);
		ans = knife_empty_list(kcd);
		knife_append_list(kcd, ans, kfe);

		found = find_hole_search(kcd, kfe->v1, kfe->v2, fedges, visited, ans);

		BLI_smallhash_release(visited);
	}

	if (found) {
		for (r = ans->first; r; r = r->next) {
			kfe = r->ref;
			ref = find_ref(fedges, r->ref);
			if (ref)
				BLI_remlink(fedges, ref);
		}
		return ans;
	}
	else {
		return NULL;
	}
}

/* Try to find "nice" diagonals - short, and far apart from each other.
 * If found, return true and make a 'main chain' going across f which uses
 * the two diagonals and one part of the hole, and a 'side chain' that
 * completes the hole. */
static bool find_hole_chains(KnifeTool_OpData *kcd, ListBase *hole, BMFace *f, ListBase **mainchain,
                             ListBase **sidechain)
{
	float **fco, **hco;
	BMVert **fv;
	KnifeVert **hv;
	KnifeEdge **he;
	Ref *r;
	KnifeVert *kfv, *kfvother;
	KnifeEdge *kfe;
	ListBase *chain;
	BMVert *v;
	BMIter iter;
	int nh, nf, i, j, k, m, ax, ay, sep = 0 /* Quite warnings */, bestsep;
	int besti[2], bestj[2];
	float dist_sq, dist_best_sq;

	nh = BLI_countlist(hole);
	nf = f->len;
	if (nh < 2 || nf < 3)
		return false;

	/* Gather 2d projections of hole and face vertex coordinates.
	 * Use best-axis projection - not completely accurate, maybe revisit */
	axis_dominant_v3(&ax, &ay, f->no);
	hco = BLI_memarena_alloc(kcd->arena, nh * sizeof(float *));
	fco = BLI_memarena_alloc(kcd->arena, nf * sizeof(float *));
	hv = BLI_memarena_alloc(kcd->arena, nh * sizeof(KnifeVert *));
	fv = BLI_memarena_alloc(kcd->arena, nf * sizeof(BMVert *));
	he = BLI_memarena_alloc(kcd->arena, nh * sizeof(KnifeEdge *));

	i = 0;
	kfv = NULL;
	kfvother = NULL;
	for (r = hole->first; r; r = r->next) {
		kfe = r->ref;
		he[i] = kfe;
		if (kfvother == NULL) {
			kfv = kfe->v1;
		}
		else {
			kfv = kfvother;
			BLI_assert(kfv == kfe->v1 || kfv == kfe->v2);
		}
		hco[i] = BLI_memarena_alloc(kcd->arena, 2 * sizeof(float));
		hco[i][0] = kfv->co[ax];
		hco[i][1] = kfv->co[ay];
		hv[i] = kfv;
		kfvother = (kfe->v1 == kfv) ? kfe->v2 : kfe->v1;
		i++;
	}

	j = 0;
	BM_ITER_ELEM (v, &iter, f, BM_VERTS_OF_FACE) {
		fco[j] = BLI_memarena_alloc(kcd->arena, 2 * sizeof(float));
		fco[j][0] = v->co[ax];
		fco[j][1] = v->co[ay];
		fv[j] = v;
		j++;
	}

	/* For first diagonal  (m == 0), want shortest length.
	 * For second diagonal (m == 1), want max separation of index of hole
	 * vertex from the hole vertex used in the first diagonal, and from there
	 * want the one with shortest length not to the same vertex as the first diagonal. */
	for (m = 0; m < 2; m++) {
		besti[m] = -1;
		bestj[m] = -1;
		dist_best_sq = FLT_MAX;
		bestsep = 0;
		for (i = 0; i < nh; i++) {
			if (m == 1) {
				if (i == besti[0])
					continue;
				sep = (i + nh - besti[0]) % nh;
				sep = MIN2(sep, nh - sep);
				if (sep < bestsep)
					continue;
				dist_best_sq = FLT_MAX;
			}
			for (j = 0; j < nf; j++) {
				bool ok;

				if (m == 1 && j == bestj[0])
					continue;
				dist_sq = len_squared_v2v2(hco[i], fco[j]);
				if (dist_sq > dist_best_sq)
					continue;

				ok = true;
				for (k = 0; k < nh && ok; k++) {
					if (k == i || (k + 1) % nh == i)
						continue;
					if (isect_line_line_v2(hco[i], fco[j], hco[k], hco[(k + 1) % nh]))
						ok = false;
				}
				if (!ok)
					continue;
				for (k = 0; k < nf && ok; k++) {
					if (k == j || (k + 1) % nf == j)
						continue;
					if (isect_line_line_v2(hco[i], fco[j], fco[k], fco[(k + 1) % nf]))
						ok = false;
				}
				if (ok) {
					besti[m] = i;
					bestj[m] = j;
					if (m == 1)
						bestsep = sep;
					dist_best_sq = dist_sq;
				}
			}
		}
	}

	if (besti[0] != -1 && besti[1] != -1) {
		BLI_assert(besti[0] != besti[1] && bestj[0] != bestj[1]);
		kfe = new_knife_edge(kcd);
		kfe->v1 = get_bm_knife_vert(kcd, fv[bestj[0]]);
		kfe->v2 = hv[besti[0]];
		chain = knife_empty_list(kcd);
		knife_append_list(kcd, chain, kfe);
		for (i = besti[0]; i != besti[1]; i = (i + 1) % nh) {
			knife_append_list(kcd, chain, he[i]);
		}
		kfe = new_knife_edge(kcd);
		kfe->v1 = hv[besti[1]];
		kfe->v2 = get_bm_knife_vert(kcd, fv[bestj[1]]);
		knife_append_list(kcd, chain, kfe);
		*mainchain = chain;

		chain = knife_empty_list(kcd);
		for (i = besti[1]; i != besti[0]; i = (i + 1) % nh) {
			knife_append_list(kcd, chain, he[i]);
		}
		*sidechain = chain;

		return true;
	}
	else {
		return false;
	}
}

static bool knife_verts_edge_in_face(KnifeVert *v1, KnifeVert *v2, BMFace *f)
{
	bool v1_inside, v2_inside;
	bool v1_inface, v2_inface;

	if (!f || !v1 || !v2)
		return false;

	/* find out if v1 and v2, if set, are part of the face */
	v1_inface = v1->v ? BM_vert_in_face(f, v1->v) : false;
	v2_inface = v2->v ? BM_vert_in_face(f, v2->v) : false;

	/* BM_face_point_inside_test uses best-axis projection so this isn't most accurate test... */
	v1_inside = v1_inface ? false : BM_face_point_inside_test(f, v1->co);
	v2_inside = v2_inface ? false : BM_face_point_inside_test(f, v2->co);
	if ((v1_inface && v2_inside) ||
	    (v2_inface && v1_inside) ||
	    (v1_inside && v2_inside))
	{
		return true;
	}

	if (v1_inface && v2_inface) {
		float mid[3];
		/* Can have case where v1 and v2 are on shared chain between two faces.
		 * BM_face_splits_check_legal does visibility and self-intersection tests,
		 * but it is expensive and maybe a bit buggy, so use a simple
		 * "is the midpoint in the face" test */
		mid_v3_v3v3(mid, v1->co, v2->co);
		return BM_face_point_inside_test(f, mid);
	}
	return false;
}

static bool knife_edge_in_face(KnifeEdge *kfe, BMFace *f)
{
	return knife_verts_edge_in_face(kfe->v1, kfe->v2, f);
}

/* Split face f with KnifeEdges on chain.  f remains as one side, the face formed is put in *newface.
 * The new face will be on the left side of the chain as viewed from the normal-out side of f. */
static void knife_make_chain_cut(KnifeTool_OpData *kcd, BMFace *f, ListBase *chain, BMFace **r_f_new)
{
	BMesh *bm = kcd->em->bm;
	KnifeEdge *kfe, *kfelast;
	BMVert *v1, *v2;
	BMLoop *l_v1, *l_v2;
	BMFace *f_new;
	Ref *ref;
	KnifeVert *kfv, *kfvprev;
	BMLoop *l_new, *l_iter;
	int i;
	int nco = BLI_countlist(chain) - 1;
	float (*cos)[3] = BLI_array_alloca(cos, nco);
	KnifeVert **kverts = BLI_array_alloca(kverts, nco);

	kfe = ((Ref *)chain->first)->ref;
	v1 = kfe->v1->v ? kfe->v1->v : kfe->v2->v;
	kfelast = ((Ref *)chain->last)->ref;
	v2 = kfelast->v2->v ? kfelast->v2->v : kfelast->v1->v;
	BLI_assert(v1 != NULL && v2 != NULL);
	kfvprev = kfe->v1->v == v1 ? kfe->v1 : kfe->v2;
	for (ref = chain->first, i = 0; i < nco && ref != chain->last; ref = ref->next, i++) {
		kfe = ref->ref;
		BLI_assert(kfvprev == kfe->v1 || kfvprev == kfe->v2);
		kfv = kfe->v1 == kfvprev ? kfe->v2 : kfe->v1;
		copy_v3_v3(cos[i], kfv->co);
		kverts[i] = kfv;
		kfvprev = kfv;
	}
	BLI_assert(i == nco);
	l_new = NULL;

	if ((l_v1 = BM_face_vert_share_loop(f, v1)) &&
	    (l_v2 = BM_face_vert_share_loop(f, v2)))
	{
		if (nco == 0) {
			/* Want to prevent creating two-sided polygons */
			if (v1 == v2 || BM_edge_exists(v1, v2)) {
				f_new = NULL;
			}
			else {
				f_new = BM_face_split(bm, f, l_v1, l_v2, &l_new, NULL, true);
			}
		}
		else {
			f_new = BM_face_split_n(bm, f, l_v1, l_v2, cos, nco, &l_new, NULL);
			if (f_new) {
				/* Now go through lnew chain matching up chain kv's and assign real v's to them */
				for (l_iter = l_new->next, i = 0; i < nco; l_iter = l_iter->next, i++) {
					BLI_assert(equals_v3v3(cos[i], l_iter->v->co));
					if (kcd->select_result) {
						BM_edge_select_set(bm, l_iter->e, true);
					}
					kverts[i]->v = l_iter->v;
				}
			}
		}
	}
	else {
		f_new = NULL;
	}

	/* the select chain above doesnt account for the first loop */
	if (kcd->select_result) {
		if (l_new) {
			BM_edge_select_set(bm, l_new->e, true);
		}
	}
	else if (f_new) {
		BM_elem_select_copy(bm, bm, f_new, f);
	}

	*r_f_new = f_new;
}

static void knife_make_face_cuts(KnifeTool_OpData *kcd, BMFace *f, ListBase *kfedges)
{
	BMesh *bm = kcd->em->bm;
	KnifeEdge *kfe;
	BMFace *fnew, *fnew2, *fhole;
	ListBase *chain, *hole, *sidechain;
	ListBase *fnew_kfedges, *fnew2_kfedges;
	Ref *ref, *refnext;
	int count, oldcount;

	oldcount = BLI_countlist(kfedges);
	while ((chain = find_chain(kcd, kfedges)) != NULL) {
		knife_make_chain_cut(kcd, f, chain, &fnew);
		if (!fnew) {
			return;
		}

		/* Move kfedges to fnew_kfedges if they are now in fnew.
		 * The chain edges were removed already */
		fnew_kfedges = knife_empty_list(kcd);
		for (ref = kfedges->first; ref; ref = refnext) {
			kfe = ref->ref;
			refnext = ref->next;
			if (knife_edge_in_face(kfe, fnew)) {
				BLI_remlink(kfedges, ref);
				kfe->basef = fnew;
				knife_append_list(kcd, fnew_kfedges, kfe);
			}
		}
		if (fnew_kfedges->first)
			knife_make_face_cuts(kcd, fnew, fnew_kfedges);

		/* find_chain should always remove edges if it returns true,
		 * but guard against infinite loop anyway */
		count = BLI_countlist(kfedges);
		if (count >= oldcount) {
			BLI_assert(!"knife find_chain infinite loop");
			return;
		}
		oldcount = count;
	}

	while ((hole = find_hole(kcd, kfedges)) != NULL) {
		if (find_hole_chains(kcd, hole, f, &chain, &sidechain)) {
			/* chain goes across f and sidechain comes back
			 * from the second last vertex to the second vertex.
			 */
			knife_make_chain_cut(kcd, f, chain, &fnew);
			if (!fnew) {
				BLI_assert(!"knife failed hole cut");
				return;
			}
			kfe = ((Ref *)sidechain->first)->ref;
			if (knife_edge_in_face(kfe, f)) {
				knife_make_chain_cut(kcd, f, sidechain, &fnew2);
				if (fnew2 == NULL) {
					return;
				}
				fhole = f;
			}
			else if (knife_edge_in_face(kfe, fnew)) {
				knife_make_chain_cut(kcd, fnew, sidechain, &fnew2);
				if (fnew2 == NULL) {
					return;
				}
				fhole = fnew2;
			}
			else {
				/* shouldn't happen except in funny edge cases */
				return;
			}
			BM_face_kill(bm, fhole);
			/* Move kfedges to either fnew or fnew2 if appropriate.
			 * The hole edges were removed already */
			fnew_kfedges = knife_empty_list(kcd);
			fnew2_kfedges = knife_empty_list(kcd);
			for (ref = kfedges->first; ref; ref = refnext) {
				kfe = ref->ref;
				refnext = ref->next;
				if (knife_edge_in_face(kfe, fnew)) {
					BLI_remlink(kfedges, ref);
					kfe->basef = fnew;
					knife_append_list(kcd, fnew_kfedges, kfe);
				}
				else if (knife_edge_in_face(kfe, fnew2)) {
					BLI_remlink(kfedges, ref);
					kfe->basef = fnew2;
					knife_append_list(kcd, fnew2_kfedges, kfe);
				}
			}
			/* We'll skip knife edges that are in the newly formed hole.
			 * (Maybe we shouldn't have made a hole in the first place?) */
			if (fnew != fhole && fnew_kfedges->first)
				knife_make_face_cuts(kcd, fnew, fnew_kfedges);
			if (fnew2 != fhole && fnew2_kfedges->first)
				knife_make_face_cuts(kcd, fnew2, fnew2_kfedges);
			if (f == fhole)
				break;
			/* find_hole should always remove edges if it returns true,
			 * but guard against infinite loop anyway */
			count = BLI_countlist(kfedges);
			if (count >= oldcount) {
				BLI_assert(!"knife find_hole infinite loop");
				return;
			}
			oldcount = count;
		}
	}
}

/* Use the network of KnifeEdges and KnifeVerts accumulated to make real BMVerts and BMEdedges */
static void knife_make_cuts(KnifeTool_OpData *kcd)
{
	BMesh *bm = kcd->em->bm;
	KnifeEdge *kfe;
	KnifeVert *kfv;
	BMFace *f;
	BMEdge *e, *enew;
	ListBase *lst;
	Ref *ref;
	float pct;
	SmallHashIter hiter;
	BLI_mempool_iter iter;
	SmallHash fhash_, *fhash = &fhash_;
	SmallHash ehash_, *ehash = &ehash_;

	BLI_smallhash_init(fhash);
	BLI_smallhash_init(ehash);

	/* put list of cutting edges for a face into fhash, keyed by face */
	BLI_mempool_iternew(kcd->kedges, &iter);
	for (kfe = BLI_mempool_iterstep(&iter); kfe; kfe = BLI_mempool_iterstep(&iter)) {
		f = kfe->basef;
		if (!f || kfe->e)
			continue;
		lst = BLI_smallhash_lookup(fhash, (uintptr_t)f);
		if (!lst) {
			lst = knife_empty_list(kcd);
			BLI_smallhash_insert(fhash, (uintptr_t)f, lst);
		}
		knife_append_list(kcd, lst, kfe);
	}

	/* put list of splitting vertices for an edge into ehash, keyed by edge */
	BLI_mempool_iternew(kcd->kverts, &iter);
	for (kfv = BLI_mempool_iterstep(&iter); kfv; kfv = BLI_mempool_iterstep(&iter)) {
		if (kfv->v)
			continue;  /* already have a BMVert */
		for (ref = kfv->edges.first; ref; ref = ref->next) {
			kfe = ref->ref;
			e = kfe->e;
			if (!e)
				continue;
			lst = BLI_smallhash_lookup(ehash, (uintptr_t)e);
			if (!lst) {
				lst = knife_empty_list(kcd);
				BLI_smallhash_insert(ehash, (uintptr_t)e, lst);
			}
			/* there can be more than one kfe in kfv's list with same e */
			if (!find_ref(lst, kfv))
				knife_append_list(kcd, lst, kfv);
		}
	}

	/* split bmesh edges where needed */
	for (lst = BLI_smallhash_iternew(ehash, &hiter, (uintptr_t *)&e); lst;
	     lst = BLI_smallhash_iternext(&hiter, (uintptr_t *)&e))
	{
		sort_by_frac_along(lst, e);
		for (ref = lst->first; ref; ref = ref->next) {
			kfv = ref->ref;
			pct = line_point_factor_v3(kfv->co, e->v1->co, e->v2->co);
			kfv->v = BM_edge_split(bm, e, e->v1, &enew, pct);
		}
	}

	if (kcd->only_select) {
		EDBM_flag_disable_all(kcd->em, BM_ELEM_SELECT);
	}

	/* do cuts for each face */
	for (lst = BLI_smallhash_iternew(fhash, &hiter, (uintptr_t *)&f); lst;
	     lst = BLI_smallhash_iternext(&hiter, (uintptr_t *)&f))
	{
		knife_make_face_cuts(kcd, f, lst);
	}

	BLI_smallhash_release(fhash);
	BLI_smallhash_release(ehash);
}

/* called on tool confirmation */
static void knifetool_finish_ex(KnifeTool_OpData *kcd)
{
	knife_make_cuts(kcd);

	EDBM_selectmode_flush(kcd->em);
	EDBM_mesh_normals_update(kcd->em);
	EDBM_update_generic(kcd->em, true, true);
}
static void knifetool_finish(wmOperator *op)
{
	KnifeTool_OpData *kcd = op->customdata;
	knifetool_finish_ex(kcd);
}

static void knife_recalc_projmat(KnifeTool_OpData *kcd)
{
	invert_m4_m4(kcd->ob->imat, kcd->ob->obmat);
	ED_view3d_ob_project_mat_get(kcd->ar->regiondata, kcd->ob, kcd->projmat);
	//mul_m4_m4m4(kcd->projmat, kcd->vc.rv3d->winmat, kcd->vc.rv3d->viewmat);

	kcd->is_ortho = ED_view3d_clip_range_get(kcd->vc.v3d, kcd->vc.rv3d,
	                                         &kcd->clipsta, &kcd->clipend, true);
}

/* called when modal loop selection is done... */
static void knifetool_exit_ex(bContext *C, KnifeTool_OpData *kcd)
{
	if (!kcd)
		return;

	if (kcd->is_interactive) {
		WM_cursor_modal_restore(CTX_wm_window(C));

		/* deactivate the extra drawing stuff in 3D-View */
		ED_region_draw_cb_exit(kcd->ar->type, kcd->draw_handle);
	}

	/* free the custom data */
	BLI_mempool_destroy(kcd->refs);
	BLI_mempool_destroy(kcd->kverts);
	BLI_mempool_destroy(kcd->kedges);

	BLI_ghash_free(kcd->origedgemap, NULL, NULL);
	BLI_ghash_free(kcd->origvertmap, NULL, NULL);
	BLI_ghash_free(kcd->kedgefacemap, NULL, NULL);
	BLI_ghash_free(kcd->facetrimap, NULL, NULL);

	BKE_bmbvh_free(kcd->bmbvh);
	BLI_memarena_free(kcd->arena);

	/* tag for redraw */
	ED_region_tag_redraw(kcd->ar);

	if (kcd->cagecos)
		MEM_freeN((void *)kcd->cagecos);

	if (kcd->linehits)
		MEM_freeN(kcd->linehits);

	/* destroy kcd itself */
	MEM_freeN(kcd);
}
static void knifetool_exit(bContext *C, wmOperator *op)
{
	KnifeTool_OpData *kcd = op->customdata;
	knifetool_exit_ex(C, kcd);
	op->customdata = NULL;
}

static void knifetool_update_mval(KnifeTool_OpData *kcd, const float mval[2])
{
	knife_recalc_projmat(kcd);
	copy_v2_v2(kcd->mval, mval);

	if (knife_update_active(kcd)) {
		ED_region_tag_redraw(kcd->ar);
	}
}

static void knifetool_update_mval_i(KnifeTool_OpData *kcd, const int mval_i[2])
{
	float mval[2] = {UNPACK2(mval_i)};
	knifetool_update_mval(kcd, mval);
}

/* called when modal loop selection gets set up... */
static void knifetool_init(bContext *C, KnifeTool_OpData *kcd,
                           const bool only_select, const bool cut_through, const bool is_interactive)
{
	Scene *scene = CTX_data_scene(C);
	Object *obedit = CTX_data_edit_object(C);

	/* assign the drawing handle for drawing preview line... */
	kcd->ob = obedit;
	kcd->ar = CTX_wm_region(C);

	em_setup_viewcontext(C, &kcd->vc);

	kcd->em = BKE_editmesh_from_object(kcd->ob);

	BM_mesh_elem_index_ensure(kcd->em->bm, BM_VERT);

	kcd->cagecos = (const float (*)[3])BKE_editmesh_vertexCos_get(kcd->em, scene, NULL);

	kcd->bmbvh = BKE_bmbvh_new_from_editmesh(kcd->em,
	                                         BMBVH_RETURN_ORIG |
	                                         (only_select ? BMBVH_RESPECT_SELECT : BMBVH_RESPECT_HIDDEN),
	                                         kcd->cagecos, false);

	kcd->arena = BLI_memarena_new(MEM_SIZE_OPTIMAL(1 << 15), "knife");
	kcd->vthresh = KMAXDIST - 1;
	kcd->ethresh = KMAXDIST;

	knife_recalc_projmat(kcd);

	ED_region_tag_redraw(kcd->ar);

	kcd->refs = BLI_mempool_create(sizeof(Ref), 0, 2048, 0);
	kcd->kverts = BLI_mempool_create(sizeof(KnifeVert), 0, 512, BLI_MEMPOOL_ALLOW_ITER);
	kcd->kedges = BLI_mempool_create(sizeof(KnifeEdge), 0, 512, BLI_MEMPOOL_ALLOW_ITER);

	kcd->origedgemap = BLI_ghash_ptr_new("knife origedgemap");
	kcd->origvertmap = BLI_ghash_ptr_new("knife origvertmap");
	kcd->kedgefacemap = BLI_ghash_ptr_new("knife kedgefacemap");
	kcd->facetrimap = BLI_ghash_ptr_new("knife facetrimap");

	/* cut all the way through the mesh if use_occlude_geometry button not pushed */
	kcd->is_interactive = is_interactive;
	kcd->cut_through = cut_through;
	kcd->only_select = only_select;

	/* can't usefully select resulting edges in face mode */
	kcd->select_result = (kcd->em->selectmode != SCE_SELECT_FACE);

	knife_pos_data_clear(&kcd->curr);
	knife_pos_data_clear(&kcd->prev);

	if (is_interactive) {
		kcd->draw_handle = ED_region_draw_cb_activate(kcd->ar->type, knifetool_draw, kcd, REGION_DRAW_POST_VIEW);

		knife_init_colors(&kcd->colors);
	}
}

static void knifetool_cancel(bContext *C, wmOperator *op)
{
	/* this is just a wrapper around exit() */
	knifetool_exit(C, op);
}

static int knifetool_invoke(bContext *C, wmOperator *op, const wmEvent *event)
{
	const bool only_select = RNA_boolean_get(op->ptr, "only_selected");
	const bool cut_through = !RNA_boolean_get(op->ptr, "use_occlude_geometry");

	KnifeTool_OpData *kcd;

	if (only_select) {
		Object *obedit = CTX_data_edit_object(C);
		BMEditMesh *em = BKE_editmesh_from_object(obedit);
		if (em->bm->totfacesel == 0) {
			BKE_report(op->reports, RPT_ERROR, "Selected faces required");
			return OPERATOR_CANCELLED;
		}
	}

	view3d_operator_needs_opengl(C);

	/* alloc new customdata */
	kcd = op->customdata = MEM_callocN(sizeof(KnifeTool_OpData), __func__);

	knifetool_init(C, kcd, only_select, cut_through, true);

	/* add a modal handler for this operator - handles loop selection */
	WM_cursor_modal_set(CTX_wm_window(C), BC_KNIFECURSOR);
	WM_event_add_modal_handler(C, op);

	knifetool_update_mval_i(kcd, event->mval);

	knife_update_header(C, kcd);

	return OPERATOR_RUNNING_MODAL;
}

enum {
	KNF_MODAL_CANCEL = 1,
	KNF_MODAL_CONFIRM,
	KNF_MODAL_MIDPOINT_ON,
	KNF_MODAL_MIDPOINT_OFF,
	KNF_MODAL_NEW_CUT,
	KNF_MODEL_IGNORE_SNAP_ON,
	KNF_MODEL_IGNORE_SNAP_OFF,
	KNF_MODAL_ADD_CUT,
	KNF_MODAL_ANGLE_SNAP_TOGGLE,
	KNF_MODAL_CUT_THROUGH_TOGGLE,
	KNF_MODAL_PANNING
};

wmKeyMap *knifetool_modal_keymap(wmKeyConfig *keyconf)
{
	static EnumPropertyItem modal_items[] = {
		{KNF_MODAL_CANCEL, "CANCEL", 0, "Cancel", ""},
		{KNF_MODAL_CONFIRM, "CONFIRM", 0, "Confirm", ""},
		{KNF_MODAL_MIDPOINT_ON, "SNAP_MIDPOINTS_ON", 0, "Snap To Midpoints On", ""},
		{KNF_MODAL_MIDPOINT_OFF, "SNAP_MIDPOINTS_OFF", 0, "Snap To Midpoints Off", ""},
		{KNF_MODEL_IGNORE_SNAP_ON, "IGNORE_SNAP_ON", 0, "Ignore Snapping On", ""},
		{KNF_MODEL_IGNORE_SNAP_OFF, "IGNORE_SNAP_OFF", 0, "Ignore Snapping Off", ""},
		{KNF_MODAL_ANGLE_SNAP_TOGGLE, "ANGLE_SNAP_TOGGLE", 0, "Toggle Angle Snapping", ""},
		{KNF_MODAL_CUT_THROUGH_TOGGLE, "CUT_THROUGH_TOGGLE", 0, "Toggle Cut Through", ""},
		{KNF_MODAL_NEW_CUT, "NEW_CUT", 0, "End Current Cut", ""},
		{KNF_MODAL_ADD_CUT, "ADD_CUT", 0, "Add Cut", ""},
		{KNF_MODAL_PANNING, "PANNING", 0, "Panning", ""},
		{0, NULL, 0, NULL, NULL}
	};

	wmKeyMap *keymap = WM_modalkeymap_get(keyconf, "Knife Tool Modal Map");

	/* this function is called for each spacetype, only needs to add map once */
	if (keymap && keymap->modal_items)
		return NULL;

	keymap = WM_modalkeymap_add(keyconf, "Knife Tool Modal Map", modal_items);

	/* items for modal map */
	WM_modalkeymap_add_item(keymap, ESCKEY, KM_PRESS, KM_ANY, 0, KNF_MODAL_CANCEL);
	WM_modalkeymap_add_item(keymap, MIDDLEMOUSE, KM_ANY, KM_ANY, 0, KNF_MODAL_PANNING);
	WM_modalkeymap_add_item(keymap, LEFTMOUSE, KM_PRESS, KM_ANY, 0, KNF_MODAL_ADD_CUT);
	WM_modalkeymap_add_item(keymap, RIGHTMOUSE, KM_PRESS, KM_ANY, 0, KNF_MODAL_CANCEL);
	WM_modalkeymap_add_item(keymap, RETKEY, KM_PRESS, KM_ANY, 0, KNF_MODAL_CONFIRM);
	WM_modalkeymap_add_item(keymap, PADENTER, KM_PRESS, KM_ANY, 0, KNF_MODAL_CONFIRM);
	WM_modalkeymap_add_item(keymap, SPACEKEY, KM_PRESS, KM_ANY, 0, KNF_MODAL_CONFIRM);
	WM_modalkeymap_add_item(keymap, EKEY, KM_PRESS, 0, 0, KNF_MODAL_NEW_CUT);

	WM_modalkeymap_add_item(keymap, LEFTCTRLKEY, KM_PRESS, KM_ANY, 0, KNF_MODAL_MIDPOINT_ON);
	WM_modalkeymap_add_item(keymap, LEFTCTRLKEY, KM_RELEASE, KM_ANY, 0, KNF_MODAL_MIDPOINT_OFF);
	WM_modalkeymap_add_item(keymap, RIGHTCTRLKEY, KM_PRESS, KM_ANY, 0, KNF_MODAL_MIDPOINT_ON);
	WM_modalkeymap_add_item(keymap, RIGHTCTRLKEY, KM_RELEASE, KM_ANY, 0, KNF_MODAL_MIDPOINT_OFF);

	WM_modalkeymap_add_item(keymap, LEFTSHIFTKEY, KM_PRESS, KM_ANY, 0, KNF_MODEL_IGNORE_SNAP_ON);
	WM_modalkeymap_add_item(keymap, LEFTSHIFTKEY, KM_RELEASE, KM_ANY, 0, KNF_MODEL_IGNORE_SNAP_OFF);
	WM_modalkeymap_add_item(keymap, RIGHTSHIFTKEY, KM_PRESS, KM_ANY, 0, KNF_MODEL_IGNORE_SNAP_ON);
	WM_modalkeymap_add_item(keymap, RIGHTSHIFTKEY, KM_RELEASE, KM_ANY, 0, KNF_MODEL_IGNORE_SNAP_OFF);

	WM_modalkeymap_add_item(keymap, CKEY, KM_PRESS, 0, 0, KNF_MODAL_ANGLE_SNAP_TOGGLE);
	WM_modalkeymap_add_item(keymap, ZKEY, KM_PRESS, 0, 0, KNF_MODAL_CUT_THROUGH_TOGGLE);

	WM_modalkeymap_assign(keymap, "MESH_OT_knife_tool");

	return keymap;
}

static int knifetool_modal(bContext *C, wmOperator *op, const wmEvent *event)
{
	Object *obedit = CTX_data_edit_object(C);
	KnifeTool_OpData *kcd = op->customdata;
	bool do_refresh = false;

	if (!obedit || obedit->type != OB_MESH || BKE_editmesh_from_object(obedit) != kcd->em) {
		knifetool_exit(C, op);
		ED_area_headerprint(CTX_wm_area(C), NULL);
		return OPERATOR_FINISHED;
	}

	view3d_operator_needs_opengl(C);
	ED_view3d_init_mats_rv3d(obedit, kcd->vc.rv3d);  /* needed to initialize clipping */

	if (kcd->mode == MODE_PANNING)
		kcd->mode = kcd->prevmode;

	/* handle modal keymap */
	if (event->type == EVT_MODAL_MAP) {
		switch (event->val) {
			case KNF_MODAL_CANCEL:
				/* finish */
				ED_region_tag_redraw(kcd->ar);

				knifetool_exit(C, op);
				ED_area_headerprint(CTX_wm_area(C), NULL);

				return OPERATOR_CANCELLED;
			case KNF_MODAL_CONFIRM:
				/* finish */
				ED_region_tag_redraw(kcd->ar);

				knifetool_finish(op);
				knifetool_exit(C, op);
				ED_area_headerprint(CTX_wm_area(C), NULL);

				return OPERATOR_FINISHED;
			case KNF_MODAL_MIDPOINT_ON:
				kcd->snap_midpoints = true;

				knife_recalc_projmat(kcd);
				knife_update_active(kcd);
				knife_update_header(C, kcd);
				ED_region_tag_redraw(kcd->ar);
				do_refresh = true;
				break;
			case KNF_MODAL_MIDPOINT_OFF:
				kcd->snap_midpoints = false;

				knife_recalc_projmat(kcd);
				knife_update_active(kcd);
				knife_update_header(C, kcd);
				ED_region_tag_redraw(kcd->ar);
				do_refresh = true;
				break;
			case KNF_MODEL_IGNORE_SNAP_ON:
				ED_region_tag_redraw(kcd->ar);
				kcd->ignore_vert_snapping = kcd->ignore_edge_snapping = true;
				knife_update_header(C, kcd);
				do_refresh = true;
				break;
			case KNF_MODEL_IGNORE_SNAP_OFF:
				ED_region_tag_redraw(kcd->ar);
				kcd->ignore_vert_snapping = kcd->ignore_edge_snapping = false;
				knife_update_header(C, kcd);
				do_refresh = true;
				break;
			case KNF_MODAL_ANGLE_SNAP_TOGGLE:
				kcd->angle_snapping = !kcd->angle_snapping;
				knife_update_header(C, kcd);
				do_refresh = true;
				break;
			case KNF_MODAL_CUT_THROUGH_TOGGLE:
				kcd->cut_through = !kcd->cut_through;
				knife_update_header(C, kcd);
				do_refresh = true;
				break;
			case KNF_MODAL_NEW_CUT:
				ED_region_tag_redraw(kcd->ar);
				knife_finish_cut(kcd);
				kcd->mode = MODE_IDLE;
				break;
			case KNF_MODAL_ADD_CUT:
				knife_recalc_projmat(kcd);

				if (kcd->mode == MODE_DRAGGING) {
					knife_add_cut(kcd);
				}
				else if (kcd->mode != MODE_PANNING) {
					knife_start_cut(kcd);
					kcd->mode = MODE_DRAGGING;
				}

				ED_region_tag_redraw(kcd->ar);
				break;
			case KNF_MODAL_PANNING:
				if (event->val != KM_RELEASE) {
					if (kcd->mode != MODE_PANNING) {
						kcd->prevmode = kcd->mode;
						kcd->mode = MODE_PANNING;
					}
				}
				else {
					kcd->mode = kcd->prevmode;
				}

				ED_region_tag_redraw(kcd->ar);
				return OPERATOR_PASS_THROUGH;
		}
	}
	else { /* non-modal-mapped events */
		switch (event->type) {
			case MOUSEPAN:
			case MOUSEZOOM:
			case MOUSEROTATE:
			case WHEELUPMOUSE:
			case WHEELDOWNMOUSE:
				return OPERATOR_PASS_THROUGH;
			case MOUSEMOVE: /* mouse moved somewhere to select another loop */
				if (kcd->mode != MODE_PANNING) {
					knifetool_update_mval_i(kcd, event->mval);
				}

				break;
		}
	}

	if (do_refresh) {
		/* we don't really need to update mval,
		 * but this happens to be the best way to refresh at the moment */
		knifetool_update_mval_i(kcd, event->mval);
	}

	/* keep going until the user confirms */
	return OPERATOR_RUNNING_MODAL;
}

void MESH_OT_knife_tool(wmOperatorType *ot)
{
	/* description */
	ot->name = "Knife Topology Tool";
	ot->idname = "MESH_OT_knife_tool";
	ot->description = "Cut new topology";

	/* callbacks */
	ot->invoke = knifetool_invoke;
	ot->modal = knifetool_modal;
	ot->cancel = knifetool_cancel;
	ot->poll = ED_operator_editmesh_view3d;

	/* flags */
	ot->flag = OPTYPE_REGISTER | OPTYPE_UNDO | OPTYPE_BLOCKING;

	RNA_def_boolean(ot->srna, "use_occlude_geometry", true, "Occlude Geometry", "Only cut the front most geometry");
	RNA_def_boolean(ot->srna, "only_selected", false, "Only Selected", "Only cut selected geometry");
}


/* -------------------------------------------------------------------- */
/* Knife tool as a utility function
 * that can be used for internal slicing operations */

/**
 * Return a point inside the face.
 *
 * tessellation here seems way overkill,
 * but without this its very hard to know of a point is inside the face
 */
static void edvm_mesh_knife_face_point(BMFace *f, float r_cent[3])
{
	const int tottri = f->len - 2;
	BMLoop **loops = BLI_array_alloca(loops, f->len);
	unsigned int  (*index)[3] = BLI_array_alloca(index, tottri);
	int j;

	const float *best_co[3] = {NULL};
	float best_area  = -1.0f;
	bool ok = false;

	BM_face_calc_tessellation(f, loops, index);

	for (j = 0; j < tottri; j++) {
		const float *p1 = loops[index[j][0]]->v->co;
		const float *p2 = loops[index[j][1]]->v->co;
		const float *p3 = loops[index[j][2]]->v->co;
		float area;

		float cross[3];
		cross_v3_v3v3(cross, p2, p3);
		area = fabsf(dot_v3v3(p1, cross));
		if (area > best_area) {
			best_co[0] = p1;
			best_co[1] = p2;
			best_co[2] = p3;
			best_area = area;
			ok = true;
		}
	}

	if (ok) {
		mid_v3_v3v3v3(r_cent, best_co[0], best_co[1], best_co[2]);
	}
	else {
		mid_v3_v3v3v3(r_cent, loops[0]->v->co, loops[1]->v->co, loops[2]->v->co);
	}
}

static bool edbm_mesh_knife_face_isect(ARegion *ar, LinkNode *polys, BMFace *f, float projmat[4][4])
{
	float cent_ss[2];
	float cent[3];

	edvm_mesh_knife_face_point(f, cent);

	ED_view3d_project_float_v2_m4(ar, cent, cent_ss, projmat);

	/* check */
	{
		LinkNode *p = polys;
		int isect = 0;

		while (p) {
			const float (*mval_fl)[2] = p->link;
			const int mval_tot = MEM_allocN_len(mval_fl) / sizeof(*mval_fl);
			isect += (int)isect_point_poly_v2(cent_ss, mval_fl, mval_tot - 1, false);
			p = p->next;
		}

		if (isect % 2) {
			return true;
		}
	}

	return false;
}

/**
 * \param use_tag  When set, tag all faces inside the polylines.
 */
void EDBM_mesh_knife(bContext *C, LinkNode *polys, bool use_tag, bool cut_through)
{
	KnifeTool_OpData *kcd;

	view3d_operator_needs_opengl(C);

	/* init */
	{
		const bool only_select = false;
		const bool is_interactive = false;  /* can enable for testing */

		kcd = MEM_callocN(sizeof(KnifeTool_OpData), __func__);

		knifetool_init(C, kcd, only_select, cut_through, is_interactive);

		kcd->ignore_edge_snapping = true;
		kcd->ignore_vert_snapping = true;

		if (use_tag) {
			BM_mesh_elem_hflag_enable_all(kcd->em->bm, BM_EDGE, BM_ELEM_TAG, false);
		}
	}

	/* execute */
	{
		LinkNode *p = polys;

		knife_recalc_projmat(kcd);

		while (p) {
			const float (*mval_fl)[2] = p->link;
			const int mval_tot = MEM_allocN_len(mval_fl) / sizeof(*mval_fl);
			int i;

			for (i = 0; i < mval_tot; i++) {
				knifetool_update_mval(kcd, mval_fl[i]);
				if (i == 0) {
					knife_start_cut(kcd);
					kcd->mode = MODE_DRAGGING;
				}
				else {
					knife_add_cut(kcd);
				}
			}
			knife_finish_cut(kcd);
			kcd->mode = MODE_IDLE;
			p = p->next;
		}
	}

	/* finish */
	{
		knifetool_finish_ex(kcd);

		/* tag faces inside! */
		if (use_tag) {
			BMesh *bm = kcd->em->bm;
			float projmat[4][4];

			BMEdge *e;
			BMIter iter;

			bool keep_search;

			ED_view3d_ob_project_mat_get(kcd->ar->regiondata, kcd->ob, projmat);

			/* use face-loop tag to store if we have intersected */
#define F_ISECT_IS_UNKNOWN(f)  BM_elem_flag_test(BM_FACE_FIRST_LOOP(f), BM_ELEM_TAG)
#define F_ISECT_SET_UNKNOWN(f) BM_elem_flag_enable(BM_FACE_FIRST_LOOP(f), BM_ELEM_TAG)
#define F_ISECT_SET_OUTSIDE(f) BM_elem_flag_disable(BM_FACE_FIRST_LOOP(f), BM_ELEM_TAG)
			{
				BMFace *f;
				BM_ITER_MESH (f, &iter, bm, BM_FACES_OF_MESH) {
					F_ISECT_SET_UNKNOWN(f);
					BM_elem_flag_disable(f, BM_ELEM_TAG);
				}
			}

			/* tag all faces linked to cut edges */
			BM_ITER_MESH (e, &iter, bm, BM_EDGES_OF_MESH) {
				/* check are we tagged?, then we are an original face */
				if (BM_elem_flag_test(e, BM_ELEM_TAG) == false) {
					BMFace *f;
					BMIter fiter;
					BM_ITER_ELEM (f, &fiter, e, BM_FACES_OF_EDGE) {
						if (edbm_mesh_knife_face_isect(kcd->ar, polys, f, projmat)) {
							BM_elem_flag_enable(f, BM_ELEM_TAG);
						}
					}
				}
			}

			/* expand tags for faces which are not cut, but are inside the polys */
			do {
				BMFace *f;
				keep_search = false;
				BM_ITER_MESH (f, &iter, bm, BM_FACES_OF_MESH) {
					if (BM_elem_flag_test(f, BM_ELEM_TAG) == false && (F_ISECT_IS_UNKNOWN(f))) {
						/* am I connected to a tagged face via an un-tagged edge (ie, not across a cut) */
						BMLoop *l_first = BM_FACE_FIRST_LOOP(f);
						BMLoop *l_iter = l_first;
						bool found = false;

						do {
							if (BM_elem_flag_test(l_iter->e, BM_ELEM_TAG) != false) {
								/* now check if the adjacent faces is tagged */
								BMLoop *l_radial_iter = l_iter->radial_next;
								if (l_radial_iter != l_iter) {
									do {
										if (BM_elem_flag_test(l_radial_iter->f, BM_ELEM_TAG)) {
											found = true;
										}
									} while ((l_radial_iter = l_radial_iter->radial_next) != l_iter && (found == false));
								}
							}
						} while ((l_iter = l_iter->next) != l_first && (found == false));

						if (found) {
							if (edbm_mesh_knife_face_isect(kcd->ar, polys, f, projmat)) {
								BM_elem_flag_enable(f, BM_ELEM_TAG);
								keep_search = true;
							}
							else {
								/* don't loose time on this face again, set it as outside */
								F_ISECT_SET_OUTSIDE(f);
							}
						}
					}
				}
			} while (keep_search);

#undef F_ISECT_IS_UNKNOWN
#undef F_ISECT_SET_UNKNOWN
#undef F_ISECT_SET_OUTSIDE

		}

		knifetool_exit_ex(C, kcd);
		kcd = NULL;
	}
}
