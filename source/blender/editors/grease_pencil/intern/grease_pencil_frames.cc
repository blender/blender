/* SPDX-FileCopyrightText: 2023 Blender Authors
 *
 * SPDX-License-Identifier: GPL-2.0-or-later */

/** \file
 * \ingroup edgreasepencil
 */

#include "BLI_map.hh"
#include "BLI_math_vector_types.hh"
#include "BLI_utildefines.h"

#include "BKE_context.hh"
#include "BKE_grease_pencil.hh"
#include "BKE_paint.hh"
#include "BKE_report.hh"

#include "DEG_depsgraph.hh"

#include "DNA_scene_types.h"

#include "ANIM_keyframing.hh"

#include "ED_grease_pencil.hh"
#include "ED_keyframes_edit.hh"
#include "ED_markers.hh"

#include "RNA_access.hh"
#include "RNA_define.hh"

#include "WM_api.hh"

namespace blender::ed::greasepencil {

void set_selected_frames_type(bke::greasepencil::Layer &layer,
                              const eBezTriple_KeyframeType key_type)
{
  for (GreasePencilFrame &frame : layer.frames_for_write().values()) {
    if (frame.is_selected()) {
      frame.type = key_type;
      layer.tag_frames_map_changed();
    }
  }
}

static float get_snapped_frame_number(const float frame_number,
                                      Scene &scene,
                                      const eEditKeyframes_Snap mode)
{
  switch (mode) {
    case SNAP_KEYS_CURFRAME: /* Snap to current frame. */
      return scene.r.cfra;
    case SNAP_KEYS_NEARSEC: /* Snap to nearest second. */
    {
      float secf = (scene.r.frs_sec / scene.r.frs_sec_base);
      return floorf(frame_number / secf + 0.5f) * secf;
    }
    case SNAP_KEYS_NEARMARKER: /* Snap to nearest marker. */
      return ED_markers_find_nearest_marker_time(&scene.markers, frame_number);
    default:
      break;
  }
  return frame_number;
}

bool snap_selected_frames(GreasePencil &grease_pencil,
                          bke::greasepencil::Layer &layer,
                          Scene &scene,
                          const eEditKeyframes_Snap mode)
{
  bool changed = false;
  blender::Map<int, int> frame_number_destinations;
  for (auto [frame_number, frame] : layer.frames().items()) {
    if (!frame.is_selected()) {
      continue;
    }
    const int snapped = round_fl_to_int(
        get_snapped_frame_number(float(frame_number), scene, mode));
    if (snapped != frame_number) {
      frame_number_destinations.add(frame_number, snapped);
      changed = true;
    }
  }

  if (changed) {
    grease_pencil.move_frames(layer, frame_number_destinations);
  }

  return changed;
}

static int get_mirrored_frame_number(const int frame_number,
                                     const Scene &scene,
                                     const eEditKeyframes_Mirror mode,
                                     const TimeMarker *first_selected_marker)
{
  switch (mode) {
    case MIRROR_KEYS_CURFRAME: /* Mirror over current frame. */
      return 2 * scene.r.cfra - frame_number;
    case MIRROR_KEYS_XAXIS:
    case MIRROR_KEYS_YAXIS: /* Mirror over frame 0. */
      return -frame_number;
    case MIRROR_KEYS_MARKER: /* Mirror over marker. */
      if (first_selected_marker == nullptr) {
        break;
      }
      return 2 * first_selected_marker->frame - frame_number;
    default:
      break;
  }
  return frame_number;
}

bool mirror_selected_frames(GreasePencil &grease_pencil,
                            bke::greasepencil::Layer &layer,
                            Scene &scene,
                            const eEditKeyframes_Mirror mode)
{
  bool changed = false;
  Map<int, int> frame_number_destinations;

  /* Pre-compute the first selected marker, so that we don't compute it for each frame. */
  const TimeMarker *first_selected_marker = (mode == MIRROR_KEYS_MARKER) ?
                                                ED_markers_get_first_selected(&scene.markers) :
                                                nullptr;

  for (auto [frame_number, frame] : layer.frames().items()) {
    if (!frame.is_selected()) {
      continue;
    }

    const int mirrored_frame_number = get_mirrored_frame_number(
        frame_number, scene, mode, first_selected_marker);

    if (mirrored_frame_number != frame_number) {
      frame_number_destinations.add(frame_number, mirrored_frame_number);
      changed = true;
    }
  }

  if (changed) {
    grease_pencil.move_frames(layer, frame_number_destinations);
  }

  return changed;
}

bool duplicate_selected_frames(GreasePencil &grease_pencil, bke::greasepencil::Layer &layer)
{
  using namespace bke::greasepencil;
  bool changed = false;
  LayerTransformData &trans_data = layer.runtime->trans_data_;

  for (auto [frame_number, frame] : layer.frames_for_write().items()) {
    if (!frame.is_selected()) {
      continue;
    }

    /* Create the duplicate drawing. */
    const Drawing *drawing = grease_pencil.get_editable_drawing_at(layer, frame_number);
    if (drawing == nullptr) {
      continue;
    }
    const int duplicated_drawing_index = grease_pencil.drawings().size();
    grease_pencil.add_duplicate_drawings(1, *drawing);

    /* Make a copy of the frame in the duplicates. */
    GreasePencilFrame frame_duplicate = frame;
    frame_duplicate.drawing_index = duplicated_drawing_index;
    trans_data.temp_frames_buffer.add_overwrite(frame_number, frame_duplicate);

    /* Deselect the current frame, so that only the copy is selected. */
    frame.flag ^= GP_FRAME_SELECTED;

    changed = true;
  }

  if (changed) {
    layer.tag_frames_map_changed();
  }

  return changed;
}

bool remove_all_selected_frames(GreasePencil &grease_pencil, bke::greasepencil::Layer &layer)
{
  Vector<int> frames_to_remove;
  for (auto [frame_number, frame] : layer.frames().items()) {
    if (!frame.is_selected()) {
      continue;
    }
    frames_to_remove.append(frame_number);
  }
  return grease_pencil.remove_frames(layer, frames_to_remove.as_span());
}

static void select_frame(GreasePencilFrame &frame, const short select_mode)
{
  switch (select_mode) {
    case SELECT_ADD:
      frame.flag |= GP_FRAME_SELECTED;
      break;
    case SELECT_SUBTRACT:
      frame.flag &= ~GP_FRAME_SELECTED;
      break;
    case SELECT_INVERT:
      frame.flag ^= GP_FRAME_SELECTED;
      break;
  }
}

bool select_frame_at(bke::greasepencil::Layer &layer,
                     const int frame_number,
                     const short select_mode)
{

  GreasePencilFrame *frame = layer.frames_for_write().lookup_ptr(frame_number);
  if (frame == nullptr) {
    return false;
  }
  select_frame(*frame, select_mode);
  layer.tag_frames_map_changed();
  return true;
}

void select_frames_at(bke::greasepencil::LayerGroup &layer_group,
                      const int frame_number,
                      const short select_mode)
{
  LISTBASE_FOREACH_BACKWARD (GreasePencilLayerTreeNode *, node_, &layer_group.children) {
    bke::greasepencil::TreeNode &node = node_->wrap();
    if (node.is_group()) {
      select_frames_at(node.as_group(), frame_number, select_mode);
    }
    else if (node.is_layer()) {
      select_frame_at(node.as_layer(), frame_number, select_mode);
    }
  }
}

void select_all_frames(bke::greasepencil::Layer &layer, const short select_mode)
{
  for (auto item : layer.frames_for_write().items()) {
    select_frame(item.value, select_mode);
    layer.tag_frames_map_changed();
  }
}

bool has_any_frame_selected(const bke::greasepencil::Layer &layer)
{
  for (const auto &[frame_number, frame] : layer.frames().items()) {
    if (frame.is_selected()) {
      return true;
    }
  }
  return false;
}

void select_frames_region(KeyframeEditData *ked,
                          bke::greasepencil::TreeNode &node,
                          const short tool,
                          const short select_mode)
{
  if (node.is_layer()) {
    for (auto [frame_number, frame] : node.as_layer().frames_for_write().items()) {
      /* Construct a dummy point coordinate to do this testing with. */
      const float2 pt(float(frame_number), ked->channel_y);

      /* Check the necessary regions. */
      if (tool == BEZT_OK_CHANNEL_LASSO) {
        if (keyframe_region_lasso_test(static_cast<const KeyframeEdit_LassoData *>(ked->data), pt))
        {
          select_frame(frame, select_mode);
        }
      }
      else if (tool == BEZT_OK_CHANNEL_CIRCLE) {
        if (keyframe_region_circle_test(static_cast<const KeyframeEdit_CircleData *>(ked->data),
                                        pt))
        {
          select_frame(frame, select_mode);
        }
      }

      node.as_layer().tag_frames_map_changed();
    }
  }
  else if (node.is_group()) {
    LISTBASE_FOREACH_BACKWARD (GreasePencilLayerTreeNode *, node_, &node.as_group().children) {
      select_frames_region(ked, node_->wrap(), tool, select_mode);
    }
  }
}

void select_frames_range(bke::greasepencil::TreeNode &node,
                         const float min,
                         const float max,
                         const short select_mode)
{
  /* Only select those frames which are in bounds. */
  if (node.is_layer()) {
    for (auto [frame_number, frame] : node.as_layer().frames_for_write().items()) {
      if (IN_RANGE(float(frame_number), min, max)) {
        select_frame(frame, select_mode);
        node.as_layer().tag_frames_map_changed();
      }
    }
  }
  else if (node.is_group()) {
    LISTBASE_FOREACH_BACKWARD (GreasePencilLayerTreeNode *, node_, &node.as_group().children) {
      select_frames_range(node_->wrap(), min, max, select_mode);
    }
  }
}

static void append_frame_to_key_edit_data(KeyframeEditData *ked,
                                          const int frame_number,
                                          const GreasePencilFrame &frame)
{
  CfraElem *ce = MEM_cnew<CfraElem>(__func__);
  ce->cfra = float(frame_number);
  ce->sel = frame.is_selected();
  BLI_addtail(&ked->list, ce);
}

void create_keyframe_edit_data_selected_frames_list(KeyframeEditData *ked,
                                                    const bke::greasepencil::Layer &layer)
{
  BLI_assert(ked != nullptr);

  for (const auto &[frame_number, frame] : layer.frames().items()) {
    if (frame.is_selected()) {
      append_frame_to_key_edit_data(ked, frame_number, frame);
    }
  }
}

bool ensure_active_keyframe(const Scene &scene, GreasePencil &grease_pencil)
{
  const int current_frame = scene.r.cfra;
  bke::greasepencil::Layer &active_layer = *grease_pencil.get_active_layer();

  if (!active_layer.has_drawing_at(current_frame) && !blender::animrig::is_autokey_on(&scene)) {
    return false;
  }

  /* If auto-key is on and the drawing at the current frame starts before the current frame a new
   * keyframe needs to be inserted. */
  const bool is_first = active_layer.sorted_keys().is_empty() ||
                        (active_layer.sorted_keys().first() > current_frame);
  const bool needs_new_drawing = is_first ||
                                 (*active_layer.frame_key_at(current_frame) < current_frame);

  if (blender::animrig::is_autokey_on(&scene) && needs_new_drawing) {
    const Brush *brush = BKE_paint_brush_for_read(&scene.toolsettings->gp_paint->paint);
    if (((scene.toolsettings->gpencil_flags & GP_TOOL_FLAG_RETAIN_LAST) != 0) ||
        (brush->gpencil_tool == GPAINT_TOOL_ERASE))
    {
      /* For additive drawing, we duplicate the frame that's currently visible and insert it at the
       * current frame. Also duplicate the frame when erasing, Otherwise empty drawing is added,
       * see !119051 */
      grease_pencil.insert_duplicate_frame(
          active_layer, *active_layer.frame_key_at(current_frame), current_frame, false);
    }
    else {
      /* Otherwise we just insert a blank keyframe at the current frame. */
      grease_pencil.insert_blank_frame(active_layer, current_frame, 0, BEZT_KEYTYPE_KEYFRAME);
    }
  }
  /* There should now always be a drawing at the current frame. */
  BLI_assert(active_layer.has_drawing_at(current_frame));

  return true;
}

static int insert_blank_frame_exec(bContext *C, wmOperator *op)
{
  using namespace blender::bke::greasepencil;
  Scene *scene = CTX_data_scene(C);
  Object *object = CTX_data_active_object(C);
  GreasePencil &grease_pencil = *static_cast<GreasePencil *>(object->data);
  const int current_frame = scene->r.cfra;
  const bool all_layers = RNA_boolean_get(op->ptr, "all_layers");
  const int duration = RNA_int_get(op->ptr, "duration");

  bool changed = false;
  if (all_layers) {
    for (Layer *layer : grease_pencil.layers_for_write()) {
      if (!layer->is_editable()) {
        continue;
      }
      changed = grease_pencil.insert_blank_frame(
          *layer, current_frame, duration, BEZT_KEYTYPE_KEYFRAME);
    }
  }
  else {
    if (!grease_pencil.has_active_layer()) {
      return OPERATOR_CANCELLED;
    }
    changed = grease_pencil.insert_blank_frame(
        *grease_pencil.get_active_layer(), current_frame, duration, BEZT_KEYTYPE_KEYFRAME);
  }

  if (changed) {
    DEG_id_tag_update(&grease_pencil.id, ID_RECALC_GEOMETRY);
    WM_event_add_notifier(C, NC_GEOM | ND_DATA, &grease_pencil);
    WM_event_add_notifier(C, NC_GPENCIL | NA_EDITED, nullptr);
  }

  return OPERATOR_FINISHED;
}

static void GREASE_PENCIL_OT_insert_blank_frame(wmOperatorType *ot)
{
  PropertyRNA *prop;

  /* identifiers */
  ot->name = "Insert Blank Frame";
  ot->idname = "GREASE_PENCIL_OT_insert_blank_frame";
  ot->description = "Insert a blank frame on the current scene frame";

  /* callbacks */
  ot->exec = insert_blank_frame_exec;
  ot->poll = active_grease_pencil_poll;

  ot->flag = OPTYPE_REGISTER | OPTYPE_UNDO;

  /* properties */
  prop = RNA_def_boolean(
      ot->srna, "all_layers", false, "All Layers", "Insert a blank frame in all editable layers");
  RNA_def_property_flag(prop, PROP_SKIP_SAVE);
  RNA_def_int(ot->srna, "duration", 0, 0, MAXFRAME, "Duration", "", 0, 100);
}

}  // namespace blender::ed::greasepencil

void ED_operatortypes_grease_pencil_frames()
{
  using namespace blender::ed::greasepencil;
  WM_operatortype_append(GREASE_PENCIL_OT_insert_blank_frame);
}
