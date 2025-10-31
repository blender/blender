/* SPDX-FileCopyrightText: 2008 Blender Authors
 *
 * SPDX-License-Identifier: GPL-2.0-or-later */

/** \file
 * \ingroup spseq
 */

#pragma once

#include "BLI_map.hh"
#include "BLI_span.hh"
#include "BLI_string_ref.hh"
#include "BLI_utility_mixins.hh"
#include "BLI_vector.hh"
#include "BLI_vector_set.hh"

#include "DNA_sequence_types.h"

#include "RNA_access.hh"

#include "GPU_viewport.hh"

#include "sequencer_scopes.hh"

/* Internal exports only. */

struct ARegion;
struct ARegionType;
struct ColorManagedViewSettings;
struct ColorManagedDisplaySettings;
struct Scene;
struct SeqRetimingKey;
struct Strip;
struct SpaceSeq;
struct StripElem;
struct View2D;
struct bContext;
struct rctf;
struct wmKeyConfig;
struct wmOperator;
struct wmOperatorType;
struct ScrArea;
struct Editing;
struct ListBase;

namespace blender::ed::asset {
struct AssetItemTree;
}

namespace blender::ed::vse {

class SeqQuadsBatch;
class StripsDrawBatch;

#define DEFAULT_IMG_STRIP_LENGTH 25 /* XXX arbitrary but ok for now. */

struct SpaceSeq_Runtime : public NonCopyable {
  int rename_channel_index = 0;
  float timeline_clamp_custom_range = 0;

  SeqScopes scopes;

  std::shared_ptr<asset::AssetItemTree> assets_for_menu;

  SpaceSeq_Runtime() = default;
  ~SpaceSeq_Runtime();
};

struct SeqChannelDrawContext {
  const bContext *C;
  ScrArea *area;
  ARegion *region;
  ARegion *timeline_region;
  View2D *v2d;
  View2D *timeline_region_v2d;

  Scene *scene;
  Editing *ed;
  ListBase *seqbase;  /* Displayed seqbase. */
  ListBase *channels; /* Displayed channels. */

  float draw_offset;
  float channel_height;
  float frame_width;
  float scale;
};

struct StripDrawContext {
  Strip *strip;
  const FCurve *curve = nullptr; /* Curve for overlay, if any (blend factor or volume). */

  /* Strip boundary in timeline space. Content start/end is clamped by left/right handle. */
  float content_start, content_end, bottom, top;
  float left_handle, right_handle; /* Position in frames. */
  float strip_content_top; /* Position in timeline space without content and text overlay. */
  float handle_width;      /* Width of strip handle in frames. */
  float strip_length;

  bool can_draw_text_overlay;
  bool can_draw_retiming_overlay;
  bool can_draw_strip_content;
  bool strip_is_too_small; /* Shorthand for (!can_draw_text_overlay && !can_draw_strip_content). */
  bool is_active_strip;
  bool is_single_image; /* Strip has single frame of content. */
  bool show_strip_color_tag;
  bool missing_data_block;
  bool missing_media;
  bool is_connected;
  bool is_muted;
};

struct TimelineDrawContext {
  const bContext *C;
  ARegion *region;
  Scene *scene;
  SpaceSeq *sseq;
  View2D *v2d;
  Editing *ed;
  ListBase *channels;
  GPUViewport *viewport;
  gpu::FrameBuffer *framebuffer_overlay;
  float pixelx, pixely; /* Width and height of pixel in timeline space. */
  Map<SeqRetimingKey *, Strip *> retiming_selection;

  SeqQuadsBatch *quads;
};

/* `sequencer_timeline_draw.cc` */

/* Returns value in frames (view-space), 5px for large strips, 1/4 of the strip for smaller. */
float strip_handle_draw_size_get(const Scene *scene, const Strip *strip, float pixelx);
void draw_timeline_seq(const bContext *C, const ARegion *region);
void draw_timeline_seq_display(const bContext *C, ARegion *region);

/* `sequencer_preview_draw.cc` */

/**
 * Draw callback for the sequencer preview region.
 *
 * It is supposed to be set as the draw function of the ARegionType corresponding to the preview
 * region.
 */
void sequencer_preview_region_draw(const bContext *C, ARegion *region);
void sequencer_special_update_set(Strip *strip);

/* UNUSED */
/* void seq_reset_imageofs(SpaceSeq *sseq); */

/**
 * Rendering using the GPU will change the current viewport/context.
 * This is why we need the \a region, to set back the render area.
 *
 * TODO: do not rely on such hack and just update the \a ibuf outside of
 * the UI drawing code.
 */
ImBuf *sequencer_ibuf_get(const bContext *C, int timeline_frame, const char *viewname);

/* `sequencer_thumbnails.cc` */

void draw_strip_thumbnails(const TimelineDrawContext &ctx,
                           StripsDrawBatch &strips_batch,
                           const Vector<StripDrawContext> &strips);

/* sequencer_draw_channels.c */

void draw_channels(const bContext *C, ARegion *region);
void channel_draw_context_init(const bContext *C,
                               ARegion *region,
                               SeqChannelDrawContext *r_context);

/* `sequencer_edit.cc` */

void slip_modal_keymap(wmKeyConfig *keyconf);
VectorSet<Strip *> strip_effect_get_new_inputs(const Scene *scene,
                                               int num_inputs,
                                               bool ignore_active = false);
StringRef effect_inputs_validate(const VectorSet<Strip *> &inputs, int num_inputs);

/* Operator helpers. */
bool sequencer_edit_poll(bContext *C);
bool sequencer_edit_with_channel_region_poll(bContext *C);
bool sequencer_editing_initialized_and_active(bContext *C);
/* UNUSED */
// bool sequencer_strip_poll( bContext *C);
bool sequencer_strip_editable_poll(bContext *C);
bool sequencer_strip_has_path_poll(bContext *C);
bool sequencer_view_has_preview_poll(bContext *C);
bool sequencer_view_preview_only_poll(const bContext *C);
bool sequencer_view_strips_poll(bContext *C);

/**
 * Returns collection with all strips presented to user. If operation is done in preview,
 * collection is limited to all presented strips that can produce image output.
 *
 * \param C: context
 * \return collection of strips (`Strip`)
 */
VectorSet<Strip *> all_strips_from_context(bContext *C);

/* Externals. */

extern const EnumPropertyItem sequencer_prop_effect_types[];
extern const EnumPropertyItem prop_side_types[];

/* Operators. */

void SEQUENCER_OT_split(wmOperatorType *ot);
void SEQUENCER_OT_slip(wmOperatorType *ot);
void SEQUENCER_OT_mute(wmOperatorType *ot);
void SEQUENCER_OT_unmute(wmOperatorType *ot);
void SEQUENCER_OT_lock(wmOperatorType *ot);
void SEQUENCER_OT_unlock(wmOperatorType *ot);
void SEQUENCER_OT_connect(wmOperatorType *ot);
void SEQUENCER_OT_disconnect(wmOperatorType *ot);
void SEQUENCER_OT_reload(wmOperatorType *ot);
void SEQUENCER_OT_refresh_all(wmOperatorType *ot);
void SEQUENCER_OT_reassign_inputs(wmOperatorType *ot);
void SEQUENCER_OT_swap_inputs(wmOperatorType *ot);
void SEQUENCER_OT_duplicate(wmOperatorType *ot);
void SEQUENCER_OT_delete(wmOperatorType *ot);
void SEQUENCER_OT_offset_clear(wmOperatorType *ot);
void SEQUENCER_OT_images_separate(wmOperatorType *ot);
void SEQUENCER_OT_meta_toggle(wmOperatorType *ot);
void SEQUENCER_OT_meta_make(wmOperatorType *ot);
void SEQUENCER_OT_meta_separate(wmOperatorType *ot);

void SEQUENCER_OT_gap_remove(wmOperatorType *ot);
void SEQUENCER_OT_gap_insert(wmOperatorType *ot);
void SEQUENCER_OT_snap(wmOperatorType *ot);

void SEQUENCER_OT_strip_jump(wmOperatorType *ot);
void SEQUENCER_OT_swap(wmOperatorType *ot);
void SEQUENCER_OT_swap_data(wmOperatorType *ot);
void SEQUENCER_OT_rendersize(wmOperatorType *ot);

void SEQUENCER_OT_change_effect_type(wmOperatorType *ot);
void SEQUENCER_OT_change_path(wmOperatorType *ot);
void SEQUENCER_OT_change_scene(wmOperatorType *ot);

void SEQUENCER_OT_copy(wmOperatorType *ot);
void SEQUENCER_OT_paste(wmOperatorType *ot);

void SEQUENCER_OT_rebuild_proxy(wmOperatorType *ot);
void SEQUENCER_OT_enable_proxies(wmOperatorType *ot);

void SEQUENCER_OT_export_subtitles(wmOperatorType *ot);

void SEQUENCER_OT_set_range_to_strips(wmOperatorType *ot);
void SEQUENCER_OT_strip_transform_clear(wmOperatorType *ot);
void SEQUENCER_OT_strip_transform_fit(wmOperatorType *ot);

void SEQUENCER_OT_strip_color_tag_set(wmOperatorType *ot);
void SEQUENCER_OT_cursor_set(wmOperatorType *ot);
void SEQUENCER_OT_scene_frame_range_update(wmOperatorType *ot);

/* `sequencer_select.cc` */

void strip_rectf(const Scene *scene, const Strip *strip, rctf *r_rect);
Strip *find_neighboring_strip(const Scene *scene, const Strip *test, const int lr, int sel);
void recurs_sel_strip(Strip *strip_meta);

void SEQUENCER_OT_select_all(wmOperatorType *ot);
void SEQUENCER_OT_select(wmOperatorType *ot);
void SEQUENCER_OT_select_handle(wmOperatorType *ot);
void SEQUENCER_OT_select_side_of_frame(wmOperatorType *ot);
void SEQUENCER_OT_select_more(wmOperatorType *ot);
void SEQUENCER_OT_select_less(wmOperatorType *ot);
void SEQUENCER_OT_select_linked(wmOperatorType *ot);
void SEQUENCER_OT_select_linked_pick(wmOperatorType *ot);
void SEQUENCER_OT_select_handles(wmOperatorType *ot);
void SEQUENCER_OT_select_side(wmOperatorType *ot);
void SEQUENCER_OT_select_box(wmOperatorType *ot);
void SEQUENCER_OT_select_lasso(wmOperatorType *ot);
void SEQUENCER_OT_select_circle(wmOperatorType *ot);
void SEQUENCER_OT_select_inverse(wmOperatorType *ot);
void SEQUENCER_OT_select_grouped(wmOperatorType *ot);

bool strip_point_image_isect(const Scene *scene, const Strip *strip, float point_view[2]);
void sequencer_select_do_updates(const bContext *C, Scene *scene);
/**
 * Returns the strip that intersects with the mouse cursor in the timeline, if applicable.

 * This check is more robust than simply comparing the timeline frame and channel, since strips do
 * not take up the full height of their channels (see #STRIP_OFSBOTTOM, #STRIP_OFSTOP).
 * Does not consider padded handles.
 *
 * \param mval: Mouse cursor location in regionspace
 * \return `Strip` that intersects with the cursor, or `nullptr` if not found
 */
Strip *strip_under_mouse_get(const Scene *scene, const View2D *v2d, const int mval[2]);

/* `sequencer_add.cc` */

void SEQUENCER_OT_scene_strip_add(wmOperatorType *ot);
void SEQUENCER_OT_scene_strip_add_new(wmOperatorType *ot);
void SEQUENCER_OT_movie_strip_add(wmOperatorType *ot);
void SEQUENCER_OT_movieclip_strip_add(wmOperatorType *ot);
void SEQUENCER_OT_mask_strip_add(wmOperatorType *ot);
void SEQUENCER_OT_sound_strip_add(wmOperatorType *ot);
void SEQUENCER_OT_image_strip_add(wmOperatorType *ot);
void SEQUENCER_OT_effect_strip_add(wmOperatorType *ot);
void SEQUENCER_OT_add_scene_strip_from_scene_asset(wmOperatorType *ot);

/* `sequencer_drag_drop.cc` */

void sequencer_dropboxes();

/* `sequencer_ops.cc` */

void sequencer_operatortypes();
void sequencer_keymap(wmKeyConfig *keyconf);

/* `sequencer_buttons.cc` */

void sequencer_buttons_register(ARegionType *art);

/* sequencer_modifiers.c */

void SEQUENCER_OT_strip_modifier_add(wmOperatorType *ot);
void SEQUENCER_OT_strip_modifier_remove(wmOperatorType *ot);
void SEQUENCER_OT_strip_modifier_move(wmOperatorType *ot);
void SEQUENCER_OT_strip_modifier_copy(wmOperatorType *ot);
void SEQUENCER_OT_strip_modifier_move_to_index(wmOperatorType *ot);
void SEQUENCER_OT_strip_modifier_set_active(wmOperatorType *ot);
void SEQUENCER_OT_strip_modifier_equalizer_redefine(wmOperatorType *ot);

/* `sequencer_view.cc` */

void SEQ_get_timeline_region_padding(const bContext *C, float *r_pad_top, float *r_pad_bottom);
void SEQ_add_timeline_region_padding(const bContext *C, rctf *view_box);

void SEQUENCER_OT_sample(wmOperatorType *ot);
void SEQUENCER_OT_view_all(wmOperatorType *ot);
void SEQUENCER_OT_view_frame(wmOperatorType *ot);
void SEQUENCER_OT_view_all_preview(wmOperatorType *ot);
void SEQUENCER_OT_view_zoom_ratio(wmOperatorType *ot);
void SEQUENCER_OT_view_selected(wmOperatorType *ot);
void SEQUENCER_OT_view_ghost_border(wmOperatorType *ot);

/* `sequencer_channels_edit.cc` */

void SEQUENCER_OT_rename_channel(wmOperatorType *ot);

/* `sequencer_preview.cc` */

void sequencer_preview_add_sound(const bContext *C, const Strip *strip);

/* `sequencer_add.cc` */

int sequencer_image_strip_get_minmax_frame(wmOperator *op,
                                           int sfra,
                                           int *r_minframe,
                                           int *r_numdigits);
void sequencer_image_strip_reserve_frames(
    wmOperator *op, StripElem *se, int len, int minframe, int numdigits);

/* `sequencer_retiming.cc` */
void SEQUENCER_OT_retiming_reset(wmOperatorType *ot);
void SEQUENCER_OT_retiming_show(wmOperatorType *ot);
void SEQUENCER_OT_retiming_key_add(wmOperatorType *ot);
void SEQUENCER_OT_retiming_freeze_frame_add(wmOperatorType *ot);
void SEQUENCER_OT_retiming_transition_add(wmOperatorType *ot);
void SEQUENCER_OT_retiming_key_delete(wmOperatorType *ot);
void SEQUENCER_OT_retiming_segment_speed_set(wmOperatorType *ot);
wmOperatorStatus sequencer_retiming_key_select_exec(bContext *C,
                                                    wmOperator *op,
                                                    SeqRetimingKey *key,
                                                    const Strip *key_owner);
/* Select a key and all following keys. */
wmOperatorStatus sequencer_retiming_select_linked_time(bContext *C,
                                                       wmOperator *op,
                                                       SeqRetimingKey *key,
                                                       const Strip *key_owner);
wmOperatorStatus sequencer_select_exec(bContext *C, wmOperator *op);
wmOperatorStatus sequencer_retiming_select_all_exec(bContext *C, wmOperator *op);
wmOperatorStatus sequencer_retiming_box_select_exec(bContext *C, wmOperator *op);

/* `sequencer_retiming_draw.cc` */
void sequencer_retiming_draw_continuity(const TimelineDrawContext &ctx,
                                        const StripDrawContext &strip_ctx);
void sequencer_retiming_keys_draw(const TimelineDrawContext &ctx, Span<StripDrawContext> strips);
void sequencer_retiming_speed_draw(const TimelineDrawContext &ctx,
                                   const StripDrawContext &strip_ctx);
void realize_fake_keys(const Scene *scene, Strip *strip);
SeqRetimingKey *try_to_realize_fake_keys(const bContext *C, Strip *strip, const int mval[2]);
SeqRetimingKey *retiming_mouseover_key_get(const bContext *C, const int mval[2], Strip **r_strip);
int left_fake_key_frame_get(const bContext *C, const Strip *strip);
int right_fake_key_frame_get(const bContext *C, const Strip *strip);
bool retiming_keys_can_be_displayed(const SpaceSeq *sseq);
rctf strip_retiming_keys_box_get(const Scene *scene, const View2D *v2d, const Strip *strip);

/* `sequencer_text_edit.cc` */
bool sequencer_text_editing_active_poll(bContext *C);
void SEQUENCER_OT_text_cursor_move(wmOperatorType *ot);
void SEQUENCER_OT_text_insert(wmOperatorType *ot);
void SEQUENCER_OT_text_delete(wmOperatorType *ot);
void SEQUENCER_OT_text_line_break(wmOperatorType *ot);
void SEQUENCER_OT_text_select_all(wmOperatorType *ot);
void SEQUENCER_OT_text_deselect_all(wmOperatorType *ot);
void SEQUENCER_OT_text_edit_mode_toggle(wmOperatorType *ot);
void SEQUENCER_OT_text_cursor_set(wmOperatorType *ot);
void SEQUENCER_OT_text_edit_copy(wmOperatorType *ot);
void SEQUENCER_OT_text_edit_paste(wmOperatorType *ot);
void SEQUENCER_OT_text_edit_cut(wmOperatorType *ot);
int2 strip_text_cursor_offset_to_position(const TextVarsRuntime *text, int cursor_offset);
IndexRange strip_text_selection_range_get(const TextVars *data);

/* `sequencer_timeline_draw.cc` */
Vector<Strip *> sequencer_visible_strips_get(const bContext *C);
Vector<Strip *> sequencer_visible_strips_get(const Scene *scene, const View2D *v2d);

/* `sequencer_clipboard.cc` */
wmOperatorStatus sequencer_clipboard_copy_exec(bContext *C, wmOperator *op);
wmOperatorStatus sequencer_clipboard_paste_exec(bContext *C, wmOperator *op);
wmOperatorStatus sequencer_clipboard_paste_invoke(bContext *C,
                                                  wmOperator *op,
                                                  const wmEvent *event);

/* `sequencer_add_menu_scene_assets.cc` */
MenuType add_catalog_assets_menu_type();
MenuType add_unassigned_assets_menu_type();
MenuType add_scene_menu_type();

}  // namespace blender::ed::vse
