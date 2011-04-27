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
 * The Original Code is: none of this file.
 *
 * ***** END GPL LICENSE BLOCK *****
 */

/** \file blender/editors/space_node/node_layout.c
 *  \ingroup spnode
 */

#include <stdlib.h>

#include "MEM_guardedalloc.h"

#include "BLI_utildefines.h"

#include "DNA_node_types.h"

#include "ED_node.h"

/* Inspired by the algorithm for graphviz dot, as described in the paper:
   "A Technique for Drawing Directed Graphs", 1993
   
   We have it much easier though, as the graph is already acyclic, and we
   are given a root node. */

typedef struct NodeAutoLayout {
	int rank;
	int visited;
} NodeAutoLayout;

static void node_layout_assign_rank(NodeAutoLayout *layout, bNode *from, int rank)
{
	bNodeSocket *sock;

	for(sock=from->inputs.first; sock; sock=sock->next) {
		if(sock->link) {
			bNode *node = sock->link->fromnode;

			if(layout[node->nr].visited)
				continue;

			layout[node->nr].rank= rank;
			layout[node->nr].visited= 1;

			node_layout_assign_rank(layout, node, rank+1);
		}
	}
}

void ED_node_tree_auto_layout(bNodeTree *ntree, bNode *root)
{
	NodeAutoLayout *layout;
	bNode *node;
	float locx, locy, hspacing= 50.0f, vspacing= 30.0f;
	int a, rank, tot= 0, maxrank= 0;

	for(node=ntree->nodes.first; node; node=node->next)
		node->nr= tot++;
	
	layout= MEM_callocN(sizeof(NodeAutoLayout)*tot, "NodeAutoLayout");

	layout[root->nr].rank= 0;
	layout[root->nr].visited= 1;

	node_layout_assign_rank(layout, root, layout[root->nr].rank+1);

	for(a=0; a<tot; a++)
		maxrank = MAX2(maxrank, layout[a].rank);
	
	locx= root->locx;
	locy= root->locy - (root->totr.ymax - root->totr.ymin)*0.5f;
	
	for(rank=1; rank<=maxrank; rank++) {
		float maxwidth= 0.0f;
		float totheight= 0.0f;
		float y;

		locx -= hspacing;

		for(node=ntree->nodes.first; node; node=node->next) {
			if(layout[node->nr].rank == rank) {
				if(totheight > 0.0f)
					totheight += vspacing;
				totheight += (node->totr.ymax - node->totr.ymin);
			}
		}

		y = locy + totheight*0.5f;

		for(node=ntree->nodes.first; node; node=node->next) {
			if(layout[node->nr].rank == rank) {
				maxwidth = MAX2(maxwidth, node->width);
				node->locx = locx - node->width;
				node->locy = y;

				y -= (node->totr.ymax - node->totr.ymin);
				y -= vspacing;
			}
		}

		locx -= maxwidth;
	}

	MEM_freeN(layout);
}

