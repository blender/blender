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
 * The Original Code is Copyright (C) 2008 Blender Foundation.
 * All rights reserved.
 *
 *
 * ***** END GPL LICENSE BLOCK *****
 */

#include <stdlib.h>
#include <string.h>

#include "DNA_object_types.h"
#include "DNA_scene_types.h"
#include "DNA_screen_types.h"

#include "BKE_context.h"
#include "BKE_utildefines.h"
#include "BKE_global.h"

#include "RNA_access.h"

#include "ED_object.h"

int ed_screen_context(const bContext *C, const char *member, bContextDataResult *result)
{
	bScreen *sc= CTX_wm_screen(C);
	Scene *scene= sc->scene;
	Base *base;
	Object *ob = NULL;

	if(scene && scene->basact)
		ob = scene->basact->object;

	if(CTX_data_dir(member)) {
		static const char *dir[] = {
			"scene", "selected_objects", "selected_bases",
			"selected_editable_objects", "selected_editable_bases",
			"active_base", "active_object", "edit_object",
			"sculpt_object", "vertex_paint_object", "weight_paint_object",
			"texture_paint_object", "brush", "particle_edit_object", NULL};

		CTX_data_dir_set(result, dir);
		return 1;
	}
	else if(CTX_data_equals(member, "scene")) {
		CTX_data_id_pointer_set(result, &scene->id);
		return 1;
	}
	else if(CTX_data_equals(member, "selected_objects") || CTX_data_equals(member, "selected_bases")) {
		int selected_objects= CTX_data_equals(member, "selected_objects");

		for(base=scene->base.first; base; base=base->next) {
			if((base->flag & SELECT) && (base->lay & scene->lay)) {
				if(selected_objects)
					CTX_data_id_list_add(result, &base->object->id);
				else
					CTX_data_list_add(result, &scene->id, &RNA_UnknownType, base);
			}
		}

		return 1;
	}
	else if(CTX_data_equals(member, "selected_editable_objects") || CTX_data_equals(member, "selected_editable_bases")) {
		int selected_editable_objects= CTX_data_equals(member, "selected_editable_objects");

		for(base=scene->base.first; base; base=base->next) {
			if((base->flag & SELECT) && (base->lay & scene->lay)) {
				if((base->object->restrictflag & OB_RESTRICT_VIEW)==0) {
					if(0==object_is_libdata(base->object)) {
						if(selected_editable_objects)
							CTX_data_id_list_add(result, &base->object->id);
						else
							CTX_data_list_add(result, &scene->id, &RNA_UnknownType, base);
					}
				}
			}
		}

		return 1;
	}
	else if(CTX_data_equals(member, "active_base")) {
		if(scene->basact)
			CTX_data_pointer_set(result, &scene->id, &RNA_UnknownType, scene->basact);

		return 1;
	}
	else if(CTX_data_equals(member, "active_object")) {
		if(scene->basact)
			CTX_data_id_pointer_set(result, &scene->basact->object->id);

		return 1;
	}
	else if(CTX_data_equals(member, "object")) {
		if(scene->basact)
			CTX_data_id_pointer_set(result, &scene->basact->object->id);

		return 1;
	}
	else if(CTX_data_equals(member, "edit_object")) {
		/* convenience for now, 1 object per scene in editmode */
		if(scene->obedit)
			CTX_data_id_pointer_set(result, &scene->obedit->id);
		
		return 1;
	}
	else if(CTX_data_equals(member, "sculpt_object")) {
		if(ob && (ob->mode & OB_MODE_SCULPT))
			CTX_data_id_pointer_set(result, &ob->id);

		return 1;
	}
	else if(CTX_data_equals(member, "vertex_paint_object")) {
		if(ob && (ob->mode & OB_MODE_VERTEX_PAINT))
			CTX_data_id_pointer_set(result, &ob->id);

		return 1;
	}
	else if(CTX_data_equals(member, "weight_paint_object")) {
		if(ob && (ob->mode & OB_MODE_WEIGHT_PAINT))
			CTX_data_id_pointer_set(result, &ob->id);

		return 1;
	}
	else if(CTX_data_equals(member, "texture_paint_object")) {
		if(ob && (ob->mode & OB_MODE_TEXTURE_PAINT))
			CTX_data_id_pointer_set(result, &ob->id);

		return 1;
	}
	else if(CTX_data_equals(member, "particle_edit_object")) {
		if(ob && (ob->mode & OB_MODE_PARTICLE_EDIT))
			CTX_data_id_pointer_set(result, &ob->id);

		return 1;
	}

	return 0;
}

