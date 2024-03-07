/* SPDX-FileCopyrightText: 2024 Blender Authors
 *
 * SPDX-License-Identifier: GPL-2.0-or-later */

/** \file
 * \ingroup DNA
 */

#pragma once

#include <math.h>

/* Struct members on own line. */
/* clang-format off */

/* -------------------------------------------------------------------- */
/** \name AnimationLayer Struct
 * \{ */

#define _DNA_DEFAULT_AnimationLayer \
  { \
    .influence = 1.0f, \
  }

/** \} */

/* -------------------------------------------------------------------- */
/** \name AnimationStrip Struct
 * \{ */

#define _DNA_DEFAULT_AnimationStrip \
  { \
    .frame_start = -INFINITY, \
    .frame_end = INFINITY, \
  }

/** \} */

/* clang-format on */
