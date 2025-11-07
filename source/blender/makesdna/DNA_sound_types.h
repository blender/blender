/* SPDX-FileCopyrightText: 2001-2002 NaN Holding BV. All rights reserved.
 *
 * SPDX-License-Identifier: GPL-2.0-or-later */
/** \file
 * \ingroup DNA
 */

#pragma once

#include "DNA_ID.h"
#include "DNA_defs.h"

#ifdef __cplusplus
namespace blender::bke {
struct SoundRuntime;
}  // namespace blender::bke
using SoundRuntimeHandle = blender::bke::SoundRuntime;
#else
typedef struct SoundRuntimeHandle SoundRuntimeHandle;
#endif

struct PackedFile;

typedef struct bSound {
#ifdef __cplusplus
  /** See #ID_Type comment for why this is here. */
  static constexpr ID_Type id_type = ID_SO;
#endif

  ID id;

  /**
   * The path to the sound file.
   */
  char filepath[/*FILE_MAX*/ 1024];

  /**
   * The packed file.
   */
  struct PackedFile *packedfile;

  /**
   * Deprecated; used for loading pre 2.5 files.
   */
  struct PackedFile *newpackedfile;
  void *_pad0;

  double offset_time;
  float volume;
  float attenuation;
  float pitch;
  float min_gain;
  float max_gain;
  float distance;
  /* Description of Audio channels, as of #eSoundChannels. */
  int audio_channels;
  int samplerate;
  short flags;
  char _pad1[6];

  SoundRuntimeHandle *runtime;
  void *_pad2;
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
