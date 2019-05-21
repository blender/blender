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
 * The Original Code is Copyright (C) 2012 Blender Foundation.
 * All rights reserved.
 */

#ifndef __BKE_MASK_H__
#define __BKE_MASK_H__

/** \file
 * \ingroup bke
 */

struct Depsgraph;
struct Image;
struct ImageUser;
struct ListBase;
struct Main;
struct Mask;
struct MaskLayer;
struct MaskLayerShape;
struct MaskParent;
struct MaskSpline;
struct MaskSplinePoint;
struct MaskSplinePointUW;
struct MovieClip;
struct MovieClipUser;
struct Scene;

/* mask_ops.c */
typedef enum {
  MASK_WHICH_HANDLE_NONE = 0,
  MASK_WHICH_HANDLE_STICK = 1,
  MASK_WHICH_HANDLE_LEFT = 2,
  MASK_WHICH_HANDLE_RIGHT = 3,
  MASK_WHICH_HANDLE_BOTH = 4,
} eMaskWhichHandle;

typedef enum {
  MASK_HANDLE_MODE_STICK = 1,
  MASK_HANDLE_MODE_INDIVIDUAL_HANDLES = 2,
} eMaskhandleMode;

struct MaskSplinePoint *BKE_mask_spline_point_array(struct MaskSpline *spline);
struct MaskSplinePoint *BKE_mask_spline_point_array_from_point(
    struct MaskSpline *spline, const struct MaskSplinePoint *point_ref);

/* mask layers */
struct MaskLayer *BKE_mask_layer_new(struct Mask *mask, const char *name);
struct MaskLayer *BKE_mask_layer_active(struct Mask *mask);
void BKE_mask_layer_active_set(struct Mask *mask, struct MaskLayer *masklay);
void BKE_mask_layer_remove(struct Mask *mask, struct MaskLayer *masklay);

void BKE_mask_layer_free_shapes(struct MaskLayer *masklay);
void BKE_mask_layer_free(struct MaskLayer *masklay);
void BKE_mask_layer_free_list(struct ListBase *masklayers);
void BKE_mask_spline_free(struct MaskSpline *spline);
void BKE_mask_spline_free_list(struct ListBase *splines);
struct MaskSpline *BKE_mask_spline_copy(const struct MaskSpline *spline);
void BKE_mask_point_free(struct MaskSplinePoint *point);

void BKE_mask_layer_unique_name(struct Mask *mask, struct MaskLayer *masklay);
void BKE_mask_layer_rename(struct Mask *mask,
                           struct MaskLayer *masklay,
                           char *oldname,
                           char *newname);

struct MaskLayer *BKE_mask_layer_copy(const struct MaskLayer *layer);
void BKE_mask_layer_copy_list(struct ListBase *masklayers_new, const struct ListBase *masklayers);

/* splines */
struct MaskSpline *BKE_mask_spline_add(struct MaskLayer *masklay);
bool BKE_mask_spline_remove(struct MaskLayer *mask_layer, struct MaskSpline *spline);
void BKE_mask_point_direction_switch(struct MaskSplinePoint *point);
void BKE_mask_spline_direction_switch(struct MaskLayer *masklay, struct MaskSpline *spline);

struct BezTriple *BKE_mask_spline_point_next_bezt(struct MaskSpline *spline,
                                                  struct MaskSplinePoint *points_array,
                                                  struct MaskSplinePoint *point);

typedef enum {
  MASK_PROJ_NEG = -1,
  MASK_PROJ_ANY = 0,
  MASK_PROJ_POS = 1,
} eMaskSign;
float BKE_mask_spline_project_co(struct MaskSpline *spline,
                                 struct MaskSplinePoint *point,
                                 float start_u,
                                 const float co[2],
                                 const eMaskSign sign);

/* point */
eMaskhandleMode BKE_mask_point_handles_mode_get(struct MaskSplinePoint *point);
void BKE_mask_point_handle(struct MaskSplinePoint *point,
                           eMaskWhichHandle which_handle,
                           float handle[2]);
void BKE_mask_point_set_handle(struct MaskSplinePoint *point,
                               eMaskWhichHandle which_handle,
                               float loc[2],
                               bool keep_direction,
                               float orig_handle[2],
                               float orig_vec[3][3]);

void BKE_mask_point_segment_co(struct MaskSpline *spline,
                               struct MaskSplinePoint *point,
                               float u,
                               float co[2]);
void BKE_mask_point_normal(struct MaskSpline *spline,
                           struct MaskSplinePoint *point,
                           float u,
                           float n[2]);
float BKE_mask_point_weight_scalar(struct MaskSpline *spline,
                                   struct MaskSplinePoint *point,
                                   const float u);
float BKE_mask_point_weight(struct MaskSpline *spline,
                            struct MaskSplinePoint *point,
                            const float u);
struct MaskSplinePointUW *BKE_mask_point_sort_uw(struct MaskSplinePoint *point,
                                                 struct MaskSplinePointUW *uw);
void BKE_mask_point_add_uw(struct MaskSplinePoint *point, float u, float w);

void BKE_mask_point_select_set(struct MaskSplinePoint *point, const bool do_select);
void BKE_mask_point_select_set_handle(struct MaskSplinePoint *point,
                                      const eMaskWhichHandle which_handle,
                                      const bool do_select);

/* general */
struct Mask *BKE_mask_new(struct Main *bmain, const char *name);
void BKE_mask_copy_data(struct Main *bmain,
                        struct Mask *mask_dst,
                        const struct Mask *mask_src,
                        const int flag);
struct Mask *BKE_mask_copy_nolib(struct Mask *mask);
struct Mask *BKE_mask_copy(struct Main *bmain, const struct Mask *mask);

void BKE_mask_make_local(struct Main *bmain, struct Mask *mask, const bool lib_local);

void BKE_mask_free(struct Mask *mask);

void BKE_mask_coord_from_frame(float r_co[2], const float co[2], const float frame_size[2]);
void BKE_mask_coord_from_movieclip(struct MovieClip *clip,
                                   struct MovieClipUser *user,
                                   float r_co[2],
                                   const float co[2]);
void BKE_mask_coord_from_image(struct Image *image,
                               struct ImageUser *iuser,
                               float r_co[2],
                               const float co[2]);
void BKE_mask_coord_to_frame(float r_co[2], const float co[2], const float frame_size[2]);
void BKE_mask_coord_to_movieclip(struct MovieClip *clip,
                                 struct MovieClipUser *user,
                                 float r_co[2],
                                 const float co[2]);
void BKE_mask_coord_to_image(struct Image *image,
                             struct ImageUser *iuser,
                             float r_co[2],
                             const float co[2]);

/* parenting */

void BKE_mask_evaluate_all_masks(struct Main *bmain, float ctime, const bool do_newframe);
void BKE_mask_evaluate(struct Mask *mask, const float ctime, const bool do_newframe);
void BKE_mask_layer_evaluate(struct MaskLayer *masklay, const float ctime, const bool do_newframe);
void BKE_mask_parent_init(struct MaskParent *parent);
void BKE_mask_calc_handle_adjacent_interp(struct MaskSpline *spline,
                                          struct MaskSplinePoint *point,
                                          const float u);
void BKE_mask_calc_tangent_polyline(struct MaskSpline *spline,
                                    struct MaskSplinePoint *point,
                                    float t[2]);
void BKE_mask_calc_handle_point(struct MaskSpline *spline, struct MaskSplinePoint *point);
void BKE_mask_calc_handle_point_auto(struct MaskSpline *spline,
                                     struct MaskSplinePoint *point,
                                     const bool do_recalc_length);
void BKE_mask_get_handle_point_adjacent(struct MaskSpline *spline,
                                        struct MaskSplinePoint *point,
                                        struct MaskSplinePoint **r_point_prev,
                                        struct MaskSplinePoint **r_point_next);
void BKE_mask_layer_calc_handles(struct MaskLayer *masklay);
void BKE_mask_spline_ensure_deform(struct MaskSpline *spline);
void BKE_mask_point_parent_matrix_get(struct MaskSplinePoint *point,
                                      float ctime,
                                      float parent_matrix[3][3]);

/* animation */
int BKE_mask_layer_shape_totvert(struct MaskLayer *masklay);
void BKE_mask_layer_shape_from_mask(struct MaskLayer *masklay,
                                    struct MaskLayerShape *masklay_shape);
void BKE_mask_layer_shape_to_mask(struct MaskLayer *masklay, struct MaskLayerShape *masklay_shape);
void BKE_mask_layer_shape_to_mask_interp(struct MaskLayer *masklay,
                                         struct MaskLayerShape *masklay_shape_a,
                                         struct MaskLayerShape *masklay_shape_b,
                                         const float fac);
struct MaskLayerShape *BKE_mask_layer_shape_find_frame(struct MaskLayer *masklay, const int frame);
int BKE_mask_layer_shape_find_frame_range(struct MaskLayer *masklay,
                                          const float frame,
                                          struct MaskLayerShape **r_masklay_shape_a,
                                          struct MaskLayerShape **r_masklay_shape_b);
struct MaskLayerShape *BKE_mask_layer_shape_alloc(struct MaskLayer *masklay, const int frame);
void BKE_mask_layer_shape_free(struct MaskLayerShape *masklay_shape);
struct MaskLayerShape *BKE_mask_layer_shape_verify_frame(struct MaskLayer *masklay,
                                                         const int frame);
struct MaskLayerShape *BKE_mask_layer_shape_duplicate(struct MaskLayerShape *masklay_shape);
void BKE_mask_layer_shape_unlink(struct MaskLayer *masklay, struct MaskLayerShape *masklay_shape);
void BKE_mask_layer_shape_sort(struct MaskLayer *masklay);

bool BKE_mask_layer_shape_spline_from_index(struct MaskLayer *masklay,
                                            int index,
                                            struct MaskSpline **r_masklay_shape,
                                            int *r_index);
int BKE_mask_layer_shape_spline_to_index(struct MaskLayer *masklay, struct MaskSpline *spline);

void BKE_mask_layer_shape_changed_add(struct MaskLayer *masklay,
                                      int index,
                                      bool do_init,
                                      bool do_init_interpolate);

void BKE_mask_layer_shape_changed_remove(struct MaskLayer *masklay, int index, int count);

int BKE_mask_get_duration(struct Mask *mask);

/* clipboard */
void BKE_mask_clipboard_free(void);
void BKE_mask_clipboard_copy_from_layer(struct MaskLayer *mask_layer);
bool BKE_mask_clipboard_is_empty(void);
void BKE_mask_clipboard_paste_to_layer(struct Main *bmain, struct MaskLayer *mask_layer);

#define MASKPOINT_ISSEL_ANY(p) ((((p)->bezt.f1 | (p)->bezt.f2 | (p)->bezt.f3) & SELECT) != 0)
#define MASKPOINT_ISSEL_KNOT(p) (((p)->bezt.f2 & SELECT) != 0)

#define MASKPOINT_ISSEL_HANDLE(point, which_handle) \
  (((which_handle == MASK_WHICH_HANDLE_STICK) ? \
        ((((point)->bezt.f1 | (point)->bezt.f3) & SELECT)) : \
        ((which_handle == MASK_WHICH_HANDLE_LEFT) ? ((point)->bezt.f1 & SELECT) : \
                                                    ((point)->bezt.f3 & SELECT))) != 0)

#define MASKPOINT_SEL_ALL(p) \
  { \
    (p)->bezt.f1 |= SELECT; \
    (p)->bezt.f2 |= SELECT; \
    (p)->bezt.f3 |= SELECT; \
  } \
  (void)0
#define MASKPOINT_DESEL_ALL(p) \
  { \
    (p)->bezt.f1 &= ~SELECT; \
    (p)->bezt.f2 &= ~SELECT; \
    (p)->bezt.f3 &= ~SELECT; \
  } \
  (void)0
#define MASKPOINT_INVSEL_ALL(p) \
  { \
    (p)->bezt.f1 ^= SELECT; \
    (p)->bezt.f2 ^= SELECT; \
    (p)->bezt.f3 ^= SELECT; \
  } \
  (void)0

#define MASK_RESOL_MAX 128

/* mask_evaluate.c */
unsigned int BKE_mask_spline_resolution(struct MaskSpline *spline, int width, int height);
unsigned int BKE_mask_spline_feather_resolution(struct MaskSpline *spline, int width, int height);
int BKE_mask_spline_differentiate_calc_total(const struct MaskSpline *spline,
                                             const unsigned int resol);

float (*BKE_mask_spline_differentiate_with_resolution(struct MaskSpline *spline,
                                                      unsigned int *tot_diff_point,
                                                      const unsigned int resol))[2];
void BKE_mask_spline_feather_collapse_inner_loops(struct MaskSpline *spline,
                                                  float (*feather_points)[2],
                                                  const unsigned int tot_feather_point);
float (*BKE_mask_spline_differentiate(
    struct MaskSpline *spline, int width, int height, unsigned int *tot_diff_point))[2];
float (*BKE_mask_spline_feather_differentiated_points_with_resolution(
    struct MaskSpline *spline,
    unsigned int *tot_feather_point,
    const unsigned int resol,
    const bool do_feather_isect))[2];

/* *** mask point functions which involve evaluation *** */
float (*BKE_mask_spline_feather_points(struct MaskSpline *spline, int *tot_feather_point))[2];

float *BKE_mask_point_segment_diff(struct MaskSpline *spline,
                                   struct MaskSplinePoint *point,
                                   int width,
                                   int height,
                                   unsigned int *tot_diff_point);

float *BKE_mask_point_segment_feather_diff(struct MaskSpline *spline,
                                           struct MaskSplinePoint *point,
                                           int width,
                                           int height,
                                           unsigned int *tot_feather_point);

void BKE_mask_layer_evaluate_animation(struct MaskLayer *masklay, const float ctime);
void BKE_mask_layer_evaluate_deform(struct MaskLayer *masklay, const float ctime);

void BKE_mask_eval_animation(struct Depsgraph *depsgraph, struct Mask *mask);
void BKE_mask_eval_update(struct Depsgraph *depsgraph, struct Mask *mask);

/* mask_rasterize.c */
struct MaskRasterHandle;
typedef struct MaskRasterHandle MaskRasterHandle;

MaskRasterHandle *BKE_maskrasterize_handle_new(void);
void BKE_maskrasterize_handle_free(MaskRasterHandle *mr_handle);
void BKE_maskrasterize_handle_init(MaskRasterHandle *mr_handle,
                                   struct Mask *mask,
                                   const int width,
                                   const int height,
                                   const bool do_aspect_correct,
                                   const bool do_mask_aa,
                                   const bool do_feather);
float BKE_maskrasterize_handle_sample(MaskRasterHandle *mr_handle, const float xy[2]);

void BKE_maskrasterize_buffer(MaskRasterHandle *mr_handle,
                              const unsigned int width,
                              const unsigned int height,
                              float *buffer);

#endif /* __BKE_MASK_H__ */
