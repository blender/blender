/* SPDX-FileCopyrightText: 2024 Blender Authors
 *
 * SPDX-License-Identifier: GPL-2.0-or-later */

/** \file
 * \ingroup modifiers
 */

#include "BLI_index_range.hh"
#include "BLI_map.hh"
#include "BLI_span.hh"
#include "BLI_string.h"
#include "BLI_string_utf8.h"
#include "BLI_vector_set.hh"

#include "DNA_defaults.h"
#include "DNA_modifier_types.h"
#include "DNA_scene_types.h"

#include "BKE_curves.hh"
#include "BKE_geometry_set.hh"
#include "BKE_grease_pencil.hh"
#include "BKE_instances.hh"
#include "BKE_modifier.hh"
#include "BKE_screen.hh"

#include "BLO_read_write.hh"

#include "DEG_depsgraph_query.hh"

#include "UI_interface.hh"
#include "UI_resources.hh"

#include "BLT_translation.hh"

#include "WM_api.hh"
#include "WM_types.hh"

#include "RNA_access.hh"
#include "RNA_prototypes.h"

#include "MOD_grease_pencil_util.hh"
#include "MOD_ui_common.hh"

#include <iostream>

namespace blender {

static void init_data(ModifierData *md)
{
  auto *tmd = reinterpret_cast<GreasePencilTimeModifierData *>(md);

  BLI_assert(MEMCMP_STRUCT_AFTER_IS_ZERO(tmd, modifier));

  MEMCPY_STRUCT_AFTER(tmd, DNA_struct_default_get(GreasePencilTimeModifierData), modifier);
  modifier::greasepencil::init_influence_data(&tmd->influence, false);

  GreasePencilTimeModifierSegment *segment = DNA_struct_default_alloc(
      GreasePencilTimeModifierSegment);
  STRNCPY_UTF8(segment->name, DATA_("Segment"));
  tmd->segments_array = segment;
  tmd->segments_num = 1;
}

static void copy_data(const ModifierData *md, ModifierData *target, const int flag)
{
  const auto *tmd = reinterpret_cast<const GreasePencilTimeModifierData *>(md);
  auto *tmmd = reinterpret_cast<GreasePencilTimeModifierData *>(target);

  modifier::greasepencil::free_influence_data(&tmmd->influence);

  BKE_modifier_copydata_generic(md, target, flag);
  modifier::greasepencil::copy_influence_data(&tmd->influence, &tmmd->influence, flag);

  tmmd->segments_array = static_cast<GreasePencilTimeModifierSegment *>(
      MEM_dupallocN(tmd->segments_array));
}

static void free_data(ModifierData *md)
{
  auto *tmd = reinterpret_cast<GreasePencilTimeModifierData *>(md);
  modifier::greasepencil::free_influence_data(&tmd->influence);

  MEM_SAFE_FREE(tmd->segments_array);
}

static void foreach_ID_link(ModifierData *md, Object *ob, IDWalkFunc walk, void *user_data)
{
  auto *tmd = reinterpret_cast<GreasePencilTimeModifierData *>(md);
  modifier::greasepencil::foreach_influence_ID_link(&tmd->influence, ob, walk, user_data);
}

struct FrameRange {
  /* Start frame. */
  int sfra;
  /* End frame (unlimited range when undefined). */
  int efra;

  bool is_empty() const
  {
    return efra < sfra;
  }

  bool is_single_frame() const
  {
    return efra == sfra;
  }

  int duration() const
  {
    return std::max(efra + 1 - sfra, 0);
  }

  FrameRange drop_front(const int n) const
  {
    BLI_assert(n >= 0);
    return FrameRange{std::min(sfra + n, efra), efra};
  }

  FrameRange drop_back(const int n) const
  {
    BLI_assert(n >= 0);
    return FrameRange{sfra, std::max(efra - n, sfra)};
  }

  FrameRange shift(const int n) const
  {
    return FrameRange{sfra + n, efra + n};
  }
};

/**
 * Find the index range of sorted keys that covers the frame range, including the key right before
 * and after the interval. The extra keys are needed when frames are held at the beginning or when
 * reversing the direction.
 */
static const IndexRange find_key_range(const Span<int> sorted_keys, const FrameRange &frame_range)
{
  IndexRange result = sorted_keys.index_range();
  for (const int i : result.index_range()) {
    const int irev = result.size() - 1 - i;
    if (sorted_keys[result[irev]] <= frame_range.sfra) {
      /* Found first key affecting the frame range, drop any earlier keys. */
      result = result.drop_front(irev);
      break;
    }
  }
  for (const int i : result.index_range()) {
    if (sorted_keys[result[i]] > frame_range.efra) {
      /* Found first key outside the frame range, drop this and later keys. */
      result = result.take_front(i);
      break;
    }
  }
  return result;
}

struct TimeMapping {
 private:
  float offset_;
  float scale_;
  bool use_loop_;

 public:
  TimeMapping(const GreasePencilTimeModifierData &tmd)
      : offset_(tmd.offset),
        scale_(tmd.frame_scale),
        use_loop_(tmd.flag & MOD_GREASE_PENCIL_TIME_KEEP_LOOP)
  {
  }

  float offset() const
  {
    return offset_;
  }
  float scale() const
  {
    return scale_;
  }
  bool use_loop() const
  {
    return use_loop_;
  }

  float to_scene_time(const float local_frame) const
  {
    return float(local_frame - offset_) / scale_;
  }
  float to_local_time(const float scene_frame) const
  {
    return scene_frame * scale_ + offset_;
  }

  /* Compute scene frame number on or after the local frame. */
  int scene_frame_before_local_frame(const int local_frame) const
  {
    return int(math::floor(to_scene_time(local_frame)));
  }
  /* Compute scene frame number on or after the local frame. */
  int scene_frame_after_local_frame(const int local_frame) const
  {
    return int(math::ceil(to_scene_time(local_frame)));
  }

  /* Compute local frame number on or before the scene frame. */
  int local_frame_before_scene_frame(const int scene_frame) const
  {
    return int(math::floor(to_local_time(scene_frame)));
  }
  /* Compute local frame number on or after the scene frame. */
  int local_frame_after_scene_frame(const int scene_frame) const
  {
    return int(math::ceil(to_local_time(scene_frame)));
  }
};

/* Determine how many times the source range must be repeated to cover the destination range. */
static void calculate_repetitions(const TimeMapping &mapping,
                                  const FrameRange &gp_src,
                                  const FrameRange &scene_dst,
                                  int &r_start,
                                  int &r_count)
{
  if (!mapping.use_loop()) {
    r_start = 0;
    r_count = 1;
    return;
  }
  const int duration = gp_src.duration();
  if (duration <= 0) {
    r_start = 0;
    r_count = 0;
    return;
  }
  const FrameRange gp_dst = {mapping.local_frame_before_scene_frame(scene_dst.sfra),
                             mapping.local_frame_after_scene_frame(scene_dst.efra)};
  r_start = math::floor(float(gp_dst.sfra - gp_src.sfra) / float(duration));
  r_count = math::floor(float(gp_dst.efra - gp_src.sfra) / float(duration)) + 1 - r_start;
}

static void insert_keys_forward(const TimeMapping &mapping,
                                const Map<int, GreasePencilFrame> &frames,
                                const Span<int> sorted_keys,
                                const FrameRange gp_src_range,
                                const FrameRange gp_dst_range,
                                Map<int, GreasePencilFrame> &dst_frames)
{
  const int offset = gp_dst_range.sfra - gp_src_range.sfra;
  for (const int i : sorted_keys.index_range()) {
    const int gp_key = sorted_keys[i];
    const int gp_start_key = std::max(gp_key, gp_src_range.sfra);
    if (gp_start_key > gp_src_range.efra) {
      continue;
    }

    const int scene_key = mapping.scene_frame_after_local_frame(gp_key + offset);
    dst_frames.add_overwrite(scene_key, frames.lookup(gp_key));
  }
}

/* Insert keys in reverse order. */
static void insert_keys_reverse(const TimeMapping &mapping,
                                const Map<int, GreasePencilFrame> &frames,
                                const Span<int> sorted_keys,
                                const FrameRange gp_src_range,
                                const FrameRange gp_dst_range,
                                Map<int, GreasePencilFrame> &dst_frames)
{
  const int offset = gp_dst_range.sfra - gp_src_range.sfra;
  for (const int i : sorted_keys.index_range()) {
    /* In reverse mode keys need to be inserted in reverse order to ensure "earlier" frames can
     * overwrite "later" frames. */
    const int irev = sorted_keys.size() - 1 - i;
    /* This finds the correct scene frame starting at the end of the frame interval. */
    const int gp_key = sorted_keys[irev];
    /* The insertion scene time is the end of the keyframe interval instead of the start.
     * This is the frame after the end frame (efra) to cover the full extent of the end frame
     * interval. */
    const int gp_end_key = (irev < sorted_keys.size() - 1) ?
                               std::min(sorted_keys[irev + 1], gp_src_range.efra + 1) :
                               gp_src_range.efra + 1;
    if (gp_end_key < gp_src_range.sfra) {
      return;
    }

    /* Reverse key frame inside the range. */
    const int gp_key_rev = gp_src_range.efra + 1 - (gp_end_key - gp_src_range.sfra);
    const int scene_key = mapping.scene_frame_after_local_frame(gp_key_rev + offset);
    dst_frames.add_overwrite(scene_key, frames.lookup(gp_key));
  }
}

static void fill_scene_range_fixed(const TimeMapping &mapping,
                                   const Map<int, GreasePencilFrame> &frames,
                                   const Span<int> sorted_keys,
                                   const int gp_src_frame,
                                   const FrameRange scene_dst_range,
                                   Map<int, GreasePencilFrame> &dst_frames)
{
  const FrameRange gp_src_range = {gp_src_frame, gp_src_frame};
  const FrameRange gp_dst_range = {mapping.local_frame_before_scene_frame(scene_dst_range.sfra),
                                   mapping.local_frame_after_scene_frame(scene_dst_range.efra)};

  const Span<int> src_keys = sorted_keys.slice(find_key_range(sorted_keys, gp_src_range));
  insert_keys_forward(mapping, frames, src_keys, gp_src_range, gp_dst_range, dst_frames);
}

static void fill_scene_range_forward(const TimeMapping &mapping,
                                     const Map<int, GreasePencilFrame> &frames,
                                     const Span<int> sorted_keys,
                                     const FrameRange gp_src_range,
                                     const FrameRange scene_dst_range,
                                     Map<int, GreasePencilFrame> &dst_frames)
{
  int repeat_start = 0, repeat_count = 1;
  calculate_repetitions(mapping, gp_src_range, scene_dst_range, repeat_start, repeat_count);

  const Span<int> src_keys = sorted_keys.slice(find_key_range(sorted_keys, gp_src_range));
  FrameRange gp_dst_range = gp_src_range.shift(repeat_start * gp_src_range.duration());
  for ([[maybe_unused]] const int repeat_i : IndexRange(repeat_count)) {
    insert_keys_forward(mapping, frames, src_keys, gp_src_range, gp_dst_range, dst_frames);
    gp_dst_range = gp_dst_range.shift(gp_src_range.duration());
  }
}

static void fill_scene_range_reverse(const TimeMapping &mapping,
                                     const Map<int, GreasePencilFrame> &frames,
                                     const Span<int> sorted_keys,
                                     const FrameRange gp_src_range,
                                     const FrameRange scene_dst_range,
                                     Map<int, GreasePencilFrame> &dst_frames)
{
  int repeat_start = 0, repeat_count = 1;
  calculate_repetitions(mapping, gp_src_range, scene_dst_range, repeat_start, repeat_count);

  const Span<int> src_keys = sorted_keys.slice(find_key_range(sorted_keys, gp_src_range));
  FrameRange gp_dst_range = gp_src_range.shift(repeat_start * gp_src_range.duration());
  for ([[maybe_unused]] const int repeat_i : IndexRange(repeat_count)) {
    insert_keys_reverse(mapping, frames, src_keys, gp_src_range, gp_dst_range, dst_frames);
    gp_dst_range = gp_dst_range.shift(gp_src_range.duration());
  }
}

static void fill_scene_range_ping_pong(const TimeMapping &mapping,
                                       const Map<int, GreasePencilFrame> &frames,
                                       const Span<int> sorted_keys,
                                       const FrameRange gp_src_range,
                                       const FrameRange scene_dst_range,
                                       Map<int, GreasePencilFrame> &dst_frames)
{
  /* Double interval for ping-pong mode, start and end frame only appear once. */
  const FrameRange gp_src_range_ping = {gp_src_range.sfra, gp_src_range.efra - 1};
  const FrameRange gp_src_range_pong = {gp_src_range.sfra + 1, gp_src_range.efra};
  const FrameRange gp_range_full = {gp_src_range.sfra,
                                    2 * gp_src_range.efra - gp_src_range.sfra - 1};
  int repeat_start = 0, repeat_count = 1;
  calculate_repetitions(mapping, gp_range_full, scene_dst_range, repeat_start, repeat_count);

  const Span<int> src_keys = sorted_keys.slice(find_key_range(sorted_keys, gp_src_range));
  FrameRange gp_dst_range = gp_src_range.shift(repeat_start * gp_range_full.duration());
  for ([[maybe_unused]] const int repeat_i : IndexRange(repeat_count)) {
    /* Ping. */
    insert_keys_forward(mapping, frames, src_keys, gp_src_range, gp_dst_range, dst_frames);
    gp_dst_range = gp_dst_range.shift(gp_src_range_ping.duration());
    /* Pong. */
    insert_keys_reverse(mapping, frames, src_keys, gp_src_range, gp_dst_range, dst_frames);
    gp_dst_range = gp_dst_range.shift(gp_src_range_pong.duration());
  }
}

static void fill_scene_range_chain(const TimeMapping &mapping,
                                   const Map<int, GreasePencilFrame> &frames,
                                   const Span<int> sorted_keys,
                                   const Span<GreasePencilTimeModifierSegment> segments,
                                   const FrameRange gp_src_range,
                                   const FrameRange scene_dst_range,
                                   Map<int, GreasePencilFrame> &dst_frames)
{
  using Segment = GreasePencilTimeModifierSegment;

  if (segments.is_empty()) {
    return;
  }
  /* Segment settings tolerate start frame after end frame. */
  auto segment_base_range = [](const Segment &segment) {
    return FrameRange{std::min(segment.segment_start, segment.segment_end),
                      std::max(segment.segment_start, segment.segment_end)};
  };
  auto segment_full_range = [](const Segment &segment) {
    const FrameRange base_range = FrameRange{std::min(segment.segment_start, segment.segment_end),
                                             std::max(segment.segment_start, segment.segment_end)};
    const int base_duration = (segment.segment_mode == MOD_GREASE_PENCIL_TIME_SEG_MODE_PINGPONG ?
                                   base_range.duration() * 2 - 2 :
                                   base_range.duration());
    return FrameRange{base_range.sfra,
                      base_range.sfra + segment.segment_repeat * base_duration - 1};
  };
  /* Find src range by adding up all segments. */
  const FrameRange gp_range_full = [&]() {
    int duration = segment_full_range(segments.first()).duration();
    for (const Segment &segment : segments.drop_front(1)) {
      duration += segment_full_range(segment).duration();
    }
    /* Same start as the source range. */
    return FrameRange{gp_src_range.sfra, gp_src_range.sfra + duration - 1};
  }();
  int repeat_start = 0, repeat_count = 1;
  calculate_repetitions(mapping, gp_range_full, scene_dst_range, repeat_start, repeat_count);

  const Span<int> src_keys = sorted_keys;

  FrameRange gp_dst_range = gp_src_range.shift(repeat_start * gp_range_full.duration());
  for ([[maybe_unused]] const int repeat_i : IndexRange(repeat_count)) {
    for (const Segment &segment : segments) {
      const FrameRange segment_src_range = segment_base_range(segment);
      for ([[maybe_unused]] const int segment_repeat_i : IndexRange(segment.segment_repeat)) {
        switch (GreasePencilTimeModifierSegmentMode(segment.segment_mode)) {
          case MOD_GREASE_PENCIL_TIME_SEG_MODE_NORMAL:
            insert_keys_forward(
                mapping, frames, src_keys, segment_src_range, gp_dst_range, dst_frames);
            gp_dst_range = gp_dst_range.shift(segment_src_range.duration());
            break;
          case MOD_GREASE_PENCIL_TIME_SEG_MODE_REVERSE:
            insert_keys_reverse(
                mapping, frames, src_keys, segment_src_range, gp_dst_range, dst_frames);
            gp_dst_range = gp_dst_range.shift(segment_src_range.duration());
            break;
          case MOD_GREASE_PENCIL_TIME_SEG_MODE_PINGPONG: {
            /* Ping. */
            const FrameRange segment_src_range_ping = {segment_src_range.sfra,
                                                       segment_src_range.efra - 1};
            insert_keys_forward(
                mapping, frames, src_keys, segment_src_range_ping, gp_dst_range, dst_frames);
            gp_dst_range = gp_dst_range.shift(segment_src_range_ping.duration());
            /* Pong. */
            const FrameRange segment_src_range_pong = {segment_src_range.sfra + 1,
                                                       segment_src_range.efra};
            insert_keys_reverse(
                mapping, frames, src_keys, segment_src_range_pong, gp_dst_range, dst_frames);
            gp_dst_range = gp_dst_range.shift(segment_src_range_pong.duration());
            break;
          }
        }
      }
    }
  }
}

static void fill_scene_timeline(const GreasePencilTimeModifierData &tmd,
                                const Scene &eval_scene,
                                const Map<int, GreasePencilFrame> &frames,
                                const Span<int> sorted_keys,
                                const FrameRange scene_dst_range,
                                Map<int, GreasePencilFrame> &dst_frames)
{
  const TimeMapping mapping(tmd);
  const auto mode = GreasePencilTimeModifierMode(tmd.mode);
  const bool use_custom_range = tmd.flag & MOD_GREASE_PENCIL_TIME_CUSTOM_RANGE;

  const FrameRange scene_range = FrameRange{eval_scene.r.sfra, eval_scene.r.efra};
  const FrameRange custom_range = use_custom_range ? FrameRange{tmd.sfra, tmd.efra} : scene_range;

  switch (mode) {
    case MOD_GREASE_PENCIL_TIME_MODE_NORMAL:
      fill_scene_range_forward(
          tmd, frames, sorted_keys, custom_range, scene_dst_range, dst_frames);
      break;
    case MOD_GREASE_PENCIL_TIME_MODE_REVERSE:
      fill_scene_range_reverse(
          tmd, frames, sorted_keys, custom_range, scene_dst_range, dst_frames);
      break;
    case MOD_GREASE_PENCIL_TIME_MODE_FIX:
      fill_scene_range_fixed(tmd, frames, sorted_keys, tmd.offset, scene_dst_range, dst_frames);
      break;
    case MOD_GREASE_PENCIL_TIME_MODE_PINGPONG:
      fill_scene_range_ping_pong(
          tmd, frames, sorted_keys, custom_range, scene_dst_range, dst_frames);
      break;
    case MOD_GREASE_PENCIL_TIME_MODE_CHAIN:
      fill_scene_range_chain(
          tmd, frames, sorted_keys, tmd.segments(), scene_range, scene_dst_range, dst_frames);
      break;
  }
}

static void modify_geometry_set(ModifierData *md,
                                const ModifierEvalContext *ctx,
                                bke::GeometrySet *geometry_set)
{
  using bke::greasepencil::Drawing;
  using bke::greasepencil::Layer;

  auto *tmd = reinterpret_cast<GreasePencilTimeModifierData *>(md);
  const Scene *scene = DEG_get_evaluated_scene(ctx->depsgraph);
  /* Just include the current frame for now. The method can be applied to arbitrary ranges. */
  const FrameRange dst_keyframe_range = {scene->r.cfra, scene->r.cfra};

  if (!geometry_set->has_grease_pencil()) {
    return;
  }
  GreasePencil &grease_pencil = *geometry_set->get_grease_pencil_for_write();

  IndexMaskMemory mask_memory;
  const IndexMask layer_mask = modifier::greasepencil::get_filtered_layer_mask(
      grease_pencil, tmd->influence, mask_memory);

  const Span<Layer *> layers_for_write = grease_pencil.layers_for_write();
  layer_mask.foreach_index([&](const int64_t layer_i) {
    Layer &layer = *layers_for_write[layer_i];
    const Span<int> sorted_keys = layer.sorted_keys();
    const Map<int, GreasePencilFrame> &src_frames = layer.frames();

    Map<int, GreasePencilFrame> new_frames;
    fill_scene_timeline(*tmd, *scene, src_frames, sorted_keys, dst_keyframe_range, new_frames);
    layer.frames_for_write() = std::move(new_frames);
    layer.tag_frames_map_keys_changed();
  });
}

static void panel_draw(const bContext *C, Panel *panel)
{
  uiLayout *layout = panel->layout;

  PointerRNA ob_ptr;
  PointerRNA *ptr = modifier_panel_get_property_pointers(panel, &ob_ptr);
  auto *tmd = static_cast<GreasePencilTimeModifierData *>(ptr->data);
  const auto mode = GreasePencilTimeModifierMode(RNA_enum_get(ptr, "mode"));
  const bool use_fixed_offset = (mode == MOD_GREASE_PENCIL_TIME_MODE_FIX);
  const bool use_custom_range = !ELEM(
      mode, MOD_GREASE_PENCIL_TIME_MODE_FIX, MOD_GREASE_PENCIL_TIME_MODE_CHAIN);
  uiLayout *row, *col;

  uiLayoutSetPropSep(layout, true);

  uiItemR(layout, ptr, "mode", UI_ITEM_NONE, nullptr, ICON_NONE);

  col = uiLayoutColumn(layout, false);

  const char *text = use_fixed_offset ? IFACE_("Frame") : IFACE_("Frame Offset");
  uiItemR(col, ptr, "offset", UI_ITEM_NONE, text, ICON_NONE);

  row = uiLayoutRow(col, false);
  uiLayoutSetActive(row, !use_fixed_offset);
  uiItemR(row, ptr, "frame_scale", UI_ITEM_NONE, IFACE_("Scale"), ICON_NONE);

  row = uiLayoutRow(layout, false);
  uiLayoutSetActive(row, !use_fixed_offset);
  uiItemR(row, ptr, "use_keep_loop", UI_ITEM_NONE, nullptr, ICON_NONE);

  if (mode == MOD_GREASE_PENCIL_TIME_MODE_CHAIN) {
    row = uiLayoutRow(layout, false);
    uiLayoutSetPropSep(row, false);

    uiTemplateList(row,
                   (bContext *)C,
                   "MOD_UL_grease_pencil_time_modifier_segments",
                   "",
                   ptr,
                   "segments",
                   ptr,
                   "segment_active_index",
                   nullptr,
                   3,
                   10,
                   0,
                   1,
                   UI_TEMPLATE_LIST_FLAG_NONE);

    col = uiLayoutColumn(row, false);

    uiLayout *sub = uiLayoutColumn(col, true);
    uiItemO(sub, "", ICON_ADD, "OBJECT_OT_grease_pencil_time_modifier_segment_add");
    uiItemO(sub, "", ICON_REMOVE, "OBJECT_OT_grease_pencil_time_modifier_segment_remove");
    uiItemS(col);
    sub = uiLayoutColumn(col, true);
    uiItemEnumO_string(
        sub, "", ICON_TRIA_UP, "OBJECT_OT_grease_pencil_time_modifier_segment_move", "type", "UP");
    uiItemEnumO_string(sub,
                       "",
                       ICON_TRIA_DOWN,
                       "OBJECT_OT_grease_pencil_time_modifier_segment_move",
                       "type",
                       "DOWN");

    if (tmd->segments().index_range().contains(tmd->segment_active_index)) {
      PointerRNA segment_ptr = RNA_pointer_create(ptr->owner_id,
                                                  &RNA_GreasePencilTimeModifierSegment,
                                                  &tmd->segments()[tmd->segment_active_index]);

      sub = uiLayoutColumn(layout, true);
      uiItemR(sub, &segment_ptr, "segment_mode", UI_ITEM_NONE, nullptr, ICON_NONE);
      sub = uiLayoutColumn(layout, true);
      uiItemR(sub, &segment_ptr, "segment_start", UI_ITEM_NONE, nullptr, ICON_NONE);
      uiItemR(sub, &segment_ptr, "segment_end", UI_ITEM_NONE, nullptr, ICON_NONE);
      uiItemR(sub, &segment_ptr, "segment_repeat", UI_ITEM_NONE, nullptr, ICON_NONE);
    }
  }

  PanelLayout custom_range_panel_layout = uiLayoutPanelProp(
      C, layout, ptr, "open_custom_range_panel");
  if (uiLayout *header = custom_range_panel_layout.header) {
    uiLayoutSetPropSep(header, false);
    uiLayoutSetActive(header, use_custom_range);
    uiItemR(header, ptr, "use_custom_frame_range", UI_ITEM_NONE, nullptr, ICON_NONE);
  }
  if (uiLayout *body = custom_range_panel_layout.body) {
    uiLayoutSetPropSep(body, true);
    uiLayoutSetActive(body, use_custom_range && RNA_boolean_get(ptr, "use_custom_frame_range"));

    col = uiLayoutColumn(body, true);
    uiItemR(col, ptr, "frame_start", UI_ITEM_NONE, IFACE_("Frame Start"), ICON_NONE);
    uiItemR(col, ptr, "frame_end", UI_ITEM_NONE, IFACE_("End"), ICON_NONE);
  }

  if (uiLayout *influence_panel = uiLayoutPanelProp(
          C, layout, ptr, "open_influence_panel", "Influence"))
  {
    modifier::greasepencil::draw_layer_filter_settings(C, influence_panel, ptr);
  }

  modifier_panel_end(layout, ptr);
}

static void segment_list_item_draw(uiList * /*ui_list*/,
                                   const bContext * /*C*/,
                                   uiLayout *layout,
                                   PointerRNA * /*idataptr*/,
                                   PointerRNA *itemptr,
                                   int /*icon*/,
                                   PointerRNA * /*active_dataptr*/,
                                   const char * /*active_propname*/,
                                   int /*index*/,
                                   int /*flt_flag*/)
{
  uiLayout *row = uiLayoutRow(layout, true);
  uiItemR(row, itemptr, "name", UI_ITEM_R_NO_BG, "", ICON_NONE);
}

static void panel_register(ARegionType *region_type)
{
  modifier_panel_register(region_type, eModifierType_GreasePencilTime, panel_draw);

  uiListType *list_type = static_cast<uiListType *>(
      MEM_callocN(sizeof(uiListType), "Grease Pencil Time modifier segments"));
  STRNCPY(list_type->idname, "MOD_UL_grease_pencil_time_modifier_segments");
  list_type->draw_item = segment_list_item_draw;
  WM_uilisttype_add(list_type);
}

static void blend_write(BlendWriter *writer, const ID * /*id_owner*/, const ModifierData *md)
{
  const auto *tmd = reinterpret_cast<const GreasePencilTimeModifierData *>(md);

  BLO_write_struct(writer, GreasePencilTimeModifierData, tmd);
  modifier::greasepencil::write_influence_data(writer, &tmd->influence);

  BLO_write_struct_array(
      writer, GreasePencilTimeModifierSegment, tmd->segments_num, tmd->segments_array);
}

static void blend_read(BlendDataReader *reader, ModifierData *md)
{
  auto *tmd = reinterpret_cast<GreasePencilTimeModifierData *>(md);

  modifier::greasepencil::read_influence_data(reader, &tmd->influence);

  BLO_read_data_address(reader, &tmd->segments_array);
}

}  // namespace blender

ModifierTypeInfo modifierType_GreasePencilTime = {
    /*idname*/ "GreasePencilTime",
    /*name*/ N_("TimeOffset"),
    /*struct_name*/ "GreasePencilTimeModifierData",
    /*struct_size*/ sizeof(GreasePencilTimeModifierData),
    /*srna*/ &RNA_GreasePencilTimeModifier,
    /*type*/ ModifierTypeType::Nonconstructive,
    /*flags*/ eModifierTypeFlag_AcceptsGreasePencil | eModifierTypeFlag_SupportsEditmode |
        eModifierTypeFlag_EnableInEditmode | eModifierTypeFlag_SupportsMapping,
    /*icon*/ ICON_MOD_TIME,

    /*copy_data*/ blender::copy_data,

    /*deform_verts*/ nullptr,
    /*deform_matrices*/ nullptr,
    /*deform_verts_EM*/ nullptr,
    /*deform_matrices_EM*/ nullptr,
    /*modify_mesh*/ nullptr,
    /*modify_geometry_set*/ blender::modify_geometry_set,

    /*init_data*/ blender::init_data,
    /*required_data_mask*/ nullptr,
    /*free_data*/ blender::free_data,
    /*is_disabled*/ nullptr,
    /*update_depsgraph*/ nullptr,
    /*depends_on_time*/ nullptr,
    /*depends_on_normals*/ nullptr,
    /*foreach_ID_link*/ blender::foreach_ID_link,
    /*foreach_tex_link*/ nullptr,
    /*free_runtime_data*/ nullptr,
    /*panel_register*/ blender::panel_register,
    /*blend_write*/ blender::blend_write,
    /*blend_read*/ blender::blend_read,
};

blender::Span<GreasePencilTimeModifierSegment> GreasePencilTimeModifierData::segments() const
{
  return {this->segments_array, this->segments_num};
}

blender::MutableSpan<GreasePencilTimeModifierSegment> GreasePencilTimeModifierData::segments()
{
  return {this->segments_array, this->segments_num};
}
