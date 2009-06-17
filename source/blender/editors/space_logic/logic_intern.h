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
 * The Original Code is Copyright (C) 2009 Blender Foundation.
 * All rights reserved.
 *
 * 
 * Contributor(s): Blender Foundation
 *
 * ***** END GPL LICENSE BLOCK *****
 */

#ifndef ED_LOGIC_INTERN_H
#define ED_LOGIC_INTERN_H

/* internal exports only */
struct bContext;
struct ARegion;
struct ARegionType;
struct ScrArea;
struct SpaceLogic;
struct Object;
struct wmOperatorType;
struct Scene;

/* space_logic.c */
struct ARegion *logic_has_buttons_region(struct ScrArea *sa);

/* logic_header.c */
void logic_header_buttons(const struct bContext *C, struct ARegion *ar);

/* logic_ops.c */

/* logic_buttons.c */
void logic_buttons_register(struct ARegionType *art);
void LOGIC_OT_properties(struct wmOperatorType *ot);

/* logic_window.c */
void logic_buttons(struct bContext *C, struct ARegion *ar);

#endif /* ED_LOGIC_INTERN_H */

