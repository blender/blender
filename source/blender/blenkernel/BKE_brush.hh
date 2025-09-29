/* SPDX-FileCopyrightText: 2023 Blender Authors
 *
 * SPDX-License-Identifier: GPL-2.0-or-later */

#pragma once

/** \file
 * \ingroup bke
 *
 * General operations for brushes.
 */

#include <optional>

#include "BLI_span.hh"

#include "DNA_brush_enums.h"
#include "DNA_color_types.h"
#include "DNA_object_enums.h"
#include "DNA_userdef_enums.h"

enum class PaintMode : int8_t;
struct Brush;
struct ImBuf;
struct ImagePool;
struct Main;
struct MTex;
struct Paint;
struct Scene;
struct UnifiedPaintSettings;

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
 * Delete a Brush.
 */
bool BKE_brush_delete(Main *bmain, Brush *brush);

/**
 * Perform deep-copy of a Brush and its 'children' data-blocks.
 *
 * \param dupflag: Controls which sub-data are also duplicated
 * (see #eDupli_ID_Flags in DNA_userdef_types.h).
 * \param duplicate_options: Additional context information about current duplicate call (e.g. if
 * it's part of a higher-level duplication or not, etc.). (see #eLibIDDuplicateFlags in
 * BKE_lib_id.hh).
 *
 * \warning By default, this functions will clear all \a bmain #ID.idnew pointers
 * (#BKE_main_id_newptr_and_tag_clear), and take care of post-duplication updates like remapping to
 * new IDs (#BKE_libblock_relink_to_newid).
 * If \a #LIB_ID_DUPLICATE_IS_SUBPROCESS duplicate option is passed on (typically when duplication
 * is called recursively from another parent duplication operation), the caller is responsible to
 * handle all of these operations.
 *
 * \note Caller MUST handle updates of the depsgraph (#DAG_relations_tag_update).
 */
Brush *BKE_brush_duplicate(Main *bmain,
                           Brush *brush,
                           eDupli_ID_Flags dupflag,
                           /*eLibIDDuplicateFlags*/ uint duplicate_options);
/**
 * Add grease pencil settings.
 */
void BKE_brush_init_gpencil_settings(Brush *brush);

void BKE_brush_init_curves_sculpt_settings(Brush *brush);

/**
 * Tag a linked brush as having changed settings so an indicator can be displayed to the user,
 * showing that the brush settings differ from the state of the imported brush asset. Call
 * every time a user visible change to the brush is done.
 *
 * Since this is meant to indicate brushes that are known to differ from the linked source file,
 * tagging is only performed for linked brushes. File local brushes are normal data-blocks that get
 * saved with the file, and don't need special attention by the user.
 *
 * For convenience, null may be passed for \a brush.
 */
void BKE_brush_tag_unsaved_changes(Brush *brush);

Brush *BKE_brush_first_search(Main *bmain, eObjectMode ob_mode);

void BKE_brush_jitter_pos(const Paint &paint,
                          const Brush &brush,
                          const float pos[2],
                          float jitterpos[2]);
void BKE_brush_randomize_texture_coords(Paint *paint, bool mask);

/* Brush curve. */

/**
 * Library Operations
 */
void BKE_brush_curve_preset(Brush *b, eCurveMappingPreset preset);

/**
 * Combine the brush strength based on the distances and brush settings with the existing factors.
 */
void BKE_brush_calc_curve_factors(eBrushCurvePreset preset,
                                  const CurveMapping *cumap,
                                  blender::Span<float> distances,
                                  float brush_radius,
                                  blender::MutableSpan<float> factors);
/**
 * Uses the brush curve control to find a strength value between 0 and 1.
 */
float BKE_brush_curve_strength_clamped(const Brush *br, float p, float len);
/**
 * Uses the brush curve control to find a strength value.
 */
float BKE_brush_curve_strength(eBrushCurvePreset preset,
                               const CurveMapping *cumap,
                               float distance,
                               float brush_radius);
float BKE_brush_curve_strength(const Brush *br, float p, float len);

/* Sampling. */

/**
 * Generic texture sampler for 3D painting systems.
 * point has to be either in region space mouse coordinates,
 * or 3d world coordinates for 3D mapping.
 *
 * RGBA outputs straight alpha.
 */
float BKE_brush_sample_tex_3d(const Paint *paint,
                              const Brush *br,
                              const MTex *mtex,
                              const float point[3],
                              float rgba[4],
                              int thread,
                              ImagePool *pool);
float BKE_brush_sample_masktex(
    const Paint *paint, Brush *br, const float point[2], int thread, ImagePool *pool);

/**
 * Get the mask texture for this given object mode.
 *
 * This is preferred above using mtex/mask_mtex attributes directly as due to legacy these
 * attributes got switched in sculpt mode.
 */
const MTex *BKE_brush_mask_texture_get(const Brush *brush, eObjectMode object_mode);

/**
 * Get the color texture for this given object mode.
 *
 * This is preferred above using mtex/mask_mtex attributes directly as due to legacy these
 * attributes got switched in sculpt mode.
 */
const MTex *BKE_brush_color_texture_get(const Brush *brush, eObjectMode object_mode);

/**
 * Radial control.
 */
ImBuf *BKE_brush_gen_radial_control_imbuf(Brush *br, bool secondary, bool display_gradient);

/* Unified strength size and color. */

struct BrushColorJitterSettings {
  int flag;
  /** Jitter amounts */
  float hue;
  float saturation;
  float value;

  /** Jitter pressure curves. */
  CurveMapping *curve_hue_jitter;
  CurveMapping *curve_sat_jitter;
  CurveMapping *curve_val_jitter;
};

const float *BKE_brush_color_get(const Paint *paint, const Brush *brush);
std::optional<BrushColorJitterSettings> BKE_brush_color_jitter_get_settings(const Paint *paint,
                                                                            const Brush *brush);
const float *BKE_brush_secondary_color_get(const Paint *paint, const Brush *brush);
void BKE_brush_color_set(Paint *paint, Brush *brush, const float color[3]);

void BKE_brush_color_sync_legacy(Brush *brush);
void BKE_brush_color_sync_legacy(UnifiedPaintSettings *ups);

int BKE_brush_size_get(const Paint *paint, const Brush *brush);
void BKE_brush_size_set(Paint *paint, Brush *brush, int size);
float BKE_brush_radius_get(const Paint *paint, const Brush *brush);

float BKE_brush_unprojected_size_get(const Paint *paint, const Brush *brush);
void BKE_brush_unprojected_size_set(Paint *paint, Brush *brush, float unprojected_size);
float BKE_brush_unprojected_radius_get(const Paint *paint, const Brush *brush);

float BKE_brush_alpha_get(const Paint *paint, const Brush *brush);
void BKE_brush_alpha_set(Paint *paint, Brush *brush, float alpha);
float BKE_brush_weight_get(const Paint *paint, const Brush *brush);
void BKE_brush_weight_set(Paint *paint, Brush *brush, float value);

int BKE_brush_input_samples_get(const Paint *paint, const Brush *brush);
void BKE_brush_input_samples_set(Paint *paint, Brush *brush, int value);

bool BKE_brush_use_locked_size(const Paint *paint, const Brush *brush);
bool BKE_brush_use_alpha_pressure(const Brush *brush);
bool BKE_brush_use_size_pressure(const Brush *brush);

/**
 * Scale unprojected radius to reflect a change in the brush's 2D size.
 */
void BKE_brush_scale_unprojected_size(float *unprojected_size,
                                      int new_brush_size,
                                      int old_brush_size);

/**
 * Scale brush size to reflect a change in the brush's unprojected radius.
 */
void BKE_brush_scale_size(int *r_brush_size,
                          float new_unprojected_size,
                          float old_unprojected_size);

/* Returns true if a brush requires a cube
 * (often presented to the user as a square) tip inside a specific paint mode.
 */
bool BKE_brush_has_cube_tip(const Brush *brush, PaintMode paint_mode);

/* debugging only */
void BKE_brush_debug_print_state(Brush *br);

/* -------------------------------------------------------------------- */
/** \name Brush Capabilities
 * Common boolean checks used during both brush evaluation and in UI drawing
 * via BrushCapabilities inside rna_brush.cc.
 * \{ */

namespace blender::bke::brush {
bool supports_dyntopo(const Brush &brush);
bool supports_accumulate(const Brush &brush);
bool supports_topology_rake(const Brush &brush);
bool supports_auto_smooth(const Brush &brush);
bool supports_height(const Brush &brush);
bool supports_plane_height(const Brush &brush);
bool supports_plane_depth(const Brush &brush);
bool supports_jitter(const Brush &brush);
bool supports_normal_weight(const Brush &brush);
bool supports_rake_factor(const Brush &brush);
bool supports_persistence(const Brush &brush);
bool supports_pinch_factor(const Brush &brush);
bool supports_plane_offset(const Brush &brush);
bool supports_random_texture_angle(const Brush &brush);
bool supports_sculpt_plane(const Brush &brush);
bool supports_color(const Brush &brush);
bool supports_secondary_cursor_color(const Brush &brush);
bool supports_smooth_stroke(const Brush &brush);
bool supports_space_attenuation(const Brush &brush);
bool supports_strength_pressure(const Brush &brush);
bool supports_size_pressure(const Brush &brush);
bool supports_auto_smooth_pressure(const Brush &brush);
bool supports_hardness_pressure(const Brush &brush);
bool supports_inverted_direction(const Brush &brush);
bool supports_gravity(const Brush &brush);
bool supports_tilt(const Brush &brush);
}  // namespace blender::bke::brush

/** \} */
