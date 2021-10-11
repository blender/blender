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
 */

#pragma once

/** \file
 * \ingroup bke
 *
 * General operations for brushes.
 */

#include "DNA_object_enums.h"

#ifdef __cplusplus
extern "C" {
#endif

enum eCurveMappingPreset;
struct Brush;
struct ImBuf;
struct ImagePool;
struct Main;
struct Scene;
struct ToolSettings;
struct UnifiedPaintSettings;
struct DynTopoSettings;
struct Sculpt;
struct CurveMapping;

// enum eCurveMappingPreset;

/* globals for brush execution */
void BKE_brush_system_init(void);
void BKE_brush_system_exit(void);

/* datablock functions */
struct Brush *BKE_brush_add(struct Main *bmain, const char *name, const eObjectMode ob_mode);
struct Brush *BKE_brush_add_gpencil(struct Main *bmain,
                                    struct ToolSettings *ts,
                                    const char *name,
                                    eObjectMode mode);
bool BKE_brush_delete(struct Main *bmain, struct Brush *brush);
void BKE_brush_init_gpencil_settings(struct Brush *brush);
struct Brush *BKE_brush_first_search(struct Main *bmain, const eObjectMode ob_mode);

void BKE_brush_sculpt_reset(struct Brush *brush);

void BKE_brush_gpencil_paint_presets(struct Main *bmain,
                                     struct ToolSettings *ts,
                                     const bool reset);
void BKE_brush_gpencil_vertex_presets(struct Main *bmain,
                                      struct ToolSettings *ts,
                                      const bool reset);
void BKE_brush_gpencil_sculpt_presets(struct Main *bmain,
                                      struct ToolSettings *ts,
                                      const bool reset);
void BKE_brush_gpencil_weight_presets(struct Main *bmain,
                                      struct ToolSettings *ts,
                                      const bool reset);
void BKE_gpencil_brush_preset_set(struct Main *bmain, struct Brush *brush, const short type);

/* jitter */
void BKE_brush_jitter_pos(const struct Scene *scene,
                          struct Brush *brush,
                          const float pos[2],
                          float jitterpos[2]);
void BKE_brush_randomize_texture_coords(struct UnifiedPaintSettings *ups, bool mask);

/* brush curve */
void BKE_brush_curve_preset(struct Brush *b, enum eCurveMappingPreset preset);
float BKE_brush_curve_strength_clamped(struct Brush *br, float p, const float len);
float BKE_brush_curve_strength(const struct Brush *br, float p, const float len);

/* sampling */
float BKE_brush_sample_tex_3d(const struct Scene *scene,
                              const struct Brush *br,
                              const float point[3],
                              float rgba[4],
                              const int thread,
                              struct ImagePool *pool);
float BKE_brush_sample_masktex(const struct Scene *scene,
                               struct Brush *br,
                               const float point[2],
                               const int thread,
                               struct ImagePool *pool);

/* texture */
unsigned int *BKE_brush_gen_texture_cache(struct Brush *br, int half_side, bool use_secondary);

/* radial control */
struct ImBuf *BKE_brush_gen_radial_control_imbuf(struct Brush *br,
                                                 bool secondary,
                                                 bool display_gradient);

/* unified strength size and color */

const float *BKE_brush_color_get(const struct Scene *scene, const struct Brush *brush);
const float *BKE_brush_secondary_color_get(const struct Scene *scene, const struct Brush *brush);
void BKE_brush_color_set(struct Scene *scene,
                         struct Brush *brush,
                         const float color[3],
                         const bool use_brush_channels);

int BKE_brush_size_get(const struct Scene *scene,
                       const struct Brush *brush,
                       bool use_brush_channel);
void BKE_brush_size_set(struct Scene *scene,
                        struct Brush *brush,
                        int size,
                        bool use_brush_channel);

float BKE_brush_unprojected_radius_get(const struct Scene *scene,
                                       const struct Brush *brush,
                                       bool use_channels);
void BKE_brush_unprojected_radius_set(struct Scene *scene,
                                      struct Brush *brush,
                                      float unprojected_radius,
                                      bool use_channels);

float BKE_brush_alpha_get(const struct Scene *scene, const struct Brush *brush);
void BKE_brush_alpha_set(struct Scene *scene, struct Brush *brush, float alpha);
float BKE_brush_weight_get(const struct Scene *scene, const struct Brush *brush);
void BKE_brush_weight_set(const struct Scene *scene, struct Brush *brush, float value);

bool BKE_brush_use_locked_size(const struct Scene *scene,
                               const struct Brush *brush,
                               bool use_channels);
bool BKE_brush_use_alpha_pressure(const struct Brush *brush);
bool BKE_brush_use_size_pressure(const struct Brush *brush);

bool BKE_brush_sculpt_has_secondary_color(const struct Brush *brush);

/* scale unprojected radius to reflect a change in the brush's 2D size */
void BKE_brush_scale_unprojected_radius(float *unprojected_radius,
                                        int new_brush_size,
                                        int old_brush_size);

/* scale brush size to reflect a change in the brush's unprojected radius */
void BKE_brush_scale_size(int *r_brush_size,
                          float new_unprojected_radius,
                          float old_unprojected_radius);

void BKE_brush_default_input_curves_set(struct Brush *brush);

/* Accessors */
#define BKE_brush_tool_get(brush, p) \
  (CHECK_TYPE_ANY(brush, struct Brush *, const struct Brush *), \
   *(const char *)POINTER_OFFSET(brush, (p)->runtime.tool_offset))
#define BKE_brush_tool_set(brush, p, tool) \
  { \
    CHECK_TYPE_ANY(brush, struct Brush *); \
    char _old = *(char *)POINTER_OFFSET(brush, (p)->runtime.tool_offset); \
    *(char *)POINTER_OFFSET(brush, (p)->runtime.tool_offset) = tool; \
    if ((p)->runtime.ob_mode == OB_MODE_SCULPT) { \
      BKE_brush_builtin_patch(brush, tool); \
      if (_old != tool) { \
        BKE_brush_sculpt_reset(brush, tool); \
      } \
    } \
  } \
  ((void)0)

/* debugging only */
void BKE_brush_debug_print_state(struct Brush *br);

void BKE_brush_get_dyntopo(struct Brush *brush, struct Sculpt *sd, struct DynTopoSettings *out);

bool BKE_brush_hard_edge_mode_get(const struct Scene *scene, const struct Brush *brush);
void BKE_brush_hard_edge_mode_set(struct Scene *scene, struct Brush *brush, bool val);

float BKE_brush_fset_slide_get(const struct Scene *scene, const struct Brush *brush);
float BKE_brush_curve_strength_ex(int curve_preset,
                                  const struct CurveMapping *curve,
                                  float p,
                                  const float len);

#ifdef __cplusplus
}
#endif
