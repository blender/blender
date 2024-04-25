/* SPDX-FileCopyrightText: 2024 Blender Authors
 *
 * SPDX-License-Identifier: GPL-2.0-or-later */

/** \file
 * \ingroup edgreasepencil
 */

#include "BKE_brush.hh"
#include "BKE_context.hh"
#include "BKE_crazyspace.hh"
#include "BKE_deform.hh"
#include "BKE_grease_pencil.hh"
#include "BKE_modifier.hh"
#include "BKE_paint.hh"

#include "DNA_meshdata_types.h"

#include "ED_curves.hh"
#include "ED_view3d.hh"

#include "DEG_depsgraph_query.hh"

#include "ED_grease_pencil.hh"

namespace blender::ed::greasepencil {

Set<std::string> get_bone_deformed_vertex_group_names(const Object &object)
{
  /* Get all vertex group names in the object. */
  const ListBase *defbase = BKE_object_defgroup_list(&object);
  Set<std::string> defgroups;
  LISTBASE_FOREACH (bDeformGroup *, dg, defbase) {
    defgroups.add(dg->name);
  }

  /* Inspect all armature modifiers in the object. */
  Set<std::string> bone_deformed_vgroups;
  VirtualModifierData virtual_modifier_data;
  ModifierData *md = BKE_modifiers_get_virtual_modifierlist(&object, &virtual_modifier_data);
  for (; md; md = md->next) {
    if (!(md->mode & (eModifierMode_Realtime | eModifierMode_Virtual)) ||
        md->type != eModifierType_Armature)
    {
      continue;
    }
    ArmatureModifierData *amd = reinterpret_cast<ArmatureModifierData *>(md);
    if (!amd->object || !amd->object->pose) {
      continue;
    }

    bPose *pose = amd->object->pose;
    LISTBASE_FOREACH (bPoseChannel *, channel, &pose->chanbase) {
      if (channel->bone->flag & BONE_NO_DEFORM) {
        continue;
      }
      /* When a vertex group name matches the bone name, it is bone-deformed. */
      if (defgroups.contains(channel->name)) {
        bone_deformed_vgroups.add(channel->name);
      }
    }
  }

  return bone_deformed_vgroups;
}

/* Normalize the weights of vertex groups deformed by bones so that the sum is 1.0f.
 * Returns false when the normalization failed due to too many locked vertex groups. In that case a
 * second pass can be done with the active vertex group unlocked.
 */
static bool normalize_vertex_weights_try(MDeformVert &dvert,
                                         const int vertex_groups_num,
                                         const Span<bool> vertex_group_is_bone_deformed,
                                         const FunctionRef<bool(int)> vertex_group_is_locked)
{
  /* Nothing to normalize when there are less than two vertex group weights. */
  if (dvert.totweight <= 1) {
    return true;
  }

  /* Get the sum of weights of bone-deformed vertex groups. */
  float sum_weights_total = 0.0f;
  float sum_weights_locked = 0.0f;
  float sum_weights_unlocked = 0.0f;
  int locked_num = 0;
  int unlocked_num = 0;
  for (const int i : IndexRange(dvert.totweight)) {
    MDeformWeight &dw = dvert.dw[i];

    /* Auto-normalize is only applied on bone-deformed vertex groups that have weight already. */
    if (dw.def_nr >= vertex_groups_num || !vertex_group_is_bone_deformed[dw.def_nr] ||
        dw.weight <= FLT_EPSILON)
    {
      continue;
    }

    sum_weights_total += dw.weight;

    if (vertex_group_is_locked(dw.def_nr)) {
      locked_num++;
      sum_weights_locked += dw.weight;
    }
    else {
      unlocked_num++;
      sum_weights_unlocked += dw.weight;
    }
  }

  /* Already normalized? */
  if (sum_weights_total == 1.0f) {
    return true;
  }

  /* Any unlocked vertex group to normalize? */
  if (unlocked_num == 0) {
    /* We don't need a second pass when there is only one locked group (the active group). */
    return (locked_num == 1);
  }

  /* Locked groups can make it impossible to fully normalize. */
  if (sum_weights_locked >= 1.0f - VERTEX_WEIGHT_LOCK_EPSILON) {
    /* Zero out the weights we are allowed to touch and return false, indicating a second pass is
     * needed. */
    for (const int i : IndexRange(dvert.totweight)) {
      MDeformWeight &dw = dvert.dw[i];
      if (dw.def_nr < vertex_groups_num && vertex_group_is_bone_deformed[dw.def_nr] &&
          !vertex_group_is_locked(dw.def_nr))
      {
        dw.weight = 0.0f;
      }
    }

    return (sum_weights_locked == 1.0f);
  }

  /* When the sum of the unlocked weights isn't zero, we can use a multiplier to normalize them
   * to 1.0f. */
  if (sum_weights_unlocked != 0.0f) {
    const float normalize_factor = (1.0f - sum_weights_locked) / sum_weights_unlocked;

    for (const int i : IndexRange(dvert.totweight)) {
      MDeformWeight &dw = dvert.dw[i];
      if (dw.def_nr < vertex_groups_num && vertex_group_is_bone_deformed[dw.def_nr] &&
          dw.weight > FLT_EPSILON && !vertex_group_is_locked(dw.def_nr))
      {
        dw.weight = math::clamp(dw.weight * normalize_factor, 0.0f, 1.0f);
      }
    }

    return true;
  }

  /* Spread out the remainder of the locked weights over the unlocked weights. */
  const float weight_remainder = math::clamp(
      (1.0f - sum_weights_locked) / unlocked_num, 0.0f, 1.0f);

  for (const int i : IndexRange(dvert.totweight)) {
    MDeformWeight &dw = dvert.dw[i];
    if (dw.def_nr < vertex_groups_num && vertex_group_is_bone_deformed[dw.def_nr] &&
        dw.weight > FLT_EPSILON && !vertex_group_is_locked(dw.def_nr))
    {
      dw.weight = weight_remainder;
    }
  }

  return true;
}

void normalize_vertex_weights(MDeformVert &dvert,
                              const int active_vertex_group,
                              const Span<bool> vertex_group_is_locked,
                              const Span<bool> vertex_group_is_bone_deformed)
{
  /* Try to normalize the weights with both active and explicitly locked vertex groups restricted
   * from change. */
  const auto active_vertex_group_is_locked = [&](const int vertex_group_index) {
    return vertex_group_is_locked[vertex_group_index] || vertex_group_index == active_vertex_group;
  };
  const bool success = normalize_vertex_weights_try(dvert,
                                                    vertex_group_is_locked.size(),
                                                    vertex_group_is_bone_deformed,
                                                    active_vertex_group_is_locked);

  if (success) {
    return;
  }

  /* Do a second pass with the active vertex group unlocked. */
  const auto active_vertex_group_is_unlocked = [&](const int vertex_group_index) {
    return vertex_group_is_locked[vertex_group_index];
  };
  normalize_vertex_weights_try(dvert,
                               vertex_group_is_locked.size(),
                               vertex_group_is_bone_deformed,
                               active_vertex_group_is_unlocked);
}

struct ClosestGreasePencilDrawing {
  const bke::greasepencil::Drawing *drawing = nullptr;
  int active_defgroup_index;
  ed::curves::FindClosestData elem = {};
};

static int weight_sample_invoke(bContext *C, wmOperator * /*op*/, const wmEvent *event)
{
  Depsgraph *depsgraph = CTX_data_ensure_evaluated_depsgraph(C);
  ViewContext vc = ED_view3d_viewcontext_init(C, depsgraph);

  /* Get the active vertex group. */
  const int object_defgroup_nr = BKE_object_defgroup_active_index_get(vc.obact) - 1;
  if (object_defgroup_nr == -1) {
    return OPERATOR_CANCELLED;
  }
  const bDeformGroup *object_defgroup = static_cast<const bDeformGroup *>(
      BLI_findlink(BKE_object_defgroup_list(vc.obact), object_defgroup_nr));

  /* Collect visible drawings. */
  const Object *ob_eval = DEG_get_evaluated_object(vc.depsgraph, const_cast<Object *>(vc.obact));
  GreasePencil &grease_pencil = *static_cast<GreasePencil *>(vc.obact->data);
  const Vector<DrawingInfo> drawings = retrieve_visible_drawings(*vc.scene, grease_pencil, false);

  /* Find stroke points closest to mouse cursor position. */
  const ClosestGreasePencilDrawing closest = threading::parallel_reduce(
      drawings.index_range(),
      1L,
      ClosestGreasePencilDrawing(),
      [&](const IndexRange range, const ClosestGreasePencilDrawing &init) {
        ClosestGreasePencilDrawing new_closest = init;
        for (const int i : range) {
          DrawingInfo info = drawings[i];
          const bke::greasepencil::Layer &layer = *grease_pencil.layers()[info.layer_index];

          /* Skip drawing when it doesn't use the active vertex group. */
          const int drawing_defgroup_nr = BLI_findstringindex(
              &info.drawing.strokes().vertex_group_names,
              object_defgroup->name,
              offsetof(bDeformGroup, name));
          if (drawing_defgroup_nr == -1) {
            continue;
          }

          /* Get deformation by modifiers. */
          bke::crazyspace::GeometryDeformation deformation =
              bke::crazyspace::get_evaluated_grease_pencil_drawing_deformation(
                  ob_eval, *vc.obact, info.layer_index, info.frame_number);

          IndexMaskMemory memory;
          const IndexMask points = retrieve_visible_points(*vc.obact, info.drawing, memory);
          if (points.is_empty()) {
            continue;
          }
          const float4x4 layer_to_world = layer.to_world_space(*ob_eval);
          const float4x4 projection = ED_view3d_ob_project_mat_get_from_obmat(vc.rv3d,
                                                                              layer_to_world);
          std::optional<ed::curves::FindClosestData> new_closest_elem =
              ed::curves::closest_elem_find_screen_space(vc,
                                                         info.drawing.strokes().points_by_curve(),
                                                         deformation.positions,
                                                         projection,
                                                         points,
                                                         bke::AttrDomain::Point,
                                                         event->mval,
                                                         new_closest.elem);
          if (new_closest_elem) {
            new_closest.elem = *new_closest_elem;
            new_closest.drawing = &info.drawing;
            new_closest.active_defgroup_index = drawing_defgroup_nr;
          }
        }
        return new_closest;
      },
      [](const ClosestGreasePencilDrawing &a, const ClosestGreasePencilDrawing &b) {
        return (a.elem.distance < b.elem.distance) ? a : b;
      });

  if (!closest.drawing) {
    return OPERATOR_CANCELLED;
  }

  /* From the closest point found, get the vertex weight in the active vertex group. */
  const VArray<float> point_weights = bke::varray_for_deform_verts(
      closest.drawing->strokes().deform_verts(), closest.active_defgroup_index);
  const float new_weight = math::clamp(point_weights[closest.elem.index], 0.0f, 1.0f);

  /* Set the new brush weight. */
  const ToolSettings *ts = vc.scene->toolsettings;
  Brush *brush = BKE_paint_brush(&ts->wpaint->paint);
  BKE_brush_weight_set(vc.scene, brush, new_weight);

  /* Update brush settings in UI. */
  WM_main_add_notifier(NC_BRUSH | NA_EDITED, nullptr);

  return OPERATOR_FINISHED;
}

static void GREASE_PENCIL_OT_weight_sample(wmOperatorType *ot)
{
  /* Identifiers. */
  ot->name = "Weight Paint Sample Weight";
  ot->idname = "GREASE_PENCIL_OT_weight_sample";
  ot->description =
      "Set the weight of the Draw tool to the weight of the vertex under the mouse cursor";

  /* Callbacks. */
  ot->poll = grease_pencil_weight_painting_poll;
  ot->invoke = weight_sample_invoke;

  /* Flags. */
  ot->flag = OPTYPE_UNDO | OPTYPE_DEPENDS_ON_CURSOR;
}

static int toggle_weight_tool_direction(bContext *C, wmOperator * /*op*/)
{
  Paint *paint = BKE_paint_get_active_from_context(C);
  Brush *brush = BKE_paint_brush(paint);

  /* Toggle direction flag. */
  brush->flag ^= BRUSH_DIR_IN;

  /* Update brush settings in UI. */
  WM_main_add_notifier(NC_BRUSH | NA_EDITED, nullptr);

  return OPERATOR_FINISHED;
}

static bool toggle_weight_tool_direction_poll(bContext *C)
{
  if (!grease_pencil_weight_painting_poll(C)) {
    return false;
  }

  Paint *paint = BKE_paint_get_active_from_context(C);
  if (paint == nullptr) {
    return false;
  }
  Brush *brush = BKE_paint_brush(paint);
  if (brush == nullptr) {
    return false;
  }
  return brush->gpencil_weight_tool == GPWEIGHT_TOOL_DRAW;
}

static void GREASE_PENCIL_OT_weight_toggle_direction(wmOperatorType *ot)
{
  /* Identifiers. */
  ot->name = "Weight Paint Toggle Direction";
  ot->idname = "GREASE_PENCIL_OT_weight_toggle_direction";
  ot->description = "Toggle Add/Subtract for the weight paint draw tool";

  /* Callbacks. */
  ot->poll = toggle_weight_tool_direction_poll;
  ot->exec = toggle_weight_tool_direction;

  /* Flags. */
  ot->flag = OPTYPE_REGISTER | OPTYPE_UNDO;
}

}  // namespace blender::ed::greasepencil

void ED_operatortypes_grease_pencil_weight_paint()
{
  using namespace blender::ed::greasepencil;
  WM_operatortype_append(GREASE_PENCIL_OT_weight_toggle_direction);
  WM_operatortype_append(GREASE_PENCIL_OT_weight_sample);
}
