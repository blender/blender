/*
 * ***** BEGIN GPL LICENSE BLOCK *****
 *
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
 * The Original Code is Copyright (C) 2009, Blender Foundation, Joshua Leung
 * This is a new part of Blender
 *
 * Contributor(s): Joshua Leung
 *
 * ***** END GPL LICENSE BLOCK *****
 */

/** \file blender/editors/gpencil/gpencil_ops.c
 *  \ingroup edgpencil
 */


#include <stdlib.h>
#include <stddef.h>
#include <stdio.h>

#include "BLO_sys_types.h"

#include "BLI_blenlib.h"

#include "WM_api.h"
#include "WM_types.h"

#include "RNA_access.h"

#include "ED_gpencil.h"

#include "gpencil_intern.h"

/* ****************************************** */
/* Generic Editing Keymap */

void ED_keymap_gpencil(wmKeyConfig *keyconf)
{
	wmKeyMap *keymap = WM_keymap_find(keyconf, "Grease Pencil", 0, 0);
	wmKeyMapItem *kmi;
	
	/* Draw */

	/* draw */
	kmi = WM_keymap_add_item(keymap, "GPENCIL_OT_draw", LEFTMOUSE, KM_PRESS, 0, DKEY);
	RNA_enum_set(kmi->ptr, "mode", GP_PAINTMODE_DRAW);

	/* draw - straight lines */
	kmi = WM_keymap_add_item(keymap, "GPENCIL_OT_draw", LEFTMOUSE, KM_PRESS, KM_CTRL, DKEY);
	RNA_enum_set(kmi->ptr, "mode", GP_PAINTMODE_DRAW_STRAIGHT);

	/* draw - poly lines */
	kmi = WM_keymap_add_item(keymap, "GPENCIL_OT_draw", RIGHTMOUSE, KM_PRESS, KM_CTRL, DKEY);
	RNA_enum_set(kmi->ptr, "mode", GP_PAINTMODE_DRAW_POLY);

	/* erase */
	kmi = WM_keymap_add_item(keymap, "GPENCIL_OT_draw", RIGHTMOUSE, KM_PRESS, 0, DKEY);
	RNA_enum_set(kmi->ptr, "mode", GP_PAINTMODE_ERASER);
}

/* ****************************************** */

void ED_operatortypes_gpencil(void)
{
	/* Drawing ----------------------- */
	
	WM_operatortype_append(GPENCIL_OT_draw);
	
	/* Editing (Buttons) ------------ */
	
	WM_operatortype_append(GPENCIL_OT_data_add);
	WM_operatortype_append(GPENCIL_OT_data_unlink);
	
	WM_operatortype_append(GPENCIL_OT_layer_add);
	
	WM_operatortype_append(GPENCIL_OT_active_frame_delete);
	
	WM_operatortype_append(GPENCIL_OT_convert);
	
	/* Editing (Time) --------------- */
}

/* ****************************************** */
