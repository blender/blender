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

/** \file blender/render/intern/raytrace/rayobject_rtbuild.cpp
 *  \ingroup render
 */


#include <assert.h>
#include <math.h>
#include <stdlib.h>
#include <algorithm>

#include "rayobject_rtbuild.h"

#include "MEM_guardedalloc.h"

#include "BLI_math.h"
#include "BLI_utildefines.h"

static bool selected_node(RTBuilder::Object *node)
{
	return node->selected;
}

static void rtbuild_init(RTBuilder *b)
{
	b->split_axis = -1;
	b->primitives.begin   = 0;
	b->primitives.end     = 0;
	b->primitives.maxsize = 0;
	
	for (int i=0; i<RTBUILD_MAX_CHILDS; i++)
		b->child_offset[i] = 0;

	for (int i=0; i<3; i++)
		b->sorted_begin[i] = b->sorted_end[i] = 0;
		
	INIT_MINMAX(b->bb, b->bb+3);
}

RTBuilder* rtbuild_create(int size)
{
	RTBuilder *builder  = (RTBuilder*) MEM_mallocN( sizeof(RTBuilder), "RTBuilder" );
	RTBuilder::Object *memblock= (RTBuilder::Object*)MEM_mallocN( sizeof(RTBuilder::Object)*size,"RTBuilder.objects");


	rtbuild_init(builder);
	
	builder->primitives.begin = builder->primitives.end = memblock;
	builder->primitives.maxsize = size;
	
	for (int i=0; i<3; i++)
	{
		builder->sorted_begin[i] = (RTBuilder::Object**)MEM_mallocN( sizeof(RTBuilder::Object*)*size,"RTBuilder.sorted_objects");
		builder->sorted_end[i]   = builder->sorted_begin[i];
	} 
	

	return builder;
}

void rtbuild_free(RTBuilder *b)
{
	if (b->primitives.begin) MEM_freeN(b->primitives.begin);

	for (int i=0; i<3; i++)
		if (b->sorted_begin[i])
			MEM_freeN(b->sorted_begin[i]);

	MEM_freeN(b);
}

void rtbuild_add(RTBuilder *b, RayObject *o)
{
	float bb[6];

	assert( b->primitives.begin + b->primitives.maxsize != b->primitives.end );

	INIT_MINMAX(bb, bb+3);
	RE_rayobject_merge_bb(o, bb, bb+3);

	/* skip objects with invalid bounding boxes, nan causes DO_MINMAX
	 * to do nothing, so we get these invalid values. this shouldn't
	 * happen usually, but bugs earlier in the pipeline can cause it. */
	if (bb[0] > bb[3] || bb[1] > bb[4] || bb[2] > bb[5])
		return;
	/* skip objects with inf bounding boxes */
	if (!finite(bb[0]) || !finite(bb[1]) || !finite(bb[2]))
		return;
	if (!finite(bb[3]) || !finite(bb[4]) || !finite(bb[5]))
		return;
	/* skip objects with zero bounding box, they are of no use, and
	 * will give problems in rtbuild_heuristic_object_split later */
	if (bb[0] == bb[3] && bb[1] == bb[4] && bb[2] == bb[5])
		return;
	
	copy_v3_v3(b->primitives.end->bb, bb);
	copy_v3_v3(b->primitives.end->bb+3, bb+3);
	b->primitives.end->obj = o;
	b->primitives.end->cost = RE_rayobject_cost(o);
	
	for (int i=0; i<3; i++)
	{
		*(b->sorted_end[i]) = b->primitives.end;
		b->sorted_end[i]++;
	}
	b->primitives.end++;
}

int rtbuild_size(RTBuilder *b)
{
	return b->sorted_end[0] - b->sorted_begin[0];
}


template<class Obj,int Axis>
static bool obj_bb_compare(const Obj &a, const Obj &b)
{
	if (a->bb[Axis] != b->bb[Axis])
		return a->bb[Axis] < b->bb[Axis];
	return a->obj < b->obj;
}

template<class Item>
static void object_sort(Item *begin, Item *end, int axis)
{
	if (axis == 0) return std::sort(begin, end, obj_bb_compare<Item,0> );
	if (axis == 1) return std::sort(begin, end, obj_bb_compare<Item,1> );
	if (axis == 2) return std::sort(begin, end, obj_bb_compare<Item,2> );
	assert(false);
}

void rtbuild_done(RTBuilder *b, RayObjectControl* ctrl)
{
	for (int i=0; i<3; i++)
	if (b->sorted_begin[i])
	{
		if (RE_rayobjectcontrol_test_break(ctrl)) break;
		object_sort( b->sorted_begin[i], b->sorted_end[i], i );
	}
}

RayObject* rtbuild_get_primitive(RTBuilder *b, int index)
{
	return b->sorted_begin[0][index]->obj;
}

RTBuilder* rtbuild_get_child(RTBuilder *b, int child, RTBuilder *tmp)
{
	rtbuild_init( tmp );

	for (int i=0; i<3; i++)
		if (b->sorted_begin[i])
		{
			tmp->sorted_begin[i] = b->sorted_begin[i] +  b->child_offset[child  ];
			tmp->sorted_end  [i] = b->sorted_begin[i] +  b->child_offset[child+1];
		}
		else
		{
			tmp->sorted_begin[i] = 0;
			tmp->sorted_end  [i] = 0;
		}
	
	return tmp;
}

void rtbuild_calc_bb(RTBuilder *b)
{
	if (b->bb[0] == 1.0e30f)
	{
		for (RTBuilder::Object **index = b->sorted_begin[0]; index != b->sorted_end[0]; index++)
			RE_rayobject_merge_bb( (*index)->obj , b->bb, b->bb+3);
	}
}

void rtbuild_merge_bb(RTBuilder *b, float *min, float *max)
{
	rtbuild_calc_bb(b);
	DO_MIN(b->bb, min);
	DO_MAX(b->bb+3, max);
}

#if 0
int rtbuild_get_largest_axis(RTBuilder *b)
{
	rtbuild_calc_bb(b);
	return bb_largest_axis(b->bb, b->bb+3);
}

//Left balanced tree
int rtbuild_mean_split(RTBuilder *b, int nchilds, int axis)
{
	int i;
	int mleafs_per_child, Mleafs_per_child;
	int tot_leafs  = rtbuild_size(b);
	int missing_leafs;

	long long s;

	assert(nchilds <= RTBUILD_MAX_CHILDS);
	
	//TODO optimize calc of leafs_per_child
	for (s=nchilds; s<tot_leafs; s*=nchilds);
	Mleafs_per_child = s/nchilds;
	mleafs_per_child = Mleafs_per_child/nchilds;
	
	//split min leafs per child	
	b->child_offset[0] = 0;
	for (i=1; i<=nchilds; i++)
		b->child_offset[i] = mleafs_per_child;
	
	//split remaining leafs
	missing_leafs = tot_leafs - mleafs_per_child*nchilds;
	for (i=1; i<=nchilds; i++)
	{
		if (missing_leafs > Mleafs_per_child - mleafs_per_child)
		{
			b->child_offset[i] += Mleafs_per_child - mleafs_per_child;
			missing_leafs -= Mleafs_per_child - mleafs_per_child;
		}
		else
		{
			b->child_offset[i] += missing_leafs;
			missing_leafs = 0;
			break;
		}
	}
	
	//adjust for accumulative offsets
	for (i=1; i<=nchilds; i++)
		b->child_offset[i] += b->child_offset[i-1];

	//Count created childs
	for (i=nchilds; b->child_offset[i] == b->child_offset[i-1]; i--);
	split_leafs(b, b->child_offset, i, axis);
	
	assert( b->child_offset[0] == 0 && b->child_offset[i] == tot_leafs );
	return i;
}
	
	
int rtbuild_mean_split_largest_axis(RTBuilder *b, int nchilds)
{
	int axis = rtbuild_get_largest_axis(b);
	return rtbuild_mean_split(b, nchilds, axis);
}
#endif

/*
 * "separators" is an array of dim NCHILDS-1
 * and indicates where to cut the childs
 */
#if 0
int rtbuild_median_split(RTBuilder *b, float *separators, int nchilds, int axis)
{
	int size = rtbuild_size(b);
		
	assert(nchilds <= RTBUILD_MAX_CHILDS);
	if (size <= nchilds)
	{
		return rtbuild_mean_split(b, nchilds, axis);
	}
	else
	{
		int i;

		b->split_axis = axis;
		
		//Calculate child offsets
		b->child_offset[0] = 0;
		for (i=0; i<nchilds-1; i++)
			b->child_offset[i+1] = split_leafs_by_plane(b, b->child_offset[i], size, separators[i]);
		b->child_offset[nchilds] = size;
		
		for (i=0; i<nchilds; i++)
			if (b->child_offset[i+1] - b->child_offset[i] == size)
				return rtbuild_mean_split(b, nchilds, axis);
		
		return nchilds;
	}	
}

int rtbuild_median_split_largest_axis(RTBuilder *b, int nchilds)
{
	int la, i;
	float separators[RTBUILD_MAX_CHILDS];
	
	rtbuild_calc_bb(b);

	la = bb_largest_axis(b->bb,b->bb+3);
	for (i=1; i<nchilds; i++)
		separators[i-1] = (b->bb[la+3]-b->bb[la])*i / nchilds;
		
	return rtbuild_median_split(b, separators, nchilds, la);
}
#endif

//Heuristics Object Splitter


struct SweepCost
{
	float bb[6];
	float cost;
};

/* Object Surface Area Heuristic splitter */
int rtbuild_heuristic_object_split(RTBuilder *b, int nchilds)
{
	int size = rtbuild_size(b);		
	assert(nchilds == 2);
	assert(size > 1);
	int baxis = -1, boffset = 0;

	if (size > nchilds)
	{
		float bcost = FLT_MAX;
		baxis = -1, boffset = size/2;

		SweepCost *sweep = (SweepCost*)MEM_mallocN( sizeof(SweepCost)*size, "RTBuilder.HeuristicSweep" );
		
		for (int axis=0; axis<3; axis++)
		{
			SweepCost sweep_left;

			RTBuilder::Object **obj = b->sorted_begin[axis];
			
//			float right_cost = 0;
			for (int i=size-1; i>=0; i--)
			{
				if (i == size-1)
				{
					copy_v3_v3(sweep[i].bb, obj[i]->bb);
					copy_v3_v3(sweep[i].bb+3, obj[i]->bb+3);
					sweep[i].cost = obj[i]->cost;
				}
				else
				{
					sweep[i].bb[0] = MIN2(obj[i]->bb[0], sweep[i+1].bb[0]);
					sweep[i].bb[1] = MIN2(obj[i]->bb[1], sweep[i+1].bb[1]);
					sweep[i].bb[2] = MIN2(obj[i]->bb[2], sweep[i+1].bb[2]);					
					sweep[i].bb[3] = MAX2(obj[i]->bb[3], sweep[i+1].bb[3]);
					sweep[i].bb[4] = MAX2(obj[i]->bb[4], sweep[i+1].bb[4]);
					sweep[i].bb[5] = MAX2(obj[i]->bb[5], sweep[i+1].bb[5]);
					sweep[i].cost  = obj[i]->cost + sweep[i+1].cost;
				}
//				right_cost += obj[i]->cost;
			}
			
			sweep_left.bb[0] = obj[0]->bb[0];
			sweep_left.bb[1] = obj[0]->bb[1];
			sweep_left.bb[2] = obj[0]->bb[2];
			sweep_left.bb[3] = obj[0]->bb[3];
			sweep_left.bb[4] = obj[0]->bb[4];
			sweep_left.bb[5] = obj[0]->bb[5];
			sweep_left.cost  = obj[0]->cost;
			
//			right_cost -= obj[0]->cost;	if (right_cost < 0) right_cost = 0;

			for (int i=1; i<size; i++)
			{
				//Worst case heuristic (cost of each child is linear)
				float hcost, left_side, right_side;
				
				// not using log seems to have no impact on raytracing perf, but
				// makes tree construction quicker, left out for now to test (brecht)
				// left_side = bb_area(sweep_left.bb, sweep_left.bb+3)*(sweep_left.cost+logf((float)i));
				// right_side= bb_area(sweep[i].bb, sweep[i].bb+3)*(sweep[i].cost+logf((float)size-i));
				left_side = bb_area(sweep_left.bb, sweep_left.bb+3)*(sweep_left.cost);
				right_side= bb_area(sweep[i].bb, sweep[i].bb+3)*(sweep[i].cost);
				hcost = left_side+right_side;

				assert(left_side >= 0);
				assert(right_side >= 0);
				
				if (left_side > bcost) break;	//No way we can find a better heuristic in this axis

				assert(hcost >= 0);
				if ( hcost < bcost
				|| (hcost == bcost && axis < baxis)) //this makes sure the tree built is the same whatever is the order of the sorting axis
				{
					bcost = hcost;
					baxis = axis;
					boffset = i;
				}
				DO_MIN( obj[i]->bb,   sweep_left.bb   );
				DO_MAX( obj[i]->bb+3, sweep_left.bb+3 );

				sweep_left.cost += obj[i]->cost;
//				right_cost -= obj[i]->cost; if (right_cost < 0) right_cost = 0;
			}
			
			//assert(baxis >= 0 && baxis < 3);
			if (!(baxis >= 0 && baxis < 3))
				baxis = 0;
		}
			
		
		MEM_freeN(sweep);
	}
	else if (size == 2)
	{
		baxis = 0;
		boffset = 1;
	}
	else if (size == 1)
	{
		b->child_offset[0] = 0;
		b->child_offset[1] = 1;
		return 1;
	}
		
	b->child_offset[0] = 0;
	b->child_offset[1] = boffset;
	b->child_offset[2] = size;
	

	/* Adjust sorted arrays for childs */
	for (int i=0; i<boffset; i++) b->sorted_begin[baxis][i]->selected = true;
	for (int i=boffset; i<size; i++) b->sorted_begin[baxis][i]->selected = false;
	for (int i=0; i<3; i++)
		std::stable_partition( b->sorted_begin[i], b->sorted_end[i], selected_node );

	return nchilds;		
}

/*
 * Helper code
 * PARTITION code / used on mean-split
 * basicly this a std::nth_element (like on C++ STL algorithm)
 */
#if 0
static void split_leafs(RTBuilder *b, int *nth, int partitions, int split_axis)
{
	int i;
	b->split_axis = split_axis;

	for (i=0; i < partitions-1; i++)
	{
		assert(nth[i] < nth[i+1] && nth[i+1] < nth[partitions]);

		if (split_axis == 0)	std::nth_element(b, nth[i],  nth[i+1], nth[partitions], obj_bb_compare<RTBuilder::Object,0>);
		if (split_axis == 1)	std::nth_element(b, nth[i],  nth[i+1], nth[partitions], obj_bb_compare<RTBuilder::Object,1>);
		if (split_axis == 2)	std::nth_element(b, nth[i],  nth[i+1], nth[partitions], obj_bb_compare<RTBuilder::Object,2>);
	}
}
#endif

/*
 * Bounding Box utils
 */
float bb_volume(float *min, float *max)
{
	return (max[0]-min[0])*(max[1]-min[1])*(max[2]-min[2]);
}

float bb_area(float *min, float *max)
{
	float sub[3], a;
	sub[0] = max[0]-min[0];
	sub[1] = max[1]-min[1];
	sub[2] = max[2]-min[2];

	a = (sub[0]*sub[1] + sub[0]*sub[2] + sub[1]*sub[2])*2;
    /* used to have an assert() here on negative results 
     * however, in this case its likely some overflow or ffast math error.
     * so just return 0.0f instead. */
	return a < 0.0f ? 0.0f : a;
}

int bb_largest_axis(float *min, float *max)
{
	float sub[3];
	
	sub[0] = max[0]-min[0];
	sub[1] = max[1]-min[1];
	sub[2] = max[2]-min[2];
	if (sub[0] > sub[1])
	{
		if (sub[0] > sub[2])
			return 0;
		else
			return 2;
	}
	else
	{
		if (sub[1] > sub[2])
			return 1;
		else
			return 2;
	}	
}

int bb_fits_inside(float *outer_min, float *outer_max, float *inner_min, float *inner_max)
{
	int i;
	for (i=0; i<3; i++)
		if (outer_min[i] > inner_min[i]) return 0;

	for (i=0; i<3; i++)
		if (outer_max[i] < inner_max[i]) return 0;

	return 1;	
}
