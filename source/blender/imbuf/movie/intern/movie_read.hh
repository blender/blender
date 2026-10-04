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

struct AVFormatContext;
struct AVCodecContext;
struct AVCodec;
struct AVFrame;
struct AVPacket;
struct SwsContext;

#ifdef WITH_FFMPEG

extern "C" {
#  include <libavutil/pixfmt.h>
#  include <libavutil/rational.h>
}

#endif

namespace blender {

struct IDProperty;

struct MovieReader {
  enum class State { Uninitialized, Failed, Valid };
  ImBufFlags ib_flags = ImBufFlags::Zero;
  State state = State::Uninitialized;
  int cur_frame_index = 0; /* index  0 = 1e,  1 = 2e, enz. */
  int duration_in_frames = 0;
  int frs_sec = 0;
  double frs_sec_base = 0.0;
  double start_offset = 0.0;
  int x = 0;
  int y = 0;
  int video_rotation = 0;

  /* for number */
  char filepath[/*FILE_MAX*/ 1024] = {};

  int streamindex = 0;

  bool keep_original_colorspace = false;

#ifdef WITH_FFMPEG
  AVFormatContext *format_ctx = nullptr;
  AVCodecContext *codec_ctx = nullptr;
  const AVCodec *codec = nullptr;
  AVFrame *frame_rgb = nullptr;
  AVFrame *frame_deinterlaced = nullptr;
  SwsContext *sws_ctx = nullptr;
  int video_stream_index = 0;

  AVFrame *frame = nullptr;
  bool frame_complete = false;
  AVFrame *frame_backup = nullptr;
  bool frame_backup_complete = false;

  AVFrame *frame_sw = nullptr;
  AVPixelFormat src_pix_fmt = AV_PIX_FMT_NONE;

  int64_t cur_pts = 0;
  int64_t cur_key_frame_pts = 0;
  AVPacket *cur_packet = nullptr;

  AVRational frame_rate = {1, 1};

  bool is_float = false;

  /* When set, never seek within the video, and only ever decode one frame.
   * This is a workaround for some Ogg files that have full audio but only
   * one frame of "album art" as a video stream in non-Theora format.
   * ffmpeg crashes/aborts when trying to seek within them
   * (https://trac.ffmpeg.org/ticket/10755). */
  bool never_seek_decode_one_frame = false;
#endif

  char proxy_dir[768] = {};

  int proxies_tried = 0;

  MovieReader *proxy_anim[IMB_PROXY_MAX_SLOT] = {};

  char colorspace[/*MAX_COLORSPACE_NAME*/ 64] = {};
  /** The maximum name from multi-view. */
  char suffix[/*MAX_NAME*/ 64] = {};

  IDProperty *metadata = nullptr;
};

}  // namespace blender
