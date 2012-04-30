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
 * The Original Code is Copyright (C) 2009 Blender Foundation.
 * All rights reserved.
 *
 * The Original Code is: all of this file.
 *
 * Contributor(s): André Pinto.
 *
 * ***** END GPL LICENSE BLOCK *****
 */

/** \file blender/render/intern/raytrace/reorganize.h
 *  \ingroup render
 */


#include <float.h>
#include <math.h>
#include <stdio.h>

#include <algorithm>
#include <queue>
#include <vector>

#include "BKE_global.h"

#ifdef _WIN32
#  ifdef INFINITY
#    undef INFINITY
#  endif
#  define INFINITY FLT_MAX // in mingw math.h: (1.0F/0.0F). This generates compile error, though.
#endif

extern int tot_pushup;
extern int tot_pushdown;

#if !defined(INFINITY) && defined(HUGE_VAL)
#define INFINITY HUGE_VAL
#endif

template<class Node>
bool node_fits_inside(Node *a, Node *b)
{
	return bb_fits_inside(b->bb, b->bb+3, a->bb, a->bb+3);
}

template<class Node>
void reorganize_find_fittest_parent(Node *tree, Node *node, std::pair<float, Node*> &cost)
{
	std::queue<Node*> q;
	q.push(tree);
	
	while (!q.empty()) {
		Node *parent = q.front();
		q.pop();
		
		if (parent == node) continue;
		if (node_fits_inside(node, parent) && RE_rayobject_isAligned(parent->child) ) {
			float pcost = bb_area(parent->bb, parent->bb+3);
			cost = std::min( cost, std::make_pair(pcost, parent) );
			for (Node *child = parent->child; child; child = child->sibling)
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
	while (!q.empty()) {
		Node * node = q.front();
		q.pop();
		
		if (RE_rayobject_isAligned(node->child)) {
			for (Node **prev = &node->child; *prev; ) {
				assert(RE_rayobject_isAligned(*prev));
				q.push(*prev);

				std::pair<float, Node*> best(FLT_MAX, root);
				reorganize_find_fittest_parent(root, *prev, best);

				if (best.second == node) {
					//Already inside the fitnest BB
					prev = &(*prev)->sibling;
				}
				else {
					Node *tmp = *prev;
					*prev = (*prev)->sibling;
					
					tmp->sibling =  best.second->child;
					best.second->child = tmp;
					
					tot_moves++;
				}
			
			
			}
		}
		if (node != root) {
		}
	}
}

/*
 * Prunes useless nodes from trees:
 *  erases nodes with total amount of primitives = 0
 *  prunes nodes with only one child (except if that child is a primitive)
 */
template<class Node>
void remove_useless(Node *node, Node **new_node)
{
	if ( RE_rayobject_isAligned(node->child) ) {

		for (Node **prev = &node->child; *prev; ) {
			Node *next = (*prev)->sibling;
			remove_useless(*prev, prev);
			if (*prev == NULL)
				*prev = next;
			else {
				(*prev)->sibling = next;
				prev = &((*prev)->sibling);
			}
		}			
	}
	if (node->child) {
		if (RE_rayobject_isAligned(node->child) && node->child->sibling == 0)
			*new_node = node->child;
	}
	else if (node->child == NULL) {
		*new_node = NULL;
	}
}

/*
 * Minimizes expected number of BBtest by colapsing nodes
 * it uses surface area heuristic for determining whether a node should be colapsed
 */
template<class Node>
void pushup(Node *parent)
{
	if (is_leaf(parent)) return;
	
	float p_area = bb_area(parent->bb, parent->bb+3);
	Node **prev = &parent->child;
	for (Node *child = parent->child; RE_rayobject_isAligned(child) && child; ) {
		const float c_area = bb_area(child->bb, child->bb + 3);
		const int nchilds = count_childs(child);
		float original_cost = ((p_area != 0.0f)? (c_area / p_area)*nchilds: 1.0f) + 1;
		float flatten_cost = nchilds;
		if (flatten_cost < original_cost && nchilds >= 2) {
			append_sibling(child, child->child);
			child = child->sibling;
			*prev = child;

//			*prev = child->child;
//			append_sibling( *prev, child->sibling );
//			child = *prev;
			tot_pushup++;
		}
		else {
			*prev = child;
			prev = &(*prev)->sibling;
			child = *prev;
		}		
	}
	
	for (Node *child = parent->child; RE_rayobject_isAligned(child) && child; child = child->sibling)
		pushup(child);
}

/*
 * try to optimize number of childs to be a multiple of SSize
 */
template<class Node, int SSize>
void pushup_simd(Node *parent)
{
	if (is_leaf(parent)) return;
	
	int n = count_childs(parent);
		
	Node **prev = &parent->child;
	for (Node *child = parent->child; RE_rayobject_isAligned(child) && child; ) {
		int cn = count_childs(child);
		if (cn-1 <= (SSize - (n%SSize) ) % SSize && RE_rayobject_isAligned(child->child) ) {
			n += (cn - 1);
			append_sibling(child, child->child);
			child = child->sibling;
			*prev = child;	
		}
		else {
			*prev = child;
			prev = &(*prev)->sibling;
			child = *prev;
		}		
	}
		
	for (Node *child = parent->child; RE_rayobject_isAligned(child) && child; child = child->sibling)
		pushup_simd<Node, SSize>(child);
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
	
	while (child && RE_rayobject_isAligned(child)) {
		Node *next = child->sibling;
		Node **next_s_child = &child->sibling;
		
		//assert(bb_fits_inside(parent->bb, parent->bb+3, child->bb, child->bb+3));
		
		for (Node *i = parent->child; RE_rayobject_isAligned(i) && i; i = i->sibling)
		if (child != i && bb_fits_inside(i->bb, i->bb+3, child->bb, child->bb+3) && RE_rayobject_isAligned(i->child)) {
//			todo optimize (should the one with the smallest area?)
//			float ia = bb_area(i->bb, i->bb+3)
//			if (child->i)
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
	
	for (Node *i = parent->child; RE_rayobject_isAligned(i) && i; i = i->sibling) {
		pushdown(i);
	}
}


/*
 * BVH refit
 * readjust nodes BB (useful if nodes childs where modified)
 */
template<class Node>
float bvh_refit(Node *node)
{
	if (is_leaf(node)) return 0;
	if (is_leaf(node->child)) return 0;
	
	float total = 0;
	
	for (Node *child = node->child; child; child = child->sibling)
		total += bvh_refit(child);
		
	float old_area = bb_area(node->bb, node->bb+3);
	INIT_MINMAX(node->bb, node->bb+3);
	for (Node *child = node->child; child; child = child->sibling) {
		DO_MIN(child->bb, node->bb);
		DO_MAX(child->bb+3, node->bb+3);
	}
	total += old_area - bb_area(node->bb, node->bb+3);
	return total;
}


/*
 * this finds the best way to packing a tree according to a given test cost function
 * with the purpose to reduce the expected cost (eg.: number of BB tests).
 */
#include <vector>
#define MAX_CUT_SIZE		4				/* svbvh assumes max 4 children! */
#define MAX_OPTIMIZE_CHILDS	MAX_CUT_SIZE

struct OVBVHNode
{
	float	bb[6];

	OVBVHNode *child;
	OVBVHNode *sibling;
	
	/*
	 * Returns min cost to represent the subtree starting at the given node,
	 * allowing it to have a given cutsize
	 */
	float cut_cost[MAX_CUT_SIZE];
	float get_cost(int cutsize)
	{
		return cut_cost[cutsize-1];
	}
	
	/*
	 * This saves the cut size of this child, when parent is reaching
	 * its minimum cut with the given cut size
	 */
	int cut_size[MAX_CUT_SIZE];
	int get_cut_size(int parent_cut_size)
	{
		return cut_size[parent_cut_size-1];
	}
	
	/*
	 * Reorganize the node based on calculated cut costs
	 */	 
	int best_cutsize;
	void set_cut(int cutsize, OVBVHNode ***cut)
	{
		if (cutsize == 1) {
			**cut = this;
			 *cut = &(**cut)->sibling;
		}
		else {
			if (cutsize > MAX_CUT_SIZE) {
				for (OVBVHNode *child = this->child; child && RE_rayobject_isAligned(child); child = child->sibling) {
					child->set_cut( 1, cut );
					cutsize--;
				}
				assert(cutsize == 0);
			}
			else
				for (OVBVHNode *child = this->child; child && RE_rayobject_isAligned(child); child = child->sibling)
					child->set_cut( child->get_cut_size( cutsize ), cut );
		}
	}

	void optimize()
	{
		if (RE_rayobject_isAligned(this->child)) {
			//Calc new childs
			{
				OVBVHNode **cut = &(this->child);
				set_cut(best_cutsize, &cut);
				*cut = NULL;
			}

			//Optimize new childs
			for (OVBVHNode *child = this->child; child && RE_rayobject_isAligned(child); child = child->sibling)
				child->optimize();
		}		
	}
};

/*
 * Calculates an optimal SIMD packing
 *
 */
template<class Node, class TestCost>
struct VBVH_optimalPackSIMD
{
	TestCost testcost;
	
	VBVH_optimalPackSIMD(TestCost testcost)
	{
		this->testcost = testcost;
	}
	
	/*
	 * calc best cut on a node
	 */
	struct calc_best
	{
		Node *child[MAX_OPTIMIZE_CHILDS];
		float child_hit_prob[MAX_OPTIMIZE_CHILDS];
		
		calc_best(Node *node)
		{
			int nchilds = 0;
			//Fetch childs and needed data
			{
				float parent_area = bb_area(node->bb, node->bb+3);
				for (Node *child = node->child; child && RE_rayobject_isAligned(child); child = child->sibling) {
					this->child[nchilds] = child;
					this->child_hit_prob[nchilds] = (parent_area != 0.0f)? bb_area(child->bb, child->bb+3) / parent_area: 1.0f;
					nchilds++;
				}

				assert(nchilds >= 2 && nchilds <= MAX_OPTIMIZE_CHILDS);
			}
			
			
			//Build DP table to find minimum cost to represent this node with a given cutsize
			int   bt  [MAX_OPTIMIZE_CHILDS + 1][MAX_CUT_SIZE + 1]; //backtrace table
			float cost[MAX_OPTIMIZE_CHILDS + 1][MAX_CUT_SIZE + 1]; //cost table (can be reduced to float[2][MAX_CUT_COST])
			
			for (int i = 0; i <= nchilds; i++) {
				for (int j = 0; j <= MAX_CUT_SIZE; j++) {
					cost[i][j] = INFINITY;
				}
			}

			cost[0][0] = 0;
			
			for (int i = 1; i<=nchilds; i++) {
				for (int size = i - 1; size/*+(nchilds-i)*/<=MAX_CUT_SIZE; size++) {
					for (int cut = 1; cut+size/*+(nchilds-i)*/<=MAX_CUT_SIZE; cut++) {
						float new_cost = cost[i - 1][size] + child_hit_prob[i - 1] * child[i - 1]->get_cost(cut);
						if (new_cost < cost[i][size+cut]) {
							cost[i][size+cut] = new_cost;
							bt[i][size+cut] = cut;
						}
					}
				}
			}
			
			//Save the ways to archieve the minimum cost with a given cutsize
			for (int i = nchilds; i <= MAX_CUT_SIZE; i++) {
				node->cut_cost[i-1] = cost[nchilds][i];
				if (cost[nchilds][i] < INFINITY) {
					int current_size = i;
					for (int j=nchilds; j>0; j--) {
						child[j-1]->cut_size[i-1] = bt[j][current_size];
						current_size -= bt[j][current_size];
					}
				}
			}			
		}
	};
	
	void calc_costs(Node *node)
	{
		
		if ( RE_rayobject_isAligned(node->child) ) {
			int nchilds = 0;
			for (Node *child = node->child; child && RE_rayobject_isAligned(child); child = child->sibling) {
				calc_costs(child);
				nchilds++;
			}

			for (int i=0; i<MAX_CUT_SIZE; i++)
				node->cut_cost[i] = INFINITY;

			//We are not allowed to look on nodes with with so many childs
			if (nchilds > MAX_CUT_SIZE) {
				float cost = 0;

				float parent_area = bb_area(node->bb, node->bb+3);
				for (Node *child = node->child; child && RE_rayobject_isAligned(child); child = child->sibling) {
					cost += ((parent_area != 0.0f)? ( bb_area(child->bb, child->bb+3) / parent_area ): 1.0f) * child->get_cost(1);
				}
				
				cost += testcost(nchilds);
				node->cut_cost[0] = cost;
				node->best_cutsize = nchilds;
			}
			else {
				calc_best calc(node);
		
				//calc expected cost if we optimaly pack this node
				for (int cutsize=nchilds; cutsize<=MAX_CUT_SIZE; cutsize++) {
					float m = node->get_cost(cutsize) + testcost(cutsize);
					if (m < node->cut_cost[0]) {
						node->cut_cost[0] = m;
						node->best_cutsize = cutsize;
					}
				}
			}
			assert(node->cut_cost[0] != INFINITY);
		}
		else {
			node->cut_cost[0] = 1.0f;
			for (int i = 1; i < MAX_CUT_SIZE; i++)
				node->cut_cost[i] = INFINITY;
		}
	}

	Node *transform(Node *node)
	{
		if (RE_rayobject_isAligned(node->child)) {
			static int num = 0;
			bool first = false;
			if (num == 0) { num++; first = true; }
			
			calc_costs(node);
			if ((G.debug & G_DEBUG) && first) printf("expected cost = %f (%d)\n", node->cut_cost[0], node->best_cutsize );
			node->optimize();
		}
		return node;		
	}	
};
