/* SPDX-FileCopyrightText: 2022 Blender Authors
 *
 * SPDX-License-Identifier: GPL-2.0-or-later */

/** \file
 * \ingroup sequencer
 */

#include "MEM_guardedalloc.h"

#include "DNA_scene_types.h"
#include "DNA_screen_types.h"

#include "BKE_context.hh"
#include "BKE_screen.hh"

#include "BLI_math_base.h"
#include "BLI_string.h"
#include "BLI_utildefines.h"

#include "ED_screen.hh"

#include "GPU_matrix.hh"

#include "RNA_prototypes.hh"

#include "SEQ_channels.hh"
#include "SEQ_sequencer.hh"
#include "SEQ_time.hh"

#include "UI_interface.hh"
#include "UI_resources.hh"
#include "UI_view2d.hh"

#include "WM_api.hh"

/* Own include. */
#include "sequencer_intern.hh"

namespace blender::ed::vse {

static float draw_offset_get(const View2D *timeline_region_v2d)
{
  return timeline_region_v2d->cur.ymin;
}

static float channel_height_pixelspace_get(const View2D *timeline_region_v2d)
{
  return UI_view2d_view_to_region_y(timeline_region_v2d, 1.0f) -
         UI_view2d_view_to_region_y(timeline_region_v2d, 0.0f);
}

static float frame_width_pixelspace_get(const View2D *timeline_region_v2d)
{

  return UI_view2d_view_to_region_x(timeline_region_v2d, 1.0f) -
         UI_view2d_view_to_region_x(timeline_region_v2d, 0.0f);
}

static float icon_width_get(const SeqChannelDrawContext *context)
{
  return (U.widget_unit * 0.8 * context->scale);
}

static float widget_y_offset(const SeqChannelDrawContext *context)
{
  return ((context->channel_height / context->scale) - icon_width_get(context)) / 2;
}

static float channel_index_y_min(const SeqChannelDrawContext *context, const int index)
{
  float y = (index - context->draw_offset) * context->channel_height;
  y /= context->scale;
  return y;
}

static void displayed_channel_range_get(const SeqChannelDrawContext *context,
                                        int r_channel_range[2])
{
  /* Channel 0 is not usable, so should never be drawn. */
  r_channel_range[0] = max_ii(1, floor(context->timeline_region_v2d->cur.ymin));
  r_channel_range[1] = ceil(context->timeline_region_v2d->cur.ymax);

  rctf strip_boundbox;
  BLI_rctf_init(&strip_boundbox, 0.0f, 0.0f, 1.0f, r_channel_range[1]);
  seq::timeline_expand_boundbox(context->scene, context->seqbase, &strip_boundbox);
  CLAMP(r_channel_range[0], strip_boundbox.ymin, strip_boundbox.ymax);
  CLAMP(r_channel_range[1], strip_boundbox.ymin, seq::MAX_CHANNELS);
}

static std::string draw_channel_widget_tooltip(bContext * /*C*/,
                                               void *argN,
                                               const StringRef /*tip*/)
{
  char *dyn_tooltip = static_cast<char *>(argN);
  return dyn_tooltip;
}

static float draw_channel_widget_mute(const SeqChannelDrawContext *context,
                                      uiBlock *block,
                                      const int channel_index,
                                      const float offset)
{
  float y = channel_index_y_min(context, channel_index) + widget_y_offset(context);

  const float width = icon_width_get(context);
  SeqTimelineChannel *channel = seq::channel_get_by_index(context->channels, channel_index);
  const int icon = seq::channel_is_muted(channel) ? ICON_CHECKBOX_DEHLT : ICON_CHECKBOX_HLT;

  PointerRNA ptr = RNA_pointer_create_discrete(
      &context->scene->id, &RNA_SequenceTimelineChannel, channel);
  PropertyRNA *hide_prop = RNA_struct_type_find_property(&RNA_SequenceTimelineChannel, "mute");

  UI_block_emboss_set(block, ui::EmbossType::None);
  uiBut *but = uiDefIconButR_prop(block,
                                  ButType::Toggle,
                                  1,
                                  icon,
                                  context->v2d->cur.xmax / context->scale - offset,
                                  y,
                                  width,
                                  width,
                                  &ptr,
                                  hide_prop,
                                  0,
                                  0,
                                  0,
                                  std::nullopt);

  char *tooltip = BLI_sprintfN(
      "%s channel %d", seq::channel_is_muted(channel) ? "Unmute" : "Mute", channel_index);
  UI_but_func_tooltip_set(but, draw_channel_widget_tooltip, tooltip, MEM_freeN);

  return width;
}

static float draw_channel_widget_lock(const SeqChannelDrawContext *context,
                                      uiBlock *block,
                                      const int channel_index,
                                      const float offset)
{

  float y = channel_index_y_min(context, channel_index) + widget_y_offset(context);
  const float width = icon_width_get(context);

  SeqTimelineChannel *channel = seq::channel_get_by_index(context->channels, channel_index);
  const int icon = seq::channel_is_locked(channel) ? ICON_LOCKED : ICON_UNLOCKED;

  PointerRNA ptr = RNA_pointer_create_discrete(
      &context->scene->id, &RNA_SequenceTimelineChannel, channel);
  PropertyRNA *hide_prop = RNA_struct_type_find_property(&RNA_SequenceTimelineChannel, "lock");

  UI_block_emboss_set(block, ui::EmbossType::None);
  uiBut *but = uiDefIconButR_prop(block,
                                  ButType::Toggle,
                                  1,
                                  icon,
                                  context->v2d->cur.xmax / context->scale - offset,
                                  y,
                                  width,
                                  width,
                                  &ptr,
                                  hide_prop,
                                  0,
                                  0,
                                  0,
                                  "");

  char *tooltip = BLI_sprintfN(
      "%s channel %d", seq::channel_is_locked(channel) ? "Unlock" : "Lock", channel_index);
  UI_but_func_tooltip_set(but, draw_channel_widget_tooltip, tooltip, MEM_freeN);

  return width;
}

static bool channel_is_being_renamed(const SpaceSeq *sseq, const int channel_index)
{
  return sseq->runtime->rename_channel_index == channel_index;
}

static float text_size_get(const SeqChannelDrawContext *context)
{
  const uiStyle *style = UI_style_get_dpi();
  return UI_fontstyle_height_max(&style->widget) * 1.5f * context->scale;
}

/* TODO: decide what gets priority - label or buttons. */
static rctf label_rect_init(const SeqChannelDrawContext *context,
                            const int channel_index,
                            const float used_width)
{
  float text_size = text_size_get(context);
  float margin = (context->channel_height / context->scale - text_size) / 2.0f;
  float y = channel_index_y_min(context, channel_index) + margin;

  float margin_x = icon_width_get(context) * 0.65;
  float width = max_ff(0.0f, context->v2d->cur.xmax / context->scale - used_width);

  /* Text input has its own margin. Prevent text jumping around and use as much space as possible.
   */
  if (channel_is_being_renamed(CTX_wm_space_seq(context->C), channel_index)) {
    float input_box_margin = icon_width_get(context) * 0.5f;
    margin_x -= input_box_margin;
    width += input_box_margin;
  }

  rctf rect;
  BLI_rctf_init(&rect, margin_x, margin_x + width, y, y + text_size);
  return rect;
}

static void draw_channel_labels(const SeqChannelDrawContext *context,
                                uiBlock *block,
                                const int channel_index,
                                const float used_width)
{
  SpaceSeq *sseq = CTX_wm_space_seq(context->C);
  rctf rect = label_rect_init(context, channel_index, used_width);

  if (BLI_rctf_size_y(&rect) <= 1.0f || BLI_rctf_size_x(&rect) <= 1.0f) {
    return;
  }

  if (channel_is_being_renamed(sseq, channel_index)) {
    SeqTimelineChannel *channel = seq::channel_get_by_index(context->channels, channel_index);
    PointerRNA ptr = RNA_pointer_create_discrete(
        &context->scene->id, &RNA_SequenceTimelineChannel, channel);
    PropertyRNA *prop = RNA_struct_name_property(ptr.type);

    UI_block_emboss_set(block, ui::EmbossType::Emboss);
    uiBut *but = uiDefButR(block,
                           ButType::Text,
                           1,
                           "",
                           rect.xmin,
                           rect.ymin,
                           BLI_rctf_size_x(&rect),
                           BLI_rctf_size_y(&rect),
                           &ptr,
                           RNA_property_identifier(prop),
                           -1,
                           0,
                           0,
                           std::nullopt);
    UI_block_emboss_set(block, ui::EmbossType::None);

    if (UI_but_active_only(context->C, context->region, block, but) == false) {
      sseq->runtime->rename_channel_index = 0;
    }

    WM_event_add_notifier(context->C, NC_SCENE | ND_SEQUENCER, context->scene);
  }
  else {
    const char *label = seq::channel_name_get(context->channels, channel_index);
    uiDefBut(block,
             ButType::Label,
             0,
             label,
             rect.xmin,
             rect.ymin,
             rect.xmax - rect.xmin,
             (rect.ymax - rect.ymin),
             nullptr,
             0,
             0,
             std::nullopt);
  }
}

static void draw_channel_headers(const SeqChannelDrawContext *context)
{
  GPU_matrix_push();
  wmOrtho2_pixelspace(context->region->winx / context->scale,
                      context->region->winy / context->scale);
  uiBlock *block = UI_block_begin(context->C, context->region, __func__, ui::EmbossType::Emboss);

  int channel_range[2];
  displayed_channel_range_get(context, channel_range);

  const float icon_width = icon_width_get(context);
  const float offset_lock = icon_width * 1.5f;
  const float offset_mute = icon_width * 2.5f;
  const float offset_width = icon_width * 3.5f;
  /* Draw widgets separately from text labels so they are batched together,
   * instead of alternating between two fonts (regular and SVG/icons). */
  for (int channel = channel_range[0]; channel <= channel_range[1]; channel++) {
    draw_channel_widget_lock(context, block, channel, offset_lock);
    draw_channel_widget_mute(context, block, channel, offset_mute);
  }
  for (int channel = channel_range[0]; channel <= channel_range[1]; channel++) {
    draw_channel_labels(context, block, channel, offset_width);
  }

  UI_block_end(context->C, block);
  UI_block_draw(context->C, block);

  GPU_matrix_pop();
}

static void draw_background()
{
  UI_ThemeClearColor(TH_BACK);
}

void channel_draw_context_init(const bContext *C,
                               ARegion *region,
                               SeqChannelDrawContext *r_context)
{
  r_context->C = C;
  r_context->area = CTX_wm_area(C);
  r_context->region = region;
  r_context->v2d = &region->v2d;
  r_context->scene = CTX_data_sequencer_scene(C);
  r_context->ed = seq::editing_get(r_context->scene);
  r_context->seqbase = seq::active_seqbase_get(r_context->ed);
  r_context->channels = seq::channels_displayed_get(r_context->ed);
  r_context->timeline_region = BKE_area_find_region_type(r_context->area, RGN_TYPE_WINDOW);
  BLI_assert(r_context->timeline_region != nullptr);
  r_context->timeline_region_v2d = &r_context->timeline_region->v2d;

  r_context->channel_height = channel_height_pixelspace_get(r_context->timeline_region_v2d);
  r_context->frame_width = frame_width_pixelspace_get(r_context->timeline_region_v2d);
  r_context->draw_offset = draw_offset_get(r_context->timeline_region_v2d);

  r_context->scale = min_ff(r_context->channel_height / (U.widget_unit * 0.6), 1);
}

void draw_channels(const bContext *C, ARegion *region)
{
  draw_background();
  Scene *scene = CTX_data_sequencer_scene(C);
  if (!scene) {
    return;
  }

  Editing *ed = seq::editing_get(scene);
  if (ed == nullptr) {
    return;
  }

  SeqChannelDrawContext context;
  channel_draw_context_init(C, region, &context);

  if (round_fl_to_int(context.channel_height) == 0) {
    return;
  }

  UI_view2d_view_ortho(context.v2d);

  draw_channel_headers(&context);

  UI_view2d_view_restore(C);
}

}  // namespace blender::ed::vse
