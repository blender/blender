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
/** \name World Struct
 * \{ */

#define _DNA_DEFAULT_World \
  { \
    .horr = 0.05f, \
    .horg = 0.05f, \
    .horb = 0.05f, \
  \
    .aodist = 10.0f, \
    .aoenergy = 1.0f, \
  \
    .preview = NULL, \
    .miststa = 5.0f, \
    .mistdist = 25.0f, \
  }

/** \} */

/* clang-format on */
