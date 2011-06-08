/*
 * $Id$
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
 * Inc., 51 Franklin Street, Fifth Floor, Boston, MA 02110-1301, USA.
 *
 * The Original Code is Copyright (C) 2009 Blender Foundation.
 * All rights reserved.
 *
 * Contributor(s): Blender Foundation
 *
 * ***** END GPL LICENSE BLOCK *****
 */

/** \file blender/editors/space_buttons/buttons_texture.c
 *  \ingroup spbuttons
 */


#include <stdlib.h>
#include <string.h>

#include "MEM_guardedalloc.h"

#include "BLI_listbase.h"
#include "BLI_string.h"
#include "BLI_utildefines.h"

#include "DNA_brush_types.h"
#include "DNA_ID.h"
#include "DNA_lamp_types.h"
#include "DNA_material_types.h"
#include "DNA_node_types.h"
#include "DNA_object_types.h"
#include "DNA_particle_types.h"
#include "DNA_scene_types.h"
#include "DNA_screen_types.h"
#include "DNA_space_types.h"
#include "DNA_world_types.h"

#include "BKE_context.h"
#include "BKE_material.h"
#include "BKE_modifier.h"
#include "BKE_node.h"
#include "BKE_paint.h"

#include "RNA_access.h"

#include "UI_interface.h"
#include "UI_resources.h"

#include "../interface/interface_intern.h"

#include "buttons_intern.h"	// own include

/************************* Texture User **************************/

static void buttons_texture_user_add(ListBase *users, ID *id, 
	PointerRNA ptr, PropertyRNA *prop,
	const char *category, int icon, const char *name)
{
	ButsTextureUser *user = MEM_callocN(sizeof(ButsTextureUser), "ButsTextureUser");

	user->id= id;
	user->ptr = ptr;
	user->prop = prop;
	user->category = category;
	user->icon = icon;
	user->name = name;
	user->index = BLI_countlist(users);

	BLI_addtail(users, user);
}

static void buttons_texture_users_find_nodetree(ListBase *users, ID *id,
	bNodeTree *ntree, const char *category)
{
	bNode *node;

	if(ntree) {
		for(node=ntree->nodes.first; node; node=node->next) {
			if(node->type == SH_NODE_TEXTURE) {
				PointerRNA ptr;
				PropertyRNA *prop;

				RNA_pointer_create(&ntree->id, &RNA_Node, node, &ptr);
				prop = RNA_struct_find_property(&ptr, "texture");

				buttons_texture_user_add(users, id, ptr, prop, category, ICON_NODE, node->name);
			}
			else if(node->type == NODE_GROUP && node->id) {
				buttons_texture_users_find_nodetree(users, id, (bNodeTree*)node->id, category);
			}
		}
	}
}

static void buttons_texture_modifier_foreach(void *userData, Object *ob, ModifierData *md, const char *propname)
{
	PointerRNA ptr;
	PropertyRNA *prop;
	ListBase *users = userData;

	RNA_pointer_create(&ob->id, &RNA_Modifier, md, &ptr);
	prop = RNA_struct_find_property(&ptr, propname);

	buttons_texture_user_add(users, &ob->id, ptr, prop, "Modifiers", ICON_MODIFIER, md->name);
}

static void buttons_texture_users_from_context(ListBase *users, const bContext *C, SpaceButs *sbuts)
{
	Scene *scene= NULL;
	Object *ob= NULL;
	Material *ma= NULL;
	Lamp *la= NULL;
	World *wrld= NULL;
	Brush *brush= NULL;
	ID *pinid = sbuts->pinid;

	/* get data from context */
	if(pinid) {
		if(GS(pinid->name) == ID_SCE)
			scene= (Scene*)pinid;
		else if(GS(pinid->name) == ID_OB)
			ob= (Object*)pinid;
		else if(GS(pinid->name) == ID_LA)
			la= (Lamp*)pinid;
		else if(GS(pinid->name) == ID_WO)
			wrld= (World*)pinid;
		else if(GS(pinid->name) == ID_MA)
			ma= (Material*)pinid;
		else if(GS(pinid->name) == ID_BR)
			brush= (Brush*)pinid;
	}

	if(!scene)
		scene= CTX_data_scene(C);
	
	if(!(pinid || pinid == &scene->id)) {
		ob= (scene->basact)? scene->basact->object: NULL;
		wrld= scene->world;
		brush= paint_brush(paint_get_active(scene));
	}

	if(ob && ob->type == OB_LAMP && !la)
		la= ob->data;
	if(ob && !ma)
		ma= give_current_material(ob, ob->actcol);

	/* fill users */
	users->first = users->last = NULL;

	if(ma)
		buttons_texture_users_find_nodetree(users, &ma->id, ma->nodetree, "Material");
	if(la)
		buttons_texture_users_find_nodetree(users, &la->id, la->nodetree, "Lamp");
	if(wrld)
		buttons_texture_users_find_nodetree(users, &wrld->id, wrld->nodetree, "World");

	if(ob) {
		ParticleSystem *psys;
		MTex *mtex;
		int a;

		/* modifiers */
		modifiers_foreachTexLink(ob, buttons_texture_modifier_foreach, users);

		/* particle systems */
		/* todo: these slots are not in the UI */
		for(psys=ob->particlesystem.first; psys; psys=psys->next) {
			for(a=0; a<MAX_MTEX; a++) {
				mtex = psys->part->mtex[a];

				if(mtex) {
					PointerRNA ptr;
					PropertyRNA *prop;

					RNA_pointer_create(&psys->part->id, &RNA_ParticleSettingsTextureSlot, mtex, &ptr);
					prop = RNA_struct_find_property(&ptr, "texture");

					buttons_texture_user_add(users, &psys->part->id, ptr, prop, "Particles", ICON_PARTICLES, psys->name);
				}
			}
		}
	}

	/* brush */
	if(brush) {
		PointerRNA ptr;
		PropertyRNA *prop;

		RNA_pointer_create(&brush->id, &RNA_BrushTextureSlot, &brush->mtex, &ptr);
		prop = RNA_struct_find_property(&ptr, "texture");

		buttons_texture_user_add(users, &brush->id, ptr, prop, "Brush", ICON_BRUSH_DATA, brush->id.name+2);
	}
}

void buttons_texture_context_compute(const bContext *C, SpaceButs *sbuts)
{
	/* gatheravailable texture users in context. runs on every draw of
	   properties editor, before the buttons are created. */
	ButsContextTexture *ct= sbuts->texuser;

	if(!ct) {
		ct= MEM_callocN(sizeof(ButsContextTexture), "ButsContextTexture");
		sbuts->texuser= ct;
	}
	else {
		BLI_freelistN(&ct->users);
	}

	buttons_texture_users_from_context(&ct->users, C, sbuts);

	/* set one user as active based on active index */
	if(ct->index >= BLI_countlist(&ct->users))
		ct->index= 0;

	ct->user = BLI_findlink(&ct->users, ct->index);

	if(ct->user) {
		PointerRNA texptr;
		Tex *tex;

		texptr = RNA_property_pointer_get(&ct->user->ptr, ct->user->prop);
		tex = (RNA_struct_is_a(texptr.type, &RNA_Texture))? texptr.data: NULL;

		ct->texture = tex;
	}
}

static void template_texture_select(bContext *C, void *user_p, void *UNUSED(arg))
{
	/* callback when selecting a texture user in the menu */
	SpaceButs *sbuts = CTX_wm_space_buts(C);
	ButsContextTexture *ct= (sbuts)? sbuts->texuser: NULL;
	ButsTextureUser *user = (ButsTextureUser*)user_p;
	PointerRNA texptr;
	Tex *tex;

	if(!ct)
		return;

	/* set user as active */
	texptr = RNA_property_pointer_get(&user->ptr, user->prop);
	tex = (RNA_struct_is_a(texptr.type, &RNA_Texture))? texptr.data: NULL;

	ct->texture = tex;
	ct->user = user;
	ct->index = user->index;
}

static void template_texture_user_menu(bContext *C, uiLayout *layout, void *UNUSED(arg))
{
	/* callback when opening texture user selection menu, to create buttons. */
	SpaceButs *sbuts = CTX_wm_space_buts(C);
	ButsContextTexture *ct= (sbuts)? sbuts->texuser: NULL;
	ButsTextureUser *user;
	uiBlock *block = uiLayoutGetBlock(layout);
	const char *last_category = NULL;

	for(user=ct->users.first; user; user=user->next) {
		uiBut *but;
		char name[UI_MAX_NAME_STR];

		/* add label per category */
		if(!last_category || strcmp(last_category, user->category) != 0) {
			uiItemL(layout, user->category, ICON_NONE);
			but= block->buttons.last;
			but->flag= UI_TEXT_LEFT;
		}

		/* create button */
		BLI_snprintf(name, UI_MAX_NAME_STR, "  %s", user->name);

		but = uiDefIconTextBut(block, BUT, 0, user->icon, name, 0, 0, UI_UNIT_X*4, UI_UNIT_Y,
			NULL, 0.0, 0.0, 0.0, 0.0, "");
		uiButSetNFunc(but, template_texture_select, MEM_dupallocN(user), NULL);

		last_category = user->category;
	}
}

void uiTemplateTextureUser(uiLayout *layout, bContext *C)
{
	/* texture user selection dropdown menu. the available users have been
	   gathered before drawing in ButsContextTexture, we merely need to
	   display the current item. */
	SpaceButs *sbuts = CTX_wm_space_buts(C);
	ButsContextTexture *ct= (sbuts)? sbuts->texuser: NULL;
	uiBlock *block = uiLayoutGetBlock(layout);
	uiBut *but;
	ButsTextureUser *user;
	char name[UI_MAX_NAME_STR];

	if(!ct)
		return;

	/* get current user */
	user= ct->user;

	if(!user) {
		uiItemL(layout, "No textures in context.", ICON_NONE);
		return;
	}

	/* create button */
	BLI_snprintf(name, UI_MAX_NAME_STR, "%s", user->name);

	if(user->icon) {
		but= uiDefIconTextMenuBut(block, template_texture_user_menu, NULL,
			user->icon, name, 0, 0, UI_UNIT_X*4, UI_UNIT_Y, "");
	}
	else {
		but= uiDefMenuBut(block, template_texture_user_menu, NULL,
			name, 0, 0, UI_UNIT_X*4, UI_UNIT_Y, "");
	}

	/* some cosmetic tweaks */
	but->type= MENU;
	but->flag |= UI_TEXT_LEFT;
	but->flag &= ~UI_ICON_SUBMENU;
}

