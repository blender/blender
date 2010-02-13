/* 
 * $Id$
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
 * The Original Code is Copyright (C) 2008 Blender Foundation.
 * All rights reserved.
 *
 * The Original Code is: all of this file.
 *
 * Contributor(s): Brecht Van Lommel.
 *
 * ***** END GPL LICENSE BLOCK *****
 */

#ifndef OCCLUSION_H
#define OCCLUSION_H

struct Render;
struct ShadeInput;
struct ShadeResult;
struct RenderPart;
struct ShadeSample;
struct DerivedMesh;
struct ObjectRen;

void make_occ_tree(struct Render *re);
void free_occ(struct Render *re);
void sample_occ(struct Render *re, struct ShadeInput *shi);

void cache_occ_samples(struct Render *re, struct RenderPart *pa, struct ShadeSample *ssamp);
void free_occ_samples(struct Render *re, struct RenderPart *pa);

#endif

