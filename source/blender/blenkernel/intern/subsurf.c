
/*  subsurf.c
 * 
 *  jun 2001
 *  
 * 
 * $Id$
 *
 * ***** BEGIN GPL/BL DUAL LICENSE BLOCK *****
 *
 * This program is free software; you can redistribute it and/or
 * modify it under the terms of the GNU General Public License
 * as published by the Free Software Foundation; either version 2
 * of the License, or (at your option) any later version. The Blender
 * Foundation also sells licenses for use in proprietary software under
 * the Blender License.  See http://www.blender.org/BL/ for information
 * about this.
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
 * The Original Code is Copyright (C) 2001-2002 by NaN Holding BV.
 * All rights reserved.
 *
 * The Original Code is: all of this file.
 *
 * Contributor(s): none yet.
 *
 * ***** END GPL/BL DUAL LICENSE BLOCK *****
 */

#ifdef HAVE_CONFIG_H
#include <config.h>
#endif

#ifdef WIN32
#include "BLI_winstuff.h"
#endif

#include <stdlib.h>
#include <stdio.h>
#include <math.h>
#include "MEM_guardedalloc.h"

#include "DNA_mesh_types.h"
#include "DNA_object_types.h"

#include "BKE_bad_level_calls.h"
#include "BKE_global.h"
#include "BKE_mesh.h"
#include "BKE_subsurf.h"
#include "BKE_displist.h"

#include "BLI_blenlib.h"
#include "BLI_editVert.h"
#include "BLI_arithb.h"
#include "BLI_linklist.h"
#include "BLI_memarena.h"

/*
 * TODO
 * 
 * make uvco's && vcol's properly subdivided
 *   - requires moving uvco and vcol data to vertices 
 *        (where it belongs?), or making sharedness explicit
 * remove/integrate zsl functions
 * clean up uvco && vcol stuff
 * add option to update subsurf only after done transverting
 * decouple display subdivlevel and render subdivlevel
 * look into waves/particles with subsurfs
 * groan... make it work with sticky? 
 * U check if storing tfaces (clut, tpage) in a displist is 
 *     going to be a mem problem (for example, on duplicate)
 * U write game blender convert routine
 * U thorough rendering check + background
 * 
 */

/****/

static float *Vec2Cpy(float *t, float *a) {
	t[0]= a[0];
	t[1]= a[1];
	return t;
}
static float *Vec3Cpy(float *t, float *a) {
	t[0]= a[0];
	t[1]= a[1];
	t[2]= a[2];
	return t;
}

static float *Vec2CpyI(float *t, float x, float y) {
	t[0]= x;
	t[1]= y;
	return t;
}
static float *Vec3CpyI(float *t, float x, float y, float z) {
	t[0]= x;
	t[1]= y;
	t[2]= z;
	return t;
}

static float *Vec2AvgT(float *t, float *a, float *b) {
	t[0]= (a[0]+b[0])*0.5f;
	t[1]= (a[1]+b[1])*0.5f;
	return t;
}
static float *Vec3AvgT(float *t, float *a, float *b) {
	t[0]= (a[0]+b[0])*0.5f;
	t[1]= (a[1]+b[1])*0.5f;
	t[2]= (a[2]+b[2])*0.5f;
	return t;
}

static float *Vec3AddT(float *t, float *a, float *b) {
	t[0]= a[0]+b[0];
	t[1]= a[1]+b[1];
	t[2]= a[2]+b[2];
	return t;
}
static float *Vec2Add(float *ta, float *b) {
	ta[0]+= b[0];
	ta[1]+= b[1];
	return ta;
}


static float *Vec3MulNT(float *t, float *a, float n) {
	t[0]= a[0]*n;
	t[1]= a[1]*n;
	t[2]= a[2]*n;
	return t;
}

static float *Vec3Add(float *ta, float *b) {
	ta[0]+= b[0];
	ta[1]+= b[1];
	ta[2]+= b[2];
	return ta;
}

static float *Vec2MulN(float *ta, float n) {
	ta[0]*= n;
	ta[1]*= n;
	return ta;
}
static float *Vec3MulN(float *ta, float n) {
	ta[0]*= n;
	ta[1]*= n;
	ta[2]*= n;
	return ta;
}

/****/

typedef struct _HyperVert HyperVert;
typedef struct _HyperEdge HyperEdge;
typedef struct _HyperFace HyperFace;
typedef struct _HyperMesh HyperMesh;

struct _HyperVert {
	HyperVert *next;
	
	float co[3];
	int flag;		// added for drawing optim
	float *orig;	// if set, pointer to original vertex, for handles
	HyperVert *nmv;
	LinkNode *edges, *faces;
};

struct _HyperEdge {
	HyperEdge *next;

	HyperVert *v[2];
	HyperVert *ep;
	int flag;		// added for drawing optim
	LinkNode *faces;
};

struct _HyperFace {
	HyperFace *next;

	int nverts;
	HyperVert **verts;	
	HyperEdge **edges;
	
	HyperVert *mid;

	unsigned char (*vcol)[4];
	float (*uvco)[2];
		
		/* for getting back tface, matnr, etc */
	union {
		int ind;
		EditVlak *ef;
	} orig;
};

struct _HyperMesh {
	HyperVert *verts;
	HyperEdge *edges;
	HyperFace *faces;
	Mesh *orig_me;
	short hasuvco, hasvcol;
	
	MemArena *arena;
};

/***/

static HyperEdge *hypervert_find_edge(HyperVert *v, HyperVert *to) {
	LinkNode *l;
	
	for (l= v->edges; l; l= l->next) {
		HyperEdge *e= l->link;
		
		if ((e->v[0]==v&&e->v[1]==to) || (e->v[1]==v&&e->v[0]==to))
			 return e;
	}

	return NULL;
}

static int hyperedge_is_boundary(HyperEdge *e) {
		/* len(e->faces) <= 1 */
	return (!e->faces || !e->faces->next);
}

static int hypervert_is_boundary(HyperVert *v) {
	LinkNode *l;
	
	for (l= v->edges; l; l= l->next)
		if (hyperedge_is_boundary(l->link))
			return 1;
			
	return 0;
}

static HyperVert *hyperedge_other_vert(HyperEdge *e, HyperVert *a) {
	return (a==e->v[0])?e->v[1]:e->v[0];
}

static HyperVert *hypermesh_add_vert(HyperMesh *hme, float *co, float *orig) {
	HyperVert *hv= BLI_memarena_alloc(hme->arena, sizeof(*hv));
	
	hv->nmv= NULL;
	hv->edges= NULL;
	hv->faces= NULL;
	
	Vec3Cpy(hv->co, co);
	hv->orig= orig;
	
	hv->next= hme->verts;
	hme->verts= hv;
	
	return hv;
}

static HyperEdge *hypermesh_add_edge(HyperMesh *hme, HyperVert *v1, HyperVert *v2, int flag) {
	HyperEdge *he= BLI_memarena_alloc(hme->arena, sizeof(*he));
	
	BLI_linklist_prepend_arena(&v1->edges, he, hme->arena);
	BLI_linklist_prepend_arena(&v2->edges, he, hme->arena);
	
	he->v[0]= v1;
	he->v[1]= v2;
	he->ep= NULL;
	he->faces= NULL;
	he->flag= flag;
	
	he->next= hme->edges;
	hme->edges= he;
	
	return he;
}

static HyperFace *hypermesh_add_face(HyperMesh *hme, HyperVert **verts, int nverts, int flag) {
	HyperFace *f= BLI_memarena_alloc(hme->arena, sizeof(*f));
	HyperVert *last;
	int j;

	f->mid= NULL;
	f->vcol= NULL;
	f->uvco= NULL;

	f->nverts= nverts;
	f->verts= BLI_memarena_alloc(hme->arena, sizeof(*f->verts)*f->nverts);
	f->edges= BLI_memarena_alloc(hme->arena, sizeof(*f->edges)*f->nverts);
	
	last= verts[nverts-1];
	for (j=0; j<nverts; j++) {
		HyperVert *v= verts[j];
		HyperEdge *e= hypervert_find_edge(v, last);

		if (!e)
			e= hypermesh_add_edge(hme, v, last, flag);

		f->verts[j]= v;
		f->edges[j]= e;
		
		BLI_linklist_prepend_arena(&v->faces, f, hme->arena);
		BLI_linklist_prepend_arena(&e->faces, f, hme->arena);
		
		last= v;
	}
	
	f->next= hme->faces;
	hme->faces= f;
	
	return f;	
}

static HyperMesh *hypermesh_new(void) {
	HyperMesh *hme= MEM_mallocN(sizeof(*hme), "hme");

	hme->verts= NULL;
	hme->edges= NULL;
	hme->faces= NULL;
	hme->orig_me= NULL;
	hme->hasuvco= hme->hasvcol= 0;
	hme->arena= BLI_memarena_new(BLI_MEMARENA_STD_BUFSIZE);
	
	return hme;
}

static HyperMesh *hypermesh_from_mesh(Mesh *me, DispList *dlverts) {
	HyperMesh *hme= hypermesh_new();
	HyperVert **vert_tbl;
	MFace *mface= me->mface;
	int i, j;
	
	hme->orig_me= me;
	if (me->tface)
		hme->hasvcol= hme->hasuvco= 1;
	else if (me->mcol)
		hme->hasvcol= 1;
		
	vert_tbl= MEM_mallocN(sizeof(*vert_tbl)*me->totvert, "vert_tbl");
	
	for (i= 0; i<me->totvert; i++) {
		if (dlverts)
			vert_tbl[i]= hypermesh_add_vert(hme, &dlverts->verts[i*3], NULL);
		else
			vert_tbl[i]= hypermesh_add_vert(hme, me->mvert[i].co, NULL);
	}

	for (i=0; i<me->totface; i++) {
		MFace *mf= &mface[i];
		
		if (mf->v3) {
			int nverts= mf->v4?4:3;
			HyperVert *verts[4];
			HyperFace *f;
		
			verts[0]= vert_tbl[mf->v1];
			verts[1]= vert_tbl[mf->v2];
			verts[2]= vert_tbl[mf->v3];
			if (nverts>3)
				verts[3]= vert_tbl[mf->v4];
		
			f= hypermesh_add_face(hme, verts, nverts, 1);
			f->orig.ind= i;

			if (hme->hasuvco) {
				TFace *tf= &((TFace*) me->tface)[i];
			
				f->uvco= BLI_memarena_alloc(hme->arena, sizeof(*f->uvco)*nverts);
				for (j=0; j<nverts; j++)
					Vec2Cpy(f->uvco[j], tf->uv[j]);

				f->vcol= BLI_memarena_alloc(hme->arena, sizeof(*f->vcol)*nverts);
				for (j=0; j<nverts; j++)
					*((unsigned int*) f->vcol[j])= tf->col[j];
			} else if (hme->hasvcol) {
				MCol *mcol= &me->mcol[i*4];
			
				f->vcol= BLI_memarena_alloc(hme->arena, sizeof(*f->vcol)*nverts);
				for (j=0; j<nverts; j++)
					*((unsigned int*) f->vcol[j])= *((unsigned int*) &mcol[j]);
			}
		} else {
			hypermesh_add_edge(hme, vert_tbl[mf->v1], vert_tbl[mf->v2], 1);
		}
	}

	MEM_freeN(vert_tbl);
	
	return hme;
}

static HyperMesh *hypermesh_from_editmesh(EditVert *everts, EditEdge *eedges, EditVlak *efaces) {
	HyperMesh *hme= hypermesh_new();
	EditVert *ev, *prevev;
	EditEdge *ee;
	EditVlak *ef;

		/* we only add vertices with edges, 'f1' is a free flag */
	for (ev= everts; ev; ev= ev->next) ev->f1= 1;	

		/* hack, tuck the new hypervert pointer into
		 * the ev->prev link so we can find it easy, 
		 * then restore real prev links later.
		 */
	for (ee= eedges; ee; ee= ee->next) {
		if(ee->v1->f1) {
			ee->v1->prev= (EditVert*) hypermesh_add_vert(hme, ee->v1->co, ee->v1->co);
			ee->v1->f1= 0;
		}
		if(ee->v2->f1) {
			ee->v2->prev= (EditVert*) hypermesh_add_vert(hme, ee->v2->co, ee->v2->co);
			ee->v2->f1= 0;
		}
			
		hypermesh_add_edge(hme, (HyperVert*) ee->v1->prev, (HyperVert*) ee->v2->prev, 1);
	}
	for (ef= efaces; ef; ef= ef->next) {
		int nverts= ef->v4?4:3;
		HyperVert *verts[4];
		HyperFace *f;
		
		verts[0]= (HyperVert*) ef->v1->prev;
		verts[1]= (HyperVert*) ef->v2->prev;
		verts[2]= (HyperVert*) ef->v3->prev;
		if (nverts>3)
			verts[3]= (HyperVert*) ef->v4->prev;

		f= hypermesh_add_face(hme, verts, nverts, 1);
		f->orig.ef= ef;
	}

		/* see hack above, restore the prev links */
	for (prevev= NULL, ev= everts; ev; prevev= ev, ev= ev->next)
		ev->prev= prevev;
	
	return hme;
}

static void VColAvgT(unsigned char *t, unsigned char *a, unsigned char *b) {
	t[0]= (a[0]+b[0])>>1;
	t[1]= (a[1]+b[1])>>1;
	t[2]= (a[2]+b[2])>>1;
	t[3]= (a[3]+b[3])>>1;
}

static void hypermesh_subdivide(HyperMesh *me, HyperMesh *nme) {
	HyperVert *v;
	HyperEdge *e;
	HyperFace *f;
	LinkNode *link;
	float co[3];
	int j, k, count;

	for (f= me->faces; f; f= f->next) {
		Vec3CpyI(co, 0.0, 0.0, 0.0);
		for (j=0; j<f->nverts; j++)
			Vec3Add(co, f->verts[j]->co);
		Vec3MulN(co, (float)(1.0/f->nverts));

		f->mid= hypermesh_add_vert(nme, co, NULL);
	}
		
	for (e= me->edges; e; e= e->next) {
		if (hyperedge_is_boundary(e)) {
			Vec3AvgT(co, e->v[0]->co, e->v[1]->co);
		} else {
			Vec3AddT(co, e->v[0]->co, e->v[1]->co);
			for (count=2, link= e->faces; link; count++, link= link->next) {
				f= (HyperFace *) link->link;
				Vec3Add(co, f->mid->co);
			}
			Vec3MulN(co, (float)(1.0/count));
		}
		
		e->ep= hypermesh_add_vert(nme, co, NULL);
	}

	for (v= me->verts; v; v= v->next) {
		float q[3], r[3], s[3];

		if (hypervert_is_boundary(v)) {
			Vec3CpyI(r, 0.0, 0.0, 0.0);

			for (count= 0, link= v->edges; link; link= link->next) {
				if (hyperedge_is_boundary(link->link)) {
					HyperVert *ov= hyperedge_other_vert(link->link, v);

					Vec3Add(r, ov->co);
					count++;
				}
			}

				/* I believe CC give the factors as
					3/2k and 1/4k, but that doesn't make
					sense (to me) as they don't sum to unity... 
					It's rarely important.
				*/
			Vec3MulNT(s, v->co, 0.75f);
			Vec3Add(s, Vec3MulN(r, (float)(1.0/(4.0*count))));
		} else {
			Vec3Cpy(q, Vec3Cpy(r, Vec3CpyI(s, 0.0f, 0.0f, 0.0f)));
		
			for (count=0, link= v->faces; link; count++, link= link->next) {
				f= (HyperFace *) link->link;
				Vec3Add(q, f->mid->co);
			}
			Vec3MulN(q, (float)(1.0/count));

			for (count=0, link= v->edges; link; count++, link= link->next) {
				e= (HyperEdge *) link->link;
				Vec3Add(r, hyperedge_other_vert(e, v)->co);
			}
			Vec3MulN(r, (float)(1.0/count));
		
			Vec3MulNT(s, v->co, (float)(count-2));

			Vec3Add(s, q);
			Vec3Add(s, r);
			Vec3MulN(s, (float)(1.0/count));
		}

		v->nmv= hypermesh_add_vert(nme, s, v->orig);
	}

	for (e= me->edges; e; e= e->next) {
		hypermesh_add_edge(nme, e->v[0]->nmv, e->ep, e->flag);
		hypermesh_add_edge(nme, e->v[1]->nmv, e->ep, e->flag);
	}

	for (f= me->faces; f; f= f->next) {
		int last= f->nverts-1;
		unsigned char vcol_mid[4];
		unsigned char vcol_edge[4][4];
		float uvco_mid[2];
		float uvco_edge[4][4];
		
		if (me->hasvcol) {
			int t[4]= {0, 0, 0, 0};
			for (j=0; j<f->nverts; j++) {
				t[0]+= f->vcol[j][0];
				t[1]+= f->vcol[j][1];
				t[2]+= f->vcol[j][2];
				t[3]+= f->vcol[j][3];
			}
			vcol_mid[0]= t[0]/f->nverts;
			vcol_mid[1]= t[1]/f->nverts;
			vcol_mid[2]= t[2]/f->nverts;
			vcol_mid[3]= t[3]/f->nverts;
			
			for (j=0; j<f->nverts; last= j, j++)
				VColAvgT(vcol_edge[j], f->vcol[last], f->vcol[j]);
			last= f->nverts-1;
		}
		if (me->hasuvco) {
			Vec2CpyI(uvco_mid, 0.0, 0.0);
			for (j=0; j<f->nverts; j++)
				Vec2Add(uvco_mid, f->uvco[j]);
			Vec2MulN(uvco_mid, (float)(1.0/f->nverts));

			for (j=0; j<f->nverts; last= j, j++)
				Vec2AvgT(uvco_edge[j], f->uvco[last], f->uvco[j]);
			last= f->nverts-1;
		}
		
		for (j=0; j<f->nverts; last=j, j++) {
			HyperVert *nv[4];
			HyperFace *nf;
			
			nv[0]= f->verts[last]->nmv;
			nv[1]= f->edges[j]->ep;
			nv[2]= f->mid;
			nv[3]= f->edges[last]->ep;
			
			nf= hypermesh_add_face(nme, nv, 4, 0);
			nf->orig= f->orig;
			
			if (me->hasvcol) {
				nf->vcol= BLI_memarena_alloc(nme->arena, sizeof(*nf->vcol)*4);
				
				for (k=0; k<4; k++) {
					nf->vcol[0][k]= f->vcol[last][k];
					nf->vcol[1][k]= vcol_edge[j][k];
					nf->vcol[2][k]= vcol_mid[k];
					nf->vcol[3][k]= vcol_edge[last][k];
				}
			}
			if (me->hasuvco) {
				nf->uvco= BLI_memarena_alloc(nme->arena, sizeof(*nf->uvco)*4);
				
				Vec2Cpy(nf->uvco[0], f->uvco[last]);
				Vec2Cpy(nf->uvco[1], uvco_edge[j]);
				Vec2Cpy(nf->uvco[2], uvco_mid);
				Vec2Cpy(nf->uvco[3], uvco_edge[last]);
			}
		}
	}
}

static void hypermesh_free(HyperMesh *me) {
	BLI_memarena_free(me->arena);
			
	MEM_freeN(me);
}

/*****/

static void add_mvert_normals_from_mfaces(MVert *mverts, int nmverts, MFaceInt *mfaces, int nmfaces) {
	float (*tnorms)[3]= MEM_callocN(nmverts*sizeof(*tnorms), "tnorms");
	int i;
	
	for (i=0; i<nmfaces; i++) {
		MFaceInt *mf= &mfaces[i];
		float f_no[3];

		if (!mf->v3)
			continue;
			
		if (mf->v4)
			CalcNormFloat4(mverts[mf->v1].co, mverts[mf->v2].co, mverts[mf->v3].co, mverts[mf->v4].co, f_no);
		else
			CalcNormFloat(mverts[mf->v1].co, mverts[mf->v2].co, mverts[mf->v3].co, f_no);
		
		Vec3Add(tnorms[mf->v1], f_no);
		Vec3Add(tnorms[mf->v2], f_no);
		Vec3Add(tnorms[mf->v3], f_no);
		if (mf->v4)
			Vec3Add(tnorms[mf->v4], f_no);
	}
	for (i=0; i<nmverts; i++) {
		MVert *mv= &mverts[i];
		float *no= tnorms[i];
		
		Normalise(no);
		mv->no[0]= (short)(no[0]*32767.0);
		mv->no[1]= (short)(no[1]*32767.0);
		mv->no[2]= (short)(no[2]*32767.0);
	}
	
	MEM_freeN(tnorms);
}

static int hypermesh_get_nverts(HyperMesh *hme) {
	HyperVert *v;
	int count= 0;
	
	for (v= hme->verts; v; v= v->next)
		count++;
	
	return count;
}

static int hypermesh_get_nverts_handles(HyperMesh *hme) {
	HyperVert *v;
	int count= 0;
	
	for (v= hme->verts; v; v= v->next)
		if(v->orig) count++;
	
	return count;
}

static int hypermesh_get_nfaces(HyperMesh *hme) {
	HyperFace *f;
	int count= 0;
	
	for (f= hme->faces; f; f= f->next)
		count++;
	
	return count;
}

static int hypermesh_get_nlines(HyperMesh *hme) {
	HyperEdge *e;
	int n= 0;
	
	for (e= hme->edges; e; e= e->next)
		if (!e->faces)
			n++;
	
	return n;
}

static int editface_is_hidden(EditVlak *ef) {
	return (ef->v1->h || ef->v2->h || ef->v3->h || (ef->v4 && ef->v4->h));
}

static int hypermesh_get_nhidden(HyperMesh *hme) {
	int count= 0;

		/* hme->orig_me==NULL if we are working on an editmesh */
	if (!hme->orig_me) {
		HyperFace *f;
	
		for (f= hme->faces; f; f= f->next)
			if (editface_is_hidden(f->orig.ef))
				count++;
	}
	
	return count;
}

/* flag is me->flag, for handles and 'optim' */
static DispList *hypermesh_to_displist(HyperMesh *hme, short flag) {
	int nverts= hypermesh_get_nverts(hme);
	int nfaces= hypermesh_get_nfaces(hme) + hypermesh_get_nlines(hme) - hypermesh_get_nhidden(hme);
	DispList *dl= MEM_callocN(sizeof(*dl), "dl");
	DispListMesh *dlm= MEM_callocN(sizeof(*dlm), "dlmesh");
	HyperFace *f;
	HyperVert *v;
	HyperEdge *e;
	TFace *tfaces;
	MFace *mfaces;
	MFaceInt *mf;
	int i, j, handles=0;

		/* hme->orig_me==NULL if we are working on an editmesh */
	if (hme->orig_me) {
		tfaces= hme->orig_me->tface;
		mfaces= hme->orig_me->mface;
	} else {
		tfaces= NULL;
		mfaces= NULL;
	}

	/* added: handles for editmode */
	if (hme->orig_me==NULL && (flag & ME_OPT_EDGES)) {
		handles= hypermesh_get_nverts_handles(hme);
	}

	dl->type= DL_MESH;
	dl->mesh= dlm;
	
	dlm->totvert= nverts+handles;
	dlm->totface= nfaces+handles;
	dlm->mvert= MEM_mallocN(dlm->totvert*sizeof(*dlm->mvert), "dlm->mvert");
	dlm->mface= MEM_mallocN(dlm->totface*sizeof(*dlm->mface), "dlm->mface");
	if(hme->orig_me) dlm->flag= hme->orig_me->flag;
	
	if (hme->hasuvco)
		dlm->tface= MEM_callocN(dlm->totface*sizeof(*dlm->tface), "dlm->tface");
	else if (hme->hasvcol)
		dlm->mcol= MEM_mallocN(dlm->totface*4*sizeof(*dlm->mcol), "dlm->mcol");
				
	for (i=0, v= hme->verts; i<nverts; i++, v= v->next) {
		MVert *mv= &dlm->mvert[i];
		Vec3Cpy(mv->co, v->co);
		v->nmv= (void*) i;
	}
	
	mf= dlm->mface;
	for (i=0, f= hme->faces; f; i++, f= f->next) {
		int voff= (((int) f->verts[3]->nmv)==0)?1:0;
		
		if (!hme->orig_me && editface_is_hidden(f->orig.ef))
			continue;
			
			/* compensate for blender's [braindead] way of encoding
			 * nverts by face vertices, if necessary.
			 */
		
		mf->v1= (int) f->verts[(0+voff)]->nmv;
		mf->v2= (int) f->verts[(1+voff)]->nmv;
		mf->v3= (int) f->verts[(2+voff)]->nmv;
		mf->v4= (int) f->verts[(3+voff)%4]->nmv;

		if (hme->orig_me) {			
			MFace *origmf= &mfaces[f->orig.ind];

			mf->mat_nr= origmf->mat_nr;
			mf->flag= origmf->flag;
			mf->puno= 0;
			
			
		} else {
			EditVlak *origef= f->orig.ef;
			
			mf->mat_nr= origef->mat_nr;
			mf->flag= origef->flag;
			mf->puno= 0;
		}
		
		{ 	// draw flag			
			mf->edcode= 0;
			
			f->verts[0]->flag= 0;
			f->verts[1]->flag= 0;
			f->verts[2]->flag= 0;
			f->verts[3]->flag= 0;
			
			if(f->edges[0]->flag) {
				f->edges[0]->flag= 0;
				f->edges[0]->v[0]->flag++;
				f->edges[0]->v[1]->flag++;
			}
			if(f->edges[1]->flag) {
				f->edges[1]->flag= 0;
				f->edges[1]->v[0]->flag++;
				f->edges[1]->v[1]->flag++;
			}
			if(f->edges[2]->flag) {
				f->edges[2]->flag= 0;
				f->edges[2]->v[0]->flag++;
				f->edges[2]->v[1]->flag++;
			}
			if(f->edges[3]->flag) {
				f->edges[3]->flag= 0;
				f->edges[3]->v[0]->flag++;
				f->edges[3]->v[1]->flag++;
			}
			if( f->verts[0+voff]->flag && f->verts[1+voff]->flag ) mf->edcode|=ME_V1V2;
			if( f->verts[1+voff]->flag && f->verts[2+voff]->flag ) mf->edcode|=ME_V2V3;
			if( f->verts[2+voff]->flag && f->verts[(3+voff)%4]->flag ) mf->edcode|=ME_V3V4;
			if( f->verts[(3+voff)%4]->flag && f->verts[0+voff]->flag ) mf->edcode|=ME_V4V1;
		}
			
		if (hme->hasuvco) {
			TFace *origtf, *tf= &dlm->tface[i];
			
			if (hme->orig_me)
				origtf= &tfaces[f->orig.ind];
			else
				origtf= f->orig.ef->tface;
			
			for (j=0; j<4; j++) {
				Vec2Cpy(tf->uv[j], f->uvco[(j+voff)%4]);
				tf->col[j]= *((unsigned int*) f->vcol[(j+voff)%4]);
			}
			
			tf->tpage= origtf->tpage;
			tf->flag= origtf->flag;
			tf->transp= origtf->transp;
			tf->mode= origtf->mode;
			tf->tile= origtf->tile;
		} else if (hme->hasvcol) {
			MCol *mcolbase= &dlm->mcol[i*4];
			
			for (j=0; j<4; j++)
				*((unsigned int*) &mcolbase[j])= *((unsigned int*) f->vcol[(j+voff)%4]);
		}
		
		mf++;
	}

	for (e= hme->edges; e; e= e->next) {
		if (!e->faces) {
			mf->v1= (int) e->v[0]->nmv;
			mf->v2= (int) e->v[1]->nmv;
			mf->v3= 0;
			mf->v4= 0;
			
			mf->mat_nr= 0;
			mf->flag= 0;
			mf->puno= 0;
			mf->edcode= ME_V1V2;
				
			mf++;
		}
	}
	
	/* and we add the handles */
	if(handles) {
		MVert *mv= dlm->mvert+nverts;
		mf= dlm->mface+nfaces;
		i= nverts;
		for (v= hme->verts; v; v= v->next) {
			if(v->orig) {
				/* new vertex */
				Vec3Cpy(mv->co, v->orig);

				/* new face */
				mf->v1= (int) v->nmv;
				mf->v2= i;
				mf->v3= 0;
				mf->v4= 0;
				
				mf->mat_nr= 0;
				mf->flag= 0;
				mf->puno= 0;
				mf->edcode= ME_V1V2;
					
				mf++; i++; mv++;
			}
		}
	}	
	
	add_mvert_normals_from_mfaces(dlm->mvert, dlm->totvert, dlm->mface, dlm->totface);

	return dl;
}

/* flag is me->flag, for handles and 'optim' */
static DispList *subsurf_subdivide_to_displist(HyperMesh *hme, short subdiv, short flag) {
	DispList *dl;
	int i;

	for (i= 0; i<subdiv; i++) {
		HyperMesh *tmp= hypermesh_new();
		tmp->hasvcol= hme->hasvcol;
		tmp->hasuvco= hme->hasuvco;
		tmp->orig_me= hme->orig_me;
		
		hypermesh_subdivide(hme, tmp);
		hypermesh_free(hme);
		
		hme= tmp;
	}

	dl= hypermesh_to_displist(hme, flag);
	hypermesh_free(hme);
	
	return dl;
}

void subsurf_make_editmesh(Object *ob) {
	DispList *dl;
	
	if (G.eded.first) {
		Mesh *me= ob->data;
		HyperMesh *hme= hypermesh_from_editmesh(G.edve.first, G.eded.first, G.edvl.first);

		free_displist_by_type(&me->disp, DL_MESH);
		BLI_addtail(&me->disp, subsurf_subdivide_to_displist(hme, me->subdiv, me->flag));
		
		dl= me->disp.first;
		if(dl && dl->mesh) dl->mesh->flag= me->flag;
	}
	
}

void subsurf_make_mesh(Object *ob, short subdiv) {
	Mesh *me= ob->data;

	if (me->totface) {
		HyperMesh *hme= hypermesh_from_mesh(me, find_displist(&ob->disp, DL_VERTS));

		free_displist_by_type(&me->disp, DL_MESH);
		BLI_addtail(&me->disp, subsurf_subdivide_to_displist(hme, subdiv, me->flag));
	}
}

void subsurf_to_mesh(Object *oldob, Mesh *me) {
	Mesh *oldme= oldob->data;
	
	if (oldme->totface) {
		HyperMesh *hme= hypermesh_from_mesh(oldme, NULL);
		DispList *dl= subsurf_subdivide_to_displist(hme, oldme->subdiv, oldme->flag);
		DispListMesh *dlm= dl->mesh;
		MFace *mfaces;
		int i;
	
		if (dlm->totvert>65000)
			error("Too many vertices");
		else {
			me->totface= dlm->totface;
			me->totvert= dlm->totvert;
	
			me->mvert= MEM_dupallocN(dlm->mvert);
			me->mface= mfaces= MEM_mallocN(sizeof(*mfaces)*me->totface, "me->mface");
			me->tface= MEM_dupallocN(dlm->tface);
			me->mcol= MEM_dupallocN(dlm->mcol);
	
			for (i=0; i<me->totface; i++) {
				MFace *mf= &mfaces[i];
				MFaceInt *oldmf= &dlm->mface[i];
		
				mf->v1= oldmf->v1;
				mf->v2= oldmf->v2;
				mf->v3= oldmf->v3;
				mf->v4= oldmf->v4;
				mf->flag= oldmf->flag;
				mf->mat_nr= oldmf->mat_nr;
				mf->puno= 0;
				mf->edcode= ME_V1V2|ME_V2V3|ME_V3V4|ME_V4V1;
			}
		}
		
		free_disp_elem(dl);
	}
}

DispList* subsurf_mesh_to_displist(Mesh *me, DispList *dl, short subdiv)
{
	HyperMesh *hme;
	
	hme= hypermesh_from_mesh(me, dl);

	return subsurf_subdivide_to_displist(hme, subdiv, me->flag);
}

void subsurf_calculate_limit_positions(Mesh *me, float (*positions_r)[3]) 
{
	/* Finds the subsurf limit positions for the verts in a mesh 
	 * and puts them in an array of floats. Please note that the 
	 * calculated vert positions is incorrect for the verts 
	 * on the boundary of the mesh.
	 */
	HyperMesh *hme= hypermesh_from_mesh(me, NULL);
	HyperMesh *nme= hypermesh_new();
	float edge_sum[3], face_sum[3];
	HyperVert *hv;
	LinkNode *l;
	int i;

	hypermesh_subdivide(hme, nme);

	for (i= me->totvert-1,hv=hme->verts; i>=0; i--,hv=hv->next) {
		int N= 0;
                
		edge_sum[0]= edge_sum[1]= edge_sum[2]= 0.0;
		face_sum[0]= face_sum[1]= face_sum[2]= 0.0;

		for (N=0,l=hv->edges; l; N++,l= l->next) {
			Vec3Add(edge_sum, ((HyperEdge*) l->link)->ep->co);
		}
		for (l=hv->faces; l; l= l->next) {
			Vec3Add(face_sum, ((HyperFace*) l->link)->mid->co);
		}

		positions_r[i][0] = 
			(hv->nmv->co[0]*N*N + edge_sum[0]*4 + face_sum[0])/(N*(N+5));
		positions_r[i][1] = 
			(hv->nmv->co[1]*N*N + edge_sum[1]*4 + face_sum[1])/(N*(N+5));
		positions_r[i][2] = 
			(hv->nmv->co[2]*N*N + edge_sum[2]*4 + face_sum[2])/(N*(N+5));
	}

	hypermesh_free(nme);
	hypermesh_free(hme);
}
