/* SPDX-FileCopyrightText: 2006 Peter Schlaile.
 * SPDX-FileCopyrightText: 2023-2024 Blender Authors
 *
 * SPDX-License-Identifier: GPL-2.0-or-later */

/** \file
 * \ingroup imbuf
 */

#include "movie_write.hh"

#include "DNA_scene_types.h"

#include "MOV_write.hh"

#include "BKE_report.hh"

#ifdef WITH_FFMPEG
#  include <cstdio>
#  include <cstring>

#  include "MEM_guardedalloc.h"

#  include "BLI_endian_defines.h"
#  include "BLI_fileops.h"
#  include "BLI_math_base.h"
#  include "BLI_path_utils.hh"
#  include "BLI_string.h"
#  include "BLI_threads.h"
#  include "BLI_utildefines.h"

#  include "BKE_global.hh"
#  include "BKE_image.hh"
#  include "BKE_main.hh"

#  include "IMB_imbuf.hh"

#  include "MOV_enums.hh"
#  include "MOV_util.hh"

#  include "ffmpeg_swscale.hh"
#  include "movie_util.hh"

static constexpr int64_t ffmpeg_autosplit_size = 2'000'000'000;

#  define FF_DEBUG_PRINT \
    if (G.debug & G_DEBUG_FFMPEG) \
    printf

static void ffmpeg_dict_set_int(AVDictionary **dict, const char *key, int value)
{
  av_dict_set_int(dict, key, value, 0);
}

static void ffmpeg_movie_close(MovieWriter *context);
static void ffmpeg_filepath_get(MovieWriter *context,
                                char filepath[FILE_MAX],
                                const RenderData *rd,
                                bool preview,
                                const char *suffix);

static AVFrame *alloc_frame(AVPixelFormat pix_fmt, int width, int height)
{
  AVFrame *f = av_frame_alloc();
  if (f == nullptr) {
    return nullptr;
  }
  const size_t align = ffmpeg_get_buffer_alignment();
  f->format = pix_fmt;
  f->width = width;
  f->height = height;
  if (av_frame_get_buffer(f, align) < 0) {
    av_frame_free(&f);
    return nullptr;
  }
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
static bool write_video_frame(MovieWriter *context, AVFrame *frame, ReportList *reports)
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
    FF_DEBUG_PRINT("ffmpeg: error writing video frame: %s\n", error_str);
  }

  av_packet_free(&packet);

  return success;
}

/* read and encode a frame of video from the buffer */
static AVFrame *generate_video_frame(MovieWriter *context, const ImBuf *image)
{
  const uint8_t *pixels = image->byte_buffer.data;
  const float *pixels_fl = image->float_buffer.data;
  /* Use float input if needed. */
  const bool use_float = context->img_convert_frame != nullptr &&
                         context->img_convert_frame->format != AV_PIX_FMT_RGBA;
  if ((!use_float && (pixels == nullptr)) || (use_float && (pixels_fl == nullptr))) {
    return nullptr;
  }

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

  /* Ensure frame is writable. Some video codecs might have made previous frame
   * shared (i.e. not writable). */
  av_frame_make_writable(rgb_frame);

  const size_t linesize_dst = rgb_frame->linesize[0];
  if (use_float) {
    /* Float image: need to split up the image into a planar format,
     * because `libswscale` does not support RGBA->YUV conversions from
     * packed float formats. */
    BLI_assert_msg(rgb_frame->linesize[1] == linesize_dst &&
                       rgb_frame->linesize[2] == linesize_dst &&
                       rgb_frame->linesize[3] == linesize_dst,
                   "ffmpeg frame should be 4 same size planes for a floating point image case");
    for (int y = 0; y < height; y++) {
      size_t dst_offset = linesize_dst * (height - y - 1);
      float *dst_g = reinterpret_cast<float *>(rgb_frame->data[0] + dst_offset);
      float *dst_b = reinterpret_cast<float *>(rgb_frame->data[1] + dst_offset);
      float *dst_r = reinterpret_cast<float *>(rgb_frame->data[2] + dst_offset);
      float *dst_a = reinterpret_cast<float *>(rgb_frame->data[3] + dst_offset);
      const float *src = pixels_fl + image->x * y * 4;
      for (int x = 0; x < image->x; x++) {
        *dst_r++ = src[0];
        *dst_g++ = src[1];
        *dst_b++ = src[2];
        *dst_a++ = src[3];
        src += 4;
      }
    }
  }
  else {
    /* Byte image: flip the image vertically, possibly with endian
     * conversion. */
    const size_t linesize_src = rgb_frame->width * 4;
    for (int y = 0; y < height; y++) {
      uint8_t *target = rgb_frame->data[0] + linesize_dst * (height - y - 1);
      const uint8_t *src = pixels + linesize_src * y;

#  if ENDIAN_ORDER == L_ENDIAN
      memcpy(target, src, linesize_src);

#  elif ENDIAN_ORDER == B_ENDIAN
      const uint8_t *end = src + linesize_src;
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
  }

  /* Convert to the output pixel format, if it's different that Blender's internal one. */
  if (context->img_convert_frame != nullptr) {
    BLI_assert(context->img_convert_ctx != nullptr);
    /* Ensure the frame we are scaling to is writable as well. */
    av_frame_make_writable(context->current_frame);
    ffmpeg_sws_scale_frame(context->img_convert_ctx, context->current_frame, rgb_frame);
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
    MovieWriter *context, RenderData *rd, AVDictionary **opts, int rectx, int recty)
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
    }
  }

  return codec;
}

/* Remap H.264 CRF to H.265 CRF: 17..32 range (23 default) to 20..37 range (28 default).
 * https://trac.ffmpeg.org/wiki/Encode/H.265 */
static int remap_crf_to_h265_crf(int crf, bool is_10_or_12_bpp)
{
  /* 10/12 bit videos seem to need slightly lower CRF value for similar quality. */
  const int bias = is_10_or_12_bpp ? -3 : 0;
  switch (crf) {
    case FFM_CRF_PERC_LOSSLESS:
      return 20 + bias;
    case FFM_CRF_HIGH:
      return 24 + bias;
    case FFM_CRF_MEDIUM:
      return 28 + bias;
    case FFM_CRF_LOW:
      return 31 + bias;
    case FFM_CRF_VERYLOW:
      return 34 + bias;
    case FFM_CRF_LOWEST:
      return 37 + bias;
  }
  return crf;
}

/* 10bpp H264: remap 0..51 range to -12..51 range
 * https://trac.ffmpeg.org/wiki/Encode/H.264#a1.ChooseaCRFvalue */
static int remap_crf_to_h264_10bpp_crf(int crf)
{
  crf = int(-12.0f + (crf / 51.0f) * 63.0f);
  crf = max_ii(crf, 0);
  return crf;
}

static void set_quality_rate_options(const MovieWriter *context,
                                     const AVCodecID codec_id,
                                     const RenderData *rd,
                                     AVDictionary **opts)
{
  AVCodecContext *c = context->video_codec;

  /* Handle constant bit rate (CBR) case. */
  if (!MOV_codec_supports_crf(codec_id) || context->ffmpeg_crf < 0) {
    c->bit_rate = context->ffmpeg_video_bitrate * 1000;
    c->rc_max_rate = rd->ffcodecdata.rc_max_rate * 1000;
    c->rc_min_rate = rd->ffcodecdata.rc_min_rate * 1000;
    c->rc_buffer_size = rd->ffcodecdata.rc_buffer_size * 1024;
    return;
  }

  /* For VP9 bit rate must be set to zero to get CRF mode, just set it to zero for all codecs:
   * https://trac.ffmpeg.org/wiki/Encode/VP9 */
  c->bit_rate = 0;

  const bool is_10_bpp = rd->im_format.depth == R_IMF_CHAN_DEPTH_10;
  const bool is_12_bpp = rd->im_format.depth == R_IMF_CHAN_DEPTH_12;
  const bool av1_librav1e = codec_id == AV_CODEC_ID_AV1 && STREQ(c->codec->name, "librav1e");
  const bool av1_libsvtav1 = codec_id == AV_CODEC_ID_AV1 && STREQ(c->codec->name, "libsvtav1");

  /* Handle "lossless" case. */
  if (context->ffmpeg_crf == FFM_CRF_LOSSLESS) {
    if (codec_id == AV_CODEC_ID_VP9) {
      /* VP9 needs "lossless": https://trac.ffmpeg.org/wiki/Encode/VP9#LosslessVP9 */
      ffmpeg_dict_set_int(opts, "lossless", 1);
    }
    else if (codec_id == AV_CODEC_ID_H264 && is_10_bpp) {
      /* 10bpp H264 needs "qp": https://trac.ffmpeg.org/wiki/Encode/H.264#a1.ChooseaCRFvalue */
      ffmpeg_dict_set_int(opts, "qp", 0);
    }
    else if (codec_id == AV_CODEC_ID_H265) {
      /* H.265 needs "lossless" in private params; also make it much less verbose. */
      av_dict_set(opts, "x265-params", "log-level=1:lossless=1", 0);
    }
    else if (codec_id == AV_CODEC_ID_AV1 && (av1_librav1e || av1_libsvtav1)) {
      /* AV1 in some encoders needs qp=0 for lossless. */
      ffmpeg_dict_set_int(opts, "qp", 0);
    }
    else {
      /* For others crf=0 means lossless. */
      ffmpeg_dict_set_int(opts, "crf", 0);
    }
    return;
  }

  /* Handle CRF setting cases. */
  int crf = context->ffmpeg_crf;

  if (codec_id == AV_CODEC_ID_H264 && is_10_bpp) {
    crf = remap_crf_to_h264_10bpp_crf(crf);
  }
  else if (codec_id == AV_CODEC_ID_H265) {
    crf = remap_crf_to_h265_crf(crf, is_10_bpp || is_12_bpp);
    /* Make H.265 much less verbose. */
    av_dict_set(opts, "x265-params", "log-level=1", 0);
  }

  if (av1_librav1e) {
    /* Remap crf 0..51 to qp 0..255 for AV1 librav1e. */
    int qp = int(float(crf) / 51.0f * 255.0f);
    qp = clamp_i(qp, 0, 255);
    ffmpeg_dict_set_int(opts, "qp", qp);
  }
  else if (av1_libsvtav1) {
    /* libsvtav1 used to take CRF as "qp" parameter, do that. */
    ffmpeg_dict_set_int(opts, "qp", crf);
  }
  else {
    ffmpeg_dict_set_int(opts, "crf", crf);
  }
}

static AVStream *alloc_video_stream(MovieWriter *context,
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

  set_quality_rate_options(context, codec_id, rd, &opts);

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

  const bool is_10_bpp = rd->im_format.depth == R_IMF_CHAN_DEPTH_10;
  const bool is_12_bpp = rd->im_format.depth == R_IMF_CHAN_DEPTH_12;
  if (is_10_bpp) {
    c->pix_fmt = AV_PIX_FMT_YUV420P10LE;
  }
  else if (is_12_bpp) {
    c->pix_fmt = AV_PIX_FMT_YUV420P12LE;
  }

  if (context->ffmpeg_type == FFMPEG_XVID) {
    /* Alas! */
    c->pix_fmt = AV_PIX_FMT_YUV420P;
    c->codec_tag = (('D' << 24) + ('I' << 16) + ('V' << 8) + 'X');
  }

  if (codec_id == AV_CODEC_ID_H265) {
    /* H.265 needs hvc1 tag for Apple compatibility, see
     * https://trac.ffmpeg.org/wiki/Encode/H.265#FinalCutandApplestuffcompatibility
     * Note that in case we are doing H.265 into an XviD container,
     * this overwrites the tag set above. But that should not be what anyone does. */
    c->codec_tag = MKTAG('h', 'v', 'c', '1');
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
  else if (ELEM(codec_id, AV_CODEC_ID_H264, AV_CODEC_ID_H265, AV_CODEC_ID_VP9, AV_CODEC_ID_AV1) &&
           (context->ffmpeg_crf == 0))
  {
    /* Use 4:4:4 instead of 4:2:0 pixel format for lossless rendering. */
    c->pix_fmt = AV_PIX_FMT_YUV444P;
    if (is_10_bpp) {
      c->pix_fmt = AV_PIX_FMT_YUV444P10LE;
    }
    else if (is_12_bpp) {
      c->pix_fmt = AV_PIX_FMT_YUV444P12LE;
    }
  }

  if (codec_id == AV_CODEC_ID_PNG) {
    if (rd->im_format.planes == R_IMF_PLANES_RGBA) {
      c->pix_fmt = AV_PIX_FMT_RGBA;
    }
  }

  if (of->oformat->flags & AVFMT_GLOBALHEADER) {
    FF_DEBUG_PRINT("ffmpeg: using global video header\n");
    c->flags |= AV_CODEC_FLAG_GLOBAL_HEADER;
  }

  /* If output pixel format is not RGB(A), setup colorspace metadata. */
  const AVPixFmtDescriptor *pix_fmt_desc = av_pix_fmt_desc_get(c->pix_fmt);
  const bool set_bt709 = (pix_fmt_desc->flags & AV_PIX_FMT_FLAG_RGB) == 0;
  if (set_bt709) {
    c->color_range = AVCOL_RANGE_MPEG;
    c->color_primaries = AVCOL_PRI_BT709;
    c->color_trc = AVCOL_TRC_BT709;
    c->colorspace = AVCOL_SPC_BT709;
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
    BLI_strncpy(error, ffmpeg_last_error(), error_size);
    av_dict_free(&opts);
    avcodec_free_context(&c);
    context->video_codec = nullptr;
    return nullptr;
  }
  av_dict_free(&opts);

  /* FFMPEG expects its data in the output pixel format. */
  context->current_frame = alloc_frame(c->pix_fmt, c->width, c->height);

  if (c->pix_fmt == AV_PIX_FMT_RGBA) {
    /* Output pixel format is the same we use internally, no conversion necessary. */
    context->img_convert_frame = nullptr;
    context->img_convert_ctx = nullptr;
  }
  else {
    /* Output pixel format is different, allocate frame for conversion. */
    AVPixelFormat src_format = is_10_bpp || is_12_bpp ? AV_PIX_FMT_GBRAPF32LE : AV_PIX_FMT_RGBA;
    context->img_convert_frame = alloc_frame(src_format, c->width, c->height);
    context->img_convert_ctx = ffmpeg_sws_get_context(
        c->width, c->height, src_format, c->width, c->height, c->pix_fmt, SWS_BICUBIC);

    /* Setup BT.709 coefficients for RGB->YUV conversion, if needed. */
    if (set_bt709) {
      int *inv_table = nullptr, *table = nullptr;
      int src_range = 0, dst_range = 0, brightness = 0, contrast = 0, saturation = 0;
      sws_getColorspaceDetails(context->img_convert_ctx,
                               &inv_table,
                               &src_range,
                               &table,
                               &dst_range,
                               &brightness,
                               &contrast,
                               &saturation);
      const int *new_table = sws_getCoefficients(AVCOL_SPC_BT709);
      sws_setColorspaceDetails(context->img_convert_ctx,
                               inv_table,
                               src_range,
                               new_table,
                               dst_range,
                               brightness,
                               contrast,
                               saturation);
    }
  }

  avcodec_parameters_from_context(st->codecpar, c);

  context->video_time = 0.0f;

  return st;
}

static void ffmpeg_add_metadata_callback(void *data,
                                         const char *propname,
                                         char *propvalue,
                                         int /*propvalue_maxncpy*/)
{
  AVDictionary **metadata = (AVDictionary **)data;
  av_dict_set(metadata, propname, propvalue, 0);
}

static bool start_ffmpeg_impl(MovieWriter *context,
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
  context->ffmpeg_autosplit = (rd->ffcodecdata.flags & FFMPEG_AUTOSPLIT_OUTPUT) != 0;
  context->ffmpeg_crf = rd->ffcodecdata.constant_rate_factor;
  context->ffmpeg_preset = rd->ffcodecdata.ffmpeg_preset;

  if ((rd->ffcodecdata.flags & FFMPEG_USE_MAX_B_FRAMES) != 0) {
    context->ffmpeg_max_b_frames = rd->ffcodecdata.max_b_frames;
  }

  /* Determine the correct filename */
  ffmpeg_filepath_get(context, filepath, rd, context->ffmpeg_preview, suffix);
  FF_DEBUG_PRINT(
      "ffmpeg: starting output to %s:\n"
      "  type=%d, codec=%d, audio_codec=%d,\n"
      "  video_bitrate=%d, audio_bitrate=%d,\n"
      "  gop_size=%d, autosplit=%d\n"
      "  width=%d, height=%d\n",
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
    return false;
  }

  fmt = av_guess_format(nullptr, exts[0], nullptr);
  if (!fmt) {
    BKE_report(reports, RPT_ERROR, "No valid formats found");
    return false;
  }

  of = avformat_alloc_context();
  if (!of) {
    BKE_report(reports, RPT_ERROR, "Can't allocate FFmpeg format context");
    return false;
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
       * Currently we expect these to be `.avi`, `.mov`, `.mkv`, and `.mp4`. */
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
    FF_DEBUG_PRINT("ffmpeg: alloc video stream %p\n", context->video_stream);
    if (!context->video_stream) {
      if (error[0]) {
        BKE_report(reports, RPT_ERROR, error);
        FF_DEBUG_PRINT("ffmpeg: video stream error: %s\n", error);
      }
      else {
        BKE_report(reports, RPT_ERROR, "Error initializing video stream");
        FF_DEBUG_PRINT("ffmpeg: error initializing video stream\n");
      }
      goto fail;
    }
  }

  if (context->ffmpeg_audio_codec != AV_CODEC_ID_NONE) {
    context->audio_stream = alloc_audio_stream(context,
                                               rd->ffcodecdata.audio_mixrate,
                                               rd->ffcodecdata.audio_channels,
                                               audio_codec,
                                               of,
                                               error,
                                               sizeof(error));
    if (!context->audio_stream) {
      if (error[0]) {
        BKE_report(reports, RPT_ERROR, error);
        FF_DEBUG_PRINT("ffmpeg: audio stream error: %s\n", error);
      }
      else {
        BKE_report(reports, RPT_ERROR, "Error initializing audio stream");
        FF_DEBUG_PRINT("ffmpeg: error initializing audio stream\n");
      }
      goto fail;
    }
  }
  if (!(fmt->flags & AVFMT_NOFILE)) {
    if (avio_open(&of->pb, filepath, AVIO_FLAG_WRITE) < 0) {
      BKE_report(reports, RPT_ERROR, "Could not open file for writing");
      FF_DEBUG_PRINT("ffmpeg: could not open file %s for writing\n", filepath);
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
    FF_DEBUG_PRINT("ffmpeg: could not write media header: %s\n", error_str);
    goto fail;
  }

  context->outfile = of;
  av_dump_format(of, 0, filepath, 1);

  return true;

fail:
  if (of->pb) {
    avio_close(of->pb);
  }

  context->video_stream = nullptr;
  context->audio_stream = nullptr;

  avformat_free_context(of);
  return false;
}

/* Flush any pending frames. An encoder may use both past and future frames
 * to predict inter-frames (H.264 B-frames, for example); it can output
 * the frames in a different order from the one it was given. The delayed
 * frames must be flushed before we close the stream. */
static void flush_delayed_frames(AVCodecContext *c, AVStream *stream, AVFormatContext *outfile)
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

/* Get the output filename-- similar to the other output formats */
static void ffmpeg_filepath_get(MovieWriter *context,
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

static void ffmpeg_get_filepath(char filepath[/*FILE_MAX*/ 1024],
                                const RenderData *rd,
                                bool preview,
                                const char *suffix)
{
  ffmpeg_filepath_get(nullptr, filepath, rd, preview, suffix);
}

static MovieWriter *ffmpeg_movie_open(const Scene *scene,
                                      RenderData *rd,
                                      int rectx,
                                      int recty,
                                      ReportList *reports,
                                      bool preview,
                                      const char *suffix)
{
  MovieWriter *context = MEM_new<MovieWriter>("new FFMPEG context");

  context->ffmpeg_codec = AV_CODEC_ID_MPEG4;
  context->ffmpeg_audio_codec = AV_CODEC_ID_NONE;
  context->ffmpeg_video_bitrate = 1150;
  context->ffmpeg_audio_bitrate = 128;
  context->ffmpeg_gop_size = 12;
  context->ffmpeg_autosplit = false;
  context->stamp_data = nullptr;
  context->audio_time_total = 0.0;

  context->ffmpeg_autosplit_count = 0;
  context->ffmpeg_preview = preview;
  context->stamp_data = BKE_stamp_info_from_scene_static(scene);

  bool success = start_ffmpeg_impl(context, rd, rectx, recty, suffix, reports);

  if (success) {
    success = movie_audio_open(context,
                               scene,
                               preview ? rd->psfra : rd->sfra,
                               rd->ffcodecdata.audio_mixrate,
                               rd->ffcodecdata.audio_volume,
                               reports);
  }

  if (!success) {
    ffmpeg_movie_close(context);
    return nullptr;
  }
  return context;
}

static void end_ffmpeg_impl(MovieWriter *context, bool is_autosplit);

static bool ffmpeg_movie_append(MovieWriter *context,
                                RenderData *rd,
                                int start_frame,
                                int frame,
                                const ImBuf *image,
                                const char *suffix,
                                ReportList *reports)
{
  AVFrame *avframe;
  bool success = true;

  FF_DEBUG_PRINT("ffmpeg: writing frame #%i (%ix%i)\n", frame, image->x, image->y);

  if (context->video_stream) {
    avframe = generate_video_frame(context, image);
    success = (avframe && write_video_frame(context, avframe, reports));
  }

  if (context->audio_stream) {
    /* Add +1 frame because we want to encode audio up until the next video frame. */
    write_audio_frames(
        context, (frame - start_frame + 1) / (double(rd->frs_sec) / double(rd->frs_sec_base)));
  }

  if (context->ffmpeg_autosplit) {
    if (avio_tell(context->outfile->pb) > ffmpeg_autosplit_size) {
      end_ffmpeg_impl(context, true);
      context->ffmpeg_autosplit_count++;

      success &= start_ffmpeg_impl(context, rd, image->x, image->y, suffix, reports);
    }
  }

  return success;
}

static void end_ffmpeg_impl(MovieWriter *context, bool is_autosplit)
{
  FF_DEBUG_PRINT("ffmpeg: closing\n");

  movie_audio_close(context, is_autosplit);

  if (context->video_stream) {
    FF_DEBUG_PRINT("ffmpeg: flush delayed video frames\n");
    flush_delayed_frames(context->video_codec, context->video_stream, context->outfile);
  }

  if (context->audio_stream) {
    FF_DEBUG_PRINT("ffmpeg: flush delayed audio frames\n");
    flush_delayed_frames(context->audio_codec, context->audio_stream, context->outfile);
  }

  if (context->outfile) {
    av_write_trailer(context->outfile);
  }

  /* Close the video codec */

  context->video_stream = nullptr;
  context->audio_stream = nullptr;

  av_frame_free(&context->current_frame);
  av_frame_free(&context->img_convert_frame);

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
    ffmpeg_sws_release_context(context->img_convert_ctx);
    context->img_convert_ctx = nullptr;
  }
}

static void ffmpeg_movie_close(MovieWriter *context)
{
  if (context == nullptr) {
    return;
  }
  end_ffmpeg_impl(context, false);
  if (context->stamp_data) {
    BKE_stamp_data_free(context->stamp_data);
  }
  MEM_delete(context);
}

#endif /* WITH_FFMPEG */

static bool is_imtype_ffmpeg(const char imtype)
{
  return ELEM(imtype,
              R_IMF_IMTYPE_AVIRAW,
              R_IMF_IMTYPE_AVIJPEG,
              R_IMF_IMTYPE_FFMPEG,
              R_IMF_IMTYPE_H264,
              R_IMF_IMTYPE_XVID,
              R_IMF_IMTYPE_THEORA,
              R_IMF_IMTYPE_AV1);
}

MovieWriter *MOV_write_begin(const char imtype,
                             const Scene *scene,
                             RenderData *rd,
                             int rectx,
                             int recty,
                             ReportList *reports,
                             bool preview,
                             const char *suffix)
{
  if (!is_imtype_ffmpeg(imtype)) {
    BKE_report(reports, RPT_ERROR, "Image format is not a movie format");
    return nullptr;
  }

  MovieWriter *writer = nullptr;
#ifdef WITH_FFMPEG
  writer = ffmpeg_movie_open(scene, rd, rectx, recty, reports, preview, suffix);
#else
  UNUSED_VARS(scene, rd, rectx, recty, reports, preview, suffix);
#endif
  return writer;
}

bool MOV_write_append(MovieWriter *writer,
                      RenderData *rd,
                      int start_frame,
                      int frame,
                      const ImBuf *image,
                      const char *suffix,
                      ReportList *reports)
{
  if (writer == nullptr) {
    return false;
  }

#ifdef WITH_FFMPEG
  bool ok = ffmpeg_movie_append(writer, rd, start_frame, frame, image, suffix, reports);
  return ok;
#else
  UNUSED_VARS(rd, start_frame, frame, image, suffix, reports);
  return false;
#endif
}

void MOV_write_end(MovieWriter *writer)
{
#ifdef WITH_FFMPEG
  if (writer) {
    ffmpeg_movie_close(writer);
  }
#else
  UNUSED_VARS(writer);
#endif
}

void MOV_filepath_from_settings(char filepath[/*FILE_MAX*/ 1024],
                                const RenderData *rd,
                                bool preview,
                                const char *suffix)
{
#ifdef WITH_FFMPEG
  if (is_imtype_ffmpeg(rd->im_format.imtype)) {
    ffmpeg_get_filepath(filepath, rd, preview, suffix);
    return;
  }
#else
  UNUSED_VARS(rd, preview, suffix);
#endif
  filepath[0] = '\0';
}
