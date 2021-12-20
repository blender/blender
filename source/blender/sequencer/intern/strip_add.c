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
 *
 * The Original Code is Copyright (C) 2001-2002 by NaN Holding BV.
 * All rights reserved.
 *
 * - Blender Foundation, 2003-2009
 * - Peter Schlaile <peter [at] schlaile [dot] de> 2005/2006
 */

/** \file
 * \ingroup bke
 */

#include <math.h>
#include <string.h>

#include "MEM_guardedalloc.h"

#include "DNA_mask_types.h"
#include "DNA_scene_types.h"
#include "DNA_sequence_types.h"
#include "DNA_sound_types.h"

#include "BLI_listbase.h"
#include "BLI_path_util.h"
#include "BLI_string.h"
#include "BLI_string_utf8.h"

#include "BKE_context.h"
#include "BKE_image.h"
#include "BKE_lib_id.h"
#include "BKE_main.h"
#include "BKE_mask.h"
#include "BKE_movieclip.h"
#include "BKE_scene.h"
#include "BKE_sound.h"

#include "DEG_depsgraph_query.h"

#include "IMB_colormanagement.h"
#include "IMB_imbuf.h"
#include "IMB_imbuf_types.h"
#include "IMB_metadata.h"

#include "SEQ_add.h"
#include "SEQ_edit.h"
#include "SEQ_effects.h"
#include "SEQ_relations.h"
#include "SEQ_render.h"
#include "SEQ_select.h"
#include "SEQ_sequencer.h"
#include "SEQ_time.h"
#include "SEQ_transform.h"
#include "SEQ_utils.h"

#include "multiview.h"
#include "proxy.h"
#include "utils.h"

void SEQ_add_load_data_init(SeqLoadData *load_data,
                            const char *name,
                            const char *path,
                            const int start_frame,
                            const int channel)
{
  memset(load_data, 0, sizeof(SeqLoadData));
  if (name != NULL) {
    BLI_strncpy(load_data->name, name, sizeof(load_data->name));
  }
  if (path != NULL) {
    BLI_strncpy(load_data->path, path, sizeof(load_data->path));
  }
  load_data->start_frame = start_frame;
  load_data->channel = channel;
}

static void seq_add_generic_update(Scene *scene, ListBase *seqbase, Sequence *seq)
{
  SEQ_sequence_base_unique_name_recursive(scene, &scene->ed->seqbase, seq);
  SEQ_time_update_sequence(scene, seqbase, seq);
  SEQ_sort(seqbase);
  SEQ_relations_invalidate_cache_composite(scene, seq);
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
      struct ColorManagedDisplay *display = IMB_colormanagement_display_get_named(
          scene->display_settings.display_device);
      const char *default_view_transform =
          IMB_colormanagement_display_get_default_view_transform_name(display);
      STRNCPY(scene->view_settings.view_transform, default_view_transform);
    }
  }
}

Sequence *SEQ_add_scene_strip(Scene *scene, ListBase *seqbase, struct SeqLoadData *load_data)
{
  Sequence *seq = SEQ_sequence_alloc(
      seqbase, load_data->start_frame, load_data->channel, SEQ_TYPE_SCENE);
  seq->scene = load_data->scene;
  seq->len = load_data->scene->r.efra - load_data->scene->r.sfra + 1;
  id_us_ensure_real((ID *)load_data->scene);
  seq_add_set_name(scene, seq, load_data);
  seq_add_generic_update(scene, seqbase, seq);
  return seq;
}

Sequence *SEQ_add_movieclip_strip(Scene *scene, ListBase *seqbase, struct SeqLoadData *load_data)
{
  Sequence *seq = SEQ_sequence_alloc(
      seqbase, load_data->start_frame, load_data->channel, SEQ_TYPE_MOVIECLIP);
  seq->clip = load_data->clip;
  seq->len = BKE_movieclip_get_duration(load_data->clip);
  id_us_ensure_real((ID *)load_data->clip);
  seq_add_set_name(scene, seq, load_data);
  seq_add_generic_update(scene, seqbase, seq);
  return seq;
}

Sequence *SEQ_add_mask_strip(Scene *scene, ListBase *seqbase, struct SeqLoadData *load_data)
{
  Sequence *seq = SEQ_sequence_alloc(
      seqbase, load_data->start_frame, load_data->channel, SEQ_TYPE_MASK);
  seq->mask = load_data->mask;
  seq->len = BKE_mask_get_duration(load_data->mask);
  id_us_ensure_real((ID *)load_data->mask);
  seq_add_set_name(scene, seq, load_data);
  seq_add_generic_update(scene, seqbase, seq);
  return seq;
}

Sequence *SEQ_add_effect_strip(Scene *scene, ListBase *seqbase, struct SeqLoadData *load_data)
{
  Sequence *seq = SEQ_sequence_alloc(
      seqbase, load_data->start_frame, load_data->channel, load_data->effect.type);

  seq->flag |= SEQ_USE_EFFECT_DEFAULT_FADE;
  struct SeqEffectHandle sh = SEQ_effect_handle_get(seq);
  sh.init(seq);
  seq->seq1 = load_data->effect.seq1;
  seq->seq2 = load_data->effect.seq2;
  seq->seq3 = load_data->effect.seq3;

  if (SEQ_effect_get_num_inputs(seq->type) == 1) {
    seq->blend_mode = seq->seq1->blend_mode;
  }

  if (!load_data->effect.seq1) {
    seq->len = 1; /* Effect is generator, set non zero length. */
    SEQ_transform_set_right_handle_frame(seq, load_data->effect.end_frame);
  }

  seq_add_set_name(scene, seq, load_data);
  seq_add_generic_update(scene, seqbase, seq);

  return seq;
}

void SEQ_add_image_set_directory(Sequence *seq, char *path)
{
  BLI_strncpy(seq->strip->dir, path, sizeof(seq->strip->dir));
}

void SEQ_add_image_load_file(Sequence *seq, size_t strip_frame, char *filename)
{
  StripElem *se = SEQ_render_give_stripelem(seq, seq->start + strip_frame);
  BLI_strncpy(se->name, filename, sizeof(se->name));
}

void SEQ_add_image_init_alpha_mode(Sequence *seq)
{
  if (seq->strip && seq->strip->stripdata) {
    char name[FILE_MAX];
    ImBuf *ibuf;

    BLI_join_dirfile(name, sizeof(name), seq->strip->dir, seq->strip->stripdata->name);
    BLI_path_abs(name, BKE_main_blendfile_path_from_global());

    /* Initialize input color space. */
    if (seq->type == SEQ_TYPE_IMAGE) {
      ibuf = IMB_loadiffname(
          name, IB_test | IB_alphamode_detect, seq->strip->colorspace_settings.name);

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
  strip->stripdata = MEM_callocN(load_data->image.len * sizeof(StripElem), "stripelem");

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
  BLI_strncpy(file_path, load_data->path, sizeof(file_path));
  BLI_path_abs(file_path, BKE_main_blendfile_path(bmain));
  ImBuf *ibuf = IMB_loadiffname(file_path, IB_rect, seq->strip->colorspace_settings.name);
  if (ibuf != NULL) {
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
  BLI_strncpy(scene->ed->act_imagedir, seq->strip->dir, sizeof(scene->ed->act_imagedir));
  seq_add_set_view_transform(scene, seq, load_data);
  seq_add_set_name(scene, seq, load_data);
  seq_add_generic_update(scene, seqbase, seq);

  return seq;
}

#ifdef WITH_AUDASPACE
Sequence *SEQ_add_sound_strip(Main *bmain,
                              Scene *scene,
                              ListBase *seqbase,
                              SeqLoadData *load_data,
                              const double audio_offset)
{
  bSound *sound = BKE_sound_new_file(bmain, load_data->path); /* Handles relative paths. */
  sound->offset_time = audio_offset;
  SoundInfo info;
  bool sound_loaded = BKE_sound_info_get(bmain, sound, &info);

  if (!sound_loaded && !load_data->allow_invalid_file) {
    BKE_id_free(bmain, sound);
    return NULL;
  }

  if (info.specs.channels == SOUND_CHANNELS_INVALID && !load_data->allow_invalid_file) {
    BKE_id_free(bmain, sound);
    return NULL;
  }

  Sequence *seq = SEQ_sequence_alloc(
      seqbase, load_data->start_frame, load_data->channel, SEQ_TYPE_SOUND_RAM);
  seq->sound = sound;
  seq->scene_sound = NULL;

  /* We round the frame duration as the audio sample lengths usually does not
   * line up with the video frames. Therefore we round this number to the
   * nearest frame as the audio track usually overshoots or undershoots the
   * end frame of the video by a little bit.
   * See T47135 for under shoot example. */
  seq->len = MAX2(1, round((info.length - sound->offset_time) * FPS));

  Strip *strip = seq->strip;
  /* We only need 1 element to store the filename. */
  StripElem *se = strip->stripdata = MEM_callocN(sizeof(StripElem), "stripelem");
  BLI_split_dirfile(load_data->path, strip->dir, se->name, sizeof(strip->dir), sizeof(se->name));

  if (seq != NULL && seq->sound != NULL) {
    if (load_data->flags & SEQ_LOAD_SOUND_MONO) {
      seq->sound->flags |= SOUND_FLAGS_MONO;
    }

    if (load_data->flags & SEQ_LOAD_SOUND_CACHE) {
      if (seq->sound) {
        seq->sound->flags |= SOUND_FLAGS_CACHING;
      }
    }
  }

  /* Set Last active directory. */
  BLI_strncpy(scene->ed->act_sounddir, strip->dir, FILE_MAXDIR);
  seq_add_set_name(scene, seq, load_data);
  seq_add_generic_update(scene, seqbase, seq);

  return seq;
}

#else   // WITH_AUDASPACE
Sequence *SEQ_add_sound_strip(Main *UNUSED(bmain),
                              Scene *UNUSED(scene),
                              ListBase *UNUSED(seqbase),
                              SeqLoadData *UNUSED(load_data),
                              const double UNUSED(audio_offset))
{
  return NULL;
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
  SEQ_time_update_sequence(scene, seqbase, seqm);

  return seqm;
}

Sequence *SEQ_add_movie_strip(
    Main *bmain, Scene *scene, ListBase *seqbase, SeqLoadData *load_data, double *r_start_offset)
{
  char path[sizeof(load_data->path)];
  BLI_strncpy(path, load_data->path, sizeof(path));
  BLI_path_abs(path, BKE_main_blendfile_path(bmain));

  char colorspace[64] = "\0"; /* MAX_COLORSPACE_NAME */
  bool is_multiview_loaded = false;
  const int totfiles = seq_num_files(scene, load_data->views_format, load_data->use_multiview);
  struct anim **anim_arr = MEM_callocN(sizeof(struct anim *) * totfiles, "Video files");
  int i;
  int orig_width = 0;
  int orig_height = 0;

  if (load_data->use_multiview && (load_data->views_format == R_IMF_VIEWS_INDIVIDUAL)) {
    char prefix[FILE_MAX];
    const char *ext = NULL;
    size_t j = 0;

    BKE_scene_multiview_view_prefix_get(scene, path, prefix, &ext);

    if (prefix[0] != '\0') {
      for (i = 0; i < totfiles; i++) {
        char str[FILE_MAX];

        seq_multiview_name(scene, i, prefix, ext, str, FILE_MAX);
        anim_arr[j] = openanim(str, IB_rect, 0, colorspace);

        if (anim_arr[j]) {
          seq_anim_add_suffix(scene, anim_arr[j], i);
          j++;
        }
      }
      is_multiview_loaded = true;
    }
  }

  if (is_multiview_loaded == false) {
    anim_arr[0] = openanim(path, IB_rect, 0, colorspace);
  }

  if (anim_arr[0] == NULL && !load_data->allow_invalid_file) {
    MEM_freeN(anim_arr);
    return NULL;
  }

  int video_frame_offset = 0;
  float video_fps = 0.0f;

  if (anim_arr[0] != NULL) {
    short fps_denom;
    float fps_num;

    IMB_anim_get_fps(anim_arr[0], &fps_denom, &fps_num, true);

    video_fps = fps_denom / fps_num;

    /* Adjust scene's frame rate settings to match. */
    if (load_data->flags & SEQ_LOAD_MOVIE_SYNC_FPS) {
      scene->r.frs_sec = fps_denom;
      scene->r.frs_sec_base = fps_num;
    }

    double video_start_offset = IMD_anim_get_offset(anim_arr[0]);
    int minimum_frame_offset;

    if (*r_start_offset >= 0) {
      minimum_frame_offset = MIN2(video_start_offset, *r_start_offset) * FPS;
    }
    else {
      minimum_frame_offset = video_start_offset * FPS;
    }

    video_frame_offset = video_start_offset * FPS - minimum_frame_offset;

    *r_start_offset = video_start_offset;
  }

  Sequence *seq = SEQ_sequence_alloc(
      seqbase, load_data->start_frame + video_frame_offset, load_data->channel, SEQ_TYPE_MOVIE);

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
      StripAnim *sanim = MEM_mallocN(sizeof(StripAnim), "Strip Anim");
      BLI_addtail(&seq->anims, sanim);
      sanim->anim = anim_arr[i];
    }
    else {
      break;
    }
  }

  if (anim_arr[0] != NULL) {
    seq->len = IMB_anim_get_duration(anim_arr[0], IMB_TC_RECORD_RUN);

    IMB_anim_load_metadata(anim_arr[0]);

    /* Set initial scale based on load_data->fit_method. */
    orig_width = IMB_anim_get_image_width(anim_arr[0]);
    orig_height = IMB_anim_get_image_height(anim_arr[0]);
    SEQ_set_scale_to_fit(
        seq, orig_width, orig_height, scene->r.xsch, scene->r.ysch, load_data->fit_method);
  }

  seq->len = MAX2(1, seq->len);
  BLI_strncpy(seq->strip->colorspace_settings.name,
              colorspace,
              sizeof(seq->strip->colorspace_settings.name));

  Strip *strip = seq->strip;
  /* We only need 1 element for MOVIE strips. */
  StripElem *se;
  strip->stripdata = se = MEM_callocN(sizeof(StripElem), "stripelem");
  strip->stripdata->orig_width = orig_width;
  strip->stripdata->orig_height = orig_height;
  strip->stripdata->orig_fps = video_fps;
  BLI_split_dirfile(load_data->path, strip->dir, se->name, sizeof(strip->dir), sizeof(se->name));

  seq_add_set_view_transform(scene, seq, load_data);
  seq_add_set_name(scene, seq, load_data);
  seq_add_generic_update(scene, seqbase, seq);

  MEM_freeN(anim_arr);
  return seq;
}

void SEQ_add_reload_new_file(Main *bmain, Scene *scene, Sequence *seq, const bool lock_range)
{
  char path[FILE_MAX];
  int prev_startdisp = 0, prev_enddisp = 0;
  /* NOTE: don't rename the strip, will break animation curves. */

  if (ELEM(seq->type,
           SEQ_TYPE_MOVIE,
           SEQ_TYPE_IMAGE,
           SEQ_TYPE_SOUND_RAM,
           SEQ_TYPE_SCENE,
           SEQ_TYPE_META,
           SEQ_TYPE_MOVIECLIP,
           SEQ_TYPE_MASK) == 0) {
    return;
  }

  if (lock_range) {
    /* keep so we don't have to move the actual start and end points (only the data) */
    Editing *ed = SEQ_editing_get(scene);
    ListBase *seqbase = SEQ_get_seqbase_by_seq(&ed->seqbase, seq);
    SEQ_time_update_sequence(scene, seqbase, seq);
    prev_startdisp = seq->startdisp;
    prev_enddisp = seq->enddisp;
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
      StripAnim *sanim;
      bool is_multiview_loaded = false;
      const bool is_multiview = (seq->flag & SEQ_USE_VIEWS) != 0 &&
                                (scene->r.scemode & R_MULTIVIEW) != 0;

      BLI_join_dirfile(path, sizeof(path), seq->strip->dir, seq->strip->stripdata->name);
      BLI_path_abs(path, BKE_main_blendfile_path_from_global());

      SEQ_relations_sequence_free_anim(seq);

      if (is_multiview && (seq->views_format == R_IMF_VIEWS_INDIVIDUAL)) {
        char prefix[FILE_MAX];
        const char *ext = NULL;
        const int totfiles = seq_num_files(scene, seq->views_format, true);
        int i = 0;

        BKE_scene_multiview_view_prefix_get(scene, path, prefix, &ext);

        if (prefix[0] != '\0') {
          for (i = 0; i < totfiles; i++) {
            struct anim *anim;
            char str[FILE_MAX];

            seq_multiview_name(scene, i, prefix, ext, str, FILE_MAX);
            anim = openanim(str,
                            IB_rect | ((seq->flag & SEQ_FILTERY) ? IB_animdeinterlace : 0),
                            seq->streamindex,
                            seq->strip->colorspace_settings.name);

            if (anim) {
              seq_anim_add_suffix(scene, anim, i);
              sanim = MEM_mallocN(sizeof(StripAnim), "Strip Anim");
              BLI_addtail(&seq->anims, sanim);
              sanim->anim = anim;
            }
          }
          is_multiview_loaded = true;
        }
      }

      if (is_multiview_loaded == false) {
        struct anim *anim;
        anim = openanim(path,
                        IB_rect | ((seq->flag & SEQ_FILTERY) ? IB_animdeinterlace : 0),
                        seq->streamindex,
                        seq->strip->colorspace_settings.name);
        if (anim) {
          sanim = MEM_mallocN(sizeof(StripAnim), "Strip Anim");
          BLI_addtail(&seq->anims, sanim);
          sanim->anim = anim;
        }
      }

      /* use the first video as reference for everything */
      sanim = seq->anims.first;

      if ((!sanim) || (!sanim->anim)) {
        return;
      }

      IMB_anim_load_metadata(sanim->anim);

      seq->len = IMB_anim_get_duration(
          sanim->anim, seq->strip->proxy ? seq->strip->proxy->tc : IMB_TC_RECORD_RUN);

      seq->len -= seq->anim_startofs;
      seq->len -= seq->anim_endofs;
      if (seq->len < 0) {
        seq->len = 0;
      }
      break;
    }
    case SEQ_TYPE_MOVIECLIP:
      if (seq->clip == NULL) {
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
      if (seq->mask == NULL) {
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
      seq->len = ceil((double)BKE_sound_get_length(bmain, seq->sound) * FPS);
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
    SEQ_transform_set_left_handle_frame(seq, prev_startdisp);
    SEQ_transform_set_right_handle_frame(seq, prev_enddisp);
    SEQ_transform_fix_single_image_seq_offsets(seq);
  }

  ListBase *seqbase = SEQ_active_seqbase_get(SEQ_editing_get(scene));
  SEQ_time_update_sequence(scene, seqbase, seq);
  SEQ_relations_invalidate_cache_raw(scene, seq);
}

void SEQ_add_movie_reload_if_needed(struct Main *bmain,
                                    struct Scene *scene,
                                    struct Sequence *seq,
                                    bool *r_was_reloaded,
                                    bool *r_can_produce_frames)
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
