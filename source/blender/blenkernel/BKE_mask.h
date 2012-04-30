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
struct MaskShape;
struct MaskSpline;
struct MaskSplinePoint;
struct MaskSplinePointUW;
struct Scene;

/* shapes */
struct MaskShape *BKE_mask_shape_new(struct Mask *mask, const char *name);
struct MaskShape *BKE_mask_shape_active(struct Mask *mask);
void BKE_mask_shape_active_set(struct Mask *mask, struct MaskShape *shape);
void BKE_mask_shape_remove(struct Mask *mask, struct MaskShape *shape);

void BKE_mask_shape_free(struct MaskShape *shape);
void BKE_mask_spline_free(struct MaskSpline *spline);
void BKE_mask_point_free(struct MaskSplinePoint *point);

void BKE_mask_shape_unique_name(struct Mask *mask, struct MaskShape *shape);

/* splines */
struct MaskSpline *BKE_mask_spline_add(struct MaskShape *shape);
int BKE_mask_spline_resolution(struct MaskSpline *spline);
float *BKE_mask_spline_differentiate(struct MaskSpline *spline, int *tot_diff_point);
float *BKE_mask_spline_feather_differentiated_points(struct MaskSpline *spline, float aspx,
                                                    float aspy, int *tot_feather_point);
float *BKE_mask_spline_feather_points(struct MaskSpline *spline, float aspx, float aspy, int *tot_feather_point);

/* point */
int BKE_mask_point_has_handle(struct MaskSplinePoint *point);
void BKE_mask_point_handle(struct MaskSplinePoint *point, float aspx, float aspy, float handle[2]);
void BKE_mask_point_set_handle(struct MaskSplinePoint *point, float loc[2], int keep_direction, float aspx, float aspy, float orig_handle[2], float orig_vec[3][3]);
float *BKE_mask_point_segment_diff(struct MaskSpline *spline, struct MaskSplinePoint *point, int *tot_diff_point);
float *BKE_mask_point_segment_feather_diff(struct MaskSpline *spline, struct MaskSplinePoint *point,
                                           float aspx, float aspy, int *tot_feather_point);
void BKE_mask_point_segment_co(struct MaskSpline *spline, struct MaskSplinePoint *point, float u, float co[2]);
void BKE_mask_point_normal(struct MaskSpline *spline, struct MaskSplinePoint *point, float aspx, float aspy, float u, float n[2]);
float BKE_mask_point_weight(struct MaskSpline *spline, struct MaskSplinePoint *point, float u);
struct MaskSplinePointUW * BKE_mask_point_sort_uw(struct MaskSplinePoint *point, struct MaskSplinePointUW *uw);
void BKE_mask_point_add_uw(struct MaskSplinePoint *point, float u, float w);

/* general */
struct Mask *BKE_mask_new(const char *name);

void BKE_mask_free(struct Mask *mask);
void BKE_mask_unlink(struct Main *bmain, struct Mask *mask);

/* parenting */

void BKE_mask_evaluate_all_masks(struct Main *bmain, float ctime);
void BKE_mask_update_scene(struct Main *bmain, struct Scene *scene);
void BKE_mask_parent_init(struct MaskParent *parent);

#define MASKPOINT_ISSEL(p)	( ((p)->bezt.f1 | (p)->bezt.f2 | (p)->bezt.f2) & SELECT )
#define MASKPOINT_SEL(p)	{ (p)->bezt.f1 |=  SELECT; (p)->bezt.f2 |=  SELECT; (p)->bezt.f3 |=  SELECT; }
#define MASKPOINT_DESEL(p)	{ (p)->bezt.f1 &= ~SELECT; (p)->bezt.f2 &= ~SELECT; (p)->bezt.f3 &= ~SELECT; }
#define MASKPOINT_INVSEL(p)	{ (p)->bezt.f1 ^=  SELECT; (p)->bezt.f2 ^=  SELECT; (p)->bezt.f3 ^=  SELECT; }

#define MASKPOINT_CV_ISSEL(p)		( (p)->bezt.f2 & SELECT )

#define MASKPOINT_HANDLE_ONLY_ISSEL(p)	( (((p)->bezt.f1 | (p)->bezt.f2) & SELECT ) && (((p)->bezt.f2 & SELECT) == 0) )
#define MASKPOINT_HANDLE_ISSEL(p)	( (((p)->bezt.f1 | (p)->bezt.f2) & SELECT ) )
#define MASKPOINT_HANDLE_SEL(p)		{ (p)->bezt.f1 |=  SELECT; (p)->bezt.f3 |=  SELECT; }

#endif
