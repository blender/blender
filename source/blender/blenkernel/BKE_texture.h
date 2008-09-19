/**
 * blenlib/BKE_texture.h (mar-2001 nzc)
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
#ifndef BKE_TEXTURE_H
#define BKE_TEXTURE_H

struct Tex;
struct MTex;
struct PluginTex;
struct LampRen;
struct ColorBand;
struct HaloRen;
struct TexMapping;
struct EnvMap;

/*  in ColorBand struct */
#define MAXCOLORBAND 32


void free_texture(struct Tex *t); 
int test_dlerr(const char *name,  const char *symbol);
void open_plugin_tex(struct PluginTex *pit);
struct PluginTex *add_plugin_tex(char *str);
void free_plugin_tex(struct PluginTex *pit);

void init_colorband(struct ColorBand *coba, int rangetype);
struct ColorBand *add_colorband(int rangetype);
int do_colorband(struct ColorBand *coba, float in, float out[4]);

void default_tex(struct Tex *tex);
struct Tex *add_texture(char *name);
void default_mtex(struct MTex *mtex);
struct MTex *add_mtex(void);
struct Tex *copy_texture(struct Tex *tex);
void make_local_texture(struct Tex *tex);
void autotexname(struct Tex *tex);
struct Tex *give_current_texture(struct Object *ob, int act);
struct Tex *give_current_world_texture(void);

struct TexMapping *add_mapping(void);
void init_mapping(struct TexMapping *texmap);


void    BKE_free_envmapdata(struct EnvMap *env);
void    BKE_free_envmap(struct EnvMap *env);
struct EnvMap *BKE_add_envmap(void);
struct EnvMap *BKE_copy_envmap(struct EnvMap *env);

int     BKE_texture_dependsOnTime(const struct Tex *texture);

#endif

