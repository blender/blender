/* SPDX-FileCopyrightText: 2008 Blender Authors
 *
 * SPDX-License-Identifier: GPL-2.0-or-later */

/** \file
 * \ingroup spseq
 */

#pragma once

#include "DNA_sequence_types.h"
#include "RNA_access.hh"

/* Internal exports only. */

struct ARegion;
struct ARegionType;
struct Depsgraph;
struct wmGizmoGroupType;
struct wmGizmoType;
struct Main;
struct Scene;
struct SeqCollection;
struct Sequence;
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

#define DEFAULT_IMG_STRIP_LENGTH 25 /* XXX arbitrary but ok for now. */
#define OVERLAP_ALPHA 180

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

/* `sequencer_timeline_draw.cc` */

void draw_timeline_seq(const bContext *C, ARegion *region);
void draw_timeline_seq_display(const bContext *C, ARegion *region);
void color3ubv_from_seq(const Scene *curscene,
                        const Sequence *seq,
                        bool show_strip_color_tag,
                        uchar r_col[3]);

/* `sequencer_preview_draw.cc` */

void sequencer_draw_preview(const bContext *C,
                            Scene *scene,
                            ARegion *region,
                            SpaceSeq *sseq,
                            int timeline_frame,
                            int offset,
                            bool draw_overlay,
                            bool draw_backdrop);
bool sequencer_draw_get_transform_preview(SpaceSeq *sseq, Scene *scene);
int sequencer_draw_get_transform_preview_frame(Scene *scene);

void sequencer_special_update_set(Sequence *seq);
/* Get handle width in 2d-View space. */
float sequence_handle_size_get_clamped(const Scene *scene, Sequence *seq, float pixelx);

/* UNUSED */
/* void seq_reset_imageofs(SpaceSeq *sseq); */

/**
 * Rendering using opengl will change the current viewport/context.
 * This is why we need the \a region, to set back the render area.
 *
 * TODO: do not rely on such hack and just update the \a ibuf outside of
 * the UI drawing code.
 */
ImBuf *sequencer_ibuf_get(Main *bmain,
                          ARegion *region,
                          Depsgraph *depsgraph,
                          Scene *scene,
                          SpaceSeq *sseq,
                          int timeline_frame,
                          int frame_ofs,
                          const char *viewname);

/* `sequencer_thumbnails.cc` */

void last_displayed_thumbnails_list_free(void *val);
void draw_seq_strip_thumbnail(View2D *v2d,
                              const bContext *C,
                              Scene *scene,
                              Sequence *seq,
                              float y1,
                              float y2,
                              float pixelx,
                              float pixely);

/* sequencer_draw_channels.c */

void draw_channels(const bContext *C, ARegion *region);
void channel_draw_context_init(const bContext *C,
                               ARegion *region,
                               SeqChannelDrawContext *r_context);

/* `sequencer_edit.cc` */

void seq_rectf(const Scene *scene, Sequence *seq, rctf *rectf);
Sequence *find_nearest_seq(Scene *scene, View2D *v2d, int *hand, const int mval[2]);
Sequence *find_neighboring_sequence(Scene *scene, Sequence *test, int lr, int sel);
void recurs_sel_seq(Sequence *seq_meta);
int seq_effect_find_selected(Scene *scene,
                             Sequence *activeseq,
                             int type,
                             Sequence **r_selseq1,
                             Sequence **r_selseq2,
                             Sequence **r_selseq3,
                             const char **r_error_str);

/* Operator helpers. */
bool sequencer_edit_poll(bContext *C);
bool sequencer_edit_with_channel_region_poll(bContext *C);
bool sequencer_editing_initialized_and_active(bContext *C);
/* UNUSED */
/* bool sequencer_strip_poll( bContext *C); */
bool sequencer_strip_has_path_poll(bContext *C);
bool sequencer_view_has_preview_poll(bContext *C);
bool sequencer_view_preview_only_poll(const bContext *C);
bool sequencer_view_strips_poll(bContext *C);

/**
 * Returns collection with all strips presented to user. If operation is done in preview,
 * collection is limited to all presented strips that can produce image output.
 *
 * \param C: context
 * \return collection of strips (`Sequence`)
 */
SeqCollection *all_strips_from_context(bContext *C);

/**
 * Returns collection with selected strips presented to user. If operation is done in preview,
 * collection is limited to selected presented strips, that can produce image output at current
 * frame.
 *
 * \param C: context
 * \return collection of strips (`Sequence`)
 */
SeqCollection *selected_strips_from_context(bContext *C);

/* Externals. */

extern EnumPropertyItem sequencer_prop_effect_types[];
extern EnumPropertyItem prop_side_types[];

/* Operators. */

void SEQUENCER_OT_split(wmOperatorType *ot);
void SEQUENCER_OT_slip(wmOperatorType *ot);
void SEQUENCER_OT_mute(wmOperatorType *ot);
void SEQUENCER_OT_unmute(wmOperatorType *ot);
void SEQUENCER_OT_lock(wmOperatorType *ot);
void SEQUENCER_OT_unlock(wmOperatorType *ot);
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

void SEQUENCER_OT_change_effect_input(wmOperatorType *ot);
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

void SEQUENCER_OT_select_all(wmOperatorType *ot);
void SEQUENCER_OT_select(wmOperatorType *ot);
void SEQUENCER_OT_select_side_of_frame(wmOperatorType *ot);
void SEQUENCER_OT_select_more(wmOperatorType *ot);
void SEQUENCER_OT_select_less(wmOperatorType *ot);
void SEQUENCER_OT_select_linked(wmOperatorType *ot);
void SEQUENCER_OT_select_linked_pick(wmOperatorType *ot);
void SEQUENCER_OT_select_handles(wmOperatorType *ot);
void SEQUENCER_OT_select_side(wmOperatorType *ot);
void SEQUENCER_OT_select_box(wmOperatorType *ot);
void SEQUENCER_OT_select_inverse(wmOperatorType *ot);
void SEQUENCER_OT_select_grouped(wmOperatorType *ot);

/* `sequencer_add.cc` */

void SEQUENCER_OT_scene_strip_add(wmOperatorType *ot);
void SEQUENCER_OT_scene_strip_add_new(wmOperatorType *ot);
void SEQUENCER_OT_movie_strip_add(wmOperatorType *ot);
void SEQUENCER_OT_movieclip_strip_add(wmOperatorType *ot);
void SEQUENCER_OT_mask_strip_add(wmOperatorType *ot);
void SEQUENCER_OT_sound_strip_add(wmOperatorType *ot);
void SEQUENCER_OT_image_strip_add(wmOperatorType *ot);
void SEQUENCER_OT_effect_strip_add(wmOperatorType *ot);

/* `sequencer_drag_drop.cc` */

void sequencer_dropboxes();

/* `sequencer_ops.cc` */

void sequencer_operatortypes();
void sequencer_keymap(wmKeyConfig *keyconf);

/* sequencer_scope.c */

ImBuf *make_waveform_view_from_ibuf(ImBuf *ibuf);
ImBuf *make_sep_waveform_view_from_ibuf(ImBuf *ibuf);
ImBuf *make_vectorscope_view_from_ibuf(ImBuf *ibuf);
ImBuf *make_zebra_view_from_ibuf(ImBuf *ibuf, float perc);
ImBuf *make_histogram_view_from_ibuf(ImBuf *ibuf);

/* `sequencer_buttons.cc` */

void sequencer_buttons_register(ARegionType *art);

/* sequencer_modifiers.c */

void SEQUENCER_OT_strip_modifier_add(wmOperatorType *ot);
void SEQUENCER_OT_strip_modifier_remove(wmOperatorType *ot);
void SEQUENCER_OT_strip_modifier_move(wmOperatorType *ot);
void SEQUENCER_OT_strip_modifier_copy(wmOperatorType *ot);
void SEQUENCER_OT_strip_modifier_equalizer_redefine(wmOperatorType *ot);

/* `sequencer_view.cc` */

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

void sequencer_preview_add_sound(const bContext *C, Sequence *seq);

/* `sequencer_add.cc` */

int sequencer_image_seq_get_minmax_frame(wmOperator *op,
                                         int sfra,
                                         int *r_minframe,
                                         int *r_numdigits);
void sequencer_image_seq_reserve_frames(
    wmOperator *op, StripElem *se, int len, int minframe, int numdigits);

/* `sequencer_retiming.cc` */
void SEQUENCER_OT_retiming_reset(wmOperatorType *ot);
void SEQUENCER_OT_retiming_handle_move(wmOperatorType *ot);
void SEQUENCER_OT_retiming_handle_add(wmOperatorType *ot);
void SEQUENCER_OT_retiming_handle_remove(wmOperatorType *ot);
void SEQUENCER_OT_retiming_segment_speed_set(wmOperatorType *ot);

/* `sequencer_gizmo_retime.cc` */
void SEQUENCER_GGT_gizmo_retime(wmGizmoGroupType *gzgt);

/* `sequencer_gizmo_retime_type.cc` */
void GIZMO_GT_retime_handle_add(wmGizmoType *gzt);
void GIZMO_GT_retime_handle(wmGizmoType *gzt);
void GIZMO_GT_retime_remove(wmGizmoType *gzt);
void GIZMO_GT_speed_set_remove(wmGizmoType *gzt);
