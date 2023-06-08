/* SPDX-FileCopyrightText: 2016 Blender Foundation
 *
 * SPDX-License-Identifier: GPL-2.0-or-later */

#pragma once

/** \file
 * \ingroup editor/io
 */

struct wmOperatorType;

void WM_OT_alembic_export(struct wmOperatorType *ot);
void WM_OT_alembic_import(struct wmOperatorType *ot);
