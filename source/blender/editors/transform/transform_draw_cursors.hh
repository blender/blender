/* SPDX-FileCopyrightText: 2001-2002 NaN Holding BV. All rights reserved.
 *
 * SPDX-License-Identifier: GPL-2.0-or-later */

/** \file
 * \ingroup edtransform
 */

#pragma once

#include "BLI_math_vector_types.hh"

namespace blender {

struct bContext;

namespace ed::transform {

/* Callbacks for #WM_paint_cursor_activate. */

/**
 * Poll callback for cursor drawing:
 * #WM_paint_cursor_activate
 */
bool transform_draw_cursor_poll(bContext *C);
/**
 * Cursor and help-line drawing, callback for:
 * #WM_paint_cursor_activate
 */
void transform_draw_cursor_draw(bContext *C, const int2 &xy, const float2 &tilt, void *customdata);

}  // namespace ed::transform
}  // namespace blender
