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
#include <limits.h>

#include "MEM_guardedalloc.h"

#include "BLI_listbase.h"
#include "BLI_math.h"
#include "BLI_memarena.h"
#include "BLI_utildefines.h"

#include "BLI_scanfill.h"  /* own include */

#include "BLI_strict_flags.h"

/* local types */
typedef struct PolyFill {
	unsigned int edges, verts;
	float min_xy[2], max_xy[2];
	unsigned short nr;
	bool f;
} PolyFill;

typedef struct ScanFillVertLink {
	ScanFillVert *vert;
	ScanFillEdge *edge_first, *edge_last;
} ScanFillVertLink;


/* local funcs */

#define SF_EPSILON   0.00003f
#define SF_EPSILON_SQ (SF_EPSILON * SF_EPSILON)


/* ScanFillVert.status */
#define SF_VERT_NEW        0  /* all new verts have this flag set */
#define SF_VERT_AVAILABLE  1  /* available - in an edge */
#define SF_VERT_ZERO_LEN   2


/* ScanFillEdge.status */
/* Optionally set ScanFillEdge f to this to mark original boundary edges.
 * Only needed if there are internal diagonal edges passed to BLI_scanfill_calc. */
#define SF_EDGE_NEW      0  /* all new edges have this flag set */
// #define SF_EDGE_BOUNDARY 1  /* UNUSED */
#define SF_EDGE_INTERNAL 2  /* edge is created while scan-filling */


/* PolyFill.status */
#define SF_POLY_NEW   0  /* all polys initialized to this */
#define SF_POLY_VALID 1  /* has at least 3 verts */

/* ****  FUNCTIONS FOR QSORT *************************** */


static int vergscdata(const void *a1, const void *a2)
{
	const ScanFillVertLink *x1 = a1, *x2 = a2;
	
	if      (x1->vert->xy[1] < x2->vert->xy[1]) return  1;
	else if (x1->vert->xy[1] > x2->vert->xy[1]) return -1;
	else if (x1->vert->xy[0] > x2->vert->xy[0]) return  1;
	else if (x1->vert->xy[0] < x2->vert->xy[0]) return -1;

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

/* ****  FILL ROUTINES *************************** */

ScanFillVert *BLI_scanfill_vert_add(ScanFillContext *sf_ctx, const float vec[3])
{
	ScanFillVert *sf_v;
	
	sf_v = BLI_memarena_alloc(sf_ctx->arena, sizeof(ScanFillVert));

	BLI_addtail(&sf_ctx->fillvertbase, sf_v);

	sf_v->tmp.p = NULL;
	copy_v3_v3(sf_v->co, vec);

	/* just zero out the rest */
	zero_v2(sf_v->xy);
	sf_v->keyindex = 0;
	sf_v->poly_nr = sf_ctx->poly_nr;
	sf_v->edge_tot = 0;
	sf_v->f = SF_VERT_NEW;
	sf_v->user_flag = 0;

	return sf_v;
}

ScanFillEdge *BLI_scanfill_edge_add(ScanFillContext *sf_ctx, ScanFillVert *v1, ScanFillVert *v2)
{
	ScanFillEdge *sf_ed;

	sf_ed = BLI_memarena_alloc(sf_ctx->arena, sizeof(ScanFillEdge));
	BLI_addtail(&sf_ctx->filledgebase, sf_ed);
	
	sf_ed->v1 = v1;
	sf_ed->v2 = v2;

	/* just zero out the rest */
	sf_ed->poly_nr = sf_ctx->poly_nr;
	sf_ed->f = SF_EDGE_NEW;
	sf_ed->user_flag = 0;
	sf_ed->tmp.c = 0;

	return sf_ed;
}

static void addfillface(ScanFillContext *sf_ctx, ScanFillVert *v1, ScanFillVert *v2, ScanFillVert *v3)
{
	/* does not make edges */
	ScanFillFace *sf_tri;

	sf_tri = BLI_memarena_alloc(sf_ctx->arena, sizeof(ScanFillFace));
	BLI_addtail(&sf_ctx->fillfacebase, sf_tri);
	
	sf_tri->v1 = v1;
	sf_tri->v2 = v2;
	sf_tri->v3 = v3;
}

static bool boundisect(PolyFill *pf2, PolyFill *pf1)
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
	for (eve = sf_ctx->fillvertbase.first; eve; eve = eve->next) {
		if (eve->poly_nr == pf2->nr) {
			eve->poly_nr = pf1->nr;
		}
	}

	for (eed = sf_ctx->filledgebase.first; eed; eed = eed->next) {
		if (eed->poly_nr == pf2->nr) {
			eed->poly_nr = pf1->nr;
		}
	}

	pf1->verts += pf2->verts;
	pf1->edges += pf2->edges;
	pf2->verts = pf2->edges = 0;
	pf1->f = (pf1->f | pf2->f);
}

static bool testedgeside(const float v1[2], const float v2[2], const float v3[2])
/* is v3 to the right of v1-v2 ? With exception: v3 == v1 || v3 == v2 */
{
	float inp;

	inp = (v2[0] - v1[0]) * (v1[1] - v3[1]) +
	      (v1[1] - v2[1]) * (v1[0] - v3[0]);

	if (inp < 0.0f) {
		return 0;
	}
	else if (inp == 0.0f) {
		if (v1[0] == v3[0] && v1[1] == v3[1]) return 0;
		if (v2[0] == v3[0] && v2[1] == v3[1]) return 0;
	}
	return 1;
}

static bool addedgetoscanvert(ScanFillVertLink *sc, ScanFillEdge *eed)
{
	/* find first edge to the right of eed, and insert eed before that */
	ScanFillEdge *ed;
	float fac, fac1, x, y;

	if (sc->edge_first == NULL) {
		sc->edge_first = sc->edge_last = eed;
		eed->prev = eed->next = NULL;
		return 1;
	}

	x = eed->v1->xy[0];
	y = eed->v1->xy[1];

	fac1 = eed->v2->xy[1] - y;
	if (fac1 == 0.0f) {
		fac1 = 1.0e10f * (eed->v2->xy[0] - x);

	}
	else {
		fac1 = (x - eed->v2->xy[0]) / fac1;
	}

	for (ed = sc->edge_first; ed; ed = ed->next) {

		if (ed->v2 == eed->v2) {
			return 0;
		}

		fac = ed->v2->xy[1] - y;
		if (fac == 0.0f) {
			fac = 1.0e10f * (ed->v2->xy[0] - x);
		}
		else {
			fac = (x - ed->v2->xy[0]) / fac;
		}

		if (fac > fac1) {
			break;
		}
	}
	if (ed) BLI_insertlinkbefore((ListBase *)&(sc->edge_first), ed, eed);
	else BLI_addtail((ListBase *)&(sc->edge_first), eed);

	return 1;
}


static ScanFillVertLink *addedgetoscanlist(ScanFillVertLink *scdata, ScanFillEdge *eed, unsigned int len)
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
	scsearch.vert = eed->v1;
	sc = (ScanFillVertLink *)bsearch(&scsearch, scdata, len,
	                                 sizeof(ScanFillVertLink), vergscdata);

	if (UNLIKELY(sc == NULL)) {
		printf("Error in search edge: %p\n", (void *)eed);
	}
	else if (addedgetoscanvert(sc, eed) == false) {
		return sc;
	}

	return NULL;
}

static bool boundinsideEV(ScanFillEdge *eed, ScanFillVert *eve)
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
		if (eve->xy[1] >= miny && eve->xy[1] <= maxy) {
			return 1;
		}
	}
	return 0;
}


static void testvertexnearedge(ScanFillContext *sf_ctx)
{
	/* only vertices with (->edge_tot == 1) are being tested for
	 * being close to an edge, if true insert */

	ScanFillVert *eve;
	ScanFillEdge *eed, *ed1;

	for (eve = sf_ctx->fillvertbase.first; eve; eve = eve->next) {
		if (eve->edge_tot == 1) {
			/* find the edge which has vertex eve,
			 * note: we _know_ this will crash if 'ed1' becomes NULL
			 * but this will never happen. */
			for (ed1 = sf_ctx->filledgebase.first;
			     !(ed1->v1 == eve || ed1->v2 == eve);
			     ed1 = ed1->next)
			{
				/* do nothing */
			}

			if (ed1->v1 == eve) {
				ed1->v1 = ed1->v2;
				ed1->v2 = eve;
			}

			for (eed = sf_ctx->filledgebase.first; eed; eed = eed->next) {
				if (eve != eed->v1 && eve != eed->v2 && eve->poly_nr == eed->poly_nr) {
					if (compare_v2v2(eve->xy, eed->v1->xy, SF_EPSILON)) {
						ed1->v2 = eed->v1;
						eed->v1->edge_tot++;
						eve->edge_tot = 0;
						break;
					}
					else if (compare_v2v2(eve->xy, eed->v2->xy, SF_EPSILON)) {
						ed1->v2 = eed->v2;
						eed->v2->edge_tot++;
						eve->edge_tot = 0;
						break;
					}
					else {
						if (boundinsideEV(eed, eve)) {
							const float dist = dist_squared_to_line_v2(eed->v1->xy, eed->v2->xy, eve->xy);
							if (dist < SF_EPSILON_SQ) {
								/* new edge */
								ed1 = BLI_scanfill_edge_add(sf_ctx, eed->v1, eve);
								
								/* printf("fill: vertex near edge %x\n", eve); */
								ed1->poly_nr = eed->poly_nr;
								eed->v1 = eve;
								eve->edge_tot = 3;
								break;
							}
						}
					}
				}
			}
		}
	}
}

static void splitlist(ScanFillContext *sf_ctx, ListBase *tempve, ListBase *temped, unsigned short nr)
{
	/* everything is in templist, write only poly nr to fillist */
	ScanFillVert *eve, *eve_next;
	ScanFillEdge *eed, *eed_next;

	BLI_movelisttolist(tempve, &sf_ctx->fillvertbase);
	BLI_movelisttolist(temped, &sf_ctx->filledgebase);


	for (eve = tempve->first; eve; eve = eve_next) {
		eve_next = eve->next;
		if (eve->poly_nr == nr) {
			BLI_remlink(tempve, eve);
			BLI_addtail(&sf_ctx->fillvertbase, eve);
		}

	}
	
	for (eed = temped->first; eed; eed = eed_next) {
		eed_next = eed->next;
		if (eed->poly_nr == nr) {
			BLI_remlink(temped, eed);
			BLI_addtail(&sf_ctx->filledgebase, eed);
		}
	}
}

static unsigned int scanfill(ScanFillContext *sf_ctx, PolyFill *pf, const int flag)
{
	ScanFillVertLink *scdata;
	ScanFillVertLink *sc = NULL, *sc1;
	ScanFillVert *eve, *v1, *v2, *v3;
	ScanFillEdge *eed, *eed_next, *ed1, *ed2, *ed3;
	unsigned int a, b, verts, maxface, totface;
	const unsigned short nr = pf->nr;
	bool twoconnected = false;

	/* PRINTS */
#if 0
	verts = pf->verts;
	for (eve = sf_ctx->fillvertbase.first; eve; eve = eve->next) {
		printf("vert: %x co: %f %f\n", eve, eve->xy[0], eve->xy[1]);
	}

	for (eed = sf_ctx->filledgebase.first; eed; eed = eed->next) {
		printf("edge: %x  verts: %x %x\n", eed, eed->v1, eed->v2);
	}
#endif

	/* STEP 0: remove zero sized edges */
	if (flag & BLI_SCANFILL_CALC_REMOVE_DOUBLES) {
		for (eed = sf_ctx->filledgebase.first; eed; eed = eed->next) {
			if (equals_v2v2(eed->v1->xy, eed->v2->xy)) {
				if (eed->v1->f == SF_VERT_ZERO_LEN && eed->v2->f != SF_VERT_ZERO_LEN) {
					eed->v2->f = SF_VERT_ZERO_LEN;
					eed->v2->tmp.v = eed->v1->tmp.v;
				}
				else if (eed->v2->f == SF_VERT_ZERO_LEN && eed->v1->f != SF_VERT_ZERO_LEN) {
					eed->v1->f = SF_VERT_ZERO_LEN;
					eed->v1->tmp.v = eed->v2->tmp.v;
				}
				else if (eed->v2->f == SF_VERT_ZERO_LEN && eed->v1->f == SF_VERT_ZERO_LEN) {
					eed->v1->tmp.v = eed->v2->tmp.v;
				}
				else {
					eed->v2->f = SF_VERT_ZERO_LEN;
					eed->v2->tmp.v = eed->v1;
				}
			}
		}
	}

	/* STEP 1: make using FillVert and FillEdge lists a sorted
	 * ScanFillVertLink list
	 */
	sc = scdata = MEM_mallocN(sizeof(*scdata) * pf->verts, "Scanfill1");
	verts = 0;
	for (eve = sf_ctx->fillvertbase.first; eve; eve = eve->next) {
		if (eve->poly_nr == nr) {
			if (eve->f != SF_VERT_ZERO_LEN) {
				verts++;
				eve->f = SF_VERT_NEW;  /* flag for connectedges later on */
				sc->vert = eve;
				sc->edge_first = sc->edge_last = NULL;
				/* if (even->tmp.v == NULL) eve->tmp.u = verts; */ /* Note, debug print only will work for curve polyfill, union is in use for mesh */
				sc++;
			}
		}
	}

	qsort(scdata, verts, sizeof(ScanFillVertLink), vergscdata);

	if (flag & BLI_SCANFILL_CALC_REMOVE_DOUBLES) {
		for (eed = sf_ctx->filledgebase.first; eed; eed = eed_next) {
			eed_next = eed->next;
			BLI_remlink(&sf_ctx->filledgebase, eed);
			/* This code is for handling zero-length edges that get
			 * collapsed in step 0. It was removed for some time to
			 * fix trunk bug #4544, so if that comes back, this code
			 * may need some work, or there will have to be a better
			 * fix to #4544.
			 *
			 * warning, this can hang on un-ordered edges, see: [#33281]
			 * for now disable 'BLI_SCANFILL_CALC_REMOVE_DOUBLES' for ngons.
			 */
			if (eed->v1->f == SF_VERT_ZERO_LEN) {
				v1 = eed->v1;
				while ((eed->v1->f == SF_VERT_ZERO_LEN) && (eed->v1->tmp.v != v1) && (eed->v1 != eed->v1->tmp.v))
					eed->v1 = eed->v1->tmp.v;
			}
			if (eed->v2->f == SF_VERT_ZERO_LEN) {
				v2 = eed->v2;
				while ((eed->v2->f == SF_VERT_ZERO_LEN) && (eed->v2->tmp.v != v2) && (eed->v2 != eed->v2->tmp.v))
					eed->v2 = eed->v2->tmp.v;
			}
			if (eed->v1 != eed->v2) {
				addedgetoscanlist(scdata, eed, verts);
			}
		}
	}
	else {
		for (eed = sf_ctx->filledgebase.first; eed; eed = eed_next) {
			eed_next = eed->next;
			BLI_remlink(&sf_ctx->filledgebase, eed);
			if (eed->v1 != eed->v2) {
				addedgetoscanlist(scdata, eed, verts);
			}
		}
	}
#if 0
	sc = sf_ctx->_scdata;
	for (a = 0; a < verts; a++) {
		printf("\nscvert: %x\n", sc->vert);
		for (eed = sc->edge_first; eed; eed = eed->next) {
			printf(" ed %x %x %x\n", eed, eed->v1, eed->v2);
		}
		sc++;
	}
#endif


	/* STEP 2: FILL LOOP */

	if (pf->f == SF_POLY_NEW)
		twoconnected = true;

	/* (temporal) security: never much more faces than vertices */
	totface = 0;
	if (flag & BLI_SCANFILL_CALC_HOLES) {
		maxface = 2 * verts;       /* 2*verts: based at a filled circle within a triangle */
	}
	else {
		maxface = verts - 2;       /* when we don't calc any holes, we assume face is a non overlapping loop */
	}

	sc = scdata;
	for (a = 0; a < verts; a++) {
		/* printf("VERTEX %d index %d\n", a, sc->vert->tmp.u); */
		/* set connectflags  */
		for (ed1 = sc->edge_first; ed1; ed1 = eed_next) {
			eed_next = ed1->next;
			if (ed1->v1->edge_tot == 1 || ed1->v2->edge_tot == 1) {
				BLI_remlink((ListBase *)&(sc->edge_first), ed1);
				BLI_addtail(&sf_ctx->filledgebase, ed1);
				if (ed1->v1->edge_tot > 1) ed1->v1->edge_tot--;
				if (ed1->v2->edge_tot > 1) ed1->v2->edge_tot--;
			}
			else {
				ed1->v2->f = SF_VERT_AVAILABLE;
			}
		}
		while (sc->edge_first) { /* for as long there are edges */
			ed1 = sc->edge_first;
			ed2 = ed1->next;
			
			/* commented out... the ESC here delivers corrupted memory (and doesnt work during grab) */
			/* if (callLocalInterruptCallBack()) break; */
			if (totface >= maxface) {
				/* printf("Fill error: endless loop. Escaped at vert %d,  tot: %d.\n", a, verts); */
				a = verts;
				break;
			}
			if (ed2 == NULL) {
				sc->edge_first = sc->edge_last = NULL;
				/* printf("just 1 edge to vert\n"); */
				BLI_addtail(&sf_ctx->filledgebase, ed1);
				ed1->v2->f = SF_VERT_NEW;
				ed1->v1->edge_tot--;
				ed1->v2->edge_tot--;
			}
			else {
				/* test rest of vertices */
				ScanFillVertLink *best_sc = NULL;
				float best_angle = 3.14f;
				float miny;
				bool firsttime = false;
				
				v1 = ed1->v2;
				v2 = ed1->v1;
				v3 = ed2->v2;
				
				/* this happens with a serial of overlapping edges */
				if (v1 == v2 || v2 == v3) break;
				
				/* printf("test verts %d %d %d\n", v1->tmp.u, v2->tmp.u, v3->tmp.u); */
				miny = min_ff(v1->xy[1], v3->xy[1]);
				sc1 = sc + 1;

				for (b = a + 1; b < verts; b++, sc1++) {
					if (sc1->vert->f == SF_VERT_NEW) {
						if (sc1->vert->xy[1] <= miny) break;
						if (testedgeside(v1->xy, v2->xy, sc1->vert->xy)) {
							if (testedgeside(v2->xy, v3->xy, sc1->vert->xy)) {
								if (testedgeside(v3->xy, v1->xy, sc1->vert->xy)) {
									/* point is in triangle */
									
									/* because multiple points can be inside triangle (concave holes) */
									/* we continue searching and pick the one with sharpest corner */
									
									if (best_sc == NULL) {
										/* even without holes we need to keep checking [#35861] */
										best_sc = sc1;
									}
									else {
										float angle;
										
										/* prevent angle calc for the simple cases only 1 vertex is found */
										if (firsttime == false) {
											best_angle = angle_v2v2v2(v2->xy, v1->xy, best_sc->vert->xy);
											firsttime = true;
										}

										angle = angle_v2v2v2(v2->xy, v1->xy, sc1->vert->xy);
										if (angle < best_angle) {
											best_sc = sc1;
											best_angle = angle;
										}
									}
										
								}
							}
						}
					}
				}
					
				if (best_sc) {
					/* make new edge, and start over */
					/* printf("add new edge %d %d and start again\n", v2->tmp.u, best_sc->vert->tmp.u); */

					ed3 = BLI_scanfill_edge_add(sf_ctx, v2, best_sc->vert);
					BLI_remlink(&sf_ctx->filledgebase, ed3);
					BLI_insertlinkbefore((ListBase *)&(sc->edge_first), ed2, ed3);
					ed3->v2->f = SF_VERT_AVAILABLE;
					ed3->f = SF_EDGE_INTERNAL;
					ed3->v1->edge_tot++;
					ed3->v2->edge_tot++;
				}
				else {
					/* new triangle */
					/* printf("add face %d %d %d\n", v1->tmp.u, v2->tmp.u, v3->tmp.u); */
					addfillface(sf_ctx, v1, v2, v3);
					totface++;
					BLI_remlink((ListBase *)&(sc->edge_first), ed1);
					BLI_addtail(&sf_ctx->filledgebase, ed1);
					ed1->v2->f = SF_VERT_NEW;
					ed1->v1->edge_tot--;
					ed1->v2->edge_tot--;
					/* ed2 can be removed when it's a boundary edge */
					if (((ed2->f == SF_EDGE_NEW) && twoconnected) /* || (ed2->f == SF_EDGE_BOUNDARY) */) {
						BLI_remlink((ListBase *)&(sc->edge_first), ed2);
						BLI_addtail(&sf_ctx->filledgebase, ed2);
						ed2->v2->f = SF_VERT_NEW;
						ed2->v1->edge_tot--;
						ed2->v2->edge_tot--;
					}

					/* new edge */
					ed3 = BLI_scanfill_edge_add(sf_ctx, v1, v3);
					BLI_remlink(&sf_ctx->filledgebase, ed3);
					ed3->f = SF_EDGE_INTERNAL;
					ed3->v1->edge_tot++;
					ed3->v2->edge_tot++;
					
					/* printf("add new edge %x %x\n", v1, v3); */
					sc1 = addedgetoscanlist(scdata, ed3, verts);
					
					if (sc1) {  /* ed3 already exists: remove if a boundary */
						/* printf("Edge exists\n"); */
						ed3->v1->edge_tot--;
						ed3->v2->edge_tot--;

						for (ed3 = sc1->edge_first; ed3; ed3 = ed3->next) {
							if ((ed3->v1 == v1 && ed3->v2 == v3) || (ed3->v1 == v3 && ed3->v2 == v1)) {
								if (twoconnected /* || (ed3->f == SF_EDGE_BOUNDARY) */) {
									BLI_remlink((ListBase *)&(sc1->edge_first), ed3);
									BLI_addtail(&sf_ctx->filledgebase, ed3);
									ed3->v1->edge_tot--;
									ed3->v2->edge_tot--;
								}
								break;
							}
						}
					}
				}
			}

			/* test for loose edges */
			for (ed1 = sc->edge_first; ed1; ed1 = eed_next) {
				eed_next = ed1->next;
				if (ed1->v1->edge_tot < 2 || ed1->v2->edge_tot < 2) {
					BLI_remlink((ListBase *)&(sc->edge_first), ed1);
					BLI_addtail(&sf_ctx->filledgebase, ed1);
					if (ed1->v1->edge_tot > 1) ed1->v1->edge_tot--;
					if (ed1->v2->edge_tot > 1) ed1->v2->edge_tot--;
				}
			}
			/* done with loose edges */
		}

		sc++;
	}

	MEM_freeN(scdata);

	BLI_assert(totface <= maxface);

	return totface;
}


void BLI_scanfill_begin(ScanFillContext *sf_ctx)
{
	memset(sf_ctx, 0, sizeof(*sf_ctx));
	sf_ctx->poly_nr = SF_POLY_UNSET;
	sf_ctx->arena = BLI_memarena_new(BLI_SCANFILL_ARENA_SIZE, __func__);
}

void BLI_scanfill_begin_arena(ScanFillContext *sf_ctx, MemArena *arena)
{
	memset(sf_ctx, 0, sizeof(*sf_ctx));
	sf_ctx->poly_nr = SF_POLY_UNSET;
	sf_ctx->arena = arena;
}

void BLI_scanfill_end(ScanFillContext *sf_ctx)
{
	BLI_memarena_free(sf_ctx->arena);
	sf_ctx->arena = NULL;

	BLI_listbase_clear(&sf_ctx->fillvertbase);
	BLI_listbase_clear(&sf_ctx->filledgebase);
	BLI_listbase_clear(&sf_ctx->fillfacebase);
}

void BLI_scanfill_end_arena(ScanFillContext *sf_ctx, MemArena *arena)
{
	BLI_memarena_clear(arena);
	BLI_assert(sf_ctx->arena == arena);

	BLI_listbase_clear(&sf_ctx->fillvertbase);
	BLI_listbase_clear(&sf_ctx->filledgebase);
	BLI_listbase_clear(&sf_ctx->fillfacebase);
}

unsigned int BLI_scanfill_calc_ex(ScanFillContext *sf_ctx, const int flag, const float nor_proj[3])
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
	ScanFillEdge *eed, *eed_next;
	PolyFill *pflist, *pf;
	float *min_xy_p, *max_xy_p;
	unsigned int totfaces = 0;  /* total faces added */
	unsigned short a, c, poly = 0;
	bool ok;
	float mat_2d[3][3];

	BLI_assert(!nor_proj || len_squared_v3(nor_proj) > FLT_EPSILON);

#ifdef DEBUG
	for (eve = sf_ctx->fillvertbase.first; eve; eve = eve->next) {
		/* these values used to be set,
		 * however they should always be zero'd so check instead */
		BLI_assert(eve->f == 0);
		BLI_assert(sf_ctx->poly_nr || eve->poly_nr == 0);
		BLI_assert(eve->edge_tot == 0);
	}
#endif

#if 0
	if (flag & BLI_SCANFILL_CALC_QUADTRI_FASTPATH) {
		const int totverts = BLI_countlist(&sf_ctx->fillvertbase);

		if (totverts == 3) {
			eve = sf_ctx->fillvertbase.first;

			addfillface(sf_ctx, eve, eve->next, eve->next->next);
			return 1;
		}
		else if (totverts == 4) {
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
	}
#endif

	/* first test vertices if they are in edges */
	/* including resetting of flags */
	for (eed = sf_ctx->filledgebase.first; eed; eed = eed->next) {
		BLI_assert(sf_ctx->poly_nr != SF_POLY_UNSET || eed->poly_nr == SF_POLY_UNSET);
		eed->v1->f = SF_VERT_AVAILABLE;
		eed->v2->f = SF_VERT_AVAILABLE;
	}

	for (eve = sf_ctx->fillvertbase.first; eve; eve = eve->next) {
		if (eve->f == SF_VERT_AVAILABLE) {
			break;
		}
	}

	if (UNLIKELY(eve == NULL)) {
		return 0;
	}
	else {
		float n[3];

		if (nor_proj) {
			copy_v3_v3(n, nor_proj);
		}
		else {
			/* define projection: with 'best' normal */
			/* Newell's Method */
			/* Similar code used elsewhere, but this checks for double ups
			 * which historically this function supports so better not change */
			const float *v_prev;

			zero_v3(n);
			eve = sf_ctx->fillvertbase.last;
			v_prev = eve->co;

			for (eve = sf_ctx->fillvertbase.first; eve; eve = eve->next) {
				if (LIKELY(!compare_v3v3(v_prev, eve->co, SF_EPSILON))) {
					add_newell_cross_v3_v3v3(n, v_prev, eve->co);
					v_prev = eve->co;
				}
			}
		}

		if (UNLIKELY(normalize_v3(n) == 0.0f)) {
			return 0;
		}

		axis_dominant_v3_to_m3(mat_2d, n);
	}


	/* STEP 1: COUNT POLYS */
	if (sf_ctx->poly_nr != SF_POLY_UNSET) {
		poly = (unsigned short)(sf_ctx->poly_nr + 1);
		sf_ctx->poly_nr = SF_POLY_UNSET;
	}

	if (flag & BLI_SCANFILL_CALC_POLYS && (poly == 0)) {
		for (eve = sf_ctx->fillvertbase.first; eve; eve = eve->next) {
			mul_v2_m3v3(eve->xy, mat_2d, eve->co);

			/* get first vertex with no poly number */
			if (eve->poly_nr == SF_POLY_UNSET) {
				unsigned int toggle = 0;
				/* now a sort of select connected */
				ok = true;
				eve->poly_nr = poly;

				while (ok) {

					ok = false;

					toggle++;
					for (eed = (toggle & 1) ? sf_ctx->filledgebase.first : sf_ctx->filledgebase.last;
					     eed;
					     eed = (toggle & 1) ? eed->next : eed->prev)
					{
						if (eed->v1->poly_nr == SF_POLY_UNSET && eed->v2->poly_nr == poly) {
							eed->v1->poly_nr = poly;
							eed->poly_nr = poly;
							ok = true;
						}
						else if (eed->v2->poly_nr == SF_POLY_UNSET && eed->v1->poly_nr == poly) {
							eed->v2->poly_nr = poly;
							eed->poly_nr = poly;
							ok = true;
						}
						else if (eed->poly_nr == SF_POLY_UNSET) {
							if (eed->v1->poly_nr == poly && eed->v2->poly_nr == poly) {
								eed->poly_nr = poly;
								ok = true;
							}
						}
					}
				}

				poly++;
			}
		}
		/* printf("amount of poly's: %d\n", poly); */
	}
	else if (poly) {
		/* we pre-calculated poly_nr */
		for (eve = sf_ctx->fillvertbase.first; eve; eve = eve->next) {
			mul_v2_m3v3(eve->xy, mat_2d, eve->co);
		}
	}
	else {
		poly = 1;

		for (eve = sf_ctx->fillvertbase.first; eve; eve = eve->next) {
			mul_v2_m3v3(eve->xy, mat_2d, eve->co);
			eve->poly_nr = 0;
		}

		for (eed = sf_ctx->filledgebase.first; eed; eed = eed->next) {
			eed->poly_nr = 0;
		}
	}

	/* STEP 2: remove loose edges and strings of edges */
	if (flag & BLI_SCANFILL_CALC_LOOSE) {
		unsigned int toggle = 0;
		for (eed = sf_ctx->filledgebase.first; eed; eed = eed->next) {
			if (eed->v1->edge_tot++ > 250) break;
			if (eed->v2->edge_tot++ > 250) break;
		}
		if (eed) {
			/* otherwise it's impossible to be sure you can clear vertices */
#ifdef DEBUG
			printf("No vertices with 250 edges allowed!\n");
#endif
			return 0;
		}

		/* does it only for vertices with (->edge_tot == 1) */
		testvertexnearedge(sf_ctx);

		ok = true;
		while (ok) {
			ok = false;

			toggle++;
			for (eed = (toggle & 1) ? sf_ctx->filledgebase.first : sf_ctx->filledgebase.last;
			     eed;
			     eed = eed_next)
			{
				eed_next = (toggle & 1) ? eed->next : eed->prev;
				if (eed->v1->edge_tot == 1) {
					eed->v2->edge_tot--;
					BLI_remlink(&sf_ctx->fillvertbase, eed->v1);
					BLI_remlink(&sf_ctx->filledgebase, eed);
					ok = true;
				}
				else if (eed->v2->edge_tot == 1) {
					eed->v1->edge_tot--;
					BLI_remlink(&sf_ctx->fillvertbase, eed->v2);
					BLI_remlink(&sf_ctx->filledgebase, eed);
					ok = true;
				}
			}
		}
		if (BLI_listbase_is_empty(&sf_ctx->filledgebase)) {
			/* printf("All edges removed\n"); */
			return 0;
		}
	}
	else {
		/* skip checks for loose edges */
		for (eed = sf_ctx->filledgebase.first; eed; eed = eed->next) {
			eed->v1->edge_tot++;
			eed->v2->edge_tot++;
		}
#ifdef DEBUG
		/* ensure we're right! */
		for (eed = sf_ctx->filledgebase.first; eed; eed = eed->next) {
			BLI_assert(eed->v1->edge_tot != 1);
			BLI_assert(eed->v2->edge_tot != 1);
		}
#endif
	}


	/* CURRENT STATUS:
	 * - eve->f        :1 = available in edges
	 * - eve->poly_nr  :polynumber
	 * - eve->edge_tot :amount of edges connected to vertex
	 * - eve->tmp.v    :store! original vertex number
	 * 
	 * - eed->f        :1 = boundary edge (optionally set by caller)
	 * - eed->poly_nr  :poly number
	 */


	/* STEP 3: MAKE POLYFILL STRUCT */
	pflist = MEM_mallocN(sizeof(*pflist) * (size_t)poly, "edgefill");
	pf = pflist;
	for (a = 0; a < poly; a++) {
		pf->edges = pf->verts = 0;
		pf->min_xy[0] = pf->min_xy[1] =  1.0e20f;
		pf->max_xy[0] = pf->max_xy[1] = -1.0e20f;
		pf->f = SF_POLY_NEW;
		pf->nr = a;
		pf++;
	}
	for (eed = sf_ctx->filledgebase.first; eed; eed = eed->next) {
		pflist[eed->poly_nr].edges++;
	}

	for (eve = sf_ctx->fillvertbase.first; eve; eve = eve->next) {
		pflist[eve->poly_nr].verts++;
		min_xy_p = pflist[eve->poly_nr].min_xy;
		max_xy_p = pflist[eve->poly_nr].max_xy;

		min_xy_p[0] = (min_xy_p[0]) < (eve->xy[0]) ? (min_xy_p[0]) : (eve->xy[0]);
		min_xy_p[1] = (min_xy_p[1]) < (eve->xy[1]) ? (min_xy_p[1]) : (eve->xy[1]);
		max_xy_p[0] = (max_xy_p[0]) > (eve->xy[0]) ? (max_xy_p[0]) : (eve->xy[0]);
		max_xy_p[1] = (max_xy_p[1]) > (eve->xy[1]) ? (max_xy_p[1]) : (eve->xy[1]);
		if (eve->edge_tot > 2) {
			pflist[eve->poly_nr].f = SF_POLY_VALID;
		}
	}

	/* STEP 4: FIND HOLES OR BOUNDS, JOIN THEM
	 *  ( bounds just to divide it in pieces for optimization, 
	 *    the edgefill itself has good auto-hole detection)
	 * WATCH IT: ONLY WORKS WITH SORTED POLYS!!! */
	
	if ((flag & BLI_SCANFILL_CALC_HOLES) && (poly > 1)) {
		unsigned short *polycache, *pc;

		/* so, sort first */
		qsort(pflist, (size_t)poly, sizeof(PolyFill), vergpoly);

#if 0
		pf = pflist;
		for (a = 0; a < poly; a++) {
			printf("poly:%d edges:%d verts:%d flag: %d\n", a, pf->edges, pf->verts, pf->f);
			PRINT2(f, f, pf->min[0], pf->min[1]);
			pf++;
		}
#endif

		polycache = pc = MEM_callocN(sizeof(*polycache) * (size_t)poly, "polycache");
		pf = pflist;
		for (a = 0; a < poly; a++, pf++) {
			for (c = (unsigned short)(a + 1); c < poly; c++) {
				
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
	for (a = 0; a < poly; a++) {
		printf("poly:%d edges:%d verts:%d flag: %d\n", a, pf->edges, pf->verts, pf->f);
		pf++;
	}
#endif

	/* STEP 5: MAKE TRIANGLES */

	tempve.first = sf_ctx->fillvertbase.first;
	tempve.last = sf_ctx->fillvertbase.last;
	temped.first = sf_ctx->filledgebase.first;
	temped.last = sf_ctx->filledgebase.last;
	BLI_listbase_clear(&sf_ctx->fillvertbase);
	BLI_listbase_clear(&sf_ctx->filledgebase);

	pf = pflist;
	for (a = 0; a < poly; a++) {
		if (pf->edges > 1) {
			splitlist(sf_ctx, &tempve, &temped, pf->nr);
			totfaces += scanfill(sf_ctx, pf, flag);
		}
		pf++;
	}
	BLI_movelisttolist(&sf_ctx->fillvertbase, &tempve);
	BLI_movelisttolist(&sf_ctx->filledgebase, &temped);

	/* FREE */

	MEM_freeN(pflist);

	return totfaces;
}

unsigned int BLI_scanfill_calc(ScanFillContext *sf_ctx, const int flag)
{
	return BLI_scanfill_calc_ex(sf_ctx, flag, NULL);
}
