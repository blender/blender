/**
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
 * Inc., 59 Temple Place - Suite 330, Boston, MA  02111-1307, USA.
 *
 * The Original Code is Copyright (C) 2006 Blender Foundation.
 * All rights reserved.
 *
 * The Original Code is: all of this file.
 *
 * Contributor(s): none yet.
 *
 * ***** END GPL LICENSE BLOCK *****
 */

#ifndef RENDERDATABASE_H
#define RENDERDATABASE_H

struct VlakRen;
struct VertRen;
struct HaloRen;
struct Material;
struct Render;

/* renderdatabase.c */
void free_renderdata_tables(struct Render *re);
void set_normalflags(Render *re);
void project_renderdata(struct Render *re, void (*projectfunc)(float *, float mat[][4], float *),  int do_pano, int part);

/* functions are not exported... so wrong names */

struct VlakRen *RE_findOrAddVlak(struct Render *re, int nr);
struct VertRen *RE_findOrAddVert(struct Render *re, int nr);
struct HaloRen *RE_findOrAddHalo(struct Render *re, int nr);
struct HaloRen *RE_inithalo(struct Render *re, struct Material *ma, float *vec, float *vec1, float *orco, float hasize, 
					 float vectsize, int seed);

float *RE_vertren_get_sticky(struct Render *re, struct VertRen *ver, int verify);
float *RE_vertren_get_stress(struct Render *re, struct VertRen *ver, int verify);
float *RE_vertren_get_rad(struct Render *re, struct VertRen *ver, int verify);
float *RE_vertren_get_strand(struct Render *re, struct VertRen *ver, int verify);
float *RE_vertren_get_tangent(struct Render *re, struct VertRen *ver, int verify);

/* haloren->type: flags */
#define HA_ONLYSKY		1
#define HA_VECT			2
#define HA_XALPHA		4
#define HA_FLARECIRC	8


void init_render_world(Render *re);


#endif /* RENDERDATABASE_H */

