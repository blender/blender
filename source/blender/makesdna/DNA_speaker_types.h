/* SPDX-FileCopyrightText: 2023 Blender Authors
 *
 * SPDX-License-Identifier: GPL-2.0-or-later */

/** \file
 * \ingroup DNA
 */

#pragma once

#include "DNA_ID.h"

#ifdef __cplusplus
extern "C" {
#endif

struct AnimData;
struct bSound;

typedef struct Speaker {
  ID id;
  /** Animation data (must be immediately after id for utilities to use it). */
  struct AnimData *adt;

  struct bSound *sound;

  /* not animatable properties */
  float volume_max;
  float volume_min;
  float distance_max;
  float distance_reference;
  float attenuation;
  float cone_angle_outer;
  float cone_angle_inner;
  float cone_volume_outer;

  /* animatable properties */
  float volume;
  float pitch;

  /* flag */
  short flag;
  char _pad1[6];
} Speaker;

/* **************** SPEAKER ********************* */

/** #Speaker::flag */
enum {
  SPK_DS_EXPAND = 1 << 0,
  SPK_MUTED = 1 << 1,
  // SPK_RELATIVE = 1 << 2, /* UNUSED */
};

#ifdef __cplusplus
}
#endif
