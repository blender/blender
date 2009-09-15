/**
 * blenlib/BKE_key.h (mar-2001 nzc)
 *	
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
#ifndef BKE_KEY_H
#define BKE_KEY_H

struct Key;
struct KeyBlock;
struct ID;
struct ListBase;
struct Curve;
struct Object;
struct Scene;
struct Lattice;
struct Mesh;

/* Kernel prototypes */
#ifdef __cplusplus
extern "C" {
#endif

void free_key(struct Key *sc); 
struct Key *add_key(struct ID *id);
struct Key *copy_key(struct Key *key);
void make_local_key(struct Key *key);
void sort_keys(struct Key *key);

void key_curve_position_weights(float t, float *data, int type);
void key_curve_tangent_weights(float t, float *data, int type);
void key_curve_normal_weights(float t, float *data, int type);

/* only exported to curve.c! */
void cp_cu_key(struct Curve *cu, struct KeyBlock *kb, int start, int end);

int do_ob_key(struct Scene *scene, struct Object *ob);

struct Key *ob_get_key(struct Object *ob);
struct KeyBlock *ob_get_keyblock(struct Object *ob);
struct KeyBlock *key_get_keyblock(struct Key *key, int index);
struct KeyBlock *key_get_named_keyblock(struct Key *key, const char name[]);
// needed for the GE
void do_rel_key(int start, int end, int tot, char *basispoin, struct Key *key, int mode);

#ifdef __cplusplus
};
#endif

#endif

