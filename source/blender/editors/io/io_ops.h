/* SPDX-License-Identifier: GPL-2.0-or-later
 * Copyright 2007 Blender Foundation. All rights reserved. */

/** \file
 * \ingroup editor/io
 */

#pragma once

#include <stdbool.h>

struct PanelType;


bool IO_paneltype_set_parent(struct PanelType *panel);
void ED_operatortypes_io(void);
