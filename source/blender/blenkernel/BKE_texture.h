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
#ifndef __BKE_TEXTURE_H__
#define __BKE_TEXTURE_H__

/** \file BKE_texture.h
 *  \ingroup bke
 *  \since March 2001
 *  \author nzc
 */

#ifdef __cplusplus
extern "C" {
#endif

struct bNode;
struct Brush;
struct ColorBand;
struct EnvMap;
struct HaloRen;
struct Lamp;
struct LampRen;
struct Main;
struct Material;
struct MTex;
struct OceanTex;
struct ParticleSettings;
struct PointDensity;
struct Tex;
struct TexMapping;
struct TexResult;
struct VoxelData;
struct World;

/*  in ColorBand struct */
#define MAXCOLORBAND 32


void BKE_texture_free(struct Tex *t); 

void init_colorband(struct ColorBand *coba, bool rangetype);
struct ColorBand *add_colorband(bool rangetype);
bool do_colorband(const struct ColorBand *coba, float in, float out[4]);
void colorband_table_RGBA(struct ColorBand *coba, float **array, int *size);
struct CBData *colorband_element_add(struct ColorBand *coba, float position);
int colorband_element_remove(struct ColorBand *coba, int index);
void colorband_update_sort(struct ColorBand *coba);

void default_tex(struct Tex *tex);
struct Tex *add_texture(struct Main *bmain, const char *name);
void tex_set_type(struct Tex *tex, int type);
void default_mtex(struct MTex *mtex);
struct MTex *add_mtex(void);
struct MTex *add_mtex_id(struct ID *id, int slot);
struct Tex *BKE_texture_copy(struct Tex *tex);
struct Tex *localize_texture(struct Tex *tex);
void BKE_texture_make_local(struct Tex *tex);
/* UNUSED */
// void autotexname(struct Tex *tex);

struct Tex *give_current_object_texture(struct Object *ob);
struct Tex *give_current_material_texture(struct Material *ma);
struct Tex *give_current_lamp_texture(struct Lamp *la);
struct Tex *give_current_world_texture(struct World *world);
struct Tex *give_current_brush_texture(struct Brush *br);
struct Tex *give_current_particle_texture(struct ParticleSettings *part);

struct bNode *give_current_material_texture_node(struct Material *ma);

int  give_active_mtex(struct ID *id, struct MTex ***mtex_ar, short *act);
void set_active_mtex(struct ID *id, short act);

void set_current_brush_texture(struct Brush *br, struct Tex *tex);
void set_current_world_texture(struct World *wo, struct Tex *tex);
void set_current_material_texture(struct Material *ma, struct Tex *tex);
void set_current_lamp_texture(struct Lamp *la, struct Tex *tex);
void set_current_particle_texture(struct ParticleSettings *part, struct Tex *tex);

bool has_current_material_texture(struct Material *ma);

struct TexMapping *add_tex_mapping(int type);
void default_tex_mapping(struct TexMapping *texmap, int type);
void init_tex_mapping(struct TexMapping *texmap);

struct ColorMapping *add_color_mapping(void);
void default_color_mapping(struct ColorMapping *colormap);

void    BKE_free_envmapdata(struct EnvMap *env);
void    BKE_free_envmap(struct EnvMap *env);
struct EnvMap *BKE_add_envmap(void);
struct EnvMap *BKE_copy_envmap(struct EnvMap *env);

void    BKE_free_pointdensitydata(struct PointDensity *pd);
void    BKE_free_pointdensity(struct PointDensity *pd);
struct PointDensity *BKE_add_pointdensity(void);
struct PointDensity *BKE_copy_pointdensity(struct PointDensity *pd);

void BKE_free_voxeldatadata(struct VoxelData *vd);
void BKE_free_voxeldata(struct VoxelData *vd);
struct VoxelData *BKE_add_voxeldata(void);
struct VoxelData *BKE_copy_voxeldata(struct VoxelData *vd);

void BKE_free_oceantex(struct OceanTex *ot);
struct OceanTex *BKE_add_oceantex(void);
struct OceanTex *BKE_copy_oceantex(struct OceanTex *ot);
	
bool    BKE_texture_dependsOnTime(const struct Tex *texture);

void BKE_texture_get_value(struct Scene *scene, struct Tex *texture, float *tex_co, struct TexResult *texres, bool use_color_management);

#ifdef __cplusplus
}
#endif

#endif

