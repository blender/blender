/*
 * texture_ext.h
 *
 * $Id$
 *
 * ***** BEGIN GPL/BL DUAL LICENSE BLOCK *****
 *
 * This program is free software; you can redistribute it and/or
 * modify it under the terms of the GNU General Public License
 * as published by the Free Software Foundation; either version 2
 * of the License, or (at your option) any later version. The Blender
 * Foundation also sells licenses for use in proprietary software under
 * the Blender License.  See http://www.blender.org/BL/ for information
 * about this.
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
 * The Original Code is Copyright (C) 2001-2002 by NaN Holding BV.
 * All rights reserved.
 *
 * The Original Code is: all of this file.
 *
 * Contributor(s): none yet.
 *
 * ***** END GPL/BL DUAL LICENSE BLOCK *****
 */

#ifndef TEXTURE_EXT_H
#define TEXTURE_EXT_H "$Id$"
#define TEXTURE_EXT_H "Copyright (C) 2001 NaN Technologies B.V.

struct Tex;
struct MTex;
struct HaloRen;
struct LampRen;
/**
 * Takes uv coordinates (R.uv[], O.dxuv, O.dyuv), find texture colour
 * at that spot (using imagewrap()). 
 * Result is kept in R.vcol (float vector 3)
 */
void render_realtime_texture(void);

/**
 * Do texture mapping for materials. Communicates with R.... variables.
 */
void do_material_tex(void);

/* unsorted */
int blend(struct Tex *tex, float *texvec);
int clouds(struct Tex *tex, float *texvec);
int cubemap(struct MTex *mtex, float x, float y, float z, float *adr1, float *adr2);
int cubemap_glob(struct MTex *mtex, float x, float y, float z, float *adr1, float *adr2);
int cubemap_ob(struct MTex *mtex, float x, float y, float z, float *adr1, float *adr2);
void do_2d_mapping(struct MTex *mtex, float *t, float *dxt, float *dyt);
void do_halo_tex(struct HaloRen *har, float xn, float yn, float *colf);
void do_lamp_tex(struct LampRen *la, float *lavec);
void do_sky_tex(void);
int magic(struct Tex *tex, float *texvec);
int marble(struct Tex *tex, float *texvec);
int multitex(struct Tex *tex, float *texvec, float *dxt, float *dyt);
int plugintex(struct Tex *tex, float *texvec, float *dxt, float *dyt);
int stucci(struct Tex *tex, float *texvec);
int texnoise(struct Tex *tex);
int wood(struct Tex *tex, float *texvec);

#endif /* TEXTURE_EXT_H */

