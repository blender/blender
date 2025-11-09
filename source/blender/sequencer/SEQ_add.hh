/* SPDX-FileCopyrightText: 2004 Blender Authors
 *
 * SPDX-License-Identifier: GPL-2.0-or-later */

#pragma once

/** \file
 * \ingroup sequencer
 */

#include "BLI_enum_flags.hh"

#include "DNA_scene_enums.h"

struct ListBase;
struct Main;
struct Mask;
struct MovieClip;
struct Scene;
struct Strip;
struct Stereo3dFormat;

namespace blender::seq {

/** #SeqLoadData.flags */
enum eLoadFlags {
  SEQ_LOAD_SOUND_CACHE = (1 << 1),
  SEQ_LOAD_SOUND_MONO = (1 << 2),
  SEQ_LOAD_MOVIE_SYNC_FPS = (1 << 3),
  SEQ_LOAD_SET_VIEW_TRANSFORM = (1 << 4),
};
ENUM_OPERATORS(eLoadFlags)

/** API for adding new strips. If multiple strips are added, data is updated for each one. */
struct LoadData {
  int start_frame;
  int channel;
  char name[64]; /* Strip name, including file extension. */
  /** Typically a `filepath` but may reference any kind of path. */
  char path[/*FILE_MAX*/ 1024];
  struct {
    int count; /* Number of images in this strip, 1 if not an image sequence. */
    int length;
  } image;         /* Only for image strips. */
  Scene *scene;    /* Only for scene strips. */
  MovieClip *clip; /* Only for clip strips. */
  Mask *mask;      /* Only for mask strips. */
  struct {
    int type;
    int length;
    Strip *input1;
    Strip *input2;
  } effect; /* Only for effect strips. */
  eLoadFlags flags;
  eSeqImageFitMethod fit_method;
  bool use_multiview;
  char views_format;
  Stereo3dFormat *stereo3d_format;
  bool allow_invalid_file;     /* Used by RNA API to create placeholder strips. */
  double r_video_stream_start; /* For AV synchronization. Set by `seq::add_movie_strip`. */
  bool adjust_playback_rate;
  bool allow_overlap;
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
void add_load_data_init(
    LoadData *load_data, const char *name, const char *path, int start_frame, int channel);
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
Strip *add_image_strip(Main *bmain, Scene *scene, ListBase *seqbase, LoadData *load_data);
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
Strip *add_sound_strip(Main *bmain, Scene *scene, ListBase *seqbase, LoadData *load_data);

/**
 * Sync up the sound strip 'seq' with the video data in 'load_data'.
 * This is intended to be used after adding a movie strip and you want to make sure that the audio
 * track is properly synced up with the video.
 *
 * \param bmain: Main reference
 * \param scene: Scene where the sound strip is located
 * \param strip: The sound strip that will be synced
 * \param load_data: SeqLoadData with information necessary to sync the sound strip
 */
void add_sound_av_sync(Main *bmain, Scene *scene, Strip *strip, LoadData *load_data);
/**
 * Add meta strip.
 *
 * \param scene: Scene where strips will be added
 * \param seqbase: ListBase where strips will be added
 * \param load_data: SeqLoadData with information necessary to create strip
 * \return created strip
 */
Strip *add_meta_strip(Scene *scene, ListBase *seqbase, LoadData *load_data);
/**
 * Add movie strip.
 *
 * \param bmain: Main reference
 * \param scene: Scene where strips will be added
 * \param seqbase: ListBase where strips will be added
 * \param load_data: SeqLoadData with information necessary to create strip
 * \return created strip
 */
Strip *add_movie_strip(Main *bmain, Scene *scene, ListBase *seqbase, LoadData *load_data);
/**
 * Add scene strip.
 *
 * \param scene: Scene where strips will be added
 * \param seqbase: ListBase where strips will be added
 * \param load_data: SeqLoadData with information necessary to create strip
 * \return created strip
 */
Strip *add_scene_strip(Scene *scene, ListBase *seqbase, LoadData *load_data);
/**
 * Add movieclip strip.
 *
 * \param scene: Scene where strips will be added
 * \param seqbase: ListBase where strips will be added
 * \param load_data: SeqLoadData with information necessary to create strip
 * \return created strip
 */
Strip *add_movieclip_strip(Scene *scene, ListBase *seqbase, LoadData *load_data);
/**
 * Add mask strip.
 *
 * \param scene: Scene where strips will be added
 * \param seqbase: ListBase where strips will be added
 * \param load_data: SeqLoadData with information necessary to create strip
 * \return created strip
 */
Strip *add_mask_strip(Scene *scene, ListBase *seqbase, LoadData *load_data);
/**
 * Add effect strip.
 *
 * \param scene: Scene where strips will be added
 * \param seqbase: ListBase where strips will be added
 * \param load_data: SeqLoadData with information necessary to create strip
 * \return created strip
 */
Strip *add_effect_strip(Scene *scene, ListBase *seqbase, LoadData *load_data);
/**
 * Set directory used by image strip.
 *
 * \param strip: image strip to be changed
 * \param path: directory path
 */
void add_image_set_directory(Strip *strip, const char *dirpath);
/**
 * Set directory used by image strip.
 *
 * \param strip: image strip to be changed
 * \param strip_frame: frame index of strip to be changed
 * \param filename: image filename (only filename, not complete path)
 */
void add_image_load_file(Scene *scene, Strip *strip, size_t strip_frame, const char *filename);
/**
 * Set image strip alpha mode
 *
 * \param strip: image strip to be changed
 */
void add_image_init_alpha_mode(Main *bmain, Scene *scene, Strip *strip);
void add_reload_new_file(Main *bmain, Scene *scene, Strip *strip, bool lock_range);
void add_movie_reload_if_needed(
    Main *bmain, Scene *scene, Strip *strip, bool *r_was_reloaded, bool *r_can_produce_frames);

}  // namespace blender::seq
