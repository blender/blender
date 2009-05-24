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

/* for animplayer */
typedef struct ScreenAnimData {
	ARegion *ar;	/* do not read from this, only for comparing if region exists */
	int redraws;
} ScreenAnimData;


typedef struct AZone {
	struct AZone *next, *prev;
	int type;
	short flag;
	short do_draw;
	int pos;
	short x1, y1, x2, y2;
} AZone;

/* actionzone type */
#define	AZONE_TRI			1
#define AZONE_QUAD			2

/* actionzone flag */

/* actionzone pos */
#define AZONE_S				1
#define AZONE_SW			2
#define AZONE_W				3
#define AZONE_NW			4
#define AZONE_N				5
#define AZONE_NE			6
#define AZONE_E				7
#define AZONE_SE			8

#endif /* ED_SCREEN_TYPES_H__ */
