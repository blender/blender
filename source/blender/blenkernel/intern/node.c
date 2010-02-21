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

#ifndef DISABLE_PYTHON
#include <Python.h>
#endif

#include <stdlib.h>
#include <stddef.h>
#include <string.h>

#include "DNA_anim_types.h"
#include "DNA_ID.h"
#include "DNA_image_types.h"
#include "DNA_node_types.h"
#include "DNA_material_types.h"
#include "DNA_texture_types.h"
#include "DNA_text_types.h"
#include "DNA_scene_types.h"

#include "RNA_access.h"

#include "BKE_blender.h"
#include "BKE_colortools.h"
#include "BKE_fcurve.h"
#include "BKE_global.h"
#include "BKE_image.h"
#include "BKE_library.h"
#include "BKE_main.h"
#include "BKE_node.h"
#include "BKE_texture.h"
#include "BKE_text.h"
#include "BKE_utildefines.h"
#include "BKE_animsys.h" /* BKE_free_animdata only */

#include "BLI_math.h"
#include "BLI_blenlib.h"
#include "BLI_rand.h"
#include "BLI_threads.h"

#include "PIL_time.h"

#include "MEM_guardedalloc.h"
#include "IMB_imbuf.h"

#include "RE_pipeline.h"
#include "RE_shader_ext.h"		/* <- TexResult */
#include "RE_render_ext.h"		/* <- ibuf_sample() */

#include "CMP_node.h"
#include "intern/CMP_util.h"	/* stupid include path... */

#include "SHD_node.h"
#include "TEX_node.h"
#include "intern/TEX_util.h"

#include "GPU_extensions.h"
#include "GPU_material.h"

static ListBase empty_list = {NULL, NULL};
ListBase node_all_composit = {NULL, NULL};
ListBase node_all_shaders = {NULL, NULL};
ListBase node_all_textures = {NULL, NULL};

/* ************** Type stuff **********  */

static bNodeType *node_get_type(bNodeTree *ntree, int type, bNodeTree *ngroup, ID *id)
{
	if(type==NODE_GROUP) {
		if(ngroup && GS(ngroup->id.name)==ID_NT) {
			return ngroup->owntype;
		}
		return NULL;
	}
	else {
		bNodeType *ntype = ntree->alltypes.first;
		for(; ntype; ntype= ntype->next)
			if(ntype->type==type && id==ntype->id )
				return ntype;
		
		return NULL;
	}
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
				stype= node_get_type(ntree, node->type, NULL, NULL);
			} else { /* not an empty script node */
				stype= node_get_type(ntree, node->type, NULL, node->id);
				if(!stype) {
					stype= node_get_type(ntree, node->type, NULL, NULL);
					/* needed info if the pynode script fails now: */
					if (node->id) node->storage= ntree;
				} else {
					node->custom1= 0;
					node->custom1= BSET(node->custom1,NODE_DYNAMIC_ADDEXIST);
				}
			}
			node->typeinfo= stype;
			node->typeinfo->initfunc(node);
		} else {
			node->typeinfo= node_get_type(ntree, node->type, (bNodeTree *)node->id, NULL);
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
	
	/* if((ntree->init & NTREE_TYPE_INIT)==0) */
	ntreeInitTypes(ntree);

	/* check inputs and outputs, and remove or insert them */
	for(node= ntree->nodes.first; node; node= node->next)
		nodeVerifyType(ntree, node);
	
}

/* ************** Group stuff ********** */

bNodeType node_group_typeinfo= {
	/* next,prev   */	NULL, NULL,
	/* type code   */	NODE_GROUP,
	/* name        */	"Group",
	/* width+range */	120, 60, 200,
	/* class+opts  */	NODE_CLASS_GROUP, NODE_OPTIONS,
	/* input sock  */	NULL,
	/* output sock */	NULL,
	/* storage     */	"",
	/* execfunc    */	NULL,
	/* butfunc     */	NULL,
	/* initfunc    */	NULL,
	/* freestoragefunc    */	NULL,
	/* copystoragefunc    */	NULL,
	/* id          */	NULL
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
			sock->intern= sock->flag & (SOCK_HIDDEN|SOCK_UNAVAIL);
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
	ngroup->owntype= MEM_callocN(sizeof(bNodeType), "group type");
	*ngroup->owntype= node_group_typeinfo; /* copy data, for init */
	
	/* input type arrays */
	if(totin) {
		bNodeSocketType *stype;
		bNodeSocketType *inputs= MEM_callocN(sizeof(bNodeSocketType)*(totin+1), "bNodeSocketType");
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
		bNodeSocketType *outputs= MEM_callocN(sizeof(bNodeSocketType)*(totout+1), "bNodeSocketType");
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

			/* set socket own_index to zero since it can still have a value
			 * from being in a group before, otherwise it doesn't get a unique
			 * index in group_verify_own_indices */
			for(sock= node->inputs.first; sock; sock= sock->next)
				sock->own_index= 0;
			for(sock= node->outputs.first; sock; sock= sock->next)
				sock->own_index= 0;
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
	gnode= nodeAddNodeType(ntree, NODE_GROUP, ngroup, NULL);
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
	
	/* initialize variables of unused input sockets */
	for(node= ngroup->nodes.first; node; node= node->next) {
		for(sock= node->inputs.first; sock; sock= sock->next) {
			if(sock->intern==0) {
				bNodeSocket *nsock= groupnode_find_tosock(gnode, sock->own_index);
				if(nsock) {
					QUATCOPY(nsock->ns.vec, sock->ns.vec);
				}
			}
		}
	}

	/* update node levels */
	ntreeSolveOrder(ntree);

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
	else if(ngroup->type==NTREE_TEXTURE) {
		Tex *tx;
		for(tx= G.main->tex.first; tx; tx= tx->id.next) {
			if(tx->nodetree) {
				bNode *node;
				
				/* find if group is in tree */
				for(node= tx->nodetree->nodes.first; node; node= node->next)
					if(node->id == (ID *)ngroup)
						break;
				
				if(node) {
					/* set all type pointers OK */
					ntreeInitTypes(tx->nodetree);
					
					for(node= tx->nodetree->nodes.first; node; node= node->next)
						if(node->id == (ID *)ngroup)
							nodeVerifyType(tx->nodetree, node);
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
	else if(ngroup->type==NTREE_TEXTURE) {
		Tex *tx;
		for(tx= G.main->tex.first; tx; tx= tx->id.next) {
			if(tx->nodetree) {
				for(node= tx->nodetree->nodes.first; node; node= node->next) {
					if(node->id==(ID *)ngroup) {
						for(sock= node->inputs.first; sock; sock= sock->next)
							if(sock->link)
								if(sock->tosock) 
									sock->tosock->flag |= SOCK_IN_USE;
						for(sock= node->outputs.first; sock; sock= sock->next)
							if(nodeCountSocketLinks(tx->nodetree, sock))
								if(sock->tosock) 
									sock->tosock->flag |= SOCK_IN_USE;
					}
				}
			}
		}
	}
	
}
/* finds a node based on its name */
bNode *nodeFindNodebyName(bNodeTree *ntree, const char *name)
{
	bNode *node=NULL;
	
	for(node= ntree->nodes.first; node; node= node->next) {
		if (strcmp(name, node->name) == 0)
			break;
	}
	return node;
}

/* finds a node based on given socket */
int nodeFindNode(bNodeTree *ntree, bNodeSocket *sock, bNode **nodep, int *sockindex)
{
	bNode *node;
	bNodeSocket *tsock;
	int index= 0;
	
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
	int index;
	
	ngroup= (bNodeTree *)gnode->id;
	if(ngroup==NULL) return 0;
	
	/* clear new pointers, set in copytree */
	for(node= ntree->nodes.first; node; node= node->next)
		node->new_node= NULL;

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
			nodeFindNode(ngroup, link->tosock->tosock, &nextn, &index);
			if(nextn==NULL) printf("wrong stuff!\n");
			else if(nextn->new_node==NULL) printf("wrong stuff too!\n");
			else {
				link->tonode= nextn->new_node;
				link->tosock= BLI_findlink(&link->tonode->inputs, index);
			}
		}
		else if(link->fromnode==gnode) {
			/* link->fromsock->tosock is on the node we look for */
			nodeFindNode(ngroup, link->fromsock->tosock, &nextn, &index);
			if(nextn==NULL) printf("1 wrong stuff!\n");
			else if(nextn->new_node==NULL) printf("1 wrong stuff too!\n");
			else {
				link->fromnode= nextn->new_node;
				link->fromsock= BLI_findlink(&link->fromnode->outputs, index);
			}
		}
	}
	
	/* remove the gnode & work tree */
	free_libblock(&G.main->nodetree, wgroup);
	
	nodeFreeNode(ntree, gnode);
	
	/* solve order goes fine, but the level tags not... doing it twice works for now. solve this once */
	ntreeSolveOrder(ntree);
	ntreeSolveOrder(ntree);

	return 1;
}

void nodeCopyGroup(bNode *gnode)
{
	bNodeSocket *sock;

	gnode->id->us--;
	gnode->id= (ID *)ntreeCopyTree((bNodeTree *)gnode->id, 0);

	/* new_sock was set in nodeCopyNode */
	for(sock=gnode->inputs.first; sock; sock=sock->next)
		if(sock->tosock)
			sock->tosock= sock->tosock->new_sock;

	for(sock=gnode->outputs.first; sock; sock=sock->next)
		if(sock->tosock)
			sock->tosock= sock->tosock->new_sock;
}

/* ************** Add stuff ********** */
void nodeAddSockets(bNode *node, bNodeType *ntype)
{
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
/* Find the first available, non-duplicate name for a given node */
void nodeUniqueName(bNodeTree *ntree, bNode *node)
{
	BLI_uniquename(&ntree->nodes, node, "Node", '.', offsetof(bNode, name), sizeof(node->name));
}

bNode *nodeAddNodeType(bNodeTree *ntree, int type, bNodeTree *ngroup, ID *id)
{
	bNode *node= NULL;
	bNodeType *ntype= NULL;

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
		ntype= node_get_type(ntree, type, ngroup, id);

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
bNode *nodeCopyNode(struct bNodeTree *ntree, struct bNode *node, int internal)
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
		if(internal)
			sock->own_index= 0;
	}
	
	BLI_duplicatelist(&nnode->outputs, &node->outputs);
	oldsock= node->outputs.first;
	for(sock= nnode->outputs.first; sock; sock= sock->next, oldsock= oldsock->next) {
		if(internal)
			sock->own_index= 0;
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


bNodeTree *ntreeAddTree(int type)
{
	bNodeTree *ntree= MEM_callocN(sizeof(bNodeTree), "new node tree");
	ntree->type= type;
	ntree->alltypes.first = NULL;
	ntree->alltypes.last = NULL;

	/* this helps RNA identify ID pointers as nodetree */
    if(ntree->type==NTREE_SHADER)
		BLI_strncpy(ntree->id.name, "NTShader Nodetree", sizeof(ntree->id.name));
    else if(ntree->type==NTREE_COMPOSIT)
		BLI_strncpy(ntree->id.name, "NTCompositing Nodetree", sizeof(ntree->id.name));
    else if(ntree->type==NTREE_TEXTURE)
		BLI_strncpy(ntree->id.name, "NTTexture Nodetree", sizeof(ntree->id.name));
	
	ntreeInitTypes(ntree);
	return ntree;
}

/* Warning: this function gets called during some rather unexpected times
 *	- internal_select is only 1 when used for duplicating selected nodes (i.e. Shift-D duplicate operator)
 *	- this gets called when executing compositing updates (for threaded previews)
 *	- when the nodetree datablock needs to be copied (i.e. when users get copied)
 */
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
		if(newtree) {
			newtree= copy_libblock(ntree);
		} else {
			newtree= MEM_dupallocN(ntree);
			copy_libblock_data(&newtree->id, &ntree->id); /* copy animdata and ID props */
		}
		newtree->nodes.first= newtree->nodes.last= NULL;
		newtree->links.first= newtree->links.last= NULL;
	}
	else
		newtree= ntree;
	
	last= ntree->nodes.last;
	for(node= ntree->nodes.first; node; node= node->next) {
		
		node->new_node= NULL;
		if(internal_select==0 || (node->flag & NODE_SELECT)) {
			nnode= nodeCopyNode(newtree, node, internal_select);	/* sets node->new */
			if(internal_select) {
				node->flag &= ~(NODE_SELECT|NODE_ACTIVE);
				nnode->flag |= NODE_SELECT;
			}
		}
		if(node==last) break;
	}
	
	/* check for copying links */
	for(link= ntree->links.first; link; link= link->next) {
		if(link->fromnode==NULL || link->tonode==NULL);
		else if(link->fromnode->new_node && link->tonode->new_node) {
			nlink= nodeAddLink(newtree, link->fromnode->new_node, NULL, link->tonode->new_node, NULL);
			/* sockets were copied in order */
			for(a=0, sock= link->fromnode->outputs.first; sock; sock= sock->next, a++) {
				if(sock==link->fromsock)
					break;
			}
			nlink->fromsock= BLI_findlink(&link->fromnode->new_node->outputs, a);
			
			for(a=0, sock= link->tonode->inputs.first; sock; sock= sock->next, a++) {
				if(sock==link->tosock)
					break;
			}
			nlink->tosock= BLI_findlink(&link->tonode->new_node->inputs, a);
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
	/* weird this is required... there seem to be link pointers wrong still? */
	/* anyhoo, doing this solves crashes on copying entire tree (copy scene) and delete nodes */
	ntreeSolveOrder(newtree);

	return newtree;
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
		node->preview->rect= MEM_callocN(4*xsize + xsize*ysize*sizeof(float)*4, "node preview rect");
		node->preview->xsize= xsize;
		node->preview->ysize= ysize;
	}
	else
		memset(node->preview->rect, 0, 4*xsize + xsize*ysize*sizeof(float)*4);
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
void nodeAddToPreview(bNode *node, float *col, int x, int y)
{
	bNodePreview *preview= node->preview;
	if(preview) {
		if(x>=0 && y>=0) {
			if(x<preview->xsize && y<preview->ysize) {
				unsigned char *tar= preview->rect+ 4*((preview->xsize*y) + x);
				//if(tar[0]==0.0f) {
				tar[0]= FTOCHAR(col[0]);
				tar[1]= FTOCHAR(col[1]);
				tar[2]= FTOCHAR(col[2]);
				tar[3]= FTOCHAR(col[3]);
				//}
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
			
			/* we need a check for which output node should be tagged like this, below an exception */
			if(node->type==CMP_NODE_OUTPUT_FILE)
			   continue;
			   
			/* there is more types having output class, each one is checked */
			for(tnode= ntree->nodes.first; tnode; tnode= tnode->next) {
				if(tnode->typeinfo->nclass==NODE_CLASS_OUTPUT) {
					if(tnode->type==node->type) {
						if(tnode->flag & NODE_DO_OUTPUT) {
							output++;
							if(output>1)
								tnode->flag &= ~NODE_DO_OUTPUT;
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

void NodeTagIDChanged(bNodeTree *ntree, ID *id)
{
	if(id==NULL)
		return;
	
	if(ntree->type==NTREE_COMPOSIT) {
		bNode *node;
		
		for(node= ntree->nodes.first; node; node= node->next)
			if(node->id==id)
				NodeTagChanged(ntree, node);
	}
}



/* ******************* executing ************* */

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
			
			/* for groups, only execute outputs for edited group */
			if(node->typeinfo->nclass==NODE_CLASS_OUTPUT) {
				if(gnode->flag & NODE_GROUP_EDIT)
					if(node->flag & NODE_DO_OUTPUT)
						node->typeinfo->execfunc(data, node, nsin, nsout);
			}
			else
				node->typeinfo->execfunc(data, node, nsin, nsout);
		}
	}
	
	/* free internal group output nodes */
	if(ntree->type==NTREE_COMPOSIT) {
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

/* copy socket compbufs to stack, initialize usage of curve nodes */
static void composit_begin_exec(bNodeTree *ntree, int is_group)
{
	bNode *node;
	bNodeSocket *sock;
	
	for(node= ntree->nodes.first; node; node= node->next) {
		
		/* initialize needed for groups */
		node->exec= 0;	
		
		if(is_group==0) {
			for(sock= node->outputs.first; sock; sock= sock->next) {
				bNodeStack *ns= ntree->stack + sock->stack_index;
				
				if(sock->ns.data) {
					ns->data= sock->ns.data;
					sock->ns.data= NULL;
				}
			}
		}
		/* cannot initialize them while using in threads */
		if(ELEM4(node->type, CMP_NODE_TIME, CMP_NODE_CURVE_VEC, CMP_NODE_CURVE_RGB, CMP_NODE_HUECORRECT)) {
			curvemapping_initialize(node->storage);
			if(node->type==CMP_NODE_CURVE_RGB)
				curvemapping_premultiply(node->storage, 0);
		}
		if(node->type==NODE_GROUP)
			composit_begin_exec((bNodeTree *)node->id, 1);

	}
}

/* copy stack compbufs to sockets */
static void composit_end_exec(bNodeTree *ntree, int is_group)
{
	extern void print_compbuf(char *str, struct CompBuf *cbuf);
	bNode *node;
	bNodeStack *ns;
	int a;

	for(node= ntree->nodes.first; node; node= node->next) {
		if(is_group==0) {
			bNodeSocket *sock;
		
			for(sock= node->outputs.first; sock; sock= sock->next) {
				ns= ntree->stack + sock->stack_index;
				if(ns->data) {
					sock->ns.data= ns->data;
					ns->data= NULL;
				}
			}
		}
		if(node->type==CMP_NODE_CURVE_RGB)
			curvemapping_premultiply(node->storage, 1);
		
		if(node->type==NODE_GROUP)
			composit_end_exec((bNodeTree *)node->id, 1);

		node->need_exec= 0;
	}
	
	if(is_group==0) {
		/* internally, group buffers are not stored */
		for(ns= ntree->stack, a=0; a<ntree->stacksize; a++, ns++) {
			if(ns->data) {
				printf("freed leftover buffer from stack\n");
				free_compbuf(ns->data);
				ns->data= NULL;
			}
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
						ns->sockettype= sock->link->fromsock->type;
					}
					else
						sock->ns.sockettype= sock->type;
				}
			}
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
	
	if(ntree->threadstack)
		for(th=0; th<BLENDER_MAX_THREADS; th++)
			for(nts=ntree->threadstack[th].first; nts; nts=nts->next)
				for(ns= nts->stack, a=0; a<ntree->stacksize; a++, ns++)
					if(ns->data)
						MEM_freeN(ns->data);
						
}

void ntreeBeginExecTree(bNodeTree *ntree)
{
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
			
			/* composite has own need_exec tag handling */
			if(ntree->type!=NTREE_COMPOSIT)
				node->need_exec= 1;

			for(sock= node->inputs.first; sock; sock= sock->next) {
				if(sock->link) {
					ns= ntree->stack + sock->link->fromsock->stack_index;
					ns->hasoutput= 1;
					ns->sockettype= sock->link->fromsock->type;
				}
				else
					sock->ns.sockettype= sock->type;
				
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
			
			if(node->type==NODE_GROUP && node->id)
				group_tag_used_outputs(node, ntree->stack);
			
		}
		
		if(ntree->type==NTREE_COMPOSIT)
			composit_begin_exec(ntree, 0);
	}
	
	ntree->init |= NTREE_EXEC_INIT;
}

void ntreeEndExecTree(bNodeTree *ntree)
{
	
	if(ntree->init & NTREE_EXEC_INIT) {
		bNodeThreadStack *nts;
		int a;
		
		/* another callback candidate! */
		if(ntree->type==NTREE_COMPOSIT)
			composit_end_exec(ntree, 0);
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
				node_get_stack(node, stack, nsin, nsout);
				node->typeinfo->execfunc(callerdata, node, nsin, nsout);
			}
			else if(node->type==NODE_GROUP && node->id) {
				node_get_stack(node, stack, nsin, nsout);
				node_group_execute(stack, callerdata, node, nsin, nsout); 
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
			if(sock->link)
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
	
	node_get_stack(node, thd->stack, nsin, nsout);
	
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
		node_group_execute(thd->stack, thd->rd, node, nsin, nsout); 
	}
	
	node->exec |= NODE_READY;
	return 0;
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
		
		node_get_stack(node, thd->stack, nsin, nsout);
		
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
					node_get_stack(node, thd->stack, nsin, nsout);
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
				if(sock->link)
					sock->link->fromnode->exec &= ~NODE_FREEBUFS;
		}
	}
	/* now we can free buffers */
	for(node= ntree->nodes.first; node; node= node->next) {
		if(node->exec & NODE_FREEBUFS) {
			for(sock= node->outputs.first; sock; sock= sock->next) {
				bNodeStack *ns= ntree->stack + sock->stack_index;
				if(ns->data) {
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
				if(sock->link)
					if((sock->link->fromnode->exec & NODE_READY)==0)
						break;
			}
			if(sock==NULL)
				return node;
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
	int totnode, rendering= 1;
	
	if(ntree==NULL) return;
	
	if(do_preview)
		ntreeInitPreview(ntree, 0, 0);
	
	ntreeBeginExecTree(ntree);
	
	/* prevent unlucky accidents */
	if(G.background)
		rd->scemode &= ~R_COMP_CROP;
	
	/* setup callerdata for thread callback */
	thdata.rd= rd;
	thdata.stack= ntree->stack;
	
	/* fixed seed, for example noise texture */
	BLI_srandom(rd->cfra);

	/* sets need_exec tags in nodes */
	totnode= setExecutableNodes(ntree, &thdata);

	BLI_init_threads(&threads, exec_composite_node, rd->threads);
	
	while(rendering) {
		
		if(BLI_available_threads(&threads)) {
			node= getExecutableNode(ntree);
			if(node) {
				
				if(ntree->timecursor)
					ntree->timecursor(ntree->tch, totnode);
				if(ntree->stats_draw) {
					char str[64];
					sprintf(str, "Compositing %d %s", totnode, node->name);
					ntree->stats_draw(ntree->sdh, str);
				}
				totnode--;
				
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

/* returns localized composite tree for execution in threads */
/* local tree then owns all compbufs */
bNodeTree *ntreeLocalize(bNodeTree *ntree)
{
	bNodeTree *ltree= ntreeCopyTree(ntree, 0);
	bNode *node;
	bNodeSocket *sock;
	
	/* move over the compbufs */
	/* right after ntreeCopyTree() oldsock pointers are valid */
	for(node= ntree->nodes.first; node; node= node->next) {
		
		/* store new_node pointer to original */
		node->new_node->new_node= node;
		/* ensure new user input gets handled ok */
		node->need_exec= 0;
		
		if(ELEM(node->type, CMP_NODE_VIEWER, CMP_NODE_SPLITVIEWER)) {
			if(node->id) {
				if(node->flag & NODE_DO_OUTPUT)
					node->new_node->id= (ID *)BKE_image_copy((Image *)node->id);
				else
					node->new_node->id= NULL;
			}
		}
		
		for(sock= node->outputs.first; sock; sock= sock->next) {
			
			sock->new_sock->ns.data= sock->ns.data;
			sock->ns.data= NULL;
			sock->new_sock->new_sock= sock;
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
void ntreeLocalSync(bNodeTree *localtree, bNodeTree *ntree)
{
	bNode *lnode;
	
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
		gs[i].hasoutput= ns[i]->hasinput && ns[i]->data;
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
		ns[i]->hasinput= gs[i].hasinput && gs[i].link;
		ns[i]->hasoutput= gs[i].hasoutput;
		ns[i]->sockettype= gs[i].sockettype;
	}
}

static void gpu_node_group_execute(bNodeStack *stack, GPUMaterial *mat, bNode *gnode, bNodeStack **in, bNodeStack **out)
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
			group_node_get_stack(node, stack, nsin, nsout, in, out);

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
			node_get_stack(node, stack, nsin, nsout);
			gpu_from_node_stack(&node->inputs, nsin, gpuin);
			gpu_from_node_stack(&node->outputs, nsout, gpuout);
			if(node->typeinfo->gpufunc(mat, node, gpuin, gpuout))
				data_from_gpu_stack(&node->outputs, nsout, gpuout);
		}
        else if(node->type==NODE_GROUP && node->id) {
			node_get_stack(node, stack, nsin, nsout);
			gpu_node_group_execute(stack, mat, node, nsin, nsout);
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
	sock= BLI_findlink(&node->outputs, RRES_OUT_RADIO);
	if(!(passflag & SCE_PASS_RADIO)) sock->flag |= SOCK_UNAVAIL;
	sock= BLI_findlink(&node->outputs, RRES_OUT_INDEXOB);
	if(!(passflag & SCE_PASS_INDEXOB)) sock->flag |= SOCK_UNAVAIL;
	sock= BLI_findlink(&node->outputs, RRES_OUT_MIST);
	if(!(passflag & SCE_PASS_MIST)) sock->flag |= SOCK_UNAVAIL;
	
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
	lb = RNA_struct_defined_properties(ptr.type);
	
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
		else if(node->type==CMP_NODE_R_LAYERS) {
			NodeTagChanged(ntree, node);
			tagged= 1;
		}
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
	nodeRegisterType(ntypelist, &node_group_typeinfo);
	nodeRegisterType(ntypelist, &cmp_node_rlayers);
	nodeRegisterType(ntypelist, &cmp_node_image);
	nodeRegisterType(ntypelist, &cmp_node_texture);
	nodeRegisterType(ntypelist, &cmp_node_value);
	nodeRegisterType(ntypelist, &cmp_node_rgb);
	nodeRegisterType(ntypelist, &cmp_node_curve_time);
	
	nodeRegisterType(ntypelist, &cmp_node_composite);
	nodeRegisterType(ntypelist, &cmp_node_viewer);
	nodeRegisterType(ntypelist, &cmp_node_splitviewer);
	nodeRegisterType(ntypelist, &cmp_node_output_file);
	nodeRegisterType(ntypelist, &cmp_node_view_levels);
	
	nodeRegisterType(ntypelist, &cmp_node_curve_rgb);
	nodeRegisterType(ntypelist, &cmp_node_mix_rgb);
	nodeRegisterType(ntypelist, &cmp_node_hue_sat);
	nodeRegisterType(ntypelist, &cmp_node_brightcontrast);
	nodeRegisterType(ntypelist, &cmp_node_gamma);
	nodeRegisterType(ntypelist, &cmp_node_invert);
	nodeRegisterType(ntypelist, &cmp_node_alphaover);
	nodeRegisterType(ntypelist, &cmp_node_zcombine);
	nodeRegisterType(ntypelist, &cmp_node_colorbalance);
	nodeRegisterType(ntypelist, &cmp_node_huecorrect);
	
	nodeRegisterType(ntypelist, &cmp_node_normal);
	nodeRegisterType(ntypelist, &cmp_node_curve_vec);
	nodeRegisterType(ntypelist, &cmp_node_map_value);
	nodeRegisterType(ntypelist, &cmp_node_normalize);
	
	nodeRegisterType(ntypelist, &cmp_node_filter);
	nodeRegisterType(ntypelist, &cmp_node_blur);
	nodeRegisterType(ntypelist, &cmp_node_dblur);
	nodeRegisterType(ntypelist, &cmp_node_bilateralblur);
	nodeRegisterType(ntypelist, &cmp_node_vecblur);
	nodeRegisterType(ntypelist, &cmp_node_dilateerode);
	nodeRegisterType(ntypelist, &cmp_node_defocus);
	
	nodeRegisterType(ntypelist, &cmp_node_valtorgb);
	nodeRegisterType(ntypelist, &cmp_node_rgbtobw);
	nodeRegisterType(ntypelist, &cmp_node_setalpha);
	nodeRegisterType(ntypelist, &cmp_node_idmask);
	nodeRegisterType(ntypelist, &cmp_node_math);
	nodeRegisterType(ntypelist, &cmp_node_seprgba);
	nodeRegisterType(ntypelist, &cmp_node_combrgba);
	nodeRegisterType(ntypelist, &cmp_node_sephsva);
	nodeRegisterType(ntypelist, &cmp_node_combhsva);
	nodeRegisterType(ntypelist, &cmp_node_sepyuva);
	nodeRegisterType(ntypelist, &cmp_node_combyuva);
	nodeRegisterType(ntypelist, &cmp_node_sepycca);
	nodeRegisterType(ntypelist, &cmp_node_combycca);
	nodeRegisterType(ntypelist, &cmp_node_premulkey);
	
	nodeRegisterType(ntypelist, &cmp_node_diff_matte);
	nodeRegisterType(ntypelist, &cmp_node_distance_matte);
	nodeRegisterType(ntypelist, &cmp_node_chroma_matte);
	nodeRegisterType(ntypelist, &cmp_node_color_matte);
	nodeRegisterType(ntypelist, &cmp_node_channel_matte);
	nodeRegisterType(ntypelist, &cmp_node_color_spill);
	nodeRegisterType(ntypelist, &cmp_node_luma_matte);
	
	nodeRegisterType(ntypelist, &cmp_node_translate);
	nodeRegisterType(ntypelist, &cmp_node_rotate);
	nodeRegisterType(ntypelist, &cmp_node_scale);
	nodeRegisterType(ntypelist, &cmp_node_flip);
	nodeRegisterType(ntypelist, &cmp_node_crop);
	nodeRegisterType(ntypelist, &cmp_node_displace);
	nodeRegisterType(ntypelist, &cmp_node_mapuv);
	nodeRegisterType(ntypelist, &cmp_node_glare);
	nodeRegisterType(ntypelist, &cmp_node_tonemap);
	nodeRegisterType(ntypelist, &cmp_node_lensdist);
}

static void registerShaderNodes(ListBase *ntypelist) 
{
	nodeRegisterType(ntypelist, &node_group_typeinfo);
	nodeRegisterType(ntypelist, &sh_node_output);
	nodeRegisterType(ntypelist, &sh_node_mix_rgb);
	nodeRegisterType(ntypelist, &sh_node_valtorgb);
	nodeRegisterType(ntypelist, &sh_node_rgbtobw);
	nodeRegisterType(ntypelist, &sh_node_normal);
	nodeRegisterType(ntypelist, &sh_node_geom);
	nodeRegisterType(ntypelist, &sh_node_mapping);
	nodeRegisterType(ntypelist, &sh_node_curve_vec);
	nodeRegisterType(ntypelist, &sh_node_curve_rgb);
	nodeRegisterType(ntypelist, &sh_node_math);
	nodeRegisterType(ntypelist, &sh_node_vect_math);
	nodeRegisterType(ntypelist, &sh_node_squeeze);
	nodeRegisterType(ntypelist, &sh_node_camera);
	nodeRegisterType(ntypelist, &sh_node_material);
	nodeRegisterType(ntypelist, &sh_node_material_ext);
	nodeRegisterType(ntypelist, &sh_node_value);
	nodeRegisterType(ntypelist, &sh_node_rgb);
	nodeRegisterType(ntypelist, &sh_node_texture);
	nodeRegisterType(ntypelist, &node_dynamic_typeinfo);
	nodeRegisterType(ntypelist, &sh_node_invert);
	nodeRegisterType(ntypelist, &sh_node_seprgb);
	nodeRegisterType(ntypelist, &sh_node_combrgb);
	nodeRegisterType(ntypelist, &sh_node_hue_sat);
}

static void registerTextureNodes(ListBase *ntypelist)
{
	nodeRegisterType(ntypelist, &node_group_typeinfo);
	nodeRegisterType(ntypelist, &tex_node_math);
	nodeRegisterType(ntypelist, &tex_node_mix_rgb);
	nodeRegisterType(ntypelist, &tex_node_valtorgb);
	nodeRegisterType(ntypelist, &tex_node_rgbtobw);
	nodeRegisterType(ntypelist, &tex_node_valtonor);
	nodeRegisterType(ntypelist, &tex_node_curve_rgb);
	nodeRegisterType(ntypelist, &tex_node_curve_time);
	nodeRegisterType(ntypelist, &tex_node_invert);
	nodeRegisterType(ntypelist, &tex_node_hue_sat);
	nodeRegisterType(ntypelist, &tex_node_coord);
	nodeRegisterType(ntypelist, &tex_node_distance);
	nodeRegisterType(ntypelist, &tex_node_compose);
	nodeRegisterType(ntypelist, &tex_node_decompose);
	
	nodeRegisterType(ntypelist, &tex_node_output);
	nodeRegisterType(ntypelist, &tex_node_viewer);
	
	nodeRegisterType(ntypelist, &tex_node_checker);
	nodeRegisterType(ntypelist, &tex_node_texture);
	nodeRegisterType(ntypelist, &tex_node_bricks);
	nodeRegisterType(ntypelist, &tex_node_image);
	
	nodeRegisterType(ntypelist, &tex_node_rotate);
	nodeRegisterType(ntypelist, &tex_node_translate);
	nodeRegisterType(ntypelist, &tex_node_scale);
	nodeRegisterType(ntypelist, &tex_node_at);
	
	nodeRegisterType(ntypelist, &tex_node_proc_voronoi);
	nodeRegisterType(ntypelist, &tex_node_proc_blend);
	nodeRegisterType(ntypelist, &tex_node_proc_magic);
	nodeRegisterType(ntypelist, &tex_node_proc_marble);
	nodeRegisterType(ntypelist, &tex_node_proc_clouds);
	nodeRegisterType(ntypelist, &tex_node_proc_wood);
	nodeRegisterType(ntypelist, &tex_node_proc_musgrave);
	nodeRegisterType(ntypelist, &tex_node_proc_noise);
	nodeRegisterType(ntypelist, &tex_node_proc_stucci);
	nodeRegisterType(ntypelist, &tex_node_proc_distnoise);
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
					MEM_freeN(sock->name);
					sock++;
				}
				MEM_freeN(ntype->inputs);
			}
			if(ntype->outputs) {
				bNodeSocketType *sock= ntype->outputs;
				while(sock->type!=-1) {
					MEM_freeN(sock->name);
					sock++;
				}
				MEM_freeN(ntype->outputs);
			}
			if(ntype->name) {
				MEM_freeN(ntype->name);
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

