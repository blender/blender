/* BKE_linestyle.h
 *
 *
 * $Id: BKE_particle.h 29187 2010-06-03 15:39:02Z kjym3 $
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
 * Inc., 51 Franklin Street, Fifth Floor, Boston, MA 02110-1301, USA.
 *
 * The Original Code is Copyright (C) 2010 Blender Foundation.
 * All rights reserved.
 *
 * The Original Code is: all of this file.
 *
 * Contributor(s): none yet.
 *
 * ***** END GPL LICENSE BLOCK *****
 */

#ifndef BKE_LINESTYLE_H
#define BKE_LINESTYLE_H

#include "DNA_linestyle_types.h"

#define LS_MODIFIER_TYPE_COLOR      1
#define LS_MODIFIER_TYPE_ALPHA      2
#define LS_MODIFIER_TYPE_THICKNESS  3
#define LS_MODIFIER_TYPE_GEOMETRY   4

struct Main;
struct Object;

FreestyleLineStyle *FRS_new_linestyle(const char *name, struct Main *main);
void FRS_free_linestyle(FreestyleLineStyle *linestyle);
FreestyleLineStyle *FRS_copy_linestyle(FreestyleLineStyle *linestyle);

LineStyleModifier *FRS_add_linestyle_color_modifier(FreestyleLineStyle *linestyle, int type);
LineStyleModifier *FRS_add_linestyle_alpha_modifier(FreestyleLineStyle *linestyle, int type);
LineStyleModifier *FRS_add_linestyle_thickness_modifier(FreestyleLineStyle *linestyle, int type);
LineStyleModifier *FRS_add_linestyle_geometry_modifier(FreestyleLineStyle *linestyle, int type);

LineStyleModifier *FRS_copy_linestyle_color_modifier(FreestyleLineStyle *linestyle, LineStyleModifier *m);
LineStyleModifier *FRS_copy_linestyle_alpha_modifier(FreestyleLineStyle *linestyle, LineStyleModifier *m);
LineStyleModifier *FRS_copy_linestyle_thickness_modifier(FreestyleLineStyle *linestyle, LineStyleModifier *m);
LineStyleModifier *FRS_copy_linestyle_geometry_modifier(FreestyleLineStyle *linestyle, LineStyleModifier *m);

void FRS_remove_linestyle_color_modifier(FreestyleLineStyle *linestyle, LineStyleModifier *modifier);
void FRS_remove_linestyle_alpha_modifier(FreestyleLineStyle *linestyle, LineStyleModifier *modifier);
void FRS_remove_linestyle_thickness_modifier(FreestyleLineStyle *linestyle, LineStyleModifier *modifier);
void FRS_remove_linestyle_geometry_modifier(FreestyleLineStyle *linestyle, LineStyleModifier *modifier);

void FRS_move_linestyle_color_modifier(FreestyleLineStyle *linestyle, LineStyleModifier *modifier, int direction);
void FRS_move_linestyle_alpha_modifier(FreestyleLineStyle *linestyle, LineStyleModifier *modifier, int direction);
void FRS_move_linestyle_thickness_modifier(FreestyleLineStyle *linestyle, LineStyleModifier *modifier, int direction);
void FRS_move_linestyle_geometry_modifier(FreestyleLineStyle *linestyle, LineStyleModifier *modifier, int direction);

void FRS_list_modifier_color_ramps(FreestyleLineStyle *linestyle, ListBase *listbase);
char *FRS_path_from_ID_to_color_ramp(FreestyleLineStyle *linestyle, ColorBand *color_ramp);

void FRS_unlink_linestyle_target_object(FreestyleLineStyle *linestyle, struct Object *ob);

#endif
