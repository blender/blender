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
 * General operations for brushes.
 */

#ifndef __BKE_BRUSH_H__
#define __BKE_BRUSH_H__

/** \file BKE_brush.h
 *  \ingroup bke
 */

struct ID;
struct Brush;
struct ImBuf;
struct Scene;
struct wmOperator;
// enum CurveMappingPreset;

/* datablock functions */
struct Brush *BKE_brush_add(const char *name);
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

/* brush curve */
void BKE_brush_curve_preset(struct Brush *b, /*enum CurveMappingPreset*/ int preset);
float BKE_brush_curve_strength_clamp(struct Brush *br, float p, const float len);
float BKE_brush_curve_strength(struct Brush *br, float p, const float len); /* used for sculpt */

/* sampling */
void BKE_brush_sample_tex(const struct Scene *scene, struct Brush *brush, const float xy[2], float rgba[4], const int thread);
void BKE_brush_imbuf_new(const struct Scene *scene, struct Brush *brush, short flt, short texfalloff, int size,
                         struct ImBuf **imbuf, int use_color_correction);

/* painting */
struct BrushPainter;
typedef struct BrushPainter BrushPainter;
typedef int (*BrushFunc)(void *user, struct ImBuf *ibuf, const float lastpos[2], const float pos[2]);

BrushPainter *BKE_brush_painter_new(struct Scene *scene, struct Brush *brush);
void BKE_brush_painter_require_imbuf(BrushPainter *painter, short flt,
                                     short texonly, int size);
int BKE_brush_painter_paint(BrushPainter *painter, BrushFunc func, const float pos[2],
                            double time, float pressure, void *user, int use_color_correction);
void BKE_brush_painter_break_stroke(BrushPainter *painter);
void BKE_brush_painter_free(BrushPainter *painter);

/* texture */
unsigned int *BKE_brush_gen_texture_cache(struct Brush *br, int half_side);

/* radial control */
struct ImBuf *BKE_brush_gen_radial_control_imbuf(struct Brush *br);

/* unified strength and size */

int  BKE_brush_size_get(const struct Scene *scene, struct Brush *brush);
void BKE_brush_size_set(struct Scene *scene, struct Brush *brush, int value);

float BKE_brush_unprojected_radius_get(const struct Scene *scene, struct Brush *brush);
void  BKE_brush_unprojected_radius_set(struct Scene *scene, struct Brush *brush, float value);

float BKE_brush_alpha_get(const struct Scene *scene, struct Brush *brush);
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

