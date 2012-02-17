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

/** \file blender/editors/screen/screen_intern.h
 *  \ingroup edscr
 */

#ifndef __SCREEN_INTERN_H__
#define __SCREEN_INTERN_H__

/* internal exports only */
struct wmWindow;
struct Scene;

#define AZONESPOT		12

/* area.c */
void		area_copy_data	(ScrArea *sa1, ScrArea *sa2, int swap_space);

/* screen_edit.c */
ScrEdge		*screen_findedge(bScreen *sc, ScrVert *v1, ScrVert *v2);
ScrArea		*area_split(bScreen *sc, ScrArea *sa, char dir, float fac, int merge);
int			screen_area_join(bContext *C, bScreen* scr, ScrArea *sa1, ScrArea *sa2);
int			area_getorientation(ScrArea *sa, ScrArea *sb);
void		select_connected_scredge(bScreen *sc, ScrEdge *edge);

void		removenotused_scrverts(bScreen *sc);
void		removedouble_scrverts(bScreen *sc);
void		removedouble_scredges(bScreen *sc);
void		removenotused_scredges(bScreen *sc);
int			scredge_is_horizontal(ScrEdge *se);
ScrEdge		*screen_find_active_scredge(bScreen *sc, int mx, int my);

struct AZone *is_in_area_actionzone(ScrArea *sa, int x, int y);

/* screen_context.c */
int ed_screen_context(const bContext *C, const char *member, bContextDataResult *result);

extern const char *screen_context_dir[]; /* doc access */

/* screendump.c */
void SCREEN_OT_screenshot(struct wmOperatorType *ot);
void SCREEN_OT_screencast(struct wmOperatorType *ot);

#endif /* __SCREEN_INTERN_H__ */




