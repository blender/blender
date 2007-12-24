/*
 * $Id: BDR_sculptmode.h 11036 2007-06-24 22:28:28Z nicholasbishop $
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
/* For bglMats */
#include "BIF_glutil.h"
#include "transform.h"

struct uiBlock;
struct BrushData;
struct EditData;
struct IndexNode;
struct KeyBlock;
struct Mesh;
struct Object;
struct PartialVisibility;
struct Scene;
struct ScrArea;
struct SculptData;
struct SculptStroke;

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
	bglMats mats;
	
	/* An array of lists; array is sized as
	   large as the number of verts in the mesh,
	   the list for each vert contains the index
	   for all the faces that use that vertex */
	struct ListBase *vertex_users;
	struct IndexNode *vertex_users_mem;
	int vertex_users_size;

	/* Used temporarily per-stroke */
	float *vertexcosnos;
	ListBase damaged_rects;
	ListBase damaged_verts;
	
	/* Used to cache the render of the active texture */
	unsigned int texcache_w, texcache_h, *texcache;
	
	PropsetData *propset;
	
	/* For rotating around a pivot point */
	vec3f pivot;

	struct SculptStroke *stroke;
} SculptSession;

SculptSession *sculpt_session(void);
struct SculptData *sculpt_data(void);

/* Memory */
void sculptmode_init(struct Scene *);
void sculptmode_free_all(struct Scene *);
void sculptmode_correct_state(void);

/* Interface */
void sculptmode_draw_interface_tools(struct uiBlock *block,unsigned short cx, unsigned short cy);
void sculptmode_draw_interface_textures(struct uiBlock *block,unsigned short cx, unsigned short cy);
void sculptmode_rem_tex(void*,void*);
void sculptmode_propset_init(PropsetMode mode);
void sculptmode_propset(const unsigned short event);
void sculptmode_selectbrush_menu(void);
void sculptmode_draw_mesh(int);
void sculpt_paint_brush(char clear);
void sculpt_stroke_draw();

struct BrushData *sculptmode_brush(void);
float tex_angle(void);
void do_symmetrical_brush_actions(struct EditData *e, short *, short *);

void sculptmode_update_tex(void);
char sculpt_modifiers_active(struct Object *ob);
void sculpt(void);
void set_sculptmode(void);

/* Stroke */
void sculpt_stroke_new(const int max);
void sculpt_stroke_free();
void sculpt_stroke_add_point(const short x, const short y);
void sculpt_stroke_apply(struct EditData *);
void sculpt_stroke_apply_all(struct EditData *e);
void sculpt_stroke_draw();


/* Partial Mesh Visibility */
struct PartialVisibility *sculptmode_copy_pmv(struct PartialVisibility *);
void sculptmode_pmv_free(struct PartialVisibility *);
void sculptmode_revert_pmv(struct Mesh *me);
void sculptmode_pmv_off(struct Mesh *me);
void sculptmode_pmv(int mode);

#endif
