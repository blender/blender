/* SPDX-FileCopyrightText: 2008 Blender Authors
 *
 * SPDX-License-Identifier: GPL-2.0-or-later */

#pragma once

/** \file
 * \ingroup bli
 */

#include "BLI_compiler_attrs.h"

#ifdef __cplusplus
extern "C" {
#endif

/**
 * Generate time-code/frame number string and store in \a str
 *
 * \param str: destination string
 * \param maxncpy: maximum number of characters to copy `sizeof(str)`
 * \param brevity_level: special setting for #View2D grid drawing,
 *        used to specify how detailed we need to be
 * \param time_seconds: time total time in seconds
 * \param fps: frames per second, typically from the #FPS macro
 * \param timecode_style: enum from #eTimecodeStyles
 * \return length of \a str
 */
size_t BLI_timecode_string_from_time(char *str,
                                     size_t maxncpy,
                                     int brevity_level,
                                     float time_seconds,
                                     double fps,
                                     short timecode_style) ATTR_NONNULL();

/**
 * Generate time string and store in \a str
 *
 * \param str: destination string
 * \param maxncpy: maximum number of characters to copy `sizeof(str)`
 * \param time_seconds: time total time in seconds
 * \return length of \a str
 */
size_t BLI_timecode_string_from_time_simple(char *str, size_t maxncpy, double time_seconds)
    ATTR_NONNULL();

/**
 * Generate time string and store in \a str
 *
 * \param str: destination string
 * \param maxncpy: maximum number of characters to copy `sizeof(str)`
 * \param brevity_level: special setting for #View2D grid drawing,
 *        used to specify how detailed we need to be
 * \param time_seconds: time total time in seconds
 * \return length of \a str
 *
 * \note in some cases this is used to print non-seconds values.
 */
size_t BLI_timecode_string_from_time_seconds(char *str,
                                             size_t maxncpy,
                                             int brevity_level,
                                             float time_seconds) ATTR_NONNULL();

#ifdef __cplusplus
}
#endif
