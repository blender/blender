/* SPDX-FileCopyrightText: 2024 Blender Authors
 *
 * SPDX-License-Identifier: GPL-2.0-or-later */

/** \file
 * \ingroup imbuf
 */

#pragma once

namespace blender::ocio {
class ColorSpace;
class CPUProcessor;
}  // namespace blender::ocio

using ColorSpace = blender::ocio::ColorSpace;

struct ImBuf;
enum class ColorManagedFileOutput;

#define MAX_COLORSPACE_NAME 64

/* ** Initialization / De-initialization ** */

void colormanagement_init();
void colormanagement_exit();

void colormanage_cache_free(ImBuf *ibuf);

const ColorSpace *colormanage_colorspace_get_named(const char *name);
const ColorSpace *colormanage_colorspace_get_roled(int role);

void colormanage_imbuf_set_default_spaces(ImBuf *ibuf);
void colormanage_imbuf_make_linear(ImBuf *ibuf,
                                   const char *from_colorspace,
                                   ColorManagedFileOutput output);
