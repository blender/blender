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
 * The Original Code is Copyright (C) 2012 Blender Foundation.
 * All rights reserved.
 *
 * Contributor(s): Blender Foundation,
 *                 Sergey Sharybin
 *
 * ***** END GPL LICENSE BLOCK *****
 */

#ifndef __BKE_MASK_H__
#define __BKE_MASK_H__

struct Main;
struct Mask;
struct MaskParent;
struct MaskObject;
struct MaskObjectShape;
struct MaskSpline;
struct MaskSplinePoint;
struct MaskSplinePointUW;
struct MovieClip;
struct MovieClipUser;
struct Scene;

struct MaskSplinePoint *BKE_mask_spline_point_array(struct MaskSpline *spline);
struct MaskSplinePoint *BKE_mask_spline_point_array_from_point(struct MaskSpline *spline, struct MaskSplinePoint *point_ref);

/* mask objects */
struct MaskObject *BKE_mask_object_new(struct Mask *mask, const char *name);
struct MaskObject *BKE_mask_object_active(struct Mask *mask);
void BKE_mask_object_active_set(struct Mask *mask, struct MaskObject *maskobj);
void BKE_mask_object_remove(struct Mask *mask, struct MaskObject *maskobj);

void BKE_mask_object_free(struct MaskObject *maskobj);
void BKE_mask_spline_free(struct MaskSpline *spline);
void BKE_mask_point_free(struct MaskSplinePoint *point);

void BKE_mask_object_unique_name(struct Mask *mask, struct MaskObject *maskobj);

/* splines */
struct MaskSpline *BKE_mask_spline_add(struct MaskObject *maskobj);
int BKE_mask_spline_resolution(struct MaskSpline *spline);

float (*BKE_mask_spline_differentiate(struct MaskSpline *spline, int *tot_diff_point))[2];
float (*BKE_mask_spline_feather_differentiated_points(struct MaskSpline *spline, int *tot_feather_point))[2];
float (*BKE_mask_spline_feather_points(struct MaskSpline *spline, int *tot_feather_point))[2];

/* point */
int BKE_mask_point_has_handle(struct MaskSplinePoint *point);
void BKE_mask_point_handle(struct MaskSplinePoint *point, float handle[2]);
void BKE_mask_point_set_handle(struct MaskSplinePoint *point, float loc[2], int keep_direction,
                               float aspx, float aspy, float orig_handle[2], float orig_vec[3][3]);
float *BKE_mask_point_segment_diff(struct MaskSpline *spline, struct MaskSplinePoint *point, int *tot_diff_point);
float *BKE_mask_point_segment_feather_diff(struct MaskSpline *spline, struct MaskSplinePoint *point,
                                           int *tot_feather_point);
void BKE_mask_point_segment_co(struct MaskSpline *spline, struct MaskSplinePoint *point, float u, float co[2]);
void BKE_mask_point_normal(struct MaskSpline *spline, struct MaskSplinePoint *point,
                           float u, float n[2]);
float BKE_mask_point_weight(struct MaskSpline *spline, struct MaskSplinePoint *point, float u);
struct MaskSplinePointUW *BKE_mask_point_sort_uw(struct MaskSplinePoint *point, struct MaskSplinePointUW *uw);
void BKE_mask_point_add_uw(struct MaskSplinePoint *point, float u, float w);

void BKE_mask_point_select_set(struct MaskSplinePoint *point, int select);
void BKE_mask_point_select_set_handle(struct MaskSplinePoint *point, int select);

/* general */
struct Mask *BKE_mask_new(const char *name);

void BKE_mask_free(struct Mask *mask);
void BKE_mask_unlink(struct Main *bmain, struct Mask *mask);

void BKE_mask_coord_from_movieclip(struct MovieClip *clip, struct MovieClipUser *user, float r_co[2], const float co[2]);
void BKE_mask_coord_to_movieclip(struct MovieClip *clip, struct MovieClipUser *user, float r_co[2], const float co[2]);

/* parenting */

void BKE_mask_update_display(struct Mask *mask, float ctime);

void BKE_mask_evaluate_all_masks(struct Main *bmain, float ctime, const int do_newframe);
void BKE_mask_update_scene(struct Main *bmain, struct Scene *scene, const int do_newframe);
void BKE_mask_parent_init(struct MaskParent *parent);
void BKE_mask_calc_handle_adjacent_length(struct Mask *mask, struct MaskSpline *spline, struct MaskSplinePoint *point);
void BKE_mask_calc_tangent_polyline(struct Mask *mask, struct MaskSpline *spline, struct MaskSplinePoint *point, float t[2]);
void BKE_mask_calc_handle_point(struct Mask *mask, struct MaskSpline *spline, struct MaskSplinePoint *point);
void BKE_mask_calc_handle_point_auto(struct Mask *mask, struct MaskSpline *spline, struct MaskSplinePoint *point,
                                     const short do_recalc_length);
void BKE_mask_get_handle_point_adjacent(struct Mask *mask, struct MaskSpline *spline, struct MaskSplinePoint *point,
                                        struct MaskSplinePoint **r_point_prev, struct MaskSplinePoint **r_point_next);
void BKE_mask_calc_handles(struct Mask *mask);
void BKE_mask_spline_ensure_deform(struct MaskSpline *spline);

/* animation */
int  BKE_mask_object_shape_totvert(struct MaskObject *maskobj);
void BKE_mask_object_shape_from_mask(struct MaskObject *maskobj, struct MaskObjectShape *maskobj_shape);
void BKE_mask_object_shape_to_mask(struct MaskObject *maskobj, struct MaskObjectShape *maskobj_shape);
void BKE_mask_object_shape_to_mask_interp(struct MaskObject *maskobj,
                                          struct MaskObjectShape *maskobj_shape_a,
                                          struct MaskObjectShape *maskobj_shape_b,
                                          const float fac);
struct MaskObjectShape *BKE_mask_object_shape_find_frame(struct MaskObject *maskobj, int frame);
int BKE_mask_object_shape_find_frame_range(struct MaskObject *maskobj, int frame,
                                           struct MaskObjectShape **r_maskobj_shape_a,
                                           struct MaskObjectShape **r_maskobj_shape_b);
struct MaskObjectShape *BKE_mask_object_shape_varify_frame(struct MaskObject *maskobj, int frame);
void BKE_mask_object_shape_unlink(struct MaskObject *maskobj, struct MaskObjectShape *maskobj_shape);
void BKE_mask_object_shape_sort(struct MaskObject *maskobj);

int BKE_mask_object_shape_spline_from_index(struct MaskObject *maskobj, int index,
                                            struct MaskSpline **r_maskobj_shape, int *r_index);
int BKE_mask_object_shape_spline_to_index(struct MaskObject *maskobj, struct MaskSpline *spline);

int BKE_mask_object_shape_spline_index(struct MaskObject *maskobj, int index,
                                       struct MaskSpline **r_maskobj_shape, int *r_index);
void BKE_mask_object_shape_changed_add(struct MaskObject *maskobj, int index,
                                       int do_init, int do_init_interpolate);

void BKE_mask_object_shape_changed_remove(struct MaskObject *maskobj, int index, int count);

/* rasterization */
void BKE_mask_rasterize(struct Mask *mask, int width, int height, float *buffer);

#define MASKPOINT_ISSEL_ANY(p)          ( ((p)->bezt.f1 | (p)->bezt.f2 | (p)->bezt.f2) & SELECT)
#define MASKPOINT_ISSEL_KNOT(p)         ( (p)->bezt.f2 & SELECT)
#define MASKPOINT_ISSEL_HANDLE_ONLY(p)  ( (((p)->bezt.f1 | (p)->bezt.f2) & SELECT) && (((p)->bezt.f2 & SELECT) == 0) )
#define MASKPOINT_ISSEL_HANDLE(p)       ( (((p)->bezt.f1 | (p)->bezt.f2) & SELECT) )

#define MASKPOINT_SEL_ALL(p)    { (p)->bezt.f1 |=  SELECT; (p)->bezt.f2 |=  SELECT; (p)->bezt.f3 |=  SELECT; } (void)0
#define MASKPOINT_DESEL_ALL(p)  { (p)->bezt.f1 &= ~SELECT; (p)->bezt.f2 &= ~SELECT; (p)->bezt.f3 &= ~SELECT; } (void)0
#define MASKPOINT_INVSEL_ALL(p) { (p)->bezt.f1 ^=  SELECT; (p)->bezt.f2 ^=  SELECT; (p)->bezt.f3 ^=  SELECT; } (void)0

#define MASKPOINT_SEL_HANDLE(p)     { (p)->bezt.f1 |=  SELECT; (p)->bezt.f3 |=  SELECT; } (void)0
#define MASKPOINT_DESEL_HANDLE(p)   { (p)->bezt.f1 &= ~SELECT; (p)->bezt.f3 &= ~SELECT; } (void)0

#endif
