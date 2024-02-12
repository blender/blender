/* SPDX-FileCopyrightText: 2023 Blender Authors
 *
 * SPDX-License-Identifier: GPL-2.0-or-later
 * Partial Copyright 2006 Peter Schlaile. */

/** \file
 * \ingroup bke
 */

#ifdef WITH_FFMPEG
#  include <cstdio>
#  include <cstring>

#  include <cstdlib>

#  include "MEM_guardedalloc.h"

#  include "DNA_scene_types.h"

#  include "BLI_blenlib.h"

#  ifdef WITH_AUDASPACE
#    include <AUD_Device.h>
#    include <AUD_Special.h>
#  endif

#  include "BLI_math_base.h"
#  include "BLI_threads.h"
#  include "BLI_utildefines.h"

#  include "BKE_global.hh"
#  include "BKE_image.h"
#  include "BKE_main.hh"
#  include "BKE_report.hh"
#  include "BKE_sound.h"
#  include "BKE_writeffmpeg.hh"

#  include "IMB_imbuf.hh"

/* This needs to be included after BLI_math_base.h otherwise it will redefine some math defines
 * like M_SQRT1_2 leading to warnings with MSVC */
extern "C" {
#  include <libavcodec/avcodec.h>
#  include <libavformat/avformat.h>
#  include <libavutil/buffer.h>
#  include <libavutil/channel_layout.h>
#  include <libavutil/imgutils.h>
#  include <libavutil/opt.h>
#  include <libavutil/rational.h>
#  include <libavutil/samplefmt.h>
#  include <libswscale/swscale.h>

#  include "ffmpeg_compat.h"
}

struct StampData;

struct FFMpegContext {
  int ffmpeg_type;
  AVCodecID ffmpeg_codec;
  AVCodecID ffmpeg_audio_codec;
  int ffmpeg_video_bitrate;
  int ffmpeg_audio_bitrate;
  int ffmpeg_gop_size;
  int ffmpeg_max_b_frames;
  int ffmpeg_autosplit;
  int ffmpeg_autosplit_count;
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

#  define FFMPEG_AUTOSPLIT_SIZE 2000000000

#  define PRINT \
    if (G.debug & G_DEBUG_FFMPEG) \
    printf

static void ffmpeg_dict_set_int(AVDictionary **dict, const char *key, int value);
static void ffmpeg_filepath_get(FFMpegContext *context,
                                char filepath[FILE_MAX],
                                const RenderData *rd,
                                bool preview,
                                const char *suffix);

/* Delete a picture buffer */

static void delete_picture(AVFrame *f)
{
  if (f) {
    av_frame_free(&f);
  }
}

static int request_float_audio_buffer(int codec_id)
{
  /* If any of these codecs, we prefer the float sample format (if supported) */
  return codec_id == AV_CODEC_ID_AAC || codec_id == AV_CODEC_ID_AC3 ||
         codec_id == AV_CODEC_ID_VORBIS;
}

#  ifdef WITH_AUDASPACE

static int write_audio_frame(FFMpegContext *context)
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
    fprintf(stderr, "Can't send audio frame: %s\n", error_str);
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
      fprintf(stderr, "Error encoding audio frame: %s\n", error_str);
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
      fprintf(stderr, "Error writing audio packet: %s\n", error_str);
      success = -1;
      break;
    }
  }

  av_packet_free(&pkt);
  av_frame_free(&frame);

  return success;
}
#  endif /* #ifdef WITH_AUDASPACE */

/* Allocate a temporary frame */
static AVFrame *alloc_picture(AVPixelFormat pix_fmt, int width, int height)
{
  /* allocate space for the struct */
  AVFrame *f = av_frame_alloc();
  if (f == nullptr) {
    return nullptr;
  }

  /* allocate the actual picture buffer */
  int size = av_image_get_buffer_size(pix_fmt, width, height, 1);
  AVBufferRef *buf = av_buffer_alloc(size);
  if (buf == nullptr) {
    av_frame_free(&f);
    return nullptr;
  }

  av_image_fill_arrays(f->data, f->linesize, buf->data, pix_fmt, width, height, 1);
  f->buf[0] = buf;
  f->format = pix_fmt;
  f->width = width;
  f->height = height;

  return f;
}

/* Get the correct file extensions for the requested format,
 * first is always desired guess_format parameter */
static const char **get_file_extensions(int format)
{
  switch (format) {
    case FFMPEG_DV: {
      static const char *rv[] = {".dv", nullptr};
      return rv;
    }
    case FFMPEG_MPEG1: {
      static const char *rv[] = {".mpg", ".mpeg", nullptr};
      return rv;
    }
    case FFMPEG_MPEG2: {
      static const char *rv[] = {".dvd", ".vob", ".mpg", ".mpeg", nullptr};
      return rv;
    }
    case FFMPEG_MPEG4: {
      static const char *rv[] = {".mp4", ".mpg", ".mpeg", nullptr};
      return rv;
    }
    case FFMPEG_AVI: {
      static const char *rv[] = {".avi", nullptr};
      return rv;
    }
    case FFMPEG_MOV: {
      static const char *rv[] = {".mov", nullptr};
      return rv;
    }
    case FFMPEG_H264: {
      /* FIXME: avi for now... */
      static const char *rv[] = {".avi", nullptr};
      return rv;
    }

    case FFMPEG_XVID: {
      /* FIXME: avi for now... */
      static const char *rv[] = {".avi", nullptr};
      return rv;
    }
    case FFMPEG_FLV: {
      static const char *rv[] = {".flv", nullptr};
      return rv;
    }
    case FFMPEG_MKV: {
      static const char *rv[] = {".mkv", nullptr};
      return rv;
    }
    case FFMPEG_OGG: {
      static const char *rv[] = {".ogv", ".ogg", nullptr};
      return rv;
    }
    case FFMPEG_WEBM: {
      static const char *rv[] = {".webm", nullptr};
      return rv;
    }
    case FFMPEG_AV1: {
      static const char *rv[] = {".mp4", ".mkv", nullptr};
      return rv;
    }
    default:
      return nullptr;
  }
}

/* Write a frame to the output file */
static int write_video_frame(FFMpegContext *context, AVFrame *frame, ReportList *reports)
{
  int ret, success = 1;
  AVPacket *packet = av_packet_alloc();

  AVCodecContext *c = context->video_codec;

  frame->pts = context->video_time;
  context->video_time++;

  char error_str[AV_ERROR_MAX_STRING_SIZE];
  ret = avcodec_send_frame(c, frame);
  if (ret < 0) {
    /* Can't send frame to encoder. This shouldn't happen. */
    av_make_error_string(error_str, AV_ERROR_MAX_STRING_SIZE, ret);
    fprintf(stderr, "Can't send video frame: %s\n", error_str);
    success = -1;
  }

  while (ret >= 0) {
    ret = avcodec_receive_packet(c, packet);

    if (ret == AVERROR(EAGAIN) || ret == AVERROR_EOF) {
      /* No more packets available. */
      break;
    }
    if (ret < 0) {
      av_make_error_string(error_str, AV_ERROR_MAX_STRING_SIZE, ret);
      fprintf(stderr, "Error encoding frame: %s\n", error_str);
      break;
    }

    packet->stream_index = context->video_stream->index;
    av_packet_rescale_ts(packet, c->time_base, context->video_stream->time_base);
#  ifdef FFMPEG_USE_DURATION_WORKAROUND
    my_guess_pkt_duration(context->outfile, context->video_stream, packet);
#  endif

    if (av_interleaved_write_frame(context->outfile, packet) != 0) {
      success = -1;
      break;
    }
  }

  if (!success) {
    BKE_report(reports, RPT_ERROR, "Error writing frame");
    av_make_error_string(error_str, AV_ERROR_MAX_STRING_SIZE, ret);
    PRINT("Error writing frame: %s\n", error_str);
  }

  av_packet_free(&packet);

  return success;
}

/* read and encode a frame of video from the buffer */
static AVFrame *generate_video_frame(FFMpegContext *context, const uint8_t *pixels)
{
  AVCodecParameters *codec = context->video_stream->codecpar;
  int height = codec->height;
  AVFrame *rgb_frame;

  if (context->img_convert_frame != nullptr) {
    /* Pixel format conversion is needed. */
    rgb_frame = context->img_convert_frame;
  }
  else {
    /* The output pixel format is Blender's internal pixel format. */
    rgb_frame = context->current_frame;
  }

  /* Copy the Blender pixels into the FFMPEG data-structure, taking care of endianness and flipping
   * the image vertically. */
  int linesize = rgb_frame->linesize[0];
  for (int y = 0; y < height; y++) {
    uint8_t *target = rgb_frame->data[0] + linesize * (height - y - 1);
    const uint8_t *src = pixels + linesize * y;

#  if ENDIAN_ORDER == L_ENDIAN
    memcpy(target, src, linesize);

#  elif ENDIAN_ORDER == B_ENDIAN
    const uint8_t *end = src + linesize;
    while (src != end) {
      target[3] = src[0];
      target[2] = src[1];
      target[1] = src[2];
      target[0] = src[3];

      target += 4;
      src += 4;
    }
#  else
#    error ENDIAN_ORDER should either be L_ENDIAN or B_ENDIAN.
#  endif
  }

  /* Convert to the output pixel format, if it's different that Blender's internal one. */
  if (context->img_convert_frame != nullptr) {
    BLI_assert(context->img_convert_ctx != NULL);
    BKE_ffmpeg_sws_scale_frame(context->img_convert_ctx, context->current_frame, rgb_frame);
  }

  return context->current_frame;
}

static AVRational calc_time_base(uint den, double num, int codec_id)
{
  /* Convert the input 'num' to an integer. Simply shift the decimal places until we get an integer
   * (within a floating point error range).
   * For example if we have `den = 3` and `num = 0.1` then the fps is: `den/num = 30` fps.
   * When converting this to a FFMPEG time base, we want num to be an integer.
   * So we simply move the decimal places of both numbers. i.e. `den = 30`, `num = 1`. */
  float eps = FLT_EPSILON;
  const uint DENUM_MAX = (codec_id == AV_CODEC_ID_MPEG4) ? (1UL << 16) - 1 : (1UL << 31) - 1;

  /* Calculate the precision of the initial floating point number. */
  if (num > 1.0) {
    const uint num_integer_bits = log2_floor_u(uint(num));

    /* Formula for calculating the epsilon value: (power of two range) / (pow mantissa bits)
     * For example, a float has 23 mantissa bits and the float value 3.5f as a pow2 range of
     * (4-2=2):
     * (2) / pow2(23) = floating point precision for 3.5f
     */
    eps = float(1 << num_integer_bits) * FLT_EPSILON;
  }

  /* Calculate how many decimal shifts we can do until we run out of precision. */
  const int max_num_shift = fabsf(log10f(eps));
  /* Calculate how many times we can shift the denominator. */
  const int max_den_shift = log10f(DENUM_MAX) - log10f(den);
  const int max_iter = min_ii(max_num_shift, max_den_shift);

  for (int i = 0; i < max_iter && fabs(num - round(num)) > eps; i++) {
    /* Increase the number and denominator until both are integers. */
    num *= 10;
    den *= 10;
    eps *= 10;
  }

  AVRational time_base;
  time_base.den = den;
  time_base.num = int(num);

  return time_base;
}

static const AVCodec *get_av1_encoder(
    FFMpegContext *context, RenderData *rd, AVDictionary **opts, int rectx, int recty)
{
  /* There are three possible encoders for AV1: `libaom-av1`, librav1e, and `libsvtav1`. librav1e
   * tends to give the best compression quality while `libsvtav1` tends to be the fastest encoder.
   * One of each will be picked based on the preset setting, and if a particular encoder is not
   * available, then use the default returned by FFMpeg. */
  const AVCodec *codec = nullptr;
  switch (context->ffmpeg_preset) {
    case FFM_PRESET_BEST:
      /* `libaom-av1` may produce better VMAF-scoring videos in several cases, but there are cases
       * where using a different encoder is desirable, such as in #103849. */
      codec = avcodec_find_encoder_by_name("librav1e");
      if (!codec) {
        /* Fallback to `libaom-av1` if librav1e is not found. */
        codec = avcodec_find_encoder_by_name("libaom-av1");
      }
      break;
    case FFM_PRESET_REALTIME:
      codec = avcodec_find_encoder_by_name("libsvtav1");
      break;
    case FFM_PRESET_GOOD:
    default:
      codec = avcodec_find_encoder_by_name("libaom-av1");
      break;
  }

  /* Use the default AV1 encoder if the specified encoder wasn't found. */
  if (!codec) {
    codec = avcodec_find_encoder(AV_CODEC_ID_AV1);
  }

  /* Apply AV1 encoder specific settings. */
  if (codec) {
    if (STREQ(codec->name, "librav1e")) {
      /* Set "tiles" to 8 to enable multi-threaded encoding. */
      if (rd->threads > 8) {
        ffmpeg_dict_set_int(opts, "tiles", rd->threads);
      }
      else {
        ffmpeg_dict_set_int(opts, "tiles", 8);
      }

      /* Use a reasonable speed setting based on preset. Speed ranges from 0-10.
       * Must check context->ffmpeg_preset again in case this encoder was selected due to the
       * absence of another. */
      switch (context->ffmpeg_preset) {
        case FFM_PRESET_BEST:
          ffmpeg_dict_set_int(opts, "speed", 4);
          break;
        case FFM_PRESET_REALTIME:
          ffmpeg_dict_set_int(opts, "speed", 10);
          break;
        case FFM_PRESET_GOOD:
        default:
          ffmpeg_dict_set_int(opts, "speed", 6);
          break;
      }
      if (context->ffmpeg_crf >= 0) {
        /* librav1e does not use `-crf`, but uses `-qp` in the range of 0-255.
         * Calculates the roughly equivalent float, and truncates it to an integer. */
        uint qp_value = float(context->ffmpeg_crf) * 255.0f / 51.0f;
        if (qp_value > 255) {
          qp_value = 255;
        }
        ffmpeg_dict_set_int(opts, "qp", qp_value);
      }
      /* Set gop_size as rav1e's "--keyint". */
      char buffer[64];
      SNPRINTF(buffer, "keyint=%d", context->ffmpeg_gop_size);
      av_dict_set(opts, "rav1e-params", buffer, 0);
    }
    else if (STREQ(codec->name, "libsvtav1")) {
      /* Set preset value based on ffmpeg_preset.
       * Must check `context->ffmpeg_preset` again in case this encoder was selected due to the
       * absence of another. */
      switch (context->ffmpeg_preset) {
        case FFM_PRESET_REALTIME:
          ffmpeg_dict_set_int(opts, "preset", 8);
          break;
        case FFM_PRESET_BEST:
          ffmpeg_dict_set_int(opts, "preset", 3);
          break;
        case FFM_PRESET_GOOD:
        default:
          ffmpeg_dict_set_int(opts, "preset", 5);
          break;
      }
      if (context->ffmpeg_crf >= 0) {
        /* `libsvtav1` does not support CRF until FFMPEG builds since 2022-02-24,
         * use `qp` as fallback. */
        ffmpeg_dict_set_int(opts, "qp", context->ffmpeg_crf);
      }
    }
    else if (STREQ(codec->name, "libaom-av1")) {
      /* Speed up libaom-av1 encoding by enabling multi-threading and setting tiles. */
      ffmpeg_dict_set_int(opts, "row-mt", 1);
      const char *tiles_string = nullptr;
      bool tiles_string_is_dynamic = false;
      if (rd->threads > 0) {
        /* See if threads is a square. */
        int threads_sqrt = sqrtf(rd->threads);
        if (threads_sqrt < 4) {
          /* Ensure a default minimum. */
          threads_sqrt = 4;
        }
        if (is_power_of_2_i(threads_sqrt) && threads_sqrt * threads_sqrt == rd->threads) {
          /* Is a square num, therefore just do "sqrt x sqrt" for tiles parameter. */
          int digits = 0;
          for (int t_sqrt_copy = threads_sqrt; t_sqrt_copy > 0; t_sqrt_copy /= 10) {
            ++digits;
          }
          /* A char array need only an alignment of 1. */
          char *tiles_string_mut = (char *)calloc(digits * 2 + 2, 1);
          BLI_snprintf(tiles_string_mut, digits * 2 + 2, "%dx%d", threads_sqrt, threads_sqrt);
          tiles_string_is_dynamic = true;
          tiles_string = tiles_string_mut;
        }
        else {
          /* Is not a square num, set greater side based on longer side, or use a square if both
           * sides are equal. */
          int sqrt_p2 = power_of_2_min_i(threads_sqrt);
          if (sqrt_p2 < 2) {
            /* Ensure a default minimum. */
            sqrt_p2 = 2;
          }
          int sqrt_p2_next = power_of_2_min_i(int(rd->threads) / sqrt_p2);
          if (sqrt_p2_next < 1) {
            sqrt_p2_next = 1;
          }
          if (sqrt_p2 > sqrt_p2_next) {
            /* Ensure sqrt_p2_next is greater or equal to sqrt_p2. */
            int temp = sqrt_p2;
            sqrt_p2 = sqrt_p2_next;
            sqrt_p2_next = temp;
          }
          int combined_digits = 0;
          for (int sqrt_p2_copy = sqrt_p2; sqrt_p2_copy > 0; sqrt_p2_copy /= 10) {
            ++combined_digits;
          }
          for (int sqrt_p2_copy = sqrt_p2_next; sqrt_p2_copy > 0; sqrt_p2_copy /= 10) {
            ++combined_digits;
          }
          /* A char array need only an alignment of 1. */
          char *tiles_string_mut = (char *)calloc(combined_digits + 2, 1);
          if (rectx > recty) {
            BLI_snprintf(tiles_string_mut, combined_digits + 2, "%dx%d", sqrt_p2_next, sqrt_p2);
          }
          else if (rectx < recty) {
            BLI_snprintf(tiles_string_mut, combined_digits + 2, "%dx%d", sqrt_p2, sqrt_p2_next);
          }
          else {
            BLI_snprintf(tiles_string_mut, combined_digits + 2, "%dx%d", sqrt_p2, sqrt_p2);
          }
          tiles_string_is_dynamic = true;
          tiles_string = tiles_string_mut;
        }
      }
      else {
        /* Thread count unknown, default to 8. */
        if (rectx > recty) {
          tiles_string = "4x2";
        }
        else if (rectx < recty) {
          tiles_string = "2x4";
        }
        else {
          tiles_string = "2x2";
        }
      }
      av_dict_set(opts, "tiles", tiles_string, 0);
      if (tiles_string_is_dynamic) {
        free((void *)tiles_string);
      }
      /* libaom-av1 uses "cpu-used" instead of "preset" for defining compression quality.
       * This value is in a range from 0-8. 0 and 8 are extremes, but we will allow 8.
       * Must check context->ffmpeg_preset again in case this encoder was selected due to the
       * absence of another. */
      switch (context->ffmpeg_preset) {
        case FFM_PRESET_REALTIME:
          ffmpeg_dict_set_int(opts, "cpu-used", 8);
          break;
        case FFM_PRESET_BEST:
          ffmpeg_dict_set_int(opts, "cpu-used", 4);
          break;
        case FFM_PRESET_GOOD:
        default:
          ffmpeg_dict_set_int(opts, "cpu-used", 6);
          break;
      }

      /* CRF related settings is similar to H264 for libaom-av1, so we will rely on those settings
       * applied later. */
    }
  }

  return codec;
}

SwsContext *BKE_ffmpeg_sws_get_context(
    int width, int height, int av_src_format, int av_dst_format, int sws_flags)
{
#  if defined(FFMPEG_SWSCALE_THREADING)
  /* sws_getContext does not allow passing flags that ask for multi-threaded
   * scaling context, so do it the hard way. */
  SwsContext *c = sws_alloc_context();
  if (c == nullptr) {
    return nullptr;
  }
  av_opt_set_int(c, "srcw", width, 0);
  av_opt_set_int(c, "srch", height, 0);
  av_opt_set_int(c, "src_format", av_src_format, 0);
  av_opt_set_int(c, "dstw", width, 0);
  av_opt_set_int(c, "dsth", height, 0);
  av_opt_set_int(c, "dst_format", av_dst_format, 0);
  av_opt_set_int(c, "sws_flags", sws_flags, 0);
  av_opt_set_int(c, "threads", BLI_system_thread_count(), 0);

  if (sws_init_context(c, nullptr, nullptr) < 0) {
    sws_freeContext(c);
    return nullptr;
  }
#  else
  SwsContext *c = sws_getContext(width,
                                 height,
                                 AVPixelFormat(av_src_format),
                                 width,
                                 height,
                                 AVPixelFormat(av_dst_format),
                                 sws_flags,
                                 nullptr,
                                 nullptr,
                                 nullptr);
#  endif

  return c;
}
void BKE_ffmpeg_sws_scale_frame(SwsContext *ctx, AVFrame *dst, const AVFrame *src)
{
#  if defined(FFMPEG_SWSCALE_THREADING)
  sws_scale_frame(ctx, dst, src);
#  else
  sws_scale(ctx, src->data, src->linesize, 0, src->height, dst->data, dst->linesize);
#  endif
}

/* prepare a video stream for the output file */

static AVStream *alloc_video_stream(FFMpegContext *context,
                                    RenderData *rd,
                                    AVCodecID codec_id,
                                    AVFormatContext *of,
                                    int rectx,
                                    int recty,
                                    char *error,
                                    int error_size)
{
  AVStream *st;
  const AVCodec *codec;
  AVDictionary *opts = nullptr;

  error[0] = '\0';

  st = avformat_new_stream(of, nullptr);
  if (!st) {
    return nullptr;
  }
  st->id = 0;

  /* Set up the codec context */

  if (codec_id == AV_CODEC_ID_AV1) {
    /* Use get_av1_encoder() to get the ideal (hopefully) encoder for AV1 based
     * on given parameters, and also set up opts. */
    codec = get_av1_encoder(context, rd, &opts, rectx, recty);
  }
  else {
    codec = avcodec_find_encoder(codec_id);
  }
  if (!codec) {
    fprintf(stderr, "Couldn't find valid video codec\n");
    context->video_codec = nullptr;
    return nullptr;
  }

  context->video_codec = avcodec_alloc_context3(codec);
  AVCodecContext *c = context->video_codec;

  /* Get some values from the current render settings */

  c->width = rectx;
  c->height = recty;

  if (context->ffmpeg_type == FFMPEG_DV && rd->frs_sec != 25) {
    /* FIXME: Really bad hack (tm) for NTSC support */
    c->time_base.den = 2997;
    c->time_base.num = 100;
  }
  else if (float(int(rd->frs_sec_base)) == rd->frs_sec_base) {
    c->time_base.den = rd->frs_sec;
    c->time_base.num = int(rd->frs_sec_base);
  }
  else {
    c->time_base = calc_time_base(rd->frs_sec, rd->frs_sec_base, codec_id);
  }

  /* As per the time-base documentation here:
   * https://www.ffmpeg.org/ffmpeg-codecs.html#Codec-Options
   * We want to set the time base to (1 / fps) for fixed frame rate video.
   * If it is not possible, we want to set the time-base numbers to something as
   * small as possible.
   */
  if (c->time_base.num != 1) {
    AVRational new_time_base;
    if (av_reduce(
            &new_time_base.num, &new_time_base.den, c->time_base.num, c->time_base.den, INT_MAX))
    {
      /* Exact reduction was possible. Use the new value. */
      c->time_base = new_time_base;
    }
  }

  st->time_base = c->time_base;

  c->gop_size = context->ffmpeg_gop_size;
  c->max_b_frames = context->ffmpeg_max_b_frames;

  if (context->ffmpeg_type == FFMPEG_WEBM && context->ffmpeg_crf == 0) {
    ffmpeg_dict_set_int(&opts, "lossless", 1);
  }
  else if (context->ffmpeg_crf >= 0) {
    /* As per https://trac.ffmpeg.org/wiki/Encode/VP9 we must set the bit rate to zero when
     * encoding with VP9 in CRF mode.
     * Set this to always be zero for other codecs as well.
     * We don't care about bit rate in CRF mode. */
    c->bit_rate = 0;
    ffmpeg_dict_set_int(&opts, "crf", context->ffmpeg_crf);
  }
  else {
    c->bit_rate = context->ffmpeg_video_bitrate * 1000;
    c->rc_max_rate = rd->ffcodecdata.rc_max_rate * 1000;
    c->rc_min_rate = rd->ffcodecdata.rc_min_rate * 1000;
    c->rc_buffer_size = rd->ffcodecdata.rc_buffer_size * 1024;
  }

  if (context->ffmpeg_preset) {
    /* 'preset' is used by h.264, 'deadline' is used by WEBM/VP9. I'm not
     * setting those properties conditionally based on the video codec,
     * as the FFmpeg encoder simply ignores unknown settings anyway. */
    char const *preset_name = nullptr;   /* Used by h.264. */
    char const *deadline_name = nullptr; /* Used by WEBM/VP9. */
    switch (context->ffmpeg_preset) {
      case FFM_PRESET_GOOD:
        preset_name = "medium";
        deadline_name = "good";
        break;
      case FFM_PRESET_BEST:
        preset_name = "slower";
        deadline_name = "best";
        break;
      case FFM_PRESET_REALTIME:
        preset_name = "superfast";
        deadline_name = "realtime";
        break;
      default:
        printf("Unknown preset number %i, ignoring.\n", context->ffmpeg_preset);
    }
    /* "codec_id != AV_CODEC_ID_AV1" is required due to "preset" already being set by an AV1 codec.
     */
    if (preset_name != nullptr && codec_id != AV_CODEC_ID_AV1) {
      av_dict_set(&opts, "preset", preset_name, 0);
    }
    if (deadline_name != nullptr) {
      av_dict_set(&opts, "deadline", deadline_name, 0);
    }
  }

  /* Be sure to use the correct pixel format(e.g. RGB, YUV) */

  if (codec->pix_fmts) {
    c->pix_fmt = codec->pix_fmts[0];
  }
  else {
    /* makes HuffYUV happy ... */
    c->pix_fmt = AV_PIX_FMT_YUV422P;
  }

  if (context->ffmpeg_type == FFMPEG_XVID) {
    /* Alas! */
    c->pix_fmt = AV_PIX_FMT_YUV420P;
    c->codec_tag = (('D' << 24) + ('I' << 16) + ('V' << 8) + 'X');
  }

  /* Keep lossless encodes in the RGB domain. */
  if (codec_id == AV_CODEC_ID_HUFFYUV) {
    if (rd->im_format.planes == R_IMF_PLANES_RGBA) {
      c->pix_fmt = AV_PIX_FMT_BGRA;
    }
    else {
      c->pix_fmt = AV_PIX_FMT_RGB32;
    }
  }

  if (codec_id == AV_CODEC_ID_DNXHD) {
    if (rd->ffcodecdata.flags & FFMPEG_LOSSLESS_OUTPUT) {
      /* Set the block decision algorithm to be of the highest quality ("rd" == 2). */
      c->mb_decision = 2;
    }
  }

  if (codec_id == AV_CODEC_ID_FFV1) {
    c->pix_fmt = AV_PIX_FMT_RGB32;
  }

  if (codec_id == AV_CODEC_ID_QTRLE) {
    if (rd->im_format.planes == R_IMF_PLANES_RGBA) {
      c->pix_fmt = AV_PIX_FMT_ARGB;
    }
  }

  if (codec_id == AV_CODEC_ID_VP9 && rd->im_format.planes == R_IMF_PLANES_RGBA) {
    c->pix_fmt = AV_PIX_FMT_YUVA420P;
  }
  else if (ELEM(codec_id, AV_CODEC_ID_H264, AV_CODEC_ID_VP9) && (context->ffmpeg_crf == 0)) {
    /* Use 4:4:4 instead of 4:2:0 pixel format for lossless rendering. */
    c->pix_fmt = AV_PIX_FMT_YUV444P;
  }

  if (codec_id == AV_CODEC_ID_PNG) {
    if (rd->im_format.planes == R_IMF_PLANES_RGBA) {
      c->pix_fmt = AV_PIX_FMT_RGBA;
    }
  }

  if (of->oformat->flags & AVFMT_GLOBALHEADER) {
    PRINT("Using global header\n");
    c->flags |= AV_CODEC_FLAG_GLOBAL_HEADER;
  }

  /* xasp & yasp got float lately... */

  st->sample_aspect_ratio = c->sample_aspect_ratio = av_d2q((double(rd->xasp) / double(rd->yasp)),
                                                            255);
  st->avg_frame_rate = av_inv_q(c->time_base);

  if (codec->capabilities & AV_CODEC_CAP_OTHER_THREADS) {
    c->thread_count = 0;
  }
  else {
    c->thread_count = BLI_system_thread_count();
  }

  if (codec->capabilities & AV_CODEC_CAP_FRAME_THREADS) {
    c->thread_type = FF_THREAD_FRAME;
  }
  else if (codec->capabilities & AV_CODEC_CAP_SLICE_THREADS) {
    c->thread_type = FF_THREAD_SLICE;
  }

  int ret = avcodec_open2(c, codec, &opts);

  if (ret < 0) {
    char error_str[AV_ERROR_MAX_STRING_SIZE];
    av_make_error_string(error_str, AV_ERROR_MAX_STRING_SIZE, ret);
    fprintf(stderr, "Couldn't initialize video codec: %s\n", error_str);
    BLI_strncpy(error, IMB_ffmpeg_last_error(), error_size);
    av_dict_free(&opts);
    avcodec_free_context(&c);
    context->video_codec = nullptr;
    return nullptr;
  }
  av_dict_free(&opts);

  /* FFMPEG expects its data in the output pixel format. */
  context->current_frame = alloc_picture(c->pix_fmt, c->width, c->height);

  if (c->pix_fmt == AV_PIX_FMT_RGBA) {
    /* Output pixel format is the same we use internally, no conversion necessary. */
    context->img_convert_frame = nullptr;
    context->img_convert_ctx = nullptr;
  }
  else {
    /* Output pixel format is different, allocate frame for conversion. */
    context->img_convert_frame = alloc_picture(AV_PIX_FMT_RGBA, c->width, c->height);
    context->img_convert_ctx = BKE_ffmpeg_sws_get_context(
        c->width, c->height, AV_PIX_FMT_RGBA, c->pix_fmt, SWS_BICUBIC);
  }

  avcodec_parameters_from_context(st->codecpar, c);

  context->video_time = 0.0f;

  return st;
}

static AVStream *alloc_audio_stream(FFMpegContext *context,
                                    RenderData *rd,
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
    fprintf(stderr, "Couldn't find valid audio codec\n");
    context->audio_codec = nullptr;
    return nullptr;
  }

  context->audio_codec = avcodec_alloc_context3(codec);
  AVCodecContext *c = context->audio_codec;
  c->thread_count = BLI_system_thread_count();
  c->thread_type = FF_THREAD_SLICE;

  c->sample_rate = rd->ffcodecdata.audio_mixrate;
  c->bit_rate = context->ffmpeg_audio_bitrate * 1000;
  c->sample_fmt = AV_SAMPLE_FMT_S16;

  const int num_channels = rd->ffcodecdata.audio_channels;
  int channel_layout_mask = 0;
  switch (rd->ffcodecdata.audio_channels) {
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
  c->channels = num_channels;
  c->channel_layout = channel_layout_mask;
#  else
  av_channel_layout_from_mask(&c->ch_layout, channel_layout_mask);
#  endif

  if (request_float_audio_buffer(codec_id)) {
    /* mainly for AAC codec which is experimental */
    c->strict_std_compliance = FF_COMPLIANCE_EXPERIMENTAL;
    c->sample_fmt = AV_SAMPLE_FMT_FLT;
  }

  if (codec->sample_fmts) {
    /* Check if the preferred sample format for this codec is supported.
     * this is because, depending on the version of LIBAV,
     * and with the whole FFMPEG/LIBAV fork situation,
     * you have various implementations around.
     * Float samples in particular are not always supported. */
    const enum AVSampleFormat *p = codec->sample_fmts;
    for (; *p != -1; p++) {
      if (*p == c->sample_fmt) {
        break;
      }
    }
    if (*p == -1) {
      /* sample format incompatible with codec. Defaulting to a format known to work */
      c->sample_fmt = codec->sample_fmts[0];
    }
  }

  if (codec->supported_samplerates) {
    const int *p = codec->supported_samplerates;
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
    fprintf(stderr, "Couldn't initialize audio codec: %s\n", error_str);
    BLI_strncpy(error, IMB_ffmpeg_last_error(), error_size);
    avcodec_free_context(&c);
    context->audio_codec = nullptr;
    return nullptr;
  }

  /* Need to prevent floating point exception when using VORBIS audio codec,
   * initialize this value in the same way as it's done in FFMPEG itself (sergey) */
  c->time_base.num = 1;
  c->time_base.den = c->sample_rate;

  if (c->frame_size == 0) {
    /* Used to be if ((c->codec_id >= CODEC_ID_PCM_S16LE) && (c->codec_id <= CODEC_ID_PCM_DVD))
     * not sure if that is needed anymore, so let's try out if there are any
     * complaints regarding some FFMPEG versions users might have. */
    context->audio_input_samples = AV_INPUT_BUFFER_MIN_SIZE * 8 / c->bits_per_coded_sample /
                                   num_channels;
  }
  else {
    context->audio_input_samples = c->frame_size;
  }

  context->audio_deinterleave = av_sample_fmt_is_planar(c->sample_fmt);

  context->audio_sample_size = av_get_bytes_per_sample(c->sample_fmt);

  context->audio_input_buffer = (uint8_t *)av_malloc(context->audio_input_samples * num_channels *
                                                     context->audio_sample_size);
  if (context->audio_deinterleave) {
    context->audio_deinterleave_buffer = (uint8_t *)av_malloc(
        context->audio_input_samples * num_channels * context->audio_sample_size);
  }

  context->audio_time = 0.0f;

  avcodec_parameters_from_context(st->codecpar, c);

  return st;
}
/* essential functions -- start, append, end */

static void ffmpeg_dict_set_int(AVDictionary **dict, const char *key, int value)
{
  char buffer[32];

  SNPRINTF(buffer, "%d", value);

  av_dict_set(dict, key, buffer, 0);
}

static void ffmpeg_add_metadata_callback(void *data,
                                         const char *propname,
                                         char *propvalue,
                                         int /*propvalue_maxncpy*/)
{
  AVDictionary **metadata = (AVDictionary **)data;
  av_dict_set(metadata, propname, propvalue, 0);
}

static int start_ffmpeg_impl(FFMpegContext *context,
                             RenderData *rd,
                             int rectx,
                             int recty,
                             const char *suffix,
                             ReportList *reports)
{
  /* Handle to the output file */
  AVFormatContext *of;
  const AVOutputFormat *fmt;
  char filepath[FILE_MAX], error[1024];
  const char **exts;
  int ret = 0;

  context->ffmpeg_type = rd->ffcodecdata.type;
  context->ffmpeg_codec = AVCodecID(rd->ffcodecdata.codec);
  context->ffmpeg_audio_codec = AVCodecID(rd->ffcodecdata.audio_codec);
  context->ffmpeg_video_bitrate = rd->ffcodecdata.video_bitrate;
  context->ffmpeg_audio_bitrate = rd->ffcodecdata.audio_bitrate;
  context->ffmpeg_gop_size = rd->ffcodecdata.gop_size;
  context->ffmpeg_autosplit = rd->ffcodecdata.flags & FFMPEG_AUTOSPLIT_OUTPUT;
  context->ffmpeg_crf = rd->ffcodecdata.constant_rate_factor;
  context->ffmpeg_preset = rd->ffcodecdata.ffmpeg_preset;

  if ((rd->ffcodecdata.flags & FFMPEG_USE_MAX_B_FRAMES) != 0) {
    context->ffmpeg_max_b_frames = rd->ffcodecdata.max_b_frames;
  }

  /* Determine the correct filename */
  ffmpeg_filepath_get(context, filepath, rd, context->ffmpeg_preview, suffix);
  PRINT(
      "Starting output to %s(FFMPEG)...\n"
      "  Using type=%d, codec=%d, audio_codec=%d,\n"
      "  video_bitrate=%d, audio_bitrate=%d,\n"
      "  gop_size=%d, autosplit=%d\n"
      "  render width=%d, render height=%d\n",
      filepath,
      context->ffmpeg_type,
      context->ffmpeg_codec,
      context->ffmpeg_audio_codec,
      context->ffmpeg_video_bitrate,
      context->ffmpeg_audio_bitrate,
      context->ffmpeg_gop_size,
      context->ffmpeg_autosplit,
      rectx,
      recty);

  /* Sanity checks for the output file extensions. */
  exts = get_file_extensions(context->ffmpeg_type);
  if (!exts) {
    BKE_report(reports, RPT_ERROR, "No valid formats found");
    return 0;
  }

  fmt = av_guess_format(nullptr, exts[0], nullptr);
  if (!fmt) {
    BKE_report(reports, RPT_ERROR, "No valid formats found");
    return 0;
  }

  of = avformat_alloc_context();
  if (!of) {
    BKE_report(reports, RPT_ERROR, "Can't allocate FFmpeg format context");
    return 0;
  }

  enum AVCodecID audio_codec = context->ffmpeg_audio_codec;
  enum AVCodecID video_codec = context->ffmpeg_codec;

  of->url = av_strdup(filepath);
  /* Check if we need to force change the codec because of file type codec restrictions */
  switch (context->ffmpeg_type) {
    case FFMPEG_OGG:
      video_codec = AV_CODEC_ID_THEORA;
      break;
    case FFMPEG_DV:
      video_codec = AV_CODEC_ID_DVVIDEO;
      break;
    case FFMPEG_MPEG1:
      video_codec = AV_CODEC_ID_MPEG1VIDEO;
      break;
    case FFMPEG_MPEG2:
      video_codec = AV_CODEC_ID_MPEG2VIDEO;
      break;
    case FFMPEG_H264:
      video_codec = AV_CODEC_ID_H264;
      break;
    case FFMPEG_XVID:
      video_codec = AV_CODEC_ID_MPEG4;
      break;
    case FFMPEG_FLV:
      video_codec = AV_CODEC_ID_FLV1;
      break;
    case FFMPEG_AV1:
      video_codec = AV_CODEC_ID_AV1;
      break;
    default:
      /* These containers are not restricted to any specific codec types.
       * Currently we expect these to be .avi, .mov, .mkv, and .mp4.
       */
      video_codec = context->ffmpeg_codec;
      break;
  }

    /* Returns after this must 'goto fail;' */

#  if LIBAVFORMAT_VERSION_MAJOR >= 59
  of->oformat = fmt;
#  else
  /* *DEPRECATED* 2022/08/01 For FFMPEG (<5.0) remove this else branch and the `ifdef` above. */
  of->oformat = (AVOutputFormat *)fmt;
#  endif

  if (video_codec == AV_CODEC_ID_DVVIDEO) {
    if (rectx != 720) {
      BKE_report(reports, RPT_ERROR, "Render width has to be 720 pixels for DV!");
      goto fail;
    }
    if (rd->frs_sec != 25 && recty != 480) {
      BKE_report(reports, RPT_ERROR, "Render height has to be 480 pixels for DV-NTSC!");
      goto fail;
    }
    if (rd->frs_sec == 25 && recty != 576) {
      BKE_report(reports, RPT_ERROR, "Render height has to be 576 pixels for DV-PAL!");
      goto fail;
    }
  }

  if (context->ffmpeg_type == FFMPEG_DV) {
    audio_codec = AV_CODEC_ID_PCM_S16LE;
    if (context->ffmpeg_audio_codec != AV_CODEC_ID_NONE &&
        rd->ffcodecdata.audio_mixrate != 48000 && rd->ffcodecdata.audio_channels != 2)
    {
      BKE_report(reports, RPT_ERROR, "FFmpeg only supports 48khz / stereo audio for DV!");
      goto fail;
    }
  }

  if (video_codec != AV_CODEC_ID_NONE) {
    context->video_stream = alloc_video_stream(
        context, rd, video_codec, of, rectx, recty, error, sizeof(error));
    PRINT("alloc video stream %p\n", context->video_stream);
    if (!context->video_stream) {
      if (error[0]) {
        BKE_report(reports, RPT_ERROR, error);
        PRINT("Video stream error: %s\n", error);
      }
      else {
        BKE_report(reports, RPT_ERROR, "Error initializing video stream");
        PRINT("Error initializing video stream");
      }
      goto fail;
    }
  }

  if (context->ffmpeg_audio_codec != AV_CODEC_ID_NONE) {
    context->audio_stream = alloc_audio_stream(context, rd, audio_codec, of, error, sizeof(error));
    if (!context->audio_stream) {
      if (error[0]) {
        BKE_report(reports, RPT_ERROR, error);
        PRINT("Audio stream error: %s\n", error);
      }
      else {
        BKE_report(reports, RPT_ERROR, "Error initializing audio stream");
        PRINT("Error initializing audio stream");
      }
      goto fail;
    }
  }
  if (!(fmt->flags & AVFMT_NOFILE)) {
    if (avio_open(&of->pb, filepath, AVIO_FLAG_WRITE) < 0) {
      BKE_report(reports, RPT_ERROR, "Could not open file for writing");
      PRINT("Could not open file for writing\n");
      goto fail;
    }
  }

  if (context->stamp_data != nullptr) {
    BKE_stamp_info_callback(
        &of->metadata, context->stamp_data, ffmpeg_add_metadata_callback, false);
  }

  ret = avformat_write_header(of, nullptr);
  if (ret < 0) {
    BKE_report(reports,
               RPT_ERROR,
               "Could not initialize streams, probably unsupported codec combination");
    char error_str[AV_ERROR_MAX_STRING_SIZE];
    av_make_error_string(error_str, AV_ERROR_MAX_STRING_SIZE, ret);
    PRINT("Could not write media header: %s\n", error_str);
    goto fail;
  }

  context->outfile = of;
  av_dump_format(of, 0, filepath, 1);

  return 1;

fail:
  if (of->pb) {
    avio_close(of->pb);
  }

  if (context->video_stream) {
    context->video_stream = nullptr;
  }

  if (context->audio_stream) {
    context->audio_stream = nullptr;
  }

  avformat_free_context(of);
  return 0;
}

/**
 * Writes any delayed frames in the encoder. This function is called before
 * closing the encoder.
 *
 * <p>
 * Since an encoder may use both past and future frames to predict
 * inter-frames (H.264 B-frames, for example), it can output the frames
 * in a different order from the one it was given.
 * For example, when sending frames 1, 2, 3, 4 to the encoder, it may write
 * them in the order 1, 4, 2, 3 - first the two frames used for prediction,
 * and then the bidirectionally-predicted frames. What this means in practice
 * is that the encoder may not immediately produce one output frame for each
 * input frame. These delayed frames must be flushed before we close the
 * stream. We do this by calling avcodec_encode_video with NULL for the last
 * parameter.
 * </p>
 */
static void flush_ffmpeg(AVCodecContext *c, AVStream *stream, AVFormatContext *outfile)
{
  char error_str[AV_ERROR_MAX_STRING_SIZE];
  AVPacket *packet = av_packet_alloc();

  avcodec_send_frame(c, nullptr);

  /* Get the packets frames. */
  int ret = 1;
  while (ret >= 0) {
    ret = avcodec_receive_packet(c, packet);

    if (ret == AVERROR(EAGAIN) || ret == AVERROR_EOF) {
      /* No more packets to flush. */
      break;
    }
    if (ret < 0) {
      av_make_error_string(error_str, AV_ERROR_MAX_STRING_SIZE, ret);
      fprintf(stderr, "Error encoding delayed frame: %s\n", error_str);
      break;
    }

    packet->stream_index = stream->index;
    av_packet_rescale_ts(packet, c->time_base, stream->time_base);
#  ifdef FFMPEG_USE_DURATION_WORKAROUND
    my_guess_pkt_duration(outfile, stream, packet);
#  endif

    int write_ret = av_interleaved_write_frame(outfile, packet);
    if (write_ret != 0) {
      av_make_error_string(error_str, AV_ERROR_MAX_STRING_SIZE, ret);
      fprintf(stderr, "Error writing delayed frame: %s\n", error_str);
      break;
    }
  }

  av_packet_free(&packet);
}

/* **********************************************************************
 * * public interface
 * ********************************************************************** */

/* Get the output filename-- similar to the other output formats */
static void ffmpeg_filepath_get(FFMpegContext *context,
                                char filepath[FILE_MAX],
                                const RenderData *rd,
                                bool preview,
                                const char *suffix)
{
  char autosplit[20];

  const char **exts = get_file_extensions(rd->ffcodecdata.type);
  const char **fe = exts;
  int sfra, efra;

  if (!filepath || !exts) {
    return;
  }

  if (preview) {
    sfra = rd->psfra;
    efra = rd->pefra;
  }
  else {
    sfra = rd->sfra;
    efra = rd->efra;
  }

  BLI_strncpy(filepath, rd->pic, FILE_MAX);
  BLI_path_abs(filepath, BKE_main_blendfile_path_from_global());

  BLI_file_ensure_parent_dir_exists(filepath);

  autosplit[0] = '\0';

  if ((rd->ffcodecdata.flags & FFMPEG_AUTOSPLIT_OUTPUT) != 0) {
    if (context) {
      SNPRINTF(autosplit, "_%03d", context->ffmpeg_autosplit_count);
    }
  }

  if (rd->scemode & R_EXTENSION) {
    while (*fe) {
      if (BLI_strcasecmp(filepath + strlen(filepath) - strlen(*fe), *fe) == 0) {
        break;
      }
      fe++;
    }

    if (*fe == nullptr) {
      BLI_strncat(filepath, autosplit, FILE_MAX);

      BLI_path_frame_range(filepath, FILE_MAX, sfra, efra, 4);
      BLI_strncat(filepath, *exts, FILE_MAX);
    }
    else {
      *(filepath + strlen(filepath) - strlen(*fe)) = '\0';
      BLI_strncat(filepath, autosplit, FILE_MAX);
      BLI_strncat(filepath, *fe, FILE_MAX);
    }
  }
  else {
    if (BLI_path_frame_check_chars(filepath)) {
      BLI_path_frame_range(filepath, FILE_MAX, sfra, efra, 4);
    }

    BLI_strncat(filepath, autosplit, FILE_MAX);
  }

  BLI_path_suffix(filepath, FILE_MAX, suffix, "");
}

void BKE_ffmpeg_filepath_get(char filepath[/*FILE_MAX*/ 1024],
                             const RenderData *rd,
                             bool preview,
                             const char *suffix)
{
  ffmpeg_filepath_get(nullptr, filepath, rd, preview, suffix);
}

int BKE_ffmpeg_start(void *context_v,
                     const Scene *scene,
                     RenderData *rd,
                     int rectx,
                     int recty,
                     ReportList *reports,
                     bool preview,
                     const char *suffix)
{
  int success;
  FFMpegContext *context = static_cast<FFMpegContext *>(context_v);

  context->ffmpeg_autosplit_count = 0;
  context->ffmpeg_preview = preview;
  context->stamp_data = BKE_stamp_info_from_scene_static(scene);

  success = start_ffmpeg_impl(context, rd, rectx, recty, suffix, reports);
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
        return -31415;
    }

    specs.rate = rd->ffcodecdata.audio_mixrate;
    context->audio_mixdown_device = BKE_sound_mixdown(
        scene, specs, preview ? rd->psfra : rd->sfra, rd->ffcodecdata.audio_volume);
  }
#  endif
  return success;
}

static void end_ffmpeg_impl(FFMpegContext *context, int is_autosplit);

#  ifdef WITH_AUDASPACE
static void write_audio_frames(FFMpegContext *context, double to_pts)
{
  AVCodecContext *c = context->audio_codec;

  while (context->audio_stream) {
    if ((context->audio_time_total >= to_pts) || !write_audio_frame(context)) {
      break;
    }
    context->audio_time_total += double(context->audio_input_samples) / double(c->sample_rate);
    context->audio_time += double(context->audio_input_samples) / double(c->sample_rate);
  }
}
#  endif

int BKE_ffmpeg_append(void *context_v,
                      RenderData *rd,
                      int start_frame,
                      int frame,
                      int *pixels,
                      int rectx,
                      int recty,
                      const char *suffix,
                      ReportList *reports)
{
  FFMpegContext *context = static_cast<FFMpegContext *>(context_v);
  AVFrame *avframe;
  int success = 1;

  PRINT("Writing frame %i, render width=%d, render height=%d\n", frame, rectx, recty);

  if (context->video_stream) {
    avframe = generate_video_frame(context, (uchar *)pixels);
    success = (avframe && write_video_frame(context, avframe, reports));
#  ifdef WITH_AUDASPACE
    /* Add +1 frame because we want to encode audio up until the next video frame. */
    write_audio_frames(
        context, (frame - start_frame + 1) / (double(rd->frs_sec) / double(rd->frs_sec_base)));
#  else
    UNUSED_VARS(start_frame);
#  endif

    if (context->ffmpeg_autosplit) {
      if (avio_tell(context->outfile->pb) > FFMPEG_AUTOSPLIT_SIZE) {
        end_ffmpeg_impl(context, true);
        context->ffmpeg_autosplit_count++;

        success &= start_ffmpeg_impl(context, rd, rectx, recty, suffix, reports);
      }
    }
  }

  return success;
}

static void end_ffmpeg_impl(FFMpegContext *context, int is_autosplit)
{
  PRINT("Closing FFMPEG...\n");

#  ifdef WITH_AUDASPACE
  if (is_autosplit == false) {
    if (context->audio_mixdown_device) {
      AUD_Device_free(context->audio_mixdown_device);
      context->audio_mixdown_device = nullptr;
    }
  }
#  else
  UNUSED_VARS(is_autosplit);
#  endif

  if (context->video_stream) {
    PRINT("Flushing delayed video frames...\n");
    flush_ffmpeg(context->video_codec, context->video_stream, context->outfile);
  }

  if (context->audio_stream) {
    PRINT("Flushing delayed audio frames...\n");
    flush_ffmpeg(context->audio_codec, context->audio_stream, context->outfile);
  }

  if (context->outfile) {
    av_write_trailer(context->outfile);
  }

  /* Close the video codec */

  if (context->video_stream != nullptr) {
    PRINT("zero video stream %p\n", context->video_stream);
    context->video_stream = nullptr;
  }

  if (context->audio_stream != nullptr) {
    context->audio_stream = nullptr;
  }

  /* free the temp buffer */
  if (context->current_frame != nullptr) {
    delete_picture(context->current_frame);
    context->current_frame = nullptr;
  }
  if (context->img_convert_frame != nullptr) {
    delete_picture(context->img_convert_frame);
    context->img_convert_frame = nullptr;
  }

  if (context->outfile != nullptr && context->outfile->oformat) {
    if (!(context->outfile->oformat->flags & AVFMT_NOFILE)) {
      avio_close(context->outfile->pb);
    }
  }

  if (context->video_codec != nullptr) {
    avcodec_free_context(&context->video_codec);
    context->video_codec = nullptr;
  }
  if (context->audio_codec != nullptr) {
    avcodec_free_context(&context->audio_codec);
    context->audio_codec = nullptr;
  }

  if (context->outfile != nullptr) {
    avformat_free_context(context->outfile);
    context->outfile = nullptr;
  }
  if (context->audio_input_buffer != nullptr) {
    av_free(context->audio_input_buffer);
    context->audio_input_buffer = nullptr;
  }

  if (context->audio_deinterleave_buffer != nullptr) {
    av_free(context->audio_deinterleave_buffer);
    context->audio_deinterleave_buffer = nullptr;
  }

  if (context->img_convert_ctx != nullptr) {
    sws_freeContext(context->img_convert_ctx);
    context->img_convert_ctx = nullptr;
  }
}

void BKE_ffmpeg_end(void *context_v)
{
  FFMpegContext *context = static_cast<FFMpegContext *>(context_v);
  end_ffmpeg_impl(context, false);
}

void BKE_ffmpeg_preset_set(RenderData *rd, int preset)
{
  bool is_ntsc = (rd->frs_sec != 25);

  switch (preset) {
    case FFMPEG_PRESET_H264:
      rd->ffcodecdata.type = FFMPEG_AVI;
      rd->ffcodecdata.codec = AV_CODEC_ID_H264;
      rd->ffcodecdata.video_bitrate = 6000;
      rd->ffcodecdata.gop_size = is_ntsc ? 18 : 15;
      rd->ffcodecdata.rc_max_rate = 9000;
      rd->ffcodecdata.rc_min_rate = 0;
      rd->ffcodecdata.rc_buffer_size = 224 * 8;
      rd->ffcodecdata.mux_packet_size = 2048;
      rd->ffcodecdata.mux_rate = 10080000;
      break;

    case FFMPEG_PRESET_THEORA:
    case FFMPEG_PRESET_XVID:
      if (preset == FFMPEG_PRESET_XVID) {
        rd->ffcodecdata.type = FFMPEG_AVI;
        rd->ffcodecdata.codec = AV_CODEC_ID_MPEG4;
      }
      else if (preset == FFMPEG_PRESET_THEORA) {
        rd->ffcodecdata.type = FFMPEG_OGG; /* XXX broken */
        rd->ffcodecdata.codec = AV_CODEC_ID_THEORA;
      }

      rd->ffcodecdata.video_bitrate = 6000;
      rd->ffcodecdata.gop_size = is_ntsc ? 18 : 15;
      rd->ffcodecdata.rc_max_rate = 9000;
      rd->ffcodecdata.rc_min_rate = 0;
      rd->ffcodecdata.rc_buffer_size = 224 * 8;
      rd->ffcodecdata.mux_packet_size = 2048;
      rd->ffcodecdata.mux_rate = 10080000;
      break;

    case FFMPEG_PRESET_AV1:
      rd->ffcodecdata.type = FFMPEG_AV1;
      rd->ffcodecdata.codec = AV_CODEC_ID_AV1;
      rd->ffcodecdata.video_bitrate = 6000;
      rd->ffcodecdata.gop_size = is_ntsc ? 18 : 15;
      rd->ffcodecdata.rc_max_rate = 9000;
      rd->ffcodecdata.rc_min_rate = 0;
      rd->ffcodecdata.rc_buffer_size = 224 * 8;
      rd->ffcodecdata.mux_packet_size = 2048;
      rd->ffcodecdata.mux_rate = 10080000;
      break;
  }
}

void BKE_ffmpeg_image_type_verify(RenderData *rd, const ImageFormatData *imf)
{
  int audio = 0;

  if (imf->imtype == R_IMF_IMTYPE_FFMPEG) {
    if (rd->ffcodecdata.type <= 0 || rd->ffcodecdata.codec <= 0 ||
        rd->ffcodecdata.audio_codec <= 0 || rd->ffcodecdata.video_bitrate <= 1)
    {
      BKE_ffmpeg_preset_set(rd, FFMPEG_PRESET_H264);
      rd->ffcodecdata.constant_rate_factor = FFM_CRF_MEDIUM;
      rd->ffcodecdata.ffmpeg_preset = FFM_PRESET_GOOD;
      rd->ffcodecdata.type = FFMPEG_MKV;
    }
    if (rd->ffcodecdata.type == FFMPEG_OGG) {
      rd->ffcodecdata.type = FFMPEG_MPEG2;
    }

    audio = 1;
  }
  else if (imf->imtype == R_IMF_IMTYPE_H264) {
    if (rd->ffcodecdata.codec != AV_CODEC_ID_H264) {
      BKE_ffmpeg_preset_set(rd, FFMPEG_PRESET_H264);
      audio = 1;
    }
  }
  else if (imf->imtype == R_IMF_IMTYPE_XVID) {
    if (rd->ffcodecdata.codec != AV_CODEC_ID_MPEG4) {
      BKE_ffmpeg_preset_set(rd, FFMPEG_PRESET_XVID);
      audio = 1;
    }
  }
  else if (imf->imtype == R_IMF_IMTYPE_THEORA) {
    if (rd->ffcodecdata.codec != AV_CODEC_ID_THEORA) {
      BKE_ffmpeg_preset_set(rd, FFMPEG_PRESET_THEORA);
      audio = 1;
    }
  }
  else if (imf->imtype == R_IMF_IMTYPE_AV1) {
    if (rd->ffcodecdata.codec != AV_CODEC_ID_AV1) {
      BKE_ffmpeg_preset_set(rd, FFMPEG_PRESET_AV1);
      audio = 1;
    }
  }

  if (audio && rd->ffcodecdata.audio_codec < 0) {
    rd->ffcodecdata.audio_codec = AV_CODEC_ID_NONE;
    rd->ffcodecdata.audio_bitrate = 128;
  }
}

bool BKE_ffmpeg_alpha_channel_is_supported(const RenderData *rd)
{
  int codec = rd->ffcodecdata.codec;

  return ELEM(codec,
              AV_CODEC_ID_FFV1,
              AV_CODEC_ID_QTRLE,
              AV_CODEC_ID_PNG,
              AV_CODEC_ID_VP9,
              AV_CODEC_ID_HUFFYUV);
}

void *BKE_ffmpeg_context_create()
{
  /* New FFMPEG data struct. */
  FFMpegContext *context = static_cast<FFMpegContext *>(
      MEM_callocN(sizeof(FFMpegContext), "new FFMPEG context"));

  context->ffmpeg_codec = AV_CODEC_ID_MPEG4;
  context->ffmpeg_audio_codec = AV_CODEC_ID_NONE;
  context->ffmpeg_video_bitrate = 1150;
  context->ffmpeg_audio_bitrate = 128;
  context->ffmpeg_gop_size = 12;
  context->ffmpeg_autosplit = 0;
  context->ffmpeg_autosplit_count = 0;
  context->ffmpeg_preview = false;
  context->stamp_data = nullptr;
  context->audio_time_total = 0.0;

  return context;
}

void BKE_ffmpeg_context_free(void *context_v)
{
  FFMpegContext *context = static_cast<FFMpegContext *>(context_v);
  if (context == nullptr) {
    return;
  }
  if (context->stamp_data) {
    MEM_freeN(context->stamp_data);
  }
  MEM_freeN(context);
}

#endif /* WITH_FFMPEG */
