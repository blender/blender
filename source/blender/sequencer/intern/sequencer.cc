/* SPDX-FileCopyrightText: 2001-2002 NaN Holding BV. All rights reserved.
 * SPDX-FileCopyrightText: 2003-2009 Blender Authors
 * SPDX-FileCopyrightText: 2005-2006 Peter Schlaile <peter [at] schlaile [dot] de>
 *
 * SPDX-License-Identifier: GPL-2.0-or-later */

/** \file
 * \ingroup bke
 */

#include "BKE_duplilist.hh"
#include "BLI_assert.h"
#include "BLI_map.hh"
#include "DNA_listBase.h"
#include <cstddef>
#define DNA_DEPRECATED_ALLOW

#include "MEM_guardedalloc.h"

#include "DNA_scene_types.h"
#include "DNA_sequence_types.h"
#include "DNA_sound_types.h"

#include "BLI_listbase.h"
#include "BLI_path_utils.hh"

#include "BKE_fcurve.hh"
#include "BKE_idprop.hh"
#include "BKE_lib_id.hh"
#include "BKE_main.hh"
#include "BKE_scene.hh"
#include "BKE_sound.h"

#include "DEG_depsgraph.hh"

#include "MOV_read.hh"

#include "SEQ_channels.hh"
#include "SEQ_connect.hh"
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
#include "SEQ_thumbnail_cache.hh"
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
  strip_proxy->build_tc_flags = SEQ_PROXY_TC_RECORD_RUN | SEQ_PROXY_TC_RECORD_RUN_NO_GAPS;
  strip_proxy->tc = SEQ_PROXY_TC_RECORD_RUN;
  return strip_proxy;
}

static StripData *seq_strip_alloc(int type)
{
  StripData *data = static_cast<StripData *>(MEM_callocN(sizeof(StripData), "strip"));

  if (type != STRIP_TYPE_SOUND_RAM) {
    data->transform = static_cast<StripTransform *>(
        MEM_callocN(sizeof(StripTransform), "StripTransform"));
    data->transform->scale_x = 1;
    data->transform->scale_y = 1;
    data->transform->origin[0] = 0.5f;
    data->transform->origin[1] = 0.5f;
    data->transform->filter = SEQ_TRANSFORM_FILTER_AUTO;
    data->crop = static_cast<StripCrop *>(MEM_callocN(sizeof(StripCrop), "StripCrop"));
  }

  data->us = 1;
  return data;
}

static void seq_free_strip(StripData *data)
{
  data->us--;
  if (data->us > 0) {
    return;
  }
  if (data->us < 0) {
    printf("error: negative users in strip\n");
    return;
  }

  if (data->stripdata) {
    MEM_freeN(data->stripdata);
  }

  if (data->proxy) {
    if (data->proxy->anim) {
      MOV_close(data->proxy->anim);
    }

    MEM_freeN(data->proxy);
  }
  if (data->crop) {
    MEM_freeN(data->crop);
  }
  if (data->transform) {
    MEM_freeN(data->transform);
  }

  MEM_freeN(data);
}

Strip *SEQ_sequence_alloc(ListBase *lb, int timeline_frame, int machine, int type)
{
  Strip *strip;

  strip = static_cast<Strip *>(MEM_callocN(sizeof(Strip), "addseq"));
  BLI_addtail(lb, strip);

  *((short *)strip->name) = ID_SEQ;
  strip->name[2] = 0;

  strip->flag = SELECT;
  strip->start = timeline_frame;
  strip->machine = machine;
  strip->sat = 1.0;
  strip->mul = 1.0;
  strip->blend_opacity = 100.0;
  strip->volume = 1.0f;
  strip->scene_sound = nullptr;
  strip->type = type;
  strip->media_playback_rate = 0.0f;
  strip->speed_factor = 1.0f;

  if (strip->type == STRIP_TYPE_ADJUSTMENT) {
    strip->blend_mode = STRIP_TYPE_CROSS;
  }
  else {
    strip->blend_mode = STRIP_TYPE_ALPHAOVER;
  }

  strip->data = seq_strip_alloc(type);
  strip->stereo3d_format = static_cast<Stereo3dFormat *>(
      MEM_callocN(sizeof(Stereo3dFormat), "Sequence Stereo Format"));

  strip->color_tag = STRIP_COLOR_NONE;

  if (strip->type == STRIP_TYPE_META) {
    SEQ_channels_ensure(&strip->channels);
  }

  SEQ_relations_session_uid_generate(strip);

  return strip;
}

/* only give option to skip cache locally (static func) */
static void seq_sequence_free_ex(Scene *scene,
                                 Strip *strip,
                                 const bool do_cache,
                                 const bool do_id_user)
{
  if (strip->data) {
    seq_free_strip(strip->data);
  }

  SEQ_relations_sequence_free_anim(strip);

  if (strip->type & STRIP_TYPE_EFFECT) {
    SeqEffectHandle sh = SEQ_effect_handle_get(strip);
    sh.free(strip, do_id_user);
  }

  if (strip->sound && do_id_user) {
    id_us_min((ID *)strip->sound);
  }

  if (strip->stereo3d_format) {
    MEM_freeN(strip->stereo3d_format);
  }

  /* clipboard has no scene and will never have a sound handle or be active
   * same goes to sequences copy for proxy rebuild job
   */
  if (scene) {
    Editing *ed = scene->ed;

    if (ed->act_seq == strip) {
      ed->act_seq = nullptr;
    }

    if (strip->scene_sound && ELEM(strip->type, STRIP_TYPE_SOUND_RAM, STRIP_TYPE_SCENE)) {
      BKE_sound_remove_scene_sound(scene, strip->scene_sound);
    }
  }

  if (strip->prop) {
    IDP_FreePropertyContent_ex(strip->prop, do_id_user);
    MEM_freeN(strip->prop);
  }

  /* free modifiers */
  SEQ_modifier_clear(strip);

  if (SEQ_is_strip_connected(strip)) {
    SEQ_disconnect(strip);
  }

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
      SEQ_relations_invalidate_cache_raw(scene, strip);
    }
  }
  if (strip->type == STRIP_TYPE_META) {
    SEQ_channels_free(&strip->channels);
  }

  if (strip->retiming_keys != nullptr) {
    MEM_freeN(strip->retiming_keys);
    strip->retiming_keys = nullptr;
    strip->retiming_keys_num = 0;
  }

  MEM_freeN(strip);
}

void SEQ_sequence_free(Scene *scene, Strip *strip)
{
  seq_sequence_free_ex(scene, strip, true, true);
}

void seq_free_sequence_recurse(Scene *scene, Strip *strip, const bool do_id_user)
{
  Strip *iseq, *iseq_next;

  for (iseq = static_cast<Strip *>(strip->seqbase.first); iseq; iseq = iseq_next) {
    iseq_next = iseq->next;
    seq_free_sequence_recurse(scene, iseq, do_id_user);
  }

  seq_sequence_free_ex(scene, strip, false, do_id_user);
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
    ed->cache_flag = (SEQ_CACHE_STORE_FINAL_OUT | SEQ_CACHE_STORE_RAW);
    ed->show_missing_media_flag = SEQ_EDIT_SHOW_MISSING_MEDIA;
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
  LISTBASE_FOREACH_MUTABLE (Strip *, strip, &ed->seqbase) {
    seq_free_sequence_recurse(scene, strip, do_id_user);
  }

  BLI_freelistN(&ed->metastack);
  SEQ_strip_lookup_free(scene);
  blender::seq::media_presence_free(scene);
  blender::seq::thumbnail_cache_destroy(scene);
  SEQ_channels_free(&ed->channels);

  MEM_freeN(ed);

  scene->ed = nullptr;
}

static void seq_new_fix_links_recursive(Strip *strip, blender::Map<Strip *, Strip *> strip_map)
{
  if (strip->type & STRIP_TYPE_EFFECT) {
    strip->seq1 = strip_map.lookup_default(strip->seq1, strip->seq1);
    strip->seq2 = strip_map.lookup_default(strip->seq2, strip->seq2);
  }

  LISTBASE_FOREACH (SequenceModifierData *, smd, &strip->modifiers) {
    smd->mask_sequence = strip_map.lookup_default(smd->mask_sequence, smd->mask_sequence);
  }

  if (SEQ_is_strip_connected(strip)) {
    LISTBASE_FOREACH (StripConnection *, con, &strip->connections) {
      con->strip_ref = strip_map.lookup_default(con->strip_ref, con->strip_ref);
    }
  }

  if (strip->type == STRIP_TYPE_META) {
    LISTBASE_FOREACH (Strip *, seqn, &strip->seqbase) {
      seq_new_fix_links_recursive(seqn, strip_map);
    }
  }
}

SequencerToolSettings *SEQ_tool_settings_init()
{
  SequencerToolSettings *tool_settings = static_cast<SequencerToolSettings *>(
      MEM_callocN(sizeof(SequencerToolSettings), "Sequencer tool settings"));
  tool_settings->fit_method = SEQ_SCALE_TO_FIT;
  tool_settings->snap_mode = SEQ_SNAP_TO_STRIPS | SEQ_SNAP_TO_CURRENT_FRAME |
                             SEQ_SNAP_TO_STRIP_HOLD | SEQ_SNAP_TO_MARKERS | SEQ_SNAP_TO_RETIMING |
                             SEQ_SNAP_TO_PREVIEW_BORDERS | SEQ_SNAP_TO_PREVIEW_CENTER |
                             SEQ_SNAP_TO_STRIPS_PREVIEW;
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

static MetaStack *seq_meta_stack_alloc(const Scene *scene, Strip *strip_meta)
{
  Editing *ed = SEQ_editing_get(scene);

  MetaStack *ms = static_cast<MetaStack *>(MEM_mallocN(sizeof(MetaStack), "metastack"));
  BLI_addhead(&ed->metastack, ms);
  ms->parseq = strip_meta;

  /* Reference to previously displayed timeline data. */
  Strip *higher_level_meta = SEQ_lookup_meta_by_strip(scene, strip_meta);
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

void SEQ_meta_stack_set(const Scene *scene, Strip *dst_seq)
{
  Editing *ed = SEQ_editing_get(scene);
  /* Clear metastack */
  BLI_freelistN(&ed->metastack);

  if (dst_seq != nullptr) {
    /* Allocate meta stack in a way, that represents meta hierarchy in timeline. */
    seq_meta_stack_alloc(scene, dst_seq);
    Strip *meta_parent = dst_seq;
    while ((meta_parent = SEQ_lookup_meta_by_strip(scene, meta_parent))) {
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

Strip *SEQ_meta_stack_pop(Editing *ed)
{
  MetaStack *ms = SEQ_meta_stack_active_get(ed);
  Strip *meta_parent = ms->parseq;
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

static Strip *strip_dupli(const Scene *scene_src,
                          Scene *scene_dst,
                          ListBase *new_seq_list,
                          Strip *strip,
                          int dupe_flag,
                          const int flag,
                          blender::Map<Strip *, Strip *> &strip_map)
{
  Strip *seqn = static_cast<Strip *>(MEM_dupallocN(strip));
  strip_map.add(strip, seqn);

  if ((flag & LIB_ID_CREATE_NO_MAIN) == 0) {
    SEQ_relations_session_uid_generate(seqn);
  }

  seqn->data = static_cast<StripData *>(MEM_dupallocN(strip->data));

  seqn->stereo3d_format = static_cast<Stereo3dFormat *>(MEM_dupallocN(strip->stereo3d_format));

  /* XXX: add F-Curve duplication stuff? */

  if (strip->data->crop) {
    seqn->data->crop = static_cast<StripCrop *>(MEM_dupallocN(strip->data->crop));
  }

  if (strip->data->transform) {
    seqn->data->transform = static_cast<StripTransform *>(MEM_dupallocN(strip->data->transform));
  }

  if (strip->data->proxy) {
    seqn->data->proxy = static_cast<StripProxy *>(MEM_dupallocN(strip->data->proxy));
    seqn->data->proxy->anim = nullptr;
  }

  if (strip->prop) {
    seqn->prop = IDP_CopyProperty_ex(strip->prop, flag);
  }

  if (seqn->modifiers.first) {
    BLI_listbase_clear(&seqn->modifiers);

    SEQ_modifier_list_copy(seqn, strip);
  }

  if (SEQ_is_strip_connected(strip)) {
    BLI_listbase_clear(&seqn->connections);
    SEQ_connections_duplicate(&seqn->connections, &strip->connections);
  }

  if (strip->type == STRIP_TYPE_META) {
    seqn->data->stripdata = nullptr;

    BLI_listbase_clear(&seqn->seqbase);
    BLI_listbase_clear(&seqn->channels);
    SEQ_channels_duplicate(&seqn->channels, &strip->channels);
  }
  else if (strip->type == STRIP_TYPE_SCENE) {
    seqn->data->stripdata = nullptr;
    if (strip->scene_sound) {
      seqn->scene_sound = BKE_sound_scene_add_scene_sound_defaults(scene_dst, seqn);
    }
  }
  else if (strip->type == STRIP_TYPE_MOVIECLIP) {
    /* avoid assert */
  }
  else if (strip->type == STRIP_TYPE_MASK) {
    /* avoid assert */
  }
  else if (strip->type == STRIP_TYPE_MOVIE) {
    seqn->data->stripdata = static_cast<StripElem *>(MEM_dupallocN(strip->data->stripdata));
    BLI_listbase_clear(&seqn->anims);
  }
  else if (strip->type == STRIP_TYPE_SOUND_RAM) {
    seqn->data->stripdata = static_cast<StripElem *>(MEM_dupallocN(strip->data->stripdata));
    seqn->scene_sound = nullptr;
    if ((flag & LIB_ID_CREATE_NO_USER_REFCOUNT) == 0) {
      id_us_plus((ID *)seqn->sound);
    }
  }
  else if (strip->type == STRIP_TYPE_IMAGE) {
    seqn->data->stripdata = static_cast<StripElem *>(MEM_dupallocN(strip->data->stripdata));
  }
  else if (strip->type & STRIP_TYPE_EFFECT) {
    SeqEffectHandle sh;
    sh = SEQ_effect_handle_get(strip);
    if (sh.copy) {
      sh.copy(seqn, strip, flag);
    }

    seqn->data->stripdata = nullptr;
  }
  else {
    /* sequence type not handled in duplicate! Expect a crash now... */
    BLI_assert_unreachable();
  }

  /* When using #STRIP_DUPE_UNIQUE_NAME, it is mandatory to add new sequences in relevant container
   * (scene or meta's one), *before* checking for unique names. Otherwise the meta's list is empty
   * and hence we miss all sequence-strips in that meta that have already been duplicated,
   * (see #55668). Note that unique name check itself could be done at a later step in calling
   * code, once all sequence-strips have bee duplicated (that was first, simpler solution),
   * but then handling of animation data will be broken (see #60194). */
  if (new_seq_list != nullptr) {
    BLI_addtail(new_seq_list, seqn);
  }

  if (scene_src == scene_dst) {
    if (dupe_flag & STRIP_DUPE_UNIQUE_NAME) {
      SEQ_sequence_base_unique_name_recursive(scene_dst, &scene_dst->ed->seqbase, seqn);
    }
  }

  if (strip->retiming_keys != nullptr) {
    seqn->retiming_keys = static_cast<SeqRetimingKey *>(MEM_dupallocN(strip->retiming_keys));
    seqn->retiming_keys_num = strip->retiming_keys_num;
  }

  return seqn;
}

static Strip *sequence_dupli_recursive_do(const Scene *scene_src,
                                          Scene *scene_dst,
                                          ListBase *new_seq_list,
                                          Strip *strip,
                                          const int dupe_flag,
                                          blender::Map<Strip *, Strip *> &strip_map)
{
  Strip *seqn = strip_dupli(scene_src, scene_dst, new_seq_list, strip, dupe_flag, 0, strip_map);
  if (strip->type == STRIP_TYPE_META) {
    LISTBASE_FOREACH (Strip *, s, &strip->seqbase) {
      sequence_dupli_recursive_do(scene_src, scene_dst, &seqn->seqbase, s, dupe_flag, strip_map);
    }
  }
  return seqn;
}

Strip *SEQ_sequence_dupli_recursive(
    const Scene *scene_src, Scene *scene_dst, ListBase *new_seq_list, Strip *strip, int dupe_flag)
{
  blender::Map<Strip *, Strip *> strip_map;

  Strip *seqn = sequence_dupli_recursive_do(
      scene_src, scene_dst, new_seq_list, strip, dupe_flag, strip_map);

  seq_new_fix_links_recursive(seqn, strip_map);
  if (SEQ_is_strip_connected(seqn)) {
    SEQ_cut_one_way_connections(seqn);
  }

  return seqn;
}

static void seqbase_dupli_recursive(const Scene *scene_src,
                                    Scene *scene_dst,
                                    ListBase *nseqbase,
                                    const ListBase *seqbase,
                                    int dupe_flag,
                                    const int flag,
                                    blender::Map<Strip *, Strip *> &strip_map)
{
  LISTBASE_FOREACH (Strip *, strip, seqbase) {
    if ((strip->flag & SELECT) == 0 && (dupe_flag & STRIP_DUPE_ALL) == 0) {
      continue;
    }

    Strip *seqn = strip_dupli(scene_src, scene_dst, nseqbase, strip, dupe_flag, flag, strip_map);
    BLI_assert(seqn != nullptr);

    if (strip->type == STRIP_TYPE_META) {
      /* Always include meta all strip children. */
      int dupe_flag_recursive = dupe_flag | STRIP_DUPE_ALL;
      seqbase_dupli_recursive(scene_src,
                              scene_dst,
                              &seqn->seqbase,
                              &strip->seqbase,
                              dupe_flag_recursive,
                              flag,
                              strip_map);
    }
  }
}

void SEQ_sequence_base_dupli_recursive(const Scene *scene_src,
                                       Scene *scene_dst,
                                       ListBase *nseqbase,
                                       const ListBase *seqbase,
                                       int dupe_flag,
                                       const int flag)
{
  blender::Map<Strip *, Strip *> strip_map;

  seqbase_dupli_recursive(scene_src, scene_dst, nseqbase, seqbase, dupe_flag, flag, strip_map);

  /* Fix effect, modifier, and connected strip links. */
  LISTBASE_FOREACH (Strip *, strip, nseqbase) {
    seq_new_fix_links_recursive(strip, strip_map);
  }
  /* One-way connections cannot be cut until after all connections are resolved. */
  LISTBASE_FOREACH (Strip *, strip, nseqbase) {
    if (SEQ_is_strip_connected(strip)) {
      SEQ_cut_one_way_connections(strip);
    }
  }
}

bool SEQ_is_valid_strip_channel(const Strip *strip)
{
  return strip->machine >= 1 && strip->machine <= SEQ_MAX_CHANNELS;
}

SequencerToolSettings *SEQ_tool_settings_copy(SequencerToolSettings *tool_settings)
{
  SequencerToolSettings *tool_settings_copy = static_cast<SequencerToolSettings *>(
      MEM_dupallocN(tool_settings));
  return tool_settings_copy;
}

/** \} */

static bool seq_set_strip_done_cb(Strip *strip, void * /*userdata*/)
{
  if (strip->data) {
    strip->data->done = false;
  }
  return true;
}

static bool strip_write_data_cb(Strip *strip, void *userdata)
{
  BlendWriter *writer = (BlendWriter *)userdata;
  BLO_write_struct(writer, Strip, strip);
  if (strip->data && strip->data->done == 0) {
    /* Write strip with 'done' at 0 because read-file. */

    /* TODO this doesn't depend on the `Strip` data to be present? */
    if (strip->effectdata) {
      switch (strip->type) {
        case STRIP_TYPE_COLOR:
          BLO_write_struct(writer, SolidColorVars, strip->effectdata);
          break;
        case STRIP_TYPE_SPEED:
          BLO_write_struct(writer, SpeedControlVars, strip->effectdata);
          break;
        case STRIP_TYPE_WIPE:
          BLO_write_struct(writer, WipeVars, strip->effectdata);
          break;
        case STRIP_TYPE_GLOW:
          BLO_write_struct(writer, GlowVars, strip->effectdata);
          break;
        case STRIP_TYPE_TRANSFORM:
          BLO_write_struct(writer, TransformVars, strip->effectdata);
          break;
        case STRIP_TYPE_GAUSSIAN_BLUR:
          BLO_write_struct(writer, GaussianBlurVars, strip->effectdata);
          break;
        case STRIP_TYPE_TEXT:
          BLO_write_struct(writer, TextVars, strip->effectdata);
          break;
        case STRIP_TYPE_COLORMIX:
          BLO_write_struct(writer, ColorMixVars, strip->effectdata);
          break;
      }
    }

    BLO_write_struct(writer, Stereo3dFormat, strip->stereo3d_format);

    StripData *data = strip->data;
    BLO_write_struct(writer, StripData, data);
    if (data->crop) {
      BLO_write_struct(writer, StripCrop, data->crop);
    }
    if (data->transform) {
      BLO_write_struct(writer, StripTransform, data->transform);
    }
    if (data->proxy) {
      BLO_write_struct(writer, StripProxy, data->proxy);
    }
    if (strip->type == STRIP_TYPE_IMAGE) {
      BLO_write_struct_array(
          writer, StripElem, MEM_allocN_len(data->stripdata) / sizeof(StripElem), data->stripdata);
    }
    else if (ELEM(strip->type, STRIP_TYPE_MOVIE, STRIP_TYPE_SOUND_RAM)) {
      BLO_write_struct(writer, StripElem, data->stripdata);
    }

    data->done = true;
  }

  if (strip->prop) {
    IDP_BlendWrite(writer, strip->prop);
  }

  SEQ_modifier_blend_write(writer, &strip->modifiers);

  LISTBASE_FOREACH (SeqTimelineChannel *, channel, &strip->channels) {
    BLO_write_struct(writer, SeqTimelineChannel, channel);
  }

  LISTBASE_FOREACH (StripConnection *, con, &strip->connections) {
    BLO_write_struct(writer, StripConnection, con);
  }

  if (strip->retiming_keys != nullptr) {
    int size = SEQ_retiming_keys_count(strip);
    BLO_write_struct_array(writer, SeqRetimingKey, size, strip->retiming_keys);
  }

  return true;
}

void SEQ_blend_write(BlendWriter *writer, ListBase *seqbase)
{
  /* reset write flags */
  SEQ_for_each_callback(seqbase, seq_set_strip_done_cb, nullptr);

  SEQ_for_each_callback(seqbase, strip_write_data_cb, writer);
}

static bool strip_read_data_cb(Strip *strip, void *user_data)
{
  BlendDataReader *reader = (BlendDataReader *)user_data;

  /* Runtime data cleanup. */
  strip->scene_sound = nullptr;
  BLI_listbase_clear(&strip->anims);

  /* Do as early as possible, so that other parts of reading can rely on valid session UID. */
  SEQ_relations_session_uid_generate(strip);

  BLO_read_struct(reader, Strip, &strip->seq1);
  BLO_read_struct(reader, Strip, &strip->seq2);

  if (strip->effectdata) {
    switch (strip->type) {
      case STRIP_TYPE_COLOR:
        BLO_read_struct(reader, SolidColorVars, &strip->effectdata);
        break;
      case STRIP_TYPE_SPEED:
        BLO_read_struct(reader, SpeedControlVars, &strip->effectdata);
        break;
      case STRIP_TYPE_WIPE:
        BLO_read_struct(reader, WipeVars, &strip->effectdata);
        break;
      case STRIP_TYPE_GLOW:
        BLO_read_struct(reader, GlowVars, &strip->effectdata);
        break;
      case STRIP_TYPE_TRANSFORM:
        BLO_read_struct(reader, TransformVars, &strip->effectdata);
        break;
      case STRIP_TYPE_GAUSSIAN_BLUR:
        BLO_read_struct(reader, GaussianBlurVars, &strip->effectdata);
        break;
      case STRIP_TYPE_TEXT:
        BLO_read_struct(reader, TextVars, &strip->effectdata);
        break;
      case STRIP_TYPE_COLORMIX:
        BLO_read_struct(reader, ColorMixVars, &strip->effectdata);
        break;
      default:
        BLI_assert_unreachable();
        strip->effectdata = nullptr;
        break;
    }
  }

  BLO_read_struct(reader, Stereo3dFormat, &strip->stereo3d_format);

  if (strip->type & STRIP_TYPE_EFFECT) {
    strip->flag |= SEQ_EFFECT_NOT_LOADED;
  }

  if (strip->type == STRIP_TYPE_TEXT) {
    TextVars *t = static_cast<TextVars *>(strip->effectdata);
    t->text_blf_id = STRIP_FONT_NOT_LOADED;
    t->runtime = nullptr;
  }

  BLO_read_struct(reader, IDProperty, &strip->prop);
  IDP_BlendDataRead(reader, &strip->prop);

  BLO_read_struct(reader, StripData, &strip->data);
  if (strip->data && strip->data->done == 0) {
    strip->data->done = true;

    /* `STRIP_TYPE_SOUND_HD` case needs to be kept here, for backward compatibility. */
    if (ELEM(strip->type,
             STRIP_TYPE_IMAGE,
             STRIP_TYPE_MOVIE,
             STRIP_TYPE_SOUND_RAM,
             STRIP_TYPE_SOUND_HD))
    {
      /* FIXME In #STRIP_TYPE_IMAGE case, there is currently no available information about the
       * length of the stored array of #StripElem.
       *
       * This is 'not a problem' because the reading code only checks that the loaded buffer is at
       * least large enough for the requested data (here a single #StripElem item), and always
       * assign the whole read memory (without any truncating). But relying on this behavior is
       * weak and should be addressed. */
      BLO_read_struct(reader, StripElem, &strip->data->stripdata);
    }
    else {
      strip->data->stripdata = nullptr;
    }
    BLO_read_struct(reader, StripCrop, &strip->data->crop);
    BLO_read_struct(reader, StripTransform, &strip->data->transform);
    BLO_read_struct(reader, StripProxy, &strip->data->proxy);
    if (strip->data->proxy) {
      strip->data->proxy->anim = nullptr;
    }
    else if (strip->flag & SEQ_USE_PROXY) {
      SEQ_proxy_set(strip, true);
    }

    /* need to load color balance to it could be converted to modifier */
    BLO_read_struct(reader, StripColorBalance, &strip->data->color_balance);
  }

  SEQ_modifier_blend_read_data(reader, &strip->modifiers);

  BLO_read_struct_list(reader, StripConnection, &strip->connections);
  LISTBASE_FOREACH (StripConnection *, con, &strip->connections) {
    if (con->strip_ref) {
      BLO_read_struct(reader, Strip, &con->strip_ref);
    }
  }

  BLO_read_struct_list(reader, SeqTimelineChannel, &strip->channels);

  if (strip->retiming_keys != nullptr) {
    const int size = SEQ_retiming_keys_count(strip);
    BLO_read_struct_array(reader, SeqRetimingKey, size, &strip->retiming_keys);
  }

  return true;
}
void SEQ_blend_read(BlendDataReader *reader, ListBase *seqbase)
{
  SEQ_for_each_callback(seqbase, strip_read_data_cb, reader);
}

static bool strip_doversion_250_sound_proxy_update_cb(Strip *strip, void *user_data)
{
  Main *bmain = static_cast<Main *>(user_data);
  if (strip->type == STRIP_TYPE_SOUND_HD) {
    char filepath_abs[FILE_MAX];
    BLI_path_join(filepath_abs,
                  sizeof(filepath_abs),
                  strip->data->dirpath,
                  strip->data->stripdata->filename);
    BLI_path_abs(filepath_abs, BKE_main_blendfile_path(bmain));
    strip->sound = BKE_sound_new_file(bmain, filepath_abs);
    strip->type = STRIP_TYPE_SOUND_RAM;
  }
  return true;
}

void SEQ_doversion_250_sound_proxy_update(Main *bmain, Editing *ed)
{
  SEQ_for_each_callback(&ed->seqbase, strip_doversion_250_sound_proxy_update_cb, bmain);
}

/* Depsgraph update functions. */

static bool seq_mute_sound_strips_cb(Strip *strip, void *user_data)
{
  Scene *scene = (Scene *)user_data;
  if (strip->scene_sound != nullptr) {
    BKE_sound_remove_scene_sound(scene, strip->scene_sound);
    strip->scene_sound = nullptr;
  }
  return true;
}

/* Adds sound of strip to the `scene->sound_scene` - "sound timeline". */
static void strip_update_mix_sounds(Scene *scene, Strip *strip)
{
  if (strip->scene_sound != nullptr) {
    return;
  }

  if (strip->sound != nullptr) {
    /* Adds `strip->sound->playback_handle` to `scene->sound_scene` */
    strip->scene_sound = BKE_sound_add_scene_sound_defaults(scene, strip);
  }
  else if (strip->type == STRIP_TYPE_SCENE && strip->scene != nullptr) {
    /* Adds `strip->scene->sound_scene` to `scene->sound_scene`. */
    BKE_sound_ensure_scene(strip->scene);
    strip->scene_sound = BKE_sound_scene_add_scene_sound_defaults(scene, strip);
  }
}

static void strip_update_sound_properties(const Scene *scene, const Strip *strip)
{
  const int frame = BKE_scene_frame_get(scene);
  BKE_sound_set_scene_sound_volume_at_frame(
      strip->scene_sound, frame, strip->volume, (strip->flag & SEQ_AUDIO_VOLUME_ANIMATED) != 0);
  SEQ_retiming_sound_animation_data_set(scene, strip);
  BKE_sound_set_scene_sound_pan_at_frame(
      strip->scene_sound, frame, strip->pan, (strip->flag & SEQ_AUDIO_PAN_ANIMATED) != 0);
}

static void strip_update_sound_modifiers(Strip *strip)
{
  void *sound_handle = strip->sound->playback_handle;
  if (!BLI_listbase_is_empty(&strip->modifiers)) {
    LISTBASE_FOREACH (SequenceModifierData *, smd, &strip->modifiers) {
      sound_handle = SEQ_sound_modifier_recreator(strip, smd, sound_handle);
    }
  }

  /* Assign modified sound back to `strip`. */
  BKE_sound_update_sequence_handle(strip->scene_sound, sound_handle);
}

static bool must_update_strip_sound(Scene *scene, Strip *strip)
{
  return (scene->id.recalc & (ID_RECALC_AUDIO | ID_RECALC_SYNC_TO_EVAL)) != 0 ||
         (strip->sound->id.recalc & (ID_RECALC_AUDIO | ID_RECALC_SYNC_TO_EVAL)) != 0;
}

static void seq_update_sound_strips(Scene *scene, Strip *strip)
{
  if (strip->sound == nullptr || !must_update_strip_sound(scene, strip)) {
    return;
  }
  /* Ensure strip is playing correct sound. */
  BKE_sound_update_scene_sound(strip->scene_sound, strip->sound);
  strip_update_sound_modifiers(strip);
}

static bool scene_sequencer_is_used(const Scene *scene, ListBase *seqbase)
{
  bool sequencer_is_used = false;
  LISTBASE_FOREACH (Strip *, strip_iter, seqbase) {
    if (strip_iter->scene == scene && (strip_iter->flag & SEQ_SCENE_STRIPS) != 0) {
      sequencer_is_used = true;
    }
    if (strip_iter->type == STRIP_TYPE_META) {
      sequencer_is_used |= scene_sequencer_is_used(scene, &strip_iter->seqbase);
    }
  }

  return sequencer_is_used;
}

static void seq_update_scene_strip_sound(const Scene *scene, Strip *strip)
{
  if (strip->type != STRIP_TYPE_SCENE || strip->scene == nullptr) {
    return;
  }

  /* Set `strip->scene` volume.
   * NOTE: Currently this doesn't work well, when this property is animated. Scene strip volume is
   * also controlled by `strip_update_sound_properties()` via `strip->volume` which works if
   * animated.
   *
   * Ideally, the entire `BKE_scene_update_sound()` will happen from a dependency graph, so
   * then it is no longer needed to do such manual forced updates. */
  BKE_sound_set_scene_volume(strip->scene, strip->scene->audio.volume);

  /* Mute sound when all scene strips using particular scene are not rendering sequencer strips. */
  bool sequencer_is_used = scene_sequencer_is_used(strip->scene, &scene->ed->seqbase);

  if (!sequencer_is_used && strip->scene->sound_scene != nullptr && strip->scene->ed != nullptr) {
    SEQ_for_each_callback(&strip->scene->ed->seqbase, seq_mute_sound_strips_cb, strip->scene);
  }
}

static bool strip_sound_update_cb(Strip *strip, void *user_data)
{
  Scene *scene = (Scene *)user_data;

  strip_update_mix_sounds(scene, strip);

  if (strip->scene_sound == nullptr) {
    return true;
  }

  seq_update_sound_strips(scene, strip);
  seq_update_scene_strip_sound(scene, strip);
  strip_update_sound_properties(scene, strip);
  return true;
}

void SEQ_eval_sequences(Depsgraph *depsgraph, Scene *scene, ListBase *seqbase)
{
  DEG_debug_print_eval(depsgraph, __func__, scene->id.name, scene);
  BKE_sound_ensure_scene(scene);

  SEQ_for_each_callback(seqbase, strip_sound_update_cb, scene);

  SEQ_edit_update_muting(scene->ed);
  SEQ_sound_update_bounds_all(scene);
}
