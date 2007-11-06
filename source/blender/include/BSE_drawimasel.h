/**
 * $Id$
 *
 * ***** BEGIN GPL/BL DUAL LICENSE BLOCK *****
 *
 * This program is free software; you can redistribute it and/or
 * modify it under the terms of the GNU General Public License
 * as published by the Free Software Foundation; either version 2
 * of the License, or (at your option) any later version. The Blender
 * Foundation also sells licenses for use in proprietary software under
 * the Blender License.  See http://www.blender.org/BL/ for information
 * about this.
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
 * The Original Code is Copyright (C) 2001-2002 by NaN Holding BV.
 * All rights reserved.
 *
 * The Original Code is: all of this file.
 *
 * Contributor(s): none yet.
 *
 * ***** END GPL/BL DUAL LICENSE BLOCK *****
 */

#ifndef BSE_DRAWIMASEL_H
#define BSE_DRAWIMASEL_H


/* button events */
#define B_FS_FILENAME	1
#define B_FS_DIRNAME	2
#define B_FS_DIR_MENU	3
#define B_FS_PARDIR	4
#define B_FS_LOAD	5
#define B_FS_CANCEL	6
#define B_FS_LIBNAME	7
#define B_FS_BOOKMARK	8

/* ui geometry */
#define IMASEL_BUTTONS_HEIGHT 60
#define TILE_BORDER_X 8
#define TILE_BORDER_Y 8

struct ScrArea;
struct SpaceImaSel;

void drawimaselspace(struct ScrArea *sa, void *spacedata);   
void calc_imasel_rcts(SpaceImaSel *simasel, int winx, int winy);
void do_imasel_buttonevents(short event, SpaceImaSel *simasel);

#endif  /*  BSE_DRAWIMASEL_H */

