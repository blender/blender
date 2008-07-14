/**
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
 * along with this program; if not, write to the Free Software Foundation,
 * Inc., 59 Temple Place - Suite 330, Boston, MA  02111-1307, USA.
 *
 * The Original Code is Copyright (C) 2001-2002 by NaN Holding BV.
 * All rights reserved.
 *
 * The Original Code is: all of this file.
 *
 * Contributor(s): none yet.
 *
 * ***** END GPL LICENSE BLOCK *****
 */

#ifndef BDR_DRAWOBJECT_H
#define BDR_DRAWOBJECT_H

#ifdef __cplusplus
extern "C" { 
#endif


struct Object;
struct Nurb;
struct Lamp;
struct ListBase;
struct BoundBox;
struct Base;
struct BPoint;
struct BezTriple;
struct EditVert;
struct EditFace;
struct EditEdge;

int set_gl_material(int nr);
int init_gl_materials(struct Object *ob, int check_alpha);

void mesh_foreachScreenVert(void (*func)(void *userData, struct EditVert *eve, int x, int y, int index), void *userData, int clipVerts);
void mesh_foreachScreenEdge(void (*func)(void *userData, struct EditEdge *eed, int x0, int y0, int x1, int y1, int index), void *userData, int clipVerts);
void mesh_foreachScreenFace(void (*func)(void *userData, struct EditFace *efa, int x, int y, int index), void *userData);

void lattice_foreachScreenVert(void (*func)(void *userData, struct BPoint *bp, int x, int y), void *userData);
void nurbs_foreachScreenVert(void (*func)(void *userData, struct Nurb *nu, struct BPoint *bp, struct BezTriple *bezt, int beztindex, int x, int y), void *userData);

void drawcircball(int mode, float *cent, float rad, float tmat[][4]);
void get_local_bounds(struct Object *ob, float *center, float *size);

/* drawing flags: */
#define DRAW_PICKING	1
#define DRAW_CONSTCOLOR	2
#define DRAW_SCENESET	4
void draw_object(struct Base *base, int flag);
void drawaxes(float size, int flag, char drawtype);

void draw_object_ext(struct Base *base);
void drawsolidcube(float size);
extern void draw_object_backbufsel(struct Object *ob);
void draw_object_instance(struct Object *ob, int dt, int outline);

#ifdef __cplusplus
}
#endif

#endif  /*  BDR_DRAWOBJECT_H */

