/* SPDX-FileCopyrightText: 2020 Blender Foundation
 *
 * SPDX-License-Identifier: GPL-2.0-or-later */

/** \file
 * \ingroup edsculpt
 */

#include "MEM_guardedalloc.h"

#include "BLI_math.h"
#include "BLI_task.h"

#include "DNA_meshdata_types.h"
#include "DNA_modifier_types.h"

#include "BKE_context.h"
#include "BKE_paint.hh"
#include "BKE_pbvh_api.hh"
#include "BKE_scene.h"

#include "DEG_depsgraph.h"

#include "WM_api.h"
#include "WM_types.h"

#include "sculpt_intern.hh"

#include "RNA_access.h"
#include "RNA_define.h"

#include "bmesh.h"

#include <cmath>
#include <cstdlib>

enum eSculptMaskFilterTypes {
  MASK_FILTER_SMOOTH = 0,
  MASK_FILTER_SHARPEN = 1,
  MASK_FILTER_GROW = 2,
  MASK_FILTER_SHRINK = 3,
  MASK_FILTER_CONTRAST_INCREASE = 5,
  MASK_FILTER_CONTRAST_DECREASE = 6,
};

static EnumPropertyItem prop_mask_filter_types[] = {
    {MASK_FILTER_SMOOTH, "SMOOTH", 0, "Smooth Mask", "Smooth mask"},
    {MASK_FILTER_SHARPEN, "SHARPEN", 0, "Sharpen Mask", "Sharpen mask"},
    {MASK_FILTER_GROW, "GROW", 0, "Grow Mask", "Grow mask"},
    {MASK_FILTER_SHRINK, "SHRINK", 0, "Shrink Mask", "Shrink mask"},
    {MASK_FILTER_CONTRAST_INCREASE,
     "CONTRAST_INCREASE",
     0,
     "Increase Contrast",
     "Increase the contrast of the paint mask"},
    {MASK_FILTER_CONTRAST_DECREASE,
     "CONTRAST_DECREASE",
     0,
     "Decrease Contrast",
     "Decrease the contrast of the paint mask"},
    {0, nullptr, 0, nullptr, nullptr},
};

static void mask_filter_task_cb(void *__restrict userdata,
                                const int i,
                                const TaskParallelTLS *__restrict /*tls*/)
{
  SculptThreadedTaskData *data = static_cast<SculptThreadedTaskData *>(userdata);
  SculptSession *ss = data->ob->sculpt;
  PBVHNode *node = data->nodes[i];
  bool update = false;

  const int mode = data->filter_type;
  float contrast = 0.0f;

  PBVHVertexIter vd;

  if (mode == MASK_FILTER_CONTRAST_INCREASE) {
    contrast = 0.1f;
  }

  if (mode == MASK_FILTER_CONTRAST_DECREASE) {
    contrast = -0.1f;
  }

  BKE_pbvh_vertex_iter_begin (ss->pbvh, node, vd, PBVH_ITER_UNIQUE) {
    float delta, gain, offset, max, min;
    float prev_val = *vd.mask;
    SculptVertexNeighborIter ni;
    switch (mode) {
      case MASK_FILTER_SMOOTH:
      case MASK_FILTER_SHARPEN: {
        float val = SCULPT_neighbor_mask_average(ss, vd.vertex);

        val -= *vd.mask;

        if (mode == MASK_FILTER_SMOOTH) {
          *vd.mask += val;
        }
        else if (mode == MASK_FILTER_SHARPEN) {
          if (*vd.mask > 0.5f) {
            *vd.mask += 0.05f;
          }
          else {
            *vd.mask -= 0.05f;
          }
          *vd.mask += val / 2.0f;
        }
        break;
      }
      case MASK_FILTER_GROW:
        max = 0.0f;
        SCULPT_VERTEX_NEIGHBORS_ITER_BEGIN (ss, vd.vertex, ni) {
          float vmask_f = data->prev_mask[ni.index];
          if (vmask_f > max) {
            max = vmask_f;
          }
        }
        SCULPT_VERTEX_NEIGHBORS_ITER_END(ni);
        *vd.mask = max;
        break;
      case MASK_FILTER_SHRINK:
        min = 1.0f;
        SCULPT_VERTEX_NEIGHBORS_ITER_BEGIN (ss, vd.vertex, ni) {
          float vmask_f = data->prev_mask[ni.index];
          if (vmask_f < min) {
            min = vmask_f;
          }
        }
        SCULPT_VERTEX_NEIGHBORS_ITER_END(ni);
        *vd.mask = min;
        break;
      case MASK_FILTER_CONTRAST_INCREASE:
      case MASK_FILTER_CONTRAST_DECREASE:
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
        *vd.mask = gain * (*vd.mask) + offset;
        break;
    }
    *vd.mask = clamp_f(*vd.mask, 0.0f, 1.0f);
    if (*vd.mask != prev_val) {
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
  Object *ob = CTX_data_active_object(C);
  Depsgraph *depsgraph = CTX_data_depsgraph_pointer(C);
  const Scene *scene = CTX_data_scene(C);
  Sculpt *sd = CTX_data_tool_settings(C)->sculpt;
  int filter_type = RNA_enum_get(op->ptr, "filter_type");

  MultiresModifierData *mmd = BKE_sculpt_multires_active(scene, ob);
  BKE_sculpt_mask_layers_ensure(CTX_data_depsgraph_pointer(C), CTX_data_main(C), ob, mmd);

  BKE_sculpt_update_object_for_edit(depsgraph, ob, true, true, false);

  SculptSession *ss = ob->sculpt;
  PBVH *pbvh = ob->sculpt->pbvh;

  SCULPT_vertex_random_access_ensure(ss);

  int num_verts = SCULPT_vertex_count_get(ss);

  Vector<PBVHNode *> nodes = blender::bke::pbvh::search_gather(pbvh, nullptr, nullptr);
  SCULPT_undo_push_begin(ob, op);

  for (PBVHNode *node : nodes) {
    SCULPT_undo_push_node(ob, node, SCULPT_UNDO_MASK);
  }

  float *prev_mask = nullptr;
  int iterations = RNA_int_get(op->ptr, "iterations");

  /* Auto iteration count calculates the number of iteration based on the vertices of the mesh to
   * avoid adding an unnecessary amount of undo steps when using the operator from a shortcut.
   * One iteration per 50000 vertices in the mesh should be fine in most cases.
   * Maybe we want this to be configurable. */
  if (RNA_boolean_get(op->ptr, "auto_iteration_count")) {
    iterations = int(num_verts / 50000.0f) + 1;
  }

  for (int i = 0; i < iterations; i++) {
    if (ELEM(filter_type, MASK_FILTER_GROW, MASK_FILTER_SHRINK)) {
      prev_mask = static_cast<float *>(MEM_mallocN(num_verts * sizeof(float), __func__));
      for (int j = 0; j < num_verts; j++) {
        PBVHVertRef vertex = BKE_pbvh_index_to_vertex(ss->pbvh, j);
        prev_mask[j] = SCULPT_vertex_mask_get(ss, vertex);
      }
    }

    SculptThreadedTaskData data{};
    data.sd = sd;
    data.ob = ob;
    data.nodes = nodes;
    data.filter_type = filter_type;
    data.prev_mask = prev_mask;

    TaskParallelSettings settings;
    BKE_pbvh_parallel_range_settings(&settings, true, nodes.size());
    BLI_task_parallel_range(0, nodes.size(), &data, mask_filter_task_cb, &settings);

    if (ELEM(filter_type, MASK_FILTER_GROW, MASK_FILTER_SHRINK)) {
      MEM_freeN(prev_mask);
    }
  }

  SCULPT_undo_push_end(ob);

  SCULPT_tag_update_overlays(C);

  return OPERATOR_FINISHED;
}

void SCULPT_mask_filter_smooth_apply(Sculpt *sd,
                                     Object *ob,
                                     Span<PBVHNode *> nodes,
                                     const int smooth_iterations)
{
  SculptThreadedTaskData data{};
  data.sd = sd;
  data.ob = ob;
  data.nodes = nodes;
  data.filter_type = MASK_FILTER_SMOOTH;

  for (int i = 0; i < smooth_iterations; i++) {
    TaskParallelSettings settings;
    BKE_pbvh_parallel_range_settings(&settings, true, nodes.size());
    BLI_task_parallel_range(0, nodes.size(), &data, mask_filter_task_cb, &settings);
  }
}

void SCULPT_OT_mask_filter(wmOperatorType *ot)
{
  /* Identifiers. */
  ot->name = "Mask Filter";
  ot->idname = "SCULPT_OT_mask_filter";
  ot->description = "Applies a filter to modify the current mask";

  /* API callbacks. */
  ot->exec = sculpt_mask_filter_exec;
  ot->poll = SCULPT_mode_poll;

  ot->flag = OPTYPE_REGISTER | OPTYPE_UNDO;

  /* RNA. */
  RNA_def_enum(ot->srna,
               "filter_type",
               prop_mask_filter_types,
               MASK_FILTER_SMOOTH,
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
