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
/** \name Action Struct
 * \{ */

/* The last_slot_handle is set to a high value to disambiguate slot handles from
 * array indices.
 *
 * This particular value was obtained by taking the 31 most-significant bits of
 * the TentHash value (Nathan's hash function) for the string "Quercus&Laksa"
 * (Sybren's cats). */
#define DNA_DEFAULT_ACTION_LAST_SLOT_HANDLE 0x37627bf5

#define _DNA_DEFAULT_bAction \
  { \
  .last_slot_handle = DNA_DEFAULT_ACTION_LAST_SLOT_HANDLE, \
  }

/** \} */

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
    .data_index = -1, \
    .frame_start = -INFINITY, \
    .frame_end = INFINITY, \
  }

/** \} */

/* clang-format on */
