/* SPDX-FileCopyrightText: 2012 Blender Authors
 *
 * SPDX-License-Identifier: GPL-2.0-or-later */

#pragma once

/** \file
 * \ingroup bke
 */

#include "DNA_mask_types.h"

#ifndef SELECT
#  define SELECT 1
#endif

struct BezTriple;
struct Depsgraph;
struct Image;
struct ImageUser;
struct ListBase;
struct Main;
struct MovieClip;
struct MovieClipUser;

/* `mask_ops.cc` */

enum eMaskWhichHandle {
  MASK_WHICH_HANDLE_NONE = 0,
  MASK_WHICH_HANDLE_STICK = 1,
  MASK_WHICH_HANDLE_LEFT = 2,
  MASK_WHICH_HANDLE_RIGHT = 3,
  MASK_WHICH_HANDLE_BOTH = 4,
};

enum eMaskhandleMode {
  MASK_HANDLE_MODE_STICK = 1,
  MASK_HANDLE_MODE_INDIVIDUAL_HANDLES = 2,
};

/* -------------------------------------------------------------------- */
/** \name Mask Layers
 * \{ */

MaskLayer *BKE_mask_layer_new(Mask *mask, const char *name);
/**
 * \note The returned mask-layer may be hidden, caller needs to check.
 */
MaskLayer *BKE_mask_layer_active(Mask *mask);
void BKE_mask_layer_active_set(Mask *mask, MaskLayer *masklay);
void BKE_mask_layer_remove(Mask *mask, MaskLayer *masklay);

/** \brief Free all animation keys for a mask layer. */
void BKE_mask_layer_free_shapes(MaskLayer *masklay);
void BKE_mask_layer_free(MaskLayer *masklay);
void BKE_mask_layer_free_list(ListBase *masklayers);
void BKE_mask_spline_free(MaskSpline *spline);
void BKE_mask_spline_free_list(ListBase *splines);
MaskSpline *BKE_mask_spline_copy(const MaskSpline *spline);
void BKE_mask_point_free(MaskSplinePoint *point);

void BKE_mask_layer_unique_name(Mask *mask, MaskLayer *masklay);
void BKE_mask_layer_rename(Mask *mask,
                           MaskLayer *masklay,
                           const char *oldname,
                           const char *newname);

MaskLayer *BKE_mask_layer_copy(const MaskLayer *masklay);
void BKE_mask_layer_copy_list(ListBase *masklayers_new, const ListBase *masklayers);

struct MaskLayerShapeElem {
  float point[3][2];
  float weight;
  float radius;
};
/* MaskLayerShapeElem is serialized as 8 floats in DNA data. */
static_assert(sizeof(MaskLayerShapeElem) == 8 * sizeof(float),
              "MaskLayerShapeElem expected size mismatch");

/** \} */

/* -------------------------------------------------------------------- */
/** \name Splines
 * \{ */

MaskSplinePoint *BKE_mask_spline_point_array(MaskSpline *spline);
MaskSplinePoint *BKE_mask_spline_point_array_from_point(MaskSpline *spline,
                                                        const MaskSplinePoint *point_ref);

MaskSpline *BKE_mask_spline_add(MaskLayer *masklay);
bool BKE_mask_spline_remove(MaskLayer *mask_layer, MaskSpline *spline);
void BKE_mask_point_direction_switch(MaskSplinePoint *point);
void BKE_mask_spline_direction_switch(MaskLayer *masklay, MaskSpline *spline);

BezTriple *BKE_mask_spline_point_next_bezt(MaskSpline *spline,
                                           MaskSplinePoint *points_array,
                                           MaskSplinePoint *point);

enum eMaskSign {
  MASK_PROJ_NEG = -1,
  MASK_PROJ_ANY = 0,
  MASK_PROJ_POS = 1,
};
float BKE_mask_spline_project_co(
    MaskSpline *spline, MaskSplinePoint *point, float start_u, const float co[2], eMaskSign sign);

/** \} */

/* -------------------------------------------------------------------- */
/** \name Point
 * \{ */

eMaskhandleMode BKE_mask_point_handles_mode_get(const MaskSplinePoint *point);
void BKE_mask_point_handle(const MaskSplinePoint *point,
                           eMaskWhichHandle which_handle,
                           float r_handle[2]);
void BKE_mask_point_set_handle(MaskSplinePoint *point,
                               eMaskWhichHandle which_handle,
                               float loc[2],
                               bool keep_direction,
                               float orig_handle[2],
                               float orig_vec[3][3]);

void BKE_mask_point_segment_co(MaskSpline *spline, MaskSplinePoint *point, float u, float co[2]);
void BKE_mask_point_normal(MaskSpline *spline, MaskSplinePoint *point, float u, float n[2]);
float BKE_mask_point_weight_scalar(MaskSpline *spline, MaskSplinePoint *point, float u);
float BKE_mask_point_weight(MaskSpline *spline, MaskSplinePoint *point, float u);
MaskSplinePointUW *BKE_mask_point_sort_uw(MaskSplinePoint *point, MaskSplinePointUW *uw);
void BKE_mask_point_add_uw(MaskSplinePoint *point, float u, float w);

void BKE_mask_point_select_set(MaskSplinePoint *point, bool do_select);
void BKE_mask_point_select_set_handle(MaskSplinePoint *point,
                                      eMaskWhichHandle which_handle,
                                      bool do_select);

inline bool BKE_mask_point_selected(const MaskSplinePoint *p)
{
  return ((p->bezt.f1 | p->bezt.f2 | p->bezt.f3) & SELECT) != 0;
}

inline bool BKE_mask_point_selected_knot(const MaskSplinePoint *p)
{
  return (p->bezt.f2 & SELECT) != 0;
}

inline bool BKE_mask_point_is_handle_selected(const MaskSplinePoint *point,
                                              eMaskWhichHandle handle)
{
  return (handle == MASK_WHICH_HANDLE_STICK  ? (point->bezt.f1 | point->bezt.f3) & SELECT :
          (handle == MASK_WHICH_HANDLE_LEFT) ? (point->bezt.f1 & SELECT) :
                                               (point->bezt.f3 & SELECT)) != 0;
}

inline void BKE_mask_point_select_handles(MaskSplinePoint *p)
{
  p->bezt.f1 |= SELECT;
  p->bezt.f2 |= SELECT;
  p->bezt.f3 |= SELECT;
}

inline void BKE_mask_point_deselect_handles(MaskSplinePoint *p)
{
  p->bezt.f1 &= ~SELECT;
  p->bezt.f2 &= ~SELECT;
  p->bezt.f3 &= ~SELECT;
}

/** \} */

/* -------------------------------------------------------------------- */
/** \name General
 * \{ */

Mask *BKE_mask_new(Main *bmain, const char *name);

void BKE_mask_coord_from_frame(float r_co[2], const float co[2], const float frame_size[2]);
void BKE_mask_coord_from_movieclip(MovieClip *clip,
                                   MovieClipUser *user,
                                   float r_co[2],
                                   const float co[2]);
void BKE_mask_coord_from_image(Image *image, ImageUser *iuser, float r_co[2], const float co[2]);
/**
 * Inverse of #BKE_mask_coord_from_image.
 */
void BKE_mask_coord_to_frame(float r_co[2], const float co[2], const float frame_size[2]);
void BKE_mask_coord_to_movieclip(MovieClip *clip,
                                 MovieClipUser *user,
                                 float r_co[2],
                                 const float co[2]);
void BKE_mask_coord_to_image(Image *image, ImageUser *iuser, float r_co[2], const float co[2]);

/** \} */

/* -------------------------------------------------------------------- */
/** \name Parenting
 * \{ */

void BKE_mask_evaluate(Mask *mask, float ctime, bool do_newframe);
void BKE_mask_layer_evaluate(MaskLayer *masklay, float ctime, bool do_newframe);
void BKE_mask_parent_init(MaskParent *parent);
void BKE_mask_calc_handle_adjacent_interp(MaskSpline *spline, MaskSplinePoint *point, float u);
/**
 * Calculates the tangent of a point by its previous and next
 * (ignoring handles - as if its a poly line).
 */
void BKE_mask_calc_tangent_polyline(MaskSpline *spline, MaskSplinePoint *point, float t[2]);
void BKE_mask_calc_handle_point(MaskSpline *spline, MaskSplinePoint *point);
/**
 * \brief Resets auto handles even for non-auto bezier points
 *
 * Useful for giving sane defaults.
 */
void BKE_mask_calc_handle_point_auto(MaskSpline *spline,
                                     MaskSplinePoint *point,
                                     bool do_recalc_length);
void BKE_mask_get_handle_point_adjacent(MaskSpline *spline,
                                        MaskSplinePoint *point,
                                        MaskSplinePoint **r_point_prev,
                                        MaskSplinePoint **r_point_next);
void BKE_mask_layer_calc_handles(MaskLayer *masklay);
void BKE_mask_spline_ensure_deform(MaskSpline *spline);
void BKE_mask_point_parent_matrix_get(MaskSplinePoint *point,
                                      float ctime,
                                      float parent_matrix[3][3]);

/** \} */

/* -------------------------------------------------------------------- */
/** \name Animation
 * \{ */

int BKE_mask_layer_shape_totvert(MaskLayer *masklay);
/**
 * Inverse of #BKE_mask_layer_shape_to_mask
 */
void BKE_mask_layer_shape_from_mask(MaskLayer *masklay, MaskLayerShape *masklay_shape);
/**
 * Inverse of #BKE_mask_layer_shape_from_mask
 */
void BKE_mask_layer_shape_to_mask(MaskLayer *masklay, MaskLayerShape *masklay_shape);
/**
 * \note Linear interpolation only.
 */
void BKE_mask_layer_shape_to_mask_interp(MaskLayer *masklay,
                                         MaskLayerShape *masklay_shape_a,
                                         MaskLayerShape *masklay_shape_b,
                                         float fac);
MaskLayerShape *BKE_mask_layer_shape_find_frame(MaskLayer *masklay, int frame);
/**
 * When returning 2 - the frame isn't found but before/after frames are.
 */
int BKE_mask_layer_shape_find_frame_range(MaskLayer *masklay,
                                          float frame,
                                          MaskLayerShape **r_masklay_shape_a,
                                          MaskLayerShape **r_masklay_shape_b);
/**
 * \note Does *not* add to the list.
 */
MaskLayerShape *BKE_mask_layer_shape_alloc(MaskLayer *masklay, int frame);
void BKE_mask_layer_shape_free(MaskLayerShape *masklay_shape);
MaskLayerShape *BKE_mask_layer_shape_verify_frame(MaskLayer *masklay, int frame);
MaskLayerShape *BKE_mask_layer_shape_duplicate(MaskLayerShape *masklay_shape);
void BKE_mask_layer_shape_unlink(MaskLayer *masklay, MaskLayerShape *masklay_shape);
void BKE_mask_layer_shape_sort(MaskLayer *masklay);

bool BKE_mask_layer_shape_spline_from_index(MaskLayer *masklay,
                                            int index,
                                            MaskSpline **r_masklay_shape,
                                            int *r_index);
int BKE_mask_layer_shape_spline_to_index(MaskLayer *masklay, MaskSpline *spline);

/**
 * When a new points added, resizing all shape-key arrays.
 */
void BKE_mask_layer_shape_changed_add(MaskLayer *masklay,
                                      int index,
                                      bool do_init,
                                      bool do_init_interpolate);

/**
 * Move array elements to account for removed point.
 */
void BKE_mask_layer_shape_changed_remove(MaskLayer *masklay, int index, int count);

int BKE_mask_get_duration(Mask *mask);

/** \} */

/* -------------------------------------------------------------------- */
/** \name Clipboard
 * \{ */

/**
 * Free the clipboard.
 */
void BKE_mask_clipboard_free();
/**
 * Copy selected visible splines from the given layer to clipboard.
 */
void BKE_mask_clipboard_copy_from_layer(MaskLayer *mask_layer);
/**
 * Check clipboard is empty.
 */
bool BKE_mask_clipboard_is_empty();
/**
 * Paste the contents of clipboard to given mask layer.
 */
void BKE_mask_clipboard_paste_to_layer(Main *bmain, MaskLayer *mask_layer);

/** \} */

/* -------------------------------------------------------------------- */
/** \name Evaluation
 * \{ */

/* `mask_evaluate.cc` */

int BKE_mask_spline_resolution(MaskSpline *spline, int width, int height);
unsigned int BKE_mask_spline_feather_resolution(MaskSpline *spline, int width, int height);
int BKE_mask_spline_differentiate_calc_total(const MaskSpline *spline, unsigned int resol);

float (*BKE_mask_spline_differentiate_with_resolution(MaskSpline *spline,
                                                      unsigned int resol,
                                                      unsigned int *r_tot_diff_point))[2];
void BKE_mask_spline_feather_collapse_inner_loops(MaskSpline *spline,
                                                  float (*feather_points)[2],
                                                  unsigned int tot_feather_point);
float (*BKE_mask_spline_differentiate(
    MaskSpline *spline, int width, int height, unsigned int *r_tot_diff_point))[2];
/**
 * values align with #BKE_mask_spline_differentiate_with_resolution
 * when \a resol arguments match.
 */
float (*BKE_mask_spline_feather_differentiated_points_with_resolution(
    MaskSpline *spline,
    unsigned int resol,
    bool do_feather_isect,
    unsigned int *r_tot_feather_point))[2];

/* *** mask point functions which involve evaluation *** */

float (*BKE_mask_spline_feather_points(MaskSpline *spline, int *tot_feather_point))[2];

float *BKE_mask_point_segment_diff(MaskSpline *spline,
                                   MaskSplinePoint *point,
                                   int width,
                                   int height,
                                   unsigned int *r_tot_diff_point);

/* *** mask point functions which involve evaluation *** */

float *BKE_mask_point_segment_feather_diff(MaskSpline *spline,
                                           MaskSplinePoint *point,
                                           int width,
                                           int height,
                                           unsigned int *r_tot_feather_point);

void BKE_mask_layer_evaluate_animation(MaskLayer *masklay, float ctime);
void BKE_mask_layer_evaluate_deform(MaskLayer *masklay, float ctime);

void BKE_mask_eval_animation(Depsgraph *depsgraph, Mask *mask);
void BKE_mask_eval_update(Depsgraph *depsgraph, Mask *mask);

/** \} */

/* -------------------------------------------------------------------- */
/** \name Rasterization
 * \{ */

/* `mask_rasterize.cc` */

struct MaskRasterHandle;

MaskRasterHandle *BKE_maskrasterize_handle_new();
void BKE_maskrasterize_handle_free(MaskRasterHandle *mr_handle);
void BKE_maskrasterize_handle_init(MaskRasterHandle *mr_handle,
                                   Mask *mask,
                                   int width,
                                   int height,
                                   bool do_aspect_correct,
                                   bool do_mask_aa,
                                   bool do_feather);
float BKE_maskrasterize_handle_sample(MaskRasterHandle *mr_handle, const float xy[2]);

/**
 * \brief Rasterize a buffer from a single mask (threaded execution).
 */
void BKE_maskrasterize_buffer(MaskRasterHandle *mr_handle,
                              unsigned int width,
                              unsigned int height,
                              float *buffer);

/** \} */
