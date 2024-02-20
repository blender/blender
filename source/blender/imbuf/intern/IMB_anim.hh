/* SPDX-FileCopyrightText: 2001-2002 NaN Holding BV. All rights reserved.
 *
 * SPDX-License-Identifier: GPL-2.0-or-later */

/** \file
 * \ingroup imbuf
 */

#pragma once

#include <ctype.h>
#include <stdio.h>
#include <stdlib.h>
#include <sys/types.h>

#ifdef _WIN32
#  include <io.h>
#else
#  include <dirent.h>
#endif

#include "imbuf.hh"

#include "IMB_imbuf.hh"
#include "IMB_imbuf_types.hh"

#ifdef WITH_FFMPEG
extern "C" {
#  include <libavcodec/avcodec.h>
#  include <libavformat/avformat.h>
#  include <libswscale/swscale.h>
}
#endif

struct IDProperty;
struct ImBufAnimIndex;

struct ImBufAnim {
  int ib_flags;
  ImbAnimType curtype;
  int cur_position; /* index  0 = 1e,  1 = 2e, enz. */
  int duration_in_frames;
  int frs_sec;
  double frs_sec_base;
  double start_offset;
  int x, y;

  /* for number */
  char filepath[1024];
  /* for sequence */
  char filepath_first[1024];

  /* movie */
  void *movie;
  void *track;
  void *params;
  int orientation;
  size_t framesize;
  int interlacing;
  int streamindex;

#ifdef WITH_FFMPEG
  AVFormatContext *pFormatCtx;
  AVCodecContext *pCodecCtx;
  const AVCodec *pCodec;
  AVFrame *pFrameRGB;
  AVFrame *pFrameDeinterlaced;
  struct SwsContext *img_convert_ctx;
  int videoStream;

  AVFrame *pFrame;
  bool pFrame_complete;
  AVFrame *pFrame_backup;
  bool pFrame_backup_complete;

  int64_t cur_pts;
  int64_t cur_key_frame_pts;
  AVPacket *cur_packet;

  bool seek_before_decode;
#endif

  char index_dir[768];

  int proxies_tried;
  int indices_tried;

  struct ImBufAnim *proxy_anim[IMB_PROXY_MAX_SLOT];
  struct ImBufAnimIndex *curr_idx[IMB_TC_MAX_SLOT];

  char colorspace[64];
  char suffix[64]; /* MAX_NAME - multiview */

  struct IDProperty *metadata;
};
