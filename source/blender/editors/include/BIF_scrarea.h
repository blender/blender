/**
 * $Id: BIF_scrarea.h 229 2002-12-27 13:11:01Z mein $
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

#ifdef __cplusplus
extern "C" {
#endif

struct ScrArea;

	/**
	 * Finds the first spacedata of @a type within
	 * the scrarea.
	 */
void *scrarea_find_space_of_type(ScrArea *sa, int type);

int		scrarea_get_win_x		(struct ScrArea *sa);
int		scrarea_get_win_y		(struct ScrArea *sa);
int		scrarea_get_win_width	(struct ScrArea *sa);
int		scrarea_get_win_height	(struct ScrArea *sa);

#ifdef __cplusplus
}

#endif

