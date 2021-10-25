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
 * The Original Code is Copyright (C) 2008 Blender Foundation.
 * All rights reserved.
 *
 * 
 * Contributor(s): Blender Foundation
 *
 * ***** END GPL LICENSE BLOCK *****
 */

/** \file ED_screen_types.h
 *  \ingroup editors
 */

#ifndef __ED_SCREEN_TYPES_H__
#define __ED_SCREEN_TYPES_H__

/* ----------------------------------------------------- */

/* for animplayer */
typedef struct ScreenAnimData {
	ARegion *ar;            /* do not read from this, only for comparing if region exists */
	short redraws;
	short refresh;
	short flag;             /* flags for playback */
	int sfra;               /* frame that playback was started from */
	int nextfra;            /* next frame to go to (when ANIMPLAY_FLAG_USE_NEXT_FRAME is set) */
	double last_duration;   /* used for frame dropping */
	bool from_anim_edit;    /* playback was invoked from animation editor */
} ScreenAnimData;

/* for animplayer */
enum {
	/* user-setting - frame range is played backwards */
	ANIMPLAY_FLAG_REVERSE       = (1 << 0),
	/* temporary - playback just jumped to the start/end */
	ANIMPLAY_FLAG_JUMPED        = (1 << 1),
	/* drop frames as needed to maintain framerate */
	ANIMPLAY_FLAG_SYNC          = (1 << 2),
	/* don't drop frames (and ignore SCE_FRAME_DROP flag) */
	ANIMPLAY_FLAG_NO_SYNC       = (1 << 3),
	/* use nextfra at next timer update */
	ANIMPLAY_FLAG_USE_NEXT_FRAME = (1 << 4)
};

/* ----------------------------------------------------- */

#define REDRAW_FRAME_AVERAGE 8

/* for playback framerate info 
 * stored during runtime as scene->fps_info
 */
typedef struct ScreenFrameRateInfo {
	double redrawtime;
	double lredrawtime;
	float redrawtimes_fps[REDRAW_FRAME_AVERAGE];
	short redrawtime_index;
} ScreenFrameRateInfo;

/* ----------------------------------------------------- */

/* Enum for Action Zone Edges. Which edge of area is action zone. */
typedef enum {
	AE_RIGHT_TO_TOPLEFT,    /* Region located on the left, _right_ edge is action zone. Region minimized to the top left */
	AE_LEFT_TO_TOPRIGHT,    /* Region located on the right, _left_ edge is action zone. Region minimized to the top right */
	AE_TOP_TO_BOTTOMRIGHT,  /* Region located at the bottom, _top_ edge is action zone. Region minimized to the bottom right */
	AE_BOTTOM_TO_TOPLEFT    /* Region located at the top, _bottom_ edge is action zone. Region minimized to the top left */
} AZEdge;

/* for editing areas/regions */
typedef struct AZone {
	struct AZone *next, *prev;
	ARegion *ar;
	int type;
	/* region-azone, which of the edges (only for AZONE_REGION) */
	AZEdge edge;
	/* for draw */
	short x1, y1, x2, y2;
	/* for clip */
	rcti rect;
	/* for fade in/out */
	float alpha;
} AZone;

/* actionzone type */
#define AZONE_AREA      1  /* corner widgets for splitting areas */
#define AZONE_REGION    2  /* when a region is collapsed, draw a handle to expose */
#define AZONE_FULLSCREEN 3 /* when in editor fullscreen draw a corner to go to normal mode */

#endif /* __ED_SCREEN_TYPES_H__ */
