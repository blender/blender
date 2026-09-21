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

struct ShapeData {
  Vector<int> shape_map;
  Vector<int> shape_offsets;

  const GroupedSpan<int> shapes() const
  {
    return GroupedSpan<int>(shape_offsets.as_span(), shape_map.as_span());
  };
};

/**
 * Calculate all of the shapes from the "fill_id" attribute. Each shape is either a group of
 * multiple curves that share the same "fill_id", or a single curve when the "fill_id" is zero.
 *
 * For example:
 *
 * curve index:   0 1 2 3 4 5 6 7 8
 * fill_id:       0 0 a 0 a c a b b   (a, b, c are some integers != 0)
 *
 * shape_map:     0 1 2 4 6 3 5 7 8
 * shape_offsets: 0 1 2     5 6 7   9
 * shapes:        _ _ _____ _ _ ___
 *                    a       c b     (ordered by the first occurrence in `fill_id`)
 *
 * Returns a #ShapeData struct with the #shape_map and #shape_offsets.
 */
ShapeData shapes_from_fill_ids(const VArray<int> &fill_ids, int curves_num);

/* Get the next available fill ID. */
int get_next_available_fill_id(Span<int> fill_ids);
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
