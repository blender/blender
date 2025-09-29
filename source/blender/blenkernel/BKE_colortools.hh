/* SPDX-FileCopyrightText: 2006 Blender Authors
 *
 * SPDX-License-Identifier: GPL-2.0-or-later */
#pragma once

/** \file
 * \ingroup bke
 */
#include <cstdint>

struct BlendDataReader;
struct BlendWriter;
struct ColorManagedColorspaceSettings;
struct ColorManagedDisplaySettings;
struct ColorManagedViewSettings;
struct CurveMap;
struct CurveMapPoint;
struct CurveMapping;
struct Histogram;
struct ImBuf;
struct Scopes;
struct rctf;

void BKE_curvemapping_set_defaults(CurveMapping *cumap,
                                   int tot,
                                   float minx,
                                   float miny,
                                   float maxx,
                                   float maxy,
                                   short default_handle_type);
CurveMapping *BKE_curvemapping_add(int tot, float minx, float miny, float maxx, float maxy);
void BKE_curvemapping_free_data(CurveMapping *cumap);
void BKE_curvemapping_free(CurveMapping *cumap);
void BKE_curvemapping_copy_data(CurveMapping *target, const CurveMapping *cumap);
CurveMapping *BKE_curvemapping_copy(const CurveMapping *cumap);
void BKE_curvemapping_set_black_white_ex(const float black[3],
                                         const float white[3],
                                         float r_bwmul[3]);
void BKE_curvemapping_set_black_white(CurveMapping *cumap,
                                      const float black[3],
                                      const float white[3]);

enum class CurveMapSlopeType : int8_t {
  Negative = 0,
  Positive = 1,
  PositiveNegative = 2,
};

/**
 * Reset the view for current curve.
 */
void BKE_curvemapping_reset_view(CurveMapping *cumap);
void BKE_curvemap_reset(CurveMap *cuma, const rctf *clipr, int preset, CurveMapSlopeType slope);
/**
 * Removes with flag set.
 */
void BKE_curvemap_remove(CurveMap *cuma, short flag);
/**
 * Remove specified point.
 */
bool BKE_curvemap_remove_point(CurveMap *cuma, CurveMapPoint *point);
CurveMapPoint *BKE_curvemap_insert(CurveMap *cuma, float x, float y);
/**
 * \param type: #eBezTriple_Handle
 */
void BKE_curvemap_handle_set(CurveMap *cuma, int type);

/**
 * \note only does current curvemap!.
 */
void BKE_curvemapping_changed(CurveMapping *cumap, bool rem_doubles);
void BKE_curvemapping_changed_all(CurveMapping *cumap);

/**
 * Call before _all_ evaluation functions.
 */
void BKE_curvemapping_init(CurveMapping *cumap);

/**
 * Keep these `const CurveMap` - to help with thread safety.
 * \note Single curve, no table check.
 * \note Table should be verified.
 */
float BKE_curvemap_evaluateF(const CurveMapping *cumap, const CurveMap *cuma, float value);
/**
 * Single curve, with table check.
 * Works with curve 'cur'.
 */
float BKE_curvemapping_evaluateF(const CurveMapping *cumap, int cur, float value);
/**
 * Vector case.
 */
void BKE_curvemapping_evaluate3F(const CurveMapping *cumap, float vecout[3], const float vecin[3]);
/**
 * RGB case, no black/white points, no pre-multiply.
 */
void BKE_curvemapping_evaluateRGBF(const CurveMapping *cumap,
                                   float vecout[3],
                                   const float vecin[3]);
/**
 * Byte version of #BKE_curvemapping_evaluateRGBF.
 */
void BKE_curvemapping_evaluate_premulRGB(const CurveMapping *cumap,
                                         unsigned char vecout_byte[3],
                                         const unsigned char vecin_byte[3]);
/**
 * Same as #BKE_curvemapping_evaluate_premulRGBF
 * but black/bwmul are passed as args for the compositor
 * where they can change per pixel.
 *
 * Use in conjunction with #BKE_curvemapping_set_black_white_ex
 *
 * \param black: Use instead of cumap->black
 * \param bwmul: Use instead of cumap->bwmul
 */
void BKE_curvemapping_evaluate_premulRGBF_ex(const CurveMapping *cumap,
                                             float vecout[3],
                                             const float vecin[3],
                                             const float black[3],
                                             const float bwmul[3]);
/**
 * RGB with black/white points and pre-multiply. tables are checked.
 */
void BKE_curvemapping_evaluate_premulRGBF(const CurveMapping *cumap,
                                          float vecout[3],
                                          const float vecin[3]);
bool BKE_curvemapping_RGBA_does_something(const CurveMapping *cumap);
void BKE_curvemapping_table_F(const CurveMapping *cumap, float **array, int *size);
void BKE_curvemapping_table_RGBA(const CurveMapping *cumap, float **array, int *size);

/** Get the minimum x value of each curve map table. */
void BKE_curvemapping_get_range_minimums(const CurveMapping *curve_mapping, float minimums[4]);

/**
 * Get the reciprocal of the difference between the maximum and the minimum x value of each curve
 * map table. Evaluation parameters can be multiplied by this value to be normalized. If the
 * difference is zero, 1^8 is returned.
 */
void BKE_curvemapping_compute_range_dividers(const CurveMapping *curve_mapping, float dividers[4]);

/**
 * Compute the slopes at the start and end points of each curve map. The slopes are multiplied by
 * the range of the curve map to compensate for parameter normalization. If the slope is vertical,
 * 1^8 is returned.
 */
void BKE_curvemapping_compute_slopes(const CurveMapping *curve_mapping,
                                     float start_slopes[4],
                                     float end_slopes[4]);

/**
 * Check if the curve map at the index is identity, that is, does nothing.
 * A curve map is said to be identity if:
 * - The curve mapping uses extrapolation.
 * - Its range is 1.
 * - The slope at its start point is 1.
 * - The slope at its end point is 1.
 * - The number of points is 2.
 * - The start point is at (0, 0).
 * - The end point is at (1, 1).
 * Note that this could return false even if the curve map is identity, this happens in the case
 * when more than 2 points exist in the curve map but all points are collinear. */
bool BKE_curvemapping_is_map_identity(const CurveMapping *curve_mapping, int index);

/**
 * Call when you do images etc, needs restore too. also verifies tables.
 * non-const (these modify the curve).
 */
void BKE_curvemapping_premultiply(CurveMapping *cumap, bool restore);

void BKE_curvemapping_blend_write(BlendWriter *writer, const CurveMapping *cumap);
void BKE_curvemapping_curves_blend_write(BlendWriter *writer, const CurveMapping *cumap);

/**
 * \note `cumap` itself has been read already.
 */
void BKE_curvemapping_blend_read(BlendDataReader *reader, CurveMapping *cumap);

void BKE_histogram_update_sample_line(Histogram *hist,
                                      ImBuf *ibuf,
                                      const ColorManagedViewSettings *view_settings,
                                      const ColorManagedDisplaySettings *display_settings);
void BKE_scopes_update(Scopes *scopes,
                       ImBuf *ibuf,
                       const ColorManagedViewSettings *view_settings,
                       const ColorManagedDisplaySettings *display_settings);
void BKE_scopes_free(Scopes *scopes);
void BKE_scopes_new(Scopes *scopes);

void BKE_color_managed_display_settings_init(ColorManagedDisplaySettings *settings);
void BKE_color_managed_display_settings_copy(ColorManagedDisplaySettings *new_settings,
                                             const ColorManagedDisplaySettings *settings);

/**
 * Initialize view settings to the default.
 */
void BKE_color_managed_view_settings_init(ColorManagedViewSettings *view_settings,
                                          const ColorManagedDisplaySettings *display_settings,
                                          const char *view_transform);

void BKE_color_managed_view_settings_copy(ColorManagedViewSettings *new_settings,
                                          const ColorManagedViewSettings *settings);

/**
 * Copy view settings that are not related to the curve mapping. Keep the curve mapping unchanged
 * in the new_settings.
 */
void BKE_color_managed_view_settings_copy_keep_curve_mapping(
    ColorManagedViewSettings *new_settings, const ColorManagedViewSettings *settings);

void BKE_color_managed_view_settings_free(ColorManagedViewSettings *settings);

void BKE_color_managed_view_settings_blend_write(BlendWriter *writer,
                                                 const ColorManagedViewSettings *settings);
void BKE_color_managed_view_settings_blend_read_data(BlendDataReader *reader,
                                                     ColorManagedViewSettings *settings);

void BKE_color_managed_colorspace_settings_init(
    ColorManagedColorspaceSettings *colorspace_settings);
void BKE_color_managed_colorspace_settings_copy(
    ColorManagedColorspaceSettings *colorspace_settings,
    const ColorManagedColorspaceSettings *settings);
bool BKE_color_managed_colorspace_settings_equals(const ColorManagedColorspaceSettings *settings1,
                                                  const ColorManagedColorspaceSettings *settings2);
