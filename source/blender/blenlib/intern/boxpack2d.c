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

/** \file blender/blenlib/intern/boxpack2d.c
 *  \ingroup bli
 */

#include <stdlib.h> /* for qsort */

#include "MEM_guardedalloc.h"
#include "BLI_boxpack2d.h"

/* BoxPacker for backing 2D rectangles into a square
 * 
 * The defined Below are for internal use only */

typedef struct boxVert {
	float x;
	float y;
	short free;

	struct BoxPack *trb; /* top right box */
	struct BoxPack *blb; /* bottom left box */
	struct BoxPack *brb; /* bottom right box */
	struct BoxPack *tlb; /* top left box */

	/* Store last intersecting boxes here
	 * speedup intersection testing */
	struct BoxPack *isect_cache[4];

	int index;
} boxVert;

/* free vert flags */
#define EPSILON 0.0000001f
#define BLF 1
#define TRF 2
#define TLF 4
#define BRF 8
#define CORNERFLAGS (BLF|TRF|TLF|BRF)

#define BL 0
#define TR 1
#define TL 2
#define BR 3

#define BOXLEFT(b)		((b)->v[BL]->x)
#define BOXRIGHT(b)		((b)->v[TR]->x)
#define BOXBOTTOM(b)	((b)->v[BL]->y)
#define BOXTOP(b)		((b)->v[TR]->y)
#define BOXAREA(b)		((b)->w * (b)->h)

#define UPDATE_V34X(b)	((b)->v[TL]->x = (b)->v[BL]->x); \
						((b)->v[BR]->x = (b)->v[TR]->x)
#define UPDATE_V34Y(b)	((b)->v[TL]->y = (b)->v[TR]->y); \
						((b)->v[BR]->y = (b)->v[BL]->y)
#define UPDATE_V34(b) UPDATE_V34X(b); UPDATE_V34Y(b)

#define SET_BOXLEFT(b, f)	(b)->v[TR]->x = f + (b)->w; \
							(b)->v[BL]->x = f;          \
							UPDATE_V34X(b)
#define SET_BOXRIGHT(b, f)	(b)->v[BL]->x = f - (b)->w; \
							(b)->v[TR]->x = f;          \
							UPDATE_V34X(b)
#define SET_BOXBOTTOM(b, f)	(b)->v[TR]->y = f + (b)->h; \
							(b)->v[BL]->y = f;          \
							UPDATE_V34Y(b)
#define SET_BOXTOP(b, f)	(b)->v[BL]->y = f - (b)->h; \
							(b)->v[TR]->y = f;          \
							UPDATE_V34Y(b)
#define BOXINTERSECT(b1, b2)                 \
	!(BOXLEFT(b1)   + EPSILON >= BOXRIGHT(b2) || \
	  BOXBOTTOM(b1) + EPSILON >= BOXTOP(b2)   || \
	  BOXRIGHT(b1)  - EPSILON <= BOXLEFT(b2)  || \
	  BOXTOP(b1)    - EPSILON <= BOXBOTTOM(b2))

#define MIN2(x,y)               ((x) < (y) ? (x) : (y))
#define MAX2(x,y)               ((x) > (y) ? (x) : (y))

#if 0
#define BOXDEBUG(b) \
	printf("\tBox Debug i %i, w:%.3f h:%.3f x:%.3f y:%.3f\n", \
	b->index, b->w, b->h, b->x, b->y)
#endif

/* qsort function - sort largest to smallest */
static int box_areasort(const void *p1, const void *p2)
{
	const BoxPack *b1 = p1, *b2 = p2;
	const float a1 = BOXAREA(b1);
	const float a2 = BOXAREA(b2);

	if      (a1 < a2) return  1;
	else if (a1 > a2) return -1;
	return 0;
}

/* qsort vertex sorting function
 * sorts from lower left to top right It uses the current box's width and height 
 * as offsets when sorting, this has the result of not placing boxes outside
 * the bounds of the existing backed area where possible
 * */
static float box_width;
static float box_height;
static boxVert *vertarray;

static int vertex_sort(const void *p1, const void *p2)
{
	boxVert *v1, *v2;
	float a1, a2;

	v1 = vertarray + ((int *)p1)[0];
	v2 = vertarray + ((int *)p2)[0];

	a1 = MAX2(v1->x+box_width, v1->y+box_height);
	a2 = MAX2(v2->x+box_width, v2->y+box_height);

	/* sort largest to smallest */
	if      (a1 > a2) return  1;
	else if (a1 < a2) return -1;
	return 0;
}
/* Main boxpacking function accessed from other functions
 * This sets boxes x,y to positive values, sorting from 0,0 outwards.
 * There is no limit to the space boxes may take, only that they will be packed
 * tightly into the lower left hand corner (0,0)
 * 
 * boxarray - a pre allocated array of boxes.
 * 		only the 'box->x' and 'box->y' are set, 'box->w' and 'box->h' are used,
 * 		'box->index' is not used at all, the only reason its there
 * 			is that the box array is sorted by area and programs need to be able
 * 			to have some way of writing the boxes back to the original data.
 * 	len - the number of boxes in the array.
 *	tot_width and tot_height are set so you can normalize the data.
 *  */
void BLI_box_pack_2D(BoxPack *boxarray, const int len, float *tot_width, float *tot_height)
{
	boxVert *vert; /* the current vert */
	int box_index, verts_pack_len, i, j, k, isect;
	int quad_flags[4] = {BLF, TRF, TLF, BRF}; /* use for looping */
	BoxPack *box, *box_test; /*current box and another for intersection tests*/
	int *vertex_pack_indices; /*an array of indices used for sorting verts*/

	if (!len) {
		*tot_width = 0.0f;
		*tot_height = 0.0f;
		return;
	}

	/* Sort boxes, biggest first */
	qsort(boxarray, len, sizeof(BoxPack), box_areasort);

	/* add verts to the boxes, these are only used internally  */
	vert = vertarray = MEM_mallocN(len * 4 * sizeof(boxVert), "BoxPack Verts");
	vertex_pack_indices = MEM_mallocN(len * 3 * sizeof(int), "BoxPack Indices");

	for (box = boxarray, box_index = 0, i = 0; box_index < len; box_index++, box++) {

		vert->blb = vert->brb = vert->tlb =
		        vert->isect_cache[0] = vert->isect_cache[1] =
		        vert->isect_cache[2] = vert->isect_cache[3] = NULL;
		vert->free = CORNERFLAGS &~ TRF;
		vert->trb = box;
		vert->index = i; i++;
		box->v[BL] = vert; vert++;
		
		vert->trb= vert->brb = vert->tlb =
		        vert->isect_cache[0] = vert->isect_cache[1] =
		        vert->isect_cache[2] = vert->isect_cache[3] = NULL;
		vert->free = CORNERFLAGS &~ BLF;
		vert->blb = box;
		vert->index = i; i++;
		box->v[TR] = vert; vert++;
		
		vert->trb = vert->blb = vert->tlb =
		        vert->isect_cache[0] = vert->isect_cache[1] =
		        vert->isect_cache[2] = vert->isect_cache[3] = NULL;
		vert->free = CORNERFLAGS &~ BRF;
		vert->brb = box;
		vert->index = i; i++;
		box->v[TL] = vert; vert++;
		
		vert->trb = vert->blb = vert->brb =
		        vert->isect_cache[0] = vert->isect_cache[1] =
		        vert->isect_cache[2] = vert->isect_cache[3] = NULL;
		vert->free = CORNERFLAGS &~ TLF;
		vert->tlb = box; 
		vert->index = i; i++;
		box->v[BR] = vert; vert++;
	}
	vert = NULL;

	/* Pack the First box!
	 * then enter the main box-packing loop */

	box = boxarray; /* get the first box  */
	/* First time, no boxes packed */
	box->v[BL]->free = 0; /* Can't use any if these */
	box->v[BR]->free &= ~(BLF | BRF);
	box->v[TL]->free &= ~(BLF | TLF);

	*tot_width = box->w;
	*tot_height = box->h;

	/* This sets all the vertex locations */
	SET_BOXLEFT(box, 0.0f);
	SET_BOXBOTTOM(box, 0.0f);
	box->x = box->y = 0.0f;

	for (i = 0; i < 3; i++)
		vertex_pack_indices[i] = box->v[i + 1]->index;
	verts_pack_len = 3;
	box++; /* next box, needed for the loop below */
	/* ...done packing the first box */

	/* Main boxpacking loop */
	for (box_index = 1; box_index < len; box_index++, box++) {

		/* These static floatds are used for sorting */
		box_width = box->w;
		box_height = box->h;

		qsort(vertex_pack_indices, verts_pack_len, sizeof(int), vertex_sort);

		/* Pack the box in with the others */
		/* sort the verts */
		isect = 1;

		for (i = 0; i < verts_pack_len && isect; i++) {
			vert = vertarray + vertex_pack_indices[i];
			/* printf("\ttesting vert %i %i %i %f %f\n", i,
			 * 		vert->free, verts_pack_len, vert->x, vert->y); */

			/* This vert has a free quadrant
			 * Test if we can place the box here
			 * vert->free & quad_flags[j] - Checks 
			 * */

			for (j = 0; (j < 4) && isect; j++) {
				if (vert->free & quad_flags[j]) {
					switch (j) {
						case BL:
							SET_BOXRIGHT(box, vert->x);
							SET_BOXTOP(box, vert->y);
							break;
						case TR:
							SET_BOXLEFT(box, vert->x);
							SET_BOXBOTTOM(box, vert->y);
							break;
						case TL:
							SET_BOXRIGHT(box, vert->x);
							SET_BOXBOTTOM(box, vert->y);
							break;
						case BR:
							SET_BOXLEFT(box, vert->x);
							SET_BOXTOP(box, vert->y);
							break;
					}

					/* Now we need to check that the box intersects
					 * with any other boxes
					 * Assume no intersection... */
					isect = 0;
					
					if (/* Constrain boxes to positive X/Y values */
						BOXLEFT(box) < 0.0f || BOXBOTTOM(box) < 0.0f ||
						/* check for last intersected */
						(	vert->isect_cache[j] &&
							BOXINTERSECT(box, vert->isect_cache[j])))
					{
						/* Here we check that the last intersected
						 * box will intersect with this one using
						 * isect_cache that can store a pointer to a
						 * box for each quadrant
						 * big speedup */
						isect = 1;
					}
					else {
						/* do a full search for colliding box
						 * this is really slow, some spatially divided
						 * data-structure would be better */
						for (box_test = boxarray; box_test != box; box_test++) {
							if (BOXINTERSECT(box, box_test)) {
								/* Store the last intersecting here as cache
								 * for faster checking next time around */
								vert->isect_cache[j] = box_test;
								isect = 1;
								break;
							}
						}
					}

					if (!isect) {

						/* maintain the total width and height */
						(*tot_width) = MAX2(BOXRIGHT(box), (*tot_width));
						(*tot_height) = MAX2(BOXTOP(box), (*tot_height));

						/* Place the box */
						vert->free &= ~quad_flags[j];

						switch (j) {
							case TR:
								box->v[BL] = vert;
								vert->trb = box;
								break;
							case TL:
								box->v[BR] = vert;
								vert->tlb = box;
								break;
							case BR:
								box->v[TL] = vert;
								vert->brb = box;
								break;
							case BL:
								box->v[TR] = vert;
								vert->blb = box;
								break;
						}

						/* Mask free flags for verts that are
						 * on the bottom or side so we don't get
						 * boxes outside the given rectangle ares
						 * 
						 * We can do an else/if here because only the first 
						 * box can be at the very bottom left corner */
						if (BOXLEFT(box) <= 0) {
							box->v[TL]->free &= ~(TLF | BLF);
							box->v[BL]->free &= ~(TLF | BLF);
						}
						else if (BOXBOTTOM(box) <= 0) {
							box->v[BL]->free &= ~(BRF | BLF);
							box->v[BR]->free &= ~(BRF | BLF);
						}

						/* The following block of code does a logical
						 * check with 2 adjacent boxes, its possible to
						 * flag verts on one or both of the boxes 
						 * as being used by checking the width or
						 * height of both boxes */
						if (vert->tlb && vert->trb && (box == vert->tlb || box == vert->trb)) {
							if (vert->tlb->h > vert->trb->h) {
								vert->trb->v[TL]->free &= ~(TLF | BLF);
							}
							else if (vert->tlb->h < vert->trb->h) {
								vert->tlb->v[TR]->free &= ~(TRF | BRF);
							}
							else { /*same*/
								vert->tlb->v[TR]->free &= ~BLF;
								vert->trb->v[TL]->free &= ~BRF;
							}
						}
						else if (vert->blb && vert->brb && (box == vert->blb || box == vert->brb)) {
							if (vert->blb->h > vert->brb->h) {
								vert->brb->v[BL]->free &= ~(TLF | BLF);
							}
							else if (vert->blb->h < vert->brb->h) {
								vert->blb->v[BR]->free &= ~(TRF | BRF);
							}
							else { /*same*/
								vert->blb->v[BR]->free &= ~TRF;
								vert->brb->v[BL]->free &= ~TLF;
							}
						}
						/* Horizontal */
						if (vert->tlb && vert->blb && (box == vert->tlb || box == vert->blb)) {
							if (vert->tlb->w > vert->blb->w) {
								vert->blb->v[TL]->free &= ~(TLF | TRF);
							}
							else if (vert->tlb->w < vert->blb->w) {
								vert->tlb->v[BL]->free &= ~(BLF | BRF);
							}
							else { /*same*/
								vert->blb->v[TL]->free &= ~TRF;
								vert->tlb->v[BL]->free &= ~BRF;
							}
						}
						else if (vert->trb && vert->brb && (box == vert->trb || box == vert->brb)) {
							if (vert->trb->w > vert->brb->w) {
								vert->brb->v[TR]->free &= ~(TLF | TRF);
							}
							else if (vert->trb->w < vert->brb->w) {
								vert->trb->v[BR]->free &= ~(BLF | BRF);
							}
							else { /*same*/
								vert->brb->v[TR]->free &= ~TLF;
								vert->trb->v[BR]->free &= ~BLF;
							}
						}
						/* End logical check */

						for (k = 0; k < 4; k++) {
							if (box->v[k] != vert) {
								vertex_pack_indices[verts_pack_len] = box->v[k]->index;
								verts_pack_len++;
							}
						}
						/* The Box verts are only used internally
						 * Update the box x and y since thats what external
						 * functions will see */
						box->x = BOXLEFT(box);
						box->y = BOXBOTTOM(box);
					}
				}
			}
		}
	}

	/* free all the verts, not really needed because they shouldn't be
	 * touched anymore but accessing the pointers would crash blender */
	for (box_index = 0; box_index < len; box_index++) {
		box = boxarray + box_index;
		box->v[0] = box->v[1] = box->v[2] = box->v[3] = NULL;
	}
	MEM_freeN(vertex_pack_indices);
	MEM_freeN(vertarray);
}

