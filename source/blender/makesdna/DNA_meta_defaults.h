/* SPDX-FileCopyrightText: 2023 Blender Authors
 *
 * SPDX-License-Identifier: GPL-2.0-or-later */

/** \file
 * \ingroup DNA
 */

#pragma once

/* Struct members on own line. */
/* clang-format off */

/* -------------------------------------------------------------------- */
/** \name MetaBall Struct
 * \{ */

#define _DNA_DEFAULT_MetaBall \
  { \
    .texspace_size = {1, 1, 1}, \
    .texspace_flag = MB_TEXSPACE_FLAG_AUTO, \
    .wiresize = 0.4f, \
    .rendersize = 0.2f, \
    .thresh = 0.6f, \
  }

/** \} */

/* clang-format on */
