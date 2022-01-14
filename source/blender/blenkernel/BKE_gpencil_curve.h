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

/**
 * Convert a curve object to grease pencil stroke.
 *
 * \param bmain: Main thread pointer
 * \param scene: Original scene.
 * \param ob_gp: Grease pencil object to add strokes.
 * \param ob_cu: Curve to convert.
 * \param use_collections: Create layers using collection names.
 * \param scale_thickness: Scale thickness factor.
 * \param sample: Sample distance, zero to disable.
 */
void BKE_gpencil_convert_curve(struct Main *bmain,
                               struct Scene *scene,
                               struct Object *ob_gp,
                               struct Object *ob_cu,
                               bool use_collections,
                               float scale_thickness,
                               float sample);

/**
 * Creates a bGPDcurve by doing a cubic curve fitting on the grease pencil stroke points.
 */
struct bGPDcurve *BKE_gpencil_stroke_editcurve_generate(struct bGPDstroke *gps,
                                                        float error_threshold,
                                                        float corner_angle,
                                                        float stroke_radius);
/**
 * Updates the edit-curve for a stroke. Frees the old curve if one exists and generates a new one.
 */
void BKE_gpencil_stroke_editcurve_update(struct bGPdata *gpd,
                                         struct bGPDlayer *gpl,
                                         struct bGPDstroke *gps);
/**
 * Sync the selection from stroke to edit-curve.
 */
void BKE_gpencil_editcurve_stroke_sync_selection(struct bGPdata *gpd,
                                                 struct bGPDstroke *gps,
                                                 struct bGPDcurve *gpc);
/**
 * Sync the selection from edit-curve to stroke.
 */
void BKE_gpencil_stroke_editcurve_sync_selection(struct bGPdata *gpd,
                                                 struct bGPDstroke *gps,
                                                 struct bGPDcurve *gpc);
void BKE_gpencil_strokes_selected_update_editcurve(struct bGPdata *gpd);
void BKE_gpencil_strokes_selected_sync_selection_editcurve(struct bGPdata *gpd);
/**
 * Recalculate stroke points with the edit-curve of the stroke.
 */
void BKE_gpencil_stroke_update_geometry_from_editcurve(struct bGPDstroke *gps,
                                                       uint resolution,
                                                       bool is_adaptive);
/**
 * Recalculate the handles of the edit curve of a grease pencil stroke.
 */
void BKE_gpencil_editcurve_recalculate_handles(struct bGPDstroke *gps);
void BKE_gpencil_editcurve_subdivide(struct bGPDstroke *gps, int cuts);

#ifdef __cplusplus
}
#endif
