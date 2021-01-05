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
 * The Original Code is Copyright (C) 2004 Blender Foundation.
 * All rights reserved.
 */

#pragma once

/** \file
 * \ingroup sequencer
 */

#ifdef __cplusplus
extern "C" {
#endif

struct ListBase;
struct Scene;
struct Sequence;
struct bContext;

/* api for adding new sequence strips */
typedef struct SeqLoadInfo {
  int start_frame;
  int end_frame;
  int channel;
  int flag; /* use sound, replace sel */
  int type;
  int len;         /* only for image strips */
  char path[1024]; /* 1024 = FILE_MAX */
  eSeqImageFitMethod fit_method;

  /* multiview */
  char views_format;
  struct Stereo3dFormat *stereo3d_format;

  /* return values */
  char name[64];
  struct Sequence *seq_sound; /* for movie's */
  int tot_success;
  int tot_error;
} SeqLoadInfo;

/* SeqLoadInfo.flag */
#define SEQ_LOAD_REPLACE_SEL (1 << 0)
#define SEQ_LOAD_FRAME_ADVANCE (1 << 1)
#define SEQ_LOAD_MOVIE_SOUND (1 << 2)
#define SEQ_LOAD_SOUND_CACHE (1 << 3)
#define SEQ_LOAD_SYNC_FPS (1 << 4)
#define SEQ_LOAD_SOUND_MONO (1 << 5)

/* use as an api function */
typedef struct Sequence *(*SeqLoadFn)(struct bContext *, ListBase *, struct SeqLoadInfo *);

struct Sequence *SEQ_add_image_strip(struct bContext *C,
                                     ListBase *seqbasep,
                                     struct SeqLoadInfo *seq_load);
struct Sequence *SEQ_add_sound_strip(struct bContext *C,
                                     ListBase *seqbasep,
                                     struct SeqLoadInfo *seq_load);
struct Sequence *SEQ_add_movie_strip(struct bContext *C,
                                     ListBase *seqbasep,
                                     struct SeqLoadInfo *seq_load);
void SEQ_add_reload_new_file(struct Main *bmain,
                             struct Scene *scene,
                             struct Sequence *seq,
                             const bool lock_range);
void SEQ_add_movie_reload_if_needed(struct Main *bmain,
                                    struct Scene *scene,
                                    struct Sequence *seq,
                                    bool *r_was_reloaded,
                                    bool *r_can_produce_frames);

#ifdef __cplusplus
}
#endif
