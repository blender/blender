/**
 * blenlib/BKE_texture.h (mar-2001 nzc)
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
#ifndef BKE_TEXTURE_H
#define BKE_TEXTURE_H

#ifdef HAVE_CONFIG_H
#include <config.h>
#endif

struct Tex;
struct MTex;
struct PluginTex;
struct LampRen;
struct ColorBand;
struct HaloRen;

/*  in ColorBand struct */
#define MAXCOLORBAND 16


void free_texture(struct Tex *t); 
int test_dlerr(const char *name,  const char *symbol);
void open_plugin_tex(struct PluginTex *pit);
struct PluginTex *add_plugin_tex(char *str);
void free_plugin_tex(struct PluginTex *pit);
struct ColorBand *add_colorband(void);
int do_colorband(struct ColorBand *coba);
void default_tex(struct Tex *tex);
struct Tex *add_texture(char *name);
void default_mtex(struct MTex *mtex);
struct MTex *add_mtex(void);
struct Tex *copy_texture(struct Tex *tex);
void make_local_texture(struct Tex *tex);
void autotexname(struct Tex *tex);
void init_render_texture(struct Tex *tex);
void init_render_textures(void);
void end_render_texture(struct Tex *tex);
void end_render_textures(void);
int clouds(struct Tex *tex, float *texvec);
int blend(struct Tex *tex, float *texvec);
int wood(struct Tex *tex, float *texvec);
int marble(struct Tex *tex, float *texvec);
int magic(struct Tex *tex, float *texvec);
int stucci(struct Tex *tex, float *texvec);
int texnoise(struct Tex *tex);
int plugintex(struct Tex *tex, float *texvec, float *dxt, float *dyt);
void tubemap(float x, float y, float z, float *adr1, float *adr2);
void spheremap(float x, float y, float z, float *adr1, float *adr2);
void do_2d_mapping(struct MTex *mtex, float *t, float *dxt, float *dyt);
int multitex(struct Tex *tex, float *texvec, float *dxt, float *dyt);
void do_material_tex(void);
void do_halo_tex(struct HaloRen *har, float xn, float yn, float *colf);
void do_sky_tex(void);
void do_lamp_tex(struct LampRen *la, float *lavec);
void externtex(struct MTex *mtex, float *vec);
void externtexcol(struct MTex *mtex, float *orco, char *col);
void render_realtime_texture(void);           

#endif

