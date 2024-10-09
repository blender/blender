/* SPDX-FileCopyrightText: 2008 Blender Authors
 *
 * SPDX-License-Identifier: GPL-2.0-or-later */

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
 * Creates a bGPDcurve by doing a cubic curve fitting on the grease pencil stroke points.
 */
struct bGPDcurve *BKE_gpencil_stroke_editcurve_generate(struct bGPDstroke *gps,
                                                        float error_threshold,
                                                        float corner_angle,
                                                        float stroke_radius);
/**
 * Sync the selection from stroke to edit-curve.
 */
void BKE_gpencil_editcurve_stroke_sync_selection(struct bGPdata *gpd,
                                                 struct bGPDstroke *gps,
                                                 struct bGPDcurve *gpc);
/**
 * Recalculate the handles of the edit curve of a grease pencil stroke.
 */
void BKE_gpencil_editcurve_recalculate_handles(struct bGPDstroke *gps);
void BKE_gpencil_editcurve_subdivide(struct bGPDstroke *gps, int cuts);

#ifdef __cplusplus
}
#endif
