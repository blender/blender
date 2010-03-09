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
 * The Original Code is Copyright (C) 2001-2002 by NaN Holding BV.
 * All rights reserved.
 *
 * The Original Code is: none of this file.
 *
 * Contributor(s): Janne Karhu
 *                 Brecht Van Lommel
 *
 * ***** END GPL LICENSE BLOCK *****
 */

#include <stdlib.h>
#include <string.h>
#include <math.h>

#include "MEM_guardedalloc.h"

#include "BLI_math.h"
#include "BLI_kdtree.h"

#ifndef SWAP
#define SWAP(type, a, b) { type sw_ap; sw_ap=(a); (a)=(b); (b)=sw_ap; }
#endif

typedef struct KDTreeNode {
	struct KDTreeNode *left, *right;
	float co[3], nor[3];
	int index;
	short d;
} KDTreeNode;

struct KDTree {
	KDTreeNode *nodes;
	int totnode;
	KDTreeNode *root;
};

KDTree *BLI_kdtree_new(int maxsize)
{
	KDTree *tree;

	tree= MEM_callocN(sizeof(KDTree), "KDTree");
	tree->nodes= MEM_callocN(sizeof(KDTreeNode)*maxsize, "KDTreeNode");
	tree->totnode= 0;

	return tree;
}

void BLI_kdtree_free(KDTree *tree)
{
	if(tree) {
		MEM_freeN(tree->nodes);
		MEM_freeN(tree);
	}
}

void BLI_kdtree_insert(KDTree *tree, int index, float *co, float *nor)
{
	KDTreeNode *node= &tree->nodes[tree->totnode++];

	node->index= index;
	copy_v3_v3(node->co, co);
	if(nor) copy_v3_v3(node->nor, nor);
}

static KDTreeNode *kdtree_balance(KDTreeNode *nodes, int totnode, int axis)
{
	KDTreeNode *node;
	float co;
	int left, right, median, i, j;

	if(totnode <= 0)
		return NULL;
	else if(totnode == 1)
		return nodes;
	
	/* quicksort style sorting around median */
	left= 0;
	right= totnode-1;
	median= totnode/2;

	while(right > left) {
		co= nodes[right].co[axis];
		i= left-1;
		j= right;

		while(1) {
			while(nodes[++i].co[axis] < co);
			while(nodes[--j].co[axis] > co && j>left);

			if(i >= j) break;
			SWAP(KDTreeNode, nodes[i], nodes[j]);
		}

		SWAP(KDTreeNode, nodes[i], nodes[right]);
		if(i >= median)
			right= i-1;
		if(i <= median)
			left= i+1;
	}

	/* set node and sort subnodes */
	node= &nodes[median];
	node->d= axis;
	node->left= kdtree_balance(nodes, median, (axis+1)%3);
	node->right= kdtree_balance(nodes+median+1, (totnode-(median+1)), (axis+1)%3);

	return node;
}

void BLI_kdtree_balance(KDTree *tree)
{
	tree->root= kdtree_balance(tree->nodes, tree->totnode, 0);
}

static float squared_distance(float *v2, float *v1, float *n1, float *n2)
{
	float d[3], dist;

	d[0]= v2[0]-v1[0];
	d[1]= v2[1]-v1[1];
	d[2]= v2[2]-v1[2];

	dist= d[0]*d[0] + d[1]*d[1] + d[2]*d[2];

	//if(n1 && n2 && n1[0]*n2[0] + n1[1]*n2[1] + n1[2]*n2[2] < 0.0f)
	if(n2 && d[0]*n2[0] + d[1]*n2[1] + d[2]*n2[2] < 0.0f)
		dist *= 10.0f;

	return dist;
}

int	BLI_kdtree_find_nearest(KDTree *tree, float *co, float *nor, KDTreeNearest *nearest)
{
	KDTreeNode *root, *node, *min_node;
	KDTreeNode **stack, *defaultstack[100];
	float min_dist, cur_dist;
	int totstack, cur=0;

	if(!tree->root)
		return -1;

	stack= defaultstack;
	totstack= 100;

	root= tree->root;
	min_node= root;
	min_dist= squared_distance(root->co,co,root->nor,nor);

	if(co[root->d] < root->co[root->d]) {
		if(root->right)
			stack[cur++]=root->right;
		if(root->left)
			stack[cur++]=root->left;
	}
	else {
		if(root->left)
			stack[cur++]=root->left;
		if(root->right)
			stack[cur++]=root->right;
	}
	
	while(cur--){
		node=stack[cur];

		cur_dist = node->co[node->d] - co[node->d];

		if(cur_dist<0.0){
			cur_dist= -cur_dist*cur_dist;

			if(-cur_dist<min_dist){
				cur_dist=squared_distance(node->co,co,node->nor,nor);
				if(cur_dist<min_dist){
					min_dist=cur_dist;
					min_node=node;
				}
				if(node->left)
					stack[cur++]=node->left;
			}
			if(node->right)
				stack[cur++]=node->right;
		}
		else{
			cur_dist= cur_dist*cur_dist;

			if(cur_dist<min_dist){
				cur_dist=squared_distance(node->co,co,node->nor,nor);
				if(cur_dist<min_dist){
					min_dist=cur_dist;
					min_node=node;
				}
				if(node->right)
					stack[cur++]=node->right;
			}
			if(node->left)
				stack[cur++]=node->left;
		}
		if(cur+3 > totstack){
			KDTreeNode **temp=MEM_callocN((totstack+100)*sizeof(KDTreeNode*), "psys_treestack");
			memcpy(temp,stack,totstack*sizeof(KDTreeNode*));
			if(stack != defaultstack)
				MEM_freeN(stack);
			stack=temp;
			totstack+=100;
		}
	}

	if(nearest) {
		nearest->index= min_node->index;
		nearest->dist= sqrt(min_dist);
		copy_v3_v3(nearest->co, min_node->co);
	}

	if(stack != defaultstack)
		MEM_freeN(stack);

	return min_node->index;
}

static void add_nearest(KDTreeNearest *ptn, int *found, int n, int index, float dist, float *co)
{
	int i;

	if(*found<n) (*found)++;

	for(i=*found-1; i>0; i--) {
		if(dist >= ptn[i-1].dist)
			break;
		else
			ptn[i]= ptn[i-1];
	}

	ptn[i].index= index;
	ptn[i].dist= dist;
	copy_v3_v3(ptn[i].co, co);
}

/* finds the nearest n entries in tree to specified coordinates */
int	BLI_kdtree_find_n_nearest(KDTree *tree, int n, float *co, float *nor, KDTreeNearest *nearest)
{
	KDTreeNode *root, *node=0;
	KDTreeNode **stack, *defaultstack[100];
	float cur_dist;
	int i, totstack, cur=0, found=0;

	if(!tree->root)
		return 0;

	stack= defaultstack;
	totstack= 100;

	root= tree->root;

	cur_dist= squared_distance(root->co,co,root->nor,nor);
	add_nearest(nearest,&found,n,root->index,cur_dist,root->co);
	
	if(co[root->d] < root->co[root->d]) {
		if(root->right)
			stack[cur++]=root->right;
		if(root->left)
			stack[cur++]=root->left;
	}
	else {
		if(root->left)
			stack[cur++]=root->left;
		if(root->right)
			stack[cur++]=root->right;
	}

	while(cur--){
		node=stack[cur];

		cur_dist = node->co[node->d] - co[node->d];

		if(cur_dist<0.0){
			cur_dist= -cur_dist*cur_dist;

			if(found<n || -cur_dist<nearest[found-1].dist){
				cur_dist=squared_distance(node->co,co,node->nor,nor);

				if(found<n || cur_dist<nearest[found-1].dist)
					add_nearest(nearest,&found,n,node->index,cur_dist,node->co);

				if(node->left)
					stack[cur++]=node->left;
			}
			if(node->right)
				stack[cur++]=node->right;
		}
		else{
			cur_dist= cur_dist*cur_dist;

			if(found<n || cur_dist<nearest[found-1].dist){
				cur_dist=squared_distance(node->co,co,node->nor,nor);
				if(found<n || cur_dist<nearest[found-1].dist)
					add_nearest(nearest,&found,n,node->index,cur_dist,node->co);

				if(node->right)
					stack[cur++]=node->right;
			}
			if(node->left)
				stack[cur++]=node->left;
		}
		if(cur+3 > totstack){
			KDTreeNode **temp=MEM_callocN((totstack+100)*sizeof(KDTreeNode*), "psys_treestack");
			memcpy(temp,stack,totstack*sizeof(KDTreeNode*));
			if(stack != defaultstack)
				MEM_freeN(stack);
			stack=temp;
			totstack+=100;
		}
	}

	for(i=0; i<found; i++)
		nearest[i].dist= sqrt(nearest[i].dist);

	if(stack != defaultstack)
		MEM_freeN(stack);

	return found;
}

static int range_compare(const void * a, const void * b)
{
	const KDTreeNearest *kda = a;
	const KDTreeNearest *kdb = b;

	if(kda->dist < kdb->dist)
		return -1;
	else if(kda->dist > kdb->dist)
		return 1;
	else
		return 0;
}
static void add_in_range(KDTreeNearest **ptn, int found, int *totfoundstack, int index, float dist, float *co)
{
	KDTreeNearest *to;

	if(found+1 > *totfoundstack) {
		KDTreeNearest *temp=MEM_callocN((*totfoundstack+50)*sizeof(KDTreeNode), "psys_treefoundstack");
		memcpy(temp, *ptn, *totfoundstack * sizeof(KDTreeNearest));
		if(*ptn)
			MEM_freeN(*ptn);
		*ptn = temp;
		*totfoundstack+=50;
	}

	to = (*ptn) + found;

	to->index = index;
	to->dist = sqrt(dist);
	copy_v3_v3(to->co, co);
}
int BLI_kdtree_range_search(KDTree *tree, float range, float *co, float *nor, KDTreeNearest **nearest)
{
	KDTreeNode *root, *node=0;
	KDTreeNode **stack, *defaultstack[100];
	KDTreeNearest *foundstack=NULL;
	float range2 = range*range, dist2;
	int totstack, cur=0, found=0, totfoundstack=0;

	if(!tree || !tree->root)
		return 0;

	stack= defaultstack;
	totstack= 100;

	root= tree->root;

	if(co[root->d] + range < root->co[root->d]) {
		if(root->left)
			stack[cur++]=root->left;
	}
	else if(co[root->d] - range > root->co[root->d]) {
		if(root->right)
			stack[cur++]=root->right;
	}
	else {
		dist2 = squared_distance(root->co, co, root->nor, nor);
		if(dist2  <= range2)
			add_in_range(&foundstack, found++, &totfoundstack, root->index, dist2, root->co);

		if(root->left)
			stack[cur++]=root->left;
		if(root->right)
			stack[cur++]=root->right;
	}

	while(cur--) {
		node=stack[cur];

		if(co[node->d] + range < node->co[node->d]) {
			if(node->left)
				stack[cur++]=node->left;
		}
		else if(co[node->d] - range > node->co[node->d]) {
			if(node->right)
				stack[cur++]=node->right;
		}
		else {
			dist2 = squared_distance(node->co, co, node->nor, nor);
			if(dist2 <= range2)
				add_in_range(&foundstack, found++, &totfoundstack, node->index, dist2, node->co);

			if(node->left)
				stack[cur++]=node->left;
			if(node->right)
				stack[cur++]=node->right;
		}

		if(cur+3 > totstack){
			KDTreeNode **temp=MEM_callocN((totstack+100)*sizeof(KDTreeNode*), "psys_treestack");
			memcpy(temp,stack,totstack*sizeof(KDTreeNode*));
			if(stack != defaultstack)
				MEM_freeN(stack);
			stack=temp;
			totstack+=100;
		}
	}

	if(stack != defaultstack)
		MEM_freeN(stack);

	if(found)
		qsort(foundstack, found, sizeof(KDTreeNearest), range_compare);

	*nearest = foundstack;

	return found;
}
