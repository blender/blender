/*
 * This program is free software; you can redistribute it and/or
 * modify it under the terms of the GNU General Public License
 * as published by the Free Software Foundation; either version 2
 * of the License, or (at your option) any later version.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with this program; if not, write to the Free Software Foundation,
 * Inc., 51 Franklin Street, Fifth Floor, Boston, MA 02110-1301, USA.
 *
 * The Original Code is Copyright (C) 2001-2002 by NaN Holding BV.
 * All rights reserved.
 */

/** \file
 * \ingroup spseq
 */

#include <ctype.h>
#include <math.h>
#include <stdlib.h>
#include <string.h>

#include "MEM_guardedalloc.h"

#include "BLI_blenlib.h"
#include "BLI_ghash.h"
#include "BLI_math.h"
#include "BLI_utildefines.h"

#include "BLT_translation.h"

#include "DNA_mask_types.h"
#include "DNA_scene_types.h"
#include "DNA_sound_types.h"
#include "DNA_space_types.h"

#include "BKE_context.h"
#include "BKE_global.h"
#include "BKE_lib_id.h"
#include "BKE_main.h"
#include "BKE_mask.h"
#include "BKE_movieclip.h"
#include "BKE_report.h"
#include "BKE_sound.h"

#include "IMB_imbuf.h"

#include "WM_api.h"
#include "WM_types.h"

#include "RNA_define.h"
#include "RNA_enum_types.h"

#include "SEQ_add.h"
#include "SEQ_effects.h"
#include "SEQ_proxy.h"
#include "SEQ_relations.h"
#include "SEQ_render.h"
#include "SEQ_select.h"
#include "SEQ_sequencer.h"
#include "SEQ_time.h"
#include "SEQ_transform.h"
#include "SEQ_utils.h"

/* For menu, popup, icons, etc. */
#include "ED_screen.h"
#include "ED_sequencer.h"

#include "UI_interface.h"

#ifdef WITH_AUDASPACE
#  include <AUD_Sequence.h>
#endif

#include "DEG_depsgraph.h"
#include "DEG_depsgraph_build.h"

/* Own include. */
#include "sequencer_intern.h"

typedef struct SequencerAddData {
  ImageFormatData im_format;
} SequencerAddData;

/* Generic functions, reused by add strip operators. */

/* Avoid passing multiple args and be more verbose. */
#define SEQPROP_STARTFRAME (1 << 0)
#define SEQPROP_ENDFRAME (1 << 1)
#define SEQPROP_NOPATHS (1 << 2)
#define SEQPROP_NOCHAN (1 << 3)
#define SEQPROP_FIT_METHOD (1 << 4)
#define SEQPROP_VIEW_TRANSFORM (1 << 5)

static const EnumPropertyItem scale_fit_methods[] = {
    {SEQ_SCALE_TO_FIT, "FIT", 0, "Scale to Fit", "Scale image to fit within the canvas"},
    {SEQ_SCALE_TO_FILL, "FILL", 0, "Scale to Fill", "Scale image to completely fill the canvas"},
    {SEQ_STRETCH_TO_FILL, "STRETCH", 0, "Stretch to Fill", "Stretch image to fill the canvas"},
    {SEQ_USE_ORIGINAL_SIZE, "ORIGINAL", 0, "Use Original Size", "Keep image at its original size"},
    {0, NULL, 0, NULL, NULL},
};

static void sequencer_generic_props__internal(wmOperatorType *ot, int flag)
{
  PropertyRNA *prop;

  if (flag & SEQPROP_STARTFRAME) {
    RNA_def_int(ot->srna,
                "frame_start",
                0,
                INT_MIN,
                INT_MAX,
                "Start Frame",
                "Start frame of the sequence strip",
                -MAXFRAME,
                MAXFRAME);
  }

  if (flag & SEQPROP_ENDFRAME) {
    /* Not usual since most strips have a fixed length. */
    RNA_def_int(ot->srna,
                "frame_end",
                0,
                INT_MIN,
                INT_MAX,
                "End Frame",
                "End frame for the color strip",
                -MAXFRAME,
                MAXFRAME);
  }

  RNA_def_int(
      ot->srna, "channel", 1, 1, MAXSEQ, "Channel", "Channel to place this strip into", 1, MAXSEQ);

  RNA_def_boolean(
      ot->srna, "replace_sel", 1, "Replace Selection", "Replace the current selection");

  /* Only for python scripts which import strips and place them after. */
  prop = RNA_def_boolean(
      ot->srna, "overlap", 0, "Allow Overlap", "Don't correct overlap on new sequence strips");
  RNA_def_property_flag(prop, PROP_HIDDEN);

  if (flag & SEQPROP_FIT_METHOD) {
    ot->prop = RNA_def_enum(ot->srna,
                            "fit_method",
                            scale_fit_methods,
                            SEQ_SCALE_TO_FIT,
                            "Fit Method",
                            "Scale fit method");
  }

  if (flag & SEQPROP_VIEW_TRANSFORM) {
    ot->prop = RNA_def_boolean(ot->srna,
                               "set_view_transform",
                               true,
                               "Set View Transform",
                               "Set appropriate view transform based on media color space");
  }
}

static void sequencer_generic_invoke_path__internal(bContext *C,
                                                    wmOperator *op,
                                                    const char *identifier)
{
  if (RNA_struct_find_property(op->ptr, identifier)) {
    Scene *scene = CTX_data_scene(C);
    Sequence *last_seq = SEQ_select_active_get(scene);
    if (last_seq && last_seq->strip && SEQ_HAS_PATH(last_seq)) {
      Main *bmain = CTX_data_main(C);
      char path[FILE_MAX];
      BLI_strncpy(path, last_seq->strip->dir, sizeof(path));
      BLI_path_abs(path, BKE_main_blendfile_path(bmain));
      RNA_string_set(op->ptr, identifier, path);
    }
  }
}

static int sequencer_generic_invoke_xy_guess_channel(bContext *C, int type)
{
  Sequence *tgt = NULL;
  Sequence *seq;
  Scene *scene = CTX_data_scene(C);
  Editing *ed = SEQ_editing_ensure(scene);
  int timeline_frame = (int)CFRA;
  int proximity = INT_MAX;

  if (!ed || !ed->seqbasep) {
    return 1;
  }

  for (seq = ed->seqbasep->first; seq; seq = seq->next) {
    if ((type == -1 || seq->type == type) && (seq->enddisp < timeline_frame) &&
        (timeline_frame - seq->enddisp < proximity)) {
      tgt = seq;
      proximity = timeline_frame - seq->enddisp;
    }
  }

  if (tgt) {
    return tgt->machine + 1;
  }
  return 1;
}

static void sequencer_generic_invoke_xy__internal(bContext *C, wmOperator *op, int flag, int type)
{
  Scene *scene = CTX_data_scene(C);

  int timeline_frame = (int)CFRA;

  /* Effect strips don't need a channel initialized from the mouse. */
  if (!(flag & SEQPROP_NOCHAN) && RNA_struct_property_is_set(op->ptr, "channel") == 0) {
    RNA_int_set(op->ptr, "channel", sequencer_generic_invoke_xy_guess_channel(C, type));
  }

  RNA_int_set(op->ptr, "frame_start", timeline_frame);

  if ((flag & SEQPROP_ENDFRAME) && RNA_struct_property_is_set(op->ptr, "frame_end") == 0) {
    RNA_int_set(op->ptr, "frame_end", timeline_frame + 25); /* XXX arbitrary but ok for now. */
  }

  if (!(flag & SEQPROP_NOPATHS)) {
    sequencer_generic_invoke_path__internal(C, op, "filepath");
    sequencer_generic_invoke_path__internal(C, op, "directory");
  }
}

static void load_data_init_from_operator(SeqLoadData *load_data, bContext *C, wmOperator *op)
{
  Main *bmain = CTX_data_main(C);

  PropertyRNA *prop;
  const bool relative = (prop = RNA_struct_find_property(op->ptr, "relative_path")) &&
                        RNA_property_boolean_get(op->ptr, prop);
  memset(load_data, 0, sizeof(SeqLoadData));

  load_data->start_frame = RNA_int_get(op->ptr, "frame_start");
  load_data->channel = RNA_int_get(op->ptr, "channel");
  load_data->image.end_frame = load_data->start_frame;
  load_data->image.len = 1;

  if ((prop = RNA_struct_find_property(op->ptr, "fit_method"))) {
    load_data->fit_method = RNA_enum_get(op->ptr, "fit_method");
    SEQ_tool_settings_fit_method_set(CTX_data_scene(C), load_data->fit_method);
  }

  if ((prop = RNA_struct_find_property(op->ptr, "filepath"))) {
    RNA_property_string_get(op->ptr, prop, load_data->path);
    BLI_strncpy(load_data->name, BLI_path_basename(load_data->path), sizeof(load_data->name));
  }
  else if ((prop = RNA_struct_find_property(op->ptr, "directory"))) {
    char *directory = RNA_string_get_alloc(op->ptr, "directory", NULL, 0, NULL);

    if ((prop = RNA_struct_find_property(op->ptr, "files"))) {
      RNA_PROP_BEGIN (op->ptr, itemptr, prop) {
        char *filename = RNA_string_get_alloc(&itemptr, "name", NULL, 0, NULL);
        BLI_strncpy(load_data->name, filename, sizeof(load_data->name));
        BLI_join_dirfile(load_data->path, sizeof(load_data->path), directory, filename);
        MEM_freeN(filename);
        break;
      }
      RNA_PROP_END;
    }
    MEM_freeN(directory);
  }

  if (relative) {
    BLI_path_rel(load_data->path, BKE_main_blendfile_path(bmain));
  }

  if ((prop = RNA_struct_find_property(op->ptr, "frame_end"))) {
    load_data->image.end_frame = RNA_property_int_get(op->ptr, prop);
    load_data->effect.end_frame = load_data->image.end_frame;
  }

  if ((prop = RNA_struct_find_property(op->ptr, "cache")) &&
      RNA_property_boolean_get(op->ptr, prop)) {
    load_data->flags |= SEQ_LOAD_SOUND_CACHE;
  }

  if ((prop = RNA_struct_find_property(op->ptr, "mono")) &&
      RNA_property_boolean_get(op->ptr, prop)) {
    load_data->flags |= SEQ_LOAD_SOUND_MONO;
  }

  if ((prop = RNA_struct_find_property(op->ptr, "use_framerate")) &&
      RNA_property_boolean_get(op->ptr, prop)) {
    load_data->flags |= SEQ_LOAD_MOVIE_SYNC_FPS;
  }

  if ((prop = RNA_struct_find_property(op->ptr, "set_view_transform")) &&
      RNA_property_boolean_get(op->ptr, prop)) {
    load_data->flags |= SEQ_LOAD_SET_VIEW_TRANSFORM;
  }

  if ((prop = RNA_struct_find_property(op->ptr, "use_multiview")) &&
      RNA_property_boolean_get(op->ptr, prop)) {
    if (op->customdata) {
      SequencerAddData *sad = op->customdata;
      ImageFormatData *imf = &sad->im_format;

      load_data->use_multiview = true;
      load_data->views_format = imf->views_format;
      load_data->stereo3d_format = &imf->stereo3d_format;
    }
  }
}

static void seq_load_apply_generic_options(bContext *C, wmOperator *op, Sequence *seq)
{
  Scene *scene = CTX_data_scene(C);
  Editing *ed = SEQ_editing_get(scene);

  if (seq == NULL) {
    return;
  }

  if (RNA_boolean_get(op->ptr, "replace_sel")) {
    seq->flag |= SELECT;
    SEQ_select_active_set(scene, seq);
  }

  if (RNA_boolean_get(op->ptr, "overlap") == false) {
    if (SEQ_transform_test_overlap(ed->seqbasep, seq)) {
      SEQ_transform_seqbase_shuffle(ed->seqbasep, seq, scene);
    }
  }
}

static bool seq_effect_add_properties_poll(const bContext *UNUSED(C),
                                           wmOperator *op,
                                           const PropertyRNA *prop)
{
  const char *prop_id = RNA_property_identifier(prop);
  int type = RNA_enum_get(op->ptr, "type");

  /* Hide start/end frames for effect strips that are locked to their parents' location. */
  if (SEQ_effect_get_num_inputs(type) != 0) {
    if (STR_ELEM(prop_id, "frame_start", "frame_end")) {
      return false;
    }
  }
  if ((type != SEQ_TYPE_COLOR) && (STREQ(prop_id, "color"))) {
    return false;
  }

  return true;
}

static int sequencer_add_scene_strip_exec(bContext *C, wmOperator *op)
{
  Main *bmain = CTX_data_main(C);
  Scene *scene = CTX_data_scene(C);
  const Editing *ed = SEQ_editing_ensure(scene);
  Scene *sce_seq = BLI_findlink(&bmain->scenes, RNA_enum_get(op->ptr, "scene"));

  if (sce_seq == NULL) {
    BKE_report(op->reports, RPT_ERROR, "Scene not found");
    return OPERATOR_CANCELLED;
  }

  if (RNA_boolean_get(op->ptr, "replace_sel")) {
    ED_sequencer_deselect_all(scene);
  }

  SeqLoadData load_data;
  load_data_init_from_operator(&load_data, C, op);
  load_data.scene = sce_seq;

  Sequence *seq = SEQ_add_scene_strip(scene, ed->seqbasep, &load_data);
  seq_load_apply_generic_options(C, op, seq);

  DEG_id_tag_update(&scene->id, ID_RECALC_SEQUENCER_STRIPS);
  DEG_relations_tag_update(bmain);
  WM_event_add_notifier(C, NC_SCENE | ND_SEQUENCER, scene);

  return OPERATOR_FINISHED;
}

static void sequencer_disable_one_time_properties(bContext *C, wmOperator *op)
{
  Editing *ed = SEQ_editing_get(CTX_data_scene(C));
  /* Disable following properties if there are any existing strips, unless overridden by user. */
  if (ed && ed->seqbasep && ed->seqbasep->first) {
    if (RNA_struct_find_property(op->ptr, "use_framerate")) {
      RNA_boolean_set(op->ptr, "use_framerate", false);
    }
    if (RNA_struct_find_property(op->ptr, "set_view_transform")) {
      RNA_boolean_set(op->ptr, "set_view_transform", false);
    }
  }
}

static int sequencer_add_scene_strip_invoke(bContext *C, wmOperator *op, const wmEvent *event)
{
  sequencer_disable_one_time_properties(C, op);
  if (!RNA_struct_property_is_set(op->ptr, "scene")) {
    return WM_enum_search_invoke(C, op, event);
  }

  sequencer_generic_invoke_xy__internal(C, op, 0, SEQ_TYPE_SCENE);
  return sequencer_add_scene_strip_exec(C, op);
}

void SEQUENCER_OT_scene_strip_add(struct wmOperatorType *ot)
{
  PropertyRNA *prop;

  /* Identifiers. */
  ot->name = "Add Scene Strip";
  ot->idname = "SEQUENCER_OT_scene_strip_add";
  ot->description = "Add a strip to the sequencer using a blender scene as a source";

  /* Api callbacks. */
  ot->invoke = sequencer_add_scene_strip_invoke;
  ot->exec = sequencer_add_scene_strip_exec;
  ot->poll = ED_operator_sequencer_active_editable;

  /* Flags. */
  ot->flag = OPTYPE_REGISTER | OPTYPE_UNDO;

  sequencer_generic_props__internal(ot, SEQPROP_STARTFRAME);
  prop = RNA_def_enum(ot->srna, "scene", DummyRNA_NULL_items, 0, "Scene", "");
  RNA_def_enum_funcs(prop, RNA_scene_without_active_itemf);
  RNA_def_property_flag(prop, PROP_ENUM_NO_TRANSLATE);
  ot->prop = prop;
}

static int sequencer_add_movieclip_strip_exec(bContext *C, wmOperator *op)
{
  Main *bmain = CTX_data_main(C);
  Scene *scene = CTX_data_scene(C);
  const Editing *ed = SEQ_editing_ensure(scene);
  MovieClip *clip = BLI_findlink(&bmain->movieclips, RNA_enum_get(op->ptr, "clip"));

  if (clip == NULL) {
    BKE_report(op->reports, RPT_ERROR, "Movie clip not found");
    return OPERATOR_CANCELLED;
  }

  if (RNA_boolean_get(op->ptr, "replace_sel")) {
    ED_sequencer_deselect_all(scene);
  }

  SeqLoadData load_data;
  load_data_init_from_operator(&load_data, C, op);
  load_data.clip = clip;

  Sequence *seq = SEQ_add_movieclip_strip(scene, ed->seqbasep, &load_data);
  seq_load_apply_generic_options(C, op, seq);

  DEG_id_tag_update(&scene->id, ID_RECALC_SEQUENCER_STRIPS);
  WM_event_add_notifier(C, NC_SCENE | ND_SEQUENCER, scene);

  return OPERATOR_FINISHED;
}

static int sequencer_add_movieclip_strip_invoke(bContext *C, wmOperator *op, const wmEvent *event)
{
  if (!RNA_struct_property_is_set(op->ptr, "clip")) {
    return WM_enum_search_invoke(C, op, event);
  }

  sequencer_generic_invoke_xy__internal(C, op, 0, SEQ_TYPE_MOVIECLIP);
  return sequencer_add_movieclip_strip_exec(C, op);
}

void SEQUENCER_OT_movieclip_strip_add(struct wmOperatorType *ot)
{
  PropertyRNA *prop;

  /* Identifiers. */
  ot->name = "Add MovieClip Strip";
  ot->idname = "SEQUENCER_OT_movieclip_strip_add";
  ot->description = "Add a movieclip strip to the sequencer";

  /* Api callbacks. */
  ot->invoke = sequencer_add_movieclip_strip_invoke;
  ot->exec = sequencer_add_movieclip_strip_exec;
  ot->poll = ED_operator_sequencer_active_editable;

  /* Flags. */
  ot->flag = OPTYPE_REGISTER | OPTYPE_UNDO;

  sequencer_generic_props__internal(ot, SEQPROP_STARTFRAME);
  prop = RNA_def_enum(ot->srna, "clip", DummyRNA_NULL_items, 0, "Clip", "");
  RNA_def_enum_funcs(prop, RNA_movieclip_itemf);
  RNA_def_property_translation_context(prop, BLT_I18NCONTEXT_ID_MOVIECLIP);
  RNA_def_property_flag(prop, PROP_ENUM_NO_TRANSLATE);
  ot->prop = prop;
}

static int sequencer_add_mask_strip_exec(bContext *C, wmOperator *op)
{
  Main *bmain = CTX_data_main(C);
  Scene *scene = CTX_data_scene(C);
  const Editing *ed = SEQ_editing_ensure(scene);
  Mask *mask = BLI_findlink(&bmain->masks, RNA_enum_get(op->ptr, "mask"));

  if (mask == NULL) {
    BKE_report(op->reports, RPT_ERROR, "Mask not found");
    return OPERATOR_CANCELLED;
  }

  if (RNA_boolean_get(op->ptr, "replace_sel")) {
    ED_sequencer_deselect_all(scene);
  }

  SeqLoadData load_data;
  load_data_init_from_operator(&load_data, C, op);
  load_data.mask = mask;

  Sequence *seq = SEQ_add_mask_strip(scene, ed->seqbasep, &load_data);
  seq_load_apply_generic_options(C, op, seq);

  DEG_id_tag_update(&scene->id, ID_RECALC_SEQUENCER_STRIPS);
  WM_event_add_notifier(C, NC_SCENE | ND_SEQUENCER, scene);

  return OPERATOR_FINISHED;
}

static int sequencer_add_mask_strip_invoke(bContext *C, wmOperator *op, const wmEvent *event)
{
  if (!RNA_struct_property_is_set(op->ptr, "mask")) {
    return WM_enum_search_invoke(C, op, event);
  }

  sequencer_generic_invoke_xy__internal(C, op, 0, SEQ_TYPE_MASK);
  return sequencer_add_mask_strip_exec(C, op);
}

void SEQUENCER_OT_mask_strip_add(struct wmOperatorType *ot)
{
  PropertyRNA *prop;

  /* Identifiers. */
  ot->name = "Add Mask Strip";
  ot->idname = "SEQUENCER_OT_mask_strip_add";
  ot->description = "Add a mask strip to the sequencer";

  /* Api callbacks. */
  ot->invoke = sequencer_add_mask_strip_invoke;
  ot->exec = sequencer_add_mask_strip_exec;
  ot->poll = ED_operator_sequencer_active_editable;

  /* Flags. */
  ot->flag = OPTYPE_REGISTER | OPTYPE_UNDO;

  sequencer_generic_props__internal(ot, SEQPROP_STARTFRAME);
  prop = RNA_def_enum(ot->srna, "mask", DummyRNA_NULL_items, 0, "Mask", "");
  RNA_def_enum_funcs(prop, RNA_mask_itemf);
  RNA_def_property_flag(prop, PROP_ENUM_NO_TRANSLATE);
  ot->prop = prop;
}

static void sequencer_add_init(bContext *UNUSED(C), wmOperator *op)
{
  op->customdata = MEM_callocN(sizeof(SequencerAddData), __func__);
}

static void sequencer_add_cancel(bContext *UNUSED(C), wmOperator *op)
{
  MEM_SAFE_FREE(op->customdata);
}

static bool sequencer_add_draw_check_fn(PointerRNA *UNUSED(ptr),
                                        PropertyRNA *prop,
                                        void *UNUSED(user_data))
{
  const char *prop_id = RNA_property_identifier(prop);

  return !(STR_ELEM(prop_id, "filepath", "directory", "filename"));
}

/* Strips are added in context of timeline which has different preview size than actual preview. We
 * must search for preview area. In most cases there will be only one preview area, but there can
 * be more with different preview sizes. */
static IMB_Proxy_Size seq_get_proxy_size_flags(bContext *C)
{
  bScreen *screen = CTX_wm_screen(C);
  IMB_Proxy_Size proxy_sizes = 0;
  LISTBASE_FOREACH (ScrArea *, area, &screen->areabase) {
    LISTBASE_FOREACH (SpaceLink *, sl, &area->spacedata) {
      switch (sl->spacetype) {
        case SPACE_SEQ: {
          SpaceSeq *sseq = (SpaceSeq *)sl;
          if (!ELEM(sseq->view, SEQ_VIEW_PREVIEW, SEQ_VIEW_SEQUENCE_PREVIEW)) {
            continue;
          }
          proxy_sizes |= SEQ_rendersize_to_proxysize(sseq->render_size);
        }
      }
    }
  }
  return proxy_sizes;
}

static void seq_build_proxy(bContext *C, Sequence *seq)
{
  if (U.sequencer_proxy_setup != USER_SEQ_PROXY_SETUP_AUTOMATIC) {
    return;
  }

  /* Enable and set proxy size. */
  SEQ_proxy_set(seq, true);
  seq->strip->proxy->build_size_flags = seq_get_proxy_size_flags(C);
  seq->strip->proxy->build_flags |= SEQ_PROXY_SKIP_EXISTING;

  /* Build proxy. */
  GSet *file_list = BLI_gset_new(BLI_ghashutil_strhash_p, BLI_ghashutil_strcmp, "file list");
  wmJob *wm_job = ED_seq_proxy_wm_job_get(C);
  ProxyJob *pj = ED_seq_proxy_job_get(C, wm_job);
  SEQ_proxy_rebuild_context(pj->main, pj->depsgraph, pj->scene, seq, file_list, &pj->queue);
  BLI_gset_free(file_list, MEM_freeN);

  if (!WM_jobs_is_running(wm_job)) {
    G.is_break = false;
    WM_jobs_start(CTX_wm_manager(C), wm_job);
  }

  ED_area_tag_redraw(CTX_wm_area(C));
}

static void sequencer_add_movie_multiple_strips(bContext *C,
                                                wmOperator *op,
                                                SeqLoadData *load_data)
{
  Main *bmain = CTX_data_main(C);
  Scene *scene = CTX_data_scene(C);
  const Editing *ed = SEQ_editing_ensure(scene);

  RNA_BEGIN (op->ptr, itemptr, "files") {
    char dir_only[FILE_MAX];
    char file_only[FILE_MAX];
    RNA_string_get(op->ptr, "directory", dir_only);
    RNA_string_get(&itemptr, "name", file_only);
    BLI_join_dirfile(load_data->path, sizeof(load_data->path), dir_only, file_only);
    BLI_strncpy(load_data->name, file_only, sizeof(load_data->name));
    Sequence *seq_movie = NULL;
    Sequence *seq_sound = NULL;
    double video_start_offset = -1;
    double audio_start_offset = 0;

    if (RNA_boolean_get(op->ptr, "sound")) {
      SoundStreamInfo sound_info;
      if (BKE_sound_stream_info_get(bmain, load_data->path, 0, &sound_info)) {
        audio_start_offset = video_start_offset = sound_info.start;
      }
    }

    load_data->channel++;
    seq_movie = SEQ_add_movie_strip(bmain, scene, ed->seqbasep, load_data, &video_start_offset);
    load_data->channel--;
    if (seq_movie == NULL) {
      BKE_reportf(op->reports, RPT_ERROR, "File '%s' could not be loaded", load_data->path);
    }
    else {
      if (RNA_boolean_get(op->ptr, "sound")) {
        int minimum_frame_offset = MIN2(video_start_offset, audio_start_offset) * FPS;

        int video_frame_offset = video_start_offset * FPS;
        int audio_frame_offset = audio_start_offset * FPS;

        double video_frame_remainder = video_start_offset * FPS - video_frame_offset;
        double audio_frame_remainder = audio_start_offset * FPS - audio_frame_offset;

        double audio_skip = (video_frame_remainder - audio_frame_remainder) / FPS;

        video_frame_offset -= minimum_frame_offset;
        audio_frame_offset -= minimum_frame_offset;

        load_data->start_frame += audio_frame_offset;
        seq_sound = SEQ_add_sound_strip(bmain, scene, ed->seqbasep, load_data, audio_skip);

        int min_startdisp = MIN2(seq_movie->startdisp, seq_sound->startdisp);
        int max_enddisp = MAX2(seq_movie->enddisp, seq_sound->enddisp);

        load_data->start_frame += max_enddisp - min_startdisp - audio_frame_offset;
      }
      else {
        load_data->start_frame += seq_movie->enddisp - seq_movie->startdisp;
      }
      seq_load_apply_generic_options(C, op, seq_sound);
      seq_load_apply_generic_options(C, op, seq_movie);
      seq_build_proxy(C, seq_movie);
    }
  }
  RNA_END;
}

static bool sequencer_add_movie_single_strip(bContext *C, wmOperator *op, SeqLoadData *load_data)
{
  Main *bmain = CTX_data_main(C);
  Scene *scene = CTX_data_scene(C);
  const Editing *ed = SEQ_editing_ensure(scene);

  Sequence *seq_movie = NULL;
  Sequence *seq_sound = NULL;
  double video_start_offset = -1;
  double audio_start_offset = 0;

  if (RNA_boolean_get(op->ptr, "sound")) {
    SoundStreamInfo sound_info;
    if (BKE_sound_stream_info_get(bmain, load_data->path, 0, &sound_info)) {
      audio_start_offset = video_start_offset = sound_info.start;
    }
  }

  load_data->channel++;
  seq_movie = SEQ_add_movie_strip(bmain, scene, ed->seqbasep, load_data, &video_start_offset);
  load_data->channel--;

  if (seq_movie == NULL) {
    BKE_reportf(op->reports, RPT_ERROR, "File '%s' could not be loaded", load_data->path);
    return false;
  }
  if (RNA_boolean_get(op->ptr, "sound")) {
    int minimum_frame_offset = MIN2(video_start_offset, audio_start_offset) * FPS;

    int video_frame_offset = video_start_offset * FPS;
    int audio_frame_offset = audio_start_offset * FPS;

    double video_frame_remainder = video_start_offset * FPS - video_frame_offset;
    double audio_frame_remainder = audio_start_offset * FPS - audio_frame_offset;

    double audio_skip = (video_frame_remainder - audio_frame_remainder) / FPS;

    video_frame_offset -= minimum_frame_offset;
    audio_frame_offset -= minimum_frame_offset;

    load_data->start_frame += audio_frame_offset;
    seq_sound = SEQ_add_sound_strip(bmain, scene, ed->seqbasep, load_data, audio_skip);
  }
  seq_load_apply_generic_options(C, op, seq_sound);
  seq_load_apply_generic_options(C, op, seq_movie);
  seq_build_proxy(C, seq_movie);

  return true;
}

static int sequencer_add_movie_strip_exec(bContext *C, wmOperator *op)
{
  Main *bmain = CTX_data_main(C);
  Scene *scene = CTX_data_scene(C);
  SeqLoadData load_data;

  load_data_init_from_operator(&load_data, C, op);

  if (RNA_boolean_get(op->ptr, "replace_sel")) {
    ED_sequencer_deselect_all(scene);
  }

  const int tot_files = RNA_property_collection_length(op->ptr,
                                                       RNA_struct_find_property(op->ptr, "files"));
  if (tot_files > 1) {
    sequencer_add_movie_multiple_strips(C, op, &load_data);
  }
  else {
    if (!sequencer_add_movie_single_strip(C, op, &load_data)) {
      sequencer_add_cancel(C, op);
      return OPERATOR_CANCELLED;
    }
  }

  /* Free custom data. */
  sequencer_add_cancel(C, op);

  DEG_relations_tag_update(bmain);
  DEG_id_tag_update(&scene->id, ID_RECALC_SEQUENCER_STRIPS);
  WM_event_add_notifier(C, NC_SCENE | ND_SEQUENCER, scene);

  return OPERATOR_FINISHED;
}

static int sequencer_add_movie_strip_invoke(bContext *C,
                                            wmOperator *op,
                                            const wmEvent *UNUSED(event))
{
  PropertyRNA *prop;
  Scene *scene = CTX_data_scene(C);

  sequencer_disable_one_time_properties(C, op);

  RNA_enum_set(op->ptr, "fit_method", SEQ_tool_settings_fit_method_get(scene));

  /* This is for drag and drop. */
  if ((RNA_struct_property_is_set(op->ptr, "files") && RNA_collection_length(op->ptr, "files")) ||
      RNA_struct_property_is_set(op->ptr, "filepath")) {
    sequencer_generic_invoke_xy__internal(C, op, SEQPROP_NOPATHS, SEQ_TYPE_MOVIE);
    return sequencer_add_movie_strip_exec(C, op);
  }

  sequencer_generic_invoke_xy__internal(C, op, 0, SEQ_TYPE_MOVIE);
  sequencer_add_init(C, op);

  /* Show multiview save options only if scene use multiview. */
  prop = RNA_struct_find_property(op->ptr, "show_multiview");
  RNA_property_boolean_set(op->ptr, prop, (scene->r.scemode & R_MULTIVIEW) != 0);

  WM_event_add_fileselect(C, op);
  return OPERATOR_RUNNING_MODAL;
}

static void sequencer_add_draw(bContext *UNUSED(C), wmOperator *op)
{
  uiLayout *layout = op->layout;
  SequencerAddData *sad = op->customdata;
  ImageFormatData *imf = &sad->im_format;
  PointerRNA imf_ptr;

  /* Main draw call. */
  uiDefAutoButsRNA(
      layout, op->ptr, sequencer_add_draw_check_fn, NULL, NULL, UI_BUT_LABEL_ALIGN_NONE, false);

  /* Image template. */
  RNA_pointer_create(NULL, &RNA_ImageFormatSettings, imf, &imf_ptr);

  /* Multiview template. */
  if (RNA_boolean_get(op->ptr, "show_multiview")) {
    uiTemplateImageFormatViews(layout, &imf_ptr, op->ptr);
  }
}

void SEQUENCER_OT_movie_strip_add(struct wmOperatorType *ot)
{

  /* Identifiers. */
  ot->name = "Add Movie Strip";
  ot->idname = "SEQUENCER_OT_movie_strip_add";
  ot->description = "Add a movie strip to the sequencer";

  /* Api callbacks. */
  ot->invoke = sequencer_add_movie_strip_invoke;
  ot->exec = sequencer_add_movie_strip_exec;
  ot->cancel = sequencer_add_cancel;
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
  sequencer_generic_props__internal(
      ot, SEQPROP_STARTFRAME | SEQPROP_FIT_METHOD | SEQPROP_VIEW_TRANSFORM);
  RNA_def_boolean(ot->srna, "sound", true, "Sound", "Load sound with the movie");
  RNA_def_boolean(ot->srna,
                  "use_framerate",
                  true,
                  "Use Movie Framerate",
                  "Use framerate from the movie to keep sound and video in sync");
}

static void sequencer_add_sound_multiple_strips(bContext *C,
                                                wmOperator *op,
                                                SeqLoadData *load_data)
{
  Main *bmain = CTX_data_main(C);
  Scene *scene = CTX_data_scene(C);
  Editing *ed = SEQ_editing_ensure(scene);

  RNA_BEGIN (op->ptr, itemptr, "files") {
    char dir_only[FILE_MAX];
    char file_only[FILE_MAX];
    RNA_string_get(op->ptr, "directory", dir_only);
    RNA_string_get(&itemptr, "name", file_only);
    BLI_join_dirfile(load_data->path, sizeof(load_data->path), dir_only, file_only);
    BLI_strncpy(load_data->name, file_only, sizeof(load_data->name));
    Sequence *seq = SEQ_add_sound_strip(bmain, scene, ed->seqbasep, load_data, 0.0f);
    if (seq == NULL) {
      BKE_reportf(op->reports, RPT_ERROR, "File '%s' could not be loaded", load_data->path);
    }
    else {
      seq_load_apply_generic_options(C, op, seq);
      load_data->start_frame += seq->enddisp - seq->startdisp;
    }
  }
  RNA_END;
}

static bool sequencer_add_sound_single_strip(bContext *C, wmOperator *op, SeqLoadData *load_data)
{
  Main *bmain = CTX_data_main(C);
  Scene *scene = CTX_data_scene(C);
  Editing *ed = SEQ_editing_ensure(scene);

  Sequence *seq = SEQ_add_sound_strip(bmain, scene, ed->seqbasep, load_data, 0.0f);
  if (seq == NULL) {
    BKE_reportf(op->reports, RPT_ERROR, "File '%s' could not be loaded", load_data->path);
    return false;
  }
  seq_load_apply_generic_options(C, op, seq);

  return true;
}

static int sequencer_add_sound_strip_exec(bContext *C, wmOperator *op)
{
  Main *bmain = CTX_data_main(C);
  Scene *scene = CTX_data_scene(C);
  SeqLoadData load_data;
  load_data_init_from_operator(&load_data, C, op);

  if (RNA_boolean_get(op->ptr, "replace_sel")) {
    ED_sequencer_deselect_all(scene);
  }

  const int tot_files = RNA_property_collection_length(op->ptr,
                                                       RNA_struct_find_property(op->ptr, "files"));
  if (tot_files > 1) {
    sequencer_add_sound_multiple_strips(C, op, &load_data);
  }
  else {
    if (!sequencer_add_sound_single_strip(C, op, &load_data)) {
      return OPERATOR_CANCELLED;
    }
  }

  if (op->customdata) {
    MEM_freeN(op->customdata);
  }

  DEG_relations_tag_update(bmain);
  DEG_id_tag_update(&scene->id, ID_RECALC_SEQUENCER_STRIPS);
  WM_event_add_notifier(C, NC_SCENE | ND_SEQUENCER, scene);

  return OPERATOR_FINISHED;
}

static int sequencer_add_sound_strip_invoke(bContext *C,
                                            wmOperator *op,
                                            const wmEvent *UNUSED(event))
{
  /* This is for drag and drop. */
  if ((RNA_struct_property_is_set(op->ptr, "files") && RNA_collection_length(op->ptr, "files")) ||
      RNA_struct_property_is_set(op->ptr, "filepath")) {
    sequencer_generic_invoke_xy__internal(C, op, SEQPROP_NOPATHS, SEQ_TYPE_SOUND_RAM);
    return sequencer_add_sound_strip_exec(C, op);
  }

  sequencer_generic_invoke_xy__internal(C, op, 0, SEQ_TYPE_SOUND_RAM);

  WM_event_add_fileselect(C, op);
  return OPERATOR_RUNNING_MODAL;
}

void SEQUENCER_OT_sound_strip_add(struct wmOperatorType *ot)
{

  /* Identifiers. */
  ot->name = "Add Sound Strip";
  ot->idname = "SEQUENCER_OT_sound_strip_add";
  ot->description = "Add a sound strip to the sequencer";

  /* Api callbacks. */
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
  sequencer_generic_props__internal(ot, SEQPROP_STARTFRAME);
  RNA_def_boolean(ot->srna, "cache", false, "Cache", "Cache the sound in memory");
  RNA_def_boolean(ot->srna, "mono", false, "Mono", "Merge all the sound's channels into one");
}

int sequencer_image_seq_get_minmax_frame(wmOperator *op,
                                         int sfra,
                                         int *r_minframe,
                                         int *r_numdigits)
{
  int minframe = INT32_MAX, maxframe = INT32_MIN;
  int numdigits = 0;

  RNA_BEGIN (op->ptr, itemptr, "files") {
    char *filename;
    int frame;
    filename = RNA_string_get_alloc(&itemptr, "name", NULL, 0, NULL);

    if (filename) {
      if (BLI_path_frame_get(filename, &frame, &numdigits)) {
        minframe = min_ii(minframe, frame);
        maxframe = max_ii(maxframe, frame);
      }

      MEM_freeN(filename);
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

void sequencer_image_seq_reserve_frames(
    wmOperator *op, StripElem *se, int len, int minframe, int numdigits)
{
  char *filename = NULL;
  RNA_BEGIN (op->ptr, itemptr, "files") {
    filename = RNA_string_get_alloc(&itemptr, "name", NULL, 0, NULL);
    break;
  }
  RNA_END;

  if (filename) {
    char ext[PATH_MAX];
    char filename_stripped[PATH_MAX];
    /* Strip the frame from filename and substitute with `#`. */
    BLI_path_frame_strip(filename, ext);

    for (int i = 0; i < len; i++, se++) {
      BLI_strncpy(filename_stripped, filename, sizeof(filename_stripped));
      BLI_path_frame(filename_stripped, minframe + i, numdigits);
      BLI_snprintf(se->name, sizeof(se->name), "%s%s", filename_stripped, ext);
    }

    MEM_freeN(filename);
  }
}

static int sequencer_add_image_strip_calculate_length(wmOperator *op,
                                                      const int start_frame,
                                                      int *minframe,
                                                      int *numdigits)
{
  const bool use_placeholders = RNA_boolean_get(op->ptr, "use_placeholders");

  if (use_placeholders) {
    return sequencer_image_seq_get_minmax_frame(op, start_frame, minframe, numdigits);
  }
  return RNA_property_collection_length(op->ptr, RNA_struct_find_property(op->ptr, "files"));
}

static void sequencer_add_image_strip_load_files(
    wmOperator *op, Sequence *seq, SeqLoadData *load_data, const int minframe, const int numdigits)
{
  const bool use_placeholders = RNA_boolean_get(op->ptr, "use_placeholders");
  /* size of Strip->dir. */
  char directory[768];
  BLI_split_dir_part(load_data->path, directory, sizeof(directory));
  SEQ_add_image_set_directory(seq, directory);

  if (use_placeholders) {
    sequencer_image_seq_reserve_frames(
        op, seq->strip->stripdata, load_data->image.len, minframe, numdigits);
  }
  else {
    size_t strip_frame = 0;
    RNA_BEGIN (op->ptr, itemptr, "files") {
      char *filename = RNA_string_get_alloc(&itemptr, "name", NULL, 0, NULL);
      SEQ_add_image_load_file(seq, strip_frame, filename);
      MEM_freeN(filename);
      strip_frame++;
    }
    RNA_END;
  }
}

static int sequencer_add_image_strip_exec(bContext *C, wmOperator *op)
{
  Scene *scene = CTX_data_scene(C);
  Editing *ed = SEQ_editing_ensure(scene);

  SeqLoadData load_data;
  load_data_init_from_operator(&load_data, C, op);

  int minframe, numdigits;
  load_data.image.len = sequencer_add_image_strip_calculate_length(
      op, load_data.start_frame, &minframe, &numdigits);
  if (load_data.image.len == 0) {
    sequencer_add_cancel(C, op);
    return OPERATOR_CANCELLED;
  }

  if (RNA_boolean_get(op->ptr, "replace_sel")) {
    ED_sequencer_deselect_all(scene);
  }

  Sequence *seq = SEQ_add_image_strip(CTX_data_main(C), scene, ed->seqbasep, &load_data);
  sequencer_add_image_strip_load_files(op, seq, &load_data, minframe, numdigits);
  SEQ_add_image_init_alpha_mode(seq);

  /* Adjust length. */
  if (load_data.image.len == 1) {
    SEQ_transform_set_right_handle_frame(seq, load_data.image.end_frame);
    SEQ_time_update_sequence(scene, SEQ_active_seqbase_get(ed), seq);
  }

  seq_load_apply_generic_options(C, op, seq);

  DEG_id_tag_update(&scene->id, ID_RECALC_SEQUENCER_STRIPS);
  WM_event_add_notifier(C, NC_SCENE | ND_SEQUENCER, scene);

  /* Free custom data. */
  sequencer_add_cancel(C, op);

  return OPERATOR_FINISHED;
}

static int sequencer_add_image_strip_invoke(bContext *C,
                                            wmOperator *op,
                                            const wmEvent *UNUSED(event))
{
  PropertyRNA *prop;
  Scene *scene = CTX_data_scene(C);

  sequencer_disable_one_time_properties(C, op);

  RNA_enum_set(op->ptr, "fit_method", SEQ_tool_settings_fit_method_get(scene));

  /* Name set already by drag and drop. */
  if (RNA_struct_property_is_set(op->ptr, "files") && RNA_collection_length(op->ptr, "files")) {
    sequencer_generic_invoke_xy__internal(
        C, op, SEQPROP_ENDFRAME | SEQPROP_NOPATHS, SEQ_TYPE_IMAGE);
    return sequencer_add_image_strip_exec(C, op);
  }

  sequencer_generic_invoke_xy__internal(C, op, SEQPROP_ENDFRAME, SEQ_TYPE_IMAGE);
  sequencer_add_init(C, op);

  /* Show multiview save options only if scene use multiview. */
  prop = RNA_struct_find_property(op->ptr, "show_multiview");
  RNA_property_boolean_set(op->ptr, prop, (scene->r.scemode & R_MULTIVIEW) != 0);

  WM_event_add_fileselect(C, op);
  return OPERATOR_RUNNING_MODAL;
}

void SEQUENCER_OT_image_strip_add(struct wmOperatorType *ot)
{

  /* Identifiers. */
  ot->name = "Add Image Strip";
  ot->idname = "SEQUENCER_OT_image_strip_add";
  ot->description = "Add an image or image sequence to the sequencer";

  /* Api callbacks. */
  ot->invoke = sequencer_add_image_strip_invoke;
  ot->exec = sequencer_add_image_strip_exec;
  ot->cancel = sequencer_add_cancel;
  ot->ui = sequencer_add_draw;
  ot->poll = ED_operator_sequencer_active_editable;

  /* Flags. */
  ot->flag = OPTYPE_REGISTER | OPTYPE_UNDO;

  WM_operator_properties_filesel(ot,
                                 FILE_TYPE_FOLDER | FILE_TYPE_IMAGE,
                                 FILE_SPECIAL,
                                 FILE_OPENFILE,
                                 WM_FILESEL_DIRECTORY | WM_FILESEL_RELPATH | WM_FILESEL_FILES |
                                     WM_FILESEL_SHOW_PROPS | WM_FILESEL_DIRECTORY,
                                 FILE_DEFAULTDISPLAY,
                                 FILE_SORT_DEFAULT);
  sequencer_generic_props__internal(
      ot, SEQPROP_STARTFRAME | SEQPROP_ENDFRAME | SEQPROP_FIT_METHOD | SEQPROP_VIEW_TRANSFORM);

  RNA_def_boolean(ot->srna,
                  "use_placeholders",
                  false,
                  "Use Placeholders",
                  "Use placeholders for missing frames of the strip");
}

static int sequencer_add_effect_strip_exec(bContext *C, wmOperator *op)
{
  Scene *scene = CTX_data_scene(C);
  Editing *ed = SEQ_editing_ensure(scene);
  const char *error_msg;

  SeqLoadData load_data;
  load_data_init_from_operator(&load_data, C, op);
  load_data.effect.type = RNA_enum_get(op->ptr, "type");

  Sequence *seq1, *seq2, *seq3;
  if (!seq_effect_find_selected(
          scene, NULL, load_data.effect.type, &seq1, &seq2, &seq3, &error_msg)) {
    BKE_report(op->reports, RPT_ERROR, error_msg);
    return OPERATOR_CANCELLED;
  }

  if (RNA_boolean_get(op->ptr, "replace_sel")) {
    ED_sequencer_deselect_all(scene);
  }

  load_data.effect.seq1 = seq1;
  load_data.effect.seq2 = seq2;
  load_data.effect.seq3 = seq3;

  /* Set channel. If unset, use lowest free one above strips. */
  if (!RNA_struct_property_is_set(op->ptr, "channel")) {
    if (seq1 != NULL) {
      int chan = max_iii(
          seq1 ? seq1->machine : 0, seq2 ? seq2->machine : 0, seq3 ? seq3->machine : 0);
      if (chan < MAXSEQ) {
        load_data.channel = chan;
      }
    }
  }

  Sequence *seq = SEQ_add_effect_strip(scene, ed->seqbasep, &load_data);
  seq_load_apply_generic_options(C, op, seq);

  if (seq->type == SEQ_TYPE_COLOR) {
    SolidColorVars *colvars = (SolidColorVars *)seq->effectdata;
    RNA_float_get_array(op->ptr, "color", colvars->col);
  }

  DEG_id_tag_update(&scene->id, ID_RECALC_SEQUENCER_STRIPS);
  WM_event_add_notifier(C, NC_SCENE | ND_SEQUENCER, scene);

  return OPERATOR_FINISHED;
}

static int sequencer_add_effect_strip_invoke(bContext *C,
                                             wmOperator *op,
                                             const wmEvent *UNUSED(event))
{
  bool is_type_set = RNA_struct_property_is_set(op->ptr, "type");
  int type = -1;
  int prop_flag = SEQPROP_ENDFRAME | SEQPROP_NOPATHS;

  if (is_type_set) {
    type = RNA_enum_get(op->ptr, "type");

    /* When invoking an effect strip which uses inputs, skip initializing the channel from the
     * mouse. */
    if (SEQ_effect_get_num_inputs(type) != 0) {
      prop_flag |= SEQPROP_NOCHAN;
    }
  }

  sequencer_generic_invoke_xy__internal(C, op, prop_flag, type);

  return sequencer_add_effect_strip_exec(C, op);
}

static char *sequencer_add_effect_strip_desc(bContext *UNUSED(C),
                                             wmOperatorType *UNUSED(op),
                                             PointerRNA *ptr)
{
  const int type = RNA_enum_get(ptr, "type");

  switch (type) {
    case SEQ_TYPE_CROSS:
      return BLI_strdup(TIP_("Add a crossfade transition to the sequencer"));
    case SEQ_TYPE_ADD:
      return BLI_strdup(TIP_("Add an add effect strip to the sequencer"));
    case SEQ_TYPE_SUB:
      return BLI_strdup(TIP_("Add a subtract effect strip to the sequencer"));
    case SEQ_TYPE_ALPHAOVER:
      return BLI_strdup(TIP_("Add an alpha over effect strip to the sequencer"));
    case SEQ_TYPE_ALPHAUNDER:
      return BLI_strdup(TIP_("Add an alpha under effect strip to the sequencer"));
    case SEQ_TYPE_GAMCROSS:
      return BLI_strdup(TIP_("Add a gamma cross transition to the sequencer"));
    case SEQ_TYPE_MUL:
      return BLI_strdup(TIP_("Add a multiply effect strip to the sequencer"));
    case SEQ_TYPE_OVERDROP:
      return BLI_strdup(TIP_("Add an alpha over drop effect strip to the sequencer"));
    case SEQ_TYPE_WIPE:
      return BLI_strdup(TIP_("Add a wipe transition to the sequencer"));
    case SEQ_TYPE_GLOW:
      return BLI_strdup(TIP_("Add a glow effect strip to the sequencer"));
    case SEQ_TYPE_TRANSFORM:
      return BLI_strdup(TIP_("Add a transform effect strip to the sequencer"));
    case SEQ_TYPE_COLOR:
      return BLI_strdup(TIP_("Add a color strip to the sequencer"));
    case SEQ_TYPE_SPEED:
      return BLI_strdup(TIP_("Add a speed effect strip to the sequencer"));
    case SEQ_TYPE_MULTICAM:
      return BLI_strdup(TIP_("Add a multicam selector effect strip to the sequencer"));
    case SEQ_TYPE_ADJUSTMENT:
      return BLI_strdup(TIP_("Add an adjustment layer effect strip to the sequencer"));
    case SEQ_TYPE_GAUSSIAN_BLUR:
      return BLI_strdup(TIP_("Add a gaussian blur effect strip to the sequencer"));
    case SEQ_TYPE_TEXT:
      return BLI_strdup(TIP_("Add a text strip to the sequencer"));
    case SEQ_TYPE_COLORMIX:
      return BLI_strdup(TIP_("Add a color mix effect strip to the sequencer"));
    default:
      break;
  }

  /* Use default description. */
  return NULL;
}

void SEQUENCER_OT_effect_strip_add(struct wmOperatorType *ot)
{
  PropertyRNA *prop;

  /* Identifiers. */
  ot->name = "Add Effect Strip";
  ot->idname = "SEQUENCER_OT_effect_strip_add";
  ot->description = "Add an effect to the sequencer, most are applied on top of existing strips";

  /* Api callbacks. */
  ot->invoke = sequencer_add_effect_strip_invoke;
  ot->exec = sequencer_add_effect_strip_exec;
  ot->poll = ED_operator_sequencer_active_editable;
  ot->poll_property = seq_effect_add_properties_poll;
  ot->get_description = sequencer_add_effect_strip_desc;

  /* Flags. */
  ot->flag = OPTYPE_REGISTER | OPTYPE_UNDO;

  RNA_def_enum(ot->srna,
               "type",
               sequencer_prop_effect_types,
               SEQ_TYPE_CROSS,
               "Type",
               "Sequencer effect type");
  sequencer_generic_props__internal(ot, SEQPROP_STARTFRAME | SEQPROP_ENDFRAME);
  /* Only used when strip is of the Color type. */
  prop = RNA_def_float_color(ot->srna,
                             "color",
                             3,
                             NULL,
                             0.0f,
                             1.0f,
                             "Color",
                             "Initialize the strip with this color",
                             0.0f,
                             1.0f);
  RNA_def_property_subtype(prop, PROP_COLOR_GAMMA);
}
