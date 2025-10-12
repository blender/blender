/* SPDX-FileCopyrightText: 2024 Blender Authors
 *
 * SPDX-License-Identifier: GPL-2.0-or-later */

/** \file
 * \ingroup imbuf
 *
 * API for reading and writing multi-layer EXR files.
 */

#pragma once

#include "BLI_string_ref.hh"

/* XXX layer+pass name max 64? */
#define EXR_LAY_MAXNAME 64
#define EXR_PASS_MAXNAME 64
#define EXR_VIEW_MAXNAME 64
#define EXR_TOT_MAXNAME 64
/** Number of supported channels per pass (easy to change). */
#define EXR_PASS_MAXCHAN 24

struct StampData;
struct ExrHandle;

ExrHandle *IMB_exr_get_handle(bool write_multipart = false);

/**
 * Add multiple channels to EXR file.
 * The number of channels is determined by channelnames.size() with
 * each character a channel name.
 * Layer and pass name, view name and colorspace are all optional.
 */
void IMB_exr_add_channels(ExrHandle *handle,
                          blender::StringRefNull layerpassname,
                          blender::StringRefNull channelnames,
                          blender::StringRefNull viewname,
                          blender::StringRefNull colorspace,
                          size_t xstride,
                          size_t ystride,
                          float *rect,
                          bool use_half_float);

/**
 * Read from file.
 */
bool IMB_exr_begin_read(
    ExrHandle *handle, const char *filepath, int *width, int *height, bool parse_channels);
/**
 * Used for output files (from #RenderResult) (single and multi-layer, single and multi-view).
 */
bool IMB_exr_begin_write(ExrHandle *handle,
                         const char *filepath,
                         int width,
                         int height,
                         const double ppm[2],
                         int compress,
                         int quality,
                         const StampData *stamp);

/**
 * For reading, set the rect buffer to write into.
 * \param passname: Full channel name including layer, pass, view and channel.
 */
bool IMB_exr_set_channel(
    ExrHandle *handle, blender::StringRefNull full_name, int xstride, int ystride, float *rect);

void IMB_exr_read_channels(ExrHandle *handle);
void IMB_exr_write_channels(ExrHandle *handle);

void IMB_exr_multilayer_convert(ExrHandle *handle,
                                void *base,
                                void *(*addview)(void *base, const char *str),
                                void *(*addlayer)(void *base, const char *str),
                                void (*addpass)(void *base,
                                                void *lay,
                                                const char *str,
                                                float *rect,
                                                int totchan,
                                                const char *chan_id,
                                                const char *view));

void IMB_exr_close(ExrHandle *handle);

void IMB_exr_add_view(ExrHandle *handle, const char *name);

bool IMB_exr_has_multilayer(ExrHandle *handle);

bool IMB_exr_get_ppm(ExrHandle *handle, double ppm[2]);
