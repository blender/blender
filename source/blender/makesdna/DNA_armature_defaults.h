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
/** \name bArmature Struct
 * \{ */

#define _DNA_DEFAULT_bArmature \
  { \
    .deformflag = ARM_DEF_VGROUP | ARM_DEF_ENVELOPE, \
    .flag = ARM_COL_CUSTOM,  /* custom bone-group colors */ \
    .layer = 1, \
    .drawtype = ARM_OCTA, \
  }

/** \} */

/* clang-format on */
