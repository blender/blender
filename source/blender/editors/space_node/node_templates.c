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
 * Contributor(s): Blender Foundation 2009.
 *
 * ***** END GPL LICENSE BLOCK *****
 */

/** \file blender/editors/space_node/node_templates.c
 *  \ingroup edinterface
 */

#include <stdlib.h>
#include <stddef.h>
#include <string.h>

#include "MEM_guardedalloc.h"

#include "DNA_node_types.h"
#include "DNA_scene_types.h"
#include "DNA_screen_types.h"

#include "BLI_listbase.h"
#include "BLI_string.h"
#include "BLI_utildefines.h"

#include "BLF_translation.h"

#include "BKE_context.h"
#include "BKE_depsgraph.h"
#include "BKE_library.h"
#include "BKE_main.h"
#include "BKE_node.h"
#include "BKE_scene.h"

#include "RNA_access.h"

#include "NOD_socket.h"

#include "WM_api.h"
#include "WM_types.h"

#include "UI_interface.h"
#include "UI_resources.h"
#include "../interface/interface_intern.h"

#include "ED_node.h"
#include "ED_util.h"

/************************* Node Socket Manipulation **************************/

static void node_tag_recursive(bNode *node)
{
	bNodeSocket *input;

	if (!node || (node->flag & NODE_TEST))
		return; /* in case of cycles */
	
	node->flag |= NODE_TEST;

	for (input=node->inputs.first; input; input=input->next)
		if (input->link)
			node_tag_recursive(input->link->fromnode);
}

static void node_clear_recursive(bNode *node)
{
	bNodeSocket *input;

	if (!node || !(node->flag & NODE_TEST))
		return; /* in case of cycles */
	
	node->flag &= ~NODE_TEST;

	for (input=node->inputs.first; input; input=input->next)
		if (input->link)
			node_clear_recursive(input->link->fromnode);
}

static void node_remove_linked(bNodeTree *ntree, bNode *rem_node)
{
	bNode *node, *next;
	bNodeSocket *sock;

	if (!rem_node)
		return;

	/* tag linked nodes to be removed */
	for (node=ntree->nodes.first; node; node=node->next)
		node->flag &= ~NODE_TEST;
	
	node_tag_recursive(rem_node);

	/* clear tags on nodes that are still used by other nodes */
	for (node=ntree->nodes.first; node; node=node->next)
		if (!(node->flag & NODE_TEST))
			for (sock=node->inputs.first; sock; sock=sock->next)
				if (sock->link && sock->link->fromnode != rem_node)
					node_clear_recursive(sock->link->fromnode);

	/* remove nodes */
	for (node=ntree->nodes.first; node; node=next) {
		next = node->next;

		if (node->flag & NODE_TEST) {
			if (node->id)
				node->id->us--;
			nodeFreeNode(ntree, node);
		}
	}
}

/* disconnect socket from the node it is connected to */
static void node_socket_disconnect(Main *bmain, bNodeTree *ntree, bNode *node_to, bNodeSocket *sock_to)
{
	if (!sock_to->link)
		return;

	nodeRemLink(ntree, sock_to->link);

	nodeUpdate(ntree, node_to);
	ntreeUpdateTree(ntree);

	ED_node_generic_update(bmain, ntree, node_to);
}

/* remove all nodes connected to this socket, if they aren't connected to other nodes */
static void node_socket_remove(Main *bmain, bNodeTree *ntree, bNode *node_to, bNodeSocket *sock_to)
{
	if (!sock_to->link)
		return;

	node_remove_linked(ntree, sock_to->link->fromnode);

	nodeUpdate(ntree, node_to);
	ntreeUpdateTree(ntree);

	ED_node_generic_update(bmain, ntree, node_to);
}

/* add new node connected to this socket, or replace an existing one */
static void node_socket_add_replace(Main *bmain, bNodeTree *ntree, bNode *node_to, bNodeSocket *sock_to, bNodeTemplate *ntemp, int sock_num)
{
	bNode *node_from;
	bNodeSocket *sock_from;
	bNode *node_prev = NULL;

	/* unlink existing node */
	if (sock_to->link) {
		node_prev = sock_to->link->fromnode;
		nodeRemLink(ntree, sock_to->link);
	}

	/* find existing node that we can use */
	for (node_from=ntree->nodes.first; node_from; node_from=node_from->next)
		if (node_from->type == ntemp->type)
			break;

	if (node_from)
		if (!(node_from->inputs.first == NULL && !(node_from->typeinfo->flag & NODE_OPTIONS)))
			node_from = NULL;

	if (node_prev && node_prev->type == ntemp->type &&
	    (ntemp->type != NODE_GROUP || node_prev->id == &ntemp->ngroup->id))
	{
		/* keep the previous node if it's the same type */
		node_from = node_prev;
	}
	else if (!node_from) {
		node_from= nodeAddNode(ntree, ntemp);
		node_from->locx = node_to->locx - (node_from->typeinfo->width + 50);
		node_from->locy = node_to->locy;

		if (node_from->id)
			id_us_plus(node_from->id);
	}

	nodeSetActive(ntree, node_from);

	/* add link */
	sock_from = BLI_findlink(&node_from->outputs, sock_num);
	nodeAddLink(ntree, node_from, sock_from, node_to, sock_to);

	/* copy input sockets from previous node */
	if (node_prev && node_from != node_prev) {
		bNodeSocket *sock_prev, *sock_from;

		for (sock_prev=node_prev->inputs.first; sock_prev; sock_prev=sock_prev->next) {
			for (sock_from=node_from->inputs.first; sock_from; sock_from=sock_from->next) {
				if (nodeCountSocketLinks(ntree, sock_from) >= sock_from->limit)
					continue;
				
				if (strcmp(sock_prev->name, sock_from->name) == 0 && sock_prev->type == sock_from->type) {
					bNodeLink *link = sock_prev->link;

					if (link && link->fromnode) {
						nodeAddLink(ntree, link->fromnode, link->fromsock, node_from, sock_from);
						nodeRemLink(ntree, link);
					}

					node_socket_free_default_value(sock_from->type, sock_from->default_value);
					sock_from->default_value = node_socket_make_default_value(sock_from->type);
					node_socket_copy_default_value(sock_from->type, sock_from->default_value, sock_prev->default_value);
				}
			}
		}

		/* also preserve mapping for texture nodes */
		if (node_from->typeinfo->nclass == NODE_CLASS_TEXTURE &&
		    node_prev->typeinfo->nclass == NODE_CLASS_TEXTURE)
		{
			memcpy(node_from->storage, node_prev->storage, sizeof(NodeTexBase));
		}

		/* remove node */
		node_remove_linked(ntree, node_prev);
	}

	nodeUpdate(ntree, node_from);
	nodeUpdate(ntree, node_to);
	ntreeUpdateTree(ntree);

	ED_node_generic_update(bmain, ntree, node_to);
}

/****************************** Node Link Menu *******************************/

#define UI_NODE_LINK_ADD		0
#define UI_NODE_LINK_DISCONNECT	-1
#define UI_NODE_LINK_REMOVE		-2

typedef struct NodeLinkArg {
	Main *bmain;
	Scene *scene;
	bNodeTree *ntree;
	bNode *node;
	bNodeSocket *sock;

	bNodeTree *ngroup;
	int type;
	int output;

	uiLayout *layout;
} NodeLinkArg;

static void ui_node_link(bContext *C, void *arg_p, void *event_p)
{
	NodeLinkArg *arg = (NodeLinkArg*)arg_p;
	Main *bmain = arg->bmain;
	bNode *node_to = arg->node;
	bNodeSocket *sock_to = arg->sock;
	bNodeTree *ntree = arg->ntree;
	int event = GET_INT_FROM_POINTER(event_p);
	bNodeTemplate ntemp;

	ntemp.type = arg->type;
	ntemp.ngroup = arg->ngroup;
	ntemp.scene = CTX_data_scene(C);
	ntemp.main = CTX_data_main(C);

	if (event == UI_NODE_LINK_DISCONNECT)
		node_socket_disconnect(bmain, ntree, node_to, sock_to);
	else if (event == UI_NODE_LINK_REMOVE)
		node_socket_remove(bmain, ntree, node_to, sock_to);
	else
		node_socket_add_replace(bmain, ntree, node_to, sock_to, &ntemp, arg->output);
	
	ED_undo_push(C, "Node input modify");
}

static void ui_node_sock_name(bNodeSocket *sock, char name[UI_MAX_NAME_STR])
{
	if (sock->link && sock->link->fromnode) {
		bNode *node = sock->link->fromnode;
		char node_name[UI_MAX_NAME_STR];

		if (node->type == NODE_GROUP) {
			if (node->id)
				BLI_strncpy(node_name, node->id->name+2, UI_MAX_NAME_STR);
			else
				BLI_strncpy(node_name, N_("Group"), UI_MAX_NAME_STR);
		}
		else
			BLI_strncpy(node_name, node->typeinfo->name, UI_MAX_NAME_STR);

		if (node->inputs.first == NULL &&
		    node->outputs.first != node->outputs.last &&
		    !(node->typeinfo->flag & NODE_OPTIONS))
		{
			BLI_snprintf(name, UI_MAX_NAME_STR, "%s | %s", IFACE_(node_name), IFACE_(sock->link->fromsock->name));
		}
		else {
			BLI_strncpy(name, IFACE_(node_name), UI_MAX_NAME_STR);
		}
	}
	else if (sock->type == SOCK_SHADER)
		BLI_strncpy(name, IFACE_("None"), UI_MAX_NAME_STR);
	else
		BLI_strncpy(name, IFACE_("Default"), UI_MAX_NAME_STR);
}

static int ui_compatible_sockets(int typeA, int typeB)
{
	return (typeA == typeB);
}

static void ui_node_menu_column(NodeLinkArg *arg, int nclass, const char *cname)
{
	Main *bmain = arg->bmain;
	bNodeTree *ntree = arg->ntree;
	bNodeSocket *sock = arg->sock;
	uiLayout *layout = arg->layout;
	uiLayout *column = NULL;
	uiBlock *block = uiLayoutGetBlock(layout);
	uiBut *but;
	bNodeType *ntype;
	bNodeTree *ngroup;
	NodeLinkArg *argN;
	int first = 1;
	int compatibility= 0;

	if (ntree->type == NTREE_SHADER) {
		if (BKE_scene_use_new_shading_nodes(arg->scene))
			compatibility= NODE_NEW_SHADING;
		else
			compatibility= NODE_OLD_SHADING;
	}

	if (nclass == NODE_CLASS_GROUP) {
		for (ngroup=bmain->nodetree.first; ngroup; ngroup=ngroup->id.next) {
			bNodeSocket *gsock;
			char name[UI_MAX_NAME_STR];
			int i, j, num = 0;

			if (ngroup->type != ntree->type)
				continue;

			for (gsock=ngroup->inputs.first; gsock; gsock=gsock->next)
				if (ui_compatible_sockets(gsock->type, sock->type))
					num++;

			for (i=0, j=0, gsock=ngroup->outputs.first; gsock; gsock=gsock->next, i++) {
				if (!ui_compatible_sockets(gsock->type, sock->type))
					continue;

				if (first) {
					column = uiLayoutColumn(layout, FALSE);
					uiBlockSetCurLayout(block, column);

					uiItemL(column, IFACE_(cname), ICON_NODE);
					but= block->buttons.last;
					but->flag= UI_TEXT_LEFT;

					first = 0;
				}

				if (num > 1) {
					if (j == 0) {
						uiItemL(column, ngroup->id.name+2, ICON_NODE);
						but= block->buttons.last;
						but->flag= UI_TEXT_LEFT;
					}

					BLI_snprintf(name, UI_MAX_NAME_STR, "  %s", gsock->name);
					j++;
				}
				else
					BLI_strncpy(name, ngroup->id.name+2, UI_MAX_NAME_STR);

				but = uiDefBut(block, BUT, 0, ngroup->id.name+2, 0, 0, UI_UNIT_X*4, UI_UNIT_Y,
					NULL, 0.0, 0.0, 0.0, 0.0, TIP_("Add node to input"));

				argN = MEM_dupallocN(arg);
				argN->type = NODE_GROUP;
				argN->ngroup = ngroup;
				argN->output = i;
				uiButSetNFunc(but, ui_node_link, argN, NULL);
			}
		}
	}
	else {
		bNodeTreeType *ttype= ntreeGetType(ntree->type);

		for (ntype=ttype->node_types.first; ntype; ntype=ntype->next) {
			bNodeSocketTemplate *stemp;
			char name[UI_MAX_NAME_STR];
			int i, j, num = 0;

			if (compatibility && !(ntype->compatibility & compatibility))
				continue;

			if (ntype->nclass != nclass)
				continue;

			for (i=0, stemp=ntype->outputs; stemp && stemp->type != -1; stemp++, i++)
				if (ui_compatible_sockets(stemp->type, sock->type))
					num++;

			for (i=0, j=0, stemp=ntype->outputs; stemp && stemp->type != -1; stemp++, i++) {
				if (!ui_compatible_sockets(stemp->type, sock->type))
					continue;

				if (first) {
					column = uiLayoutColumn(layout, FALSE);
					uiBlockSetCurLayout(block, column);

					uiItemL(column, IFACE_(cname), ICON_NODE);
					but= block->buttons.last;
					but->flag= UI_TEXT_LEFT;

					first = 0;
				}

				if (num > 1) {
					if (j == 0) {
						uiItemL(column, IFACE_(ntype->name), ICON_NODE);
						but= block->buttons.last;
						but->flag= UI_TEXT_LEFT;
					}

					BLI_snprintf(name, UI_MAX_NAME_STR, "  %s", IFACE_(stemp->name));
					j++;
				}
				else
					BLI_strncpy(name, IFACE_(ntype->name), UI_MAX_NAME_STR);

				but = uiDefBut(block, BUT, 0, name, 0, 0, UI_UNIT_X*4, UI_UNIT_Y,
					NULL, 0.0, 0.0, 0.0, 0.0, TIP_("Add node to input"));

				argN = MEM_dupallocN(arg);
				argN->type = ntype->type;
				argN->output = i;
				uiButSetNFunc(but, ui_node_link, argN, NULL);
			}
		}
	}
}

static void node_menu_column_foreach_cb(void *calldata, int nclass, const char *name)
{
	NodeLinkArg *arg = (NodeLinkArg*)calldata;

	if (!ELEM(nclass, NODE_CLASS_GROUP, NODE_CLASS_LAYOUT))
		ui_node_menu_column(arg, nclass, name);
}

static void ui_template_node_link_menu(bContext *C, uiLayout *layout, void *but_p)
{
	Main *bmain= CTX_data_main(C);
	Scene *scene= CTX_data_scene(C);
	uiBlock *block = uiLayoutGetBlock(layout);
	uiBut *but = (uiBut*)but_p;
	uiLayout *split, *column;
	NodeLinkArg *arg = (NodeLinkArg*)but->func_argN;
	bNodeSocket *sock = arg->sock;
	bNodeTreeType *ntreetype= ntreeGetType(arg->ntree->type);

	uiBlockSetCurLayout(block, layout);
	split = uiLayoutSplit(layout, 0.0f, FALSE);

	arg->bmain= bmain;
	arg->scene= scene;
	arg->layout= split;
	
	if (ntreetype && ntreetype->foreach_nodeclass)
		ntreetype->foreach_nodeclass(scene, arg, node_menu_column_foreach_cb);

	column = uiLayoutColumn(split, FALSE);
	uiBlockSetCurLayout(block, column);

	if (sock->link) {
		uiItemL(column, IFACE_("Link"), ICON_NONE);
		but= block->buttons.last;
		but->flag= UI_TEXT_LEFT;

		but = uiDefBut(block, BUT, 0, IFACE_("Remove"), 0, 0, UI_UNIT_X*4, UI_UNIT_Y,
			NULL, 0.0, 0.0, 0.0, 0.0, TIP_("Remove nodes connected to the input"));
		uiButSetNFunc(but, ui_node_link, MEM_dupallocN(arg), SET_INT_IN_POINTER(UI_NODE_LINK_REMOVE));

		but = uiDefBut(block, BUT, 0, IFACE_("Disconnect"), 0, 0, UI_UNIT_X*4, UI_UNIT_Y,
			NULL, 0.0, 0.0, 0.0, 0.0, TIP_("Disconnect nodes connected to the input"));
		uiButSetNFunc(but, ui_node_link, MEM_dupallocN(arg), SET_INT_IN_POINTER(UI_NODE_LINK_DISCONNECT));
	}

	ui_node_menu_column(arg, NODE_CLASS_GROUP, N_("Group"));
}

void uiTemplateNodeLink(uiLayout *layout, bNodeTree *ntree, bNode *node, bNodeSocket *sock)
{
	uiBlock *block = uiLayoutGetBlock(layout);
	NodeLinkArg *arg;
	uiBut *but;

	arg = MEM_callocN(sizeof(NodeLinkArg), "NodeLinkArg");
	arg->ntree = ntree;
	arg->node = node;
	arg->sock = sock;
	arg->type = 0;
	arg->output = 0;

	uiBlockSetCurLayout(block, layout);

	if (sock->link || sock->type == SOCK_SHADER || (sock->flag & SOCK_HIDE_VALUE)) {
		char name[UI_MAX_NAME_STR];
		ui_node_sock_name(sock, name);
		but = uiDefMenuBut(block, ui_template_node_link_menu, NULL, name, 0, 0, UI_UNIT_X*4, UI_UNIT_Y, "");
	}
	else
		but = uiDefIconMenuBut(block, ui_template_node_link_menu, NULL, ICON_NONE, 0, 0, UI_UNIT_X, UI_UNIT_Y, "");

	but->type= MENU;
	but->flag |= UI_TEXT_LEFT|UI_BUT_NODE_LINK;
	but->poin= (char*)but;
	but->func_argN = arg;

	if (sock->link && sock->link->fromnode)
		if (sock->link->fromnode->flag & NODE_ACTIVE_TEXTURE)
			but->flag |= UI_BUT_NODE_ACTIVE;
}

/**************************** Node Tree Layout *******************************/

static void ui_node_draw_input(uiLayout *layout, bContext *C,
	bNodeTree *ntree, bNode *node, bNodeSocket *input, int depth);

static void ui_node_draw_node(uiLayout *layout, bContext *C, bNodeTree *ntree, bNode *node, int depth)
{
	bNodeSocket *input;
	uiLayout *col, *split;
	PointerRNA nodeptr;

	RNA_pointer_create(&ntree->id, &RNA_Node, node, &nodeptr);

	if (node->typeinfo->uifunc) {
		if (node->type != NODE_GROUP) {
			split = uiLayoutSplit(layout, 0.35f, FALSE);
			col = uiLayoutColumn(split, FALSE);
			col = uiLayoutColumn(split, FALSE);

			node->typeinfo->uifunc(col, C, &nodeptr);
		}
	}

	for (input=node->inputs.first; input; input=input->next)
		ui_node_draw_input(layout, C, ntree, node, input, depth+1);
}

static void ui_node_draw_input(uiLayout *layout, bContext *C, bNodeTree *ntree, bNode *node, bNodeSocket *input, int depth)
{
	PointerRNA inputptr;
	uiBlock *block = uiLayoutGetBlock(layout);
	uiBut *bt;
	uiLayout *split, *row, *col;
	bNode *lnode;
	char label[UI_MAX_NAME_STR];
	int indent = (depth > 1)? 2*(depth - 1): 0;
	int dependency_loop;

	if (input->flag & SOCK_UNAVAIL)
		return;

	/* to avoid eternal loops on cyclic dependencies */
	node->flag |= NODE_TEST;
	lnode = (input->link)? input->link->fromnode: NULL;

	dependency_loop = (lnode && (lnode->flag & NODE_TEST));
	if (dependency_loop)
		lnode = NULL;

	/* socket RNA pointer */
	RNA_pointer_create(&ntree->id, &RNA_NodeSocket, input, &inputptr);

	/* indented label */
	memset(label, ' ', indent);
	label[indent] = '\0';
	BLI_snprintf(label, UI_MAX_NAME_STR, "%s%s:", label, IFACE_(input->name));

	/* split in label and value */
	split = uiLayoutSplit(layout, 0.35f, FALSE);

	row = uiLayoutRow(split, TRUE);

	if (depth > 0) {
		uiBlockSetEmboss(block, UI_EMBOSSN);

		if (lnode && (lnode->inputs.first || (lnode->typeinfo->uifunc && lnode->type != NODE_GROUP))) {
			int icon = (input->flag & SOCK_COLLAPSED)? ICON_DISCLOSURE_TRI_RIGHT: ICON_DISCLOSURE_TRI_DOWN;
			uiItemR(row, &inputptr, "show_expanded", UI_ITEM_R_ICON_ONLY, "", icon);
		}
		else
			uiItemL(row, "", ICON_BLANK1);

		bt = block->buttons.last;
		bt->x2 = UI_UNIT_X/2;

		uiBlockSetEmboss(block, UI_EMBOSS);
	}

	uiItemL(row, label, ICON_NONE);
	bt= block->buttons.last;
	bt->flag= UI_TEXT_LEFT;

	if (dependency_loop) {
		row = uiLayoutRow(split, FALSE);
		uiItemL(row, IFACE_("Dependency Loop"), ICON_ERROR);
	}
	else if (lnode) {
		/* input linked to a node */
		uiTemplateNodeLink(split, ntree, node, input);

		if (!(input->flag & SOCK_COLLAPSED)) {
			if (depth == 0)
				uiItemS(layout);

			ui_node_draw_node(layout, C, ntree, lnode, depth);
		}
	}
	else {
		/* input not linked, show value */
		if (input->type != SOCK_SHADER && !(input->flag & SOCK_HIDE_VALUE)) {
			if (input->type == SOCK_VECTOR) {
				row = uiLayoutRow(split, FALSE);
				col = uiLayoutColumn(row, FALSE);

				uiItemR(col, &inputptr, "default_value", 0, "", 0);
			}
			else {
				row = uiLayoutRow(split, TRUE);
				uiItemR(row, &inputptr, "default_value", 0, "", 0);
			}
		}
		else
			row = uiLayoutRow(split, FALSE);

		uiTemplateNodeLink(row, ntree, node, input);
	}

	/* clear */
	node->flag &= ~NODE_TEST;
}

void uiTemplateNodeView(uiLayout *layout, bContext *C, bNodeTree *ntree, bNode *node, bNodeSocket *input)
{
	bNode *tnode;

	if (!ntree)
		return;

	/* clear for cycle check */
	for (tnode=ntree->nodes.first; tnode; tnode=tnode->next)
		tnode->flag &= ~NODE_TEST;

	if (input)
		ui_node_draw_input(layout, C, ntree, node, input, 0);
	else
		ui_node_draw_node(layout, C, ntree, node, 0);
}

