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
/** \file raskter_mt.c
 *  \ingroup RASKTER
 */
#include <stdlib.h>

#include "raskter.h"
static int rast_scan_init(struct r_fill_context *ctx, struct poly_vert *verts, int num_verts) {
    int x_curr;                 /* current pixel position in X */
    int y_curr;                 /* current scan line being drawn */
    int yp;                     /* y-pixel's position in frame buffer */
    int swixd = 0;              /* whether or not edges switched position in X */
    int i=0;                    /* counter */
    float *cpxl;                /* pixel pointers... */
    float *mpxl;
    float *spxl;
    struct e_status *e_curr;    /* edge pointers... */
    struct e_status *e_temp;
    struct e_status *edgbuf;
    struct e_status **edgec;


    if(num_verts < 3) {
        return(1);
    }

    if((edgbuf = (struct e_status *)(malloc(sizeof(struct e_status) * num_verts))) == NULL) {
        return(0);
    }

    /* set initial bounds length to 0 */
    ctx->bounds_length=0;

    /* round 1, count up all the possible spans in the base buffer */
    preprocess_all_edges(ctx, verts, num_verts, edgbuf);

    /* can happen with a zero area mask */
    if (ctx->all_edges == NULL) {
        free(edgbuf);
        return(1);
    }
    ctx->possible_edges = NULL;

    for(y_curr = ctx->all_edges->ybeg; (ctx->all_edges || ctx->possible_edges); y_curr++) {

        for(edgec = &ctx->possible_edges; ctx->all_edges && (ctx->all_edges->ybeg == y_curr);) {
            x_curr = ctx->all_edges->x;                  /* Set current X position. */
            for(;;) {                                    /* Start looping edges. Will break when edges run out. */
                e_curr = *edgec;                         /* Set up a current edge pointer. */
                if(!e_curr || (e_curr->x >= x_curr)) {   /* If we have an no edge, or we need to skip some X-span, */
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

        yp = y_curr * ctx->rb.sizex;
        spxl = ctx->rb.buf + (yp);

        for(e_curr = ctx->possible_edges; e_curr; e_curr = e_curr->e_next) {

            /* set up xmin and xmax bounds on this scan line */
            cpxl = spxl + MAX2(e_curr->x, 0);
            e_curr = e_curr->e_next;
            mpxl = spxl + MIN2(e_curr->x, ctx->rb.sizex) - 1;

            if((y_curr >= 0) && (y_curr < ctx->rb.sizey)) {
                ctx->bounds_length++;
            }
        }

        for(edgec = &ctx->possible_edges; (e_curr = *edgec);) {
            if(!(--(e_curr->num))) {
                *edgec = e_curr->e_next;
            } else {
                e_curr->x += e_curr->xshift;
                if((e_curr->drift += e_curr->drift_inc) > 0) {
                    e_curr->x += e_curr->xdir;
                    e_curr->drift -= e_curr->drift_dec;
                }
                edgec = &e_curr->e_next;
            }
        }
        if(ctx->possible_edges) {
            for(edgec = &ctx->possible_edges; (e_curr = *edgec)->e_next; edgec = &(*edgec)->e_next) {
                /* if the current edge hits scan line at greater X than the next edge, we need to exchange the edges */
                if(e_curr->x > e_curr->e_next->x) {
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
            for(;;) {
                /* reset exchange flag so it's only set if we encounter another one */
                swixd = 0;
                for(edgec = &ctx->possible_edges; (e_curr = *edgec)->e_next; edgec = &(*edgec)->e_next) {
                    /* again, if current edge hits scan line at higher X than next edge, exchange the edges and set flag */
                    if(e_curr->x > e_curr->e_next->x) {
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
                if(!swixd) {
                    break;
                }
            }
        }
    }


/*initialize index buffer and bounds buffers*/
    //gets the +1 for dummy at the end
    if((ctx->bound_indexes = (int *)(malloc(sizeof(int) * ctx->rb.sizey+1)))==NULL) {
        return(0);
    }
    //gets the +1 for dummy at the start
    if((ctx->bounds = (struct scan_line *)(malloc(sizeof(struct scan_line) * ctx->bounds_length+1)))==NULL){
        return(0);
    }
    //init all the indexes to zero (are they already zeroed from malloc???)
    for(i=0;i<ctx->rb.sizey+1;i++){
        ctx->bound_indexes[i]=0;
    }
    /* round 2, fill in the full list of bounds, and create indexes to the list... */
    preprocess_all_edges(ctx, verts, num_verts, edgbuf);

    /* can happen with a zero area mask */
    if (ctx->all_edges == NULL) {
        free(edgbuf);
        return(1);
    }
    ctx->possible_edges = NULL;

    /* restart i as a counter for total span placement in buffer */
    i=1;
    for(y_curr = ctx->all_edges->ybeg; (ctx->all_edges || ctx->possible_edges); y_curr++) {

        for(edgec = &ctx->possible_edges; ctx->all_edges && (ctx->all_edges->ybeg == y_curr);) {
            x_curr = ctx->all_edges->x;                  /* Set current X position. */
            for(;;) {                                    /* Start looping edges. Will break when edges run out. */
                e_curr = *edgec;                         /* Set up a current edge pointer. */
                if(!e_curr || (e_curr->x >= x_curr)) {   /* If we have an no edge, or we need to skip some X-span, */
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

        yp = y_curr * ctx->rb.sizex;
        spxl = ctx->rb.buf + (yp);
        if((y_curr >=0) && (y_curr < ctx->rb.sizey)){
            ctx->bound_indexes[y_curr]=i;
        }
        for(e_curr = ctx->possible_edges; e_curr; e_curr = e_curr->e_next) {

            /* set up xmin and xmax bounds on this scan line */
            cpxl = spxl + MAX2(e_curr->x, 0);
            e_curr = e_curr->e_next;
            mpxl = spxl + MIN2(e_curr->x, ctx->rb.sizex) - 1;

            if((y_curr >= 0) && (y_curr < ctx->rb.sizey)) {
                ctx->bounds[i].xstart=cpxl-spxl;
                ctx->bounds[i].xend=mpxl-spxl;
                i++;
            }
        }

        for(edgec = &ctx->possible_edges; (e_curr = *edgec);) {
            if(!(--(e_curr->num))) {
                *edgec = e_curr->e_next;
            } else {
                e_curr->x += e_curr->xshift;
                if((e_curr->drift += e_curr->drift_inc) > 0) {
                    e_curr->x += e_curr->xdir;
                    e_curr->drift -= e_curr->drift_dec;
                }
                edgec = &e_curr->e_next;
            }
        }
        if(ctx->possible_edges) {
            for(edgec = &ctx->possible_edges; (e_curr = *edgec)->e_next; edgec = &(*edgec)->e_next) {
                /* if the current edge hits scan line at greater X than the next edge, we need to exchange the edges */
                if(e_curr->x > e_curr->e_next->x) {
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
            for(;;) {
                /* reset exchange flag so it's only set if we encounter another one */
                swixd = 0;
                for(edgec = &ctx->possible_edges; (e_curr = *edgec)->e_next; edgec = &(*edgec)->e_next) {
                    /* again, if current edge hits scan line at higher X than next edge, exchange the edges and set flag */
                    if(e_curr->x > e_curr->e_next->x) {
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
                if(!swixd) {
                    break;
                }
            }
        }
    }

    free(edgbuf);
    return 1;
}

static void init_base_data(float(*base_verts)[2], int num_base_verts,
                   float *buf, int buf_x, int buf_y) {
    int i;                                   /* i: Loop counter. */
    struct poly_vert *ply;                   /* ply: Pointer to a list of integer buffer-space vertex coordinates. */
    struct r_fill_context ctx = {0};
    const float buf_x_f = (float)(buf_x);
    const float buf_y_f = (float)(buf_y);
    if((ply = (struct poly_vert *)(malloc(sizeof(struct poly_vert) * num_base_verts))) == NULL) {
        return(0);
    }
    ctx.rb.buf = buf;                            /* Set the output buffer pointer. */
    ctx.rb.sizex = buf_x;                        /* Set the output buffer size in X. (width) */
    ctx.rb.sizey = buf_y;                        /* Set the output buffer size in Y. (height) */
    for(i = 0; i < num_base_verts; i++) {                           /* Loop over all base_verts. */
        ply[i].x = (int)((base_verts[i][0] * buf_x_f) + 0.5f);       /* Range expand normalized X to integer buffer-space X. */
        ply[i].y = (int)((base_verts[i][1] * buf_y_f) + 0.5f); /* Range expand normalized Y to integer buffer-space Y. */
    }
    i = rast_scan_init(&ctx, ply, num_base_verts);  /* Call our rasterizer, passing in the integer coords for each vert. */
    free(ply);                                      /* Free the memory allocated for the integer coordinate table. */
    return(i);                                      /* Return the value returned by the rasterizer. */
}

