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

#ifdef __cplusplus
extern "C" {
#endif

int PLX_raskterize(float (*base_verts)[2], int num_base_verts,
                   float *buf, int buf_x, int buf_y, int do_mask_AA);
int PLX_raskterize_feather(float (*base_verts)[2], int num_base_verts,
                           float (*feather_verts)[2], int num_feather_verts,
                           float *buf, int buf_x, int buf_y);
int PLX_antialias_buffer(float *buf, int buf_x, int buf_y);
#ifdef __cplusplus
}
#endif
