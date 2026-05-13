/* SPDX-FileCopyrightText: 2026 Blender Authors
 *
 * SPDX-License-Identifier: GPL-2.0-or-later */

/** \file
 * \ingroup pythonintern
 */

#pragma once

#include <Python.h>

namespace blender {

extern PyMethodDef BPY_rna_windowmanager_draw_cursor_add_method_def;
extern PyMethodDef BPY_rna_windowmanager_draw_cursor_remove_method_def;
extern PyGetSetDef BPY_rna_windowmanager_clipboard_getset_def;

extern PyMethodDef BPY_rna_window_screenshot_method_def;

}  // namespace blender
