/* SPDX-FileCopyrightText: 2001-2002 NaN Holding BV. All rights reserved.
 * SPDX-FileCopyrightText: 2024-2025 Blender Authors
 *
 * SPDX-License-Identifier: GPL-2.0-or-later */

/** \file
 * \ingroup imbuf
 */

#pragma once

#include <cstdint>

#include "IMB_imbuf_enums.h"

#ifdef WITH_FFMPEG

extern "C" {
#  include <libavutil/rational.h>
}

struct AVFormatContext;
struct AVCodecContext;
struct AVCodec;
struct AVFrame;
struct AVPacket;
struct SwsContext;
#endif

struct IDProperty;
struct MovieIndex;

struct MovieReader {
  enum class State { Uninitialized, Failed, Valid };
  int ib_flags = 0;
  State state = State::Uninitialized;
  int cur_position = 0; /* index  0 = 1e,  1 = 2e, enz. */
  int duration_in_frames = 0;
  int frs_sec = 0;
  double frs_sec_base = 0.0;
  double start_offset = 0.0;
  int x = 0;
  int y = 0;
  int video_rotation = 0;

  /* for number */
  char filepath[1024] = {};

  int streamindex = 0;

#ifdef WITH_FFMPEG
  AVFormatContext *pFormatCtx = nullptr;
  AVCodecContext *pCodecCtx = nullptr;
  const AVCodec *pCodec = nullptr;
  AVFrame *pFrameRGB = nullptr;
  AVFrame *pFrameDeinterlaced = nullptr;
  SwsContext *img_convert_ctx = nullptr;
  int videoStream = 0;

  AVFrame *pFrame = nullptr;
  bool pFrame_complete = false;
  AVFrame *pFrame_backup = nullptr;
  bool pFrame_backup_complete = false;

  int64_t cur_pts = 0;
  int64_t cur_key_frame_pts = 0;
  AVPacket *cur_packet = nullptr;

  AVRational frame_rate = {1, 1};

  bool seek_before_decode = false;
  bool is_float = false;

  /* When set, never seek within the video, and only ever decode one frame.
   * This is a workaround for some Ogg files that have full audio but only
   * one frame of "album art" as a video stream in non-Theora format.
   * ffmpeg crashes/aborts when trying to seek within them
   * (https://trac.ffmpeg.org/ticket/10755). */
  bool never_seek_decode_one_frame = false;
#endif

  char index_dir[768] = {};

  int proxies_tried = 0;
  int indices_tried = 0;

  MovieReader *proxy_anim[IMB_PROXY_MAX_SLOT] = {};
  MovieIndex *record_run = nullptr;
  MovieIndex *no_gaps = nullptr;

  char colorspace[64] = {};
  char suffix[64] = {}; /* MAX_NAME - multiview */

  IDProperty *metadata = nullptr;
};
