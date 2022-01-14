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
 * The Original Code is Copyright (C) 2006 Blender Foundation.
 * All rights reserved.
 */
#pragma once

/** \file
 * \ingroup bke
 */

#ifdef __cplusplus
extern "C" {
#endif

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

void BKE_curvemapping_set_defaults(
    struct CurveMapping *cumap, int tot, float minx, float miny, float maxx, float maxy);
struct CurveMapping *BKE_curvemapping_add(int tot, float minx, float miny, float maxx, float maxy);
void BKE_curvemapping_free_data(struct CurveMapping *cumap);
void BKE_curvemapping_free(struct CurveMapping *cumap);
void BKE_curvemapping_copy_data(struct CurveMapping *target, const struct CurveMapping *cumap);
struct CurveMapping *BKE_curvemapping_copy(const struct CurveMapping *cumap);
void BKE_curvemapping_set_black_white_ex(const float black[3],
                                         const float white[3],
                                         float r_bwmul[3]);
void BKE_curvemapping_set_black_white(struct CurveMapping *cumap,
                                      const float black[3],
                                      const float white[3]);

enum {
  CURVEMAP_SLOPE_NEGATIVE = 0,
  CURVEMAP_SLOPE_POSITIVE = 1,
  CURVEMAP_SLOPE_POS_NEG = 2,
};

/**
 * Reset the view for current curve.
 */
void BKE_curvemapping_reset_view(struct CurveMapping *cumap);
void BKE_curvemap_reset(struct CurveMap *cuma, const struct rctf *clipr, int preset, int slope);
/**
 * Removes with flag set.
 */
void BKE_curvemap_remove(struct CurveMap *cuma, short flag);
/**
 * Remove specified point.
 */
bool BKE_curvemap_remove_point(struct CurveMap *cuma, struct CurveMapPoint *cmp);
struct CurveMapPoint *BKE_curvemap_insert(struct CurveMap *cuma, float x, float y);
/**
 * \param type: #eBezTriple_Handle
 */
void BKE_curvemap_handle_set(struct CurveMap *cuma, int type);

/**
 * \note only does current curvemap!.
 */
void BKE_curvemapping_changed(struct CurveMapping *cumap, bool rem_doubles);
void BKE_curvemapping_changed_all(struct CurveMapping *cumap);

/**
 * Call before _all_ evaluation functions.
 */
void BKE_curvemapping_init(struct CurveMapping *cumap);

/**
 * Keep these `const CurveMap` - to help with thread safety.
 * \note Single curve, no table check.
 * \note Table should be verified.
 */
float BKE_curvemap_evaluateF(const struct CurveMapping *cumap,
                             const struct CurveMap *cuma,
                             float value);
/**
 * Single curve, with table check.
 * Works with curve 'cur'.
 */
float BKE_curvemapping_evaluateF(const struct CurveMapping *cumap, int cur, float value);
/**
 * Vector case.
 */
void BKE_curvemapping_evaluate3F(const struct CurveMapping *cumap,
                                 float vecout[3],
                                 const float vecin[3]);
/**
 * RGB case, no black/white points, no pre-multiply.
 */
void BKE_curvemapping_evaluateRGBF(const struct CurveMapping *cumap,
                                   float vecout[3],
                                   const float vecin[3]);
/**
 * Byte version of #BKE_curvemapping_evaluateRGBF.
 */
void BKE_curvemapping_evaluate_premulRGB(const struct CurveMapping *cumap,
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
void BKE_curvemapping_evaluate_premulRGBF_ex(const struct CurveMapping *cumap,
                                             float vecout[3],
                                             const float vecin[3],
                                             const float black[3],
                                             const float bwmul[3]);
/**
 * RGB with black/white points and pre-multiply. tables are checked.
 */
void BKE_curvemapping_evaluate_premulRGBF(const struct CurveMapping *cumap,
                                          float vecout[3],
                                          const float vecin[3]);
bool BKE_curvemapping_RGBA_does_something(const struct CurveMapping *cumap);
void BKE_curvemapping_table_F(const struct CurveMapping *cumap, float **array, int *size);
void BKE_curvemapping_table_RGBA(const struct CurveMapping *cumap, float **array, int *size);

/**
 * Call when you do images etc, needs restore too. also verifies tables.
 * non-const (these modify the curve).
 */
void BKE_curvemapping_premultiply(struct CurveMapping *cumap, bool restore);

void BKE_curvemapping_blend_write(struct BlendWriter *writer, const struct CurveMapping *cumap);
void BKE_curvemapping_curves_blend_write(struct BlendWriter *writer,
                                         const struct CurveMapping *cumap);
/**
 * \note `cumap` itself has been read already.
 */
void BKE_curvemapping_blend_read(struct BlendDataReader *reader, struct CurveMapping *cumap);

void BKE_histogram_update_sample_line(struct Histogram *hist,
                                      struct ImBuf *ibuf,
                                      const struct ColorManagedViewSettings *view_settings,
                                      const struct ColorManagedDisplaySettings *display_settings);
void BKE_scopes_update(struct Scopes *scopes,
                       struct ImBuf *ibuf,
                       const struct ColorManagedViewSettings *view_settings,
                       const struct ColorManagedDisplaySettings *display_settings);
void BKE_scopes_free(struct Scopes *scopes);
void BKE_scopes_new(struct Scopes *scopes);

void BKE_color_managed_display_settings_init(struct ColorManagedDisplaySettings *settings);
void BKE_color_managed_display_settings_copy(struct ColorManagedDisplaySettings *new_settings,
                                             const struct ColorManagedDisplaySettings *settings);

/**
 * Initialize view settings to be best suitable for render type of viewing.
 * This will use default view transform from the OCIO configuration if none
 * is specified.
 */
void BKE_color_managed_view_settings_init_render(
    struct ColorManagedViewSettings *settings,
    const struct ColorManagedDisplaySettings *display_settings,
    const char *view_transform);

/**
 * Initialize view settings which are best suitable for viewing non-render images.
 * For example,s movie clips while tracking.
 */
void BKE_color_managed_view_settings_init_default(
    struct ColorManagedViewSettings *settings,
    const struct ColorManagedDisplaySettings *display_settings);

void BKE_color_managed_view_settings_copy(struct ColorManagedViewSettings *new_settings,
                                          const struct ColorManagedViewSettings *settings);
void BKE_color_managed_view_settings_free(struct ColorManagedViewSettings *settings);

void BKE_color_managed_view_settings_blend_write(struct BlendWriter *writer,
                                                 struct ColorManagedViewSettings *settings);
void BKE_color_managed_view_settings_blend_read_data(struct BlendDataReader *reader,
                                                     struct ColorManagedViewSettings *settings);

void BKE_color_managed_colorspace_settings_init(
    struct ColorManagedColorspaceSettings *colorspace_settings);
void BKE_color_managed_colorspace_settings_copy(
    struct ColorManagedColorspaceSettings *colorspace_settings,
    const struct ColorManagedColorspaceSettings *settings);
bool BKE_color_managed_colorspace_settings_equals(
    const struct ColorManagedColorspaceSettings *settings1,
    const struct ColorManagedColorspaceSettings *settings2);

#ifdef __cplusplus
}
#endif
