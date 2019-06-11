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
 */

/** \file
 * \ingroup RNA
 */

#include <stdlib.h>
#include <stdio.h>
#include <string.h>

#include "DNA_scene_types.h"
#include "DNA_sequence_types.h"

#include "BLI_utildefines.h"

#include "RNA_access.h"
#include "RNA_define.h"

#include "rna_internal.h"

#ifdef RNA_RUNTIME

//#include "DNA_anim_types.h"
#  include "DNA_image_types.h"
#  include "DNA_mask_types.h"
#  include "DNA_sound_types.h"

#  include "BLI_path_util.h" /* BLI_split_dirfile */

#  include "BKE_image.h"
#  include "BKE_mask.h"
#  include "BKE_movieclip.h"

#  include "BKE_report.h"
#  include "BKE_sequencer.h"
#  include "BKE_sound.h"

#  include "IMB_imbuf.h"
#  include "IMB_imbuf_types.h"

#  include "WM_api.h"

static void rna_Sequence_update_rnafunc(ID *id, Sequence *self, bool do_data)
{
  if (do_data) {
    BKE_sequencer_update_changed_seq_and_deps((Scene *)id, self, true, true);
    // new_tstripdata(self); // need 2.6x version of this.
  }
  BKE_sequence_calc((Scene *)id, self);
  BKE_sequence_calc_disp((Scene *)id, self);
}

static void rna_Sequence_swap_internal(Sequence *seq_self,
                                       ReportList *reports,
                                       Sequence *seq_other)
{
  const char *error_msg;

  if (BKE_sequence_swap(seq_self, seq_other, &error_msg) == 0) {
    BKE_report(reports, RPT_ERROR, error_msg);
  }
}

static Sequence *alloc_generic_sequence(
    Editing *ed, const char *name, int frame_start, int channel, int type, const char *file)
{
  Sequence *seq;
  Strip *strip;
  StripElem *se;

  seq = BKE_sequence_alloc(ed->seqbasep, frame_start, channel, type);

  BLI_strncpy(seq->name + 2, name, sizeof(seq->name) - 2);
  BKE_sequence_base_unique_name_recursive(&ed->seqbase, seq);

  seq->strip = strip = MEM_callocN(sizeof(Strip), "strip");
  seq->strip->us = 1;

  if (file) {
    strip->stripdata = se = MEM_callocN(sizeof(StripElem), "stripelem");
    BLI_split_dirfile(file, strip->dir, se->name, sizeof(strip->dir), sizeof(se->name));

    BKE_sequence_init_colorspace(seq);
  }
  else {
    strip->stripdata = NULL;
  }

  return seq;
}

static Sequence *rna_Sequences_new_clip(ID *id,
                                        Editing *ed,
                                        Main *bmain,
                                        const char *name,
                                        MovieClip *clip,
                                        int channel,
                                        int frame_start)
{
  Scene *scene = (Scene *)id;
  Sequence *seq;

  seq = alloc_generic_sequence(ed, name, frame_start, channel, SEQ_TYPE_MOVIECLIP, clip->name);
  seq->clip = clip;
  seq->len = BKE_movieclip_get_duration(clip);
  id_us_plus((ID *)clip);

  BKE_sequence_calc_disp(scene, seq);

  DEG_relations_tag_update(bmain);
  DEG_id_tag_update(&scene->id, ID_RECALC_SEQUENCER_STRIPS);
  WM_main_add_notifier(NC_SCENE | ND_SEQUENCER, scene);

  return seq;
}

static Sequence *rna_Sequences_new_mask(
    ID *id, Editing *ed, Main *bmain, const char *name, Mask *mask, int channel, int frame_start)
{
  Scene *scene = (Scene *)id;
  Sequence *seq;

  seq = alloc_generic_sequence(ed, name, frame_start, channel, SEQ_TYPE_MASK, mask->id.name);
  seq->mask = mask;
  seq->len = BKE_mask_get_duration(mask);
  id_us_plus((ID *)mask);

  BKE_sequence_calc_disp(scene, seq);
  BKE_sequence_invalidate_cache_composite(scene, seq);

  DEG_relations_tag_update(bmain);
  DEG_id_tag_update(&scene->id, ID_RECALC_SEQUENCER_STRIPS);
  WM_main_add_notifier(NC_SCENE | ND_SEQUENCER, scene);

  return seq;
}

static Sequence *rna_Sequences_new_scene(ID *id,
                                         Editing *ed,
                                         Main *bmain,
                                         const char *name,
                                         Scene *sce_seq,
                                         int channel,
                                         int frame_start)
{
  Scene *scene = (Scene *)id;
  Sequence *seq;

  seq = alloc_generic_sequence(ed, name, frame_start, channel, SEQ_TYPE_SCENE, NULL);
  seq->scene = sce_seq;
  seq->len = sce_seq->r.efra - sce_seq->r.sfra + 1;
  id_us_plus((ID *)sce_seq);

  BKE_sequence_calc_disp(scene, seq);
  BKE_sequence_invalidate_cache_composite(scene, seq);

  DEG_relations_tag_update(bmain);
  DEG_id_tag_update(&scene->id, ID_RECALC_SEQUENCER_STRIPS);
  WM_main_add_notifier(NC_SCENE | ND_SEQUENCER, scene);

  return seq;
}

static Sequence *rna_Sequences_new_image(ID *id,
                                         Editing *ed,
                                         Main *bmain,
                                         ReportList *reports,
                                         const char *name,
                                         const char *file,
                                         int channel,
                                         int frame_start)
{
  Scene *scene = (Scene *)id;
  Sequence *seq;

  seq = alloc_generic_sequence(ed, name, frame_start, channel, SEQ_TYPE_IMAGE, file);
  seq->len = 1;

  if (seq->strip->stripdata->name[0] == '\0') {
    BKE_report(reports, RPT_ERROR, "Sequences.new_image: unable to open image file");
    BLI_remlink(&ed->seqbase, seq);
    BKE_sequence_free(scene, seq);
    return NULL;
  }

  BKE_sequence_calc_disp(scene, seq);
  BKE_sequence_invalidate_cache_composite(scene, seq);

  DEG_relations_tag_update(bmain);
  DEG_id_tag_update(&scene->id, ID_RECALC_SEQUENCER_STRIPS);
  WM_main_add_notifier(NC_SCENE | ND_SEQUENCER, scene);

  return seq;
}

static Sequence *rna_Sequences_new_movie(ID *id,
                                         Editing *ed,
                                         ReportList *reports,
                                         const char *name,
                                         const char *file,
                                         int channel,
                                         int frame_start)
{
  Scene *scene = (Scene *)id;
  Sequence *seq;
  StripAnim *sanim;

  struct anim *an = openanim(file, IB_rect, 0, NULL);

  if (an == NULL) {
    BKE_report(reports, RPT_ERROR, "Sequences.new_movie: unable to open movie file");
    return NULL;
  }

  seq = alloc_generic_sequence(ed, name, frame_start, channel, SEQ_TYPE_MOVIE, file);

  sanim = MEM_mallocN(sizeof(StripAnim), "Strip Anim");
  BLI_addtail(&seq->anims, sanim);
  sanim->anim = an;

  seq->anim_preseek = IMB_anim_get_preseek(an);
  seq->len = IMB_anim_get_duration(an, IMB_TC_RECORD_RUN);

  BKE_sequence_calc_disp(scene, seq);
  BKE_sequence_invalidate_cache_composite(scene, seq);

  DEG_id_tag_update(&scene->id, ID_RECALC_SEQUENCER_STRIPS);
  WM_main_add_notifier(NC_SCENE | ND_SEQUENCER, scene);

  return seq;
}

#  ifdef WITH_AUDASPACE
static Sequence *rna_Sequences_new_sound(ID *id,
                                         Editing *ed,
                                         Main *bmain,
                                         ReportList *reports,
                                         const char *name,
                                         const char *file,
                                         int channel,
                                         int frame_start)
{
  Scene *scene = (Scene *)id;
  Sequence *seq;

  bSound *sound = BKE_sound_new_file(bmain, file);

  SoundInfo info;
  if (!BKE_sound_info_get(bmain, sound, &info)) {
    BKE_id_free(bmain, sound);
    BKE_report(reports, RPT_ERROR, "Sequences.new_sound: unable to open sound file");
    return NULL;
  }
  seq = alloc_generic_sequence(ed, name, frame_start, channel, SEQ_TYPE_SOUND_RAM, sound->name);
  seq->sound = sound;
  seq->len = ceil((double)info.length * FPS);

  BKE_sequence_calc_disp(scene, seq);

  DEG_relations_tag_update(bmain);
  DEG_id_tag_update(&scene->id, ID_RECALC_SEQUENCER_STRIPS);
  WM_main_add_notifier(NC_SCENE | ND_SEQUENCER, scene);

  return seq;
}
#  else  /* WITH_AUDASPACE */
static Sequence *rna_Sequences_new_sound(ID *UNUSED(id),
                                         Editing *UNUSED(ed),
                                         Main *UNUSED(bmain),
                                         ReportList *reports,
                                         const char *UNUSED(name),
                                         const char *UNUSED(file),
                                         int UNUSED(channel),
                                         int UNUSED(frame_start))
{
  BKE_report(reports, RPT_ERROR, "Blender compiled without Audaspace support");
  return NULL;
}
#  endif /* WITH_AUDASPACE */

static Sequence *rna_Sequences_new_effect(ID *id,
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
  Scene *scene = (Scene *)id;
  Sequence *seq;
  struct SeqEffectHandle sh;
  int num_inputs = BKE_sequence_effect_get_num_inputs(type);

  switch (num_inputs) {
    case 0:
      if (frame_end <= frame_start) {
        BKE_report(reports, RPT_ERROR, "Sequences.new_effect: end frame not set");
        return NULL;
      }
      break;
    case 1:
      if (seq1 == NULL) {
        BKE_report(reports, RPT_ERROR, "Sequences.new_effect: effect takes 1 input sequence");
        return NULL;
      }
      break;
    case 2:
      if (seq1 == NULL || seq2 == NULL) {
        BKE_report(reports, RPT_ERROR, "Sequences.new_effect: effect takes 2 input sequences");
        return NULL;
      }
      break;
    case 3:
      if (seq1 == NULL || seq2 == NULL || seq3 == NULL) {
        BKE_report(reports, RPT_ERROR, "Sequences.new_effect: effect takes 3 input sequences");
        return NULL;
      }
      break;
    default:
      BKE_reportf(
          reports,
          RPT_ERROR,
          "Sequences.new_effect: effect expects more than 3 inputs (%d, should never happen!)",
          num_inputs);
      return NULL;
  }

  seq = alloc_generic_sequence(ed, name, frame_start, channel, type, NULL);

  sh = BKE_sequence_get_effect(seq);

  seq->seq1 = seq1;
  seq->seq2 = seq2;
  seq->seq3 = seq3;

  sh.init(seq);

  if (!seq1) { /* effect has no deps */
    seq->len = 1;
    BKE_sequence_tx_set_final_right(seq, frame_end);
  }

  seq->flag |= SEQ_USE_EFFECT_DEFAULT_FADE;

  BKE_sequence_calc(scene, seq);
  BKE_sequence_calc_disp(scene, seq);
  BKE_sequence_invalidate_cache_composite(scene, seq);

  DEG_id_tag_update(&scene->id, ID_RECALC_SEQUENCER_STRIPS);
  WM_main_add_notifier(NC_SCENE | ND_SEQUENCER, scene);

  return seq;
}

static void rna_Sequences_remove(
    ID *id, Editing *ed, Main *bmain, ReportList *reports, PointerRNA *seq_ptr)
{
  Sequence *seq = seq_ptr->data;
  Scene *scene = (Scene *)id;

  if (BLI_remlink_safe(&ed->seqbase, seq) == false) {
    BKE_reportf(
        reports, RPT_ERROR, "Sequence '%s' not in scene '%s'", seq->name + 2, scene->id.name + 2);
    return;
  }

  BKE_sequence_free(scene, seq);
  RNA_POINTER_INVALIDATE(seq_ptr);

  DEG_relations_tag_update(bmain);
  DEG_id_tag_update(&scene->id, ID_RECALC_SEQUENCER_STRIPS);
  WM_main_add_notifier(NC_SCENE | ND_SEQUENCER, scene);
}

static StripElem *rna_SequenceElements_append(ID *id, Sequence *seq, const char *filename)
{
  Scene *scene = (Scene *)id;
  StripElem *se;

  seq->strip->stripdata = se = MEM_reallocN(seq->strip->stripdata,
                                            sizeof(StripElem) * (seq->len + 1));
  se += seq->len;
  BLI_strncpy(se->name, filename, sizeof(se->name));
  seq->len++;

  BKE_sequence_calc_disp(scene, seq);
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

  new_seq = MEM_callocN(sizeof(StripElem) * (seq->len - 1), "SequenceElements_pop");
  seq->len--;

  se = seq->strip->stripdata;
  if (index > 0) {
    memcpy(new_seq, se, sizeof(StripElem) * index);
  }

  if (index < seq->len) {
    memcpy(&new_seq[index], &se[index + 1], sizeof(StripElem) * (seq->len - index));
  }

  MEM_freeN(seq->strip->stripdata);
  seq->strip->stripdata = new_seq;

  BKE_sequence_calc_disp(scene, seq);

  WM_main_add_notifier(NC_SCENE | ND_SEQUENCER, scene);
}

#else

void RNA_api_sequence_strip(StructRNA *srna)
{
  FunctionRNA *func;
  PropertyRNA *parm;

  func = RNA_def_function(srna, "update", "rna_Sequence_update_rnafunc");
  RNA_def_function_flag(func, FUNC_USE_SELF_ID);
  RNA_def_function_ui_description(func, "Update the strip dimensions");
  parm = RNA_def_boolean(func, "data", false, "Data", "Update strip data");

  func = RNA_def_function(srna, "strip_elem_from_frame", "BKE_sequencer_give_stripelem");
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
  RNA_def_parameter_flags(parm, 0, PARM_REQUIRED);
  RNA_def_function_return(
      func,
      RNA_def_pointer(func, "elem", "SequenceElement", "", "strip element of the current frame"));

  func = RNA_def_function(srna, "swap", "rna_Sequence_swap_internal");
  RNA_def_function_flag(func, FUNC_USE_REPORTS);
  parm = RNA_def_pointer(func, "other", "Sequence", "Other", "");
  RNA_def_parameter_flags(parm, PROP_NEVER_NULL, PARM_REQUIRED);
}

void RNA_api_sequence_elements(BlenderRNA *brna, PropertyRNA *cprop)
{
  StructRNA *srna;
  PropertyRNA *parm;
  FunctionRNA *func;

  RNA_def_property_srna(cprop, "SequenceElements");
  srna = RNA_def_struct(brna, "SequenceElements", NULL);
  RNA_def_struct_sdna(srna, "Sequence");
  RNA_def_struct_ui_text(srna, "SequenceElements", "Collection of SequenceElement");

  func = RNA_def_function(srna, "append", "rna_SequenceElements_append");
  RNA_def_function_flag(func, FUNC_USE_SELF_ID);
  RNA_def_function_ui_description(func, "Push an image from ImageSequence.directory");
  parm = RNA_def_string(func, "filename", "File", 0, "", "Filepath to image");
  RNA_def_parameter_flags(parm, 0, PARM_REQUIRED);
  /* return type */
  parm = RNA_def_pointer(func, "elem", "SequenceElement", "", "New SequenceElement");
  RNA_def_function_return(func, parm);

  func = RNA_def_function(srna, "pop", "rna_SequenceElements_pop");
  RNA_def_function_flag(func, FUNC_USE_REPORTS | FUNC_USE_SELF_ID);
  RNA_def_function_ui_description(func, "Pop an image off the collection");
  parm = RNA_def_int(
      func, "index", -1, INT_MIN, INT_MAX, "", "Index of image to remove", INT_MIN, INT_MAX);
  RNA_def_parameter_flags(parm, 0, PARM_REQUIRED);
}

void RNA_api_sequences(BlenderRNA *brna, PropertyRNA *cprop)
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
      {0, NULL, 0, NULL, NULL},
  };

  RNA_def_property_srna(cprop, "Sequences");
  srna = RNA_def_struct(brna, "Sequences", NULL);
  RNA_def_struct_sdna(srna, "Editing");
  RNA_def_struct_ui_text(srna, "Sequences", "Collection of Sequences");

  func = RNA_def_function(srna, "new_clip", "rna_Sequences_new_clip");
  RNA_def_function_flag(func, FUNC_USE_SELF_ID | FUNC_USE_MAIN);
  RNA_def_function_ui_description(func, "Add a new movie clip sequence");
  parm = RNA_def_string(func, "name", "Name", 0, "", "Name for the new sequence");
  RNA_def_parameter_flags(parm, 0, PARM_REQUIRED);
  parm = RNA_def_pointer(func, "clip", "MovieClip", "", "Movie clip to add");
  RNA_def_parameter_flags(parm, PROP_NEVER_NULL, PARM_REQUIRED);
  parm = RNA_def_int(
      func, "channel", 0, 1, MAXSEQ, "Channel", "The channel for the new sequence", 1, MAXSEQ);
  RNA_def_parameter_flags(parm, 0, PARM_REQUIRED);
  parm = RNA_def_int(func,
                     "frame_start",
                     0,
                     -MAXFRAME,
                     MAXFRAME,
                     "",
                     "The start frame for the new sequence",
                     -MAXFRAME,
                     MAXFRAME);
  RNA_def_parameter_flags(parm, 0, PARM_REQUIRED);
  /* return type */
  parm = RNA_def_pointer(func, "sequence", "Sequence", "", "New Sequence");
  RNA_def_function_return(func, parm);

  func = RNA_def_function(srna, "new_mask", "rna_Sequences_new_mask");
  RNA_def_function_flag(func, FUNC_USE_SELF_ID | FUNC_USE_MAIN);
  RNA_def_function_ui_description(func, "Add a new mask sequence");
  parm = RNA_def_string(func, "name", "Name", 0, "", "Name for the new sequence");
  RNA_def_parameter_flags(parm, 0, PARM_REQUIRED);
  parm = RNA_def_pointer(func, "mask", "Mask", "", "Mask to add");
  RNA_def_parameter_flags(parm, PROP_NEVER_NULL, PARM_REQUIRED);
  parm = RNA_def_int(
      func, "channel", 0, 1, MAXSEQ, "Channel", "The channel for the new sequence", 1, MAXSEQ);
  RNA_def_parameter_flags(parm, 0, PARM_REQUIRED);
  parm = RNA_def_int(func,
                     "frame_start",
                     0,
                     -MAXFRAME,
                     MAXFRAME,
                     "",
                     "The start frame for the new sequence",
                     -MAXFRAME,
                     MAXFRAME);
  RNA_def_parameter_flags(parm, 0, PARM_REQUIRED);
  /* return type */
  parm = RNA_def_pointer(func, "sequence", "Sequence", "", "New Sequence");
  RNA_def_function_return(func, parm);

  func = RNA_def_function(srna, "new_scene", "rna_Sequences_new_scene");
  RNA_def_function_flag(func, FUNC_USE_SELF_ID | FUNC_USE_MAIN);
  RNA_def_function_ui_description(func, "Add a new scene sequence");
  parm = RNA_def_string(func, "name", "Name", 0, "", "Name for the new sequence");
  RNA_def_parameter_flags(parm, 0, PARM_REQUIRED);
  parm = RNA_def_pointer(func, "scene", "Scene", "", "Scene to add");
  RNA_def_parameter_flags(parm, PROP_NEVER_NULL, PARM_REQUIRED);
  parm = RNA_def_int(
      func, "channel", 0, 1, MAXSEQ, "Channel", "The channel for the new sequence", 1, MAXSEQ);
  RNA_def_parameter_flags(parm, 0, PARM_REQUIRED);
  parm = RNA_def_int(func,
                     "frame_start",
                     0,
                     -MAXFRAME,
                     MAXFRAME,
                     "",
                     "The start frame for the new sequence",
                     -MAXFRAME,
                     MAXFRAME);
  RNA_def_parameter_flags(parm, 0, PARM_REQUIRED);
  /* return type */
  parm = RNA_def_pointer(func, "sequence", "Sequence", "", "New Sequence");
  RNA_def_function_return(func, parm);

  func = RNA_def_function(srna, "new_image", "rna_Sequences_new_image");
  RNA_def_function_flag(func, FUNC_USE_REPORTS | FUNC_USE_SELF_ID | FUNC_USE_MAIN);
  RNA_def_function_ui_description(func, "Add a new image sequence");
  parm = RNA_def_string(func, "name", "Name", 0, "", "Name for the new sequence");
  RNA_def_parameter_flags(parm, 0, PARM_REQUIRED);
  parm = RNA_def_string(func, "filepath", "File", 0, "", "Filepath to image");
  RNA_def_parameter_flags(parm, 0, PARM_REQUIRED);
  parm = RNA_def_int(
      func, "channel", 0, 1, MAXSEQ, "Channel", "The channel for the new sequence", 1, MAXSEQ);
  RNA_def_parameter_flags(parm, 0, PARM_REQUIRED);
  parm = RNA_def_int(func,
                     "frame_start",
                     0,
                     -MAXFRAME,
                     MAXFRAME,
                     "",
                     "The start frame for the new sequence",
                     -MAXFRAME,
                     MAXFRAME);
  RNA_def_parameter_flags(parm, 0, PARM_REQUIRED);
  /* return type */
  parm = RNA_def_pointer(func, "sequence", "Sequence", "", "New Sequence");
  RNA_def_function_return(func, parm);

  func = RNA_def_function(srna, "new_movie", "rna_Sequences_new_movie");
  RNA_def_function_flag(func, FUNC_USE_REPORTS | FUNC_USE_SELF_ID);
  RNA_def_function_ui_description(func, "Add a new movie sequence");
  parm = RNA_def_string(func, "name", "Name", 0, "", "Name for the new sequence");
  RNA_def_parameter_flags(parm, 0, PARM_REQUIRED);
  parm = RNA_def_string(func, "filepath", "File", 0, "", "Filepath to movie");
  RNA_def_parameter_flags(parm, 0, PARM_REQUIRED);
  parm = RNA_def_int(
      func, "channel", 0, 1, MAXSEQ, "Channel", "The channel for the new sequence", 1, MAXSEQ);
  RNA_def_parameter_flags(parm, 0, PARM_REQUIRED);
  parm = RNA_def_int(func,
                     "frame_start",
                     0,
                     -MAXFRAME,
                     MAXFRAME,
                     "",
                     "The start frame for the new sequence",
                     -MAXFRAME,
                     MAXFRAME);
  RNA_def_parameter_flags(parm, 0, PARM_REQUIRED);
  /* return type */
  parm = RNA_def_pointer(func, "sequence", "Sequence", "", "New Sequence");
  RNA_def_function_return(func, parm);

  func = RNA_def_function(srna, "new_sound", "rna_Sequences_new_sound");
  RNA_def_function_flag(func, FUNC_USE_REPORTS | FUNC_USE_SELF_ID | FUNC_USE_MAIN);
  RNA_def_function_ui_description(func, "Add a new sound sequence");
  parm = RNA_def_string(func, "name", "Name", 0, "", "Name for the new sequence");
  RNA_def_parameter_flags(parm, 0, PARM_REQUIRED);
  parm = RNA_def_string(func, "filepath", "File", 0, "", "Filepath to movie");
  RNA_def_parameter_flags(parm, 0, PARM_REQUIRED);
  parm = RNA_def_int(
      func, "channel", 0, 1, MAXSEQ, "Channel", "The channel for the new sequence", 1, MAXSEQ);
  RNA_def_parameter_flags(parm, 0, PARM_REQUIRED);
  parm = RNA_def_int(func,
                     "frame_start",
                     0,
                     -MAXFRAME,
                     MAXFRAME,
                     "",
                     "The start frame for the new sequence",
                     -MAXFRAME,
                     MAXFRAME);
  RNA_def_parameter_flags(parm, 0, PARM_REQUIRED);
  /* return type */
  parm = RNA_def_pointer(func, "sequence", "Sequence", "", "New Sequence");
  RNA_def_function_return(func, parm);

  func = RNA_def_function(srna, "new_effect", "rna_Sequences_new_effect");
  RNA_def_function_flag(func, FUNC_USE_REPORTS | FUNC_USE_SELF_ID);
  RNA_def_function_ui_description(func, "Add a new effect sequence");
  parm = RNA_def_string(func, "name", "Name", 0, "", "Name for the new sequence");
  RNA_def_parameter_flags(parm, 0, PARM_REQUIRED);
  parm = RNA_def_enum(func, "type", seq_effect_items, 0, "Type", "type for the new sequence");
  RNA_def_parameter_flags(parm, 0, PARM_REQUIRED);
  parm = RNA_def_int(
      func, "channel", 0, 1, MAXSEQ, "Channel", "The channel for the new sequence", 1, MAXSEQ);
  /* don't use MAXFRAME since it makes importer scripts fail */
  RNA_def_parameter_flags(parm, 0, PARM_REQUIRED);
  parm = RNA_def_int(func,
                     "frame_start",
                     0,
                     INT_MIN,
                     INT_MAX,
                     "",
                     "The start frame for the new sequence",
                     INT_MIN,
                     INT_MAX);
  RNA_def_parameter_flags(parm, 0, PARM_REQUIRED);
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

  func = RNA_def_function(srna, "remove", "rna_Sequences_remove");
  RNA_def_function_flag(func, FUNC_USE_SELF_ID | FUNC_USE_REPORTS | FUNC_USE_MAIN);
  RNA_def_function_ui_description(func, "Remove a Sequence");
  parm = RNA_def_pointer(func, "sequence", "Sequence", "", "Sequence to remove");
  RNA_def_parameter_flags(parm, PROP_NEVER_NULL, PARM_REQUIRED | PARM_RNAPTR);
  RNA_def_parameter_clear_flags(parm, PROP_THICK_WRAP, 0);
}

#endif
