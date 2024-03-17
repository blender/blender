/* SPDX-FileCopyrightText: 2024 Blender Authors
 *
 * SPDX-License-Identifier: GPL-2.0-or-later */

/** \file
 * \ingroup imbuf
 */

#pragma once

/* API for reading and writing multilayer EXR files. */

/* XXX layer+pass name max 64? */
/* This api also supports max 8 channels per pass now. easy to fix! */
#define EXR_LAY_MAXNAME 64
#define EXR_PASS_MAXNAME 64
#define EXR_VIEW_MAXNAME 64
#define EXR_TOT_MAXNAME 64
#define EXR_PASS_MAXCHAN 24

struct StampData;

void *IMB_exr_get_handle();
void *IMB_exr_get_handle_name(const char *name);

/**
 * Adds flattened #ExrChannel's
 * `xstride`, `ystride` and `rect` can be done in set_channel too, for tile writing.
 * \param passname: Does not include view.
 */
void IMB_exr_add_channel(void *handle,
                         const char *layname,
                         const char *passname,
                         const char *view,
                         int xstride,
                         int ystride,
                         float *rect,
                         bool use_half_float);

/**
 * Read from file.
 */
bool IMB_exr_begin_read(
    void *handle, const char *filepath, int *width, int *height, bool parse_channels);
/**
 * Used for output files (from #RenderResult) (single and multi-layer, single and multi-view).
 */
bool IMB_exr_begin_write(void *handle,
                         const char *filepath,
                         int width,
                         int height,
                         int compress,
                         const StampData *stamp);
/**
 * Only used for writing temp. render results (not image files)
 * (FSA and Save Buffers).
 */
void IMB_exrtile_begin_write(
    void *handle, const char *filepath, int mipmap, int width, int height, int tilex, int tiley);

/**
 * Still clumsy name handling, layers/channels can be ordered as list in list later.
 *
 * \param passname: Here is the raw channel name without the layer.
 */
bool IMB_exr_set_channel(void *handle,
                         const char *layname,
                         const char *passname,
                         int xstride,
                         int ystride,
                         float *rect);
float *IMB_exr_channel_rect(void *handle,
                            const char *layname,
                            const char *passname,
                            const char *view);

void IMB_exr_read_channels(void *handle);
void IMB_exr_write_channels(void *handle);
/**
 * Temporary function, used for FSA and Save Buffers.
 * called once per `tile * view`.
 */
void IMB_exrtile_write_channels(
    void *handle, int partx, int party, int level, const char *viewname, bool empty);
void IMB_exr_clear_channels(void *handle);

void IMB_exr_multilayer_convert(void *handle,
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

void IMB_exr_close(void *handle);

void IMB_exr_add_view(void *handle, const char *name);

bool IMB_exr_has_multilayer(void *handle);
