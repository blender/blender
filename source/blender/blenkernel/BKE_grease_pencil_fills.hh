/* SPDX-FileCopyrightText: 2026 Blender Authors
 *
 * SPDX-License-Identifier: GPL-2.0-or-later */

#pragma once

/** \file
 * \ingroup bke
 */

#include <optional>

#include "BKE_attribute.hh"
#include "BKE_grease_pencil.hh"

namespace blender::bke::greasepencil {

std::optional<FillCache> fill_cache_from_fill_ids(const VArray<int> &fill_ids);

/* Get the next available fill ID. */
int get_next_available_fill_id(const VArray<int> &fill_ids);
/* Fill the mutable span with the next available fill IDs. */
void gather_next_available_fill_ids(const VArray<int> &fill_ids, MutableSpan<int> r_new_fill_ids);
/* Write to the mutable span the next available fill ids at the indices of the given mask. */
void gather_next_available_fill_ids(const VArray<int> &fill_ids,
                                    const IndexMask &curve_mask,
                                    MutableSpan<int> r_new_fill_ids);

IndexMask selected_mask_to_fills(const IndexMask &selected_mask,
                                 const CurvesGeometry &curves,
                                 AttrDomain selection_domain,
                                 IndexMaskMemory &memory);
void separate_fill_ids(CurvesGeometry &curves, const IndexMask &strokes_to_keep);

}  // namespace blender::bke::greasepencil
