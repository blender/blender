/* SPDX-FileCopyrightText: 2001-2002 NaN Holding BV. All rights reserved.
 *
 * SPDX-License-Identifier: GPL-2.0-or-later */
#pragma once

/** \file
 * \ingroup bke
 */

#ifdef __cplusplus
extern "C" {
#endif

struct Brush;
struct ColorBand;
struct FreestyleLineStyle;
struct ImagePool;
struct LibraryForeachIDData;
struct MTex;
struct Main;
struct ParticleSettings;
struct PointDensity;
struct Tex;
struct TexMapping;
struct TexResult;

/** #ColorBand.data length. */
#define MAXCOLORBAND 32

/**
 * Utility for all IDs using those texture slots.
 */
void BKE_texture_mtex_foreach_id(struct LibraryForeachIDData *data, struct MTex *mtex);

void BKE_texture_default(struct Tex *tex);
struct Tex *BKE_texture_add(struct Main *bmain, const char *name);
void BKE_texture_type_set(struct Tex *tex, int type);

void BKE_texture_mtex_default(struct MTex *mtex);
struct MTex *BKE_texture_mtex_add(void);
/**
 * Slot -1 for first free ID.
 */
struct MTex *BKE_texture_mtex_add_id(struct ID *id, int slot);
/* UNUSED */
// void autotexname(struct Tex *tex);

struct Tex *give_current_linestyle_texture(struct FreestyleLineStyle *linestyle);
struct Tex *give_current_brush_texture(struct Brush *br);
struct Tex *give_current_particle_texture(struct ParticleSettings *part);

bool give_active_mtex(struct ID *id, struct MTex ***mtex_ar, short *act);
void set_active_mtex(struct ID *id, short act);

void set_current_brush_texture(struct Brush *br, struct Tex *tex);
void set_current_linestyle_texture(struct FreestyleLineStyle *linestyle, struct Tex *tex);
void set_current_particle_texture(struct ParticleSettings *part, struct Tex *tex);

struct TexMapping *BKE_texture_mapping_add(int type);
void BKE_texture_mapping_default(struct TexMapping *texmap, int type);
void BKE_texture_mapping_init(struct TexMapping *texmap);

struct ColorMapping *BKE_texture_colormapping_add(void);
void BKE_texture_colormapping_default(struct ColorMapping *colormap);

void BKE_texture_pointdensity_init_data(struct PointDensity *pd);
void BKE_texture_pointdensity_free_data(struct PointDensity *pd);
void BKE_texture_pointdensity_free(struct PointDensity *pd);
struct PointDensity *BKE_texture_pointdensity_add(void);
struct PointDensity *BKE_texture_pointdensity_copy(const struct PointDensity *pd, int flag);

bool BKE_texture_dependsOnTime(const struct Tex *texture);
/**
 * \returns true if this texture can use its #Texture.ima (even if its NULL).
 */
bool BKE_texture_is_image_user(const struct Tex *tex);

void BKE_texture_get_value_ex(struct Tex *texture,
                              const float *tex_co,
                              struct TexResult *texres,
                              struct ImagePool *pool,
                              bool use_color_management);

void BKE_texture_get_value(struct Tex *texture,
                           const float *tex_co,
                           struct TexResult *texres,
                           bool use_color_management);

/**
 * Make sure all images used by texture are loaded into pool.
 */
void BKE_texture_fetch_images_for_pool(struct Tex *texture, struct ImagePool *pool);

#ifdef __cplusplus
}
#endif
