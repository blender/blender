/* SPDX-FileCopyrightText: 2026 Blender Authors
 *
 * SPDX-License-Identifier: GPL-2.0-or-later */

#include "BLI_multi_value_map.hh"

#include "GEO_xpbd_constraint_coloring_utils.hh"

namespace blender::xpbd {

template<typename PointID, typename GetConstraintPointIdsFn>
inline int color_constraints(GetConstraintPointIdsFn &&get_constraint_point_ids_fn,
                             MutableSpan<int> r_colors)
{
  const int constraints_num = r_colors.size();
  MultiValueMap<PointID, int> constraints_by_point;
  for (const int constraint_i : IndexRange(constraints_num)) {
    for (const PointID &point_id : get_constraint_point_ids_fn(constraint_i)) {
      constraints_by_point.add(point_id, constraint_i);
    }
  }
  int colors_num = 0;
  for (const int constraint_i : IndexRange(constraints_num)) {
    Vector<int> used_colors;
    for (const PointID &point_id : get_constraint_point_ids_fn(constraint_i)) {
      for (const int other_constraint_i : constraints_by_point.lookup(point_id)) {
        if (other_constraint_i >= constraint_i) {
          continue;
        }
        used_colors.append_non_duplicates(r_colors[other_constraint_i]);
      }
    }
    int best_color = 0;
    while (used_colors.contains(best_color)) {
      best_color++;
    }
    r_colors[constraint_i] = best_color;
    colors_num = std::max(colors_num, best_color + 1);
  }
  return colors_num;
}

template<typename PointID, typename GetConstraintPointIdsFn>
inline ConstraintColoring generic_constraint_coloring(
    GetConstraintPointIdsFn &&get_constraint_points_fn,
    const int constraints_num,
    IndexMaskMemory &memory)
{
  if (constraints_num == 0) {
    return {};
  }
  Array<int> colors(constraints_num);
  const int colors_num = color_constraints<PointID>(get_constraint_points_fn, colors);
  Array<Vector<int>> color_indices(colors_num);
  for (const int constraint_i : IndexRange(constraints_num)) {
    color_indices[colors[constraint_i]].append(constraint_i);
  }
  ConstraintColoring coloring;
  for (const int color_i : IndexRange(colors_num)) {
    const IndexMask mask = IndexMask::from_indices<int>(color_indices[color_i], memory);
    coloring.colors.append(mask);
  }
  return coloring;
}

ConstraintColoring color_constraints__unary(const Span<int> affected_points,
                                            IndexMaskMemory &memory)
{
  return generic_constraint_coloring<int>(
      [&](const int constraint_i) { return Span<int>(&affected_points[constraint_i], 1); },
      affected_points.size(),
      memory);
}

ConstraintColoring color_constraints__binary(const Span<int2> affected_points,
                                             IndexMaskMemory &memory)
{
  return generic_constraint_coloring<int>(
      [&](const int constraint_i) { return Span<int>(&affected_points[constraint_i][0], 2); },
      affected_points.size(),
      memory);
}

ConstraintColoring color_constraints__n_ary(const GroupedSpan<int> affected_points,
                                            IndexMaskMemory &memory)
{
  return generic_constraint_coloring<int>(
      [&](const int constraint_i) { return affected_points[constraint_i]; },
      affected_points.size(),
      memory);
}

ConstraintColoring color_constraints__all_independent(const int constraints_num)
{
  return ConstraintColoring{{IndexMask(constraints_num)}};
}

}  // namespace blender::xpbd
