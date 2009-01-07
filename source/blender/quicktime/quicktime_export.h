/* $Id$ 
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
 * The Original Code is Copyright (C) 2001-2002 by NaN Holding BV.
 * All rights reserved.
 *
 * The Original Code is: all of this file.
 *
 * Contributor(s): none yet.
 *
 * ***** END GPL LICENSE BLOCK *****
 */

#ifndef __QUICKTIME_EXPORT_H__
#define __QUICKTIME_EXPORT_H__

#if defined (_WIN32) || (__APPLE__)

#define __AIFF__

// quicktime movie output functions
struct RenderData;

void start_qt(struct RenderData *rd, int rectx, int recty);	//for movie handle (BKE writeavi.c now)
void append_qt(struct RenderData *rd, int frame, int *pixels, int rectx, int recty);
void end_qt(void);

int  get_qtcodec_settings(struct RenderData *rd);		//for buttons.c
void free_qtcomponentdata(void);
void makeqtstring(struct RenderData *rd, char *string);		//for playanim.c

#endif //(_WIN32) || (__APPLE__)

#endif  // __QUICKTIME_IMP_H__
