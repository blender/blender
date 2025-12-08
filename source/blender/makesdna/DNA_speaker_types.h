/* SPDX-FileCopyrightText: 2023 Blender Authors
 *
 * SPDX-License-Identifier: GPL-2.0-or-later */

/** \file
 * \ingroup DNA
 */

#pragma once

#include <cfloat>

#include "DNA_ID.h"

struct AnimData;
struct bSound;

/* **************** SPEAKER ********************* */

/** #Speaker::flag */
enum {
  SPK_DS_EXPAND = 1 << 0,
  SPK_MUTED = 1 << 1,
  // SPK_RELATIVE = 1 << 2, /* UNUSED */
};

struct Speaker {
#ifdef __cplusplus
  /** See #ID_Type comment for why this is here. */
  static constexpr ID_Type id_type = ID_SPK;
#endif

  ID id;
  /** Animation data (must be immediately after id for utilities to use it). */
  struct AnimData *adt = nullptr;

  struct bSound *sound = nullptr;

  /* not animatable properties */
  float volume_max = 1.0f;
  float volume_min = 0.0f;
  float distance_max = FLT_MAX;
  float distance_reference = 1.0f;
  float attenuation = 1.0f;
  float cone_angle_outer = 360.0f;
  float cone_angle_inner = 360.0f;
  float cone_volume_outer = 1.0f;

  /* animatable properties */
  float volume = 1.0f;
  float pitch = 1.0f;

  /* flag */
  short flag = 0;
  char _pad1[6] = {};
};
