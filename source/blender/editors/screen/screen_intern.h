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
#ifndef ED_SCREEN_INTERN_H
#define ED_SCREEN_INTERN_H

/* internal exports only */
struct wmWindow;

/* area.c */
void area_copy_data(ScrArea *sa1, ScrArea *sa2, int swap_space);

/* screen_edit.c */
bScreen *screen_add(struct wmWindow *win, char *name);
ScrEdge *screen_findedge(bScreen *sc, ScrVert *v1, ScrVert *v2);
ScrArea *area_split(wmWindow *win, bScreen *sc, ScrArea *sa, char dir, float fac);
int screen_area_join(bScreen* scr, ScrArea *sa1, ScrArea *sa2);
int area_getorientation(bScreen *screen, ScrArea *sa, ScrArea *sb);

void removenotused_scrverts(bScreen *sc);
void removedouble_scrverts(bScreen *sc);
void removedouble_scredges(bScreen *sc);
void removenotused_scredges(bScreen *sc);

#endif /* ED_SCREEN_INTERN_H */




