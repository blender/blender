/*
 * This program is free software; you can redistribute it and/or
 * modify it under the terms of the GNU General Public License
 * as published by the Free Software Foundation; either version 2
 * of the License, or (at your option) any later version.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with this program; if not, write to the Free Software Foundation,
 * Inc., 51 Franklin Street, Fifth Floor, Boston, MA 02110-1301, USA.
 */

/** \file
 * \ingroup modifiers
 */

#pragma once

#include "DNA_windowmanager_types.h"

/* Operator types should be in exposed header. */
void OBJECT_OT_lineart_bake_strokes(struct wmOperatorType *ot);
void OBJECT_OT_lineart_bake_strokes_all(struct wmOperatorType *ot);
void OBJECT_OT_lineart_clear(struct wmOperatorType *ot);
void OBJECT_OT_lineart_clear_all(struct wmOperatorType *ot);

void WM_operatortypes_lineart(void);
