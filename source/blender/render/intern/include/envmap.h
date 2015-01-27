/*
 * envmap_ext.h
 *
 *
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
 */

/** \file blender/render/intern/include/envmap.h
 *  \ingroup render
 */


#ifndef __ENVMAP_H__
#define __ENVMAP_H__ 

/**
 * Make environment maps for all objects in the scene that have an
 * environment map as texture. 
 * (initrender.c)
 */

struct Render;
struct TexResult;
struct ImagePool;

void make_envmaps(struct Render *re);
int envmaptex(struct Tex *tex, const float texvec[3], float dxt[3], float dyt[3], int osatex, struct TexResult *texres, struct ImagePool *pool, const bool skip_image_load);
void env_rotate_scene(struct Render *re, float mat[4][4], int do_rotate);

#endif /* __ENVMAP_H__ */

