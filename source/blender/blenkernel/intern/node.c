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

#include "BKE_blender.h"
#include "BKE_node.h"
#include "BKE_texture.h"
#include "BKE_utildefines.h"

#include "BLI_arithb.h"
#include "BLI_blenlib.h"

#include "MEM_guardedalloc.h"

/* ************** Type stuff ********** */

static bNodeType *nodeGetType(bNodeTree *ntree, int type)
{
	bNodeType **typedefs= ntree->alltypes;
	
	while( *typedefs && (*typedefs)->type!=type)
		typedefs++;
	
	return *typedefs;
}

void ntreeInitTypes(bNodeTree *ntree)
{
	bNode *node, *next;
	
	if(ntree->type==NTREE_SHADER)
		ntree->alltypes= node_all_shaders;
	else {
		ntree->alltypes= NULL;
		printf("Error: no type definitions for nodes\n");
	}
	
	for(node= ntree->nodes.first; node; node= next) {
		next= node->next;
		node->typeinfo= nodeGetType(ntree, node->type);
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
	
	sock->ns.vec[0]= stype->val1;
	sock->ns.vec[1]= stype->val2;
	sock->ns.vec[2]= stype->val3;
	sock->ns.vec[3]= stype->val4;
				
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

void ntreeVerifyTypes(bNodeTree *ntree)
{
	bNode *node;
	bNodeType *ntype;
	
	if((ntree->init & NTREE_TYPE_INIT)==0)
		ntreeInitTypes(ntree);
	
	/* check inputs and outputs, and remove or insert them */
	for(node= ntree->nodes.first; node; node= node->next) {
		ntype= node->typeinfo;
		if(ntype) {
			/* might add some other verify stuff here */
			
			verify_socket_list(ntree, &node->inputs, ntype->inputs);
			verify_socket_list(ntree, &node->outputs, ntype->outputs);
		}
	}
}



/* ************** Add stuff ********** */

/* not very important, but the stack solver likes to know a maximum */
#define MAX_SOCKET	64

bNode *nodeAddNodeType(bNodeTree *ntree, int type)
{
	bNode *node;
	bNodeType *ntype= nodeGetType(ntree, type);
	bNodeSocketType *stype;
	
	node= MEM_callocN(sizeof(bNode), "new node");
	BLI_addtail(&ntree->nodes, node);
	node->typeinfo= ntype;
	
	BLI_strncpy(node->name, ntype->name, NODE_MAXSTR);
	node->type= ntype->type;
	node->flag= NODE_SELECT|ntype->flag;
	node->width= ntype->width;
	node->miniwidth= 15.0f;		/* small value only, allows print of first chars */
	
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
		else if(node->type==SH_NODE_VALTORGB)
			node->storage= add_colorband(1);
	}
	
	return node;
}

/* keep socket listorder identical, for copying links */
/* ntree is the target tree */
bNode *nodeCopyNode(struct bNodeTree *ntree, struct bNode *node)
{
	bNode *nnode= MEM_callocN(sizeof(bNode), "dupli node");
	
	*nnode= *node;
	BLI_addtail(&ntree->nodes, nnode);
	
	duplicatelist(&nnode->inputs, &node->inputs);
	duplicatelist(&nnode->outputs, &node->outputs);
	if(nnode->id)
		nnode->id->us++;
	
	if(nnode->storage)
		nnode->storage= MEM_dupallocN(nnode->storage);
	
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

/* ************** Free stuff ********** */

/* goes over entire tree */
static void node_unlink_node(bNodeTree *ntree, bNode *node)
{
	bNodeLink *link, *next;
	bNodeSocket *sock;
	ListBase *lb;
	
	for(link= ntree->links.first; link; link= next) {
		next= link->next;
		
		if(link->fromnode==node)
			lb= &node->outputs;
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

void nodeFreeNode(bNodeTree *ntree, bNode *node)
{
	if(ntree) {
		node_unlink_node(ntree, node);
		BLI_remlink(&ntree->nodes, node);
	}
	if(node->id)
		node->id->us--;
	
	BLI_freelistN(&node->inputs);
	BLI_freelistN(&node->outputs);
	
	if(node->preview) {
		if(node->preview->rect)
			MEM_freeN(node->preview->rect);
		MEM_freeN(node->preview);
	}
	if(node->storage)
		MEM_freeN(node->storage);
	
	MEM_freeN(node);
}

void ntreeFreeTree(bNodeTree *ntree)
{
	bNode *node, *next;
	
	for(node= ntree->nodes.first; node; node= next) {
		next= node->next;
		nodeFreeNode(NULL, node);		/* NULL -> no unlinking needed */
	}
	BLI_freelistN(&ntree->links);
	
	MEM_freeN(ntree);
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
	return newtree;
}

/* ************ find stuff *************** */

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
	
	/* find the active outputs, tree type dependant, might become handler */
	if(ntree->type==NTREE_SHADER) {
		/* shader nodes only accepts one output */
		int output= 0;
		
		for(node= ntree->nodes.first; node; node= node->next) {
			if(node->typeinfo->nclass==NODE_CLASS_OUTPUT) {
				if(output==0)
					node->flag |= NODE_DO_OUTPUT;
				else
					node->flag &= ~NODE_DO_OUTPUT;
				output= 1;
			}
		}
	}
	
	/* here we could recursively set which nodes have to be done,
		might be different for editor or for "real" use... */
	
	
	
}

/* *************** preview *********** */

/* if node->preview, then we assume the rect to exist */

static void nodeInitPreview(bNode *node, int xsize, int ysize)
{
	/* signal we don't do anything, preview writing is protected */
	if(xsize==0 || ysize==0)
		return;
	
	/* sanity checks & initialize */
	if(node->preview) {
		if(node->preview->xsize!=xsize && node->preview->ysize!=ysize) {
			MEM_freeN(node->preview->rect);
			node->preview->rect= NULL;
		}
	}
	
	if(node->preview==NULL) {
		node->preview= MEM_callocN(sizeof(bNodePreview), "node preview");
	}
	if(node->preview->rect==NULL) {
		node->preview->rect= MEM_callocN(4*xsize + xsize*ysize*sizeof(float)*4, "node preview rect");
		node->preview->xsize= xsize;
		node->preview->ysize= ysize;
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



/* ******************* executing ************* */


void ntreeBeginExecTree(bNodeTree *ntree, int xsize, int ysize)
{
	bNode *node;
	bNodeSocket *sock;
	int index= 0;
	
	if((ntree->init & NTREE_TYPE_INIT)==0)
		ntreeInitTypes(ntree);
	
	/* create indices for stack, check preview */
	for(node= ntree->nodes.first; node; node= node->next) {
		for(sock= node->outputs.first; sock; sock= sock->next) {
			sock->stack_index= index++;
		}
		
		if(node->typeinfo->flag & NODE_PREVIEW)	/* hrms, check for closed nodes? */
			nodeInitPreview(node, xsize, ysize);
		
	}
	if(index) {
		bNodeStack *ns;
		int a;
		
		ns=ntree->stack= MEM_callocN(index*sizeof(bNodeStack), "node stack");
		for(a=0; a<index; a++, ns++) ns->hasinput= 1;
	}
	
	ntree->init |= NTREE_EXEC_INIT;
}

void ntreeEndExecTree(bNodeTree *ntree)
{
	if(ntree->stack) {
		MEM_freeN(ntree->stack);
		ntree->stack= NULL;
	}

	ntree->init &= ~NTREE_EXEC_INIT;
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
void ntreeExecTree(bNodeTree *ntree)
{
	bNode *node;
	bNodeStack *nsin[MAX_SOCKET];	/* arbitrary... watch this */
	bNodeStack *nsout[MAX_SOCKET];	/* arbitrary... watch this */
	
	/* only when initialized */
	if(ntree->init & NTREE_EXEC_INIT) {
	
		for(node= ntree->nodes.first; node; node= node->next) {
			if(node->typeinfo->execfunc) {
				node_get_stack(node, ntree->stack, nsin, nsout);
				node->typeinfo->execfunc(ntree->data, node, nsin, nsout);
			}
		}
	}
}

/* clear one pixel in all the preview images */
void ntreeClearPixelTree(bNodeTree *ntree, int x, int y)
{
	bNode *node;
	float vec[4]= {0.0f, 0.0f, 0.0f, 0.0f};
	
	/* only when initialized */
	if(ntree->init & NTREE_EXEC_INIT) {
		for(node= ntree->nodes.first; node; node= node->next) {
			if(node->preview)
				nodeAddToPreview(node, vec, x, y);
		}
	}
}
