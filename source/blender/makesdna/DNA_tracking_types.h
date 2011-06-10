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

struct MovieTrackingCamera;
struct MovieTrackingBundle;
struct MovieTrackingMarker;
struct MovieTrackingTrack;
struct MovieTracking;

typedef struct MovieTrackingCamera {
	float focal;	/* focal length */
	float pad;
} MovieTrackingCamera;

typedef struct MovieTrackingMarker {
	float pos[2];	/* 2d position of marker on frame (in unified 0..1 space) */
	int framenr;	/* number of frame marker is associated with */
} MovieTrackingMarker;

typedef struct MovieTrackingBundle {
	struct MovieTrackingBundle *next, *prev;
	float pos[3];	/* 3d position of bundle */

	int flag;		/* flags (selection, ...) */

	struct MovieTrackingTrack *track;	/* track associated with this bundle */
} MovieTrackingBundle;

typedef struct MovieTrackingTrack {
	struct MovieTrackingTrack *next, *prev;

	/* ** setings ** */
	float pat_min[2], pat_max[2];		/* positions of left-bottom and right-top corners of pattern (in unified 0..1 space) */
	float search_min[2], search_max[2];	/* positions of left-bottom and right-top corners of search area (in unified 0..1 space) */

	int pad;

	/* ** track ** */
	int markersnr;					/* count of markers in track */
	int last_marker;				/* most recently used marker */
	int pad2;
	MovieTrackingMarker *markers;	/* markers in track */

	struct MovieTrackingBundle *bundle;	/* bundle, associated with this tracker */

	/* ** UI editing ** */
	int flag, pat_flag, search_flag;	/* flags (selection, ...) */

	char pad3[4];
} MovieTrackingTrack;

typedef struct MovieTracking {
	MovieTrackingCamera camera;
	ListBase tracks;
	ListBase bundles;
} MovieTracking;

/* MovieTrackingTrack->flag */
#define TRACK_PROCESSED	2

#endif
