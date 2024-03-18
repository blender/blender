/* SPDX-FileCopyrightText: 2023 Blender Authors
 *
 * SPDX-License-Identifier: GPL-2.0-or-later */

/** \file
 * \ingroup RNA
 */

#include <climits>
#include <cstdlib>

#include "DNA_anim_types.h"
#include "DNA_movieclip_types.h"
#include "DNA_object_types.h"
#include "DNA_scene_types.h"
#include "DNA_sequence_types.h"
#include "DNA_vfont_types.h"

#include "BLI_iterator.h"
#include "BLI_listbase.h"
#include "BLI_math_rotation.h"
#include "BLI_string_utf8_symbols.h"
#include "BLI_string_utils.hh"

#include "BLT_translation.h"

#include "BKE_anim_data.h"
#include "BKE_animsys.h"
#include "BKE_sound.h"

#include "IMB_metadata.hh"

#include "MEM_guardedalloc.h"

#include "RNA_access.hh"
#include "RNA_define.hh"
#include "RNA_enum_types.hh"

#include "rna_internal.hh"

#include "SEQ_add.hh"
#include "SEQ_channels.hh"
#include "SEQ_effects.hh"
#include "SEQ_iterator.hh"
#include "SEQ_modifier.hh"
#include "SEQ_prefetch.hh"
#include "SEQ_proxy.hh"
#include "SEQ_relations.hh"
#include "SEQ_retiming.hh"
#include "SEQ_select.hh"
#include "SEQ_sequencer.hh"
#include "SEQ_sound.hh"
#include "SEQ_time.hh"
#include "SEQ_transform.hh"
#include "SEQ_utils.hh"

#include "WM_types.hh"

struct EffectInfo {
  const char *struct_name;
  const char *ui_name;
  const char *ui_desc;
  void (*func)(StructRNA *);
  int inputs;
};

/* These wrap strangely, disable formatting for fixed indentation and wrapping. */
/* clang-format off */
#define RNA_ENUM_SEQUENCER_VIDEO_MODIFIER_TYPE_ITEMS \
  {seqModifierType_BrightContrast, "BRIGHT_CONTRAST", ICON_NONE, "Brightness/Contrast", ""}, \
  {seqModifierType_ColorBalance, "COLOR_BALANCE", ICON_NONE, "Color Balance", ""}, \
  {seqModifierType_Curves, "CURVES", ICON_NONE, "Curves", ""}, \
  {seqModifierType_HueCorrect, "HUE_CORRECT", ICON_NONE, "Hue Correct", ""}, \
  {seqModifierType_Mask, "MASK", ICON_NONE, "Mask", ""}, \
  {seqModifierType_Tonemap, "TONEMAP", ICON_NONE, "Tone Map", ""}, \
  {seqModifierType_WhiteBalance, "WHITE_BALANCE", ICON_NONE, "White Balance", ""}

#define RNA_ENUM_SEQUENCER_AUDIO_MODIFIER_TYPE_ITEMS \
  {seqModifierType_SoundEqualizer, "SOUND_EQUALIZER", ICON_NONE, "Sound Equalizer", ""}
/* clang-format on */

const EnumPropertyItem rna_enum_sequence_modifier_type_items[] = {
    RNA_ENUM_SEQUENCER_VIDEO_MODIFIER_TYPE_ITEMS,
    RNA_ENUM_SEQUENCER_AUDIO_MODIFIER_TYPE_ITEMS,
    {0, nullptr, 0, nullptr, nullptr},
};

const EnumPropertyItem rna_enum_sequence_video_modifier_type_items[] = {
    RNA_ENUM_SEQUENCER_VIDEO_MODIFIER_TYPE_ITEMS,
    {0, nullptr, 0, nullptr, nullptr},
};

const EnumPropertyItem rna_enum_sequence_sound_modifier_type_items[] = {
    RNA_ENUM_SEQUENCER_AUDIO_MODIFIER_TYPE_ITEMS,
    {0, nullptr, 0, nullptr, nullptr},
};

const EnumPropertyItem rna_enum_strip_color_items[] = {
    {SEQUENCE_COLOR_NONE, "NONE", ICON_X, "None", "Assign no color tag to the collection"},
    {SEQUENCE_COLOR_01, "COLOR_01", ICON_SEQUENCE_COLOR_01, "Color 01", ""},
    {SEQUENCE_COLOR_02, "COLOR_02", ICON_SEQUENCE_COLOR_02, "Color 02", ""},
    {SEQUENCE_COLOR_03, "COLOR_03", ICON_SEQUENCE_COLOR_03, "Color 03", ""},
    {SEQUENCE_COLOR_04, "COLOR_04", ICON_SEQUENCE_COLOR_04, "Color 04", ""},
    {SEQUENCE_COLOR_05, "COLOR_05", ICON_SEQUENCE_COLOR_05, "Color 05", ""},
    {SEQUENCE_COLOR_06, "COLOR_06", ICON_SEQUENCE_COLOR_06, "Color 06", ""},
    {SEQUENCE_COLOR_07, "COLOR_07", ICON_SEQUENCE_COLOR_07, "Color 07", ""},
    {SEQUENCE_COLOR_08, "COLOR_08", ICON_SEQUENCE_COLOR_08, "Color 08", ""},
    {SEQUENCE_COLOR_09, "COLOR_09", ICON_SEQUENCE_COLOR_09, "Color 09", ""},
    {0, nullptr, 0, nullptr, nullptr},
};

#ifdef RNA_RUNTIME

#  include <algorithm>

#  include <fmt/format.h>

#  include "BKE_global.h"
#  include "BKE_idprop.h"
#  include "BKE_movieclip.h"
#  include "BKE_report.h"

#  include "WM_api.hh"

#  include "DEG_depsgraph.hh"
#  include "DEG_depsgraph_build.hh"

#  include "IMB_imbuf.hh"

#  include "SEQ_edit.hh"

struct SequenceSearchData {
  Sequence *seq;
  void *data;
  SequenceModifierData *smd;
};

static void rna_SequenceElement_update(Main * /*bmain*/, Scene * /*scene*/, PointerRNA *ptr)
{
  Scene *scene = (Scene *)ptr->owner_id;
  Editing *ed = SEQ_editing_get(scene);

  if (ed) {
    StripElem *se = (StripElem *)ptr->data;
    Sequence *seq;

    /* slow but we can't avoid! */
    seq = SEQ_sequence_from_strip_elem(&ed->seqbase, se);
    if (seq) {
      SEQ_relations_invalidate_cache_raw(scene, seq);
    }
  }
}

static void rna_Sequence_invalidate_raw_update(Main * /*bmain*/,
                                               Scene * /*scene*/,
                                               PointerRNA *ptr)
{
  Scene *scene = (Scene *)ptr->owner_id;
  Editing *ed = SEQ_editing_get(scene);

  if (ed) {
    Sequence *seq = (Sequence *)ptr->data;

    SEQ_relations_invalidate_cache_raw(scene, seq);
  }
}

static void rna_Sequence_invalidate_preprocessed_update(Main * /*bmain*/,
                                                        Scene * /*scene*/,
                                                        PointerRNA *ptr)
{
  Scene *scene = (Scene *)ptr->owner_id;
  Editing *ed = SEQ_editing_get(scene);

  if (ed) {
    Sequence *seq = (Sequence *)ptr->data;

    SEQ_relations_invalidate_cache_preprocessed(scene, seq);
  }
}

static void UNUSED_FUNCTION(rna_Sequence_invalidate_composite_update)(Main * /*bmain*/,
                                                                      Scene * /*scene*/,
                                                                      PointerRNA *ptr)
{
  Scene *scene = (Scene *)ptr->owner_id;
  Editing *ed = SEQ_editing_get(scene);

  if (ed) {
    Sequence *seq = (Sequence *)ptr->data;

    SEQ_relations_invalidate_cache_composite(scene, seq);
  }
}

static void rna_Sequence_scene_switch_update(Main *bmain, Scene *scene, PointerRNA *ptr)
{
  rna_Sequence_invalidate_raw_update(bmain, scene, ptr);
  DEG_id_tag_update(&scene->id, ID_RECALC_AUDIO | ID_RECALC_SEQUENCER_STRIPS);
  DEG_relations_tag_update(bmain);
}

static void rna_Sequence_use_sequence(Main *bmain, Scene *scene, PointerRNA *ptr)
{
  /* General update callback. */
  rna_Sequence_invalidate_raw_update(bmain, scene, ptr);
  /* Changing recursion changes set of IDs which needs to be remapped by the copy-on-write.
   * the only way for this currently is to tag the ID for ID_RECALC_COPY_ON_WRITE. */
  Editing *ed = SEQ_editing_get(scene);
  if (ed) {
    Sequence *seq = (Sequence *)ptr->data;
    if (seq->scene != nullptr) {
      DEG_id_tag_update(&seq->scene->id, ID_RECALC_COPY_ON_WRITE);
    }
  }
  /* The sequencer scene is to be updated as well, including new relations from the nested
   * sequencer. */
  DEG_id_tag_update(&scene->id, ID_RECALC_SEQUENCER_STRIPS);
  DEG_relations_tag_update(bmain);
}

static void add_strips_from_seqbase(const ListBase *seqbase, blender::Vector<Sequence *> &strips)
{
  LISTBASE_FOREACH (Sequence *, seq, seqbase) {
    strips.append(seq);

    if (seq->type == SEQ_TYPE_META) {
      add_strips_from_seqbase(&seq->seqbase, strips);
    }
  }
}

struct SequencesAllIterator {
  blender::Vector<Sequence *> strips;
  int index;
};

static void rna_SequenceEditor_sequences_all_begin(CollectionPropertyIterator *iter,
                                                   PointerRNA *ptr)
{
  Scene *scene = (Scene *)ptr->owner_id;
  Editing *ed = SEQ_editing_get(scene);

  SequencesAllIterator *seq_iter = MEM_new<SequencesAllIterator>(__func__);
  seq_iter->index = 0;
  add_strips_from_seqbase(&ed->seqbase, seq_iter->strips);

  BLI_Iterator *bli_iter = static_cast<BLI_Iterator *>(
      MEM_callocN(sizeof(BLI_Iterator), __func__));
  iter->internal.custom = bli_iter;
  bli_iter->data = seq_iter;

  Sequence **seq_arr = seq_iter->strips.begin();
  bli_iter->current = *seq_arr;
  iter->valid = bli_iter->current != nullptr;
}

static void rna_SequenceEditor_sequences_all_next(CollectionPropertyIterator *iter)
{
  BLI_Iterator *bli_iter = static_cast<BLI_Iterator *>(iter->internal.custom);
  SequencesAllIterator *seq_iter = static_cast<SequencesAllIterator *>(bli_iter->data);

  seq_iter->index++;
  Sequence **seq_arr = seq_iter->strips.begin();
  bli_iter->current = *(seq_arr + seq_iter->index);

  iter->valid = bli_iter->current != nullptr && seq_iter->index < seq_iter->strips.size();
}

static PointerRNA rna_SequenceEditor_sequences_all_get(CollectionPropertyIterator *iter)
{
  Sequence *seq = static_cast<Sequence *>(((BLI_Iterator *)iter->internal.custom)->current);
  return rna_pointer_inherit_refine(&iter->parent, &RNA_Sequence, seq);
}

static void rna_SequenceEditor_sequences_all_end(CollectionPropertyIterator *iter)
{
  BLI_Iterator *bli_iter = static_cast<BLI_Iterator *>(iter->internal.custom);
  SequencesAllIterator *seq_iter = static_cast<SequencesAllIterator *>(bli_iter->data);

  MEM_delete(seq_iter);
  MEM_freeN(bli_iter);
}

static int rna_SequenceEditor_sequences_all_lookup_string(PointerRNA *ptr,
                                                          const char *key,
                                                          PointerRNA *r_ptr)
{
  ID *id = ptr->owner_id;
  Scene *scene = (Scene *)id;

  Sequence *seq = SEQ_sequence_lookup_seq_by_name(scene, key);
  if (seq) {
    *r_ptr = RNA_pointer_create(ptr->owner_id, &RNA_Sequence, seq);
    return true;
  }
  return false;
}

static void rna_SequenceEditor_update_cache(Main * /*bmain*/, Scene *scene, PointerRNA * /*ptr*/)
{
  Editing *ed = scene->ed;

  SEQ_relations_free_imbuf(scene, &ed->seqbase, false);
  SEQ_cache_cleanup(scene);
}

/* internal use */
static int rna_SequenceEditor_elements_length(PointerRNA *ptr)
{
  Sequence *seq = (Sequence *)ptr->data;

  /* Hack? copied from `sequencer.cc`, #reload_sequence_new_file(). */
  size_t olen = MEM_allocN_len(seq->strip->stripdata) / sizeof(StripElem);

  /* The problem with `seq->strip->len` and `seq->len` is that it's discounted from the offset
   * (hard cut trim). */
  return int(olen);
}

static void rna_Sequence_elements_begin(CollectionPropertyIterator *iter, PointerRNA *ptr)
{
  Sequence *seq = (Sequence *)ptr->data;
  rna_iterator_array_begin(iter,
                           (void *)seq->strip->stripdata,
                           sizeof(StripElem),
                           rna_SequenceEditor_elements_length(ptr),
                           0,
                           nullptr);
}

static int rna_Sequence_retiming_keys_length(PointerRNA *ptr)
{
  return SEQ_retiming_keys_count((Sequence *)ptr->data);
}

static void rna_SequenceEditor_retiming_keys_begin(CollectionPropertyIterator *iter,
                                                   PointerRNA *ptr)
{
  Sequence *seq = (Sequence *)ptr->data;
  rna_iterator_array_begin(iter,
                           (void *)seq->retiming_keys,
                           sizeof(SeqRetimingKey),
                           SEQ_retiming_keys_count(seq),
                           0,
                           nullptr);
}

static Sequence *strip_by_key_find(Scene *scene, SeqRetimingKey *key)
{
  Editing *ed = SEQ_editing_get(scene);
  blender::VectorSet strips = SEQ_query_all_strips_recursive(&ed->seqbase);

  for (Sequence *seq : strips) {
    const int retiming_keys_count = SEQ_retiming_keys_count(seq);
    SeqRetimingKey *first = seq->retiming_keys;
    SeqRetimingKey *last = seq->retiming_keys + retiming_keys_count - 1;

    if (key >= first && key <= last) {
      return seq;
    }
  }

  return nullptr;
}

static void rna_Sequence_retiming_key_remove(ID *id, SeqRetimingKey *key)
{
  Scene *scene = (Scene *)id;
  Sequence *seq = strip_by_key_find(scene, key);

  if (seq == nullptr) {
    return;
  }

  SEQ_retiming_remove_key(scene, seq, key);

  SEQ_relations_invalidate_cache_raw(scene, seq);
  WM_main_add_notifier(NC_SCENE | ND_SEQUENCER, nullptr);
}

static int rna_Sequence_retiming_key_frame_get(PointerRNA *ptr)
{
  SeqRetimingKey *key = (SeqRetimingKey *)ptr->data;
  Scene *scene = (Scene *)ptr->owner_id;
  Sequence *seq = strip_by_key_find(scene, key);

  if (seq == nullptr) {
    return 0;
  }

  return SEQ_time_start_frame_get(seq) + key->strip_frame_index;
}

static void rna_Sequence_retiming_key_frame_set(PointerRNA *ptr, int value)
{
  SeqRetimingKey *key = (SeqRetimingKey *)ptr->data;
  Scene *scene = (Scene *)ptr->owner_id;
  Sequence *seq = strip_by_key_find(scene, key);

  if (seq == nullptr) {
    return;
  }

  SEQ_retiming_key_timeline_frame_set(scene, seq, key, value);
  SEQ_relations_invalidate_cache_raw(scene, seq);
}

static bool rna_SequenceEditor_selected_retiming_key_get(PointerRNA *ptr)
{
  Scene *scene = (Scene *)ptr->owner_id;
  return SEQ_retiming_selection_get(SEQ_editing_get(scene)).size() != 0;
}

static void rna_Sequence_views_format_update(Main *bmain, Scene *scene, PointerRNA *ptr)
{
  rna_Sequence_invalidate_raw_update(bmain, scene, ptr);
}

static void do_sequence_frame_change_update(Scene *scene, Sequence *seq)
{
  ListBase *seqbase = SEQ_get_seqbase_by_seq(scene, seq);

  if (SEQ_transform_test_overlap(scene, seqbase, seq)) {
    SEQ_transform_seqbase_shuffle(seqbase, seq, scene);
  }

  if (seq->type == SEQ_TYPE_SOUND_RAM) {
    DEG_id_tag_update(&scene->id, ID_RECALC_SEQUENCER_STRIPS);
  }
}

/* A simple wrapper around above func, directly usable as prop update func.
 * Also invalidate cache if needed.
 */
static void rna_Sequence_frame_change_update(Main * /*bmain*/, Scene * /*scene*/, PointerRNA *ptr)
{
  Scene *scene = (Scene *)ptr->owner_id;
  do_sequence_frame_change_update(scene, (Sequence *)ptr->data);
}

static int rna_Sequence_frame_final_start_get(PointerRNA *ptr)
{
  Scene *scene = (Scene *)ptr->owner_id;
  return SEQ_time_left_handle_frame_get(scene, (Sequence *)ptr->data);
}

static int rna_Sequence_frame_final_end_get(PointerRNA *ptr)
{
  Scene *scene = (Scene *)ptr->owner_id;
  return SEQ_time_right_handle_frame_get(scene, (Sequence *)ptr->data);
}

static void rna_Sequence_start_frame_final_set(PointerRNA *ptr, int value)
{
  Sequence *seq = (Sequence *)ptr->data;
  Scene *scene = (Scene *)ptr->owner_id;

  SEQ_time_left_handle_frame_set(scene, seq, value);
  do_sequence_frame_change_update(scene, seq);
  SEQ_relations_invalidate_cache_composite(scene, seq);
}

static void rna_Sequence_end_frame_final_set(PointerRNA *ptr, int value)
{
  Sequence *seq = (Sequence *)ptr->data;
  Scene *scene = (Scene *)ptr->owner_id;

  SEQ_time_right_handle_frame_set(scene, seq, value);
  do_sequence_frame_change_update(scene, seq);
  SEQ_relations_invalidate_cache_composite(scene, seq);
}

static void rna_Sequence_start_frame_set(PointerRNA *ptr, float value)
{
  Sequence *seq = (Sequence *)ptr->data;
  Scene *scene = (Scene *)ptr->owner_id;

  SEQ_transform_translate_sequence(scene, seq, value - seq->start);
  do_sequence_frame_change_update(scene, seq);
  SEQ_relations_invalidate_cache_composite(scene, seq);
}

static void rna_Sequence_frame_offset_start_set(PointerRNA *ptr, float value)
{
  Sequence *seq = (Sequence *)ptr->data;
  Scene *scene = (Scene *)ptr->owner_id;

  SEQ_relations_invalidate_cache_composite(scene, seq);
  seq->startofs = value;
}

static void rna_Sequence_frame_offset_end_set(PointerRNA *ptr, float value)
{
  Sequence *seq = (Sequence *)ptr->data;
  Scene *scene = (Scene *)ptr->owner_id;

  SEQ_relations_invalidate_cache_composite(scene, seq);
  seq->endofs = value;
}

static void rna_Sequence_anim_startofs_final_set(PointerRNA *ptr, int value)
{
  Sequence *seq = (Sequence *)ptr->data;
  Scene *scene = (Scene *)ptr->owner_id;

  seq->anim_startofs = std::min(value, seq->len + seq->anim_startofs);

  SEQ_add_reload_new_file(G.main, scene, seq, false);
  do_sequence_frame_change_update(scene, seq);
}

static void rna_Sequence_anim_endofs_final_set(PointerRNA *ptr, int value)
{
  Sequence *seq = (Sequence *)ptr->data;
  Scene *scene = (Scene *)ptr->owner_id;

  seq->anim_endofs = std::min(value, seq->len + seq->anim_endofs);

  SEQ_add_reload_new_file(G.main, scene, seq, false);
  do_sequence_frame_change_update(scene, seq);
}

static void rna_Sequence_anim_endofs_final_range(
    PointerRNA *ptr, int *min, int *max, int * /*softmin*/, int * /*softmax*/)
{
  Sequence *seq = (Sequence *)ptr->data;

  *min = 0;
  *max = seq->len + seq->anim_endofs - seq->startofs - seq->endofs - 1;
}

static void rna_Sequence_anim_startofs_final_range(
    PointerRNA *ptr, int *min, int *max, int * /*softmin*/, int * /*softmax*/)
{
  Sequence *seq = (Sequence *)ptr->data;

  *min = 0;
  *max = seq->len + seq->anim_startofs - seq->startofs - seq->endofs - 1;
}

static void rna_Sequence_frame_offset_start_range(
    PointerRNA *ptr, float *min, float *max, float * /*softmin*/, float * /*softmax*/)
{
  Sequence *seq = (Sequence *)ptr->data;
  *min = (seq->type == SEQ_TYPE_SOUND_RAM) ? 0 : INT_MIN;
  *max = seq->len - seq->endofs - 1;
}

static void rna_Sequence_frame_offset_end_range(
    PointerRNA *ptr, float *min, float *max, float * /*softmin*/, float * /*softmax*/)
{
  Sequence *seq = (Sequence *)ptr->data;
  *min = (seq->type == SEQ_TYPE_SOUND_RAM) ? 0 : INT_MIN;
  *max = seq->len - seq->startofs - 1;
}

static void rna_Sequence_frame_length_set(PointerRNA *ptr, int value)
{
  Sequence *seq = (Sequence *)ptr->data;
  Scene *scene = (Scene *)ptr->owner_id;

  SEQ_time_right_handle_frame_set(scene, seq, SEQ_time_left_handle_frame_get(scene, seq) + value);
  do_sequence_frame_change_update(scene, seq);
  SEQ_relations_invalidate_cache_composite(scene, seq);
}

static int rna_Sequence_frame_length_get(PointerRNA *ptr)
{
  Sequence *seq = (Sequence *)ptr->data;
  Scene *scene = (Scene *)ptr->owner_id;
  return SEQ_time_right_handle_frame_get(scene, seq) - SEQ_time_left_handle_frame_get(scene, seq);
}

static int rna_Sequence_frame_editable(const PointerRNA *ptr, const char ** /*r_info*/)
{
  Sequence *seq = (Sequence *)ptr->data;
  /* Effect sequences' start frame and length must be readonly! */
  return (SEQ_effect_get_num_inputs(seq->type)) ? PropertyFlag(0) : PROP_EDITABLE;
}

static void rna_Sequence_channel_set(PointerRNA *ptr, int value)
{
  Sequence *seq = (Sequence *)ptr->data;
  Scene *scene = (Scene *)ptr->owner_id;
  ListBase *seqbase = SEQ_get_seqbase_by_seq(scene, seq);

  /* check channel increment or decrement */
  const int channel_delta = (value >= seq->machine) ? 1 : -1;
  seq->machine = value;

  if (SEQ_transform_test_overlap(scene, seqbase, seq)) {
    SEQ_transform_seqbase_shuffle_ex(seqbase, seq, scene, channel_delta);
  }
  SEQ_relations_invalidate_cache_composite(scene, seq);
}

static void rna_Sequence_use_proxy_set(PointerRNA *ptr, bool value)
{
  Sequence *seq = (Sequence *)ptr->data;
  SEQ_proxy_set(seq, value != 0);
}

static bool transform_seq_cmp_fn(Sequence *seq, void *arg_pt)
{
  SequenceSearchData *data = static_cast<SequenceSearchData *>(arg_pt);

  if (seq->strip && seq->strip->transform == data->data) {
    data->seq = seq;
    return false; /* done so bail out */
  }
  return true;
}

static Sequence *sequence_get_by_transform(Editing *ed, StripTransform *transform)
{
  SequenceSearchData data;

  data.seq = nullptr;
  data.data = transform;

  /* irritating we need to search for our sequence! */
  SEQ_for_each_callback(&ed->seqbase, transform_seq_cmp_fn, &data);

  return data.seq;
}

static std::optional<std::string> rna_SequenceTransform_path(const PointerRNA *ptr)
{
  Scene *scene = (Scene *)ptr->owner_id;
  Editing *ed = SEQ_editing_get(scene);
  Sequence *seq = sequence_get_by_transform(ed, static_cast<StripTransform *>(ptr->data));

  if (seq) {
    char name_esc[(sizeof(seq->name) - 2) * 2];
    BLI_str_escape(name_esc, seq->name + 2, sizeof(name_esc));
    return fmt::format("sequence_editor.sequences_all[\"{}\"].transform", name_esc);
  }
  return "";
}

static void rna_SequenceTransform_update(Main * /*bmain*/, Scene * /*scene*/, PointerRNA *ptr)
{
  Scene *scene = (Scene *)ptr->owner_id;
  Editing *ed = SEQ_editing_get(scene);
  Sequence *seq = sequence_get_by_transform(ed, static_cast<StripTransform *>(ptr->data));

  SEQ_relations_invalidate_cache_preprocessed(scene, seq);
}

static bool crop_seq_cmp_fn(Sequence *seq, void *arg_pt)
{
  SequenceSearchData *data = static_cast<SequenceSearchData *>(arg_pt);

  if (seq->strip && seq->strip->crop == data->data) {
    data->seq = seq;
    return false; /* done so bail out */
  }
  return true;
}

static Sequence *sequence_get_by_crop(Editing *ed, StripCrop *crop)
{
  SequenceSearchData data;

  data.seq = nullptr;
  data.data = crop;

  /* irritating we need to search for our sequence! */
  SEQ_for_each_callback(&ed->seqbase, crop_seq_cmp_fn, &data);

  return data.seq;
}

static std::optional<std::string> rna_SequenceCrop_path(const PointerRNA *ptr)
{
  Scene *scene = (Scene *)ptr->owner_id;
  Editing *ed = SEQ_editing_get(scene);
  Sequence *seq = sequence_get_by_crop(ed, static_cast<StripCrop *>(ptr->data));

  if (seq) {
    char name_esc[(sizeof(seq->name) - 2) * 2];
    BLI_str_escape(name_esc, seq->name + 2, sizeof(name_esc));
    return fmt::format("sequence_editor.sequences_all[\"{}\"].crop", name_esc);
  }
  return "";
}

static void rna_SequenceCrop_update(Main * /*bmain*/, Scene * /*scene*/, PointerRNA *ptr)
{
  Scene *scene = (Scene *)ptr->owner_id;
  Editing *ed = SEQ_editing_get(scene);
  Sequence *seq = sequence_get_by_crop(ed, static_cast<StripCrop *>(ptr->data));

  SEQ_relations_invalidate_cache_preprocessed(scene, seq);
}

static void rna_Sequence_text_font_set(PointerRNA *ptr,
                                       PointerRNA ptr_value,
                                       ReportList * /*reports*/)
{
  Sequence *seq = static_cast<Sequence *>(ptr->data);
  TextVars *data = static_cast<TextVars *>(seq->effectdata);
  VFont *value = static_cast<VFont *>(ptr_value.data);

  SEQ_effect_text_font_unload(data, true);

  id_us_plus(&value->id);
  data->text_blf_id = SEQ_FONT_NOT_LOADED;
  data->text_font = value;
}

/* name functions that ignore the first two characters */
static void rna_Sequence_name_get(PointerRNA *ptr, char *value)
{
  Sequence *seq = (Sequence *)ptr->data;
  strcpy(value, seq->name + 2);
}

static int rna_Sequence_name_length(PointerRNA *ptr)
{
  Sequence *seq = (Sequence *)ptr->data;
  return strlen(seq->name + 2);
}

static void rna_Sequence_name_set(PointerRNA *ptr, const char *value)
{
  Scene *scene = (Scene *)ptr->owner_id;
  Sequence *seq = (Sequence *)ptr->data;
  char oldname[sizeof(seq->name)];
  AnimData *adt;

  SEQ_prefetch_stop(scene);

  /* make a copy of the old name first */
  BLI_strncpy(oldname, seq->name + 2, sizeof(seq->name) - 2);

  /* copy the new name into the name slot */
  SEQ_edit_sequence_name_set(scene, seq, value);

  /* make sure the name is unique */
  SEQ_sequence_base_unique_name_recursive(scene, &scene->ed->seqbase, seq);
  /* fix all the animation data which may link to this */

  /* Don't rename everywhere because these are per scene. */
#  if 0
  BKE_animdata_fix_paths_rename_all(
      nullptr, "sequence_editor.sequences_all", oldname, seq->name + 2);
#  endif
  adt = BKE_animdata_from_id(&scene->id);
  if (adt) {
    BKE_animdata_fix_paths_rename(&scene->id,
                                  adt,
                                  nullptr,
                                  "sequence_editor.sequences_all",
                                  oldname,
                                  seq->name + 2,
                                  0,
                                  0,
                                  1);
  }
}

static StructRNA *rna_Sequence_refine(PointerRNA *ptr)
{
  Sequence *seq = (Sequence *)ptr->data;

  switch (seq->type) {
    case SEQ_TYPE_IMAGE:
      return &RNA_ImageSequence;
    case SEQ_TYPE_META:
      return &RNA_MetaSequence;
    case SEQ_TYPE_SCENE:
      return &RNA_SceneSequence;
    case SEQ_TYPE_MOVIE:
      return &RNA_MovieSequence;
    case SEQ_TYPE_MOVIECLIP:
      return &RNA_MovieClipSequence;
    case SEQ_TYPE_MASK:
      return &RNA_MaskSequence;
    case SEQ_TYPE_SOUND_RAM:
      return &RNA_SoundSequence;
    case SEQ_TYPE_CROSS:
      return &RNA_CrossSequence;
    case SEQ_TYPE_ADD:
      return &RNA_AddSequence;
    case SEQ_TYPE_SUB:
      return &RNA_SubtractSequence;
    case SEQ_TYPE_ALPHAOVER:
      return &RNA_AlphaOverSequence;
    case SEQ_TYPE_ALPHAUNDER:
      return &RNA_AlphaUnderSequence;
    case SEQ_TYPE_GAMCROSS:
      return &RNA_GammaCrossSequence;
    case SEQ_TYPE_MUL:
      return &RNA_MultiplySequence;
    case SEQ_TYPE_OVERDROP:
      return &RNA_OverDropSequence;
    case SEQ_TYPE_MULTICAM:
      return &RNA_MulticamSequence;
    case SEQ_TYPE_ADJUSTMENT:
      return &RNA_AdjustmentSequence;
    case SEQ_TYPE_WIPE:
      return &RNA_WipeSequence;
    case SEQ_TYPE_GLOW:
      return &RNA_GlowSequence;
    case SEQ_TYPE_TRANSFORM:
      return &RNA_TransformSequence;
    case SEQ_TYPE_COLOR:
      return &RNA_ColorSequence;
    case SEQ_TYPE_SPEED:
      return &RNA_SpeedControlSequence;
    case SEQ_TYPE_GAUSSIAN_BLUR:
      return &RNA_GaussianBlurSequence;
    case SEQ_TYPE_TEXT:
      return &RNA_TextSequence;
    case SEQ_TYPE_COLORMIX:
      return &RNA_ColorMixSequence;
    default:
      return &RNA_Sequence;
  }
}

static std::optional<std::string> rna_Sequence_path(const PointerRNA *ptr)
{
  const Sequence *seq = (Sequence *)ptr->data;

  /* sequencer data comes from scene...
   * TODO: would be nice to make SequenceEditor data a data-block of its own (for shorter paths)
   */
  char name_esc[(sizeof(seq->name) - 2) * 2];

  BLI_str_escape(name_esc, seq->name + 2, sizeof(name_esc));
  return fmt::format("sequence_editor.sequences_all[\"{}\"]", name_esc);
}

static IDProperty **rna_Sequence_idprops(PointerRNA *ptr)
{
  Sequence *seq = static_cast<Sequence *>(ptr->data);
  return &seq->prop;
}

static bool rna_MovieSequence_reload_if_needed(ID *scene_id, Sequence *seq, Main *bmain)
{
  Scene *scene = (Scene *)scene_id;

  bool has_reloaded;
  bool can_produce_frames;

  SEQ_add_movie_reload_if_needed(bmain, scene, seq, &has_reloaded, &can_produce_frames);

  if (has_reloaded && can_produce_frames) {
    SEQ_relations_invalidate_cache_raw(scene, seq);

    DEG_id_tag_update(&scene->id, ID_RECALC_SEQUENCER_STRIPS);
    WM_main_add_notifier(NC_SCENE | ND_SEQUENCER, scene);
  }

  return can_produce_frames;
}

static PointerRNA rna_MovieSequence_metadata_get(ID *scene_id, Sequence *seq)
{
  if (seq == nullptr || seq->anims.first == nullptr) {
    return PointerRNA_NULL;
  }

  StripAnim *sanim = static_cast<StripAnim *>(seq->anims.first);
  if (sanim->anim == nullptr) {
    return PointerRNA_NULL;
  }

  IDProperty *metadata = IMB_anim_load_metadata(sanim->anim);
  if (metadata == nullptr) {
    return PointerRNA_NULL;
  }

  PointerRNA ptr = RNA_pointer_create(scene_id, &RNA_IDPropertyWrapPtr, metadata);
  return ptr;
}

static PointerRNA rna_SequenceEditor_meta_stack_get(CollectionPropertyIterator *iter)
{
  ListBaseIterator *internal = &iter->internal.listbase;
  MetaStack *ms = (MetaStack *)internal->link;

  return rna_pointer_inherit_refine(&iter->parent, &RNA_Sequence, ms->parseq);
}

/* TODO: expose seq path setting as a higher level sequencer BKE function. */
static void rna_Sequence_filepath_set(PointerRNA *ptr, const char *value)
{
  Sequence *seq = (Sequence *)(ptr->data);
  BLI_path_split_dir_file(value,
                          seq->strip->dirpath,
                          sizeof(seq->strip->dirpath),
                          seq->strip->stripdata->filename,
                          sizeof(seq->strip->stripdata->filename));
}

static void rna_Sequence_filepath_get(PointerRNA *ptr, char *value)
{
  Sequence *seq = (Sequence *)(ptr->data);
  char filepath[FILE_MAX];

  BLI_path_join(filepath, sizeof(filepath), seq->strip->dirpath, seq->strip->stripdata->filename);
  strcpy(value, filepath);
}

static int rna_Sequence_filepath_length(PointerRNA *ptr)
{
  Sequence *seq = (Sequence *)(ptr->data);
  char filepath[FILE_MAX];

  BLI_path_join(filepath, sizeof(filepath), seq->strip->dirpath, seq->strip->stripdata->filename);
  return strlen(filepath);
}

static void rna_Sequence_proxy_filepath_set(PointerRNA *ptr, const char *value)
{
  StripProxy *proxy = (StripProxy *)(ptr->data);
  BLI_path_split_dir_file(
      value, proxy->dirpath, sizeof(proxy->dirpath), proxy->filename, sizeof(proxy->filename));
  if (proxy->anim) {
    IMB_free_anim(proxy->anim);
    proxy->anim = nullptr;
  }
}

static void rna_Sequence_proxy_filepath_get(PointerRNA *ptr, char *value)
{
  StripProxy *proxy = (StripProxy *)(ptr->data);
  char filepath[FILE_MAX];

  BLI_path_join(filepath, sizeof(filepath), proxy->dirpath, proxy->filename);
  strcpy(value, filepath);
}

static int rna_Sequence_proxy_filepath_length(PointerRNA *ptr)
{
  StripProxy *proxy = (StripProxy *)(ptr->data);
  char filepath[FILE_MAX];

  BLI_path_join(filepath, sizeof(filepath), proxy->dirpath, proxy->filename);
  return strlen(filepath);
}

static void rna_Sequence_audio_update(Main * /*bmain*/, Scene * /*scene*/, PointerRNA *ptr)
{
  DEG_id_tag_update(ptr->owner_id, ID_RECALC_SEQUENCER_STRIPS | ID_RECALC_AUDIO);
}

static void rna_Sequence_pan_range(
    PointerRNA *ptr, float *min, float *max, float *softmin, float *softmax)
{
  Scene *scene = (Scene *)ptr->owner_id;

  *min = -FLT_MAX;
  *max = FLT_MAX;
  *softmax = 1 + int(scene->r.ffcodecdata.audio_channels > 2);
  *softmin = -*softmax;
}

static int rna_Sequence_input_count_get(PointerRNA *ptr)
{
  Sequence *seq = (Sequence *)(ptr->data);

  return SEQ_effect_get_num_inputs(seq->type);
}

static void rna_Sequence_input_set(PointerRNA *ptr,
                                   PointerRNA ptr_value,
                                   ReportList *reports,
                                   int input_num)
{

  Sequence *seq = static_cast<Sequence *>(ptr->data);
  Sequence *input = static_cast<Sequence *>(ptr_value.data);

  if (SEQ_relations_render_loop_check(input, seq)) {
    BKE_report(reports, RPT_ERROR, "Cannot reassign inputs: recursion detected");
    return;
  }

  switch (input_num) {
    case 1:
      seq->seq1 = input;
      break;
    case 2:
      seq->seq2 = input;
      break;
  }
}

static void rna_Sequence_input_1_set(PointerRNA *ptr, PointerRNA ptr_value, ReportList *reports)
{
  rna_Sequence_input_set(ptr, ptr_value, reports, 1);
}

static void rna_Sequence_input_2_set(PointerRNA *ptr, PointerRNA ptr_value, ReportList *reports)
{
  rna_Sequence_input_set(ptr, ptr_value, reports, 2);
}
#  if 0
static void rna_SoundSequence_filename_set(PointerRNA *ptr, const char *value)
{
  Sequence *seq = (Sequence *)(ptr->data);
  BLI_path_split_dir_file(value,
                          seq->strip->dirpath,
                          sizeof(seq->strip->dirpath),
                          seq->strip->stripdata->name,
                          sizeof(seq->strip->stripdata->name));
}

static void rna_SequenceElement_filename_set(PointerRNA *ptr, const char *value)
{
  StripElem *elem = (StripElem *)(ptr->data);
  BLI_path_split_file_part(value, elem->name, sizeof(elem->name));
}
#  endif

static void rna_Sequence_reopen_files_update(Main *bmain, Scene * /*scene*/, PointerRNA *ptr)
{
  Scene *scene = (Scene *)ptr->owner_id;
  Editing *ed = SEQ_editing_get(scene);

  SEQ_relations_free_imbuf(scene, &ed->seqbase, false);
  rna_Sequence_invalidate_raw_update(bmain, scene, ptr);

  if (RNA_struct_is_a(ptr->type, &RNA_SoundSequence)) {
    SEQ_sound_update_bounds(scene, static_cast<Sequence *>(ptr->data));
  }
}

static void rna_Sequence_filepath_update(Main *bmain, Scene * /*scene*/, PointerRNA *ptr)
{
  Scene *scene = (Scene *)ptr->owner_id;
  Sequence *seq = (Sequence *)(ptr->data);
  SEQ_add_reload_new_file(bmain, scene, seq, true);
  rna_Sequence_invalidate_raw_update(bmain, scene, ptr);
}

static void rna_Sequence_sound_update(Main *bmain, Scene * /*active_scene*/, PointerRNA *ptr)
{
  Scene *scene = (Scene *)ptr->owner_id;
  DEG_id_tag_update(&scene->id, ID_RECALC_SEQUENCER_STRIPS | ID_RECALC_AUDIO);
  DEG_relations_tag_update(bmain);
}

static bool seqproxy_seq_cmp_fn(Sequence *seq, void *arg_pt)
{
  SequenceSearchData *data = static_cast<SequenceSearchData *>(arg_pt);

  if (seq->strip && seq->strip->proxy == data->data) {
    data->seq = seq;
    return false; /* done so bail out */
  }
  return true;
}

static Sequence *sequence_get_by_proxy(Editing *ed, StripProxy *proxy)
{
  SequenceSearchData data;

  data.seq = nullptr;
  data.data = proxy;

  SEQ_for_each_callback(&ed->seqbase, seqproxy_seq_cmp_fn, &data);
  return data.seq;
}

static void rna_Sequence_tcindex_update(Main *bmain, Scene * /*scene*/, PointerRNA *ptr)
{
  Scene *scene = (Scene *)ptr->owner_id;
  Editing *ed = SEQ_editing_get(scene);
  Sequence *seq = sequence_get_by_proxy(ed, static_cast<StripProxy *>(ptr->data));

  SEQ_add_reload_new_file(bmain, scene, seq, false);
  do_sequence_frame_change_update(scene, seq);
}

static void rna_SequenceProxy_update(Main * /*bmain*/, Scene * /*scene*/, PointerRNA *ptr)
{
  Scene *scene = (Scene *)ptr->owner_id;
  Editing *ed = SEQ_editing_get(scene);
  Sequence *seq = sequence_get_by_proxy(ed, static_cast<StripProxy *>(ptr->data));
  SEQ_relations_invalidate_cache_preprocessed(scene, seq);
}

/* do_versions? */
static float rna_Sequence_opacity_get(PointerRNA *ptr)
{
  Sequence *seq = (Sequence *)(ptr->data);
  return seq->blend_opacity / 100.0f;
}
static void rna_Sequence_opacity_set(PointerRNA *ptr, float value)
{
  Sequence *seq = (Sequence *)(ptr->data);
  CLAMP(value, 0.0f, 1.0f);
  seq->blend_opacity = value * 100.0f;
}

static int rna_Sequence_color_tag_get(PointerRNA *ptr)
{
  Sequence *seq = (Sequence *)(ptr->data);
  return seq->color_tag;
}

static void rna_Sequence_color_tag_set(PointerRNA *ptr, int value)
{
  Sequence *seq = (Sequence *)(ptr->data);
  seq->color_tag = value;
}

static bool colbalance_seq_cmp_fn(Sequence *seq, void *arg_pt)
{
  SequenceSearchData *data = static_cast<SequenceSearchData *>(arg_pt);

  for (SequenceModifierData *smd = static_cast<SequenceModifierData *>(seq->modifiers.first); smd;
       smd = smd->next)
  {
    if (smd->type == seqModifierType_ColorBalance) {
      ColorBalanceModifierData *cbmd = (ColorBalanceModifierData *)smd;

      if (&cbmd->color_balance == data->data) {
        data->seq = seq;
        data->smd = smd;
        return false; /* done so bail out */
      }
    }
  }

  return true;
}

static Sequence *sequence_get_by_colorbalance(Editing *ed,
                                              StripColorBalance *cb,
                                              SequenceModifierData **r_smd)
{
  SequenceSearchData data;

  data.seq = nullptr;
  data.smd = nullptr;
  data.data = cb;

  /* irritating we need to search for our sequence! */
  SEQ_for_each_callback(&ed->seqbase, colbalance_seq_cmp_fn, &data);

  *r_smd = data.smd;

  return data.seq;
}

static std::optional<std::string> rna_SequenceColorBalance_path(const PointerRNA *ptr)
{
  Scene *scene = (Scene *)ptr->owner_id;
  SequenceModifierData *smd;
  Editing *ed = SEQ_editing_get(scene);
  Sequence *seq = sequence_get_by_colorbalance(
      ed, static_cast<StripColorBalance *>(ptr->data), &smd);

  if (seq) {
    char name_esc[(sizeof(seq->name) - 2) * 2];

    BLI_str_escape(name_esc, seq->name + 2, sizeof(name_esc));

    if (!smd) {
      /* Path to old filter color balance. */
      return fmt::format("sequence_editor.sequences_all[\"{}\"].color_balance", name_esc);
    }
    /* Path to modifier. */
    char name_esc_smd[sizeof(smd->name) * 2];

    BLI_str_escape(name_esc_smd, smd->name, sizeof(name_esc_smd));
    return fmt::format("sequence_editor.sequences_all[\"{}\"].modifiers[\"{}\"].color_balance",
                       name_esc,
                       name_esc_smd);
  }
  return "";
}

static void rna_SequenceColorBalance_update(Main * /*bmain*/, Scene * /*scene*/, PointerRNA *ptr)
{
  Scene *scene = (Scene *)ptr->owner_id;
  Editing *ed = SEQ_editing_get(scene);
  SequenceModifierData *smd;
  Sequence *seq = sequence_get_by_colorbalance(
      ed, static_cast<StripColorBalance *>(ptr->data), &smd);

  SEQ_relations_invalidate_cache_preprocessed(scene, seq);
}

static void rna_SequenceEditor_overlay_lock_set(PointerRNA *ptr, bool value)
{
  Scene *scene = (Scene *)ptr->owner_id;
  Editing *ed = SEQ_editing_get(scene);

  if (ed == nullptr) {
    return;
  }

  /* convert from abs to relative and back */
  if ((ed->overlay_frame_flag & SEQ_EDIT_OVERLAY_FRAME_ABS) == 0 && value) {
    ed->overlay_frame_abs = scene->r.cfra + ed->overlay_frame_ofs;
    ed->overlay_frame_flag |= SEQ_EDIT_OVERLAY_FRAME_ABS;
  }
  else if ((ed->overlay_frame_flag & SEQ_EDIT_OVERLAY_FRAME_ABS) && !value) {
    ed->overlay_frame_ofs = ed->overlay_frame_abs - scene->r.cfra;
    ed->overlay_frame_flag &= ~SEQ_EDIT_OVERLAY_FRAME_ABS;
  }
}

static int rna_SequenceEditor_overlay_frame_get(PointerRNA *ptr)
{
  Scene *scene = (Scene *)ptr->owner_id;
  Editing *ed = SEQ_editing_get(scene);

  if (ed == nullptr) {
    return scene->r.cfra;
  }

  if (ed->overlay_frame_flag & SEQ_EDIT_OVERLAY_FRAME_ABS) {
    return ed->overlay_frame_abs - scene->r.cfra;
  }
  else {
    return ed->overlay_frame_ofs;
  }
}

static void rna_SequenceEditor_overlay_frame_set(PointerRNA *ptr, int value)
{
  Scene *scene = (Scene *)ptr->owner_id;
  Editing *ed = SEQ_editing_get(scene);

  if (ed == nullptr) {
    return;
  }

  if (ed->overlay_frame_flag & SEQ_EDIT_OVERLAY_FRAME_ABS) {
    ed->overlay_frame_abs = (scene->r.cfra + value);
  }
  else {
    ed->overlay_frame_ofs = value;
  }
}

static void rna_SequenceEditor_display_stack(ID *id,
                                             Editing *ed,
                                             ReportList *reports,
                                             Sequence *seqm)
{
  /* Check for non-meta sequence */
  if (seqm != nullptr && seqm->type != SEQ_TYPE_META && SEQ_exists_in_seqbase(seqm, &ed->seqbase))
  {
    BKE_report(reports, RPT_ERROR, "Sequence type must be 'META'");
    return;
  }

  /* Get editing base of meta sequence */
  Scene *scene = (Scene *)id;
  SEQ_meta_stack_set(scene, seqm);
  /* De-activate strip. This is to prevent strip from different timeline being drawn. */
  SEQ_select_active_set(scene, nullptr);

  WM_main_add_notifier(NC_SCENE | ND_SEQUENCER, scene);
}

static bool modifier_seq_cmp_fn(Sequence *seq, void *arg_pt)
{
  SequenceSearchData *data = static_cast<SequenceSearchData *>(arg_pt);

  if (BLI_findindex(&seq->modifiers, data->data) != -1) {
    data->seq = seq;
    return false; /* done so bail out */
  }

  return true;
}

static Sequence *sequence_get_by_modifier(Editing *ed, SequenceModifierData *smd)
{
  SequenceSearchData data;

  data.seq = nullptr;
  data.data = smd;

  /* irritating we need to search for our sequence! */
  SEQ_for_each_callback(&ed->seqbase, modifier_seq_cmp_fn, &data);

  return data.seq;
}

static StructRNA *rna_SequenceModifier_refine(PointerRNA *ptr)
{
  SequenceModifierData *smd = (SequenceModifierData *)ptr->data;

  switch (smd->type) {
    case seqModifierType_ColorBalance:
      return &RNA_ColorBalanceModifier;
    case seqModifierType_Curves:
      return &RNA_CurvesModifier;
    case seqModifierType_HueCorrect:
      return &RNA_HueCorrectModifier;
    case seqModifierType_BrightContrast:
      return &RNA_BrightContrastModifier;
    case seqModifierType_WhiteBalance:
      return &RNA_WhiteBalanceModifier;
    case seqModifierType_Tonemap:
      return &RNA_SequencerTonemapModifierData;
    case seqModifierType_SoundEqualizer:
      return &RNA_SoundEqualizerModifier;
    default:
      return &RNA_SequenceModifier;
  }
}

static std::optional<std::string> rna_SequenceModifier_path(const PointerRNA *ptr)
{
  Scene *scene = (Scene *)ptr->owner_id;
  Editing *ed = SEQ_editing_get(scene);
  SequenceModifierData *smd = static_cast<SequenceModifierData *>(ptr->data);
  Sequence *seq = sequence_get_by_modifier(ed, smd);

  if (seq) {
    char name_esc[(sizeof(seq->name) - 2) * 2];
    char name_esc_smd[sizeof(smd->name) * 2];

    BLI_str_escape(name_esc, seq->name + 2, sizeof(name_esc));
    BLI_str_escape(name_esc_smd, smd->name, sizeof(name_esc_smd));
    return fmt::format(
        "sequence_editor.sequences_all[\"{}\"].modifiers[\"{}\"]", name_esc, name_esc_smd);
  }
  return "";
}

static void rna_SequenceModifier_name_set(PointerRNA *ptr, const char *value)
{
  SequenceModifierData *smd = static_cast<SequenceModifierData *>(ptr->data);
  Scene *scene = (Scene *)ptr->owner_id;
  Editing *ed = SEQ_editing_get(scene);
  Sequence *seq = sequence_get_by_modifier(ed, smd);
  AnimData *adt;
  char oldname[sizeof(smd->name)];

  /* make a copy of the old name first */
  STRNCPY(oldname, smd->name);

  /* copy the new name into the name slot */
  STRNCPY_UTF8(smd->name, value);

  /* make sure the name is truly unique */
  SEQ_modifier_unique_name(seq, smd);

  /* fix all the animation data which may link to this */
  adt = BKE_animdata_from_id(&scene->id);
  if (adt) {
    char rna_path_prefix[1024];

    char seq_name_esc[(sizeof(seq->name) - 2) * 2];
    BLI_str_escape(seq_name_esc, seq->name + 2, sizeof(seq_name_esc));

    SNPRINTF(rna_path_prefix, "sequence_editor.sequences_all[\"%s\"].modifiers", seq_name_esc);
    BKE_animdata_fix_paths_rename(
        &scene->id, adt, nullptr, rna_path_prefix, oldname, smd->name, 0, 0, 1);
  }
}

static void rna_SequenceModifier_update(Main *bmain, Scene * /*scene*/, PointerRNA *ptr)
{
  /* strip from other scenes could be modified, so using active scene is not reliable */
  Scene *scene = (Scene *)ptr->owner_id;
  Editing *ed = SEQ_editing_get(scene);
  Sequence *seq = sequence_get_by_modifier(ed, static_cast<SequenceModifierData *>(ptr->data));

  if (ELEM(seq->type, SEQ_TYPE_SOUND_RAM, SEQ_TYPE_SOUND_HD)) {
    DEG_id_tag_update(&scene->id, ID_RECALC_SEQUENCER_STRIPS | ID_RECALC_AUDIO);
    DEG_relations_tag_update(bmain);
  }
  else {
    SEQ_relations_invalidate_cache_preprocessed(scene, seq);
  }
}

/*
 * Update of Curve in an EQ Sound Modifier
 */
static void rna_SequenceModifier_EQCurveMapping_update(Main *bmain,
                                                       Scene * /*scene*/,
                                                       PointerRNA *ptr)
{
  Scene *scene = (Scene *)ptr->owner_id;

  DEG_id_tag_update(&scene->id, ID_RECALC_SEQUENCER_STRIPS | ID_RECALC_AUDIO);
  DEG_relations_tag_update(bmain);
  WM_main_add_notifier(NC_SCENE | ND_SEQUENCER, NULL);
}

static bool rna_SequenceModifier_otherSequence_poll(PointerRNA *ptr, PointerRNA value)
{
  Scene *scene = (Scene *)ptr->owner_id;
  Editing *ed = SEQ_editing_get(scene);
  Sequence *seq = sequence_get_by_modifier(ed, static_cast<SequenceModifierData *>(ptr->data));
  Sequence *cur = (Sequence *)value.data;

  if ((seq == cur) || (cur->type == SEQ_TYPE_SOUND_RAM)) {
    return false;
  }

  return true;
}

static SequenceModifierData *rna_Sequence_modifier_new(
    Sequence *seq, bContext *C, ReportList *reports, const char *name, int type)
{
  if (!SEQ_sequence_supports_modifiers(seq)) {
    BKE_report(reports, RPT_ERROR, "Sequence type does not support modifiers");

    return nullptr;
  }
  else {
    Scene *scene = CTX_data_scene(C);
    SequenceModifierData *smd;

    smd = SEQ_modifier_new(seq, name, type);

    SEQ_relations_invalidate_cache_preprocessed(scene, seq);

    WM_main_add_notifier(NC_SCENE | ND_SEQUENCER, nullptr);

    return smd;
  }
}

static void rna_Sequence_modifier_remove(Sequence *seq,
                                         bContext *C,
                                         ReportList *reports,
                                         PointerRNA *smd_ptr)
{
  SequenceModifierData *smd = static_cast<SequenceModifierData *>(smd_ptr->data);
  Scene *scene = CTX_data_scene(C);

  if (SEQ_modifier_remove(seq, smd) == false) {
    BKE_report(reports, RPT_ERROR, "Modifier was not found in the stack");
    return;
  }

  RNA_POINTER_INVALIDATE(smd_ptr);
  SEQ_relations_invalidate_cache_preprocessed(scene, seq);

  WM_main_add_notifier(NC_SCENE | ND_SEQUENCER, nullptr);
}

static void rna_Sequence_modifier_clear(Sequence *seq, bContext *C)
{
  Scene *scene = CTX_data_scene(C);

  SEQ_modifier_clear(seq);

  SEQ_relations_invalidate_cache_preprocessed(scene, seq);

  WM_main_add_notifier(NC_SCENE | ND_SEQUENCER, nullptr);
}

static void rna_SequenceModifier_strip_set(PointerRNA *ptr, PointerRNA value, ReportList *reports)
{
  SequenceModifierData *smd = static_cast<SequenceModifierData *>(ptr->data);
  Scene *scene = (Scene *)ptr->owner_id;
  Editing *ed = SEQ_editing_get(scene);
  Sequence *seq = sequence_get_by_modifier(ed, smd);
  Sequence *target = (Sequence *)value.data;

  if (target != nullptr && SEQ_relations_render_loop_check(target, seq)) {
    BKE_report(reports, RPT_ERROR, "Recursion detected, cannot use this strip");
    return;
  }

  smd->mask_sequence = target;
}

static float rna_Sequence_fps_get(PointerRNA *ptr)
{
  Scene *scene = (Scene *)ptr->owner_id;
  Sequence *seq = (Sequence *)(ptr->data);
  return SEQ_time_sequence_get_fps(scene, seq);
}

static void rna_Sequence_separate(ID *id, Sequence *seqm, Main *bmain)
{
  Scene *scene = (Scene *)id;

  /* Find the appropriate seqbase */
  ListBase *seqbase = SEQ_get_seqbase_by_seq(scene, seqm);

  LISTBASE_FOREACH_MUTABLE (Sequence *, seq, &seqm->seqbase) {
    SEQ_edit_move_strip_to_seqbase(scene, &seqm->seqbase, seq, seqbase);
  }

  SEQ_edit_flag_for_removal(scene, seqbase, seqm);
  SEQ_edit_remove_flagged_sequences(scene, seqbase);

  /* Update depsgraph. */
  DEG_relations_tag_update(bmain);
  DEG_id_tag_update(&scene->id, ID_RECALC_SEQUENCER_STRIPS);

  WM_main_add_notifier(NC_SCENE | ND_SEQUENCER, scene);
}

/* Find channel owner. If nullptr, owner is `Editing`, otherwise it's `Sequence`. */
static Sequence *rna_SeqTimelineChannel_owner_get(Editing *ed, SeqTimelineChannel *channel)
{
  blender::VectorSet strips = SEQ_query_all_meta_strips_recursive(&ed->seqbase);

  Sequence *channel_owner = nullptr;
  for (Sequence *seq : strips) {
    if (BLI_findindex(&seq->channels, channel) != -1) {
      channel_owner = seq;
      break;
    }
  }

  return channel_owner;
}

static void rna_SequenceTimelineChannel_name_set(PointerRNA *ptr, const char *value)
{
  SeqTimelineChannel *channel = (SeqTimelineChannel *)ptr->data;
  Scene *scene = (Scene *)ptr->owner_id;
  Editing *ed = SEQ_editing_get(scene);

  Sequence *channel_owner = rna_SeqTimelineChannel_owner_get(ed, channel);
  ListBase *channels_base = &ed->channels;

  if (channel_owner != nullptr) {
    channels_base = &channel_owner->channels;
  }

  STRNCPY_UTF8(channel->name, value);
  BLI_uniquename(channels_base,
                 channel,
                 "Channel",
                 '.',
                 offsetof(SeqTimelineChannel, name),
                 sizeof(channel->name));
}

static void rna_SequenceTimelineChannel_mute_update(Main *bmain,
                                                    Scene *active_scene,
                                                    PointerRNA *ptr)
{
  Scene *scene = (Scene *)ptr->owner_id;
  Editing *ed = SEQ_editing_get(scene);
  SeqTimelineChannel *channel = (SeqTimelineChannel *)ptr;

  Sequence *channel_owner = rna_SeqTimelineChannel_owner_get(ed, channel);
  ListBase *seqbase;
  if (channel_owner == nullptr) {
    seqbase = &ed->seqbase;
  }
  else {
    seqbase = &channel_owner->seqbase;
  }

  LISTBASE_FOREACH (Sequence *, seq, seqbase) {
    SEQ_relations_invalidate_cache_composite(scene, seq);
  }

  rna_Sequence_sound_update(bmain, active_scene, ptr);
}

static std::optional<std::string> rna_SeqTimelineChannel_path(const PointerRNA *ptr)
{
  Scene *scene = (Scene *)ptr->owner_id;
  Editing *ed = SEQ_editing_get(scene);
  SeqTimelineChannel *channel = (SeqTimelineChannel *)ptr->data;

  Sequence *channel_owner = rna_SeqTimelineChannel_owner_get(ed, channel);

  char channel_name_esc[(sizeof(channel->name)) * 2];
  BLI_str_escape(channel_name_esc, channel->name, sizeof(channel_name_esc));

  if (channel_owner == nullptr) {
    return fmt::format("sequence_editor.channels[\"{}\"]", channel_name_esc);
  }
  char owner_name_esc[(sizeof(channel_owner->name) - 2) * 2];
  BLI_str_escape(owner_name_esc, channel_owner->name + 2, sizeof(owner_name_esc));
  return fmt::format(
      "sequence_editor.sequences_all[\"{}\"].channels[\"{}\"]", owner_name_esc, channel_name_esc);
}

static EQCurveMappingData *rna_Sequence_SoundEqualizer_Curve_add(SoundEqualizerModifierData *semd,
                                                                 bContext * /*C*/,
                                                                 float min_freq,
                                                                 float max_freq)
{
  EQCurveMappingData *eqcmd = SEQ_sound_equalizermodifier_add_graph(semd, min_freq, max_freq);
  WM_main_add_notifier(NC_SCENE | ND_SEQUENCER, NULL);
  return eqcmd;
}

static void rna_Sequence_SoundEqualizer_Curve_clear(SoundEqualizerModifierData *semd,
                                                    bContext * /*C*/)
{
  SEQ_sound_equalizermodifier_free((SequenceModifierData *)semd);
  WM_main_add_notifier(NC_SCENE | ND_SEQUENCER, NULL);
}

#else

static void rna_def_strip_element(BlenderRNA *brna)
{
  StructRNA *srna;
  PropertyRNA *prop;

  srna = RNA_def_struct(brna, "SequenceElement", nullptr);
  RNA_def_struct_ui_text(srna, "Sequence Element", "Sequence strip data for a single frame");
  RNA_def_struct_sdna(srna, "StripElem");

  prop = RNA_def_property(srna, "filename", PROP_STRING, PROP_FILENAME);
  RNA_def_property_string_sdna(prop, nullptr, "filename");
  RNA_def_property_ui_text(prop, "Filename", "Name of the source file");
  RNA_def_property_update(prop, NC_SCENE | ND_SEQUENCER, "rna_SequenceElement_update");

  prop = RNA_def_property(srna, "orig_width", PROP_INT, PROP_NONE);
  RNA_def_property_int_sdna(prop, nullptr, "orig_width");
  RNA_def_property_clear_flag(prop, PROP_EDITABLE);
  RNA_def_property_ui_text(prop, "Orig Width", "Original image width");

  prop = RNA_def_property(srna, "orig_height", PROP_INT, PROP_NONE);
  RNA_def_property_int_sdna(prop, nullptr, "orig_height");
  RNA_def_property_clear_flag(prop, PROP_EDITABLE);
  RNA_def_property_ui_text(prop, "Orig Height", "Original image height");

  prop = RNA_def_property(srna, "orig_fps", PROP_FLOAT, PROP_NONE);
  RNA_def_property_float_sdna(prop, nullptr, "orig_fps");
  RNA_def_property_clear_flag(prop, PROP_EDITABLE);
  RNA_def_property_ui_text(prop, "Orig FPS", "Original frames per second");
}

static void rna_def_retiming_key(BlenderRNA *brna)
{
  StructRNA *srna;
  PropertyRNA *prop;

  srna = RNA_def_struct(brna, "RetimingKey", nullptr);
  RNA_def_struct_ui_text(
      srna,
      "Retiming Key",
      "Key mapped to particular frame that can be moved to change playback speed");
  RNA_def_struct_sdna(srna, "SeqRetimingKey");

  prop = RNA_def_property(srna, "timeline_frame", PROP_INT, PROP_NONE);
  RNA_def_property_int_funcs(
      prop, "rna_Sequence_retiming_key_frame_get", "rna_Sequence_retiming_key_frame_set", nullptr);
  RNA_def_property_ui_text(prop, "Timeline Frame", "Position of retiming key in timeline");

  FunctionRNA *func = RNA_def_function(srna, "remove", "rna_Sequence_retiming_key_remove");
  RNA_def_function_flag(func, FUNC_USE_SELF_ID);
  RNA_def_function_ui_description(func, "Remove retiming key");
}

static void rna_def_strip_crop(BlenderRNA *brna)
{
  StructRNA *srna;
  PropertyRNA *prop;

  srna = RNA_def_struct(brna, "SequenceCrop", nullptr);
  RNA_def_struct_ui_text(srna, "Sequence Crop", "Cropping parameters for a sequence strip");
  RNA_def_struct_sdna(srna, "StripCrop");

  prop = RNA_def_property(srna, "max_y", PROP_INT, PROP_PIXEL);
  RNA_def_property_int_sdna(prop, nullptr, "top");
  RNA_def_property_ui_text(prop, "Top", "Number of pixels to crop from the top");
  RNA_def_property_ui_range(prop, 0, 4096, 1, -1);
  RNA_def_property_update(prop, NC_SCENE | ND_SEQUENCER, "rna_SequenceCrop_update");

  prop = RNA_def_property(srna, "min_y", PROP_INT, PROP_PIXEL);
  RNA_def_property_int_sdna(prop, nullptr, "bottom");
  RNA_def_property_ui_text(prop, "Bottom", "Number of pixels to crop from the bottom");
  RNA_def_property_ui_range(prop, 0, 4096, 1, -1);
  RNA_def_property_update(prop, NC_SCENE | ND_SEQUENCER, "rna_SequenceCrop_update");

  prop = RNA_def_property(srna, "min_x", PROP_INT, PROP_PIXEL);
  RNA_def_property_int_sdna(prop, nullptr, "left");
  RNA_def_property_ui_text(prop, "Left", "Number of pixels to crop from the left side");
  RNA_def_property_ui_range(prop, 0, 4096, 1, -1);
  RNA_def_property_update(prop, NC_SCENE | ND_SEQUENCER, "rna_SequenceCrop_update");

  prop = RNA_def_property(srna, "max_x", PROP_INT, PROP_PIXEL);
  RNA_def_property_int_sdna(prop, nullptr, "right");
  RNA_def_property_ui_text(prop, "Right", "Number of pixels to crop from the right side");
  RNA_def_property_ui_range(prop, 0, 4096, 1, -1);
  RNA_def_property_update(prop, NC_SCENE | ND_SEQUENCER, "rna_SequenceCrop_update");

  RNA_def_struct_path_func(srna, "rna_SequenceCrop_path");
}

static const EnumPropertyItem transform_filter_items[] = {
    {SEQ_TRANSFORM_FILTER_AUTO,
     "AUTO",
     0,
     "Auto",
     "Automatically choose filter based on scaling factor"},
    {SEQ_TRANSFORM_FILTER_NEAREST, "NEAREST", 0, "Nearest", "Use nearest sample"},
    {SEQ_TRANSFORM_FILTER_BILINEAR,
     "BILINEAR",
     0,
     "Bilinear",
     "Interpolate between 2" BLI_STR_UTF8_MULTIPLICATION_SIGN "2 samples"},
    {SEQ_TRANSFORM_FILTER_CUBIC_MITCHELL,
     "CUBIC_MITCHELL",
     0,
     "Cubic Mitchell",
     "Cubic Mitchell filter on 4" BLI_STR_UTF8_MULTIPLICATION_SIGN "4 samples"},
    {SEQ_TRANSFORM_FILTER_CUBIC_BSPLINE,
     "CUBIC_BSPLINE",
     0,
     "Cubic B-Spline",
     "Cubic B-Spline filter (blurry but no ringing) on 4" BLI_STR_UTF8_MULTIPLICATION_SIGN
     "4 samples"},
    {SEQ_TRANSFORM_FILTER_BOX,
     "BOX",
     0,
     "Box",
     "Averages source image samples that fall under destination pixel"},
    {0, nullptr, 0, nullptr, nullptr},
};

static void rna_def_strip_transform(BlenderRNA *brna)
{
  StructRNA *srna;
  PropertyRNA *prop;

  srna = RNA_def_struct(brna, "SequenceTransform", nullptr);
  RNA_def_struct_ui_text(srna, "Sequence Transform", "Transform parameters for a sequence strip");
  RNA_def_struct_sdna(srna, "StripTransform");

  prop = RNA_def_property(srna, "scale_x", PROP_FLOAT, PROP_UNSIGNED);
  RNA_def_property_float_sdna(prop, nullptr, "scale_x");
  RNA_def_property_ui_text(prop, "Scale X", "Scale along X axis");
  RNA_def_property_ui_range(prop, 0, FLT_MAX, 3, 3);
  RNA_def_property_float_default(prop, 1.0f);
  RNA_def_property_update(prop, NC_SCENE | ND_SEQUENCER, "rna_SequenceTransform_update");

  prop = RNA_def_property(srna, "scale_y", PROP_FLOAT, PROP_UNSIGNED);
  RNA_def_property_float_sdna(prop, nullptr, "scale_y");
  RNA_def_property_ui_text(prop, "Scale Y", "Scale along Y axis");
  RNA_def_property_ui_range(prop, 0, FLT_MAX, 3, 3);
  RNA_def_property_float_default(prop, 1.0f);
  RNA_def_property_update(prop, NC_SCENE | ND_SEQUENCER, "rna_SequenceTransform_update");

  prop = RNA_def_property(srna, "offset_x", PROP_FLOAT, PROP_PIXEL);
  RNA_def_property_float_sdna(prop, nullptr, "xofs");
  RNA_def_property_ui_text(prop, "Translate X", "Move along X axis");
  RNA_def_property_ui_range(prop, -FLT_MAX, FLT_MAX, 100, 3);
  RNA_def_property_update(prop, NC_SCENE | ND_SEQUENCER, "rna_SequenceTransform_update");

  prop = RNA_def_property(srna, "offset_y", PROP_FLOAT, PROP_PIXEL);
  RNA_def_property_float_sdna(prop, nullptr, "yofs");
  RNA_def_property_ui_text(prop, "Translate Y", "Move along Y axis");
  RNA_def_property_ui_range(prop, -FLT_MAX, FLT_MAX, 100, 3);
  RNA_def_property_update(prop, NC_SCENE | ND_SEQUENCER, "rna_SequenceTransform_update");

  prop = RNA_def_property(srna, "rotation", PROP_FLOAT, PROP_ANGLE);
  RNA_def_property_float_sdna(prop, nullptr, "rotation");
  RNA_def_property_ui_text(prop, "Rotation", "Rotate around image center");
  RNA_def_property_update(prop, NC_SCENE | ND_SEQUENCER, "rna_SequenceTransform_update");

  prop = RNA_def_property(srna, "origin", PROP_FLOAT, PROP_NONE);
  RNA_def_property_float_sdna(prop, nullptr, "origin");
  RNA_def_property_ui_text(prop, "Origin", "Origin of image for transformation");
  RNA_def_property_ui_range(prop, 0, 1, 1, 3);
  RNA_def_property_update(prop, NC_SCENE | ND_SEQUENCER, "rna_SequenceTransform_update");

  prop = RNA_def_property(srna, "filter", PROP_ENUM, PROP_NONE);
  RNA_def_property_enum_sdna(prop, nullptr, "filter");
  RNA_def_property_enum_items(prop, transform_filter_items);
  RNA_def_property_enum_default(prop, SEQ_TRANSFORM_FILTER_AUTO);
  RNA_def_property_ui_text(prop, "Filter", "Type of filter to use for image transformation");
  RNA_def_property_update(prop, NC_SCENE | ND_SEQUENCER, "rna_SequenceTransform_update");

  RNA_def_struct_path_func(srna, "rna_SequenceTransform_path");
}

static void rna_def_strip_proxy(BlenderRNA *brna)
{
  StructRNA *srna;
  PropertyRNA *prop;

  static const EnumPropertyItem seq_tc_items[] = {
      {SEQ_PROXY_TC_NONE, "NONE", 0, "None", ""},
      {SEQ_PROXY_TC_RECORD_RUN,
       "RECORD_RUN",
       0,
       "Record Run",
       "Use images in the order as they are recorded"},
      {SEQ_PROXY_TC_FREE_RUN,
       "FREE_RUN",
       0,
       "Free Run",
       "Use global timestamp written by recording device"},
      {SEQ_PROXY_TC_INTERP_REC_DATE_FREE_RUN,
       "FREE_RUN_REC_DATE",
       0,
       "Free Run (rec date)",
       "Interpolate a global timestamp using the "
       "record date and time written by recording device"},
      {SEQ_PROXY_TC_RECORD_RUN_NO_GAPS,
       "RECORD_RUN_NO_GAPS",
       0,
       "Record Run No Gaps",
       "Like record run, but ignore timecode, "
       "changes in framerate or dropouts"},
      {0, nullptr, 0, nullptr, nullptr},
  };

  srna = RNA_def_struct(brna, "SequenceProxy", nullptr);
  RNA_def_struct_ui_text(srna, "Sequence Proxy", "Proxy parameters for a sequence strip");
  RNA_def_struct_sdna(srna, "StripProxy");

  prop = RNA_def_property(srna, "directory", PROP_STRING, PROP_DIRPATH);
  RNA_def_property_string_sdna(prop, nullptr, "dirpath");
  RNA_def_property_ui_text(prop, "Directory", "Location to store the proxy files");
  RNA_def_property_update(prop, NC_SCENE | ND_SEQUENCER, "rna_SequenceProxy_update");

  prop = RNA_def_property(srna, "filepath", PROP_STRING, PROP_FILEPATH);
  RNA_def_property_ui_text(prop, "Path", "Location of custom proxy file");
  RNA_def_property_translation_context(prop, BLT_I18NCONTEXT_EDITOR_FILEBROWSER);
  RNA_def_property_string_funcs(prop,
                                "rna_Sequence_proxy_filepath_get",
                                "rna_Sequence_proxy_filepath_length",
                                "rna_Sequence_proxy_filepath_set");

  RNA_def_property_update(prop, NC_SCENE | ND_SEQUENCER, "rna_SequenceProxy_update");

  prop = RNA_def_property(srna, "use_overwrite", PROP_BOOLEAN, PROP_NONE);
  RNA_def_property_boolean_negative_sdna(prop, nullptr, "build_flags", SEQ_PROXY_SKIP_EXISTING);
  RNA_def_property_ui_text(prop, "Overwrite", "Overwrite existing proxy files when building");

  prop = RNA_def_property(srna, "build_25", PROP_BOOLEAN, PROP_NONE);
  RNA_def_property_boolean_sdna(prop, nullptr, "build_size_flags", SEQ_PROXY_IMAGE_SIZE_25);
  RNA_def_property_ui_text(prop, "25%", "Build 25% proxy resolution");

  prop = RNA_def_property(srna, "build_50", PROP_BOOLEAN, PROP_NONE);
  RNA_def_property_boolean_sdna(prop, nullptr, "build_size_flags", SEQ_PROXY_IMAGE_SIZE_50);
  RNA_def_property_ui_text(prop, "50%", "Build 50% proxy resolution");

  prop = RNA_def_property(srna, "build_75", PROP_BOOLEAN, PROP_NONE);
  RNA_def_property_boolean_sdna(prop, nullptr, "build_size_flags", SEQ_PROXY_IMAGE_SIZE_75);
  RNA_def_property_ui_text(prop, "75%", "Build 75% proxy resolution");

  prop = RNA_def_property(srna, "build_100", PROP_BOOLEAN, PROP_NONE);
  RNA_def_property_boolean_sdna(prop, nullptr, "build_size_flags", SEQ_PROXY_IMAGE_SIZE_100);
  RNA_def_property_ui_text(prop, "100%", "Build 100% proxy resolution");

  prop = RNA_def_property(srna, "build_record_run", PROP_BOOLEAN, PROP_NONE);
  RNA_def_property_boolean_sdna(prop, nullptr, "build_tc_flags", SEQ_PROXY_TC_RECORD_RUN);
  RNA_def_property_ui_text(prop, "Rec Run", "Build record run time code index");

  prop = RNA_def_property(srna, "build_free_run", PROP_BOOLEAN, PROP_NONE);
  RNA_def_property_boolean_sdna(prop, nullptr, "build_tc_flags", SEQ_PROXY_TC_FREE_RUN);
  RNA_def_property_ui_text(prop, "Free Run", "Build free run time code index");

  prop = RNA_def_property(srna, "build_free_run_rec_date", PROP_BOOLEAN, PROP_NONE);
  RNA_def_property_boolean_sdna(
      prop, nullptr, "build_tc_flags", SEQ_PROXY_TC_INTERP_REC_DATE_FREE_RUN);
  RNA_def_property_ui_text(
      prop, "Free Run (Rec Date)", "Build free run time code index using Record Date/Time");

  prop = RNA_def_property(srna, "quality", PROP_INT, PROP_UNSIGNED);
  RNA_def_property_int_sdna(prop, nullptr, "quality");
  RNA_def_property_ui_text(prop, "Quality", "Quality of proxies to build");
  RNA_def_property_ui_range(prop, 1, 100, 1, -1);

  prop = RNA_def_property(srna, "timecode", PROP_ENUM, PROP_NONE);
  RNA_def_property_enum_sdna(prop, nullptr, "tc");
  RNA_def_property_enum_items(prop, seq_tc_items);
  RNA_def_property_ui_text(prop, "Timecode", "Method for reading the inputs timecode");
  RNA_def_property_update(prop, NC_SCENE | ND_SEQUENCER, "rna_Sequence_tcindex_update");

  prop = RNA_def_property(srna, "use_proxy_custom_directory", PROP_BOOLEAN, PROP_NONE);
  RNA_def_property_boolean_sdna(prop, nullptr, "storage", SEQ_STORAGE_PROXY_CUSTOM_DIR);
  RNA_def_property_ui_text(prop, "Proxy Custom Directory", "Use a custom directory to store data");
  RNA_def_property_update(
      prop, NC_SCENE | ND_SEQUENCER, "rna_Sequence_invalidate_preprocessed_update");

  prop = RNA_def_property(srna, "use_proxy_custom_file", PROP_BOOLEAN, PROP_NONE);
  RNA_def_property_boolean_sdna(prop, nullptr, "storage", SEQ_STORAGE_PROXY_CUSTOM_FILE);
  RNA_def_property_ui_text(prop, "Proxy Custom File", "Use a custom file to read proxy data from");
  RNA_def_property_update(
      prop, NC_SCENE | ND_SEQUENCER, "rna_Sequence_invalidate_preprocessed_update");
}

static void rna_def_color_balance(BlenderRNA *brna)
{
  StructRNA *srna;
  PropertyRNA *prop;

  static const EnumPropertyItem method_items[] = {
      {SEQ_COLOR_BALANCE_METHOD_LIFTGAMMAGAIN, "LIFT_GAMMA_GAIN", 0, "Lift/Gamma/Gain", ""},
      {SEQ_COLOR_BALANCE_METHOD_SLOPEOFFSETPOWER,
       "OFFSET_POWER_SLOPE",
       0,
       "Offset/Power/Slope (ASC-CDL)",
       "ASC-CDL standard color correction"},
      {0, nullptr, 0, nullptr, nullptr},
  };

  srna = RNA_def_struct(brna, "SequenceColorBalanceData", nullptr);
  RNA_def_struct_ui_text(srna,
                         "Sequence Color Balance Data",
                         "Color balance parameters for a sequence strip and its modifiers");
  RNA_def_struct_sdna(srna, "StripColorBalance");

  prop = RNA_def_property(srna, "correction_method", PROP_ENUM, PROP_NONE);
  RNA_def_property_enum_sdna(prop, nullptr, "method");
  RNA_def_property_enum_items(prop, method_items);
  RNA_def_property_ui_text(prop, "Correction Method", "");
  RNA_def_property_update(prop, NC_SCENE | ND_SEQUENCER, "rna_SequenceColorBalance_update");

  prop = RNA_def_property(srna, "lift", PROP_FLOAT, PROP_COLOR_GAMMA);
  RNA_def_property_ui_text(prop, "Lift", "Color balance lift (shadows)");
  RNA_def_property_ui_range(prop, 0, 2, 0.1, 3);
  RNA_def_property_float_default(prop, 1.0f);
  RNA_def_property_update(prop, NC_SCENE | ND_SEQUENCER, "rna_SequenceColorBalance_update");

  prop = RNA_def_property(srna, "gamma", PROP_FLOAT, PROP_COLOR_GAMMA);
  RNA_def_property_ui_text(prop, "Gamma", "Color balance gamma (midtones)");
  RNA_def_property_ui_range(prop, 0, 2, 0.1, 3);
  RNA_def_property_float_default(prop, 1.0f);
  RNA_def_property_update(prop, NC_SCENE | ND_SEQUENCER, "rna_SequenceColorBalance_update");

  prop = RNA_def_property(srna, "gain", PROP_FLOAT, PROP_COLOR_GAMMA);
  RNA_def_property_ui_text(prop, "Gain", "Color balance gain (highlights)");
  RNA_def_property_ui_range(prop, 0, 2, 0.1, 3);
  RNA_def_property_float_default(prop, 1.0f);
  RNA_def_property_update(prop, NC_SCENE | ND_SEQUENCER, "rna_SequenceColorBalance_update");

  prop = RNA_def_property(srna, "slope", PROP_FLOAT, PROP_COLOR_GAMMA);
  RNA_def_property_ui_text(prop, "Slope", "Correction for highlights");
  RNA_def_property_ui_range(prop, 0, 2, 0.1, 3);
  RNA_def_property_float_default(prop, 1.0f);
  RNA_def_property_update(prop, NC_SCENE | ND_SEQUENCER, "rna_SequenceColorBalance_update");

  prop = RNA_def_property(srna, "offset", PROP_FLOAT, PROP_COLOR_GAMMA);
  RNA_def_property_ui_text(prop, "Offset", "Correction for entire tonal range");
  RNA_def_property_ui_range(prop, 0, 2, 0.1, 3);
  RNA_def_property_float_default(prop, 1.0f);
  RNA_def_property_update(prop, NC_SCENE | ND_SEQUENCER, "rna_SequenceColorBalance_update");

  prop = RNA_def_property(srna, "power", PROP_FLOAT, PROP_COLOR_GAMMA);
  RNA_def_property_ui_text(prop, "Power", "Correction for midtones");
  RNA_def_property_ui_range(prop, 0, 2, 0.1, 3);
  RNA_def_property_float_default(prop, 1.0f);
  RNA_def_property_update(prop, NC_SCENE | ND_SEQUENCER, "rna_SequenceColorBalance_update");

  prop = RNA_def_property(srna, "invert_lift", PROP_BOOLEAN, PROP_NONE);
  RNA_def_property_boolean_sdna(prop, nullptr, "flag", SEQ_COLOR_BALANCE_INVERSE_LIFT);
  RNA_def_property_ui_text(prop, "Inverse Lift", "Invert the lift color");
  RNA_def_property_update(prop, NC_SCENE | ND_SEQUENCER, "rna_SequenceColorBalance_update");

  prop = RNA_def_property(srna, "invert_gamma", PROP_BOOLEAN, PROP_NONE);
  RNA_def_property_boolean_sdna(prop, nullptr, "flag", SEQ_COLOR_BALANCE_INVERSE_GAMMA);
  RNA_def_property_ui_text(prop, "Inverse Gamma", "Invert the gamma color");
  RNA_def_property_update(prop, NC_SCENE | ND_SEQUENCER, "rna_SequenceColorBalance_update");

  prop = RNA_def_property(srna, "invert_gain", PROP_BOOLEAN, PROP_NONE);
  RNA_def_property_boolean_sdna(prop, nullptr, "flag", SEQ_COLOR_BALANCE_INVERSE_GAIN);
  RNA_def_property_ui_text(prop, "Inverse Gain", "Invert the gain color");
  RNA_def_property_update(prop, NC_SCENE | ND_SEQUENCER, "rna_SequenceColorBalance_update");

  prop = RNA_def_property(srna, "invert_slope", PROP_BOOLEAN, PROP_NONE);
  RNA_def_property_boolean_sdna(prop, nullptr, "flag", SEQ_COLOR_BALANCE_INVERSE_SLOPE);
  RNA_def_property_ui_text(prop, "Inverse Slope", "Invert the slope color");
  RNA_def_property_update(prop, NC_SCENE | ND_SEQUENCER, "rna_SequenceColorBalance_update");

  prop = RNA_def_property(srna, "invert_offset", PROP_BOOLEAN, PROP_NONE);
  RNA_def_property_boolean_sdna(prop, nullptr, "flag", SEQ_COLOR_BALANCE_INVERSE_OFFSET);
  RNA_def_property_ui_text(prop, "Inverse Offset", "Invert the offset color");
  RNA_def_property_update(prop, NC_SCENE | ND_SEQUENCER, "rna_SequenceColorBalance_update");

  prop = RNA_def_property(srna, "invert_power", PROP_BOOLEAN, PROP_NONE);
  RNA_def_property_boolean_sdna(prop, nullptr, "flag", SEQ_COLOR_BALANCE_INVERSE_POWER);
  RNA_def_property_ui_text(prop, "Inverse Power", "Invert the power color");
  RNA_def_property_update(prop, NC_SCENE | ND_SEQUENCER, "rna_SequenceColorBalance_update");

  /* not yet used */
#  if 0
  prop = RNA_def_property(srna, "exposure", PROP_FLOAT, PROP_NONE);
  RNA_def_property_range(prop, 0.0f, 1.0f);
  RNA_def_property_ui_text(prop, "Exposure", "");
  RNA_def_property_update(prop, NC_SCENE | ND_SEQUENCER, "rna_Sequence_ColorBabalnce_update");

  prop = RNA_def_property(srna, "saturation", PROP_FLOAT, PROP_NONE);
  RNA_def_property_range(prop, 0.0f, 1.0f);
  RNA_def_property_ui_text(prop, "Saturation", "");
  RNA_def_property_update(prop, NC_SCENE | ND_SEQUENCER, "rna_Sequence_ColorBabalnce_update");
#  endif

  RNA_def_struct_path_func(srna, "rna_SequenceColorBalance_path");
}

static void rna_def_strip_color_balance(BlenderRNA *brna)
{
  StructRNA *srna;

  srna = RNA_def_struct(brna, "SequenceColorBalance", "SequenceColorBalanceData");
  RNA_def_struct_ui_text(
      srna, "Sequence Color Balance", "Color balance parameters for a sequence strip");
  RNA_def_struct_sdna(srna, "StripColorBalance");
}

static const EnumPropertyItem blend_mode_items[] = {
    {SEQ_BLEND_REPLACE, "REPLACE", 0, "Replace", ""},
    {SEQ_TYPE_CROSS, "CROSS", 0, "Cross", ""},
    RNA_ENUM_ITEM_SEPR,
    {SEQ_TYPE_DARKEN, "DARKEN", 0, "Darken", ""},
    {SEQ_TYPE_MUL, "MULTIPLY", 0, "Multiply", ""},
    {SEQ_TYPE_COLOR_BURN, "BURN", 0, "Color Burn", ""},
    {SEQ_TYPE_LINEAR_BURN, "LINEAR_BURN", 0, "Linear Burn", ""},
    RNA_ENUM_ITEM_SEPR,
    {SEQ_TYPE_LIGHTEN, "LIGHTEN", 0, "Lighten", ""},
    {SEQ_TYPE_SCREEN, "SCREEN", 0, "Screen", ""},
    {SEQ_TYPE_DODGE, "DODGE", 0, "Color Dodge", ""},
    {SEQ_TYPE_ADD, "ADD", 0, "Add", ""},
    RNA_ENUM_ITEM_SEPR,
    {SEQ_TYPE_OVERLAY, "OVERLAY", 0, "Overlay", ""},
    {SEQ_TYPE_SOFT_LIGHT, "SOFT_LIGHT", 0, "Soft Light", ""},
    {SEQ_TYPE_HARD_LIGHT, "HARD_LIGHT", 0, "Hard Light", ""},
    {SEQ_TYPE_VIVID_LIGHT, "VIVID_LIGHT", 0, "Vivid Light", ""},
    {SEQ_TYPE_LIN_LIGHT, "LINEAR_LIGHT", 0, "Linear Light", ""},
    {SEQ_TYPE_PIN_LIGHT, "PIN_LIGHT", 0, "Pin Light", ""},
    RNA_ENUM_ITEM_SEPR,
    {SEQ_TYPE_DIFFERENCE, "DIFFERENCE", 0, "Difference", ""},
    {SEQ_TYPE_EXCLUSION, "EXCLUSION", 0, "Exclusion", ""},
    {SEQ_TYPE_SUB, "SUBTRACT", 0, "Subtract", ""},
    RNA_ENUM_ITEM_SEPR,
    {SEQ_TYPE_HUE, "HUE", 0, "Hue", ""},
    {SEQ_TYPE_SATURATION, "SATURATION", 0, "Saturation", ""},
    {SEQ_TYPE_BLEND_COLOR, "COLOR", 0, "Color", ""},
    {SEQ_TYPE_VALUE, "VALUE", 0, "Value", ""},
    RNA_ENUM_ITEM_SEPR,
    {SEQ_TYPE_ALPHAOVER, "ALPHA_OVER", 0, "Alpha Over", ""},
    {SEQ_TYPE_ALPHAUNDER, "ALPHA_UNDER", 0, "Alpha Under", ""},
    {SEQ_TYPE_GAMCROSS, "GAMMA_CROSS", 0, "Gamma Cross", ""},
    {SEQ_TYPE_OVERDROP, "OVER_DROP", 0, "Over Drop", ""},
    {0, nullptr, 0, nullptr, nullptr},
};

static void rna_def_sequence_modifiers(BlenderRNA *brna, PropertyRNA *cprop)
{
  StructRNA *srna;

  FunctionRNA *func;
  PropertyRNA *parm;

  RNA_def_property_srna(cprop, "SequenceModifiers");
  srna = RNA_def_struct(brna, "SequenceModifiers", nullptr);
  RNA_def_struct_sdna(srna, "Sequence");
  RNA_def_struct_ui_text(srna, "Strip Modifiers", "Collection of strip modifiers");

  /* add modifier */
  func = RNA_def_function(srna, "new", "rna_Sequence_modifier_new");
  RNA_def_function_flag(func, FUNC_USE_CONTEXT | FUNC_USE_REPORTS);
  RNA_def_function_ui_description(func, "Add a new modifier");
  parm = RNA_def_string(func, "name", "Name", 0, "", "New name for the modifier");
  RNA_def_parameter_flags(parm, PropertyFlag(0), PARM_REQUIRED);
  /* modifier to add */
  parm = RNA_def_enum(func,
                      "type",
                      rna_enum_sequence_modifier_type_items,
                      seqModifierType_ColorBalance,
                      "",
                      "Modifier type to add");
  RNA_def_parameter_flags(parm, PropertyFlag(0), PARM_REQUIRED);
  /* return type */
  parm = RNA_def_pointer(func, "modifier", "SequenceModifier", "", "Newly created modifier");
  RNA_def_function_return(func, parm);

  /* remove modifier */
  func = RNA_def_function(srna, "remove", "rna_Sequence_modifier_remove");
  RNA_def_function_flag(func, FUNC_USE_CONTEXT | FUNC_USE_REPORTS);
  RNA_def_function_ui_description(func, "Remove an existing modifier from the sequence");
  /* modifier to remove */
  parm = RNA_def_pointer(func, "modifier", "SequenceModifier", "", "Modifier to remove");
  RNA_def_parameter_flags(parm, PROP_NEVER_NULL, PARM_REQUIRED | PARM_RNAPTR);
  RNA_def_parameter_clear_flags(parm, PROP_THICK_WRAP, ParameterFlag(0));

  /* clear all modifiers */
  func = RNA_def_function(srna, "clear", "rna_Sequence_modifier_clear");
  RNA_def_function_flag(func, FUNC_USE_CONTEXT);
  RNA_def_function_ui_description(func, "Remove all modifiers from the sequence");
}

static void rna_def_sequence(BlenderRNA *brna)
{
  StructRNA *srna;
  PropertyRNA *prop;

  static const EnumPropertyItem seq_type_items[] = {
      {SEQ_TYPE_IMAGE, "IMAGE", 0, "Image", ""},
      {SEQ_TYPE_META, "META", 0, "Meta", ""},
      {SEQ_TYPE_SCENE, "SCENE", 0, "Scene", ""},
      {SEQ_TYPE_MOVIE, "MOVIE", 0, "Movie", ""},
      {SEQ_TYPE_MOVIECLIP, "MOVIECLIP", 0, "Clip", ""},
      {SEQ_TYPE_MASK, "MASK", 0, "Mask", ""},
      {SEQ_TYPE_SOUND_RAM, "SOUND", 0, "Sound", ""},
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

  srna = RNA_def_struct(brna, "Sequence", nullptr);
  RNA_def_struct_ui_text(srna, "Sequence", "Sequence strip in the sequence editor");
  RNA_def_struct_refine_func(srna, "rna_Sequence_refine");
  RNA_def_struct_path_func(srna, "rna_Sequence_path");
  RNA_def_struct_idprops_func(srna, "rna_Sequence_idprops");

  prop = RNA_def_property(srna, "name", PROP_STRING, PROP_NONE);
  RNA_def_property_string_funcs(
      prop, "rna_Sequence_name_get", "rna_Sequence_name_length", "rna_Sequence_name_set");
  RNA_def_property_string_maxlength(prop, sizeof(Sequence::name) - 2);
  RNA_def_property_ui_text(prop, "Name", "");
  RNA_def_struct_name_property(srna, prop);
  RNA_def_property_update(prop, NC_SCENE | ND_SEQUENCER, nullptr);

  prop = RNA_def_property(srna, "type", PROP_ENUM, PROP_NONE);
  RNA_def_property_clear_flag(prop, PROP_EDITABLE);
  RNA_def_property_enum_items(prop, seq_type_items);
  RNA_def_property_ui_text(prop, "Type", "");
  RNA_def_property_translation_context(prop, BLT_I18NCONTEXT_ID_SEQUENCE);
  RNA_def_property_update(prop, NC_SCENE | ND_SEQUENCER, "rna_Sequence_invalidate_raw_update");

  /* flags */
  prop = RNA_def_property(srna, "select", PROP_BOOLEAN, PROP_NONE);
  RNA_def_property_boolean_sdna(prop, nullptr, "flag", SELECT);
  RNA_def_property_ui_text(prop, "Select", "");
  RNA_def_property_update(prop, NC_SCENE | ND_SEQUENCER | NA_SELECTED, nullptr);

  prop = RNA_def_property(srna, "select_left_handle", PROP_BOOLEAN, PROP_NONE);
  RNA_def_property_boolean_sdna(prop, nullptr, "flag", SEQ_LEFTSEL);
  RNA_def_property_ui_text(prop, "Left Handle Selected", "");
  RNA_def_property_update(prop, NC_SCENE | ND_SEQUENCER | NA_SELECTED, nullptr);

  prop = RNA_def_property(srna, "select_right_handle", PROP_BOOLEAN, PROP_NONE);
  RNA_def_property_boolean_sdna(prop, nullptr, "flag", SEQ_RIGHTSEL);
  RNA_def_property_ui_text(prop, "Right Handle Selected", "");
  RNA_def_property_update(prop, NC_SCENE | ND_SEQUENCER | NA_SELECTED, nullptr);

  prop = RNA_def_property(srna, "mute", PROP_BOOLEAN, PROP_NONE);
  RNA_def_property_boolean_sdna(prop, nullptr, "flag", SEQ_MUTE);
  RNA_def_property_ui_icon(prop, ICON_CHECKBOX_HLT, -1);
  RNA_def_property_ui_text(
      prop, "Mute", "Disable strip so that it cannot be viewed in the output");
  RNA_def_property_update(prop, NC_SCENE | ND_SEQUENCER, "rna_Sequence_invalidate_raw_update");

  prop = RNA_def_property(srna, "lock", PROP_BOOLEAN, PROP_NONE);
  RNA_def_property_boolean_sdna(prop, nullptr, "flag", SEQ_LOCK);
  RNA_def_property_clear_flag(prop, PROP_ANIMATABLE);
  RNA_def_property_ui_icon(prop, ICON_UNLOCKED, true);
  RNA_def_property_ui_text(prop, "Lock", "Lock strip so that it cannot be transformed");
  RNA_def_property_update(prop, NC_SCENE | ND_SEQUENCER, nullptr);

  /* strip positioning */
  /* Cache has to be invalidated before and after transformation. */
  prop = RNA_def_property(srna, "frame_final_duration", PROP_INT, PROP_TIME);
  RNA_def_property_range(prop, 1, MAXFRAME);
  RNA_def_property_clear_flag(prop, PROP_ANIMATABLE);
  RNA_def_property_ui_text(
      prop, "Length", "The length of the contents of this strip after the handles are applied");
  RNA_def_property_int_funcs(
      prop, "rna_Sequence_frame_length_get", "rna_Sequence_frame_length_set", nullptr);
  RNA_def_property_editable_func(prop, "rna_Sequence_frame_editable");
  RNA_def_property_update(
      prop, NC_SCENE | ND_SEQUENCER, "rna_Sequence_invalidate_preprocessed_update");

  prop = RNA_def_property(srna, "frame_duration", PROP_INT, PROP_TIME);
  RNA_def_property_int_sdna(prop, nullptr, "len");
  RNA_def_property_clear_flag(prop, PROP_EDITABLE | PROP_ANIMATABLE);
  RNA_def_property_range(prop, 1, MAXFRAME);
  RNA_def_property_ui_text(
      prop, "Length", "The length of the contents of this strip before the handles are applied");

  prop = RNA_def_property(srna, "frame_start", PROP_FLOAT, PROP_TIME);
  RNA_def_property_float_sdna(prop, nullptr, "start");
  RNA_def_property_clear_flag(prop, PROP_ANIMATABLE);
  RNA_def_property_ui_text(prop, "Start Frame", "X position where the strip begins");
  RNA_def_property_ui_range(prop, MINFRAME, MAXFRAME, 100.0f, 0);
  RNA_def_property_float_funcs(prop,
                               nullptr,
                               "rna_Sequence_start_frame_set",
                               nullptr); /* overlap tests and calc_seq_disp */
  RNA_def_property_editable_func(prop, "rna_Sequence_frame_editable");
  RNA_def_property_update(
      prop, NC_SCENE | ND_SEQUENCER, "rna_Sequence_invalidate_preprocessed_update");

  prop = RNA_def_property(srna, "frame_final_start", PROP_INT, PROP_TIME);
  RNA_def_property_int_sdna(prop, nullptr, "startdisp");
  RNA_def_property_int_funcs(
      prop, "rna_Sequence_frame_final_start_get", "rna_Sequence_start_frame_final_set", nullptr);
  RNA_def_property_editable_func(prop, "rna_Sequence_frame_editable");
  RNA_def_property_clear_flag(prop, PROP_ANIMATABLE);
  RNA_def_property_ui_text(
      prop,
      "Start Frame",
      "Start frame displayed in the sequence editor after offsets are applied, setting this is "
      "equivalent to moving the handle, not the actual start frame");
  /* overlap tests and calc_seq_disp */
  RNA_def_property_update(
      prop, NC_SCENE | ND_SEQUENCER, "rna_Sequence_invalidate_preprocessed_update");

  prop = RNA_def_property(srna, "frame_final_end", PROP_INT, PROP_TIME);
  RNA_def_property_int_sdna(prop, nullptr, "enddisp");
  RNA_def_property_int_funcs(
      prop, "rna_Sequence_frame_final_end_get", "rna_Sequence_end_frame_final_set", nullptr);
  RNA_def_property_editable_func(prop, "rna_Sequence_frame_editable");
  RNA_def_property_clear_flag(prop, PROP_ANIMATABLE);
  RNA_def_property_ui_text(
      prop, "End Frame", "End frame displayed in the sequence editor after offsets are applied");
  /* overlap tests and calc_seq_disp */
  RNA_def_property_update(
      prop, NC_SCENE | ND_SEQUENCER, "rna_Sequence_invalidate_preprocessed_update");

  prop = RNA_def_property(srna, "frame_offset_start", PROP_FLOAT, PROP_TIME);
  RNA_def_property_float_sdna(prop, nullptr, "startofs");
  //  RNA_def_property_clear_flag(prop, PROP_EDITABLE); /* overlap tests */
  RNA_def_property_ui_text(prop, "Start Offset", "");
  RNA_def_property_ui_range(prop, MINFRAME, MAXFRAME, 100.0f, 0);
  RNA_def_property_float_funcs(prop,
                               nullptr,
                               "rna_Sequence_frame_offset_start_set",
                               "rna_Sequence_frame_offset_start_range");
  RNA_def_property_update(prop, NC_SCENE | ND_SEQUENCER, "rna_Sequence_frame_change_update");

  prop = RNA_def_property(srna, "frame_offset_end", PROP_FLOAT, PROP_TIME);
  RNA_def_property_float_sdna(prop, nullptr, "endofs");
  //  RNA_def_property_clear_flag(prop, PROP_EDITABLE); /* overlap tests */
  RNA_def_property_ui_text(prop, "End Offset", "");
  RNA_def_property_ui_range(prop, MINFRAME, MAXFRAME, 100.0f, 0);
  RNA_def_property_float_funcs(
      prop, nullptr, "rna_Sequence_frame_offset_end_set", "rna_Sequence_frame_offset_end_range");
  RNA_def_property_update(prop, NC_SCENE | ND_SEQUENCER, "rna_Sequence_frame_change_update");

  prop = RNA_def_property(srna, "channel", PROP_INT, PROP_UNSIGNED);
  RNA_def_property_int_sdna(prop, nullptr, "machine");
  RNA_def_property_clear_flag(prop, PROP_ANIMATABLE);
  RNA_def_property_range(prop, 1, MAXSEQ);
  RNA_def_property_ui_text(prop, "Channel", "Y position of the sequence strip");
  RNA_def_property_int_funcs(
      prop, nullptr, "rna_Sequence_channel_set", nullptr); /* overlap test */
  RNA_def_property_update(
      prop, NC_SCENE | ND_SEQUENCER, "rna_Sequence_invalidate_preprocessed_update");

  prop = RNA_def_property(srna, "use_linear_modifiers", PROP_BOOLEAN, PROP_NONE);
  RNA_def_property_boolean_sdna(prop, nullptr, "flag", SEQ_USE_LINEAR_MODIFIERS);
  RNA_def_property_ui_text(prop,
                           "Use Linear Modifiers",
                           "Calculate modifiers in linear space instead of sequencer's space");
  RNA_def_property_update(
      prop, NC_SCENE | ND_SEQUENCER, "rna_Sequence_invalidate_preprocessed_update");

  /* blending */

  prop = RNA_def_property(srna, "blend_type", PROP_ENUM, PROP_NONE);
  RNA_def_property_enum_sdna(prop, nullptr, "blend_mode");
  RNA_def_property_enum_items(prop, blend_mode_items);
  RNA_def_property_ui_text(
      prop, "Blending Mode", "Method for controlling how the strip combines with other strips");
  RNA_def_property_update(
      prop, NC_SCENE | ND_SEQUENCER, "rna_Sequence_invalidate_preprocessed_update");

  prop = RNA_def_property(srna, "blend_alpha", PROP_FLOAT, PROP_FACTOR);
  RNA_def_property_range(prop, 0.0f, 1.0f);
  RNA_def_property_ui_text(
      prop, "Blend Opacity", "Percentage of how much the strip's colors affect other strips");
  /* stupid 0-100 -> 0-1 */
  RNA_def_property_float_funcs(
      prop, "rna_Sequence_opacity_get", "rna_Sequence_opacity_set", nullptr);
  RNA_def_property_update(
      prop, NC_SCENE | ND_SEQUENCER, "rna_Sequence_invalidate_preprocessed_update");

  prop = RNA_def_property(srna, "effect_fader", PROP_FLOAT, PROP_FACTOR);
  RNA_def_property_range(prop, 0.0f, 1.0f);
  RNA_def_property_ui_range(prop, 0.0f, 1.0f, 0.1, 3);
  RNA_def_property_float_sdna(prop, nullptr, "effect_fader");
  RNA_def_property_ui_text(prop, "Effect Fader Position", "Custom fade value");
  RNA_def_property_update(
      prop, NC_SCENE | ND_SEQUENCER, "rna_Sequence_invalidate_preprocessed_update");

  prop = RNA_def_property(srna, "use_default_fade", PROP_BOOLEAN, PROP_NONE);
  RNA_def_property_boolean_sdna(prop, nullptr, "flag", SEQ_USE_EFFECT_DEFAULT_FADE);
  RNA_def_property_ui_text(
      prop,
      "Use Default Fade",
      "Fade effect using the built-in default (usually make transition as long as "
      "effect strip)");
  RNA_def_property_update(
      prop, NC_SCENE | ND_SEQUENCER, "rna_Sequence_invalidate_preprocessed_update");

  prop = RNA_def_property(srna, "color_tag", PROP_ENUM, PROP_NONE);
  RNA_def_property_enum_sdna(prop, nullptr, "color_tag");
  RNA_def_property_enum_funcs(
      prop, "rna_Sequence_color_tag_get", "rna_Sequence_color_tag_set", nullptr);
  RNA_def_property_enum_items(prop, rna_enum_strip_color_items);
  RNA_def_property_ui_text(prop, "Strip Color", "Color tag for a strip");
  RNA_def_property_update(prop, NC_SCENE | ND_SEQUENCER, nullptr);

  /* modifiers */
  prop = RNA_def_property(srna, "modifiers", PROP_COLLECTION, PROP_NONE);
  RNA_def_property_struct_type(prop, "SequenceModifier");
  RNA_def_property_ui_text(prop, "Modifiers", "Modifiers affecting this strip");
  rna_def_sequence_modifiers(brna, prop);

  prop = RNA_def_property(srna, "use_cache_raw", PROP_BOOLEAN, PROP_NONE);
  RNA_def_property_boolean_sdna(prop, nullptr, "cache_flag", SEQ_CACHE_STORE_RAW);
  RNA_def_property_ui_text(prop,
                           "Cache Raw",
                           "Cache raw images read from disk, for faster tweaking of strip "
                           "parameters at the cost of memory usage");

  prop = RNA_def_property(srna, "use_cache_preprocessed", PROP_BOOLEAN, PROP_NONE);
  RNA_def_property_boolean_sdna(prop, nullptr, "cache_flag", SEQ_CACHE_STORE_PREPROCESSED);
  RNA_def_property_ui_text(
      prop,
      "Cache Preprocessed",
      "Cache preprocessed images, for faster tweaking of effects at the cost of memory usage");

  prop = RNA_def_property(srna, "use_cache_composite", PROP_BOOLEAN, PROP_NONE);
  RNA_def_property_boolean_sdna(prop, nullptr, "cache_flag", SEQ_CACHE_STORE_COMPOSITE);
  RNA_def_property_ui_text(prop,
                           "Cache Composite",
                           "Cache intermediate composited images, for faster tweaking of stacked "
                           "strips at the cost of memory usage");

  prop = RNA_def_property(srna, "override_cache_settings", PROP_BOOLEAN, PROP_NONE);
  RNA_def_property_boolean_sdna(prop, nullptr, "cache_flag", SEQ_CACHE_OVERRIDE);
  RNA_def_property_ui_text(prop, "Override Cache Settings", "Override global cache settings");

  prop = RNA_def_property(srna, "show_retiming_keys", PROP_BOOLEAN, PROP_NONE);
  RNA_def_property_boolean_sdna(prop, nullptr, "flag", SEQ_SHOW_RETIMING);
  RNA_def_property_ui_text(prop, "Show Retiming Keys", "Show retiming keys, so they can be moved");

  RNA_api_sequence_strip(srna);
}

static void rna_def_channel(BlenderRNA *brna)
{
  StructRNA *srna;
  PropertyRNA *prop;

  srna = RNA_def_struct(brna, "SequenceTimelineChannel", nullptr);
  RNA_def_struct_sdna(srna, "SeqTimelineChannel");
  RNA_def_struct_path_func(srna, "rna_SeqTimelineChannel_path");
  RNA_def_struct_ui_text(srna, "Channel", "");

  prop = RNA_def_property(srna, "name", PROP_STRING, PROP_NONE);
  RNA_def_property_string_maxlength(prop, sizeof(SeqTimelineChannel::name));
  RNA_def_property_ui_text(prop, "Name", "");
  RNA_def_struct_name_property(srna, prop);
  RNA_def_property_string_funcs(prop, nullptr, nullptr, "rna_SequenceTimelineChannel_name_set");
  RNA_def_property_update(prop, NC_SCENE | ND_SEQUENCER, nullptr);

  prop = RNA_def_property(srna, "lock", PROP_BOOLEAN, PROP_NONE);
  RNA_def_property_boolean_sdna(prop, nullptr, "flag", SEQ_CHANNEL_LOCK);
  RNA_def_property_ui_text(prop, "Lock channel", "");
  RNA_def_property_update(prop, NC_SCENE | ND_SEQUENCER, nullptr);

  prop = RNA_def_property(srna, "mute", PROP_BOOLEAN, PROP_NONE);
  RNA_def_property_boolean_sdna(prop, nullptr, "flag", SEQ_CHANNEL_MUTE);
  RNA_def_property_ui_text(prop, "Mute channel", "");
  RNA_def_property_update(
      prop, NC_SCENE | ND_SEQUENCER, "rna_SequenceTimelineChannel_mute_update");
}

static void rna_def_editor(BlenderRNA *brna)
{
  StructRNA *srna;
  FunctionRNA *func;
  PropertyRNA *parm;
  PropertyRNA *prop;

  static const EnumPropertyItem editing_storage_items[] = {
      {0, "PER_STRIP", 0, "Per Strip", "Store proxies using per strip settings"},
      {SEQ_EDIT_PROXY_DIR_STORAGE,
       "PROJECT",
       0,
       "Project",
       "Store proxies using project directory"},
      {0, nullptr, 0, nullptr, nullptr},
  };
  srna = RNA_def_struct(brna, "SequenceEditor", nullptr);
  RNA_def_struct_ui_text(srna, "Sequence Editor", "Sequence editing data for a Scene data-block");
  RNA_def_struct_ui_icon(srna, ICON_SEQUENCE);
  RNA_def_struct_sdna(srna, "Editing");

  prop = RNA_def_property(srna, "sequences", PROP_COLLECTION, PROP_NONE);
  RNA_def_property_collection_sdna(prop, nullptr, "seqbase", nullptr);
  RNA_def_property_struct_type(prop, "Sequence");
  RNA_def_property_ui_text(prop, "Sequences", "Top-level strips only");
  RNA_api_sequences(brna, prop, false);

  prop = RNA_def_property(srna, "sequences_all", PROP_COLLECTION, PROP_NONE);
  RNA_def_property_collection_sdna(prop, nullptr, "seqbase", nullptr);
  RNA_def_property_struct_type(prop, "Sequence");
  RNA_def_property_ui_text(
      prop, "All Sequences", "All strips, recursively including those inside metastrips");
  RNA_def_property_collection_funcs(prop,
                                    "rna_SequenceEditor_sequences_all_begin",
                                    "rna_SequenceEditor_sequences_all_next",
                                    "rna_SequenceEditor_sequences_all_end",
                                    "rna_SequenceEditor_sequences_all_get",
                                    nullptr,
                                    nullptr,
                                    "rna_SequenceEditor_sequences_all_lookup_string",
                                    nullptr);

  prop = RNA_def_property(srna, "meta_stack", PROP_COLLECTION, PROP_NONE);
  RNA_def_property_collection_sdna(prop, nullptr, "metastack", nullptr);
  RNA_def_property_struct_type(prop, "Sequence");
  RNA_def_property_ui_text(
      prop, "Meta Stack", "Meta strip stack, last is currently edited meta strip");
  RNA_def_property_collection_funcs(prop,
                                    nullptr,
                                    nullptr,
                                    nullptr,
                                    "rna_SequenceEditor_meta_stack_get",
                                    nullptr,
                                    nullptr,
                                    nullptr,
                                    nullptr);

  prop = RNA_def_property(srna, "channels", PROP_COLLECTION, PROP_NONE);
  RNA_def_property_collection_sdna(prop, nullptr, "channels", nullptr);
  RNA_def_property_struct_type(prop, "SequenceTimelineChannel");
  RNA_def_property_ui_text(prop, "Channels", "");

  prop = RNA_def_property(srna, "active_strip", PROP_POINTER, PROP_NONE);
  RNA_def_property_pointer_sdna(prop, nullptr, "act_seq");
  RNA_def_property_flag(prop, PROP_EDITABLE);
  RNA_def_property_ui_text(prop, "Active Strip", "Sequencer's active strip");

  prop = RNA_def_property(srna, "selected_retiming_keys", PROP_BOOLEAN, PROP_NONE);
  RNA_def_property_ui_text(prop, "Retiming Key Selection Status", "");
  RNA_def_property_boolean_funcs(prop, "rna_SequenceEditor_selected_retiming_key_get", nullptr);
  RNA_def_property_clear_flag(prop, PROP_EDITABLE);

  prop = RNA_def_property(srna, "show_overlay_frame", PROP_BOOLEAN, PROP_NONE);
  RNA_def_property_boolean_sdna(prop, nullptr, "overlay_frame_flag", SEQ_EDIT_OVERLAY_FRAME_SHOW);
  RNA_def_property_ui_text(
      prop, "Show Overlay", "Partial overlay on top of the sequencer with a frame offset");
  RNA_def_property_update(prop, NC_SPACE | ND_SPACE_SEQUENCER, nullptr);

  prop = RNA_def_property(srna, "use_overlay_frame_lock", PROP_BOOLEAN, PROP_NONE);
  RNA_def_property_boolean_sdna(prop, nullptr, "overlay_frame_flag", SEQ_EDIT_OVERLAY_FRAME_ABS);
  RNA_def_property_ui_text(prop, "Overlay Lock", "");
  RNA_def_property_boolean_funcs(prop, nullptr, "rna_SequenceEditor_overlay_lock_set");
  RNA_def_property_update(prop, NC_SPACE | ND_SPACE_SEQUENCER, nullptr);

  /* access to fixed and relative frame */
  prop = RNA_def_property(srna, "overlay_frame", PROP_INT, PROP_NONE);
  RNA_def_property_ui_text(prop, "Overlay Offset", "Number of frames to offset");
  RNA_def_property_int_funcs(prop,
                             "rna_SequenceEditor_overlay_frame_get",
                             "rna_SequenceEditor_overlay_frame_set",
                             nullptr);
  RNA_def_property_update(prop, NC_SPACE | ND_SPACE_SEQUENCER, nullptr);

  prop = RNA_def_property(srna, "proxy_storage", PROP_ENUM, PROP_NONE);
  RNA_def_property_enum_items(prop, editing_storage_items);
  RNA_def_property_ui_text(prop, "Proxy Storage", "How to store proxies for this project");
  RNA_def_property_translation_context(prop, BLT_I18NCONTEXT_ID_SEQUENCE);
  RNA_def_property_update(prop, NC_SPACE | ND_SPACE_SEQUENCER, "rna_SequenceEditor_update_cache");

  prop = RNA_def_property(srna, "proxy_dir", PROP_STRING, PROP_DIRPATH);
  RNA_def_property_string_sdna(prop, nullptr, "proxy_dir");
  RNA_def_property_ui_text(prop, "Proxy Directory", "");
  RNA_def_property_update(prop, NC_SPACE | ND_SPACE_SEQUENCER, "rna_SequenceEditor_update_cache");

  /* cache flags */

  prop = RNA_def_property(srna, "show_cache", PROP_BOOLEAN, PROP_NONE);
  RNA_def_property_boolean_sdna(prop, nullptr, "cache_flag", SEQ_CACHE_VIEW_ENABLE);
  RNA_def_property_ui_text(prop, "Show Cache", "Visualize cached images on the timeline");
  RNA_def_property_update(prop, NC_SCENE | ND_SEQUENCER, nullptr);

  prop = RNA_def_property(srna, "show_cache_final_out", PROP_BOOLEAN, PROP_NONE);
  RNA_def_property_boolean_sdna(prop, nullptr, "cache_flag", SEQ_CACHE_VIEW_FINAL_OUT);
  RNA_def_property_ui_text(prop, "Final Images", "Visualize cached complete frames");
  RNA_def_property_update(prop, NC_SCENE | ND_SEQUENCER, nullptr);

  prop = RNA_def_property(srna, "show_cache_raw", PROP_BOOLEAN, PROP_NONE);
  RNA_def_property_boolean_sdna(prop, nullptr, "cache_flag", SEQ_CACHE_VIEW_RAW);
  RNA_def_property_ui_text(prop, "Raw Images", "Visualize cached raw images");
  RNA_def_property_update(prop, NC_SCENE | ND_SEQUENCER, nullptr);

  prop = RNA_def_property(srna, "show_cache_preprocessed", PROP_BOOLEAN, PROP_NONE);
  RNA_def_property_boolean_sdna(prop, nullptr, "cache_flag", SEQ_CACHE_VIEW_PREPROCESSED);
  RNA_def_property_ui_text(prop, "Preprocessed Images", "Visualize cached pre-processed images");
  RNA_def_property_update(prop, NC_SCENE | ND_SEQUENCER, nullptr);

  prop = RNA_def_property(srna, "show_cache_composite", PROP_BOOLEAN, PROP_NONE);
  RNA_def_property_boolean_sdna(prop, nullptr, "cache_flag", SEQ_CACHE_VIEW_COMPOSITE);
  RNA_def_property_ui_text(prop, "Composite Images", "Visualize cached composite images");
  RNA_def_property_update(prop, NC_SCENE | ND_SEQUENCER, nullptr);

  prop = RNA_def_property(srna, "use_cache_raw", PROP_BOOLEAN, PROP_NONE);
  RNA_def_property_boolean_sdna(prop, nullptr, "cache_flag", SEQ_CACHE_STORE_RAW);
  RNA_def_property_ui_text(prop,
                           "Cache Raw",
                           "Cache raw images read from disk, for faster tweaking of strip "
                           "parameters at the cost of memory usage");

  prop = RNA_def_property(srna, "use_cache_preprocessed", PROP_BOOLEAN, PROP_NONE);
  RNA_def_property_boolean_sdna(prop, nullptr, "cache_flag", SEQ_CACHE_STORE_PREPROCESSED);
  RNA_def_property_ui_text(
      prop,
      "Cache Preprocessed",
      "Cache preprocessed images, for faster tweaking of effects at the cost of memory usage");

  prop = RNA_def_property(srna, "use_cache_composite", PROP_BOOLEAN, PROP_NONE);
  RNA_def_property_boolean_sdna(prop, nullptr, "cache_flag", SEQ_CACHE_STORE_COMPOSITE);
  RNA_def_property_ui_text(prop,
                           "Cache Composite",
                           "Cache intermediate composited images, for faster tweaking of stacked "
                           "strips at the cost of memory usage");

  prop = RNA_def_property(srna, "use_cache_final", PROP_BOOLEAN, PROP_NONE);
  RNA_def_property_boolean_sdna(prop, nullptr, "cache_flag", SEQ_CACHE_STORE_FINAL_OUT);
  RNA_def_property_ui_text(prop, "Cache Final", "Cache final image for each frame");

  prop = RNA_def_property(srna, "use_prefetch", PROP_BOOLEAN, PROP_NONE);
  RNA_def_property_boolean_sdna(prop, nullptr, "cache_flag", SEQ_CACHE_PREFETCH_ENABLE);
  RNA_def_property_ui_text(
      prop,
      "Prefetch Frames",
      "Render frames ahead of current frame in the background for faster playback");
  RNA_def_property_update(prop, NC_SCENE | ND_SEQUENCER, nullptr);

  /* functions */

  func = RNA_def_function(srna, "display_stack", "rna_SequenceEditor_display_stack");
  RNA_def_function_flag(func, FUNC_USE_SELF_ID | FUNC_USE_REPORTS);
  RNA_def_function_ui_description(func, "Display sequences stack");
  parm = RNA_def_pointer(
      func, "meta_sequence", "Sequence", "Meta Sequence", "Meta to display its stack");
  RNA_def_parameter_flags(parm, PropertyFlag(0), PARM_REQUIRED);
}

static void rna_def_filter_video(StructRNA *srna)
{
  PropertyRNA *prop;

  static const EnumPropertyItem alpha_mode_items[] = {
      {SEQ_ALPHA_STRAIGHT,
       "STRAIGHT",
       0,
       "Straight",
       "RGB channels in transparent pixels are unaffected by the alpha channel"},
      {SEQ_ALPHA_PREMUL,
       "PREMUL",
       0,
       "Premultiplied",
       "RGB channels in transparent pixels are multiplied by the alpha channel"},
      {0, nullptr, 0, nullptr, nullptr},
  };

  prop = RNA_def_property(srna, "use_deinterlace", PROP_BOOLEAN, PROP_NONE);
  RNA_def_property_boolean_sdna(prop, nullptr, "flag", SEQ_FILTERY);
  RNA_def_property_ui_text(prop, "Deinterlace", "Remove fields from video movies");
  RNA_def_property_update(prop, NC_SCENE | ND_SEQUENCER, "rna_Sequence_reopen_files_update");

  prop = RNA_def_property(srna, "alpha_mode", PROP_ENUM, PROP_NONE);
  RNA_def_property_enum_items(prop, alpha_mode_items);
  RNA_def_property_ui_text(
      prop, "Alpha Mode", "Representation of alpha information in the RGBA pixels");
  RNA_def_property_update(prop, NC_SCENE | ND_SEQUENCER, "rna_Sequence_invalidate_raw_update");

  prop = RNA_def_property(srna, "use_flip_x", PROP_BOOLEAN, PROP_NONE);
  RNA_def_property_boolean_sdna(prop, nullptr, "flag", SEQ_FLIPX);
  RNA_def_property_ui_text(prop, "Flip X", "Flip on the X axis");
  RNA_def_property_update(prop, NC_SCENE | ND_SEQUENCER, "rna_Sequence_invalidate_raw_update");

  prop = RNA_def_property(srna, "use_flip_y", PROP_BOOLEAN, PROP_NONE);
  RNA_def_property_boolean_sdna(prop, nullptr, "flag", SEQ_FLIPY);
  RNA_def_property_ui_text(prop, "Flip Y", "Flip on the Y axis");
  RNA_def_property_update(prop, NC_SCENE | ND_SEQUENCER, "rna_Sequence_invalidate_raw_update");

  prop = RNA_def_property(srna, "use_float", PROP_BOOLEAN, PROP_NONE);
  RNA_def_property_boolean_sdna(prop, nullptr, "flag", SEQ_MAKE_FLOAT);
  RNA_def_property_ui_text(prop, "Convert Float", "Convert input to float data");
  RNA_def_property_update(prop, NC_SCENE | ND_SEQUENCER, "rna_Sequence_invalidate_raw_update");

  prop = RNA_def_property(srna, "use_reverse_frames", PROP_BOOLEAN, PROP_NONE);
  RNA_def_property_boolean_sdna(prop, nullptr, "flag", SEQ_REVERSE_FRAMES);
  RNA_def_property_ui_text(prop, "Reverse Frames", "Reverse frame order");
  RNA_def_property_update(
      prop, NC_SCENE | ND_SEQUENCER, "rna_Sequence_invalidate_preprocessed_update");

  prop = RNA_def_property(srna, "color_multiply", PROP_FLOAT, PROP_UNSIGNED);
  RNA_def_property_float_sdna(prop, nullptr, "mul");
  RNA_def_property_range(prop, 0.0f, 20.0f);
  RNA_def_property_float_default(prop, 1.0f);
  RNA_def_property_ui_text(prop, "Multiply Colors", "");
  RNA_def_property_update(
      prop, NC_SCENE | ND_SEQUENCER, "rna_Sequence_invalidate_preprocessed_update");

  prop = RNA_def_property(srna, "multiply_alpha", PROP_BOOLEAN, PROP_NONE);
  RNA_def_property_boolean_sdna(prop, nullptr, "flag", SEQ_MULTIPLY_ALPHA);
  RNA_def_property_ui_text(prop, "Multiply Alpha", "Multiply alpha along with color channels");
  RNA_def_property_update(prop, NC_SCENE | ND_SEQUENCER, "rna_Sequence_invalidate_raw_update");

  prop = RNA_def_property(srna, "color_saturation", PROP_FLOAT, PROP_UNSIGNED);
  RNA_def_property_float_sdna(prop, nullptr, "sat");
  RNA_def_property_range(prop, 0.0f, 20.0f);
  RNA_def_property_ui_range(prop, 0.0f, 2.0f, 3, 3);
  RNA_def_property_float_default(prop, 1.0f);
  RNA_def_property_ui_text(prop, "Saturation", "Adjust the intensity of the input's color");
  RNA_def_property_update(
      prop, NC_SCENE | ND_SEQUENCER, "rna_Sequence_invalidate_preprocessed_update");

  prop = RNA_def_property(srna, "strobe", PROP_FLOAT, PROP_NONE);
  RNA_def_property_range(prop, 1.0f, 30.0f);
  RNA_def_property_ui_text(prop, "Strobe", "Only display every nth frame");
  RNA_def_property_update(
      prop, NC_SCENE | ND_SEQUENCER, "rna_Sequence_invalidate_preprocessed_update");

  prop = RNA_def_property(srna, "transform", PROP_POINTER, PROP_NONE);
  RNA_def_property_pointer_sdna(prop, nullptr, "strip->transform");
  RNA_def_property_ui_text(prop, "Transform", "");

  prop = RNA_def_property(srna, "crop", PROP_POINTER, PROP_NONE);
  RNA_def_property_pointer_sdna(prop, nullptr, "strip->crop");
  RNA_def_property_ui_text(prop, "Crop", "");
}

static void rna_def_proxy(StructRNA *srna)
{
  PropertyRNA *prop;

  prop = RNA_def_property(srna, "use_proxy", PROP_BOOLEAN, PROP_NONE);
  RNA_def_property_boolean_sdna(prop, nullptr, "flag", SEQ_USE_PROXY);
  RNA_def_property_ui_text(
      prop, "Use Proxy / Timecode", "Use a preview proxy and/or time-code index for this strip");
  RNA_def_property_boolean_funcs(prop, nullptr, "rna_Sequence_use_proxy_set");
  RNA_def_property_update(
      prop, NC_SCENE | ND_SEQUENCER, "rna_Sequence_invalidate_preprocessed_update");

  prop = RNA_def_property(srna, "proxy", PROP_POINTER, PROP_NONE);
  RNA_def_property_pointer_sdna(prop, nullptr, "strip->proxy");
  RNA_def_property_ui_text(prop, "Proxy", "");
}

static void rna_def_input(StructRNA *srna)
{
  PropertyRNA *prop;

  prop = RNA_def_property(srna, "animation_offset_start", PROP_INT, PROP_UNSIGNED);
  RNA_def_property_int_sdna(prop, nullptr, "anim_startofs");
  RNA_def_property_clear_flag(prop, PROP_ANIMATABLE);
  RNA_def_property_int_funcs(prop,
                             nullptr,
                             "rna_Sequence_anim_startofs_final_set",
                             "rna_Sequence_anim_startofs_final_range"); /* overlap tests */
  RNA_def_property_ui_text(prop, "Animation Start Offset", "Animation start offset (trim start)");
  RNA_def_property_update(
      prop, NC_SCENE | ND_SEQUENCER, "rna_Sequence_invalidate_preprocessed_update");

  prop = RNA_def_property(srna, "animation_offset_end", PROP_INT, PROP_UNSIGNED);
  RNA_def_property_int_sdna(prop, nullptr, "anim_endofs");
  RNA_def_property_clear_flag(prop, PROP_ANIMATABLE);
  RNA_def_property_int_funcs(prop,
                             nullptr,
                             "rna_Sequence_anim_endofs_final_set",
                             "rna_Sequence_anim_endofs_final_range"); /* overlap tests */
  RNA_def_property_ui_text(prop, "Animation End Offset", "Animation end offset (trim end)");
  RNA_def_property_update(
      prop, NC_SCENE | ND_SEQUENCER, "rna_Sequence_invalidate_preprocessed_update");
}

static void rna_def_effect_inputs(StructRNA *srna, int count)
{
  PropertyRNA *prop;

  prop = RNA_def_property(srna, "input_count", PROP_INT, PROP_UNSIGNED);
  RNA_def_property_clear_flag(prop, PROP_EDITABLE);
  RNA_def_property_int_funcs(prop, "rna_Sequence_input_count_get", nullptr, nullptr);

  if (count >= 1) {
    prop = RNA_def_property(srna, "input_1", PROP_POINTER, PROP_NONE);
    RNA_def_property_pointer_sdna(prop, nullptr, "seq1");
    RNA_def_property_flag(prop, PROP_EDITABLE | PROP_NEVER_NULL);
    RNA_def_property_pointer_funcs(prop, nullptr, "rna_Sequence_input_1_set", nullptr, nullptr);
    RNA_def_property_ui_text(prop, "Input 1", "First input for the effect strip");
  }

  if (count >= 2) {
    prop = RNA_def_property(srna, "input_2", PROP_POINTER, PROP_NONE);
    RNA_def_property_pointer_sdna(prop, nullptr, "seq2");
    RNA_def_property_flag(prop, PROP_EDITABLE | PROP_NEVER_NULL);
    RNA_def_property_pointer_funcs(prop, nullptr, "rna_Sequence_input_2_set", nullptr, nullptr);
    RNA_def_property_ui_text(prop, "Input 2", "Second input for the effect strip");
  }

#  if 0
  if (count == 3) {
    /* Not used by any effects (perhaps one day plugins?). */
    prop = RNA_def_property(srna, "input_3", PROP_POINTER, PROP_NONE);
    RNA_def_property_pointer_sdna(prop, nullptr, "seq3");
    RNA_def_property_flag(prop, PROP_EDITABLE | PROP_NEVER_NULL);
    RNA_def_property_ui_text(prop, "Input 3", "Third input for the effect strip");
  }
#  endif
}

static void rna_def_color_management(StructRNA *srna)
{
  PropertyRNA *prop;

  prop = RNA_def_property(srna, "colorspace_settings", PROP_POINTER, PROP_NONE);
  RNA_def_property_pointer_sdna(prop, nullptr, "strip->colorspace_settings");
  RNA_def_property_struct_type(prop, "ColorManagedInputColorspaceSettings");
  RNA_def_property_ui_text(prop, "Color Space Settings", "Input color space settings");
}

static void rna_def_movie_types(StructRNA *srna)
{
  PropertyRNA *prop;

  prop = RNA_def_property(srna, "fps", PROP_FLOAT, PROP_NONE);
  RNA_def_property_ui_text(prop, "FPS", "Frames per second");
  RNA_def_property_clear_flag(prop, PROP_EDITABLE);
  RNA_def_property_float_funcs(prop, "rna_Sequence_fps_get", nullptr, nullptr);
}

static void rna_def_image(BlenderRNA *brna)
{
  StructRNA *srna;
  PropertyRNA *prop;

  srna = RNA_def_struct(brna, "ImageSequence", "Sequence");
  RNA_def_struct_ui_text(srna, "Image Sequence", "Sequence strip to load one or more images");
  RNA_def_struct_sdna(srna, "Sequence");

  prop = RNA_def_property(srna, "directory", PROP_STRING, PROP_DIRPATH);
  RNA_def_property_string_sdna(prop, nullptr, "strip->dirpath");
  RNA_def_property_ui_text(prop, "Directory", "");
  RNA_def_property_update(prop, NC_SCENE | ND_SEQUENCER, "rna_Sequence_invalidate_raw_update");

  prop = RNA_def_property(srna, "elements", PROP_COLLECTION, PROP_NONE);
  RNA_def_property_collection_sdna(prop, nullptr, "strip->stripdata", nullptr);
  RNA_def_property_struct_type(prop, "SequenceElement");
  RNA_def_property_ui_text(prop, "Elements", "");
  RNA_def_property_collection_funcs(prop,
                                    "rna_Sequence_elements_begin",
                                    "rna_iterator_array_next",
                                    "rna_iterator_array_end",
                                    "rna_iterator_array_get",
                                    "rna_SequenceEditor_elements_length",
                                    nullptr,
                                    nullptr,
                                    nullptr);
  RNA_api_sequence_elements(brna, prop);

  /* multiview */
  prop = RNA_def_property(srna, "use_multiview", PROP_BOOLEAN, PROP_NONE);
  RNA_def_property_boolean_sdna(prop, nullptr, "flag", SEQ_USE_VIEWS);
  RNA_def_property_ui_text(prop, "Use Multi-View", "Use Multiple Views (when available)");
  RNA_def_property_update(prop, NC_SCENE | ND_SEQUENCER, "rna_Sequence_views_format_update");

  prop = RNA_def_property(srna, "views_format", PROP_ENUM, PROP_NONE);
  RNA_def_property_enum_sdna(prop, nullptr, "views_format");
  RNA_def_property_enum_items(prop, rna_enum_views_format_items);
  RNA_def_property_ui_text(prop, "Views Format", "Mode to load image views");
  RNA_def_property_update(prop, NC_IMAGE | ND_DISPLAY, "rna_Sequence_views_format_update");

  prop = RNA_def_property(srna, "stereo_3d_format", PROP_POINTER, PROP_NONE);
  RNA_def_property_pointer_sdna(prop, nullptr, "stereo3d_format");
  RNA_def_property_flag(prop, PROP_NEVER_NULL);
  RNA_def_property_struct_type(prop, "Stereo3dFormat");
  RNA_def_property_ui_text(prop, "Stereo 3D Format", "Settings for stereo 3D");

  rna_def_filter_video(srna);
  rna_def_proxy(srna);
  rna_def_input(srna);
  rna_def_color_management(srna);
}

static void rna_def_meta(BlenderRNA *brna)
{
  StructRNA *srna;
  FunctionRNA *func;
  PropertyRNA *prop;

  srna = RNA_def_struct(brna, "MetaSequence", "Sequence");
  RNA_def_struct_ui_text(
      srna, "Meta Sequence", "Sequence strip to group other strips as a single sequence strip");
  RNA_def_struct_sdna(srna, "Sequence");

  prop = RNA_def_property(srna, "sequences", PROP_COLLECTION, PROP_NONE);
  RNA_def_property_collection_sdna(prop, nullptr, "seqbase", nullptr);
  RNA_def_property_struct_type(prop, "Sequence");
  RNA_def_property_ui_text(prop, "Sequences", "Sequences nested in meta strip");
  RNA_api_sequences(brna, prop, true);

  prop = RNA_def_property(srna, "channels", PROP_COLLECTION, PROP_NONE);
  RNA_def_property_collection_sdna(prop, nullptr, "channels", nullptr);
  RNA_def_property_struct_type(prop, "SequenceTimelineChannel");
  RNA_def_property_ui_text(prop, "Channels", "");

  func = RNA_def_function(srna, "separate", "rna_Sequence_separate");
  RNA_def_function_flag(func, FUNC_USE_SELF_ID | FUNC_USE_MAIN);
  RNA_def_function_ui_description(func, "Separate meta");

  rna_def_filter_video(srna);
  rna_def_proxy(srna);
  rna_def_input(srna);
}

static void rna_def_audio_options(StructRNA *srna)
{
  PropertyRNA *prop;

  prop = RNA_def_property(srna, "volume", PROP_FLOAT, PROP_NONE);
  RNA_def_property_float_sdna(prop, nullptr, "volume");
  RNA_def_property_range(prop, 0.0f, 100.0f);
  RNA_def_property_ui_text(prop, "Volume", "Playback volume of the sound");
  RNA_def_property_translation_context(prop, BLT_I18NCONTEXT_ID_SOUND);
  RNA_def_property_update(prop, NC_SCENE | ND_SEQUENCER, "rna_Sequence_audio_update");
}

static void rna_def_scene(BlenderRNA *brna)
{
  StructRNA *srna;
  PropertyRNA *prop;

  static const EnumPropertyItem scene_input_items[] = {
      {0, "CAMERA", ICON_VIEW3D, "Camera", "Use the Scene's 3D camera as input"},
      {SEQ_SCENE_STRIPS,
       "SEQUENCER",
       ICON_SEQUENCE,
       "Sequencer",
       "Use the Scene's Sequencer timeline as input"},
      {0, nullptr, 0, nullptr, nullptr},
  };

  srna = RNA_def_struct(brna, "SceneSequence", "Sequence");
  RNA_def_struct_ui_text(
      srna, "Scene Sequence", "Sequence strip using the rendered image of a scene");
  RNA_def_struct_sdna(srna, "Sequence");

  prop = RNA_def_property(srna, "scene", PROP_POINTER, PROP_NONE);
  RNA_def_property_flag(prop, PROP_EDITABLE | PROP_ID_SELF_CHECK);
  RNA_def_property_ui_text(prop, "Scene", "Scene that this sequence uses");
  RNA_def_property_update(prop, NC_SCENE | ND_SEQUENCER, "rna_Sequence_scene_switch_update");

  prop = RNA_def_property(srna, "scene_camera", PROP_POINTER, PROP_NONE);
  RNA_def_property_flag(prop, PROP_EDITABLE);
  RNA_def_property_pointer_funcs(prop, nullptr, nullptr, nullptr, "rna_Camera_object_poll");
  RNA_def_property_ui_text(prop, "Camera Override", "Override the scene's active camera");
  RNA_def_property_update(prop, NC_SCENE | ND_SEQUENCER, "rna_Sequence_invalidate_raw_update");

  prop = RNA_def_property(srna, "scene_input", PROP_ENUM, PROP_NONE);
  RNA_def_property_enum_bitflag_sdna(prop, nullptr, "flag");
  RNA_def_property_enum_items(prop, scene_input_items);
  RNA_def_property_ui_text(prop, "Input", "Input type to use for the Scene strip");
  RNA_def_property_update(prop, NC_SCENE | ND_SEQUENCER, "rna_Sequence_use_sequence");

  prop = RNA_def_property(srna, "use_annotations", PROP_BOOLEAN, PROP_NONE);
  RNA_def_property_boolean_negative_sdna(prop, nullptr, "flag", SEQ_SCENE_NO_ANNOTATION);
  RNA_def_property_ui_text(prop, "Use Annotations", "Show Annotations in OpenGL previews");
  RNA_def_property_update(prop, NC_SCENE | ND_SEQUENCER, "rna_Sequence_invalidate_raw_update");

  rna_def_audio_options(srna);
  rna_def_filter_video(srna);
  rna_def_proxy(srna);
  rna_def_input(srna);
  rna_def_movie_types(srna);
}

static void rna_def_movie(BlenderRNA *brna)
{
  StructRNA *srna;
  PropertyRNA *prop;
  FunctionRNA *func;
  PropertyRNA *parm;

  srna = RNA_def_struct(brna, "MovieSequence", "Sequence");
  RNA_def_struct_ui_text(srna, "Movie Sequence", "Sequence strip to load a video");
  RNA_def_struct_sdna(srna, "Sequence");

  prop = RNA_def_property(srna, "stream_index", PROP_INT, PROP_NONE);
  RNA_def_property_int_sdna(prop, nullptr, "streamindex");
  RNA_def_property_range(prop, 0, 20);
  RNA_def_property_ui_text(
      prop,
      "Stream Index",
      "For files with several movie streams, use the stream with the given index");
  RNA_def_property_update(prop, NC_SCENE | ND_SEQUENCER, "rna_Sequence_reopen_files_update");

  prop = RNA_def_property(srna, "elements", PROP_COLLECTION, PROP_NONE);
  RNA_def_property_collection_sdna(prop, nullptr, "strip->stripdata", nullptr);
  RNA_def_property_struct_type(prop, "SequenceElement");
  RNA_def_property_ui_text(prop, "Elements", "");
  RNA_def_property_collection_funcs(prop,
                                    "rna_Sequence_elements_begin",
                                    "rna_iterator_array_next",
                                    "rna_iterator_array_end",
                                    "rna_iterator_array_get",
                                    "rna_SequenceEditor_elements_length",
                                    nullptr,
                                    nullptr,
                                    nullptr);

  prop = RNA_def_property(srna, "retiming_keys", PROP_COLLECTION, PROP_NONE);
  RNA_def_property_collection_sdna(prop, nullptr, "retiming_keys", nullptr);
  RNA_def_property_struct_type(prop, "RetimingKey");
  RNA_def_property_ui_text(prop, "Retiming Keys", "");
  RNA_def_property_collection_funcs(prop,
                                    "rna_SequenceEditor_retiming_keys_begin",
                                    "rna_iterator_array_next",
                                    "rna_iterator_array_end",
                                    "rna_iterator_array_get",
                                    "rna_Sequence_retiming_keys_length",
                                    nullptr,
                                    nullptr,
                                    nullptr);
  RNA_api_sequence_retiming_keys(brna, prop);

  prop = RNA_def_property(srna, "filepath", PROP_STRING, PROP_FILEPATH);
  RNA_def_property_ui_text(prop, "File", "");
  RNA_def_property_string_funcs(prop,
                                "rna_Sequence_filepath_get",
                                "rna_Sequence_filepath_length",
                                "rna_Sequence_filepath_set");
  RNA_def_property_update(prop, NC_SCENE | ND_SEQUENCER, "rna_Sequence_filepath_update");

  func = RNA_def_function(srna, "reload_if_needed", "rna_MovieSequence_reload_if_needed");
  RNA_def_function_flag(func, FUNC_USE_SELF_ID | FUNC_USE_MAIN);
  /* return type */
  parm = RNA_def_boolean(func,
                         "can_produce_frames",
                         false,
                         "True if the strip can produce frames, False otherwise",
                         "");
  RNA_def_function_return(func, parm);

  /* metadata */
  func = RNA_def_function(srna, "metadata", "rna_MovieSequence_metadata_get");
  RNA_def_function_flag(func, FUNC_USE_SELF_ID);
  RNA_def_function_ui_description(func, "Retrieve metadata of the movie file");
  /* return type */
  parm = RNA_def_pointer(
      func, "metadata", "IDPropertyWrapPtr", "", "Dict-like object containing the metadata");
  RNA_def_parameter_flags(parm, PropertyFlag(0), PARM_RNAPTR);
  RNA_def_function_return(func, parm);

  /* multiview */
  prop = RNA_def_property(srna, "use_multiview", PROP_BOOLEAN, PROP_NONE);
  RNA_def_property_boolean_sdna(prop, nullptr, "flag", SEQ_USE_VIEWS);
  RNA_def_property_ui_text(prop, "Use Multi-View", "Use Multiple Views (when available)");
  RNA_def_property_update(prop, NC_SCENE | ND_SEQUENCER, "rna_Sequence_views_format_update");

  prop = RNA_def_property(srna, "views_format", PROP_ENUM, PROP_NONE);
  RNA_def_property_enum_sdna(prop, nullptr, "views_format");
  RNA_def_property_enum_items(prop, rna_enum_views_format_items);
  RNA_def_property_ui_text(prop, "Views Format", "Mode to load movie views");
  RNA_def_property_update(prop, NC_IMAGE | ND_DISPLAY, "rna_Sequence_views_format_update");

  prop = RNA_def_property(srna, "stereo_3d_format", PROP_POINTER, PROP_NONE);
  RNA_def_property_pointer_sdna(prop, nullptr, "stereo3d_format");
  RNA_def_property_flag(prop, PROP_NEVER_NULL);
  RNA_def_property_struct_type(prop, "Stereo3dFormat");
  RNA_def_property_ui_text(prop, "Stereo 3D Format", "Settings for stereo 3D");

  rna_def_filter_video(srna);
  rna_def_proxy(srna);
  rna_def_input(srna);
  rna_def_color_management(srna);
  rna_def_movie_types(srna);
}

static void rna_def_movieclip(BlenderRNA *brna)
{
  StructRNA *srna;
  PropertyRNA *prop;

  srna = RNA_def_struct(brna, "MovieClipSequence", "Sequence");
  RNA_def_struct_ui_text(
      srna, "MovieClip Sequence", "Sequence strip to load a video from the clip editor");
  RNA_def_struct_sdna(srna, "Sequence");

  /* TODO: add clip property? */

  prop = RNA_def_property(srna, "undistort", PROP_BOOLEAN, PROP_NONE);
  RNA_def_property_boolean_sdna(prop, nullptr, "clip_flag", SEQ_MOVIECLIP_RENDER_UNDISTORTED);
  RNA_def_property_ui_text(prop, "Undistort Clip", "Use the undistorted version of the clip");
  RNA_def_property_update(prop, NC_SCENE | ND_SEQUENCER, "rna_Sequence_invalidate_raw_update");

  prop = RNA_def_property(srna, "stabilize2d", PROP_BOOLEAN, PROP_NONE);
  RNA_def_property_boolean_sdna(prop, nullptr, "clip_flag", SEQ_MOVIECLIP_RENDER_STABILIZED);
  RNA_def_property_ui_text(prop, "Stabilize 2D Clip", "Use the 2D stabilized version of the clip");
  RNA_def_property_update(prop, NC_SCENE | ND_SEQUENCER, "rna_Sequence_invalidate_raw_update");

  rna_def_filter_video(srna);
  rna_def_input(srna);
  rna_def_movie_types(srna);
}

static void rna_def_mask(BlenderRNA *brna)
{
  StructRNA *srna;
  PropertyRNA *prop;

  srna = RNA_def_struct(brna, "MaskSequence", "Sequence");
  RNA_def_struct_ui_text(srna, "Mask Sequence", "Sequence strip to load a video from a mask");
  RNA_def_struct_sdna(srna, "Sequence");

  prop = RNA_def_property(srna, "mask", PROP_POINTER, PROP_NONE);
  RNA_def_property_flag(prop, PROP_EDITABLE);
  RNA_def_property_ui_text(prop, "Mask", "Mask that this sequence uses");
  RNA_def_property_update(prop, NC_SCENE | ND_SEQUENCER, "rna_Sequence_invalidate_raw_update");

  rna_def_filter_video(srna);
  rna_def_input(srna);
}

static void rna_def_sound(BlenderRNA *brna)
{
  StructRNA *srna;
  PropertyRNA *prop;

  srna = RNA_def_struct(brna, "SoundSequence", "Sequence");
  RNA_def_struct_ui_text(srna,
                         "Sound Sequence",
                         "Sequence strip defining a sound to be played over a period of time");
  RNA_def_struct_sdna(srna, "Sequence");

  prop = RNA_def_property(srna, "sound", PROP_POINTER, PROP_NONE);
  RNA_def_property_flag(prop, PROP_EDITABLE);
  RNA_def_property_struct_type(prop, "Sound");
  RNA_def_property_ui_text(prop, "Sound", "Sound data-block used by this sequence");
  RNA_def_property_update(prop, NC_SCENE | ND_SEQUENCER, "rna_Sequence_sound_update");

  rna_def_audio_options(srna);

  prop = RNA_def_property(srna, "pan", PROP_FLOAT, PROP_NONE);
  RNA_def_property_float_sdna(prop, nullptr, "pan");
  RNA_def_property_range(prop, -FLT_MAX, FLT_MAX);
  RNA_def_property_ui_range(prop, -2, 2, 1, 2);
  RNA_def_property_ui_text(prop, "Pan", "Playback panning of the sound (only for Mono sources)");
  RNA_def_property_translation_context(prop, BLT_I18NCONTEXT_ID_SOUND);
  RNA_def_property_float_funcs(prop, nullptr, nullptr, "rna_Sequence_pan_range");
  RNA_def_property_update(prop, NC_SCENE | ND_SEQUENCER, "rna_Sequence_audio_update");

  prop = RNA_def_property(srna, "show_waveform", PROP_BOOLEAN, PROP_NONE);
  RNA_def_property_boolean_sdna(prop, nullptr, "flag", SEQ_AUDIO_DRAW_WAVEFORM);
  RNA_def_property_ui_text(
      prop, "Display Waveform", "Display the audio waveform inside the strip");
  RNA_def_property_update(prop, NC_SCENE | ND_SEQUENCER, nullptr);

  rna_def_input(srna);
}

static void rna_def_effect(BlenderRNA *brna)
{
  StructRNA *srna;

  srna = RNA_def_struct(brna, "EffectSequence", "Sequence");
  RNA_def_struct_ui_text(
      srna,
      "Effect Sequence",
      "Sequence strip applying an effect on the images created by other strips");
  RNA_def_struct_sdna(srna, "Sequence");

  rna_def_filter_video(srna);
  rna_def_proxy(srna);
}

static void rna_def_multicam(StructRNA *srna)
{
  PropertyRNA *prop;

  prop = RNA_def_property(srna, "multicam_source", PROP_INT, PROP_UNSIGNED);
  RNA_def_property_int_sdna(prop, nullptr, "multicam_source");
  RNA_def_property_range(prop, 0, MAXSEQ - 1);
  RNA_def_property_ui_text(prop, "Multicam Source Channel", "");
  RNA_def_property_update(prop, NC_SCENE | ND_SEQUENCER, "rna_Sequence_invalidate_raw_update");

  rna_def_input(srna);
}

static void rna_def_wipe(StructRNA *srna)
{
  PropertyRNA *prop;

  static const EnumPropertyItem wipe_type_items[] = {
      {DO_SINGLE_WIPE, "SINGLE", 0, "Single", ""},
      {DO_DOUBLE_WIPE, "DOUBLE", 0, "Double", ""},
      /* not used yet {DO_BOX_WIPE, "BOX", 0, "Box", ""}, */
      /* not used yet {DO_CROSS_WIPE, "CROSS", 0, "Cross", ""}, */
      {DO_IRIS_WIPE, "IRIS", 0, "Iris", ""},
      {DO_CLOCK_WIPE, "CLOCK", 0, "Clock", ""},
      {0, nullptr, 0, nullptr, nullptr},
  };

  static const EnumPropertyItem wipe_direction_items[] = {
      {0, "OUT", 0, "Out", ""},
      {1, "IN", 0, "In", ""},
      {0, nullptr, 0, nullptr, nullptr},
  };

  RNA_def_struct_sdna_from(srna, "WipeVars", "effectdata");

  prop = RNA_def_property(srna, "blur_width", PROP_FLOAT, PROP_FACTOR);
  RNA_def_property_float_sdna(prop, nullptr, "edgeWidth");
  RNA_def_property_range(prop, 0.0f, 1.0f);
  RNA_def_property_ui_text(
      prop,
      "Blur Width",
      "Width of the blur for the transition, in percentage relative to the image size");
  RNA_def_property_update(prop, NC_SCENE | ND_SEQUENCER, "rna_Sequence_invalidate_raw_update");

  prop = RNA_def_property(srna, "angle", PROP_FLOAT, PROP_ANGLE);
  RNA_def_property_range(prop, DEG2RADF(-90.0f), DEG2RADF(90.0f));
  RNA_def_property_ui_text(prop, "Angle", "Angle of the transition");
  RNA_def_property_update(prop, NC_SCENE | ND_SEQUENCER, "rna_Sequence_invalidate_raw_update");

  prop = RNA_def_property(srna, "direction", PROP_ENUM, PROP_NONE);
  RNA_def_property_enum_sdna(prop, nullptr, "forward");
  RNA_def_property_enum_items(prop, wipe_direction_items);
  RNA_def_property_ui_text(prop, "Direction", "Whether to fade in or out");
  RNA_def_property_translation_context(prop, BLT_I18NCONTEXT_ID_SEQUENCE);
  RNA_def_property_update(prop, NC_SCENE | ND_SEQUENCER, "rna_Sequence_invalidate_raw_update");

  prop = RNA_def_property(srna, "transition_type", PROP_ENUM, PROP_NONE);
  RNA_def_property_enum_sdna(prop, nullptr, "wipetype");
  RNA_def_property_enum_items(prop, wipe_type_items);
  RNA_def_property_translation_context(prop, BLT_I18NCONTEXT_ID_SEQUENCE);
  RNA_def_property_ui_text(prop, "Transition Type", "");
  RNA_def_property_update(prop, NC_SCENE | ND_SEQUENCER, "rna_Sequence_invalidate_raw_update");
}

static void rna_def_glow(StructRNA *srna)
{
  PropertyRNA *prop;

  RNA_def_struct_sdna_from(srna, "GlowVars", "effectdata");

  prop = RNA_def_property(srna, "threshold", PROP_FLOAT, PROP_FACTOR);
  RNA_def_property_float_sdna(prop, nullptr, "fMini");
  RNA_def_property_range(prop, 0.0f, 1.0f);
  RNA_def_property_ui_text(prop, "Threshold", "Minimum intensity to trigger a glow");
  RNA_def_property_update(prop, NC_SCENE | ND_SEQUENCER, "rna_Sequence_invalidate_raw_update");

  prop = RNA_def_property(srna, "clamp", PROP_FLOAT, PROP_FACTOR);
  RNA_def_property_float_sdna(prop, nullptr, "fClamp");
  RNA_def_property_range(prop, 0.0f, 1.0f);
  RNA_def_property_ui_text(prop, "Clamp", "Brightness limit of intensity");
  RNA_def_property_update(prop, NC_SCENE | ND_SEQUENCER, "rna_Sequence_invalidate_raw_update");

  prop = RNA_def_property(srna, "boost_factor", PROP_FLOAT, PROP_NONE);
  RNA_def_property_float_sdna(prop, nullptr, "fBoost");
  RNA_def_property_range(prop, 0.0f, 10.0f);
  RNA_def_property_ui_text(prop, "Boost Factor", "Brightness multiplier");
  RNA_def_property_update(prop, NC_SCENE | ND_SEQUENCER, "rna_Sequence_invalidate_raw_update");

  prop = RNA_def_property(srna, "blur_radius", PROP_FLOAT, PROP_NONE);
  RNA_def_property_float_sdna(prop, nullptr, "dDist");
  RNA_def_property_range(prop, 0.5f, 20.0f);
  RNA_def_property_ui_text(prop, "Blur Distance", "Radius of glow effect");
  RNA_def_property_update(prop, NC_SCENE | ND_SEQUENCER, "rna_Sequence_invalidate_raw_update");

  prop = RNA_def_property(srna, "quality", PROP_INT, PROP_NONE);
  RNA_def_property_int_sdna(prop, nullptr, "dQuality");
  RNA_def_property_range(prop, 1, 5);
  RNA_def_property_ui_text(prop, "Quality", "Accuracy of the blur effect");
  RNA_def_property_update(prop, NC_SCENE | ND_SEQUENCER, "rna_Sequence_invalidate_raw_update");

  prop = RNA_def_property(srna, "use_only_boost", PROP_BOOLEAN, PROP_NONE);
  RNA_def_property_boolean_sdna(prop, nullptr, "bNoComp", 0);
  RNA_def_property_ui_text(prop, "Only Boost", "Show the glow buffer only");
  RNA_def_property_update(prop, NC_SCENE | ND_SEQUENCER, "rna_Sequence_invalidate_raw_update");
}

static void rna_def_transform(StructRNA *srna)
{
  PropertyRNA *prop;

  static const EnumPropertyItem interpolation_items[] = {
      {0, "NONE", 0, "None", "No interpolation"},
      {1, "BILINEAR", 0, "Bilinear", "Bilinear interpolation"},
      {2, "BICUBIC", 0, "Bicubic", "Bicubic interpolation"},
      {0, nullptr, 0, nullptr, nullptr},
  };

  static const EnumPropertyItem translation_unit_items[] = {
      {0, "PIXELS", 0, "Pixels", ""},
      {1, "PERCENT", 0, "Percent", ""},
      {0, nullptr, 0, nullptr, nullptr},
  };

  RNA_def_struct_sdna_from(srna, "TransformVars", "effectdata");

  prop = RNA_def_property(srna, "scale_start_x", PROP_FLOAT, PROP_UNSIGNED);
  RNA_def_property_float_sdna(prop, nullptr, "ScalexIni");
  RNA_def_property_ui_text(prop, "Scale X", "Amount to scale the input in the X axis");
  RNA_def_property_ui_range(prop, 0, 10, 3, 6);
  RNA_def_property_update(prop, NC_SCENE | ND_SEQUENCER, "rna_Sequence_invalidate_raw_update");

  prop = RNA_def_property(srna, "scale_start_y", PROP_FLOAT, PROP_UNSIGNED);
  RNA_def_property_float_sdna(prop, nullptr, "ScaleyIni");
  RNA_def_property_ui_text(prop, "Scale Y", "Amount to scale the input in the Y axis");
  RNA_def_property_ui_range(prop, 0, 10, 3, 6);
  RNA_def_property_update(prop, NC_SCENE | ND_SEQUENCER, "rna_Sequence_invalidate_raw_update");

  prop = RNA_def_property(srna, "use_uniform_scale", PROP_BOOLEAN, PROP_NONE);
  RNA_def_property_boolean_sdna(prop, nullptr, "uniform_scale", 0);
  RNA_def_property_ui_text(prop, "Uniform Scale", "Scale uniformly, preserving aspect ratio");
  RNA_def_property_update(prop, NC_SCENE | ND_SEQUENCER, "rna_Sequence_invalidate_raw_update");

  prop = RNA_def_property(srna, "translate_start_x", PROP_FLOAT, PROP_NONE);
  RNA_def_property_float_sdna(prop, nullptr, "xIni");
  RNA_def_property_ui_text(prop, "Translate X", "Amount to move the input on the X axis");
  RNA_def_property_ui_range(prop, -4000.0f, 4000.0f, 3, 6);
  RNA_def_property_update(prop, NC_SCENE | ND_SEQUENCER, "rna_Sequence_invalidate_raw_update");

  prop = RNA_def_property(srna, "translate_start_y", PROP_FLOAT, PROP_NONE);
  RNA_def_property_float_sdna(prop, nullptr, "yIni");
  RNA_def_property_ui_text(prop, "Translate Y", "Amount to move the input on the Y axis");
  RNA_def_property_ui_range(prop, -4000.0f, 4000.0f, 3, 6);
  RNA_def_property_update(prop, NC_SCENE | ND_SEQUENCER, "rna_Sequence_invalidate_raw_update");

  prop = RNA_def_property(srna, "rotation_start", PROP_FLOAT, PROP_NONE);
  RNA_def_property_float_sdna(prop, nullptr, "rotIni");
  RNA_def_property_ui_text(prop, "Rotation", "Degrees to rotate the input");
  RNA_def_property_update(prop, NC_SCENE | ND_SEQUENCER, "rna_Sequence_invalidate_raw_update");

  prop = RNA_def_property(srna, "translation_unit", PROP_ENUM, PROP_NONE);
  RNA_def_property_enum_sdna(prop, nullptr, "percent");
  RNA_def_property_clear_flag(prop, PROP_ANIMATABLE); /* not meant to be animated */
  RNA_def_property_enum_items(prop, translation_unit_items);
  RNA_def_property_ui_text(prop, "Translation Unit", "Unit of measure to translate the input");
  RNA_def_property_update(prop, NC_SCENE | ND_SEQUENCER, "rna_Sequence_invalidate_raw_update");

  prop = RNA_def_property(srna, "interpolation", PROP_ENUM, PROP_NONE);
  RNA_def_property_enum_items(prop, interpolation_items);
  RNA_def_property_clear_flag(prop, PROP_ANIMATABLE); /* not meant to be animated */
  RNA_def_property_ui_text(
      prop, "Interpolation", "Method to determine how missing pixels are created");
  RNA_def_property_update(prop, NC_SCENE | ND_SEQUENCER, "rna_Sequence_invalidate_raw_update");
}

static void rna_def_solid_color(StructRNA *srna)
{
  PropertyRNA *prop;

  RNA_def_struct_sdna_from(srna, "SolidColorVars", "effectdata");

  prop = RNA_def_property(srna, "color", PROP_FLOAT, PROP_COLOR_GAMMA);
  RNA_def_property_float_sdna(prop, nullptr, "col");
  RNA_def_property_ui_text(prop, "Color", "Effect Strip color");
  RNA_def_property_update(prop, NC_SCENE | ND_SEQUENCER, "rna_Sequence_invalidate_raw_update");
}

static void rna_def_speed_control(StructRNA *srna)
{
  PropertyRNA *prop;

  RNA_def_struct_sdna_from(srna, "SpeedControlVars", "effectdata");

  static const EnumPropertyItem speed_control_items[] = {
      {SEQ_SPEED_STRETCH,
       "STRETCH",
       0,
       "Stretch",
       "Adjust input playback speed, so its duration fits strip length"},
      {SEQ_SPEED_MULTIPLY, "MULTIPLY", 0, "Multiply", "Multiply with the speed factor"},
      {SEQ_SPEED_FRAME_NUMBER,
       "FRAME_NUMBER",
       0,
       "Frame Number",
       "Frame number of the input strip"},
      {SEQ_SPEED_LENGTH, "LENGTH", 0, "Length", "Percentage of the input strip length"},
      {0, nullptr, 0, nullptr, nullptr},
  };

  prop = RNA_def_property(srna, "speed_control", PROP_ENUM, PROP_NONE);
  RNA_def_property_enum_sdna(prop, nullptr, "speed_control_type");
  RNA_def_property_enum_items(prop, speed_control_items);
  RNA_def_property_clear_flag(prop, PROP_ANIMATABLE);
  RNA_def_property_ui_text(prop, "Speed Control", "Speed control method");
  RNA_def_property_update(prop, NC_SCENE | ND_SEQUENCER, "rna_Sequence_invalidate_raw_update");

  prop = RNA_def_property(srna, "speed_factor", PROP_FLOAT, PROP_NONE);
  RNA_def_property_float_sdna(prop, nullptr, "speed_fader");
  RNA_def_property_ui_text(
      prop,
      "Multiply Factor",
      "Multiply the current speed of the sequence with this number or remap current frame "
      "to this frame");
  RNA_def_property_update(prop, NC_SCENE | ND_SEQUENCER, "rna_Sequence_invalidate_raw_update");

  prop = RNA_def_property(srna, "speed_frame_number", PROP_FLOAT, PROP_NONE);
  RNA_def_property_float_sdna(prop, nullptr, "speed_fader_frame_number");
  RNA_def_property_ui_text(prop, "Frame Number", "Frame number of input strip");
  RNA_def_property_ui_range(prop, 0.0, MAXFRAME, 1.0, -1);
  RNA_def_property_update(prop, NC_SCENE | ND_SEQUENCER, "rna_Sequence_invalidate_raw_update");

  prop = RNA_def_property(srna, "speed_length", PROP_FLOAT, PROP_PERCENTAGE);
  RNA_def_property_float_sdna(prop, nullptr, "speed_fader_length");
  RNA_def_property_ui_text(prop, "Length", "Percentage of input strip length");
  RNA_def_property_ui_range(prop, 0.0, 100.0, 1, -1);
  RNA_def_property_update(prop, NC_SCENE | ND_SEQUENCER, "rna_Sequence_invalidate_raw_update");

  prop = RNA_def_property(srna, "use_frame_interpolate", PROP_BOOLEAN, PROP_NONE);
  RNA_def_property_boolean_sdna(prop, nullptr, "flags", SEQ_SPEED_USE_INTERPOLATION);
  RNA_def_property_ui_text(
      prop, "Frame Interpolation", "Do crossfade blending between current and next frame");
  RNA_def_property_update(prop, NC_SCENE | ND_SEQUENCER, "rna_Sequence_invalidate_raw_update");
}

static void rna_def_gaussian_blur(StructRNA *srna)
{
  PropertyRNA *prop;

  RNA_def_struct_sdna_from(srna, "GaussianBlurVars", "effectdata");
  prop = RNA_def_property(srna, "size_x", PROP_FLOAT, PROP_UNSIGNED);
  RNA_def_property_ui_text(prop, "Size X", "Size of the blur along X axis");
  RNA_def_property_ui_range(prop, 0.0f, FLT_MAX, 1, -1);
  RNA_def_property_update(prop, NC_SCENE | ND_SEQUENCER, "rna_Sequence_invalidate_raw_update");

  prop = RNA_def_property(srna, "size_y", PROP_FLOAT, PROP_UNSIGNED);
  RNA_def_property_ui_text(prop, "Size Y", "Size of the blur along Y axis");
  RNA_def_property_ui_range(prop, 0.0f, FLT_MAX, 1, -1);
  RNA_def_property_update(prop, NC_SCENE | ND_SEQUENCER, "rna_Sequence_invalidate_raw_update");
}

static void rna_def_text(StructRNA *srna)
{
  /* Avoid text icons because they imply this aligns within a frame, see: #71082 */
  static const EnumPropertyItem text_align_x_items[] = {
      {SEQ_TEXT_ALIGN_X_LEFT, "LEFT", ICON_ANCHOR_LEFT, "Left", ""},
      {SEQ_TEXT_ALIGN_X_CENTER, "CENTER", ICON_ANCHOR_CENTER, "Center", ""},
      {SEQ_TEXT_ALIGN_X_RIGHT, "RIGHT", ICON_ANCHOR_RIGHT, "Right", ""},
      {0, nullptr, 0, nullptr, nullptr},
  };
  static const EnumPropertyItem text_align_y_items[] = {
      {SEQ_TEXT_ALIGN_Y_TOP, "TOP", ICON_ANCHOR_TOP, "Top", ""},
      {SEQ_TEXT_ALIGN_Y_CENTER, "CENTER", ICON_ANCHOR_CENTER, "Center", ""},
      {SEQ_TEXT_ALIGN_Y_BOTTOM, "BOTTOM", ICON_ANCHOR_BOTTOM, "Bottom", ""},
      {0, nullptr, 0, nullptr, nullptr},
  };

  PropertyRNA *prop;

  RNA_def_struct_sdna_from(srna, "TextVars", "effectdata");

  prop = RNA_def_property(srna, "font", PROP_POINTER, PROP_NONE);
  RNA_def_property_pointer_sdna(prop, nullptr, "text_font");
  RNA_def_property_ui_icon(prop, ICON_FILE_FONT, false);
  RNA_def_property_ui_text(prop, "Font", "Font of the text. Falls back to the UI font by default");
  RNA_def_property_flag(prop, PROP_EDITABLE);
  RNA_def_property_pointer_funcs(prop, nullptr, "rna_Sequence_text_font_set", nullptr, nullptr);
  RNA_def_property_update(prop, NC_SCENE | ND_SEQUENCER, "rna_Sequence_invalidate_raw_update");

  prop = RNA_def_property(srna, "font_size", PROP_FLOAT, PROP_UNSIGNED);
  RNA_def_property_float_sdna(prop, nullptr, "text_size");
  RNA_def_property_ui_text(prop, "Size", "Size of the text");
  RNA_def_property_range(prop, 0.0, 2000);
  RNA_def_property_ui_range(prop, 0.0f, 2000, 10.0f, 1);
  RNA_def_property_update(prop, NC_SCENE | ND_SEQUENCER, "rna_Sequence_invalidate_raw_update");

  prop = RNA_def_property(srna, "color", PROP_FLOAT, PROP_COLOR_GAMMA);
  RNA_def_property_float_sdna(prop, nullptr, "color");
  RNA_def_property_ui_text(prop, "Color", "Text color");
  RNA_def_property_update(prop, NC_SCENE | ND_SEQUENCER, "rna_Sequence_invalidate_raw_update");

  prop = RNA_def_property(srna, "shadow_color", PROP_FLOAT, PROP_COLOR_GAMMA);
  RNA_def_property_float_sdna(prop, nullptr, "shadow_color");
  RNA_def_property_ui_text(prop, "Shadow Color", "");
  RNA_def_property_update(prop, NC_SCENE | ND_SEQUENCER, "rna_Sequence_invalidate_raw_update");

  prop = RNA_def_property(srna, "box_color", PROP_FLOAT, PROP_COLOR_GAMMA);
  RNA_def_property_float_sdna(prop, nullptr, "box_color");
  RNA_def_property_ui_text(prop, "Box Color", "");
  RNA_def_property_update(prop, NC_SCENE | ND_SEQUENCER, "rna_Sequence_invalidate_raw_update");

  prop = RNA_def_property(srna, "location", PROP_FLOAT, PROP_XYZ);
  RNA_def_property_float_sdna(prop, nullptr, "loc");
  RNA_def_property_ui_text(prop, "Location", "Location of the text");
  RNA_def_property_range(prop, -FLT_MAX, FLT_MAX);
  RNA_def_property_ui_range(prop, -10.0, 10.0, 1, -1);
  RNA_def_property_update(prop, NC_SCENE | ND_SEQUENCER, "rna_Sequence_invalidate_raw_update");

  prop = RNA_def_property(srna, "wrap_width", PROP_FLOAT, PROP_NONE);
  RNA_def_property_float_sdna(prop, nullptr, "wrap_width");
  RNA_def_property_ui_text(prop, "Wrap Width", "Word wrap width as factor, zero disables");
  RNA_def_property_range(prop, 0, FLT_MAX);
  RNA_def_property_ui_range(prop, 0.0, 1.0, 1, -1);
  RNA_def_property_update(prop, NC_SCENE | ND_SEQUENCER, "rna_Sequence_invalidate_raw_update");

  prop = RNA_def_property(srna, "box_margin", PROP_FLOAT, PROP_NONE);
  RNA_def_property_float_sdna(prop, nullptr, "box_margin");
  RNA_def_property_ui_text(prop, "Box Margin", "Box margin as factor of image width");
  RNA_def_property_range(prop, 0, 1.0);
  RNA_def_property_ui_range(prop, 0.0, 1.0, 1, -1);
  RNA_def_property_float_default(prop, 0.01f);
  RNA_def_property_update(prop, NC_SCENE | ND_SEQUENCER, "rna_Sequence_invalidate_raw_update");

  prop = RNA_def_property(srna, "align_x", PROP_ENUM, PROP_NONE);
  RNA_def_property_enum_sdna(prop, nullptr, "align");
  RNA_def_property_enum_items(prop, text_align_x_items);
  RNA_def_property_ui_text(
      prop, "Align X", "Align the text along the X axis, relative to the text bounds");
  RNA_def_property_update(prop, NC_SCENE | ND_SEQUENCER, "rna_Sequence_invalidate_raw_update");

  prop = RNA_def_property(srna, "align_y", PROP_ENUM, PROP_NONE);
  RNA_def_property_enum_sdna(prop, nullptr, "align_y");
  RNA_def_property_enum_items(prop, text_align_y_items);
  RNA_def_property_ui_text(
      prop, "Align Y", "Align the text along the Y axis, relative to the text bounds");
  RNA_def_property_update(prop, NC_SCENE | ND_SEQUENCER, "rna_Sequence_invalidate_raw_update");

  prop = RNA_def_property(srna, "text", PROP_STRING, PROP_NONE);
  RNA_def_property_ui_text(prop, "Text", "Text that will be displayed");
  RNA_def_property_flag(prop, PROP_TEXTEDIT_UPDATE);
  RNA_def_property_update(prop, NC_SCENE | ND_SEQUENCER, "rna_Sequence_invalidate_raw_update");

  prop = RNA_def_property(srna, "use_shadow", PROP_BOOLEAN, PROP_NONE);
  RNA_def_property_boolean_sdna(prop, nullptr, "flag", SEQ_TEXT_SHADOW);
  RNA_def_property_ui_text(prop, "Shadow", "Display shadow behind text");
  RNA_def_property_update(prop, NC_SCENE | ND_SEQUENCER, "rna_Sequence_invalidate_raw_update");

  prop = RNA_def_property(srna, "use_box", PROP_BOOLEAN, PROP_NONE);
  RNA_def_property_boolean_sdna(prop, nullptr, "flag", SEQ_TEXT_BOX);
  RNA_def_property_ui_text(prop, "Box", "Display colored box behind text");
  RNA_def_property_translation_context(prop, BLT_I18NCONTEXT_ID_SEQUENCE);
  RNA_def_property_update(prop, NC_SCENE | ND_SEQUENCER, "rna_Sequence_invalidate_raw_update");

  prop = RNA_def_property(srna, "use_bold", PROP_BOOLEAN, PROP_NONE);
  RNA_def_property_boolean_sdna(prop, nullptr, "flag", SEQ_TEXT_BOLD);
  RNA_def_property_ui_text(prop, "Bold", "Display text as bold");
  RNA_def_property_update(prop, NC_SCENE | ND_SEQUENCER, "rna_Sequence_invalidate_raw_update");

  prop = RNA_def_property(srna, "use_italic", PROP_BOOLEAN, PROP_NONE);
  RNA_def_property_boolean_sdna(prop, nullptr, "flag", SEQ_TEXT_ITALIC);
  RNA_def_property_ui_text(prop, "Italic", "Display text as italic");
  RNA_def_property_update(prop, NC_SCENE | ND_SEQUENCER, "rna_Sequence_invalidate_raw_update");
}

static void rna_def_color_mix(StructRNA *srna)
{
  static EnumPropertyItem blend_color_items[] = {
      {SEQ_TYPE_DARKEN, "DARKEN", 0, "Darken", ""},
      {SEQ_TYPE_MUL, "MULTIPLY", 0, "Multiply", ""},
      {SEQ_TYPE_COLOR_BURN, "BURN", 0, "Color Burn", ""},
      {SEQ_TYPE_LINEAR_BURN, "LINEAR_BURN", 0, "Linear Burn", ""},
      RNA_ENUM_ITEM_SEPR,
      {SEQ_TYPE_LIGHTEN, "LIGHTEN", 0, "Lighten", ""},
      {SEQ_TYPE_SCREEN, "SCREEN", 0, "Screen", ""},
      {SEQ_TYPE_DODGE, "DODGE", 0, "Color Dodge", ""},
      {SEQ_TYPE_ADD, "ADD", 0, "Add", ""},
      RNA_ENUM_ITEM_SEPR,
      {SEQ_TYPE_OVERLAY, "OVERLAY", 0, "Overlay", ""},
      {SEQ_TYPE_SOFT_LIGHT, "SOFT_LIGHT", 0, "Soft Light", ""},
      {SEQ_TYPE_HARD_LIGHT, "HARD_LIGHT", 0, "Hard Light", ""},
      {SEQ_TYPE_VIVID_LIGHT, "VIVID_LIGHT", 0, "Vivid Light", ""},
      {SEQ_TYPE_LIN_LIGHT, "LINEAR_LIGHT", 0, "Linear Light", ""},
      {SEQ_TYPE_PIN_LIGHT, "PIN_LIGHT", 0, "Pin Light", ""},
      RNA_ENUM_ITEM_SEPR,
      {SEQ_TYPE_DIFFERENCE, "DIFFERENCE", 0, "Difference", ""},
      {SEQ_TYPE_EXCLUSION, "EXCLUSION", 0, "Exclusion", ""},
      {SEQ_TYPE_SUB, "SUBTRACT", 0, "Subtract", ""},
      RNA_ENUM_ITEM_SEPR,
      {SEQ_TYPE_HUE, "HUE", 0, "Hue", ""},
      {SEQ_TYPE_SATURATION, "SATURATION", 0, "Saturation", ""},
      {SEQ_TYPE_BLEND_COLOR, "COLOR", 0, "Color", ""},
      {SEQ_TYPE_VALUE, "VALUE", 0, "Value", ""},
      {0, nullptr, 0, nullptr, nullptr},
  };

  PropertyRNA *prop;

  RNA_def_struct_sdna_from(srna, "ColorMixVars", "effectdata");

  prop = RNA_def_property(srna, "blend_effect", PROP_ENUM, PROP_NONE);
  RNA_def_property_enum_sdna(prop, nullptr, "blend_effect");
  RNA_def_property_enum_items(prop, blend_color_items);
  RNA_def_property_ui_text(
      prop, "Blending Mode", "Method for controlling how the strip combines with other strips");
  RNA_def_property_update(prop, NC_SCENE | ND_SEQUENCER, "rna_Sequence_invalidate_raw_update");

  prop = RNA_def_property(srna, "factor", PROP_FLOAT, PROP_FACTOR);
  RNA_def_property_range(prop, 0.0f, 1.0f);
  RNA_def_property_ui_text(
      prop, "Blend Factor", "Percentage of how much the strip's colors affect other strips");
  RNA_def_property_update(prop, NC_SCENE | ND_SEQUENCER, "rna_Sequence_invalidate_raw_update");
}

static EffectInfo def_effects[] = {
    {"AddSequence", "Add Sequence", "Add Sequence", nullptr, 2},
    {"AdjustmentSequence",
     "Adjustment Layer Sequence",
     "Sequence strip to perform filter adjustments to layers below",
     rna_def_input,
     0},
    {"AlphaOverSequence", "Alpha Over Sequence", "Alpha Over Sequence", nullptr, 2},
    {"AlphaUnderSequence", "Alpha Under Sequence", "Alpha Under Sequence", nullptr, 2},
    {"ColorSequence",
     "Color Sequence",
     "Sequence strip creating an image filled with a single color",
     rna_def_solid_color,
     0},
    {"CrossSequence", "Cross Sequence", "Cross Sequence", nullptr, 2},
    {"GammaCrossSequence", "Gamma Cross Sequence", "Gamma Cross Sequence", nullptr, 2},
    {"GlowSequence", "Glow Sequence", "Sequence strip creating a glow effect", rna_def_glow, 1},
    {"MulticamSequence",
     "Multicam Select Sequence",
     "Sequence strip to perform multicam editing",
     rna_def_multicam,
     0},
    {"MultiplySequence", "Multiply Sequence", "Multiply Sequence", nullptr, 2},
    {"OverDropSequence", "Over Drop Sequence", "Over Drop Sequence", nullptr, 2},
    {"SpeedControlSequence",
     "SpeedControl Sequence",
     "Sequence strip to control the speed of other strips",
     rna_def_speed_control,
     1},
    {"SubtractSequence", "Subtract Sequence", "Subtract Sequence", nullptr, 2},
    {"TransformSequence",
     "Transform Sequence",
     "Sequence strip applying affine transformations to other strips",
     rna_def_transform,
     1},
    {"WipeSequence",
     "Wipe Sequence",
     "Sequence strip creating a wipe transition",
     rna_def_wipe,
     2},
    {"GaussianBlurSequence",
     "Gaussian Blur Sequence",
     "Sequence strip creating a gaussian blur",
     rna_def_gaussian_blur,
     1},
    {"TextSequence", "Text Sequence", "Sequence strip creating text", rna_def_text, 0},
    {"ColorMixSequence", "Color Mix Sequence", "Color Mix Sequence", rna_def_color_mix, 2},
    {"", "", "", nullptr, 0},
};

static void rna_def_effects(BlenderRNA *brna)
{
  StructRNA *srna;
  EffectInfo *effect;

  for (effect = def_effects; effect->struct_name[0] != '\0'; effect++) {
    srna = RNA_def_struct(brna, effect->struct_name, "EffectSequence");
    RNA_def_struct_ui_text(srna, effect->ui_name, effect->ui_desc);
    RNA_def_struct_sdna(srna, "Sequence");

    rna_def_effect_inputs(srna, effect->inputs);

    if (effect->func) {
      effect->func(srna);
    }
  }
}

static void rna_def_modifier(BlenderRNA *brna)
{
  StructRNA *srna;
  PropertyRNA *prop;

  static const EnumPropertyItem mask_input_type_items[] = {
      {SEQUENCE_MASK_INPUT_STRIP, "STRIP", 0, "Strip", "Use sequencer strip as mask input"},
      {SEQUENCE_MASK_INPUT_ID, "ID", 0, "Mask", "Use mask ID as mask input"},
      {0, nullptr, 0, nullptr, nullptr},
  };

  static const EnumPropertyItem mask_time_items[] = {
      {SEQUENCE_MASK_TIME_RELATIVE,
       "RELATIVE",
       0,
       "Relative",
       "Mask animation is offset to start of strip"},
      {SEQUENCE_MASK_TIME_ABSOLUTE,
       "ABSOLUTE",
       0,
       "Absolute",
       "Mask animation is in sync with scene frame"},
      {0, nullptr, 0, nullptr, nullptr},
  };

  srna = RNA_def_struct(brna, "SequenceModifier", nullptr);
  RNA_def_struct_sdna(srna, "SequenceModifierData");
  RNA_def_struct_ui_text(srna, "SequenceModifier", "Modifier for sequence strip");
  RNA_def_struct_refine_func(srna, "rna_SequenceModifier_refine");
  RNA_def_struct_path_func(srna, "rna_SequenceModifier_path");

  prop = RNA_def_property(srna, "name", PROP_STRING, PROP_NONE);
  RNA_def_property_string_funcs(prop, nullptr, nullptr, "rna_SequenceModifier_name_set");
  RNA_def_property_ui_text(prop, "Name", "");
  RNA_def_struct_name_property(srna, prop);
  RNA_def_property_update(prop, NC_SCENE | ND_SEQUENCER, nullptr);

  prop = RNA_def_property(srna, "type", PROP_ENUM, PROP_NONE);
  RNA_def_property_clear_flag(prop, PROP_EDITABLE);
  RNA_def_property_enum_items(prop, rna_enum_sequence_modifier_type_items);
  RNA_def_property_ui_text(prop, "Type", "");
  RNA_def_property_update(prop, NC_SCENE | ND_SEQUENCER, nullptr);

  prop = RNA_def_property(srna, "mute", PROP_BOOLEAN, PROP_NONE);
  RNA_def_property_boolean_sdna(prop, nullptr, "flag", SEQUENCE_MODIFIER_MUTE);
  RNA_def_property_ui_text(prop, "Mute", "Mute this modifier");
  RNA_def_property_ui_icon(prop, ICON_HIDE_OFF, -1);
  RNA_def_property_update(prop, NC_SCENE | ND_SEQUENCER, "rna_SequenceModifier_update");

  prop = RNA_def_property(srna, "show_expanded", PROP_BOOLEAN, PROP_NONE);
  RNA_def_property_flag(prop, PROP_NO_DEG_UPDATE);
  RNA_def_property_boolean_sdna(prop, nullptr, "flag", SEQUENCE_MODIFIER_EXPANDED);
  RNA_def_property_ui_text(prop, "Expanded", "Mute expanded settings for the modifier");
  RNA_def_property_ui_icon(prop, ICON_RIGHTARROW, 1);
  RNA_def_property_update(prop, NC_SCENE | ND_SEQUENCER, nullptr);

  prop = RNA_def_property(srna, "input_mask_type", PROP_ENUM, PROP_NONE);
  RNA_def_property_enum_sdna(prop, nullptr, "mask_input_type");
  RNA_def_property_enum_items(prop, mask_input_type_items);
  RNA_def_property_ui_text(prop, "Mask Input Type", "Type of input data used for mask");
  RNA_def_property_update(prop, NC_SCENE | ND_SEQUENCER, "rna_SequenceModifier_update");

  prop = RNA_def_property(srna, "mask_time", PROP_ENUM, PROP_NONE);
  RNA_def_property_enum_sdna(prop, nullptr, "mask_time");
  RNA_def_property_enum_items(prop, mask_time_items);
  RNA_def_property_ui_text(prop, "Mask Time", "Time to use for the Mask animation");
  RNA_def_property_update(prop, NC_SCENE | ND_SEQUENCER, "rna_SequenceModifier_update");

  prop = RNA_def_property(srna, "input_mask_strip", PROP_POINTER, PROP_NONE);
  RNA_def_property_pointer_sdna(prop, nullptr, "mask_sequence");
  RNA_def_property_pointer_funcs(prop,
                                 nullptr,
                                 "rna_SequenceModifier_strip_set",
                                 nullptr,
                                 "rna_SequenceModifier_otherSequence_poll");
  RNA_def_property_flag(prop, PROP_EDITABLE);
  RNA_def_property_ui_text(prop, "Mask Strip", "Strip used as mask input for the modifier");
  RNA_def_property_update(prop, NC_SCENE | ND_SEQUENCER, "rna_SequenceModifier_update");

  prop = RNA_def_property(srna, "input_mask_id", PROP_POINTER, PROP_NONE);
  RNA_def_property_pointer_sdna(prop, nullptr, "mask_id");
  RNA_def_property_flag(prop, PROP_EDITABLE);
  RNA_def_property_ui_text(prop, "Mask", "Mask ID used as mask input for the modifier");
  RNA_def_property_update(prop, NC_SCENE | ND_SEQUENCER, "rna_SequenceModifier_update");
}

static void rna_def_colorbalance_modifier(BlenderRNA *brna)
{
  StructRNA *srna;
  PropertyRNA *prop;

  srna = RNA_def_struct(brna, "ColorBalanceModifier", "SequenceModifier");
  RNA_def_struct_sdna(srna, "ColorBalanceModifierData");
  RNA_def_struct_ui_text(
      srna, "ColorBalanceModifier", "Color balance modifier for sequence strip");

  prop = RNA_def_property(srna, "color_balance", PROP_POINTER, PROP_NONE);
  RNA_def_property_struct_type(prop, "SequenceColorBalanceData");

  prop = RNA_def_property(srna, "color_multiply", PROP_FLOAT, PROP_UNSIGNED);
  RNA_def_property_float_sdna(prop, nullptr, "color_multiply");
  RNA_def_property_range(prop, 0.0f, 20.0f);
  RNA_def_property_float_default(prop, 1.0f);
  RNA_def_property_ui_text(prop, "Multiply Colors", "Multiply the intensity of each pixel");
  RNA_def_property_update(prop, NC_SCENE | ND_SEQUENCER, "rna_SequenceModifier_update");
}

static void rna_def_whitebalance_modifier(BlenderRNA *brna)
{
  StructRNA *srna;
  PropertyRNA *prop;

  srna = RNA_def_struct(brna, "WhiteBalanceModifier", "SequenceModifier");
  RNA_def_struct_sdna(srna, "WhiteBalanceModifierData");
  RNA_def_struct_ui_text(
      srna, "WhiteBalanceModifier", "White balance modifier for sequence strip");

  prop = RNA_def_property(srna, "white_value", PROP_FLOAT, PROP_COLOR_GAMMA);
  RNA_def_property_range(prop, 0.0, 1.0);
  RNA_def_property_float_sdna(prop, nullptr, "white_value");
  RNA_def_property_ui_text(prop, "White Value", "This color defines white in the strip");
  RNA_def_property_update(prop, NC_SCENE | ND_SEQUENCER, "rna_SequenceModifier_update");
}

static void rna_def_curves_modifier(BlenderRNA *brna)
{
  StructRNA *srna;
  PropertyRNA *prop;

  srna = RNA_def_struct(brna, "CurvesModifier", "SequenceModifier");
  RNA_def_struct_sdna(srna, "CurvesModifierData");
  RNA_def_struct_ui_text(srna, "CurvesModifier", "RGB curves modifier for sequence strip");

  prop = RNA_def_property(srna, "curve_mapping", PROP_POINTER, PROP_NONE);
  RNA_def_property_pointer_sdna(prop, nullptr, "curve_mapping");
  RNA_def_property_struct_type(prop, "CurveMapping");
  RNA_def_property_ui_text(prop, "Curve Mapping", "");
  RNA_def_property_update(prop, NC_SCENE | ND_SEQUENCER, "rna_SequenceModifier_update");
}

static void rna_def_hue_modifier(BlenderRNA *brna)
{
  StructRNA *srna;
  PropertyRNA *prop;

  srna = RNA_def_struct(brna, "HueCorrectModifier", "SequenceModifier");
  RNA_def_struct_sdna(srna, "HueCorrectModifierData");
  RNA_def_struct_ui_text(srna, "HueCorrectModifier", "Hue correction modifier for sequence strip");

  prop = RNA_def_property(srna, "curve_mapping", PROP_POINTER, PROP_NONE);
  RNA_def_property_pointer_sdna(prop, nullptr, "curve_mapping");
  RNA_def_property_struct_type(prop, "CurveMapping");
  RNA_def_property_ui_text(prop, "Curve Mapping", "");
  RNA_def_property_update(prop, NC_SCENE | ND_SEQUENCER, "rna_SequenceModifier_update");
}

static void rna_def_brightcontrast_modifier(BlenderRNA *brna)
{
  StructRNA *srna;
  PropertyRNA *prop;

  srna = RNA_def_struct(brna, "BrightContrastModifier", "SequenceModifier");
  RNA_def_struct_sdna(srna, "BrightContrastModifierData");
  RNA_def_struct_ui_text(
      srna, "BrightContrastModifier", "Bright/contrast modifier data for sequence strip");

  prop = RNA_def_property(srna, "bright", PROP_FLOAT, PROP_UNSIGNED);
  RNA_def_property_float_sdna(prop, nullptr, "bright");
  RNA_def_property_range(prop, -FLT_MAX, FLT_MAX);
  RNA_def_property_ui_text(prop, "Bright", "Adjust the luminosity of the colors");
  RNA_def_property_update(prop, NC_SCENE | ND_SEQUENCER, "rna_SequenceModifier_update");

  prop = RNA_def_property(srna, "contrast", PROP_FLOAT, PROP_UNSIGNED);
  RNA_def_property_float_sdna(prop, nullptr, "contrast");
  RNA_def_property_range(prop, -100.0f, 100.0f);
  RNA_def_property_ui_text(prop, "Contrast", "Adjust the difference in luminosity between pixels");
  RNA_def_property_update(prop, NC_SCENE | ND_SEQUENCER, "rna_SequenceModifier_update");
}

static void rna_def_tonemap_modifier(BlenderRNA *brna)
{
  StructRNA *srna;
  PropertyRNA *prop;

  static const EnumPropertyItem type_items[] = {
      {SEQ_TONEMAP_RD_PHOTORECEPTOR, "RD_PHOTORECEPTOR", 0, "R/D Photoreceptor", ""},
      {SEQ_TONEMAP_RH_SIMPLE, "RH_SIMPLE", 0, "Rh Simple", ""},
      {0, nullptr, 0, nullptr, nullptr},
  };

  srna = RNA_def_struct(brna, "SequencerTonemapModifierData", "SequenceModifier");
  RNA_def_struct_sdna(srna, "SequencerTonemapModifierData");
  RNA_def_struct_ui_text(srna, "SequencerTonemapModifierData", "Tone mapping modifier");

  prop = RNA_def_property(srna, "tonemap_type", PROP_ENUM, PROP_NONE);
  RNA_def_property_enum_sdna(prop, nullptr, "type");
  RNA_def_property_enum_items(prop, type_items);
  RNA_def_property_ui_text(prop, "Tonemap Type", "Tone mapping algorithm");
  RNA_def_property_update(prop, NC_SCENE | ND_SEQUENCER, "rna_SequenceModifier_update");

  prop = RNA_def_property(srna, "key", PROP_FLOAT, PROP_FACTOR);
  RNA_def_property_range(prop, 0.0f, 1.0f);
  RNA_def_property_ui_text(prop, "Key", "The value the average luminance is mapped to");
  RNA_def_property_update(prop, NC_SCENE | ND_SEQUENCER, "rna_SequenceModifier_update");

  prop = RNA_def_property(srna, "offset", PROP_FLOAT, PROP_NONE);
  RNA_def_property_range(prop, 0.001f, 10.0f);
  RNA_def_property_ui_text(
      prop,
      "Offset",
      "Normally always 1, but can be used as an extra control to alter the brightness curve");
  RNA_def_property_update(prop, NC_SCENE | ND_SEQUENCER, "rna_SequenceModifier_update");

  prop = RNA_def_property(srna, "gamma", PROP_FLOAT, PROP_NONE);
  RNA_def_property_range(prop, 0.001f, 3.0f);
  RNA_def_property_ui_text(prop, "Gamma", "If not used, set to 1");
  RNA_def_property_update(prop, NC_SCENE | ND_SEQUENCER, "rna_SequenceModifier_update");

  prop = RNA_def_property(srna, "intensity", PROP_FLOAT, PROP_NONE);
  RNA_def_property_range(prop, -8.0f, 8.0f);
  RNA_def_property_ui_text(
      prop, "Intensity", "If less than zero, darkens image; otherwise, makes it brighter");
  RNA_def_property_update(prop, NC_SCENE | ND_SEQUENCER, "rna_SequenceModifier_update");

  prop = RNA_def_property(srna, "contrast", PROP_FLOAT, PROP_FACTOR);
  RNA_def_property_range(prop, 0.0f, 1.0f);
  RNA_def_property_ui_text(prop, "Contrast", "Set to 0 to use estimate from input image");
  RNA_def_property_update(prop, NC_SCENE | ND_SEQUENCER, "rna_SequenceModifier_update");

  prop = RNA_def_property(srna, "adaptation", PROP_FLOAT, PROP_FACTOR);
  RNA_def_property_range(prop, 0.0f, 1.0f);
  RNA_def_property_ui_text(prop, "Adaptation", "If 0, global; if 1, based on pixel intensity");
  RNA_def_property_update(prop, NC_SCENE | ND_SEQUENCER, "rna_SequenceModifier_update");

  prop = RNA_def_property(srna, "correction", PROP_FLOAT, PROP_FACTOR);
  RNA_def_property_range(prop, 0.0f, 1.0f);
  RNA_def_property_ui_text(
      prop, "Color Correction", "If 0, same for all channels; if 1, each independent");
  RNA_def_property_update(prop, NC_SCENE | ND_SEQUENCER, "rna_SequenceModifier_update");
}

static void rna_def_modifiers(BlenderRNA *brna)
{
  rna_def_modifier(brna);

  rna_def_colorbalance_modifier(brna);
  rna_def_curves_modifier(brna);
  rna_def_hue_modifier(brna);
  rna_def_brightcontrast_modifier(brna);
  rna_def_whitebalance_modifier(brna);
  rna_def_tonemap_modifier(brna);
}

static void rna_def_graphical_sound_equalizer(BlenderRNA *brna)
{
  StructRNA *srna;
  PropertyRNA *prop;

  /* Define Sound EQ */
  srna = RNA_def_struct(brna, "EQCurveMappingData", nullptr);
  RNA_def_struct_sdna(srna, "EQCurveMappingData");
  RNA_def_struct_ui_text(srna, "EQCurveMappingData", "EQCurveMappingData");

  prop = RNA_def_property(srna, "curve_mapping", PROP_POINTER, PROP_NONE);
  RNA_def_property_pointer_sdna(prop, nullptr, "curve_mapping");
  RNA_def_property_struct_type(prop, "CurveMapping");
  RNA_def_property_ui_text(prop, "Curve Mapping", "");
  RNA_def_property_update(
      prop, NC_SCENE | ND_SEQUENCER, "rna_SequenceModifier_EQCurveMapping_update");
}

static void rna_def_sound_equalizer_modifier(BlenderRNA *brna)
{
  StructRNA *srna;
  PropertyRNA *prop;
  FunctionRNA *func;
  PropertyRNA *parm;

  srna = RNA_def_struct(brna, "SoundEqualizerModifier", "SequenceModifier");
  RNA_def_struct_sdna(srna, "SoundEqualizerModifierData");
  RNA_def_struct_ui_text(srna, "SoundEqualizerModifier", "Equalize audio");

  /* Sound Equalizers. */
  prop = RNA_def_property(srna, "graphics", PROP_COLLECTION, PROP_NONE);
  RNA_def_property_struct_type(prop, "EQCurveMappingData");
  RNA_def_property_ui_text(
      prop, "Graphical definition equalization", "Graphical definition equalization");

  /* Add band. */
  func = RNA_def_function(srna, "new_graphic", "rna_Sequence_SoundEqualizer_Curve_add");
  RNA_def_function_flag(func, FUNC_USE_CONTEXT);
  RNA_def_function_ui_description(func, "Add a new EQ band");

  parm = RNA_def_float(func,
                       "min_freq",
                       SOUND_EQUALIZER_DEFAULT_MIN_FREQ,
                       0.0,
                       SOUND_EQUALIZER_DEFAULT_MAX_FREQ, /*  Hard min and max */
                       "Minimum Frequency",
                       "Minimum Frequency",
                       0.0,
                       SOUND_EQUALIZER_DEFAULT_MAX_FREQ); /*  Soft min and max */
  RNA_def_parameter_flags(parm, PropertyFlag(0), PARM_REQUIRED);
  parm = RNA_def_float(func,
                       "max_freq",
                       SOUND_EQUALIZER_DEFAULT_MAX_FREQ,
                       0.0,
                       SOUND_EQUALIZER_DEFAULT_MAX_FREQ, /*  Hard min and max */
                       "Maximum Frequency",
                       "Maximum Frequency",
                       0.0,
                       SOUND_EQUALIZER_DEFAULT_MAX_FREQ); /*  Soft min and max */
  RNA_def_parameter_flags(parm, PropertyFlag(0), PARM_REQUIRED);

  /* return type */
  parm = RNA_def_pointer(func,
                         "graphic_eqs",
                         "EQCurveMappingData",
                         "",
                         "Newly created graphical Equalizer definition");
  RNA_def_function_return(func, parm);

  /* clear all modifiers */
  func = RNA_def_function(srna, "clear_soundeqs", "rna_Sequence_SoundEqualizer_Curve_clear");
  RNA_def_function_flag(func, FUNC_USE_CONTEXT);
  RNA_def_function_ui_description(func,
                                  "Remove all graphical equalizers from the Equalizer modifier");

  rna_def_graphical_sound_equalizer(brna);
}

static void rna_def_sound_modifiers(BlenderRNA *brna)
{
  rna_def_sound_equalizer_modifier(brna);
}

void RNA_def_sequencer(BlenderRNA *brna)
{
  rna_def_color_balance(brna);

  rna_def_strip_element(brna);
  rna_def_retiming_key(brna);
  rna_def_strip_proxy(brna);
  rna_def_strip_color_balance(brna);
  rna_def_strip_crop(brna);
  rna_def_strip_transform(brna);

  rna_def_sequence(brna);
  rna_def_editor(brna);
  rna_def_channel(brna);

  rna_def_image(brna);
  rna_def_meta(brna);
  rna_def_scene(brna);
  rna_def_movie(brna);
  rna_def_movieclip(brna);
  rna_def_mask(brna);
  rna_def_sound(brna);
  rna_def_effect(brna);
  rna_def_effects(brna);
  rna_def_modifiers(brna);
  rna_def_sound_modifiers(brna);
}

#endif
