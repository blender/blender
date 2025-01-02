/* SPDX-FileCopyrightText: 2024 Blender Authors
 *
 * SPDX-License-Identifier: GPL-2.0-or-later */

/** \file
 * \ingroup imbuf
 */

#include "BLI_path_utils.hh"
#include "BLI_utildefines.h"

#include "DNA_scene_types.h"

#include "MOV_enums.hh"
#include "MOV_util.hh"

#include "ffmpeg_swscale.hh"
#include "movie_util.hh"

#ifdef WITH_FFMPEG

#  include "BLI_string.h"

#  include "BKE_global.hh"

extern "C" {
#  include "ffmpeg_compat.h"
#  include <libavcodec/avcodec.h>
#  include <libavdevice/avdevice.h>
#  include <libavformat/avformat.h>
#  include <libavutil/log.h>
}

static char ffmpeg_last_error_buffer[1024];

/* BLI_vsnprintf in ffmpeg_log_callback() causes invalid warning */
#  ifdef __GNUC__
#    pragma GCC diagnostic push
#    pragma GCC diagnostic ignored "-Wmissing-format-attribute"
#  endif

static void ffmpeg_log_callback(void *ptr, int level, const char *format, va_list arg)
{
  if (ELEM(level, AV_LOG_FATAL, AV_LOG_ERROR)) {
    size_t n;
    va_list args_cpy;

    va_copy(args_cpy, arg);
    n = VSNPRINTF(ffmpeg_last_error_buffer, format, args_cpy);
    va_end(args_cpy);

    /* strip trailing \n */
    ffmpeg_last_error_buffer[n - 1] = '\0';
  }

  if (G.debug & G_DEBUG_FFMPEG) {
    /* call default logger to print all message to console */
    av_log_default_callback(ptr, level, format, arg);
  }
}

#  ifdef __GNUC__
#    pragma GCC diagnostic pop
#  endif

const char *ffmpeg_last_error()
{
  return ffmpeg_last_error_buffer;
}

static int isffmpeg(const char *filepath)
{
  AVFormatContext *pFormatCtx = nullptr;
  uint i;
  int videoStream;
  const AVCodec *pCodec;

  if (BLI_path_extension_check_n(filepath,
                                 ".swf",
                                 ".jpg",
                                 ".jp2",
                                 ".j2c",
                                 ".png",
                                 ".dds",
                                 ".tga",
                                 ".bmp",
                                 ".tif",
                                 ".exr",
                                 ".cin",
                                 ".wav",
                                 nullptr))
  {
    return 0;
  }

  if (avformat_open_input(&pFormatCtx, filepath, nullptr, nullptr) != 0) {
    return 0;
  }

  if (avformat_find_stream_info(pFormatCtx, nullptr) < 0) {
    avformat_close_input(&pFormatCtx);
    return 0;
  }

  /* Find the first video stream */
  videoStream = -1;
  for (i = 0; i < pFormatCtx->nb_streams; i++) {
    if (pFormatCtx->streams[i] && pFormatCtx->streams[i]->codecpar &&
        (pFormatCtx->streams[i]->codecpar->codec_type == AVMEDIA_TYPE_VIDEO))
    {
      videoStream = i;
      break;
    }
  }

  if (videoStream == -1) {
    avformat_close_input(&pFormatCtx);
    return 0;
  }

  AVCodecParameters *codec_par = pFormatCtx->streams[videoStream]->codecpar;

  /* Find the decoder for the video stream */
  pCodec = avcodec_find_decoder(codec_par->codec_id);
  if (pCodec == nullptr) {
    avformat_close_input(&pFormatCtx);
    return 0;
  }

  avformat_close_input(&pFormatCtx);

  return 1;
}

/* -------------------------------------------------------------------- */
/* AVFrame de-interlacing. Code for this was originally based on FFMPEG 2.6.4 (LGPL). */

#  define MAX_NEG_CROP 1024

#  define times4(x) x, x, x, x
#  define times256(x) times4(times4(times4(times4(times4(x)))))

static const uint8_t ff_compat_crop_tab[256 + 2 * MAX_NEG_CROP] = {
    times256(0x00), 0x00, 0x01, 0x02, 0x03, 0x04,          0x05, 0x06, 0x07, 0x08, 0x09, 0x0A,
    0x0B,           0x0C, 0x0D, 0x0E, 0x0F, 0x10,          0x11, 0x12, 0x13, 0x14, 0x15, 0x16,
    0x17,           0x18, 0x19, 0x1A, 0x1B, 0x1C,          0x1D, 0x1E, 0x1F, 0x20, 0x21, 0x22,
    0x23,           0x24, 0x25, 0x26, 0x27, 0x28,          0x29, 0x2A, 0x2B, 0x2C, 0x2D, 0x2E,
    0x2F,           0x30, 0x31, 0x32, 0x33, 0x34,          0x35, 0x36, 0x37, 0x38, 0x39, 0x3A,
    0x3B,           0x3C, 0x3D, 0x3E, 0x3F, 0x40,          0x41, 0x42, 0x43, 0x44, 0x45, 0x46,
    0x47,           0x48, 0x49, 0x4A, 0x4B, 0x4C,          0x4D, 0x4E, 0x4F, 0x50, 0x51, 0x52,
    0x53,           0x54, 0x55, 0x56, 0x57, 0x58,          0x59, 0x5A, 0x5B, 0x5C, 0x5D, 0x5E,
    0x5F,           0x60, 0x61, 0x62, 0x63, 0x64,          0x65, 0x66, 0x67, 0x68, 0x69, 0x6A,
    0x6B,           0x6C, 0x6D, 0x6E, 0x6F, 0x70,          0x71, 0x72, 0x73, 0x74, 0x75, 0x76,
    0x77,           0x78, 0x79, 0x7A, 0x7B, 0x7C,          0x7D, 0x7E, 0x7F, 0x80, 0x81, 0x82,
    0x83,           0x84, 0x85, 0x86, 0x87, 0x88,          0x89, 0x8A, 0x8B, 0x8C, 0x8D, 0x8E,
    0x8F,           0x90, 0x91, 0x92, 0x93, 0x94,          0x95, 0x96, 0x97, 0x98, 0x99, 0x9A,
    0x9B,           0x9C, 0x9D, 0x9E, 0x9F, 0xA0,          0xA1, 0xA2, 0xA3, 0xA4, 0xA5, 0xA6,
    0xA7,           0xA8, 0xA9, 0xAA, 0xAB, 0xAC,          0xAD, 0xAE, 0xAF, 0xB0, 0xB1, 0xB2,
    0xB3,           0xB4, 0xB5, 0xB6, 0xB7, 0xB8,          0xB9, 0xBA, 0xBB, 0xBC, 0xBD, 0xBE,
    0xBF,           0xC0, 0xC1, 0xC2, 0xC3, 0xC4,          0xC5, 0xC6, 0xC7, 0xC8, 0xC9, 0xCA,
    0xCB,           0xCC, 0xCD, 0xCE, 0xCF, 0xD0,          0xD1, 0xD2, 0xD3, 0xD4, 0xD5, 0xD6,
    0xD7,           0xD8, 0xD9, 0xDA, 0xDB, 0xDC,          0xDD, 0xDE, 0xDF, 0xE0, 0xE1, 0xE2,
    0xE3,           0xE4, 0xE5, 0xE6, 0xE7, 0xE8,          0xE9, 0xEA, 0xEB, 0xEC, 0xED, 0xEE,
    0xEF,           0xF0, 0xF1, 0xF2, 0xF3, 0xF4,          0xF5, 0xF6, 0xF7, 0xF8, 0xF9, 0xFA,
    0xFB,           0xFC, 0xFD, 0xFE, 0xFF, times256(0xFF)};

#  undef times4
#  undef times256

/* filter parameters: [-1 4 2 4 -1] // 8 */
FFMPEG_INLINE void deinterlace_line(uint8_t *dst,
                                    const uint8_t *lum_m4,
                                    const uint8_t *lum_m3,
                                    const uint8_t *lum_m2,
                                    const uint8_t *lum_m1,
                                    const uint8_t *lum,
                                    int size)
{
  const uint8_t *cm = ff_compat_crop_tab + MAX_NEG_CROP;
  int sum;

  for (; size > 0; size--) {
    sum = -lum_m4[0];
    sum += lum_m3[0] << 2;
    sum += lum_m2[0] << 1;
    sum += lum_m1[0] << 2;
    sum += -lum[0];
    dst[0] = cm[(sum + 4) >> 3];
    lum_m4++;
    lum_m3++;
    lum_m2++;
    lum_m1++;
    lum++;
    dst++;
  }
}

FFMPEG_INLINE void deinterlace_line_inplace(
    uint8_t *lum_m4, uint8_t *lum_m3, uint8_t *lum_m2, uint8_t *lum_m1, uint8_t *lum, int size)
{
  const uint8_t *cm = ff_compat_crop_tab + MAX_NEG_CROP;
  int sum;

  for (; size > 0; size--) {
    sum = -lum_m4[0];
    sum += lum_m3[0] << 2;
    sum += lum_m2[0] << 1;
    lum_m4[0] = lum_m2[0];
    sum += lum_m1[0] << 2;
    sum += -lum[0];
    lum_m2[0] = cm[(sum + 4) >> 3];
    lum_m4++;
    lum_m3++;
    lum_m2++;
    lum_m1++;
    lum++;
  }
}

/**
 * De-interlacing: 2 temporal taps, 3 spatial taps linear filter.
 * The top field is copied as is, but the bottom field is de-interlaced against the top field.
 */
FFMPEG_INLINE void deinterlace_bottom_field(
    uint8_t *dst, int dst_wrap, const uint8_t *src1, int src_wrap, int width, int height)
{
  const uint8_t *src_m2, *src_m1, *src_0, *src_p1, *src_p2;
  int y;

  src_m2 = src1;
  src_m1 = src1;
  src_0 = &src_m1[src_wrap];
  src_p1 = &src_0[src_wrap];
  src_p2 = &src_p1[src_wrap];
  for (y = 0; y < (height - 2); y += 2) {
    memcpy(dst, src_m1, width);
    dst += dst_wrap;
    deinterlace_line(dst, src_m2, src_m1, src_0, src_p1, src_p2, width);
    src_m2 = src_0;
    src_m1 = src_p1;
    src_0 = src_p2;
    src_p1 += 2 * src_wrap;
    src_p2 += 2 * src_wrap;
    dst += dst_wrap;
  }
  memcpy(dst, src_m1, width);
  dst += dst_wrap;
  /* do last line */
  deinterlace_line(dst, src_m2, src_m1, src_0, src_0, src_0, width);
}

FFMPEG_INLINE int deinterlace_bottom_field_inplace(uint8_t *src1,
                                                   int src_wrap,
                                                   int width,
                                                   int height)
{
  uint8_t *src_m1, *src_0, *src_p1, *src_p2;
  int y;
  uint8_t *buf = (uint8_t *)av_malloc(width);
  if (!buf) {
    return AVERROR(ENOMEM);
  }

  src_m1 = src1;
  memcpy(buf, src_m1, width);
  src_0 = &src_m1[src_wrap];
  src_p1 = &src_0[src_wrap];
  src_p2 = &src_p1[src_wrap];
  for (y = 0; y < (height - 2); y += 2) {
    deinterlace_line_inplace(buf, src_m1, src_0, src_p1, src_p2, width);
    src_m1 = src_p1;
    src_0 = src_p2;
    src_p1 += 2 * src_wrap;
    src_p2 += 2 * src_wrap;
  }
  /* do last line */
  deinterlace_line_inplace(buf, src_m1, src_0, src_0, src_0, width);
  av_free(buf);
  return 0;
}

int ffmpeg_deinterlace(
    AVFrame *dst, const AVFrame *src, enum AVPixelFormat pix_fmt, int width, int height)
{
  int i, ret;

  if (!ELEM(pix_fmt,
            AV_PIX_FMT_YUV420P,
            AV_PIX_FMT_YUVJ420P,
            AV_PIX_FMT_YUV422P,
            AV_PIX_FMT_YUVJ422P,
            AV_PIX_FMT_YUV444P,
            AV_PIX_FMT_YUV411P,
            AV_PIX_FMT_GRAY8))
  {
    return -1;
  }
  if ((width & 3) != 0 || (height & 3) != 0) {
    return -1;
  }

  for (i = 0; i < 3; i++) {
    if (i == 1) {
      switch (pix_fmt) {
        case AV_PIX_FMT_YUVJ420P:
        case AV_PIX_FMT_YUV420P:
          width >>= 1;
          height >>= 1;
          break;
        case AV_PIX_FMT_YUV422P:
        case AV_PIX_FMT_YUVJ422P:
          width >>= 1;
          break;
        case AV_PIX_FMT_YUV411P:
          width >>= 2;
          break;
        default:
          break;
      }
      if (pix_fmt == AV_PIX_FMT_GRAY8) {
        break;
      }
    }
    if (src == dst) {
      ret = deinterlace_bottom_field_inplace(dst->data[i], dst->linesize[i], width, height);
      if (ret < 0) {
        return ret;
      }
    }
    else {
      deinterlace_bottom_field(
          dst->data[i], dst->linesize[i], src->data[i], src->linesize[i], width, height);
    }
  }
  return 0;
}

#endif /* WITH_FFMPEG */

bool MOV_is_movie_file(const char *filepath)
{
  BLI_assert(!BLI_path_is_rel(filepath));

#ifdef WITH_FFMPEG
  if (isffmpeg(filepath)) {
    return true;
  }
#else
  UNUSED_VARS(filepath);
#endif

  return false;
}

void MOV_init()
{
#ifdef WITH_FFMPEG
  avdevice_register_all();

  ffmpeg_last_error_buffer[0] = '\0';

  if (G.debug & G_DEBUG_FFMPEG) {
    av_log_set_level(AV_LOG_DEBUG);
  }

  /* set separate callback which could store last error to report to UI */
  av_log_set_callback(ffmpeg_log_callback);
#endif
}

void MOV_exit()
{
#ifdef WITH_FFMPEG
  ffmpeg_sws_exit();
#endif
}

int MOV_codec_valid_bit_depths(int av_codec_id)
{
  int bit_depths = R_IMF_CHAN_DEPTH_8;
#ifdef WITH_FFMPEG
  /* Note: update properties_output.py `use_bpp` when changing this function. */
  if (ELEM(av_codec_id, AV_CODEC_ID_H264, AV_CODEC_ID_H265, AV_CODEC_ID_AV1)) {
    bit_depths |= R_IMF_CHAN_DEPTH_10;
  }
  if (ELEM(av_codec_id, AV_CODEC_ID_H265, AV_CODEC_ID_AV1)) {
    bit_depths |= R_IMF_CHAN_DEPTH_12;
  }
#else
  UNUSED_VARS(av_codec_id);
#endif
  return bit_depths;
}

#ifdef WITH_FFMPEG
static void ffmpeg_preset_set(RenderData *rd, int preset)
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
#endif

void MOV_validate_output_settings(RenderData *rd, const ImageFormatData *imf)
{
#ifdef WITH_FFMPEG
  int audio = 0;

  if (imf->imtype == R_IMF_IMTYPE_FFMPEG) {
    if (rd->ffcodecdata.type <= 0 || rd->ffcodecdata.codec <= 0 ||
        rd->ffcodecdata.audio_codec <= 0 || rd->ffcodecdata.video_bitrate <= 1)
    {
      ffmpeg_preset_set(rd, FFMPEG_PRESET_H264);
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
      ffmpeg_preset_set(rd, FFMPEG_PRESET_H264);
      audio = 1;
    }
  }
  else if (imf->imtype == R_IMF_IMTYPE_XVID) {
    if (rd->ffcodecdata.codec != AV_CODEC_ID_MPEG4) {
      ffmpeg_preset_set(rd, FFMPEG_PRESET_XVID);
      audio = 1;
    }
  }
  else if (imf->imtype == R_IMF_IMTYPE_THEORA) {
    if (rd->ffcodecdata.codec != AV_CODEC_ID_THEORA) {
      ffmpeg_preset_set(rd, FFMPEG_PRESET_THEORA);
      audio = 1;
    }
  }
  else if (imf->imtype == R_IMF_IMTYPE_AV1) {
    if (rd->ffcodecdata.codec != AV_CODEC_ID_AV1) {
      ffmpeg_preset_set(rd, FFMPEG_PRESET_AV1);
      audio = 1;
    }
  }

  if (audio && rd->ffcodecdata.audio_codec < 0) {
    rd->ffcodecdata.audio_codec = AV_CODEC_ID_NONE;
    rd->ffcodecdata.audio_bitrate = 128;
  }
#else
  UNUSED_VARS(rd, imf);
#endif
}

bool MOV_codec_supports_alpha(int av_codec_id)
{
#ifdef WITH_FFMPEG
  return ELEM(av_codec_id,
              AV_CODEC_ID_FFV1,
              AV_CODEC_ID_QTRLE,
              AV_CODEC_ID_PNG,
              AV_CODEC_ID_VP9,
              AV_CODEC_ID_HUFFYUV);
#else
  UNUSED_VARS(av_codec_id);
  return false;
#endif
}

bool MOV_codec_supports_crf(int av_codec_id)
{
#ifdef WITH_FFMPEG
  return ELEM(av_codec_id,
              AV_CODEC_ID_H264,
              AV_CODEC_ID_H265,
              AV_CODEC_ID_MPEG4,
              AV_CODEC_ID_VP9,
              AV_CODEC_ID_AV1);
#else
  UNUSED_VARS(av_codec_id);
  return false;
#endif
}
