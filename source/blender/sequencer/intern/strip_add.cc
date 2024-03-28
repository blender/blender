/* SPDX-FileCopyrightText: 2001-2002 NaN Holding BV. All rights reserved.
 * SPDX-FileCopyrightText: 2003-2009 Blender Authors
 * SPDX-FileCopyrightText: 2005-2006 Peter Schlaile <peter [at] schlaile [dot] de>
 *
 * SPDX-License-Identifier: GPL-2.0-or-later */

/** \file
 * \ingroup bke
 */

#include <algorithm>
#include <cmath>
#include <cstring>

#include "MEM_guardedalloc.h"

#include "DNA_mask_types.h"
#include "DNA_scene_types.h"
#include "DNA_sequence_types.h"
#include "DNA_sound_types.h"

#include "BLI_listbase.h"
#include "BLI_path_util.h"
#include "BLI_string.h"

#include "BKE_image.h"
#include "BKE_lib_id.hh"
#include "BKE_main.hh"
#include "BKE_mask.h"
#include "BKE_movieclip.h"
#include "BKE_scene.hh"
#include "BKE_sound.h"

#include "DEG_depsgraph_query.hh"

#include "IMB_colormanagement.hh"
#include "IMB_imbuf.hh"
#include "IMB_imbuf_types.hh"
#include "IMB_metadata.hh"

#include "SEQ_add.hh"
#include "SEQ_edit.hh"
#include "SEQ_effects.hh"
#include "SEQ_relations.hh"
#include "SEQ_render.hh"
#include "SEQ_sequencer.hh"
#include "SEQ_time.hh"
#include "SEQ_transform.hh"
#include "SEQ_utils.hh"

#include "multiview.hh"
#include "proxy.hh"
#include "sequencer.hh"
#include "strip_time.hh"

void SEQ_add_load_data_init(SeqLoadData *load_data,
                            const char *name,
                            const char *path,
                            const int start_frame,
                            const int channel)
{
  memset(load_data, 0, sizeof(SeqLoadData));
  if (name != nullptr) {
    STRNCPY(load_data->name, name);
  }
  if (path != nullptr) {
    STRNCPY(load_data->path, path);
  }
  load_data->start_frame = start_frame;
  load_data->channel = channel;
}

static void seq_add_generic_update(Scene *scene, Sequence *seq)
{
  SEQ_sequence_base_unique_name_recursive(scene, &scene->ed->seqbase, seq);
  SEQ_relations_invalidate_cache_composite(scene, seq);
  SEQ_sequence_lookup_tag(scene, SEQ_LOOKUP_TAG_INVALID);
  seq_time_effect_range_set(scene, seq);
  SEQ_time_update_meta_strip_range(scene, seq_sequence_lookup_meta_by_seq(scene, seq));
}

static void seq_add_set_name(Scene *scene, Sequence *seq, SeqLoadData *load_data)
{
  if (load_data->name[0] != '\0') {
    SEQ_edit_sequence_name_set(scene, seq, load_data->name);
  }
  else {
    if (seq->type == SEQ_TYPE_SCENE) {
      SEQ_edit_sequence_name_set(scene, seq, load_data->scene->id.name + 2);
    }
    else if (seq->type == SEQ_TYPE_MOVIECLIP) {
      SEQ_edit_sequence_name_set(scene, seq, load_data->clip->id.name + 2);
    }
    else if (seq->type == SEQ_TYPE_MASK) {
      SEQ_edit_sequence_name_set(scene, seq, load_data->mask->id.name + 2);
    }
    else if ((seq->type & SEQ_TYPE_EFFECT) != 0) {
      SEQ_edit_sequence_name_set(scene, seq, SEQ_sequence_give_name(seq));
    }
    else { /* Image, sound and movie. */
      SEQ_edit_sequence_name_set(scene, seq, load_data->name);
    }
  }
}

static void seq_add_set_view_transform(Scene *scene, Sequence *seq, SeqLoadData *load_data)
{
  const char *strip_colorspace = seq->strip->colorspace_settings.name;

  if (load_data->flags & SEQ_LOAD_SET_VIEW_TRANSFORM) {
    const char *role_colorspace_byte;
    role_colorspace_byte = IMB_colormanagement_role_colorspace_name_get(COLOR_ROLE_DEFAULT_BYTE);

    if (STREQ(strip_colorspace, role_colorspace_byte)) {
      ColorManagedDisplay *display = IMB_colormanagement_display_get_named(
          scene->display_settings.display_device);
      const char *default_view_transform =
          IMB_colormanagement_display_get_default_view_transform_name(display);
      STRNCPY(scene->view_settings.view_transform, default_view_transform);
    }
  }
}

Sequence *SEQ_add_scene_strip(Scene *scene, ListBase *seqbase, SeqLoadData *load_data)
{
  Sequence *seq = SEQ_sequence_alloc(
      seqbase, load_data->start_frame, load_data->channel, SEQ_TYPE_SCENE);
  seq->scene = load_data->scene;
  seq->len = load_data->scene->r.efra - load_data->scene->r.sfra + 1;
  id_us_ensure_real((ID *)load_data->scene);
  seq_add_set_name(scene, seq, load_data);
  seq_add_generic_update(scene, seq);
  return seq;
}

Sequence *SEQ_add_movieclip_strip(Scene *scene, ListBase *seqbase, SeqLoadData *load_data)
{
  Sequence *seq = SEQ_sequence_alloc(
      seqbase, load_data->start_frame, load_data->channel, SEQ_TYPE_MOVIECLIP);
  seq->clip = load_data->clip;
  seq->len = BKE_movieclip_get_duration(load_data->clip);
  id_us_ensure_real((ID *)load_data->clip);
  seq_add_set_name(scene, seq, load_data);
  seq_add_generic_update(scene, seq);
  return seq;
}

Sequence *SEQ_add_mask_strip(Scene *scene, ListBase *seqbase, SeqLoadData *load_data)
{
  Sequence *seq = SEQ_sequence_alloc(
      seqbase, load_data->start_frame, load_data->channel, SEQ_TYPE_MASK);
  seq->mask = load_data->mask;
  seq->len = BKE_mask_get_duration(load_data->mask);
  id_us_ensure_real((ID *)load_data->mask);
  seq_add_set_name(scene, seq, load_data);
  seq_add_generic_update(scene, seq);
  return seq;
}

Sequence *SEQ_add_effect_strip(Scene *scene, ListBase *seqbase, SeqLoadData *load_data)
{
  Sequence *seq = SEQ_sequence_alloc(
      seqbase, load_data->start_frame, load_data->channel, load_data->effect.type);

  seq->flag |= SEQ_USE_EFFECT_DEFAULT_FADE;
  SeqEffectHandle sh = SEQ_effect_handle_get(seq);
  sh.init(seq);
  seq->seq1 = load_data->effect.seq1;
  seq->seq2 = load_data->effect.seq2;
  seq->seq3 = load_data->effect.seq3;

  if (SEQ_effect_get_num_inputs(seq->type) == 1) {
    seq->blend_mode = seq->seq1->blend_mode;
  }

  if (!load_data->effect.seq1) {
    seq->len = 1; /* Effect is generator, set non zero length. */
    seq->flag |= SEQ_SINGLE_FRAME_CONTENT;
    SEQ_time_right_handle_frame_set(scene, seq, load_data->effect.end_frame);
  }

  seq_add_set_name(scene, seq, load_data);
  seq_add_generic_update(scene, seq);

  return seq;
}

void SEQ_add_image_set_directory(Sequence *seq, const char *dirpath)
{
  STRNCPY(seq->strip->dirpath, dirpath);
}

void SEQ_add_image_load_file(Scene *scene, Sequence *seq, size_t strip_frame, const char *filename)
{
  StripElem *se = SEQ_render_give_stripelem(
      scene, seq, SEQ_time_start_frame_get(seq) + strip_frame);
  STRNCPY(se->filename, filename);
}

void SEQ_add_image_init_alpha_mode(Sequence *seq)
{
  if (seq->strip && seq->strip->stripdata) {
    char filepath[FILE_MAX];
    ImBuf *ibuf;

    BLI_path_join(
        filepath, sizeof(filepath), seq->strip->dirpath, seq->strip->stripdata->filename);
    BLI_path_abs(filepath, BKE_main_blendfile_path_from_global());

    /* Initialize input color space. */
    if (seq->type == SEQ_TYPE_IMAGE) {
      ibuf = IMB_loadiffname(
          filepath, IB_test | IB_alphamode_detect, seq->strip->colorspace_settings.name);

      /* Byte images are default to straight alpha, however sequencer
       * works in premul space, so mark strip to be premultiplied first.
       */
      seq->alpha_mode = SEQ_ALPHA_STRAIGHT;
      if (ibuf) {
        if (ibuf->flags & IB_alphamode_premul) {
          seq->alpha_mode = IMA_ALPHA_PREMUL;
        }

        IMB_freeImBuf(ibuf);
      }
    }
  }
}

Sequence *SEQ_add_image_strip(Main *bmain, Scene *scene, ListBase *seqbase, SeqLoadData *load_data)
{
  Sequence *seq = SEQ_sequence_alloc(
      seqbase, load_data->start_frame, load_data->channel, SEQ_TYPE_IMAGE);
  seq->len = load_data->image.len;
  Strip *strip = seq->strip;
  strip->stripdata = static_cast<StripElem *>(
      MEM_callocN(load_data->image.len * sizeof(StripElem), "stripelem"));

  if (seq->len == 1) {
    seq->flag |= SEQ_SINGLE_FRAME_CONTENT;
  }

  /* Multiview settings. */
  if (load_data->use_multiview) {
    seq->flag |= SEQ_USE_VIEWS;
    seq->views_format = load_data->views_format;
  }
  if (load_data->stereo3d_format) {
    seq->stereo3d_format = load_data->stereo3d_format;
  }

  /* Set initial scale based on load_data->fit_method. */
  char file_path[FILE_MAX];
  STRNCPY(file_path, load_data->path);
  BLI_path_abs(file_path, BKE_main_blendfile_path(bmain));
  ImBuf *ibuf = IMB_loadiffname(file_path, IB_rect, seq->strip->colorspace_settings.name);
  if (ibuf != nullptr) {
    /* Set image resolution. Assume that all images in sequence are same size. This fields are only
     * informative. */
    StripElem *strip_elem = strip->stripdata;
    for (int i = 0; i < load_data->image.len; i++) {
      strip_elem->orig_width = ibuf->x;
      strip_elem->orig_height = ibuf->y;
      strip_elem++;
    }

    SEQ_set_scale_to_fit(
        seq, ibuf->x, ibuf->y, scene->r.xsch, scene->r.ysch, load_data->fit_method);
    IMB_freeImBuf(ibuf);
  }

  /* Set Last active directory. */
  STRNCPY(scene->ed->act_imagedir, seq->strip->dirpath);
  seq_add_set_view_transform(scene, seq, load_data);
  seq_add_set_name(scene, seq, load_data);
  seq_add_generic_update(scene, seq);

  return seq;
}

#ifdef WITH_AUDASPACE

static void seq_add_sound_av_sync(Main *bmain, Scene *scene, Sequence *seq, SeqLoadData *load_data)
{
  SoundStreamInfo sound_stream;
  if (!BKE_sound_stream_info_get(bmain, load_data->path, 0, &sound_stream)) {
    return;
  }

  const double av_stream_offset = sound_stream.start - load_data->r_video_stream_start;
  const int frame_offset = av_stream_offset * FPS;
  /* Set sub-frame offset. */
  seq->sound->offset_time = (double(frame_offset) / FPS) - av_stream_offset;
  SEQ_transform_translate_sequence(scene, seq, frame_offset);
}

Sequence *SEQ_add_sound_strip(Main *bmain, Scene *scene, ListBase *seqbase, SeqLoadData *load_data)
{
  bSound *sound = BKE_sound_new_file(bmain, load_data->path); /* Handles relative paths. */
  SoundInfo info;
  bool sound_loaded = BKE_sound_info_get(bmain, sound, &info);

  if (!sound_loaded && !load_data->allow_invalid_file) {
    BKE_id_free(bmain, sound);
    return nullptr;
  }

  if (info.specs.channels == SOUND_CHANNELS_INVALID && !load_data->allow_invalid_file) {
    BKE_id_free(bmain, sound);
    return nullptr;
  }

  Sequence *seq = SEQ_sequence_alloc(
      seqbase, load_data->start_frame, load_data->channel, SEQ_TYPE_SOUND_RAM);
  seq->sound = sound;
  seq->scene_sound = nullptr;

  /* We round the frame duration as the audio sample lengths usually does not
   * line up with the video frames. Therefore we round this number to the
   * nearest frame as the audio track usually overshoots or undershoots the
   * end frame of the video by a little bit.
   * See #47135 for under shoot example. */
  seq->len = std::max(1, int(round((info.length - sound->offset_time) * FPS)));

  Strip *strip = seq->strip;
  /* We only need 1 element to store the filename. */
  StripElem *se = strip->stripdata = static_cast<StripElem *>(
      MEM_callocN(sizeof(StripElem), "stripelem"));
  BLI_path_split_dir_file(
      load_data->path, strip->dirpath, sizeof(strip->dirpath), se->filename, sizeof(se->filename));

  if (seq != nullptr && seq->sound != nullptr) {
    if (load_data->flags & SEQ_LOAD_SOUND_MONO) {
      seq->sound->flags |= SOUND_FLAGS_MONO;
    }

    if (load_data->flags & SEQ_LOAD_SOUND_CACHE) {
      if (seq->sound) {
        seq->sound->flags |= SOUND_FLAGS_CACHING;
      }
    }
  }

  seq_add_sound_av_sync(bmain, scene, seq, load_data);

  /* Set Last active directory. */
  BLI_strncpy(scene->ed->act_sounddir, strip->dirpath, FILE_MAXDIR);
  seq_add_set_name(scene, seq, load_data);
  seq_add_generic_update(scene, seq);

  return seq;
}

#else   // WITH_AUDASPACE
Sequence *SEQ_add_sound_strip(Main * /*bmain*/,
                              Scene * /*scene*/,
                              ListBase * /*seqbase*/,
                              SeqLoadData * /*load_data*/)
{
  return nullptr;
}
#endif  // WITH_AUDASPACE

Sequence *SEQ_add_meta_strip(Scene *scene, ListBase *seqbase, SeqLoadData *load_data)
{
  /* Allocate sequence. */
  Sequence *seqm = SEQ_sequence_alloc(
      seqbase, load_data->start_frame, load_data->channel, SEQ_TYPE_META);

  /* Set name. */
  seq_add_set_name(scene, seqm, load_data);

  /* Set frames start and length. */
  seqm->start = load_data->start_frame;
  seqm->len = 1;

  seq_add_generic_update(scene, seqm);

  return seqm;
}

Sequence *SEQ_add_movie_strip(Main *bmain, Scene *scene, ListBase *seqbase, SeqLoadData *load_data)
{
  char filepath[sizeof(load_data->path)];
  STRNCPY(filepath, load_data->path);
  BLI_path_abs(filepath, BKE_main_blendfile_path(bmain));

  char colorspace[64] = "\0"; /* MAX_COLORSPACE_NAME */
  bool is_multiview_loaded = false;
  const int totfiles = seq_num_files(scene, load_data->views_format, load_data->use_multiview);
  ImBufAnim **anim_arr = static_cast<ImBufAnim **>(
      MEM_callocN(sizeof(ImBufAnim *) * totfiles, "Video files"));
  int i;
  int orig_width = 0;
  int orig_height = 0;

  if (load_data->use_multiview && (load_data->views_format == R_IMF_VIEWS_INDIVIDUAL)) {
    char prefix[FILE_MAX];
    const char *ext = nullptr;
    size_t j = 0;

    BKE_scene_multiview_view_prefix_get(scene, filepath, prefix, &ext);

    if (prefix[0] != '\0') {
      for (i = 0; i < totfiles; i++) {
        char filepath_view[FILE_MAX];

        seq_multiview_name(scene, i, prefix, ext, filepath_view, sizeof(filepath_view));
        anim_arr[j] = openanim(filepath_view, IB_rect, 0, colorspace);

        if (anim_arr[j]) {
          seq_anim_add_suffix(scene, anim_arr[j], i);
          j++;
        }
      }
      is_multiview_loaded = true;
    }
  }

  if (is_multiview_loaded == false) {
    anim_arr[0] = openanim(filepath, IB_rect, 0, colorspace);
  }

  if (anim_arr[0] == nullptr && !load_data->allow_invalid_file) {
    MEM_freeN(anim_arr);
    return nullptr;
  }

  float video_fps = 0.0f;
  load_data->r_video_stream_start = 0.0;

  if (anim_arr[0] != nullptr) {
    short fps_denom;
    float fps_num;

    IMB_anim_get_fps(anim_arr[0], true, &fps_denom, &fps_num);

    video_fps = fps_denom / fps_num;

    /* Adjust scene's frame rate settings to match. */
    if (load_data->flags & SEQ_LOAD_MOVIE_SYNC_FPS) {
      scene->r.frs_sec = fps_denom;
      scene->r.frs_sec_base = fps_num;
      DEG_id_tag_update(&scene->id, ID_RECALC_AUDIO_FPS | ID_RECALC_SEQUENCER_STRIPS);
    }

    load_data->r_video_stream_start = IMD_anim_get_offset(anim_arr[0]);
  }

  Sequence *seq = SEQ_sequence_alloc(
      seqbase, load_data->start_frame, load_data->channel, SEQ_TYPE_MOVIE);

  /* Multiview settings. */
  if (load_data->use_multiview) {
    seq->flag |= SEQ_USE_VIEWS;
    seq->views_format = load_data->views_format;
  }
  if (load_data->stereo3d_format) {
    seq->stereo3d_format = load_data->stereo3d_format;
  }

  for (i = 0; i < totfiles; i++) {
    if (anim_arr[i]) {
      StripAnim *sanim = static_cast<StripAnim *>(MEM_mallocN(sizeof(StripAnim), "Strip Anim"));
      BLI_addtail(&seq->anims, sanim);
      sanim->anim = anim_arr[i];
    }
    else {
      break;
    }
  }

  if (anim_arr[0] != nullptr) {
    seq->len = IMB_anim_get_duration(anim_arr[0], IMB_TC_RECORD_RUN);

    IMB_anim_load_metadata(anim_arr[0]);

    /* Set initial scale based on load_data->fit_method. */
    orig_width = IMB_anim_get_image_width(anim_arr[0]);
    orig_height = IMB_anim_get_image_height(anim_arr[0]);
    SEQ_set_scale_to_fit(
        seq, orig_width, orig_height, scene->r.xsch, scene->r.ysch, load_data->fit_method);

    short frs_sec;
    float frs_sec_base;
    if (IMB_anim_get_fps(anim_arr[0], true, &frs_sec, &frs_sec_base)) {
      seq->media_playback_rate = float(frs_sec) / frs_sec_base;
    }
  }

  seq->len = std::max(1, seq->len);
  if (load_data->adjust_playback_rate) {
    seq->flag |= SEQ_AUTO_PLAYBACK_RATE;
  }

  STRNCPY(seq->strip->colorspace_settings.name, colorspace);

  Strip *strip = seq->strip;
  /* We only need 1 element for MOVIE strips. */
  StripElem *se;
  strip->stripdata = se = static_cast<StripElem *>(MEM_callocN(sizeof(StripElem), "stripelem"));
  strip->stripdata->orig_width = orig_width;
  strip->stripdata->orig_height = orig_height;
  strip->stripdata->orig_fps = video_fps;
  BLI_path_split_dir_file(
      load_data->path, strip->dirpath, sizeof(strip->dirpath), se->filename, sizeof(se->filename));

  seq_add_set_view_transform(scene, seq, load_data);
  seq_add_set_name(scene, seq, load_data);
  seq_add_generic_update(scene, seq);

  MEM_freeN(anim_arr);
  return seq;
}

void SEQ_add_reload_new_file(Main *bmain, Scene *scene, Sequence *seq, const bool lock_range)
{
  int prev_startdisp = 0, prev_enddisp = 0;
  /* NOTE: don't rename the strip, will break animation curves. */

  if (ELEM(seq->type,
           SEQ_TYPE_MOVIE,
           SEQ_TYPE_IMAGE,
           SEQ_TYPE_SOUND_RAM,
           SEQ_TYPE_SCENE,
           SEQ_TYPE_META,
           SEQ_TYPE_MOVIECLIP,
           SEQ_TYPE_MASK) == 0)
  {
    return;
  }

  if (lock_range) {
    /* keep so we don't have to move the actual start and end points (only the data) */
    prev_startdisp = SEQ_time_left_handle_frame_get(scene, seq);
    prev_enddisp = SEQ_time_right_handle_frame_get(scene, seq);
  }

  switch (seq->type) {
    case SEQ_TYPE_IMAGE: {
      /* Hack? */
      size_t olen = MEM_allocN_len(seq->strip->stripdata) / sizeof(StripElem);

      seq->len = olen;
      seq->len -= seq->anim_startofs;
      seq->len -= seq->anim_endofs;
      if (seq->len < 0) {
        seq->len = 0;
      }
      break;
    }
    case SEQ_TYPE_MOVIE: {
      char filepath[FILE_MAX];
      StripAnim *sanim;
      bool is_multiview_loaded = false;
      const bool is_multiview = (seq->flag & SEQ_USE_VIEWS) != 0 &&
                                (scene->r.scemode & R_MULTIVIEW) != 0;

      BLI_path_join(
          filepath, sizeof(filepath), seq->strip->dirpath, seq->strip->stripdata->filename);
      BLI_path_abs(filepath, BKE_main_blendfile_path_from_global());

      SEQ_relations_sequence_free_anim(seq);

      if (is_multiview && (seq->views_format == R_IMF_VIEWS_INDIVIDUAL)) {
        char prefix[FILE_MAX];
        const char *ext = nullptr;
        const int totfiles = seq_num_files(scene, seq->views_format, true);
        int i = 0;

        BKE_scene_multiview_view_prefix_get(scene, filepath, prefix, &ext);

        if (prefix[0] != '\0') {
          for (i = 0; i < totfiles; i++) {
            ImBufAnim *anim;
            char filepath_view[FILE_MAX];

            seq_multiview_name(scene, i, prefix, ext, filepath_view, sizeof(filepath_view));
            anim = openanim(filepath_view,
                            IB_rect | ((seq->flag & SEQ_FILTERY) ? IB_animdeinterlace : 0),
                            seq->streamindex,
                            seq->strip->colorspace_settings.name);

            if (anim) {
              seq_anim_add_suffix(scene, anim, i);
              sanim = static_cast<StripAnim *>(MEM_mallocN(sizeof(StripAnim), "Strip Anim"));
              BLI_addtail(&seq->anims, sanim);
              sanim->anim = anim;
            }
          }
          is_multiview_loaded = true;
        }
      }

      if (is_multiview_loaded == false) {
        ImBufAnim *anim;
        anim = openanim(filepath,
                        IB_rect | ((seq->flag & SEQ_FILTERY) ? IB_animdeinterlace : 0),
                        seq->streamindex,
                        seq->strip->colorspace_settings.name);
        if (anim) {
          sanim = static_cast<StripAnim *>(MEM_mallocN(sizeof(StripAnim), "Strip Anim"));
          BLI_addtail(&seq->anims, sanim);
          sanim->anim = anim;
        }
      }

      /* use the first video as reference for everything */
      sanim = static_cast<StripAnim *>(seq->anims.first);

      if ((!sanim) || (!sanim->anim)) {
        return;
      }

      IMB_anim_load_metadata(sanim->anim);

      seq->len = IMB_anim_get_duration(
          sanim->anim,
          IMB_Timecode_Type(seq->strip->proxy ? IMB_Timecode_Type(seq->strip->proxy->tc) :
                                                IMB_TC_RECORD_RUN));

      seq->len -= seq->anim_startofs;
      seq->len -= seq->anim_endofs;
      if (seq->len < 0) {
        seq->len = 0;
      }
      break;
    }
    case SEQ_TYPE_MOVIECLIP:
      if (seq->clip == nullptr) {
        return;
      }

      seq->len = BKE_movieclip_get_duration(seq->clip);

      seq->len -= seq->anim_startofs;
      seq->len -= seq->anim_endofs;
      if (seq->len < 0) {
        seq->len = 0;
      }
      break;
    case SEQ_TYPE_MASK:
      if (seq->mask == nullptr) {
        return;
      }
      seq->len = BKE_mask_get_duration(seq->mask);
      seq->len -= seq->anim_startofs;
      seq->len -= seq->anim_endofs;
      if (seq->len < 0) {
        seq->len = 0;
      }
      break;
    case SEQ_TYPE_SOUND_RAM:
#ifdef WITH_AUDASPACE
      if (!seq->sound) {
        return;
      }
      seq->len = ceil(double(BKE_sound_get_length(bmain, seq->sound)) * FPS);
      seq->len -= seq->anim_startofs;
      seq->len -= seq->anim_endofs;
      if (seq->len < 0) {
        seq->len = 0;
      }
#else
      UNUSED_VARS(bmain);
      return;
#endif
      break;
    case SEQ_TYPE_SCENE: {
      seq->len = (seq->scene) ? seq->scene->r.efra - seq->scene->r.sfra + 1 : 0;
      seq->len -= seq->anim_startofs;
      seq->len -= seq->anim_endofs;
      if (seq->len < 0) {
        seq->len = 0;
      }
      break;
    }
  }

  free_proxy_seq(seq);

  if (lock_range) {
    SEQ_time_left_handle_frame_set(scene, seq, prev_startdisp);
    SEQ_time_right_handle_frame_set(scene, seq, prev_enddisp);
  }

  SEQ_relations_invalidate_cache_raw(scene, seq);
}

void SEQ_add_movie_reload_if_needed(
    Main *bmain, Scene *scene, Sequence *seq, bool *r_was_reloaded, bool *r_can_produce_frames)
{
  BLI_assert(seq->type == SEQ_TYPE_MOVIE ||
             !"This function is only implemented for movie strips.");

  bool must_reload = false;

  /* The Sequence struct allows for multiple anim structs to be associated with one strip.
   * This function will return true only if there is at least one 'anim' AND all anims can
   * produce frames. */

  if (BLI_listbase_is_empty(&seq->anims)) {
    /* No anim present, so reloading is always necessary. */
    must_reload = true;
  }
  else {
    LISTBASE_FOREACH (StripAnim *, sanim, &seq->anims) {
      if (!IMB_anim_can_produce_frames(sanim->anim)) {
        /* Anim cannot produce frames, try reloading. */
        must_reload = true;
        break;
      }
    };
  }

  if (!must_reload) {
    /* There are one or more anims, and all can produce frames. */
    *r_was_reloaded = false;
    *r_can_produce_frames = true;
    return;
  }

  SEQ_add_reload_new_file(bmain, scene, seq, true);
  *r_was_reloaded = true;

  if (BLI_listbase_is_empty(&seq->anims)) {
    /* No anims present after reloading => no frames can be produced. */
    *r_can_produce_frames = false;
    return;
  }

  /* Check if there are still anims that cannot produce frames. */
  LISTBASE_FOREACH (StripAnim *, sanim, &seq->anims) {
    if (!IMB_anim_can_produce_frames(sanim->anim)) {
      /* There still is an anim that cannot produce frames. */
      *r_can_produce_frames = false;
      return;
    }
  };

  /* There are one or more anims, and all can produce frames. */
  *r_can_produce_frames = true;
}
