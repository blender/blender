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
 * The Original Code is Copyright (C) 2007 Blender Foundation.
 * All rights reserved.
 *
 * The Original Code is: all of this file.
 *
 * Contributor(s): Lukas Toenne.
 *
 * ***** END GPL LICENSE BLOCK *****
 */

/** \file blender/nodes/intern/node_common.c
 *  \ingroup nodes
 */


#include <string.h>

#include "DNA_node_types.h"

#include "BLI_listbase.h"
#include "BLI_string.h"
#include "BLI_utildefines.h"

#include "BLF_translation.h"

#include "BKE_global.h"
#include "BKE_library.h"
#include "BKE_main.h"
#include "BLI_math.h"
#include "BKE_node.h"
#include "BKE_utildefines.h"

#include "RNA_access.h"
#include "RNA_types.h"

#include "MEM_guardedalloc.h"

#include "node_common.h"
#include "node_util.h"
#include "node_exec.h"
#include "NOD_socket.h"

/**** Group ****/

bNodeSocket *node_group_find_input(bNode *gnode, bNodeSocket *gsock)
{
	bNodeSocket *sock;
	for (sock=gnode->inputs.first; sock; sock=sock->next)
		if (sock->groupsock == gsock)
			return sock;
	return NULL;
}

bNodeSocket *node_group_find_output(bNode *gnode, bNodeSocket *gsock)
{
	bNodeSocket *sock;
	for (sock=gnode->outputs.first; sock; sock=sock->next)
		if (sock->groupsock == gsock)
			return sock;
	return NULL;
}

bNodeSocket *node_group_add_extern_socket(bNodeTree *UNUSED(ntree), ListBase *lb, int in_out, bNodeSocket *gsock)
{
	bNodeSocket *sock;
	
	if (gsock->flag & SOCK_INTERNAL)
		return NULL;
	
	sock= MEM_callocN(sizeof(bNodeSocket), "sock");
	
	/* make a copy of the group socket */
	*sock = *gsock;
	sock->link = NULL;
	sock->next = sock->prev = NULL;
	sock->new_sock = NULL;
	
	/* group sockets are dynamically added */
	sock->flag |= SOCK_DYNAMIC;
	
	sock->own_index = gsock->own_index;
	sock->groupsock = gsock;
	sock->limit = (in_out==SOCK_IN ? 1 : 0xFFF);
	
	sock->default_value = node_socket_make_default_value(sock->type);
	node_socket_copy_default_value(sock->type, sock->default_value, gsock->default_value);
	
	if (lb)
		BLI_addtail(lb, sock);
	
	return sock;
}

bNodeSocket *node_group_add_socket(bNodeTree *ngroup, const char *name, int type, int in_out)
{
	bNodeSocketType *stype = ntreeGetSocketType(type);
	bNodeSocket *gsock = MEM_callocN(sizeof(bNodeSocket), "bNodeSocket");
	
	BLI_strncpy(gsock->name, name, sizeof(gsock->name));
	gsock->type = type;
	/* group sockets are dynamically added */
	gsock->flag |= SOCK_DYNAMIC;

	gsock->next = gsock->prev = NULL;
	gsock->new_sock = NULL;
	gsock->link = NULL;
	/* assign new unique index */
	gsock->own_index = ngroup->cur_index++;
	gsock->limit = (in_out==SOCK_IN ? 0xFFF : 1);
	
	if (stype->value_structsize > 0)
		gsock->default_value = MEM_callocN(stype->value_structsize, "default socket value");
	
	BLI_addtail(in_out==SOCK_IN ? &ngroup->inputs : &ngroup->outputs, gsock);
	
	ngroup->update |= (in_out==SOCK_IN ? NTREE_UPDATE_GROUP_IN : NTREE_UPDATE_GROUP_OUT);
	
	return gsock;
}

bNodeSocket *node_group_expose_socket(bNodeTree *ngroup, bNodeSocket *sock, int in_out)
{
	bNodeSocket *gsock= node_group_add_socket(ngroup, sock->name, sock->type, in_out);
	
	/* initialize the default value. */
	node_socket_copy_default_value(gsock->type, gsock->default_value, sock->default_value);
	
	return gsock;
}

void node_group_expose_all_sockets(bNodeTree *ngroup)
{
	bNode *node;
	bNodeSocket *sock, *gsock;
	
	for (node=ngroup->nodes.first; node; node=node->next) {
		for (sock=node->inputs.first; sock; sock=sock->next) {
			if (!sock->link && !nodeSocketIsHidden(sock)) {
				gsock = node_group_add_socket(ngroup, sock->name, sock->type, SOCK_IN);
				
				/* initialize the default value. */
				node_socket_copy_default_value(gsock->type, gsock->default_value, sock->default_value);
				
				sock->link = nodeAddLink(ngroup, NULL, gsock, node, sock);
			}
		}
		for (sock=node->outputs.first; sock; sock=sock->next) {
			if (nodeCountSocketLinks(ngroup, sock)==0 && !nodeSocketIsHidden(sock)) {
				gsock = node_group_add_socket(ngroup, sock->name, sock->type, SOCK_OUT);
				
				/* initialize the default value. */
				node_socket_copy_default_value(gsock->type, gsock->default_value, sock->default_value);
				
				gsock->link = nodeAddLink(ngroup, node, sock, NULL, gsock);
			}
		}
	}
}

void node_group_remove_socket(bNodeTree *ngroup, bNodeSocket *gsock, int in_out)
{
	nodeRemSocketLinks(ngroup, gsock);
	
	switch (in_out) {
	case SOCK_IN:
		BLI_remlink(&ngroup->inputs, gsock);
		ngroup->update |= NTREE_UPDATE_GROUP_IN;
		break;
	case SOCK_OUT:
		BLI_remlink(&ngroup->outputs, gsock);
		ngroup->update |= NTREE_UPDATE_GROUP_OUT;
		break;
	}
	
	if (gsock->default_value)
		MEM_freeN(gsock->default_value);
	
	MEM_freeN(gsock);
}

/* groups display their internal tree name as label */
const char *node_group_label(bNode *node)
{
	return (node->id)? node->id->name+2: IFACE_("Missing Datablock");
}

int node_group_valid(bNodeTree *ntree, bNodeTemplate *ntemp)
{
	bNodeTemplate childtemp;
	bNode *node;
	
	/* regular groups cannot be recursive */
	if (ntree == ntemp->ngroup)
		return 0;
	
	/* make sure all children are valid */
	for (node=ntemp->ngroup->nodes.first; node; node=node->next) {
		childtemp = nodeMakeTemplate(node);
		if (!nodeValid(ntree, &childtemp))
			return 0;
	}
	
	return 1;
}

bNodeTemplate node_group_template(bNode *node)
{
	bNodeTemplate ntemp;
	ntemp.type = NODE_GROUP;
	ntemp.ngroup = (bNodeTree*)node->id;
	return ntemp;
}

void node_group_init(bNodeTree *ntree, bNode *node, bNodeTemplate *ntemp)
{
	node->id = (ID*)ntemp->ngroup;
	
	/* NB: group socket input/output roles are inverted internally!
	 * Group "inputs" work as outputs in links and vice versa.
	 */
	if (ntemp->ngroup) {
		bNodeSocket *gsock;
		for (gsock=ntemp->ngroup->inputs.first; gsock; gsock=gsock->next)
			node_group_add_extern_socket(ntree, &node->inputs, SOCK_IN, gsock);
		for (gsock=ntemp->ngroup->outputs.first; gsock; gsock=gsock->next)
			node_group_add_extern_socket(ntree, &node->outputs, SOCK_OUT, gsock);
	}
}

static bNodeSocket *group_verify_socket(bNodeTree *ntree, ListBase *lb, int in_out, bNodeSocket *gsock)
{
	bNodeSocket *sock;
	
	/* group sockets tagged as internal are not exposed ever */
	if (gsock->flag & SOCK_INTERNAL)
		return NULL;
	
	for (sock= lb->first; sock; sock= sock->next) {
		if (sock->own_index==gsock->own_index)
				break;
	}
	if (sock) {
		sock->groupsock = gsock;
		
		BLI_strncpy(sock->name, gsock->name, sizeof(sock->name));
		if (gsock->type != sock->type)
			nodeSocketSetType(sock, gsock->type);
		
		/* XXX hack: group socket input/output roles are inverted internally,
		 * need to change the limit value when making actual node sockets from them.
		 */
		sock->limit = (in_out==SOCK_IN ? 1 : 0xFFF);
		
		BLI_remlink(lb, sock);
		
		return sock;
	}
	else {
		return node_group_add_extern_socket(ntree, NULL, in_out, gsock);
	}
}

static void group_verify_socket_list(bNodeTree *ntree, bNode *node, ListBase *lb, int in_out, ListBase *glb)
{
	bNodeSocket *sock, *nextsock, *gsock;
	
	/* step by step compare */
	for (gsock= glb->first; gsock; gsock=gsock->next) {
		/* abusing new_sock pointer for verification here! only used inside this function */
		gsock->new_sock= group_verify_socket(ntree, lb, in_out, gsock);
	}
	/* leftovers are removed */
	for (sock=lb->first; sock; sock=nextsock) {
		nextsock=sock->next;
		if (sock->flag & SOCK_DYNAMIC)
			nodeRemoveSocket(ntree, node, sock);
	}
	/* and we put back the verified sockets */
	for (gsock= glb->first; gsock; gsock=gsock->next) {
		if (gsock->new_sock) {
			BLI_addtail(lb, gsock->new_sock);
			gsock->new_sock = NULL;
		}
	}
}

/* make sure all group node in ntree, which use ngroup, are sync'd */
void node_group_verify(struct bNodeTree *ntree, struct bNode *node, struct ID *id)
{
	/* check inputs and outputs, and remove or insert them */
	if (node->id==id) {
		bNodeTree *ngroup= (bNodeTree*)node->id;
		group_verify_socket_list(ntree, node, &node->inputs, SOCK_IN, &ngroup->inputs);
		group_verify_socket_list(ntree, node, &node->outputs, SOCK_OUT, &ngroup->outputs);
	}
}

struct bNodeTree *node_group_edit_get(bNode *node)
{
	if (node->flag & NODE_GROUP_EDIT)
		return (bNodeTree*)node->id;
	else
		return NULL;
}

struct bNodeTree *node_group_edit_set(bNode *node, int edit)
{
	if (edit) {
		bNodeTree *ngroup= (bNodeTree*)node->id;
		if (ngroup) {
			if (ngroup->id.lib)
				ntreeMakeLocal(ngroup);
			
			node->flag |= NODE_GROUP_EDIT;
		}
		return ngroup;
	}
	else {
		node->flag &= ~NODE_GROUP_EDIT;
		return NULL;
	}
}

void node_group_edit_clear(bNode *node)
{
	bNodeTree *ngroup= (bNodeTree*)node->id;
	bNode *inode;
	
	node->flag &= ~NODE_GROUP_EDIT;
	
	if (ngroup)
		for (inode=ngroup->nodes.first; inode; inode=inode->next)
			nodeGroupEditClear(inode);
}

void node_group_link(bNodeTree *ntree, bNodeSocket *sock, int in_out)
{
	node_group_expose_socket(ntree, sock, in_out);
}

/**** For Loop ****/

/* Essentially a group node with slightly different behavior.
 * The internal tree is executed several times, with each output being re-used
 * as an input in the next iteration. For this purpose, input and output socket
 * lists are kept identical!
 */

bNodeTemplate node_forloop_template(bNode *node)
{
	bNodeTemplate ntemp;
	ntemp.type = NODE_FORLOOP;
	ntemp.ngroup = (bNodeTree*)node->id;
	return ntemp;
}

void node_forloop_init(bNodeTree *ntree, bNode *node, bNodeTemplate *ntemp)
{
	bNodeSocket *sock;
	
	node->id = (ID*)ntemp->ngroup;
	
	sock = nodeAddSocket(ntree, node, SOCK_IN, "Iterations", SOCK_FLOAT);
	node_socket_set_default_value_float(sock->default_value, PROP_UNSIGNED, 1, 0, 10000);
	
	/* NB: group socket input/output roles are inverted internally!
	 * Group "inputs" work as outputs in links and vice versa.
	 */
	if (ntemp->ngroup) {
		bNodeSocket *gsock;
		for (gsock=ntemp->ngroup->inputs.first; gsock; gsock=gsock->next)
			node_group_add_extern_socket(ntree, &node->inputs, SOCK_IN, gsock);
		for (gsock=ntemp->ngroup->outputs.first; gsock; gsock=gsock->next)
			node_group_add_extern_socket(ntree, &node->outputs, SOCK_OUT, gsock);
	}
}

void node_forloop_init_tree(bNodeTree *ntree)
{
	bNodeSocket *sock;
	sock = node_group_add_socket(ntree, "Iteration", SOCK_FLOAT, SOCK_IN);
	sock->flag |= SOCK_INTERNAL;
}

static void loop_sync(bNodeTree *ntree, int sync_in_out)
{
	bNodeSocket *sock, *sync, *nsync, *mirror;
	ListBase *sync_lb;
	
	if (sync_in_out==SOCK_IN) {
		sock = ntree->outputs.first;
		
		sync = ntree->inputs.first;
		sync_lb = &ntree->inputs;
	}
	else {
		sock = ntree->inputs.first;
		
		sync = ntree->outputs.first;
		sync_lb = &ntree->outputs;
	}
	
	/* NB: the sock->storage pointer is used here directly to store the own_index int
	 * out the mirrored socket counterpart!
	 */
	
	while (sock) {
		/* skip static and internal sockets on the sync side (preserves socket order!) */
		while (sync && ((sync->flag & SOCK_INTERNAL) || !(sync->flag & SOCK_DYNAMIC)))
			sync = sync->next;
		
		if (sync && !(sync->flag & SOCK_INTERNAL) && (sync->flag & SOCK_DYNAMIC)) {
			if (sock->storage==NULL) {
				/* if mirror index is 0, the sockets is newly added and a new mirror must be created. */
				mirror = node_group_expose_socket(ntree, sock, sync_in_out);
				/* store the mirror index */
				sock->storage = SET_INT_IN_POINTER(mirror->own_index);
				mirror->storage = SET_INT_IN_POINTER(sock->own_index);
				/* move mirror to the right place */
				BLI_remlink(sync_lb, mirror);
				if (sync)
					BLI_insertlinkbefore(sync_lb, sync, mirror);
				else
					BLI_addtail(sync_lb, mirror);
			}
			else {
				/* look up the mirror socket */
				for (mirror=sync; mirror; mirror=mirror->next)
					if (mirror->own_index == GET_INT_FROM_POINTER(sock->storage))
						break;
				/* make sure the name is the same (only for identification by user, no deeper meaning) */
				BLI_strncpy(mirror->name, sock->name, sizeof(mirror->name));
				/* fix the socket order if necessary */
				if (mirror != sync) {
					BLI_remlink(sync_lb, mirror);
					BLI_insertlinkbefore(sync_lb, sync, mirror);
				}
				else
					sync = sync->next;
			}
		}
		
		sock = sock->next;
	}
	
	/* remaining sockets in sync_lb are leftovers from deleted sockets, remove them */
	while (sync) {
		nsync = sync->next;
		if (!(sync->flag & SOCK_INTERNAL) && (sync->flag & SOCK_DYNAMIC))
			node_group_remove_socket(ntree, sync, sync_in_out);
		sync = nsync;
	}
}

void node_loop_update_tree(bNodeTree *ngroup)
{
	/* make sure inputs & outputs are identical */
	if (ngroup->update & NTREE_UPDATE_GROUP_IN)
		loop_sync(ngroup, SOCK_OUT);
	if (ngroup->update & NTREE_UPDATE_GROUP_OUT)
		loop_sync(ngroup, SOCK_IN);
}

void node_whileloop_init(bNodeTree *ntree, bNode *node, bNodeTemplate *ntemp)
{
	bNodeSocket *sock;
	
	node->id = (ID*)ntemp->ngroup;
	
	sock = nodeAddSocket(ntree, node, SOCK_IN, "Condition", SOCK_FLOAT);
	node_socket_set_default_value_float(sock->default_value, PROP_NONE, 1, 0, 1);
	
	/* max iterations */
	node->custom1 = 10000;
	
	/* NB: group socket input/output roles are inverted internally!
	 * Group "inputs" work as outputs in links and vice versa.
	 */
	if (ntemp->ngroup) {
		bNodeSocket *gsock;
		for (gsock=ntemp->ngroup->inputs.first; gsock; gsock=gsock->next)
			node_group_add_extern_socket(ntree, &node->inputs, SOCK_IN, gsock);
		for (gsock=ntemp->ngroup->outputs.first; gsock; gsock=gsock->next)
			node_group_add_extern_socket(ntree, &node->outputs, SOCK_OUT, gsock);
	}
}

void node_whileloop_init_tree(bNodeTree *ntree)
{
	bNodeSocket *sock;
	sock = node_group_add_socket(ntree, "Condition", SOCK_FLOAT, SOCK_OUT);
	sock->flag |= SOCK_INTERNAL;
}

bNodeTemplate node_whileloop_template(bNode *node)
{
	bNodeTemplate ntemp;
	ntemp.type = NODE_WHILELOOP;
	ntemp.ngroup = (bNodeTree*)node->id;
	return ntemp;
}

/**** FRAME ****/

static void node_frame_init(bNodeTree *UNUSED(ntree), bNode *node, bNodeTemplate *UNUSED(ntemp))
{
	NodeFrame *data = (NodeFrame *)MEM_callocN(sizeof(NodeFrame), "frame node storage");
	node->storage = data;
	
	data->flag |= NODE_FRAME_SHRINK;
	
	data->label_size = 20;
}

void register_node_type_frame(bNodeTreeType *ttype)
{
	/* frame type is used for all tree types, needs dynamic allocation */
	bNodeType *ntype= MEM_callocN(sizeof(bNodeType), "frame node type");

	node_type_base(ttype, ntype, NODE_FRAME, "Frame", NODE_CLASS_LAYOUT, NODE_BACKGROUND|NODE_OPTIONS);
	node_type_init(ntype, node_frame_init);
	node_type_storage(ntype, "NodeFrame", node_free_standard_storage, node_copy_standard_storage);
	node_type_size(ntype, 150, 100, 0);
	node_type_compatibility(ntype, NODE_OLD_SHADING|NODE_NEW_SHADING);
	
	ntype->needs_free = 1;
	nodeRegisterType(ttype, ntype);
}


/* **************** REROUTE ******************** */

static bNodeSocketTemplate node_reroute_in[]= {
	{	SOCK_RGBA, 1, "Input",			0.0f, 0.0f, 0.0f, 1.0f, 0.0f, 1.0f},
	{	-1, 0, ""	}
};
static bNodeSocketTemplate node_reroute_out[]= {
	{	SOCK_RGBA, 0, "Output",			0.0f, 0.0f, 0.0f, 1.0f, 0.0f, 1.0f},
	{	-1, 0, ""	}
};

/* simple, only a single input and output here */
static ListBase node_reroute_internal_connect(bNodeTree *ntree, bNode *node)
{
	bNodeLink *link;
	ListBase ret;

	ret.first = ret.last = NULL;

	/* Security check! */
	if (!ntree)
		return ret;

	link = MEM_callocN(sizeof(bNodeLink), "internal node link");
	link->fromnode = node;
	link->fromsock = node->inputs.first;
	link->tonode = node;
	link->tosock = node->outputs.first;
	/* internal link is always valid */
	link->flag |= NODE_LINK_VALID;
	BLI_addtail(&ret, link);

	return ret;
}

static void node_reroute_update(bNodeTree *UNUSED(ntree), bNode *node)
{
	bNodeSocket *input = node->inputs.first;
	bNodeSocket *output = node->outputs.first;
	int type = SOCK_FLOAT;
	
	/* determine socket type from unambiguous input/output connection if possible */
	if (input->limit==1 && input->link)
		type = input->link->fromsock->type;
	else if (output->limit==1 && output->link)
			type = output->link->tosock->type;
	
	/* same type for input/output */
	nodeSocketSetType(input, type);
	nodeSocketSetType(output, type);
}

void register_node_type_reroute(bNodeTreeType *ttype)
{
	/* frame type is used for all tree types, needs dynamic allocation */
	bNodeType *ntype= MEM_callocN(sizeof(bNodeType), "frame node type");
	
	node_type_base(ttype, ntype, NODE_REROUTE, "Reroute", NODE_CLASS_LAYOUT, 0);
	node_type_socket_templates(ntype, node_reroute_in, node_reroute_out);
	node_type_internal_connect(ntype, node_reroute_internal_connect);
	node_type_update(ntype, node_reroute_update, NULL);
	
	ntype->needs_free = 1;
	nodeRegisterType(ttype, ntype);
}
