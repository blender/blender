/* SPDX-FileCopyrightText: 2008 Blender Authors
 *
 * SPDX-License-Identifier: GPL-2.0-or-later */

#pragma once

/** \file
 * \ingroup bke
 */

namespace blender {

struct bGPDstroke;
struct bGPdata;

/* Stroke geometry utilities. */

/**
 * Recalc all internal geometry data for the stroke
 * \param gpd: Legacy Grease pencil data-block
 * \param gps: Legacy Grease pencil stroke
 */
void BKE_gpencil_legacy_stroke_geometry_update(struct bGPdata *gpd, struct bGPDstroke *gps);

}  // namespace blender
