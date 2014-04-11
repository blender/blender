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
 * Contributor(s): 2007 - Joshua Leung (major recode)
 *
 * ***** END GPL LICENSE BLOCK *****
 */

#ifndef __BKE_CONSTRAINT_H__
#define __BKE_CONSTRAINT_H__

/** \file BKE_constraint.h
 *  \ingroup bke
 *  \author Joshua Leung (major recode 2007)
 */

struct ID;
struct bConstraint;
struct bConstraintTarget;
struct ListBase;
struct Object;
struct Scene;
struct bPoseChannel;

/* ---------------------------------------------------------------------------- */
#ifdef __cplusplus
extern "C" {
#endif

/* special struct for use in constraint evaluation */
typedef struct bConstraintOb {
	struct Scene *scene;        /* for system time, part of deglobalization, code nicer later with local time (ton) */
	struct Object *ob;          /* if pchan, then armature that it comes from, otherwise constraint owner */
	struct bPoseChannel *pchan; /* pose channel that owns the constraints being evaluated */
	
	float matrix[4][4];         /* matrix where constraints are accumulated + solved */
	float startmat[4][4];       /* original matrix (before constraint solving) */
	
	short type;                 /* type of owner  */
	short rotOrder;             /* rotation order for constraint owner (as defined in eEulerRotationOrders in BLI_math.h) */
} bConstraintOb;

/* ---------------------------------------------------------------------------- */

/* Callback format for performing operations on ID-pointers for Constraints */
typedef void (*ConstraintIDFunc)(struct bConstraint *con, struct ID **idpoin, bool is_reference, void *userdata);

/* ....... */

/* Constraint Type-Info (shorthand in code = cti):
 *  This struct provides function pointers for runtime, so that functions can be
 *  written more generally (with fewer/no special exceptions for various constraints).
 *
 *  Callers of these functions must check that they actually point to something useful,
 *  as some constraints don't define some of these.
 *
 *  Warning: it is not too advisable to reorder order of members of this struct,
 *			as you'll have to edit quite a few ($NUM_CONSTRAINT_TYPES) of these
 *			structs.
 */
typedef struct bConstraintTypeInfo {
	/* admin/ident */
	short type;             /* CONSTRAINT_TYPE_### */
	short size;             /* size in bytes of the struct */
	char name[32];          /* name of constraint in interface */
	char structName[32];    /* name of struct for SDNA */
	
	/* data management function pointers - special handling */
	/* free any data that is allocated separately (optional) */
	void (*free_data)(struct bConstraint *con);
	/* run the provided callback function on all the ID-blocks linked to the constraint */
	void (*id_looper)(struct bConstraint *con, ConstraintIDFunc func, void *userdata);
	/* copy any special data that is allocated separately (optional) */
	void (*copy_data)(struct bConstraint *con, struct bConstraint *src);
	/* set settings for data that will be used for bConstraint.data (memory already allocated using MEM_callocN) */
	void (*new_data)(void *cdata);
	
	/* target handling function pointers */
	/* for multi-target constraints: return that list; otherwise make a temporary list (returns number of targets) */
	int (*get_constraint_targets)(struct bConstraint *con, struct ListBase *list);
	/* for single-target constraints only: flush data back to source data, and the free memory used */
	void (*flush_constraint_targets)(struct bConstraint *con, struct ListBase *list, bool no_copy);
	
	/* evaluation */
	/* set the ct->matrix for the given constraint target (at the given ctime) */
	void (*get_target_matrix)(struct bConstraint *con, struct bConstraintOb *cob, struct bConstraintTarget *ct, float ctime);
	/* evaluate the constraint for the given time */
	void (*evaluate_constraint)(struct bConstraint *con, struct bConstraintOb *cob, struct ListBase *targets);
} bConstraintTypeInfo;

/* Function Prototypes for bConstraintTypeInfo's */
bConstraintTypeInfo *BKE_constraint_typeinfo_get(struct bConstraint *con);
bConstraintTypeInfo *BKE_constraint_typeinfo_from_type(int type);


/* ---------------------------------------------------------------------------- */

/* Constraint function prototypes */
void BKE_constraint_unique_name(struct bConstraint *con, struct ListBase *list);

void BKE_constraints_free(struct ListBase *list);
void BKE_constraints_copy(struct ListBase *dst, const struct ListBase *src, bool do_extern);
void BKE_constraints_relink(struct ListBase *list);
void BKE_constraints_id_loop(struct ListBase *list, ConstraintIDFunc func, void *userdata);
void BKE_constraint_free_data(struct bConstraint *con);

/* Constraint API function prototypes */
struct bConstraint *BKE_constraints_active_get(struct ListBase *list);
void                BKE_constraints_active_set(ListBase *list, struct bConstraint *con);
struct bConstraint *BKE_constraints_find_name(struct ListBase *list, const char *name);

struct bConstraint *BKE_constraint_add_for_object(struct Object *ob, const char *name, short type);
struct bConstraint *BKE_constraint_add_for_pose(struct Object *ob, struct bPoseChannel *pchan, const char *name, short type);

bool                BKE_constraint_remove(ListBase *list, struct bConstraint *con);

/* Constraints + Proxies function prototypes */
void BKE_constraints_proxylocal_extract(struct ListBase *dst, struct ListBase *src);
bool BKE_constraints_proxylocked_owner(struct Object *ob, struct bPoseChannel *pchan);

/* Constraint Evaluation function prototypes */
struct bConstraintOb *BKE_constraints_make_evalob(struct Scene *scene, struct Object *ob, void *subdata, short datatype);
void                  BKE_constraints_clear_evalob(struct bConstraintOb *cob);

void BKE_constraint_mat_convertspace(struct Object *ob, struct bPoseChannel *pchan, float mat[4][4], short from, short to);

void BKE_constraint_target_matrix_get(struct Scene *scene, struct bConstraint *con, int n, short ownertype, void *ownerdata, float mat[4][4], float ctime);
void BKE_constraint_targets_for_solving_get(struct bConstraint *con, struct bConstraintOb *ob, struct ListBase *targets, float ctime);
void BKE_constraints_solve(struct ListBase *conlist, struct bConstraintOb *cob, float ctime);

#ifdef __cplusplus
}
#endif

#endif

