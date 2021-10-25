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
 * Contributor(s): Matt Ebb
 *
 * ***** END GPL LICENSE BLOCK *****
 */

/** \file blender/render/intern/include/pointdensity.h
 *  \ingroup render
 */


#ifndef __POINTDENSITY_H__
#define __POINTDENSITY_H__ 

/**
 * Make point density kd-trees for all point density textures in the scene
 */

struct PointDensity;
struct Render;
struct TexResult;

void free_pointdensity(struct PointDensity *pd);
void cache_pointdensity(struct Render *re, struct PointDensity *pd);
void make_pointdensities(struct Render *re);
void free_pointdensities(struct Render *re);
int pointdensitytex(struct Tex *tex, const float texvec[3], struct TexResult *texres);

#endif /* __POINTDENSITY_H__ */

