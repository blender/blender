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
 * Contributor(s): 2007, Joshua Leung, major recode
 *
 * ***** END GPL LICENSE BLOCK *****
 */

/** \file blender/blenkernel/intern/constraint.c
 *  \ingroup bke
 */


#include <stdio.h> 
#include <stddef.h>
#include <string.h>
#include <math.h>
#include <float.h>

#include "MEM_guardedalloc.h"

#include "BLI_blenlib.h"
#include "BLI_math.h"
#include "BLI_kdopbvh.h"
#include "BLI_utildefines.h"
#include "BLI_string_utils.h"
#include "BLT_translation.h"

#include "DNA_armature_types.h"
#include "DNA_cachefile_types.h"
#include "DNA_constraint_types.h"
#include "DNA_modifier_types.h"
#include "DNA_object_types.h"
#include "DNA_action_types.h"
#include "DNA_curve_types.h"
#include "DNA_meshdata_types.h"

#include "DNA_lattice_types.h"
#include "DNA_scene_types.h"
#include "DNA_tracking_types.h"
#include "DNA_movieclip_types.h"


#include "BKE_action.h"
#include "BKE_anim.h" /* for the curve calculation part */
#include "BKE_armature.h"
#include "BKE_bvhutils.h"
#include "BKE_cachefile.h"
#include "BKE_camera.h"
#include "BKE_constraint.h"
#include "BKE_curve.h"
#include "BKE_displist.h"
#include "BKE_deform.h"
#include "BKE_DerivedMesh.h"    /* for geometry targets */
#include "BKE_cdderivedmesh.h" /* for geometry targets */
#include "BKE_object.h"
#include "BKE_global.h"
#include "BKE_library.h"
#include "BKE_idprop.h"
#include "BKE_shrinkwrap.h"
#include "BKE_editmesh.h"
#include "BKE_scene.h"
#include "BKE_tracking.h"
#include "BKE_movieclip.h"

#include "BIK_api.h"

#ifdef WITH_PYTHON
#  include "BPY_extern.h"
#endif

#ifdef WITH_ALEMBIC
#  include "ABC_alembic.h"
#endif

/* ---------------------------------------------------------------------------- */
/* Useful macros for testing various common flag combinations */

/* Constraint Target Macros */
#define VALID_CONS_TARGET(ct) ((ct) && (ct->tar))

/* Workaround for cyclic depenndnecy with curves.
 * In such case curve_cache might not be ready yet,
 */
#define CYCLIC_DEPENDENCY_WORKAROUND

/* ************************ Constraints - General Utilities *************************** */
/* These functions here don't act on any specific constraints, and are therefore should/will
 * not require any of the special function-pointers afforded by the relevant constraint 
 * type-info structs.
 */

/* -------------- Naming -------------- */

/* Find the first available, non-duplicate name for a given constraint */
void BKE_constraint_unique_name(bConstraint *con, ListBase *list)
{
	BLI_uniquename(list, con, DATA_("Const"), '.', offsetof(bConstraint, name), sizeof(con->name));
}

/* ----------------- Evaluation Loop Preparation --------------- */

/* package an object/bone for use in constraint evaluation */
/* This function MEM_calloc's a bConstraintOb struct, that will need to be freed after evaluation */
bConstraintOb *BKE_constraints_make_evalob(Scene *scene, Object *ob, void *subdata, short datatype)
{
	bConstraintOb *cob;
	
	/* create regardless of whether we have any data! */
	cob = MEM_callocN(sizeof(bConstraintOb), "bConstraintOb");
	
	/* for system time, part of deglobalization, code nicer later with local time (ton) */
	cob->scene = scene;
	
	/* based on type of available data */
	switch (datatype) {
		case CONSTRAINT_OBTYPE_OBJECT:
		{
			/* disregard subdata... calloc should set other values right */
			if (ob) {
				cob->ob = ob;
				cob->type = datatype;
				
				if (cob->ob->rotmode > 0) {
					/* Should be some kind of Euler order, so use it */
					/* NOTE: Versions <= 2.76 assumed that "default" order
					 *       would always get used, so we may seem some rig
					 *       breakage as a result. However, this change here
					 *       is needed to fix T46599
					 */
					cob->rotOrder = ob->rotmode;
				}
				else {
					/* Quats/Axis-Angle, so Eulers should just use default order */
					cob->rotOrder = EULER_ORDER_DEFAULT;
				}
				copy_m4_m4(cob->matrix, ob->obmat);
			}
			else
				unit_m4(cob->matrix);
			
			copy_m4_m4(cob->startmat, cob->matrix);
			break;
		}
		case CONSTRAINT_OBTYPE_BONE:
		{
			/* only set if we have valid bone, otherwise default */
			if (ob && subdata) {
				cob->ob = ob;
				cob->pchan = (bPoseChannel *)subdata;
				cob->type = datatype;
				
				if (cob->pchan->rotmode > 0) {
					/* should be some type of Euler order */
					cob->rotOrder = cob->pchan->rotmode;
				}
				else {
					/* Quats, so eulers should just use default order */
					cob->rotOrder = EULER_ORDER_DEFAULT;
				}
				
				/* matrix in world-space */
				mul_m4_m4m4(cob->matrix, ob->obmat, cob->pchan->pose_mat);
			}
			else
				unit_m4(cob->matrix);
				
			copy_m4_m4(cob->startmat, cob->matrix);
			break;
		}
		default: /* other types not yet handled */
			unit_m4(cob->matrix);
			unit_m4(cob->startmat);
			break;
	}

	return cob;
}

/* cleanup after constraint evaluation */
void BKE_constraints_clear_evalob(bConstraintOb *cob)
{
	float delta[4][4], imat[4][4];
	
	/* prevent crashes */
	if (cob == NULL) 
		return;
	
	/* calculate delta of constraints evaluation */
	invert_m4_m4(imat, cob->startmat);
	/* XXX This would seem to be in wrong order. However, it does not work in 'right' order - would be nice to
	 *     understand why premul is needed here instead of usual postmul?
	 *     In any case, we **do not get a delta** here (e.g. startmat & matrix having same location, still gives
	 *     a 'delta' with non-null translation component :/ ).*/
	mul_m4_m4m4(delta, cob->matrix, imat);
	
	/* copy matrices back to source */
	switch (cob->type) {
		case CONSTRAINT_OBTYPE_OBJECT:
		{
			/* cob->ob might not exist! */
			if (cob->ob) {
				/* copy new ob-matrix back to owner */
				copy_m4_m4(cob->ob->obmat, cob->matrix);
				
				/* copy inverse of delta back to owner */
				invert_m4_m4(cob->ob->constinv, delta);
			}
			break;
		}
		case CONSTRAINT_OBTYPE_BONE:
		{
			/* cob->ob or cob->pchan might not exist */
			if (cob->ob && cob->pchan) {
				/* copy new pose-matrix back to owner */
				mul_m4_m4m4(cob->pchan->pose_mat, cob->ob->imat, cob->matrix);
				
				/* copy inverse of delta back to owner */
				invert_m4_m4(cob->pchan->constinv, delta);
			}
			break;
		}
	}
	
	/* free tempolary struct */
	MEM_freeN(cob);
}

/* -------------- Space-Conversion API -------------- */

/* This function is responsible for the correct transformations/conversions 
 * of a matrix from one space to another for constraint evaluation.
 * For now, this is only implemented for Objects and PoseChannels.
 */
void BKE_constraint_mat_convertspace(
        Object *ob, bPoseChannel *pchan, float mat[4][4], short from, short to, const bool keep_scale)
{
	float diff_mat[4][4];
	float imat[4][4];
	
	/* prevent crashes in these unlikely events  */
	if (ob == NULL || mat == NULL) return;
	/* optimize trick - check if need to do anything */
	if (from == to) return;
	
	/* are we dealing with pose-channels or objects */
	if (pchan) {
		/* pose channels */
		switch (from) {
			case CONSTRAINT_SPACE_WORLD: /* ---------- FROM WORLDSPACE ---------- */
			{
				/* world to pose */
				invert_m4_m4(imat, ob->obmat);
				mul_m4_m4m4(mat, imat, mat);
				
				/* use pose-space as stepping stone for other spaces... */
				if (ELEM(to, CONSTRAINT_SPACE_LOCAL, CONSTRAINT_SPACE_PARLOCAL)) {
					/* call self with slightly different values */
					BKE_constraint_mat_convertspace(ob, pchan, mat, CONSTRAINT_SPACE_POSE, to, keep_scale);
				}
				break;
			}
			case CONSTRAINT_SPACE_POSE: /* ---------- FROM POSESPACE ---------- */
			{
				/* pose to world */
				if (to == CONSTRAINT_SPACE_WORLD) {
					mul_m4_m4m4(mat, ob->obmat, mat);
				}
				/* pose to local */
				else if (to == CONSTRAINT_SPACE_LOCAL) {
					if (pchan->bone) {
						BKE_armature_mat_pose_to_bone(pchan, mat, mat);
					}
				}
				/* pose to local with parent */
				else if (to == CONSTRAINT_SPACE_PARLOCAL) {
					if (pchan->bone) {
						invert_m4_m4(imat, pchan->bone->arm_mat);
						mul_m4_m4m4(mat, imat, mat);
					}
				}
				break;
			}
			case CONSTRAINT_SPACE_LOCAL: /* ------------ FROM LOCALSPACE --------- */
			{
				/* local to pose - do inverse procedure that was done for pose to local */
				if (pchan->bone) {
					/* we need the posespace_matrix = local_matrix + (parent_posespace_matrix + restpos) */
					BKE_armature_mat_bone_to_pose(pchan, mat, mat);
				}
				
				/* use pose-space as stepping stone for other spaces */
				if (ELEM(to, CONSTRAINT_SPACE_WORLD, CONSTRAINT_SPACE_PARLOCAL)) {
					/* call self with slightly different values */
					BKE_constraint_mat_convertspace(ob, pchan, mat, CONSTRAINT_SPACE_POSE, to, keep_scale);
				}
				break;
			}
			case CONSTRAINT_SPACE_PARLOCAL: /* -------------- FROM LOCAL WITH PARENT ---------- */
			{
				/* local + parent to pose */
				if (pchan->bone) {
					copy_m4_m4(diff_mat, pchan->bone->arm_mat);
					mul_m4_m4m4(mat, mat, diff_mat);
				}
				
				/* use pose-space as stepping stone for other spaces */
				if (ELEM(to, CONSTRAINT_SPACE_WORLD, CONSTRAINT_SPACE_LOCAL)) {
					/* call self with slightly different values */
					BKE_constraint_mat_convertspace(ob, pchan, mat, CONSTRAINT_SPACE_POSE, to, keep_scale);
				}
				break;
			}
		}
	}
	else {
		/* objects */
		if (from == CONSTRAINT_SPACE_WORLD && to == CONSTRAINT_SPACE_LOCAL) {
			/* check if object has a parent */
			if (ob->parent) {
				/* 'subtract' parent's effects from owner */
				mul_m4_m4m4(diff_mat, ob->parent->obmat, ob->parentinv);
				invert_m4_m4_safe(imat, diff_mat);
				mul_m4_m4m4(mat, imat, mat);
			}
			else {
				/* Local space in this case will have to be defined as local to the owner's 
				 * transform-property-rotated axes. So subtract this rotation component.
				 */
				/* XXX This is actually an ugly hack, local space of a parent-less object *is* the same as
				 *     global space!
				 *     Think what we want actually here is some kind of 'Final Space', i.e. once transformations
				 *     are applied - users are often confused about this too, this is not consistent with bones
				 *     local space either... Meh :|
				 *     --mont29
				 */
				BKE_object_to_mat4(ob, diff_mat);
				if (!keep_scale) {
					normalize_m4(diff_mat);
				}
				zero_v3(diff_mat[3]);
				
				invert_m4_m4_safe(imat, diff_mat);
				mul_m4_m4m4(mat, imat, mat);
			}
		}
		else if (from == CONSTRAINT_SPACE_LOCAL && to == CONSTRAINT_SPACE_WORLD) {
			/* check that object has a parent - otherwise this won't work */
			if (ob->parent) {
				/* 'add' parent's effect back to owner */
				mul_m4_m4m4(diff_mat, ob->parent->obmat, ob->parentinv);
				mul_m4_m4m4(mat, diff_mat, mat);
			}
			else {
				/* Local space in this case will have to be defined as local to the owner's 
				 * transform-property-rotated axes. So add back this rotation component.
				 */
				/* XXX See comment above for world->local case... */
				BKE_object_to_mat4(ob, diff_mat);
				if (!keep_scale) {
					normalize_m4(diff_mat);
				}
				zero_v3(diff_mat[3]);
				
				mul_m4_m4m4(mat, diff_mat, mat);
			}
		}
	}
}

/* ------------ General Target Matrix Tools ---------- */

/* function that sets the given matrix based on given vertex group in mesh */
static void contarget_get_mesh_mat(Object *ob, const char *substring, float mat[4][4])
{
	DerivedMesh *dm = NULL;
	BMEditMesh *em = BKE_editmesh_from_object(ob);
	float vec[3] = {0.0f, 0.0f, 0.0f};
	float normal[3] = {0.0f, 0.0f, 0.0f}, plane[3];
	float imat[3][3], tmat[3][3];
	const int defgroup = defgroup_name_index(ob, substring);
	short freeDM = 0;
	
	/* initialize target matrix using target matrix */
	copy_m4_m4(mat, ob->obmat);
	
	/* get index of vertex group */
	if (defgroup == -1) return;

	/* get DerivedMesh */
	if (em) {
		/* target is in editmode, so get a special derived mesh */
		dm = CDDM_from_editbmesh(em, false, false);
		freeDM = 1;
	}
	else {
		/* when not in EditMode, use the 'final' derived mesh, depsgraph
		 * ensures we build with CD_MDEFORMVERT layer 
		 */
		dm = (DerivedMesh *)ob->derivedFinal;
	}
	
	/* only continue if there's a valid DerivedMesh */
	if (dm) {
		MDeformVert *dvert = dm->getVertDataArray(dm, CD_MDEFORMVERT);
		int numVerts = dm->getNumVerts(dm);
		int i;
		float co[3], nor[3];
		
		/* check that dvert is a valid pointers (just in case) */
		if (dvert) {
			MDeformVert *dv = dvert;
			float weightsum = 0.0f;

			/* get the average of all verts with that are in the vertex-group */
			for (i = 0; i < numVerts; i++, dv++) {
				MDeformWeight *dw = defvert_find_index(dv, defgroup);

				if (dw && dw->weight > 0.0f) {
					dm->getVertCo(dm, i, co);
					dm->getVertNo(dm, i, nor);
					madd_v3_v3fl(vec, co, dw->weight);
					madd_v3_v3fl(normal, nor, dw->weight);
					weightsum += dw->weight;
				}
			}

			/* calculate averages of normal and coordinates */
			if (weightsum > 0) {
				mul_v3_fl(vec, 1.0f / weightsum);
				mul_v3_fl(normal, 1.0f / weightsum);
			}
			
			
			/* derive the rotation from the average normal: 
			 *		- code taken from transform_manipulator.c, 
			 *			calc_manipulator_stats, V3D_MANIP_NORMAL case
			 */
			/*	we need the transpose of the inverse for a normal... */
			copy_m3_m4(imat, ob->obmat);
			
			invert_m3_m3(tmat, imat);
			transpose_m3(tmat);
			mul_m3_v3(tmat, normal);
			
			normalize_v3(normal);
			copy_v3_v3(plane, tmat[1]);
			
			cross_v3_v3v3(mat[0], normal, plane);
			if (len_squared_v3(mat[0]) < SQUARE(1e-3f)) {
				copy_v3_v3(plane, tmat[0]);
				cross_v3_v3v3(mat[0], normal, plane);
			}

			copy_v3_v3(mat[2], normal);
			cross_v3_v3v3(mat[1], mat[2], mat[0]);

			normalize_m4(mat);

			
			/* apply the average coordinate as the new location */
			mul_v3_m4v3(mat[3], ob->obmat, vec);
		}
	}
	
	/* free temporary DerivedMesh created (in EditMode case) */
	if (dm && freeDM)
		dm->release(dm);
}

/* function that sets the given matrix based on given vertex group in lattice */
static void contarget_get_lattice_mat(Object *ob, const char *substring, float mat[4][4])
{
	Lattice *lt = (Lattice *)ob->data;
	
	DispList *dl = ob->curve_cache ? BKE_displist_find(&ob->curve_cache->disp, DL_VERTS) : NULL;
	const float *co = dl ? dl->verts : NULL;
	BPoint *bp = lt->def;
	
	MDeformVert *dv = lt->dvert;
	int tot_verts = lt->pntsu * lt->pntsv * lt->pntsw;
	float vec[3] = {0.0f, 0.0f, 0.0f}, tvec[3];
	int grouped = 0;
	int i, n;
	const int defgroup = defgroup_name_index(ob, substring);
	
	/* initialize target matrix using target matrix */
	copy_m4_m4(mat, ob->obmat);

	/* get index of vertex group */
	if (defgroup == -1) return;
	if (dv == NULL) return;
	
	/* 1. Loop through control-points checking if in nominated vertex-group.
	 * 2. If it is, add it to vec to find the average point.
	 */
	for (i = 0; i < tot_verts; i++, dv++) {
		for (n = 0; n < dv->totweight; n++) {
			MDeformWeight *dw = defvert_find_index(dv, defgroup);
			if (dw && dw->weight > 0.0f) {
				/* copy coordinates of point to temporary vector, then add to find average */
				memcpy(tvec, co ? co : bp->vec, 3 * sizeof(float));

				add_v3_v3(vec, tvec);
				grouped++;
			}
		}
		
		/* advance pointer to coordinate data */
		if (co) co += 3;
		else bp++;
	}
	
	/* find average location, then multiply by ob->obmat to find world-space location */
	if (grouped)
		mul_v3_fl(vec, 1.0f / grouped);
	mul_v3_m4v3(tvec, ob->obmat, vec);
	
	/* copy new location to matrix */
	copy_v3_v3(mat[3], tvec);
}

/* generic function to get the appropriate matrix for most target cases */
/* The cases where the target can be object data have not been implemented */
static void constraint_target_to_mat4(Object *ob, const char *substring, float mat[4][4], short from, short to, short flag, float headtail)
{
	/*	Case OBJECT */
	if (substring[0] == '\0') {
		copy_m4_m4(mat, ob->obmat);
		BKE_constraint_mat_convertspace(ob, NULL, mat, from, to, false);
	}
	/*  Case VERTEXGROUP */
	/* Current method just takes the average location of all the points in the
	 * VertexGroup, and uses that as the location value of the targets. Where 
	 * possible, the orientation will also be calculated, by calculating an
	 * 'average' vertex normal, and deriving the rotation from that.
	 *
	 * NOTE: EditMode is not currently supported, and will most likely remain that
	 *		way as constraints can only really affect things on object/bone level.
	 */
	else if (ob->type == OB_MESH) {
		contarget_get_mesh_mat(ob, substring, mat);
		BKE_constraint_mat_convertspace(ob, NULL, mat, from, to, false);
	}
	else if (ob->type == OB_LATTICE) {
		contarget_get_lattice_mat(ob, substring, mat);
		BKE_constraint_mat_convertspace(ob, NULL, mat, from, to, false);
	}
	/*	Case BONE */
	else {
		bPoseChannel *pchan;
		
		pchan = BKE_pose_channel_find_name(ob->pose, substring);
		if (pchan) {
			/* Multiply the PoseSpace accumulation/final matrix for this
			 * PoseChannel by the Armature Object's Matrix to get a worldspace
			 * matrix.
			 */
			if (headtail < 0.000001f) {
				/* skip length interpolation if set to head */
				mul_m4_m4m4(mat, ob->obmat, pchan->pose_mat);
			}
			else if ((pchan->bone) && (pchan->bone->segments > 1) && (flag & CONSTRAINT_BBONE_SHAPE)) {
				/* use point along bbone */
				Mat4 bbone[MAX_BBONE_SUBDIV];
				float tempmat[4][4];
				float loc[3], fac;
				
				/* get bbone segments */
				b_bone_spline_setup(pchan, 0, bbone);
				
				/* figure out which segment(s) the headtail value falls in */
				fac = (float)pchan->bone->segments * headtail;
				
				if (fac >= pchan->bone->segments - 1) {
					/* special case: end segment doesn't get created properly... */
					float pt[3], sfac;
					int index;
					
					/* bbone points are in bonespace, so need to move to posespace first */
					index = pchan->bone->segments - 1;
					mul_v3_m4v3(pt, pchan->pose_mat, bbone[index].mat[3]);
					
					/* interpolate between last segment point and the endpoint */
					sfac = fac - (float)(pchan->bone->segments - 1); /* fac is just the "leftover" between penultimate and last points */
					interp_v3_v3v3(loc, pt, pchan->pose_tail, sfac);
				}
				else {
					/* get indices for finding interpolating between points along the bbone */
					float pt_a[3], pt_b[3], pt[3];
					int   index_a, index_b;
					
					index_a = floorf(fac);
					CLAMP(index_a, 0, MAX_BBONE_SUBDIV - 1);
					
					index_b = ceilf(fac);
					CLAMP(index_b, 0, MAX_BBONE_SUBDIV - 1);
					
					/* interpolate between these points */
					copy_v3_v3(pt_a, bbone[index_a].mat[3]);
					copy_v3_v3(pt_b, bbone[index_b].mat[3]);
					
					interp_v3_v3v3(pt, pt_a, pt_b, fac - floorf(fac));
					
					/* move the point from bone local space to pose space... */
					mul_v3_m4v3(loc, pchan->pose_mat, pt);
				}
				
				/* use interpolated distance for subtarget */
				copy_m4_m4(tempmat, pchan->pose_mat);
				copy_v3_v3(tempmat[3], loc);
				
				mul_m4_m4m4(mat, ob->obmat, tempmat);
			}
			else {
				float tempmat[4][4], loc[3];
				
				/* interpolate along length of bone */
				interp_v3_v3v3(loc, pchan->pose_head, pchan->pose_tail, headtail);
				
				/* use interpolated distance for subtarget */
				copy_m4_m4(tempmat, pchan->pose_mat);
				copy_v3_v3(tempmat[3], loc);
				
				mul_m4_m4m4(mat, ob->obmat, tempmat);
			}
		}
		else
			copy_m4_m4(mat, ob->obmat);
			
		/* convert matrix space as required */
		BKE_constraint_mat_convertspace(ob, pchan, mat, from, to, false);
	}
}

/* ************************* Specific Constraints ***************************** */
/* Each constraint defines a set of functions, which will be called at the appropriate
 * times. In addition to this, each constraint should have a type-info struct, where
 * its functions are attached for use. 
 */
 
/* Template for type-info data:
 *  - make a copy of this when creating new constraints, and just change the functions
 *    pointed to as necessary
 *  - although the naming of functions doesn't matter, it would help for code
 *    readability, to follow the same naming convention as is presented here
 *  - any functions that a constraint doesn't need to define, don't define
 *    for such cases, just use NULL 
 *  - these should be defined after all the functions have been defined, so that
 *    forward-definitions/prototypes don't need to be used!
 *	- keep this copy #if-def'd so that future constraints can get based off this
 */
#if 0
static bConstraintTypeInfo CTI_CONSTRNAME = {
	CONSTRAINT_TYPE_CONSTRNAME, /* type */
	sizeof(bConstrNameConstraint), /* size */
	"ConstrName", /* name */
	"bConstrNameConstraint", /* struct name */
	constrname_free, /* free data */
	constrname_id_looper, /* id looper */
	constrname_copy, /* copy data */
	constrname_new_data, /* new data */
	constrname_get_tars, /* get constraint targets */
	constrname_flush_tars, /* flush constraint targets */
	constrname_get_tarmat, /* get target matrix */
	constrname_evaluate /* evaluate */
};
#endif

/* This function should be used for the get_target_matrix member of all 
 * constraints that are not picky about what happens to their target matrix.
 */
static void default_get_tarmat(bConstraint *con, bConstraintOb *UNUSED(cob), bConstraintTarget *ct, float UNUSED(ctime))
{
	if (VALID_CONS_TARGET(ct))
		constraint_target_to_mat4(ct->tar, ct->subtarget, ct->matrix, CONSTRAINT_SPACE_WORLD, ct->space, con->flag, con->headtail);
	else if (ct)
		unit_m4(ct->matrix);
}

/* This following macro should be used for all standard single-target *_get_tars functions 
 * to save typing and reduce maintenance woes.
 * (Hopefully all compilers will be happy with the lines with just a space on them. Those are
 *  really just to help this code easier to read)
 */
// TODO: cope with getting rotation order...
#define SINGLETARGET_GET_TARS(con, datatar, datasubtarget, ct, list) \
	{ \
		ct = MEM_callocN(sizeof(bConstraintTarget), "tempConstraintTarget"); \
		 \
		ct->tar = datatar; \
		BLI_strncpy(ct->subtarget, datasubtarget, sizeof(ct->subtarget)); \
		ct->space = con->tarspace; \
		ct->flag = CONSTRAINT_TAR_TEMP; \
		 \
		if (ct->tar) { \
			if ((ct->tar->type == OB_ARMATURE) && (ct->subtarget[0])) { \
				bPoseChannel *pchan = BKE_pose_channel_find_name(ct->tar->pose, ct->subtarget); \
				ct->type = CONSTRAINT_OBTYPE_BONE; \
				ct->rotOrder = (pchan) ? (pchan->rotmode) : EULER_ORDER_DEFAULT; \
			} \
			else if (OB_TYPE_SUPPORT_VGROUP(ct->tar->type) && (ct->subtarget[0])) { \
				ct->type = CONSTRAINT_OBTYPE_VERT; \
				ct->rotOrder = EULER_ORDER_DEFAULT; \
			} \
			else { \
				ct->type = CONSTRAINT_OBTYPE_OBJECT; \
				ct->rotOrder = ct->tar->rotmode; \
			} \
		} \
		 \
		BLI_addtail(list, ct); \
	} (void)0
	
/* This following macro should be used for all standard single-target *_get_tars functions 
 * to save typing and reduce maintenance woes. It does not do the subtarget related operations
 * (Hopefully all compilers will be happy with the lines with just a space on them. Those are
 *  really just to help this code easier to read)
 */
// TODO: cope with getting rotation order...
#define SINGLETARGETNS_GET_TARS(con, datatar, ct, list) \
	{ \
		ct = MEM_callocN(sizeof(bConstraintTarget), "tempConstraintTarget"); \
		 \
		ct->tar = datatar; \
		ct->space = con->tarspace; \
		ct->flag = CONSTRAINT_TAR_TEMP; \
		 \
		if (ct->tar) ct->type = CONSTRAINT_OBTYPE_OBJECT; \
		 \
		BLI_addtail(list, ct); \
	} (void)0

/* This following macro should be used for all standard single-target *_flush_tars functions
 * to save typing and reduce maintenance woes.
 * Note: the pointer to ct will be changed to point to the next in the list (as it gets removed)
 * (Hopefully all compilers will be happy with the lines with just a space on them. Those are
 *  really just to help this code easier to read)
 */
#define SINGLETARGET_FLUSH_TARS(con, datatar, datasubtarget, ct, list, no_copy) \
	{ \
		if (ct) { \
			bConstraintTarget *ctn = ct->next; \
			if (no_copy == 0) { \
				datatar = ct->tar; \
				BLI_strncpy(datasubtarget, ct->subtarget, sizeof(datasubtarget)); \
				con->tarspace = (char)ct->space; \
			} \
			 \
			BLI_freelinkN(list, ct); \
			ct = ctn; \
		} \
	} (void)0
	
/* This following macro should be used for all standard single-target *_flush_tars functions
 * to save typing and reduce maintenance woes. It does not do the subtarget related operations.
 * Note: the pointer to ct will be changed to point to the next in the list (as it gets removed)
 * (Hopefully all compilers will be happy with the lines with just a space on them. Those are
 *  really just to help this code easier to read)
 */
#define SINGLETARGETNS_FLUSH_TARS(con, datatar, ct, list, no_copy) \
	{ \
		if (ct) { \
			bConstraintTarget *ctn = ct->next; \
			if (no_copy == 0) { \
				datatar = ct->tar; \
				con->tarspace = (char)ct->space; \
			} \
			 \
			BLI_freelinkN(list, ct); \
			ct = ctn; \
		} \
	} (void)0

/* --------- ChildOf Constraint ------------ */

static void childof_new_data(void *cdata)
{
	bChildOfConstraint *data = (bChildOfConstraint *)cdata;
	
	data->flag = (CHILDOF_LOCX | CHILDOF_LOCY | CHILDOF_LOCZ |
	              CHILDOF_ROTX | CHILDOF_ROTY | CHILDOF_ROTZ |
	              CHILDOF_SIZEX | CHILDOF_SIZEY | CHILDOF_SIZEZ);
	unit_m4(data->invmat);
}

static void childof_id_looper(bConstraint *con, ConstraintIDFunc func, void *userdata)
{
	bChildOfConstraint *data = con->data;
	
	/* target only */
	func(con, (ID **)&data->tar, false, userdata);
}

static int childof_get_tars(bConstraint *con, ListBase *list)
{
	if (con && list) {
		bChildOfConstraint *data = con->data;
		bConstraintTarget *ct;
		
		/* standard target-getting macro for single-target constraints */
		SINGLETARGET_GET_TARS(con, data->tar, data->subtarget, ct, list);
		
		return 1;
	}
	
	return 0;
}

static void childof_flush_tars(bConstraint *con, ListBase *list, bool no_copy)
{
	if (con && list) {
		bChildOfConstraint *data = con->data;
		bConstraintTarget *ct = list->first;
		
		/* the following macro is used for all standard single-target constraints */
		SINGLETARGET_FLUSH_TARS(con, data->tar, data->subtarget, ct, list, no_copy);
	}
}

static void childof_evaluate(bConstraint *con, bConstraintOb *cob, ListBase *targets)
{
	bChildOfConstraint *data = con->data;
	bConstraintTarget *ct = targets->first;

	/* only evaluate if there is a target */
	if (VALID_CONS_TARGET(ct)) {
		float parmat[4][4];
		
		/* simple matrix parenting */
		if (data->flag == CHILDOF_ALL) {
			
			/* multiply target (parent matrix) by offset (parent inverse) to get 
			 * the effect of the parent that will be exerted on the owner
			 */
			mul_m4_m4m4(parmat, ct->matrix, data->invmat);
			
			/* now multiply the parent matrix by the owner matrix to get the 
			 * the effect of this constraint (i.e. owner is 'parented' to parent)
			 */
			mul_m4_m4m4(cob->matrix, parmat, cob->matrix);
		}
		else {
			float invmat[4][4], tempmat[4][4];
			float loc[3], eul[3], size[3];
			float loco[3], eulo[3], sizo[3];
			
			/* get offset (parent-inverse) matrix */
			copy_m4_m4(invmat, data->invmat);
			
			/* extract components of both matrices */
			copy_v3_v3(loc, ct->matrix[3]);
			mat4_to_eulO(eul, ct->rotOrder, ct->matrix);
			mat4_to_size(size, ct->matrix);
			
			copy_v3_v3(loco, invmat[3]);
			mat4_to_eulO(eulo, cob->rotOrder, invmat);
			mat4_to_size(sizo, invmat);
			
			/* disable channels not enabled */
			if (!(data->flag & CHILDOF_LOCX)) loc[0] = loco[0] = 0.0f;
			if (!(data->flag & CHILDOF_LOCY)) loc[1] = loco[1] = 0.0f;
			if (!(data->flag & CHILDOF_LOCZ)) loc[2] = loco[2] = 0.0f;
			if (!(data->flag & CHILDOF_ROTX)) eul[0] = eulo[0] = 0.0f;
			if (!(data->flag & CHILDOF_ROTY)) eul[1] = eulo[1] = 0.0f;
			if (!(data->flag & CHILDOF_ROTZ)) eul[2] = eulo[2] = 0.0f;
			if (!(data->flag & CHILDOF_SIZEX)) size[0] = sizo[0] = 1.0f;
			if (!(data->flag & CHILDOF_SIZEY)) size[1] = sizo[1] = 1.0f;
			if (!(data->flag & CHILDOF_SIZEZ)) size[2] = sizo[2] = 1.0f;
			
			/* make new target mat and offset mat */
			loc_eulO_size_to_mat4(ct->matrix, loc, eul, size, ct->rotOrder);
			loc_eulO_size_to_mat4(invmat, loco, eulo, sizo, cob->rotOrder);
			
			/* multiply target (parent matrix) by offset (parent inverse) to get 
			 * the effect of the parent that will be exerted on the owner
			 */
			mul_m4_m4m4(parmat, ct->matrix, invmat);
			
			/* now multiply the parent matrix by the owner matrix to get the 
			 * the effect of this constraint (i.e.  owner is 'parented' to parent)
			 */
			copy_m4_m4(tempmat, cob->matrix);
			mul_m4_m4m4(cob->matrix, parmat, tempmat);
			
			/* without this, changes to scale and rotation can change location
			 * of a parentless bone or a disconnected bone. Even though its set
			 * to zero above. */
			if (!(data->flag & CHILDOF_LOCX)) cob->matrix[3][0] = tempmat[3][0];
			if (!(data->flag & CHILDOF_LOCY)) cob->matrix[3][1] = tempmat[3][1];
			if (!(data->flag & CHILDOF_LOCZ)) cob->matrix[3][2] = tempmat[3][2];
		}
	}
}

/* XXX note, con->flag should be CONSTRAINT_SPACEONCE for bone-childof, patched in readfile.c */
static bConstraintTypeInfo CTI_CHILDOF = {
	CONSTRAINT_TYPE_CHILDOF, /* type */
	sizeof(bChildOfConstraint), /* size */
	"Child Of", /* name */
	"bChildOfConstraint", /* struct name */
	NULL, /* free data */
	childof_id_looper, /* id looper */
	NULL, /* copy data */
	childof_new_data, /* new data */
	childof_get_tars, /* get constraint targets */
	childof_flush_tars, /* flush constraint targets */
	default_get_tarmat, /* get a target matrix */
	childof_evaluate /* evaluate */
};

/* -------- TrackTo Constraint ------- */

static void trackto_new_data(void *cdata)
{
	bTrackToConstraint *data = (bTrackToConstraint *)cdata;
	
	data->reserved1 = TRACK_Y;
	data->reserved2 = UP_Z;
}	

static void trackto_id_looper(bConstraint *con, ConstraintIDFunc func, void *userdata)
{
	bTrackToConstraint *data = con->data;
	
	/* target only */
	func(con, (ID **)&data->tar, false, userdata);
}

static int trackto_get_tars(bConstraint *con, ListBase *list)
{
	if (con && list) {
		bTrackToConstraint *data = con->data;
		bConstraintTarget *ct;
		
		/* standard target-getting macro for single-target constraints */
		SINGLETARGET_GET_TARS(con, data->tar, data->subtarget, ct, list);
		
		return 1;
	}
	
	return 0;
}

static void trackto_flush_tars(bConstraint *con, ListBase *list, bool no_copy)
{
	if (con && list) {
		bTrackToConstraint *data = con->data;
		bConstraintTarget *ct = list->first;
		
		/* the following macro is used for all standard single-target constraints */
		SINGLETARGET_FLUSH_TARS(con, data->tar, data->subtarget, ct, list, no_copy);
	}
}


static int basis_cross(int n, int m)
{
	switch (n - m) {
		case 1: 
		case -2:
			return 1;
			
		case -1: 
		case 2:
			return -1;
			
		default:
			return 0;
	}
}

static void vectomat(const float vec[3], const float target_up[3], short axis, short upflag, short flags, float m[3][3])
{
	float n[3];
	float u[3]; /* vector specifying the up axis */
	float proj[3];
	float right[3];
	float neg = -1;
	int right_index;

	if (normalize_v3_v3(n, vec) == 0.0f) {
		n[0] = 0.0f;
		n[1] = 0.0f;
		n[2] = 1.0f;
	}
	if (axis > 2) axis -= 3;
	else negate_v3(n);

	/* n specifies the transformation of the track axis */
	if (flags & TARGET_Z_UP) {
		/* target Z axis is the global up axis */
		copy_v3_v3(u, target_up);
	}
	else {
		/* world Z axis is the global up axis */
		u[0] = 0;
		u[1] = 0;
		u[2] = 1;
	}

	/* project the up vector onto the plane specified by n */
	project_v3_v3v3(proj, u, n); /* first u onto n... */
	sub_v3_v3v3(proj, u, proj); /* then onto the plane */
	/* proj specifies the transformation of the up axis */

	if (normalize_v3(proj) == 0.0f) { /* degenerate projection */
		proj[0] = 0.0f;
		proj[1] = 1.0f;
		proj[2] = 0.0f;
	}

	/* Normalized cross product of n and proj specifies transformation of the right axis */
	cross_v3_v3v3(right, proj, n);
	normalize_v3(right);

	if (axis != upflag) {
		right_index = 3 - axis - upflag;
		neg = (float)basis_cross(axis, upflag);
		
		/* account for up direction, track direction */
		m[right_index][0] = neg * right[0];
		m[right_index][1] = neg * right[1];
		m[right_index][2] = neg * right[2];
		
		copy_v3_v3(m[upflag], proj);
		
		copy_v3_v3(m[axis], n);
	}
	/* identity matrix - don't do anything if the two axes are the same */
	else {
		unit_m3(m);
	}
}


static void trackto_evaluate(bConstraint *con, bConstraintOb *cob, ListBase *targets)
{
	bTrackToConstraint *data = con->data;
	bConstraintTarget *ct = targets->first;
	
	if (VALID_CONS_TARGET(ct)) {
		float size[3], vec[3];
		float totmat[3][3];
		
		/* Get size property, since ob->size is only the object's own relative size, not its global one */
		mat4_to_size(size, cob->matrix);
		
		/* Clear the object's rotation */
		cob->matrix[0][0] = size[0];
		cob->matrix[0][1] = 0;
		cob->matrix[0][2] = 0;
		cob->matrix[1][0] = 0;
		cob->matrix[1][1] = size[1];
		cob->matrix[1][2] = 0;
		cob->matrix[2][0] = 0;
		cob->matrix[2][1] = 0;
		cob->matrix[2][2] = size[2];
		
		/* targetmat[2] instead of ownermat[2] is passed to vectomat
		 * for backwards compatibility it seems... (Aligorith)
		 */
		sub_v3_v3v3(vec, cob->matrix[3], ct->matrix[3]);
		vectomat(vec, ct->matrix[2], 
		         (short)data->reserved1, (short)data->reserved2,
		         data->flags, totmat);

		mul_m4_m3m4(cob->matrix, totmat, cob->matrix);
	}
}

static bConstraintTypeInfo CTI_TRACKTO = {
	CONSTRAINT_TYPE_TRACKTO, /* type */
	sizeof(bTrackToConstraint), /* size */
	"Track To", /* name */
	"bTrackToConstraint", /* struct name */
	NULL, /* free data */
	trackto_id_looper, /* id looper */
	NULL, /* copy data */
	trackto_new_data, /* new data */
	trackto_get_tars, /* get constraint targets */
	trackto_flush_tars, /* flush constraint targets */
	default_get_tarmat, /* get target matrix */
	trackto_evaluate /* evaluate */
};

/* --------- Inverse-Kinematics --------- */

static void kinematic_new_data(void *cdata)
{
	bKinematicConstraint *data = (bKinematicConstraint *)cdata;
	
	data->weight = 1.0f;
	data->orientweight = 1.0f;
	data->iterations = 500;
	data->dist = 1.0f;
	data->flag = CONSTRAINT_IK_TIP | CONSTRAINT_IK_STRETCH | CONSTRAINT_IK_POS;
}

static void kinematic_id_looper(bConstraint *con, ConstraintIDFunc func, void *userdata)
{
	bKinematicConstraint *data = con->data;
	
	/* chain target */
	func(con, (ID **)&data->tar, false, userdata);
	
	/* poletarget */
	func(con, (ID **)&data->poletar, false, userdata);
}

static int kinematic_get_tars(bConstraint *con, ListBase *list)
{
	if (con && list) {
		bKinematicConstraint *data = con->data;
		bConstraintTarget *ct;
		
		/* standard target-getting macro for single-target constraints is used twice here */
		SINGLETARGET_GET_TARS(con, data->tar, data->subtarget, ct, list);
		SINGLETARGET_GET_TARS(con, data->poletar, data->polesubtarget, ct, list);
		
		return 2;
	}
	
	return 0;
}

static void kinematic_flush_tars(bConstraint *con, ListBase *list, bool no_copy)
{
	if (con && list) {
		bKinematicConstraint *data = con->data;
		bConstraintTarget *ct = list->first;
		
		/* the following macro is used for all standard single-target constraints */
		SINGLETARGET_FLUSH_TARS(con, data->tar, data->subtarget, ct, list, no_copy);
		SINGLETARGET_FLUSH_TARS(con, data->poletar, data->polesubtarget, ct, list, no_copy);
	}
}

static void kinematic_get_tarmat(bConstraint *con, bConstraintOb *cob, bConstraintTarget *ct, float UNUSED(ctime))
{
	bKinematicConstraint *data = con->data;
	
	if (VALID_CONS_TARGET(ct)) 
		constraint_target_to_mat4(ct->tar, ct->subtarget, ct->matrix, CONSTRAINT_SPACE_WORLD, ct->space, con->flag, con->headtail);
	else if (ct) {
		if (data->flag & CONSTRAINT_IK_AUTO) {
			Object *ob = cob->ob;
			
			if (ob == NULL) {
				unit_m4(ct->matrix);
			}
			else {
				float vec[3];
				/* move grabtarget into world space */
				mul_v3_m4v3(vec, ob->obmat, data->grabtarget);
				copy_m4_m4(ct->matrix, ob->obmat);
				copy_v3_v3(ct->matrix[3], vec);
			}
		}
		else
			unit_m4(ct->matrix);
	}
}

static bConstraintTypeInfo CTI_KINEMATIC = {
	CONSTRAINT_TYPE_KINEMATIC, /* type */
	sizeof(bKinematicConstraint), /* size */
	"IK", /* name */
	"bKinematicConstraint", /* struct name */
	NULL, /* free data */
	kinematic_id_looper, /* id looper */
	NULL, /* copy data */
	kinematic_new_data, /* new data */
	kinematic_get_tars, /* get constraint targets */
	kinematic_flush_tars, /* flush constraint targets */
	kinematic_get_tarmat, /* get target matrix */
	NULL /* evaluate - solved as separate loop */
};

/* -------- Follow-Path Constraint ---------- */

static void followpath_new_data(void *cdata)
{
	bFollowPathConstraint *data = (bFollowPathConstraint *)cdata;
	
	data->trackflag = TRACK_Y;
	data->upflag = UP_Z;
	data->offset = 0;
	data->followflag = 0;
}

static void followpath_id_looper(bConstraint *con, ConstraintIDFunc func, void *userdata)
{
	bFollowPathConstraint *data = con->data;
	
	/* target only */
	func(con, (ID **)&data->tar, false, userdata);
}

static int followpath_get_tars(bConstraint *con, ListBase *list)
{
	if (con && list) {
		bFollowPathConstraint *data = con->data;
		bConstraintTarget *ct;
		
		/* standard target-getting macro for single-target constraints without subtargets */
		SINGLETARGETNS_GET_TARS(con, data->tar, ct, list);
		
		return 1;
	}
	
	return 0;
}

static void followpath_flush_tars(bConstraint *con, ListBase *list, bool no_copy)
{
	if (con && list) {
		bFollowPathConstraint *data = con->data;
		bConstraintTarget *ct = list->first;
		
		/* the following macro is used for all standard single-target constraints */
		SINGLETARGETNS_FLUSH_TARS(con, data->tar, ct, list, no_copy);
	}
}

static void followpath_get_tarmat(bConstraint *con, bConstraintOb *cob, bConstraintTarget *ct, float UNUSED(ctime))
{
	bFollowPathConstraint *data = con->data;
	
	if (VALID_CONS_TARGET(ct) && (ct->tar->type == OB_CURVE)) {
		Curve *cu = ct->tar->data;
		float vec[4], dir[3], radius;
		float curvetime;

		unit_m4(ct->matrix);

		/* note: when creating constraints that follow path, the curve gets the CU_PATH set now,
		 *		currently for paths to work it needs to go through the bevlist/displist system (ton) 
		 */

#ifdef CYCLIC_DEPENDENCY_WORKAROUND
		if (ct->tar->curve_cache == NULL) {
			BKE_displist_make_curveTypes(cob->scene, ct->tar, false);
		}
#endif

		if (ct->tar->curve_cache->path && ct->tar->curve_cache->path->data) {
			float quat[4];
			if ((data->followflag & FOLLOWPATH_STATIC) == 0) {
				/* animated position along curve depending on time */
				Nurb *nu = cu->nurb.first;
				curvetime = cu->ctime - data->offset;
				
				/* ctime is now a proper var setting of Curve which gets set by Animato like any other var that's animated,
				 * but this will only work if it actually is animated... 
				 *
				 * we divide the curvetime calculated in the previous step by the length of the path, to get a time
				 * factor, which then gets clamped to lie within 0.0 - 1.0 range
				 */
				curvetime /= cu->pathlen;

				if (nu && nu->flagu & CU_NURB_CYCLIC) {
					/* If the curve is cyclic, enable looping around if the time is
					 * outside the bounds 0..1 */
					if ((curvetime < 0.0f) || (curvetime > 1.0f)) {
						curvetime -= floorf(curvetime);
					}
				}
				else {
					/* The curve is not cyclic, so clamp to the begin/end points. */
					CLAMP(curvetime, 0.0f, 1.0f);
				}
			}
			else {
				/* fixed position along curve */
				curvetime = data->offset_fac;
			}
			
			if (where_on_path(ct->tar, curvetime, vec, dir, (data->followflag & FOLLOWPATH_FOLLOW) ? quat : NULL, &radius, NULL) ) {  /* quat_pt is quat or NULL*/
				float totmat[4][4];
				unit_m4(totmat);

				if (data->followflag & FOLLOWPATH_FOLLOW) {
#if 0
					float x1, q[4];
					vec_to_quat(quat, dir, (short)data->trackflag, (short)data->upflag);
					
					normalize_v3(dir);
					q[0] = cosf(0.5 * vec[3]);
					x1 = sinf(0.5 * vec[3]);
					q[1] = -x1 * dir[0];
					q[2] = -x1 * dir[1];
					q[3] = -x1 * dir[2];
					mul_qt_qtqt(quat, q, quat);
#else
					quat_apply_track(quat, data->trackflag, data->upflag);
#endif

					quat_to_mat4(totmat, quat);
				}

				if (data->followflag & FOLLOWPATH_RADIUS) {
					float tmat[4][4], rmat[4][4];
					scale_m4_fl(tmat, radius);
					mul_m4_m4m4(rmat, tmat, totmat);
					copy_m4_m4(totmat, rmat);
				}
				
				copy_v3_v3(totmat[3], vec);
				
				mul_m4_m4m4(ct->matrix, ct->tar->obmat, totmat);
			}
		}
	}
	else if (ct)
		unit_m4(ct->matrix);
}

static void followpath_evaluate(bConstraint *con, bConstraintOb *cob, ListBase *targets)
{
	bConstraintTarget *ct = targets->first;
	
	/* only evaluate if there is a target */
	if (VALID_CONS_TARGET(ct)) {
		float obmat[4][4];
		float size[3];
		bFollowPathConstraint *data = con->data;
		
		/* get Object transform (loc/rot/size) to determine transformation from path */
		/* TODO: this used to be local at one point, but is probably more useful as-is */
		copy_m4_m4(obmat, cob->matrix);
		
		/* get scaling of object before applying constraint */
		mat4_to_size(size, cob->matrix);
		
		/* apply targetmat - containing location on path, and rotation */
		mul_m4_m4m4(cob->matrix, ct->matrix, obmat);
		
		/* un-apply scaling caused by path */
		if ((data->followflag & FOLLOWPATH_RADIUS) == 0) { /* XXX - assume that scale correction means that radius will have some scale error in it - Campbell */
			float obsize[3];
			
			mat4_to_size(obsize, cob->matrix);
			if (obsize[0])
				mul_v3_fl(cob->matrix[0], size[0] / obsize[0]);
			if (obsize[1])
				mul_v3_fl(cob->matrix[1], size[1] / obsize[1]);
			if (obsize[2])
				mul_v3_fl(cob->matrix[2], size[2] / obsize[2]);
		}
	}
}

static bConstraintTypeInfo CTI_FOLLOWPATH = {
	CONSTRAINT_TYPE_FOLLOWPATH, /* type */
	sizeof(bFollowPathConstraint), /* size */
	"Follow Path", /* name */
	"bFollowPathConstraint", /* struct name */
	NULL, /* free data */
	followpath_id_looper, /* id looper */
	NULL, /* copy data */
	followpath_new_data, /* new data */
	followpath_get_tars, /* get constraint targets */
	followpath_flush_tars, /* flush constraint targets */
	followpath_get_tarmat, /* get target matrix */
	followpath_evaluate /* evaluate */
};

/* --------- Limit Location --------- */


static void loclimit_evaluate(bConstraint *con, bConstraintOb *cob, ListBase *UNUSED(targets))
{
	bLocLimitConstraint *data = con->data;
	
	if (data->flag & LIMIT_XMIN) {
		if (cob->matrix[3][0] < data->xmin)
			cob->matrix[3][0] = data->xmin;
	}
	if (data->flag & LIMIT_XMAX) {
		if (cob->matrix[3][0] > data->xmax)
			cob->matrix[3][0] = data->xmax;
	}
	if (data->flag & LIMIT_YMIN) {
		if (cob->matrix[3][1] < data->ymin)
			cob->matrix[3][1] = data->ymin;
	}
	if (data->flag & LIMIT_YMAX) {
		if (cob->matrix[3][1] > data->ymax)
			cob->matrix[3][1] = data->ymax;
	}
	if (data->flag & LIMIT_ZMIN) {
		if (cob->matrix[3][2] < data->zmin) 
			cob->matrix[3][2] = data->zmin;
	}
	if (data->flag & LIMIT_ZMAX) {
		if (cob->matrix[3][2] > data->zmax)
			cob->matrix[3][2] = data->zmax;
	}
}

static bConstraintTypeInfo CTI_LOCLIMIT = {
	CONSTRAINT_TYPE_LOCLIMIT, /* type */
	sizeof(bLocLimitConstraint), /* size */
	"Limit Location", /* name */
	"bLocLimitConstraint", /* struct name */
	NULL, /* free data */
	NULL, /* id looper */
	NULL, /* copy data */
	NULL, /* new data */
	NULL, /* get constraint targets */
	NULL, /* flush constraint targets */
	NULL, /* get target matrix */
	loclimit_evaluate /* evaluate */
};

/* -------- Limit Rotation --------- */

static void rotlimit_evaluate(bConstraint *con, bConstraintOb *cob, ListBase *UNUSED(targets))
{
	bRotLimitConstraint *data = con->data;
	float loc[3];
	float eul[3];
	float size[3];
	
	copy_v3_v3(loc, cob->matrix[3]);
	mat4_to_size(size, cob->matrix);

	mat4_to_eulO(eul, cob->rotOrder, cob->matrix);

	/* constraint data uses radians internally */
	
	/* limiting of euler values... */
	if (data->flag & LIMIT_XROT) {
		if (eul[0] < data->xmin) 
			eul[0] = data->xmin;
			
		if (eul[0] > data->xmax)
			eul[0] = data->xmax;
	}
	if (data->flag & LIMIT_YROT) {
		if (eul[1] < data->ymin)
			eul[1] = data->ymin;
			
		if (eul[1] > data->ymax)
			eul[1] = data->ymax;
	}
	if (data->flag & LIMIT_ZROT) {
		if (eul[2] < data->zmin)
			eul[2] = data->zmin;
			
		if (eul[2] > data->zmax)
			eul[2] = data->zmax;
	}
		
	loc_eulO_size_to_mat4(cob->matrix, loc, eul, size, cob->rotOrder);
}

static bConstraintTypeInfo CTI_ROTLIMIT = {
	CONSTRAINT_TYPE_ROTLIMIT, /* type */
	sizeof(bRotLimitConstraint), /* size */
	"Limit Rotation", /* name */
	"bRotLimitConstraint", /* struct name */
	NULL, /* free data */
	NULL, /* id looper */
	NULL, /* copy data */
	NULL, /* new data */
	NULL, /* get constraint targets */
	NULL, /* flush constraint targets */
	NULL, /* get target matrix */
	rotlimit_evaluate /* evaluate */
};

/* --------- Limit Scale --------- */


static void sizelimit_evaluate(bConstraint *con, bConstraintOb *cob, ListBase *UNUSED(targets))
{
	bSizeLimitConstraint *data = con->data;
	float obsize[3], size[3];
	
	mat4_to_size(size, cob->matrix);
	mat4_to_size(obsize, cob->matrix);
	
	if (data->flag & LIMIT_XMIN) {
		if (size[0] < data->xmin) 
			size[0] = data->xmin;
	}
	if (data->flag & LIMIT_XMAX) {
		if (size[0] > data->xmax) 
			size[0] = data->xmax;
	}
	if (data->flag & LIMIT_YMIN) {
		if (size[1] < data->ymin) 
			size[1] = data->ymin;
	}
	if (data->flag & LIMIT_YMAX) {
		if (size[1] > data->ymax) 
			size[1] = data->ymax;
	}
	if (data->flag & LIMIT_ZMIN) {
		if (size[2] < data->zmin) 
			size[2] = data->zmin;
	}
	if (data->flag & LIMIT_ZMAX) {
		if (size[2] > data->zmax) 
			size[2] = data->zmax;
	}
	
	if (obsize[0]) 
		mul_v3_fl(cob->matrix[0], size[0] / obsize[0]);
	if (obsize[1]) 
		mul_v3_fl(cob->matrix[1], size[1] / obsize[1]);
	if (obsize[2]) 
		mul_v3_fl(cob->matrix[2], size[2] / obsize[2]);
}

static bConstraintTypeInfo CTI_SIZELIMIT = {
	CONSTRAINT_TYPE_SIZELIMIT, /* type */
	sizeof(bSizeLimitConstraint), /* size */
	"Limit Scale", /* name */
	"bSizeLimitConstraint", /* struct name */
	NULL, /* free data */
	NULL, /* id looper */
	NULL, /* copy data */
	NULL, /* new data */
	NULL, /* get constraint targets */
	NULL, /* flush constraint targets */
	NULL, /* get target matrix */
	sizelimit_evaluate /* evaluate */
};

/* ----------- Copy Location ------------- */

static void loclike_new_data(void *cdata)
{
	bLocateLikeConstraint *data = (bLocateLikeConstraint *)cdata;
	
	data->flag = LOCLIKE_X | LOCLIKE_Y | LOCLIKE_Z;
}

static void loclike_id_looper(bConstraint *con, ConstraintIDFunc func, void *userdata)
{
	bLocateLikeConstraint *data = con->data;
	
	/* target only */
	func(con, (ID **)&data->tar, false, userdata);
}

static int loclike_get_tars(bConstraint *con, ListBase *list)
{
	if (con && list) {
		bLocateLikeConstraint *data = con->data;
		bConstraintTarget *ct;
		
		/* standard target-getting macro for single-target constraints */
		SINGLETARGET_GET_TARS(con, data->tar, data->subtarget, ct, list);
		
		return 1;
	}
	
	return 0;
}

static void loclike_flush_tars(bConstraint *con, ListBase *list, bool no_copy)
{
	if (con && list) {
		bLocateLikeConstraint *data = con->data;
		bConstraintTarget *ct = list->first;
		
		/* the following macro is used for all standard single-target constraints */
		SINGLETARGET_FLUSH_TARS(con, data->tar, data->subtarget, ct, list, no_copy);
	}
}

static void loclike_evaluate(bConstraint *con, bConstraintOb *cob, ListBase *targets)
{
	bLocateLikeConstraint *data = con->data;
	bConstraintTarget *ct = targets->first;
	
	if (VALID_CONS_TARGET(ct)) {
		float offset[3] = {0.0f, 0.0f, 0.0f};
		
		if (data->flag & LOCLIKE_OFFSET)
			copy_v3_v3(offset, cob->matrix[3]);
			
		if (data->flag & LOCLIKE_X) {
			cob->matrix[3][0] = ct->matrix[3][0];
			
			if (data->flag & LOCLIKE_X_INVERT) cob->matrix[3][0] *= -1;
			cob->matrix[3][0] += offset[0];
		}
		if (data->flag & LOCLIKE_Y) {
			cob->matrix[3][1] = ct->matrix[3][1];
			
			if (data->flag & LOCLIKE_Y_INVERT) cob->matrix[3][1] *= -1;
			cob->matrix[3][1] += offset[1];
		}
		if (data->flag & LOCLIKE_Z) {
			cob->matrix[3][2] = ct->matrix[3][2];
			
			if (data->flag & LOCLIKE_Z_INVERT) cob->matrix[3][2] *= -1;
			cob->matrix[3][2] += offset[2];
		}
	}
}

static bConstraintTypeInfo CTI_LOCLIKE = {
	CONSTRAINT_TYPE_LOCLIKE, /* type */
	sizeof(bLocateLikeConstraint), /* size */
	"Copy Location", /* name */
	"bLocateLikeConstraint", /* struct name */
	NULL, /* free data */
	loclike_id_looper, /* id looper */
	NULL, /* copy data */
	loclike_new_data, /* new data */
	loclike_get_tars, /* get constraint targets */
	loclike_flush_tars, /* flush constraint targets */
	default_get_tarmat, /* get target matrix */
	loclike_evaluate /* evaluate */
};

/* ----------- Copy Rotation ------------- */

static void rotlike_new_data(void *cdata)
{
	bRotateLikeConstraint *data = (bRotateLikeConstraint *)cdata;
	
	data->flag = ROTLIKE_X | ROTLIKE_Y | ROTLIKE_Z;
}

static void rotlike_id_looper(bConstraint *con, ConstraintIDFunc func, void *userdata)
{
	bRotateLikeConstraint *data = con->data;
	
	/* target only */
	func(con, (ID **)&data->tar, false, userdata);
}

static int rotlike_get_tars(bConstraint *con, ListBase *list)
{
	if (con && list) {
		bRotateLikeConstraint *data = con->data;
		bConstraintTarget *ct;
		
		/* standard target-getting macro for single-target constraints */
		SINGLETARGET_GET_TARS(con, data->tar, data->subtarget, ct, list);
		
		return 1;
	}
	
	return 0;
}

static void rotlike_flush_tars(bConstraint *con, ListBase *list, bool no_copy)
{
	if (con && list) {
		bRotateLikeConstraint *data = con->data;
		bConstraintTarget *ct = list->first;
		
		/* the following macro is used for all standard single-target constraints */
		SINGLETARGET_FLUSH_TARS(con, data->tar, data->subtarget, ct, list, no_copy);
	}
}

static void rotlike_evaluate(bConstraint *con, bConstraintOb *cob, ListBase *targets)
{
	bRotateLikeConstraint *data = con->data;
	bConstraintTarget *ct = targets->first;
	
	if (VALID_CONS_TARGET(ct)) {
		float loc[3];
		float eul[3], obeul[3];
		float size[3];
		
		copy_v3_v3(loc, cob->matrix[3]);
		mat4_to_size(size, cob->matrix);
		
		/* to allow compatible rotations, must get both rotations in the order of the owner... */
		mat4_to_eulO(obeul, cob->rotOrder, cob->matrix);
		/* we must get compatible eulers from the beginning because some of them can be modified below (see bug #21875) */
		mat4_to_compatible_eulO(eul, obeul, cob->rotOrder, ct->matrix);
		
		if ((data->flag & ROTLIKE_X) == 0)
			eul[0] = obeul[0];
		else {
			if (data->flag & ROTLIKE_OFFSET)
				rotate_eulO(eul, cob->rotOrder, 'X', obeul[0]);
			
			if (data->flag & ROTLIKE_X_INVERT)
				eul[0] *= -1;
		}
		
		if ((data->flag & ROTLIKE_Y) == 0)
			eul[1] = obeul[1];
		else {
			if (data->flag & ROTLIKE_OFFSET)
				rotate_eulO(eul, cob->rotOrder, 'Y', obeul[1]);
			
			if (data->flag & ROTLIKE_Y_INVERT)
				eul[1] *= -1;
		}
		
		if ((data->flag & ROTLIKE_Z) == 0)
			eul[2] = obeul[2];
		else {
			if (data->flag & ROTLIKE_OFFSET)
				rotate_eulO(eul, cob->rotOrder, 'Z', obeul[2]);
			
			if (data->flag & ROTLIKE_Z_INVERT)
				eul[2] *= -1;
		}
		
		/* good to make eulers compatible again, since we don't know how much they were changed above */
		compatible_eul(eul, obeul);
		loc_eulO_size_to_mat4(cob->matrix, loc, eul, size, cob->rotOrder);
	}
}

static bConstraintTypeInfo CTI_ROTLIKE = {
	CONSTRAINT_TYPE_ROTLIKE, /* type */
	sizeof(bRotateLikeConstraint), /* size */
	"Copy Rotation", /* name */
	"bRotateLikeConstraint", /* struct name */
	NULL, /* free data */
	rotlike_id_looper, /* id looper */
	NULL, /* copy data */
	rotlike_new_data, /* new data */
	rotlike_get_tars, /* get constraint targets */
	rotlike_flush_tars, /* flush constraint targets */
	default_get_tarmat, /* get target matrix */
	rotlike_evaluate /* evaluate */
};

/* ---------- Copy Scale ---------- */

static void sizelike_new_data(void *cdata)
{
	bSizeLikeConstraint *data = (bSizeLikeConstraint *)cdata;
	
	data->flag = SIZELIKE_X | SIZELIKE_Y | SIZELIKE_Z;
}

static void sizelike_id_looper(bConstraint *con, ConstraintIDFunc func, void *userdata)
{
	bSizeLikeConstraint *data = con->data;
	
	/* target only */
	func(con, (ID **)&data->tar, false, userdata);
}

static int sizelike_get_tars(bConstraint *con, ListBase *list)
{
	if (con && list) {
		bSizeLikeConstraint *data = con->data;
		bConstraintTarget *ct;
		
		/* standard target-getting macro for single-target constraints */
		SINGLETARGET_GET_TARS(con, data->tar, data->subtarget, ct, list);
		
		return 1;
	}
	
	return 0;
}

static void sizelike_flush_tars(bConstraint *con, ListBase *list, bool no_copy)
{
	if (con && list) {
		bSizeLikeConstraint *data = con->data;
		bConstraintTarget *ct = list->first;
		
		/* the following macro is used for all standard single-target constraints */
		SINGLETARGET_FLUSH_TARS(con, data->tar, data->subtarget, ct, list, no_copy);
	}
}

static void sizelike_evaluate(bConstraint *con, bConstraintOb *cob, ListBase *targets)
{
	bSizeLikeConstraint *data = con->data;
	bConstraintTarget *ct = targets->first;
	
	if (VALID_CONS_TARGET(ct)) {
		float obsize[3], size[3];
		
		mat4_to_size(size, ct->matrix);
		mat4_to_size(obsize, cob->matrix);
		
		if ((data->flag & SIZELIKE_X) && (obsize[0] != 0)) {
			if (data->flag & SIZELIKE_OFFSET) {
				size[0] += (obsize[0] - 1.0f);
				mul_v3_fl(cob->matrix[0], size[0] / obsize[0]);
			}
			else
				mul_v3_fl(cob->matrix[0], size[0] / obsize[0]);
		}
		if ((data->flag & SIZELIKE_Y) && (obsize[1] != 0)) {
			if (data->flag & SIZELIKE_OFFSET) {
				size[1] += (obsize[1] - 1.0f);
				mul_v3_fl(cob->matrix[1], size[1] / obsize[1]);
			}
			else
				mul_v3_fl(cob->matrix[1], size[1] / obsize[1]);
		}
		if ((data->flag & SIZELIKE_Z) && (obsize[2] != 0)) {
			if (data->flag & SIZELIKE_OFFSET) {
				size[2] += (obsize[2] - 1.0f);
				mul_v3_fl(cob->matrix[2], size[2] / obsize[2]);
			}
			else
				mul_v3_fl(cob->matrix[2], size[2] / obsize[2]);
		}
	}
}

static bConstraintTypeInfo CTI_SIZELIKE = {
	CONSTRAINT_TYPE_SIZELIKE, /* type */
	sizeof(bSizeLikeConstraint), /* size */
	"Copy Scale", /* name */
	"bSizeLikeConstraint", /* struct name */
	NULL, /* free data */
	sizelike_id_looper, /* id looper */
	NULL, /* copy data */
	sizelike_new_data, /* new data */
	sizelike_get_tars, /* get constraint targets */
	sizelike_flush_tars, /* flush constraint targets */
	default_get_tarmat, /* get target matrix */
	sizelike_evaluate /* evaluate */
};

/* ----------- Copy Transforms ------------- */

static void translike_id_looper(bConstraint *con, ConstraintIDFunc func, void *userdata)
{
	bTransLikeConstraint *data = con->data;
	
	/* target only */
	func(con, (ID **)&data->tar, false, userdata);
}

static int translike_get_tars(bConstraint *con, ListBase *list)
{
	if (con && list) {
		bTransLikeConstraint *data = con->data;
		bConstraintTarget *ct;
		
		/* standard target-getting macro for single-target constraints */
		SINGLETARGET_GET_TARS(con, data->tar, data->subtarget, ct, list);
		
		return 1;
	}
	
	return 0;
}

static void translike_flush_tars(bConstraint *con, ListBase *list, bool no_copy)
{
	if (con && list) {
		bTransLikeConstraint *data = con->data;
		bConstraintTarget *ct = list->first;
		
		/* the following macro is used for all standard single-target constraints */
		SINGLETARGET_FLUSH_TARS(con, data->tar, data->subtarget, ct, list, no_copy);
	}
}

static void translike_evaluate(bConstraint *UNUSED(con), bConstraintOb *cob, ListBase *targets)
{
	bConstraintTarget *ct = targets->first;
	
	if (VALID_CONS_TARGET(ct)) {
		/* just copy the entire transform matrix of the target */
		copy_m4_m4(cob->matrix, ct->matrix);
	}
}

static bConstraintTypeInfo CTI_TRANSLIKE = {
	CONSTRAINT_TYPE_TRANSLIKE, /* type */
	sizeof(bTransLikeConstraint), /* size */
	"Copy Transforms", /* name */
	"bTransLikeConstraint", /* struct name */
	NULL, /* free data */
	translike_id_looper, /* id looper */
	NULL, /* copy data */
	NULL, /* new data */
	translike_get_tars, /* get constraint targets */
	translike_flush_tars, /* flush constraint targets */
	default_get_tarmat, /* get target matrix */
	translike_evaluate /* evaluate */
};

/* ---------- Maintain Volume ---------- */

static void samevolume_new_data(void *cdata)
{
	bSameVolumeConstraint *data = (bSameVolumeConstraint *)cdata;

	data->flag = SAMEVOL_Y;
	data->volume = 1.0f;
}

static void samevolume_evaluate(bConstraint *con, bConstraintOb *cob, ListBase *UNUSED(targets))
{
	bSameVolumeConstraint *data = con->data;

	float volume = data->volume;
	float fac = 1.0f;
	float obsize[3];

	mat4_to_size(obsize, cob->matrix);
	
	/* calculate normalizing scale factor for non-essential values */
	if (obsize[data->flag] != 0) 
		fac = sqrtf(volume / obsize[data->flag]) / obsize[data->flag];
	
	/* apply scaling factor to the channels not being kept */
	switch (data->flag) {
		case SAMEVOL_X:
			mul_v3_fl(cob->matrix[1], fac);
			mul_v3_fl(cob->matrix[2], fac);
			break;
		case SAMEVOL_Y:
			mul_v3_fl(cob->matrix[0], fac);
			mul_v3_fl(cob->matrix[2], fac);
			break;
		case SAMEVOL_Z:
			mul_v3_fl(cob->matrix[0], fac);
			mul_v3_fl(cob->matrix[1], fac);
			break;
	}
}

static bConstraintTypeInfo CTI_SAMEVOL = {
	CONSTRAINT_TYPE_SAMEVOL, /* type */
	sizeof(bSameVolumeConstraint), /* size */
	"Maintain Volume", /* name */
	"bSameVolumeConstraint", /* struct name */
	NULL, /* free data */
	NULL, /* id looper */
	NULL, /* copy data */
	samevolume_new_data, /* new data */
	NULL, /* get constraint targets */
	NULL, /* flush constraint targets */
	NULL, /* get target matrix */
	samevolume_evaluate /* evaluate */
};

/* ----------- Python Constraint -------------- */

static void pycon_free(bConstraint *con)
{
	bPythonConstraint *data = con->data;
	
	/* id-properties */
	IDP_FreeProperty(data->prop);
	MEM_freeN(data->prop);
	
	/* multiple targets */
	BLI_freelistN(&data->targets);
}	

static void pycon_copy(bConstraint *con, bConstraint *srccon)
{
	bPythonConstraint *pycon = (bPythonConstraint *)con->data;
	bPythonConstraint *opycon = (bPythonConstraint *)srccon->data;
	
	pycon->prop = IDP_CopyProperty(opycon->prop);
	BLI_duplicatelist(&pycon->targets, &opycon->targets);
}

static void pycon_new_data(void *cdata)
{
	bPythonConstraint *data = (bPythonConstraint *)cdata;
	
	/* everything should be set correctly by calloc, except for the prop->type constant.*/
	data->prop = MEM_callocN(sizeof(IDProperty), "PyConstraintProps");
	data->prop->type = IDP_GROUP;
}

static int pycon_get_tars(bConstraint *con, ListBase *list)
{
	if (con && list) {
		bPythonConstraint *data = con->data;
		
		list->first = data->targets.first;
		list->last = data->targets.last;
		
		return data->tarnum;
	}
	
	return 0;
}

static void pycon_id_looper(bConstraint *con, ConstraintIDFunc func, void *userdata)
{
	bPythonConstraint *data = con->data;
	bConstraintTarget *ct;
	
	/* targets */
	for (ct = data->targets.first; ct; ct = ct->next)
		func(con, (ID **)&ct->tar, false, userdata);
		
	/* script */
	func(con, (ID **)&data->text, true, userdata);
}

/* Whether this approach is maintained remains to be seen (aligorith) */
static void pycon_get_tarmat(bConstraint *con, bConstraintOb *cob, bConstraintTarget *ct, float UNUSED(ctime))
{
#ifdef WITH_PYTHON
	bPythonConstraint *data = con->data;
#endif

	if (VALID_CONS_TARGET(ct)) {
#ifdef CYCLIC_DEPENDENCY_WORKAROUND
		/* special exception for curves - depsgraph issues */
		if (ct->tar->type == OB_CURVE) {
			if (ct->tar->curve_cache == NULL) {
				BKE_displist_make_curveTypes(cob->scene, ct->tar, false);
			}
		}
#endif

		/* firstly calculate the matrix the normal way, then let the py-function override
		 * this matrix if it needs to do so
		 */
		constraint_target_to_mat4(ct->tar, ct->subtarget, ct->matrix, CONSTRAINT_SPACE_WORLD, ct->space, con->flag, con->headtail);
		
		/* only execute target calculation if allowed */
#ifdef WITH_PYTHON
		if (G.f & G_SCRIPT_AUTOEXEC)
			BPY_pyconstraint_target(data, ct);
#endif
	}
	else if (ct)
		unit_m4(ct->matrix);
}

static void pycon_evaluate(bConstraint *con, bConstraintOb *cob, ListBase *targets)
{
#ifndef WITH_PYTHON
	UNUSED_VARS(con, cob, targets);
	return;
#else
	bPythonConstraint *data = con->data;
	
	/* only evaluate in python if we're allowed to do so */
	if ((G.f & G_SCRIPT_AUTOEXEC) == 0) return;
	
/* currently removed, until I this can be re-implemented for multiple targets */
#if 0
	/* Firstly, run the 'driver' function which has direct access to the objects involved 
	 * Technically, this is potentially dangerous as users may abuse this and cause dependency-problems,
	 * but it also allows certain 'clever' rigging hacks to work.
	 */
	BPY_pyconstraint_driver(data, cob, targets);
#endif
	
	/* Now, run the actual 'constraint' function, which should only access the matrices */
	BPY_pyconstraint_exec(data, cob, targets);
#endif /* WITH_PYTHON */
}

static bConstraintTypeInfo CTI_PYTHON = {
	CONSTRAINT_TYPE_PYTHON, /* type */
	sizeof(bPythonConstraint), /* size */
	"Script", /* name */
	"bPythonConstraint", /* struct name */
	pycon_free, /* free data */
	pycon_id_looper, /* id looper */
	pycon_copy, /* copy data */
	pycon_new_data, /* new data */
	pycon_get_tars, /* get constraint targets */
	NULL, /* flush constraint targets */
	pycon_get_tarmat, /* get target matrix */
	pycon_evaluate /* evaluate */
};

/* -------- Action Constraint ----------- */

static void actcon_new_data(void *cdata)
{
	bActionConstraint *data = (bActionConstraint *)cdata;
	
	/* set type to 20 (Loc X), as 0 is Rot X for backwards compatibility */
	data->type = 20;
}

static void actcon_id_looper(bConstraint *con, ConstraintIDFunc func, void *userdata)
{
	bActionConstraint *data = con->data;
	
	/* target */
	func(con, (ID **)&data->tar, false, userdata);
	
	/* action */
	func(con, (ID **)&data->act, true, userdata);
}

static int actcon_get_tars(bConstraint *con, ListBase *list)
{
	if (con && list) {
		bActionConstraint *data = con->data;
		bConstraintTarget *ct;
		
		/* standard target-getting macro for single-target constraints */
		SINGLETARGET_GET_TARS(con, data->tar, data->subtarget, ct, list);
		
		return 1;
	}
	
	return 0;
}

static void actcon_flush_tars(bConstraint *con, ListBase *list, bool no_copy)
{
	if (con && list) {
		bActionConstraint *data = con->data;
		bConstraintTarget *ct = list->first;
		
		/* the following macro is used for all standard single-target constraints */
		SINGLETARGET_FLUSH_TARS(con, data->tar, data->subtarget, ct, list, no_copy);
	}
}

static void actcon_get_tarmat(bConstraint *con, bConstraintOb *cob, bConstraintTarget *ct, float UNUSED(ctime))
{
	bActionConstraint *data = con->data;
	
	if (VALID_CONS_TARGET(ct)) {
		float tempmat[4][4], vec[3];
		float s, t;
		short axis;
		
		/* initialize return matrix */
		unit_m4(ct->matrix);
		
		/* get the transform matrix of the target */
		constraint_target_to_mat4(ct->tar, ct->subtarget, tempmat, CONSTRAINT_SPACE_WORLD, ct->space, con->flag, con->headtail);
		
		/* determine where in transform range target is */
		/* data->type is mapped as follows for backwards compatibility:
		 *	00,01,02	- rotation (it used to be like this)
		 *  10,11,12	- scaling
		 *	20,21,22	- location
		 */
		if (data->type < 10) {
			/* extract rotation (is in whatever space target should be in) */
			mat4_to_eul(vec, tempmat);
			mul_v3_fl(vec, RAD2DEGF(1.0f)); /* rad -> deg */
			axis = data->type;
		}
		else if (data->type < 20) {
			/* extract scaling (is in whatever space target should be in) */
			mat4_to_size(vec, tempmat);
			axis = data->type - 10;
		}
		else {
			/* extract location */
			copy_v3_v3(vec, tempmat[3]);
			axis = data->type - 20;
		}
		
		BLI_assert((unsigned int)axis < 3);

		/* Target defines the animation */
		s = (vec[axis] - data->min) / (data->max - data->min);
		CLAMP(s, 0, 1);
		t = (s * (data->end - data->start)) + data->start;
		
		if (G.debug & G_DEBUG)
			printf("do Action Constraint %s - Ob %s Pchan %s\n", con->name, cob->ob->id.name + 2, (cob->pchan) ? cob->pchan->name : NULL);
		
		/* Get the appropriate information from the action */
		if (cob->type == CONSTRAINT_OBTYPE_OBJECT || (data->flag & ACTCON_BONE_USE_OBJECT_ACTION)) {
			Object workob;
			
			/* evaluate using workob */
			/* FIXME: we don't have any consistent standards on limiting effects on object... */
			what_does_obaction(cob->ob, &workob, NULL, data->act, NULL, t);
			BKE_object_to_mat4(&workob, ct->matrix);
		}
		else if (cob->type == CONSTRAINT_OBTYPE_BONE) {
			Object workob;
			bPose pose = {{0}};
			bPoseChannel *pchan, *tchan;

			/* make a copy of the bone of interest in the temp pose before evaluating action, so that it can get set 
			 *	- we need to manually copy over a few settings, including rotation order, otherwise this fails
			 */
			pchan = cob->pchan;
			
			tchan = BKE_pose_channel_verify(&pose, pchan->name);
			tchan->rotmode = pchan->rotmode;
			
			/* evaluate action using workob (it will only set the PoseChannel in question) */
			what_does_obaction(cob->ob, &workob, &pose, data->act, pchan->name, t);
			
			/* convert animation to matrices for use here */
			BKE_pchan_calc_mat(tchan);
			copy_m4_m4(ct->matrix, tchan->chan_mat);
			
			/* Clean up */
			BKE_pose_free_data(&pose);
		}
		else {
			/* behavior undefined... */
			puts("Error: unknown owner type for Action Constraint");
		}
	}
}

static void actcon_evaluate(bConstraint *UNUSED(con), bConstraintOb *cob, ListBase *targets)
{
	bConstraintTarget *ct = targets->first;
	
	if (VALID_CONS_TARGET(ct)) {
		float temp[4][4];
		
		/* Nice and simple... we just need to multiply the matrices, as the get_target_matrix
		 * function has already taken care of everything else.
		 */
		copy_m4_m4(temp, cob->matrix);
		mul_m4_m4m4(cob->matrix, temp, ct->matrix);
	}
}

static bConstraintTypeInfo CTI_ACTION = {
	CONSTRAINT_TYPE_ACTION, /* type */
	sizeof(bActionConstraint), /* size */
	"Action", /* name */
	"bActionConstraint", /* struct name */
	NULL, /* free data */
	actcon_id_looper, /* id looper */
	NULL, /* copy data */
	actcon_new_data, /* new data */
	actcon_get_tars, /* get constraint targets */
	actcon_flush_tars, /* flush constraint targets */
	actcon_get_tarmat, /* get target matrix */
	actcon_evaluate /* evaluate */
};

/* --------- Locked Track ---------- */

static void locktrack_new_data(void *cdata)
{
	bLockTrackConstraint *data = (bLockTrackConstraint *)cdata;
	
	data->trackflag = TRACK_Y;
	data->lockflag = LOCK_Z;
}	

static void locktrack_id_looper(bConstraint *con, ConstraintIDFunc func, void *userdata)
{
	bLockTrackConstraint *data = con->data;
	
	/* target only */
	func(con, (ID **)&data->tar, false, userdata);
}

static int locktrack_get_tars(bConstraint *con, ListBase *list)
{
	if (con && list) {
		bLockTrackConstraint *data = con->data;
		bConstraintTarget *ct;
		
		/* the following macro is used for all standard single-target constraints */
		SINGLETARGET_GET_TARS(con, data->tar, data->subtarget, ct, list);
		
		return 1;
	}
	
	return 0;
}

static void locktrack_flush_tars(bConstraint *con, ListBase *list, bool no_copy)
{
	if (con && list) {
		bLockTrackConstraint *data = con->data;
		bConstraintTarget *ct = list->first;
		
		/* the following macro is used for all standard single-target constraints */
		SINGLETARGET_FLUSH_TARS(con, data->tar, data->subtarget, ct, list, no_copy);
	}
}

static void locktrack_evaluate(bConstraint *con, bConstraintOb *cob, ListBase *targets)
{
	bLockTrackConstraint *data = con->data;
	bConstraintTarget *ct = targets->first;
	
	if (VALID_CONS_TARGET(ct)) {
		float vec[3], vec2[3];
		float totmat[3][3];
		float tmpmat[3][3];
		float invmat[3][3];
		float mdet;
		
		/* Vector object -> target */
		sub_v3_v3v3(vec, ct->matrix[3], cob->matrix[3]);
		switch (data->lockflag) {
			case LOCK_X: /* LOCK X */
			{
				switch (data->trackflag) {
					case TRACK_Y: /* LOCK X TRACK Y */
					{
						/* Projection of Vector on the plane */
						project_v3_v3v3(vec2, vec, cob->matrix[0]);
						sub_v3_v3v3(totmat[1], vec, vec2);
						normalize_v3(totmat[1]);
					
						/* the x axis is fixed */
						normalize_v3_v3(totmat[0], cob->matrix[0]);
					
						/* the z axis gets mapped onto a third orthogonal vector */
						cross_v3_v3v3(totmat[2], totmat[0], totmat[1]);
						break;
					}
					case TRACK_Z: /* LOCK X TRACK Z */
					{
						/* Projection of Vector on the plane */
						project_v3_v3v3(vec2, vec, cob->matrix[0]);
						sub_v3_v3v3(totmat[2], vec, vec2);
						normalize_v3(totmat[2]);
					
						/* the x axis is fixed */
						normalize_v3_v3(totmat[0], cob->matrix[0]);
					
						/* the z axis gets mapped onto a third orthogonal vector */
						cross_v3_v3v3(totmat[1], totmat[2], totmat[0]);
						break;
					}
					case TRACK_nY: /* LOCK X TRACK -Y */
					{
						/* Projection of Vector on the plane */
						project_v3_v3v3(vec2, vec, cob->matrix[0]);
						sub_v3_v3v3(totmat[1], vec, vec2);
						normalize_v3(totmat[1]);
						negate_v3(totmat[1]);
					
						/* the x axis is fixed */
						normalize_v3_v3(totmat[0], cob->matrix[0]);
					
						/* the z axis gets mapped onto a third orthogonal vector */
						cross_v3_v3v3(totmat[2], totmat[0], totmat[1]);
						break;
					}
					case TRACK_nZ: /* LOCK X TRACK -Z */
					{
						/* Projection of Vector on the plane */
						project_v3_v3v3(vec2, vec, cob->matrix[0]);
						sub_v3_v3v3(totmat[2], vec, vec2);
						normalize_v3(totmat[2]);
						negate_v3(totmat[2]);
						
						/* the x axis is fixed */
						normalize_v3_v3(totmat[0], cob->matrix[0]);
						
						/* the z axis gets mapped onto a third orthogonal vector */
						cross_v3_v3v3(totmat[1], totmat[2], totmat[0]);
						break;
					}
					default:
					{
						unit_m3(totmat);
						break;
					}
				}
				break;
			}
			case LOCK_Y: /* LOCK Y */
			{
				switch (data->trackflag) {
					case TRACK_X: /* LOCK Y TRACK X */
					{
						/* Projection of Vector on the plane */
						project_v3_v3v3(vec2, vec, cob->matrix[1]);
						sub_v3_v3v3(totmat[0], vec, vec2);
						normalize_v3(totmat[0]);
					
						/* the y axis is fixed */
						normalize_v3_v3(totmat[1], cob->matrix[1]);

						/* the z axis gets mapped onto a third orthogonal vector */
						cross_v3_v3v3(totmat[2], totmat[0], totmat[1]);
						break;
					}
					case TRACK_Z: /* LOCK Y TRACK Z */
					{
						/* Projection of Vector on the plane */
						project_v3_v3v3(vec2, vec, cob->matrix[1]);
						sub_v3_v3v3(totmat[2], vec, vec2);
						normalize_v3(totmat[2]);
					
						/* the y axis is fixed */
						normalize_v3_v3(totmat[1], cob->matrix[1]);
					
						/* the z axis gets mapped onto a third orthogonal vector */
						cross_v3_v3v3(totmat[0], totmat[1], totmat[2]);
						break;
					}
					case TRACK_nX: /* LOCK Y TRACK -X */
					{
						/* Projection of Vector on the plane */
						project_v3_v3v3(vec2, vec, cob->matrix[1]);
						sub_v3_v3v3(totmat[0], vec, vec2);
						normalize_v3(totmat[0]);
						negate_v3(totmat[0]);
					
						/* the y axis is fixed */
						normalize_v3_v3(totmat[1], cob->matrix[1]);
					
						/* the z axis gets mapped onto a third orthogonal vector */
						cross_v3_v3v3(totmat[2], totmat[0], totmat[1]);
						break;
					}
					case TRACK_nZ: /* LOCK Y TRACK -Z */
					{
						/* Projection of Vector on the plane */
						project_v3_v3v3(vec2, vec, cob->matrix[1]);
						sub_v3_v3v3(totmat[2], vec, vec2);
						normalize_v3(totmat[2]);
						negate_v3(totmat[2]);
					
						/* the y axis is fixed */
						normalize_v3_v3(totmat[1], cob->matrix[1]);
					
						/* the z axis gets mapped onto a third orthogonal vector */
						cross_v3_v3v3(totmat[0], totmat[1], totmat[2]);
						break;
					}
					default:
					{
						unit_m3(totmat);
						break;
					}
				}
				break;
			}
			case LOCK_Z: /* LOCK Z */
			{
				switch (data->trackflag) {
					case TRACK_X: /* LOCK Z TRACK X */
					{
						/* Projection of Vector on the plane */
						project_v3_v3v3(vec2, vec, cob->matrix[2]);
						sub_v3_v3v3(totmat[0], vec, vec2);
						normalize_v3(totmat[0]);
					
						/* the z axis is fixed */
						normalize_v3_v3(totmat[2], cob->matrix[2]);
					
						/* the x axis gets mapped onto a third orthogonal vector */
						cross_v3_v3v3(totmat[1], totmat[2], totmat[0]);
						break;
					}
					case TRACK_Y: /* LOCK Z TRACK Y */
					{
						/* Projection of Vector on the plane */
						project_v3_v3v3(vec2, vec, cob->matrix[2]);
						sub_v3_v3v3(totmat[1], vec, vec2);
						normalize_v3(totmat[1]);
					
						/* the z axis is fixed */
						normalize_v3_v3(totmat[2], cob->matrix[2]);
						
						/* the x axis gets mapped onto a third orthogonal vector */
						cross_v3_v3v3(totmat[0], totmat[1], totmat[2]);
						break;
					}
					case TRACK_nX: /* LOCK Z TRACK -X */
					{
						/* Projection of Vector on the plane */
						project_v3_v3v3(vec2, vec, cob->matrix[2]);
						sub_v3_v3v3(totmat[0], vec, vec2);
						normalize_v3(totmat[0]);
						negate_v3(totmat[0]);
					
						/* the z axis is fixed */
						normalize_v3_v3(totmat[2], cob->matrix[2]);
					
						/* the x axis gets mapped onto a third orthogonal vector */
						cross_v3_v3v3(totmat[1], totmat[2], totmat[0]);
						break;
					}
					case TRACK_nY: /* LOCK Z TRACK -Y */
					{
						/* Projection of Vector on the plane */
						project_v3_v3v3(vec2, vec, cob->matrix[2]);
						sub_v3_v3v3(totmat[1], vec, vec2);
						normalize_v3(totmat[1]);
						negate_v3(totmat[1]);
					
						/* the z axis is fixed */
						normalize_v3_v3(totmat[2], cob->matrix[2]);
						
						/* the x axis gets mapped onto a third orthogonal vector */
						cross_v3_v3v3(totmat[0], totmat[1], totmat[2]);
						break;
					}
					default:
					{
						unit_m3(totmat);
						break;
					}
				}
				break;
			}
			default:
			{
				unit_m3(totmat);
				break;
			}
		}
		/* Block to keep matrix heading */
		copy_m3_m4(tmpmat, cob->matrix);
		normalize_m3(tmpmat);
		invert_m3_m3(invmat, tmpmat);
		mul_m3_m3m3(tmpmat, totmat, invmat);
		totmat[0][0] = tmpmat[0][0]; totmat[0][1] = tmpmat[0][1]; totmat[0][2] = tmpmat[0][2];
		totmat[1][0] = tmpmat[1][0]; totmat[1][1] = tmpmat[1][1]; totmat[1][2] = tmpmat[1][2];
		totmat[2][0] = tmpmat[2][0]; totmat[2][1] = tmpmat[2][1]; totmat[2][2] = tmpmat[2][2];
		
		mdet = determinant_m3(totmat[0][0], totmat[0][1], totmat[0][2],
		                      totmat[1][0], totmat[1][1], totmat[1][2],
		                      totmat[2][0], totmat[2][1], totmat[2][2]);
		if (mdet == 0) {
			unit_m3(totmat);
		}
		
		/* apply out transformaton to the object */
		mul_m4_m3m4(cob->matrix, totmat, cob->matrix);
	}
}

static bConstraintTypeInfo CTI_LOCKTRACK = {
	CONSTRAINT_TYPE_LOCKTRACK, /* type */
	sizeof(bLockTrackConstraint), /* size */
	"Locked Track", /* name */
	"bLockTrackConstraint", /* struct name */
	NULL, /* free data */
	locktrack_id_looper, /* id looper */
	NULL, /* copy data */
	locktrack_new_data, /* new data */
	locktrack_get_tars, /* get constraint targets */
	locktrack_flush_tars, /* flush constraint targets */
	default_get_tarmat, /* get target matrix */
	locktrack_evaluate /* evaluate */
};

/* ---------- Limit Distance Constraint ----------- */

static void distlimit_new_data(void *cdata)
{
	bDistLimitConstraint *data = (bDistLimitConstraint *)cdata;
	
	data->dist = 0.0f;
}

static void distlimit_id_looper(bConstraint *con, ConstraintIDFunc func, void *userdata)
{
	bDistLimitConstraint *data = con->data;
	
	/* target only */
	func(con, (ID **)&data->tar, false, userdata);
}

static int distlimit_get_tars(bConstraint *con, ListBase *list)
{
	if (con && list) {
		bDistLimitConstraint *data = con->data;
		bConstraintTarget *ct;
		
		/* standard target-getting macro for single-target constraints */
		SINGLETARGET_GET_TARS(con, data->tar, data->subtarget, ct, list);
		
		return 1;
	}
	
	return 0;
}

static void distlimit_flush_tars(bConstraint *con, ListBase *list, bool no_copy)
{
	if (con && list) {
		bDistLimitConstraint *data = con->data;
		bConstraintTarget *ct = list->first;
		
		/* the following macro is used for all standard single-target constraints */
		SINGLETARGET_FLUSH_TARS(con, data->tar, data->subtarget, ct, list, no_copy);
	}
}

static void distlimit_evaluate(bConstraint *con, bConstraintOb *cob, ListBase *targets)
{
	bDistLimitConstraint *data = con->data;
	bConstraintTarget *ct = targets->first;
	
	/* only evaluate if there is a target */
	if (VALID_CONS_TARGET(ct)) {
		float dvec[3], dist, sfac = 1.0f;
		short clamp_surf = 0;
		
		/* calculate our current distance from the target */
		dist = len_v3v3(cob->matrix[3], ct->matrix[3]);
		
		/* set distance (flag is only set when user demands it) */
		if (data->dist == 0)
			data->dist = dist;
		
		/* check if we're which way to clamp from, and calculate interpolation factor (if needed) */
		if (data->mode == LIMITDIST_OUTSIDE) {
			/* if inside, then move to surface */
			if (dist <= data->dist) {
				clamp_surf = 1;
				if (dist != 0.0f) sfac = data->dist / dist;
			}
			/* if soft-distance is enabled, start fading once owner is dist+softdist from the target */
			else if (data->flag & LIMITDIST_USESOFT) {
				if (dist <= (data->dist + data->soft)) {
					
				}
			}
		}
		else if (data->mode == LIMITDIST_INSIDE) {
			/* if outside, then move to surface */
			if (dist >= data->dist) {
				clamp_surf = 1;
				if (dist != 0.0f) sfac = data->dist / dist;
			}
			/* if soft-distance is enabled, start fading once owner is dist-soft from the target */
			else if (data->flag & LIMITDIST_USESOFT) {
				/* FIXME: there's a problem with "jumping" when this kicks in */
				if (dist >= (data->dist - data->soft)) {
					sfac = (float)(data->soft * (1.0f - expf(-(dist - data->dist) / data->soft)) + data->dist);
					if (dist != 0.0f) sfac /= dist;
					
					clamp_surf = 1;
				}
			}
		}
		else {
			if (IS_EQF(dist, data->dist) == 0) {
				clamp_surf = 1;
				if (dist != 0.0f) sfac = data->dist / dist;
			}
		}
		
		/* clamp to 'surface' (i.e. move owner so that dist == data->dist) */
		if (clamp_surf) {
			/* simply interpolate along line formed by target -> owner */
			interp_v3_v3v3(dvec, ct->matrix[3], cob->matrix[3], sfac);
			
			/* copy new vector onto owner */
			copy_v3_v3(cob->matrix[3], dvec);
		}
	}
}

static bConstraintTypeInfo CTI_DISTLIMIT = {
	CONSTRAINT_TYPE_DISTLIMIT, /* type */
	sizeof(bDistLimitConstraint), /* size */
	"Limit Distance", /* name */
	"bDistLimitConstraint", /* struct name */
	NULL, /* free data */
	distlimit_id_looper, /* id looper */
	NULL, /* copy data */
	distlimit_new_data, /* new data */
	distlimit_get_tars, /* get constraint targets */
	distlimit_flush_tars, /* flush constraint targets */
	default_get_tarmat, /* get a target matrix */
	distlimit_evaluate /* evaluate */
};

/* ---------- Stretch To ------------ */

static void stretchto_new_data(void *cdata)
{
	bStretchToConstraint *data = (bStretchToConstraint *)cdata;
	
	data->volmode = 0;
	data->plane = 0;
	data->orglength = 0.0; 
	data->bulge = 1.0;
	data->bulge_max = 1.0f;
	data->bulge_min = 1.0f;
}

static void stretchto_id_looper(bConstraint *con, ConstraintIDFunc func, void *userdata)
{
	bStretchToConstraint *data = con->data;
	
	/* target only */
	func(con, (ID **)&data->tar, false, userdata);
}

static int stretchto_get_tars(bConstraint *con, ListBase *list)
{
	if (con && list) {
		bStretchToConstraint *data = con->data;
		bConstraintTarget *ct;
		
		/* standard target-getting macro for single-target constraints */
		SINGLETARGET_GET_TARS(con, data->tar, data->subtarget, ct, list);
		
		return 1;
	}
	
	return 0;
}

static void stretchto_flush_tars(bConstraint *con, ListBase *list, bool no_copy)
{
	if (con && list) {
		bStretchToConstraint *data = con->data;
		bConstraintTarget *ct = list->first;
		
		/* the following macro is used for all standard single-target constraints */
		SINGLETARGET_FLUSH_TARS(con, data->tar, data->subtarget, ct, list, no_copy);
	}
}

static void stretchto_evaluate(bConstraint *con, bConstraintOb *cob, ListBase *targets)
{
	bStretchToConstraint *data = con->data;
	bConstraintTarget *ct = targets->first;
	
	/* only evaluate if there is a target */
	if (VALID_CONS_TARGET(ct)) {
		float size[3], scale[3], vec[3], xx[3], zz[3], orth[3];
		float totmat[3][3];
		float dist, bulge;
		
		/* store scaling before destroying obmat */
		mat4_to_size(size, cob->matrix);
		
		/* store X orientation before destroying obmat */
		normalize_v3_v3(xx, cob->matrix[0]);
		
		/* store Z orientation before destroying obmat */
		normalize_v3_v3(zz, cob->matrix[2]);
		
		/* XXX That makes the constraint buggy with asymmetrically scaled objects, see #29940. */
/*		sub_v3_v3v3(vec, cob->matrix[3], ct->matrix[3]);*/
/*		vec[0] /= size[0];*/
/*		vec[1] /= size[1];*/
/*		vec[2] /= size[2];*/
		
/*		dist = normalize_v3(vec);*/

		dist = len_v3v3(cob->matrix[3], ct->matrix[3]);
		/* Only Y constrained object axis scale should be used, to keep same length when scaling it. */
		dist /= size[1];
		
		/* data->orglength==0 occurs on first run, and after 'R' button is clicked */
		if (data->orglength == 0)
			data->orglength = dist;

		scale[1] = dist / data->orglength;
		
		bulge = powf(data->orglength / dist, data->bulge);
		
		if (bulge > 1.0f) {
			if (data->flag & STRETCHTOCON_USE_BULGE_MAX) {
				float bulge_max = max_ff(data->bulge_max, 1.0f);
				float hard = min_ff(bulge, bulge_max);
				
				float range = bulge_max - 1.0f;
				float scale_fac = (range > 0.0f) ? 1.0f / range : 0.0f;
				float soft = 1.0f + range * atanf((bulge - 1.0f) * scale_fac) / (float)M_PI_2;
				
				bulge = interpf(soft, hard, data->bulge_smooth);
			}
		}
		if (bulge < 1.0f) {
			if (data->flag & STRETCHTOCON_USE_BULGE_MIN) {
				float bulge_min = CLAMPIS(data->bulge_min, 0.0f, 1.0f);
				float hard = max_ff(bulge, bulge_min);
				
				float range = 1.0f - bulge_min;
				float scale_fac = (range > 0.0f) ? 1.0f / range : 0.0f;
				float soft = 1.0f - range * atanf((1.0f - bulge) * scale_fac) / (float)M_PI_2;
				
				bulge = interpf(soft, hard, data->bulge_smooth);
			}
		}
		
		switch (data->volmode) {
			/* volume preserving scaling */
			case VOLUME_XZ:
				scale[0] = sqrtf(bulge);
				scale[2] = scale[0];
				break;
			case VOLUME_X:
				scale[0] = bulge;
				scale[2] = 1.0;
				break;
			case VOLUME_Z:
				scale[0] = 1.0;
				scale[2] = bulge;
				break;
			/* don't care for volume */
			case NO_VOLUME:
				scale[0] = 1.0;
				scale[2] = 1.0;
				break;
			default: /* should not happen, but in case*/
				return;
		} /* switch (data->volmode) */
		
		/* Clear the object's rotation and scale */
		cob->matrix[0][0] = size[0] * scale[0];
		cob->matrix[0][1] = 0;
		cob->matrix[0][2] = 0;
		cob->matrix[1][0] = 0;
		cob->matrix[1][1] = size[1] * scale[1];
		cob->matrix[1][2] = 0;
		cob->matrix[2][0] = 0;
		cob->matrix[2][1] = 0;
		cob->matrix[2][2] = size[2] * scale[2];
		
		sub_v3_v3v3(vec, cob->matrix[3], ct->matrix[3]);
		normalize_v3(vec);
		
		/* new Y aligns  object target connection*/
		negate_v3_v3(totmat[1], vec);
		switch (data->plane) {
			case PLANE_X:
				/* build new Z vector */
				/* othogonal to "new Y" "old X! plane */
				cross_v3_v3v3(orth, vec, xx);
				normalize_v3(orth);
				
				/* new Z*/
				copy_v3_v3(totmat[2], orth);
				
				/* we decided to keep X plane*/
				cross_v3_v3v3(xx, orth, vec);
				normalize_v3_v3(totmat[0], xx);
				break;
			case PLANE_Z:
				/* build new X vector */
				/* othogonal to "new Y" "old Z! plane */
				cross_v3_v3v3(orth, vec, zz);
				normalize_v3(orth);
				
				/* new X */
				negate_v3_v3(totmat[0], orth);
				
				/* we decided to keep Z */
				cross_v3_v3v3(zz, orth, vec);
				normalize_v3_v3(totmat[2], zz);
				break;
		} /* switch (data->plane) */
		
		mul_m4_m3m4(cob->matrix, totmat, cob->matrix);
	}
}

static bConstraintTypeInfo CTI_STRETCHTO = {
	CONSTRAINT_TYPE_STRETCHTO, /* type */
	sizeof(bStretchToConstraint), /* size */
	"Stretch To", /* name */
	"bStretchToConstraint", /* struct name */
	NULL, /* free data */
	stretchto_id_looper, /* id looper */
	NULL, /* copy data */
	stretchto_new_data, /* new data */
	stretchto_get_tars, /* get constraint targets */
	stretchto_flush_tars, /* flush constraint targets */
	default_get_tarmat, /* get target matrix */
	stretchto_evaluate /* evaluate */
};

/* ---------- Floor ------------ */

static void minmax_new_data(void *cdata)
{
	bMinMaxConstraint *data = (bMinMaxConstraint *)cdata;
	
	data->minmaxflag = TRACK_Z;
	data->offset = 0.0f;
	zero_v3(data->cache);
	data->flag = 0;
}

static void minmax_id_looper(bConstraint *con, ConstraintIDFunc func, void *userdata)
{
	bMinMaxConstraint *data = con->data;
	
	/* target only */
	func(con, (ID **)&data->tar, false, userdata);
}

static int minmax_get_tars(bConstraint *con, ListBase *list)
{
	if (con && list) {
		bMinMaxConstraint *data = con->data;
		bConstraintTarget *ct;
		
		/* standard target-getting macro for single-target constraints */
		SINGLETARGET_GET_TARS(con, data->tar, data->subtarget, ct, list);
		
		return 1;
	}
	
	return 0;
}

static void minmax_flush_tars(bConstraint *con, ListBase *list, bool no_copy)
{
	if (con && list) {
		bMinMaxConstraint *data = con->data;
		bConstraintTarget *ct = list->first;
		
		/* the following macro is used for all standard single-target constraints */
		SINGLETARGET_FLUSH_TARS(con, data->tar, data->subtarget, ct, list, no_copy);
	}
}

static void minmax_evaluate(bConstraint *con, bConstraintOb *cob, ListBase *targets)
{
	bMinMaxConstraint *data = con->data;
	bConstraintTarget *ct = targets->first;
	
	/* only evaluate if there is a target */
	if (VALID_CONS_TARGET(ct)) {
		float obmat[4][4], imat[4][4], tarmat[4][4], tmat[4][4];
		float val1, val2;
		int index;
		
		copy_m4_m4(obmat, cob->matrix);
		copy_m4_m4(tarmat, ct->matrix);
		
		if (data->flag & MINMAX_USEROT) {
			/* take rotation of target into account by doing the transaction in target's localspace */
			invert_m4_m4(imat, tarmat);
			mul_m4_m4m4(tmat, imat, obmat);
			copy_m4_m4(obmat, tmat);
			unit_m4(tarmat);
		}
		
		switch (data->minmaxflag) {
			case TRACK_Z:
				val1 = tarmat[3][2];
				val2 = obmat[3][2] - data->offset;
				index = 2;
				break;
			case TRACK_Y:
				val1 = tarmat[3][1];
				val2 = obmat[3][1] - data->offset;
				index = 1;
				break;
			case TRACK_X:
				val1 = tarmat[3][0];
				val2 = obmat[3][0] - data->offset;
				index = 0;
				break;
			case TRACK_nZ:
				val2 = tarmat[3][2];
				val1 = obmat[3][2] - data->offset;
				index = 2;
				break;
			case TRACK_nY:
				val2 = tarmat[3][1];
				val1 = obmat[3][1] - data->offset;
				index = 1;
				break;
			case TRACK_nX:
				val2 = tarmat[3][0];
				val1 = obmat[3][0] - data->offset;
				index = 0;
				break;
			default:
				return;
		}
		
		if (val1 > val2) {
			obmat[3][index] = tarmat[3][index] + data->offset;
			if (data->flag & MINMAX_STICKY) {
				if (data->flag & MINMAX_STUCK) {
					copy_v3_v3(obmat[3], data->cache);
				}
				else {
					copy_v3_v3(data->cache, obmat[3]);
					data->flag |= MINMAX_STUCK;
				}
			}
			if (data->flag & MINMAX_USEROT) {
				/* get out of localspace */
				mul_m4_m4m4(tmat, ct->matrix, obmat);
				copy_m4_m4(cob->matrix, tmat);
			}
			else {
				copy_v3_v3(cob->matrix[3], obmat[3]);
			}
		}
		else {
			data->flag &= ~MINMAX_STUCK;
		}
	}
}

static bConstraintTypeInfo CTI_MINMAX = {
	CONSTRAINT_TYPE_MINMAX, /* type */
	sizeof(bMinMaxConstraint), /* size */
	"Floor", /* name */
	"bMinMaxConstraint", /* struct name */
	NULL, /* free data */
	minmax_id_looper, /* id looper */
	NULL, /* copy data */
	minmax_new_data, /* new data */
	minmax_get_tars, /* get constraint targets */
	minmax_flush_tars, /* flush constraint targets */
	default_get_tarmat, /* get target matrix */
	minmax_evaluate /* evaluate */
};

/* ------- RigidBody Joint ---------- */

static void rbj_new_data(void *cdata)
{
	bRigidBodyJointConstraint *data = (bRigidBodyJointConstraint *)cdata;
	
	/* removed code which set target of this constraint */
	data->type = 1;
}

static void rbj_id_looper(bConstraint *con, ConstraintIDFunc func, void *userdata)
{
	bRigidBodyJointConstraint *data = con->data;
	
	/* target only */
	func(con, (ID **)&data->tar, false, userdata);
	func(con, (ID **)&data->child, false, userdata);
}

static int rbj_get_tars(bConstraint *con, ListBase *list)
{
	if (con && list) {
		bRigidBodyJointConstraint *data = con->data;
		bConstraintTarget *ct;
		
		/* standard target-getting macro for single-target constraints without subtargets */
		SINGLETARGETNS_GET_TARS(con, data->tar, ct, list);
		
		return 1;
	}
	
	return 0;
}

static void rbj_flush_tars(bConstraint *con, ListBase *list, bool no_copy)
{
	if (con && list) {
		bRigidBodyJointConstraint *data = con->data;
		bConstraintTarget *ct = list->first;
		
		/* the following macro is used for all standard single-target constraints */
		SINGLETARGETNS_FLUSH_TARS(con, data->tar, ct, list, no_copy);
	}
}

static bConstraintTypeInfo CTI_RIGIDBODYJOINT = {
	CONSTRAINT_TYPE_RIGIDBODYJOINT, /* type */
	sizeof(bRigidBodyJointConstraint), /* size */
	"Rigid Body Joint", /* name */
	"bRigidBodyJointConstraint", /* struct name */
	NULL, /* free data */
	rbj_id_looper, /* id looper */
	NULL, /* copy data */
	rbj_new_data, /* new data */
	rbj_get_tars, /* get constraint targets */
	rbj_flush_tars, /* flush constraint targets */
	default_get_tarmat, /* get target matrix */
	NULL /* evaluate - this is not solved here... is just an interface for game-engine */
};

/* -------- Clamp To ---------- */

static void clampto_id_looper(bConstraint *con, ConstraintIDFunc func, void *userdata)
{
	bClampToConstraint *data = con->data;
	
	/* target only */
	func(con, (ID **)&data->tar, false, userdata);
}

static int clampto_get_tars(bConstraint *con, ListBase *list)
{
	if (con && list) {
		bClampToConstraint *data = con->data;
		bConstraintTarget *ct;
		
		/* standard target-getting macro for single-target constraints without subtargets */
		SINGLETARGETNS_GET_TARS(con, data->tar, ct, list);
		
		return 1;
	}
	
	return 0;
}

static void clampto_flush_tars(bConstraint *con, ListBase *list, bool no_copy)
{
	if (con && list) {
		bClampToConstraint *data = con->data;
		bConstraintTarget *ct = list->first;
		
		/* the following macro is used for all standard single-target constraints */
		SINGLETARGETNS_FLUSH_TARS(con, data->tar, ct, list, no_copy);
	}
}

static void clampto_get_tarmat(bConstraint *UNUSED(con), bConstraintOb *cob, bConstraintTarget *ct, float UNUSED(ctime))
{
#ifdef CYCLIC_DEPENDENCY_WORKAROUND
	if (VALID_CONS_TARGET(ct)) {
		if (ct->tar->curve_cache == NULL) {
			BKE_displist_make_curveTypes(cob->scene, ct->tar, false);
		}
	}
#endif

	/* technically, this isn't really needed for evaluation, but we don't know what else
	 * might end up calling this...
	 */
	if (ct)
		unit_m4(ct->matrix);
}

static void clampto_evaluate(bConstraint *con, bConstraintOb *cob, ListBase *targets)
{
	bClampToConstraint *data = con->data;
	bConstraintTarget *ct = targets->first;
	
	/* only evaluate if there is a target and it is a curve */
	if (VALID_CONS_TARGET(ct) && (ct->tar->type == OB_CURVE)) {
		float obmat[4][4], ownLoc[3];
		float curveMin[3], curveMax[3];
		float targetMatrix[4][4];
		
		copy_m4_m4(obmat, cob->matrix);
		copy_v3_v3(ownLoc, obmat[3]);
		
		unit_m4(targetMatrix);
		INIT_MINMAX(curveMin, curveMax);
		/* XXX - don't think this is good calling this here - campbell */
		BKE_object_minmax(ct->tar, curveMin, curveMax, true);
		
		/* get targetmatrix */
		if (data->tar->curve_cache &&  data->tar->curve_cache->path && data->tar->curve_cache->path->data) {
			float vec[4], dir[3], totmat[4][4];
			float curvetime;
			short clamp_axis;
			
			/* find best position on curve */
			/* 1. determine which axis to sample on? */
			if (data->flag == CLAMPTO_AUTO) {
				float size[3];
				sub_v3_v3v3(size, curveMax, curveMin);
				
				/* find axis along which the bounding box has the greatest
				 * extent. Otherwise, default to the x-axis, as that is quite
				 * frequently used.
				 */
				if ((size[2] > size[0]) && (size[2] > size[1]))
					clamp_axis = CLAMPTO_Z - 1;
				else if ((size[1] > size[0]) && (size[1] > size[2]))
					clamp_axis = CLAMPTO_Y - 1;
				else
					clamp_axis = CLAMPTO_X - 1;
			}
			else 
				clamp_axis = data->flag - 1;
				
			/* 2. determine position relative to curve on a 0-1 scale based on bounding box */
			if (data->flag2 & CLAMPTO_CYCLIC) {
				/* cyclic, so offset within relative bounding box is used */
				float len = (curveMax[clamp_axis] - curveMin[clamp_axis]);
				float offset;
				
				/* check to make sure len is not so close to zero that it'll cause errors */
				if (IS_EQF(len, 0.0f) == false) {
					/* find bounding-box range where target is located */
					if (ownLoc[clamp_axis] < curveMin[clamp_axis]) {
						/* bounding-box range is before */
						offset = curveMin[clamp_axis] - ceilf((curveMin[clamp_axis] - ownLoc[clamp_axis]) / len) * len;

						/* now, we calculate as per normal, except using offset instead of curveMin[clamp_axis] */
						curvetime = (ownLoc[clamp_axis] - offset) / (len);
					}
					else if (ownLoc[clamp_axis] > curveMax[clamp_axis]) {
						/* bounding-box range is after */
						offset = curveMax[clamp_axis] + (int)((ownLoc[clamp_axis] - curveMax[clamp_axis]) / len) * len;

						/* now, we calculate as per normal, except using offset instead of curveMax[clamp_axis] */
						curvetime = (ownLoc[clamp_axis] - offset) / (len);
					}
					else {
						/* as the location falls within bounds, just calculate */
						curvetime = (ownLoc[clamp_axis] - curveMin[clamp_axis]) / (len);
					}
				}
				else {
					/* as length is close to zero, curvetime by default should be 0 (i.e. the start) */
					curvetime = 0.0f;
				}
			}
			else {
				/* no cyclic, so position is clamped to within the bounding box */
				if (ownLoc[clamp_axis] <= curveMin[clamp_axis])
					curvetime = 0.0f;
				else if (ownLoc[clamp_axis] >= curveMax[clamp_axis])
					curvetime = 1.0f;
				else if (IS_EQF((curveMax[clamp_axis] - curveMin[clamp_axis]), 0.0f) == false)
					curvetime = (ownLoc[clamp_axis] - curveMin[clamp_axis]) / (curveMax[clamp_axis] - curveMin[clamp_axis]);
				else 
					curvetime = 0.0f;
			}
			
			/* 3. position on curve */
			if (where_on_path(ct->tar, curvetime, vec, dir, NULL, NULL, NULL) ) {
				unit_m4(totmat);
				copy_v3_v3(totmat[3], vec);
				
				mul_m4_m4m4(targetMatrix, ct->tar->obmat, totmat);
			}
		}
		
		/* obtain final object position */
		copy_v3_v3(cob->matrix[3], targetMatrix[3]);
	}
}

static bConstraintTypeInfo CTI_CLAMPTO = {
	CONSTRAINT_TYPE_CLAMPTO, /* type */
	sizeof(bClampToConstraint), /* size */
	"Clamp To", /* name */
	"bClampToConstraint", /* struct name */
	NULL, /* free data */
	clampto_id_looper, /* id looper */
	NULL, /* copy data */
	NULL, /* new data */
	clampto_get_tars, /* get constraint targets */
	clampto_flush_tars, /* flush constraint targets */
	clampto_get_tarmat, /* get target matrix */
	clampto_evaluate /* evaluate */
};

/* ---------- Transform Constraint ----------- */

static void transform_new_data(void *cdata)
{
	bTransformConstraint *data = (bTransformConstraint *)cdata;
	
	data->map[0] = 0;
	data->map[1] = 1;
	data->map[2] = 2;
}

static void transform_id_looper(bConstraint *con, ConstraintIDFunc func, void *userdata)
{
	bTransformConstraint *data = con->data;
	
	/* target only */
	func(con, (ID **)&data->tar, false, userdata);
}

static int transform_get_tars(bConstraint *con, ListBase *list)
{
	if (con && list) {
		bTransformConstraint *data = con->data;
		bConstraintTarget *ct;
		
		/* standard target-getting macro for single-target constraints */
		SINGLETARGET_GET_TARS(con, data->tar, data->subtarget, ct, list);
		
		return 1;
	}
	
	return 0;
}

static void transform_flush_tars(bConstraint *con, ListBase *list, bool no_copy)
{
	if (con && list) {
		bTransformConstraint *data = con->data;
		bConstraintTarget *ct = list->first;
		
		/* the following macro is used for all standard single-target constraints */
		SINGLETARGET_FLUSH_TARS(con, data->tar, data->subtarget, ct, list, no_copy);
	}
}

static void transform_evaluate(bConstraint *con, bConstraintOb *cob, ListBase *targets)
{
	bTransformConstraint *data = con->data;
	bConstraintTarget *ct = targets->first;
	
	/* only evaluate if there is a target */
	if (VALID_CONS_TARGET(ct)) {
		float *from_min, *from_max, *to_min, *to_max;
		float loc[3], eul[3], size[3];
		float dvec[3], sval[3];
		int i;
		
		/* obtain target effect */
		switch (data->from) {
			case TRANS_SCALE:
				mat4_to_size(dvec, ct->matrix);
				
				if (is_negative_m4(ct->matrix)) {
					/* Bugfix [#27886] 
					 * We can't be sure which axis/axes are negative, though we know that something is negative.
					 * Assume we don't care about negativity of separate axes. <--- This is a limitation that
					 * riggers will have to live with for now.
					 */
					negate_v3(dvec);
				}
				from_min = data->from_min_scale;
				from_max = data->from_max_scale;
				break;
			case TRANS_ROTATION:
				mat4_to_eulO(dvec, cob->rotOrder, ct->matrix);
				from_min = data->from_min_rot;
				from_max = data->from_max_rot;
				break;
			case TRANS_LOCATION:
			default:
				copy_v3_v3(dvec, ct->matrix[3]);
				from_min = data->from_min;
				from_max = data->from_max;
				break;
		}
		
		/* extract components of owner's matrix */
		copy_v3_v3(loc, cob->matrix[3]);
		mat4_to_eulO(eul, cob->rotOrder, cob->matrix);
		mat4_to_size(size, cob->matrix);
		
		/* determine where in range current transforms lie */
		if (data->expo) {
			for (i = 0; i < 3; i++) {
				if (from_max[i] - from_min[i])
					sval[i] = (dvec[i] - from_min[i]) / (from_max[i] - from_min[i]);
				else
					sval[i] = 0.0f;
			}
		}
		else {
			/* clamp transforms out of range */
			for (i = 0; i < 3; i++) {
				CLAMP(dvec[i], from_min[i], from_max[i]);
				if (from_max[i] - from_min[i])
					sval[i] = (dvec[i] - from_min[i]) / (from_max[i] - from_min[i]);
				else
					sval[i] = 0.0f;
			}
		}
		
		
		/* apply transforms */
		switch (data->to) {
			case TRANS_SCALE:
				to_min = data->to_min_scale;
				to_max = data->to_max_scale;
				for (i = 0; i < 3; i++) {
					/* multiply with original scale (so that it can still be scaled) */
					/* size[i] *= to_min[i] + (sval[(int)data->map[i]] * (to_max[i] - to_min[i])); */
					/* Stay absolute, else it breaks existing rigs... sigh. */
					size[i] = to_min[i] + (sval[(int)data->map[i]] * (to_max[i] - to_min[i]));
				}
				break;
			case TRANS_ROTATION:
				to_min = data->to_min_rot;
				to_max = data->to_max_rot;
				for (i = 0; i < 3; i++) {
					/* add to original rotation (so that it can still be rotated) */
					eul[i] += to_min[i] + (sval[(int)data->map[i]] * (to_max[i] - to_min[i]));
				}
				break;
			case TRANS_LOCATION:
			default:
				to_min = data->to_min;
				to_max = data->to_max;
				for (i = 0; i < 3; i++) {
					/* add to original location (so that it can still be moved) */
					loc[i] += (to_min[i] + (sval[(int)data->map[i]] * (to_max[i] - to_min[i])));
				}
				break;
		}
		
		/* apply to matrix */
		loc_eulO_size_to_mat4(cob->matrix, loc, eul, size, cob->rotOrder);
	}
}

static bConstraintTypeInfo CTI_TRANSFORM = {
	CONSTRAINT_TYPE_TRANSFORM, /* type */
	sizeof(bTransformConstraint), /* size */
	"Transformation", /* name */
	"bTransformConstraint", /* struct name */
	NULL, /* free data */
	transform_id_looper, /* id looper */
	NULL, /* copy data */
	transform_new_data, /* new data */
	transform_get_tars, /* get constraint targets */
	transform_flush_tars, /* flush constraint targets */
	default_get_tarmat, /* get a target matrix */
	transform_evaluate /* evaluate */
};

/* ---------- Shrinkwrap Constraint ----------- */

static void shrinkwrap_id_looper(bConstraint *con, ConstraintIDFunc func, void *userdata)
{
	bShrinkwrapConstraint *data = con->data;
	
	/* target only */
	func(con, (ID **)&data->target, false, userdata);
}

static void shrinkwrap_new_data(void *cdata)
{
	bShrinkwrapConstraint *data = (bShrinkwrapConstraint *)cdata;

	data->projAxis = OB_POSZ;
	data->projAxisSpace = CONSTRAINT_SPACE_LOCAL;
}

static int shrinkwrap_get_tars(bConstraint *con, ListBase *list)
{
	if (con && list) {
		bShrinkwrapConstraint *data = con->data;
		bConstraintTarget *ct;
		
		SINGLETARGETNS_GET_TARS(con, data->target, ct, list);
		
		return 1;
	}
	
	return 0;
}


static void shrinkwrap_flush_tars(bConstraint *con, ListBase *list, bool no_copy)
{
	if (con && list) {
		bShrinkwrapConstraint *data = con->data;
		bConstraintTarget *ct = list->first;
		
		SINGLETARGETNS_FLUSH_TARS(con, data->target, ct, list, no_copy);
	}
}


static void shrinkwrap_get_tarmat(bConstraint *con, bConstraintOb *cob, bConstraintTarget *ct, float UNUSED(ctime))
{
	bShrinkwrapConstraint *scon = (bShrinkwrapConstraint *) con->data;
	
	if (VALID_CONS_TARGET(ct) && (ct->tar->type == OB_MESH) ) {
		bool fail = false;
		float co[3] = {0.0f, 0.0f, 0.0f};
		
		SpaceTransform transform;
		/* TODO(sergey): use proper for_render flag here when known. */
		DerivedMesh *target = object_get_derived_final(ct->tar, false);
		
		BVHTreeFromMesh treeData = {NULL};
		
		unit_m4(ct->matrix);
		
		if (target != NULL) {
			BLI_space_transform_from_matrices(&transform, cob->matrix, ct->tar->obmat);
			
			switch (scon->shrinkType) {
				case MOD_SHRINKWRAP_NEAREST_SURFACE:
				case MOD_SHRINKWRAP_NEAREST_VERTEX:
				{
					BVHTreeNearest nearest;
					float dist;

					nearest.index = -1;
					nearest.dist_sq = FLT_MAX;

					if (scon->shrinkType == MOD_SHRINKWRAP_NEAREST_VERTEX)
						bvhtree_from_mesh_verts(&treeData, target, 0.0, 2, 6);
					else
						bvhtree_from_mesh_looptri(&treeData, target, 0.0, 2, 6);
					
					if (treeData.tree == NULL) {
						fail = true;
						break;
					}
					
					BLI_space_transform_apply(&transform, co);
					
					BLI_bvhtree_find_nearest(treeData.tree, co, &nearest, treeData.nearest_callback, &treeData);
					
					dist = len_v3v3(co, nearest.co);
					if (dist != 0.0f) {
						interp_v3_v3v3(co, co, nearest.co, (dist - scon->dist) / dist);   /* linear interpolation */
					}
					BLI_space_transform_invert(&transform, co);
					break;
				}
				case MOD_SHRINKWRAP_PROJECT:
				{
					BVHTreeRayHit hit;

					float mat[4][4];
					float no[3] = {0.0f, 0.0f, 0.0f};

					/* TODO should use FLT_MAX.. but normal projection doenst yet supports it */
					hit.index = -1;
					hit.dist = (scon->projLimit == 0.0f) ? BVH_RAYCAST_DIST_MAX : scon->projLimit;

					switch (scon->projAxis) {
						case OB_POSX: case OB_POSY: case OB_POSZ:
							no[scon->projAxis - OB_POSX] = 1.0f;
							break;
						case OB_NEGX: case OB_NEGY: case OB_NEGZ:
							no[scon->projAxis - OB_NEGX] = -1.0f;
							break;
					}
					
					/* transform normal into requested space */
					/* Note that in this specific case, we need to keep scaling in non-parented 'local2world' object
					 * case, because SpaceTransform also takes it into account when handling normals. See T42447. */
					unit_m4(mat);
					BKE_constraint_mat_convertspace(cob->ob, cob->pchan, mat,
					                                CONSTRAINT_SPACE_LOCAL, scon->projAxisSpace, true);
					invert_m4(mat);
					mul_mat3_m4_v3(mat, no);

					if (normalize_v3(no) < FLT_EPSILON) {
						fail = true;
						break;
					}

					bvhtree_from_mesh_looptri(&treeData, target, scon->dist, 4, 6);
					if (treeData.tree == NULL) {
						fail = true;
						break;
					}

					
					if (BKE_shrinkwrap_project_normal(0, co, no, &transform, treeData.tree, &hit,
					                                  treeData.raycast_callback, &treeData) == false)
					{
						fail = true;
						break;
					}
					copy_v3_v3(co, hit.co);
					break;
				}
			}
			
			free_bvhtree_from_mesh(&treeData);
			
			if (fail == true) {
				/* Don't move the point */
				zero_v3(co);
			}
			
			/* co is in local object coordinates, change it to global and update target position */
			mul_m4_v3(cob->matrix, co);
			copy_v3_v3(ct->matrix[3], co);
		}
	}
}

static void shrinkwrap_evaluate(bConstraint *UNUSED(con), bConstraintOb *cob, ListBase *targets)
{
	bConstraintTarget *ct = targets->first;
	
	/* only evaluate if there is a target */
	if (VALID_CONS_TARGET(ct)) {
		copy_v3_v3(cob->matrix[3], ct->matrix[3]);
	}
}

static bConstraintTypeInfo CTI_SHRINKWRAP = {
	CONSTRAINT_TYPE_SHRINKWRAP, /* type */
	sizeof(bShrinkwrapConstraint), /* size */
	"Shrinkwrap", /* name */
	"bShrinkwrapConstraint", /* struct name */
	NULL, /* free data */
	shrinkwrap_id_looper, /* id looper */
	NULL, /* copy data */
	shrinkwrap_new_data, /* new data */
	shrinkwrap_get_tars, /* get constraint targets */
	shrinkwrap_flush_tars, /* flush constraint targets */
	shrinkwrap_get_tarmat, /* get a target matrix */
	shrinkwrap_evaluate /* evaluate */
};

/* --------- Damped Track ---------- */

static void damptrack_new_data(void *cdata)
{
	bDampTrackConstraint *data = (bDampTrackConstraint *)cdata;
	
	data->trackflag = TRACK_Y;
}	

static void damptrack_id_looper(bConstraint *con, ConstraintIDFunc func, void *userdata)
{
	bDampTrackConstraint *data = con->data;
	
	/* target only */
	func(con, (ID **)&data->tar, false, userdata);
}

static int damptrack_get_tars(bConstraint *con, ListBase *list)
{
	if (con && list) {
		bDampTrackConstraint *data = con->data;
		bConstraintTarget *ct;
		
		/* the following macro is used for all standard single-target constraints */
		SINGLETARGET_GET_TARS(con, data->tar, data->subtarget, ct, list);
		
		return 1;
	}
	
	return 0;
}

static void damptrack_flush_tars(bConstraint *con, ListBase *list, bool no_copy)
{
	if (con && list) {
		bDampTrackConstraint *data = con->data;
		bConstraintTarget *ct = list->first;
		
		/* the following macro is used for all standard single-target constraints */
		SINGLETARGET_FLUSH_TARS(con, data->tar, data->subtarget, ct, list, no_copy);
	}
}

/* array of direction vectors for the tracking flags */
static const float track_dir_vecs[6][3] = {
	{+1, 0, 0}, {0, +1, 0}, {0, 0, +1},     /* TRACK_X,  TRACK_Y,  TRACK_Z */
	{-1, 0, 0}, {0, -1, 0}, {0, 0, -1}      /* TRACK_NX, TRACK_NY, TRACK_NZ */
};

static void damptrack_evaluate(bConstraint *con, bConstraintOb *cob, ListBase *targets)
{
	bDampTrackConstraint *data = con->data;
	bConstraintTarget *ct = targets->first;
	
	if (VALID_CONS_TARGET(ct)) {
		float obvec[3], tarvec[3], obloc[3];
		float raxis[3], rangle;
		float rmat[3][3], tmat[4][4];
		
		/* find the (unit) direction that the axis we're interested in currently points 
		 *	- mul_mat3_m4_v3() only takes the 3x3 (rotation+scaling) components of the 4x4 matrix 
		 *	- the normalization step at the end should take care of any unwanted scaling
		 *	  left over in the 3x3 matrix we used
		 */
		copy_v3_v3(obvec, track_dir_vecs[data->trackflag]);
		mul_mat3_m4_v3(cob->matrix, obvec);
		
		if (normalize_v3(obvec) == 0.0f) {
			/* exceptional case - just use the track vector as appropriate */
			copy_v3_v3(obvec, track_dir_vecs[data->trackflag]);
		}
		
		/* find the (unit) direction vector going from the owner to the target */
		copy_v3_v3(obloc, cob->matrix[3]);
		sub_v3_v3v3(tarvec, ct->matrix[3], obloc);
		
		if (normalize_v3(tarvec) == 0.0f) {
			/* the target is sitting on the owner, so just make them use the same direction vectors */
			/* FIXME: or would it be better to use the pure direction vector? */
			copy_v3_v3(tarvec, obvec);
			//copy_v3_v3(tarvec, track_dir_vecs[data->trackflag]);
		}
		
		/* determine the axis-angle rotation, which represents the smallest possible rotation
		 * between the two rotation vectors (i.e. the 'damping' referred to in the name)
		 *	- we take this to be the rotation around the normal axis/vector to the plane defined
		 *	  by the current and destination vectors, which will 'map' the current axis to the 
		 *	  destination vector
		 *	- the min/max wrappers around (obvec . tarvec) result (stored temporarily in rangle)
		 *	  are used to ensure that the smallest angle is chosen
		 */
		cross_v3_v3v3(raxis, obvec, tarvec);
		
		rangle = dot_v3v3(obvec, tarvec);
		rangle = acosf(max_ff(-1.0f, min_ff(1.0f, rangle)));
		
		/* construct rotation matrix from the axis-angle rotation found above 
		 *	- this call takes care to make sure that the axis provided is a unit vector first
		 */
		axis_angle_to_mat3(rmat, raxis, rangle);
		
		/* rotate the owner in the way defined by this rotation matrix, then reapply the location since
		 * we may have destroyed that in the process of multiplying the matrix
		 */
		unit_m4(tmat);
		mul_m4_m3m4(tmat, rmat, cob->matrix); // m1, m3, m2
		
		copy_m4_m4(cob->matrix, tmat);
		copy_v3_v3(cob->matrix[3], obloc);
	}
}

static bConstraintTypeInfo CTI_DAMPTRACK = {
	CONSTRAINT_TYPE_DAMPTRACK, /* type */
	sizeof(bDampTrackConstraint), /* size */
	"Damped Track", /* name */
	"bDampTrackConstraint", /* struct name */
	NULL, /* free data */
	damptrack_id_looper, /* id looper */
	NULL, /* copy data */
	damptrack_new_data, /* new data */
	damptrack_get_tars, /* get constraint targets */
	damptrack_flush_tars, /* flush constraint targets */
	default_get_tarmat, /* get target matrix */
	damptrack_evaluate /* evaluate */
};

/* ----------- Spline IK ------------ */

static void splineik_free(bConstraint *con)
{
	bSplineIKConstraint *data = con->data;
	
	/* binding array */
	if (data->points)
		MEM_freeN(data->points);
}	

static void splineik_copy(bConstraint *con, bConstraint *srccon)
{
	bSplineIKConstraint *src = srccon->data;
	bSplineIKConstraint *dst = con->data;
	
	/* copy the binding array */
	dst->points = MEM_dupallocN(src->points);
}

static void splineik_new_data(void *cdata)
{
	bSplineIKConstraint *data = (bSplineIKConstraint *)cdata;

	data->chainlen = 1;
	data->bulge = 1.0;
	data->bulge_max = 1.0f;
	data->bulge_min = 1.0f;
}

static void splineik_id_looper(bConstraint *con, ConstraintIDFunc func, void *userdata)
{
	bSplineIKConstraint *data = con->data;
	
	/* target only */
	func(con, (ID **)&data->tar, false, userdata);
}

static int splineik_get_tars(bConstraint *con, ListBase *list)
{
	if (con && list) {
		bSplineIKConstraint *data = con->data;
		bConstraintTarget *ct;
		
		/* standard target-getting macro for single-target constraints without subtargets */
		SINGLETARGETNS_GET_TARS(con, data->tar, ct, list);
		
		return 1;
	}
	
	return 0;
}

static void splineik_flush_tars(bConstraint *con, ListBase *list, bool no_copy)
{
	if (con && list) {
		bSplineIKConstraint *data = con->data;
		bConstraintTarget *ct = list->first;
		
		/* the following macro is used for all standard single-target constraints */
		SINGLETARGETNS_FLUSH_TARS(con, data->tar, ct, list, no_copy);
	}
}

static void splineik_get_tarmat(bConstraint *UNUSED(con), bConstraintOb *cob, bConstraintTarget *ct, float UNUSED(ctime))
{
#ifdef CYCLIC_DEPENDENCY_WORKAROUND
	if (VALID_CONS_TARGET(ct)) {
		if (ct->tar->curve_cache == NULL) {
			BKE_displist_make_curveTypes(cob->scene, ct->tar, false);
		}
	}
#endif

	/* technically, this isn't really needed for evaluation, but we don't know what else
	 * might end up calling this...
	 */
	if (ct)
		unit_m4(ct->matrix);
}

static bConstraintTypeInfo CTI_SPLINEIK = {
	CONSTRAINT_TYPE_SPLINEIK, /* type */
	sizeof(bSplineIKConstraint), /* size */
	"Spline IK", /* name */
	"bSplineIKConstraint", /* struct name */
	splineik_free, /* free data */
	splineik_id_looper, /* id looper */
	splineik_copy, /* copy data */
	splineik_new_data, /* new data */
	splineik_get_tars, /* get constraint targets */
	splineik_flush_tars, /* flush constraint targets */
	splineik_get_tarmat, /* get target matrix */
	NULL /* evaluate - solved as separate loop */
};

/* ----------- Pivot ------------- */

static void pivotcon_id_looper(bConstraint *con, ConstraintIDFunc func, void *userdata)
{
	bPivotConstraint *data = con->data;
	
	/* target only */
	func(con, (ID **)&data->tar, false, userdata);
}

static int pivotcon_get_tars(bConstraint *con, ListBase *list)
{
	if (con && list) {
		bPivotConstraint *data = con->data;
		bConstraintTarget *ct;
		
		/* standard target-getting macro for single-target constraints */
		SINGLETARGET_GET_TARS(con, data->tar, data->subtarget, ct, list);
		
		return 1;
	}
	
	return 0;
}

static void pivotcon_flush_tars(bConstraint *con, ListBase *list, bool no_copy)
{
	if (con && list) {
		bPivotConstraint *data = con->data;
		bConstraintTarget *ct = list->first;
		
		/* the following macro is used for all standard single-target constraints */
		SINGLETARGET_FLUSH_TARS(con, data->tar, data->subtarget, ct, list, no_copy);
	}
}

static void pivotcon_evaluate(bConstraint *con, bConstraintOb *cob, ListBase *targets)
{
	bPivotConstraint *data = con->data;
	bConstraintTarget *ct = targets->first;
	
	float pivot[3], vec[3];
	float rotMat[3][3];

	/* pivot correction */
	float axis[3], angle;
	
	/* firstly, check if pivoting should take place based on the current rotation */
	if (data->rotAxis != PIVOTCON_AXIS_NONE) {
		float rot[3];
		
		/* extract euler-rotation of target */
		mat4_to_eulO(rot, cob->rotOrder, cob->matrix);
		
		/* check which range might be violated */
		if (data->rotAxis < PIVOTCON_AXIS_X) {
			/* negative rotations (data->rotAxis = 0 -> 2) */
			if (rot[data->rotAxis] > 0.0f)
				return;
		}
		else {
			/* positive rotations (data->rotAxis = 3 -> 5 */
			if (rot[data->rotAxis - PIVOTCON_AXIS_X] < 0.0f)
				return;
		}
	}
	
	/* find the pivot-point to use  */
	if (VALID_CONS_TARGET(ct)) {
		/* apply offset to target location */
		add_v3_v3v3(pivot, ct->matrix[3], data->offset);
	}
	else {
		/* no targets to worry about... */
		if ((data->flag & PIVOTCON_FLAG_OFFSET_ABS) == 0) {
			/* offset is relative to owner */
			add_v3_v3v3(pivot, cob->matrix[3], data->offset);
		}
		else {
			/* directly use the 'offset' specified as an absolute position instead */
			copy_v3_v3(pivot, data->offset);
		}
	}
	
	/* get rotation matrix representing the rotation of the owner */
	/* TODO: perhaps we might want to include scaling based on the pivot too? */
	copy_m3_m4(rotMat, cob->matrix);
	normalize_m3(rotMat);


	/* correct the pivot by the rotation axis otherwise the pivot translates when it shouldnt */
	mat3_normalized_to_axis_angle(axis, &angle, rotMat);
	if (angle) {
		float dvec[3];
		sub_v3_v3v3(vec, pivot, cob->matrix[3]);
		project_v3_v3v3(dvec, vec, axis);
		sub_v3_v3(pivot, dvec);
	}

	/* perform the pivoting... */
	/* 1. take the vector from owner to the pivot */
	sub_v3_v3v3(vec, cob->matrix[3], pivot);
	/* 2. rotate this vector by the rotation of the object... */
	mul_m3_v3(rotMat, vec);
	/* 3. make the rotation in terms of the pivot now */
	add_v3_v3v3(cob->matrix[3], pivot, vec);
}


static bConstraintTypeInfo CTI_PIVOT = {
	CONSTRAINT_TYPE_PIVOT, /* type */
	sizeof(bPivotConstraint), /* size */
	"Pivot", /* name */
	"bPivotConstraint", /* struct name */
	NULL, /* free data */
	pivotcon_id_looper, /* id looper */
	NULL, /* copy data */
	NULL, /* new data */ // XXX: might be needed to get 'normal' pivot behavior...
	pivotcon_get_tars, /* get constraint targets */
	pivotcon_flush_tars, /* flush constraint targets */
	default_get_tarmat, /* get target matrix */
	pivotcon_evaluate /* evaluate */
};

/* ----------- Follow Track ------------- */

static void followtrack_new_data(void *cdata)
{
	bFollowTrackConstraint *data = (bFollowTrackConstraint *)cdata;

	data->clip = NULL;
	data->flag |= FOLLOWTRACK_ACTIVECLIP;
}

static void followtrack_id_looper(bConstraint *con, ConstraintIDFunc func, void *userdata)
{
	bFollowTrackConstraint *data = con->data;

	func(con, (ID **)&data->clip, true, userdata);
	func(con, (ID **)&data->camera, false, userdata);
	func(con, (ID **)&data->depth_ob, false, userdata);
}

static void followtrack_evaluate(bConstraint *con, bConstraintOb *cob, ListBase *UNUSED(targets))
{
	Scene *scene = cob->scene;
	bFollowTrackConstraint *data = con->data;
	MovieClip *clip = data->clip;
	MovieTracking *tracking;
	MovieTrackingTrack *track;
	MovieTrackingObject *tracking_object;
	Object *camob = data->camera ? data->camera : scene->camera;
	float ctime = BKE_scene_frame_get(scene);
	float framenr;

	if (data->flag & FOLLOWTRACK_ACTIVECLIP)
		clip = scene->clip;

	if (!clip || !data->track[0] || !camob)
		return;

	tracking = &clip->tracking;

	if (data->object[0])
		tracking_object = BKE_tracking_object_get_named(tracking, data->object);
	else
		tracking_object = BKE_tracking_object_get_camera(tracking);

	if (!tracking_object)
		return;

	track = BKE_tracking_track_get_named(tracking, tracking_object, data->track);

	if (!track)
		return;

	framenr = BKE_movieclip_remap_scene_to_clip_frame(clip, ctime);

	if (data->flag & FOLLOWTRACK_USE_3D_POSITION) {
		if (track->flag & TRACK_HAS_BUNDLE) {
			float obmat[4][4], mat[4][4];

			copy_m4_m4(obmat, cob->matrix);

			if ((tracking_object->flag & TRACKING_OBJECT_CAMERA) == 0) {
				float imat[4][4];

				copy_m4_m4(mat, camob->obmat);

				BKE_tracking_camera_get_reconstructed_interpolate(tracking, tracking_object, framenr, imat);
				invert_m4(imat);

				mul_m4_series(cob->matrix, obmat, mat, imat);
				translate_m4(cob->matrix, track->bundle_pos[0], track->bundle_pos[1], track->bundle_pos[2]);
			}
			else {
				BKE_tracking_get_camera_object_matrix(cob->scene, camob, mat);

				mul_m4_m4m4(cob->matrix, obmat, mat);
				translate_m4(cob->matrix, track->bundle_pos[0], track->bundle_pos[1], track->bundle_pos[2]);
			}
		}
	}
	else {
		float vec[3], disp[3], axis[3], mat[4][4];
		float aspect = (scene->r.xsch * scene->r.xasp) / (scene->r.ysch * scene->r.yasp);
		float len, d;

		BKE_object_where_is_calc_mat4(scene, camob, mat);

		/* camera axis */
		vec[0] = 0.0f;
		vec[1] = 0.0f;
		vec[2] = 1.0f;
		mul_v3_m4v3(axis, mat, vec);

		/* distance to projection plane */
		copy_v3_v3(vec, cob->matrix[3]);
		sub_v3_v3(vec, mat[3]);
		project_v3_v3v3(disp, vec, axis);

		len = len_v3(disp);

		if (len > FLT_EPSILON) {
			CameraParams params;
			int width, height;
			float pos[2], rmat[4][4];

			BKE_movieclip_get_size(clip, NULL, &width, &height);
			BKE_tracking_marker_get_subframe_position(track, framenr, pos);

			if (data->flag & FOLLOWTRACK_USE_UNDISTORTION) {
				/* Undistortion need to happen in pixel space. */
				pos[0] *= width;
				pos[1] *= height;

				BKE_tracking_undistort_v2(tracking, pos, pos);

				/* Normalize pixel coordinates back. */
				pos[0] /= width;
				pos[1] /= height;
			}

			/* aspect correction */
			if (data->frame_method != FOLLOWTRACK_FRAME_STRETCH) {
				float w_src, h_src, w_dst, h_dst, asp_src, asp_dst;

				/* apply clip display aspect */
				w_src = width * clip->aspx;
				h_src = height * clip->aspy;

				w_dst = scene->r.xsch * scene->r.xasp;
				h_dst = scene->r.ysch * scene->r.yasp;

				asp_src = w_src / h_src;
				asp_dst = w_dst / h_dst;

				if (fabsf(asp_src - asp_dst) >= FLT_EPSILON) {
					if ((asp_src > asp_dst) == (data->frame_method == FOLLOWTRACK_FRAME_CROP)) {
						/* fit X */
						float div = asp_src / asp_dst;
						float cent = (float) width / 2.0f;

						pos[0] = (((pos[0] * width - cent) * div) + cent) / width;
					}
					else {
						/* fit Y */
						float div = asp_dst / asp_src;
						float cent = (float) height / 2.0f;

						pos[1] = (((pos[1] * height - cent) * div) + cent) / height;
					}
				}
			}

			BKE_camera_params_init(&params);
			BKE_camera_params_from_object(&params, camob);

			if (params.is_ortho) {
				vec[0] = params.ortho_scale * (pos[0] - 0.5f + params.shiftx);
				vec[1] = params.ortho_scale * (pos[1] - 0.5f + params.shifty);
				vec[2] = -len;

				if (aspect > 1.0f)
					vec[1] /= aspect;
				else
					vec[0] *= aspect;

				mul_v3_m4v3(disp, camob->obmat, vec);

				copy_m4_m4(rmat, camob->obmat);
				zero_v3(rmat[3]);
				mul_m4_m4m4(cob->matrix, cob->matrix, rmat);

				copy_v3_v3(cob->matrix[3], disp);
			}
			else {
				d =  (len * params.sensor_x) / (2.0f * params.lens);

				vec[0] = d * (2.0f * (pos[0] + params.shiftx) - 1.0f);
				vec[1] = d * (2.0f * (pos[1] + params.shifty) - 1.0f);
				vec[2] = -len;

				if (aspect > 1.0f)
					vec[1] /= aspect;
				else
					vec[0] *= aspect;

				mul_v3_m4v3(disp, camob->obmat, vec);

				/* apply camera rotation so Z-axis would be co-linear */
				copy_m4_m4(rmat, camob->obmat);
				zero_v3(rmat[3]);
				mul_m4_m4m4(cob->matrix, cob->matrix, rmat);

				copy_v3_v3(cob->matrix[3], disp);
			}

			if (data->depth_ob) {
				Object *depth_ob = data->depth_ob;
				/* TODO(sergey): use proper for_render flag here when known. */
				DerivedMesh *target = object_get_derived_final(depth_ob, false);
				if (target) {
					BVHTreeFromMesh treeData = NULL_BVHTreeFromMesh;
					BVHTreeRayHit hit;
					float ray_start[3], ray_end[3], ray_nor[3], imat[4][4];
					int result;

					invert_m4_m4(imat, depth_ob->obmat);

					mul_v3_m4v3(ray_start, imat, camob->obmat[3]);
					mul_v3_m4v3(ray_end, imat, cob->matrix[3]);

					sub_v3_v3v3(ray_nor, ray_end, ray_start);
					normalize_v3(ray_nor);

					bvhtree_from_mesh_looptri(&treeData, target, 0.0f, 4, 6);

					hit.dist = BVH_RAYCAST_DIST_MAX;
					hit.index = -1;

					result = BLI_bvhtree_ray_cast(treeData.tree, ray_start, ray_nor, 0.0f, &hit, treeData.raycast_callback, &treeData);

					if (result != -1) {
						mul_v3_m4v3(cob->matrix[3], depth_ob->obmat, hit.co);
					}

					free_bvhtree_from_mesh(&treeData);
					target->release(target);
				}
			}
		}
	}
}

static bConstraintTypeInfo CTI_FOLLOWTRACK = {
	CONSTRAINT_TYPE_FOLLOWTRACK, /* type */
	sizeof(bFollowTrackConstraint), /* size */
	"Follow Track", /* name */
	"bFollowTrackConstraint", /* struct name */
	NULL, /* free data */
	followtrack_id_looper, /* id looper */
	NULL, /* copy data */
	followtrack_new_data, /* new data */
	NULL, /* get constraint targets */
	NULL, /* flush constraint targets */
	NULL, /* get target matrix */
	followtrack_evaluate /* evaluate */
};

/* ----------- Camre Solver ------------- */

static void camerasolver_new_data(void *cdata)
{
	bCameraSolverConstraint *data = (bCameraSolverConstraint *)cdata;

	data->clip = NULL;
	data->flag |= CAMERASOLVER_ACTIVECLIP;
}

static void camerasolver_id_looper(bConstraint *con, ConstraintIDFunc func, void *userdata)
{
	bCameraSolverConstraint *data = con->data;

	func(con, (ID **)&data->clip, true, userdata);
}

static void camerasolver_evaluate(bConstraint *con, bConstraintOb *cob, ListBase *UNUSED(targets))
{
	Scene *scene = cob->scene;
	bCameraSolverConstraint *data = con->data;
	MovieClip *clip = data->clip;

	if (data->flag & CAMERASOLVER_ACTIVECLIP)
		clip = scene->clip;

	if (clip) {
		float mat[4][4], obmat[4][4];
		MovieTracking *tracking = &clip->tracking;
		MovieTrackingObject *object = BKE_tracking_object_get_camera(tracking);
		float ctime = BKE_scene_frame_get(scene);
		float framenr = BKE_movieclip_remap_scene_to_clip_frame(clip, ctime);

		BKE_tracking_camera_get_reconstructed_interpolate(tracking, object, framenr, mat);

		copy_m4_m4(obmat, cob->matrix);

		mul_m4_m4m4(cob->matrix, obmat, mat);
	}
}

static bConstraintTypeInfo CTI_CAMERASOLVER = {
	CONSTRAINT_TYPE_CAMERASOLVER, /* type */
	sizeof(bCameraSolverConstraint), /* size */
	"Camera Solver", /* name */
	"bCameraSolverConstraint", /* struct name */
	NULL, /* free data */
	camerasolver_id_looper, /* id looper */
	NULL, /* copy data */
	camerasolver_new_data, /* new data */
	NULL, /* get constraint targets */
	NULL, /* flush constraint targets */
	NULL, /* get target matrix */
	camerasolver_evaluate /* evaluate */
};

/* ----------- Object Solver ------------- */

static void objectsolver_new_data(void *cdata)
{
	bObjectSolverConstraint *data = (bObjectSolverConstraint *)cdata;

	data->clip = NULL;
	data->flag |= OBJECTSOLVER_ACTIVECLIP;
	unit_m4(data->invmat);
}

static void objectsolver_id_looper(bConstraint *con, ConstraintIDFunc func, void *userdata)
{
	bObjectSolverConstraint *data = con->data;

	func(con, (ID **)&data->clip, false, userdata);
	func(con, (ID **)&data->camera, false, userdata);
}

static void objectsolver_evaluate(bConstraint *con, bConstraintOb *cob, ListBase *UNUSED(targets))
{
	Scene *scene = cob->scene;
	bObjectSolverConstraint *data = con->data;
	MovieClip *clip = data->clip;
	Object *camob = data->camera ? data->camera : scene->camera;

	if (data->flag & OBJECTSOLVER_ACTIVECLIP)
		clip = scene->clip;

	if (!camob || !clip)
		return;

	if (clip) {
		MovieTracking *tracking = &clip->tracking;
		MovieTrackingObject *object;

		object = BKE_tracking_object_get_named(tracking, data->object);

		if (object) {
			float mat[4][4], obmat[4][4], imat[4][4], cammat[4][4], camimat[4][4], parmat[4][4];
			float ctime = BKE_scene_frame_get(scene);
			float framenr = BKE_movieclip_remap_scene_to_clip_frame(clip, ctime);

			BKE_object_where_is_calc_mat4(scene, camob, cammat);

			BKE_tracking_camera_get_reconstructed_interpolate(tracking, object, framenr, mat);

			invert_m4_m4(camimat, cammat);
			mul_m4_m4m4(parmat, cammat, data->invmat);

			copy_m4_m4(cammat, camob->obmat);
			copy_m4_m4(obmat, cob->matrix);

			invert_m4_m4(imat, mat);

			mul_m4_series(cob->matrix, cammat, imat, camimat, parmat, obmat);
		}
	}
}

static bConstraintTypeInfo CTI_OBJECTSOLVER = {
	CONSTRAINT_TYPE_OBJECTSOLVER, /* type */
	sizeof(bObjectSolverConstraint), /* size */
	"Object Solver", /* name */
	"bObjectSolverConstraint", /* struct name */
	NULL, /* free data */
	objectsolver_id_looper, /* id looper */
	NULL, /* copy data */
	objectsolver_new_data, /* new data */
	NULL, /* get constraint targets */
	NULL, /* flush constraint targets */
	NULL, /* get target matrix */
	objectsolver_evaluate /* evaluate */
};

/* ----------- Transform Cache ------------- */

static void transformcache_id_looper(bConstraint *con, ConstraintIDFunc func, void *userdata)
{
	bTransformCacheConstraint *data = con->data;
	func(con, (ID **)&data->cache_file, true, userdata);
}

static void transformcache_evaluate(bConstraint *con, bConstraintOb *cob, ListBase *targets)
{
#ifdef WITH_ALEMBIC
	bTransformCacheConstraint *data = con->data;
	Scene *scene = cob->scene;

	CacheFile *cache_file = data->cache_file;

	if (!cache_file) {
		return;
	}

	const float frame = BKE_scene_frame_get(scene);
	const float time = BKE_cachefile_time_offset(cache_file, frame, FPS);

	BKE_cachefile_ensure_handle(G.main, cache_file);

	if (!data->reader) {
		data->reader = CacheReader_open_alembic_object(cache_file->handle,
		                                               data->reader,
		                                               cob->ob,
		                                               data->object_path);
	}

	ABC_get_transform(data->reader, cob->matrix, time, cache_file->scale);
#else
	UNUSED_VARS(con, cob);
#endif

	UNUSED_VARS(targets);
}

static void transformcache_copy(bConstraint *con, bConstraint *srccon)
{
	bTransformCacheConstraint *src = srccon->data;
	bTransformCacheConstraint *dst = con->data;

	BLI_strncpy(dst->object_path, src->object_path, sizeof(dst->object_path));
	dst->cache_file = src->cache_file;

#ifdef WITH_ALEMBIC
	if (dst->reader) {
		CacheReader_incref(dst->reader);
	}
#endif
}

static void transformcache_free(bConstraint *con)
{
	bTransformCacheConstraint *data = con->data;

	if (data->reader) {
#ifdef WITH_ALEMBIC
		CacheReader_free(data->reader);
#endif
		data->reader = NULL;
	}
}

static void transformcache_new_data(void *cdata)
{
	bTransformCacheConstraint *data = (bTransformCacheConstraint *)cdata;

	data->cache_file = NULL;
}

static bConstraintTypeInfo CTI_TRANSFORM_CACHE = {
	CONSTRAINT_TYPE_TRANSFORM_CACHE, /* type */
	sizeof(bTransformCacheConstraint), /* size */
	"Transform Cache", /* name */
	"bTransformCacheConstraint", /* struct name */
	transformcache_free,  /* free data */
	transformcache_id_looper,  /* id looper */
	transformcache_copy,  /* copy data */
	transformcache_new_data,  /* new data */
	NULL,  /* get constraint targets */
	NULL,  /* flush constraint targets */
	NULL,  /* get target matrix */
	transformcache_evaluate  /* evaluate */
};

/* ************************* Constraints Type-Info *************************** */
/* All of the constraints api functions use bConstraintTypeInfo structs to carry out
 * and operations that involve constraint specific code.
 */

/* These globals only ever get directly accessed in this file */
static bConstraintTypeInfo *constraintsTypeInfo[NUM_CONSTRAINT_TYPES];
static short CTI_INIT = 1; /* when non-zero, the list needs to be updated */

/* This function only gets called when CTI_INIT is non-zero */
static void constraints_init_typeinfo(void)
{
	constraintsTypeInfo[0] =  NULL;                  /* 'Null' Constraint */
	constraintsTypeInfo[1] =  &CTI_CHILDOF;          /* ChildOf Constraint */
	constraintsTypeInfo[2] =  &CTI_TRACKTO;          /* TrackTo Constraint */
	constraintsTypeInfo[3] =  &CTI_KINEMATIC;        /* IK Constraint */
	constraintsTypeInfo[4] =  &CTI_FOLLOWPATH;       /* Follow-Path Constraint */
	constraintsTypeInfo[5] =  &CTI_ROTLIMIT;         /* Limit Rotation Constraint */
	constraintsTypeInfo[6] =  &CTI_LOCLIMIT;         /* Limit Location Constraint */
	constraintsTypeInfo[7] =  &CTI_SIZELIMIT;        /* Limit Scale Constraint */
	constraintsTypeInfo[8] =  &CTI_ROTLIKE;          /* Copy Rotation Constraint */
	constraintsTypeInfo[9] =  &CTI_LOCLIKE;          /* Copy Location Constraint */
	constraintsTypeInfo[10] = &CTI_SIZELIKE;         /* Copy Scale Constraint */
	constraintsTypeInfo[11] = &CTI_PYTHON;           /* Python/Script Constraint */
	constraintsTypeInfo[12] = &CTI_ACTION;           /* Action Constraint */
	constraintsTypeInfo[13] = &CTI_LOCKTRACK;        /* Locked-Track Constraint */
	constraintsTypeInfo[14] = &CTI_DISTLIMIT;        /* Limit Distance Constraint */
	constraintsTypeInfo[15] = &CTI_STRETCHTO;        /* StretchTo Constaint */
	constraintsTypeInfo[16] = &CTI_MINMAX;           /* Floor Constraint */
	constraintsTypeInfo[17] = &CTI_RIGIDBODYJOINT;   /* RigidBody Constraint */
	constraintsTypeInfo[18] = &CTI_CLAMPTO;          /* ClampTo Constraint */
	constraintsTypeInfo[19] = &CTI_TRANSFORM;        /* Transformation Constraint */
	constraintsTypeInfo[20] = &CTI_SHRINKWRAP;       /* Shrinkwrap Constraint */
	constraintsTypeInfo[21] = &CTI_DAMPTRACK;        /* Damped TrackTo Constraint */
	constraintsTypeInfo[22] = &CTI_SPLINEIK;         /* Spline IK Constraint */
	constraintsTypeInfo[23] = &CTI_TRANSLIKE;        /* Copy Transforms Constraint */
	constraintsTypeInfo[24] = &CTI_SAMEVOL;          /* Maintain Volume Constraint */
	constraintsTypeInfo[25] = &CTI_PIVOT;            /* Pivot Constraint */
	constraintsTypeInfo[26] = &CTI_FOLLOWTRACK;      /* Follow Track Constraint */
	constraintsTypeInfo[27] = &CTI_CAMERASOLVER;     /* Camera Solver Constraint */
	constraintsTypeInfo[28] = &CTI_OBJECTSOLVER;     /* Object Solver Constraint */
	constraintsTypeInfo[29] = &CTI_TRANSFORM_CACHE;  /* Transform Cache Constraint */
}

/* This function should be used for getting the appropriate type-info when only
 * a constraint type is known
 */
const bConstraintTypeInfo *BKE_constraint_typeinfo_from_type(int type)
{
	/* initialize the type-info list? */
	if (CTI_INIT) {
		constraints_init_typeinfo();
		CTI_INIT = 0;
	}
	
	/* only return for valid types */
	if ((type >= CONSTRAINT_TYPE_NULL) &&
	    (type < NUM_CONSTRAINT_TYPES))
	{
		/* there shouldn't be any segfaults here... */
		return constraintsTypeInfo[type];
	}
	else {
		printf("No valid constraint type-info data available. Type = %i\n", type);
	}
	
	return NULL;
} 
 
/* This function should always be used to get the appropriate type-info, as it
 * has checks which prevent segfaults in some weird cases.
 */
const bConstraintTypeInfo *BKE_constraint_typeinfo_get(bConstraint *con)
{
	/* only return typeinfo for valid constraints */
	if (con)
		return BKE_constraint_typeinfo_from_type(con->type);
	else
		return NULL;
}

/* ************************* General Constraints API ************************** */
/* The functions here are called by various parts of Blender. Very few (should be none if possible)
 * constraint-specific code should occur here.
 */
 
/* ---------- Data Management ------- */

/* helper function for BKE_constraint_free_data() - unlinks references */
static void con_unlink_refs_cb(bConstraint *UNUSED(con), ID **idpoin, bool is_reference, void *UNUSED(userData))
{
	if (*idpoin && is_reference)
		id_us_min(*idpoin);
}

/* Free data of a specific constraint if it has any info.
 * be sure to run BIK_clear_data() when freeing an IK constraint,
 * unless DAG_relations_tag_update is called. 
 */
void BKE_constraint_free_data_ex(bConstraint *con, bool do_id_user)
{
	if (con->data) {
		const bConstraintTypeInfo *cti = BKE_constraint_typeinfo_get(con);
		
		if (cti) {
			/* perform any special freeing constraint may have */
			if (cti->free_data)
				cti->free_data(con);
				
			/* unlink the referenced resources it uses */
			if (do_id_user && cti->id_looper)
				cti->id_looper(con, con_unlink_refs_cb, NULL);
		}
		
		/* free constraint data now */
		MEM_freeN(con->data);
	}
}

void BKE_constraint_free_data(bConstraint *con)
{
	BKE_constraint_free_data_ex(con, true);
}

/* Free all constraints from a constraint-stack */
void BKE_constraints_free_ex(ListBase *list, bool do_id_user)
{
	bConstraint *con;
	
	/* Free constraint data and also any extra data */
	for (con = list->first; con; con = con->next)
		BKE_constraint_free_data_ex(con, do_id_user);
	
	/* Free the whole list */
	BLI_freelistN(list);
}

void BKE_constraints_free(ListBase *list)
{
	BKE_constraints_free_ex(list, true);
}

/* Remove the specified constraint from the given constraint stack */
bool BKE_constraint_remove(ListBase *list, bConstraint *con)
{
	if (con) {
		BKE_constraint_free_data(con);
		BLI_freelinkN(list, con);
		return true;
	}
	else {
		return false;
	}
}

bool BKE_constraint_remove_ex(ListBase *list, Object *ob, bConstraint *con, bool clear_dep)
{
	const short type = con->type;
	if (BKE_constraint_remove(list, con)) {
		/* ITASC needs to be rebuilt once a constraint is removed [#26920] */
		if (clear_dep && ELEM(type, CONSTRAINT_TYPE_KINEMATIC, CONSTRAINT_TYPE_SPLINEIK)) {
			BIK_clear_data(ob->pose);
		}
		return true;
	}
	else {
		return false;
	}
}

/* ......... */

/* Creates a new constraint, initializes its data, and returns it */
static bConstraint *add_new_constraint_internal(const char *name, short type)
{
	bConstraint *con = MEM_callocN(sizeof(bConstraint), "Constraint");
	const bConstraintTypeInfo *cti = BKE_constraint_typeinfo_from_type(type);
	const char *newName;

	/* Set up a generic constraint datablock */
	con->type = type;
	con->flag |= CONSTRAINT_EXPAND;
	con->enforce = 1.0f;

	/* Determine a basic name, and info */
	if (cti) {
		/* initialize constraint data */
		con->data = MEM_callocN(cti->size, cti->structName);
		
		/* only constraints that change any settings need this */
		if (cti->new_data)
			cti->new_data(con->data);
		
		/* if no name is provided, use the type of the constraint as the name */
		newName = (name && name[0]) ? name : DATA_(cti->name);
	}
	else {
		/* if no name is provided, use the generic "Const" name */
		/* NOTE: any constraint type that gets here really shouldn't get added... */
		newName = (name && name[0]) ? name : DATA_("Const");
	}
	
	/* copy the name */
	BLI_strncpy(con->name, newName, sizeof(con->name));
	
	/* return the new constraint */
	return con;
}

/* if pchan is not NULL then assume we're adding a pose constraint */
static bConstraint *add_new_constraint(Object *ob, bPoseChannel *pchan, const char *name, short type)
{
	bConstraint *con;
	ListBase *list;
	
	/* add the constraint */
	con = add_new_constraint_internal(name, type);
	
	/* find the constraint stack - bone or object? */
	list = (pchan) ? (&pchan->constraints) : (&ob->constraints);
	
	if (list) {
		/* add new constraint to end of list of constraints before ensuring that it has a unique name
		 * (otherwise unique-naming code will fail, since it assumes element exists in list)
		 */
		BLI_addtail(list, con);
		BKE_constraint_unique_name(con, list);
		
		/* if the target list is a list on some PoseChannel belonging to a proxy-protected
		 * Armature layer, we must tag newly added constraints with a flag which allows them
		 * to persist after proxy syncing has been done
		 */
		if (BKE_constraints_proxylocked_owner(ob, pchan))
			con->flag |= CONSTRAINT_PROXY_LOCAL;
		
		/* make this constraint the active one */
		BKE_constraints_active_set(list, con);
	}

	/* set type+owner specific immutable settings */
	/* TODO: does action constraint need anything here - i.e. spaceonce? */
	switch (type) {
		case CONSTRAINT_TYPE_CHILDOF:
		{
			/* if this constraint is being added to a posechannel, make sure
			 * the constraint gets evaluated in pose-space */
			if (pchan) {
				con->ownspace = CONSTRAINT_SPACE_POSE;
				con->flag |= CONSTRAINT_SPACEONCE;
			}
			break;
		}
	}
	
	return con;
}

/* ......... */

/* Add new constraint for the given bone */
bConstraint *BKE_constraint_add_for_pose(Object *ob, bPoseChannel *pchan, const char *name, short type)
{
	if (pchan == NULL)
		return NULL;
	
	return add_new_constraint(ob, pchan, name, type);
}

/* Add new constraint for the given object */
bConstraint *BKE_constraint_add_for_object(Object *ob, const char *name, short type)
{
	return add_new_constraint(ob, NULL, name, type);
}

/* ......... */

/* Run the given callback on all ID-blocks in list of constraints */
void BKE_constraints_id_loop(ListBase *conlist, ConstraintIDFunc func, void *userdata)
{
	bConstraint *con;
	
	for (con = conlist->first; con; con = con->next) {
		const bConstraintTypeInfo *cti = BKE_constraint_typeinfo_get(con);
		
		if (cti) {
			if (cti->id_looper)
				cti->id_looper(con, func, userdata);
		}
	}
}

/* ......... */

/* helper for BKE_constraints_copy(), to be used for making sure that ID's are valid */
static void con_extern_cb(bConstraint *UNUSED(con), ID **idpoin, bool UNUSED(is_reference), void *UNUSED(userData))
{
	if (*idpoin && ID_IS_LINKED_DATABLOCK(*idpoin))
		id_lib_extern(*idpoin);
}

/* helper for BKE_constraints_copy(), to be used for making sure that usercounts of copied ID's are fixed up */
static void con_fix_copied_refs_cb(bConstraint *UNUSED(con), ID **idpoin, bool is_reference, void *UNUSED(userData))
{
	/* increment usercount if this is a reference type */
	if ((*idpoin) && (is_reference))
		id_us_plus(*idpoin);
}

/* duplicate all of the constraints in a constraint stack */
void BKE_constraints_copy(ListBase *dst, const ListBase *src, bool do_extern)
{
	bConstraint *con, *srccon;
	
	BLI_listbase_clear(dst);
	BLI_duplicatelist(dst, src);
	
	for (con = dst->first, srccon = src->first; con && srccon; srccon = srccon->next, con = con->next) {
		const bConstraintTypeInfo *cti = BKE_constraint_typeinfo_get(con);
		
		/* make a new copy of the constraint's data */
		con->data = MEM_dupallocN(con->data);
		
		/* only do specific constraints if required */
		if (cti) {
			/* perform custom copying operations if needed */
			if (cti->copy_data)
				cti->copy_data(con, srccon);
				
			/* fix usercounts for all referenced data in referenced data */
			if (cti->id_looper)
				cti->id_looper(con, con_fix_copied_refs_cb, NULL);
			
			/* for proxies we don't want to make extern */
			if (do_extern) {
				/* go over used ID-links for this constraint to ensure that they are valid for proxies */
				if (cti->id_looper)
					cti->id_looper(con, con_extern_cb, NULL);
			}
		}
	}
}

/* ......... */

bConstraint *BKE_constraints_find_name(ListBase *list, const char *name)
{
	return BLI_findstring(list, name, offsetof(bConstraint, name));
}

/* finds the 'active' constraint in a constraint stack */
bConstraint *BKE_constraints_active_get(ListBase *list)
{
	bConstraint *con;
	
	/* search for the first constraint with the 'active' flag set */
	if (list) {
		for (con = list->first; con; con = con->next) {
			if (con->flag & CONSTRAINT_ACTIVE)
				return con;
		}
	}
	
	/* no active constraint found */
	return NULL;
}

/* Set the given constraint as the active one (clearing all the others) */
void BKE_constraints_active_set(ListBase *list, bConstraint *con)
{
	bConstraint *c;
	
	if (list) {
		for (c = list->first; c; c = c->next) {
			if (c == con) 
				c->flag |= CONSTRAINT_ACTIVE;
			else 
				c->flag &= ~CONSTRAINT_ACTIVE;
		}
	}
}

/* -------- Constraints and Proxies ------- */

/* Rescue all constraints tagged as being CONSTRAINT_PROXY_LOCAL (i.e. added to bone that's proxy-synced in this file) */
void BKE_constraints_proxylocal_extract(ListBase *dst, ListBase *src)
{
	bConstraint *con, *next;
	
	/* for each tagged constraint, remove from src and move to dst */
	for (con = src->first; con; con = next) {
		next = con->next;
		
		/* check if tagged */
		if (con->flag & CONSTRAINT_PROXY_LOCAL) {
			BLI_remlink(src, con);
			BLI_addtail(dst, con);
		}
	}
}

/* Returns if the owner of the constraint is proxy-protected */
bool BKE_constraints_proxylocked_owner(Object *ob, bPoseChannel *pchan)
{
	/* Currently, constraints can only be on object or bone level */
	if (ob && ob->proxy) {
		if (ob->pose && pchan) {
			bArmature *arm = ob->data;
			
			/* On bone-level, check if bone is on proxy-protected layer */
			if ((pchan->bone) && (pchan->bone->layer & arm->layer_protected))
				return true;
		}
		else {
			/* FIXME: constraints on object-level are not handled well yet */
			return true;
		}
	}
	
	return false;
}

/* -------- Target-Matrix Stuff ------- */

/* This function is a relic from the prior implementations of the constraints system, when all
 * constraints either had one or no targets. It used to be called during the main constraint solving
 * loop, but is now only used for the remaining cases for a few constraints. 
 *
 * None of the actual calculations of the matrices should be done here! Also, this function is
 * not to be used by any new constraints, particularly any that have multiple targets.
 */
void BKE_constraint_target_matrix_get(Scene *scene, bConstraint *con, int index, short ownertype, void *ownerdata, float mat[4][4], float ctime)
{
	const bConstraintTypeInfo *cti = BKE_constraint_typeinfo_get(con);
	ListBase targets = {NULL, NULL};
	bConstraintOb *cob;
	bConstraintTarget *ct;
	
	if (cti && cti->get_constraint_targets) {
		/* make 'constraint-ob' */
		cob = MEM_callocN(sizeof(bConstraintOb), "tempConstraintOb");
		cob->type = ownertype;
		cob->scene = scene;
		switch (ownertype) {
			case CONSTRAINT_OBTYPE_OBJECT: /* it is usually this case */
			{
				cob->ob = (Object *)ownerdata;
				cob->pchan = NULL;
				if (cob->ob) {
					copy_m4_m4(cob->matrix, cob->ob->obmat);
					copy_m4_m4(cob->startmat, cob->matrix);
				}
				else {
					unit_m4(cob->matrix);
					unit_m4(cob->startmat);
				}
				break;
			}
			case CONSTRAINT_OBTYPE_BONE: /* this may occur in some cases */
			{
				cob->ob = NULL; /* this might not work at all :/ */
				cob->pchan = (bPoseChannel *)ownerdata;
				if (cob->pchan) {
					copy_m4_m4(cob->matrix, cob->pchan->pose_mat);
					copy_m4_m4(cob->startmat, cob->matrix);
				}
				else {
					unit_m4(cob->matrix);
					unit_m4(cob->startmat);
				}
				break;
			}
		}
		
		/* get targets - we only need the first one though (and there should only be one) */
		cti->get_constraint_targets(con, &targets);
		
		/* only calculate the target matrix on the first target */
		ct = (bConstraintTarget *)BLI_findlink(&targets, index);
		
		if (ct) {
			if (cti->get_target_matrix)
				cti->get_target_matrix(con, cob, ct, ctime);
			copy_m4_m4(mat, ct->matrix);
		}
		
		/* free targets + 'constraint-ob' */
		if (cti->flush_constraint_targets)
			cti->flush_constraint_targets(con, &targets, 1);
		MEM_freeN(cob);
	}
	else {
		/* invalid constraint - perhaps... */
		unit_m4(mat);
	}
}

/* Get the list of targets required for solving a constraint */
void BKE_constraint_targets_for_solving_get(bConstraint *con, bConstraintOb *cob, ListBase *targets, float ctime)
{
	const bConstraintTypeInfo *cti = BKE_constraint_typeinfo_get(con);
	
	if (cti && cti->get_constraint_targets) {
		bConstraintTarget *ct;
		
		/* get targets 
		 *  - constraints should use ct->matrix, not directly accessing values
		 *	- ct->matrix members have not yet been calculated here! 
		 */
		cti->get_constraint_targets(con, targets);
		
		/* set matrices 
		 *  - calculate if possible, otherwise just initialize as identity matrix
		 */
		if (cti->get_target_matrix) {
			for (ct = targets->first; ct; ct = ct->next)
				cti->get_target_matrix(con, cob, ct, ctime);
		}
		else {
			for (ct = targets->first; ct; ct = ct->next)
				unit_m4(ct->matrix);
		}
	}
}
 
/* ---------- Evaluation ----------- */

/* This function is called whenever constraints need to be evaluated. Currently, all
 * constraints that can be evaluated are every time this gets run.
 *
 * BKE_constraints_make_evalob and BKE_constraints_clear_evalob should be called before and 
 * after running this function, to sort out cob
 */
void BKE_constraints_solve(ListBase *conlist, bConstraintOb *cob, float ctime)
{
	bConstraint *con;
	float oldmat[4][4];
	float enf;

	/* check that there is a valid constraint object to evaluate */
	if (cob == NULL)
		return;
	
	/* loop over available constraints, solving and blending them */
	for (con = conlist->first; con; con = con->next) {
		const bConstraintTypeInfo *cti = BKE_constraint_typeinfo_get(con);
		ListBase targets = {NULL, NULL};
		
		/* these we can skip completely (invalid constraints...) */
		if (cti == NULL) continue;
		if (con->flag & (CONSTRAINT_DISABLE | CONSTRAINT_OFF)) continue;
		/* these constraints can't be evaluated anyway */
		if (cti->evaluate_constraint == NULL) continue;
		/* influence == 0 should be ignored */
		if (con->enforce == 0.0f) continue;
		
		/* influence of constraint
		 *  - value should have been set from animation data already
		 */
		enf = con->enforce;
		
		/* make copy of worldspace matrix pre-constraint for use with blending later */
		copy_m4_m4(oldmat, cob->matrix);
		
		/* move owner matrix into right space */
		BKE_constraint_mat_convertspace(cob->ob, cob->pchan, cob->matrix, CONSTRAINT_SPACE_WORLD, con->ownspace, false);
		
		/* prepare targets for constraint solving */
		BKE_constraint_targets_for_solving_get(con, cob, &targets, ctime);
		
		/* Solve the constraint and put result in cob->matrix */
		cti->evaluate_constraint(con, cob, &targets);
		
		/* clear targets after use 
		 *	- this should free temp targets but no data should be copied back
		 *	  as constraints may have done some nasty things to it...
		 */
		if (cti->flush_constraint_targets) {
			cti->flush_constraint_targets(con, &targets, 1);
		}
		
		/* move owner back into worldspace for next constraint/other business */
		if ((con->flag & CONSTRAINT_SPACEONCE) == 0) 
			BKE_constraint_mat_convertspace(cob->ob, cob->pchan, cob->matrix, con->ownspace, CONSTRAINT_SPACE_WORLD, false);
			
		/* Interpolate the enforcement, to blend result of constraint into final owner transform 
		 *  - all this happens in worldspace to prevent any weirdness creeping in ([#26014] and [#25725]),
		 *    since some constraints may not convert the solution back to the input space before blending
		 *    but all are guaranteed to end up in good "worldspace" result
		 */
		/* Note: all kind of stuff here before (caused trouble), much easier to just interpolate,
		 * or did I miss something? -jahka (r.32105) */
		if (enf < 1.0f) {
			float solution[4][4];
			copy_m4_m4(solution, cob->matrix);
			interp_m4_m4m4(cob->matrix, oldmat, solution, enf);
		}
	}
}
