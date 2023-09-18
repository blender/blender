/* SPDX-FileCopyrightText: 2016 Blender Authors
 *
 * SPDX-License-Identifier: GPL-2.0-or-later */

#pragma once

/** \file
 * \ingroup editor/io
 */

struct wmOperatorType;

void CACHEFILE_OT_open(wmOperatorType *ot);
void CACHEFILE_OT_reload(wmOperatorType *ot);

void CACHEFILE_OT_layer_add(wmOperatorType *ot);
void CACHEFILE_OT_layer_remove(wmOperatorType *ot);
void CACHEFILE_OT_layer_move(wmOperatorType *ot);
