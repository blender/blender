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

#include "BLI_bitmap.h"
#include "BKE_pbvh.h"

struct bContext;
struct Brush;
struct KeyBlock;
struct Mesh;
struct MultiresModifierData;
struct Object;
struct Scene;
struct Sculpt;
struct SculptStroke;
struct SculptUndoNode;

/* Interface */
struct MultiresModifierData *sculpt_multires_active(struct Scene *scene, struct Object *ob);

int sculpt_mode_poll(struct bContext *C);
int sculpt_mode_poll_view3d(struct bContext *C);
/* checks for a brush, not just sculpt mode */
int sculpt_poll(struct bContext *C);
int sculpt_poll_view3d(struct bContext *C);
void sculpt_update_mesh_elements(struct Scene *scene, struct Sculpt *sd, struct Object *ob,
                                 bool need_pmap, bool need_mask);

/* Stroke */
bool sculpt_stroke_get_location(bContext *C, float out[3], const float mouse[2]);

/* Dynamic topology */
void sculpt_pbvh_clear(Object *ob);
void sculpt_dyntopo_node_layers_add(struct SculptSession *ss);
void sculpt_update_after_dynamic_topology_toggle(bContext *C);
void sculpt_dynamic_topology_enable(struct bContext *C);
void sculpt_dynamic_topology_disable(struct bContext *C,
                                     struct SculptUndoNode *unode);

/* Undo */

typedef enum {
	SCULPT_UNDO_COORDS,
	SCULPT_UNDO_HIDDEN,
	SCULPT_UNDO_MASK,
	SCULPT_UNDO_DYNTOPO_BEGIN,
	SCULPT_UNDO_DYNTOPO_END,
	SCULPT_UNDO_DYNTOPO_SYMMETRIZE,
} SculptUndoType;

typedef struct SculptUndoNode {
	struct SculptUndoNode *next, *prev;

	SculptUndoType type;

	char idname[MAX_ID_NAME];   /* name instead of pointer*/
	void *node;                 /* only during push, not valid afterwards! */

	float (*co)[3];
	float (*orig_co)[3];
	short (*no)[3];
	float *mask;
	int totvert;

	/* non-multires */
	int maxvert;                /* to verify if totvert it still the same */
	int *index;                 /* to restore into right location */
	BLI_bitmap *vert_hidden;

	/* multires */
	int maxgrid;                /* same for grid */
	int gridsize;               /* same for grid */
	int totgrid;                /* to restore into right location */
	int *grids;                 /* to restore into right location */
	BLI_bitmap **grid_hidden;

	/* bmesh */
	struct BMLogEntry *bm_entry;
	bool applied;
	CustomData bm_enter_vdata;
	CustomData bm_enter_edata;
	CustomData bm_enter_ldata;
	CustomData bm_enter_pdata;
	int bm_enter_totvert;
	int bm_enter_totedge;
	int bm_enter_totloop;
	int bm_enter_totpoly;

	/* shape keys */
	char shapeName[sizeof(((KeyBlock *)0))->name];
} SculptUndoNode;

SculptUndoNode *sculpt_undo_push_node(Object *ob, PBVHNode *node, SculptUndoType type);
SculptUndoNode *sculpt_undo_get_node(PBVHNode *node);
void sculpt_undo_push_begin(const char *name);
void sculpt_undo_push_end(void);

void sculpt_vertcos_to_key(Object *ob, KeyBlock *kb, float (*vertCos)[3]);

void sculpt_update_object_bounding_box(struct Object *ob);

#endif
