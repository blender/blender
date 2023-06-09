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
#include "BKE_paint.h"
#include "BKE_pbvh.h"
#include "BKE_scene.h"

#include "DEG_depsgraph.h"

#include "WM_api.h"
#include "WM_types.h"

#include "sculpt_intern.hh"

#include "RNA_access.h"
#include "RNA_define.h"

#include "UI_interface.h"

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

/******************************************************************************************/
/* Interactive Preview Mask Filter */

#define SCULPT_IPMASK_FILTER_MIN_MULTITHREAD 1000
#define SCULPT_IPMASK_FILTER_GRANULARITY 100

#define SCULPT_IPMASK_FILTER_QUANTIZE_STEP 0.1

enum eSculptIPMaskFilterType {
  IPMASK_FILTER_SMOOTH_SHARPEN,
  IPMASK_FILTER_GROW_SHRINK,
  IPMASK_FILTER_HARDER_SOFTER,
  IPMASK_FILTER_CONTRAST,
  IPMASK_FILTER_ADD_SUBSTRACT,
  IPMASK_FILTER_INVERT,
  IPMASK_FILTER_QUANTIZE,
};
ENUM_OPERATORS(eSculptIPMaskFilterType, IPMASK_FILTER_QUANTIZE);

enum MaskFilterStepDirectionType {
  MASK_FILTER_STEP_DIRECTION_FORWARD,
  MASK_FILTER_STEP_DIRECTION_BACKWARD,
};
ENUM_OPERATORS(MaskFilterStepDirectionType, MASK_FILTER_STEP_DIRECTION_BACKWARD);

/* Grown/Shrink vertex callbacks. */
static float sculpt_ipmask_vertex_grow_cb(SculptSession *ss,
                                          const PBVHVertRef vertex,
                                          float *current_mask)
{
  float max = 0.0f;
  SculptVertexNeighborIter ni;
  SCULPT_VERTEX_NEIGHBORS_ITER_BEGIN (ss, vertex, ni) {
    float vmask_f = current_mask[ni.index];
    if (vmask_f > max) {
      max = vmask_f;
    }
  }
  SCULPT_VERTEX_NEIGHBORS_ITER_END(ni);
  return max;
}

static float sculpt_ipmask_vertex_shrink_cb(SculptSession *ss,
                                            const PBVHVertRef vertex,
                                            float *current_mask)
{
  float min = 1.0f;
  SculptVertexNeighborIter ni;
  SCULPT_VERTEX_NEIGHBORS_ITER_BEGIN (ss, vertex, ni) {
    float vmask_f = current_mask[ni.index];
    if (vmask_f < min) {
      min = vmask_f;
    }
  }
  SCULPT_VERTEX_NEIGHBORS_ITER_END(ni);
  return min;
}

/* Smooth/Sharpen vertex callbacks. */
static float sculpt_ipmask_vertex_smooth_cb(SculptSession *ss,
                                            const PBVHVertRef vertex,
                                            float *current_mask)
{
  int vertex_i = BKE_pbvh_vertex_to_index(ss->pbvh, vertex);

  float accum = current_mask[vertex_i];
  int total = 1;
  SculptVertexNeighborIter ni;
  SCULPT_VERTEX_NEIGHBORS_ITER_BEGIN (ss, vertex, ni) {
    accum += current_mask[ni.index];
    total++;
  }
  SCULPT_VERTEX_NEIGHBORS_ITER_END(ni);
  return total > 0 ? accum / total : current_mask[vertex_i];
}

static float sculpt_ipmask_vertex_sharpen_cb(SculptSession *ss,
                                             const PBVHVertRef vertex,
                                             float *current_mask)
{
  int vertex_i = BKE_pbvh_vertex_to_index(ss->pbvh, vertex);

  float accum = 0.0f;
  int total = 0;
  float vmask = current_mask[vertex_i];
  SculptVertexNeighborIter ni;
  SCULPT_VERTEX_NEIGHBORS_ITER_BEGIN (ss, vertex, ni) {
    accum += current_mask[ni.index];
    total++;
  }
  SCULPT_VERTEX_NEIGHBORS_ITER_END(ni);
  const float avg = total > 0 ? accum / total : current_mask[vertex_i];
  const float val = avg - vmask;

  float new_mask;
  if (vmask > 0.5f) {
    new_mask = vmask + 0.03f;
  }
  else {
    new_mask = vmask - 0.03f;
  }
  new_mask += val / 2.0f;
  return clamp_f(new_mask, 0.0f, 1.0f);

#ifdef SHARP_KERNEL
  float accum = 0.0f;
  float weight_accum = 0.0f;
  const float neighbor_weight = -1.0f;
  int neighbor_count = 0;

  SculptVertexNeighborIter ni;
  SCULPT_VERTEX_NEIGHBORS_ITER_BEGIN (ss, vertex, ni) {
    accum += neighbor_weight * current_mask[ni.index];
    weight_accum += neighbor_weight;
    neighbor_count++;
  }
  SCULPT_VERTEX_NEIGHBORS_ITER_END(ni);

  const float main_weight = (neighbor_count + 1) * 2.0f;
  accum += main_weight * current_mask[vertex];
  weight_accum += main_weight;

  return clamp_f(accum / weight_accum, 0.0f, 1.0f);
#endif
}

/* Harder/Softer callbacks. */
#define SCULPT_IPMASK_FILTER_HARDER_SOFTER_STEP 0.01f
static float sculpt_ipmask_vertex_harder_cb(SculptSession *ss,
                                            const PBVHVertRef vertex,
                                            float *current_mask)
{
  int vertex_i = BKE_pbvh_vertex_to_index(ss->pbvh, vertex);

  return clamp_f(current_mask[vertex_i] += current_mask[vertex_i] *
                                           SCULPT_IPMASK_FILTER_HARDER_SOFTER_STEP,
                 0.0f,
                 1.0f);
}

static float sculpt_ipmask_vertex_softer_cb(SculptSession *ss,
                                            const PBVHVertRef vertex,
                                            float *current_mask)
{
  int vertex_i = BKE_pbvh_vertex_to_index(ss->pbvh, vertex);

  return clamp_f(current_mask[vertex_i] -= current_mask[vertex_i] *
                                           SCULPT_IPMASK_FILTER_HARDER_SOFTER_STEP,
                 0.0f,
                 1.0f);
}

/* Contrast Increase/Decrease callbacks. */

#define SCULPT_IPMASK_FILTER_CONTRAST_STEP 0.05f
static float sculpt_ipmask_filter_contrast(const float mask, const float contrast)
{
  float offset;
  float delta = contrast / 2.0f;
  float gain = 1.0f - delta * 2.0f;
  if (contrast > 0.0f) {
    gain = 1.0f / ((gain != 0.0f) ? gain : FLT_EPSILON);
    offset = gain * (-delta);
  }
  else {
    delta *= -1.0f;
    offset = gain * (delta);
  }
  return clamp_f(gain * mask + offset, 0.0f, 1.0f);
}

static float sculpt_ipmask_vertex_contrast_increase_cb(SculptSession *ss,
                                                       const PBVHVertRef vertex,
                                                       float *current_mask)
{
  int vertex_i = BKE_pbvh_vertex_to_index(ss->pbvh, vertex);

  return sculpt_ipmask_filter_contrast(current_mask[vertex_i], SCULPT_IPMASK_FILTER_CONTRAST_STEP);
}

static float sculpt_ipmask_vertex_contrast_decrease_cb(SculptSession *ss,
                                                       const PBVHVertRef vertex,
                                                       float *current_mask)
{
  int vertex_i = BKE_pbvh_vertex_to_index(ss->pbvh, vertex);

  return sculpt_ipmask_filter_contrast(current_mask[vertex_i],
                                       -1.0f * SCULPT_IPMASK_FILTER_CONTRAST_STEP);
}

static MaskFilterDeltaStep *sculpt_ipmask_filter_delta_create(const float *current_mask,
                                                              const float *next_mask,
                                                              const int totvert)
{
  int tot_modified_values = 0;
  for (int i = 0; i < totvert; i++) {
    if (current_mask[i] == next_mask[i]) {
      continue;
    }
    tot_modified_values++;
  }

  MaskFilterDeltaStep *delta_step = MEM_cnew<MaskFilterDeltaStep>("mask filter delta step");
  delta_step->totelem = tot_modified_values;
  delta_step->index = MEM_cnew_array<int>(tot_modified_values, "delta indices");
  delta_step->delta = MEM_cnew_array<float>(tot_modified_values, "delta values");

  int delta_step_index = 0;
  for (int i = 0; i < totvert; i++) {
    if (current_mask[i] == next_mask[i]) {
      continue;
    }
    delta_step->index[delta_step_index] = i;
    delta_step->delta[delta_step_index] = next_mask[i] - current_mask[i];
    delta_step_index++;
  }
  return delta_step;
}

typedef struct SculptIPMaskFilterTaskData {
  SculptSession *ss;
  float *next_mask;
  float *current_mask;
  MaskFilterStepDirectionType direction;
} SculptIPMaskFilterTaskData;

static void ipmask_filter_compute_step_task_cb(void *__restrict userdata,
                                               const int i,
                                               const TaskParallelTLS *__restrict /* tls */)
{
  SculptIPMaskFilterTaskData *data = static_cast<SculptIPMaskFilterTaskData *>(userdata);
  PBVHVertRef vertex = BKE_pbvh_index_to_vertex(data->ss->pbvh, i);

  if (data->direction == MASK_FILTER_STEP_DIRECTION_FORWARD) {
    data->next_mask[i] = data->ss->filter_cache->mask_filter_step_forward(
        data->ss, vertex, data->current_mask);
  }
  else {
    data->next_mask[i] = data->ss->filter_cache->mask_filter_step_backward(
        data->ss, vertex, data->current_mask);
  }
}

static float *sculpt_ipmask_step_compute(SculptSession *ss,
                                         float *current_mask,
                                         MaskFilterStepDirectionType direction)
{
  const int totvert = SCULPT_vertex_count_get(ss);
  float *next_mask = MEM_cnew_array<float>(totvert, "delta values");

  SculptIPMaskFilterTaskData data = {};
  data.ss = ss;
  data.next_mask = next_mask;
  data.current_mask = current_mask;
  data.direction = direction;

  TaskParallelSettings settings;
  memset(&settings, 0, sizeof(TaskParallelSettings));
  settings.use_threading = totvert > SCULPT_IPMASK_FILTER_MIN_MULTITHREAD;
  settings.min_iter_per_thread = SCULPT_IPMASK_FILTER_GRANULARITY;
  BLI_task_parallel_range(0, totvert, &data, ipmask_filter_compute_step_task_cb, &settings);

  return next_mask;
}

static float *sculpt_ipmask_current_state_get(SculptSession *ss)
{
  return static_cast<float *>(MEM_dupallocN(ss->filter_cache->mask_filter_ref));
}

static void sculpt_ipmask_reference_set(SculptSession *ss, float *new_mask)
{
  const int totvert = SCULPT_vertex_count_get(ss);
  for (int i = 0; i < totvert; i++) {
    ss->filter_cache->mask_filter_ref[i] = new_mask[i];
  }
}

static void sculpt_ipmask_store_reference_step(SculptSession *ss)
{
  const int totvert = SCULPT_vertex_count_get(ss);
  if (!ss->filter_cache->mask_filter_ref) {
    ss->filter_cache->mask_filter_ref = MEM_cnew_array<float>(totvert, "delta values");
  }

  for (int i = 0; i < totvert; i++) {
    PBVHVertRef vertex = BKE_pbvh_index_to_vertex(ss->pbvh, i);

    ss->filter_cache->mask_filter_ref[i] = SCULPT_vertex_mask_get(ss, vertex);
  }
}

static void ipmask_filter_apply_task_cb(void *__restrict userdata,
                                        const int i,
                                        const TaskParallelTLS *__restrict /* tls */)
{
  SculptThreadedTaskData *data = static_cast<SculptThreadedTaskData *>(userdata);
  SculptSession *ss = data->ss;
  FilterCache *filter_cache = ss->filter_cache;
  PBVHNode *node = filter_cache->nodes[i];
  PBVHVertexIter vd;
  bool update = false;

  AutomaskingNodeData automask_data;
  SCULPT_automasking_node_begin(data->ob, ss, filter_cache->automasking, &automask_data, node);

  BKE_pbvh_vertex_iter_begin (ss->pbvh, node, vd, PBVH_ITER_UNIQUE) {
    SCULPT_automasking_node_update(ss, &automask_data, &vd);

    if (SCULPT_automasking_factor_get(filter_cache->automasking, ss, vd.vertex, &automask_data) <
        0.5f) {
      continue;
    }

    float new_mask;
    if (data->next_mask) {
      new_mask = interpf(
          data->next_mask[vd.index], data->new_mask[vd.index], data->mask_interpolation);
    }
    else {
      new_mask = data->new_mask[vd.index];
    }

    if (*vd.mask == new_mask) {
      continue;
    }

    *vd.mask = new_mask;
    update = true;
    if (vd.is_mesh) {
      BKE_pbvh_vert_tag_update_normal(ss->pbvh, vd.vertex);
    }
  }
  BKE_pbvh_vertex_iter_end;

  if (update) {
    BKE_pbvh_node_mark_redraw(node);
  }
}

static void sculpt_ipmask_apply_mask_data(SculptSession *ss,
                                          float *new_mask,
                                          float *next_mask,
                                          const float interpolation)
{
  FilterCache *filter_cache = ss->filter_cache;
  SculptThreadedTaskData data = {};
  data.ss = ss;
  data.nodes = filter_cache->nodes;
  data.new_mask = new_mask;
  data.next_mask = next_mask;
  data.mask_interpolation = interpolation;

  TaskParallelSettings settings;
  BKE_pbvh_parallel_range_settings(&settings, true, filter_cache->nodes.size());
  BLI_task_parallel_range(
      0, filter_cache->nodes.size(), &data, ipmask_filter_apply_task_cb, &settings);
}

static float *sculpt_ipmask_apply_delta_step(MaskFilterDeltaStep *delta_step,
                                             const float *current_mask,
                                             const MaskFilterStepDirectionType direction)
{
  float *next_mask = static_cast<float *>(MEM_dupallocN(current_mask));
  for (int i = 0; i < delta_step->totelem; i++) {
    if (direction == MASK_FILTER_STEP_DIRECTION_FORWARD) {
      next_mask[delta_step->index[i]] = current_mask[delta_step->index[i]] + delta_step->delta[i];
    }
    else {
      next_mask[delta_step->index[i]] = current_mask[delta_step->index[i]] - delta_step->delta[i];
    }
  }
  return next_mask;
}

static float *sculpt_ipmask_restore_state_from_delta(SculptSession *ss,
                                                     MaskFilterDeltaStep *delta_step,
                                                     MaskFilterStepDirectionType direction)
{
  float *current_mask = sculpt_ipmask_current_state_get(ss);
  float *next_mask = sculpt_ipmask_apply_delta_step(delta_step, current_mask, direction);
  MEM_freeN(current_mask);
  return next_mask;
}

static float *sculpt_ipmask_compute_and_store_step(SculptSession *ss,
                                                   const int iterations,
                                                   const int delta_index,
                                                   MaskFilterStepDirectionType direction)
{
  BLI_assert(iterations > 0);
  const int totvert = SCULPT_vertex_count_get(ss);
  float *current_mask = sculpt_ipmask_current_state_get(ss);
  float *original_mask = static_cast<float *>(MEM_dupallocN(current_mask));
  float *next_mask = nullptr;

  /* Compute the filter. */
  for (int i = 0; i < iterations; i++) {
    MEM_SAFE_FREE(next_mask);
    next_mask = sculpt_ipmask_step_compute(ss, current_mask, direction);
    MEM_freeN(current_mask);
    current_mask = static_cast<float *>(MEM_dupallocN(next_mask));
  }
  MEM_freeN(current_mask);

  /* Pack and store the delta step. */
  MaskFilterDeltaStep *delta_step;
  if (direction == MASK_FILTER_STEP_DIRECTION_FORWARD) {
    delta_step = sculpt_ipmask_filter_delta_create(original_mask, next_mask, totvert);
  }
  else {
    delta_step = sculpt_ipmask_filter_delta_create(next_mask, original_mask, totvert);
  }
  BLI_ghash_insert(ss->filter_cache->mask_delta_step, POINTER_FROM_INT(delta_index), delta_step);
  MEM_freeN(original_mask);

  return next_mask;
}

static float *sculpt_ipmask_filter_mask_for_step_get(SculptSession *ss,
                                                     MaskFilterStepDirectionType direction,
                                                     const int iteration_count)
{
  FilterCache *filter_cache = ss->filter_cache;
  int next_step = filter_cache->mask_filter_current_step;
  int delta_index = next_step;
  /* Get the next step and the delta step index associated with it. */
  if (direction == MASK_FILTER_STEP_DIRECTION_FORWARD) {
    next_step = filter_cache->mask_filter_current_step + 1;
    delta_index = filter_cache->mask_filter_current_step;
  }
  else {
    next_step = filter_cache->mask_filter_current_step - 1;
    delta_index = filter_cache->mask_filter_current_step - 1;
  }

  /* Update the data one step forward/backward. */
  if (BLI_ghash_haskey(filter_cache->mask_delta_step, POINTER_FROM_INT(delta_index))) {
    /* This step was already computed, restore it from the current step and a delta. */
    MaskFilterDeltaStep *delta_step = static_cast<MaskFilterDeltaStep *>(
        BLI_ghash_lookup(filter_cache->mask_delta_step, POINTER_FROM_INT(delta_index)));
    return sculpt_ipmask_restore_state_from_delta(ss, delta_step, direction);
  }

  /* New step that was not yet computed. Compute and store the delta. */
  return sculpt_ipmask_compute_and_store_step(ss, iteration_count, delta_index, direction);
}

static void sculpt_ipmask_filter_update_to_target_step(SculptSession *ss,
                                                       const int target_step,
                                                       const int iteration_count,
                                                       const float step_interpolation)
{
  FilterCache *filter_cache = ss->filter_cache;

  MaskFilterStepDirectionType direction;
  /* Get the next step and the delta step index associated with it. */
  if (target_step > filter_cache->mask_filter_current_step) {
    direction = MASK_FILTER_STEP_DIRECTION_FORWARD;
  }
  else {
    direction = MASK_FILTER_STEP_DIRECTION_BACKWARD;
  }

  while (filter_cache->mask_filter_current_step != target_step) {
    /* Restore or compute a mask in the given direction. */
    float *new_mask = sculpt_ipmask_filter_mask_for_step_get(ss, direction, iteration_count);

    /* Store the full step. */
    sculpt_ipmask_reference_set(ss, new_mask);
    MEM_freeN(new_mask);

    /* Update the current step count. */
    if (direction == MASK_FILTER_STEP_DIRECTION_FORWARD) {
      filter_cache->mask_filter_current_step += 1;
    }
    else {
      filter_cache->mask_filter_current_step -= 1;
    }
  }

  if (step_interpolation != 0.0f) {
    float *next_mask = sculpt_ipmask_filter_mask_for_step_get(
        ss, MASK_FILTER_STEP_DIRECTION_FORWARD, iteration_count);
    sculpt_ipmask_apply_mask_data(
        ss, filter_cache->mask_filter_ref, next_mask, step_interpolation);
    MEM_freeN(next_mask);
  }
  else {
    sculpt_ipmask_apply_mask_data(ss, filter_cache->mask_filter_ref, nullptr, 0.0f);
  }
}

static void ipmask_filter_apply_from_original_task_cb(void *__restrict userdata,
                                                      const int i,
                                                      const TaskParallelTLS *__restrict /* tls */)
{
  SculptThreadedTaskData *data = static_cast<SculptThreadedTaskData *>(userdata);
  SculptSession *ss = data->ss;
  FilterCache *filter_cache = ss->filter_cache;
  PBVHNode *node = filter_cache->nodes[i];
  PBVHVertexIter vd;
  SculptOrigVertData orig_data;
  const eSculptIPMaskFilterType filter_type = static_cast<eSculptIPMaskFilterType>(
      data->filter_type);
  bool update = false;

  /* Used for quantize filter. */
  const int steps = data->filter_strength / SCULPT_IPMASK_FILTER_QUANTIZE_STEP;
  if (steps == 0) {
    return;
  }
  const float step_size = 1.0f / steps;

  AutomaskingNodeData automask_data;
  SCULPT_automasking_node_begin(data->ob, ss, filter_cache->automasking, &automask_data, node);

  SCULPT_orig_vert_data_init(&orig_data, data->ob, node, SCULPT_UNDO_COORDS);
  BKE_pbvh_vertex_iter_begin (ss->pbvh, node, vd, PBVH_ITER_UNIQUE) {
    SCULPT_automasking_node_update(ss, &automask_data, &vd);

    if (SCULPT_automasking_factor_get(filter_cache->automasking, ss, vd.vertex, &automask_data) <
        0.5f) {
      continue;
    }
    SCULPT_orig_vert_data_update(ss, &orig_data, vd.vertex);
    float new_mask = orig_data.mask;
    switch (filter_type) {
      case IPMASK_FILTER_ADD_SUBSTRACT:
        new_mask = orig_data.mask + data->filter_strength;
        break;
      case IPMASK_FILTER_INVERT: {
        const float strength = clamp_f(data->filter_strength, 0.0f, 1.0f);
        const float mask_invert = 1.0f - orig_data.mask;
        new_mask = interpf(mask_invert, orig_data.mask, strength);
        break;
      }
      case IPMASK_FILTER_QUANTIZE: {
        const float remainder = fmod(orig_data.mask, step_size);
        const float total_steps = (orig_data.mask - remainder) / step_size;
        new_mask = total_steps * step_size;
        break;
      }
      default:
        BLI_assert(false);
        break;
    }
    new_mask = clamp_f(new_mask, 0.0f, 1.0f);
    if (*vd.mask == new_mask) {
      continue;
    }

    *vd.mask = new_mask;
    update = true;
    if (vd.is_mesh) {
      BKE_pbvh_vert_tag_update_normal(ss->pbvh, vd.vertex);
    }
  }
  BKE_pbvh_vertex_iter_end;

  if (update) {
    BKE_pbvh_node_mark_redraw(node);
  }
}

static void sculpt_ipmask_apply_from_original_mask_data(Object *ob,
                                                        eSculptIPMaskFilterType filter_type,
                                                        const float strength)
{
  SculptSession *ss = ob->sculpt;
  FilterCache *filter_cache = ss->filter_cache;
  SculptThreadedTaskData data;
  data.ob = ob;
  data.ss = ss;
  data.nodes = filter_cache->nodes;
  data.filter_strength = strength;
  data.filter_type = filter_type;

  TaskParallelSettings settings;
  BKE_pbvh_parallel_range_settings(&settings, true, filter_cache->nodes.size());
  BLI_task_parallel_range(
      0, filter_cache->nodes.size(), &data, ipmask_filter_apply_from_original_task_cb, &settings);
}

static bool sculpt_ipmask_filter_uses_apply_from_original(
    const eSculptIPMaskFilterType filter_type)
{
  return ELEM(
      filter_type, IPMASK_FILTER_INVERT, IPMASK_FILTER_ADD_SUBSTRACT, IPMASK_FILTER_QUANTIZE);
}

static void ipmask_filter_restore_original_mask_task_cb(
    void *__restrict userdata, const int i, const TaskParallelTLS *__restrict /* tls */)
{
  SculptThreadedTaskData *data = static_cast<SculptThreadedTaskData *>(userdata);
  SculptSession *ss = data->ss;
  PBVHNode *node = data->nodes[i];
  SculptOrigVertData orig_data;
  bool update = false;
  SCULPT_orig_vert_data_init(&orig_data, data->ob, node, SCULPT_UNDO_COORDS);
  PBVHVertexIter vd;
  BKE_pbvh_vertex_iter_begin (ss->pbvh, node, vd, PBVH_ITER_UNIQUE) {
    SCULPT_orig_vert_data_update(ss, &orig_data, vd.vertex);
    *vd.mask = orig_data.mask;
    update = true;
    if (vd.is_mesh) {
      BKE_pbvh_vert_tag_update_normal(ss->pbvh, vd.vertex);
    }
  }
  BKE_pbvh_vertex_iter_end;

  if (update) {
    BKE_pbvh_node_mark_redraw(node);
  }
}

static void sculpt_ipmask_restore_original_mask(Object *ob)
{
  SculptSession *ss = ob->sculpt;
  FilterCache *filter_cache = ss->filter_cache;
  SculptThreadedTaskData data = {};
  data.ob = ob;
  data.ss = ss;
  data.nodes = filter_cache->nodes;

  TaskParallelSettings settings;
  BKE_pbvh_parallel_range_settings(&settings, true, filter_cache->nodes.size());
  BLI_task_parallel_range(0,
                          filter_cache->nodes.size(),
                          &data,
                          ipmask_filter_restore_original_mask_task_cb,
                          &settings);
}

static void sculpt_ipmask_filter_cancel(bContext *C, wmOperator * /* op */)
{
  Object *ob = CTX_data_active_object(C);
  SculptSession *ss = ob->sculpt;

  sculpt_ipmask_restore_original_mask(ob);
  SCULPT_undo_push_end(ob);
  SCULPT_filter_cache_free(ss, ob);
  SCULPT_flush_update_done(C, ob, SCULPT_UPDATE_MASK);
}

#define IPMASK_FILTER_STEP_SENSITIVITY 0.05f
#define IPMASK_FILTER_STEPS_PER_FULL_STRENGTH 20
static int sculpt_ipmask_filter_modal(bContext *C, wmOperator *op, const wmEvent *event)
{
  Object *ob = CTX_data_active_object(C);
  Depsgraph *depsgraph = CTX_data_depsgraph_pointer(C);
  SculptSession *ss = ob->sculpt;
  FilterCache *filter_cache = ss->filter_cache;
  const eSculptIPMaskFilterType filter_type = (eSculptIPMaskFilterType)RNA_enum_get(op->ptr,
                                                                                    "filter_type");
  const bool use_step_interpolation = RNA_boolean_get(op->ptr, "use_step_interpolation");
  const int iteration_count = RNA_int_get(op->ptr, "iterations");

  if ((event->type == EVT_ESCKEY && event->val == KM_PRESS) ||
      (event->type == RIGHTMOUSE && event->val == KM_PRESS))
  {
    sculpt_ipmask_filter_cancel(C, op);
    return OPERATOR_FINISHED;
  }

  if (ELEM(event->type, LEFTMOUSE, EVT_RETKEY, EVT_PADENTER)) {
    for (int i = 0; i < filter_cache->nodes.size(); i++) {
      BKE_pbvh_node_mark_update_mask(filter_cache->nodes[i]);
    }
    SCULPT_filter_cache_free(ss, ob);
    SCULPT_undo_push_end(ob);
    SCULPT_flush_update_done(C, ob, SCULPT_UPDATE_MASK);
    return OPERATOR_FINISHED;
  }

  if (event->type != MOUSEMOVE) {
    return OPERATOR_RUNNING_MODAL;
  }

  const float len = event->xy[0] - event->prev_press_xy[0];
  const float target_step_fl = len *
                               IPMASK_FILTER_STEP_SENSITIVITY /* TODO: multiply by UI_DPI_FAC */;
  const int target_step = floorf(target_step_fl);
  const float step_interpolation = use_step_interpolation ? target_step_fl - target_step : 0.0f;
  const float full_step_strength = target_step_fl / IPMASK_FILTER_STEPS_PER_FULL_STRENGTH;

  BKE_sculpt_update_object_for_edit(depsgraph, ob, true, true, false);

  if (sculpt_ipmask_filter_uses_apply_from_original(filter_type)) {
    sculpt_ipmask_apply_from_original_mask_data(ob, filter_type, full_step_strength);
  }
  else {
    sculpt_ipmask_filter_update_to_target_step(
        ss, target_step, iteration_count, step_interpolation);
  }

  SCULPT_tag_update_overlays(C);

  return OPERATOR_RUNNING_MODAL;
}

static void sculpt_ipmask_store_initial_undo_step(Object *ob)
{
  SculptSession *ss = ob->sculpt;
  for (int i = 0; i < ss->filter_cache->nodes.size(); i++) {
    SCULPT_undo_push_node(ob, ss->filter_cache->nodes[i], SCULPT_UNDO_MASK);
  }
}

static FilterCache *sculpt_ipmask_filter_cache_init(Object *ob,
                                                    Sculpt *sd,
                                                    const eSculptIPMaskFilterType filter_type,
                                                    const bool init_automasking)
{
  SculptSession *ss = ob->sculpt;
  FilterCache *filter_cache = MEM_new<FilterCache>("filter cache");

  filter_cache->active_face_set = SCULPT_FACE_SET_NONE;
  if (init_automasking) {
    filter_cache->automasking = SCULPT_automasking_cache_init(sd, nullptr, ob);
  }
  filter_cache->mask_filter_current_step = 0;

  filter_cache->nodes = blender::bke::pbvh::search_gather(ss->pbvh, nullptr, nullptr);

  filter_cache->mask_delta_step = BLI_ghash_int_new("mask filter delta steps");
  switch (filter_type) {
    case IPMASK_FILTER_SMOOTH_SHARPEN:
      filter_cache->mask_filter_step_forward = sculpt_ipmask_vertex_smooth_cb;
      filter_cache->mask_filter_step_backward = sculpt_ipmask_vertex_sharpen_cb;
      break;
    case IPMASK_FILTER_GROW_SHRINK:
      filter_cache->mask_filter_step_forward = sculpt_ipmask_vertex_grow_cb;
      filter_cache->mask_filter_step_backward = sculpt_ipmask_vertex_shrink_cb;
      break;
    case IPMASK_FILTER_HARDER_SOFTER:
      filter_cache->mask_filter_step_forward = sculpt_ipmask_vertex_harder_cb;
      filter_cache->mask_filter_step_backward = sculpt_ipmask_vertex_softer_cb;
      break;
    case IPMASK_FILTER_CONTRAST:
      filter_cache->mask_filter_step_forward = sculpt_ipmask_vertex_contrast_increase_cb;
      filter_cache->mask_filter_step_backward = sculpt_ipmask_vertex_contrast_decrease_cb;
      break;
    case IPMASK_FILTER_ADD_SUBSTRACT:
    case IPMASK_FILTER_INVERT:
    case IPMASK_FILTER_QUANTIZE:
      break;
  }

  return filter_cache;
}

static int sculpt_ipmask_filter_invoke(bContext *C, wmOperator *op, const wmEvent *event)
{
  Object *ob = CTX_data_active_object(C);
  Sculpt *sd = CTX_data_tool_settings(C)->sculpt;
  SculptSession *ss = ob->sculpt;
  Depsgraph *depsgraph = CTX_data_ensure_evaluated_depsgraph(C);

  SCULPT_undo_push_begin(ob, op);

  BKE_sculpt_update_object_for_edit(depsgraph, ob, true, true, false);
  BKE_sculpt_ensure_origmask(ob);

  const eSculptIPMaskFilterType filter_type = (eSculptIPMaskFilterType)RNA_enum_get(op->ptr,
                                                                                    "filter_type");
  ss->filter_cache = sculpt_ipmask_filter_cache_init(ob, sd, filter_type, true);
  sculpt_ipmask_store_initial_undo_step(ob);
  sculpt_ipmask_store_reference_step(ss);

  WM_event_add_modal_handler(C, op);
  return OPERATOR_RUNNING_MODAL;
}
static int sculpt_ipmask_filter_exec(bContext *C, wmOperator *op)
{
  Object *ob = CTX_data_active_object(C);
  Sculpt *sd = CTX_data_tool_settings(C)->sculpt;
  SculptSession *ss = ob->sculpt;
  Depsgraph *depsgraph = CTX_data_ensure_evaluated_depsgraph(C);

  const int iteration_count = RNA_int_get(op->ptr, "iterations");
  const float strength = RNA_float_get(op->ptr, "strength");
  const eSculptIPMaskFilterType filter_type = (eSculptIPMaskFilterType)RNA_enum_get(op->ptr,
                                                                                    "filter_type");
  const int direction = RNA_enum_get(op->ptr, "direction");

  SCULPT_undo_push_begin(ob, op);
  BKE_sculpt_update_object_for_edit(depsgraph, ob, true, true, false);
  BKE_sculpt_ensure_origmask(ob);

  ss->filter_cache = sculpt_ipmask_filter_cache_init(ob, sd, filter_type, false);
  sculpt_ipmask_store_initial_undo_step(ob);
  sculpt_ipmask_store_reference_step(ss);

  const float target_step = direction == MASK_FILTER_STEP_DIRECTION_FORWARD ? 1 : -1;
  if (sculpt_ipmask_filter_uses_apply_from_original(filter_type)) {
    sculpt_ipmask_apply_from_original_mask_data(ob, filter_type, strength * target_step);
  }
  else {
    sculpt_ipmask_filter_update_to_target_step(ss, target_step, iteration_count, 0.0f);
  }

  SCULPT_tag_update_overlays(C);
  SCULPT_filter_cache_free(ss, ob);
  SCULPT_undo_push_end(ob);
  SCULPT_flush_update_done(C, ob, SCULPT_UPDATE_MASK);
  return OPERATOR_FINISHED;
}

void SCULPT_OT_ipmask_filter(struct wmOperatorType *ot)
{
  /* Identifiers. */
  ot->name = "Interactive Preview Mask Filter";
  ot->idname = "SCULPT_OT_ipmask_filter";
  ot->description = "Applies a filter to modify the current mask";

  /* API callbacks. */
  ot->exec = sculpt_ipmask_filter_exec;
  ot->invoke = sculpt_ipmask_filter_invoke;
  ot->modal = sculpt_ipmask_filter_modal;
  ot->cancel = sculpt_ipmask_filter_cancel;
  ot->poll = SCULPT_mode_poll;

  ot->flag = OPTYPE_REGISTER;

  static EnumPropertyItem prop_ipmask_filter_types[] = {
      {IPMASK_FILTER_SMOOTH_SHARPEN,
       "SMOOTH_SHARPEN",
       0,
       "Smooth/Sharpen",
       "Smooth and sharpen the mask"},
      {IPMASK_FILTER_GROW_SHRINK, "GROW_SHRINK", 0, "Grow/Shrink", "Grow and shirnk the mask"},
      {IPMASK_FILTER_HARDER_SOFTER,
       "HARDER_SOFTER",
       0,
       "Harder/Softer",
       "Makes the entire mask harder or softer"},
      {IPMASK_FILTER_ADD_SUBSTRACT,
       "ADD_SUBSTRACT",
       0,
       "Add/Substract",
       "Adds or substract a value to the mask"},
      {IPMASK_FILTER_CONTRAST,
       "CONTRAST",
       0,
       "Contrast",
       "Increases or decreases the contrast of the mask"},
      {IPMASK_FILTER_INVERT, "INVERT", 0, "Invert", "Inverts the mask"},
      {IPMASK_FILTER_QUANTIZE, "QUANTIZE", 0, "Quantize", "Quantizes the mask to intervals"},
      {0, nullptr, 0, nullptr, nullptr},
  };

  static EnumPropertyItem prop_ipmask_filter_direction_types[] = {
      {MASK_FILTER_STEP_DIRECTION_FORWARD,
       "FORWARD",
       0,
       "Forward",
       "Apply the filter in the forward direction"},
      {MASK_FILTER_STEP_DIRECTION_BACKWARD,
       "BACKWARD",
       0,
       "Backward",
       "Apply the filter in the backward direction"},
      {0, nullptr, 0, nullptr, nullptr},
  };

  /* RNA. */
  RNA_def_enum(ot->srna,
               "filter_type",
               prop_ipmask_filter_types,
               IPMASK_FILTER_GROW_SHRINK,
               "Type",
               "Filter that is going to be applied to the mask");
  RNA_def_enum(ot->srna,
               "direction",
               prop_ipmask_filter_direction_types,
               MASK_FILTER_STEP_DIRECTION_FORWARD,
               "Direction",
               "Direction to apply the filter step");
  RNA_def_int(ot->srna,
              "iterations",
              1,
              1,
              100,
              "Iterations per Step",
              "Number of times that the filter is going to be applied per step",
              1,
              100);
  RNA_def_boolean(
      ot->srna,
      "use_step_interpolation",
      true,
      "Step Interpolation",
      "Calculate and render intermediate values between multiple full steps of the filter");
  RNA_def_float(
      ot->srna, "strength", 1.0f, -10.0f, 10.0f, "Strength", "Filter strength", -10.0f, 10.0f);
}
