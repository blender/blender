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
	int simple, lvl, tot_lvl, bake_filter;
	short mode, use_lores_mesh;

	int tot_obj, tot_image;
	ListBase image;

	int baked_objects, baked_faces;

	short *stop;
	short *do_update;
	float *progress;
} MultiresBakeRender;

void RE_multires_bake_images(struct MultiresBakeRender *bkr);

#endif
