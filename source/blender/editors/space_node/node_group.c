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
 * The Original Code is Copyright (C) 2005 Blender Foundation.
 * All rights reserved.
 *
 * The Original Code is: all of this file.
 *
 * Contributor(s): David Millan Escriva, Juho Vepsäläinen, Nathan Letwory
 *
 * ***** END GPL LICENSE BLOCK *****
 */

/** \file blender/editors/space_node/node_group.c
 *  \ingroup spnode
 */

#include "MEM_guardedalloc.h"

#include "DNA_node_types.h"
#include "DNA_object_types.h"
#include "DNA_anim_types.h"

#include "BLI_blenlib.h"

#include "BKE_action.h"
#include "BKE_animsys.h"
#include "BKE_context.h"
#include "BKE_global.h"
#include "BKE_library.h"
#include "BKE_main.h"
#include "BKE_report.h"

#include "ED_node.h"  /* own include */
#include "ED_screen.h"
#include "ED_render.h"

#include "RNA_access.h"
#include "RNA_define.h"
#include "RNA_enum_types.h"

#include "WM_api.h"
#include "WM_types.h"

#include "UI_resources.h"

#include "node_intern.h"  /* own include */
#include "NOD_socket.h"

static EnumPropertyItem socket_in_out_items[] = {
	{ SOCK_IN, "SOCK_IN", 0, "Input", "" },
	{ SOCK_OUT, "SOCK_OUT", 0, "Output", "" },
	{ 0, NULL, 0, NULL, NULL },
};

/* ***************** Edit Group operator ************* */

void snode_make_group_editable(SpaceNode *snode, bNode *gnode)
{
	bNode *node;

	/* make sure nothing has group editing on */
	for (node = snode->nodetree->nodes.first; node; node = node->next) {
		nodeGroupEditClear(node);

		/* while we're here, clear texture active */
		if (node->typeinfo->nclass == NODE_CLASS_TEXTURE) {
			/* this is not 100% sure to be reliable, see comment on the flag */
			node->flag &= ~NODE_ACTIVE_TEXTURE;
		}
	}

	if (gnode == NULL) {
		/* with NULL argument we do a toggle */
		if (snode->edittree == snode->nodetree)
			gnode = nodeGetActive(snode->nodetree);
	}

	if (gnode) {
		snode->edittree = nodeGroupEditSet(gnode, 1);

		/* deselect all other nodes, so we can also do grabbing of entire subtree */
		for (node = snode->nodetree->nodes.first; node; node = node->next) {
			node_deselect(node);

			if (node->typeinfo->nclass == NODE_CLASS_TEXTURE) {
				/* this is not 100% sure to be reliable, see comment on the flag */
				node->flag &= ~NODE_ACTIVE_TEXTURE;
			}
		}
		node_select(gnode);
	}
	else
		snode->edittree = snode->nodetree;
}

static int node_group_edit_exec(bContext *C, wmOperator *UNUSED(op))
{
	SpaceNode *snode = CTX_wm_space_node(C);

	ED_preview_kill_jobs(C);

	if (snode->nodetree == snode->edittree) {
		bNode *gnode = nodeGetActive(snode->edittree);
		snode_make_group_editable(snode, gnode);
	}
	else
		snode_make_group_editable(snode, NULL);

	WM_event_add_notifier(C, NC_SCENE | ND_NODES, NULL);

	return OPERATOR_FINISHED;
}

static int node_group_edit_invoke(bContext *C, wmOperator *op, wmEvent *UNUSED(event))
{
	SpaceNode *snode = CTX_wm_space_node(C);
	bNode *gnode;

	/* XXX callback? */
	if (snode->nodetree == snode->edittree) {
		gnode = nodeGetActive(snode->edittree);
		if (gnode && gnode->id && GS(gnode->id->name) == ID_NT && gnode->id->lib) {
			uiPupMenuOkee(C, op->type->idname, "Make group local?");
			return OPERATOR_CANCELLED;
		}
	}

	return node_group_edit_exec(C, op);
}

void NODE_OT_group_edit(wmOperatorType *ot)
{
	/* identifiers */
	ot->name = "Edit Group";
	ot->description = "Edit node group";
	ot->idname = "NODE_OT_group_edit";

	/* api callbacks */
	ot->invoke = node_group_edit_invoke;
	ot->exec = node_group_edit_exec;
	ot->poll = ED_operator_node_active;

	/* flags */
	ot->flag = OPTYPE_REGISTER | OPTYPE_UNDO;
}

/* ***************** Add Group Socket operator ************* */

static int node_group_socket_add_exec(bContext *C, wmOperator *op)
{
	SpaceNode *snode = CTX_wm_space_node(C);
	int in_out = -1;
	char name[MAX_NAME] = "";
	int type = SOCK_FLOAT;
	bNodeTree *ngroup = snode->edittree;
	/* bNodeSocket *sock; */ /* UNUSED */

	ED_preview_kill_jobs(C);

	if (RNA_struct_property_is_set(op->ptr, "name"))
		RNA_string_get(op->ptr, "name", name);

	if (RNA_struct_property_is_set(op->ptr, "type"))
		type = RNA_enum_get(op->ptr, "type");

	if (RNA_struct_property_is_set(op->ptr, "in_out"))
		in_out = RNA_enum_get(op->ptr, "in_out");
	else
		return OPERATOR_CANCELLED;

	/* using placeholder subtype first */
	/* sock = */ /* UNUSED */ node_group_add_socket(ngroup, name, type, in_out);

	ntreeUpdateTree(ngroup);

	snode_notify(C, snode);

	return OPERATOR_FINISHED;
}

void NODE_OT_group_socket_add(wmOperatorType *ot)
{
	/* identifiers */
	ot->name = "Add Group Socket";
	ot->description = "Add node group socket";
	ot->idname = "NODE_OT_group_socket_add";

	/* api callbacks */
	ot->exec = node_group_socket_add_exec;
	ot->poll = ED_operator_node_active;

	/* flags */
	ot->flag = OPTYPE_REGISTER | OPTYPE_UNDO;

	RNA_def_enum(ot->srna, "in_out", socket_in_out_items, SOCK_IN, "Socket Type", "Input or Output");
	RNA_def_string(ot->srna, "name", "", MAX_NAME, "Name", "Group socket name");
	RNA_def_enum(ot->srna, "type", node_socket_type_items, SOCK_FLOAT, "Type", "Type of the group socket");
}

/* ***************** Remove Group Socket operator ************* */

static int node_group_socket_remove_exec(bContext *C, wmOperator *op)
{
	SpaceNode *snode = CTX_wm_space_node(C);
	int index = -1;
	int in_out = -1;
	bNodeTree *ngroup = snode->edittree;
	bNodeSocket *sock;

	ED_preview_kill_jobs(C);

	if (RNA_struct_property_is_set(op->ptr, "index"))
		index = RNA_int_get(op->ptr, "index");
	else
		return OPERATOR_CANCELLED;

	if (RNA_struct_property_is_set(op->ptr, "in_out"))
		in_out = RNA_enum_get(op->ptr, "in_out");
	else
		return OPERATOR_CANCELLED;

	sock = (bNodeSocket *)BLI_findlink(in_out == SOCK_IN ? &ngroup->inputs : &ngroup->outputs, index);
	if (sock) {
		node_group_remove_socket(ngroup, sock, in_out);
		ntreeUpdateTree(ngroup);

		snode_notify(C, snode);
	}

	return OPERATOR_FINISHED;
}

void NODE_OT_group_socket_remove(wmOperatorType *ot)
{
	/* identifiers */
	ot->name = "Remove Group Socket";
	ot->description = "Remove a node group socket";
	ot->idname = "NODE_OT_group_socket_remove";

	/* api callbacks */
	ot->exec = node_group_socket_remove_exec;
	ot->poll = ED_operator_node_active;

	/* flags */
	ot->flag = OPTYPE_REGISTER | OPTYPE_UNDO;

	RNA_def_int(ot->srna, "index", 0, 0, INT_MAX, "Index", "", 0, INT_MAX);
	RNA_def_enum(ot->srna, "in_out", socket_in_out_items, SOCK_IN, "Socket Type", "Input or Output");
}

/* ***************** Move Group Socket Up operator ************* */

static int node_group_socket_move_up_exec(bContext *C, wmOperator *op)
{
	SpaceNode *snode = CTX_wm_space_node(C);
	int index = -1;
	int in_out = -1;
	bNodeTree *ngroup = snode->edittree;
	bNodeSocket *sock, *prev;

	ED_preview_kill_jobs(C);

	if (RNA_struct_property_is_set(op->ptr, "index"))
		index = RNA_int_get(op->ptr, "index");
	else
		return OPERATOR_CANCELLED;

	if (RNA_struct_property_is_set(op->ptr, "in_out"))
		in_out = RNA_enum_get(op->ptr, "in_out");
	else
		return OPERATOR_CANCELLED;

	/* swap */
	if (in_out == SOCK_IN) {
		sock = (bNodeSocket *)BLI_findlink(&ngroup->inputs, index);
		prev = sock->prev;
		/* can't move up the first socket */
		if (!prev)
			return OPERATOR_CANCELLED;
		BLI_remlink(&ngroup->inputs, sock);
		BLI_insertlinkbefore(&ngroup->inputs, prev, sock);

		ngroup->update |= NTREE_UPDATE_GROUP_IN;
	}
	else if (in_out == SOCK_OUT) {
		sock = (bNodeSocket *)BLI_findlink(&ngroup->outputs, index);
		prev = sock->prev;
		/* can't move up the first socket */
		if (!prev)
			return OPERATOR_CANCELLED;
		BLI_remlink(&ngroup->outputs, sock);
		BLI_insertlinkbefore(&ngroup->outputs, prev, sock);

		ngroup->update |= NTREE_UPDATE_GROUP_OUT;
	}
	ntreeUpdateTree(ngroup);

	snode_notify(C, snode);

	return OPERATOR_FINISHED;
}

void NODE_OT_group_socket_move_up(wmOperatorType *ot)
{
	/* identifiers */
	ot->name = "Move Group Socket Up";
	ot->description = "Move up node group socket";
	ot->idname = "NODE_OT_group_socket_move_up";

	/* api callbacks */
	ot->exec = node_group_socket_move_up_exec;
	ot->poll = ED_operator_node_active;

	/* flags */
	ot->flag = OPTYPE_REGISTER | OPTYPE_UNDO;

	RNA_def_int(ot->srna, "index", 0, 0, INT_MAX, "Index", "", 0, INT_MAX);
	RNA_def_enum(ot->srna, "in_out", socket_in_out_items, SOCK_IN, "Socket Type", "Input or Output");
}

/* ***************** Move Group Socket Up operator ************* */

static int node_group_socket_move_down_exec(bContext *C, wmOperator *op)
{
	SpaceNode *snode = CTX_wm_space_node(C);
	int index = -1;
	int in_out = -1;
	bNodeTree *ngroup = snode->edittree;
	bNodeSocket *sock, *next;

	ED_preview_kill_jobs(C);

	if (RNA_struct_property_is_set(op->ptr, "index"))
		index = RNA_int_get(op->ptr, "index");
	else
		return OPERATOR_CANCELLED;

	if (RNA_struct_property_is_set(op->ptr, "in_out"))
		in_out = RNA_enum_get(op->ptr, "in_out");
	else
		return OPERATOR_CANCELLED;

	/* swap */
	if (in_out == SOCK_IN) {
		sock = (bNodeSocket *)BLI_findlink(&ngroup->inputs, index);
		next = sock->next;
		/* can't move down the last socket */
		if (!next)
			return OPERATOR_CANCELLED;
		BLI_remlink(&ngroup->inputs, sock);
		BLI_insertlinkafter(&ngroup->inputs, next, sock);

		ngroup->update |= NTREE_UPDATE_GROUP_IN;
	}
	else if (in_out == SOCK_OUT) {
		sock = (bNodeSocket *)BLI_findlink(&ngroup->outputs, index);
		next = sock->next;
		/* can't move down the last socket */
		if (!next)
			return OPERATOR_CANCELLED;
		BLI_remlink(&ngroup->outputs, sock);
		BLI_insertlinkafter(&ngroup->outputs, next, sock);

		ngroup->update |= NTREE_UPDATE_GROUP_OUT;
	}
	ntreeUpdateTree(ngroup);

	snode_notify(C, snode);

	return OPERATOR_FINISHED;
}

void NODE_OT_group_socket_move_down(wmOperatorType *ot)
{
	/* identifiers */
	ot->name = "Move Group Socket Down";
	ot->description = "Move down node group socket";
	ot->idname = "NODE_OT_group_socket_move_down";

	/* api callbacks */
	ot->exec = node_group_socket_move_down_exec;
	ot->poll = ED_operator_node_active;

	/* flags */
	ot->flag = OPTYPE_REGISTER | OPTYPE_UNDO;

	RNA_def_int(ot->srna, "index", 0, 0, INT_MAX, "Index", "", 0, INT_MAX);
	RNA_def_enum(ot->srna, "in_out", socket_in_out_items, SOCK_IN, "Socket Type", "Input or Output");
}

/* ******************** Ungroup operator ********************** */

/* returns 1 if its OK */
static int node_group_ungroup(bNodeTree *ntree, bNode *gnode)
{
	bNodeLink *link, *linkn;
	bNode *node, *nextn;
	bNodeTree *ngroup, *wgroup;
	ListBase anim_basepaths = {NULL, NULL};

	ngroup = (bNodeTree *)gnode->id;
	if (ngroup == NULL) return 0;

	/* clear new pointers, set in copytree */
	for (node = ntree->nodes.first; node; node = node->next)
		node->new_node = NULL;

	/* wgroup is a temporary copy of the NodeTree we're merging in
	 * - all of wgroup's nodes are transferred across to their new home
	 * - ngroup (i.e. the source NodeTree) is left unscathed
	 * - temp copy. don't change ID usercount
	 */
	wgroup = ntreeCopyTree_ex(ngroup, FALSE);

	/* add the nodes into the ntree */
	for (node = wgroup->nodes.first; node; node = nextn) {
		nextn = node->next;

		/* keep track of this node's RNA "base" path (the part of the path identifying the node)
		 * if the old nodetree has animation data which potentially covers this node
		 */
		if (wgroup->adt) {
			PointerRNA ptr;
			char *path;

			RNA_pointer_create(&wgroup->id, &RNA_Node, node, &ptr);
			path = RNA_path_from_ID_to_struct(&ptr);

			if (path)
				BLI_addtail(&anim_basepaths, BLI_genericNodeN(path));
		}

		/* migrate node */
		BLI_remlink(&wgroup->nodes, node);
		BLI_addtail(&ntree->nodes, node);

		/* ensure unique node name in the nodee tree */
		nodeUniqueName(ntree, node);

		node->locx += gnode->locx;
		node->locy += gnode->locy;

		node->flag |= NODE_SELECT;
	}

	/* restore external links to and from the gnode */
	for (link = ntree->links.first; link; link = link->next) {
		if (link->fromnode == gnode) {
			if (link->fromsock->groupsock) {
				bNodeSocket *gsock = link->fromsock->groupsock;
				if (gsock->link) {
					if (gsock->link->fromnode) {
						/* NB: using the new internal copies here! the groupsock pointer still maps to the old tree */
						link->fromnode = (gsock->link->fromnode ? gsock->link->fromnode->new_node : NULL);
						link->fromsock = gsock->link->fromsock->new_sock;
					}
					else {
						/* group output directly maps to group input */
						bNodeSocket *insock = node_group_find_input(gnode, gsock->link->fromsock);
						if (insock->link) {
							link->fromnode = insock->link->fromnode;
							link->fromsock = insock->link->fromsock;
						}
					}
				}
				else {
					/* copy the default input value from the group socket default to the external socket */
					node_socket_convert_default_value(link->tosock->type, link->tosock->default_value, gsock->type, gsock->default_value);
				}
			}
		}
	}
	/* remove internal output links, these are not used anymore */
	for (link = wgroup->links.first; link; link = linkn) {
		linkn = link->next;
		if (!link->tonode)
			nodeRemLink(wgroup, link);
	}
	/* restore links from internal nodes */
	for (link = wgroup->links.first; link; link = linkn) {
		linkn = link->next;
		/* indicates link to group input */
		if (!link->fromnode) {
			/* NB: can't use find_group_node_input here,
			 * because gnode sockets still point to the old tree!
			 */
			bNodeSocket *insock;
			for (insock = gnode->inputs.first; insock; insock = insock->next)
				if (insock->groupsock->new_sock == link->fromsock)
					break;
			if (insock->link) {
				link->fromnode = insock->link->fromnode;
				link->fromsock = insock->link->fromsock;
			}
			else {
				/* copy the default input value from the group node socket default to the internal socket */
				node_socket_convert_default_value(link->tosock->type, link->tosock->default_value, insock->type, insock->default_value);
				nodeRemLink(wgroup, link);
			}
		}
	}

	/* add internal links to the ntree */
	for (link = wgroup->links.first; link; link = linkn) {
		linkn = link->next;
		BLI_remlink(&wgroup->links, link);
		BLI_addtail(&ntree->links, link);
	}

	/* and copy across the animation,
	 * note that the animation data's action can be NULL here */
	if (wgroup->adt) {
		LinkData *ld, *ldn = NULL;
		bAction *waction;

		/* firstly, wgroup needs to temporary dummy action that can be destroyed, as it shares copies */
		waction = wgroup->adt->action = BKE_action_copy(wgroup->adt->action);

		/* now perform the moving */
		BKE_animdata_separate_by_basepath(&wgroup->id, &ntree->id, &anim_basepaths);

		/* paths + their wrappers need to be freed */
		for (ld = anim_basepaths.first; ld; ld = ldn) {
			ldn = ld->next;

			MEM_freeN(ld->data);
			BLI_freelinkN(&anim_basepaths, ld);
		}

		/* free temp action too */
		if (waction) {
			BKE_libblock_free(&G.main->action, waction);
		}
	}

	/* delete the group instance. this also removes old input links! */
	nodeFreeNode(ntree, gnode);

	/* free the group tree (takes care of user count) */
	BKE_libblock_free(&G.main->nodetree, wgroup);

	ntree->update |= NTREE_UPDATE_NODES | NTREE_UPDATE_LINKS;

	return 1;
}

static int node_group_ungroup_exec(bContext *C, wmOperator *op)
{
	SpaceNode *snode = CTX_wm_space_node(C);
	bNode *gnode;

	ED_preview_kill_jobs(C);

	/* are we inside of a group? */
	gnode = node_tree_get_editgroup(snode->nodetree);
	if (gnode)
		snode_make_group_editable(snode, NULL);

	gnode = nodeGetActive(snode->edittree);
	if (gnode == NULL)
		return OPERATOR_CANCELLED;

	if (gnode->type != NODE_GROUP) {
		BKE_report(op->reports, RPT_WARNING, "Not a group");
		return OPERATOR_CANCELLED;
	}
	else if (node_group_ungroup(snode->nodetree, gnode)) {
		ntreeUpdateTree(snode->nodetree);
	}
	else {
		BKE_report(op->reports, RPT_WARNING, "Cannot ungroup");
		return OPERATOR_CANCELLED;
	}

	snode_notify(C, snode);
	snode_dag_update(C, snode);

	return OPERATOR_FINISHED;
}

void NODE_OT_group_ungroup(wmOperatorType *ot)
{
	/* identifiers */
	ot->name = "Ungroup";
	ot->description = "Ungroup selected nodes";
	ot->idname = "NODE_OT_group_ungroup";

	/* api callbacks */
	ot->exec = node_group_ungroup_exec;
	ot->poll = ED_operator_node_active;

	/* flags */
	ot->flag = OPTYPE_REGISTER | OPTYPE_UNDO;
}

/* ******************** Separate operator ********************** */

/* returns 1 if its OK */
static int node_group_separate_selected(bNodeTree *ntree, bNode *gnode, int make_copy)
{
	bNodeLink *link, *link_next;
	bNode *node, *node_next, *newnode;
	bNodeTree *ngroup;
	ListBase anim_basepaths = {NULL, NULL};

	ngroup = (bNodeTree *)gnode->id;
	if (ngroup == NULL) return 0;

	/* deselect all nodes in the target tree */
	for (node = ntree->nodes.first; node; node = node->next)
		node_deselect(node);

	/* clear new pointers, set in nodeCopyNode */
	for (node = ngroup->nodes.first; node; node = node->next)
		node->new_node = NULL;

	/* add selected nodes into the ntree */
	for (node = ngroup->nodes.first; node; node = node_next) {
		node_next = node->next;
		if (node->flag & NODE_SELECT) {
			
			if (make_copy) {
				/* make a copy */
				newnode = nodeCopyNode(ngroup, node);
			}
			else {
				/* use the existing node */
				newnode = node;
			}
			
			/* keep track of this node's RNA "base" path (the part of the path identifying the node)
			 * if the old nodetree has animation data which potentially covers this node
			 */
			if (ngroup->adt) {
				PointerRNA ptr;
				char *path;
				
				RNA_pointer_create(&ngroup->id, &RNA_Node, newnode, &ptr);
				path = RNA_path_from_ID_to_struct(&ptr);
				
				if (path)
					BLI_addtail(&anim_basepaths, BLI_genericNodeN(path));
			}
			
			/* ensure valid parent pointers, detach if parent stays inside the group */
			if (newnode->parent && !(newnode->parent->flag & NODE_SELECT))
				nodeDetachNode(newnode);
			
			/* migrate node */
			BLI_remlink(&ngroup->nodes, newnode);
			BLI_addtail(&ntree->nodes, newnode);
			
			/* ensure unique node name in the node tree */
			nodeUniqueName(ntree, newnode);
			
			newnode->locx += gnode->locx;
			newnode->locy += gnode->locy;
		}
		else {
			/* ensure valid parent pointers, detach if child stays inside the group */
			if (node->parent && (node->parent->flag & NODE_SELECT))
				nodeDetachNode(node);
		}
	}

	/* add internal links to the ntree */
	for (link = ngroup->links.first; link; link = link_next) {
		int fromselect = (link->fromnode && (link->fromnode->flag & NODE_SELECT));
		int toselect = (link->tonode && (link->tonode->flag & NODE_SELECT));
		link_next = link->next;

		if (make_copy) {
			/* make a copy of internal links */
			if (fromselect && toselect)
				nodeAddLink(ntree, link->fromnode->new_node, link->fromsock->new_sock, link->tonode->new_node, link->tosock->new_sock);
		}
		else {
			/* move valid links over, delete broken links */
			if (fromselect && toselect) {
				BLI_remlink(&ngroup->links, link);
				BLI_addtail(&ntree->links, link);
			}
			else if (fromselect || toselect) {
				nodeRemLink(ngroup, link);
			}
		}
	}

	/* and copy across the animation,
	 * note that the animation data's action can be NULL here */
	if (ngroup->adt) {
		LinkData *ld, *ldn = NULL;

		/* now perform the moving */
		BKE_animdata_separate_by_basepath(&ngroup->id, &ntree->id, &anim_basepaths);

		/* paths + their wrappers need to be freed */
		for (ld = anim_basepaths.first; ld; ld = ldn) {
			ldn = ld->next;

			MEM_freeN(ld->data);
			BLI_freelinkN(&anim_basepaths, ld);
		}
	}

	ntree->update |= NTREE_UPDATE_NODES | NTREE_UPDATE_LINKS;
	if (!make_copy)
		ngroup->update |= NTREE_UPDATE_NODES | NTREE_UPDATE_LINKS;

	return 1;
}

typedef enum eNodeGroupSeparateType {
	NODE_GS_COPY,
	NODE_GS_MOVE
} eNodeGroupSeparateType;

/* Operator Property */
EnumPropertyItem node_group_separate_types[] = {
	{NODE_GS_COPY, "COPY", 0, "Copy", "Copy to parent node tree, keep group intact"},
	{NODE_GS_MOVE, "MOVE", 0, "Move", "Move to parent node tree, remove from group"},
	{0, NULL, 0, NULL, NULL}
};

static int node_group_separate_exec(bContext *C, wmOperator *op)
{
	SpaceNode *snode = CTX_wm_space_node(C);
	bNode *gnode;
	int type = RNA_enum_get(op->ptr, "type");

	ED_preview_kill_jobs(C);

	/* are we inside of a group? */
	gnode = node_tree_get_editgroup(snode->nodetree);
	if (!gnode) {
		BKE_report(op->reports, RPT_WARNING, "Not inside node group");
		return OPERATOR_CANCELLED;
	}

	switch (type) {
		case NODE_GS_COPY:
			if (!node_group_separate_selected(snode->nodetree, gnode, 1)) {
				BKE_report(op->reports, RPT_WARNING, "Cannot separate nodes");
				return OPERATOR_CANCELLED;
			}
			break;
		case NODE_GS_MOVE:
			if (!node_group_separate_selected(snode->nodetree, gnode, 0)) {
				BKE_report(op->reports, RPT_WARNING, "Cannot separate nodes");
				return OPERATOR_CANCELLED;
			}
			break;
	}

	/* switch to parent tree */
	snode_make_group_editable(snode, NULL);

	ntreeUpdateTree(snode->nodetree);

	snode_notify(C, snode);
	snode_dag_update(C, snode);

	return OPERATOR_FINISHED;
}

static int node_group_separate_invoke(bContext *C, wmOperator *UNUSED(op), wmEvent *UNUSED(event))
{
	uiPopupMenu *pup = uiPupMenuBegin(C, "Separate", ICON_NONE);
	uiLayout *layout = uiPupMenuLayout(pup);

	uiLayoutSetOperatorContext(layout, WM_OP_EXEC_DEFAULT);
	uiItemEnumO(layout, "NODE_OT_group_separate", NULL, 0, "type", NODE_GS_COPY);
	uiItemEnumO(layout, "NODE_OT_group_separate", NULL, 0, "type", NODE_GS_MOVE);

	uiPupMenuEnd(C, pup);

	return OPERATOR_CANCELLED;
}

void NODE_OT_group_separate(wmOperatorType *ot)
{
	/* identifiers */
	ot->name = "Separate";
	ot->description = "Separate selected nodes from the node group";
	ot->idname = "NODE_OT_group_separate";

	/* api callbacks */
	ot->invoke = node_group_separate_invoke;
	ot->exec = node_group_separate_exec;
	ot->poll = ED_operator_node_active;

	/* flags */
	ot->flag = OPTYPE_REGISTER | OPTYPE_UNDO;

	RNA_def_enum(ot->srna, "type", node_group_separate_types, NODE_GS_COPY, "Type", "");
}

/* ****************** Make Group operator ******************* */

static int node_group_make_test(bNodeTree *ntree, bNode *gnode)
{
	bNode *node;
	bNodeLink *link;
	int totnode = 0;

	/* is there something to group? also do some clearing */
	for (node = ntree->nodes.first; node; node = node->next) {
		if (node == gnode)
			continue;

		if (node->flag & NODE_SELECT) {
			/* no groups in groups */
			if (node->type == NODE_GROUP)
				return 0;
			totnode++;
		}

		node->done = 0;
	}
	if (totnode == 0) return 0;

	/* check if all connections are OK, no unselected node has both
	 * inputs and outputs to a selection */
	for (link = ntree->links.first; link; link = link->next) {
		if (link->fromnode && link->tonode && link->fromnode->flag & NODE_SELECT && link->fromnode != gnode)
			link->tonode->done |= 1;
		if (link->fromnode && link->tonode && link->tonode->flag & NODE_SELECT && link->tonode != gnode)
			link->fromnode->done |= 2;
	}

	for (node = ntree->nodes.first; node; node = node->next) {
		if (node == gnode)
			continue;
		if ((node->flag & NODE_SELECT) == 0)
			if (node->done == 3)
				break;
	}
	if (node)
		return 0;

	return 1;
}


static void node_get_selected_minmax(bNodeTree *ntree, bNode *gnode, float *min, float *max)
{
	bNode *node;
	INIT_MINMAX2(min, max);
	for (node = ntree->nodes.first; node; node = node->next) {
		if (node == gnode)
			continue;
		if (node->flag & NODE_SELECT) {
			DO_MINMAX2((&node->locx), min, max);
		}
	}
}

static int node_group_make_insert_selected(bNodeTree *ntree, bNode *gnode)
{
	bNodeTree *ngroup = (bNodeTree *)gnode->id;
	bNodeLink *link, *linkn;
	bNode *node, *nextn;
	bNodeSocket *gsock;
	ListBase anim_basepaths = {NULL, NULL};
	float min[2], max[2];

	/* deselect all nodes in the target tree */
	for (node = ngroup->nodes.first; node; node = node->next)
		node_deselect(node);

	node_get_selected_minmax(ntree, gnode, min, max);

	/* move nodes over */
	for (node = ntree->nodes.first; node; node = nextn) {
		nextn = node->next;
		if (node == gnode)
			continue;
		if (node->flag & NODE_SELECT) {
			/* keep track of this node's RNA "base" path (the part of the pat identifying the node)
			 * if the old nodetree has animation data which potentially covers this node
			 */
			if (ntree->adt) {
				PointerRNA ptr;
				char *path;

				RNA_pointer_create(&ntree->id, &RNA_Node, node, &ptr);
				path = RNA_path_from_ID_to_struct(&ptr);

				if (path)
					BLI_addtail(&anim_basepaths, BLI_genericNodeN(path));
			}

			/* ensure valid parent pointers, detach if parent stays outside the group */
			if (node->parent && !(node->parent->flag & NODE_SELECT))
				nodeDetachNode(node);

			/* change node-collection membership */
			BLI_remlink(&ntree->nodes, node);
			BLI_addtail(&ngroup->nodes, node);

			/* ensure unique node name in the ngroup */
			nodeUniqueName(ngroup, node);

			node->locx -= 0.5f * (min[0] + max[0]);
			node->locy -= 0.5f * (min[1] + max[1]);
		}
		else {
			/* if the parent is to be inserted but not the child, detach properly */
			if (node->parent && (node->parent->flag & NODE_SELECT))
				nodeDetachNode(node);
		}
	}

	/* move animation data over */
	if (ntree->adt) {
		LinkData *ld, *ldn = NULL;

		BKE_animdata_separate_by_basepath(&ntree->id, &ngroup->id, &anim_basepaths);

		/* paths + their wrappers need to be freed */
		for (ld = anim_basepaths.first; ld; ld = ldn) {
			ldn = ld->next;

			MEM_freeN(ld->data);
			BLI_freelinkN(&anim_basepaths, ld);
		}
	}

	/* node groups don't use internal cached data */
	ntreeFreeCache(ngroup);

	/* relink external sockets */
	for (link = ntree->links.first; link; link = linkn) {
		int fromselect = (link->fromnode && (link->fromnode->flag & NODE_SELECT) && link->fromnode != gnode);
		int toselect = (link->tonode && (link->tonode->flag & NODE_SELECT) && link->tonode != gnode);
		linkn = link->next;

		if ((fromselect && link->tonode == gnode) || (toselect && link->fromnode == gnode)) {
			/* remove all links to/from the gnode.
			 * this can remove link information, but there's no general way to preserve it.
			 */
			nodeRemLink(ntree, link);
		}
		else if (fromselect && toselect) {
			BLI_remlink(&ntree->links, link);
			BLI_addtail(&ngroup->links, link);
		}
		else if (toselect) {
			gsock = node_group_expose_socket(ngroup, link->tosock, SOCK_IN);
			link->tosock->link = nodeAddLink(ngroup, NULL, gsock, link->tonode, link->tosock);
			link->tosock = node_group_add_extern_socket(ntree, &gnode->inputs, SOCK_IN, gsock);
			link->tonode = gnode;
		}
		else if (fromselect) {
			/* search for existing group node socket */
			for (gsock = ngroup->outputs.first; gsock; gsock = gsock->next)
				if (gsock->link && gsock->link->fromsock == link->fromsock)
					break;
			if (!gsock) {
				gsock = node_group_expose_socket(ngroup, link->fromsock, SOCK_OUT);
				gsock->link = nodeAddLink(ngroup, link->fromnode, link->fromsock, NULL, gsock);
				link->fromsock = node_group_add_extern_socket(ntree, &gnode->outputs, SOCK_OUT, gsock);
			}
			else
				link->fromsock = node_group_find_output(gnode, gsock);
			link->fromnode = gnode;
		}
	}

	/* update of the group tree */
	ngroup->update |= NTREE_UPDATE;
	/* update of the tree containing the group instance node */
	ntree->update |= NTREE_UPDATE_NODES | NTREE_UPDATE_LINKS;

	return 1;
}

static bNode *node_group_make_from_selected(bNodeTree *ntree)
{
	bNode *gnode;
	bNodeTree *ngroup;
	float min[2], max[2];
	bNodeTemplate ntemp;

	node_get_selected_minmax(ntree, NULL, min, max);

	/* new nodetree */
	ngroup = ntreeAddTree("NodeGroup", ntree->type, NODE_GROUP);

	/* make group node */
	ntemp.type = NODE_GROUP;
	ntemp.ngroup = ngroup;
	gnode = nodeAddNode(ntree, &ntemp);
	gnode->locx = 0.5f * (min[0] + max[0]);
	gnode->locy = 0.5f * (min[1] + max[1]);

	node_group_make_insert_selected(ntree, gnode);

	/* update of the tree containing the group instance node */
	ntree->update |= NTREE_UPDATE_NODES;

	return gnode;
}

typedef enum eNodeGroupMakeType {
	NODE_GM_NEW,
	NODE_GM_INSERT
} eNodeGroupMakeType;

/* Operator Property */
EnumPropertyItem node_group_make_types[] = {
	{NODE_GM_NEW, "NEW", 0, "New", "Create a new node group from selected nodes"},
	{NODE_GM_INSERT, "INSERT", 0, "Insert", "Insert into active node group"},
	{0, NULL, 0, NULL, NULL}
};

static int node_group_make_exec(bContext *C, wmOperator *op)
{
	SpaceNode *snode = CTX_wm_space_node(C);
	bNode *gnode;
	int type = RNA_enum_get(op->ptr, "type");

	if (snode->edittree != snode->nodetree) {
		BKE_report(op->reports, RPT_WARNING, "Cannot add a new group in a group");
		return OPERATOR_CANCELLED;
	}

	/* for time being... is too complex to handle */
	if (snode->treetype == NTREE_COMPOSIT) {
		for (gnode = snode->nodetree->nodes.first; gnode; gnode = gnode->next) {
			if (gnode->flag & SELECT)
				if (gnode->type == CMP_NODE_R_LAYERS)
					break;
		}

		if (gnode) {
			BKE_report(op->reports, RPT_WARNING, "Cannot add a Render Layers node in a group");
			return OPERATOR_CANCELLED;
		}
	}

	ED_preview_kill_jobs(C);

	switch (type) {
		case NODE_GM_NEW:
			if (node_group_make_test(snode->nodetree, NULL)) {
				gnode = node_group_make_from_selected(snode->nodetree);
			}
			else {
				BKE_report(op->reports, RPT_WARNING, "Cannot make group");
				return OPERATOR_CANCELLED;
			}
			break;
		case NODE_GM_INSERT:
			gnode = nodeGetActive(snode->nodetree);
			if (!gnode || gnode->type != NODE_GROUP) {
				BKE_report(op->reports, RPT_WARNING, "No active group node");
				return OPERATOR_CANCELLED;
			}
			if (node_group_make_test(snode->nodetree, gnode)) {
				node_group_make_insert_selected(snode->nodetree, gnode);
			}
			else {
				BKE_report(op->reports, RPT_WARNING, "Cannot insert into group");
				return OPERATOR_CANCELLED;
			}
			break;
	}

	if (gnode) {
		nodeSetActive(snode->nodetree, gnode);
		snode_make_group_editable(snode, gnode);
	}

	if (gnode)
		ntreeUpdateTree((bNodeTree *)gnode->id);
	ntreeUpdateTree(snode->nodetree);

	snode_notify(C, snode);
	snode_dag_update(C, snode);

	return OPERATOR_FINISHED;
}

static int node_group_make_invoke(bContext *C, wmOperator *UNUSED(op), wmEvent *UNUSED(event))
{
	SpaceNode *snode = CTX_wm_space_node(C);
	bNode *act = nodeGetActive(snode->edittree);
	uiPopupMenu *pup = uiPupMenuBegin(C, "Make Group", ICON_NONE);
	uiLayout *layout = uiPupMenuLayout(pup);

	uiLayoutSetOperatorContext(layout, WM_OP_EXEC_DEFAULT);
	uiItemEnumO(layout, "NODE_OT_group_make", NULL, 0, "type", NODE_GM_NEW);

	/* if active node is a group, add insert option */
	if (act && act->type == NODE_GROUP) {
		uiItemEnumO(layout, "NODE_OT_group_make", NULL, 0, "type", NODE_GM_INSERT);
	}

	uiPupMenuEnd(C, pup);

	return OPERATOR_CANCELLED;
}

void NODE_OT_group_make(wmOperatorType *ot)
{
	/* identifiers */
	ot->name = "Group";
	ot->description = "Make group from selected nodes";
	ot->idname = "NODE_OT_group_make";

	/* api callbacks */
	ot->invoke = node_group_make_invoke;
	ot->exec = node_group_make_exec;
	ot->poll = ED_operator_node_active;

	/* flags */
	ot->flag = OPTYPE_REGISTER | OPTYPE_UNDO;

	RNA_def_enum(ot->srna, "type", node_group_make_types, NODE_GM_NEW, "Type", "");
}
