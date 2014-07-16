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
 * The Original Code is Copyright (C) 2010 Blender Foundation.
 * All rights reserved.
 *
 * The Original Code is: all of this file.
 *
 * Contributor(s): none yet.
 *
 * ***** END GPL LICENSE BLOCK *****
 */

#ifndef __BKE_LINESTYLE_H__
#define __BKE_LINESTYLE_H__

/** \file BKE_linestyle.h
 *  \ingroup bke
 *  \brief Blender kernel freestyle line style functionality.
 */

#include "DNA_linestyle_types.h"

#define LS_MODIFIER_TYPE_COLOR      1
#define LS_MODIFIER_TYPE_ALPHA      2
#define LS_MODIFIER_TYPE_THICKNESS  3
#define LS_MODIFIER_TYPE_GEOMETRY   4

struct Main;
struct Object;
struct ColorBand;

FreestyleLineStyle *BKE_new_linestyle(const char *name, struct Main *main);
void BKE_free_linestyle(FreestyleLineStyle *linestyle);
FreestyleLineStyle *BKE_copy_linestyle(FreestyleLineStyle *linestyle);

FreestyleLineStyle *BKE_get_linestyle_from_scene(struct Scene *scene);

LineStyleModifier *BKE_add_linestyle_color_modifier(FreestyleLineStyle *linestyle, const char *name, int type);
LineStyleModifier *BKE_add_linestyle_alpha_modifier(FreestyleLineStyle *linestyle, const char *name, int type);
LineStyleModifier *BKE_add_linestyle_thickness_modifier(FreestyleLineStyle *linestyle, const char *name, int type);
LineStyleModifier *BKE_add_linestyle_geometry_modifier(FreestyleLineStyle *linestyle, const char *name, int type);

LineStyleModifier *BKE_copy_linestyle_color_modifier(FreestyleLineStyle *linestyle, LineStyleModifier *m);
LineStyleModifier *BKE_copy_linestyle_alpha_modifier(FreestyleLineStyle *linestyle, LineStyleModifier *m);
LineStyleModifier *BKE_copy_linestyle_thickness_modifier(FreestyleLineStyle *linestyle, LineStyleModifier *m);
LineStyleModifier *BKE_copy_linestyle_geometry_modifier(FreestyleLineStyle *linestyle, LineStyleModifier *m);

int BKE_remove_linestyle_color_modifier(FreestyleLineStyle *linestyle, LineStyleModifier *modifier);
int BKE_remove_linestyle_alpha_modifier(FreestyleLineStyle *linestyle, LineStyleModifier *modifier);
int BKE_remove_linestyle_thickness_modifier(FreestyleLineStyle *linestyle, LineStyleModifier *modifier);
int BKE_remove_linestyle_geometry_modifier(FreestyleLineStyle *linestyle, LineStyleModifier *modifier);

void BKE_move_linestyle_color_modifier(FreestyleLineStyle *linestyle, LineStyleModifier *modifier, int direction);
void BKE_move_linestyle_alpha_modifier(FreestyleLineStyle *linestyle, LineStyleModifier *modifier, int direction);
void BKE_move_linestyle_thickness_modifier(FreestyleLineStyle *linestyle, LineStyleModifier *modifier, int direction);
void BKE_move_linestyle_geometry_modifier(FreestyleLineStyle *linestyle, LineStyleModifier *modifier, int direction);

void BKE_list_modifier_color_ramps(FreestyleLineStyle *linestyle, ListBase *listbase);
char *BKE_path_from_ID_to_color_ramp(FreestyleLineStyle *linestyle, struct ColorBand *color_ramp);

void BKE_unlink_linestyle_target_object(FreestyleLineStyle *linestyle, struct Object *ob);

#endif  /* __BKE_LINESTYLE_H__ */
