/* SPDX-FileCopyrightText: 2023 Blender Authors
 *
 * SPDX-License-Identifier: GPL-2.0-or-later */

/** \file
 * \ingroup RNA
 */

#include <cstdio>
#include <cstdlib>
#include <cstring>

#include "DNA_scene_types.h"
#include "DNA_sequence_types.h"

#include "BLI_utildefines.h"

#include "RNA_access.hh"
#include "RNA_define.hh"

#include "SEQ_edit.hh"

#include "rna_internal.hh"

#ifdef RNA_RUNTIME

// #include "DNA_anim_types.h"
#  include "DNA_image_types.h"
#  include "DNA_mask_types.h"
#  include "DNA_sound_types.h"

#  include "BLI_path_util.h" /* #BLI_path_split_dir_file */

#  include "BKE_image.h"
#  include "BKE_mask.h"
#  include "BKE_movieclip.h"

#  include "BKE_report.h"
#  include "BKE_sound.h"

#  include "IMB_imbuf.hh"
#  include "IMB_imbuf_types.hh"

#  include "SEQ_add.hh"
#  include "SEQ_edit.hh"
#  include "SEQ_effects.hh"
#  include "SEQ_relations.hh"
#  include "SEQ_render.hh"
#  include "SEQ_retiming.hh"
#  include "SEQ_sequencer.hh"
#  include "SEQ_time.hh"

#  include "WM_api.hh"

static StripElem *rna_Sequence_strip_elem_from_frame(ID *id, Sequence *self, int timeline_frame)
{
  Scene *scene = (Scene *)id;
  return SEQ_render_give_stripelem(scene, self, timeline_frame);
}

static void rna_Sequence_swap_internal(ID *id,
                                       Sequence *seq_self,
                                       ReportList *reports,
                                       Sequence *seq_other)
{
  const char *error_msg;
  Scene *scene = (Scene *)id;

  if (SEQ_edit_sequence_swap(scene, seq_self, seq_other, &error_msg) == false) {
    BKE_report(reports, RPT_ERROR, error_msg);
  }
}

static void rna_Sequences_move_strip_to_meta(
    ID *id, Sequence *seq_self, Main *bmain, ReportList *reports, Sequence *meta_dst)
{
  Scene *scene = (Scene *)id;
  const char *error_msg;

  /* Move strip to meta. */
  if (!SEQ_edit_move_strip_to_meta(scene, seq_self, meta_dst, &error_msg)) {
    BKE_report(reports, RPT_ERROR, error_msg);
  }

  /* Update depsgraph. */
  DEG_relations_tag_update(bmain);
  DEG_id_tag_update(&scene->id, ID_RECALC_SEQUENCER_STRIPS);

  SEQ_sequence_lookup_tag(scene, SEQ_LOOKUP_TAG_INVALID);

  WM_main_add_notifier(NC_SCENE | ND_SEQUENCER, scene);
}

static Sequence *rna_Sequence_split(
    ID *id, Sequence *seq, Main *bmain, ReportList *reports, int frame, int split_method)
{
  Scene *scene = (Scene *)id;
  ListBase *seqbase = SEQ_get_seqbase_by_seq(scene, seq);

  const char *error_msg = nullptr;
  Sequence *r_seq = SEQ_edit_strip_split(
      bmain, scene, seqbase, seq, frame, eSeqSplitMethod(split_method), &error_msg);
  if (error_msg != nullptr) {
    BKE_report(reports, RPT_ERROR, error_msg);
  }

  /* Update depsgraph. */
  DEG_relations_tag_update(bmain);
  DEG_id_tag_update(&scene->id, ID_RECALC_SEQUENCER_STRIPS);

  WM_main_add_notifier(NC_SCENE | ND_SEQUENCER, scene);

  return r_seq;
}

static Sequence *rna_Sequence_parent_meta(ID *id, Sequence *seq_self)
{
  Scene *scene = (Scene *)id;
  Editing *ed = SEQ_editing_get(scene);

  return SEQ_find_metastrip_by_sequence(&ed->seqbase, nullptr, seq_self);
}

static Sequence *rna_Sequences_new_clip(ID *id,
                                        ListBase *seqbase,
                                        Main *bmain,
                                        const char *name,
                                        MovieClip *clip,
                                        int channel,
                                        int frame_start)
{
  Scene *scene = (Scene *)id;
  SeqLoadData load_data;
  SEQ_add_load_data_init(&load_data, name, nullptr, frame_start, channel);
  load_data.clip = clip;
  Sequence *seq = SEQ_add_movieclip_strip(scene, seqbase, &load_data);

  DEG_relations_tag_update(bmain);
  DEG_id_tag_update(&scene->id, ID_RECALC_SEQUENCER_STRIPS);
  WM_main_add_notifier(NC_SCENE | ND_SEQUENCER, scene);

  return seq;
}

static Sequence *rna_Sequences_editing_new_clip(ID *id,
                                                Editing *ed,
                                                Main *bmain,
                                                const char *name,
                                                MovieClip *clip,
                                                int channel,
                                                int frame_start)
{
  return rna_Sequences_new_clip(id, &ed->seqbase, bmain, name, clip, channel, frame_start);
}

static Sequence *rna_Sequences_meta_new_clip(ID *id,
                                             Sequence *seq,
                                             Main *bmain,
                                             const char *name,
                                             MovieClip *clip,
                                             int channel,
                                             int frame_start)
{
  return rna_Sequences_new_clip(id, &seq->seqbase, bmain, name, clip, channel, frame_start);
}

static Sequence *rna_Sequences_new_mask(ID *id,
                                        ListBase *seqbase,
                                        Main *bmain,
                                        const char *name,
                                        Mask *mask,
                                        int channel,
                                        int frame_start)
{
  Scene *scene = (Scene *)id;
  SeqLoadData load_data;
  SEQ_add_load_data_init(&load_data, name, nullptr, frame_start, channel);
  load_data.mask = mask;
  Sequence *seq = SEQ_add_mask_strip(scene, seqbase, &load_data);

  DEG_relations_tag_update(bmain);
  DEG_id_tag_update(&scene->id, ID_RECALC_SEQUENCER_STRIPS);
  WM_main_add_notifier(NC_SCENE | ND_SEQUENCER, scene);

  return seq;
}
static Sequence *rna_Sequences_editing_new_mask(
    ID *id, Editing *ed, Main *bmain, const char *name, Mask *mask, int channel, int frame_start)
{
  return rna_Sequences_new_mask(id, &ed->seqbase, bmain, name, mask, channel, frame_start);
}

static Sequence *rna_Sequences_meta_new_mask(
    ID *id, Sequence *seq, Main *bmain, const char *name, Mask *mask, int channel, int frame_start)
{
  return rna_Sequences_new_mask(id, &seq->seqbase, bmain, name, mask, channel, frame_start);
}

static Sequence *rna_Sequences_new_scene(ID *id,
                                         ListBase *seqbase,
                                         Main *bmain,
                                         const char *name,
                                         Scene *sce_seq,
                                         int channel,
                                         int frame_start)
{
  Scene *scene = (Scene *)id;
  SeqLoadData load_data;
  SEQ_add_load_data_init(&load_data, name, nullptr, frame_start, channel);
  load_data.scene = sce_seq;
  Sequence *seq = SEQ_add_scene_strip(scene, seqbase, &load_data);

  DEG_relations_tag_update(bmain);
  DEG_id_tag_update(&scene->id, ID_RECALC_SEQUENCER_STRIPS);
  WM_main_add_notifier(NC_SCENE | ND_SEQUENCER, scene);

  return seq;
}

static Sequence *rna_Sequences_editing_new_scene(ID *id,
                                                 Editing *ed,
                                                 Main *bmain,
                                                 const char *name,
                                                 Scene *sce_seq,
                                                 int channel,
                                                 int frame_start)
{
  return rna_Sequences_new_scene(id, &ed->seqbase, bmain, name, sce_seq, channel, frame_start);
}

static Sequence *rna_Sequences_meta_new_scene(ID *id,
                                              Sequence *seq,
                                              Main *bmain,
                                              const char *name,
                                              Scene *sce_seq,
                                              int channel,
                                              int frame_start)
{
  return rna_Sequences_new_scene(id, &seq->seqbase, bmain, name, sce_seq, channel, frame_start);
}

static Sequence *rna_Sequences_new_image(ID *id,
                                         ListBase *seqbase,
                                         Main *bmain,
                                         ReportList * /*reports*/,
                                         const char *name,
                                         const char *file,
                                         int channel,
                                         int frame_start,
                                         int fit_method)
{
  Scene *scene = (Scene *)id;

  SeqLoadData load_data;
  SEQ_add_load_data_init(&load_data, name, file, frame_start, channel);
  load_data.image.len = 1;
  load_data.fit_method = eSeqImageFitMethod(fit_method);
  Sequence *seq = SEQ_add_image_strip(bmain, scene, seqbase, &load_data);

  char dirpath[FILE_MAX], filename[FILE_MAXFILE];
  BLI_path_split_dir_file(file, dirpath, sizeof(dirpath), filename, sizeof(filename));
  SEQ_add_image_set_directory(seq, dirpath);
  SEQ_add_image_load_file(scene, seq, 0, filename);
  SEQ_add_image_init_alpha_mode(seq);

  DEG_relations_tag_update(bmain);
  DEG_id_tag_update(&scene->id, ID_RECALC_SEQUENCER_STRIPS);
  WM_main_add_notifier(NC_SCENE | ND_SEQUENCER, scene);

  return seq;
}

static Sequence *rna_Sequences_editing_new_image(ID *id,
                                                 Editing *ed,
                                                 Main *bmain,
                                                 ReportList *reports,
                                                 const char *name,
                                                 const char *file,
                                                 int channel,
                                                 int frame_start,
                                                 int fit_method)
{
  return rna_Sequences_new_image(
      id, &ed->seqbase, bmain, reports, name, file, channel, frame_start, fit_method);
}

static Sequence *rna_Sequences_meta_new_image(ID *id,
                                              Sequence *seq,
                                              Main *bmain,
                                              ReportList *reports,
                                              const char *name,
                                              const char *file,
                                              int channel,
                                              int frame_start,
                                              int fit_method)
{
  return rna_Sequences_new_image(
      id, &seq->seqbase, bmain, reports, name, file, channel, frame_start, fit_method);
}

static Sequence *rna_Sequences_new_movie(ID *id,
                                         ListBase *seqbase,
                                         Main *bmain,
                                         const char *name,
                                         const char *file,
                                         int channel,
                                         int frame_start,
                                         int fit_method)
{
  Scene *scene = (Scene *)id;
  SeqLoadData load_data;
  SEQ_add_load_data_init(&load_data, name, file, frame_start, channel);
  load_data.fit_method = eSeqImageFitMethod(fit_method);
  load_data.allow_invalid_file = true;
  Sequence *seq = SEQ_add_movie_strip(bmain, scene, seqbase, &load_data);

  DEG_relations_tag_update(bmain);
  DEG_id_tag_update(&scene->id, ID_RECALC_SEQUENCER_STRIPS);
  WM_main_add_notifier(NC_SCENE | ND_SEQUENCER, scene);

  return seq;
}

static Sequence *rna_Sequences_editing_new_movie(ID *id,
                                                 Editing *ed,
                                                 Main *bmain,
                                                 const char *name,
                                                 const char *file,
                                                 int channel,
                                                 int frame_start,
                                                 int fit_method)
{
  return rna_Sequences_new_movie(
      id, &ed->seqbase, bmain, name, file, channel, frame_start, fit_method);
}

static Sequence *rna_Sequences_meta_new_movie(ID *id,
                                              Sequence *seq,
                                              Main *bmain,
                                              const char *name,
                                              const char *file,
                                              int channel,
                                              int frame_start,
                                              int fit_method)
{
  return rna_Sequences_new_movie(
      id, &seq->seqbase, bmain, name, file, channel, frame_start, fit_method);
}

#  ifdef WITH_AUDASPACE
static Sequence *rna_Sequences_new_sound(ID *id,
                                         ListBase *seqbase,
                                         Main *bmain,
                                         ReportList *reports,
                                         const char *name,
                                         const char *file,
                                         int channel,
                                         int frame_start)
{
  Scene *scene = (Scene *)id;
  SeqLoadData load_data;
  SEQ_add_load_data_init(&load_data, name, file, frame_start, channel);
  load_data.allow_invalid_file = true;
  Sequence *seq = SEQ_add_sound_strip(bmain, scene, seqbase, &load_data);

  if (seq == nullptr) {
    BKE_report(reports, RPT_ERROR, "Sequences.new_sound: unable to open sound file");
    return nullptr;
  }

  DEG_relations_tag_update(bmain);
  DEG_id_tag_update(&scene->id, ID_RECALC_SEQUENCER_STRIPS);
  WM_main_add_notifier(NC_SCENE | ND_SEQUENCER, scene);

  return seq;
}
#  else  /* WITH_AUDASPACE */
static Sequence *rna_Sequences_new_sound(ID * /*id*/,
                                         ListBase * /*seqbase*/,
                                         Main * /*bmain*/,
                                         ReportList *reports,
                                         const char * /*name*/,
                                         const char * /*file*/,
                                         int /*channel*/,
                                         int /*frame_start*/)
{
  BKE_report(reports, RPT_ERROR, "Blender compiled without Audaspace support");
  return nullptr;
}
#  endif /* WITH_AUDASPACE */

static Sequence *rna_Sequences_editing_new_sound(ID *id,
                                                 Editing *ed,
                                                 Main *bmain,
                                                 ReportList *reports,
                                                 const char *name,
                                                 const char *file,
                                                 int channel,
                                                 int frame_start)
{
  return rna_Sequences_new_sound(
      id, &ed->seqbase, bmain, reports, name, file, channel, frame_start);
}

static Sequence *rna_Sequences_meta_new_sound(ID *id,
                                              Sequence *seq,
                                              Main *bmain,
                                              ReportList *reports,
                                              const char *name,
                                              const char *file,
                                              int channel,
                                              int frame_start)
{
  return rna_Sequences_new_sound(
      id, &seq->seqbase, bmain, reports, name, file, channel, frame_start);
}

/* Meta sequence
 * Possibility to create an empty meta to avoid plenty of meta toggling
 * Created meta have a length equal to 1, must be set through the API. */
static Sequence *rna_Sequences_new_meta(
    ID *id, ListBase *seqbase, const char *name, int channel, int frame_start)
{
  Scene *scene = (Scene *)id;
  SeqLoadData load_data;
  SEQ_add_load_data_init(&load_data, name, nullptr, frame_start, channel);
  Sequence *seqm = SEQ_add_meta_strip(scene, seqbase, &load_data);

  return seqm;
}

static Sequence *rna_Sequences_editing_new_meta(
    ID *id, Editing *ed, const char *name, int channel, int frame_start)
{
  return rna_Sequences_new_meta(id, &ed->seqbase, name, channel, frame_start);
}

static Sequence *rna_Sequences_meta_new_meta(
    ID *id, Sequence *seq, const char *name, int channel, int frame_start)
{
  return rna_Sequences_new_meta(id, &seq->seqbase, name, channel, frame_start);
}

static Sequence *rna_Sequences_new_effect(ID *id,
                                          ListBase *seqbase,
                                          ReportList *reports,
                                          const char *name,
                                          int type,
                                          int channel,
                                          int frame_start,
                                          int frame_end,
                                          Sequence *seq1,
                                          Sequence *seq2,
                                          Sequence *seq3)
{
  Scene *scene = (Scene *)id;
  Sequence *seq;
  const int num_inputs = SEQ_effect_get_num_inputs(type);

  switch (num_inputs) {
    case 0:
      if (frame_end <= frame_start) {
        BKE_report(reports, RPT_ERROR, "Sequences.new_effect: end frame not set");
        return nullptr;
      }
      break;
    case 1:
      if (seq1 == nullptr) {
        BKE_report(reports, RPT_ERROR, "Sequences.new_effect: effect takes 1 input sequence");
        return nullptr;
      }
      break;
    case 2:
      if (seq1 == nullptr || seq2 == nullptr) {
        BKE_report(reports, RPT_ERROR, "Sequences.new_effect: effect takes 2 input sequences");
        return nullptr;
      }
      break;
    case 3:
      if (seq1 == nullptr || seq2 == nullptr || seq3 == nullptr) {
        BKE_report(reports, RPT_ERROR, "Sequences.new_effect: effect takes 3 input sequences");
        return nullptr;
      }
      break;
    default:
      BKE_reportf(
          reports,
          RPT_ERROR,
          "Sequences.new_effect: effect expects more than 3 inputs (%d, should never happen!)",
          num_inputs);
      return nullptr;
  }

  SeqLoadData load_data;
  SEQ_add_load_data_init(&load_data, name, nullptr, frame_start, channel);
  load_data.effect.end_frame = frame_end;
  load_data.effect.type = type;
  load_data.effect.seq1 = seq1;
  load_data.effect.seq2 = seq2;
  load_data.effect.seq3 = seq3;
  seq = SEQ_add_effect_strip(scene, seqbase, &load_data);

  DEG_id_tag_update(&scene->id, ID_RECALC_SEQUENCER_STRIPS);
  WM_main_add_notifier(NC_SCENE | ND_SEQUENCER, scene);

  return seq;
}

static Sequence *rna_Sequences_editing_new_effect(ID *id,
                                                  Editing *ed,
                                                  ReportList *reports,
                                                  const char *name,
                                                  int type,
                                                  int channel,
                                                  int frame_start,
                                                  int frame_end,
                                                  Sequence *seq1,
                                                  Sequence *seq2,
                                                  Sequence *seq3)
{
  return rna_Sequences_new_effect(
      id, &ed->seqbase, reports, name, type, channel, frame_start, frame_end, seq1, seq2, seq3);
}

static Sequence *rna_Sequences_meta_new_effect(ID *id,
                                               Sequence *seq,
                                               ReportList *reports,
                                               const char *name,
                                               int type,
                                               int channel,
                                               int frame_start,
                                               int frame_end,
                                               Sequence *seq1,
                                               Sequence *seq2,
                                               Sequence *seq3)
{
  return rna_Sequences_new_effect(
      id, &seq->seqbase, reports, name, type, channel, frame_start, frame_end, seq1, seq2, seq3);
}

static void rna_Sequences_remove(
    ID *id, ListBase *seqbase, Main *bmain, ReportList *reports, PointerRNA *seq_ptr)
{
  Sequence *seq = static_cast<Sequence *>(seq_ptr->data);
  Scene *scene = (Scene *)id;

  if (BLI_findindex(seqbase, seq) == -1) {
    BKE_reportf(
        reports, RPT_ERROR, "Sequence '%s' not in scene '%s'", seq->name + 2, scene->id.name + 2);
    return;
  }

  SEQ_edit_flag_for_removal(scene, seqbase, seq);
  SEQ_edit_remove_flagged_sequences(scene, seqbase);
  RNA_POINTER_INVALIDATE(seq_ptr);

  DEG_relations_tag_update(bmain);
  DEG_id_tag_update(&scene->id, ID_RECALC_SEQUENCER_STRIPS);
  WM_main_add_notifier(NC_SCENE | ND_SEQUENCER, scene);
}

static void rna_Sequences_editing_remove(
    ID *id, Editing *ed, Main *bmain, ReportList *reports, PointerRNA *seq_ptr)
{
  rna_Sequences_remove(id, &ed->seqbase, bmain, reports, seq_ptr);
}

static void rna_Sequences_meta_remove(
    ID *id, Sequence *seq, Main *bmain, ReportList *reports, PointerRNA *seq_ptr)
{
  rna_Sequences_remove(id, &seq->seqbase, bmain, reports, seq_ptr);
}

static StripElem *rna_SequenceElements_append(ID *id, Sequence *seq, const char *filename)
{
  Scene *scene = (Scene *)id;
  StripElem *se;

  seq->strip->stripdata = se = static_cast<StripElem *>(
      MEM_reallocN(seq->strip->stripdata, sizeof(StripElem) * (seq->len + 1)));
  se += seq->len;
  STRNCPY(se->filename, filename);
  seq->len++;

  seq->flag &= ~SEQ_SINGLE_FRAME_CONTENT;

  WM_main_add_notifier(NC_SCENE | ND_SEQUENCER, scene);

  return se;
}

static void rna_SequenceElements_pop(ID *id, Sequence *seq, ReportList *reports, int index)
{
  Scene *scene = (Scene *)id;
  StripElem *new_seq, *se;

  if (seq->len == 1) {
    BKE_report(reports, RPT_ERROR, "SequenceElements.pop: cannot pop the last element");
    return;
  }

  /* python style negative indexing */
  if (index < 0) {
    index += seq->len;
  }

  if (seq->len <= index || index < 0) {
    BKE_report(reports, RPT_ERROR, "SequenceElements.pop: index out of range");
    return;
  }

  new_seq = static_cast<StripElem *>(
      MEM_callocN(sizeof(StripElem) * (seq->len - 1), "SequenceElements_pop"));
  seq->len--;

  if (seq->len == 1) {
    seq->flag |= SEQ_SINGLE_FRAME_CONTENT;
  }

  se = seq->strip->stripdata;
  if (index > 0) {
    memcpy(new_seq, se, sizeof(StripElem) * index);
  }

  if (index < seq->len) {
    memcpy(&new_seq[index], &se[index + 1], sizeof(StripElem) * (seq->len - index));
  }

  MEM_freeN(seq->strip->stripdata);
  seq->strip->stripdata = new_seq;

  WM_main_add_notifier(NC_SCENE | ND_SEQUENCER, scene);
}

static void rna_Sequence_invalidate_cache_rnafunc(ID *id, Sequence *self, int type)
{
  switch (type) {
    case SEQ_CACHE_STORE_RAW:
      SEQ_relations_invalidate_cache_raw((Scene *)id, self);
      break;
    case SEQ_CACHE_STORE_PREPROCESSED:
      SEQ_relations_invalidate_cache_preprocessed((Scene *)id, self);
      break;
    case SEQ_CACHE_STORE_COMPOSITE:
      SEQ_relations_invalidate_cache_composite((Scene *)id, self);
      break;
  }
}

static SeqRetimingKey *rna_Sequence_retiming_keys_add(ID *id, Sequence *seq, int timeline_frame)
{
  Scene *scene = (Scene *)id;

  SeqRetimingKey *key = SEQ_retiming_add_key(scene, seq, timeline_frame);

  SEQ_relations_invalidate_cache_raw(scene, seq);
  WM_main_add_notifier(NC_SCENE | ND_SEQUENCER, nullptr);
  return key;
}

static void rna_Sequence_retiming_keys_reset(ID *id, Sequence *seq)
{
  Scene *scene = (Scene *)id;

  SEQ_retiming_data_clear(seq);

  SEQ_relations_invalidate_cache_raw(scene, seq);
  WM_main_add_notifier(NC_SCENE | ND_SEQUENCER, nullptr);
}

#else

void RNA_api_sequence_strip(StructRNA *srna)
{
  FunctionRNA *func;
  PropertyRNA *parm;

  static const EnumPropertyItem seq_cahce_type_items[] = {
      {SEQ_CACHE_STORE_RAW, "RAW", 0, "Raw", ""},
      {SEQ_CACHE_STORE_PREPROCESSED, "PREPROCESSED", 0, "Preprocessed", ""},
      {SEQ_CACHE_STORE_COMPOSITE, "COMPOSITE", 0, "Composite", ""},
      {0, nullptr, 0, nullptr, nullptr},
  };

  static const EnumPropertyItem seq_split_method_items[] = {
      {SEQ_SPLIT_SOFT, "SOFT", 0, "Soft", ""},
      {SEQ_SPLIT_HARD, "HARD", 0, "Hard", ""},
      {0, nullptr, 0, nullptr, nullptr},
  };

  func = RNA_def_function(srna, "strip_elem_from_frame", "rna_Sequence_strip_elem_from_frame");
  RNA_def_function_flag(func, FUNC_USE_SELF_ID);
  RNA_def_function_ui_description(func, "Return the strip element from a given frame or None");
  parm = RNA_def_int(func,
                     "frame",
                     0,
                     -MAXFRAME,
                     MAXFRAME,
                     "Frame",
                     "The frame to get the strip element from",
                     -MAXFRAME,
                     MAXFRAME);
  RNA_def_parameter_flags(parm, PropertyFlag(0), PARM_REQUIRED);
  RNA_def_function_return(
      func,
      RNA_def_pointer(func, "elem", "SequenceElement", "", "strip element of the current frame"));

  func = RNA_def_function(srna, "swap", "rna_Sequence_swap_internal");
  RNA_def_function_flag(func, FUNC_USE_REPORTS | FUNC_USE_SELF_ID);
  RNA_def_function_flag(func, FUNC_USE_REPORTS);
  parm = RNA_def_pointer(func, "other", "Sequence", "Other", "");
  RNA_def_parameter_flags(parm, PROP_NEVER_NULL, PARM_REQUIRED);

  func = RNA_def_function(srna, "move_to_meta", "rna_Sequences_move_strip_to_meta");
  RNA_def_function_flag(func, FUNC_USE_REPORTS | FUNC_USE_SELF_ID | FUNC_USE_MAIN);
  parm = RNA_def_pointer(func,
                         "meta_sequence",
                         "Sequence",
                         "Destination Meta Sequence",
                         "Meta to move the strip into");
  RNA_def_parameter_flags(parm, PROP_NEVER_NULL, PARM_REQUIRED);

  func = RNA_def_function(srna, "parent_meta", "rna_Sequence_parent_meta");
  RNA_def_function_flag(func, FUNC_USE_SELF_ID);
  RNA_def_function_ui_description(func, "Parent meta");
  /* return type */
  parm = RNA_def_pointer(func, "sequence", "Sequence", "", "Parent Meta");
  RNA_def_function_return(func, parm);

  func = RNA_def_function(srna, "invalidate_cache", "rna_Sequence_invalidate_cache_rnafunc");
  RNA_def_function_flag(func, FUNC_USE_SELF_ID);
  RNA_def_function_ui_description(func,
                                  "Invalidate cached images for strip and all dependent strips");
  parm = RNA_def_enum(func, "type", seq_cahce_type_items, 0, "Type", "Cache Type");
  RNA_def_parameter_flags(parm, PROP_NEVER_NULL, PARM_REQUIRED);

  func = RNA_def_function(srna, "split", "rna_Sequence_split");
  RNA_def_function_flag(func, FUNC_USE_REPORTS | FUNC_USE_SELF_ID | FUNC_USE_MAIN);
  RNA_def_function_ui_description(func, "Split Sequence");
  parm = RNA_def_int(
      func, "frame", 0, INT_MIN, INT_MAX, "", "Frame where to split the strip", INT_MIN, INT_MAX);
  RNA_def_parameter_flags(parm, PropertyFlag(0), PARM_REQUIRED);
  parm = RNA_def_enum(func, "split_method", seq_split_method_items, 0, "", "");
  RNA_def_parameter_flags(parm, PROP_NEVER_NULL, PARM_REQUIRED);
  /* Return type. */
  parm = RNA_def_pointer(func, "sequence", "Sequence", "", "Right side Sequence");
  RNA_def_function_return(func, parm);
}

void RNA_api_sequence_elements(BlenderRNA *brna, PropertyRNA *cprop)
{
  StructRNA *srna;
  PropertyRNA *parm;
  FunctionRNA *func;

  RNA_def_property_srna(cprop, "SequenceElements");
  srna = RNA_def_struct(brna, "SequenceElements", nullptr);
  RNA_def_struct_sdna(srna, "Sequence");
  RNA_def_struct_ui_text(srna, "SequenceElements", "Collection of SequenceElement");

  func = RNA_def_function(srna, "append", "rna_SequenceElements_append");
  RNA_def_function_flag(func, FUNC_USE_SELF_ID);
  RNA_def_function_ui_description(func, "Push an image from ImageSequence.directory");
  parm = RNA_def_string(func, "filename", "File", 0, "", "Filepath to image");
  RNA_def_parameter_flags(parm, PropertyFlag(0), PARM_REQUIRED);
  /* return type */
  parm = RNA_def_pointer(func, "elem", "SequenceElement", "", "New SequenceElement");
  RNA_def_function_return(func, parm);

  func = RNA_def_function(srna, "pop", "rna_SequenceElements_pop");
  RNA_def_function_flag(func, FUNC_USE_REPORTS | FUNC_USE_SELF_ID);
  RNA_def_function_ui_description(func, "Pop an image off the collection");
  parm = RNA_def_int(
      func, "index", -1, INT_MIN, INT_MAX, "", "Index of image to remove", INT_MIN, INT_MAX);
  RNA_def_parameter_flags(parm, PropertyFlag(0), PARM_REQUIRED);
}

void RNA_api_sequence_retiming_keys(BlenderRNA *brna, PropertyRNA *cprop)
{
  StructRNA *srna;

  RNA_def_property_srna(cprop, "RetimingKeys");
  srna = RNA_def_struct(brna, "RetimingKeys", nullptr);
  RNA_def_struct_sdna(srna, "Sequence");
  RNA_def_struct_ui_text(srna, "RetimingKeys", "Collection of RetimingKey");

  FunctionRNA *func = RNA_def_function(srna, "add", "rna_Sequence_retiming_keys_add");
  RNA_def_function_flag(func, FUNC_USE_SELF_ID);
  RNA_def_int(
      func, "timeline_frame", 0, -MAXFRAME, MAXFRAME, "Timeline Frame", "", -MAXFRAME, MAXFRAME);
  RNA_def_function_ui_description(func, "Add retiming key");
  /* return type */
  PropertyRNA *parm = RNA_def_pointer(func, "retiming_key", "RetimingKey", "", "New RetimingKey");
  RNA_def_function_return(func, parm);

  func = RNA_def_function(srna, "reset", "rna_Sequence_retiming_keys_reset");
  RNA_def_function_flag(func, FUNC_USE_SELF_ID);
  RNA_def_function_ui_description(func, "Remove all retiming keys");
}

void RNA_api_sequences(BlenderRNA *brna, PropertyRNA *cprop, const bool metastrip)
{
  StructRNA *srna;
  PropertyRNA *parm;
  FunctionRNA *func;

  static const EnumPropertyItem seq_effect_items[] = {
      {SEQ_TYPE_CROSS, "CROSS", 0, "Cross", ""},
      {SEQ_TYPE_ADD, "ADD", 0, "Add", ""},
      {SEQ_TYPE_SUB, "SUBTRACT", 0, "Subtract", ""},
      {SEQ_TYPE_ALPHAOVER, "ALPHA_OVER", 0, "Alpha Over", ""},
      {SEQ_TYPE_ALPHAUNDER, "ALPHA_UNDER", 0, "Alpha Under", ""},
      {SEQ_TYPE_GAMCROSS, "GAMMA_CROSS", 0, "Gamma Cross", ""},
      {SEQ_TYPE_MUL, "MULTIPLY", 0, "Multiply", ""},
      {SEQ_TYPE_OVERDROP, "OVER_DROP", 0, "Over Drop", ""},
      {SEQ_TYPE_WIPE, "WIPE", 0, "Wipe", ""},
      {SEQ_TYPE_GLOW, "GLOW", 0, "Glow", ""},
      {SEQ_TYPE_TRANSFORM, "TRANSFORM", 0, "Transform", ""},
      {SEQ_TYPE_COLOR, "COLOR", 0, "Color", ""},
      {SEQ_TYPE_SPEED, "SPEED", 0, "Speed", ""},
      {SEQ_TYPE_MULTICAM, "MULTICAM", 0, "Multicam Selector", ""},
      {SEQ_TYPE_ADJUSTMENT, "ADJUSTMENT", 0, "Adjustment Layer", ""},
      {SEQ_TYPE_GAUSSIAN_BLUR, "GAUSSIAN_BLUR", 0, "Gaussian Blur", ""},
      {SEQ_TYPE_TEXT, "TEXT", 0, "Text", ""},
      {SEQ_TYPE_COLORMIX, "COLORMIX", 0, "Color Mix", ""},
      {0, nullptr, 0, nullptr, nullptr},
  };

  static const EnumPropertyItem scale_fit_methods[] = {
      {SEQ_SCALE_TO_FIT, "FIT", 0, "Scale to Fit", "Scale image so fits in preview"},
      {SEQ_SCALE_TO_FILL,
       "FILL",
       0,
       "Scale to Fill",
       "Scale image so it fills preview completely"},
      {SEQ_STRETCH_TO_FILL, "STRETCH", 0, "Stretch to Fill", "Stretch image so it fills preview"},
      {SEQ_USE_ORIGINAL_SIZE, "ORIGINAL", 0, "Use Original Size", "Don't scale the image"},
      {0, nullptr, 0, nullptr, nullptr},
  };

  const char *new_clip_func_name = "rna_Sequences_editing_new_clip";
  const char *new_mask_func_name = "rna_Sequences_editing_new_mask";
  const char *new_scene_func_name = "rna_Sequences_editing_new_scene";
  const char *new_image_func_name = "rna_Sequences_editing_new_image";
  const char *new_movie_func_name = "rna_Sequences_editing_new_movie";
  const char *new_sound_func_name = "rna_Sequences_editing_new_sound";
  const char *new_meta_func_name = "rna_Sequences_editing_new_meta";
  const char *new_effect_func_name = "rna_Sequences_editing_new_effect";
  const char *remove_func_name = "rna_Sequences_editing_remove";

  if (metastrip) {
    RNA_def_property_srna(cprop, "SequencesMeta");
    srna = RNA_def_struct(brna, "SequencesMeta", nullptr);
    RNA_def_struct_sdna(srna, "Sequence");

    new_clip_func_name = "rna_Sequences_meta_new_clip";
    new_mask_func_name = "rna_Sequences_meta_new_mask";
    new_scene_func_name = "rna_Sequences_meta_new_scene";
    new_image_func_name = "rna_Sequences_meta_new_image";
    new_movie_func_name = "rna_Sequences_meta_new_movie";
    new_sound_func_name = "rna_Sequences_meta_new_sound";
    new_meta_func_name = "rna_Sequences_meta_new_meta";
    new_effect_func_name = "rna_Sequences_meta_new_effect";
    remove_func_name = "rna_Sequences_meta_remove";
  }
  else {
    RNA_def_property_srna(cprop, "SequencesTopLevel");
    srna = RNA_def_struct(brna, "SequencesTopLevel", nullptr);
    RNA_def_struct_sdna(srna, "Editing");
  }

  RNA_def_struct_ui_text(srna, "Sequences", "Collection of Sequences");

  func = RNA_def_function(srna, "new_clip", new_clip_func_name);
  RNA_def_function_flag(func, FUNC_USE_SELF_ID | FUNC_USE_MAIN);
  RNA_def_function_ui_description(func, "Add a new movie clip sequence");
  parm = RNA_def_string(func, "name", "Name", 0, "", "Name for the new sequence");
  RNA_def_parameter_flags(parm, PropertyFlag(0), PARM_REQUIRED);
  parm = RNA_def_pointer(func, "clip", "MovieClip", "", "Movie clip to add");
  RNA_def_parameter_flags(parm, PROP_NEVER_NULL, PARM_REQUIRED);
  parm = RNA_def_int(
      func, "channel", 0, 1, MAXSEQ, "Channel", "The channel for the new sequence", 1, MAXSEQ);
  RNA_def_parameter_flags(parm, PropertyFlag(0), PARM_REQUIRED);
  parm = RNA_def_int(func,
                     "frame_start",
                     0,
                     -MAXFRAME,
                     MAXFRAME,
                     "",
                     "The start frame for the new sequence",
                     -MAXFRAME,
                     MAXFRAME);
  RNA_def_parameter_flags(parm, PropertyFlag(0), PARM_REQUIRED);
  /* return type */
  parm = RNA_def_pointer(func, "sequence", "Sequence", "", "New Sequence");
  RNA_def_function_return(func, parm);

  func = RNA_def_function(srna, "new_mask", new_mask_func_name);
  RNA_def_function_flag(func, FUNC_USE_SELF_ID | FUNC_USE_MAIN);
  RNA_def_function_ui_description(func, "Add a new mask sequence");
  parm = RNA_def_string(func, "name", "Name", 0, "", "Name for the new sequence");
  RNA_def_parameter_flags(parm, PropertyFlag(0), PARM_REQUIRED);
  parm = RNA_def_pointer(func, "mask", "Mask", "", "Mask to add");
  RNA_def_parameter_flags(parm, PROP_NEVER_NULL, PARM_REQUIRED);
  parm = RNA_def_int(
      func, "channel", 0, 1, MAXSEQ, "Channel", "The channel for the new sequence", 1, MAXSEQ);
  RNA_def_parameter_flags(parm, PropertyFlag(0), PARM_REQUIRED);
  parm = RNA_def_int(func,
                     "frame_start",
                     0,
                     -MAXFRAME,
                     MAXFRAME,
                     "",
                     "The start frame for the new sequence",
                     -MAXFRAME,
                     MAXFRAME);
  RNA_def_parameter_flags(parm, PropertyFlag(0), PARM_REQUIRED);
  /* return type */
  parm = RNA_def_pointer(func, "sequence", "Sequence", "", "New Sequence");
  RNA_def_function_return(func, parm);

  func = RNA_def_function(srna, "new_scene", new_scene_func_name);
  RNA_def_function_flag(func, FUNC_USE_SELF_ID | FUNC_USE_MAIN);
  RNA_def_function_ui_description(func, "Add a new scene sequence");
  parm = RNA_def_string(func, "name", "Name", 0, "", "Name for the new sequence");
  RNA_def_parameter_flags(parm, PropertyFlag(0), PARM_REQUIRED);
  parm = RNA_def_pointer(func, "scene", "Scene", "", "Scene to add");
  RNA_def_parameter_flags(parm, PROP_NEVER_NULL, PARM_REQUIRED);
  parm = RNA_def_int(
      func, "channel", 0, 1, MAXSEQ, "Channel", "The channel for the new sequence", 1, MAXSEQ);
  RNA_def_parameter_flags(parm, PropertyFlag(0), PARM_REQUIRED);
  parm = RNA_def_int(func,
                     "frame_start",
                     0,
                     -MAXFRAME,
                     MAXFRAME,
                     "",
                     "The start frame for the new sequence",
                     -MAXFRAME,
                     MAXFRAME);
  RNA_def_parameter_flags(parm, PropertyFlag(0), PARM_REQUIRED);
  /* return type */
  parm = RNA_def_pointer(func, "sequence", "Sequence", "", "New Sequence");
  RNA_def_function_return(func, parm);

  func = RNA_def_function(srna, "new_image", new_image_func_name);
  RNA_def_function_flag(func, FUNC_USE_REPORTS | FUNC_USE_SELF_ID | FUNC_USE_MAIN);
  RNA_def_function_ui_description(func, "Add a new image sequence");
  parm = RNA_def_string(func, "name", "Name", 0, "", "Name for the new sequence");
  RNA_def_parameter_flags(parm, PropertyFlag(0), PARM_REQUIRED);
  parm = RNA_def_string(func, "filepath", "File", 0, "", "Filepath to image");
  RNA_def_parameter_flags(parm, PropertyFlag(0), PARM_REQUIRED);
  parm = RNA_def_int(
      func, "channel", 0, 1, MAXSEQ, "Channel", "The channel for the new sequence", 1, MAXSEQ);
  RNA_def_parameter_flags(parm, PropertyFlag(0), PARM_REQUIRED);
  parm = RNA_def_int(func,
                     "frame_start",
                     0,
                     -MAXFRAME,
                     MAXFRAME,
                     "",
                     "The start frame for the new sequence",
                     -MAXFRAME,
                     MAXFRAME);
  RNA_def_parameter_flags(parm, PropertyFlag(0), PARM_REQUIRED);
  parm = RNA_def_enum(
      func, "fit_method", scale_fit_methods, SEQ_USE_ORIGINAL_SIZE, "Image Fit Method", nullptr);
  RNA_def_parameter_flags(parm, PropertyFlag(0), PARM_PYFUNC_OPTIONAL);
  /* return type */
  parm = RNA_def_pointer(func, "sequence", "Sequence", "", "New Sequence");
  RNA_def_function_return(func, parm);

  func = RNA_def_function(srna, "new_movie", new_movie_func_name);
  RNA_def_function_flag(func, FUNC_USE_SELF_ID | FUNC_USE_MAIN);
  RNA_def_function_ui_description(func, "Add a new movie sequence");
  parm = RNA_def_string(func, "name", "Name", 0, "", "Name for the new sequence");
  RNA_def_parameter_flags(parm, PropertyFlag(0), PARM_REQUIRED);
  parm = RNA_def_string(func, "filepath", "File", 0, "", "Filepath to movie");
  RNA_def_parameter_flags(parm, PropertyFlag(0), PARM_REQUIRED);
  parm = RNA_def_int(
      func, "channel", 0, 1, MAXSEQ, "Channel", "The channel for the new sequence", 1, MAXSEQ);
  RNA_def_parameter_flags(parm, PropertyFlag(0), PARM_REQUIRED);
  parm = RNA_def_int(func,
                     "frame_start",
                     0,
                     -MAXFRAME,
                     MAXFRAME,
                     "",
                     "The start frame for the new sequence",
                     -MAXFRAME,
                     MAXFRAME);
  RNA_def_parameter_flags(parm, PropertyFlag(0), PARM_REQUIRED);
  parm = RNA_def_enum(
      func, "fit_method", scale_fit_methods, SEQ_USE_ORIGINAL_SIZE, "Image Fit Method", nullptr);
  RNA_def_parameter_flags(parm, PropertyFlag(0), PARM_PYFUNC_OPTIONAL);
  /* return type */
  parm = RNA_def_pointer(func, "sequence", "Sequence", "", "New Sequence");
  RNA_def_function_return(func, parm);

  func = RNA_def_function(srna, "new_sound", new_sound_func_name);
  RNA_def_function_flag(func, FUNC_USE_REPORTS | FUNC_USE_SELF_ID | FUNC_USE_MAIN);
  RNA_def_function_ui_description(func, "Add a new sound sequence");
  parm = RNA_def_string(func, "name", "Name", 0, "", "Name for the new sequence");
  RNA_def_parameter_flags(parm, PropertyFlag(0), PARM_REQUIRED);
  parm = RNA_def_string(func, "filepath", "File", 0, "", "Filepath to movie");
  RNA_def_parameter_flags(parm, PropertyFlag(0), PARM_REQUIRED);
  parm = RNA_def_int(
      func, "channel", 0, 1, MAXSEQ, "Channel", "The channel for the new sequence", 1, MAXSEQ);
  RNA_def_parameter_flags(parm, PropertyFlag(0), PARM_REQUIRED);
  parm = RNA_def_int(func,
                     "frame_start",
                     0,
                     -MAXFRAME,
                     MAXFRAME,
                     "",
                     "The start frame for the new sequence",
                     -MAXFRAME,
                     MAXFRAME);
  RNA_def_parameter_flags(parm, PropertyFlag(0), PARM_REQUIRED);
  /* return type */
  parm = RNA_def_pointer(func, "sequence", "Sequence", "", "New Sequence");
  RNA_def_function_return(func, parm);

  func = RNA_def_function(srna, "new_meta", new_meta_func_name);
  RNA_def_function_flag(func, FUNC_USE_SELF_ID);
  RNA_def_function_ui_description(func, "Add a new meta sequence");
  parm = RNA_def_string(func, "name", "Name", 0, "", "Name for the new sequence");
  RNA_def_parameter_flags(parm, PropertyFlag(0), PARM_REQUIRED);
  parm = RNA_def_int(
      func, "channel", 0, 1, MAXSEQ, "Channel", "The channel for the new sequence", 1, MAXSEQ);
  RNA_def_parameter_flags(parm, PropertyFlag(0), PARM_REQUIRED);
  parm = RNA_def_int(func,
                     "frame_start",
                     0,
                     -MAXFRAME,
                     MAXFRAME,
                     "",
                     "The start frame for the new sequence",
                     -MAXFRAME,
                     MAXFRAME);
  RNA_def_parameter_flags(parm, PropertyFlag(0), PARM_REQUIRED);
  /* return type */
  parm = RNA_def_pointer(func, "sequence", "Sequence", "", "New Sequence");
  RNA_def_function_return(func, parm);

  func = RNA_def_function(srna, "new_effect", new_effect_func_name);
  RNA_def_function_flag(func, FUNC_USE_REPORTS | FUNC_USE_SELF_ID);
  RNA_def_function_ui_description(func, "Add a new effect sequence");
  parm = RNA_def_string(func, "name", "Name", 0, "", "Name for the new sequence");
  RNA_def_parameter_flags(parm, PropertyFlag(0), PARM_REQUIRED);
  parm = RNA_def_enum(func, "type", seq_effect_items, 0, "Type", "type for the new sequence");
  RNA_def_parameter_flags(parm, PropertyFlag(0), PARM_REQUIRED);
  parm = RNA_def_int(
      func, "channel", 0, 1, MAXSEQ, "Channel", "The channel for the new sequence", 1, MAXSEQ);
  /* don't use MAXFRAME since it makes importer scripts fail */
  RNA_def_parameter_flags(parm, PropertyFlag(0), PARM_REQUIRED);
  parm = RNA_def_int(func,
                     "frame_start",
                     0,
                     INT_MIN,
                     INT_MAX,
                     "",
                     "The start frame for the new sequence",
                     INT_MIN,
                     INT_MAX);
  RNA_def_parameter_flags(parm, PropertyFlag(0), PARM_REQUIRED);
  RNA_def_int(func,
              "frame_end",
              0,
              INT_MIN,
              INT_MAX,
              "",
              "The end frame for the new sequence",
              INT_MIN,
              INT_MAX);
  RNA_def_pointer(func, "seq1", "Sequence", "", "Sequence 1 for effect");
  RNA_def_pointer(func, "seq2", "Sequence", "", "Sequence 2 for effect");
  RNA_def_pointer(func, "seq3", "Sequence", "", "Sequence 3 for effect");
  /* return type */
  parm = RNA_def_pointer(func, "sequence", "Sequence", "", "New Sequence");
  RNA_def_function_return(func, parm);

  func = RNA_def_function(srna, "remove", remove_func_name);
  RNA_def_function_flag(func, FUNC_USE_SELF_ID | FUNC_USE_REPORTS | FUNC_USE_MAIN);
  RNA_def_function_ui_description(func, "Remove a Sequence");
  parm = RNA_def_pointer(func, "sequence", "Sequence", "", "Sequence to remove");
  RNA_def_parameter_flags(parm, PROP_NEVER_NULL, PARM_REQUIRED | PARM_RNAPTR);
  RNA_def_parameter_clear_flags(parm, PROP_THICK_WRAP, ParameterFlag(0));
}

#endif
