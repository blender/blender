/* SPDX-FileCopyrightText: 2001-2002 NaN Holding BV. All rights reserved.
 *
 * SPDX-License-Identifier: GPL-2.0-or-later */

/** \file
 * \ingroup imbuf
 */

#pragma once

#include <cstdint>

#include "IMB_imbuf_enums.h"

#ifdef WITH_FFMPEG
struct AVFormatContext;
struct AVCodecContext;
struct AVCodec;
struct AVFrame;
struct AVPacket;
struct SwsContext;
#endif

struct IDProperty;
struct ImBufAnimIndex;

struct ImBufAnim {
  enum class State { Uninitialized, Failed, Valid };
  int ib_flags;
  State state;
  int cur_position; /* index  0 = 1e,  1 = 2e, enz. */
  int duration_in_frames;
  int frs_sec;
  double frs_sec_base;
  double start_offset;
  int x, y;

  /* for number */
  char filepath[1024];

  int streamindex;

#ifdef WITH_FFMPEG
  AVFormatContext *pFormatCtx;
  AVCodecContext *pCodecCtx;
  const AVCodec *pCodec;
  AVFrame *pFrameRGB;
  AVFrame *pFrameDeinterlaced;
  SwsContext *img_convert_ctx;
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

  ImBufAnim *proxy_anim[IMB_PROXY_MAX_SLOT];
  ImBufAnimIndex *record_run;
  ImBufAnimIndex *no_gaps;

  char colorspace[64];
  char suffix[64]; /* MAX_NAME - multiview */

  IDProperty *metadata;
};
