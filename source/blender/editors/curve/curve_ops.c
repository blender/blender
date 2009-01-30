/**
 * $Id:
 *
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
 * Inc., 59 Temple Place - Suite 330, Boston, MA  02111-1307, USA.
 *
 * The Original Code is Copyright (C) 2009 Blender Foundation.
 * All rights reserved.
 *
 * 
 * Contributor(s): Blender Foundation
 *
 * ***** END GPL LICENSE BLOCK *****
 */

#include <stdlib.h>
#include <math.h>

#include "MEM_guardedalloc.h"

#include "DNA_object_types.h"
#include "DNA_scene_types.h"
#include "DNA_screen_types.h"
#include "DNA_space_types.h"
#include "DNA_userdef_types.h"
#include "DNA_view3d_types.h"
#include "DNA_windowmanager_types.h"

#include "BLI_arithb.h"
#include "BLI_blenlib.h"

#include "BKE_context.h"
#include "BKE_global.h"
#include "BKE_utildefines.h"

#include "RNA_access.h"
#include "RNA_define.h"

#include "WM_api.h"
#include "WM_types.h"

#include "ED_screen.h"
#include "ED_object.h"

#include "curve_intern.h"


/* ************************** registration **********************************/


void ED_operatortypes_curve(void)
{
	WM_operatortype_append(FONT_OT_textedit);
}

void ED_keymap_curve(wmWindowManager *wm)
{
	ListBase *keymap= WM_keymap_listbase(wm, "Font", 0, 0);
	
	/* only set in editmode font, by space_view3d listener */
	WM_keymap_add_item(keymap, "FONT_OT_textedit", KM_TEXTINPUT, KM_ANY, KM_ANY, 0);

	/* only set in editmode curve, by space_view3d listener */
	keymap= WM_keymap_listbase(wm, "Curve", 0, 0);
	
	WM_keymap_add_item(keymap, "OBJECT_OT_curve_add", AKEY, KM_PRESS, KM_SHIFT, 0);

}

