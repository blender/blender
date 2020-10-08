/*
 * This program is free software; you can redistribute it and/or
 * modify it under the terms of the GNU General Public License
 * as published by the Free Software Foundation; either version 2
 * of the License, or (at your option) any later version.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with this program; if not, write to the Free Software Foundation,
 * Inc., 51 Franklin Street, Fifth Floor, Boston, MA 02110-1301, USA.
 */

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
