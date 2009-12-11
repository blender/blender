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

#include "MEM_guardedalloc.h"

#include "DNA_brush_types.h"
#include "DNA_object_types.h"
#include "DNA_mesh_types.h"
#include "DNA_scene_types.h"

#include "BKE_brush.h"
#include "BKE_global.h"
#include "BKE_library.h"
#include "BKE_paint.h"

#include <stdlib.h>
#include <string.h>

const char PAINT_CURSOR_SCULPT[3] = {255, 100, 100};
const char PAINT_CURSOR_VERTEX_PAINT[3] = {255, 255, 255};
const char PAINT_CURSOR_WEIGHT_PAINT[3] = {200, 200, 255};
const char PAINT_CURSOR_TEXTURE_PAINT[3] = {255, 255, 255};

Paint *paint_get_active(Scene *sce)
{
	if(sce) {
		ToolSettings *ts = sce->toolsettings;
		
		if(sce->basact && sce->basact->object) {
			switch(sce->basact->object->mode) {
			case OB_MODE_SCULPT:
				return &ts->sculpt->paint;
			case OB_MODE_VERTEX_PAINT:
				return &ts->vpaint->paint;
			case OB_MODE_WEIGHT_PAINT:
				return &ts->wpaint->paint;
			case OB_MODE_TEXTURE_PAINT:
				return &ts->imapaint.paint;
			}
		}

		/* default to image paint */
		return &ts->imapaint.paint;
	}

	return NULL;
}

Brush *paint_brush(Paint *p)
{
	return p && p->brushes ? p->brushes[p->active_brush_index] : NULL;
}

void paint_brush_set(Paint *p, Brush *br)
{
	if(p && !br) {
		/* Setting to NULL removes the current slot */
		paint_brush_slot_remove(p);
	}
	else if(p) {
		int found = 0;
	
		if(p->brushes) {
			int i;
			
			/* See if there's already a slot with the brush */
			for(i = 0; i < p->brush_count; ++i) {
				if(p->brushes[i] == br) {
					p->active_brush_index = i;
					found = 1;
					break;
				}
			}
			
		}
		
		if(!found) {
			paint_brush_slot_add(p);
			id_us_plus(&br->id);
		}
		
		/* Make sure the current slot is the new brush */
		p->brushes[p->active_brush_index] = br;
	}
}

static void paint_brush_slots_alloc(Paint *p, const int count)
{
	p->brush_count = count;
	if(count == 0)
		p->brushes = NULL;
	else
		p->brushes = MEM_callocN(sizeof(Brush*) * count, "Brush slots");
}

void paint_brush_slot_add(Paint *p)
{
	if(p) {
		Brush **orig = p->brushes;
		int orig_count = p->brushes ? p->brush_count : 0;

		/* Increase size of brush slot array */
		paint_brush_slots_alloc(p, orig_count + 1);
		if(orig) {
			memcpy(p->brushes, orig, sizeof(Brush*) * orig_count);
			MEM_freeN(orig);
		}

		p->active_brush_index = orig_count;
	}
}

void paint_brush_slot_remove(Paint *p)
{
	if(p && p->brushes) {
		Brush **orig = p->brushes;
		int src, dst;
		
		/* Decrease size of brush slot array */
		paint_brush_slots_alloc(p, p->brush_count - 1);
		if(p->brushes) {
			for(src = 0, dst = 0; dst < p->brush_count; ++src) {
				if(src != p->active_brush_index) {
					p->brushes[dst] = orig[src];
					++dst;
				}
			}
		}
		MEM_freeN(orig);

		if(p->active_brush_index >= p->brush_count)
			p->active_brush_index = p->brush_count - 1;
		if(p->active_brush_index < 0)
			p->active_brush_index = 0;
	}
}

int paint_facesel_test(Object *ob)
{
	return (ob && ob->type==OB_MESH && ob->data && (((Mesh *)ob->data)->editflag & ME_EDIT_PAINT_MASK) && (ob->mode & (OB_MODE_VERTEX_PAINT|OB_MODE_WEIGHT_PAINT|OB_MODE_TEXTURE_PAINT)));
}

void paint_init(Paint *p, const char col[3])
{
	Brush *brush;

	/* If there's no brush, create one */
	brush = paint_brush(p);
	brush_check_exists(&brush, "Brush");
	paint_brush_set(p, brush);

	memcpy(p->paint_cursor_col, col, 3);
	p->paint_cursor_col[3] = 128;

	p->flags |= PAINT_SHOW_BRUSH;
}

void free_paint(Paint *paint)
{
	if(paint->brushes)
		MEM_freeN(paint->brushes);
}

void copy_paint(Paint *orig, Paint *new)
{
	if(orig->brushes) {
		int i;
		new->brushes = MEM_dupallocN(orig->brushes);
		for(i = 0; i < orig->brush_count; ++i)
			id_us_plus((ID *)new->brushes[i]);
	}
}
