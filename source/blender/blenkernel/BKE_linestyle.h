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

FreestyleLineStyle *BKE_linestyle_new(const char *name, struct Main *main);
void                BKE_linestyle_free(FreestyleLineStyle *linestyle);
FreestyleLineStyle *BKE_linestyle_copy(FreestyleLineStyle *linestyle);

FreestyleLineStyle *BKE_linestyle_active_from_scene(struct Scene *scene);

LineStyleModifier *BKE_linestyle_modifier_add_color(FreestyleLineStyle *linestyle, const char *name, int type);
LineStyleModifier *BKE_linestyle_modifier_add_alpha(FreestyleLineStyle *linestyle, const char *name, int type);
LineStyleModifier *BKE_linestyle_modifier_add_thickness(FreestyleLineStyle *linestyle, const char *name, int type);
LineStyleModifier *BKE_linestyle_modifier_add_geometry(FreestyleLineStyle *linestyle, const char *name, int type);

LineStyleModifier *BKE_linestyle_modifier_copy_color(FreestyleLineStyle *linestyle, LineStyleModifier *m);
LineStyleModifier *BKE_linestyle_modifier_copy_alpha(FreestyleLineStyle *linestyle, LineStyleModifier *m);
LineStyleModifier *BKE_linestyle_modifier_copy_thickness(FreestyleLineStyle *linestyle, LineStyleModifier *m);
LineStyleModifier *BKE_linestyle_modifier_copy_geometry(FreestyleLineStyle *linestyle, LineStyleModifier *m);

int BKE_linestyle_modifier_remove_color(FreestyleLineStyle *linestyle, LineStyleModifier *modifier);
int BKE_linestyle_modifier_remove_alpha(FreestyleLineStyle *linestyle, LineStyleModifier *modifier);
int BKE_linestyle_modifier_remove_thickness(FreestyleLineStyle *linestyle, LineStyleModifier *modifier);
int BKE_linestyle_modifier_remove_geometry(FreestyleLineStyle *linestyle, LineStyleModifier *modifier);

void BKE_linestyle_modifier_move_color(FreestyleLineStyle *linestyle, LineStyleModifier *modifier, int direction);
void BKE_linestyle_modifier_move_alpha(FreestyleLineStyle *linestyle, LineStyleModifier *modifier, int direction);
void BKE_linestyle_modifier_move_thickness(FreestyleLineStyle *linestyle, LineStyleModifier *modifier, int direction);
void BKE_linestyle_modifier_move_geometry(FreestyleLineStyle *linestyle, LineStyleModifier *modifier, int direction);

void BKE_linestyle_modifier_list_color_ramps(FreestyleLineStyle *linestyle, ListBase *listbase);
char *BKE_linestyle_path_to_color_ramp(FreestyleLineStyle *linestyle, struct ColorBand *color_ramp);

void BKE_linestyle_target_object_unlink(FreestyleLineStyle *linestyle, struct Object *ob);

#endif  /* __BKE_LINESTYLE_H__ */
