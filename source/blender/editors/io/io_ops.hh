/* SPDX-FileCopyrightText: 2007 Blender Foundation
 *
 * SPDX-License-Identifier: GPL-2.0-or-later */

/** \file
 * \ingroup editor/io
 */

#pragma once

struct PanelType;

bool IO_paneltype_set_parent(struct PanelType *panel);

void ED_operatortypes_io();
