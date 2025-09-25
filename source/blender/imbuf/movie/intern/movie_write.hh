/* SPDX-FileCopyrightText: 2024 Blender Authors
 *
 * SPDX-License-Identifier: GPL-2.0-or-later */

/** \file
 * \ingroup imbuf
 */

#pragma once

#ifdef WITH_FFMPEG

#  include <cstdint>
/* Note: include cmath before ffmpeg headers, since both of them define
 * M_PI and other macros. This is to avoid warnings about macro redefinition
 * if later including cmath (MSVC 2019). */
#  if defined(_MSC_VER) && !defined(_USE_MATH_DEFINES)
#    define _USE_MATH_DEFINES
#  endif
#  include <cmath>  // IWYU pragma: keep

extern "C" {
#  include <libavcodec/codec_id.h>
#  include <libavformat/avformat.h>
#  include <libavutil/buffer.h>
#  include <libavutil/channel_layout.h>
#  include <libavutil/imgutils.h>
#  include <libavutil/mastering_display_metadata.h>
#  include <libavutil/opt.h>
#  include <libavutil/rational.h>
#  include <libavutil/samplefmt.h>

#  include "ffmpeg_compat.h"
}

#  ifdef WITH_AUDASPACE
#    include <AUD_Types.h>
#  endif

struct Scene;
struct ReportList;
struct StampData;

struct MovieWriter {
  int ffmpeg_type = 0;
  AVCodecID ffmpeg_codec = {};
  AVCodecID ffmpeg_audio_codec = {};
  int ffmpeg_video_bitrate = 0;
  int ffmpeg_audio_bitrate = 0;
  int ffmpeg_gop_size = 0;
  int ffmpeg_max_b_frames = 0;
  int ffmpeg_autosplit_count = 0;
  bool ffmpeg_autosplit = false;
  bool ffmpeg_preview = false;

  int ffmpeg_crf = 0; /* set to 0 to not use CRF mode; we have another flag for lossless anyway. */
  int ffmpeg_preset = 0; /* see eFFMpegPreset */
  int ffmpeg_profile = 0;

  AVFormatContext *outfile = nullptr;
  AVCodecContext *video_codec = nullptr;
  AVCodecContext *audio_codec = nullptr;
  AVStream *video_stream = nullptr;
  AVStream *audio_stream = nullptr;
  AVFrame *current_frame = nullptr; /* Image frame in output pixel format. */
  int video_time = 0;

  /* Image frame in Blender's own pixel format, may need conversion to the output pixel format. */
  AVFrame *img_convert_frame = nullptr;
  SwsContext *img_convert_ctx = nullptr;

  uint8_t *audio_input_buffer = nullptr;
  uint8_t *audio_deinterleave_buffer = nullptr;
  int audio_input_samples = 0;
  double audio_time = 0.0;
  double audio_time_total = 0.0;
  bool audio_deinterleave = false;
  int audio_sample_size = 0;

  StampData *stamp_data = nullptr;

#  ifdef WITH_AUDASPACE
  AUD_Device *audio_mixdown_device = nullptr;
#  endif
};

bool movie_audio_open(MovieWriter *context,
                      const Scene *scene,
                      int start_frame,
                      int mixrate,
                      float volume,
                      ReportList *reports);
void movie_audio_close(MovieWriter *context, bool is_autosplit);

AVStream *alloc_audio_stream(MovieWriter *context,
                             int audio_mixrate,
                             int audio_channels,
                             AVCodecID codec_id,
                             AVFormatContext *of,
                             char *error,
                             int error_size);
void write_audio_frames(MovieWriter *context, double to_pts);

#endif /* WITH_FFMPEG */
