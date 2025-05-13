/* SPDX-FileCopyrightText: 2024 Blender Authors
 *
 * SPDX-License-Identifier: GPL-2.0-or-later */

/** \file
 * \ingroup imbuf
 */

#pragma once

#include "BLI_math_matrix_types.hh"
#include "BLI_math_vector_types.hh"
#include "DNA_listBase.h"

namespace blender::ocio {
class ColorSpace;
class CPUProcessor;
}  // namespace blender::ocio

using ColorSpace = blender::ocio::ColorSpace;

struct ImBuf;

extern blender::float3 imbuf_luma_coefficients;
extern blender::float3x3 imbuf_scene_linear_to_xyz;
extern blender::float3x3 imbuf_xyz_to_scene_linear;
extern blender::float3x3 imbuf_scene_linear_to_aces;
extern blender::float3x3 imbuf_aces_to_scene_linear;
extern blender::float3x3 imbuf_scene_linear_to_rec709;
extern blender::float3x3 imbuf_rec709_to_scene_linear;

#define MAX_COLORSPACE_NAME 64

/* ** Initialization / De-initialization ** */

void colormanagement_init();
void colormanagement_exit();

void colormanage_cache_free(ImBuf *ibuf);

const ColorSpace *colormanage_colorspace_get_named(const char *name);
const ColorSpace *colormanage_colorspace_get_roled(int role);

void colormanage_imbuf_set_default_spaces(ImBuf *ibuf);
void colormanage_imbuf_make_linear(ImBuf *ibuf, const char *from_colorspace);
