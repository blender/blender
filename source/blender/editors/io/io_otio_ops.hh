/* SPDX-FileCopyrightText: 2026 Blender Authors
 *
 * SPDX-License-Identifier: GPL-2.0-or-later */

#pragma once

/** \file
 * \ingroup editor/io
 */

namespace blender {
struct wmOperatorType;

void WM_OT_otio_export(wmOperatorType *ot);
void WM_OT_otio_import(wmOperatorType *ot);

namespace ed::io {
void otio_file_handler_add();
}

}  // namespace blender
