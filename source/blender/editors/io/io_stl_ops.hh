/* SPDX-FileCopyrightText: 2023 Blender Foundation
 *
 * SPDX-License-Identifier: GPL-2.0-or-later */

/** \file
 * \ingroup editor/io
 */

#pragma once

struct wmOperatorType;

void WM_OT_stl_export(struct wmOperatorType *ot);
void WM_OT_stl_import(struct wmOperatorType *ot);
