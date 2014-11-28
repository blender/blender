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
 * The Original Code is: all of this file.
 * Contributor(s): Nathan Letwory
 *
 * ***** END GPL LICENSE BLOCK *****
 */

/** \file blender/editors/space_node/node_draw.c
 *  \ingroup spnode
 *  \brief higher level node drawing for the node editor.
 */

#include "DNA_lamp_types.h"
#include "DNA_node_types.h"
#include "DNA_material_types.h"
#include "DNA_screen_types.h"
#include "DNA_space_types.h"
#include "DNA_texture_types.h"
#include "DNA_world_types.h"
#include "DNA_linestyle_types.h"

#include "BLI_math.h"
#include "BLI_blenlib.h"

#include "BLF_translation.h"

#include "BKE_context.h"
#include "BKE_depsgraph.h"
#include "BKE_main.h"
#include "BKE_node.h"

#include "BLF_api.h"

#include "BIF_gl.h"
#include "BIF_glutil.h"

#include "WM_api.h"
#include "WM_types.h"

#include "ED_node.h"
#include "ED_gpencil.h"
#include "ED_space_api.h"

#include "UI_resources.h"
#include "UI_view2d.h"

#include "RNA_access.h"

#include "node_intern.h"  /* own include */

#ifdef WITH_COMPOSITOR
#  include "COM_compositor.h"
#endif

/* XXX interface.h */
extern void ui_draw_dropshadow(const rctf *rct, float radius, float aspect, float alpha, int select);

float ED_node_grid_size(void)
{
	return U.widget_unit;
}

void ED_node_tree_update(const bContext *C)
{
	SpaceNode *snode = CTX_wm_space_node(C);
	if (snode) {
		snode_set_context(C);

		if (snode->nodetree && snode->nodetree->id.us == 0) {
			snode->nodetree->id.us = 1;
		}
	}
}

/* id is supposed to contain a node tree */
static bNodeTree *node_tree_from_ID(ID *id)
{
	if (id) {
		short idtype = GS(id->name);
	
		switch (idtype) {
			case ID_NT:
				return (bNodeTree *)id;
			case ID_MA:
				return ((Material *)id)->nodetree;
			case ID_LA:
				return ((Lamp *)id)->nodetree;
			case ID_WO:
				return ((World *)id)->nodetree;
			case ID_SCE:
				return ((Scene *)id)->nodetree;
			case ID_TE:
				return ((Tex *)id)->nodetree;
			case ID_LS:
				return ((FreestyleLineStyle *)id)->nodetree;
		}
	}
	
	return NULL;
}

void ED_node_tag_update_id(ID *id)
{
	bNodeTree *ntree = node_tree_from_ID(id);
	if (id == NULL || ntree == NULL)
		return;
	
	if (ntree->type == NTREE_SHADER) {
		DAG_id_tag_update(id, 0);
		
		if (GS(id->name) == ID_MA)
			WM_main_add_notifier(NC_MATERIAL | ND_SHADING, id);
		else if (GS(id->name) == ID_LA)
			WM_main_add_notifier(NC_LAMP | ND_LIGHTING, id);
		else if (GS(id->name) == ID_WO)
			WM_main_add_notifier(NC_WORLD | ND_WORLD, id);
	}
	else if (ntree->type == NTREE_COMPOSIT) {
		WM_main_add_notifier(NC_SCENE | ND_NODES, id);
	}
	else if (ntree->type == NTREE_TEXTURE) {
		DAG_id_tag_update(id, 0);
		WM_main_add_notifier(NC_TEXTURE | ND_NODES, id);
	}
	else if (id == &ntree->id) {
		/* node groups */
		DAG_id_tag_update(id, 0);
	}
}

void ED_node_tag_update_nodetree(Main *bmain, bNodeTree *ntree)
{
	if (!ntree)
		return;
	
	/* look through all datablocks, to support groups */
	FOREACH_NODETREE(bmain, tntree, id) {
		/* check if nodetree uses the group */
		if (ntreeHasTree(tntree, ntree))
			ED_node_tag_update_id(id);
	} FOREACH_NODETREE_END
	
	if (ntree->type == NTREE_TEXTURE)
		ntreeTexCheckCyclics(ntree);
}

static int compare_nodes(bNode *a, bNode *b)
{
	bNode *parent;
	/* These tell if either the node or any of the parent nodes is selected.
	 * A selected parent means an unselected node is also in foreground!
	 */
	int a_select = (a->flag & NODE_SELECT), b_select = (b->flag & NODE_SELECT);
	int a_active = (a->flag & NODE_ACTIVE), b_active = (b->flag & NODE_ACTIVE);
	
	/* if one is an ancestor of the other */
	/* XXX there might be a better sorting algorithm for stable topological sort, this is O(n^2) worst case */
	for (parent = a->parent; parent; parent = parent->parent) {
		/* if b is an ancestor, it is always behind a */
		if (parent == b)
			return 1;
		/* any selected ancestor moves the node forward */
		if (parent->flag & NODE_ACTIVE)
			a_active = 1;
		if (parent->flag & NODE_SELECT)
			a_select = 1;
	}
	for (parent = b->parent; parent; parent = parent->parent) {
		/* if a is an ancestor, it is always behind b */
		if (parent == a)
			return 0;
		/* any selected ancestor moves the node forward */
		if (parent->flag & NODE_ACTIVE)
			b_active = 1;
		if (parent->flag & NODE_SELECT)
			b_select = 1;
	}

	/* if one of the nodes is in the background and the other not */
	if ((a->flag & NODE_BACKGROUND) && !(b->flag & NODE_BACKGROUND))
		return 0;
	else if (!(a->flag & NODE_BACKGROUND) && (b->flag & NODE_BACKGROUND))
		return 1;
	
	/* if one has a higher selection state (active > selected > nothing) */
	if (!b_active && a_active)
		return 1;
	else if (!b_select && (a_active || a_select))
		return 1;
	
	return 0;
}

/* Sorts nodes by selection: unselected nodes first, then selected,
 * then the active node at the very end. Relative order is kept intact!
 */
void ED_node_sort(bNodeTree *ntree)
{
	/* merge sort is the algorithm of choice here */
	bNode *first_a, *first_b, *node_a, *node_b, *tmp;
	int totnodes = BLI_listbase_count(&ntree->nodes);
	int k, a, b;
	
	k = 1;
	while (k < totnodes) {
		first_a = first_b = ntree->nodes.first;
		
		do {
			/* setup first_b pointer */
			for (b = 0; b < k && first_b; ++b) {
				first_b = first_b->next;
			}
			/* all batches merged? */
			if (first_b == NULL)
				break;
			
			/* merge batches */
			node_a = first_a;
			node_b = first_b;
			a = b = 0;
			while (a < k && b < k && node_b) {
				if (compare_nodes(node_a, node_b) == 0) {
					node_a = node_a->next;
					a++;
				}
				else {
					tmp = node_b;
					node_b = node_b->next;
					b++;
					BLI_remlink(&ntree->nodes, tmp);
					BLI_insertlinkbefore(&ntree->nodes, node_a, tmp);
				}
			}

			/* setup first pointers for next batch */
			first_b = node_b;
			for (; b < k; ++b) {
				/* all nodes sorted? */
				if (first_b == NULL)
					break;
				first_b = first_b->next;
			}
			first_a = first_b;
		} while (first_b);
		
		k = k << 1;
	}
}


static void do_node_internal_buttons(bContext *C, void *UNUSED(node_v), int event)
{
	if (event == B_NODE_EXEC) {
		SpaceNode *snode = CTX_wm_space_node(C);
		if (snode && snode->id)
			ED_node_tag_update_id(snode->id);
	}
}

static void node_uiblocks_init(const bContext *C, bNodeTree *ntree)
{
	bNode *node;
	char uiblockstr[32];
	
	/* add node uiBlocks in drawing order - prevents events going to overlapping nodes */
	
	for (node = ntree->nodes.first; node; node = node->next) {
		/* ui block */
		BLI_snprintf(uiblockstr, sizeof(uiblockstr), "node buttons %p", (void *)node);
		node->block = UI_block_begin(C, CTX_wm_region(C), uiblockstr, UI_EMBOSS);
		UI_block_func_handle_set(node->block, do_node_internal_buttons, node);

		/* this cancels events for background nodes */
		UI_block_flag_enable(node->block, UI_BLOCK_CLIP_EVENTS);
	}
}

void node_to_view(struct bNode *node, float x, float y, float *rx, float *ry)
{
	nodeToView(node, x, y, rx, ry);
	*rx *= UI_DPI_FAC;
	*ry *= UI_DPI_FAC;
}

void node_from_view(struct bNode *node, float x, float y, float *rx, float *ry)
{
	x /= UI_DPI_FAC;
	y /= UI_DPI_FAC;
	nodeFromView(node, x, y, rx, ry);
}


/* based on settings in node, sets drawing rect info. each redraw! */
static void node_update_basis(const bContext *C, bNodeTree *ntree, bNode *node)
{
	uiLayout *layout, *row;
	PointerRNA nodeptr, sockptr;
	bNodeSocket *nsock;
	float locx, locy;
	float dy;
	int buty;
	
	RNA_pointer_create(&ntree->id, &RNA_Node, node, &nodeptr);
	
	/* get "global" coords */
	node_to_view(node, 0.0f, 0.0f, &locx, &locy);
	dy = locy;
	
	/* header */
	dy -= NODE_DY;
	
	/* little bit space in top */
	if (node->outputs.first)
		dy -= NODE_DYS / 2;
	
	/* output sockets */
	for (nsock = node->outputs.first; nsock; nsock = nsock->next) {
		if (nodeSocketIsHidden(nsock))
			continue;
		
		RNA_pointer_create(&ntree->id, &RNA_NodeSocket, nsock, &sockptr);
		
		layout = UI_block_layout(
		        node->block, UI_LAYOUT_VERTICAL, UI_LAYOUT_PANEL,
		        locx + NODE_DYS, dy, NODE_WIDTH(node) - NODE_DY, NODE_DY, 0, UI_style_get());
		/* context pointers for current node and socket */
		uiLayoutSetContextPointer(layout, "node", &nodeptr);
		uiLayoutSetContextPointer(layout, "socket", &sockptr);
		
		/* align output buttons to the right */
		row = uiLayoutRow(layout, 1);
		uiLayoutSetAlignment(row, UI_LAYOUT_ALIGN_RIGHT);
		
		nsock->typeinfo->draw((bContext *)C, row, &sockptr, &nodeptr, IFACE_(nsock->name));
		
		UI_block_align_end(node->block);
		UI_block_layout_resolve(node->block, NULL, &buty);
		
		/* ensure minimum socket height in case layout is empty */
		buty = min_ii(buty, dy - NODE_DY);
		
		nsock->locx = locx + NODE_WIDTH(node);
		/* place the socket circle in the middle of the layout */
		nsock->locy = 0.5f * (dy + buty);
		
		dy = buty;
		if (nsock->next)
			dy -= NODE_SOCKDY;
	}

	node->prvr.xmin = locx + NODE_DYS;
	node->prvr.xmax = locx + NODE_WIDTH(node) - NODE_DYS;

	/* preview rect? */
	if (node->flag & NODE_PREVIEW) {
		float aspect = 1.0f;
		
		if (node->preview_xsize && node->preview_ysize) 
			aspect = (float)node->preview_ysize / (float)node->preview_xsize;
		
		dy -= NODE_DYS / 2;
		node->prvr.ymax = dy;
		
		if (aspect <= 1.0f)
			node->prvr.ymin = dy - aspect * (NODE_WIDTH(node) - NODE_DY);
		else {
			/* width correction of image */
			/* XXX huh? (ton) */
			float dx = (NODE_WIDTH(node) - NODE_DYS) - (NODE_WIDTH(node) - NODE_DYS) / aspect;
			
			node->prvr.ymin = dy - (NODE_WIDTH(node) - NODE_DY);
			
			node->prvr.xmin += 0.5f * dx;
			node->prvr.xmax -= 0.5f * dx;
		}
		
		dy = node->prvr.ymin - NODE_DYS / 2;
		
		/* make sure that maximums are bigger or equal to minimums */
		if (node->prvr.xmax < node->prvr.xmin) SWAP(float, node->prvr.xmax, node->prvr.xmin);
		if (node->prvr.ymax < node->prvr.ymin) SWAP(float, node->prvr.ymax, node->prvr.ymin);
	}

	/* buttons rect? */
	if (node->typeinfo->draw_buttons && (node->flag & NODE_OPTIONS)) {
		dy -= NODE_DYS / 2;

		/* set this for uifunc() that don't use layout engine yet */
		node->butr.xmin = 0;
		node->butr.xmax = NODE_WIDTH(node) - 2 * NODE_DYS;
		node->butr.ymin = 0;
		node->butr.ymax = 0;
		
			
		layout = UI_block_layout(
		        node->block, UI_LAYOUT_VERTICAL, UI_LAYOUT_PANEL,
		        locx + NODE_DYS, dy, node->butr.xmax, 0, 0, UI_style_get());
		uiLayoutSetContextPointer(layout, "node", &nodeptr);
		
		node->typeinfo->draw_buttons(layout, (bContext *)C, &nodeptr);
		
		UI_block_align_end(node->block);
		UI_block_layout_resolve(node->block, NULL, &buty);
			
		dy = buty - NODE_DYS / 2;
	}

	/* input sockets */
	for (nsock = node->inputs.first; nsock; nsock = nsock->next) {
		if (nodeSocketIsHidden(nsock))
			continue;
		
		RNA_pointer_create(&ntree->id, &RNA_NodeSocket, nsock, &sockptr);
		
		layout = UI_block_layout(
		        node->block, UI_LAYOUT_VERTICAL, UI_LAYOUT_PANEL,
		        locx + NODE_DYS, dy, NODE_WIDTH(node) - NODE_DY, NODE_DY, 0, UI_style_get());
		/* context pointers for current node and socket */
		uiLayoutSetContextPointer(layout, "node", &nodeptr);
		uiLayoutSetContextPointer(layout, "socket", &sockptr);
		
		row = uiLayoutRow(layout, 1);
		
		nsock->typeinfo->draw((bContext *)C, row, &sockptr, &nodeptr, IFACE_(nsock->name));
		
		UI_block_align_end(node->block);
		UI_block_layout_resolve(node->block, NULL, &buty);
		
		/* ensure minimum socket height in case layout is empty */
		buty = min_ii(buty, dy - NODE_DY);
		
		nsock->locx = locx;
		/* place the socket circle in the middle of the layout */
		nsock->locy = 0.5f * (dy + buty);
		
		dy = buty;
		if (nsock->next)
			dy -= NODE_SOCKDY;
	}
	
	/* little bit space in end */
	if (node->inputs.first || (node->flag & (NODE_OPTIONS | NODE_PREVIEW)) == 0)
		dy -= NODE_DYS / 2;

	node->totr.xmin = locx;
	node->totr.xmax = locx + NODE_WIDTH(node);
	node->totr.ymax = locy;
	node->totr.ymin = min_ff(dy, locy - 2 * NODE_DY);
	
	/* Set the block bounds to clip mouse events from underlying nodes.
	 * Add a margin for sockets on each side.
	 */
	UI_block_bounds_set_explicit(
	        node->block,
	        node->totr.xmin - NODE_SOCKSIZE,
	        node->totr.ymin,
	        node->totr.xmax + NODE_SOCKSIZE,
	        node->totr.ymax);
}

/* based on settings in node, sets drawing rect info. each redraw! */
static void node_update_hidden(bNode *node)
{
	bNodeSocket *nsock;
	float locx, locy;
	float rad, drad, hiddenrad = HIDDEN_RAD;
	int totin = 0, totout = 0, tot;
	
	/* get "global" coords */
	node_to_view(node, 0.0f, 0.0f, &locx, &locy);

	/* calculate minimal radius */
	for (nsock = node->inputs.first; nsock; nsock = nsock->next)
		if (!nodeSocketIsHidden(nsock))
			totin++;
	for (nsock = node->outputs.first; nsock; nsock = nsock->next)
		if (!nodeSocketIsHidden(nsock))
			totout++;
	
	tot = MAX2(totin, totout);
	if (tot > 4) {
		hiddenrad += 5.0f * (float)(tot - 4);
	}
	
	node->totr.xmin = locx;
	node->totr.xmax = locx + 3 * hiddenrad + node->miniwidth;
	node->totr.ymax = locy + (hiddenrad - 0.5f * NODE_DY);
	node->totr.ymin = node->totr.ymax - 2 * hiddenrad;
	
	/* output sockets */
	rad = drad = (float)M_PI / (1.0f + (float)totout);
	
	for (nsock = node->outputs.first; nsock; nsock = nsock->next) {
		if (!nodeSocketIsHidden(nsock)) {
			nsock->locx = node->totr.xmax - hiddenrad + sinf(rad) * hiddenrad;
			nsock->locy = node->totr.ymin + hiddenrad + cosf(rad) * hiddenrad;
			rad += drad;
		}
	}
	
	/* input sockets */
	rad = drad = -(float)M_PI / (1.0f + (float)totin);
	
	for (nsock = node->inputs.first; nsock; nsock = nsock->next) {
		if (!nodeSocketIsHidden(nsock)) {
			nsock->locx = node->totr.xmin + hiddenrad + sinf(rad) * hiddenrad;
			nsock->locy = node->totr.ymin + hiddenrad + cosf(rad) * hiddenrad;
			rad += drad;
		}
	}

	/* Set the block bounds to clip mouse events from underlying nodes.
	 * Add a margin for sockets on each side.
	 */
	UI_block_bounds_set_explicit(
	        node->block,
	        node->totr.xmin - NODE_SOCKSIZE,
	        node->totr.ymin,
	        node->totr.xmax + NODE_SOCKSIZE,
	        node->totr.ymax);
}

void node_update_default(const bContext *C, bNodeTree *ntree, bNode *node)
{
	if (node->flag & NODE_HIDDEN)
		node_update_hidden(node);
	else
		node_update_basis(C, ntree, node);
}

int node_select_area_default(bNode *node, int x, int y)
{
	return BLI_rctf_isect_pt(&node->totr, x, y);
}

int node_tweak_area_default(bNode *node, int x, int y)
{
	return BLI_rctf_isect_pt(&node->totr, x, y);
}

int node_get_colorid(bNode *node)
{
	switch (node->typeinfo->nclass) {
		case NODE_CLASS_INPUT:      return TH_NODE_INPUT;
		case NODE_CLASS_OUTPUT:     return (node->flag & NODE_DO_OUTPUT) ? TH_NODE_OUTPUT : TH_NODE;
		case NODE_CLASS_CONVERTOR:  return TH_NODE_CONVERTOR;
		case NODE_CLASS_OP_COLOR:   return TH_NODE_COLOR;
		case NODE_CLASS_OP_VECTOR:  return TH_NODE_VECTOR;
		case NODE_CLASS_OP_FILTER:  return TH_NODE_FILTER;
		case NODE_CLASS_GROUP:      return TH_NODE_GROUP;
		case NODE_CLASS_INTERFACE:  return TH_NODE_INTERFACE;
		case NODE_CLASS_MATTE:      return TH_NODE_MATTE;
		case NODE_CLASS_DISTORT:    return TH_NODE_DISTORT;
		case NODE_CLASS_TEXTURE:    return TH_NODE_TEXTURE;
		case NODE_CLASS_SHADER:     return TH_NODE_SHADER;
		case NODE_CLASS_SCRIPT:     return TH_NODE_SCRIPT;
		case NODE_CLASS_PATTERN:    return TH_NODE_PATTERN;
		case NODE_CLASS_LAYOUT:     return TH_NODE_LAYOUT;
		default:                    return TH_NODE;
	}
}

/* note: in cmp_util.c is similar code, for node_compo_pass_on()
 *       the same goes for shader and texture nodes. */
/* note: in node_edit.c is similar code, for untangle node */
static void node_draw_mute_line(View2D *v2d, SpaceNode *snode, bNode *node)
{
	bNodeLink *link;

	glEnable(GL_BLEND);
	glEnable(GL_LINE_SMOOTH);

	for (link = node->internal_links.first; link; link = link->next)
		node_draw_link_bezier(v2d, snode, link, TH_REDALERT, 0, TH_WIRE, 0, TH_WIRE);

	glDisable(GL_BLEND);
	glDisable(GL_LINE_SMOOTH);
}

/* this might have some more generic use */
static void node_circle_draw(float x, float y, float size, const float col[4], int highlight)
{
	/* 16 values of sin function */
	static const float si[16] = {
		0.00000000f, 0.39435585f, 0.72479278f, 0.93775213f,
		0.99871650f, 0.89780453f, 0.65137248f, 0.29936312f,
		-0.10116832f, -0.48530196f, -0.79077573f, -0.96807711f,
		-0.98846832f, -0.84864425f, -0.57126821f, -0.20129852f
	};
	/* 16 values of cos function */
	static const float co[16] = {
		1.00000000f, 0.91895781f, 0.68896691f, 0.34730525f,
		-0.05064916f, -0.44039415f, -0.75875812f, -0.95413925f,
		-0.99486932f, -0.87434661f, -0.61210598f, -0.25065253f,
		0.15142777f, 0.52896401f, 0.82076344f, 0.97952994f,
	};
	int a;
	
	glColor4fv(col);
	
	glEnable(GL_BLEND);
	glBegin(GL_POLYGON);
	for (a = 0; a < 16; a++)
		glVertex2f(x + size * si[a], y + size * co[a]);
	glEnd();
	glDisable(GL_BLEND);
	
	if (highlight) {
		UI_ThemeColor(TH_TEXT_HI);
		glLineWidth(1.5f);
	}
	else {
		glColor4ub(0, 0, 0, 150);
	}
	glEnable(GL_BLEND);
	glEnable(GL_LINE_SMOOTH);
	glBegin(GL_LINE_LOOP);
	for (a = 0; a < 16; a++)
		glVertex2f(x + size * si[a], y + size * co[a]);
	glEnd();
	glDisable(GL_LINE_SMOOTH);
	glDisable(GL_BLEND);
	glLineWidth(1.0f);
}

void node_socket_circle_draw(const bContext *C, bNodeTree *ntree, bNode *node, bNodeSocket *sock, float size, int highlight)
{
	PointerRNA ptr, node_ptr;
	float color[4];
	
	RNA_pointer_create((ID *)ntree, &RNA_NodeSocket, sock, &ptr);
	RNA_pointer_create((ID *)ntree, &RNA_Node, node, &node_ptr);
	sock->typeinfo->draw_color((bContext *)C, &ptr, &node_ptr, color);
	node_circle_draw(sock->locx, sock->locy, size, color, highlight);
}

/* **************  Socket callbacks *********** */

static void node_draw_preview_background(float tile, rctf *rect)
{
	float x, y;
	
	/* draw checkerboard backdrop to show alpha */
	glColor3ub(120, 120, 120);
	glRectf(rect->xmin, rect->ymin, rect->xmax, rect->ymax);
	glColor3ub(160, 160, 160);
	
	for (y = rect->ymin; y < rect->ymax; y += tile * 2) {
		for (x = rect->xmin; x < rect->xmax; x += tile * 2) {
			float tilex = tile, tiley = tile;

			if (x + tile > rect->xmax)
				tilex = rect->xmax - x;
			if (y + tile > rect->ymax)
				tiley = rect->ymax - y;

			glRectf(x, y, x + tilex, y + tiley);
		}
	}
	for (y = rect->ymin + tile; y < rect->ymax; y += tile * 2) {
		for (x = rect->xmin + tile; x < rect->xmax; x += tile * 2) {
			float tilex = tile, tiley = tile;

			if (x + tile > rect->xmax)
				tilex = rect->xmax - x;
			if (y + tile > rect->ymax)
				tiley = rect->ymax - y;

			glRectf(x, y, x + tilex, y + tiley);
		}
	}
}

/* not a callback */
static void node_draw_preview(bNodePreview *preview, rctf *prv)
{
	float xrect = BLI_rctf_size_x(prv);
	float yrect = BLI_rctf_size_y(prv);
	float xscale = xrect / ((float)preview->xsize);
	float yscale = yrect / ((float)preview->ysize);
	float scale;
	rctf draw_rect;
	
	/* uniform scale and offset */
	draw_rect = *prv;
	if (xscale < yscale) {
		float offset = 0.5f * (yrect - ((float)preview->ysize) * xscale);
		draw_rect.ymin += offset;
		draw_rect.ymax -= offset;
		scale = xscale;
	}
	else {
		float offset = 0.5f * (xrect - ((float)preview->xsize) * yscale);
		draw_rect.xmin += offset;
		draw_rect.xmax -= offset;
		scale = yscale;
	}
	
	node_draw_preview_background(BLI_rctf_size_x(prv) / 10.0f, &draw_rect);
	
	glEnable(GL_BLEND);
	glBlendFunc(GL_SRC_ALPHA, GL_ONE_MINUS_SRC_ALPHA);  /* premul graphics */
	
	glColor4f(1.0, 1.0, 1.0, 1.0);
	glPixelZoom(scale, scale);
	glaDrawPixelsTex(draw_rect.xmin, draw_rect.ymin, preview->xsize, preview->ysize, GL_RGBA, GL_UNSIGNED_BYTE, GL_LINEAR, preview->rect);
	glPixelZoom(1.0f, 1.0f);
	
	glDisable(GL_BLEND);

	UI_ThemeColorShadeAlpha(TH_BACK, -15, +100);
	fdrawbox(draw_rect.xmin, draw_rect.ymin, draw_rect.xmax, draw_rect.ymax);
}

/* common handle function for operator buttons that need to select the node first */
static void node_toggle_button_cb(struct bContext *C, void *node_argv, void *op_argv)
{
	bNode *node = (bNode *)node_argv;
	const char *opname = (const char *)op_argv;
	
	/* select & activate only the button's node */
	node_select_single(C, node);
	
	WM_operator_name_call(C, opname, WM_OP_INVOKE_DEFAULT, NULL);
}

void node_draw_shadow(SpaceNode *snode, bNode *node, float radius, float alpha)
{
	rctf *rct = &node->totr;
	
	UI_draw_roundbox_corner_set(UI_CNR_ALL);
	if (node->parent == NULL)
		ui_draw_dropshadow(rct, radius, snode->aspect, alpha, node->flag & SELECT);
	else {
		const float margin = 3.0f;
		
		glColor4f(0.0f, 0.0f, 0.0f, 0.33f);
		glEnable(GL_BLEND);
		UI_draw_roundbox(rct->xmin - margin, rct->ymin - margin,
		                 rct->xmax + margin, rct->ymax + margin, radius + margin);
		glDisable(GL_BLEND);
	}
}

static void node_draw_basis(const bContext *C, ARegion *ar, SpaceNode *snode, bNodeTree *ntree, bNode *node, bNodeInstanceKey key)
{
	bNodeInstanceHash *previews = CTX_data_pointer_get(C, "node_previews").data;
	bNodeSocket *sock;
	rctf *rct = &node->totr;
	float iconofs;
	/* float socket_size = NODE_SOCKSIZE*U.dpi/72; */ /* UNUSED */
	float iconbutw = 0.8f * UI_UNIT_X;
	int color_id = node_get_colorid(node);
	char showname[128]; /* 128 used below */
	View2D *v2d = &ar->v2d;
	
	/* XXX hack: copy values from linked ID data where displayed as sockets */
	if (node->block)
		nodeSynchronizeID(node, false);
	
	/* skip if out of view */
	if (BLI_rctf_isect(&node->totr, &ar->v2d.cur, NULL) == false) {
		UI_block_end(C, node->block);
		node->block = NULL;
		return;
	}
	
	/* shadow */
	node_draw_shadow(snode, node, BASIS_RAD, 1.0f);
	
	/* header uses color from backdrop, but we make it opaqie */
	if (color_id == TH_NODE) {
		float col[3];
		UI_GetThemeColorShade3fv(color_id, -20, col);
		glColor4f(col[0], col[1], col[2], 1.0f);
	}
	else
		UI_ThemeColor(color_id);
	
	if (node->flag & NODE_MUTED)
		UI_ThemeColorBlend(color_id, TH_REDALERT, 0.5f);
	

#ifdef WITH_COMPOSITOR
	if (ntree->type == NTREE_COMPOSIT && (snode->flag & SNODE_SHOW_HIGHLIGHT)) {
		if (COM_isHighlightedbNode(node)) {
			UI_ThemeColorBlend(color_id, TH_ACTIVE, 0.5f);
		}
	}
#endif

	UI_draw_roundbox_corner_set(UI_CNR_TOP_LEFT | UI_CNR_TOP_RIGHT);
	UI_draw_roundbox(rct->xmin, rct->ymax - NODE_DY, rct->xmax, rct->ymax, BASIS_RAD);
	
	/* show/hide icons */
	iconofs = rct->xmax - 0.35f * U.widget_unit;
	
	/* preview */
	if (node->typeinfo->flag & NODE_PREVIEW) {
		uiBut *but;
		iconofs -= iconbutw;
		UI_block_emboss_set(node->block, UI_EMBOSS_NONE);
		but = uiDefIconBut(node->block, UI_BTYPE_BUT_TOGGLE, B_REDR, ICON_MATERIAL,
		                   iconofs, rct->ymax - NODE_DY, iconbutw, UI_UNIT_Y, NULL, 0, 0, 0, 0, "");
		UI_but_func_set(but, node_toggle_button_cb, node, (void *)"NODE_OT_preview_toggle");
		/* XXX this does not work when node is activated and the operator called right afterwards,
		 * since active ID is not updated yet (needs to process the notifier).
		 * This can only work as visual indicator!
		 */
//		if (!(node->flag & (NODE_ACTIVE_ID|NODE_DO_OUTPUT)))
//			UI_but_flag_enable(but, UI_BUT_DISABLED);
		UI_block_emboss_set(node->block, UI_EMBOSS);
	}
	/* group edit */
	if (node->type == NODE_GROUP) {
		uiBut *but;
		iconofs -= iconbutw;
		UI_block_emboss_set(node->block, UI_EMBOSS_NONE);
		but = uiDefIconBut(node->block, UI_BTYPE_BUT_TOGGLE, B_REDR, ICON_NODETREE,
		                   iconofs, rct->ymax - NODE_DY, iconbutw, UI_UNIT_Y, NULL, 0, 0, 0, 0, "");
		UI_but_func_set(but, node_toggle_button_cb, node, (void *)"NODE_OT_group_edit");
		UI_block_emboss_set(node->block, UI_EMBOSS);
	}
	
	/* title */
	if (node->flag & SELECT) 
		UI_ThemeColor(TH_SELECT);
	else
		UI_ThemeColorBlendShade(TH_TEXT, color_id, 0.4f, 10);
	
	/* open/close entirely? */
	{
		uiBut *but;
		int but_size = UI_UNIT_X * 0.6f;
		/* XXX button uses a custom triangle draw below, so make it invisible without icon */
		UI_block_emboss_set(node->block, UI_EMBOSS_NONE);
		but = uiDefBut(node->block, UI_BTYPE_BUT_TOGGLE, B_REDR, "",
		               rct->xmin + 0.5f * U.widget_unit - but_size / 2, rct->ymax - NODE_DY / 2.0f - but_size / 2,
		               but_size, but_size, NULL, 0, 0, 0, 0, "");
		UI_but_func_set(but, node_toggle_button_cb, node, (void *)"NODE_OT_hide_toggle");
		UI_block_emboss_set(node->block, UI_EMBOSS);
		
		/* custom draw function for this button */
		UI_draw_icon_tri(rct->xmin + 0.5f * U.widget_unit, rct->ymax - NODE_DY / 2.0f, 'v');
	}
	
	/* this isn't doing anything for the label, so commenting out */
#if 0
	if (node->flag & SELECT) 
		UI_ThemeColor(TH_TEXT_HI);
	else
		UI_ThemeColor(TH_TEXT);
#endif
	
	nodeLabel(ntree, node, showname, sizeof(showname));
	
	//if (node->flag & NODE_MUTED)
	//	BLI_snprintf(showname, sizeof(showname), "[%s]", showname); /* XXX - don't print into self! */
	
	uiDefBut(node->block, UI_BTYPE_LABEL, 0, showname,
	         (int)(rct->xmin + (NODE_MARGIN_X)), (int)(rct->ymax - NODE_DY),
	         (short)(iconofs - rct->xmin - 18.0f), (short)NODE_DY,
	         NULL, 0, 0, 0, 0, "");

	/* body */
	if (!nodeIsRegistered(node))
		UI_ThemeColor4(TH_REDALERT);	/* use warning color to indicate undefined types */
	else if (node->flag & NODE_CUSTOM_COLOR)
		glColor3fv(node->color);
	else
		UI_ThemeColor4(TH_NODE);
	glEnable(GL_BLEND);
	UI_draw_roundbox_corner_set(UI_CNR_BOTTOM_LEFT | UI_CNR_BOTTOM_RIGHT);
	UI_draw_roundbox(rct->xmin, rct->ymin, rct->xmax, rct->ymax - NODE_DY, BASIS_RAD);
	glDisable(GL_BLEND);

	/* outline active and selected emphasis */
	if (node->flag & SELECT) {
		
		glEnable(GL_BLEND);
		glEnable(GL_LINE_SMOOTH);
		
		if (node->flag & NODE_ACTIVE)
			UI_ThemeColorShadeAlpha(TH_ACTIVE, 0, -40);
		else
			UI_ThemeColorShadeAlpha(TH_SELECT, 0, -40);
		
		UI_draw_roundbox_corner_set(UI_CNR_ALL);
		UI_draw_roundbox_gl_mode(GL_LINE_LOOP, rct->xmin, rct->ymin, rct->xmax, rct->ymax, BASIS_RAD);
		
		glDisable(GL_LINE_SMOOTH);
		glDisable(GL_BLEND);
	}
	
	/* disable lines */
	if (node->flag & NODE_MUTED)
		node_draw_mute_line(v2d, snode, node);

	
	/* socket inputs, buttons */
	for (sock = node->inputs.first; sock; sock = sock->next) {
		if (nodeSocketIsHidden(sock))
			continue;
		
		node_socket_circle_draw(C, ntree, node, sock, NODE_SOCKSIZE, sock->flag & SELECT);
	}
	
	/* socket outputs */
	for (sock = node->outputs.first; sock; sock = sock->next) {
		if (nodeSocketIsHidden(sock))
			continue;
		
		node_socket_circle_draw(C, ntree, node, sock, NODE_SOCKSIZE, sock->flag & SELECT);
	}
	
	/* preview */
	if (node->flag & NODE_PREVIEW && previews) {
		bNodePreview *preview = BKE_node_instance_hash_lookup(previews, key);
		if (preview && (preview->xsize && preview->ysize)) {
			if (preview->rect && !BLI_rctf_is_empty(&node->prvr)) {
				node_draw_preview(preview, &node->prvr);
			}
		}
	}
	
	UI_ThemeClearColor(color_id);
		
	UI_block_end(C, node->block);
	UI_block_draw(C, node->block);
	node->block = NULL;
}

static void node_draw_hidden(const bContext *C, ARegion *ar, SpaceNode *snode, bNodeTree *ntree, bNode *node, bNodeInstanceKey UNUSED(key))
{
	bNodeSocket *sock;
	rctf *rct = &node->totr;
	float dx, centy = BLI_rctf_cent_y(rct);
	float hiddenrad = BLI_rctf_size_y(rct) / 2.0f;
	float socket_size = NODE_SOCKSIZE;
	int color_id = node_get_colorid(node);
	char showname[128]; /* 128 is used below */
	
	/* shadow */
	node_draw_shadow(snode, node, hiddenrad, 1.0f);

	/* body */
	UI_ThemeColor(color_id);
	if (node->flag & NODE_MUTED)
		UI_ThemeColorBlend(color_id, TH_REDALERT, 0.5f);

#ifdef WITH_COMPOSITOR
	if (ntree->type == NTREE_COMPOSIT && (snode->flag & SNODE_SHOW_HIGHLIGHT)) {
		if (COM_isHighlightedbNode(node)) {
			UI_ThemeColorBlend(color_id, TH_ACTIVE, 0.5f);
		}
	}
#else
	(void)ntree;
#endif
	
	UI_draw_roundbox(rct->xmin, rct->ymin, rct->xmax, rct->ymax, hiddenrad);
	
	/* outline active and selected emphasis */
	if (node->flag & SELECT) {
		glEnable(GL_BLEND);
		glEnable(GL_LINE_SMOOTH);
		
		if (node->flag & NODE_ACTIVE)
			UI_ThemeColorShadeAlpha(TH_ACTIVE, 0, -40);
		else
			UI_ThemeColorShadeAlpha(TH_SELECT, 0, -40);
		UI_draw_roundbox_gl_mode(GL_LINE_LOOP, rct->xmin, rct->ymin, rct->xmax, rct->ymax, hiddenrad);
		
		glDisable(GL_LINE_SMOOTH);
		glDisable(GL_BLEND);
	}

	/* custom color inline */
	if (node->flag & NODE_CUSTOM_COLOR) {
		glEnable(GL_BLEND);
		glEnable(GL_LINE_SMOOTH);

		glColor3fv(node->color);
		UI_draw_roundbox_gl_mode(GL_LINE_LOOP, rct->xmin + 1, rct->ymin + 1, rct->xmax -1, rct->ymax - 1, hiddenrad);

		glDisable(GL_LINE_SMOOTH);
		glDisable(GL_BLEND);
	}

	/* title */
	if (node->flag & SELECT) 
		UI_ThemeColor(TH_SELECT);
	else
		UI_ThemeColorBlendShade(TH_TEXT, color_id, 0.4f, 10);
	
	/* open entirely icon */
	{
		uiBut *but;
		int but_size = UI_UNIT_X * 0.6f;
		/* XXX button uses a custom triangle draw below, so make it invisible without icon */
		UI_block_emboss_set(node->block, UI_EMBOSS_NONE);
		but = uiDefBut(node->block, UI_BTYPE_BUT_TOGGLE, B_REDR, "",
		               rct->xmin + 10.0f - but_size / 2, centy - but_size / 2,
		               but_size, but_size, NULL, 0, 0, 0, 0, "");
		UI_but_func_set(but, node_toggle_button_cb, node, (void *)"NODE_OT_hide_toggle");
		UI_block_emboss_set(node->block, UI_EMBOSS);
		
		/* custom draw function for this button */
		UI_draw_icon_tri(rct->xmin + 10.0f, centy, 'h');
	}
	
	/* disable lines */
	if (node->flag & NODE_MUTED)
		node_draw_mute_line(&ar->v2d, snode, node);
	
	if (node->flag & SELECT) 
		UI_ThemeColor(TH_SELECT);
	else
		UI_ThemeColor(TH_TEXT);
	
	if (node->miniwidth > 0.0f) {
		nodeLabel(ntree, node, showname, sizeof(showname));

		//if (node->flag & NODE_MUTED)
		//	BLI_snprintf(showname, sizeof(showname), "[%s]", showname); /* XXX - don't print into self! */

		uiDefBut(node->block, UI_BTYPE_LABEL, 0, showname,
		         (int)(rct->xmin + (NODE_MARGIN_X)), (int)(centy - 10),
		         (short)(BLI_rctf_size_x(rct) - 18.0f - 12.0f), (short)NODE_DY,
		         NULL, 0, 0, 0, 0, "");
	}

	/* scale widget thing */
	UI_ThemeColorShade(color_id, -10);
	dx = 10.0f;
	fdrawline(rct->xmax - dx, centy - 4.0f, rct->xmax - dx, centy + 4.0f);
	fdrawline(rct->xmax - dx - 3.0f * snode->aspect, centy - 4.0f, rct->xmax - dx - 3.0f * snode->aspect, centy + 4.0f);
	
	UI_ThemeColorShade(color_id, +30);
	dx -= snode->aspect;
	fdrawline(rct->xmax - dx, centy - 4.0f, rct->xmax - dx, centy + 4.0f);
	fdrawline(rct->xmax - dx - 3.0f * snode->aspect, centy - 4.0f, rct->xmax - dx - 3.0f * snode->aspect, centy + 4.0f);

	/* sockets */
	for (sock = node->inputs.first; sock; sock = sock->next) {
		if (!nodeSocketIsHidden(sock))
			node_socket_circle_draw(C, ntree, node, sock, socket_size, sock->flag & SELECT);
	}
	
	for (sock = node->outputs.first; sock; sock = sock->next) {
		if (!nodeSocketIsHidden(sock))
			node_socket_circle_draw(C, ntree, node, sock, socket_size, sock->flag & SELECT);
	}
	
	UI_block_end(C, node->block);
	UI_block_draw(C, node->block);
	node->block = NULL;
}

int node_get_resize_cursor(int directions)
{
	if (directions == 0)
		return CURSOR_STD;
	else if ((directions & ~(NODE_RESIZE_TOP | NODE_RESIZE_BOTTOM)) == 0)
		return CURSOR_Y_MOVE;
	else if ((directions & ~(NODE_RESIZE_RIGHT | NODE_RESIZE_LEFT)) == 0)
		return CURSOR_X_MOVE;
	else
		return CURSOR_EDIT;
}

void node_set_cursor(wmWindow *win, SpaceNode *snode, float cursor[2])
{
	bNodeTree *ntree = snode->edittree;
	bNode *node;
	bNodeSocket *sock;
	int wmcursor = CURSOR_STD;
	
	if (ntree) {
		if (node_find_indicated_socket(snode, &node, &sock, cursor, SOCK_IN | SOCK_OUT)) {
			/* pass */
		}
		else {
			/* check nodes front to back */
			for (node = ntree->nodes.last; node; node = node->prev) {
				if (BLI_rctf_isect_pt(&node->totr, cursor[0], cursor[1]))
					break;  /* first hit on node stops */
			}
			if (node) {
				int dir = node->typeinfo->resize_area_func(node, cursor[0], cursor[1]);
				wmcursor = node_get_resize_cursor(dir);
			}
		}
	}
	
	WM_cursor_set(win, wmcursor);
}

void node_draw_default(const bContext *C, ARegion *ar, SpaceNode *snode, bNodeTree *ntree, bNode *node, bNodeInstanceKey key)
{
	if (node->flag & NODE_HIDDEN)
		node_draw_hidden(C, ar, snode, ntree, node, key);
	else
		node_draw_basis(C, ar, snode, ntree, node, key);
}

static void node_update(const bContext *C, bNodeTree *ntree, bNode *node)
{
	if (node->typeinfo->draw_nodetype_prepare)
		node->typeinfo->draw_nodetype_prepare(C, ntree, node);
}

void node_update_nodetree(const bContext *C, bNodeTree *ntree)
{
	bNode *node;
	
	/* update nodes front to back, so children sizes get updated before parents */
	for (node = ntree->nodes.last; node; node = node->prev) {
		node_update(C, ntree, node);
	}
}

static void node_draw(const bContext *C, ARegion *ar, SpaceNode *snode, bNodeTree *ntree, bNode *node, bNodeInstanceKey key)
{
	if (node->typeinfo->draw_nodetype)
		node->typeinfo->draw_nodetype(C, ar, snode, ntree, node, key);
}

#define USE_DRAW_TOT_UPDATE

void node_draw_nodetree(const bContext *C, ARegion *ar, SpaceNode *snode, bNodeTree *ntree, bNodeInstanceKey parent_key)
{
	bNode *node;
	bNodeLink *link;
	int a;
	
	if (ntree == NULL) return;      /* groups... */

#ifdef USE_DRAW_TOT_UPDATE
	if (ntree->nodes.first) {
		BLI_rctf_init_minmax(&ar->v2d.tot);
	}
#endif

	/* draw background nodes, last nodes in front */
	for (a = 0, node = ntree->nodes.first; node; node = node->next, a++) {
		bNodeInstanceKey key;

#ifdef USE_DRAW_TOT_UPDATE
		/* unrelated to background nodes, update the v2d->tot,
		 * can be anywhere before we draw the scroll bars */
		BLI_rctf_union(&ar->v2d.tot, &node->totr);
#endif

		if (!(node->flag & NODE_BACKGROUND))
			continue;

		key = BKE_node_instance_key(parent_key, ntree, node);
		node->nr = a;        /* index of node in list, used for exec event code */
		node_draw(C, ar, snode, ntree, node, key);
	}
	
	/* node lines */
	glEnable(GL_BLEND);
	glEnable(GL_LINE_SMOOTH);
	for (link = ntree->links.first; link; link = link->next) {
		if (!nodeLinkIsHidden(link))
			node_draw_link(&ar->v2d, snode, link);
	}
	glDisable(GL_LINE_SMOOTH);
	glDisable(GL_BLEND);
	
	/* draw foreground nodes, last nodes in front */
	for (a = 0, node = ntree->nodes.first; node; node = node->next, a++) {
		bNodeInstanceKey key;
		if (node->flag & NODE_BACKGROUND)
			continue;

		key = BKE_node_instance_key(parent_key, ntree, node);
		node->nr = a;        /* index of node in list, used for exec event code */
		node_draw(C, ar, snode, ntree, node, key);
	}
}

/* draw tree path info in lower left corner */
static void draw_tree_path(SpaceNode *snode)
{
	char info[256];
	
	ED_node_tree_path_get_fixedbuf(snode, info, sizeof(info));
	
	UI_ThemeColor(TH_TEXT_HI);
	BLF_draw_default(1.5f * UI_UNIT_X, 1.5f * UI_UNIT_Y, 0.0f, info, sizeof(info));
}

static void snode_setup_v2d(SpaceNode *snode, ARegion *ar, const float center[2])
{
	View2D *v2d = &ar->v2d;
	
	/* shift view to node tree center */
	UI_view2d_center_set(v2d, center[0], center[1]);
	UI_view2d_view_ortho(v2d);
	
	/* aspect+font, set each time */
	snode->aspect = BLI_rctf_size_x(&v2d->cur) / (float)ar->winx;
	// XXX snode->curfont = uiSetCurFont_ext(snode->aspect);
}

static void draw_nodetree(const bContext *C, ARegion *ar, bNodeTree *ntree, bNodeInstanceKey parent_key)
{
	SpaceNode *snode = CTX_wm_space_node(C);
	
	node_uiblocks_init(C, ntree);
	
#ifdef WITH_COMPOSITOR
	if (ntree->type == NTREE_COMPOSIT) {
		COM_startReadHighlights();
	}
#endif
	
	node_update_nodetree(C, ntree);
	node_draw_nodetree(C, ar, snode, ntree, parent_key);
}

/* shade the parent node group and add a uiBlock to clip mouse events */
static void draw_group_overlay(const bContext *C, ARegion *ar)
{
	View2D *v2d = &ar->v2d;
	rctf rect = v2d->cur;
	uiBlock *block;
	
	/* shade node groups to separate them visually */
	UI_ThemeColorShadeAlpha(TH_NODE_GROUP, 0, -70);
	glEnable(GL_BLEND);
	UI_draw_roundbox_corner_set(UI_CNR_NONE);
	UI_draw_roundbox_gl_mode(GL_POLYGON, rect.xmin, rect.ymin, rect.xmax, rect.ymax, 0);
	glDisable(GL_BLEND);
	
	/* set the block bounds to clip mouse events from underlying nodes */
	block = UI_block_begin(C, ar, "node tree bounds block", UI_EMBOSS);
	UI_block_bounds_set_explicit(block, rect.xmin, rect.ymin, rect.xmax, rect.ymax);
	UI_block_flag_enable(block, UI_BLOCK_CLIP_EVENTS);
	UI_block_end(C, block);
}

void drawnodespace(const bContext *C, ARegion *ar)
{
	wmWindow *win = CTX_wm_window(C);
	View2DScrollers *scrollers;
	SpaceNode *snode = CTX_wm_space_node(C);
	View2D *v2d = &ar->v2d;

	UI_ThemeClearColor(TH_BACK);
	glClear(GL_COLOR_BUFFER_BIT);

	UI_view2d_view_ortho(v2d);
	
	/* XXX snode->cursor set in coordspace for placing new nodes, used for drawing noodles too */
	UI_view2d_region_to_view(&ar->v2d, win->eventstate->x - ar->winrct.xmin, win->eventstate->y - ar->winrct.ymin,
	                         &snode->cursor[0], &snode->cursor[1]);
	snode->cursor[0] /= UI_DPI_FAC;
	snode->cursor[1] /= UI_DPI_FAC;
	
	ED_region_draw_cb_draw(C, ar, REGION_DRAW_PRE_VIEW);

	/* only set once */
	glBlendFunc(GL_SRC_ALPHA, GL_ONE_MINUS_SRC_ALPHA);
	glEnable(GL_MAP1_VERTEX_3);
	
	/* nodes */
	snode_set_context(C);
	
	/* draw parent node trees */
	if (snode->treepath.last) {
		static const int max_depth = 2;
		bNodeTreePath *path;
		int depth, curdepth;
		float center[2];
		bNodeTree *ntree;
		bNodeLinkDrag *nldrag;
		LinkData *linkdata;
		
		path = snode->treepath.last;
		
		/* current View2D center, will be set temporarily for parent node trees */
		UI_view2d_center_get(v2d, &center[0], &center[1]);
		
		/* store new view center in path and current edittree */
		copy_v2_v2(path->view_center, center);
		if (snode->edittree)
			copy_v2_v2(snode->edittree->view_center, center);
		
		depth = 0;
		while (path->prev && depth < max_depth) {
			path = path->prev;
			++depth;
		}
		
		/* parent node trees in the background */
		for (curdepth = depth; curdepth > 0; path = path->next, --curdepth) {
			ntree = path->nodetree;
			if (ntree) {
				snode_setup_v2d(snode, ar, path->view_center);
				
				draw_nodetree(C, ar, ntree, path->parent_key);
				
				draw_group_overlay(C, ar);
			}
		}
		
		/* top-level edit tree */
		ntree = path->nodetree;
		if (ntree) {
			snode_setup_v2d(snode, ar, center);
			
			/* grid, uses theme color based on node path depth */
			UI_view2d_multi_grid_draw(v2d, (depth > 0 ? TH_NODE_GROUP : TH_BACK), ED_node_grid_size(), NODE_GRID_STEPS, 2);
			
			/* backdrop */
			draw_nodespace_back_pix(C, ar, snode, path->parent_key);
			
			draw_nodetree(C, ar, ntree, path->parent_key);
		}
		
		/* temporary links */
		glEnable(GL_BLEND);
		glEnable(GL_LINE_SMOOTH);
		for (nldrag = snode->linkdrag.first; nldrag; nldrag = nldrag->next) {
			for (linkdata = nldrag->links.first; linkdata; linkdata = linkdata->next)
				node_draw_link(v2d, snode, (bNodeLink *)linkdata->data);
		}
		glDisable(GL_LINE_SMOOTH);
		glDisable(GL_BLEND);
		
		if (snode->flag & SNODE_SHOW_GPENCIL) {
			/* draw grease-pencil ('canvas' strokes) */
			ED_gpencil_draw_view2d(C, true);
		}
	}
	else {
		/* default grid */
		UI_view2d_multi_grid_draw(v2d, TH_BACK, ED_node_grid_size(), NODE_GRID_STEPS, 2);
		
		/* backdrop */
		draw_nodespace_back_pix(C, ar, snode, NODE_INSTANCE_KEY_NONE);
	}
	
	ED_region_draw_cb_draw(C, ar, REGION_DRAW_POST_VIEW);
	
	/* reset view matrix */
	UI_view2d_view_restore(C);
	
	if (snode->treepath.last) {
		if (snode->flag & SNODE_SHOW_GPENCIL) {
			/* draw grease-pencil (screen strokes, and also paintbuffer) */
			ED_gpencil_draw_view2d(C, false);
		}
	}

	/* tree path info */
	draw_tree_path(snode);
	
	/* scrollers */
	scrollers = UI_view2d_scrollers_calc(C, v2d, 10, V2D_GRID_CLAMP, V2D_ARG_DUMMY, V2D_ARG_DUMMY);
	UI_view2d_scrollers_draw(C, v2d, scrollers);
	UI_view2d_scrollers_free(scrollers);
}
