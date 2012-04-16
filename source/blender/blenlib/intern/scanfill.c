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
 * The Original Code is Copyright (C) 2001-2002 by NaN Holding BV.
 * All rights reserved.
 *
 * The Original Code is: all of this file.
 *
 * Contributor(s): none yet.
 *
 * ***** END GPL LICENSE BLOCK *****
 * (uit traces) maart 95
 */

/** \file blender/blenlib/intern/scanfill.c
 *  \ingroup bli
 */

#include <stdio.h>
#include <math.h>
#include <stdlib.h>
#include <string.h>

#include "MEM_guardedalloc.h"

#include "BLI_callbacks.h"
#include "BLI_listbase.h"
#include "BLI_math.h"
#include "BLI_scanfill.h"
#include "BLI_utildefines.h"

/* callbacks for errors and interrupts and some goo */
static void (*BLI_localErrorCallBack)(const char *) = NULL;
static int (*BLI_localInterruptCallBack)(void) = NULL;

void BLI_setErrorCallBack(void (*f)(const char *))
{
	BLI_localErrorCallBack = f;
}

void BLI_setInterruptCallBack(int (*f)(void))
{
	BLI_localInterruptCallBack = f;
}

/* just flush the error to /dev/null if the error handler is missing */
void callLocalErrorCallBack(const char *msg)
{
	if (BLI_localErrorCallBack) {
		BLI_localErrorCallBack(msg);
	}
}

#if 0
/* ignore if the interrupt wasn't set */
static int callLocalInterruptCallBack(void)
{
	if (BLI_localInterruptCallBack) {
		return BLI_localInterruptCallBack();
	}
	else {
		return 0;
	}
}
#endif

/* local types */
typedef struct PolyFill {
	int edges, verts;
	float min_xy[2], max_xy[2];
	short f, nr;
} PolyFill;

typedef struct ScanFillVertLink {
	ScanFillVert *v1;
	ScanFillEdge *first, *last;
} ScanFillVertLink;


/* local funcs */

#define COMPLIMIT   0.00003f

/* ****  FUNCTIONS FOR QSORT *************************** */


static int vergscdata(const void *a1, const void *a2)
{
	const ScanFillVertLink *x1 = a1, *x2 = a2;
	
	if      (x1->v1->xy[1] < x2->v1->xy[1]) return  1;
	else if (x1->v1->xy[1] > x2->v1->xy[1]) return -1;
	else if (x1->v1->xy[0] > x2->v1->xy[0]) return  1;
	else if (x1->v1->xy[0] < x2->v1->xy[0]) return -1;

	return 0;
}

static int vergpoly(const void *a1, const void *a2)
{
	const PolyFill *x1 = a1, *x2 = a2;

	if      (x1->min_xy[0] > x2->min_xy[0]) return  1;
	else if (x1->min_xy[0] < x2->min_xy[0]) return -1;
	else if (x1->min_xy[1] > x2->min_xy[1]) return  1;
	else if (x1->min_xy[1] < x2->min_xy[1]) return -1;
	
	return 0;
}

/* ************* MEMORY MANAGEMENT ************* */

/* memory management */
struct mem_elements {
	struct mem_elements *next, *prev;
	char *data;
};

static void *mem_element_new(ScanFillContext *sf_ctx, int size)
{
	BLI_assert(!(size > 10000 || size == 0)); /* this is invalid use! */

	size = (size + 3) & ~3;     /* allocate in units of 4 */
	
	if (sf_ctx->melem__cur && (size + sf_ctx->melem__offs < MEM_ELEM_BLOCKSIZE)) {
		void *adr = (void *) (sf_ctx->melem__cur->data + sf_ctx->melem__offs);
		sf_ctx->melem__offs += size;
		return adr;
	}
	else {
		sf_ctx->melem__cur = MEM_callocN(sizeof(struct mem_elements), "newmem");
		sf_ctx->melem__cur->data = MEM_callocN(MEM_ELEM_BLOCKSIZE, "newmem");
		BLI_addtail(&sf_ctx->melem__lb, sf_ctx->melem__cur);

		sf_ctx->melem__offs = size;
		return sf_ctx->melem__cur->data;
	}
}
static void mem_element_reset(ScanFillContext *sf_ctx, int keep_first)
{
	struct mem_elements *first;

	if ((first = sf_ctx->melem__lb.first)) { /* can be false if first fill fails */
		if (keep_first) {
			BLI_remlink(&sf_ctx->melem__lb, first);
		}

		sf_ctx->melem__cur = sf_ctx->melem__lb.first;
		while (sf_ctx->melem__cur) {
			MEM_freeN(sf_ctx->melem__cur->data);
			sf_ctx->melem__cur = sf_ctx->melem__cur->next;
		}
		BLI_freelistN(&sf_ctx->melem__lb);

		/*reset the block we're keeping*/
		if (keep_first) {
			BLI_addtail(&sf_ctx->melem__lb, first);
			memset(first->data, 0, MEM_ELEM_BLOCKSIZE);
		}
		else {
			first = NULL;

		}
	}

	sf_ctx->melem__cur = first;
	sf_ctx->melem__offs = 0;
}

void BLI_end_edgefill(ScanFillContext *sf_ctx)
{
	 mem_element_reset(sf_ctx, FALSE);
	
	sf_ctx->fillvertbase.first = sf_ctx->fillvertbase.last = NULL;
	sf_ctx->filledgebase.first = sf_ctx->filledgebase.last = NULL;
	sf_ctx->fillfacebase.first = sf_ctx->fillfacebase.last = NULL;
}

/* ****  FILL ROUTINES *************************** */

ScanFillVert *BLI_addfillvert(ScanFillContext *sf_ctx, const float vec[3])
{
	ScanFillVert *eve;
	
	eve = mem_element_new(sf_ctx, sizeof(ScanFillVert));
	BLI_addtail(&sf_ctx->fillvertbase, eve);
	
	eve->co[0] = vec[0];
	eve->co[1] = vec[1];
	eve->co[2] = vec[2];

	return eve;	
}

ScanFillEdge *BLI_addfilledge(ScanFillContext *sf_ctx, ScanFillVert *v1, ScanFillVert *v2)
{
	ScanFillEdge *newed;

	newed = mem_element_new(sf_ctx, sizeof(ScanFillEdge));
	BLI_addtail(&sf_ctx->filledgebase, newed);
	
	newed->v1 = v1;
	newed->v2 = v2;

	return newed;
}

static void addfillface(ScanFillContext *sf_ctx, ScanFillVert *v1, ScanFillVert *v2, ScanFillVert *v3)
{
	/* does not make edges */
	ScanFillFace *evl;

	evl = mem_element_new(sf_ctx, sizeof(ScanFillFace));
	BLI_addtail(&sf_ctx->fillfacebase, evl);
	
	evl->v1 = v1;
	evl->v2 = v2;
	evl->v3 = v3;
}

static int boundisect(PolyFill *pf2, PolyFill *pf1)
{
	/* has pf2 been touched (intersected) by pf1 ? with bounding box */
	/* test first if polys exist */

	if (pf1->edges == 0 || pf2->edges == 0) return 0;

	if (pf2->max_xy[0] < pf1->min_xy[0]) return 0;
	if (pf2->max_xy[1] < pf1->min_xy[1]) return 0;

	if (pf2->min_xy[0] > pf1->max_xy[0]) return 0;
	if (pf2->min_xy[1] > pf1->max_xy[1]) return 0;

	/* join */
	if (pf2->max_xy[0] < pf1->max_xy[0]) pf2->max_xy[0] = pf1->max_xy[0];
	if (pf2->max_xy[1] < pf1->max_xy[1]) pf2->max_xy[1] = pf1->max_xy[1];

	if (pf2->min_xy[0] > pf1->min_xy[0]) pf2->min_xy[0] = pf1->min_xy[0];
	if (pf2->min_xy[1] > pf1->min_xy[1]) pf2->min_xy[1] = pf1->min_xy[1];

	return 1;
}


static void mergepolysSimp(ScanFillContext *sf_ctx, PolyFill *pf1, PolyFill *pf2)    /* add pf2 to pf1 */
{
	ScanFillVert *eve;
	ScanFillEdge *eed;

	/* replace old poly numbers */
	eve = sf_ctx->fillvertbase.first;
	while (eve) {
		if (eve->poly_nr == pf2->nr) eve->poly_nr = pf1->nr;
		eve = eve->next;
	}
	eed = sf_ctx->filledgebase.first;
	while (eed) {
		if (eed->poly_nr == pf2->nr) eed->poly_nr = pf1->nr;
		eed = eed->next;
	}

	pf1->verts += pf2->verts;
	pf1->edges += pf2->edges;
	pf2->verts = pf2->edges = 0;
	pf1->f = (pf1->f | pf2->f);
}

static short testedgeside(const float v1[2], const float v2[2], const float v3[2])
/* is v3 to the right of v1-v2 ? With exception: v3==v1 || v3==v2 */
{
	float inp;

	inp = (v2[0] - v1[0]) * (v1[1] - v3[1]) +
	      (v1[1] - v2[1]) * (v1[0] - v3[0]);

	if (inp < 0.0f) {
		return 0;
	}
	else if (inp == 0) {
		if (v1[0] == v3[0] && v1[1] == v3[1]) return 0;
		if (v2[0] == v3[0] && v2[1] == v3[1]) return 0;
	}
	return 1;
}

static short addedgetoscanvert(ScanFillVertLink *sc, ScanFillEdge *eed)
{
	/* find first edge to the right of eed, and insert eed before that */
	ScanFillEdge *ed;
	float fac, fac1, x, y;

	if (sc->first == NULL) {
		sc->first = sc->last = eed;
		eed->prev = eed->next = NULL;
		return 1;
	}

	x = eed->v1->xy[0];
	y = eed->v1->xy[1];

	fac1 = eed->v2->xy[1] - y;
	if (fac1 == 0.0f) {
		fac1 = 1.0e10f * (eed->v2->xy[0] - x);

	}
	else fac1 = (x - eed->v2->xy[0]) / fac1;

	ed = sc->first;
	while (ed) {

		if (ed->v2 == eed->v2) return 0;

		fac = ed->v2->xy[1] - y;
		if (fac == 0.0f) {
			fac = 1.0e10f * (ed->v2->xy[0] - x);
		}
		else {
			fac = (x - ed->v2->xy[0]) / fac;
		}

		if (fac > fac1) break;

		ed = ed->next;
	}
	if (ed) BLI_insertlinkbefore((ListBase *)&(sc->first), ed, eed);
	else BLI_addtail((ListBase *)&(sc->first), eed);

	return 1;
}


static ScanFillVertLink *addedgetoscanlist(ScanFillContext *sf_ctx, ScanFillEdge *eed, int len)
{
	/* inserts edge at correct location in ScanFillVertLink list */
	/* returns sc when edge already exists */
	ScanFillVertLink *sc, scsearch;
	ScanFillVert *eve;

	/* which vert is left-top? */
	if (eed->v1->xy[1] == eed->v2->xy[1]) {
		if (eed->v1->xy[0] > eed->v2->xy[0]) {
			eve = eed->v1;
			eed->v1 = eed->v2;
			eed->v2 = eve;
		}
	}
	else if (eed->v1->xy[1] < eed->v2->xy[1]) {
		eve = eed->v1;
		eed->v1 = eed->v2;
		eed->v2 = eve;
	}
	/* find location in list */
	scsearch.v1 = eed->v1;
	sc = (ScanFillVertLink *)bsearch(&scsearch, sf_ctx->_scdata, len,
	                                 sizeof(ScanFillVertLink), vergscdata);

	if (sc == 0) printf("Error in search edge: %p\n", (void *)eed);
	else if (addedgetoscanvert(sc, eed) == 0) return sc;

	return 0;
}

static short boundinsideEV(ScanFillEdge *eed, ScanFillVert *eve)
/* is eve inside boundbox eed */
{
	float minx, maxx, miny, maxy;

	if (eed->v1->xy[0] < eed->v2->xy[0]) {
		minx = eed->v1->xy[0];
		maxx = eed->v2->xy[0];
	}
	else {
		minx = eed->v2->xy[0];
		maxx = eed->v1->xy[0];
	}
	if (eve->xy[0] >= minx && eve->xy[0] <= maxx) {
		if (eed->v1->xy[1] < eed->v2->xy[1]) {
			miny = eed->v1->xy[1];
			maxy = eed->v2->xy[1];
		}
		else {
			miny = eed->v2->xy[1];
			maxy = eed->v1->xy[1];
		}
		if (eve->xy[1] >= miny && eve->xy[1] <= maxy) return 1;
	}
	return 0;
}


static void testvertexnearedge(ScanFillContext *sf_ctx)
{
	/* only vertices with ->h==1 are being tested for
	 * being close to an edge, if true insert */

	ScanFillVert *eve;
	ScanFillEdge *eed, *ed1;
	float dist, vec1[2], vec2[2], vec3[2];

	eve = sf_ctx->fillvertbase.first;
	while (eve) {
		if (eve->h == 1) {
			vec3[0] = eve->xy[0];
			vec3[1] = eve->xy[1];
			/* find the edge which has vertex eve */
			ed1 = sf_ctx->filledgebase.first;
			while (ed1) {
				if (ed1->v1 == eve || ed1->v2 == eve) break;
				ed1 = ed1->next;
			}
			if (ed1->v1 == eve) {
				ed1->v1 = ed1->v2;
				ed1->v2 = eve;
			}
			eed = sf_ctx->filledgebase.first;
			while (eed) {
				if (eve != eed->v1 && eve != eed->v2 && eve->poly_nr == eed->poly_nr) {
					if (compare_v3v3(eve->co, eed->v1->co, COMPLIMIT)) {
						ed1->v2 = eed->v1;
						eed->v1->h++;
						eve->h = 0;
						break;
					}
					else if (compare_v3v3(eve->co, eed->v2->co, COMPLIMIT)) {
						ed1->v2 = eed->v2;
						eed->v2->h++;
						eve->h = 0;
						break;
					}
					else {
						vec1[0] = eed->v1->xy[0];
						vec1[1] = eed->v1->xy[1];
						vec2[0] = eed->v2->xy[0];
						vec2[1] = eed->v2->xy[1];
						if (boundinsideEV(eed, eve)) {
							dist = dist_to_line_v2(vec1, vec2, vec3);
							if (dist < COMPLIMIT) {
								/* new edge */
								ed1 = BLI_addfilledge(sf_ctx, eed->v1, eve);
								
								/* printf("fill: vertex near edge %x\n",eve); */
								ed1->f = 0;
								ed1->poly_nr = eed->poly_nr;
								eed->v1 = eve;
								eve->h = 3;
								break;
							}
						}
					}
				}
				eed = eed->next;
			}
		}
		eve = eve->next;
	}
}

static void splitlist(ScanFillContext *sf_ctx, ListBase *tempve, ListBase *temped, short nr)
{
	/* everything is in templist, write only poly nr to fillist */
	ScanFillVert *eve, *nextve;
	ScanFillEdge *eed, *nexted;

	BLI_movelisttolist(tempve, &sf_ctx->fillvertbase);
	BLI_movelisttolist(temped, &sf_ctx->filledgebase);

	eve = tempve->first;
	while (eve) {
		nextve = eve->next;
		if (eve->poly_nr == nr) {
			BLI_remlink(tempve, eve);
			BLI_addtail(&sf_ctx->fillvertbase, eve);
		}
		eve = nextve;
	}
	eed = temped->first;
	while (eed) {
		nexted = eed->next;
		if (eed->poly_nr == nr) {
			BLI_remlink(temped, eed);
			BLI_addtail(&sf_ctx->filledgebase, eed);
		}
		eed = nexted;
	}
}


static int scanfill(ScanFillContext *sf_ctx, PolyFill *pf)
{
	ScanFillVertLink *sc = NULL, *sc1;
	ScanFillVert *eve, *v1, *v2, *v3;
	ScanFillEdge *eed, *nexted, *ed1, *ed2, *ed3;
	float miny = 0.0;
	int a, b, verts, maxface, totface;
	short nr, test, twoconnected = 0;

	nr = pf->nr;

	/* PRINTS */
#if 0
	verts = pf->verts;
	eve = sf_ctx->fillvertbase.first;
	while (eve) {
		printf("vert: %x co: %f %f\n", eve, eve->xy[0], eve->xy[1]);
		eve = eve->next;
	}	
	eed = sf_ctx->filledgebase.first;
	while (eed) {
		printf("edge: %x  verts: %x %x\n", eed, eed->v1, eed->v2);
		eed = eed->next;
	}
#endif

	/* STEP 0: remove zero sized edges */
	eed = sf_ctx->filledgebase.first;
	while (eed) {
		if (eed->v1->xy[0] == eed->v2->xy[0]) {
			if (eed->v1->xy[1] == eed->v2->xy[1]) {
				if (eed->v1->f == 255 && eed->v2->f != 255) {
					eed->v2->f = 255;
					eed->v2->tmp.v = eed->v1->tmp.v;
				}
				else if (eed->v2->f == 255 && eed->v1->f != 255) {
					eed->v1->f = 255;
					eed->v1->tmp.v = eed->v2->tmp.v;
				}
				else if (eed->v2->f == 255 && eed->v1->f == 255) {
					eed->v1->tmp.v = eed->v2->tmp.v;
				}
				else {
					eed->v2->f = 255;
					eed->v2->tmp.v = eed->v1;
				}
			}
		}
		eed = eed->next;
	}

	/* STEP 1: make using FillVert and FillEdge lists a sorted
	 * ScanFillVertLink list
	 */
	sc = sf_ctx->_scdata = (ScanFillVertLink *)MEM_callocN(pf->verts * sizeof(ScanFillVertLink), "Scanfill1");
	eve = sf_ctx->fillvertbase.first;
	verts = 0;
	while (eve) {
		if (eve->poly_nr == nr) {
			if (eve->f != 255) {
				verts++;
				eve->f = 0;  /* flag for connectedges later on */
				sc->v1 = eve;
				sc++;
			}
		}
		eve = eve->next;
	}

	qsort(sf_ctx->_scdata, verts, sizeof(ScanFillVertLink), vergscdata);

	eed = sf_ctx->filledgebase.first;
	while (eed) {
		nexted = eed->next;
		BLI_remlink(&sf_ctx->filledgebase, eed);
		/* This code is for handling zero-length edges that get
		 * collapsed in step 0. It was removed for some time to
		 * fix trunk bug #4544, so if that comes back, this code
		 * may need some work, or there will have to be a better
		 * fix to #4544. */
		if (eed->v1->f == 255) {
			v1 = eed->v1;
			while ((eed->v1->f == 255) && (eed->v1->tmp.v != v1)) 
				eed->v1 = eed->v1->tmp.v;
		}
		if (eed->v2->f == 255) {
			v2 = eed->v2;
			while ((eed->v2->f == 255) && (eed->v2->tmp.v != v2))
				eed->v2 = eed->v2->tmp.v;
		}
		if (eed->v1 != eed->v2) addedgetoscanlist(sf_ctx, eed, verts);

		eed = nexted;
	}
#if 0
	sc = scdata;
	for (a = 0; a < verts; a++) {
		printf("\nscvert: %x\n", sc->v1);
		eed = sc->first;
		while (eed) {
			printf(" ed %x %x %x\n", eed, eed->v1, eed->v2);
			eed = eed->next;
		}
		sc++;
	}
#endif


	/* STEP 2: FILL LOOP */

	if (pf->f == 0) twoconnected = 1;

	/* (temporal) security: never much more faces than vertices */
	totface = 0;
	maxface = 2 * verts;       /* 2*verts: based at a filled circle within a triangle */

	sc = sf_ctx->_scdata;
	for (a = 0; a < verts; a++) {
		/* printf("VERTEX %d %x\n",a,sc->v1); */
		ed1 = sc->first;
		while (ed1) {   /* set connectflags  */
			nexted = ed1->next;
			if (ed1->v1->h == 1 || ed1->v2->h == 1) {
				BLI_remlink((ListBase *)&(sc->first), ed1);
				BLI_addtail(&sf_ctx->filledgebase, ed1);
				if (ed1->v1->h > 1) ed1->v1->h--;
				if (ed1->v2->h > 1) ed1->v2->h--;
			}
			else ed1->v2->f = 1;

			ed1 = nexted;
		}
		while (sc->first) { /* for as long there are edges */
			ed1 = sc->first;
			ed2 = ed1->next;
			
			/* commented out... the ESC here delivers corrupted memory (and doesnt work during grab) */
			/* if (callLocalInterruptCallBack()) break; */
			if (totface > maxface) {
				/* printf("Fill error: endless loop. Escaped at vert %d,  tot: %d.\n", a, verts); */
				a = verts;
				break;
			}
			if (ed2 == 0) {
				sc->first = sc->last = NULL;
				/* printf("just 1 edge to vert\n"); */
				BLI_addtail(&sf_ctx->filledgebase, ed1);
				ed1->v2->f = 0;
				ed1->v1->h--; 
				ed1->v2->h--;
			}
			else {
				/* test rest of vertices */
				v1 = ed1->v2;
				v2 = ed1->v1;
				v3 = ed2->v2;
				/* this happens with a serial of overlapping edges */
				if (v1 == v2 || v2 == v3) break;
				/* printf("test verts %x %x %x\n",v1,v2,v3); */
				miny = ( (v1->xy[1]) < (v3->xy[1]) ? (v1->xy[1]) : (v3->xy[1]) );
				/*  miny= MIN2(v1->xy[1],v3->xy[1]); */
				sc1 = sc + 1;
				test = 0;

				for (b = a + 1; b < verts; b++) {
					if (sc1->v1->f == 0) {
						if (sc1->v1->xy[1] <= miny) break;

						if (testedgeside(v1->xy, v2->xy, sc1->v1->xy))
							if (testedgeside(v2->xy, v3->xy, sc1->v1->xy))
								if (testedgeside(v3->xy, v1->xy, sc1->v1->xy)) {
									/* point in triangle */
								
									test = 1;
									break;
								}
					}
					sc1++;
				}
				if (test) {
					/* make new edge, and start over */
					/* printf("add new edge %x %x and start again\n",v2,sc1->v1); */

					ed3 = BLI_addfilledge(sf_ctx, v2, sc1->v1);
					BLI_remlink(&sf_ctx->filledgebase, ed3);
					BLI_insertlinkbefore((ListBase *)&(sc->first), ed2, ed3);
					ed3->v2->f = 1;
					ed3->f = 2;
					ed3->v1->h++; 
					ed3->v2->h++;
				}
				else {
					/* new triangle */
					/* printf("add face %x %x %x\n",v1,v2,v3); */
					addfillface(sf_ctx, v1, v2, v3);
					totface++;
					BLI_remlink((ListBase *)&(sc->first), ed1);
					BLI_addtail(&sf_ctx->filledgebase, ed1);
					ed1->v2->f = 0;
					ed1->v1->h--; 
					ed1->v2->h--;
					/* ed2 can be removed when it's a boundary edge */
					if ((ed2->f == 0 && twoconnected) || (ed2->f == FILLBOUNDARY)) {
						BLI_remlink((ListBase *)&(sc->first), ed2);
						BLI_addtail(&sf_ctx->filledgebase, ed2);
						ed2->v2->f = 0;
						ed2->v1->h--; 
						ed2->v2->h--;
					}

					/* new edge */
					ed3 = BLI_addfilledge(sf_ctx, v1, v3);
					BLI_remlink(&sf_ctx->filledgebase, ed3);
					ed3->f = 2;
					ed3->v1->h++; 
					ed3->v2->h++;
					
					/* printf("add new edge %x %x\n",v1,v3); */
					sc1 = addedgetoscanlist(sf_ctx, ed3, verts);
					
					if (sc1) {  /* ed3 already exists: remove if a boundary */
						/* printf("Edge exists\n"); */
						ed3->v1->h--; 
						ed3->v2->h--;

						ed3 = sc1->first;
						while (ed3) {
							if ( (ed3->v1 == v1 && ed3->v2 == v3) || (ed3->v1 == v3 && ed3->v2 == v1) ) {
								if (twoconnected || ed3->f == FILLBOUNDARY) {
									BLI_remlink((ListBase *)&(sc1->first), ed3);
									BLI_addtail(&sf_ctx->filledgebase, ed3);
									ed3->v1->h--; 
									ed3->v2->h--;
								}
								break;
							}
							ed3 = ed3->next;
						}
					}

				}
			}
			/* test for loose edges */
			ed1 = sc->first;
			while (ed1) {
				nexted = ed1->next;
				if (ed1->v1->h < 2 || ed1->v2->h < 2) {
					BLI_remlink((ListBase *)&(sc->first), ed1);
					BLI_addtail(&sf_ctx->filledgebase, ed1);
					if (ed1->v1->h > 1) ed1->v1->h--;
					if (ed1->v2->h > 1) ed1->v2->h--;
				}

				ed1 = nexted;
			}
		}
		sc++;
	}

	MEM_freeN(sf_ctx->_scdata);
	sf_ctx->_scdata = NULL;

	return totface;
}


int BLI_begin_edgefill(ScanFillContext *sf_ctx)
{
	memset(sf_ctx, 0, sizeof(*sf_ctx));

	return 1;
}

int BLI_edgefill(ScanFillContext *sf_ctx, const short do_quad_tri_speedup)
{
	/*
	 * - fill works with its own lists, so create that first (no faces!)
	 * - for vertices, put in ->tmp.v the old pointer
	 * - struct elements xs en ys are not used here: don't hide stuff in it
	 * - edge flag ->f becomes 2 when it's a new edge
	 * - mode: & 1 is check for crossings, then create edges (TO DO )
	 * - returns number of triangle faces added.
	 */
	ListBase tempve, temped;
	ScanFillVert *eve;
	ScanFillEdge *eed, *nexted;
	PolyFill *pflist, *pf;
	float *min_xy_p, *max_xy_p;
	short a, c, poly = 0, ok = 0, toggle = 0;
	int totfaces = 0; /* total faces added */
	int co_x, co_y;

	/* reset variables */
	eve = sf_ctx->fillvertbase.first;
	a = 0;
	while (eve) {
		eve->f = 0;
		eve->poly_nr = 0;
		eve->h = 0;
		eve = eve->next;
		a += 1;
	}

	if (do_quad_tri_speedup && (a == 3)) {
		eve = sf_ctx->fillvertbase.first;

		addfillface(sf_ctx, eve, eve->next, eve->next->next);
		return 1;
	}
	else if (do_quad_tri_speedup && (a == 4)) {
		float vec1[3], vec2[3];

		eve = sf_ctx->fillvertbase.first;
		/* no need to check 'eve->next->next->next' is valid, already counted */
		/* use shortest diagonal for quad */
		sub_v3_v3v3(vec1, eve->co, eve->next->next->co);
		sub_v3_v3v3(vec2, eve->next->co, eve->next->next->next->co);

		if (dot_v3v3(vec1, vec1) < dot_v3v3(vec2, vec2)) {
			addfillface(sf_ctx, eve, eve->next, eve->next->next);
			addfillface(sf_ctx, eve->next->next, eve->next->next->next, eve);
		}
		else {
			addfillface(sf_ctx, eve->next, eve->next->next, eve->next->next->next);
			addfillface(sf_ctx, eve->next->next->next, eve, eve->next);
		}
		return 2;
	}

	/* first test vertices if they are in edges */
	/* including resetting of flags */
	eed = sf_ctx->filledgebase.first;
	while (eed) {
		eed->poly_nr = 0;
		eed->v1->f = 1;
		eed->v2->f = 1;

		eed = eed->next;
	}

	eve = sf_ctx->fillvertbase.first;
	while (eve) {
		if (eve->f & 1) {
			ok = 1;
			break;
		}
		eve = eve->next;
	}

	if (ok == 0) {
		return 0;
	}
	else {
		/* define projection: with 'best' normal */
		/* Newell's Method */
		/* Similar code used elsewhere, but this checks for double ups
		 * which historically this function supports so better not change */
		float *v_prev;
		float n[3] = {0.0f};

		eve = sf_ctx->fillvertbase.last;
		v_prev = eve->co;

		for (eve = sf_ctx->fillvertbase.first; eve; eve = eve->next) {
			if (LIKELY(!compare_v3v3(v_prev, eve->co, COMPLIMIT))) {
				n[0] += (v_prev[1] - eve->co[1]) * (v_prev[2] + eve->co[2]);
				n[1] += (v_prev[2] - eve->co[2]) * (v_prev[0] + eve->co[0]);
				n[2] += (v_prev[0] - eve->co[0]) * (v_prev[1] + eve->co[1]);
			}
			v_prev = eve->co;
		}

		if (UNLIKELY(normalize_v3(n) == 0.0f)) {
			n[2] = 1.0f; /* other axis set to 0.0 */
		}

		axis_dominant_v3(&co_x, &co_y, n);
	}


	/* STEP 1: COUNT POLYS */
	eve = sf_ctx->fillvertbase.first;
	while (eve) {
		eve->xy[0] = eve->co[co_x];
		eve->xy[1] = eve->co[co_y];

		/* get first vertex with no poly number */
		if (eve->poly_nr == 0) {
			poly++;
			/* now a sortof select connected */
			ok = 1;
			eve->poly_nr = poly;
			
			while (ok) {
				
				ok = 0;
				toggle++;
				if (toggle & 1) eed = sf_ctx->filledgebase.first;
				else eed = sf_ctx->filledgebase.last;

				while (eed) {
					if (eed->v1->poly_nr == 0 && eed->v2->poly_nr == poly) {
						eed->v1->poly_nr = poly;
						eed->poly_nr = poly;
						ok = 1;
					}
					else if (eed->v2->poly_nr == 0 && eed->v1->poly_nr == poly) {
						eed->v2->poly_nr = poly;
						eed->poly_nr = poly;
						ok = 1;
					}
					else if (eed->poly_nr == 0) {
						if (eed->v1->poly_nr == poly && eed->v2->poly_nr == poly) {
							eed->poly_nr = poly;
							ok = 1;
						}
					}
					if (toggle & 1) eed = eed->next;
					else eed = eed->prev;
				}
			}
		}
		eve = eve->next;
	}
	/* printf("amount of poly's: %d\n",poly); */

	/* STEP 2: remove loose edges and strings of edges */
	eed = sf_ctx->filledgebase.first;
	while (eed) {
		if (eed->v1->h++ > 250) break;
		if (eed->v2->h++ > 250) break;
		eed = eed->next;
	}
	if (eed) {
		/* otherwise it's impossible to be sure you can clear vertices */
		callLocalErrorCallBack("No vertices with 250 edges allowed!");
		return 0;
	}
	
	/* does it only for vertices with ->h==1 */
	testvertexnearedge(sf_ctx);

	ok = 1;
	while (ok) {
		ok = 0;
		toggle++;
		if (toggle & 1) eed = sf_ctx->filledgebase.first;
		else eed = sf_ctx->filledgebase.last;
		while (eed) {
			if (toggle & 1) nexted = eed->next;
			else nexted = eed->prev;
			if (eed->v1->h == 1) {
				eed->v2->h--;
				BLI_remlink(&sf_ctx->fillvertbase, eed->v1);
				BLI_remlink(&sf_ctx->filledgebase, eed);
				ok = 1;
			}
			else if (eed->v2->h == 1) {
				eed->v1->h--;
				BLI_remlink(&sf_ctx->fillvertbase, eed->v2);
				BLI_remlink(&sf_ctx->filledgebase, eed);
				ok = 1;
			}
			eed = nexted;
		}
	}
	if (sf_ctx->filledgebase.first == 0) {
		/* printf("All edges removed\n"); */
		return 0;
	}


	/* CURRENT STATUS:
	 * - eve->f       :1= availalble in edges
	 * - eve->xs      :polynumber
	 * - eve->h       :amount of edges connected to vertex
	 * - eve->tmp.v   :store! original vertex number
	 * 
	 * - eed->f       :1= boundary edge (optionally set by caller)
	 * - eed->poly_nr :poly number
	 */


	/* STEP 3: MAKE POLYFILL STRUCT */
	pflist = (PolyFill *)MEM_callocN(poly * sizeof(PolyFill), "edgefill");
	pf = pflist;
	for (a = 1; a <= poly; a++) {
		pf->nr = a;
		pf->min_xy[0] = pf->min_xy[1] =  1.0e20;
		pf->max_xy[0] = pf->max_xy[1] = -1.0e20;
		pf++;
	}
	eed = sf_ctx->filledgebase.first;
	while (eed) {
		pflist[eed->poly_nr - 1].edges++;
		eed = eed->next;
	}

	eve = sf_ctx->fillvertbase.first;
	while (eve) {
		pflist[eve->poly_nr - 1].verts++;
		min_xy_p = pflist[eve->poly_nr - 1].min_xy;
		max_xy_p = pflist[eve->poly_nr - 1].max_xy;

		min_xy_p[0] = (min_xy_p[0]) < (eve->xy[0]) ? (min_xy_p[0]) : (eve->xy[0]);
		min_xy_p[1] = (min_xy_p[1]) < (eve->xy[1]) ? (min_xy_p[1]) : (eve->xy[1]);
		max_xy_p[0] = (max_xy_p[0]) > (eve->xy[0]) ? (max_xy_p[0]) : (eve->xy[0]);
		max_xy_p[1] = (max_xy_p[1]) > (eve->xy[1]) ? (max_xy_p[1]) : (eve->xy[1]);
		if (eve->h > 2) pflist[eve->poly_nr - 1].f = 1;

		eve = eve->next;
	}

	/* STEP 4: FIND HOLES OR BOUNDS, JOIN THEM
	 *  ( bounds just to divide it in pieces for optimization, 
	 *    the edgefill itself has good auto-hole detection)
	 * WATCH IT: ONLY WORKS WITH SORTED POLYS!!! */
	
	if (poly > 1) {
		short *polycache, *pc;

		/* so, sort first */
		qsort(pflist, poly, sizeof(PolyFill), vergpoly);

#if 0
		pf = pflist;
		for (a = 1; a <= poly; a++) {
			printf("poly:%d edges:%d verts:%d flag: %d\n", a, pf->edges, pf->verts, pf->f);
			PRINT2(f, f, pf->min[0], pf->min[1]);
			pf++;
		}
#endif
	
		polycache = pc = MEM_callocN(sizeof(short) * poly, "polycache");
		pf = pflist;
		for (a = 0; a < poly; a++, pf++) {
			for (c = a + 1; c < poly; c++) {
				
				/* if 'a' inside 'c': join (bbox too)
				 * Careful: 'a' can also be inside another poly.
				 */
				if (boundisect(pf, pflist + c)) {
					*pc = c;
					pc++;
				}
				/* only for optimize! */
				/* else if (pf->max_xy[0] < (pflist+c)->min[cox]) break; */
				
			}
			while (pc != polycache) {
				pc--;
				mergepolysSimp(sf_ctx, pf, pflist + *pc);
			}
		}
		MEM_freeN(polycache);
	}

#if 0
	printf("after merge\n");
	pf = pflist;
	for (a = 1; a <= poly; a++) {
		printf("poly:%d edges:%d verts:%d flag: %d\n", a, pf->edges, pf->verts, pf->f);
		pf++;
	}
#endif

	/* STEP 5: MAKE TRIANGLES */

	tempve.first = sf_ctx->fillvertbase.first;
	tempve.last = sf_ctx->fillvertbase.last;
	temped.first = sf_ctx->filledgebase.first;
	temped.last = sf_ctx->filledgebase.last;
	sf_ctx->fillvertbase.first = sf_ctx->fillvertbase.last = NULL;
	sf_ctx->filledgebase.first = sf_ctx->filledgebase.last = NULL;

	pf = pflist;
	for (a = 0; a < poly; a++) {
		if (pf->edges > 1) {
			splitlist(sf_ctx, &tempve, &temped, pf->nr);
			totfaces += scanfill(sf_ctx, pf);
		}
		pf++;
	}
	BLI_movelisttolist(&sf_ctx->fillvertbase, &tempve);
	BLI_movelisttolist(&sf_ctx->filledgebase, &temped);

	/* FREE */

	MEM_freeN(pflist);

	return totfaces;
}
