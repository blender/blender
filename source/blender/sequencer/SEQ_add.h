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
  SEQ_LOAD_SET_VIEW_TRANSFORM = (1 << 4),
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

/**
 * Initialize common SeqLoadData members
 *
 * \param load_data: SeqLoadData to be initialized
 * \param name: strip name (can be NULL)
 * \param path: path to file that is used as strip input (can be NULL)
 * \param start_frame: timeline frame where strip will be created
 * \param channel: timeline channel where strip will be created
 */
void SEQ_add_load_data_init(struct SeqLoadData *load_data,
                            const char *name,
                            const char *path,
                            int start_frame,
                            int channel);
/**
 * Add image strip.
 * \note Use #SEQ_add_image_set_directory() and #SEQ_add_image_load_file() to load image sequences
 *
 * \param bmain: Main reference
 * \param scene: Scene where strips will be added
 * \param seqbase: ListBase where strips will be added
 * \param load_data: SeqLoadData with information necessary to create strip
 * \return created strip
 */
struct Sequence *SEQ_add_image_strip(struct Main *bmain,
                                     struct Scene *scene,
                                     struct ListBase *seqbase,
                                     struct SeqLoadData *load_data);
/**
 * Add sound strip.
 * \note Use SEQ_add_image_set_directory() and SEQ_add_image_load_file() to load image sequences
 *
 * \param bmain: Main reference
 * \param scene: Scene where strips will be added
 * \param seqbase: ListBase where strips will be added
 * \param load_data: SeqLoadData with information necessary to create strip
 * \return created strip
 */
struct Sequence *SEQ_add_sound_strip(struct Main *bmain,
                                     struct Scene *scene,
                                     struct ListBase *seqbase,
                                     struct SeqLoadData *load_data,
                                     double audio_offset);
/**
 * Add meta strip.
 *
 * \param scene: Scene where strips will be added
 * \param seqbase: ListBase where strips will be added
 * \param load_data: SeqLoadData with information necessary to create strip
 * \return created strip
 */
struct Sequence *SEQ_add_meta_strip(struct Scene *scene,
                                    struct ListBase *seqbase,
                                    struct SeqLoadData *load_data);
/**
 * Add movie strip.
 *
 * \param bmain: Main reference
 * \param scene: Scene where strips will be added
 * \param seqbase: ListBase where strips will be added
 * \param load_data: SeqLoadData with information necessary to create strip
 * \return created strip
 */
struct Sequence *SEQ_add_movie_strip(struct Main *bmain,
                                     struct Scene *scene,
                                     struct ListBase *seqbase,
                                     struct SeqLoadData *load_data,
                                     double *r_start_offset);
/**
 * Add scene strip.
 *
 * \param scene: Scene where strips will be added
 * \param seqbase: ListBase where strips will be added
 * \param load_data: SeqLoadData with information necessary to create strip
 * \return created strip
 */
struct Sequence *SEQ_add_scene_strip(struct Scene *scene,
                                     struct ListBase *seqbase,
                                     struct SeqLoadData *load_data);
/**
 * Add movieclip strip.
 *
 * \param scene: Scene where strips will be added
 * \param seqbase: ListBase where strips will be added
 * \param load_data: SeqLoadData with information necessary to create strip
 * \return created strip
 */
struct Sequence *SEQ_add_movieclip_strip(struct Scene *scene,
                                         struct ListBase *seqbase,
                                         struct SeqLoadData *load_data);
/**
 * Add mask strip.
 *
 * \param scene: Scene where strips will be added
 * \param seqbase: ListBase where strips will be added
 * \param load_data: SeqLoadData with information necessary to create strip
 * \return created strip
 */
struct Sequence *SEQ_add_mask_strip(struct Scene *scene,
                                    struct ListBase *seqbase,
                                    struct SeqLoadData *load_data);
/**
 * Add effect strip.
 *
 * \param scene: Scene where strips will be added
 * \param seqbase: ListBase where strips will be added
 * \param load_data: SeqLoadData with information necessary to create strip
 * \return created strip
 */
struct Sequence *SEQ_add_effect_strip(struct Scene *scene,
                                      struct ListBase *seqbase,
                                      struct SeqLoadData *load_data);
/**
 * Set directory used by image strip.
 *
 * \param seq: image strip to be changed
 * \param path: directory path
 */
void SEQ_add_image_set_directory(struct Sequence *seq, char *path);
/**
 * Set directory used by image strip.
 *
 * \param seq: image strip to be changed
 * \param strip_frame: frame index of strip to be changed
 * \param filename: image filename (only filename, not complete path)
 */
void SEQ_add_image_load_file(struct Sequence *seq, size_t strip_frame, char *filename);
/**
 * Set image strip alpha mode
 *
 * \param seq: image strip to be changed
 */
void SEQ_add_image_init_alpha_mode(struct Sequence *seq);
/**
 * \note caller should run `SEQ_time_update_sequence(scene, seq)` after..
 */
void SEQ_add_reload_new_file(struct Main *bmain,
                             struct Scene *scene,
                             struct Sequence *seq,
                             bool lock_range);
void SEQ_add_movie_reload_if_needed(struct Main *bmain,
                                    struct Scene *scene,
                                    struct Sequence *seq,
                                    bool *r_was_reloaded,
                                    bool *r_can_produce_frames);

#ifdef __cplusplus
}
#endif
