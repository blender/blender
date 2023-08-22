/* SPDX-FileCopyrightText: 2023 Blender Authors
 *
 * SPDX-License-Identifier: GPL-2.0-or-later */

#pragma once

/** \file
 * \ingroup bke
 *
 * General operations for brushes.
 */

#include "DNA_color_types.h"
#include "DNA_object_enums.h"

#include "BKE_paint.hh" /* for ePaintMode */

struct Brush;
struct ImBuf;
struct ImagePool;
struct Main;
struct MTex;
struct Scene;
struct ToolSettings;
struct UnifiedPaintSettings;
struct DynTopoSettings;
struct Sculpt;
struct CurveMapping;

// enum eCurveMappingPreset;

/* Globals for brush execution. */

void BKE_brush_system_init();
void BKE_brush_system_exit();

/* Data-block functions. */

/**
 * \note Resulting brush will have two users: one as a fake user,
 * another is assumed to be used by the caller.
 */
Brush *BKE_brush_add(Main *bmain, const char *name, eObjectMode ob_mode);
/**
 * Add a new gp-brush.
 */
Brush *BKE_brush_add_gpencil(Main *bmain, ToolSettings *ts, const char *name, eObjectMode mode);
/**
 * Delete a Brush.
 */
bool BKE_brush_delete(Main *bmain, Brush *brush);
/**
 * Add grease pencil settings.
 */
void BKE_brush_init_gpencil_settings(Brush *brush);

void BKE_brush_init_curves_sculpt_settings(Brush *brush);

Brush *BKE_brush_first_search(Main *bmain, eObjectMode ob_mode);

void BKE_brush_sculpt_reset(Brush *brush);

/* Which dyntopo settings are inherited by this brush from scene
 * defaults.  In most cases this is everything except for the
 * local dyntopo disable flag.
 */
int BKE_brush_dyntopo_inherit_flags(Brush *brush);

/**
 * Create a set of grease pencil Drawing presets.
 */
void BKE_brush_gpencil_paint_presets(Main *bmain, ToolSettings *ts, bool reset);
/**
 * Create a set of grease pencil Vertex Paint presets.
 */
void BKE_brush_gpencil_vertex_presets(Main *bmain, ToolSettings *ts, bool reset);
/**
 * Create a set of grease pencil Sculpt Paint presets.
 */
void BKE_brush_gpencil_sculpt_presets(Main *bmain, ToolSettings *ts, bool reset);
/**
 * Create a set of grease pencil Weight Paint presets.
 */
void BKE_brush_gpencil_weight_presets(Main *bmain, ToolSettings *ts, bool reset);
void BKE_gpencil_brush_preset_set(Main *bmain, Brush *brush, short type);

void BKE_brush_jitter_pos(const Scene *scene,
                          Brush *brush,
                          const float pos[2],
                          float jitterpos[2]);
void BKE_brush_randomize_texture_coords(UnifiedPaintSettings *ups, bool mask);

/* Brush curve. */

/**
 * Library Operations
 */
void BKE_brush_curve_preset(Brush *b, enum eCurveMappingPreset preset);
/**
 * Uses the brush curve control to find a strength value between 0 and 1.
 */
float BKE_brush_curve_strength_clamped(const Brush *br, float p, float len);
/**
 * Uses the brush curve control to find a strength value.
 */
float BKE_brush_curve_strength(const Brush *br, float p, float len);

/* Sampling. */

/**
 * Generic texture sampler for 3D painting systems.
 * point has to be either in region space mouse coordinates,
 * or 3d world coordinates for 3D mapping.
 *
 * RGBA outputs straight alpha.
 */
float BKE_brush_sample_tex_3d(const Scene *scene,
                              const Brush *br,
                              const MTex *mtex,
                              const float point[3],
                              float rgba[4],
                              int thread,
                              ImagePool *pool);
float BKE_brush_sample_masktex(
    const Scene *scene, Brush *br, const float point[2], int thread, ImagePool *pool);

/**
 * Get the mask texture for this given object mode.
 *
 * This is preferred above using mtex/mask_mtex attributes directly as due to legacy these
 * attributes got switched in sculpt mode.
 */
const MTex *BKE_brush_mask_texture_get(const Brush *brush, const eObjectMode object_mode);

/**
 * Get the color texture for this given object mode.
 *
 * This is preferred above using mtex/mask_mtex attributes directly as due to legacy these
 * attributes got switched in sculpt mode.
 */
const MTex *BKE_brush_color_texture_get(const Brush *brush, const eObjectMode object_mode);

/**
 * Radial control.
 */
ImBuf *BKE_brush_gen_radial_control_imbuf(Brush *br, bool secondary, bool display_gradient);

/* Unified strength size and color. */

const float *BKE_brush_color_get(const Scene *scene, const Brush *brush);
const float *BKE_brush_secondary_color_get(const Scene *scene, const Brush *brush);
void BKE_brush_color_set(Scene *scene, Brush *brush, const float color[3]);

int BKE_brush_size_get(const Scene *scene, const Brush *brush);
void BKE_brush_size_set(Scene *scene, Brush *brush, int size);

float BKE_brush_unprojected_radius_get(const Scene *scene, const Brush *brush);
void BKE_brush_unprojected_radius_set(Scene *scene, Brush *brush, float unprojected_radius);

float BKE_brush_alpha_get(const Scene *scene, const Brush *brush);
void BKE_brush_alpha_set(Scene *scene, Brush *brush, float alpha);
float BKE_brush_weight_get(const Scene *scene, const Brush *brush);
void BKE_brush_weight_set(const Scene *scene, Brush *brush, float value);

bool BKE_brush_use_locked_size(const Scene *scene, const Brush *brush);
bool BKE_brush_use_alpha_pressure(const Brush *brush);
bool BKE_brush_use_size_pressure(const Brush *brush);
bool BKE_brush_sculpt_has_secondary_color(const Brush *brush);

/**
 * Scale unprojected radius to reflect a change in the brush's 2D size.
 */
void BKE_brush_scale_unprojected_radius(float *unprojected_radius,
                                        int new_brush_size,
                                        int old_brush_size);

/**
 * Scale brush size to reflect a change in the brush's unprojected radius.
 */
void BKE_brush_scale_size(int *r_brush_size,
                          float new_unprojected_radius,
                          float old_unprojected_radius);

void BKE_brush_default_input_curves_set(Brush *brush);

/* Returns true if a brush requires a cube
 * (often presented to the user as a square) tip inside a specific paint mode.
 */
bool BKE_brush_has_cube_tip(const Brush *brush, ePaintMode paint_mode);

/* Accessors */
#define BKE_brush_tool_get(brush, p) \
  (CHECK_TYPE_ANY(brush, Brush *, const Brush *), \
   *(const char *)POINTER_OFFSET(brush, (p)->runtime.tool_offset))
#define BKE_brush_tool_set(brush, p, tool) \
  { \
    CHECK_TYPE_ANY(brush, Brush *); \
    *(char *)POINTER_OFFSET(brush, (p)->runtime.tool_offset) = tool; \
  } \
  ((void)0)

/* debugging only */
void BKE_brush_debug_print_state(Brush *br);

void BKE_brush_get_dyntopo(Brush *brush, Sculpt *sd, DynTopoSettings *out);

bool BKE_brush_hard_edge_mode_get(const Scene *scene, const Brush *brush);
void BKE_brush_hard_edge_mode_set(Scene *scene, Brush *brush, bool val);
float BKE_brush_hard_corner_pin_get(const Scene *scene, const Brush *brush);

float BKE_brush_fset_slide_get(const Scene *scene, const Brush *brush);
float BKE_brush_curve_strength_ex(
    int curve_preset, const CurveMapping *curve, float p, const float len, const bool invert);
