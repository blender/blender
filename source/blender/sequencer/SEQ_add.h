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

/* SeqLoadData.flags */
typedef enum eSeqLoadFlags {
  SEQ_LOAD_SOUND_CACHE = (1 << 1),
  SEQ_LOAD_SOUND_MONO = (1 << 2),
  SEQ_LOAD_MOVIE_SYNC_FPS = (1 << 3),
} eSeqLoadFlags;

/* Api for adding new sequence strips. */
typedef struct SeqLoadData {
  int start_frame;
  int channel;
  char name[64];   /* Strip name. */
  char path[1024]; /* 1024 = FILE_MAX */
  struct {
    int len;
    int end_frame;
  } image;                /* Only for image strips. */
  struct Scene *scene;    /* Only for scene strips. */
  struct MovieClip *clip; /* Only for clip strips. */
  struct Mask *mask;      /* Only for mask strips. */
  struct {
    int type;
    int end_frame;
    struct Sequence *seq1;
    struct Sequence *seq2;
    struct Sequence *seq3;
  } effect; /* Only for effect strips. */
  eSeqLoadFlags flags;
  eSeqImageFitMethod fit_method;
  bool use_multiview;
  char views_format;
  struct Stereo3dFormat *stereo3d_format;
  bool allow_invalid_file; /* Used by RNA API to create placeholder strips. */
} SeqLoadData;

void SEQ_add_load_data_init(struct SeqLoadData *load_data,
                            const char *name,
                            const char *path,
                            const int start_frame,
                            const int channel);
struct Sequence *SEQ_add_image_strip(struct Main *bmain,
                                     struct Scene *scene,
                                     struct ListBase *seqbase,
                                     struct SeqLoadData *load_data);
struct Sequence *SEQ_add_sound_strip(struct Main *bmain,
                                     struct Scene *scene,
                                     struct ListBase *seqbase,
                                     struct SeqLoadData *load_data);
struct Sequence *SEQ_add_movie_strip(struct Main *bmain,
                                     struct Scene *scene,
                                     struct ListBase *seqbase,
                                     struct SeqLoadData *load_data);
struct Sequence *SEQ_add_scene_strip(struct Scene *scene,
                                     struct ListBase *seqbase,
                                     struct SeqLoadData *load_data);
struct Sequence *SEQ_add_movieclip_strip(struct Scene *scene,
                                         struct ListBase *seqbase,
                                         struct SeqLoadData *load_data);
struct Sequence *SEQ_add_mask_strip(struct Scene *scene,
                                    struct ListBase *seqbase,
                                    struct SeqLoadData *load_data);
struct Sequence *SEQ_add_effect_strip(struct Scene *scene,
                                      struct ListBase *seqbase,
                                      struct SeqLoadData *load_data);
void SEQ_add_image_set_directory(struct Sequence *seq, char *path);
void SEQ_add_image_load_file(struct Sequence *seq, size_t strip_frame, char *filename);
void SEQ_add_image_init_alpha_mode(struct Sequence *seq);
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
