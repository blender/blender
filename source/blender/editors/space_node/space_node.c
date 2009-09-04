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
 * Contributor(s): Blender Foundation
 *
 * ***** END GPL LICENSE BLOCK *****
 */

#include <string.h>
#include <stdio.h>

#include "DNA_node_types.h"
#include "DNA_object_types.h"
#include "DNA_material_types.h"
#include "DNA_texture_types.h"
#include "DNA_space_types.h"
#include "DNA_scene_types.h"
#include "DNA_screen_types.h"

#include "MEM_guardedalloc.h"

#include "BLI_blenlib.h"
#include "BLI_arithb.h"
#include "BLI_rand.h"

#include "BKE_colortools.h"
#include "BKE_context.h"
#include "BKE_screen.h"
#include "BKE_node.h"

#include "ED_previewrender.h"
#include "ED_space_api.h"
#include "ED_screen.h"

#include "BIF_gl.h"

#include "WM_api.h"
#include "WM_types.h"

#include "UI_interface.h"
#include "UI_resources.h"
#include "UI_view2d.h"

#include "RNA_access.h"

#include "node_intern.h"	// own include

/* ******************** default callbacks for node space ***************** */

static SpaceLink *node_new(const bContext *C)
{
	ARegion *ar;
	SpaceNode *snode;
	
	snode= MEM_callocN(sizeof(SpaceNode), "initnode");
	snode->spacetype= SPACE_NODE;	
	
	/* header */
	ar= MEM_callocN(sizeof(ARegion), "header for node");
	
	BLI_addtail(&snode->regionbase, ar);
	ar->regiontype= RGN_TYPE_HEADER;
	ar->alignment= RGN_ALIGN_BOTTOM;
	
#if 0
	/* channels */
	ar= MEM_callocN(sizeof(ARegion), "nodetree area for node");
	
	BLI_addtail(&snode->regionbase, ar);
	ar->regiontype= RGN_TYPE_CHANNELS;
	ar->alignment= RGN_ALIGN_LEFT;
	
	//ar->v2d.scroll = (V2D_SCROLL_RIGHT|V2D_SCROLL_BOTTOM);
#endif
	
	/* main area */
	ar= MEM_callocN(sizeof(ARegion), "main area for node");
	
	BLI_addtail(&snode->regionbase, ar);
	ar->regiontype= RGN_TYPE_WINDOW;
	
	ar->v2d.tot.xmin=  -10.0f;
	ar->v2d.tot.ymin=  -10.0f;
	ar->v2d.tot.xmax= 512.0f;
	ar->v2d.tot.ymax= 512.0f;
	
	ar->v2d.cur.xmin=  0.0f;
	ar->v2d.cur.ymin=  0.0f;
	ar->v2d.cur.xmax= 512.0f;
	ar->v2d.cur.ymax= 512.0f;
	
	ar->v2d.min[0]= 1.0f;
	ar->v2d.min[1]= 1.0f;
	
	ar->v2d.max[0]= 32000.0f;
	ar->v2d.max[1]= 32000.0f;
	
	ar->v2d.minzoom= 0.5f;
	ar->v2d.maxzoom= 1.21f;
	
	ar->v2d.scroll= (V2D_SCROLL_RIGHT|V2D_SCROLL_BOTTOM);
	ar->v2d.keepzoom= V2D_LIMITZOOM|V2D_KEEPASPECT;
	ar->v2d.keeptot= 0;
	
	return (SpaceLink *)snode;
}

/* not spacelink itself */
static void node_free(SpaceLink *sl)
{	
//	SpaceNode *snode= (SpaceNode*) sl;
	
// XXX	if(snode->gpd) free_gpencil_data(snode->gpd);
}


/* spacetype; init callback */
static void node_init(struct wmWindowManager *wm, ScrArea *sa)
{

}

static void node_area_listener(ScrArea *sa, wmNotifier *wmn)
{
	
	/* preview renders */
	switch(wmn->category) {
		case NC_SCENE:
			if(wmn->data==ND_NODES)
				ED_area_tag_refresh(sa);
			break;
		case NC_WM:
			if(wmn->data==ND_FILEREAD)
				ED_area_tag_refresh(sa);
			break;
			
		/* future: add ID checks? */
		case NC_MATERIAL:
			if(wmn->data==ND_SHADING)
				ED_area_tag_refresh(sa);
			break;
		case NC_TEXTURE:
			if(wmn->data==ND_NODES)
				ED_area_tag_refresh(sa);
			break;
		case NC_TEXT:
			/* pynodes */
			if(wmn->data==ND_SHADING)
				ED_area_tag_refresh(sa);
			break;
		case NC_SPACE:
			if(wmn->data==ND_SPACE_NODE)
				ED_area_tag_refresh(sa);
			break;
	}
}

static void node_area_refresh(const struct bContext *C, struct ScrArea *sa)
{
	/* default now: refresh node is starting preview */
	SpaceNode *snode= sa->spacedata.first;
	
	if(snode->nodetree) {
		if(snode->treetype==NTREE_SHADER) {
			Material *ma= (Material *)snode->id;
			if(ma->use_nodes)
				ED_preview_shader_job(C, sa, snode->id, NULL, NULL, 100, 100);
		}
		else if(snode->treetype==NTREE_COMPOSIT) {
			Scene *scene= (Scene *)snode->id;
			if(scene->use_nodes)
				snode_composite_job(C, sa);
		}
		else if(snode->treetype==NTREE_TEXTURE) {
			Tex *tex= (Tex *)snode->id;
			if(tex->use_nodes) {
				ED_preview_shader_job(C, sa, snode->id, NULL, NULL, 100, 100);
			}
		}
	}
}

static SpaceLink *node_duplicate(SpaceLink *sl)
{
	SpaceNode *snoden= MEM_dupallocN(sl);
	
	/* clear or remove stuff from old */
	snoden->nodetree= NULL;
// XXX	snoden->gpd= gpencil_data_duplicate(snode->gpd);
	
	return (SpaceLink *)snoden;
}

#if 0
static void node_channel_area_init(wmWindowManager *wm, ARegion *ar)
{
	UI_view2d_region_reinit(&ar->v2d, V2D_COMMONVIEW_LIST, ar->winx, ar->winy);
}

static void node_channel_area_draw(const bContext *C, ARegion *ar)
{
	View2D *v2d= &ar->v2d;
	View2DScrollers *scrollers;
	float col[3];
	
	/* clear and setup matrix */
	UI_GetThemeColor3fv(TH_BACK, col);
	glClearColor(col[0], col[1], col[2], 0.0);
	glClear(GL_COLOR_BUFFER_BIT);
	
	UI_view2d_view_ortho(C, v2d);
	
	/* data... */
	
	/* reset view matrix */
	UI_view2d_view_restore(C);
	
	/* scrollers */
	scrollers= UI_view2d_scrollers_calc(C, v2d, V2D_ARG_DUMMY, V2D_ARG_DUMMY, V2D_ARG_DUMMY, V2D_ARG_DUMMY);
	UI_view2d_scrollers_draw(C, v2d, scrollers);
	UI_view2d_scrollers_free(scrollers);
}
#endif

/* Initialise main area, setting handlers. */
static void node_main_area_init(wmWindowManager *wm, ARegion *ar)
{
	ListBase *keymap;
	
	UI_view2d_region_reinit(&ar->v2d, V2D_COMMONVIEW_CUSTOM, ar->winx, ar->winy);
	
	/* own keymap */
	keymap= WM_keymap_listbase(wm, "Node", SPACE_NODE, 0);	/* XXX weak? */
	WM_event_add_keymap_handler_bb(&ar->handlers, keymap, &ar->v2d.mask, &ar->winrct);
}

static void node_main_area_draw(const bContext *C, ARegion *ar)
{
	View2D *v2d= &ar->v2d;
	
	drawnodespace(C, ar, v2d);
}

/* add handlers, stuff you only do once or on area/region changes */
static void node_header_area_init(wmWindowManager *wm, ARegion *ar)
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
		case NC_SCENE:
		case NC_MATERIAL:
		case NC_TEXTURE:
		case NC_NODE:
			ED_region_tag_redraw(ar);
			break;
	}
}

static int node_context(const bContext *C, const char *member, bContextDataResult *result)
{
	SpaceNode *snode= CTX_wm_space_node(C);
	
	if(CTX_data_dir(member)) {
		static const char *dir[] = {"selected_nodes", NULL};
		CTX_data_dir_set(result, dir);
		return 1;
	}
	else if(CTX_data_equals(member, "selected_nodes")) {
		bNode *node;
		
		for(next_node(snode->edittree); (node=next_node(NULL));) {
			if(node->flag & SELECT) {
				CTX_data_list_add(result, &snode->edittree->id, &RNA_Node, node);
			}
		}
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
	
	st->new= node_new;
	st->free= node_free;
	st->init= node_init;
	st->duplicate= node_duplicate;
	st->operatortypes= node_operatortypes;
	st->keymap= node_keymap;
	st->listener= node_area_listener;
	st->refresh= node_area_refresh;
	st->context= node_context;
	
	/* regions: main window */
	art= MEM_callocN(sizeof(ARegionType), "spacetype node region");
	art->regionid = RGN_TYPE_WINDOW;
	art->init= node_main_area_init;
	art->draw= node_main_area_draw;
	art->listener= node_region_listener;
	art->keymapflag= ED_KEYMAP_UI|ED_KEYMAP_VIEW2D;

	BLI_addhead(&st->regiontypes, art);
	
	/* regions: header */
	art= MEM_callocN(sizeof(ARegionType), "spacetype node region");
	art->regionid = RGN_TYPE_HEADER;
	art->minsizey= HEADERY;
	art->keymapflag= ED_KEYMAP_UI|ED_KEYMAP_VIEW2D|ED_KEYMAP_FRAMES;
	art->listener= node_region_listener;
	art->init= node_header_area_init;
	art->draw= node_header_area_draw;
	
	BLI_addhead(&st->regiontypes, art);

	node_menus_register(art);
	
#if 0
	/* regions: channels */
	art= MEM_callocN(sizeof(ARegionType), "spacetype node region");
	art->regionid = RGN_TYPE_CHANNELS;
	art->minsizex= 100;
	art->keymapflag= ED_KEYMAP_UI|ED_KEYMAP_VIEW2D|ED_KEYMAP_FRAMES;
	
	art->init= node_channel_area_init;
	art->draw= node_channel_area_draw;
	
	BLI_addhead(&st->regiontypes, art);
#endif
	
	
	BKE_spacetype_register(st);
}

