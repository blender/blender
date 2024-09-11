/* SPDX-FileCopyrightText: 2024 Blender Authors
 *
 * SPDX-License-Identifier: GPL-2.0-or-later */

/** \file
 * \ingroup editor/io
 */

#pragma once

struct wmOperatorType;

void WM_OT_grease_pencil_import_svg(wmOperatorType *ot);

#ifdef WITH_PUGIXML
void WM_OT_grease_pencil_export_svg(wmOperatorType *ot);
#endif
#ifdef WITH_HARU
void WM_OT_grease_pencil_export_pdf(wmOperatorType *ot);
#endif

namespace blender::ed::io {
void grease_pencil_file_handler_add();
}
