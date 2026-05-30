/* SPDX-FileCopyrightText: 2026 Blender Authors
 *
 * SPDX-License-Identifier: GPL-2.0-or-later */

/** \file
 * \ingroup editor/io
 */

#pragma once

namespace blender {

struct wmOperatorType;

void WM_OT_spz_import(wmOperatorType *ot);

namespace ed::io {
void spz_file_handler_add();
}

}  // namespace blender
