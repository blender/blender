/* SPDX-FileCopyrightText: 2001-2002 NaN Holding BV. All rights reserved.
 *
 * SPDX-License-Identifier: GPL-2.0-or-later */
/** \file
 * \ingroup DNA
 */

#pragma once

#include "DNA_ID.h"
#include "DNA_defs.h"

struct Ipo;
struct PackedFile;

typedef struct bSound {
  ID id;

  /**
   * The path to the sound file.
   */
  /** 1024 = FILE_MAX. */
  char filepath[1024];

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
  struct Ipo *ipo;

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

  /* Unused currently. */
  // int type;
  // struct bSound *child_sound;

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
  /* XXX unused currently (SOUND_TYPE_LIMITER) */
  /* float start, end; */

  /* Description of Audio channels, as of #eSoundChannels. */
  int audio_channels;

  int samplerate;

} bSound;

/* XXX unused currently */
#if 0
typedef enum eSound_Type {
  SOUND_TYPE_INVALID = -1,
  SOUND_TYPE_FILE = 0,
  SOUND_TYPE_BUFFER = 1,
  SOUND_TYPE_LIMITER = 2,
} eSound_Type;
#endif

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
