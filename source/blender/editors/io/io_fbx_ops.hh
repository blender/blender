/* SPDX-FileCopyrightText: 2025 Blender Authors
 *
 * SPDX-License-Identifier: GPL-2.0-or-later */

/** \file
 * \ingroup editor/io
 */

#pragma once

struct wmOperatorType;

void WM_OT_fbx_import(wmOperatorType *ot);

namespace blender::ed::io {
void fbx_file_handler_add();
}
