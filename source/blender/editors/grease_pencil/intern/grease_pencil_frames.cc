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

#include "ED_anim_api.hh"
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
    trans_data.duplicated_frames_buffer.add_overwrite(frame_number, frame_duplicate);

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

bool ensure_active_keyframe(const Scene &scene,
                            GreasePencil &grease_pencil,
                            bool &r_inserted_keyframe)
{
  const int current_frame = scene.r.cfra;
  bke::greasepencil::Layer &active_layer = *grease_pencil.get_active_layer();

  if (!active_layer.has_drawing_at(current_frame) && !blender::animrig::is_autokey_on(&scene)) {
    return false;
  }

  /* If auto-key is on and the drawing at the current frame starts before the current frame a new
   * keyframe needs to be inserted. */
  const bool is_first = active_layer.is_empty() ||
                        (active_layer.sorted_keys().first() > current_frame);
  const bool needs_new_drawing = is_first ||
                                 (*active_layer.frame_key_at(current_frame) < current_frame);

  if (blender::animrig::is_autokey_on(&scene) && needs_new_drawing) {
    const Brush *brush = BKE_paint_brush_for_read(&scene.toolsettings->gp_paint->paint);
    if (((scene.toolsettings->gpencil_flags & GP_TOOL_FLAG_RETAIN_LAST) != 0) ||
        (brush->gpencil_tool == GPAINT_TOOL_ERASE))
    {
      /* For additive drawing, we duplicate the frame that's currently visible and insert it at the
       * current frame.
       * Note: Also duplicate the frame when erasing, Otherwise empty drawing is added, see
       * !119051.
       */
      grease_pencil.insert_duplicate_frame(
          active_layer, *active_layer.frame_key_at(current_frame), current_frame, false);
    }
    else {
      /* Otherwise we just insert a blank keyframe at the current frame. */
      grease_pencil.insert_frame(active_layer, current_frame);
    }
    r_inserted_keyframe = true;
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
      changed |= grease_pencil.insert_frame(*layer, current_frame, duration) != nullptr;
    }
  }
  else {
    if (!grease_pencil.has_active_layer()) {
      return OPERATOR_CANCELLED;
    }
    changed |= grease_pencil.insert_frame(
                   *grease_pencil.get_active_layer(), current_frame, duration) != nullptr;
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

bool grease_pencil_copy_keyframes(bAnimContext *ac, KeyframeClipboard &clipboard)
{
  using namespace bke::greasepencil;

  /* Clear buffer first. */
  clipboard.clear();

  /* Filter data. */
  const int filter = (ANIMFILTER_DATA_VISIBLE | ANIMFILTER_LIST_VISIBLE | ANIMFILTER_NODUPLIS);
  ListBase anim_data = {nullptr, nullptr};

  ANIM_animdata_filter(
      ac, &anim_data, eAnimFilter_Flags(filter), ac->data, eAnimCont_Types(ac->datatype));

  LISTBASE_FOREACH (bAnimListElem *, ale, &anim_data) {
    /* This function only deals with grease pencil layer frames.
     * This check is needed in the case of a call from the main dopesheet. */
    if (ale->type != ANIMTYPE_GREASE_PENCIL_LAYER) {
      continue;
    }

    GreasePencil *grease_pencil = reinterpret_cast<GreasePencil *>(ale->id);
    Layer *layer = reinterpret_cast<Layer *>(ale->data);
    Vector<KeyframeClipboard::DrawingBufferItem> buf;
    FramesMapKey layer_first_frame = std::numeric_limits<int>::max();
    FramesMapKey layer_last_frame = std::numeric_limits<int>::min();
    for (auto [frame_number, frame] : layer->frames().items()) {
      if (frame.is_selected()) {
        const Drawing *drawing = grease_pencil->get_drawing_at(*layer, frame_number);
        const int duration = layer->get_frame_duration_at(frame_number);
        buf.append(
            {frame_number, Drawing(*drawing), duration, eBezTriple_KeyframeType(frame.type)});

        /* Check the range of this layer only. */
        if (frame_number < layer_first_frame) {
          layer_first_frame = frame_number;
        }
        if (frame_number > layer_last_frame) {
          layer_last_frame = frame_number;
        }
      }
    }
    if (!buf.is_empty()) {
      BLI_assert(!clipboard.copy_buffer.contains(layer->name()));
      clipboard.copy_buffer.add_new(layer->name(), {buf, layer_first_frame, layer_last_frame});
      /* Update the range of entire copy buffer. */
      if (layer_first_frame < clipboard.first_frame) {
        clipboard.first_frame = layer_first_frame;
      }
      if (layer_last_frame > clipboard.last_frame) {
        clipboard.last_frame = layer_last_frame;
      }
    }
  }

  /* In case 'relative' paste method is used. */
  clipboard.cfra = ac->scene->r.cfra;

  /* Clean up. */
  ANIM_animdata_freelist(&anim_data);

  /* If nothing ended up in the buffer, copy failed. */
  return !clipboard.copy_buffer.is_empty();
}

static int calculate_offset(const eKeyPasteOffset offset_mode,
                            const int cfra,
                            const KeyframeClipboard &clipboard)
{
  int offset = 0;
  switch (offset_mode) {
    case KEYFRAME_PASTE_OFFSET_CFRA_START:
      offset = (cfra - clipboard.first_frame);
      break;
    case KEYFRAME_PASTE_OFFSET_CFRA_END:
      offset = (cfra - clipboard.last_frame);
      break;
    case KEYFRAME_PASTE_OFFSET_CFRA_RELATIVE:
      offset = (cfra - clipboard.cfra);
      break;
    case KEYFRAME_PASTE_OFFSET_NONE:
      offset = 0;
      break;
  }
  return offset;
}

bool grease_pencil_paste_keyframes(bAnimContext *ac,
                                   const eKeyPasteOffset offset_mode,
                                   const eKeyMergeMode merge_mode,
                                   const KeyframeClipboard &clipboard)
{
  using namespace bke::greasepencil;

  /* Check if buffer is empty. */
  if (clipboard.copy_buffer.is_empty()) {
    return false;
  }

  const int offset = calculate_offset(offset_mode, ac->scene->r.cfra, clipboard);

  const int filter = (ANIMFILTER_DATA_VISIBLE | ANIMFILTER_LIST_VISIBLE | ANIMFILTER_NODUPLIS |
                      ANIMFILTER_FOREDIT | ANIMFILTER_SEL);
  ListBase anim_data = {nullptr, nullptr};

  ANIM_animdata_filter(
      ac, &anim_data, eAnimFilter_Flags(filter), ac->data, eAnimCont_Types(ac->datatype));

  /* Check if single channel in buffer (disregard names if so). */
  const bool from_single_channel = clipboard.copy_buffer.size() == 1;

  LISTBASE_FOREACH (bAnimListElem *, ale, &anim_data) {
    /* Only deal with GPlayers (case of calls from general dopesheet). */
    if (ale->type != ANIMTYPE_GREASE_PENCIL_LAYER) {
      continue;
    }
    GreasePencil *grease_pencil = reinterpret_cast<GreasePencil *>(ale->id);
    Layer &layer = *reinterpret_cast<Layer *>(ale->data);
    const std::string layer_name = layer.name();
    if (!from_single_channel && !clipboard.copy_buffer.contains(layer_name)) {
      continue;
    }
    const KeyframeClipboard::LayerBufferItem layer_buffer =
        from_single_channel ? *clipboard.copy_buffer.values().begin() :
                              clipboard.copy_buffer.lookup(layer_name);
    bool changed = false;

    /* Mix mode with existing data. */
    switch (merge_mode) {
      case KEYFRAME_PASTE_MERGE_MIX:
        /* Do nothing. */
        break;

      case KEYFRAME_PASTE_MERGE_OVER: {
        /* Remove all keys. */
        Vector<int> frames_to_remove;
        for (auto frame_number : layer.frames().keys()) {
          frames_to_remove.append(frame_number);
        }
        grease_pencil->remove_frames(layer, frames_to_remove);
        changed = true;
        break;
      }
      case KEYFRAME_PASTE_MERGE_OVER_RANGE:
      case KEYFRAME_PASTE_MERGE_OVER_RANGE_ALL: {
        int frame_min, frame_max;

        if (merge_mode == KEYFRAME_PASTE_MERGE_OVER_RANGE) {
          /* Entire range of this layer. */
          frame_min = layer_buffer.first_frame + offset;
          frame_max = layer_buffer.last_frame + offset;
        }
        else {
          /* Entire range of all copied keys. */
          frame_min = clipboard.first_frame + offset;
          frame_max = clipboard.last_frame + offset;
        }

        /* Remove keys in range. */
        if (frame_min < frame_max) {
          Vector<int> frames_to_remove;
          for (auto frame_number : layer.frames().keys()) {
            if (frame_min < frame_number && frame_number < frame_max) {
              frames_to_remove.append(frame_number);
            }
          }
          grease_pencil->remove_frames(layer, frames_to_remove);
          changed = true;
        }
        break;
      }
    }
    for (const KeyframeClipboard::DrawingBufferItem &item : layer_buffer.drawing_buffers) {
      const int target_frame_number = item.frame_number + offset;
      if (layer.frames().contains(target_frame_number)) {
        grease_pencil->remove_frames(layer, {target_frame_number});
      }
      Drawing &dst_drawing = *grease_pencil->insert_frame(
          layer, target_frame_number, item.duration, item.keytype);
      dst_drawing = item.drawing;
      changed = true;
    }

    if (changed) {
      DEG_id_tag_update(&grease_pencil->id, ID_RECALC_GEOMETRY);
    }
  }

  /* Clean up. */
  ANIM_animdata_freelist(&anim_data);

  return true;
}

}  // namespace blender::ed::greasepencil

void ED_operatortypes_grease_pencil_frames()
{
  using namespace blender::ed::greasepencil;
  WM_operatortype_append(GREASE_PENCIL_OT_insert_blank_frame);
}
