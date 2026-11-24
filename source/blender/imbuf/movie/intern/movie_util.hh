/* SPDX-FileCopyrightText: 2024 Blender Authors
 *
 * SPDX-License-Identifier: GPL-2.0-or-later */

#pragma once

/** \file
 * \ingroup imbuf
 */

#ifdef WITH_FFMPEG

extern "C" {
#  include <libavcodec/avcodec.h>
#  include <libavutil/hwcontext.h>
#  include <libavutil/pixfmt.h>
}
#  include "DNA_scene_types.h"

struct AVFrame;
struct AVStream;
struct AVFormatContext;

namespace blender {

int ffmpeg_deinterlace(
    AVFrame *dst, const AVFrame *src, enum AVPixelFormat pix_fmt, int width, int height);

/**
 * Whether `stream` should be treated as a loadable video stream. Only count attached pictures
 * (cover art / embedded thumbnails) if there are no other "real" video streams in the container as
 * a fallback.
 */
bool ffmpeg_stream_counts_as_video(const AVFormatContext *format_ctx, const AVStream *stream);

const char *ffmpeg_last_error();
AVCodecID mov_av_codec_id_get(IMB_Ffmpeg_Codec_ID id);
AVBufferRef *ffmpeg_hw_device_get(AVHWDeviceType device_type);

struct FFmpegHardwareEncoder {
  const AVCodec *codec = nullptr;
  const AVCodecHWConfig *config = nullptr;
};

/**
 * Find the preferred hardware encoder for the codec and software pixel format (NV12 or
 * P010) that can actually be used on this system.
 * Returns a struct with null codec if none are available.
 */
FFmpegHardwareEncoder ffmpeg_hw_encoder_find(AVCodecID codec_id, AVPixelFormat sw_format);

/**
 * Create a hardware frames context for the encoder's width and height, and assign it (and the
 * hardware pixel format) to the codec context. Returns negative ffmpeg error code on failure.
 */
int ffmpeg_hw_setup_frames(AVCodecContext *codec_ctx,
                           AVBufferRef *hw_device_ctx,
                           const AVCodecHWConfig *config,
                           AVPixelFormat sw_format);

/** Software pixel format used for uploading frames to hardware encoders. */
AVPixelFormat ffmpeg_hw_encode_sw_format(int depth);

/** `crf` is negative for bitrate based rate control; `min_rate` and `bitrate` are in kbit/s. */
const char *ffmpeg_hw_encode_settings_unsupported_reason(
    AVCodecID codec_id, int crf, int min_rate, int bitrate, int depth, int width, int height);

/** Checks whether given FFMPEG codec and profile combination supports alpha channel (RGBA). */
bool MOV_codec_supports_alpha(AVCodecID codec_id, int ffmpeg_profile);

/**
 * Checks whether given FFMPEG video AVCodecID supports CRF (i.e. "quality level")
 * setting. For codecs that do not support constant quality, only target bit-rate
 * can be specified.
 */
bool MOV_codec_supports_crf(AVCodecID codec_id);

/**
 * Which pixel bit depths are supported by a given FFMPEG video CodecID.
 * Returns bit-mask of `R_IMF_CHAN_DEPTH_` flags.
 */
eImageFormatDepth MOV_codec_valid_bit_depths(AVCodecID codec_id);

/**
 * Returns thread count to be used for ffmpeg.
 */
int MOV_thread_count();

}  // namespace blender

#endif /* WITH_FFMPEG */
