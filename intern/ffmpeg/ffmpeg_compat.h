/* SPDX-License-Identifier: GPL-2.0-or-later
 * Copyright 2011 Peter Schlaile. */

/** \file
 * \ingroup ffmpeg
 *
 * Compatibility macros to make every FFMPEG installation appear
 * like the most current installation (wrapping some functionality sometimes)
 * it also includes all FFMPEG header files at once, no need to do it
 * separately.
 */

#ifndef __FFMPEG_COMPAT_H__
#define __FFMPEG_COMPAT_H__

#include <libavcodec/avcodec.h>
#include <libavformat/avformat.h>

/* Check if our ffmpeg is new enough, avoids user complaints.
 * Minimum supported version is currently 3.2.0 which mean the following library versions:
 * libavutil   > 55.30
 * libavcodec  > 57.60
 * libavformat > 57.50
 *
 * We only check for one of these as they are usually updated in tandem.
 */
#if (LIBAVFORMAT_VERSION_MAJOR < 57) || \
    ((LIBAVFORMAT_VERSION_MAJOR == 57) && (LIBAVFORMAT_VERSION_MINOR <= 50))
#  error "FFmpeg 3.2.0 or newer is needed, Upgrade your FFmpeg or disable it"
#endif
/* end sanity check */

/* visual studio 2012 does not define inline for C */
#ifdef _MSC_VER
#  define FFMPEG_INLINE static __inline
#else
#  define FFMPEG_INLINE static inline
#endif

#if (LIBAVFORMAT_VERSION_MAJOR < 59)
/* For versions older than ffmpeg 5.0, use the old channel layout variables.
 * We intend to only keep this  workaround for around two releases (3.5, 3.6).
 * If it sticks around any longer, then we should consider refactoring this.
 */
#  define FFMPEG_USE_OLD_CHANNEL_VARS
#endif

/* AV_CODEC_CAP_AUTO_THREADS was renamed to AV_CODEC_CAP_OTHER_THREADS with
 * upstream commit
 * github.com/FFmpeg/FFmpeg/commit/7d09579190def3ef7562399489e628f3b65714ce
 * (lavc 58.132.100) and removed with commit
 * github.com/FFmpeg/FFmpeg/commit/10c9a0874cb361336237557391d306d26d43f137
 * for ffmpeg 6.0.
 */
#if LIBAVCODEC_VERSION_INT < AV_VERSION_INT(58,132,100)
#  define AV_CODEC_CAP_OTHER_THREADS AV_CODEC_CAP_AUTO_THREADS
#endif

#if (LIBAVFORMAT_VERSION_MAJOR < 58) || \
    ((LIBAVFORMAT_VERSION_MAJOR == 58) && (LIBAVFORMAT_VERSION_MINOR < 76))
#  define FFMPEG_USE_DURATION_WORKAROUND 1

/* Before ffmpeg 4.4, package duration calculation used depricated variables to calculate the
 * packet duration. Use the function from commit
 * github.com/FFmpeg/FFmpeg/commit/1c0885334dda9ee8652e60c586fa2e3674056586
 * to calculate the correct framerate for ffmpeg < 4.4.
 */

FFMPEG_INLINE
void my_guess_pkt_duration(AVFormatContext *s, AVStream *st, AVPacket *pkt)
{
  if (pkt->duration < 0 && st->codecpar->codec_type != AVMEDIA_TYPE_SUBTITLE) {
    av_log(s,
           AV_LOG_WARNING,
           "Packet with invalid duration %" PRId64 " in stream %d\n",
           pkt->duration,
           pkt->stream_index);
    pkt->duration = 0;
  }

  if (pkt->duration) {
    return;
  }

  switch (st->codecpar->codec_type) {
    case AVMEDIA_TYPE_VIDEO:
      if (st->avg_frame_rate.num > 0 && st->avg_frame_rate.den > 0) {
        pkt->duration = av_rescale_q(1, av_inv_q(st->avg_frame_rate), st->time_base);
      }
      else if (st->time_base.num * 1000LL > st->time_base.den) {
        pkt->duration = 1;
      }
      break;
    case AVMEDIA_TYPE_AUDIO: {
      int frame_size = av_get_audio_frame_duration2(st->codecpar, pkt->size);
      if (frame_size && st->codecpar->sample_rate) {
        pkt->duration = av_rescale_q(
            frame_size, (AVRational){1, st->codecpar->sample_rate}, st->time_base);
      }
      break;
    }
    default:
      break;
  }
}
#endif

FFMPEG_INLINE
int64_t timestamp_from_pts_or_dts(int64_t pts, int64_t dts)
{
  /* Some videos do not have any pts values, use dts instead in those cases if
   * possible. Usually when this happens dts can act as pts because as all frames
   * should then be presented in their decoded in order. IE pts == dts. */
  if (pts == AV_NOPTS_VALUE) {
    return dts;
  }
  return pts;
}

FFMPEG_INLINE
int64_t av_get_pts_from_frame(AVFrame *picture)
{
  return timestamp_from_pts_or_dts(picture->pts, picture->pkt_dts);
}

/* -------------------------------------------------------------------- */
/** \name Deinterlace code block
 *
 * NOTE: The code in this block are from FFmpeg 2.6.4, which is licensed by LGPL.
 * \{ */

#define MAX_NEG_CROP 1024

#define times4(x) x, x, x, x
#define times256(x) times4(times4(times4(times4(times4(x)))))

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

#undef times4
#undef times256

/* filter parameters: [-1 4 2 4 -1] // 8 */
FFMPEG_INLINE
void deinterlace_line(uint8_t *dst,
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

FFMPEG_INLINE
void deinterlace_line_inplace(
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

/* deinterlacing : 2 temporal taps, 3 spatial taps linear filter. The
   top field is copied as is, but the bottom field is deinterlaced
   against the top field. */
FFMPEG_INLINE
void deinterlace_bottom_field(
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

FFMPEG_INLINE
int deinterlace_bottom_field_inplace(uint8_t *src1, int src_wrap, int width, int height)
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

FFMPEG_INLINE
int av_image_deinterlace(
    AVFrame *dst, const AVFrame *src, enum AVPixelFormat pix_fmt, int width, int height)
{
  int i, ret;

  if (pix_fmt != AV_PIX_FMT_YUV420P && pix_fmt != AV_PIX_FMT_YUVJ420P &&
      pix_fmt != AV_PIX_FMT_YUV422P && pix_fmt != AV_PIX_FMT_YUVJ422P &&
      pix_fmt != AV_PIX_FMT_YUV444P && pix_fmt != AV_PIX_FMT_YUV411P &&
      pix_fmt != AV_PIX_FMT_GRAY8) {
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

/** \} Deinterlace code block */

#endif
