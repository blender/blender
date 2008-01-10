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
#ifndef ED_SCREEN_H
#define ED_SCREEN_H

#include "DNA_screen_types.h"
#include "DNA_space_types.h"
#include "DNA_view2d_types.h"
#include "DNA_view3d_types.h"

struct wmWindowManager;
struct wmWindow;
struct wmNotifier;

/* regions */
void	ED_region_do_listen(ARegion *ar, struct wmNotifier *note);
void	ED_region_do_draw(struct bContext *C, ARegion *ar);
void	ED_region_do_refresh(struct bContext *C, ARegion *ar);

/* spaces */
void	ED_spacetypes_init(void);

/* areas */
void	ED_area_initialize(struct wmWindowManager *wm, struct wmWindow *win, struct ScrArea *sa);

/* screens */
void	ED_screens_initialize(struct wmWindowManager *wm);
void	ED_screen_draw(struct wmWindow *win);
void	ED_screen_refresh(struct wmWindowManager *wm, struct wmWindow *win);
void	ED_screen_do_listen(bScreen *screen, struct wmNotifier *note);
bScreen *ED_screen_duplicate(struct wmWindow *win, bScreen *sc);
void	ED_screen_set_subwinactive(struct wmWindow *win);

void	ed_screen_keymap(struct wmWindowManager *wm);

/* operators; context poll callbacks */
int		ED_operator_screenactive(bContext *C);
int		ED_operator_screen_mainwinactive(bContext *C);


#endif /* ED_SCREEN_H */

