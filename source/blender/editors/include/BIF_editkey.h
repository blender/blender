/**
 * $Id: BIF_editkey.h 10579 2007-04-25 11:57:02Z aligorith $
 *
 * ***** BEGIN GPL/BL DUAL LICENSE BLOCK *****
 *
 * This program is free software; you can redistribute it and/or
 * modify it under the terms of the GNU General Public License
 * as published by the Free Software Foundation; either version 2
 * of the License, or (at your option) any later version. The Blender
 * Foundation also sells licenses for use in proprietary software under
 * the Blender License.  See http://www.blender.org/BL/ for information
 * about this.
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
 * ***** END GPL/BL DUAL LICENSE BLOCK *****
 */

#ifndef BIF_EDITKEY_H
#define BIF_EDITKEY_H

struct Key;
struct KeyBlock;
struct Mesh;
struct Object;
struct Lattice;
struct Curve;
struct uiBlock;
struct BezTriple;
struct IpoCurve;

void mesh_to_key(struct Mesh *me, struct KeyBlock *kb);
void key_to_mesh(struct KeyBlock *kb, struct Mesh *me);
void insert_meshkey(struct Mesh *me, short rel);

void latt_to_key(struct Lattice *lt, struct KeyBlock *kb);
void key_to_latt(struct KeyBlock *kb, struct Lattice *lt);
void insert_lattkey(struct Lattice *lt, short rel);

void curve_to_key(struct Curve *cu, struct KeyBlock *kb, ListBase *nurb);
void key_to_curve(struct KeyBlock *kb, struct Curve  *cu, ListBase *nurb);
void insert_curvekey(struct Curve *cu, short rel);

void insert_shapekey(struct Object *ob);

void delete_key(struct Object *ob);
void move_keys(struct Object *ob);

void make_rvk_slider(struct uiBlock *block, struct Object *ob, int keynum,
							int x, int y, int w, int h, char *tip);
							
// FIXME: move me somewhere else 
struct BezTriple *get_bezt_icu_time(struct IpoCurve *icu, float *frame, float *val); 
							
#endif

