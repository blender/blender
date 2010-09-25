#if 1
/**
 * $Id$
 *
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
 * Inc., 59 Temple Place - Suite 330, Boston, MA  02111-1307, USA.
 *
 * The Original Code is Copyright (C) 2007 Blender Foundation.
 * All rights reserved.
 *
 * 
 * Contributor(s): Joseph Eagar, Joshua Leung
 *
 * ***** END GPL LICENSE BLOCK *****
 */

#include <float.h>
#define _USE_MATH_DEFINES
#include <math.h>
#include <string.h>
#include <ctype.h>
#include <stdio.h>

#include "DNA_ID.h"
#include "DNA_screen_types.h"
#include "DNA_scene_types.h"
#include "DNA_userdef_types.h"
#include "DNA_windowmanager_types.h"
#include "DNA_object_types.h"

#include "MEM_guardedalloc.h"

#include "PIL_time.h"

#include "BLI_blenlib.h"
#include "BLI_dynstr.h" /*for WM_operator_pystring */
#include "BLI_editVert.h"
#include "BLI_array.h"
#include "BLI_ghash.h"
#include "BLI_memarena.h"
#include "BLI_mempool.h"
#include "BLI_math.h"
#include "BLI_kdopbvh.h"
#include "BLI_smallhash.h"

#include "BKE_blender.h"
#include "BKE_context.h"
#include "BKE_depsgraph.h"
#include "BKE_scene.h"
#include "BKE_mesh.h"
#include "BKE_tessmesh.h"
#include "BKE_depsgraph.h"

#include "BIF_gl.h"
#include "BIF_glutil.h" /* for paint cursor */

#include "IMB_imbuf_types.h"

#include "ED_screen.h"
#include "ED_space_api.h"
#include "ED_view3d.h"
#include "ED_mesh.h"

#include "RNA_access.h"
#include "RNA_define.h"

#include "UI_interface.h"

#include "WM_api.h"
#include "WM_types.h"

#include "mesh_intern.h"
#include "editbmesh_bvh.h"

#define MAXGROUP	30

/* knifetool operator */
typedef struct KnifeVert {
	BMVert *v; /*non-NULL if this is an original vert*/
	ListBase edges;

	float co[3], sco[3]; /*sco is screen coordinates*/
	short flag, draw, isface;
} KnifeVert;

typedef struct Ref {
	struct Ref *next, *prev;
	void *ref;
} Ref;

typedef struct KnifeEdge {
	KnifeVert *v1, *v2;
	BMFace *basef; /*face to restrict face fill to*/
	ListBase faces;
	int draw;
	
	BMEdge *e, *oe; /*non-NULL if this is an original edge*/
} KnifeEdge;

#define KMAXDIST	12	/*max mouse distance from edge before not detecting it*/
#define MARK		4
#define DEL			8

typedef struct BMEdgeHit {
	BMEdge *e;
	float hit[3];
	float shit[3];
	float l; /*lambda along line*/
	BMVert *v; //set if snapped to a vert
} BMEdgeHit;

/* struct for properties used while drawing */
typedef struct knifetool_opdata {
	ARegion *ar;		/* region that knifetool was activated in */
	void *draw_handle;	/* for drawing preview loop */
	ViewContext vc;

	Object *ob;
	BMEditMesh *em;
	
	MemArena *arena;

	GHash *origvertmap;
	GHash *origedgemap;
	
	GHash *kedgefacemap;
	
	BMBVHTree *bmbvh;

	BLI_mempool *kverts;
	BLI_mempool *kedges;
	
	float vthresh;
	float ethresh;
	
	float vertco[3];
	float prevco[3];
	
	/*used for drag-cutting*/
	BMEdgeHit *linehits;
	int totlinehit;
	
	/*if curedge is NULL, attach to curvert;
	  if curvert is NULL, attach to curbmface,
	  otherwise create null vert*/
	KnifeEdge *curedge, *prevedge;
	KnifeVert *curvert, *prevvert;
	BMFace *curbmface, *prevbmface;

	int totkedge, totkvert, cutnr;
	
	BLI_mempool *refs;
	
	float projmat[4][4];
	
	enum {
		MODE_IDLE,
		MODE_DRAGGING,
		MODE_CONNECT,
	} mode;
} knifetool_opdata;

static ListBase *knife_get_face_kedges(knifetool_opdata *kcd, BMFace *f);

static KnifeEdge *new_knife_edge(knifetool_opdata *kcd)
{
	kcd->totkedge++;
	return BLI_mempool_calloc(kcd->kedges);
}

static KnifeVert *new_knife_vert(knifetool_opdata *kcd, float *co)
{
	KnifeVert *kfv = BLI_mempool_calloc(kcd->kverts);
	
	kcd->totkvert++;
	
	copy_v3_v3(kfv->co, co);
	copy_v3_v3(kfv->sco, co);

	view3d_project_float(kcd->ar, kfv->co, kfv->sco, kcd->projmat);

	return kfv;
}

/*get a KnifeVert wrapper for an existing BMVert*/
static KnifeVert *get_bm_knife_vert(knifetool_opdata *kcd, BMVert *v)
{
	KnifeVert *kfv = BLI_ghash_lookup(kcd->origvertmap, v);
	
	if (!kfv) {
		kfv = new_knife_vert(kcd, v->co);
		kfv->v = v;
		BLI_ghash_insert(kcd->origvertmap, v, kfv);
	}
	
	return kfv;
}

/*get a KnifeEdge wrapper for an existing BMEdge*/
static KnifeEdge *get_bm_knife_edge(knifetool_opdata *kcd, BMEdge *e)
{
	KnifeEdge *kfe = BLI_ghash_lookup(kcd->origedgemap, e);
	if (!kfe) {
		Ref *ref;
		BMIter iter;
		BMFace *f;
		
		kfe = new_knife_edge(kcd);
		kfe->e = e;
		kfe->v1 = get_bm_knife_vert(kcd, e->v1);
		kfe->v2 = get_bm_knife_vert(kcd, e->v2);
		
		ref = BLI_mempool_calloc(kcd->refs);
		ref->ref = kfe;
		BLI_addtail(&kfe->v1->edges, ref);

		ref = BLI_mempool_calloc(kcd->refs);
		ref->ref = kfe;
		BLI_addtail(&kfe->v2->edges, ref);
		
		BLI_ghash_insert(kcd->origedgemap, e, kfe);
		
		BM_ITER(f, &iter, kcd->em->bm, BM_FACES_OF_EDGE, e) {
			ref = BLI_mempool_calloc(kcd->refs);
			ref->ref = f;
			BLI_addtail(&kfe->faces, ref);
			
			/*ensures the kedges lst for this f is initialized,
			  it automatically adds kfe by itself*/
			knife_get_face_kedges(kcd, f);
		}
	}
	
	return kfe;
}

static void knife_start_cut(knifetool_opdata *kcd)
{
	kcd->prevedge = kcd->curedge;
	kcd->prevvert = kcd->curvert;
	kcd->prevbmface = kcd->curbmface;
	kcd->cutnr++;
	
	copy_v3_v3(kcd->prevco, kcd->vertco);
}

static Ref *find_ref(ListBase *lb, void *ref)
{
	Ref *ref1;
	
	for (ref1=lb->first; ref1; ref1=ref1->next) {
		if (ref1->ref == ref)
			return ref1;
	}
	
	return NULL;
}

static ListBase *knife_get_face_kedges(knifetool_opdata *kcd, BMFace *f)
{
	ListBase *lst = BLI_ghash_lookup(kcd->kedgefacemap, f);
	
	if (!lst) {
		BMIter iter;
		BMEdge *e;
		
		lst = BLI_memarena_alloc(kcd->arena, sizeof(ListBase));
		lst->first = lst->last = NULL;
		
		BM_ITER(e, &iter, kcd->em->bm, BM_EDGES_OF_FACE, f) {
			Ref *ref = BLI_mempool_calloc(kcd->refs);
			ref->ref = get_bm_knife_edge(kcd, e);
			BLI_addtail(lst, ref);
		}
		
		BLI_ghash_insert(kcd->kedgefacemap, f, lst);
	}
	
	return lst;
}

/*finds the proper face to restrict face fill to*/
void knife_find_basef(knifetool_opdata *kcd, KnifeEdge *kfe)
{
	if (!kfe->basef) {
		Ref *r1, *r2, *r3, *r4;
		
		if (kfe->v1->isface || kfe->v2->isface) {
			if (kfe->v2->isface)
				kfe->basef = kcd->curbmface;
			else 
				kfe->basef = kcd->prevbmface;
		} else {		
			for (r1=kfe->v1->edges.first; r1 && !kfe->basef; r1=r1->next) {
				KnifeEdge *ke1 = r1->ref;
				for (r2=ke1->faces.first; r2 && !kfe->basef; r2=r2->next) {
					for (r3=kfe->v2->edges.first; r3 && !kfe->basef; r3=r3->next) {
						KnifeEdge *ke2 = r3->ref;
					
						for (r4=ke2->faces.first; r4 && !kfe->basef; r4=r4->next) {
							if (r2->ref == r4->ref) {
								kfe->basef = r2->ref;
							}
						}	
					}
				}
			}
		}
		/*ok, at this point kfe->basef should be set if any valid possibility
		  exists*/
	}
}

static KnifeVert *knife_split_edge(knifetool_opdata *kcd, KnifeEdge *kfe, float co[3], KnifeEdge **newkfe_out)
{
	KnifeEdge *newkfe = new_knife_edge(kcd);
	ListBase *lst;
	Ref *ref;
	
	newkfe->v1 = kfe->v1;
	newkfe->v2 = new_knife_vert(kcd, co);
	newkfe->v2->draw = 1;
	newkfe->basef = kfe->basef;
	
	ref = find_ref(&kfe->v1->edges, kfe);
	BLI_remlink(&kfe->v1->edges, ref);
	
	kfe->v1 = newkfe->v2;
	BLI_addtail(&kfe->v1->edges, ref);
	
	for (ref=kfe->faces.first; ref; ref=ref->next) {
		Ref *ref2 = BLI_mempool_calloc(kcd->refs);
		
		/*add kedge ref to bm faces*/
		lst = knife_get_face_kedges(kcd, ref->ref);
		ref2->ref = newkfe;
		BLI_addtail(lst, ref2);

		ref2 = BLI_mempool_calloc(kcd->refs);
		ref2->ref = ref->ref;
		BLI_addtail(&newkfe->faces, ref2);
	}

	ref = BLI_mempool_calloc(kcd->refs);
	ref->ref = newkfe;
	BLI_addtail(&newkfe->v1->edges, ref);

	ref = BLI_mempool_calloc(kcd->refs);
	ref->ref = newkfe;
	BLI_addtail(&newkfe->v2->edges, ref);
	
	newkfe->draw = kfe->draw;
	newkfe->e = kfe->e;
	
	*newkfe_out = newkfe;
			
	return newkfe->v2;
}

static void knife_edge_append_face(knifetool_opdata *kcd, KnifeEdge *kfe, BMFace *f)
{
	ListBase *lst = knife_get_face_kedges(kcd, f);
	Ref *ref = BLI_mempool_calloc(kcd->refs);
	
	ref->ref = kfe;
	BLI_addtail(lst, ref);
	
	ref = BLI_mempool_calloc(kcd->refs);
	ref->ref = f;
	BLI_addtail(&kfe->faces, ref);
}

static void knife_copy_edge_facelist(knifetool_opdata *kcd, KnifeEdge *dest, KnifeEdge *source) 
{
	Ref *ref, *ref2;
	
	for (ref2 = source->faces.first; ref2; ref2=ref2->next) {
		ListBase *lst = knife_get_face_kedges(kcd, ref2->ref);
		
		/*add new edge to face knife edge list*/
		ref = BLI_mempool_calloc(kcd->refs);
		ref->ref = dest;
		BLI_addtail(lst, ref);
		
		/*add face to new edge's face list*/
		ref = BLI_mempool_calloc(kcd->refs);
		ref->ref = ref2->ref;
		BLI_addtail(&dest->faces, ref);
	}
}

static void knife_add_single_cut(knifetool_opdata *kcd)
{
	KnifeEdge *kfe = new_knife_edge(kcd), *kfe2 = NULL, *kfe3 = NULL;
	Ref *ref;
	
	if (kcd->prevvert && kcd->prevvert == kcd->curvert)
		return;
	if (kcd->prevedge && kcd->prevedge == kcd->curedge)
		return;
	
	if (kcd->prevvert) {
		kfe->v1 = kcd->prevvert;
	} else if (kcd->prevedge) {
		kfe->v1 = knife_split_edge(kcd, kcd->prevedge, kcd->prevco, &kfe2);
	} else {
		kfe->v1 = new_knife_vert(kcd, kcd->prevco);
		kfe->v1->draw = 1;
		kfe->v1->isface = 1;
	}
	
	if (kcd->curvert) {
		kfe->v2 = kcd->curvert;
	} else if (kcd->curedge) {
		kfe->v2 = knife_split_edge(kcd, kcd->curedge, kcd->vertco, &kfe3);
		
		kcd->curvert = kfe->v2;
	} else {
		kfe->v2 = new_knife_vert(kcd, kcd->vertco);
		kfe->v2->draw = 1;
		kfe->v2->isface = 1;

		kcd->curvert = kfe->v2;
	}
	
	knife_find_basef(kcd, kfe);
	
	kfe->draw = 1;
	
	ref = BLI_mempool_calloc(kcd->refs);
	ref->ref = kfe;	
	BLI_addtail(&kfe->v1->edges, ref);

	ref = BLI_mempool_calloc(kcd->refs);
	ref->ref = kfe;
	BLI_addtail(&kfe->v2->edges, ref);	
	
	if (kfe->basef && !find_ref(&kfe->faces, kfe->basef))
		knife_edge_append_face(kcd, kfe, kfe->basef);

	/*sanity check to make sure we're in the right edge/face lists*/
	if (kcd->curbmface) {
		if (!find_ref(&kfe->faces, kcd->curbmface)) {
			knife_edge_append_face(kcd, kfe, kcd->curbmface);
		}

		if (kcd->prevbmface && kcd->prevbmface != kcd->curbmface) {
			if (!find_ref(&kfe->faces, kcd->prevbmface)) {
				knife_edge_append_face(kcd, kfe, kcd->prevbmface);
			}
		}
	}
		
	/*set up for next cut*/
	kcd->prevbmface = kcd->curbmface;
	kcd->prevvert = kcd->curvert;
	kcd->prevedge = kcd->curedge;
	copy_v3_v3(kcd->prevco, kcd->vertco);
}

static int verge_linehit(const void *vlh1, const void *vlh2)
{
	const BMEdgeHit *lh1=vlh1, *lh2=vlh2;

	if (lh1->l < lh2->l) return -1;
	else if (lh1->l > lh2->l) return 1;
	else return 0;
}

static void knife_add_cut(knifetool_opdata *kcd)
{
	BMEditMesh *em = kcd->em;
	BMesh *bm = em->bm;
	knifetool_opdata oldkcd = *kcd;
	
	if (kcd->linehits) {
		BMEdgeHit *lh, *lastlh, *firstlh;
		int i;
		
		qsort(kcd->linehits, kcd->totlinehit, sizeof(BMEdgeHit), verge_linehit);
		
		lh = kcd->linehits;
		lastlh = firstlh = NULL;
		for (i=0; i<kcd->totlinehit; i++, (lastlh=lh), lh++) {
			if (lastlh && len_v3v3(lastlh->hit, lh->hit) == 0.0f) {
				if (!firstlh)
					firstlh = lastlh;
				continue;
			} else if (lastlh && firstlh) {
				if (firstlh->v || lastlh->v) {
					BMVert *bmv = firstlh->v ? firstlh->v : lastlh->v;
					
					kcd->prevvert = get_bm_knife_vert(kcd, bmv);
					copy_v3_v3(kcd->prevco, firstlh->hit);
					kcd->prevedge = NULL;
					kcd->prevbmface = firstlh->e->l ? firstlh->e->l->f : NULL;
				}
				lastlh = firstlh = NULL;
			}
			
			if (!lastlh && len_v3v3(kcd->prevco, lh->hit) < FLT_EPSILON*10)
				continue;
			
			kcd->curedge = get_bm_knife_edge(kcd, lh->e);
			kcd->curbmface = kcd->curedge->e->l ? kcd->curedge->e->l->f : NULL;
			kcd->curvert = lh->v ? get_bm_knife_vert(kcd, lh->v) : NULL;
			copy_v3_v3(kcd->vertco, lh->hit);

			knife_add_single_cut(kcd);
		}
		
		kcd->curbmface = oldkcd.curbmface;
		kcd->curvert = oldkcd.curvert;
		kcd->curedge = oldkcd.curedge;
		copy_v3_v3(kcd->vertco, oldkcd.vertco);
		
		knife_add_single_cut(kcd);
		
		MEM_freeN(kcd->linehits);
		kcd->linehits = NULL;
		kcd->totlinehit = 0;
	} else {
		knife_add_single_cut(kcd);
	}
}

static void knife_finish_cut(knifetool_opdata *kcd)
{

}

/* modal loop selection drawing callback */
static void knifetool_draw(const bContext *C, ARegion *ar, void *arg)
{
	knifetool_opdata *kcd = arg;
	
	glDisable(GL_DEPTH_TEST);

	glPushMatrix();
	glMultMatrixf(kcd->ob->obmat);
	
	if (kcd->mode == MODE_DRAGGING) {
		glColor3f(0.1, 0.1, 0.1);
		glLineWidth(2.0);
		
		glBegin(GL_LINES);
		glVertex3fv(kcd->prevco);	
		glVertex3fv(kcd->vertco);
		glEnd();
		
		glLineWidth(1.0);	
	}
	
	if (kcd->curedge) {
		glColor3f(0.5, 0.3, 0.15);
		glLineWidth(2.0);
		
		glBegin(GL_LINES);
		glVertex3fv(kcd->curedge->v1->co);	
		glVertex3fv(kcd->curedge->v2->co);
		glEnd();
		
		glLineWidth(1.0);
	} else if (kcd->curvert) {
		glColor3f(0.8, 0.2, 0.1);
		glPointSize(9);
		
		glBegin(GL_POINTS);
		glVertex3fv(kcd->vertco);
		glEnd();
	}
	
	if (kcd->curbmface) {		
		glColor3f(0.1, 0.8, 0.05);
		glPointSize(7);
		
		glBegin(GL_POINTS);
		glVertex3fv(kcd->vertco);
		glEnd();
	}
	
	if (kcd->totlinehit > 0) {
		BMEdgeHit *lh;
		int i;
		
		glEnable(GL_BLEND);
		glBlendFunc(GL_SRC_ALPHA, GL_ONE_MINUS_SRC_ALPHA);
		
		/*draw any snapped verts first*/
		glColor4f(0.8, 0.2, 0.1, 0.4);
		glPointSize(9);
		glBegin(GL_POINTS);
		lh = kcd->linehits;
		for (i=0; i<kcd->totlinehit; i++, lh++) {
			float sv1[3], sv2[3];

			view3d_project_float_v3(kcd->ar, lh->e->v1->co, sv1, kcd->projmat);
			view3d_project_float_v3(kcd->ar, lh->e->v2->co, sv2, kcd->projmat);
			
			if (len_v2v2(lh->shit, sv1) < kcd->vthresh/4) {
				copy_v3_v3(lh->hit, lh->e->v1->co);
				glVertex3fv(lh->hit);
				lh->v = lh->e->v1;
			} else if (len_v2v2(lh->shit, sv2) < kcd->vthresh/4) {
				copy_v3_v3(lh->hit, lh->e->v2->co);
				glVertex3fv(lh->hit);
				lh->v = lh->e->v2;
			}
		}
		glEnd();
		
		/*now draw the rest*/
		glColor4f(0.1, 0.8, 0.05, 0.4);
		glPointSize(5);
		glBegin(GL_POINTS);
		lh = kcd->linehits;
		for (i=0; i<kcd->totlinehit; i++, lh++) {
			glVertex3fv(lh->hit);
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
		for (kfe=BLI_mempool_iterstep(&iter); kfe; kfe=BLI_mempool_iterstep(&iter)) {
			if (!kfe->draw)
				continue;
				
			glColor3f(0.2, 0.2, 0.2);
			
			glVertex3fv(kfe->v1->co);
			glVertex3fv(kfe->v2->co);
		}
		
		glEnd();		
		glLineWidth(1.0);	
	}

	if (kcd->totkvert > 0) {
		BLI_mempool_iter iter;
		KnifeVert *kfv;
		
		glPointSize(4.0);
				
		glBegin(GL_POINTS);
		BLI_mempool_iternew(kcd->kverts, &iter);
		for (kfv=BLI_mempool_iterstep(&iter); kfv; kfv=BLI_mempool_iterstep(&iter)) {
			if (!kfv->draw)
				continue;
				
			glColor3f(0.6, 0.1, 0.2);
			
			glVertex3fv(kfv->co);
		}
		
		glEnd();		
	}

	glPopMatrix();
	glEnable(GL_DEPTH_TEST);
}

BMEdgeHit *knife_edge_tri_isect(knifetool_opdata *kcd, BMBVHTree *bmtree, float v1[3], 
                              float v2[3], float v3[3], bglMats *mats, int *count)
{
	BVHTree *tree2 = BLI_bvhtree_new(3, FLT_EPSILON*4, 8, 8), *tree = BMBVH_BVHTree(bmtree);
	BMEdgeHit *edges = NULL;
	BLI_array_declare(edges);
	SmallHash hash, *ehash = &hash;
	BVHTreeOverlap *results, *result;
	BMLoop *l, **ls;
	float cos[9], uv[3], lambda;
	int tot=0, i, j;
	
	BLI_smallhash_init(ehash);
	
	copy_v3_v3(cos, v1);
	copy_v3_v3(cos+3, v2);
	copy_v3_v3(cos+6, v3);
	
	BLI_bvhtree_insert(tree2, 0, cos, 3);
	BLI_bvhtree_balance(tree2);
	
	result = results = BLI_bvhtree_overlap(tree, tree2, &tot);
	
	for (i=0; i<tot; i++, result++) {
		float p[3];
		
		ls = (BMLoop**)kcd->em->looptris[result->indexA];
		
		for (j=0; j<3; j++) {
			if (isect_line_tri_v3(ls[j]->e->v1->co, ls[j]->e->v2->co, v1, v2, v3, &lambda, uv)) {
				float no[3], view[3], sp[3];
				
				sub_v3_v3v3(p, ls[j]->e->v2->co, ls[j]->e->v1->co);
				mul_v3_fl(p, lambda);
				add_v3_v3(p, ls[j]->e->v1->co);
				
				view3d_project_float_v3(kcd->ar, p, sp, kcd->projmat);
				view3d_unproject(mats, view, sp[0], sp[1], 0.0f);
				sub_v3_v3v3(view, p, view);
				normalize_v3(view);

				copy_v3_v3(no, view);
				mul_v3_fl(no, -0.00001);
				
				/*go backwards toward view a bit*/
				add_v3_v3(p, no);
				
				if (!BMBVH_RayCast(bmtree, p, no, NULL) && !BLI_smallhash_haskey(ehash, (intptr_t)ls[j]->e)) {
					BMEdgeHit hit;
					
					hit.e = ls[j]->e;
					hit.v = NULL;
					copy_v3_v3(hit.hit, p);
					view3d_project_float_v3(kcd->ar, hit.hit, hit.shit, kcd->projmat);
					
					BLI_array_append(edges, hit);
					BLI_smallhash_insert(ehash, (intptr_t)ls[j]->e, NULL);
				}
			}
		}
	}
	
	BLI_smallhash_release(ehash);
	
	if (results)
		MEM_freeN(results);
	
	*count = BLI_array_count(edges);
	return edges;
}

/*finds (visible) edges that intersects the current screen drag line*/
static void knife_find_line_hits(knifetool_opdata *kcd)
{
	bglMats mats;
	BMEdgeHit *e1, *e2;
	float v1[3], v2[3], v3[3], v4[4], s1[3], s2[3], view[3];
	int i, c1, c2;
	
	bgl_get_mats(&mats);
	
	if (kcd->linehits) {
		MEM_freeN(kcd->linehits);
		kcd->linehits = NULL;
		kcd->totlinehit = 0;
	}
	
	/*project screen line's 3d coordinates back into 2d*/
	view3d_project_float_v3(kcd->ar, kcd->prevco, s1, kcd->projmat);
	view3d_project_float_v3(kcd->ar, kcd->vertco, s2, kcd->projmat);
	
	if (len_v2v2(s1, s2) < 1)
		return;

	/*unproject screen line*/
	view3d_unproject(&mats, v1, s1[0], s1[1], 0.0f);
	view3d_unproject(&mats, v2, s2[0], s2[1], 0.0f);
	view3d_unproject(&mats, v3, s1[0], s1[1], 1.0f-FLT_EPSILON);
	view3d_unproject(&mats, v4, s2[0], s2[1], 1.0f-FLT_EPSILON);
	
	mul_m4_v3(kcd->ob->imat, v1);
	mul_m4_v3(kcd->ob->imat, v2);
	mul_m4_v3(kcd->ob->imat, v3);
	mul_m4_v3(kcd->ob->imat, v4);
	
	sub_v3_v3v3(view, v4, v1);
	normalize_v3(view);
	
	/*test two triangles of sceen line's plane*/
	e1 = knife_edge_tri_isect(kcd, kcd->bmbvh, v1, v2, v3, &mats, &c1);
	e2 = knife_edge_tri_isect(kcd, kcd->bmbvh, v1, v3, v4, &mats, &c2);
	if (c1 && c2) {
		e1 = MEM_reallocN(e1, sizeof(BMEdgeHit)*(c1+c2));
		memcpy(e1+c1, e2, sizeof(BMEdgeHit)*c2);
		MEM_freeN(e2);
	} else if (c2) {
		e1 = e2;
	}
	
	kcd->linehits = e1;
	kcd->totlinehit = c1+c2;

	/*find position along screen line, used for sorting*/
	for (i=0; i<kcd->totlinehit; i++) {
		BMEdgeHit *lh = e1+i;
		
		lh->l = len_v2v2(lh->shit, s1) / len_v2v2(s2, s1);
	}
}

static BMFace *knife_find_closest_face(knifetool_opdata *kcd, float co[3])
{
	BMFace *f;
	bglMats mats;
	float origin[3], ray[3];
	float mval[2];
	int dist = KMAXDIST; 
	
	bgl_get_mats(&mats);

	mval[0] = kcd->vc.mval[0]; mval[1] = kcd->vc.mval[1];
	
	/*unproject to find view ray*/
	view3d_unproject(&mats, origin, mval[0], mval[1], 0.0f);
	
	sub_v3_v3v3(ray, origin, kcd->vc.rv3d->viewinv[3]);
	normalize_v3(ray);
	
	/*transform into object space*/
	mul_m4_v3(kcd->ob->imat, origin);
	mul_m4_v3(kcd->ob->imat, ray);
	
	f = BMBVH_RayCast(kcd->bmbvh, origin, ray, co);
	if (!f) {
		/*try to use backbuffer selection method if ray casting failed*/
		f = EDBM_findnearestface(&kcd->vc, &dist);
		
		/*cheat for now; just put in the origin instead 
		  of a true coordinate on the face*/
		copy_v3_v3(co, origin);
	}
	
	//copy_v3_v3(co, origin);
	
	return f;
}

/*find the 2d screen space density of vertices within a radius.  used to scale snapping
  distance for picking edges/verts.*/
static int knife_sample_screen_density(knifetool_opdata *kcd, float radius)
{
	BMFace *f;
	float co[3], sco[3];
	
	f = knife_find_closest_face(kcd, co);
	
	if (f) {
		ListBase *lst;
		Ref *ref;
		KnifeVert *curv = NULL;
		float dis;
		int c = 0;
		
		view3d_project_float_v3(kcd->ar, co, sco, kcd->projmat);
		
		lst = knife_get_face_kedges(kcd, f);
		for (ref=lst->first; ref; ref=ref->next) {
			KnifeEdge *kfe = ref->ref;
			int i;
			
			for (i=0; i<2; i++) {
				KnifeVert *kfv = i ? kfe->v2 : kfe->v1;
				
				view3d_project_float_v3(kcd->ar, kfv->co, kfv->sco, kcd->projmat);
				
				dis = len_v2v2(kfv->sco, sco);
				if (dis < radius) {
					if(kcd->vc.rv3d->rflag & RV3D_CLIPPING) {
						float vec[3];
						
						copy_v3_v3(vec, kfv->co);
						mul_m4_v3(kcd->vc.obedit->obmat, vec);
			
						if(view3d_test_clipping(kcd->vc.rv3d, vec, 1)==0) {
							c++;
						}
					} else {
						c++;
					}
				}
			}
		}
		
		return c;
	}
		
	return 0;
}

/*returns snapping distance for edges/verts, scaled by the density of the
  surrounding mesh (in screen space)*/
static float knife_snap_size(knifetool_opdata *kcd, float maxsize)
{
	float density = (float)knife_sample_screen_density(kcd, maxsize*2.0f);
	
	density = MAX2(density, 1);
	
	return MIN2(maxsize / (density*0.5f), maxsize);
}

/*p is closest point on edge to the mouse cursor*/
static KnifeEdge *knife_find_closest_edge(knifetool_opdata *kcd, float p[3], BMFace **fptr)
{
	BMFace *f;
	float co[3], sco[3], maxdist = knife_snap_size(kcd, kcd->ethresh);
	
	f = knife_find_closest_face(kcd, co);
	/*set p to co, in case we don't find anything, means a face cut*/
	copy_v3_v3(p, co);

	if (f) {
		KnifeEdge *cure = NULL;
		ListBase *lst;
		Ref *ref;
		float dis, curdis=FLT_MAX;
		
		view3d_project_float_v3(kcd->ar, co, sco, kcd->projmat);
		
		/*look through all edges associated with this face*/
		lst = knife_get_face_kedges(kcd, f);
		for (ref=lst->first; ref; ref=ref->next) {
			KnifeEdge *kfe = ref->ref;
			
			/*project edge vertices into screen space*/
			view3d_project_float_v3(kcd->ar, kfe->v1->co, kfe->v1->sco, kcd->projmat);
			view3d_project_float_v3(kcd->ar, kfe->v2->co, kfe->v2->sco, kcd->projmat);

			dis = dist_to_line_segment_v2(sco, kfe->v1->sco, kfe->v2->sco);
			if (dis < curdis && dis < maxdist) {
				if(kcd->vc.rv3d->rflag & RV3D_CLIPPING) {
					float labda_PdistVL2Dfl(float *v1, float *v2, float *v3);
					float labda= labda_PdistVL2Dfl(sco, kfe->v1->sco, kfe->v2->sco);
					float vec[3];
		
					vec[0]= kfe->v1->co[0] + labda*(kfe->v2->co[0] - kfe->v1->co[0]);
					vec[1]= kfe->v1->co[1] + labda*(kfe->v2->co[1] - kfe->v1->co[1]);
					vec[2]= kfe->v1->co[2] + labda*(kfe->v2->co[2] - kfe->v1->co[2]);
					mul_m4_v3(kcd->vc.obedit->obmat, vec);
		
					if(view3d_test_clipping(kcd->vc.rv3d, vec, 1)==0) {
						cure = kfe;
						curdis = dis;
					}
				} else {
					cure = kfe;
					curdis = dis;				
				}
			}
		}
		
		if (fptr)
			*fptr = f;
		
		if (cure && p) {
			closest_to_line_segment_v3(p, co, cure->v1->co, cure->v2->co);
		}
		
		return cure;
	}
		
	if (fptr)
		*fptr = NULL;
	
	return NULL;
}

/*find a vertex near the mouse cursor, if it exists*/
static KnifeVert *knife_find_closest_vert(knifetool_opdata *kcd, float p[3], BMFace **fptr)
{
	BMFace *f;
	float co[3], sco[3], maxdist = knife_snap_size(kcd, kcd->vthresh);
	
	f = knife_find_closest_face(kcd, co);
	/*set p to co, in case we don't find anything, means a face cut*/
	copy_v3_v3(p, co);
	
	if (f) {
		ListBase *lst;
		Ref *ref;
		KnifeVert *curv = NULL;
		float dis, curdis=FLT_MAX;
		
		view3d_project_float_v3(kcd->ar, co, sco, kcd->projmat);
		
		lst = knife_get_face_kedges(kcd, f);
		for (ref=lst->first; ref; ref=ref->next) {
			KnifeEdge *kfe = ref->ref;
			int i;
			
			for (i=0; i<2; i++) {
				KnifeVert *kfv = i ? kfe->v2 : kfe->v1;
				
				view3d_project_float_v3(kcd->ar, kfv->co, kfv->sco, kcd->projmat);
				
				dis = len_v2v2(kfv->sco, sco);
				if (dis < curdis && dis < maxdist) {
					if(kcd->vc.rv3d->rflag & RV3D_CLIPPING) {
						float vec[3];
						
						copy_v3_v3(vec, kfv->co);
						mul_m4_v3(kcd->vc.obedit->obmat, vec);
			
						if(view3d_test_clipping(kcd->vc.rv3d, vec, 1)==0) {
							curv = kfv;
							curdis = dis;
						}
					} else {
						curv = kfv;
						curdis = dis;				
					}
				}
			}
		}
		
		if (fptr)
			*fptr = f;
		
		if (curv && p) {
			copy_v3_v3(p, curv->co);
		}
		
		return curv;
	}
		
	if (fptr)
		*fptr = NULL;
	
	return NULL;
}

/*update active knife edge/vert pointers*/
static int knife_update_active(knifetool_opdata *kcd)
{
	kcd->curvert = NULL; kcd->curedge = NULL; kcd->curbmface = NULL;
	
	kcd->curvert = knife_find_closest_vert(kcd, kcd->vertco, &kcd->curbmface);
	if (!kcd->curvert)
		kcd->curedge = knife_find_closest_edge(kcd, kcd->vertco, &kcd->curbmface);
	
	if (kcd->mode == MODE_DRAGGING) {
		knife_find_line_hits(kcd);
	}
	return 1;
}

#define VERT_ON_EDGE	8
#define VERT_ORIG		16
#define FACE_FLIP		32

/*use edgenet to fill faces.  this is a bit annoying and convoluted.*/
void knifenet_fill_faces(knifetool_opdata *kcd)
{
	BLI_mempool_iter iter;
	BMOperator bmop;
	BMesh *bm = kcd->em->bm;
	BMOIter siter;
	BMIter bmiter;
	BMFace *f;
	BMEdge *e;
	KnifeVert *kfv;
	KnifeEdge *kfe;
	float (*vertcos)[3] = NULL;
	void **blocks = NULL;
	BLI_array_declare(blocks);
	BLI_array_declare(vertcos);
	float *w = NULL;
	BLI_array_declare(w);
	
	BMO_push(bm, NULL);
	
	BM_ITER(f, &bmiter, bm, BM_FACES_OF_MESH, NULL) {
		BMINDEX_SET(f, 0);
	}

	/*assign a bit flag to each face.  adjacent
	  faces cannot share the same bit flag, nor can
	  diagonally adjacent faces*/
	BM_ITER(f, &bmiter, bm, BM_FACES_OF_MESH, NULL) {
		BMIter bmiter2;
		BMLoop *l;
		int group = 0, ok=0;
		
		while (!ok) {
			ok = 1;
			BM_ITER(l, &bmiter2, bm, BM_LOOPS_OF_FACE, f) {
				BMLoop *l2 = l->radial_next;
				while (l2 != l) {
					BMLoop *l3;
					BMIter bmiter3;
					
					if (l2->f == l->f) {
						l2 = l2->radial_next;
						continue;
					}
					
					if (BMINDEX_GET(l2->f) == (1<<group) && group <= MAXGROUP) {
						group++;
						ok = 0;
					} else if (BMINDEX_GET(l2->f) == (1<<group)) {
						printf("yeek! ran out of groups! 1\n");
					}

					BM_ITER(l3, &bmiter3, bm, BM_LOOPS_OF_FACE, l2->f) {
						BMLoop *l4 = l3->radial_next;
						
						do {
							if (l4->f == l->f) {
								l4 = l4->radial_next;
								continue;
							}
							
							if (BMINDEX_GET(l4->f) == (1<<group) && group <= MAXGROUP) {
								group++;
								ok = 0;
							} else if (BMINDEX_GET(l4->f) == (1<<group)) {
								printf("yeek! ran out of groups! 2`\n");
							}
							
							l4 = l4->radial_next;
						} while (l4 != l3);
					}
					
					l2 = l2->radial_next;
				}
			}
		}
		
		BMINDEX_SET(f, (1<<group));
	}

	/*turn knife verts into real verts, as necassary*/
	BLI_mempool_iternew(kcd->kverts, &iter);
	for (kfv=BLI_mempool_iterstep(&iter); kfv; kfv=BLI_mempool_iterstep(&iter)) {
		if (!kfv->v) {
			kfv->v = BM_Make_Vert(bm, kfv->co, NULL);
			kfv->flag = 1;
		} else {
			kfv->flag = 0;
			BMO_SetFlag(bm, kfv->v, VERT_ORIG);
		}

		BMO_SetFlag(bm, kfv->v, MARK);
	}

	BMO_InitOpf(bm, &bmop, "edgenet_fill use_restrict=%i", 1);

	/*turn knife edges into real edges, and assigns bit masks representing
	  the faces they are adjacent too*/
	BLI_mempool_iternew(kcd->kedges, &iter);
	for (kfe=BLI_mempool_iterstep(&iter); kfe; kfe=BLI_mempool_iterstep(&iter)) {
		int group = 0;

		if (!kfe->v1 || !kfe->v2)
			continue;
		
		if (kfe->e) {
			BM_ITER(f, &bmiter, bm, BM_FACES_OF_EDGE, kfe->e) {
				group |= BMINDEX_GET(f);

				if (kfe->v1->v != kfe->e->v1 || kfe->v2->v != kfe->e->v2) {
					BMO_SetFlag(bm, f, DEL);
				}
			}

			kfe->oe = kfe->e;

			if (kfe->v1->v != kfe->e->v1 || kfe->v2->v != kfe->e->v2) {
				BMO_SetFlag(bm, kfe->e, DEL);
				BMO_ClearFlag(bm, kfe->e, MARK);
				
				if (kfe->v1->v != kfe->e->v1)
					BMO_SetFlag(bm, kfe->v1->v, VERT_ON_EDGE);
				if (kfe->v2->v != kfe->e->v2)
					BMO_SetFlag(bm, kfe->v2->v, VERT_ON_EDGE);

				kfe->e = NULL;
			}
		}
		
		if (!kfe->e) {
			kfe->e = BM_Make_Edge(bm, kfe->v1->v, kfe->v2->v, NULL, 0);
		}
		
		BMO_SetFlag(bm, kfe->e, MARK);
		
		if (kfe->basef) {
			BMEdge *e;
			BM_ITER(e, &bmiter, bm, BM_EDGES_OF_FACE, kfe->basef) {
				BMO_SetFlag(bm, e, MARK);			
			}

			BMO_SetFlag(bm, kfe->basef, DEL);
			group |= BMINDEX_GET(kfe->basef);
		} 
		
		if (!group) {
			Ref *ref;
			
			for (ref=kfe->faces.first; ref; ref=ref->next) {
				kfe->basef = ref->ref;
				group |= BMINDEX_GET(ref->ref);	
			}
		}

		if (group)
			BMO_Insert_MapInt(bm, &bmop, "restrict", kfe->e, group);
		else
			printf("yeek!\n");
	}
	
	/*not sure why this is needed, sanity check to make sure del'd edges are not
	  marked as well*/
	BM_ITER(e, &bmiter, bm, BM_EDGES_OF_MESH, NULL) {
		if (BMO_TestFlag(bm, e, DEL))
			BMO_ClearFlag(bm, e, MARK);
	}

	BMO_Flag_To_Slot(bm, &bmop, "edges", MARK, BM_EDGE);
	BMO_Flag_To_Slot(bm, &bmop, "excludefaces", DEL, BM_FACE);
	
	/*execute the edgenet fill operator.  it will restrict filled faces to edges
	  belonging to the same group (note edges can belong to multiple groups, using
	  bitmasks)*/
	BMO_Exec_Op(bm, &bmop);
	BMO_Flag_Buffer(bm, &bmop, "faceout", MARK, BM_FACE);
	
	BM_Compute_Normals(bm);
	
	//void interp_weights_poly_v3(float *w,float v[][3], int n, float *co)
	
	/* interpolate customdata */
	
	/*first deal with interpolating vertices that lie on original edges*/	
	BLI_mempool_iternew(kcd->kedges, &iter);
	for (kfe=BLI_mempool_iterstep(&iter); kfe; kfe=BLI_mempool_iterstep(&iter)) {
		BMLoop *l1, *l2;
		float fac, w[2];
		void *blocks[2];
		
		if (!kfe->oe)
			continue;
		
		BM_ITER(l1, &bmiter, bm, BM_LOOPS_OF_EDGE, kfe->oe) {
			BMLoop *l2 = NULL;
			BMIter liter;
			int i, j;
			
			BM_ITER(l2, &liter, bm, BM_LOOPS_OF_EDGE, kfe->e) {
				if (!BM_Vert_In_Edge(kfe->e, l2->next->v)) {
					l2 = l2->prev;
				}
				
				if (!BMO_InMap(bm, &bmop, "faceout_groupmap", l2->f))
					continue;
				
				if (BMINDEX_GET(l1->f) == BMO_Get_MapInt(bm, &bmop, "faceout_groupmap", l2->f))
					break;	
			}
			
			if (!l2)
				continue;
			
			if (dot_v3v3(l1->f->no, l2->f->no) < 0.0f) {
				BMO_SetFlag(bm, l2->f, FACE_FLIP);
			}
			
			for (i=0; i<2; i++) {
				if (i)
					l2 = l2->next;
				
				fac = len_v3v3(kfe->oe->v1->co, l2->v->co) / (len_v3v3(kfe->oe->v1->co, kfe->oe->v2->co)+FLT_EPSILON);
				
				if (kfe->oe->v1 == l1->v) {
					w[0] = 1.0-fac;
					w[1] = fac;
				} else {
					w[0] = fac;
					w[1] = 1.0-fac;
				}
				
				if (l1->e == kfe->oe) {
					blocks[0] = l1->head.data;
					blocks[1] = l1->next->head.data;
				} else {
					blocks[0] = l1->prev->head.data;
					blocks[1] = l1->head.data;
				}
				
				BM_Copy_Attributes(bm, bm, l1->f, l2->f);
				CustomData_bmesh_interp(&bm->ldata, blocks, w, NULL, 2, l2->head.data);
			}
		}
	}
	
	/*ensure normals are correct*/
	BM_ITER(f, &bmiter, bm, BM_FACES_OF_MESH, NULL) {
		if (BMO_TestFlag(bm, f, FACE_FLIP)) {
			BM_flip_normal(bm, f);
		}
	}

	/*now deal with interior vertex interpolation
	  still kindof buggy*/
	BMO_ITER(f, &siter, bm, &bmop, "faceout_groupmap", BM_FACE) {
		BMLoop *l1, *l2;
		BMFace *f2; 
		BMIter liter1, liter2;
		int group = *(int*)BMO_IterMapVal(&siter);
		
		BM_ITER(l1, &liter1, bm, BM_LOOPS_OF_FACE, f) {
			float co[3], hit[3];
			float dir[3];
			int i;
			
			if (BMO_TestFlag(bm, l1->v, VERT_ORIG) || BMO_TestFlag(bm, l1->v, VERT_ON_EDGE))
				continue;
			
			copy_v3_v3(co, l1->v->co);
			copy_v3_v3(dir, l1->v->no);
			mul_v3_fl(dir, 0.001f);
			
			add_v3_v3(co, dir);
			copy_v3_v3(dir, l1->v->no);
			mul_v3_fl(dir, -1.0);
			
			f2 = BMBVH_RayCast(kcd->bmbvh, co, dir, hit);
			if (!f2) {
				printf("eek!!\n");
				continue;
			}
			
			BLI_array_empty(vertcos);
			BLI_array_empty(blocks);
			BLI_array_empty(w);
			BM_ITER(l2, &liter2, bm, BM_LOOPS_OF_FACE, f2) {
				BLI_array_growone(vertcos);
				BLI_array_append(blocks, l2->head.data);

				copy_v3_v3(vertcos[BLI_array_count(vertcos)-1], l2->v->co);
				BLI_array_append(w, 0.0f);
			}
			
			BM_Copy_Attributes(bm, bm, f2, f);

			interp_weights_poly_v3(w, vertcos, f2->len, l1->v->co);
			CustomData_bmesh_interp(&bm->ldata, blocks, w, NULL, BLI_array_count(blocks), l1->head.data);
		}
	}

	BMO_Finish_Op(bm, &bmop);

	/*delete left over faces*/
	BMO_CallOpf(bm, "del geom=%ff context=%i", DEL, DEL_ONLYFACES);
	BMO_CallOpf(bm, "del geom=%fe context=%i", DEL, DEL_EDGES);

	BMO_pop(bm);
}

/*called on tool confirmation*/
static void knifetool_finish(bContext *C, wmOperator *op)
{
	knifetool_opdata *kcd= op->customdata;
	
	knifenet_fill_faces(kcd);
}

void knife_recalc_projmat(knifetool_opdata *kcd)
{
	invert_m4_m4(kcd->ob->imat, kcd->ob->obmat);
	view3d_get_object_project_mat(kcd->ar->regiondata, kcd->ob, kcd->projmat);	
}

/* called when modal loop selection is done... */
static void knifetool_exit (bContext *C, wmOperator *op)
{
	knifetool_opdata *kcd= op->customdata;
	
	if (!kcd)
		return;
		
	/* deactivate the extra drawing stuff in 3D-View */
	ED_region_draw_cb_exit(kcd->ar->type, kcd->draw_handle);
	
	/* free the custom data */
	BLI_mempool_destroy(kcd->refs);
	BLI_mempool_destroy(kcd->kverts);
	BLI_mempool_destroy(kcd->kedges);

	BLI_ghash_free(kcd->origedgemap, NULL, NULL);
	BLI_ghash_free(kcd->origvertmap, NULL, NULL);
	BLI_ghash_free(kcd->kedgefacemap, NULL, NULL);
	
	BMBVH_FreeBVH(kcd->bmbvh);
	BLI_memarena_free(kcd->arena);
	
	/* tag for redraw */
	ED_region_tag_redraw(kcd->ar);

	/* destroy kcd itself */
	MEM_freeN(kcd);
	op->customdata= NULL;			
}

/* called when modal loop selection gets set up... */
static int knifetool_init (bContext *C, wmOperator *op, int do_cut)
{
	knifetool_opdata *kcd;
	
	/* alloc new customdata */
	kcd= op->customdata= MEM_callocN(sizeof(knifetool_opdata), "knifetool Modal Op Data");
	
	/* assign the drawing handle for drawing preview line... */
	kcd->ar= CTX_wm_region(C);
	kcd->draw_handle= ED_region_draw_cb_activate(kcd->ar->type, knifetool_draw, kcd, REGION_DRAW_POST_VIEW);
	em_setup_viewcontext(C, &kcd->vc);

	kcd->ob = CTX_data_edit_object(C);
	kcd->em= ((Mesh *)kcd->ob->data)->edit_btmesh;
	kcd->bmbvh = BMBVH_NewBVH(kcd->em);
	kcd->arena = BLI_memarena_new(1<<15, "knife");
	kcd->vthresh = KMAXDIST-1;
	kcd->ethresh = KMAXDIST;
	
	knife_recalc_projmat(kcd);
	
	ED_region_tag_redraw(kcd->ar);
	
	kcd->refs = BLI_mempool_create(sizeof(Ref), 1, 2048, 0, 0);
	kcd->kverts = BLI_mempool_create(sizeof(KnifeVert), 1, 512, 0, 1);
	kcd->kedges = BLI_mempool_create(sizeof(KnifeEdge), 1, 512, 0, 1);
	
	kcd->origedgemap = BLI_ghash_new(BLI_ghashutil_ptrhash, BLI_ghashutil_ptrcmp, "knife origedgemap");
	kcd->origvertmap = BLI_ghash_new(BLI_ghashutil_ptrhash, BLI_ghashutil_ptrcmp, "knife origvertmap");
	kcd->kedgefacemap = BLI_ghash_new(BLI_ghashutil_ptrhash, BLI_ghashutil_ptrcmp, "knife origvertmap");

	return 1;
}

static int knifetool_cancel (bContext *C, wmOperator *op)
{
	/* this is just a wrapper around exit() */
	knifetool_exit(C, op);
	return OPERATOR_CANCELLED;
}

static int knifetool_invoke (bContext *C, wmOperator *op, wmEvent *evt)
{
	knifetool_opdata *kcd;

	view3d_operator_needs_opengl(C);

	if (!knifetool_init(C, op, 0))
		return OPERATOR_CANCELLED;
	
	/* add a modal handler for this operator - handles loop selection */
	WM_event_add_modal_handler(C, op);

	kcd = op->customdata;
	kcd->vc.mval[0] = evt->mval[0];
	kcd->vc.mval[1] = evt->mval[1];
	
	return OPERATOR_RUNNING_MODAL;
}

static int knifetool_modal (bContext *C, wmOperator *op, wmEvent *event)
{
	knifetool_opdata *kcd= op->customdata;

	view3d_operator_needs_opengl(C);

	switch (event->type) {
		case ESCKEY:
		case RETKEY: /* confirm */ // XXX hardcoded
			if (event->val == KM_RELEASE) {
				/* finish */
				ED_region_tag_redraw(kcd->ar);
				
				knifetool_finish(C, op);
				knifetool_exit(C, op);
				
				return OPERATOR_FINISHED;
			}
			
			ED_region_tag_redraw(kcd->ar);
			return OPERATOR_RUNNING_MODAL;
		case LEFTMOUSE:
			knife_recalc_projmat(kcd);
			if (event->val != KM_RELEASE)
				break;
			
			if (kcd->mode == MODE_DRAGGING) {
				knife_add_cut(kcd);
				if (!event->ctrl) {
					knife_finish_cut(kcd);
					kcd->mode = MODE_IDLE;
				}
			} else {
				knife_start_cut(kcd);
				kcd->mode = MODE_DRAGGING;
			}

			ED_region_tag_redraw(kcd->ar);
			return OPERATOR_RUNNING_MODAL;
		case LEFTCTRLKEY:
		case RIGHTCTRLKEY:
			knife_recalc_projmat(kcd);
			
			if (event->val == KM_RELEASE) {
				knife_finish_cut(kcd);
				kcd->mode = MODE_IDLE;
			}
			
			ED_region_tag_redraw(kcd->ar);
			return OPERATOR_RUNNING_MODAL;
		case MOUSEMOVE: { /* mouse moved somewhere to select another loop */
			knife_recalc_projmat(kcd);
			kcd->vc.mval[0] = event->mval[0];
			kcd->vc.mval[1] = event->mval[1];
			
			if (knife_update_active(kcd))					
				ED_region_tag_redraw(kcd->ar);
			
			break;
		/*block undo*/
		case ZKEY:
			if (event->ctrl)
				return OPERATOR_RUNNING_MODAL;
		}			
	}
	
	/* keep going until the user confirms */
	return OPERATOR_PASS_THROUGH;
}

void MESH_OT_knifetool (wmOperatorType *ot)
{
	/* description */
	ot->name= "Knife Topology Tool";
	ot->idname= "MESH_OT_knifetool";
	ot->description= "Cut new topology";
	
	/* callbacks */
	ot->invoke= knifetool_invoke;
	ot->modal= knifetool_modal;
	ot->cancel= knifetool_cancel;
	ot->poll= ED_operator_editmesh_view3d;
	
	/* flags */
	ot->flag= OPTYPE_REGISTER|OPTYPE_UNDO|OPTYPE_BLOCKING;
}
#endif

/*
	if (0) {
		ListBase fgroups[30];
		BMEdge *e;
		
		memset(fgroups, 0, sizeof(fgroups));
		BM_ITER(f, &bmiter, kcd->em->bm, BM_FACES_OF_MESH, NULL) {
			Ref *ref = BLI_mempool_alloc(kcd->refs);
			int i;
			
			ref->ref = f;
			for (i=0; i<30; i++) {
				if ((1<<i) == BMINDEX_GET(f))
					break;
			}
			
			BLI_addtail(&fgroups[i], ref);
		}
		
		BM_ITER(e, &bmiter, kcd->em->bm, BM_EDGES_OF_MESH, NULL) {
			Ref *ref;
			int group = 0;
			
			if (!BMO_InMap(kcd->em->bm, &bmop, "restrict", e))
					continue;
			
			group = BMO_Get_MapInt(kcd->em->bm, &bmop, "restrict", e);
			
			for (i=0; i<30; i++) {
				if ((1<<i) & group) {
					float co[3];
					BMVert *v1, *v2;

					add_v3_v3v3(co, e->v1->co, e->v2->co);
					mul_v3_fl(co, 0.5f);
					
					v1 = BM_Make_Vert(kcd->em->bm, co, NULL);
					
					for (ref=fgroups[i].first; ref; ref=ref->next) {
						BMFace *f = ref->ref;
						BMLoop *l;
						BMIter liter;
						BMEdge *e2;
						
						zero_v3(co);
						BM_ITER(l, &liter, kcd->em->bm, BM_LOOPS_OF_FACE, f) {
							add_v3_v3(co, l->v->co);
						}
						mul_v3_fl(co, 1.0f/(float)f->len);
						
						v2 = BM_Make_Vert(kcd->em->bm, co, NULL);
						e2 = BM_Make_Edge(kcd->em->bm, v1, v2, NULL, 0);
					}
				}
			}
		}
	}
 
  */
