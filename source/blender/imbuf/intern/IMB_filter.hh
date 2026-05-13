/* SPDX-FileCopyrightText: 2001-2002 NaN Holding BV. All rights reserved.
 *
 * SPDX-License-Identifier: GPL-2.0-or-later */

/** \file
 * \ingroup imbuf
 * \brief Function declarations for `filter.cc`.
 */

#pragma once

#include <cstdint>

#include "DNA_image_enums.h"

namespace blender {

struct ImBuf;

void IMB_premultiply_rect(uint8_t *rect, ImColorMode color_mode, int w, int h);
void IMB_premultiply_rect_float(float *rect_float, int channels, int w, int h);

void IMB_unpremultiply_rect(uint8_t *rect, ImColorMode color_mode, int w, int h);
void IMB_unpremultiply_rect_float(float *rect_float, int channels, int w, int h);

}  // namespace blender
