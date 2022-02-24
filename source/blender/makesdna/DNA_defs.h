/* SPDX-License-Identifier: GPL-2.0-or-later */

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
