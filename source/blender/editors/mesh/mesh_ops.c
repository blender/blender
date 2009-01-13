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
#include "BLI_editVert.h"

#include "BKE_context.h"
#include "BKE_global.h"
#include "BKE_utildefines.h"

#include "RNA_access.h"
#include "RNA_define.h"

#include "WM_api.h"
#include "WM_types.h"

#include "ED_screen.h"
#include "ED_mesh.h"
#include "ED_view3d.h"

#include "mesh_intern.h"


/* ************************** registration **********************************/

void ED_operatortypes_mesh(void)
{
	WM_operatortype_append(MESH_OT_de_select_all);

	WM_operatortype_append(MESH_OT_select_more);

	WM_operatortype_append(MESH_OT_select_less);
	
	WM_operatortype_append(MESH_OT_selectswap_mesh);
	
	WM_operatortype_append(MESH_OT_select_non_manifold);
	
	WM_operatortype_append(MESH_OT_selectconnected_mesh_all);
	
	WM_operatortype_append(MESH_OT_selectconnected_mesh);
	
	WM_operatortype_append(MESH_OT_hide_mesh);
	
	WM_operatortype_append(MESH_OT_reveal_mesh);
	
	WM_operatortype_append(MESH_OT_righthandfaces);
	
	WM_operatortype_append(MESH_OT_select_linked_flat_faces);
	
	WM_operatortype_append(MESH_OT_select_sharp_edges);

}

/* note mesh keymap also for other space? */
void ED_keymap_mesh(wmWindowManager *wm)
{
	ListBase *keymap= WM_keymap_listbase(wm, "EditMesh", 0, 0);
	
	WM_keymap_add_item(keymap, "MESH_OT_de_select_all", AKEY, KM_PRESS, 0, 0);
	
	WM_keymap_add_item(keymap, "MESH_OT_select_more", PADPLUSKEY, KM_PRESS, KM_CTRL, 0);

	WM_keymap_add_item(keymap, "MESH_OT_select_less", PADMINUS, KM_PRESS, KM_CTRL, 0);
	
	WM_keymap_add_item(keymap, "MESH_OT_selectswap_mesh", IKEY, KM_PRESS, KM_CTRL, 0);
	
	WM_keymap_add_item(keymap, "MESH_OT_select_non_manifold", MKEY, KM_PRESS, (KM_CTRL|KM_SHIFT|KM_ALT), 0);
	
	WM_keymap_add_item(keymap, "MESH_OT_selectconnected_mesh_all", LKEY, KM_PRESS, KM_CTRL, 0);
	
	WM_keymap_add_item(keymap, "MESH_OT_selectconnected_mesh", LKEY, KM_PRESS, 0, 0);
	
	WM_keymap_add_item(keymap, "MESH_OT_hide_mesh", HKEY, KM_PRESS, 0, 0);
	
	RNA_boolean_set(WM_keymap_add_item(keymap, "MESH_OT_hide_mesh", HKEY, KM_PRESS, KM_SHIFT, 0)->ptr, "swap", 1);
	
	WM_keymap_add_item(keymap, "MESH_OT_reveal_mesh", HKEY, KM_PRESS, KM_ALT, 0);
	
	RNA_int_set(WM_keymap_add_item(keymap, "MESH_OT_righthandfaces", NKEY, KM_PRESS, KM_CTRL|KM_SHIFT, 0)->ptr, "select", 2);
	
	RNA_int_set(WM_keymap_add_item(keymap, "MESH_OT_righthandfaces", NKEY, KM_PRESS, KM_CTRL, 0)->ptr, "select", 1);
	
	RNA_float_set(WM_keymap_add_item(keymap, "MESH_OT_select_linked_flat_faces", FKEY, KM_PRESS, (KM_CTRL|KM_SHIFT|KM_ALT), 0)->ptr,"fsharpness",135.0);
	
	RNA_float_set(WM_keymap_add_item(keymap, "MESH_OT_select_sharp_edges", SKEY, KM_PRESS, (KM_CTRL|KM_SHIFT|KM_ALT), 0)->ptr,"fsharpness",135.0);		
	
	
	

//	RNA_int_set(WM_keymap_add_item(keymap, "OBJECT_OT_viewzoom", PADPLUSKEY, KM_PRESS, 0, 0)->ptr, "delta", 1);
}

