/*
 * ***** BEGIN GPL LICENSE BLOCK *****
 *
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
 *
 * The Original Code is Copyright (C) 2008 Blender Foundation.
 * All rights reserved.
 *
 * 
 * Contributor(s): Joshua Leung
 *
 * ***** END GPL LICENSE BLOCK *****
 */

#ifndef __BLI_TIMECODE_H__
#define __BLI_TIMECODE_H__

/** \file BLI_timecode.h
 *  \ingroup BLI
 */

#include "BLI_compiler_attrs.h"

size_t BLI_timecode_string_from_time(
        char *str, const size_t len, const int power, const float time_seconds,
        const double scene_fps, const short timecode_style)
        ATTR_NONNULL();

size_t BLI_timecode_string_from_time_simple(
        char *str, const size_t maxlen, const double time_seconds)
        ATTR_NONNULL();

size_t BLI_timecode_string_from_time_seconds(
        char *str, const size_t len, const int power, const float time_seconds)
        ATTR_NONNULL();

#endif  /* __BLI_TIMECODE_H__ */
