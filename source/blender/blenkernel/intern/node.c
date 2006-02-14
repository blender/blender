/**
 * $Id$
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
 * The Original Code is Copyright (C) 2005 Blender Foundation.
 * All rights reserved.
 *
 * The Original Code is: all of this file.
 *
 * Contributor(s): none yet.
 *
 * ***** END GPL LICENSE BLOCK *****
 */

#include <stdlib.h>
#include <string.h>

#include "DNA_ID.h"
#include "DNA_node_types.h"
#include "DNA_material_types.h"
#include "DNA_scene_types.h"

#include "BKE_blender.h"
#include "BKE_colortools.h"
#include "BKE_global.h"
#include "BKE_image.h"
#include "BKE_library.h"
#include "BKE_main.h"
#include "BKE_node.h"
#include "BKE_texture.h"
#include "BKE_utildefines.h"

#include "BLI_arithb.h"
#include "BLI_blenlib.h"
#include "BLI_threads.h"

#include "PIL_time.h"

#include "MEM_guardedalloc.h"
#include "IMB_imbuf.h"

/* not very important, but the stack solver likes to know a maximum */
#define MAX_SOCKET	64

#pragma mark /* ************** Type stuff **********  */

static bNodeType *node_get_type(bNodeTree *ntree, int type, bNodeTree *ngroup)
{
	if(type==NODE_GROUP) {
		if(ngroup && GS(ngroup->id.name)==ID_NT) {
			return ngroup->owntype;
		}
		return NULL;
	}
	else {
		bNodeType **typedefs= ntree->alltypes;
		
		while( *typedefs && (*typedefs)->type!=type)
			typedefs++;
		
		return *typedefs;
	}
}

void ntreeInitTypes(bNodeTree *ntree)
{
	bNode *node, *next;
	
	if(ntree->type==NTREE_SHADER)
		ntree->alltypes= node_all_shaders;
	else if(ntree->type==NTREE_COMPOSIT)
		ntree->alltypes= node_all_composit;
	else {
		ntree->alltypes= NULL;
		printf("Error: no type definitions for nodes\n");
	}
	
	for(node= ntree->nodes.first; node; node= next) {
		next= node->next;
		node->typeinfo= node_get_type(ntree, node->type, (bNodeTree *)node->id);
		if(node->typeinfo==NULL) {
			printf("Error: Node type %s doesn't exist anymore, removed\n", node->name);
			nodeFreeNode(ntree, node);
		}
	}
			
	ntree->init |= NTREE_TYPE_INIT;
}

/* only used internal... we depend on type definitions! */
static bNodeSocket *node_add_socket_type(ListBase *lb, bNodeSocketType *stype)
{
	bNodeSocket *sock= MEM_callocN(sizeof(bNodeSocket), "sock");
	
	BLI_strncpy(sock->name, stype->name, NODE_MAXSTR);
	if(stype->limit==0) sock->limit= 0xFFF;
	else sock->limit= stype->limit;
	sock->type= stype->type;
	
	sock->to_index= stype->own_index;
	sock->tosock= stype->internsock;
	
	sock->ns.vec[0]= stype->val1;
	sock->ns.vec[1]= stype->val2;
	sock->ns.vec[2]= stype->val3;
	sock->ns.vec[3]= stype->val4;
	sock->ns.min= stype->min;
	sock->ns.max= stype->max;
	
	if(lb)
		BLI_addtail(lb, sock);
	
	return sock;
}

static void node_rem_socket(bNodeTree *ntree, ListBase *lb, bNodeSocket *sock)
{
	bNodeLink *link, *next;
	
	for(link= ntree->links.first; link; link= next) {
		next= link->next;
		if(link->fromsock==sock || link->tosock==sock) {
			nodeRemLink(ntree, link);
		}
	}
	
	BLI_remlink(lb, sock);
	MEM_freeN(sock);
}

static bNodeSocket *verify_socket(ListBase *lb, bNodeSocketType *stype)
{
	bNodeSocket *sock;
	
	for(sock= lb->first; sock; sock= sock->next) {
		/* both indices are zero for non-groups, otherwise it's a unique index */
		if(sock->to_index==stype->own_index)
			if(strncmp(sock->name, stype->name, NODE_MAXSTR)==0)
				break;
	}
	if(sock) {
		sock->type= stype->type;		/* in future, read this from tydefs! */
		if(stype->limit==0) sock->limit= 0xFFF;
		else sock->limit= stype->limit;
		sock->ns.min= stype->min;
		sock->ns.max= stype->max;
		sock->tosock= stype->internsock;
		
		BLI_remlink(lb, sock);
		
		return sock;
	}
	else {
		return node_add_socket_type(NULL, stype);
	}
}

static void verify_socket_list(bNodeTree *ntree, ListBase *lb, bNodeSocketType *stype_first)
{
	bNodeSocketType *stype;
	
	/* no inputs anymore? */
	if(stype_first==NULL) {
		while(lb->first)
			node_rem_socket(ntree, lb, lb->first);
	}
	else {
		/* step by step compare */
		stype= stype_first;
		while(stype->type != -1) {
			stype->sock= verify_socket(lb, stype);
			stype++;
		}
		/* leftovers are removed */
		while(lb->first)
			node_rem_socket(ntree, lb, lb->first);
		/* and we put back the verified sockets */
		stype= stype_first;
		while(stype->type != -1) {
			BLI_addtail(lb, stype->sock);
			stype++;
		}
	}
}

void nodeVerifyType(bNodeTree *ntree, bNode *node)
{
	bNodeType *ntype= node->typeinfo;
	
	if(ntype) {
		/* might add some other verify stuff here */
		
		verify_socket_list(ntree, &node->inputs, ntype->inputs);
		verify_socket_list(ntree, &node->outputs, ntype->outputs);
	}
}

void ntreeVerifyTypes(bNodeTree *ntree)
{
	bNode *node;
	
	if((ntree->init & NTREE_TYPE_INIT)==0)
		ntreeInitTypes(ntree);
	
	/* check inputs and outputs, and remove or insert them */
	for(node= ntree->nodes.first; node; node= node->next)
		nodeVerifyType(ntree, node);
	
}

#pragma mark /* ************** Group stuff ********** */

bNodeType node_group_typeinfo= {
	/* type code   */	NODE_GROUP,
	/* name        */	"Group",
	/* width+range */	120, 60, 200,
	/* class+opts  */	NODE_CLASS_GROUP, NODE_OPTIONS,
	/* input sock  */	NULL,
	/* output sock */	NULL,
	/* storage     */	"",
	/* execfunc    */	NULL,
	
};

/* tag internal sockets */
static void group_tag_internal_sockets(bNodeTree *ngroup)
{
	bNode *node;
	bNodeSocket *sock;
	bNodeLink *link;
	
	/* clear intern tag, but check already for hidden sockets */
	for(node= ngroup->nodes.first; node; node= node->next) {
		for(sock= node->inputs.first; sock; sock= sock->next)
			sock->intern= sock->flag & SOCK_HIDDEN;
		for(sock= node->outputs.first; sock; sock= sock->next)
			sock->intern= sock->flag & SOCK_HIDDEN;
	}
	/* set tag */
	for(link= ngroup->links.first; link; link= link->next) {
		link->fromsock->intern= 1;
		link->tosock->intern= 1;
	}
	
	/* remove link pointer to external links (only happens on create group) */
	for(node= ngroup->nodes.first; node; node= node->next) {
		for(sock= node->inputs.first; sock; sock= sock->next)
			if(sock->intern==0)
				sock->link= NULL;
	}

	/* set all intern sockets to own_index zero, makes sure that later use won't mixup */
	for(node= ngroup->nodes.first; node; node= node->next) {
		for(sock= node->inputs.first; sock; sock= sock->next)
			if(sock->intern)
				sock->own_index= 0;
		for(sock= node->outputs.first; sock; sock= sock->next)
			if(sock->intern)
				sock->own_index= 0;
	}
}

/* after editing group, new sockets are zero */
/* this routine ensures unique identifiers for zero sockets that are exposed */
static void group_verify_own_indices(bNodeTree *ngroup)
{
	bNode *node;
	bNodeSocket *sock;
	
	for(node= ngroup->nodes.first; node; node= node->next) {
		for(sock= node->inputs.first; sock; sock= sock->next)
			if(sock->own_index==0 && sock->intern==0)
				sock->own_index= ++(ngroup->cur_index);
		for(sock= node->outputs.first; sock; sock= sock->next)
			if(sock->own_index==0 && sock->intern==0)
				sock->own_index= ++(ngroup->cur_index);
	}
	//printf("internal index %d\n", ngroup->cur_index);
}


/* nodetrees can be used as groups, so we need typeinfo structs generated */
void ntreeMakeOwnType(bNodeTree *ngroup)
{
	bNode *node;
	bNodeSocket *sock;
	int totin= 0, totout=0, a;

	/* tags socket when internal linked */
	group_tag_internal_sockets(ngroup);
	
	/* ensure all sockets have own unique id */
	group_verify_own_indices(ngroup);
	
	/* counting stats */
	for(node= ngroup->nodes.first; node; node= node->next) {
		if(node->type==NODE_GROUP)
			break;
		for(sock= node->inputs.first; sock; sock= sock->next)
			if(sock->intern==0) 
				totin++;
		for(sock= node->outputs.first; sock; sock= sock->next)
			if(sock->intern==0) 
				totout++;
	}
	/* debug: nodetrees in nodetrees not handled yet */
	if(node) {
		printf("group in group, not supported yet\n");
		return;
	}
	
	/* free own type struct */
	if(ngroup->owntype) {
		if(ngroup->owntype->inputs)
			MEM_freeN(ngroup->owntype->inputs);
		if(ngroup->owntype->outputs)
			MEM_freeN(ngroup->owntype->outputs);
		MEM_freeN(ngroup->owntype);
	}
	
	/* make own type struct */
	ngroup->owntype= MEM_mallocN(sizeof(bNodeType), "group type");
	*ngroup->owntype= node_group_typeinfo;
	
	/* input type arrays */
	if(totin) {
		bNodeSocketType *stype;
		bNodeSocketType *inputs= MEM_mallocN(sizeof(bNodeSocketType)*(totin+1), "bNodeSocketType");
		a= 0;
		
		for(node= ngroup->nodes.first; node; node= node->next) {
			/* nodes are presumed fully verified, stype and socket list are in sync */
			stype= node->typeinfo->inputs;
			for(sock= node->inputs.first; sock; sock= sock->next, stype++) {
				if(sock->intern==0) {
					/* debug only print */
					if(stype==NULL || stype->type==-1) printf("group verification error %s\n", ngroup->id.name);
					
					inputs[a]= *stype;
					inputs[a].own_index= sock->own_index;
					inputs[a].internsock= sock;	
					a++;
				}
			}
		}
		inputs[a].type= -1;	/* terminator code */
		ngroup->owntype->inputs= inputs;
	}	
	
	/* output type arrays */
	if(totout) {
		bNodeSocketType *stype;
		bNodeSocketType *outputs= MEM_mallocN(sizeof(bNodeSocketType)*(totout+1), "bNodeSocketType");
		a= 0;
		
		for(node= ngroup->nodes.first; node; node= node->next) {
			/* nodes are presumed fully verified, stype and socket list are in sync */
			stype= node->typeinfo->outputs;
			for(sock= node->outputs.first; sock; sock= sock->next, stype++) {
				if(sock->intern==0) {
					/* debug only print */
					if(stype==NULL || stype->type==-1) printf("group verification error %s\n", ngroup->id.name);
					
					outputs[a]= *stype;
					outputs[a].own_index= sock->own_index;
					outputs[a].internsock= sock;	
					a++;
				}
			}
		}
		outputs[a].type= -1;	/* terminator code */
		ngroup->owntype->outputs= outputs;
	}
	
	/* voila, the nodetree has the full definition for generating group-node instances! */
}


static bNodeSocket *groupnode_find_tosock(bNode *gnode, int index)
{
	bNodeSocket *sock;
	
	for(sock= gnode->inputs.first; sock; sock= sock->next)
		if(sock->to_index==index)
			return sock;
	return NULL;
}

static bNodeSocket *groupnode_find_fromsock(bNode *gnode, int index)
{
	bNodeSocket *sock;
	
	for(sock= gnode->outputs.first; sock; sock= sock->next)
		if(sock->to_index==index)
			return sock;
	return NULL;
}

bNode *nodeMakeGroupFromSelected(bNodeTree *ntree)
{
	bNodeLink *link, *linkn;
	bNode *node, *gnode, *nextn;
	bNodeSocket *sock;
	bNodeTree *ngroup;
	float min[2], max[2];
	int totnode=0;
	
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
		if(link->fromnode->flag & NODE_SELECT)
			link->tonode->done |= 1;
		if(link->tonode->flag & NODE_SELECT)
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
	ngroup= alloc_libblock(&G.main->nodetree, ID_NT, "NodeGroup");
	ngroup->type= ntree->type;
	ngroup->alltypes= ntree->alltypes;
	
	/* move nodes over */
	for(node= ntree->nodes.first; node; node= nextn) {
		nextn= node->next;
		if(node->flag & NODE_SELECT) {
			BLI_remlink(&ntree->nodes, node);
			BLI_addtail(&ngroup->nodes, node);
			node->locx-= 0.5f*(min[0]+max[0]);
			node->locy-= 0.5f*(min[1]+max[1]);
		}
	}

	/* move links over */
	for(link= ntree->links.first; link; link= linkn) {
		linkn= link->next;
		if(link->fromnode->flag & link->tonode->flag & NODE_SELECT) {
			BLI_remlink(&ntree->links, link);
			BLI_addtail(&ngroup->links, link);
		}
	}
	
	/* now we can make own group typeinfo */
	ntreeMakeOwnType(ngroup);
	
	/* make group node */
	gnode= nodeAddNodeType(ntree, NODE_GROUP, ngroup);
	gnode->locx= 0.5f*(min[0]+max[0]);
	gnode->locy= 0.5f*(min[1]+max[1]);
	
	/* relink external sockets */
	for(link= ntree->links.first; link; link= linkn) {
		linkn= link->next;
		
		if(link->tonode->flag & NODE_SELECT) {
			link->tonode= gnode;
			sock= groupnode_find_tosock(gnode, link->tosock->own_index);
			if(sock==NULL) {
				nodeRemLink(ntree, link);	
				printf("Removed link, cannot mix internal and external sockets in group\n");
			}
			else link->tosock= sock;
		}
		else if(link->fromnode->flag & NODE_SELECT) {
			link->fromnode= gnode;
			sock= groupnode_find_fromsock(gnode, link->fromsock->own_index);
			if(sock==NULL) {
				nodeRemLink(ntree, link);	
				printf("Removed link, cannot mix internal and external sockets in group\n");
			}
			else link->fromsock= sock;
		}
	}
	
	return gnode;
}

/* note: ungroup: group_indices zero! */

/* here's a nasty little one, need to check users... */
/* should become callbackable... */
void nodeVerifyGroup(bNodeTree *ngroup)
{
	
	/* group changed, so we rebuild the type definition */
	ntreeMakeOwnType(ngroup);
	
	if(ngroup->type==NTREE_SHADER) {
		Material *ma;
		for(ma= G.main->mat.first; ma; ma= ma->id.next) {
			if(ma->nodetree) {
				bNode *node;
				
				/* find if group is in tree */
				for(node= ma->nodetree->nodes.first; node; node= node->next)
					if(node->id == (ID *)ngroup)
						break;
				
				if(node) {
					/* set all type pointers OK */
					ntreeInitTypes(ma->nodetree);
					
					for(node= ma->nodetree->nodes.first; node; node= node->next)
						if(node->id == (ID *)ngroup)
							nodeVerifyType(ma->nodetree, node);
				}
			}
		}
	}
	else if(ngroup->type==NTREE_COMPOSIT) {
		Scene *sce;
		for(sce= G.main->scene.first; sce; sce= sce->id.next) {
			if(sce->nodetree) {
				bNode *node;
				
				/* find if group is in tree */
				for(node= sce->nodetree->nodes.first; node; node= node->next)
					if(node->id == (ID *)ngroup)
						break;
				
				if(node) {
					/* set all type pointers OK */
					ntreeInitTypes(sce->nodetree);
					
					for(node= sce->nodetree->nodes.first; node; node= node->next)
						if(node->id == (ID *)ngroup)
							nodeVerifyType(sce->nodetree, node);
				}
			}
		}
	}
}

/* also to check all users of groups. Now only used in editor for hide/unhide */
/* should become callbackable? */
void nodeGroupSocketUseFlags(bNodeTree *ngroup)
{
	bNode *node;
	bNodeSocket *sock;

	/* clear flags */
	for(node= ngroup->nodes.first; node; node= node->next) {
		for(sock= node->inputs.first; sock; sock= sock->next)
			sock->flag &= ~SOCK_IN_USE;
		for(sock= node->outputs.first; sock; sock= sock->next)
			sock->flag &= ~SOCK_IN_USE;
	}
	
	/* tag all thats in use */
	if(ngroup->type==NTREE_SHADER) {
		Material *ma;
		for(ma= G.main->mat.first; ma; ma= ma->id.next) {
			if(ma->nodetree) {
				for(node= ma->nodetree->nodes.first; node; node= node->next) {
					if(node->id==(ID *)ngroup) {
						for(sock= node->inputs.first; sock; sock= sock->next)
							if(sock->link)
								if(sock->tosock) 
									sock->tosock->flag |= SOCK_IN_USE;
						for(sock= node->outputs.first; sock; sock= sock->next)
							if(nodeCountSocketLinks(ma->nodetree, sock))
								if(sock->tosock) 
									sock->tosock->flag |= SOCK_IN_USE;
					}
				}
			}
		}
	}
	else if(ngroup->type==NTREE_COMPOSIT) {
		Scene *sce;
		for(sce= G.main->scene.first; sce; sce= sce->id.next) {
			if(sce->nodetree) {
				for(node= sce->nodetree->nodes.first; node; node= node->next) {
					if(node->id==(ID *)ngroup) {
						for(sock= node->inputs.first; sock; sock= sock->next)
							if(sock->link)
								if(sock->tosock) 
									sock->tosock->flag |= SOCK_IN_USE;
						for(sock= node->outputs.first; sock; sock= sock->next)
							if(nodeCountSocketLinks(sce->nodetree, sock))
								if(sock->tosock) 
									sock->tosock->flag |= SOCK_IN_USE;
					}
				}
			}
		}
	}
}

static void find_node_with_socket(bNodeTree *ntree, bNodeSocket *sock, bNode **nodep, int *sockindex)
{
	bNode *node;
	bNodeSocket *tsock;
	int index;
	
	for(node= ntree->nodes.first; node; node= node->next) {
		for(index=0, tsock= node->inputs.first; tsock; tsock= tsock->next, index++)
			if(tsock==sock)
				break;
		if(tsock)
			break;
		for(index=0, tsock= node->outputs.first; tsock; tsock= tsock->next, index++)
			if(tsock==sock)
				break;
		if(tsock)
			break;
	}
	if(node) {
		*nodep= node;
		*sockindex= index;
	}
	else {
		*nodep= NULL;
	}
}

/* returns 1 if its OK */
int nodeGroupUnGroup(bNodeTree *ntree, bNode *gnode)
{
	bNodeLink *link, *linkn;
	bNode *node, *nextn;
	bNodeTree *ngroup, *wgroup;
	int index;
	
	ngroup= (bNodeTree *)gnode->id;
	if(ngroup==NULL) return 0;
	
	/* clear new pointers, set in copytree */
	for(node= ntree->nodes.first; node; node= node->next)
		node->new= NULL;

	wgroup= ntreeCopyTree(ngroup, 0);
	
	/* add the nodes into the ntree */
	for(node= wgroup->nodes.first; node; node= nextn) {
		nextn= node->next;
		BLI_remlink(&wgroup->nodes, node);
		BLI_addtail(&ntree->nodes, node);
		node->locx+= gnode->locx;
		node->locy+= gnode->locy;
		node->flag |= NODE_SELECT;
	}
	/* and the internal links */
	for(link= wgroup->links.first; link; link= linkn) {
		linkn= link->next;
		BLI_remlink(&wgroup->links, link);
		BLI_addtail(&ntree->links, link);
	}

	/* restore links to and from the gnode */
	for(link= ntree->links.first; link; link= link->next) {
		if(link->tonode==gnode) {
			/* link->tosock->tosock is on the node we look for */
			find_node_with_socket(ngroup, link->tosock->tosock, &nextn, &index);
			if(nextn==NULL) printf("wrong stuff!\n");
			else if(nextn->new==NULL) printf("wrong stuff too!\n");
			else {
				link->tonode= nextn->new;
				link->tosock= BLI_findlink(&link->tonode->inputs, index);
			}
		}
		else if(link->fromnode==gnode) {
			/* link->fromsock->tosock is on the node we look for */
			find_node_with_socket(ngroup, link->fromsock->tosock, &nextn, &index);
			if(nextn==NULL) printf("1 wrong stuff!\n");
			else if(nextn->new==NULL) printf("1 wrong stuff too!\n");
			else {
				link->fromnode= nextn->new;
				link->fromsock= BLI_findlink(&link->fromnode->outputs, index);
			}
		}
	}
	
	/* remove the gnode & work tree */
	free_libblock(&G.main->nodetree, wgroup);
	
	nodeFreeNode(ntree, gnode);
	
	return 1;
}

#pragma mark /* ************** Add stuff ********** */

bNode *nodeAddNodeType(bNodeTree *ntree, int type, bNodeTree *ngroup)
{
	bNode *node;
	bNodeType *ntype= node_get_type(ntree, type, ngroup);
	bNodeSocketType *stype;
	
	node= MEM_callocN(sizeof(bNode), "new node");
	BLI_addtail(&ntree->nodes, node);
	node->typeinfo= ntype;
	
	if(ngroup)
		BLI_strncpy(node->name, ngroup->id.name+2, NODE_MAXSTR);
	else
		BLI_strncpy(node->name, ntype->name, NODE_MAXSTR);
	node->type= ntype->type;
	node->flag= NODE_SELECT|ntype->flag;
	node->width= ntype->width;
	node->miniwidth= 15.0f;		/* small value only, allows print of first chars */
	
	if(type==NODE_GROUP)
		node->id= (ID *)ngroup;
	
	if(ntype->inputs) {
		stype= ntype->inputs;
		while(stype->type != -1) {
			node_add_socket_type(&node->inputs, stype);
			stype++;
		}
	}
	if(ntype->outputs) {
		stype= ntype->outputs;
		while(stype->type != -1) {
			node_add_socket_type(&node->outputs, stype);
			stype++;
		}
	}
	
	/* need init handler later? */
	if(ntree->type==NTREE_SHADER) {
		if(type==SH_NODE_MATERIAL)
			node->custom1= SH_NODE_MAT_DIFF|SH_NODE_MAT_SPEC;
		else if(type==SH_NODE_VALTORGB)
			node->storage= add_colorband(1);
		else if(type==SH_NODE_MAPPING)
			node->storage= add_mapping();
		else if(type==SH_NODE_CURVE_VEC)
			node->storage= curvemapping_add(3, -1.0f, -1.0f, 1.0f, 1.0f);
		else if(type==SH_NODE_CURVE_RGB)
			node->storage= curvemapping_add(4, 0.0f, 0.0f, 1.0f, 1.0f);
	}
	else if(ntree->type==NTREE_COMPOSIT) {
		if(type==CMP_NODE_VALTORGB)
			node->storage= add_colorband(1);
		else if(type==CMP_NODE_CURVE_VEC)
			node->storage= curvemapping_add(3, -1.0f, -1.0f, 1.0f, 1.0f);
		else if(type==CMP_NODE_CURVE_RGB)
			node->storage= curvemapping_add(4, 0.0f, 0.0f, 1.0f, 1.0f);
		else if(type==CMP_NODE_TIME) {
			node->custom1= G.scene->r.sfra;
			node->custom2= G.scene->r.efra;
			node->storage= curvemapping_add(1, 0.0f, 0.0f, 1.0f, 1.0f);
		}
		else if(type==CMP_NODE_MAP_VALUE)
			node->storage= add_mapping();
		else if(type==CMP_NODE_BLUR)
			node->storage= MEM_callocN(sizeof(NodeBlurData), "node blur data");
		else if(type==CMP_NODE_VECBLUR) {
			NodeBlurData *nbd= MEM_callocN(sizeof(NodeBlurData), "node blur data");
			node->storage= nbd;
			nbd->samples= 32;
			nbd->fac= 1.0f;
		}
	}
	
	return node;
}

/* keep socket listorder identical, for copying links */
/* ntree is the target tree */
bNode *nodeCopyNode(struct bNodeTree *ntree, struct bNode *node)
{
	bNode *nnode= MEM_callocN(sizeof(bNode), "dupli node");
	bNodeSocket *sock;

	*nnode= *node;
	BLI_addtail(&ntree->nodes, nnode);
	
	duplicatelist(&nnode->inputs, &node->inputs);
	for(sock= nnode->inputs.first; sock; sock= sock->next)
		sock->own_index= 0;
	
	duplicatelist(&nnode->outputs, &node->outputs);
	for(sock= nnode->outputs.first; sock; sock= sock->next) {
		sock->own_index= 0;
		sock->ns.data= NULL;
	}
	
	if(nnode->id)
		nnode->id->us++;
	
	if(nnode->storage) {
		/* another candidate for handlerizing! */
		if(ntree->type==NTREE_SHADER) {
			if(node->type==SH_NODE_CURVE_VEC || node->type==SH_NODE_CURVE_RGB)
				nnode->storage= curvemapping_copy(node->storage);
			else 
				nnode->storage= MEM_dupallocN(nnode->storage);
		}
		else if(ntree->type==NTREE_COMPOSIT) {
			if(ELEM3(node->type, CMP_NODE_TIME, CMP_NODE_CURVE_VEC, CMP_NODE_CURVE_RGB))
				nnode->storage= curvemapping_copy(node->storage);
			else 
				nnode->storage= MEM_dupallocN(nnode->storage);
		}
		else 
			nnode->storage= MEM_dupallocN(nnode->storage);
	}
	
	node->new= nnode;
	nnode->new= NULL;
	nnode->preview= NULL;
	
	return nnode;
}

bNodeLink *nodeAddLink(bNodeTree *ntree, bNode *fromnode, bNodeSocket *fromsock, bNode *tonode, bNodeSocket *tosock)
{
	bNodeLink *link= MEM_callocN(sizeof(bNodeLink), "link");
	
	BLI_addtail(&ntree->links, link);
	link->fromnode= fromnode;
	link->fromsock= fromsock;
	link->tonode= tonode;
	link->tosock= tosock;
	
	return link;
}

void nodeRemLink(bNodeTree *ntree, bNodeLink *link)
{
	BLI_remlink(&ntree->links, link);
	if(link->tosock)
		link->tosock->link= NULL;
	MEM_freeN(link);
}


bNodeTree *ntreeAddTree(int type)
{
	bNodeTree *ntree= MEM_callocN(sizeof(bNodeTree), "new node tree");
	ntree->type= type;
	
	ntreeInitTypes(ntree);
	return ntree;
}

bNodeTree *ntreeCopyTree(bNodeTree *ntree, int internal_select)
{
	bNodeTree *newtree;
	bNode *node, *nnode, *last;
	bNodeLink *link, *nlink;
	bNodeSocket *sock;
	int a;
	
	if(ntree==NULL) return NULL;
	
	if(internal_select==0) {
		/* is ntree part of library? */
		for(newtree=G.main->nodetree.first; newtree; newtree= newtree->id.next)
			if(newtree==ntree) break;
		if(newtree)
			newtree= copy_libblock(ntree);
		else
			newtree= MEM_dupallocN(ntree);
		newtree->nodes.first= newtree->nodes.last= NULL;
		newtree->links.first= newtree->links.last= NULL;
	}
	else
		newtree= ntree;
	
	last= ntree->nodes.last;
	for(node= ntree->nodes.first; node; node= node->next) {
		
		node->new= NULL;
		if(internal_select==0 || (node->flag & NODE_SELECT)) {
			nnode= nodeCopyNode(newtree, node);	/* sets node->new */
			if(internal_select) {
				node->flag &= ~NODE_SELECT;
				nnode->flag |= NODE_SELECT;
			}
			node->flag &= ~NODE_ACTIVE;
		}
		if(node==last) break;
	}
	
	/* check for copying links */
	for(link= ntree->links.first; link; link= link->next) {
		if(link->fromnode->new && link->tonode->new) {
			nlink= nodeAddLink(newtree, link->fromnode->new, NULL, link->tonode->new, NULL);
			/* sockets were copied in order */
			for(a=0, sock= link->fromnode->outputs.first; sock; sock= sock->next, a++) {
				if(sock==link->fromsock)
					break;
			}
			nlink->fromsock= BLI_findlink(&link->fromnode->new->outputs, a);
			
			for(a=0, sock= link->tonode->inputs.first; sock; sock= sock->next, a++) {
				if(sock==link->tosock)
					break;
			}
			nlink->tosock= BLI_findlink(&link->tonode->new->inputs, a);
		}
	}
	
	/* own type definition for group usage */
	if(internal_select==0) {
		if(ntree->owntype) {
			newtree->owntype= MEM_dupallocN(ntree->owntype);
			if(ntree->owntype->inputs)
				newtree->owntype->inputs= MEM_dupallocN(ntree->owntype->inputs);
			if(ntree->owntype->outputs)
				newtree->owntype->outputs= MEM_dupallocN(ntree->owntype->outputs);
		}
	}	
	return newtree;
}

#pragma mark /* ************** Free stuff ********** */

/* goes over entire tree */
static void node_unlink_node(bNodeTree *ntree, bNode *node)
{
	bNodeLink *link, *next;
	bNodeSocket *sock;
	ListBase *lb;
	
	for(link= ntree->links.first; link; link= next) {
		next= link->next;
		
		if(link->fromnode==node) {
			lb= &node->outputs;
			NodeTagChanged(ntree, link->tonode);
		}
		else if(link->tonode==node)
			lb= &node->inputs;
		else
			lb= NULL;

		if(lb) {
			for(sock= lb->first; sock; sock= sock->next) {
				if(link->fromsock==sock || link->tosock==sock)
					break;
			}
			if(sock) {
				nodeRemLink(ntree, link);
			}
		}
	}
}

static void composit_free_node_cache(bNode *node)
{
	bNodeSocket *sock;
	
	for(sock= node->outputs.first; sock; sock= sock->next) {
		if(sock->ns.data) {
			free_compbuf(sock->ns.data);
			sock->ns.data= NULL;
		}
	}
}

void nodeFreeNode(bNodeTree *ntree, bNode *node)
{
	node_unlink_node(ntree, node);
	BLI_remlink(&ntree->nodes, node);

	if(node->id)
		node->id->us--;
	
	if(ntree->type==NTREE_COMPOSIT)
		composit_free_node_cache(node);
	BLI_freelistN(&node->inputs);
	BLI_freelistN(&node->outputs);
	
	if(node->preview) {
		if(node->preview->rect)
			MEM_freeN(node->preview->rect);
		MEM_freeN(node->preview);
	}
	if(node->storage) {
		/* could be handlerized at some point, now only 1 exception still */
		if(ntree->type==NTREE_SHADER) {
			if(node->type==SH_NODE_CURVE_VEC || node->type==SH_NODE_CURVE_RGB)
				curvemapping_free(node->storage);
			else 
				MEM_freeN(node->storage);
		}
		else if(ntree->type==NTREE_COMPOSIT) {
			if(ELEM3(node->type, CMP_NODE_TIME, CMP_NODE_CURVE_VEC, CMP_NODE_CURVE_RGB))
				curvemapping_free(node->storage);
			else 
				MEM_freeN(node->storage);
		}
		else 
			MEM_freeN(node->storage);
	}
	MEM_freeN(node);
}

/* do not free ntree itself here, free_libblock calls this function too */
void ntreeFreeTree(bNodeTree *ntree)
{
	bNode *node, *next;
	
	if(ntree==NULL) return;
	
	BLI_freelistN(&ntree->links);	/* do first, then unlink_node goes fast */
	
	for(node= ntree->nodes.first; node; node= next) {
		next= node->next;
		nodeFreeNode(ntree, node);
	}
	
	if(ntree->owntype) {
		if(ntree->owntype->inputs)
			MEM_freeN(ntree->owntype->inputs);
		if(ntree->owntype->outputs)
			MEM_freeN(ntree->owntype->outputs);
		MEM_freeN(ntree->owntype);
	}
}

void ntreeFreeCache(bNodeTree *ntree)
{
	bNode *node;
	
	if(ntree==NULL) return;

	if(ntree->type==NTREE_COMPOSIT)
		for(node= ntree->nodes.first; node; node= node->next)
			composit_free_node_cache(node);

}

void ntreeMakeLocal(bNodeTree *ntree)
{
	int local=0, lib=0;
	
	/* - only lib users: do nothing
	    * - only local users: set flag
	    * - mixed: make copy
	    */
	
	if(ntree->id.lib==NULL) return;
	if(ntree->id.us==1) {
		ntree->id.lib= 0;
		ntree->id.flag= LIB_LOCAL;
		new_id(0, (ID *)ntree, 0);
		return;
	}
	
	/* now check users of groups... again typedepending, callback... */
	if(ntree->type==NTREE_SHADER) {
		Material *ma;
		for(ma= G.main->mat.first; ma; ma= ma->id.next) {
			if(ma->nodetree) {
				bNode *node;
				
				/* find if group is in tree */
				for(node= ma->nodetree->nodes.first; node; node= node->next) {
					if(node->id == (ID *)ntree) {
						if(ma->id.lib) lib= 1;
						else local= 1;
					}
				}
			}
		}
	}
	else if(ntree->type==NTREE_COMPOSIT) {
		Scene *sce;
		for(sce= G.main->scene.first; sce; sce= sce->id.next) {
			if(sce->nodetree) {
				bNode *node;
				
				/* find if group is in tree */
				for(node= sce->nodetree->nodes.first; node; node= node->next) {
					if(node->id == (ID *)ntree) {
						if(sce->id.lib) lib= 1;
						else local= 1;
					}
				}
			}
		}
	}
	
	/* if all users are local, we simply make tree local */
	if(local && lib==0) {
		ntree->id.lib= NULL;
		ntree->id.flag= LIB_LOCAL;
		new_id(0, (ID *)ntree, 0);
	}
	else if(local && lib) {
		/* this is the mixed case, we copy the tree and assign it to local users */
		bNodeTree *newtree= ntreeCopyTree(ntree, 0);
		
		newtree->id.us= 0;
		
		if(ntree->type==NTREE_SHADER) {
			Material *ma;
			for(ma= G.main->mat.first; ma; ma= ma->id.next) {
				if(ma->nodetree) {
					bNode *node;
					
					/* find if group is in tree */
					for(node= ma->nodetree->nodes.first; node; node= node->next) {
						if(node->id == (ID *)ntree) {
							if(ma->id.lib==NULL) {
								node->id= &newtree->id;
								newtree->id.us++;
								ntree->id.us--;
							}
						}
					}
				}
			}
		}
		else if(ntree->type==NTREE_COMPOSIT) {
			Scene *sce;
			for(sce= G.main->scene.first; sce; sce= sce->id.next) {
				if(sce->nodetree) {
					bNode *node;
					
					/* find if group is in tree */
					for(node= sce->nodetree->nodes.first; node; node= node->next) {
						if(node->id == (ID *)ntree) {
							if(sce->id.lib==NULL) {
								node->id= &newtree->id;
								newtree->id.us++;
								ntree->id.us--;
							}
						}
					}
				}
			}
		}
	}
}


#pragma mark /* ************ find stuff *************** */

bNodeLink *nodeFindLink(bNodeTree *ntree, bNodeSocket *from, bNodeSocket *to)
{
	bNodeLink *link;
	
	for(link= ntree->links.first; link; link= link->next) {
		if(link->fromsock==from && link->tosock==to)
			return link;
		if(link->fromsock==to && link->tosock==from)	/* hrms? */
			return link;
	}
	return NULL;
}

int nodeCountSocketLinks(bNodeTree *ntree, bNodeSocket *sock)
{
	bNodeLink *link;
	int tot= 0;
	
	for(link= ntree->links.first; link; link= link->next) {
		if(link->fromsock==sock || link->tosock==sock)
			tot++;
	}
	return tot;
}

bNode *nodeGetActive(bNodeTree *ntree)
{
	bNode *node;
	
	if(ntree==NULL) return NULL;
	
	for(node= ntree->nodes.first; node; node= node->next)
		if(node->flag & NODE_ACTIVE)
			break;
	return node;
}

/* two active flags, ID nodes have special flag for buttons display */
bNode *nodeGetActiveID(bNodeTree *ntree, short idtype)
{
	bNode *node;
	
	if(ntree==NULL) return NULL;
	
	for(node= ntree->nodes.first; node; node= node->next)
		if(node->id && GS(node->id->name)==idtype)
			if(node->flag & NODE_ACTIVE_ID)
				break;
	return node;
}

/* two active flags, ID nodes have special flag for buttons display */
void nodeClearActiveID(bNodeTree *ntree, short idtype)
{
	bNode *node;
	
	if(ntree==NULL) return;
	
	for(node= ntree->nodes.first; node; node= node->next)
		if(node->id && GS(node->id->name)==idtype)
			node->flag &= ~NODE_ACTIVE_ID;
}

/* two active flags, ID nodes have special flag for buttons display */
void nodeSetActive(bNodeTree *ntree, bNode *node)
{
	bNode *tnode;
	
	/* make sure only one node is active, and only one per ID type */
	for(tnode= ntree->nodes.first; tnode; tnode= tnode->next) {
		tnode->flag &= ~NODE_ACTIVE;
		
		if(node->id && tnode->id) {
			if(GS(node->id->name) == GS(tnode->id->name))
				tnode->flag &= ~NODE_ACTIVE_ID;
		}
	}
	
	node->flag |= NODE_ACTIVE;
	if(node->id)
		node->flag |= NODE_ACTIVE_ID;
}

/* use flags are not persistant yet, groups might need different tagging, so we do it each time
   when we need to get this info */
void ntreeSocketUseFlags(bNodeTree *ntree)
{
	bNode *node;
	bNodeSocket *sock;
	bNodeLink *link;
	
	/* clear flags */
	for(node= ntree->nodes.first; node; node= node->next) {
		for(sock= node->inputs.first; sock; sock= sock->next)
			sock->flag &= ~SOCK_IN_USE;
		for(sock= node->outputs.first; sock; sock= sock->next)
			sock->flag &= ~SOCK_IN_USE;
	}
	
	/* tag all thats in use */
	for(link= ntree->links.first; link; link= link->next) {
		link->fromsock->flag |= SOCK_IN_USE;
		link->tosock->flag |= SOCK_IN_USE;
	}
}

#pragma mark /* ************** dependency stuff *********** */

/* node is guaranteed to be not checked before */
static int node_recurs_check(bNode *node, bNode ***nsort, int level)
{
	bNode *fromnode;
	bNodeSocket *sock;
	int has_inputlinks= 0;
	
	node->done= 1;
	level++;
	
	for(sock= node->inputs.first; sock; sock= sock->next) {
		if(sock->link) {
			has_inputlinks= 1;
			fromnode= sock->link->fromnode;
			if(fromnode->done==0) {
				fromnode->level= node_recurs_check(fromnode, nsort, level);
			}
		}
	}
//	printf("node sort %s level %d\n", node->name, level);
	**nsort= node;
	(*nsort)++;
	
	if(has_inputlinks)
		return level;
	else 
		return 0xFFF;
}

void ntreeSolveOrder(bNodeTree *ntree)
{
	bNode *node, **nodesort, **nsort;
	bNodeSocket *sock;
	bNodeLink *link;
	int a, totnode=0;
	
	/* the solve-order is called on each tree change, so we should be sure no exec can be running */
	ntreeEndExecTree(ntree);

	/* set links pointers the input sockets, to find dependencies */
	/* first clear data */
	for(node= ntree->nodes.first; node; node= node->next) {
		node->done= 0;
		totnode++;
		for(sock= node->inputs.first; sock; sock= sock->next)
			sock->link= NULL;
	}
	if(totnode==0)
		return;
	
	for(link= ntree->links.first; link; link= link->next) {
		link->tosock->link= link;
	}
	
	nsort= nodesort= MEM_callocN(totnode*sizeof(void *), "sorted node array");
	
	/* recursive check */
	for(node= ntree->nodes.first; node; node= node->next) {
		if(node->done==0) {
			node->level= node_recurs_check(node, &nsort, 0);
		}
	}
	
	/* re-insert nodes in order, first a paranoia check */
	for(a=0; a<totnode; a++) {
		if(nodesort[a]==NULL)
			break;
	}
	if(a<totnode)
		printf("sort error in node tree");
	else {
		ntree->nodes.first= ntree->nodes.last= NULL;
		for(a=0; a<totnode; a++)
			BLI_addtail(&ntree->nodes, nodesort[a]);
	}
	
	MEM_freeN(nodesort);
	
	/* find the active outputs, might become tree type dependant handler */
	for(node= ntree->nodes.first; node; node= node->next) {
		if(node->typeinfo->nclass==NODE_CLASS_OUTPUT) {
			bNode *tnode;
			int output= 0;
			/* there is more types having output class, each one is checked */
			for(tnode= ntree->nodes.first; tnode; tnode= tnode->next) {
				if(tnode->typeinfo->nclass==NODE_CLASS_OUTPUT) {
					if(tnode->type==node->type) {
						if(tnode->flag & NODE_DO_OUTPUT) {
							if(output>1)
								tnode->flag &= ~NODE_DO_OUTPUT;
							output++;
						}
					}
				}
			}
			if(output==0)
				node->flag |= NODE_DO_OUTPUT;
		}
	}
	
	/* here we could recursively set which nodes have to be done,
		might be different for editor or for "real" use... */
}

/* should be callback! */
void NodeTagChanged(bNodeTree *ntree, bNode *node)
{
	if(ntree->type==NTREE_COMPOSIT) {
		bNodeSocket *sock;

		for(sock= node->outputs.first; sock; sock= sock->next) {
			if(sock->ns.data) {
				free_compbuf(sock->ns.data);
				sock->ns.data= NULL;
			}
		}
		node->need_exec= 1;
	}
}

#pragma mark /* *************** preview *********** */

/* if node->preview, then we assume the rect to exist */

static void nodeInitPreview(bNode *node, int xsize, int ysize)
{
	
	if(node->preview==NULL) {
		node->preview= MEM_callocN(sizeof(bNodePreview), "node preview");
		printf("added preview %s\n", node->name);
	}
	
	/* node previews can get added with variable size this way */
	if(xsize==0 || ysize==0)
		return;
	
	/* sanity checks & initialize */
	if(node->preview && node->preview->rect) {
		if(node->preview->xsize!=xsize && node->preview->ysize!=ysize) {
			MEM_freeN(node->preview->rect);
			node->preview->rect= NULL;
		}
	}
	
	if(node->preview->rect==NULL) {
		node->preview->rect= MEM_callocN(4*xsize + xsize*ysize*sizeof(float)*4, "node preview rect");
		node->preview->xsize= xsize;
		node->preview->ysize= ysize;
	}
}

void ntreeInitPreview(bNodeTree *ntree, int xsize, int ysize)
{
	bNode *node;
	
	if(ntree==NULL)
		return;
	
	for(node= ntree->nodes.first; node; node= node->next) {
		if(node->typeinfo->flag & NODE_PREVIEW)	/* hrms, check for closed nodes? */
			nodeInitPreview(node, xsize, ysize);
		if(node->type==NODE_GROUP && (node->flag & NODE_GROUP_EDIT))
			ntreeInitPreview((bNodeTree *)node->id, xsize, ysize);
	}		
}

void nodeAddToPreview(bNode *node, float *col, int x, int y)
{
	bNodePreview *preview= node->preview;
	if(preview) {
		if(x>=0 && y>=0) {
			if(x<preview->xsize && y<preview->ysize) {
				float *tar= preview->rect+ 4*((preview->xsize*y) + x);
				QUATCOPY(tar, col);
			}
			else printf("prv out bound x y %d %d\n", x, y);
		}
		else printf("prv out bound x y %d %d\n", x, y);
	}
}



#pragma mark /* ******************* executing ************* */

/* see notes at ntreeBeginExecTree */
static void group_node_get_stack(bNode *node, bNodeStack *stack, bNodeStack **in, bNodeStack **out, bNodeStack **gin, bNodeStack **gout)
{
	bNodeSocket *sock;
	
	/* build pointer stack */
	for(sock= node->inputs.first; sock; sock= sock->next) {
		if(sock->intern) {
			/* yep, intern can have link or is hidden socket */
			if(sock->link)
				*(in++)= stack + sock->link->fromsock->stack_index;
			else
				*(in++)= &sock->ns;
		}
		else
			*(in++)= gin[sock->stack_index_ext];
	}
	
	for(sock= node->outputs.first; sock; sock= sock->next) {
		if(sock->intern)
			*(out++)= stack + sock->stack_index;
		else
			*(out++)= gout[sock->stack_index_ext];
	}
}

static void node_group_execute(bNodeStack *stack, void *data, bNode *gnode, bNodeStack **in, bNodeStack **out)
{
	bNode *node;
	bNodeTree *ntree= (bNodeTree *)gnode->id;
	bNodeStack *nsin[MAX_SOCKET];	/* arbitrary... watch this */
	bNodeStack *nsout[MAX_SOCKET];	/* arbitrary... watch this */
	
	if(ntree==NULL) return;
	
	stack+= gnode->stack_index;
		
	for(node= ntree->nodes.first; node; node= node->next) {
		if(node->typeinfo->execfunc) {
			group_node_get_stack(node, stack, nsin, nsout, in, out);
			node->typeinfo->execfunc(data, node, nsin, nsout);
		}
	}
	
	/* free internal group output nodes */
	for(node= ntree->nodes.first; node; node= node->next) {
		if(node->typeinfo->execfunc) {
			bNodeSocket *sock;
			
			for(sock= node->outputs.first; sock; sock= sock->next) {
				if(sock->intern) {
					bNodeStack *ns= stack + sock->stack_index;
					if(ns->data) {
						free_compbuf(ns->data);
						ns->data= NULL;
					}
				}
			}
		}
	}
}

/* recursively called for groups */
/* we set all trees on own local indices, but put a total counter
   in the groups, so each instance of a group has own stack */
static int ntree_begin_exec_tree(bNodeTree *ntree)
{
	bNode *node;
	bNodeSocket *sock;
	int index= 0, index_in= 0, index_out= 0;
	
	if((ntree->init & NTREE_TYPE_INIT)==0)
		ntreeInitTypes(ntree);
	
	/* create indices for stack, check preview */
	for(node= ntree->nodes.first; node; node= node->next) {
		
		for(sock= node->inputs.first; sock; sock= sock->next) {
			if(sock->intern==0)
				sock->stack_index_ext= index_in++;
		}
		
		for(sock= node->outputs.first; sock; sock= sock->next) {
			sock->stack_index= index++;
			if(sock->intern==0)
				sock->stack_index_ext= index_out++;
		}
		
		if(node->type==NODE_GROUP) {
			if(node->id) {
				node->stack_index= index;
				index+= ntree_begin_exec_tree((bNodeTree *)node->id);
			}
		}
	}
	
	return index;
}

/* groups have same node storage data... cannot initialize them while using in threads */
static void composit_begin_exec_groupdata(bNodeTree *ntree)
{
	bNode *node;
	
	for(node= ntree->nodes.first; node; node= node->next) {
		if(ELEM3(node->type, CMP_NODE_TIME, CMP_NODE_CURVE_VEC, CMP_NODE_CURVE_RGB)) {
			curvemapping_initialize(node->storage);
			if(node->type==CMP_NODE_CURVE_RGB)
				curvemapping_premultiply(node->storage, 0);
		}
	}
}

/* copy socket compbufs to stack, initialize group usage of curve nodes */
static void composit_begin_exec(bNodeTree *ntree)
{
	bNode *node;
	
	for(node= ntree->nodes.first; node; node= node->next) {
		bNodeSocket *sock;
		for(sock= node->outputs.first; sock; sock= sock->next) {
			if(sock->ns.data) {
				bNodeStack *ns= ntree->stack + sock->stack_index;

				ns->data= sock->ns.data;
				sock->ns.data= NULL;
			}
		}
		if(node->type==NODE_GROUP)
			composit_begin_exec_groupdata((bNodeTree *)node->id);
	}
}

/* groups have same node storage data... cannot initialize them while using in threads */
static void composit_end_exec_groupdata(bNodeTree *ntree)
{
	bNode *node;
	
	for(node= ntree->nodes.first; node; node= node->next) {
		if(node->type==CMP_NODE_CURVE_RGB)
			curvemapping_premultiply(node->storage, 1);
	}
}


/* copy stack compbufs to sockets */
static void composit_end_exec(bNodeTree *ntree)
{
	extern void print_compbuf(char *str, struct CompBuf *cbuf);
	bNode *node;
	bNodeStack *ns;
	int a;

	for(node= ntree->nodes.first; node; node= node->next) {
		bNodeSocket *sock;
		
		for(sock= node->outputs.first; sock; sock= sock->next) {
			ns= ntree->stack + sock->stack_index;
			if(ns->data) {
				sock->ns.data= ns->data;
				ns->data= NULL;
			}
		}
		if(node->type==NODE_GROUP)
			composit_end_exec_groupdata((bNodeTree *)node->id);

		node->need_exec= 0;
	}
	
	/* internally, group buffers are not stored */
	for(ns= ntree->stack, a=0; a<ntree->stacksize; a++, ns++) {
		if(ns->data) {
			printf("freed leftover buffer from stack\n");
			free_compbuf(ns->data);
		}
	}
}

static void group_tag_used_outputs(bNode *gnode, bNodeStack *stack)
{
	bNodeTree *ntree= (bNodeTree *)gnode->id;
	bNode *node;
	
	stack+= gnode->stack_index;
	
	for(node= ntree->nodes.first; node; node= node->next) {
		if(node->typeinfo->execfunc) {
			bNodeSocket *sock;
			
			for(sock= node->inputs.first; sock; sock= sock->next) {
				if(sock->intern) {
					if(sock->link) {
						bNodeStack *ns= stack + sock->link->fromsock->stack_index;
						ns->hasoutput= 1;
					}
				}
			}
		}
	}
}

/* stack indices make sure all nodes only write in allocated data, for making it thread safe */
/* only root tree gets the stack, to enable instances to have own stack entries */
/* only two threads now! */
/* per tree (and per group) unique indices are created */
/* the index_ext we need to be able to map from groups to the group-node own stack */

void ntreeBeginExecTree(bNodeTree *ntree)
{
	
	/* goes recursive over all groups */
	ntree->stacksize= ntree_begin_exec_tree(ntree);
	
	if(ntree->stacksize) {
		bNode *node;
		bNodeStack *ns;
		int a;
		
		/* allocate stack */
		ns=ntree->stack= MEM_callocN(ntree->stacksize*sizeof(bNodeStack), "node stack");
		
		/* tag inputs, the get_stack() gives own socket stackdata if not in use */
		for(a=0; a<ntree->stacksize; a++, ns++) ns->hasinput= 1;
		
		/* tag used outputs, so we know when we can skip operations */
		for(node= ntree->nodes.first; node; node= node->next) {
			bNodeSocket *sock;
			for(sock= node->inputs.first; sock; sock= sock->next) {
				if(sock->link) {
					ns= ntree->stack + sock->link->fromsock->stack_index;
					ns->hasoutput= 1;
				}
			}
			if(node->type==NODE_GROUP && node->id)
				group_tag_used_outputs(node, ntree->stack);
		}
		if(ntree->type==NTREE_COMPOSIT)
			composit_begin_exec(ntree);
		else
			ntree->stack1= MEM_dupallocN(ntree->stack);
	}
	
	ntree->init |= NTREE_EXEC_INIT;
}

void ntreeEndExecTree(bNodeTree *ntree)
{
	
	if(ntree->init & NTREE_EXEC_INIT) {
		
		/* another callback candidate! */
		if(ntree->type==NTREE_COMPOSIT)
			composit_end_exec(ntree);
		
		if(ntree->stack)
			MEM_freeN(ntree->stack);
		ntree->stack= NULL;
		
		if(ntree->stack1)
			MEM_freeN(ntree->stack1);
		ntree->stack1= NULL;

		ntree->init &= ~NTREE_EXEC_INIT;
	}
}

static void node_get_stack(bNode *node, bNodeStack *stack, bNodeStack **in, bNodeStack **out)
{
	bNodeSocket *sock;
	
	/* build pointer stack */
	for(sock= node->inputs.first; sock; sock= sock->next) {
		if(sock->link)
			*(in++)= stack + sock->link->fromsock->stack_index;
		else
			*(in++)= &sock->ns;
	}
	
	for(sock= node->outputs.first; sock; sock= sock->next) {
		*(out++)= stack + sock->stack_index;
	}
}

/* nodes are presorted, so exec is in order of list */
void ntreeExecTree(bNodeTree *ntree, void *callerdata, int thread)
{
	bNode *node;
	bNodeStack *nsin[MAX_SOCKET];	/* arbitrary... watch this */
	bNodeStack *nsout[MAX_SOCKET];	/* arbitrary... watch this */
	bNodeStack *stack;
	
	/* only when initialized */
	if((ntree->init & NTREE_EXEC_INIT)==0)
		ntreeBeginExecTree(ntree);
		
	if(thread)
		stack= ntree->stack1;
	else
		stack= ntree->stack;
	
	for(node= ntree->nodes.first; node; node= node->next) {
		if(node->typeinfo->execfunc) {
			node_get_stack(node, stack, nsin, nsout);
			node->typeinfo->execfunc(callerdata, node, nsin, nsout);
		}
		else if(node->type==NODE_GROUP && node->id) {
			node_get_stack(node, stack, nsin, nsout);
			node_group_execute(stack, callerdata, node, nsin, nsout); 
		}
	}
}


/* ***************************** threaded version for execute composite nodes ************* */

#define NODE_PROCESSING	1
#define NODE_READY		2
#define NODE_FINISHED	4

/* not changing info, for thread callback */
typedef struct ThreadData {
	bNodeStack *stack;
	RenderData *rd;
} ThreadData;

static int exec_composite_node(void *node_v)
{
	bNodeStack *nsin[MAX_SOCKET];	/* arbitrary... watch this */
	bNodeStack *nsout[MAX_SOCKET];	/* arbitrary... watch this */
	bNode *node= node_v;
	ThreadData *thd= (ThreadData *)node->new;
	
	node_get_stack(node, thd->stack, nsin, nsout);
	
	if(node->typeinfo->execfunc) {
		node->typeinfo->execfunc(thd->rd, node, nsin, nsout);
	}
	else if(node->type==NODE_GROUP && node->id) {
		node_group_execute(thd->stack, thd->rd, node, nsin, nsout); 
	}
	
	node->exec |= NODE_READY;
	return 0;
}

/* return total of executable nodes, for timecursor */
static int setExecutableNodes(bNodeTree *ntree, ThreadData *thd)
{
	bNodeStack *nsin[MAX_SOCKET];	/* arbitrary... watch this */
	bNodeStack *nsout[MAX_SOCKET];	/* arbitrary... watch this */
	bNode *node;
	bNodeSocket *sock;
	int totnode= 0;
	
	for(node= ntree->nodes.first; node; node= node->next) {
		int a;
		
		node_get_stack(node, thd->stack, nsin, nsout);
		
		/* test the inputs */
		for(a=0, sock= node->inputs.first; sock; sock= sock->next, a++) {
			/* is sock in use? */
			if(sock->link) {
				if(nsin[a]->data==NULL || sock->link->fromnode->need_exec) {
					node->need_exec= 1;
					break;
				}
			}
		}
		
		/* test the outputs */
		for(a=0, sock= node->outputs.first; sock; sock= sock->next, a++) {
			if(nsout[a]->data==NULL && nsout[a]->hasoutput) {
				node->need_exec= 1;
				break;
			}
		}
		if(node->need_exec) {
			
			/* free output buffers */
			for(a=0, sock= node->outputs.first; sock; sock= sock->next, a++) {
				if(nsout[a]->data) {
					free_compbuf(nsout[a]->data);
					nsout[a]->data= NULL;
				}
			}
			totnode++;
			//printf("node needs exec %s\n", node->name);
			
			/* tag for getExecutableNode() */
			node->exec= 0;
		}
		else
			/* tag for getExecutableNode() */
			node->exec= NODE_READY|NODE_FINISHED;
	}
	return totnode;
}

static bNode *getExecutableNode(bNodeTree *ntree)
{
	bNode *node;
	bNodeSocket *sock;
	
	for(node= ntree->nodes.first; node; node= node->next) {
		if(node->exec==0) {
			
			/* input sockets should be ready */
			for(sock= node->inputs.first; sock; sock= sock->next) {
				if(sock->link)
					if((sock->link->fromnode->exec & NODE_READY)==0)
						break;
			}
			if(sock==NULL) {
				printf("exec %s\n", node->name);
				return node;
			}
		}
	}
	return NULL;
}


/* optimized tree execute test for compositing */
void ntreeCompositExecTree(bNodeTree *ntree, RenderData *rd, int do_preview)
{
	bNode *node;
	ListBase threads;
	ThreadData thdata;
	int totnode, maxthreads, rendering= 1;
	
	if(ntree==NULL) return;
	
	if(rd->mode & R_THREADS)
		maxthreads= 2;
	else
		maxthreads= 1;
	
	if(do_preview)
		ntreeInitPreview(ntree, 0, 0);
	
	ntreeBeginExecTree(ntree);
	
	/* prevent unlucky accidents */
	if(G.background)
		rd->scemode &= ~R_COMP_CROP;
	
	/* setup callerdata for thread callback */
	thdata.rd= rd;
	thdata.stack= ntree->stack;
	
	/* sets need_exec tags in nodes */
	totnode= setExecutableNodes(ntree, &thdata);
	
	BLI_init_threads(&threads, exec_composite_node, maxthreads);
	
	while(rendering) {
		
		if(BLI_available_threads(&threads)) {
			node= getExecutableNode(ntree);
			if(node) {
				
				if(ntree->timecursor)
					ntree->timecursor(totnode--);
				
				node->new = (bNode *)&thdata;
				node->exec= NODE_PROCESSING;
				BLI_insert_thread(&threads, node);
			}
			else
				PIL_sleep_ms(50);
		}
		else
			PIL_sleep_ms(50);
		
		/* check for ready ones, and if we need to continue */
		rendering= 0;
		for(node= ntree->nodes.first; node; node= node->next) {
			if(node->exec & NODE_READY) {
				if((node->exec & NODE_FINISHED)==0) {
					BLI_remove_thread(&threads, node);
					node->exec |= NODE_FINISHED;
				}
			}
			else rendering= 1;
		}
	}
	
	
	BLI_end_threads(&threads);
	
	ntreeEndExecTree(ntree);
	
	free_unused_animimages();
	
}


