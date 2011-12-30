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
#include "BLI_editVert.h"
#include "BLI_utildefines.h"

#include "DNA_armature_types.h"
#include "DNA_camera_types.h"
#include "DNA_constraint_types.h"
#include "DNA_modifier_types.h"
#include "DNA_object_types.h"
#include "DNA_action_types.h"
#include "DNA_curve_types.h"
#include "DNA_mesh_types.h"
#include "DNA_meshdata_types.h"

#include "DNA_lattice_types.h"
#include "DNA_scene_types.h"
#include "DNA_text_types.h"
#include "DNA_tracking_types.h"
#include "DNA_movieclip_types.h"


#include "BKE_action.h"
#include "BKE_anim.h" /* for the curve calculation part */
#include "BKE_armature.h"
#include "BKE_blender.h"
#include "BKE_camera.h"
#include "BKE_constraint.h"
#include "BKE_displist.h"
#include "BKE_deform.h"
#include "BKE_DerivedMesh.h"	/* for geometry targets */
#include "BKE_cdderivedmesh.h" /* for geometry targets */
#include "BKE_object.h"
#include "BKE_ipo.h"
#include "BKE_global.h"
#include "BKE_library.h"
#include "BKE_idprop.h"
#include "BKE_shrinkwrap.h"
#include "BKE_mesh.h"
#include "BKE_tracking.h"
#include "BKE_movieclip.h"

#ifdef WITH_PYTHON
#include "BPY_extern.h"
#endif

#ifndef M_PI
#define M_PI		3.14159265358979323846
#endif



/* ************************ Constraints - General Utilities *************************** */
/* These functions here don't act on any specific constraints, and are therefore should/will
 * not require any of the special function-pointers afforded by the relevant constraint 
 * type-info structs.
 */

/* -------------- Naming -------------- */

/* Find the first available, non-duplicate name for a given constraint */
void unique_constraint_name (bConstraint *con, ListBase *list)
{
	BLI_uniquename(list, con, "Const", '.', offsetof(bConstraint, name), sizeof(con->name));
}

/* ----------------- Evaluation Loop Preparation --------------- */

/* package an object/bone for use in constraint evaluation */
/* This function MEM_calloc's a bConstraintOb struct, that will need to be freed after evaluation */
bConstraintOb *constraints_make_evalob (Scene *scene, Object *ob, void *subdata, short datatype)
{
	bConstraintOb *cob;
	
	/* create regardless of whether we have any data! */
	cob= MEM_callocN(sizeof(bConstraintOb), "bConstraintOb");
	
	/* for system time, part of deglobalization, code nicer later with local time (ton) */
	cob->scene= scene;
	
	/* based on type of available data */
	switch (datatype) {
		case CONSTRAINT_OBTYPE_OBJECT:
		{
			/* disregard subdata... calloc should set other values right */
			if (ob) {
				cob->ob = ob;
				cob->type = datatype;
				cob->rotOrder = EULER_ORDER_DEFAULT; // TODO: when objects have rotation order too, use that
				copy_m4_m4(cob->matrix, ob->obmat);
			}
			else
				unit_m4(cob->matrix);
			
			copy_m4_m4(cob->startmat, cob->matrix);
		}
			break;
		case CONSTRAINT_OBTYPE_BONE:
		{
			/* only set if we have valid bone, otherwise default */
			if (ob && subdata) {
				cob->ob = ob;
				cob->pchan = (bPoseChannel *)subdata;
				cob->type = datatype;
				
				if (cob->pchan->rotmode > 0) {
					/* should be some type of Euler order */
					cob->rotOrder= cob->pchan->rotmode; 
				}
				else {
					/* Quats, so eulers should just use default order */
					cob->rotOrder= EULER_ORDER_DEFAULT;
				}
				
				/* matrix in world-space */
				mult_m4_m4m4(cob->matrix, ob->obmat, cob->pchan->pose_mat);
			}
			else
				unit_m4(cob->matrix);
				
			copy_m4_m4(cob->startmat, cob->matrix);
		}
			break;
			
		default: /* other types not yet handled */
			unit_m4(cob->matrix);
			unit_m4(cob->startmat);
			break;
	}
	
	return cob;
}

/* cleanup after constraint evaluation */
void constraints_clear_evalob (bConstraintOb *cob)
{
	float delta[4][4], imat[4][4];
	
	/* prevent crashes */
	if (cob == NULL) 
		return;
	
	/* calculate delta of constraints evaluation */
	invert_m4_m4(imat, cob->startmat);
	mult_m4_m4m4(delta, cob->matrix, imat);
	
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
		}
			break;
		case CONSTRAINT_OBTYPE_BONE:
		{
			/* cob->ob or cob->pchan might not exist */
			if (cob->ob && cob->pchan) {
				/* copy new pose-matrix back to owner */
				mult_m4_m4m4(cob->pchan->pose_mat, cob->ob->imat, cob->matrix);
				
				/* copy inverse of delta back to owner */
				invert_m4_m4(cob->pchan->constinv, delta);
			}
		}
			break;
	}
	
	/* free tempolary struct */
	MEM_freeN(cob);
}

/* -------------- Space-Conversion API -------------- */

/* This function is responsible for the correct transformations/conversions 
 * of a matrix from one space to another for constraint evaluation.
 * For now, this is only implemented for Objects and PoseChannels.
 */
void constraint_mat_convertspace (Object *ob, bPoseChannel *pchan, float mat[][4], short from, short to)
{
	float tempmat[4][4];
	float diff_mat[4][4];
	float imat[4][4];
	
	/* prevent crashes in these unlikely events  */
	if (ob==NULL || mat==NULL) return;
	/* optimise trick - check if need to do anything */
	if (from == to) return;
	
	/* are we dealing with pose-channels or objects */
	if (pchan) {
		/* pose channels */
		switch (from) {
			case CONSTRAINT_SPACE_WORLD: /* ---------- FROM WORLDSPACE ---------- */
			{
				/* world to pose */
				invert_m4_m4(imat, ob->obmat);
				copy_m4_m4(tempmat, mat);
				mult_m4_m4m4(mat, imat, tempmat);
				
				/* use pose-space as stepping stone for other spaces... */
				if (ELEM(to, CONSTRAINT_SPACE_LOCAL, CONSTRAINT_SPACE_PARLOCAL)) {
					/* call self with slightly different values */
					constraint_mat_convertspace(ob, pchan, mat, CONSTRAINT_SPACE_POSE, to);
				}
			}
				break;
			case CONSTRAINT_SPACE_POSE:	/* ---------- FROM POSESPACE ---------- */
			{
				/* pose to world */
				if (to == CONSTRAINT_SPACE_WORLD) {
					copy_m4_m4(tempmat, mat);
					mult_m4_m4m4(mat, ob->obmat, tempmat);
				}
				/* pose to local */
				else if (to == CONSTRAINT_SPACE_LOCAL) {
					if (pchan->bone) {
						if (pchan->parent) {
							float offs_bone[4][4];
								
							/* construct offs_bone the same way it is done in armature.c */
							copy_m4_m3(offs_bone, pchan->bone->bone_mat);
							copy_v3_v3(offs_bone[3], pchan->bone->head);
							offs_bone[3][1]+= pchan->bone->parent->length;
							
							if (pchan->bone->flag & BONE_HINGE) {
								/* pose_mat = par_pose-space_location * chan_mat */
								float tmat[4][4];
								
								/* the rotation of the parent restposition */
								copy_m4_m4(tmat, pchan->bone->parent->arm_mat);
								
								/* the location of actual parent transform */
								copy_v3_v3(tmat[3], offs_bone[3]);
								offs_bone[3][0]= offs_bone[3][1]= offs_bone[3][2]= 0.0f;
								mul_m4_v3(pchan->parent->pose_mat, tmat[3]);
								
								mult_m4_m4m4(diff_mat, tmat, offs_bone);
								invert_m4_m4(imat, diff_mat);
							}
							else {
								/* pose_mat = par_pose_mat * bone_mat * chan_mat */
								mult_m4_m4m4(diff_mat, pchan->parent->pose_mat, offs_bone);
								invert_m4_m4(imat, diff_mat);
							}
						}
						else {
							/* pose_mat = chan_mat * arm_mat */
							invert_m4_m4(imat, pchan->bone->arm_mat);
						}
						
						copy_m4_m4(tempmat, mat);
						mult_m4_m4m4(mat, imat, tempmat);
					}
				}
				/* pose to local with parent */
				else if (to == CONSTRAINT_SPACE_PARLOCAL) {
					if (pchan->bone) {
						invert_m4_m4(imat, pchan->bone->arm_mat);
						copy_m4_m4(tempmat, mat);
						mult_m4_m4m4(mat, imat, tempmat);
					}
				}
			}
				break;
			case CONSTRAINT_SPACE_LOCAL: /* ------------ FROM LOCALSPACE --------- */
			{
				/* local to pose - do inverse procedure that was done for pose to local */
				if (pchan->bone) {
					/* we need the posespace_matrix = local_matrix + (parent_posespace_matrix + restpos) */						
					if (pchan->parent) {
						float offs_bone[4][4];
						
						/* construct offs_bone the same way it is done in armature.c */
						copy_m4_m3(offs_bone, pchan->bone->bone_mat);
						copy_v3_v3(offs_bone[3], pchan->bone->head);
						offs_bone[3][1]+= pchan->bone->parent->length;
						
						if (pchan->bone->flag & BONE_HINGE) {
							/* pose_mat = par_pose-space_location * chan_mat */
							float tmat[4][4];
							
							/* the rotation of the parent restposition */
							copy_m4_m4(tmat, pchan->bone->parent->arm_mat);
							
							/* the location of actual parent transform */
							copy_v3_v3(tmat[3], offs_bone[3]);
							zero_v3(offs_bone[3]);
							mul_m4_v3(pchan->parent->pose_mat, tmat[3]);
							
							mult_m4_m4m4(diff_mat, tmat, offs_bone);
							copy_m4_m4(tempmat, mat);
							mult_m4_m4m4(mat, diff_mat, tempmat);
						}
						else {
							/* pose_mat = par_pose_mat * bone_mat * chan_mat */
							mult_m4_m4m4(diff_mat, pchan->parent->pose_mat, offs_bone);
							copy_m4_m4(tempmat, mat);
							mult_m4_m4m4(mat, diff_mat, tempmat);
						}
					}
					else {
						copy_m4_m4(diff_mat, pchan->bone->arm_mat);
						
						copy_m4_m4(tempmat, mat);
						mult_m4_m4m4(mat, diff_mat, tempmat);
					}
				}
				
				/* use pose-space as stepping stone for other spaces */
				if (ELEM(to, CONSTRAINT_SPACE_WORLD, CONSTRAINT_SPACE_PARLOCAL)) {
					/* call self with slightly different values */
					constraint_mat_convertspace(ob, pchan, mat, CONSTRAINT_SPACE_POSE, to);
				}				
			}
				break;
			case CONSTRAINT_SPACE_PARLOCAL: /* -------------- FROM LOCAL WITH PARENT ---------- */
			{
				/* local + parent to pose */
				if (pchan->bone) {					
					copy_m4_m4(diff_mat, pchan->bone->arm_mat);
					copy_m4_m4(tempmat, mat);
					mult_m4_m4m4(mat, tempmat, diff_mat);
				}
				
				/* use pose-space as stepping stone for other spaces */
				if (ELEM(to, CONSTRAINT_SPACE_WORLD, CONSTRAINT_SPACE_LOCAL)) {
					/* call self with slightly different values */
					constraint_mat_convertspace(ob, pchan, mat, CONSTRAINT_SPACE_POSE, to);
				}
			}
				break;
		}
	}
	else {
		/* objects */
		if (from==CONSTRAINT_SPACE_WORLD && to==CONSTRAINT_SPACE_LOCAL) {
			/* check if object has a parent */
			if (ob->parent) {
				/* 'subtract' parent's effects from owner */
				mult_m4_m4m4(diff_mat, ob->parent->obmat, ob->parentinv);
				invert_m4_m4(imat, diff_mat);
				copy_m4_m4(tempmat, mat);
				mult_m4_m4m4(mat, imat, tempmat);
			}
			else {
				/* Local space in this case will have to be defined as local to the owner's 
				 * transform-property-rotated axes. So subtract this rotation component.
				 */
				object_to_mat4(ob, diff_mat);
				normalize_m4(diff_mat);
				zero_v3(diff_mat[3]);
				
				invert_m4_m4(imat, diff_mat);
				copy_m4_m4(tempmat, mat);
				mult_m4_m4m4(mat, imat, tempmat);
			}
		}
		else if (from==CONSTRAINT_SPACE_LOCAL && to==CONSTRAINT_SPACE_WORLD) {
			/* check that object has a parent - otherwise this won't work */
			if (ob->parent) {
				/* 'add' parent's effect back to owner */
				copy_m4_m4(tempmat, mat);
				mult_m4_m4m4(diff_mat, ob->parent->obmat, ob->parentinv);
				mult_m4_m4m4(mat, diff_mat, tempmat);
			}
			else {
				/* Local space in this case will have to be defined as local to the owner's 
				 * transform-property-rotated axes. So add back this rotation component.
				 */
				object_to_mat4(ob, diff_mat);
				normalize_m4(diff_mat);
				zero_v3(diff_mat[3]);
				
				copy_m4_m4(tempmat, mat);
				mult_m4_m4m4(mat, diff_mat, tempmat);
			}
		}
	}
}

/* ------------ General Target Matrix Tools ---------- */

/* function that sets the given matrix based on given vertex group in mesh */
static void contarget_get_mesh_mat (Object *ob, const char *substring, float mat[][4])
{
	DerivedMesh *dm = NULL;
	Mesh *me= ob->data;
	EditMesh *em = BKE_mesh_get_editmesh(me);
	float vec[3] = {0.0f, 0.0f, 0.0f};
	float normal[3] = {0.0f, 0.0f, 0.0f}, plane[3];
	float imat[3][3], tmat[3][3];
	const int defgroup= defgroup_name_index(ob, substring);
	short freeDM = 0;
	
	/* initialize target matrix using target matrix */
	copy_m4_m4(mat, ob->obmat);
	
	/* get index of vertex group */
	if (defgroup == -1) return;

	/* get DerivedMesh */
	if (em) {
		/* target is in editmode, so get a special derived mesh */
		dm = CDDM_from_editmesh(em, ob->data);
		freeDM= 1;
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
		int i, count = 0;
		float co[3], nor[3];
		
		/* check that dvert is a valid pointers (just in case) */
		if (dvert) {
			MDeformVert *dv= dvert;
			/* get the average of all verts with that are in the vertex-group */
			for (i = 0; i < numVerts; i++, dv++) {
				MDeformWeight *dw= defvert_find_index(dv, defgroup);
				if (dw && dw->weight != 0.0f) {
					dm->getVertCo(dm, i, co);
					dm->getVertNo(dm, i, nor);
					add_v3_v3(vec, co);
					add_v3_v3(normal, nor);
					count++;
					
				}
			}

			/* calculate averages of normal and coordinates */
			if (count > 0) {
				mul_v3_fl(vec, 1.0f / count);
				mul_v3_fl(normal, 1.0f / count);
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
			
			copy_v3_v3(tmat[2], normal);
			cross_v3_v3v3(tmat[0], normal, plane);
			cross_v3_v3v3(tmat[1], tmat[2], tmat[0]);
			
			copy_m4_m3(mat, tmat);
			normalize_m4(mat);
			
			
			/* apply the average coordinate as the new location */
			mul_v3_m4v3(mat[3], ob->obmat, vec);
		}
	}
	
	/* free temporary DerivedMesh created (in EditMode case) */
	if (dm && freeDM)
		dm->release(dm);
	if (em)
		BKE_mesh_end_editmesh(me, em);
}

/* function that sets the given matrix based on given vertex group in lattice */
static void contarget_get_lattice_mat (Object *ob, const char *substring, float mat[][4])
{
	Lattice *lt= (Lattice *)ob->data;
	
	DispList *dl = find_displist(&ob->disp, DL_VERTS);
	float *co = dl?dl->verts:NULL;
	BPoint *bp = lt->def;
	
	MDeformVert *dv = lt->dvert;
	int tot_verts= lt->pntsu*lt->pntsv*lt->pntsw;
	float vec[3]= {0.0f, 0.0f, 0.0f}, tvec[3];
	int grouped=0;
	int i, n;
	const int defgroup= defgroup_name_index(ob, substring);
	
	/* initialize target matrix using target matrix */
	copy_m4_m4(mat, ob->obmat);

	/* get index of vertex group */
	if (defgroup == -1) return;
	if (dv == NULL) return;
	
	/* 1. Loop through control-points checking if in nominated vertex-group.
	 * 2. If it is, add it to vec to find the average point.
	 */
	for (i=0; i < tot_verts; i++, dv++) {
		for (n= 0; n < dv->totweight; n++) {
			MDeformWeight *dw= defvert_find_index(dv, defgroup);
			if (dw && dw->weight > 0.0f) {
				/* copy coordinates of point to temporary vector, then add to find average */
				memcpy(tvec, co ? co : bp->vec, 3 * sizeof(float));

				add_v3_v3(vec, tvec);
				grouped++;
			}
		}
		
		/* advance pointer to coordinate data */
		if (co) co += 3;
		else    bp++;
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
static void constraint_target_to_mat4 (Object *ob, const char *substring, float mat[][4], short from, short to, float headtail)
{
	/*	Case OBJECT */
	if (!strlen(substring)) {
		copy_m4_m4(mat, ob->obmat);
		constraint_mat_convertspace(ob, NULL, mat, from, to);
	}
	/* 	Case VERTEXGROUP */
	/* Current method just takes the average location of all the points in the
	 * VertexGroup, and uses that as the location value of the targets. Where 
	 * possible, the orientation will also be calculated, by calculating an
	 * 'average' vertex normal, and deriving the rotaation from that.
	 *
	 * NOTE: EditMode is not currently supported, and will most likely remain that
	 *		way as constraints can only really affect things on object/bone level.
	 */
	else if (ob->type == OB_MESH) {
		contarget_get_mesh_mat(ob, substring, mat);
		constraint_mat_convertspace(ob, NULL, mat, from, to);
	}
	else if (ob->type == OB_LATTICE) {
		contarget_get_lattice_mat(ob, substring, mat);
		constraint_mat_convertspace(ob, NULL, mat, from, to);
	}
	/*	Case BONE */
	else {
		bPoseChannel *pchan;
		
		pchan = get_pose_channel(ob->pose, substring);
		if (pchan) {
			/* Multiply the PoseSpace accumulation/final matrix for this
			 * PoseChannel by the Armature Object's Matrix to get a worldspace
			 * matrix.
			 */
			if (headtail < 0.000001f) {
				/* skip length interpolation if set to head */
				mult_m4_m4m4(mat, ob->obmat, pchan->pose_mat);
			}
			else {
				float tempmat[4][4], loc[3];
				
				/* interpolate along length of bone */
				interp_v3_v3v3(loc, pchan->pose_head, pchan->pose_tail, headtail);	
				
				/* use interpolated distance for subtarget */
				copy_m4_m4(tempmat, pchan->pose_mat);	
				copy_v3_v3(tempmat[3], loc);
				
				mult_m4_m4m4(mat, ob->obmat, tempmat);
			}
		} 
		else
			copy_m4_m4(mat, ob->obmat);
			
		/* convert matrix space as required */
		constraint_mat_convertspace(ob, pchan, mat, from, to);
	}
}

/* ************************* Specific Constraints ***************************** */
/* Each constraint defines a set of functions, which will be called at the appropriate
 * times. In addition to this, each constraint should have a type-info struct, where
 * its functions are attached for use. 
 */
 
/* Template for type-info data:
 *	- make a copy of this when creating new constraints, and just change the functions
 *	  pointed to as necessary
 *	- although the naming of functions doesn't matter, it would help for code
 *	  readability, to follow the same naming convention as is presented here
 * 	- any functions that a constraint doesn't need to define, don't define
 *	  for such cases, just use NULL 
 *	- these should be defined after all the functions have been defined, so that
 * 	  forward-definitions/prototypes don't need to be used!
 *	- keep this copy #if-def'd so that future constraints can get based off this
 */
#if 0
static bConstraintTypeInfo CTI_CONSTRNAME = {
	CONSTRAINT_TYPE_CONSTRNAME, /* type */
	sizeof(bConstrNameConstraint), /* size */
	"ConstrName", /* name */
	"bConstrNameConstraint", /* struct name */
	constrname_free, /* free data */
	constrname_relink, /* relink data */
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
static void default_get_tarmat (bConstraint *con, bConstraintOb *UNUSED(cob), bConstraintTarget *ct, float UNUSED(ctime))
{
	if (VALID_CONS_TARGET(ct))
		constraint_target_to_mat4(ct->tar, ct->subtarget, ct->matrix, CONSTRAINT_SPACE_WORLD, ct->space, con->headtail);
	else if (ct)
		unit_m4(ct->matrix);
}

/* This following macro should be used for all standard single-target *_get_tars functions 
 * to save typing and reduce maintainance woes.
 * (Hopefully all compilers will be happy with the lines with just a space on them. Those are
 *  really just to help this code easier to read)
 */
// TODO: cope with getting rotation order...
#define SINGLETARGET_GET_TARS(con, datatar, datasubtarget, ct, list) \
	{ \
		ct= MEM_callocN(sizeof(bConstraintTarget), "tempConstraintTarget"); \
		 \
		ct->tar= datatar; \
		BLI_strncpy(ct->subtarget, datasubtarget, sizeof(ct->subtarget)); \
		ct->space= con->tarspace; \
		ct->flag= CONSTRAINT_TAR_TEMP; \
		 \
		if (ct->tar) { \
			if ((ct->tar->type==OB_ARMATURE) && (ct->subtarget[0])) { \
				bPoseChannel *pchan= get_pose_channel(ct->tar->pose, ct->subtarget); \
				ct->type = CONSTRAINT_OBTYPE_BONE; \
				ct->rotOrder= (pchan) ? (pchan->rotmode) : EULER_ORDER_DEFAULT; \
			}\
			else if (OB_TYPE_SUPPORT_VGROUP(ct->tar->type) && (ct->subtarget[0])) { \
				ct->type = CONSTRAINT_OBTYPE_VERT; \
				ct->rotOrder = EULER_ORDER_DEFAULT; \
			} \
			else {\
				ct->type = CONSTRAINT_OBTYPE_OBJECT; \
				ct->rotOrder= ct->tar->rotmode; \
			} \
		} \
		 \
		BLI_addtail(list, ct); \
	}
	
/* This following macro should be used for all standard single-target *_get_tars functions 
 * to save typing and reduce maintainance woes. It does not do the subtarget related operations
 * (Hopefully all compilers will be happy with the lines with just a space on them. Those are
 *  really just to help this code easier to read)
 */
// TODO: cope with getting rotation order...
#define SINGLETARGETNS_GET_TARS(con, datatar, ct, list) \
	{ \
		ct= MEM_callocN(sizeof(bConstraintTarget), "tempConstraintTarget"); \
		 \
		ct->tar= datatar; \
		ct->space= con->tarspace; \
		ct->flag= CONSTRAINT_TAR_TEMP; \
		 \
		if (ct->tar) ct->type = CONSTRAINT_OBTYPE_OBJECT; \
		 \
		BLI_addtail(list, ct); \
	}

/* This following macro should be used for all standard single-target *_flush_tars functions
 * to save typing and reduce maintainance woes.
 * Note: the pointer to ct will be changed to point to the next in the list (as it gets removed)
 * (Hopefully all compilers will be happy with the lines with just a space on them. Those are
 *  really just to help this code easier to read)
 */
#define SINGLETARGET_FLUSH_TARS(con, datatar, datasubtarget, ct, list, nocopy) \
	{ \
		if (ct) { \
			bConstraintTarget *ctn = ct->next; \
			if (nocopy == 0) { \
				datatar= ct->tar; \
				BLI_strncpy(datasubtarget, ct->subtarget, sizeof(datasubtarget)); \
				con->tarspace= (char)ct->space; \
			} \
			 \
			BLI_freelinkN(list, ct); \
			ct= ctn; \
		} \
	}
	
/* This following macro should be used for all standard single-target *_flush_tars functions
 * to save typing and reduce maintainance woes. It does not do the subtarget related operations.
 * Note: the pointer to ct will be changed to point to the next in the list (as it gets removed)
 * (Hopefully all compilers will be happy with the lines with just a space on them. Those are
 *  really just to help this code easier to read)
 */
#define SINGLETARGETNS_FLUSH_TARS(con, datatar, ct, list, nocopy) \
	{ \
		if (ct) { \
			bConstraintTarget *ctn = ct->next; \
			if (nocopy == 0) { \
				datatar= ct->tar; \
				con->tarspace= (char)ct->space; \
			} \
			 \
			BLI_freelinkN(list, ct); \
			ct= ctn; \
		} \
	}
 
/* --------- ChildOf Constraint ------------ */

static void childof_new_data (void *cdata)
{
	bChildOfConstraint *data= (bChildOfConstraint *)cdata;
	
	data->flag = (CHILDOF_LOCX | CHILDOF_LOCY | CHILDOF_LOCZ |
					CHILDOF_ROTX |CHILDOF_ROTY | CHILDOF_ROTZ |
					CHILDOF_SIZEX | CHILDOF_SIZEY | CHILDOF_SIZEZ);
	unit_m4(data->invmat);
}

static void childof_id_looper (bConstraint *con, ConstraintIDFunc func, void *userdata)
{
	bChildOfConstraint *data= con->data;
	
	/* target only */
	func(con, (ID**)&data->tar, userdata);
}

static int childof_get_tars (bConstraint *con, ListBase *list)
{
	if (con && list) {
		bChildOfConstraint *data= con->data;
		bConstraintTarget *ct;
		
		/* standard target-getting macro for single-target constraints */
		SINGLETARGET_GET_TARS(con, data->tar, data->subtarget, ct, list)
		
		return 1;
	}
	
	return 0;
}

static void childof_flush_tars (bConstraint *con, ListBase *list, short nocopy)
{
	if (con && list) {
		bChildOfConstraint *data= con->data;
		bConstraintTarget *ct= list->first;
		
		/* the following macro is used for all standard single-target constraints */
		SINGLETARGET_FLUSH_TARS(con, data->tar, data->subtarget, ct, list, nocopy)
	}
}

static void childof_evaluate (bConstraint *con, bConstraintOb *cob, ListBase *targets)
{
	bChildOfConstraint *data= con->data;
	bConstraintTarget *ct= targets->first;
	
	/* only evaluate if there is a target */
	if (VALID_CONS_TARGET(ct)) {
		float parmat[4][4];
		
		/* simple matrix parenting */
		if(data->flag == CHILDOF_ALL) {
			
			/* multiply target (parent matrix) by offset (parent inverse) to get 
			 * the effect of the parent that will be exherted on the owner
			 */
			mult_m4_m4m4(parmat, ct->matrix, data->invmat);
			
			/* now multiply the parent matrix by the owner matrix to get the 
			 * the effect of this constraint (i.e.  owner is 'parented' to parent)
			 */
			mult_m4_m4m4(cob->matrix, parmat, cob->matrix);
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
			if (!(data->flag & CHILDOF_LOCX)) loc[0]= loco[0]= 0.0f;
			if (!(data->flag & CHILDOF_LOCY)) loc[1]= loco[1]= 0.0f;
			if (!(data->flag & CHILDOF_LOCZ)) loc[2]= loco[2]= 0.0f;
			if (!(data->flag & CHILDOF_ROTX)) eul[0]= eulo[0]= 0.0f;
			if (!(data->flag & CHILDOF_ROTY)) eul[1]= eulo[1]= 0.0f;
			if (!(data->flag & CHILDOF_ROTZ)) eul[2]= eulo[2]= 0.0f;
			if (!(data->flag & CHILDOF_SIZEX)) size[0]= sizo[0]= 1.0f;
			if (!(data->flag & CHILDOF_SIZEY)) size[1]= sizo[1]= 1.0f;
			if (!(data->flag & CHILDOF_SIZEZ)) size[2]= sizo[2]= 1.0f;
			
			/* make new target mat and offset mat */
			loc_eulO_size_to_mat4(ct->matrix, loc, eul, size, ct->rotOrder);
			loc_eulO_size_to_mat4(invmat, loco, eulo, sizo, cob->rotOrder);
			
			/* multiply target (parent matrix) by offset (parent inverse) to get 
			 * the effect of the parent that will be exherted on the owner
			 */
			mult_m4_m4m4(parmat, ct->matrix, invmat);
			
			/* now multiply the parent matrix by the owner matrix to get the 
			 * the effect of this constraint (i.e.  owner is 'parented' to parent)
			 */
			copy_m4_m4(tempmat, cob->matrix);
			mult_m4_m4m4(cob->matrix, parmat, tempmat);

			/* without this, changes to scale and rotation can change location
			 * of a parentless bone or a disconnected bone. Even though its set
			 * to zero above. */
			if (!(data->flag & CHILDOF_LOCX)) cob->matrix[3][0]= tempmat[3][0];
			if (!(data->flag & CHILDOF_LOCY)) cob->matrix[3][1]= tempmat[3][1];
			if (!(data->flag & CHILDOF_LOCZ)) cob->matrix[3][2]= tempmat[3][2];	
		}
	}
}

/* XXX note, con->flag should be CONSTRAINT_SPACEONCE for bone-childof, patched in readfile.c */
static bConstraintTypeInfo CTI_CHILDOF = {
	CONSTRAINT_TYPE_CHILDOF, /* type */
	sizeof(bChildOfConstraint), /* size */
	"ChildOf", /* name */
	"bChildOfConstraint", /* struct name */
	NULL, /* free data */
	NULL, /* relink data */
	childof_id_looper, /* id looper */
	NULL, /* copy data */
	childof_new_data, /* new data */
	childof_get_tars, /* get constraint targets */
	childof_flush_tars, /* flush constraint targets */
	default_get_tarmat, /* get a target matrix */
	childof_evaluate /* evaluate */
};

/* -------- TrackTo Constraint ------- */

static void trackto_new_data (void *cdata)
{
	bTrackToConstraint *data= (bTrackToConstraint *)cdata;
	
	data->reserved1 = TRACK_Y;
	data->reserved2 = UP_Z;
}	

static void trackto_id_looper (bConstraint *con, ConstraintIDFunc func, void *userdata)
{
	bTrackToConstraint *data= con->data;
	
	/* target only */
	func(con, (ID**)&data->tar, userdata);
}

static int trackto_get_tars (bConstraint *con, ListBase *list)
{
	if (con && list) {
		bTrackToConstraint *data= con->data;
		bConstraintTarget *ct;
		
		/* standard target-getting macro for single-target constraints */
		SINGLETARGET_GET_TARS(con, data->tar, data->subtarget, ct, list)
		
		return 1;
	}
	
	return 0;
}

static void trackto_flush_tars (bConstraint *con, ListBase *list, short nocopy)
{
	if (con && list) {
		bTrackToConstraint *data= con->data;
		bConstraintTarget *ct= list->first;
		
		/* the following macro is used for all standard single-target constraints */
		SINGLETARGET_FLUSH_TARS(con, data->tar, data->subtarget, ct, list, nocopy)
	}
}


static int basis_cross (int n, int m)
{
	switch (n-m) {
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

static void vectomat (float *vec, float *target_up, short axis, short upflag, short flags, float m[][3])
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


static void trackto_evaluate (bConstraint *con, bConstraintOb *cob, ListBase *targets)
{
	bTrackToConstraint *data= con->data;
	bConstraintTarget *ct= targets->first;
	
	if (VALID_CONS_TARGET(ct)) {
		float size[3], vec[3];
		float totmat[3][3];
		float tmat[4][4];
		
		/* Get size property, since ob->size is only the object's own relative size, not its global one */
		mat4_to_size(size, cob->matrix);
		
		/* Clear the object's rotation */ 	
		cob->matrix[0][0]=size[0];
		cob->matrix[0][1]=0;
		cob->matrix[0][2]=0;
		cob->matrix[1][0]=0;
		cob->matrix[1][1]=size[1];
		cob->matrix[1][2]=0;
		cob->matrix[2][0]=0;
		cob->matrix[2][1]=0;
		cob->matrix[2][2]=size[2];
		
		/* targetmat[2] instead of ownermat[2] is passed to vectomat
		 * for backwards compatibility it seems... (Aligorith)
		 */
		sub_v3_v3v3(vec, cob->matrix[3], ct->matrix[3]);
		vectomat(vec, ct->matrix[2], 
				(short)data->reserved1, (short)data->reserved2, 
				data->flags, totmat);
		
		copy_m4_m4(tmat, cob->matrix);
		mul_m4_m3m4(cob->matrix, totmat, tmat);
	}
}

static bConstraintTypeInfo CTI_TRACKTO = {
	CONSTRAINT_TYPE_TRACKTO, /* type */
	sizeof(bTrackToConstraint), /* size */
	"TrackTo", /* name */
	"bTrackToConstraint", /* struct name */
	NULL, /* free data */
	NULL, /* relink data */
	trackto_id_looper, /* id looper */
	NULL, /* copy data */
	trackto_new_data, /* new data */
	trackto_get_tars, /* get constraint targets */
	trackto_flush_tars, /* flush constraint targets */
	default_get_tarmat, /* get target matrix */
	trackto_evaluate /* evaluate */
};

/* --------- Inverse-Kinemetics --------- */

static void kinematic_new_data (void *cdata)
{
	bKinematicConstraint *data= (bKinematicConstraint *)cdata;
	
	data->weight= 1.0f;
	data->orientweight= 1.0f;
	data->iterations = 500;
	data->dist= 1.0f;
	data->flag= CONSTRAINT_IK_TIP|CONSTRAINT_IK_STRETCH|CONSTRAINT_IK_POS;
}

static void kinematic_id_looper (bConstraint *con, ConstraintIDFunc func, void *userdata)
{
	bKinematicConstraint *data= con->data;
	
	/* chain target */
	func(con, (ID**)&data->tar, userdata);
	
	/* poletarget */
	func(con, (ID**)&data->poletar, userdata);
}

static int kinematic_get_tars (bConstraint *con, ListBase *list)
{
	if (con && list) {
		bKinematicConstraint *data= con->data;
		bConstraintTarget *ct;
		
		/* standard target-getting macro for single-target constraints is used twice here */
		SINGLETARGET_GET_TARS(con, data->tar, data->subtarget, ct, list)
		SINGLETARGET_GET_TARS(con, data->poletar, data->polesubtarget, ct, list)
		
		return 2;
	}
	
	return 0;
}

static void kinematic_flush_tars (bConstraint *con, ListBase *list, short nocopy)
{
	if (con && list) {
		bKinematicConstraint *data= con->data;
		bConstraintTarget *ct= list->first;
		
		/* the following macro is used for all standard single-target constraints */
		SINGLETARGET_FLUSH_TARS(con, data->tar, data->subtarget, ct, list, nocopy)
		SINGLETARGET_FLUSH_TARS(con, data->poletar, data->polesubtarget, ct, list, nocopy)
	}
}

static void kinematic_get_tarmat (bConstraint *con, bConstraintOb *cob, bConstraintTarget *ct, float UNUSED(ctime))
{
	bKinematicConstraint *data= con->data;
	
	if (VALID_CONS_TARGET(ct)) 
		constraint_target_to_mat4(ct->tar, ct->subtarget, ct->matrix, CONSTRAINT_SPACE_WORLD, ct->space, con->headtail);
	else if (ct) {
		if (data->flag & CONSTRAINT_IK_AUTO) {
			Object *ob= cob->ob;
			
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
	NULL, /* relink data */
	kinematic_id_looper, /* id looper */
	NULL, /* copy data */
	kinematic_new_data, /* new data */
	kinematic_get_tars, /* get constraint targets */
	kinematic_flush_tars, /* flush constraint targets */
	kinematic_get_tarmat, /* get target matrix */
	NULL /* evaluate - solved as separate loop */
};

/* -------- Follow-Path Constraint ---------- */

static void followpath_new_data (void *cdata)
{
	bFollowPathConstraint *data= (bFollowPathConstraint *)cdata;
	
	data->trackflag = TRACK_Y;
	data->upflag = UP_Z;
	data->offset = 0;
	data->followflag = 0;
}

static void followpath_id_looper (bConstraint *con, ConstraintIDFunc func, void *userdata)
{
	bFollowPathConstraint *data= con->data;
	
	/* target only */
	func(con, (ID**)&data->tar, userdata);
}

static int followpath_get_tars (bConstraint *con, ListBase *list)
{
	if (con && list) {
		bFollowPathConstraint *data= con->data;
		bConstraintTarget *ct;
		
		/* standard target-getting macro for single-target constraints without subtargets */
		SINGLETARGETNS_GET_TARS(con, data->tar, ct, list)
		
		return 1;
	}
	
	return 0;
}

static void followpath_flush_tars (bConstraint *con, ListBase *list, short nocopy)
{
	if (con && list) {
		bFollowPathConstraint *data= con->data;
		bConstraintTarget *ct= list->first;
		
		/* the following macro is used for all standard single-target constraints */
		SINGLETARGETNS_FLUSH_TARS(con, data->tar, ct, list, nocopy)
	}
}

static void followpath_get_tarmat (bConstraint *con, bConstraintOb *cob, bConstraintTarget *ct, float UNUSED(ctime))
{
	bFollowPathConstraint *data= con->data;
	
	if (VALID_CONS_TARGET(ct)) {
		Curve *cu= ct->tar->data;
		float vec[4], dir[3], radius;
		float totmat[4][4]= MAT4_UNITY;
		float curvetime;

		unit_m4(ct->matrix);

		/* note: when creating constraints that follow path, the curve gets the CU_PATH set now,
		 *		currently for paths to work it needs to go through the bevlist/displist system (ton) 
		 */
		
		/* only happens on reload file, but violates depsgraph still... fix! */
		if (cu->path==NULL || cu->path->data==NULL)
			makeDispListCurveTypes(cob->scene, ct->tar, 0);
		
		if (cu->path && cu->path->data) {
			float quat[4];
			if ((data->followflag & FOLLOWPATH_STATIC) == 0) {
				/* animated position along curve depending on time */
				Nurb *nu = cu->nurb.first;
				curvetime= cu->ctime - data->offset;
				
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
						curvetime -= floor(curvetime);
					}
				}
				else {
					/* The curve is not cyclic, so clamp to the begin/end points. */
					CLAMP(curvetime, 0.0f, 1.0f);
				}
			}
			else {
				/* fixed position along curve */
				curvetime= data->offset_fac;
			}
			
			if ( where_on_path(ct->tar, curvetime, vec, dir, (data->followflag & FOLLOWPATH_FOLLOW) ? quat : NULL, &radius, NULL) ) { /* quat_pt is quat or NULL*/
				if (data->followflag & FOLLOWPATH_FOLLOW) {
#if 0
					float x1, q[4];
					vec_to_quat(quat, dir, (short)data->trackflag, (short)data->upflag);
					
					normalize_v3(dir);
					q[0]= (float)cos(0.5*vec[3]);
					x1= (float)sin(0.5*vec[3]);
					q[1]= -x1*dir[0];
					q[2]= -x1*dir[1];
					q[3]= -x1*dir[2];
					mul_qt_qtqt(quat, q, quat);
#else
					quat_apply_track(quat, data->trackflag, data->upflag);
#endif

					quat_to_mat4(totmat, quat);
				}

				if (data->followflag & FOLLOWPATH_RADIUS) {
					float tmat[4][4], rmat[4][4];
					scale_m4_fl(tmat, radius);
					mult_m4_m4m4(rmat, tmat, totmat);
					copy_m4_m4(totmat, rmat);
				}
				
				copy_v3_v3(totmat[3], vec);
				
				mul_serie_m4(ct->matrix, ct->tar->obmat, totmat, NULL, NULL, NULL, NULL, NULL, NULL);
			}
		}
	}
	else if (ct)
		unit_m4(ct->matrix);
}

static void followpath_evaluate (bConstraint *con, bConstraintOb *cob, ListBase *targets)
{
	bConstraintTarget *ct= targets->first;
	
	/* only evaluate if there is a target */
	if (VALID_CONS_TARGET(ct)) {
		float obmat[4][4];
		float size[3];
		bFollowPathConstraint *data= con->data;
		
		/* get Object transform (loc/rot/size) to determine transformation from path */
		// TODO: this used to be local at one point, but is probably more useful as-is
		copy_m4_m4(obmat, cob->matrix);
		
		/* get scaling of object before applying constraint */
		mat4_to_size(size, cob->matrix);
		
		/* apply targetmat - containing location on path, and rotation */
		mul_serie_m4(cob->matrix, ct->matrix, obmat, NULL, NULL, NULL, NULL, NULL, NULL);
		
		/* un-apply scaling caused by path */
		if ((data->followflag & FOLLOWPATH_RADIUS)==0) { /* XXX - assume that scale correction means that radius will have some scale error in it - Campbell */
			float obsize[3];
			
			mat4_to_size( obsize,cob->matrix);
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
	NULL, /* relink data */
	followpath_id_looper, /* id looper */
	NULL, /* copy data */
	followpath_new_data, /* new data */
	followpath_get_tars, /* get constraint targets */
	followpath_flush_tars, /* flush constraint targets */
	followpath_get_tarmat, /* get target matrix */
	followpath_evaluate /* evaluate */
};

/* --------- Limit Location --------- */


static void loclimit_evaluate (bConstraint *con, bConstraintOb *cob, ListBase *UNUSED(targets))
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
	NULL, /* relink data */
	NULL, /* id looper */
	NULL, /* copy data */
	NULL, /* new data */
	NULL, /* get constraint targets */
	NULL, /* flush constraint targets */
	NULL, /* get target matrix */
	loclimit_evaluate /* evaluate */
};

/* -------- Limit Rotation --------- */

static void rotlimit_evaluate (bConstraint *con, bConstraintOb *cob, ListBase *UNUSED(targets))
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
	NULL, /* relink data */
	NULL, /* id looper */
	NULL, /* copy data */
	NULL, /* new data */
	NULL, /* get constraint targets */
	NULL, /* flush constraint targets */
	NULL, /* get target matrix */
	rotlimit_evaluate /* evaluate */
};

/* --------- Limit Scaling --------- */


static void sizelimit_evaluate (bConstraint *con, bConstraintOb *cob, ListBase *UNUSED(targets))
{
	bSizeLimitConstraint *data = con->data;
	float obsize[3], size[3];
	
	mat4_to_size( size,cob->matrix);
	mat4_to_size( obsize,cob->matrix);
	
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
		mul_v3_fl(cob->matrix[0], size[0]/obsize[0]);
	if (obsize[1]) 
		mul_v3_fl(cob->matrix[1], size[1]/obsize[1]);
	if (obsize[2]) 
		mul_v3_fl(cob->matrix[2], size[2]/obsize[2]);
}

static bConstraintTypeInfo CTI_SIZELIMIT = {
	CONSTRAINT_TYPE_SIZELIMIT, /* type */
	sizeof(bSizeLimitConstraint), /* size */
	"Limit Scaling", /* name */
	"bSizeLimitConstraint", /* struct name */
	NULL, /* free data */
	NULL, /* relink data */
	NULL, /* id looper */
	NULL, /* copy data */
	NULL, /* new data */
	NULL, /* get constraint targets */
	NULL, /* flush constraint targets */
	NULL, /* get target matrix */
	sizelimit_evaluate /* evaluate */
};

/* ----------- Copy Location ------------- */

static void loclike_new_data (void *cdata)
{
	bLocateLikeConstraint *data= (bLocateLikeConstraint *)cdata;
	
	data->flag = LOCLIKE_X|LOCLIKE_Y|LOCLIKE_Z;
}

static void loclike_id_looper (bConstraint *con, ConstraintIDFunc func, void *userdata)
{
	bLocateLikeConstraint *data= con->data;
	
	/* target only */
	func(con, (ID**)&data->tar, userdata);
}

static int loclike_get_tars (bConstraint *con, ListBase *list)
{
	if (con && list) {
		bLocateLikeConstraint *data= con->data;
		bConstraintTarget *ct;
		
		/* standard target-getting macro for single-target constraints */
		SINGLETARGET_GET_TARS(con, data->tar, data->subtarget, ct, list)
		
		return 1;
	}
	
	return 0;
}

static void loclike_flush_tars (bConstraint *con, ListBase *list, short nocopy)
{
	if (con && list) {
		bLocateLikeConstraint *data= con->data;
		bConstraintTarget *ct= list->first;
		
		/* the following macro is used for all standard single-target constraints */
		SINGLETARGET_FLUSH_TARS(con, data->tar, data->subtarget, ct, list, nocopy)
	}
}

static void loclike_evaluate (bConstraint *con, bConstraintOb *cob, ListBase *targets)
{
	bLocateLikeConstraint *data= con->data;
	bConstraintTarget *ct= targets->first;
	
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
	NULL, /* relink data */
	loclike_id_looper, /* id looper */
	NULL, /* copy data */
	loclike_new_data, /* new data */
	loclike_get_tars, /* get constraint targets */
	loclike_flush_tars, /* flush constraint targets */
	default_get_tarmat, /* get target matrix */
	loclike_evaluate /* evaluate */
};

/* ----------- Copy Rotation ------------- */

static void rotlike_new_data (void *cdata)
{
	bRotateLikeConstraint *data= (bRotateLikeConstraint *)cdata;
	
	data->flag = ROTLIKE_X|ROTLIKE_Y|ROTLIKE_Z;
}

static void rotlike_id_looper (bConstraint *con, ConstraintIDFunc func, void *userdata)
{
	bChildOfConstraint *data= con->data;
	
	/* target only */
	func(con, (ID**)&data->tar, userdata);
}

static int rotlike_get_tars (bConstraint *con, ListBase *list)
{
	if (con && list) {
		bRotateLikeConstraint *data= con->data;
		bConstraintTarget *ct;
		
		/* standard target-getting macro for single-target constraints */
		SINGLETARGET_GET_TARS(con, data->tar, data->subtarget, ct, list)
		
		return 1;
	}
	
	return 0;
}

static void rotlike_flush_tars (bConstraint *con, ListBase *list, short nocopy)
{
	if (con && list) {
		bRotateLikeConstraint *data= con->data;
		bConstraintTarget *ct= list->first;
		
		/* the following macro is used for all standard single-target constraints */
		SINGLETARGET_FLUSH_TARS(con, data->tar, data->subtarget, ct, list, nocopy)
	}
}

static void rotlike_evaluate (bConstraint *con, bConstraintOb *cob, ListBase *targets)
{
	bRotateLikeConstraint *data= con->data;
	bConstraintTarget *ct= targets->first;
	
	if (VALID_CONS_TARGET(ct)) {
		float	loc[3];
		float	eul[3], obeul[3];
		float	size[3];
		
		copy_v3_v3(loc, cob->matrix[3]);
		mat4_to_size(size, cob->matrix);
		
		/* to allow compatible rotations, must get both rotations in the order of the owner... */
		mat4_to_eulO(obeul, cob->rotOrder, cob->matrix);
		/* we must get compatible eulers from the beginning because some of them can be modified below (see bug #21875) */
		mat4_to_compatible_eulO(eul, obeul, cob->rotOrder, ct->matrix);
		
		if ((data->flag & ROTLIKE_X)==0)
			eul[0] = obeul[0];
		else {
			if (data->flag & ROTLIKE_OFFSET)
				rotate_eulO(eul, cob->rotOrder, 'X', obeul[0]);
			
			if (data->flag & ROTLIKE_X_INVERT)
				eul[0] *= -1;
		}
		
		if ((data->flag & ROTLIKE_Y)==0)
			eul[1] = obeul[1];
		else {
			if (data->flag & ROTLIKE_OFFSET)
				rotate_eulO(eul, cob->rotOrder, 'Y', obeul[1]);
			
			if (data->flag & ROTLIKE_Y_INVERT)
				eul[1] *= -1;
		}
		
		if ((data->flag & ROTLIKE_Z)==0)
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
	NULL, /* relink data */
	rotlike_id_looper, /* id looper */
	NULL, /* copy data */
	rotlike_new_data, /* new data */
	rotlike_get_tars, /* get constraint targets */
	rotlike_flush_tars, /* flush constraint targets */
	default_get_tarmat, /* get target matrix */
	rotlike_evaluate /* evaluate */
};

/* ---------- Copy Scaling ---------- */

static void sizelike_new_data (void *cdata)
{
	bSizeLikeConstraint *data= (bSizeLikeConstraint *)cdata;
	
	data->flag = SIZELIKE_X|SIZELIKE_Y|SIZELIKE_Z;
}

static void sizelike_id_looper (bConstraint *con, ConstraintIDFunc func, void *userdata)
{
	bSizeLikeConstraint *data= con->data;
	
	/* target only */
	func(con, (ID**)&data->tar, userdata);
}

static int sizelike_get_tars (bConstraint *con, ListBase *list)
{
	if (con && list) {
		bSizeLikeConstraint *data= con->data;
		bConstraintTarget *ct;
		
		/* standard target-getting macro for single-target constraints */
		SINGLETARGET_GET_TARS(con, data->tar, data->subtarget, ct, list)
		
		return 1;
	}
	
	return 0;
}

static void sizelike_flush_tars (bConstraint *con, ListBase *list, short nocopy)
{
	if (con && list) {
		bSizeLikeConstraint *data= con->data;
		bConstraintTarget *ct= list->first;
		
		/* the following macro is used for all standard single-target constraints */
		SINGLETARGET_FLUSH_TARS(con, data->tar, data->subtarget, ct, list, nocopy)
	}
}

static void sizelike_evaluate (bConstraint *con, bConstraintOb *cob, ListBase *targets)
{
	bSizeLikeConstraint *data= con->data;
	bConstraintTarget *ct= targets->first;
	
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
	NULL, /* relink data */
	sizelike_id_looper, /* id looper */
	NULL, /* copy data */
	sizelike_new_data, /* new data */
	sizelike_get_tars, /* get constraint targets */
	sizelike_flush_tars, /* flush constraint targets */
	default_get_tarmat, /* get target matrix */
	sizelike_evaluate /* evaluate */
};

/* ----------- Copy Transforms ------------- */

static void translike_id_looper (bConstraint *con, ConstraintIDFunc func, void *userdata)
{
	bTransLikeConstraint *data= con->data;
	
	/* target only */
	func(con, (ID**)&data->tar, userdata);
}

static int translike_get_tars (bConstraint *con, ListBase *list)
{
	if (con && list) {
		bTransLikeConstraint *data= con->data;
		bConstraintTarget *ct;
		
		/* standard target-getting macro for single-target constraints */
		SINGLETARGET_GET_TARS(con, data->tar, data->subtarget, ct, list)
		
		return 1;
	}
	
	return 0;
}

static void translike_flush_tars (bConstraint *con, ListBase *list, short nocopy)
{
	if (con && list) {
		bTransLikeConstraint *data= con->data;
		bConstraintTarget *ct= list->first;
		
		/* the following macro is used for all standard single-target constraints */
		SINGLETARGET_FLUSH_TARS(con, data->tar, data->subtarget, ct, list, nocopy)
	}
}

static void translike_evaluate (bConstraint *UNUSED(con), bConstraintOb *cob, ListBase *targets)
{
	bConstraintTarget *ct= targets->first;
	
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
	NULL, /* relink data */
	translike_id_looper, /* id looper */
	NULL, /* copy data */
	NULL, /* new data */
	translike_get_tars, /* get constraint targets */
	translike_flush_tars, /* flush constraint targets */
	default_get_tarmat, /* get target matrix */
	translike_evaluate /* evaluate */
};

/* ---------- Maintain Volume ---------- */

static void samevolume_new_data (void *cdata)
{
	bSameVolumeConstraint *data= (bSameVolumeConstraint *)cdata;

	data->flag = SAMEVOL_Y;
	data->volume = 1.0f;
}

static void samevolume_evaluate (bConstraint *con, bConstraintOb *cob, ListBase *UNUSED(targets))
{
	bSameVolumeConstraint *data= con->data;

	float volume = data->volume;
	float fac = 1.0f;
	float obsize[3];

	mat4_to_size(obsize, cob->matrix);
	
	/* calculate normalising scale factor for non-essential values */
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
	NULL, /* relink data */
	NULL, /* id looper */
	NULL, /* copy data */
	samevolume_new_data, /* new data */
	NULL, /* get constraint targets */
	NULL, /* flush constraint targets */
	NULL, /* get target matrix */
	samevolume_evaluate /* evaluate */
};

/* ----------- Python Constraint -------------- */

static void pycon_free (bConstraint *con)
{
	bPythonConstraint *data= con->data;
	
	/* id-properties */
	IDP_FreeProperty(data->prop);
	MEM_freeN(data->prop);
	
	/* multiple targets */
	BLI_freelistN(&data->targets);
}	

static void pycon_relink (bConstraint *con)
{
	bPythonConstraint *data= con->data;
	
	ID_NEW(data->text);
}

static void pycon_copy (bConstraint *con, bConstraint *srccon)
{
	bPythonConstraint *pycon = (bPythonConstraint *)con->data;
	bPythonConstraint *opycon = (bPythonConstraint *)srccon->data;
	
	pycon->prop = IDP_CopyProperty(opycon->prop);
	BLI_duplicatelist(&pycon->targets, &opycon->targets);
}

static void pycon_new_data (void *cdata)
{
	bPythonConstraint *data= (bPythonConstraint *)cdata;
	
	/* everything should be set correctly by calloc, except for the prop->type constant.*/
	data->prop = MEM_callocN(sizeof(IDProperty), "PyConstraintProps");
	data->prop->type = IDP_GROUP;
}

static int pycon_get_tars (bConstraint *con, ListBase *list)
{
	if (con && list) {
		bPythonConstraint *data= con->data;
		
		list->first = data->targets.first;
		list->last = data->targets.last;
		
		return data->tarnum;
	}
	
	return 0;
}

static void pycon_id_looper (bConstraint *con, ConstraintIDFunc func, void *userdata)
{
	bPythonConstraint *data= con->data;
	bConstraintTarget *ct;
	
	/* targets */
	for (ct= data->targets.first; ct; ct= ct->next)
		func(con, (ID**)&ct->tar, userdata);
		
	/* script */
	func(con, (ID**)&data->text, userdata);
}

/* Whether this approach is maintained remains to be seen (aligorith) */
static void pycon_get_tarmat (bConstraint *con, bConstraintOb *cob, bConstraintTarget *ct, float UNUSED(ctime))
{
#ifdef WITH_PYTHON
	bPythonConstraint *data= con->data;
#endif

	if (VALID_CONS_TARGET(ct)) {
		/* special exception for curves - depsgraph issues */
		if (ct->tar->type == OB_CURVE) {
			Curve *cu= ct->tar->data;
			
			/* this check is to make sure curve objects get updated on file load correctly.*/
			if (cu->path==NULL || cu->path->data==NULL) /* only happens on reload file, but violates depsgraph still... fix! */
				makeDispListCurveTypes(cob->scene, ct->tar, 0);				
		}
		
		/* firstly calculate the matrix the normal way, then let the py-function override
		 * this matrix if it needs to do so
		 */
		constraint_target_to_mat4(ct->tar, ct->subtarget, ct->matrix, CONSTRAINT_SPACE_WORLD, ct->space, con->headtail);
		
		/* only execute target calculation if allowed */
#ifdef WITH_PYTHON
		if (G.f & G_SCRIPT_AUTOEXEC)
			BPY_pyconstraint_target(data, ct);
#endif
	}
	else if (ct)
		unit_m4(ct->matrix);
}

static void pycon_evaluate (bConstraint *con, bConstraintOb *cob, ListBase *targets)
{
#ifndef WITH_PYTHON
	(void)con; (void)cob; (void)targets; /* unused */
	return;
#else
	bPythonConstraint *data= con->data;
	
	/* only evaluate in python if we're allowed to do so */
	if ((G.f & G_SCRIPT_AUTOEXEC)==0)  return;
	
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
	pycon_relink, /* relink data */
	pycon_id_looper, /* id looper */
	pycon_copy, /* copy data */
	pycon_new_data, /* new data */
	pycon_get_tars, /* get constraint targets */
	NULL, /* flush constraint targets */
	pycon_get_tarmat, /* get target matrix */
	pycon_evaluate /* evaluate */
};

/* -------- Action Constraint ----------- */

static void actcon_relink (bConstraint *con)
{
	bActionConstraint *data= con->data;
	ID_NEW(data->act);
}

static void actcon_new_data (void *cdata)
{
	bActionConstraint *data= (bActionConstraint *)cdata;
	
	/* set type to 20 (Loc X), as 0 is Rot X for backwards compatibility */
	data->type = 20;
}

static void actcon_id_looper (bConstraint *con, ConstraintIDFunc func, void *userdata)
{
	bActionConstraint *data= con->data;
	
	/* target */
	func(con, (ID**)&data->tar, userdata);
	
	/* action */
	func(con, (ID**)&data->act, userdata);
}

static int actcon_get_tars (bConstraint *con, ListBase *list)
{
	if (con && list) {
		bActionConstraint *data= con->data;
		bConstraintTarget *ct;
		
		/* standard target-getting macro for single-target constraints */
		SINGLETARGET_GET_TARS(con, data->tar, data->subtarget, ct, list)
		
		return 1;
	}
	
	return 0;
}

static void actcon_flush_tars (bConstraint *con, ListBase *list, short nocopy)
{
	if (con && list) {
		bActionConstraint *data= con->data;
		bConstraintTarget *ct= list->first;
		
		/* the following macro is used for all standard single-target constraints */
		SINGLETARGET_FLUSH_TARS(con, data->tar, data->subtarget, ct, list, nocopy)
	}
}

static void actcon_get_tarmat (bConstraint *con, bConstraintOb *cob, bConstraintTarget *ct, float UNUSED(ctime))
{
	bActionConstraint *data = con->data;
	
	if (VALID_CONS_TARGET(ct)) {
		float tempmat[4][4], vec[3];
		float s, t;
		short axis;
		
		/* initialise return matrix */
		unit_m4(ct->matrix);
		
		/* get the transform matrix of the target */
		constraint_target_to_mat4(ct->tar, ct->subtarget, tempmat, CONSTRAINT_SPACE_WORLD, ct->space, con->headtail);
		
		/* determine where in transform range target is */
		/* data->type is mapped as follows for backwards compatibility:
		 *	00,01,02	- rotation (it used to be like this)
		 * 	10,11,12	- scaling
		 *	20,21,22	- location
		 */
		if (data->type < 10) {
			/* extract rotation (is in whatever space target should be in) */
			mat4_to_eul(vec, tempmat);
			mul_v3_fl(vec, RAD2DEGF(1.0f)); /* rad -> deg */
			axis= data->type;
		}
		else if (data->type < 20) {
			/* extract scaling (is in whatever space target should be in) */
			mat4_to_size(vec, tempmat);
			axis= data->type - 10;
		}
		else {
			/* extract location */
			copy_v3_v3(vec, tempmat[3]);
			axis= data->type - 20;
		}
		
		/* Target defines the animation */
		s = (vec[axis]-data->min) / (data->max-data->min);
		CLAMP(s, 0, 1);
		t = (s * (data->end-data->start)) + data->start;
		
		if (G.f & G_DEBUG)
			printf("do Action Constraint %s - Ob %s Pchan %s \n", con->name, cob->ob->id.name+2, (cob->pchan)?cob->pchan->name:NULL);
		
		/* Get the appropriate information from the action */
		if (cob->type == CONSTRAINT_OBTYPE_BONE) {
			Object workob;
			bPose *pose;
			bPoseChannel *pchan, *tchan;
			
			/* make a temporary pose and evaluate using that */
			pose = MEM_callocN(sizeof(bPose), "pose");
			
			/* make a copy of the bone of interest in the temp pose before evaluating action, so that it can get set 
			 *	- we need to manually copy over a few settings, including rotation order, otherwise this fails
			 */
			pchan = cob->pchan;
			
			tchan= verify_pose_channel(pose, pchan->name);
			tchan->rotmode= pchan->rotmode;
			
			/* evaluate action using workob (it will only set the PoseChannel in question) */
			what_does_obaction(cob->ob, &workob, pose, data->act, pchan->name, t);
			
			/* convert animation to matrices for use here */
			pchan_calc_mat(tchan);
			copy_m4_m4(ct->matrix, tchan->chan_mat);
			
			/* Clean up */
			free_pose(pose);
		}
		else if (cob->type == CONSTRAINT_OBTYPE_OBJECT) {
			Object workob;
			
			/* evaluate using workob */
			// FIXME: we don't have any consistent standards on limiting effects on object...
			what_does_obaction(cob->ob, &workob, NULL, data->act, NULL, t);
			object_to_mat4(&workob, ct->matrix);
		}
		else {
			/* behaviour undefined... */
			puts("Error: unknown owner type for Action Constraint");
		}
	}
}

static void actcon_evaluate (bConstraint *UNUSED(con), bConstraintOb *cob, ListBase *targets)
{
	bConstraintTarget *ct= targets->first;
	
	if (VALID_CONS_TARGET(ct)) {
		float temp[4][4];
		
		/* Nice and simple... we just need to multiply the matrices, as the get_target_matrix
		 * function has already taken care of everything else.
		 */
		copy_m4_m4(temp, cob->matrix);
		mult_m4_m4m4(cob->matrix, temp, ct->matrix);
	}
}

static bConstraintTypeInfo CTI_ACTION = {
	CONSTRAINT_TYPE_ACTION, /* type */
	sizeof(bActionConstraint), /* size */
	"Action", /* name */
	"bActionConstraint", /* struct name */
	NULL, /* free data */
	actcon_relink, /* relink data */
	actcon_id_looper, /* id looper */
	NULL, /* copy data */
	actcon_new_data, /* new data */
	actcon_get_tars, /* get constraint targets */
	actcon_flush_tars, /* flush constraint targets */
	actcon_get_tarmat, /* get target matrix */
	actcon_evaluate /* evaluate */
};

/* --------- Locked Track ---------- */

static void locktrack_new_data (void *cdata)
{
	bLockTrackConstraint *data= (bLockTrackConstraint *)cdata;
	
	data->trackflag = TRACK_Y;
	data->lockflag = LOCK_Z;
}	

static void locktrack_id_looper (bConstraint *con, ConstraintIDFunc func, void *userdata)
{
	bLockTrackConstraint *data= con->data;
	
	/* target only */
	func(con, (ID**)&data->tar, userdata);
}

static int locktrack_get_tars (bConstraint *con, ListBase *list)
{
	if (con && list) {
		bLockTrackConstraint *data= con->data;
		bConstraintTarget *ct;
		
		/* the following macro is used for all standard single-target constraints */
		SINGLETARGET_GET_TARS(con, data->tar, data->subtarget, ct, list)
		
		return 1;
	}
	
	return 0;
}

static void locktrack_flush_tars (bConstraint *con, ListBase *list, short nocopy)
{
	if (con && list) {
		bLockTrackConstraint *data= con->data;
		bConstraintTarget *ct= list->first;
		
		/* the following macro is used for all standard single-target constraints */
		SINGLETARGET_FLUSH_TARS(con, data->tar, data->subtarget, ct, list, nocopy)
	}
}

static void locktrack_evaluate (bConstraint *con, bConstraintOb *cob, ListBase *targets)
{
	bLockTrackConstraint *data= con->data;
	bConstraintTarget *ct= targets->first;
	
	if (VALID_CONS_TARGET(ct)) {
		float vec[3],vec2[3];
		float totmat[3][3];
		float tmpmat[3][3];
		float invmat[3][3];
		float tmat[4][4];
		float mdet;
		
		/* Vector object -> target */
		sub_v3_v3v3(vec, ct->matrix[3], cob->matrix[3]);
		switch (data->lockflag){
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
				}
					break;
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
				}
					break;
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
				}
					break;
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
				}
					break;
				default:
				{
					unit_m3(totmat);
				}
					break;
			}
		}
			break;
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
				}
					break;
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
				}
					break;
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
				}
					break;
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
				}
					break;
				default:
				{
					unit_m3(totmat);
				}
					break;
			}
		}
			break;
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
				}
					break;
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
				}
					break;
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
				}
					break;
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
				}
					break;
				default:
				{
					unit_m3(totmat);
				}
					break;
			}
		}
			break;
		default:
		{
			unit_m3(totmat);
		}
			break;
		}
		/* Block to keep matrix heading */
		copy_m3_m4(tmpmat, cob->matrix);
		normalize_m3(tmpmat);
		invert_m3_m3(invmat, tmpmat);
		mul_m3_m3m3(tmpmat, totmat, invmat);
		totmat[0][0] = tmpmat[0][0];totmat[0][1] = tmpmat[0][1];totmat[0][2] = tmpmat[0][2];
		totmat[1][0] = tmpmat[1][0];totmat[1][1] = tmpmat[1][1];totmat[1][2] = tmpmat[1][2];
		totmat[2][0] = tmpmat[2][0];totmat[2][1] = tmpmat[2][1];totmat[2][2] = tmpmat[2][2];
		
		copy_m4_m4(tmat, cob->matrix);
		
		mdet = determinant_m3(	totmat[0][0],totmat[0][1],totmat[0][2],
						totmat[1][0],totmat[1][1],totmat[1][2],
						totmat[2][0],totmat[2][1],totmat[2][2]);
		if (mdet==0) {
			unit_m3(totmat);
		}
		
		/* apply out transformaton to the object */
		mul_m4_m3m4(cob->matrix, totmat, tmat);
	}
}

static bConstraintTypeInfo CTI_LOCKTRACK = {
	CONSTRAINT_TYPE_LOCKTRACK, /* type */
	sizeof(bLockTrackConstraint), /* size */
	"Locked Track", /* name */
	"bLockTrackConstraint", /* struct name */
	NULL, /* free data */
	NULL, /* relink data */
	locktrack_id_looper, /* id looper */
	NULL, /* copy data */
	locktrack_new_data, /* new data */
	locktrack_get_tars, /* get constraint targets */
	locktrack_flush_tars, /* flush constraint targets */
	default_get_tarmat, /* get target matrix */
	locktrack_evaluate /* evaluate */
};

/* ---------- Limit Distance Constraint ----------- */

static void distlimit_new_data (void *cdata)
{
	bDistLimitConstraint *data= (bDistLimitConstraint *)cdata;
	
	data->dist= 0.0f;
}

static void distlimit_id_looper (bConstraint *con, ConstraintIDFunc func, void *userdata)
{
	bDistLimitConstraint *data= con->data;
	
	/* target only */
	func(con, (ID**)&data->tar, userdata);
}

static int distlimit_get_tars (bConstraint *con, ListBase *list)
{
	if (con && list) {
		bDistLimitConstraint *data= con->data;
		bConstraintTarget *ct;
		
		/* standard target-getting macro for single-target constraints */
		SINGLETARGET_GET_TARS(con, data->tar, data->subtarget, ct, list)
		
		return 1;
	}
	
	return 0;
}

static void distlimit_flush_tars (bConstraint *con, ListBase *list, short nocopy)
{
	if (con && list) {
		bDistLimitConstraint *data= con->data;
		bConstraintTarget *ct= list->first;
		
		/* the following macro is used for all standard single-target constraints */
		SINGLETARGET_FLUSH_TARS(con, data->tar, data->subtarget, ct, list, nocopy)
	}
}

static void distlimit_evaluate (bConstraint *con, bConstraintOb *cob, ListBase *targets)
{
	bDistLimitConstraint *data= con->data;
	bConstraintTarget *ct= targets->first;
	
	/* only evaluate if there is a target */
	if (VALID_CONS_TARGET(ct)) {
		float dvec[3], dist=0.0f, sfac=1.0f;
		short clamp_surf= 0;
		
		/* calculate our current distance from the target */
		dist= len_v3v3(cob->matrix[3], ct->matrix[3]);
		
		/* set distance (flag is only set when user demands it) */
		if (data->dist == 0)
			data->dist= dist;
		
		/* check if we're which way to clamp from, and calculate interpolation factor (if needed) */
		if (data->mode == LIMITDIST_OUTSIDE) {
			/* if inside, then move to surface */
			if (dist <= data->dist) {
				clamp_surf= 1;
				if (dist != 0.0f) sfac= data->dist / dist;
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
				clamp_surf= 1;
				if (dist != 0.0f) sfac= data->dist / dist;
			}
			/* if soft-distance is enabled, start fading once owner is dist-soft from the target */
			else if (data->flag & LIMITDIST_USESOFT) {
				// FIXME: there's a problem with "jumping" when this kicks in
				if (dist >= (data->dist - data->soft)) {
					sfac = (float)( data->soft*(1.0f - expf(-(dist - data->dist)/data->soft)) + data->dist );
					if (dist != 0.0f) sfac /= dist;
					
					clamp_surf= 1;
				}
			}
		}
		else {
			if (IS_EQF(dist, data->dist)==0) {
				clamp_surf= 1;
				if (dist != 0.0f) sfac= data->dist / dist;
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
	NULL, /* relink data */
	distlimit_id_looper, /* id looper */
	NULL, /* copy data */
	distlimit_new_data, /* new data */
	distlimit_get_tars, /* get constraint targets */
	distlimit_flush_tars, /* flush constraint targets */
	default_get_tarmat, /* get a target matrix */
	distlimit_evaluate /* evaluate */
};

/* ---------- Stretch To ------------ */

static void stretchto_new_data (void *cdata)
{
	bStretchToConstraint *data= (bStretchToConstraint *)cdata;
	
	data->volmode = 0;
	data->plane = 0;
	data->orglength = 0.0; 
	data->bulge = 1.0;
}

static void stretchto_id_looper (bConstraint *con, ConstraintIDFunc func, void *userdata)
{
	bStretchToConstraint *data= con->data;
	
	/* target only */
	func(con, (ID**)&data->tar, userdata);
}

static int stretchto_get_tars (bConstraint *con, ListBase *list)
{
	if (con && list) {
		bStretchToConstraint *data= con->data;
		bConstraintTarget *ct;
		
		/* standard target-getting macro for single-target constraints */
		SINGLETARGET_GET_TARS(con, data->tar, data->subtarget, ct, list)
		
		return 1;
	}
	
	return 0;
}

static void stretchto_flush_tars (bConstraint *con, ListBase *list, short nocopy)
{
	if (con && list) {
		bStretchToConstraint *data= con->data;
		bConstraintTarget *ct= list->first;
		
		/* the following macro is used for all standard single-target constraints */
		SINGLETARGET_FLUSH_TARS(con, data->tar, data->subtarget, ct, list, nocopy)
	}
}

static void stretchto_evaluate (bConstraint *con, bConstraintOb *cob, ListBase *targets)
{
	bStretchToConstraint *data= con->data;
	bConstraintTarget *ct= targets->first;
	
	/* only evaluate if there is a target */
	if (VALID_CONS_TARGET(ct)) {
		float size[3], scale[3], vec[3], xx[3], zz[3], orth[3];
		float totmat[3][3];
		float tmat[4][4];
		float dist;
		
		/* store scaling before destroying obmat */
		mat4_to_size(size, cob->matrix);
		
		/* store X orientation before destroying obmat */
		normalize_v3_v3(xx, cob->matrix[0]);
		
		/* store Z orientation before destroying obmat */
		normalize_v3_v3(zz, cob->matrix[2]);
		
		sub_v3_v3v3(vec, cob->matrix[3], ct->matrix[3]);
		vec[0] /= size[0];
		vec[1] /= size[1];
		vec[2] /= size[2];
		
		dist = normalize_v3(vec);
		//dist = len_v3v3( ob->obmat[3], targetmat[3]);
		
		/* data->orglength==0 occurs on first run, and after 'R' button is clicked */
		if (data->orglength == 0)  
			data->orglength = dist;
		if (data->bulge == 0) 
			data->bulge = 1.0;
		
		scale[1] = dist/data->orglength;
		switch (data->volmode) {
		/* volume preserving scaling */
		case VOLUME_XZ :
			scale[0] = 1.0f - (float)sqrt(data->bulge) + (float)sqrt(data->bulge*(data->orglength/dist));
			scale[2] = scale[0];
			break;
		case VOLUME_X:
			scale[0] = 1.0f + data->bulge * (data->orglength /dist - 1);
			scale[2] = 1.0;
			break;
		case VOLUME_Z:
			scale[0] = 1.0;
			scale[2] = 1.0f + data->bulge * (data->orglength /dist - 1);
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
		cob->matrix[0][0]=size[0]*scale[0];
		cob->matrix[0][1]=0;
		cob->matrix[0][2]=0;
		cob->matrix[1][0]=0;
		cob->matrix[1][1]=size[1]*scale[1];
		cob->matrix[1][2]=0;
		cob->matrix[2][0]=0;
		cob->matrix[2][1]=0;
		cob->matrix[2][2]=size[2]*scale[2];
		
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
		
		copy_m4_m4(tmat, cob->matrix);
		mul_m4_m3m4(cob->matrix, totmat, tmat);
	}
}

static bConstraintTypeInfo CTI_STRETCHTO = {
	CONSTRAINT_TYPE_STRETCHTO, /* type */
	sizeof(bStretchToConstraint), /* size */
	"Stretch To", /* name */
	"bStretchToConstraint", /* struct name */
	NULL, /* free data */
	NULL, /* relink data */
	stretchto_id_looper, /* id looper */
	NULL, /* copy data */
	stretchto_new_data, /* new data */
	stretchto_get_tars, /* get constraint targets */
	stretchto_flush_tars, /* flush constraint targets */
	default_get_tarmat, /* get target matrix */
	stretchto_evaluate /* evaluate */
};

/* ---------- Floor ------------ */

static void minmax_new_data (void *cdata)
{
	bMinMaxConstraint *data= (bMinMaxConstraint *)cdata;
	
	data->minmaxflag = TRACK_Z;
	data->offset = 0.0f;
	data->cache[0] = data->cache[1] = data->cache[2] = 0.0f;
	data->flag = 0;
}

static void minmax_id_looper (bConstraint *con, ConstraintIDFunc func, void *userdata)
{
	bMinMaxConstraint *data= con->data;
	
	/* target only */
	func(con, (ID**)&data->tar, userdata);
}

static int minmax_get_tars (bConstraint *con, ListBase *list)
{
	if (con && list) {
		bMinMaxConstraint *data= con->data;
		bConstraintTarget *ct;
		
		/* standard target-getting macro for single-target constraints */
		SINGLETARGET_GET_TARS(con, data->tar, data->subtarget, ct, list)
		
		return 1;
	}
	
	return 0;
}

static void minmax_flush_tars (bConstraint *con, ListBase *list, short nocopy)
{
	if (con && list) {
		bMinMaxConstraint *data= con->data;
		bConstraintTarget *ct= list->first;
		
		/* the following macro is used for all standard single-target constraints */
		SINGLETARGET_FLUSH_TARS(con, data->tar, data->subtarget, ct, list, nocopy)
	}
}

static void minmax_evaluate (bConstraint *con, bConstraintOb *cob, ListBase *targets)
{
	bMinMaxConstraint *data= con->data;
	bConstraintTarget *ct= targets->first;
	
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
			mult_m4_m4m4(tmat, imat, obmat);
			copy_m4_m4(obmat, tmat);
			unit_m4(tarmat);
		}
		
		switch (data->minmaxflag) {
		case TRACK_Z:
			val1 = tarmat[3][2];
			val2 = obmat[3][2]-data->offset;
			index = 2;
			break;
		case TRACK_Y:
			val1 = tarmat[3][1];
			val2 = obmat[3][1]-data->offset;
			index = 1;
			break;
		case TRACK_X:
			val1 = tarmat[3][0];
			val2 = obmat[3][0]-data->offset;
			index = 0;
			break;
		case TRACK_nZ:
			val2 = tarmat[3][2];
			val1 = obmat[3][2]-data->offset;
			index = 2;
			break;
		case TRACK_nY:
			val2 = tarmat[3][1];
			val1 = obmat[3][1]-data->offset;
			index = 1;
			break;
		case TRACK_nX:
			val2 = tarmat[3][0];
			val1 = obmat[3][0]-data->offset;
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
				mult_m4_m4m4(tmat, ct->matrix, obmat);
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
	NULL, /* relink data */
	minmax_id_looper, /* id looper */
	NULL, /* copy data */
	minmax_new_data, /* new data */
	minmax_get_tars, /* get constraint targets */
	minmax_flush_tars, /* flush constraint targets */
	default_get_tarmat, /* get target matrix */
	minmax_evaluate /* evaluate */
};

/* ------- RigidBody Joint ---------- */

static void rbj_new_data (void *cdata)
{
	bRigidBodyJointConstraint *data= (bRigidBodyJointConstraint *)cdata;
	
	// removed code which set target of this constraint  
	data->type=1;
}

static void rbj_id_looper (bConstraint *con, ConstraintIDFunc func, void *userdata)
{
	bRigidBodyJointConstraint *data= con->data;
	
	/* target only */
	func(con, (ID**)&data->tar, userdata);
	func(con, (ID**)&data->child, userdata);
}

static int rbj_get_tars (bConstraint *con, ListBase *list)
{
	if (con && list) {
		bRigidBodyJointConstraint *data= con->data;
		bConstraintTarget *ct;
		
		/* standard target-getting macro for single-target constraints without subtargets */
		SINGLETARGETNS_GET_TARS(con, data->tar, ct, list)
		
		return 1;
	}
	
	return 0;
}

static void rbj_flush_tars (bConstraint *con, ListBase *list, short nocopy)
{
	if (con && list) {
		bRigidBodyJointConstraint *data= con->data;
		bConstraintTarget *ct= list->first;
		
		/* the following macro is used for all standard single-target constraints */
		SINGLETARGETNS_FLUSH_TARS(con, data->tar, ct, list, nocopy)
	}
}

static bConstraintTypeInfo CTI_RIGIDBODYJOINT = {
	CONSTRAINT_TYPE_RIGIDBODYJOINT, /* type */
	sizeof(bRigidBodyJointConstraint), /* size */
	"Rigid Body Joint", /* name */
	"bRigidBodyJointConstraint", /* struct name */
	NULL, /* free data */
	NULL, /* relink data */
	rbj_id_looper, /* id looper */
	NULL, /* copy data */
	rbj_new_data, /* new data */
	rbj_get_tars, /* get constraint targets */
	rbj_flush_tars, /* flush constraint targets */
	default_get_tarmat, /* get target matrix */
	NULL /* evaluate - this is not solved here... is just an interface for game-engine */
};

/* -------- Clamp To ---------- */

static void clampto_id_looper (bConstraint *con, ConstraintIDFunc func, void *userdata)
{
	bClampToConstraint *data= con->data;
	
	/* target only */
	func(con, (ID**)&data->tar, userdata);
}

static int clampto_get_tars (bConstraint *con, ListBase *list)
{
	if (con && list) {
		bClampToConstraint *data= con->data;
		bConstraintTarget *ct;
		
		/* standard target-getting macro for single-target constraints without subtargets */
		SINGLETARGETNS_GET_TARS(con, data->tar, ct, list)
		
		return 1;
	}
	
	return 0;
}

static void clampto_flush_tars (bConstraint *con, ListBase *list, short nocopy)
{
	if (con && list) {
		bClampToConstraint *data= con->data;
		bConstraintTarget *ct= list->first;
		
		/* the following macro is used for all standard single-target constraints */
		SINGLETARGETNS_FLUSH_TARS(con, data->tar, ct, list, nocopy)
	}
}

static void clampto_get_tarmat (bConstraint *UNUSED(con), bConstraintOb *cob, bConstraintTarget *ct, float UNUSED(ctime))
{
	if (VALID_CONS_TARGET(ct)) {
		Curve *cu= ct->tar->data;
		
		/* note: when creating constraints that follow path, the curve gets the CU_PATH set now,
		 *		currently for paths to work it needs to go through the bevlist/displist system (ton) 
		 */
		
		/* only happens on reload file, but violates depsgraph still... fix! */
		if (cu->path==NULL || cu->path->data==NULL)
			makeDispListCurveTypes(cob->scene, ct->tar, 0);
	}
	
	/* technically, this isn't really needed for evaluation, but we don't know what else
	 * might end up calling this...
	 */
	if (ct)
		unit_m4(ct->matrix);
}

static void clampto_evaluate (bConstraint *con, bConstraintOb *cob, ListBase *targets)
{
	bClampToConstraint *data= con->data;
	bConstraintTarget *ct= targets->first;
	
	/* only evaluate if there is a target and it is a curve */
	if (VALID_CONS_TARGET(ct) && (ct->tar->type == OB_CURVE)) {
		Curve *cu= data->tar->data;
		float obmat[4][4], ownLoc[3];
		float curveMin[3], curveMax[3];
		float targetMatrix[4][4]= MAT4_UNITY;
		
		copy_m4_m4(obmat, cob->matrix);
		copy_v3_v3(ownLoc, obmat[3]);
		
		INIT_MINMAX(curveMin, curveMax)
		minmax_object(ct->tar, curveMin, curveMax);
		
		/* get targetmatrix */
		if (cu->path && cu->path->data) {
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
				if ((size[2]>size[0]) && (size[2]>size[1]))
					clamp_axis= CLAMPTO_Z - 1;
				else if ((size[1]>size[0]) && (size[1]>size[2]))
					clamp_axis= CLAMPTO_Y - 1;
				else
					clamp_axis = CLAMPTO_X - 1;
			}
			else 
				clamp_axis= data->flag - 1;
				
			/* 2. determine position relative to curve on a 0-1 scale based on bounding box */
			if (data->flag2 & CLAMPTO_CYCLIC) {
				/* cyclic, so offset within relative bounding box is used */
				float len= (curveMax[clamp_axis] - curveMin[clamp_axis]);
				float offset;
				
				/* check to make sure len is not so close to zero that it'll cause errors */
				if (IS_EQ(len, 0) == 0) {
					/* find bounding-box range where target is located */
					if (ownLoc[clamp_axis] < curveMin[clamp_axis]) {
						/* bounding-box range is before */
						offset= curveMin[clamp_axis];
						
						while (ownLoc[clamp_axis] < offset)
							offset -= len;
						
						/* now, we calculate as per normal, except using offset instead of curveMin[clamp_axis] */
						curvetime = (ownLoc[clamp_axis] - offset) / (len);
					}
					else if (ownLoc[clamp_axis] > curveMax[clamp_axis]) {
						/* bounding-box range is after */
						offset= curveMax[clamp_axis];
						
						while (ownLoc[clamp_axis] > offset) {
							if ((offset + len) > ownLoc[clamp_axis])
								break;
							else
								offset += len;
						}
						
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
					curvetime= 0.0f;
				}
			}
			else {
				/* no cyclic, so position is clamped to within the bounding box */
				if (ownLoc[clamp_axis] <= curveMin[clamp_axis])
					curvetime = 0.0f;
				else if (ownLoc[clamp_axis] >= curveMax[clamp_axis])
					curvetime = 1.0f;
				else if ( IS_EQ((curveMax[clamp_axis] - curveMin[clamp_axis]), 0) == 0 )
					curvetime = (ownLoc[clamp_axis] - curveMin[clamp_axis]) / (curveMax[clamp_axis] - curveMin[clamp_axis]);
				else 
					curvetime = 0.0f;
			}
			
			/* 3. position on curve */
			if (where_on_path(ct->tar, curvetime, vec, dir, NULL, NULL, NULL) ) {
				unit_m4(totmat);
				copy_v3_v3(totmat[3], vec);
				
				mul_serie_m4(targetMatrix, ct->tar->obmat, totmat, NULL, NULL, NULL, NULL, NULL, NULL);
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
	NULL, /* relink data */
	clampto_id_looper, /* id looper */
	NULL, /* copy data */
	NULL, /* new data */
	clampto_get_tars, /* get constraint targets */
	clampto_flush_tars, /* flush constraint targets */
	clampto_get_tarmat, /* get target matrix */
	clampto_evaluate /* evaluate */
};

/* ---------- Transform Constraint ----------- */

static void transform_new_data (void *cdata)
{
	bTransformConstraint *data= (bTransformConstraint *)cdata;
	
	data->map[0]= 0;
	data->map[1]= 1;
	data->map[2]= 2;
}

static void transform_id_looper (bConstraint *con, ConstraintIDFunc func, void *userdata)
{
	bTransformConstraint *data= con->data;
	
	/* target only */
	func(con, (ID**)&data->tar, userdata);
}

static int transform_get_tars (bConstraint *con, ListBase *list)
{
	if (con && list) {
		bTransformConstraint *data= con->data;
		bConstraintTarget *ct;
		
		/* standard target-getting macro for single-target constraints */
		SINGLETARGET_GET_TARS(con, data->tar, data->subtarget, ct, list)
		
		return 1;
	}
	
	return 0;
}

static void transform_flush_tars (bConstraint *con, ListBase *list, short nocopy)
{
	if (con && list) {
		bTransformConstraint *data= con->data;
		bConstraintTarget *ct= list->first;
		
		/* the following macro is used for all standard single-target constraints */
		SINGLETARGET_FLUSH_TARS(con, data->tar, data->subtarget, ct, list, nocopy)
	}
}

static void transform_evaluate (bConstraint *con, bConstraintOb *cob, ListBase *targets)
{
	bTransformConstraint *data= con->data;
	bConstraintTarget *ct= targets->first;
	
	/* only evaluate if there is a target */
	if (VALID_CONS_TARGET(ct)) {
		float loc[3], eul[3], size[3];
		float dvec[3], sval[3];
		int i;
		
		/* obtain target effect */
		switch (data->from) {
			case 2: /* scale */
				mat4_to_size( dvec,ct->matrix);
				break;
			case 1: /* rotation (convert to degrees first) */
				mat4_to_eulO(dvec, cob->rotOrder, ct->matrix);
				mul_v3_fl(dvec, RAD2DEGF(1.0f)); /* rad -> deg */
				break;
			default: /* location */
				copy_v3_v3(dvec, ct->matrix[3]);
				break;
		}
		
		/* extract components of owner's matrix */
		copy_v3_v3(loc, cob->matrix[3]);
		mat4_to_eulO(eul, cob->rotOrder, cob->matrix);
		mat4_to_size(size, cob->matrix);	
		
		/* determine where in range current transforms lie */
		if (data->expo) {
			for (i=0; i<3; i++) {
				if (data->from_max[i] - data->from_min[i])
					sval[i]= (dvec[i] - data->from_min[i]) / (data->from_max[i] - data->from_min[i]);
				else
					sval[i]= 0.0f;
			}
		}
		else {
			/* clamp transforms out of range */
			for (i=0; i<3; i++) {
				CLAMP(dvec[i], data->from_min[i], data->from_max[i]);
				if (data->from_max[i] - data->from_min[i])
					sval[i]= (dvec[i] - data->from_min[i]) / (data->from_max[i] - data->from_min[i]);
				else
					sval[i]= 0.0f;
			}
		}
		
		
		/* apply transforms */
		switch (data->to) {
			case 2: /* scaling */
				for (i=0; i<3; i++)
					size[i]= data->to_min[i] + (sval[(int)data->map[i]] * (data->to_max[i] - data->to_min[i])); 
				break;
			case 1: /* rotation */
				for (i=0; i<3; i++) {
					float tmin, tmax;
					
					tmin= data->to_min[i];
					tmax= data->to_max[i];
					
					/* all values here should be in degrees */
					eul[i]= tmin + (sval[(int)data->map[i]] * (tmax - tmin)); 
					
					/* now convert final value back to radians */
					eul[i] = DEG2RADF(eul[i]);
				}
				break;
			default: /* location */
				/* get new location */
				for (i=0; i<3; i++)
					loc[i]= (data->to_min[i] + (sval[(int)data->map[i]] * (data->to_max[i] - data->to_min[i])));
				
				/* add original location back on (so that it can still be moved) */
				add_v3_v3v3(loc, cob->matrix[3], loc);
				break;
		}
		
		/* apply to matrix */
		loc_eulO_size_to_mat4(cob->matrix, loc, eul, size, cob->rotOrder);
	}
}

static bConstraintTypeInfo CTI_TRANSFORM = {
	CONSTRAINT_TYPE_TRANSFORM, /* type */
	sizeof(bTransformConstraint), /* size */
	"Transform", /* name */
	"bTransformConstraint", /* struct name */
	NULL, /* free data */
	NULL, /* relink data */
	transform_id_looper, /* id looper */
	NULL, /* copy data */
	transform_new_data, /* new data */
	transform_get_tars, /* get constraint targets */
	transform_flush_tars, /* flush constraint targets */
	default_get_tarmat, /* get a target matrix */
	transform_evaluate /* evaluate */
};

/* ---------- Shrinkwrap Constraint ----------- */

static void shrinkwrap_id_looper (bConstraint *con, ConstraintIDFunc func, void *userdata)
{
	bShrinkwrapConstraint *data= con->data;
	
	/* target only */
	func(con, (ID**)&data->target, userdata);
}

static int shrinkwrap_get_tars (bConstraint *con, ListBase *list)
{
	if (con && list) {
		bShrinkwrapConstraint *data = con->data;
		bConstraintTarget *ct;
		
		SINGLETARGETNS_GET_TARS(con, data->target, ct, list)
		
		return 1;
	}
	
	return 0;
}


static void shrinkwrap_flush_tars (bConstraint *con, ListBase *list, short nocopy)
{
	if (con && list) {
		bShrinkwrapConstraint *data = con->data;
		bConstraintTarget *ct= list->first;
		
		SINGLETARGETNS_FLUSH_TARS(con, data->target, ct, list, nocopy)
	}
}


static void shrinkwrap_get_tarmat (bConstraint *con, bConstraintOb *cob, bConstraintTarget *ct, float UNUSED(ctime))
{
	bShrinkwrapConstraint *scon = (bShrinkwrapConstraint *) con->data;
	
	if( VALID_CONS_TARGET(ct) && (ct->tar->type == OB_MESH) )
	{
		int fail = FALSE;
		float co[3] = {0.0f, 0.0f, 0.0f};
		float no[3] = {0.0f, 0.0f, 0.0f};
		float dist;
		
		SpaceTransform transform;
		DerivedMesh *target = object_get_derived_final(ct->tar);
		BVHTreeRayHit hit;
		BVHTreeNearest nearest;
		
		BVHTreeFromMesh treeData= {NULL};
		
		nearest.index = -1;
		nearest.dist = FLT_MAX;
		
		hit.index = -1;
		hit.dist = 100000.0f;  //TODO should use FLT_MAX.. but normal projection doenst yet supports it
		
		unit_m4(ct->matrix);
		
		if(target != NULL)
		{
			space_transform_from_matrixs(&transform, cob->matrix, ct->tar->obmat);
			
			switch(scon->shrinkType)
			{
				case MOD_SHRINKWRAP_NEAREST_SURFACE:
				case MOD_SHRINKWRAP_NEAREST_VERTEX:
					
					if(scon->shrinkType == MOD_SHRINKWRAP_NEAREST_VERTEX)
						bvhtree_from_mesh_verts(&treeData, target, 0.0, 2, 6);
					else
						bvhtree_from_mesh_faces(&treeData, target, 0.0, 2, 6);
					
					if(treeData.tree == NULL)
					{
						fail = TRUE;
						break;
					}
					
					space_transform_apply(&transform, co);
					
					BLI_bvhtree_find_nearest(treeData.tree, co, &nearest, treeData.nearest_callback, &treeData);
					
					dist = len_v3v3(co, nearest.co);
					if(dist != 0.0f) {
						interp_v3_v3v3(co, co, nearest.co, (dist - scon->dist)/dist);	/* linear interpolation */
					}
					space_transform_invert(&transform, co);
				break;
				
				case MOD_SHRINKWRAP_PROJECT:
					if(scon->projAxis & MOD_SHRINKWRAP_PROJECT_OVER_X_AXIS) no[0] = 1.0f;
					if(scon->projAxis & MOD_SHRINKWRAP_PROJECT_OVER_Y_AXIS) no[1] = 1.0f;
					if(scon->projAxis & MOD_SHRINKWRAP_PROJECT_OVER_Z_AXIS) no[2] = 1.0f;
					
					if(INPR(no,no) < FLT_EPSILON)
					{
						fail = TRUE;
						break;
					}
					
					normalize_v3(no);
					
					
					bvhtree_from_mesh_faces(&treeData, target, scon->dist, 4, 6);
					if(treeData.tree == NULL)
					{
						fail = TRUE;
						break;
					}
					
					if(normal_projection_project_vertex(0, co, no, &transform, treeData.tree, &hit, treeData.raycast_callback, &treeData) == FALSE)
					{
						fail = TRUE;
						break;
					}
					copy_v3_v3(co, hit.co);
				break;
			}
			
			free_bvhtree_from_mesh(&treeData);
			
			target->release(target);
			
			if(fail == TRUE)
			{
				/* Don't move the point */
				co[0] = co[1] = co[2] = 0.0f;
			}
			
			/* co is in local object coordinates, change it to global and update target position */
			mul_m4_v3(cob->matrix, co);
			copy_v3_v3(ct->matrix[3], co);
		}
	}
}

static void shrinkwrap_evaluate (bConstraint *UNUSED(con), bConstraintOb *cob, ListBase *targets)
{
	bConstraintTarget *ct= targets->first;
	
	/* only evaluate if there is a target */
	if (VALID_CONS_TARGET(ct))
	{
		copy_v3_v3(cob->matrix[3], ct->matrix[3]);
	}
}

static bConstraintTypeInfo CTI_SHRINKWRAP = {
	CONSTRAINT_TYPE_SHRINKWRAP, /* type */
	sizeof(bShrinkwrapConstraint), /* size */
	"Shrinkwrap", /* name */
	"bShrinkwrapConstraint", /* struct name */
	NULL, /* free data */
	NULL, /* relink data */
	shrinkwrap_id_looper, /* id looper */
	NULL, /* copy data */
	NULL, /* new data */
	shrinkwrap_get_tars, /* get constraint targets */
	shrinkwrap_flush_tars, /* flush constraint targets */
	shrinkwrap_get_tarmat, /* get a target matrix */
	shrinkwrap_evaluate /* evaluate */
};

/* --------- Damped Track ---------- */

static void damptrack_new_data (void *cdata)
{
	bDampTrackConstraint *data= (bDampTrackConstraint *)cdata;
	
	data->trackflag = TRACK_Y;
}	

static void damptrack_id_looper (bConstraint *con, ConstraintIDFunc func, void *userdata)
{
	bDampTrackConstraint *data= con->data;
	
	/* target only */
	func(con, (ID**)&data->tar, userdata);
}

static int damptrack_get_tars (bConstraint *con, ListBase *list)
{
	if (con && list) {
		bDampTrackConstraint *data= con->data;
		bConstraintTarget *ct;
		
		/* the following macro is used for all standard single-target constraints */
		SINGLETARGET_GET_TARS(con, data->tar, data->subtarget, ct, list)
		
		return 1;
	}
	
	return 0;
}

static void damptrack_flush_tars (bConstraint *con, ListBase *list, short nocopy)
{
	if (con && list) {
		bDampTrackConstraint *data= con->data;
		bConstraintTarget *ct= list->first;
		
		/* the following macro is used for all standard single-target constraints */
		SINGLETARGET_FLUSH_TARS(con, data->tar, data->subtarget, ct, list, nocopy)
	}
}

/* array of direction vectors for the tracking flags */
static const float track_dir_vecs[6][3] = {
	{+1,0,0}, {0,+1,0}, {0,0,+1},		/* TRACK_X,  TRACK_Y,  TRACK_Z */
	{-1,0,0}, {0,-1,0}, {0,0,-1}		/* TRACK_NX, TRACK_NY, TRACK_NZ */
};

static void damptrack_evaluate (bConstraint *con, bConstraintOb *cob, ListBase *targets)
{
	bDampTrackConstraint *data= con->data;
	bConstraintTarget *ct= targets->first;
	
	if (VALID_CONS_TARGET(ct)) {
		float obvec[3], tarvec[3], obloc[3];
		float raxis[3], rangle;
		float rmat[3][3], tmat[4][4];
		
		/* find the (unit) direction that the axis we're interested in currently points 
		 *	- mul_mat3_m4_v3() only takes the 3x3 (rotation+scaling) components of the 4x4 matrix 
		 *	- the normalisation step at the end should take care of any unwanted scaling
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
			// FIXME: or would it be better to use the pure direction vector?
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
		
		rangle= dot_v3v3(obvec, tarvec);
		rangle= acos( MAX2(-1.0f, MIN2(1.0f, rangle)) );
		
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
	NULL, /* relink data */
	damptrack_id_looper, /* id looper */
	NULL, /* copy data */
	damptrack_new_data, /* new data */
	damptrack_get_tars, /* get constraint targets */
	damptrack_flush_tars, /* flush constraint targets */
	default_get_tarmat, /* get target matrix */
	damptrack_evaluate /* evaluate */
};

/* ----------- Spline IK ------------ */

static void splineik_free (bConstraint *con)
{
	bSplineIKConstraint *data= con->data;
	
	/* binding array */
	if (data->points)
		MEM_freeN(data->points);
}	

static void splineik_copy (bConstraint *con, bConstraint *srccon)
{
	bSplineIKConstraint *src= srccon->data;
	bSplineIKConstraint *dst= con->data;
	
	/* copy the binding array */
	dst->points= MEM_dupallocN(src->points);
}

static void splineik_new_data (void *cdata)
{
	bSplineIKConstraint *data= (bSplineIKConstraint *)cdata;

	data->chainlen= 1;
}

static void splineik_id_looper (bConstraint *con, ConstraintIDFunc func, void *userdata)
{
	bSplineIKConstraint *data= con->data;
	
	/* target only */
	func(con, (ID**)&data->tar, userdata);
}

static int splineik_get_tars (bConstraint *con, ListBase *list)
{
	if (con && list) {
		bSplineIKConstraint *data= con->data;
		bConstraintTarget *ct;
		
		/* standard target-getting macro for single-target constraints without subtargets */
		SINGLETARGETNS_GET_TARS(con, data->tar, ct, list)
		
		return 1;
	}
	
	return 0;
}

static void splineik_flush_tars (bConstraint *con, ListBase *list, short nocopy)
{
	if (con && list) {
		bSplineIKConstraint *data= con->data;
		bConstraintTarget *ct= list->first;
		
		/* the following macro is used for all standard single-target constraints */
		SINGLETARGETNS_FLUSH_TARS(con, data->tar, ct, list, nocopy)
	}
}

static void splineik_get_tarmat (bConstraint *UNUSED(con), bConstraintOb *cob, bConstraintTarget *ct, float UNUSED(ctime))
{
	if (VALID_CONS_TARGET(ct)) {
		Curve *cu= ct->tar->data;
		
		/* note: when creating constraints that follow path, the curve gets the CU_PATH set now,
		 *		currently for paths to work it needs to go through the bevlist/displist system (ton) 
		 */
		
		/* only happens on reload file, but violates depsgraph still... fix! */
		if (cu->path==NULL || cu->path->data==NULL)
			makeDispListCurveTypes(cob->scene, ct->tar, 0);
	}
	
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
	NULL, /* relink data */
	splineik_id_looper, /* id looper */
	splineik_copy, /* copy data */
	splineik_new_data, /* new data */
	splineik_get_tars, /* get constraint targets */
	splineik_flush_tars, /* flush constraint targets */
	splineik_get_tarmat, /* get target matrix */
	NULL /* evaluate - solved as separate loop */
};

/* ----------- Pivot ------------- */

static void pivotcon_id_looper (bConstraint *con, ConstraintIDFunc func, void *userdata)
{
	bPivotConstraint *data= con->data;
	
	/* target only */
	func(con, (ID**)&data->tar, userdata);
}

static int pivotcon_get_tars (bConstraint *con, ListBase *list)
{
	if (con && list) {
		bPivotConstraint *data= con->data;
		bConstraintTarget *ct;
		
		/* standard target-getting macro for single-target constraints */
		SINGLETARGET_GET_TARS(con, data->tar, data->subtarget, ct, list)
		
		return 1;
	}
	
	return 0;
}

static void pivotcon_flush_tars (bConstraint *con, ListBase *list, short nocopy)
{
	if (con && list) {
		bPivotConstraint *data= con->data;
		bConstraintTarget *ct= list->first;
		
		/* the following macro is used for all standard single-target constraints */
		SINGLETARGET_FLUSH_TARS(con, data->tar, data->subtarget, ct, list, nocopy)
	}
}

static void pivotcon_evaluate (bConstraint *con, bConstraintOb *cob, ListBase *targets)
{
	bPivotConstraint *data= con->data;
	bConstraintTarget *ct= targets->first;
	
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
	// TODO: perhaps we might want to include scaling based on the pivot too?
	copy_m3_m4(rotMat, cob->matrix);
	normalize_m3(rotMat);


	/* correct the pivot by the rotation axis otherwise the pivot translates when it shouldnt */
	mat3_to_axis_angle(axis, &angle, rotMat);
	if(angle) {
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
	NULL, /* relink data */
	pivotcon_id_looper, /* id looper */
	NULL, /* copy data */
	NULL, /* new data */ // XXX: might be needed to get 'normal' pivot behaviour...
	pivotcon_get_tars, /* get constraint targets */
	pivotcon_flush_tars, /* flush constraint targets */
	default_get_tarmat, /* get target matrix */
	pivotcon_evaluate /* evaluate */
};

/* ----------- Follow Track ------------- */

static void followtrack_new_data (void *cdata)
{
	bFollowTrackConstraint *data= (bFollowTrackConstraint *)cdata;
	
	data->clip= NULL;
	data->flag |= FOLLOWTRACK_ACTIVECLIP;
}

static void followtrack_id_looper (bConstraint *con, ConstraintIDFunc func, void *userdata)
{
	bFollowTrackConstraint *data= con->data;
	
	func(con, (ID**)&data->clip, userdata);
}

static void followtrack_evaluate (bConstraint *con, bConstraintOb *cob, ListBase *UNUSED(targets))
{
	Scene *scene= cob->scene;
	bFollowTrackConstraint *data= con->data;
	MovieClip *clip= data->clip;
	MovieTrackingTrack *track;
	
	if (data->flag & FOLLOWTRACK_ACTIVECLIP)
		clip= scene->clip;
	
	if (!clip || !data->track[0])
		return;
	
	track= BKE_tracking_named_track(&clip->tracking, data->track);
	
	if (!track)
		return;
	
	if (data->flag & FOLLOWTRACK_USE_3D_POSITION) {
		if (track->flag & TRACK_HAS_BUNDLE) {
			float pos[3], mat[4][4], obmat[4][4];
			
			copy_m4_m4(obmat, cob->matrix);
			
			BKE_get_tracking_mat(cob->scene, NULL, mat);
			mul_v3_m4v3(pos, mat, track->bundle_pos);
			
			cob->matrix[3][0] += pos[0];
			cob->matrix[3][1] += pos[1];
			cob->matrix[3][2] += pos[2];
		}
	} 
	else {
		Object *camob= cob->scene->camera;
		
		if (camob) {
			MovieClipUser user;
			MovieTrackingMarker *marker;
			float vec[3], disp[3], axis[3], mat[4][4];
			float aspect= (scene->r.xsch*scene->r.xasp) / (scene->r.ysch*scene->r.yasp);
			float len, d;
			
			where_is_object_mat(scene, camob, mat);
			
			/* camera axis */
			vec[0]= 0.0f;
			vec[1]= 0.0f;
			vec[2]= 1.0f;
			mul_v3_m4v3(axis, mat, vec);
			
			/* distance to projection plane */
			copy_v3_v3(vec, cob->matrix[3]);
			sub_v3_v3(vec, mat[3]);
			project_v3_v3v3(disp, vec, axis);
			
			len= len_v3(disp);
			
			if (len > FLT_EPSILON) {
				CameraParams params;
				float pos[2], rmat[4][4];
				
				user.framenr= scene->r.cfra;
				marker= BKE_tracking_get_marker(track, user.framenr);
				
				add_v2_v2v2(pos, marker->pos, track->offset);
				
				camera_params_init(&params);
				camera_params_from_object(&params, camob);

				if (params.is_ortho) {
					vec[0]= params.ortho_scale * (pos[0]-0.5f+params.shiftx);
					vec[1]= params.ortho_scale * (pos[1]-0.5f+params.shifty);
					vec[2]= -len;
					
					if (aspect > 1.0f) vec[1] /= aspect;
					else vec[0] *= aspect;
					
					mul_v3_m4v3(disp, camob->obmat, vec);
					
					copy_m4_m4(rmat, camob->obmat);
					zero_v3(rmat[3]);
					mult_m4_m4m4(cob->matrix, cob->matrix, rmat);
					
					copy_v3_v3(cob->matrix[3], disp);
				}
				else {
					d= (len*params.sensor_x) / (2.0f*params.lens);
					
					vec[0]= d*(2.0f*(pos[0]+params.shiftx)-1.0f);
					vec[1]= d*(2.0f*(pos[1]+params.shifty)-1.0f);
					vec[2]= -len;
					
					if (aspect > 1.0f) vec[1] /= aspect;
					else vec[0] *= aspect;
					
					mul_v3_m4v3(disp, camob->obmat, vec);
					
					/* apply camera rotation so Z-axis would be co-linear */
					copy_m4_m4(rmat, camob->obmat);
					zero_v3(rmat[3]);
					mult_m4_m4m4(cob->matrix, cob->matrix, rmat);
					
					copy_v3_v3(cob->matrix[3], disp);
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
	NULL, /* relink data */
	followtrack_id_looper, /* id looper */
	NULL, /* copy data */
	followtrack_new_data, /* new data */
	NULL, /* get constraint targets */
	NULL, /* flush constraint targets */
	NULL, /* get target matrix */
	followtrack_evaluate /* evaluate */
};

/* ----------- Camre Solver ------------- */

static void camerasolver_new_data (void *cdata)
{
	bCameraSolverConstraint *data= (bCameraSolverConstraint *)cdata;
	
	data->clip = NULL;
	data->flag |= CAMERASOLVER_ACTIVECLIP;
}

static void camerasolver_id_looper (bConstraint *con, ConstraintIDFunc func, void *userdata)
{
	bCameraSolverConstraint *data= con->data;
	
	func(con, (ID**)&data->clip, userdata);
}

static void camerasolver_evaluate (bConstraint *con, bConstraintOb *cob, ListBase *UNUSED(targets))
{
	Scene *scene= cob->scene;
	bCameraSolverConstraint *data= con->data;
	MovieClip *clip= data->clip;
	
	if (data->flag & CAMERASOLVER_ACTIVECLIP)
		clip= scene->clip;
	
	if (clip) {
		float mat[4][4], obmat[4][4];
		
		BKE_tracking_get_interpolated_camera(&clip->tracking, scene->r.cfra, mat);
		
		copy_m4_m4(obmat, cob->matrix);
		mult_m4_m4m4(cob->matrix, obmat, mat);
	}
}

static bConstraintTypeInfo CTI_CAMERASOLVER = {
	CONSTRAINT_TYPE_CAMERASOLVER, /* type */
	sizeof(bCameraSolverConstraint), /* size */
	"Camera Solver", /* name */
	"bCameraSolverConstraint", /* struct name */
	NULL, /* free data */
	NULL, /* relink data */
	camerasolver_id_looper, /* id looper */
	NULL, /* copy data */
	camerasolver_new_data, /* new data */
	NULL, /* get constraint targets */
	NULL, /* flush constraint targets */
	NULL, /* get target matrix */
	camerasolver_evaluate /* evaluate */
};

/* ************************* Constraints Type-Info *************************** */
/* All of the constraints api functions use bConstraintTypeInfo structs to carry out
 * and operations that involve constraint specific code.
 */

/* These globals only ever get directly accessed in this file */
static bConstraintTypeInfo *constraintsTypeInfo[NUM_CONSTRAINT_TYPES];
static short CTI_INIT= 1; /* when non-zero, the list needs to be updated */

/* This function only gets called when CTI_INIT is non-zero */
static void constraints_init_typeinfo (void)
{
	constraintsTypeInfo[0]=  NULL; 					/* 'Null' Constraint */
	constraintsTypeInfo[1]=  &CTI_CHILDOF; 			/* ChildOf Constraint */
	constraintsTypeInfo[2]=  &CTI_TRACKTO;			/* TrackTo Constraint */
	constraintsTypeInfo[3]=  &CTI_KINEMATIC;		/* IK Constraint */
	constraintsTypeInfo[4]=  &CTI_FOLLOWPATH;		/* Follow-Path Constraint */
	constraintsTypeInfo[5]=  &CTI_ROTLIMIT;			/* Limit Rotation Constraint */
	constraintsTypeInfo[6]=  &CTI_LOCLIMIT;			/* Limit Location Constraint */
	constraintsTypeInfo[7]=  &CTI_SIZELIMIT;		/* Limit Scaling Constraint */
	constraintsTypeInfo[8]=  &CTI_ROTLIKE;			/* Copy Rotation Constraint */
	constraintsTypeInfo[9]=  &CTI_LOCLIKE;			/* Copy Location Constraint */
	constraintsTypeInfo[10]= &CTI_SIZELIKE;			/* Copy Scaling Constraint */
	constraintsTypeInfo[11]= &CTI_PYTHON;			/* Python/Script Constraint */
	constraintsTypeInfo[12]= &CTI_ACTION;			/* Action Constraint */
	constraintsTypeInfo[13]= &CTI_LOCKTRACK;		/* Locked-Track Constraint */
	constraintsTypeInfo[14]= &CTI_DISTLIMIT;		/* Limit Distance Constraint */
	constraintsTypeInfo[15]= &CTI_STRETCHTO; 		/* StretchTo Constaint */ 
	constraintsTypeInfo[16]= &CTI_MINMAX;  			/* Floor Constraint */
	constraintsTypeInfo[17]= &CTI_RIGIDBODYJOINT;	/* RigidBody Constraint */
	constraintsTypeInfo[18]= &CTI_CLAMPTO; 			/* ClampTo Constraint */	
	constraintsTypeInfo[19]= &CTI_TRANSFORM;		/* Transformation Constraint */
	constraintsTypeInfo[20]= &CTI_SHRINKWRAP;		/* Shrinkwrap Constraint */
	constraintsTypeInfo[21]= &CTI_DAMPTRACK;		/* Damped TrackTo Constraint */
	constraintsTypeInfo[22]= &CTI_SPLINEIK;			/* Spline IK Constraint */
	constraintsTypeInfo[23]= &CTI_TRANSLIKE;		/* Copy Transforms Constraint */
	constraintsTypeInfo[24]= &CTI_SAMEVOL;			/* Maintain Volume Constraint */
	constraintsTypeInfo[25]= &CTI_PIVOT;			/* Pivot Constraint */
	constraintsTypeInfo[26]= &CTI_FOLLOWTRACK;		/* Follow Track Constraint */
	constraintsTypeInfo[27]= &CTI_CAMERASOLVER;		/* Camera Solver Constraint */
}

/* This function should be used for getting the appropriate type-info when only
 * a constraint type is known
 */
bConstraintTypeInfo *get_constraint_typeinfo (int type)
{
	/* initialise the type-info list? */
	if (CTI_INIT) {
		constraints_init_typeinfo();
		CTI_INIT = 0;
	}
	
	/* only return for valid types */
	if ( (type >= CONSTRAINT_TYPE_NULL) && 
		 (type < NUM_CONSTRAINT_TYPES ) ) 
	{
		/* there shouldn't be any segfaults here... */
		return constraintsTypeInfo[type];
	}
	else {
		printf("No valid constraint type-info data available. Type = %i \n", type);
	}
	
	return NULL;
} 
 
/* This function should always be used to get the appropriate type-info, as it
 * has checks which prevent segfaults in some weird cases.
 */
bConstraintTypeInfo *constraint_get_typeinfo (bConstraint *con)
{
	/* only return typeinfo for valid constraints */
	if (con)
		return get_constraint_typeinfo(con->type);
	else
		return NULL;
}

/* ************************* General Constraints API ************************** */
/* The functions here are called by various parts of Blender. Very few (should be none if possible)
 * constraint-specific code should occur here.
 */
 
/* ---------- Data Management ------- */

/* Free data of a specific constraint if it has any info.
 * be sure to run BIK_clear_data() when freeing an IK constraint,
 * unless DAG_scene_sort is called. */
void free_constraint_data (bConstraint *con)
{
	if (con->data) {
		bConstraintTypeInfo *cti= constraint_get_typeinfo(con);
		
		/* perform any special freeing constraint may have */
		if (cti && cti->free_data)
			cti->free_data(con);
		
		/* free constraint data now */
		MEM_freeN(con->data);
	}
}

/* Free all constraints from a constraint-stack */
void free_constraints (ListBase *list)
{
	bConstraint *con;
	
	/* Free constraint data and also any extra data */
	for (con= list->first; con; con= con->next)
		free_constraint_data(con);
	
	/* Free the whole list */
	BLI_freelistN(list);
}


/* Remove the specified constraint from the given constraint stack */
int remove_constraint (ListBase *list, bConstraint *con)
{
	if (con) {
		free_constraint_data(con);
		BLI_freelinkN(list, con);
		return 1;
	}
	else
		return 0;
}

/* Remove all the constraints of the specified type from the given constraint stack */
void remove_constraints_type (ListBase *list, short type, short last_only)
{
	bConstraint *con, *conp;
	
	if (list == NULL)
		return;
	
	/* remove from the end of the list to make it faster to find the last instance */
	for (con= list->last; con; con= conp) {
		conp= con->prev;
		
		if (con->type == type) {
			remove_constraint(list, con);
			if (last_only) 
				return;
		}
	}
}

/* ......... */

/* Creates a new constraint, initialises its data, and returns it */
static bConstraint *add_new_constraint_internal (const char *name, short type)
{
	bConstraint *con= MEM_callocN(sizeof(bConstraint), "Constraint");
	bConstraintTypeInfo *cti= get_constraint_typeinfo(type);
	const char *newName;

	/* Set up a generic constraint datablock */
	con->type = type;
	con->flag |= CONSTRAINT_EXPAND;
	con->enforce = 1.0f;

	/* Determine a basic name, and info */
	if (cti) {
		/* initialise constraint data */
		con->data = MEM_callocN(cti->size, cti->structName);
		
		/* only constraints that change any settings need this */
		if (cti->new_data)
			cti->new_data(con->data);
		
		/* if no name is provided, use the type of the constraint as the name */
		newName= (name && name[0]) ? name : cti->name;
	}
	else {
		/* if no name is provided, use the generic "Const" name */
		// NOTE: any constraint type that gets here really shouldn't get added...
		newName= (name && name[0]) ? name : "Const";
	}
	
	/* copy the name */
	BLI_strncpy(con->name, newName, sizeof(con->name));
	
	/* return the new constraint */
	return con;
}

/* if pchan is not NULL then assume we're adding a pose constraint */
static bConstraint *add_new_constraint (Object *ob, bPoseChannel *pchan, const char *name, short type)
{
	bConstraint *con;
	ListBase *list;
	
	/* add the constraint */
	con= add_new_constraint_internal(name, type);
	
	/* find the constraint stack - bone or object? */
	list = (pchan) ? (&pchan->constraints) : (&ob->constraints);
	
	if (list) {
		/* add new constraint to end of list of constraints before ensuring that it has a unique name
		 * (otherwise unique-naming code will fail, since it assumes element exists in list)
		 */
		BLI_addtail(list, con);
		unique_constraint_name(con, list);
		
		/* if the target list is a list on some PoseChannel belonging to a proxy-protected
		 * Armature layer, we must tag newly added constraints with a flag which allows them
		 * to persist after proxy syncing has been done
		 */
		if (proxylocked_constraints_owner(ob, pchan))
			con->flag |= CONSTRAINT_PROXY_LOCAL;
		
		/* make this constraint the active one */
		constraints_set_active(list, con);
	}
	
	/* set type+owner specific immutable settings */
	// TODO: does action constraint need anything here - i.e. spaceonce?
	switch (type) {
		case CONSTRAINT_TYPE_CHILDOF:
		{
			/* if this constraint is being added to a posechannel, make sure
			 * the constraint gets evaluated in pose-space */
			if (pchan) {
				con->ownspace = CONSTRAINT_SPACE_POSE;
				con->flag |= CONSTRAINT_SPACEONCE;
			}
		}
			break;
	}
	
	return con;
}

/* ......... */

/* Add new constraint for the given bone */
bConstraint *add_pose_constraint (Object *ob, bPoseChannel *pchan, const char *name, short type)
{
	if (pchan == NULL)
		return NULL;
	
	return add_new_constraint(ob, pchan, name, type);
}

/* Add new constraint for the given object */
bConstraint *add_ob_constraint(Object *ob, const char *name, short type)
{
	return add_new_constraint(ob, NULL, name, type);
}

/* ......... */

/* Reassign links that constraints have to other data (called during file loading?) */
void relink_constraints (ListBase *conlist)
{
	bConstraint *con;
	bConstraintTarget *ct;
	
	for (con= conlist->first; con; con= con->next) {
		bConstraintTypeInfo *cti= constraint_get_typeinfo(con);
		
		if (cti) {
			/* relink any targets */
			if (cti->get_constraint_targets) {
				ListBase targets = {NULL, NULL};
				
				cti->get_constraint_targets(con, &targets);
				for (ct= targets.first; ct; ct= ct->next) {
					ID_NEW(ct->tar);
				}
				
				if (cti->flush_constraint_targets)
					cti->flush_constraint_targets(con, &targets, 0);
			}
			
			/* relink any other special data */
			if (cti->relink_data)
				cti->relink_data(con);
		}
	}
}

/* Run the given callback on all ID-blocks in list of constraints */
void id_loop_constraints (ListBase *conlist, ConstraintIDFunc func, void *userdata)
{
	bConstraint *con;
	
	for (con= conlist->first; con; con= con->next) {
		bConstraintTypeInfo *cti= constraint_get_typeinfo(con);
		
		if (cti) {
			if (cti->id_looper)
				cti->id_looper(con, func, userdata);
		}
	}
}

/* ......... */

/* helper for copy_constraints(), to be used for making sure that ID's are valid */
static void con_extern_cb(bConstraint *UNUSED(con), ID **idpoin, void *UNUSED(userData))
{
	if (*idpoin && (*idpoin)->lib)
		id_lib_extern(*idpoin);
}

/* duplicate all of the constraints in a constraint stack */
void copy_constraints (ListBase *dst, const ListBase *src, int do_extern)
{
	bConstraint *con, *srccon;
	
	dst->first= dst->last= NULL;
	BLI_duplicatelist(dst, src);
	
	for (con=dst->first, srccon=src->first; con && srccon; srccon=srccon->next, con=con->next) {
		bConstraintTypeInfo *cti= constraint_get_typeinfo(con);
		
		/* make a new copy of the constraint's data */
		con->data = MEM_dupallocN(con->data);
		
		/* only do specific constraints if required */
		if (cti) {
			/* perform custom copying operations if needed */
			if (cti->copy_data)
				cti->copy_data(con, srccon);
			
			/* for proxies we dont want to make extern */
			if (do_extern) {
				/* go over used ID-links for this constraint to ensure that they are valid for proxies */
				if (cti->id_looper)
					cti->id_looper(con, con_extern_cb, NULL);
			}
		}
	}
}

/* ......... */

bConstraint *constraints_findByName(ListBase *list, const char *name)
{
	return BLI_findstring(list, name, offsetof(bConstraint, name));
}

/* finds the 'active' constraint in a constraint stack */
bConstraint *constraints_get_active (ListBase *list)
{
	bConstraint *con;
	
	/* search for the first constraint with the 'active' flag set */
	if (list) {
		for (con= list->first; con; con= con->next) {
			if (con->flag & CONSTRAINT_ACTIVE)
				return con;
		}
	}
	
	/* no active constraint found */
	return NULL;
}

/* Set the given constraint as the active one (clearing all the others) */
void constraints_set_active (ListBase *list, bConstraint *con)
{
	bConstraint *c;
	
	if (list) {
		for (c= list->first; c; c= c->next) {
			if (c == con) 
				c->flag |= CONSTRAINT_ACTIVE;
			else 
				c->flag &= ~CONSTRAINT_ACTIVE;
		}
	}
}

/* -------- Constraints and Proxies ------- */

/* Rescue all constraints tagged as being CONSTRAINT_PROXY_LOCAL (i.e. added to bone that's proxy-synced in this file) */
void extract_proxylocal_constraints (ListBase *dst, ListBase *src)
{
	bConstraint *con, *next;
	
	/* for each tagged constraint, remove from src and move to dst */
	for (con= src->first; con; con= next) {
		next= con->next;
		
		/* check if tagged */
		if (con->flag & CONSTRAINT_PROXY_LOCAL) {
			BLI_remlink(src, con);
			BLI_addtail(dst, con);
		}
	}
}

/* Returns if the owner of the constraint is proxy-protected */
short proxylocked_constraints_owner (Object *ob, bPoseChannel *pchan)
{
	/* Currently, constraints can only be on object or bone level */
	if (ob && ob->proxy) {
		if (ob->pose && pchan) {
			bArmature *arm= ob->data;
			
			/* On bone-level, check if bone is on proxy-protected layer */
			if ((pchan->bone) && (pchan->bone->layer & arm->layer_protected))
				return 1;
		}
		else {
			/* FIXME: constraints on object-level are not handled well yet */
			return 1;
		}	
	}
	
	return 0;
}

/* -------- Target-Matrix Stuff ------- */

/* This function is a relic from the prior implementations of the constraints system, when all
 * constraints either had one or no targets. It used to be called during the main constraint solving
 * loop, but is now only used for the remaining cases for a few constraints. 
 *
 * None of the actual calculations of the matrices should be done here! Also, this function is
 * not to be used by any new constraints, particularly any that have multiple targets.
 */
void get_constraint_target_matrix (struct Scene *scene, bConstraint *con, int n, short ownertype, void *ownerdata, float mat[][4], float ctime)
{
	bConstraintTypeInfo *cti= constraint_get_typeinfo(con);
	ListBase targets = {NULL, NULL};
	bConstraintOb *cob;
	bConstraintTarget *ct;
	
	if (cti && cti->get_constraint_targets) {
		/* make 'constraint-ob' */
		cob= MEM_callocN(sizeof(bConstraintOb), "tempConstraintOb");
		cob->type= ownertype;
		cob->scene = scene;
		switch (ownertype) {
			case CONSTRAINT_OBTYPE_OBJECT: /* it is usually this case */
			{
				cob->ob= (Object *)ownerdata;
				cob->pchan= NULL;
				if (cob->ob) {
					copy_m4_m4(cob->matrix, cob->ob->obmat);
					copy_m4_m4(cob->startmat, cob->matrix);
				}
				else {
					unit_m4(cob->matrix);
					unit_m4(cob->startmat);
				}
			}	
				break;
			case CONSTRAINT_OBTYPE_BONE: /* this may occur in some cases */
			{
				cob->ob= NULL; /* this might not work at all :/ */
				cob->pchan= (bPoseChannel *)ownerdata;
				if (cob->pchan) {
					copy_m4_m4(cob->matrix, cob->pchan->pose_mat);
					copy_m4_m4(cob->startmat, cob->matrix);
				}
				else {
					unit_m4(cob->matrix);
					unit_m4(cob->startmat);
				}
			}
				break;
		}
		
		/* get targets - we only need the first one though (and there should only be one) */
		cti->get_constraint_targets(con, &targets);
		
		/* only calculate the target matrix on the first target */
		ct= (bConstraintTarget *)targets.first;
		while(ct && n-- > 0)
			ct= ct->next;

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
void get_constraint_targets_for_solving (bConstraint *con, bConstraintOb *cob, ListBase *targets, float ctime)
{
	bConstraintTypeInfo *cti= constraint_get_typeinfo(con);
	
	if (cti && cti->get_constraint_targets) {
		bConstraintTarget *ct;
		
		/* get targets 
		 * 	- constraints should use ct->matrix, not directly accessing values
		 *	- ct->matrix members have not yet been calculated here! 
		 */
		cti->get_constraint_targets(con, targets);
		
		/* set matrices 
		 * 	- calculate if possible, otherwise just initialise as identity matrix 
		 */
		if (cti->get_target_matrix) {
			for (ct= targets->first; ct; ct= ct->next) 
				cti->get_target_matrix(con, cob, ct, ctime);
		}
		else {
			for (ct= targets->first; ct; ct= ct->next)
				unit_m4(ct->matrix);
		}
	}
}
 
/* ---------- Evaluation ----------- */

/* This function is called whenever constraints need to be evaluated. Currently, all
 * constraints that can be evaluated are everytime this gets run.
 *
 * constraints_make_evalob and constraints_clear_evalob should be called before and 
 * after running this function, to sort out cob
 */
void solve_constraints (ListBase *conlist, bConstraintOb *cob, float ctime)
{
	bConstraint *con;
	float oldmat[4][4];
	float enf;

	/* check that there is a valid constraint object to evaluate */
	if (cob == NULL)
		return;
	
	/* loop over available constraints, solving and blending them */
	for (con= conlist->first; con; con= con->next) {
		bConstraintTypeInfo *cti= constraint_get_typeinfo(con);
		ListBase targets = {NULL, NULL};
		
		/* these we can skip completely (invalid constraints...) */
		if (cti == NULL) continue;
		if (con->flag & (CONSTRAINT_DISABLE|CONSTRAINT_OFF)) continue;
		/* these constraints can't be evaluated anyway */
		if (cti->evaluate_constraint == NULL) continue;
		/* influence == 0 should be ignored */
		if (con->enforce == 0.0f) continue;
		
		/* influence of constraint
		 * 	- value should have been set from animation data already
		 */
		enf = con->enforce;
		
		/* make copy of worldspace matrix pre-constraint for use with blending later */
		copy_m4_m4(oldmat, cob->matrix);
		
		/* move owner matrix into right space */
		constraint_mat_convertspace(cob->ob, cob->pchan, cob->matrix, CONSTRAINT_SPACE_WORLD, con->ownspace);
		
		/* prepare targets for constraint solving */
		get_constraint_targets_for_solving(con, cob, &targets, ctime);
		
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
			constraint_mat_convertspace(cob->ob, cob->pchan, cob->matrix, con->ownspace, CONSTRAINT_SPACE_WORLD);
			
		/* Interpolate the enforcement, to blend result of constraint into final owner transform 
		 * 	- all this happens in worldspace to prevent any weirdness creeping in ([#26014] and [#25725]),
		 *	  since some constraints may not convert the solution back to the input space before blending
		 *	  but all are guaranteed to end up in good "worldspace" result
		 */
		/* Note: all kind of stuff here before (caused trouble), much easier to just interpolate, or did I miss something? -jahka */
		if (enf < 1.0f) {
			float solution[4][4];
			copy_m4_m4(solution, cob->matrix);
			blend_m4_m4m4(cob->matrix, oldmat, solution, enf);
		}
	}
}
