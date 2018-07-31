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
#ifndef __BKE_ARMATURE_H__
#define __BKE_ARMATURE_H__

/** \file BKE_armature.h
 *  \ingroup bke
 *  \since March 2001
 *  \author nzc
 */

struct bPose;
struct Bone;
struct Depsgraph;
struct GHash;
struct Main;
struct bArmature;
struct bPoseChannel;
struct bConstraint;
struct Scene;
struct Object;
struct PoseTree;
struct ListBase;

typedef struct PoseTarget {
	struct PoseTarget *next, *prev;

	struct bConstraint *con;        /* the constrait of this target */
	int tip;                        /* index of tip pchan in PoseTree */
} PoseTarget;

typedef struct PoseTree {
	struct PoseTree *next, *prev;

	int type;                       /* type of IK that this serves (CONSTRAINT_TYPE_KINEMATIC or ..._SPLINEIK) */
	int totchannel;                 /* number of pose channels */

	struct ListBase targets;        /* list of targets of the tree */
	struct bPoseChannel **pchan;    /* array of pose channels */
	int     *parent;                /* and their parents */

	float (*basis_change)[3][3];    /* basis change result from solver */
	int iterations;                 /* iterations from the constraint */
	int stretch;                    /* disable stretching */
} PoseTree;

/*	Core armature functionality */
#ifdef __cplusplus
extern "C" {
#endif

struct bArmature *BKE_armature_add(struct Main *bmain, const char *name);
struct bArmature *BKE_armature_from_object(struct Object *ob);
int  BKE_armature_bonelist_count(struct ListBase *lb);
void BKE_armature_bonelist_free(struct ListBase *lb);
void BKE_armature_free(struct bArmature *arm);
void BKE_armature_make_local(struct Main *bmain, struct bArmature *arm, const bool lib_local);
void BKE_armature_copy_data(struct Main *bmain, struct bArmature *arm_dst, const struct bArmature *arm_src, const int flag);
struct bArmature *BKE_armature_copy(struct Main *bmain, const struct bArmature *arm);

/* Bounding box. */
struct BoundBox *BKE_armature_boundbox_get(struct Object *ob);

bool BKE_pose_minmax(struct Object *ob, float r_min[3], float r_max[3], bool use_hidden, bool use_select);

int bone_autoside_name(char name[64], int strip_number, short axis, float head, float tail);

struct Bone  *BKE_armature_find_bone_name(struct bArmature *arm, const char *name);
struct GHash *BKE_armature_bone_from_name_map(struct bArmature *arm);

bool         BKE_armature_bone_flag_test_recursive(const struct Bone *bone, int flag);

float distfactor_to_bone(const float vec[3], const float b1[3], const float b2[3], float r1, float r2, float rdist);

void BKE_armature_where_is(struct bArmature *arm);
void BKE_armature_where_is_bone(struct Bone *bone, struct Bone *prevbone, const bool use_recursion);
void BKE_pose_clear_pointers(struct bPose *pose);
void BKE_pose_remap_bone_pointers(struct bArmature *armature, struct bPose *pose);
void BKE_pose_rebuild(struct Main *bmain, struct Object *ob, struct bArmature *arm, const bool do_id_user);
void BKE_pose_where_is(struct Depsgraph *depsgraph, struct Scene *scene, struct Object *ob);
void BKE_pose_where_is_bone(struct Depsgraph *depsgraph, struct Scene *scene, struct Object *ob, struct bPoseChannel *pchan, float ctime, bool do_extra);
void BKE_pose_where_is_bone_tail(struct bPoseChannel *pchan);

/* get_objectspace_bone_matrix has to be removed still */
void get_objectspace_bone_matrix(struct Bone *bone, float M_accumulatedMatrix[4][4], int root, int posed);
void vec_roll_to_mat3(const float vec[3], const float roll, float mat[3][3]);
void vec_roll_to_mat3_normalized(const float nor[3], const float roll, float mat[3][3]);
void mat3_to_vec_roll(float mat[3][3], float r_vec[3], float *r_roll);

/* Common Conversions Between Co-ordinate Spaces */
void BKE_armature_mat_world_to_pose(struct Object *ob, float inmat[4][4], float outmat[4][4]);
void BKE_armature_loc_world_to_pose(struct Object *ob, const float inloc[3], float outloc[3]);
void BKE_armature_mat_pose_to_bone(struct bPoseChannel *pchan, float inmat[4][4], float outmat[4][4]);
void BKE_armature_loc_pose_to_bone(struct bPoseChannel *pchan, const float inloc[3], float outloc[3]);
void BKE_armature_mat_bone_to_pose(struct bPoseChannel *pchan, float inmat[4][4], float outmat[4][4]);
void BKE_armature_mat_pose_to_delta(float delta_mat[4][4], float pose_mat[4][4], float arm_mat[4][4]);

void BKE_armature_mat_pose_to_bone_ex(struct Depsgraph *depsgraph, struct Object *ob, struct bPoseChannel *pchan, float inmat[4][4], float outmat[4][4]);

void BKE_pchan_mat3_to_rot(struct bPoseChannel *pchan, float mat[3][3], bool use_compat);
void BKE_pchan_apply_mat4(struct bPoseChannel *pchan, float mat[4][4], bool use_comat);
void BKE_pchan_to_mat4(struct bPoseChannel *pchan, float chan_mat[4][4]);
void BKE_pchan_calc_mat(struct bPoseChannel *pchan);

/* Get the "pchan to pose" transform matrix. These matrices apply the effects of
 * HINGE/NO_SCALE/NO_LOCAL_LOCATION options over the pchan loc/rot/scale transformations. */
void BKE_pchan_to_pose_mat(struct bPoseChannel *pchan, float rotscale_mat[4][4], float loc_mat[4][4]);

/* Rotation Mode Conversions - Used for PoseChannels + Objects... */
void BKE_rotMode_change_values(float quat[4], float eul[3], float axis[3], float *angle, short oldMode, short newMode);

/* B-Bone support */
#define MAX_BBONE_SUBDIV    32

typedef struct Mat4 {
	float mat[4][4];
} Mat4;

void equalize_bbone_bezier(float *data, int desired);
void b_bone_spline_setup(struct bPoseChannel *pchan, int rest, Mat4 result_array[MAX_BBONE_SUBDIV]);

/* like EBONE_VISIBLE */
#define PBONE_VISIBLE(arm, bone) ( \
	CHECK_TYPE_INLINE(arm, bArmature *), \
	CHECK_TYPE_INLINE(bone, Bone *), \
	(((bone)->layer & (arm)->layer) && !((bone)->flag & BONE_HIDDEN_P)) \
	)

#define PBONE_SELECTABLE(arm, bone) \
	(PBONE_VISIBLE(arm, bone) && !((bone)->flag & BONE_UNSELECTABLE))


/* context.selected_pose_bones */
#define FOREACH_PCHAN_SELECTED_IN_OBJECT_BEGIN(_ob, _pchan) \
	for (bPoseChannel *_pchan = (_ob)->pose->chanbase.first; _pchan;  _pchan = _pchan->next) { \
		if (PBONE_VISIBLE(((bArmature *)(_ob)->data), (_pchan)->bone) && ((_pchan)->bone->flag & BONE_SELECTED)) {
#define FOREACH_PCHAN_SELECTED_IN_OBJECT_END \
		} \
	} ((void)0)
/* context.visible_pose_bones */
#define FOREACH_PCHAN_VISIBLE_IN_OBJECT_BEGIN(_ob, _pchan) \
	for (bPoseChannel *_pchan = (_ob)->pose->chanbase.first; _pchan;  _pchan = _pchan->next) { \
		if (PBONE_VISIBLE(((bArmature *)(_ob)->data), (_pchan)->bone)) {
#define FOREACH_PCHAN_VISIBLE_IN_OBJECT_END \
		} \
	} ((void)0)


/* Evaluation helpers */
struct bKinematicConstraint;
struct bPose;
struct bSplineIKConstraint;

struct bPoseChannel *BKE_armature_ik_solver_find_root(
        struct bPoseChannel *pchan,
        struct bKinematicConstraint *data);
struct bPoseChannel *BKE_armature_splineik_solver_find_root(
        struct bPoseChannel *pchan,
        struct bSplineIKConstraint *data);

void BKE_pose_splineik_init_tree(struct Scene *scene, struct Object *ob, float ctime);
void BKE_splineik_execute_tree(
        struct Depsgraph *depsgraph, struct Scene *scene,
        struct Object *ob, struct bPoseChannel *pchan_root, float ctime);

void BKE_pose_eval_init(
        struct Depsgraph *depsgraph,
        struct Scene *scene,
        struct Object *ob);

void BKE_pose_eval_init_ik(
        struct Depsgraph *depsgraph,
        struct Scene *scene,
        struct Object *ob);

void BKE_pose_eval_bone(
        struct Depsgraph *depsgraph,
        struct Scene *scene,
        struct Object *ob,
        int pchan_index);

void BKE_pose_constraints_evaluate(
        struct Depsgraph *depsgraph,
        struct Scene *scene,
        struct Object *ob,
        int pchan_index);

void BKE_pose_bone_done(
        struct Depsgraph *depsgraph,
        struct Object *ob,
        int pchan_index);

void BKE_pose_iktree_evaluate(
        struct Depsgraph *depsgraph,
        struct Scene *scene,
        struct Object *ob,
        int rootchan_index);

void BKE_pose_splineik_evaluate(
        struct Depsgraph *depsgraph,
        struct Scene *scene,
        struct Object *ob,
        int rootchan_index);

void BKE_pose_eval_flush(
        struct Depsgraph *depsgraph,
        struct Scene *scene,
        struct Object *ob);

void BKE_pose_eval_proxy_copy(
        struct Depsgraph *depsgraph,
        struct Object *ob);

#ifdef __cplusplus
}
#endif

#endif
