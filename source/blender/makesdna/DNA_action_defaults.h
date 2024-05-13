/* SPDX-FileCopyrightText: 2024 Blender Authors
 *
 * SPDX-License-Identifier: GPL-2.0-or-later */

/** \file
 * \ingroup DNA
 */

#pragma once

#include <math.h>

/* clang-format off */

/* -------------------------------------------------------------------- */
/** \name ActionLayer Struct
 * \{ */

#define _DNA_DEFAULT_ActionLayer \
  { \
    .influence = 1.0f, \
  }

/** \} */

/* -------------------------------------------------------------------- */
/** \name ActionStrip Struct
 * \{ */

#define _DNA_DEFAULT_ActionStrip \
  { \
    .frame_start = -INFINITY, \
    .frame_end = INFINITY, \
  }

/** \} */

/* clang-format on */
