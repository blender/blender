/* SPDX-FileCopyrightText: 2023 Blender Foundation
 *
 * SPDX-License-Identifier: GPL-2.0-or-later */

/** \file
 * \ingroup edgreasepencil
 */

#include "ED_grease_pencil.h"

void ED_operatortypes_grease_pencil(void)
{
  ED_operatortypes_grease_pencil_draw();
  ED_operatortypes_grease_pencil_layers();
  ED_operatortypes_grease_pencil_select();
}
