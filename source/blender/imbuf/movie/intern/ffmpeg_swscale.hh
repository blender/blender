/* SPDX-FileCopyrightText: 2024 Blender Authors
 *
 * SPDX-License-Identifier: GPL-2.0-or-later */

#pragma once

/** \file
 * \ingroup imbuf
 */

#ifdef WITH_FFMPEG

struct AVFrame;
struct SwsContext;

/**
 * Gets a `libswscale` context for given size and format parameters.
 * After you're done using the context, call #ffmpeg_sws_release_context
 * to release it. Internally the contexts are coming from the context
 * pool/cache.
 *
 * \param src_full_range: whether source uses full (pc/jpeg) range or limited (tv/mpeg) range.
 *
 * \param src_color_space: -1 for defaults, or AVColorSpace value to override
 * sws_setColorspaceDetails `inv_table`.
 *
 * \param dst_full_range: whether destination uses full (pc/jpeg) range or limited (tv/mpeg) range.
 *
 * \param dst_color_space: -1 for defaults, or AVColorSpace value to override
 * sws_setColorspaceDetails `table`.
 */
SwsContext *ffmpeg_sws_get_context(int src_width,
                                   int src_height,
                                   int av_src_format,
                                   bool src_full_range,
                                   int src_color_space,
                                   int dst_width,
                                   int dst_height,
                                   int av_dst_format,
                                   bool dst_full_range,
                                   int dst_color_space,
                                   int sws_flags);
void ffmpeg_sws_release_context(SwsContext *ctx);

void ffmpeg_sws_scale_frame(SwsContext *ctx, AVFrame *dst, const AVFrame *src);

void ffmpeg_sws_exit();

#endif /* WITH_FFMPEG */
