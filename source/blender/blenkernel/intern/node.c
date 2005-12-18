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

#include "DNA_ID.h"
#include "DNA_node_types.h"

#include "BKE_blender.h"
#include "BKE_node.h"

#include "BLI_blenlib.h"

#include "MEM_guardedalloc.h"

/* ************** Add stuff ********** */

bNode *nodeAddNode(struct bNodeTree *ntree, char *name)
{
	bNode *node= MEM_callocN(sizeof(bNode), "new node");
	
	BLI_addtail(&ntree->nodes, node);
	BLI_strncpy(node->name, name, NODE_MAXSTR);
	return node;
}

bNode *nodeCopyNode(struct bNodeTree *ntree, struct bNode *node)
{
	bNode *nnode= MEM_callocN(sizeof(bNode), "dupli node");
	
	*nnode= *node;
	BLI_addtail(&ntree->nodes, nnode);
	
	duplicatelist(&nnode->inputs, &node->inputs);
	duplicatelist(&nnode->outputs, &node->outputs);
	if(nnode->id)
		nnode->id->us++;

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


/* ************** Free stuff ********** */

/* goes over entire tree */
static void node_unlink_node(bNodeTree *ntree, bNode *node)
{
	
}

void nodeFreeNode(bNodeTree *ntree, bNode *node)
{
	if(ntree)
		node_unlink_node(ntree, node);
	
	if(node->id)
		node->id->us--;
	
	BLI_freelistN(&node->inputs);
	BLI_freelistN(&node->outputs);
	
	MEM_freeN(node);
}

void nodeFreeTree(bNodeTree *ntree)
{
	bNode *node, *next;
	
	for(node= ntree->nodes.first; node; node= next) {
		next= node->next;
		nodeFreeNode(NULL, node);		/* NULL -> no unlinking needed */
	}
	BLI_freelistN(&ntree->links);
	BLI_freelistN(&ntree->inputs);
	BLI_freelistN(&ntree->outputs);
	
	MEM_freeN(ntree);
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

