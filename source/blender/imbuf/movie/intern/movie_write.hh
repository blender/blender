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
#  include <cmath>

extern "C" {
#  include <libavcodec/codec_id.h>
#  include <libavformat/avformat.h>
#  include <libavutil/buffer.h>
#  include <libavutil/channel_layout.h>
#  include <libavutil/imgutils.h>
#  include <libavutil/opt.h>
#  include <libavutil/rational.h>
#  include <libavutil/samplefmt.h>

#  include "ffmpeg_compat.h"
}

#  ifdef WITH_AUDASPACE
#    include <AUD_Types.h>
#  endif

struct Scene;
struct StampData;

struct MovieWriter {
  int ffmpeg_type;
  AVCodecID ffmpeg_codec;
  AVCodecID ffmpeg_audio_codec;
  int ffmpeg_video_bitrate;
  int ffmpeg_audio_bitrate;
  int ffmpeg_gop_size;
  int ffmpeg_max_b_frames;
  int ffmpeg_autosplit_count;
  bool ffmpeg_autosplit;
  bool ffmpeg_preview;

  int ffmpeg_crf;    /* set to 0 to not use CRF mode; we have another flag for lossless anyway. */
  int ffmpeg_preset; /* see eFFMpegPreset */

  AVFormatContext *outfile;
  AVCodecContext *video_codec;
  AVCodecContext *audio_codec;
  AVStream *video_stream;
  AVStream *audio_stream;
  AVFrame *current_frame; /* Image frame in output pixel format. */
  int video_time;

  /* Image frame in Blender's own pixel format, may need conversion to the output pixel format. */
  AVFrame *img_convert_frame;
  SwsContext *img_convert_ctx;

  uint8_t *audio_input_buffer;
  uint8_t *audio_deinterleave_buffer;
  int audio_input_samples;
  double audio_time;
  double audio_time_total;
  bool audio_deinterleave;
  int audio_sample_size;

  StampData *stamp_data;

#  ifdef WITH_AUDASPACE
  AUD_Device *audio_mixdown_device;
#  endif
};

bool movie_audio_open(
    MovieWriter *context, const Scene *scene, int start_frame, int mixrate, float volume);
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
