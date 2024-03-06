/* SPDX-FileCopyrightText: 2001-2002 NaN Holding BV. All rights reserved.
 * SPDX-FileCopyrightText: 2003-2009 Blender Authors
 * SPDX-FileCopyrightText: 2005-2006 Peter Schlaile <peter [at] schlaile [dot] de>
 *
 * SPDX-License-Identifier: GPL-2.0-or-later */

/** \file
 * \ingroup bke
 */

#define DNA_DEPRECATED_ALLOW

#include "MEM_guardedalloc.h"

#include "DNA_anim_types.h"
#include "DNA_scene_types.h"
#include "DNA_sequence_types.h"
#include "DNA_sound_types.h"

#include "BLI_listbase.h"
#include "BLI_path_util.h"

#include "BKE_fcurve.hh"
#include "BKE_idprop.h"
#include "BKE_lib_id.hh"
#include "BKE_main.hh"
#include "BKE_scene.hh"
#include "BKE_sound.h"

#include "DEG_depsgraph.hh"

#include "IMB_imbuf.hh"

#include "SEQ_channels.hh"
#include "SEQ_edit.hh"
#include "SEQ_effects.hh"
#include "SEQ_iterator.hh"
#include "SEQ_modifier.hh"
#include "SEQ_proxy.hh"
#include "SEQ_relations.hh"
#include "SEQ_retiming.hh"
#include "SEQ_select.hh"
#include "SEQ_sequencer.hh"
#include "SEQ_sound.hh"
#include "SEQ_time.hh"
#include "SEQ_utils.hh"

#include "BLO_read_write.hh"

#include "image_cache.hh"
#include "prefetch.hh"
#include "sequencer.hh"
#include "utils.hh"

/* -------------------------------------------------------------------- */
/** \name Allocate / Free Functions
 * \{ */

StripProxy *seq_strip_proxy_alloc()
{
  StripProxy *strip_proxy = static_cast<StripProxy *>(
      MEM_callocN(sizeof(StripProxy), "StripProxy"));
  strip_proxy->quality = 50;
  strip_proxy->build_tc_flags = SEQ_PROXY_TC_ALL;
  strip_proxy->tc = SEQ_PROXY_TC_RECORD_RUN;
  return strip_proxy;
}

static Strip *seq_strip_alloc(int type)
{
  Strip *strip = static_cast<Strip *>(MEM_callocN(sizeof(Strip), "strip"));

  if (type != SEQ_TYPE_SOUND_RAM) {
    strip->transform = static_cast<StripTransform *>(
        MEM_callocN(sizeof(StripTransform), "StripTransform"));
    strip->transform->scale_x = 1;
    strip->transform->scale_y = 1;
    strip->transform->origin[0] = 0.5f;
    strip->transform->origin[1] = 0.5f;
    strip->transform->filter = SEQ_TRANSFORM_FILTER_AUTO;
    strip->crop = static_cast<StripCrop *>(MEM_callocN(sizeof(StripCrop), "StripCrop"));
  }

  strip->us = 1;
  return strip;
}

static void seq_free_strip(Strip *strip)
{
  strip->us--;
  if (strip->us > 0) {
    return;
  }
  if (strip->us < 0) {
    printf("error: negative users in strip\n");
    return;
  }

  if (strip->stripdata) {
    MEM_freeN(strip->stripdata);
  }

  if (strip->proxy) {
    if (strip->proxy->anim) {
      IMB_free_anim(strip->proxy->anim);
    }

    MEM_freeN(strip->proxy);
  }
  if (strip->crop) {
    MEM_freeN(strip->crop);
  }
  if (strip->transform) {
    MEM_freeN(strip->transform);
  }

  MEM_freeN(strip);
}

Sequence *SEQ_sequence_alloc(ListBase *lb, int timeline_frame, int machine, int type)
{
  Sequence *seq;

  seq = static_cast<Sequence *>(MEM_callocN(sizeof(Sequence), "addseq"));
  BLI_addtail(lb, seq);

  *((short *)seq->name) = ID_SEQ;
  seq->name[2] = 0;

  seq->flag = SELECT;
  seq->start = timeline_frame;
  seq->machine = machine;
  seq->sat = 1.0;
  seq->mul = 1.0;
  seq->blend_opacity = 100.0;
  seq->volume = 1.0f;
  seq->scene_sound = nullptr;
  seq->type = type;
  seq->media_playback_rate = 0.0f;
  seq->speed_factor = 1.0f;

  if (seq->type == SEQ_TYPE_ADJUSTMENT) {
    seq->blend_mode = SEQ_TYPE_CROSS;
  }
  else {
    seq->blend_mode = SEQ_TYPE_ALPHAOVER;
  }

  seq->strip = seq_strip_alloc(type);
  seq->stereo3d_format = static_cast<Stereo3dFormat *>(
      MEM_callocN(sizeof(Stereo3dFormat), "Sequence Stereo Format"));

  seq->color_tag = SEQUENCE_COLOR_NONE;

  if (seq->type == SEQ_TYPE_META) {
    SEQ_channels_ensure(&seq->channels);
  }

  SEQ_relations_session_uid_generate(seq);

  return seq;
}

/* only give option to skip cache locally (static func) */
static void seq_sequence_free_ex(Scene *scene,
                                 Sequence *seq,
                                 const bool do_cache,
                                 const bool do_id_user)
{
  if (seq->strip) {
    seq_free_strip(seq->strip);
  }

  SEQ_relations_sequence_free_anim(seq);

  if (seq->type & SEQ_TYPE_EFFECT) {
    SeqEffectHandle sh = SEQ_effect_handle_get(seq);
    sh.free(seq, do_id_user);
  }

  if (seq->sound && do_id_user) {
    id_us_min((ID *)seq->sound);
  }

  if (seq->stereo3d_format) {
    MEM_freeN(seq->stereo3d_format);
  }

  /* clipboard has no scene and will never have a sound handle or be active
   * same goes to sequences copy for proxy rebuild job
   */
  if (scene) {
    Editing *ed = scene->ed;

    if (ed->act_seq == seq) {
      ed->act_seq = nullptr;
    }

    if (seq->scene_sound && ELEM(seq->type, SEQ_TYPE_SOUND_RAM, SEQ_TYPE_SCENE)) {
      BKE_sound_remove_scene_sound(scene, seq->scene_sound);
    }
  }

  if (seq->prop) {
    IDP_FreePropertyContent_ex(seq->prop, do_id_user);
    MEM_freeN(seq->prop);
  }

  /* free modifiers */
  SEQ_modifier_clear(seq);

  /* free cached data used by this strip,
   * also invalidate cache for all dependent sequences
   *
   * be _very_ careful here, invalidating cache loops over the scene sequences and
   * assumes the listbase is valid for all strips,
   * this may not be the case if lists are being freed.
   * this is optional SEQ_relations_invalidate_cache
   */
  if (do_cache) {
    if (scene) {
      SEQ_relations_invalidate_cache_raw(scene, seq);
    }
  }
  if (seq->type == SEQ_TYPE_META) {
    SEQ_channels_free(&seq->channels);
  }

  if (seq->retiming_keys != nullptr) {
    MEM_freeN(seq->retiming_keys);
    seq->retiming_keys = nullptr;
    seq->retiming_keys_num = 0;
  }

  MEM_freeN(seq);
}

void SEQ_sequence_free(Scene *scene, Sequence *seq)
{
  seq_sequence_free_ex(scene, seq, true, true);
}

void seq_free_sequence_recurse(Scene *scene, Sequence *seq, const bool do_id_user)
{
  Sequence *iseq, *iseq_next;

  for (iseq = static_cast<Sequence *>(seq->seqbase.first); iseq; iseq = iseq_next) {
    iseq_next = iseq->next;
    seq_free_sequence_recurse(scene, iseq, do_id_user);
  }

  seq_sequence_free_ex(scene, seq, false, do_id_user);
}

Editing *SEQ_editing_get(const Scene *scene)
{
  return scene->ed;
}

Editing *SEQ_editing_ensure(Scene *scene)
{
  if (scene->ed == nullptr) {
    Editing *ed;

    ed = scene->ed = static_cast<Editing *>(MEM_callocN(sizeof(Editing), "addseq"));
    ed->seqbasep = &ed->seqbase;
    ed->cache = nullptr;
    ed->cache_flag = SEQ_CACHE_STORE_FINAL_OUT;
    ed->cache_flag |= SEQ_CACHE_STORE_RAW;
    ed->displayed_channels = &ed->channels;
    SEQ_channels_ensure(ed->displayed_channels);
  }

  return scene->ed;
}

void SEQ_editing_free(Scene *scene, const bool do_id_user)
{
  Editing *ed = scene->ed;

  if (ed == nullptr) {
    return;
  }

  seq_prefetch_free(scene);
  seq_cache_destruct(scene);

  /* handle cache freeing above */
  LISTBASE_FOREACH_MUTABLE (Sequence *, seq, &ed->seqbase) {
    seq_free_sequence_recurse(scene, seq, do_id_user);
  }

  BLI_freelistN(&ed->metastack);
  SEQ_sequence_lookup_free(scene);
  SEQ_channels_free(&ed->channels);

  MEM_freeN(ed);

  scene->ed = nullptr;
}

static void seq_new_fix_links_recursive(Sequence *seq)
{
  if (seq->type & SEQ_TYPE_EFFECT) {
    if (seq->seq1 && seq->seq1->tmp) {
      seq->seq1 = static_cast<Sequence *>(seq->seq1->tmp);
    }
    if (seq->seq2 && seq->seq2->tmp) {
      seq->seq2 = static_cast<Sequence *>(seq->seq2->tmp);
    }
    if (seq->seq3 && seq->seq3->tmp) {
      seq->seq3 = static_cast<Sequence *>(seq->seq3->tmp);
    }
  }
  else if (seq->type == SEQ_TYPE_META) {
    LISTBASE_FOREACH (Sequence *, seqn, &seq->seqbase) {
      seq_new_fix_links_recursive(seqn);
    }
  }

  LISTBASE_FOREACH (SequenceModifierData *, smd, &seq->modifiers) {
    if (smd->mask_sequence && smd->mask_sequence->tmp) {
      smd->mask_sequence = static_cast<Sequence *>(smd->mask_sequence->tmp);
    }
  }
}

SequencerToolSettings *SEQ_tool_settings_init()
{
  SequencerToolSettings *tool_settings = static_cast<SequencerToolSettings *>(
      MEM_callocN(sizeof(SequencerToolSettings), "Sequencer tool settings"));
  tool_settings->fit_method = SEQ_SCALE_TO_FIT;
  tool_settings->snap_mode = SEQ_SNAP_TO_STRIPS | SEQ_SNAP_TO_CURRENT_FRAME |
                             SEQ_SNAP_TO_STRIP_HOLD;
  tool_settings->snap_distance = 15;
  tool_settings->overlap_mode = SEQ_OVERLAP_SHUFFLE;
  tool_settings->pivot_point = V3D_AROUND_LOCAL_ORIGINS;

  return tool_settings;
}

SequencerToolSettings *SEQ_tool_settings_ensure(Scene *scene)
{
  SequencerToolSettings *tool_settings = scene->toolsettings->sequencer_tool_settings;
  if (tool_settings == nullptr) {
    scene->toolsettings->sequencer_tool_settings = SEQ_tool_settings_init();
    tool_settings = scene->toolsettings->sequencer_tool_settings;
  }

  return tool_settings;
}

void SEQ_tool_settings_free(SequencerToolSettings *tool_settings)
{
  MEM_freeN(tool_settings);
}

eSeqImageFitMethod SEQ_tool_settings_fit_method_get(Scene *scene)
{
  const SequencerToolSettings *tool_settings = SEQ_tool_settings_ensure(scene);
  return eSeqImageFitMethod(tool_settings->fit_method);
}

short SEQ_tool_settings_snap_mode_get(Scene *scene)
{
  const SequencerToolSettings *tool_settings = SEQ_tool_settings_ensure(scene);
  return tool_settings->snap_mode;
}

short SEQ_tool_settings_snap_flag_get(Scene *scene)
{
  const SequencerToolSettings *tool_settings = SEQ_tool_settings_ensure(scene);
  return tool_settings->snap_flag;
}

int SEQ_tool_settings_snap_distance_get(Scene *scene)
{
  const SequencerToolSettings *tool_settings = SEQ_tool_settings_ensure(scene);
  return tool_settings->snap_distance;
}

void SEQ_tool_settings_fit_method_set(Scene *scene, eSeqImageFitMethod fit_method)
{
  SequencerToolSettings *tool_settings = SEQ_tool_settings_ensure(scene);
  tool_settings->fit_method = fit_method;
}

eSeqOverlapMode SEQ_tool_settings_overlap_mode_get(Scene *scene)
{
  const SequencerToolSettings *tool_settings = SEQ_tool_settings_ensure(scene);
  return eSeqOverlapMode(tool_settings->overlap_mode);
}

int SEQ_tool_settings_pivot_point_get(Scene *scene)
{
  const SequencerToolSettings *tool_settings = SEQ_tool_settings_ensure(scene);
  return tool_settings->pivot_point;
}

ListBase *SEQ_active_seqbase_get(const Editing *ed)
{
  if (ed == nullptr) {
    return nullptr;
  }

  return ed->seqbasep;
}

void SEQ_seqbase_active_set(Editing *ed, ListBase *seqbase)
{
  ed->seqbasep = seqbase;
}

static MetaStack *seq_meta_stack_alloc(const Scene *scene, Sequence *seq_meta)
{
  Editing *ed = SEQ_editing_get(scene);

  MetaStack *ms = static_cast<MetaStack *>(MEM_mallocN(sizeof(MetaStack), "metastack"));
  BLI_addhead(&ed->metastack, ms);
  ms->parseq = seq_meta;

  /* Reference to previously displayed timeline data. */
  Sequence *higher_level_meta = seq_sequence_lookup_meta_by_seq(scene, seq_meta);
  ms->oldbasep = higher_level_meta ? &higher_level_meta->seqbase : &ed->seqbase;
  ms->old_channels = higher_level_meta ? &higher_level_meta->channels : &ed->channels;

  ms->disp_range[0] = SEQ_time_left_handle_frame_get(scene, ms->parseq);
  ms->disp_range[1] = SEQ_time_right_handle_frame_get(scene, ms->parseq);
  return ms;
}

MetaStack *SEQ_meta_stack_active_get(const Editing *ed)
{
  if (ed == nullptr) {
    return nullptr;
  }

  return static_cast<MetaStack *>(ed->metastack.last);
}

void SEQ_meta_stack_set(const Scene *scene, Sequence *dst_seq)
{
  Editing *ed = SEQ_editing_get(scene);
  /* Clear metastack */
  BLI_freelistN(&ed->metastack);

  if (dst_seq != nullptr) {
    /* Allocate meta stack in a way, that represents meta hierarchy in timeline. */
    seq_meta_stack_alloc(scene, dst_seq);
    Sequence *meta_parent = dst_seq;
    while ((meta_parent = seq_sequence_lookup_meta_by_seq(scene, meta_parent))) {
      seq_meta_stack_alloc(scene, meta_parent);
    }

    SEQ_seqbase_active_set(ed, &dst_seq->seqbase);
    SEQ_channels_displayed_set(ed, &dst_seq->channels);
  }
  else {
    /* Go to top level, exiting meta strip. */
    SEQ_seqbase_active_set(ed, &ed->seqbase);
    SEQ_channels_displayed_set(ed, &ed->channels);
  }
}

Sequence *SEQ_meta_stack_pop(Editing *ed)
{
  MetaStack *ms = SEQ_meta_stack_active_get(ed);
  Sequence *meta_parent = ms->parseq;
  SEQ_seqbase_active_set(ed, ms->oldbasep);
  SEQ_channels_displayed_set(ed, ms->old_channels);
  BLI_remlink(&ed->metastack, ms);
  MEM_freeN(ms);
  return meta_parent;
}

/** \} */

/* -------------------------------------------------------------------- */
/** \name Duplicate Functions
 * \{ */

static Sequence *seq_dupli(const Scene *scene_src,
                           Scene *scene_dst,
                           ListBase *new_seq_list,
                           Sequence *seq,
                           int dupe_flag,
                           const int flag)
{
  Sequence *seqn = static_cast<Sequence *>(MEM_dupallocN(seq));

  if ((flag & LIB_ID_CREATE_NO_MAIN) == 0) {
    SEQ_relations_session_uid_generate(seqn);
  }

  seq->tmp = seqn;
  seqn->strip = static_cast<Strip *>(MEM_dupallocN(seq->strip));

  seqn->stereo3d_format = static_cast<Stereo3dFormat *>(MEM_dupallocN(seq->stereo3d_format));

  /* XXX: add F-Curve duplication stuff? */

  if (seq->strip->crop) {
    seqn->strip->crop = static_cast<StripCrop *>(MEM_dupallocN(seq->strip->crop));
  }

  if (seq->strip->transform) {
    seqn->strip->transform = static_cast<StripTransform *>(MEM_dupallocN(seq->strip->transform));
  }

  if (seq->strip->proxy) {
    seqn->strip->proxy = static_cast<StripProxy *>(MEM_dupallocN(seq->strip->proxy));
    seqn->strip->proxy->anim = nullptr;
  }

  if (seq->prop) {
    seqn->prop = IDP_CopyProperty_ex(seq->prop, flag);
  }

  if (seqn->modifiers.first) {
    BLI_listbase_clear(&seqn->modifiers);

    SEQ_modifier_list_copy(seqn, seq);
  }

  if (seq->type == SEQ_TYPE_META) {
    seqn->strip->stripdata = nullptr;

    BLI_listbase_clear(&seqn->seqbase);
    /* WARNING: This meta-strip is not recursively duplicated here - do this after! */
    // seq_dupli_recursive(&seq->seqbase, &seqn->seqbase);

    BLI_listbase_clear(&seqn->channels);
    SEQ_channels_duplicate(&seqn->channels, &seq->channels);
  }
  else if (seq->type == SEQ_TYPE_SCENE) {
    seqn->strip->stripdata = nullptr;
    if (seq->scene_sound) {
      seqn->scene_sound = BKE_sound_scene_add_scene_sound_defaults(scene_dst, seqn);
    }
  }
  else if (seq->type == SEQ_TYPE_MOVIECLIP) {
    /* avoid assert */
  }
  else if (seq->type == SEQ_TYPE_MASK) {
    /* avoid assert */
  }
  else if (seq->type == SEQ_TYPE_MOVIE) {
    seqn->strip->stripdata = static_cast<StripElem *>(MEM_dupallocN(seq->strip->stripdata));
    BLI_listbase_clear(&seqn->anims);
  }
  else if (seq->type == SEQ_TYPE_SOUND_RAM) {
    seqn->strip->stripdata = static_cast<StripElem *>(MEM_dupallocN(seq->strip->stripdata));
    seqn->scene_sound = nullptr;
    if ((flag & LIB_ID_CREATE_NO_USER_REFCOUNT) == 0) {
      id_us_plus((ID *)seqn->sound);
    }
  }
  else if (seq->type == SEQ_TYPE_IMAGE) {
    seqn->strip->stripdata = static_cast<StripElem *>(MEM_dupallocN(seq->strip->stripdata));
  }
  else if (seq->type & SEQ_TYPE_EFFECT) {
    SeqEffectHandle sh;
    sh = SEQ_effect_handle_get(seq);
    if (sh.copy) {
      sh.copy(seqn, seq, flag);
    }

    seqn->strip->stripdata = nullptr;
  }
  else {
    /* sequence type not handled in duplicate! Expect a crash now... */
    BLI_assert_unreachable();
  }

  /* When using #SEQ_DUPE_UNIQUE_NAME, it is mandatory to add new sequences in relevant container
   * (scene or meta's one), *before* checking for unique names. Otherwise the meta's list is empty
   * and hence we miss all sequence-strips in that meta that have already been duplicated,
   * (see #55668). Note that unique name check itself could be done at a later step in calling
   * code, once all sequence-strips have bee duplicated (that was first, simpler solution),
   * but then handling of animation data will be broken (see #60194). */
  if (new_seq_list != nullptr) {
    BLI_addtail(new_seq_list, seqn);
  }

  if (scene_src == scene_dst) {
    if (dupe_flag & SEQ_DUPE_UNIQUE_NAME) {
      SEQ_sequence_base_unique_name_recursive(scene_dst, &scene_dst->ed->seqbase, seqn);
    }
  }

  if (seq->retiming_keys != nullptr) {
    seqn->retiming_keys = static_cast<SeqRetimingKey *>(MEM_dupallocN(seq->retiming_keys));
    seqn->retiming_keys_num = seq->retiming_keys_num;
  }

  return seqn;
}

static Sequence *sequence_dupli_recursive_do(const Scene *scene_src,
                                             Scene *scene_dst,
                                             ListBase *new_seq_list,
                                             Sequence *seq,
                                             const int dupe_flag)
{
  Sequence *seqn;

  seq->tmp = nullptr;
  seqn = seq_dupli(scene_src, scene_dst, new_seq_list, seq, dupe_flag, 0);
  if (seq->type == SEQ_TYPE_META) {
    LISTBASE_FOREACH (Sequence *, s, &seq->seqbase) {
      sequence_dupli_recursive_do(scene_src, scene_dst, &seqn->seqbase, s, dupe_flag);
    }
  }

  return seqn;
}

Sequence *SEQ_sequence_dupli_recursive(
    const Scene *scene_src, Scene *scene_dst, ListBase *new_seq_list, Sequence *seq, int dupe_flag)
{
  Sequence *seqn = sequence_dupli_recursive_do(scene_src, scene_dst, new_seq_list, seq, dupe_flag);

  /* This does not need to be in recursive call itself, since it is already recursive... */
  seq_new_fix_links_recursive(seqn);

  return seqn;
}

void SEQ_sequence_base_dupli_recursive(const Scene *scene_src,
                                       Scene *scene_dst,
                                       ListBase *nseqbase,
                                       const ListBase *seqbase,
                                       int dupe_flag,
                                       const int flag)
{
  LISTBASE_FOREACH (Sequence *, seq, seqbase) {
    seq->tmp = nullptr;
    if ((seq->flag & SELECT) || (dupe_flag & SEQ_DUPE_ALL)) {
      Sequence *seqn = seq_dupli(scene_src, scene_dst, nseqbase, seq, dupe_flag, flag);

      if (seqn == nullptr) {
        continue; /* Should never fail. */
      }

      if (seq->type == SEQ_TYPE_META) {
        /* Always include meta all strip children. */
        int dupe_flag_recursive = dupe_flag | SEQ_DUPE_ALL | SEQ_DUPE_IS_RECURSIVE_CALL;
        SEQ_sequence_base_dupli_recursive(
            scene_src, scene_dst, &seqn->seqbase, &seq->seqbase, dupe_flag_recursive, flag);
      }
    }
  }

  /* Fix modifier links recursively from the top level only, when all sequences have been
   * copied. */
  if (dupe_flag & SEQ_DUPE_IS_RECURSIVE_CALL) {
    return;
  }

  /* fix modifier linking */
  LISTBASE_FOREACH (Sequence *, seq, nseqbase) {
    seq_new_fix_links_recursive(seq);
  }
}

bool SEQ_valid_strip_channel(Sequence *seq)
{
  if (seq->machine < 1) {
    return false;
  }
  if (seq->machine > MAXSEQ) {
    return false;
  }
  return true;
}

SequencerToolSettings *SEQ_tool_settings_copy(SequencerToolSettings *tool_settings)
{
  SequencerToolSettings *tool_settings_copy = static_cast<SequencerToolSettings *>(
      MEM_dupallocN(tool_settings));
  return tool_settings_copy;
}

/** \} */

static bool seq_set_strip_done_cb(Sequence *seq, void * /*userdata*/)
{
  if (seq->strip) {
    seq->strip->done = false;
  }
  return true;
}

static bool seq_write_data_cb(Sequence *seq, void *userdata)
{
  BlendWriter *writer = (BlendWriter *)userdata;
  BLO_write_struct(writer, Sequence, seq);
  if (seq->strip && seq->strip->done == 0) {
    /* Write strip with 'done' at 0 because read-file. */

    /* TODO this doesn't depend on the `Strip` data to be present? */
    if (seq->effectdata) {
      switch (seq->type) {
        case SEQ_TYPE_COLOR:
          BLO_write_struct(writer, SolidColorVars, seq->effectdata);
          break;
        case SEQ_TYPE_SPEED:
          BLO_write_struct(writer, SpeedControlVars, seq->effectdata);
          break;
        case SEQ_TYPE_WIPE:
          BLO_write_struct(writer, WipeVars, seq->effectdata);
          break;
        case SEQ_TYPE_GLOW:
          BLO_write_struct(writer, GlowVars, seq->effectdata);
          break;
        case SEQ_TYPE_TRANSFORM:
          BLO_write_struct(writer, TransformVars, seq->effectdata);
          break;
        case SEQ_TYPE_GAUSSIAN_BLUR:
          BLO_write_struct(writer, GaussianBlurVars, seq->effectdata);
          break;
        case SEQ_TYPE_TEXT:
          BLO_write_struct(writer, TextVars, seq->effectdata);
          break;
        case SEQ_TYPE_COLORMIX:
          BLO_write_struct(writer, ColorMixVars, seq->effectdata);
          break;
      }
    }

    BLO_write_struct(writer, Stereo3dFormat, seq->stereo3d_format);

    Strip *strip = seq->strip;
    BLO_write_struct(writer, Strip, strip);
    if (strip->crop) {
      BLO_write_struct(writer, StripCrop, strip->crop);
    }
    if (strip->transform) {
      BLO_write_struct(writer, StripTransform, strip->transform);
    }
    if (strip->proxy) {
      BLO_write_struct(writer, StripProxy, strip->proxy);
    }
    if (seq->type == SEQ_TYPE_IMAGE) {
      BLO_write_struct_array(writer,
                             StripElem,
                             MEM_allocN_len(strip->stripdata) / sizeof(StripElem),
                             strip->stripdata);
    }
    else if (ELEM(seq->type, SEQ_TYPE_MOVIE, SEQ_TYPE_SOUND_RAM)) {
      BLO_write_struct(writer, StripElem, strip->stripdata);
    }

    strip->done = true;
  }

  if (seq->prop) {
    IDP_BlendWrite(writer, seq->prop);
  }

  SEQ_modifier_blend_write(writer, &seq->modifiers);

  LISTBASE_FOREACH (SeqTimelineChannel *, channel, &seq->channels) {
    BLO_write_struct(writer, SeqTimelineChannel, channel);
  }

  if (seq->retiming_keys != nullptr) {
    int size = SEQ_retiming_keys_count(seq);
    BLO_write_struct_array(writer, SeqRetimingKey, size, seq->retiming_keys);
  }

  return true;
}

void SEQ_blend_write(BlendWriter *writer, ListBase *seqbase)
{
  /* reset write flags */
  SEQ_for_each_callback(seqbase, seq_set_strip_done_cb, nullptr);

  SEQ_for_each_callback(seqbase, seq_write_data_cb, writer);
}

static bool seq_read_data_cb(Sequence *seq, void *user_data)
{
  BlendDataReader *reader = (BlendDataReader *)user_data;

  /* Runtime data cleanup. */
  seq->scene_sound = nullptr;
  BLI_listbase_clear(&seq->anims);
  seq->flag &= ~SEQ_FLAG_SKIP_THUMBNAILS;

  /* Do as early as possible, so that other parts of reading can rely on valid session UID. */
  SEQ_relations_session_uid_generate(seq);

  BLO_read_data_address(reader, &seq->seq1);
  BLO_read_data_address(reader, &seq->seq2);
  BLO_read_data_address(reader, &seq->seq3);

  /* a patch: after introduction of effects with 3 input strips */
  if (seq->seq3 == nullptr) {
    seq->seq3 = seq->seq2;
  }

  BLO_read_data_address(reader, &seq->effectdata);
  BLO_read_data_address(reader, &seq->stereo3d_format);

  if (seq->type & SEQ_TYPE_EFFECT) {
    seq->flag |= SEQ_EFFECT_NOT_LOADED;
  }

  if (seq->type == SEQ_TYPE_TEXT) {
    TextVars *t = static_cast<TextVars *>(seq->effectdata);
    t->text_blf_id = SEQ_FONT_NOT_LOADED;
  }

  BLO_read_data_address(reader, &seq->prop);
  IDP_BlendDataRead(reader, &seq->prop);

  BLO_read_data_address(reader, &seq->strip);
  if (seq->strip && seq->strip->done == 0) {
    seq->strip->done = true;

    /* `SEQ_TYPE_SOUND_HD` case needs to be kept here, for backward compatibility. */
    if (ELEM(seq->type, SEQ_TYPE_IMAGE, SEQ_TYPE_MOVIE, SEQ_TYPE_SOUND_RAM, SEQ_TYPE_SOUND_HD)) {
      BLO_read_data_address(reader, &seq->strip->stripdata);
    }
    else {
      seq->strip->stripdata = nullptr;
    }
    BLO_read_data_address(reader, &seq->strip->crop);
    BLO_read_data_address(reader, &seq->strip->transform);
    BLO_read_data_address(reader, &seq->strip->proxy);
    if (seq->strip->proxy) {
      seq->strip->proxy->anim = nullptr;
    }
    else if (seq->flag & SEQ_USE_PROXY) {
      SEQ_proxy_set(seq, true);
    }

    /* need to load color balance to it could be converted to modifier */
    BLO_read_data_address(reader, &seq->strip->color_balance);
  }

  SEQ_modifier_blend_read_data(reader, &seq->modifiers);

  BLO_read_list(reader, &seq->channels);

  if (seq->retiming_keys != nullptr) {
    BLO_read_data_address(reader, &seq->retiming_keys);
  }

  return true;
}
void SEQ_blend_read(BlendDataReader *reader, ListBase *seqbase)
{
  SEQ_for_each_callback(seqbase, seq_read_data_cb, reader);
}

static bool seq_doversion_250_sound_proxy_update_cb(Sequence *seq, void *user_data)
{
  Main *bmain = static_cast<Main *>(user_data);
  if (seq->type == SEQ_TYPE_SOUND_HD) {
    char filepath_abs[FILE_MAX];
    BLI_path_join(
        filepath_abs, sizeof(filepath_abs), seq->strip->dirpath, seq->strip->stripdata->filename);
    BLI_path_abs(filepath_abs, BKE_main_blendfile_path(bmain));
    seq->sound = BKE_sound_new_file(bmain, filepath_abs);
    seq->type = SEQ_TYPE_SOUND_RAM;
  }
  return true;
}

void SEQ_doversion_250_sound_proxy_update(Main *bmain, Editing *ed)
{
  SEQ_for_each_callback(&ed->seqbase, seq_doversion_250_sound_proxy_update_cb, bmain);
}

/* Depsgraph update functions. */

static bool seq_mute_sound_strips_cb(Sequence *seq, void *user_data)
{
  Scene *scene = (Scene *)user_data;
  if (seq->scene_sound != nullptr) {
    BKE_sound_remove_scene_sound(scene, seq->scene_sound);
    seq->scene_sound = nullptr;
  }
  return true;
}

/* Adds sound of strip to the `scene->sound_scene` - "sound timeline". */
static void seq_update_mix_sounds(Scene *scene, Sequence *seq)
{
  if (seq->scene_sound != nullptr) {
    return;
  }

  if (seq->sound != nullptr) {
    /* Adds `seq->sound->playback_handle` to `scene->sound_scene` */
    seq->scene_sound = BKE_sound_add_scene_sound_defaults(scene, seq);
  }
  else if (seq->type == SEQ_TYPE_SCENE && seq->scene != nullptr) {
    /* Adds `seq->scene->sound_scene` to `scene->sound_scene`. */
    BKE_sound_ensure_scene(seq->scene);
    seq->scene_sound = BKE_sound_scene_add_scene_sound_defaults(scene, seq);
  }
}

static void seq_update_sound_properties(const Scene *scene, const Sequence *seq)
{
  const int frame = BKE_scene_frame_get(scene);
  BKE_sound_set_scene_sound_volume_at_frame(
      seq->scene_sound, frame, seq->volume, (seq->flag & SEQ_AUDIO_VOLUME_ANIMATED) != 0);
  SEQ_retiming_sound_animation_data_set(scene, seq);
  BKE_sound_set_scene_sound_pan_at_frame(
      seq->scene_sound, frame, seq->pan, (seq->flag & SEQ_AUDIO_PAN_ANIMATED) != 0);
}

static void seq_update_sound_modifiers(Sequence *seq)
{
  void *sound_handle = seq->sound->playback_handle;
  if (!BLI_listbase_is_empty(&seq->modifiers)) {
    LISTBASE_FOREACH (SequenceModifierData *, smd, &seq->modifiers) {
      sound_handle = SEQ_sound_modifier_recreator(seq, smd, sound_handle);
    }
  }

  /* Assign modified sound back to `seq`. */
  BKE_sound_update_sequence_handle(seq->scene_sound, sound_handle);
}

static bool must_update_strip_sound(Scene *scene, Sequence *seq)
{
  return (scene->id.recalc & (ID_RECALC_AUDIO | ID_RECALC_SYNC_TO_EVAL)) != 0 ||
         (seq->sound->id.recalc & (ID_RECALC_AUDIO | ID_RECALC_SYNC_TO_EVAL)) != 0;
}

static void seq_update_sound_strips(Scene *scene, Sequence *seq)
{
  if (seq->sound == nullptr || !must_update_strip_sound(scene, seq)) {
    return;
  }
  /* Ensure strip is playing correct sound. */
  BKE_sound_update_scene_sound(seq->scene_sound, seq->sound);
  seq_update_sound_modifiers(seq);
}

static void seq_update_scene_strip_sound(Sequence *seq)
{
  if (seq->type != SEQ_TYPE_SCENE || seq->scene == nullptr) {
    return;
  }

  /* Set `seq->scene` volume.
   * Note: Currently this doesn't work well, when this property is animated. Scene strip volume is
   * also controlled by `seq_update_sound_properties()` via `seq->volume` which works if animated.
   *
   * Ideally, the entire `BKE_scene_update_sound()` will happen from a dependency graph, so
   * then it is no longer needed to do such manual forced updates. */
  BKE_sound_set_scene_volume(seq->scene, seq->scene->audio.volume);

  /* Mute nested strips of scene when not using sequencer as input. */
  if ((seq->flag & SEQ_SCENE_STRIPS) == 0 && seq->scene->sound_scene != nullptr &&
      seq->scene->ed != nullptr)
  {
    SEQ_for_each_callback(&seq->scene->ed->seqbase, seq_mute_sound_strips_cb, seq->scene);
  }
}

static bool seq_sound_update_cb(Sequence *seq, void *user_data)
{
  Scene *scene = (Scene *)user_data;

  seq_update_mix_sounds(scene, seq);

  if (seq->scene_sound == nullptr) {
    return true;
  }

  seq_update_sound_strips(scene, seq);
  seq_update_scene_strip_sound(seq);
  seq_update_sound_properties(scene, seq);
  return true;
}

void SEQ_eval_sequences(Depsgraph *depsgraph, Scene *scene, ListBase *seqbase)
{
  DEG_debug_print_eval(depsgraph, __func__, scene->id.name, scene);
  BKE_sound_ensure_scene(scene);

  SEQ_for_each_callback(seqbase, seq_sound_update_cb, scene);

  SEQ_edit_update_muting(scene->ed);
  SEQ_sound_update_bounds_all(scene);
}
