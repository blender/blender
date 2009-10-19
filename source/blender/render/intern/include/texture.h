/*
 * texture_ext.h
 *
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
 * The Original Code is Copyright (C) 2001-2002 by NaN Holding BV.
 * All rights reserved.
 *
 * The Original Code is: all of this file.
 *
 * Contributor(s): none yet.
 *
 * ***** END GPL LICENSE BLOCK *****
 */

#ifndef TEXTURE_EXT_H
#define TEXTURE_EXT_H

#define BRICONT		texres->tin= (texres->tin-0.5)*tex->contrast+tex->bright-0.5; \
if(texres->tin<0.0) texres->tin= 0.0; else if(texres->tin>1.0) texres->tin= 1.0;

#define BRICONTRGB	texres->tr= tex->rfac*((texres->tr-0.5)*tex->contrast+tex->bright-0.5); \
if(texres->tr<0.0) texres->tr= 0.0; \
texres->tg= tex->gfac*((texres->tg-0.5)*tex->contrast+tex->bright-0.5); \
if(texres->tg<0.0) texres->tg= 0.0; \
texres->tb= tex->bfac*((texres->tb-0.5)*tex->contrast+tex->bright-0.5); \
if(texres->tb<0.0) texres->tb= 0.0; 


struct HaloRen;
struct ShadeInput;
struct TexResult;
struct Tex;
struct Image;
struct ImBuf;

/* texture.h */

void do_halo_tex(struct HaloRen *har, float xn, float yn, float *colf);
void do_sky_tex(float *rco, float *lo, float *dxyview, float *hor, float *zen, float *blend, int skyflag, short thread);
void do_material_tex(struct ShadeInput *shi);
void do_lamp_tex(LampRen *la, float *lavec, struct ShadeInput *shi, float *colf, int effect);
void do_volume_tex(struct ShadeInput *shi, float *xyz, int mapto_flag, float *col, float *val);

void init_render_textures(Render *re);
void end_render_textures(void);

void render_realtime_texture(struct ShadeInput *shi, struct Image *ima);

/* imagetexture.h */

int imagewraposa(struct Tex *tex, struct Image *ima, struct ImBuf *ibuf, float *texvec, float *dxt, float *dyt, struct TexResult *texres);
int imagewrap(struct Tex *tex, struct Image *ima, struct ImBuf *ibuf, float *texvec, struct TexResult *texres);
void image_sample(struct Image *ima, float fx, float fy, float dx, float dy, float *result);

#endif /* TEXTURE_EXT_H */

