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
#include "BLI_math.h"
#include "BLI_utildefines.h"

#include "BLT_translation.h"

#include "DNA_mask_types.h"
#include "DNA_scene_types.h"

#include "BKE_context.h"
#include "BKE_lib_id.h"
#include "BKE_main.h"
#include "BKE_mask.h"
#include "BKE_movieclip.h"
#include "BKE_report.h"
#include "BKE_sequencer.h"

#include "WM_api.h"
#include "WM_types.h"

#include "RNA_define.h"
#include "RNA_enum_types.h"

/* For menu, popup, icons, etc. */
#include "ED_screen.h"
#include "ED_sequencer.h"

#include "UI_interface.h"

#include "BKE_sound.h"

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

#define SELECT 1

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
}

static void sequencer_generic_invoke_path__internal(bContext *C,
                                                    wmOperator *op,
                                                    const char *identifier)
{
  if (RNA_struct_find_property(op->ptr, identifier)) {
    Scene *scene = CTX_data_scene(C);
    Sequence *last_seq = BKE_sequencer_active_get(scene);
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
  Editing *ed = BKE_sequencer_editing_get(scene, true);
  int cfra = (int)CFRA;
  int proximity = INT_MAX;

  if (!ed || !ed->seqbasep) {
    return 2;
  }

  for (seq = ed->seqbasep->first; seq; seq = seq->next) {
    if ((type == -1 || seq->type == type) && (seq->enddisp < cfra) &&
        (cfra - seq->enddisp < proximity)) {
      tgt = seq;
      proximity = cfra - seq->enddisp;
    }
  }

  if (tgt) {
    return tgt->machine + 1;
  }
  return 2;
}

static void sequencer_generic_invoke_xy__internal(bContext *C, wmOperator *op, int flag, int type)
{
  Scene *scene = CTX_data_scene(C);

  int cfra = (int)CFRA;

  /* Effect strips don't need a channel initialized from the mouse. */
  if (!(flag & SEQPROP_NOCHAN) && RNA_struct_property_is_set(op->ptr, "channel") == 0) {
    RNA_int_set(op->ptr, "channel", sequencer_generic_invoke_xy_guess_channel(C, type));
  }

  RNA_int_set(op->ptr, "frame_start", cfra);

  if ((flag & SEQPROP_ENDFRAME) && RNA_struct_property_is_set(op->ptr, "frame_end") == 0) {
    RNA_int_set(op->ptr, "frame_end", cfra + 25);  // XXX arbitrary but ok for now.
  }

  if (!(flag & SEQPROP_NOPATHS)) {
    sequencer_generic_invoke_path__internal(C, op, "filepath");
    sequencer_generic_invoke_path__internal(C, op, "directory");
  }
}

static void seq_load_operator_info(SeqLoadInfo *seq_load, bContext *C, wmOperator *op)
{
  Main *bmain = CTX_data_main(C);

  PropertyRNA *prop;
  const bool relative = (prop = RNA_struct_find_property(op->ptr, "relative_path")) &&
                        RNA_property_boolean_get(op->ptr, prop);
  int is_file = -1;
  memset(seq_load, 0, sizeof(SeqLoadInfo));

  seq_load->start_frame = RNA_int_get(op->ptr, "frame_start");
  seq_load->end_frame = seq_load->start_frame;
  seq_load->channel = RNA_int_get(op->ptr, "channel");
  seq_load->len = 1;

  if ((prop = RNA_struct_find_property(op->ptr, "filepath"))) {
    /* Full path, file is set by the caller. */
    RNA_property_string_get(op->ptr, prop, seq_load->path);
    is_file = 1;
  }
  else if ((prop = RNA_struct_find_property(op->ptr, "directory"))) {
    /* Full path, file is set by the caller. */
    RNA_property_string_get(op->ptr, prop, seq_load->path);
    is_file = 0;
  }

  if ((is_file != -1) && relative) {
    BLI_path_rel(seq_load->path, BKE_main_blendfile_path(bmain));
  }

  if ((prop = RNA_struct_find_property(op->ptr, "frame_end"))) {
    seq_load->end_frame = RNA_property_int_get(op->ptr, prop);
  }

  if ((prop = RNA_struct_find_property(op->ptr, "replace_sel")) &&
      RNA_property_boolean_get(op->ptr, prop)) {
    seq_load->flag |= SEQ_LOAD_REPLACE_SEL;
  }

  if ((prop = RNA_struct_find_property(op->ptr, "cache")) &&
      RNA_property_boolean_get(op->ptr, prop)) {
    seq_load->flag |= SEQ_LOAD_SOUND_CACHE;
  }

  if ((prop = RNA_struct_find_property(op->ptr, "mono")) &&
      RNA_property_boolean_get(op->ptr, prop)) {
    seq_load->flag |= SEQ_LOAD_SOUND_MONO;
  }

  if ((prop = RNA_struct_find_property(op->ptr, "sound")) &&
      RNA_property_boolean_get(op->ptr, prop)) {
    seq_load->flag |= SEQ_LOAD_MOVIE_SOUND;
  }

  if ((prop = RNA_struct_find_property(op->ptr, "use_framerate")) &&
      RNA_property_boolean_get(op->ptr, prop)) {
    seq_load->flag |= SEQ_LOAD_SYNC_FPS;
  }

  /* Create consecutive array of strips. */
  seq_load->flag |= SEQ_LOAD_FRAME_ADVANCE;

  if (is_file == 1) {
    BLI_strncpy(seq_load->name, BLI_path_basename(seq_load->path), sizeof(seq_load->name));
  }
  else if ((prop = RNA_struct_find_property(op->ptr, "files"))) {
    RNA_PROP_BEGIN (op->ptr, itemptr, prop) {
      char *name = RNA_string_get_alloc(&itemptr, "name", NULL, 0);
      BLI_strncpy(seq_load->name, name, sizeof(seq_load->name));
      MEM_freeN(name);
      break;
    }
    RNA_PROP_END;
  }

  if ((prop = RNA_struct_find_property(op->ptr, "use_multiview")) &&
      RNA_property_boolean_get(op->ptr, prop)) {
    if (op->customdata) {
      SequencerAddData *sad = op->customdata;
      ImageFormatData *imf = &sad->im_format;

      seq_load->views_format = imf->views_format;
      seq_load->flag |= SEQ_USE_VIEWS;
      seq_load->stereo3d_format = &imf->stereo3d_format;
    }
  }
}

/**
 * Apply generic operator options.
 */
static void sequencer_add_apply_overlap(bContext *C, wmOperator *op, Sequence *seq)
{
  Scene *scene = CTX_data_scene(C);
  Editing *ed = BKE_sequencer_editing_get(scene, false);

  if (RNA_boolean_get(op->ptr, "overlap") == false) {
    if (BKE_sequence_test_overlap(ed->seqbasep, seq)) {
      BKE_sequence_base_shuffle(ed->seqbasep, seq, scene);
    }
  }
}

static void sequencer_add_apply_replace_sel(bContext *C, wmOperator *op, Sequence *seq)
{
  Scene *scene = CTX_data_scene(C);

  if (RNA_boolean_get(op->ptr, "replace_sel")) {
    ED_sequencer_deselect_all(scene);
    BKE_sequencer_active_set(scene, seq);
    seq->flag |= SELECT;
  }
}

static bool seq_effect_add_properties_poll(const bContext *UNUSED(C),
                                           wmOperator *op,
                                           const PropertyRNA *prop)
{
  const char *prop_id = RNA_property_identifier(prop);
  int type = RNA_enum_get(op->ptr, "type");

  /* Hide start/end frames for effect strips that are locked to their parents' location. */
  if (BKE_sequence_effect_get_num_inputs(type) != 0) {
    if ((STREQ(prop_id, "frame_start")) || (STREQ(prop_id, "frame_end"))) {
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
  Editing *ed = BKE_sequencer_editing_get(scene, true);
  Scene *sce_seq;
  Sequence *seq;

  int start_frame, channel;
  start_frame = RNA_int_get(op->ptr, "frame_start");
  channel = RNA_int_get(op->ptr, "channel");
  sce_seq = BLI_findlink(&bmain->scenes, RNA_enum_get(op->ptr, "scene"));

  if (sce_seq == NULL) {
    BKE_report(op->reports, RPT_ERROR, "Scene not found");
    return OPERATOR_CANCELLED;
  }

  seq = BKE_sequence_alloc(ed->seqbasep, start_frame, channel, SEQ_TYPE_SCENE);
  seq->blend_mode = SEQ_TYPE_CROSS;
  seq->scene = sce_seq;
  seq->len = sce_seq->r.efra - sce_seq->r.sfra + 1;

  BLI_strncpy(seq->name + 2, sce_seq->id.name + 2, sizeof(seq->name) - 2);
  BKE_sequence_base_unique_name_recursive(&ed->seqbase, seq);

  BKE_sequence_calc_disp(scene, seq);
  BKE_sequencer_sort(scene);

  sequencer_add_apply_replace_sel(C, op, seq);
  sequencer_add_apply_overlap(C, op, seq);
  BKE_sequence_invalidate_cache_composite(scene, seq);

  DEG_id_tag_update(&scene->id, ID_RECALC_SEQUENCER_STRIPS);
  DEG_relations_tag_update(bmain);
  WM_event_add_notifier(C, NC_SCENE | ND_SEQUENCER, scene);

  return OPERATOR_FINISHED;
}

static int sequencer_add_scene_strip_invoke(bContext *C, wmOperator *op, const wmEvent *event)
{
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
  Editing *ed = BKE_sequencer_editing_get(scene, true);
  MovieClip *clip;
  Sequence *seq;

  int start_frame, channel;
  start_frame = RNA_int_get(op->ptr, "frame_start");
  channel = RNA_int_get(op->ptr, "channel");
  clip = BLI_findlink(&bmain->movieclips, RNA_enum_get(op->ptr, "clip"));

  if (clip == NULL) {
    BKE_report(op->reports, RPT_ERROR, "Movie clip not found");
    return OPERATOR_CANCELLED;
  }

  seq = BKE_sequence_alloc(ed->seqbasep, start_frame, channel, SEQ_TYPE_MOVIECLIP);
  seq->blend_mode = SEQ_TYPE_CROSS;
  seq->clip = clip;
  seq->len = BKE_movieclip_get_duration(clip);

  id_us_ensure_real(&seq->clip->id);

  BLI_strncpy(seq->name + 2, clip->id.name + 2, sizeof(seq->name) - 2);
  BKE_sequence_base_unique_name_recursive(&ed->seqbase, seq);

  BKE_sequence_calc_disp(scene, seq);
  BKE_sequencer_sort(scene);

  sequencer_add_apply_replace_sel(C, op, seq);
  sequencer_add_apply_overlap(C, op, seq);
  BKE_sequence_invalidate_cache_composite(scene, seq);

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
  Editing *ed = BKE_sequencer_editing_get(scene, true);
  Mask *mask;
  Sequence *seq;

  int start_frame, channel;
  start_frame = RNA_int_get(op->ptr, "frame_start");
  channel = RNA_int_get(op->ptr, "channel");
  mask = BLI_findlink(&bmain->masks, RNA_enum_get(op->ptr, "mask"));

  if (mask == NULL) {
    BKE_report(op->reports, RPT_ERROR, "Mask not found");
    return OPERATOR_CANCELLED;
  }

  seq = BKE_sequence_alloc(ed->seqbasep, start_frame, channel, SEQ_TYPE_MASK);
  seq->blend_mode = SEQ_TYPE_CROSS;
  seq->mask = mask;
  seq->len = BKE_mask_get_duration(mask);

  id_us_ensure_real(&seq->mask->id);

  BLI_strncpy(seq->name + 2, mask->id.name + 2, sizeof(seq->name) - 2);
  BKE_sequence_base_unique_name_recursive(&ed->seqbase, seq);

  BKE_sequence_calc_disp(scene, seq);
  BKE_sequencer_sort(scene);

  sequencer_add_apply_replace_sel(C, op, seq);
  sequencer_add_apply_overlap(C, op, seq);
  BKE_sequence_invalidate_cache_composite(scene, seq);

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

static int sequencer_add_generic_strip_exec(bContext *C, wmOperator *op, SeqLoadFn seq_load_fn)
{
  Scene *scene = CTX_data_scene(C);
  Editing *ed = BKE_sequencer_editing_get(scene, true);
  SeqLoadInfo seq_load;
  int tot_files;

  seq_load_operator_info(&seq_load, C, op);

  if (seq_load.flag & SEQ_LOAD_REPLACE_SEL) {
    ED_sequencer_deselect_all(scene);
  }

  tot_files = RNA_property_collection_length(op->ptr, RNA_struct_find_property(op->ptr, "files"));

  if (tot_files > 1) {
    char dir_only[FILE_MAX];
    char file_only[FILE_MAX];

    RNA_BEGIN (op->ptr, itemptr, "files") {
      Sequence *seq;

      RNA_string_get(op->ptr, "directory", dir_only);
      RNA_string_get(&itemptr, "name", file_only);
      BLI_join_dirfile(seq_load.path, sizeof(seq_load.path), dir_only, file_only);

      /* Set seq_load.name, otherwise all video/audio files get the same name. */
      BLI_strncpy(seq_load.name, file_only, sizeof(seq_load.name));

      seq = seq_load_fn(C, ed->seqbasep, &seq_load);
      if (seq) {
        sequencer_add_apply_overlap(C, op, seq);
        if (seq_load.seq_sound) {
          sequencer_add_apply_overlap(C, op, seq_load.seq_sound);
        }
      }
    }
    RNA_END;
  }
  else { /* Single file./ */
    Sequence *seq;
    seq = seq_load_fn(C, ed->seqbasep, &seq_load);

    if (seq) {
      sequencer_add_apply_overlap(C, op, seq);
      if (seq_load.seq_sound) {
        sequencer_add_apply_overlap(C, op, seq_load.seq_sound);
      }
    }
  }

  if (op->customdata) {
    MEM_freeN(op->customdata);
  }

  if (seq_load.tot_success == 0) {
    BKE_reportf(op->reports, RPT_ERROR, "File '%s' could not be loaded", seq_load.path);

    return OPERATOR_CANCELLED;
  }

  BKE_sequencer_sort(scene);

  DEG_id_tag_update(&scene->id, ID_RECALC_SEQUENCER_STRIPS);
  WM_event_add_notifier(C, NC_SCENE | ND_SEQUENCER, scene);

  return OPERATOR_FINISHED;
}

static void sequencer_add_init(bContext *UNUSED(C), wmOperator *op)
{
  op->customdata = MEM_callocN(sizeof(SequencerAddData), __func__);
}

static void sequencer_add_cancel(bContext *UNUSED(C), wmOperator *op)
{
  if (op->customdata) {
    MEM_freeN(op->customdata);
  }
  op->customdata = NULL;
}

static bool sequencer_add_draw_check_fn(PointerRNA *UNUSED(ptr),
                                        PropertyRNA *prop,
                                        void *UNUSED(user_data))
{
  const char *prop_id = RNA_property_identifier(prop);

  return !(STREQ(prop_id, "filepath") || STREQ(prop_id, "directory") ||
           STREQ(prop_id, "filename"));
}

static int sequencer_add_movie_strip_exec(bContext *C, wmOperator *op)
{
  return sequencer_add_generic_strip_exec(C, op, BKE_sequencer_add_movie_strip);
}

static int sequencer_add_movie_strip_invoke(bContext *C,
                                            wmOperator *op,
                                            const wmEvent *UNUSED(event))
{
  PropertyRNA *prop;
  Scene *scene = CTX_data_scene(C);
  Editing *ed = BKE_sequencer_editing_get(scene, false);

  /* Only enable "use_framerate" if there aren't any existing strips, unless overridden by user. */
  if (ed && ed->seqbasep && ed->seqbasep->first) {
    RNA_boolean_set(op->ptr, "use_framerate", false);
  }

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
  PointerRNA imf_ptr, ptr;

  /* Main draw call. */
  RNA_pointer_create(NULL, op->type->srna, op->properties, &ptr);
  uiDefAutoButsRNA(
      layout, &ptr, sequencer_add_draw_check_fn, NULL, NULL, UI_BUT_LABEL_ALIGN_NONE, false);

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
                                 FILE_SORT_ALPHA);
  sequencer_generic_props__internal(ot, SEQPROP_STARTFRAME);
  RNA_def_boolean(ot->srna, "sound", true, "Sound", "Load sound with the movie");
  RNA_def_boolean(ot->srna,
                  "use_framerate",
                  true,
                  "Use Movie Framerate",
                  "Use framerate from the movie to keep sound and video in sync");
}

static int sequencer_add_sound_strip_exec(bContext *C, wmOperator *op)
{
  return sequencer_add_generic_strip_exec(C, op, BKE_sequencer_add_sound_strip);
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
                                 FILE_SORT_ALPHA);
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
    filename = RNA_string_get_alloc(&itemptr, "name", NULL, 0);

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
  int i;
  char *filename = NULL;
  RNA_BEGIN (op->ptr, itemptr, "files") {
    filename = RNA_string_get_alloc(&itemptr, "name", NULL, 0);
    break;
  }
  RNA_END;

  if (filename) {
    char ext[PATH_MAX];
    char filename_stripped[PATH_MAX];
    /* Strip the frame from filename and substitute with `#`. */
    BLI_path_frame_strip(filename, ext);

    for (i = 0; i < len; i++, se++) {
      BLI_strncpy(filename_stripped, filename, sizeof(filename_stripped));
      BLI_path_frame(filename_stripped, minframe + i, numdigits);
      BLI_snprintf(se->name, sizeof(se->name), "%s%s", filename_stripped, ext);
    }

    MEM_freeN(filename);
  }
}

static int sequencer_add_image_strip_exec(bContext *C, wmOperator *op)
{
  int minframe, numdigits;
  Scene *scene = CTX_data_scene(C);
  Editing *ed = BKE_sequencer_editing_get(scene, true);
  SeqLoadInfo seq_load;
  Sequence *seq;
  Strip *strip;
  StripElem *se;
  const bool use_placeholders = RNA_boolean_get(op->ptr, "use_placeholders");

  seq_load_operator_info(&seq_load, C, op);

  /* Images are unique in how they handle this - 1 per strip elem. */
  if (use_placeholders) {
    seq_load.len = sequencer_image_seq_get_minmax_frame(
        op, seq_load.start_frame, &minframe, &numdigits);
  }
  else {
    seq_load.len = RNA_property_collection_length(op->ptr,
                                                  RNA_struct_find_property(op->ptr, "files"));
  }

  if (seq_load.len == 0) {
    return OPERATOR_CANCELLED;
  }

  if (seq_load.flag & SEQ_LOAD_REPLACE_SEL) {
    ED_sequencer_deselect_all(scene);
  }

  /* Main adding function. */
  seq = BKE_sequencer_add_image_strip(C, ed->seqbasep, &seq_load);
  strip = seq->strip;
  se = strip->stripdata;
  seq->blend_mode = SEQ_TYPE_ALPHAOVER;

  if (use_placeholders) {
    sequencer_image_seq_reserve_frames(op, se, seq_load.len, minframe, numdigits);
  }
  else {
    RNA_BEGIN (op->ptr, itemptr, "files") {
      char *filename = RNA_string_get_alloc(&itemptr, "name", NULL, 0);
      BLI_strncpy(se->name, filename, sizeof(se->name));
      MEM_freeN(filename);
      se++;
    }
    RNA_END;
  }

  if (seq_load.len == 1) {
    if (seq_load.start_frame < seq_load.end_frame) {
      seq->endstill = seq_load.end_frame - seq_load.start_frame;
    }
  }

  BKE_sequence_init_colorspace(seq);
  BKE_sequence_calc_disp(scene, seq);
  BKE_sequencer_sort(scene);

  /* Last active name. */
  BLI_strncpy(ed->act_imagedir, strip->dir, sizeof(ed->act_imagedir));
  sequencer_add_apply_overlap(C, op, seq);

  if (op->customdata) {
    MEM_freeN(op->customdata);
  }

  BKE_sequence_invalidate_cache_composite(scene, seq);
  DEG_id_tag_update(&scene->id, ID_RECALC_SEQUENCER_STRIPS);
  WM_event_add_notifier(C, NC_SCENE | ND_SEQUENCER, scene);

  return OPERATOR_FINISHED;
}

static int sequencer_add_image_strip_invoke(bContext *C,
                                            wmOperator *op,
                                            const wmEvent *UNUSED(event))
{
  PropertyRNA *prop;
  Scene *scene = CTX_data_scene(C);

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
                                 FILE_SORT_ALPHA);
  sequencer_generic_props__internal(ot, SEQPROP_STARTFRAME | SEQPROP_ENDFRAME);

  RNA_def_boolean(ot->srna,
                  "use_placeholders",
                  false,
                  "Use Placeholders",
                  "Use placeholders for missing frames of the strip");
}

static int sequencer_add_effect_strip_exec(bContext *C, wmOperator *op)
{
  Scene *scene = CTX_data_scene(C);
  Editing *ed = BKE_sequencer_editing_get(scene, true);
  Sequence *seq;
  struct SeqEffectHandle sh;
  Sequence *seq1, *seq2, *seq3;
  const char *error_msg;
  int start_frame, end_frame, channel, type;

  start_frame = RNA_int_get(op->ptr, "frame_start");
  end_frame = RNA_int_get(op->ptr, "frame_end");
  channel = RNA_int_get(op->ptr, "channel");
  type = RNA_enum_get(op->ptr, "type");

  if (!seq_effect_find_selected(scene, NULL, type, &seq1, &seq2, &seq3, &error_msg)) {
    BKE_report(op->reports, RPT_ERROR, error_msg);
    return OPERATOR_CANCELLED;
  }

  /* Check its start and end frames are valid. */
  if (seq1 == NULL && end_frame <= start_frame) {
    end_frame = start_frame + 1;
    RNA_int_set(op->ptr, "frame_end", end_frame);
  }

  seq = BKE_sequence_alloc(ed->seqbasep, start_frame, channel, type);
  BLI_strncpy(seq->name + 2, BKE_sequence_give_name(seq), sizeof(seq->name) - 2);
  BKE_sequence_base_unique_name_recursive(&ed->seqbase, seq);

  sh = BKE_sequence_get_effect(seq);
  sh.init(seq);
  seq->seq1 = seq1;
  seq->seq2 = seq2;
  seq->seq3 = seq3;

  if (!seq1) {
    seq->len = 1; /* Effect is generator, set non zero length. */
    BKE_sequence_tx_set_final_right(seq, end_frame);
  }

  seq->flag |= SEQ_USE_EFFECT_DEFAULT_FADE;
  BKE_sequence_calc(scene, seq);

  if (seq->type == SEQ_TYPE_COLOR) {
    SolidColorVars *colvars = (SolidColorVars *)seq->effectdata;
    RNA_float_get_array(op->ptr, "color", colvars->col);
    seq->blend_mode = SEQ_TYPE_CROSS;
  }
  else if (seq->type == SEQ_TYPE_ADJUSTMENT) {
    seq->blend_mode = SEQ_TYPE_CROSS;
  }
  else if (seq->type == SEQ_TYPE_TEXT) {
    seq->blend_mode = SEQ_TYPE_ALPHAOVER;
  }

  /* Set channel. If unset, use lowest free one above strips. */
  if (!RNA_struct_property_is_set(op->ptr, "channel")) {
    if (seq->seq1) {
      int chan = max_iii(seq->seq1 ? seq->seq1->machine : 0,
                         seq->seq2 ? seq->seq2->machine : 0,
                         seq->seq3 ? seq->seq3->machine : 0);
      if (chan < MAXSEQ) {
        seq->machine = chan;
      }
    }
  }

  sequencer_add_apply_replace_sel(C, op, seq);
  sequencer_add_apply_overlap(C, op, seq);

  BKE_sequencer_update_changed_seq_and_deps(scene, seq, 1, 1); /* Runs BKE_sequence_calc. */
  BKE_sequencer_sort(scene);

  BKE_sequence_invalidate_cache_composite(scene, seq);
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
    if (BKE_sequence_effect_get_num_inputs(type) != 0) {
      prop_flag |= SEQPROP_NOCHAN;
    }
  }

  sequencer_generic_invoke_xy__internal(C, op, prop_flag, type);

  return sequencer_add_effect_strip_exec(C, op);
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

  /* Flags. */
  ot->flag = OPTYPE_REGISTER | OPTYPE_UNDO;

  RNA_def_enum(ot->srna,
               "type",
               sequencer_prop_effect_types,
               SEQ_TYPE_CROSS,
               "Type",
               "Sequencer effect type");
  sequencer_generic_props__internal(ot, SEQPROP_STARTFRAME | SEQPROP_ENDFRAME);
  prop = RNA_def_float_color(ot->srna,
                             "color",
                             3,
                             NULL,
                             0.0f,
                             1.0f,
                             "Color",
                             "Initialize the strip with this color (only used when type='COLOR')",
                             0.0f,
                             1.0f);
  RNA_def_property_subtype(prop, PROP_COLOR_GAMMA);
}
