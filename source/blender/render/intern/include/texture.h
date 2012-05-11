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
 * Contributor(s): none yet.
 *
 * ***** END GPL LICENSE BLOCK *****
 */

/** \file blender/render/intern/include/texture.h
 *  \ingroup render
 */


#ifndef __TEXTURE_H__
#define __TEXTURE_H__

#define BRICONT                                                               \
	texres->tin= (texres->tin-0.5f) * tex->contrast+tex->bright-0.5f;         \
	if(texres->tin < 0.0f)      texres->tin= 0.0f;                            \
	else if(texres->tin > 1.0f) texres->tin= 1.0f;                            \

#define BRICONTRGB                                                            \
	texres->tr= tex->rfac*((texres->tr-0.5f)*tex->contrast+tex->bright-0.5f); \
	if(texres->tr<0.0f) texres->tr= 0.0f;                                     \
	texres->tg= tex->gfac*((texres->tg-0.5f)*tex->contrast+tex->bright-0.5f); \
	if(texres->tg<0.0f) texres->tg= 0.0f;                                     \
	texres->tb= tex->bfac*((texres->tb-0.5f)*tex->contrast+tex->bright-0.5f); \
	if(texres->tb<0.0f) texres->tb= 0.0f;                                     \
	if(tex->saturation != 1.0f) {                                             \
		float _hsv[3];                                                        \
		rgb_to_hsv(texres->tr, texres->tg, texres->tb,                        \
		           _hsv, _hsv+1, _hsv+2);                                     \
		_hsv[1] *= tex->saturation;                                           \
		hsv_to_rgb(_hsv[0], _hsv[1], _hsv[2],                                 \
		           &texres->tr, &texres->tg, &texres->tb);                    \
	}                                                                         \

struct HaloRen;
struct ShadeInput;
struct TexResult;
struct Tex;
struct Image;
struct ImBuf;

/* texture.h */

void do_halo_tex(struct HaloRen *har, float xn, float yn, float col_r[4]);
void do_sky_tex(const float rco[3], float lo[3], const float dxyview[2], float hor[3], float zen[3], float *blend, int skyflag, short thread);
void do_material_tex(struct ShadeInput *shi, struct Render *re);
void do_lamp_tex(LampRen *la, const float lavec[3], struct ShadeInput *shi, float col_r[3], int effect);
void do_volume_tex(struct ShadeInput *shi, const float xyz[3], int mapto_flag, float col[3], float *val, struct Render *re);

void init_render_textures(Render *re);
void end_render_textures(Render *re);

void render_realtime_texture(struct ShadeInput *shi, struct Image *ima);

/* imagetexture.h */

int imagewraposa(struct Tex *tex, struct Image *ima, struct ImBuf *ibuf, const float texvec[3], const float dxt[3], const float dyt[3], struct TexResult *texres);
int imagewrap(struct Tex *tex, struct Image *ima, struct ImBuf *ibuf, const float texvec[3], struct TexResult *texres);
void image_sample(struct Image *ima, float fx, float fy, float dx, float dy, float *result);

#endif /* __TEXTURE_H__ */

