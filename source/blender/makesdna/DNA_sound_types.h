/* SPDX-FileCopyrightText: 2001-2002 NaN Holding BV. All rights reserved.
 *
 * SPDX-License-Identifier: GPL-2.0-or-later */
/** \file
 * \ingroup DNA
 */

#pragma once

#include "DNA_ID.h"

namespace blender {

namespace bke {
struct SoundRuntime;
}  // namespace bke

struct PackedFile;

/** #bSound.flags */
enum {
#ifdef DNA_DEPRECATED_ALLOW
  /* deprecated! used for sound actuator loading */
  SOUND_FLAGS_3D = (1 << 3),
#endif
  SOUND_FLAGS_CACHING = (1 << 4),
  SOUND_FLAGS_MONO = (1 << 5),
};

struct bSound {
#ifdef __cplusplus
  /** See #ID_Type comment for why this is here. */
  static constexpr ID_Type id_type = ID_SO;
#endif

  ID id;

  /**
   * The path to the sound file.
   */
  char filepath[/*FILE_MAX*/ 1024] = "";

  /**
   * The packed file.
   */
  struct PackedFile *packedfile = nullptr;

  /**
   * Deprecated; used for loading pre 2.5 files.
   */
  struct PackedFile *newpackedfile = nullptr;
  void *_pad0 = nullptr;

  double offset_time = 0;
  float volume = 0;
  float attenuation = 0;
  float pitch = 0;
  float min_gain = 0;
  float max_gain = 0;
  float distance = 0;
  /* Description of Audio channels, as of #eSoundChannels. */
  int audio_channels = 0;
  int samplerate = 0;
  short flags = 0;
  char _pad1[6] = {};

  bke::SoundRuntime *runtime = nullptr;
  void *_pad2 = nullptr;
};

}  // namespace blender
