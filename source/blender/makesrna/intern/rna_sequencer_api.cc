/* SPDX-FileCopyrightText: 2023 Blender Authors
 *
 * SPDX-License-Identifier: GPL-2.0-or-later */

/** \file
 * \ingroup RNA
 */

#include <cstdlib>
#include <cstring>

#include "DNA_scene_types.h"
#include "DNA_sequence_types.h"

#include "RNA_define.hh"

#include "RNA_enum_types.hh"
#include "SEQ_edit.hh"
#include "SEQ_sequencer.hh"

#include "rna_internal.hh"

#ifdef RNA_RUNTIME

// #include "DNA_anim_types.h"
#  include "DNA_image_types.h"
#  include "DNA_mask_types.h"
#  include "DNA_sound_types.h"

#  include "BLI_path_utils.hh" /* #BLI_path_split_dir_file */

#  include "BKE_image.hh"
#  include "BKE_mask.h"
#  include "BKE_movieclip.h"

#  include "BKE_report.hh"

#  include "IMB_imbuf.hh"
#  include "IMB_imbuf_types.hh"

#  include "SEQ_add.hh"
#  include "SEQ_edit.hh"
#  include "SEQ_effects.hh"
#  include "SEQ_relations.hh"
#  include "SEQ_render.hh"
#  include "SEQ_retiming.hh"
#  include "SEQ_time.hh"

#  include "WM_api.hh"

static StripElem *rna_Strip_elem_from_frame(ID *id, Strip *self, int timeline_frame)
{
  Scene *scene = (Scene *)id;
  return blender::seq::render_give_stripelem(scene, self, timeline_frame);
}

static void rna_Strip_swap_internal(ID *id,
                                    Strip *strip_self,
                                    ReportList *reports,
                                    Strip *strip_other)
{
  const char *error_msg;
  Scene *scene = (Scene *)id;

  if (blender::seq::edit_strip_swap(scene, strip_self, strip_other, &error_msg) == false) {
    BKE_report(reports, RPT_ERROR, error_msg);
  }
}

static void rna_Strips_move_strip_to_meta(
    ID *id, Strip *strip_self, Main *bmain, ReportList *reports, Strip *meta_dst)
{
  Scene *scene = (Scene *)id;
  const char *error_msg;

  /* Move strip to meta. */
  if (!blender::seq::edit_move_strip_to_meta(scene, strip_self, meta_dst, &error_msg)) {
    BKE_report(reports, RPT_ERROR, error_msg);
  }

  /* Update depsgraph. */
  DEG_relations_tag_update(bmain);
  DEG_id_tag_update(&scene->id, ID_RECALC_SEQUENCER_STRIPS);

  blender::seq::strip_lookup_invalidate(scene->ed);

  WM_main_add_notifier(NC_SCENE | ND_SEQUENCER, scene);
}

static Strip *rna_Strip_split(ID *id,
                              Strip *strip,
                              Main *bmain,
                              ReportList *reports,
                              int frame,
                              int split_method,
                              bool ignore_connections)
{
  Scene *scene = (Scene *)id;
  ListBase *seqbase = blender::seq::get_seqbase_by_strip(scene, strip);

  const char *error_msg = nullptr;
  Strip *strip_split = blender::seq::edit_strip_split(bmain,
                                                      scene,
                                                      seqbase,
                                                      strip,
                                                      frame,
                                                      blender::seq::eSplitMethod(split_method),
                                                      ignore_connections,
                                                      &error_msg);
  if (error_msg != nullptr) {
    BKE_report(reports, RPT_ERROR, error_msg);
  }

  /* Update depsgraph. */
  DEG_relations_tag_update(bmain);
  DEG_id_tag_update(&scene->id, ID_RECALC_SEQUENCER_STRIPS);

  WM_main_add_notifier(NC_SCENE | ND_SEQUENCER, scene);

  return strip_split;
}

static Strip *rna_Strip_parent_meta(ID *id, Strip *strip_self)
{
  Scene *scene = (Scene *)id;
  return blender::seq::lookup_meta_by_strip(blender::seq::editing_get(scene), strip_self);
}

static Strip *rna_Strips_new_clip(ID *id,
                                  ListBase *seqbase,
                                  Main *bmain,
                                  const char *name,
                                  MovieClip *clip,
                                  int channel,
                                  int frame_start)
{
  Scene *scene = (Scene *)id;
  blender::seq::LoadData load_data;
  blender::seq::add_load_data_init(&load_data, name, nullptr, frame_start, channel);
  load_data.clip = clip;
  Strip *strip = blender::seq::add_movieclip_strip(scene, seqbase, &load_data);

  DEG_relations_tag_update(bmain);
  DEG_id_tag_update(&scene->id, ID_RECALC_SEQUENCER_STRIPS);
  WM_main_add_notifier(NC_SCENE | ND_SEQUENCER, scene);

  return strip;
}

static Strip *rna_Strips_editing_new_clip(ID *id,
                                          Editing *ed,
                                          Main *bmain,
                                          const char *name,
                                          MovieClip *clip,
                                          int channel,
                                          int frame_start)
{
  return rna_Strips_new_clip(id, &ed->seqbase, bmain, name, clip, channel, frame_start);
}

static Strip *rna_Strips_meta_new_clip(ID *id,
                                       Strip *strip,
                                       Main *bmain,
                                       const char *name,
                                       MovieClip *clip,
                                       int channel,
                                       int frame_start)
{
  return rna_Strips_new_clip(id, &strip->seqbase, bmain, name, clip, channel, frame_start);
}

static Strip *rna_Strips_new_mask(ID *id,
                                  ListBase *seqbase,
                                  Main *bmain,
                                  const char *name,
                                  Mask *mask,
                                  int channel,
                                  int frame_start)
{
  Scene *scene = (Scene *)id;
  blender::seq::LoadData load_data;
  blender::seq::add_load_data_init(&load_data, name, nullptr, frame_start, channel);
  load_data.mask = mask;
  Strip *strip = blender::seq::add_mask_strip(scene, seqbase, &load_data);

  DEG_relations_tag_update(bmain);
  DEG_id_tag_update(&scene->id, ID_RECALC_SEQUENCER_STRIPS);
  WM_main_add_notifier(NC_SCENE | ND_SEQUENCER, scene);

  return strip;
}
static Strip *rna_Strips_editing_new_mask(
    ID *id, Editing *ed, Main *bmain, const char *name, Mask *mask, int channel, int frame_start)
{
  return rna_Strips_new_mask(id, &ed->seqbase, bmain, name, mask, channel, frame_start);
}

static Strip *rna_Strips_meta_new_mask(
    ID *id, Strip *strip, Main *bmain, const char *name, Mask *mask, int channel, int frame_start)
{
  return rna_Strips_new_mask(id, &strip->seqbase, bmain, name, mask, channel, frame_start);
}

static Strip *rna_Strips_new_scene(ID *id,
                                   ListBase *seqbase,
                                   Main *bmain,
                                   const char *name,
                                   Scene *sce_strip,
                                   int channel,
                                   int frame_start)
{
  Scene *scene = (Scene *)id;
  blender::seq::LoadData load_data;
  blender::seq::add_load_data_init(&load_data, name, nullptr, frame_start, channel);
  load_data.scene = sce_strip;
  Strip *strip = blender::seq::add_scene_strip(scene, seqbase, &load_data);

  DEG_relations_tag_update(bmain);
  DEG_id_tag_update(&scene->id, ID_RECALC_SEQUENCER_STRIPS);
  WM_main_add_notifier(NC_SCENE | ND_SEQUENCER, scene);

  return strip;
}

static Strip *rna_Strips_editing_new_scene(ID *id,
                                           Editing *ed,
                                           Main *bmain,
                                           const char *name,
                                           Scene *sce_strip,
                                           int channel,
                                           int frame_start)
{
  return rna_Strips_new_scene(id, &ed->seqbase, bmain, name, sce_strip, channel, frame_start);
}

static Strip *rna_Strips_meta_new_scene(ID *id,
                                        Strip *strip,
                                        Main *bmain,
                                        const char *name,
                                        Scene *sce_strip,
                                        int channel,
                                        int frame_start)
{
  return rna_Strips_new_scene(id, &strip->seqbase, bmain, name, sce_strip, channel, frame_start);
}

static Strip *rna_Strips_new_image(ID *id,
                                   ListBase *seqbase,
                                   Main *bmain,
                                   ReportList *reports,
                                   const char *name,
                                   const char *file,
                                   int channel,
                                   int frame_start,
                                   int fit_method)
{
  Scene *scene = (Scene *)id;

  blender::seq::LoadData load_data;
  blender::seq::add_load_data_init(&load_data, name, file, frame_start, channel);
  load_data.image.count = 1;
  load_data.fit_method = eSeqImageFitMethod(fit_method);

  char vt_old[64];
  STRNCPY_UTF8(vt_old, scene->view_settings.view_transform);

  Strip *strip = blender::seq::add_image_strip(bmain, scene, seqbase, &load_data);

  if (!STREQ(vt_old, scene->view_settings.view_transform)) {
    BKE_reportf(reports,
                RPT_WARNING,
                "View transform set to %s (converted from %s)",
                scene->view_settings.view_transform,
                vt_old);
  }

  char dirpath[FILE_MAX], filename[FILE_MAXFILE];
  BLI_path_split_dir_file(file, dirpath, sizeof(dirpath), filename, sizeof(filename));
  blender::seq::add_image_set_directory(strip, dirpath);
  blender::seq::add_image_load_file(scene, strip, 0, filename);
  blender::seq::add_image_init_alpha_mode(bmain, scene, strip);

  DEG_relations_tag_update(bmain);
  DEG_id_tag_update(&scene->id, ID_RECALC_SEQUENCER_STRIPS);
  WM_main_add_notifier(NC_SCENE | ND_SEQUENCER, scene);

  return strip;
}

static Strip *rna_Strips_editing_new_image(ID *id,
                                           Editing *ed,
                                           Main *bmain,
                                           ReportList *reports,
                                           const char *name,
                                           const char *file,
                                           int channel,
                                           int frame_start,
                                           int fit_method)
{
  return rna_Strips_new_image(
      id, &ed->seqbase, bmain, reports, name, file, channel, frame_start, fit_method);
}

static Strip *rna_Strips_meta_new_image(ID *id,
                                        Strip *strip,
                                        Main *bmain,
                                        ReportList *reports,
                                        const char *name,
                                        const char *file,
                                        int channel,
                                        int frame_start,
                                        int fit_method)
{
  return rna_Strips_new_image(
      id, &strip->seqbase, bmain, reports, name, file, channel, frame_start, fit_method);
}

static Strip *rna_Strips_new_movie(ID *id,
                                   ListBase *seqbase,
                                   Main *bmain,
                                   ReportList *reports,
                                   const char *name,
                                   const char *file,
                                   int channel,
                                   int frame_start,
                                   int fit_method)
{
  Scene *scene = (Scene *)id;
  blender::seq::LoadData load_data;
  blender::seq::add_load_data_init(&load_data, name, file, frame_start, channel);
  load_data.fit_method = eSeqImageFitMethod(fit_method);
  load_data.allow_invalid_file = true;

  char vt_old[64];
  STRNCPY_UTF8(vt_old, scene->view_settings.view_transform);
  float fps_old = scene->r.frs_sec / scene->r.frs_sec_base;

  Strip *strip = blender::seq::add_movie_strip(bmain, scene, seqbase, &load_data);

  if (!STREQ(vt_old, scene->view_settings.view_transform)) {
    BKE_reportf(reports,
                RPT_WARNING,
                "View transform set to %s (converted from %s)",
                scene->view_settings.view_transform,
                vt_old);
  }

  if (fps_old != scene->r.frs_sec / scene->r.frs_sec_base) {
    BKE_reportf(reports,
                RPT_WARNING,
                "Scene frame rate set to %.4g (converted from %.4g)",
                scene->r.frs_sec / scene->r.frs_sec_base,
                fps_old);
  }

  DEG_relations_tag_update(bmain);
  DEG_id_tag_update(&scene->id, ID_RECALC_SEQUENCER_STRIPS);
  WM_main_add_notifier(NC_SCENE | ND_SEQUENCER, scene);

  return strip;
}

static Strip *rna_Strips_editing_new_movie(ID *id,
                                           Editing *ed,
                                           Main *bmain,
                                           ReportList *reports,
                                           const char *name,
                                           const char *file,
                                           int channel,
                                           int frame_start,
                                           int fit_method)
{
  return rna_Strips_new_movie(
      id, &ed->seqbase, bmain, reports, name, file, channel, frame_start, fit_method);
}

static Strip *rna_Strips_meta_new_movie(ID *id,
                                        Strip *strip,
                                        Main *bmain,
                                        ReportList *reports,
                                        const char *name,
                                        const char *file,
                                        int channel,
                                        int frame_start,
                                        int fit_method)
{
  return rna_Strips_new_movie(
      id, &strip->seqbase, bmain, reports, name, file, channel, frame_start, fit_method);
}

#  ifdef WITH_AUDASPACE
static Strip *rna_Strips_new_sound(ID *id,
                                   ListBase *seqbase,
                                   Main *bmain,
                                   ReportList *reports,
                                   const char *name,
                                   const char *file,
                                   int channel,
                                   int frame_start)
{
  Scene *scene = (Scene *)id;
  blender::seq::LoadData load_data;
  blender::seq::add_load_data_init(&load_data, name, file, frame_start, channel);
  load_data.allow_invalid_file = true;
  Strip *strip = blender::seq::add_sound_strip(bmain, scene, seqbase, &load_data);

  if (strip == nullptr) {
    BKE_report(reports, RPT_ERROR, "Strips.new_sound: unable to open sound file");
    return nullptr;
  }

  DEG_relations_tag_update(bmain);
  DEG_id_tag_update(&scene->id, ID_RECALC_SEQUENCER_STRIPS);
  WM_main_add_notifier(NC_SCENE | ND_SEQUENCER, scene);

  return strip;
}
#  else  /* WITH_AUDASPACE */
static Strip *rna_Strips_new_sound(ID * /*id*/,
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

static Strip *rna_Strips_editing_new_sound(ID *id,
                                           Editing *ed,
                                           Main *bmain,
                                           ReportList *reports,
                                           const char *name,
                                           const char *file,
                                           int channel,
                                           int frame_start)
{
  return rna_Strips_new_sound(id, &ed->seqbase, bmain, reports, name, file, channel, frame_start);
}

static Strip *rna_Strips_meta_new_sound(ID *id,
                                        Strip *strip,
                                        Main *bmain,
                                        ReportList *reports,
                                        const char *name,
                                        const char *file,
                                        int channel,
                                        int frame_start)
{
  return rna_Strips_new_sound(
      id, &strip->seqbase, bmain, reports, name, file, channel, frame_start);
}

/* Meta strip
 * Possibility to create an empty meta to avoid plenty of meta toggling
 * Created meta have a length equal to 1, must be set through the API. */
static Strip *rna_Strips_new_meta(
    ID *id, ListBase *seqbase, const char *name, int channel, int frame_start)
{
  Scene *scene = (Scene *)id;
  blender::seq::LoadData load_data;
  blender::seq::add_load_data_init(&load_data, name, nullptr, frame_start, channel);
  Strip *seqm = blender::seq::add_meta_strip(scene, seqbase, &load_data);

  return seqm;
}

static Strip *rna_Strips_editing_new_meta(
    ID *id, Editing *ed, const char *name, int channel, int frame_start)
{
  return rna_Strips_new_meta(id, &ed->seqbase, name, channel, frame_start);
}

static Strip *rna_Strips_meta_new_meta(
    ID *id, Strip *strip, const char *name, int channel, int frame_start)
{
  return rna_Strips_new_meta(id, &strip->seqbase, name, channel, frame_start);
}

static Strip *rna_Strips_new_effect(ID *id,
                                    ListBase *seqbase,
                                    ReportList *reports,
                                    const char *name,
                                    int type,
                                    int channel,
                                    int frame_start,
                                    int length,
                                    Strip *input1,
                                    Strip *input2)
{
  Scene *scene = (Scene *)id;
  Strip *strip;
  const int num_inputs = blender::seq::effect_get_num_inputs(type);

  switch (num_inputs) {
    case 0:
      if (length <= 0) {
        BKE_report(reports, RPT_ERROR, "Strips.new_effect: invalid length");
        return nullptr;
      }
      break;
    case 1:
      if (input1 == nullptr) {
        BKE_report(reports, RPT_ERROR, "Strips.new_effect: effect takes 1 input strip");
        return nullptr;
      }
      break;
    case 2:
      if (input1 == nullptr || input2 == nullptr) {
        BKE_report(reports, RPT_ERROR, "Strips.new_effect: effect takes 2 input strips");
        return nullptr;
      }
      break;
    default:
      BKE_reportf(
          reports,
          RPT_ERROR,
          "Strips.new_effect: effect expects more than 2 inputs (%d, should never happen!)",
          num_inputs);
      return nullptr;
  }

  blender::seq::LoadData load_data;
  blender::seq::add_load_data_init(&load_data, name, nullptr, frame_start, channel);
  load_data.effect.length = length;
  load_data.effect.type = type;
  load_data.effect.input1 = input1;
  load_data.effect.input2 = input2;
  strip = blender::seq::add_effect_strip(scene, seqbase, &load_data);

  DEG_id_tag_update(&scene->id, ID_RECALC_SEQUENCER_STRIPS);
  WM_main_add_notifier(NC_SCENE | ND_SEQUENCER, scene);

  return strip;
}

static Strip *rna_Strips_editing_new_effect(ID *id,
                                            Editing *ed,
                                            ReportList *reports,
                                            const char *name,
                                            int type,
                                            int channel,
                                            int frame_start,
                                            int length,
                                            Strip *input1,
                                            Strip *input2)
{
  return rna_Strips_new_effect(
      id, &ed->seqbase, reports, name, type, channel, frame_start, length, input1, input2);
}

static Strip *rna_Strips_meta_new_effect(ID *id,
                                         Strip *strip,
                                         ReportList *reports,
                                         const char *name,
                                         int type,
                                         int channel,
                                         int frame_start,
                                         int length,
                                         Strip *input1,
                                         Strip *input2)
{
  return rna_Strips_new_effect(
      id, &strip->seqbase, reports, name, type, channel, frame_start, length, input1, input2);
}

static void rna_Strips_remove(
    ID *id, ListBase *seqbase, Main *bmain, ReportList *reports, PointerRNA *strip_ptr)
{
  Strip *strip = static_cast<Strip *>(strip_ptr->data);
  Scene *scene = (Scene *)id;

  if (BLI_findindex(seqbase, strip) == -1) {
    BKE_reportf(
        reports, RPT_ERROR, "Strip '%s' not in scene '%s'", strip->name + 2, scene->id.name + 2);
    return;
  }

  blender::seq::edit_flag_for_removal(scene, seqbase, strip);
  blender::seq::edit_remove_flagged_strips(scene, seqbase);
  strip_ptr->invalidate();

  DEG_relations_tag_update(bmain);
  DEG_id_tag_update(&scene->id, ID_RECALC_SEQUENCER_STRIPS);
  WM_main_add_notifier(NC_SCENE | ND_SEQUENCER, scene);
}

static void rna_Strips_editing_remove(
    ID *id, Editing *ed, Main *bmain, ReportList *reports, PointerRNA *strip_ptr)
{
  rna_Strips_remove(id, &ed->seqbase, bmain, reports, strip_ptr);
}

static void rna_Strips_meta_remove(
    ID *id, Strip *strip, Main *bmain, ReportList *reports, PointerRNA *strip_ptr)
{
  rna_Strips_remove(id, &strip->seqbase, bmain, reports, strip_ptr);
}

static StripElem *rna_StripElements_append(ID *id, Strip *strip, const char *filename)
{
  Scene *scene = (Scene *)id;
  StripElem *se;

  strip->data->stripdata = se = static_cast<StripElem *>(
      MEM_reallocN(strip->data->stripdata, sizeof(StripElem) * (strip->len + 1)));
  se += strip->len;
  STRNCPY(se->filename, filename);
  strip->len++;

  strip->flag &= ~SEQ_SINGLE_FRAME_CONTENT;

  WM_main_add_notifier(NC_SCENE | ND_SEQUENCER, scene);

  return se;
}

static void rna_StripElements_pop(ID *id, Strip *strip, ReportList *reports, int index)
{
  Scene *scene = (Scene *)id;
  StripElem *new_se, *se;

  if (strip->len == 1) {
    BKE_report(reports, RPT_ERROR, "StripElements.pop: cannot pop the last element");
    return;
  }

  /* python style negative indexing */
  if (index < 0) {
    index += strip->len;
  }

  if (strip->len <= index || index < 0) {
    BKE_report(reports, RPT_ERROR, "StripElements.pop: index out of range");
    return;
  }

  new_se = MEM_calloc_arrayN<StripElem>(size_t(strip->len) - 1, "StripElements_pop");
  strip->len--;

  if (strip->len == 1) {
    strip->flag |= SEQ_SINGLE_FRAME_CONTENT;
  }

  se = strip->data->stripdata;
  if (index > 0) {
    memcpy(new_se, se, sizeof(StripElem) * index);
  }

  if (index < strip->len) {
    memcpy(&new_se[index], &se[index + 1], sizeof(StripElem) * (strip->len - index));
  }

  MEM_freeN(strip->data->stripdata);
  strip->data->stripdata = new_se;

  WM_main_add_notifier(NC_SCENE | ND_SEQUENCER, scene);
}

static void rna_Strip_invalidate_cache_rnafunc(ID *id, Strip *self, int type)
{
  switch (type) {
    case SEQ_CACHE_STORE_RAW:
      blender::seq::relations_invalidate_cache_raw((Scene *)id, self);
      break;
    case SEQ_CACHE_STORE_FINAL_OUT:
      blender::seq::relations_invalidate_cache((Scene *)id, self);
      break;
  }
}

static SeqRetimingKey *rna_Strip_retiming_keys_add(ID *id, Strip *strip, int timeline_frame)
{
  Scene *scene = (Scene *)id;

  SeqRetimingKey *key = blender::seq::retiming_add_key(scene, strip, timeline_frame);

  blender::seq::relations_invalidate_cache_raw(scene, strip);
  WM_main_add_notifier(NC_SCENE | ND_SEQUENCER, nullptr);
  return key;
}

static void rna_Strip_retiming_keys_reset(ID *id, Strip *strip)
{
  Scene *scene = (Scene *)id;

  blender::seq::retiming_data_clear(strip);

  blender::seq::relations_invalidate_cache_raw(scene, strip);
  WM_main_add_notifier(NC_SCENE | ND_SEQUENCER, nullptr);
}

#else

void RNA_api_strip(StructRNA *srna)
{
  FunctionRNA *func;
  PropertyRNA *parm;

  static const EnumPropertyItem strip_cache_type_items[] = {
      {SEQ_CACHE_STORE_RAW, "RAW", 0, "Raw", ""},
      {SEQ_CACHE_STORE_FINAL_OUT, "COMPOSITE", 0, "Composite", ""},
      {0, nullptr, 0, nullptr, nullptr},
  };

  static const EnumPropertyItem strip_split_method_items[] = {
      {blender::seq::SPLIT_SOFT, "SOFT", 0, "Soft", ""},
      {blender::seq::SPLIT_HARD, "HARD", 0, "Hard", ""},
      {0, nullptr, 0, nullptr, nullptr},
  };

  func = RNA_def_function(srna, "strip_elem_from_frame", "rna_Strip_elem_from_frame");
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
      RNA_def_pointer(func, "elem", "StripElement", "", "strip element of the current frame"));

  func = RNA_def_function(srna, "swap", "rna_Strip_swap_internal");
  RNA_def_function_flag(func, FUNC_USE_REPORTS | FUNC_USE_SELF_ID);
  RNA_def_function_flag(func, FUNC_USE_REPORTS);
  parm = RNA_def_pointer(func, "other", "Strip", "Other", "");
  RNA_def_parameter_flags(parm, PROP_NEVER_NULL, PARM_REQUIRED);

  func = RNA_def_function(srna, "move_to_meta", "rna_Strips_move_strip_to_meta");
  RNA_def_function_flag(func, FUNC_USE_REPORTS | FUNC_USE_SELF_ID | FUNC_USE_MAIN);
  parm = RNA_def_pointer(
      func, "meta_sequence", "Strip", "Destination Meta Strip", "Meta to move the strip into");
  RNA_def_parameter_flags(parm, PROP_NEVER_NULL, PARM_REQUIRED);

  func = RNA_def_function(srna, "parent_meta", "rna_Strip_parent_meta");
  RNA_def_function_flag(func, FUNC_USE_SELF_ID);
  RNA_def_function_ui_description(func, "Parent meta");
  /* return type */
  parm = RNA_def_pointer(func, "sequence", "Strip", "", "Parent Meta");
  RNA_def_function_return(func, parm);

  func = RNA_def_function(srna, "invalidate_cache", "rna_Strip_invalidate_cache_rnafunc");
  RNA_def_function_flag(func, FUNC_USE_SELF_ID);
  RNA_def_function_ui_description(func,
                                  "Invalidate cached images for strip and all dependent strips");
  parm = RNA_def_enum(func, "type", strip_cache_type_items, 0, "Type", "Cache Type");
  RNA_def_parameter_flags(parm, PROP_NEVER_NULL, PARM_REQUIRED);

  func = RNA_def_function(srna, "split", "rna_Strip_split");
  RNA_def_function_flag(func, FUNC_USE_REPORTS | FUNC_USE_SELF_ID | FUNC_USE_MAIN);
  RNA_def_function_ui_description(func, "Split Strip");
  parm = RNA_def_int(
      func, "frame", 0, INT_MIN, INT_MAX, "", "Frame where to split the strip", INT_MIN, INT_MAX);
  RNA_def_parameter_flags(parm, PropertyFlag(0), PARM_REQUIRED);
  parm = RNA_def_enum(func, "split_method", strip_split_method_items, 0, "", "");
  RNA_def_parameter_flags(parm, PROP_NEVER_NULL, PARM_REQUIRED);
  parm = RNA_def_boolean(
      func, "ignore_connections", false, "", "Don't propagate split to connected strips");
  /* Return type. */
  parm = RNA_def_pointer(func, "sequence", "Strip", "", "Right side Strip");
  RNA_def_function_return(func, parm);
}

void RNA_api_strip_elements(BlenderRNA *brna, PropertyRNA *cprop)
{
  StructRNA *srna;
  PropertyRNA *parm;
  FunctionRNA *func;

  RNA_def_property_srna(cprop, "StripElements");
  srna = RNA_def_struct(brna, "StripElements", nullptr);
  RNA_def_struct_sdna(srna, "Strip");
  RNA_def_struct_ui_text(srna, "StripElements", "Collection of StripElement");

  func = RNA_def_function(srna, "append", "rna_StripElements_append");
  RNA_def_function_flag(func, FUNC_USE_SELF_ID);
  RNA_def_function_ui_description(func, "Push an image from ImageStrip.directory");
  parm = RNA_def_string(func, "filename", "File", 0, "", "Filepath to image");
  RNA_def_parameter_flags(parm, PropertyFlag(0), PARM_REQUIRED);
  /* return type */
  parm = RNA_def_pointer(func, "elem", "StripElement", "", "New StripElement");
  RNA_def_function_return(func, parm);

  func = RNA_def_function(srna, "pop", "rna_StripElements_pop");
  RNA_def_function_flag(func, FUNC_USE_REPORTS | FUNC_USE_SELF_ID);
  RNA_def_function_ui_description(func, "Pop an image off the collection");
  parm = RNA_def_int(
      func, "index", -1, INT_MIN, INT_MAX, "", "Index of image to remove", INT_MIN, INT_MAX);
  RNA_def_parameter_flags(parm, PropertyFlag(0), PARM_REQUIRED);
}

void RNA_api_strip_retiming_keys(BlenderRNA *brna)
{
  StructRNA *srna = RNA_def_struct(brna, "RetimingKeys", nullptr);
  RNA_def_struct_sdna(srna, "Strip");
  RNA_def_struct_ui_text(srna, "RetimingKeys", "Collection of RetimingKey");

  FunctionRNA *func = RNA_def_function(srna, "add", "rna_Strip_retiming_keys_add");
  RNA_def_function_flag(func, FUNC_USE_SELF_ID);
  RNA_def_int(
      func, "timeline_frame", 0, -MAXFRAME, MAXFRAME, "Timeline Frame", "", -MAXFRAME, MAXFRAME);
  RNA_def_function_ui_description(func, "Add retiming key");
  /* return type */
  PropertyRNA *parm = RNA_def_pointer(func, "retiming_key", "RetimingKey", "", "New RetimingKey");
  RNA_def_function_return(func, parm);

  func = RNA_def_function(srna, "reset", "rna_Strip_retiming_keys_reset");
  RNA_def_function_flag(func, FUNC_USE_SELF_ID);
  RNA_def_function_ui_description(func, "Remove all retiming keys");
}

void RNA_api_strips(StructRNA *srna, const bool metastrip)
{
  PropertyRNA *parm;
  FunctionRNA *func;

  static const EnumPropertyItem strip_effect_items[] = {
      {STRIP_TYPE_CROSS, "CROSS", 0, "Crossfade", "Fade out of one video, fading into another"},
      {STRIP_TYPE_ADD, "ADD", 0, "Add", "Add together color channels from two videos"},
      {STRIP_TYPE_SUB, "SUBTRACT", 0, "Subtract", "Subtract one strip's color from another"},
      {STRIP_TYPE_ALPHAOVER, "ALPHA_OVER", 0, "Alpha Over", "Blend alpha on top of another video"},
      {STRIP_TYPE_ALPHAUNDER, "ALPHA_UNDER", 0, "Alpha Under", "Blend alpha below another video"},
      {STRIP_TYPE_GAMCROSS,
       "GAMMA_CROSS",
       0,
       "Gamma Crossfade",
       "Crossfade with color correction"},
      {STRIP_TYPE_MUL, "MULTIPLY", 0, "Multiply", "Multiply color channels from two videos"},
      {STRIP_TYPE_WIPE, "WIPE", 0, "Wipe", "Sweep a transition line across the frame"},
      {STRIP_TYPE_GLOW, "GLOW", 0, "Glow", "Add blur and brightness to light areas"},
      {STRIP_TYPE_COLOR, "COLOR", 0, "Color", "Add a simple color strip"},
      {STRIP_TYPE_SPEED, "SPEED", 0, "Speed", "Timewarp video strips, modifying playback speed"},
      {STRIP_TYPE_MULTICAM, "MULTICAM", 0, "Multicam Selector", "Control active camera angles"},
      {STRIP_TYPE_ADJUSTMENT, "ADJUSTMENT", 0, "Adjustment Layer", "Apply nondestructive effects"},
      {STRIP_TYPE_GAUSSIAN_BLUR, "GAUSSIAN_BLUR", 0, "Gaussian Blur", "Soften details along axes"},
      {STRIP_TYPE_TEXT, "TEXT", 0, "Text", "Add a simple text strip"},
      {STRIP_TYPE_COLORMIX, "COLORMIX", 0, "Color Mix", "Combine two strips using blend modes"},
      {0, nullptr, 0, nullptr, nullptr},
  };

  const char *new_clip_func_name = "rna_Strips_editing_new_clip";
  const char *new_mask_func_name = "rna_Strips_editing_new_mask";
  const char *new_scene_func_name = "rna_Strips_editing_new_scene";
  const char *new_image_func_name = "rna_Strips_editing_new_image";
  const char *new_movie_func_name = "rna_Strips_editing_new_movie";
  const char *new_sound_func_name = "rna_Strips_editing_new_sound";
  const char *new_meta_func_name = "rna_Strips_editing_new_meta";
  const char *new_effect_func_name = "rna_Strips_editing_new_effect";
  const char *remove_func_name = "rna_Strips_editing_remove";

  if (metastrip) {
    new_clip_func_name = "rna_Strips_meta_new_clip";
    new_mask_func_name = "rna_Strips_meta_new_mask";
    new_scene_func_name = "rna_Strips_meta_new_scene";
    new_image_func_name = "rna_Strips_meta_new_image";
    new_movie_func_name = "rna_Strips_meta_new_movie";
    new_sound_func_name = "rna_Strips_meta_new_sound";
    new_meta_func_name = "rna_Strips_meta_new_meta";
    new_effect_func_name = "rna_Strips_meta_new_effect";
    remove_func_name = "rna_Strips_meta_remove";
  }

  func = RNA_def_function(srna, "new_clip", new_clip_func_name);
  RNA_def_function_flag(func, FUNC_USE_SELF_ID | FUNC_USE_MAIN);
  RNA_def_function_ui_description(func, "Add a new movie clip strip");
  parm = RNA_def_string(func, "name", "Name", 0, "", "Name for the new strip");
  RNA_def_parameter_flags(parm, PropertyFlag(0), PARM_REQUIRED);
  parm = RNA_def_pointer(func, "clip", "MovieClip", "", "Movie clip to add");
  RNA_def_parameter_flags(parm, PROP_NEVER_NULL, PARM_REQUIRED);
  parm = RNA_def_int(func,
                     "channel",
                     0,
                     1,
                     blender::seq::MAX_CHANNELS,
                     "Channel",
                     "The channel for the new strip",
                     1,
                     blender::seq::MAX_CHANNELS);
  RNA_def_parameter_flags(parm, PropertyFlag(0), PARM_REQUIRED);
  parm = RNA_def_int(func,
                     "frame_start",
                     0,
                     -MAXFRAME,
                     MAXFRAME,
                     "",
                     "The start frame for the new strip",
                     -MAXFRAME,
                     MAXFRAME);
  RNA_def_parameter_flags(parm, PropertyFlag(0), PARM_REQUIRED);
  /* return type */
  parm = RNA_def_pointer(func, "sequence", "Strip", "", "New Strip");
  RNA_def_function_return(func, parm);

  func = RNA_def_function(srna, "new_mask", new_mask_func_name);
  RNA_def_function_flag(func, FUNC_USE_SELF_ID | FUNC_USE_MAIN);
  RNA_def_function_ui_description(func, "Add a new mask strip");
  parm = RNA_def_string(func, "name", "Name", 0, "", "Name for the new strip");
  RNA_def_parameter_flags(parm, PropertyFlag(0), PARM_REQUIRED);
  parm = RNA_def_pointer(func, "mask", "Mask", "", "Mask to add");
  RNA_def_parameter_flags(parm, PROP_NEVER_NULL, PARM_REQUIRED);
  parm = RNA_def_int(func,
                     "channel",
                     0,
                     1,
                     blender::seq::MAX_CHANNELS,
                     "Channel",
                     "The channel for the new strip",
                     1,
                     blender::seq::MAX_CHANNELS);
  RNA_def_parameter_flags(parm, PropertyFlag(0), PARM_REQUIRED);
  parm = RNA_def_int(func,
                     "frame_start",
                     0,
                     -MAXFRAME,
                     MAXFRAME,
                     "",
                     "The start frame for the new strip",
                     -MAXFRAME,
                     MAXFRAME);
  RNA_def_parameter_flags(parm, PropertyFlag(0), PARM_REQUIRED);
  /* return type */
  parm = RNA_def_pointer(func, "sequence", "Strip", "", "New Strip");
  RNA_def_function_return(func, parm);

  func = RNA_def_function(srna, "new_scene", new_scene_func_name);
  RNA_def_function_flag(func, FUNC_USE_SELF_ID | FUNC_USE_MAIN);
  RNA_def_function_ui_description(func, "Add a new scene strip");
  parm = RNA_def_string(func, "name", "Name", 0, "", "Name for the new strip");
  RNA_def_parameter_flags(parm, PropertyFlag(0), PARM_REQUIRED);
  parm = RNA_def_pointer(func, "scene", "Scene", "", "Scene to add");
  RNA_def_parameter_flags(parm, PROP_NEVER_NULL, PARM_REQUIRED);
  parm = RNA_def_int(func,
                     "channel",
                     0,
                     1,
                     blender::seq::MAX_CHANNELS,
                     "Channel",
                     "The channel for the new strip",
                     1,
                     blender::seq::MAX_CHANNELS);
  RNA_def_parameter_flags(parm, PropertyFlag(0), PARM_REQUIRED);
  parm = RNA_def_int(func,
                     "frame_start",
                     0,
                     -MAXFRAME,
                     MAXFRAME,
                     "",
                     "The start frame for the new strip",
                     -MAXFRAME,
                     MAXFRAME);
  RNA_def_parameter_flags(parm, PropertyFlag(0), PARM_REQUIRED);
  /* return type */
  parm = RNA_def_pointer(func, "sequence", "Strip", "", "New Strip");
  RNA_def_function_return(func, parm);

  func = RNA_def_function(srna, "new_image", new_image_func_name);
  RNA_def_function_flag(func, FUNC_USE_REPORTS | FUNC_USE_SELF_ID | FUNC_USE_MAIN);
  RNA_def_function_ui_description(func, "Add a new image strip");
  parm = RNA_def_string(func, "name", "Name", 0, "", "Name for the new strip");
  RNA_def_parameter_flags(parm, PropertyFlag(0), PARM_REQUIRED);
  parm = RNA_def_string(func, "filepath", "File", 0, "", "Filepath to image");
  RNA_def_parameter_flags(parm, PropertyFlag(0), PARM_REQUIRED);
  parm = RNA_def_int(func,
                     "channel",
                     0,
                     1,
                     blender::seq::MAX_CHANNELS,
                     "Channel",
                     "The channel for the new strip",
                     1,
                     blender::seq::MAX_CHANNELS);
  RNA_def_parameter_flags(parm, PropertyFlag(0), PARM_REQUIRED);
  parm = RNA_def_int(func,
                     "frame_start",
                     0,
                     -MAXFRAME,
                     MAXFRAME,
                     "",
                     "The start frame for the new strip",
                     -MAXFRAME,
                     MAXFRAME);
  RNA_def_parameter_flags(parm, PropertyFlag(0), PARM_REQUIRED);
  parm = RNA_def_enum(func,
                      "fit_method",
                      rna_enum_strip_scale_method_items,
                      SEQ_USE_ORIGINAL_SIZE,
                      "Image Fit Method",
                      nullptr);
  /* return type */
  parm = RNA_def_pointer(func, "sequence", "Strip", "", "New Strip");
  RNA_def_function_return(func, parm);

  func = RNA_def_function(srna, "new_movie", new_movie_func_name);
  RNA_def_function_flag(func, FUNC_USE_REPORTS | FUNC_USE_SELF_ID | FUNC_USE_MAIN);
  RNA_def_function_ui_description(func, "Add a new movie strip");
  parm = RNA_def_string(func, "name", "Name", 0, "", "Name for the new strip");
  RNA_def_parameter_flags(parm, PropertyFlag(0), PARM_REQUIRED);
  parm = RNA_def_string(func, "filepath", "File", 0, "", "Filepath to movie");
  RNA_def_parameter_flags(parm, PropertyFlag(0), PARM_REQUIRED);
  parm = RNA_def_int(func,
                     "channel",
                     0,
                     1,
                     blender::seq::MAX_CHANNELS,
                     "Channel",
                     "The channel for the new strip",
                     1,
                     blender::seq::MAX_CHANNELS);
  RNA_def_parameter_flags(parm, PropertyFlag(0), PARM_REQUIRED);
  parm = RNA_def_int(func,
                     "frame_start",
                     0,
                     -MAXFRAME,
                     MAXFRAME,
                     "",
                     "The start frame for the new strip",
                     -MAXFRAME,
                     MAXFRAME);
  RNA_def_parameter_flags(parm, PropertyFlag(0), PARM_REQUIRED);
  parm = RNA_def_enum(func,
                      "fit_method",
                      rna_enum_strip_scale_method_items,
                      SEQ_USE_ORIGINAL_SIZE,
                      "Image Fit Method",
                      nullptr);
  /* return type */
  parm = RNA_def_pointer(func, "sequence", "Strip", "", "New Strip");
  RNA_def_function_return(func, parm);

  func = RNA_def_function(srna, "new_sound", new_sound_func_name);
  RNA_def_function_flag(func, FUNC_USE_REPORTS | FUNC_USE_SELF_ID | FUNC_USE_MAIN);
  RNA_def_function_ui_description(func, "Add a new sound strip");
  parm = RNA_def_string(func, "name", "Name", 0, "", "Name for the new strip");
  RNA_def_parameter_flags(parm, PropertyFlag(0), PARM_REQUIRED);
  parm = RNA_def_string(func, "filepath", "File", 0, "", "Filepath to movie");
  RNA_def_parameter_flags(parm, PropertyFlag(0), PARM_REQUIRED);
  parm = RNA_def_int(func,
                     "channel",
                     0,
                     1,
                     blender::seq::MAX_CHANNELS,
                     "Channel",
                     "The channel for the new strip",
                     1,
                     blender::seq::MAX_CHANNELS);
  RNA_def_parameter_flags(parm, PropertyFlag(0), PARM_REQUIRED);
  parm = RNA_def_int(func,
                     "frame_start",
                     0,
                     -MAXFRAME,
                     MAXFRAME,
                     "",
                     "The start frame for the new strip",
                     -MAXFRAME,
                     MAXFRAME);
  RNA_def_parameter_flags(parm, PropertyFlag(0), PARM_REQUIRED);
  /* return type */
  parm = RNA_def_pointer(func, "sequence", "Strip", "", "New Strip");
  RNA_def_function_return(func, parm);

  func = RNA_def_function(srna, "new_meta", new_meta_func_name);
  RNA_def_function_flag(func, FUNC_USE_SELF_ID);
  RNA_def_function_ui_description(func, "Add a new meta strip");
  parm = RNA_def_string(func, "name", "Name", 0, "", "Name for the new strip");
  RNA_def_parameter_flags(parm, PropertyFlag(0), PARM_REQUIRED);
  parm = RNA_def_int(func,
                     "channel",
                     0,
                     1,
                     blender::seq::MAX_CHANNELS,
                     "Channel",
                     "The channel for the new strip",
                     1,
                     blender::seq::MAX_CHANNELS);
  RNA_def_parameter_flags(parm, PropertyFlag(0), PARM_REQUIRED);
  parm = RNA_def_int(func,
                     "frame_start",
                     0,
                     -MAXFRAME,
                     MAXFRAME,
                     "",
                     "The start frame for the new strip",
                     -MAXFRAME,
                     MAXFRAME);
  RNA_def_parameter_flags(parm, PropertyFlag(0), PARM_REQUIRED);
  /* return type */
  parm = RNA_def_pointer(func, "sequence", "Strip", "", "New Strip");
  RNA_def_function_return(func, parm);

  func = RNA_def_function(srna, "new_effect", new_effect_func_name);
  RNA_def_function_flag(func, FUNC_USE_REPORTS | FUNC_USE_SELF_ID);
  RNA_def_function_ui_description(func, "Add a new effect strip");
  parm = RNA_def_string(func, "name", "Name", 0, "", "Name for the new strip");
  RNA_def_parameter_flags(parm, PropertyFlag(0), PARM_REQUIRED);
  parm = RNA_def_enum(func, "type", strip_effect_items, 0, "Type", "type for the new strip");
  RNA_def_parameter_flags(parm, PropertyFlag(0), PARM_REQUIRED);
  parm = RNA_def_int(func,
                     "channel",
                     0,
                     1,
                     blender::seq::MAX_CHANNELS,
                     "Channel",
                     "The channel for the new strip",
                     1,
                     blender::seq::MAX_CHANNELS);
  /* don't use MAXFRAME since it makes importer scripts fail */
  RNA_def_parameter_flags(parm, PropertyFlag(0), PARM_REQUIRED);
  parm = RNA_def_int(func,
                     "frame_start",
                     0,
                     INT_MIN,
                     INT_MAX,
                     "",
                     "The start frame for the new strip",
                     INT_MIN,
                     INT_MAX);
  RNA_def_parameter_flags(parm, PropertyFlag(0), PARM_REQUIRED);
  RNA_def_int(func,
              "length",
              0,
              INT_MIN,
              INT_MAX,
              "",
              "Length of the strip in frames, or the length of each strip if multiple are added",
              INT_MIN,
              INT_MAX);
  RNA_def_pointer(func, "input1", "Strip", "", "First input strip for effect");
  RNA_def_pointer(func, "input2", "Strip", "", "Second input strip for effect");
  /* return type */
  parm = RNA_def_pointer(func, "sequence", "Strip", "", "New Strip");
  RNA_def_function_return(func, parm);

  func = RNA_def_function(srna, "remove", remove_func_name);
  RNA_def_function_flag(func, FUNC_USE_SELF_ID | FUNC_USE_REPORTS | FUNC_USE_MAIN);
  RNA_def_function_ui_description(func, "Remove a Strip");
  parm = RNA_def_pointer(func, "sequence", "Strip", "", "Strip to remove");
  RNA_def_parameter_flags(parm, PROP_NEVER_NULL, PARM_REQUIRED | PARM_RNAPTR);
  RNA_def_parameter_clear_flags(parm, PROP_THICK_WRAP, ParameterFlag(0));
}

#endif
