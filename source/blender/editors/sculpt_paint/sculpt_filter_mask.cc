/* SPDX-FileCopyrightText: 2020 Blender Authors
 *
 * SPDX-License-Identifier: GPL-2.0-or-later */

/** \file
 * \ingroup edsculpt
 */

#include "BKE_context.hh"
#include "BKE_layer.hh"
#include "BKE_paint.hh"
#include "BKE_pbvh_api.hh"

#include "WM_api.hh"
#include "WM_types.hh"

#include "paint_intern.hh"
#include "sculpt_intern.hh"

#include "RNA_access.hh"
#include "RNA_define.hh"

#include "bmesh.hh"

#include <cmath>
#include <cstdlib>

namespace blender::ed::sculpt_paint::mask {

enum class FilterType {
  Smooth = 0,
  Sharpen = 1,
  Grow = 2,
  Shrink = 3,
  ContrastIncrease = 5,
  ContrastDecrease = 6,
};

static EnumPropertyItem prop_mask_filter_types[] = {
    {int(FilterType::Smooth), "SMOOTH", 0, "Smooth Mask", ""},
    {int(FilterType::Sharpen), "SHARPEN", 0, "Sharpen Mask", ""},
    {int(FilterType::Grow), "GROW", 0, "Grow Mask", ""},
    {int(FilterType::Shrink), "SHRINK", 0, "Shrink Mask", ""},
    {int(FilterType::ContrastIncrease), "CONTRAST_INCREASE", 0, "Increase Contrast", ""},
    {int(FilterType::ContrastDecrease), "CONTRAST_DECREASE", 0, "Decrease Contrast", ""},
    {0, nullptr, 0, nullptr, nullptr},
};

static void mask_filter_task(SculptSession &ss,
                             const FilterType mode,
                             const Span<float> prev_mask,
                             const SculptMaskWriteInfo mask_write,
                             PBVHNode *node)
{
  bool update = false;

  float contrast = 0.0f;

  PBVHVertexIter vd;

  if (mode == FilterType::ContrastIncrease) {
    contrast = 0.1f;
  }

  if (mode == FilterType::ContrastDecrease) {
    contrast = -0.1f;
  }

  BKE_pbvh_vertex_iter_begin (*ss.pbvh, node, vd, PBVH_ITER_UNIQUE) {
    float delta, gain, offset, max, min;

    float mask = vd.mask;
    SculptVertexNeighborIter ni;
    switch (mode) {
      case FilterType::Smooth:
      case FilterType::Sharpen: {
        float val = smooth::neighbor_mask_average(ss, mask_write, vd.vertex);

        val -= mask;

        if (mode == FilterType::Smooth) {
          mask += val;
        }
        else if (mode == FilterType::Sharpen) {
          if (mask > 0.5f) {
            mask += 0.05f;
          }
          else {
            mask -= 0.05f;
          }
          mask += val / 2.0f;
        }
        break;
      }
      case FilterType::Grow:
        max = 0.0f;
        SCULPT_VERTEX_NEIGHBORS_ITER_BEGIN (ss, vd.vertex, ni) {
          float vmask_f = prev_mask[ni.index];
          if (vmask_f > max) {
            max = vmask_f;
          }
        }
        SCULPT_VERTEX_NEIGHBORS_ITER_END(ni);
        mask = max;
        break;
      case FilterType::Shrink:
        min = 1.0f;
        SCULPT_VERTEX_NEIGHBORS_ITER_BEGIN (ss, vd.vertex, ni) {
          float vmask_f = prev_mask[ni.index];
          if (vmask_f < min) {
            min = vmask_f;
          }
        }
        SCULPT_VERTEX_NEIGHBORS_ITER_END(ni);
        mask = min;
        break;
      case FilterType::ContrastIncrease:
      case FilterType::ContrastDecrease:
        delta = contrast / 2.0f;
        gain = 1.0f - delta * 2.0f;
        if (contrast > 0) {
          gain = 1.0f / ((gain != 0.0f) ? gain : FLT_EPSILON);
          offset = gain * (-delta);
        }
        else {
          delta *= -1.0f;
          offset = gain * (delta);
        }
        mask = gain * (mask) + offset;
        break;
    }
    mask = clamp_f(mask, 0.0f, 1.0f);
    if (mask != vd.mask) {
      SCULPT_mask_vert_set(BKE_pbvh_type(*ss.pbvh), mask_write, mask, vd);
      update = true;
    }
  }
  BKE_pbvh_vertex_iter_end;

  if (update) {
    BKE_pbvh_node_mark_update_mask(node);
  }
}

static int sculpt_mask_filter_exec(bContext *C, wmOperator *op)
{
  Object &ob = *CTX_data_active_object(C);
  Depsgraph *depsgraph = CTX_data_depsgraph_pointer(C);
  const Scene *scene = CTX_data_scene(C);
  const FilterType filter_type = FilterType(RNA_enum_get(op->ptr, "filter_type"));

  const View3D *v3d = CTX_wm_view3d(C);
  const Base *base = CTX_data_active_base(C);
  if (!BKE_base_is_visible(v3d, base)) {
    return OPERATOR_CANCELLED;
  }

  MultiresModifierData *mmd = BKE_sculpt_multires_active(scene, &ob);
  BKE_sculpt_mask_layers_ensure(CTX_data_depsgraph_pointer(C), CTX_data_main(C), &ob, mmd);

  BKE_sculpt_update_object_for_edit(depsgraph, &ob, false);

  SculptSession &ss = *ob.sculpt;
  PBVH &pbvh = *ob.sculpt->pbvh;

  SCULPT_vertex_random_access_ensure(ss);

  int num_verts = SCULPT_vertex_count_get(ss);

  Vector<PBVHNode *> nodes = bke::pbvh::search_gather(pbvh, {});
  undo::push_begin(ob, op);

  for (PBVHNode *node : nodes) {
    undo::push_node(ob, node, undo::Type::Mask);
  }

  Array<float> prev_mask;
  int iterations = RNA_int_get(op->ptr, "iterations");

  /* Auto iteration count calculates the number of iteration based on the vertices of the mesh to
   * avoid adding an unnecessary amount of undo steps when using the operator from a shortcut.
   * One iteration per 50000 vertices in the mesh should be fine in most cases.
   * Maybe we want this to be configurable. */
  if (RNA_boolean_get(op->ptr, "auto_iteration_count")) {
    iterations = int(num_verts / 50000.0f) + 1;
  }

  const SculptMaskWriteInfo mask_write = SCULPT_mask_get_for_write(ss);

  for (int i = 0; i < iterations; i++) {
    if (ELEM(filter_type, FilterType::Grow, FilterType::Shrink)) {
      prev_mask = duplicate_mask(ob);
    }

    threading::parallel_for(nodes.index_range(), 1, [&](const IndexRange range) {
      for (const int i : range) {
        mask_filter_task(ss, filter_type, prev_mask, mask_write, nodes[i]);
      }
    });
  }

  undo::push_end(ob);

  SCULPT_tag_update_overlays(C);

  return OPERATOR_FINISHED;
}

void SCULPT_OT_mask_filter(wmOperatorType *ot)
{
  ot->name = "Mask Filter";
  ot->idname = "SCULPT_OT_mask_filter";
  ot->description = "Applies a filter to modify the current mask";

  ot->exec = sculpt_mask_filter_exec;
  ot->poll = SCULPT_mode_poll;

  ot->flag = OPTYPE_REGISTER | OPTYPE_UNDO;

  RNA_def_enum(ot->srna,
               "filter_type",
               prop_mask_filter_types,
               int(FilterType::Smooth),
               "Type",
               "Filter that is going to be applied to the mask");
  RNA_def_int(ot->srna,
              "iterations",
              1,
              1,
              100,
              "Iterations",
              "Number of times that the filter is going to be applied",
              1,
              100);
  RNA_def_boolean(
      ot->srna,
      "auto_iteration_count",
      true,
      "Auto Iteration Count",
      "Use a automatic number of iterations based on the number of vertices of the sculpt");
}

}  // namespace blender::ed::sculpt_paint::mask
