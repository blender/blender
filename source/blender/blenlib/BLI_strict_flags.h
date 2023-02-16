/* SPDX-License-Identifier: GPL-2.0-or-later */

#pragma once

/** \file
 * \ingroup bli
 * \brief Strict compiler flags for areas of code we want
 * to ensure don't do conversions without us knowing about it.
 */

#ifdef __GNUC__
/* NOTE(@ideasman42): CLANG behaves slightly differently to GCC,
 * these can be enabled but do so carefully as they can introduce build-errors.  */
#  if !defined(__clang__)
#    pragma GCC diagnostic error "-Wsign-compare"
#    pragma GCC diagnostic error "-Wconversion"
#    pragma GCC diagnostic error "-Wshadow"
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
