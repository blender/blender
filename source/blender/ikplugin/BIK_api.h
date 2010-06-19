/**
 * $Id$
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
 * Original author: Benoit Bolsee
 * Contributor(s): 
 *
 * ***** END GPL LICENSE BLOCK *****
 */

#ifndef BIK_API_H
#define BIK_API_H

#ifdef __cplusplus
extern "C" {
#endif

struct Object;
struct bPoseChannel;
struct bPose;
struct bArmature;
struct Scene;
struct bConstraint;

enum BIK_ParamType {
	BIK_PARAM_TYPE_FLOAT = 0,
	BIK_PARAM_TYPE_INT,
	BIK_PARAM_TYPE_STRING,
};

struct BIK_ParamValue {
	short type;			/* BIK_PARAM_TYPE_.. */
	short length;		/* for string, does not include terminating 0 */
	union {
		float f[8];
		int   i[8];
		char  s[32];
	} value;		
};
typedef struct BIK_ParamValue BIK_ParamValue;

void BIK_initialize_tree(struct Scene *scene, struct Object *ob, float ctime);
void BIK_execute_tree(struct Scene *scene, struct Object *ob, struct bPoseChannel *pchan, float ctime);
void BIK_release_tree(struct Scene *scene, struct Object *ob, float ctime);
void BIK_clear_data(struct bPose *pose);
void BIK_clear_cache(struct bPose *pose);
void BIK_update_param(struct bPose *pose);
void BIK_test_constraint(struct Object *ob, struct bConstraint *cons);
// not yet implemented
int BIK_get_constraint_param(struct bPose *pose, struct bConstraint *cons, int id, BIK_ParamValue *value);
int BIK_get_channel_param(struct bPose *pose, struct bPoseChannel *pchan, int id, BIK_ParamValue *value);
int BIK_get_solver_param(struct bPose *pose, struct bPoseChannel *pchan, int id, BIK_ParamValue *value);

// number of solver available
// 0 = iksolver
// 1 = iTaSC
#define BIK_SOLVER_COUNT		2

/* for use in BIK_get_constraint_param */
#define BIK_PARAM_CONSTRAINT_ERROR		0

/* for use in BIK_get_channel_param */
#define BIK_PARAM_CHANNEL_JOINT			0

/* for use in BIK_get_solver_param */
#define BIK_PARAM_SOLVER_RANK			0
#define BIK_PARAM_SOLVER_ITERATION		1

#ifdef __cplusplus
}
#endif

#endif // BIK_API_H

