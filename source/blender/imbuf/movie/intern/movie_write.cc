/* SPDX-FileCopyrightText: 2006 Peter Schlaile.
 * SPDX-FileCopyrightText: 2023-2024 Blender Authors
 *
 * SPDX-License-Identifier: GPL-2.0-or-later */

/** \file
 * \ingroup imbuf
 */

#include "movie_write.hh"

#include "BLI_string_ref.hh"

#include "DNA_scene_types.h"

#include "MOV_write.hh"

#include "BKE_report.hh"

#ifdef WITH_FFMPEG
#  include <cstdio>
#  include <cstring>

#  include "MEM_guardedalloc.h"

#  include "BLI_fileops.h"
#  include "BLI_math_base.h"
#  include "BLI_math_color.h"
#  include "BLI_path_utils.hh"
#  include "BLI_string.h"
#  include "BLI_string_utf8.h"
#  include "BLI_utildefines.h"

#  include "BKE_image.hh"
#  include "BKE_main.hh"
#  include "BKE_path_templates.hh"

#  include "IMB_imbuf.hh"

#  include "MOV_enums.hh"
#  include "MOV_util.hh"

#  include "IMB_colormanagement.hh"

#  include "CLG_log.h"

#  include "ffmpeg_swscale.hh"
#  include "movie_util.hh"

static CLG_LogRef LOG = {"video.write"};
static constexpr int64_t ffmpeg_autosplit_size = 2'000'000'000;

static void ffmpeg_dict_set_int(AVDictionary **dict, const char *key, int value)
{
  av_dict_set_int(dict, key, value, 0);
}

static void ffmpeg_movie_close(MovieWriter *context);
static bool ffmpeg_filepath_get(MovieWriter *context,
                                char filepath[FILE_MAX],
                                const Scene *scene,
                                const RenderData *rd,
                                bool preview,
                                const char *suffix,
                                ReportList *reports);

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

static void add_hdr_mastering_display_metadata(AVCodecParameters *codecpar,
                                               AVCodecContext *c,
                                               const ImageFormatData *imf)
{
  if (c->color_primaries != AVCOL_PRI_BT2020) {
    return;
  }

  int max_luminance = 0;
  if (c->color_trc == AVCOL_TRC_ARIB_STD_B67) {
    /* HLG is always 1000 nits. */
    max_luminance = 1000;
  }
  else if (c->color_trc == AVCOL_TRC_SMPTEST2084) {
    /* PQ uses heuristic based on view transform name. In the future this could become
     * a user control, but this solves the common cases. */
    blender::StringRefNull view_name = imf->view_settings.view_transform;
    if (view_name.find("HDR 500 nits") != blender::StringRef::not_found) {
      max_luminance = 500;
    }
    else if (view_name.find("HDR 1000 nits") != blender::StringRef::not_found) {
      max_luminance = 1000;
    }
    else if (view_name.find("HDR 2000 nits") != blender::StringRef::not_found) {
      max_luminance = 2000;
    }
    else if (view_name.find("HDR 4000 nits") != blender::StringRef::not_found) {
      max_luminance = 4000;
    }
    else if (view_name.find("HDR 10000 nits") != blender::StringRef::not_found) {
      max_luminance = 10000;
    }
  }

  /* If we don't know anything, don't write metadata. The video player will make some
   * default assumption, often 1000 nits. */
  if (max_luminance == 0) {
    return;
  }

  AVPacketSideData *side_data = av_packet_side_data_new(&codecpar->coded_side_data,
                                                        &codecpar->nb_coded_side_data,
                                                        AV_PKT_DATA_MASTERING_DISPLAY_METADATA,
                                                        sizeof(AVMasteringDisplayMetadata),
                                                        0);
  if (side_data == nullptr) {
    CLOG_ERROR(&LOG, "Failed to attached mastering display metadata to stream");
    return;
  }

  AVMasteringDisplayMetadata *mastering_metadata = reinterpret_cast<AVMasteringDisplayMetadata *>(
      side_data->data);

  /* Rec.2020 primaries and D65 white point. */
  mastering_metadata->has_primaries = 1;
  mastering_metadata->display_primaries[0][0] = av_make_q(34000, 50000);
  mastering_metadata->display_primaries[0][1] = av_make_q(16000, 50000);
  mastering_metadata->display_primaries[1][0] = av_make_q(13250, 50000);
  mastering_metadata->display_primaries[1][1] = av_make_q(34500, 50000);
  mastering_metadata->display_primaries[2][0] = av_make_q(7500, 50000);
  mastering_metadata->display_primaries[2][1] = av_make_q(3000, 50000);

  mastering_metadata->white_point[0] = av_make_q(15635, 50000);
  mastering_metadata->white_point[1] = av_make_q(16450, 50000);

  mastering_metadata->has_luminance = 1;
  mastering_metadata->min_luminance = av_make_q(1, 10000);
  mastering_metadata->max_luminance = av_make_q(max_luminance, 1);
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
    CLOG_ERROR(&LOG, "Can't send video frame: %s", error_str);
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
      CLOG_ERROR(&LOG, "Error encoding frame: %s", error_str);
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
    CLOG_INFO(&LOG, "ffmpeg: error writing video frame: %s", error_str);
  }

  av_packet_free(&packet);

  return success;
}

/* Allocate new ImBuf of the size of the given input which only contains float buffer with pixels
 * from the input.
 *
 * For the float image buffers it is similar to IMB_dupImBuf() but it ensures that the byte buffer
 * is not allocated.
 *
 * For the byte image buffers it is similar to IMB_dupImBuf() followed by IMB_float_from_byte(),
 * but without temporary allocation, and result containing only single float buffer.
 *
 * No color space conversion is performed. The result float buffer might be in a non-linear space
 * denoted by the float_buffer.colorspace. */
static ImBuf *alloc_imbuf_for_colorspace_transform(const ImBuf *input_ibuf)
{
  if (!input_ibuf) {
    return nullptr;
  }

  /* Allocate new image buffer without float buffer just yet.
   * This allows to properly initialize the number of channels used in the buffer. */
  /* TODO(sergey): Make it a reusable function.
   * This is a common pattern used in few areas with the goal to bypass the hardcoded number of
   * channels used by IMB_allocImBuf(). */
  ImBuf *result_ibuf = IMB_allocImBuf(input_ibuf->x, input_ibuf->y, input_ibuf->planes, 0);
  result_ibuf->channels = input_ibuf->float_buffer.data ? input_ibuf->channels : 4;

  /* Allocate float buffer with the proper number of channels. */
  const size_t num_pixels = IMB_get_pixel_count(input_ibuf);
  float *buffer = MEM_malloc_arrayN<float>(num_pixels * result_ibuf->channels, "movie hdr image");
  IMB_assign_float_buffer(result_ibuf, buffer, IB_TAKE_OWNERSHIP);

  /* Transfer flags related to color space conversion from the original image buffer. */
  result_ibuf->flags |= (input_ibuf->flags & IB_alphamode_channel_packed);

  if (input_ibuf->float_buffer.data) {
    /* Simple case: copy pixels from the source image as-is, without any conversion.
     * The result has the same colorspace as the input. */
    memcpy(result_ibuf->float_buffer.data,
           input_ibuf->float_buffer.data,
           num_pixels * input_ibuf->channels * sizeof(float));
    result_ibuf->float_buffer.colorspace = input_ibuf->float_buffer.colorspace;
  }
  else {
    /* Convert byte buffer to float buffer.
     * The exact profile is not important here: it should match for the source and destination so
     * that the function only does alpha and byte->float conversions. */
    const bool predivide = IMB_alpha_affects_rgb(input_ibuf);
    IMB_buffer_float_from_byte(buffer,
                               input_ibuf->byte_buffer.data,
                               IB_PROFILE_SRGB,
                               IB_PROFILE_SRGB,
                               predivide,
                               input_ibuf->x,
                               input_ibuf->y,
                               result_ibuf->x,
                               input_ibuf->x);
  }

  return result_ibuf;
}

/* read and encode a frame of video from the buffer */
static AVFrame *generate_video_frame(MovieWriter *context, const ImBuf *input_ibuf)
{
  /* Use float input if needed. */
  const bool use_float =
      context->img_convert_frame != nullptr &&
      !(context->img_convert_frame->format == AV_PIX_FMT_RGBA &&
        ELEM(context->img_convert_frame->colorspace, AVCOL_SPC_RGB, AVCOL_SPC_UNSPECIFIED));

  const ImBuf *image = (use_float && input_ibuf->float_buffer.data == nullptr) ?
                           alloc_imbuf_for_colorspace_transform(input_ibuf) :
                           input_ibuf;

  const uint8_t *pixels = image->byte_buffer.data;
  const float *pixels_fl = image->float_buffer.data;

  if ((!use_float && (pixels == nullptr)) || (use_float && (pixels_fl == nullptr))) {
    if (image != input_ibuf) {
      IMB_freeImBuf(const_cast<ImBuf *>(image));
    }
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
     * packed float formats.
     * Un-premultiply the image if the output format supports alpha, to
     * match the format of the byte image. */
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

      if (MOV_codec_supports_alpha(context->ffmpeg_codec, context->ffmpeg_profile)) {
        for (int x = 0; x < image->x; x++) {
          float tmp[4];
          premul_to_straight_v4_v4(tmp, src);
          *dst_r++ = tmp[0];
          *dst_g++ = tmp[1];
          *dst_b++ = tmp[2];
          *dst_a++ = tmp[3];
          src += 4;
        }
      }
      else {
        for (int x = 0; x < image->x; x++) {
          *dst_r++ = src[0];
          *dst_g++ = src[1];
          *dst_b++ = src[2];
          *dst_a++ = src[3];
          src += 4;
        }
      }
    }
  }
  else {
    /* Byte image: flip the image vertically. */
    const size_t linesize_src = rgb_frame->width * 4;
    for (int y = 0; y < height; y++) {
      uint8_t *target = rgb_frame->data[0] + linesize_dst * (height - y - 1);
      const uint8_t *src = pixels + linesize_src * y;

      /* NOTE: this is endianness-sensitive. */
      /* The target buffer is always expected to contain little-endian RGBA values. */
      memcpy(target, src, linesize_src);
    }
  }

  /* Convert to the output pixel format, if it's different that Blender's internal one. */
  if (context->img_convert_frame != nullptr) {
    BLI_assert(context->img_convert_ctx != nullptr);
    /* Ensure the frame we are scaling to is writable as well. */
    av_frame_make_writable(context->current_frame);
    ffmpeg_sws_scale_frame(context->img_convert_ctx, context->current_frame, rgb_frame);
  }

  if (image != input_ibuf) {
    IMB_freeImBuf(const_cast<ImBuf *>(image));
  }

  return context->current_frame;
}

static AVRational calc_time_base(uint den, double num, AVCodecID codec_id)
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
    MovieWriter *context, const RenderData *rd, AVDictionary **opts, int rectx, int recty)
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
        /* Fall back to `libaom-av1` if librav1e is not found. */
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
      SNPRINTF_UTF8(buffer, "keyint=%d", context->ffmpeg_gop_size);
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
          BLI_snprintf_utf8(tiles_string_mut, digits * 2 + 2, "%dx%d", threads_sqrt, threads_sqrt);
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
            BLI_snprintf_utf8(
                tiles_string_mut, combined_digits + 2, "%dx%d", sqrt_p2_next, sqrt_p2);
          }
          else if (rectx < recty) {
            BLI_snprintf_utf8(
                tiles_string_mut, combined_digits + 2, "%dx%d", sqrt_p2, sqrt_p2_next);
          }
          else {
            BLI_snprintf_utf8(tiles_string_mut, combined_digits + 2, "%dx%d", sqrt_p2, sqrt_p2);
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

static const AVCodec *get_prores_encoder(const ImageFormatData *imf, int rectx, int recty)
{
  /* The prores_aw encoder currently (April 2025) has issues when encoding alpha with high
   * resolution but is faster in most cases for similar quality. Use it instead of prores_ks
   * if possible. (Upstream issue https://trac.ffmpeg.org/ticket/11536) */
  if (imf->planes == R_IMF_PLANES_RGBA) {
    if ((size_t(rectx) * size_t(recty)) > (3840 * 2160)) {
      return avcodec_find_encoder_by_name("prores_ks");
    }
  }
  return avcodec_find_encoder_by_name("prores_aw");
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
                                     const FFMpegCodecData *ffcodecdata,
                                     const ImageFormatData *imf,
                                     AVDictionary **opts)
{
  AVCodecContext *c = context->video_codec;

  /* Handle constant bit rate (CBR) case. */
  if (!MOV_codec_supports_crf(codec_id) || context->ffmpeg_crf < 0) {
    c->bit_rate = context->ffmpeg_video_bitrate * 1000;
    c->rc_max_rate = ffcodecdata->rc_max_rate * 1000;
    c->rc_min_rate = ffcodecdata->rc_min_rate * 1000;
    c->rc_buffer_size = ffcodecdata->rc_buffer_size * 1024;
    return;
  }

  /* For VP9 bit rate must be set to zero to get CRF mode, just set it to zero for all codecs:
   * https://trac.ffmpeg.org/wiki/Encode/VP9 */
  c->bit_rate = 0;

  const bool is_10_bpp = imf->depth == R_IMF_CHAN_DEPTH_10;
  const bool is_12_bpp = imf->depth == R_IMF_CHAN_DEPTH_12;
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

static void set_colorspace_options(AVCodecContext *c, const ColorSpace *colorspace)
{
  const AVPixFmtDescriptor *pix_fmt_desc = av_pix_fmt_desc_get(c->pix_fmt);
  const bool is_rgb_format = (pix_fmt_desc->flags & AV_PIX_FMT_FLAG_RGB);
  const bool rgb_matrix = false;

  int cicp[4];
  if (colorspace && IMB_colormanagement_space_to_cicp(
                        colorspace, ColorManagedFileOutput::Video, rgb_matrix, cicp))
  {
    /* Note ffmpeg enums are documented to match CICP. */
    c->color_primaries = AVColorPrimaries(cicp[0]);
    c->color_trc = AVColorTransferCharacteristic(cicp[1]);
    c->colorspace = (is_rgb_format) ? AVCOL_SPC_RGB : AVColorSpace(cicp[2]);
    c->color_range = AVCOL_RANGE_JPEG;
  }
  else if (!is_rgb_format) {
    /* Note BT.709 is wrong for sRGB.
     * But we have been writing sRGB like this forever, and there is the so called
     * "Quicktime gamma shift bug" that complicates things. */
    c->color_primaries = AVCOL_PRI_BT709;
    c->color_trc = AVCOL_TRC_BT709;
    c->colorspace = AVCOL_SPC_BT709;
    /* TODO(sergey): Consider making the range an option to cover more use-cases. */
    c->color_range = AVCOL_RANGE_MPEG;
  }
  else {
    /* We don't set anything for pure sRGB writing, for backwards compatibility. */
  }
}

static AVStream *alloc_video_stream(MovieWriter *context,
                                    const RenderData *rd,
                                    const ImageFormatData *imf,
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
  else if (codec_id == AV_CODEC_ID_PRORES) {
    codec = get_prores_encoder(imf, rectx, recty);
  }
  else {
    codec = avcodec_find_encoder(codec_id);
  }
  if (!codec) {
    CLOG_ERROR(&LOG, "Couldn't find valid video codec");
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

  set_quality_rate_options(context, codec_id, &rd->ffcodecdata, imf, &opts);

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
        CLOG_WARN(&LOG, "Unknown preset number %i, ignoring.", context->ffmpeg_preset);
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

  const enum AVPixelFormat *pix_fmts = ffmpeg_get_pix_fmts(c, codec);
  if (pix_fmts) {
    c->pix_fmt = pix_fmts[0];
  }
  else {
    /* makes HuffYUV happy ... */
    c->pix_fmt = AV_PIX_FMT_YUV422P;
  }

  const bool is_10_bpp = imf->depth == R_IMF_CHAN_DEPTH_10;
  const bool is_12_bpp = imf->depth == R_IMF_CHAN_DEPTH_12;
  const bool is_16_bpp = imf->depth == R_IMF_CHAN_DEPTH_16;

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
    if (imf->planes == R_IMF_PLANES_RGBA) {
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
    if (imf->planes == R_IMF_PLANES_BW) {
      c->pix_fmt = AV_PIX_FMT_GRAY8;
      if (is_10_bpp) {
        c->pix_fmt = AV_PIX_FMT_GRAY10;
      }
      else if (is_12_bpp) {
        c->pix_fmt = AV_PIX_FMT_GRAY12;
      }
      else if (is_16_bpp) {
        c->pix_fmt = AV_PIX_FMT_GRAY16;
      }
    }
    else if (imf->planes == R_IMF_PLANES_RGBA) {
      c->pix_fmt = AV_PIX_FMT_RGB32;
      if (is_10_bpp) {
        c->pix_fmt = AV_PIX_FMT_GBRAP10;
      }
      else if (is_12_bpp) {
        c->pix_fmt = AV_PIX_FMT_GBRAP12;
      }
      else if (is_16_bpp) {
        c->pix_fmt = AV_PIX_FMT_GBRAP16;
      }
    }
    else { /* RGB */
      c->pix_fmt = AV_PIX_FMT_0RGB32;
      if (is_10_bpp) {
        c->pix_fmt = AV_PIX_FMT_GBRP10;
      }
      else if (is_12_bpp) {
        c->pix_fmt = AV_PIX_FMT_GBRP12;
      }
      else if (is_16_bpp) {
        c->pix_fmt = AV_PIX_FMT_GBRP16;
      }
    }
  }

  if (codec_id == AV_CODEC_ID_QTRLE) {
    if (imf->planes == R_IMF_PLANES_BW) {
      c->pix_fmt = AV_PIX_FMT_GRAY8;
    }
    else if (imf->planes == R_IMF_PLANES_RGBA) {
      c->pix_fmt = AV_PIX_FMT_ARGB;
    }
    else { /* RGB */
      c->pix_fmt = AV_PIX_FMT_RGB24;
    }
  }

  if (codec_id == AV_CODEC_ID_VP9 && imf->planes == R_IMF_PLANES_RGBA) {
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
    if (imf->planes == R_IMF_PLANES_BW) {
      c->pix_fmt = AV_PIX_FMT_GRAY8;
    }
    else if (imf->planes == R_IMF_PLANES_RGBA) {
      c->pix_fmt = AV_PIX_FMT_RGBA;
    }
    else { /* RGB */
      c->pix_fmt = AV_PIX_FMT_RGB24;
    }
  }
  if (codec_id == AV_CODEC_ID_PRORES) {
    if ((context->ffmpeg_profile >= FFM_PRORES_PROFILE_422_PROXY) &&
        (context->ffmpeg_profile <= FFM_PRORES_PROFILE_422_HQ))
    {
      c->profile = context->ffmpeg_profile;
      c->pix_fmt = AV_PIX_FMT_YUV422P10LE;
    }
    else if ((context->ffmpeg_profile >= FFM_PRORES_PROFILE_4444) &&
             (context->ffmpeg_profile <= FFM_PRORES_PROFILE_4444_XQ))
    {
      c->profile = context->ffmpeg_profile;
      c->pix_fmt = AV_PIX_FMT_YUV444P10LE;

      if (imf->planes == R_IMF_PLANES_RGBA) {
        c->pix_fmt = AV_PIX_FMT_YUVA444P10LE;
      }
    }
    else {
      CLOG_ERROR(&LOG, "ffmpeg: invalid profile %d", context->ffmpeg_profile);
    }
  }

  if (of->oformat->flags & AVFMT_GLOBALHEADER) {
    CLOG_STR_INFO(&LOG, "ffmpeg: using global video header");
    c->flags |= AV_CODEC_FLAG_GLOBAL_HEADER;
  }

  /* Set colorspace based on display space of image. */
  const ColorSpace *display_colorspace = IMB_colormangement_display_get_color_space(
      &imf->view_settings, &imf->display_settings);
  set_colorspace_options(c, display_colorspace);

  /* xasp & yasp got float lately... */

  st->sample_aspect_ratio = c->sample_aspect_ratio = av_d2q((double(rd->xasp) / double(rd->yasp)),
                                                            255);
  st->avg_frame_rate = av_inv_q(c->time_base);

  if (codec->capabilities & AV_CODEC_CAP_OTHER_THREADS) {
    c->thread_count = 0;
  }
  else {
    c->thread_count = MOV_thread_count();
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
    CLOG_ERROR(&LOG, "Couldn't initialize video codec: %s\n", error_str);
    BLI_strncpy(error, ffmpeg_last_error(), error_size);
    av_dict_free(&opts);
    avcodec_free_context(&c);
    context->video_codec = nullptr;
    return nullptr;
  }
  av_dict_free(&opts);

  /* FFMPEG expects its data in the output pixel format. */
  context->current_frame = alloc_frame(c->pix_fmt, c->width, c->height);

  if (c->pix_fmt == AV_PIX_FMT_RGBA && ELEM(c->colorspace, AVCOL_SPC_RGB, AVCOL_SPC_UNSPECIFIED)) {
    /* Output pixel format and colorspace is the same we use internally, no conversion needed. */
    context->img_convert_frame = nullptr;
    context->img_convert_ctx = nullptr;
  }
  else {
    /* Output pixel format is different, allocate frame for conversion.
     * Setup RGB->YUV conversion with proper coefficients, depending on range and colorspace. */
    const AVPixelFormat src_format = is_10_bpp || is_12_bpp || is_16_bpp ? AV_PIX_FMT_GBRAPF32LE :
                                                                           AV_PIX_FMT_RGBA;
    context->img_convert_frame = alloc_frame(src_format, c->width, c->height);
    context->img_convert_ctx = ffmpeg_sws_get_context(
        c->width,
        c->height,
        src_format,
        true,
        -1,
        c->width,
        c->height,
        c->pix_fmt,
        c->color_range == AVCOL_RANGE_JPEG,
        c->colorspace != AVCOL_SPC_RGB ? c->colorspace : -1,
        SWS_BICUBIC);
  }

  avcodec_parameters_from_context(st->codecpar, c);

  add_hdr_mastering_display_metadata(st->codecpar, c, imf);

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
                              const Scene *scene,
                              const RenderData *rd,
                              const ImageFormatData *imf,
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
  context->ffmpeg_codec = mov_av_codec_id_get(rd->ffcodecdata.codec_id_get());
  context->ffmpeg_audio_codec = mov_av_codec_id_get(rd->ffcodecdata.audio_codec_id_get());
  context->ffmpeg_video_bitrate = rd->ffcodecdata.video_bitrate;
  context->ffmpeg_audio_bitrate = rd->ffcodecdata.audio_bitrate;
  context->ffmpeg_gop_size = rd->ffcodecdata.gop_size;
  context->ffmpeg_autosplit = (rd->ffcodecdata.flags & FFMPEG_AUTOSPLIT_OUTPUT) != 0;
  context->ffmpeg_crf = rd->ffcodecdata.constant_rate_factor;
  context->ffmpeg_preset = rd->ffcodecdata.ffmpeg_preset;
  context->ffmpeg_profile = 0;

  if ((rd->ffcodecdata.flags & FFMPEG_USE_MAX_B_FRAMES) != 0) {
    context->ffmpeg_max_b_frames = rd->ffcodecdata.max_b_frames;
  }

  /* Determine the correct filename */
  if (!ffmpeg_filepath_get(context, filepath, scene, rd, context->ffmpeg_preview, suffix, reports))
  {
    return false;
  }
  CLOG_INFO(&LOG,
            "ffmpeg: starting output to %s:\n"
            "  type=%d, codec=%d, audio_codec=%d,\n"
            "  video_bitrate=%d, audio_bitrate=%d,\n"
            "  gop_size=%d, autosplit=%d\n"
            "  width=%d, height=%d",
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
    BKE_report(reports, RPT_ERROR, "Cannot allocate FFmpeg format context");
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

  if (video_codec == AV_CODEC_ID_PRORES) {
    context->ffmpeg_profile = rd->ffcodecdata.ffmpeg_prores_profile;
  }

  if (video_codec != AV_CODEC_ID_NONE) {
    context->video_stream = alloc_video_stream(
        context, rd, imf, video_codec, of, rectx, recty, error, sizeof(error));
    CLOG_INFO(&LOG, "ffmpeg: alloc video stream %p", context->video_stream);
    if (!context->video_stream) {
      if (error[0]) {
        BKE_report(reports, RPT_ERROR, error);
        CLOG_INFO(&LOG, "ffmpeg: video stream error: %s", error);
      }
      else {
        BKE_report(reports, RPT_ERROR, "Error initializing video stream");
        CLOG_STR_INFO(&LOG, "ffmpeg: error initializing video stream");
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
        CLOG_INFO(&LOG, "ffmpeg: audio stream error: %s", error);
      }
      else {
        BKE_report(reports, RPT_ERROR, "Error initializing audio stream");
        CLOG_STR_INFO(&LOG, "ffmpeg: error initializing audio stream");
      }
      goto fail;
    }
  }
  if (!(fmt->flags & AVFMT_NOFILE)) {
    if (avio_open(&of->pb, filepath, AVIO_FLAG_WRITE) < 0) {
      BKE_report(reports, RPT_ERROR, "Could not open file for writing");
      CLOG_INFO(&LOG, "ffmpeg: could not open file %s for writing", filepath);
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
    CLOG_INFO(&LOG, "ffmpeg: could not write media header: %s", error_str);
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
      CLOG_ERROR(&LOG, "Error encoding delayed frame: %s", error_str);
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
      CLOG_ERROR(&LOG, "Error writing delayed frame: %s", error_str);
      break;
    }
  }

  av_packet_free(&packet);
}

/**
 * Get the output filename-- similar to the other output formats.
 *
 * \param reports: If non-null, will report errors with `RPT_ERROR` level reports.
 *
 * \return true on success, false on failure due to errors.
 */
static bool ffmpeg_filepath_get(MovieWriter *context,
                                char filepath[FILE_MAX],
                                const Scene *scene,
                                const RenderData *rd,
                                bool preview,
                                const char *suffix,
                                ReportList *reports)
{
  char autosplit[20];

  const char **exts = get_file_extensions(rd->ffcodecdata.type);
  const char **fe = exts;
  int sfra, efra;

  if (!filepath || !exts) {
    return false;
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

  blender::bke::path_templates::VariableMap template_variables;
  BKE_add_template_variables_general(template_variables, &scene->id);
  BKE_add_template_variables_for_render_path(template_variables, *scene);

  const blender::Vector<blender::bke::path_templates::Error> errors = BKE_path_apply_template(
      filepath, FILE_MAX, template_variables);
  if (!errors.is_empty()) {
    BKE_report_path_template_errors(reports, RPT_ERROR, filepath, errors);
    return false;
  }

  BLI_path_abs(filepath, BKE_main_blendfile_path_from_global());

  if (!BLI_file_ensure_parent_dir_exists(filepath)) {
    CLOG_ERROR(&LOG, "Couldn't create directory for file %s: %s", filepath, std::strerror(errno));
    return false;
  }

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

  return true;
}

static void ffmpeg_get_filepath(char filepath[/*FILE_MAX*/ 1024],
                                const Scene *scene,
                                const RenderData *rd,
                                bool preview,
                                const char *suffix,
                                ReportList *reports)
{
  ffmpeg_filepath_get(nullptr, filepath, scene, rd, preview, suffix, reports);
}

static MovieWriter *ffmpeg_movie_open(const Scene *scene,
                                      const RenderData *rd,
                                      const ImageFormatData *imf,
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

  bool success = start_ffmpeg_impl(context, scene, rd, imf, rectx, recty, suffix, reports);

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
                                const Scene *scene,
                                const RenderData *rd,
                                const ImageFormatData *imf,
                                int start_frame,
                                int frame,
                                const ImBuf *image,
                                const char *suffix,
                                ReportList *reports)
{
  AVFrame *avframe;
  bool success = true;

  CLOG_INFO(&LOG, "ffmpeg: writing frame #%i (%ix%i)", frame, image->x, image->y);

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

      success &= start_ffmpeg_impl(context, scene, rd, imf, image->x, image->y, suffix, reports);
    }
  }

  return success;
}

static void end_ffmpeg_impl(MovieWriter *context, bool is_autosplit)
{
  CLOG_STR_INFO(&LOG, "ffmpeg: closing");

  movie_audio_close(context, is_autosplit);

  if (context->video_stream) {
    CLOG_STR_INFO(&LOG, "ffmpeg: flush delayed video frames");
    flush_delayed_frames(context->video_codec, context->video_stream, context->outfile);
  }

  if (context->audio_stream) {
    CLOG_STR_INFO(&LOG, "ffmpeg: flush delayed audio frames");
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

MovieWriter *MOV_write_begin(const Scene *scene,
                             const RenderData *rd,
                             const ImageFormatData *imf,
                             int rectx,
                             int recty,
                             ReportList *reports,
                             bool preview,
                             const char *suffix)
{
  if (imf->imtype != R_IMF_IMTYPE_FFMPEG) {
    BKE_report(reports, RPT_ERROR, "Image format is not a movie format");
    return nullptr;
  }

  MovieWriter *writer = nullptr;
#ifdef WITH_FFMPEG
  writer = ffmpeg_movie_open(scene, rd, imf, rectx, recty, reports, preview, suffix);
#else
  UNUSED_VARS(scene, rd, imf, rectx, recty, reports, preview, suffix);
#endif
  return writer;
}

bool MOV_write_append(MovieWriter *writer,
                      const Scene *scene,
                      const RenderData *rd,
                      const ImageFormatData *imf,
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
  bool ok = ffmpeg_movie_append(
      writer, scene, rd, imf, start_frame, frame, image, suffix, reports);
  return ok;
#else
  UNUSED_VARS(scene, rd, imf, start_frame, frame, image, suffix, reports);
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
                                const Scene *scene,
                                const RenderData *rd,
                                bool preview,
                                const char *suffix,
                                ReportList *reports)
{
#ifdef WITH_FFMPEG
  if (rd->im_format.imtype == R_IMF_IMTYPE_FFMPEG) {
    ffmpeg_get_filepath(filepath, scene, rd, preview, suffix, reports);
    return;
  }
#else
  UNUSED_VARS(scene, rd, preview, suffix, reports);
#endif
  filepath[0] = '\0';
}
