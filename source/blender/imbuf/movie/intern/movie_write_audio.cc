/* SPDX-FileCopyrightText: 2024 Blender Authors
 *
 * SPDX-License-Identifier: GPL-2.0-or-later */

/** \file
 * \ingroup imbuf
 */

#ifdef _MSC_VER
/* This needs to be included first to prevent ffmpegs headers adding defines for various math
 * constants leading to duplicate definitions. */
#  define _USE_MATH_DEFINES
#  include <cmath>
#endif

#include "movie_util.hh"
#include "movie_write.hh"

#ifdef WITH_FFMPEG

#  include <cstdio>
#  include <cstring>

#  ifdef WITH_AUDASPACE
#    include <AUD_Device.h>
#    include <AUD_Special.h>
#  endif

#  include "DNA_scene_types.h"

#  include "BLI_string.h"
#  include "BLI_utildefines.h"

#  include "BKE_report.hh"
#  include "BKE_sound.hh"

#  include "CLG_log.h"

static CLG_LogRef LOG = {"video.write"};

/* If any of these codecs, we prefer the float sample format (if supported) */
static bool request_float_audio_buffer(int codec_id)
{
  return ELEM(codec_id, AV_CODEC_ID_AAC, AV_CODEC_ID_AC3, AV_CODEC_ID_VORBIS);
}

#  ifdef WITH_AUDASPACE

static int write_audio_frame(MovieWriter *context)
{
  AVFrame *frame = nullptr;
  AVCodecContext *c = context->audio_codec;

  AUD_Device_read(
      context->audio_mixdown_device, context->audio_input_buffer, context->audio_input_samples);

  frame = av_frame_alloc();
  frame->pts = context->audio_time / av_q2d(c->time_base);
  frame->nb_samples = context->audio_input_samples;
  frame->format = c->sample_fmt;
#    ifdef FFMPEG_USE_OLD_CHANNEL_VARS
  frame->channels = c->channels;
  frame->channel_layout = c->channel_layout;
  const int num_channels = c->channels;
#    else
  av_channel_layout_copy(&frame->ch_layout, &c->ch_layout);
  const int num_channels = c->ch_layout.nb_channels;
#    endif

  if (context->audio_deinterleave) {
    int channel, i;
    uint8_t *temp;

    for (channel = 0; channel < num_channels; channel++) {
      for (i = 0; i < frame->nb_samples; i++) {
        memcpy(context->audio_deinterleave_buffer +
                   (i + channel * frame->nb_samples) * context->audio_sample_size,
               context->audio_input_buffer +
                   (num_channels * i + channel) * context->audio_sample_size,
               context->audio_sample_size);
      }
    }

    temp = context->audio_deinterleave_buffer;
    context->audio_deinterleave_buffer = context->audio_input_buffer;
    context->audio_input_buffer = temp;
  }

  avcodec_fill_audio_frame(frame,
                           num_channels,
                           c->sample_fmt,
                           context->audio_input_buffer,
                           context->audio_input_samples * num_channels *
                               context->audio_sample_size,
                           1);

  int success = 1;

  char error_str[AV_ERROR_MAX_STRING_SIZE];
  int ret = avcodec_send_frame(c, frame);
  if (ret < 0) {
    /* Can't send frame to encoder. This shouldn't happen. */
    av_make_error_string(error_str, AV_ERROR_MAX_STRING_SIZE, ret);
    CLOG_ERROR(&LOG, "Can't send audio frame: %s", error_str);
    success = -1;
  }

  AVPacket *pkt = av_packet_alloc();

  while (ret >= 0) {

    ret = avcodec_receive_packet(c, pkt);
    if (ret == AVERROR(EAGAIN) || ret == AVERROR_EOF) {
      break;
    }
    if (ret < 0) {
      av_make_error_string(error_str, AV_ERROR_MAX_STRING_SIZE, ret);
      CLOG_ERROR(&LOG, "Error encoding audio frame: %s", error_str);
      success = -1;
    }

    pkt->stream_index = context->audio_stream->index;
    av_packet_rescale_ts(pkt, c->time_base, context->audio_stream->time_base);
#    ifdef FFMPEG_USE_DURATION_WORKAROUND
    my_guess_pkt_duration(context->outfile, context->audio_stream, pkt);
#    endif

    pkt->flags |= AV_PKT_FLAG_KEY;

    int write_ret = av_interleaved_write_frame(context->outfile, pkt);
    if (write_ret != 0) {
      av_make_error_string(error_str, AV_ERROR_MAX_STRING_SIZE, ret);
      CLOG_ERROR(&LOG, "Error writing audio packet: %s", error_str);
      success = -1;
      break;
    }
  }

  av_packet_free(&pkt);
  av_frame_free(&frame);

  return success;
}
#  endif /* #ifdef WITH_AUDASPACE */

bool movie_audio_open(MovieWriter *context,
                      const Scene *scene,
                      int start_frame,
                      int mixrate,
                      float volume,
                      ReportList *reports)
{
  bool success = true;
#  ifdef WITH_AUDASPACE
  if (context->audio_stream) {
    AVCodecContext *c = context->audio_codec;

    AUD_DeviceSpecs specs;
#    ifdef FFMPEG_USE_OLD_CHANNEL_VARS
    specs.channels = AUD_Channels(c->channels);
#    else
    specs.channels = AUD_Channels(c->ch_layout.nb_channels);
#    endif

    switch (av_get_packed_sample_fmt(c->sample_fmt)) {
      case AV_SAMPLE_FMT_U8:
        specs.format = AUD_FORMAT_U8;
        break;
      case AV_SAMPLE_FMT_S16:
        specs.format = AUD_FORMAT_S16;
        break;
      case AV_SAMPLE_FMT_S32:
        specs.format = AUD_FORMAT_S32;
        break;
      case AV_SAMPLE_FMT_FLT:
        specs.format = AUD_FORMAT_FLOAT32;
        break;
      case AV_SAMPLE_FMT_DBL:
        specs.format = AUD_FORMAT_FLOAT64;
        break;
      default:
        BKE_report(reports, RPT_ERROR, "Audio sample format unsupported");
        success = false;
        break;
    }

    specs.rate = mixrate;
    if (success) {
      context->audio_mixdown_device = BKE_sound_mixdown(scene, specs, start_frame, volume);
    }
  }
#  else
  UNUSED_VARS(context, scene, start_frame, mixrate, volume, reports);
#  endif
  return success;
}

void movie_audio_close(MovieWriter *context, bool is_autosplit)
{
#  ifdef WITH_AUDASPACE
  if (!is_autosplit) {
    if (context->audio_mixdown_device) {
      AUD_Device_free(context->audio_mixdown_device);
      context->audio_mixdown_device = nullptr;
    }
  }
#  else
  UNUSED_VARS(context, is_autosplit);
#  endif
}

AVStream *alloc_audio_stream(MovieWriter *context,
                             int audio_mixrate,
                             int audio_channels,
                             AVCodecID codec_id,
                             AVFormatContext *of,
                             char *error,
                             int error_size)
{
  AVStream *st;
  const AVCodec *codec;

  error[0] = '\0';

  st = avformat_new_stream(of, nullptr);
  if (!st) {
    return nullptr;
  }
  st->id = 1;

  codec = avcodec_find_encoder(codec_id);
  if (!codec) {
    CLOG_ERROR(&LOG, "Couldn't find valid audio codec");
    context->audio_codec = nullptr;
    return nullptr;
  }

  context->audio_codec = avcodec_alloc_context3(codec);
  AVCodecContext *c = context->audio_codec;
  c->thread_count = MOV_thread_count();
  c->thread_type = FF_THREAD_SLICE;

  c->sample_rate = audio_mixrate;
  c->bit_rate = context->ffmpeg_audio_bitrate * 1000;
  c->sample_fmt = AV_SAMPLE_FMT_S16;

  int channel_layout_mask = 0;
  switch (audio_channels) {
    case FFM_CHANNELS_MONO:
      channel_layout_mask = AV_CH_LAYOUT_MONO;
      break;
    case FFM_CHANNELS_STEREO:
      channel_layout_mask = AV_CH_LAYOUT_STEREO;
      break;
    case FFM_CHANNELS_SURROUND4:
      channel_layout_mask = AV_CH_LAYOUT_QUAD;
      break;
    case FFM_CHANNELS_SURROUND51:
      channel_layout_mask = AV_CH_LAYOUT_5POINT1_BACK;
      break;
    case FFM_CHANNELS_SURROUND71:
      channel_layout_mask = AV_CH_LAYOUT_7POINT1;
      break;
  }
  BLI_assert(channel_layout_mask != 0);

#  ifdef FFMPEG_USE_OLD_CHANNEL_VARS
  c->channels = audio_channels;
  c->channel_layout = channel_layout_mask;
#  else
  av_channel_layout_from_mask(&c->ch_layout, channel_layout_mask);
#  endif

  if (request_float_audio_buffer(codec_id)) {
    /* mainly for AAC codec which is experimental */
    c->strict_std_compliance = FF_COMPLIANCE_EXPERIMENTAL;
    c->sample_fmt = AV_SAMPLE_FMT_FLT;
  }

  const enum AVSampleFormat *sample_fmts = ffmpeg_get_sample_fmts(c, codec);
  if (sample_fmts) {
    /* Check if the preferred sample format for this codec is supported.
     * this is because, depending on the version of LIBAV,
     * and with the whole FFMPEG/LIBAV fork situation,
     * you have various implementations around.
     * Float samples in particular are not always supported. */
    const enum AVSampleFormat *p = sample_fmts;
    for (; *p != -1; p++) {
      if (*p == c->sample_fmt) {
        break;
      }
    }
    if (*p == -1) {
      /* sample format incompatible with codec. Defaulting to a format known to work */
      c->sample_fmt = sample_fmts[0];
    }
  }

  const int *supported_samplerates = ffmpeg_get_sample_rates(c, codec);
  if (supported_samplerates) {
    const int *p = supported_samplerates;
    int best = 0;
    int best_dist = INT_MAX;
    for (; *p; p++) {
      int dist = abs(c->sample_rate - *p);
      if (dist < best_dist) {
        best_dist = dist;
        best = *p;
      }
    }
    /* best is the closest supported sample rate (same as selected if best_dist == 0) */
    c->sample_rate = best;
  }

  if (of->oformat->flags & AVFMT_GLOBALHEADER) {
    c->flags |= AV_CODEC_FLAG_GLOBAL_HEADER;
  }

  int ret = avcodec_open2(c, codec, nullptr);

  if (ret < 0) {
    char error_str[AV_ERROR_MAX_STRING_SIZE];
    av_make_error_string(error_str, AV_ERROR_MAX_STRING_SIZE, ret);
    CLOG_ERROR(&LOG, "Couldn't initialize audio codec: %s", error_str);
    BLI_strncpy(error, ffmpeg_last_error(), error_size);
    avcodec_free_context(&c);
    context->audio_codec = nullptr;
    return nullptr;
  }

  /* Need to prevent floating point exception when using VORBIS audio codec,
   * initialize this value in the same way as it's done in FFMPEG itself (sergey) */
  c->time_base.num = 1;
  c->time_base.den = c->sample_rate;

  if (c->codec->capabilities & AV_CODEC_CAP_VARIABLE_FRAME_SIZE) {
    /* If the audio format has a variable frame size, default to 1024.
     * This is because we won't try to encode any variable frame size.
     * 1024 seems to be a good compromize between size and speed.
     */
    context->audio_input_samples = 1024;
  }
  else {
    context->audio_input_samples = c->frame_size;
  }

  context->audio_deinterleave = av_sample_fmt_is_planar(c->sample_fmt);

  context->audio_sample_size = av_get_bytes_per_sample(c->sample_fmt);

  context->audio_input_buffer = (uint8_t *)av_malloc(context->audio_input_samples *
                                                     audio_channels * context->audio_sample_size);
  if (context->audio_deinterleave) {
    context->audio_deinterleave_buffer = (uint8_t *)av_malloc(
        context->audio_input_samples * audio_channels * context->audio_sample_size);
  }

  context->audio_time = 0.0f;

  avcodec_parameters_from_context(st->codecpar, c);

  return st;
}

void write_audio_frames(MovieWriter *context, double to_pts)
{
#  ifdef WITH_AUDASPACE
  AVCodecContext *c = context->audio_codec;

  while (context->audio_stream) {
    if ((context->audio_time_total >= to_pts) || !write_audio_frame(context)) {
      break;
    }
    context->audio_time_total += double(context->audio_input_samples) / double(c->sample_rate);
    context->audio_time += double(context->audio_input_samples) / double(c->sample_rate);
  }
#  else
  UNUSED_VARS(context, to_pts);
#  endif
}

#endif /* WITH_FFMPEG */
