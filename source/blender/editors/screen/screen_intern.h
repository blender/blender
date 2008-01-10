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
struct wmOperator;
struct wmEvent;

/* area.c */
void area_copy_data(ScrArea *sa1, ScrArea *sa2, int swap_space);

/* screen_edit.c */
int screen_cursor_test(bContext *C, wmOperator *op, wmEvent *event);

void ED_SCR_OT_move_areas(wmOperatorType *ot);


#endif /* ED_SCREEN_INTERN_H */

