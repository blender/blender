/* SPDX-FileCopyrightText: 2001-2002 NaN Holding BV. All rights reserved.
 *
 * SPDX-License-Identifier: GPL-2.0-or-later */

/** \file
 * \ingroup avi
 *
 * This is external code. Sets some compression related options
 * (width, height quality, frame-rate).
 */

#include "AVI_avi.h"
#include "BLI_fileops.h"
#include "avi_endian.h"
#include "avi_intern.h"

#ifdef WIN32
#  include "BLI_winstuff.h"
#endif

/* avi_set_compress_options gets its own file... now don't WE feel important? */

AviError AVI_set_compress_option(
    AviMovie *movie, int option_type, int stream, AviOption option, void *opt_data)
{
  int i;
  int useconds;

  (void)stream; /* unused */

  if (movie->header->TotalFrames != 0) {
    /* Can't change parameters after we have already started writing frames. */
    return AVI_ERROR_OPTION;
  }

  switch (option_type) {
    case AVI_OPTION_TYPE_MAIN:
      switch (option) {
        case AVI_OPTION_WIDTH:
          movie->header->Width = *((int *)opt_data);
          movie->header->SuggestedBufferSize = movie->header->Width * movie->header->Height * 3;

          for (i = 0; i < movie->header->Streams; i++) {
            if (avi_get_format_type(movie->streams[i].format) == FCC("vids")) {
              ((AviBitmapInfoHeader *)movie->streams[i].sf)->Width = *((int *)opt_data);
              movie->streams[i].sh.SuggestedBufferSize = movie->header->SuggestedBufferSize;
              movie->streams[i].sh.right = *((int *)opt_data);
              ((AviBitmapInfoHeader *)movie->streams[i].sf)->SizeImage =
                  movie->header->SuggestedBufferSize;
              BLI_fseek(movie->fp, movie->offset_table[1 + i * 2 + 1], SEEK_SET);
              awrite(movie,
                     movie->streams[i].sf,
                     1,
                     movie->streams[i].sf_size,
                     movie->fp,
                     AVI_BITMAPH);
            }
          }

          break;

        case AVI_OPTION_HEIGHT:
          movie->header->Height = *((int *)opt_data);
          movie->header->SuggestedBufferSize = movie->header->Width * movie->header->Height * 3;

          for (i = 0; i < movie->header->Streams; i++) {
            if (avi_get_format_type(movie->streams[i].format) == FCC("vids")) {
              ((AviBitmapInfoHeader *)movie->streams[i].sf)->Height = *((int *)opt_data);
              movie->streams[i].sh.SuggestedBufferSize = movie->header->SuggestedBufferSize;
              movie->streams[i].sh.bottom = *((int *)opt_data);
              ((AviBitmapInfoHeader *)movie->streams[i].sf)->SizeImage =
                  movie->header->SuggestedBufferSize;
              BLI_fseek(movie->fp, movie->offset_table[1 + i * 2 + 1], SEEK_SET);
              awrite(movie,
                     movie->streams[i].sf,
                     1,
                     movie->streams[i].sf_size,
                     movie->fp,
                     AVI_BITMAPH);
            }
          }

          break;

        case AVI_OPTION_QUALITY:
          for (i = 0; i < movie->header->Streams; i++) {
            if (avi_get_format_type(movie->streams[i].format) == FCC("vids")) {
              movie->streams[i].sh.Quality = *((int *)opt_data) * 100;
              BLI_fseek(movie->fp, movie->offset_table[1 + i * 2 + 1], SEEK_SET);
              awrite(movie,
                     movie->streams[i].sf,
                     1,
                     movie->streams[i].sf_size,
                     movie->fp,
                     AVI_BITMAPH);
            }
          }
          break;

        case AVI_OPTION_FRAMERATE:
          useconds = (int)(1000000 / *((double *)opt_data));
          if (useconds) {
            movie->header->MicroSecPerFrame = useconds;
          }

          for (i = 0; i < movie->header->Streams; i++) {
            if (avi_get_format_type(movie->streams[i].format) == FCC("vids")) {
              movie->streams[i].sh.Scale = movie->header->MicroSecPerFrame;
              BLI_fseek(movie->fp, movie->offset_table[1 + i * 2 + 1], SEEK_SET);
              awrite(movie,
                     movie->streams[i].sf,
                     1,
                     movie->streams[i].sf_size,
                     movie->fp,
                     AVI_BITMAPH);
            }
          }
          break;
      }

      BLI_fseek(movie->fp, movie->offset_table[0], SEEK_SET);
      awrite(movie, movie->header, 1, sizeof(AviMainHeader), movie->fp, AVI_MAINH);

      break;
    case AVI_OPTION_TYPE_STRH:
      break;
    case AVI_OPTION_TYPE_STRF:
      break;
    default:
      return AVI_ERROR_OPTION;
  }

  return AVI_ERROR_NONE;
}
