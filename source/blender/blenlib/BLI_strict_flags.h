/* SPDX-License-Identifier: GPL-2.0-or-later */

#pragma once

/** \file
 * \ingroup bli
 * \brief Strict compiler flags for areas of code we want
 * to ensure don't do conversions without us knowing about it.
 */

#ifdef __GNUC__
#  if (__GNUC__ * 100 + __GNUC_MINOR__) >= 406 /* gcc4.6+ only */
#    pragma GCC diagnostic error "-Wsign-compare"
#  endif
#  if __GNUC__ >= 6 /* gcc6+ only */
#    pragma GCC diagnostic error "-Wconversion"
#  endif
#  if (__GNUC__ * 100 + __GNUC_MINOR__) >= 408
/* gcc4.8+ only (behavior changed to ignore globals). */
#    pragma GCC diagnostic error "-Wshadow"
/* older gcc changed behavior with ternary */
#    pragma GCC diagnostic error "-Wsign-conversion"
#  endif
/* pedantic gives too many issues, developers can define this for own use */
#  ifdef WARN_PEDANTIC
#    pragma GCC diagnostic error "-Wpedantic"
#    ifdef __clang__ /* pedantic causes clang error */
#      pragma GCC diagnostic ignored "-Wlanguage-extension-token"
#    endif
#  endif
#endif

#ifdef _MSC_VER
#  pragma warning(error : 4018) /* signed/unsigned mismatch */
#  pragma warning(error : 4244) /* conversion from 'type1' to 'type2', possible loss of data */
#  pragma warning(error : 4245) /* conversion from 'int' to 'unsigned int' */
#  pragma warning(error : 4267) /* conversion from 'size_t' to 'type', possible loss of data */
#  pragma warning(error : 4305) /* truncation from 'type1' to 'type2' */
#  pragma warning(error : 4389) /* signed/unsigned mismatch */
#endif
