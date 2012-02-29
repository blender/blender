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

#ifndef __BKE_OBJECT_H__
#define __BKE_OBJECT_H__

/** \file BKE_object.h
 *  \ingroup bke
 *  \brief General operations, lookup, etc. for blender objects.
 */
#ifdef __cplusplus
extern "C" {
#endif

struct Base;
struct Scene;
struct Object;
struct Camera;
struct BoundBox;
struct View3D;
struct SoftBody;
struct BulletSoftBody;
struct Group;
struct bAction;
struct RenderData;
struct rctf;
struct MovieClip;

void clear_workob(struct Object *workob);
void what_does_parent(struct Scene *scene, struct Object *ob, struct Object *workob);

void copy_baseflags(struct Scene *scene);
void copy_objectflags(struct Scene *scene);
struct SoftBody *copy_softbody(struct SoftBody *sb);
struct BulletSoftBody *copy_bulletsoftbody(struct BulletSoftBody *sb);
void copy_object_particlesystems(struct Object *obn, struct Object *ob);
void copy_object_softbody(struct Object *obn, struct Object *ob);
void object_free_particlesystems(struct Object *ob);
void object_free_softbody(struct Object *ob);
void object_free_bulletsoftbody(struct Object *ob);
void update_base_layer(struct Scene *scene, struct Object *ob);

void free_object(struct Object *ob);
void object_free_display(struct Object *ob);

void object_link_modifiers(struct Object *ob, struct Object *from);
void object_free_modifiers(struct Object *ob);

void object_make_proxy(struct Object *ob, struct Object *target, struct Object *gob);
void object_copy_proxy_drivers(struct Object *ob, struct Object *target);

void unlink_object(struct Object *ob);
int exist_object(struct Object *obtest);
	
struct Object *add_only_object(int type, const char *name);
struct Object *add_object(struct Scene *scene, int type);

struct Object *copy_object(struct Object *ob);
void make_local_object(struct Object *ob);
int object_is_libdata(struct Object *ob);
int object_data_is_libdata(struct Object *ob);

void object_scale_to_mat3(struct Object *ob, float mat[][3]);
void object_rot_to_mat3(struct Object *ob, float mat[][3]);
void object_mat3_to_rot(struct Object *ob, float mat[][3], short use_compat);
void object_to_mat3(struct Object *ob, float mat[][3]);
void object_to_mat4(struct Object *ob, float mat[][4]);
void object_apply_mat4(struct Object *ob, float mat[][4], const short use_compat, const short use_parent);

struct Object *object_pose_armature_get(struct Object *ob);

void where_is_object_time(struct Scene *scene, struct Object *ob, float ctime);
void where_is_object(struct Scene *scene, struct Object *ob);
void where_is_object_simul(struct Scene *scene, struct Object *ob);
void where_is_object_mat(struct Scene *scene, struct Object *ob, float obmat[4][4]);

struct BoundBox *unit_boundbox(void);
void boundbox_set_from_min_max(struct BoundBox *bb, float min[3], float max[3]);
struct BoundBox *object_get_boundbox(struct Object *ob);
void object_get_dimensions(struct Object *ob, float vec[3]);
void object_set_dimensions(struct Object *ob, const float *value);
void object_boundbox_flag(struct Object *ob, int flag, int set);
void minmax_object(struct Object *ob, float min[3], float max[3]);
int minmax_object_duplis(struct Scene *scene, struct Object *ob, float min[3], float max[3]);

/* sometimes min-max isnt enough, we need to loop over each point */
void BKE_object_foreach_display_point(
        struct Object *ob, float obmat[4][4],
        void (*func_cb)(const float[3], void *), void *user_data);
void BKE_scene_foreach_display_point(
        struct Scene *scene,
        struct View3D *v3d,
        const short flag,
        void (*func_cb)(const float[3], void *), void *user_data);

int BKE_object_parent_loop_check(const struct Object *parent, const struct Object *ob);

int ray_hit_boundbox(struct BoundBox *bb, float ray_start[3], float ray_normal[3]);

void *object_tfm_backup(struct Object *ob);
void object_tfm_restore(struct Object *ob, void *obtfm_pt);

typedef struct ObjectTfmProtectedChannels {
	float loc[3],     dloc[3];
	float size[3],    dscale[3];
	float rot[3],     drot[3];
	float quat[4],    dquat[4];
	float rotAxis[3], drotAxis[3];
	float rotAngle,   drotAngle;
} ObjectTfmProtectedChannels;

void object_tfm_protected_backup(const struct Object *ob,
                                 ObjectTfmProtectedChannels *obtfm);

void object_tfm_protected_restore(struct Object *ob,
                                  const ObjectTfmProtectedChannels *obtfm,
                                  const short protectflag);

void object_handle_update(struct Scene *scene, struct Object *ob);
void object_sculpt_modifiers_changed(struct Object *ob);

int give_obdata_texspace(struct Object *ob, short **texflag, float **loc, float **size, float **rot);

int object_insert_ptcache(struct Object *ob);
// void object_delete_ptcache(struct Object *ob, int index);
struct KeyBlock *object_insert_shape_key(struct Scene *scene, struct Object *ob, const char *name, int from_mix);

int object_is_modified(struct Scene *scene, struct Object *ob);

void object_relink(struct Object *ob);

struct MovieClip *object_get_movieclip(struct Scene *scene, struct Object *ob, int use_default);

#ifdef __cplusplus
}
#endif

#endif
