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

#include <stdlib.h>

#include "MEM_guardedalloc.h"

#include "DNA_node_types.h"
#include "DNA_anim_types.h"

#include "BLI_listbase.h"
#include "BLI_math.h"

#include "BLF_translation.h"

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
#include "NOD_common.h"
#include "NOD_socket.h"

static int node_group_operator_active(bContext *C)
{
	if (ED_operator_node_active(C)) {
		SpaceNode *snode = CTX_wm_space_node(C);
		
		/* Group operators only defined for standard node tree types.
		 * Disabled otherwise to allow pynodes define their own operators
		 * with same keymap.
		 */
		if (STREQ(snode->tree_idname, "ShaderNodeTree") ||
		    STREQ(snode->tree_idname, "CompositorNodeTree") ||
		    STREQ(snode->tree_idname, "TextureNodeTree"))
		{
			return true;
		}
	}
	return false;
}

static int node_group_operator_editable(bContext *C)
{
	if (ED_operator_node_editable(C)) {
		SpaceNode *snode = CTX_wm_space_node(C);
		
		/* Group operators only defined for standard node tree types.
		 * Disabled otherwise to allow pynodes define their own operators
		 * with same keymap.
		 */
		if (STREQ(snode->tree_idname, "ShaderNodeTree") ||
		    STREQ(snode->tree_idname, "CompositorNodeTree") ||
		    STREQ(snode->tree_idname, "TextureNodeTree"))
		{
			return true;
		}
	}
	return false;
}

static const char *group_ntree_idname(bContext *C)
{
	SpaceNode *snode = CTX_wm_space_node(C);
	return snode->tree_idname;
}

static const char *group_node_idname(bContext *C)
{
	SpaceNode *snode = CTX_wm_space_node(C);
	
	if (STREQ(snode->tree_idname, "ShaderNodeTree"))
		return "ShaderNodeGroup";
	else if (STREQ(snode->tree_idname, "CompositorNodeTree"))
		return "CompositorNodeGroup";
	else if (STREQ(snode->tree_idname, "TextureNodeTree"))
		return "TextureNodeGroup";
	
	return "";
}

static bNode *node_group_get_active(bContext *C, const char *node_idname)
{
	SpaceNode *snode = CTX_wm_space_node(C);
	bNode *node = nodeGetActive(snode->edittree);
	
	if (node && STREQ(node->idname, node_idname))
		return node;
	else
		return NULL;
}

/* ***************** Edit Group operator ************* */

static int node_group_edit_exec(bContext *C, wmOperator *op)
{
	SpaceNode *snode = CTX_wm_space_node(C);
	const char *node_idname = group_node_idname(C);
	bNode *gnode;
	const bool exit = RNA_boolean_get(op->ptr, "exit");
	
	ED_preview_kill_jobs(C);
	
	gnode = node_group_get_active(C, node_idname);
	
	if (gnode && !exit) {
		bNodeTree *ngroup = (bNodeTree *)gnode->id;
		
		if (ngroup)
			ED_node_tree_push(snode, ngroup, gnode);
	}
	else
		ED_node_tree_pop(snode);
	
	WM_event_add_notifier(C, NC_SCENE | ND_NODES, NULL);
	
	return OPERATOR_FINISHED;
}

void NODE_OT_group_edit(wmOperatorType *ot)
{
	/* identifiers */
	ot->name = "Edit Group";
	ot->description = "Edit node group";
	ot->idname = "NODE_OT_group_edit";
	
	/* api callbacks */
	ot->exec = node_group_edit_exec;
	ot->poll = node_group_operator_active;
	
	/* flags */
	ot->flag = OPTYPE_REGISTER | OPTYPE_UNDO;
	
	RNA_def_boolean(ot->srna, "exit", false, "Exit", "");
}

/* ******************** Ungroup operator ********************** */

/* returns 1 if its OK */
static int node_group_ungroup(bNodeTree *ntree, bNode *gnode)
{
	bNodeLink *link, *linkn, *tlink;
	bNode *node, *nextnode;
	bNodeTree *ngroup, *wgroup;
	ListBase anim_basepaths = {NULL, NULL};
	
	ngroup = (bNodeTree *)gnode->id;
	
	/* clear new pointers, set in copytree */
	for (node = ntree->nodes.first; node; node = node->next)
		node->new_node = NULL;
	
	/* wgroup is a temporary copy of the NodeTree we're merging in
	 * - all of wgroup's nodes are transferred across to their new home
	 * - ngroup (i.e. the source NodeTree) is left unscathed
	 * - temp copy. don't change ID usercount
	 */
	wgroup = ntreeCopyTree_ex(ngroup, G.main, false);
	
	/* Add the nodes into the ntree */
	for (node = wgroup->nodes.first; node; node = nextnode) {
		nextnode = node->next;
		
		/* Remove interface nodes.
		 * This also removes remaining links to and from interface nodes.
		 */
		if (ELEM(node->type, NODE_GROUP_INPUT, NODE_GROUP_OUTPUT)) {
			nodeFreeNode(wgroup, node);
			continue;
		}
		
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
		
		/* ensure unique node name in the node tree */
		nodeUniqueName(ntree, node);
		
		if (!node->parent) {
			node->locx += gnode->locx;
			node->locy += gnode->locy;
		}
		
		node->flag |= NODE_SELECT;
	}
	
	/* Add internal links to the ntree */
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
			BKE_libblock_free(G.main, waction);
		}
	}
	
	/* free the group tree (takes care of user count) */
	BKE_libblock_free(G.main, wgroup);
	
	/* restore external links to and from the gnode */
	/* note: the nodes have been copied to intermediate wgroup first (so need to use new_node),
	 *       then transferred to ntree (new_node pointers remain valid).
	 */
	
	/* input links */
	for (link = ngroup->links.first; link; link = link->next) {
		if (link->fromnode->type == NODE_GROUP_INPUT) {
			const char *identifier = link->fromsock->identifier;
			int num_external_links = 0;
			
			/* find external links to this input */
			for (tlink = ntree->links.first; tlink; tlink = tlink->next) {
				if (tlink->tonode == gnode && STREQ(tlink->tosock->identifier, identifier)) {
					nodeAddLink(ntree, tlink->fromnode, tlink->fromsock, link->tonode->new_node, link->tosock->new_sock);
					++num_external_links;
				}
			}
			
			/* if group output is not externally linked,
			 * convert the constant input value to ensure somewhat consistent behavior */
			if (num_external_links == 0) {
				/* XXX TODO bNodeSocket *sock = node_group_find_input_socket(gnode, identifier);
				BLI_assert(sock);*/
				
				/* XXX TODO nodeSocketCopy(ntree, link->tosock->new_sock, link->tonode->new_node, ntree, sock, gnode);*/
			}
		}
	}
	
	/* output links */
	for (link = ntree->links.first; link; link = link->next) {
		if (link->fromnode == gnode) {
			const char *identifier = link->fromsock->identifier;
			int num_internal_links = 0;
			
			/* find internal links to this output */
			for (tlink = ngroup->links.first; tlink; tlink = tlink->next) {
				/* only use active output node */
				if (tlink->tonode->type == NODE_GROUP_OUTPUT && (tlink->tonode->flag & NODE_DO_OUTPUT)) {
					if (STREQ(tlink->tosock->identifier, identifier)) {
						nodeAddLink(ntree, tlink->fromnode->new_node, tlink->fromsock->new_sock, link->tonode, link->tosock);
						++num_internal_links;
					}
				}
			}
			
			/* if group output is not internally linked,
			 * convert the constant output value to ensure somewhat consistent behavior */
			if (num_internal_links == 0) {
				/* XXX TODO bNodeSocket *sock = node_group_find_output_socket(gnode, identifier);
				BLI_assert(sock);*/
				
				/* XXX TODO nodeSocketCopy(ntree, link->tosock, link->tonode, ntree, sock, gnode); */
			}
		}
	}
	
	/* delete the group instance */
	nodeFreeNode(ntree, gnode);
	
	ntree->update |= NTREE_UPDATE_NODES | NTREE_UPDATE_LINKS;
	
	return 1;
}


static int node_group_ungroup_exec(bContext *C, wmOperator *op)
{
	SpaceNode *snode = CTX_wm_space_node(C);
	const char *node_idname = group_node_idname(C);
	bNode *gnode;

	ED_preview_kill_jobs(C);

	gnode = node_group_get_active(C, node_idname);
	if (!gnode)
		return OPERATOR_CANCELLED;
	
	if (gnode->id && node_group_ungroup(snode->edittree, gnode)) {
		ntreeUpdateTree(CTX_data_main(C), snode->nodetree);
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
	ot->poll = node_group_operator_editable;
	
	/* flags */
	ot->flag = OPTYPE_REGISTER | OPTYPE_UNDO;
}

/* ******************** Separate operator ********************** */

/* returns 1 if its OK */
static int node_group_separate_selected(bNodeTree *ntree, bNodeTree *ngroup, float offx, float offy, int make_copy)
{
	bNodeLink *link, *link_next;
	bNode *node, *node_next, *newnode;
	ListBase anim_basepaths = {NULL, NULL};
	
	/* deselect all nodes in the target tree */
	for (node = ntree->nodes.first; node; node = node->next)
		nodeSetSelected(node, false);
	
	/* clear new pointers, set in nodeCopyNode */
	for (node = ngroup->nodes.first; node; node = node->next)
		node->new_node = NULL;
	
	/* add selected nodes into the ntree */
	for (node = ngroup->nodes.first; node; node = node_next) {
		node_next = node->next;
		if (!(node->flag & NODE_SELECT))
			continue;
		
		/* ignore interface nodes */
		if (ELEM(node->type, NODE_GROUP_INPUT, NODE_GROUP_OUTPUT)) {
			nodeSetSelected(node, false);
			continue;
		}
		
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

		if (!newnode->parent) {
			newnode->locx += offx;
			newnode->locy += offy;		
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
static EnumPropertyItem node_group_separate_types[] = {
	{NODE_GS_COPY, "COPY", 0, "Copy", "Copy to parent node tree, keep group intact"},
	{NODE_GS_MOVE, "MOVE", 0, "Move", "Move to parent node tree, remove from group"},
	{0, NULL, 0, NULL, NULL}
};

static int node_group_separate_exec(bContext *C, wmOperator *op)
{
	SpaceNode *snode = CTX_wm_space_node(C);
	bNodeTree *ngroup, *nparent;
	int type = RNA_enum_get(op->ptr, "type");
	float offx, offy;

	ED_preview_kill_jobs(C);

	/* are we inside of a group? */
	ngroup = snode->edittree;
	nparent = ED_node_tree_get(snode, 1);
	if (!nparent) {
		BKE_report(op->reports, RPT_WARNING, "Not inside node group");
		return OPERATOR_CANCELLED;
	}
	/* get node tree offset */
	snode_group_offset(snode, &offx, &offy);
	
	switch (type) {
		case NODE_GS_COPY:
			if (!node_group_separate_selected(nparent, ngroup, offx, offy, 1)) {
				BKE_report(op->reports, RPT_WARNING, "Cannot separate nodes");
				return OPERATOR_CANCELLED;
			}
			break;
		case NODE_GS_MOVE:
			if (!node_group_separate_selected(nparent, ngroup, offx, offy, 0)) {
				BKE_report(op->reports, RPT_WARNING, "Cannot separate nodes");
				return OPERATOR_CANCELLED;
			}
			break;
	}
	
	/* switch to parent tree */
	ED_node_tree_pop(snode);
	
	ntreeUpdateTree(CTX_data_main(C), snode->nodetree);
	
	snode_notify(C, snode);
	snode_dag_update(C, snode);

	return OPERATOR_FINISHED;
}

static int node_group_separate_invoke(bContext *C, wmOperator *UNUSED(op), const wmEvent *UNUSED(event))
{
	uiPopupMenu *pup = uiPupMenuBegin(C, CTX_IFACE_(BLF_I18NCONTEXT_OPERATOR_DEFAULT, "Separate"), ICON_NONE);
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
	ot->poll = node_group_operator_editable;
	
	/* flags */
	ot->flag = OPTYPE_REGISTER | OPTYPE_UNDO;
	
	RNA_def_enum(ot->srna, "type", node_group_separate_types, NODE_GS_COPY, "Type", "");
}

/* ****************** Make Group operator ******************* */

static bool node_group_make_use_node(bNode *node, bNode *gnode)
{
	return (node != gnode &&
	        !ELEM(node->type, NODE_GROUP_INPUT, NODE_GROUP_OUTPUT) &&
	        (node->flag & NODE_SELECT));
}

static bool node_group_make_test_selected(bNodeTree *ntree, bNode *gnode, const char *ntree_idname, struct ReportList *reports)
{
	bNodeTree *ngroup;
	bNode *node;
	bNodeLink *link;
	int ok = true;
	
	/* make a local pseudo node tree to pass to the node poll functions */
	ngroup = ntreeAddTree(NULL, "Pseudo Node Group", ntree_idname);
	
	/* check poll functions for selected nodes */
	for (node = ntree->nodes.first; node; node = node->next) {
		if (node_group_make_use_node(node, gnode)) {
			if (node->typeinfo->poll_instance && !node->typeinfo->poll_instance(node, ngroup)) {
				BKE_reportf(reports, RPT_WARNING, "Can not add node '%s' in a group", node->name);
				ok = false;
				break;
			}
		}

		node->done = 0;
	}
	
	/* free local pseudo node tree again */
	ntreeFreeTree(ngroup);
	MEM_freeN(ngroup);
	if (!ok)
		return false;
	
	/* check if all connections are OK, no unselected node has both
	 * inputs and outputs to a selection */
	for (link = ntree->links.first; link; link = link->next) {
		if (node_group_make_use_node(link->fromnode, gnode))
			link->tonode->done |= 1;
		if (node_group_make_use_node(link->tonode, gnode))
			link->fromnode->done |= 2;
	}
	for (node = ntree->nodes.first; node; node = node->next) {
		if (!(node->flag & NODE_SELECT) &&
		    node != gnode &&
		    node->done == 3)
		{
			return false;
		}
	}
	return true;
}

static int node_get_selected_minmax(bNodeTree *ntree, bNode *gnode, float *min, float *max)
{
	bNode *node;
	float loc[2];
	int totselect = 0;
	
	INIT_MINMAX2(min, max);
	for (node = ntree->nodes.first; node; node = node->next) {
		if (node_group_make_use_node(node, gnode)) {
			nodeToView(node, 0.0f, 0.0f, &loc[0], &loc[1]);
			minmax_v2v2_v2(min, max, loc);
			++totselect;
		}
	}
	
	/* sane min/max if no selected nodes */
	if (totselect == 0) {
		min[0] = min[1] = max[0] = max[1] = 0.0f;
	}
	
	return totselect;
}

static void node_group_make_insert_selected(const bContext *C, bNodeTree *ntree, bNode *gnode)
{
	bNodeTree *ngroup = (bNodeTree *)gnode->id;
	bNodeLink *link, *linkn;
	bNode *node, *nextn;
	bNodeSocket *sock;
	ListBase anim_basepaths = {NULL, NULL};
	float min[2], max[2], center[2];
	int totselect;
	bool expose_all = false;
	bNode *input_node, *output_node;
	
	/* XXX rough guess, not nice but we don't have access to UI constants here ... */
	static const float offsetx = 200;
	static const float offsety = 0.0f;
	
	/* deselect all nodes in the target tree */
	for (node = ngroup->nodes.first; node; node = node->next)
		nodeSetSelected(node, false);
	
	totselect = node_get_selected_minmax(ntree, gnode, min, max);
	add_v2_v2v2(center, min, max);
	mul_v2_fl(center, 0.5f);
	
	/* auto-add interface for "solo" nodes */
	if (totselect == 1)
		expose_all = true;
	
	/* move nodes over */
	for (node = ntree->nodes.first; node; node = nextn) {
		nextn = node->next;
		if (node_group_make_use_node(node, gnode)) {
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
	
	/* create input node */
	input_node = nodeAddStaticNode(C, ngroup, NODE_GROUP_INPUT);
	input_node->locx = min[0] - center[0] - offsetx;
	input_node->locy = -offsety;
	
	/* create output node */
	output_node = nodeAddStaticNode(C, ngroup, NODE_GROUP_OUTPUT);
	output_node->locx = max[0] - center[0] + offsetx;
	output_node->locy = -offsety;
	
	/* relink external sockets */
	for (link = ntree->links.first; link; link = linkn) {
		int fromselect = node_group_make_use_node(link->fromnode, gnode);
		int toselect = node_group_make_use_node(link->tonode, gnode);
		
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
			bNodeSocket *iosock = ntreeAddSocketInterfaceFromSocket(ngroup, link->tonode, link->tosock);
			bNodeSocket *input_sock;
			
			/* update the group node and interface node sockets,
			 * so the new interface socket can be linked.
			 */
			node_group_verify(ntree, gnode, (ID *)ngroup);
			node_group_input_verify(ngroup, input_node, (ID *)ngroup);
			
			/* create new internal link */
			input_sock = node_group_input_find_socket(input_node, iosock->identifier);
			nodeAddLink(ngroup, input_node, input_sock, link->tonode, link->tosock);
			
			/* redirect external link */
			link->tonode = gnode;
			link->tosock = node_group_find_input_socket(gnode, iosock->identifier);
		}
		else if (fromselect) {
			bNodeSocket *iosock = ntreeAddSocketInterfaceFromSocket(ngroup, link->fromnode, link->fromsock);
			bNodeSocket *output_sock;
			
			/* update the group node and interface node sockets,
			 * so the new interface socket can be linked.
			 */
			node_group_verify(ntree, gnode, (ID *)ngroup);
			node_group_output_verify(ngroup, output_node, (ID *)ngroup);

			/* create new internal link */
			output_sock = node_group_output_find_socket(output_node, iosock->identifier);
			nodeAddLink(ngroup, link->fromnode, link->fromsock, output_node, output_sock);
			
			/* redirect external link */
			link->fromnode = gnode;
			link->fromsock = node_group_find_output_socket(gnode, iosock->identifier);
		}
	}

	/* move nodes in the group to the center */
	for (node = ngroup->nodes.first; node; node = node->next) {
		if (node_group_make_use_node(node, gnode) && !node->parent) {
			node->locx -= center[0];
			node->locy -= center[1];
		}
	}
	
	/* expose all unlinked sockets too */
	if (expose_all) {
		for (node = ngroup->nodes.first; node; node = node->next) {
			if (node_group_make_use_node(node, gnode)) {
				for (sock = node->inputs.first; sock; sock = sock->next) {
					bNodeSocket *iosock, *input_sock;
					bool skip = false;
					for (link = ngroup->links.first; link; link = link->next) {
						if (link->tosock == sock) {
							skip = true;
							break;
						}
					}
					if (skip)
						continue;
					
					iosock = ntreeAddSocketInterfaceFromSocket(ngroup, node, sock);
					
					node_group_input_verify(ngroup, input_node, (ID *)ngroup);
					
					/* create new internal link */
					input_sock = node_group_input_find_socket(input_node, iosock->identifier);
					nodeAddLink(ngroup, input_node, input_sock, node, sock);
				}
				
				for (sock = node->outputs.first; sock; sock = sock->next) {
					bNodeSocket *iosock, *output_sock;
					bool skip = false;
					for (link = ngroup->links.first; link; link = link->next)
						if (link->fromsock == sock)
							skip = true;
					if (skip)
						continue;
					
					iosock = ntreeAddSocketInterfaceFromSocket(ngroup, node, sock);
					
					node_group_output_verify(ngroup, output_node, (ID *)ngroup);
					
					/* create new internal link */
					output_sock = node_group_output_find_socket(output_node, iosock->identifier);
					nodeAddLink(ngroup, node, sock, output_node, output_sock);
				}
			}
		}
	}

	/* update of the group tree */
	ngroup->update |= NTREE_UPDATE | NTREE_UPDATE_LINKS;
	/* update of the tree containing the group instance node */
	ntree->update |= NTREE_UPDATE_NODES | NTREE_UPDATE_LINKS;
}

static bNode *node_group_make_from_selected(const bContext *C, bNodeTree *ntree, const char *ntype, const char *ntreetype)
{
	Main *bmain = CTX_data_main(C);
	bNode *gnode;
	bNodeTree *ngroup;
	float min[2], max[2];
	int totselect;
	
	totselect = node_get_selected_minmax(ntree, NULL, min, max);
	/* don't make empty group */
	if (totselect == 0)
		return NULL;
	
	/* new nodetree */
	ngroup = ntreeAddTree(bmain, "NodeGroup", ntreetype);
	
	/* make group node */
	gnode = nodeAddNode(C, ntree, ntype);
	gnode->id = (ID *)ngroup;
	
	gnode->locx = 0.5f * (min[0] + max[0]);
	gnode->locy = 0.5f * (min[1] + max[1]);
	
	node_group_make_insert_selected(C, ntree, gnode);

	/* update of the tree containing the group instance node */
	ntree->update |= NTREE_UPDATE_NODES;

	return gnode;
}

static int node_group_make_exec(bContext *C, wmOperator *op)
{
	SpaceNode *snode = CTX_wm_space_node(C);
	bNodeTree *ntree = snode->edittree;
	const char *ntree_idname = group_ntree_idname(C);
	const char *node_idname = group_node_idname(C);
	bNodeTree *ngroup;
	bNode *gnode;
	Main *bmain = CTX_data_main(C);
	
	ED_preview_kill_jobs(C);
	
	if (!node_group_make_test_selected(ntree, NULL, ntree_idname, op->reports))
		return OPERATOR_CANCELLED;
	
	gnode = node_group_make_from_selected(C, ntree, node_idname, ntree_idname);
	
	if (gnode) {
		ngroup = (bNodeTree *)gnode->id;
		
		nodeSetActive(ntree, gnode);
		if (ngroup) {
			ED_node_tree_push(snode, ngroup, gnode);
			ntreeUpdateTree(bmain, ngroup);
		}
	}
	
	ntreeUpdateTree(bmain, ntree);

	snode_notify(C, snode);
	snode_dag_update(C, snode);
	
	return OPERATOR_FINISHED;
}

void NODE_OT_group_make(wmOperatorType *ot)
{
	/* identifiers */
	ot->name = "Make Group";
	ot->description = "Make group from selected nodes";
	ot->idname = "NODE_OT_group_make";
	
	/* api callbacks */
	ot->exec = node_group_make_exec;
	ot->poll = node_group_operator_editable;
	
	/* flags */
	ot->flag = OPTYPE_REGISTER | OPTYPE_UNDO;
}

/* ****************** Group Insert operator ******************* */

static int node_group_insert_exec(bContext *C, wmOperator *op)
{
	SpaceNode *snode = CTX_wm_space_node(C);
	bNodeTree *ntree = snode->edittree;
	bNodeTree *ngroup;
	const char *node_idname = group_node_idname(C);
	bNode *gnode;
	Main *bmain = CTX_data_main(C);
	
	ED_preview_kill_jobs(C);
	
	gnode = node_group_get_active(C, node_idname);
	
	if (!gnode || !gnode->id)
		return OPERATOR_CANCELLED;
	
	ngroup = (bNodeTree *)gnode->id;
	if (!node_group_make_test_selected(ntree, gnode, ngroup->idname, op->reports))
		return OPERATOR_CANCELLED;
	
	node_group_make_insert_selected(C, ntree, gnode);
	
	nodeSetActive(ntree, gnode);
	ED_node_tree_push(snode, ngroup, gnode);
	ntreeUpdateTree(bmain, ngroup);
	
	ntreeUpdateTree(bmain, ntree);
	
	snode_notify(C, snode);
	snode_dag_update(C, snode);
	
	return OPERATOR_FINISHED;
}

void NODE_OT_group_insert(wmOperatorType *ot)
{
	/* identifiers */
	ot->name = "Group Insert";
	ot->description = "Insert selected nodes into a node group";
	ot->idname = "NODE_OT_group_insert";
	
	/* api callbacks */
	ot->exec = node_group_insert_exec;
	ot->poll = node_group_operator_editable;
	
	/* flags */
	ot->flag = OPTYPE_REGISTER | OPTYPE_UNDO;
}
