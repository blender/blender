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
 * General operations, lookup, etc. for blender objects.
 */

#ifndef BKE_OBJECT_H
#define BKE_OBJECT_H

#ifdef __cplusplus
extern "C" {
#endif

struct Base;
struct Object;
struct Camera;
struct BoundBox;
struct View3D;
struct SoftBody;
struct Group;
struct bAction;

void clear_workob(void);
void copy_baseflags(void);
void copy_objectflags(void);
struct SoftBody *copy_softbody(struct SoftBody *sb);
void copy_object_particlesystems(struct Object *obn, struct Object *ob);
void copy_object_softbody(struct Object *obn, struct Object *ob);
void object_free_particlesystems(struct Object *ob);
void object_free_softbody(struct Object *ob);
void update_base_layer(struct Object *ob);

void free_object(struct Object *ob);
void object_free_display(struct Object *ob);
void object_free_modifiers(struct Object *ob);

void object_make_proxy(struct Object *ob, struct Object *target, struct Object *gob);

void unlink_object(struct Object *ob);
int exist_object(struct Object *obtest);
void *add_camera(char *name);
struct Camera *copy_camera(struct Camera *cam);
void make_local_camera(struct Camera *cam);
float dof_camera(struct Object *ob);
void *add_lamp(char *name);
struct Lamp *copy_lamp(struct Lamp *la);
void make_local_lamp(struct Lamp *la);
void free_camera(struct Camera *ca);
void free_lamp(struct Lamp *la);
void *add_wave(void);

struct Object *add_only_object(int type, char *name);
struct Object *add_object(int type);
void base_init_from_view3d(struct Base *base, struct View3D *v3d);

struct Object *copy_object(struct Object *ob);
void expand_local_object(struct Object *ob);
void make_local_object(struct Object *ob);
void set_mblur_offs(float blur);
void set_field_offs(float field);
void disable_speed_curve(int val);

float bsystem_time(struct Object *ob, float cfra, float ofs);
void object_to_mat3(struct Object *ob, float mat[][3]);
void object_to_mat4(struct Object *ob, float mat[][4]);

void set_no_parent_ipo(int val);

void disable_where_script(short on);
int during_script(void);
void disable_where_scriptlink(short on);
int during_scriptlink(void);

void where_is_object_time(struct Object *ob, float ctime);
void where_is_object(struct Object *ob);
void where_is_object_simul(struct Object *ob);

void what_does_parent(struct Object *ob);

struct BoundBox *unit_boundbox(void);
void boundbox_set_from_min_max(struct BoundBox *bb, float min[3], float max[3]);
struct BoundBox *object_get_boundbox(struct Object *ob);
void object_boundbox_flag(struct Object *ob, int flag, int set);
void minmax_object(struct Object *ob, float *min, float *max);
void minmax_object_duplis(struct Object *ob, float *min, float *max);
void solve_tracking (struct Object *ob, float targetmat[][4]);
int ray_hit_boundbox(struct BoundBox *bb, float ray_start[3], float ray_normal[3]);

void object_handle_update(struct Object *ob);

float give_timeoffset(struct Object *ob);
int give_obdata_texspace(struct Object *ob, int **texflag, float **loc, float **size, float **rot);
#ifdef __cplusplus
}
#endif

#endif
