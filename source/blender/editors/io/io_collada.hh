/* SPDX-FileCopyrightText: 2007 Blender Authors
 *
 * SPDX-License-Identifier: GPL-2.0-or-later */

/** \file
 * \ingroup editor/io
 */

#pragma once

struct wmOperatorType;

void WM_OT_collada_export(wmOperatorType *ot);
void WM_OT_collada_import(wmOperatorType *ot);

namespace blender::ed::io {
void collada_file_handler_add();
}
