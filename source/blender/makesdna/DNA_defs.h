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
 *
 * Group generic defines for all DNA headers may use in this file.
 */

#pragma once

/* makesdna ignores */
#ifdef DNA_DEPRECATED_ALLOW
/* allow use of deprecated items */
#  define DNA_DEPRECATED
#else
#  ifndef DNA_DEPRECATED
#    ifdef __GNUC__
#      define DNA_DEPRECATED __attribute__((deprecated))
#    else
/* TODO: MSVC & others. */
#      define DNA_DEPRECATED
#    endif
#  endif
#endif

#ifdef __GNUC__
#  define DNA_PRIVATE_ATTR __attribute__((deprecated))
#else
#  define DNA_PRIVATE_ATTR
#endif

/* poison pragma */
#ifdef DNA_DEPRECATED_ALLOW
#  define DNA_DEPRECATED_GCC_POISON 0
#else
/* enable the pragma if we can */
#  ifdef __GNUC__
#    define DNA_DEPRECATED_GCC_POISON 1
#  else
#    define DNA_DEPRECATED_GCC_POISON 0
#  endif
#endif

/* hrmf, we need a better include then this */
#include "../blenlib/BLI_sys_types.h" /* needed for int64_t only! */

/* non-id name variables should use this length */
#define MAX_NAME 64
