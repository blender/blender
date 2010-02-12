/**
 * $Id:
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
 * The Original Code is Copyright (C) 2008 Blender Foundation.
 * All rights reserved.
 *
 * 
 * Contributor(s): Blender Foundation
 *
 * ***** END GPL LICENSE BLOCK *****
 */

#ifndef ED_SCREEN_TYPES_H__
#define ED_SCREEN_TYPES_H__

/* ----------------------------------------------------- */

/* for animplayer */
typedef struct ScreenAnimData {
	ARegion *ar;		/* do not read from this, only for comparing if region exists */
	short redraws;
	short flag;			/* flags for playback */
	int sfra;			/* frame that playback was started from */
} ScreenAnimData;

/* for animplayer */
enum {
		/* user-setting - frame range is played backwards */
	ANIMPLAY_FLAG_REVERSE		= (1<<0),
		/* temporary - playback just jumped to the start/end */
	ANIMPLAY_FLAG_JUMPED		= (1<<1),
		/* drop frames as needed to maintain framerate */
	ANIMPLAY_FLAG_SYNC			= (1<<2),
		/* don't drop frames (and ignore AUDIO_SYNC flag) */
	ANIMPLAY_FLAG_NO_SYNC		= (1<<3),
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

/* for editing areas/regions */
typedef struct AZone {
	struct AZone *next, *prev;
	ARegion *ar;
	int type;
	/* region-azone, which of the edges */
	short edge;
	/* internal */
	short do_draw;
	/* for draw */
	short x1, y1, x2, y2;
	/* for clip */
	rcti rect;	
} AZone;

/* actionzone type */
#define	AZONE_AREA			1
#define AZONE_REGION		2

#endif /* ED_SCREEN_TYPES_H__ */
