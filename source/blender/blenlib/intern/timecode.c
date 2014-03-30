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

/** \file blender/blendlib/intern/timecode.c
 *  \ingroup blendlib
 *
 * Time-Code string formatting
 */

#include <stdio.h>


#include "BLI_string.h"
#include "BLI_math.h"

#include "BLI_timecode.h"  /* own include */

#include "DNA_userdef_types.h"  /* for eTimecodeStyles only */

#include "BLI_strict_flags.h"

/**
 * Generate timecode/frame number string and store in \a str
 *
 * \param str  destination string
 * \param maxncpy  maximum number of characters to copy ``sizeof(str)``
 * \param power  special setting for #View2D grid drawing,
 *        used to specify how detailed we need to be
 * \param time_seconds  time total time in seconds
 * \param fps  frames per second, typically from the #FPS macro
 * \param timecode_style  enum from eTimecodeStyles
 * \return length of \a str
 */

size_t BLI_timecode_string_from_time(
        char *str, const size_t maxncpy, const int power, const float time_seconds,
        const double fps, const short timecode_style)
{
	int hours = 0, minutes = 0, seconds = 0, frames = 0;
	float time = time_seconds;
	char neg[2] = {'\0'};
	size_t rlen;

	/* get cframes */
	if (time < 0) {
		/* correction for negative cfraues */
		neg[0] = '-';
		time = -time;
	}

	if (time >= 3600) {
		/* hours */
		/* XXX should we only display a single digit for hours since clips are
		 *     VERY UNLIKELY to be more than 1-2 hours max? However, that would
		 *     go against conventions...
		 */
		hours = (int)time / 3600;
		time = (float)fmod(time, 3600);
	}

	if (time >= 60) {
		/* minutes */
		minutes = (int)time / 60;
		time = (float)fmod(time, 60);
	}

	if (power <= 0) {
		/* seconds + frames
		 * Frames are derived from 'fraction' of second. We need to perform some additional rounding
		 * to cope with 'half' frames, etc., which should be fine in most cases
		 */
		seconds = (int)time;
		frames = iroundf((float)(((double)time - (double)seconds) * fps));
	}
	else {
		/* seconds (with pixel offset rounding) */
		seconds = iroundf(time);
	}

	switch (timecode_style) {
		case USER_TIMECODE_MINIMAL:
		{
			/* - In general, minutes and seconds should be shown, as most clips will be
			 *   within this length. Hours will only be included if relevant.
			 * - Only show frames when zoomed in enough for them to be relevant
			 *   (using separator of '+' for frames).
			 *   When showing frames, use slightly different display to avoid confusion with mm:ss format
			 */
			if (power <= 0) {
				/* include "frames" in display */
				if (hours) {
					rlen = BLI_snprintf(str, maxncpy, "%s%02d:%02d:%02d+%02d", neg, hours, minutes, seconds, frames);
				}
				else if (minutes) {
					rlen = BLI_snprintf(str, maxncpy, "%s%02d:%02d+%02d", neg, minutes, seconds, frames);
				}
				else {
					rlen = BLI_snprintf(str, maxncpy, "%s%d+%02d", neg, seconds, frames);
				}
			}
			else {
				/* don't include 'frames' in display */
				if (hours) {
					rlen = BLI_snprintf(str, maxncpy, "%s%02d:%02d:%02d", neg, hours, minutes, seconds);
				}
				else {
					rlen = BLI_snprintf(str, maxncpy, "%s%02d:%02d", neg, minutes, seconds);
				}
			}
			break;
		}
		case USER_TIMECODE_SMPTE_MSF:
		{
			/* reduced SMPTE format that always shows minutes, seconds, frames. Hours only shown as needed. */
			if (hours) {
				rlen = BLI_snprintf(str, maxncpy, "%s%02d:%02d:%02d:%02d", neg, hours, minutes, seconds, frames);
			}
			else {
				rlen = BLI_snprintf(str, maxncpy, "%s%02d:%02d:%02d", neg, minutes, seconds, frames);
			}
			break;
		}
		case USER_TIMECODE_MILLISECONDS:
		{
			/* reduced SMPTE. Instead of frames, milliseconds are shown */

			/* precision of decimal part */
			const int ms_dp = (power <= 0) ? (1 - power) : 1;

			/* to get 2 digit whole-number part for seconds display
			 * (i.e. 3 is for 2 digits + radix, on top of full length) */
			const int s_pad = ms_dp + 3;

			if (hours) {
				rlen = BLI_snprintf(str, maxncpy, "%s%02d:%02d:%0*.*f", neg, hours, minutes, s_pad, ms_dp, time);
			}
			else {
				rlen = BLI_snprintf(str, maxncpy, "%s%02d:%0*.*f", neg, minutes, s_pad,  ms_dp, time);
			}
			break;
		}
		case USER_TIMECODE_SECONDS_ONLY:
		{
			/* only show the original seconds display */
			/* round to whole numbers if power is >= 1 (i.e. scale is coarse) */
			if (power <= 0) {
				rlen = BLI_snprintf(str, maxncpy, "%.*f", 1 - power, time_seconds);
			}
			else {
				rlen = BLI_snprintf(str, maxncpy, "%d", iroundf(time_seconds));
			}
			break;
		}
		case USER_TIMECODE_SMPTE_FULL:
		default:
		{
			/* full SMPTE format */
			rlen = BLI_snprintf(str, maxncpy, "%s%02d:%02d:%02d:%02d", neg, hours, minutes, seconds, frames);
			break;
		}
	}

	return rlen;
}


/**
 * Generate time string and store in \a str
 *
 * \param str  destination string
 * \param maxncpy  maximum number of characters to copy ``sizeof(str)``
 * \param power  special setting for #View2D grid drawing,
 *        used to specify how detailed we need to be
 * \param time_seconds  time total time in seconds
 * \param seconds  time in seconds.
 * \return length of \a str
 *
 * \note in some cases this is used to print non-seconds values.
 */
size_t BLI_timecode_string_from_time_simple(
        char *str, const size_t maxncpy, const int power, const float time_seconds)
{
	size_t rlen;

	/* round to whole numbers if power is >= 1 (i.e. scale is coarse) */
	if (power <= 0) {
		rlen = BLI_snprintf(str, maxncpy, "%.*f", 1 - power, time_seconds);
	}
	else {
		rlen = BLI_snprintf(str, maxncpy, "%d", iroundf(time_seconds));
	}

	return rlen;
}
