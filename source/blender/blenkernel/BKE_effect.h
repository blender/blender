/**
 * blenlib/BKE_effect.h (mar-2001 nzc)
 *	
 * $Id$ 
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
#ifndef BKE_EFFECT_H
#define BKE_EFFECT_H

struct Effect;
struct ListBase;
struct Object;
struct PartEff;
struct MTex;
struct Mesh;
struct WaveEff;
struct Particle;

struct Effect *add_effect(int type);
void free_effect(struct Effect *eff);
void free_effects(struct ListBase *lb);
struct Effect *copy_effect(struct Effect *eff);
void copy_act_effect(struct Object *ob);
void copy_effects(struct ListBase *lbn, struct ListBase *lb);
void deselectall_eff(struct Object *ob);
void set_buildvars(struct Object *ob, int *start, int *end);
struct Particle *new_particle(struct PartEff *paf);
struct PartEff *give_parteff(struct Object *ob);
void where_is_particle(struct PartEff *paf, struct Particle *pa, float ctime, float *vec);
void particle_tex(struct MTex *mtex, struct PartEff *paf, float *co, float *no);
void make_particle_keys(int depth, int nr, struct PartEff *paf, struct Particle *part, float *force, int deform, struct MTex *mtex, unsigned int par_layer);
void init_mv_jit(float *jit, int num,float seed2);
void give_mesh_mvert(struct Mesh *me, int nr, float *co, short *no,float seed2);
void build_particle_system(struct Object *ob);
void calc_wave_deform(struct WaveEff *wav, float ctime, float *co);
void object_wave(struct Object *ob);                            

#endif

