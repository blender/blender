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
 * The Original Code is Copyright (C) 2007 Blender Foundation.
 * All rights reserved.
 *
 * The Original Code is: all of this file.
 *
 * Contributor(s): none yet.
 *
 * ***** END GPL LICENSE BLOCK *****
 */

/** \file blender/render/intern/include/sss.h
 *  \ingroup render
 */


#ifndef __SSS_H__
#define __SSS_H__

/* Generic multiple scattering API */

struct ScatterSettings;
typedef struct ScatterSettings ScatterSettings;

struct ScatterTree;
typedef struct ScatterTree ScatterTree;

ScatterSettings *scatter_settings_new(float refl, float radius, float ior,
                                      float reflfac, float frontweight, float backweight);
void scatter_settings_free(ScatterSettings *ss);

ScatterTree *scatter_tree_new(ScatterSettings *ss[3], float scale, float error,
                              float (*co)[3], float (*color)[3], float *area, int totpoint);
void scatter_tree_build(ScatterTree *tree);
void scatter_tree_sample(ScatterTree *tree, const float co[3], float color[3]);
void scatter_tree_free(ScatterTree *tree);

/* Internal renderer API */

struct Render;
struct Material;
struct VlakRen;

void make_sss_tree(struct Render *re);
void sss_add_points(Render *re, float (*co)[3], float (*color)[3], float *area, int totpoint);
void free_sss(struct Render *re);

int sample_sss(struct Render *re, struct Material *mat, const float co[3], float color[3]);
int sss_pass_done(struct Render *re, struct Material *mat);

#endif /*__SSS_H__*/

