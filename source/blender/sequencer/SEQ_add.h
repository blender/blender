/* SPDX-FileCopyrightText: 2004 Blender Foundation
 *
 * SPDX-License-Identifier: GPL-2.0-or-later */

#pragma once

/** \file
 * \ingroup sequencer
 */
#include "BLI_utildefines.h"
#ifdef __cplusplus
extern "C" {
#endif

struct ListBase;
struct Scene;
struct Sequence;

/** #SeqLoadData.flags */
typedef enum eSeqLoadFlags {
  SEQ_LOAD_SOUND_CACHE = (1 << 1),
  SEQ_LOAD_SOUND_MONO = (1 << 2),
  SEQ_LOAD_MOVIE_SYNC_FPS = (1 << 3),
  SEQ_LOAD_SET_VIEW_TRANSFORM = (1 << 4),
} eSeqLoadFlags;
ENUM_OPERATORS(eSeqLoadFlags, SEQ_LOAD_SET_VIEW_TRANSFORM)

/* Api for adding new sequence strips. */
typedef struct SeqLoadData {
  int start_frame;
  int channel;
  char name[64]; /* Strip name. */
  /** Typically a `filepath` but may reference any kind of path. */
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
  bool allow_invalid_file;     /* Used by RNA API to create placeholder strips. */
  double r_video_stream_start; /* For AV synchronization. Set by `SEQ_add_movie_strip`. */
  bool adjust_playback_rate;
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
                                     struct SeqLoadData *load_data);
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
                                     struct SeqLoadData *load_data);
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
void SEQ_add_image_set_directory(struct Sequence *seq, const char *dirpath);
/**
 * Set directory used by image strip.
 *
 * \param seq: image strip to be changed
 * \param strip_frame: frame index of strip to be changed
 * \param filename: image filename (only filename, not complete path)
 */
void SEQ_add_image_load_file(struct Scene *scene,
                             struct Sequence *seq,
                             size_t strip_frame,
                             const char *filename);
/**
 * Set image strip alpha mode
 *
 * \param seq: image strip to be changed
 */
void SEQ_add_image_init_alpha_mode(struct Sequence *seq);
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
