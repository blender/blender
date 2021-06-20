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

#pragma once

/** \file
 * \ingroup bli
 */

/* NOTE: these names are historic and could use a more generic prefix.
 * This could be done as part of a bigger refactor. */

/** ENDIAN_ORDER: indicates what endianness the platform where the file was written had. */
#if !defined(__BIG_ENDIAN__) && !defined(__LITTLE_ENDIAN__)
#  error Either __BIG_ENDIAN__ or __LITTLE_ENDIAN__ must be defined.
#endif

#define L_ENDIAN 1
#define B_ENDIAN 0

#ifdef __BIG_ENDIAN__
#  define ENDIAN_ORDER B_ENDIAN
#else
#  define ENDIAN_ORDER L_ENDIAN
#endif
