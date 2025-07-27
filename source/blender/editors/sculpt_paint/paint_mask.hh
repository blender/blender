/* SPDX-FileCopyrightText: 2024 Blender Authors
 *
 * SPDX-License-Identifier: GPL-2.0-or-later */

/** \file
 * \ingroup edsculpt
 */

#pragma once

#include "BLI_array.hh"
#include "BLI_function_ref.hh"
#include "BLI_index_mask_fwd.hh"
#include "BLI_set.hh"

struct BMesh;
struct BMVert;
struct CCGKey;
struct Depsgraph;
struct Object;
struct SubdivCCG;
struct wmOperatorType;
namespace blender::bke::pbvh {
class Node;
}

namespace blender::ed::sculpt_paint::mask {

Array<float> duplicate_mask(const Object &object);
void mix_new_masks(Span<float> new_masks, float factor, MutableSpan<float> masks);
void mix_new_masks(Span<float> new_masks, Span<float> factors, MutableSpan<float> masks);
void clamp_mask(MutableSpan<float> masks);
void invert_mask(MutableSpan<float> masks);

void gather_mask_grids(const SubdivCCG &subdiv_ccg, Span<int> grids, MutableSpan<float> r_mask);
void gather_mask_bmesh(const BMesh &bm, const Set<BMVert *, 0> &verts, MutableSpan<float> r_mask);

void scatter_mask_grids(Span<float> mask, SubdivCCG &subdiv_ccg, Span<int> grids);
void scatter_mask_bmesh(Span<float> mask, const BMesh &bm, const Set<BMVert *, 0> &verts);

void average_neighbor_mask_bmesh(int mask_offset,
                                 const Set<BMVert *, 0> &verts,
                                 MutableSpan<float> new_masks);

/** Write to the mask attribute for each node, storing undo data. */
void write_mask_mesh(const Depsgraph &depsgraph,
                     Object &object,
                     const IndexMask &node_mask,
                     FunctionRef<void(MutableSpan<float>, Span<int>)> write_fn);

/**
 * Write to each node's mask data for visible vertices. Store undo data and mark for redraw only
 * if the data is actually changed.
 */
void update_mask_mesh(const Depsgraph &depsgraph,
                      Object &object,
                      const IndexMask &node_mask,
                      FunctionRef<void(MutableSpan<float>, Span<int>)> update_fn);

/** Check whether array data is the same as the stored mask for the referenced geometry. */
bool mask_equals_array_grids(Span<float> masks,
                             const CCGKey &key,
                             Span<int> grids,
                             Span<float> values);
bool mask_equals_array_bmesh(int mask_offset, const Set<BMVert *, 0> &verts, Span<float> values);

void PAINT_OT_mask_flood_fill(wmOperatorType *ot);
void PAINT_OT_mask_lasso_gesture(wmOperatorType *ot);
void PAINT_OT_mask_box_gesture(wmOperatorType *ot);
void PAINT_OT_mask_line_gesture(wmOperatorType *ot);
void PAINT_OT_mask_polyline_gesture(wmOperatorType *ot);
}  // namespace blender::ed::sculpt_paint::mask
