/**
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

#include <stdlib.h>
#include <string.h>
#include <math.h>

#ifdef HAVE_CONFIG_H
#include <config.h>
#endif

#ifdef WIN32
#include "BLI_winstuff.h"
#endif
#include "MEM_guardedalloc.h"

#include "BLI_blenlib.h"
#include "BLI_arithb.h"
#include "BLI_editVert.h"
#include "BLI_rand.h"

#include "MTC_matrixops.h"

#include "DNA_mesh_types.h"
#include "DNA_object_types.h"
#include "DNA_screen_types.h"
#include "DNA_key_types.h"
#include "DNA_scene_types.h"
#include "DNA_view3d_types.h"
#include "DNA_material_types.h"
#include "DNA_texture_types.h"

#include "BKE_utildefines.h"
#include "BKE_key.h"
#include "BKE_object.h"
#include "BKE_texture.h"
#include "BKE_displist.h"
#include "BKE_global.h"
#include "BKE_library.h"
#include "BKE_main.h"
#include "BKE_material.h"
#include "BKE_mesh.h"

#include "BIF_gl.h"
#include "BIF_graphics.h"
#include "BIF_editkey.h"
#include "BIF_space.h"
#include "BIF_toolbox.h"
#include "BIF_screen.h"
#include "BIF_interface.h"
#include "BIF_editmesh.h"
#include "BIF_mywindow.h"

#include "BSE_view.h"
#include "BSE_edit.h"
#include "BSE_trans_types.h"

#include "BDR_drawobject.h"
#include "BDR_editobject.h"
#include "BDR_editface.h"
#include "BDR_vpaint.h"

#include "mydevice.h"
#include "blendef.h"
#include "interface.h" /* MAART: for NUM and FLO types */
#include "nla.h"		/* For __NLA : Important - Do not remove! */
#include "render.h"

/****/

static void free_editverts(ListBase *edve);
static float convex(float *v1, float *v2, float *v3, float *v4);

/****/


/* extern ListBase fillvertbase, filledgebase; */ /* scanfill.c, in
    the lib... already in BLI_blenlib.h */

/*  voor debug:
#define free(a)			freeN(a)
#define malloc(a)		mallocN(a, "malloc")
#define calloc(a, b)	callocN((a)*(b), "calloc")
#define freelist(a)		freelistN(a)
*/

extern short editbutflag;

static float icovert[12][3] = {
	{0,0,-200}, 
	{144.72, -105.144,-89.443},
	{-55.277, -170.128,-89.443}, 
	{-178.885,0,-89.443},
	{-55.277,170.128,-89.443}, 
	{144.72,105.144,-89.443},
	{55.277,-170.128,89.443},
	{-144.72,-105.144,89.443},
	{-144.72,105.144,89.443},
	{55.277,170.128,89.443},
	{178.885,0,89.443},
	{0,0,200}
};
static short icovlak[20][3] = {
	{1,0,2},
	{1,0,5},
	{2,0,3},
	{3,0,4},
	{4,0,5},
	{1,5,10},
	{2,1,6},
	{3,2,7},
	{4,3,8},
	{5,4,9},
	{10,1,6},
	{6,2,7},
	{7,3,8},
	{8,4,9},
	{9,5,10},
	{6,10,11},
	{7,6,11},
	{8,7,11},
	{9,8,11},
	{10,9,11}
};

/* DEFINES */
#define UVCOPY(t, s) memcpy(t, s, 2 * sizeof(float));

#define TEST_EDITMESH	if(G.obedit==0) return; \
						if( (G.vd->lay & G.obedit->lay)==0 ) return;

#define FACE_MARKCLEAR(f) (f->f1 = 1)

/* ***************** HASH ********************* */

/* HASH struct voor snel opzoeken edges */
struct HashEdge {
	struct EditEdge *eed;
	struct HashEdge *next;
};

struct HashEdge *hashedgetab=0;

/********* qsort routines *********/


struct xvertsort {
	float x;
	EditVert *v1;
};

/* Functions */
static int vergxco(const void *v1, const void *v2)
{
	const struct xvertsort *x1=v1, *x2=v2;

	if( x1->x > x2->x ) return 1;
	else if( x1->x < x2->x) return -1;
	return 0;
}

struct vlaksort {
	long x;
	struct EditVlak *evl;
};


static int vergvlak(const void *v1, const void *v2)
{
	const struct vlaksort *x1=v1, *x2=v2;
	
	if( x1->x > x2->x ) return 1;
	else if( x1->x < x2->x) return -1;
	return 0;
}


/* ************ ADD / REMOVE / FIND ****************** */

#define EDHASH(a, b)	( (a)*256 + (b) )
#define EDHASHSIZE	65536

#if 0
static void check_hashedge(void)
{
	int i, i2,  doubedge=0;
	struct HashEdge *he,  *he2;
	
	for (i=0; i<64; i++) {
		he= hashedgetab+i;
		
		while (he && he->eed) {
			for (i2=i+1; i2<64; i2++) {
				he2= hashedgetab+i2;
				
				while (he2) {
					if (he->eed == he2->eed) doubedge++;
									
					he2= he2->next;
				}	
			}
			
			he= he->next;
		}	
	}
	
	if (doubedge) printf("%d double edges!\n", doubedge);
}
#endif

EditVert *addvertlist(float *vec)
{
	EditVert *eve;
	static unsigned char hashnr= 0;

	eve= calloc(sizeof(EditVert),1);
	BLI_addtail(&G.edve, eve);
	
	if(vec) VECCOPY(eve->co, vec);

	eve->hash= hashnr++;

	return eve;
}

EditEdge *findedgelist(EditVert *v1, EditVert *v2)
{
	EditVert *v3;
	struct HashEdge *he;

	if(hashedgetab==0) {
		hashedgetab= MEM_callocN(EDHASHSIZE*sizeof(struct HashEdge), "hashedgetab");
	}
	
	/* swap ? */
	if( (long)v1 > (long)v2) {
		v3= v2; 
		v2= v1; 
		v1= v3;
	}
	
	/* eerst even op de flip-plek kijken */
	
/* 	he= hashedgetab + EDHASH(v2->hash, v1->hash); */
/* 	if(he->eed && he->eed->v1==v1 && he->eed->v2==v2) return he->eed; */
	
	
	he= hashedgetab + EDHASH(v1->hash, v2->hash);
	
	while(he) {
		
		if(he->eed && he->eed->v1==v1 && he->eed->v2==v2) return he->eed;
		
		he= he->next;
	}
	return 0;
}

static void insert_hashedge(EditEdge *eed)
{
	/* er van uitgaande dat eed nog niet in lijst zit, en eerst een find is gedaan */
	
	struct HashEdge *first, *he;

	/* eerst even op de flip-plek kijken */
/* 	he= hashedgetab + EDHASH(eed->v2->hash, eed->v1->hash); */

/* 	if(he->eed==0) { */
/* 		he->eed= eed; */
/* 		return; */
/* 	} */

	first= hashedgetab + EDHASH(eed->v1->hash, eed->v2->hash);

	if( first->eed==0 ) {
		first->eed= eed;
	}
	else {
		he= (struct HashEdge *)malloc(sizeof(struct HashEdge)); 
		he->eed= eed;
		he->next= first->next;
		first->next= he;
	}
}

static void remove_hashedge(EditEdge *eed)
{
	/* er van uitgaande dat eed in lijst zit */
	
	struct HashEdge *first, *he, *prev=NULL;


	/* eerst even op de flip-plek kijken */
/* 	first= hashedgetab + EDHASH(eed->v2->hash, eed->v1->hash); */

/* 	if(first->eed==eed) { */
		/* uit lijst verwijderen */
		
/* 		if(first->next) { */
/* 			he= first->next; */
/* 			first->eed= he->eed; */
/* 			first->next= he->next; */
/* 			free(he); */
/* 		} */
/* 		else first->eed= 0; */

/* 		return; */
/* 	} */


	he=first= hashedgetab + EDHASH(eed->v1->hash, eed->v2->hash);

	while(he) {
		if(he->eed == eed) {
			/* uit lijst verwijderen */
			if(he==first) {
				if(first->next) {
					he= first->next;
					first->eed= he->eed;
					first->next= he->next;
					free(he);
				}
				else he->eed= 0;
			}
			else {
				prev->next= he->next;
				free(he);
			}
			return;
		}
		prev= he;
		he= he->next;
	}
}

void free_hashedgetab(void)
{
	struct HashEdge *he, *first, *hen;
	int a;
/* 	int test[30], nr, toted=0; */
	
	/* for(a=0; a<30; a++) test[a]=0; */
	
	if(hashedgetab) {
	
		first= hashedgetab;
		for(a=0; a<EDHASHSIZE; a++, first++) {
			he= first->next;
			/* nr= 0; */
			/* if(first->eed) toted++; */
			/* if(first->eed) nr++; */
			while(he) {
				hen= he->next;
				free(he);
				he= hen;
				/* nr++; */
			}
			/* if(nr>29) nr= 29; */
			/* test[nr]++; */
		}
		MEM_freeN(hashedgetab);
		hashedgetab= 0;

		/* printf("toted %d\n", toted); */
		/* toted= 0; */
		/* for(a=0; a<30; a++) { */
		/* 	printf("tab %d %d\n", a, test[a]); */
		/* } */
	}
}

EditEdge *addedgelist(EditVert *v1, EditVert *v2)
{
	EditVert *v3;
	EditEdge *eed;
	int swap= 0;
	
	/* swap ? */
	if(v1>v2) {
		v3= v2; 
		v2= v1; 
		v1= v3;
		swap= 1;
	}

	if(v1==v2) return 0;
	if(v1==0 || v2==0) return 0;
	
	/* opzoeken in hashlijst */
	eed= findedgelist(v1, v2);
	
	if(eed==0) {

		eed= (EditEdge *)calloc(sizeof(EditEdge), 1);
		eed->v1= v1;
		eed->v2= v2;
		BLI_addtail(&G.eded, eed);
		eed->dir= swap;
		insert_hashedge(eed);
	}
	return eed;
}


void remedge(EditEdge *eed)
{

	BLI_remlink(&G.eded, eed);

	remove_hashedge(eed);
}

static void freevlak(EditVlak *evl)
{
	free(evl);
}

static void freevlaklist(ListBase *lb)
{
	EditVlak *evl, *next;
	
	evl= lb->first;
	while(evl) {
		next= evl->next;
		freevlak(evl);
		evl= next;
	}
	lb->first= lb->last= 0;
}

EditVlak *addvlaklist(EditVert *v1, EditVert *v2, EditVert *v3, EditVert *v4, EditVlak *example)
{
	EditVlak *evl;
	EditEdge *e1, *e2=0, *e3=0, *e4=0;
	

	/* voeg vlak toe aan lijst en doe meteen de edges */
	e1= addedgelist(v1, v2);
	if(v3) e2= addedgelist(v2, v3);
	if(v4) e3= addedgelist(v3, v4); else e3= addedgelist(v3, v1);
	if(v4) e4= addedgelist(v4, v1);
	
	if(v1==v2 || v2==v3 || v1==v3) return 0;
	if(e2==0) return 0;

	evl= (EditVlak *)calloc(sizeof(EditVlak), 1);
	evl->v1= v1;
	evl->v2= v2;
	evl->v3= v3;
	evl->v4= v4;

	evl->e1= e1;
	evl->e2= e2;
	evl->e3= e3;
	evl->e4= e4;

	if(example) {
		evl->mat_nr= example->mat_nr;
		evl->tface= example->tface;
		evl->flag= example->flag;
		memcpy(evl->col, example->col, sizeof(example->col));
		memcpy(evl->uv, example->uv, sizeof(example->uv));
	}
	else { 
		if (G.obedit && G.obedit->actcol)
			evl->mat_nr= G.obedit->actcol-1;
		default_uv(evl->uv, 1.0);

		/* Initialize colors */
		evl->col[0]= evl->col[1]= evl->col[2]= evl->col[3]= vpaint_get_current_col();
	}
	
	BLI_addtail(&G.edvl, evl);

	if(evl->v4) CalcNormFloat4(v1->co, v2->co, v3->co, v4->co, evl->n);
	else CalcNormFloat(v1->co, v2->co, v3->co, evl->n);

	return evl;
}

static int comparevlak(EditVlak *vl1, EditVlak *vl2)
{
	EditVert *v1, *v2, *v3, *v4;
	
	if(vl1->v4 && vl2->v4) {
		v1= vl2->v1;
		v2= vl2->v2;
		v3= vl2->v3;
		v4= vl2->v4;
		
		if(vl1->v1==v1 || vl1->v2==v1 || vl1->v3==v1 || vl1->v4==v1) {
			if(vl1->v1==v2 || vl1->v2==v2 || vl1->v3==v2 || vl1->v4==v2) {
				if(vl1->v1==v3 || vl1->v2==v3 || vl1->v3==v3 || vl1->v4==v3) {
					if(vl1->v1==v4 || vl1->v2==v4 || vl1->v3==v4 || vl1->v4==v4) {
						return 1;
					}
				}
			}
		}
	}
	else if(vl1->v4==0 && vl2->v4==0) {
		v1= vl2->v1;
		v2= vl2->v2;
		v3= vl2->v3;

		if(vl1->v1==v1 || vl1->v2==v1 || vl1->v3==v1) {
			if(vl1->v1==v2 || vl1->v2==v2 || vl1->v3==v2) {
				if(vl1->v1==v3 || vl1->v2==v3 || vl1->v3==v3) {
					return 1;
				}
			}
		}
	}

	return 0;
}


#if 0
static int dubbelvlak(EditVlak *evltest)
{
	
	EditVlak *evl;
	
	evl= G.edvl.first;
	while(evl) {
		if(evl!=evltest) {
			if(comparevlak(evltest, evl)) return 1;
		}
		evl= evl->next;
	}
	return 0;
}
#endif

static int exist_vlak(EditVert *v1, EditVert *v2, EditVert *v3, EditVert *v4)
{
	EditVlak *evl, evltest;
	
	evltest.v1= v1;
	evltest.v2= v2;
	evltest.v3= v3;
	evltest.v4= v4;
	
	evl= G.edvl.first;
	while(evl) {
		if(comparevlak(&evltest, evl)) return 1;
		evl= evl->next;
	}
	return 0;
}


static int vlakselectedOR(EditVlak *evl, int flag)
{
	
	if(evl->v1->f & flag) return 1;
	if(evl->v2->f & flag) return 1;
	if(evl->v3->f & flag) return 1;
	if(evl->v4 && (evl->v4->f & 1)) return 1;
	return 0;
}

int vlakselectedAND(EditVlak *evl, int flag)
{
	if(evl->v1->f & flag) {
		if(evl->v2->f & flag) {
			if(evl->v3->f & flag) {
				if(evl->v4) {
					if(evl->v4->f & flag) return 1;
				}
				else return 1;
			}
		}
	}
	return 0;
}

void recalc_editnormals(void)
{
	EditVlak *evl;

	evl= G.edvl.first;
	while(evl) {
		if(evl->v4) CalcNormFloat4(evl->v1->co, evl->v2->co, evl->v3->co, evl->v4->co, evl->n);
		else CalcNormFloat(evl->v1->co, evl->v2->co, evl->v3->co, evl->n);
		evl= evl->next;
	}
}

static void flipvlak(EditVlak *evl)
{
	if(evl->v4) {
		SWAP(EditVert *, evl->v2, evl->v4);
		SWAP(EditEdge *, evl->e1, evl->e4);
		SWAP(EditEdge *, evl->e2, evl->e3);
		SWAP(unsigned int, evl->col[1], evl->col[3]);
		if(evl->tface) {
			SWAP(float, evl->uv[1][0], evl->uv[3][0]);
			SWAP(float, evl->uv[1][1], evl->uv[3][1]);
		}
	}
	else {
		SWAP(EditVert *, evl->v2, evl->v3);
		SWAP(EditEdge *, evl->e1, evl->e3);
		SWAP(unsigned int, evl->col[1], evl->col[2]);
		evl->e2->dir= 1-evl->e2->dir;
		if(evl->tface) {
			SWAP(float, evl->uv[1][0], evl->uv[2][0]);
			SWAP(float, evl->uv[1][1], evl->uv[2][1]);
		}
	}
	if(evl->v4) CalcNormFloat4(evl->v1->co, evl->v2->co, evl->v3->co, evl->v4->co, evl->n);
	else CalcNormFloat(evl->v1->co, evl->v2->co, evl->v3->co, evl->n);
}


void flip_editnormals(void)
{
	EditVlak *evl;
	
	evl= G.edvl.first;
	while(evl) {
		if( vlakselectedAND(evl, 1) ) {
			flipvlak(evl);
		}
		evl= evl->next;
	}
}

/* ************************ IN & OUT ***************************** */

static void edge_normal_compare(EditEdge *eed, EditVlak *evl1)
{
	EditVlak *evl2;
	float cent1[3], cent2[3];
	float inp;
	
	evl2= (EditVlak *)eed->vn;
	if(evl1==evl2) return;
	
	inp= evl1->n[0]*evl2->n[0] + evl1->n[1]*evl2->n[1] + evl1->n[2]*evl2->n[2];
	if(inp<0.999 && inp >-0.999) eed->f= 1;
		
	if(evl1->v4) CalcCent4f(cent1, evl1->v1->co, evl1->v2->co, evl1->v3->co, evl1->v4->co);
	else CalcCent3f(cent1, evl1->v1->co, evl1->v2->co, evl1->v3->co);
	if(evl2->v4) CalcCent4f(cent2, evl2->v1->co, evl2->v2->co, evl2->v3->co, evl2->v4->co);
	else CalcCent3f(cent2, evl2->v1->co, evl2->v2->co, evl2->v3->co);
	
	VecSubf(cent1, cent2, cent1);
	Normalise(cent1);
	inp= cent1[0]*evl1->n[0] + cent1[1]*evl1->n[1] + cent1[2]*evl1->n[2]; 

	if(inp < -0.001 ) eed->f1= 1;
}

static void edge_drawflags(void)
{
	EditVert *eve;
	EditEdge *eed, *e1, *e2, *e3, *e4;
	EditVlak *evl;
	
	/* - tel aantal keren in vlakken gebruikt: 0 en 1 is tekenen
	 * - edges meer dan 1 keer: in *vn zit pointer naar (eerste) vlak
	 * - loop alle vlakken af, is normaal te afwijkend: tekenen (flag wordt 1)
	 */
	
	recalc_editnormals();
	
	/* init */
	eve= G.edve.first;
	while(eve) {
		eve->f1= 1;		/* wordt bij test op nul gezet */
		eve= eve->next;
	}
	eed= G.eded.first;
	while(eed) {
		eed->f= eed->f1= 0;
		eed->vn= 0;
		eed= eed->next;
	}

	evl= G.edvl.first;
	while(evl) {
		e1= evl->e1;
		e2= evl->e2;
		e3= evl->e3;
		e4= evl->e4;
		if(e1->f<3) e1->f+= 1;
		if(e2->f<3) e2->f+= 1;
		if(e3->f<3) e3->f+= 1;
		if(e4 && e4->f<3) e4->f+= 1;
		
		if(e1->vn==0) e1->vn= (EditVert *)evl;
		if(e2->vn==0) e2->vn= (EditVert *)evl;
		if(e3->vn==0) e3->vn= (EditVert *)evl;
		if(e4 && e4->vn==0) e4->vn= (EditVert *)evl;
		
		evl= evl->next;
	}

	if(G.f & G_ALLEDGES) {
		evl= G.edvl.first;
		while(evl) {
			if(evl->e1->f>=2) evl->e1->f= 1;
			if(evl->e2->f>=2) evl->e2->f= 1;
			if(evl->e3->f>=2) evl->e3->f= 1;
			if(evl->e4 && evl->e4->f>=2) evl->e4->f= 1;
			
			evl= evl->next;
		}		
	}	
	else {
		
		/* single-edges afvangen voor cylinder flag */
		
		eed= G.eded.first;
		while(eed) {
			if(eed->f==1) eed->f1= 1;
			eed= eed->next;
		}

		/* alle vlakken, alle edges met flag==2: vergelijk normaal */
		evl= G.edvl.first;
		while(evl) {
			if(evl->e1->f==2) edge_normal_compare(evl->e1, evl);
			if(evl->e2->f==2) edge_normal_compare(evl->e2, evl);
			if(evl->e3->f==2) edge_normal_compare(evl->e3, evl);
			if(evl->e4 && evl->e4->f==2) edge_normal_compare(evl->e4, evl);
			
			evl= evl->next;
		}
		
		/* sphere collision flag */
		
		eed= G.eded.first;
		while(eed) {
			if(eed->f1!=1) {
				eed->v1->f1= eed->v2->f1= 0;
			}
			eed= eed->next;
		}
		
	}
}

static int contrpuntnorm(float *n, float *puno)
{
	float inp;

	inp= n[0]*puno[0]+n[1]*puno[1]+n[2]*puno[2];

	/* angles 90 degrees: dont flip */
	if(inp> -0.000001) return 0;

	return 1;
}

void vertexnormals(int testflip)
{
	Mesh *me;
	EditVert *eve;
	EditVlak *evl;	
	float n1[3], n2[3], n3[3], n4[3], co[4], fac1, fac2, fac3, fac4, *temp;
	float *f1, *f2, *f3, *f4, xn, yn, zn;
	float opp, len;
	
	if(G.obedit && G.obedit->type==OB_MESH) {
		me= G.obedit->data;
		if((me->flag & ME_TWOSIDED)==0) testflip= 0;
	}

	if(G.totvert==0) return;

	if(G.totface==0) {
		/* namaak puno's voor halopuno! */
		eve= G.edve.first;
		while(eve) {
			VECCOPY(eve->no, eve->co);
			Normalise( (float *)eve->no);
			eve= eve->next;
		}
		return;
	}

	/* clear normals */	
	eve= G.edve.first;
	while(eve) {
		eve->no[0]= eve->no[1]= eve->no[2]= 0.0;
		eve= eve->next;
	}
	
	/* berekenen cos hoeken en oppervlakte en optellen bij puno */
	evl= G.edvl.first;
	while(evl) {
		VecSubf(n1, evl->v2->co, evl->v1->co);
		VecSubf(n2, evl->v3->co, evl->v2->co);
		Normalise(n1);
		Normalise(n2);

		if(evl->v4==0) {
			VecSubf(n3, evl->v1->co, evl->v3->co);
			Normalise(n3);
			
			/* opp= AreaT3Dfl(evl->v1->co, evl->v2->co, evl->v3->co); */
			/* if(opp!=0.0) opp=1.0/opp;  */
			/* opp= sqrt(opp); */
			/* for smooth subdivide...*/
			opp= 1.0;
			co[0]= opp*saacos(-n3[0]*n1[0]-n3[1]*n1[1]-n3[2]*n1[2]);
			co[1]= opp*saacos(-n1[0]*n2[0]-n1[1]*n2[1]-n1[2]*n2[2]);
			co[2]= opp*saacos(-n2[0]*n3[0]-n2[1]*n3[1]-n2[2]*n3[2]);
			
		}
		else {
			VecSubf(n3, evl->v4->co, evl->v3->co);
			VecSubf(n4, evl->v1->co, evl->v4->co);
			Normalise(n3);
			Normalise(n4);
			
			/* opp= AreaQ3Dfl(evl->v1->co, evl->v2->co, evl->v3->co, evl->v4->co); */
			/* if(opp!=0.0) opp=1.0/opp;  */
			/* opp= sqrt(opp); */
			/* for smooth subdivide...*/
			opp= 1.0;
			co[0]= opp*saacos(-n4[0]*n1[0]-n4[1]*n1[1]-n4[2]*n1[2]);
			co[1]= opp*saacos(-n1[0]*n2[0]-n1[1]*n2[1]-n1[2]*n2[2]);
			co[2]= opp*saacos(-n2[0]*n3[0]-n2[1]*n3[1]-n2[2]*n3[2]);
			co[3]= opp*saacos(-n3[0]*n4[0]-n3[1]*n4[1]-n3[2]*n4[2]);
		}
		
		temp= evl->v1->no;
		if(testflip && contrpuntnorm(evl->n, temp) ) co[0]= -co[0];
		temp[0]+= co[0]*evl->n[0];
		temp[1]+= co[0]*evl->n[1];
		temp[2]+= co[0]*evl->n[2];
		
		temp= evl->v2->no;
		if(testflip && contrpuntnorm(evl->n, temp) ) co[1]= -co[1];
		temp[0]+= co[1]*evl->n[0];
		temp[1]+= co[1]*evl->n[1];
		temp[2]+= co[1]*evl->n[2];
		
		temp= evl->v3->no;
		if(testflip && contrpuntnorm(evl->n, temp) ) co[2]= -co[2];
		temp[0]+= co[2]*evl->n[0];
		temp[1]+= co[2]*evl->n[1];
		temp[2]+= co[2]*evl->n[2];
		
		if(evl->v4) {
			temp= evl->v4->no;
			if(testflip && contrpuntnorm(evl->n, temp) ) co[3]= -co[3];
			temp[0]+= co[3]*evl->n[0];
			temp[1]+= co[3]*evl->n[1];
			temp[2]+= co[3]*evl->n[2];
		}
		
		evl= evl->next;
	}

	/* normaliseren puntnormalen */
	eve= G.edve.first;
	while(eve) {
		len= Normalise(eve->no);
		if(len==0.0) {
			VECCOPY(eve->no, eve->co);
			Normalise( eve->no);
		}
		eve= eve->next;
	}
	
	/* puntnormaal omklap-vlaggen voor bij shade */
	evl= G.edvl.first;
	while(evl) {
		evl->f=0;			

		if(testflip) {
			f1= evl->v1->no;
			f2= evl->v2->no;
			f3= evl->v3->no;
			
			fac1= evl->n[0]*f1[0] + evl->n[1]*f1[1] + evl->n[2]*f1[2];
			if(fac1<0.0) {
				evl->f = ME_FLIPV1;
			}
			fac2= evl->n[0]*f2[0] + evl->n[1]*f2[1] + evl->n[2]*f2[2];
			if(fac2<0.0) {
				evl->f += ME_FLIPV2;
			}
			fac3= evl->n[0]*f3[0] + evl->n[1]*f3[1] + evl->n[2]*f3[2];
			if(fac3<0.0) {
				evl->f += ME_FLIPV3;
			}
			if(evl->v4) {
				f4= evl->v4->no;
				fac4= evl->n[0]*f4[0] + evl->n[1]*f4[1] + evl->n[2]*f4[2];
				if(fac4<0.0) {
					evl->f += ME_FLIPV4;
				}
			}
		}
		/* proj voor cubemap! */
		xn= fabs(evl->n[0]);
		yn= fabs(evl->n[1]);
		zn= fabs(evl->n[2]);
		
		if(zn>xn && zn>yn) evl->f += ME_PROJXY;
		else if(yn>xn && yn>zn) evl->f += ME_PROJXZ;
		else evl->f += ME_PROJYZ;
		
		evl= evl->next;
	}
}

void free_editMesh(void)
{

//	if(G.edve.first) BLI_freelist(&G.edve);
	if(G.edve.first) free_editverts(&G.edve);
	if(G.eded.first) BLI_freelist(&G.eded);
	if(G.edvl.first) freevlaklist(&G.edvl);
	free_hashedgetab();
	G.totvert= G.totface= 0;
}

static void free_editverts(ListBase *edve) {
#ifdef __NLA
	EditVert *eve;
#endif

	if (!edve)
		return;

	if (!edve->first)
		return;

#ifdef __NLA
	for (eve= edve->first; eve; eve=eve->next){
		if (eve->dw)
			MEM_freeN (eve->dw);
	}
#endif

	BLI_freelist (edve);

}

static void free_editvert (EditVert *eve)
{
#ifdef __NLA
	if (eve->dw)
		MEM_freeN (eve->dw);
#endif
	free (eve);
}

void make_editMesh(void)
{
	Mesh *me;
	MFace *mface;
	TFace *tface;
	MVert *mvert;
	KeyBlock *actkey=0;
	EditVert *eve, **evlist, *eve1, *eve2, *eve3, *eve4;
	EditVlak *evl;
	int tot, a;

	if(G.obedit==0) return;

	/* ivm reload */
	free_editMesh();
	
	me= get_mesh(G.obedit);
	G.totvert= tot= me->totvert;

	if(tot==0) {
		countall();
		return;
	}
	
	waitcursor(1);

	/* keys? */
	if(me->key) {
		actkey= me->key->block.first;
		while(actkey) {
			if(actkey->flag & SELECT) break;
			actkey= actkey->next;
		}
	}

	if(actkey) {
		key_to_mesh(actkey, me);
		tot= actkey->totelem;
	}

	/* editverts aanmaken */
	mvert= me->mvert;

	evlist= (EditVert **)MEM_mallocN(tot*sizeof(void *),"evlist");
	for(a=0; a<tot; a++, mvert++) {
		eve= addvertlist(mvert->co);
		evlist[a]= eve;
		eve->no[0]= mvert->no[0]/32767.0;
		eve->no[1]= mvert->no[1]/32767.0;
		eve->no[2]= mvert->no[2]/32767.0;
#ifdef __NLA

		/* OLD VERSION */
		/*
		eve->totweight = mvert->totweight;
		if (mvert->dw){
			eve->dw = BLI_callocN (sizeof(MDeformWeight) * mvert->totweight, "deformWeight");
			memcpy (eve->dw, mvert->dw, sizeof(MDeformWeight) * mvert->totweight);
		}
		*/

		/* NEW VERSION */
		if (me->dvert){
			eve->totweight = me->dvert[a].totweight;
			if (me->dvert[a].dw){
				eve->dw = MEM_callocN (sizeof(MDeformWeight) * me->dvert[a].totweight, "deformWeight");
				memcpy (eve->dw, me->dvert[a].dw, sizeof(MDeformWeight) * me->dvert[a].totweight);
			}
		}

#endif
	}

	if(actkey && actkey->totelem!=me->totvert);
	else {
		unsigned int *mcol;
		
		/* edges en vlakken maken */
		mface= me->mface;
		tface= me->tface;
		mcol= (unsigned int *)me->mcol;
		
		for(a=0; a<me->totface; a++, mface++) {
			eve1= evlist[mface->v1];
			eve2= evlist[mface->v2];
			if(mface->v3) eve3= evlist[mface->v3]; else eve3= 0;
			if(mface->v4) eve4= evlist[mface->v4]; else eve4= 0;
			
			evl= addvlaklist(eve1, eve2, eve3, eve4, NULL);
			
			if(evl) {
				if(mcol) memcpy(evl->col, mcol, 4*sizeof(int));

				if(me->tface) {
					memcpy(evl->col, tface->col, sizeof(tface->col));
					memcpy(evl->uv, tface->uv, sizeof(tface->uv));

					if( tface->flag & TF_SELECT) {
						if(G.f & G_FACESELECT) {
							eve1->f |= 1;
							eve2->f |= 1;
							if(eve3) eve3->f |= 1;
							if(eve4) eve4->f |= 1;
						}
					}
				}
			
				evl->mat_nr= mface->mat_nr;
				evl->flag= mface->flag;
				evl->tface= tface;
			}

			if(me->tface) tface++;
			if(mcol) mcol+=4;
		}
	}
	MEM_freeN(evlist);
	
	countall();
	
	if (mesh_uses_displist(me))
		makeDispList(G.obedit);
	
	waitcursor(0);
}

/** Rotates MFace and UVFace vertices in case the last
  * vertex index is = 0. 
  * This function is a hack and may only be called in the
  * conversion from EditMesh to Mesh data.
  * This function is similar to test_index_mface in
  * blenkernel/intern/mesh.c. 
  * To not clutter the blenkernel code with more bad level
  * calls/structures, this function resides here.
  */


static void fix_faceindices(MFace *mface, EditVlak *evl, int nr)
{
	int a;
	float tmpuv[2];
	unsigned int tmpcol;

/*
mface =  ((MFace *) me->mface) + index;
	tface =  ((TFace *) me->tface) + index;

*/

	/* first test if the face is legal */

	if(mface->v3 && mface->v3==mface->v4) {
		mface->v4= 0;
		nr--;
	}
	if(mface->v2 && mface->v2==mface->v3) {
		mface->v3= mface->v4;
		mface->v4= 0;
		nr--;
	}
	if(mface->v1==mface->v2) {
		mface->v2= mface->v3;
		mface->v3= mface->v4;
		mface->v4= 0;
		nr--;
	}

	/* voorkom dat een nul op de verkeerde plek staat */
	if(nr==2) {
		if(mface->v2==0) SWAP(int, mface->v1, mface->v2);
	}
	else if(nr==3) {
		if(mface->v3==0) {
			SWAP(int, mface->v1, mface->v2);
			SWAP(int, mface->v2, mface->v3);
			/* rotate face UV coordinates, too */
			UVCOPY(tmpuv, evl->uv[0]);
			UVCOPY(evl->uv[0], evl->uv[1]);
			UVCOPY(evl->uv[1], evl->uv[2]);
			UVCOPY(evl->uv[2], tmpuv);
			/* same with vertex colours */
			tmpcol = evl->col[0];
			evl->col[0] = evl->col[1];
			evl->col[1] = evl->col[2];
			evl->col[2] = tmpcol;

			
			a= mface->edcode;
			mface->edcode= 0;
			if(a & ME_V1V2) mface->edcode |= ME_V3V1;
			if(a & ME_V2V3) mface->edcode |= ME_V1V2;
			if(a & ME_V3V1) mface->edcode |= ME_V2V3;
			
			a= mface->puno;
			mface->puno &= ~15;
			if(a & ME_FLIPV1) mface->puno |= ME_FLIPV2;
			if(a & ME_FLIPV2) mface->puno |= ME_FLIPV3;
			if(a & ME_FLIPV3) mface->puno |= ME_FLIPV1;
		}
	}
	else if(nr==4) {
		if(mface->v3==0 || mface->v4==0) {
			SWAP(int, mface->v1, mface->v3);
			SWAP(int, mface->v2, mface->v4);
			/* swap UV coordinates */
			UVCOPY(tmpuv, evl->uv[0]);
			UVCOPY(evl->uv[0], evl->uv[2]);
			UVCOPY(evl->uv[2], tmpuv);
			UVCOPY(tmpuv, evl->uv[1]);
			UVCOPY(evl->uv[1], evl->uv[3]);
			UVCOPY(evl->uv[3], tmpuv);
			/* swap vertex colours */
			tmpcol = evl->col[0];
			evl->col[0] = evl->col[2];
			evl->col[2] = tmpcol;
			tmpcol = evl->col[1];
			evl->col[1] = evl->col[3];
			evl->col[3] = tmpcol;

			a= mface->edcode;
			mface->edcode= 0;
			if(a & ME_V1V2) mface->edcode |= ME_V3V4;
			if(a & ME_V2V3) mface->edcode |= ME_V2V3;
			if(a & ME_V3V4) mface->edcode |= ME_V1V2;
			if(a & ME_V4V1) mface->edcode |= ME_V4V1;

			a= mface->puno;
			mface->puno &= ~15;
			if(a & ME_FLIPV1) mface->puno |= ME_FLIPV3;
			if(a & ME_FLIPV2) mface->puno |= ME_FLIPV4;
			if(a & ME_FLIPV3) mface->puno |= ME_FLIPV1;
			if(a & ME_FLIPV4) mface->puno |= ME_FLIPV2;
		}
	}

}



/* load from EditMode to Mesh */

void load_editMesh(void)
{
	Mesh *me;
	MFace *mface;
	MVert *mvert;
	MSticky *ms;
	KeyBlock *actkey=0;
	EditVert *eve;
	EditVlak *evl;
	EditEdge *eed;
	float *fp, nor[3];
	int i, a, ototvert;
#ifdef __NLA
	MDeformVert *dvert;
	int	usedDvert = 0;
#endif

	waitcursor(1);
	countall();
	
	me= get_mesh(G.obedit);
	
	ototvert= me->totvert;
	
	/* zijn er keys? */
	if(me->key) {
		actkey= me->key->block.first;
		while(actkey) {
			if(actkey->flag & SELECT) break;
			actkey= actkey->next;
		}
	}

	
	if(actkey && me->key->refkey!=actkey) {
		/* aktieve key && niet de refkey: alleen vertices */
				
		if(G.totvert) {
			if(actkey->data) MEM_freeN(actkey->data);
		
			fp=actkey->data= MEM_callocN(me->key->elemsize*G.totvert, "actkey->data");
			actkey->totelem= G.totvert;
	
			eve= G.edve.first;
			while(eve) {
				VECCOPY(fp, eve->co);
				fp+= 3;
				eve= eve->next;
			}
		}
	}
	else if(me->key && actkey==0) {
		/* er zijn keys, alleen veranderingen in mverts schrijven */
		/* als aantal vertices verschillen, beetje onvoorspelbaar */
			
		eve= G.edve.first;
		mvert= me->mvert;
		for(a=0; a<me->totvert; a++, mvert++) {
			VECCOPY(mvert->co, eve->co);
			eve= eve->next;
			if(eve==0) break;
		}
	}
	else {
		/* als er keys zijn: de refkey, anders gewoon de me */
		
		/* deze telt ook of edges niet in vlakken zitten: */
		/* eed->f==0 niet in vlak, f==1 is tekenen */
		/* eed->f1 : flag voor dynaface (cylindertest) */
		/* eve->f1 : flag voor dynaface (sphere test) */
		edge_drawflags();
	
		/* LET OP: op evl->f de punoflag */
		vertexnormals( (me->flag & ME_NOPUNOFLIP)==0 );
	
		eed= G.eded.first;
		while(eed) {
			if(eed->f==0) G.totface++;
			eed= eed->next;
		}
	
		/* nieuw Face blok */
		if(G.totface==0) mface= 0;
		else mface= MEM_callocN(G.totface*sizeof(MFace), "loadeditMesh1");
		/* nieuw Vertex blok */
		if(G.totvert==0) mvert= 0;
		else mvert= MEM_callocN(G.totvert*sizeof(MVert), "loadeditMesh2");

#ifdef __NLA
		if (G.totvert==0) dvert=0;
		else dvert = MEM_callocN(G.totvert*sizeof(MDeformVert), "loadeditMesh3");

		if (me->dvert) free_dverts(me->dvert, me->totvert);
		me->dvert=dvert;
#endif		
		if(me->mvert) MEM_freeN(me->mvert);
		me->mvert= mvert;

		if(me->mface) MEM_freeN(me->mface);
		me->mface= mface;
		me->totvert= G.totvert;
		me->totface= G.totface;
		
		/* de vertices, gebruik ->vn als teller */
		eve= G.edve.first;
		a=0;



		while(eve) {
			VECCOPY(mvert->co, eve->co);
			mvert->mat_nr= 255;  /* waarvoor ook al weer, haloos? */
			
			/* puno */
			VECCOPY(nor, eve->no);
			VecMulf(nor, 32767.0);
			VECCOPY(mvert->no, nor);
#ifdef __NLA
/*			OLD VERSION */
/*			mvert->totweight = eve->totweight;
			if (eve->dw){
				int	cv;
				mvert->dw = BLI_callocN (sizeof(MDeformWeight)*eve->totweight, "deformWeight");
				memcpy (mvert->dw, eve->dw, sizeof(MDeformWeight)*eve->totweight);
			}
*/
			/* NEW VERSION */
			if (dvert){
				dvert->totweight=eve->totweight;
				if (eve->dw){
					dvert->dw = MEM_callocN (sizeof(MDeformWeight)*eve->totweight, "deformWeight");
					memcpy (dvert->dw, eve->dw, sizeof(MDeformWeight)*eve->totweight);
					usedDvert++;
				}
			}
#endif

			eve->vn= (EditVert *)(long)(a++);  /* teller */
			
			mvert->flag= 0;
			if(eve->f1==1) mvert->flag |= ME_SPHERETEST;
			
			eve= eve->next;
			mvert++;
#ifdef __NLA
			dvert++;
#endif
		}
	
#ifdef __NLA
		/* If we didn't actually need the dverts, get rid of them */
		if (!usedDvert){
			free_dverts(me->dvert, G.totvert);
			me->dvert=NULL;
		}
#endif

		/* de vlakken */
		evl= G.edvl.first;
		i = 0;
		while(evl) {
			mface= &((MFace *) me->mface)[i];

			mface->v1= (unsigned int) evl->v1->vn;
			mface->v2= (unsigned int) evl->v2->vn;
			mface->v3= (unsigned int) evl->v3->vn;
			if(evl->v4) mface->v4= (unsigned int) evl->v4->vn;
			
			mface->mat_nr= evl->mat_nr;
			mface->puno= evl->f;
			mface->flag= evl->flag;
			
			/* mat_nr in vertex */
			if(me->totcol>1) {
				mvert= me->mvert+mface->v1;
				if(mvert->mat_nr == 255) mvert->mat_nr= mface->mat_nr;
				mvert= me->mvert+mface->v2;
				if(mvert->mat_nr == 255) mvert->mat_nr= mface->mat_nr;
				mvert= me->mvert+mface->v3;
				if(mvert->mat_nr == 255) mvert->mat_nr= mface->mat_nr;
				if(mface->v4) {
					mvert= me->mvert+mface->v4;
					if(mvert->mat_nr == 255) mvert->mat_nr= mface->mat_nr;
				}
			}
			
			/* dyna cilinder flag minder kritisch testen: 'dubbel' in vlakken laten zitten. 
			 * gaat anders fout bij scherpe hoeken (inpspeed voor een wel, ander niet!)
			 * Mogelijk oplossen door volgorde aan te passen: sphere-cyl-face. Kost te veel?
			 */

			/* letop: evl->e1->f==0 is losse edge */ 
			
			if(evl->e1->f==1) {
				mface->edcode |= ME_V1V2; 
				evl->e1->f= 2;
			}			
			if(evl->e2->f==1) {
				mface->edcode |= ME_V2V3; 
				evl->e2->f= 2;
			}
			if(evl->e3->f==1) {
				if(evl->v4) {
					mface->edcode |= ME_V3V4;
				}
				else {
					mface->edcode |= ME_V3V1;
				}
				evl->e3->f= 2;
			}
			if(evl->e4 && evl->e4->f==1) {
				mface->edcode |= ME_V4V1; 
				evl->e4->f= 2;
			}
			
			/* geen index '0' op plek 3 of 4 */
			if(evl->v4) fix_faceindices(mface, evl, 4);
			else fix_faceindices(mface, evl, 3);
			
			i++;
			evl= evl->next;
		}
		
		/* losse edges als vlak toevoegen */
		eed= G.eded.first;
		while(eed) {
			if( eed->f==0 ) {
				mface= &((MFace *) me->mface)[i];
				mface->v1= (unsigned int) eed->v1->vn;
				mface->v2= (unsigned int) eed->v2->vn;
				test_index_mface(mface, 2);
				mface->edcode= ME_V1V2;
				i++;
			}
			eed= eed->next;
		}
		
		tex_space_mesh(me);
		if(actkey) mesh_to_key(me, actkey);
		
		/* texmesh: ahv ->tface alles opnieuw maken */
		if(me->tface && me->totface) {
			TFace *tfn, *tf;
			
			tf=tfn= MEM_callocN(sizeof(TFace)*me->totface, "tface");
			evl= G.edvl.first;
			while(evl) {
				
				if(evl->tface) *tf= *(evl->tface);
				else default_tface(tf);
				
				memcpy(tf->col, evl->col, sizeof(tf->col));
				memcpy(tf->uv, evl->uv, sizeof(tf->uv));
				
				if(G.f & G_FACESELECT) {
					if( vlakselectedAND(evl, 1) ) tf->flag |= TF_SELECT;
					else tf->flag &= ~TF_SELECT;
				}
				
				/* sometimes editmode doesn't free (before render) */
				evl->tface= tf;

				tf++;
				evl= evl->next;
			}
			
			MEM_freeN(me->tface);
			me->tface= tfn;
		}
		else if(me->tface) {
			/* freeN(me->tface); */
			/* me->tface= 0; */
		}
		
		/* mcol: ahv indexnrs opnieuw maken */
		if(me->mcol && me->totface) {
			unsigned int *mcn, *mc;
			
			mc=mcn= MEM_mallocN(4*sizeof(int)*me->totface, "mcol");
			evl= G.edvl.first;
			while(evl) {
			
				memcpy(mc, evl->col, 4*sizeof(int));
				
				mc+=4;
				evl= evl->next;
			}
			
			MEM_freeN(me->mcol);
			me->mcol= (MCol *)mcn;
		}
		else if(me->mcol) {
			MEM_freeN(me->mcol);
			me->mcol= 0;
		}
	}
	
	if(actkey) do_spec_key(me->key);
	
	/* voor zekerheid: ->vn pointers wissen */
	eve= G.edve.first;
	while(eve) {
		eve->vn= 0;
		eve= eve->next;
	}

	/* displisten van alle users, ook deze */
	freedisplist(&me->disp);
	freedisplist(&G.obedit->disp);
	
	/* sticky */
	if(me->msticky) {
		if (ototvert<me->totvert) {
			ms= MEM_callocN(me->totvert*sizeof(MSticky), "msticky");
			memcpy(ms, me->msticky, ototvert*sizeof(MSticky));
			MEM_freeN(me->msticky);
			me->msticky= ms;
			error("Sticky was too small");
		}
	}
	waitcursor(0);
}


void remake_editMesh(void)
{

	if(okee("Reload Original data")==0) return;
	
	make_editMesh();
	allqueue(REDRAWVIEW3D, 0);
	makeDispList(G.obedit);
}

/* *********************  TOOLS  ********************* */



void make_sticky(void)
{
	Object *ob;
	Base *base;
	MVert *mvert;
	Mesh *me;
	MSticky *ms;
	float ho[4], mat[4][4];
	int a;
	
	if(G.scene->camera==0) return;
	
	if(G.obedit) {
		error("Unable to perform function in EditMode");
		return;
	}
	base= FIRSTBASE;
	while(base) {
		if TESTBASELIB(base) {
			if(base->object->type==OB_MESH) {
				ob= base->object;
				
				me= ob->data;
				mvert= me->mvert;
				if(me->msticky) MEM_freeN(me->msticky);
				me->msticky= MEM_mallocN(me->totvert*sizeof(MSticky), "sticky");
				
				/* stukje roteerscene */		
				R.r= G.scene->r;
				R.r.xsch= (R.r.size*R.r.xsch)/100;
				R.r.ysch= (R.r.size*R.r.ysch)/100;
				
				R.afmx= R.r.xsch/2;
				R.afmy= R.r.ysch/2;
				
				R.ycor= ( (float)R.r.yasp)/( (float)R.r.xasp);
		
				R.rectx= R.r.xsch; 
				R.recty= R.r.ysch;
				R.xstart= -R.afmx; 
				R.ystart= -R.afmy;
				R.xend= R.xstart+R.rectx-1;
				R.yend= R.ystart+R.recty-1;
		
				where_is_object(G.scene->camera);
				Mat4CpyMat4(R.viewinv, G.scene->camera->obmat);
				Mat4Ortho(R.viewinv);
				Mat4Invert(R.viewmat, R.viewinv);
				
				RE_setwindowclip(1, -1);
		
				where_is_object(ob);
				Mat4MulMat4(mat, ob->obmat, R.viewmat);
		
				ms= me->msticky;
				for(a=0; a<me->totvert; a++, ms++, mvert++) {
					VECCOPY(ho, mvert->co);
					Mat4MulVecfl(mat, ho);
					RE_projectverto(ho, ho);
					ms->co[0]= ho[0]/ho[3];
					ms->co[1]= ho[1]/ho[3];
				}
			}
		}
		base= base->next;
	}
	allqueue(REDRAWBUTSEDIT, 0);
}

void fasterdraw(void)
{
	Base *base;
	Mesh *me;
	MFace *mface;
	int toggle, a;

	if(G.obedit) return;

	/* vlaggen resetten */
	me= G.main->mesh.first;
	while(me) {
		me->flag &= ~ME_ISDONE;
		me= me->id.next;
	}

	base= FIRSTBASE;
	while(base) {
		if( TESTBASELIB(base) && (base->object->type==OB_MESH)) {
			me= base->object->data;
			if(me->id.lib==0 && (me->flag & ME_ISDONE)==0) {
				me->flag |= ME_ISDONE;
				mface= me->mface;
				toggle= 0;
				for(a=0; a<me->totface; a++) {
					if( (mface->edcode & ME_V1V2) && ( (toggle++) & 1) ) {
						mface->edcode-= ME_V1V2;
					}
					if( (mface->edcode & ME_V2V3) && ( (toggle++) & 1)) {
						mface->edcode-= ME_V2V3;
					}
					if( (mface->edcode & ME_V3V1) && ( (toggle++) & 1)) {
						mface->edcode-= ME_V3V1;
					}
					if( (mface->edcode & ME_V4V1) && ( (toggle++) & 1)) {
						mface->edcode-= ME_V4V1;
					}
					if( (mface->edcode & ME_V3V4) && ( (toggle++) & 1)) {
						mface->edcode-= ME_V3V4;
					}
					mface++;
				}
			}
		}
		base= base->next;
	}

	/* belangrijk?: vlaggen weer resetten */
	me= G.main->mesh.first;
	while(me) {
		me->flag &= ~ME_ISDONE;
		me= me->id.next;
	}

	allqueue(REDRAWVIEW3D, 0);
}

void slowerdraw(void)		/* reset fasterdraw */
{
	Base *base;
	Mesh *me;
	MFace *mface;
	int a;

	if(G.obedit) return;

	base= FIRSTBASE;
	while(base) {
		if( TESTBASELIB(base) && (base->object->type==OB_MESH)) {
			me= base->object->data;
			if(me->id.lib==0) {
				
				mface= me->mface;
				
				for(a=0; a<me->totface; a++) {
				
					mface->edcode |= ME_V1V2|ME_V2V3;
					mface++;
				}
			}
		}
		base= base->next;
	}

	allqueue(REDRAWVIEW3D, 0);
}


void convert_to_triface(int all)
{
	EditVlak *evl, *evln, *next;
	
	evl= G.edvl.first;
	while(evl) {
		next= evl->next;
		if(evl->v4) {
			if(all || vlakselectedAND(evl, 1) ) {
				
				evln= addvlaklist(evl->v1, evl->v2, evl->v3, 0, evl);
				evln= addvlaklist(evl->v1, evl->v3, evl->v4, 0, evl);

				if(evl->tface) {
					evln->uv[1][0]= evln->uv[2][0];
					evln->uv[1][1]= evln->uv[2][1];
					evln->uv[2][0]= evln->uv[3][0];
					evln->uv[2][1]= evln->uv[3][1];
				}
				
				evln->col[1]= evln->col[2];
				evln->col[2]= evln->col[3];
				
				BLI_remlink(&G.edvl, evl);
				freevlak(evl);
			}
		}
		evl= next;
	}
	
}


void deselectall_mesh(void)	/* toggle */
{
	EditVert *eve;
	int a;
	
	if(G.obedit->lay & G.vd->lay) {
		a= 0;
		eve= G.edve.first;
		while(eve) {
			if(eve->f & 1) {
				a= 1;
				break;
			}
			eve= eve->next;
		}
		eve= G.edve.first;
		while(eve) {
			if(eve->h==0) {
				if(a) eve->f&= -2;
				else eve->f|= 1;
			}
			eve= eve->next;
		}
		tekenvertices_ext(a==0);
	}
	countall();
}


void righthandfaces(int select)	/* maakt vlakken rechtsdraaiend */
{
	EditEdge *eed, *ed1, *ed2, *ed3, *ed4;
	EditVlak *evl, *startvl;
	float maxx, nor[3], cent[3];
	int totsel, found, foundone, direct, turn;

   /* op basis selectconnected om losse objecten te onderscheiden */

	/* tel per edge hoeveel vlakken het heeft */

	/* vind het meest linkse, voorste, bovenste vlak */

	/* zet normaal naar buiten en de eerste richtings vlaggen in de edges */

	/* loop object af en zet richtingen / richtingsvlaggen: alleen bij edges van 1 of 2 vlakken */
	/* dit is in feit de select connected */

	/* indien nog (selected) vlakken niet gedaan: opnieuw vind de meest linkse ... */

	waitcursor(1);

	eed= G.eded.first;
	while(eed) {
		eed->f= 0;
		eed->f1= 0;
		eed= eed->next;
	}

	/* vlakken en edges tellen */
	totsel= 0;
	evl= G.edvl.first;
	while(evl) {
		if(select==0 || vlakselectedAND(evl, 1) ) {
			evl->f= 1;
			totsel++;
			evl->e1->f1++;
			evl->e2->f1++;
			evl->e3->f1++;
			if(evl->v4) evl->e4->f1++;
		}
		else evl->f= 0;

		evl= evl->next;
	}

	while(totsel>0) {
		/* van buiten naar binnen */

		evl= G.edvl.first;
		startvl= 0;
		maxx= -1.0e10;

		while(evl) {
			if(evl->f) {
				CalcCent3f(cent, evl->v1->co, evl->v2->co, evl->v3->co);
				cent[0]= fabs(cent[0])+fabs(cent[1])+fabs(cent[2]);
				
				if(cent[0]>maxx) {
					maxx= cent[0];
					startvl= evl;
				}
			}
			evl= evl->next;
		}
		
		/* eerste vlak goedzetten: normaal berekenen */
		CalcNormFloat(startvl->v1->co, startvl->v2->co, startvl->v3->co, nor);
		CalcCent3f(cent, startvl->v1->co, startvl->v2->co, startvl->v3->co);
		
		/* eerste normaal staat zus of zo */
		if(select) {
			if(select==2) {
				if(cent[0]*nor[0]+cent[1]*nor[1]+cent[2]*nor[2] > 0.0) flipvlak(startvl);
			}
			else {
				if(cent[0]*nor[0]+cent[1]*nor[1]+cent[2]*nor[2] < 0.0) flipvlak(startvl);
			}
		}
		else if(cent[0]*nor[0]+cent[1]*nor[1]+cent[2]*nor[2] < 0.0) flipvlak(startvl);


		eed= startvl->e1;
		if(eed->v1==startvl->v1) eed->f= 1; 
		else eed->f= 2;
		
		eed= startvl->e2;
		if(eed->v1==startvl->v2) eed->f= 1; 
		else eed->f= 2;
		
		eed= startvl->e3;
		if(eed->v1==startvl->v3) eed->f= 1; 
		else eed->f= 2;
		
		eed= startvl->e4;
		if(eed) {
			if(eed->v1==startvl->v4) eed->f= 1; 
			else eed->f= 2;
		}
		
		startvl->f= 0;
		totsel--;

		/* de normalen testen */
		found= 1;
		direct= 1;
		while(found) {
			found= 0;
			if(direct) evl= G.edvl.first;
			else evl= G.edvl.last;
			while(evl) {
				if(evl->f) {
					turn= 0;
					foundone= 0;

					ed1= evl->e1;
					ed2= evl->e2;
					ed3= evl->e3;
					ed4= evl->e4;

					if(ed1->f) {
						if(ed1->v1==evl->v1 && ed1->f==1) turn= 1;
						if(ed1->v2==evl->v1 && ed1->f==2) turn= 1;
						foundone= 1;
					}
					else if(ed2->f) {
						if(ed2->v1==evl->v2 && ed2->f==1) turn= 1;
						if(ed2->v2==evl->v2 && ed2->f==2) turn= 1;
						foundone= 1;
					}
					else if(ed3->f) {
						if(ed3->v1==evl->v3 && ed3->f==1) turn= 1;
						if(ed3->v2==evl->v3 && ed3->f==2) turn= 1;
						foundone= 1;
					}
					else if(ed4 && ed4->f) {
						if(ed4->v1==evl->v4 && ed4->f==1) turn= 1;
						if(ed4->v2==evl->v4 && ed4->f==2) turn= 1;
						foundone= 1;
					}

					if(foundone) {
						found= 1;
						totsel--;
						evl->f= 0;

						if(turn) {
							if(ed1->v1==evl->v1) ed1->f= 2; 
							else ed1->f= 1;
							if(ed2->v1==evl->v2) ed2->f= 2; 
							else ed2->f= 1;
							if(ed3->v1==evl->v3) ed3->f= 2; 
							else ed3->f= 1;
							if(ed4) {
								if(ed4->v1==evl->v4) ed4->f= 2; 
								else ed4->f= 1;
							}

							flipvlak(evl);

						}
						else {
							if(ed1->v1== evl->v1) ed1->f= 1; 
							else ed1->f= 2;
							if(ed2->v1==evl->v2) ed2->f= 1; 
							else ed2->f= 2;
							if(ed3->v1==evl->v3) ed3->f= 1; 
							else ed3->f= 2;
							if(ed4) {
								if(ed4->v1==evl->v4) ed4->f= 1; 
								else ed4->f= 2;
							}
						}
					}
				}
				if(direct) evl= evl->next;
				else evl= evl->prev;
			}
			direct= 1-direct;
		}
	}

	recalc_editnormals();
	
	makeDispList(G.obedit);
	
	waitcursor(0);
}

static EditVert *findnearestvert(short sel)
{
	/* als sel==1 krijgen vertices met flag==1 een nadeel */
	EditVert *eve,*act=0;
	static EditVert *acto=0;
	short dist=100,temp,mval[2];

	if(G.edve.first==0) return 0;

	/* projektie doen */
	calc_meshverts_ext();	/* drawobject.c */
	
	/* er wordt geteld van acto->next tot last en van first tot acto */
	/* bestaat acto ? */
	eve= G.edve.first;
	while(eve) {
		if(eve==acto) break;
		eve= eve->next;
	}
	if(eve==0) acto= G.edve.first;

	if(acto==0) return 0;

	/* is er een aangegeven vertex? deel 1 */
	getmouseco_areawin(mval);
	eve= acto->next;
	while(eve) {
		if(eve->h==0) {
			temp= abs(mval[0]- eve->xs)+ abs(mval[1]- eve->ys);
			if( (eve->f & 1)==sel ) temp+=5;
			if(temp<dist) {
				act= eve;
				dist= temp;
				if(dist<4) break;
			}
		}
		eve= eve->next;
	}
	/* is er een aangegeven vertex? deel 2 */
	if(dist>3) {
		eve= G.edve.first;
		while(eve) {
			if(eve->h==0) {
				temp= abs(mval[0]- eve->xs)+ abs(mval[1]- eve->ys);
				if( (eve->f & 1)==sel ) temp+=5;
				if(temp<dist) {
					act= eve;
					if(temp<4) break;
					dist= temp;
				}
				if(eve== acto) break;
			}
			eve= eve->next;
		}
	}

	acto= act;
	return act;
}

static void tekenvertices_special(int mode, EditVert *act)
{
	/* voor speciale gevallen:
	 * mode 0: deselect geselecteerde, teken ze,  behalve act
	 * mode 1: teken alleen act
	 */
	ScrArea *tempsa, *sa;
	View3D *vd;
	EditVert *eve;
	float mat[4][4];
	int doit=0;
	
	/* eerst testen of er wel special vertices zijn */
	
	eve= (EditVert *)G.edve.first;
	while(eve) {
		eve->f1= 0;
		if(eve->h==0) {
			if(mode==0) {
				if(eve!=act && eve->f & 1) {
					doit= 1;
					eve->f1= 1;
					eve->f -= 1;
				}
			}
			else if(mode==1) {
				if(eve==act) eve->f1= 1;
				doit= 1;
			}
		}
		eve= eve->next;
	}
	if(doit==0) return;
	
	if(G.f & (G_FACESELECT+G_DRAWFACES)) {
		scrarea_queue_winredraw(curarea);
		return;
	}
	
	if(G.zbuf) glDisable(GL_DEPTH_TEST);
	
	glDrawBuffer(GL_FRONT);

	/* alle views aflopen */
	tempsa= curarea;
	sa= G.curscreen->areabase.first;
	while(sa) {
		if(sa->spacetype==SPACE_VIEW3D) {
			vd= sa->spacedata.first;
			if(G.obedit->lay & vd->lay) {
				areawinset(sa->win);
				mymultmatrix(G.obedit->obmat);

				MTC_Mat4SwapMat4(G.vd->persmat, mat);
				mygetsingmatrix(G.vd->persmat);
			
				tekenvertices(0);
				tekenvertices(1);
				
				MTC_Mat4SwapMat4(G.vd->persmat, mat);

				sa->win_swap= WIN_FRONT_OK;
				
				myloadmatrix(G.vd->viewmat);
			}
		}
		sa= sa->next;
	}
	if(curarea!=tempsa) areawinset(tempsa->win);
	
	glDrawBuffer(GL_BACK);
	if(G.zbuf) glEnable(GL_DEPTH_TEST);
}

void mouse_mesh(void)
{
	EditVert *act=0;

	act= findnearestvert(1);
	if(act) {
		
		if((G.qual & LR_SHIFTKEY)==0) {
			tekenvertices_special(0, act);
		}
		if( (act->f & 1)==0) act->f+= 1;
		else if(G.qual & LR_SHIFTKEY) act->f-= 1;

		tekenvertices_special(1, act);
		countall();
	}

	rightmouse_transform();
}

static void selectconnectedAll(void)
{
	EditVert *v1,*v2;
	EditEdge *eed;
	short flag=1,toggle=0;

	if(G.eded.first==0) return;

	while(flag==1) {
		flag= 0;
		toggle++;
		if(toggle & 1) eed= G.eded.first;
		else eed= G.eded.last;
		while(eed) {
			v1= eed->v1;
			v2= eed->v2;
			if(eed->h==0) {
				if(v1->f & 1) {
					if( (v2->f & 1)==0 ) {
						v2->f |= 1;
						flag= 1;
					}
				}
				else if(v2->f & 1) {
					if( (v1->f & 1)==0 ) {
						v1->f |= 1;
						flag= 1;
					}
				}
			}
			if(toggle & 1) eed= eed->next;
			else eed= eed->prev;
		}
	}
	countall();

	tekenvertices_ext(1);

}


void selectconnected_mesh(void)
{
	EditVert *eve,*v1,*v2,*act= 0;
	EditEdge *eed;
	short flag=1,sel,toggle=0;

	if(G.eded.first==0) return;

	if(G.qual & LR_CTRLKEY) {
		selectconnectedAll();
		return;
	}

	sel= 3;
	if(G.qual & LR_SHIFTKEY) sel=2;

	act= findnearestvert(sel-2);
	if(act==0) {
		error(" Nothing indicated ");
		return;
	}

	/* testflaggen wissen */
	eve= G.edve.first;
	while(eve) {
		eve->f&= ~2;
		eve= eve->next;
	}
	act->f= (act->f & ~3) | sel;

	while(flag==1) {
		flag= 0;
		toggle++;
		if(toggle & 1) eed= G.eded.first;
		else eed= G.eded.last;
		while(eed) {
			v1= eed->v1;
			v2= eed->v2;
			if(eed->h==0) {
				if(v1->f & 2) {
					if( (v2->f & 2)==0 ) {
						v2->f= (v2->f & ~3) | sel;
						flag= 1;
					}
				}
				else if(v2->f & 2) {
					if( (v1->f & 2)==0 ) {
						v1->f= (v1->f & ~3) | sel;
						flag= 1;
					}
				}
			}
			if(toggle & 1) eed= eed->next;
			else eed= eed->prev;
		}
	}
	countall();
	
	tekenvertices_ext( sel==3 );
}


short extrudeflag(short flag,short type)
{
	/* als type=1 worden oude extrudevlakken verwijderd (ivm spin etc) */
	/* alle verts met (flag & 'flag') extrude */
	/* van oude wordt flag 'flag' gewist, van nieuwe gezet */

	EditVert *eve, *v1, *v2, *v3, *v4, *nextve;
	EditEdge *eed, *e1, *e2, *e3, *e4, *nexted;
	EditVlak *evl, *nextvl;
	short sel=0, deloud= 0;

	if(G.obedit==0 || get_mesh(G.obedit)==0) return 0;

	/* de vert flag f1 wissen, hiermee test op losse geselecteerde vert */
	eve= G.edve.first;
	while(eve) {
		if(eve->f & flag) eve->f1= 1;
		else eve->f1= 0;
		eve= eve->next;
	}
	/* de edges tellerflag wissen, als selected op 1 zetten */
	eed= G.eded.first;
	while(eed) {
		if( (eed->v1->f & flag) && (eed->v2->f & flag) ) {
			eed->f= 1;
			eed->v1->f1= 0;
			eed->v2->f1= 0;
		}
		else eed->f= 0;
		
		eed->f1= 1;		/* aangeven is 'oude' edge (er worden in deze routine nieuwe gemaakt */
		
		eed= eed->next;
	}


	/* in alle vlak sel een dupl.flag zetten en bijhorende edgeflags ophogen */

	evl= G.edvl.first;
	while(evl) {
		evl->f= 0;
		
		if(vlakselectedAND(evl, flag)) {
			e1= evl->e1;
			e2= evl->e2;
			e3= evl->e3;
			e4= evl->e4;

			if(e1->f < 3) e1->f++;
			if(e2->f < 3) e2->f++;
			if(e3->f < 3) e3->f++;
			if(e4 && e4->f < 3) e4->f++;
			evl->f= 1;
		}
		else if(vlakselectedOR(evl, flag)) {
			e1= evl->e1;
			e2= evl->e2;
			e3= evl->e3;
			e4= evl->e4;
			
			if( (e1->v1->f & flag) && (e1->v2->f & flag) ) e1->f1= 2;
			if( (e2->v1->f & flag) && (e2->v2->f & flag) ) e2->f1= 2;
			if( (e3->v1->f & flag) && (e3->v2->f & flag) ) e3->f1= 2;
			if( e4 && (e4->v1->f & flag) && (e4->v2->f & flag) ) e4->f1= 2;
		}
		
		evl= evl->next;
	}

	/* set direction of edges */
	evl= G.edvl.first;
	while(evl) {
		if(evl->f== 0) {
			if(evl->e1->f==2) {
				if(evl->e1->v1 == evl->v1) evl->e1->dir= 0;
				else evl->e1->dir= 1;
			}
			if(evl->e2->f==2) {
				if(evl->e2->v1 == evl->v2) evl->e2->dir= 0;
				else evl->e2->dir= 1;
			}
			if(evl->e3->f==2) {
				if(evl->e3->v1 == evl->v3) evl->e3->dir= 0;
				else evl->e3->dir= 1;
			}
			if(evl->e4 && evl->e4->f==2) {
				if(evl->e4->v1 == evl->v4) evl->e4->dir= 0;
				else evl->e4->dir= 1;
			}
		}
		evl= evl->next;
	}	


	/* de stand van zaken nu:
		eve->f1==1: losse selected vertex 

		eed->f==0 : edge niet selected, geen extrude
		eed->f==1 : edge selected, komt niet in vlak voor, extrude
		eed->f==2 : edge selected, komt 1 keer in vlak voor, extrude
		eed->f==3 : edge selected, komt in meer vlakken voor, geen extrude
		
		eed->f1==0: nieuwe edge
		eed->f1==1: edge selected,  komt in selected vlak voor,  als f==3: remove
		eed->f1==2: edge selected, komt in NIET selected vlak voor
					
					
		evl->f==1 : vlak dupliceren
	*/

	/* alle geselecteerde vertices kopieeren, */
	/* de pointer naar nieuwe vert in oude struct schrijven op eve->vn */
	eve= G.edve.last;
	while(eve) {
		eve->f&= ~128;  /* wissen voor test later op losse verts */
		if(eve->f & flag) {
			sel= 1;
			v1= addvertlist(0);
			
			VECCOPY(v1->co, eve->co);
			v1->f= eve->f;
			eve->f-= flag;
			eve->vn= v1;
		}
		else eve->vn= 0;
		eve= eve->prev;
	}

	if(sel==0) return 0;

	/* alle edges met eed->f==1 of eed->f==2 worden vlakken */
	/* als deloud==1 worden edges eed->f>2 verwijderd */
	eed= G.eded.last;
	while(eed) {
		nexted= eed->prev;
		if( eed->f<3) {
			eed->v1->f|=128;  /* =geen losse vert! */
			eed->v2->f|=128;
		}
		if( (eed->f==1 || eed->f==2) ) {
			if(eed->f1==2) deloud=1;
			
			/* that dir thing does work somewhat... */
			
			if(eed->dir==1) addvlaklist(eed->v1, eed->v2, eed->v2->vn, eed->v1->vn, NULL);
			else addvlaklist(eed->v2, eed->v1, eed->v1->vn, eed->v2->vn, NULL);
		}

		eed= nexted;
	}
	if(deloud) {
		eed= G.eded.first;
		while(eed) {
			nexted= eed->next;
			if(eed->f==3 && eed->f1==1) {
				remedge(eed);
				free(eed);
			}
			eed= nexted;
		}
	}
	/* de vlakken dupliceren, eventueel oude verwijderen  */
	evl= G.edvl.first;
	while(evl) {
		nextvl= evl->next;
		if(evl->f & 1) {
		
			v1= evl->v1->vn;
			v2= evl->v2->vn;
			v3= evl->v3->vn;
			if(evl->v4) v4= evl->v4->vn; else v4= 0;
			
			addvlaklist(v1, v2, v3, v4, evl);
			
			if(deloud) {
				BLI_remlink(&G.edvl, evl);
				freevlak(evl);
			}
			
		}
		evl= nextvl;
	}
	/* alle verts met eve->vn!=0 
		als eve->f1==1: edge maken
		als flag!=128 :als deloud==1:  verwijderen
	*/
	eve= G.edve.last;
	while(eve) {
		nextve= eve->prev;
		if(eve->vn) {
			if(eve->f1==1) addedgelist(eve,eve->vn);
			else if( (eve->f & 128)==0) {
				if(deloud) {
					BLI_remlink(&G.edve,eve);
//					free(eve);
					free_editvert(eve);
					eve= NULL;
				}
			}
		}
		if(eve) eve->f&= ~128;
		
		eve= nextve;
	}

	/* debug temp: testen op consistente:
	evl= G.edvl.first;
	while(evl) {
		e1= findedgelist(evl->v1, evl->v2);
		e2= findedgelist(evl->v2, evl->v3);
		e3= findedgelist(evl->v3, evl->v1);
		if(e1==0 || e2==0 || e3==0) {
			error("edge not in edgelist");
			break;
		} 
		evl= evl->next;
	}
	*/

	return 1;
}

void rotateflag(short flag, float *cent, float rotmat[][3])
{
	/* alle verts met (flag & 'flag') rotate */

	EditVert *eve;

	eve= G.edve.first;
	while(eve) {
		if(eve->f & flag) {
			eve->co[0]-=cent[0];
			eve->co[1]-=cent[1];
			eve->co[2]-=cent[2];
			Mat3MulVecfl(rotmat,eve->co);
			eve->co[0]+=cent[0];
			eve->co[1]+=cent[1];
			eve->co[2]+=cent[2];
		}
		eve= eve->next;
	}
}

void translateflag(short flag, float *vec)
{
	/* alle verts met (flag & 'flag') translate */

	EditVert *eve;

	eve= G.edve.first;
	while(eve) {
		if(eve->f & flag) {
			eve->co[0]+=vec[0];
			eve->co[1]+=vec[1];
			eve->co[2]+=vec[2];
		}
		eve= eve->next;
	}
}

short removedoublesflag(short flag, float limit)		/* return aantal */
{
	/* alle verts met (flag & 'flag') worden getest */
	EditVert *eve, *v1, *nextve;
	EditEdge *eed, *e1, *nexted;
	EditVlak *evl, *nextvl;
	struct xvertsort *sortblock, *sb, *sb1;
	struct vlaksort *vlsortblock, *vsb, *vsb1;
	float dist;
	int a, b, test, aantal;

	/* flag 128 wordt gewist, aantal tellen */
	eve= G.edve.first;
	aantal= 0;
	while(eve) {
		eve->f&= ~128;
		if(eve->f & flag) aantal++;
		eve= eve->next;
	}
	if(aantal==0) return 0;

	/* geheugen reserveren en qsorten */
	sb= sortblock= (struct xvertsort *)MEM_mallocN(sizeof(struct xvertsort)*aantal,"sortremovedoub");
	eve= G.edve.first;
	while(eve) {
		if(eve->f & flag) {
			sb->x= eve->co[0]+eve->co[1]+eve->co[2];
			sb->v1= eve;
			sb++;
		}
		eve= eve->next;
	}
	qsort(sortblock, aantal, sizeof(struct xvertsort), vergxco);

	/* testen op doubles */
	sb= sortblock;
	for(a=0; a<aantal; a++) {
		eve= sb->v1;
		if( (eve->f & 128)==0 ) {
			sb1= sb+1;
			for(b=a+1; b<aantal; b++) {
				/* eerste test: simpel dist */
				dist= sb1->x - sb->x;
				if(dist > limit) break;
				
				/* tweede test: is vertex toegestaan */
				v1= sb1->v1;
				if( (v1->f & 128)==0 ) {
					
					dist= fabs(v1->co[0]-eve->co[0]);
					if(dist<=limit) {
						dist= fabs(v1->co[1]-eve->co[1]);
						if(dist<=limit) {
							dist= fabs(v1->co[2]-eve->co[2]);
							if(dist<=limit) {
								v1->f|= 128;
								v1->vn= eve;
							}
						}
					}
				}
				sb1++;
			}
		}
		sb++;
	}
	MEM_freeN(sortblock);

	/* edges testen en opnieuw invoegen */
	eed= G.eded.first;
	while(eed) {
		eed->f= 0;
		eed= eed->next;
	}
	eed= G.eded.last;
	while(eed) {
		nexted= eed->prev;

		if(eed->f==0) {
			if( (eed->v1->f & 128) || (eed->v2->f & 128) ) {
				remedge(eed);

				if(eed->v1->f & 128) eed->v1= eed->v1->vn;
				if(eed->v2->f & 128) eed->v2= eed->v2->vn;

				e1= addedgelist(eed->v1,eed->v2);
				
				if(e1) e1->f= 1;
				if(e1!=eed) free(eed);
			}
		}
		eed= nexted;
	}

	/* eerst aantal testvlakken tellen */
	evl= (struct EditVlak *)G.edvl.first;
	aantal= 0;
	while(evl) {
		evl->f= 0;
		if(evl->v1->f & 128) evl->f= 1;
		else if(evl->v2->f & 128) evl->f= 1;
		else if(evl->v3->f & 128) evl->f= 1;
		else if(evl->v4 && (evl->v4->f & 128)) evl->f= 1;
		
		if(evl->f==1) aantal++;
		evl= evl->next;
	}

	/* vlakken testen op dubbele punten en eventueel verwijderen */
	evl= (struct EditVlak *)G.edvl.first;
	while(evl) {
		nextvl= evl->next;
		if(evl->f==1) {
			
			if(evl->v1->f & 128) evl->v1= evl->v1->vn;
			if(evl->v2->f & 128) evl->v2= evl->v2->vn;
			if(evl->v3->f & 128) evl->v3= evl->v3->vn;
			if(evl->v4 && (evl->v4->f & 128)) evl->v4= evl->v4->vn;
		
			test= 0;
			if(evl->v1==evl->v2) test+=1;
			if(evl->v2==evl->v3) test+=2;
			if(evl->v3==evl->v1) test+=4;
			if(evl->v4==evl->v1) test+=8;
			if(evl->v3==evl->v4) test+=16;
			if(evl->v2==evl->v4) test+=32;
			
			if(test) {
				if(evl->v4) {
					if(test==1 || test==2) {
						evl->v2= evl->v3;
						evl->v3= evl->v4;
						evl->v4= 0;
						test= 0;
					}
					else if(test==8 || test==16) {
						evl->v4= 0;
						test= 0;
					}
					else {
						BLI_remlink(&G.edvl, evl);
						freevlak(evl);
						aantal--;
					}
				}
				else {
					BLI_remlink(&G.edvl, evl);
					freevlak(evl);
					aantal--;
				}
			}
			
			if(test==0) {
				/* edgepointers goedzetten */
				evl->e1= findedgelist(evl->v1, evl->v2);
				evl->e2= findedgelist(evl->v2, evl->v3);
				if(evl->v4==0) {
					evl->e3= findedgelist(evl->v3, evl->v1);
					evl->e4= 0;
				}
				else {
					evl->e3= findedgelist(evl->v3, evl->v4);
					evl->e4= findedgelist(evl->v4, evl->v1);
				}
			}
		}
		evl= nextvl;
	}

	/* dubbele vlakken: sortblock */
	/* opnieuw tellen, nu alle selected vlakken */
	aantal= 0;
	evl= G.edvl.first;
	while(evl) {
		evl->f= 0;
		if(vlakselectedAND(evl, 1)) {
			evl->f= 1;
			aantal++;
		}
		evl= evl->next;
	}

	if(aantal) {
		/* dubbele vlakken: sortblock */
		vsb= vlsortblock= MEM_mallocN(sizeof(struct vlaksort)*aantal, "sortremovedoub");
		evl= G.edvl.first;
		while(evl) {
			if(evl->f & 1) {
				if(evl->v4) vsb->x= (long) MIN4( (long)evl->v1, (long)evl->v2, (long)evl->v3, (long)evl->v4);
				else vsb->x= (long) MIN3( (long)evl->v1, (long)evl->v2, (long)evl->v3);

				vsb->evl= evl;
				vsb++;
			}
			evl= evl->next;
		}
		
		qsort(vlsortblock, aantal, sizeof(struct vlaksort), vergvlak);
			
		vsb= vlsortblock;
		for(a=0; a<aantal; a++) {
			evl= vsb->evl;
			if( (evl->f & 128)==0 ) {
				vsb1= vsb+1;

				for(b=a+1; b<aantal; b++) {
				
					/* eerste test: zelfde poin? */
					if(vsb->x != vsb1->x) break;
					
					/* tweede test: is test toegestaan */
					evl= vsb1->evl;
					if( (evl->f & 128)==0 ) {
						if( comparevlak(evl, vsb->evl)) evl->f |= 128;
						
					}
					vsb1++;
				}
			}
			vsb++;
		}
		
		MEM_freeN(vlsortblock);
		
		/* dubbele vlakken eruit */
		evl= (struct EditVlak *)G.edvl.first;
		while(evl) {
			nextvl= evl->next;
			if(evl->f & 128) {
				BLI_remlink(&G.edvl, evl);
				freevlak(evl);
			}
			evl= nextvl;
		}
	}
	
	/* dubbele vertices eruit */
	a= 0;
	eve= (struct EditVert *)G.edve.first;
	while(eve) {
		nextve= eve->next;
		if(eve->f & flag) {
			if(eve->f & 128) {
				a++;
				BLI_remlink(&G.edve, eve);
				
//				free(eve);
				free_editvert(eve);
			}
		}
		eve= nextve;
	}
	return a;	/* aantal */
}

void xsortvert_flag(int flag)
//short flag;
{
	/* alle verts met (flag & 'flag') worden gesorteerd */
	EditVert *eve;
	struct xvertsort *sortblock, *sb;
	ListBase tbase;
	int aantal;
	
	/* aantal tellen */
	eve= G.edve.first;
	aantal= 0;
	while(eve) {
		if(eve->f & flag) aantal++;
		eve= eve->next;
	}
	if(aantal==0) return;

	/* geheugen reserveren en qsorten */
	sb= sortblock= (struct xvertsort *)MEM_mallocN(sizeof(struct xvertsort)*aantal,"sortremovedoub");
	eve= G.edve.first;
	while(eve) {
		if(eve->f & flag) {
			sb->x= eve->xs;
			sb->v1= eve;
			sb++;
		}
		eve= eve->next;
	}
	qsort(sortblock, aantal, sizeof(struct xvertsort), vergxco);
	
	/* tijdelijke listbase maken */
	tbase.first= tbase.last= 0;
	sb= sortblock;
	while(aantal--) {
		eve= sb->v1;
		BLI_remlink(&G.edve, eve);
		BLI_addtail(&tbase, eve);
		sb++;
	}
	
	addlisttolist(&G.edve, &tbase);
	
	MEM_freeN(sortblock);
}


void hashvert_flag(int flag)
{
	EditVert *eve;
	struct xvertsort *sortblock, *sb, onth, *newsort;
	ListBase tbase;
	int aantal, a, b;
	
	/* aantal tellen */
	eve= G.edve.first;
	aantal= 0;
	while(eve) {
		if(eve->f & flag) aantal++;
		eve= eve->next;
	}
	if(aantal==0) return;

	/* geheugen reserveren */
	sb= sortblock= (struct xvertsort *)MEM_mallocN(sizeof(struct xvertsort)*aantal,"sortremovedoub");
	eve= G.edve.first;
	while(eve) {
		if(eve->f & flag) {
			sb->v1= eve;
			sb++;
		}
		eve= eve->next;
	}

	BLI_srand(1);
	
	sb= sortblock;
	for(a=0; a<aantal; a++, sb++) {
		b= aantal*BLI_drand();
		if(b>=0 && b<aantal) {
			newsort= sortblock+b;
			onth= *sb;
			*sb= *newsort;
			*newsort= onth;
		}
	}

	/* tijdelijke listbase maken */
	tbase.first= tbase.last= 0;
	sb= sortblock;
	while(aantal--) {
		eve= sb->v1;
		BLI_remlink(&G.edve, eve);
		BLI_addtail(&tbase, eve);
		sb++;
	}
	
	addlisttolist(&G.edve, &tbase);
	
	MEM_freeN(sortblock);
}

static unsigned int cpack_half(unsigned int col1, unsigned int col2)
{
	char *cp1, *cp2, *cp;
	unsigned int col=0;
	
	cp1= (char *)&col1;
	cp2= (char *)&col2;
	cp=  (char *)&col;
	
	cp[0]= (cp1[0]+cp2[0])>>1;
	cp[1]= (cp1[1]+cp2[1])>>1;
	cp[2]= (cp1[2]+cp2[2])>>1;
	cp[3]= (cp1[3]+cp2[3])>>1;
	
	return col;
}


static void uv_half(float *uv, float *uv1, float *uv2)
{
	uv[0]= (uv1[0]+uv2[0])/2.0;
	uv[1]= (uv1[1]+uv2[1])/2.0;
	
}

static void uv_quart(float *uv, float *uv1)
{
	uv[0]= (uv1[0]+uv1[2]+uv1[4]+uv1[6])/4.0;
	uv[1]= (uv1[1]+uv1[3]+uv1[5]+uv1[7])/4.0;
}

static void set_wuv(int tot, EditVlak *evl, int v1, int v2, int v3, int v4)
{
	/* deze vreemde fie alleen bij de subdiv te gebruiken, de 'w' in de naam slaat nergens op! */
	float *uv, uvo[4][2];
	unsigned int *col, colo[4], col1, col2;
	int a, v;
	
	memcpy(uvo, evl->uv, sizeof(uvo));
	uv= evl->uv[0];

	memcpy(colo, evl->col, sizeof(colo));
	col= evl->col;
	
	if(tot==4) {
		for(a=0; a<4; a++, uv+=2, col++) {
			if(a==0) v= v1;
			else if(a==1) v= v2;
			else if(a==2) v= v3;
			else v= v4;
			
			if(a==3 && v4==0) break;
			
			if(v<=4) {
				uv[0]= uvo[v-1][0];
				uv[1]= uvo[v-1][1];
				*col= colo[v-1];
			}
			else if(v==8) {
				uv_half(uv, uvo[3], uvo[0]);
				*col= cpack_half(colo[3], colo[0]);
			}
			else if(v==9) {
				uv_quart(uv, uvo[0]);
				col1= cpack_half(colo[1], colo[0]);
				col2= cpack_half(colo[2], colo[3]);
				*col= cpack_half(col1, col2);
			}
			else {
				uv_half(uv, uvo[v-5], uvo[v-4]);
				*col= cpack_half(colo[v-5], colo[v-4]);
			}
		}
	}
	else {
		for(a=0; a<3; a++, uv+=2, col++) {
			if(a==0) v= v1;
			else if(a==1) v= v2;
			else v= v3;
		
			if(v<=4) {
				uv[0]= uvo[v-1][0];
				uv[1]= uvo[v-1][1];
				*col= colo[v-1];
			}
			else if(v==7) {
				uv_half(uv, uvo[2], uvo[0]);
				*col= cpack_half(colo[2], colo[0]);
			}
			else {
				uv_half(uv, uvo[v-5], uvo[v-4]);
				*col= cpack_half(colo[v-5], colo[v-4]);
			}
		}
	}
}

static EditVert *vert_from_number(EditVlak *evl, int nr)
{
	switch(nr) {
	case 0:
		return 0;
	case 1:
		return evl->v1;
	case 2:
		return evl->v2;
	case 3:
		return evl->v3;
	case 4:
		return evl->v4;
	case 5:
		return evl->e1->vn;
	case 6:
		return evl->e2->vn;
	case 7:
		return evl->e3->vn;
	case 8:
		return evl->e4->vn;
	}
	
	return NULL;
}

static void addvlak_subdiv(EditVlak *evl, int val1, int val2, int val3, int val4, EditVert *eve)
{
	EditVlak *w;
	EditVert *v1, *v2, *v3, *v4;
	
	if(val1==9) v1= eve;
	else v1= vert_from_number(evl, val1);
	
	if(val2==9) v2= eve;
	else v2= vert_from_number(evl, val2);

	if(val3==9) v3= eve;
	else v3= vert_from_number(evl, val3);

	if(val4==9) v4= eve;
	else v4= vert_from_number(evl, val4);
	
	w= addvlaklist(v1, v2, v3, v4, evl);

	if(w) {
		if(evl->v4) set_wuv(4, w, val1, val2, val3, val4);
		else set_wuv(3, w, val1, val2, val3, val4);
	}
}

static float smoothperc= 0.0;

static void smooth_subdiv_vec(float *v1, float *v2, float *n1, float *n2, float *vec)
{
	float len, fac, nor[3], nor1[3], nor2[3];
	
	VecSubf(nor, v1, v2);
	len= 0.5*Normalise(nor);

	VECCOPY(nor1, n1);
	VECCOPY(nor2, n2);

	/* cosine angle */
	fac= nor[0]*nor1[0] + nor[1]*nor1[1] + nor[2]*nor1[2] ;
	
	vec[0]= fac*nor1[0];
	vec[1]= fac*nor1[1];
	vec[2]= fac*nor1[2];

	/* cosine angle */
	fac= -nor[0]*nor2[0] - nor[1]*nor2[1] - nor[2]*nor2[2] ;
	
	vec[0]+= fac*nor2[0];
	vec[1]+= fac*nor2[1];
	vec[2]+= fac*nor2[2];

	vec[0]*= smoothperc*len;
	vec[1]*= smoothperc*len;
	vec[2]*= smoothperc*len;
}

static void smooth_subdiv_quad(EditVlak *evl, float *vec)
{
	
	float nor1[3], nor2[3];
	float vec1[3], vec2[3];
	float cent[3];
	
	/* vlr->e1->vn is new vertex inbetween v1 / v2 */
	
	VecMidf(nor1, evl->v1->no, evl->v2->no);
	Normalise(nor1);
	VecMidf(nor2, evl->v3->no, evl->v4->no);
	Normalise(nor2);

	smooth_subdiv_vec( evl->e1->vn->co, evl->e3->vn->co, nor1, nor2, vec1);

	VecMidf(nor1, evl->v2->no, evl->v3->no);
	Normalise(nor1);
	VecMidf(nor2, evl->v4->no, evl->v1->no);
	Normalise(nor2);

	smooth_subdiv_vec( evl->e2->vn->co, evl->e4->vn->co, nor1, nor2, vec2);

	VecAddf(vec1, vec1, vec2);

	CalcCent4f(cent, evl->v1->co,  evl->v2->co,  evl->v3->co,  evl->v4->co);
	VecAddf(vec, cent, vec1);
}

void subdivideflag(int flag, float rad, int beauty)
{
	/* divide alle vlakken met (vertflag & flag) */
	/* als rad>0.0 zet dan nieuw vert op afstand rad van 0,0,0 */
	extern float doublimit;
	EditVert *eve;
	EditEdge *eed, *e1, *e2, *e3, *e4, *nexted;
	EditVlak *evl;
	float fac, vec[3], vec1[3], len1, len2, len3;
	short test;
	
	if(beauty & B_SMOOTH) {
		short perc= 100;

		if(button(&perc, 10, 500, "Percentage:")==0) return;
		
		smoothperc= 0.292*perc/100.0;
	}

	/* edgeflags */
	eed= G.eded.first;
	while(eed) {
		
		if( (eed->v1->f & flag) && (eed->v2->f & flag) ) eed->f= flag;
		else eed->f= 0;
		
		eed= eed->next;
	}
	
	/* als beauty: opp testen en edgeflags wissen van 'lelijke' edges */
	if(beauty & B_BEAUTY) {
		evl= G.edvl.first;
		while(evl) {
			if( vlakselectedAND(evl, flag) ) {
				if(evl->v4) {
				
					/* opp */
					len1= AreaQ3Dfl(evl->v1->co, evl->v2->co, evl->v3->co, evl->v4->co);
					if(len1 <= doublimit) {
						evl->e1->f = 0;
						evl->e2->f = 0;
						evl->e3->f = 0;
						evl->e4->f = 0;
					}
					else {
						len1= VecLenf(evl->v1->co, evl->v2->co) + VecLenf(evl->v3->co, evl->v4->co);
						len2= VecLenf(evl->v2->co, evl->v3->co) + VecLenf(evl->v1->co, evl->v4->co);
						
						if(len1 < len2) {
							evl->e1->f = 0;
							evl->e3->f = 0;
						}
						else if(len1 > len2) {
							evl->e2->f = 0;
							evl->e4->f = 0;
						}
					}
				}
				else {

					/* opp */
					len1= AreaT3Dfl(evl->v1->co, evl->v2->co, evl->v3->co);
					if(len1 <= doublimit) {
						evl->e1->f = 0;
						evl->e2->f = 0;
						evl->e3->f = 0;
					}
					else {
	
						len1= VecLenf(evl->v1->co, evl->v2->co) ;
						len2= VecLenf(evl->v2->co, evl->v3->co) ;
						len3= VecLenf(evl->v3->co, evl->v1->co) ;
						
						if(len1<len2 && len1<len3) {
							evl->e1->f = 0;
						}
						else if(len2<len3 && len2<len1) {
							evl->e2->f = 0;
						}
						else if(len3<len2 && len3<len1) {
							evl->e3->f = 0;
						}
					}
				}
			}
			evl= evl->next;
		}
	}

	if(beauty & B_SMOOTH) {
		
		vertexnormals(0);		/* no1*/
			
	}
	
	/* nieuw punt maken en in edge wegschrijven, flag wissen! is voor vlakkenmaak stuk nodig */
	eed= G.eded.first;
	while(eed) {
		if(eed->f & flag) {
			
			vec[0]= (eed->v1->co[0]+eed->v2->co[0])/2.0;
			vec[1]= (eed->v1->co[1]+eed->v2->co[1])/2.0;
			vec[2]= (eed->v1->co[2]+eed->v2->co[2])/2.0;

			if(rad > 0.0) {   /* perf sph */
				Normalise(vec);
				vec[0]*= rad;
				vec[1]*= rad;
				vec[2]*= rad;
			}
			else if(rad< 0.0) {  /* fract */
				fac= rad* VecLenf(eed->v1->co, eed->v2->co);
				vec1[0]= fac*BLI_drand();
				vec1[1]= fac*BLI_drand();
				vec1[2]= fac*BLI_drand();
				VecAddf(vec, vec, vec1);
			}
			
			if(beauty & B_SMOOTH) {
				smooth_subdiv_vec(eed->v1->co, eed->v2->co, eed->v1->no, eed->v2->no, vec1);
				VecAddf(vec, vec, vec1);
			}
			
			eed->vn= addvertlist(vec);
			eed->vn->f= eed->v1->f;

		}
		else eed->vn= 0;
		
		eed->f= 0; /* moet! */
		
		eed= eed->next;
	}

	/* alle vlakken testen op subdiv edges, 8 of 16 gevallen! */

	evl= G.edvl.last;
	while(evl) {
		if( vlakselectedOR(evl, flag) ) {
			e1= evl->e1;
			e2= evl->e2;
			e3= evl->e3;
			e4= evl->e4;

			test= 0;
			if(e1 && e1->vn) { 
				test+= 1; 
				e1->f= 1;
			}
			if(e2 && e2->vn) { 
				test+= 2; 
				e2->f= 1;
			}
			if(e3 && e3->vn) { 
				test+= 4; 
				e3->f= 1;
			}
			if(e4 && e4->vn) { 
				test+= 8; 
				e4->f= 1;
			}
			
			if(test) {
				if(evl->v4==0) {
					if((test & 3)==3) addvlak_subdiv(evl, 2, 2+4, 1+4, 0, 0);
					if((test & 6)==6) addvlak_subdiv(evl, 3, 3+4, 2+4, 0, 0);
					if((test & 5)==5) addvlak_subdiv(evl, 1, 1+4, 3+4, 0, 0);

					if(test==7) {  /* vier nieuwe vlakken, oude vernieuwt */
						evl->v1= e1->vn;
						evl->v2= e2->vn;
						evl->v3= e3->vn;
						set_wuv(3, evl, 1+4, 2+4, 3+4, 0);
					}
					else if(test==3) {
						addvlak_subdiv(evl, 1+4, 2+4, 3, 0, 0);
						evl->v2= e1->vn;
						set_wuv(3, evl, 1, 1+4, 3, 0);
					}
					else if(test==6) {
						addvlak_subdiv(evl, 2+4, 3+4, 1, 0, 0);
						evl->v3= e2->vn;
						set_wuv(3, evl, 1, 2, 2+4, 0);
					}
					else if(test==5) {
						addvlak_subdiv(evl, 3+4, 1+4, 2, 0, 0);
						evl->v1= e3->vn;
						set_wuv(3, evl, 3+4, 2, 3, 0);
					}
					else if(test==1) {
						addvlak_subdiv(evl, 1+4, 2, 3, 0, 0);
						evl->v2= e1->vn;
						set_wuv(3, evl, 1, 1+4, 3, 0);
					}
					else if(test==2) {
						addvlak_subdiv(evl, 2+4, 3, 1, 0, 0);
						evl->v3= e2->vn;
						set_wuv(3, evl, 1, 2, 2+4, 0);
					}
					else if(test==4) {
						addvlak_subdiv(evl, 3+4, 1, 2, 0, 0);
						evl->v1= e3->vn;
						set_wuv(3, evl, 3+4, 2, 3, 0);
					}
					evl->e1= addedgelist(evl->v1, evl->v2);
					evl->e2= addedgelist(evl->v2, evl->v3);
					evl->e3= addedgelist(evl->v3, evl->v1);
					
				}
				else {
					if(test==15) {
						/* nog een nieuw punt toevoegen */
						CalcCent4f(vec, evl->v1->co, evl->v2->co, evl->v3->co, evl->v4->co);
						
						if(beauty & B_SMOOTH) {
							smooth_subdiv_quad(evl, vec);	/* adds */
						}
						eve= addvertlist(vec);
						
						eve->f |= flag;

						addvlak_subdiv(evl, 2, 2+4, 9, 1+4, eve);
						addvlak_subdiv(evl, 3, 3+4, 9, 2+4, eve);
						addvlak_subdiv(evl, 4, 4+4, 9, 3+4, eve);

						evl->v2= e1->vn;
						evl->v3= eve;
						evl->v4= e4->vn;
						set_wuv(4, evl, 1, 1+4, 9, 4+4);
					}
					else {
						/* kleine hoekpunten */
						if((test & 3)==3) addvlak_subdiv(evl, 1+4, 2, 2+4, 0, 0);
						if((test & 6)==6) addvlak_subdiv(evl, 2+4, 3, 3+4, 0, 0);
						if((test & 12)==12) addvlak_subdiv(evl, 3+4, 4, 4+4, 0, 0);
						if((test & 9)==9) addvlak_subdiv(evl, 4+4, 1, 1+4, 0, 0);
						
						if(test==1) {
							addvlak_subdiv(evl, 1+4, 2, 3, 0, 0);
							addvlak_subdiv(evl, 1+4, 3, 4, 0, 0);
							evl->v2= e1->vn;
							evl->v3= evl->v4;
							evl->v4= 0;
							set_wuv(4, evl, 1, 1+4, 4, 0);
						}
						else if(test==2) {
							addvlak_subdiv(evl, 2+4, 3, 4, 0, 0);
							addvlak_subdiv(evl, 2+4, 4, 1, 0, 0);
							evl->v3= e2->vn;
							evl->v4= 0;
							set_wuv(4, evl, 1, 2, 2+4, 0);
						}
						else if(test==4) {
							addvlak_subdiv(evl, 3+4, 4, 1, 0, 0);
							addvlak_subdiv(evl, 3+4, 1, 2, 0, 0);
							evl->v1= evl->v2;
							evl->v2= evl->v3;
							evl->v3= e3->vn;
							evl->v4= 0;
							set_wuv(4, evl, 2, 3, 3+4, 0);
						}
						else if(test==8) {
							addvlak_subdiv(evl, 4+4, 1, 2, 0, 0);
							addvlak_subdiv(evl, 4+4, 2, 3, 0, 0);
							evl->v1= evl->v3;
							evl->v2= evl->v4;
							evl->v3= e4->vn;
							evl->v4= 0;
							set_wuv(4, evl, 3, 4, 4+4, 0);
						}
						else if(test==3) {
							addvlak_subdiv(evl, 1+4, 2+4, 4, 0, 0);
							addvlak_subdiv(evl, 2+4, 3, 4, 0, 0);
							evl->v2= e1->vn;
							evl->v3= evl->v4;
							evl->v4= 0;
							set_wuv(4, evl, 1, 1+4, 4, 0);
						}
						else if(test==6) {
							addvlak_subdiv(evl, 2+4, 3+4, 1, 0, 0);
							addvlak_subdiv(evl, 3+4, 4, 1, 0, 0);
							evl->v3= e2->vn;
							evl->v4= 0;
							set_wuv(4, evl, 1, 2, 2+4, 0);
						}
						else if(test==12) {
							addvlak_subdiv(evl, 3+4, 4+4, 2, 0, 0);
							addvlak_subdiv(evl, 4+4, 1, 2, 0, 0);
							evl->v1= evl->v2;
							evl->v2= evl->v3;
							evl->v3= e3->vn;
							evl->v4= 0;
							set_wuv(4, evl, 2, 3, 3+4, 0);
						}
						else if(test==9) {
							addvlak_subdiv(evl, 4+4, 1+4, 3, 0, 0);
							addvlak_subdiv(evl, 1+4, 2, 3, 0, 0);
							evl->v1= evl->v3;
							evl->v2= evl->v4;
							evl->v3= e4->vn;
							evl->v4= 0;
							set_wuv(4, evl, 3, 4, 4+4, 0);
						}
						else if(test==5) {
							addvlak_subdiv(evl, 1+4, 2, 3, 3+4, 0);
							evl->v2= e1->vn;
							evl->v3= e3->vn;
							set_wuv(4, evl, 1, 1+4, 3+4, 4);
						}
						else if(test==10) {
							addvlak_subdiv(evl, 2+4, 3, 4, 4+4, 0);
							evl->v3= e2->vn;
							evl->v4= e4->vn;
							set_wuv(4, evl, 1, 2, 2+4, 4+4);
						}
						
						else if(test==7) {
							addvlak_subdiv(evl, 1+4, 2+4, 3+4, 0, 0);
							evl->v2= e1->vn;
							evl->v3= e3->vn;
							set_wuv(4, evl, 1, 1+4, 3+4, 4);
						}
						else if(test==14) {
							addvlak_subdiv(evl, 2+4, 3+4, 4+4, 0, 0);
							evl->v3= e2->vn;
							evl->v4= e4->vn;
							set_wuv(4, evl, 1, 2, 2+4, 4+4);
						}
						else if(test==13) {
							addvlak_subdiv(evl, 3+4, 4+4, 1+4, 0, 0);
							evl->v4= e3->vn;
							evl->v1= e1->vn;
							set_wuv(4, evl, 1+4, 3, 3, 3+4);
						}
						else if(test==11) {
							addvlak_subdiv(evl, 4+4, 1+4, 2+4, 0, 0);
							evl->v1= e4->vn;
							evl->v2= e2->vn;
							set_wuv(4, evl, 4+4, 2+4, 3, 4);
						}
					}
					evl->e1= addedgelist(evl->v1, evl->v2);
					evl->e2= addedgelist(evl->v2, evl->v3);
					if(evl->v4) evl->e3= addedgelist(evl->v3, evl->v4);
					else evl->e3= addedgelist(evl->v3, evl->v1);
					if(evl->v4) evl->e4= addedgelist(evl->v4, evl->v1);
					else evl->e4= 0;
				}
			}
		}
		evl= evl->prev;
	}

	/* alle oude edges verwijderen, eventueel nog nieuwe maken */
	eed= G.eded.first;
	while(eed) {
		nexted= eed->next;
		if( eed->vn ) {
			if(eed->f==0) {  /* niet gebruikt in vlak */
				addedgelist(eed->v1,eed->vn);
				addedgelist(eed->vn,eed->v2);
			}
			remedge(eed);
			free(eed);
		}
		eed= nexted;
	}
	countall();
	allqueue(REDRAWVIEW3D, 0);
	makeDispList(G.obedit);
}

void adduplicateflag(int flag)
{
	/* oude verts hebben flag 128 gezet en flag 'flag' gewist
	   nieuwe verts hebben flag 'flag' gezet */
	EditVert *eve, *v1, *v2, *v3, *v4;
	EditEdge *eed;
	EditVlak *evl;

	/* eerst vertices */
	eve= G.edve.last;
	while(eve) {
		eve->f&= ~128;
		if(eve->f & flag) {
			v1= addvertlist(eve->co);
			v1->f= eve->f;
			eve->f-= flag;
			eve->f|= 128;
			eve->vn= v1;
#ifdef __NLA
			/* >>>>> FIXME: Copy deformation weight ? */
			v1->totweight = eve->totweight;
			if (eve->totweight){
				v1->dw = MEM_mallocN (eve->totweight * sizeof(MDeformWeight), "deformWeight");
				memcpy (v1->dw, eve->dw, eve->totweight * sizeof(MDeformWeight));
			}
			else
				v1->dw=NULL;
#endif
		}
		eve= eve->prev;
	}
	eed= G.eded.first;
	while(eed) {
		if( (eed->v1->f & 128) && (eed->v2->f & 128) ) {
			v1= eed->v1->vn;
			v2= eed->v2->vn;
			addedgelist(v1,v2);
		}
		eed= eed->next;
	}

	/* tenslotte de vlakken dupliceren */
	evl= G.edvl.first;
	while(evl) {
		if( (evl->v1->f & 128) && (evl->v2->f & 128) && (evl->v3->f & 128) ) {
			if(evl->v4) {
				if(evl->v4->f & 128) {
					v1= evl->v1->vn;
					v2= evl->v2->vn;
					v3= evl->v3->vn;
					v4= evl->v4->vn;
					addvlaklist(v1, v2, v3, v4, evl);
				}
			}
			else {
				v1= evl->v1->vn;
				v2= evl->v2->vn;
				v3= evl->v3->vn;
				addvlaklist(v1, v2, v3, 0, evl);
			}
		}
		evl= evl->next;
	}
}

static void delvlakflag(int flag)
{
	/* alle vlak 3/4 verts flag + edges + losse vertices deleten */
	/* van alle verts wordt 'flag' gewist */
	EditVert *eve,*nextve;
	EditEdge *eed, *nexted;
	EditVlak *evl,*nextvl;

	eed= G.eded.first;
	while(eed) {
		eed->f= 0;
		eed= eed->next;
	}

	evl= G.edvl.first;
	while(evl) {
		nextvl= evl->next;
		if(vlakselectedAND(evl, flag)) {
			
			evl->e1->f= 1;
			evl->e2->f= 1;
			evl->e3->f= 1;
			if(evl->e4) {
				evl->e4->f= 1;
			}
								
			BLI_remlink(&G.edvl, evl);
			freevlak(evl);
		}
		evl= nextvl;
	}
	/* alle vlakken 1, 2 (3) verts select edges behouden */
	evl= G.edvl.first;
	while(evl) {
		evl->e1->f= 0;
		evl->e2->f= 0;
		evl->e3->f= 0;
		if(evl->e4) {
			evl->e4->f= 0;
		}

		evl= evl->next;
	}
	
	/* alle edges testen op vertices met flag en wissen */
	eed= G.eded.first;
	while(eed) {
		nexted= eed->next;
		if(eed->f==1) {
			remedge(eed);
			free(eed);
		}
		else if( (eed->v1->f & flag) || (eed->v2->f & flag) ) {
			eed->v1->f&= ~flag;
			eed->v2->f&= ~flag;
		}
		eed= nexted;
	}
	/* vertices met flag nog gezet zijn losse en worden verwijderd */
	eve= G.edve.first;
	while(eve) {
		nextve= eve->next;
		if(eve->f & flag) {
			BLI_remlink(&G.edve, eve);
//			free(eve);
			free_editvert(eve);
		}
		eve= nextve;
	}

}

void extrude_mesh(void)
{
	short a;

	TEST_EDITMESH

	if(okee("Extrude")==0) return;

	waitcursor(1);

	a= extrudeflag(1,1);
	waitcursor(0);
	if(a==0) {
		error("Can't extrude");
	}
	else {
		countall();  /* voor G.totvert in calc_meshverts() */
		calc_meshverts();
		transform('d');
	}

}

void adduplicate_mesh(void)
{

	TEST_EDITMESH

	waitcursor(1);
	adduplicateflag(1);
	waitcursor(0);

	countall();  /* voor G.totvert in calc_meshverts() */
	transform('d');
}

void split_mesh(void)
{

	TEST_EDITMESH

	if(okee(" Split ")==0) return;

	waitcursor(1);

	/* eerst duplicate maken */
	adduplicateflag(1);
	/* oude vlakken hebben 3x flag 128 gezet, deze deleten */
	delvlakflag(128);

	waitcursor(0);

	countall();
	allqueue(REDRAWVIEW3D, 0);
	makeDispList(G.obedit);
}

void separate_mesh(void)
{
	EditVert *eve, *v1;
	EditEdge *eed, *e1;
	EditVlak *evl, *vl1;
	Object *oldob;
	Mesh *me, *men;
	Base *base, *oldbase;
	ListBase edve, eded, edvl;
	float trans[9];
	int ok, flag;
	
	TEST_EDITMESH
	
	if(okee("Separate")==0) return;

	waitcursor(1);
	
	me= get_mesh(G.obedit);
	if(me->key) {
		error("Can't separate with vertex keys");
		return;
	}
	
	/* we gaan de zaak als volgt neppen:
	 * 1. duplicate object: dit wordt de nieuwe,  oude pointer onthouden
	 * 2: split doen als modig.
	 * 3. alle NIET geselecteerde verts, edges, vlakken apart zetten
	 * 4. loadobeditdata(): dit is de nieuwe ob
	 * 5. freelist en oude verts, eds, vlakken weer terughalen
	 */
	
	/* alleen obedit geselecteerd */
	base= FIRSTBASE;
	while(base) {
		if(base->lay & G.vd->lay) {
			if(base->object==G.obedit) base->flag |= SELECT;
			else base->flag &= ~SELECT;
		}
		base= base->next;
	}
	
	/* testen of split */
	ok= 0;
	eed= G.eded.first;
	while(eed) {
		flag= (eed->v1->f & 1)+(eed->v2->f & 1);
		if(flag==1) {
			ok= 1;
			break;
		}
		eed= eed->next;
	}
	if(ok) {
		/* SPLIT: eerst duplicate maken */
		adduplicateflag(1);
		/* SPLIT: oude vlakken hebben 3x flag 128 gezet, deze deleten */
		delvlakflag(128);
	}
	
	/* apart zetten: alles wat maar enigszins NIET select is */
	edve.first= edve.last= eded.first= eded.last= edvl.first= edvl.last= 0;
	eve= G.edve.first;
	while(eve) {
		v1= eve->next;
		if((eve->f & 1)==0) {
			BLI_remlink(&G.edve, eve);
			BLI_addtail(&edve, eve);
		}
		eve= v1;
	}
	eed= G.eded.first;
	while(eed) {
		e1= eed->next;
		if( (eed->v1->f & 1)==0 || (eed->v2->f & 1)==0 ) {
			BLI_remlink(&G.eded, eed);
			BLI_addtail(&eded, eed);
		}
		eed= e1;
	}
	evl= G.edvl.first;
	while(evl) {
		vl1= evl->next;
		if( (evl->v1->f & 1)==0 || (evl->v2->f & 1)==0 || (evl->v3->f & 1)==0 ) {
			BLI_remlink(&G.edvl, evl);
			BLI_addtail(&edvl, evl);
		}
		evl= vl1;
	}
	
	oldob= G.obedit;
	oldbase= BASACT;
	
	trans[0]=trans[1]=trans[2]=trans[3]=trans[4]=trans[5]= 0.0;
	trans[6]=trans[7]=trans[8]= 1.0;
	G.qual |= LR_ALTKEY;	/* patch om zeker te zijn van gelinkte dupli */
	adduplicate(trans);
	G.qual &= ~LR_ALTKEY;
	
	G.obedit= BASACT->object;	/* basact wordt in adduplicate() gezet */

	men= copy_mesh(me);
	set_mesh(G.obedit, men);
	/* omdat nieuwe mesh een kopie is: aantal users verlagen */
	men->id.us--;
	
	load_editMesh();
	
	BASACT->flag &= ~SELECT;
	
	makeDispList(G.obedit);
	free_editMesh();
	
	G.edve= edve;
	G.eded= eded;
	G.edvl= edvl;
	
	/* hashedges are freed now, make new! */
	eed= G.eded.first;
	while(eed) {
		if( findedgelist(eed->v1, eed->v2)==NULL )
			insert_hashedge(eed);
		eed= eed->next;
	}
	
	G.obedit= oldob;
	BASACT= oldbase;
	BASACT->flag |= SELECT;
	
	waitcursor(0);

	countall();
	allqueue(REDRAWVIEW3D, 0);
	makeDispList(G.obedit);

}

void extrude_repeat_mesh(int steps, float offs)
{
	float dvec[3], tmat[3][3], bmat[3][3];
/* 	float phi; */
	short a,ok;

	TEST_EDITMESH
	waitcursor(1);

	/* dvec */
	dvec[0]= G.vd->persinv[2][0];
	dvec[1]= G.vd->persinv[2][1];
	dvec[2]= G.vd->persinv[2][2];
	Normalise(dvec);
	dvec[0]*= offs;
	dvec[1]*= offs;
	dvec[2]*= offs;

	/* base correctie */
	Mat3CpyMat4(bmat, G.obedit->obmat);
	/* phi= ((struct ObData *)G.obedit->d)->vv->ws; */
	/* Mat3MulFloat(bmat, phi); */
	Mat3Inv(tmat, bmat);
	Mat3MulVecfl(tmat, dvec);

	for(a=0;a<steps;a++) {
		ok= extrudeflag(1,1);
		if(ok==0) {
			error("Can't extrude");
			break;
		}
		translateflag(1, dvec);
	}

	countall();
	allqueue(REDRAWVIEW3D, 0);
	makeDispList(G.obedit);
	waitcursor(0);
}

void spin_mesh(int steps,int degr,float *dvec, int mode)
{
	EditVert *eve,*nextve;
	float *curs, si,n[3],q[4],cmat[3][3],imat[3][3], tmat[3][3];
	float cent[3],bmat[3][3];
	float phi;
	short a,ok;

	TEST_EDITMESH
	
	waitcursor(1);

	/* imat en centrum en afmeting */
	Mat3CpyMat4(bmat, G.obedit->obmat);
	Mat3Inv(imat,bmat);

	curs= give_cursor();
	VECCOPY(cent, curs);
	cent[0]-= G.obedit->obmat[3][0];
	cent[1]-= G.obedit->obmat[3][1];
	cent[2]-= G.obedit->obmat[3][2];
	Mat3MulVecfl(imat, cent);

	phi= degr*M_PI/360.0;
	phi/= steps;
	if(editbutflag & B_CLOCKWISE) phi= -phi;

	if(dvec) {
		n[0]=n[1]= 0.0;
		n[2]= 1.0;
	} else {
		n[0]= G.vd->viewinv[2][0];
		n[1]= G.vd->viewinv[2][1];
		n[2]= G.vd->viewinv[2][2];
		Normalise(n);
	}

	q[0]= cos(phi);
	si= sin(phi);
	q[1]= n[0]*si;
	q[2]= n[1]*si;
	q[3]= n[2]*si;
	QuatToMat3(q, cmat);

	Mat3MulMat3(tmat,cmat,bmat);
	Mat3MulMat3(bmat,imat,tmat);

	if(mode==0) if(editbutflag & B_KEEPORIG) adduplicateflag(1);
	ok= 1;

	for(a=0;a<steps;a++) {
		if(mode==0) ok= extrudeflag(1,1);
		else adduplicateflag(1);
		if(ok==0) {
			error("Can't spin");
			break;
		}
		rotateflag(1, cent, bmat);
		if(dvec) {
			Mat3MulVecfl(bmat,dvec);
			translateflag(1,dvec);
		}
	}

	waitcursor(0);
	if(ok==0) {
		/* geen of alleen losse verts select, dups verwijderen */
		eve= G.edve.first;
		while(eve) {
			nextve= eve->next;
			if(eve->f & 1) {
				BLI_remlink(&G.edve,eve);
//				free(eve);
				free_editvert(eve);
			}
			eve= nextve;
		}
	}
	countall();
	recalc_editnormals();
	allqueue(REDRAWVIEW3D, 0);
	makeDispList(G.obedit);
}

void screw_mesh(int steps,int turns)
{
	EditVert *eve,*v1=0,*v2=0;
	EditEdge *eed;
	float dvec[3], nor[3];

	TEST_EDITMESH

	/* eerste voorwaarde: frontview! */
	if(G.vd->view!=1) {
		error("Only in frontview!");
		return;
	}

	/* flags wissen */
	eve= G.edve.first;
	while(eve) {
		eve->f1= 0;
		eve= eve->next;
	}
	/* edges zetten flags in verts */
	eed= G.eded.first;
	while(eed) {
		if(eed->v1->f & 1) {
			if(eed->v2->f & 1) {
				/* oppassen f1 is een byte */
				if(eed->v1->f1<2) eed->v1->f1++;
				if(eed->v2->f1<2) eed->v2->f1++;
			}
		}
		eed= eed->next;
	}
	/* vind twee vertices met eve->f1==1, meer of minder is fout */
	eve= G.edve.first;
	while(eve) {
		if(eve->f1==1) {
			if(v1==0) v1= eve;
			else if(v2==0) v2= eve;
			else {
				v1=0;
				break;
			}
		}
		eve= eve->next;
	}
	if(v1==0 || v2==0) {
		error("No curve selected");
		return;
	}

	/* bereken dvec */
	dvec[0]= ( (v1->co[0]- v2->co[0]) )/(steps);
	dvec[1]= ( (v1->co[1]- v2->co[1]) )/(steps);
	dvec[2]= ( (v1->co[2]- v2->co[2]) )/(steps);

	VECCOPY(nor, G.obedit->obmat[2]);

	if(nor[0]*dvec[0]+nor[1]*dvec[1]+nor[2]*dvec[2]>0.000) {
		dvec[0]= -dvec[0];
		dvec[1]= -dvec[1];
		dvec[2]= -dvec[2];
	}

	spin_mesh(turns*steps, turns*360, dvec, 0);

}

void selectswap_mesh(void)
{
	EditVert *eve;

	eve= G.edve.first;
	while(eve) {
		if(eve->h==0) {
			if(eve->f & 1) eve->f&= ~1;
			else eve->f|= 1;
		}
		eve= eve->next;
	}
	countall();
	allqueue(REDRAWVIEW3D, 0);

}

/* *******************************  ADD  ********************* */

void addvert_mesh(void)
{
	EditVert *eve,*v1=0;
	float *curs, mat[3][3],imat[3][3];

	TEST_EDITMESH

	Mat3CpyMat4(mat, G.obedit->obmat);
	Mat3Inv(imat, mat);

	v1= G.edve.first;
	while(v1) {
		if(v1->f & 1) break;
		v1= v1->next;
	}
	eve= v1;	/* voorkomen dat er nog meer select zijn */
	while(eve) {
		eve->f&= ~1;
		eve= eve->next;
	}

	eve= addvertlist(0);
	
	curs= give_cursor();
	VECCOPY(eve->co, curs);
	eve->xs= G.vd->mx;
	eve->ys= G.vd->my;
	VecSubf(eve->co, eve->co, G.obedit->obmat[3]);

	Mat3MulVecfl(imat, eve->co);
	eve->f= 1;

	if(v1) {
		addedgelist(v1, eve);
		v1->f= 0;
	}
	countall();
	allqueue(REDRAWVIEW3D, 0);
	makeDispList(G.obedit);
	
	while(get_mbut()&R_MOUSE);

}

void addedgevlak_mesh(void)
{
	EditVert *eve, *neweve[4];
	EditVlak *evl;
	float con1, con2, con3;
	short aantal=0;

	if( (G.vd->lay & G.obedit->lay)==0 ) return;

	/* hoeveel geselecteerd ? */
	eve= G.edve.first;
	while(eve) {
		if(eve->f & 1) {
			aantal++;
			if(aantal>4) break;			
			neweve[aantal-1]= eve;
		}
		eve= eve->next;
	}
	if(aantal==2) {
		addedgelist(neweve[0], neweve[1]);
		allqueue(REDRAWVIEW3D, 0);
		makeDispList(G.obedit);
		return;
	}
	if(aantal<2 || aantal>4) {
		error("Can't make edge/face");
		return;
	}

	evl= NULL; // check later

	if(aantal==3) {
		if(exist_vlak(neweve[0], neweve[1], neweve[2], 0)==0) {
			
			evl= addvlaklist(neweve[0], neweve[1], neweve[2], 0, NULL);

		}
		else error("Already a face");
	}
	else if(aantal==4) {
		if(exist_vlak(neweve[0], neweve[1], neweve[2], neweve[3])==0) {
		
			con1= convex(neweve[0]->co, neweve[1]->co, neweve[2]->co, neweve[3]->co);
			con2= convex(neweve[0]->co, neweve[2]->co, neweve[3]->co, neweve[1]->co);
			con3= convex(neweve[0]->co, neweve[3]->co, neweve[1]->co, neweve[2]->co);

			if(con1>=con2 && con1>=con3)
				evl= addvlaklist(neweve[0], neweve[1], neweve[2], neweve[3], NULL);
			else if(con2>=con1 && con2>=con3)
				evl= addvlaklist(neweve[0], neweve[2], neweve[3], neweve[1], NULL);
			else 
				evl= addvlaklist(neweve[0], neweve[2], neweve[1], neweve[3], NULL);

		}
		else error("Already a face");
	}
	
	if(evl) {	// now we're calculating direction of normal
		float inp;	
		/* dot product view mat with normal, should give info! */
	
		CalcNormFloat(evl->v1->co, evl->v2->co, evl->v3->co, evl->n);

		inp= evl->n[0]*G.vd->viewmat[0][2] + evl->n[1]*G.vd->viewmat[1][2] + evl->n[2]*G.vd->viewmat[2][2];

		if(inp < 0.0) flipvlak(evl);
	}
	
	countall();
	allqueue(REDRAWVIEW3D, 0);
	makeDispList(G.obedit);
}

static void erase_edges(ListBase *l)
{
	EditEdge *ed, *nexted;
	
	ed = (EditEdge *) l->first;
	while(ed) {
		nexted= ed->next;
		if( (ed->v1->f & 1) || (ed->v2->f & 1) ) {
			remedge(ed);
			free(ed);
		}
		ed= nexted;
	}
}

static void erase_faces(ListBase *l)
{
	EditVlak *f, *nextf;

	f = (EditVlak *) l->first;

	while(f) {
		nextf= f->next;
		if( vlakselectedOR(f, 1) ) {
			BLI_remlink(l, f);
			freevlak(f);
		}
		f = nextf;
	}
}	

static void erase_vertices(ListBase *l)
{
	EditVert *v, *nextv;

	v = (EditVert *) l->first;
	while(v) {
		nextv= v->next;
		if(v->f & 1) {
			BLI_remlink(l, v);
			free_editvert(v);
		}
		v = nextv;
	}
}

void delete_mesh(void)
{
	EditVlak *evl, *nextvl;
	EditVert *eve,*nextve;
	EditEdge *eed,*nexted;
	short event;
	int count;

	TEST_EDITMESH
	
	event= pupmenu("ERASE %t|Vertices%x10|Edges%x1|Faces%x2|All%x3|Edges & Faces%x4|Only Faces%x5");
	if(event<1) return;

	if(event==10 ) {
		erase_edges(&G.eded);
		erase_faces(&G.edvl);
		erase_vertices(&G.edve);
	} 
	else if(event==4) {
		evl= G.edvl.first;
		while(evl) {
			nextvl= evl->next;
			/* delete only faces with 2 or more vertices selected */
			count= 0;
			if(evl->v1->f & 1) count++;
			if(evl->v2->f & 1) count++;
			if(evl->v3->f & 1) count++;
			if(evl->v4 && (evl->v4->f & 1)) count++;
			if(count>1) {
				BLI_remlink(&G.edvl, evl);
				freevlak(evl);
			}
			evl= nextvl;
		}
		eed= G.eded.first;
		while(eed) {
			nexted= eed->next;
			if( (eed->v1->f & 1) && (eed->v2->f & 1) ) {
				remedge(eed);
				free(eed);
			}
			eed= nexted;
		}
		evl= G.edvl.first;
		while(evl) {
			nextvl= evl->next;
			event=0;
			if( evl->v1->f & 1) event++;
			if( evl->v2->f & 1) event++;
			if( evl->v3->f & 1) event++;
			if(evl->v4 && (evl->v4->f & 1)) event++;
			
			if(event>1) {
				BLI_remlink(&G.edvl, evl);
				freevlak(evl);
			}
			evl= nextvl;
		}
	} 
	else if(event==1) {
		eed= G.eded.first;
		while(eed) {
			nexted= eed->next;
			if( (eed->v1->f & 1) && (eed->v2->f & 1) ) {
				remedge(eed);
				free(eed);
			}
			eed= nexted;
		}
		evl= G.edvl.first;
		while(evl) {
			nextvl= evl->next;
			event=0;
			if( evl->v1->f & 1) event++;
			if( evl->v2->f & 1) event++;
			if( evl->v3->f & 1) event++;
			if(evl->v4 && (evl->v4->f & 1)) event++;
			
			if(event>1) {
				BLI_remlink(&G.edvl, evl);
				freevlak(evl);
			}
			evl= nextvl;
		}
		/* om losse vertices te wissen: */
		eed= G.eded.first;
		while(eed) {
			if( eed->v1->f & 1) eed->v1->f-=1;
			if( eed->v2->f & 1) eed->v2->f-=1;
			eed= eed->next;
		}
		eve= G.edve.first;
		while(eve) {
			nextve= eve->next;
			if(eve->f & 1) {
				BLI_remlink(&G.edve,eve);
//				free(eve);
				free_editvert(eve);
			}
			eve= nextve;
		}

	}
	else if(event==2) delvlakflag(1);
	else if(event==3) {
//		if(G.edve.first) BLI_freelist(&G.edve);
		if(G.edve.first) free_editverts(&G.edve);
		if(G.eded.first) BLI_freelist(&G.eded);
		if(G.edvl.first) freevlaklist(&G.edvl);
	}
	else if(event==5) {
		evl= G.edvl.first;
		while(evl) {
			nextvl= evl->next;
			if(vlakselectedAND(evl, 1)) {
				BLI_remlink(&G.edvl, evl);
				freevlak(evl);
			}
			evl= nextvl;
		}
	}

	countall();
	allqueue(REDRAWVIEW3D, 0);
	makeDispList(G.obedit);
}



void add_primitiveMesh(int type)
{
	Mesh *me;
	EditVert *eve, *v1=NULL, *v2, *v3, *v4=NULL, *vtop, *vdown;
	float *curs, d, dia, phi, phid, cent[3], vec[3], imat[3][3], mat[3][3];
	float q[4], cmat[3][3];
	static short tot=32, seg=32, subdiv=2;
	short a, b, ext=0, fill=0, totoud, newob=0;
	
	if(G.scene->id.lib) return;

	/* this function also comes from an info window */
	if ELEM(curarea->spacetype, SPACE_VIEW3D, SPACE_INFO); else return;
	if(G.vd==0) return;

	check_editmode(OB_MESH);
	
	G.f &= ~(G_VERTEXPAINT+G_FACESELECT+G_TEXTUREPAINT);
	setcursor_space(SPACE_VIEW3D, CURSOR_STD);

	/* als geen obedit: nieuw object en in editmode gaan */
	if(G.obedit==0) {
		/* add_object actually returns an object ! :-)
		But it also stores the added object struct in
		G.scene->basact->object (BASACT->object) */

		add_object(OB_MESH);
		base_init_from_view3d(BASACT, G.vd);
		G.obedit= BASACT->object;
		
		where_is_object(G.obedit);
		
		make_editMesh();
		setcursor_space(SPACE_VIEW3D, CURSOR_EDIT);
		newob= 1;
	}
	me= G.obedit->data;
	
	/* deselectall */
	eve= G.edve.first;
	while(eve) {
		if(eve->f & 1) eve->f&= ~1;
		eve= eve->next;
	}

	totoud= tot; /* onthouden en terugzetten als cube/plane */
	
	/* imat en centrum en afmeting */
	Mat3CpyMat4(mat, G.obedit->obmat);

	curs= give_cursor();
	VECCOPY(cent, curs);
	cent[0]-= G.obedit->obmat[3][0];
	cent[1]-= G.obedit->obmat[3][1];
	cent[2]-= G.obedit->obmat[3][2];

	if(type!= 11) {
		Mat3CpyMat4(imat, G.vd->viewmat);
		Mat3MulVecfl(imat, cent);
		Mat3MulMat3(cmat, imat, mat);
		Mat3Inv(imat,cmat);
	} else {
		Mat3Inv(imat, mat);
	}
	
	/* ext==extrudeflag, tot==aantal verts in basis */

	switch(type) {
	case 0:		/* plane */
		tot= 4;
		ext= 0;
		fill= 1;
		if(newob) rename_id((ID *)G.obedit, "Plane");
		if(newob) rename_id((ID *)me, "Plane");
		break;
	case 1:		/* cube  */
		tot= 4;
		ext= 1;
		fill= 1;
		if(newob) rename_id((ID *)G.obedit, "Cube");
		if(newob) rename_id((ID *)me, "Cube");
		break;
	case 4:		/* circle  */
		if(button(&tot,3,100,"Vertices:")==0) return;
		ext= 0;
		fill= 0;
		if(newob) rename_id((ID *)G.obedit, "Circle");
		if(newob) rename_id((ID *)me, "Circle");
		break;
	case 5:		/* cylinder  */
		if(button(&tot,3,100,"Vertices:")==0) return;
		ext= 1;
		fill= 1;
		if(newob) rename_id((ID *)G.obedit, "Cylinder");
		if(newob) rename_id((ID *)me, "Cylinder");
		break;
	case 6:		/* tube  */
		if(button(&tot,3,100,"Vertices:")==0) return;
		ext= 1;
		fill= 0;
		if(newob) rename_id((ID *)G.obedit, "Tube");
		if(newob) rename_id((ID *)me, "Tube");
		break;
	case 7:		/* cone  */
		if(button(&tot,3,100,"Vertices:")==0) return;
		ext= 0;
		fill= 1;
		if(newob) rename_id((ID *)G.obedit, "Cone");
		if(newob) rename_id((ID *)me, "Cone");
		break;
	case 10:	/* grid */
		if(button(&tot,2,100,"X res:")==0) return;
		if(button(&seg,2,100,"Y res:")==0) return;
		if(newob) rename_id((ID *)G.obedit, "Grid");
		if(newob) rename_id((ID *)me, "Grid");
		break;
	case 11:	/* UVsphere */
		if(button(&seg,3,100,"Segments:")==0) return;
		if(button(&tot,3,100,"Rings:")==0) return;
		if(newob) rename_id((ID *)G.obedit, "Sphere");
		if(newob) rename_id((ID *)me, "Sphere");
		break;
	case 12:	/* Icosphere */
		if(button(&subdiv,1,5,"Subdivision:")==0) return;
		if(newob) rename_id((ID *)G.obedit, "Sphere");
		if(newob) rename_id((ID *)me, "Sphere");
		break;
	case 13:	/* Monkey */
		if(newob) rename_id((ID *)G.obedit, "Suzanne");
		if(newob) rename_id((ID *)me, "Suzanne");
		break;
	}

	dia= sqrt(2.0)*G.vd->grid;
	d= -G.vd->grid;
	phid= 2*M_PI/tot;
	phi= .25*M_PI;


	if(type<10) {	/* alles behalve grid of sphere */
		if(ext==0 && type!=7) d= 0;
	
		/* de vertices */
		vtop= vdown= v1= v2= 0;
		for(b=0; b<=ext; b++) {
			for(a=0; a<tot; a++) {
				
				vec[0]= cent[0]+dia*sin(phi);
				vec[1]= cent[1]+dia*cos(phi);
				vec[2]= cent[2]+d;
				
				Mat3MulVecfl(imat, vec);
				eve= addvertlist(vec);
				eve->f= 1;
				if(a==0) {
					if(b==0) v1= eve;
					else v2= eve;
				}
				phi+=phid;
			}
			d= -d;
		}
		/* centrum vertices */
		if(fill && type>1) {
			VECCOPY(vec,cent);
			vec[2]-= -d;
			Mat3MulVecfl(imat,vec);
			vdown= addvertlist(vec);
			if(ext || type==7) {
				VECCOPY(vec,cent);
				vec[2]-= d;
				Mat3MulVecfl(imat,vec);
				vtop= addvertlist(vec);
			}
		} else {
			vdown= v1;
			vtop= v2;
		}
		if(vtop) vtop->f= 1;
		if(vdown) vdown->f= 1;
	
		/* boven en ondervlak */
		if(fill) {
			if(tot==4 && (type==0 || type==1)) {
				v3= v1->next->next;
				if(ext) v4= v2->next->next;
				
				addvlaklist(v3, v1->next, v1, v3->next, NULL);
				if(ext) addvlaklist(v2, v2->next, v4, v4->next, NULL);
				
			}
			else {
				v3= v1;
				v4= v2;
				for(a=1; a<tot; a++) {
					addvlaklist(vdown, v3, v3->next, 0, NULL);
					v3= v3->next;
					if(ext) {
						addvlaklist(vtop, v4, v4->next, 0, NULL);
						v4= v4->next;
					}
				}
				if(type>1) {
					addvlaklist(vdown, v3, v1, 0, NULL);
					if(ext) addvlaklist(vtop, v4, v2, 0, NULL);
				}
			}
		}
		else if(type==4) {  /* wel edges bij circle */
			v3= v1;
			for(a=1;a<tot;a++) {
				addedgelist(v3,v3->next);
				v3= v3->next;
			}
			addedgelist(v3,v1);
		}
		/* zijvlakken */
		if(ext) {
			v3= v1;
			v4= v2;
			for(a=1; a<tot; a++) {
				addvlaklist(v3, v3->next, v4->next, v4, NULL);
				v3= v3->next;
				v4= v4->next;
			}
			addvlaklist(v3, v1, v2, v4, NULL);
		}
		else if(type==7) { /* cone */
			v3= v1;
			for(a=1; a<tot; a++) {
				addvlaklist(vtop, v3->next, v3, 0, NULL);
				v3= v3->next;
			}
			addvlaklist(vtop, v1, v3, 0, NULL);
		}
		
		if(type<2) tot= totoud;
		
	}
	else if(type==10) {	/*  grid */
		/* alle flags wissen */
		eve= G.edve.first;
		while(eve) {
			eve->f= 0;
			eve= eve->next;
		}
		dia= G.vd->grid;
		/* eerst een segment: de X as */
		phi= -1.0; 
		phid= 2.0/((float)tot-1);
		for(a=0;a<tot;a++) {
			vec[0]= cent[0]+dia*phi;
			vec[1]= cent[1]- dia;
			vec[2]= cent[2];
			Mat3MulVecfl(imat,vec);
			eve= addvertlist(vec);
			eve->f= 1+2+4;
			if (a) addedgelist(eve->prev,eve);
			phi+=phid;
		}
		/* extruden en transleren */
		vec[0]= vec[2]= 0.0;
		vec[1]= dia*phid;
		Mat3MulVecfl(imat, vec);
		for(a=0;a<seg-1;a++) {
			extrudeflag(2,0);
			translateflag(2, vec);
		}
	}
	else if(type==11) {	/*  UVsphere */
		float tmat[3][3];
		
		/* alle flags wissen */
		eve= G.edve.first;
		while(eve) {
			eve->f= 0;
			eve= eve->next;
		}
		
		/* eerst een segment */
		phi= 0; 
		phid/=2;
		for(a=0; a<=tot; a++) {
			vec[0]= cent[0]+dia*sin(phi);
			vec[1]= cent[1];
			vec[2]= cent[2]+dia*cos(phi);
			Mat3MulVecfl(imat,vec);
			eve= addvertlist(vec);
			eve->f= 1+2+4;
			if(a==0) v1= eve;
			else addedgelist(eve->prev, eve);
			phi+= phid;
		}
		
		/* extruden en roteren */
		phi= M_PI/seg;
		q[0]= cos(phi);
		q[3]= sin(phi);
		q[1]=q[2]= 0;
		QuatToMat3(q, cmat);
		Mat3MulMat3(tmat, cmat, mat);
		Mat3MulMat3(cmat, imat, tmat);
		
		for(a=0; a<seg; a++) {
			extrudeflag(2, 0);
			rotateflag(2, v1->co, cmat);
		}
		removedoublesflag(4, 0.01);
	}
	else if(type==12) {	/* Icosphere */
		EditVert *eva[12];

		/* alle flags wissen */
		eve= G.edve.first;
		while(eve) {
			eve->f= 0;
			eve= eve->next;
		}
		dia/=200;
		for(a=0;a<12;a++) {
			vec[0]= dia*icovert[a][0];
			vec[1]= dia*icovert[a][1];
			vec[2]= dia*icovert[a][2];
			eva[a]= addvertlist(vec);
			eva[a]->f= 1+2;
		}
		for(a=0;a<20;a++) {
			v1= eva[ icovlak[a][0] ];
			v2= eva[ icovlak[a][1] ];
			v3= eva[ icovlak[a][2] ];
			addvlaklist(v1, v2, v3, 0, NULL);
		}

		dia*=200;
		for(a=1; a<subdiv; a++) subdivideflag(2, dia, 0);
		/* nu pas met imat */
		eve= G.edve.first;
		while(eve) {
			if(eve->f & 2) {
				VecAddf(eve->co,eve->co,cent);
				Mat3MulVecfl(imat,eve->co);
			}
			eve= eve->next;
		}
	} else if (type==13) {	/* Monkey */
		extern int monkeyo, monkeynv, monkeynf;
		extern signed char monkeyf[][4];
		extern signed char monkeyv[][3];
		EditVert **tv= MEM_mallocN(sizeof(*tv)*monkeynv*2, "tv");
		int i;

		for (i=0; i<monkeynv; i++) {
			float v[3];
			v[0]= (monkeyv[i][0]+127)/128.0, v[1]= monkeyv[i][1]/128.0, v[2]= monkeyv[i][2]/128.0;
			tv[i]= addvertlist(v);
			tv[monkeynv+i]= (fabs(v[0]= -v[0])<0.001)?tv[i]:addvertlist(v);
		}
		for (i=0; i<monkeynf; i++) {
			addvlaklist(tv[monkeyf[i][0]+i-monkeyo], tv[monkeyf[i][1]+i-monkeyo], tv[monkeyf[i][2]+i-monkeyo], (monkeyf[i][3]!=monkeyf[i][2])?tv[monkeyf[i][3]+i-monkeyo]:NULL, NULL);
			addvlaklist(tv[monkeynv+monkeyf[i][2]+i-monkeyo], tv[monkeynv+monkeyf[i][1]+i-monkeyo], tv[monkeynv+monkeyf[i][0]+i-monkeyo], (monkeyf[i][3]!=monkeyf[i][2])?tv[monkeynv+monkeyf[i][3]+i-monkeyo]:NULL, NULL);
		}

		MEM_freeN(tv);
	}
	
	if(type!=0 && type!=10) righthandfaces(1);
	countall();
	allqueue(REDRAWVIEW3D, 0);
	allqueue(REDRAWIPO, 0);
	allqueue(REDRAWHEADERS, 0);
	allqueue(REDRAWINFO, 1); 	/* 1, want header->win==0! */	
	allqueue(REDRAWBUTSEDIT, 0);
	makeDispList(G.obedit);

	if (type==13) notice("Oooh Oooh Oooh");
}

void vertexsmooth(void)
{
	struct EditVert *eve;
	struct EditEdge *eed;
	float *adror, *adr, fac;
	float fvec[3];
	int teller=0;

	if(G.obedit==0) return;

	/* aantal tellen */
	eve= G.edve.first;
	while(eve) {
		if(eve->f & 1) teller++;
		eve= eve->next;
	}
	if(teller==0) return;

	adr=adror= (float *)MEM_callocN(3*sizeof(float *)*teller, "vertsmooth");
	eve= G.edve.first;
	while(eve) {
		if(eve->f & 1) {
			eve->vn= (EditVert *)adr;
			eve->f1= 0;
			adr+= 3;
		}
		eve= eve->next;
	}
	
	eed= G.eded.first;
	while(eed) {
		if( (eed->v1->f & 1) || (eed->v2->f & 1) ) {
			fvec[0]= (eed->v1->co[0]+eed->v2->co[0])/2.0;
			fvec[1]= (eed->v1->co[1]+eed->v2->co[1])/2.0;
			fvec[2]= (eed->v1->co[2]+eed->v2->co[2])/2.0;
			
			if((eed->v1->f & 1) && eed->v1->f1<255) {
				eed->v1->f1++;
				VecAddf((float *)eed->v1->vn, (float *)eed->v1->vn, fvec);
			}
			if((eed->v2->f & 1) && eed->v2->f1<255) {
				eed->v2->f1++;
				VecAddf((float *)eed->v2->vn, (float *)eed->v2->vn, fvec);
			}
		}
		eed= eed->next;
	}

	eve= G.edve.first;
	while(eve) {
		if(eve->f & 1) {
			if(eve->f1) {
				adr= (float *)eve->vn;
				fac= 0.5/(float)eve->f1;
				
				eve->co[0]= 0.5*eve->co[0]+fac*adr[0];
				eve->co[1]= 0.5*eve->co[1]+fac*adr[1];
				eve->co[2]= 0.5*eve->co[2]+fac*adr[2];
			}
			eve->vn= 0;
		}
		eve= eve->next;
	}
	MEM_freeN(adror);

	allqueue(REDRAWVIEW3D, 0);
	makeDispList(G.obedit);
}


void vertexnoise(void)
{
	extern float Tin;
	Material *ma;
	Tex *tex;
	EditVert *eve;
	float b2, ofs, vec[3];

	if(G.obedit==0) return;

	ma= give_current_material(G.obedit, G.obedit->actcol);
	if(ma==0 || ma->mtex[0]==0 || ma->mtex[0]->tex==0) {
		return;
	}
	tex= ma->mtex[0]->tex;
	
	ofs= tex->turbul/200.0;
	
	eve= (struct EditVert *)G.edve.first;
	while(eve) {
		if(eve->f & 1) {
			
			if(tex->type==TEX_STUCCI) {
				
				b2= BLI_hnoise(tex->noisesize, eve->co[0], eve->co[1], eve->co[2]);
				if(tex->stype) ofs*=(b2*b2);
				vec[0]= 0.2*(b2-BLI_hnoise(tex->noisesize, eve->co[0]+ofs, eve->co[1], eve->co[2]));
				vec[1]= 0.2*(b2-BLI_hnoise(tex->noisesize, eve->co[0], eve->co[1]+ofs, eve->co[2]));
				vec[2]= 0.2*(b2-BLI_hnoise(tex->noisesize, eve->co[0], eve->co[1], eve->co[2]+ofs));
				
				VecAddf(eve->co, eve->co, vec);
			}
			else {
				
				externtex(ma->mtex[0], eve->co);
			
				eve->co[2]+= 0.05*Tin;
			}
		}
		eve= eve->next;
	}

	allqueue(REDRAWVIEW3D, 0);
	makeDispList(G.obedit);
}

void hide_mesh(int swap)
{
	struct EditVert *eve;
	struct EditEdge *eed;

	if(G.obedit==0) return;

	if(swap) {
		eve= G.edve.first;
		while(eve) {
			if((eve->f & 1)==0) {
				eve->xs= 3200;
				eve->h= 1;
			}
			eve= eve->next;
		}
	}
	else {
		eve= G.edve.first;
		while(eve) {
			if(eve->f & 1) {
				eve->f-=1;
				eve->xs= 3200;
				eve->h= 1;
			}
			eve= eve->next;
		}
	}
	eed= G.eded.first;
	while(eed) {
		if(eed->v1->h || eed->v2->h) eed->h= 1;
		else eed->h= 0;
		eed= eed->next;
	}

	allqueue(REDRAWVIEW3D, 0);
	makeDispList(G.obedit);
}


void reveal_mesh(void)
{
	struct EditVert *eve;
	struct EditEdge *eed;

	if(G.obedit==0) return;

	eve= G.edve.first;
	while(eve) {
		if(eve->h) {
			eve->h= 0;
			eve->f|=1;
		}
		eve= eve->next;
	}

	eed= G.eded.first;
	while(eed) {
		eed->h= 0;
		eed= eed->next;
	}

	allqueue(REDRAWVIEW3D, 0);
	makeDispList(G.obedit);
}

static float convex(float *v1, float *v2, float *v3, float *v4)
{
	float cross[3], test[3];
	float inpr;
	
	CalcNormFloat(v1, v2, v3, cross);
	CalcNormFloat(v1, v3, v4, test);

	inpr= cross[0]*test[0]+cross[1]*test[1]+cross[2]*test[2];

	return inpr;
}

/* returns vertices of two adjacent triangles forming a quad 
   - can be righthand or lefthand

			4-----3
			|\    |
			| \ 2 | <- evl1
			|  \  | 
	  evl-> | 1 \ | 
			|    \| 
			1-----2

*/
#define VTEST(face, num, other) \
	(face->v##num != other->v1 && face->v##num != other->v2 && face->v##num != other->v3) 

static void givequadverts(EditVlak *evl, EditVlak *evl1, EditVert **v1, EditVert **v2, EditVert **v3, EditVert **v4, float **uv, unsigned int *col)
{
	if VTEST(evl, 1, evl1) {
	//if(evl->v1!=evl1->v1 && evl->v1!=evl1->v2 && evl->v1!=evl1->v3) {
		*v1= evl->v1;
		*v2= evl->v2;
		uv[0] = evl->uv[0];
		uv[1] = evl->uv[1];
		col[0] = evl->col[0];
		col[1] = evl->col[1];
	}
	else if VTEST(evl, 2, evl1) {
	//else if(evl->v2!=evl1->v1 && evl->v2!=evl1->v2 && evl->v2!=evl1->v3) {
		*v1= evl->v2;
		*v2= evl->v3;
		uv[0] = evl->uv[1];
		uv[1] = evl->uv[2];
		col[0] = evl->col[1];
		col[1] = evl->col[2];
	}
	else if VTEST(evl, 3, evl1) {
	// else if(evl->v3!=evl1->v1 && evl->v3!=evl1->v2 && evl->v3!=evl1->v3) {
		*v1= evl->v3;
		*v2= evl->v1;
		uv[0] = evl->uv[2];
		uv[1] = evl->uv[0];
		col[0] = evl->col[2];
		col[1] = evl->col[0];
	}
	
	if VTEST(evl1, 1, evl) {
	// if(evl1->v1!=evl->v1 && evl1->v1!=evl->v2 && evl1->v1!=evl->v3) {
		*v3= evl1->v1;
		uv[2] = evl1->uv[0];
		col[2] = evl1->col[0];

		*v4= evl1->v2;
		uv[3] = evl1->uv[1];
		col[3] = evl1->col[1];
/*
if(evl1->v2== *v2) {
			*v4= evl1->v3;
			uv[3] = evl1->uv[2];
		} else {
			*v4= evl1->v2;
			uv[3] = evl1->uv[1];
		}	
		*/
	}
	else if VTEST(evl1, 2, evl) {
	// else if(evl1->v2!=evl->v1 && evl1->v2!=evl->v2 && evl1->v2!=evl->v3) {
		*v3= evl1->v2;
		uv[2] = evl1->uv[1];
		col[2] = evl1->col[1];

		*v4= evl1->v3;
		uv[3] = evl1->uv[2];
		col[3] = evl1->col[2];
/*
if(evl1->v3== *v2) {
			*v4= evl1->v1;
			uv[3] = evl1->uv[0];
		} else {	
			*v4= evl1->v3;
			uv[3] = evl1->uv[2];
		}	
		*/
	}
	else if VTEST(evl1, 3, evl) {
	// else if(evl1->v3!=evl->v1 && evl1->v3!=evl->v2 && evl1->v3!=evl->v3) {
		*v3= evl1->v3;
		uv[2] = evl1->uv[2];
		col[2] = evl1->col[2];

		*v4= evl1->v1;
		uv[3] = evl1->uv[0];
		col[3] = evl1->col[0];
/*
if(evl1->v1== *v2) {
			*v4= evl1->v2;
			uv[3] = evl1->uv[3];
		} else {	
			*v4= evl1->v1;
			uv[3] = evl1->uv[0];
		}	
		*/
	}
	else {
		pupmenu("Wanna crash?%t|Yes Please!%x1");
		return;
	}
	
}


/* ook weer twee zeer vreemde 'patch' functies om de uv van tfaces te bewaren */
/*
static float *set_correct_uv(EditVert *eve,  EditVlak **evla)
{
	
	if(eve== evla[1]->v3) return evla[1]->uv[2];
	if(eve== evla[0]->v3) return evla[0]->uv[2];
	if(eve== evla[1]->v2) return evla[1]->uv[1];
	if(eve== evla[0]->v2) return evla[0]->uv[1];
	if(eve== evla[1]->v1) return evla[1]->uv[0];
	if(eve== evla[0]->v1) return evla[0]->uv[0];
	return 0;
}

crazy code commented out..
static void restore_wuv(EditVlak *evl, void **evla)
{
	int *lp;
	
	lp= (int *)set_correct_uv(evl->v1, (EditVlak **)evla);
	((int *)(evl->uv[0]))[0]= lp[0];
	((int *)(evl->uv[0]))[4]= lp[4];
	
	lp= (int *)set_correct_uv(evl->v2, (EditVlak **)evla);
	((int *)(evl->uv[1]))[0]= lp[0];
	((int *)(evl->uv[1]))[4]= lp[4];

	lp= (int *)set_correct_uv(evl->v3, (EditVlak **)evla);
	((int *)(evl->uv[2]))[0]= lp[0];
	((int *)(evl->uv[2]))[4]= lp[4];
	
}
*/


/* Helper functions for edge/quad edit features*/
/*

*/

static void untag_edges(EditVlak *f)
{
	f->e1->f = 0;
	f->e2->f = 0;
	if (f->e3) f->e3->f = 0;
	if (f->e4) f->e4->f = 0;
}

#if 0
static void mark_clear_edges(EditVlak *f)
{
	f->e1->f1 = 1;
	f->e2->f1 = 1;
	if (f->e3) f->e3->f1 = 1;
	if (f->e4) f->e4->f1 = 1;
}
#endif

static int count_edges(EditEdge *ed)
{
	int totedge = 0;
	while(ed) {
		ed->vn= 0;
		if( (ed->v1->f & 1) && (ed->v2->f & 1) ) totedge++;
		ed= ed->next;
	}
	return totedge;
}

/** remove and free list of tagged edges */
static void free_tagged_edgelist(EditEdge *eed)
{
	EditEdge *nexted;

	while(eed) {
		nexted= eed->next;
		if(eed->f1) {
			remedge(eed);
			free(eed);
		}
		eed= nexted;
	}	
}	
/** remove and free list of tagged faces */

static void free_tagged_facelist(EditVlak *evl)
{	
	EditVlak *nextvl;

	while(evl) {
		nextvl= evl->next;
		if(evl->f1) {
			BLI_remlink(&G.edvl, evl);
			freevlak(evl);
		}
		evl= nextvl;
	}
}	

typedef EditVlak *EVPtr;
typedef EVPtr EVPTuple[2];

/** builds EVPTuple array evla of face tuples (in fact pointers to EditVlaks)
    sharing one edge.
	arguments: selected edge list, face list.
	Edges will also be tagged accordingly (see eed->f)          */

static int collect_quadedges(EVPTuple *evla, EditEdge *eed, EditVlak *evl)
{
	int i = 0;
	EditEdge *e1, *e2, *e3;
	EVPtr *evp;

	/* run through edges, if selected, set pointer edge-> facearray */
	while(eed) {
		eed->f= 0;
		eed->f1= 0;
		if( (eed->v1->f & 1) && (eed->v2->f & 1) ) {
			eed->vn= (EditVert *) (&evla[i]);
			i++;
		}
		eed= eed->next;
	}
		
	
	/* find edges pointing to 2 faces by procedure:
	
	- run through faces and their edges, increase
	  face counter e->f for each face 
	*/

	while(evl) {
		evl->f1= 0;
		if(evl->v4==0) {  /* if triangle */
			if(vlakselectedAND(evl, 1)) {
				
				e1= evl->e1;
				e2= evl->e2;
				e3= evl->e3;
				if(e1->f<3) {
					if(e1->f<2) {
						evp= (EVPtr *) e1->vn;
						evp[(int)e1->f]= evl;
					}
					e1->f+= 1;
				}
				if(e2->f<3) {
					if(e2->f<2) {
						evp= (EVPtr *) e2->vn;
						evp[(int)e2->f]= evl;
					}
					e2->f+= 1;
				}
				if(e3->f<3) {
					if(e3->f<2) {
						evp= (EVPtr *) e3->vn;
						evp[(int)e3->f]= evl;
					}
					e3->f+= 1;
				}
			}
		}
		evl= evl->next;
	}
	return i;
}


void join_triangles(void)
{
	EditVert *v1, *v2, *v3, *v4;
	EditVlak *evl, *w;
	EVPTuple *evlar;
	EVPtr *evla;
	EditEdge *eed, *nexted;
	int totedge, ok;
	float *uv[4];
	unsigned int col[4];


	totedge = count_edges(G.eded.first);
	if(totedge==0) return;

	evlar= (EVPTuple *) MEM_callocN(totedge * sizeof(EVPTuple), "jointris");

	ok = collect_quadedges(evlar, G.eded.first, G.edvl.first);
	if (G.f & G_DEBUG) {
		printf("edges selected: %d\n", ok);
	}	

	eed= G.eded.first;
	while(eed) {
		nexted= eed->next;
		
		if(eed->f==2) {  /* points to 2 faces */
			
			evla= (EVPtr *) eed->vn;
			
			/* don't do it if flagged */

			ok= 1;
			evl= evla[0];
			if(evl->e1->f1 || evl->e2->f1 || evl->e3->f1) ok= 0;
			evl= evla[1];
			if(evl->e1->f1 || evl->e2->f1 || evl->e3->f1) ok= 0;
			
			if(ok) {
				/* test convex */
				givequadverts(evla[0], evla[1], &v1, &v2, &v3, &v4, uv, col);

/*
		4-----3        4-----3
		|\    |        |     |
		| \ 1 |        |     |
		|  \  |  ->    |     |	
		| 0 \ |        |     | 
		|    \|        |     |
		1-----2        1-----2
*/
				/* make new faces */
				if( convex(v1->co, v2->co, v3->co, v4->co) > 0.01) {
					if(exist_vlak(v1, v2, v3, v4)==0) {
						w = addvlaklist(v1, v2, v3, v4, evla[0]);
						untag_edges(w);
						if (w->tface) {
							UVCOPY(w->uv[0], uv[0]);
							UVCOPY(w->uv[1], uv[1]);
							UVCOPY(w->uv[2], uv[2]);
							UVCOPY(w->uv[3], uv[3]);
						}	
						memcpy(w->col, col, sizeof(w->col));
					}
					/* tag as to-be-removed */
					FACE_MARKCLEAR(evla[0]);
					FACE_MARKCLEAR(evla[1]);
					eed->f1 = 1; 
				} /* endif test convex */
			}
		}
		eed= nexted;
	}
	free_tagged_edgelist(G.eded.first);
	free_tagged_facelist(G.edvl.first);

	MEM_freeN(evlar);
	
	allqueue(REDRAWVIEW3D, 0);
	makeDispList(G.obedit);

}

/* quick hack, basically a copy of beauty_fill */
void edge_flip(void)
{
	EditVert *v1, *v2, *v3, *v4;
	EditEdge *eed, *nexted;
	EditVlak *evl, *w;
	//void **evlar, **evla;
	EVPTuple *evlar;
	EVPtr *evla;

	float *uv[4];
	unsigned int col[4];

	int totedge, ok;
	
	/* - alle geselecteerde edges met 2 vlakken
	 * - vind die vlakken: opslaan in edges (extra datablok)
	 * - per edge: - test convex
	 *			   - test edge: flip?
						- zoja: remedge,  addedge, alle randedges nieuwe vlakpointers
	 */

	totedge = count_edges(G.eded.first);
	if(totedge==0) return;

	/* temporary array for : edge -> face[1], face[2] */
	evlar= (EVPTuple *) MEM_callocN(totedge * sizeof(EVPTuple), "edgeflip");

	ok = collect_quadedges(evlar, G.eded.first, G.edvl.first);
	
	eed= G.eded.first;
	while(eed) {
		nexted= eed->next;
		
		if(eed->f==2) {  /* points to 2 faces */
			
			evla= (EVPtr *) eed->vn;
			
			/* don't do it if flagged */

			ok= 1;
			evl= evla[0];
			if(evl->e1->f1 || evl->e2->f1 || evl->e3->f1) ok= 0;
			evl= evla[1];
			if(evl->e1->f1 || evl->e2->f1 || evl->e3->f1) ok= 0;
			
			if(ok) {
				/* test convex */
				givequadverts(evla[0], evla[1], &v1, &v2, &v3, &v4, uv, col);

/*
		4-----3        4-----3
		|\    |        |    /|
		| \ 1 |        | 1 / |
		|  \  |  ->    |  /  |	
		| 0 \ |        | / 0 | 
		|    \|        |/    |
		1-----2        1-----2
*/
				/* make new faces */
				if (v1 && v2 && v3){
					if( convex(v1->co, v2->co, v3->co, v4->co) > 0.01) {
						if(exist_vlak(v1, v2, v3, v4)==0) {
							w = addvlaklist(v1, v2, v3, 0, evla[1]);
							
							untag_edges(w);
							if (w->tface) {
								UVCOPY(w->uv[0], uv[0]);
								UVCOPY(w->uv[1], uv[1]);
								UVCOPY(w->uv[2], uv[2]);
							}	
							w->col[0] = col[0]; w->col[1] = col[1]; w->col[2] = col[2]; 
							
							w = addvlaklist(v1, v3, v4, 0, evla[1]);
							untag_edges(w);
							if (w->tface) {
								UVCOPY(w->uv[0], uv[0]);
								UVCOPY(w->uv[1], uv[2]);
								UVCOPY(w->uv[2], uv[3]);
							}	
							w->col[0] = col[0]; w->col[1] = col[2]; w->col[2] = col[3]; 
							
							/* erase old faces and edge */
						}
						/* tag as to-be-removed */
						FACE_MARKCLEAR(evla[1]);
						FACE_MARKCLEAR(evla[0]);
						eed->f1 = 1; 
						
					} /* endif test convex */
				}
			}
		}
		eed= nexted;
	}

	/* clear tagged edges and faces: */
	free_tagged_edgelist(G.eded.first);
	free_tagged_facelist(G.edvl.first);
		
	MEM_freeN(evlar);
	
	allqueue(REDRAWVIEW3D, 0);
	makeDispList(G.obedit);
}
						
void beauty_fill(void)
{
    EditVert *v1, *v2, *v3, *v4;
    EditEdge *eed, *nexted;
    EditEdge dia1, dia2;
    EditVlak *evl, *w;
    // void **evlar, **evla;
    EVPTuple *evlar;
    EVPtr *evla;
    float *uv[4];
    unsigned int col[4];
    float len1, len2, len3, len4, len5, len6, opp1, opp2, fac1, fac2;
    int totedge, ok, notbeauty=8, onedone;

    /* - alle geselecteerde edges met 2 vlakken
     * - vind die vlakken: opslaan in edges (extra datablok)
     * - per edge: - test convex
     *             - test edge: flip?
                        - zoja: remedge,  addedge, alle randedges nieuwe vlakpointers
     */

    totedge = count_edges(G.eded.first);
    if(totedge==0) return;

    if(okee("Beauty Fill")==0) return;

    /* tempblok met vlakpointers */
    evlar= (EVPTuple *) MEM_callocN(totedge * sizeof(EVPTuple), "beautyfill");

    while (notbeauty) {
        notbeauty--;

        ok = collect_quadedges(evlar, G.eded.first, G.edvl.first);

        /* gaatie */
        onedone= 0;

        eed= G.eded.first;
        while(eed) {
            nexted= eed->next;

            if(eed->f==2) {

                evla = (EVPtr *) eed->vn;

                /* geen van de vlakken mag al gedaan zijn */
                ok= 1;
                evl= evla[0];
                if(evl->e1->f1 || evl->e2->f1 || evl->e3->f1) ok= 0;
                evl= evla[1];
                if(evl->e1->f1 || evl->e2->f1 || evl->e3->f1) ok= 0;

                if(ok) {
                    /* test convex */
                    givequadverts(evla[0], evla[1], &v1, &v2, &v3, &v4, uv, col);
                    if( convex(v1->co, v2->co, v3->co, v4->co) > -0.5) {

                        /* test edges */
                        if( ((long)v1) > ((long)v3) ) {
                            dia1.v1= v3;
                            dia1.v2= v1;
                        }
                        else {
                            dia1.v1= v1;
                            dia1.v2= v3;
                        }

                        if( ((long)v2) > ((long)v4) ) {
                            dia2.v1= v4;
                            dia2.v2= v2;
                        }
                        else {
                            dia2.v1= v2;
                            dia2.v2= v4;
                        }

                        /* testregel:
                         * de oppervlakte gedeeld door de totale edgelengte
                         *
                         */

                        len1= VecLenf(v1->co, v2->co);
                        len2= VecLenf(v2->co, v3->co);
                        len3= VecLenf(v3->co, v4->co);
                        len4= VecLenf(v4->co, v1->co);
                        len5= VecLenf(v1->co, v3->co);
                        len6= VecLenf(v2->co, v4->co);

                        opp1= AreaT3Dfl(v1->co, v2->co, v3->co);
                        opp2= AreaT3Dfl(v1->co, v3->co, v4->co);

                        fac1= opp1/(len1+len2+len5) + opp2/(len3+len4+len5);

                        opp1= AreaT3Dfl(v2->co, v3->co, v4->co);
                        opp2= AreaT3Dfl(v2->co, v4->co, v1->co);

                        fac2= opp1/(len2+len3+len6) + opp2/(len4+len1+len6);

                        ok= 0;
                        if(fac1 > fac2) {
                            if(dia2.v1==eed->v1 && dia2.v2==eed->v2) {
                                eed->f1= 1;
                                evl= evla[0];
                                evl->f1= 1;
                                evl= evla[1];
                                evl->f1= 1;

                                w= addvlaklist(v1, v2, v3, 0, evl);
                                if (w->tface) {
                                    UVCOPY(w->uv[0], uv[0]);
                                    UVCOPY(w->uv[1], uv[1]);
                                    UVCOPY(w->uv[2], uv[2]);
                                }
                                w->col[0] = col[0]; w->col[1] = col[1]; w->col[2] = col[2];
                                w= addvlaklist(v1, v3, v4, 0, evl);
                                if (w->tface) {
                                    UVCOPY(w->uv[0], uv[0]);
                                    UVCOPY(w->uv[1], uv[2]);
                                    UVCOPY(w->uv[2], uv[3]);
                                }
                                w->col[0] = col[0]; w->col[1] = col[2]; w->col[2] = col[3];

                                onedone= 1;
                            }
                        }
                        else if(fac1 < fac2) {
                            if(dia1.v1==eed->v1 && dia1.v2==eed->v2) {
                                eed->f1= 1;
                                evl= evla[0];
                                evl->f1= 1;
                                evl= evla[1];
                                evl->f1= 1;

                                w= addvlaklist(v2, v3, v4, 0, evl);
                                if (w->tface) {
                                    UVCOPY(w->uv[0], uv[1]);
                                    UVCOPY(w->uv[1], uv[3]);
                                    UVCOPY(w->uv[2], uv[4]);
                                }

                                w= addvlaklist(v1, v2, v4, 0, evl);
                                if (w->tface) {
                                    UVCOPY(w->uv[0], uv[0]);
                                    UVCOPY(w->uv[1], uv[1]);
                                    UVCOPY(w->uv[2], uv[3]);
                                }
                                onedone= 1;
                            }
                        }
                    }
                }

            }
            eed= nexted;
        }

        free_tagged_edgelist(G.eded.first);
        free_tagged_facelist(G.edvl.first);

        if(onedone==0) break;
    }

    MEM_freeN(evlar);

    allqueue(REDRAWVIEW3D, 0);
    makeDispList(G.obedit);
}

/** tests whether selected mesh objects have tfaces */
static int testSelected_TfaceMesh(void)
{
	Base *base;
	Mesh *me;

	base = FIRSTBASE;
	while (base) {
		if TESTBASE(base) {
			if(base->object->type==OB_MESH) {
				me= base->object->data;
				if (me->tface) 
					return 1;
			}		
		}			
		base= base->next;
	}	
	return 0;
}	

void join_mesh(void)
{
	Base *base, *nextb;
	Object *ob;
	Material **matar, *ma;
	Mesh *me;
	MVert *mvert, *mvertmain;
	MFace *mface = NULL, *mfacemain;
	TFace *tface = NULL, *tfacemain;
	unsigned int *mcol=NULL, *mcolmain;
	float imat[4][4], cmat[4][4];
	int a, b, totcol, totvert=0, totface=0, ok=0, vertofs, map[MAXMAT];
#ifdef __NLA
	int	i, j, index;
	bDeformGroup *dg, *odg;
	MDeformVert *dvert, *dvertmain;
#endif
	
	if(G.obedit) return;
	
	ob= OBACT;
	if(!ob || ob->type!=OB_MESH) return;
	
	/* tellen */
	
	base= FIRSTBASE;
	while(base) {
		if TESTBASE(base) {
			if(base->object->type==OB_MESH) {
				me= base->object->data;
				totvert+= me->totvert;
				totface+= me->totface;
				
				if(base->object == ob) ok= 1;
			}
		}
		base= base->next;
	}
	
	/* zodoende is het aktieve object altijd select */ 
	if(ok==0) return;
	
	if(totvert==0 || totvert>65000) return;
	
	if(okee("Join selected Meshes")==0) return;
	
	/* nieuwe materiaal indexen en hoofdarray */
	matar= MEM_callocN(sizeof(void *)*MAXMAT, "join_mesh");
	totcol= ob->totcol;
	
	/* obact materials in nieuw hoofdarray, is mooiere start! */
	for(a=1; a<=ob->totcol; a++) {
		matar[a-1]= give_current_material(ob, a);
		id_us_plus((ID *)matar[a-1]);
		/* id->us ophogen: wordt ook verlaagd */
	}
	
	base= FIRSTBASE;
	while(base) {
		if TESTBASE(base) {
			if(ob!=base->object && base->object->type==OB_MESH) {
				me= base->object->data;
#ifdef __NLA
				// Join this object's vertex groups to the base one's
				for (dg=base->object->defbase.first; dg; dg=dg->next){
					/* See if this group exists in the object */
					for (odg=ob->defbase.first; odg; odg=odg->next){
						if (!strcmp(odg->name, dg->name)){
							break;
						}
					}
					if (!odg){
						odg = MEM_callocN (sizeof(bDeformGroup), "deformGroup");
						memcpy (odg, dg, sizeof(bDeformGroup));
						BLI_addtail(&ob->defbase, odg);
					}

				}
				if (ob->defbase.first && ob->actdef==0)
					ob->actdef=1;
#endif
				if(me->totvert) {
					for(a=1; a<=base->object->totcol; a++) {
						ma= give_current_material(base->object, a);
						if(ma) {
							for(b=0; b<totcol; b++) {
								if(ma == matar[b]) break;
							}
							if(b==totcol) {
								matar[b]= ma;
								ma->id.us++;
								totcol++;
							}
							if(totcol>=MAXMAT-1) break;
						}
					}
				}
			}
			if(totcol>=MAXMAT-1) break;
		}
		base= base->next;
	}

	me= ob->data;
	mvert= mvertmain= MEM_mallocN(totvert*sizeof(MVert), "joinmesh1");

	if (totface) mface= mfacemain= MEM_mallocN(totface*sizeof(MFace), "joinmesh2");
	else mfacemain= 0;

	if(me->mcol) mcol= mcolmain= MEM_callocN(totface*4*sizeof(int), "joinmesh3");
	else mcolmain= 0;

	/* if active object doesn't have Tfaces, but one in the selection does,
	   make TFaces for active, so we don't lose texture information in the
	   join process */
	if(me->tface || testSelected_TfaceMesh()) tface= tfacemain= MEM_callocN(totface*4*sizeof(TFace), "joinmesh4");
	else
		tfacemain= 0;

#ifdef __NLA
	if(me->dvert)
		dvert= dvertmain= MEM_callocN(totvert*sizeof(MDeformVert), "joinmesh5");
	else dvert=dvertmain= NULL;
#endif

	vertofs= 0;
	
	/* alle geselecteerde meshes invers transformen in obact */
	Mat4Invert(imat, ob->obmat);
	
	base= FIRSTBASE;
	while(base) {
		nextb= base->next;
		if TESTBASE(base) {
			if(base->object->type==OB_MESH) {
				
				me= base->object->data;
				
				if(me->totvert) {
					
					memcpy(mvert, me->mvert, me->totvert*sizeof(MVert));
					
#ifdef __NLA
					copy_dverts(dvert, me->dvert, me->totvert);

						/* >>>>> FIXME: Ensure that deformation groups are updated correctly */
					/* OLD VERSION */
					/*
					for (i=0; i<me->totvert; i++){
						for (j=0; j<mvert[i].totweight; j++){
							//	Find the old vertex group
							odg = BLI_findlink (&base->object->defbase, mvert[i].dw[j].def_nr);

							//	Search for a match in the new object
							for (dg=ob->defbase.first, index=0; dg; dg=dg->next, index++){
								if (!strcmp(dg->name, odg->name)){
									mvert[i].dw[j].def_nr = index;
									break;
								}
							}
						}
					}
					*/
					/* NEW VERSION */
					if (dvertmain){
						for (i=0; i<me->totvert; i++){
							for (j=0; j<dvert[i].totweight; j++){
								//	Find the old vertex group
								odg = BLI_findlink (&base->object->defbase, dvert[i].dw[j].def_nr);
								
								//	Search for a match in the new object
								for (dg=ob->defbase.first, index=0; dg; dg=dg->next, index++){
									if (!strcmp(dg->name, odg->name)){
										dvert[i].dw[j].def_nr = index;
										break;
									}
								}
							}
						}
						dvert+=me->totvert;
					}

#endif
					if(base->object != ob) {
						/* let op: matmul omkeren is ECHT fout */
						Mat4MulMat4(cmat, base->object->obmat, imat);
						
						a= me->totvert;
						while(a--) {
							Mat4MulVecfl(cmat, mvert->co);
							mvert++;
						}
					}
					else mvert+= me->totvert;
					
					if(mcolmain) {
						if(me->mcol) memcpy(mcol, me->mcol, me->totface*4*4);
						mcol+= 4*me->totface;
					}
				}
				if(me->totface) {
				
					/* mapping maken voor materialen */
					memset(map, 0, 4*MAXMAT);
					for(a=1; a<=base->object->totcol; a++) {
						ma= give_current_material(base->object, a);
						if(ma) {
							for(b=0; b<totcol; b++) {
								if(ma == matar[b]) {
									map[a-1]= b;
									break;
								}
							}
						}
					}

					memcpy(mface, me->mface, me->totface*sizeof(MFace));
					
					a= me->totface;
					while(a--) {
						mface->v1+= vertofs;
						mface->v2+= vertofs;
						if(mface->v3) mface->v3+= vertofs;
						if(mface->v4) mface->v4+= vertofs;
						
						mface->mat_nr= map[(int)mface->mat_nr];
						
						mface++;
					}
					
					if(tfacemain) {
						if(me->tface) memcpy(tface, me->tface, me->totface*sizeof(TFace));
						tface+= me->totface;
					}
					
				}
				vertofs+= me->totvert;
				
				if(base->object!=ob) {
					free_and_unlink_base(base);
				}
			}
		}
		base= nextb;
	}
	
	me= ob->data;
	
	if(me->mface) MEM_freeN(me->mface);
	me->mface= mfacemain;
	if(me->mvert) MEM_freeN(me->mvert);
#ifdef __NLA
	if(me->dvert) free_dverts(me->dvert, me->totvert);
	me->dvert = dvertmain;
#endif
	me->mvert= mvertmain;
	if(me->mcol) MEM_freeN(me->mcol);
	me->mcol= (MCol *)mcolmain;
	if(me->tface) MEM_freeN(me->tface);
	me->tface= tfacemain;
	me->totvert= totvert;
	me->totface= totface;
	
	/* oude material array */
	for(a=1; a<=ob->totcol; a++) {
		ma= ob->mat[a-1];
		if(ma) ma->id.us--;
	}
	for(a=1; a<=me->totcol; a++) {
		ma= me->mat[a-1];
		if(ma) ma->id.us--;
	}
	if(ob->mat) MEM_freeN(ob->mat);
	if(me->mat) MEM_freeN(me->mat);
	ob->mat= me->mat= 0;
	
	if(totcol) {
		me->mat= matar;
		ob->mat= MEM_callocN(sizeof(void *)*totcol, "join obmatar");
	}
	else MEM_freeN(matar);
	
	ob->totcol= me->totcol= totcol;
	ob->colbits= 0;
	
	/* andere mesh gebruikers */
	test_object_materials((ID *)me);
	
	enter_editmode();
	exit_editmode(1);
	
	allqueue(REDRAWVIEW3D, 0);
	allqueue(REDRAWBUTSMAT, 0);
	makeDispList(G.obedit);

}

void clever_numbuts_mesh(void)
{
	EditVert *eve;
	
	eve= G.edve.first;
	while(eve) {
		if(eve->f & 1) break;
		eve= eve->next;
	}
	if(eve==0) return;

	add_numbut(0, NUM|FLO, "LocX:", -G.vd->far, G.vd->far, eve->co, 0);
	add_numbut(1, NUM|FLO, "LocY:", -G.vd->far, G.vd->far, eve->co+1, 0);
	add_numbut(2, NUM|FLO, "LocZ:", -G.vd->far, G.vd->far, eve->co+2, 0);
	
	do_clever_numbuts("Active Vertex", 3, REDRAW);
}

/* never used, see CVS */
/*  static void insert_radiogour(char *str) */

static void permutate(void *list, int num, int size, int *index)
{
	void *buf;
	int len;
	int i;

	len = num * size;

	buf = malloc(len);
	memcpy(buf, list, len);
	
	for (i = 0; i < num; i++) {
		memcpy((char *)list + (i * size), (char *)buf + (index[i] * size), size);
	}
	free(buf);
}

static MVert *mvertbase;
static MFace *mfacebase;

static int verg_mface(const void *v1, const void *v2)
{
	MFace *x1, *x2;

	MVert *ve1, *ve2;
	int i1, i2;

	i1 = ((int *) v1)[0];
	i2 = ((int *) v2)[0];
	
	x1 = mfacebase + i1;
	x2 = mfacebase + i2;

	ve1= mvertbase+x1->v1;
	ve2= mvertbase+x2->v1;
	
	if( ve1->co[2] > ve2->co[2] ) return 1;
	else if( ve1->co[2] < ve2->co[2]) return -1;
	return 0;
}


void sort_faces(void)
{
	Object *ob= OBACT;
	Mesh *me;
	
	int i, *index;
	
	if(ob==0) return;
	if(G.obedit) return;
	if(ob->type!=OB_MESH) return;
	
	if(okee("Sort Faces in Z")==0) return;
	me= ob->data;
	if(me->totface==0) return;

/*	create index list */
	index = (int *) malloc(sizeof(int) * me->totface);
	for (i = 0; i < me->totface; i++) {
		index[i] = i;
	}
	mvertbase= me->mvert;
	mfacebase = me->mface;

/* sort index list instead of faces itself 
   and apply this permutation to the face list plus
   to the texture faces */
	qsort(index, me->totface, sizeof(int), verg_mface);

	permutate(mfacebase, me->totface, sizeof(MFace), index);
	if (me->tface) 
		permutate(me->tface, me->totface, sizeof(TFace), index);

	free(index);

	allqueue(REDRAWVIEW3D, 0);
	makeDispList(G.obedit);
}

void vertices_to_sphere(void)
{
	EditVert *eve;
	Object *ob= OBACT;
	float *curs, len, vec[3], cent[3], fac, facm, imat[3][3], bmat[3][3];
	int tot;
	short perc=100;
	
	if(ob==0) return;
	TEST_EDITMESH
	
	if(button(&perc, 1, 100, "Percentage:")==0) return;
	fac= perc/100.0;
	facm= 1.0-fac;
	
	Mat3CpyMat4(bmat, ob->obmat);
	Mat3Inv(imat, bmat);

	/* centrum */
	curs= give_cursor();
	cent[0]= curs[0]-ob->obmat[3][0];
	cent[1]= curs[1]-ob->obmat[3][1];
	cent[2]= curs[2]-ob->obmat[3][2];
	Mat3MulVecfl(imat, cent);

	len= 0.0;
	tot= 0;
	eve= G.edve.first;
	while(eve) {
		if(eve->f & 1) {
			tot++;
			len+= VecLenf(cent, eve->co);
		}
		eve= eve->next;
	}
	len/=tot;
	
	if(len==0.0) len= 10.0;
	
	eve= G.edve.first;
	while(eve) {
		if(eve->f & 1) {
			vec[0]= eve->co[0]-cent[0];
			vec[1]= eve->co[1]-cent[1];
			vec[2]= eve->co[2]-cent[2];
			
			Normalise(vec);
			
			eve->co[0]= fac*(cent[0]+vec[0]*len) + facm*eve->co[0];
			eve->co[1]= fac*(cent[1]+vec[1]*len) + facm*eve->co[1];
			eve->co[2]= fac*(cent[2]+vec[2]*len) + facm*eve->co[2];
			
		}
		eve= eve->next;
	}
	
	allqueue(REDRAWVIEW3D, 0);
	makeDispList(G.obedit);
}

/* Got this from scanfill.c. You will need to juggle around the
 * callbacks for the scanfill.c code a bit for this to work. */
void fill_mesh(void)
{
	EditVert *eve,*v1;
	EditEdge *eed,*e1,*nexted;
	EditVlak *evl,*nextvl;
	short ok;

	if(G.obedit==0 || (G.obedit->type!=OB_MESH)) return;

	waitcursor(1);

	/* alle selected vertices kopieeren */
	eve= G.edve.first;
	while(eve) {
		if(eve->f & 1) {
			v1= BLI_addfillvert(eve->co);
			eve->vn= v1;
			v1->vn= eve;
			v1->h= 0;
		}
		eve= eve->next;
	}
	/* alle selected edges kopieeren */
	eed= G.eded.first;
	while(eed) {
		if( (eed->v1->f & 1) && (eed->v2->f & 1) ) {
			e1= BLI_addfilledge(eed->v1->vn, eed->v2->vn);
			e1->v1->h++; 
			e1->v2->h++;
		}
		eed= eed->next;
	}
	/* van alle selected vlakken vertices en edges verwijderen om dubbels te voorkomen */
	/* alle edges tellen punten op, vlakken trekken af,
	   edges met vertices ->h<2 verwijderen */
	evl= G.edvl.first;
	ok= 0;
	while(evl) {
		nextvl= evl->next;
		if( vlakselectedAND(evl, 1) ) {
			evl->v1->vn->h--;
			evl->v2->vn->h--;
			evl->v3->vn->h--;
			if(evl->v4) evl->v4->vn->h--;
			ok= 1;
			
		}
		evl= nextvl;
	}
	if(ok) {	/* er zijn vlakken geselecteerd */
		eed= filledgebase.first;
		while(eed) {
			nexted= eed->next;
			if(eed->v1->h<2 || eed->v2->h<2) {
				BLI_remlink(&filledgebase,eed);
			}
			eed= nexted;
		}
	}

	/* tijd=clock(); */
	
	/* to make edgefill work */
	BLI_setScanFillObjectRef(G.obedit);
	BLI_setScanFillColourRef(&G.obedit->actcol);

	ok= BLI_edgefill(0);

	/* printf("time: %d\n",(clock()-tijd)/1000); */

	if(ok) {
		evl= fillvlakbase.first;
		while(evl) {
			addvlaklist(evl->v1->vn, evl->v2->vn, evl->v3->vn, 0, evl);
			evl= evl->next;
		}
	}
	/* else printf("fill error\n"); */

	BLI_end_edgefill();

	waitcursor(0);

	countall();
	allqueue(REDRAWVIEW3D, 0);
	makeDispList(G.obedit);
}

/* ***************** */

/* this one for NOT in editmode 

(only used by external modules, that is, until now by the 
python NMesh module) 

TODO: Probably it's better to convert the mesh into a EditMesh, call
vertexnormals() and convert it back to a Mesh again.

*/

void vertexnormals_mesh(Mesh *me, float *extverts)
{
	MVert *mvert;
	MFace *mface;
	float n1[3], n2[3], n3[3], n4[3], co[4], fac1, fac2, fac3, fac4, *temp;
	float *f1, *f2, *f3, *f4, xn, yn, zn, *normals;
	float *v1, *v2, *v3, *v4, len, vnor[3];
	int a, testflip;

	if(me->totvert==0) return;

	testflip= (me->flag & ME_NOPUNOFLIP)==0;
	if((me->flag & ME_TWOSIDED)==0) testflip= 0;	/* grote hoeken */
	
	if(me->totface==0) {
		/* namaak puno's voor halopuno! */
		mvert= me->mvert;
		for(a=0; a<me->totvert; a++, mvert++) {
			VECCOPY(n1, mvert->co);
			Normalise(n1);
			mvert->no[0]= 32767.0*n1[0];
			mvert->no[1]= 32767.0*n1[1];
			mvert->no[2]= 32767.0*n1[2];
		}
		return;
	}

	normals= MEM_callocN(me->totvert*3*sizeof(float), "normals");
	
	/* berekenen cos hoeken en oppervlakte en optellen bij puno */
	mface= me->mface;
	mvert= me->mvert;
	for(a=0; a<me->totface; a++, mface++) {
		
		if(mface->v3==0) continue;
		
		if(extverts) {
			v1= extverts+3*mface->v1;
			v2= extverts+3*mface->v2;
			v3= extverts+3*mface->v3;
			v4= extverts+3*mface->v4;
		}
		else {		
			v1= (mvert+mface->v1)->co;
			v2= (mvert+mface->v2)->co;
			v3= (mvert+mface->v3)->co;
			v4= (mvert+mface->v4)->co;
		}
		
		VecSubf(n1, v2, v1);
		VecSubf(n2, v3, v2);
		Normalise(n1);
		Normalise(n2);

		if(mface->v4==0) {
			VecSubf(n3, v1, v3);
			Normalise(n3);
			
			co[0]= saacos(-n3[0]*n1[0]-n3[1]*n1[1]-n3[2]*n1[2]);
			co[1]= saacos(-n1[0]*n2[0]-n1[1]*n2[1]-n1[2]*n2[2]);
			co[2]= saacos(-n2[0]*n3[0]-n2[1]*n3[1]-n2[2]*n3[2]);
			
		}
		else {
			VecSubf(n3, v4, v3);
			VecSubf(n4, v1, v4);
			Normalise(n3);
			Normalise(n4);

			co[0]= saacos(-n4[0]*n1[0]-n4[1]*n1[1]-n4[2]*n1[2]);
			co[1]= saacos(-n1[0]*n2[0]-n1[1]*n2[1]-n1[2]*n2[2]);
			co[2]= saacos(-n2[0]*n3[0]-n2[1]*n3[1]-n2[2]*n3[2]);
			co[3]= saacos(-n3[0]*n4[0]-n3[1]*n4[1]-n3[2]*n4[2]);
		}
		
		CalcNormFloat(v1, v2, v3, vnor);
		
		temp= normals+3*mface->v1;
		if(testflip && contrpuntnorm(vnor, temp) ) co[0]= -co[0];
		temp[0]+= co[0]*vnor[0];
		temp[1]+= co[0]*vnor[1];
		temp[2]+= co[0]*vnor[2];
		
		temp= normals+3*mface->v2;
		if(testflip && contrpuntnorm(vnor, temp) ) co[1]= -co[1];
		temp[0]+= co[1]*vnor[0];
		temp[1]+= co[1]*vnor[1];
		temp[2]+= co[1]*vnor[2];
		
		temp= normals+3*mface->v3;
		if(testflip && contrpuntnorm(vnor, temp) ) co[2]= -co[2];
		temp[0]+= co[2]*vnor[0];
		temp[1]+= co[2]*vnor[1];
		temp[2]+= co[2]*vnor[2];
		
		if(mface->v4) {
			temp= normals+3*mface->v4;
			if(testflip && contrpuntnorm(vnor, temp) ) co[3]= -co[3];
			temp[0]+= co[3]*vnor[0];
			temp[1]+= co[3]*vnor[1];
			temp[2]+= co[3]*vnor[2];
		}
	}

	/* normaliseren puntnormalen */
	mvert= me->mvert;
	for(a=0; a<me->totvert; a++, mvert++) {
		len= Normalise(normals+3*a);
		if(len!=0.0) {
			VECCOPY(n1, normals+3*a);
			Normalise(n1);

			mvert->no[0]= 32767.0*n1[0];
			mvert->no[1]= 32767.0*n1[1];
			mvert->no[2]= 32767.0*n1[2];
		}
	}
	
	/* puntnormaal omklap-vlaggen voor bij shade */
	mface= me->mface;
	mvert= me->mvert;
	for(a=0; a<me->totface; a++, mface++) {
		mface->puno=0;			
		
		if(mface->v3==0) continue;
		
		if(extverts) {
			v1= extverts+3*mface->v1;
			v2= extverts+3*mface->v2;
			v3= extverts+3*mface->v3;
		}
		else {		
			v1= (mvert+mface->v1)->co;
			v2= (mvert+mface->v2)->co;
			v3= (mvert+mface->v3)->co;
		}

		CalcNormFloat(v1, v2, v3, vnor);

		if(testflip) {
			f1= normals + 3*mface->v1;
			f2= normals + 3*mface->v2;
			f3= normals + 3*mface->v3;

			fac1= vnor[0]*f1[0] + vnor[1]*f1[1] + vnor[2]*f1[2];
			if(fac1<0.0) {
				mface->puno = ME_FLIPV1;
			}
			fac2= vnor[0]*f2[0] + vnor[1]*f2[1] + vnor[2]*f2[2];
			if(fac2<0.0) {
				mface->puno += ME_FLIPV2;
			}
			fac3= vnor[0]*f3[0] + vnor[1]*f3[1] + vnor[2]*f3[2];
			if(fac3<0.0) {
				mface->puno += ME_FLIPV3;
			}
			if(mface->v4) {
				f4= normals + 3*mface->v4;
				fac4= vnor[0]*f4[0] + vnor[1]*f4[1] + vnor[2]*f4[2];
				if(fac4<0.0) {
					mface->puno += ME_FLIPV4;
				}
			}
		}
		/* proj voor cubemap! */
		xn= fabs(vnor[0]);
		yn= fabs(vnor[1]);
		zn= fabs(vnor[2]);
		
		if(zn>xn && zn>yn) mface->puno += ME_PROJXY;
		else if(yn>xn && yn>zn) mface->puno += ME_PROJXZ;
		else mface->puno += ME_PROJYZ;
		
	}
	
	MEM_freeN(normals);
}

/***/

static int editmesh_nfaces_selected(void)
{
	EditVlak *evl;
	int count= 0;

	for (evl= G.edvl.first; evl; evl= evl->next)
		if (vlakselectedAND(evl, SELECT))
			count++;

	return count;
}

static int editmesh_nvertices_selected(void)
{
	EditVert *eve;
	int count= 0;

	for (eve= G.edve.first; eve; eve= eve->next)
		if (eve->f & SELECT)
			count++;

	return count;
}

static void editmesh_calc_selvert_center(float cent_r[3])
{
	EditVert *eve;
	int nsel= 0;

	cent_r[0]= cent_r[1]= cent_r[0]= 0.0;

	for (eve= G.edve.first; eve; eve= eve->next) {
		if (eve->f & SELECT) {
			cent_r[0]+= eve->co[0];
			cent_r[1]+= eve->co[1];
			cent_r[2]+= eve->co[2];
			nsel++;
		}
	}

	if (nsel) {
		cent_r[0]/= nsel;
		cent_r[1]/= nsel;
		cent_r[2]/= nsel;
	}
}

static int tface_is_selected(TFace *tf)
{
	return (!(tf->flag & TF_HIDE) && (tf->flag & TF_SELECT));
}

static int faceselect_nfaces_selected(Mesh *me)
{
	int i, count= 0;

	for (i=0; i<me->totface; i++) {
		MFace *mf= ((MFace*) me->mface) + i;
		TFace *tf= ((TFace*) me->tface) + i;

		if (mf->v3 && tface_is_selected(tf))
			count++;
	}

	return count;
}

	/* XXX, code for both these functions should be abstract,
	 * then unified, then written for other things (like objects,
	 * which would use same as vertices method), then added
	 * to interface! Hoera! - zr
	 */
void faceselect_align_view_to_selected(View3D *v3d, Mesh *me, int axis)
{
	if (!faceselect_nfaces_selected(me)) {
		error("No faces selected.");
	} else {
		float norm[3];
		int i;

		norm[0]= norm[1]= norm[2]= 0.0;
		for (i=0; i<me->totface; i++) {
			MFace *mf= ((MFace*) me->mface) + i;
			TFace *tf= ((TFace*) me->tface) + i;
	
			if (mf->v3 && tface_is_selected(tf)) {
				float *v1, *v2, *v3, fno[3];

				v1= me->mvert[mf->v1].co;
				v2= me->mvert[mf->v2].co;
				v3= me->mvert[mf->v3].co;
				if (mf->v4) {
					float *v4= me->mvert[mf->v4].co;
					CalcNormFloat4(v1, v2, v3, v4, fno);
				} else {
					CalcNormFloat(v1, v2, v3, fno);
				}

				norm[0]+= fno[0];
				norm[1]+= fno[1];
				norm[2]+= fno[2];
			}
		}

		view3d_align_axis_to_vector(v3d, axis, norm);
	}
}

void editmesh_align_view_to_selected(View3D *v3d, int axis)
{
	int nselverts= editmesh_nvertices_selected();

	if (nselverts<3) {
		if (nselverts==0) {
			error("No faces or vertices selected.");
		} else {
			error("At least one face or three vertices must be selected.");
		}
	} else if (editmesh_nfaces_selected()) {
		float norm[3];
		EditVlak *evl;

		norm[0]= norm[1]= norm[2]= 0.0;
		for (evl= G.edvl.first; evl; evl= evl->next) {
			if (vlakselectedAND(evl, SELECT)) {
				float fno[3];
				if (evl->v4) CalcNormFloat4(evl->v1->co, evl->v2->co, evl->v3->co, evl->v4->co, fno);
				else CalcNormFloat(evl->v1->co, evl->v2->co, evl->v3->co, fno);
						/* XXX, fixme, should be flipped intp a 
						 * consistent direction. -zr
						 */
				norm[0]+= fno[0];
				norm[1]+= fno[1];
				norm[2]+= fno[2];
			}
		}

		view3d_align_axis_to_vector(v3d, axis, norm);
	} else {
		float cent[3], norm[3];
		EditVert *eve, *leve= NULL;

		norm[0]= norm[1]= norm[2]= 0.0;
		editmesh_calc_selvert_center(cent);
		for (eve= G.edve.first; eve; eve= eve->next) {
			if (eve->f & SELECT) {
				if (leve) {
					float tno[3];
					CalcNormFloat(cent, leve->co, eve->co, tno);
					
						/* XXX, fixme, should be flipped intp a 
						 * consistent direction. -zr
						 */
					norm[0]+= tno[0];
					norm[1]+= tno[1];
					norm[2]+= tno[2];
				}
				leve= eve;
			}
		}

		view3d_align_axis_to_vector(v3d, axis, norm);
	}
}
