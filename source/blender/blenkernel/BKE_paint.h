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
 * along with this program; if not, write to the Free Software  Foundation,
 * Inc., 59 Temple Place - Suite 330, Boston, MA  02111-1307, USA.
 *
 * The Original Code is Copyright (C) 2009 by Nicholas Bishop
 * All rights reserved.
 *
 * The Original Code is: all of this file.
 *
 * Contributor(s): none yet.
 *
 * ***** END GPL LICENSE BLOCK *****
 */ 

#ifndef BKE_PAINT_H
#define BKE_PAINT_H

struct Brush;
struct Object;
struct Paint;
struct Scene;

void paint_init(struct Paint *p, const char *brush_name);
void free_paint(struct Paint *p);
void copy_paint(struct Paint *orig, struct Paint *new);

struct Paint *paint_get_active(struct Scene *sce);
struct Brush *paint_brush(struct Paint *paint);
void paint_brush_set(struct Paint *paint, struct Brush *br);
void paint_brush_slot_add(struct Paint *p);
void paint_brush_slot_remove(struct Paint *p);

/* testing face select mode
 * Texture paint could be removed since selected faces are not used
 * however hiding faces is useful */
int paint_facesel_test(struct Object *ob);

#endif
