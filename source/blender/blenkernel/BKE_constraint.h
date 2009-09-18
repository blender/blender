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
 * Contributor(s): 2007 - Joshua Leung (major recode)
 *
 * ***** END GPL LICENSE BLOCK *****
 */

#ifndef BKE_CONSTRAINT_H
#define BKE_CONSTRAINT_H

struct bConstraint;
struct bConstraintTarget;
struct ListBase;
struct Object;
struct Scene;
struct bPoseChannel;

/* ---------------------------------------------------------------------------- */

/* special struct for use in constraint evaluation */
typedef struct bConstraintOb {
	struct Scene *scene;		/* for system time, part of deglobalization, code nicer later with local time (ton) */
	struct Object *ob;			/* if pchan, then armature that it comes from, otherwise constraint owner */
	struct bPoseChannel *pchan;	/* pose channel that owns the constraints being evaluated */
	
	float matrix[4][4];			/* matrix where constraints are accumulated + solved */
	float startmat[4][4];		/* original matrix (before constraint solving) */
	
	short type;					/* type of owner  */
	short rotOrder;				/* rotation order for constraint owner (as defined in eEulerRotationOrders in BLI_arithb.h) */
} bConstraintOb;

/* ---------------------------------------------------------------------------- */

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
	short type;				/* CONSTRAINT_TYPE_### */
	short size;				/* size in bytes of the struct */
	char name[32]; 			/* name of constraint in interface */
	char structName[32];	/* name of struct for SDNA */
	
	/* data management function pointers - special handling */
		/* free any data that is allocated separately (optional) */
	void (*free_data)(struct bConstraint *con);
		/* adjust pointer to other ID-data using ID_NEW(), but not to targets (optional) */
	void (*relink_data)(struct bConstraint *con);
		/* copy any special data that is allocated separately (optional) */
	void (*copy_data)(struct bConstraint *con, struct bConstraint *src);
		/* set settings for data that will be used for bConstraint.data (memory already allocated using MEM_callocN) */
	void (*new_data)(void *cdata);
	
	/* target handling function pointers */
		/* for multi-target constraints: return that list; otherwise make a temporary list (returns number of targets) */
	int (*get_constraint_targets)(struct bConstraint *con, struct ListBase *list);
		/* for single-target constraints only: flush data back to source data, and the free memory used */
	void (*flush_constraint_targets)(struct bConstraint *con, struct ListBase *list, short nocopy);
	
	/* evaluation */
		/* set the ct->matrix for the given constraint target (at the given ctime) */
	void (*get_target_matrix)(struct bConstraint *con, struct bConstraintOb *cob, struct bConstraintTarget *ct, float ctime);
		/* evaluate the constraint for the given time */
	void (*evaluate_constraint)(struct bConstraint *con, struct bConstraintOb *cob, struct ListBase *targets);
} bConstraintTypeInfo;

/* Function Prototypes for bConstraintTypeInfo's */
bConstraintTypeInfo *constraint_get_typeinfo(struct bConstraint *con);
bConstraintTypeInfo *get_constraint_typeinfo(int type);

/* ---------------------------------------------------------------------------- */
/* Useful macros for testing various common flag combinations */

/* Constraint Target Macros */
#define VALID_CONS_TARGET(ct) ((ct) && (ct->tar))


/* ---------------------------------------------------------------------------- */

/* Constraint function prototypes */
void unique_constraint_name(struct bConstraint *con, struct ListBase *list);

void free_constraints(struct ListBase *list);
void copy_constraints(struct ListBase *dst, struct ListBase *src);
void relink_constraints(struct ListBase *list);
void free_constraint_data(struct bConstraint *con);

struct bConstraint *constraints_get_active(struct ListBase *list);

/* Constraints + Proxies function prototypes */
void extract_proxylocal_constraints(struct ListBase *dst, struct ListBase *src);
short proxylocked_constraints_owner(struct Object *ob, struct bPoseChannel *pchan);

/* Constraint Evaluation function prototypes */
struct bConstraintOb *constraints_make_evalob(struct Scene *scene, struct Object *ob, void *subdata, short datatype);
void constraints_clear_evalob(struct bConstraintOb *cob);

void constraint_mat_convertspace(struct Object *ob, struct bPoseChannel *pchan, float mat[][4], short from, short to);

void get_constraint_target_matrix(struct bConstraint *con, int n, short ownertype, void *ownerdata, float mat[][4], float ctime);
void solve_constraints(struct ListBase *conlist, struct bConstraintOb *cob, float ctime);


#endif

