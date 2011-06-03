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
 * The Original Code is: all of this file.
 *
 * Contributor(s): Blender Foundation,
 *                 Sergey Sharybin
 *
 * ***** END GPL LICENSE BLOCK *****
 */

#ifndef DNA_TRACKING_TYPES_H
#define DNA_TRACKING_TYPES_H

/** \file DNA_tracking_types.h
 *  \ingroup DNA
 *  \since may-2011
 *  \author Sergey Sharybin
 */

#include "DNA_listBase.h"

/* match-moving data */

typedef struct MovieTrackingCamera {
	float focal;	/* focal length */
	float pad;
} MovieTrackingCamera;

typedef struct MovieTrackingMarker {
	struct MovieTrackingMarker *next, *prev;
	float pos[2];						/* 2d position of marker on frame (in unified 0..1 space) */
	float pat_min[2], pat_max[2];		/* positions of left-bottom and right-top corners of pattern (in unified 0..1 space) */
	float search_min[2], search_max[2];	/* positions of left-bottom and right-top corners of search area (in unified 0..1 space) */
	int flag, pat_flag, search_flag;	/* flags (selection, ...) */
	int pad;
} MovieTrackingMarker;

typedef struct MovieTracking {
	MovieTrackingCamera camera;
	ListBase markers;
} MovieTracking;

#endif
