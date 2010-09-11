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
 * Inc., 51 Franklin Street, Fifth Floor, Boston, MA 02110-1301, USA.
 *
 * The Original Code is Copyright (C) 2006 by Blender Foundation
 * All rights reserved.
 *
 * The Original Code is: all of this file.
 *
 * Contributor(s): none yet.
 *
 * ***** END GPL/BL DUAL LICENSE BLOCK *****
 */

#ifndef RE_RENDER_EXT_H
#define RE_RENDER_EXT_H

/* ~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~ */
/* this include is for non-render pipeline exports (still old cruft here) */
/* ~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~ */

/* called by meshtools */
struct View3D;
struct Scene;

void	RE_make_sticky(struct Scene *scene, struct View3D *v3d);
	
/* for radiosity module */	
struct RadView;
struct RNode;
struct Render;
struct MTex;
struct ImBuf;

// RADIO REMOVED, Maybe this will be useful later
//void    RE_zbufferall_radio(struct RadView *vw, struct RNode **rg_elem, int rg_totelem, struct Render *re);

/* particle.c, effect.c, editmesh_modes.c and brush.c, returns 1 if rgb, 0 otherwise */
int	externtex(struct MTex *mtex, float *vec, float *tin, float *tr, float *tg, float *tb, float *ta);

/* particle.c */
void texture_rgb_blend(float *in, float *tex, float *out, float fact, float facg, int blendtype);
float texture_value_blend(float tex, float out, float fact, float facg, int blendtype);

/* node_composite.c */
void ibuf_sample(struct ImBuf *ibuf, float fx, float fy, float dx, float dy, float *result);
void antialias_tagbuf(int xsize, int ysize, char *rectmove);

#endif /* RE_RENDER_EXT_H */

