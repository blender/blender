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
/** \file raskter.h
 *  \ingroup RASKTER
 */
/* from BLI_utildefines.h */
#define MIN2(x, y)               ( (x) < (y) ? (x) : (y) )
#define MAX2(x, y)               ( (x) > (y) ? (x) : (y) )
#define ABS(a)                   ( (a) < 0 ? (-(a)) : (a) )

struct poly_vert {
    int x;
    int y;
};

struct scan_line {
    int xstart;
    int xend;
};

struct scan_line_batch {
    int num;
    int ystart;
    struct scan_line *slines;
};

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
    int ymin;
    int ymax;
    int xmin;
    int xmax;
};

struct r_fill_context {
    struct e_status *all_edges, *possible_edges;
    struct r_buffer_stats rb;
    struct scan_line *bounds;
    void *kdo;  //only used with kd tree
    void *kdi;  //only used with kd tree
    int *bound_indexes;
    int bounds_length;
};

struct layer_init_data {
	struct poly_vert *imask;
	struct poly_vert *omask;
	struct scan_line *bounds;
	int *bound_indexes;
	int bounds_length;
};

#ifdef __cplusplus
extern "C" {
#endif

void preprocess_all_edges(struct r_fill_context *ctx, struct poly_vert *verts, int num_verts, struct e_status *open_edge);
int PLX_init_base_data(struct layer_init_data *mlayer_data, float(*base_verts)[2], int num_base_verts, 
							float *buf, int buf_x, int buf_y);
int PLX_raskterize(float (*base_verts)[2], int num_base_verts,
                   float *buf, int buf_x, int buf_y, int do_mask_AA);
int PLX_raskterize_feather(float (*base_verts)[2], int num_base_verts,
                           float (*feather_verts)[2], int num_feather_verts,
                           float *buf, int buf_x, int buf_y);
int PLX_antialias_buffer(float *buf, int buf_x, int buf_y);
#ifdef __cplusplus
}
#endif
