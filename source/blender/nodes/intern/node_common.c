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

#include "DNA_action_types.h"
#include "DNA_anim_types.h"
#include "DNA_node_types.h"

#include "BLI_listbase.h"
#include "BLI_string.h"
#include "BLI_utildefines.h"

#include "BKE_action.h"
#include "BKE_animsys.h"
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
	
	if (gsock->default_value)
		sock->default_value = MEM_dupallocN(gsock->default_value);
	
	if(lb)
		BLI_addtail(lb, sock);
	
	return sock;
}

bNode *node_group_make_from_selected(bNodeTree *ntree)
{
	bNodeLink *link, *linkn;
	bNode *node, *gnode, *nextn;
	bNodeTree *ngroup;
	bNodeSocket *gsock;
	ListBase anim_basepaths = {NULL, NULL};
	float min[2], max[2];
	int totnode=0;
	bNodeTemplate ntemp;
	
	INIT_MINMAX2(min, max);
	
	/* is there something to group? also do some clearing */
	for(node= ntree->nodes.first; node; node= node->next) {
		if(node->flag & NODE_SELECT) {
			/* no groups in groups */
			if(node->type==NODE_GROUP)
				return NULL;
			DO_MINMAX2( (&node->locx), min, max);
			totnode++;
		}
		node->done= 0;
	}
	if(totnode==0) return NULL;
	
	/* check if all connections are OK, no unselected node has both
		inputs and outputs to a selection */
	for(link= ntree->links.first; link; link= link->next) {
		if(link->fromnode && link->tonode && link->fromnode->flag & NODE_SELECT)
			link->tonode->done |= 1;
		if(link->fromnode && link->tonode && link->tonode->flag & NODE_SELECT)
			link->fromnode->done |= 2;
	}	
	
	for(node= ntree->nodes.first; node; node= node->next) {
		if((node->flag & NODE_SELECT)==0)
			if(node->done==3)
				break;
	}
	if(node) 
		return NULL;
	
	/* OK! new nodetree */
	ngroup= ntreeAddTree("NodeGroup", ntree->type, NODE_GROUP);
	
	/* move nodes over */
	for(node= ntree->nodes.first; node; node= nextn) {
		nextn= node->next;
		if(node->flag & NODE_SELECT) {
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
			
			/* change node-collection membership */
			BLI_remlink(&ntree->nodes, node);
			BLI_addtail(&ngroup->nodes, node);
			
			node->locx-= 0.5f*(min[0]+max[0]);
			node->locy-= 0.5f*(min[1]+max[1]);
		}
	}

	/* move animation data over */
	if (ntree->adt) {
		LinkData *ld, *ldn=NULL;
		
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
	
	/* make group node */
	ntemp.type = NODE_GROUP;
	ntemp.ngroup = ngroup;
	gnode= nodeAddNode(ntree, &ntemp);
	gnode->locx= 0.5f*(min[0]+max[0]);
	gnode->locy= 0.5f*(min[1]+max[1]);
	
	/* relink external sockets */
	for(link= ntree->links.first; link; link= linkn) {
		linkn= link->next;
		
		if(link->fromnode && link->tonode && (link->fromnode->flag & link->tonode->flag & NODE_SELECT)) {
			BLI_remlink(&ntree->links, link);
			BLI_addtail(&ngroup->links, link);
		}
		else if(link->tonode && (link->tonode->flag & NODE_SELECT)) {
			gsock = node_group_expose_socket(ngroup, link->tosock, SOCK_IN);
			link->tosock->link = nodeAddLink(ngroup, NULL, gsock, link->tonode, link->tosock);
			link->tosock = node_group_add_extern_socket(ntree, &gnode->inputs, SOCK_IN, gsock);
			link->tonode = gnode;
		}
		else if(link->fromnode && (link->fromnode->flag & NODE_SELECT)) {
			/* search for existing group node socket */
			for (gsock=ngroup->outputs.first; gsock; gsock=gsock->next)
				if (gsock->link && gsock->link->fromsock==link->fromsock)
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
	ntreeUpdateTree(ngroup);
	/* update of the tree containing the group instance node */
	ntree->update |= NTREE_UPDATE_NODES | NTREE_UPDATE_LINKS;
	ntreeUpdateTree(ntree);

	return gnode;
}

/* XXX This is a makeshift function to have useful initial group socket values.
 * In the end this should be implemented by a flexible socket data conversion system,
 * which is yet to be implemented. The idea is that beside default standard conversions,
 * such as int-to-float, it should be possible to quickly select a conversion method or
 * a chain of conversions for each input, whenever there is more than one option.
 * E.g. a vector-to-float conversion could use either of the x/y/z components or
 * the vector length.
 *
 * In the interface this could be implemented by a pseudo-script textbox on linked inputs,
 * with quick selection from a predefined list of conversion options. Some Examples:
 * - vector component 'z' (vector->float):						"z"
 * - greyscale color (float->color):							"grey"
 * - color luminance (color->float):							"lum"
 * - matrix column 2 length (matrix->vector->float):			"col[1].len"
 * - mesh vertex coordinate 'y' (mesh->vertex->vector->float):	"vertex.co.y"
 *
 * The actual conversion is then done by a series of conversion functions,
 * which are defined in the socket type structs.
 */
static void convert_socket_value(bNodeSocket *from, bNodeSocket *to)
{
	/* XXX only one of these pointers is valid! just putting them here for convenience */
	bNodeSocketValueFloat *fromfloat= (bNodeSocketValueFloat*)from->default_value;
	bNodeSocketValueInt *fromint= (bNodeSocketValueInt*)from->default_value;
	bNodeSocketValueBoolean *frombool= (bNodeSocketValueBoolean*)from->default_value;
	bNodeSocketValueVector *fromvector= (bNodeSocketValueVector*)from->default_value;
	bNodeSocketValueRGBA *fromrgba= (bNodeSocketValueRGBA*)from->default_value;

	bNodeSocketValueFloat *tofloat= (bNodeSocketValueFloat*)to->default_value;
	bNodeSocketValueInt *toint= (bNodeSocketValueInt*)to->default_value;
	bNodeSocketValueBoolean *tobool= (bNodeSocketValueBoolean*)to->default_value;
	bNodeSocketValueVector *tovector= (bNodeSocketValueVector*)to->default_value;
	bNodeSocketValueRGBA *torgba= (bNodeSocketValueRGBA*)to->default_value;

	switch (from->type) {
	case SOCK_FLOAT:
		switch (to->type) {
		case SOCK_FLOAT:
			tofloat->value = fromfloat->value;
			break;
		case SOCK_INT:
			toint->value = (int)fromfloat->value;
			break;
		case SOCK_BOOLEAN:
			tobool->value = (fromfloat->value > 0.0f);
			break;
		case SOCK_VECTOR:
			tovector->value[0] = tovector->value[1] = tovector->value[2] = fromfloat->value;
			break;
		case SOCK_RGBA:
			torgba->value[0] = torgba->value[1] = torgba->value[2] = torgba->value[3] = fromfloat->value;
			break;
		}
		break;
	case SOCK_INT:
		switch (to->type) {
		case SOCK_FLOAT:
			tofloat->value = (float)fromint->value;
			break;
		case SOCK_INT:
			toint->value = fromint->value;
			break;
		case SOCK_BOOLEAN:
			tobool->value = (fromint->value > 0);
			break;
		case SOCK_VECTOR:
			tovector->value[0] = tovector->value[1] = tovector->value[2] = (float)fromint->value;
			break;
		case SOCK_RGBA:
			torgba->value[0] = torgba->value[1] = torgba->value[2] = torgba->value[3] = (float)fromint->value;
			break;
		}
		break;
	case SOCK_BOOLEAN:
		switch (to->type) {
		case SOCK_FLOAT:
			tofloat->value = (float)frombool->value;
			break;
		case SOCK_INT:
			toint->value = (int)frombool->value;
			break;
		case SOCK_BOOLEAN:
			tobool->value = frombool->value;
			break;
		case SOCK_VECTOR:
			tovector->value[0] = tovector->value[1] = tovector->value[2] = (float)frombool->value;
			break;
		case SOCK_RGBA:
			torgba->value[0] = torgba->value[1] = torgba->value[2] = torgba->value[3] = (float)frombool->value;
			break;
		}
		break;
	case SOCK_VECTOR:
		switch (to->type) {
		case SOCK_FLOAT:
			tofloat->value = fromvector->value[0];
			break;
		case SOCK_INT:
			toint->value = (int)fromvector->value[0];
			break;
		case SOCK_BOOLEAN:
			tobool->value = (fromvector->value[0] > 0.0f);
			break;
		case SOCK_VECTOR:
			copy_v3_v3(tovector->value, fromvector->value);
			break;
		case SOCK_RGBA:
			copy_v3_v3(torgba->value, fromvector->value);
			torgba->value[3] = 1.0f;
			break;
		}
		break;
	case SOCK_RGBA:
		switch (to->type) {
		case SOCK_FLOAT:
			tofloat->value = fromrgba->value[0];
			break;
		case SOCK_INT:
			toint->value = (int)fromrgba->value[0];
			break;
		case SOCK_BOOLEAN:
			tobool->value = (fromrgba->value[0] > 0.0f);
			break;
		case SOCK_VECTOR:
			copy_v3_v3(tovector->value, fromrgba->value);
			break;
		case SOCK_RGBA:
			copy_v4_v4(torgba->value, fromrgba->value);
			break;
		}
		break;
	}
}

static void copy_socket_value(bNodeSocket *from, bNodeSocket *to)
{
	/* XXX only one of these pointers is valid! just putting them here for convenience */
	bNodeSocketValueFloat *fromfloat= (bNodeSocketValueFloat*)from->default_value;
	bNodeSocketValueInt *fromint= (bNodeSocketValueInt*)from->default_value;
	bNodeSocketValueBoolean *frombool= (bNodeSocketValueBoolean*)from->default_value;
	bNodeSocketValueVector *fromvector= (bNodeSocketValueVector*)from->default_value;
	bNodeSocketValueRGBA *fromrgba= (bNodeSocketValueRGBA*)from->default_value;

	bNodeSocketValueFloat *tofloat= (bNodeSocketValueFloat*)to->default_value;
	bNodeSocketValueInt *toint= (bNodeSocketValueInt*)to->default_value;
	bNodeSocketValueBoolean *tobool= (bNodeSocketValueBoolean*)to->default_value;
	bNodeSocketValueVector *tovector= (bNodeSocketValueVector*)to->default_value;
	bNodeSocketValueRGBA *torgba= (bNodeSocketValueRGBA*)to->default_value;

	if (from->type != to->type)
		return;

	switch (from->type) {
	case SOCK_FLOAT:
		*tofloat = *fromfloat;
		break;
	case SOCK_INT:
		*toint = *fromint;
		break;
	case SOCK_BOOLEAN:
		*tobool = *frombool;
		break;
	case SOCK_VECTOR:
		*tovector = *fromvector;
		break;
	case SOCK_RGBA:
		*torgba = *fromrgba;
		break;
	}
}

/* returns 1 if its OK */
int node_group_ungroup(bNodeTree *ntree, bNode *gnode)
{
	bNodeLink *link, *linkn;
	bNode *node, *nextn;
	bNodeTree *ngroup, *wgroup;
	ListBase anim_basepaths = {NULL, NULL};
	
	ngroup= (bNodeTree *)gnode->id;
	if(ngroup==NULL) return 0;
	
	/* clear new pointers, set in copytree */
	for(node= ntree->nodes.first; node; node= node->next)
		node->new_node= NULL;
	
	/* wgroup is a temporary copy of the NodeTree we're merging in
	 *	- all of wgroup's nodes are transferred across to their new home
	 *	- ngroup (i.e. the source NodeTree) is left unscathed
	 */
	wgroup= ntreeCopyTree(ngroup);
	
	/* add the nodes into the ntree */
	for(node= wgroup->nodes.first; node; node= nextn) {
		nextn= node->next;
		
		/* keep track of this node's RNA "base" path (the part of the pat identifying the node) 
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
		
		node->locx+= gnode->locx;
		node->locy+= gnode->locy;
		
		node->flag |= NODE_SELECT;
	}
	
	/* restore external links to and from the gnode */
	for(link= ntree->links.first; link; link= link->next) {
		if (link->fromnode==gnode) {
			if (link->fromsock->groupsock) {
				bNodeSocket *gsock= link->fromsock->groupsock;
				if (gsock->link) {
					if (gsock->link->fromnode) {
						/* NB: using the new internal copies here! the groupsock pointer still maps to the old tree */
						link->fromnode = (gsock->link->fromnode ? gsock->link->fromnode->new_node : NULL);
						link->fromsock = gsock->link->fromsock->new_sock;
					}
					else {
						/* group output directly maps to group input */
						bNodeSocket *insock= node_group_find_input(gnode, gsock->link->fromsock);
						if (insock->link) {
							link->fromnode = insock->link->fromnode;
							link->fromsock = insock->link->fromsock;
						}
					}
				}
				else {
					/* copy the default input value from the group socket default to the external socket */
					convert_socket_value(gsock, link->tosock);
				}
			}
		}
	}
	/* remove internal output links, these are not used anymore */
	for(link=wgroup->links.first; link; link= linkn) {
		linkn = link->next;
		if (!link->tonode)
			nodeRemLink(wgroup, link);
	}
	/* restore links from internal nodes */
	for(link= wgroup->links.first; link; link= link->next) {
		/* indicates link to group input */
		if (!link->fromnode) {
			/* NB: can't use find_group_node_input here,
			 * because gnode sockets still point to the old tree!
			 */
			bNodeSocket *insock;
			for (insock= gnode->inputs.first; insock; insock= insock->next)
				if (insock->groupsock->new_sock == link->fromsock)
					break;
			if (insock->link) {
				link->fromnode = insock->link->fromnode;
				link->fromsock = insock->link->fromsock;
			}
			else {
				/* copy the default input value from the group node socket default to the internal socket */
				convert_socket_value(insock, link->tosock);
				nodeRemLink(wgroup, link);
			}
		}
	}
	
	/* add internal links to the ntree */
	for(link= wgroup->links.first; link; link= linkn) {
		linkn= link->next;
		BLI_remlink(&wgroup->links, link);
		BLI_addtail(&ntree->links, link);
	}
	
	/* and copy across the animation,
	 * note that the animation data's action can be NULL here */
	if (wgroup->adt) {
		LinkData *ld, *ldn=NULL;
		bAction *waction;
		
		/* firstly, wgroup needs to temporary dummy action that can be destroyed, as it shares copies */
		waction = wgroup->adt->action = copy_action(wgroup->adt->action);
		
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
			free_libblock(&G.main->action, waction);
		}
	}
	
	/* delete the group instance. this also removes old input links! */
	nodeFreeNode(ntree, gnode);

	/* free the group tree (takes care of user count) */
	free_libblock(&G.main->nodetree, wgroup);
	
	ntree->update |= NTREE_UPDATE_NODES | NTREE_UPDATE_LINKS;
	ntreeUpdateTree(ntree);
	
	return 1;
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
	copy_socket_value(sock, gsock);
	
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
				copy_socket_value(sock, gsock);
				
				sock->link = nodeAddLink(ngroup, NULL, gsock, node, sock);
			}
		}
		for (sock=node->outputs.first; sock; sock=sock->next) {
			if (nodeCountSocketLinks(ngroup, sock)==0 && !nodeSocketIsHidden(sock)) {
				gsock = node_group_add_socket(ngroup, sock->name, sock->type, SOCK_OUT);
				
				/* initialize the default value. */
				copy_socket_value(sock, gsock);
				
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
	return (node->id)? node->id->name+2: "Missing Datablock";
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
	
	for(sock= lb->first; sock; sock= sock->next) {
		if(sock->own_index==gsock->own_index)
				break;
	}
	if(sock) {
		sock->groupsock = gsock;
		
		BLI_strncpy(sock->name, gsock->name, sizeof(sock->name));
		sock->type= gsock->type;
		
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
			if(ngroup->id.lib)
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
	/* bNodeSocket *sock; */ /* UNUSED */
	
	node->id = (ID*)ntemp->ngroup;
	
	/* sock = */ nodeAddInputFloat(ntree, node, "Iterations", PROP_UNSIGNED, 1, 0, 10000);
	
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
	/* bNodeSocket *sock; */ /* UNUSED */
	
	node->id = (ID*)ntemp->ngroup;
	
	/* sock = */ nodeAddInputFloat(ntree, node, "Condition", PROP_NONE, 1, 0, 1);
	
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

void register_node_type_frame(bNodeTreeType *ttype)
{
	/* frame type is used for all tree types, needs dynamic allocation */
	bNodeType *ntype= MEM_callocN(sizeof(bNodeType), "frame node type");

	node_type_base(ttype, ntype, NODE_FRAME, "Frame", NODE_CLASS_LAYOUT, NODE_BACKGROUND);
	node_type_size(ntype, 150, 100, 0);
	node_type_compatibility(ntype, NODE_OLD_SHADING|NODE_NEW_SHADING);
	
	ntype->needs_free = 1;
	nodeRegisterType(ttype, ntype);
}
