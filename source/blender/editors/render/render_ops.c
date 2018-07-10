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
 * The Original Code is Copyright (C) 2009 Blender Foundation.
 * All rights reserved.
 *
 * Contributor(s): Blender Foundation
 *
 * ***** END GPL LICENSE BLOCK *****
 */

/** \file blender/editors/render/render_ops.c
 *  \ingroup edrend
 */

#include <stdlib.h>

#include "BLI_utildefines.h"

#include "ED_render.h"

#include "WM_api.h"

#include "render_intern.h" // own include

/***************************** render ***********************************/

void ED_operatortypes_render(void)
{
	WM_operatortype_append(OBJECT_OT_material_slot_add);
	WM_operatortype_append(OBJECT_OT_material_slot_remove);
	WM_operatortype_append(OBJECT_OT_material_slot_assign);
	WM_operatortype_append(OBJECT_OT_material_slot_select);
	WM_operatortype_append(OBJECT_OT_material_slot_deselect);
	WM_operatortype_append(OBJECT_OT_material_slot_copy);
	WM_operatortype_append(OBJECT_OT_material_slot_move);

	WM_operatortype_append(MATERIAL_OT_new);
	WM_operatortype_append(TEXTURE_OT_new);
	WM_operatortype_append(WORLD_OT_new);

	WM_operatortype_append(MATERIAL_OT_copy);
	WM_operatortype_append(MATERIAL_OT_paste);

	WM_operatortype_append(SCENE_OT_view_layer_add);
	WM_operatortype_append(SCENE_OT_view_layer_remove);

	WM_operatortype_append(SCENE_OT_render_view_add);
	WM_operatortype_append(SCENE_OT_render_view_remove);

	WM_operatortype_append(SCENE_OT_light_cache_bake);
	WM_operatortype_append(SCENE_OT_light_cache_free);

#ifdef WITH_FREESTYLE
	WM_operatortype_append(SCENE_OT_freestyle_module_add);
	WM_operatortype_append(SCENE_OT_freestyle_module_remove);
	WM_operatortype_append(SCENE_OT_freestyle_module_move);
	WM_operatortype_append(SCENE_OT_freestyle_lineset_add);
	WM_operatortype_append(SCENE_OT_freestyle_lineset_copy);
	WM_operatortype_append(SCENE_OT_freestyle_lineset_paste);
	WM_operatortype_append(SCENE_OT_freestyle_lineset_remove);
	WM_operatortype_append(SCENE_OT_freestyle_lineset_move);
	WM_operatortype_append(SCENE_OT_freestyle_linestyle_new);
	WM_operatortype_append(SCENE_OT_freestyle_color_modifier_add);
	WM_operatortype_append(SCENE_OT_freestyle_alpha_modifier_add);
	WM_operatortype_append(SCENE_OT_freestyle_thickness_modifier_add);
	WM_operatortype_append(SCENE_OT_freestyle_geometry_modifier_add);
	WM_operatortype_append(SCENE_OT_freestyle_modifier_remove);
	WM_operatortype_append(SCENE_OT_freestyle_modifier_move);
	WM_operatortype_append(SCENE_OT_freestyle_modifier_copy);
	WM_operatortype_append(SCENE_OT_freestyle_stroke_material_create);
#endif

	WM_operatortype_append(TEXTURE_OT_slot_copy);
	WM_operatortype_append(TEXTURE_OT_slot_paste);
	WM_operatortype_append(TEXTURE_OT_slot_move);

	/* render_internal.c */
	WM_operatortype_append(RENDER_OT_view_show);
	WM_operatortype_append(RENDER_OT_render);
	WM_operatortype_append(RENDER_OT_view_cancel);
	WM_operatortype_append(RENDER_OT_shutter_curve_preset);

	/* render_opengl.c */
	WM_operatortype_append(RENDER_OT_opengl);
}
