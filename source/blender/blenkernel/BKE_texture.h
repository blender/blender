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
struct FreestyleLineStyle;
struct ImagePool;
struct Lamp;
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


void init_colorband(struct ColorBand *coba, bool rangetype);
struct ColorBand *add_colorband(bool rangetype);
bool do_colorband(const struct ColorBand *coba, float in, float out[4]);
void colorband_table_RGBA(struct ColorBand *coba, float **array, int *size);
struct CBData *colorband_element_add(struct ColorBand *coba, float position);
int colorband_element_remove(struct ColorBand *coba, int index);
void colorband_update_sort(struct ColorBand *coba);

void         BKE_texture_free(struct Tex *tex);
void         BKE_texture_default(struct Tex *tex);
struct Tex  *BKE_texture_copy(struct Main *bmain, const struct Tex *tex);
struct Tex  *BKE_texture_add(struct Main *bmain, const char *name);
struct Tex  *BKE_texture_localize(struct Tex *tex);
void         BKE_texture_make_local(struct Main *bmain, struct Tex *tex, const bool lib_local);
void         BKE_texture_type_set(struct Tex *tex, int type);

void         BKE_texture_mtex_default(struct MTex *mtex);
struct MTex *BKE_texture_mtex_add(void);
struct MTex *BKE_texture_mtex_add_id(struct ID *id, int slot);
/* UNUSED */
// void autotexname(struct Tex *tex);

struct Tex *give_current_object_texture(struct Object *ob);
struct Tex *give_current_material_texture(struct Material *ma);
struct Tex *give_current_lamp_texture(struct Lamp *la);
struct Tex *give_current_linestyle_texture(struct FreestyleLineStyle *linestyle);
struct Tex *give_current_world_texture(struct World *world);
struct Tex *give_current_brush_texture(struct Brush *br);
struct Tex *give_current_particle_texture(struct ParticleSettings *part);

struct bNode *give_current_material_texture_node(struct Material *ma);

bool give_active_mtex(struct ID *id, struct MTex ***mtex_ar, short *act);
void set_active_mtex(struct ID *id, short act);

void set_current_brush_texture(struct Brush *br, struct Tex *tex);
void set_current_world_texture(struct World *wo, struct Tex *tex);
void set_current_material_texture(struct Material *ma, struct Tex *tex);
void set_current_lamp_texture(struct Lamp *la, struct Tex *tex);
void set_current_linestyle_texture(struct FreestyleLineStyle *linestyle, struct Tex *tex);
void set_current_particle_texture(struct ParticleSettings *part, struct Tex *tex);

bool has_current_material_texture(struct Material *ma);

struct TexMapping *BKE_texture_mapping_add(int type);
void               BKE_texture_mapping_default(struct TexMapping *texmap, int type);
void               BKE_texture_mapping_init(struct TexMapping *texmap);

struct ColorMapping *BKE_texture_colormapping_add(void);
void                 BKE_texture_colormapping_default(struct ColorMapping *colormap);

void           BKE_texture_envmap_free_data(struct EnvMap *env);
void           BKE_texture_envmap_free(struct EnvMap *env);
struct EnvMap *BKE_texture_envmap_add(void);
struct EnvMap *BKE_texture_envmap_copy(const struct EnvMap *env);

void                 BKE_texture_pointdensity_init_data(struct PointDensity *pd);
void                 BKE_texture_pointdensity_free_data(struct PointDensity *pd);
void                 BKE_texture_pointdensity_free(struct PointDensity *pd);
struct PointDensity *BKE_texture_pointdensity_add(void);
struct PointDensity *BKE_texture_pointdensity_copy(const struct PointDensity *pd);

void              BKE_texture_voxeldata_free_data(struct VoxelData *vd);
void              BKE_texture_voxeldata_free(struct VoxelData *vd);
struct VoxelData *BKE_texture_voxeldata_add(void);
struct VoxelData *BKE_texture_voxeldata_copy(struct VoxelData *vd);

void             BKE_texture_ocean_free(struct OceanTex *ot);
struct OceanTex *BKE_texture_ocean_add(void);
struct OceanTex *BKE_texture_ocean_copy(const struct OceanTex *ot);

bool    BKE_texture_dependsOnTime(const struct Tex *texture);
bool    BKE_texture_is_image_user(const struct Tex *tex);

void BKE_texture_get_value_ex(
        const struct Scene *scene, struct Tex *texture,
        float *tex_co, struct TexResult *texres,
        struct ImagePool *pool,
        bool use_color_management);

void BKE_texture_get_value(
        const struct Scene *scene, struct Tex *texture,
        float *tex_co, struct TexResult *texres, bool use_color_management);

void BKE_texture_fetch_images_for_pool(struct Tex *texture, struct ImagePool *pool);

#ifdef __cplusplus
}
#endif

#endif

