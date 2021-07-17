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
/** \name Asset Struct
 * \{ */

#define _DNA_DEFAULT_AssetMetaData \
  { \
    0 \
  }

#define _DNA_DEFAULT_AssetLibraryReference \
  { \
    .type = ASSET_LIBRARY_LOCAL, \
    /* Not needed really (should be ignored for #ASSET_LIBRARY_LOCAL), but helps debugging. */ \
    .custom_library_index = -1, \
  }

/** \} */

/* clang-format on */
