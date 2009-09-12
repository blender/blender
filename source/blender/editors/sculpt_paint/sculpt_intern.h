/*
 * $Id$
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
 * along with this program; if not, write to the Free Software  Foundation,
 * Inc., 59 Temple Place - Suite 330, Boston, MA  02111-1307, USA.
 *
 * The Original Code is Copyright (C) 2006 by Nicholas Bishop
 * All rights reserved.
 *
 * The Original Code is: all of this file.
 *
 * Contributor(s): none yet.
 *
 * ***** END GPL LICENSE BLOCK *****
 */ 

#ifndef BDR_SCULPTMODE_H
#define BDR_SCULPTMODE_H

#include "DNA_listBase.h"
#include "DNA_vec_types.h"

struct Brush;
struct Mesh;
struct Object;
struct Scene;
struct Sculpt;
struct SculptStroke;

/* Interface */
void sculptmode_selectbrush_menu(void);
void sculptmode_draw_mesh(int);
void sculpt_paint_brush(char clear);
void sculpt_stroke_draw(struct SculptStroke *);
void sculpt_radialcontrol_start(int mode);

struct Brush *sculptmode_brush(void);
//void do_symmetrical_brush_actions(struct Sculpt *sd, struct wmOperator *wm, struct BrushAction *a, short *, short *);

char sculpt_modifiers_active(struct Object *ob);
void sculpt(Sculpt *sd);

/* Stroke */
struct SculptStroke *sculpt_stroke_new(const int max);
void sculpt_stroke_free(struct SculptStroke *);
void sculpt_stroke_add_point(struct SculptStroke *, const short x, const short y);
void sculpt_stroke_apply(struct Sculpt *sd, struct SculptStroke *);
void sculpt_stroke_apply_all(struct Sculpt *sd, struct SculptStroke *);

/* Partial Mesh Visibility */
void sculptmode_pmv(int mode);

#endif
