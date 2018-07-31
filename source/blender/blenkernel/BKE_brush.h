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

enum eCurveMappingPreset;
struct bContext;
struct Brush;
struct Paint;
struct ImBuf;
struct ImagePool;
struct Main;
struct Scene;
struct ToolSettings;
struct UnifiedPaintSettings;
// enum eCurveMappingPreset;

#include "DNA_object_enums.h"

/* globals for brush execution */
void BKE_brush_system_init(void);
void BKE_brush_system_exit(void);

/* datablock functions */
void BKE_brush_init(struct Brush *brush);
struct Brush *BKE_brush_add(struct Main *bmain, const char *name, const eObjectMode ob_mode);
struct Brush *BKE_brush_add_gpencil(struct Main *bmain, struct ToolSettings *ts, const char *name);
struct Brush *BKE_brush_first_search(struct Main *bmain, const eObjectMode ob_mode);
void BKE_brush_copy_data(struct Main *bmain, struct Brush *brush_dst, const struct Brush *brush_src, const int flag);
struct Brush *BKE_brush_copy(struct Main *bmain, const struct Brush *brush);
void BKE_brush_make_local(struct Main *bmain, struct Brush *brush, const bool lib_local);
void BKE_brush_free(struct Brush *brush);

void BKE_brush_sculpt_reset(struct Brush *brush);
void BKE_brush_gpencil_presets(struct bContext *C);
struct Brush *BKE_brush_getactive_gpencil(struct ToolSettings *ts);
struct Paint *BKE_brush_get_gpencil_paint(struct ToolSettings *ts);

/* image icon function */
struct ImBuf *get_brush_icon(struct Brush *brush);

/* jitter */
void BKE_brush_jitter_pos(
        const struct Scene *scene, struct Brush *brush,
        const float pos[2], float jitterpos[2]);
void BKE_brush_randomize_texture_coords(struct UnifiedPaintSettings *ups, bool mask);

/* brush curve */
void BKE_brush_curve_preset(struct Brush *b, enum eCurveMappingPreset preset);
float BKE_brush_curve_strength_clamped(struct Brush *br, float p, const float len);
float BKE_brush_curve_strength(const struct Brush *br, float p, const float len);

/* sampling */
float BKE_brush_sample_tex_3D(
        const struct Scene *scene, const struct Brush *br, const float point[3],
        float rgba[4], const int thread, struct ImagePool *pool);
float BKE_brush_sample_masktex(
        const struct Scene *scene, struct Brush *br, const float point[2],
        const int thread, struct ImagePool *pool);

/* texture */
unsigned int *BKE_brush_gen_texture_cache(struct Brush *br, int half_side, bool use_secondary);

/* radial control */
struct ImBuf *BKE_brush_gen_radial_control_imbuf(struct Brush *br, bool secondary);

/* unified strength size and color */

const float *BKE_brush_color_get(const struct Scene *scene, const struct Brush *brush);
const float *BKE_brush_secondary_color_get(const struct Scene *scene, const struct Brush *brush);
void BKE_brush_color_set(struct Scene *scene, struct Brush *brush, const float color[3]);

int  BKE_brush_size_get(const struct Scene *scene, const struct Brush *brush);
void BKE_brush_size_set(struct Scene *scene, struct Brush *brush, int value);

float BKE_brush_unprojected_radius_get(const struct Scene *scene, const struct Brush *brush);
void  BKE_brush_unprojected_radius_set(struct Scene *scene, struct Brush *brush, float value);

float BKE_brush_alpha_get(const struct Scene *scene, const struct Brush *brush);
void BKE_brush_alpha_set(struct Scene *scene, struct Brush *brush, float alpha);
float BKE_brush_weight_get(const struct Scene *scene, const struct Brush *brush);
void BKE_brush_weight_set(const struct Scene *scene, struct Brush *brush, float value);

bool BKE_brush_use_locked_size(const struct Scene *scene, const struct Brush *brush);
bool BKE_brush_use_alpha_pressure(const struct Scene *scene, const struct Brush *brush);
bool BKE_brush_use_size_pressure(const struct Scene *scene, const struct Brush *brush);

bool BKE_brush_sculpt_has_secondary_color(const struct Brush *brush);

/* scale unprojected radius to reflect a change in the brush's 2D size */
void BKE_brush_scale_unprojected_radius(
        float *unprojected_radius,
        int new_brush_size,
        int old_brush_size);

/* scale brush size to reflect a change in the brush's unprojected radius */
void BKE_brush_scale_size(
        int *r_brush_size,
        float new_unprojected_radius,
        float old_unprojected_radius);

/* debugging only */
void BKE_brush_debug_print_state(struct Brush *br);

#endif
