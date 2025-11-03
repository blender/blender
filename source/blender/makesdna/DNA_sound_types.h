/* SPDX-FileCopyrightText: 2001-2002 NaN Holding BV. All rights reserved.
 *
 * SPDX-License-Identifier: GPL-2.0-or-later */
/** \file
 * \ingroup DNA
 */

#pragma once

#include "DNA_ID.h"
#include "DNA_defs.h"

struct PackedFile;

typedef struct bSound {
#ifdef __cplusplus
  /** See #ID_Type comment for why this is here. */
  static constexpr ID_Type id_type = ID_SO;
#endif

  ID id;

  void *_pad1;

  /**
   * The path to the sound file.
   */
  char filepath[/*FILE_MAX*/ 1024];

  /**
   * The packed file.
   */
  struct PackedFile *packedfile;

  /**
   * The handle for audaspace.
   */
  void *handle;

  /**
   * Deprecated; used for loading pre 2.5 files.
   */
  struct PackedFile *newpackedfile;
  void *_pad0;

  float volume;
  float attenuation;
  float pitch;
  float min_gain;
  float max_gain;
  float distance;
  short flags;
  /** Runtime only, always reset in readfile. */
  short tags;
  char _pad[4];
  double offset_time;

  /**
   * The audaspace handle for cache.
   */
  void *cache;

  /**
   * Waveform display data.
   */
  void *waveform;

  /**
   * The audaspace handle that should actually be played back.
   * Should be cache if cache != NULL; otherwise its handle
   */
  void *playback_handle;

  /** Spin-lock for asynchronous loading of sounds. */
  void *spinlock;

  /* Description of Audio channels, as of #eSoundChannels. */
  int audio_channels;

  int samplerate;

} bSound;

/** #bSound.flags */
enum {
#ifdef DNA_DEPRECATED_ALLOW
  /* deprecated! used for sound actuator loading */
  SOUND_FLAGS_3D = (1 << 3),
#endif
  SOUND_FLAGS_CACHING = (1 << 4),
  SOUND_FLAGS_MONO = (1 << 5),
};

/** #bSound.tags */
enum {
  /* Do not free/reset waveform on sound load, only used by undo code. */
  SOUND_TAGS_WAVEFORM_NO_RELOAD = 1 << 0,
  SOUND_TAGS_WAVEFORM_LOADING = (1 << 6),
};
