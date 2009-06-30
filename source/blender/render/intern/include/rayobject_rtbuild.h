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
#ifndef RE_RAYOBJECT_RTBUILD_H
#define RE_RAYOBJECT_RTBUILD_H

#include "rayobject.h"

/*
 * Ray Tree Builder
 *	this structs helps building any type of tree
 *	it contains several methods to organiza/split nodes
 *	allowing to create a given tree on the fly.
 *
 * Idea is that other trees BVH, BIH can use this code to
 * generate with simple calls, and then convert to the theirs
 * specific structure on the fly.
 */
#define MAX_CHILDS 32
typedef struct RTBuilder
{
	/* list to all primitives in this tree */
	RayObject **begin, **end;
	
	/* axis used (if any) on the split method */
	int split_axis;
	
	/* links to child partitions calculated during splitting */
	RayObject **child[MAX_CHILDS+1];

} RTBuilder;

/* used during creation */
RTBuilder* rtbuild_create(int size);
void rtbuild_free(RTBuilder *b);
void rtbuild_add(RTBuilder *b, RayObject *o);
int rtbuild_size(RTBuilder *b);

/* used during tree reorganization */
RTBuilder* rtbuild_get_child(RTBuilder *b, int child, RTBuilder *tmp);
void rtbuild_mean_split(RTBuilder *b, int nchilds, int axis);
void rtbuild_mean_split_largest_axis(RTBuilder *b, int nchilds);

/*
static BVHNode *bvh_rearrange(BVHTree *tree, RTBuilder *b)
{
	int i;
	int nc = rtbuild_mean_split_largest_axis(b, BVH_NCHILDS);
	RTBuilder tmp;
	
	BVHNode *bvh = tree->next_node++;

	bvh->split_axis = tmp->split_axis;	
	for(i=0; i<nc; i++)
		bvh->child[i] = bvh_rearrange( rtbuild_get_child(b, i, &tmp) );
}
 */

#endif
