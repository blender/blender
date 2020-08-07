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
