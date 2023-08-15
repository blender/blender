/* SPDX-FileCopyrightText: 2008 Blender Authors
 *
 * SPDX-License-Identifier: GPL-2.0-or-later */

/** \file
 * \ingroup spseq
 */

#pragma once

#include "DNA_sequence_types.h"
#include "RNA_access.hh"

#ifdef __cplusplus
extern "C" {
#endif

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
struct wmOperator;
struct ScrArea;
struct Editing;
struct ListBase;

#define DEFAULT_IMG_STRIP_LENGTH 25 /* XXX arbitrary but ok for now. */
#define OVERLAP_ALPHA 180

typedef struct SeqChannelDrawContext {
  const struct bContext *C;
  struct ScrArea *area;
  struct ARegion *region;
  struct ARegion *timeline_region;
  struct View2D *v2d;
  struct View2D *timeline_region_v2d;

  struct Scene *scene;
  struct Editing *ed;
  struct ListBase *seqbase;  /* Displayed seqbase. */
  struct ListBase *channels; /* Displayed channels. */

  float draw_offset;
  float channel_height;
  float frame_width;
  float scale;
} SeqChannelDrawContext;

/* `sequencer_draw.cc` */

void draw_timeline_seq(const struct bContext *C, struct ARegion *region);
void draw_timeline_seq_display(const struct bContext *C, struct ARegion *region);
void sequencer_draw_preview(const struct bContext *C,
                            struct Scene *scene,
                            struct ARegion *region,
                            struct SpaceSeq *sseq,
                            int timeline_frame,
                            int offset,
                            bool draw_overlay,
                            bool draw_backdrop);
void color3ubv_from_seq(const struct Scene *curscene,
                        const struct Sequence *seq,
                        bool show_strip_color_tag,
                        uchar r_col[3]);

void sequencer_special_update_set(Sequence *seq);
/* Get handle width in 2d-View space. */
float sequence_handle_size_get_clamped(const struct Scene *scene,
                                       struct Sequence *seq,
                                       float pixelx);

/* UNUSED */
/* void seq_reset_imageofs(struct SpaceSeq *sseq); */

/**
 * Rendering using opengl will change the current viewport/context.
 * This is why we need the \a region, to set back the render area.
 *
 * TODO: do not rely on such hack and just update the \a ibuf outside of
 * the UI drawing code.
 */
struct ImBuf *sequencer_ibuf_get(struct Main *bmain,
                                 struct ARegion *region,
                                 struct Depsgraph *depsgraph,
                                 struct Scene *scene,
                                 struct SpaceSeq *sseq,
                                 int timeline_frame,
                                 int frame_ofs,
                                 const char *viewname);

/* `sequencer_thumbnails.cc` */

void last_displayed_thumbnails_list_free(void *val);
void draw_seq_strip_thumbnail(struct View2D *v2d,
                              const struct bContext *C,
                              struct Scene *scene,
                              struct Sequence *seq,
                              float y1,
                              float y2,
                              float pixelx,
                              float pixely);

/* sequencer_draw_channels.c */

void draw_channels(const struct bContext *C, struct ARegion *region);
void channel_draw_context_init(const struct bContext *C,
                               struct ARegion *region,
                               struct SeqChannelDrawContext *r_context);

/* `sequencer_edit.cc` */

struct View2D;
void seq_rectf(const struct Scene *scene, struct Sequence *seq, struct rctf *rectf);
struct Sequence *find_nearest_seq(struct Scene *scene,
                                  struct View2D *v2d,
                                  int *hand,
                                  const int mval[2]);
struct Sequence *find_neighboring_sequence(struct Scene *scene,
                                           struct Sequence *test,
                                           int lr,
                                           int sel);
void recurs_sel_seq(struct Sequence *seq_meta);
int seq_effect_find_selected(struct Scene *scene,
                             struct Sequence *activeseq,
                             int type,
                             struct Sequence **r_selseq1,
                             struct Sequence **r_selseq2,
                             struct Sequence **r_selseq3,
                             const char **r_error_str);

/* Operator helpers. */
bool sequencer_edit_poll(struct bContext *C);
bool sequencer_edit_with_channel_region_poll(struct bContext *C);
bool sequencer_editing_initialized_and_active(struct bContext *C);
/* UNUSED */
/* bool sequencer_strip_poll(struct bContext *C); */
bool sequencer_strip_has_path_poll(struct bContext *C);
bool sequencer_view_has_preview_poll(struct bContext *C);
bool sequencer_view_preview_only_poll(const struct bContext *C);
bool sequencer_view_strips_poll(struct bContext *C);

/**
 * Returns collection with all strips presented to user. If operation is done in preview,
 * collection is limited to all presented strips that can produce image output.
 *
 * \param C: context
 * \return collection of strips (`Sequence`)
 */
struct SeqCollection *all_strips_from_context(struct bContext *C);

/**
 * Returns collection with selected strips presented to user. If operation is done in preview,
 * collection is limited to selected presented strips, that can produce image output at current
 * frame.
 *
 * \param C: context
 * \return collection of strips (`Sequence`)
 */
struct SeqCollection *selected_strips_from_context(struct bContext *C);

/* Externs. */
extern EnumPropertyItem sequencer_prop_effect_types[];
extern EnumPropertyItem prop_side_types[];

/* Operators. */
struct wmKeyConfig;
struct wmOperatorType;

void SEQUENCER_OT_split(struct wmOperatorType *ot);
void SEQUENCER_OT_slip(struct wmOperatorType *ot);
void SEQUENCER_OT_mute(struct wmOperatorType *ot);
void SEQUENCER_OT_unmute(struct wmOperatorType *ot);
void SEQUENCER_OT_lock(struct wmOperatorType *ot);
void SEQUENCER_OT_unlock(struct wmOperatorType *ot);
void SEQUENCER_OT_reload(struct wmOperatorType *ot);
void SEQUENCER_OT_refresh_all(struct wmOperatorType *ot);
void SEQUENCER_OT_reassign_inputs(struct wmOperatorType *ot);
void SEQUENCER_OT_swap_inputs(struct wmOperatorType *ot);
void SEQUENCER_OT_duplicate(struct wmOperatorType *ot);
void SEQUENCER_OT_delete(struct wmOperatorType *ot);
void SEQUENCER_OT_offset_clear(struct wmOperatorType *ot);
void SEQUENCER_OT_images_separate(struct wmOperatorType *ot);
void SEQUENCER_OT_meta_toggle(struct wmOperatorType *ot);
void SEQUENCER_OT_meta_make(struct wmOperatorType *ot);
void SEQUENCER_OT_meta_separate(struct wmOperatorType *ot);

void SEQUENCER_OT_gap_remove(struct wmOperatorType *ot);
void SEQUENCER_OT_gap_insert(struct wmOperatorType *ot);
void SEQUENCER_OT_snap(struct wmOperatorType *ot);

void SEQUENCER_OT_strip_jump(struct wmOperatorType *ot);
void SEQUENCER_OT_swap(struct wmOperatorType *ot);
void SEQUENCER_OT_swap_data(struct wmOperatorType *ot);
void SEQUENCER_OT_rendersize(struct wmOperatorType *ot);

void SEQUENCER_OT_change_effect_input(struct wmOperatorType *ot);
void SEQUENCER_OT_change_effect_type(struct wmOperatorType *ot);
void SEQUENCER_OT_change_path(struct wmOperatorType *ot);
void SEQUENCER_OT_change_scene(struct wmOperatorType *ot);

void SEQUENCER_OT_copy(struct wmOperatorType *ot);
void SEQUENCER_OT_paste(struct wmOperatorType *ot);

void SEQUENCER_OT_rebuild_proxy(struct wmOperatorType *ot);
void SEQUENCER_OT_enable_proxies(struct wmOperatorType *ot);

void SEQUENCER_OT_export_subtitles(struct wmOperatorType *ot);

void SEQUENCER_OT_set_range_to_strips(struct wmOperatorType *ot);
void SEQUENCER_OT_strip_transform_clear(struct wmOperatorType *ot);
void SEQUENCER_OT_strip_transform_fit(struct wmOperatorType *ot);

void SEQUENCER_OT_strip_color_tag_set(struct wmOperatorType *ot);
void SEQUENCER_OT_cursor_set(struct wmOperatorType *ot);
void SEQUENCER_OT_scene_frame_range_update(struct wmOperatorType *ot);

/* `sequencer_select.cc` */

void SEQUENCER_OT_select_all(struct wmOperatorType *ot);
void SEQUENCER_OT_select(struct wmOperatorType *ot);
void SEQUENCER_OT_select_side_of_frame(struct wmOperatorType *ot);
void SEQUENCER_OT_select_more(struct wmOperatorType *ot);
void SEQUENCER_OT_select_less(struct wmOperatorType *ot);
void SEQUENCER_OT_select_linked(struct wmOperatorType *ot);
void SEQUENCER_OT_select_linked_pick(struct wmOperatorType *ot);
void SEQUENCER_OT_select_handles(struct wmOperatorType *ot);
void SEQUENCER_OT_select_side(struct wmOperatorType *ot);
void SEQUENCER_OT_select_box(struct wmOperatorType *ot);
void SEQUENCER_OT_select_inverse(struct wmOperatorType *ot);
void SEQUENCER_OT_select_grouped(struct wmOperatorType *ot);

/* `sequencer_add.cc` */

void SEQUENCER_OT_scene_strip_add(struct wmOperatorType *ot);
void SEQUENCER_OT_scene_strip_add_new(struct wmOperatorType *ot);
void SEQUENCER_OT_movie_strip_add(struct wmOperatorType *ot);
void SEQUENCER_OT_movieclip_strip_add(struct wmOperatorType *ot);
void SEQUENCER_OT_mask_strip_add(struct wmOperatorType *ot);
void SEQUENCER_OT_sound_strip_add(struct wmOperatorType *ot);
void SEQUENCER_OT_image_strip_add(struct wmOperatorType *ot);
void SEQUENCER_OT_effect_strip_add(struct wmOperatorType *ot);

/* `sequencer_drag_drop.cc` */

void sequencer_dropboxes(void);

/* `sequencer_ops.cc` */

void sequencer_operatortypes(void);
void sequencer_keymap(struct wmKeyConfig *keyconf);

/* sequencer_scope.c */

struct ImBuf *make_waveform_view_from_ibuf(struct ImBuf *ibuf);
struct ImBuf *make_sep_waveform_view_from_ibuf(struct ImBuf *ibuf);
struct ImBuf *make_vectorscope_view_from_ibuf(struct ImBuf *ibuf);
struct ImBuf *make_zebra_view_from_ibuf(struct ImBuf *ibuf, float perc);
struct ImBuf *make_histogram_view_from_ibuf(struct ImBuf *ibuf);

/* `sequencer_buttons.cc` */

void sequencer_buttons_register(struct ARegionType *art);

/* sequencer_modifiers.c */

void SEQUENCER_OT_strip_modifier_add(struct wmOperatorType *ot);
void SEQUENCER_OT_strip_modifier_remove(struct wmOperatorType *ot);
void SEQUENCER_OT_strip_modifier_move(struct wmOperatorType *ot);
void SEQUENCER_OT_strip_modifier_copy(struct wmOperatorType *ot);

/* `sequencer_view.cc` */

void SEQUENCER_OT_sample(struct wmOperatorType *ot);
void SEQUENCER_OT_view_all(struct wmOperatorType *ot);
void SEQUENCER_OT_view_frame(struct wmOperatorType *ot);
void SEQUENCER_OT_view_all_preview(struct wmOperatorType *ot);
void SEQUENCER_OT_view_zoom_ratio(struct wmOperatorType *ot);
void SEQUENCER_OT_view_selected(struct wmOperatorType *ot);
void SEQUENCER_OT_view_ghost_border(struct wmOperatorType *ot);

/* `sequencer_channels_edit.cc` */

void SEQUENCER_OT_rename_channel(struct wmOperatorType *ot);

/* `sequencer_preview.cc` */

void sequencer_preview_add_sound(const struct bContext *C, struct Sequence *seq);

/* `sequencer_add.cc` */

int sequencer_image_seq_get_minmax_frame(struct wmOperator *op,
                                         int sfra,
                                         int *r_minframe,
                                         int *r_numdigits);
void sequencer_image_seq_reserve_frames(
    struct wmOperator *op, struct StripElem *se, int len, int minframe, int numdigits);

/* `sequencer_retiming.cc` */
void SEQUENCER_OT_retiming_reset(struct wmOperatorType *ot);
void SEQUENCER_OT_retiming_handle_move(struct wmOperatorType *ot);
void SEQUENCER_OT_retiming_handle_add(struct wmOperatorType *ot);
void SEQUENCER_OT_retiming_handle_remove(struct wmOperatorType *ot);
void SEQUENCER_OT_retiming_segment_speed_set(struct wmOperatorType *ot);

/* `sequencer_gizmo_retime.cc` */
void SEQUENCER_GGT_gizmo_retime(struct wmGizmoGroupType *gzgt);

/* `sequencer_gizmo_retime_type.cc` */
void GIZMO_GT_retime_handle_add(struct wmGizmoType *gzt);
void GIZMO_GT_retime_handle(struct wmGizmoType *gzt);
void GIZMO_GT_retime_remove(struct wmGizmoType *gzt);
void GIZMO_GT_speed_set_remove(struct wmGizmoType *gzt);

#ifdef __cplusplus
}
#endif
