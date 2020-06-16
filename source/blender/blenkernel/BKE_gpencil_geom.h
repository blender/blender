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
 * The Original Code is Copyright (C) 2008, Blender Foundation
 * This is a new part of Blender
 */

#ifndef __BKE_GPENCIL_GEOM_H__
#define __BKE_GPENCIL_GEOM_H__

/** \file
 * \ingroup bke
 */

#ifdef __cplusplus
extern "C" {
#endif

struct BoundBox;
struct Depsgraph;
struct Main;
struct Object;
struct Scene;
struct bGPDframe;
struct bGPDlayer;
struct bGPDspoint;
struct bGPDstroke;
struct bGPdata;

/* Object boundbox. */
bool BKE_gpencil_data_minmax(const struct bGPdata *gpd, float r_min[3], float r_max[3]);
bool BKE_gpencil_stroke_minmax(const struct bGPDstroke *gps,
                               const bool use_select,
                               float r_min[3],
                               float r_max[3]);

struct BoundBox *BKE_gpencil_boundbox_get(struct Object *ob);
void BKE_gpencil_centroid_3d(struct bGPdata *gpd, float r_centroid[3]);
void BKE_gpencil_stroke_boundingbox_calc(struct bGPDstroke *gps);

/* stroke geometry utilities */
void BKE_gpencil_stroke_normal(const struct bGPDstroke *gps, float r_normal[3]);
void BKE_gpencil_stroke_simplify_adaptive(struct bGPDstroke *gps, float factor);
void BKE_gpencil_stroke_simplify_fixed(struct bGPDstroke *gps);
void BKE_gpencil_stroke_subdivide(struct bGPDstroke *gps, int level, int type);
bool BKE_gpencil_stroke_trim(struct bGPDstroke *gps);
void BKE_gpencil_stroke_merge_distance(struct bGPDframe *gpf,
                                       struct bGPDstroke *gps,
                                       const float threshold,
                                       const bool use_unselected);

void BKE_gpencil_stroke_2d_flat(const struct bGPDspoint *points,
                                int totpoints,
                                float (*points2d)[2],
                                int *r_direction);
void BKE_gpencil_stroke_2d_flat_ref(const struct bGPDspoint *ref_points,
                                    int ref_totpoints,
                                    const struct bGPDspoint *points,
                                    int totpoints,
                                    float (*points2d)[2],
                                    const float scale,
                                    int *r_direction);
void BKE_gpencil_stroke_fill_triangulate(struct bGPDstroke *gps);
void BKE_gpencil_stroke_geometry_update(struct bGPDstroke *gps);
void BKE_gpencil_stroke_uv_update(struct bGPDstroke *gps);

void BKE_gpencil_transform(struct bGPdata *gpd, float mat[4][4]);

bool BKE_gpencil_stroke_sample(struct bGPDstroke *gps, const float dist, const bool select);
bool BKE_gpencil_stroke_smooth(struct bGPDstroke *gps, int i, float inf);
bool BKE_gpencil_stroke_smooth_strength(struct bGPDstroke *gps, int point_index, float influence);
bool BKE_gpencil_stroke_smooth_thickness(struct bGPDstroke *gps, int point_index, float influence);
bool BKE_gpencil_stroke_smooth_uv(struct bGPDstroke *gps, int point_index, float influence);
bool BKE_gpencil_stroke_close(struct bGPDstroke *gps);
void BKE_gpencil_dissolve_points(struct bGPDframe *gpf, struct bGPDstroke *gps, const short tag);

bool BKE_gpencil_stroke_stretch(struct bGPDstroke *gps, const float dist, const float tip_length);
bool BKE_gpencil_stroke_trim_points(struct bGPDstroke *gps,
                                    const int index_from,
                                    const int index_to);
bool BKE_gpencil_stroke_split(struct bGPDframe *gpf,
                              struct bGPDstroke *gps,
                              const int before_index,
                              struct bGPDstroke **remaining_gps);
bool BKE_gpencil_stroke_shrink(struct bGPDstroke *gps, const float dist);

float BKE_gpencil_stroke_length(const struct bGPDstroke *gps, bool use_3d);

void BKE_gpencil_convert_mesh(struct Main *bmain,
                              struct Depsgraph *depsgraph,
                              struct Scene *scene,
                              struct Object *ob_gp,
                              struct Object *ob_mesh,
                              const float angle,
                              const int thickness,
                              const float offset,
                              const float matrix[4][4],
                              const int frame_offset,
                              const bool use_seams,
                              const bool use_faces);

#ifdef __cplusplus
}
#endif

#endif /*  __BKE_GPENCIL_GEOM_H__ */
