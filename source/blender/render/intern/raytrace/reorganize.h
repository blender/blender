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
 * The Original Code is Copyright (C) 2009 Blender Foundation.
 * All rights reserved.
 *
 * The Original Code is: all of this file.
 *
 * Contributor(s): André Pinto.
 *
 * ***** END GPL LICENSE BLOCK *****
 */
#include <algorithm>
#include <queue>

template<class Node>
bool node_fits_inside(Node *a, Node *b)
{
	return bb_fits_inside(b->bb, b->bb+3, a->bb, a->bb+3);
}

template<class Node>
void reorganize_find_fittest_parent(Node *tree, Node *node, std::pair<float,Node*> &cost)
{
	std::queue<Node*> q;
	q.push(tree);
	
	while(!q.empty())
	{
		Node *parent = q.front();
		q.pop();
		
		if(parent == node) continue;
		if(node_fits_inside(node, parent) && RayObject_isAligned(parent->child) )
		{
			float pcost = bb_area(parent->bb, parent->bb+3);
			cost = std::min( cost, std::make_pair(pcost,parent) );
			for(Node *child = parent->child; child; child = child->sibling)
				q.push(child);			
		}
	}
}

static int tot_moves = 0;
template<class Node>
void reorganize(Node *root)
{
	std::queue<Node*> q;

	q.push(root);
	while(!q.empty())
	{
		Node * node = q.front();
		q.pop();
		
		if( RayObject_isAligned(node->child) )
		{
			for(Node **prev = &node->child; *prev; )
			{
				assert( RayObject_isAligned(*prev) ); 
				q.push(*prev);

				std::pair<float,Node*> best(FLT_MAX, root);
				reorganize_find_fittest_parent( root, *prev, best );

				if(best.second == node)
				{
					//Already inside the fitnest BB
					prev = &(*prev)->sibling;
				}
				else
				{
					Node *tmp = *prev;
					*prev = (*prev)->sibling;
					
					tmp->sibling =  best.second->child;
					best.second->child = tmp;
					
					tot_moves++;
				}
			
			
			}
		}
		if(node != root)
		{
		}
	}
}

/*
 * Prunes useless nodes from trees:
 *  erases nodes with total ammount of primitives = 0
 *  prunes nodes with only one child (except if that child is a primitive)
 */
template<class Node>
void remove_useless(Node *node, Node **new_node)
{
	if( RayObject_isAligned(node->child) )
	{

		for(Node **prev = &node->child; *prev; )
		{
			Node *next = (*prev)->sibling;
			remove_useless(*prev, prev);
			if(*prev == 0)
				*prev = next;
			else
			{
				(*prev)->sibling = next;
				prev = &((*prev)->sibling);
			}
		}			
	}
	if(node->child)
	{
		if(RayObject_isAligned(node->child) && node->child->child == 0)
			*new_node = node->child;
	}
	else if(node->child == 0)
		*new_node = 0;	
}
