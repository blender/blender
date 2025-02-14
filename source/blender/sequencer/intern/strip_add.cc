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
#include "BLI_path_utils.hh"
#include "BLI_string.h"

#include "BKE_image.hh"
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

#include "MOV_read.hh"

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

static void strip_add_generic_update(Scene *scene, Strip *strip)
{
  SEQ_sequence_base_unique_name_recursive(scene, &scene->ed->seqbase, strip);
  SEQ_relations_invalidate_cache_composite(scene, strip);
  SEQ_strip_lookup_invalidate(scene);
  strip_time_effect_range_set(scene, strip);
  SEQ_time_update_meta_strip_range(scene, SEQ_lookup_meta_by_strip(scene, strip));
}

static void strip_add_set_name(Scene *scene, Strip *strip, SeqLoadData *load_data)
{
  if (load_data->name[0] != '\0') {
    SEQ_edit_sequence_name_set(scene, strip, load_data->name);
  }
  else {
    if (strip->type == STRIP_TYPE_SCENE) {
      SEQ_edit_sequence_name_set(scene, strip, load_data->scene->id.name + 2);
    }
    else if (strip->type == STRIP_TYPE_MOVIECLIP) {
      SEQ_edit_sequence_name_set(scene, strip, load_data->clip->id.name + 2);
    }
    else if (strip->type == STRIP_TYPE_MASK) {
      SEQ_edit_sequence_name_set(scene, strip, load_data->mask->id.name + 2);
    }
    else if ((strip->type & STRIP_TYPE_EFFECT) != 0) {
      SEQ_edit_sequence_name_set(scene, strip, SEQ_sequence_give_name(strip));
    }
    else { /* Image, sound and movie. */
      SEQ_edit_sequence_name_set(scene, strip, load_data->name);
    }
  }
}

static void strip_add_set_view_transform(Scene *scene, Strip *strip, SeqLoadData *load_data)
{
  const char *strip_colorspace = strip->data->colorspace_settings.name;

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

Strip *SEQ_add_scene_strip(Scene *scene, ListBase *seqbase, SeqLoadData *load_data)
{
  Strip *strip = SEQ_sequence_alloc(
      seqbase, load_data->start_frame, load_data->channel, STRIP_TYPE_SCENE);
  strip->scene = load_data->scene;
  strip->len = load_data->scene->r.efra - load_data->scene->r.sfra + 1;
  id_us_ensure_real((ID *)load_data->scene);
  strip_add_set_name(scene, strip, load_data);
  strip_add_generic_update(scene, strip);
  return strip;
}

Strip *SEQ_add_movieclip_strip(Scene *scene, ListBase *seqbase, SeqLoadData *load_data)
{
  Strip *strip = SEQ_sequence_alloc(
      seqbase, load_data->start_frame, load_data->channel, STRIP_TYPE_MOVIECLIP);
  strip->clip = load_data->clip;
  strip->len = BKE_movieclip_get_duration(load_data->clip);
  id_us_ensure_real((ID *)load_data->clip);
  strip_add_set_name(scene, strip, load_data);
  strip_add_generic_update(scene, strip);
  return strip;
}

Strip *SEQ_add_mask_strip(Scene *scene, ListBase *seqbase, SeqLoadData *load_data)
{
  Strip *strip = SEQ_sequence_alloc(
      seqbase, load_data->start_frame, load_data->channel, STRIP_TYPE_MASK);
  strip->mask = load_data->mask;
  strip->len = BKE_mask_get_duration(load_data->mask);
  id_us_ensure_real((ID *)load_data->mask);
  strip_add_set_name(scene, strip, load_data);
  strip_add_generic_update(scene, strip);
  return strip;
}

Strip *SEQ_add_effect_strip(Scene *scene, ListBase *seqbase, SeqLoadData *load_data)
{
  Strip *strip = SEQ_sequence_alloc(
      seqbase, load_data->start_frame, load_data->channel, load_data->effect.type);

  strip->flag |= SEQ_USE_EFFECT_DEFAULT_FADE;
  SeqEffectHandle sh = SEQ_effect_handle_get(strip);
  sh.init(strip);
  strip->seq1 = load_data->effect.seq1;
  strip->seq2 = load_data->effect.seq2;

  if (SEQ_effect_get_num_inputs(strip->type) == 1) {
    strip->blend_mode = strip->seq1->blend_mode;
    strip->blend_opacity = strip->seq1->blend_opacity;
  }

  if (!load_data->effect.seq1) {
    strip->len = 1; /* Effect is generator, set non zero length. */
    strip->flag |= SEQ_SINGLE_FRAME_CONTENT;
    SEQ_time_right_handle_frame_set(scene, strip, load_data->effect.end_frame);
  }

  strip_add_set_name(scene, strip, load_data);
  strip_add_generic_update(scene, strip);

  return strip;
}

void SEQ_add_image_set_directory(Strip *strip, const char *dirpath)
{
  STRNCPY(strip->data->dirpath, dirpath);
}

void SEQ_add_image_load_file(Scene *scene, Strip *strip, size_t strip_frame, const char *filename)
{
  StripElem *se = SEQ_render_give_stripelem(
      scene, strip, SEQ_time_start_frame_get(strip) + strip_frame);
  STRNCPY(se->filename, filename);
}

void SEQ_add_image_init_alpha_mode(Strip *strip)
{
  if (strip->data && strip->data->stripdata) {
    char filepath[FILE_MAX];
    ImBuf *ibuf;

    BLI_path_join(
        filepath, sizeof(filepath), strip->data->dirpath, strip->data->stripdata->filename);
    BLI_path_abs(filepath, BKE_main_blendfile_path_from_global());

    /* Initialize input color space. */
    if (strip->type == STRIP_TYPE_IMAGE) {
      ibuf = IMB_loadiffname(filepath,
                             IB_test | IB_multilayer | IB_alphamode_detect,
                             strip->data->colorspace_settings.name);

      /* Byte images are default to straight alpha, however sequencer
       * works in premul space, so mark strip to be premultiplied first.
       */
      strip->alpha_mode = SEQ_ALPHA_STRAIGHT;
      if (ibuf) {
        if (ibuf->flags & IB_alphamode_premul) {
          strip->alpha_mode = IMA_ALPHA_PREMUL;
        }

        IMB_freeImBuf(ibuf);
      }
    }
  }
}

Strip *SEQ_add_image_strip(Main *bmain, Scene *scene, ListBase *seqbase, SeqLoadData *load_data)
{
  Strip *strip = SEQ_sequence_alloc(
      seqbase, load_data->start_frame, load_data->channel, STRIP_TYPE_IMAGE);
  strip->len = load_data->image.len;
  StripData *data = strip->data;
  data->stripdata = static_cast<StripElem *>(
      MEM_callocN(load_data->image.len * sizeof(StripElem), "stripelem"));

  if (strip->len == 1) {
    strip->flag |= SEQ_SINGLE_FRAME_CONTENT;
  }

  /* Multiview settings. */
  if (load_data->use_multiview) {
    strip->flag |= SEQ_USE_VIEWS;
    strip->views_format = load_data->views_format;
  }
  if (load_data->stereo3d_format) {
    strip->stereo3d_format = load_data->stereo3d_format;
  }

  /* Set initial scale based on load_data->fit_method. */
  char file_path[FILE_MAX];
  STRNCPY(file_path, load_data->path);
  BLI_path_abs(file_path, BKE_main_blendfile_path(bmain));
  ImBuf *ibuf = IMB_loadiffname(
      file_path, IB_rect | IB_multilayer, strip->data->colorspace_settings.name);
  if (ibuf != nullptr) {
    /* Set image resolution. Assume that all images in sequence are same size. This fields are only
     * informative. */
    StripElem *strip_elem = data->stripdata;
    for (int i = 0; i < load_data->image.len; i++) {
      strip_elem->orig_width = ibuf->x;
      strip_elem->orig_height = ibuf->y;
      strip_elem++;
    }

    SEQ_set_scale_to_fit(
        strip, ibuf->x, ibuf->y, scene->r.xsch, scene->r.ysch, load_data->fit_method);
    IMB_freeImBuf(ibuf);
  }

  /* Set Last active directory. */
  STRNCPY(scene->ed->act_imagedir, strip->data->dirpath);
  strip_add_set_view_transform(scene, strip, load_data);
  strip_add_set_name(scene, strip, load_data);
  strip_add_generic_update(scene, strip);

  return strip;
}

#ifdef WITH_AUDASPACE

void SEQ_add_sound_av_sync(Main *bmain, Scene *scene, Strip *strip, SeqLoadData *load_data)
{
  SoundStreamInfo sound_stream;
  if (!BKE_sound_stream_info_get(bmain, load_data->path, 0, &sound_stream)) {
    return;
  }

  const double av_stream_offset = sound_stream.start - load_data->r_video_stream_start;
  const int frame_offset = av_stream_offset * FPS;
  /* Set sub-frame offset. */
  strip->sound->offset_time = (double(frame_offset) / FPS) - av_stream_offset;
  SEQ_transform_translate_sequence(scene, strip, frame_offset);
}

Strip *SEQ_add_sound_strip(Main *bmain, Scene *scene, ListBase *seqbase, SeqLoadData *load_data)
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

  Strip *strip = SEQ_sequence_alloc(
      seqbase, load_data->start_frame, load_data->channel, STRIP_TYPE_SOUND_RAM);
  strip->sound = sound;
  strip->scene_sound = nullptr;

  /* We round the frame duration as the audio sample lengths usually does not
   * line up with the video frames. Therefore we round this number to the
   * nearest frame as the audio track usually overshoots or undershoots the
   * end frame of the video by a little bit.
   * See #47135 for under shoot example. */
  strip->len = std::max(1, int(round((info.length - sound->offset_time) * FPS)));

  StripData *data = strip->data;
  /* We only need 1 element to store the filename. */
  StripElem *se = data->stripdata = static_cast<StripElem *>(
      MEM_callocN(sizeof(StripElem), "stripelem"));
  BLI_path_split_dir_file(
      load_data->path, data->dirpath, sizeof(data->dirpath), se->filename, sizeof(se->filename));

  if (strip->sound != nullptr) {
    if (load_data->flags & SEQ_LOAD_SOUND_MONO) {
      strip->sound->flags |= SOUND_FLAGS_MONO;
    }

    if (load_data->flags & SEQ_LOAD_SOUND_CACHE) {
      if (strip->sound) {
        strip->sound->flags |= SOUND_FLAGS_CACHING;
      }
    }

    /* Turn on Display Waveform by default. */
    strip->flag |= SEQ_AUDIO_DRAW_WAVEFORM;
  }

  /* Set Last active directory. */
  BLI_strncpy(scene->ed->act_sounddir, data->dirpath, FILE_MAXDIR);
  strip_add_set_name(scene, strip, load_data);
  strip_add_generic_update(scene, strip);

  return strip;
}

#else   // WITH_AUDASPACE

void SEQ_add_sound_av_sync(Main * /*bmain*/,
                           Scene * /*scene*/,
                           Strip * /*strip*/,
                           SeqLoadData * /*load_data*/)
{
}

Strip *SEQ_add_sound_strip(Main * /*bmain*/,
                           Scene * /*scene*/,
                           ListBase * /*seqbase*/,
                           SeqLoadData * /*load_data*/)
{
  return nullptr;
}
#endif  // WITH_AUDASPACE

Strip *SEQ_add_meta_strip(Scene *scene, ListBase *seqbase, SeqLoadData *load_data)
{
  /* Allocate sequence. */
  Strip *seqm = SEQ_sequence_alloc(
      seqbase, load_data->start_frame, load_data->channel, STRIP_TYPE_META);

  /* Set name. */
  strip_add_set_name(scene, seqm, load_data);

  /* Set frames start and length. */
  seqm->start = load_data->start_frame;
  seqm->len = 1;

  strip_add_generic_update(scene, seqm);

  return seqm;
}

Strip *SEQ_add_movie_strip(Main *bmain, Scene *scene, ListBase *seqbase, SeqLoadData *load_data)
{
  char filepath[sizeof(load_data->path)];
  STRNCPY(filepath, load_data->path);
  BLI_path_abs(filepath, BKE_main_blendfile_path(bmain));

  char colorspace[64] = "\0"; /* MAX_COLORSPACE_NAME */
  bool is_multiview_loaded = false;
  const int totfiles = seq_num_files(scene, load_data->views_format, load_data->use_multiview);
  MovieReader **anim_arr = static_cast<MovieReader **>(
      MEM_callocN(sizeof(MovieReader *) * totfiles, "Video files"));
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
    short fps_num;
    float fps_denom;
    bool have_fps = MOV_get_fps_num_denom(anim_arr[0], fps_num, fps_denom);
    if (have_fps) {
      video_fps = fps_num / fps_denom;
    }

    /* Adjust scene's frame rate settings to match. */
    if (have_fps && (load_data->flags & SEQ_LOAD_MOVIE_SYNC_FPS)) {
      scene->r.frs_sec = fps_num;
      scene->r.frs_sec_base = fps_denom;
      DEG_id_tag_update(&scene->id, ID_RECALC_AUDIO_FPS | ID_RECALC_SEQUENCER_STRIPS);
    }

    load_data->r_video_stream_start = MOV_get_start_offset_seconds(anim_arr[0]);
  }

  Strip *strip = SEQ_sequence_alloc(
      seqbase, load_data->start_frame, load_data->channel, STRIP_TYPE_MOVIE);

  /* Multiview settings. */
  if (load_data->use_multiview) {
    strip->flag |= SEQ_USE_VIEWS;
    strip->views_format = load_data->views_format;
  }
  if (load_data->stereo3d_format) {
    strip->stereo3d_format = load_data->stereo3d_format;
  }

  for (i = 0; i < totfiles; i++) {
    if (anim_arr[i]) {
      StripAnim *sanim = static_cast<StripAnim *>(MEM_mallocN(sizeof(StripAnim), "Strip Anim"));
      BLI_addtail(&strip->anims, sanim);
      sanim->anim = anim_arr[i];
    }
    else {
      break;
    }
  }

  if (anim_arr[0] != nullptr) {
    strip->len = MOV_get_duration_frames(anim_arr[0], IMB_TC_RECORD_RUN);

    MOV_load_metadata(anim_arr[0]);

    /* Set initial scale based on load_data->fit_method. */
    orig_width = MOV_get_image_width(anim_arr[0]);
    orig_height = MOV_get_image_height(anim_arr[0]);
    SEQ_set_scale_to_fit(
        strip, orig_width, orig_height, scene->r.xsch, scene->r.ysch, load_data->fit_method);

    float fps = MOV_get_fps(anim_arr[0]);
    if (fps > 0.0f) {
      strip->media_playback_rate = fps;
    }
  }

  strip->len = std::max(1, strip->len);
  if (load_data->adjust_playback_rate) {
    strip->flag |= SEQ_AUTO_PLAYBACK_RATE;
  }

  STRNCPY(strip->data->colorspace_settings.name, colorspace);

  StripData *data = strip->data;
  /* We only need 1 element for MOVIE strips. */
  StripElem *se;
  data->stripdata = se = static_cast<StripElem *>(MEM_callocN(sizeof(StripElem), "stripelem"));
  data->stripdata->orig_width = orig_width;
  data->stripdata->orig_height = orig_height;
  data->stripdata->orig_fps = video_fps;
  BLI_path_split_dir_file(
      load_data->path, data->dirpath, sizeof(data->dirpath), se->filename, sizeof(se->filename));

  strip_add_set_view_transform(scene, strip, load_data);
  strip_add_set_name(scene, strip, load_data);
  strip_add_generic_update(scene, strip);

  MEM_freeN(anim_arr);
  return strip;
}

void SEQ_add_reload_new_file(Main *bmain, Scene *scene, Strip *strip, const bool lock_range)
{
  int prev_startdisp = 0, prev_enddisp = 0;
  /* NOTE: don't rename the strip, will break animation curves. */

  if (ELEM(strip->type,
           STRIP_TYPE_MOVIE,
           STRIP_TYPE_IMAGE,
           STRIP_TYPE_SOUND_RAM,
           STRIP_TYPE_SCENE,
           STRIP_TYPE_META,
           STRIP_TYPE_MOVIECLIP,
           STRIP_TYPE_MASK) == 0)
  {
    return;
  }

  if (lock_range) {
    /* keep so we don't have to move the actual start and end points (only the data) */
    prev_startdisp = SEQ_time_left_handle_frame_get(scene, strip);
    prev_enddisp = SEQ_time_right_handle_frame_get(scene, strip);
  }

  switch (strip->type) {
    case STRIP_TYPE_IMAGE: {
      /* Hack? */
      size_t olen = MEM_allocN_len(strip->data->stripdata) / sizeof(StripElem);

      strip->len = olen;
      strip->len -= strip->anim_startofs;
      strip->len -= strip->anim_endofs;
      strip->len = std::max(strip->len, 0);
      break;
    }
    case STRIP_TYPE_MOVIE: {
      char filepath[FILE_MAX];
      StripAnim *sanim;
      bool is_multiview_loaded = false;
      const bool is_multiview = (strip->flag & SEQ_USE_VIEWS) != 0 &&
                                (scene->r.scemode & R_MULTIVIEW) != 0;

      BLI_path_join(
          filepath, sizeof(filepath), strip->data->dirpath, strip->data->stripdata->filename);
      BLI_path_abs(filepath, BKE_main_blendfile_path_from_global());

      SEQ_relations_sequence_free_anim(strip);

      if (is_multiview && (strip->views_format == R_IMF_VIEWS_INDIVIDUAL)) {
        char prefix[FILE_MAX];
        const char *ext = nullptr;
        const int totfiles = seq_num_files(scene, strip->views_format, true);
        int i = 0;

        BKE_scene_multiview_view_prefix_get(scene, filepath, prefix, &ext);

        if (prefix[0] != '\0') {
          for (i = 0; i < totfiles; i++) {
            MovieReader *anim;
            char filepath_view[FILE_MAX];

            seq_multiview_name(scene, i, prefix, ext, filepath_view, sizeof(filepath_view));
            anim = openanim(filepath_view,
                            IB_rect | ((strip->flag & SEQ_FILTERY) ? IB_animdeinterlace : 0),
                            strip->streamindex,
                            strip->data->colorspace_settings.name);

            if (anim) {
              seq_anim_add_suffix(scene, anim, i);
              sanim = static_cast<StripAnim *>(MEM_mallocN(sizeof(StripAnim), "Strip Anim"));
              BLI_addtail(&strip->anims, sanim);
              sanim->anim = anim;
            }
          }
          is_multiview_loaded = true;
        }
      }

      if (is_multiview_loaded == false) {
        MovieReader *anim;
        anim = openanim(filepath,
                        IB_rect | ((strip->flag & SEQ_FILTERY) ? IB_animdeinterlace : 0),
                        strip->streamindex,
                        strip->data->colorspace_settings.name);
        if (anim) {
          sanim = static_cast<StripAnim *>(MEM_mallocN(sizeof(StripAnim), "Strip Anim"));
          BLI_addtail(&strip->anims, sanim);
          sanim->anim = anim;
        }
      }

      /* use the first video as reference for everything */
      sanim = static_cast<StripAnim *>(strip->anims.first);

      if ((!sanim) || (!sanim->anim)) {
        return;
      }

      MOV_load_metadata(sanim->anim);

      strip->len = MOV_get_duration_frames(
          sanim->anim,
          IMB_Timecode_Type(strip->data->proxy ? IMB_Timecode_Type(strip->data->proxy->tc) :
                                                 IMB_TC_RECORD_RUN));

      strip->len -= strip->anim_startofs;
      strip->len -= strip->anim_endofs;
      strip->len = std::max(strip->len, 0);
      break;
    }
    case STRIP_TYPE_MOVIECLIP:
      if (strip->clip == nullptr) {
        return;
      }

      strip->len = BKE_movieclip_get_duration(strip->clip);

      strip->len -= strip->anim_startofs;
      strip->len -= strip->anim_endofs;
      strip->len = std::max(strip->len, 0);
      break;
    case STRIP_TYPE_MASK:
      if (strip->mask == nullptr) {
        return;
      }
      strip->len = BKE_mask_get_duration(strip->mask);
      strip->len -= strip->anim_startofs;
      strip->len -= strip->anim_endofs;
      strip->len = std::max(strip->len, 0);
      break;
    case STRIP_TYPE_SOUND_RAM:
#ifdef WITH_AUDASPACE
      if (!strip->sound) {
        return;
      }
      strip->len = ceil(double(BKE_sound_get_length(bmain, strip->sound)) * FPS);
      strip->len -= strip->anim_startofs;
      strip->len -= strip->anim_endofs;
      strip->len = std::max(strip->len, 0);
#else
      UNUSED_VARS(bmain);
      return;
#endif
      break;
    case STRIP_TYPE_SCENE: {
      strip->len = (strip->scene) ? strip->scene->r.efra - strip->scene->r.sfra + 1 : 0;
      strip->len -= strip->anim_startofs;
      strip->len -= strip->anim_endofs;
      strip->len = std::max(strip->len, 0);
      break;
    }
  }

  free_proxy_seq(strip);

  if (lock_range) {
    SEQ_time_left_handle_frame_set(scene, strip, prev_startdisp);
    SEQ_time_right_handle_frame_set(scene, strip, prev_enddisp);
  }

  SEQ_relations_invalidate_cache_raw(scene, strip);
}

void SEQ_add_movie_reload_if_needed(
    Main *bmain, Scene *scene, Strip *strip, bool *r_was_reloaded, bool *r_can_produce_frames)
{
  BLI_assert_msg(strip->type == STRIP_TYPE_MOVIE,
                 "This function is only implemented for movie strips.");

  bool must_reload = false;

  /* The Sequence struct allows for multiple anim structs to be associated with one strip.
   * This function will return true only if there is at least one 'anim' AND all anims can
   * produce frames. */

  if (BLI_listbase_is_empty(&strip->anims)) {
    /* No anim present, so reloading is always necessary. */
    must_reload = true;
  }
  else {
    LISTBASE_FOREACH (StripAnim *, sanim, &strip->anims) {
      if (!MOV_is_initialized_and_valid(sanim->anim)) {
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

  SEQ_add_reload_new_file(bmain, scene, strip, true);
  *r_was_reloaded = true;

  if (BLI_listbase_is_empty(&strip->anims)) {
    /* No anims present after reloading => no frames can be produced. */
    *r_can_produce_frames = false;
    return;
  }

  /* Check if there are still anims that cannot produce frames. */
  LISTBASE_FOREACH (StripAnim *, sanim, &strip->anims) {
    if (!MOV_is_initialized_and_valid(sanim->anim)) {
      /* There still is an anim that cannot produce frames. */
      *r_can_produce_frames = false;
      return;
    }
  };

  /* There are one or more anims, and all can produce frames. */
  *r_can_produce_frames = true;
}
