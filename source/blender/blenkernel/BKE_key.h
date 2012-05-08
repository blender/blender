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
 * along with this program; if not, write to the Free Software Foundation,
 * Inc., 51 Franklin Street, Fifth Floor, Boston, MA 02110-1301, USA.
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
#ifndef __BKE_KEY_H__
#define __BKE_KEY_H__

/** \file BKE_key.h
 *  \ingroup bke
 *  \since March 2001
 *  \author nzc
 */
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
void free_key_nolib(struct Key *key);
struct Key *add_key(struct ID *id);
struct Key *copy_key(struct Key *key);
struct Key *copy_key_nolib(struct Key *key);
void make_local_key(struct Key *key);
void sort_keys(struct Key *key);

void key_curve_position_weights(float t, float *data, int type);
void key_curve_tangent_weights(float t, float *data, int type);
void key_curve_normal_weights(float t, float *data, int type);

float *do_ob_key(struct Scene *scene, struct Object *ob);

struct Key *ob_get_key(struct Object *ob);
struct KeyBlock *add_keyblock(struct Key *key, const char *name);
struct KeyBlock *add_keyblock_ctime(struct Key *key, const char * name, const short do_force);
struct KeyBlock *ob_get_keyblock(struct Object *ob);
struct KeyBlock *ob_get_reference_keyblock(struct Object *ob);
struct KeyBlock *key_get_keyblock(struct Key *key, int index);
struct KeyBlock *key_get_named_keyblock(struct Key *key, const char name[]);
char *key_get_curValue_rnaPath(struct Key *key, struct KeyBlock *kb);
// needed for the GE
void do_rel_key(const int start, int end, const int tot, char *basispoin, struct Key *key, struct KeyBlock *actkb, const int mode);

/* conversion functions */
void key_to_mesh(struct KeyBlock *kb, struct Mesh *me);
void mesh_to_key(struct Mesh *me, struct KeyBlock *kb);
void key_to_latt(struct KeyBlock *kb, struct Lattice *lt);
void latt_to_key(struct Lattice *lt, struct KeyBlock *kb);
void key_to_curve(struct KeyBlock *kb, struct Curve  *cu, struct ListBase *nurb);
void curve_to_key(struct Curve *cu, struct KeyBlock *kb, struct ListBase *nurb);
float (*key_to_vertcos(struct Object *ob, struct KeyBlock *kb))[3];
void vertcos_to_key(struct Object *ob, struct KeyBlock *kb, float (*vertCos)[3]);
void offset_to_key(struct Object *ob, struct KeyBlock *kb, float (*ofs)[3]);

/* key.c */
extern int slurph_opt;

#ifdef __cplusplus
};
#endif

#endif // __BKE_KEY_H__
