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
 * The Original Code is Copyright (C) 2008 Blender Foundation.
 * All rights reserved.
 *
 * 
 * Contributor(s): Blender Foundation
 *
 * ***** END GPL LICENSE BLOCK *****
 */

/** \file blender/editors/space_node/space_node.c
 *  \ingroup spnode
 */


#include <string.h>
#include <stdio.h>

#include "DNA_lamp_types.h"
#include "DNA_material_types.h"
#include "DNA_node_types.h"
#include "DNA_object_types.h"
#include "DNA_scene_types.h"
#include "DNA_world_types.h"

#include "MEM_guardedalloc.h"

#include "BLI_blenlib.h"
#include "BLI_math.h"
#include "BLI_rand.h"
#include "BLI_utildefines.h"

#include "BKE_context.h"
#include "BKE_screen.h"
#include "BKE_node.h"

#include "ED_space_api.h"
#include "ED_render.h"
#include "ED_screen.h"


#include "WM_api.h"
#include "WM_types.h"

#include "UI_resources.h"
#include "UI_view2d.h"

#include "RNA_access.h"

#include "node_intern.h"	// own include

/* ******************** manage regions ********************* */

ARegion *node_has_buttons_region(ScrArea *sa)
{
	ARegion *ar, *arnew;

	ar= BKE_area_find_region_type(sa, RGN_TYPE_UI);
	if(ar) return ar;
	
	/* add subdiv level; after header */
	ar= BKE_area_find_region_type(sa, RGN_TYPE_HEADER);

	/* is error! */
	if(ar==NULL) return NULL;
	
	arnew= MEM_callocN(sizeof(ARegion), "buttons for node");
	
	BLI_insertlinkafter(&sa->regionbase, ar, arnew);
	arnew->regiontype= RGN_TYPE_UI;
	arnew->alignment= RGN_ALIGN_RIGHT;
	
	arnew->flag = RGN_FLAG_HIDDEN;
	
	return arnew;
}

/* ******************** default callbacks for node space ***************** */

static SpaceLink *node_new(const bContext *UNUSED(C))
{
	ARegion *ar;
	SpaceNode *snode;
	
	snode= MEM_callocN(sizeof(SpaceNode), "initnode");
	snode->spacetype= SPACE_NODE;	
	
	/* backdrop */
	snode->zoom = 1.0f;
	
	/* header */
	ar= MEM_callocN(sizeof(ARegion), "header for node");
	
	BLI_addtail(&snode->regionbase, ar);
	ar->regiontype= RGN_TYPE_HEADER;
	ar->alignment= RGN_ALIGN_BOTTOM;
	
	/* buttons/list view */
	ar= MEM_callocN(sizeof(ARegion), "buttons for node");
	
	BLI_addtail(&snode->regionbase, ar);
	ar->regiontype= RGN_TYPE_UI;
	ar->alignment= RGN_ALIGN_RIGHT;
	ar->flag = RGN_FLAG_HIDDEN;
	
	/* main area */
	ar= MEM_callocN(sizeof(ARegion), "main area for node");
	
	BLI_addtail(&snode->regionbase, ar);
	ar->regiontype= RGN_TYPE_WINDOW;
	
	ar->v2d.tot.xmin=  -256.0f;
	ar->v2d.tot.ymin=  -256.0f;
	ar->v2d.tot.xmax= 768.0f;
	ar->v2d.tot.ymax= 768.0f;
	
	ar->v2d.cur.xmin=  -256.0f;
	ar->v2d.cur.ymin=  -256.0f;
	ar->v2d.cur.xmax= 768.0f;
	ar->v2d.cur.ymax= 768.0f;
	
	ar->v2d.min[0]= 1.0f;
	ar->v2d.min[1]= 1.0f;
	
	ar->v2d.max[0]= 32000.0f;
	ar->v2d.max[1]= 32000.0f;
	
	ar->v2d.minzoom= 0.09f;
	ar->v2d.maxzoom= 2.31f;
	
	ar->v2d.scroll= (V2D_SCROLL_RIGHT|V2D_SCROLL_BOTTOM);
	ar->v2d.keepzoom= V2D_LIMITZOOM|V2D_KEEPASPECT;
	ar->v2d.keeptot= 0;
	
	return (SpaceLink *)snode;
}

/* not spacelink itself */
static void node_free(SpaceLink *UNUSED(sl))
{	
	
}


/* spacetype; init callback */
static void node_init(struct wmWindowManager *UNUSED(wm), ScrArea *UNUSED(sa))
{

}

static void node_area_listener(ScrArea *sa, wmNotifier *wmn)
{
	/* note, ED_area_tag_refresh will re-execute compositor */
	SpaceNode *snode= sa->spacedata.first;
	int type= snode->treetype;
	short shader_type = snode->shaderfrom;
	
	/* preview renders */
	switch(wmn->category) {
		case NC_SCENE:
			switch (wmn->data) {
				case ND_NODES:
				case ND_FRAME:
					ED_area_tag_refresh(sa);
					break;
				case ND_COMPO_RESULT:
					ED_area_tag_redraw(sa);
					break;
				case ND_TRANSFORM_DONE:
					if(type==NTREE_COMPOSIT) {
						if(snode->flag & SNODE_AUTO_RENDER) {
							snode->recalc= 1;
							ED_area_tag_refresh(sa);
						}
					}
					break;
			}
			break;
		case NC_WM:
			if(wmn->data==ND_FILEREAD)
				ED_area_tag_refresh(sa);
			break;
		
		/* future: add ID checks? */
		case NC_MATERIAL:
			if(type==NTREE_SHADER) {
				if(wmn->data==ND_SHADING)
					ED_area_tag_refresh(sa);
				else if(wmn->data==ND_SHADING_DRAW)
					ED_area_tag_refresh(sa);
				else if(wmn->action==NA_ADDED && snode->edittree)
					nodeSetActiveID(snode->edittree, ID_MA, wmn->reference);
					
			}
			break;
		case NC_TEXTURE:
			if(type==NTREE_SHADER || type==NTREE_TEXTURE) {
				if(wmn->data==ND_NODES)
					ED_area_tag_refresh(sa);
			}
			break;
		case NC_WORLD:
			if(type==NTREE_SHADER && shader_type==SNODE_SHADER_WORLD) {
				ED_area_tag_refresh(sa);	
			}
			break;
		case NC_OBJECT:
			if(type==NTREE_SHADER) {
				if(wmn->data==ND_OB_SHADING)
					ED_area_tag_refresh(sa);
			}
			break;
		case NC_TEXT:
			/* pynodes */
			if(wmn->data==ND_SHADING)
				ED_area_tag_refresh(sa);
			break;
		case NC_SPACE:
			if(wmn->data==ND_SPACE_NODE)
				ED_area_tag_refresh(sa);
			else if(wmn->data==ND_SPACE_NODE_VIEW)
				ED_area_tag_redraw(sa);
			break;
		case NC_NODE:
			if (wmn->action == NA_EDITED)
				ED_area_tag_refresh(sa);
			else if (wmn->action == NA_SELECTED)
				ED_area_tag_redraw(sa);
			break;
		case NC_SCREEN:
			switch(wmn->data) {
				case ND_ANIMPLAY:
					ED_area_tag_refresh(sa);
					break;
			}
			break;

		case NC_IMAGE:
			if (wmn->action == NA_EDITED) {
				if(type==NTREE_COMPOSIT) {
					/* note that nodeUpdateID is already called by BKE_image_signal() on all
					 * scenes so really this is just to know if the images is used in the compo else
					 * painting on images could become very slow when the compositor is open. */
					if(nodeUpdateID(snode->nodetree, wmn->reference))
						ED_area_tag_refresh(sa);
				}
			}
			break;
	}
}

static void node_area_refresh(const struct bContext *C, struct ScrArea *sa)
{
	/* default now: refresh node is starting preview */
	SpaceNode *snode= sa->spacedata.first;

	snode_set_context(snode, CTX_data_scene(C));
	
	if(snode->nodetree) {
		if(snode->treetype==NTREE_SHADER) {
			if(GS(snode->id->name) == ID_MA) {
				Material *ma= (Material *)snode->id;
				if(ma->use_nodes)
					ED_preview_shader_job(C, sa, snode->id, NULL, NULL, 100, 100, PR_NODE_RENDER);
			}
			else if(GS(snode->id->name) == ID_LA) {
				Lamp *la= (Lamp *)snode->id;
				if(la->use_nodes)
					ED_preview_shader_job(C, sa, snode->id, NULL, NULL, 100, 100, PR_NODE_RENDER);
			}
			else if(GS(snode->id->name) == ID_WO) {
				World *wo= (World *)snode->id;
				if(wo->use_nodes)
					ED_preview_shader_job(C, sa, snode->id, NULL, NULL, 100, 100, PR_NODE_RENDER);
			}
		}
		else if(snode->treetype==NTREE_COMPOSIT) {
			Scene *scene= (Scene *)snode->id;
			if(scene->use_nodes) {
				/* recalc is set on 3d view changes for auto compo */
				if(snode->recalc) {
					snode->recalc= 0;
					node_render_changed_exec((struct bContext*)C, NULL);
				}
				else 
					snode_composite_job(C, sa);
			}
		}
		else if(snode->treetype==NTREE_TEXTURE) {
			Tex *tex= (Tex *)snode->id;
			if(tex->use_nodes) {
				ED_preview_shader_job(C, sa, snode->id, NULL, NULL, 100, 100, PR_NODE_RENDER);
			}
		}
	}
}

static SpaceLink *node_duplicate(SpaceLink *sl)
{
	SpaceNode *snoden= MEM_dupallocN(sl);
	
	/* clear or remove stuff from old */
	snoden->nodetree= NULL;
	snoden->linkdrag.first= snoden->linkdrag.last= NULL;
	
	return (SpaceLink *)snoden;
}


/* add handlers, stuff you only do once or on area/region changes */
static void node_buttons_area_init(wmWindowManager *wm, ARegion *ar)
{
	wmKeyMap *keymap;

	ED_region_panels_init(wm, ar);
	
	keymap= WM_keymap_find(wm->defaultconf, "Node Generic", SPACE_NODE, 0);
	WM_event_add_keymap_handler(&ar->handlers, keymap);
}

static void node_buttons_area_draw(const bContext *C, ARegion *ar)
{
	ED_region_panels(C, ar, 1, NULL, -1);
}

/* Initialize main area, setting handlers. */
static void node_main_area_init(wmWindowManager *wm, ARegion *ar)
{
	wmKeyMap *keymap;
	ListBase *lb;
	
	UI_view2d_region_reinit(&ar->v2d, V2D_COMMONVIEW_CUSTOM, ar->winx, ar->winy);
	
	/* own keymaps */
	keymap= WM_keymap_find(wm->defaultconf, "Node Generic", SPACE_NODE, 0);
	WM_event_add_keymap_handler(&ar->handlers, keymap);
	
	keymap= WM_keymap_find(wm->defaultconf, "Node Editor", SPACE_NODE, 0);
	WM_event_add_keymap_handler_bb(&ar->handlers, keymap, &ar->v2d.mask, &ar->winrct);
	
	/* add drop boxes */
	lb = WM_dropboxmap_find("Node Editor", SPACE_NODE, RGN_TYPE_WINDOW);
	
	WM_event_add_dropbox_handler(&ar->handlers, lb);
}

static void node_main_area_draw(const bContext *C, ARegion *ar)
{
	View2D *v2d= &ar->v2d;
	
	drawnodespace(C, ar, v2d);
}


/* ************* dropboxes ************* */

static int node_drop_poll(bContext *UNUSED(C), wmDrag *drag, wmEvent *UNUSED(event))
{
	if(drag->type==WM_DRAG_ID) {
		ID *id= (ID *)drag->poin;
		if( GS(id->name)==ID_IM )
			return 1;
	}
	else if(drag->type==WM_DRAG_PATH){
		if(ELEM(drag->icon, 0, ICON_FILE_IMAGE))	/* rule might not work? */
			return 1;
	}
	return 0;
}

static void node_id_path_drop_copy(wmDrag *drag, wmDropBox *drop)
{
	ID *id= (ID *)drag->poin;
	
	if(id) {
		RNA_string_set(drop->ptr, "name", id->name+2);
	}
	if (drag->path[0]) {
		RNA_string_set(drop->ptr, "filepath", drag->path);
	}
}

/* this region dropbox definition */
static void node_dropboxes(void)
{
	ListBase *lb= WM_dropboxmap_find("Node Editor", SPACE_NODE, RGN_TYPE_WINDOW);
	
	WM_dropbox_add(lb, "NODE_OT_add_file", node_drop_poll, node_id_path_drop_copy);
	
}

/* ************* end drop *********** */


/* add handlers, stuff you only do once or on area/region changes */
static void node_header_area_init(wmWindowManager *UNUSED(wm), ARegion *ar)
{
	ED_region_header_init(ar);
}

static void node_header_area_draw(const bContext *C, ARegion *ar)
{
	SpaceNode *snode= CTX_wm_space_node(C);
	Scene *scene= CTX_data_scene(C);

	/* find and set the context */
	snode_set_context(snode, scene);

	ED_region_header(C, ar);
}

/* used for header + main area */
static void node_region_listener(ARegion *ar, wmNotifier *wmn)
{
	/* context changes */
	switch(wmn->category) {
		case NC_SPACE:
			if(wmn->data==ND_SPACE_NODE)
				ED_region_tag_redraw(ar);
			break;
		case NC_SCREEN:
			if(wmn->data == ND_GPENCIL)	
				ED_region_tag_redraw(ar);
			break;
		case NC_SCENE:
		case NC_MATERIAL:
		case NC_TEXTURE:
		case NC_NODE:
			ED_region_tag_redraw(ar);
			break;
		case NC_OBJECT:
			if(wmn->data==ND_OB_SHADING)
				ED_region_tag_redraw(ar);
			break;
		case NC_ID:
			if(wmn->action == NA_RENAME)
				ED_region_tag_redraw(ar);
			break;
	}
}

const char *node_context_dir[] = {"selected_nodes", NULL};

static int node_context(const bContext *C, const char *member, bContextDataResult *result)
{
	SpaceNode *snode= CTX_wm_space_node(C);
	
	if(CTX_data_dir(member)) {
		CTX_data_dir_set(result, node_context_dir);
		return 1;
	}
	else if(CTX_data_equals(member, "selected_nodes")) {
		bNode *node;
		
		if(snode->edittree) {
			for(node=snode->edittree->nodes.last; node; node=node->prev) {
				if(node->flag & NODE_SELECT) {
					CTX_data_list_add(result, &snode->edittree->id, &RNA_Node, node);
				}
			}
		}
		CTX_data_type_set(result, CTX_DATA_TYPE_COLLECTION);
		return 1;
	}
	else if(CTX_data_equals(member, "active_node")) {
		bNode *node;
		
		if(snode->edittree) {
			for(node=snode->edittree->nodes.last; node; node=node->prev) {
				if(node->flag & NODE_ACTIVE) {
					CTX_data_pointer_set(result, &snode->edittree->id, &RNA_Node, node);
					break;
				}
			}
		}
		CTX_data_type_set(result, CTX_DATA_TYPE_POINTER);
		return 1;
	}
	
	return 0;
}

/* only called once, from space/spacetypes.c */
void ED_spacetype_node(void)
{
	SpaceType *st= MEM_callocN(sizeof(SpaceType), "spacetype node");
	ARegionType *art;
	
	st->spaceid= SPACE_NODE;
	strncpy(st->name, "Node", BKE_ST_MAXNAME);
	
	st->new= node_new;
	st->free= node_free;
	st->init= node_init;
	st->duplicate= node_duplicate;
	st->operatortypes= node_operatortypes;
	st->keymap= node_keymap;
	st->listener= node_area_listener;
	st->refresh= node_area_refresh;
	st->context= node_context;
	st->dropboxes = node_dropboxes;
	
	/* regions: main window */
	art= MEM_callocN(sizeof(ARegionType), "spacetype node region");
	art->regionid = RGN_TYPE_WINDOW;
	art->init= node_main_area_init;
	art->draw= node_main_area_draw;
	art->listener= node_region_listener;
	art->keymapflag= ED_KEYMAP_UI|ED_KEYMAP_VIEW2D|ED_KEYMAP_FRAMES|ED_KEYMAP_GPENCIL;

	BLI_addhead(&st->regiontypes, art);
	
	/* regions: header */
	art= MEM_callocN(sizeof(ARegionType), "spacetype node region");
	art->regionid = RGN_TYPE_HEADER;
	art->prefsizey= HEADERY;
	art->keymapflag= ED_KEYMAP_UI|ED_KEYMAP_VIEW2D|ED_KEYMAP_FRAMES|ED_KEYMAP_HEADER;
	art->listener= node_region_listener;
	art->init= node_header_area_init;
	art->draw= node_header_area_draw;
	
	BLI_addhead(&st->regiontypes, art);

	node_menus_register();
	
	/* regions: listview/buttons */
	art= MEM_callocN(sizeof(ARegionType), "spacetype node region");
	art->regionid = RGN_TYPE_UI;
	art->prefsizex= 180; // XXX
	art->keymapflag= ED_KEYMAP_UI|ED_KEYMAP_FRAMES;
	art->listener= node_region_listener;
	art->init= node_buttons_area_init;
	art->draw= node_buttons_area_draw;
	BLI_addhead(&st->regiontypes, art);
	
	node_buttons_register(art);
	
	BKE_spacetype_register(st);
}

