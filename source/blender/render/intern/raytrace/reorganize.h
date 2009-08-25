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
		if(node_fits_inside(node, parent) && RE_rayobject_isAligned(parent->child) )
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
		
		if( RE_rayobject_isAligned(node->child) )
		{
			for(Node **prev = &node->child; *prev; )
			{
				assert( RE_rayobject_isAligned(*prev) ); 
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
	if( RE_rayobject_isAligned(node->child) )
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
		if(RE_rayobject_isAligned(node->child) && node->child->sibling == 0)
			*new_node = node->child;
	}
	else if(node->child == 0)
		*new_node = 0;	
}

/*
 * Minimizes expected number of BBtest by colapsing nodes
 * it uses surface area heuristic for determining whether a node should be colapsed
 */
template<class Node>
void pushup(Node *parent)
{
	if(is_leaf(parent)) return;
	
	float p_area = bb_area(parent->bb, parent->bb+3);
	Node **prev = &parent->child;
	for(Node *child = parent->child; RE_rayobject_isAligned(child) && child; )
	{
		float c_area = bb_area(child->bb, child->bb+3) ;
		int nchilds = count_childs(child);
		float original_cost = (c_area / p_area)*nchilds + 1;
		float flatten_cost = nchilds;
		if(flatten_cost < original_cost && nchilds >= 2)
		{
			append_sibling(child, child->child);
			child = child->sibling;
			*prev = child;

//			*prev = child->child;
//			append_sibling( *prev, child->sibling );
//			child = *prev;
			tot_pushup++;
		}
		else
		{
			*prev = child;
			prev = &(*prev)->sibling;
			child = *prev;
		}		
	}
	
	for(Node *child = parent->child; RE_rayobject_isAligned(child) && child; child = child->sibling)
		pushup(child);
}

/*
 * try to optimize number of childs to be a multiple of SSize
 */
template<class Node, int SSize>
void pushup_simd(Node *parent)
{
	if(is_leaf(parent)) return;
	
	int n = count_childs(parent);
		
	Node **prev = &parent->child;
	for(Node *child = parent->child; RE_rayobject_isAligned(child) && child; )
	{
		int cn = count_childs(child);
		if(cn-1 <= (SSize - (n%SSize) ) % SSize && RE_rayobject_isAligned(child->child) )
		{
			n += (cn - 1);
			append_sibling(child, child->child);
			child = child->sibling;
			*prev = child;	
		}
		else
		{
			*prev = child;
			prev = &(*prev)->sibling;
			child = *prev;
		}		
	}
		
	for(Node *child = parent->child; RE_rayobject_isAligned(child) && child; child = child->sibling)
		pushup_simd<Node,SSize>(child);
}


/*
 * Pushdown
 *	makes sure no child fits inside any of its sibling
 */
template<class Node>
void pushdown(Node *parent)
{
	Node **s_child = &parent->child;
	Node * child = parent->child;
	
	while(child && RE_rayobject_isAligned(child))
	{
		Node *next = child->sibling;
		Node **next_s_child = &child->sibling;
		
		//assert(bb_fits_inside(parent->bb, parent->bb+3, child->bb, child->bb+3));
		
		for(Node *i = parent->child; RE_rayobject_isAligned(i) && i; i = i->sibling)
		if(child != i && bb_fits_inside(i->bb, i->bb+3, child->bb, child->bb+3) && RE_rayobject_isAligned(i->child))
		{
//			todo optimize (should the one with the smallest area?)
//			float ia = bb_area(i->bb, i->bb+3)
//			if(child->i)
			*s_child = child->sibling;
			child->sibling = i->child;
			i->child = child;
			next_s_child = s_child;
			
			tot_pushdown++;
			break;
		}
		child = next;
		s_child = next_s_child;
	}
	
	for(Node *i = parent->child; RE_rayobject_isAligned(i) && i; i = i->sibling)
		pushdown( i );	
}


/*
 * BVH refit
 * reajust nodes BB (useful if nodes childs where modified)
 */
template<class Node>
float bvh_refit(Node *node)
{
	if(is_leaf(node)) return 0;	
	if(is_leaf(node->child)) return 0;
	
	float total = 0;
	
	for(Node *child = node->child; child; child = child->sibling)
		total += bvh_refit(child);
		
	float old_area = bb_area(node->bb, node->bb+3);
	INIT_MINMAX(node->bb, node->bb+3);
	for(Node *child = node->child; child; child = child->sibling)
	{
		DO_MIN(child->bb, node->bb);
		DO_MAX(child->bb+3, node->bb+3);
	}
	total += old_area - bb_area(node->bb, node->bb+3);
	return total;
}
