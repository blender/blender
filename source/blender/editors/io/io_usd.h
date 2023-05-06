/* SPDX-License-Identifier: GPL-2.0-or-later
 * Copyright 2019 Blender Foundation */

#pragma once

/** \file
 * \ingroup editor/io
 */

struct wmOperatorType;

void WM_OT_usd_export(struct wmOperatorType *ot);

void WM_OT_usd_import(struct wmOperatorType *ot);

void WM_PT_USDExportPanelsRegister(void);
void WM_PT_USDImportPanelsRegister(void);

