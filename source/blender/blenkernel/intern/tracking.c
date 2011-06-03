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
 * The Original Code is Copyright (C) 2001-2002 by NaN Holding BV.
 * All rights reserved.
 *
 * Contributor(s): Blender Foundation,
 *                 Sergey Sharybin
 *
 * ***** END GPL LICENSE BLOCK *****
 */

/** \file blender/blenkernel/intern/tracking.c
 *  \ingroup bke
 */

#include "MEM_guardedalloc.h"

#include "DNA_movieclip_types.h"

#include "BLI_utildefines.h"
#include "BLI_math.h"

#include "BKE_tracking.h"

void BKE_tracking_clamp_marker(MovieTrackingMarker *marker, int event)
{
	int a;

	/* sort */
	for(a= 0; a<2; a++) {
		if(marker->pat_min[a]>marker->pat_max[a])
			SWAP(float, marker->pat_min[a], marker->pat_max[a]);

		if(marker->search_min[a]>marker->search_max[a])
			SWAP(float, marker->search_min[a], marker->search_max[a]);
	}

	if(event==CLAMP_PAT_DIM) {
		for(a= 0; a<2; a++) {
			marker->pat_min[a]= MAX2(marker->pat_min[a], marker->search_min[a]);
			marker->pat_max[a]= MIN2(marker->pat_max[a], marker->search_max[a]);
		}
	}
	else if(event==CLAMP_PAT_POS) {
		float dim[2];
		sub_v2_v2v2(dim, marker->pat_max, marker->pat_min);

		for(a= 0; a<2; a++) {
			if(marker->pat_min[a] < marker->search_min[a]) {
				marker->pat_min[a]= marker->search_min[a];
				marker->pat_max[a]= marker->pat_min[a]+dim[a];
			}
			if(marker->pat_max[a] > marker->search_max[a]) {
				marker->pat_max[a]= marker->search_max[a];
				marker->pat_min[a]= marker->pat_max[a]-dim[a];
			}
		}
	}
	else if(event==CLAMP_SEARCH_DIM) {
		for(a= 0; a<2; a++) {
			marker->search_min[a]= MIN2(marker->pat_min[a], marker->search_min[a]);
			marker->search_max[a]= MAX2(marker->pat_max[a], marker->search_max[a]);
		}
	}
	else if(event==CLAMP_SEARCH_POS) {
		float dim[2];
		sub_v2_v2v2(dim, marker->search_max, marker->search_min);

		for(a= 0; a<2; a++) {
			if(marker->search_min[a] > marker->pat_min[a]) {
				marker->search_min[a]= marker->pat_min[a];
				marker->search_max[a]= marker->search_min[a]+dim[a];
			}
			if(marker->search_max[a] < marker->pat_max[a]) {
				marker->search_max[a]= marker->pat_max[a];
				marker->search_min[a]= marker->search_max[a]-dim[a];
			}
		}
	}
}

void BKE_tracking_marker_flag(MovieTrackingMarker *marker, int area, int flag, int clear)
{
	if(clear) {
		if(area==MARKER_AREA_ALL || area==MARKER_AREA_POINT) marker->flag&= ~flag;
		if(area==MARKER_AREA_ALL || area==MARKER_AREA_PAT) marker->pat_flag&= ~flag;
		if(area==MARKER_AREA_ALL || area==MARKER_AREA_SEARCH) marker->search_flag&= ~flag;
	} else {
		if(area==MARKER_AREA_ALL || area==MARKER_AREA_POINT) marker->flag|= flag;
		if(area==MARKER_AREA_ALL || area==MARKER_AREA_PAT) marker->pat_flag|= flag;
		if(area==MARKER_AREA_ALL || area==MARKER_AREA_SEARCH) marker->search_flag|= flag;
	}
}
