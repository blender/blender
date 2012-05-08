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
#include "DNA_object_force.h"
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
#include "BKE_particle.h"
#include "BKE_scene.h"

#include "RNA_access.h"

#include "UI_interface.h"
#include "UI_resources.h"

#include "ED_node.h"
#include "ED_screen.h"

#include "../interface/interface_intern.h"

#include "buttons_intern.h" // own include

/************************* Texture User **************************/

static void buttons_texture_user_property_add(ListBase *users, ID *id, 
                                              PointerRNA ptr, PropertyRNA *prop,
                                              const char *category, int icon, const char *name)
{
	ButsTextureUser *user = MEM_callocN(sizeof(ButsTextureUser), "ButsTextureUser");

	user->id = id;
	user->ptr = ptr;
	user->prop = prop;
	user->category = category;
	user->icon = icon;
	user->name = name;
	user->index = BLI_countlist(users);

	BLI_addtail(users, user);
}

static void buttons_texture_user_node_add(ListBase *users, ID *id, 
                                          bNodeTree *ntree, bNode *node,
                                          const char *category, int icon, const char *name)
{
	ButsTextureUser *user = MEM_callocN(sizeof(ButsTextureUser), "ButsTextureUser");

	user->id = id;
	user->ntree = ntree;
	user->node = node;
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

	if (ntree) {
		for (node = ntree->nodes.first; node; node = node->next) {
			if (node->typeinfo->nclass == NODE_CLASS_TEXTURE) {
				PointerRNA ptr;
				/* PropertyRNA *prop; */ /* UNUSED */

				RNA_pointer_create(&ntree->id, &RNA_Node, node, &ptr);
				/* prop = RNA_struct_find_property(&ptr, "texture"); */ /* UNUSED */

				buttons_texture_user_node_add(users, id, ntree, node,
				                              category, RNA_struct_ui_icon(ptr.type), node->name);
			}
			else if (node->type == NODE_GROUP && node->id) {
				buttons_texture_users_find_nodetree(users, id, (bNodeTree *)node->id, category);
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

	buttons_texture_user_property_add(users, &ob->id, ptr, prop,
	                                  "Modifiers", RNA_struct_ui_icon(ptr.type), md->name);
}

static void buttons_texture_users_from_context(ListBase *users, const bContext *C, SpaceButs *sbuts)
{
	Scene *scene = NULL;
	Object *ob = NULL;
	Material *ma = NULL;
	Lamp *la = NULL;
	World *wrld = NULL;
	Brush *brush = NULL;
	ID *pinid = sbuts->pinid;

	/* get data from context */
	if (pinid) {
		if (GS(pinid->name) == ID_SCE)
			scene = (Scene *)pinid;
		else if (GS(pinid->name) == ID_OB)
			ob = (Object *)pinid;
		else if (GS(pinid->name) == ID_LA)
			la = (Lamp *)pinid;
		else if (GS(pinid->name) == ID_WO)
			wrld = (World *)pinid;
		else if (GS(pinid->name) == ID_MA)
			ma = (Material *)pinid;
		else if (GS(pinid->name) == ID_BR)
			brush = (Brush *)pinid;
	}

	if (!scene)
		scene = CTX_data_scene(C);
	
	if (!(pinid || pinid == &scene->id)) {
		ob = (scene->basact) ? scene->basact->object : NULL;
		wrld = scene->world;
		brush = paint_brush(paint_get_active(scene));
	}

	if (ob && ob->type == OB_LAMP && !la)
		la = ob->data;
	if (ob && !ma)
		ma = give_current_material(ob, ob->actcol);

	/* fill users */
	users->first = users->last = NULL;

	if (ma)
		buttons_texture_users_find_nodetree(users, &ma->id, ma->nodetree, "Material");
	if (la)
		buttons_texture_users_find_nodetree(users, &la->id, la->nodetree, "Lamp");
	if (wrld)
		buttons_texture_users_find_nodetree(users, &wrld->id, wrld->nodetree, "World");

	if (ob) {
		ParticleSystem *psys = psys_get_current(ob);
		MTex *mtex;
		int a;

		/* modifiers */
		modifiers_foreachTexLink(ob, buttons_texture_modifier_foreach, users);

		/* particle systems */
		if (psys) {
			/* todo: these slots are not in the UI */
			for (a = 0; a < MAX_MTEX; a++) {
				mtex = psys->part->mtex[a];

				if (mtex) {
					PointerRNA ptr;
					PropertyRNA *prop;

					RNA_pointer_create(&psys->part->id, &RNA_ParticleSettingsTextureSlot, mtex, &ptr);
					prop = RNA_struct_find_property(&ptr, "texture");

					buttons_texture_user_property_add(users, &psys->part->id, ptr, prop,
					                                  "Particles", RNA_struct_ui_icon(&RNA_ParticleSettings), psys->name);
				}
			}
		}

		/* field */
		if (ob->pd && ob->pd->forcefield == PFIELD_TEXTURE) {
			PointerRNA ptr;
			PropertyRNA *prop;

			RNA_pointer_create(&ob->id, &RNA_FieldSettings, ob->pd, &ptr);
			prop = RNA_struct_find_property(&ptr, "texture");

			buttons_texture_user_property_add(users, &ob->id, ptr, prop,
			                                  "Fields", ICON_FORCE_TEXTURE, "Texture Field");
		}
	}

	/* brush */
	if (brush) {
		PointerRNA ptr;
		PropertyRNA *prop;

		RNA_pointer_create(&brush->id, &RNA_BrushTextureSlot, &brush->mtex, &ptr);
		prop = RNA_struct_find_property(&ptr, "texture");

		buttons_texture_user_property_add(users, &brush->id, ptr, prop,
		                                  "Brush", ICON_BRUSH_DATA, brush->id.name + 2);
	}
}

void buttons_texture_context_compute(const bContext *C, SpaceButs *sbuts)
{
	/* gatheravailable texture users in context. runs on every draw of
	 * properties editor, before the buttons are created. */
	ButsContextTexture *ct = sbuts->texuser;
	Scene *scene = CTX_data_scene(C);

	if (!BKE_scene_use_new_shading_nodes(scene)) {
		if (ct) {
			BLI_freelistN(&ct->users);
			MEM_freeN(ct);
			sbuts->texuser = NULL;
		}
		
		return;
	}

	if (!ct) {
		ct = MEM_callocN(sizeof(ButsContextTexture), "ButsContextTexture");
		sbuts->texuser = ct;
	}
	else {
		BLI_freelistN(&ct->users);
	}

	buttons_texture_users_from_context(&ct->users, C, sbuts);

	/* set one user as active based on active index */
	if (ct->index >= BLI_countlist(&ct->users))
		ct->index = 0;

	ct->user = BLI_findlink(&ct->users, ct->index);
	ct->texture = NULL;

	if (ct->user) {
		if (ct->user->ptr.data) {
			PointerRNA texptr;
			Tex *tex;

			/* get texture datablock pointer if it's a property */
			texptr = RNA_property_pointer_get(&ct->user->ptr, ct->user->prop);
			tex = (RNA_struct_is_a(texptr.type, &RNA_Texture)) ? texptr.data : NULL;

			ct->texture = tex;
		}
		else if (ct->user->node && !(ct->user->node->flag & NODE_ACTIVE_TEXTURE)) {
			ButsTextureUser *user;

			/* detect change of active texture node in same node tree, in that
			 * case we also automatically switch to the other node */
			for (user = ct->users.first; user; user = user->next) {
				if (user->ntree == ct->user->ntree && user->node != ct->user->node) {
					if (user->node->flag & NODE_ACTIVE_TEXTURE) {
						ct->user = user;
						ct->index = BLI_findindex(&ct->users, user);
						break;
					}
				}
			}
		}
	}
}

static void template_texture_select(bContext *C, void *user_p, void *UNUSED(arg))
{
	/* callback when selecting a texture user in the menu */
	SpaceButs *sbuts = CTX_wm_space_buts(C);
	ButsContextTexture *ct = (sbuts) ? sbuts->texuser : NULL;
	ButsTextureUser *user = (ButsTextureUser *)user_p;
	PointerRNA texptr;
	Tex *tex;

	if (!ct)
		return;

	/* set user as active */
	if (user->node) {
		ED_node_set_active(CTX_data_main(C), user->ntree, user->node);
		ct->texture = NULL;
	}
	else {
		texptr = RNA_property_pointer_get(&user->ptr, user->prop);
		tex = (RNA_struct_is_a(texptr.type, &RNA_Texture)) ? texptr.data : NULL;

		ct->texture = tex;
	}

	ct->user = user;
	ct->index = user->index;
}

static void template_texture_user_menu(bContext *C, uiLayout *layout, void *UNUSED(arg))
{
	/* callback when opening texture user selection menu, to create buttons. */
	SpaceButs *sbuts = CTX_wm_space_buts(C);
	ButsContextTexture *ct = (sbuts) ? sbuts->texuser : NULL;
	ButsTextureUser *user;
	uiBlock *block = uiLayoutGetBlock(layout);
	const char *last_category = NULL;

	for (user = ct->users.first; user; user = user->next) {
		uiBut *but;
		char name[UI_MAX_NAME_STR];

		/* add label per category */
		if (!last_category || strcmp(last_category, user->category) != 0) {
			uiItemL(layout, user->category, ICON_NONE);
			but = block->buttons.last;
			but->flag = UI_TEXT_LEFT;
		}

		/* create button */
		BLI_snprintf(name, UI_MAX_NAME_STR, "  %s", user->name);

		but = uiDefIconTextBut(block, BUT, 0, user->icon, name, 0, 0, UI_UNIT_X * 4, UI_UNIT_Y,
		                       NULL, 0.0, 0.0, 0.0, 0.0, "");
		uiButSetNFunc(but, template_texture_select, MEM_dupallocN(user), NULL);

		last_category = user->category;
	}
}

void uiTemplateTextureUser(uiLayout *layout, bContext *C)
{
	/* texture user selection dropdown menu. the available users have been
	 * gathered before drawing in ButsContextTexture, we merely need to
	 * display the current item. */
	SpaceButs *sbuts = CTX_wm_space_buts(C);
	ButsContextTexture *ct = (sbuts) ? sbuts->texuser : NULL;
	uiBlock *block = uiLayoutGetBlock(layout);
	uiBut *but;
	ButsTextureUser *user;
	char name[UI_MAX_NAME_STR];

	if (!ct)
		return;

	/* get current user */
	user = ct->user;

	if (!user) {
		uiItemL(layout, "No textures in context.", ICON_NONE);
		return;
	}

	/* create button */
	BLI_snprintf(name, UI_MAX_NAME_STR, "%s", user->name);

	if (user->icon) {
		but = uiDefIconTextMenuBut(block, template_texture_user_menu, NULL,
		                           user->icon, name, 0, 0, UI_UNIT_X * 4, UI_UNIT_Y, "");
	}
	else {
		but = uiDefMenuBut(block, template_texture_user_menu, NULL,
		                   name, 0, 0, UI_UNIT_X * 4, UI_UNIT_Y, "");
	}

	/* some cosmetic tweaks */
	but->type = MENU;
	but->flag |= UI_TEXT_LEFT;
	but->flag &= ~UI_ICON_SUBMENU;
}

/************************* Texture Show **************************/

static void template_texture_show(bContext *C, void *data_p, void *prop_p)
{
	SpaceButs *sbuts = CTX_wm_space_buts(C);
	ButsContextTexture *ct = (sbuts) ? sbuts->texuser : NULL;
	ButsTextureUser *user;

	if (!ct)
		return;

	for (user = ct->users.first; user; user = user->next)
		if (user->ptr.data == data_p && user->prop == prop_p)
			break;
	
	if (user) {
		/* select texture */
		template_texture_select(C, user, NULL);

		/* change context */
		sbuts->mainb = BCONTEXT_TEXTURE;
		sbuts->mainbuser = sbuts->mainb;
		sbuts->preview = 1;

		/* redraw editor */
		ED_area_tag_redraw(CTX_wm_area(C));
	}
}

void uiTemplateTextureShow(uiLayout *layout, bContext *C, PointerRNA *ptr, PropertyRNA *prop)
{
	/* button to quickly show texture in texture tab */
	SpaceButs *sbuts = CTX_wm_space_buts(C);
	ButsContextTexture *ct = (sbuts) ? sbuts->texuser : NULL;
	ButsTextureUser *user;

	/* only show button in other tabs in properties editor */
	if (!ct || sbuts->mainb == BCONTEXT_TEXTURE)
		return;

	/* find corresponding texture user */
	for (user = ct->users.first; user; user = user->next)
		if (user->ptr.data == ptr->data && user->prop == prop)
			break;
	
	/* draw button */
	if (user) {
		uiBlock *block = uiLayoutGetBlock(layout);
		uiBut *but;
		
		but = uiDefIconBut(block, BUT, 0, ICON_BUTS, 0, 0, UI_UNIT_X, UI_UNIT_Y,
		                   NULL, 0.0, 0.0, 0.0, 0.0, "Show texture in texture tab");
		uiButSetFunc(but, template_texture_show, user->ptr.data, user->prop);
	}
}

