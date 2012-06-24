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
 * The Original Code is Copyright (C) 2012 Blender Foundation.
 * All rights reserved.
 *
 * The Original Code is: all of this file.
 *
 * Contributor(s): Peter Larabell.
 *
 * ***** END GPL LICENSE BLOCK *****
 */
/** \file raskter.c
 *  \ingroup RASKTER
 */

#include <stdlib.h>
#include "raskter.h"

#define __PLX__FAKE_AA__

/* from BLI_utildefines.h */
#define MIN2(x, y)               ( (x) < (y) ? (x) : (y) )
#define MAX2(x, y)               ( (x) > (y) ? (x) : (y) )
#define ABS(a)          ( (a) < 0 ? (-(a)) : (a) )

struct e_status {
	int x;
	int ybeg;
	int xshift;
	int xdir;
	int drift;
	int drift_inc;
	int drift_dec;
	int num;
	struct e_status *e_next;
};

struct r_buffer_stats {
	float *buf;
	int sizex;
	int sizey;
};

struct r_fill_context {
	struct e_status *all_edges, *possible_edges;
	struct r_buffer_stats rb;
};

/*
 * Sort all the edges of the input polygon by Y, then by X, of the "first" vertex encountered.
 * This will ensure we can scan convert the entire poly in one pass.
 *
 * Really the poly should be clipped to the frame buffer's dimensions here for speed of drawing
 * just the poly. Since the DEM code could end up being coupled with this, we'll keep it separate
 * for now.
 */
static void preprocess_all_edges(struct r_fill_context *ctx, struct poly_vert *verts, int num_verts, struct e_status *open_edge)
{
	int i;
	int xbeg;
	int ybeg;
	int xend;
	int yend;
	int dx;
	int dy;
	int temp_pos;
	int xdist;
	struct e_status *e_new;
	struct e_status *next_edge;
	struct e_status **next_edge_ref;
	struct poly_vert *v;
	/* set up pointers */
	v = verts;
	ctx->all_edges = NULL;
	/* loop all verts */
	for (i = 0; i < num_verts; i++) {
		/* determine beginnings and endings of edges, linking last vertex to first vertex */
		xbeg = v[i].x;
		ybeg = v[i].y;
		if (i) {
			/* we're not at the last vert, so end of the edge is the previous vertex */
			xend = v[i - 1].x;
			yend = v[i - 1].y;
		} else {
			/* we're at the first vertex, so the "end" of this edge is the last vertex */
			xend = v[num_verts - 1].x;
			yend = v[num_verts - 1].y;
		}
		/* make sure our edges are facing the correct direction */
		if (ybeg > yend) {
			/* flip the Xs */
			temp_pos = xbeg;
			xbeg = xend;
			xend = temp_pos;
			/* flip the Ys */
			temp_pos = ybeg;
			ybeg = yend;
			yend = temp_pos;
		}

		/* calculate y delta */
		dy = yend - ybeg;
		/* dont draw horizontal lines directly, they are scanned as part of the edges they connect, so skip em. :) */
		if (dy) {
			/* create the edge and determine it's slope (for incremental line drawing) */
			e_new = open_edge++;

			/* calculate x delta */
			dx = xend - xbeg;
			if (dx > 0) {
				e_new->xdir = 1;
				xdist = dx;
			} else {
				e_new->xdir = -1;
				xdist = -dx;
			}

			e_new->x = xbeg;
			e_new->ybeg = ybeg;
			e_new->num = dy;
			e_new->drift_dec = dy;

			/* calculate deltas for incremental drawing */
			if (dx >= 0) {
				e_new->drift = 0;
			} else {
				e_new->drift = -dy + 1;
			}
			if (dy >= xdist) {
				e_new->drift_inc = xdist;
				e_new->xshift = 0;
			} else {
				e_new->drift_inc = xdist % dy;
				e_new->xshift = (xdist / dy) * e_new->xdir;
			}
			next_edge_ref = &ctx->all_edges;
			/* link in all the edges, in sorted order */
			for (;; ) {
				next_edge = *next_edge_ref;
				if (!next_edge || (next_edge->ybeg > ybeg) || ((next_edge->ybeg == ybeg) && (next_edge->x >= xbeg))) {
					e_new->e_next = next_edge;
					*next_edge_ref = e_new;
					break;
				}
				next_edge_ref = &next_edge->e_next;
			}
		}
	}
}

/*
 * This function clips drawing to the frame buffer. That clipping will likely be moved into the preprocessor
 * for speed, but waiting on final design choices for curve-data before eliminating data the DEM code will need
 * if it ends up being coupled with this function.
 */
static int rast_scan_fill(struct r_fill_context *ctx, struct poly_vert *verts, int num_verts, float intensity)
{
	int x_curr;                 /* current pixel position in X */
	int y_curr;                 /* current scan line being drawn */
	int yp;                     /* y-pixel's position in frame buffer */
	int swixd = 0;              /* whether or not edges switched position in X */
	float *cpxl;                /* pixel pointers... */
	float *mpxl;
	float *spxl;
	struct e_status *e_curr;    /* edge pointers... */
	struct e_status *e_temp;
	struct e_status *edgbuf;
	struct e_status **edgec;


	/*
	 * If the number of verts specified to render as a polygon is less than 3,
	 * return immediately. Obviously we cant render a poly with sides < 3. The
	 * return for this we set to 1, simply so it can be distinguished from the
	 * next place we could return, /home/guest/blender-svn/soc-2011-tomato/intern/raskter/raskter.
	 * which is a failure to allocate memory.
	 */
	if (num_verts < 3) {
		return(1);
	}

	/*
	 * Try to allocate an edge buffer in memory. needs to be the size of the edge tracking data
	 * multiplied by the number of edges, which is always equal to the number of verts in
	 * a 2D polygon. Here we return 0 to indicate a memory allocation failure, as opposed to a 1 for
	 * the preceeding error, which was a rasterization request on a 2D poly with less than
	 * 3 sides.
	 */
	if ((edgbuf = (struct e_status *)(malloc(sizeof(struct e_status) * num_verts))) == NULL) {
		return(0);
	}

	/*
	 * Do some preprocessing on all edges. This constructs a table structure in memory of all
	 * the edge properties and can "flip" some edges so sorting works correctly.
	 */
	preprocess_all_edges(ctx, verts, num_verts, edgbuf);

	/*
	 * Set the pointer for tracking the edges currently in processing to NULL to make sure
	 * we don't get some crazy value after initialization.
	 */
	ctx->possible_edges = NULL;

	/*
	 * Loop through all scan lines to be drawn. Since we sorted by Y values during
	 * preprocess_all_edges(), we can already exact values for the lowest and
	 * highest Y values we could possibly need by induction. The preprocessing sorted
	 * out edges by Y position, we can cycle the current edge being processed once
	 * it runs out of Y pixels. When we have no more edges, meaning the current edge
	 * is NULL after setting the "current" edge to be the previous current edge's
	 * "next" edge in the Y sorted edge connection chain, we can stop looping Y values,
	 * since we can't possibly have more scan lines if we ran out of edges. :)
	 *
	 * TODO: This clips Y to the frame buffer, which should be done in the preprocessor, but for now is done here.
	 *       Will get changed once DEM code gets in.
	 */
	for (y_curr = ctx->all_edges->ybeg; (ctx->all_edges || ctx->possible_edges); y_curr++) {

		/*
		 * Link any edges that start on the current scan line into the list of
		 * edges currently needed to draw at least this, if not several, scan lines.
		 */

		/*
		 * Set the current edge to the beginning of the list of edges to be rasterized
		 * into this scan line.
		 *
		 * We could have lots of edge here, so iterate over all the edges needed. The
		 * preprocess_all_edges() function sorted edges by X within each chunk of Y sorting
		 * so we safely cycle edges to thier own "next" edges in order.
		 *
		 * At each iteration, make sure we still have a non-NULL edge.
		 */
		for (edgec = &ctx->possible_edges; ctx->all_edges && (ctx->all_edges->ybeg == y_curr); ) {
			x_curr = ctx->all_edges->x;                  /* Set current X position. */
			for (;; ) {                                  /* Start looping edges. Will break when edges run out. */
				e_curr = *edgec;                         /* Set up a current edge pointer. */
				if (!e_curr || (e_curr->x >= x_curr)) {  /* If we have an no edge, or we need to skip some X-span, */
					e_temp = ctx->all_edges->e_next;     /* set a temp "next" edge to test. */
					*edgec = ctx->all_edges;             /* Add this edge to the list to be scanned. */
					ctx->all_edges->e_next = e_curr;     /* Set up the next edge. */
					edgec = &ctx->all_edges->e_next;     /* Set our list to the next edge's location in memory. */
					ctx->all_edges = e_temp;             /* Skip the NULL or bad X edge, set pointer to next edge. */
					break;                               /* Stop looping edges (since we ran out or hit empty X span. */
				} else {
					edgec = &e_curr->e_next;             /* Set the pointer to the edge list the "next" edge. */
				}
			}
		}

		/*
		 * Determine the current scan line's offset in the pixel buffer based on its Y position.
		 * Basically we just multiply the current scan line's Y value by the number of pixels in each line.
		 */
		yp = y_curr * ctx->rb.sizex;
		/*
		 * Set a "scan line pointer" in memory. The location of the buffer plus the row offset.
		 */
		spxl = ctx->rb.buf + (yp);
		/*
		 * Set up the current edge to the first (in X) edge. The edges which could possibly be in this
		 * list were determined in the preceeding edge loop above. They were already sorted in X by the
		 * initial processing function.
		 *
		 * At each iteration, test for a NULL edge. Since we'll keep cycling edge's to their own "next" edge
		 * we will eventually hit a NULL when the list runs out.
		 */
		for (e_curr = ctx->possible_edges; e_curr; e_curr = e_curr->e_next) {
			/*
			 * Calculate a span of pixels to fill on the current scan line.
			 *
			 * Set the current pixel pointer by adding the X offset to the scan line's start offset.
			 * Cycle the current edge the next edge.
			 * Set the max X value to draw to be one less than the next edge's first pixel. This way we are
			 * sure not to ever get into a situation where we have overdraw. (drawing the same pixel more than
			 * one time because it's on a vertex connecting two edges)
			 *
			 * Then blast through all the pixels in the span, advancing the pointer and setting the color to white.
			 *
			 * TODO: Here we clip to the scan line, this is not efficient, and should be done in the preprocessor,
			 *       but for now it is done here until the DEM code comes in.
			 */

			/* set up xmin and xmax bounds on this scan line */
			cpxl = spxl + MAX2(e_curr->x, 0);
			e_curr = e_curr->e_next;
			mpxl = spxl + MIN2(e_curr->x, ctx->rb.sizex) - 1;

			if ((y_curr >= 0) && (y_curr < ctx->rb.sizey)) {
				/* draw the pixels. */
				for(; cpxl <= mpxl; *cpxl++ += intensity);
			}
		}

		/*
		 * Loop through all edges of polygon that could be hit by this scan line,
		 * and figure out their x-intersections with the next scan line.
		 *
		 * Either A.) we wont have any more edges to test, or B.) we just add on the
		 * slope delta computed in preprocessing step. Since this draws non-antialiased
		 * polygons, we dont have fractional positions, so we only move in x-direction
		 * when needed to get all the way to the next pixel over...
		 */
		for (edgec = &ctx->possible_edges; (e_curr = *edgec); ) {
			if (!(--(e_curr->num))) {
				*edgec = e_curr->e_next;
			} else {
				e_curr->x += e_curr->xshift;
				if ((e_curr->drift += e_curr->drift_inc) > 0) {
					e_curr->x += e_curr->xdir;
					e_curr->drift -= e_curr->drift_dec;
				}
				edgec = &e_curr->e_next;
			}
		}
		/*
		 * It's possible that some edges may have crossed during the last step, so we'll be sure
		 * that we ALWAYS intersect scan lines in order by shuffling if needed to make all edges
		 * sorted by x-intersection coordinate. We'll always scan through at least once to see if
		 * edges crossed, and if so, we set the 'swixd' flag. If 'swixd' gets set on the initial
		 * pass, then we know we need to sort by x, so then cycle through edges again and perform
		 * the sort.-
		 */
		if (ctx->possible_edges) {
			for (edgec = &ctx->possible_edges; (e_curr = *edgec)->e_next; edgec = &(*edgec)->e_next) {
				/* if the current edge hits scan line at greater X than the next edge, we need to exchange the edges */
				if (e_curr->x > e_curr->e_next->x) {
					*edgec = e_curr->e_next;
					/* exchange the pointers */
					e_temp = e_curr->e_next->e_next;
					e_curr->e_next->e_next = e_curr;
					e_curr->e_next = e_temp;
					/* set flag that we had at least one switch */
					swixd = 1;
				}
			}
			/* if we did have a switch, look for more (there will more if there was one) */
			for (;; ) {
				/* reset exchange flag so it's only set if we encounter another one */
				swixd = 0;
				for (edgec = &ctx->possible_edges; (e_curr = *edgec)->e_next; edgec = &(*edgec)->e_next) {
					/* again, if current edge hits scan line at higher X than next edge, exchange the edges and set flag */
					if (e_curr->x > e_curr->e_next->x) {
						*edgec = e_curr->e_next;
						/* exchange the pointers */
						e_temp = e_curr->e_next->e_next;
						e_curr->e_next->e_next = e_curr;
						e_curr->e_next = e_temp;
						/* flip the exchanged flag */
						swixd = 1;
					}
				}
				/* if we had no exchanges, we're done reshuffling the pointers */
				if (!swixd) {
					break;
				}
			}
		}
	}

	free(edgbuf);
	return 1;
}

int PLX_raskterize(float (*base_verts)[2], int num_base_verts,
				   float *buf, int buf_x, int buf_y, int do_mask_AA) {
	int subdiv_AA = (do_mask_AA != 0)? 8:0;
	int i;                                   /* i: Loop counter. */
	int sAx;
	int sAy;
	struct poly_vert *ply;                   /* ply: Pointer to a list of integer buffer-space vertex coordinates. */
	struct r_fill_context ctx = {0};
	const float buf_x_f = (float)(buf_x);
	const float buf_y_f = (float)(buf_y);
	float div_offset=(1.0f / (float)(subdiv_AA));
	float div_offset_static = 0.5f * (float)(subdiv_AA) * div_offset;
	/*
	 * Allocate enough memory for our poly_vert list. It'll be the size of the poly_vert
	 * data structure multiplied by the number of base_verts.
	 *
	 * In the event of a failure to allocate the memory, return 0, so this error can
	 * be distinguished as a memory allocation error.
	 */
	if ((ply = (struct poly_vert *)(malloc(sizeof(struct poly_vert) * num_base_verts))) == NULL) {
		return(0);
	}

	ctx.rb.buf = buf;                            /* Set the output buffer pointer. */
	ctx.rb.sizex = buf_x;                        /* Set the output buffer size in X. (width) */
	ctx.rb.sizey = buf_y;                        /* Set the output buffer size in Y. (height) */
	/*
	 * Loop over all verts passed in to be rasterized. Each vertex's X and Y coordinates are
	 * then converted from normalized screen space (0.0 <= POS <= 1.0) to integer coordinates
	 * in the buffer-space coordinates passed in inside buf_x and buf_y.
	 *
	 * It's worth noting that this function ONLY outputs fully white pixels in a mask. Every pixel
	 * drawn will be 1.0f in value, there is no anti-aliasing.
	 */

	if(!subdiv_AA) {
	for (i = 0; i < num_base_verts; i++) {                          /* Loop over all base_verts. */
			ply[i].x = (int)((base_verts[i][0] * buf_x_f) + 0.5f);       /* Range expand normalized X to integer buffer-space X. */
			ply[i].y = (int)((base_verts[i][1] * buf_y_f) + 0.5f); /* Range expand normalized Y to integer buffer-space Y. */
	}

		i = rast_scan_fill(&ctx, ply, num_base_verts,1.0f);  /* Call our rasterizer, passing in the integer coords for each vert. */
	} else {
		for(sAx=0; sAx < subdiv_AA; sAx++) {
			for(sAy=0; sAy < subdiv_AA; sAy++) {
				for(i=0; i < num_base_verts; i++) {
					ply[i].x = (int)((base_verts[i][0]*buf_x_f)+0.5f - div_offset_static + (div_offset*(float)(sAx)));
					ply[i].y = (int)((base_verts[i][1]*buf_y_f)+0.5f - div_offset_static + (div_offset*(float)(sAy)));
				}
				i = rast_scan_fill(&ctx, ply, num_base_verts,(1.0f / (float)(subdiv_AA*subdiv_AA)));
			}
		}
	}
	free(ply);                                      /* Free the memory allocated for the integer coordinate table. */
	return(i);                                      /* Return the value returned by the rasterizer. */
}

/*
 * This function clips drawing to the frame buffer. That clipping will likely be moved into the preprocessor
 * for speed, but waiting on final design choices for curve-data before eliminating data the DEM code will need
 * if it ends up being coupled with this function.
 */
static int rast_scan_feather(struct r_fill_context *ctx,
                             float (*base_verts_f)[2], int num_base_verts,
							 struct poly_vert *feather_verts, float(*feather_verts_f)[2], int num_feather_verts) {
	int x_curr;                 /* current pixel position in X */
	int y_curr;                 /* current scan line being drawn */
	int yp;                     /* y-pixel's position in frame buffer */
	int swixd = 0;              /* whether or not edges switched position in X */
	float *cpxl;                /* pixel pointers... */
	float *mpxl;
	float *spxl;
	struct e_status *e_curr;    /* edge pointers... */
	struct e_status *e_temp;
	struct e_status *edgbuf;
	struct e_status **edgec;

	/* from dem */
	int a;                          // a = temporary pixel index buffer loop counter
	float fsz;                        // size of the frame
	unsigned int rsl;               // long used for finding fast 1.0/sqrt
	float rsf;                      // float used for finding fast 1.0/sqrt
	const float rsopf = 1.5f;       // constant float used for finding fast 1.0/sqrt

	//unsigned int gradientFillOffset;
	float t;
	float ud;                // ud = unscaled edge distance
	float dmin;              // dmin = minimun edge distance
	float odist;                    // odist = current outer edge distance
	float idist;                    // idist = current inner edge distance
	float dx;                         // dx = X-delta (used for distance proportion calculation)
	float dy;                         // dy = Y-delta (used for distance proportion calculation)
	float xpxw = (1.0f / (float)(ctx->rb.sizex));  // xpxw = normalized pixel width
	float ypxh = (1.0f / (float)(ctx->rb.sizey));  // ypxh = normalized pixel height

	/*
	 * If the number of verts specified to render as a polygon is less than 3,
	 * return immediately. Obviously we cant render a poly with sides < 3. The
	 * return for this we set to 1, simply so it can be distinguished from the
	 * next place we could return, /home/guest/blender-svn/soc-2011-tomato/intern/raskter/raskter
	 * which is a failure to allocate memory.
	 */
	if (num_feather_verts < 3) {
		return(1);
	}

	/*
	 * Try to allocate an edge buffer in memory. needs to be the size of the edge tracking data
	 * multiplied by the number of edges, which is always equal to the number of verts in
	 * a 2D polygon. Here we return 0 to indicate a memory allocation failure, as opposed to a 1 for
	 * the preceeding error, which was a rasterization request on a 2D poly with less than
	 * 3 sides.
	 */
	if ((edgbuf = (struct e_status *)(malloc(sizeof(struct e_status) * num_feather_verts))) == NULL) {
		return(0);
	}

	/*
	 * Do some preprocessing on all edges. This constructs a table structure in memory of all
	 * the edge properties and can "flip" some edges so sorting works correctly.
	 */
	preprocess_all_edges(ctx, feather_verts, num_feather_verts, edgbuf);

	/*
	 * Set the pointer for tracking the edges currently in processing to NULL to make sure
	 * we don't get some crazy value after initialization.
	 */
	ctx->possible_edges = NULL;

	/*
	 * Loop through all scan lines to be drawn. Since we sorted by Y values during
	 * preprocess_all_edges(), we can already exact values for the lowest and
	 * highest Y values we could possibly need by induction. The preprocessing sorted
	 * out edges by Y position, we can cycle the current edge being processed once
	 * it runs out of Y pixels. When we have no more edges, meaning the current edge
	 * is NULL after setting the "current" edge to be the previous current edge's
	 * "next" edge in the Y sorted edge connection chain, we can stop looping Y values,
	 * since we can't possibly have more scan lines if we ran out of edges. :)
	 *
	 * TODO: This clips Y to the frame buffer, which should be done in the preprocessor, but for now is done here.
	 *       Will get changed once DEM code gets in.
	 */
	for (y_curr = ctx->all_edges->ybeg; (ctx->all_edges || ctx->possible_edges); y_curr++) {

		/*
		 * Link any edges that start on the current scan line into the list of
		 * edges currently needed to draw at least this, if not several, scan lines.
		 */

		/*
		 * Set the current edge to the beginning of the list of edges to be rasterized
		 * into this scan line.
		 *
		 * We could have lots of edge here, so iterate over all the edges needed. The
		 * preprocess_all_edges() function sorted edges by X within each chunk of Y sorting
		 * so we safely cycle edges to thier own "next" edges in order.
		 *
		 * At each iteration, make sure we still have a non-NULL edge.
		 */
		for (edgec = &ctx->possible_edges; ctx->all_edges && (ctx->all_edges->ybeg == y_curr); ) {
			x_curr = ctx->all_edges->x;                  /* Set current X position. */
			for (;; ) {                                  /* Start looping edges. Will break when edges run out. */
				e_curr = *edgec;                         /* Set up a current edge pointer. */
				if (!e_curr || (e_curr->x >= x_curr)) {  /* If we have an no edge, or we need to skip some X-span, */
					e_temp = ctx->all_edges->e_next;     /* set a temp "next" edge to test. */
					*edgec = ctx->all_edges;             /* Add this edge to the list to be scanned. */
					ctx->all_edges->e_next = e_curr;     /* Set up the next edge. */
					edgec = &ctx->all_edges->e_next;     /* Set our list to the next edge's location in memory. */
					ctx->all_edges = e_temp;             /* Skip the NULL or bad X edge, set pointer to next edge. */
					break;                               /* Stop looping edges (since we ran out or hit empty X span. */
				} else {
					edgec = &e_curr->e_next;             /* Set the pointer to the edge list the "next" edge. */
				}
			}
		}

		/*
		 * Determine the current scan line's offset in the pixel buffer based on its Y position.
		 * Basically we just multiply the current scan line's Y value by the number of pixels in each line.
		 */
		yp = y_curr * ctx->rb.sizex;
		/*
		 * Set a "scan line pointer" in memory. The location of the buffer plus the row offset.
		 */
		spxl = ctx->rb.buf + (yp);
		/*
		 * Set up the current edge to the first (in X) edge. The edges which could possibly be in this
		 * list were determined in the preceeding edge loop above. They were already sorted in X by the
		 * initial processing function.
		 *
		 * At each iteration, test for a NULL edge. Since we'll keep cycling edge's to their own "next" edge
		 * we will eventually hit a NULL when the list runs out.
		 */
		for (e_curr = ctx->possible_edges; e_curr; e_curr = e_curr->e_next) {
			/*
			 * Calculate a span of pixels to fill on the current scan line.
			 *
			 * Set the current pixel pointer by adding the X offset to the scan line's start offset.
			 * Cycle the current edge the next edge.
			 * Set the max X value to draw to be one less than the next edge's first pixel. This way we are
			 * sure not to ever get into a situation where we have overdraw. (drawing the same pixel more than
			 * one time because it's on a vertex connecting two edges)
			 *
			 * Then blast through all the pixels in the span, advancing the pointer and setting the color to white.
			 *
			 * TODO: Here we clip to the scan line, this is not efficient, and should be done in the preprocessor,
			 *       but for now it is done here until the DEM code comes in.
			 */

			/* set up xmin and xmax bounds on this scan line */
			cpxl = spxl + MAX2(e_curr->x, 0);
			e_curr = e_curr->e_next;
			mpxl = spxl + MIN2(e_curr->x, ctx->rb.sizex) - 1;

			if ((y_curr >= 0) && (y_curr < ctx->rb.sizey)) {
				t = ((float)((cpxl - spxl) % ctx->rb.sizex) + 0.5f) * xpxw;
				fsz = ((float)(y_curr) + 0.5f) * ypxh;
				/* draw the pixels. */
				for (; cpxl <= mpxl; cpxl++, t += xpxw) {
					//do feather check
					// first check that pixel isn't already full, and only operate if it is not
					if (*cpxl < 0.9999f) {

						dmin = 2.0f;                        // reset min distance to edge pixel
						for (a = 0; a < num_feather_verts; a++) { // loop through all outer edge buffer pixels
							dy = t - feather_verts_f[a][0];          // set dx to gradient pixel column - outer edge pixel row
							dx = fsz - feather_verts_f[a][1];        // set dy to gradient pixel row - outer edge pixel column
							ud = dx * dx + dy * dy;               // compute sum of squares
							if (ud < dmin) {                      // if our new sum of squares is less than the current minimum
								dmin = ud;                        // set a new minimum equal to the new lower value
							}
						}
						odist = dmin;                    // cast outer min to a float
						rsf = odist * 0.5f;                       //
						rsl = *(unsigned int *)&odist;            // use some peculiar properties of the way bits are stored
						rsl = 0x5f3759df - (rsl >> 1);            // in floats vs. unsigned ints to compute an approximate
						odist = *(float *)&rsl;                   // reciprocal square root
						odist = odist * (rsopf - (rsf * odist * odist));  // -- ** this line can be iterated for more accuracy ** --
						odist = odist * (rsopf - (rsf * odist * odist));
						dmin = 2.0f;                        // reset min distance to edge pixel
						for (a = 0; a < num_base_verts; a++) {    // loop through all inside edge pixels
							dy = t - base_verts_f[a][0];             // compute delta in Y from gradient pixel to inside edge pixel
							dx = fsz - base_verts_f[a][1];           // compute delta in X from gradient pixel to inside edge pixel
							ud = dx * dx + dy * dy;   // compute sum of squares
							if (ud < dmin) {          // if our new sum of squares is less than the current minimum we've found
								dmin = ud;            // set a new minimum equal to the new lower value
							}
						}
						idist = dmin;                    // cast inner min to a float
						rsf = idist * 0.5f;                       //
						rsl = *(unsigned int *)&idist;            //
						rsl = 0x5f3759df - (rsl >> 1);            // see notes above
						idist = *(float *)&rsl;                   //
						idist = idist * (rsopf - (rsf * idist * idist));  //
						idist = idist * (rsopf - (rsf * idist * idist));
						/*
						 * Note once again that since we are using reciprocals of distance values our
						 * proportion is already the correct intensity, and does not need to be
						 * subracted from 1.0 like it would have if we used real distances.
						 */

						/* set intensity, do the += so overlapping gradients are additive */
						*cpxl = (idist / (idist + odist));
					}
				}
			}
		}

		/*
		 * Loop through all edges of polygon that could be hit by this scan line,
		 * and figure out their x-intersections with the next scan line.
		 *
		 * Either A.) we wont have any more edges to test, or B.) we just add on the
		 * slope delta computed in preprocessing step. Since this draws non-antialiased
		 * polygons, we dont have fractional positions, so we only move in x-direction
		 * when needed to get all the way to the next pixel over...
		 */
		for (edgec = &ctx->possible_edges; (e_curr = *edgec); ) {
			if (!(--(e_curr->num))) {
				*edgec = e_curr->e_next;
			} else {
				e_curr->x += e_curr->xshift;
				if ((e_curr->drift += e_curr->drift_inc) > 0) {
					e_curr->x += e_curr->xdir;
					e_curr->drift -= e_curr->drift_dec;
				}
				edgec = &e_curr->e_next;
			}
		}
		/*
		 * It's possible that some edges may have crossed during the last step, so we'll be sure
		 * that we ALWAYS intersect scan lines in order by shuffling if needed to make all edges
		 * sorted by x-intersection coordinate. We'll always scan through at least once to see if
		 * edges crossed, and if so, we set the 'swixd' flag. If 'swixd' gets set on the initial
		 * pass, then we know we need to sort by x, so then cycle through edges again and perform
		 * the sort.-
		 */
		if (ctx->possible_edges) {
			for (edgec = &ctx->possible_edges; (e_curr = *edgec)->e_next; edgec = &(*edgec)->e_next) {
				/* if the current edge hits scan line at greater X than the next edge, we need to exchange the edges */
				if (e_curr->x > e_curr->e_next->x) {
					*edgec = e_curr->e_next;
					/* exchange the pointers */
					e_temp = e_curr->e_next->e_next;
					e_curr->e_next->e_next = e_curr;
					e_curr->e_next = e_temp;
					/* set flag that we had at least one switch */
					swixd = 1;
				}
			}
			/* if we did have a switch, look for more (there will more if there was one) */
			for (;; ) {
				/* reset exchange flag so it's only set if we encounter another one */
				swixd = 0;
				for (edgec = &ctx->possible_edges; (e_curr = *edgec)->e_next; edgec = &(*edgec)->e_next) {
					/* again, if current edge hits scan line at higher X than next edge,
					 * exchange the edges and set flag */
					if (e_curr->x > e_curr->e_next->x) {
						*edgec = e_curr->e_next;
						/* exchange the pointers */
						e_temp = e_curr->e_next->e_next;
						e_curr->e_next->e_next = e_curr;
						e_curr->e_next = e_temp;
						/* flip the exchanged flag */
						swixd = 1;
					}
				}
				/* if we had no exchanges, we're done reshuffling the pointers */
				if (!swixd) {
					break;
				}
			}
		}
	}

	free(edgbuf);
	return 1;
}

int PLX_raskterize_feather(float (*base_verts)[2], int num_base_verts, float (*feather_verts)[2], int num_feather_verts,
						   float *buf, int buf_x, int buf_y) {
	int i;                            /* i: Loop counter. */
	struct poly_vert *fe;             /* fe: Pointer to a list of integer buffer-space feather vertex coords. */
	struct r_fill_context ctx = {0};

	/* for faster multiply */
	const float buf_x_f = (float)buf_x;
	const float buf_y_f = (float)buf_y;

	/*
	 * Allocate enough memory for our poly_vert list. It'll be the size of the poly_vert
	 * data structure multiplied by the number of verts.
	 *
	 * In the event of a failure to allocate the memory, return 0, so this error can
	 * be distinguished as a memory allocation error.
	 */
	if ((fe = (struct poly_vert *)(malloc(sizeof(struct poly_vert) * num_feather_verts))) == NULL) {
		return(0);
	}

	/*
	 * Loop over all verts passed in to be rasterized. Each vertex's X and Y coordinates are
	 * then converted from normalized screen space (0.0 <= POS <= 1.0) to integer coordinates
	 * in the buffer-space coordinates passed in inside buf_x and buf_y.
	 *
	 * It's worth noting that this function ONLY outputs fully white pixels in a mask. Every pixel
	 * drawn will be 1.0f in value, there is no anti-aliasing.
	 */
	for (i = 0; i < num_feather_verts; i++) {            /* Loop over all verts. */
		fe[i].x = (int)((feather_verts[i][0] * buf_x_f) + 0.5f);  /* Range expand normalized X to integer buffer-space X. */
		fe[i].y = (int)((feather_verts[i][1] * buf_y_f) + 0.5f);  /* Range expand normalized Y to integer buffer-space Y. */
	}

	ctx.rb.buf = buf;                            /* Set the output buffer pointer. */
	ctx.rb.sizex = buf_x;                        /* Set the output buffer size in X. (width) */
	ctx.rb.sizey = buf_y;                        /* Set the output buffer size in Y. (height) */

	/* Call our rasterizer, passing in the integer coords for each vert. */
	i = rast_scan_feather(&ctx, base_verts, num_base_verts, fe, feather_verts, num_feather_verts);
	free(fe);
	return i;                                   /* Return the value returned by the rasterizer. */
}

#ifndef __PLX__FAKE_AA__

static int get_range_expanded_pixel_coord(float normalized_value, int max_value)
{
	return (int)((normalized_value * (float)(max_value)) + 0.5f);
}

static float get_pixel_intensity(float *buf, int buf_x, int buf_y, int pos_x, int pos_y)
{
	if(pos_x < 0 || pos_x >= buf_x || pos_y < 0 || pos_y >= buf_y) {
		return 0.0f;
	}
	return buf[(pos_y * buf_y) + buf_x];
}

static float get_pixel_intensity_bilinear(float *buf, int buf_x, int buf_y, float u, float v)
{
	int a;
	int b;
	int a_plus_1;
	int b_plus_1;
	float prop_u;
	float prop_v;
	float inv_prop_u;
	float inv_prop_v;
	if(u<0.0f || u>1.0f || v<0.0f || v>1.0f) {
		return 0.0f;
	}
	u = u * (float)(buf_x) - 0.5f;
	v = v * (float)(buf_y) - 0.5f;
	a = (int)(u);
	b = (int)(v);
	prop_u = u - (float)(a);
	prop_v = v - (float)(b);
	inv_prop_u = 1.0f - prop_u;
	inv_prop_v = 1.0f - prop_v;
	a_plus_1 = MIN2((buf_x-1),a+1);
	b_plus_1 = MIN2((buf_y-1),b+1);
	return (buf[(b * buf_y) + a] * inv_prop_u + buf[(b*buf_y)+(a_plus_1)] * prop_u)*inv_prop_v+(buf[((b_plus_1) * buf_y)+a] * inv_prop_u + buf[((b_plus_1)*buf_y)+(a_plus_1)] * prop_u) * prop_v;

}

static void set_pixel_intensity(float *buf, int buf_x, int buf_y, int pos_x, int pos_y, float intensity)
{
	if(pos_x < 0 || pos_x >= buf_x || pos_y < 0 || pos_y >= buf_y) {
		return;
	}
	buf[(pos_y * buf_y) + buf_x] = intensity;
}
#endif

int PLX_antialias_buffer(float *buf, int buf_x, int buf_y)
{
#ifdef __PLX__FAKE_AA__
#ifdef __PLX_GREY_AA__
	int i=0;
	int sz = buf_x * buf_y;
	for(i=0; i<sz; i++) {
		buf[i] *= 0.5f;
	}
#endif
	(void)buf_x;
	(void)buf_y;
	(void)buf;
	return 1;
#else
	/*XXX - TODO: THIS IS NOT FINAL CODE - IT DOES NOT WORK - DO NOT ENABLE IT */
	const float p0 = 1.0f;
	const float p1 = 1.0f;
	const float p2 = 1.0f;
	const float p3 = 1.0f;
	const float p4 = 1.0f;
	const float p5 = 1.5f;
	const float p6 = 2.0f;
	const float p7 = 2.0f;
	const float p8 = 2.0f;
	const float p9 = 2.0f;
	const float p10 = 4.0f;
	const float p11 = 8.0f;

	const float edge_threshold = 0.063f;
	const float edge_threshold_min = 0.0312f;
	const float quality_subpix = 1.0f;
//	int px_x;
//	int px_y;

	float posM_x,posM_y;
	float posB_x,posB_y;
	float posN_x,posN_y;
	float posP_x,posP_y;
	float offNP_x,offNP_y;
	float lumaM;
	float lumaS;
	float lumaE;
	float lumaN;
	float lumaW;
	float lumaNW;
	float lumaSE;
	float lumaNE;
	float lumaSW;
	float lumaNS;
	float lumaWE;
	float lumaNESE;
	float lumaNWNE;
	float lumaNWSW;
	float lumaSWSE;
	float lumaNN;
	float lumaSS;
	float lumaEndN;
	float lumaEndP;
	float lumaMM;
	float lumaMLTZero;
	float subpixNWSWNESE;
	float subpixRcpRange;
	float subpixNSWE;
	float maxSM;
	float minSM;
	float maxESM;
	float minESM;
	float maxWN;
	float minWN;
	float rangeMax;
	float rangeMin;
	float rangeMaxScaled;
	float range;
	float rangeMaxClamped;
	float edgeHorz;
	float edgeVert;
	float edgeHorz1;
	float edgeVert1;
	float edgeHorz2;
	float edgeVert2;
	float edgeHorz3;
	float edgeVert3;
	float edgeHorz4;
	float edgeVert4;
	float lengthSign;
	float subpixA;
	float subpixB;
	float subpixC;
	float subpixD;
	float subpixE;
	float subpixF;
	float subpixG;
	float subpixH;
	float gradientN;
	float gradientS;
	float gradient;
	float gradientScaled;
	float dstN;
	float dstP;
	float dst;
	float spanLength;
	float spanLengthRcp;
	float pixelOffset;
	float pixelOffsetGood;
	float pixelOffsetSubpix;
	int directionN;
	int goodSpan;
	int goodSpanN;
	int goodSpanP;
	int horzSpan;
	int earlyExit;
	int pairN;
	int doneN;
	int doneP;
	int doneNP;
	int curr_x=0;
	int curr_y=0;
	for(curr_y=0; curr_y < buf_y; curr_y++) {
		for(curr_x=0; curr_x < buf_x; curr_x++) {
			posM_x = ((float)(curr_x) + 0.5f) * (1.0f/(float)(buf_x));
			posM_y = ((float)(curr_y) + 0.5f) * (1.0f/(float)(buf_y));

			lumaM = get_pixel_intensity(buf, buf_x, buf_y, curr_x, curr_y);
			lumaS = get_pixel_intensity(buf, buf_x, buf_y, curr_x, curr_y - 1);
			lumaE = get_pixel_intensity(buf, buf_x, buf_y, curr_x + 1, curr_y);
			lumaN = get_pixel_intensity(buf, buf_x, buf_y, curr_x, curr_y + 1);
			lumaW = get_pixel_intensity(buf, buf_x, buf_y, curr_x - 1, curr_y);

			maxSM = MAX2(lumaS, lumaM);
			minSM = MIN2(lumaS, lumaM);
			maxESM = MAX2(lumaE, maxSM);
			minESM = MIN2(lumaE, minSM);
			maxWN = MAX2(lumaN, lumaW);
			minWN = MIN2(lumaN, lumaW);
			rangeMax = MAX2(maxWN, maxESM);
			rangeMin = MIN2(minWN, minESM);
			rangeMaxScaled = rangeMax * edge_threshold;
			range = rangeMax - rangeMin;
			rangeMaxClamped = MAX2(edge_threshold_min, rangeMaxScaled);

			earlyExit = range < rangeMaxClamped ? 1:0;
			if(earlyExit) {
				set_pixel_intensity(buf, buf_x, buf_y, curr_x, curr_y, lumaM);
			}

			lumaNW = get_pixel_intensity(buf, buf_x, buf_y, curr_x + 1, curr_y - 1);
			lumaSE = get_pixel_intensity(buf, buf_x, buf_y, curr_x - 1, curr_y + 1);
			lumaNE = get_pixel_intensity(buf, buf_x, buf_y, curr_x + 1, curr_y + 1);
			lumaSW = get_pixel_intensity(buf, buf_x, buf_y, curr_x - 1, curr_y - 1);

			lumaNS = lumaN + lumaS;
			lumaWE = lumaW + lumaE;
			subpixRcpRange = 1.0f/range;
			subpixNSWE = lumaNS + lumaWE;
			edgeHorz1 = (-2.0f * lumaM) + lumaNS;
			edgeVert1 = (-2.0f * lumaM) + lumaWE;

			lumaNESE = lumaNE + lumaSE;
			lumaNWNE = lumaNW + lumaNE;
			edgeHorz2 = (-2.0f * lumaE) + lumaNESE;
			edgeVert2 = (-2.0f * lumaN) + lumaNWNE;

			lumaNWSW = lumaNW + lumaSW;
			lumaSWSE = lumaSW + lumaSE;
			edgeHorz4 = (ABS(edgeHorz1) * 2.0f) + ABS(edgeHorz2);
			edgeVert4 = (ABS(edgeVert1) * 2.0f) + ABS(edgeVert2);
			edgeHorz3 = (-2.0f * lumaW) + lumaNWSW;
			edgeVert3 = (-2.0f * lumaS) + lumaSWSE;
			edgeHorz = ABS(edgeHorz3) + edgeHorz4;
			edgeVert = ABS(edgeVert3) + edgeVert4;

			subpixNWSWNESE = lumaNWSW + lumaNESE;
			lengthSign = 1.0f / (float)(buf_x);
			horzSpan = edgeHorz >= edgeVert ? 1:0;
			subpixA = subpixNSWE * 2.0f + subpixNWSWNESE;

			if(!horzSpan) {
				lumaN = lumaW;
				lumaS = lumaE;
			} else {
				lengthSign = 1.0f / (float)(buf_y);
			}
			subpixB = (subpixA * (1.0f/12.0f)) - lumaM;

			gradientN = lumaN - lumaM;
			gradientS = lumaS - lumaM;
			lumaNN = lumaN + lumaM;
			lumaSS = lumaS + lumaM;
			pairN = (ABS(gradientN)) >= (ABS(gradientS)) ? 1:0;
			gradient = MAX2(ABS(gradientN), ABS(gradientS));
			if(pairN) {
				lengthSign = -lengthSign;
			}
			subpixC = MAX2(MIN2(ABS(subpixB) * subpixRcpRange,1.0f),0.0f);

			posB_x = posM_x;
			posB_y = posM_y;
			offNP_x = (!horzSpan) ? 0.0f:(1.0f / (float)(buf_x));
			offNP_y = (horzSpan) ? 0.0f:(1.0f / (float)(buf_y));
			if(!horzSpan) {
				posB_x += lengthSign * 0.5f;
			} else {
				posB_y += lengthSign * 0.5f;
			}

			posN_x = posB_x - offNP_x * p0;
			posN_y = posB_y - offNP_y * p0;
			posP_x = posB_x + offNP_x * p0;
			posP_y = posB_y + offNP_y * p0;
			subpixD = ((-2.0f)*subpixC) + 3.0f;
			//may need bilinear filtered get_pixel_intensity() here...done
			lumaEndN = get_pixel_intensity_bilinear(buf, buf_x, buf_y,posN_x,posN_y);
			subpixE = subpixC * subpixC;
			//may need bilinear filtered get_pixel_intensity() here...done
			lumaEndP = get_pixel_intensity_bilinear(buf, buf_x, buf_y, posP_x,posP_y);

			if(!pairN) {
				lumaNN = lumaSS;
			}
			gradientScaled = gradient * 1.0f/4.0f;
			lumaMM =lumaM - lumaNN * 0.5f;
			subpixF = subpixD * subpixE;
			lumaMLTZero = lumaMM < 0.0f ? 1:0;

			lumaEndN -= lumaNN * 0.5f;
			lumaEndP -= lumaNN * 0.5f;
			doneN = (ABS(lumaEndN)) >= gradientScaled ? 1:0;
			doneP = (ABS(lumaEndP)) >= gradientScaled ? 1:0;
			if(!doneN) {
				posN_x -= offNP_x * p1;
				posN_y -= offNP_y * p1;
			}
			doneNP = (!doneN) || (!doneP) ? 1:0;
			if(!doneP) {
				posP_x += offNP_x * p1;
				posP_y += offNP_y * p1;
			}

			if(doneNP) {
				if(!doneN) {
					lumaEndN = get_pixel_intensity_bilinear(buf, buf_x, buf_y, posN_x,posN_y);
				}
				if(!doneP) {
					lumaEndP = get_pixel_intensity_bilinear(buf, buf_x, buf_y, posP_x, posP_y);
				}
				if(!doneN) {
					lumaEndN = lumaEndN - lumaNN * 0.5;
				}
				if(!doneP) {
					lumaEndP = lumaEndP - lumaNN * 0.5;
				}
				doneN = (ABS(lumaEndN)) >= gradientScaled ? 1:0;
				doneP = (ABS(lumaEndP)) >= gradientScaled ? 1:0;
				if(!doneN) {
					posN_x -= offNP_x * p2;
					posN_y -= offNP_y * p2;
				}
				doneNP = (!doneN) || (!doneP) ? 1:0;
				if(!doneP) {
					posP_x += offNP_x * p2;
					posP_y += offNP_y * p2;
				}
				if(doneNP) {
					if(!doneN) {
						lumaEndN = get_pixel_intensity_bilinear(buf, buf_x, buf_y,posN_x,posN_y);
					}
					if(!doneP) {
						lumaEndP = get_pixel_intensity_bilinear(buf, buf_x, buf_y, posP_x,posP_y);
					}
					if(!doneN) {
						lumaEndN = lumaEndN - lumaNN * 0.5;
					}
					if(!doneP) {
						lumaEndP = lumaEndP - lumaNN * 0.5;
					}
					doneN = (ABS(lumaEndN)) >= gradientScaled ? 1:0;
					doneP = (ABS(lumaEndP)) >= gradientScaled ? 1:0;
					if(!doneN) {
						posN_x -= offNP_x * p3;
						posN_y -= offNP_y * p3;
					}
					doneNP = (!doneN) || (!doneP) ? 1:0;
					if(!doneP) {
						posP_x += offNP_x * p3;
						posP_y += offNP_y * p3;
					}
					if(doneNP) {
						if(!doneN) {
							lumaEndN = get_pixel_intensity_bilinear(buf, buf_x, buf_y,posN_x,posN_y);
						}
						if(!doneP) {
							lumaEndP = get_pixel_intensity_bilinear(buf, buf_x, buf_y, posP_x,posP_y);
						}
						if(!doneN) {
							lumaEndN = lumaEndN - lumaNN * 0.5;
						}
						if(!doneP) {
							lumaEndP = lumaEndP - lumaNN * 0.5;
						}
						doneN = (ABS(lumaEndN)) >= gradientScaled ? 1:0;
						doneP = (ABS(lumaEndP)) >= gradientScaled ? 1:0;
						if(!doneN) {
							posN_x -= offNP_x * p4;
							posN_y -= offNP_y * p4;
						}
						doneNP = (!doneN) || (!doneP) ? 1:0;
						if(!doneP) {
							posP_x += offNP_x * p4;
							posP_y += offNP_y * p4;
						}
						if(doneNP) {
							if(!doneN) {
								lumaEndN = get_pixel_intensity_bilinear(buf, buf_x, buf_y,posN_x,posN_y);
							}
							if(!doneP) {
								lumaEndP = get_pixel_intensity_bilinear(buf, buf_x, buf_y, posP_x,posP_y);
							}
							if(!doneN) {
								lumaEndN = lumaEndN - lumaNN * 0.5;
							}
							if(!doneP) {
								lumaEndP = lumaEndP - lumaNN * 0.5;
							}
							doneN = (ABS(lumaEndN)) >= gradientScaled ? 1:0;
							doneP = (ABS(lumaEndP)) >= gradientScaled ? 1:0;
							if(!doneN) {
								posN_x -= offNP_x * p5;
								posN_y -= offNP_y * p5;
							}
							doneNP = (!doneN) || (!doneP) ? 1:0;
							if(!doneP) {
								posP_x += offNP_x * p5;
								posP_y += offNP_y * p5;
							}
							if(doneNP) {
								if(!doneN) {
									lumaEndN = get_pixel_intensity_bilinear(buf, buf_x, buf_y,posN_x,posN_y);
								}
								if(!doneP) {
									lumaEndP = get_pixel_intensity_bilinear(buf, buf_x, buf_y, posP_x,posP_y);
								}
								if(!doneN) {
									lumaEndN = lumaEndN - lumaNN * 0.5;
								}
								if(!doneP) {
									lumaEndP = lumaEndP - lumaNN * 0.5;
								}
								doneN = (ABS(lumaEndN)) >= gradientScaled ? 1:0;
								doneP = (ABS(lumaEndP)) >= gradientScaled ? 1:0;
								if(!doneN) {
									posN_x -= offNP_x * p6;
									posN_y -= offNP_y * p6;
								}
								doneNP = (!doneN) || (!doneP) ? 1:0;
								if(!doneP) {
									posP_x += offNP_x * p6;
									posP_y += offNP_y * p6;
								}
								if(doneNP) {
									if(!doneN) {
										lumaEndN = get_pixel_intensity_bilinear(buf, buf_x, buf_y,posN_x,posN_y);
									}
									if(!doneP) {
										lumaEndP = get_pixel_intensity_bilinear(buf, buf_x, buf_y, posP_x,posP_y);
									}
									if(!doneN) {
										lumaEndN = lumaEndN - lumaNN * 0.5;
									}
									if(!doneP) {
										lumaEndP = lumaEndP - lumaNN * 0.5;
									}
									doneN = (ABS(lumaEndN)) >= gradientScaled ? 1:0;
									doneP = (ABS(lumaEndP)) >= gradientScaled ? 1:0;
									if(!doneN) {
										posN_x -= offNP_x * p7;
										posN_y -= offNP_y * p7;
									}
									doneNP = (!doneN) || (!doneP) ? 1:0;
									if(!doneP) {
										posP_x += offNP_x * p7;
										posP_y += offNP_y * p7;
									}
									if(doneNP) {
										if(!doneN) {
											lumaEndN = get_pixel_intensity_bilinear(buf, buf_x, buf_y,posN_x,posN_y);
										}
										if(!doneP) {
											lumaEndP = get_pixel_intensity_bilinear(buf, buf_x, buf_y, posP_x,posP_y);
										}
										if(!doneN) {
											lumaEndN = lumaEndN - lumaNN * 0.5;
										}
										if(!doneP) {
											lumaEndP = lumaEndP - lumaNN * 0.5;
										}
										doneN = (ABS(lumaEndN)) >= gradientScaled ? 1:0;
										doneP = (ABS(lumaEndP)) >= gradientScaled ? 1:0;
										if(!doneN) {
											posN_x -= offNP_x * p8;
											posN_y -= offNP_y * p8;
										}
										doneNP = (!doneN) || (!doneP) ? 1:0;
										if(!doneP) {
											posP_x += offNP_x * p8;
											posP_y += offNP_y * p8;
										}
										if(doneNP) {
											if(!doneN) {
												lumaEndN = get_pixel_intensity_bilinear(buf, buf_x, buf_y,posN_x,posN_y);
											}
											if(!doneP) {
												lumaEndP = get_pixel_intensity_bilinear(buf, buf_x, buf_y, posP_x,posP_y);
											}
											if(!doneN) {
												lumaEndN = lumaEndN - lumaNN * 0.5;
											}
											if(!doneP) {
												lumaEndP = lumaEndP - lumaNN * 0.5;
											}
											doneN = (ABS(lumaEndN)) >= gradientScaled ? 1:0;
											doneP = (ABS(lumaEndP)) >= gradientScaled ? 1:0;
											if(!doneN) {
												posN_x -= offNP_x * p9;
												posN_y -= offNP_y * p9;
											}
											doneNP = (!doneN) || (!doneP) ? 1:0;
											if(!doneP) {
												posP_x += offNP_x * p9;
												posP_y += offNP_y * p9;
											}
											if(doneNP) {
												if(!doneN) {
													lumaEndN = get_pixel_intensity_bilinear(buf, buf_x, buf_y,posN_x,posN_y);
												}
												if(!doneP) {
													lumaEndP = get_pixel_intensity_bilinear(buf, buf_x, buf_y, posP_x,posP_y);
												}
												if(!doneN) {
													lumaEndN = lumaEndN - lumaNN * 0.5;
												}
												if(!doneP) {
													lumaEndP = lumaEndP - lumaNN * 0.5;
												}
												doneN = (ABS(lumaEndN)) >= gradientScaled ? 1:0;
												doneP = (ABS(lumaEndP)) >= gradientScaled ? 1:0;
												if(!doneN) {
													posN_x -= offNP_x * p10;
													posN_y -= offNP_y * p10;
												}
												doneNP = (!doneN) || (!doneP) ? 1:0;
												if(!doneP) {
													posP_x += offNP_x * p10;
													posP_y += offNP_y * p10;
												}
												if(doneNP) {
													if(!doneN) {
														lumaEndN = get_pixel_intensity_bilinear(buf, buf_x, buf_y,posN_x,posN_y);
													}
													if(!doneP) {
														lumaEndP = get_pixel_intensity_bilinear(buf, buf_x, buf_y, posP_x,posP_y);
													}
													if(!doneN) {
														lumaEndN = lumaEndN - lumaNN * 0.5;
													}
													if(!doneP) {
														lumaEndP = lumaEndP - lumaNN * 0.5;
													}
													doneN = (ABS(lumaEndN)) >= gradientScaled ? 1:0;
													doneP = (ABS(lumaEndP)) >= gradientScaled ? 1:0;
													if(!doneN) {
														posN_x -= offNP_x * p11;
														posN_y -= offNP_y * p11;
													}
													doneNP = (!doneN) || (!doneP) ? 1:0;
													if(!doneP) {
														posP_x += offNP_x * p11;
														posP_y += offNP_y * p11;
													}
												}
											}
										}
									}
								}
							}
						}
					}
				}
			}
			dstN = posM_x - posN_x;
			dstP = posP_x - posM_x;
			if(!horzSpan) {
				dstN = posM_y - posN_y;
				dstP = posP_y - posM_y;
			}

			goodSpanN = ((lumaEndN < 0.0f) ? 1:0) != lumaMLTZero ? 1:0;
			spanLength = (dstP + dstN);
			goodSpanP = ((lumaEndP < 0.0f) ? 1:0) != lumaMLTZero ? 1:0;
			spanLengthRcp = 1.0f/spanLength;

			directionN = dstN < dstP ? 1:0;
			dst = MIN2(dstN, dstP);
			goodSpan = (directionN==1) ? goodSpanN:goodSpanP;
			subpixG = subpixF * subpixF;
			pixelOffset = (dst * (-spanLengthRcp)) + 0.5f;
			subpixH = subpixG * quality_subpix;

			pixelOffsetGood = (goodSpan==1) ? pixelOffset : 0.0f;
			pixelOffsetSubpix = MAX2(pixelOffsetGood, subpixH);
			if(!horzSpan) {
				posM_x += pixelOffsetSubpix * lengthSign;
			} else {
				posM_y += pixelOffsetSubpix * lengthSign;
			}
			//may need bilinear filtered get_pixel_intensity() here...
			set_pixel_intensity(buf,buf_x,buf_y,curr_x,curr_y,get_pixel_intensity_bilinear(buf, buf_x, buf_y, posM_x,posM_y)* lumaM);

		}
	}
	return 1;

#endif
}

