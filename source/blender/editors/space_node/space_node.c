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



#include "DNA_lamp_types.h"
#include "DNA_material_types.h"
#include "DNA_node_types.h"
#include "DNA_world_types.h"

#include "MEM_guardedalloc.h"

#include "BLI_blenlib.h"
#include "BLI_math.h"

#include "BKE_context.h"
#include "BKE_library.h"
#include "BKE_scene.h"
#include "BKE_screen.h"
#include "BKE_node.h"

#include "ED_space_api.h"
#include "ED_node.h"
#include "ED_render.h"
#include "ED_screen.h"
#include "WM_api.h"
#include "WM_types.h"

#include "UI_resources.h"
#include "UI_view2d.h"

#include "RNA_access.h"

#include "node_intern.h"  /* own include */


/* ******************** tree path ********************* */

void ED_node_tree_start(SpaceNode *snode, bNodeTree *ntree, ID *id, ID *from)
{
	bNodeTreePath *path, *path_next;
	for (path = snode->treepath.first; path; path = path_next) {
		path_next = path->next;
		MEM_freeN(path);
	}
	BLI_listbase_clear(&snode->treepath);
	
	if (ntree) {
		path = MEM_callocN(sizeof(bNodeTreePath), "node tree path");
		path->nodetree = ntree;
		path->parent_key = NODE_INSTANCE_KEY_BASE;
		
		/* copy initial offset from bNodeTree */
		copy_v2_v2(path->view_center, ntree->view_center);
		
		if (id)
			BLI_strncpy(path->node_name, id->name + 2, sizeof(path->node_name));
		
		BLI_addtail(&snode->treepath, path);
		
		id_us_ensure_real(&ntree->id);
	}
	
	/* update current tree */
	snode->nodetree = snode->edittree = ntree;
	snode->id = id;
	snode->from = from;
	
	ED_node_set_active_viewer_key(snode);
	
	WM_main_add_notifier(NC_SCENE | ND_NODES, NULL);
}

void ED_node_tree_push(SpaceNode *snode, bNodeTree *ntree, bNode *gnode)
{
	bNodeTreePath *path = MEM_callocN(sizeof(bNodeTreePath), "node tree path");
	bNodeTreePath *prev_path = snode->treepath.last;
	path->nodetree = ntree;
	if (gnode) {
		if (prev_path)
			path->parent_key = BKE_node_instance_key(prev_path->parent_key, prev_path->nodetree, gnode);
		else
			path->parent_key = NODE_INSTANCE_KEY_BASE;
		
		BLI_strncpy(path->node_name, gnode->name, sizeof(path->node_name));
	}
	else
		path->parent_key = NODE_INSTANCE_KEY_BASE;
	
	/* copy initial offset from bNodeTree */
	copy_v2_v2(path->view_center, ntree->view_center);
	
	BLI_addtail(&snode->treepath, path);
	
	id_us_ensure_real(&ntree->id);
	
	/* update current tree */
	snode->edittree = ntree;
	
	ED_node_set_active_viewer_key(snode);
	
	WM_main_add_notifier(NC_SCENE | ND_NODES, NULL);
}

void ED_node_tree_pop(SpaceNode *snode)
{
	bNodeTreePath *path = snode->treepath.last;
	
	/* don't remove root */
	if (path == snode->treepath.first)
		return;
	
	BLI_remlink(&snode->treepath, path);
	MEM_freeN(path);
	
	/* update current tree */
	path = snode->treepath.last;
	snode->edittree = path->nodetree;
	
	ED_node_set_active_viewer_key(snode);
	
	/* listener updates the View2D center from edittree */
	WM_main_add_notifier(NC_SCENE | ND_NODES, NULL);
}

int ED_node_tree_depth(SpaceNode *snode)
{
	return BLI_countlist(&snode->treepath);
}

bNodeTree *ED_node_tree_get(SpaceNode *snode, int level)
{
	bNodeTreePath *path;
	int i;
	for (path = snode->treepath.last, i = 0; path; path = path->prev, ++i) {
		if (i == level)
			return path->nodetree;
	}
	return NULL;
}

int ED_node_tree_path_length(SpaceNode *snode)
{
	bNodeTreePath *path;
	int length = 0;
	int i;
	for (path = snode->treepath.first, i = 0; path; path = path->next, ++i) {
		length += strlen(path->node_name);
		if (i > 0)
			length += 1;	/* for separator char */
	}
	return length;
}

void ED_node_tree_path_get(SpaceNode *snode, char *value)
{
	bNodeTreePath *path;
	int i;
	
	value[0] = '\0';
	for (path = snode->treepath.first, i = 0; path; path = path->next, ++i) {
		if (i == 0) {
			strcpy(value, path->node_name);
			value += strlen(path->node_name);
		}
		else {
			sprintf(value, "/%s", path->node_name);
			value += strlen(path->node_name) + 1;
		}
	}
}

void ED_node_tree_path_get_fixedbuf(SpaceNode *snode, char *value, int max_length)
{
	bNodeTreePath *path;
	int size, i;
	
	value[0] = '\0';
	for (path = snode->treepath.first, i = 0; path; path = path->next, ++i) {
		if (i == 0) {
			BLI_strncpy(value, path->node_name, max_length);
			size = strlen(path->node_name);
		}
		else {
			BLI_snprintf(value, max_length, "/%s", path->node_name);
			size = strlen(path->node_name) + 1;
		}
		max_length -= size;
		if (max_length <= 0)
			break;
		value += size;
	}
}

void ED_node_set_active_viewer_key(SpaceNode *snode)
{
	bNodeTreePath *path = snode->treepath.last;
	if (snode->nodetree && path) {
		snode->nodetree->active_viewer_key = path->parent_key;
	}
}

void snode_group_offset(SpaceNode *snode, float *x, float *y)
{
	bNodeTreePath *path = snode->treepath.last;
	
	if (path && path->prev) {
		float dcenter[2];
		sub_v2_v2v2(dcenter, path->view_center, path->prev->view_center);
		*x = dcenter[0];
		*y = dcenter[1];
	}
	else
		*x = *y = 0.0f;
}

/* ******************** manage regions ********************* */

ARegion *node_has_buttons_region(ScrArea *sa)
{
	ARegion *ar, *arnew;

	ar = BKE_area_find_region_type(sa, RGN_TYPE_UI);
	if (ar) return ar;

	/* add subdiv level; after header */
	ar = BKE_area_find_region_type(sa, RGN_TYPE_HEADER);

	/* is error! */
	if (ar == NULL) return NULL;

	arnew = MEM_callocN(sizeof(ARegion), "buttons for node");

	BLI_insertlinkafter(&sa->regionbase, ar, arnew);
	arnew->regiontype = RGN_TYPE_UI;
	arnew->alignment = RGN_ALIGN_RIGHT;

	arnew->flag = RGN_FLAG_HIDDEN;

	return arnew;
}

ARegion *node_has_tools_region(ScrArea *sa)
{
	ARegion *ar, *arnew;
	
	ar = BKE_area_find_region_type(sa, RGN_TYPE_TOOLS);
	if (ar) return ar;
	
	/* add subdiv level; after header */
	ar = BKE_area_find_region_type(sa, RGN_TYPE_HEADER);
	
	/* is error! */
	if (ar == NULL) return NULL;
	
	arnew = MEM_callocN(sizeof(ARegion), "node tools");
	
	BLI_insertlinkafter(&sa->regionbase, ar, arnew);
	arnew->regiontype = RGN_TYPE_TOOLS;
	arnew->alignment = RGN_ALIGN_LEFT;
	
	arnew->flag = RGN_FLAG_HIDDEN;
	
	return arnew;
}

/* ******************** default callbacks for node space ***************** */

static SpaceLink *node_new(const bContext *UNUSED(C))
{
	ARegion *ar;
	SpaceNode *snode;

	snode = MEM_callocN(sizeof(SpaceNode), "initnode");
	snode->spacetype = SPACE_NODE;

	snode->flag = SNODE_SHOW_GPENCIL | SNODE_USE_ALPHA;

	/* backdrop */
	snode->zoom = 1.0f;

	/* select the first tree type for valid type */
	NODE_TREE_TYPES_BEGIN (treetype)
	{
		strcpy(snode->tree_idname, treetype->idname);
		break;
	}
	NODE_TREE_TYPES_END;

	/* header */
	ar = MEM_callocN(sizeof(ARegion), "header for node");

	BLI_addtail(&snode->regionbase, ar);
	ar->regiontype = RGN_TYPE_HEADER;
	ar->alignment = RGN_ALIGN_BOTTOM;

	/* buttons/list view */
	ar = MEM_callocN(sizeof(ARegion), "buttons for node");

	BLI_addtail(&snode->regionbase, ar);
	ar->regiontype = RGN_TYPE_UI;
	ar->alignment = RGN_ALIGN_RIGHT;

	/* toolbar */
	ar = MEM_callocN(sizeof(ARegion), "node tools");

	BLI_addtail(&snode->regionbase, ar);
	ar->regiontype = RGN_TYPE_TOOLS;
	ar->alignment = RGN_ALIGN_LEFT;

	ar->flag = RGN_FLAG_HIDDEN;

	/* main area */
	ar = MEM_callocN(sizeof(ARegion), "main area for node");

	BLI_addtail(&snode->regionbase, ar);
	ar->regiontype = RGN_TYPE_WINDOW;

	ar->v2d.tot.xmin =  -12.8f * U.widget_unit;
	ar->v2d.tot.ymin =  -12.8f * U.widget_unit;
	ar->v2d.tot.xmax = 38.4f * U.widget_unit;
	ar->v2d.tot.ymax = 38.4f * U.widget_unit;

	ar->v2d.cur =  ar->v2d.tot;

	ar->v2d.min[0] = 1.0f;
	ar->v2d.min[1] = 1.0f;

	ar->v2d.max[0] = 32000.0f;
	ar->v2d.max[1] = 32000.0f;

	ar->v2d.minzoom = 0.09f;
	ar->v2d.maxzoom = 2.31f;

	ar->v2d.scroll = (V2D_SCROLL_RIGHT | V2D_SCROLL_BOTTOM);
	ar->v2d.keepzoom = V2D_LIMITZOOM | V2D_KEEPASPECT;
	ar->v2d.keeptot = 0;

	return (SpaceLink *)snode;
}

static void node_free(SpaceLink *sl)
{
	SpaceNode *snode = (SpaceNode *)sl;
	bNodeTreePath *path, *path_next;

	for (path = snode->treepath.first; path; path = path_next) {
		path_next = path->next;
		MEM_freeN(path);
	}
}


/* spacetype; init callback */
static void node_init(struct wmWindowManager *UNUSED(wm), ScrArea *UNUSED(sa))
{

}

static void node_area_listener(bScreen *sc, ScrArea *sa, wmNotifier *wmn)
{
	/* note, ED_area_tag_refresh will re-execute compositor */
	SpaceNode *snode = sa->spacedata.first;
	/* shaderfrom is only used for new shading nodes, otherwise all shaders are from objects */
	short shader_type = BKE_scene_use_new_shading_nodes(sc->scene) ? snode->shaderfrom : SNODE_SHADER_OBJECT;

	/* preview renders */
	switch (wmn->category) {
		case NC_SCENE:
			switch (wmn->data) {
				case ND_NODES:
				{
					ARegion *ar = BKE_area_find_region_type(sa, RGN_TYPE_WINDOW);
					bNodeTreePath *path = snode->treepath.last;
					/* shift view to node tree center */
					if (ar && path)
						UI_view2d_center_set(&ar->v2d, path->view_center[0], path->view_center[1]);
					
					ED_area_tag_refresh(sa);
					break;
				}
				case ND_FRAME:
					ED_area_tag_refresh(sa);
					break;
				case ND_COMPO_RESULT:
					ED_area_tag_redraw(sa);
					break;
				case ND_TRANSFORM_DONE:
					if (ED_node_is_compositor(snode)) {
						if (snode->flag & SNODE_AUTO_RENDER) {
							snode->recalc = 1;
							ED_area_tag_refresh(sa);
						}
					}
					break;
				case ND_LAYER_CONTENT:
					ED_area_tag_refresh(sa);
					break;
			}
			break;

		/* future: add ID checks? */
		case NC_MATERIAL:
			if (ED_node_is_shader(snode)) {
				if (wmn->data == ND_SHADING)
					ED_area_tag_refresh(sa);
				else if (wmn->data == ND_SHADING_DRAW)
					ED_area_tag_refresh(sa);
				else if (wmn->data == ND_SHADING_LINKS)
					ED_area_tag_refresh(sa);
				else if (wmn->action == NA_ADDED && snode->edittree)
					nodeSetActiveID(snode->edittree, ID_MA, wmn->reference);

			}
			break;
		case NC_TEXTURE:
			if (ED_node_is_shader(snode) || ED_node_is_texture(snode)) {
				if (wmn->data == ND_NODES)
					ED_area_tag_refresh(sa);
			}
			break;
		case NC_WORLD:
			if (ED_node_is_shader(snode) && shader_type == SNODE_SHADER_WORLD) {
				ED_area_tag_refresh(sa);
			}
			break;
		case NC_OBJECT:
			if (ED_node_is_shader(snode)) {
				if (wmn->data == ND_OB_SHADING)
					ED_area_tag_refresh(sa);
			}
			break;
		case NC_SPACE:
			if (wmn->data == ND_SPACE_NODE)
				ED_area_tag_refresh(sa);
			else if (wmn->data == ND_SPACE_NODE_VIEW)
				ED_area_tag_redraw(sa);
			break;
		case NC_NODE:
			if (wmn->action == NA_EDITED)
				ED_area_tag_refresh(sa);
			else if (wmn->action == NA_SELECTED)
				ED_area_tag_redraw(sa);
			break;
		case NC_SCREEN:
			switch (wmn->data) {
				case ND_ANIMPLAY:
					ED_area_tag_refresh(sa);
					break;
			}
			break;
		case NC_MASK:
			if (wmn->action == NA_EDITED) {
				if (snode->nodetree && snode->nodetree->type == NTREE_COMPOSIT) {
					ED_area_tag_refresh(sa);
				}
			}
			break;

		case NC_IMAGE:
			if (wmn->action == NA_EDITED) {
				if (ED_node_is_compositor(snode)) {
					/* note that nodeUpdateID is already called by BKE_image_signal() on all
					 * scenes so really this is just to know if the images is used in the compo else
					 * painting on images could become very slow when the compositor is open. */
					if (nodeUpdateID(snode->nodetree, wmn->reference))
						ED_area_tag_refresh(sa);
				}
			}
			break;

		case NC_MOVIECLIP:
			if (wmn->action == NA_EDITED) {
				if (ED_node_is_compositor(snode)) {
					if (nodeUpdateID(snode->nodetree, wmn->reference))
						ED_area_tag_refresh(sa);
				}
			}
			break;

		case NC_LINESTYLE:
			if (ED_node_is_shader(snode) && shader_type == SNODE_SHADER_LINESTYLE) {
				ED_area_tag_refresh(sa);
			}
			break;
		case NC_WM:
			if(wmn->data == ND_UNDO) {
				ED_area_tag_refresh(sa);
			}
			break;
	}
}

static void node_area_refresh(const struct bContext *C, ScrArea *sa)
{
	/* default now: refresh node is starting preview */
	SpaceNode *snode = sa->spacedata.first;
	
	snode_set_context(C);

	if (snode->nodetree) {
		if (snode->nodetree->type == NTREE_SHADER) {
			if (GS(snode->id->name) == ID_MA) {
				Material *ma = (Material *)snode->id;
				if (ma->use_nodes)
					ED_preview_shader_job(C, sa, snode->id, NULL, NULL, 100, 100, PR_NODE_RENDER);
			}
			else if (GS(snode->id->name) == ID_LA) {
				Lamp *la = (Lamp *)snode->id;
				if (la->use_nodes)
					ED_preview_shader_job(C, sa, snode->id, NULL, NULL, 100, 100, PR_NODE_RENDER);
			}
			else if (GS(snode->id->name) == ID_WO) {
				World *wo = (World *)snode->id;
				if (wo->use_nodes)
					ED_preview_shader_job(C, sa, snode->id, NULL, NULL, 100, 100, PR_NODE_RENDER);
			}
		}
		else if (snode->nodetree->type == NTREE_COMPOSIT) {
			Scene *scene = (Scene *)snode->id;
			if (scene->use_nodes) {
				/* recalc is set on 3d view changes for auto compo */
				if (snode->recalc) {
					snode->recalc = 0;
					node_render_changed_exec((struct bContext *)C, NULL);
				}
				else {
					ED_node_composite_job(C, snode->nodetree, scene);
				}
			}
		}
		else if (snode->nodetree->type == NTREE_TEXTURE) {
			Tex *tex = (Tex *)snode->id;
			if (tex->use_nodes) {
				ED_preview_shader_job(C, sa, snode->id, NULL, NULL, 100, 100, PR_NODE_RENDER);
			}
		}
	}
}

static SpaceLink *node_duplicate(SpaceLink *sl)
{
	SpaceNode *snode = (SpaceNode *)sl;
	SpaceNode *snoden = MEM_dupallocN(snode);

	BLI_duplicatelist(&snoden->treepath, &snode->treepath);

	/* clear or remove stuff from old */
	BLI_listbase_clear(&snoden->linkdrag);

	/* Note: no need to set node tree user counts,
	 * the editor only keeps at least 1 (id_us_ensure_real),
	 * which is already done by the original SpaceNode.
	 */

	return (SpaceLink *)snoden;
}


/* add handlers, stuff you only do once or on area/region changes */
static void node_buttons_area_init(wmWindowManager *wm, ARegion *ar)
{
	wmKeyMap *keymap;

	ED_region_panels_init(wm, ar);

	keymap = WM_keymap_find(wm->defaultconf, "Node Generic", SPACE_NODE, 0);
	WM_event_add_keymap_handler(&ar->handlers, keymap);
}

static void node_buttons_area_draw(const bContext *C, ARegion *ar)
{
	ED_region_panels(C, ar, 1, NULL, -1);
}

/* add handlers, stuff you only do once or on area/region changes */
static void node_toolbar_area_init(wmWindowManager *wm, ARegion *ar)
{
	wmKeyMap *keymap;

	ED_region_panels_init(wm, ar);

	keymap = WM_keymap_find(wm->defaultconf, "Node Generic", SPACE_NODE, 0);
	WM_event_add_keymap_handler(&ar->handlers, keymap);
}

static void node_toolbar_area_draw(const bContext *C, ARegion *ar)
{
	ED_region_panels(C, ar, 1, NULL, -1);
}

static void node_cursor(wmWindow *win, ScrArea *sa, ARegion *ar)
{
	SpaceNode *snode = sa->spacedata.first;

	/* convert mouse coordinates to v2d space */
	UI_view2d_region_to_view(&ar->v2d, win->eventstate->x - ar->winrct.xmin, win->eventstate->y - ar->winrct.ymin,
	                         &snode->cursor[0], &snode->cursor[1]);
	
	/* here snode->cursor is used to detect the node edge for sizing */
	node_set_cursor(win, snode, snode->cursor);

	/* XXX snode->cursor is in placing new nodes space */
	snode->cursor[0] /= UI_DPI_FAC;
	snode->cursor[1] /= UI_DPI_FAC;
	
}

/* Initialize main area, setting handlers. */
static void node_main_area_init(wmWindowManager *wm, ARegion *ar)
{
	wmKeyMap *keymap;
	ListBase *lb;

	UI_view2d_region_reinit(&ar->v2d, V2D_COMMONVIEW_CUSTOM, ar->winx, ar->winy);

	/* own keymaps */
	keymap = WM_keymap_find(wm->defaultconf, "Node Generic", SPACE_NODE, 0);
	WM_event_add_keymap_handler(&ar->handlers, keymap);

	keymap = WM_keymap_find(wm->defaultconf, "Node Editor", SPACE_NODE, 0);
	WM_event_add_keymap_handler_bb(&ar->handlers, keymap, &ar->v2d.mask, &ar->winrct);

	/* add drop boxes */
	lb = WM_dropboxmap_find("Node Editor", SPACE_NODE, RGN_TYPE_WINDOW);

	WM_event_add_dropbox_handler(&ar->handlers, lb);
}

static void node_main_area_draw(const bContext *C, ARegion *ar)
{
	drawnodespace(C, ar);
}


/* ************* dropboxes ************* */

static int node_ima_drop_poll(bContext *UNUSED(C), wmDrag *drag, const wmEvent *UNUSED(event))
{
	if (drag->type == WM_DRAG_ID) {
		ID *id = (ID *)drag->poin;
		if (GS(id->name) == ID_IM)
			return 1;
	}
	else if (drag->type == WM_DRAG_PATH) {
		if (ELEM(drag->icon, 0, ICON_FILE_IMAGE))   /* rule might not work? */
			return 1;
	}
	return 0;
}

static int node_mask_drop_poll(bContext *UNUSED(C), wmDrag *drag, const wmEvent *UNUSED(event))
{
	if (drag->type == WM_DRAG_ID) {
		ID *id = (ID *)drag->poin;
		if (GS(id->name) == ID_MSK)
			return 1;
	}
	return 0;
}

static void node_id_drop_copy(wmDrag *drag, wmDropBox *drop)
{
	ID *id = (ID *)drag->poin;

	RNA_string_set(drop->ptr, "name", id->name + 2);
}

static void node_id_path_drop_copy(wmDrag *drag, wmDropBox *drop)
{
	ID *id = (ID *)drag->poin;

	if (id) {
		RNA_string_set(drop->ptr, "name", id->name + 2);
	}
	if (drag->path[0]) {
		RNA_string_set(drop->ptr, "filepath", drag->path);
	}
}

/* this region dropbox definition */
static void node_dropboxes(void)
{
	ListBase *lb = WM_dropboxmap_find("Node Editor", SPACE_NODE, RGN_TYPE_WINDOW);

	WM_dropbox_add(lb, "NODE_OT_add_file", node_ima_drop_poll, node_id_path_drop_copy);
	WM_dropbox_add(lb, "NODE_OT_add_mask", node_mask_drop_poll, node_id_drop_copy);

}

/* ************* end drop *********** */


/* add handlers, stuff you only do once or on area/region changes */
static void node_header_area_init(wmWindowManager *UNUSED(wm), ARegion *ar)
{
	ED_region_header_init(ar);
}

static void node_header_area_draw(const bContext *C, ARegion *ar)
{
	/* find and set the context */
	snode_set_context(C);

	ED_region_header(C, ar);
}

/* used for header + main area */
static void node_region_listener(bScreen *UNUSED(sc), ScrArea *UNUSED(sa), ARegion *ar, wmNotifier *wmn)
{
	/* context changes */
	switch (wmn->category) {
		case NC_SPACE:
			if (wmn->data == ND_SPACE_NODE)
				ED_region_tag_redraw(ar);
			break;
		case NC_SCREEN:
			switch (wmn->data) {
				case ND_SCREENCAST:
				case ND_ANIMPLAY:
					ED_region_tag_redraw(ar);
					break;
			}
			break;
		case NC_SCENE:
		case NC_MATERIAL:
		case NC_TEXTURE:
		case NC_WORLD:
		case NC_NODE:
		case NC_LINESTYLE:
			ED_region_tag_redraw(ar);
			break;
		case NC_OBJECT:
			if (wmn->data == ND_OB_SHADING)
				ED_region_tag_redraw(ar);
			break;
		case NC_ID:
			if (wmn->action == NA_RENAME)
				ED_region_tag_redraw(ar);
			break;
		case NC_GPENCIL:
			if (wmn->action == NA_EDITED)
				ED_region_tag_redraw(ar);
			break;
	}
}

const char *node_context_dir[] = {"selected_nodes", "active_node", NULL};

static int node_context(const bContext *C, const char *member, bContextDataResult *result)
{
	SpaceNode *snode = CTX_wm_space_node(C);

	if (CTX_data_dir(member)) {
		CTX_data_dir_set(result, node_context_dir);
		return 1;
	}
	else if (CTX_data_equals(member, "selected_nodes")) {
		bNode *node;

		if (snode->edittree) {
			for (node = snode->edittree->nodes.last; node; node = node->prev) {
				if (node->flag & NODE_SELECT) {
					CTX_data_list_add(result, &snode->edittree->id, &RNA_Node, node);
				}
			}
		}
		CTX_data_type_set(result, CTX_DATA_TYPE_COLLECTION);
		return 1;
	}
	else if (CTX_data_equals(member, "active_node")) {
		if (snode->edittree) {
			bNode *node = nodeGetActive(snode->edittree);
			CTX_data_pointer_set(result, &snode->edittree->id, &RNA_Node, node);
		}

		CTX_data_type_set(result, CTX_DATA_TYPE_POINTER);
		return 1;
	}
	else if (CTX_data_equals(member, "node_previews")) {
		if (snode->nodetree) {
			CTX_data_pointer_set(result, &snode->nodetree->id, &RNA_NodeInstanceHash, snode->nodetree->previews);
		}

		CTX_data_type_set(result, CTX_DATA_TYPE_POINTER);
		return 1;
	}

	return 0;
}

/* only called once, from space/spacetypes.c */
void ED_spacetype_node(void)
{
	SpaceType *st = MEM_callocN(sizeof(SpaceType), "spacetype node");
	ARegionType *art;

	st->spaceid = SPACE_NODE;
	strncpy(st->name, "Node", BKE_ST_MAXNAME);

	st->new = node_new;
	st->free = node_free;
	st->init = node_init;
	st->duplicate = node_duplicate;
	st->operatortypes = node_operatortypes;
	st->keymap = node_keymap;
	st->listener = node_area_listener;
	st->refresh = node_area_refresh;
	st->context = node_context;
	st->dropboxes = node_dropboxes;

	/* regions: main window */
	art = MEM_callocN(sizeof(ARegionType), "spacetype node region");
	art->regionid = RGN_TYPE_WINDOW;
	art->init = node_main_area_init;
	art->draw = node_main_area_draw;
	art->listener = node_region_listener;
	art->cursor = node_cursor;
	art->event_cursor = true;
	art->keymapflag = ED_KEYMAP_UI | ED_KEYMAP_VIEW2D | ED_KEYMAP_FRAMES | ED_KEYMAP_GPENCIL;

	BLI_addhead(&st->regiontypes, art);

	/* regions: header */
	art = MEM_callocN(sizeof(ARegionType), "spacetype node region");
	art->regionid = RGN_TYPE_HEADER;
	art->prefsizey = HEADERY;
	art->keymapflag = ED_KEYMAP_UI | ED_KEYMAP_VIEW2D | ED_KEYMAP_FRAMES | ED_KEYMAP_HEADER;
	art->listener = node_region_listener;
	art->init = node_header_area_init;
	art->draw = node_header_area_draw;

	BLI_addhead(&st->regiontypes, art);

	/* regions: listview/buttons */
	art = MEM_callocN(sizeof(ARegionType), "spacetype node region");
	art->regionid = RGN_TYPE_UI;
	art->prefsizex = 180; // XXX
	art->keymapflag = ED_KEYMAP_UI | ED_KEYMAP_FRAMES;
	art->listener = node_region_listener;
	art->init = node_buttons_area_init;
	art->draw = node_buttons_area_draw;
	BLI_addhead(&st->regiontypes, art);

	node_buttons_register(art);

	/* regions: toolbar */
	art = MEM_callocN(sizeof(ARegionType), "spacetype view3d tools region");
	art->regionid = RGN_TYPE_TOOLS;
	art->prefsizex = 160; /* XXX */
	art->prefsizey = 50; /* XXX */
	art->keymapflag = ED_KEYMAP_UI | ED_KEYMAP_FRAMES;
	art->listener = node_region_listener;
	art->init = node_toolbar_area_init;
	art->draw = node_toolbar_area_draw;
	BLI_addhead(&st->regiontypes, art);
	
	node_toolbar_register(art);

	BKE_spacetype_register(st);
}

