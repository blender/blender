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

void BKE_object_workob_clear(struct Object *workob);
void BKE_object_workob_calc_parent(struct Scene *scene, struct Object *ob, struct Object *workob);

struct SoftBody *copy_softbody(struct SoftBody *sb);
struct BulletSoftBody *copy_bulletsoftbody(struct BulletSoftBody *sb);
void BKE_object_copy_particlesystems(struct Object *obn, struct Object *ob);
void BKE_object_copy_softbody(struct Object *obn, struct Object *ob);
void BKE_object_free_particlesystems(struct Object *ob);
void BKE_object_free_softbody(struct Object *ob);
void BKE_object_free_bulletsoftbody(struct Object *ob);
void BKE_object_update_base_layer(struct Scene *scene, struct Object *ob);

void BKE_object_free(struct Object *ob);
void BKE_object_free_display(struct Object *ob);

int  BKE_object_support_modifier_type_check(struct Object *ob, int modifier_type);

void BKE_object_link_modifiers(struct Object *ob, struct Object *from);
void BKE_object_free_modifiers(struct Object *ob);

void BKE_object_make_proxy(struct Object *ob, struct Object *target, struct Object *gob);
void BKE_object_copy_proxy_drivers(struct Object *ob, struct Object *target);

void BKE_object_unlink(struct Object *ob);
int  BKE_object_exists_check(struct Object *obtest);
	
struct Object *BKE_object_add_only_object(int type, const char *name);
struct Object *BKE_object_add(struct Scene *scene, int type);
void *BKE_object_obdata_add_from_type(int type);

struct Object *BKE_object_copy(struct Object *ob);
void BKE_object_make_local(struct Object *ob);
int  BKE_object_is_libdata(struct Object *ob);
int  BKE_object_obdata_is_libdata(struct Object *ob);

void BKE_object_scale_to_mat3(struct Object *ob, float mat[][3]);
void BKE_object_rot_to_mat3(struct Object *ob, float mat[][3]);
void BKE_object_mat3_to_rot(struct Object *ob, float mat[][3], short use_compat);
void BKE_object_to_mat3(struct Object *ob, float mat[][3]);
void BKE_object_to_mat4(struct Object *ob, float mat[][4]);
void BKE_object_apply_mat4(struct Object *ob, float mat[][4], const short use_compat, const short use_parent);

struct Object *BKE_object_pose_armature_get(struct Object *ob);

void BKE_object_where_is_calc(struct Scene *scene, struct Object *ob);
void BKE_object_where_is_calc_time(struct Scene *scene, struct Object *ob, float ctime);
void BKE_object_where_is_calc_simul(struct Scene *scene, struct Object *ob);
void BKE_object_where_is_calc_mat4(struct Scene *scene, struct Object *ob, float obmat[4][4]);

/* possibly belong in own moduke? */
struct BoundBox *BKE_boundbox_alloc_unit(void);
void             BKE_boundbox_init_from_minmax(struct BoundBox *bb, float min[3], float max[3]);
int              BKE_boundbox_ray_hit_check(struct BoundBox *bb, float ray_start[3], float ray_normal[3]);

struct BoundBox *BKE_object_boundbox_get(struct Object *ob);
void BKE_object_dimensions_get(struct Object *ob, float vec[3]);
void BKE_object_dimensions_set(struct Object *ob, const float *value);
void BKE_object_boundbox_flag(struct Object *ob, int flag, int set);
void BKE_object_minmax(struct Object *ob, float r_min[3], float r_max[3]);
int  BKE_object_minmax_dupli(struct Scene *scene, struct Object *ob, float r_min[3], float r_max[3]);

/* sometimes min-max isn't enough, we need to loop over each point */
void BKE_object_foreach_display_point(
        struct Object *ob, float obmat[4][4],
        void (*func_cb)(const float[3], void *), void *user_data);
void BKE_scene_foreach_display_point(
        struct Scene *scene,
        struct View3D *v3d,
        const short flag,
        void (*func_cb)(const float[3], void *), void *user_data);

int BKE_object_parent_loop_check(const struct Object *parent, const struct Object *ob);

void *BKE_object_tfm_backup(struct Object *ob);
void  BKE_object_tfm_restore(struct Object *ob, void *obtfm_pt);

typedef struct ObjectTfmProtectedChannels {
	float loc[3],     dloc[3];
	float size[3],    dscale[3];
	float rot[3],     drot[3];
	float quat[4],    dquat[4];
	float rotAxis[3], drotAxis[3];
	float rotAngle,   drotAngle;
} ObjectTfmProtectedChannels;

void BKE_object_tfm_protected_backup(const struct Object *ob,
									 ObjectTfmProtectedChannels *obtfm);

void BKE_object_tfm_protected_restore(struct Object *ob,
									  const ObjectTfmProtectedChannels *obtfm,
									  const short protectflag);

void BKE_object_handle_update(struct Scene *scene, struct Object *ob);
void BKE_object_sculpt_modifiers_changed(struct Object *ob);

int BKE_object_obdata_texspace_get(struct Object *ob, short **r_texflag, float **r_loc, float **r_size, float **r_rot);

int BKE_object_insert_ptcache(struct Object *ob);
// void object_delete_ptcache(struct Object *ob, int index);
struct KeyBlock *BKE_object_insert_shape_key(struct Scene *scene, struct Object *ob, const char *name, int from_mix);

int BKE_object_is_modified(struct Scene *scene, struct Object *ob);
int BKE_object_is_deform_modified(struct Scene *scene, struct Object *ob);

void BKE_object_relink(struct Object *ob);

struct MovieClip *BKE_object_movieclip_get(struct Scene *scene, struct Object *ob, int use_default);

#ifdef __cplusplus
}
#endif

#endif
