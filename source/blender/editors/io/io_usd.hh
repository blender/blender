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

void WM_PT_USDExportPanelsRegister(void);
void WM_PT_USDImportPanelsRegister(void);
