/*
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
 * Inc., 51 Franklin Street, Fifth Floor, Boston, MA 02110-1301, USA.
 *
 * The Original Code is Copyright (C) 2005 Blender Foundation.
 * All rights reserved.
 *
 * The Original Code is: all of this file.
 *
 * Contributor(s): Bob Holcomb.
 *
 * ***** END GPL LICENSE BLOCK *****
 */

/** \file blender/blenkernel/intern/node.c
 *  \ingroup bke
 */


#if 0 /* pynodes commented for now */
#  ifdef WITH_PYTHON
#    include <Python.h>
#  endif
#endif

#include "MEM_guardedalloc.h"

#include <stdlib.h>
#include <stddef.h>
#include <string.h>
#include <limits.h>

#include "DNA_anim_types.h"
#include "DNA_action_types.h"
#include "DNA_node_types.h"

#include "BLI_listbase.h"

#include "RNA_access.h"

#include "BKE_animsys.h"
#include "BKE_action.h"
#include "BKE_fcurve.h"
#include "BKE_node.h"
#include "BKE_utildefines.h"

#include "PIL_time.h"

#include "CMP_node.h"
#include "intern/CMP_util.h"	/* stupid include path... */

#include "SHD_node.h"
#include "TEX_node.h"
#include "intern/TEX_util.h"

#include "GPU_material.h"

static ListBase empty_list = {NULL, NULL};
ListBase node_all_composit = {NULL, NULL};
ListBase node_all_shaders = {NULL, NULL};
ListBase node_all_textures = {NULL, NULL};

/* ************** Type stuff **********  */

static bNodeType *node_get_type(bNodeTree *ntree, int type, ID *id)
{
	bNodeType *ntype = ntree->alltypes.first;
	for(; ntype; ntype= ntype->next)
		if(ntype->type==type && id==ntype->id )
			return ntype;
	
	return NULL;
}

void ntreeInitTypes(bNodeTree *ntree)
{
	bNode *node, *next;
	
	if(ntree->type==NTREE_SHADER)
		ntree->alltypes= node_all_shaders;
	else if(ntree->type==NTREE_COMPOSIT)
		ntree->alltypes= node_all_composit;
	else if(ntree->type==NTREE_TEXTURE)
		ntree->alltypes= node_all_textures;
	else {
		ntree->alltypes= empty_list;
		printf("Error: no type definitions for nodes\n");
	}
	
	for(node= ntree->nodes.first; node; node= next) {
		next= node->next;
		if(node->type==NODE_DYNAMIC) {
			bNodeType *stype= NULL;
			if(node->id==NULL) { /* empty script node */
				stype= node_get_type(ntree, node->type, NULL);
			} else { /* not an empty script node */
				stype= node_get_type(ntree, node->type, node->id);
				if(!stype) {
					stype= node_get_type(ntree, node->type, NULL);
					/* needed info if the pynode script fails now: */
					if (node->id) node->storage= ntree;
				} else {
					node->custom1= 0;
					node->custom1= BSET(node->custom1,NODE_DYNAMIC_ADDEXIST);
				}
			}
			node->typeinfo= stype;
			if(node->typeinfo)
				node->typeinfo->initfunc(node);
		} else {
			node->typeinfo= node_get_type(ntree, node->type, NULL);
		}

		if(node->typeinfo==NULL) {
			printf("Error: Node type %s doesn't exist anymore, removed\n", node->name);
			nodeFreeNode(ntree, node);
		}
	}
			
	ntree->init |= NTREE_TYPE_INIT;
}

/* updates node with (modified) bNodeType.. this should be done for all trees */
void ntreeUpdateType(bNodeTree *ntree, bNodeType *ntype)
{
	bNode *node;

	for(node= ntree->nodes.first; node; node= node->next) {
		if(node->typeinfo== ntype) {
			nodeUpdateType(ntree, node, ntype);
		}
	}
}

/* only used internal... we depend on type definitions! */
static bNodeSocket *node_add_socket_type(ListBase *lb, bNodeSocketType *stype)
{
	bNodeSocket *sock= MEM_callocN(sizeof(bNodeSocket), "sock");
	
	BLI_strncpy(sock->name, stype->name, NODE_MAXSTR);
	if(stype->limit==0) sock->limit= 0xFFF;
	else sock->limit= stype->limit;
	sock->type= stype->type;
	
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

static bNodeSocket *node_add_group_socket(ListBase *lb, bNodeSocket *gsock)
{
	bNodeSocket *sock= MEM_callocN(sizeof(bNodeSocket), "sock");
	
	/* make a copy of the group socket */
	*sock = *gsock;
	sock->link = NULL;
	sock->next = sock->prev = NULL;
	sock->new_sock = NULL;
	sock->ns.data = NULL;
	
	sock->own_index = gsock->own_index;
	sock->groupsock = gsock;
	/* XXX hack: group socket input/output roles are inverted internally,
	 * need to change the limit value when making actual node sockets from them.
	 */
	sock->limit = (gsock->limit==1 ? 0xFFF : 1);
	
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
		if(strncmp(sock->name, stype->name, NODE_MAXSTR)==0)
			break;
	}
	if(sock) {
		sock->type= stype->type;		/* in future, read this from tydefs! */
		if(stype->limit==0) sock->limit= 0xFFF;
		else sock->limit= stype->limit;
		
		sock->ns.min= stype->min;
		sock->ns.max= stype->max;
		
		BLI_remlink(lb, sock);
		
		return sock;
	}
	else {
		return node_add_socket_type(NULL, stype);
	}
}

static bNodeSocket *verify_group_socket(ListBase *lb, bNodeSocket *gsock)
{
	bNodeSocket *sock;
	
	for(sock= lb->first; sock; sock= sock->next) {
		if(sock->own_index==gsock->own_index)
				break;
	}
	if(sock) {
		sock->groupsock = gsock;
		
		strcpy(sock->name, gsock->name);
		sock->type= gsock->type;
		
		/* XXX hack: group socket input/output roles are inverted internally,
		 * need to change the limit value when making actual node sockets from them.
		 */
		sock->limit = (gsock->limit==1 ? 0xFFF : 1);
		
		sock->ns.min= gsock->ns.min;
		sock->ns.max= gsock->ns.max;
		
		BLI_remlink(lb, sock);
		
		return sock;
	}
	else {
		return node_add_group_socket(NULL, gsock);
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

static void verify_group_socket_list(bNodeTree *ntree, ListBase *lb, ListBase *glb)
{
	bNodeSocket *gsock;
	
	/* step by step compare */
	for (gsock= glb->first; gsock; gsock=gsock->next) {
		/* abusing new_sock pointer for verification here! only used inside this function */
		gsock->new_sock= verify_group_socket(lb, gsock);
	}
	/* leftovers are removed */
	while(lb->first)
		node_rem_socket(ntree, lb, lb->first);
	/* and we put back the verified sockets */
	for (gsock= glb->first; gsock; gsock=gsock->next) {
		BLI_addtail(lb, gsock->new_sock);
		gsock->new_sock = NULL;
	}
}

void nodeVerifyType(bNodeTree *ntree, bNode *node)
{
	/* node groups don't have static sock lists, but use external sockets from the tree instead */
	if (node->type==NODE_GROUP) {
		bNodeTree *ngroup= (bNodeTree*)node->id;
		if (ngroup) {
			verify_group_socket_list(ntree, &node->inputs, &ngroup->inputs);
			verify_group_socket_list(ntree, &node->outputs, &ngroup->outputs);
		}
	}
	else {
		bNodeType *ntype= node->typeinfo;
		if(ntype) {
			verify_socket_list(ntree, &node->inputs, ntype->inputs);
			verify_socket_list(ntree, &node->outputs, ntype->outputs);
		}
	}
}

void ntreeVerifyTypes(bNodeTree *ntree)
{
	bNode *node;
	
	/* if((ntree->init & NTREE_TYPE_INIT)==0) */
	ntreeInitTypes(ntree);

	/* check inputs and outputs, and remove or insert them */
	for(node= ntree->nodes.first; node; node= node->next)
		nodeVerifyType(ntree, node);
	
}

/* ************** Group stuff ********** */

/* XXX group typeinfo struct is used directly in ntreeMakeOwnType, needs cleanup */
static bNodeType ntype_group;

/* groups display their internal tree name as label */
static const char *group_label(bNode *node)
{
	return (node->id)? node->id->name+2: "Missing Datablock";
}

void register_node_type_group(ListBase *lb)
{
	node_type_base(&ntype_group, NODE_GROUP, "Group", NODE_CLASS_GROUP, NODE_OPTIONS, NULL, NULL);
	node_type_size(&ntype_group, 120, 60, 200);
	node_type_label(&ntype_group, group_label);
	
	nodeRegisterType(lb, &ntype_group);
}

static bNodeSocket *find_group_node_input(bNode *gnode, bNodeSocket *gsock)
{
	bNodeSocket *sock;
	for (sock=gnode->inputs.first; sock; sock=sock->next)
		if (sock->groupsock == gsock)
			return sock;
	return NULL;
}

static bNodeSocket *find_group_node_output(bNode *gnode, bNodeSocket *gsock)
{
	bNodeSocket *sock;
	for (sock=gnode->outputs.first; sock; sock=sock->next)
		if (sock->groupsock == gsock)
			return sock;
	return NULL;
}

bNode *nodeMakeGroupFromSelected(bNodeTree *ntree)
{
	bNodeLink *link, *linkn;
	bNode *node, *gnode, *nextn;
	bNodeTree *ngroup;
	bNodeSocket *gsock;
	ListBase anim_basepaths = {NULL, NULL};
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
	ngroup= ntreeAddTree("NodeGroup", ntree->type, TRUE);
	
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
	
	/* make group node */
	gnode= nodeAddNodeType(ntree, NODE_GROUP, ngroup, NULL);
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
			gsock = nodeGroupExposeSocket(ngroup, link->tosock, SOCK_IN);
			link->tosock->link = nodeAddLink(ngroup, NULL, gsock, link->tonode, link->tosock);
			link->tosock = node_add_group_socket(&gnode->inputs, gsock);
			link->tonode = gnode;
		}
		else if(link->fromnode && (link->fromnode->flag & NODE_SELECT)) {
			/* search for existing group node socket */
			for (gsock=ngroup->outputs.first; gsock; gsock=gsock->next)
				if (gsock->link && gsock->link->fromsock==link->fromsock)
					break;
			if (!gsock) {
				gsock = nodeGroupExposeSocket(ngroup, link->fromsock, SOCK_OUT);
				gsock->link = nodeAddLink(ngroup, link->fromnode, link->fromsock, NULL, gsock);
				link->fromsock = node_add_group_socket(&gnode->outputs, gsock);
			}
			else
				link->fromsock = find_group_node_output(gnode, gsock);
			link->fromnode = gnode;
		}
	}

	/* update node levels */
	ntreeSolveOrder(ntree);

	return gnode;
}

/* here's a nasty little one, need to check users... */
/* should become callbackable... */
void nodeGroupVerify(bNodeTree *ngroup)
{
	/* group changed, so we rebuild the type definition */
//	ntreeMakeGroupSockets(ngroup);
	
	if(ngroup->type==NTREE_SHADER) {
		Material *ma;
		for(ma= G.main->mat.first; ma; ma= ma->id.next) {
			if(ma->nodetree) {
				bNode *node;
				for(node= ma->nodetree->nodes.first; node; node= node->next)
					if(node->id == (ID *)ngroup)
						nodeVerifyType(ma->nodetree, node);
			}
		}
	}
	else if(ngroup->type==NTREE_COMPOSIT) {
		Scene *sce;
		for(sce= G.main->scene.first; sce; sce= sce->id.next) {
			if(sce->nodetree) {
				bNode *node;
				for(node= sce->nodetree->nodes.first; node; node= node->next)
					if(node->id == (ID *)ngroup)
						nodeVerifyType(sce->nodetree, node);
			}
		}
	}
	else if(ngroup->type==NTREE_TEXTURE) {
		Tex *tx;
		for(tx= G.main->tex.first; tx; tx= tx->id.next) {
			if(tx->nodetree) {
				bNode *node;
				for(node= tx->nodetree->nodes.first; node; node= node->next)
					if(node->id == (ID *)ngroup)
						nodeVerifyType(tx->nodetree, node);
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
					if(node->id==&ngroup->id) {
						for(sock= node->inputs.first; sock; sock= sock->next)
							if(sock->link)
								if(sock->groupsock) 
									sock->groupsock->flag |= SOCK_IN_USE;
						for(sock= node->outputs.first; sock; sock= sock->next)
							if(nodeCountSocketLinks(ma->nodetree, sock))
								if(sock->groupsock) 
									sock->groupsock->flag |= SOCK_IN_USE;
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
								if(sock->groupsock) 
									sock->groupsock->flag |= SOCK_IN_USE;
						for(sock= node->outputs.first; sock; sock= sock->next)
							if(nodeCountSocketLinks(sce->nodetree, sock))
								if(sock->groupsock) 
									sock->groupsock->flag |= SOCK_IN_USE;
					}
				}
			}
		}
	}
	else if(ngroup->type==NTREE_TEXTURE) {
		Tex *tx;
		for(tx= G.main->tex.first; tx; tx= tx->id.next) {
			if(tx->nodetree) {
				for(node= tx->nodetree->nodes.first; node; node= node->next) {
					if(node->id==(ID *)ngroup) {
						for(sock= node->inputs.first; sock; sock= sock->next)
							if(sock->link)
								if(sock->groupsock) 
									sock->groupsock->flag |= SOCK_IN_USE;
						for(sock= node->outputs.first; sock; sock= sock->next)
							if(nodeCountSocketLinks(tx->nodetree, sock))
								if(sock->groupsock) 
									sock->groupsock->flag |= SOCK_IN_USE;
					}
				}
			}
		}
	}
	
}
/* finds a node based on its name */
bNode *nodeFindNodebyName(bNodeTree *ntree, const char *name)
{
	return BLI_findstring(&ntree->nodes, name, offsetof(bNode, name));
}

/* finds a node based on given socket */
int nodeFindNode(bNodeTree *ntree, bNodeSocket *sock, bNode **nodep, int *sockindex, int *in_out)
{
	bNode *node;
	bNodeSocket *tsock;
	int index= 0;
	
	for(node= ntree->nodes.first; node; node= node->next) {
		for(index=0, tsock= node->inputs.first; tsock; tsock= tsock->next, index++) {
			if(tsock==sock) {
				if (in_out) *in_out= SOCK_IN;
				break;
			}
		}
		if(tsock)
			break;
		for(index=0, tsock= node->outputs.first; tsock; tsock= tsock->next, index++) {
			if(tsock==sock) {
				if (in_out) *in_out= SOCK_OUT;
				break;
			}
		}
		if(tsock)
			break;
	}

	if(node) {
		*nodep= node;
		if(sockindex) *sockindex= index;
		return 1;
	}
	
	*nodep= NULL;
	return 0;
}

/* returns 1 if its OK */
int nodeGroupUnGroup(bNodeTree *ntree, bNode *gnode)
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
						bNodeSocket *insock= find_group_node_input(gnode, gsock->link->fromsock);
						if (insock->link) {
							link->fromnode = insock->link->fromnode;
							link->fromsock = insock->link->fromsock;
						}
					}
				}
				else {
					/* constant group output: copy the stack value to the external socket.
					 * the link is kept here until all possible external users have been fixed.
					 */
					QUATCOPY(link->tosock->ns.vec, gsock->ns.vec);
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
				/* uses group constant input. copy the input value and remove the dead link. */
				QUATCOPY(link->tosock->ns.vec, insock->ns.vec);
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
	
	/* and copy across the animation */
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
		free_libblock(&G.main->action, waction);
	}
	
	/* delete the group instance. this also removes old input links! */
	nodeFreeNode(ntree, gnode);
	
	/* free the group tree (takes care of user count) */
	free_libblock(&G.main->nodetree, wgroup);
	
	/* solve order goes fine, but the level tags not... doing it twice works for now. solve this once */
	/* XXX is this still necessary with new groups? it may have been caused by non-updated sock->link pointers. lukas */
	ntreeSolveOrder(ntree);
	ntreeSolveOrder(ntree);

	return 1;
}

void nodeGroupCopy(bNode *gnode)
{
	bNodeSocket *sock;

	gnode->id->us--;
	gnode->id= (ID *)ntreeCopyTree((bNodeTree *)gnode->id);

	/* new_sock was set in nodeCopyNode */
	for(sock=gnode->inputs.first; sock; sock=sock->next)
		if(sock->groupsock)
			sock->groupsock= sock->groupsock->new_sock;

	for(sock=gnode->outputs.first; sock; sock=sock->next)
		if(sock->groupsock)
			sock->groupsock= sock->groupsock->new_sock;
}

bNodeSocket *nodeGroupAddSocket(bNodeTree *ngroup, const char *name, int type, int in_out)
{
	bNodeSocket *gsock = MEM_callocN(sizeof(bNodeSocket), "bNodeSocket");
	
	strncpy(gsock->name, name, sizeof(gsock->name));
	gsock->type = type;
	gsock->ns.sockettype = type;
	gsock->ns.min = INT_MIN;
	gsock->ns.max = INT_MAX;
	zero_v4(gsock->ns.vec);
	gsock->ns.data = NULL;
	gsock->flag = 0;

	gsock->next = gsock->prev = NULL;
	gsock->new_sock = NULL;
	gsock->link = NULL;
	gsock->ns.data = NULL;
	/* assign new unique index */
	gsock->own_index = ngroup->cur_index++;
	gsock->limit = (in_out==SOCK_IN ? 0xFFF : 1);
	
	BLI_addtail(in_out==SOCK_IN ? &ngroup->inputs : &ngroup->outputs, gsock);
	
	return gsock;
}

bNodeSocket *nodeGroupExposeSocket(bNodeTree *ngroup, bNodeSocket *sock, int in_out)
{
	bNodeSocket *gsock= nodeGroupAddSocket(ngroup, sock->name, sock->type, in_out);
	/* initialize the default socket value */
	QUATCOPY(gsock->ns.vec, sock->ns.vec);
	return gsock;
}

void nodeGroupExposeAllSockets(bNodeTree *ngroup)
{
	bNode *node;
	bNodeSocket *sock, *gsock;
	
	for (node=ngroup->nodes.first; node; node=node->next) {
		for (sock=node->inputs.first; sock; sock=sock->next) {
			if (!sock->link && !(sock->flag & SOCK_HIDDEN)) {
				gsock = nodeGroupAddSocket(ngroup, sock->name, sock->type, SOCK_IN);
				/* initialize the default socket value */
				QUATCOPY(gsock->ns.vec, sock->ns.vec);
				sock->link = nodeAddLink(ngroup, NULL, gsock, node, sock);
			}
		}
		for (sock=node->outputs.first; sock; sock=sock->next) {
			if (nodeCountSocketLinks(ngroup, sock)==0 && !(sock->flag & SOCK_HIDDEN)) {
				gsock = nodeGroupAddSocket(ngroup, sock->name, sock->type, SOCK_OUT);
				/* initialize the default socket value */
				QUATCOPY(gsock->ns.vec, sock->ns.vec);
				gsock->link = nodeAddLink(ngroup, node, sock, NULL, gsock);
			}
		}
	}
}

void nodeGroupRemoveSocket(bNodeTree *ngroup, bNodeSocket *gsock, int in_out)
{
	nodeRemSocketLinks(ngroup, gsock);
	switch (in_out) {
	case SOCK_IN:	BLI_remlink(&ngroup->inputs, gsock);	break;
	case SOCK_OUT:	BLI_remlink(&ngroup->outputs, gsock);	break;
	}
	MEM_freeN(gsock);
}

/* ************** Add stuff ********** */
void nodeAddSockets(bNode *node, bNodeType *ntype)
{
	if (node->type==NODE_GROUP) {
		bNodeTree *ntree= (bNodeTree*)node->id;
		if (ntree) {
			bNodeSocket *gsock;
			for (gsock=ntree->inputs.first; gsock; gsock=gsock->next)
				node_add_group_socket(&node->inputs, gsock);
			for (gsock=ntree->outputs.first; gsock; gsock=gsock->next)
				node_add_group_socket(&node->outputs, gsock);
		}
	}
	else {
		bNodeSocketType *stype;
		
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
	}
}

/* Find the first available, non-duplicate name for a given node */
void nodeUniqueName(bNodeTree *ntree, bNode *node)
{
	BLI_uniquename(&ntree->nodes, node, "Node", '.', offsetof(bNode, name), sizeof(node->name));
}

bNode *nodeAddNodeType(bNodeTree *ntree, int type, bNodeTree *ngroup, ID *id)
{
	bNode *node= NULL;
	bNodeType *ntype= NULL;

	if (ngroup && BLI_findindex(&G.main->nodetree, ngroup)==-1) {
		printf("nodeAddNodeType() error: '%s' not in main->nodetree\n", ngroup->id.name);
		return NULL;
	}

	if(type>=NODE_DYNAMIC_MENU) {
		int a=0, idx= type-NODE_DYNAMIC_MENU;
		ntype= ntree->alltypes.first;
		while(ntype) {
			if(ntype->type==NODE_DYNAMIC) {
				if(a==idx)
					break;
				a++;
			}
			ntype= ntype->next;
		}
	} else
		ntype= node_get_type(ntree, type, id);

	if(ntype == NULL) {
		printf("nodeAddNodeType() error: '%d' type invalid\n", type);
		return NULL;
	}

	node= MEM_callocN(sizeof(bNode), "new node");
	BLI_addtail(&ntree->nodes, node);
	node->typeinfo= ntype;
	if(type>=NODE_DYNAMIC_MENU)
		node->custom2= type; /* for node_dynamic_init */

	if(ngroup)
		BLI_strncpy(node->name, ngroup->id.name+2, NODE_MAXSTR);
	else if(type>NODE_DYNAMIC_MENU) {
		BLI_strncpy(node->name, ntype->id->name+2, NODE_MAXSTR);
	}
	else
		BLI_strncpy(node->name, ntype->name, NODE_MAXSTR);

	nodeUniqueName(ntree, node);
	
	node->type= ntype->type;
	node->flag= NODE_SELECT|ntype->flag;
	node->width= ntype->width;
	node->miniwidth= 42.0f;		/* small value only, allows print of first chars */

	if(type==NODE_GROUP)
		node->id= (ID *)ngroup;

	/* need init handler later? */
	/* got it-bob*/
	if(ntype->initfunc!=NULL)
		ntype->initfunc(node);
	
	nodeAddSockets(node, ntype);
	
	return node;
}

void nodeMakeDynamicType(bNode *node)
{
	/* find SH_DYNAMIC_NODE ntype */
	bNodeType *ntype= node_all_shaders.first;
	while(ntype) {
		if(ntype->type==NODE_DYNAMIC && ntype->id==NULL)
			break;
		ntype= ntype->next;
	}

	/* make own type struct to fill */
	if(ntype) {
		/*node->typeinfo= MEM_dupallocN(ntype);*/
		bNodeType *newtype= MEM_callocN(sizeof(bNodeType), "dynamic bNodeType");
		*newtype= *ntype;
		newtype->name= BLI_strdup(ntype->name);
		node->typeinfo= newtype;
	}
}

void nodeUpdateType(bNodeTree *ntree, bNode* node, bNodeType *ntype)
{
	verify_socket_list(ntree, &node->inputs, ntype->inputs);
	verify_socket_list(ntree, &node->outputs, ntype->outputs);
}

/* keep socket listorder identical, for copying links */
/* ntree is the target tree */
bNode *nodeCopyNode(struct bNodeTree *ntree, struct bNode *node)
{
	bNode *nnode= MEM_callocN(sizeof(bNode), "dupli node");
	bNodeSocket *sock, *oldsock;

	*nnode= *node;
	nodeUniqueName(ntree, nnode);
	
	BLI_addtail(&ntree->nodes, nnode);
	
	BLI_duplicatelist(&nnode->inputs, &node->inputs);
	oldsock= node->inputs.first;
	for(sock= nnode->inputs.first; sock; sock= sock->next, oldsock= oldsock->next) {
		oldsock->new_sock= sock;
	}
	
	BLI_duplicatelist(&nnode->outputs, &node->outputs);
	oldsock= node->outputs.first;
	for(sock= nnode->outputs.first; sock; sock= sock->next, oldsock= oldsock->next) {
		sock->stack_index= 0;
		sock->ns.data= NULL;
		oldsock->new_sock= sock;
	}
	
	/* don't increase node->id users, freenode doesn't decrement either */
	
	if(node->typeinfo->copystoragefunc)
		node->typeinfo->copystoragefunc(node, nnode);
	
	node->new_node= nnode;
	nnode->new_node= NULL;
	nnode->preview= NULL;
	
	return nnode;
}

/* fromsock and tosock can be NULL */
/* also used via rna api, so we check for proper input output direction */
bNodeLink *nodeAddLink(bNodeTree *ntree, bNode *fromnode, bNodeSocket *fromsock, bNode *tonode, bNodeSocket *tosock)
{
	bNodeSocket *sock;
	bNodeLink *link= NULL; 
	int from= 0, to= 0;
	
	if(fromnode) {
		/* test valid input */
		for(sock= fromnode->outputs.first; sock; sock= sock->next)
			if(sock==fromsock)
				break;
		if(sock)
			from= 1; /* OK */
		else {
			for(sock= fromnode->inputs.first; sock; sock= sock->next)
				if(sock==fromsock)
					break;
			if(sock)
				from= -1; /* OK but flip */
		}
	}
	if(tonode) {
		for(sock= tonode->inputs.first; sock; sock= sock->next)
			if(sock==tosock)
				break;
		if(sock)
			to= 1; /* OK */
		else {
			for(sock= tonode->outputs.first; sock; sock= sock->next)
				if(sock==tosock)
					break;
			if(sock)
				to= -1; /* OK but flip */
		}
	}
	
	/* this allows NULL sockets to work */
	if(from >= 0 && to >= 0) {
		link= MEM_callocN(sizeof(bNodeLink), "link");
		BLI_addtail(&ntree->links, link);
		link->fromnode= fromnode;
		link->fromsock= fromsock;
		link->tonode= tonode;
		link->tosock= tosock;
	}
	else if(from <= 0 && to <= 0) {
		link= MEM_callocN(sizeof(bNodeLink), "link");
		BLI_addtail(&ntree->links, link);
		link->fromnode= tonode;
		link->fromsock= tosock;
		link->tonode= fromnode;
		link->tosock= fromsock;
	}
	
	return link;
}

void nodeRemLink(bNodeTree *ntree, bNodeLink *link)
{
	BLI_remlink(&ntree->links, link);
	if(link->tosock)
		link->tosock->link= NULL;
	MEM_freeN(link);
}

void nodeRemSocketLinks(bNodeTree *ntree, bNodeSocket *sock)
{
	bNodeLink *link, *next;
	
	for(link= ntree->links.first; link; link= next) {
		next= link->next;
		if(link->fromsock==sock || link->tosock==sock) {
			nodeRemLink(ntree, link);
		}
	}
}


bNodeTree *ntreeAddTree(const char *name, int type, const short is_group)
{
	bNodeTree *ntree;

	if (is_group)
		ntree= alloc_libblock(&G.main->nodetree, ID_NT, name);
	else {
		ntree= MEM_callocN(sizeof(bNodeTree), "new node tree");
		*( (short *)ntree->id.name )= ID_NT; /* not "type", as that is ntree->type */
		BLI_strncpy(ntree->id.name+2, name, sizeof(ntree->id.name));
	}

	ntree->type= type;
	ntree->alltypes.first = NULL;
	ntree->alltypes.last = NULL;

	ntreeInitTypes(ntree);
	return ntree;
}

/* Warning: this function gets called during some rather unexpected times
 *	- this gets called when executing compositing updates (for threaded previews)
 *	- when the nodetree datablock needs to be copied (i.e. when users get copied)
 *	- for scene duplication use ntreeSwapID() after so we dont have stale pointers.
 */
bNodeTree *ntreeCopyTree(bNodeTree *ntree)
{
	bNodeTree *newtree;
	bNode *node, *nnode, *last;
	bNodeLink *link;
	bNodeSocket *gsock, *oldgsock;
	
	if(ntree==NULL) return NULL;
	
	/* is ntree part of library? */
	for(newtree=G.main->nodetree.first; newtree; newtree= newtree->id.next)
		if(newtree==ntree) break;
	if(newtree) {
		newtree= copy_libblock(ntree);
	} else {
		newtree= MEM_dupallocN(ntree);
		copy_libblock_data(&newtree->id, &ntree->id, TRUE); /* copy animdata and ID props */
	}

	id_us_plus((ID *)newtree->gpd);

	/* in case a running nodetree is copied */
	newtree->init &= ~(NTREE_EXEC_INIT);
	newtree->threadstack= NULL;
	newtree->stack= NULL;
	
	newtree->nodes.first= newtree->nodes.last= NULL;
	newtree->links.first= newtree->links.last= NULL;
	
	last = ntree->nodes.last;
	for(node= ntree->nodes.first; node; node= node->next) {
		node->new_node= NULL;
		nnode= nodeCopyNode(newtree, node);	/* sets node->new */
		if(node==last) break;
	}
	
	/* socket definition for group usage */
	BLI_duplicatelist(&newtree->inputs, &ntree->inputs);
	for(gsock= newtree->inputs.first, oldgsock= ntree->inputs.first; gsock; gsock=gsock->next, oldgsock=oldgsock->next) {
		oldgsock->new_sock= gsock;
		gsock->groupsock = (oldgsock->groupsock ? oldgsock->groupsock->new_sock : NULL);
	}
	
	BLI_duplicatelist(&newtree->outputs, &ntree->outputs);
	for(gsock= newtree->outputs.first, oldgsock= ntree->outputs.first; gsock; gsock=gsock->next, oldgsock=oldgsock->next) {
		oldgsock->new_sock= gsock;
		gsock->groupsock = (oldgsock->groupsock ? oldgsock->groupsock->new_sock : NULL);
	}

	/* copy links */
	BLI_duplicatelist(&newtree->links, &ntree->links);
	for(link= newtree->links.first; link; link= link->next) {
		link->fromnode = (link->fromnode ? link->fromnode->new_node : NULL);
		link->fromsock = (link->fromsock ? link->fromsock->new_sock : NULL);
		link->tonode = (link->tonode ? link->tonode->new_node : NULL);
		link->tosock = (link->tosock ? link->tosock->new_sock : NULL);
		/* update the link socket's pointer */
		if (link->tosock)
			link->tosock->link = link;
	}

	return newtree;
}

/* use when duplicating scenes */
void ntreeSwitchID(bNodeTree *ntree, ID *id_from, ID *id_to)
{
	bNode *node;
	/* for scene duplication only */
	for(node= ntree->nodes.first; node; node= node->next) {
		if(node->id==id_from) {
			node->id= id_to;
		}
	}
}

/* *************** preview *********** */
/* if node->preview, then we assume the rect to exist */

static void node_free_preview(bNode *node)
{
	if(node->preview) {
		if(node->preview->rect)
			MEM_freeN(node->preview->rect);
		MEM_freeN(node->preview);
		node->preview= NULL;
	}	
}

static void node_init_preview(bNode *node, int xsize, int ysize)
{
	
	if(node->preview==NULL) {
		node->preview= MEM_callocN(sizeof(bNodePreview), "node preview");
		//		printf("added preview %s\n", node->name);
	}
	
	/* node previews can get added with variable size this way */
	if(xsize==0 || ysize==0)
		return;
	
	/* sanity checks & initialize */
	if(node->preview->rect) {
		if(node->preview->xsize!=xsize && node->preview->ysize!=ysize) {
			MEM_freeN(node->preview->rect);
			node->preview->rect= NULL;
		}
	}
	
	if(node->preview->rect==NULL) {
		node->preview->rect= MEM_callocN(4*xsize + xsize*ysize*sizeof(char)*4, "node preview rect");
		node->preview->xsize= xsize;
		node->preview->ysize= ysize;
	}
	/* no clear, makes nicer previews */
}

void ntreeInitPreview(bNodeTree *ntree, int xsize, int ysize)
{
	bNode *node;
	
	if(ntree==NULL)
		return;
	
	for(node= ntree->nodes.first; node; node= node->next) {
		if(node->typeinfo->flag & NODE_PREVIEW)	/* hrms, check for closed nodes? */
			node_init_preview(node, xsize, ysize);
		if(node->type==NODE_GROUP && (node->flag & NODE_GROUP_EDIT))
			ntreeInitPreview((bNodeTree *)node->id, xsize, ysize);
	}		
}

static void nodeClearPreview(bNode *node)
{
	if(node->preview && node->preview->rect)
		memset(node->preview->rect, 0, MEM_allocN_len(node->preview->rect));
}

/* use it to enforce clear */
void ntreeClearPreview(bNodeTree *ntree)
{
	bNode *node;
	
	if(ntree==NULL)
		return;
	
	for(node= ntree->nodes.first; node; node= node->next) {
		if(node->typeinfo->flag & NODE_PREVIEW)
			nodeClearPreview(node);
		if(node->type==NODE_GROUP && (node->flag & NODE_GROUP_EDIT))
			ntreeClearPreview((bNodeTree *)node->id);
	}		
}

/* hack warning! this function is only used for shader previews, and 
since it gets called multiple times per pixel for Ztransp we only
add the color once. Preview gets cleared before it starts render though */
void nodeAddToPreview(bNode *node, float *col, int x, int y, int do_manage)
{
	bNodePreview *preview= node->preview;
	if(preview) {
		if(x>=0 && y>=0) {
			if(x<preview->xsize && y<preview->ysize) {
				unsigned char *tar= preview->rect+ 4*((preview->xsize*y) + x);
				
				if(do_manage) {
					tar[0]= FTOCHAR(linearrgb_to_srgb(col[0]));
					tar[1]= FTOCHAR(linearrgb_to_srgb(col[1]));
					tar[2]= FTOCHAR(linearrgb_to_srgb(col[2]));
				}
				else {
					tar[0]= FTOCHAR(col[0]);
					tar[1]= FTOCHAR(col[1]);
					tar[2]= FTOCHAR(col[2]);
				}
				tar[3]= FTOCHAR(col[3]);
			}
			//else printf("prv out bound x y %d %d\n", x, y);
		}
		//else printf("prv out bound x y %d %d\n", x, y);
	}
}


/* ************** Free stuff ********** */

/* goes over entire tree */
void nodeUnlinkNode(bNodeTree *ntree, bNode *node)
{
	bNodeLink *link, *next;
	bNodeSocket *sock;
	ListBase *lb;
	
	for(link= ntree->links.first; link; link= next) {
		next= link->next;
		
		if(link->fromnode==node) {
			lb= &node->outputs;
			if (link->tonode)
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
	nodeUnlinkNode(ntree, node);
	BLI_remlink(&ntree->nodes, node);

	/* since it is called while free database, node->id is undefined */
	
	if(ntree->type==NTREE_COMPOSIT)
		composit_free_node_cache(node);
	BLI_freelistN(&node->inputs);
	BLI_freelistN(&node->outputs);
	
	node_free_preview(node);

	if(node->typeinfo && node->typeinfo->freestoragefunc) {
		node->typeinfo->freestoragefunc(node);
	}

	MEM_freeN(node);
}

/* do not free ntree itself here, free_libblock calls this function too */
void ntreeFreeTree(bNodeTree *ntree)
{
	bNode *node, *next;
	
	if(ntree==NULL) return;
	
	ntreeEndExecTree(ntree);	/* checks for if it is still initialized */
	
	BKE_free_animdata((ID *)ntree);

	id_us_min((ID *)ntree->gpd);

	BLI_freelistN(&ntree->links);	/* do first, then unlink_node goes fast */
	
	for(node= ntree->nodes.first; node; node= next) {
		next= node->next;
		nodeFreeNode(ntree, node);
	}
	
	BLI_freelistN(&ntree->inputs);
	BLI_freelistN(&ntree->outputs);
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
		ntree->id.lib= NULL;
		ntree->id.flag= LIB_LOCAL;
		new_id(NULL, (ID *)ntree, NULL);
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
	else if(ntree->type==NTREE_TEXTURE) {
		Tex *tx;
		for(tx= G.main->tex.first; tx; tx= tx->id.next) {
			if(tx->nodetree) {
				bNode *node;
				
				/* find if group is in tree */
				for(node= tx->nodetree->nodes.first; node; node= node->next) {
					if(node->id == (ID *)ntree) {
						if(tx->id.lib) lib= 1;
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
		new_id(NULL, (ID *)ntree, NULL);
	}
	else if(local && lib) {
		/* this is the mixed case, we copy the tree and assign it to local users */
		bNodeTree *newtree= ntreeCopyTree(ntree);
		
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
		else if(ntree->type==NTREE_TEXTURE) {
			Tex *tx;
			for(tx= G.main->tex.first; tx; tx= tx->id.next) {
				if(tx->nodetree) {
					bNode *node;
					
					/* find if group is in tree */
					for(node= tx->nodetree->nodes.first; node; node= node->next) {
						if(node->id == (ID *)ntree) {
							if(tx->id.lib==NULL) {
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


/* ************ find stuff *************** */

static int ntreeHasType(bNodeTree *ntree, int type)
{
	bNode *node;
	
	if(ntree)
		for(node= ntree->nodes.first; node; node= node->next)
			if(node->type == type)
				return 1;
	return 0;
}

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

	/* check for group edit */
	for(node= ntree->nodes.first; node; node= node->next)
		if(node->flag & NODE_GROUP_EDIT)
			break;

	if(node)
		ntree= (bNodeTree*)node->id;
	
	/* now find active node with this id */
	for(node= ntree->nodes.first; node; node= node->next)
		if(node->id && GS(node->id->name)==idtype)
			if(node->flag & NODE_ACTIVE_ID)
				break;

	return node;
}

int nodeSetActiveID(bNodeTree *ntree, short idtype, ID *id)
{
	bNode *node;
	int ok= FALSE;

	if(ntree==NULL) return ok;

	/* check for group edit */
	for(node= ntree->nodes.first; node; node= node->next)
		if(node->flag & NODE_GROUP_EDIT)
			break;

	if(node)
		ntree= (bNodeTree*)node->id;

	/* now find active node with this id */
	for(node= ntree->nodes.first; node; node= node->next) {
		if(node->id && GS(node->id->name)==idtype) {
			if(id && ok==FALSE && node->id==id) {
				node->flag |= NODE_ACTIVE_ID;
				ok= TRUE;
			} else {
				node->flag &= ~NODE_ACTIVE_ID;
			}
		}
	}

	return ok;
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
	
		if(link->fromsock) // FIXME, see below
			link->fromsock->flag |= SOCK_IN_USE;
		if(link->tosock) // FIXME This can be NULL, when dragging a new link in the UI, should probably copy the node tree for preview render - campbell
			link->tosock->flag |= SOCK_IN_USE;
	}
}

/* ************** dependency stuff *********** */

/* node is guaranteed to be not checked before */
static int node_recurs_check(bNode *node, bNode ***nsort)
{
	bNode *fromnode;
	bNodeSocket *sock;
	int level = 0xFFF;
	
	node->done= 1;
	
	for(sock= node->inputs.first; sock; sock= sock->next) {
		if(sock->link) {
			fromnode= sock->link->fromnode;
			if(fromnode) {
				if (fromnode->done==0)
					fromnode->level= node_recurs_check(fromnode, nsort);
				if (fromnode->level <= level)
					level = fromnode->level - 1;
			}
		}
	}
	**nsort= node;
	(*nsort)++;
	
	return level;
}


static void ntreeSetOutput(bNodeTree *ntree)
{
	bNode *node;

	/* find the active outputs, might become tree type dependant handler */
	for(node= ntree->nodes.first; node; node= node->next) {
		if(node->typeinfo->nclass==NODE_CLASS_OUTPUT) {
			bNode *tnode;
			int output= 0;
			
			/* we need a check for which output node should be tagged like this, below an exception */
			if(node->type==CMP_NODE_OUTPUT_FILE)
			   continue;
			   
			/* there is more types having output class, each one is checked */
			for(tnode= ntree->nodes.first; tnode; tnode= tnode->next) {
				if(tnode->typeinfo->nclass==NODE_CLASS_OUTPUT) {
					
					if(ntree->type==NTREE_COMPOSIT) {
							
						/* same type, exception for viewer */
						if(tnode->type==node->type ||
						   (ELEM(tnode->type, CMP_NODE_VIEWER, CMP_NODE_SPLITVIEWER) &&
							ELEM(node->type, CMP_NODE_VIEWER, CMP_NODE_SPLITVIEWER))) {
							if(tnode->flag & NODE_DO_OUTPUT) {
								output++;
								if(output>1)
									tnode->flag &= ~NODE_DO_OUTPUT;
							}
						}
					}
					else {
						/* same type */
						if(tnode->type==node->type) {
							if(tnode->flag & NODE_DO_OUTPUT) {
								output++;
								if(output>1)
									tnode->flag &= ~NODE_DO_OUTPUT;
							}
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
	/* clear group socket links */
	for(sock= ntree->outputs.first; sock; sock= sock->next)
		sock->link= NULL;
	if(totnode==0)
		return;
	
	for(link= ntree->links.first; link; link= link->next) {
		link->tosock->link= link;
	}
	
	nsort= nodesort= MEM_callocN(totnode*sizeof(void *), "sorted node array");
	
	/* recursive check */
	for(node= ntree->nodes.first; node; node= node->next) {
		if(node->done==0) {
			node->level= node_recurs_check(node, &nsort);
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

	ntreeSetOutput(ntree);
}


/* Should be callback! */
/* Do not call execs here */
void NodeTagChanged(bNodeTree *ntree, bNode *node)
{
	if(ntree->type==NTREE_COMPOSIT) {
		bNodeSocket *sock;

		for(sock= node->outputs.first; sock; sock= sock->next) {
			if(sock->ns.data) {
				//free_compbuf(sock->ns.data);
				//sock->ns.data= NULL;
			}
		}
		node->need_exec= 1;
	}
}

int NodeTagIDChanged(bNodeTree *ntree, ID *id)
{
	int change = FALSE;

	if(ELEM(NULL, id, ntree))
		return change;
	
	if(ntree->type==NTREE_COMPOSIT) {
		bNode *node;
		
		for(node= ntree->nodes.first; node; node= node->next) {
			if(node->id==id) {
				change= TRUE;
				NodeTagChanged(ntree, node);
			}
		}
	}
	
	return change;
}



/* ******************* executing ************* */

/* for a given socket, find the actual stack entry */
static bNodeStack *get_socket_stack(bNodeStack *stack, bNodeSocket *sock, bNodeStack **gin)
{
	switch (sock->stack_type) {
	case SOCK_STACK_LOCAL:
		return stack + sock->stack_index;
	case SOCK_STACK_EXTERN:
		return (gin ? gin[sock->stack_index] : NULL);
	case SOCK_STACK_CONST:
		return sock->stack_ptr;
	}
	return NULL;
}

/* see notes at ntreeBeginExecTree */
static void node_get_stack(bNode *node, bNodeStack *stack, bNodeStack **in, bNodeStack **out, bNodeStack **gin)
{
	bNodeSocket *sock;
	
	/* build pointer stack */
	if (in) {
		for(sock= node->inputs.first; sock; sock= sock->next) {
			*(in++) = get_socket_stack(stack, sock, gin);
		}
	}
	
	if (out) {
		for(sock= node->outputs.first; sock; sock= sock->next) {
			*(out++) = get_socket_stack(stack, sock, gin);
		}
	}
}

static void node_group_execute(bNodeStack *stack, void *data, bNode *gnode, bNodeStack **in)
{
	bNode *node;
	bNodeTree *ntree= (bNodeTree *)gnode->id;
	bNodeStack *nsin[MAX_SOCKET];	/* arbitrary... watch this */
	bNodeStack *nsout[MAX_SOCKET];	/* arbitrary... watch this */
	
	if(ntree==NULL) return;
	
	stack+= gnode->stack_index;
		
	for(node= ntree->nodes.first; node; node= node->next) {
		if(node->typeinfo->execfunc) {
			node_get_stack(node, stack, nsin, nsout, in);
			
			/* for groups, only execute outputs for edited group */
			if(node->typeinfo->nclass==NODE_CLASS_OUTPUT) {
				if(node->type==CMP_NODE_OUTPUT_FILE || (gnode->flag & NODE_GROUP_EDIT))
					node->typeinfo->execfunc(data, node, nsin, nsout);
			}
			else
				node->typeinfo->execfunc(data, node, nsin, nsout);
		}
	}
	
	/* free internal buffers */
	if (ntree->type==NTREE_COMPOSIT) {
		bNodeSocket *sock;
		bNodeStack *ns;
		
		/* clear hasoutput on all local stack data,
		 * only the group output will be used from now on
		 */
		for (node=ntree->nodes.first; node; node=node->next) {
			for (sock=node->outputs.first; sock; sock=sock->next) {
				if (sock->stack_type==SOCK_STACK_LOCAL) {
					ns= get_socket_stack(stack, sock, in);
					ns->hasoutput = 0;
				}
			}
		}
		/* use the hasoutput flag to tag external sockets */
		for (sock=ntree->outputs.first; sock; sock=sock->next) {
			if (sock->stack_type==SOCK_STACK_LOCAL) {
				ns= get_socket_stack(stack, sock, in);
				ns->hasoutput = 1;
			}
		}
		/* now free all stacks that are not used from outside */
		for (node=ntree->nodes.first; node; node=node->next) {
			for (sock=node->outputs.first; sock; sock=sock->next) {
				if (sock->stack_type==SOCK_STACK_LOCAL ) {
					ns= get_socket_stack(stack, sock, in);
					if (ns->hasoutput==0 && ns->data) {
						free_compbuf(ns->data);
						ns->data = NULL;
					}
				}
			}
		}
	}
}

static int set_stack_indexes_default(bNode *node, int index)
{
	bNodeSocket *sock;
	
	for (sock=node->inputs.first; sock; sock=sock->next) {
		if (sock->link && sock->link->fromsock) {
			sock->stack_type = sock->link->fromsock->stack_type;
			sock->stack_index = sock->link->fromsock->stack_index;
			sock->stack_ptr = sock->link->fromsock->stack_ptr;
		}
		else {
			sock->stack_type = SOCK_STACK_CONST;
			sock->stack_index = -1;
			sock->stack_ptr = &sock->ns;
		}
	}
	
	for (sock=node->outputs.first; sock; sock=sock->next) {
		sock->stack_type = SOCK_STACK_LOCAL;
		sock->stack_index = index++;
		sock->stack_ptr = NULL;
	}
	
	return index;
}

static int ntree_begin_exec_tree(bNodeTree *ntree);
static int set_stack_indexes_group(bNode *node, int index)
{
	bNodeTree *ngroup= (bNodeTree*)node->id;
	bNodeSocket *sock;
	
	if(ngroup && (ngroup->init & NTREE_TYPE_INIT)==0)
		ntreeInitTypes(ngroup);
	
	node->stack_index = index;
	if(ngroup)
		index += ntree_begin_exec_tree(ngroup);
	
	for (sock=node->inputs.first; sock; sock=sock->next) {
		if (sock->link && sock->link->fromsock) {
			sock->stack_type = sock->link->fromsock->stack_type;
			sock->stack_index = sock->link->fromsock->stack_index;
			sock->stack_ptr = sock->link->fromsock->stack_ptr;
		}
		else {
			sock->stack_type = SOCK_STACK_CONST;
			sock->stack_index = -1;
			sock->stack_ptr = &sock->ns;
		}
	}
	
	/* identify group node outputs from internal group sockets */
	for(sock= node->outputs.first; sock; sock= sock->next) {
		if (sock->groupsock) {
			bNodeSocket *insock, *gsock = sock->groupsock;
			switch (gsock->stack_type) {
			case SOCK_STACK_EXTERN:
				/* extern stack is resolved for this group node instance */
				insock= find_group_node_input(node, gsock->link->fromsock);
				sock->stack_type = insock->stack_type;
				sock->stack_index = insock->stack_index;
				sock->stack_ptr = insock->stack_ptr;
				break;
			case SOCK_STACK_LOCAL:
				sock->stack_type = SOCK_STACK_LOCAL;
				/* local stack index must be offset by group node instance */
				sock->stack_index = gsock->stack_index + node->stack_index;
				sock->stack_ptr = NULL;
				break;
			case SOCK_STACK_CONST:
				sock->stack_type = SOCK_STACK_CONST;
				sock->stack_index = -1;
				sock->stack_ptr = gsock->stack_ptr;
				break;
			}
		}
		else {
			sock->stack_type = SOCK_STACK_LOCAL;
			sock->stack_index = index++;
			sock->stack_ptr = NULL;
		}
	}
	
	return index;
}

/* recursively called for groups */
/* we set all trees on own local indices, but put a total counter
   in the groups, so each instance of a group has own stack */
static int ntree_begin_exec_tree(bNodeTree *ntree)
{
	bNode *node;
	bNodeSocket *gsock;
	int index= 0, i;
	
	if((ntree->init & NTREE_TYPE_INIT)==0)
		ntreeInitTypes(ntree);
	
	/* group inputs are numbered 0..totinputs, so external stack can easily be addressed */
	i = 0;
	for(gsock=ntree->inputs.first; gsock; gsock = gsock->next) {
		gsock->stack_type = SOCK_STACK_EXTERN;
		gsock->stack_index = i++;
		gsock->stack_ptr = NULL;
	}
	
	/* create indices for stack, check preview */
	for(node= ntree->nodes.first; node; node= node->next) {
		/* XXX can this be done by a generic one-for-all function?
		 * otherwise should use node-type callback.
		 */
		if(node->type==NODE_GROUP)
			index = set_stack_indexes_group(node, index);
		else
			index = set_stack_indexes_default(node, index);
	}
	
	/* group outputs */
	for(gsock=ntree->outputs.first; gsock; gsock = gsock->next) {
		if (gsock->link && gsock->link->fromsock) {
			gsock->stack_type = gsock->link->fromsock->stack_type;
			gsock->stack_index = gsock->link->fromsock->stack_index;
			gsock->stack_ptr = gsock->link->fromsock->stack_ptr;
		}
		else {
			gsock->stack_type = SOCK_STACK_CONST;
			gsock->stack_index = -1;
			gsock->stack_ptr = &gsock->ns;
		}
	}
	
	return index;
}

/* copy socket compbufs to stack, initialize usage of curve nodes */
static void composit_begin_exec(bNodeTree *ntree, bNodeStack *stack)
{
	bNode *node;
	bNodeSocket *sock;
	
	for(node= ntree->nodes.first; node; node= node->next) {
		
		/* initialize needed for groups */
		node->exec= 0;	
		
		for(sock= node->outputs.first; sock; sock= sock->next) {
			bNodeStack *ns= get_socket_stack(stack, sock, NULL);
			if(ns && sock->ns.data) {
				ns->data= sock->ns.data;
				sock->ns.data= NULL;
			}
		}
		
		/* cannot initialize them while using in threads */
		if(ELEM4(node->type, CMP_NODE_TIME, CMP_NODE_CURVE_VEC, CMP_NODE_CURVE_RGB, CMP_NODE_HUECORRECT)) {
			curvemapping_initialize(node->storage);
			if(node->type==CMP_NODE_CURVE_RGB)
				curvemapping_premultiply(node->storage, 0);
		}
		if(node->type==NODE_GROUP && node->id)
			composit_begin_exec((bNodeTree *)node->id, stack + node->stack_index);

	}
}

/* copy stack compbufs to sockets */
static void composit_end_exec(bNodeTree *ntree, bNodeStack *stack)
{
	bNode *node;
	bNodeStack *ns;

	for(node= ntree->nodes.first; node; node= node->next) {
		bNodeSocket *sock;
		
		for(sock= node->outputs.first; sock; sock= sock->next) {
			ns = get_socket_stack(stack, sock, NULL);
			if(ns && ns->data) {
				sock->ns.data= ns->data;
				ns->data= NULL;
			}
		}
		
		if(node->type==CMP_NODE_CURVE_RGB)
			curvemapping_premultiply(node->storage, 1);
		
		if(node->type==NODE_GROUP && node->id)
			composit_end_exec((bNodeTree *)node->id, stack + node->stack_index);

		node->need_exec= 0;
	}
}

static void group_tag_used_outputs(bNode *gnode, bNodeStack *stack, bNodeStack **gin)
{
	bNodeTree *ntree= (bNodeTree *)gnode->id;
	bNode *node;
	bNodeSocket *sock;
	
	stack+= gnode->stack_index;
	
	for(node= ntree->nodes.first; node; node= node->next) {
		if(node->typeinfo->execfunc) {
			for(sock= node->inputs.first; sock; sock= sock->next) {
				bNodeStack *ns = get_socket_stack(stack, sock, gin);
				ns->hasoutput= 1;
			}
		}
		
		/* non-composite trees do all nodes by default */
		if (ntree->type!=NTREE_COMPOSIT)
			node->need_exec = 1;
		
		for(sock= node->inputs.first; sock; sock= sock->next) {
			bNodeStack *ns = get_socket_stack(stack, sock, gin);
			if (ns) {
				ns->hasoutput = 1;
				
				/* sock type is needed to detect rgba or value or vector types */
				if(sock->link && sock->link->fromsock)
					ns->sockettype= sock->link->fromsock->type;
				else
					sock->ns.sockettype= sock->type;
			}
			
			if(sock->link) {
				bNodeLink *link= sock->link;
				/* this is the test for a cyclic case */
				if(link->fromnode && link->tonode) {
					if(link->fromnode->level >= link->tonode->level && link->tonode->level!=0xFFF);
					else {
						node->need_exec= 0;
					}
				}
			}
		}
		
		/* set stack types (for local stack entries) */
		for(sock= node->outputs.first; sock; sock= sock->next) {
			bNodeStack *ns = get_socket_stack(stack, sock, gin);
			if (ns)
				ns->sockettype = sock->type;
		}
	}
}

/* notes below are ancient! (ton) */
/* stack indices make sure all nodes only write in allocated data, for making it thread safe */
/* only root tree gets the stack, to enable instances to have own stack entries */
/* per tree (and per group) unique indices are created */
/* the index_ext we need to be able to map from groups to the group-node own stack */

typedef struct bNodeThreadStack {
	struct bNodeThreadStack *next, *prev;
	bNodeStack *stack;
	int used;
} bNodeThreadStack;

static bNodeThreadStack *ntreeGetThreadStack(bNodeTree *ntree, int thread)
{
	ListBase *lb= &ntree->threadstack[thread];
	bNodeThreadStack *nts;
	
	for(nts=lb->first; nts; nts=nts->next) {
		if(!nts->used) {
			nts->used= 1;
			return nts;
		}
	}
	nts= MEM_callocN(sizeof(bNodeThreadStack), "bNodeThreadStack");
	nts->stack= MEM_dupallocN(ntree->stack);
	nts->used= 1;
	BLI_addtail(lb, nts);

	return nts;
}

static void ntreeReleaseThreadStack(bNodeThreadStack *nts)
{
	nts->used= 0;
}

/* free texture delegates */
static void tex_end_exec(bNodeTree *ntree)
{
	bNodeThreadStack *nts;
	bNodeStack *ns;
	int th, a;
	
	if(ntree->threadstack) {
		for(th=0; th<BLENDER_MAX_THREADS; th++) {
			for(nts=ntree->threadstack[th].first; nts; nts=nts->next) {
				for(ns= nts->stack, a=0; a<ntree->stacksize; a++, ns++) {
					if(ns->data) {
						MEM_freeN(ns->data);
						ns->data= NULL;
					}
				}
			}
		}
	}
}

void ntreeBeginExecTree(bNodeTree *ntree)
{
	bNodeStack *nsin[MAX_SOCKET];	/* arbitrary... watch this */
	
	/* let's make it sure */
	if(ntree->init & NTREE_EXEC_INIT)
		return;
	
	/* allocate the thread stack listbase array */
	if(ntree->type!=NTREE_COMPOSIT)
		ntree->threadstack= MEM_callocN(BLENDER_MAX_THREADS*sizeof(ListBase), "thread stack array");

	/* goes recursive over all groups */
	ntree->stacksize= ntree_begin_exec_tree(ntree);

	if(ntree->stacksize) {
		bNode *node;
		bNodeStack *ns;
		int a;
		
		/* allocate the base stack */
		ns=ntree->stack= MEM_callocN(ntree->stacksize*sizeof(bNodeStack), "node stack");
		
		/* tag inputs, the get_stack() gives own socket stackdata if not in use */
		for(a=0; a<ntree->stacksize; a++, ns++) ns->hasinput= 1;
		
		/* tag used outputs, so we know when we can skip operations */
		for(node= ntree->nodes.first; node; node= node->next) {
			bNodeSocket *sock;
			
			/* non-composite trees do all nodes by default */
			if(ntree->type!=NTREE_COMPOSIT)
				node->need_exec= 1;

			for(sock= node->inputs.first; sock; sock= sock->next) {
				ns = get_socket_stack(ntree->stack, sock, NULL);
				if (ns) {
					ns->hasoutput = 1;
					
					/* sock type is needed to detect rgba or value or vector types */
					if(sock->link && sock->link->fromsock)
						ns->sockettype= sock->link->fromsock->type;
					else
						sock->ns.sockettype= sock->type;
				}
				
				if(sock->link) {
					bNodeLink *link= sock->link;
					/* this is the test for a cyclic case */
					if(link->fromnode && link->tonode) {
						if(link->fromnode->level >= link->tonode->level && link->tonode->level!=0xFFF);
						else {
							node->need_exec= 0;
						}
					}
				}
			}
			
			/* set stack types (for local stack entries) */
			for(sock= node->outputs.first; sock; sock= sock->next) {
				ns = get_socket_stack(ntree->stack, sock, NULL);
				if (ns)
					ns->sockettype = sock->type;
			}
			
			if(node->type==NODE_GROUP && node->id) {
				node_get_stack(node, ntree->stack, nsin, NULL, NULL);
				group_tag_used_outputs(node, ntree->stack, nsin);
			}
		}
		
		if(ntree->type==NTREE_COMPOSIT)
			composit_begin_exec(ntree, ntree->stack);
	}
	
	ntree->init |= NTREE_EXEC_INIT;
}

void ntreeEndExecTree(bNodeTree *ntree)
{
	bNodeStack *ns;
	
	if(ntree->init & NTREE_EXEC_INIT) {
		bNodeThreadStack *nts;
		int a;
		
		/* another callback candidate! */
		if(ntree->type==NTREE_COMPOSIT) {
			composit_end_exec(ntree, ntree->stack);
			
			for(ns= ntree->stack, a=0; a<ntree->stacksize; a++, ns++) {
				if(ns->data) {
					printf("freed leftover buffer from stack\n");
					free_compbuf(ns->data);
					ns->data= NULL;
				}
			}
		}
		else if(ntree->type==NTREE_TEXTURE)
			tex_end_exec(ntree);
		
		if(ntree->stack) {
			MEM_freeN(ntree->stack);
			ntree->stack= NULL;
		}

		if(ntree->threadstack) {
			for(a=0; a<BLENDER_MAX_THREADS; a++) {
				for(nts=ntree->threadstack[a].first; nts; nts=nts->next)
					if (nts->stack) MEM_freeN(nts->stack);
				BLI_freelistN(&ntree->threadstack[a]);
			}

			MEM_freeN(ntree->threadstack);
			ntree->threadstack= NULL;
		}

		ntree->init &= ~NTREE_EXEC_INIT;
	}
}

/* nodes are presorted, so exec is in order of list */
void ntreeExecTree(bNodeTree *ntree, void *callerdata, int thread)
{
	bNode *node;
	bNodeStack *nsin[MAX_SOCKET];	/* arbitrary... watch this */
	bNodeStack *nsout[MAX_SOCKET];	/* arbitrary... watch this */
	bNodeStack *stack;
	bNodeThreadStack *nts = NULL;
	
	/* only when initialized */
	if((ntree->init & NTREE_EXEC_INIT)==0)
		ntreeBeginExecTree(ntree);
	
	/* composite does 1 node per thread, so no multiple stacks needed */
	if(ntree->type==NTREE_COMPOSIT) {
		stack= ntree->stack;
	}
	else {
		nts= ntreeGetThreadStack(ntree, thread);
		stack= nts->stack;
	}
	
	for(node= ntree->nodes.first; node; node= node->next) {
		if(node->need_exec) {
			if(node->typeinfo->execfunc) {
				node_get_stack(node, stack, nsin, nsout, NULL);
				node->typeinfo->execfunc(callerdata, node, nsin, nsout);
			}
			else if(node->type==NODE_GROUP && node->id) {
				node_get_stack(node, stack, nsin, NULL, NULL);
				node_group_execute(stack, callerdata, node, nsin);
			}
		}
	}

	if(nts)
		ntreeReleaseThreadStack(nts);
}


/* ***************************** threaded version for execute composite nodes ************* */
/* these are nodes without input, only giving values */
/* or nodes with only value inputs */
static int node_only_value(bNode *node)
{
	bNodeSocket *sock;
	
	if(ELEM3(node->type, CMP_NODE_TIME, CMP_NODE_VALUE, CMP_NODE_RGB))
		return 1;
	
	/* doing this for all node types goes wrong. memory free errors */
	if(node->inputs.first && node->type==CMP_NODE_MAP_VALUE) {
		int retval= 1;
		for(sock= node->inputs.first; sock; sock= sock->next) {
			if(sock->link && sock->link->fromnode)
				retval &= node_only_value(sock->link->fromnode);
		}
		return retval;
	}
	return 0;
}


/* not changing info, for thread callback */
typedef struct ThreadData {
	bNodeStack *stack;
	RenderData *rd;
} ThreadData;

static void *exec_composite_node(void *node_v)
{
	bNodeStack *nsin[MAX_SOCKET];	/* arbitrary... watch this */
	bNodeStack *nsout[MAX_SOCKET];	/* arbitrary... watch this */
	bNode *node= node_v;
	ThreadData *thd= (ThreadData *)node->threaddata;
	
	node_get_stack(node, thd->stack, nsin, nsout, NULL);
	
	if((node->flag & NODE_MUTED) && (!node_only_value(node))) {
		/* viewers we execute, for feedback to user */
		if(ELEM(node->type, CMP_NODE_VIEWER, CMP_NODE_SPLITVIEWER)) 
			node->typeinfo->execfunc(thd->rd, node, nsin, nsout);
		else
			node_compo_pass_on(node, nsin, nsout);
	}
	else if(node->typeinfo->execfunc) {
		node->typeinfo->execfunc(thd->rd, node, nsin, nsout);
	}
	else if(node->type==NODE_GROUP && node->id) {
		node_group_execute(thd->stack, thd->rd, node, nsin); 
	}
	
	node->exec |= NODE_READY;
	return NULL;
}

/* return total of executable nodes, for timecursor */
/* only compositor uses it */
static int setExecutableNodes(bNodeTree *ntree, ThreadData *thd)
{
	bNodeStack *nsin[MAX_SOCKET];	/* arbitrary... watch this */
	bNodeStack *nsout[MAX_SOCKET];	/* arbitrary... watch this */
	bNode *node;
	bNodeSocket *sock;
	int totnode= 0, group_edit= 0;
	
	/* note; do not add a dependency sort here, the stack was created already */
	
	/* if we are in group edit, viewer nodes get skipped when group has viewer */
	for(node= ntree->nodes.first; node; node= node->next)
		if(node->type==NODE_GROUP && (node->flag & NODE_GROUP_EDIT))
			if(ntreeHasType((bNodeTree *)node->id, CMP_NODE_VIEWER))
				group_edit= 1;
	
	for(node= ntree->nodes.first; node; node= node->next) {
		int a;
		
		node_get_stack(node, thd->stack, nsin, nsout, NULL);
		
		/* test the outputs */
		/* skip value-only nodes (should be in type!) */
		if(!node_only_value(node)) {
			for(a=0, sock= node->outputs.first; sock; sock= sock->next, a++) {
				if(nsout[a]->data==NULL && nsout[a]->hasoutput) {
					node->need_exec= 1;
					break;
				}
			}
		}
		
		/* test the inputs */
		for(a=0, sock= node->inputs.first; sock; sock= sock->next, a++) {
			/* skip viewer nodes in bg render or group edit */
			if( ELEM(node->type, CMP_NODE_VIEWER, CMP_NODE_SPLITVIEWER) && (G.background || group_edit))
				node->need_exec= 0;
			/* is sock in use? */
			else if(sock->link) {
				bNodeLink *link= sock->link;
				
				/* this is the test for a cyclic case */
				if(link->fromnode==NULL || link->tonode==NULL);
				else if(link->fromnode->level >= link->tonode->level && link->tonode->level!=0xFFF) {
					if(link->fromnode->need_exec) {
						node->need_exec= 1;
						break;
					}
				}
				else {
					node->need_exec= 0;
					printf("Node %s skipped, cyclic dependency\n", node->name);
				}
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
			/* printf("node needs exec %s\n", node->name); */
			
			/* tag for getExecutableNode() */
			node->exec= 0;
		}
		else {
			/* tag for getExecutableNode() */
			node->exec= NODE_READY|NODE_FINISHED|NODE_SKIPPED;
			
		}
	}
	
	/* last step: set the stack values for only-value nodes */
	/* just does all now, compared to a full buffer exec this is nothing */
	if(totnode) {
		for(node= ntree->nodes.first; node; node= node->next) {
			if(node->need_exec==0 && node_only_value(node)) {
				if(node->typeinfo->execfunc) {
					node_get_stack(node, thd->stack, nsin, nsout, NULL);
					node->typeinfo->execfunc(thd->rd, node, nsin, nsout);
				}
			}
		}
	}
	
	return totnode;
}

/* while executing tree, free buffers from nodes that are not needed anymore */
static void freeExecutableNode(bNodeTree *ntree)
{
	/* node outputs can be freed when:
	- not a render result or image node
	- when node outputs go to nodes all being set NODE_FINISHED
	*/
	bNode *node;
	bNodeSocket *sock;
	
	/* set exec flag for finished nodes that might need freed */
	for(node= ntree->nodes.first; node; node= node->next) {
		if(node->type!=CMP_NODE_R_LAYERS)
			if(node->exec & NODE_FINISHED)
				node->exec |= NODE_FREEBUFS;
	}
	/* clear this flag for input links that are not done yet */
	for(node= ntree->nodes.first; node; node= node->next) {
		if((node->exec & NODE_FINISHED)==0) {
			for(sock= node->inputs.first; sock; sock= sock->next)
				if(sock->link && sock->link->fromnode)
					sock->link->fromnode->exec &= ~NODE_FREEBUFS;
		}
	}
	/* now we can free buffers */
	for(node= ntree->nodes.first; node; node= node->next) {
		if(node->exec & NODE_FREEBUFS) {
			for(sock= node->outputs.first; sock; sock= sock->next) {
				bNodeStack *ns= get_socket_stack(ntree->stack, sock, NULL);
				if(ns && ns->data) {
					free_compbuf(ns->data);
					ns->data= NULL;
					// printf("freed buf node %s \n", node->name);
				}
			}
		}
	}
}

static bNode *getExecutableNode(bNodeTree *ntree)
{
	bNode *node;
	bNodeSocket *sock;
	
	for(node= ntree->nodes.first; node; node= node->next) {
		if(node->exec==0) {
			
			/* input sockets should be ready */
			for(sock= node->inputs.first; sock; sock= sock->next) {
				if(sock->link && sock->link->fromnode)
					if((sock->link->fromnode->exec & NODE_READY)==0)
						break;
			}
			if(sock==NULL)
				return node;
		}
	}
	return NULL;
}

/* check if texture nodes need exec or end */
static  void ntree_composite_texnode(bNodeTree *ntree, int init)
{
	bNode *node;
	
	for(node= ntree->nodes.first; node; node= node->next) {
		if(node->type==CMP_NODE_TEXTURE && node->id) {
			Tex *tex= (Tex *)node->id;
			if(tex->nodetree && tex->use_nodes) {
				/* has internal flag to detect it only does it once */
				if(init)
					ntreeBeginExecTree(tex->nodetree); 
				else
					ntreeEndExecTree(tex->nodetree);
			}
		}
	}

}

/* optimized tree execute test for compositing */
void ntreeCompositExecTree(bNodeTree *ntree, RenderData *rd, int do_preview)
{
	bNode *node;
	ListBase threads;
	ThreadData thdata;
	int totnode, curnode, rendering= 1;
	
	if(ntree==NULL) return;
	
	if(do_preview)
		ntreeInitPreview(ntree, 0, 0);
	
	ntreeBeginExecTree(ntree);
	ntree_composite_texnode(ntree, 1);
	
	/* prevent unlucky accidents */
	if(G.background)
		rd->scemode &= ~R_COMP_CROP;
	
	/* setup callerdata for thread callback */
	thdata.rd= rd;
	thdata.stack= ntree->stack;
	
	/* fixed seed, for example noise texture */
	BLI_srandom(rd->cfra);

	/* ensures only a single output node is enabled */
	ntreeSetOutput(ntree);

	/* sets need_exec tags in nodes */
	curnode = totnode= setExecutableNodes(ntree, &thdata);

	BLI_init_threads(&threads, exec_composite_node, rd->threads);
	
	while(rendering) {
		
		if(BLI_available_threads(&threads)) {
			node= getExecutableNode(ntree);
			if(node) {
				if(ntree->progress && totnode)
					ntree->progress(ntree->prh, (1.0f - curnode/(float)totnode));
				if(ntree->stats_draw) {
					char str[64];
					sprintf(str, "Compositing %d %s", curnode, node->name);
					ntree->stats_draw(ntree->sdh, str);
				}
				curnode--;
				
				node->threaddata = &thdata;
				node->exec= NODE_PROCESSING;
				BLI_insert_thread(&threads, node);
			}
			else
				PIL_sleep_ms(50);
		}
		else
			PIL_sleep_ms(50);
		
		rendering= 0;
		/* test for ESC */
		if(ntree->test_break && ntree->test_break(ntree->tbh)) {
			for(node= ntree->nodes.first; node; node= node->next)
				node->exec |= NODE_READY;
		}
		
		/* check for ready ones, and if we need to continue */
		for(node= ntree->nodes.first; node; node= node->next) {
			if(node->exec & NODE_READY) {
				if((node->exec & NODE_FINISHED)==0) {
					BLI_remove_thread(&threads, node); /* this waits for running thread to finish btw */
					node->exec |= NODE_FINISHED;
					
					/* freeing unused buffers */
					if(rd->scemode & R_COMP_FREE)
						freeExecutableNode(ntree);
				}
			}
			else rendering= 1;
		}
	}
	
	BLI_end_threads(&threads);
	
	ntreeEndExecTree(ntree);
}


/* ********** copy composite tree entirely, to allow threaded exec ******************* */
/* ***************** do NOT execute this in a thread!               ****************** */

/* returns localized tree for execution in threads */
/* local tree then owns all compbufs (for composite) */
bNodeTree *ntreeLocalize(bNodeTree *ntree)
{
	bNodeTree *ltree;
	bNode *node;
	bNodeSocket *sock;
	
	bAction *action_backup= NULL, *tmpact_backup= NULL;
	
	/* Workaround for copying an action on each render!
	 * set action to NULL so animdata actions dont get copied */
	AnimData *adt= BKE_animdata_from_id(&ntree->id);

	if(adt) {
		action_backup= adt->action;
		tmpact_backup= adt->tmpact;

		adt->action= NULL;
		adt->tmpact= NULL;
	}

	/* node copy func */
	ltree= ntreeCopyTree(ntree);

	if(adt) {
		AnimData *ladt= BKE_animdata_from_id(&ltree->id);

		adt->action= ladt->action= action_backup;
		adt->tmpact= ladt->tmpact= tmpact_backup;

		if(action_backup) action_backup->id.us++;
		if(tmpact_backup) tmpact_backup->id.us++;
		
	}
	/* end animdata uglyness */

	/* ensures only a single output node is enabled */
	ntreeSetOutput(ltree);

	for(node= ntree->nodes.first; node; node= node->next) {
		
		/* store new_node pointer to original */
		node->new_node->new_node= node;
		
		if(ntree->type==NTREE_COMPOSIT) {
			/* ensure new user input gets handled ok, only composites (texture nodes will break, for painting since it uses no tags) */
			node->need_exec= 0;
			
			/* move over the compbufs */
			/* right after ntreeCopyTree() oldsock pointers are valid */
			
			if(ELEM(node->type, CMP_NODE_VIEWER, CMP_NODE_SPLITVIEWER)) {
				if(node->id) {
					if(node->flag & NODE_DO_OUTPUT)
						node->new_node->id= (ID *)copy_image((Image *)node->id);
					else
						node->new_node->id= NULL;
				}
			}
			
			for(sock= node->outputs.first; sock; sock= sock->next) {
				
				sock->new_sock->ns.data= sock->ns.data;
				compbuf_set_node(sock->new_sock->ns.data, node->new_node);
				
				sock->ns.data= NULL;
				sock->new_sock->new_sock= sock;
			}
		}
	}
	
	return ltree;
}

static int node_exists(bNodeTree *ntree, bNode *testnode)
{
	bNode *node= ntree->nodes.first;
	for(; node; node= node->next)
		if(node==testnode)
			return 1;
	return 0;
}

static int outsocket_exists(bNode *node, bNodeSocket *testsock)
{
	bNodeSocket *sock= node->outputs.first;
	for(; sock; sock= sock->next)
		if(sock==testsock)
			return 1;
	return 0;
}


/* sync local composite with real tree */
/* local composite is supposed to be running, be careful moving previews! */
/* is called by jobs manager, outside threads, so it doesnt happen during draw */
void ntreeLocalSync(bNodeTree *localtree, bNodeTree *ntree)
{
	bNode *lnode;
	
	if(ntree->type==NTREE_COMPOSIT) {
		/* move over the compbufs and previews */
		for(lnode= localtree->nodes.first; lnode; lnode= lnode->next) {
			if( (lnode->exec & NODE_READY) && !(lnode->exec & NODE_SKIPPED) ) {
				if(node_exists(ntree, lnode->new_node)) {
					
					if(lnode->preview && lnode->preview->rect) {
						node_free_preview(lnode->new_node);
						lnode->new_node->preview= lnode->preview;
						lnode->preview= NULL;
					}
				}
			}
		}
	}
	else if(ELEM(ntree->type, NTREE_SHADER, NTREE_TEXTURE)) {
		/* copy over contents of previews */
		for(lnode= localtree->nodes.first; lnode; lnode= lnode->next) {
			if(node_exists(ntree, lnode->new_node)) {
				bNode *node= lnode->new_node;
				
				if(node->preview && node->preview->rect) {
					if(lnode->preview && lnode->preview->rect) {
						int xsize= node->preview->xsize;
						int ysize= node->preview->ysize;
						memcpy(node->preview->rect, lnode->preview->rect, 4*xsize + xsize*ysize*sizeof(char)*4);
					}
				}
			}
		}
	}
}

/* merge local tree results back, and free local tree */
/* we have to assume the editor already changed completely */
void ntreeLocalMerge(bNodeTree *localtree, bNodeTree *ntree)
{
	bNode *lnode;
	bNodeSocket *lsock;
	
	/* move over the compbufs and previews */
	for(lnode= localtree->nodes.first; lnode; lnode= lnode->next) {
		if(node_exists(ntree, lnode->new_node)) {
			
			if(lnode->preview && lnode->preview->rect) {
				node_free_preview(lnode->new_node);
				lnode->new_node->preview= lnode->preview;
				lnode->preview= NULL;
			}
			
			if(ELEM(lnode->type, CMP_NODE_VIEWER, CMP_NODE_SPLITVIEWER)) {
				if(lnode->id && (lnode->flag & NODE_DO_OUTPUT)) {
					/* image_merge does sanity check for pointers */
					BKE_image_merge((Image *)lnode->new_node->id, (Image *)lnode->id);
				}
			}
			
			for(lsock= lnode->outputs.first; lsock; lsock= lsock->next) {
				if(outsocket_exists(lnode->new_node, lsock->new_sock)) {
					lsock->new_sock->ns.data= lsock->ns.data;
					compbuf_set_node(lsock->new_sock->ns.data, lnode->new_node);
					lsock->ns.data= NULL;
					lsock->new_sock= NULL;
				}
			}
		}
	}
	ntreeFreeTree(localtree);
	MEM_freeN(localtree);
}

/* *********************************************** */

/* GPU material from shader nodes */

static void gpu_from_node_stack(ListBase *sockets, bNodeStack **ns, GPUNodeStack *gs)
{
	bNodeSocket *sock;
	int i;

	for (sock=sockets->first, i=0; sock; sock=sock->next, i++) {
		memset(&gs[i], 0, sizeof(gs[i]));

		QUATCOPY(gs[i].vec, ns[i]->vec);
		gs[i].link= ns[i]->data;

		if (sock->type == SOCK_VALUE)
			gs[i].type= GPU_FLOAT;
		else if (sock->type == SOCK_VECTOR)
			gs[i].type= GPU_VEC3;
		else if (sock->type == SOCK_RGBA)
			gs[i].type= GPU_VEC4;
		else
			gs[i].type= GPU_NONE;

		gs[i].name = "";
		gs[i].hasinput= ns[i]->hasinput && ns[i]->data;
		gs[i].hasoutput= ns[i]->hasoutput && ns[i]->data;
		gs[i].sockettype= ns[i]->sockettype;
	}

	gs[i].type= GPU_NONE;
}

static void data_from_gpu_stack(ListBase *sockets, bNodeStack **ns, GPUNodeStack *gs)
{
	bNodeSocket *sock;
	int i;

	for (sock=sockets->first, i=0; sock; sock=sock->next, i++) {
		ns[i]->data= gs[i].link;
		ns[i]->sockettype= gs[i].sockettype;
	}
}

static void gpu_node_group_execute(bNodeStack *stack, GPUMaterial *mat, bNode *gnode, bNodeStack **in)
{
	bNode *node;
	bNodeTree *ntree= (bNodeTree *)gnode->id;
	bNodeStack *nsin[MAX_SOCKET];	/* arbitrary... watch this */
	bNodeStack *nsout[MAX_SOCKET];	/* arbitrary... watch this */
	GPUNodeStack gpuin[MAX_SOCKET+1], gpuout[MAX_SOCKET+1];
	int doit = 0;
	
	if(ntree==NULL) return;
	
	stack+= gnode->stack_index;
		
	for(node= ntree->nodes.first; node; node= node->next) {
		if(node->typeinfo->gpufunc) {
			node_get_stack(node, stack, nsin, nsout, in);

			doit = 0;
			
			/* for groups, only execute outputs for edited group */
			if(node->typeinfo->nclass==NODE_CLASS_OUTPUT) {
				if(gnode->flag & NODE_GROUP_EDIT)
					if(node->flag & NODE_DO_OUTPUT)
						doit = 1;
			}
			else
				doit = 1;

			if(doit)  {
				gpu_from_node_stack(&node->inputs, nsin, gpuin);
				gpu_from_node_stack(&node->outputs, nsout, gpuout);
				if(node->typeinfo->gpufunc(mat, node, gpuin, gpuout))
					data_from_gpu_stack(&node->outputs, nsout, gpuout);
			}
		}
	}
}

void ntreeGPUMaterialNodes(bNodeTree *ntree, GPUMaterial *mat)
{
	bNode *node;
	bNodeStack *stack;
	bNodeStack *nsin[MAX_SOCKET];	/* arbitrary... watch this */
	bNodeStack *nsout[MAX_SOCKET];	/* arbitrary... watch this */
	GPUNodeStack gpuin[MAX_SOCKET+1], gpuout[MAX_SOCKET+1];

	if((ntree->init & NTREE_EXEC_INIT)==0)
		ntreeBeginExecTree(ntree);

	stack= ntree->stack;

	for(node= ntree->nodes.first; node; node= node->next) {
		if(node->typeinfo->gpufunc) {
			node_get_stack(node, stack, nsin, nsout, NULL);
			gpu_from_node_stack(&node->inputs, nsin, gpuin);
			gpu_from_node_stack(&node->outputs, nsout, gpuout);
			if(node->typeinfo->gpufunc(mat, node, gpuin, gpuout))
				data_from_gpu_stack(&node->outputs, nsout, gpuout);
		}
		else if(node->type==NODE_GROUP && node->id) {
			node_get_stack(node, stack, nsin, nsout, NULL);
			gpu_node_group_execute(stack, mat, node, nsin);
		}
	}

	ntreeEndExecTree(ntree);
}

/* **************** call to switch lamploop for material node ************ */

void (*node_shader_lamp_loop)(struct ShadeInput *, struct ShadeResult *);

void set_node_shader_lamp_loop(void (*lamp_loop_func)(ShadeInput *, ShadeResult *))
{
	node_shader_lamp_loop= lamp_loop_func;
}

/* clumsy checking... should do dynamic outputs once */
static void force_hidden_passes(bNode *node, int passflag)
{
	bNodeSocket *sock;
	
	for(sock= node->outputs.first; sock; sock= sock->next)
		sock->flag &= ~SOCK_UNAVAIL;
	
	sock= BLI_findlink(&node->outputs, RRES_OUT_Z);
	if(!(passflag & SCE_PASS_Z)) sock->flag |= SOCK_UNAVAIL;
	sock= BLI_findlink(&node->outputs, RRES_OUT_NORMAL);
	if(!(passflag & SCE_PASS_NORMAL)) sock->flag |= SOCK_UNAVAIL;
	sock= BLI_findlink(&node->outputs, RRES_OUT_VEC);
	if(!(passflag & SCE_PASS_VECTOR)) sock->flag |= SOCK_UNAVAIL;
	sock= BLI_findlink(&node->outputs, RRES_OUT_UV);
	if(!(passflag & SCE_PASS_UV)) sock->flag |= SOCK_UNAVAIL;
	sock= BLI_findlink(&node->outputs, RRES_OUT_RGBA);
	if(!(passflag & SCE_PASS_RGBA)) sock->flag |= SOCK_UNAVAIL;
	sock= BLI_findlink(&node->outputs, RRES_OUT_DIFF);
	if(!(passflag & SCE_PASS_DIFFUSE)) sock->flag |= SOCK_UNAVAIL;
	sock= BLI_findlink(&node->outputs, RRES_OUT_SPEC);
	if(!(passflag & SCE_PASS_SPEC)) sock->flag |= SOCK_UNAVAIL;
	sock= BLI_findlink(&node->outputs, RRES_OUT_SHADOW);
	if(!(passflag & SCE_PASS_SHADOW)) sock->flag |= SOCK_UNAVAIL;
	sock= BLI_findlink(&node->outputs, RRES_OUT_AO);
	if(!(passflag & SCE_PASS_AO)) sock->flag |= SOCK_UNAVAIL;
	sock= BLI_findlink(&node->outputs, RRES_OUT_REFLECT);
	if(!(passflag & SCE_PASS_REFLECT)) sock->flag |= SOCK_UNAVAIL;
	sock= BLI_findlink(&node->outputs, RRES_OUT_REFRACT);
	if(!(passflag & SCE_PASS_REFRACT)) sock->flag |= SOCK_UNAVAIL;
	sock= BLI_findlink(&node->outputs, RRES_OUT_INDIRECT);
	if(!(passflag & SCE_PASS_INDIRECT)) sock->flag |= SOCK_UNAVAIL;
	sock= BLI_findlink(&node->outputs, RRES_OUT_INDEXOB);
	if(!(passflag & SCE_PASS_INDEXOB)) sock->flag |= SOCK_UNAVAIL;
	sock= BLI_findlink(&node->outputs, RRES_OUT_INDEXMA);
	if(!(passflag & SCE_PASS_INDEXMA)) sock->flag |= SOCK_UNAVAIL;
	sock= BLI_findlink(&node->outputs, RRES_OUT_MIST);
	if(!(passflag & SCE_PASS_MIST)) sock->flag |= SOCK_UNAVAIL;
	sock= BLI_findlink(&node->outputs, RRES_OUT_EMIT);
	if(!(passflag & SCE_PASS_EMIT)) sock->flag |= SOCK_UNAVAIL;
	sock= BLI_findlink(&node->outputs, RRES_OUT_ENV);
	if(!(passflag & SCE_PASS_ENVIRONMENT)) sock->flag |= SOCK_UNAVAIL;
	
}

/* based on rules, force sockets hidden always */
void ntreeCompositForceHidden(bNodeTree *ntree, Scene *curscene)
{
	bNode *node;
	
	if(ntree==NULL) return;
	
	for(node= ntree->nodes.first; node; node= node->next) {
		if( node->type==CMP_NODE_R_LAYERS) {
			Scene *sce= node->id?(Scene *)node->id:curscene;
			SceneRenderLayer *srl= BLI_findlink(&sce->r.layers, node->custom1);
			if(srl)
				force_hidden_passes(node, srl->passflag);
		}
		else if( node->type==CMP_NODE_IMAGE) {
			Image *ima= (Image *)node->id;
			if(ima) {
				if(ima->rr) {
					ImageUser *iuser= node->storage;
					RenderLayer *rl= BLI_findlink(&ima->rr->layers, iuser->layer);
					if(rl)
						force_hidden_passes(node, rl->passflag);
					else
						force_hidden_passes(node, 0);
				}
				else if(ima->type!=IMA_TYPE_MULTILAYER) {	/* if ->rr not yet read we keep inputs */
					force_hidden_passes(node, RRES_OUT_Z);
				}
				else
					force_hidden_passes(node, 0);
			}
			else
				force_hidden_passes(node, 0);
		}
	}

}

/* called from render pipeline, to tag render input and output */
/* need to do all scenes, to prevent errors when you re-render 1 scene */
void ntreeCompositTagRender(Scene *curscene)
{
	Scene *sce;
	
	for(sce= G.main->scene.first; sce; sce= sce->id.next) {
		if(sce->nodetree) {
			bNode *node;
			
			for(node= sce->nodetree->nodes.first; node; node= node->next) {
				if(node->id==(ID *)curscene || node->type==CMP_NODE_COMPOSITE)
					NodeTagChanged(sce->nodetree, node);
				else if(node->type==CMP_NODE_TEXTURE) /* uses scene sizex/sizey */
					NodeTagChanged(sce->nodetree, node);
			}
		}
	}
}

static int node_animation_properties(bNodeTree *ntree, bNode *node)
{
	bNodeSocket *sock;
	const ListBase *lb;
	Link *link;
	PointerRNA ptr;
	PropertyRNA *prop;
	
	/* check to see if any of the node's properties have fcurves */
	RNA_pointer_create((ID *)ntree, &RNA_Node, node, &ptr);
	lb = RNA_struct_type_properties(ptr.type);
	
	for (link=lb->first; link; link=link->next) {
		int driven, len=1, index;
		prop = (PropertyRNA *)link;
		
		if (RNA_property_array_check(&ptr, prop))
			len = RNA_property_array_length(&ptr, prop);
		
		for (index=0; index<len; index++) {
			if (rna_get_fcurve(&ptr, prop, index, NULL, &driven)) {
				NodeTagChanged(ntree, node);
				return 1;
			}
		}
	}
	
	/* now check node sockets */
	for (sock = node->inputs.first; sock; sock=sock->next) {
		int driven, len=1, index;
		
		RNA_pointer_create((ID *)ntree, &RNA_NodeSocket, sock, &ptr);
		prop = RNA_struct_find_property(&ptr, "default_value");
		
		if (RNA_property_array_check(&ptr, prop))
			len = RNA_property_array_length(&ptr, prop);
		
		for (index=0; index<len; index++) {
			if (rna_get_fcurve(&ptr, prop, index, NULL, &driven)) {
				NodeTagChanged(ntree, node);
				return 1;
			}
		}
	}

	return 0;
}

/* tags nodes that have animation capabilities */
int ntreeCompositTagAnimated(bNodeTree *ntree)
{
	bNode *node;
	int tagged= 0;
	
	if(ntree==NULL) return 0;
	
	for(node= ntree->nodes.first; node; node= node->next) {
		
		tagged = node_animation_properties(ntree, node);
		
		/* otherwise always tag these node types */
		if(node->type==CMP_NODE_IMAGE) {
			Image *ima= (Image *)node->id;
			if(ima && ELEM(ima->source, IMA_SRC_MOVIE, IMA_SRC_SEQUENCE)) {
				NodeTagChanged(ntree, node);
				tagged= 1;
			}
		}
		else if(node->type==CMP_NODE_TIME) {
			NodeTagChanged(ntree, node);
			tagged= 1;
		}
		/* here was tag render layer, but this is called after a render, so re-composites fail */
		else if(node->type==NODE_GROUP) {
			if( ntreeCompositTagAnimated((bNodeTree *)node->id) ) {
				NodeTagChanged(ntree, node);
			}
		}
	}
	
	return tagged;
}


/* called from image window preview */
void ntreeCompositTagGenerators(bNodeTree *ntree)
{
	bNode *node;
	
	if(ntree==NULL) return;
	
	for(node= ntree->nodes.first; node; node= node->next) {
		if( ELEM(node->type, CMP_NODE_R_LAYERS, CMP_NODE_IMAGE))
			NodeTagChanged(ntree, node);
	}
}

/* XXX after render animation system gets a refresh, this call allows composite to end clean */
void ntreeClearTags(bNodeTree *ntree)
{
	bNode *node;
	
	if(ntree==NULL) return;
	
	for(node= ntree->nodes.first; node; node= node->next) {
		node->need_exec= 0;
		if(node->type==NODE_GROUP)
			ntreeClearTags((bNodeTree *)node->id);
	}
}


int ntreeTexTagAnimated(bNodeTree *ntree)
{
	bNode *node;
	
	if(ntree==NULL) return 0;
	
	for(node= ntree->nodes.first; node; node= node->next) {
		if(node->type==TEX_NODE_CURVE_TIME) {
			NodeTagChanged(ntree, node);
			return 1;
		}
		else if(node->type==NODE_GROUP) {
			if( ntreeTexTagAnimated((bNodeTree *)node->id) ) {
				return 1;
			}
		}
	}
	
	return 0;
}

/* ************* node definition init ********** */

void node_type_base(bNodeType *ntype, int type, const char *name, short nclass, short flag,
					struct bNodeSocketType *inputs, struct bNodeSocketType *outputs)
{
	memset(ntype, 0, sizeof(bNodeType));
	
	ntype->type = type;
	ntype->name = name;
	ntype->nclass = nclass;
	ntype->flag = flag;
	
	ntype->inputs = inputs;
	ntype->outputs = outputs;
	
	/* default size values */
	ntype->width = 140;
	ntype->minwidth = 100;
	ntype->maxwidth = 320;
}

void node_type_init(bNodeType *ntype, void (*initfunc)(struct bNode *))
{
	ntype->initfunc = initfunc;
}

void node_type_size(struct bNodeType *ntype, int width, int minwidth, int maxwidth)
{
	ntype->width = width;
	ntype->minwidth = minwidth;
	ntype->maxwidth = maxwidth;
}

void node_type_storage(bNodeType *ntype, const char *storagename, void (*freestoragefunc)(struct bNode *), void (*copystoragefunc)(struct bNode *, struct bNode *))
{
	if (storagename)
		strncpy(ntype->storagename, storagename, sizeof(ntype->storagename));
	else
		ntype->storagename[0] = '\0';
	ntype->copystoragefunc = copystoragefunc;
	ntype->freestoragefunc = freestoragefunc;
}

void node_type_exec(struct bNodeType *ntype, void (*execfunc)(void *data, struct bNode *, struct bNodeStack **, struct bNodeStack **))
{
	ntype->execfunc = execfunc;
}

void node_type_gpu(struct bNodeType *ntype, int (*gpufunc)(struct GPUMaterial *mat, struct bNode *node, struct GPUNodeStack *in, struct GPUNodeStack *out))
{
	ntype->gpufunc = gpufunc;
}

void node_type_label(struct bNodeType *ntype, const char *(*labelfunc)(struct bNode *))
{
	ntype->labelfunc = labelfunc;
}

static bNodeType *is_nodetype_registered(ListBase *typelist, int type, ID *id) 
{
	bNodeType *ntype= typelist->first;
	
	for(;ntype; ntype= ntype->next )
		if(ntype->type==type && ntype->id==id)
			return ntype;
	
	return NULL;
}

/* type can be from a static array, we make copy for duplicate types (like group) */
void nodeRegisterType(ListBase *typelist, const bNodeType *ntype) 
{
	bNodeType *found= is_nodetype_registered(typelist, ntype->type, ntype->id);
	
	if(found==NULL) {
		bNodeType *ntypen= MEM_callocN(sizeof(bNodeType), "node type");
		*ntypen= *ntype;
		BLI_addtail(typelist, ntypen);
	 }
}

static void registerCompositNodes(ListBase *ntypelist)
{
	register_node_type_group(ntypelist);
	
	register_node_type_cmp_rlayers(ntypelist);
	register_node_type_cmp_image(ntypelist);
	register_node_type_cmp_texture(ntypelist);
	register_node_type_cmp_value(ntypelist);
	register_node_type_cmp_rgb(ntypelist);
	register_node_type_cmp_curve_time(ntypelist);
	
	register_node_type_cmp_composite(ntypelist);
	register_node_type_cmp_viewer(ntypelist);
	register_node_type_cmp_splitviewer(ntypelist);
	register_node_type_cmp_output_file(ntypelist);
	register_node_type_cmp_view_levels(ntypelist);
	
	register_node_type_cmp_curve_rgb(ntypelist);
	register_node_type_cmp_mix_rgb(ntypelist);
	register_node_type_cmp_hue_sat(ntypelist);
	register_node_type_cmp_brightcontrast(ntypelist);
	register_node_type_cmp_gamma(ntypelist);
	register_node_type_cmp_invert(ntypelist);
	register_node_type_cmp_alphaover(ntypelist);
	register_node_type_cmp_zcombine(ntypelist);
	register_node_type_cmp_colorbalance(ntypelist);
	register_node_type_cmp_huecorrect(ntypelist);
	
	register_node_type_cmp_normal(ntypelist);
	register_node_type_cmp_curve_vec(ntypelist);
	register_node_type_cmp_map_value(ntypelist);
	register_node_type_cmp_normalize(ntypelist);
	
	register_node_type_cmp_filter(ntypelist);
	register_node_type_cmp_blur(ntypelist);
	register_node_type_cmp_dblur(ntypelist);
	register_node_type_cmp_bilateralblur(ntypelist);
	register_node_type_cmp_vecblur(ntypelist);
	register_node_type_cmp_dilateerode(ntypelist);
	register_node_type_cmp_defocus(ntypelist);
	
	register_node_type_cmp_valtorgb(ntypelist);
	register_node_type_cmp_rgbtobw(ntypelist);
	register_node_type_cmp_setalpha(ntypelist);
	register_node_type_cmp_idmask(ntypelist);
	register_node_type_cmp_math(ntypelist);
	register_node_type_cmp_seprgba(ntypelist);
	register_node_type_cmp_combrgba(ntypelist);
	register_node_type_cmp_sephsva(ntypelist);
	register_node_type_cmp_combhsva(ntypelist);
	register_node_type_cmp_sepyuva(ntypelist);
	register_node_type_cmp_combyuva(ntypelist);
	register_node_type_cmp_sepycca(ntypelist);
	register_node_type_cmp_combycca(ntypelist);
	register_node_type_cmp_premulkey(ntypelist);
	
	register_node_type_cmp_diff_matte(ntypelist);
	register_node_type_cmp_distance_matte(ntypelist);
	register_node_type_cmp_chroma_matte(ntypelist);
	register_node_type_cmp_color_matte(ntypelist);
	register_node_type_cmp_channel_matte(ntypelist);
	register_node_type_cmp_color_spill(ntypelist);
	register_node_type_cmp_luma_matte(ntypelist);
	
	register_node_type_cmp_translate(ntypelist);
	register_node_type_cmp_rotate(ntypelist);
	register_node_type_cmp_scale(ntypelist);
	register_node_type_cmp_flip(ntypelist);
	register_node_type_cmp_crop(ntypelist);
	register_node_type_cmp_displace(ntypelist);
	register_node_type_cmp_mapuv(ntypelist);
	register_node_type_cmp_glare(ntypelist);
	register_node_type_cmp_tonemap(ntypelist);
	register_node_type_cmp_lensdist(ntypelist);
}

static void registerShaderNodes(ListBase *ntypelist) 
{
	register_node_type_group(ntypelist);
	
	register_node_type_sh_output(ntypelist);
	register_node_type_sh_mix_rgb(ntypelist);
	register_node_type_sh_valtorgb(ntypelist);
	register_node_type_sh_rgbtobw(ntypelist);
	register_node_type_sh_normal(ntypelist);
	register_node_type_sh_geom(ntypelist);
	register_node_type_sh_mapping(ntypelist);
	register_node_type_sh_curve_vec(ntypelist);
	register_node_type_sh_curve_rgb(ntypelist);
	register_node_type_sh_math(ntypelist);
	register_node_type_sh_vect_math(ntypelist);
	register_node_type_sh_squeeze(ntypelist);
	register_node_type_sh_camera(ntypelist);
	register_node_type_sh_material(ntypelist);
	register_node_type_sh_material_ext(ntypelist);
	register_node_type_sh_value(ntypelist);
	register_node_type_sh_rgb(ntypelist);
	register_node_type_sh_texture(ntypelist);
//	register_node_type_sh_dynamic(ntypelist);
	register_node_type_sh_invert(ntypelist);
	register_node_type_sh_seprgb(ntypelist);
	register_node_type_sh_combrgb(ntypelist);
	register_node_type_sh_hue_sat(ntypelist);
}

static void registerTextureNodes(ListBase *ntypelist)
{
	register_node_type_group(ntypelist);
	
	register_node_type_tex_math(ntypelist);
	register_node_type_tex_mix_rgb(ntypelist);
	register_node_type_tex_valtorgb(ntypelist);
	register_node_type_tex_rgbtobw(ntypelist);
	register_node_type_tex_valtonor(ntypelist);
	register_node_type_tex_curve_rgb(ntypelist);
	register_node_type_tex_curve_time(ntypelist);
	register_node_type_tex_invert(ntypelist);
	register_node_type_tex_hue_sat(ntypelist);
	register_node_type_tex_coord(ntypelist);
	register_node_type_tex_distance(ntypelist);
	register_node_type_tex_compose(ntypelist);
	register_node_type_tex_decompose(ntypelist);
	
	register_node_type_tex_output(ntypelist);
	register_node_type_tex_viewer(ntypelist);
	
	register_node_type_tex_checker(ntypelist);
	register_node_type_tex_texture(ntypelist);
	register_node_type_tex_bricks(ntypelist);
	register_node_type_tex_image(ntypelist);
	
	register_node_type_tex_rotate(ntypelist);
	register_node_type_tex_translate(ntypelist);
	register_node_type_tex_scale(ntypelist);
	register_node_type_tex_at(ntypelist);
	
	register_node_type_tex_proc_voronoi(ntypelist);
	register_node_type_tex_proc_blend(ntypelist);
	register_node_type_tex_proc_magic(ntypelist);
	register_node_type_tex_proc_marble(ntypelist);
	register_node_type_tex_proc_clouds(ntypelist);
	register_node_type_tex_proc_wood(ntypelist);
	register_node_type_tex_proc_musgrave(ntypelist);
	register_node_type_tex_proc_noise(ntypelist);
	register_node_type_tex_proc_stucci(ntypelist);
	register_node_type_tex_proc_distnoise(ntypelist);
}

static void remove_dynamic_typeinfos(ListBase *list)
{
	bNodeType *ntype= list->first;
	bNodeType *next= NULL;
	while(ntype) {
		next= ntype->next;
		if(ntype->type==NODE_DYNAMIC && ntype->id!=NULL) {
			BLI_remlink(list, ntype);
			if(ntype->inputs) {
				bNodeSocketType *sock= ntype->inputs;
				while(sock->type!=-1) {
					MEM_freeN((void *)sock->name);
					sock++;
				}
				MEM_freeN(ntype->inputs);
			}
			if(ntype->outputs) {
				bNodeSocketType *sock= ntype->outputs;
				while(sock->type!=-1) {
					MEM_freeN((void *)sock->name);
					sock++;
				}
				MEM_freeN(ntype->outputs);
			}
			if(ntype->name) {
				MEM_freeN((void *)ntype->name);
			}
			MEM_freeN(ntype);
		}
		ntype= next;
	}
}

void init_nodesystem(void) 
{
	registerCompositNodes(&node_all_composit);
	registerShaderNodes(&node_all_shaders);
	registerTextureNodes(&node_all_textures);
}

void free_nodesystem(void) 
{
	/*remove_dynamic_typeinfos(&node_all_composit);*/ /* unused for now */
	BLI_freelistN(&node_all_composit);
	remove_dynamic_typeinfos(&node_all_shaders);
	BLI_freelistN(&node_all_shaders);
	BLI_freelistN(&node_all_textures);
}

/* called from unlink_scene, when deleting a scene goes over all scenes
 * other than the input, checks if they have render layer nodes referencing
 * the to-be-deleted scene, and resets them to NULL. */

/* XXX needs to get current scene then! */
void clear_scene_in_nodes(Main *bmain, Scene *sce)
{
	Scene *sce1;
	bNode *node;

	for(sce1= bmain->scene.first; sce1; sce1=sce1->id.next) {
		if(sce1!=sce) {
			if(sce1->nodetree) {
				for(node= sce1->nodetree->nodes.first; node; node= node->next) {
					if(node->type==CMP_NODE_R_LAYERS) {
						Scene *nodesce= (Scene *)node->id;
						
						if (nodesce==sce) node->id = NULL;
					}
				}
			}
		}
	}
}

