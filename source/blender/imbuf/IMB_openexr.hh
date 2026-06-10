/* SPDX-FileCopyrightText: 2024 Blender Authors
 *
 * SPDX-License-Identifier: GPL-2.0-or-later */

/** \file
 * \ingroup imbuf
 *
 * API for reading and writing multi-layer EXR files.
 */

#pragma once

#include "BLI_math_vector_types.hh"
#include "BLI_span.hh"
#include "BLI_string_ref.hh"
#include "BLI_sys_types.h"
#include "BLI_vector.hh"

struct ImBuf;

namespace blender {

/* XXX layer+pass name max 64? */
#define EXR_LAY_MAXNAME 64
#define EXR_PASS_MAXNAME 64
#define EXR_VIEW_MAXNAME 64
#define EXR_TOT_MAXNAME 64
/** Number of supported channels per pass (easy to change). */
#define EXR_PASS_MAXCHAN 24

struct StampData;
struct ExrReadHandle;
struct ExrWriteHandle;

/* -------------------------------------------------------------------- */
/** \name Write
 *
 * Wrapper for writing EXR files, used for single and multi-layer, single
 * and multi-view.
 * \{ */

ExrWriteHandle *IMB_exr_write_begin(bool write_multipart = false);

/** Add view to be written, must call before #IMB_exr_write_pass uses it. */
void IMB_exr_write_view(ExrWriteHandle *handle, const char *viewname);

/**
 * Add pass containing multiple channels to EXR file.
 * The number of channels is determined by channelnames.size() with
 * each character a channel name.
 * Layer and pass name, view name and colorspace are all optional.
 *
 * The #rect point must remain valid until after #IMB_exr_write_end.
 */
void IMB_exr_write_pass(ExrWriteHandle *handle,
                        StringRefNull layerpassname,
                        StringRefNull channelnames,
                        StringRefNull viewname,
                        StringRefNull colorspace,
                        size_t xstride,
                        size_t ystride,
                        const float *rect,
                        bool use_half_float);

/** Write and close the file. */
bool IMB_exr_write_end(ExrWriteHandle *handle,
                       const char *filepath,
                       int width,
                       int height,
                       const double ppm[2],
                       int compress,
                       int quality,
                       const StampData *stamp);

/** \} */

/* -------------------------------------------------------------------- */
/** \name Read
 * \{ */

/**
 * Open an EXR file for reading, returning a handle or null on failure. The
 * handle must be closed with #IMB_exr_close.
 */
ExrReadHandle *IMB_exr_open(const char *filepath);

/**
 * Same as #IMB_exr_open, but fails if file is not multi-layer.
 */
ExrReadHandle *IMB_exr_open_multilayer(const char *filepath);

/**
 * Same as #IMB_exr_open_multilayer, but opening from a memory buffer.
 * The memory buffer must stay alive until after #IMB_exr_close.
 */
ExrReadHandle *IMB_exr_open_multilayer_from_memory(const uchar *mem, size_t size);

/** Get pixels per meter metadata. */
bool IMB_exr_get_ppm(ExrReadHandle *handle, double ppm[2]);

/* Get display window metadata. The information is in the same structure
 * specified in ImBuf, see its documentation for more information. */
void IMB_exr_get_display_window(ExrReadHandle *handle,
                                int display_size[2],
                                int display_offset[2],
                                int data_offset[2]);

/** Get image resolution. */
int2 IMB_exr_get_size(ExrReadHandle *handle);

/* Close reader. */
void IMB_exr_close(ExrReadHandle *handle);

/** \} */

/* -------------------------------------------------------------------- */
/** \name Read passes
 * \{ */

/** One pass in an EXR file. */
struct ExrPassInfo {
  StringRefNull layer;
  StringRefNull pass;
  StringRefNull view;
  /** Channel layout, e.g. "RGBA", "XYZ", "V". */
  StringRefNull chan_id;
  int channels = 0;
  /** Image buffer for reading pass pixels. */
  ImBuf *ibuf = nullptr;
};

/**
 * Enumerate all passes in a multilayer EXR file, without reading any pixels
 * yet. The string refs are valid until #IMB_exr_close is called on the handle.
 */
Vector<ExrPassInfo> IMB_exr_get_passes(ExrReadHandle *handle);

/**
 * Enumerate all views in a multilayer EXR file.
 * The string refs are valid until #IMB_exr_close is called on the handle.
 */
Vector<StringRefNull> IMB_exr_get_views(ExrReadHandle *handle);

/**
 * Read pixel data into the #ibuf of each entry, which is created automatically
 * if null. If the ibuf was already provided, it must have the appropriate
 * dimensions and number of channels.
 */
void IMB_exr_read_passes(ExrReadHandle *handle,
                         MutableSpan<ExrPassInfo> entries,
                         const char *src_colorspace = nullptr,
                         bool predivide = false);

/** \} */

}  // namespace blender
