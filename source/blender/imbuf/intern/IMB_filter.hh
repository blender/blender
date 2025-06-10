/* SPDX-FileCopyrightText: 2001-2002 NaN Holding BV. All rights reserved.
 *
 * SPDX-License-Identifier: GPL-2.0-or-later */

/** \file
 * \ingroup imbuf
 * \brief Function declarations for `filter.cc`.
 */

#pragma once

#include <cstdint>

struct ImBuf;

void IMB_premultiply_rect(uint8_t *rect, char planes, int w, int h);
void IMB_premultiply_rect_float(float *rect_float, int channels, int w, int h);

void IMB_unpremultiply_rect(uint8_t *rect, char planes, int w, int h);
void IMB_unpremultiply_rect_float(float *rect_float, int channels, int w, int h);
