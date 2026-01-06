/* SPDX-FileCopyrightText: 2023 Blender Authors
 *
 * SPDX-License-Identifier: GPL-2.0-or-later */

/** \file
 * \ingroup editor/io
 */

#pragma once

namespace blender {

struct wmOperatorType;

void WM_OT_obj_export(wmOperatorType *ot);
void WM_OT_obj_import(wmOperatorType *ot);

namespace ed::io {
void obj_file_handler_add();
}

}  // namespace blender
