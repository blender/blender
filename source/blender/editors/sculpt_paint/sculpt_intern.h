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
 * Inc., 51 Franklin Street, Fifth Floor, Boston, MA 02110-1301, USA.
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

/** \file blender/editors/sculpt_paint/sculpt_intern.h
 *  \ingroup edsculpt
 */
 

#ifndef __SCULPT_INTERN_H__
#define __SCULPT_INTERN_H__

#include "DNA_listBase.h"
#include "DNA_vec_types.h"
#include "DNA_key_types.h"

#include "BLI_pbvh.h"

struct bContext;
struct Brush;
struct KeyBlock;
struct Mesh;
struct MultiresModifierData;
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
struct MultiresModifierData *sculpt_multires_active(struct Scene *scene, struct Object *ob);

struct Brush *sculptmode_brush(void);

void sculpt(Sculpt *sd);

int sculpt_poll(struct bContext *C);
void sculpt_update_mesh_elements(struct Scene *scene, struct Sculpt *sd, struct Object *ob, int need_pmap);

/* Deformed mesh sculpt */
void free_sculptsession_deformMats(struct SculptSession *ss);

/* Stroke */
struct SculptStroke *sculpt_stroke_new(const int max);
void sculpt_stroke_free(struct SculptStroke *);
void sculpt_stroke_add_point(struct SculptStroke *, const short x, const short y);
void sculpt_stroke_apply(struct Sculpt *sd, struct SculptStroke *);
void sculpt_stroke_apply_all(struct Sculpt *sd, struct SculptStroke *);
int sculpt_stroke_get_location(bContext *C, float out[3], float mouse[2]);

/* Undo */

typedef struct SculptUndoNode {
	struct SculptUndoNode *next, *prev;

	char idname[MAX_ID_NAME];	/* name instead of pointer*/
	void *node;					/* only during push, not valid afterwards! */

	float (*co)[3];
	float (*orig_co)[3];
	short (*no)[3];
	int totvert;

	/* non-multires */
	int maxvert;				/* to verify if totvert it still the same */
	int *index;					/* to restore into right location */

	/* multires */
	int maxgrid;				/* same for grid */
	int gridsize;				/* same for grid */
	int totgrid;				/* to restore into right location */
	int *grids;					/* to restore into right location */

	/* layer brush */
	float *layer_disp;

	/* shape keys */
	char shapeName[sizeof(((KeyBlock *)0))->name];
} SculptUndoNode;

SculptUndoNode *sculpt_undo_push_node(Object *ob, PBVHNode *node);
SculptUndoNode *sculpt_undo_get_node(PBVHNode *node);
void sculpt_undo_push_begin(const char *name);
void sculpt_undo_push_end(void);

void sculpt_vertcos_to_key(Object *ob, KeyBlock *kb, float (*vertCos)[3]);

#endif
