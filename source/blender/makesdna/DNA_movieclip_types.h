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

/** \file DNA_movieclip_types.h
 *  \ingroup DNA
 *  \since may-2011
 *  \author Sergey Sharybin
 */

#ifndef DNA_MOVIECLIP_TYPES_H
#define DNA_MOVIECLIP_TYPES_H

#include "DNA_ID.h"
#include "DNA_tracking_types.h"

struct anim;
struct bGPdata;
struct ImBuf;
struct MovieClipProxy;
struct MovieTrackingTrack;
struct MovieTrackingMarker;

typedef struct MovieClipUser {
	int framenr;	/* current frame number */
	short render_size, render_flag;		/* proxy render size */
} MovieClipUser;

typedef struct MovieClipProxy {
	char dir[768];			/* custom directory for index and proxy files (defaults to BL_proxy) */

	short tc;				/* time code in use */
	short quality;			/* proxy build quality */
	short build_size_flag;	/* size flags (see below) of all proxies to build */
	short build_tc_flag;	/* time code flags (see below) of all tc indices to build */
} MovieClipProxy;

typedef struct MovieClip {
	ID id;

	char name[1024];		/* file path, 1024 = FILE_MAX */

	int source;			/* sequence or movie */
	int lastframe;		/* last accessed frame number */
	int lastsize[2];	/* size of last accessed frame */

	float aspx, aspy;	/* display aspect */

	struct anim *anim;	/* movie source data */
	struct MovieClipCache *cache;		/* cache for different stuff, not in file */
	struct bGPdata *gpd;				/* grease pencil data */

	struct MovieTracking tracking;		/* data for SfM tracking */
	void *tracking_context;				/* context of tracking job
										   used to synchronize data like framenumber
										   in SpaceClip clip user */

	struct MovieClipProxy proxy;		/* proxy to clip data */
	int flag, pad;
} MovieClip;

typedef struct MovieClipScopes {
	int ok;							/* 1 means scopes are ok and recalculation is unneeded */
	int track_preview_height;		/* height of track preview widget */
	struct ImBuf *track_preview;	/* ImBuf displayed in track preview */
	float track_pos[2];				/* sub-pizel position of marker in track ImBuf */
	short track_disabled;			/* active track is disabled, special notifier should be drawn */
	char pad[2];
	int framenr;					/* frame number scopes are created for */
	struct MovieTrackingTrack *track;	/* track scopes are created for */
	struct MovieTrackingMarker *marker;	/* marker scopes are created for */
	float slide_scale[2];			/* scale used for sliding from previewe area */
} MovieClipScopes;

/* MovieClipProxy->build_size_flag */
#define MCLIP_PROXY_SIZE_25		(1<<0)
#define MCLIP_PROXY_SIZE_50		(1<<1)
#define MCLIP_PROXY_SIZE_75		(1<<2)
#define MCLIP_PROXY_SIZE_100	(1<<3)
#define MCLIP_PROXY_UNDISTORTED_SIZE_25		(1<<4)
#define MCLIP_PROXY_UNDISTORTED_SIZE_50		(1<<5)
#define MCLIP_PROXY_UNDISTORTED_SIZE_75		(1<<6)
#define MCLIP_PROXY_UNDISTORTED_SIZE_100	(1<<7)

/* MovieClip->source */
#define MCLIP_SRC_SEQUENCE	1
#define MCLIP_SRC_MOVIE		2

/* MovieClip->selection types */
#define MCLIP_SEL_NONE		0
#define MCLIP_SEL_TRACK		1

/* MovieClip->flag */
#define MCLIP_USE_PROXY					(1<<0)
#define MCLIP_USE_PROXY_CUSTOM_DIR		(1<<1)

#define MCLIP_TIMECODE_FLAGS			(MCLIP_USE_PROXY|MCLIP_USE_PROXY_CUSTOM_DIR)

/* MovieClip->render_size */
#define MCLIP_PROXY_RENDER_SIZE_FULL	0
#define MCLIP_PROXY_RENDER_SIZE_25		1
#define MCLIP_PROXY_RENDER_SIZE_50		2
#define MCLIP_PROXY_RENDER_SIZE_75		3
#define MCLIP_PROXY_RENDER_SIZE_100		4

/* MovieClip->render_flag */
#define MCLIP_PROXY_RENDER_UNDISTORT	1

#endif
