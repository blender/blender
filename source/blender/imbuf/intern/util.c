/*
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
 * The Original Code is Copyright (C) 2001-2002 by NaN Holding BV.
 * All rights reserved.
 * util.c
 */

/** \file
 * \ingroup imbuf
 */

#ifdef _WIN32
#  include <io.h>
#endif

#include <stdlib.h>

#include "BLI_fileops.h"
#include "BLI_path_util.h"
#include "BLI_utildefines.h"
#ifdef _WIN32
#  include "BLI_winstuff.h"
#endif

#include "IMB_filetype.h"
#include "IMB_imbuf.h"
#include "IMB_imbuf_types.h"
#include "imbuf.h"

#include "IMB_anim.h"

#ifdef WITH_FFMPEG
#  include "BLI_string.h" /* BLI_vsnprintf */

#  include "BKE_global.h" /* G.debug */

#  include <libavcodec/avcodec.h>
#  include <libavdevice/avdevice.h>
#  include <libavformat/avformat.h>
#  include <libavutil/log.h>

#  include "ffmpeg_compat.h"
#endif

#define UTIL_DEBUG 0

const char *imb_ext_image[] = {
    ".png", ".tga",  ".bmp", ".jpg", ".jpeg", ".sgi", ".rgb", ".rgba",
#ifdef WITH_TIFF
    ".tif", ".tiff", ".tx",
#endif
#ifdef WITH_OPENJPEG
    ".jp2", ".j2c",
#endif
#ifdef WITH_HDR
    ".hdr",
#endif
#ifdef WITH_DDS
    ".dds",
#endif
#ifdef WITH_CINEON
    ".dpx", ".cin",
#endif
#ifdef WITH_OPENEXR
    ".exr",
#endif
#ifdef WITH_OPENIMAGEIO
    ".psd", ".pdd",  ".psb",
#endif
    NULL,
};

const char *imb_ext_image_filepath_only[] = {
#ifdef WITH_OPENIMAGEIO
    ".psd",
    ".pdd",
    ".psb",
#endif
    NULL,
};

const char *imb_ext_movie[] = {
    ".avi",  ".flc", ".mov", ".movie", ".mp4",  ".m4v",  ".m2v", ".m2t",  ".m2ts", ".mts",
    ".ts",   ".mv",  ".avs", ".wmv",   ".ogv",  ".ogg",  ".r3d", ".dv",   ".mpeg", ".mpg",
    ".mpg2", ".vob", ".mkv", ".flv",   ".divx", ".xvid", ".mxf", ".webm", NULL,
};

/** Sort of wrong having audio extensions in imbuf. */
const char *imb_ext_audio[] = {
    ".wav",
    ".ogg",
    ".oga",
    ".mp3",
    ".mp2",
    ".ac3",
    ".aac",
    ".flac",
    ".wma",
    ".eac3",
    ".aif",
    ".aiff",
    ".m4a",
    ".mka",
    NULL,
};

/* Increased from 32 to 64 because of the bitmaps header size. */
#define HEADER_SIZE 64

static ssize_t imb_ispic_read_header_from_filepath(const char *filepath,
                                                   unsigned char buf[HEADER_SIZE])
{
  BLI_stat_t st;
  int fp;

  BLI_assert(!BLI_path_is_rel(filepath));

  if (UTIL_DEBUG) {
    printf("%s: loading %s\n", __func__, filepath);
  }

  if (BLI_stat(filepath, &st) == -1) {
    return -1;
  }
  if (((st.st_mode) & S_IFMT) != S_IFREG) {
    return -1;
  }

  if ((fp = BLI_open(filepath, O_BINARY | O_RDONLY, 0)) == -1) {
    return -1;
  }

  const ssize_t size = read(fp, buf, HEADER_SIZE);

  close(fp);
  return size;
}

int IMB_ispic_type_from_memory(const unsigned char *buf, const size_t buf_size)
{
  for (const ImFileType *type = IMB_FILE_TYPES; type < IMB_FILE_TYPES_LAST; type++) {
    if (type->is_a != NULL) {
      if (type->is_a(buf, buf_size)) {
        return type->filetype;
      }
    }
  }

  return IMB_FTYPE_NONE;
}

int IMB_ispic_type(const char *filepath)
{
  unsigned char buf[HEADER_SIZE];
  const ssize_t buf_size = imb_ispic_read_header_from_filepath(filepath, buf);
  if (buf_size <= 0) {
    return IMB_FTYPE_NONE;
  }
  return IMB_ispic_type_from_memory(buf, (size_t)buf_size);
}

bool IMB_ispic_type_matches(const char *filepath, int filetype)
{
  unsigned char buf[HEADER_SIZE];
  const ssize_t buf_size = imb_ispic_read_header_from_filepath(filepath, buf);
  if (buf_size <= 0) {
    return false;
  }

  const ImFileType *type = IMB_file_type_from_ftype(filetype);
  if (type != NULL) {
    /* Requesting to load a type that can't check its own header doesn't make sense.
     * Keep the check for developers. */
    BLI_assert(type->is_a != NULL);
    if (type->is_a != NULL) {
      return type->is_a(buf, (size_t)buf_size);
    }
  }
  return false;
}

#undef HEADER_SIZE

bool IMB_ispic(const char *filepath)
{
  return (IMB_ispic_type(filepath) != IMB_FTYPE_NONE);
}

static bool isavi(const char *filepath)
{
#ifdef WITH_AVI
  return AVI_is_avi(filepath);
#else
  (void)filepath;
  return false;
#endif
}

#ifdef WITH_FFMPEG

/* BLI_vsnprintf in ffmpeg_log_callback() causes invalid warning */
#  ifdef __GNUC__
#    pragma GCC diagnostic push
#    pragma GCC diagnostic ignored "-Wmissing-format-attribute"
#  endif

static char ffmpeg_last_error[1024];

static void ffmpeg_log_callback(void *ptr, int level, const char *format, va_list arg)
{
  if (ELEM(level, AV_LOG_FATAL, AV_LOG_ERROR)) {
    size_t n;
    va_list args_cpy;

    va_copy(args_cpy, arg);
    n = BLI_vsnprintf(ffmpeg_last_error, sizeof(ffmpeg_last_error), format, args_cpy);
    va_end(args_cpy);

    /* strip trailing \n */
    ffmpeg_last_error[n - 1] = '\0';
  }

  if (G.debug & G_DEBUG_FFMPEG) {
    /* call default logger to print all message to console */
    av_log_default_callback(ptr, level, format, arg);
  }
}

#  ifdef __GNUC__
#    pragma GCC diagnostic pop
#  endif

void IMB_ffmpeg_init(void)
{
  avdevice_register_all();

  ffmpeg_last_error[0] = '\0';

  if (G.debug & G_DEBUG_FFMPEG) {
    av_log_set_level(AV_LOG_DEBUG);
  }

  /* set own callback which could store last error to report to UI */
  av_log_set_callback(ffmpeg_log_callback);
}

const char *IMB_ffmpeg_last_error(void)
{
  return ffmpeg_last_error;
}

static int isffmpeg(const char *filepath)
{
  AVFormatContext *pFormatCtx = NULL;
  unsigned int i;
  int videoStream;
  const AVCodec *pCodec;

  if (BLI_path_extension_check_n(filepath,
                                 ".swf",
                                 ".jpg",
                                 ".jp2",
                                 ".j2c",
                                 ".png",
                                 ".dds",
                                 ".tga",
                                 ".bmp",
                                 ".tif",
                                 ".exr",
                                 ".cin",
                                 ".wav",
                                 NULL)) {
    return 0;
  }

  if (avformat_open_input(&pFormatCtx, filepath, NULL, NULL) != 0) {
    if (UTIL_DEBUG) {
      fprintf(stderr, "isffmpeg: av_open_input_file failed\n");
    }
    return 0;
  }

  if (avformat_find_stream_info(pFormatCtx, NULL) < 0) {
    if (UTIL_DEBUG) {
      fprintf(stderr, "isffmpeg: avformat_find_stream_info failed\n");
    }
    avformat_close_input(&pFormatCtx);
    return 0;
  }

  if (UTIL_DEBUG) {
    av_dump_format(pFormatCtx, 0, filepath, 0);
  }

  /* Find the first video stream */
  videoStream = -1;
  for (i = 0; i < pFormatCtx->nb_streams; i++) {
    if (pFormatCtx->streams[i] && pFormatCtx->streams[i]->codecpar &&
        (pFormatCtx->streams[i]->codecpar->codec_type == AVMEDIA_TYPE_VIDEO)) {
      videoStream = i;
      break;
    }
  }

  if (videoStream == -1) {
    avformat_close_input(&pFormatCtx);
    return 0;
  }

  AVCodecParameters *codec_par = pFormatCtx->streams[videoStream]->codecpar;

  /* Find the decoder for the video stream */
  pCodec = avcodec_find_decoder(codec_par->codec_id);
  if (pCodec == NULL) {
    avformat_close_input(&pFormatCtx);
    return 0;
  }

  avformat_close_input(&pFormatCtx);

  return 1;
}
#endif

int imb_get_anim_type(const char *filepath)
{
  BLI_stat_t st;

  BLI_assert(!BLI_path_is_rel(filepath));

  if (UTIL_DEBUG) {
    printf("%s: %s\n", __func__, filepath);
  }

#ifndef _WIN32
#  ifdef WITH_FFMPEG
  /* stat test below fails on large files > 4GB */
  if (isffmpeg(filepath)) {
    return ANIM_FFMPEG;
  }
#  endif
  if (BLI_stat(filepath, &st) == -1) {
    return 0;
  }
  if (((st.st_mode) & S_IFMT) != S_IFREG) {
    return 0;
  }

  if (isavi(filepath)) {
    return ANIM_AVI;
  }

  if (ismovie(filepath)) {
    return ANIM_MOVIE;
  }
#else /* !_WIN32 */
  if (BLI_stat(filepath, &st) == -1) {
    return 0;
  }
  if (((st.st_mode) & S_IFMT) != S_IFREG) {
    return 0;
  }

  if (ismovie(filepath)) {
    return ANIM_MOVIE;
  }
#  ifdef WITH_FFMPEG
  if (isffmpeg(filepath)) {
    return ANIM_FFMPEG;
  }
#  endif

  if (isavi(filepath)) {
    return ANIM_AVI;
  }
#endif /* !_WIN32 */

  /* Assume a single image is part of an image sequence. */
  if (IMB_ispic(filepath)) {
    return ANIM_SEQUENCE;
  }

  return ANIM_NONE;
}

bool IMB_isanim(const char *filepath)
{
  int type;

  type = imb_get_anim_type(filepath);

  return (type && type != ANIM_SEQUENCE);
}

bool IMB_isfloat(const ImBuf *ibuf)
{
  const ImFileType *type = IMB_file_type_from_ibuf(ibuf);
  if (type != NULL) {
    if (type->flag & IM_FTYPE_FLOAT) {
      return true;
    }
  }
  return false;
}
