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

#include "transform.h"

struct uiBlock;
struct BrushData;
struct IndexNode;
struct KeyBlock;
struct Mesh;
struct Object;
struct PartialVisibility;
struct RenderInfo;
struct Scene;
struct ScrArea;
struct SculptData;
struct SculptUndo;

typedef enum PropsetMode {
	PropsetNone = 0,
	PropsetSize,
	PropsetStrength,
	PropsetTexRot
} PropsetMode;
typedef struct PropsetData {
	PropsetMode mode;
	unsigned int tex;
	short origloc[2];
	float *texdata;
	
	short origsize;
	char origstrength;
	float origtexrot;
	
	NumInput num;
} PropsetData;

typedef struct SculptSession {
	/* Cache of the OpenGL matrices */
	double modelviewmat[16];
	double projectionmat[16];
	int viewport[4];
	
	/* An array of lists; array is sized as
	   large as the number of verts in the mesh,
	   the list for each vert contains the index
	   for all the faces that use that vertex */
	struct ListBase *vertex_users;
	struct IndexNode *vertex_users_mem;
	int vertex_users_size;
	
	/* Used to cache the render of the active texture */
	struct RenderInfo *texrndr;
	
	PropsetData *propset;
	
	struct SculptUndo *undo;
	
	/* For rotating around a pivot point */
	vec3f pivot;
} SculptSession;

SculptSession *sculpt_session();
struct SculptData *sculpt_data();

/* Memory */
void sculptmode_init(struct Scene *);
void sculptmode_free_all(struct Scene *);
void sculptmode_correct_state();

/* Undo */
typedef enum SculptUndoType {
	SUNDO_VERT= 1, /* Vertex locations modified */
	SUNDO_TOPO= 2, /* Any face/edge change, different # of verts, etc. */
	SUNDO_PVIS= 4, /* Mesh.pv changed */
	SUNDO_MRES= 8  /* Mesh.mr changed */
} SculptUndoType;
void sculptmode_undo_push(char *str, SculptUndoType type);
void sculptmode_undo();
void sculptmode_redo();
void sculptmode_undo_menu();

/* Interface */
void sculptmode_draw_interface_tools(struct uiBlock *block,unsigned short cx, unsigned short cy);
void sculptmode_draw_interface_textures(struct uiBlock *block,unsigned short cx, unsigned short cy);
void sculptmode_rem_tex(void*,void*);
void sculptmode_propset_init(PropsetMode mode);
void sculptmode_propset(const unsigned short event);
void sculptmode_selectbrush_menu();
void sculptmode_draw_mesh(int);
void sculpt_paint_brush(char clear);

struct BrushData *sculptmode_brush();
float *get_tex_angle();

void sculptmode_update_tex();
char sculpt_modifiers_active(struct Object *ob);
void sculpt();
void set_sculpt_object(struct Object *ob);
void set_sculptmode();

/* Partial Mesh Visibility */
struct PartialVisibility *sculptmode_copy_pmv(struct PartialVisibility *);
void sculptmode_pmv_free(struct PartialVisibility *);
void sculptmode_revert_pmv(struct Mesh *me);
void sculptmode_pmv_off(struct Mesh *me);
void sculptmode_pmv(int mode);

#endif
