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
#include <limits.h>

#include "DNA_anim_types.h"
#include "DNA_object_types.h"
#include "DNA_scene_types.h"
#include "DNA_sequence_types.h"
#include "DNA_movieclip_types.h"
#include "DNA_vfont_types.h"

#include "BLI_math.h"

#include "BLT_translation.h"

#include "BKE_animsys.h"
#include "BKE_sequencer.h"
#include "BKE_sound.h"

#include "IMB_metadata.h"

#include "MEM_guardedalloc.h"

#include "RNA_access.h"
#include "RNA_define.h"
#include "RNA_enum_types.h"

#include "rna_internal.h"

#include "WM_types.h"

typedef struct EffectInfo {
  const char *struct_name;
  const char *ui_name;
  const char *ui_desc;
  void (*func)(StructRNA *);
  int inputs;
} EffectInfo;

const EnumPropertyItem rna_enum_sequence_modifier_type_items[] = {
    {seqModifierType_ColorBalance, "COLOR_BALANCE", ICON_NONE, "Color Balance", ""},
    {seqModifierType_Curves, "CURVES", ICON_NONE, "Curves", ""},
    {seqModifierType_HueCorrect, "HUE_CORRECT", ICON_NONE, "Hue Correct", ""},
    {seqModifierType_BrightContrast, "BRIGHT_CONTRAST", ICON_NONE, "Bright/Contrast", ""},
    {seqModifierType_Mask, "MASK", ICON_NONE, "Mask", ""},
    {seqModifierType_WhiteBalance, "WHITE_BALANCE", ICON_NONE, "White Balance", ""},
    {seqModifierType_Tonemap, "TONEMAP", ICON_NONE, "Tone Map", ""},
    {0, NULL, 0, NULL, NULL},
};

#ifdef RNA_RUNTIME

#  include "BKE_report.h"
#  include "BKE_idprop.h"
#  include "BKE_movieclip.h"

#  include "WM_api.h"

#  include "IMB_imbuf.h"

typedef struct SequenceSearchData {
  Sequence *seq;
  void *data;
  SequenceModifierData *smd;
} SequenceSearchData;

/* build a temp reference to the parent */
static void meta_tmp_ref(Sequence *seq_par, Sequence *seq)
{
  for (; seq; seq = seq->next) {
    seq->tmp = seq_par;
    if (seq->type == SEQ_TYPE_META) {
      meta_tmp_ref(seq, seq->seqbase.first);
    }
  }
}

static void rna_SequenceElement_update(Main *UNUSED(bmain), Scene *UNUSED(scene), PointerRNA *ptr)
{
  Scene *scene = (Scene *)ptr->id.data;
  Editing *ed = BKE_sequencer_editing_get(scene, false);

  if (ed) {
    StripElem *se = (StripElem *)ptr->data;
    Sequence *seq;

    /* slow but we can't avoid! */
    seq = BKE_sequencer_from_elem(&ed->seqbase, se);
    if (seq) {
      BKE_sequence_invalidate_cache(scene, seq);
    }
  }
}

static void rna_Sequence_update(Main *UNUSED(bmain), Scene *UNUSED(scene), PointerRNA *ptr)
{
  Scene *scene = (Scene *)ptr->id.data;
  Editing *ed = BKE_sequencer_editing_get(scene, false);

  if (ed) {
    Sequence *seq = (Sequence *)ptr->data;

    BKE_sequence_invalidate_cache(scene, seq);
  }
}

static void rna_SequenceEditor_sequences_all_begin(CollectionPropertyIterator *iter,
                                                   PointerRNA *ptr)
{
  Scene *scene = (Scene *)ptr->id.data;
  Editing *ed = BKE_sequencer_editing_get(scene, false);

  meta_tmp_ref(NULL, ed->seqbase.first);

  rna_iterator_listbase_begin(iter, &ed->seqbase, NULL);
}

static void rna_SequenceEditor_update_cache(Main *UNUSED(bmain),
                                            Scene *scene,
                                            PointerRNA *UNUSED(ptr))
{
  Editing *ed = scene->ed;

  BKE_sequencer_free_imbuf(scene, &ed->seqbase, false);
}

static void rna_SequenceEditor_sequences_all_next(CollectionPropertyIterator *iter)
{
  ListBaseIterator *internal = &iter->internal.listbase;
  Sequence *seq = (Sequence *)internal->link;

  if (seq->seqbase.first)
    internal->link = (Link *)seq->seqbase.first;
  else if (seq->next)
    internal->link = (Link *)seq->next;
  else {
    internal->link = NULL;

    do {
      seq = seq->tmp; /* XXX - seq's don't reference their parents! */
      if (seq && seq->next) {
        internal->link = (Link *)seq->next;
        break;
      }
    } while (seq);
  }

  iter->valid = (internal->link != NULL);
}

/* internal use */
static int rna_SequenceEditor_elements_length(PointerRNA *ptr)
{
  Sequence *seq = (Sequence *)ptr->data;

  /* Hack? copied from sequencer.c::reload_sequence_new_file() */
  size_t olen = MEM_allocN_len(seq->strip->stripdata) / sizeof(struct StripElem);

  /* The problem with seq->strip->len and seq->len is that it's discounted from the offset
   * (hard cut trim). */
  return (int)olen;
}

static void rna_SequenceEditor_elements_begin(CollectionPropertyIterator *iter, PointerRNA *ptr)
{
  Sequence *seq = (Sequence *)ptr->data;
  rna_iterator_array_begin(iter,
                           (void *)seq->strip->stripdata,
                           sizeof(StripElem),
                           rna_SequenceEditor_elements_length(ptr),
                           0,
                           NULL);
}

static void rna_Sequence_views_format_update(Main *bmain, Scene *scene, PointerRNA *ptr)
{
  rna_Sequence_update(bmain, scene, ptr);
}

static void do_sequence_frame_change_update(Scene *scene, Sequence *seq)
{
  Editing *ed = BKE_sequencer_editing_get(scene, false);
  ListBase *seqbase = BKE_sequence_seqbase(&ed->seqbase, seq);
  Sequence *tseq;
  BKE_sequence_calc_disp(scene, seq);

  /* ensure effects are always fit in length to their input */

  /* TODO(sergey): probably could be optimized.
   *               in terms skipping update of non-changing strips
   */
  for (tseq = seqbase->first; tseq; tseq = tseq->next) {
    if (tseq->seq1 || tseq->seq2 || tseq->seq3) {
      BKE_sequence_calc(scene, tseq);
    }
  }

  if (BKE_sequence_test_overlap(seqbase, seq)) {
    BKE_sequence_base_shuffle(seqbase, seq, scene); /* XXX - BROKEN!, uses context seqbasep */
  }
  BKE_sequencer_sort(scene);
}

/* A simple wrapper around above func, directly usable as prop update func.
 * Also invalidate cache if needed.
 */
static void rna_Sequence_frame_change_update(Main *bmain, Scene *UNUSED(scene), PointerRNA *ptr)
{
  Scene *scene = (Scene *)ptr->id.data;
  do_sequence_frame_change_update(scene, (Sequence *)ptr->data);
  rna_Sequence_update(bmain, scene, ptr);
}

static void rna_Sequence_start_frame_set(PointerRNA *ptr, int value)
{
  Sequence *seq = (Sequence *)ptr->data;
  Scene *scene = (Scene *)ptr->id.data;

  BKE_sequence_translate(scene, seq, value - seq->start);
  do_sequence_frame_change_update(scene, seq);
}

static void rna_Sequence_start_frame_final_set(PointerRNA *ptr, int value)
{
  Sequence *seq = (Sequence *)ptr->data;
  Scene *scene = (Scene *)ptr->id.data;

  BKE_sequence_tx_set_final_left(seq, value);
  BKE_sequence_single_fix(seq);
  do_sequence_frame_change_update(scene, seq);
}

static void rna_Sequence_end_frame_final_set(PointerRNA *ptr, int value)
{
  Sequence *seq = (Sequence *)ptr->data;
  Scene *scene = (Scene *)ptr->id.data;

  BKE_sequence_tx_set_final_right(seq, value);
  BKE_sequence_single_fix(seq);
  do_sequence_frame_change_update(scene, seq);
}

static void rna_Sequence_anim_startofs_final_set(PointerRNA *ptr, int value)
{
  Sequence *seq = (Sequence *)ptr->data;
  Scene *scene = (Scene *)ptr->id.data;

  seq->anim_startofs = MIN2(value, seq->len + seq->anim_startofs);

  BKE_sequence_reload_new_file(scene, seq, false);
  do_sequence_frame_change_update(scene, seq);
}

static void rna_Sequence_anim_endofs_final_set(PointerRNA *ptr, int value)
{
  Sequence *seq = (Sequence *)ptr->data;
  Scene *scene = (Scene *)ptr->id.data;

  seq->anim_endofs = MIN2(value, seq->len + seq->anim_endofs);

  BKE_sequence_reload_new_file(scene, seq, false);
  do_sequence_frame_change_update(scene, seq);
}

static void rna_Sequence_frame_length_set(PointerRNA *ptr, int value)
{
  Sequence *seq = (Sequence *)ptr->data;
  Scene *scene = (Scene *)ptr->id.data;

  BKE_sequence_tx_set_final_right(seq, BKE_sequence_tx_get_final_left(seq, false) + value);
  do_sequence_frame_change_update(scene, seq);
}

static int rna_Sequence_frame_length_get(PointerRNA *ptr)
{
  Sequence *seq = (Sequence *)ptr->data;
  return BKE_sequence_tx_get_final_right(seq, false) - BKE_sequence_tx_get_final_left(seq, false);
}

static int rna_Sequence_frame_editable(PointerRNA *ptr, const char **UNUSED(r_info))
{
  Sequence *seq = (Sequence *)ptr->data;
  /* Effect sequences' start frame and length must be readonly! */
  return (BKE_sequence_effect_get_num_inputs(seq->type)) ? 0 : PROP_EDITABLE;
}

static void rna_Sequence_channel_set(PointerRNA *ptr, int value)
{
  Sequence *seq = (Sequence *)ptr->data;
  Scene *scene = (Scene *)ptr->id.data;
  Editing *ed = BKE_sequencer_editing_get(scene, false);
  ListBase *seqbase = BKE_sequence_seqbase(&ed->seqbase, seq);

  /* check channel increment or decrement */
  const int channel_delta = (value >= seq->machine) ? 1 : -1;
  seq->machine = value;

  if (BKE_sequence_test_overlap(seqbase, seq)) {
    /* XXX - BROKEN!, uses context seqbasep */
    BKE_sequence_base_shuffle_ex(seqbase, seq, scene, channel_delta);
  }
  BKE_sequencer_sort(scene);
}

static void rna_Sequence_frame_offset_range(
    PointerRNA *ptr, int *min, int *max, int *UNUSED(softmin), int *UNUSED(softmax))
{
  Sequence *seq = (Sequence *)ptr->data;
  *min = ELEM(seq->type, SEQ_TYPE_SOUND_RAM, SEQ_TYPE_SOUND_HD) ? 0 : INT_MIN;
  *max = INT_MAX;
}

static void rna_Sequence_use_proxy_set(PointerRNA *ptr, bool value)
{
  Sequence *seq = (Sequence *)ptr->data;
  BKE_sequencer_proxy_set(seq, value != 0);
}

static void rna_Sequence_use_translation_set(PointerRNA *ptr, bool value)
{
  Sequence *seq = (Sequence *)ptr->data;
  if (value) {
    seq->flag |= SEQ_USE_TRANSFORM;
    if (seq->strip->transform == NULL) {
      seq->strip->transform = MEM_callocN(sizeof(struct StripTransform), "StripTransform");
    }
  }
  else {
    seq->flag &= ~SEQ_USE_TRANSFORM;
  }
}

static void rna_Sequence_use_crop_set(PointerRNA *ptr, bool value)
{
  Sequence *seq = (Sequence *)ptr->data;
  if (value) {
    seq->flag |= SEQ_USE_CROP;
    if (seq->strip->crop == NULL) {
      seq->strip->crop = MEM_callocN(sizeof(struct StripCrop), "StripCrop");
    }
  }
  else {
    seq->flag &= ~SEQ_USE_CROP;
  }
}

static int transform_seq_cmp_cb(Sequence *seq, void *arg_pt)
{
  SequenceSearchData *data = arg_pt;

  if (seq->strip && seq->strip->transform == data->data) {
    data->seq = seq;
    return -1; /* done so bail out */
  }
  return 1;
}

static Sequence *sequence_get_by_transform(Editing *ed, StripTransform *transform)
{
  SequenceSearchData data;

  data.seq = NULL;
  data.data = transform;

  /* irritating we need to search for our sequence! */
  BKE_sequencer_base_recursive_apply(&ed->seqbase, transform_seq_cmp_cb, &data);

  return data.seq;
}

static char *rna_SequenceTransform_path(PointerRNA *ptr)
{
  Scene *scene = ptr->id.data;
  Editing *ed = BKE_sequencer_editing_get(scene, false);
  Sequence *seq = sequence_get_by_transform(ed, ptr->data);

  if (seq && seq->name + 2) {
    char name_esc[(sizeof(seq->name) - 2) * 2];

    BLI_strescape(name_esc, seq->name + 2, sizeof(name_esc));
    return BLI_sprintfN("sequence_editor.sequences_all[\"%s\"].transform", name_esc);
  }
  else {
    return BLI_strdup("");
  }
}

static void rna_SequenceTransform_update(Main *UNUSED(bmain),
                                         Scene *UNUSED(scene),
                                         PointerRNA *ptr)
{
  Scene *scene = (Scene *)ptr->id.data;
  Editing *ed = BKE_sequencer_editing_get(scene, false);
  Sequence *seq = sequence_get_by_transform(ed, ptr->data);

  BKE_sequence_invalidate_cache(scene, seq);
}

static int crop_seq_cmp_cb(Sequence *seq, void *arg_pt)
{
  SequenceSearchData *data = arg_pt;

  if (seq->strip && seq->strip->crop == data->data) {
    data->seq = seq;
    return -1; /* done so bail out */
  }
  return 1;
}

static Sequence *sequence_get_by_crop(Editing *ed, StripCrop *crop)
{
  SequenceSearchData data;

  data.seq = NULL;
  data.data = crop;

  /* irritating we need to search for our sequence! */
  BKE_sequencer_base_recursive_apply(&ed->seqbase, crop_seq_cmp_cb, &data);

  return data.seq;
}

static char *rna_SequenceCrop_path(PointerRNA *ptr)
{
  Scene *scene = ptr->id.data;
  Editing *ed = BKE_sequencer_editing_get(scene, false);
  Sequence *seq = sequence_get_by_crop(ed, ptr->data);

  if (seq && seq->name + 2) {
    char name_esc[(sizeof(seq->name) - 2) * 2];

    BLI_strescape(name_esc, seq->name + 2, sizeof(name_esc));
    return BLI_sprintfN("sequence_editor.sequences_all[\"%s\"].crop", name_esc);
  }
  else {
    return BLI_strdup("");
  }
}

static void rna_SequenceCrop_update(Main *UNUSED(bmain), Scene *UNUSED(scene), PointerRNA *ptr)
{
  Scene *scene = (Scene *)ptr->id.data;
  Editing *ed = BKE_sequencer_editing_get(scene, false);
  Sequence *seq = sequence_get_by_crop(ed, ptr->data);

  BKE_sequence_invalidate_cache(scene, seq);
}

static void rna_Sequence_text_font_set(PointerRNA *ptr, PointerRNA ptr_value)
{
  Sequence *seq = ptr->data;
  TextVars *data = seq->effectdata;
  VFont *value = ptr_value.data;

  BKE_sequencer_text_font_unload(data, true);

  id_us_plus(&value->id);
  data->text_blf_id = SEQ_FONT_NOT_LOADED;
  data->text_font = value;
}

/* name functions that ignore the first two characters */
static void rna_Sequence_name_get(PointerRNA *ptr, char *value)
{
  Sequence *seq = (Sequence *)ptr->data;
  BLI_strncpy(value, seq->name + 2, sizeof(seq->name) - 2);
}

static int rna_Sequence_name_length(PointerRNA *ptr)
{
  Sequence *seq = (Sequence *)ptr->data;
  return strlen(seq->name + 2);
}

static void rna_Sequence_name_set(PointerRNA *ptr, const char *value)
{
  Scene *scene = (Scene *)ptr->id.data;
  Sequence *seq = (Sequence *)ptr->data;
  char oldname[sizeof(seq->name)];
  AnimData *adt;

  /* make a copy of the old name first */
  BLI_strncpy(oldname, seq->name + 2, sizeof(seq->name) - 2);

  /* copy the new name into the name slot */
  BLI_strncpy_utf8(seq->name + 2, value, sizeof(seq->name) - 2);

  /* make sure the name is unique */
  BKE_sequence_base_unique_name_recursive(&scene->ed->seqbase, seq);

  /* fix all the animation data which may link to this */

  /* Don't rename everywhere because these are per scene. */
#  if 0
  BKE_animdata_fix_paths_rename_all(NULL, "sequence_editor.sequences_all", oldname, seq->name + 2);
#  endif
  adt = BKE_animdata_from_id(&scene->id);
  if (adt)
    BKE_animdata_fix_paths_rename(
        &scene->id, adt, NULL, "sequence_editor.sequences_all", oldname, seq->name + 2, 0, 0, 1);
}

static StructRNA *rna_Sequence_refine(struct PointerRNA *ptr)
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

static char *rna_Sequence_path(PointerRNA *ptr)
{
  Sequence *seq = (Sequence *)ptr->data;

  /* sequencer data comes from scene...
   * TODO: would be nice to make SequenceEditor data a data-block of its own (for shorter paths)
   */
  if (seq->name + 2) {
    char name_esc[(sizeof(seq->name) - 2) * 2];

    BLI_strescape(name_esc, seq->name + 2, sizeof(name_esc));
    return BLI_sprintfN("sequence_editor.sequences_all[\"%s\"]", name_esc);
  }
  else {
    return BLI_strdup("");
  }
}

static IDProperty *rna_Sequence_idprops(PointerRNA *ptr, bool create)
{
  Sequence *seq = ptr->data;

  if (create && !seq->prop) {
    IDPropertyTemplate val = {0};
    seq->prop = IDP_New(IDP_GROUP, &val, "Sequence ID properties");
  }

  return seq->prop;
}

static PointerRNA rna_MovieSequence_metadata_get(Sequence *seq)
{
  if (seq == NULL || seq->anims.first == NULL) {
    return PointerRNA_NULL;
  }

  StripAnim *sanim = seq->anims.first;
  if (sanim->anim == NULL) {
    return PointerRNA_NULL;
  }

  IDProperty *metadata = IMB_anim_load_metadata(sanim->anim);
  if (metadata == NULL) {
    return PointerRNA_NULL;
  }

  PointerRNA ptr;
  RNA_pointer_create(NULL, &RNA_IDPropertyWrapPtr, metadata, &ptr);
  return ptr;
}

static PointerRNA rna_SequenceEditor_meta_stack_get(CollectionPropertyIterator *iter)
{
  ListBaseIterator *internal = &iter->internal.listbase;
  MetaStack *ms = (MetaStack *)internal->link;

  return rna_pointer_inherit_refine(&iter->parent, &RNA_Sequence, ms->parseq);
}

/* TODO, expose seq path setting as a higher level sequencer BKE function */
static void rna_Sequence_filepath_set(PointerRNA *ptr, const char *value)
{
  Sequence *seq = (Sequence *)(ptr->data);
  BLI_split_dirfile(value,
                    seq->strip->dir,
                    seq->strip->stripdata->name,
                    sizeof(seq->strip->dir),
                    sizeof(seq->strip->stripdata->name));
}

static void rna_Sequence_filepath_get(PointerRNA *ptr, char *value)
{
  Sequence *seq = (Sequence *)(ptr->data);

  BLI_join_dirfile(value, FILE_MAX, seq->strip->dir, seq->strip->stripdata->name);
}

static int rna_Sequence_filepath_length(PointerRNA *ptr)
{
  Sequence *seq = (Sequence *)(ptr->data);
  char path[FILE_MAX];

  BLI_join_dirfile(path, sizeof(path), seq->strip->dir, seq->strip->stripdata->name);
  return strlen(path);
}

static void rna_Sequence_proxy_filepath_set(PointerRNA *ptr, const char *value)
{
  StripProxy *proxy = (StripProxy *)(ptr->data);
  BLI_split_dirfile(value, proxy->dir, proxy->file, sizeof(proxy->dir), sizeof(proxy->file));
  if (proxy->anim) {
    IMB_free_anim(proxy->anim);
    proxy->anim = NULL;
  }
}

static void rna_Sequence_proxy_filepath_get(PointerRNA *ptr, char *value)
{
  StripProxy *proxy = (StripProxy *)(ptr->data);

  BLI_join_dirfile(value, FILE_MAX, proxy->dir, proxy->file);
}

static int rna_Sequence_proxy_filepath_length(PointerRNA *ptr)
{
  StripProxy *proxy = (StripProxy *)(ptr->data);
  char path[FILE_MAX];

  BLI_join_dirfile(path, sizeof(path), proxy->dir, proxy->file);
  return strlen(path);
}

static void rna_Sequence_volume_set(PointerRNA *ptr, float value)
{
  Sequence *seq = (Sequence *)(ptr->data);

  seq->volume = value;
  if (seq->scene_sound)
    BKE_sound_set_scene_sound_volume(
        seq->scene_sound, value, (seq->flag & SEQ_AUDIO_VOLUME_ANIMATED) != 0);
}

static void rna_Sequence_pitch_set(PointerRNA *ptr, float value)
{
  Sequence *seq = (Sequence *)(ptr->data);

  seq->pitch = value;
  if (seq->scene_sound)
    BKE_sound_set_scene_sound_pitch(
        seq->scene_sound, value, (seq->flag & SEQ_AUDIO_PITCH_ANIMATED) != 0);
}

static void rna_Sequence_pan_set(PointerRNA *ptr, float value)
{
  Sequence *seq = (Sequence *)(ptr->data);

  seq->pan = value;
  if (seq->scene_sound)
    BKE_sound_set_scene_sound_pan(
        seq->scene_sound, value, (seq->flag & SEQ_AUDIO_PAN_ANIMATED) != 0);
}

static int rna_Sequence_input_count_get(PointerRNA *ptr)
{
  Sequence *seq = (Sequence *)(ptr->data);

  return BKE_sequence_effect_get_num_inputs(seq->type);
}

#  if 0
static void rna_SoundSequence_filename_set(PointerRNA *ptr, const char *value)
{
  Sequence *seq = (Sequence *)(ptr->data);
  BLI_split_dirfile(value,
                    seq->strip->dir,
                    seq->strip->stripdata->name,
                    sizeof(seq->strip->dir),
                    sizeof(seq->strip->stripdata->name));
}

static void rna_SequenceElement_filename_set(PointerRNA *ptr, const char *value)
{
  StripElem *elem = (StripElem *)(ptr->data);
  BLI_split_file_part(value, elem->name, sizeof(elem->name));
}
#  endif

static void rna_Sequence_update_reopen_files(Main *UNUSED(bmain),
                                             Scene *UNUSED(scene),
                                             PointerRNA *ptr)
{
  Scene *scene = (Scene *)ptr->id.data;
  Editing *ed = BKE_sequencer_editing_get(scene, false);

  BKE_sequencer_free_imbuf(scene, &ed->seqbase, false);

  if (RNA_struct_is_a(ptr->type, &RNA_SoundSequence))
    BKE_sequencer_update_sound_bounds(scene, ptr->data);
}

static void rna_Sequence_mute_update(Main *bmain, Scene *UNUSED(scene), PointerRNA *ptr)
{
  Scene *scene = (Scene *)ptr->id.data;
  Editing *ed = BKE_sequencer_editing_get(scene, false);

  BKE_sequencer_update_muting(ed);
  rna_Sequence_update(bmain, scene, ptr);
}

static void rna_Sequence_filepath_update(Main *bmain, Scene *UNUSED(scene), PointerRNA *ptr)
{
  Scene *scene = (Scene *)ptr->id.data;
  Sequence *seq = (Sequence *)(ptr->data);
  BKE_sequence_reload_new_file(scene, seq, true);
  BKE_sequence_calc(scene, seq);
  rna_Sequence_update(bmain, scene, ptr);
}

static void rna_Sequence_sound_update(Main *bmain, Scene *scene, PointerRNA *ptr)
{
  Sequence *seq = (Sequence *)ptr->data;
  if (seq->sound != NULL) {
    BKE_sound_update_scene_sound(seq->scene_sound, seq->sound);
  }
  rna_Sequence_update(bmain, scene, ptr);
}

static int seqproxy_seq_cmp_cb(Sequence *seq, void *arg_pt)
{
  SequenceSearchData *data = arg_pt;

  if (seq->strip && seq->strip->proxy == data->data) {
    data->seq = seq;
    return -1; /* done so bail out */
  }
  return 1;
}

static Sequence *sequence_get_by_proxy(Editing *ed, StripProxy *proxy)
{
  SequenceSearchData data;

  data.seq = NULL;
  data.data = proxy;

  BKE_sequencer_base_recursive_apply(&ed->seqbase, seqproxy_seq_cmp_cb, &data);
  return data.seq;
}

static void rna_Sequence_tcindex_update(Main *UNUSED(bmain), Scene *UNUSED(scene), PointerRNA *ptr)
{
  Scene *scene = (Scene *)ptr->id.data;
  Editing *ed = BKE_sequencer_editing_get(scene, false);
  Sequence *seq = sequence_get_by_proxy(ed, ptr->data);

  BKE_sequence_reload_new_file(scene, seq, false);
  do_sequence_frame_change_update(scene, seq);
}

static void rna_SequenceProxy_update(Main *UNUSED(bmain), Scene *UNUSED(scene), PointerRNA *ptr)
{
  Scene *scene = (Scene *)ptr->id.data;
  Editing *ed = BKE_sequencer_editing_get(scene, false);
  Sequence *seq = sequence_get_by_proxy(ed, ptr->data);

  BKE_sequence_invalidate_cache(scene, seq);
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

static int colbalance_seq_cmp_cb(Sequence *seq, void *arg_pt)
{
  SequenceSearchData *data = arg_pt;

  if (seq->modifiers.first) {
    SequenceModifierData *smd = seq->modifiers.first;

    for (smd = seq->modifiers.first; smd; smd = smd->next) {
      if (smd->type == seqModifierType_ColorBalance) {
        ColorBalanceModifierData *cbmd = (ColorBalanceModifierData *)smd;

        if (&cbmd->color_balance == data->data) {
          data->seq = seq;
          data->smd = smd;
          return -1; /* done so bail out */
        }
      }
    }
  }

  return 1;
}

static Sequence *sequence_get_by_colorbalance(Editing *ed,
                                              StripColorBalance *cb,
                                              SequenceModifierData **r_smd)
{
  SequenceSearchData data;

  data.seq = NULL;
  data.smd = NULL;
  data.data = cb;

  /* irritating we need to search for our sequence! */
  BKE_sequencer_base_recursive_apply(&ed->seqbase, colbalance_seq_cmp_cb, &data);

  *r_smd = data.smd;

  return data.seq;
}

static char *rna_SequenceColorBalance_path(PointerRNA *ptr)
{
  Scene *scene = ptr->id.data;
  SequenceModifierData *smd;
  Editing *ed = BKE_sequencer_editing_get(scene, false);
  Sequence *seq = sequence_get_by_colorbalance(ed, ptr->data, &smd);

  if (seq && seq->name + 2) {
    char name_esc[(sizeof(seq->name) - 2) * 2];

    BLI_strescape(name_esc, seq->name + 2, sizeof(name_esc));

    if (!smd) {
      /* path to old filter color balance */
      return BLI_sprintfN("sequence_editor.sequences_all[\"%s\"].color_balance", name_esc);
    }
    else {
      /* path to modifier */
      char name_esc_smd[sizeof(smd->name) * 2];

      BLI_strescape(name_esc_smd, smd->name, sizeof(name_esc_smd));
      return BLI_sprintfN("sequence_editor.sequences_all[\"%s\"].modifiers[\"%s\"].color_balance",
                          name_esc,
                          name_esc_smd);
    }
  }
  else
    return BLI_strdup("");
}

static void rna_SequenceColorBalance_update(Main *UNUSED(bmain),
                                            Scene *UNUSED(scene),
                                            PointerRNA *ptr)
{
  Scene *scene = (Scene *)ptr->id.data;
  Editing *ed = BKE_sequencer_editing_get(scene, false);
  SequenceModifierData *smd;
  Sequence *seq = sequence_get_by_colorbalance(ed, ptr->data, &smd);

  if (smd == NULL)
    BKE_sequence_invalidate_cache(scene, seq);
  else
    BKE_sequence_invalidate_cache_for_modifier(scene, seq);
}

static void rna_SequenceEditor_overlay_lock_set(PointerRNA *ptr, bool value)
{
  Scene *scene = ptr->id.data;
  Editing *ed = BKE_sequencer_editing_get(scene, false);

  if (ed == NULL)
    return;

  /* convert from abs to relative and back */
  if ((ed->over_flag & SEQ_EDIT_OVERLAY_ABS) == 0 && value) {
    ed->over_cfra = scene->r.cfra + ed->over_ofs;
    ed->over_flag |= SEQ_EDIT_OVERLAY_ABS;
  }
  else if ((ed->over_flag & SEQ_EDIT_OVERLAY_ABS) && !value) {
    ed->over_ofs = ed->over_cfra - scene->r.cfra;
    ed->over_flag &= ~SEQ_EDIT_OVERLAY_ABS;
  }
}

static int rna_SequenceEditor_overlay_frame_get(PointerRNA *ptr)
{
  Scene *scene = (Scene *)ptr->id.data;
  Editing *ed = BKE_sequencer_editing_get(scene, false);

  if (ed == NULL)
    return scene->r.cfra;

  if (ed->over_flag & SEQ_EDIT_OVERLAY_ABS)
    return ed->over_cfra - scene->r.cfra;
  else
    return ed->over_ofs;
}

static void rna_SequenceEditor_overlay_frame_set(PointerRNA *ptr, int value)
{
  Scene *scene = (Scene *)ptr->id.data;
  Editing *ed = BKE_sequencer_editing_get(scene, false);

  if (ed == NULL)
    return;

  if (ed->over_flag & SEQ_EDIT_OVERLAY_ABS)
    ed->over_cfra = (scene->r.cfra + value);
  else
    ed->over_ofs = value;
}

static int modifier_seq_cmp_cb(Sequence *seq, void *arg_pt)
{
  SequenceSearchData *data = arg_pt;

  if (BLI_findindex(&seq->modifiers, data->data) != -1) {
    data->seq = seq;
    return -1; /* done so bail out */
  }

  return 1;
}

static Sequence *sequence_get_by_modifier(Editing *ed, SequenceModifierData *smd)
{
  SequenceSearchData data;

  data.seq = NULL;
  data.data = smd;

  /* irritating we need to search for our sequence! */
  BKE_sequencer_base_recursive_apply(&ed->seqbase, modifier_seq_cmp_cb, &data);

  return data.seq;
}

static StructRNA *rna_SequenceModifier_refine(struct PointerRNA *ptr)
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
    default:
      return &RNA_SequenceModifier;
  }
}

static char *rna_SequenceModifier_path(PointerRNA *ptr)
{
  Scene *scene = ptr->id.data;
  Editing *ed = BKE_sequencer_editing_get(scene, false);
  SequenceModifierData *smd = ptr->data;
  Sequence *seq = sequence_get_by_modifier(ed, smd);

  if (seq && seq->name + 2) {
    char name_esc[(sizeof(seq->name) - 2) * 2];
    char name_esc_smd[sizeof(smd->name) * 2];

    BLI_strescape(name_esc, seq->name + 2, sizeof(name_esc));
    BLI_strescape(name_esc_smd, smd->name, sizeof(name_esc_smd));
    return BLI_sprintfN(
        "sequence_editor.sequences_all[\"%s\"].modifiers[\"%s\"]", name_esc, name_esc_smd);
  }
  else {
    return BLI_strdup("");
  }
}

static void rna_SequenceModifier_name_set(PointerRNA *ptr, const char *value)
{
  SequenceModifierData *smd = ptr->data;
  Scene *scene = (Scene *)ptr->id.data;
  Editing *ed = BKE_sequencer_editing_get(scene, false);
  Sequence *seq = sequence_get_by_modifier(ed, smd);
  AnimData *adt;
  char oldname[sizeof(smd->name)];

  /* make a copy of the old name first */
  BLI_strncpy(oldname, smd->name, sizeof(smd->name));

  /* copy the new name into the name slot */
  BLI_strncpy_utf8(smd->name, value, sizeof(smd->name));

  /* make sure the name is truly unique */
  BKE_sequence_modifier_unique_name(seq, smd);

  /* fix all the animation data which may link to this */
  adt = BKE_animdata_from_id(&scene->id);
  if (adt) {
    char path[1024];

    BLI_snprintf(
        path, sizeof(path), "sequence_editor.sequences_all[\"%s\"].modifiers", seq->name + 2);
    BKE_animdata_fix_paths_rename(&scene->id, adt, NULL, path, oldname, smd->name, 0, 0, 1);
  }
}

static void rna_SequenceModifier_update(Main *UNUSED(bmain), Scene *UNUSED(scene), PointerRNA *ptr)
{
  /* strip from other scenes could be modified, so using active scene is not reliable */
  Scene *scene = (Scene *)ptr->id.data;
  Editing *ed = BKE_sequencer_editing_get(scene, false);
  Sequence *seq = sequence_get_by_modifier(ed, ptr->data);

  BKE_sequence_invalidate_cache_for_modifier(scene, seq);
}

static bool rna_SequenceModifier_otherSequence_poll(PointerRNA *ptr, PointerRNA value)
{
  Scene *scene = (Scene *)ptr->id.data;
  Editing *ed = BKE_sequencer_editing_get(scene, false);
  Sequence *seq = sequence_get_by_modifier(ed, ptr->data);
  Sequence *cur = (Sequence *)value.data;

  if ((seq == cur) || (cur->type == SEQ_TYPE_SOUND_RAM)) {
    return false;
  }

  return true;
}

static SequenceModifierData *rna_Sequence_modifier_new(
    Sequence *seq, bContext *C, ReportList *reports, const char *name, int type)
{
  if (!BKE_sequence_supports_modifiers(seq)) {
    BKE_report(reports, RPT_ERROR, "Sequence type does not support modifiers");

    return NULL;
  }
  else {
    Scene *scene = CTX_data_scene(C);
    SequenceModifierData *smd;

    smd = BKE_sequence_modifier_new(seq, name, type);

    BKE_sequence_invalidate_cache_for_modifier(scene, seq);

    WM_main_add_notifier(NC_SCENE | ND_SEQUENCER, NULL);

    return smd;
  }
}

static void rna_Sequence_modifier_remove(Sequence *seq,
                                         bContext *C,
                                         ReportList *reports,
                                         PointerRNA *smd_ptr)
{
  SequenceModifierData *smd = smd_ptr->data;
  Scene *scene = CTX_data_scene(C);

  if (BKE_sequence_modifier_remove(seq, smd) == false) {
    BKE_report(reports, RPT_ERROR, "Modifier was not found in the stack");
    return;
  }

  RNA_POINTER_INVALIDATE(smd_ptr);
  BKE_sequence_invalidate_cache_for_modifier(scene, seq);

  WM_main_add_notifier(NC_SCENE | ND_SEQUENCER, NULL);
}

static void rna_Sequence_modifier_clear(Sequence *seq, bContext *C)
{
  Scene *scene = CTX_data_scene(C);

  BKE_sequence_modifier_clear(seq);

  BKE_sequence_invalidate_cache_for_modifier(scene, seq);

  WM_main_add_notifier(NC_SCENE | ND_SEQUENCER, NULL);
}

static float rna_Sequence_fps_get(PointerRNA *ptr)
{
  Scene *scene = (Scene *)ptr->id.data;
  Sequence *seq = (Sequence *)(ptr->data);
  return BKE_sequence_get_fps(scene, seq);
}

#else

static void rna_def_strip_element(BlenderRNA *brna)
{
  StructRNA *srna;
  PropertyRNA *prop;

  srna = RNA_def_struct(brna, "SequenceElement", NULL);
  RNA_def_struct_ui_text(srna, "Sequence Element", "Sequence strip data for a single frame");
  RNA_def_struct_sdna(srna, "StripElem");

  prop = RNA_def_property(srna, "filename", PROP_STRING, PROP_FILENAME);
  RNA_def_property_string_sdna(prop, NULL, "name");
  RNA_def_property_ui_text(prop, "Filename", "Name of the source file");
  RNA_def_property_update(prop, NC_SCENE | ND_SEQUENCER, "rna_SequenceElement_update");

  prop = RNA_def_property(srna, "orig_width", PROP_INT, PROP_NONE);
  RNA_def_property_int_sdna(prop, NULL, "orig_width");
  RNA_def_property_clear_flag(prop, PROP_EDITABLE);
  RNA_def_property_ui_text(prop, "Orig Width", "Original image width");

  prop = RNA_def_property(srna, "orig_height", PROP_INT, PROP_NONE);
  RNA_def_property_int_sdna(prop, NULL, "orig_height");
  RNA_def_property_clear_flag(prop, PROP_EDITABLE);
  RNA_def_property_ui_text(prop, "Orig Height", "Original image height");
}

static void rna_def_strip_crop(BlenderRNA *brna)
{
  StructRNA *srna;
  PropertyRNA *prop;

  srna = RNA_def_struct(brna, "SequenceCrop", NULL);
  RNA_def_struct_ui_text(srna, "Sequence Crop", "Cropping parameters for a sequence strip");
  RNA_def_struct_sdna(srna, "StripCrop");

  prop = RNA_def_property(srna, "max_y", PROP_INT, PROP_UNSIGNED);
  RNA_def_property_int_sdna(prop, NULL, "top");
  RNA_def_property_ui_text(prop, "Top", "Number of pixels to crop from the top");
  RNA_def_property_ui_range(prop, 0, 4096, 1, -1);
  RNA_def_property_update(prop, NC_SCENE | ND_SEQUENCER, "rna_SequenceCrop_update");

  prop = RNA_def_property(srna, "min_y", PROP_INT, PROP_UNSIGNED);
  RNA_def_property_int_sdna(prop, NULL, "bottom");
  RNA_def_property_ui_text(prop, "Bottom", "Number of pixels to crop from the bottom");
  RNA_def_property_ui_range(prop, 0, 4096, 1, -1);
  RNA_def_property_update(prop, NC_SCENE | ND_SEQUENCER, "rna_SequenceCrop_update");

  prop = RNA_def_property(srna, "min_x", PROP_INT, PROP_UNSIGNED);
  RNA_def_property_int_sdna(prop, NULL, "left");
  RNA_def_property_ui_text(prop, "Left", "Number of pixels to crop from the left side");
  RNA_def_property_ui_range(prop, 0, 4096, 1, -1);
  RNA_def_property_update(prop, NC_SCENE | ND_SEQUENCER, "rna_SequenceCrop_update");

  prop = RNA_def_property(srna, "max_x", PROP_INT, PROP_UNSIGNED);
  RNA_def_property_int_sdna(prop, NULL, "right");
  RNA_def_property_ui_text(prop, "Right", "Number of pixels to crop from the right side");
  RNA_def_property_ui_range(prop, 0, 4096, 1, -1);
  RNA_def_property_update(prop, NC_SCENE | ND_SEQUENCER, "rna_SequenceCrop_update");

  RNA_def_struct_path_func(srna, "rna_SequenceCrop_path");
}

static void rna_def_strip_transform(BlenderRNA *brna)
{
  StructRNA *srna;
  PropertyRNA *prop;

  srna = RNA_def_struct(brna, "SequenceTransform", NULL);
  RNA_def_struct_ui_text(srna, "Sequence Transform", "Transform parameters for a sequence strip");
  RNA_def_struct_sdna(srna, "StripTransform");

  prop = RNA_def_property(srna, "offset_x", PROP_INT, PROP_NONE);
  RNA_def_property_int_sdna(prop, NULL, "xofs");
  RNA_def_property_ui_text(
      prop, "Offset X", "Amount to move the input on the X axis within its boundaries");
  RNA_def_property_ui_range(prop, -4096, 4096, 1, -1);
  RNA_def_property_update(prop, NC_SCENE | ND_SEQUENCER, "rna_SequenceTransform_update");

  prop = RNA_def_property(srna, "offset_y", PROP_INT, PROP_NONE);
  RNA_def_property_int_sdna(prop, NULL, "yofs");
  RNA_def_property_ui_text(
      prop, "Offset Y", "Amount to move the input on the Y axis within its boundaries");
  RNA_def_property_ui_range(prop, -4096, 4096, 1, -1);
  RNA_def_property_update(prop, NC_SCENE | ND_SEQUENCER, "rna_SequenceTransform_update");

  RNA_def_struct_path_func(srna, "rna_SequenceTransform_path");
}

static void rna_def_strip_proxy(BlenderRNA *brna)
{
  StructRNA *srna;
  PropertyRNA *prop;

  static const EnumPropertyItem seq_tc_items[] = {
      {SEQ_PROXY_TC_NONE, "NONE", 0, "No TC in use", ""},
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
      {0, NULL, 0, NULL, NULL},
  };

  srna = RNA_def_struct(brna, "SequenceProxy", NULL);
  RNA_def_struct_ui_text(srna, "Sequence Proxy", "Proxy parameters for a sequence strip");
  RNA_def_struct_sdna(srna, "StripProxy");

  prop = RNA_def_property(srna, "directory", PROP_STRING, PROP_DIRPATH);
  RNA_def_property_string_sdna(prop, NULL, "dir");
  RNA_def_property_ui_text(prop, "Directory", "Location to store the proxy files");
  RNA_def_property_update(prop, NC_SCENE | ND_SEQUENCER, "rna_SequenceProxy_update");

  prop = RNA_def_property(srna, "filepath", PROP_STRING, PROP_FILEPATH);
  RNA_def_property_ui_text(prop, "Path", "Location of custom proxy file");
  RNA_def_property_string_funcs(prop,
                                "rna_Sequence_proxy_filepath_get",
                                "rna_Sequence_proxy_filepath_length",
                                "rna_Sequence_proxy_filepath_set");

  RNA_def_property_update(prop, NC_SCENE | ND_SEQUENCER, "rna_SequenceProxy_update");

  prop = RNA_def_property(srna, "use_overwrite", PROP_BOOLEAN, PROP_NONE);
  RNA_def_property_boolean_negative_sdna(prop, NULL, "build_flags", SEQ_PROXY_SKIP_EXISTING);
  RNA_def_property_ui_text(prop, "Overwrite", "Overwrite existing proxy files when building");

  prop = RNA_def_property(srna, "build_25", PROP_BOOLEAN, PROP_NONE);
  RNA_def_property_boolean_sdna(prop, NULL, "build_size_flags", SEQ_PROXY_IMAGE_SIZE_25);
  RNA_def_property_ui_text(prop, "25%", "Build 25% proxy resolution");

  prop = RNA_def_property(srna, "build_50", PROP_BOOLEAN, PROP_NONE);
  RNA_def_property_boolean_sdna(prop, NULL, "build_size_flags", SEQ_PROXY_IMAGE_SIZE_50);
  RNA_def_property_ui_text(prop, "50%", "Build 50% proxy resolution");

  prop = RNA_def_property(srna, "build_75", PROP_BOOLEAN, PROP_NONE);
  RNA_def_property_boolean_sdna(prop, NULL, "build_size_flags", SEQ_PROXY_IMAGE_SIZE_75);
  RNA_def_property_ui_text(prop, "75%", "Build 75% proxy resolution");

  prop = RNA_def_property(srna, "build_100", PROP_BOOLEAN, PROP_NONE);
  RNA_def_property_boolean_sdna(prop, NULL, "build_size_flags", SEQ_PROXY_IMAGE_SIZE_100);
  RNA_def_property_ui_text(prop, "100%", "Build 100% proxy resolution");

  prop = RNA_def_property(srna, "build_record_run", PROP_BOOLEAN, PROP_NONE);
  RNA_def_property_boolean_sdna(prop, NULL, "build_tc_flags", SEQ_PROXY_TC_RECORD_RUN);
  RNA_def_property_ui_text(prop, "Rec Run", "Build record run time code index");

  prop = RNA_def_property(srna, "build_free_run", PROP_BOOLEAN, PROP_NONE);
  RNA_def_property_boolean_sdna(prop, NULL, "build_tc_flags", SEQ_PROXY_TC_FREE_RUN);
  RNA_def_property_ui_text(prop, "Free Run", "Build free run time code index");

  prop = RNA_def_property(srna, "build_free_run_rec_date", PROP_BOOLEAN, PROP_NONE);
  RNA_def_property_boolean_sdna(
      prop, NULL, "build_tc_flags", SEQ_PROXY_TC_INTERP_REC_DATE_FREE_RUN);
  RNA_def_property_ui_text(
      prop, "Free Run (Rec Date)", "Build free run time code index using Record Date/Time");

  prop = RNA_def_property(srna, "quality", PROP_INT, PROP_UNSIGNED);
  RNA_def_property_int_sdna(prop, NULL, "quality");
  RNA_def_property_ui_text(prop, "Quality", "JPEG Quality of proxies to build");
  RNA_def_property_ui_range(prop, 1, 100, 1, -1);

  prop = RNA_def_property(srna, "timecode", PROP_ENUM, PROP_NONE);
  RNA_def_property_enum_sdna(prop, NULL, "tc");
  RNA_def_property_enum_items(prop, seq_tc_items);
  RNA_def_property_ui_text(prop, "Timecode", "Method for reading the inputs timecode");
  RNA_def_property_update(prop, NC_SCENE | ND_SEQUENCER, "rna_Sequence_tcindex_update");

  prop = RNA_def_property(srna, "use_proxy_custom_directory", PROP_BOOLEAN, PROP_NONE);
  RNA_def_property_boolean_sdna(prop, NULL, "storage", SEQ_STORAGE_PROXY_CUSTOM_DIR);
  RNA_def_property_ui_text(prop, "Proxy Custom Directory", "Use a custom directory to store data");
  RNA_def_property_update(prop, NC_SCENE | ND_SEQUENCER, "rna_Sequence_update");

  prop = RNA_def_property(srna, "use_proxy_custom_file", PROP_BOOLEAN, PROP_NONE);
  RNA_def_property_boolean_sdna(prop, NULL, "storage", SEQ_STORAGE_PROXY_CUSTOM_FILE);
  RNA_def_property_ui_text(prop, "Proxy Custom File", "Use a custom file to read proxy data from");
  RNA_def_property_update(prop, NC_SCENE | ND_SEQUENCER, "rna_Sequence_update");
}

static void rna_def_color_balance(BlenderRNA *brna)
{
  StructRNA *srna;
  PropertyRNA *prop;

  srna = RNA_def_struct(brna, "SequenceColorBalanceData", NULL);
  RNA_def_struct_ui_text(srna,
                         "Sequence Color Balance Data",
                         "Color balance parameters for a sequence strip and it's modifiers");
  RNA_def_struct_sdna(srna, "StripColorBalance");

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

  prop = RNA_def_property(srna, "invert_gain", PROP_BOOLEAN, PROP_NONE);
  RNA_def_property_boolean_sdna(prop, NULL, "flag", SEQ_COLOR_BALANCE_INVERSE_GAIN);
  RNA_def_property_ui_text(prop, "Inverse Gain", "Invert the gain color`");
  RNA_def_property_update(prop, NC_SCENE | ND_SEQUENCER, "rna_SequenceColorBalance_update");

  prop = RNA_def_property(srna, "invert_gamma", PROP_BOOLEAN, PROP_NONE);
  RNA_def_property_boolean_sdna(prop, NULL, "flag", SEQ_COLOR_BALANCE_INVERSE_GAMMA);
  RNA_def_property_ui_text(prop, "Inverse Gamma", "Invert the gamma color");
  RNA_def_property_update(prop, NC_SCENE | ND_SEQUENCER, "rna_SequenceColorBalance_update");

  prop = RNA_def_property(srna, "invert_lift", PROP_BOOLEAN, PROP_NONE);
  RNA_def_property_boolean_sdna(prop, NULL, "flag", SEQ_COLOR_BALANCE_INVERSE_LIFT);
  RNA_def_property_ui_text(prop, "Inverse Lift", "Invert the lift color");
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
    {0, "", ICON_NONE, NULL, NULL},
    {SEQ_TYPE_DARKEN, "DARKEN", 0, "Darken", ""},
    {SEQ_TYPE_MUL, "MULTIPLY", 0, "Multiply", ""},
    {SEQ_TYPE_BURN, "BURN", 0, "Burn", ""},
    {SEQ_TYPE_LINEAR_BURN, "LINEAR_BURN", 0, "Linear Burn", ""},
    {0, "", ICON_NONE, NULL, NULL},
    {SEQ_TYPE_LIGHTEN, "LIGHTEN", 0, "Lighten", ""},
    {SEQ_TYPE_SCREEN, "SCREEN", 0, "Screen", ""},
    {SEQ_TYPE_DODGE, "DODGE", 0, "Dodge", ""},
    {SEQ_TYPE_ADD, "ADD", 0, "Add", ""},
    {0, "", ICON_NONE, NULL, NULL},
    {SEQ_TYPE_OVERLAY, "OVERLAY", 0, "Overlay", ""},
    {SEQ_TYPE_SOFT_LIGHT, "SOFT_LIGHT", 0, "Soft Light", ""},
    {SEQ_TYPE_HARD_LIGHT, "HARD_LIGHT", 0, "Hard Light", ""},
    {SEQ_TYPE_VIVID_LIGHT, "VIVID_LIGHT", 0, "Vivid Light", ""},
    {SEQ_TYPE_LIN_LIGHT, "LINEAR_LIGHT", 0, "Linear Light", ""},
    {SEQ_TYPE_PIN_LIGHT, "PIN_LIGHT", 0, "Pin Light", ""},
    {0, "", ICON_NONE, NULL, NULL},
    {SEQ_TYPE_DIFFERENCE, "DIFFERENCE", 0, "Difference", ""},
    {SEQ_TYPE_EXCLUSION, "EXCLUSION", 0, "Exclusion", ""},
    {SEQ_TYPE_SUB, "SUBTRACT", 0, "Subtract", ""},
    {0, "", ICON_NONE, NULL, NULL},
    {SEQ_TYPE_HUE, "HUE", 0, "Hue", ""},
    {SEQ_TYPE_SATURATION, "SATURATION", 0, "Saturation", ""},
    {SEQ_TYPE_BLEND_COLOR, "COLOR", 0, "Color", ""},
    {SEQ_TYPE_VALUE, "VALUE", 0, "Value", ""},
    {0, "", ICON_NONE, NULL, NULL},
    {SEQ_TYPE_ALPHAOVER, "ALPHA_OVER", 0, "Alpha Over", ""},
    {SEQ_TYPE_ALPHAUNDER, "ALPHA_UNDER", 0, "Alpha Under", ""},
    {SEQ_TYPE_GAMCROSS, "GAMMA_CROSS", 0, "Gamma Cross", ""},
    {SEQ_TYPE_OVERDROP, "OVER_DROP", 0, "Over Drop", ""},
    {0, NULL, 0, NULL, NULL},
};

static void rna_def_sequence_modifiers(BlenderRNA *brna, PropertyRNA *cprop)
{
  StructRNA *srna;

  FunctionRNA *func;
  PropertyRNA *parm;

  RNA_def_property_srna(cprop, "SequenceModifiers");
  srna = RNA_def_struct(brna, "SequenceModifiers", NULL);
  RNA_def_struct_sdna(srna, "Sequence");
  RNA_def_struct_ui_text(srna, "Strip Modifiers", "Collection of strip modifiers");

  /* add modifier */
  func = RNA_def_function(srna, "new", "rna_Sequence_modifier_new");
  RNA_def_function_flag(func, FUNC_USE_CONTEXT | FUNC_USE_REPORTS);
  RNA_def_function_ui_description(func, "Add a new modifier");
  parm = RNA_def_string(func, "name", "Name", 0, "", "New name for the modifier");
  RNA_def_parameter_flags(parm, 0, PARM_REQUIRED);
  /* modifier to add */
  parm = RNA_def_enum(func,
                      "type",
                      rna_enum_sequence_modifier_type_items,
                      seqModifierType_ColorBalance,
                      "",
                      "Modifier type to add");
  RNA_def_parameter_flags(parm, 0, PARM_REQUIRED);
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
  RNA_def_parameter_clear_flags(parm, PROP_THICK_WRAP, 0);

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
      {0, NULL, 0, NULL, NULL},
  };

  srna = RNA_def_struct(brna, "Sequence", NULL);
  RNA_def_struct_ui_text(srna, "Sequence", "Sequence strip in the sequence editor");
  RNA_def_struct_refine_func(srna, "rna_Sequence_refine");
  RNA_def_struct_path_func(srna, "rna_Sequence_path");
  RNA_def_struct_idprops_func(srna, "rna_Sequence_idprops");

  prop = RNA_def_property(srna, "name", PROP_STRING, PROP_NONE);
  RNA_def_property_string_funcs(
      prop, "rna_Sequence_name_get", "rna_Sequence_name_length", "rna_Sequence_name_set");
  RNA_def_property_string_maxlength(prop, sizeof(((Sequence *)NULL)->name) - 2);
  RNA_def_property_ui_text(prop, "Name", "");
  RNA_def_struct_name_property(srna, prop);
  RNA_def_property_update(prop, NC_SCENE | ND_SEQUENCER, NULL);

  prop = RNA_def_property(srna, "type", PROP_ENUM, PROP_NONE);
  RNA_def_property_clear_flag(prop, PROP_EDITABLE);
  RNA_def_property_enum_items(prop, seq_type_items);
  RNA_def_property_ui_text(prop, "Type", "");
  RNA_def_property_translation_context(prop, BLT_I18NCONTEXT_ID_SEQUENCE);
  RNA_def_property_update(prop, NC_SCENE | ND_SEQUENCER, "rna_Sequence_update");

  /* flags */
  prop = RNA_def_property(srna, "select", PROP_BOOLEAN, PROP_NONE);
  RNA_def_property_boolean_sdna(prop, NULL, "flag", SELECT);
  RNA_def_property_ui_text(prop, "Select", "");
  RNA_def_property_update(prop, NC_SCENE | ND_SEQUENCER | NA_SELECTED, NULL);

  prop = RNA_def_property(srna, "select_left_handle", PROP_BOOLEAN, PROP_NONE);
  RNA_def_property_boolean_sdna(prop, NULL, "flag", SEQ_LEFTSEL);
  RNA_def_property_ui_text(prop, "Left Handle Selected", "");
  RNA_def_property_update(prop, NC_SCENE | ND_SEQUENCER | NA_SELECTED, NULL);

  prop = RNA_def_property(srna, "select_right_handle", PROP_BOOLEAN, PROP_NONE);
  RNA_def_property_boolean_sdna(prop, NULL, "flag", SEQ_RIGHTSEL);
  RNA_def_property_ui_text(prop, "Right Handle Selected", "");
  RNA_def_property_update(prop, NC_SCENE | ND_SEQUENCER | NA_SELECTED, NULL);

  prop = RNA_def_property(srna, "mute", PROP_BOOLEAN, PROP_NONE);
  RNA_def_property_boolean_sdna(prop, NULL, "flag", SEQ_MUTE);
  RNA_def_property_ui_icon(prop, ICON_HIDE_OFF, -1);
  RNA_def_property_ui_text(
      prop, "Mute", "Disable strip so that it cannot be viewed in the output");
  RNA_def_property_update(prop, NC_SCENE | ND_SEQUENCER, "rna_Sequence_mute_update");

  prop = RNA_def_property(srna, "lock", PROP_BOOLEAN, PROP_NONE);
  RNA_def_property_boolean_sdna(prop, NULL, "flag", SEQ_LOCK);
  RNA_def_property_clear_flag(prop, PROP_ANIMATABLE);
  RNA_def_property_ui_icon(prop, ICON_UNLOCKED, true);
  RNA_def_property_ui_text(prop, "Lock", "Lock strip so that it cannot be transformed");
  RNA_def_property_update(prop, NC_SCENE | ND_SEQUENCER, NULL);

  /* strip positioning */
  prop = RNA_def_property(srna, "frame_final_duration", PROP_INT, PROP_TIME);
  RNA_def_property_range(prop, 1, MAXFRAME);
  RNA_def_property_clear_flag(prop, PROP_ANIMATABLE);
  RNA_def_property_ui_text(
      prop, "Length", "The length of the contents of this strip after the handles are applied");
  RNA_def_property_int_funcs(
      prop, "rna_Sequence_frame_length_get", "rna_Sequence_frame_length_set", NULL);
  RNA_def_property_editable_func(prop, "rna_Sequence_frame_editable");
  RNA_def_property_update(prop, NC_SCENE | ND_SEQUENCER, "rna_Sequence_update");

  prop = RNA_def_property(srna, "frame_duration", PROP_INT, PROP_TIME);
  RNA_def_property_int_sdna(prop, NULL, "len");
  RNA_def_property_clear_flag(prop, PROP_EDITABLE | PROP_ANIMATABLE);
  RNA_def_property_range(prop, 1, MAXFRAME);
  RNA_def_property_ui_text(
      prop, "Length", "The length of the contents of this strip before the handles are applied");

  prop = RNA_def_property(srna, "frame_start", PROP_INT, PROP_TIME);
  RNA_def_property_int_sdna(prop, NULL, "start");
  RNA_def_property_clear_flag(prop, PROP_ANIMATABLE);
  RNA_def_property_ui_text(prop, "Start Frame", "X position where the strip begins");
  RNA_def_property_int_funcs(
      prop, NULL, "rna_Sequence_start_frame_set", NULL); /* overlap tests and calc_seq_disp */
  RNA_def_property_editable_func(prop, "rna_Sequence_frame_editable");
  RNA_def_property_update(prop, NC_SCENE | ND_SEQUENCER, "rna_Sequence_update");

  prop = RNA_def_property(srna, "frame_final_start", PROP_INT, PROP_TIME);
  RNA_def_property_int_sdna(prop, NULL, "startdisp");
  RNA_def_property_clear_flag(prop, PROP_ANIMATABLE);
  RNA_def_property_ui_text(
      prop,
      "Start Frame",
      "Start frame displayed in the sequence editor after offsets are applied, setting this is "
      "equivalent to moving the handle, not the actual start frame");
  /* overlap tests and calc_seq_disp */
  RNA_def_property_int_funcs(prop, NULL, "rna_Sequence_start_frame_final_set", NULL);
  RNA_def_property_editable_func(prop, "rna_Sequence_frame_editable");
  RNA_def_property_update(prop, NC_SCENE | ND_SEQUENCER, "rna_Sequence_update");

  prop = RNA_def_property(srna, "frame_final_end", PROP_INT, PROP_TIME);
  RNA_def_property_int_sdna(prop, NULL, "enddisp");
  RNA_def_property_clear_flag(prop, PROP_ANIMATABLE);
  RNA_def_property_ui_text(
      prop, "End Frame", "End frame displayed in the sequence editor after offsets are applied");
  /* overlap tests and calc_seq_disp */
  RNA_def_property_int_funcs(prop, NULL, "rna_Sequence_end_frame_final_set", NULL);
  RNA_def_property_editable_func(prop, "rna_Sequence_frame_editable");
  RNA_def_property_update(prop, NC_SCENE | ND_SEQUENCER, "rna_Sequence_update");

  prop = RNA_def_property(srna, "frame_offset_start", PROP_INT, PROP_TIME);
  RNA_def_property_int_sdna(prop, NULL, "startofs");
  //  RNA_def_property_clear_flag(prop, PROP_EDITABLE); /* overlap tests */
  RNA_def_property_ui_text(prop, "Start Offset", "");
  RNA_def_property_int_funcs(prop, NULL, NULL, "rna_Sequence_frame_offset_range");
  RNA_def_property_update(prop, NC_SCENE | ND_SEQUENCER, "rna_Sequence_frame_change_update");

  prop = RNA_def_property(srna, "frame_offset_end", PROP_INT, PROP_TIME);
  RNA_def_property_int_sdna(prop, NULL, "endofs");
  //  RNA_def_property_clear_flag(prop, PROP_EDITABLE); /* overlap tests */
  RNA_def_property_ui_text(prop, "End Offset", "");
  RNA_def_property_int_funcs(prop, NULL, NULL, "rna_Sequence_frame_offset_range");
  RNA_def_property_update(prop, NC_SCENE | ND_SEQUENCER, "rna_Sequence_frame_change_update");

  prop = RNA_def_property(srna, "frame_still_start", PROP_INT, PROP_TIME);
  RNA_def_property_int_sdna(prop, NULL, "startstill");
  //  RNA_def_property_clear_flag(prop, PROP_EDITABLE); /* overlap tests */
  RNA_def_property_range(prop, 0, MAXFRAME);
  RNA_def_property_ui_text(prop, "Start Still", "");
  RNA_def_property_update(prop, NC_SCENE | ND_SEQUENCER, "rna_Sequence_frame_change_update");

  prop = RNA_def_property(srna, "frame_still_end", PROP_INT, PROP_TIME);
  RNA_def_property_int_sdna(prop, NULL, "endstill");
  //  RNA_def_property_clear_flag(prop, PROP_EDITABLE); /* overlap tests */
  RNA_def_property_range(prop, 0, MAXFRAME);
  RNA_def_property_ui_text(prop, "End Still", "");
  RNA_def_property_update(prop, NC_SCENE | ND_SEQUENCER, "rna_Sequence_frame_change_update");

  prop = RNA_def_property(srna, "channel", PROP_INT, PROP_UNSIGNED);
  RNA_def_property_int_sdna(prop, NULL, "machine");
  RNA_def_property_clear_flag(prop, PROP_ANIMATABLE);
  RNA_def_property_range(prop, 1, MAXSEQ);
  RNA_def_property_ui_text(prop, "Channel", "Y position of the sequence strip");
  RNA_def_property_int_funcs(prop, NULL, "rna_Sequence_channel_set", NULL); /* overlap test */
  RNA_def_property_update(prop, NC_SCENE | ND_SEQUENCER, "rna_Sequence_update");

  prop = RNA_def_property(srna, "use_linear_modifiers", PROP_BOOLEAN, PROP_NONE);
  RNA_def_property_boolean_sdna(prop, NULL, "flag", SEQ_USE_LINEAR_MODIFIERS);
  RNA_def_property_ui_text(prop,
                           "Use Linear Modifiers",
                           "Calculate modifiers in linear space instead of sequencer's space");
  RNA_def_property_update(prop, NC_SCENE | ND_SEQUENCER, "rna_Sequence_update");

  /* blending */

  prop = RNA_def_property(srna, "blend_type", PROP_ENUM, PROP_NONE);
  RNA_def_property_enum_sdna(prop, NULL, "blend_mode");
  RNA_def_property_enum_items(prop, blend_mode_items);
  RNA_def_property_ui_text(
      prop, "Blend Mode", "Method for controlling how the strip combines with other strips");
  RNA_def_property_update(prop, NC_SCENE | ND_SEQUENCER, "rna_Sequence_update");

  prop = RNA_def_property(srna, "blend_alpha", PROP_FLOAT, PROP_FACTOR);
  RNA_def_property_range(prop, 0.0f, 1.0f);
  RNA_def_property_ui_text(
      prop, "Blend Opacity", "Percentage of how much the strip's colors affect other strips");
  /* stupid 0-100 -> 0-1 */
  RNA_def_property_float_funcs(prop, "rna_Sequence_opacity_get", "rna_Sequence_opacity_set", NULL);
  RNA_def_property_update(prop, NC_SCENE | ND_SEQUENCER, "rna_Sequence_update");

  prop = RNA_def_property(srna, "effect_fader", PROP_FLOAT, PROP_FACTOR);
  RNA_def_property_range(prop, 0.0f, 1.0f);
  RNA_def_property_ui_range(prop, 0.0f, 1.0f, 0.1, 3);
  RNA_def_property_float_sdna(prop, NULL, "effect_fader");
  RNA_def_property_ui_text(prop, "Effect fader position", "Custom fade value");
  RNA_def_property_update(prop, NC_SCENE | ND_SEQUENCER, "rna_Sequence_update");

  prop = RNA_def_property(srna, "use_default_fade", PROP_BOOLEAN, PROP_NONE);
  RNA_def_property_boolean_sdna(prop, NULL, "flag", SEQ_USE_EFFECT_DEFAULT_FADE);
  RNA_def_property_ui_text(
      prop,
      "Use Default Fade",
      "Fade effect using the built-in default (usually make transition as long as "
      "effect strip)");
  RNA_def_property_update(prop, NC_SCENE | ND_SEQUENCER, "rna_Sequence_update");

  prop = RNA_def_property(srna, "speed_factor", PROP_FLOAT, PROP_NONE);
  RNA_def_property_float_sdna(prop, NULL, "speed_fader");
  RNA_def_property_ui_text(
      prop,
      "Speed factor",
      "Multiply the current speed of the sequence with this number or remap current frame "
      "to this frame");
  RNA_def_property_update(prop, NC_SCENE | ND_SEQUENCER, "rna_Sequence_update");

  /* modifiers */
  prop = RNA_def_property(srna, "modifiers", PROP_COLLECTION, PROP_NONE);
  RNA_def_property_struct_type(prop, "SequenceModifier");
  RNA_def_property_ui_text(prop, "Modifiers", "Modifiers affecting this strip");
  rna_def_sequence_modifiers(brna, prop);

  prop = RNA_def_property(srna, "cache_raw", PROP_BOOLEAN, PROP_NONE);
  RNA_def_property_boolean_sdna(prop, NULL, "cache_flag", SEQ_CACHE_STORE_RAW);
  RNA_def_property_ui_text(prop,
                           "Cache Raw",
                           "Cache raw images read from disk, for faster tweaking of strip "
                           "parameters at the cost of memory usage");

  prop = RNA_def_property(srna, "cache_preprocessed", PROP_BOOLEAN, PROP_NONE);
  RNA_def_property_boolean_sdna(prop, NULL, "cache_flag", SEQ_CACHE_STORE_PREPROCESSED);
  RNA_def_property_ui_text(
      prop,
      "Cache Rreprocessed",
      "Cache preprocessed images, for faster tweaking of effects at the cost of memory usage");

  prop = RNA_def_property(srna, "cache_composite", PROP_BOOLEAN, PROP_NONE);
  RNA_def_property_boolean_sdna(prop, NULL, "cache_flag", SEQ_CACHE_STORE_COMPOSITE);
  RNA_def_property_ui_text(prop,
                           "Cache Composite",
                           "Cache intermediate composited images, for faster tweaking of stacked "
                           "strips at the cost of memory usage");

  prop = RNA_def_property(srna, "override_cache_settings", PROP_BOOLEAN, PROP_NONE);
  RNA_def_property_boolean_sdna(prop, NULL, "cache_flag", SEQ_CACHE_OVERRIDE);
  RNA_def_property_ui_text(prop, "Override Cache Settings", "Override global cache settings");

  RNA_api_sequence_strip(srna);
}

static void rna_def_editor(BlenderRNA *brna)
{
  StructRNA *srna;
  PropertyRNA *prop;

  static const EnumPropertyItem editing_storage_items[] = {
      {0, "PER_STRIP", 0, "Per Strip", "Store proxies using per strip settings"},
      {SEQ_EDIT_PROXY_DIR_STORAGE,
       "PROJECT",
       0,
       "Project",
       "Store proxies using project directory"},
      {0, NULL, 0, NULL, NULL},
  };
  srna = RNA_def_struct(brna, "SequenceEditor", NULL);
  RNA_def_struct_ui_text(srna, "Sequence Editor", "Sequence editing data for a Scene data-block");
  RNA_def_struct_ui_icon(srna, ICON_SEQUENCE);
  RNA_def_struct_sdna(srna, "Editing");

  prop = RNA_def_property(srna, "sequences", PROP_COLLECTION, PROP_NONE);
  RNA_def_property_collection_sdna(prop, NULL, "seqbase", NULL);
  RNA_def_property_struct_type(prop, "Sequence");
  RNA_def_property_ui_text(prop, "Sequences", "Top-level strips only");
  RNA_api_sequences(brna, prop);

  prop = RNA_def_property(srna, "sequences_all", PROP_COLLECTION, PROP_NONE);
  RNA_def_property_collection_sdna(prop, NULL, "seqbase", NULL);
  RNA_def_property_struct_type(prop, "Sequence");
  RNA_def_property_ui_text(
      prop, "All Sequences", "All strips, recursively including those inside metastrips");
  RNA_def_property_collection_funcs(prop,
                                    "rna_SequenceEditor_sequences_all_begin",
                                    "rna_SequenceEditor_sequences_all_next",
                                    NULL,
                                    NULL,
                                    NULL,
                                    NULL,
                                    NULL,
                                    NULL);

  prop = RNA_def_property(srna, "meta_stack", PROP_COLLECTION, PROP_NONE);
  RNA_def_property_collection_sdna(prop, NULL, "metastack", NULL);
  RNA_def_property_struct_type(prop, "Sequence");
  RNA_def_property_ui_text(
      prop, "Meta Stack", "Meta strip stack, last is currently edited meta strip");
  RNA_def_property_collection_funcs(
      prop, NULL, NULL, NULL, "rna_SequenceEditor_meta_stack_get", NULL, NULL, NULL, NULL);

  prop = RNA_def_property(srna, "active_strip", PROP_POINTER, PROP_NONE);
  RNA_def_property_pointer_sdna(prop, NULL, "act_seq");
  RNA_def_property_flag(prop, PROP_EDITABLE);
  RNA_def_property_ui_text(prop, "Active Strip", "Sequencer's active strip");

  prop = RNA_def_property(srna, "show_overlay", PROP_BOOLEAN, PROP_NONE);
  RNA_def_property_boolean_sdna(prop, NULL, "over_flag", SEQ_EDIT_OVERLAY_SHOW);
  RNA_def_property_ui_text(prop, "Draw Axes", "Partial overlay on top of the sequencer");
  RNA_def_property_update(prop, NC_SPACE | ND_SPACE_SEQUENCER, NULL);

  prop = RNA_def_property(srna, "use_overlay_lock", PROP_BOOLEAN, PROP_NONE);
  RNA_def_property_boolean_sdna(prop, NULL, "over_flag", SEQ_EDIT_OVERLAY_ABS);
  RNA_def_property_ui_text(prop, "Overlay Lock", "");
  RNA_def_property_boolean_funcs(prop, NULL, "rna_SequenceEditor_overlay_lock_set");
  RNA_def_property_update(prop, NC_SPACE | ND_SPACE_SEQUENCER, NULL);

  /* access to fixed and relative frame */
  prop = RNA_def_property(srna, "overlay_frame", PROP_INT, PROP_NONE);
  RNA_def_property_ui_text(prop, "Overlay Offset", "");
  RNA_def_property_int_funcs(
      prop, "rna_SequenceEditor_overlay_frame_get", "rna_SequenceEditor_overlay_frame_set", NULL);
  RNA_def_property_update(prop, NC_SPACE | ND_SPACE_SEQUENCER, NULL);

  prop = RNA_def_property(srna, "proxy_storage", PROP_ENUM, PROP_NONE);
  RNA_def_property_enum_items(prop, editing_storage_items);
  RNA_def_property_ui_text(prop, "Proxy Storage", "How to store proxies for this project");
  RNA_def_property_update(prop, NC_SPACE | ND_SPACE_SEQUENCER, "rna_SequenceEditor_update_cache");

  prop = RNA_def_property(srna, "proxy_dir", PROP_STRING, PROP_DIRPATH);
  RNA_def_property_string_sdna(prop, NULL, "proxy_dir");
  RNA_def_property_ui_text(prop, "Proxy Directory", "");
  RNA_def_property_update(prop, NC_SPACE | ND_SPACE_SEQUENCER, "rna_SequenceEditor_update_cache");

  /* cache flags */

  prop = RNA_def_property(srna, "show_cache", PROP_BOOLEAN, PROP_NONE);
  RNA_def_property_boolean_sdna(prop, NULL, "cache_flag", SEQ_CACHE_VIEW_ENABLE);
  RNA_def_property_ui_text(prop, "Show Cache", "Visualize cached images on the timeline");
  RNA_def_property_update(prop, NC_SCENE | ND_SEQUENCER, NULL);

  prop = RNA_def_property(srna, "show_cache_final_out", PROP_BOOLEAN, PROP_NONE);
  RNA_def_property_boolean_sdna(prop, NULL, "cache_flag", SEQ_CACHE_VIEW_FINAL_OUT);
  RNA_def_property_ui_text(prop, "Final Images", "Visualize cached complete frames");
  RNA_def_property_update(prop, NC_SCENE | ND_SEQUENCER, NULL);

  prop = RNA_def_property(srna, "show_cache_raw", PROP_BOOLEAN, PROP_NONE);
  RNA_def_property_boolean_sdna(prop, NULL, "cache_flag", SEQ_CACHE_VIEW_RAW);
  RNA_def_property_ui_text(prop, "Raw Images", "Visualize cached raw images");
  RNA_def_property_update(prop, NC_SCENE | ND_SEQUENCER, NULL);

  prop = RNA_def_property(srna, "show_cache_preprocessed", PROP_BOOLEAN, PROP_NONE);
  RNA_def_property_boolean_sdna(prop, NULL, "cache_flag", SEQ_CACHE_VIEW_PREPROCESSED);
  RNA_def_property_ui_text(prop, "Preprocessed Images", "Visualize cached preprocessed images");
  RNA_def_property_update(prop, NC_SCENE | ND_SEQUENCER, NULL);

  prop = RNA_def_property(srna, "show_cache_composite", PROP_BOOLEAN, PROP_NONE);
  RNA_def_property_boolean_sdna(prop, NULL, "cache_flag", SEQ_CACHE_VIEW_COMPOSITE);
  RNA_def_property_ui_text(prop, "Composite Images", "Visualize cached composite images");
  RNA_def_property_update(prop, NC_SCENE | ND_SEQUENCER, NULL);

  prop = RNA_def_property(srna, "cache_raw", PROP_BOOLEAN, PROP_NONE);
  RNA_def_property_boolean_sdna(prop, NULL, "cache_flag", SEQ_CACHE_STORE_RAW);
  RNA_def_property_ui_text(prop,
                           "Cache Raw",
                           "Cache raw images read from disk, for faster tweaking of strip "
                           "parameters at the cost of memory usage");

  prop = RNA_def_property(srna, "cache_preprocessed", PROP_BOOLEAN, PROP_NONE);
  RNA_def_property_boolean_sdna(prop, NULL, "cache_flag", SEQ_CACHE_STORE_PREPROCESSED);
  RNA_def_property_ui_text(
      prop,
      "Cache Preprocessed",
      "Cache preprocessed images, for faster tweaking of effects at the cost of memory usage");

  prop = RNA_def_property(srna, "cache_composite", PROP_BOOLEAN, PROP_NONE);
  RNA_def_property_boolean_sdna(prop, NULL, "cache_flag", SEQ_CACHE_STORE_COMPOSITE);
  RNA_def_property_ui_text(prop,
                           "Cache Composite",
                           "Cache intermediate composited images, for faster tweaking of stacked "
                           "strips at the cost of memory usage");

  prop = RNA_def_property(srna, "cache_final", PROP_BOOLEAN, PROP_NONE);
  RNA_def_property_boolean_sdna(prop, NULL, "cache_flag", SEQ_CACHE_STORE_FINAL_OUT);
  RNA_def_property_ui_text(prop, "Cache Final", "Cache final image for each frame");

  prop = RNA_def_property(srna, "recycle_max_cost", PROP_FLOAT, PROP_NONE);
  RNA_def_property_range(prop, 0.0f, SEQ_CACHE_COST_MAX);
  RNA_def_property_ui_range(prop, 0.0f, SEQ_CACHE_COST_MAX, 0.1f, 1);
  RNA_def_property_float_sdna(prop, NULL, "recycle_max_cost");
  RNA_def_property_ui_text(
      prop, "Recycle Up To Cost", "Only frames with cost lower than this value will be recycled");
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
      {0, NULL, 0, NULL, NULL},
  };

  prop = RNA_def_property(srna, "use_deinterlace", PROP_BOOLEAN, PROP_NONE);
  RNA_def_property_boolean_sdna(prop, NULL, "flag", SEQ_FILTERY);
  RNA_def_property_ui_text(prop, "Deinterlace", "Remove fields from video movies");
  RNA_def_property_update(prop, NC_SCENE | ND_SEQUENCER, "rna_Sequence_update_reopen_files");

  prop = RNA_def_property(srna, "alpha_mode", PROP_ENUM, PROP_NONE);
  RNA_def_property_enum_items(prop, alpha_mode_items);
  RNA_def_property_ui_text(
      prop, "Alpha Mode", "Representation of alpha information in the RGBA pixels");
  RNA_def_property_update(prop, NC_SCENE | ND_SEQUENCER, "rna_Sequence_update");

  prop = RNA_def_property(srna, "use_flip_x", PROP_BOOLEAN, PROP_NONE);
  RNA_def_property_boolean_sdna(prop, NULL, "flag", SEQ_FLIPX);
  RNA_def_property_ui_text(prop, "Flip X", "Flip on the X axis");
  RNA_def_property_update(prop, NC_SCENE | ND_SEQUENCER, "rna_Sequence_update");

  prop = RNA_def_property(srna, "use_flip_y", PROP_BOOLEAN, PROP_NONE);
  RNA_def_property_boolean_sdna(prop, NULL, "flag", SEQ_FLIPY);
  RNA_def_property_ui_text(prop, "Flip Y", "Flip on the Y axis");
  RNA_def_property_update(prop, NC_SCENE | ND_SEQUENCER, "rna_Sequence_update");

  prop = RNA_def_property(srna, "use_float", PROP_BOOLEAN, PROP_NONE);
  RNA_def_property_boolean_sdna(prop, NULL, "flag", SEQ_MAKE_FLOAT);
  RNA_def_property_ui_text(prop, "Convert Float", "Convert input to float data");
  RNA_def_property_update(prop, NC_SCENE | ND_SEQUENCER, "rna_Sequence_update");

  prop = RNA_def_property(srna, "use_reverse_frames", PROP_BOOLEAN, PROP_NONE);
  RNA_def_property_boolean_sdna(prop, NULL, "flag", SEQ_REVERSE_FRAMES);
  RNA_def_property_ui_text(prop, "Flip Time", "Reverse frame order");
  RNA_def_property_update(prop, NC_SCENE | ND_SEQUENCER, "rna_Sequence_update");

  prop = RNA_def_property(srna, "color_multiply", PROP_FLOAT, PROP_UNSIGNED);
  RNA_def_property_float_sdna(prop, NULL, "mul");
  RNA_def_property_range(prop, 0.0f, 20.0f);
  RNA_def_property_float_default(prop, 1.0f);
  RNA_def_property_ui_text(prop, "Multiply Colors", "");
  RNA_def_property_update(prop, NC_SCENE | ND_SEQUENCER, "rna_Sequence_update");

  prop = RNA_def_property(srna, "color_saturation", PROP_FLOAT, PROP_UNSIGNED);
  RNA_def_property_float_sdna(prop, NULL, "sat");
  RNA_def_property_range(prop, 0.0f, 20.0f);
  RNA_def_property_ui_range(prop, 0.0f, 2.0f, 3, 3);
  RNA_def_property_float_default(prop, 1.0f);
  RNA_def_property_ui_text(prop, "Saturation", "Adjust the intensity of the input's color");
  RNA_def_property_update(prop, NC_SCENE | ND_SEQUENCER, "rna_Sequence_update");

  prop = RNA_def_property(srna, "strobe", PROP_FLOAT, PROP_NONE);
  RNA_def_property_range(prop, 1.0f, 30.0f);
  RNA_def_property_ui_text(prop, "Strobe", "Only display every nth frame");
  RNA_def_property_update(prop, NC_SCENE | ND_SEQUENCER, "rna_Sequence_update");

  prop = RNA_def_property(srna, "use_translation", PROP_BOOLEAN, PROP_NONE);
  RNA_def_property_boolean_sdna(prop, NULL, "flag", SEQ_USE_TRANSFORM);
  RNA_def_property_ui_text(prop, "Use Translation", "Translate image before processing");
  RNA_def_property_boolean_funcs(prop, NULL, "rna_Sequence_use_translation_set");
  RNA_def_property_update(prop, NC_SCENE | ND_SEQUENCER, "rna_Sequence_update");

  prop = RNA_def_property(srna, "transform", PROP_POINTER, PROP_NONE);
  RNA_def_property_pointer_sdna(prop, NULL, "strip->transform");
  RNA_def_property_ui_text(prop, "Transform", "");

  prop = RNA_def_property(srna, "use_crop", PROP_BOOLEAN, PROP_NONE);
  RNA_def_property_boolean_sdna(prop, NULL, "flag", SEQ_USE_CROP);
  RNA_def_property_ui_text(prop, "Use Crop", "Crop image before processing");
  RNA_def_property_boolean_funcs(prop, NULL, "rna_Sequence_use_crop_set");
  RNA_def_property_update(prop, NC_SCENE | ND_SEQUENCER, "rna_Sequence_update");

  prop = RNA_def_property(srna, "crop", PROP_POINTER, PROP_NONE);
  RNA_def_property_pointer_sdna(prop, NULL, "strip->crop");
  RNA_def_property_ui_text(prop, "Crop", "");
}

static void rna_def_proxy(StructRNA *srna)
{
  PropertyRNA *prop;

  prop = RNA_def_property(srna, "use_proxy", PROP_BOOLEAN, PROP_NONE);
  RNA_def_property_boolean_sdna(prop, NULL, "flag", SEQ_USE_PROXY);
  RNA_def_property_ui_text(
      prop, "Use Proxy / Timecode", "Use a preview proxy and/or timecode index for this strip");
  RNA_def_property_boolean_funcs(prop, NULL, "rna_Sequence_use_proxy_set");

  prop = RNA_def_property(srna, "proxy", PROP_POINTER, PROP_NONE);
  RNA_def_property_pointer_sdna(prop, NULL, "strip->proxy");
  RNA_def_property_ui_text(prop, "Proxy", "");
}

static void rna_def_input(StructRNA *srna)
{
  PropertyRNA *prop;

  prop = RNA_def_property(srna, "animation_offset_start", PROP_INT, PROP_UNSIGNED);
  RNA_def_property_int_sdna(prop, NULL, "anim_startofs");
  RNA_def_property_clear_flag(prop, PROP_ANIMATABLE);
  RNA_def_property_int_funcs(
      prop, NULL, "rna_Sequence_anim_startofs_final_set", NULL); /* overlap tests */
  RNA_def_property_ui_text(prop, "Animation Start Offset", "Animation start offset (trim start)");
  RNA_def_property_update(prop, NC_SCENE | ND_SEQUENCER, "rna_Sequence_update");

  prop = RNA_def_property(srna, "animation_offset_end", PROP_INT, PROP_UNSIGNED);
  RNA_def_property_int_sdna(prop, NULL, "anim_endofs");
  RNA_def_property_clear_flag(prop, PROP_ANIMATABLE);
  RNA_def_property_int_funcs(
      prop, NULL, "rna_Sequence_anim_endofs_final_set", NULL); /* overlap tests */
  RNA_def_property_ui_text(prop, "Animation End Offset", "Animation end offset (trim end)");
  RNA_def_property_update(prop, NC_SCENE | ND_SEQUENCER, "rna_Sequence_update");
}

static void rna_def_effect_inputs(StructRNA *srna, int count)
{
  PropertyRNA *prop;

  prop = RNA_def_property(srna, "input_count", PROP_INT, PROP_UNSIGNED);
  RNA_def_property_clear_flag(prop, PROP_EDITABLE);
  RNA_def_property_int_funcs(prop, "rna_Sequence_input_count_get", NULL, NULL);

  if (count >= 1) {
    prop = RNA_def_property(srna, "input_1", PROP_POINTER, PROP_NONE);
    RNA_def_property_pointer_sdna(prop, NULL, "seq1");
    RNA_def_property_flag(prop, PROP_EDITABLE | PROP_NEVER_NULL);
    RNA_def_property_ui_text(prop, "Input 1", "First input for the effect strip");
  }

  if (count >= 2) {
    prop = RNA_def_property(srna, "input_2", PROP_POINTER, PROP_NONE);
    RNA_def_property_pointer_sdna(prop, NULL, "seq2");
    RNA_def_property_flag(prop, PROP_EDITABLE | PROP_NEVER_NULL);
    RNA_def_property_ui_text(prop, "Input 2", "Second input for the effect strip");
  }

#  if 0
  if (count == 3) {  // not used by any effects (perhaps one day plugins?)
    prop = RNA_def_property(srna, "input_3", PROP_POINTER, PROP_NONE);
    RNA_def_property_pointer_sdna(prop, NULL, "seq3");
    RNA_def_property_flag(prop, PROP_EDITABLE | PROP_NEVER_NULL);
    RNA_def_property_ui_text(prop, "Input 3", "Third input for the effect strip");
  }
#  endif
}

static void rna_def_color_management(StructRNA *srna)
{
  PropertyRNA *prop;

  prop = RNA_def_property(srna, "colorspace_settings", PROP_POINTER, PROP_NONE);
  RNA_def_property_pointer_sdna(prop, NULL, "strip->colorspace_settings");
  RNA_def_property_struct_type(prop, "ColorManagedInputColorspaceSettings");
  RNA_def_property_ui_text(prop, "Color Space Settings", "Input color space settings");
}

static void rna_def_movie_types(StructRNA *srna)
{
  PropertyRNA *prop;

  prop = RNA_def_property(srna, "fps", PROP_FLOAT, PROP_NONE);
  RNA_def_property_ui_text(prop, "FPS", "Frames per second");
  RNA_def_property_clear_flag(prop, PROP_EDITABLE);
  RNA_def_property_float_funcs(prop, "rna_Sequence_fps_get", NULL, NULL);
}

static void rna_def_image(BlenderRNA *brna)
{
  StructRNA *srna;
  PropertyRNA *prop;

  srna = RNA_def_struct(brna, "ImageSequence", "Sequence");
  RNA_def_struct_ui_text(srna, "Image Sequence", "Sequence strip to load one or more images");
  RNA_def_struct_sdna(srna, "Sequence");

  prop = RNA_def_property(srna, "directory", PROP_STRING, PROP_DIRPATH);
  RNA_def_property_string_sdna(prop, NULL, "strip->dir");
  RNA_def_property_ui_text(prop, "Directory", "");
  RNA_def_property_update(prop, NC_SCENE | ND_SEQUENCER, "rna_Sequence_update");

  prop = RNA_def_property(srna, "elements", PROP_COLLECTION, PROP_NONE);
  RNA_def_property_collection_sdna(prop, NULL, "strip->stripdata", NULL);
  RNA_def_property_struct_type(prop, "SequenceElement");
  RNA_def_property_ui_text(prop, "Elements", "");
  RNA_def_property_collection_funcs(prop,
                                    "rna_SequenceEditor_elements_begin",
                                    "rna_iterator_array_next",
                                    "rna_iterator_array_end",
                                    "rna_iterator_array_get",
                                    "rna_SequenceEditor_elements_length",
                                    NULL,
                                    NULL,
                                    NULL);
  RNA_api_sequence_elements(brna, prop);

  /* multiview */
  prop = RNA_def_property(srna, "use_multiview", PROP_BOOLEAN, PROP_NONE);
  RNA_def_property_boolean_sdna(prop, NULL, "flag", SEQ_USE_VIEWS);
  RNA_def_property_ui_text(prop, "Use Multi-View", "Use Multiple Views (when available)");
  RNA_def_property_update(prop, NC_SCENE | ND_SEQUENCER, "rna_Sequence_views_format_update");

  prop = RNA_def_property(srna, "views_format", PROP_ENUM, PROP_NONE);
  RNA_def_property_enum_sdna(prop, NULL, "views_format");
  RNA_def_property_enum_items(prop, rna_enum_views_format_items);
  RNA_def_property_ui_text(prop, "Views Format", "Mode to load image views");
  RNA_def_property_update(prop, NC_IMAGE | ND_DISPLAY, "rna_Sequence_views_format_update");

  prop = RNA_def_property(srna, "stereo_3d_format", PROP_POINTER, PROP_NONE);
  RNA_def_property_pointer_sdna(prop, NULL, "stereo3d_format");
  RNA_def_property_flag(prop, PROP_NEVER_NULL);
  RNA_def_property_struct_type(prop, "Stereo3dFormat");
  RNA_def_property_ui_text(prop, "Stereo 3D Format", "Settings for stereo 3d");

  rna_def_filter_video(srna);
  rna_def_proxy(srna);
  rna_def_input(srna);
  rna_def_color_management(srna);
}

static void rna_def_meta(BlenderRNA *brna)
{
  StructRNA *srna;
  PropertyRNA *prop;

  srna = RNA_def_struct(brna, "MetaSequence", "Sequence");
  RNA_def_struct_ui_text(
      srna, "Meta Sequence", "Sequence strip to group other strips as a single sequence strip");
  RNA_def_struct_sdna(srna, "Sequence");

  prop = RNA_def_property(srna, "sequences", PROP_COLLECTION, PROP_NONE);
  RNA_def_property_collection_sdna(prop, NULL, "seqbase", NULL);
  RNA_def_property_struct_type(prop, "Sequence");
  RNA_def_property_ui_text(prop, "Sequences", "");

  rna_def_filter_video(srna);
  rna_def_proxy(srna);
  rna_def_input(srna);
}

static void rna_def_scene(BlenderRNA *brna)
{
  StructRNA *srna;
  PropertyRNA *prop;

  srna = RNA_def_struct(brna, "SceneSequence", "Sequence");
  RNA_def_struct_ui_text(
      srna, "Scene Sequence", "Sequence strip to used the rendered image of a scene");
  RNA_def_struct_sdna(srna, "Sequence");

  prop = RNA_def_property(srna, "scene", PROP_POINTER, PROP_NONE);
  RNA_def_property_flag(prop, PROP_EDITABLE);
  RNA_def_property_ui_text(prop, "Scene", "Scene that this sequence uses");
  RNA_def_property_update(prop, NC_SCENE | ND_SEQUENCER, "rna_Sequence_update");

  prop = RNA_def_property(srna, "scene_camera", PROP_POINTER, PROP_NONE);
  RNA_def_property_flag(prop, PROP_EDITABLE);
  RNA_def_property_pointer_funcs(prop, NULL, NULL, NULL, "rna_Camera_object_poll");
  RNA_def_property_ui_text(prop, "Camera Override", "Override the scenes active camera");
  RNA_def_property_update(prop, NC_SCENE | ND_SEQUENCER, "rna_Sequence_update");

  prop = RNA_def_property(srna, "use_sequence", PROP_BOOLEAN, PROP_NONE);
  RNA_def_property_boolean_sdna(prop, NULL, "flag", SEQ_SCENE_STRIPS);
  RNA_def_property_ui_text(
      prop, "Use Sequence", "Use scenes sequence strips directly, instead of rendering");
  RNA_def_property_update(prop, NC_SCENE | ND_SEQUENCER, "rna_Sequence_update");

  prop = RNA_def_property(srna, "use_grease_pencil", PROP_BOOLEAN, PROP_NONE);
  RNA_def_property_boolean_negative_sdna(prop, NULL, "flag", SEQ_SCENE_NO_GPENCIL);
  RNA_def_property_ui_text(
      prop, "Use Grease Pencil", "Show Grease Pencil strokes in OpenGL previews");
  RNA_def_property_update(prop, NC_SCENE | ND_SEQUENCER, "rna_Sequence_update");

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

  prop = RNA_def_property(srna, "mpeg_preseek", PROP_INT, PROP_NONE);
  RNA_def_property_int_sdna(prop, NULL, "anim_preseek");
  RNA_def_property_range(prop, 0, 50);
  RNA_def_property_ui_text(prop, "MPEG Preseek", "For MPEG movies, preseek this many frames");
  RNA_def_property_update(prop, NC_SCENE | ND_SEQUENCER, "rna_Sequence_update");

  prop = RNA_def_property(srna, "stream_index", PROP_INT, PROP_NONE);
  RNA_def_property_int_sdna(prop, NULL, "streamindex");
  RNA_def_property_range(prop, 0, 20);
  RNA_def_property_ui_text(
      prop,
      "Stream Index",
      "For files with several movie streams, use the stream with the given index");
  RNA_def_property_update(prop, NC_SCENE | ND_SEQUENCER, "rna_Sequence_update_reopen_files");

  prop = RNA_def_property(srna, "elements", PROP_COLLECTION, PROP_NONE);
  RNA_def_property_collection_sdna(prop, NULL, "strip->stripdata", NULL);
  RNA_def_property_struct_type(prop, "SequenceElement");
  RNA_def_property_ui_text(prop, "Elements", "");
  RNA_def_property_collection_funcs(prop,
                                    "rna_SequenceEditor_elements_begin",
                                    "rna_iterator_array_next",
                                    "rna_iterator_array_end",
                                    "rna_iterator_array_get",
                                    "rna_SequenceEditor_elements_length",
                                    NULL,
                                    NULL,
                                    NULL);

  prop = RNA_def_property(srna, "filepath", PROP_STRING, PROP_FILEPATH);
  RNA_def_property_ui_text(prop, "File", "");
  RNA_def_property_string_funcs(prop,
                                "rna_Sequence_filepath_get",
                                "rna_Sequence_filepath_length",
                                "rna_Sequence_filepath_set");
  RNA_def_property_update(prop, NC_SCENE | ND_SEQUENCER, "rna_Sequence_filepath_update");

  /* metadata */
  func = RNA_def_function(srna, "metadata", "rna_MovieSequence_metadata_get");
  RNA_def_function_ui_description(func, "Retrieve metadata of the movie file");
  /* return type */
  parm = RNA_def_pointer(
      func, "metadata", "IDPropertyWrapPtr", "", "Dict-like object containing the metadata");
  RNA_def_parameter_flags(parm, 0, PARM_RNAPTR);
  RNA_def_function_return(func, parm);

  /* multiview */
  prop = RNA_def_property(srna, "use_multiview", PROP_BOOLEAN, PROP_NONE);
  RNA_def_property_boolean_sdna(prop, NULL, "flag", SEQ_USE_VIEWS);
  RNA_def_property_ui_text(prop, "Use Multi-View", "Use Multiple Views (when available)");
  RNA_def_property_update(prop, NC_SCENE | ND_SEQUENCER, "rna_Sequence_views_format_update");

  prop = RNA_def_property(srna, "views_format", PROP_ENUM, PROP_NONE);
  RNA_def_property_enum_sdna(prop, NULL, "views_format");
  RNA_def_property_enum_items(prop, rna_enum_views_format_items);
  RNA_def_property_ui_text(prop, "Views Format", "Mode to load movie views");
  RNA_def_property_update(prop, NC_IMAGE | ND_DISPLAY, "rna_Sequence_views_format_update");

  prop = RNA_def_property(srna, "stereo_3d_format", PROP_POINTER, PROP_NONE);
  RNA_def_property_pointer_sdna(prop, NULL, "stereo3d_format");
  RNA_def_property_flag(prop, PROP_NEVER_NULL);
  RNA_def_property_struct_type(prop, "Stereo3dFormat");
  RNA_def_property_ui_text(prop, "Stereo 3D Format", "Settings for stereo 3d");

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

  /* TODO - add clip property? */

  prop = RNA_def_property(srna, "undistort", PROP_BOOLEAN, PROP_NONE);
  RNA_def_property_boolean_sdna(prop, NULL, "clip_flag", SEQ_MOVIECLIP_RENDER_UNDISTORTED);
  RNA_def_property_ui_text(prop, "Undistort Clip", "Use the undistorted version of the clip");
  RNA_def_property_update(prop, NC_SCENE | ND_SEQUENCER, "rna_Sequence_update");

  prop = RNA_def_property(srna, "stabilize2d", PROP_BOOLEAN, PROP_NONE);
  RNA_def_property_boolean_sdna(prop, NULL, "clip_flag", SEQ_MOVIECLIP_RENDER_STABILIZED);
  RNA_def_property_ui_text(prop, "Stabilize 2D Clip", "Use the 2D stabilized version of the clip");
  RNA_def_property_update(prop, NC_SCENE | ND_SEQUENCER, "rna_Sequence_update");

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
  RNA_def_property_update(prop, NC_SCENE | ND_SEQUENCER, "rna_Sequence_update");

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

  prop = RNA_def_property(srna, "volume", PROP_FLOAT, PROP_NONE);
  RNA_def_property_float_sdna(prop, NULL, "volume");
  RNA_def_property_range(prop, 0.0f, 100.0f);
  RNA_def_property_ui_text(prop, "Volume", "Playback volume of the sound");
  RNA_def_property_translation_context(prop, BLT_I18NCONTEXT_ID_SOUND);
  RNA_def_property_float_funcs(prop, NULL, "rna_Sequence_volume_set", NULL);
  RNA_def_property_update(prop, NC_SCENE | ND_SEQUENCER, "rna_Sequence_update");

  prop = RNA_def_property(srna, "pitch", PROP_FLOAT, PROP_NONE);
  RNA_def_property_float_sdna(prop, NULL, "pitch");
  RNA_def_property_range(prop, 0.1f, 10.0f);
  RNA_def_property_ui_text(prop, "Pitch", "Playback pitch of the sound");
  RNA_def_property_translation_context(prop, BLT_I18NCONTEXT_ID_SOUND);
  RNA_def_property_float_funcs(prop, NULL, "rna_Sequence_pitch_set", NULL);
  RNA_def_property_update(prop, NC_SCENE | ND_SEQUENCER, "rna_Sequence_update");

  prop = RNA_def_property(srna, "pan", PROP_FLOAT, PROP_NONE);
  RNA_def_property_float_sdna(prop, NULL, "pan");
  RNA_def_property_range(prop, -2.0f, 2.0f);
  RNA_def_property_ui_text(prop, "Pan", "Playback panning of the sound (only for Mono sources)");
  RNA_def_property_float_funcs(prop, NULL, "rna_Sequence_pan_set", NULL);
  RNA_def_property_update(prop, NC_SCENE | ND_SEQUENCER, "rna_Sequence_update");

  prop = RNA_def_property(srna, "show_waveform", PROP_BOOLEAN, PROP_NONE);
  RNA_def_property_boolean_sdna(prop, NULL, "flag", SEQ_AUDIO_DRAW_WAVEFORM);
  RNA_def_property_ui_text(prop, "Display Waveform", "Display the audio waveform inside the clip");
  RNA_def_property_update(prop, NC_SCENE | ND_SEQUENCER, NULL);

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
  RNA_def_property_int_sdna(prop, NULL, "multicam_source");
  RNA_def_property_range(prop, 0, MAXSEQ - 1);
  RNA_def_property_ui_text(prop, "Multicam Source Channel", "");
  RNA_def_property_update(prop, NC_SCENE | ND_SEQUENCER, "rna_Sequence_update");

  rna_def_input(srna);
}

static void rna_def_wipe(StructRNA *srna)
{
  PropertyRNA *prop;

  static const EnumPropertyItem wipe_type_items[] = {
      {0, "SINGLE", 0, "Single", ""},
      {1, "DOUBLE", 0, "Double", ""},
      /* not used yet {2, "BOX", 0, "Box", ""}, */
      /* not used yet {3, "CROSS", 0, "Cross", ""}, */
      {4, "IRIS", 0, "Iris", ""},
      {5, "CLOCK", 0, "Clock", ""},
      {0, NULL, 0, NULL, NULL},
  };

  static const EnumPropertyItem wipe_direction_items[] = {
      {0, "OUT", 0, "Out", ""},
      {1, "IN", 0, "In", ""},
      {0, NULL, 0, NULL, NULL},
  };

  RNA_def_struct_sdna_from(srna, "WipeVars", "effectdata");

  prop = RNA_def_property(srna, "blur_width", PROP_FLOAT, PROP_FACTOR);
  RNA_def_property_float_sdna(prop, NULL, "edgeWidth");
  RNA_def_property_range(prop, 0.0f, 1.0f);
  RNA_def_property_ui_text(
      prop, "Blur Width", "Width of the blur edge, in percentage relative to the image size");
  RNA_def_property_update(prop, NC_SCENE | ND_SEQUENCER, "rna_Sequence_update");

  prop = RNA_def_property(srna, "angle", PROP_FLOAT, PROP_ANGLE);
  RNA_def_property_range(prop, DEG2RADF(-90.0f), DEG2RADF(90.0f));
  RNA_def_property_ui_text(prop, "Angle", "Edge angle");
  RNA_def_property_update(prop, NC_SCENE | ND_SEQUENCER, "rna_Sequence_update");

  prop = RNA_def_property(srna, "direction", PROP_ENUM, PROP_NONE);
  RNA_def_property_enum_sdna(prop, NULL, "forward");
  RNA_def_property_enum_items(prop, wipe_direction_items);
  RNA_def_property_ui_text(prop, "Direction", "Wipe direction");
  RNA_def_property_update(prop, NC_SCENE | ND_SEQUENCER, "rna_Sequence_update");

  prop = RNA_def_property(srna, "transition_type", PROP_ENUM, PROP_NONE);
  RNA_def_property_enum_sdna(prop, NULL, "wipetype");
  RNA_def_property_enum_items(prop, wipe_type_items);
  RNA_def_property_translation_context(prop, BLT_I18NCONTEXT_ID_SEQUENCE);
  RNA_def_property_ui_text(prop, "Transition Type", "");
  RNA_def_property_update(prop, NC_SCENE | ND_SEQUENCER, "rna_Sequence_update");
}

static void rna_def_glow(StructRNA *srna)
{
  PropertyRNA *prop;

  RNA_def_struct_sdna_from(srna, "GlowVars", "effectdata");

  prop = RNA_def_property(srna, "threshold", PROP_FLOAT, PROP_FACTOR);
  RNA_def_property_float_sdna(prop, NULL, "fMini");
  RNA_def_property_range(prop, 0.0f, 1.0f);
  RNA_def_property_ui_text(prop, "Threshold", "Minimum intensity to trigger a glow");
  RNA_def_property_update(prop, NC_SCENE | ND_SEQUENCER, "rna_Sequence_update");

  prop = RNA_def_property(srna, "clamp", PROP_FLOAT, PROP_FACTOR);
  RNA_def_property_float_sdna(prop, NULL, "fClamp");
  RNA_def_property_range(prop, 0.0f, 1.0f);
  RNA_def_property_ui_text(prop, "Clamp", "Brightness limit of intensity");
  RNA_def_property_update(prop, NC_SCENE | ND_SEQUENCER, "rna_Sequence_update");

  prop = RNA_def_property(srna, "boost_factor", PROP_FLOAT, PROP_NONE);
  RNA_def_property_float_sdna(prop, NULL, "fBoost");
  RNA_def_property_range(prop, 0.0f, 10.0f);
  RNA_def_property_ui_text(prop, "Boost Factor", "Brightness multiplier");
  RNA_def_property_update(prop, NC_SCENE | ND_SEQUENCER, "rna_Sequence_update");

  prop = RNA_def_property(srna, "blur_radius", PROP_FLOAT, PROP_NONE);
  RNA_def_property_float_sdna(prop, NULL, "dDist");
  RNA_def_property_range(prop, 0.5f, 20.0f);
  RNA_def_property_ui_text(prop, "Blur Distance", "Radius of glow effect");
  RNA_def_property_update(prop, NC_SCENE | ND_SEQUENCER, "rna_Sequence_update");

  prop = RNA_def_property(srna, "quality", PROP_INT, PROP_NONE);
  RNA_def_property_int_sdna(prop, NULL, "dQuality");
  RNA_def_property_range(prop, 1, 5);
  RNA_def_property_ui_text(prop, "Quality", "Accuracy of the blur effect");
  RNA_def_property_update(prop, NC_SCENE | ND_SEQUENCER, "rna_Sequence_update");

  prop = RNA_def_property(srna, "use_only_boost", PROP_BOOLEAN, PROP_NONE);
  RNA_def_property_boolean_sdna(prop, NULL, "bNoComp", 0);
  RNA_def_property_ui_text(prop, "Only Boost", "Show the glow buffer only");
  RNA_def_property_update(prop, NC_SCENE | ND_SEQUENCER, "rna_Sequence_update");
}

static void rna_def_transform(StructRNA *srna)
{
  PropertyRNA *prop;

  static const EnumPropertyItem interpolation_items[] = {
      {0, "NONE", 0, "None", "No interpolation"},
      {1, "BILINEAR", 0, "Bilinear", "Bilinear interpolation"},
      {2, "BICUBIC", 0, "Bicubic", "Bicubic interpolation"},
      {0, NULL, 0, NULL, NULL},
  };

  static const EnumPropertyItem translation_unit_items[] = {
      {0, "PIXELS", 0, "Pixels", ""},
      {1, "PERCENT", 0, "Percent", ""},
      {0, NULL, 0, NULL, NULL},
  };

  RNA_def_struct_sdna_from(srna, "TransformVars", "effectdata");

  prop = RNA_def_property(srna, "scale_start_x", PROP_FLOAT, PROP_UNSIGNED);
  RNA_def_property_float_sdna(prop, NULL, "ScalexIni");
  RNA_def_property_ui_text(prop, "Scale X", "Amount to scale the input in the X axis");
  RNA_def_property_ui_range(prop, 0, 10, 3, 6);
  RNA_def_property_update(prop, NC_SCENE | ND_SEQUENCER, "rna_Sequence_update");

  prop = RNA_def_property(srna, "scale_start_y", PROP_FLOAT, PROP_UNSIGNED);
  RNA_def_property_float_sdna(prop, NULL, "ScaleyIni");
  RNA_def_property_ui_text(prop, "Scale Y", "Amount to scale the input in the Y axis");
  RNA_def_property_ui_range(prop, 0, 10, 3, 6);
  RNA_def_property_update(prop, NC_SCENE | ND_SEQUENCER, "rna_Sequence_update");

  prop = RNA_def_property(srna, "use_uniform_scale", PROP_BOOLEAN, PROP_NONE);
  RNA_def_property_boolean_sdna(prop, NULL, "uniform_scale", 0);
  RNA_def_property_ui_text(prop, "Uniform Scale", "Scale uniformly, preserving aspect ratio");
  RNA_def_property_update(prop, NC_SCENE | ND_SEQUENCER, "rna_Sequence_update");

  prop = RNA_def_property(srna, "translate_start_x", PROP_FLOAT, PROP_NONE);
  RNA_def_property_float_sdna(prop, NULL, "xIni");
  RNA_def_property_ui_text(prop, "Translate X", "Amount to move the input on the X axis");
  RNA_def_property_ui_range(prop, -4000.0f, 4000.0f, 3, 6);
  RNA_def_property_update(prop, NC_SCENE | ND_SEQUENCER, "rna_Sequence_update");

  prop = RNA_def_property(srna, "translate_start_y", PROP_FLOAT, PROP_NONE);
  RNA_def_property_float_sdna(prop, NULL, "yIni");
  RNA_def_property_ui_text(prop, "Translate Y", "Amount to move the input on the Y axis");
  RNA_def_property_ui_range(prop, -4000.0f, 4000.0f, 3, 6);
  RNA_def_property_update(prop, NC_SCENE | ND_SEQUENCER, "rna_Sequence_update");

  prop = RNA_def_property(srna, "rotation_start", PROP_FLOAT, PROP_NONE);
  RNA_def_property_float_sdna(prop, NULL, "rotIni");
  RNA_def_property_ui_text(prop, "Rotation", "Degrees to rotate the input");
  RNA_def_property_update(prop, NC_SCENE | ND_SEQUENCER, "rna_Sequence_update");

  prop = RNA_def_property(srna, "translation_unit", PROP_ENUM, PROP_NONE);
  RNA_def_property_enum_sdna(prop, NULL, "percent");
  RNA_def_property_clear_flag(prop, PROP_ANIMATABLE); /* not meant to be animated */
  RNA_def_property_enum_items(prop, translation_unit_items);
  RNA_def_property_ui_text(prop, "Translation Unit", "Unit of measure to translate the input");
  RNA_def_property_update(prop, NC_SCENE | ND_SEQUENCER, "rna_Sequence_update");

  prop = RNA_def_property(srna, "interpolation", PROP_ENUM, PROP_NONE);
  RNA_def_property_enum_items(prop, interpolation_items);
  RNA_def_property_clear_flag(prop, PROP_ANIMATABLE); /* not meant to be animated */
  RNA_def_property_ui_text(
      prop, "Interpolation", "Method to determine how missing pixels are created");
  RNA_def_property_update(prop, NC_SCENE | ND_SEQUENCER, "rna_Sequence_update");
}

static void rna_def_solid_color(StructRNA *srna)
{
  PropertyRNA *prop;

  RNA_def_struct_sdna_from(srna, "SolidColorVars", "effectdata");

  prop = RNA_def_property(srna, "color", PROP_FLOAT, PROP_COLOR_GAMMA);
  RNA_def_property_float_sdna(prop, NULL, "col");
  RNA_def_property_ui_text(prop, "Color", "Effect Strip color");
  RNA_def_property_update(prop, NC_SCENE | ND_SEQUENCER, "rna_Sequence_update");
}

static void rna_def_speed_control(StructRNA *srna)
{
  PropertyRNA *prop;

  RNA_def_struct_sdna_from(srna, "SpeedControlVars", "effectdata");

  prop = RNA_def_property(srna, "multiply_speed", PROP_FLOAT, PROP_UNSIGNED);
  RNA_def_property_float_sdna(prop, NULL, "globalSpeed");
  RNA_def_property_clear_flag(prop, PROP_ANIMATABLE); /* seq->facf0 is used to animate this */
  RNA_def_property_ui_text(
      prop, "Multiply Speed", "Multiply the resulting speed after the speed factor");
  RNA_def_property_ui_range(prop, 0.0f, 100.0f, 1, -1);
  RNA_def_property_update(prop, NC_SCENE | ND_SEQUENCER, "rna_Sequence_update");

  prop = RNA_def_property(srna, "use_as_speed", PROP_BOOLEAN, PROP_NONE);
  RNA_def_property_boolean_sdna(prop, NULL, "flags", SEQ_SPEED_INTEGRATE);
  RNA_def_property_ui_text(
      prop, "Use as speed", "Interpret the value as speed instead of a frame number");
  RNA_def_property_update(prop, NC_SCENE | ND_SEQUENCER, "rna_Sequence_update");

  prop = RNA_def_property(srna, "use_scale_to_length", PROP_BOOLEAN, PROP_NONE);
  RNA_def_property_boolean_sdna(prop, NULL, "flags", SEQ_SPEED_COMPRESS_IPO_Y);
  RNA_def_property_ui_text(
      prop, "Scale to length", "Scale values from 0.0 to 1.0 to target sequence length");
  RNA_def_property_update(prop, NC_SCENE | ND_SEQUENCER, "rna_Sequence_update");
}

static void rna_def_gaussian_blur(StructRNA *srna)
{
  PropertyRNA *prop;

  RNA_def_struct_sdna_from(srna, "GaussianBlurVars", "effectdata");
  prop = RNA_def_property(srna, "size_x", PROP_FLOAT, PROP_UNSIGNED);
  RNA_def_property_ui_text(prop, "Size X", "Size of the blur along X axis");
  RNA_def_property_ui_range(prop, 0.0f, FLT_MAX, 1, -1);
  RNA_def_property_update(prop, NC_SCENE | ND_SEQUENCER, "rna_Sequence_update");

  prop = RNA_def_property(srna, "size_y", PROP_FLOAT, PROP_UNSIGNED);
  RNA_def_property_ui_text(prop, "Size Y", "Size of the blur along Y axis");
  RNA_def_property_ui_range(prop, 0.0f, FLT_MAX, 1, -1);
  RNA_def_property_update(prop, NC_SCENE | ND_SEQUENCER, "rna_Sequence_update");
}

static void rna_def_text(StructRNA *srna)
{
  static const EnumPropertyItem text_align_x_items[] = {
      {SEQ_TEXT_ALIGN_X_LEFT, "LEFT", 0, "Left", ""},
      {SEQ_TEXT_ALIGN_X_CENTER, "CENTER", 0, "Center", ""},
      {SEQ_TEXT_ALIGN_X_RIGHT, "RIGHT", 0, "Right", ""},
      {0, NULL, 0, NULL, NULL},
  };
  static const EnumPropertyItem text_align_y_items[] = {
      {SEQ_TEXT_ALIGN_Y_TOP, "TOP", 0, "Top", ""},
      {SEQ_TEXT_ALIGN_Y_CENTER, "CENTER", 0, "Center", ""},
      {SEQ_TEXT_ALIGN_Y_BOTTOM, "BOTTOM", 0, "Bottom", ""},
      {0, NULL, 0, NULL, NULL},
  };

  PropertyRNA *prop;

  RNA_def_struct_sdna_from(srna, "TextVars", "effectdata");

  prop = RNA_def_property(srna, "font", PROP_POINTER, PROP_NONE);
  RNA_def_property_pointer_sdna(prop, NULL, "text_font");
  RNA_def_property_ui_icon(prop, ICON_FILE_FONT, false);
  RNA_def_property_ui_text(prop, "Font", "Font of the text. Falls back to the UI font by default");
  RNA_def_property_flag(prop, PROP_EDITABLE);
  RNA_def_property_pointer_funcs(prop, NULL, "rna_Sequence_text_font_set", NULL, NULL);
  RNA_def_property_update(prop, NC_SCENE | ND_SEQUENCER, "rna_Sequence_update");

  prop = RNA_def_property(srna, "font_size", PROP_INT, PROP_UNSIGNED);
  RNA_def_property_int_sdna(prop, NULL, "text_size");
  RNA_def_property_ui_text(prop, "Size", "Size of the text");
  RNA_def_property_range(prop, 0.0, 2000);
  RNA_def_property_ui_range(prop, 0.0f, 1000, 1, -1);
  RNA_def_property_update(prop, NC_SCENE | ND_SEQUENCER, "rna_Sequence_update");

  prop = RNA_def_property(srna, "color", PROP_FLOAT, PROP_COLOR_GAMMA);
  RNA_def_property_float_sdna(prop, NULL, "color");
  RNA_def_property_ui_text(prop, "Color", "Text color");
  RNA_def_property_update(prop, NC_SCENE | ND_SEQUENCER, "rna_Sequence_update");

  prop = RNA_def_property(srna, "shadow_color", PROP_FLOAT, PROP_COLOR_GAMMA);
  RNA_def_property_float_sdna(prop, NULL, "shadow_color");
  RNA_def_property_ui_text(prop, "Shadow Color", "");
  RNA_def_property_update(prop, NC_SCENE | ND_SEQUENCER, "rna_Sequence_update");

  prop = RNA_def_property(srna, "location", PROP_FLOAT, PROP_XYZ);
  RNA_def_property_float_sdna(prop, NULL, "loc");
  RNA_def_property_ui_text(prop, "Location", "Location of the text");
  RNA_def_property_range(prop, -FLT_MAX, FLT_MAX);
  RNA_def_property_ui_range(prop, 0.0, 1.0, 1, -1);
  RNA_def_property_update(prop, NC_SCENE | ND_SEQUENCER, "rna_Sequence_update");

  prop = RNA_def_property(srna, "wrap_width", PROP_FLOAT, PROP_NONE);
  RNA_def_property_float_sdna(prop, NULL, "wrap_width");
  RNA_def_property_ui_text(prop, "Wrap Width", "Word wrap width as factor, zero disables");
  RNA_def_property_range(prop, 0, FLT_MAX);
  RNA_def_property_ui_range(prop, 0.0, 1.0, 1, -1);
  RNA_def_property_update(prop, NC_SCENE | ND_SEQUENCER, "rna_Sequence_update");

  prop = RNA_def_property(srna, "align_x", PROP_ENUM, PROP_NONE);
  RNA_def_property_enum_sdna(prop, NULL, "align");
  RNA_def_property_enum_items(prop, text_align_x_items);
  RNA_def_property_ui_text(prop, "Align X", "Align the text along the X axis");
  RNA_def_property_update(prop, NC_SCENE | ND_SEQUENCER, "rna_Sequence_update");

  prop = RNA_def_property(srna, "align_y", PROP_ENUM, PROP_NONE);
  RNA_def_property_enum_sdna(prop, NULL, "align_y");
  RNA_def_property_enum_items(prop, text_align_y_items);
  RNA_def_property_ui_text(prop, "Align Y", "Align the image along the Y axis");
  RNA_def_property_update(prop, NC_SCENE | ND_SEQUENCER, "rna_Sequence_update");

  prop = RNA_def_property(srna, "text", PROP_STRING, PROP_NONE);
  RNA_def_property_ui_text(prop, "Text", "Text that will be displayed");
  RNA_def_property_update(prop, NC_SCENE | ND_SEQUENCER, "rna_Sequence_update");

  prop = RNA_def_property(srna, "use_shadow", PROP_BOOLEAN, PROP_NONE);
  RNA_def_property_boolean_sdna(prop, NULL, "flag", SEQ_TEXT_SHADOW);
  RNA_def_property_ui_text(prop, "Shadow", "Draw text with shadow");
  RNA_def_property_update(prop, NC_SCENE | ND_SEQUENCER, "rna_Sequence_update");
}

static void rna_def_color_mix(StructRNA *srna)
{
  static EnumPropertyItem blend_color_items[] = {
      {SEQ_TYPE_ADD, "ADD", 0, "Add", ""},
      {SEQ_TYPE_SUB, "SUBTRACT", 0, "Subtract", ""},
      {SEQ_TYPE_MUL, "MULTIPLY", 0, "Multiply", ""},
      {SEQ_TYPE_LIGHTEN, "LIGHTEN", 0, "Lighten", ""},
      {SEQ_TYPE_DARKEN, "DARKEN", 0, "Darken", ""},
      {SEQ_TYPE_SCREEN, "SCREEN", 0, "Screen", ""},
      {SEQ_TYPE_OVERLAY, "OVERLAY", 0, "Overlay", ""},
      {SEQ_TYPE_DODGE, "DODGE", 0, "Dodge", ""},
      {SEQ_TYPE_BURN, "BURN", 0, "Burn", ""},
      {SEQ_TYPE_LINEAR_BURN, "LINEAR_BURN", 0, "Linear Burn", ""},
      {SEQ_TYPE_SOFT_LIGHT, "SOFT_LIGHT", 0, "Soft Light", ""},
      {SEQ_TYPE_HARD_LIGHT, "HARD_LIGHT", 0, "Hard Light", ""},
      {SEQ_TYPE_PIN_LIGHT, "PIN_LIGHT", 0, "Pin Light", ""},
      {SEQ_TYPE_LIN_LIGHT, "LINEAR_LIGHT", 0, "Linear Light", ""},
      {SEQ_TYPE_VIVID_LIGHT, "VIVID_LIGHT", 0, "Vivid Light", ""},
      {SEQ_TYPE_BLEND_COLOR, "COLOR", 0, "Color", ""},
      {SEQ_TYPE_HUE, "HUE", 0, "Hue", ""},
      {SEQ_TYPE_SATURATION, "SATURATION", 0, "Saturation", ""},
      {SEQ_TYPE_VALUE, "VALUE", 0, "Value", ""},
      {SEQ_TYPE_DIFFERENCE, "DIFFERENCE", 0, "Difference", ""},
      {SEQ_TYPE_EXCLUSION, "EXCLUSION", 0, "Exclusion", ""},
      {0, NULL, 0, NULL, NULL},
  };

  PropertyRNA *prop;

  RNA_def_struct_sdna_from(srna, "ColorMixVars", "effectdata");

  prop = RNA_def_property(srna, "blend_effect", PROP_ENUM, PROP_NONE);
  RNA_def_property_enum_sdna(prop, NULL, "blend_effect");
  RNA_def_property_enum_items(prop, blend_color_items);
  RNA_def_property_ui_text(
      prop, "Blend Effect", "Method for controlling how the strip combines with other strips");
  RNA_def_property_update(prop, NC_SCENE | ND_SEQUENCER, "rna_Sequence_update");

  prop = RNA_def_property(srna, "factor", PROP_FLOAT, PROP_FACTOR);
  RNA_def_property_range(prop, 0.0f, 1.0f);
  RNA_def_property_ui_text(
      prop, "Blend Factor", "Percentage of how much the strip's colors affect other strips");
  RNA_def_property_update(prop, NC_SCENE | ND_SEQUENCER, "rna_Sequence_update");
}

static EffectInfo def_effects[] = {
    {"AddSequence", "Add Sequence", "Add Sequence", NULL, 2},
    {"AdjustmentSequence",
     "Adjustment Layer Sequence",
     "Sequence strip to perform filter adjustments to layers below",
     rna_def_input,
     0},
    {"AlphaOverSequence", "Alpha Over Sequence", "Alpha Over Sequence", NULL, 2},
    {"AlphaUnderSequence", "Alpha Under Sequence", "Alpha Under Sequence", NULL, 2},
    {"ColorSequence",
     "Color Sequence",
     "Sequence strip creating an image filled with a single color",
     rna_def_solid_color,
     0},
    {"CrossSequence", "Cross Sequence", "Cross Sequence", NULL, 2},
    {"GammaCrossSequence", "Gamma Cross Sequence", "Gamma Cross Sequence", NULL, 2},
    {"GlowSequence", "Glow Sequence", "Sequence strip creating a glow effect", rna_def_glow, 1},
    {"MulticamSequence",
     "Multicam Select Sequence",
     "Sequence strip to perform multicam editing",
     rna_def_multicam,
     0},
    {"MultiplySequence", "Multiply Sequence", "Multiply Sequence", NULL, 2},
    {"OverDropSequence", "Over Drop Sequence", "Over Drop Sequence", NULL, 2},
    {"SpeedControlSequence",
     "SpeedControl Sequence",
     "Sequence strip to control the speed of other strips",
     rna_def_speed_control,
     1},
    {"SubtractSequence", "Subtract Sequence", "Subtract Sequence", NULL, 2},
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
    {"", "", "", NULL, 0},
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
      {0, NULL, 0, NULL, NULL},
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
      {0, NULL, 0, NULL, NULL},
  };

  srna = RNA_def_struct(brna, "SequenceModifier", NULL);
  RNA_def_struct_sdna(srna, "SequenceModifierData");
  RNA_def_struct_ui_text(srna, "SequenceModifier", "Modifier for sequence strip");
  RNA_def_struct_refine_func(srna, "rna_SequenceModifier_refine");
  RNA_def_struct_path_func(srna, "rna_SequenceModifier_path");

  prop = RNA_def_property(srna, "name", PROP_STRING, PROP_NONE);
  RNA_def_property_string_funcs(prop, NULL, NULL, "rna_SequenceModifier_name_set");
  RNA_def_property_ui_text(prop, "Name", "");
  RNA_def_struct_name_property(srna, prop);
  RNA_def_property_update(prop, NC_SCENE | ND_SEQUENCER, NULL);

  prop = RNA_def_property(srna, "type", PROP_ENUM, PROP_NONE);
  RNA_def_property_clear_flag(prop, PROP_EDITABLE);
  RNA_def_property_enum_items(prop, rna_enum_sequence_modifier_type_items);
  RNA_def_property_ui_text(prop, "Type", "");
  RNA_def_property_update(prop, NC_SCENE | ND_SEQUENCER, NULL);

  prop = RNA_def_property(srna, "mute", PROP_BOOLEAN, PROP_NONE);
  RNA_def_property_boolean_sdna(prop, NULL, "flag", SEQUENCE_MODIFIER_MUTE);
  RNA_def_property_ui_text(prop, "Mute", "Mute this modifier");
  RNA_def_property_ui_icon(prop, ICON_HIDE_OFF, -1);
  RNA_def_property_update(prop, NC_SCENE | ND_SEQUENCER, "rna_SequenceModifier_update");

  prop = RNA_def_property(srna, "show_expanded", PROP_BOOLEAN, PROP_NONE);
  RNA_def_property_flag(prop, PROP_NO_DEG_UPDATE);
  RNA_def_property_boolean_sdna(prop, NULL, "flag", SEQUENCE_MODIFIER_EXPANDED);
  RNA_def_property_ui_text(prop, "Expanded", "Mute expanded settings for the modifier");
  RNA_def_property_ui_icon(prop, ICON_TRIA_RIGHT, 1);
  RNA_def_property_update(prop, NC_SCENE | ND_SEQUENCER, NULL);

  prop = RNA_def_property(srna, "input_mask_type", PROP_ENUM, PROP_NONE);
  RNA_def_property_enum_sdna(prop, NULL, "mask_input_type");
  RNA_def_property_enum_items(prop, mask_input_type_items);
  RNA_def_property_ui_text(prop, "Mask Input Type", "Type of input data used for mask");
  RNA_def_property_update(prop, NC_SCENE | ND_SEQUENCER, "rna_SequenceModifier_update");

  prop = RNA_def_property(srna, "mask_time", PROP_ENUM, PROP_NONE);
  RNA_def_property_enum_sdna(prop, NULL, "mask_time");
  RNA_def_property_enum_items(prop, mask_time_items);
  RNA_def_property_ui_text(prop, "Mask Time", "Time to use for the Mask animation");
  RNA_def_property_update(prop, NC_SCENE | ND_SEQUENCER, "rna_SequenceModifier_update");

  prop = RNA_def_property(srna, "input_mask_strip", PROP_POINTER, PROP_NONE);
  RNA_def_property_pointer_sdna(prop, NULL, "mask_sequence");
  RNA_def_property_pointer_funcs(
      prop, NULL, NULL, NULL, "rna_SequenceModifier_otherSequence_poll");
  RNA_def_property_flag(prop, PROP_EDITABLE);
  RNA_def_property_ui_text(prop, "Mask Strip", "Strip used as mask input for the modifier");
  RNA_def_property_update(prop, NC_SCENE | ND_SEQUENCER, "rna_SequenceModifier_update");

  prop = RNA_def_property(srna, "input_mask_id", PROP_POINTER, PROP_NONE);
  RNA_def_property_pointer_sdna(prop, NULL, "mask_id");
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
  RNA_def_property_float_sdna(prop, NULL, "color_multiply");
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
  RNA_def_property_float_sdna(prop, NULL, "white_value");
  RNA_def_property_ui_text(prop, "White value", "This color defines white in the strip");
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
  RNA_def_property_pointer_sdna(prop, NULL, "curve_mapping");
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
  RNA_def_property_pointer_sdna(prop, NULL, "curve_mapping");
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
  RNA_def_property_float_sdna(prop, NULL, "bright");
  RNA_def_property_range(prop, -FLT_MAX, FLT_MAX);
  RNA_def_property_ui_text(prop, "Bright", "Adjust the luminosity of the colors");
  RNA_def_property_update(prop, NC_SCENE | ND_SEQUENCER, "rna_SequenceModifier_update");

  prop = RNA_def_property(srna, "contrast", PROP_FLOAT, PROP_UNSIGNED);
  RNA_def_property_float_sdna(prop, NULL, "contrast");
  RNA_def_property_range(prop, -FLT_MAX, FLT_MAX);
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
      {0, NULL, 0, NULL, NULL},
  };

  srna = RNA_def_struct(brna, "SequencerTonemapModifierData", "SequenceModifier");
  RNA_def_struct_sdna(srna, "SequencerTonemapModifierData");
  RNA_def_struct_ui_text(srna, "SequencerTonemapModifierData", "Tone mapping modifier");

  prop = RNA_def_property(srna, "tonemap_type", PROP_ENUM, PROP_NONE);
  RNA_def_property_enum_sdna(prop, NULL, "type");
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

void RNA_def_sequencer(BlenderRNA *brna)
{
  rna_def_color_balance(brna);

  rna_def_strip_element(brna);
  rna_def_strip_proxy(brna);
  rna_def_strip_color_balance(brna);
  rna_def_strip_crop(brna);
  rna_def_strip_transform(brna);

  rna_def_sequence(brna);
  rna_def_editor(brna);

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
}

#endif
