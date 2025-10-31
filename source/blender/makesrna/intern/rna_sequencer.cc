/* SPDX-FileCopyrightText: 2023 Blender Authors
 *
 * SPDX-License-Identifier: GPL-2.0-or-later */

/** \file
 * \ingroup RNA
 */

#include <cstdlib>

#include "DNA_scene_types.h"
#include "DNA_sequence_types.h"

#include "BLI_math_rotation.h"
#include "BLI_string_utf8_symbols.h"

#include "BLT_translation.hh"

#include "BKE_animsys.h"

#include "RNA_define.hh"
#include "RNA_enum_types.hh"
#include "RNA_types.hh"
#include "rna_internal.hh"

#include "UI_resources.hh"

#include "SEQ_effects.hh"
#include "SEQ_sequencer.hh"
#include "SEQ_sound.hh"

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
  {eSeqModifierType_BrightContrast, "BRIGHT_CONTRAST", ICON_MOD_BRIGHTNESS_CONTRAST, "Brightness/Contrast", ""}, \
  {eSeqModifierType_ColorBalance, "COLOR_BALANCE", ICON_MOD_COLOR_BALANCE, "Color Balance", ""}, \
  {eSeqModifierType_Compositor, "COMPOSITOR", ICON_NODE_COMPOSITING, "Compositor", ""}, \
  {eSeqModifierType_Curves, "CURVES", ICON_MOD_CURVES, "Curves", ""}, \
  {eSeqModifierType_HueCorrect, "HUE_CORRECT", ICON_MOD_HUE_CORRECT, "Hue Correct", ""}, \
  {eSeqModifierType_Mask, "MASK", ICON_MOD_MASK, "Mask", ""}, \
  {eSeqModifierType_Tonemap, "TONEMAP", ICON_MOD_TONEMAP, "Tone Map", ""}, \
  {eSeqModifierType_WhiteBalance, "WHITE_BALANCE", ICON_MOD_WHITE_BALANCE, "White Balance", ""}

#define RNA_ENUM_SEQUENCER_AUDIO_MODIFIER_TYPE_ITEMS \
  {eSeqModifierType_SoundEqualizer, "SOUND_EQUALIZER", ICON_NONE, "Sound Equalizer", ""}
/* clang-format on */

const EnumPropertyItem rna_enum_strip_modifier_type_items[] = {
    RNA_ENUM_SEQUENCER_VIDEO_MODIFIER_TYPE_ITEMS,
    RNA_ENUM_SEQUENCER_AUDIO_MODIFIER_TYPE_ITEMS,
    {0, nullptr, 0, nullptr, nullptr},
};

const EnumPropertyItem rna_enum_strip_video_modifier_type_items[] = {
    RNA_ENUM_SEQUENCER_VIDEO_MODIFIER_TYPE_ITEMS,
    {0, nullptr, 0, nullptr, nullptr},
};

const EnumPropertyItem rna_enum_strip_sound_modifier_type_items[] = {
    RNA_ENUM_SEQUENCER_AUDIO_MODIFIER_TYPE_ITEMS,
    {0, nullptr, 0, nullptr, nullptr},
};

const EnumPropertyItem rna_enum_strip_color_items[] = {
    {STRIP_COLOR_NONE, "NONE", ICON_X, "None", "Assign no color tag to the collection"},
    {STRIP_COLOR_01, "COLOR_01", ICON_STRIP_COLOR_01, "Color 01", ""},
    {STRIP_COLOR_02, "COLOR_02", ICON_STRIP_COLOR_02, "Color 02", ""},
    {STRIP_COLOR_03, "COLOR_03", ICON_STRIP_COLOR_03, "Color 03", ""},
    {STRIP_COLOR_04, "COLOR_04", ICON_STRIP_COLOR_04, "Color 04", ""},
    {STRIP_COLOR_05, "COLOR_05", ICON_STRIP_COLOR_05, "Color 05", ""},
    {STRIP_COLOR_06, "COLOR_06", ICON_STRIP_COLOR_06, "Color 06", ""},
    {STRIP_COLOR_07, "COLOR_07", ICON_STRIP_COLOR_07, "Color 07", ""},
    {STRIP_COLOR_08, "COLOR_08", ICON_STRIP_COLOR_08, "Color 08", ""},
    {STRIP_COLOR_09, "COLOR_09", ICON_STRIP_COLOR_09, "Color 09", ""},
    {0, nullptr, 0, nullptr, nullptr},
};

const EnumPropertyItem rna_enum_strip_scale_method_items[] = {
    {SEQ_SCALE_TO_FIT,
     "FIT",
     0,
     "Scale to Fit",
     "Fits the image bounds inside the canvas, avoiding crops while maintaining aspect ratio"},
    {SEQ_SCALE_TO_FILL,
     "FILL",
     0,
     "Scale to Fill",
     "Fills the canvas edge-to-edge, cropping if needed, while maintaining aspect ratio"},
    {SEQ_STRETCH_TO_FILL,
     "STRETCH",
     0,
     "Stretch to Fill",
     "Stretches image bounds to the canvas without preserving aspect ratio"},
    {SEQ_USE_ORIGINAL_SIZE,
     "ORIGINAL",
     0,
     "Use Original Size",
     "Display image at its original size"},
    {0, nullptr, 0, nullptr, nullptr},
};

#ifdef RNA_RUNTIME

#  include <algorithm>

#  include <fmt/format.h>

#  include "DNA_vfont_types.h"

#  include "BLI_iterator.h"
#  include "BLI_string_utils.hh"

#  include "BKE_anim_data.hh"
#  include "BKE_global.hh"
#  include "BKE_idprop.hh"
#  include "BKE_lib_id.hh"
#  include "BKE_movieclip.h"
#  include "BKE_report.hh"

#  include "WM_api.hh"

#  include "DEG_depsgraph.hh"
#  include "DEG_depsgraph_build.hh"

#  include "IMB_imbuf.hh"

#  include "MOV_read.hh"

#  include "ED_sequencer.hh"

#  include "SEQ_add.hh"
#  include "SEQ_channels.hh"
#  include "SEQ_edit.hh"
#  include "SEQ_effects.hh"
#  include "SEQ_iterator.hh"
#  include "SEQ_modifier.hh"
#  include "SEQ_prefetch.hh"
#  include "SEQ_proxy.hh"
#  include "SEQ_relations.hh"
#  include "SEQ_retiming.hh"
#  include "SEQ_select.hh"
#  include "SEQ_sequencer.hh"
#  include "SEQ_sound.hh"
#  include "SEQ_thumbnail_cache.hh"
#  include "SEQ_time.hh"
#  include "SEQ_transform.hh"
#  include "SEQ_utils.hh"

struct StripSearchData {
  Strip *strip;
  void *data;
  StripModifierData *smd;
};

static void rna_StripElement_update(Main * /*bmain*/, Scene * /*scene*/, PointerRNA *ptr)
{
  Scene *scene = (Scene *)ptr->owner_id;
  Editing *ed = blender::seq::editing_get(scene);

  if (ed) {
    StripElem *se = (StripElem *)ptr->data;
    Strip *strip;

    /* slow but we can't avoid! */
    strip = blender::seq::strip_from_strip_elem(&ed->seqbase, se);
    if (strip) {
      blender::seq::relations_invalidate_cache_raw(scene, strip);
    }
  }
}

static void rna_Strip_invalidate_raw_update(Main * /*bmain*/, Scene * /*scene*/, PointerRNA *ptr)
{
  Scene *scene = (Scene *)ptr->owner_id;
  Editing *ed = blender::seq::editing_get(scene);

  if (ed) {
    Strip *strip = (Strip *)ptr->data;

    blender::seq::relations_invalidate_cache_raw(scene, strip);
  }
}

static void rna_Strip_invalidate_preprocessed_update(Main * /*bmain*/,
                                                     Scene * /*scene*/,
                                                     PointerRNA *ptr)
{
  Scene *scene = (Scene *)ptr->owner_id;
  Editing *ed = blender::seq::editing_get(scene);

  if (ed) {
    Strip *strip = (Strip *)ptr->data;

    blender::seq::relations_invalidate_cache(scene, strip);
  }
}

static void rna_Strip_mute_update(bContext *C, PointerRNA *ptr)
{
  blender::ed::vse::sync_active_scene_and_time_with_scene_strip(*C);
  rna_Strip_invalidate_raw_update(nullptr, nullptr, ptr);
}

static void UNUSED_FUNCTION(rna_Strip_invalidate_composite_update)(Main * /*bmain*/,
                                                                   Scene * /*scene*/,
                                                                   PointerRNA *ptr)
{
  Scene *scene = (Scene *)ptr->owner_id;
  Editing *ed = blender::seq::editing_get(scene);

  if (ed) {
    Strip *strip = (Strip *)ptr->data;

    blender::seq::relations_invalidate_cache(scene, strip);
  }
}

static void rna_Strip_scene_switch_update(Main *bmain, Scene *scene, PointerRNA *ptr)
{
  rna_Strip_invalidate_raw_update(bmain, scene, ptr);
  DEG_id_tag_update(&scene->id, ID_RECALC_AUDIO | ID_RECALC_SEQUENCER_STRIPS);
  DEG_relations_tag_update(bmain);
}

static void rna_Strip_use_strip(Main *bmain, Scene * /*scene*/, PointerRNA *ptr)
{
  Scene *scene = reinterpret_cast<Scene *>(ptr->owner_id);

  /* General update callback. */
  rna_Strip_invalidate_raw_update(bmain, scene, ptr);
  /* Changing recursion changes set of IDs which needs to be remapped by the copy-on-evaluation.
   * the only way for this currently is to tag the ID for ID_RECALC_SYNC_TO_EVAL. */
  Editing *ed = blender::seq::editing_get(scene);
  if (ed) {
    Strip *strip = (Strip *)ptr->data;
    if (strip->scene != nullptr) {
      DEG_id_tag_update(&strip->scene->id, ID_RECALC_SYNC_TO_EVAL);
    }
  }
  /* The sequencer scene is to be updated as well, including new relations from the nested
   * sequencer. */
  DEG_id_tag_update(&scene->id, ID_RECALC_SEQUENCER_STRIPS);
  DEG_relations_tag_update(bmain);
}

static void add_strips_from_seqbase(const ListBase *seqbase, blender::Vector<Strip *> &strips)
{
  LISTBASE_FOREACH (Strip *, strip, seqbase) {
    strips.append(strip);

    if (strip->type == STRIP_TYPE_META) {
      add_strips_from_seqbase(&strip->seqbase, strips);
    }
  }
}

struct StripsAllIterator {
  blender::Vector<Strip *> strips;
  int index;
};

static std::optional<std::string> rna_SequenceEditor_path(const PointerRNA * /*ptr*/)
{
  return "sequence_editor";
}

static void rna_SequenceEditor_strips_all_begin(CollectionPropertyIterator *iter, PointerRNA *ptr)
{
  Scene *scene = (Scene *)ptr->owner_id;
  Editing *ed = blender::seq::editing_get(scene);

  StripsAllIterator *strip_iter = MEM_new<StripsAllIterator>(__func__);
  strip_iter->index = 0;
  add_strips_from_seqbase(&ed->seqbase, strip_iter->strips);

  BLI_Iterator *bli_iter = MEM_callocN<BLI_Iterator>(__func__);
  iter->internal.custom = bli_iter;
  bli_iter->data = strip_iter;

  Strip **strip_arr = strip_iter->strips.begin();
  bli_iter->current = *strip_arr;
  iter->valid = bli_iter->current != nullptr;
}

static void rna_SequenceEditor_strips_all_next(CollectionPropertyIterator *iter)
{
  BLI_Iterator *bli_iter = static_cast<BLI_Iterator *>(iter->internal.custom);
  StripsAllIterator *strip_iter = static_cast<StripsAllIterator *>(bli_iter->data);

  strip_iter->index++;
  Strip **strip_arr = strip_iter->strips.begin();
  bli_iter->current = *(strip_arr + strip_iter->index);

  iter->valid = bli_iter->current != nullptr && strip_iter->index < strip_iter->strips.size();
}

static PointerRNA rna_SequenceEditor_strips_all_get(CollectionPropertyIterator *iter)
{
  Strip *strip = static_cast<Strip *>(((BLI_Iterator *)iter->internal.custom)->current);
  return RNA_pointer_create_with_parent(iter->parent, &RNA_Strip, strip);
}

static void rna_SequenceEditor_strips_all_end(CollectionPropertyIterator *iter)
{
  BLI_Iterator *bli_iter = static_cast<BLI_Iterator *>(iter->internal.custom);
  StripsAllIterator *strip_iter = static_cast<StripsAllIterator *>(bli_iter->data);

  MEM_delete(strip_iter);
  MEM_freeN(bli_iter);
}

static bool rna_SequenceEditor_strips_all_lookup_string(PointerRNA *ptr,
                                                        const char *key,
                                                        PointerRNA *r_ptr)
{
  ID *id = ptr->owner_id;
  Scene *scene = (Scene *)id;

  Strip *strip = blender::seq::lookup_strip_by_name(scene->ed, key);
  if (strip) {
    rna_pointer_create_with_ancestors(*ptr, &RNA_Strip, strip, *r_ptr);
    return true;
  }
  return false;
}

static void rna_SequenceEditor_update_cache(Main * /*bmain*/, Scene *scene, PointerRNA * /*ptr*/)
{
  Editing *ed = scene->ed;

  blender::seq::relations_free_imbuf(scene, &ed->seqbase, false);
  blender::seq::cache_cleanup(scene, blender::seq::CacheCleanup::FinalAndIntra);
}

static void rna_SequenceEditor_cache_settings_changed(Main * /*bmain*/,
                                                      Scene *scene,
                                                      PointerRNA * /*ptr*/)
{
  blender::seq::cache_settings_changed(scene);
}

/* internal use */
static int rna_Strip_elements_length(PointerRNA *ptr)
{
  Strip *strip = (Strip *)ptr->data;

  /* Hack? copied from `sequencer.cc`, #reload_sequence_new_file(). */
  size_t olen = MEM_allocN_len(strip->data->stripdata) / sizeof(StripElem);

  /* The problem with `strip->data->len` and `strip->len` is that it's discounted from the offset
   * (hard cut trim). */
  return int(olen);
}

static void rna_Strip_elements_begin(CollectionPropertyIterator *iter, PointerRNA *ptr)
{
  Strip *strip = (Strip *)ptr->data;
  rna_iterator_array_begin(iter,
                           ptr,
                           (void *)strip->data->stripdata,
                           sizeof(StripElem),
                           rna_Strip_elements_length(ptr),
                           0,
                           nullptr);
}

static int rna_Strip_retiming_keys_length(PointerRNA *ptr)
{
  return blender::seq::retiming_keys_count((Strip *)ptr->data);
}

static void rna_Strip_retiming_keys_begin(CollectionPropertyIterator *iter, PointerRNA *ptr)
{
  Strip *strip = (Strip *)ptr->data;
  rna_iterator_array_begin(iter,
                           ptr,
                           (void *)strip->retiming_keys,
                           sizeof(SeqRetimingKey),
                           blender::seq::retiming_keys_count(strip),
                           0,
                           nullptr);
}

static Strip *strip_by_key_find(Scene *scene, SeqRetimingKey *key)
{
  Editing *ed = blender::seq::editing_get(scene);
  blender::VectorSet strips = blender::seq::query_all_strips_recursive(&ed->seqbase);

  for (Strip *strip : strips) {
    const int retiming_keys_count = blender::seq::retiming_keys_count(strip);
    SeqRetimingKey *first = strip->retiming_keys;
    SeqRetimingKey *last = strip->retiming_keys + retiming_keys_count - 1;

    if (key >= first && key <= last) {
      return strip;
    }
  }

  return nullptr;
}

static void rna_Strip_retiming_key_remove(ID *id, SeqRetimingKey *key)
{
  Scene *scene = (Scene *)id;
  Strip *strip = strip_by_key_find(scene, key);

  if (strip == nullptr) {
    return;
  }

  blender::seq::retiming_remove_key(strip, key);

  blender::seq::relations_invalidate_cache_raw(scene, strip);
  WM_main_add_notifier(NC_SCENE | ND_SEQUENCER, nullptr);
}

static int rna_Strip_retiming_key_frame_get(PointerRNA *ptr)
{
  SeqRetimingKey *key = (SeqRetimingKey *)ptr->data;
  Scene *scene = (Scene *)ptr->owner_id;
  Strip *strip = strip_by_key_find(scene, key);

  if (strip == nullptr) {
    return 0;
  }

  return blender::seq::time_start_frame_get(strip) + key->strip_frame_index;
}

static void rna_Strip_retiming_key_frame_set(PointerRNA *ptr, int value)
{
  SeqRetimingKey *key = (SeqRetimingKey *)ptr->data;
  Scene *scene = (Scene *)ptr->owner_id;
  Strip *strip = strip_by_key_find(scene, key);

  if (strip == nullptr) {
    return;
  }

  blender::seq::retiming_key_timeline_frame_set(scene, strip, key, value);
  blender::seq::relations_invalidate_cache_raw(scene, strip);
}

static bool rna_SequenceEditor_selected_retiming_key_get(PointerRNA *ptr)
{
  Scene *scene = (Scene *)ptr->owner_id;
  return blender::seq::retiming_selection_get(blender::seq::editing_get(scene)).size() != 0;
}

static void rna_Strip_views_format_update(Main *bmain, Scene *scene, PointerRNA *ptr)
{
  rna_Strip_invalidate_raw_update(bmain, scene, ptr);
}

static void do_strip_frame_change_update(Scene *scene, Strip *strip)
{
  ListBase *seqbase = blender::seq::get_seqbase_by_strip(scene, strip);

  if (blender::seq::transform_test_overlap(scene, seqbase, strip)) {
    blender::seq::transform_seqbase_shuffle(seqbase, strip, scene);
  }

  if (strip->type == STRIP_TYPE_SOUND_RAM) {
    DEG_id_tag_update(&scene->id, ID_RECALC_SEQUENCER_STRIPS);
  }
}

/* A simple wrapper around above func, directly usable as prop update func.
 * Also invalidate cache if needed.
 */
static void rna_Strip_frame_change_update(Main * /*bmain*/, Scene * /*scene*/, PointerRNA *ptr)
{
  Scene *scene = (Scene *)ptr->owner_id;
  do_strip_frame_change_update(scene, (Strip *)ptr->data);
}

static int rna_Strip_frame_final_start_get(PointerRNA *ptr)
{
  Scene *scene = (Scene *)ptr->owner_id;
  return blender::seq::time_left_handle_frame_get(scene, (Strip *)ptr->data);
}

static int rna_Strip_frame_final_end_get(PointerRNA *ptr)
{
  Scene *scene = (Scene *)ptr->owner_id;
  return blender::seq::time_right_handle_frame_get(scene, (Strip *)ptr->data);
}

static void rna_Strip_start_frame_final_set(PointerRNA *ptr, int value)
{
  Strip *strip = (Strip *)ptr->data;
  Scene *scene = (Scene *)ptr->owner_id;

  blender::seq::time_left_handle_frame_set(scene, strip, value);
  do_strip_frame_change_update(scene, strip);
  blender::seq::relations_invalidate_cache(scene, strip);
}

static void rna_Strip_end_frame_final_set(PointerRNA *ptr, int value)
{
  Strip *strip = (Strip *)ptr->data;
  Scene *scene = (Scene *)ptr->owner_id;

  blender::seq::time_right_handle_frame_set(scene, strip, value);
  do_strip_frame_change_update(scene, strip);
  blender::seq::relations_invalidate_cache(scene, strip);
}

static void rna_Strip_start_frame_set(PointerRNA *ptr, float value)
{
  Strip *strip = (Strip *)ptr->data;
  Scene *scene = (Scene *)ptr->owner_id;

  blender::seq::transform_translate_strip(scene, strip, value - strip->start);
  do_strip_frame_change_update(scene, strip);
  blender::seq::relations_invalidate_cache(scene, strip);
}

static void rna_Strip_frame_offset_start_set(PointerRNA *ptr, float value)
{
  Strip *strip = (Strip *)ptr->data;
  Scene *scene = (Scene *)ptr->owner_id;

  blender::seq::relations_invalidate_cache(scene, strip);
  strip->startofs = value;
}

static void rna_Strip_frame_offset_end_set(PointerRNA *ptr, float value)
{
  Strip *strip = (Strip *)ptr->data;
  Scene *scene = (Scene *)ptr->owner_id;

  blender::seq::relations_invalidate_cache(scene, strip);
  strip->endofs = value;
}

static void rna_Strip_anim_startofs_final_set(PointerRNA *ptr, int value)
{
  Strip *strip = (Strip *)ptr->data;
  Scene *scene = (Scene *)ptr->owner_id;

  strip->anim_startofs = std::min(value, strip->len + strip->anim_startofs);

  blender::seq::add_reload_new_file(G.main, scene, strip, false);
  do_strip_frame_change_update(scene, strip);
}

static void rna_Strip_anim_endofs_final_set(PointerRNA *ptr, int value)
{
  Strip *strip = (Strip *)ptr->data;
  Scene *scene = (Scene *)ptr->owner_id;

  strip->anim_endofs = std::min(value, strip->len + strip->anim_endofs);

  blender::seq::add_reload_new_file(G.main, scene, strip, false);
  do_strip_frame_change_update(scene, strip);
}

static void rna_Strip_anim_endofs_final_range(
    PointerRNA *ptr, int *min, int *max, int * /*softmin*/, int * /*softmax*/)
{
  Strip *strip = (Strip *)ptr->data;

  *min = 0;
  *max = strip->len + strip->anim_endofs - strip->startofs - strip->endofs - 1;
}

static void rna_Strip_anim_startofs_final_range(
    PointerRNA *ptr, int *min, int *max, int * /*softmin*/, int * /*softmax*/)
{
  Strip *strip = (Strip *)ptr->data;

  *min = 0;
  *max = strip->len + strip->anim_startofs - strip->startofs - strip->endofs - 1;
}

static void rna_Strip_frame_offset_start_range(
    PointerRNA *ptr, float *min, float *max, float * /*softmin*/, float * /*softmax*/)
{
  Strip *strip = (Strip *)ptr->data;
  *min = INT_MIN;
  *max = strip->len - strip->endofs - 1;
}

static void rna_Strip_frame_offset_end_range(
    PointerRNA *ptr, float *min, float *max, float * /*softmin*/, float * /*softmax*/)
{
  Strip *strip = (Strip *)ptr->data;
  *min = INT_MIN;
  *max = strip->len - strip->startofs - 1;
}

static void rna_Strip_frame_length_set(PointerRNA *ptr, int value)
{
  Strip *strip = (Strip *)ptr->data;
  Scene *scene = (Scene *)ptr->owner_id;

  blender::seq::time_right_handle_frame_set(
      scene, strip, blender::seq::time_left_handle_frame_get(scene, strip) + value);
  do_strip_frame_change_update(scene, strip);
  blender::seq::relations_invalidate_cache(scene, strip);
}

static int rna_Strip_frame_length_get(PointerRNA *ptr)
{
  Strip *strip = (Strip *)ptr->data;
  Scene *scene = (Scene *)ptr->owner_id;
  return blender::seq::time_right_handle_frame_get(scene, strip) -
         blender::seq::time_left_handle_frame_get(scene, strip);
}

static int rna_Strip_frame_duration_get(PointerRNA *ptr)
{
  Strip *strip = static_cast<Strip *>(ptr->data);
  Scene *scene = reinterpret_cast<Scene *>(ptr->owner_id);
  return blender::seq::time_strip_length_get(scene, strip);
}

static int rna_Strip_frame_editable(const PointerRNA *ptr, const char ** /*r_info*/)
{
  Strip *strip = (Strip *)ptr->data;
  /* Effect strips' start frame and length must be readonly! */
  return (blender::seq::effect_get_num_inputs(strip->type)) ? PropertyFlag(0) : PROP_EDITABLE;
}

static void rna_Strip_channel_set(PointerRNA *ptr, int value)
{
  Strip *strip = (Strip *)ptr->data;
  Scene *scene = (Scene *)ptr->owner_id;
  ListBase *seqbase = blender::seq::get_seqbase_by_strip(scene, strip);

  /* check channel increment or decrement */
  const int channel_delta = (value >= strip->channel) ? 1 : -1;
  blender::seq::strip_channel_set(strip, value);

  if (blender::seq::transform_test_overlap(scene, seqbase, strip)) {
    blender::seq::transform_seqbase_shuffle_ex(seqbase, strip, scene, channel_delta);
  }
  blender::seq::relations_invalidate_cache(scene, strip);
}

static bool rna_Strip_lock_get(PointerRNA *ptr)
{
  Scene *scene = reinterpret_cast<Scene *>(ptr->owner_id);
  Strip *strip = static_cast<Strip *>(ptr->data);
  Editing *ed = blender::seq::editing_get(scene);
  ListBase *channels = blender::seq::get_channels_by_strip(ed, strip);
  return blender::seq::transform_is_locked(channels, strip);
}

static void rna_Strip_use_proxy_set(PointerRNA *ptr, bool value)
{
  Strip *strip = (Strip *)ptr->data;
  blender::seq::proxy_set(strip, value != 0);
}

static PointerRNA rna_Strip_active_modifier_get(PointerRNA *ptr)
{
  const Strip *strip = ptr->data_as<Strip>();
  StripModifierData *smd = blender::seq::modifier_get_active(strip);
  return RNA_pointer_create_with_parent(*ptr, &RNA_StripModifier, smd);
}

static void rna_Strip_active_modifier_set(PointerRNA *ptr, PointerRNA value, ReportList *reports)
{
  Strip *strip = ptr->data_as<Strip>();
  StripModifierData *smd = value.data_as<StripModifierData>();

  WM_main_add_notifier(NC_SCENE | ND_SEQUENCER, ptr->owner_id);

  if (RNA_pointer_is_null(&value)) {
    blender::seq::modifier_set_active(strip, nullptr);
    return;
  }

  if (BLI_findindex(&strip->modifiers, smd) == -1) {
    BKE_reportf(
        reports, RPT_ERROR, "Modifier \"%s\" is not in the strip's modifier list", smd->name);
    return;
  }

  blender::seq::modifier_set_active(strip, smd);
}

static bool transform_strip_cmp_fn(Strip *strip, void *arg_pt)
{
  StripSearchData *data = static_cast<StripSearchData *>(arg_pt);

  if (strip->data && strip->data->transform == data->data) {
    data->strip = strip;
    return false; /* done so bail out */
  }
  return true;
}

static Strip *strip_get_by_transform(Editing *ed, StripTransform *transform)
{
  StripSearchData data;

  data.strip = nullptr;
  data.data = transform;

  /* irritating we need to search for our strip! */
  blender::seq::foreach_strip(&ed->seqbase, transform_strip_cmp_fn, &data);

  return data.strip;
}

static std::optional<std::string> rna_StripTransform_path(const PointerRNA *ptr)
{
  Scene *scene = (Scene *)ptr->owner_id;
  Editing *ed = blender::seq::editing_get(scene);
  Strip *strip = strip_get_by_transform(ed, static_cast<StripTransform *>(ptr->data));

  if (strip) {
    char name_esc[(sizeof(strip->name) - 2) * 2];
    BLI_str_escape(name_esc, strip->name + 2, sizeof(name_esc));
    return fmt::format("sequence_editor.strips_all[\"{}\"].transform", name_esc);
  }
  return "";
}

static void rna_StripTransform_update(Main * /*bmain*/, Scene * /*scene*/, PointerRNA *ptr)
{
  Scene *scene = (Scene *)ptr->owner_id;
  Editing *ed = blender::seq::editing_get(scene);
  Strip *strip = strip_get_by_transform(ed, static_cast<StripTransform *>(ptr->data));

  blender::seq::relations_invalidate_cache(scene, strip);
}

static bool crop_strip_cmp_fn(Strip *strip, void *arg_pt)
{
  StripSearchData *data = static_cast<StripSearchData *>(arg_pt);

  if (strip->data && strip->data->crop == data->data) {
    data->strip = strip;
    return false; /* done so bail out */
  }
  return true;
}

static Strip *strip_get_by_crop(Editing *ed, StripCrop *crop)
{
  StripSearchData data;

  data.strip = nullptr;
  data.data = crop;

  /* irritating we need to search for our strip! */
  blender::seq::foreach_strip(&ed->seqbase, crop_strip_cmp_fn, &data);

  return data.strip;
}

static std::optional<std::string> rna_StripCrop_path(const PointerRNA *ptr)
{
  Scene *scene = (Scene *)ptr->owner_id;
  Editing *ed = blender::seq::editing_get(scene);
  Strip *strip = strip_get_by_crop(ed, static_cast<StripCrop *>(ptr->data));

  if (strip) {
    char name_esc[(sizeof(strip->name) - 2) * 2];
    BLI_str_escape(name_esc, strip->name + 2, sizeof(name_esc));
    return fmt::format("sequence_editor.strips_all[\"{}\"].crop", name_esc);
  }
  return "";
}

static void rna_StripCrop_update(Main * /*bmain*/, Scene * /*scene*/, PointerRNA *ptr)
{
  Scene *scene = (Scene *)ptr->owner_id;
  Editing *ed = blender::seq::editing_get(scene);
  Strip *strip = strip_get_by_crop(ed, static_cast<StripCrop *>(ptr->data));

  blender::seq::relations_invalidate_cache(scene, strip);
}

static void rna_Strip_text_font_set(PointerRNA *ptr,
                                    PointerRNA ptr_value,
                                    ReportList * /*reports*/)
{
  Strip *strip = static_cast<Strip *>(ptr->data);
  VFont *value = static_cast<VFont *>(ptr_value.data);
  blender::seq::effect_text_font_set(strip, value);
}

/* name functions that ignore the first two characters */
static void rna_Strip_name_get(PointerRNA *ptr, char *value)
{
  Strip *strip = (Strip *)ptr->data;
  strcpy(value, strip->name + 2);
}

static int rna_Strip_name_length(PointerRNA *ptr)
{
  Strip *strip = (Strip *)ptr->data;
  return strlen(strip->name + 2);
}

static void rna_Strip_name_set(PointerRNA *ptr, const char *value)
{
  Scene *scene = (Scene *)ptr->owner_id;
  Strip *strip = (Strip *)ptr->data;
  char oldname[sizeof(strip->name)];
  AnimData *adt;

  blender::seq::prefetch_stop(scene);

  /* make a copy of the old name first */
  BLI_strncpy(oldname, strip->name + 2, sizeof(strip->name) - 2);

  /* copy the new name into the name slot */
  blender::seq::edit_strip_name_set(scene, strip, value);

  /* make sure the name is unique */
  blender::seq::strip_unique_name_set(scene, &scene->ed->seqbase, strip);
  /* fix all the animation data which may link to this */

  /* Don't rename everywhere because these are per scene. */
#  if 0
  BKE_animdata_fix_paths_rename_all(
      nullptr, "sequence_editor.strips_all", oldname, strip->name + 2);
#  endif
  adt = BKE_animdata_from_id(&scene->id);
  if (adt) {
    BKE_animdata_fix_paths_rename(
        &scene->id, adt, nullptr, "sequence_editor.strips_all", oldname, strip->name + 2, 0, 0, 1);
  }
}

static int rna_Strip_text_length(PointerRNA *ptr)
{
  Strip *strip = static_cast<Strip *>(ptr->data);
  TextVars *text = static_cast<TextVars *>(strip->effectdata);
  return text->text_len_bytes;
}

static void rna_Strip_text_get(PointerRNA *ptr, char *value)
{
  Strip *strip = static_cast<Strip *>(ptr->data);
  TextVars *text = static_cast<TextVars *>(strip->effectdata);
  memcpy(value, text->text_ptr, text->text_len_bytes + 1);
}

static void rna_Strip_text_set(PointerRNA *ptr, const char *value)
{
  Strip *strip = static_cast<Strip *>(ptr->data);
  TextVars *text = static_cast<TextVars *>(strip->effectdata);

  if (text->text_ptr) {
    MEM_freeN(text->text_ptr);
  }
  text->text_ptr = BLI_strdup(value);
  text->text_len_bytes = strlen(text->text_ptr);
}

static StructRNA *rna_Strip_refine(PointerRNA *ptr)
{
  Strip *strip = (Strip *)ptr->data;

  switch (strip->type) {
    case STRIP_TYPE_IMAGE:
      return &RNA_ImageStrip;
    case STRIP_TYPE_META:
      return &RNA_MetaStrip;
    case STRIP_TYPE_SCENE:
      return &RNA_SceneStrip;
    case STRIP_TYPE_MOVIE:
      return &RNA_MovieStrip;
    case STRIP_TYPE_MOVIECLIP:
      return &RNA_MovieClipStrip;
    case STRIP_TYPE_MASK:
      return &RNA_MaskStrip;
    case STRIP_TYPE_SOUND_RAM:
      return &RNA_SoundStrip;
    case STRIP_TYPE_CROSS:
      return &RNA_CrossStrip;
    case STRIP_TYPE_ADD:
      return &RNA_AddStrip;
    case STRIP_TYPE_SUB:
      return &RNA_SubtractStrip;
    case STRIP_TYPE_ALPHAOVER:
      return &RNA_AlphaOverStrip;
    case STRIP_TYPE_ALPHAUNDER:
      return &RNA_AlphaUnderStrip;
    case STRIP_TYPE_GAMCROSS:
      return &RNA_GammaCrossStrip;
    case STRIP_TYPE_MUL:
      return &RNA_MultiplyStrip;
    case STRIP_TYPE_MULTICAM:
      return &RNA_MulticamStrip;
    case STRIP_TYPE_ADJUSTMENT:
      return &RNA_AdjustmentStrip;
    case STRIP_TYPE_WIPE:
      return &RNA_WipeStrip;
    case STRIP_TYPE_GLOW:
      return &RNA_GlowStrip;
    case STRIP_TYPE_COLOR:
      return &RNA_ColorStrip;
    case STRIP_TYPE_SPEED:
      return &RNA_SpeedControlStrip;
    case STRIP_TYPE_GAUSSIAN_BLUR:
      return &RNA_GaussianBlurStrip;
    case STRIP_TYPE_TEXT:
      return &RNA_TextStrip;
    case STRIP_TYPE_COLORMIX:
      return &RNA_ColorMixStrip;
    default:
      return &RNA_Strip;
  }
}

static std::optional<std::string> rna_Strip_path(const PointerRNA *ptr)
{
  const Strip *strip = (Strip *)ptr->data;

  /* sequencer data comes from scene...
   * TODO: would be nice to make SequenceEditor data a data-block of its own (for shorter paths)
   */
  char name_esc[(sizeof(strip->name) - 2) * 2];

  BLI_str_escape(name_esc, strip->name + 2, sizeof(name_esc));
  return fmt::format("sequence_editor.strips_all[\"{}\"]", name_esc);
}

static IDProperty **rna_Strip_idprops(PointerRNA *ptr)
{
  Strip *strip = static_cast<Strip *>(ptr->data);
  return &strip->prop;
}

static IDProperty **rna_Strip_system_idprops(PointerRNA *ptr)
{
  Strip *strip = static_cast<Strip *>(ptr->data);
  return &strip->system_properties;
}

static bool rna_MovieStrip_reload_if_needed(ID *scene_id, Strip *strip, Main *bmain)
{
  Scene *scene = (Scene *)scene_id;

  bool has_reloaded;
  bool can_produce_frames;

  blender::seq::add_movie_reload_if_needed(
      bmain, scene, strip, &has_reloaded, &can_produce_frames);

  if (has_reloaded && can_produce_frames) {
    blender::seq::relations_invalidate_cache_raw(scene, strip);

    DEG_id_tag_update(&scene->id, ID_RECALC_SEQUENCER_STRIPS);
    WM_main_add_notifier(NC_SCENE | ND_SEQUENCER, scene);
  }

  return can_produce_frames;
}

static PointerRNA rna_MovieStrip_metadata_get(ID *scene_id, Strip *strip)
{
  if (strip == nullptr || strip->anims.first == nullptr) {
    return PointerRNA_NULL;
  }

  StripAnim *sanim = static_cast<StripAnim *>(strip->anims.first);
  if (sanim->anim == nullptr) {
    return PointerRNA_NULL;
  }

  IDProperty *metadata = MOV_load_metadata(sanim->anim);
  if (metadata == nullptr) {
    return PointerRNA_NULL;
  }

  PointerRNA ptr = RNA_pointer_create_discrete(scene_id, &RNA_IDPropertyWrapPtr, metadata);
  return ptr;
}

static PointerRNA rna_SequenceEditor_meta_stack_get(CollectionPropertyIterator *iter)
{
  ListBaseIterator *internal = &iter->internal.listbase;
  MetaStack *ms = (MetaStack *)internal->link;

  return RNA_pointer_create_with_parent(iter->parent, &RNA_Strip, ms->parent_strip);
}

/* TODO: expose strip path setting as a higher level sequencer BKE function. */
static void rna_Strip_filepath_set(PointerRNA *ptr, const char *value)
{
  Strip *strip = (Strip *)(ptr->data);
  BLI_path_split_dir_file(value,
                          strip->data->dirpath,
                          sizeof(strip->data->dirpath),
                          strip->data->stripdata->filename,
                          sizeof(strip->data->stripdata->filename));
}

static void rna_Strip_filepath_get(PointerRNA *ptr, char *value)
{
  Strip *strip = (Strip *)(ptr->data);
  char filepath[FILE_MAX];

  BLI_path_join(
      filepath, sizeof(filepath), strip->data->dirpath, strip->data->stripdata->filename);
  strcpy(value, filepath);
}

static int rna_Strip_filepath_length(PointerRNA *ptr)
{
  Strip *strip = (Strip *)(ptr->data);
  char filepath[FILE_MAX];

  BLI_path_join(
      filepath, sizeof(filepath), strip->data->dirpath, strip->data->stripdata->filename);
  return strlen(filepath);
}

static void rna_Strip_proxy_filepath_set(PointerRNA *ptr, const char *value)
{
  StripProxy *proxy = (StripProxy *)(ptr->data);
  BLI_path_split_dir_file(
      value, proxy->dirpath, sizeof(proxy->dirpath), proxy->filename, sizeof(proxy->filename));
  if (proxy->anim) {
    MOV_close(proxy->anim);
    proxy->anim = nullptr;
  }
}

static void rna_Strip_proxy_filepath_get(PointerRNA *ptr, char *value)
{
  StripProxy *proxy = (StripProxy *)(ptr->data);
  char filepath[FILE_MAX];

  BLI_path_join(filepath, sizeof(filepath), proxy->dirpath, proxy->filename);
  strcpy(value, filepath);
}

static int rna_Strip_proxy_filepath_length(PointerRNA *ptr)
{
  StripProxy *proxy = (StripProxy *)(ptr->data);
  char filepath[FILE_MAX];

  BLI_path_join(filepath, sizeof(filepath), proxy->dirpath, proxy->filename);
  return strlen(filepath);
}

static void rna_Strip_audio_update(Main * /*bmain*/, Scene * /*scene*/, PointerRNA *ptr)
{
  DEG_id_tag_update(ptr->owner_id, ID_RECALC_SEQUENCER_STRIPS | ID_RECALC_AUDIO);
}

static void rna_Strip_pan_range(
    PointerRNA *ptr, float *min, float *max, float *softmin, float *softmax)
{
  Scene *scene = (Scene *)ptr->owner_id;

  *min = -FLT_MAX;
  *max = FLT_MAX;
  *softmax = 1 + int(scene->r.ffcodecdata.audio_channels > 2);
  *softmin = -*softmax;
}

static int rna_Strip_input_count_get(PointerRNA *ptr)
{
  Strip *strip = (Strip *)(ptr->data);

  return blender::seq::effect_get_num_inputs(strip->type);
}

static void rna_Strip_input_set(PointerRNA *ptr,
                                const PointerRNA &ptr_value,
                                ReportList *reports,
                                int input_num)
{

  Strip *strip = static_cast<Strip *>(ptr->data);
  Strip *input = static_cast<Strip *>(ptr_value.data);

  if (blender::seq::relations_render_loop_check(input, strip)) {
    BKE_report(reports, RPT_ERROR, "Cannot reassign inputs: recursion detected");
    return;
  }

  switch (input_num) {
    case 1:
      strip->input1 = input;
      break;
    case 2:
      strip->input2 = input;
      break;
  }
}

static void rna_Strip_input_1_set(PointerRNA *ptr, PointerRNA ptr_value, ReportList *reports)
{
  rna_Strip_input_set(ptr, ptr_value, reports, 1);
}

static void rna_Strip_input_2_set(PointerRNA *ptr, PointerRNA ptr_value, ReportList *reports)
{
  rna_Strip_input_set(ptr, ptr_value, reports, 2);
}
#  if 0
static void rna_SoundStrip_filename_set(PointerRNA *ptr, const char *value)
{
  Strip *strip = (Strip *)(ptr->data);
  BLI_path_split_dir_file(value,
                          strip->data->dirpath,
                          sizeof(strip->data->dirpath),
                          strip->data->stripdata->name,
                          sizeof(strip->data->stripdata->name));
}

static void rna_StripElement_filename_set(PointerRNA *ptr, const char *value)
{
  StripElem *elem = (StripElem *)(ptr->data);
  BLI_path_split_file_part(value, elem->name, sizeof(elem->name));
}
#  endif

static void rna_Strip_reopen_files_update(Main *bmain, Scene * /*scene*/, PointerRNA *ptr)
{
  Scene *scene = (Scene *)ptr->owner_id;
  Editing *ed = blender::seq::editing_get(scene);

  blender::seq::relations_free_imbuf(scene, &ed->seqbase, false);
  rna_Strip_invalidate_raw_update(bmain, scene, ptr);

  if (RNA_struct_is_a(ptr->type, &RNA_SoundStrip)) {
    blender::seq::sound_update_bounds(scene, static_cast<Strip *>(ptr->data));
  }
}

static void rna_Strip_filepath_update(Main *bmain, Scene * /*scene*/, PointerRNA *ptr)
{
  Scene *scene = (Scene *)ptr->owner_id;
  Strip *strip = (Strip *)(ptr->data);
  blender::seq::add_reload_new_file(bmain, scene, strip, true);
  rna_Strip_invalidate_raw_update(bmain, scene, ptr);
}

static void rna_Strip_sound_update(Main *bmain, Scene * /*active_scene*/, PointerRNA *ptr)
{
  Scene *scene = (Scene *)ptr->owner_id;
  DEG_id_tag_update(&scene->id, ID_RECALC_SEQUENCER_STRIPS | ID_RECALC_AUDIO);
  DEG_relations_tag_update(bmain);
}

static bool seqproxy_strip_cmp_fn(Strip *strip, void *arg_pt)
{
  StripSearchData *data = static_cast<StripSearchData *>(arg_pt);

  if (strip->data && strip->data->proxy == data->data) {
    data->strip = strip;
    return false; /* done so bail out */
  }
  return true;
}

static Strip *strip_get_by_proxy(Editing *ed, StripProxy *proxy)
{
  StripSearchData data;

  data.strip = nullptr;
  data.data = proxy;

  blender::seq::foreach_strip(&ed->seqbase, seqproxy_strip_cmp_fn, &data);
  return data.strip;
}

static void rna_Strip_tcindex_update(Main *bmain, Scene * /*scene*/, PointerRNA *ptr)
{
  Scene *scene = (Scene *)ptr->owner_id;
  Editing *ed = blender::seq::editing_get(scene);
  Strip *strip = strip_get_by_proxy(ed, static_cast<StripProxy *>(ptr->data));

  blender::seq::add_reload_new_file(bmain, scene, strip, false);
  do_strip_frame_change_update(scene, strip);
}

static void rna_StripProxy_update(Main * /*bmain*/, Scene * /*scene*/, PointerRNA *ptr)
{
  Scene *scene = (Scene *)ptr->owner_id;
  Editing *ed = blender::seq::editing_get(scene);
  Strip *strip = strip_get_by_proxy(ed, static_cast<StripProxy *>(ptr->data));
  blender::seq::relations_invalidate_cache(scene, strip);
}

/* do_versions? */
static float rna_Strip_opacity_get(PointerRNA *ptr)
{
  Strip *strip = (Strip *)(ptr->data);
  return strip->blend_opacity / 100.0f;
}
static void rna_Strip_opacity_set(PointerRNA *ptr, float value)
{
  Strip *strip = (Strip *)(ptr->data);
  CLAMP(value, 0.0f, 1.0f);
  strip->blend_opacity = value * 100.0f;
}

static int rna_Strip_color_tag_get(PointerRNA *ptr)
{
  Strip *strip = (Strip *)(ptr->data);
  return strip->color_tag;
}

static void rna_Strip_color_tag_set(PointerRNA *ptr, int value)
{
  Strip *strip = (Strip *)(ptr->data);
  strip->color_tag = value;
}

static bool colbalance_seq_cmp_fn(Strip *strip, void *arg_pt)
{
  StripSearchData *data = static_cast<StripSearchData *>(arg_pt);

  for (StripModifierData *smd = static_cast<StripModifierData *>(strip->modifiers.first); smd;
       smd = smd->next)
  {
    if (smd->type == eSeqModifierType_ColorBalance) {
      ColorBalanceModifierData *cbmd = (ColorBalanceModifierData *)smd;

      if (&cbmd->color_balance == data->data) {
        data->strip = strip;
        data->smd = smd;
        return false; /* done so bail out */
      }
    }
  }

  return true;
}

static Strip *strip_get_by_colorbalance(Editing *ed,
                                        StripColorBalance *cb,
                                        StripModifierData **r_smd)
{
  StripSearchData data;

  data.strip = nullptr;
  data.smd = nullptr;
  data.data = cb;

  /* irritating we need to search for our strip! */
  blender::seq::foreach_strip(&ed->seqbase, colbalance_seq_cmp_fn, &data);

  *r_smd = data.smd;

  return data.strip;
}

static std::optional<std::string> rna_StripColorBalance_path(const PointerRNA *ptr)
{
  Scene *scene = (Scene *)ptr->owner_id;
  StripModifierData *smd;
  Editing *ed = blender::seq::editing_get(scene);
  Strip *strip = strip_get_by_colorbalance(ed, static_cast<StripColorBalance *>(ptr->data), &smd);

  if (strip) {
    char name_esc[(sizeof(strip->name) - 2) * 2];

    BLI_str_escape(name_esc, strip->name + 2, sizeof(name_esc));

    if (!smd) {
      /* Path to old filter color balance. */
      return fmt::format("sequence_editor.strips_all[\"{}\"].color_balance", name_esc);
    }
    /* Path to modifier. */
    char name_esc_smd[sizeof(smd->name) * 2];

    BLI_str_escape(name_esc_smd, smd->name, sizeof(name_esc_smd));
    return fmt::format("sequence_editor.strips_all[\"{}\"].modifiers[\"{}\"].color_balance",
                       name_esc,
                       name_esc_smd);
  }
  return "";
}

static void rna_StripColorBalance_update(Main * /*bmain*/, Scene * /*scene*/, PointerRNA *ptr)
{
  Scene *scene = (Scene *)ptr->owner_id;
  Editing *ed = blender::seq::editing_get(scene);
  StripModifierData *smd;
  Strip *strip = strip_get_by_colorbalance(ed, static_cast<StripColorBalance *>(ptr->data), &smd);

  blender::seq::relations_invalidate_cache(scene, strip);
}

static void rna_SequenceEditor_overlay_lock_set(PointerRNA *ptr, bool value)
{
  Scene *scene = (Scene *)ptr->owner_id;
  Editing *ed = blender::seq::editing_get(scene);

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
  Editing *ed = blender::seq::editing_get(scene);

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
  Editing *ed = blender::seq::editing_get(scene);

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

static int rna_SequenceEditor_get_cache_raw_size(PointerRNA *ptr)
{
  Scene *scene = (Scene *)ptr->owner_id;
  if (scene == nullptr) {
    return 0;
  }
  return int(blender::seq::source_image_cache_calc_memory_size(scene) / 1024 / 1024);
}

static int rna_SequenceEditor_get_cache_final_size(PointerRNA *ptr)
{
  Scene *scene = (Scene *)ptr->owner_id;
  if (scene == nullptr) {
    return 0;
  }
  return int(blender::seq::final_image_cache_calc_memory_size(scene) / 1024 / 1024);
}

static void rna_SequenceEditor_display_stack(ID *id,
                                             Editing *ed,
                                             ReportList *reports,
                                             Strip *strip_meta)
{
  /* Check for non-meta sequence */
  if (strip_meta != nullptr && strip_meta->type != STRIP_TYPE_META &&
      blender::seq::exists_in_seqbase(strip_meta, &ed->seqbase))
  {
    BKE_report(reports, RPT_ERROR, "Strip type must be 'META'");
    return;
  }

  /* Get editing base of meta sequence */
  Scene *scene = (Scene *)id;
  blender::seq::meta_stack_set(scene, strip_meta);
  /* De-activate strip. This is to prevent strip from different timeline being drawn. */
  blender::seq::select_active_set(scene, nullptr);

  WM_main_add_notifier(NC_SCENE | ND_SEQUENCER, scene);
}

static bool modifier_strip_cmp_fn(Strip *strip, void *arg_pt)
{
  StripSearchData *data = static_cast<StripSearchData *>(arg_pt);

  if (BLI_findindex(&strip->modifiers, data->data) != -1) {
    data->strip = strip;
    return false; /* done so bail out */
  }

  return true;
}

static Strip *strip_get_by_modifier(Editing *ed, StripModifierData *smd)
{
  StripSearchData data;

  data.strip = nullptr;
  data.data = smd;

  /* irritating we need to search for our strip! */
  blender::seq::foreach_strip(&ed->seqbase, modifier_strip_cmp_fn, &data);

  return data.strip;
}

static StructRNA *rna_StripModifier_refine(PointerRNA *ptr)
{
  StripModifierData *smd = (StripModifierData *)ptr->data;

  switch (smd->type) {
    case eSeqModifierType_ColorBalance:
      return &RNA_ColorBalanceModifier;
    case eSeqModifierType_Curves:
      return &RNA_CurvesModifier;
    case eSeqModifierType_HueCorrect:
      return &RNA_HueCorrectModifier;
    case eSeqModifierType_Mask:
      return &RNA_MaskStripModifier;
    case eSeqModifierType_BrightContrast:
      return &RNA_BrightContrastModifier;
    case eSeqModifierType_WhiteBalance:
      return &RNA_WhiteBalanceModifier;
    case eSeqModifierType_Tonemap:
      return &RNA_SequencerTonemapModifierData;
    case eSeqModifierType_SoundEqualizer:
      return &RNA_SoundEqualizerModifier;
    case eSeqModifierType_Compositor:
      return &RNA_SequencerCompositorModifierData;
    default:
      return &RNA_StripModifier;
  }
}

static std::optional<std::string> rna_StripModifier_path(const PointerRNA *ptr)
{
  Scene *scene = (Scene *)ptr->owner_id;
  Editing *ed = blender::seq::editing_get(scene);
  StripModifierData *smd = static_cast<StripModifierData *>(ptr->data);
  Strip *strip = strip_get_by_modifier(ed, smd);

  if (strip) {
    char name_esc[(sizeof(strip->name) - 2) * 2];
    char name_esc_smd[sizeof(smd->name) * 2];

    BLI_str_escape(name_esc, strip->name + 2, sizeof(name_esc));
    BLI_str_escape(name_esc_smd, smd->name, sizeof(name_esc_smd));
    return fmt::format(
        "sequence_editor.strips_all[\"{}\"].modifiers[\"{}\"]", name_esc, name_esc_smd);
  }
  return "";
}

static void rna_StripModifier_name_set(PointerRNA *ptr, const char *value)
{
  StripModifierData *smd = static_cast<StripModifierData *>(ptr->data);
  Scene *scene = (Scene *)ptr->owner_id;
  Editing *ed = blender::seq::editing_get(scene);
  Strip *strip = strip_get_by_modifier(ed, smd);
  AnimData *adt;
  char oldname[sizeof(smd->name)];

  /* make a copy of the old name first */
  STRNCPY(oldname, smd->name);

  /* copy the new name into the name slot */
  STRNCPY_UTF8(smd->name, value);

  /* make sure the name is truly unique */
  blender::seq::modifier_unique_name(strip, smd);

  /* fix all the animation data which may link to this */
  adt = BKE_animdata_from_id(&scene->id);
  if (adt) {
    char rna_path_prefix[1024];

    char strip_name_esc[(sizeof(strip->name) - 2) * 2];
    BLI_str_escape(strip_name_esc, strip->name + 2, sizeof(strip_name_esc));

    SNPRINTF(rna_path_prefix, "sequence_editor.strips_all[\"%s\"].modifiers", strip_name_esc);
    BKE_animdata_fix_paths_rename(
        &scene->id, adt, nullptr, rna_path_prefix, oldname, smd->name, 0, 0, 1);
  }
}

static void rna_StripModifier_is_active_set(PointerRNA *ptr, bool value)
{
  StripModifierData *smd = ptr->data_as<StripModifierData>();

  if (value) {
    /* Disable the active flag of all other modifiers. */
    for (StripModifierData *prev_smd = smd->prev; prev_smd != nullptr; prev_smd = prev_smd->prev) {
      prev_smd->flag &= ~STRIP_MODIFIER_FLAG_ACTIVE;
    }
    for (StripModifierData *next_smd = smd->next; next_smd != nullptr; next_smd = next_smd->next) {
      next_smd->flag &= ~STRIP_MODIFIER_FLAG_ACTIVE;
    }

    smd->flag |= STRIP_MODIFIER_FLAG_ACTIVE;
    WM_main_add_notifier(NC_SCENE | ND_SEQUENCER, ptr->owner_id);
  }
}

static void rna_StripModifier_update(Main *bmain, Scene * /*scene*/, PointerRNA *ptr)
{
  /* strip from other scenes could be modified, so using active scene is not reliable */
  Scene *scene = (Scene *)ptr->owner_id;
  Editing *ed = blender::seq::editing_get(scene);
  Strip *strip = strip_get_by_modifier(ed, static_cast<StripModifierData *>(ptr->data));

  if (ELEM(strip->type, STRIP_TYPE_SOUND_RAM, STRIP_TYPE_SOUND_HD)) {
    DEG_id_tag_update(&scene->id, ID_RECALC_SEQUENCER_STRIPS | ID_RECALC_AUDIO);
    DEG_relations_tag_update(bmain);
  }
  else {
    blender::seq::relations_invalidate_cache(scene, strip);
  }
}

/*
 * Update of Curve in an EQ Sound Modifier
 */
static void rna_StripModifier_EQCurveMapping_update(Main *bmain,
                                                    Scene * /*scene*/,
                                                    PointerRNA *ptr)
{
  Scene *scene = (Scene *)ptr->owner_id;

  DEG_id_tag_update(&scene->id, ID_RECALC_SEQUENCER_STRIPS | ID_RECALC_AUDIO);
  DEG_relations_tag_update(bmain);
  WM_main_add_notifier(NC_SCENE | ND_SEQUENCER, NULL);
}

static bool rna_StripModifier_otherStrip_poll(PointerRNA *ptr, PointerRNA value)
{
  Scene *scene = (Scene *)ptr->owner_id;
  Editing *ed = blender::seq::editing_get(scene);
  Strip *strip = strip_get_by_modifier(ed, static_cast<StripModifierData *>(ptr->data));
  Strip *cur = (Strip *)value.data;

  if ((strip == cur) || (cur->type == STRIP_TYPE_SOUND_RAM)) {
    return false;
  }

  return true;
}

static StripModifierData *rna_Strip_modifier_new(
    Strip *strip, bContext *C, ReportList *reports, const char *name, int type)
{
  if (!blender::seq::sequence_supports_modifiers(strip)) {
    BKE_report(reports, RPT_ERROR, "Strip type does not support modifiers");

    return nullptr;
  }
  else {
    Scene *scene = CTX_data_sequencer_scene(C);
    StripModifierData *smd;

    smd = blender::seq::modifier_new(strip, name, type);
    blender::seq::modifier_persistent_uid_init(*strip, *smd);

    blender::seq::relations_invalidate_cache(scene, strip);

    WM_main_add_notifier(NC_SCENE | ND_SEQUENCER, nullptr);

    return smd;
  }
}

static void rna_Strip_modifier_remove(Strip *strip,
                                      bContext *C,
                                      ReportList *reports,
                                      PointerRNA *smd_ptr)
{
  StripModifierData *smd = static_cast<StripModifierData *>(smd_ptr->data);
  Scene *scene = CTX_data_sequencer_scene(C);

  if (blender::seq::modifier_remove(strip, smd) == false) {
    BKE_report(reports, RPT_ERROR, "Modifier was not found in the stack");
    return;
  }

  smd_ptr->invalidate();
  blender::seq::relations_invalidate_cache(scene, strip);

  WM_main_add_notifier(NC_SCENE | ND_SEQUENCER, nullptr);
}

static void rna_Strip_modifier_clear(Strip *strip, bContext *C)
{
  Scene *scene = CTX_data_sequencer_scene(C);

  blender::seq::modifier_clear(strip);

  blender::seq::relations_invalidate_cache(scene, strip);

  WM_main_add_notifier(NC_SCENE | ND_SEQUENCER, nullptr);
}

static void rna_StripModifier_strip_set(PointerRNA *ptr, PointerRNA value, ReportList *reports)
{
  StripModifierData *smd = static_cast<StripModifierData *>(ptr->data);
  Scene *scene = (Scene *)ptr->owner_id;
  Editing *ed = blender::seq::editing_get(scene);
  Strip *strip = strip_get_by_modifier(ed, smd);
  Strip *target = (Strip *)value.data;

  if (target != nullptr && blender::seq::relations_render_loop_check(target, strip)) {
    BKE_report(reports, RPT_ERROR, "Recursion detected, cannot use this strip");
    return;
  }

  smd->mask_strip = target;
}

static float rna_Strip_fps_get(PointerRNA *ptr)
{
  Scene *scene = (Scene *)ptr->owner_id;
  Strip *strip = (Strip *)(ptr->data);
  return blender::seq::time_strip_fps_get(scene, strip);
}

static void rna_Strip_separate(ID *id, Strip *strip_meta, Main *bmain)
{
  Scene *scene = (Scene *)id;

  /* Find the appropriate seqbase */
  ListBase *seqbase = blender::seq::get_seqbase_by_strip(scene, strip_meta);

  LISTBASE_FOREACH_MUTABLE (Strip *, strip, &strip_meta->seqbase) {
    blender::seq::edit_move_strip_to_seqbase(scene, &strip_meta->seqbase, strip, seqbase);
  }

  blender::seq::edit_flag_for_removal(scene, seqbase, strip_meta);
  blender::seq::edit_remove_flagged_strips(scene, seqbase);

  /* Update depsgraph. */
  DEG_relations_tag_update(bmain);
  DEG_id_tag_update(&scene->id, ID_RECALC_SEQUENCER_STRIPS);

  WM_main_add_notifier(NC_SCENE | ND_SEQUENCER, scene);
}

static void rna_SequenceTimelineChannel_name_set(PointerRNA *ptr, const char *value)
{
  SeqTimelineChannel *channel = (SeqTimelineChannel *)ptr->data;
  Scene *scene = (Scene *)ptr->owner_id;
  Editing *ed = blender::seq::editing_get(scene);

  Strip *channel_owner = blender::seq::lookup_strip_by_channel_owner(ed, channel);
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
  Editing *ed = blender::seq::editing_get(scene);
  SeqTimelineChannel *channel = (SeqTimelineChannel *)ptr;

  Strip *channel_owner = blender::seq::lookup_strip_by_channel_owner(ed, channel);
  ListBase *seqbase;
  if (channel_owner == nullptr) {
    seqbase = &ed->seqbase;
  }
  else {
    seqbase = &channel_owner->seqbase;
  }

  LISTBASE_FOREACH (Strip *, strip, seqbase) {
    blender::seq::relations_invalidate_cache(scene, strip);
  }

  rna_Strip_sound_update(bmain, active_scene, ptr);
}

static std::optional<std::string> rna_SeqTimelineChannel_path(const PointerRNA *ptr)
{
  Scene *scene = (Scene *)ptr->owner_id;
  SeqTimelineChannel *channel = (SeqTimelineChannel *)ptr->data;

  Strip *channel_owner = blender::seq::lookup_strip_by_channel_owner(scene->ed, channel);

  char channel_name_esc[(sizeof(channel->name)) * 2];
  BLI_str_escape(channel_name_esc, channel->name, sizeof(channel_name_esc));

  if (channel_owner == nullptr) {
    return fmt::format("sequence_editor.channels[\"{}\"]", channel_name_esc);
  }
  char owner_name_esc[(sizeof(channel_owner->name) - 2) * 2];
  BLI_str_escape(owner_name_esc, channel_owner->name + 2, sizeof(owner_name_esc));
  return fmt::format(
      "sequence_editor.strips_all[\"{}\"].channels[\"{}\"]", owner_name_esc, channel_name_esc);
}

static EQCurveMappingData *rna_Strip_SoundEqualizer_Curve_add(SoundEqualizerModifierData *semd,
                                                              bContext * /*C*/,
                                                              float min_freq,
                                                              float max_freq)
{
  EQCurveMappingData *eqcmd = blender::seq::sound_equalizermodifier_add_graph(
      semd, min_freq, max_freq);
  WM_main_add_notifier(NC_SCENE | ND_SEQUENCER, NULL);
  return eqcmd;
}

static void rna_Strip_SoundEqualizer_Curve_clear(SoundEqualizerModifierData *semd,
                                                 bContext * /*C*/)
{
  blender::seq::sound_equalizermodifier_free((StripModifierData *)semd);
  WM_main_add_notifier(NC_SCENE | ND_SEQUENCER, NULL);
}

static bool rna_CompositorModifier_node_group_poll(PointerRNA * /*ptr*/, PointerRNA value)
{
  const bNodeTree *node_tree = value.data_as<bNodeTree>();
  if (node_tree->type != NTREE_COMPOSIT) {
    return false;
  }
  return true;
}

static void rna_CompositorModifier_node_group_update(Main *bmain, Scene *scene, PointerRNA *ptr)
{
  rna_StripModifier_update(bmain, scene, ptr);

  /* Tag depsgraph relations for an update since the modifier could now be referencing a different
   * node tree. */
  DEG_relations_tag_update(bmain);

  /* Strips from other scenes could be modified, so use the scene of the strip as opposed to the
   * active scene argument. */
  Scene *strip_scene = reinterpret_cast<Scene *>(ptr->owner_id);
  Editing *ed = blender::seq::editing_get(strip_scene);

  /* The sequencer stores a cached mapping between compositor node trees and strips that use them
   * as a modifier, so we need to invalidate the cache since the node tree changed. */
  blender::seq::strip_lookup_invalidate(ed);
}

#else

static void rna_def_strip_element(BlenderRNA *brna)
{
  StructRNA *srna;
  PropertyRNA *prop;

  srna = RNA_def_struct(brna, "StripElement", nullptr);
  RNA_def_struct_ui_text(srna, "Strip Element", "Sequence strip data for a single frame");
  RNA_def_struct_sdna(srna, "StripElem");

  prop = RNA_def_property(srna, "filename", PROP_STRING, PROP_FILENAME);
  RNA_def_property_string_sdna(prop, nullptr, "filename");
  RNA_def_property_ui_text(prop, "Filename", "Name of the source file");
  RNA_def_property_update(prop, NC_SCENE | ND_SEQUENCER, "rna_StripElement_update");

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
      prop, "rna_Strip_retiming_key_frame_get", "rna_Strip_retiming_key_frame_set", nullptr);
  RNA_def_property_ui_text(prop, "Timeline Frame", "Position of retiming key in timeline");

  FunctionRNA *func = RNA_def_function(srna, "remove", "rna_Strip_retiming_key_remove");
  RNA_def_function_flag(func, FUNC_USE_SELF_ID);
  RNA_def_function_ui_description(func, "Remove retiming key");
}

static void rna_def_strip_crop(BlenderRNA *brna)
{
  StructRNA *srna;
  PropertyRNA *prop;

  srna = RNA_def_struct(brna, "StripCrop", nullptr);
  RNA_def_struct_ui_text(srna, "Strip Crop", "Cropping parameters for a sequence strip");
  RNA_def_struct_sdna(srna, "StripCrop");

  prop = RNA_def_property(srna, "max_y", PROP_INT, PROP_PIXEL);
  RNA_def_property_int_sdna(prop, nullptr, "top");
  RNA_def_property_ui_text(prop, "Top", "Number of pixels to crop from the top");
  RNA_def_property_ui_range(prop, 0, 4096, 1, -1);
  RNA_def_property_update(prop, NC_SCENE | ND_SEQUENCER, "rna_StripCrop_update");

  prop = RNA_def_property(srna, "min_y", PROP_INT, PROP_PIXEL);
  RNA_def_property_int_sdna(prop, nullptr, "bottom");
  RNA_def_property_ui_text(prop, "Bottom", "Number of pixels to crop from the bottom");
  RNA_def_property_ui_range(prop, 0, 4096, 1, -1);
  RNA_def_property_update(prop, NC_SCENE | ND_SEQUENCER, "rna_StripCrop_update");

  prop = RNA_def_property(srna, "min_x", PROP_INT, PROP_PIXEL);
  RNA_def_property_int_sdna(prop, nullptr, "left");
  RNA_def_property_ui_text(prop, "Left", "Number of pixels to crop from the left side");
  RNA_def_property_ui_range(prop, 0, 4096, 1, -1);
  RNA_def_property_update(prop, NC_SCENE | ND_SEQUENCER, "rna_StripCrop_update");

  prop = RNA_def_property(srna, "max_x", PROP_INT, PROP_PIXEL);
  RNA_def_property_int_sdna(prop, nullptr, "right");
  RNA_def_property_ui_text(prop, "Right", "Number of pixels to crop from the right side");
  RNA_def_property_ui_range(prop, 0, 4096, 1, -1);
  RNA_def_property_update(prop, NC_SCENE | ND_SEQUENCER, "rna_StripCrop_update");

  RNA_def_struct_path_func(srna, "rna_StripCrop_path");
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

  srna = RNA_def_struct(brna, "StripTransform", nullptr);
  RNA_def_struct_ui_text(srna, "Strip Transform", "Transform parameters for a sequence strip");
  RNA_def_struct_sdna(srna, "StripTransform");

  prop = RNA_def_property(srna, "scale_x", PROP_FLOAT, PROP_UNSIGNED);
  RNA_def_property_float_sdna(prop, nullptr, "scale_x");
  RNA_def_property_ui_text(prop, "Scale X", "Scale along X axis");
  RNA_def_property_ui_range(prop, 0, FLT_MAX, 3, 3);
  RNA_def_property_float_default(prop, 1.0f);
  RNA_def_property_update(prop, NC_SCENE | ND_SEQUENCER, "rna_StripTransform_update");

  prop = RNA_def_property(srna, "scale_y", PROP_FLOAT, PROP_UNSIGNED);
  RNA_def_property_float_sdna(prop, nullptr, "scale_y");
  RNA_def_property_ui_text(prop, "Scale Y", "Scale along Y axis");
  RNA_def_property_ui_range(prop, 0, FLT_MAX, 3, 3);
  RNA_def_property_float_default(prop, 1.0f);
  RNA_def_property_update(prop, NC_SCENE | ND_SEQUENCER, "rna_StripTransform_update");

  prop = RNA_def_property(srna, "offset_x", PROP_FLOAT, PROP_PIXEL);
  RNA_def_property_float_sdna(prop, nullptr, "xofs");
  RNA_def_property_ui_text(prop, "Translate X", "Move along X axis");
  RNA_def_property_ui_range(prop, -FLT_MAX, FLT_MAX, 100, 3);
  RNA_def_property_update(prop, NC_SCENE | ND_SEQUENCER, "rna_StripTransform_update");

  prop = RNA_def_property(srna, "offset_y", PROP_FLOAT, PROP_PIXEL);
  RNA_def_property_float_sdna(prop, nullptr, "yofs");
  RNA_def_property_ui_text(prop, "Translate Y", "Move along Y axis");
  RNA_def_property_ui_range(prop, -FLT_MAX, FLT_MAX, 100, 3);
  RNA_def_property_update(prop, NC_SCENE | ND_SEQUENCER, "rna_StripTransform_update");

  prop = RNA_def_property(srna, "rotation", PROP_FLOAT, PROP_ANGLE);
  RNA_def_property_float_sdna(prop, nullptr, "rotation");
  RNA_def_property_ui_text(prop, "Rotation", "Rotate around image center");
  RNA_def_property_update(prop, NC_SCENE | ND_SEQUENCER, "rna_StripTransform_update");

  prop = RNA_def_property(srna, "origin", PROP_FLOAT, PROP_NONE);
  RNA_def_property_float_sdna(prop, nullptr, "origin");
  RNA_def_property_ui_text(prop, "Origin", "Origin of image for transformation");
  RNA_def_property_ui_range(prop, 0, 1, 1, 3);
  RNA_def_property_update(prop, NC_SCENE | ND_SEQUENCER, "rna_StripTransform_update");

  prop = RNA_def_property(srna, "filter", PROP_ENUM, PROP_NONE);
  RNA_def_property_enum_sdna(prop, nullptr, "filter");
  RNA_def_property_enum_items(prop, transform_filter_items);
  RNA_def_property_enum_default(prop, SEQ_TRANSFORM_FILTER_AUTO);
  RNA_def_property_ui_text(prop, "Filter", "Type of filter to use for image transformation");
  RNA_def_property_update(prop, NC_SCENE | ND_SEQUENCER, "rna_StripTransform_update");

  RNA_def_struct_path_func(srna, "rna_StripTransform_path");
}

static void rna_def_strip_proxy(BlenderRNA *brna)
{
  StructRNA *srna;
  PropertyRNA *prop;

  static const EnumPropertyItem strip_tc_items[] = {
      {SEQ_PROXY_TC_NONE,
       "NONE",
       0,
       "None",
       "Ignore generated timecodes, seek in movie stream based on calculated timestamp"},
      {SEQ_PROXY_TC_RECORD_RUN,
       "RECORD_RUN",
       0,
       "Record Run",
       "Seek based on timestamps read from movie stream, giving the best match between scene and "
       "movie times"},
      {SEQ_PROXY_TC_RECORD_RUN_NO_GAPS,
       "RECORD_RUN_NO_GAPS",
       0,
       "Record Run No Gaps",
       "Effectively convert movie to an image sequence, ignoring incomplete or dropped frames, "
       "and changes in frame rate"},
      {0, nullptr, 0, nullptr, nullptr},
  };

  srna = RNA_def_struct(brna, "StripProxy", nullptr);
  RNA_def_struct_ui_text(srna, "Strip Proxy", "Proxy parameters for a sequence strip");
  RNA_def_struct_sdna(srna, "StripProxy");

  prop = RNA_def_property(srna, "directory", PROP_STRING, PROP_DIRPATH);
  RNA_def_property_string_sdna(prop, nullptr, "dirpath");
  RNA_def_property_ui_text(prop, "Directory", "Location to store the proxy files");
  RNA_def_property_flag(prop, PROP_PATH_SUPPORTS_BLEND_RELATIVE);
  RNA_def_property_update(prop, NC_SCENE | ND_SEQUENCER, "rna_StripProxy_update");

  prop = RNA_def_property(srna, "filepath", PROP_STRING, PROP_FILEPATH);
  RNA_def_property_ui_text(prop, "Path", "Location of custom proxy file");
  RNA_def_property_translation_context(prop, BLT_I18NCONTEXT_EDITOR_FILEBROWSER);
  RNA_def_property_string_funcs(prop,
                                "rna_Strip_proxy_filepath_get",
                                "rna_Strip_proxy_filepath_length",
                                "rna_Strip_proxy_filepath_set");
  RNA_def_property_flag(prop, PROP_PATH_SUPPORTS_BLEND_RELATIVE);
  RNA_def_property_update(prop, NC_SCENE | ND_SEQUENCER, "rna_StripProxy_update");

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

  prop = RNA_def_property(srna, "quality", PROP_INT, PROP_UNSIGNED);
  RNA_def_property_int_sdna(prop, nullptr, "quality");
  RNA_def_property_ui_text(prop, "Quality", "Quality of proxies to build");
  RNA_def_property_ui_range(prop, 1, 100, 1, -1);

  prop = RNA_def_property(srna, "timecode", PROP_ENUM, PROP_NONE);
  RNA_def_property_enum_sdna(prop, nullptr, "tc");
  RNA_def_property_enum_items(prop, strip_tc_items);
  RNA_def_property_ui_text(prop, "Timecode", "Method for reading the inputs timecode");
  RNA_def_property_update(prop, NC_SCENE | ND_SEQUENCER, "rna_Strip_tcindex_update");

  prop = RNA_def_property(srna, "use_proxy_custom_directory", PROP_BOOLEAN, PROP_NONE);
  RNA_def_property_boolean_sdna(prop, nullptr, "storage", SEQ_STORAGE_PROXY_CUSTOM_DIR);
  RNA_def_property_ui_text(prop, "Proxy Custom Directory", "Use a custom directory to store data");
  RNA_def_property_update(
      prop, NC_SCENE | ND_SEQUENCER, "rna_Strip_invalidate_preprocessed_update");

  prop = RNA_def_property(srna, "use_proxy_custom_file", PROP_BOOLEAN, PROP_NONE);
  RNA_def_property_boolean_sdna(prop, nullptr, "storage", SEQ_STORAGE_PROXY_CUSTOM_FILE);
  RNA_def_property_ui_text(prop, "Proxy Custom File", "Use a custom file to read proxy data from");
  RNA_def_property_update(
      prop, NC_SCENE | ND_SEQUENCER, "rna_Strip_invalidate_preprocessed_update");
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

  srna = RNA_def_struct(brna, "StripColorBalanceData", nullptr);
  RNA_def_struct_ui_text(srna,
                         "Strip Color Balance Data",
                         "Color balance parameters for a sequence strip and its modifiers");
  RNA_def_struct_sdna(srna, "StripColorBalance");

  prop = RNA_def_property(srna, "correction_method", PROP_ENUM, PROP_NONE);
  RNA_def_property_enum_sdna(prop, nullptr, "method");
  RNA_def_property_enum_items(prop, method_items);
  RNA_def_property_ui_text(prop, "Correction Method", "");
  RNA_def_property_update(prop, NC_SCENE | ND_SEQUENCER, "rna_StripColorBalance_update");

  prop = RNA_def_property(srna, "lift", PROP_FLOAT, PROP_COLOR_GAMMA);
  RNA_def_property_ui_text(prop, "Lift", "Color balance lift (shadows)");
  RNA_def_property_ui_range(prop, 0, 2, 0.1, 3);
  RNA_def_property_float_default(prop, 1.0f);
  RNA_def_property_update(prop, NC_SCENE | ND_SEQUENCER, "rna_StripColorBalance_update");

  prop = RNA_def_property(srna, "gamma", PROP_FLOAT, PROP_COLOR_GAMMA);
  RNA_def_property_ui_text(prop, "Gamma", "Color balance gamma (midtones)");
  RNA_def_property_ui_range(prop, 0, 2, 0.1, 3);
  RNA_def_property_float_default(prop, 1.0f);
  RNA_def_property_update(prop, NC_SCENE | ND_SEQUENCER, "rna_StripColorBalance_update");

  prop = RNA_def_property(srna, "gain", PROP_FLOAT, PROP_COLOR_GAMMA);
  RNA_def_property_ui_text(prop, "Gain", "Color balance gain (highlights)");
  RNA_def_property_ui_range(prop, 0, 2, 0.1, 3);
  RNA_def_property_float_default(prop, 1.0f);
  RNA_def_property_update(prop, NC_SCENE | ND_SEQUENCER, "rna_StripColorBalance_update");

  prop = RNA_def_property(srna, "slope", PROP_FLOAT, PROP_COLOR_GAMMA);
  RNA_def_property_ui_text(prop, "Slope", "Correction for highlights");
  RNA_def_property_ui_range(prop, 0, 2, 0.1, 3);
  RNA_def_property_float_default(prop, 1.0f);
  RNA_def_property_update(prop, NC_SCENE | ND_SEQUENCER, "rna_StripColorBalance_update");

  prop = RNA_def_property(srna, "offset", PROP_FLOAT, PROP_COLOR_GAMMA);
  RNA_def_property_ui_text(prop, "Offset", "Correction for entire tonal range");
  RNA_def_property_ui_range(prop, 0, 2, 0.1, 3);
  RNA_def_property_float_default(prop, 1.0f);
  RNA_def_property_update(prop, NC_SCENE | ND_SEQUENCER, "rna_StripColorBalance_update");

  prop = RNA_def_property(srna, "power", PROP_FLOAT, PROP_COLOR_GAMMA);
  RNA_def_property_ui_text(prop, "Power", "Correction for midtones");
  RNA_def_property_translation_context(prop, BLT_I18NCONTEXT_ID_MOVIECLIP);
  RNA_def_property_ui_range(prop, 0, 2, 0.1, 3);
  RNA_def_property_float_default(prop, 1.0f);
  RNA_def_property_update(prop, NC_SCENE | ND_SEQUENCER, "rna_StripColorBalance_update");

  prop = RNA_def_property(srna, "invert_lift", PROP_BOOLEAN, PROP_NONE);
  RNA_def_property_boolean_sdna(prop, nullptr, "flag", SEQ_COLOR_BALANCE_INVERSE_LIFT);
  RNA_def_property_ui_text(prop, "Inverse Lift", "Invert the lift color");
  RNA_def_property_update(prop, NC_SCENE | ND_SEQUENCER, "rna_StripColorBalance_update");

  prop = RNA_def_property(srna, "invert_gamma", PROP_BOOLEAN, PROP_NONE);
  RNA_def_property_boolean_sdna(prop, nullptr, "flag", SEQ_COLOR_BALANCE_INVERSE_GAMMA);
  RNA_def_property_ui_text(prop, "Inverse Gamma", "Invert the gamma color");
  RNA_def_property_update(prop, NC_SCENE | ND_SEQUENCER, "rna_StripColorBalance_update");

  prop = RNA_def_property(srna, "invert_gain", PROP_BOOLEAN, PROP_NONE);
  RNA_def_property_boolean_sdna(prop, nullptr, "flag", SEQ_COLOR_BALANCE_INVERSE_GAIN);
  RNA_def_property_ui_text(prop, "Inverse Gain", "Invert the gain color");
  RNA_def_property_update(prop, NC_SCENE | ND_SEQUENCER, "rna_StripColorBalance_update");

  prop = RNA_def_property(srna, "invert_slope", PROP_BOOLEAN, PROP_NONE);
  RNA_def_property_boolean_sdna(prop, nullptr, "flag", SEQ_COLOR_BALANCE_INVERSE_SLOPE);
  RNA_def_property_ui_text(prop, "Inverse Slope", "Invert the slope color");
  RNA_def_property_update(prop, NC_SCENE | ND_SEQUENCER, "rna_StripColorBalance_update");

  prop = RNA_def_property(srna, "invert_offset", PROP_BOOLEAN, PROP_NONE);
  RNA_def_property_boolean_sdna(prop, nullptr, "flag", SEQ_COLOR_BALANCE_INVERSE_OFFSET);
  RNA_def_property_ui_text(prop, "Inverse Offset", "Invert the offset color");
  RNA_def_property_update(prop, NC_SCENE | ND_SEQUENCER, "rna_StripColorBalance_update");

  prop = RNA_def_property(srna, "invert_power", PROP_BOOLEAN, PROP_NONE);
  RNA_def_property_boolean_sdna(prop, nullptr, "flag", SEQ_COLOR_BALANCE_INVERSE_POWER);
  RNA_def_property_ui_text(prop, "Inverse Power", "Invert the power color");
  RNA_def_property_update(prop, NC_SCENE | ND_SEQUENCER, "rna_StripColorBalance_update");

  /* not yet used */
#  if 0
  prop = RNA_def_property(srna, "exposure", PROP_FLOAT, PROP_NONE);
  RNA_def_property_range(prop, 0.0f, 1.0f);
  RNA_def_property_ui_text(prop, "Exposure", "");
  RNA_def_property_update(prop, NC_SCENE | ND_SEQUENCER, "rna_Strip_ColorBabalnce_update");

  prop = RNA_def_property(srna, "saturation", PROP_FLOAT, PROP_NONE);
  RNA_def_property_range(prop, 0.0f, 1.0f);
  RNA_def_property_ui_text(prop, "Saturation", "");
  RNA_def_property_update(prop, NC_SCENE | ND_SEQUENCER, "rna_Strip_ColorBabalnce_update");
#  endif

  RNA_def_struct_path_func(srna, "rna_StripColorBalance_path");
}

static void rna_def_strip_color_balance(BlenderRNA *brna)
{
  StructRNA *srna;

  srna = RNA_def_struct(brna, "StripColorBalance", "StripColorBalanceData");
  RNA_def_struct_ui_text(
      srna, "Strip Color Balance", "Color balance parameters for a sequence strip");
  RNA_def_struct_sdna(srna, "StripColorBalance");
}

static const EnumPropertyItem blend_mode_items[] = {
    {STRIP_BLEND_REPLACE, "REPLACE", 0, "Replace", ""},
    {STRIP_BLEND_CROSS, "CROSS", 0, "Cross", ""},
    RNA_ENUM_ITEM_SEPR,
    {STRIP_BLEND_DARKEN, "DARKEN", 0, "Darken", ""},
    {STRIP_BLEND_MUL, "MULTIPLY", 0, "Multiply", ""},
    {STRIP_BLEND_COLOR_BURN, "BURN", 0, "Color Burn", ""},
    {STRIP_BLEND_LINEAR_BURN, "LINEAR_BURN", 0, "Linear Burn", ""},
    RNA_ENUM_ITEM_SEPR,
    {STRIP_BLEND_LIGHTEN, "LIGHTEN", 0, "Lighten", ""},
    {STRIP_BLEND_SCREEN, "SCREEN", 0, "Screen", ""},
    {STRIP_BLEND_DODGE, "DODGE", 0, "Color Dodge", ""},
    {STRIP_BLEND_ADD, "ADD", 0, "Add", ""},
    RNA_ENUM_ITEM_SEPR,
    {STRIP_BLEND_OVERLAY, "OVERLAY", 0, "Overlay", ""},
    {STRIP_BLEND_SOFT_LIGHT, "SOFT_LIGHT", 0, "Soft Light", ""},
    {STRIP_BLEND_HARD_LIGHT, "HARD_LIGHT", 0, "Hard Light", ""},
    {STRIP_BLEND_VIVID_LIGHT, "VIVID_LIGHT", 0, "Vivid Light", ""},
    {STRIP_BLEND_LIN_LIGHT, "LINEAR_LIGHT", 0, "Linear Light", ""},
    {STRIP_BLEND_PIN_LIGHT, "PIN_LIGHT", 0, "Pin Light", ""},
    RNA_ENUM_ITEM_SEPR,
    {STRIP_BLEND_DIFFERENCE, "DIFFERENCE", 0, "Difference", ""},
    {STRIP_BLEND_EXCLUSION, "EXCLUSION", 0, "Exclusion", ""},
    {STRIP_BLEND_SUB, "SUBTRACT", 0, "Subtract", ""},
    RNA_ENUM_ITEM_SEPR,
    {STRIP_BLEND_HUE, "HUE", 0, "Hue", ""},
    {STRIP_BLEND_SATURATION, "SATURATION", 0, "Saturation", ""},
    {STRIP_BLEND_BLEND_COLOR, "COLOR", 0, "Color", ""},
    {STRIP_BLEND_VALUE, "VALUE", 0, "Value", ""},
    RNA_ENUM_ITEM_SEPR,
    {STRIP_BLEND_ALPHAOVER, "ALPHA_OVER", 0, "Alpha Over", ""},
    {STRIP_BLEND_ALPHAUNDER, "ALPHA_UNDER", 0, "Alpha Under", ""},
    {STRIP_BLEND_GAMCROSS, "GAMMA_CROSS", 0, "Gamma Cross", ""},
    {0, nullptr, 0, nullptr, nullptr},
};

static void rna_def_strip_modifiers(BlenderRNA *brna, PropertyRNA *cprop)
{
  StructRNA *srna;
  PropertyRNA *prop;

  FunctionRNA *func;
  PropertyRNA *parm;

  RNA_def_property_srna(cprop, "StripModifiers");
  srna = RNA_def_struct(brna, "StripModifiers", nullptr);
  RNA_def_struct_sdna(srna, "Strip");
  RNA_def_struct_ui_text(srna, "Strip Modifiers", "Collection of strip modifiers");

  /* add modifier */
  func = RNA_def_function(srna, "new", "rna_Strip_modifier_new");
  RNA_def_function_flag(func, FUNC_USE_CONTEXT | FUNC_USE_REPORTS);
  RNA_def_function_ui_description(func, "Add a new modifier");
  parm = RNA_def_string(func, "name", "Name", 0, "", "New name for the modifier");
  RNA_def_parameter_flags(parm, PropertyFlag(0), PARM_REQUIRED);
  /* modifier to add */
  parm = RNA_def_enum(func,
                      "type",
                      rna_enum_strip_modifier_type_items,
                      eSeqModifierType_ColorBalance,
                      "",
                      "Modifier type to add");
  RNA_def_parameter_flags(parm, PropertyFlag(0), PARM_REQUIRED);
  /* return type */
  parm = RNA_def_pointer(func, "modifier", "StripModifier", "", "Newly created modifier");
  RNA_def_function_return(func, parm);

  /* remove modifier */
  func = RNA_def_function(srna, "remove", "rna_Strip_modifier_remove");
  RNA_def_function_flag(func, FUNC_USE_CONTEXT | FUNC_USE_REPORTS);
  RNA_def_function_ui_description(func, "Remove an existing modifier from the strip");
  /* modifier to remove */
  parm = RNA_def_pointer(func, "modifier", "StripModifier", "", "Modifier to remove");
  RNA_def_parameter_flags(parm, PROP_NEVER_NULL, PARM_REQUIRED | PARM_RNAPTR);
  RNA_def_parameter_clear_flags(parm, PROP_THICK_WRAP, ParameterFlag(0));

  /* clear all modifiers */
  func = RNA_def_function(srna, "clear", "rna_Strip_modifier_clear");
  RNA_def_function_flag(func, FUNC_USE_CONTEXT);
  RNA_def_function_ui_description(func, "Remove all modifiers from the strip");

  /* Active modifier. */
  prop = RNA_def_property(srna, "active", PROP_POINTER, PROP_NONE);
  RNA_def_property_struct_type(prop, "StripModifier");
  RNA_def_property_pointer_funcs(
      prop, "rna_Strip_active_modifier_get", "rna_Strip_active_modifier_set", nullptr, nullptr);
  RNA_def_property_flag(prop, PROP_EDITABLE);
  RNA_def_property_flag(prop, PROP_NO_DEG_UPDATE);
  RNA_def_property_override_flag(prop, PROPOVERRIDE_OVERRIDABLE_LIBRARY);
  RNA_def_property_ui_text(prop, "Active Modifier", "The active strip modifier in the list");
  RNA_def_property_update(prop, NC_OBJECT | ND_MODIFIER, nullptr);
}

static void rna_def_strip(BlenderRNA *brna)
{
  StructRNA *srna;
  PropertyRNA *prop;

  static const EnumPropertyItem strip_type_items[] = {
      {STRIP_TYPE_IMAGE, "IMAGE", 0, "Image", ""},
      {STRIP_TYPE_META, "META", 0, "Meta", ""},
      {STRIP_TYPE_SCENE, "SCENE", 0, "Scene", ""},
      {STRIP_TYPE_MOVIE, "MOVIE", 0, "Movie", ""},
      {STRIP_TYPE_MOVIECLIP, "MOVIECLIP", 0, "Clip", ""},
      {STRIP_TYPE_MASK, "MASK", 0, "Mask", ""},
      {STRIP_TYPE_SOUND_RAM, "SOUND", 0, "Sound", ""},
      {STRIP_TYPE_CROSS, "CROSS", 0, "Crossfade", ""},
      {STRIP_TYPE_ADD, "ADD", 0, "Add", ""},
      {STRIP_TYPE_SUB, "SUBTRACT", 0, "Subtract", ""},
      {STRIP_TYPE_ALPHAOVER, "ALPHA_OVER", 0, "Alpha Over", ""},
      {STRIP_TYPE_ALPHAUNDER, "ALPHA_UNDER", 0, "Alpha Under", ""},
      {STRIP_TYPE_GAMCROSS, "GAMMA_CROSS", 0, "Gamma Crossfade", ""},
      {STRIP_TYPE_MUL, "MULTIPLY", 0, "Multiply", ""},
      {STRIP_TYPE_WIPE, "WIPE", 0, "Wipe", ""},
      {STRIP_TYPE_GLOW, "GLOW", 0, "Glow", ""},
      {STRIP_TYPE_COLOR, "COLOR", 0, "Color", ""},
      {STRIP_TYPE_SPEED, "SPEED", 0, "Speed", ""},
      {STRIP_TYPE_MULTICAM, "MULTICAM", 0, "Multicam Selector", ""},
      {STRIP_TYPE_ADJUSTMENT, "ADJUSTMENT", 0, "Adjustment Layer", ""},
      {STRIP_TYPE_GAUSSIAN_BLUR, "GAUSSIAN_BLUR", 0, "Gaussian Blur", ""},
      {STRIP_TYPE_TEXT, "TEXT", 0, "Text", ""},
      {STRIP_TYPE_COLORMIX, "COLORMIX", 0, "Color Mix", ""},
      {0, nullptr, 0, nullptr, nullptr},
  };

  srna = RNA_def_struct(brna, "Strip", nullptr);
  RNA_def_struct_ui_text(srna, "Strip", "Sequence strip in the sequence editor");
  RNA_def_struct_refine_func(srna, "rna_Strip_refine");
  RNA_def_struct_path_func(srna, "rna_Strip_path");
  RNA_def_struct_ui_icon(srna, ICON_SEQ_SEQUENCER);
  RNA_def_struct_idprops_func(srna, "rna_Strip_idprops");
  RNA_def_struct_system_idprops_func(srna, "rna_Strip_system_idprops");

  prop = RNA_def_property(srna, "name", PROP_STRING, PROP_NONE);
  RNA_def_property_string_funcs(
      prop, "rna_Strip_name_get", "rna_Strip_name_length", "rna_Strip_name_set");
  RNA_def_property_string_maxlength(prop, sizeof(Strip::name) - 2);
  RNA_def_property_ui_text(prop, "Name", "");
  RNA_def_struct_name_property(srna, prop);
  RNA_def_property_update(prop, NC_SCENE | ND_SEQUENCER, nullptr);

  prop = RNA_def_property(srna, "type", PROP_ENUM, PROP_NONE);
  RNA_def_property_clear_flag(prop, PROP_EDITABLE);
  RNA_def_property_enum_items(prop, strip_type_items);
  RNA_def_property_ui_text(prop, "Type", "");
  RNA_def_property_translation_context(prop, BLT_I18NCONTEXT_ID_SEQUENCE);
  RNA_def_property_update(prop, NC_SCENE | ND_SEQUENCER, "rna_Strip_invalidate_raw_update");

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
  RNA_def_property_flag(prop, PROP_CONTEXT_UPDATE);
  RNA_def_property_update(prop, NC_SCENE | ND_SEQUENCER, "rna_Strip_mute_update");

  prop = RNA_def_property(srna, "lock", PROP_BOOLEAN, PROP_NONE);
  RNA_def_property_boolean_sdna(prop, nullptr, "flag", SEQ_LOCK);
  RNA_def_property_boolean_funcs(prop, "rna_Strip_lock_get", nullptr);
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
      prop, "rna_Strip_frame_length_get", "rna_Strip_frame_length_set", nullptr);
  RNA_def_property_editable_func(prop, "rna_Strip_frame_editable");
  RNA_def_property_update(
      prop, NC_SCENE | ND_SEQUENCER, "rna_Strip_invalidate_preprocessed_update");

  prop = RNA_def_property(srna, "frame_duration", PROP_INT, PROP_TIME);
  RNA_def_property_int_funcs(prop, "rna_Strip_frame_duration_get", nullptr, nullptr);
  RNA_def_property_clear_flag(prop, PROP_EDITABLE | PROP_ANIMATABLE);
  RNA_def_property_range(prop, 1, MAXFRAME);
  RNA_def_property_ui_text(
      prop, "Length", "The length of the contents of this strip before the handles are applied");

  prop = RNA_def_property(srna, "frame_start", PROP_FLOAT, PROP_TIME);
  RNA_def_property_float_sdna(prop, nullptr, "start");
  RNA_def_property_clear_flag(prop, PROP_ANIMATABLE);
  RNA_def_property_ui_text(prop, "Start Frame", "X position where the strip begins");
  RNA_def_property_ui_range(prop, MINFRAME, MAXFRAME, 100.0f, 0);
  RNA_def_property_float_funcs(
      prop, nullptr, "rna_Strip_start_frame_set", nullptr); /* overlap tests and calc_seq_disp */
  RNA_def_property_editable_func(prop, "rna_Strip_frame_editable");
  RNA_def_property_update(
      prop, NC_SCENE | ND_SEQUENCER, "rna_Strip_invalidate_preprocessed_update");

  prop = RNA_def_property(srna, "frame_final_start", PROP_INT, PROP_TIME);
  RNA_def_property_int_sdna(prop, nullptr, "startdisp");
  RNA_def_property_int_funcs(
      prop, "rna_Strip_frame_final_start_get", "rna_Strip_start_frame_final_set", nullptr);
  RNA_def_property_editable_func(prop, "rna_Strip_frame_editable");
  RNA_def_property_clear_flag(prop, PROP_ANIMATABLE);
  RNA_def_property_ui_text(
      prop,
      "Start Frame",
      "Start frame displayed in the sequence editor after offsets are applied, setting this is "
      "equivalent to moving the handle, not the actual start frame");
  /* overlap tests and calc_seq_disp */
  RNA_def_property_update(
      prop, NC_SCENE | ND_SEQUENCER, "rna_Strip_invalidate_preprocessed_update");

  prop = RNA_def_property(srna, "frame_final_end", PROP_INT, PROP_TIME);
  RNA_def_property_int_sdna(prop, nullptr, "enddisp");
  RNA_def_property_int_funcs(
      prop, "rna_Strip_frame_final_end_get", "rna_Strip_end_frame_final_set", nullptr);
  RNA_def_property_editable_func(prop, "rna_Strip_frame_editable");
  RNA_def_property_clear_flag(prop, PROP_ANIMATABLE);
  RNA_def_property_ui_text(
      prop, "End Frame", "End frame displayed in the sequence editor after offsets are applied");
  /* overlap tests and calc_seq_disp */
  RNA_def_property_update(
      prop, NC_SCENE | ND_SEQUENCER, "rna_Strip_invalidate_preprocessed_update");

  prop = RNA_def_property(srna, "frame_offset_start", PROP_FLOAT, PROP_TIME);
  RNA_def_property_float_sdna(prop, nullptr, "startofs");
  //  RNA_def_property_clear_flag(prop, PROP_EDITABLE); /* overlap tests */
  RNA_def_property_ui_text(prop, "Start Offset", "");
  RNA_def_property_ui_range(prop, MINFRAME, MAXFRAME, 100.0f, 0);
  RNA_def_property_float_funcs(
      prop, nullptr, "rna_Strip_frame_offset_start_set", "rna_Strip_frame_offset_start_range");
  RNA_def_property_update(prop, NC_SCENE | ND_SEQUENCER, "rna_Strip_frame_change_update");

  prop = RNA_def_property(srna, "frame_offset_end", PROP_FLOAT, PROP_TIME);
  RNA_def_property_float_sdna(prop, nullptr, "endofs");
  //  RNA_def_property_clear_flag(prop, PROP_EDITABLE); /* overlap tests */
  RNA_def_property_ui_text(prop, "End Offset", "");
  RNA_def_property_ui_range(prop, MINFRAME, MAXFRAME, 100.0f, 0);
  RNA_def_property_float_funcs(
      prop, nullptr, "rna_Strip_frame_offset_end_set", "rna_Strip_frame_offset_end_range");
  RNA_def_property_update(prop, NC_SCENE | ND_SEQUENCER, "rna_Strip_frame_change_update");

  prop = RNA_def_property(srna, "channel", PROP_INT, PROP_UNSIGNED);
  RNA_def_property_int_sdna(prop, nullptr, "channel");
  RNA_def_property_clear_flag(prop, PROP_ANIMATABLE);
  RNA_def_property_range(prop, 1, blender::seq::MAX_CHANNELS);
  RNA_def_property_ui_text(prop, "Channel", "Y position of the sequence strip");
  RNA_def_property_int_funcs(prop, nullptr, "rna_Strip_channel_set", nullptr); /* overlap test */
  RNA_def_property_update(
      prop, NC_SCENE | ND_SEQUENCER, "rna_Strip_invalidate_preprocessed_update");

  prop = RNA_def_property(srna, "use_linear_modifiers", PROP_BOOLEAN, PROP_NONE);
  RNA_def_property_boolean_sdna(prop, nullptr, "flag", SEQ_USE_LINEAR_MODIFIERS);
  RNA_def_property_ui_text(prop,
                           "Use Linear Modifiers",
                           "Calculate modifiers in linear space instead of sequencer's space");
  RNA_def_property_update(
      prop, NC_SCENE | ND_SEQUENCER, "rna_Strip_invalidate_preprocessed_update");

  /* blending */

  prop = RNA_def_property(srna, "blend_type", PROP_ENUM, PROP_NONE);
  RNA_def_property_enum_sdna(prop, nullptr, "blend_mode");
  RNA_def_property_enum_items(prop, blend_mode_items);
  RNA_def_property_enum_default(prop, STRIP_TYPE_ALPHAOVER);
  RNA_def_property_ui_text(
      prop, "Blending Mode", "Method for controlling how the strip combines with other strips");
  RNA_def_property_translation_context(prop, BLT_I18NCONTEXT_COLOR);
  RNA_def_property_update(
      prop, NC_SCENE | ND_SEQUENCER, "rna_Strip_invalidate_preprocessed_update");

  prop = RNA_def_property(srna, "blend_alpha", PROP_FLOAT, PROP_FACTOR);
  RNA_def_property_range(prop, 0.0f, 1.0f);
  RNA_def_property_float_default(prop, 1.0f);
  RNA_def_property_ui_text(
      prop, "Blend Opacity", "Percentage of how much the strip's colors affect other strips");
  /* stupid 0-100 -> 0-1 */
  RNA_def_property_float_funcs(prop, "rna_Strip_opacity_get", "rna_Strip_opacity_set", nullptr);
  RNA_def_property_update(
      prop, NC_SCENE | ND_SEQUENCER, "rna_Strip_invalidate_preprocessed_update");

  prop = RNA_def_property(srna, "effect_fader", PROP_FLOAT, PROP_FACTOR);
  RNA_def_property_range(prop, 0.0f, 1.0f);
  RNA_def_property_ui_range(prop, 0.0f, 1.0f, 0.1, 3);
  RNA_def_property_float_sdna(prop, nullptr, "effect_fader");
  RNA_def_property_ui_text(prop, "Effect Fader Position", "Custom fade value");
  RNA_def_property_update(
      prop, NC_SCENE | ND_SEQUENCER, "rna_Strip_invalidate_preprocessed_update");

  prop = RNA_def_property(srna, "use_default_fade", PROP_BOOLEAN, PROP_NONE);
  RNA_def_property_boolean_sdna(prop, nullptr, "flag", SEQ_USE_EFFECT_DEFAULT_FADE);
  RNA_def_property_ui_text(prop,
                           "Use Default Fade",
                           "Fade effect using the built-in default (usually makes the transition "
                           "as long as the effect strip)");
  RNA_def_property_update(
      prop, NC_SCENE | ND_SEQUENCER, "rna_Strip_invalidate_preprocessed_update");

  prop = RNA_def_property(srna, "color_tag", PROP_ENUM, PROP_NONE);
  RNA_def_property_enum_sdna(prop, nullptr, "color_tag");
  RNA_def_property_enum_funcs(prop, "rna_Strip_color_tag_get", "rna_Strip_color_tag_set", nullptr);
  RNA_def_property_enum_items(prop, rna_enum_strip_color_items);
  RNA_def_property_ui_text(prop, "Strip Color", "Color tag for a strip");
  RNA_def_property_update(prop, NC_SCENE | ND_SEQUENCER, nullptr);

  /* modifiers */
  prop = RNA_def_property(srna, "modifiers", PROP_COLLECTION, PROP_NONE);
  RNA_def_property_struct_type(prop, "StripModifier");
  RNA_def_property_ui_text(prop, "Modifiers", "Modifiers affecting this strip");
  rna_def_strip_modifiers(brna, prop);

  prop = RNA_def_property(srna, "show_retiming_keys", PROP_BOOLEAN, PROP_NONE);
  RNA_def_property_boolean_sdna(prop, nullptr, "flag", SEQ_SHOW_RETIMING);
  RNA_def_property_ui_text(prop, "Show Retiming Keys", "Show retiming keys, so they can be moved");

  RNA_api_strip(srna);
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

static void rna_def_strips_top_level(BlenderRNA *brna)
{
  StructRNA *srna;

  srna = RNA_def_struct(brna, "StripsTopLevel", nullptr);
  RNA_def_struct_sdna(srna, "Editing");
  RNA_def_struct_ui_text(srna, "Strips", "Collection of Strips");

  RNA_api_strips(srna, false);
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
  RNA_def_struct_path_func(srna, "rna_SequenceEditor_path");
  RNA_def_struct_ui_icon(srna, ICON_SEQUENCE);
  RNA_def_struct_sdna(srna, "Editing");

  rna_def_strips_top_level(brna);

  prop = RNA_def_property(srna, "strips", PROP_COLLECTION, PROP_NONE);
  RNA_def_property_srna(prop, "StripsTopLevel");
  RNA_def_property_collection_sdna(prop, nullptr, "seqbase", nullptr);
  RNA_def_property_struct_type(prop, "Strip");
  RNA_def_property_ui_text(prop, "Strips", "Top-level strips only");

  prop = RNA_def_property(srna, "strips_all", PROP_COLLECTION, PROP_NONE);
  RNA_def_property_collection_sdna(prop, nullptr, "seqbase", nullptr);
  RNA_def_property_struct_type(prop, "Strip");
  RNA_def_property_ui_text(
      prop, "All Strips", "All strips, recursively including those inside metastrips");
  RNA_def_property_collection_funcs(prop,
                                    "rna_SequenceEditor_strips_all_begin",
                                    "rna_SequenceEditor_strips_all_next",
                                    "rna_SequenceEditor_strips_all_end",
                                    "rna_SequenceEditor_strips_all_get",
                                    nullptr,
                                    nullptr,
                                    "rna_SequenceEditor_strips_all_lookup_string",
                                    nullptr);

  prop = RNA_def_property(srna, "meta_stack", PROP_COLLECTION, PROP_NONE);
  RNA_def_property_collection_sdna(prop, nullptr, "metastack", nullptr);
  RNA_def_property_struct_type(prop, "Strip");
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
  RNA_def_property_pointer_sdna(prop, nullptr, "act_strip");
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

  prop = RNA_def_property(srna, "show_missing_media", PROP_BOOLEAN, PROP_NONE);
  RNA_def_property_boolean_sdna(
      prop, nullptr, "show_missing_media_flag", SEQ_EDIT_SHOW_MISSING_MEDIA);
  RNA_def_property_ui_text(
      prop, "Show Missing Media", "Render missing images/movies with a solid magenta color");
  RNA_def_property_update(prop, NC_SPACE | ND_SPACE_SEQUENCER, "rna_SequenceEditor_update_cache");

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
  RNA_def_property_flag(prop, PROP_PATH_SUPPORTS_BLEND_RELATIVE);
  RNA_def_property_update(prop, NC_SPACE | ND_SPACE_SEQUENCER, "rna_SequenceEditor_update_cache");

  /* cache flags */

  prop = RNA_def_property(srna, "use_cache_raw", PROP_BOOLEAN, PROP_NONE);
  RNA_def_property_boolean_sdna(prop, nullptr, "cache_flag", SEQ_CACHE_STORE_RAW);
  RNA_def_property_ui_text(prop,
                           "Cache Raw",
                           "Cache raw images read from disk, for faster tweaking of strip "
                           "parameters at the cost of memory usage");
  RNA_def_property_update(
      prop, NC_SPACE | ND_SPACE_SEQUENCER, "rna_SequenceEditor_cache_settings_changed");

  prop = RNA_def_property(srna, "use_cache_final", PROP_BOOLEAN, PROP_NONE);
  RNA_def_property_boolean_sdna(prop, nullptr, "cache_flag", SEQ_CACHE_STORE_FINAL_OUT);
  RNA_def_property_ui_text(prop, "Cache Final", "Cache final image for each frame");
  RNA_def_property_update(
      prop, NC_SPACE | ND_SPACE_SEQUENCER, "rna_SequenceEditor_cache_settings_changed");

  prop = RNA_def_property(srna, "use_prefetch", PROP_BOOLEAN, PROP_NONE);
  RNA_def_property_boolean_sdna(prop, nullptr, "cache_flag", SEQ_CACHE_PREFETCH_ENABLE);
  RNA_def_property_ui_text(
      prop,
      "Prefetch Frames",
      "Render frames ahead of current frame in the background for faster playback");
  RNA_def_property_update(prop, NC_SCENE | ND_SEQUENCER, nullptr);

  prop = RNA_def_property(srna, "cache_raw_size", PROP_INT, PROP_NONE);
  RNA_def_property_clear_flag(prop, PROP_EDITABLE | PROP_ANIMATABLE);
  RNA_def_property_int_funcs(prop, "rna_SequenceEditor_get_cache_raw_size", nullptr, nullptr);
  RNA_def_property_ui_text(prop, "Raw Cache Size", "Size of raw source images cache in megabytes");

  prop = RNA_def_property(srna, "cache_final_size", PROP_INT, PROP_NONE);
  RNA_def_property_clear_flag(prop, PROP_EDITABLE | PROP_ANIMATABLE);
  RNA_def_property_int_funcs(prop, "rna_SequenceEditor_get_cache_final_size", nullptr, nullptr);
  RNA_def_property_ui_text(
      prop, "Final Cache Size", "Size of final rendered images cache in megabytes");

  /* functions */

  func = RNA_def_function(srna, "display_stack", "rna_SequenceEditor_display_stack");
  RNA_def_function_flag(func, FUNC_USE_SELF_ID | FUNC_USE_REPORTS);
  RNA_def_function_ui_description(func, "Display strips stack");
  parm = RNA_def_pointer(
      func, "meta_sequence", "Strip", "Meta Strip", "Meta to display its stack");
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
  RNA_def_property_update(prop, NC_SCENE | ND_SEQUENCER, "rna_Strip_reopen_files_update");

  prop = RNA_def_property(srna, "alpha_mode", PROP_ENUM, PROP_NONE);
  RNA_def_property_enum_items(prop, alpha_mode_items);
  RNA_def_property_ui_text(
      prop, "Alpha Mode", "Representation of alpha information in the RGBA pixels");
  RNA_def_property_update(prop, NC_SCENE | ND_SEQUENCER, "rna_Strip_invalidate_raw_update");

  prop = RNA_def_property(srna, "use_flip_x", PROP_BOOLEAN, PROP_NONE);
  RNA_def_property_boolean_sdna(prop, nullptr, "flag", SEQ_FLIPX);
  RNA_def_property_ui_text(prop, "Flip X", "Flip on the X axis");
  RNA_def_property_update(prop, NC_SCENE | ND_SEQUENCER, "rna_Strip_invalidate_raw_update");

  prop = RNA_def_property(srna, "use_flip_y", PROP_BOOLEAN, PROP_NONE);
  RNA_def_property_boolean_sdna(prop, nullptr, "flag", SEQ_FLIPY);
  RNA_def_property_ui_text(prop, "Flip Y", "Flip on the Y axis");
  RNA_def_property_update(prop, NC_SCENE | ND_SEQUENCER, "rna_Strip_invalidate_raw_update");

  prop = RNA_def_property(srna, "use_float", PROP_BOOLEAN, PROP_NONE);
  RNA_def_property_boolean_sdna(prop, nullptr, "flag", SEQ_MAKE_FLOAT);
  RNA_def_property_ui_text(prop, "Convert Float", "Convert input to float data");
  RNA_def_property_update(prop, NC_SCENE | ND_SEQUENCER, "rna_Strip_invalidate_raw_update");

  prop = RNA_def_property(srna, "use_reverse_frames", PROP_BOOLEAN, PROP_NONE);
  RNA_def_property_boolean_sdna(prop, nullptr, "flag", SEQ_REVERSE_FRAMES);
  RNA_def_property_ui_text(prop, "Reverse Frames", "Reverse frame order");
  RNA_def_property_update(
      prop, NC_SCENE | ND_SEQUENCER, "rna_Strip_invalidate_preprocessed_update");

  prop = RNA_def_property(srna, "color_multiply", PROP_FLOAT, PROP_UNSIGNED);
  RNA_def_property_float_sdna(prop, nullptr, "mul");
  RNA_def_property_range(prop, 0.0f, 20.0f);
  RNA_def_property_float_default(prop, 1.0f);
  RNA_def_property_ui_text(prop, "Multiply Colors", "");
  RNA_def_property_update(
      prop, NC_SCENE | ND_SEQUENCER, "rna_Strip_invalidate_preprocessed_update");

  prop = RNA_def_property(srna, "multiply_alpha", PROP_BOOLEAN, PROP_NONE);
  RNA_def_property_boolean_sdna(prop, nullptr, "flag", SEQ_MULTIPLY_ALPHA);
  RNA_def_property_ui_text(prop, "Multiply Alpha", "Multiply alpha along with color channels");
  RNA_def_property_update(prop, NC_SCENE | ND_SEQUENCER, "rna_Strip_invalidate_raw_update");

  prop = RNA_def_property(srna, "color_saturation", PROP_FLOAT, PROP_UNSIGNED);
  RNA_def_property_float_sdna(prop, nullptr, "sat");
  RNA_def_property_range(prop, 0.0f, 20.0f);
  RNA_def_property_ui_range(prop, 0.0f, 2.0f, 3, 3);
  RNA_def_property_float_default(prop, 1.0f);
  RNA_def_property_ui_text(prop, "Saturation", "Adjust the intensity of the input's color");
  RNA_def_property_update(
      prop, NC_SCENE | ND_SEQUENCER, "rna_Strip_invalidate_preprocessed_update");

  prop = RNA_def_property(srna, "strobe", PROP_FLOAT, PROP_NONE);
  RNA_def_property_range(prop, 1.0f, 30.0f);
  RNA_def_property_ui_text(prop, "Strobe", "Only display every nth frame");
  RNA_def_property_update(
      prop, NC_SCENE | ND_SEQUENCER, "rna_Strip_invalidate_preprocessed_update");

  prop = RNA_def_property(srna, "transform", PROP_POINTER, PROP_NONE);
  RNA_def_property_pointer_sdna(prop, nullptr, "data->transform");
  RNA_def_property_ui_text(prop, "Transform", "");

  prop = RNA_def_property(srna, "crop", PROP_POINTER, PROP_NONE);
  RNA_def_property_pointer_sdna(prop, nullptr, "data->crop");
  RNA_def_property_ui_text(prop, "Crop", "");
}

static void rna_def_proxy(StructRNA *srna)
{
  PropertyRNA *prop;

  prop = RNA_def_property(srna, "use_proxy", PROP_BOOLEAN, PROP_NONE);
  RNA_def_property_boolean_sdna(prop, nullptr, "flag", SEQ_USE_PROXY);
  RNA_def_property_ui_text(
      prop, "Use Proxy / Timecode", "Use a preview proxy and/or time-code index for this strip");
  RNA_def_property_boolean_funcs(prop, nullptr, "rna_Strip_use_proxy_set");
  RNA_def_property_update(
      prop, NC_SCENE | ND_SEQUENCER, "rna_Strip_invalidate_preprocessed_update");

  prop = RNA_def_property(srna, "proxy", PROP_POINTER, PROP_NONE);
  RNA_def_property_pointer_sdna(prop, nullptr, "data->proxy");
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
                             "rna_Strip_anim_startofs_final_set",
                             "rna_Strip_anim_startofs_final_range"); /* overlap tests */
  RNA_def_property_ui_text(prop, "Animation Start Offset", "Animation start offset (trim start)");
  RNA_def_property_update(
      prop, NC_SCENE | ND_SEQUENCER, "rna_Strip_invalidate_preprocessed_update");

  prop = RNA_def_property(srna, "animation_offset_end", PROP_INT, PROP_UNSIGNED);
  RNA_def_property_int_sdna(prop, nullptr, "anim_endofs");
  RNA_def_property_clear_flag(prop, PROP_ANIMATABLE);
  RNA_def_property_int_funcs(prop,
                             nullptr,
                             "rna_Strip_anim_endofs_final_set",
                             "rna_Strip_anim_endofs_final_range"); /* overlap tests */
  RNA_def_property_ui_text(prop, "Animation End Offset", "Animation end offset (trim end)");
  RNA_def_property_update(
      prop, NC_SCENE | ND_SEQUENCER, "rna_Strip_invalidate_preprocessed_update");
}

static void rna_def_effect_inputs(StructRNA *srna, int count)
{
  PropertyRNA *prop;

  prop = RNA_def_property(srna, "input_count", PROP_INT, PROP_UNSIGNED);
  RNA_def_property_clear_flag(prop, PROP_EDITABLE);
  RNA_def_property_int_funcs(prop, "rna_Strip_input_count_get", nullptr, nullptr);

  if (count >= 1) {
    prop = RNA_def_property(srna, "input_1", PROP_POINTER, PROP_NONE);
    RNA_def_property_pointer_sdna(prop, nullptr, "input1");
    RNA_def_property_flag(prop, PROP_EDITABLE | PROP_NEVER_NULL);
    RNA_def_property_pointer_funcs(prop, nullptr, "rna_Strip_input_1_set", nullptr, nullptr);
    RNA_def_property_ui_text(prop, "Input 1", "First input for the effect strip");
  }

  if (count >= 2) {
    prop = RNA_def_property(srna, "input_2", PROP_POINTER, PROP_NONE);
    RNA_def_property_pointer_sdna(prop, nullptr, "input2");
    RNA_def_property_flag(prop, PROP_EDITABLE | PROP_NEVER_NULL);
    RNA_def_property_pointer_funcs(prop, nullptr, "rna_Strip_input_2_set", nullptr, nullptr);
    RNA_def_property_ui_text(prop, "Input 2", "Second input for the effect strip");
  }
}

static void rna_def_color_management(StructRNA *srna)
{
  PropertyRNA *prop;

  prop = RNA_def_property(srna, "colorspace_settings", PROP_POINTER, PROP_NONE);
  RNA_def_property_pointer_sdna(prop, nullptr, "data->colorspace_settings");
  RNA_def_property_struct_type(prop, "ColorManagedInputColorspaceSettings");
  RNA_def_property_ui_text(prop, "Color Space Settings", "Input color space settings");
}

static void rna_def_movie_types(StructRNA *srna)
{
  PropertyRNA *prop;

  prop = RNA_def_property(srna, "fps", PROP_FLOAT, PROP_NONE);
  RNA_def_property_ui_text(prop, "FPS", "Frames per second");
  RNA_def_property_clear_flag(prop, PROP_EDITABLE);
  RNA_def_property_float_funcs(prop, "rna_Strip_fps_get", nullptr, nullptr);
}

static void rna_def_retiming_keys(StructRNA *srna)
{
  PropertyRNA *prop = RNA_def_property(srna, "retiming_keys", PROP_COLLECTION, PROP_NONE);
  RNA_def_property_collection_sdna(prop, nullptr, "retiming_keys", nullptr);
  RNA_def_property_struct_type(prop, "RetimingKey");
  RNA_def_property_ui_text(prop, "Retiming Keys", "");
  RNA_def_property_collection_funcs(prop,
                                    "rna_Strip_retiming_keys_begin",
                                    "rna_iterator_array_next",
                                    "rna_iterator_array_end",
                                    "rna_iterator_array_get",
                                    "rna_Strip_retiming_keys_length",
                                    nullptr,
                                    nullptr,
                                    nullptr);
  RNA_def_property_srna(prop, "RetimingKeys");
}

static void rna_def_image(BlenderRNA *brna)
{
  StructRNA *srna;
  PropertyRNA *prop;

  srna = RNA_def_struct(brna, "ImageStrip", "Strip");
  RNA_def_struct_ui_text(srna, "Image Strip", "Sequence strip to load one or more images");
  RNA_def_struct_sdna(srna, "Strip");

  prop = RNA_def_property(srna, "directory", PROP_STRING, PROP_DIRPATH);
  RNA_def_property_string_sdna(prop, nullptr, "data->dirpath");
  RNA_def_property_ui_text(prop, "Directory", "");
  RNA_def_property_flag(prop, PROP_PATH_SUPPORTS_BLEND_RELATIVE);
  RNA_def_property_update(prop, NC_SCENE | ND_SEQUENCER, "rna_Strip_invalidate_raw_update");

  prop = RNA_def_property(srna, "elements", PROP_COLLECTION, PROP_NONE);
  RNA_def_property_collection_sdna(prop, nullptr, "data->stripdata", nullptr);
  RNA_def_property_struct_type(prop, "StripElement");
  RNA_def_property_ui_text(prop, "Elements", "");
  RNA_def_property_collection_funcs(prop,
                                    "rna_Strip_elements_begin",
                                    "rna_iterator_array_next",
                                    "rna_iterator_array_end",
                                    "rna_iterator_array_get",
                                    "rna_Strip_elements_length",
                                    nullptr,
                                    nullptr,
                                    nullptr);
  RNA_api_strip_elements(brna, prop);

  rna_def_retiming_keys(srna);

  /* multiview */
  prop = RNA_def_property(srna, "use_multiview", PROP_BOOLEAN, PROP_NONE);
  RNA_def_property_boolean_sdna(prop, nullptr, "flag", SEQ_USE_VIEWS);
  RNA_def_property_ui_text(prop, "Use Multi-View", "Use Multiple Views (when available)");
  RNA_def_property_update(prop, NC_SCENE | ND_SEQUENCER, "rna_Strip_views_format_update");

  prop = RNA_def_property(srna, "views_format", PROP_ENUM, PROP_NONE);
  RNA_def_property_enum_sdna(prop, nullptr, "views_format");
  RNA_def_property_enum_items(prop, rna_enum_views_format_items);
  RNA_def_property_ui_text(prop, "Views Format", "Mode to load image views");
  RNA_def_property_update(prop, NC_IMAGE | ND_DISPLAY, "rna_Strip_views_format_update");

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

static void rna_def_strips_meta(BlenderRNA *brna)
{
  StructRNA *srna;

  srna = RNA_def_struct(brna, "StripsMeta", nullptr);
  RNA_def_struct_sdna(srna, "Strip");
  RNA_def_struct_ui_text(srna, "Strips", "Collection of Strips");

  RNA_api_strips(srna, true);
}

static void rna_def_meta(BlenderRNA *brna)
{
  StructRNA *srna;
  FunctionRNA *func;
  PropertyRNA *prop;

  srna = RNA_def_struct(brna, "MetaStrip", "Strip");
  RNA_def_struct_ui_text(
      srna, "Meta Strip", "Sequence strip to group other strips as a single sequence strip");
  RNA_def_struct_sdna(srna, "Strip");

  rna_def_strips_meta(brna);

  prop = RNA_def_property(srna, "strips", PROP_COLLECTION, PROP_NONE);
  RNA_def_property_srna(prop, "StripsMeta");
  RNA_def_property_collection_sdna(prop, nullptr, "seqbase", nullptr);
  RNA_def_property_struct_type(prop, "Strip");
  RNA_def_property_ui_text(prop, "Strips", "Strips nested in meta strip");

  prop = RNA_def_property(srna, "channels", PROP_COLLECTION, PROP_NONE);
  RNA_def_property_collection_sdna(prop, nullptr, "channels", nullptr);
  RNA_def_property_struct_type(prop, "SequenceTimelineChannel");
  RNA_def_property_ui_text(prop, "Channels", "");

  func = RNA_def_function(srna, "separate", "rna_Strip_separate");
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
  RNA_def_property_float_default(prop, 1.0f);
  RNA_def_property_ui_text(prop, "Volume", "Playback volume of the sound");
  RNA_def_property_translation_context(prop, BLT_I18NCONTEXT_ID_SOUND);
  RNA_def_property_update(prop, NC_SCENE | ND_SEQUENCER, "rna_Strip_audio_update");
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

  srna = RNA_def_struct(brna, "SceneStrip", "Strip");
  RNA_def_struct_ui_text(
      srna, "Scene Strip", "Sequence strip using the rendered image of a scene");
  RNA_def_struct_sdna(srna, "Strip");

  prop = RNA_def_property(srna, "scene", PROP_POINTER, PROP_NONE);
  RNA_def_property_flag(prop, PROP_EDITABLE | PROP_ID_SELF_CHECK);
  RNA_def_property_ui_text(prop, "Scene", "Scene that this strip uses");
  RNA_def_property_update(prop, NC_SCENE | ND_SEQUENCER, "rna_Strip_scene_switch_update");

  prop = RNA_def_property(srna, "scene_camera", PROP_POINTER, PROP_NONE);
  RNA_def_property_flag(prop, PROP_EDITABLE);
  RNA_def_property_pointer_funcs(prop, nullptr, nullptr, nullptr, "rna_Camera_object_poll");
  RNA_def_property_ui_text(prop, "Camera Override", "Override the scene's active camera");
  RNA_def_property_update(prop, NC_SCENE | ND_SEQUENCER, "rna_Strip_invalidate_raw_update");

  prop = RNA_def_property(srna, "scene_input", PROP_ENUM, PROP_NONE);
  RNA_def_property_enum_bitflag_sdna(prop, nullptr, "flag");
  RNA_def_property_enum_items(prop, scene_input_items);
  RNA_def_property_ui_text(prop, "Input", "Input type to use for the Scene strip");
  RNA_def_property_update(prop, NC_SCENE | ND_SEQUENCER, "rna_Strip_use_strip");

  prop = RNA_def_property(srna, "use_annotations", PROP_BOOLEAN, PROP_NONE);
  RNA_def_property_boolean_negative_sdna(prop, nullptr, "flag", SEQ_SCENE_NO_ANNOTATION);
  RNA_def_property_ui_text(prop, "Use Annotations", "Show Annotations in OpenGL previews");
  RNA_def_property_update(prop, NC_SCENE | ND_SEQUENCER, "rna_Strip_invalidate_raw_update");

  rna_def_retiming_keys(srna);
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

  srna = RNA_def_struct(brna, "MovieStrip", "Strip");
  RNA_def_struct_ui_text(srna, "Movie Strip", "Sequence strip to load a video");
  RNA_def_struct_sdna(srna, "Strip");

  prop = RNA_def_property(srna, "stream_index", PROP_INT, PROP_NONE);
  RNA_def_property_int_sdna(prop, nullptr, "streamindex");
  RNA_def_property_range(prop, 0, 20);
  RNA_def_property_ui_text(
      prop,
      "Stream Index",
      "For files with several movie streams, use the stream with the given index");
  RNA_def_property_update(prop, NC_SCENE | ND_SEQUENCER, "rna_Strip_reopen_files_update");

  prop = RNA_def_property(srna, "elements", PROP_COLLECTION, PROP_NONE);
  RNA_def_property_collection_sdna(prop, nullptr, "data->stripdata", nullptr);
  RNA_def_property_struct_type(prop, "StripElement");
  RNA_def_property_ui_text(prop, "Elements", "");
  RNA_def_property_collection_funcs(prop,
                                    "rna_Strip_elements_begin",
                                    "rna_iterator_array_next",
                                    "rna_iterator_array_end",
                                    "rna_iterator_array_get",
                                    "rna_Strip_elements_length",
                                    nullptr,
                                    nullptr,
                                    nullptr);

  rna_def_retiming_keys(srna);

  prop = RNA_def_property(srna, "filepath", PROP_STRING, PROP_FILEPATH);
  RNA_def_property_ui_text(prop, "File", "");
  RNA_def_property_string_funcs(
      prop, "rna_Strip_filepath_get", "rna_Strip_filepath_length", "rna_Strip_filepath_set");
  RNA_def_property_flag(prop, PROP_PATH_SUPPORTS_BLEND_RELATIVE);
  RNA_def_property_update(prop, NC_SCENE | ND_SEQUENCER, "rna_Strip_filepath_update");

  func = RNA_def_function(srna, "reload_if_needed", "rna_MovieStrip_reload_if_needed");
  RNA_def_function_flag(func, FUNC_USE_SELF_ID | FUNC_USE_MAIN);
  /* return type */
  parm = RNA_def_boolean(func,
                         "can_produce_frames",
                         false,
                         "True if the strip can produce frames, False otherwise",
                         "");
  RNA_def_function_return(func, parm);

  /* metadata */
  func = RNA_def_function(srna, "metadata", "rna_MovieStrip_metadata_get");
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
  RNA_def_property_update(prop, NC_SCENE | ND_SEQUENCER, "rna_Strip_views_format_update");

  prop = RNA_def_property(srna, "views_format", PROP_ENUM, PROP_NONE);
  RNA_def_property_enum_sdna(prop, nullptr, "views_format");
  RNA_def_property_enum_items(prop, rna_enum_views_format_items);
  RNA_def_property_ui_text(prop, "Views Format", "Mode to load movie views");
  RNA_def_property_update(prop, NC_IMAGE | ND_DISPLAY, "rna_Strip_views_format_update");

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

  srna = RNA_def_struct(brna, "MovieClipStrip", "Strip");
  RNA_def_struct_ui_text(
      srna, "MovieClip Strip", "Sequence strip to load a video from the clip editor");
  RNA_def_struct_sdna(srna, "Strip");

  prop = RNA_def_property(srna, "clip", PROP_POINTER, PROP_NONE);
  RNA_def_property_flag(prop, PROP_EDITABLE);
  RNA_def_property_ui_text(prop, "Movie Clip", "Movie clip that this strip uses");
  RNA_def_property_update(prop, NC_SCENE | ND_SEQUENCER, "rna_Strip_invalidate_raw_update");

  prop = RNA_def_property(srna, "undistort", PROP_BOOLEAN, PROP_NONE);
  RNA_def_property_boolean_sdna(prop, nullptr, "clip_flag", SEQ_MOVIECLIP_RENDER_UNDISTORTED);
  RNA_def_property_ui_text(prop, "Undistort Clip", "Use the undistorted version of the clip");
  RNA_def_property_update(prop, NC_SCENE | ND_SEQUENCER, "rna_Strip_invalidate_raw_update");

  prop = RNA_def_property(srna, "stabilize2d", PROP_BOOLEAN, PROP_NONE);
  RNA_def_property_boolean_sdna(prop, nullptr, "clip_flag", SEQ_MOVIECLIP_RENDER_STABILIZED);
  RNA_def_property_ui_text(prop, "Stabilize 2D Clip", "Use the 2D stabilized version of the clip");
  RNA_def_property_update(prop, NC_SCENE | ND_SEQUENCER, "rna_Strip_invalidate_raw_update");

  rna_def_filter_video(srna);
  rna_def_input(srna);
  rna_def_movie_types(srna);
}

static void rna_def_mask(BlenderRNA *brna)
{
  StructRNA *srna;
  PropertyRNA *prop;

  srna = RNA_def_struct(brna, "MaskStrip", "Strip");
  RNA_def_struct_ui_text(srna, "Mask Strip", "Sequence strip to load a video from a mask");
  RNA_def_struct_sdna(srna, "Strip");

  prop = RNA_def_property(srna, "mask", PROP_POINTER, PROP_NONE);
  RNA_def_property_flag(prop, PROP_EDITABLE);
  RNA_def_property_ui_text(prop, "Mask", "Mask that this strip uses");
  RNA_def_property_update(prop, NC_SCENE | ND_SEQUENCER, "rna_Strip_invalidate_raw_update");

  rna_def_filter_video(srna);
  rna_def_input(srna);
}

static void rna_def_sound(BlenderRNA *brna)
{
  StructRNA *srna;
  PropertyRNA *prop;

  srna = RNA_def_struct(brna, "SoundStrip", "Strip");
  RNA_def_struct_ui_text(
      srna, "Sound Strip", "Sequence strip defining a sound to be played over a period of time");
  RNA_def_struct_sdna(srna, "Strip");

  prop = RNA_def_property(srna, "sound", PROP_POINTER, PROP_NONE);
  RNA_def_property_flag(prop, PROP_EDITABLE);
  RNA_def_property_struct_type(prop, "Sound");
  RNA_def_property_ui_text(prop, "Sound", "Sound data-block used by this strip");
  RNA_def_property_update(prop, NC_SCENE | ND_SEQUENCER, "rna_Strip_sound_update");

  rna_def_audio_options(srna);

  prop = RNA_def_property(srna, "pan", PROP_FLOAT, PROP_NONE);
  RNA_def_property_float_sdna(prop, nullptr, "pan");
  RNA_def_property_range(prop, -FLT_MAX, FLT_MAX);
  RNA_def_property_ui_range(prop, -2, 2, 1, 2);
  RNA_def_property_ui_text(prop, "Pan", "Playback panning of the sound (only for Mono sources)");
  RNA_def_property_translation_context(prop, BLT_I18NCONTEXT_ID_SOUND);
  RNA_def_property_float_funcs(prop, nullptr, nullptr, "rna_Strip_pan_range");
  RNA_def_property_update(prop, NC_SCENE | ND_SEQUENCER, "rna_Strip_audio_update");

  prop = RNA_def_property(srna, "sound_offset", PROP_FLOAT, PROP_TIME_ABSOLUTE);
  RNA_def_property_float_sdna(prop, nullptr, "sound_offset");
  RNA_def_property_range(prop, -FLT_MAX, FLT_MAX);
  RNA_def_property_ui_range(prop, -FLT_MAX, FLT_MAX, 1, 3);
  RNA_def_property_clear_flag(prop, PROP_ANIMATABLE); /* not meant to be animated */
  RNA_def_property_ui_text(
      prop,
      "Sound Offset",
      "Offset of the sound from the beginning of the strip, expressed in seconds");
  RNA_def_property_translation_context(prop, BLT_I18NCONTEXT_ID_SOUND);
  RNA_def_property_update(prop, NC_SCENE | ND_SEQUENCER, "rna_Strip_audio_update");

  prop = RNA_def_property(srna, "show_waveform", PROP_BOOLEAN, PROP_NONE);
  RNA_def_property_boolean_sdna(prop, nullptr, "flag", SEQ_AUDIO_DRAW_WAVEFORM);
  RNA_def_property_ui_text(
      prop, "Display Waveform", "Display the audio waveform inside the strip");
  RNA_def_property_update(prop, NC_SCENE | ND_SEQUENCER, nullptr);

  prop = RNA_def_property(srna, "pitch_correction", PROP_BOOLEAN, PROP_NONE);
  RNA_def_property_boolean_sdna(prop, nullptr, "flag", SEQ_AUDIO_PITCH_CORRECTION);
  RNA_def_property_ui_text(
      prop,
      "Preserve Pitch",
      "Maintain the original pitch of the audio when changing playback speed");
  RNA_def_property_update(prop, NC_SCENE | ND_SEQUENCER, "rna_Strip_sound_update");
  rna_def_retiming_keys(srna);
  rna_def_input(srna);
}

static void rna_def_effect(BlenderRNA *brna)
{
  StructRNA *srna;

  srna = RNA_def_struct(brna, "EffectStrip", "Strip");
  RNA_def_struct_ui_text(
      srna,
      "Effect Strip",
      "Sequence strip applying an effect on the images created by other strips");
  RNA_def_struct_sdna(srna, "Strip");

  rna_def_filter_video(srna);
  rna_def_proxy(srna);
}

static void rna_def_multicam(StructRNA *srna)
{
  PropertyRNA *prop;

  prop = RNA_def_property(srna, "multicam_source", PROP_INT, PROP_UNSIGNED);
  RNA_def_property_int_sdna(prop, nullptr, "multicam_source");
  RNA_def_property_range(prop, 0, blender::seq::MAX_CHANNELS - 1);
  RNA_def_property_ui_text(prop, "Multicam Source Channel", "");
  RNA_def_property_update(prop, NC_SCENE | ND_SEQUENCER, "rna_Strip_invalidate_raw_update");

  rna_def_input(srna);
}

static void rna_def_wipe(StructRNA *srna)
{
  PropertyRNA *prop;

  static const EnumPropertyItem wipe_type_items[] = {
      {SEQ_WIPE_SINGLE, "SINGLE", 0, "Single", ""},
      {SEQ_WIPE_DOUBLE, "DOUBLE", 0, "Double", ""},
      /* not used yet {SEQ_WIPE_BOX, "BOX", 0, "Box", ""}, */
      /* not used yet {SEQ_WIPE_CROSS, "CROSS", 0, "Cross", ""}, */
      {SEQ_WIPE_IRIS, "IRIS", 0, "Iris", ""},
      {SEQ_WIPE_CLOCK, "CLOCK", 0, "Clock", ""},
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
  RNA_def_property_update(prop, NC_SCENE | ND_SEQUENCER, "rna_Strip_invalidate_raw_update");

  prop = RNA_def_property(srna, "angle", PROP_FLOAT, PROP_ANGLE);
  RNA_def_property_range(prop, DEG2RADF(-90.0f), DEG2RADF(90.0f));
  RNA_def_property_ui_text(prop, "Angle", "Angle of the transition");
  RNA_def_property_update(prop, NC_SCENE | ND_SEQUENCER, "rna_Strip_invalidate_raw_update");

  prop = RNA_def_property(srna, "direction", PROP_ENUM, PROP_NONE);
  RNA_def_property_enum_sdna(prop, nullptr, "forward");
  RNA_def_property_enum_items(prop, wipe_direction_items);
  RNA_def_property_ui_text(prop, "Direction", "Whether to fade in or out");
  RNA_def_property_translation_context(prop, BLT_I18NCONTEXT_ID_SEQUENCE);
  RNA_def_property_update(prop, NC_SCENE | ND_SEQUENCER, "rna_Strip_invalidate_raw_update");

  prop = RNA_def_property(srna, "transition_type", PROP_ENUM, PROP_NONE);
  RNA_def_property_enum_sdna(prop, nullptr, "wipetype");
  RNA_def_property_enum_items(prop, wipe_type_items);
  RNA_def_property_translation_context(prop, BLT_I18NCONTEXT_ID_SEQUENCE);
  RNA_def_property_ui_text(prop, "Transition Type", "");
  RNA_def_property_update(prop, NC_SCENE | ND_SEQUENCER, "rna_Strip_invalidate_raw_update");
}

static void rna_def_glow(StructRNA *srna)
{
  PropertyRNA *prop;

  RNA_def_struct_sdna_from(srna, "GlowVars", "effectdata");

  prop = RNA_def_property(srna, "threshold", PROP_FLOAT, PROP_FACTOR);
  RNA_def_property_float_sdna(prop, nullptr, "fMini");
  RNA_def_property_range(prop, 0.0f, 1.0f);
  RNA_def_property_ui_text(prop, "Threshold", "Minimum intensity to trigger a glow");
  RNA_def_property_update(prop, NC_SCENE | ND_SEQUENCER, "rna_Strip_invalidate_raw_update");

  prop = RNA_def_property(srna, "clamp", PROP_FLOAT, PROP_FACTOR);
  RNA_def_property_float_sdna(prop, nullptr, "fClamp");
  RNA_def_property_range(prop, 0.0f, 1.0f);
  RNA_def_property_ui_text(prop, "Clamp", "Brightness limit of intensity");
  RNA_def_property_update(prop, NC_SCENE | ND_SEQUENCER, "rna_Strip_invalidate_raw_update");

  prop = RNA_def_property(srna, "boost_factor", PROP_FLOAT, PROP_NONE);
  RNA_def_property_float_sdna(prop, nullptr, "fBoost");
  RNA_def_property_range(prop, 0.0f, 10.0f);
  RNA_def_property_ui_text(prop, "Boost Factor", "Brightness multiplier");
  RNA_def_property_update(prop, NC_SCENE | ND_SEQUENCER, "rna_Strip_invalidate_raw_update");

  prop = RNA_def_property(srna, "blur_radius", PROP_FLOAT, PROP_NONE);
  RNA_def_property_float_sdna(prop, nullptr, "dDist");
  RNA_def_property_range(prop, 0.5f, 20.0f);
  RNA_def_property_ui_text(prop, "Blur Distance", "Radius of glow effect");
  RNA_def_property_update(prop, NC_SCENE | ND_SEQUENCER, "rna_Strip_invalidate_raw_update");

  prop = RNA_def_property(srna, "quality", PROP_INT, PROP_NONE);
  RNA_def_property_int_sdna(prop, nullptr, "dQuality");
  RNA_def_property_range(prop, 1, 5);
  RNA_def_property_ui_text(prop, "Quality", "Accuracy of the blur effect");
  RNA_def_property_update(prop, NC_SCENE | ND_SEQUENCER, "rna_Strip_invalidate_raw_update");

  prop = RNA_def_property(srna, "use_only_boost", PROP_BOOLEAN, PROP_NONE);
  RNA_def_property_boolean_sdna(prop, nullptr, "bNoComp", 0);
  RNA_def_property_ui_text(prop, "Only Boost", "Show the glow buffer only");
  RNA_def_property_update(prop, NC_SCENE | ND_SEQUENCER, "rna_Strip_invalidate_raw_update");
}

static void rna_def_solid_color(StructRNA *srna)
{
  PropertyRNA *prop;

  RNA_def_struct_sdna_from(srna, "SolidColorVars", "effectdata");

  prop = RNA_def_property(srna, "color", PROP_FLOAT, PROP_COLOR_GAMMA);
  RNA_def_property_float_sdna(prop, nullptr, "col");
  RNA_def_property_ui_text(prop, "Color", "Effect Strip color");
  RNA_def_property_update(prop, NC_SCENE | ND_SEQUENCER, "rna_Strip_invalidate_raw_update");
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
  RNA_def_property_update(prop, NC_SCENE | ND_SEQUENCER, "rna_Strip_invalidate_raw_update");

  prop = RNA_def_property(srna, "speed_factor", PROP_FLOAT, PROP_NONE);
  RNA_def_property_float_sdna(prop, nullptr, "speed_fader");
  RNA_def_property_ui_text(
      prop,
      "Multiply Factor",
      "Multiply the current speed of the strip with this number or remap current frame "
      "to this frame");
  RNA_def_property_update(prop, NC_SCENE | ND_SEQUENCER, "rna_Strip_invalidate_raw_update");

  prop = RNA_def_property(srna, "speed_frame_number", PROP_FLOAT, PROP_NONE);
  RNA_def_property_float_sdna(prop, nullptr, "speed_fader_frame_number");
  RNA_def_property_ui_text(prop, "Frame Number", "Frame number of input strip");
  RNA_def_property_ui_range(prop, 0.0, MAXFRAME, 1.0, -1);
  RNA_def_property_update(prop, NC_SCENE | ND_SEQUENCER, "rna_Strip_invalidate_raw_update");

  prop = RNA_def_property(srna, "speed_length", PROP_FLOAT, PROP_PERCENTAGE);
  RNA_def_property_float_sdna(prop, nullptr, "speed_fader_length");
  RNA_def_property_ui_text(prop, "Length", "Percentage of input strip length");
  RNA_def_property_ui_range(prop, 0.0, 100.0, 1, -1);
  RNA_def_property_update(prop, NC_SCENE | ND_SEQUENCER, "rna_Strip_invalidate_raw_update");

  prop = RNA_def_property(srna, "use_frame_interpolate", PROP_BOOLEAN, PROP_NONE);
  RNA_def_property_boolean_sdna(prop, nullptr, "flags", SEQ_SPEED_USE_INTERPOLATION);
  RNA_def_property_ui_text(
      prop, "Frame Interpolation", "Do crossfade blending between current and next frame");
  RNA_def_property_update(prop, NC_SCENE | ND_SEQUENCER, "rna_Strip_invalidate_raw_update");
}

static void rna_def_gaussian_blur(StructRNA *srna)
{
  PropertyRNA *prop;

  RNA_def_struct_sdna_from(srna, "GaussianBlurVars", "effectdata");
  prop = RNA_def_property(srna, "size_x", PROP_FLOAT, PROP_UNSIGNED);
  RNA_def_property_ui_text(prop, "Size X", "Size of the blur along X axis");
  RNA_def_property_ui_range(prop, 0.0f, FLT_MAX, 1, -1);
  RNA_def_property_update(prop, NC_SCENE | ND_SEQUENCER, "rna_Strip_invalidate_raw_update");

  prop = RNA_def_property(srna, "size_y", PROP_FLOAT, PROP_UNSIGNED);
  RNA_def_property_ui_text(prop, "Size Y", "Size of the blur along Y axis");
  RNA_def_property_ui_range(prop, 0.0f, FLT_MAX, 1, -1);
  RNA_def_property_update(prop, NC_SCENE | ND_SEQUENCER, "rna_Strip_invalidate_raw_update");
}

static void rna_def_text(StructRNA *srna)
{
  static const EnumPropertyItem text_alignment_x_items[] = {
      {SEQ_TEXT_ALIGN_X_LEFT, "LEFT", ICON_ALIGN_LEFT, "Left", ""},
      {SEQ_TEXT_ALIGN_X_CENTER, "CENTER", ICON_ALIGN_CENTER, "Center", ""},
      {SEQ_TEXT_ALIGN_X_RIGHT, "RIGHT", ICON_ALIGN_RIGHT, "Right", ""},
      {0, nullptr, 0, nullptr, nullptr},
  };

  static const EnumPropertyItem text_anchor_x_items[] = {
      {SEQ_TEXT_ALIGN_X_LEFT, "LEFT", ICON_ANCHOR_LEFT, "Left", ""},
      {SEQ_TEXT_ALIGN_X_CENTER, "CENTER", ICON_ANCHOR_CENTER, "Center", ""},
      {SEQ_TEXT_ALIGN_X_RIGHT, "RIGHT", ICON_ANCHOR_RIGHT, "Right", ""},
      {0, nullptr, 0, nullptr, nullptr},
  };

  static const EnumPropertyItem text_anchor_y_items[] = {
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
  RNA_def_property_ui_text(
      prop, "Font", "Font of the text. Falls back to the UI font by default.");
  RNA_def_property_flag(prop, PROP_EDITABLE);
  RNA_def_property_pointer_funcs(prop, nullptr, "rna_Strip_text_font_set", nullptr, nullptr);
  RNA_def_property_update(prop, NC_SCENE | ND_SEQUENCER, "rna_Strip_invalidate_raw_update");

  prop = RNA_def_property(srna, "font_size", PROP_FLOAT, PROP_UNSIGNED);
  RNA_def_property_float_sdna(prop, nullptr, "text_size");
  RNA_def_property_ui_text(prop, "Size", "Size of the text");
  RNA_def_property_range(prop, 0.0, 2000);
  RNA_def_property_ui_range(prop, 0.0f, 2000, 10.0f, 1);
  RNA_def_property_update(prop, NC_SCENE | ND_SEQUENCER, "rna_Strip_invalidate_raw_update");

  prop = RNA_def_property(srna, "color", PROP_FLOAT, PROP_COLOR_GAMMA);
  RNA_def_property_float_sdna(prop, nullptr, "color");
  RNA_def_property_ui_text(prop, "Color", "Text color");
  RNA_def_property_update(prop, NC_SCENE | ND_SEQUENCER, "rna_Strip_invalidate_raw_update");

  prop = RNA_def_property(srna, "shadow_color", PROP_FLOAT, PROP_COLOR_GAMMA);
  RNA_def_property_float_sdna(prop, nullptr, "shadow_color");
  RNA_def_property_ui_text(prop, "Shadow Color", "");
  RNA_def_property_update(prop, NC_SCENE | ND_SEQUENCER, "rna_Strip_invalidate_raw_update");

  prop = RNA_def_property(srna, "shadow_angle", PROP_FLOAT, PROP_ANGLE);
  RNA_def_property_float_sdna(prop, nullptr, "shadow_angle");
  RNA_def_property_range(prop, 0, M_PI * 2);
  RNA_def_property_ui_text(prop, "Shadow Angle", "");
  RNA_def_property_float_default(prop, DEG2RADF(65.0f));
  RNA_def_property_update(prop, NC_SCENE | ND_SEQUENCER, "rna_Strip_invalidate_raw_update");

  prop = RNA_def_property(srna, "shadow_offset", PROP_FLOAT, PROP_UNSIGNED);
  RNA_def_property_float_sdna(prop, nullptr, "shadow_offset");
  RNA_def_property_ui_text(prop, "Shadow Offset", "");
  RNA_def_property_float_default(prop, 0.04f);
  RNA_def_property_range(prop, 0.0f, 1.0f);
  RNA_def_property_ui_range(prop, 0.0f, 1.0f, 1.0f, 2);
  RNA_def_property_update(prop, NC_SCENE | ND_SEQUENCER, "rna_Strip_invalidate_raw_update");

  prop = RNA_def_property(srna, "shadow_blur", PROP_FLOAT, PROP_UNSIGNED);
  RNA_def_property_float_sdna(prop, nullptr, "shadow_blur");
  RNA_def_property_ui_text(prop, "Shadow Blur", "");
  RNA_def_property_float_default(prop, 0.0f);
  RNA_def_property_range(prop, 0.0f, 1.0f);
  RNA_def_property_ui_range(prop, 0.0f, 1.0f, 1.0f, 2);
  RNA_def_property_update(prop, NC_SCENE | ND_SEQUENCER, "rna_Strip_invalidate_raw_update");

  prop = RNA_def_property(srna, "outline_color", PROP_FLOAT, PROP_COLOR_GAMMA);
  RNA_def_property_float_sdna(prop, nullptr, "outline_color");
  RNA_def_property_ui_text(prop, "Outline Color", "");
  RNA_def_property_update(prop, NC_SCENE | ND_SEQUENCER, "rna_Strip_invalidate_raw_update");

  prop = RNA_def_property(srna, "outline_width", PROP_FLOAT, PROP_UNSIGNED);
  RNA_def_property_float_sdna(prop, nullptr, "outline_width");
  RNA_def_property_ui_text(prop, "Outline Width", "");
  RNA_def_property_float_default(prop, 0.05f);
  RNA_def_property_range(prop, 0.0f, 1.0f);
  RNA_def_property_ui_range(prop, 0.0f, 1.0f, 1.0f, 2);
  RNA_def_property_update(prop, NC_SCENE | ND_SEQUENCER, "rna_Strip_invalidate_raw_update");

  prop = RNA_def_property(srna, "box_color", PROP_FLOAT, PROP_COLOR_GAMMA);
  RNA_def_property_float_sdna(prop, nullptr, "box_color");
  RNA_def_property_ui_text(prop, "Box Color", "");
  RNA_def_property_update(prop, NC_SCENE | ND_SEQUENCER, "rna_Strip_invalidate_raw_update");

  prop = RNA_def_property(srna, "location", PROP_FLOAT, PROP_XYZ);
  RNA_def_property_float_sdna(prop, nullptr, "loc");
  RNA_def_property_ui_text(prop, "Location", "Location of the text");
  RNA_def_property_range(prop, -FLT_MAX, FLT_MAX);
  RNA_def_property_ui_range(prop, -10.0, 10.0, 1, -1);
  RNA_def_property_update(prop, NC_SCENE | ND_SEQUENCER, "rna_Strip_invalidate_raw_update");

  prop = RNA_def_property(srna, "wrap_width", PROP_FLOAT, PROP_NONE);
  RNA_def_property_float_sdna(prop, nullptr, "wrap_width");
  RNA_def_property_ui_text(prop, "Wrap Width", "Word wrap width as factor, zero disables");
  RNA_def_property_range(prop, 0, FLT_MAX);
  RNA_def_property_ui_range(prop, 0.0, 1.0, 1, -1);
  RNA_def_property_update(prop, NC_SCENE | ND_SEQUENCER, "rna_Strip_invalidate_raw_update");

  prop = RNA_def_property(srna, "box_margin", PROP_FLOAT, PROP_NONE);
  RNA_def_property_float_sdna(prop, nullptr, "box_margin");
  RNA_def_property_ui_text(prop, "Box Margin", "Box margin as factor of image width");
  RNA_def_property_range(prop, 0, 1.0);
  RNA_def_property_ui_range(prop, 0.0, 1.0, 1, -1);
  RNA_def_property_float_default(prop, 0.01f);
  RNA_def_property_update(prop, NC_SCENE | ND_SEQUENCER, "rna_Strip_invalidate_raw_update");

  prop = RNA_def_property(srna, "box_roundness", PROP_FLOAT, PROP_NONE);
  RNA_def_property_float_sdna(prop, nullptr, "box_roundness");
  RNA_def_property_ui_text(prop, "Box Roundness", "Box corner radius as a factor of box height");
  RNA_def_property_range(prop, 0, 1.0);
  RNA_def_property_update(prop, NC_SCENE | ND_SEQUENCER, "rna_Strip_invalidate_raw_update");

  prop = RNA_def_property(srna, "alignment_x", PROP_ENUM, PROP_NONE);
  RNA_def_property_enum_sdna(prop, nullptr, "align");
  RNA_def_property_enum_items(prop, text_alignment_x_items);
  RNA_def_property_ui_text(prop, "Align X", "Horizontal text alignment");
  RNA_def_property_update(prop, NC_SCENE | ND_SEQUENCER, "rna_Strip_invalidate_raw_update");

  prop = RNA_def_property(srna, "anchor_x", PROP_ENUM, PROP_NONE);
  RNA_def_property_enum_sdna(prop, nullptr, "anchor_x");
  RNA_def_property_enum_items(prop, text_anchor_x_items);
  RNA_def_property_ui_text(
      prop, "Anchor X", "Horizontal position of the text box relative to Location");
  RNA_def_property_update(prop, NC_SCENE | ND_SEQUENCER, "rna_Strip_invalidate_raw_update");

  prop = RNA_def_property(srna, "anchor_y", PROP_ENUM, PROP_NONE);
  RNA_def_property_enum_sdna(prop, nullptr, "anchor_y");
  RNA_def_property_enum_items(prop, text_anchor_y_items);
  RNA_def_property_ui_text(
      prop, "Anchor Y", "Vertical position of the text box relative to Location");
  RNA_def_property_update(prop, NC_SCENE | ND_SEQUENCER, "rna_Strip_invalidate_raw_update");

  prop = RNA_def_property(srna, "text", PROP_STRING, PROP_NONE);
  RNA_def_property_string_sdna(prop, nullptr, "text_ptr");
  RNA_def_property_string_funcs(
      prop, "rna_Strip_text_get", "rna_Strip_text_length", "rna_Strip_text_set");
  RNA_def_property_ui_text(prop, "Text", "Text that will be displayed");
  RNA_def_property_flag(prop, PROP_TEXTEDIT_UPDATE);
  RNA_def_property_update(prop, NC_SCENE | ND_SEQUENCER, "rna_Strip_invalidate_raw_update");

  prop = RNA_def_property(srna, "use_shadow", PROP_BOOLEAN, PROP_NONE);
  RNA_def_property_boolean_sdna(prop, nullptr, "flag", SEQ_TEXT_SHADOW);
  RNA_def_property_ui_text(prop, "Shadow", "Display shadow behind text");
  RNA_def_property_update(prop, NC_SCENE | ND_SEQUENCER, "rna_Strip_invalidate_raw_update");

  prop = RNA_def_property(srna, "use_outline", PROP_BOOLEAN, PROP_NONE);
  RNA_def_property_boolean_sdna(prop, nullptr, "flag", SEQ_TEXT_OUTLINE);
  RNA_def_property_ui_text(prop, "Outline", "Display outline around text");
  RNA_def_property_update(prop, NC_SCENE | ND_SEQUENCER, "rna_Strip_invalidate_raw_update");

  prop = RNA_def_property(srna, "use_box", PROP_BOOLEAN, PROP_NONE);
  RNA_def_property_boolean_sdna(prop, nullptr, "flag", SEQ_TEXT_BOX);
  RNA_def_property_ui_text(prop, "Box", "Display colored box behind text");
  RNA_def_property_translation_context(prop, BLT_I18NCONTEXT_ID_SEQUENCE);
  RNA_def_property_update(prop, NC_SCENE | ND_SEQUENCER, "rna_Strip_invalidate_raw_update");

  prop = RNA_def_property(srna, "use_bold", PROP_BOOLEAN, PROP_NONE);
  RNA_def_property_boolean_sdna(prop, nullptr, "flag", SEQ_TEXT_BOLD);
  RNA_def_property_ui_text(prop, "Bold", "Display text as bold");
  RNA_def_property_update(prop, NC_SCENE | ND_SEQUENCER, "rna_Strip_invalidate_raw_update");

  prop = RNA_def_property(srna, "use_italic", PROP_BOOLEAN, PROP_NONE);
  RNA_def_property_boolean_sdna(prop, nullptr, "flag", SEQ_TEXT_ITALIC);
  RNA_def_property_ui_text(prop, "Italic", "Display text as italic");
  RNA_def_property_update(prop, NC_SCENE | ND_SEQUENCER, "rna_Strip_invalidate_raw_update");
}

static void rna_def_color_mix(StructRNA *srna)
{
  static const EnumPropertyItem blend_color_items[] = {
      {STRIP_BLEND_DARKEN, "DARKEN", 0, "Darken", ""},
      {STRIP_BLEND_MUL, "MULTIPLY", 0, "Multiply", ""},
      {STRIP_BLEND_COLOR_BURN, "BURN", 0, "Color Burn", ""},
      {STRIP_BLEND_LINEAR_BURN, "LINEAR_BURN", 0, "Linear Burn", ""},
      RNA_ENUM_ITEM_SEPR,
      {STRIP_BLEND_LIGHTEN, "LIGHTEN", 0, "Lighten", ""},
      {STRIP_BLEND_SCREEN, "SCREEN", 0, "Screen", ""},
      {STRIP_BLEND_DODGE, "DODGE", 0, "Color Dodge", ""},
      {STRIP_BLEND_ADD, "ADD", 0, "Add", ""},
      RNA_ENUM_ITEM_SEPR,
      {STRIP_BLEND_OVERLAY, "OVERLAY", 0, "Overlay", ""},
      {STRIP_BLEND_SOFT_LIGHT, "SOFT_LIGHT", 0, "Soft Light", ""},
      {STRIP_BLEND_HARD_LIGHT, "HARD_LIGHT", 0, "Hard Light", ""},
      {STRIP_BLEND_VIVID_LIGHT, "VIVID_LIGHT", 0, "Vivid Light", ""},
      {STRIP_BLEND_LIN_LIGHT, "LINEAR_LIGHT", 0, "Linear Light", ""},
      {STRIP_BLEND_PIN_LIGHT, "PIN_LIGHT", 0, "Pin Light", ""},
      RNA_ENUM_ITEM_SEPR,
      {STRIP_BLEND_DIFFERENCE, "DIFFERENCE", 0, "Difference", ""},
      {STRIP_BLEND_EXCLUSION, "EXCLUSION", 0, "Exclusion", ""},
      {STRIP_BLEND_SUB, "SUBTRACT", 0, "Subtract", ""},
      RNA_ENUM_ITEM_SEPR,
      {STRIP_BLEND_HUE, "HUE", 0, "Hue", ""},
      {STRIP_BLEND_SATURATION, "SATURATION", 0, "Saturation", ""},
      {STRIP_BLEND_BLEND_COLOR, "COLOR", 0, "Color", ""},
      {STRIP_BLEND_VALUE, "VALUE", 0, "Value", ""},
      {0, nullptr, 0, nullptr, nullptr},
  };

  PropertyRNA *prop;

  RNA_def_struct_sdna_from(srna, "ColorMixVars", "effectdata");

  prop = RNA_def_property(srna, "blend_effect", PROP_ENUM, PROP_NONE);
  RNA_def_property_enum_sdna(prop, nullptr, "blend_effect");
  RNA_def_property_enum_items(prop, blend_color_items);
  RNA_def_property_ui_text(
      prop, "Blending Mode", "Method for controlling how the strip combines with other strips");
  RNA_def_property_translation_context(prop, BLT_I18NCONTEXT_COLOR);
  RNA_def_property_update(prop, NC_SCENE | ND_SEQUENCER, "rna_Strip_invalidate_raw_update");

  prop = RNA_def_property(srna, "factor", PROP_FLOAT, PROP_FACTOR);
  RNA_def_property_range(prop, 0.0f, 1.0f);
  RNA_def_property_ui_text(
      prop, "Blend Factor", "Percentage of how much the strip's colors affect other strips");
  RNA_def_property_update(prop, NC_SCENE | ND_SEQUENCER, "rna_Strip_invalidate_raw_update");
}

static EffectInfo def_effects[] = {
    {"AddStrip", "Add Strip", "Add Strip", nullptr, 2},
    {"AdjustmentStrip",
     "Adjustment Layer Strip",
     "Sequence strip to perform filter adjustments to layers below",
     rna_def_input,
     0},
    {"AlphaOverStrip", "Alpha Over Strip", "Alpha Over Strip", nullptr, 2},
    {"AlphaUnderStrip", "Alpha Under Strip", "Alpha Under Strip", nullptr, 2},
    {"ColorStrip",
     "Color Strip",
     "Sequence strip creating an image filled with a single color",
     rna_def_solid_color,
     0},
    {"CrossStrip", "Crossfade Strip", "Crossfade Strip", nullptr, 2},
    {"GammaCrossStrip", "Gamma Crossfade Strip", "Gamma Crossfade Strip", nullptr, 2},
    {"GlowStrip", "Glow Strip", "Sequence strip creating a glow effect", rna_def_glow, 1},
    {"MulticamStrip",
     "Multicam Select Strip",
     "Sequence strip to perform multicam editing",
     rna_def_multicam,
     0},
    {"MultiplyStrip", "Multiply Strip", "Multiply Strip", nullptr, 2},
    {"SpeedControlStrip",
     "SpeedControl Strip",
     "Sequence strip to control the speed of other strips",
     rna_def_speed_control,
     1},
    {"SubtractStrip", "Subtract Strip", "Subtract Strip", nullptr, 2},
    {"WipeStrip", "Wipe Strip", "Sequence strip creating a wipe transition", rna_def_wipe, 2},
    {"GaussianBlurStrip",
     "Gaussian Blur Strip",
     "Sequence strip creating a gaussian blur",
     rna_def_gaussian_blur,
     1},
    {"TextStrip", "Text Strip", "Sequence strip creating text", rna_def_text, 0},
    {"ColorMixStrip", "Color Mix Strip", "Color Mix Strip", rna_def_color_mix, 2},
    {"", "", "", nullptr, 0},
};

static void rna_def_effects(BlenderRNA *brna)
{
  StructRNA *srna;
  EffectInfo *effect;

  for (effect = def_effects; effect->struct_name[0] != '\0'; effect++) {
    srna = RNA_def_struct(brna, effect->struct_name, "EffectStrip");
    RNA_def_struct_ui_text(srna, effect->ui_name, effect->ui_desc);
    RNA_def_struct_sdna(srna, "Strip");

    rna_def_effect_inputs(srna, effect->inputs);

    if (effect->func) {
      effect->func(srna);
    }
  }
}

static void rna_def_modifier_panel_open_prop(StructRNA *srna, const char *identifier, const int id)
{
  BLI_assert(id >= 0);
  BLI_assert(id < sizeof(StripModifierData::layout_panel_open_flag) * 8);

  PropertyRNA *prop;
  prop = RNA_def_property(srna, identifier, PROP_BOOLEAN, PROP_NONE);
  RNA_def_property_flag(prop, PROP_NO_DEG_UPDATE);
  RNA_def_property_boolean_sdna(
      prop, nullptr, "modifier.layout_panel_open_flag", (int64_t(1) << id));
  RNA_def_property_update(prop, NC_SCENE | ND_SEQUENCER, nullptr);
}

static void rna_def_modifier(BlenderRNA *brna)
{
  StructRNA *srna;
  PropertyRNA *prop;

  static const EnumPropertyItem mask_input_type_items[] = {
      {STRIP_MASK_INPUT_STRIP, "STRIP", 0, "Strip", "Use sequencer strip as mask input"},
      {STRIP_MASK_INPUT_ID, "ID", 0, "Mask", "Use mask ID as mask input"},
      {0, nullptr, 0, nullptr, nullptr},
  };

  static const EnumPropertyItem mask_time_items[] = {
      {STRIP_MASK_TIME_RELATIVE,
       "RELATIVE",
       0,
       "Relative",
       "Mask animation is offset to start of strip"},
      {STRIP_MASK_TIME_ABSOLUTE,
       "ABSOLUTE",
       0,
       "Absolute",
       "Mask animation is in sync with scene frame"},
      {0, nullptr, 0, nullptr, nullptr},
  };

  srna = RNA_def_struct(brna, "StripModifier", nullptr);
  RNA_def_struct_sdna(srna, "StripModifierData");
  RNA_def_struct_ui_text(srna, "Strip Modifier", "Modifier for sequence strip");
  RNA_def_struct_refine_func(srna, "rna_StripModifier_refine");
  RNA_def_struct_path_func(srna, "rna_StripModifier_path");

  prop = RNA_def_property(srna, "name", PROP_STRING, PROP_NONE);
  RNA_def_property_string_funcs(prop, nullptr, nullptr, "rna_StripModifier_name_set");
  RNA_def_property_ui_text(prop, "Name", "");
  RNA_def_struct_name_property(srna, prop);
  RNA_def_property_update(prop, NC_SCENE | ND_SEQUENCER, nullptr);

  prop = RNA_def_property(srna, "type", PROP_ENUM, PROP_NONE);
  RNA_def_property_clear_flag(prop, PROP_EDITABLE);
  RNA_def_property_enum_items(prop, rna_enum_strip_modifier_type_items);
  RNA_def_property_ui_text(prop, "Type", "");
  RNA_def_property_update(prop, NC_SCENE | ND_SEQUENCER, nullptr);

  prop = RNA_def_property(srna, "mute", PROP_BOOLEAN, PROP_NONE);
  RNA_def_property_boolean_sdna(prop, nullptr, "flag", STRIP_MODIFIER_FLAG_MUTE);
  RNA_def_property_ui_text(prop, "Mute", "Mute this modifier");
  RNA_def_property_ui_icon(prop, ICON_HIDE_OFF, -1);
  RNA_def_property_update(prop, NC_SCENE | ND_SEQUENCER, "rna_StripModifier_update");

  prop = RNA_def_property(srna, "enable", PROP_BOOLEAN, PROP_NONE);
  RNA_def_property_boolean_negative_sdna(prop, nullptr, "flag", STRIP_MODIFIER_FLAG_MUTE);
  RNA_def_property_ui_text(prop, "Enable", "Enable this modifier");
  RNA_def_property_ui_icon(prop, ICON_RESTRICT_RENDER_ON, 1);
  RNA_def_property_update(prop, NC_SCENE | ND_SEQUENCER, "rna_StripModifier_update");

  prop = RNA_def_property(srna, "show_expanded", PROP_BOOLEAN, PROP_NONE);
  RNA_def_property_flag(prop, PROP_NO_DEG_UPDATE);
  RNA_def_property_boolean_sdna(
      prop, nullptr, "layout_panel_open_flag", UI_PANEL_DATA_EXPAND_ROOT);
  RNA_def_property_ui_text(prop, "Expanded", "Mute expanded settings for the modifier");
  RNA_def_property_update(prop, NC_SCENE | ND_SEQUENCER, nullptr);

  prop = RNA_def_property(srna, "input_mask_type", PROP_ENUM, PROP_NONE);
  RNA_def_property_enum_sdna(prop, nullptr, "mask_input_type");
  RNA_def_property_enum_items(prop, mask_input_type_items);
  RNA_def_property_ui_text(prop, "Type", "Type of input data used for mask");
  RNA_def_property_update(prop, NC_SCENE | ND_SEQUENCER, "rna_StripModifier_update");

  prop = RNA_def_property(srna, "mask_time", PROP_ENUM, PROP_NONE);
  RNA_def_property_enum_sdna(prop, nullptr, "mask_time");
  RNA_def_property_enum_items(prop, mask_time_items);
  RNA_def_property_ui_text(prop, "Mask Time", "Time to use for the Mask animation");
  RNA_def_property_update(prop, NC_SCENE | ND_SEQUENCER, "rna_StripModifier_update");

  prop = RNA_def_property(srna, "input_mask_strip", PROP_POINTER, PROP_NONE);
  RNA_def_property_pointer_sdna(prop, nullptr, "mask_strip");
  RNA_def_property_pointer_funcs(
      prop, nullptr, "rna_StripModifier_strip_set", nullptr, "rna_StripModifier_otherStrip_poll");
  RNA_def_property_flag(prop, PROP_EDITABLE);
  RNA_def_property_ui_text(prop, "Mask Strip", "Strip used as mask input for the modifier");
  RNA_def_property_update(prop, NC_SCENE | ND_SEQUENCER, "rna_StripModifier_update");

  prop = RNA_def_property(srna, "input_mask_id", PROP_POINTER, PROP_NONE);
  RNA_def_property_pointer_sdna(prop, nullptr, "mask_id");
  RNA_def_property_flag(prop, PROP_EDITABLE);
  RNA_def_property_ui_text(prop, "Mask", "Mask ID used as mask input for the modifier");
  RNA_def_property_update(prop, NC_SCENE | ND_SEQUENCER, "rna_StripModifier_update");

  prop = RNA_def_property(srna, "is_active", PROP_BOOLEAN, PROP_NONE);
  RNA_def_property_boolean_sdna(prop, nullptr, "flag", STRIP_MODIFIER_FLAG_ACTIVE);
  RNA_def_property_boolean_funcs(prop, nullptr, "rna_StripModifier_is_active_set");
  RNA_def_property_flag(prop, PROP_NO_DEG_UPDATE);
  RNA_def_property_clear_flag(prop, PROP_ANIMATABLE);
  RNA_def_property_override_flag(prop, PROPOVERRIDE_OVERRIDABLE_LIBRARY);
  RNA_def_property_ui_text(prop, "Is Active", "This modifier is active");
  RNA_def_property_update(prop, NC_SCENE | ND_SEQUENCER, "rna_StripModifier_update");
}

static void rna_def_colorbalance_modifier(BlenderRNA *brna)
{
  StructRNA *srna;
  PropertyRNA *prop;

  srna = RNA_def_struct(brna, "ColorBalanceModifier", "StripModifier");
  RNA_def_struct_ui_icon(srna, ICON_MOD_COLOR_BALANCE);
  RNA_def_struct_sdna(srna, "ColorBalanceModifierData");
  RNA_def_struct_ui_text(
      srna, "ColorBalanceModifier", "Color balance modifier for sequence strip");

  prop = RNA_def_property(srna, "color_balance", PROP_POINTER, PROP_NONE);
  RNA_def_property_struct_type(prop, "StripColorBalanceData");

  prop = RNA_def_property(srna, "color_multiply", PROP_FLOAT, PROP_UNSIGNED);
  RNA_def_property_float_sdna(prop, nullptr, "color_multiply");
  RNA_def_property_range(prop, 0.0f, 20.0f);
  RNA_def_property_float_default(prop, 1.0f);
  RNA_def_property_ui_text(prop, "Multiply Colors", "Multiply the intensity of each pixel");
  RNA_def_property_update(prop, NC_SCENE | ND_SEQUENCER, "rna_StripModifier_update");

  rna_def_modifier_panel_open_prop(srna, "open_mask_input_panel", 1);
}

static void rna_def_whitebalance_modifier(BlenderRNA *brna)
{
  StructRNA *srna;
  PropertyRNA *prop;

  srna = RNA_def_struct(brna, "WhiteBalanceModifier", "StripModifier");
  RNA_def_struct_sdna(srna, "WhiteBalanceModifierData");
  RNA_def_struct_ui_icon(srna, ICON_MOD_WHITE_BALANCE);
  RNA_def_struct_ui_text(
      srna, "WhiteBalanceModifier", "White balance modifier for sequence strip");

  prop = RNA_def_property(srna, "white_value", PROP_FLOAT, PROP_COLOR_GAMMA);
  RNA_def_property_range(prop, 0.0, 1.0);
  RNA_def_property_float_sdna(prop, nullptr, "white_value");
  RNA_def_property_ui_text(prop, "White Value", "This color defines white in the strip");
  RNA_def_property_update(prop, NC_SCENE | ND_SEQUENCER, "rna_StripModifier_update");

  rna_def_modifier_panel_open_prop(srna, "open_mask_input_panel", 1);
}

static void rna_def_curves_modifier(BlenderRNA *brna)
{
  StructRNA *srna;
  PropertyRNA *prop;

  srna = RNA_def_struct(brna, "CurvesModifier", "StripModifier");
  RNA_def_struct_sdna(srna, "CurvesModifierData");
  RNA_def_struct_ui_icon(srna, ICON_MOD_CURVES);
  RNA_def_struct_ui_text(srna, "CurvesModifier", "RGB curves modifier for sequence strip");

  prop = RNA_def_property(srna, "curve_mapping", PROP_POINTER, PROP_NONE);
  RNA_def_property_pointer_sdna(prop, nullptr, "curve_mapping");
  RNA_def_property_struct_type(prop, "CurveMapping");
  RNA_def_property_ui_text(prop, "Curve Mapping", "");
  RNA_def_property_update(prop, NC_SCENE | ND_SEQUENCER, "rna_StripModifier_update");

  rna_def_modifier_panel_open_prop(srna, "open_mask_input_panel", 1);
}

static void rna_def_hue_modifier(BlenderRNA *brna)
{
  StructRNA *srna;
  PropertyRNA *prop;

  srna = RNA_def_struct(brna, "HueCorrectModifier", "StripModifier");
  RNA_def_struct_sdna(srna, "HueCorrectModifierData");
  RNA_def_struct_ui_icon(srna, ICON_MOD_HUE_CORRECT);
  RNA_def_struct_ui_text(srna, "HueCorrectModifier", "Hue correction modifier for sequence strip");

  prop = RNA_def_property(srna, "curve_mapping", PROP_POINTER, PROP_NONE);
  RNA_def_property_pointer_sdna(prop, nullptr, "curve_mapping");
  RNA_def_property_struct_type(prop, "CurveMapping");
  RNA_def_property_ui_text(prop, "Curve Mapping", "");
  RNA_def_property_update(prop, NC_SCENE | ND_SEQUENCER, "rna_StripModifier_update");

  rna_def_modifier_panel_open_prop(srna, "open_mask_input_panel", 1);
}

static void rna_def_mask_modifier(BlenderRNA *brna)
{
  StructRNA *srna;
  srna = RNA_def_struct(brna, "MaskStripModifier", "StripModifier");
  RNA_def_struct_ui_icon(srna, ICON_MOD_MASK);
  RNA_def_struct_ui_text(srna, "Mask Modifier", "Mask modifier for sequence strip");

  /* Mask properties are part of #rna_def_modifier. */
}

static void rna_def_brightcontrast_modifier(BlenderRNA *brna)
{
  StructRNA *srna;
  PropertyRNA *prop;

  srna = RNA_def_struct(brna, "BrightContrastModifier", "StripModifier");
  RNA_def_struct_ui_icon(srna, ICON_MOD_BRIGHTNESS_CONTRAST);
  RNA_def_struct_sdna(srna, "BrightContrastModifierData");
  RNA_def_struct_ui_text(
      srna, "BrightContrastModifier", "Bright/contrast modifier data for sequence strip");

  prop = RNA_def_property(srna, "bright", PROP_FLOAT, PROP_UNSIGNED);
  RNA_def_property_float_sdna(prop, nullptr, "bright");
  RNA_def_property_range(prop, -FLT_MAX, FLT_MAX);
  RNA_def_property_ui_text(prop, "Brightness", "Adjust the luminosity of the colors");
  RNA_def_property_update(prop, NC_SCENE | ND_SEQUENCER, "rna_StripModifier_update");

  prop = RNA_def_property(srna, "contrast", PROP_FLOAT, PROP_UNSIGNED);
  RNA_def_property_float_sdna(prop, nullptr, "contrast");
  RNA_def_property_range(prop, -100.0f, 100.0f);
  RNA_def_property_ui_text(prop, "Contrast", "Adjust the difference in luminosity between pixels");
  RNA_def_property_update(prop, NC_SCENE | ND_SEQUENCER, "rna_StripModifier_update");

  rna_def_modifier_panel_open_prop(srna, "open_mask_input_panel", 1);
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

  srna = RNA_def_struct(brna, "SequencerTonemapModifierData", "StripModifier");
  RNA_def_struct_sdna(srna, "SequencerTonemapModifierData");
  RNA_def_struct_ui_icon(srna, ICON_MOD_TONEMAP);
  RNA_def_struct_ui_text(srna, "SequencerTonemapModifierData", "Tone mapping modifier");

  prop = RNA_def_property(srna, "tonemap_type", PROP_ENUM, PROP_NONE);
  RNA_def_property_enum_sdna(prop, nullptr, "type");
  RNA_def_property_enum_items(prop, type_items);
  RNA_def_property_ui_text(prop, "Tonemap Type", "Tone mapping algorithm");
  RNA_def_property_update(prop, NC_SCENE | ND_SEQUENCER, "rna_StripModifier_update");

  prop = RNA_def_property(srna, "key", PROP_FLOAT, PROP_FACTOR);
  RNA_def_property_range(prop, 0.0f, 1.0f);
  RNA_def_property_ui_text(prop, "Key", "The value the average luminance is mapped to");
  RNA_def_property_update(prop, NC_SCENE | ND_SEQUENCER, "rna_StripModifier_update");

  prop = RNA_def_property(srna, "offset", PROP_FLOAT, PROP_NONE);
  RNA_def_property_range(prop, 0.001f, 10.0f);
  RNA_def_property_ui_text(
      prop,
      "Offset",
      "Normally always 1, but can be used as an extra control to alter the brightness curve");
  RNA_def_property_update(prop, NC_SCENE | ND_SEQUENCER, "rna_StripModifier_update");

  prop = RNA_def_property(srna, "gamma", PROP_FLOAT, PROP_NONE);
  RNA_def_property_range(prop, 0.001f, 3.0f);
  RNA_def_property_ui_text(prop, "Gamma", "If not used, set to 1");
  RNA_def_property_update(prop, NC_SCENE | ND_SEQUENCER, "rna_StripModifier_update");

  prop = RNA_def_property(srna, "intensity", PROP_FLOAT, PROP_NONE);
  RNA_def_property_range(prop, -8.0f, 8.0f);
  RNA_def_property_ui_text(
      prop, "Intensity", "If less than zero, darkens image; otherwise, makes it brighter");
  RNA_def_property_update(prop, NC_SCENE | ND_SEQUENCER, "rna_StripModifier_update");

  prop = RNA_def_property(srna, "contrast", PROP_FLOAT, PROP_FACTOR);
  RNA_def_property_range(prop, 0.0f, 1.0f);
  RNA_def_property_ui_text(prop, "Contrast", "Set to 0 to use estimate from input image");
  RNA_def_property_update(prop, NC_SCENE | ND_SEQUENCER, "rna_StripModifier_update");

  prop = RNA_def_property(srna, "adaptation", PROP_FLOAT, PROP_FACTOR);
  RNA_def_property_range(prop, 0.0f, 1.0f);
  RNA_def_property_ui_text(prop, "Adaptation", "If 0, global; if 1, based on pixel intensity");
  RNA_def_property_update(prop, NC_SCENE | ND_SEQUENCER, "rna_StripModifier_update");

  prop = RNA_def_property(srna, "correction", PROP_FLOAT, PROP_FACTOR);
  RNA_def_property_range(prop, 0.0f, 1.0f);
  RNA_def_property_ui_text(
      prop, "Color Correction", "If 0, same for all channels; if 1, each independent");
  RNA_def_property_update(prop, NC_SCENE | ND_SEQUENCER, "rna_StripModifier_update");

  rna_def_modifier_panel_open_prop(srna, "open_mask_input_panel", 1);
}

static void rna_def_compositor_modifier(BlenderRNA *brna)
{
  StructRNA *srna = RNA_def_struct(brna, "SequencerCompositorModifierData", "StripModifier");
  RNA_def_struct_sdna(srna, "SequencerCompositorModifierData");
  RNA_def_struct_ui_icon(srna, ICON_NODE_COMPOSITING);
  RNA_def_struct_ui_text(srna, "SequencerCompositorModifierData", "Compositor Modifier");

  PropertyRNA *prop = RNA_def_property(srna, "node_group", PROP_POINTER, PROP_NONE);
  RNA_def_property_ui_text(prop, "Node Group", "Node group that controls what this modifier does");
  RNA_def_property_pointer_funcs(
      prop, nullptr, nullptr, nullptr, "rna_CompositorModifier_node_group_poll");
  RNA_def_property_flag(prop, PROP_EDITABLE);
  RNA_def_property_update(
      prop, NC_SCENE | ND_SEQUENCER, "rna_CompositorModifier_node_group_update");

  rna_def_modifier_panel_open_prop(srna, "open_mask_input_panel", 1);
}

static void rna_def_modifiers(BlenderRNA *brna)
{
  rna_def_modifier(brna);

  rna_def_colorbalance_modifier(brna);
  rna_def_curves_modifier(brna);
  rna_def_hue_modifier(brna);
  rna_def_mask_modifier(brna);
  rna_def_brightcontrast_modifier(brna);
  rna_def_whitebalance_modifier(brna);
  rna_def_tonemap_modifier(brna);
  rna_def_compositor_modifier(brna);
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
      prop, NC_SCENE | ND_SEQUENCER, "rna_StripModifier_EQCurveMapping_update");
}

static void rna_def_sound_equalizer_modifier(BlenderRNA *brna)
{
  StructRNA *srna;
  PropertyRNA *prop;
  FunctionRNA *func;
  PropertyRNA *parm;

  srna = RNA_def_struct(brna, "SoundEqualizerModifier", "StripModifier");
  RNA_def_struct_sdna(srna, "SoundEqualizerModifierData");
  RNA_def_struct_ui_text(srna, "SoundEqualizerModifier", "Equalize audio");

  /* Sound Equalizers. */
  prop = RNA_def_property(srna, "graphics", PROP_COLLECTION, PROP_NONE);
  RNA_def_property_struct_type(prop, "EQCurveMappingData");
  RNA_def_property_ui_text(
      prop, "Graphical definition equalization", "Graphical definition equalization");

  /* Add band. */
  func = RNA_def_function(srna, "new_graphic", "rna_Strip_SoundEqualizer_Curve_add");
  RNA_def_function_flag(func, FUNC_USE_CONTEXT);
  RNA_def_function_ui_description(func, "Add a new EQ band");

  parm = RNA_def_float(func,
                       "min_freq",
                       SOUND_EQUALIZER_DEFAULT_MIN_FREQ,
                       0.0,
                       SOUND_EQUALIZER_DEFAULT_MAX_FREQ, /* Hard min and max */
                       "Minimum Frequency",
                       "Minimum Frequency",
                       0.0,
                       SOUND_EQUALIZER_DEFAULT_MAX_FREQ); /* Soft min and max */
  RNA_def_parameter_flags(parm, PropertyFlag(0), PARM_REQUIRED);
  parm = RNA_def_float(func,
                       "max_freq",
                       SOUND_EQUALIZER_DEFAULT_MAX_FREQ,
                       0.0,
                       SOUND_EQUALIZER_DEFAULT_MAX_FREQ, /* Hard min and max */
                       "Maximum Frequency",
                       "Maximum Frequency",
                       0.0,
                       SOUND_EQUALIZER_DEFAULT_MAX_FREQ); /* Soft min and max */
  RNA_def_parameter_flags(parm, PropertyFlag(0), PARM_REQUIRED);

  /* return type */
  parm = RNA_def_pointer(func,
                         "graphic_eqs",
                         "EQCurveMappingData",
                         "",
                         "Newly created graphical Equalizer definition");
  RNA_def_function_return(func, parm);

  /* clear all modifiers */
  func = RNA_def_function(srna, "clear_soundeqs", "rna_Strip_SoundEqualizer_Curve_clear");
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

  rna_def_strip(brna);
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

  RNA_api_strip_retiming_keys(brna);
}

#endif
