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

/* Quicktime codec types defines */
#define QT_CODECTYPE_JPEG				1
#define QT_CODECTYPE_MJPEGA				2
#define QT_CODECTYPE_MJPEGB				3
#define QT_CODECTYPE_DVCPAL				4
#define QT_CODECTYPE_DVCNTSC			5
#define QT_CODECTYPE_MPEG4				6
#define QT_CODECTYPE_H263				7
#define QT_CODECTYPE_H264				8
#define QT_CODECTYPE_RAW				9
#define QT_CODECTYPE_DVCPROHD720p		10
#define	QT_CODECTYPE_DVCPROHD1080i50	11
#define QT_CODECTYPE_DVCPROHD1080i60	12

// quicktime movie output functions
struct RenderData;
struct Scene;

void start_qt(struct Scene *scene, struct RenderData *rd, int rectx, int recty);	//for movie handle (BKE writeavi.c now)
void append_qt(struct RenderData *rd, int frame, int *pixels, int rectx, int recty);
void end_qt(void);

void quicktime_verify_image_type(struct RenderData *rd); //used by RNA for defaults values init, if needed

void free_qtcomponentdata(void);
void makeqtstring(struct RenderData *rd, char *string);		//for playanim.c

#endif //(_WIN32) || (__APPLE__)

#endif  // __QUICKTIME_IMP_H__
