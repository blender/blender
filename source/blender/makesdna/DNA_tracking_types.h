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

/** \file DNA_tracking_types.h
 *  \ingroup DNA
 *  \since may-2011
 *  \author Sergey Sharybin
 */

#ifndef __DNA_TRACKING_TYPES_H__
#define __DNA_TRACKING_TYPES_H__

#include "DNA_listBase.h"

/* match-moving data */

struct ImBuf;
struct MovieReconstructedCamera;
struct MovieTrackingCamera;
struct MovieTrackingBundle;
struct MovieTrackingMarker;
struct MovieTrackingTrack;
struct MovieTracking;

typedef struct MovieReconstructedCamera {
	int framenr;
	float error;
	float mat[4][4];
} MovieReconstructedCamera;

typedef struct MovieTrackingCamera {
	void *intrinsics;	/* intrinsics handle */

	float sensor_width;	/* width of CCD sensor */
	float pixel_aspect;	/* pixel aspect ratio */
	float pad;
	float focal;		/* focal length */
	short units;		/* units of focal length user is working with */
	short pad1;
	float principal[2];	/* principal point */
	float k1, k2, k3;	/* radial distortion */
} MovieTrackingCamera;

typedef struct MovieTrackingMarker {
	float pos[2];	/* 2d position of marker on frame (in unified 0..1 space) */
	int framenr;	/* number of frame marker is associated with */
	int flag;		/* Marker's flag (alive, ...) */
} MovieTrackingMarker;

typedef struct MovieTrackingTrack {
	struct MovieTrackingTrack *next, *prev;

	char name[64];	/* MAX_NAME */

	/* ** setings ** */
	float pat_min[2], pat_max[2];		/* positions of left-bottom and right-top corners of pattern (in unified 0..1 space) */
	float search_min[2], search_max[2];	/* positions of left-bottom and right-top corners of search area (in unified 0..1 space) */
	float offset[2];					/* offset to "parenting" point */

	/* ** track ** */
	int markersnr;					/* count of markers in track */
	int last_marker;				/* most recently used marker */
	MovieTrackingMarker *markers;	/* markers in track */

	/* ** reconstruction data ** */
	float bundle_pos[3];			/* reconstructed position */
	float error;					/* average track reprojection error */

	/* ** UI editing ** */
	int flag, pat_flag, search_flag;	/* flags (selection, ...) */
	float color[3];						/* custom color for track */

	/* tracking algorithm to use; can be KLT or SAD */
	short frames_limit;		/* number of frames to be tarcked during single tracking session (if TRACKING_FRAMES_LIMIT is set) */
	short margin;			/* margin from frame boundaries */
	short pattern_match;	/* re-adjust every N frames */

	short tracker;			/* tracking algorithm used for this track */

	/* ** KLT tracker settings ** */
	short pyramid_levels, pad2;		/* number of pyramid levels to use for KLT tracking */

	/* ** SAD tracker settings ** */
	float minimum_correlation;			/* minimal correlation which is still treated as successful tracking */
} MovieTrackingTrack;

typedef struct MovieTrackingSettings {
	int flag;

	/* ** default tracker settings */
	short default_tracker;				/* tracking algorithm used by default */
	short default_pyramid_levels;		/* number of pyramid levels to use for KLT tracking */
	float default_minimum_correlation;	/* minimal correlation which is still treated as successful tracking */
	short default_pattern_size;			/* size of pattern area for new tracks */
	short default_search_size;			/* size of search area for new tracks */
	short default_frames_limit;			/* number of frames to be tarcked during single tracking session (if TRACKING_FRAMES_LIMIT is set) */
	short default_margin;				/* margin from frame boundaries */
	short default_pattern_match;		/* re-adjust every N frames */
	short default_flag;					/* default flags like color channels used by default */

	short pod;

	/* ** common tracker settings ** */
	short speed;			/* speed of tracking */

	/* ** reconstruction settings ** */
	int keyframe1, keyframe2;	/* two keyframes for reconstrution initialization */

	/* ** which camera intrinsics to refine. uses on the REFINE_* flags */
	short refine_camera_intrinsics, pad23;

	/* ** tool settings ** */

	/* set scale */
	float dist;					/* distance between two bundles used for scene scaling */

	/* cleanup */
	int clean_frames, clean_action;
	float clean_error;

	/* set object scale */
	float object_distance;		/* distance between two bundles used for object scaling */

	int pad3;
} MovieTrackingSettings;

typedef struct MovieTrackingStabilization {
	int flag;
	int tot_track, act_track;		/* total number and index of active track in list */

	/* 2d stabilization */
	float maxscale;			/* max auto-scale factor */
	MovieTrackingTrack *rot_track;	/* track used to stabilize rotation */

	float locinf, scaleinf, rotinf;	/* influence on location, scale and rotation */

	int filter;		/* filter used for pixel interpolation */

	/* some pre-computing run-time variables */
	int ok;						/* are precomputed values and scaled buf relevant? */
	float scale;				/* autoscale factor */

	struct ImBuf *scaleibuf;	/* currently scaled ibuf */
} MovieTrackingStabilization;

typedef struct MovieTrackingReconstruction {
	int flag;

	float error;		/* average error of reconstruction */

	int last_camera;		/* most recently used camera */
	int camnr;				/* number of reconstructed cameras */
	struct MovieReconstructedCamera *cameras;	/* reconstructed cameras */
} MovieTrackingReconstruction;

typedef struct MovieTrackingObject {
	struct MovieTrackingObject *next, *prev;

	char name[64];			/* Name of tracking object, MAX_NAME */
	int flag;
	float scale;			/* scale of object solution in amera space */

	ListBase tracks;		/* list of tracks use to tracking this object */
	MovieTrackingReconstruction reconstruction;	/* reconstruction data for this object */
} MovieTrackingObject;

typedef struct MovieTrackingStats {
	char message[256];
} MovieTrackingStats;

typedef struct MovieTracking {
	MovieTrackingSettings settings;	/* different tracking-related settings */
	MovieTrackingCamera camera;		/* camera intrinsics */
	ListBase tracks;				/* list of tracks used for camera object */
	MovieTrackingReconstruction reconstruction;	/* reconstruction data for camera object */
	MovieTrackingStabilization stabilization;	/* stabilization data */
	MovieTrackingTrack *act_track;		/* active track */

	ListBase objects;
	int objectnr, tot_object;		/* index of active object and total number of objects */

	MovieTrackingStats *stats;		/* statistics displaying in clip editor */
} MovieTracking;

/* MovieTrackingCamera->units */
enum {
	CAMERA_UNITS_PX = 0,
	CAMERA_UNITS_MM
};

/* MovieTrackingMarker->flag */
#define MARKER_DISABLED	(1<<0)
#define MARKER_TRACKED	(1<<1)
#define MARKER_GRAPH_SEL_X (1<<2)
#define MARKER_GRAPH_SEL_Y (1<<3)

/* MovieTrackingTrack->flag */
#define TRACK_HAS_BUNDLE	(1<<1)
#define TRACK_DISABLE_RED	(1<<2)
#define TRACK_DISABLE_GREEN	(1<<3)
#define TRACK_DISABLE_BLUE	(1<<4)
#define TRACK_HIDDEN		(1<<5)
#define TRACK_LOCKED		(1<<6)
#define TRACK_CUSTOMCOLOR	(1<<7)
#define TRACK_USE_2D_STAB	(1<<8)
#define TRACK_PREVIEW_GRAYSCALE	(1<<9)

/* MovieTrackingTrack->tracker */
#define TRACKER_KLT		0
#define TRACKER_SAD		1
#define TRACKER_HYBRID		2

/* MovieTrackingTrack->adjframes */
#define TRACK_MATCH_KEYFRAME		0
#define TRACK_MATCH_PREVFRAME		1

/* MovieTrackingSettings->flag */
#define TRACKING_SETTINGS_SHOW_DEFAULT_EXPANDED	(1<<0)

/* MovieTrackingSettings->speed */
#define TRACKING_SPEED_FASTEST		0
#define TRACKING_SPEED_REALTIME		1
#define TRACKING_SPEED_HALF			2
#define TRACKING_SPEED_QUARTER		4
#define TRACKING_SPEED_DOUBLE		5

/* MovieTrackingSettings->refine_camera_intrinsics */
#define REFINE_FOCAL_LENGTH			(1<<0)
#define REFINE_PRINCIPAL_POINT		(1<<1)
#define REFINE_RADIAL_DISTORTION_K1	(1<<2)
#define REFINE_RADIAL_DISTORTION_K2	(1<<4)

/* MovieTrackingStrabilization->flag */
#define TRACKING_2D_STABILIZATION	(1<<0)
#define TRACKING_AUTOSCALE			(1<<1)
#define TRACKING_STABILIZE_ROTATION	(1<<2)

/* MovieTrackingStrabilization->filter */
#define TRACKING_FILTER_NEAREAST	0
#define TRACKING_FILTER_BILINEAR	1
#define TRACKING_FILTER_BICUBIC		2

/* MovieTrackingReconstruction->flag */
#define TRACKING_RECONSTRUCTED	(1<<0)

/* MovieTrackingObject->flag */
#define TRACKING_OBJECT_CAMERA		(1<<0)

#define TRACKING_CLEAN_SELECT			0
#define TRACKING_CLEAN_DELETE_TRACK		1
#define TRACKING_CLEAN_DELETE_SEGMENT	2

#endif
