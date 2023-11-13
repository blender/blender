/* SPDX-FileCopyrightText: 2023 Blender Authors
 *
 * SPDX-License-Identifier: GPL-2.0-or-later */

#pragma once

#include <memory>

/* Include our own math header first to avoid warnings about M_PI
 * redefinition between OpenImageIO and Windows headers. */
#include "BLI_math_base.h"
#include "BLI_sys_types.h"

#include <OpenImageIO/filesystem.h>
#include <OpenImageIO/imageio.h>

#include "IMB_imbuf.h"
#include "IMB_imbuf_types.h"

namespace blender::imbuf {

/**
 * Parameters and settings used while reading image formats.
 */
struct ReadContext {
  const uchar *mem_start;
  const size_t mem_size;
  const char *file_format;
  const eImbFileType file_type;
  const int flags;

  /** Override the automatic color-role choice with the value specified here. */
  int use_colorspace_role = -1;

  /** Allocate and use all #ImBuf image planes even if the image has fewer. */
  bool use_all_planes = false;

  /** Use the `colorspace` provided in the image metadata when available. */
  bool use_embedded_colorspace = false;
};

/**
 * Parameters and settings used while writing image formats.
 */
struct WriteContext {
  const char *file_format;
  ImBuf *ibuf;
  int flags;

  uchar *mem_start;
  OIIO::stride_t mem_xstride;
  OIIO::stride_t mem_ystride;
  OIIO::ImageSpec mem_spec;
};

/**
 * Check to see if we can load and open the given file format.
 */
bool imb_oiio_check(const uchar *mem, size_t mem_size, const char *file_format);

/**
 * The primary method for reading data into an #ImBuf.
 *
 * During the `IB_test` phase of loading, the `colorspace` parameter will be populated
 * with the appropriate `colorspace` name.
 *
 * Upon return, the `r_newspec` parameter will contain image format information
 * which can be inspected afterwards if necessary.
 */
ImBuf *imb_oiio_read(const ReadContext &ctx,
                     const OIIO::ImageSpec &config,
                     char colorspace[IM_MAX_SPACE],
                     OIIO::ImageSpec &r_newspec);

/**
 * The primary method for writing data from an #ImBuf to either a physical or in-memory
 * destination.
 *
 * The `file_spec` parameter will typically come from #imb_create_write_spec.
 */
bool imb_oiio_write(const WriteContext &ctx,
                    const char *filepath,
                    const OIIO::ImageSpec &file_spec);

/**
 * Create a #WriteContext based on the provided #ImBuf and format information.
 *
 * If the provided #ImBuf contains both byte and float buffers, the `prefer_float`
 * flag controls which buffer to use. By default, if a float buffer exists it will
 * be used.
 */
WriteContext imb_create_write_context(const char *file_format,
                                      ImBuf *ibuf,
                                      int flags,
                                      bool prefer_float = true);

/**
 * Returns an #ImageSpec filled in with all common attributes associated with the #ImBuf
 * provided as part of the #WriteContext.
 *
 * This includes optional metadata that has been attached to the #ImBuf and which should be
 * written to the new file as necessary.
 */
OIIO::ImageSpec imb_create_write_spec(const WriteContext &ctx,
                                      int file_channels,
                                      OIIO::TypeDesc data_format);

}  // namespace blender::imbuf
