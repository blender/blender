/* SPDX-FileCopyrightText: 2024 Blender Authors
 *
 * SPDX-License-Identifier: GPL-2.0-or-later */

/** \file
 * \ingroup imbuf
 */

#pragma once

#include "DNA_scene_types.h"

struct FFMpegCodecData;
struct ImageFormatData;
struct RenderData;

/** Global initialization of movie support. */
void MOV_init();

/** Global de-initialization of movie support. */
void MOV_exit();

/**
 * Test if the file is a video file (known format, has a video stream and
 * supported video codec). Note that this can be pretty expensive: it is
 * not just a file extension check, it will literally try to decode
 * file headers and find whether it is a video file with some supported
 * codec.
 */
bool MOV_is_movie_file(const char *filepath);

/** Checks whether given FFMPEG codec and profile combination supports alpha channel (RGBA). */
bool MOV_codec_supports_alpha(IMB_Ffmpeg_Codec_ID codec_id, int ffmpeg_profile);

/**
 * Checks whether given FFMPEG video AVCodecID supports CRF (i.e. "quality level")
 * setting. For codecs that do not support constant quality, only target bit-rate
 * can be specified.
 */
bool MOV_codec_supports_crf(IMB_Ffmpeg_Codec_ID codec_id);

/**
 * Which pixel bit depths are supported by a given FFMPEG video CodecID.
 * Returns bit-mask of `R_IMF_CHAN_DEPTH_` flags.
 */
int MOV_codec_valid_bit_depths(IMB_Ffmpeg_Codec_ID codec_id);

/**
 * Given desired output image format type, sets up required FFMPEG
 * related settings in render data.
 */
void MOV_validate_output_settings(RenderData *rd, const ImageFormatData *imf);
