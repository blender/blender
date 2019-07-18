/*
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
 */

/** \file
 * \ingroup editors
 */

#ifndef __ED_TEXT_H__
#define __ED_TEXT_H__

struct ARegion;
struct SpaceText;
struct UndoStep;
struct UndoType;

bool ED_text_region_location_from_cursor(struct SpaceText *st,
                                         struct ARegion *ar,
                                         const int cursor_co[2],
                                         int r_pixel_co[2]);

/* text_undo.c */
void ED_text_undosys_type(struct UndoType *ut);

struct UndoStep *ED_text_undo_push_init(struct bContext *C);

#endif /* __ED_TEXT_H__ */
