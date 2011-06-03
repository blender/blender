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
 * The Original Code is Copyright (C) Blender Foundation.
 * All rights reserved.
 *
 * Contributor(s): Blender Foundation,
 *                 Sergey Sharybin
 *
 * ***** END GPL LICENSE BLOCK *****
 */

#ifndef BKE_TRACKING_H
#define BKE_TRACKING_H

/** \file BKE_trackingp.h
 *  \ingroup bke
 *  \author Sergey Sharybin
 */

struct MovieTrackingMarker;

void BKE_tracking_clamp_marker(struct MovieTrackingMarker *marker, int event);
void BKE_tracking_marker_flag(struct MovieTrackingMarker *marker, int area, int flag, int clear);

#define MARKER_SELECTED(marker) ((marker)->flag&SELECT || (marker)->pat_flag&SELECT || (marker)->search_flag&SELECT)
#define MARKER_AREA_SELECTED(marker, area) ((area)==MARKER_AREA_POINT?(marker)->flag&SELECT : ((area)==MARKER_AREA_PAT?(marker)->pat_flag&SELECT:(marker)->search_flag&SELECT))

#define CLAMP_PAT_DIM		1
#define CLAMP_PAT_POS		2
#define CLAMP_SEARCH_DIM	3
#define CLAMP_SEARCH_POS	4

#define MARKER_AREA_NONE	-1
#define MARKER_AREA_ALL		0
#define MARKER_AREA_POINT	1
#define MARKER_AREA_PAT		2
#define MARKER_AREA_SEARCH	3

#endif
