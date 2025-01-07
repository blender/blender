/* SPDX-FileCopyrightText: 2004 Blender Authors
 *
 * SPDX-License-Identifier: GPL-2.0-or-later */

#pragma once

/** \file
 * \ingroup sequencer
 */

#include "BLI_utildefines.h"

#include "DNA_scene_enums.h"

struct ListBase;
struct Main;
struct Mask;
struct MovieClip;
struct Scene;
struct Strip;
struct Stereo3dFormat;

/** #SeqLoadData.flags */
enum eSeqLoadFlags {
  SEQ_LOAD_SOUND_CACHE = (1 << 1),
  SEQ_LOAD_SOUND_MONO = (1 << 2),
  SEQ_LOAD_MOVIE_SYNC_FPS = (1 << 3),
  SEQ_LOAD_SET_VIEW_TRANSFORM = (1 << 4),
};
ENUM_OPERATORS(eSeqLoadFlags, SEQ_LOAD_SET_VIEW_TRANSFORM)

/* Api for adding new sequence strips. */
struct SeqLoadData {
  int start_frame;
  int channel;
  char name[64]; /* Strip name. */
  /** Typically a `filepath` but may reference any kind of path. */
  char path[1024]; /* 1024 = FILE_MAX */
  struct {
    int len;
    int end_frame;
  } image;         /* Only for image strips. */
  Scene *scene;    /* Only for scene strips. */
  MovieClip *clip; /* Only for clip strips. */
  Mask *mask;      /* Only for mask strips. */
  struct {
    int type;
    int end_frame;
    Strip *seq1;
    Strip *seq2;
  } effect; /* Only for effect strips. */
  eSeqLoadFlags flags;
  eSeqImageFitMethod fit_method;
  bool use_multiview;
  char views_format;
  Stereo3dFormat *stereo3d_format;
  bool allow_invalid_file;     /* Used by RNA API to create placeholder strips. */
  double r_video_stream_start; /* For AV synchronization. Set by `SEQ_add_movie_strip`. */
  bool adjust_playback_rate;
};

/**
 * Initialize common SeqLoadData members
 *
 * \param load_data: SeqLoadData to be initialized
 * \param name: strip name (can be NULL)
 * \param path: path to file that is used as strip input (can be NULL)
 * \param start_frame: timeline frame where strip will be created
 * \param channel: timeline channel where strip will be created
 */
void SEQ_add_load_data_init(
    SeqLoadData *load_data, const char *name, const char *path, int start_frame, int channel);
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
Strip *SEQ_add_image_strip(Main *bmain, Scene *scene, ListBase *seqbase, SeqLoadData *load_data);
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
Strip *SEQ_add_sound_strip(Main *bmain, Scene *scene, ListBase *seqbase, SeqLoadData *load_data);

/**
 * Sync up the sound strip 'seq' with the video data in 'load_data'.
 * This is intended to be used after adding a movie strip and you want to make sure that the audio
 * track is properly synced up with the video.
 *
 * \param bmain: Main reference
 * \param scene: Scene where the sound strip is located
 * \param seq: The sound strip that will be synced
 * \param load_data: SeqLoadData with information necessary to sync the sound strip
 */
void SEQ_add_sound_av_sync(Main *bmain, Scene *scene, Strip *strip, SeqLoadData *load_data);
/**
 * Add meta strip.
 *
 * \param scene: Scene where strips will be added
 * \param seqbase: ListBase where strips will be added
 * \param load_data: SeqLoadData with information necessary to create strip
 * \return created strip
 */
Strip *SEQ_add_meta_strip(Scene *scene, ListBase *seqbase, SeqLoadData *load_data);
/**
 * Add movie strip.
 *
 * \param bmain: Main reference
 * \param scene: Scene where strips will be added
 * \param seqbase: ListBase where strips will be added
 * \param load_data: SeqLoadData with information necessary to create strip
 * \return created strip
 */
Strip *SEQ_add_movie_strip(Main *bmain, Scene *scene, ListBase *seqbase, SeqLoadData *load_data);
/**
 * Add scene strip.
 *
 * \param scene: Scene where strips will be added
 * \param seqbase: ListBase where strips will be added
 * \param load_data: SeqLoadData with information necessary to create strip
 * \return created strip
 */
Strip *SEQ_add_scene_strip(Scene *scene, ListBase *seqbase, SeqLoadData *load_data);
/**
 * Add movieclip strip.
 *
 * \param scene: Scene where strips will be added
 * \param seqbase: ListBase where strips will be added
 * \param load_data: SeqLoadData with information necessary to create strip
 * \return created strip
 */
Strip *SEQ_add_movieclip_strip(Scene *scene, ListBase *seqbase, SeqLoadData *load_data);
/**
 * Add mask strip.
 *
 * \param scene: Scene where strips will be added
 * \param seqbase: ListBase where strips will be added
 * \param load_data: SeqLoadData with information necessary to create strip
 * \return created strip
 */
Strip *SEQ_add_mask_strip(Scene *scene, ListBase *seqbase, SeqLoadData *load_data);
/**
 * Add effect strip.
 *
 * \param scene: Scene where strips will be added
 * \param seqbase: ListBase where strips will be added
 * \param load_data: SeqLoadData with information necessary to create strip
 * \return created strip
 */
Strip *SEQ_add_effect_strip(Scene *scene, ListBase *seqbase, SeqLoadData *load_data);
/**
 * Set directory used by image strip.
 *
 * \param seq: image strip to be changed
 * \param path: directory path
 */
void SEQ_add_image_set_directory(Strip *strip, const char *dirpath);
/**
 * Set directory used by image strip.
 *
 * \param seq: image strip to be changed
 * \param strip_frame: frame index of strip to be changed
 * \param filename: image filename (only filename, not complete path)
 */
void SEQ_add_image_load_file(Scene *scene, Strip *strip, size_t strip_frame, const char *filename);
/**
 * Set image strip alpha mode
 *
 * \param seq: image strip to be changed
 */
void SEQ_add_image_init_alpha_mode(Strip *strip);
void SEQ_add_reload_new_file(Main *bmain, Scene *scene, Strip *strip, bool lock_range);
void SEQ_add_movie_reload_if_needed(
    Main *bmain, Scene *scene, Strip *strip, bool *r_was_reloaded, bool *r_can_produce_frames);
