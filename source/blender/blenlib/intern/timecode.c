/* SPDX-License-Identifier: GPL-2.0-or-later
 * Copyright 2008 Blender Foundation */

/** \file
 * \ingroup bli
 *
 * Time-Code string formatting
 */

#include <stdio.h>

#include "BLI_math.h"
#include "BLI_string.h"
#include "BLI_utildefines.h"

#include "BLI_timecode.h" /* own include */

#include "DNA_userdef_types.h" /* for eTimecodeStyles only */

#include "BLI_strict_flags.h"

size_t BLI_timecode_string_from_time(char *str,
                                     const size_t maxncpy,
                                     const int brevity_level,
                                     const float time_seconds,
                                     const double fps,
                                     const short timecode_style)
{
  int hours = 0, minutes = 0, seconds = 0, frames = 0;
  float time = time_seconds;
  char neg[2] = {'\0'};
  size_t rlen;

  /* get cframes */
  if (time < 0) {
    /* Correction for negative cframes. */
    neg[0] = '-';
    time = -time;
  }

  if (time >= 3600.0f) {
    /* hours */
    /* XXX should we only display a single digit for hours since clips are
     *     VERY UNLIKELY to be more than 1-2 hours max? However, that would
     *     go against conventions...
     */
    hours = (int)time / 3600;
    time = fmodf(time, 3600);
  }

  if (time >= 60.0f) {
    /* minutes */
    minutes = (int)time / 60;
    time = fmodf(time, 60);
  }

  if (brevity_level <= 0) {
    /* seconds + frames
     * Frames are derived from 'fraction' of second. We need to perform some additional rounding
     * to cope with 'half' frames, etc., which should be fine in most cases
     */
    seconds = (int)time;
    frames = round_fl_to_int((float)(((double)time - (double)seconds) * fps));
  }
  else {
    /* seconds (with pixel offset rounding) */
    seconds = round_fl_to_int(time);
  }

  switch (timecode_style) {
    case USER_TIMECODE_MINIMAL: {
      /* - In general, minutes and seconds should be shown, as most clips will be
       *   within this length. Hours will only be included if relevant.
       * - Only show frames when zoomed in enough for them to be relevant
       *   (using separator of '+' for frames).
       *   When showing frames, use slightly different display to avoid confusion with mm:ss format
       */
      if (brevity_level <= 0) {
        /* include "frames" in display */
        if (hours) {
          rlen = BLI_snprintf_rlen(
              str, maxncpy, "%s%02d:%02d:%02d+%02d", neg, hours, minutes, seconds, frames);
        }
        else if (minutes) {
          rlen = BLI_snprintf_rlen(
              str, maxncpy, "%s%02d:%02d+%02d", neg, minutes, seconds, frames);
        }
        else {
          rlen = BLI_snprintf_rlen(str, maxncpy, "%s00:%02d+%02d", neg, seconds, frames);
        }
      }
      else {
        /* don't include 'frames' in display */
        if (hours) {
          rlen = BLI_snprintf_rlen(str, maxncpy, "%s%02d:%02d:%02d", neg, hours, minutes, seconds);
        }
        else {
          rlen = BLI_snprintf_rlen(str, maxncpy, "%s%02d:%02d", neg, minutes, seconds);
        }
      }
      break;
    }
    case USER_TIMECODE_SMPTE_MSF: {
      /* reduced SMPTE format that always shows minutes, seconds, frames.
       * Hours only shown as needed. */
      if (hours) {
        rlen = BLI_snprintf_rlen(
            str, maxncpy, "%s%02d:%02d:%02d:%02d", neg, hours, minutes, seconds, frames);
      }
      else {
        rlen = BLI_snprintf_rlen(str, maxncpy, "%s%02d:%02d:%02d", neg, minutes, seconds, frames);
      }
      break;
    }
    case USER_TIMECODE_MILLISECONDS: {
      /* reduced SMPTE. Instead of frames, milliseconds are shown */

      /* precision of decimal part */
      const int ms_dp = (brevity_level <= 0) ? (1 - brevity_level) : 1;

      /* to get 2 digit whole-number part for seconds display
       * (i.e. 3 is for 2 digits + radix, on top of full length) */
      const int s_pad = ms_dp + 3;

      if (hours) {
        rlen = BLI_snprintf_rlen(
            str, maxncpy, "%s%02d:%02d:%0*.*f", neg, hours, minutes, s_pad, ms_dp, time);
      }
      else {
        rlen = BLI_snprintf_rlen(str, maxncpy, "%s%02d:%0*.*f", neg, minutes, s_pad, ms_dp, time);
      }
      break;
    }
    case USER_TIMECODE_SUBRIP: {
      /* SubRip, like SMPTE milliseconds but seconds and milliseconds
       * are separated by a comma, not a dot... */

      /* precision of decimal part */
      const int ms_dp = (brevity_level <= 0) ? (1 - brevity_level) : 1;
      const int ms = round_fl_to_int((time - (float)seconds) * 1000.0f);

      rlen = BLI_snprintf_rlen(
          str, maxncpy, "%s%02d:%02d:%02d,%0*d", neg, hours, minutes, seconds, ms_dp, ms);
      break;
    }
    case USER_TIMECODE_SECONDS_ONLY: {
      /* only show the original seconds display */
      /* round to whole numbers if brevity_level is >= 1 (i.e. scale is coarse) */
      if (brevity_level <= 0) {
        rlen = BLI_snprintf_rlen(str, maxncpy, "%.*f", 1 - brevity_level, time_seconds);
      }
      else {
        rlen = BLI_snprintf_rlen(str, maxncpy, "%d", round_fl_to_int(time_seconds));
      }
      break;
    }
    case USER_TIMECODE_SMPTE_FULL:
    default: {
      /* full SMPTE format */
      rlen = BLI_snprintf_rlen(
          str, maxncpy, "%s%02d:%02d:%02d:%02d", neg, hours, minutes, seconds, frames);
      break;
    }
  }

  return rlen;
}

size_t BLI_timecode_string_from_time_simple(char *str,
                                            const size_t maxncpy,
                                            const double time_seconds)
{
  size_t rlen;

  /* format 00:00:00.00 (hr:min:sec) string has to be 12 long */
  const int hr = ((int)time_seconds) / (60 * 60);
  const int min = (((int)time_seconds) / 60) % 60;
  const int sec = ((int)time_seconds) % 60;
  const int hun = (int)(fmod(time_seconds, 1.0) * 100);

  if (hr) {
    rlen = BLI_snprintf_rlen(str, maxncpy, "%.2d:%.2d:%.2d.%.2d", hr, min, sec, hun);
  }
  else {
    rlen = BLI_snprintf_rlen(str, maxncpy, "%.2d:%.2d.%.2d", min, sec, hun);
  }

  return rlen;
}

size_t BLI_timecode_string_from_time_seconds(char *str,
                                             const size_t maxncpy,
                                             const int brevity_level,
                                             const float time_seconds)
{
  size_t rlen;

  /* round to whole numbers if brevity_level is >= 1 (i.e. scale is coarse) */
  if (brevity_level <= 0) {
    rlen = BLI_snprintf_rlen(str, maxncpy, "%.*f", 1 - brevity_level, time_seconds);
  }
  else {
    rlen = BLI_snprintf_rlen(str, maxncpy, "%d", round_fl_to_int(time_seconds));
  }

  return rlen;
}
