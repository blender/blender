/* SPDX-FileCopyrightText: 2024 Blender Authors
 *
 * SPDX-License-Identifier: GPL-2.0-or-later */

#pragma once

/** \file
 * \ingroup imbuf
 */

#ifdef WITH_FFMPEG

extern "C" {
#  include <libavutil/pixfmt.h>
}

struct AVFrame;

int ffmpeg_deinterlace(
    AVFrame *dst, const AVFrame *src, enum AVPixelFormat pix_fmt, int width, int height);

const char *ffmpeg_last_error();

#endif /* WITH_FFMPEG */
