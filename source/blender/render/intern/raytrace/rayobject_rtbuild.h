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

#ifdef __cplusplus
extern "C" {
#endif

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
#define RTBUILD_MAX_CHILDS 32


typedef struct RTBuilder
{
	struct Object
	{
		RayObject *obj;
		float cost;
		float bb[6];
		int selected;
	};
	
	/* list to all primitives added in this tree */
	struct
	{
		Object *begin, *end;
		int maxsize;
	} primitives;
	
	/* sorted list of rayobjects */
	struct Object **sorted_begin[3], **sorted_end[3];

	/* axis used (if any) on the split method */
	int split_axis;
	
	/* child partitions calculated during splitting */
	int child_offset[RTBUILD_MAX_CHILDS+1];
	
//	int child_sorted_axis; /* -1 if not sorted */
	
	float bb[6];

} RTBuilder;

/* used during creation */
RTBuilder* rtbuild_create(int size);
void rtbuild_free(RTBuilder *b);
void rtbuild_add(RTBuilder *b, RayObject *o);
void rtbuild_done(RTBuilder *b, RayObjectControl *c);
void rtbuild_merge_bb(RTBuilder *b, float *min, float *max);
int rtbuild_size(RTBuilder *b);

RayObject* rtbuild_get_primitive(RTBuilder *b, int offset);

/* used during tree reorganization */
RTBuilder* rtbuild_get_child(RTBuilder *b, int child, RTBuilder *tmp);

/* Calculates child partitions and returns number of efectively needed partitions */
int rtbuild_get_largest_axis(RTBuilder *b);

//Object partition
int rtbuild_mean_split(RTBuilder *b, int nchilds, int axis);
int rtbuild_mean_split_largest_axis(RTBuilder *b, int nchilds);

int rtbuild_heuristic_object_split(RTBuilder *b, int nchilds);

//Space partition
int rtbuild_median_split(RTBuilder *b, float *separators, int nchilds, int axis);
int rtbuild_median_split_largest_axis(RTBuilder *b, int nchilds);


/* bb utils */
float bb_area(float *min, float *max);
float bb_volume(float *min, float *max);
int bb_largest_axis(float *min, float *max);
int bb_fits_inside(float *outer_min, float *outer_max, float *inner_min, float *inner_max); /* only returns 0 if merging inner and outerbox would create a box larger than outer box */

#ifdef __cplusplus
}
#endif

#endif
