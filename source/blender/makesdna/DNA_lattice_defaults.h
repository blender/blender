/* SPDX-FileCopyrightText: 2023 Blender Foundation
 *
 * SPDX-License-Identifier: GPL-2.0-or-later */

/** \file
 * \ingroup DNA
 */

#pragma once

/* Struct members on own line. */
/* clang-format off */

/* -------------------------------------------------------------------- */
/** \name Lattice Struct
 * \{ */

#define _DNA_DEFAULT_Lattice \
  { \
    .flag = LT_GRID, \
    .typeu = KEY_BSPLINE, \
    .typev = KEY_BSPLINE, \
    .typew = KEY_BSPLINE, \
    .actbp = LT_ACTBP_NONE, \
  }

/** \} */

/* clang-format on */
