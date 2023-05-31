/* SPDX-FileCopyrightText: 2001-2002 NaN Holding BV. All rights reserved.
 *
 * SPDX-License-Identifier: GPL-2.0-or-later */

/** \file
 * \ingroup edtransform
 */

#pragma once

/* Callbacks for #WM_paint_cursor_activate */

/**
 * Poll callback for cursor drawing:
 * #WM_paint_cursor_activate
 */
bool transform_draw_cursor_poll(struct bContext *C);
/**
 * Cursor and help-line drawing, callback for:
 * #WM_paint_cursor_activate
 */
void transform_draw_cursor_draw(struct bContext *C, int x, int y, void *customdata);
