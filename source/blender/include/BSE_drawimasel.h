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

struct SpaceImaSel;

void viewgate(short sx, short sy, short ex, short ey);
void areaview (void);
void calc_hilite(struct SpaceImaSel *simasel);
void make_sima_area(struct SpaceImaSel *simasel);
void draw_sima_area(struct SpaceImaSel *simasel);
void select_ima_files(struct SpaceImaSel *simasel);
void move_imadir_sli(struct SpaceImaSel *simasel);
void move_imafile_sli(struct SpaceImaSel *simasel);
void ima_select_all(struct SpaceImaSel *simasel);
void pibplay(struct SpaceImaSel *simasel);
void drawimaselspace(void);   

/*  void calc_hilite(SpaceImaSel *simasel); */
/*  void ima_select_all(SpaceImaSel *simasel); */
/*  void move_imadir_sli(SpaceImaSel *simasel); */
/*  void move_imafile_sli(SpaceImaSel *simasel); */
/*  void pibplay(SpaceImaSel *simasel); */
/*  void select_ima_files(SpaceImaSel *simasel); */

#endif  /*  BSE_DRAWIMASEL_H */

