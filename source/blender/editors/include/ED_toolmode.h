#if 0
/**
 * $Id:
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
 * along with this program; if not, write to the Free Software Foundation,
 * Inc., 51 Franklin Street, Fifth Floor, Boston, MA 02110-1301, USA.
 *
 * The Original Code is Copyright (C) 2008 Blender Foundation.
 * All rights reserved.
 *
 * Contributor(s): Joseph Eagar
 *
 * ***** END GPL LICENSE BLOCK *****
 */
#ifndef ED_TOOLMODE_H
#define ED_TOOLMODE_H

struct ID;
struct View3D;
struct ARegion;
struct bContext;
struct wmWindowManager;
struct wmKeyConfig;
struct ReportList;
struct ViewContext;
struct bDeformGroup;
struct MDeformWeight;
struct MDeformVert;
struct Scene;
struct Mesh;
struct MCol;
struct UvVertMap;
struct UvMapVert;
struct CustomData;
struct BMEditMesh;
struct BMEditSelection;
struct BMesh;
struct BMVert;
struct BMEdge;
struct BMFace;
struct UvVertMap;
struct UvMapVert;
struct Material;
struct Object;
struct rcti;
struct wmOperator;

typedef struct ToolModeDefine {
	short idtype, icon;
	char *name;
	void (*create)(void *args);
	void (*free)(void *self);
	
	/*called when mode is set active*/
	void (*enter)(void *self, struct bContext *C);
	void (*exit)(void *self, struct bContext *C);
	
	/*called on draw*/
	void (*draw)(void *self, struct bContext *C);
	/*modal is option, and should be used carefully*/
	void (*modal)(struct bContext *C, struct wmOperator *op, struct wmEvent *event);
	
	/*keymap stuff*/	
	void (*create_keymap)(struct wmKeyConfig *km);
	void (*keymap_poll)(struct bContext *C);
};
#endif
