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
 * The Original Code is Copyright (C) 2009 Blender Foundation.
 * All rights reserved.
 *
 * 
 * Contributor(s): Blender Foundation
 *
 * ***** END GPL LICENSE BLOCK *****
 */

/** \file blender/editors/space_logic/logic_intern.h
 *  \ingroup splogic
 */


#ifndef __LOGIC_INTERN_H__
#define __LOGIC_INTERN_H__

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

/* logic_ops.c */

/* logic_buttons.c */
void logic_buttons_register(struct ARegionType *art);
void LOGIC_OT_properties(struct wmOperatorType *ot);
void LOGIC_OT_links_cut(struct wmOperatorType *ot);

/* logic_window.c */
void logic_buttons(struct bContext *C, struct ARegion *ar);
void make_unique_prop_names(struct bContext *C, char *str);

#endif /* __LOGIC_INTERN_H__ */

