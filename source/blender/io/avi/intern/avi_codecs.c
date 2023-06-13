/* SPDX-FileCopyrightText: 2001-2002 NaN Holding BV. All rights reserved.
 *
 * SPDX-License-Identifier: GPL-2.0-or-later */

/** \file
 * \ingroup avi
 *
 * This is external code. Identify and convert different avi-files.
 */

#include "AVI_avi.h"
#include "avi_intern.h"

#include "avi_mjpeg.h"
#include "avi_rgb.h"
#include "avi_rgb32.h"

#include "BLI_string.h"

void *avi_format_convert(
    AviMovie *movie, int stream, void *buffer, AviFormat from, AviFormat to, size_t *size)
{
  if (from == to) {
    return buffer;
  }

  if (from != AVI_FORMAT_RGB24 && to != AVI_FORMAT_RGB24) {
    return avi_format_convert(
        movie,
        stream,
        avi_format_convert(movie, stream, buffer, from, AVI_FORMAT_RGB24, size),
        AVI_FORMAT_RGB24,
        to,
        size);
  }

  switch (to) {
    case AVI_FORMAT_RGB24:
      switch (from) {
        case AVI_FORMAT_AVI_RGB:
          buffer = avi_converter_from_avi_rgb(movie, stream, buffer, size);
          break;
        case AVI_FORMAT_MJPEG:
          buffer = avi_converter_from_mjpeg(movie, stream, buffer, size);
          break;
        case AVI_FORMAT_RGB32:
          buffer = avi_converter_from_rgb32(movie, stream, buffer, size);
          break;
        default:
          break;
      }
      break;
    case AVI_FORMAT_AVI_RGB:
      buffer = avi_converter_to_avi_rgb(movie, stream, buffer, size);
      break;
    case AVI_FORMAT_MJPEG:
      buffer = avi_converter_to_mjpeg(movie, stream, buffer, size);
      break;
    case AVI_FORMAT_RGB32:
      buffer = avi_converter_to_rgb32(movie, stream, buffer, size);
      break;
    default:
      break;
  }

  return buffer;
}

int avi_get_data_id(AviFormat format, int stream)
{
  char fcc[5];

  if (avi_get_format_type(format) == FCC("vids")) {
    SNPRINTF(fcc, "%2.2ddc", stream);
  }
  else if (avi_get_format_type(format) == FCC("auds")) {
    SNPRINTF(fcc, "%2.2ddc", stream);
  }
  else {
    return 0;
  }

  return FCC(fcc);
}

int avi_get_format_type(AviFormat format)
{
  switch (format) {
    case AVI_FORMAT_RGB24:
    case AVI_FORMAT_RGB32:
    case AVI_FORMAT_AVI_RGB:
    case AVI_FORMAT_MJPEG:
      return FCC("vids");
    default:
      return 0;
  }
}

int avi_get_format_fcc(AviFormat format)
{
  switch (format) {
    case AVI_FORMAT_RGB24:
    case AVI_FORMAT_RGB32:
    case AVI_FORMAT_AVI_RGB:
      return FCC("DIB ");
    case AVI_FORMAT_MJPEG:
      return FCC("MJPG");
    default:
      return 0;
  }
}

int avi_get_format_compression(AviFormat format)
{
  switch (format) {
    case AVI_FORMAT_RGB24:
    case AVI_FORMAT_RGB32:
    case AVI_FORMAT_AVI_RGB:
      return 0;
    case AVI_FORMAT_MJPEG:
      return FCC("MJPG");
    default:
      return 0;
  }
}
