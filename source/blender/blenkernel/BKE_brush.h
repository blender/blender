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
 * ***** END GPL LICENSE BLOCK *****
 */

#ifndef __BKE_BRUSH_H__
#define __BKE_BRUSH_H__

/** \file BKE_brush.h
 *  \ingroup bke
 *
 * General operations for brushes.
 */

struct ID;
struct Brush;
struct ImBuf;
struct ImagePool;
struct Main;
struct rctf;
struct Scene;
struct wmOperator;
// enum CurveMappingPreset;


/* globals for brush execution */
void BKE_brush_system_init(void);
void BKE_brush_system_exit(void);

/* datablock functions */
struct Brush *BKE_brush_add(struct Main *bmain, const char *name);
struct Brush *BKE_brush_copy(struct Brush *brush);
void BKE_brush_make_local(struct Brush *brush);
void BKE_brush_free(struct Brush *brush);

void BKE_brush_sculpt_reset(struct Brush *brush);

/* image icon function */
struct ImBuf *get_brush_icon(struct Brush *brush);

/* brush library operations used by different paint panels */
int BKE_brush_texture_set_nr(struct Brush *brush, int nr);
int BKE_brush_texture_delete(struct Brush *brush);
int BKE_brush_clone_image_set_nr(struct Brush *brush, int nr);
int BKE_brush_clone_image_delete(struct Brush *brush);

/* jitter */
void BKE_brush_jitter_pos(const struct Scene *scene, struct Brush *brush,
                          const float pos[2], float jitterpos[2]);
void BKE_brush_randomize_texture_coordinates(struct UnifiedPaintSettings *ups, bool mask);

/* brush curve */
void BKE_brush_curve_preset(struct Brush *b, int preset);
float BKE_brush_curve_strength(struct Brush *br, float p, const float len);

/* sampling */
float BKE_brush_sample_tex_3D(const Scene *scene, struct Brush *br, const float point[3],
                              float rgba[4], const int thread, struct ImagePool *pool);
float BKE_brush_sample_masktex(const Scene *scene, struct Brush *br, const float point[2],
                               const int thread, struct ImagePool *pool);

/* texture */
unsigned int *BKE_brush_gen_texture_cache(struct Brush *br, int half_side, bool use_secondary);

/* radial control */
struct ImBuf *BKE_brush_gen_radial_control_imbuf(struct Brush *br, bool secondary);

/* unified strength size and color */

float *BKE_brush_color_get(const struct Scene *scene, struct Brush *brush);
float *BKE_brush_secondary_color_get(const struct Scene *scene, struct Brush *brush);
void BKE_brush_color_set(struct Scene *scene, struct Brush *brush, const float color[3]);

int  BKE_brush_size_get(const struct Scene *scene, struct Brush *brush);
void BKE_brush_size_set(struct Scene *scene, struct Brush *brush, int value);

float BKE_brush_unprojected_radius_get(const struct Scene *scene, struct Brush *brush);
void  BKE_brush_unprojected_radius_set(struct Scene *scene, struct Brush *brush, float value);

float BKE_brush_alpha_get(const struct Scene *scene, struct Brush *brush);
void BKE_brush_alpha_set(Scene *scene, struct Brush *brush, float alpha);
float BKE_brush_weight_get(const Scene *scene, struct Brush *brush);
void BKE_brush_weight_set(const Scene *scene, struct Brush *brush, float value);

int  BKE_brush_use_locked_size(const struct Scene *scene, struct Brush *brush);
int  BKE_brush_use_alpha_pressure(const struct Scene *scene, struct Brush *brush);
int  BKE_brush_use_size_pressure(const struct Scene *scene, struct Brush *brush);

/* scale unprojected radius to reflect a change in the brush's 2D size */
void BKE_brush_scale_unprojected_radius(float *unprojected_radius,
                                        int new_brush_size,
                                        int old_brush_size);

/* scale brush size to reflect a change in the brush's unprojected radius */
void BKE_brush_scale_size(int *BKE_brush_size_get,
                          float new_unprojected_radius,
                          float old_unprojected_radius);

/* debugging only */
void BKE_brush_debug_print_state(struct Brush *br);

#endif

