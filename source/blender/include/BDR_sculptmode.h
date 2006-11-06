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

struct uiBlock;
struct BrushData;
struct Mesh;
struct Object;
struct Scene;
struct ScrArea;

typedef struct PropsetData {
	short origloc[2];
	short origsize;
	char origstrength;
	unsigned int tex;
} PropsetData;

/* Memory */
void sculptmode_init(struct Scene *);
void sculptmode_free_all(struct Scene *);

/* Undo */
void sculptmode_undo_push(char *str);
void sculptmode_undo();
void sculptmode_redo();
void sculptmode_undo_menu();

/* Interface */
void sculptmode_draw_interface_tools(struct uiBlock *block,unsigned short cx, unsigned short cy);
void sculptmode_draw_interface_textures(struct uiBlock *block,unsigned short cx, unsigned short cy);
void sculptmode_rem_tex(void*,void*);
void sculptmode_propset(const unsigned short event);
void sculptmode_selectbrush_menu();
void sculptmode_draw_mesh();

struct BrushData *sculptmode_brush();

void sculptmode_update_tex();
void sculpt();
void set_sculpt_object(struct Object *ob);
void set_sculptmode();

/* Partial Mesh Visibility */
void sculptmode_revert_pmv(struct Mesh *me);
void sculptmode_pmv_off(struct Mesh *me);
void sculptmode_pmv(int mode);

#endif
