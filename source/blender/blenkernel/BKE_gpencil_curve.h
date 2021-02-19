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

#pragma once

/** \file
 * \ingroup bke
 */

#ifdef __cplusplus
extern "C" {
#endif

struct Main;
struct Object;
struct Scene;
struct bGPDcurve;
struct bGPDlayer;
struct bGPDstroke;
struct bGPdata;

void BKE_gpencil_convert_curve(struct Main *bmain,
                               struct Scene *scene,
                               struct Object *ob_gp,
                               struct Object *ob_cu,
                               const bool use_collections,
                               const float scale_thickness,
                               const float sample);

struct bGPDcurve *BKE_gpencil_stroke_editcurve_generate(struct bGPDstroke *gps,
                                                        const float error_threshold,
                                                        const float corner_angle,
                                                        const float stroke_radius);
void BKE_gpencil_stroke_editcurve_update(struct bGPdata *gpd,
                                         struct bGPDlayer *gpl,
                                         struct bGPDstroke *gps);
void BKE_gpencil_editcurve_stroke_sync_selection(struct bGPdata *gpd,
                                                 struct bGPDstroke *gps,
                                                 struct bGPDcurve *gpc);
void BKE_gpencil_stroke_editcurve_sync_selection(struct bGPdata *gpd,
                                                 struct bGPDstroke *gps,
                                                 struct bGPDcurve *gpc);
void BKE_gpencil_strokes_selected_update_editcurve(struct bGPdata *gpd);
void BKE_gpencil_strokes_selected_sync_selection_editcurve(struct bGPdata *gpd);
void BKE_gpencil_stroke_update_geometry_from_editcurve(struct bGPDstroke *gps,
                                                       const uint resolution,
                                                       const bool is_adaptive);
void BKE_gpencil_editcurve_recalculate_handles(struct bGPDstroke *gps);
void BKE_gpencil_editcurve_subdivide(struct bGPDstroke *gps, const int cuts);

#ifdef __cplusplus
}
#endif
