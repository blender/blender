/* SPDX-FileCopyrightText: 2019 Blender Authors
 *
 * SPDX-License-Identifier: GPL-2.0-or-later */

#pragma once

/** \file
 * \ingroup editor/io
 */

struct wmOperatorType;

void WM_OT_usd_export(wmOperatorType *ot);
void WM_OT_usd_import(wmOperatorType *ot);
namespace blender::ed::io {
void usd_file_handler_add();
}
