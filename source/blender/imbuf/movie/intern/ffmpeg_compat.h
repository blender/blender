/* SPDX-FileCopyrightText: 2011 Peter Schlaile
 *
 * SPDX-License-Identifier: GPL-2.0-or-later */

/** \file
 * \ingroup ffmpeg
 *
 * Compatibility helpers for supported FFmpeg versions and shared FFmpeg headers.
 */

#pragma once

#include <libavcodec/avcodec.h>
#include <libavformat/avformat.h>
#include <libavutil/cpu.h>
#include <libavutil/display.h>
#include <libswscale/swscale.h>

/* Mininum FFmpeg we attempt to support is 7.1 (libavformat 61.7.100). */
#if LIBAVFORMAT_VERSION_INT < AV_VERSION_INT(61, 7, 100)
#  error "FFmpeg 7.1 or newer is needed, upgrade your FFmpeg or disable it"
#endif

static inline int64_t timestamp_from_pts_or_dts(int64_t pts, int64_t dts)
{
  /* Some videos do not have any pts values, use dts instead in those cases if
   * possible. Usually when this happens dts can act as pts because as all frames
   * should then be presented in their decoded in order. IE pts == dts. */
  if (pts == AV_NOPTS_VALUE) {
    return dts;
  }
  return pts;
}

static inline int64_t av_get_pts_from_frame(AVFrame *picture)
{
  return timestamp_from_pts_or_dts(picture->pts, picture->pkt_dts);
}

static inline size_t ffmpeg_get_buffer_alignment()
{
  /* Use consistent alignment for FFmpeg frames and Blender-owned pixel buffers.
   * Use a minimum of 64 bytes for potential SIMD like AVX512.
   * Query av_cpu_max_align() as well in case a platform requires stronger alignment. */
  size_t align = av_cpu_max_align();
  if (align < 64) {
    align = 64;
  }
  return align;
}

static inline void ffmpeg_copy_display_matrix(const AVStream *src, AVStream *dst)
{
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
}

static inline int ffmpeg_get_video_rotation(const AVStream *stream)
{
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
  return 0;
}

static inline const enum AVPixelFormat *ffmpeg_get_pix_fmts(struct AVCodecContext *context,
                                                            const AVCodec *codec)
{
  const enum AVPixelFormat *pix_fmts = nullptr;
  avcodec_get_supported_config(
      context, codec, AV_CODEC_CONFIG_PIX_FORMAT, 0, (const void **)&pix_fmts, nullptr);
  return pix_fmts;
}

static inline const enum AVSampleFormat *ffmpeg_get_sample_fmts(struct AVCodecContext *context,
                                                                const AVCodec *codec)
{
  const enum AVSampleFormat *sample_fmts = nullptr;
  avcodec_get_supported_config(
      context, codec, AV_CODEC_CONFIG_SAMPLE_FORMAT, 0, (const void **)&sample_fmts, nullptr);
  return sample_fmts;
}

static inline const int *ffmpeg_get_sample_rates(struct AVCodecContext *context,
                                                 const AVCodec *codec)
{
  const int *sample_rates = nullptr;
  avcodec_get_supported_config(
      context, codec, AV_CODEC_CONFIG_SAMPLE_RATE, 0, (const void **)&sample_rates, nullptr);
  return sample_rates;
}
