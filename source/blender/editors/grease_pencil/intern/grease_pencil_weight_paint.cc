/* SPDX-FileCopyrightText: 2024 Blender Authors
 *
 * SPDX-License-Identifier: GPL-2.0-or-later */

/** \file
 * \ingroup edgreasepencil
 */

#include "BKE_armature.hh"
#include "BKE_brush.hh"
#include "BKE_context.hh"
#include "BKE_crazyspace.hh"
#include "BKE_deform.hh"
#include "BKE_grease_pencil.hh"
#include "BKE_modifier.hh"
#include "BKE_object_deform.h"
#include "BKE_paint.hh"
#include "BKE_report.hh"

#include "BLI_math_geom.h"
#include "BLI_math_matrix.h"
#include "BLI_string.h"

#include "DNA_meshdata_types.h"

#include "RNA_access.hh"
#include "RNA_define.hh"

#include "ED_curves.hh"
#include "ED_grease_pencil.hh"
#include "ED_view3d.hh"

#include "DEG_depsgraph_query.hh"

#include "GEO_smooth_curves.hh"

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

static int foreach_bone_in_armature_ex(
    Object &ob, const Bone *bone, const FunctionRef<bool(Object &, const Bone *)> bone_callback)
{
  int count = 0;

  if (bone != nullptr) {
    /* Only call `bone_callback` if the bone is non null */
    count += bone_callback(ob, bone) ? 1 : 0;
    /* Try to execute `bone_callback` for the first child. */
    count += foreach_bone_in_armature_ex(
        ob, static_cast<Bone *>(bone->childbase.first), bone_callback);
    /* Try to execute `bone_callback` for the next bone at this depth of the recursion. */
    count += foreach_bone_in_armature_ex(ob, bone->next, bone_callback);
  }

  return count;
}

static int foreach_bone_in_armature(Object &ob,
                                    const bArmature &armature,
                                    const FunctionRef<bool(Object &, const Bone *)> bone_callback)
{
  return foreach_bone_in_armature_ex(
      ob, static_cast<const Bone *>(armature.bonebase.first), bone_callback);
}

bool add_armature_vertex_groups(Object &object, const Object &ob_armature)
{
  const bArmature &armature = *static_cast<const bArmature *>(ob_armature.data);

  const int added_vertex_groups = foreach_bone_in_armature(
      object, armature, [&](Object &object, const Bone *bone) {
        if ((bone->flag & BONE_NO_DEFORM) == 0) {
          /* Check if the name of the bone matches a vertex group name. */
          if (!BKE_object_defgroup_find_name(&object, bone->name)) {
            /* Add a new vertex group with the name of the bone. */
            BKE_object_defgroup_add_name(&object, bone->name);
            return true;
          }
        }
        return false;
      });

  return added_vertex_groups > 0;
}

static bool get_skinnable_bones_and_deform_group_names(const bArmature &armature,
                                                       Object &object,
                                                       Vector<const Bone *> &r_skinnable_bones,
                                                       Vector<std::string> &r_deform_group_names)
{
  const int added_vertex_groups = foreach_bone_in_armature(
      object, armature, [&](Object &object, const Bone *bone) {
        if ((bone->flag & BONE_NO_DEFORM) == 0) {
          /* Check if the name of the bone matches a vertex group name. */
          bDeformGroup *dg = BKE_object_defgroup_find_name(&object, bone->name);
          if (dg == nullptr) {
            /* Add a new vertex group with the name of the bone. */
            dg = BKE_object_defgroup_add_name(&object, bone->name);
          }
          r_deform_group_names.append(dg->name);
          r_skinnable_bones.append(bone);
          return true;
        }
        return false;
      });

  if (added_vertex_groups <= 0) {
    return false;
  }
  return true;
}

static void get_root_and_tips_of_bones(Span<const Bone *> bones,
                                       const float4x4 &transform,
                                       MutableSpan<float3> roots,
                                       MutableSpan<float3> tips)
{
  threading::parallel_for(bones.index_range(), 4096, [&](const IndexRange range) {
    for (const int i : range) {
      const Bone *bone = bones[i];
      roots[i] = math::transform_point(transform, float3(bone->arm_head));
      tips[i] = math::transform_point(transform, float3(bone->arm_tail));
    }
  });
}

static int lookup_or_add_deform_group_index(CurvesGeometry &curves, const StringRef name)
{
  int def_nr = BKE_defgroup_name_index(&curves.vertex_group_names, name);

  /* Lazily add the vertex group. */
  if (def_nr == -1) {
    bDeformGroup *defgroup = MEM_cnew<bDeformGroup>(__func__);
    name.copy(defgroup->name);
    BLI_addtail(&curves.vertex_group_names, defgroup);
    def_nr = BLI_listbase_count(&curves.vertex_group_names) - 1;
    BLI_assert(def_nr >= 0);
  }

  return def_nr;
}

void add_armature_envelope_weights(Scene &scene, Object &object, const Object &ob_armature)
{
  using namespace bke;
  GreasePencil &grease_pencil = *static_cast<GreasePencil *>(object.data);
  const bArmature &armature = *static_cast<const bArmature *>(ob_armature.data);
  const float4x4 armature_to_world = ob_armature.object_to_world();
  const float scale = mat4_to_scale(armature_to_world.ptr());

  Vector<const Bone *> skinnable_bones;
  Vector<std::string> deform_group_names;
  if (!get_skinnable_bones_and_deform_group_names(
          armature, object, skinnable_bones, deform_group_names))
  {
    return;
  }

  /* Get the roots and tips of the bones in world space. */
  Array<float3> roots(skinnable_bones.size());
  Array<float3> tips(skinnable_bones.size());
  get_root_and_tips_of_bones(
      skinnable_bones, armature_to_world, roots.as_mutable_span(), tips.as_mutable_span());

  Span<const bke::greasepencil::Layer *> layers = grease_pencil.layers();
  Vector<MutableDrawingInfo> drawings = retrieve_editable_drawings(scene, grease_pencil);
  threading::parallel_for_each(drawings, [&](MutableDrawingInfo info) {
    const bke::greasepencil::Layer &layer = *layers[info.layer_index];
    const float4x4 layer_to_world = layer.to_world_space(object);

    CurvesGeometry &curves = info.drawing.strokes_for_write();
    const Span<float3> src_positions = curves.positions();
    /* Get all the positions in world space. */
    Array<float3> positions(curves.points_num());
    threading::parallel_for(positions.index_range(), 4096, [&](const IndexRange range) {
      for (const int i : range) {
        positions[i] = math::transform_point(layer_to_world, src_positions[i]);
      }
    });

    for (const int bone_i : skinnable_bones.index_range()) {
      const Bone *bone = skinnable_bones[bone_i];
      const char *deform_group_name = deform_group_names[bone_i].c_str();
      const float3 bone_root = roots[bone_i];
      const float3 bone_tip = tips[bone_i];

      const int def_nr = lookup_or_add_deform_group_index(curves, deform_group_name);

      MutableSpan<MDeformVert> dverts = curves.deform_verts_for_write();
      VMutableArray<float> weights = bke::varray_for_mutable_deform_verts(dverts, def_nr);
      for (const int point_i : curves.points_range()) {
        const float weight = distfactor_to_bone(positions[point_i],
                                                bone_root,
                                                bone_tip,
                                                bone->rad_head * scale,
                                                bone->rad_tail * scale,
                                                bone->dist * scale);
        if (weight != 0.0f) {
          weights.set(point_i, weight);
        }
      }
    }
  });
}

void add_armature_automatic_weights(Scene &scene, Object &object, const Object &ob_armature)
{
  using namespace bke;
  GreasePencil &grease_pencil = *static_cast<GreasePencil *>(object.data);
  const bArmature &armature = *static_cast<const bArmature *>(ob_armature.data);
  const float4x4 armature_to_world = ob_armature.object_to_world();
  /* Note: These constant values are taken from the legacy grease pencil code. */
  const float default_ratio = 0.1f;
  const float default_decay = 0.8f;

  Vector<const Bone *> skinnable_bones;
  Vector<std::string> deform_group_names;
  if (!get_skinnable_bones_and_deform_group_names(
          armature, object, skinnable_bones, deform_group_names))
  {
    return;
  }

  /* Get the roots and tips of the bones in world space. */
  Array<float3> roots(skinnable_bones.size());
  Array<float3> tips(skinnable_bones.size());
  get_root_and_tips_of_bones(
      skinnable_bones, armature_to_world, roots.as_mutable_span(), tips.as_mutable_span());

  /* Note: This is taken from the legacy grease pencil code. */
  const auto get_weight = [](const float dist, const float decay_rad, const float diff_rad) {
    return (dist < decay_rad) ? 1.0f :
                                math::interpolate(0.9f, 0.0f, (dist - decay_rad) / diff_rad);
  };

  Span<const bke::greasepencil::Layer *> layers = grease_pencil.layers();
  Vector<MutableDrawingInfo> drawings = retrieve_editable_drawings(scene, grease_pencil);
  threading::parallel_for_each(drawings, [&](MutableDrawingInfo info) {
    const bke::greasepencil::Layer &layer = *layers[info.layer_index];
    const float4x4 layer_to_world = layer.to_world_space(object);

    CurvesGeometry &curves = info.drawing.strokes_for_write();
    const Span<float3> src_positions = curves.positions();
    /* Get all the positions in world space. */
    Array<float3> positions(curves.points_num());
    threading::parallel_for(positions.index_range(), 4096, [&](const IndexRange range) {
      for (const int i : range) {
        positions[i] = math::transform_point(layer_to_world, src_positions[i]);
      }
    });

    for (const int bone_i : skinnable_bones.index_range()) {
      const char *deform_group_name = deform_group_names[bone_i].c_str();
      const float3 bone_root = roots[bone_i];
      const float3 bone_tip = tips[bone_i];

      const float radius_squared = math::distance_squared(bone_root, bone_tip) * default_ratio;
      const float decay_rad = radius_squared - (radius_squared * default_decay);
      const float diff_rad = radius_squared - decay_rad;

      const int def_nr = lookup_or_add_deform_group_index(curves, deform_group_name);

      MutableSpan<MDeformVert> dverts = curves.deform_verts_for_write();
      VMutableArray<float> weights = bke::varray_for_mutable_deform_verts(dverts, def_nr);
      for (const int point_i : curves.points_range()) {
        const float3 position = positions[point_i];
        const float dist_to_bone = dist_squared_to_line_segment_v3(position, bone_root, bone_tip);
        const float weight = (dist_to_bone > radius_squared) ?
                                 0.0f :
                                 get_weight(dist_to_bone, decay_rad, diff_rad);
        if (weight != 0.0f) {
          weights.set(point_i, weight);
        }
      }
    }
  });
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
          const bke::greasepencil::Layer &layer = grease_pencil.layer(info.layer_index);

          /* Skip drawing when it doesn't use the active vertex group. */
          const int drawing_defgroup_nr = BKE_defgroup_name_index(
              &info.drawing.strokes().vertex_group_names, object_defgroup->name);
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
          const bke::CurvesGeometry &curves = info.drawing.strokes();
          std::optional<ed::curves::FindClosestData> new_closest_elem =
              ed::curves::closest_elem_find_screen_space(vc,
                                                         curves.points_by_curve(),
                                                         deformation.positions,
                                                         curves.cyclic(),
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
        return (a.elem.distance_sq < b.elem.distance_sq) ? a : b;
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

  BKE_brush_tag_unsaved_changes(brush);
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
  return brush->gpencil_weight_brush_type == GPWEIGHT_BRUSH_TYPE_DRAW;
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

static int grease_pencil_weight_invert_exec(bContext *C, wmOperator *op)
{
  const Scene &scene = *CTX_data_scene(C);
  Object *object = CTX_data_active_object(C);
  GreasePencil &grease_pencil = *static_cast<GreasePencil *>(object->data);

  /* Object vgroup index. */
  const int active_index = BKE_object_defgroup_active_index_get(object) - 1;
  if (active_index == -1) {
    return OPERATOR_CANCELLED;
  }

  const bDeformGroup *active_defgroup = static_cast<const bDeformGroup *>(
      BLI_findlink(BKE_object_defgroup_list(object), active_index));

  if (active_defgroup->flag & DG_LOCK_WEIGHT) {
    BKE_report(op->reports, RPT_WARNING, "Active Vertex Group is locked");
    return OPERATOR_CANCELLED;
  }

  Vector<MutableDrawingInfo> drawings = retrieve_editable_drawings(scene, grease_pencil);

  threading::parallel_for_each(drawings, [&](MutableDrawingInfo info) {
    bke::CurvesGeometry &curves = info.drawing.strokes_for_write();
    /* Active vgroup index of drawing. */
    const int drawing_vgroup_index = BKE_defgroup_name_index(&curves.vertex_group_names,
                                                             active_defgroup->name);
    if (drawing_vgroup_index == -1) {
      return;
    }

    VMutableArray<float> weights = bke::varray_for_mutable_deform_verts(
        curves.deform_verts_for_write(), drawing_vgroup_index);
    if (weights.size() == 0) {
      return;
    }

    for (const int i : weights.index_range()) {
      const float invert_weight = 1.0f - weights[i];
      weights.set(i, invert_weight);
    }
  });

  DEG_id_tag_update(&grease_pencil.id, ID_RECALC_GEOMETRY);
  WM_event_add_notifier(C, NC_GEOM | ND_DATA, &grease_pencil);
  return OPERATOR_FINISHED;
}

static bool grease_pencil_vertex_group_weight_poll(bContext *C)
{
  if (!grease_pencil_weight_painting_poll(C)) {
    return false;
  }

  const Object *ob = CTX_data_active_object(C);
  if (ob == nullptr || BLI_listbase_is_empty(BKE_object_defgroup_list(ob))) {
    return false;
  }

  return true;
}

static void GREASE_PENCIL_OT_weight_invert(wmOperatorType *ot)
{
  /* identifiers */
  ot->name = "Invert Weight";
  ot->idname = "GREASE_PENCIL_OT_weight_invert";
  ot->description = "Invert the weight of active vertex group";

  /* api callbacks */
  ot->exec = grease_pencil_weight_invert_exec;
  ot->poll = grease_pencil_vertex_group_weight_poll;

  /* flags */
  ot->flag = OPTYPE_UNDO | OPTYPE_REGISTER;
}

static int vertex_group_smooth_exec(bContext *C, wmOperator *op)
{
  /* Get the active vertex group in the Grease Pencil object. */
  Object *object = CTX_data_active_object(C);
  const int object_defgroup_nr = BKE_object_defgroup_active_index_get(object) - 1;
  if (object_defgroup_nr == -1) {
    return OPERATOR_CANCELLED;
  }
  const bDeformGroup *object_defgroup = static_cast<const bDeformGroup *>(
      BLI_findlink(BKE_object_defgroup_list(object), object_defgroup_nr));
  if (object_defgroup->flag & DG_LOCK_WEIGHT) {
    BKE_report(op->reports, RPT_WARNING, "Active vertex group is locked");
    return OPERATOR_CANCELLED;
  }

  const float smooth_factor = RNA_float_get(op->ptr, "factor");
  const int repeat = RNA_int_get(op->ptr, "repeat");

  GreasePencil &grease_pencil = *static_cast<GreasePencil *>(object->data);
  const Scene &scene = *CTX_data_scene(C);
  const Vector<MutableDrawingInfo> drawings = retrieve_editable_drawings(scene, grease_pencil);

  /* Smooth weights in all editable drawings. */
  threading::parallel_for(drawings.index_range(), 1, [&](const IndexRange drawing_range) {
    for (const int drawing : drawing_range) {
      bke::CurvesGeometry &curves = drawings[drawing].drawing.strokes_for_write();
      bke::MutableAttributeAccessor attributes = curves.attributes_for_write();

      /* Skip the drawing when it doesn't use the active vertex group. */
      if (!attributes.contains(object_defgroup->name)) {
        continue;
      }

      bke::SpanAttributeWriter<float> weights = attributes.lookup_for_write_span<float>(
          object_defgroup->name);
      geometry::smooth_curve_attribute(curves.curves_range(),
                                       curves.points_by_curve(),
                                       VArray<bool>::ForSingle(true, curves.points_num()),
                                       curves.cyclic(),
                                       repeat,
                                       smooth_factor,
                                       true,
                                       false,
                                       weights.span);
      weights.finish();
    }
  });

  DEG_id_tag_update(&grease_pencil.id, ID_RECALC_GEOMETRY);
  WM_event_add_notifier(C, NC_GEOM | ND_DATA, &grease_pencil);

  return OPERATOR_FINISHED;
}

static void GREASE_PENCIL_OT_vertex_group_smooth(wmOperatorType *ot)
{
  /* Identifiers. */
  ot->name = "Smooth Vertex Group";
  ot->idname = "GREASE_PENCIL_OT_vertex_group_smooth";
  ot->description = "Smooth the weights of the active vertex group";

  /* Callbacks. */
  ot->poll = grease_pencil_vertex_group_weight_poll;
  ot->exec = vertex_group_smooth_exec;

  /* Flags. */
  ot->flag = OPTYPE_UNDO | OPTYPE_REGISTER;

  /* Operator properties. */
  RNA_def_float(ot->srna, "factor", 0.5f, 0.0f, 1.0, "Factor", "", 0.0f, 1.0f);
  RNA_def_int(ot->srna, "repeat", 1, 1, 10000, "Iterations", "", 1, 200);
}

static int vertex_group_normalize_exec(bContext *C, wmOperator *op)
{
  /* Get the active vertex group in the Grease Pencil object. */
  Object *object = CTX_data_active_object(C);
  const int object_defgroup_nr = BKE_object_defgroup_active_index_get(object) - 1;
  if (object_defgroup_nr == -1) {
    return OPERATOR_CANCELLED;
  }
  const bDeformGroup *object_defgroup = static_cast<const bDeformGroup *>(
      BLI_findlink(BKE_object_defgroup_list(object), object_defgroup_nr));
  if (object_defgroup->flag & DG_LOCK_WEIGHT) {
    BKE_report(op->reports, RPT_WARNING, "Active vertex group is locked");
    return OPERATOR_CANCELLED;
  }

  /* Get all editable drawings, grouped per frame. */
  GreasePencil &grease_pencil = *static_cast<GreasePencil *>(object->data);
  const Scene &scene = *CTX_data_scene(C);
  Array<Vector<MutableDrawingInfo>> drawings_per_frame =
      retrieve_editable_drawings_grouped_per_frame(scene, grease_pencil);

  /* Per frame, normalize the weights in the active vertex group. */
  bool changed = false;
  for (const int frame_i : drawings_per_frame.index_range()) {
    /* Get the maximum weight in the active vertex group for this frame. */
    const Vector<MutableDrawingInfo> drawings = drawings_per_frame[frame_i];
    const float max_weight_in_frame = threading::parallel_reduce(
        drawings.index_range(),
        1,
        0.0f,
        [&](const IndexRange drawing_range, const float &drawing_weight_init) {
          float max_weight_in_drawing = drawing_weight_init;
          for (const int drawing_i : drawing_range) {
            const bke::CurvesGeometry &curves = drawings[drawing_i].drawing.strokes();
            const bke::AttributeAccessor attributes = curves.attributes();

            /* Skip the drawing when it doesn't use the active vertex group. */
            if (!attributes.contains(object_defgroup->name)) {
              continue;
            }

            /* Get the maximum weight in this drawing. */
            const VArray<float> weights = *curves.attributes().lookup_or_default<float>(
                object_defgroup->name, bke::AttrDomain::Point, 0.0f);
            const float max_weight_in_points = threading::parallel_reduce(
                weights.index_range(),
                1024,
                max_weight_in_drawing,
                [&](const IndexRange point_range, const float &init) {
                  float max_weight = init;
                  for (const int point_i : point_range) {
                    max_weight = math::max(max_weight, weights[point_i]);
                  }
                  return max_weight;
                },
                [](const float a, const float b) { return math::max(a, b); });
            max_weight_in_drawing = math::max(max_weight_in_drawing, max_weight_in_points);
          }
          return max_weight_in_drawing;
        },
        [](const float a, const float b) { return math::max(a, b); });

    if (ELEM(max_weight_in_frame, 0.0f, 1.0f)) {
      continue;
    }

    /* Normalize weights from 0.0 to 1.0, by dividing the weights in the active vertex group by the
     * maximum weight in the frame. */
    changed = true;
    threading::parallel_for(drawings.index_range(), 1, [&](const IndexRange drawing_range) {
      for (const int drawing_i : drawing_range) {
        bke::CurvesGeometry &curves = drawings[drawing_i].drawing.strokes_for_write();
        bke::MutableAttributeAccessor attributes = curves.attributes_for_write();

        /* Skip the drawing when it doesn't use the active vertex group. */
        if (!attributes.contains(object_defgroup->name)) {
          continue;
        }

        bke::SpanAttributeWriter<float> weights = attributes.lookup_for_write_span<float>(
            object_defgroup->name);
        threading::parallel_for(
            weights.span.index_range(), 1024, [&](const IndexRange point_range) {
              for (const int point_i : point_range) {
                weights.span[point_i] /= max_weight_in_frame;
              }
            });
        weights.finish();
      }
    });
  }

  if (changed) {
    DEG_id_tag_update(&grease_pencil.id, ID_RECALC_GEOMETRY);
    WM_event_add_notifier(C, NC_GEOM | ND_DATA, &grease_pencil);
  }

  return OPERATOR_FINISHED;
}

static void GREASE_PENCIL_OT_vertex_group_normalize(wmOperatorType *ot)
{
  /* Identifiers. */
  ot->name = "Normalize Vertex Group";
  ot->idname = "GREASE_PENCIL_OT_vertex_group_normalize";
  ot->description = "Normalize weights of the active vertex group";

  /* Callbacks. */
  ot->poll = grease_pencil_vertex_group_weight_poll;
  ot->exec = vertex_group_normalize_exec;

  /* Flags. */
  ot->flag = OPTYPE_REGISTER | OPTYPE_UNDO;
}

static int vertex_group_normalize_all_exec(bContext *C, wmOperator *op)
{
  /* Get the active vertex group in the Grease Pencil object. */
  Object *object = CTX_data_active_object(C);
  const int object_defgroup_nr = BKE_object_defgroup_active_index_get(object) - 1;
  const bDeformGroup *object_defgroup = static_cast<const bDeformGroup *>(
      BLI_findlink(BKE_object_defgroup_list(object), object_defgroup_nr));

  /* Get the locked vertex groups in the object. */
  Set<std::string> object_locked_defgroups;
  const ListBase *defgroups = BKE_object_defgroup_list(object);
  LISTBASE_FOREACH (bDeformGroup *, dg, defgroups) {
    if ((dg->flag & DG_LOCK_WEIGHT) != 0) {
      object_locked_defgroups.add(dg->name);
    }
  }
  const bool lock_active_group = RNA_boolean_get(op->ptr, "lock_active");

  /* Get all editable drawings. */
  GreasePencil &grease_pencil = *static_cast<GreasePencil *>(object->data);
  const Scene &scene = *CTX_data_scene(C);
  const Vector<MutableDrawingInfo> drawings = retrieve_editable_drawings(scene, grease_pencil);

  /* Normalize weights in all drawings. */
  threading::parallel_for(drawings.index_range(), 1, [&](const IndexRange drawing_range) {
    for (const int drawing_i : drawing_range) {
      bke::CurvesGeometry &curves = drawings[drawing_i].drawing.strokes_for_write();

      /* Get the active vertex group in the drawing when it needs to be locked. */
      int active_vertex_group = -1;
      if (object_defgroup && lock_active_group) {
        active_vertex_group = BKE_defgroup_name_index(&curves.vertex_group_names,
                                                      object_defgroup->name);
      }

      /* Put the lock state of every vertex group in a boolean array. */
      Vector<bool> vertex_group_is_locked;
      Vector<bool> vertex_group_is_included;
      LISTBASE_FOREACH (bDeformGroup *, dg, &curves.vertex_group_names) {
        vertex_group_is_locked.append(object_locked_defgroups.contains(dg->name));
        /* Dummy, needed for the #normalize_vertex_weights() call.*/
        vertex_group_is_included.append(true);
      }

      /* For all points in the drawing, normalize the weights of all vertex groups to the sum
       * of 1.0. */
      MutableSpan<MDeformVert> deform_verts = curves.deform_verts_for_write();
      threading::parallel_for(curves.points_range(), 1024, [&](const IndexRange point_range) {
        for (const int point_i : point_range) {
          normalize_vertex_weights(deform_verts[point_i],
                                   active_vertex_group,
                                   vertex_group_is_locked,
                                   vertex_group_is_included);
        }
      });
    }
  });

  DEG_id_tag_update(&grease_pencil.id, ID_RECALC_GEOMETRY);
  WM_event_add_notifier(C, NC_GEOM | ND_DATA, &grease_pencil);

  return OPERATOR_FINISHED;
}

static void GREASE_PENCIL_OT_vertex_group_normalize_all(wmOperatorType *ot)
{
  /* Identifiers. */
  ot->name = "Normalize All Vertex Groups";
  ot->idname = "GREASE_PENCIL_OT_vertex_group_normalize_all";
  ot->description =
      "Normalize the weights of all vertex groups, so that for each vertex, the sum of all "
      "weights is 1.0";

  /* Callbacks. */
  ot->poll = grease_pencil_vertex_group_weight_poll;
  ot->exec = vertex_group_normalize_all_exec;

  /* Flags. */
  ot->flag = OPTYPE_REGISTER | OPTYPE_UNDO;

  /* Operator properties. */
  RNA_def_boolean(ot->srna,
                  "lock_active",
                  true,
                  "Lock Active",
                  "Keep the values of the active group while normalizing others");
}

}  // namespace blender::ed::greasepencil

void ED_operatortypes_grease_pencil_weight_paint()
{
  using namespace blender::ed::greasepencil;
  WM_operatortype_append(GREASE_PENCIL_OT_weight_toggle_direction);
  WM_operatortype_append(GREASE_PENCIL_OT_weight_sample);
  WM_operatortype_append(GREASE_PENCIL_OT_weight_invert);
  WM_operatortype_append(GREASE_PENCIL_OT_vertex_group_smooth);
  WM_operatortype_append(GREASE_PENCIL_OT_vertex_group_normalize);
  WM_operatortype_append(GREASE_PENCIL_OT_vertex_group_normalize_all);
}
