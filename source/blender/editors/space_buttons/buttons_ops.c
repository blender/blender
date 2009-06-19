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
 * Contributor(s): Blender Foundation
 *
 * ***** END GPL LICENSE BLOCK *****
 */

#include <stdlib.h>
#include <string.h>

#include "MEM_guardedalloc.h"

#include "DNA_object_types.h"
#include "DNA_material_types.h"
#include "DNA_texture_types.h"
#include "DNA_scene_types.h"
#include "DNA_world_types.h"

#include "BKE_context.h"
#include "BKE_library.h"
#include "BKE_material.h"
#include "BKE_texture.h"
#include "BKE_world.h"

#include "RNA_access.h"

#include "WM_api.h"
#include "WM_types.h"

#include "buttons_intern.h"	// own include

/********************** new material operator *********************/

static int new_material_exec(bContext *C, wmOperator *op)
{
	PointerRNA ptr;
	Material *ma;
	Object *ob;
	int index;

	/* add or copy material */
	ptr= CTX_data_pointer_get(C, "material");
	ma= (RNA_struct_is_a(ptr.type, &RNA_Material))? ptr.data: NULL;

	if(ma)
		ma= copy_material(ma);
	else
		ma= add_material("Material");

	ma->id.us--; /* compensating for us++ in assign_material */

	/* attempt to assign to material slot */
	ptr= CTX_data_pointer_get(C, "material_slot");

	if(RNA_struct_is_a(ptr.type, &RNA_MaterialSlot)) {
		ob= ptr.id.data;
		index= (Material**)ptr.data - ob->mat;

		assign_material(ob, ma, index+1);

		WM_event_add_notifier(C, NC_OBJECT|ND_DRAW, ob);
	}
	
	return OPERATOR_FINISHED;
}

void MATERIAL_OT_new(wmOperatorType *ot)
{
	/* identifiers */
	ot->name= "New Material";
	ot->idname= "MATERIAL_OT_new";
	
	/* api callbacks */
	ot->exec= new_material_exec;

	/* flags */
	ot->flag= OPTYPE_REGISTER|OPTYPE_UNDO;
}

/********************** new texture operator *********************/

static int new_texture_exec(bContext *C, wmOperator *op)
{
	PointerRNA ptr;
	ID *id;
	Tex *tex;
	MTex *mtex;

	/* add or copy texture */
	ptr= CTX_data_pointer_get(C, "texture");
	tex= (RNA_struct_is_a(ptr.type, &RNA_Texture))? ptr.data: NULL;

	if(tex)
		tex= copy_texture(tex);
	else
		tex= add_texture("Texture");

	id_us_min(&tex->id);

	/* attempt to assign to texture slot */
	ptr= CTX_data_pointer_get(C, "texture_slot");

	if(RNA_struct_is_a(ptr.type, &RNA_TextureSlot)) {
		id= ptr.id.data;
		mtex= ptr.data;

		if(mtex) {
			if(mtex->tex)
				id_us_min(&mtex->tex->id);
			mtex->tex= tex;
			id_us_plus(&tex->id);
		}

		/* XXX nodes, notifier .. */
	}
	
	return OPERATOR_FINISHED;
}

void TEXTURE_OT_new(wmOperatorType *ot)
{
	/* identifiers */
	ot->name= "New Texture";
	ot->idname= "TEXTURE_OT_new";
	
	/* api callbacks */
	ot->exec= new_texture_exec;

	/* flags */
	ot->flag= OPTYPE_REGISTER|OPTYPE_UNDO;
}

/********************** new world operator *********************/

static int new_world_exec(bContext *C, wmOperator *op)
{
	PointerRNA ptr;
	Scene *scene;
	World *wo;

	/* add or copy world */
	ptr= CTX_data_pointer_get(C, "world");
	wo= (RNA_struct_is_a(ptr.type, &RNA_World))? ptr.data: NULL;

	if(wo)
		wo= copy_world(wo);
	else
		wo= add_world("World");

	/* assign to scene */
	scene= CTX_data_scene(C);

	if(scene->world)
		id_us_min(&scene->world->id);
	scene->world= wo;

	// XXX notifier
	
	return OPERATOR_FINISHED;
}

void WORLD_OT_new(wmOperatorType *ot)
{
	/* identifiers */
	ot->name= "New World";
	ot->idname= "WORLD_OT_new";
	
	/* api callbacks */
	ot->exec= new_world_exec;

	/* flags */
	ot->flag= OPTYPE_REGISTER|OPTYPE_UNDO;
}

