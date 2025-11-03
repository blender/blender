/* SPDX-FileCopyrightText: 2001-2002 NaN Holding BV. All rights reserved.
 * SPDX-FileCopyrightText: 2003-2009 Blender Authors
 * SPDX-FileCopyrightText: 2005-2006 Peter Schlaile <peter [at] schlaile [dot] de>
 *
 * SPDX-License-Identifier: GPL-2.0-or-later */

/** \file
 * \ingroup bke
 */

#define DNA_DEPRECATED_ALLOW

#include <cstddef>

#include "MEM_guardedalloc.h"

#include "DNA_listBase.h"
#include "DNA_mask_types.h"
#include "DNA_scene_types.h"
#include "DNA_sequence_types.h"
#include "DNA_sound_types.h"

#include "BLI_assert.h"
#include "BLI_listbase.h"
#include "BLI_map.hh"
#include "BLI_path_utils.hh"
#include "BLI_string_utf8.h"

#include "BKE_duplilist.hh"
#include "BKE_fcurve.hh"
#include "BKE_idprop.hh"
#include "BKE_lib_id.hh"
#include "BKE_main.hh"
#include "BKE_scene.hh"
#include "BKE_sound.hh"

#include "DEG_depsgraph.hh"

#include "MOV_read.hh"

#include "SEQ_channels.hh"
#include "SEQ_connect.hh"
#include "SEQ_edit.hh"
#include "SEQ_iterator.hh"
#include "SEQ_modifier.hh"
#include "SEQ_preview_cache.hh"
#include "SEQ_proxy.hh"
#include "SEQ_relations.hh"
#include "SEQ_retiming.hh"
#include "SEQ_sequencer.hh"
#include "SEQ_sound.hh"
#include "SEQ_thumbnail_cache.hh"
#include "SEQ_time.hh"
#include "SEQ_transform.hh"
#include "SEQ_utils.hh"

#include "BLO_read_write.hh"

#include "cache/final_image_cache.hh"
#include "cache/intra_frame_cache.hh"
#include "cache/source_image_cache.hh"
#include "effects/effects.hh"
#include "modifiers/modifier.hh"
#include "prefetch.hh"
#include "sequencer.hh"

namespace blender::seq {

/* -------------------------------------------------------------------- */
/** \name Allocate / Free Functions
 * \{ */

StripProxy *seq_strip_proxy_alloc()
{
  StripProxy *strip_proxy = MEM_callocN<StripProxy>("StripProxy");
  strip_proxy->quality = 50;
  strip_proxy->build_tc_flags = SEQ_PROXY_TC_RECORD_RUN | SEQ_PROXY_TC_RECORD_RUN_NO_GAPS;
  strip_proxy->tc = SEQ_PROXY_TC_RECORD_RUN;
  return strip_proxy;
}

static StripData *seq_strip_alloc(int type)
{
  StripData *data = MEM_callocN<StripData>("strip");

  if (type != STRIP_TYPE_SOUND_RAM) {
    data->transform = MEM_callocN<StripTransform>("StripTransform");
    data->transform->scale_x = 1;
    data->transform->scale_y = 1;
    data->transform->origin[0] = 0.5f;
    data->transform->origin[1] = 0.5f;
    data->transform->filter = SEQ_TRANSFORM_FILTER_AUTO;
    data->crop = MEM_callocN<StripCrop>("StripCrop");
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

Strip *strip_alloc(ListBase *lb, int timeline_frame, int channel, int type)
{
  Strip *strip;

  strip = MEM_callocN<Strip>("addseq");
  BLI_addtail(lb, strip);

  *((short *)strip->name) = ID_SEQ;
  strip->name[2] = 0;

  strip->flag = SELECT;
  strip->start = timeline_frame;
  strip_channel_set(strip, channel);
  strip->sat = 1.0;
  strip->mul = 1.0;
  strip->blend_opacity = 100.0;
  strip->volume = 1.0f;
  strip->scene_sound = nullptr;
  strip->type = type;
  strip->media_playback_rate = 0.0f;
  strip->speed_factor = 1.0f;

  if (strip->type == STRIP_TYPE_ADJUSTMENT) {
    strip->blend_mode = STRIP_BLEND_CROSS;
  }
  else {
    strip->blend_mode = STRIP_BLEND_ALPHAOVER;
  }

  strip->data = seq_strip_alloc(type);
  strip->stereo3d_format = MEM_callocN<Stereo3dFormat>("Sequence Stereo Format");

  strip->color_tag = STRIP_COLOR_NONE;

  if (strip->type == STRIP_TYPE_META) {
    channels_ensure(&strip->channels);
  }

  relations_session_uid_generate(strip);

  return strip;
}

/* only give option to skip cache locally (static func) */
static void seq_strip_free_ex(Scene *scene,
                              Strip *strip,
                              const bool do_cache,
                              const bool do_id_user)
{
  if (strip->data) {
    seq_free_strip(strip->data);
  }

  relations_strip_free_anim(strip);

  if (strip->is_effect()) {
    EffectHandle sh = strip_effect_handle_get(strip);
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

    if (ed->act_strip == strip) {
      ed->act_strip = nullptr;
    }

    if (strip->scene_sound && ELEM(strip->type, STRIP_TYPE_SOUND_RAM, STRIP_TYPE_SCENE)) {
      BKE_sound_remove_scene_sound(scene, strip->scene_sound);
    }
  }

  if (strip->prop) {
    IDP_FreePropertyContent_ex(strip->prop, do_id_user);
    MEM_freeN(strip->prop);
  }
  if (strip->system_properties) {
    IDP_FreePropertyContent_ex(strip->system_properties, do_id_user);
    MEM_freeN(strip->system_properties);
  }

  /* free modifiers */
  modifier_clear(strip);

  if (is_strip_connected(strip)) {
    disconnect(strip);
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
      relations_invalidate_cache_raw(scene, strip);
    }
  }
  if (strip->type == STRIP_TYPE_META) {
    channels_free(&strip->channels);
  }

  if (strip->retiming_keys != nullptr) {
    MEM_freeN(strip->retiming_keys);
    strip->retiming_keys = nullptr;
    strip->retiming_keys_num = 0;
  }

  MEM_freeN(strip);
}

void strip_free(Scene *scene, Strip *strip)
{
  seq_strip_free_ex(scene, strip, true, true);
}

void seq_free_strip_recurse(Scene *scene, Strip *strip, const bool do_id_user)
{
  Strip *istrip, *istrip_next;

  for (istrip = static_cast<Strip *>(strip->seqbase.first); istrip; istrip = istrip_next) {
    istrip_next = istrip->next;
    seq_free_strip_recurse(scene, istrip, do_id_user);
  }

  seq_strip_free_ex(scene, strip, false, do_id_user);
}

Editing *editing_get(const Scene *scene)
{
  return scene ? scene->ed : nullptr;
}

Editing *editing_ensure(Scene *scene)
{
  if (scene->ed == nullptr) {
    Editing *ed;

    ed = scene->ed = MEM_callocN<Editing>("addseq");
    ed->cache_flag = (SEQ_CACHE_PREFETCH_ENABLE | SEQ_CACHE_STORE_FINAL_OUT | SEQ_CACHE_STORE_RAW);
    ed->show_missing_media_flag = SEQ_EDIT_SHOW_MISSING_MEDIA;
    channels_ensure(&ed->channels);
  }

  return scene->ed;
}

void editing_free(Scene *scene, const bool do_id_user)
{
  Editing *ed = scene->ed;

  if (ed == nullptr) {
    return;
  }

  seq_prefetch_free(scene);

  /* handle cache freeing above */
  LISTBASE_FOREACH_MUTABLE (Strip *, strip, &ed->seqbase) {
    seq_free_strip_recurse(scene, strip, do_id_user);
  }

  BLI_freelistN(&ed->metastack);
  strip_lookup_free(ed);
  media_presence_free(scene);
  thumbnail_cache_destroy(scene);
  intra_frame_cache_destroy(scene);
  source_image_cache_destroy(scene);
  final_image_cache_destroy(scene);
  preview_cache_destroy(scene);
  channels_free(&ed->channels);

  MEM_freeN(ed);

  scene->ed = nullptr;
}

static void seq_new_fix_links_recursive(Strip *strip, Map<Strip *, Strip *> strip_map)
{
  if (strip->is_effect()) {
    strip->input1 = strip_map.lookup_default(strip->input1, strip->input1);
    strip->input2 = strip_map.lookup_default(strip->input2, strip->input2);
  }

  LISTBASE_FOREACH (StripModifierData *, smd, &strip->modifiers) {
    smd->mask_strip = strip_map.lookup_default(smd->mask_strip, smd->mask_strip);
  }

  if (is_strip_connected(strip)) {
    LISTBASE_FOREACH (StripConnection *, con, &strip->connections) {
      con->strip_ref = strip_map.lookup_default(con->strip_ref, con->strip_ref);
    }
  }

  if (strip->type == STRIP_TYPE_META) {
    LISTBASE_FOREACH (Strip *, strip_n, &strip->seqbase) {
      seq_new_fix_links_recursive(strip_n, strip_map);
    }
  }
}

SequencerToolSettings *tool_settings_init()
{
  SequencerToolSettings *tool_settings = MEM_callocN<SequencerToolSettings>(
      "Sequencer tool settings");
  tool_settings->fit_method = SEQ_SCALE_TO_FIT;
  tool_settings->snap_mode = SEQ_SNAP_TO_STRIPS | SEQ_SNAP_TO_CURRENT_FRAME |
                             SEQ_SNAP_TO_STRIP_HOLD | SEQ_SNAP_TO_MARKERS | SEQ_SNAP_TO_RETIMING |
                             SEQ_SNAP_TO_PREVIEW_BORDERS | SEQ_SNAP_TO_PREVIEW_CENTER |
                             SEQ_SNAP_TO_STRIPS_PREVIEW | SEQ_SNAP_TO_FRAME_RANGE;
  tool_settings->snap_distance = 15;
  tool_settings->overlap_mode = SEQ_OVERLAP_SHUFFLE;
  tool_settings->pivot_point = V3D_AROUND_LOCAL_ORIGINS;

  return tool_settings;
}

SequencerToolSettings *tool_settings_ensure(Scene *scene)
{
  SequencerToolSettings *tool_settings = scene->toolsettings->sequencer_tool_settings;
  if (tool_settings == nullptr) {
    scene->toolsettings->sequencer_tool_settings = tool_settings_init();
    tool_settings = scene->toolsettings->sequencer_tool_settings;
  }

  return tool_settings;
}

void tool_settings_free(SequencerToolSettings *tool_settings)
{
  MEM_freeN(tool_settings);
}

eSeqImageFitMethod tool_settings_fit_method_get(Scene *scene)
{
  const SequencerToolSettings *tool_settings = tool_settings_ensure(scene);
  return eSeqImageFitMethod(tool_settings->fit_method);
}

short tool_settings_snap_mode_get(Scene *scene)
{
  const SequencerToolSettings *tool_settings = tool_settings_ensure(scene);
  return tool_settings->snap_mode;
}

short tool_settings_snap_flag_get(Scene *scene)
{
  const SequencerToolSettings *tool_settings = tool_settings_ensure(scene);
  return tool_settings->snap_flag;
}

int tool_settings_snap_distance_get(Scene *scene)
{
  const SequencerToolSettings *tool_settings = tool_settings_ensure(scene);
  return tool_settings->snap_distance;
}

void tool_settings_fit_method_set(Scene *scene, eSeqImageFitMethod fit_method)
{
  SequencerToolSettings *tool_settings = tool_settings_ensure(scene);
  tool_settings->fit_method = fit_method;
}

eSeqOverlapMode tool_settings_overlap_mode_get(Scene *scene)
{
  const SequencerToolSettings *tool_settings = tool_settings_ensure(scene);
  return eSeqOverlapMode(tool_settings->overlap_mode);
}

int tool_settings_pivot_point_get(Scene *scene)
{
  const SequencerToolSettings *tool_settings = tool_settings_ensure(scene);
  return tool_settings->pivot_point;
}

ListBase *active_seqbase_get(const Editing *ed)
{
  return ed ? ed->current_strips() : nullptr;
}

static MetaStack *seq_meta_stack_alloc(const Scene *scene, Strip *strip_meta)
{
  Editing *ed = editing_get(scene);

  MetaStack *ms = MEM_mallocN<MetaStack>("metastack");
  BLI_addhead(&ed->metastack, ms);
  ms->parent_strip = strip_meta;

  /* Reference to previously displayed timeline data. */
  ms->old_strip = lookup_meta_by_strip(ed, strip_meta);

  ms->disp_range[0] = time_left_handle_frame_get(scene, ms->parent_strip);
  ms->disp_range[1] = time_right_handle_frame_get(scene, ms->parent_strip);
  return ms;
}

MetaStack *meta_stack_active_get(const Editing *ed)
{
  if (ed == nullptr) {
    return nullptr;
  }

  return static_cast<MetaStack *>(ed->metastack.last);
}

void meta_stack_set(const Scene *scene, Strip *dst)
{
  Editing *ed = editing_get(scene);
  /* Clear metastack */
  BLI_freelistN(&ed->metastack);

  if (dst != nullptr) {
    /* Allocate meta stack in a way, that represents meta hierarchy in timeline. */
    seq_meta_stack_alloc(scene, dst);
    Strip *meta_parent = dst;
    while ((meta_parent = lookup_meta_by_strip(ed, meta_parent))) {
      seq_meta_stack_alloc(scene, meta_parent);
    }

    ed->current_meta_strip = dst;
  }
  else {
    ed->current_meta_strip = nullptr;
  }
}

Strip *meta_stack_pop(Editing *ed)
{
  MetaStack *ms = meta_stack_active_get(ed);
  Strip *meta_parent = ms->parent_strip;
  ed->current_meta_strip = ms->old_strip;
  BLI_remlink(&ed->metastack, ms);
  MEM_freeN(ms);
  return meta_parent;
}

/** \} */

/* -------------------------------------------------------------------- */
/** \name Duplicate Functions
 * \{ */

static Strip *strip_duplicate(Main *bmain,
                              const Scene *scene_src,
                              Scene *scene_dst,
                              ListBase *new_seq_list,
                              Strip *strip,
                              const StripDuplicate dupe_flag,
                              const int flag,
                              Map<Strip *, Strip *> &strip_map)
{
  Strip *strip_new = static_cast<Strip *>(MEM_dupallocN(strip));
  strip_map.add(strip, strip_new);

  if ((flag & LIB_ID_CREATE_NO_MAIN) == 0) {
    relations_session_uid_generate(strip_new);
  }

  strip_new->data = static_cast<StripData *>(MEM_dupallocN(strip->data));

  strip_new->stereo3d_format = static_cast<Stereo3dFormat *>(
      MEM_dupallocN(strip->stereo3d_format));

  /* XXX: add F-Curve duplication stuff? */

  if (strip->data->crop) {
    strip_new->data->crop = static_cast<StripCrop *>(MEM_dupallocN(strip->data->crop));
  }

  if (strip->data->transform) {
    strip_new->data->transform = static_cast<StripTransform *>(
        MEM_dupallocN(strip->data->transform));
  }

  if (strip->data->proxy) {
    strip_new->data->proxy = static_cast<StripProxy *>(MEM_dupallocN(strip->data->proxy));
    strip_new->data->proxy->anim = nullptr;
  }

  if (strip->prop) {
    strip_new->prop = IDP_CopyProperty_ex(strip->prop, flag);
  }
  if (strip->system_properties) {
    strip_new->system_properties = IDP_CopyProperty_ex(strip->system_properties, flag);
  }

  if (strip_new->modifiers.first) {
    BLI_listbase_clear(&strip_new->modifiers);

    modifier_list_copy(strip_new, strip);
  }
  BLI_assert(modifier_persistent_uids_are_valid(*strip));

  if (is_strip_connected(strip)) {
    BLI_listbase_clear(&strip_new->connections);
    connections_duplicate(&strip_new->connections, &strip->connections);
  }

  if (strip->type == STRIP_TYPE_META) {
    strip_new->data->stripdata = nullptr;

    BLI_listbase_clear(&strip_new->seqbase);
    BLI_listbase_clear(&strip_new->channels);
    channels_duplicate(&strip_new->channels, &strip->channels);
  }
  else if (strip->type == STRIP_TYPE_SCENE) {
    if (flag_is_set(dupe_flag, StripDuplicate::Data) && strip_new->scene != nullptr) {
      Scene *scene_old = strip_new->scene;
      strip_new->scene = BKE_scene_duplicate(bmain, scene_old, SCE_COPY_FULL);
    }
    strip_new->data->stripdata = nullptr;
    if (strip->scene_sound) {
      strip_new->scene_sound = BKE_sound_scene_add_scene_sound_defaults(scene_dst, strip_new);
    }
  }
  else if (strip->type == STRIP_TYPE_MOVIECLIP) {
    if (flag_is_set(dupe_flag, StripDuplicate::Data) && strip_new->clip != nullptr) {
      MovieClip *clip_old = strip_new->clip;
      strip_new->clip = reinterpret_cast<MovieClip *>(
          BKE_id_copy(bmain, reinterpret_cast<ID *>(clip_old)));
      if (flag & LIB_ID_CREATE_NO_USER_REFCOUNT) {
        id_us_min(&strip_new->clip->id);
      }
    }
  }
  else if (strip->type == STRIP_TYPE_MASK) {
    if (flag_is_set(dupe_flag, StripDuplicate::Data) && strip_new->mask != nullptr) {
      Mask *mask_old = strip_new->mask;
      strip_new->mask = reinterpret_cast<Mask *>(
          BKE_id_copy(bmain, reinterpret_cast<ID *>(mask_old)));
      if (flag & LIB_ID_CREATE_NO_USER_REFCOUNT) {
        id_us_min(&strip_new->mask->id);
      }
    }
  }
  else if (strip->type == STRIP_TYPE_MOVIE) {
    strip_new->data->stripdata = static_cast<StripElem *>(MEM_dupallocN(strip->data->stripdata));
    BLI_listbase_clear(&strip_new->anims);
  }
  else if (strip->type == STRIP_TYPE_SOUND_RAM) {
    strip_new->data->stripdata = static_cast<StripElem *>(MEM_dupallocN(strip->data->stripdata));
    strip_new->scene_sound = nullptr;
    if ((flag & LIB_ID_CREATE_NO_USER_REFCOUNT) == 0) {
      id_us_plus((ID *)strip_new->sound);
    }
  }
  else if (strip->type == STRIP_TYPE_IMAGE) {
    strip_new->data->stripdata = static_cast<StripElem *>(MEM_dupallocN(strip->data->stripdata));
  }
  else if (strip->is_effect()) {
    EffectHandle sh = strip_effect_handle_get(strip);
    if (sh.copy) {
      sh.copy(strip_new, strip, flag);
    }

    strip_new->data->stripdata = nullptr;
  }
  else {
    /* sequence type not handled in duplicate! Expect a crash now... */
    BLI_assert_unreachable();
  }

  /* When using StripDuplicate::UniqueName, it is mandatory to add new strips in relevant container
   * (scene or meta's one), *before* checking for unique names. Otherwise the meta's list is empty
   * and hence we miss all sequencer strips in that meta that have already been duplicated,
   * (see #55668). Note that unique name check itself could be done at a later step in calling
   * code, once all sequencer strips have been duplicated (that was first, simpler solution),
   * but then handling of animation data will be broken (see #60194). */
  if (new_seq_list != nullptr) {
    BLI_addtail(new_seq_list, strip_new);
  }

  if (scene_src == scene_dst) {
    if (flag_is_set(dupe_flag, StripDuplicate::UniqueName)) {
      strip_unique_name_set(scene_dst, &scene_dst->ed->seqbase, strip_new);
    }
  }

  if (strip->retiming_keys != nullptr) {
    strip_new->retiming_keys = static_cast<SeqRetimingKey *>(MEM_dupallocN(strip->retiming_keys));
    strip_new->retiming_keys_num = strip->retiming_keys_num;
  }

  return strip_new;
}

static Strip *strip_duplicate_recursive_impl(Main *bmain,
                                             const Scene *scene_src,
                                             Scene *scene_dst,
                                             ListBase *new_seq_list,
                                             Strip *strip,
                                             const StripDuplicate dupe_flag,
                                             Map<Strip *, Strip *> &strip_map)
{
  Strip *strip_new = strip_duplicate(
      bmain, scene_src, scene_dst, new_seq_list, strip, dupe_flag, 0, strip_map);
  if (strip->type == STRIP_TYPE_META) {
    LISTBASE_FOREACH (Strip *, s, &strip->seqbase) {
      strip_duplicate_recursive_impl(
          bmain, scene_src, scene_dst, &strip_new->seqbase, s, dupe_flag, strip_map);
    }
  }
  return strip_new;
}

Strip *strip_duplicate_recursive(Main *bmain,
                                 const Scene *scene_src,
                                 Scene *scene_dst,
                                 ListBase *new_seq_list,
                                 Strip *strip,
                                 const StripDuplicate dupe_flag)
{
  Map<Strip *, Strip *> strip_map;

  Strip *strip_new = strip_duplicate_recursive_impl(
      bmain, scene_src, scene_dst, new_seq_list, strip, dupe_flag, strip_map);

  seq_new_fix_links_recursive(strip_new, strip_map);
  if (is_strip_connected(strip_new)) {
    cut_one_way_connections(strip_new);
  }

  return strip_new;
}

static void seqbase_dupli_recursive(Main *bmain,
                                    const Scene *scene_src,
                                    Scene *scene_dst,
                                    ListBase *nseqbase,
                                    const ListBase *seqbase,
                                    const StripDuplicate dupe_flag,
                                    const int flag,
                                    Map<Strip *, Strip *> &strip_map)
{
  LISTBASE_FOREACH (Strip *, strip, seqbase) {
    if ((strip->flag & SELECT) == 0 && !flag_is_set(dupe_flag, StripDuplicate::All)) {
      continue;
    }

    Strip *strip_new = strip_duplicate(
        bmain, scene_src, scene_dst, nseqbase, strip, dupe_flag, flag, strip_map);
    BLI_assert(strip_new != nullptr);

    if (strip->type == STRIP_TYPE_META) {
      /* Always include meta all strip children. */
      const StripDuplicate dupe_flag_recursive = dupe_flag | StripDuplicate::All;
      seqbase_dupli_recursive(bmain,
                              scene_src,
                              scene_dst,
                              &strip_new->seqbase,
                              &strip->seqbase,
                              dupe_flag_recursive,
                              flag,
                              strip_map);
    }
  }
}

void seqbase_duplicate_recursive(Main *bmain,
                                 const Scene *scene_src,
                                 Scene *scene_dst,
                                 ListBase *nseqbase,
                                 const ListBase *seqbase,
                                 const StripDuplicate dupe_flag,
                                 const int flag)
{
  Map<Strip *, Strip *> strip_map;

  seqbase_dupli_recursive(
      bmain, scene_src, scene_dst, nseqbase, seqbase, dupe_flag, flag, strip_map);

  /* Fix effect, modifier, and connected strip links. */
  LISTBASE_FOREACH (Strip *, strip, nseqbase) {
    seq_new_fix_links_recursive(strip, strip_map);
  }
  /* One-way connections cannot be cut until after all connections are resolved. */
  LISTBASE_FOREACH (Strip *, strip, nseqbase) {
    if (is_strip_connected(strip)) {
      cut_one_way_connections(strip);
    }
  }
}

bool is_valid_strip_channel(const Strip *strip)
{
  return strip->channel >= 1 && strip->channel <= MAX_CHANNELS;
}

SequencerToolSettings *tool_settings_copy(SequencerToolSettings *tool_settings)
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
        case STRIP_TYPE_GAUSSIAN_BLUR:
          BLO_write_struct(writer, GaussianBlurVars, strip->effectdata);
          break;
        case STRIP_TYPE_TEXT: {
          TextVars *text = static_cast<TextVars *>(strip->effectdata);
          if (!BLO_write_is_undo(writer)) {
            /* Copy current text into legacy buffer. */
            STRNCPY_UTF8(text->text_legacy, text->text_ptr);
          }
          BLO_write_struct(writer, TextVars, text);
          BLO_write_string(writer, text->text_ptr);
        } break;
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
  if (strip->system_properties) {
    IDP_BlendWrite(writer, strip->system_properties);
  }

  modifier_blend_write(writer, &strip->modifiers);

  LISTBASE_FOREACH (SeqTimelineChannel *, channel, &strip->channels) {
    BLO_write_struct(writer, SeqTimelineChannel, channel);
  }

  LISTBASE_FOREACH (StripConnection *, con, &strip->connections) {
    BLO_write_struct(writer, StripConnection, con);
  }

  if (strip->retiming_keys != nullptr) {
    int size = retiming_keys_count(strip);
    BLO_write_struct_array(writer, SeqRetimingKey, size, strip->retiming_keys);
  }

  return true;
}

void blend_write(BlendWriter *writer, ListBase *seqbase)
{
  /* reset write flags */
  foreach_strip(seqbase, seq_set_strip_done_cb, nullptr);

  foreach_strip(seqbase, strip_write_data_cb, writer);
}

static bool strip_read_data_cb(Strip *strip, void *user_data)
{
  BlendDataReader *reader = (BlendDataReader *)user_data;

  /* Runtime data cleanup. */
  strip->scene_sound = nullptr;
  BLI_listbase_clear(&strip->anims);

  /* Do as early as possible, so that other parts of reading can rely on valid session UID. */
  relations_session_uid_generate(strip);

  BLO_read_struct(reader, Strip, &strip->input1);
  BLO_read_struct(reader, Strip, &strip->input2);

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
      case STRIP_TYPE_TRANSFORM_LEGACY:
        BLO_read_struct(reader, TransformVarsLegacy, &strip->effectdata);
        break;
      case STRIP_TYPE_GAUSSIAN_BLUR:
        BLO_read_struct(reader, GaussianBlurVars, &strip->effectdata);
        break;
      case STRIP_TYPE_TEXT: {
        BLO_read_struct(reader, TextVars, &strip->effectdata);
        TextVars *text = static_cast<TextVars *>(strip->effectdata);
        BLO_read_string(reader, &text->text_ptr);
        text->text_len_bytes = text->text_ptr ? strlen(text->text_ptr) : 0;
      } break;
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

  if (strip->is_effect()) {
    strip->runtime.flag |= STRIP_EFFECT_NOT_LOADED;
  }

  if (strip->type == STRIP_TYPE_TEXT) {
    TextVars *t = static_cast<TextVars *>(strip->effectdata);
    t->text_blf_id = STRIP_FONT_NOT_LOADED;
    t->runtime = nullptr;
  }

  BLO_read_struct(reader, IDProperty, &strip->prop);
  IDP_BlendDataRead(reader, &strip->prop);
  BLO_read_struct(reader, IDProperty, &strip->system_properties);
  IDP_BlendDataRead(reader, &strip->system_properties);

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
      proxy_set(strip, true);
    }

    /* need to load color balance to it could be converted to modifier */
    BLO_read_struct(reader, StripColorBalance, &strip->data->color_balance_legacy);
  }

  modifier_blend_read_data(reader, &strip->modifiers);

  BLO_read_struct_list(reader, StripConnection, &strip->connections);
  LISTBASE_FOREACH (StripConnection *, con, &strip->connections) {
    if (con->strip_ref) {
      BLO_read_struct(reader, Strip, &con->strip_ref);
    }
  }

  BLO_read_struct_list(reader, SeqTimelineChannel, &strip->channels);

  if (strip->retiming_keys != nullptr) {
    const int size = retiming_keys_count(strip);
    BLO_read_struct_array(reader, SeqRetimingKey, size, &strip->retiming_keys);
  }

  return true;
}
void blend_read(BlendDataReader *reader, ListBase *seqbase)
{
  foreach_strip(seqbase, strip_read_data_cb, reader);
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

void doversion_250_sound_proxy_update(Main *bmain, Editing *ed)
{
  foreach_strip(&ed->seqbase, strip_doversion_250_sound_proxy_update_cb, bmain);
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
  retiming_sound_animation_data_set(scene, strip);
  BKE_sound_set_scene_sound_pan_at_frame(
      strip->scene_sound, frame, strip->pan, (strip->flag & SEQ_AUDIO_PAN_ANIMATED) != 0);
}

static void strip_update_sound_modifiers(Strip *strip)
{
  void *sound_handle = strip->sound->playback_handle;
  bool needs_update = false;

  LISTBASE_FOREACH (StripModifierData *, smd, &strip->modifiers) {
    sound_handle = sound_modifier_recreator(strip, smd, sound_handle, needs_update);
  }

  if (needs_update) {
    /* Assign modified sound back to `strip`. */
    BKE_sound_update_sequence_handle(strip->scene_sound, sound_handle);
  }
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
  if (BLI_listbase_is_empty(&strip->modifiers)) {
    /* Just use playback handle from sound ID. */
    BKE_sound_update_scene_sound(strip->scene_sound, strip->sound);
  }
  else {
    /* Use Playback handle from sound ID as input for modifier stack. */
    strip_update_sound_modifiers(strip);
  }
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
    foreach_strip(&strip->scene->ed->seqbase, seq_mute_sound_strips_cb, strip->scene);
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

void eval_strips(Depsgraph *depsgraph, Scene *scene, ListBase *seqbase)
{
  DEG_debug_print_eval(depsgraph, __func__, scene->id.name, scene);
  BKE_sound_ensure_scene(scene);

  foreach_strip(seqbase, strip_sound_update_cb, scene);

  edit_update_muting(scene->ed);
  sound_update_bounds_all(scene);
}

}  // namespace blender::seq

ListBase *Editing::current_strips()
{
  if (this->current_meta_strip) {
    return &this->current_meta_strip->seqbase;
  }
  return &this->seqbase;
}

ListBase *Editing::current_strips() const
{
  if (this->current_meta_strip) {
    return &this->current_meta_strip->seqbase;
  }
  /* NOTE: Const correctness is non-existent with ListBase anyway. */
  return &const_cast<ListBase &>(this->seqbase);
}

ListBase *Editing::current_channels()
{
  if (this->current_meta_strip) {
    return &this->current_meta_strip->channels;
  }
  return &this->channels;
}

ListBase *Editing::current_channels() const
{
  if (this->current_meta_strip) {
    return &this->current_meta_strip->channels;
  }
  /* NOTE: Const correctness is non-existent with ListBase anyway. */
  return &const_cast<ListBase &>(this->channels);
}

bool Strip::is_effect() const
{
  return (this->type >= STRIP_TYPE_CROSS && this->type <= STRIP_TYPE_OVERDROP_REMOVED) ||
         (this->type >= STRIP_TYPE_WIPE && this->type <= STRIP_TYPE_ADJUSTMENT) ||
         (this->type >= STRIP_TYPE_GAUSSIAN_BLUR && this->type <= STRIP_TYPE_COLORMIX);
}
