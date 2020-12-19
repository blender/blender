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

#include "IMB_imbuf.h"
#include "IMB_imbuf_types.h"
#include "IMB_metadata.h"

#include "SEQ_add.h"
#include "SEQ_relations.h"
#include "SEQ_select.h"
#include "SEQ_sequencer.h"
#include "SEQ_time.h"
#include "SEQ_transform.h"
#include "SEQ_utils.h"

#include "multiview.h"
#include "proxy.h"
#include "utils.h"

static void seq_load_apply(Main *bmain, Scene *scene, Sequence *seq, SeqLoadInfo *seq_load)
{
  if (seq) {
    BLI_strncpy_utf8(seq->name + 2, seq_load->name, sizeof(seq->name) - 2);
    BLI_utf8_invalid_strip(seq->name + 2, strlen(seq->name + 2));
    SEQ_sequence_base_unique_name_recursive(&scene->ed->seqbase, seq);

    if (seq_load->flag & SEQ_LOAD_FRAME_ADVANCE) {
      seq_load->start_frame += (seq->enddisp - seq->startdisp);
    }

    if (seq_load->flag & SEQ_LOAD_REPLACE_SEL) {
      seq_load->flag |= SELECT;
      SEQ_select_active_set(scene, seq);
    }

    if (seq_load->flag & SEQ_LOAD_SOUND_MONO) {
      seq->sound->flags |= SOUND_FLAGS_MONO;
      BKE_sound_load(bmain, seq->sound);
    }

    if (seq_load->flag & SEQ_LOAD_SOUND_CACHE) {
      if (seq->sound) {
        seq->sound->flags |= SOUND_FLAGS_CACHING;
      }
    }

    seq_load->tot_success++;
  }
  else {
    seq_load->tot_error++;
  }
}

/* NOTE: this function doesn't fill in image names */
Sequence *SEQ_add_image_strip(bContext *C, ListBase *seqbasep, SeqLoadInfo *seq_load)
{
  Scene *scene = CTX_data_scene(C); /* only for active seq */
  Sequence *seq;
  Strip *strip;

  seq = SEQ_sequence_alloc(seqbasep, seq_load->start_frame, seq_load->channel, SEQ_TYPE_IMAGE);
  seq->blend_mode = SEQ_TYPE_CROSS; /* so alpha adjustment fade to the strip below */

  /* basic defaults */
  seq->len = seq_load->len ? seq_load->len : 1;

  strip = seq->strip;
  strip->stripdata = MEM_callocN(seq->len * sizeof(StripElem), "stripelem");
  BLI_strncpy(strip->dir, seq_load->path, sizeof(strip->dir));

  if (seq_load->stereo3d_format) {
    *seq->stereo3d_format = *seq_load->stereo3d_format;
  }

  seq->views_format = seq_load->views_format;
  seq->flag |= seq_load->flag & SEQ_USE_VIEWS;

  seq_load_apply(CTX_data_main(C), scene, seq, seq_load);

  char file_path[FILE_MAX];
  BLI_join_dirfile(file_path, sizeof(file_path), seq_load->path, seq_load->name);
  BLI_path_abs(file_path, BKE_main_blendfile_path(CTX_data_main(C)));
  ImBuf *ibuf = IMB_loadiffname(file_path, IB_rect, seq->strip->colorspace_settings.name);
  if (ibuf != NULL) {
    SEQ_set_scale_to_fit(
        seq, ibuf->x, ibuf->y, scene->r.xsch, scene->r.ysch, seq_load->fit_method);
    IMB_freeImBuf(ibuf);
  }

  SEQ_relations_invalidate_cache_composite(scene, seq);

  return seq;
}

#ifdef WITH_AUDASPACE
Sequence *SEQ_add_sound_strip(bContext *C, ListBase *seqbasep, SeqLoadInfo *seq_load)
{
  Main *bmain = CTX_data_main(C);
  Scene *scene = CTX_data_scene(C); /* only for sound */
  Editing *ed = SEQ_editing_get(scene, false);
  bSound *sound;

  Sequence *seq; /* generic strip vars */
  Strip *strip;
  StripElem *se;

  sound = BKE_sound_new_file(bmain, seq_load->path); /* handles relative paths */

  SoundInfo info;
  if (!BKE_sound_info_get(bmain, sound, &info)) {
    BKE_id_free(bmain, sound);
    return NULL;
  }

  if (info.specs.channels == SOUND_CHANNELS_INVALID) {
    BKE_id_free(bmain, sound);
    return NULL;
  }

  seq = SEQ_sequence_alloc(seqbasep, seq_load->start_frame, seq_load->channel, SEQ_TYPE_SOUND_RAM);
  seq->sound = sound;
  BLI_strncpy(seq->name + 2, "Sound", SEQ_NAME_MAXSTR - 2);
  SEQ_sequence_base_unique_name_recursive(&scene->ed->seqbase, seq);

  /* basic defaults */
  /* We add a very small negative offset here, because
   * ceil(132.0) == 133.0, not nice with videos, see T47135. */
  seq->len = (int)ceil((double)info.length * FPS - 1e-4);
  strip = seq->strip;

  /* we only need 1 element to store the filename */
  strip->stripdata = se = MEM_callocN(sizeof(StripElem), "stripelem");

  BLI_split_dirfile(seq_load->path, strip->dir, se->name, sizeof(strip->dir), sizeof(se->name));

  seq->scene_sound = NULL;

  SEQ_time_update_sequence_bounds(scene, seq);

  /* last active name */
  BLI_strncpy(ed->act_sounddir, strip->dir, FILE_MAXDIR);

  seq_load_apply(bmain, scene, seq, seq_load);

  /* TODO(sergey): Shall we tag here or in the operator? */
  DEG_relations_tag_update(bmain);

  return seq;
}
#else   // WITH_AUDASPACE
Sequence *SEQ_add_sound_strip(bContext *C, ListBase *seqbasep, SeqLoadInfo *seq_load)
{
  (void)C;
  (void)seqbasep;
  (void)seq_load;
  return NULL;
}
#endif  // WITH_AUDASPACE

Sequence *SEQ_add_movie_strip(bContext *C, ListBase *seqbasep, SeqLoadInfo *seq_load)
{
  Main *bmain = CTX_data_main(C);
  Scene *scene = CTX_data_scene(C); /* only for sound */
  char path[sizeof(seq_load->path)];

  Sequence *seq; /* generic strip vars */
  Strip *strip;
  StripElem *se;
  char colorspace[64] = "\0"; /* MAX_COLORSPACE_NAME */
  bool is_multiview_loaded = false;
  const bool is_multiview = (seq_load->flag & SEQ_USE_VIEWS) != 0;
  const int totfiles = seq_num_files(scene, seq_load->views_format, is_multiview);
  struct anim **anim_arr;
  int i;

  BLI_strncpy(path, seq_load->path, sizeof(path));
  BLI_path_abs(path, BKE_main_blendfile_path(bmain));

  anim_arr = MEM_callocN(sizeof(struct anim *) * totfiles, "Video files");

  if (is_multiview && (seq_load->views_format == R_IMF_VIEWS_INDIVIDUAL)) {
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

      if (j == 0) {
        MEM_freeN(anim_arr);
        return NULL;
      }
      is_multiview_loaded = true;
    }
  }

  if (is_multiview_loaded == false) {
    anim_arr[0] = openanim(path, IB_rect, 0, colorspace);

    if (anim_arr[0] == NULL) {
      MEM_freeN(anim_arr);
      return NULL;
    }
  }

  if (seq_load->flag & SEQ_LOAD_MOVIE_SOUND) {
    seq_load->channel++;
  }
  seq = SEQ_sequence_alloc(seqbasep, seq_load->start_frame, seq_load->channel, SEQ_TYPE_MOVIE);

  /* multiview settings */
  if (seq_load->stereo3d_format) {
    *seq->stereo3d_format = *seq_load->stereo3d_format;
    seq->views_format = seq_load->views_format;
  }
  seq->flag |= seq_load->flag & SEQ_USE_VIEWS;

  seq->type = SEQ_TYPE_MOVIE;
  seq->blend_mode = SEQ_TYPE_CROSS; /* so alpha adjustment fade to the strip below */

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

  IMB_anim_load_metadata(anim_arr[0]);

  seq->anim_preseek = IMB_anim_get_preseek(anim_arr[0]);

  const float width = IMB_anim_get_image_width(anim_arr[0]);
  const float height = IMB_anim_get_image_height(anim_arr[0]);
  SEQ_set_scale_to_fit(seq, width, height, scene->r.xsch, scene->r.ysch, seq_load->fit_method);

  BLI_strncpy(seq->name + 2, "Movie", SEQ_NAME_MAXSTR - 2);
  SEQ_sequence_base_unique_name_recursive(&scene->ed->seqbase, seq);

  /* adjust scene's frame rate settings to match */
  if (seq_load->flag & SEQ_LOAD_SYNC_FPS) {
    IMB_anim_get_fps(anim_arr[0], &scene->r.frs_sec, &scene->r.frs_sec_base, true);
  }

  /* basic defaults */
  seq->len = IMB_anim_get_duration(anim_arr[0], IMB_TC_RECORD_RUN);
  strip = seq->strip;

  BLI_strncpy(seq->strip->colorspace_settings.name,
              colorspace,
              sizeof(seq->strip->colorspace_settings.name));

  /* we only need 1 element for MOVIE strips */
  strip->stripdata = se = MEM_callocN(sizeof(StripElem), "stripelem");

  BLI_split_dirfile(seq_load->path, strip->dir, se->name, sizeof(strip->dir), sizeof(se->name));

  SEQ_time_update_sequence_bounds(scene, seq);

  if (seq_load->name[0] == '\0') {
    BLI_strncpy(seq_load->name, se->name, sizeof(seq_load->name));
  }

  if (seq_load->flag & SEQ_LOAD_MOVIE_SOUND) {
    int start_frame_back = seq_load->start_frame;
    seq_load->channel--;
    seq_load->seq_sound = SEQ_add_sound_strip(C, seqbasep, seq_load);
    seq_load->start_frame = start_frame_back;
  }

  /* can be NULL */
  seq_load_apply(CTX_data_main(C), scene, seq, seq_load);
  SEQ_relations_invalidate_cache_composite(scene, seq);

  MEM_freeN(anim_arr);
  return seq;
}

/* note: caller should run SEQ_time_update_sequence(scene, seq) after */
void SEQ_add_reload_new_file(Main *bmain, Scene *scene, Sequence *seq, const bool lock_range)
{
  char path[FILE_MAX];
  int prev_startdisp = 0, prev_enddisp = 0;
  /* note: don't rename the strip, will break animation curves */

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
    SEQ_time_update_sequence_bounds(scene, seq);
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

      seq->anim_preseek = IMB_anim_get_preseek(sanim->anim);

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

  SEQ_time_update_sequence(scene, seq);
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

  /* The Sequence struct allows for multiple anim structs to be associated with one strip. This
   * function will return true only if there is at least one 'anim' AND all anims can produce
   * frames. */

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
