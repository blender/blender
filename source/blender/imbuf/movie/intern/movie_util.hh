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
#  include <libavutil/pixfmt.h>
}
#  include "DNA_scene_types.h"

struct AVFrame;

int ffmpeg_deinterlace(
    AVFrame *dst, const AVFrame *src, enum AVPixelFormat pix_fmt, int width, int height);

const char *ffmpeg_last_error();
AVCodecID mov_av_codec_id_get(IMB_Ffmpeg_Codec_ID id);

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
int MOV_codec_valid_bit_depths(AVCodecID codec_id);

#endif /* WITH_FFMPEG */
