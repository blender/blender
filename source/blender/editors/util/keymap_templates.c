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
 * ***** END GPL LICENSE BLOCK *****
 */

/** \file blender/editors/util/keymap_templates.c
 *  \ingroup edutil
 */

#include "WM_api.h"
#include "WM_types.h"

#include "RNA_access.h"

#include "ED_select_utils.h"
#include "ED_keymap_templates.h"  /* own include */

void ED_keymap_template_select_all(wmKeyMap *keymap, const char *idname)
{
	wmKeyMapItem *kmi;

	kmi = WM_keymap_add_item(keymap, idname, AKEY, KM_PRESS, 0, 0);
	RNA_enum_set(kmi->ptr, "action", SEL_SELECT);
	kmi = WM_keymap_add_item(keymap, idname, AKEY, KM_PRESS, KM_ALT, 0);
	RNA_enum_set(kmi->ptr, "action", SEL_DESELECT);
	kmi = WM_keymap_add_item(keymap, idname, IKEY, KM_PRESS, KM_CTRL, 0);
	RNA_enum_set(kmi->ptr, "action", SEL_INVERT);

	/* Double tap to de-select (experimental, see: D3640). */
	kmi = WM_keymap_add_item(keymap, idname, AKEY, KM_DBL_CLICK, 0, 0);
	RNA_enum_set(kmi->ptr, "action", SEL_DESELECT);
}
