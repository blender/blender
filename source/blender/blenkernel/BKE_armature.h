/**
 * blenlib/BKE_armature.h (mar-2001 nzc)
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
#ifndef BKE_ARMATURE_H
#define BKE_ARMATURE_H

struct Bone;
struct Main;
struct bArmature;
struct bPose;
struct bPoseChannel;
struct bConstraint;
struct Object;
struct MDeformVert;
struct Mesh;
struct PoseChain;
struct ListBase;

typedef struct PoseChain
{
	struct PoseChain *next, *prev;  // hurms
	struct bPoseChannel	**pchanchain;
	struct bConstraint *con;
	struct Bone *root, *target;
	struct bPose *pose;
	int totchannel;
	float	goal[3];
	float	tolerance;
	int		iterations;
	float	goalinv[4][4];
	struct IK_Chain_Extern *solver;
} PoseChain;

/*	Core armature functionality */
#ifdef __cplusplus
extern "C" {
#endif
struct bArmature *add_armature(void);
void free_boneChildren(struct Bone *bone);
void free_bones (struct bArmature *arm);
void unlink_armature(struct bArmature *arm);
void free_armature(struct bArmature *arm);
void make_local_armature(struct bArmature *arm);
struct bArmature *copy_armature(struct bArmature *arm);

void calc_armature_deform (struct Object *ob, float *co, int index);

void init_armature_deform(struct Object *parent, struct Object *ob);
struct bArmature* get_armature (struct Object* ob);
struct Bone *get_named_bone (struct bArmature *arm, const char *name);
struct Bone *get_indexed_bone (struct Object *ob, int index);

float dist_to_bone (float vec[3], float b1[3], float b2[3]);

void where_is_armature (struct bArmature *arm);
void where_is_armature_bone(struct Bone *bone, struct Bone *prevbone);
void armature_rebuild_pose(struct Object *ob, struct bArmature *arm);
void where_is_pose (struct Object *ob);

/* get_objectspace_bone_matrix has to be removed still */
void get_objectspace_bone_matrix (struct Bone* bone, float M_accumulatedMatrix[][4], int root, int posed);
void vec_roll_to_mat3(float *vec, float roll, float mat[][3]);


/*	Animation functions */

struct PoseChain *ik_chain_to_posechain (struct Object *ob, struct Bone *bone);
void solve_posechain (PoseChain *chain);
void free_posechain (PoseChain *chain);

/*	Gameblender hacks */
void GB_init_armature_deform(struct ListBase *defbase, float premat[][4], float postmat[][4]);
void GB_calc_armature_deform (float *co, struct MDeformVert *dvert);
void GB_build_mats (float parmat[][4], float obmat[][4], float premat[][4], float postmat[][4]);
void GB_validate_defgroups (struct Mesh *mesh, struct ListBase *defbase);

/* B-Bone support */
typedef struct Mat4 {
	float mat[4][4];
} Mat4;

Mat4 *b_bone_spline_setup(struct bPoseChannel *pchan);


#ifdef __cplusplus
}
#endif

#endif

