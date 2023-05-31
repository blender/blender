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
/** \name Generic Defaults
 * \{ */

/** See #unit_m4. */
#define _DNA_DEFAULT_UNIT_M4 \
  { \
    {1, 0, 0, 0}, \
    {0, 1, 0, 0}, \
    {0, 0, 1, 0}, \
    {0, 0, 0, 1}, \
  }

#define _DNA_DEFAULT_UNIT_M3 \
  { \
    {1, 0, 0}, \
    {0, 1, 0}, \
    {0, 0, 1}, \
  }

/** See #unit_qt. */
#define _DNA_DEFAULT_UNIT_QT \
  {1, 0, 0, 0}

/** \} */

/* clang-format on */
