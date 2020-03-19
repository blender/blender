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
 *
 * The Original Code is Copyright (C) 2008 Blender Foundation.
 * All rights reserved.
 */

/** \file
 * \ingroup edlattice
 */

#include "DNA_scene_types.h"

#include "RNA_access.h"

#include "WM_api.h"
#include "WM_types.h"

#include "ED_lattice.h"
#include "ED_object.h"
#include "ED_screen.h"
#include "ED_select_utils.h"

#include "lattice_intern.h"

void ED_operatortypes_lattice(void)
{
  WM_operatortype_append(LATTICE_OT_select_all);
  WM_operatortype_append(LATTICE_OT_select_more);
  WM_operatortype_append(LATTICE_OT_select_less);
  WM_operatortype_append(LATTICE_OT_select_ungrouped);
  WM_operatortype_append(LATTICE_OT_select_random);
  WM_operatortype_append(LATTICE_OT_select_mirror);
  WM_operatortype_append(LATTICE_OT_make_regular);
  WM_operatortype_append(LATTICE_OT_flip);
}

void ED_keymap_lattice(wmKeyConfig *keyconf)
{
  wmKeyMap *keymap = WM_keymap_ensure(keyconf, "Lattice", 0, 0);
  keymap->poll = ED_operator_editlattice;
}
