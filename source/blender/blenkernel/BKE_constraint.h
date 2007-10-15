/**
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
 * Contributor(s): 2007 - Joshua Leung (major recode)
 *
 * ***** END GPL/BL DUAL LICENSE BLOCK *****
 */

#ifndef BKE_CONSTRAINT_H
#define BKE_CONSTRAINT_H


struct bConstraint;
struct ListBase;
struct Object;
struct bConstraintChannel;
struct bPoseChannel;
struct bAction;
struct bArmature;

/* ---------------------------------------------------------------------------- */

/* Constraint target/owner types */
#define TARGET_OBJECT			1	/*	string is ""				*/
#define TARGET_BONE				2	/*	string is bone-name		*/
#define TARGET_VERT				3	/*	string is vertex-group name 	*/
#define TARGET_CV				4 	/* 	string is vertex-group name - is not available until curves get vgroups */

/* ---------------------------------------------------------------------------- */

/* special struct for use in constraint evaluation */
typedef struct bConstraintOb {
	struct Object *ob;			/* if pchan, then armature that it comes from, otherwise constraint owner */
	struct bPoseChannel *pchan;	/* pose channel that owns the constraints being evaluated */
	
	float matrix[4][4];			/* matrix where constraints are accumulated + solved */
	float startmat[4][4];		/* original matrix (before constraint solving) */
	
	short type;					/* type of owner  */
} bConstraintOb;

/* ---------------------------------------------------------------------------- */

/* Constraint function prototypes */
void unique_constraint_name(struct bConstraint *con, struct ListBase *list);
void *new_constraint_data(short type);
void free_constraints(struct ListBase *conlist);
void copy_constraints(struct ListBase *dst, struct ListBase *src);
void relink_constraints(struct ListBase *list);
void free_constraint_data(struct bConstraint *con);

/* Constraint Channel function prototypes */
struct bConstraintChannel *get_constraint_channel(ListBase *list, const char *name);
struct bConstraintChannel *verify_constraint_channel(ListBase *list, const char *name);
void do_constraint_channels(struct ListBase *conbase, struct ListBase *chanbase, float ctime, int onlydrivers);
void copy_constraint_channels(ListBase *dst, ListBase *src);
void clone_constraint_channels(struct ListBase *dst, struct ListBase *src);
void free_constraint_channels(ListBase *chanbase);

/* Target function prototypes  */
char constraint_has_target(struct bConstraint *con);
struct Object *get_constraint_target(struct bConstraint *con, char **subtarget);
void set_constraint_target(struct bConstraint *con, struct Object *ob, char *subtarget);

/* Constraint Evaluation function prototypes */
struct bConstraintOb *constraints_make_evalob(struct Object *ob, void *subdata, short datatype);
void constraints_clear_evalob(struct bConstraintOb *cob);

void constraint_mat_convertspace(struct Object *ob, struct bPoseChannel *pchan, float mat[][4], short from, short to);

short get_constraint_target_matrix(struct bConstraint *con, short ownertype, void *ownerdata, float mat[][4], float time);
void solve_constraints (struct ListBase *conlist, struct bConstraintOb *cob, float ctime);


#endif

