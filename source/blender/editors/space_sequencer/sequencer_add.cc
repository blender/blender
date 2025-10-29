/* SPDX-FileCopyrightText: 2001-2002 NaN Holding BV. All rights reserved.
 *
 * SPDX-License-Identifier: GPL-2.0-or-later */

/** \file
 * \ingroup spseq
 */

#include <cctype>
#include <cstdlib>
#include <cstring>

#include "AS_asset_representation.hh"

#include "DNA_sequence_types.h"
#include "MEM_guardedalloc.h"

#include "BLI_listbase.h"
#include "BLI_math_base.h"
#include "BLI_path_utils.hh"
#include "BLI_string.h"
#include "BLI_string_utf8.h"
#include "BLI_utildefines.h"

#include "BLT_translation.hh"

#include "DNA_scene_types.h"
#include "DNA_space_types.h"

#include "BKE_context.hh"
#include "BKE_global.hh"
#include "BKE_main.hh"
#include "BKE_report.hh"
#include "BKE_scene.hh"

#include "IMB_imbuf_enums.h"

#include "SEQ_channels.hh"
#include "WM_api.hh"
#include "WM_types.hh"

#include "RNA_define.hh"
#include "RNA_enum_types.hh"
#include "RNA_prototypes.hh"

#include "SEQ_add.hh"
#include "SEQ_connect.hh"
#include "SEQ_effects.hh"
#include "SEQ_proxy.hh"
#include "SEQ_select.hh"
#include "SEQ_sequencer.hh"
#include "SEQ_time.hh"
#include "SEQ_transform.hh"

#include "ED_asset.hh"
#include "ED_asset_menu_utils.hh"
#include "ED_image.hh"
#include "ED_scene.hh"
#include "ED_screen.hh"
#include "ED_sequencer.hh"
#include "ED_time_scrub_ui.hh"

#include "UI_interface.hh"
#include "UI_interface_layout.hh"
#include "UI_view2d.hh"

#ifdef WITH_AUDASPACE
#  include <AUD_Sequence.h>
#endif

#include "DEG_depsgraph.hh"
#include "DEG_depsgraph_build.hh"

/* Own include. */
#include "sequencer_intern.hh"

namespace blender::ed::vse {

struct SequencerAddData {
  bool is_drop_event = false;
  ImageFormatData im_format;
};

/* Avoid passing multiple args and be more verbose. */
#define SEQPROP_STARTFRAME (1 << 0)
/* For image and effect strips only. */
#define SEQPROP_LENGTH (1 << 1)
/* Skips setting filepath or directory properties to active strip media directory,
 * since they have already been set by the file browser or by drag and drop. */
#define SEQPROP_NOPATHS (1 << 2)
/* Skips guessing channel for effect strips only. */
#define SEQPROP_NOCHAN (1 << 3)
#define SEQPROP_FIT_METHOD (1 << 4)
#define SEQPROP_VIEW_TRANSFORM (1 << 5)
#define SEQPROP_PLAYBACK_RATE (1 << 6)
#define SEQPROP_MOVE (1 << 7)

/* -------------------------------------------------------------------- */
/** \name Generic Add Functions
 * \{ */

static void sequencer_add_init(bContext * /*C*/, wmOperator *op)
{
  op->customdata = MEM_new<SequencerAddData>(__func__);
}

static void sequencer_add_free(bContext * /*C*/, wmOperator *op)
{
  if (op->customdata) {
    SequencerAddData *sad = reinterpret_cast<SequencerAddData *>(op->customdata);
    MEM_delete(sad);
    op->customdata = nullptr;
  }
}

static void sequencer_generic_props__internal(wmOperatorType *ot, int flag)
{
  PropertyRNA *prop;

  if (flag & SEQPROP_MOVE) {
    prop = RNA_def_boolean(
        ot->srna,
        "move_strips",
        true,
        "Move Strips",
        "Automatically begin translating strips with the mouse after adding them to the timeline");
    RNA_def_property_flag(prop, PROP_HIDDEN);
  }

  if (flag & SEQPROP_STARTFRAME) {
    RNA_def_int(ot->srna,
                "frame_start",
                0,
                INT_MIN,
                INT_MAX,
                "Start Frame",
                "Start frame of the strip",
                -MAXFRAME,
                MAXFRAME);
  }

  if (flag & SEQPROP_LENGTH) {
    /* Not usual since most strips have a predefined length. */
    RNA_def_int(ot->srna,
                "length",
                0,
                INT_MIN,
                INT_MAX,
                "Length",
                "Length of the strip in frames, or the length of each strip if multiple are added",
                -MAXFRAME,
                MAXFRAME);
  }

  RNA_def_int(ot->srna,
              "channel",
              1,
              1,
              seq::MAX_CHANNELS,
              "Channel",
              "Channel to place this strip into",
              1,
              seq::MAX_CHANNELS);

  RNA_def_boolean(ot->srna,
                  "replace_sel",
                  true,
                  "Replace Selection",
                  "Deselect previously selected strips after add operation completes");

  /* Only for python scripts which import strips and place them after. */
  prop = RNA_def_boolean(
      ot->srna, "overlap", false, "Allow Overlap", "Don't correct overlap on new strips");
  RNA_def_property_flag(prop, PROP_HIDDEN | PROP_SKIP_SAVE);

  prop = RNA_def_boolean(
      ot->srna,
      "overlap_shuffle_override",
      false,
      "Override Overlap Shuffle Behavior",
      "Use the overlap_mode tool settings to determine how to shuffle overlapping strips");
  RNA_def_property_flag(prop, PROP_HIDDEN | PROP_SKIP_SAVE);

  prop = RNA_def_boolean(ot->srna,
                         "skip_locked_or_muted_channels",
                         true,
                         "Skip Locked or Muted Channels",
                         "Add strips to muted or locked channels when adding movie strips");
  RNA_def_property_flag(prop, PROP_HIDDEN | PROP_SKIP_SAVE);

  if (flag & SEQPROP_FIT_METHOD) {
    ot->prop = RNA_def_enum(ot->srna,
                            "fit_method",
                            rna_enum_strip_scale_method_items,
                            SEQ_SCALE_TO_FIT,
                            "Fit Method",
                            "Mode for fitting the image to the canvas");
  }

  if (flag & SEQPROP_VIEW_TRANSFORM) {
    ot->prop = RNA_def_boolean(ot->srna,
                               "set_view_transform",
                               true,
                               "Set View Transform",
                               "Set appropriate view transform based on media color space");
  }

  if (flag & SEQPROP_PLAYBACK_RATE) {
    ot->prop = RNA_def_boolean(ot->srna,
                               "adjust_playback_rate",
                               true,
                               "Adjust Playback Rate",
                               "Play at normal speed regardless of scene FPS");
  }
}

static void sequencer_generic_invoke_path__internal(bContext *C,
                                                    wmOperator *op,
                                                    const char *identifier)
{
  if (RNA_struct_find_property(op->ptr, identifier)) {
    Scene *scene = CTX_data_sequencer_scene(C);
    Strip *last_strip = seq::select_active_get(scene);
    if (last_strip && last_strip->data && STRIP_HAS_PATH(last_strip)) {
      Main *bmain = CTX_data_main(C);
      char dirpath[FILE_MAX];
      STRNCPY(dirpath, last_strip->data->dirpath);
      BLI_path_abs(dirpath, ID_BLEND_PATH(bmain, &scene->id));
      RNA_string_set(op->ptr, identifier, dirpath);
    }
  }
}

static int find_unlocked_unmuted_channel(const Editing *ed, int channel_index)
{
  const ListBase *channels = seq::channels_displayed_get(ed);

  while (channel_index < seq::MAX_CHANNELS) {
    SeqTimelineChannel *channel = seq::channel_get_by_index(channels, channel_index);
    if (!seq::channel_is_muted(channel) && !seq::channel_is_locked(channel)) {
      break;
    }
    channel_index++;
  }

  return channel_index;
}

static int sequencer_generic_invoke_xy_guess_channel(bContext *C, int type)
{
  Strip *tgt = nullptr;
  Scene *scene = CTX_data_sequencer_scene(C);
  Editing *ed = seq::editing_ensure(scene);
  int timeline_frame = scene->r.cfra;
  int proximity = INT_MAX;

  if (!ed || !ed->current_strips()) {
    return 1;
  }

  LISTBASE_FOREACH (Strip *, strip, ed->current_strips()) {
    const int strip_end = seq::time_right_handle_frame_get(scene, strip);
    if (ELEM(type, -1, strip->type) && (strip_end <= timeline_frame) &&
        (timeline_frame - strip_end < proximity))
    {
      tgt = strip;
      proximity = timeline_frame - strip_end;
    }
  }

  int best_channel = 1;
  if (tgt) {
    best_channel = (type == STRIP_TYPE_MOVIE) ? tgt->channel - 1 : tgt->channel;
  }

  best_channel = find_unlocked_unmuted_channel(ed, best_channel);

  return math::clamp(best_channel, 0, seq::MAX_CHANNELS);
}

static bool have_free_channels(bContext *C,
                               wmOperator *op,
                               int need_channels,
                               const char **r_error_msg)
{
  const int channel = RNA_int_get(op->ptr, "channel");
  const int frame_start = RNA_int_get(op->ptr, "frame_start");

  /* First check simple case - strip is added to very top of timeline. */
  const int max_channel = seq::MAX_CHANNELS - need_channels + 1;
  if (channel > max_channel) {
    *r_error_msg = RPT_("No available channel for the current frame.");
    return false;
  }

  /* When adding strip(s) to lower channels, we must count number of free channels. There can be
   * gaps. */
  Set<int> used_channels;
  for (Strip *strip : all_strips_from_context(C)) {
    if (seq::time_strip_intersects_frame(CTX_data_sequencer_scene(C), strip, frame_start)) {
      used_channels.add(strip->channel);
    }
  }

  int free_channels = 0;
  for (int i : IndexRange(channel, seq::MAX_CHANNELS - channel + 1)) {
    if (!used_channels.contains(i)) {
      free_channels++;
    }
    if (free_channels == need_channels) {
      return true;
    }
  }

  *r_error_msg = RPT_("No available channel for the current frame.");
  return false;
}

/* Sets `channel` and `frame_start` properties when the operator is likely to have been invoked
 * with drag-and-drop data. */
static void sequencer_file_drop_channel_frame_set(bContext *C,
                                                  wmOperator *op,
                                                  const wmEvent *event)
{
  BLI_assert((RNA_struct_property_is_set(op->ptr, "files") &&
              !RNA_collection_is_empty(op->ptr, "files")) ||
             RNA_struct_property_is_set(op->ptr, "filepath"));

  if (RNA_struct_property_is_set(op->ptr, "channel") ||
      RNA_struct_property_is_set(op->ptr, "frame_start"))
  {
    return;
  }

  ARegion *region = CTX_wm_region(C);
  if (!region || region->regiontype != RGN_TYPE_WINDOW) {
    return;
  }

  float frame_start, channel;
  UI_view2d_region_to_view(&region->v2d, event->mval[0], event->mval[1], &frame_start, &channel);
  RNA_int_set(op->ptr, "channel", int(channel));
  RNA_int_set(op->ptr, "frame_start", int(frame_start));
}

static bool op_invoked_by_drop_event(const wmOperator *op)
{
  SequencerAddData *sad = reinterpret_cast<SequencerAddData *>(op->customdata);
  if (sad == nullptr) {
    return false;
  }
  return sad->is_drop_event;
}

static bool can_move_strips(const wmOperator *op)
{
  PropertyRNA *prop = RNA_struct_find_property(op->ptr, "move_strips");

  return prop != nullptr && RNA_property_boolean_get(op->ptr, prop) &&
         (op->flag & OP_IS_REPEAT) == 0 && !op_invoked_by_drop_event(op);
}

static void sequencer_generic_invoke_xy__internal(
    bContext *C, wmOperator *op, int flag, int type, const wmEvent *event)
{
  Scene *scene = CTX_data_sequencer_scene(C);

  int timeline_frame = scene->r.cfra;
  if (event && (flag & SEQPROP_NOPATHS)) {
    SequencerAddData *sad = reinterpret_cast<SequencerAddData *>(op->customdata);
    sad->is_drop_event = true;
    sequencer_file_drop_channel_frame_set(C, op, event);
  }

  /* Effect strips shouldn't have their channel guessed. Instead,
   * it will be set to the first free channel above the input strips. */
  if (!(flag & SEQPROP_NOCHAN) && !RNA_struct_property_is_set(op->ptr, "channel")) {
    RNA_int_set(op->ptr, "channel", sequencer_generic_invoke_xy_guess_channel(C, type));
  }

  if (!RNA_struct_property_is_set(op->ptr, "frame_start")) {
    RNA_int_set(op->ptr, "frame_start", timeline_frame);
  }

  if ((flag & SEQPROP_LENGTH) && !RNA_struct_property_is_set(op->ptr, "length")) {
    RNA_int_set(op->ptr, "length", DEFAULT_IMG_STRIP_LENGTH);
  }

  if (!(flag & SEQPROP_NOPATHS)) {
    sequencer_generic_invoke_path__internal(C, op, "filepath");
    sequencer_generic_invoke_path__internal(C, op, "directory");
  }
}

static void move_strips(bContext *C, wmOperator *op)
{
  if (!can_move_strips(op)) {
    return;
  }

  wmOperatorType *ot = WM_operatortype_find("TRANSFORM_OT_seq_slide", true);
  PointerRNA ptr;
  WM_operator_properties_create_ptr(&ptr, ot);
  RNA_boolean_set(&ptr, "remove_on_cancel", true);
  RNA_boolean_set(&ptr, "view2d_edge_pan", true);
  RNA_boolean_set(&ptr, "release_confirm", false);
  WM_operator_name_call_ptr(C, ot, wm::OpCallContext::InvokeDefault, &ptr, nullptr);
  WM_operator_properties_free(&ptr);
}

static bool load_data_init_from_operator(seq::LoadData *load_data, bContext *C, wmOperator *op)
{
  const Main *bmain = CTX_data_main(C);
  const ARegion *region = CTX_wm_region(C);

  memset(load_data, 0, sizeof(seq::LoadData));

  load_data->start_frame = RNA_int_get(op->ptr, "frame_start");
  load_data->channel = RNA_int_get(op->ptr, "channel");
  load_data->image.length = 1;
  load_data->image.count = 1;

  PropertyRNA *prop;
  if ((prop = RNA_struct_find_property(op->ptr, "fit_method"))) {
    load_data->fit_method = eSeqImageFitMethod(RNA_enum_get(op->ptr, "fit_method"));
    seq::tool_settings_fit_method_set(CTX_data_sequencer_scene(C), load_data->fit_method);
  }

  if ((prop = RNA_struct_find_property(op->ptr, "adjust_playback_rate"))) {
    load_data->adjust_playback_rate = RNA_boolean_get(op->ptr, "adjust_playback_rate");
  }

  if ((prop = RNA_struct_find_property(op->ptr, "filepath"))) {
    RNA_property_string_get(op->ptr, prop, load_data->path);
    /* File basename might be too long, better report it now than silently
     * truncating the basename later. */
    const char *basename = BLI_path_basename(load_data->path);
    if (strlen(basename) >= sizeof(StripElem::filename)) {
      BKE_reportf(op->reports,
                  RPT_ERROR,
                  "Filename '%s' too long (max length %zu, was %zu)",
                  basename,
                  sizeof(StripElem::filename),
                  strlen(basename));
      return false;
    }
    STRNCPY(load_data->name, basename);
  }
  else if ((prop = RNA_struct_find_property(op->ptr, "directory"))) {
    std::string directory = RNA_string_get(op->ptr, "directory");

    if ((prop = RNA_struct_find_property(op->ptr, "files"))) {
      RNA_PROP_BEGIN (op->ptr, itemptr, prop) {
        std::string filename = RNA_string_get(&itemptr, "name");
        STRNCPY(load_data->name, filename.c_str());
        BLI_path_join(
            load_data->path, sizeof(load_data->path), directory.c_str(), filename.c_str());
        break;
      }
      RNA_PROP_END;
    }
  }

  const bool relative = (prop = RNA_struct_find_property(op->ptr, "relative_path")) &&
                        RNA_property_boolean_get(op->ptr, prop);
  if (relative) {
    BLI_path_rel(load_data->path, BKE_main_blendfile_path(bmain));
  }

  if ((prop = RNA_struct_find_property(op->ptr, "length"))) {
    load_data->image.length = RNA_property_int_get(op->ptr, prop);
    load_data->effect.length = load_data->image.length;
  }

  if ((prop = RNA_struct_find_property(op->ptr, "cache")) &&
      RNA_property_boolean_get(op->ptr, prop))
  {
    load_data->flags |= seq::SEQ_LOAD_SOUND_CACHE;
  }

  if ((prop = RNA_struct_find_property(op->ptr, "mono")) &&
      RNA_property_boolean_get(op->ptr, prop))
  {
    load_data->flags |= seq::SEQ_LOAD_SOUND_MONO;
  }

  if ((prop = RNA_struct_find_property(op->ptr, "use_framerate")) &&
      RNA_property_boolean_get(op->ptr, prop))
  {
    load_data->flags |= seq::SEQ_LOAD_MOVIE_SYNC_FPS;
  }

  if ((prop = RNA_struct_find_property(op->ptr, "set_view_transform")) &&
      RNA_property_boolean_get(op->ptr, prop))
  {
    load_data->flags |= seq::SEQ_LOAD_SET_VIEW_TRANSFORM;
  }

  if ((prop = RNA_struct_find_property(op->ptr, "use_multiview")) &&
      RNA_property_boolean_get(op->ptr, prop))
  {
    if (op->customdata) {
      SequencerAddData *sad = reinterpret_cast<SequencerAddData *>(op->customdata);
      ImageFormatData *imf = &sad->im_format;

      load_data->use_multiview = true;
      load_data->views_format = imf->views_format;
      load_data->stereo3d_format = &imf->stereo3d_format;
    }
  }

  if (region == nullptr) {
    RNA_boolean_set(op->ptr, "move_strips", false);
  }

  /* Override strip position by current mouse position. */
  if (can_move_strips(op) && region != nullptr) {
    const wmWindow *win = CTX_wm_window(C);
    int2 mouse_region(win->eventstate->xy[0] - region->winrct.xmin,
                      win->eventstate->xy[1] - region->winrct.ymin);

    /* Clamp mouse cursor location (strip starting position) to the sequencer region bounds so that
     * it is immediately visible even if the mouse cursor is out of bounds. For maximums, use 90%
     * of the bounds instead of 1 frame away, which works well even if zoomed out. */
    const rcti mask = ED_time_scrub_clamp_scroller_mask(region->v2d.mask);
    rcti clamp_bounds;
    BLI_rcti_init(&clamp_bounds,
                  mask.xmin,
                  mask.xmin + 0.9 * BLI_rcti_size_x(&mask),
                  mask.ymin,
                  mask.ymin + 0.9 * BLI_rcti_size_y(&mask));
    BLI_rcti_clamp_pt_v(&clamp_bounds, mouse_region);

    float2 mouse_view;
    UI_view2d_region_to_view(
        &region->v2d, mouse_region.x, mouse_region.y, &mouse_view.x, &mouse_view.y);

    load_data->start_frame = std::trunc(mouse_view.x);
    load_data->channel = std::trunc(mouse_view.y);
    load_data->image.length = DEFAULT_IMG_STRIP_LENGTH;
    load_data->effect.length = load_data->image.length;
  }
  return true;
}

static void seq_load_apply_generic_options(bContext *C, wmOperator *op, Strip *strip)
{
  Scene *scene = CTX_data_sequencer_scene(C);
  Editing *ed = seq::editing_get(scene);

  if (strip == nullptr) {
    return;
  }

  if (RNA_boolean_get(op->ptr, "replace_sel")) {
    strip->flag |= SELECT;
    seq::select_active_set(scene, strip);
  }

  if (RNA_boolean_get(op->ptr, "overlap") == true ||
      !seq::transform_test_overlap(scene, ed->current_strips(), strip))
  {
    /* No overlap should be handled or the strip is not overlapping, exit early. */
    return;
  }

  if (RNA_boolean_get(op->ptr, "overlap_shuffle_override")) {
    /* Use set overlap_mode to fix overlaps. */
    VectorSet<Strip *> strip_col;
    strip_col.add(strip);

    ScrArea *area = CTX_wm_area(C);
    const bool use_sync_markers = (((SpaceSeq *)area->spacedata.first)->flag & SEQ_MARKER_TRANS) !=
                                  0;
    seq::transform_handle_overlap(scene, ed->current_strips(), strip_col, use_sync_markers);
  }
  else {
    /* Shuffle strip channel to fix overlaps. */
    seq::transform_seqbase_shuffle(ed->current_strips(), strip, scene);
  }
}

/* In this alternative version we only check for overlap, but do not do anything about them. */
static bool seq_load_apply_generic_options_only_test_overlap(bContext *C,
                                                             wmOperator *op,
                                                             Strip *strip)
{
  Scene *scene = CTX_data_sequencer_scene(C);
  Editing *ed = seq::editing_get(scene);

  if (strip == nullptr) {
    return false;
  }

  if (RNA_boolean_get(op->ptr, "replace_sel")) {
    strip->flag |= SELECT;
    seq::select_active_set(scene, strip);
  }

  return seq::transform_test_overlap(scene, ed->current_strips(), strip);
}

static void sequencer_disable_one_time_properties(bContext *C, wmOperator *op)
{
  Editing *ed = seq::editing_get(CTX_data_sequencer_scene(C));
  /* Disable following properties if there are any existing strips, unless overridden by user. */
  if (ed && ed->current_strips() && ed->current_strips()->first) {
    if (RNA_struct_find_property(op->ptr, "use_framerate")) {
      RNA_boolean_set(op->ptr, "use_framerate", false);
    }
    if (RNA_struct_find_property(op->ptr, "set_view_transform")) {
      RNA_boolean_set(op->ptr, "set_view_transform", false);
    }
  }
}

/** \} */

/* -------------------------------------------------------------------- */
/** \name Add Scene Strip
 * \{ */

static wmOperatorStatus sequencer_add_scene_strip_exec(bContext *C, wmOperator *op)
{
  Main *bmain = CTX_data_main(C);
  Scene *scene = CTX_data_sequencer_scene(C);
  const Editing *ed = seq::editing_ensure(scene);
  Scene *sce_seq = static_cast<Scene *>(
      BLI_findlink(&bmain->scenes, RNA_enum_get(op->ptr, "scene")));

  if (sce_seq == nullptr) {
    BKE_report(op->reports, RPT_ERROR, "Scene not found");
    return OPERATOR_CANCELLED;
  }

  const char *error_msg;
  if (!have_free_channels(C, op, 1, &error_msg)) {
    BKE_report(op->reports, RPT_ERROR, error_msg);
    return OPERATOR_CANCELLED;
  }

  if (RNA_boolean_get(op->ptr, "replace_sel")) {
    deselect_all_strips(scene);
  }

  seq::LoadData load_data;
  load_data_init_from_operator(&load_data, C, op);
  load_data.scene = sce_seq;

  Strip *strip = seq::add_scene_strip(scene, ed->current_strips(), &load_data);
  seq_load_apply_generic_options(C, op, strip);

  DEG_id_tag_update(&scene->id, ID_RECALC_SEQUENCER_STRIPS);
  DEG_relations_tag_update(bmain);
  sequencer_select_do_updates(C, scene);
  move_strips(C, op);

  return OPERATOR_FINISHED;
}

static wmOperatorStatus sequencer_add_scene_strip_invoke(bContext *C,
                                                         wmOperator *op,
                                                         const wmEvent *event)
{
  sequencer_disable_one_time_properties(C, op);
  if (!RNA_struct_property_is_set(op->ptr, "scene")) {
    return WM_enum_search_invoke(C, op, event);
  }

  sequencer_generic_invoke_xy__internal(C, op, 0, STRIP_TYPE_SCENE, event);
  return sequencer_add_scene_strip_exec(C, op);
}

void SEQUENCER_OT_scene_strip_add(wmOperatorType *ot)
{
  PropertyRNA *prop;

  /* Identifiers. */
  ot->name = "Add Scene Strip";
  ot->idname = "SEQUENCER_OT_scene_strip_add";
  ot->description = "Add a strip to the sequencer using a Blender scene as a source";

  /* API callbacks. */
  ot->invoke = sequencer_add_scene_strip_invoke;
  ot->exec = sequencer_add_scene_strip_exec;
  ot->poll = ED_operator_sequencer_active_editable;

  /* Flags. */
  ot->flag = OPTYPE_REGISTER | OPTYPE_UNDO;

  sequencer_generic_props__internal(ot, SEQPROP_STARTFRAME | SEQPROP_MOVE);
  prop = RNA_def_enum(ot->srna, "scene", rna_enum_dummy_NULL_items, 0, "Scene", "");
  RNA_def_enum_funcs(prop, RNA_scene_without_sequencer_scene_itemf);
  RNA_def_property_flag(prop, PROP_ENUM_NO_TRANSLATE);
  ot->prop = prop;
}

/** \} */

/* -------------------------------------------------------------------- */
/** \name Add Scene Strip With New Scene
 * \{ */

static EnumPropertyItem strip_new_scene_items[] = {
    {SCE_COPY_NEW, "NEW", 0, "New", "Add new Strip with a new empty Scene with default settings"},
    {SCE_COPY_EMPTY,
     "EMPTY",
     0,
     "Copy Settings",
     "Add a new Strip, with an empty scene, and copy settings from the current scene"},
    {SCE_COPY_LINK_COLLECTION,
     "LINK_COPY",
     0,
     "Linked Copy",
     "Add a Strip and link in the collections from the current scene (shallow copy)"},
    {SCE_COPY_FULL,
     "FULL_COPY",
     0,
     "Full Copy",
     "Add a Strip and make a full copy of the current scene"},
    {0, nullptr, 0, nullptr, nullptr},
};

static wmOperatorStatus sequencer_add_scene_strip_new_exec(bContext *C, wmOperator *op)
{
  Main *bmain = CTX_data_main(C);
  Scene *scene = CTX_data_sequencer_scene(C);
  const Editing *ed = seq::editing_ensure(scene);

  const char *error_msg;
  if (!have_free_channels(C, op, 1, &error_msg)) {
    BKE_report(op->reports, RPT_ERROR, error_msg);
    return OPERATOR_CANCELLED;
  }

  if (RNA_boolean_get(op->ptr, "replace_sel")) {
    deselect_all_strips(scene);
  }

  seq::LoadData load_data;
  load_data_init_from_operator(&load_data, C, op);

  int type = RNA_enum_get(op->ptr, "type");
  Scene *scene_new = ED_scene_sequencer_add(bmain, C, eSceneCopyMethod(type));
  if (scene_new == nullptr) {
    return OPERATOR_CANCELLED;
  }
  load_data.scene = scene_new;

  Strip *strip = seq::add_scene_strip(scene, ed->current_strips(), &load_data);
  seq_load_apply_generic_options(C, op, strip);

  DEG_id_tag_update(&scene->id, ID_RECALC_SEQUENCER_STRIPS);
  DEG_relations_tag_update(bmain);
  sequencer_select_do_updates(C, scene);
  move_strips(C, op);

  return OPERATOR_FINISHED;
}

static wmOperatorStatus sequencer_add_scene_strip_new_invoke(bContext *C,
                                                             wmOperator *op,
                                                             const wmEvent *event)
{
  sequencer_disable_one_time_properties(C, op);
  sequencer_generic_invoke_xy__internal(C, op, 0, STRIP_TYPE_SCENE, event);
  return sequencer_add_scene_strip_new_exec(C, op);
}

void SEQUENCER_OT_scene_strip_add_new(wmOperatorType *ot)
{
  /* Identifiers. */
  ot->name = "Add Strip with a new Scene";
  ot->idname = "SEQUENCER_OT_scene_strip_add_new";
  ot->description = "Create a new Strip and assign a new Scene as source";

  /* API callbacks. */
  ot->invoke = sequencer_add_scene_strip_new_invoke;
  ot->exec = sequencer_add_scene_strip_new_exec;
  ot->poll = ED_operator_sequencer_active_editable;

  /* Flags. */
  ot->flag = OPTYPE_REGISTER | OPTYPE_UNDO;

  sequencer_generic_props__internal(ot, SEQPROP_STARTFRAME | SEQPROP_MOVE);

  ot->prop = RNA_def_enum(ot->srna, "type", strip_new_scene_items, SCE_COPY_NEW, "Type", "");
}

/** \} */

/* -------------------------------------------------------------------- */
/** \name Add Scene Strip From Scene Asset
 * \{ */

/**
 * Make sure the scene is always unique and ready to edit.
 * If it was local it should be duplicated. If external it should be appended.
 */
static Scene *sequencer_add_scene_asset(const bContext &C,
                                        const asset_system::AssetRepresentation &asset,
                                        ReportList & /*reports*/)
{
  Main &bmain = *CTX_data_main(&C);
  Scene *scene_asset = reinterpret_cast<Scene *>(
      asset::asset_local_id_ensure_imported(bmain, asset, ASSET_IMPORT_APPEND));

  if (asset.is_local_id()) {
    /* Local scene that needs to be duplicated. */
    Scene *scene_copy = BKE_scene_duplicate(&bmain, scene_asset, SCE_COPY_FULL);
    return scene_copy;
  }
  return scene_asset;
}

static wmOperatorStatus sequencer_add_scene_asset_invoke(bContext *C,
                                                         wmOperator *op,
                                                         const wmEvent *event)
{
  Main *bmain = CTX_data_main(C);
  Scene *scene = CTX_data_sequencer_scene(C);
  if (!scene) {
    return OPERATOR_CANCELLED;
  }
  Editing *ed = seq::editing_ensure(scene);
  BLI_assert(ed != nullptr);

  sequencer_disable_one_time_properties(C, op);

  sequencer_generic_invoke_xy__internal(C, op, 0, STRIP_TYPE_SCENE, event);
  const asset_system::AssetRepresentation *asset =
      asset::operator_asset_reference_props_get_asset_from_all_library(*C, *op->ptr, op->reports);
  if (!asset) {
    return OPERATOR_CANCELLED;
  }

  Scene *scene_asset = sequencer_add_scene_asset(*C, *asset, *op->reports);
  if (!scene_asset) {
    return OPERATOR_CANCELLED;
  }

  const char *error_msg;
  if (!have_free_channels(C, op, 1, &error_msg)) {
    BKE_report(op->reports, RPT_ERROR, error_msg);
    return OPERATOR_CANCELLED;
  }

  if (RNA_boolean_get(op->ptr, "replace_sel")) {
    deselect_all_strips(scene);
  }

  seq::LoadData load_data;
  load_data_init_from_operator(&load_data, C, op);
  load_data.scene = scene_asset;

  Strip *strip = seq::add_scene_strip(scene, ed->current_strips(), &load_data);
  seq_load_apply_generic_options(C, op, strip);

  DEG_id_tag_update(&scene->id, ID_RECALC_SEQUENCER_STRIPS);
  DEG_relations_tag_update(bmain);
  sequencer_select_do_updates(C, scene);
  move_strips(C, op);

  return OPERATOR_FINISHED;
}

static std::string sequencer_add_scene_asset_get_description(bContext *C,
                                                             wmOperatorType * /*ot*/,
                                                             PointerRNA *ptr)
{
  const asset_system::AssetRepresentation *asset =
      asset::operator_asset_reference_props_get_asset_from_all_library(*C, *ptr, nullptr);
  if (!asset) {
    return "";
  }
  const AssetMetaData &asset_data = asset->get_metadata();
  if (!asset_data.description) {
    return "";
  }
  return TIP_(asset_data.description);
}

void SEQUENCER_OT_add_scene_strip_from_scene_asset(wmOperatorType *ot)
{
  ot->name = "Add Scene Asset";
  ot->description = "Add a scene strip from a scene asset";
  ot->idname = "SEQUENCER_OT_add_scene_strip_from_scene_asset";

  ot->invoke = sequencer_add_scene_asset_invoke;
  ot->poll = ED_operator_sequencer_active_editable;
  ot->get_description = sequencer_add_scene_asset_get_description;

  ot->flag = OPTYPE_REGISTER | OPTYPE_UNDO | OPTYPE_INTERNAL;

  sequencer_generic_props__internal(ot, SEQPROP_STARTFRAME | SEQPROP_MOVE);

  asset::operator_asset_reference_props_register(*ot->srna);
}

/** \} */

/* -------------------------------------------------------------------- */
/** \name Add Movieclip Strip
 * \{ */

static wmOperatorStatus sequencer_add_movieclip_strip_exec(bContext *C, wmOperator *op)
{
  Main *bmain = CTX_data_main(C);
  Scene *scene = CTX_data_sequencer_scene(C);
  const Editing *ed = seq::editing_ensure(scene);
  MovieClip *clip = static_cast<MovieClip *>(
      BLI_findlink(&bmain->movieclips, RNA_enum_get(op->ptr, "clip")));

  if (clip == nullptr) {
    BKE_report(op->reports, RPT_ERROR, "Movie clip not found");
    return OPERATOR_CANCELLED;
  }

  const char *error_msg;
  if (!have_free_channels(C, op, 1, &error_msg)) {
    BKE_report(op->reports, RPT_ERROR, error_msg);
    return OPERATOR_CANCELLED;
  }

  if (RNA_boolean_get(op->ptr, "replace_sel")) {
    deselect_all_strips(scene);
  }

  seq::LoadData load_data;
  if (!load_data_init_from_operator(&load_data, C, op)) {
    return OPERATOR_CANCELLED;
  }
  load_data.clip = clip;

  Strip *strip = seq::add_movieclip_strip(scene, ed->current_strips(), &load_data);
  seq_load_apply_generic_options(C, op, strip);

  DEG_id_tag_update(&scene->id, ID_RECALC_SEQUENCER_STRIPS);
  sequencer_select_do_updates(C, scene);
  move_strips(C, op);

  return OPERATOR_FINISHED;
}

static wmOperatorStatus sequencer_add_movieclip_strip_invoke(bContext *C,
                                                             wmOperator *op,
                                                             const wmEvent *event)
{
  if (!RNA_struct_property_is_set(op->ptr, "clip")) {
    return WM_enum_search_invoke(C, op, event);
  }

  sequencer_generic_invoke_xy__internal(C, op, 0, STRIP_TYPE_MOVIECLIP, event);
  return sequencer_add_movieclip_strip_exec(C, op);
}

void SEQUENCER_OT_movieclip_strip_add(wmOperatorType *ot)
{
  PropertyRNA *prop;

  /* Identifiers. */
  ot->name = "Add MovieClip Strip";
  ot->idname = "SEQUENCER_OT_movieclip_strip_add";
  ot->description = "Add a movieclip strip to the sequencer";

  /* API callbacks. */
  ot->invoke = sequencer_add_movieclip_strip_invoke;
  ot->exec = sequencer_add_movieclip_strip_exec;
  ot->poll = ED_operator_sequencer_active_editable;

  /* Flags. */
  ot->flag = OPTYPE_REGISTER | OPTYPE_UNDO;

  sequencer_generic_props__internal(ot, SEQPROP_STARTFRAME | SEQPROP_MOVE);
  prop = RNA_def_enum(ot->srna, "clip", rna_enum_dummy_NULL_items, 0, "Clip", "");
  RNA_def_enum_funcs(prop, RNA_movieclip_itemf);
  RNA_def_property_translation_context(prop, BLT_I18NCONTEXT_ID_MOVIECLIP);
  RNA_def_property_flag(prop, PROP_ENUM_NO_TRANSLATE);
  ot->prop = prop;
}

/** \} */

/* -------------------------------------------------------------------- */
/** \name Add Mask Strip
 * \{ */

static wmOperatorStatus sequencer_add_mask_strip_exec(bContext *C, wmOperator *op)
{
  Main *bmain = CTX_data_main(C);
  Scene *scene = CTX_data_sequencer_scene(C);
  const Editing *ed = seq::editing_ensure(scene);
  Mask *mask = static_cast<Mask *>(BLI_findlink(&bmain->masks, RNA_enum_get(op->ptr, "mask")));

  if (mask == nullptr) {
    BKE_report(op->reports, RPT_ERROR, "Mask not found");
    return OPERATOR_CANCELLED;
  }

  const char *error_msg;
  if (!have_free_channels(C, op, 1, &error_msg)) {
    BKE_report(op->reports, RPT_ERROR, error_msg);
    return OPERATOR_CANCELLED;
  }

  if (RNA_boolean_get(op->ptr, "replace_sel")) {
    deselect_all_strips(scene);
  }

  seq::LoadData load_data;
  load_data_init_from_operator(&load_data, C, op);
  load_data.mask = mask;

  Strip *strip = seq::add_mask_strip(scene, ed->current_strips(), &load_data);
  seq_load_apply_generic_options(C, op, strip);

  DEG_id_tag_update(&scene->id, ID_RECALC_SEQUENCER_STRIPS);
  sequencer_select_do_updates(C, scene);
  move_strips(C, op);

  return OPERATOR_FINISHED;
}

static wmOperatorStatus sequencer_add_mask_strip_invoke(bContext *C,
                                                        wmOperator *op,
                                                        const wmEvent *event)
{
  if (!RNA_struct_property_is_set(op->ptr, "mask")) {
    return WM_enum_search_invoke(C, op, event);
  }

  sequencer_generic_invoke_xy__internal(C, op, 0, STRIP_TYPE_MASK, event);
  return sequencer_add_mask_strip_exec(C, op);
}

void SEQUENCER_OT_mask_strip_add(wmOperatorType *ot)
{
  PropertyRNA *prop;

  /* Identifiers. */
  ot->name = "Add Mask Strip";
  ot->idname = "SEQUENCER_OT_mask_strip_add";
  ot->description = "Add a mask strip to the sequencer";

  /* API callbacks. */
  ot->invoke = sequencer_add_mask_strip_invoke;
  ot->exec = sequencer_add_mask_strip_exec;
  ot->poll = ED_operator_sequencer_active_editable;

  /* Flags. */
  ot->flag = OPTYPE_REGISTER | OPTYPE_UNDO;

  sequencer_generic_props__internal(ot, SEQPROP_STARTFRAME | SEQPROP_MOVE);
  prop = RNA_def_enum(ot->srna, "mask", rna_enum_dummy_NULL_items, 0, "Mask", "");
  RNA_def_enum_funcs(prop, RNA_mask_itemf);
  RNA_def_property_flag(prop, PROP_ENUM_NO_TRANSLATE);
  ot->prop = prop;
}

/** \} */

/* -------------------------------------------------------------------- */
/** \name Add Movie Strip
 * \{ */

/* Strips are added in context of timeline which has different preview size than actual preview. We
 * must search for preview area. In most cases there will be only one preview area, but there can
 * be more with different preview sizes. */
static IMB_Proxy_Size seq_get_proxy_size_flags(bContext *C)
{
  bScreen *screen = CTX_wm_screen(C);
  IMB_Proxy_Size proxy_sizes = IMB_PROXY_NONE;
  LISTBASE_FOREACH (ScrArea *, area, &screen->areabase) {
    LISTBASE_FOREACH (SpaceLink *, sl, &area->spacedata) {
      switch (sl->spacetype) {
        case SPACE_SEQ: {
          SpaceSeq *sseq = (SpaceSeq *)sl;
          if (!ELEM(sseq->view, SEQ_VIEW_PREVIEW, SEQ_VIEW_SEQUENCE_PREVIEW)) {
            continue;
          }
          proxy_sizes |= seq::rendersize_to_proxysize(
              eSpaceSeq_Proxy_RenderSize(sseq->render_size));
        }
      }
    }
  }
  return proxy_sizes;
}

static void seq_build_proxy(bContext *C, Span<Strip *> movie_strips)
{
  if (U.sequencer_proxy_setup != USER_SEQ_PROXY_SETUP_AUTOMATIC) {
    return;
  }

  wmJob *wm_job = seq::ED_seq_proxy_wm_job_get(C);
  seq::ProxyJob *pj = seq::ED_seq_proxy_job_get(C, wm_job);

  for (Strip *strip : movie_strips) {
    /* Enable and set proxy size. */
    seq::proxy_set(strip, true);
    strip->data->proxy->build_size_flags = seq_get_proxy_size_flags(C);
    strip->data->proxy->build_flags |= SEQ_PROXY_SKIP_EXISTING;
    seq::proxy_rebuild_context(
        pj->main, pj->depsgraph, pj->scene, strip, nullptr, &pj->queue, true);
  }

  if (!WM_jobs_is_running(wm_job)) {
    G.is_break = false;
    WM_jobs_start(CTX_wm_manager(C), wm_job);
  }
  ED_area_tag_redraw(CTX_wm_area(C));
}

static void sequencer_add_movie_sync_sound_strip(
    Main *bmain, Scene *scene, Strip *strip_movie, Strip *strip_sound, seq::LoadData *load_data)
{
  if (ELEM(nullptr, strip_movie, strip_sound)) {
    return;
  }

  /* Make sure that the sound strip start time relative to the movie is taken into account. */
  seq::add_sound_av_sync(bmain, scene, strip_sound, load_data);

  /* Expand missing sound data in the underlying container to fill the movie strip's length. To the
   * user, this missing data is the same as complete silence, so we pretend like it is. */
  strip_sound->len = std::max(strip_movie->len, strip_sound->len);

  /* Ensure that length matches the movie strip even if the underlying sound data
   * doesn't match up (e.g. it is longer). */
  seq::time_right_handle_frame_set(
      scene, strip_sound, seq::time_right_handle_frame_get(scene, strip_movie));
  seq::time_left_handle_frame_set(
      scene, strip_sound, seq::time_left_handle_frame_get(scene, strip_movie));
}

static void sequencer_add_movie_multiple_strips(bContext *C,
                                                wmOperator *op,
                                                seq::LoadData *load_data,
                                                VectorSet<Strip *> &r_movie_strips)
{
  Main *bmain = CTX_data_main(C);
  Scene *scene = CTX_data_sequencer_scene(C);
  const Editing *ed = seq::editing_ensure(scene);
  bool overlap_shuffle_override = RNA_boolean_get(op->ptr, "overlap") == false &&
                                  RNA_boolean_get(op->ptr, "overlap_shuffle_override");
  bool has_seq_overlap = false;
  Vector<Strip *> added_strips;

  RNA_BEGIN (op->ptr, itemptr, "files") {
    char dir_only[FILE_MAX];
    char file_only[FILE_MAX];
    RNA_string_get(op->ptr, "directory", dir_only);
    RNA_string_get(&itemptr, "name", file_only);
    BLI_path_join(load_data->path, sizeof(load_data->path), dir_only, file_only);
    STRNCPY(load_data->name, file_only);
    Strip *strip_movie = nullptr;
    Strip *strip_sound = nullptr;

    strip_movie = seq::add_movie_strip(bmain, scene, ed->current_strips(), load_data);

    if (strip_movie == nullptr) {
      BKE_reportf(op->reports, RPT_ERROR, "File '%s' could not be loaded", load_data->path);
    }
    else {
      if (RNA_boolean_get(op->ptr, "sound")) {
        strip_sound = seq::add_sound_strip(bmain, scene, ed->current_strips(), load_data);
        sequencer_add_movie_sync_sound_strip(bmain, scene, strip_movie, strip_sound, load_data);
        added_strips.append(strip_movie);

        if (strip_sound) {
          /* The video has sound, shift the video strip up a channel to make room for the sound
           * strip. */
          added_strips.append(strip_sound);
          seq::strip_channel_set(strip_movie,
                                 find_unlocked_unmuted_channel(ed, strip_movie->channel + 1));
        }
      }

      load_data->start_frame += seq::time_right_handle_frame_get(scene, strip_movie) -
                                seq::time_left_handle_frame_get(scene, strip_movie);
      if (overlap_shuffle_override) {
        has_seq_overlap |= seq_load_apply_generic_options_only_test_overlap(C, op, strip_sound);
        has_seq_overlap |= seq_load_apply_generic_options_only_test_overlap(C, op, strip_movie);
      }
      else {
        seq_load_apply_generic_options(C, op, strip_sound);
        seq_load_apply_generic_options(C, op, strip_movie);
      }

      if (U.sequencer_editor_flag & USER_SEQ_ED_CONNECT_STRIPS_BY_DEFAULT) {
        seq::connect(strip_movie, strip_sound);
      }

      r_movie_strips.add(strip_movie);
    }
  }
  RNA_END;

  if (overlap_shuffle_override) {
    if (has_seq_overlap) {
      ScrArea *area = CTX_wm_area(C);
      const bool use_sync_markers = (((SpaceSeq *)area->spacedata.first)->flag &
                                     SEQ_MARKER_TRANS) != 0;
      seq::transform_handle_overlap(scene, ed->current_strips(), added_strips, use_sync_markers);
    }
  }
}

static bool sequencer_add_movie_single_strip(bContext *C,
                                             wmOperator *op,
                                             seq::LoadData *load_data,
                                             VectorSet<Strip *> &r_movie_strips)
{
  Main *bmain = CTX_data_main(C);
  Scene *scene = CTX_data_sequencer_scene(C);
  const Editing *ed = seq::editing_ensure(scene);

  Strip *strip_movie = nullptr;
  Strip *strip_sound = nullptr;
  Vector<Strip *> added_strips;

  strip_movie = seq::add_movie_strip(bmain, scene, ed->current_strips(), load_data);

  if (strip_movie == nullptr) {
    BKE_reportf(op->reports, RPT_ERROR, "File '%s' could not be loaded", load_data->path);
    return false;
  }
  if (RNA_boolean_get(op->ptr, "sound")) {
    strip_sound = seq::add_sound_strip(bmain, scene, ed->current_strips(), load_data);
    sequencer_add_movie_sync_sound_strip(bmain, scene, strip_movie, strip_sound, load_data);
    added_strips.append(strip_movie);

    if (strip_sound) {
      added_strips.append(strip_sound);

      /* The video has sound, shift the video strip up a channel to make room for the sound
       * strip. */
      int movie_channel = strip_movie->channel + 1;

      if (RNA_boolean_get(op->ptr, "skip_locked_or_muted_channels")) {
        movie_channel = find_unlocked_unmuted_channel(ed, strip_movie->channel + 1);
      }

      seq::strip_channel_set(strip_movie, movie_channel);
    }
  }

  bool overlap_shuffle_override = RNA_boolean_get(op->ptr, "overlap") == false &&
                                  RNA_boolean_get(op->ptr, "overlap_shuffle_override");
  if (overlap_shuffle_override) {
    bool has_seq_overlap = false;

    has_seq_overlap |= seq_load_apply_generic_options_only_test_overlap(C, op, strip_sound);
    has_seq_overlap |= seq_load_apply_generic_options_only_test_overlap(C, op, strip_movie);

    if (has_seq_overlap) {
      ScrArea *area = CTX_wm_area(C);
      const bool use_sync_markers = (((SpaceSeq *)area->spacedata.first)->flag &
                                     SEQ_MARKER_TRANS) != 0;
      seq::transform_handle_overlap(scene, ed->current_strips(), added_strips, use_sync_markers);
    }
  }
  else {
    seq_load_apply_generic_options(C, op, strip_sound);
    seq_load_apply_generic_options(C, op, strip_movie);
  }

  if (U.sequencer_editor_flag & USER_SEQ_ED_CONNECT_STRIPS_BY_DEFAULT) {
    seq::connect(strip_movie, strip_sound);
  }

  r_movie_strips.add(strip_movie);

  return true;
}

static wmOperatorStatus sequencer_add_movie_strip_exec(bContext *C, wmOperator *op)
{
  Main *bmain = CTX_data_main(C);
  Scene *scene = CTX_data_sequencer_scene(C);
  seq::LoadData load_data;

  if (!load_data_init_from_operator(&load_data, C, op)) {
    return OPERATOR_CANCELLED;
  }

  sequencer_generic_invoke_xy__internal(C, op, SEQPROP_NOPATHS, STRIP_TYPE_MOVIE, nullptr);

  const char *error_msg;
  if (!have_free_channels(C, op, 2, &error_msg)) {
    BKE_report(op->reports, RPT_ERROR, error_msg);
    return OPERATOR_CANCELLED;
  }

  if (RNA_boolean_get(op->ptr, "replace_sel")) {
    deselect_all_strips(scene);
  }

  VectorSet<Strip *> movie_strips;
  const int tot_files = RNA_property_collection_length(op->ptr,
                                                       RNA_struct_find_property(op->ptr, "files"));

  char vt_old[64];
  STRNCPY_UTF8(vt_old, scene->view_settings.view_transform);
  float fps_old = scene->r.frs_sec / scene->r.frs_sec_base;

  if (tot_files > 1) {
    sequencer_add_movie_multiple_strips(C, op, &load_data, movie_strips);
  }
  else {
    sequencer_add_movie_single_strip(C, op, &load_data, movie_strips);
  }

  if (!STREQ(vt_old, scene->view_settings.view_transform)) {
    BKE_reportf(op->reports,
                RPT_WARNING,
                "View transform set to %s (converted from %s)",
                scene->view_settings.view_transform,
                vt_old);
  }

  if (fps_old != scene->r.frs_sec / scene->r.frs_sec_base) {
    BKE_reportf(op->reports,
                RPT_WARNING,
                "Scene frame rate set to %.4g (converted from %.4g)",
                scene->r.frs_sec / scene->r.frs_sec_base,
                fps_old);
  }

  if (movie_strips.is_empty()) {
    sequencer_add_free(C, op);
    return OPERATOR_CANCELLED;
  }

  seq_build_proxy(C, movie_strips);
  DEG_relations_tag_update(bmain);
  DEG_id_tag_update(&scene->id, ID_RECALC_SEQUENCER_STRIPS);
  sequencer_select_do_updates(C, scene);
  move_strips(C, op);

  sequencer_add_free(C, op);

  return OPERATOR_FINISHED;
}

static wmOperatorStatus sequencer_add_movie_strip_invoke(bContext *C,
                                                         wmOperator *op,
                                                         const wmEvent *event)
{
  PropertyRNA *prop;
  Scene *scene = CTX_data_sequencer_scene(C);

  sequencer_disable_one_time_properties(C, op);
  sequencer_add_init(C, op);

  RNA_enum_set(op->ptr, "fit_method", seq::tool_settings_fit_method_get(scene));
  RNA_boolean_set(op->ptr, "adjust_playback_rate", true);

  /* This is for drag and drop. */
  if ((RNA_struct_property_is_set(op->ptr, "files") &&
       !RNA_collection_is_empty(op->ptr, "files")) ||
      RNA_struct_property_is_set(op->ptr, "filepath"))
  {
    sequencer_generic_invoke_xy__internal(C, op, SEQPROP_NOPATHS, STRIP_TYPE_MOVIE, event);

    const char *error_msg;
    if (!have_free_channels(C, op, 2, &error_msg)) {
      BKE_report(op->reports, RPT_ERROR, error_msg);
      return OPERATOR_CANCELLED;
    }

    return sequencer_add_movie_strip_exec(C, op);
  }

  sequencer_generic_invoke_xy__internal(C, op, 0, STRIP_TYPE_MOVIE, event);

  /* Show multiview save options only if scene use multiview. */
  prop = RNA_struct_find_property(op->ptr, "show_multiview");
  RNA_property_boolean_set(op->ptr, prop, (scene->r.scemode & R_MULTIVIEW) != 0);

  WM_event_add_fileselect(C, op);
  return OPERATOR_RUNNING_MODAL;
}

static bool sequencer_add_draw_check_fn(PointerRNA *ptr, PropertyRNA *prop, void * /*user_data*/)
{
  const char *prop_id = RNA_property_identifier(prop);

  /* Only show placeholders option if sequence detection is enabled. */
  if (STREQ(prop_id, "use_placeholders")) {
    return RNA_boolean_get(ptr, "use_sequence_detection");
  }

  return !STR_ELEM(prop_id,
                   "filepath",
                   "directory",
                   "filename",
                   "frame_start",
                   "channel",
                   "length",
                   "move_strips",
                   "replace_sel");
}

static void sequencer_add_draw(bContext * /*C*/, wmOperator *op)
{
  uiLayout *layout = op->layout;
  SequencerAddData *sad = reinterpret_cast<SequencerAddData *>(op->customdata);
  ImageFormatData *imf = &sad->im_format;

  bool is_redo_panel = sad == nullptr;

  if (!is_redo_panel) {
    layout->prop(op->ptr, "move_strips", UI_ITEM_NONE, std::nullopt, ICON_NONE);
  }
  if (!RNA_boolean_get(op->ptr, "move_strips") || is_redo_panel) {
    uiLayout &col = layout->column(true);
    col.prop(op->ptr, "frame_start", UI_ITEM_NONE, std::nullopt, ICON_NONE);
    if (RNA_struct_find_property(op->ptr, "length")) {
      col.prop(op->ptr, "length", UI_ITEM_NONE, std::nullopt, ICON_NONE);
    }
    layout->prop(op->ptr, "channel", UI_ITEM_NONE, std::nullopt, ICON_NONE);
    layout->separator();
    layout->prop(op->ptr, "replace_sel", UI_ITEM_NONE, std::nullopt, ICON_NONE);
  }

  /* Main draw call. */
  uiDefAutoButsRNA(layout,
                   op->ptr,
                   sequencer_add_draw_check_fn,
                   nullptr,
                   nullptr,
                   UI_BUT_LABEL_ALIGN_NONE,
                   false);

  layout->separator();

  /* Image template. */
  PointerRNA imf_ptr = RNA_pointer_create_discrete(nullptr, &RNA_ImageFormatSettings, imf);

  /* Multiview template. */
  if (RNA_boolean_get(op->ptr, "show_multiview")) {
    uiTemplateImageFormatViews(layout, &imf_ptr, op->ptr);
  }
}

void SEQUENCER_OT_movie_strip_add(wmOperatorType *ot)
{

  /* Identifiers. */
  ot->name = "Add Movie Strip";
  ot->idname = "SEQUENCER_OT_movie_strip_add";
  ot->description = "Add a movie strip to the sequencer";

  /* API callbacks. */
  ot->invoke = sequencer_add_movie_strip_invoke;
  ot->exec = sequencer_add_movie_strip_exec;
  ot->cancel = sequencer_add_free;
  ot->ui = sequencer_add_draw;
  ot->poll = ED_operator_sequencer_active_editable;

  /* Flags. */
  ot->flag = OPTYPE_REGISTER | OPTYPE_UNDO;

  WM_operator_properties_filesel(ot,
                                 FILE_TYPE_FOLDER | FILE_TYPE_MOVIE,
                                 FILE_SPECIAL,
                                 FILE_OPENFILE,
                                 WM_FILESEL_FILEPATH | WM_FILESEL_RELPATH | WM_FILESEL_FILES |
                                     WM_FILESEL_SHOW_PROPS | WM_FILESEL_DIRECTORY,
                                 FILE_DEFAULTDISPLAY,
                                 FILE_SORT_DEFAULT);
  sequencer_generic_props__internal(ot,
                                    SEQPROP_STARTFRAME | SEQPROP_FIT_METHOD |
                                        SEQPROP_VIEW_TRANSFORM | SEQPROP_PLAYBACK_RATE |
                                        SEQPROP_MOVE);
  RNA_def_boolean(ot->srna, "sound", true, "Sound", "Load sound with the movie");
  RNA_def_boolean(ot->srna,
                  "use_framerate",
                  true,
                  "Set Scene Frame Rate",
                  "Set frame rate of the current scene to the frame rate of the movie");
}

/** \} */

/* -------------------------------------------------------------------- */
/** \name Add Sound Strip
 * \{ */

static void sequencer_add_sound_multiple_strips(bContext *C,
                                                wmOperator *op,
                                                seq::LoadData *load_data)
{
  Main *bmain = CTX_data_main(C);
  Scene *scene = CTX_data_sequencer_scene(C);
  Editing *ed = seq::editing_ensure(scene);

  RNA_BEGIN (op->ptr, itemptr, "files") {
    char dir_only[FILE_MAX];
    char file_only[FILE_MAX];
    RNA_string_get(op->ptr, "directory", dir_only);
    RNA_string_get(&itemptr, "name", file_only);
    BLI_path_join(load_data->path, sizeof(load_data->path), dir_only, file_only);
    STRNCPY(load_data->name, file_only);
    Strip *strip = seq::add_sound_strip(bmain, scene, ed->current_strips(), load_data);
    if (strip == nullptr) {
      BKE_reportf(op->reports, RPT_ERROR, "File '%s' could not be loaded", load_data->path);
    }
    else {
      seq_load_apply_generic_options(C, op, strip);
      load_data->start_frame += seq::time_right_handle_frame_get(scene, strip) -
                                seq::time_left_handle_frame_get(scene, strip);
    }
  }
  RNA_END;
}

static bool sequencer_add_sound_single_strip(bContext *C, wmOperator *op, seq::LoadData *load_data)
{
  Main *bmain = CTX_data_main(C);
  Scene *scene = CTX_data_sequencer_scene(C);
  Editing *ed = seq::editing_ensure(scene);

  Strip *strip = seq::add_sound_strip(bmain, scene, ed->current_strips(), load_data);
  if (strip == nullptr) {
    BKE_reportf(op->reports, RPT_ERROR, "File '%s' could not be loaded", load_data->path);
    return false;
  }
  seq_load_apply_generic_options(C, op, strip);

  return true;
}

static wmOperatorStatus sequencer_add_sound_strip_exec(bContext *C, wmOperator *op)
{
  Main *bmain = CTX_data_main(C);
  Scene *scene = CTX_data_sequencer_scene(C);
  seq::LoadData load_data;
  load_data_init_from_operator(&load_data, C, op);

  const char *error_msg;
  if (!have_free_channels(C, op, 1, &error_msg)) {
    BKE_report(op->reports, RPT_ERROR, error_msg);
    return OPERATOR_CANCELLED;
  }

  if (RNA_boolean_get(op->ptr, "replace_sel")) {
    deselect_all_strips(scene);
  }

  const int tot_files = RNA_property_collection_length(op->ptr,
                                                       RNA_struct_find_property(op->ptr, "files"));
  if (tot_files > 1) {
    sequencer_add_sound_multiple_strips(C, op, &load_data);
  }
  else {
    if (!sequencer_add_sound_single_strip(C, op, &load_data)) {
      sequencer_add_free(C, op);
      return OPERATOR_CANCELLED;
    }
  }

  DEG_relations_tag_update(bmain);
  DEG_id_tag_update(&scene->id, ID_RECALC_SEQUENCER_STRIPS);
  sequencer_select_do_updates(C, scene);
  move_strips(C, op);

  sequencer_add_free(C, op);

  return OPERATOR_FINISHED;
}

static wmOperatorStatus sequencer_add_sound_strip_invoke(bContext *C,
                                                         wmOperator *op,
                                                         const wmEvent *event)
{
  sequencer_add_init(C, op);

  /* This is for drag and drop. */
  if ((RNA_struct_property_is_set(op->ptr, "files") &&
       !RNA_collection_is_empty(op->ptr, "files")) ||
      RNA_struct_property_is_set(op->ptr, "filepath"))
  {
    sequencer_generic_invoke_xy__internal(C, op, SEQPROP_NOPATHS, STRIP_TYPE_SOUND_RAM, event);

    const char *error_msg;
    if (!have_free_channels(C, op, 1, &error_msg)) {
      BKE_report(op->reports, RPT_ERROR, error_msg);
      return OPERATOR_CANCELLED;
    }

    return sequencer_add_sound_strip_exec(C, op);
  }

  sequencer_generic_invoke_xy__internal(C, op, 0, STRIP_TYPE_SOUND_RAM, event);

  WM_event_add_fileselect(C, op);
  return OPERATOR_RUNNING_MODAL;
}

void SEQUENCER_OT_sound_strip_add(wmOperatorType *ot)
{

  /* Identifiers. */
  ot->name = "Add Sound Strip";
  ot->idname = "SEQUENCER_OT_sound_strip_add";
  ot->description = "Add a sound strip to the sequencer";

  /* API callbacks. */
  ot->invoke = sequencer_add_sound_strip_invoke;
  ot->exec = sequencer_add_sound_strip_exec;
  ot->poll = ED_operator_sequencer_active_editable;

  /* Flags. */
  ot->flag = OPTYPE_REGISTER | OPTYPE_UNDO;

  WM_operator_properties_filesel(ot,
                                 FILE_TYPE_FOLDER | FILE_TYPE_SOUND,
                                 FILE_SPECIAL,
                                 FILE_OPENFILE,
                                 WM_FILESEL_FILEPATH | WM_FILESEL_RELPATH | WM_FILESEL_FILES |
                                     WM_FILESEL_SHOW_PROPS | WM_FILESEL_DIRECTORY,
                                 FILE_DEFAULTDISPLAY,
                                 FILE_SORT_DEFAULT);
  sequencer_generic_props__internal(ot, SEQPROP_STARTFRAME | SEQPROP_MOVE);
  RNA_def_boolean(ot->srna, "cache", false, "Cache", "Cache the sound in memory");
  RNA_def_boolean(ot->srna, "mono", false, "Mono", "Merge all the sound's channels into one");
}

/** \} */

/* -------------------------------------------------------------------- */
/** \name Add Image Strip
 * \{ */

int sequencer_image_strip_get_minmax_frame(wmOperator *op,
                                           int sfra,
                                           int *r_minframe,
                                           int *r_numdigits)
{
  int minframe = INT32_MAX, maxframe = INT32_MIN;
  int numdigits = 0;

  RNA_BEGIN (op->ptr, itemptr, "files") {
    int frame;
    std::string filename = RNA_string_get(&itemptr, "name");

    if (!filename.empty()) {
      if (BLI_path_frame_get(filename.c_str(), &frame, &numdigits)) {
        minframe = min_ii(minframe, frame);
        maxframe = max_ii(maxframe, frame);
      }
    }
  }
  RNA_END;

  if (minframe == INT32_MAX) {
    minframe = sfra;
    maxframe = minframe + 1;
  }

  *r_minframe = minframe;
  *r_numdigits = numdigits;

  return maxframe - minframe + 1;
}

void sequencer_image_strip_reserve_frames(
    wmOperator *op, StripElem *se, int len, int minframe, int numdigits)
{
  char *filename = nullptr;
  RNA_BEGIN (op->ptr, itemptr, "files") {
    filename = RNA_string_get_alloc(&itemptr, "name", nullptr, 0, nullptr);
    break;
  }
  RNA_END;

  if (filename) {
    char ext[FILE_MAX];
    char filename_stripped[FILE_MAX];
    /* Strip the frame from filename and substitute with `#`. */
    BLI_path_frame_strip(filename, ext, sizeof(ext));

    for (int i = 0; i < len; i++, se++) {
      STRNCPY(filename_stripped, filename);
      BLI_path_frame(filename_stripped, sizeof(filename_stripped), minframe + i, numdigits);
      SNPRINTF(se->filename, "%s%s", filename_stripped, ext);
    }

    MEM_freeN(filename);
  }
}

static void frame_filename_set(char *dst,
                               size_t dst_len,
                               const char *filename_stripped,
                               const int frame,
                               const int numdigits,
                               const char *ext)
{
  BLI_strncpy(dst, filename_stripped, dst_len);
  BLI_path_frame(dst, dst_len, frame, numdigits);
  BLI_path_extension_ensure(dst, dst_len, ext);
}

static void sequencer_add_image_strip_load_files(wmOperator *op,
                                                 Scene *scene,
                                                 Strip *strip,
                                                 seq::LoadData *load_data,
                                                 const ImageFrameRange *range)
{
  int framenr, numdigits;
  BLI_path_frame_get(load_data->path, &framenr, &numdigits);
  char ext[FILE_MAX];
  char filename_stripped[FILE_MAX];
  BLI_path_split_file_part(load_data->path, filename_stripped, sizeof(filename_stripped));
  BLI_path_frame_strip(filename_stripped, ext, sizeof(ext));

  StripElem *se = strip->data->stripdata;
  const bool use_placeholders = RNA_boolean_get(op->ptr, "use_placeholders");
  if (use_placeholders) {
    for (int i = range->offset; i < range->max_framenr + 1; i++, se++) {
      frame_filename_set(se->filename, sizeof(se->filename), filename_stripped, i, numdigits, ext);
    }
  }
  else {
    size_t strip_frame = 0;
    LISTBASE_FOREACH (ImageFrame *, frame, &range->frames) {
      char filename[FILE_MAX];
      frame_filename_set(
          filename, sizeof(filename), filename_stripped, frame->framenr, numdigits, ext);
      seq::add_image_load_file(scene, strip, strip_frame, filename);
      strip_frame++;
    }
  }
}

static wmOperatorStatus sequencer_add_image_strip_exec(bContext *C, wmOperator *op)
{
  Main *bmain = CTX_data_main(C);
  Scene *scene = CTX_data_sequencer_scene(C);
  Editing *ed = seq::editing_ensure(scene);

  seq::LoadData load_data;
  if (!load_data_init_from_operator(&load_data, C, op)) {
    return OPERATOR_CANCELLED;
  }

  const char *error_msg;
  if (!have_free_channels(C, op, 1, &error_msg)) {
    BKE_report(op->reports, RPT_ERROR, error_msg);
    return OPERATOR_CANCELLED;
  }

  ListBase ranges = ED_image_filesel_detect_sequences(BKE_main_blendfile_path(bmain), op, false);
  if (BLI_listbase_is_empty(&ranges)) {
    sequencer_add_free(C, op);
    return OPERATOR_CANCELLED;
  }

  if (RNA_boolean_get(op->ptr, "replace_sel")) {
    deselect_all_strips(scene);
  }

  char vt_old[64];
  STRNCPY_UTF8(vt_old, scene->view_settings.view_transform);

  const bool use_placeholders = RNA_boolean_get(op->ptr, "use_placeholders");
  LISTBASE_FOREACH (ImageFrameRange *, range, &ranges) {
    /* Populate `load_data` with data from `range`. */
    load_data.image.count = use_placeholders ? range->max_framenr - range->offset + 1 :
                                               BLI_listbase_count(&range->frames);
    STRNCPY(load_data.path, range->filepath);
    BLI_path_split_file_part(load_data.path, load_data.name, sizeof(load_data.name));

    Strip *strip = seq::add_image_strip(bmain, scene, ed->current_strips(), &load_data);
    const bool is_sequence = !seq::transform_single_image_check(strip);

    char dirpath[sizeof(strip->data->dirpath)];
    BLI_path_split_dir_part(load_data.path, dirpath, sizeof(dirpath));
    seq::add_image_set_directory(strip, dirpath);

    /* Set `StripElem` filenames, one for each `ImageFrame` in this range, or if `use_placeholders`
     * is set, every frame between `offset` and `max_framenr` . */
    sequencer_add_image_strip_load_files(op, scene, strip, &load_data, range);

    seq::add_image_init_alpha_mode(bmain, scene, strip);

    /* Adjust starting length of strip.
     * Note that this length differs from `strip->len`, which is always 1 for single images. */
    if (!is_sequence) {
      seq::time_right_handle_frame_set(
          scene, strip, load_data.start_frame + load_data.image.length);
    }

    seq_load_apply_generic_options(C, op, strip);
    load_data.start_frame += is_sequence ? load_data.image.count : load_data.image.length;
    BLI_freelistN(&range->frames);
  }
  BLI_freelistN(&ranges);

  if (!STREQ(vt_old, scene->view_settings.view_transform)) {
    BKE_reportf(op->reports,
                RPT_WARNING,
                "View transform set to %s (converted from %s)",
                scene->view_settings.view_transform,
                vt_old);
  }

  DEG_id_tag_update(&scene->id, ID_RECALC_SEQUENCER_STRIPS);
  sequencer_select_do_updates(C, scene);
  move_strips(C, op);

  sequencer_add_free(C, op);

  return OPERATOR_FINISHED;
}

static wmOperatorStatus sequencer_add_image_strip_invoke(bContext *C,
                                                         wmOperator *op,
                                                         const wmEvent *event)
{
  PropertyRNA *prop;
  Scene *scene = CTX_data_sequencer_scene(C);

  sequencer_disable_one_time_properties(C, op);
  sequencer_add_init(C, op);

  RNA_enum_set(op->ptr, "fit_method", seq::tool_settings_fit_method_get(scene));

  /* Name set already by drag and drop. */
  if (RNA_struct_property_is_set(op->ptr, "files") && !RNA_collection_is_empty(op->ptr, "files")) {
    sequencer_generic_invoke_xy__internal(
        C, op, SEQPROP_LENGTH | SEQPROP_NOPATHS, STRIP_TYPE_IMAGE, event);

    const char *error_msg;
    if (!have_free_channels(C, op, 1, &error_msg)) {
      BKE_report(op->reports, RPT_ERROR, error_msg);
      return OPERATOR_CANCELLED;
    }

    return sequencer_add_image_strip_exec(C, op);
  }

  sequencer_generic_invoke_xy__internal(C, op, SEQPROP_LENGTH, STRIP_TYPE_IMAGE, event);

  /* Show multiview save options only if the scene uses multiview. */
  prop = RNA_struct_find_property(op->ptr, "show_multiview");
  RNA_property_boolean_set(op->ptr, prop, (scene->r.scemode & R_MULTIVIEW) != 0);

  WM_event_add_fileselect(C, op);
  return OPERATOR_RUNNING_MODAL;
}

void SEQUENCER_OT_image_strip_add(wmOperatorType *ot)
{

  /* Identifiers. */
  ot->name = "Add Image Strip";
  ot->idname = "SEQUENCER_OT_image_strip_add";
  ot->description = "Add an image or image sequence to the sequencer";

  /* API callbacks. */
  ot->invoke = sequencer_add_image_strip_invoke;
  ot->exec = sequencer_add_image_strip_exec;
  ot->cancel = sequencer_add_free;
  ot->ui = sequencer_add_draw;
  ot->poll = ED_operator_sequencer_active_editable;

  /* Flags. */
  ot->flag = OPTYPE_REGISTER | OPTYPE_UNDO;

  WM_operator_properties_filesel(
      ot,
      FILE_TYPE_FOLDER | FILE_TYPE_IMAGE,
      FILE_SPECIAL,
      FILE_OPENFILE,
      (WM_FILESEL_DIRECTORY | WM_FILESEL_RELPATH | WM_FILESEL_FILES | WM_FILESEL_SHOW_PROPS),
      FILE_DEFAULTDISPLAY,
      FILE_SORT_DEFAULT);
  sequencer_generic_props__internal(ot,
                                    SEQPROP_STARTFRAME | SEQPROP_LENGTH | SEQPROP_FIT_METHOD |
                                        SEQPROP_VIEW_TRANSFORM | SEQPROP_MOVE);

  RNA_def_boolean(
      ot->srna,
      "use_sequence_detection",
      true,
      "Detect Sequences",
      "Automatically detect animated sequences in selected images (based on file names)");

  RNA_def_boolean(ot->srna,
                  "use_placeholders",
                  false,
                  "Use Placeholders",
                  "Reserve placeholder frames for missing frames of the image sequence");
}

/** \} */

/* -------------------------------------------------------------------- */
/** \name Add Effect Strip
 * \{ */

static wmOperatorStatus sequencer_add_effect_strip_exec(bContext *C, wmOperator *op)
{
  Scene *scene = CTX_data_sequencer_scene(C);
  Editing *ed = seq::editing_ensure(scene);

  const char *error;
  if (!have_free_channels(C, op, 1, &error)) {
    BKE_report(op->reports, RPT_ERROR, error);
    return OPERATOR_CANCELLED;
  }

  seq::LoadData load_data;
  load_data_init_from_operator(&load_data, C, op);
  load_data.effect.type = RNA_enum_get(op->ptr, "type");
  const int num_inputs = seq::effect_get_num_inputs(load_data.effect.type);

  VectorSet<Strip *> inputs = strip_effect_get_new_inputs(scene, num_inputs);
  StringRef error_msg = effect_inputs_validate(inputs, num_inputs);

  if (!error_msg.is_empty()) {
    BKE_report(op->reports, RPT_ERROR, error_msg.data());
    return OPERATOR_CANCELLED;
  }

  if (RNA_boolean_get(op->ptr, "replace_sel")) {
    deselect_all_strips(scene);
  }

  Strip *input1 = inputs.size() > 0 ? inputs[0] : nullptr;
  Strip *input2 = inputs.size() == 2 ? inputs[1] : nullptr;

  load_data.effect.input1 = input1;
  load_data.effect.input2 = input2;

  /* Set channel. If unset, use lowest free one above strips. */
  if (!RNA_struct_property_is_set(op->ptr, "channel")) {
    if (input1 != nullptr) {
      int chan = max_ii(input1 ? input1->channel : 0, input2 ? input2->channel : 0);
      if (chan < seq::MAX_CHANNELS) {
        load_data.channel = chan;
      }
    }
  }

  Strip *strip = seq::add_effect_strip(scene, ed->current_strips(), &load_data);
  seq_load_apply_generic_options(C, op, strip);

  if (strip->type == STRIP_TYPE_COLOR) {
    SolidColorVars *colvars = (SolidColorVars *)strip->effectdata;
    RNA_float_get_array(op->ptr, "color", colvars->col);
  }

  DEG_id_tag_update(&scene->id, ID_RECALC_SEQUENCER_STRIPS);
  sequencer_select_do_updates(C, scene);

  /* It's reasonable to add effects with inputs directly above the input. */
  if (ELEM(load_data.effect.type,
           STRIP_TYPE_COLOR,
           STRIP_TYPE_TEXT,
           STRIP_TYPE_ADJUSTMENT,
           STRIP_TYPE_MULTICAM))
  {
    move_strips(C, op);
  }

  return OPERATOR_FINISHED;
}

static wmOperatorStatus sequencer_add_effect_strip_invoke(bContext *C,
                                                          wmOperator *op,
                                                          const wmEvent *event)
{
  bool is_type_set = RNA_struct_property_is_set(op->ptr, "type");
  int type = -1;
  int prop_flag = SEQPROP_LENGTH;

  if (!is_type_set) {
    BKE_report(op->reports, RPT_ERROR_INVALID_INPUT, "Strip type is not set.");
    return OPERATOR_CANCELLED;
  }

  type = RNA_enum_get(op->ptr, "type");

  /* When invoking an effect strip which uses inputs, skip guessing of the channel. */
  if (seq::effect_get_num_inputs(type) != 0) {
    prop_flag |= SEQPROP_NOCHAN;
  }

  sequencer_generic_invoke_xy__internal(C, op, prop_flag, type, event);

  return sequencer_add_effect_strip_exec(C, op);
}

static bool sequencer_add_effect_strip_poll_property(const bContext * /*C*/,
                                                     wmOperator *op,
                                                     const PropertyRNA *prop)
{
  const char *prop_id = RNA_property_identifier(prop);
  int type = RNA_enum_get(op->ptr, "type");

  /* Hide start frame and length for effect strips that are locked to their parents' location. */
  if (seq::effect_get_num_inputs(type) != 0) {
    if (STR_ELEM(prop_id, "frame_start", "length")) {
      return false;
    }
  }
  if ((type != STRIP_TYPE_COLOR) && STREQ(prop_id, "color")) {
    return false;
  }

  return true;
}

static std::string sequencer_add_effect_strip_get_description(bContext * /*C*/,
                                                              wmOperatorType * /*ot*/,
                                                              PointerRNA *ptr)
{
  const int type = RNA_enum_get(ptr, "type");

  switch (type) {
    case STRIP_TYPE_CROSS:
      return TIP_("Add a crossfade transition strip for two selected strips with video content");
    case STRIP_TYPE_ADD:
      return TIP_("Add an add blend mode effect strip for two selected strips with video content");
    case STRIP_TYPE_SUB:
      return TIP_(
          "Add a subtract blend mode effect strip for two selected strips with video content");
    case STRIP_TYPE_ALPHAOVER:
      return TIP_(
          "Add an alpha over blend mode effect strip for two selected strips with video content");
    case STRIP_TYPE_ALPHAUNDER:
      return TIP_(
          "Add an alpha under blend mode effect strip for two selected strips with video content");
    case STRIP_TYPE_GAMCROSS:
      return TIP_(
          "Add a gamma crossfade transition strip for two selected strips with video content");
    case STRIP_TYPE_MUL:
      return TIP_(
          "Add a multiply blend mode effect strip for two selected strips with video content");
    case STRIP_TYPE_WIPE:
      return TIP_("Add a wipe transition strip for two selected strips with video content");
    case STRIP_TYPE_GLOW:
      return TIP_("Add a glow effect strip for a single selected strip with video content");
    case STRIP_TYPE_COLOR:
      return TIP_("Add a color strip to the sequencer");
    case STRIP_TYPE_SPEED:
      return TIP_("Add a video speed effect strip for a single selected strip with video content");
    case STRIP_TYPE_MULTICAM:
      return TIP_("Add a multicam selector effect strip to the sequencer");
    case STRIP_TYPE_ADJUSTMENT:
      return TIP_("Add an adjustment layer effect strip to the sequencer");
    case STRIP_TYPE_GAUSSIAN_BLUR:
      return TIP_(
          "Add a gaussian blur effect strip for a single selected strip with video content");
    case STRIP_TYPE_TEXT:
      return TIP_("Add a text strip to the sequencer");
    case STRIP_TYPE_COLORMIX:
      return TIP_("Add a color mix effect strip to the sequencer");
    default:
      break;
  }

  /* Use default description. */
  return "";
}

void SEQUENCER_OT_effect_strip_add(wmOperatorType *ot)
{
  PropertyRNA *prop;

  /* Identifiers. */
  ot->name = "Add Effect Strip";
  ot->idname = "SEQUENCER_OT_effect_strip_add";
  ot->description = "Add an effect to the sequencer, most are applied on top of existing strips";

  /* API callbacks. */
  ot->invoke = sequencer_add_effect_strip_invoke;
  ot->exec = sequencer_add_effect_strip_exec;
  ot->poll = ED_operator_sequencer_active_editable;
  ot->poll_property = sequencer_add_effect_strip_poll_property;
  ot->get_description = sequencer_add_effect_strip_get_description;

  /* Flags. */
  ot->flag = OPTYPE_REGISTER | OPTYPE_UNDO;

  prop = RNA_def_enum(ot->srna,
                      "type",
                      sequencer_prop_effect_types,
                      STRIP_TYPE_CROSS,
                      "Type",
                      "Sequencer effect type");
  RNA_def_property_translation_context(prop, BLT_I18NCONTEXT_ID_SEQUENCE);
  sequencer_generic_props__internal(ot, SEQPROP_STARTFRAME | SEQPROP_LENGTH | SEQPROP_MOVE);
  /* Only used when strip is of the Color type. */
  prop = RNA_def_float_color(ot->srna,
                             "color",
                             3,
                             nullptr,
                             0.0f,
                             1.0f,
                             "Color",
                             "Initialize the strip with this color",
                             0.0f,
                             1.0f);
  RNA_def_property_subtype(prop, PROP_COLOR_GAMMA);
}

/** \} */

}  // namespace blender::ed::vse
