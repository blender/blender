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
 * The Original Code is Copyright (C) 2010 Blender Foundation.
 * All rights reserved.
 *
 * The Original Code is: all of this file.
 *
 * Contributor(s): Morten Mikkelsen,
 *                 Sergey Sharybin
 *
 * ***** END GPL LICENSE BLOCK *****
 */

/** \file RE_multires_bake.h
 *  \ingroup render
 */

#ifndef __RE_MULTIRES_BAKE_H__
#define __RE_MULTIRES_BAKE_H__

struct MultiresBakeRender;

typedef struct MultiresBakeRender {
	DerivedMesh *lores_dm, *hires_dm;
	bool simple;
	int bake_filter;      /* Bake-filter, aka margin */
	int lvl, tot_lvl;
	short mode;
	bool use_lores_mesh;  /* Use low-resolution mesh when baking displacement maps */

	int number_of_rays;   /* Number of rays to be cast when doing AO baking */
	float bias;           /* Bias between object and start ray point when doing AO baking */

	int tot_obj, tot_image;
	ListBase image;

	int baked_objects, baked_faces;

	int raytrace_structure;    /* Optimization structure to be used for AO baking */
	int octree_resolution;     /* Reslution of octotree when using octotree optimization structure */
	int threads;               /* Number of threads to be used for baking */

	float user_scale;          /* User scale used to scale displacement when baking derivative map. */

	short *stop;
	short *do_update;
	float *progress;
} MultiresBakeRender;

void RE_multires_bake_images(struct MultiresBakeRender *bkr);

#endif
