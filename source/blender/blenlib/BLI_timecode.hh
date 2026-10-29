/* SPDX-FileCopyrightText: 2008 Blender Authors
 *
 * SPDX-License-Identifier: GPL-2.0-or-later */

#pragma once

/** \file
 * \ingroup bli
 */

#include <stddef.h>

#include "BLI_compiler_attrs.hh"
#include "BLI_span.hh"

namespace blender {

/**
 * Generate time-code/frame number string and store in \a str
 *
 * \param str: destination string
 * \param brevity_level: special setting for #View2D grid drawing,
 *        used to specify how detailed we need to be
 * \param time_seconds: time total time in seconds
 * \param fps: frames per second
 * \param timecode_style: enum from #eTimecodeStyles
 * \return length of \a str
 */
size_t BLI_timecode_string_from_time(
    MutableCStr str, int brevity_level, float time_seconds, double fps, short timecode_style);

/**
 * Generate time string and store in \a str
 *
 * \param str: destination string
 * \param time_seconds: time total time in seconds
 * \return length of \a str
 */
size_t BLI_timecode_string_from_time_simple(MutableCStr str, double time_seconds);

/**
 * Generate time string and store in \a str
 *
 * \param str: destination string
 * \param brevity_level: special setting for #View2D grid drawing,
 *        used to specify how detailed we need to be
 * \param time_seconds: time total time in seconds
 * \return length of \a str
 *
 * \note in some cases this is used to print non-seconds values.
 */
size_t BLI_timecode_string_from_time_seconds(MutableCStr str,
                                             int brevity_level,
                                             float time_seconds);

}  // namespace blender
