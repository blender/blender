/*
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
 */
/** \file
 * \ingroup render
 */

#pragma once

/* ~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~ */
/* this include is for non-render pipeline exports (still old cruft here) */
/* ~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~ */

/* called by meshtools */
struct Depsgraph;
struct ImagePool;
struct MTex;

#ifdef __cplusplus
extern "C" {
#endif

/* texture_procedural.c */
bool RE_texture_evaluate(const struct MTex *mtex,
                         const float vec[3],
                         const int thread,
                         struct ImagePool *pool,
                         const bool skip_load_image,
                         const bool texnode_preview,
                         /* Return arguments. */
                         float *r_intensity,
                         float r_rgba[4]) ATTR_NONNULL(1, 2, 7, 8);

void texture_rgb_blend(
    float in[3], const float tex[3], const float out[3], float fact, float facg, int blendtype);
float texture_value_blend(float tex, float out, float fact, float facg, int blendtype);

void RE_texture_rng_init(void);
void RE_texture_rng_exit(void);

/* texture_image.c */
void ibuf_sample(struct ImBuf *ibuf, float fx, float fy, float dx, float dy, float result[4]);

/* texture_pointdensity.c */
struct PointDensity;

void RE_point_density_cache(struct Depsgraph *depsgraph, struct PointDensity *pd);

void RE_point_density_minmax(struct Depsgraph *depsgraph,
                             struct PointDensity *pd,
                             float r_min[3],
                             float r_max[3]);

void RE_point_density_sample(struct Depsgraph *depsgraph,
                             struct PointDensity *pd,
                             const int resolution,
                             float *values);

void RE_point_density_free(struct PointDensity *pd);

void RE_point_density_fix_linking(void);

#ifdef __cplusplus
}
#endif
