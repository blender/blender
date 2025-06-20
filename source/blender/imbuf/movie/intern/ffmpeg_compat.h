/* SPDX-FileCopyrightText: 2011 Peter Schlaile
 *
 * SPDX-License-Identifier: GPL-2.0-or-later */

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
#include <libavutil/cpu.h>
#include <libavutil/display.h>
#include <libswscale/swscale.h>

/* Check if our FFMPEG is new enough, avoids user complaints.
 * Minimum supported version is currently 3.2.0 which mean the following library versions:
 * `libavutil`   > 55.30
 * `libavcodec`  > 57.60
 * `libavformat` > 57.50
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

#if LIBAVUTIL_VERSION_INT < AV_VERSION_INT(58, 29, 100)
/* In FFMPEG 6.1 usage of the "key_frame" variable from "AVFrame" has been deprecated.
 * used the new method to query for the "AV_FRAME_FLAG_KEY" flag instead.
 */
#  define FFMPEG_OLD_KEY_FRAME_QUERY_METHOD
#endif

#if (LIBAVFORMAT_VERSION_MAJOR < 59)
/* For versions older than FFMPEG 5.0, use the old channel layout variables.
 * We intend to only keep this  workaround for around two releases (3.5, 3.6).
 * If it sticks around any longer, then we should consider refactoring this.
 */
#  define FFMPEG_USE_OLD_CHANNEL_VARS
#endif

/* Threaded sws_scale_frame was added in FFMPEG 5.0 (`swscale` version 6.1). */
#if (LIBSWSCALE_VERSION_INT >= AV_VERSION_INT(6, 1, 100))
#  define FFMPEG_SWSCALE_THREADING
#endif

/* AV_CODEC_CAP_AUTO_THREADS was renamed to AV_CODEC_CAP_OTHER_THREADS with
 * upstream commit
 * `github.com/FFmpeg/FFmpeg/commit/7d09579190def3ef7562399489e628f3b65714ce`
 * (`lavc` 58.132.100) and removed with commit
 * `github.com/FFmpeg/FFmpeg/commit/10c9a0874cb361336237557391d306d26d43f137`
 * for FFMPEG 6.0.
 */
#if LIBAVCODEC_VERSION_INT < AV_VERSION_INT(58, 132, 100)
#  define AV_CODEC_CAP_OTHER_THREADS AV_CODEC_CAP_AUTO_THREADS
#endif

#if (LIBAVFORMAT_VERSION_MAJOR < 58) || \
    ((LIBAVFORMAT_VERSION_MAJOR == 58) && (LIBAVFORMAT_VERSION_MINOR < 76))
#  define FFMPEG_USE_DURATION_WORKAROUND 1

/* Before FFMPEG 4.4, package duration calculation used deprecated variables to calculate the
 * packet duration. Use the function from commit
 * `github.com/FFmpeg/FFmpeg/commit/1c0885334dda9ee8652e60c586fa2e3674056586`
 * to calculate the correct frame-rate for FFMPEG < 4.4.
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

/*  Duration of the frame, in the same units as pts. 0 if unknown. */
FFMPEG_INLINE
int64_t av_get_frame_duration_in_pts_units(const AVFrame *picture)
{
#if LIBAVUTIL_VERSION_INT < AV_VERSION_INT(57, 30, 100)
  return picture->pkt_duration;
#else
  return picture->duration;
#endif
}

FFMPEG_INLINE size_t ffmpeg_get_buffer_alignment()
{
  /* NOTE: even if av_frame_get_buffer suggests to pass 0 for alignment,
   * as of FFMPEG 6.1/7.0 it does not use correct alignment for AVX512
   * CPU (frame.c get_video_buffer ends up always using 32 alignment,
   * whereas it should have used 64). Reported upstream:
   * https://trac.ffmpeg.org/ticket/11116 and the fix on their code
   * side is to use 64 byte alignment as soon as AVX512 is compiled
   * in (even if CPU might not support it). So play safe and
   * use at least 64 byte alignment here too. Currently larger than
   * 64 alignment would not happen anywhere, but keep on querying
   * av_cpu_max_align just in case some future platform might. */
  size_t align = av_cpu_max_align();
  if (align < 64) {
    align = 64;
  }
  return align;
}

FFMPEG_INLINE void ffmpeg_copy_display_matrix(const AVStream *src, AVStream *dst)
{
#if LIBAVCODEC_VERSION_INT >= AV_VERSION_INT(60, 29, 100)
  const AVPacketSideData *src_matrix = av_packet_side_data_get(src->codecpar->coded_side_data,
                                                               src->codecpar->nb_coded_side_data,
                                                               AV_PKT_DATA_DISPLAYMATRIX);
  if (src_matrix != nullptr) {
    uint8_t *dst_matrix = (uint8_t *)av_memdup(src_matrix->data, src_matrix->size);
    av_packet_side_data_add(&dst->codecpar->coded_side_data,
                            &dst->codecpar->nb_coded_side_data,
                            AV_PKT_DATA_DISPLAYMATRIX,
                            dst_matrix,
                            src_matrix->size,
                            0);
  }
#endif
}

FFMPEG_INLINE int ffmpeg_get_video_rotation(const AVStream *stream)
{
#if LIBAVCODEC_VERSION_INT >= AV_VERSION_INT(60, 29, 100)
  const AVPacketSideData *src_matrix = av_packet_side_data_get(
      stream->codecpar->coded_side_data,
      stream->codecpar->nb_coded_side_data,
      AV_PKT_DATA_DISPLAYMATRIX);
  if (src_matrix != nullptr) {
    /* ffmpeg reports rotation in [-180..+180] range; our image rotation
     * uses different direction and [0..360] range. */
    double theta = -av_display_rotation_get((const int32_t *)src_matrix->data);
    if (theta < 0.0) {
      theta += 360.0;
    }
    return int(theta);
  }
#endif
  return 0;
}

#if LIBAVCODEC_VERSION_INT >= AV_VERSION_INT(61, 13, 100)
FFMPEG_INLINE const enum AVPixelFormat *ffmpeg_get_pix_fmts(struct AVCodecContext *context,
                                                            const AVCodec *codec)
{
  const enum AVPixelFormat *pix_fmts = NULL;
  avcodec_get_supported_config(
      context, codec, AV_CODEC_CONFIG_PIX_FORMAT, 0, (const void **)&pix_fmts, NULL);
  return pix_fmts;
}

FFMPEG_INLINE const enum AVSampleFormat *ffmpeg_get_sample_fmts(struct AVCodecContext *context,
                                                                const AVCodec *codec)
{
  const enum AVSampleFormat *sample_fmts = NULL;
  avcodec_get_supported_config(
      context, codec, AV_CODEC_CONFIG_SAMPLE_FORMAT, 0, (const void **)&sample_fmts, NULL);
  return sample_fmts;
}

FFMPEG_INLINE const int *ffmpeg_get_sample_rates(struct AVCodecContext *context,
                                                 const AVCodec *codec)
{
  const int *sample_rates = NULL;
  avcodec_get_supported_config(
      context, codec, AV_CODEC_CONFIG_SAMPLE_RATE, 0, (const void **)&sample_rates, NULL);
  return sample_rates;
}
#else
FFMPEG_INLINE const enum AVPixelFormat *ffmpeg_get_pix_fmts(struct AVCodecContext * /*context*/,
                                                            const AVCodec *codec)
{
  return codec->pix_fmts;
}

FFMPEG_INLINE const enum AVSampleFormat *ffmpeg_get_sample_fmts(
    struct AVCodecContext * /*context*/, const AVCodec *codec)
{
  return codec->sample_fmts;
}

FFMPEG_INLINE const int *ffmpeg_get_sample_rates(struct AVCodecContext * /*context*/,
                                                 const AVCodec *codec)
{
  return codec->supported_samplerates;
}
#endif

#endif
