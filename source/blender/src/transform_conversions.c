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
 * Contributor(s): none yet.
 *
 * ***** END GPL/BL DUAL LICENSE BLOCK *****
 */

#ifdef HAVE_CONFIG_H
#include <config.h>
#endif

#ifndef WIN32
#include <unistd.h>
#else
#include <io.h>
#endif
#include <string.h>
#include <math.h>

#include "MEM_guardedalloc.h"

#include "DNA_action_types.h"
#include "DNA_armature_types.h"
#include "DNA_camera_types.h"
#include "DNA_curve_types.h"
#include "DNA_effect_types.h"
#include "DNA_image_types.h"
#include "DNA_ipo_types.h"
#include "DNA_key_types.h"
#include "DNA_lamp_types.h"
#include "DNA_lattice_types.h"
#include "DNA_mesh_types.h"
#include "DNA_meshdata_types.h"
#include "DNA_meta_types.h"
#include "DNA_modifier_types.h"
#include "DNA_object_types.h"
#include "DNA_object_force.h"
#include "DNA_scene_types.h"
#include "DNA_screen_types.h"
#include "DNA_space_types.h"
#include "DNA_texture_types.h"
#include "DNA_view3d_types.h"
#include "DNA_world_types.h"
#include "DNA_userdef_types.h"
#include "DNA_property_types.h"
#include "DNA_vfont_types.h"
#include "DNA_constraint_types.h"
#include "DNA_listBase.h"

#include "BKE_action.h"
#include "BKE_armature.h"
#include "BKE_blender.h"
#include "BKE_curve.h"
#include "BKE_constraint.h"
#include "BKE_depsgraph.h"
#include "BKE_displist.h"
#include "BKE_DerivedMesh.h"
#include "BKE_effect.h"
#include "BKE_font.h"
#include "BKE_global.h"
#include "BKE_ipo.h"
#include "BKE_lattice.h"
#include "BKE_mball.h"
#include "BKE_mesh.h"
#include "BKE_modifier.h"
#include "BKE_object.h"
#include "BKE_softbody.h"
#include "BKE_utildefines.h"

#include "BIF_editaction.h"
#include "BIF_editview.h"
#include "BIF_editlattice.h"
#include "BIF_editconstraint.h"
#include "BIF_editarmature.h"
#include "BIF_editmesh.h"
#include "BIF_editsima.h"
#include "BIF_gl.h"
#include "BIF_poseobject.h"
#include "BIF_meshtools.h"
#include "BIF_mywindow.h"
#include "BIF_resources.h"
#include "BIF_screen.h"
#include "BIF_space.h"
#include "BIF_toolbox.h"

#include "BSE_view.h"
#include "BSE_edit.h"
#include "BSE_editipo.h"
#include "BSE_editipo_types.h"

#include "BDR_editobject.h"		// reset_slowparents()
#include "BDR_unwrapper.h"

#include "BLI_arithb.h"
#include "BLI_blenlib.h"
#include "BLI_editVert.h"

#include "blendef.h"

#include "mydevice.h"

extern ListBase editNurb;
extern ListBase editelems;

#include "transform.h"

/* local function prototype - for Object/Bone Constraints */
static short constraints_list_needinv(ListBase *list);

/* ************************** Functions *************************** */

static void qsort_trans_data(TransInfo *t, TransData *head, TransData *tail) {
	TransData pivot = *head;
	TransData *ihead = head;
	TransData *itail = tail;
	short connected = t->flag & T_PROP_CONNECTED;

	while (head < tail)
	{
		if (connected) {
			while ((tail->dist >= pivot.dist) && (head < tail))
				tail--;
		}
		else {
			while ((tail->rdist >= pivot.rdist) && (head < tail))
				tail--;
		}

		if (head != tail)
		{
			*head = *tail;
			head++;
		}

		if (connected) {
			while ((head->dist <= pivot.dist) && (head < tail))
				head++;
		}
		else {
			while ((head->rdist <= pivot.rdist) && (head < tail))
				head++;
		}

		if (head != tail)
		{
			*tail = *head;
			tail--;
		}
	}

	*head = pivot;
	if (ihead < head) {
		qsort_trans_data(t, ihead, head-1);
	}
	if (itail > head) {
		qsort_trans_data(t, head+1, itail);
	}
}

void sort_trans_data_dist(TransInfo *t) {
	TransData *start = t->data;
	int i = 1;

	while(i < t->total && start->flag & TD_SELECTED) {
		start++;
		i++;
	}
	qsort_trans_data(t, start, t->data + t->total - 1);
}

static void sort_trans_data(TransInfo *t) 
{
	TransData *sel, *unsel;
	TransData temp;
	unsel = t->data;
	sel = t->data;
	sel += t->total - 1;
	while (sel > unsel) {
		while (unsel->flag & TD_SELECTED) {
			unsel++;
			if (unsel == sel) {
				return;
			}
		}
		while (!(sel->flag & TD_SELECTED)) {
			sel--;
			if (unsel == sel) {
				return;
			}
		}
		temp = *unsel;
		*unsel = *sel;
		*sel = temp;
		sel--;
		unsel++;
	}
}

/* distance calculated from not-selected vertex to nearest selected vertex
   warning; this is loops inside loop, has minor N^2 issues, but by sorting list it is OK */
static void set_prop_dist(TransInfo *t, short with_dist)
{
	TransData *tob;
	int a;

	for(a=0, tob= t->data; a<t->total; a++, tob++) {
		
		tob->rdist= 0.0f; // init, it was mallocced
		
		if((tob->flag & TD_SELECTED)==0) {
			TransData *td;
			int i;
			float dist, vec[3];

			tob->rdist = -1.0f; // signal for next loop
				
			for (i = 0, td= t->data; i < t->total; i++, td++) {
				if(td->flag & TD_SELECTED) {
					VecSubf(vec, tob->center, td->center);
					Mat3MulVecfl(tob->mtx, vec);
					dist = Normalize(vec);
					if (tob->rdist == -1.0f) {
						tob->rdist = dist;
					}
					else if (dist < tob->rdist) {
						tob->rdist = dist;
					}
				}
				else break;	// by definition transdata has selected items in beginning
			}
			if (with_dist) {
				tob->dist = tob->rdist;
			}
		}	
	}
}

/* ************************** CONVERSIONS ************************* */

/* ********************* texture space ********* */

static void createTransTexspace(TransInfo *t)
{
	TransData *td;
	Object *ob;
	ID *id;
	
	ob= OBACT;
	
	if (ob==NULL) { // Shouldn't logically happen, but still...
		t->total = 0;
		return;
	}

	id= ob->data;
	if(id==NULL || !ELEM3( GS(id->name), ID_ME, ID_CU, ID_MB )) {
		t->total = 0;
		return;
	}

	t->total = 1;
	td= t->data= MEM_callocN(sizeof(TransData), "TransTexspace");
	td->ext= t->ext= MEM_callocN(sizeof(TransDataExtension), "TransTexspace");
	
	td->flag= TD_SELECTED;
	VECCOPY(td->center, ob->obmat[3]);
	td->ob = ob;
	
	Mat3CpyMat4(td->mtx, ob->obmat);
	Mat3CpyMat4(td->axismtx, ob->obmat);
	Mat3Ortho(td->axismtx);
	Mat3Inv(td->smtx, td->mtx);
	
	if( GS(id->name)==ID_ME) {
		Mesh *me= ob->data;
		me->texflag &= ~AUTOSPACE;
		td->loc= me->loc;
		td->ext->rot= me->rot;
		td->ext->size= me->size;
	}
	else if( GS(id->name)==ID_CU) {
		Curve *cu= ob->data;
		cu->texflag &= ~CU_AUTOSPACE;
		td->loc= cu->loc;
		td->ext->rot= cu->rot;
		td->ext->size= cu->size;
	}
	else if( GS(id->name)==ID_MB) {
		MetaBall *mb= ob->data;
		mb->texflag &= ~MB_AUTOSPACE;
		td->loc= mb->loc;
		td->ext->rot= mb->rot;
		td->ext->size= mb->size;
	}
	
	VECCOPY(td->iloc, td->loc);
	VECCOPY(td->ext->irot, td->ext->rot);
	VECCOPY(td->ext->isize, td->ext->size);
}

/* ********************* edge (for crease) ***** */

static void createTransEdge(TransInfo *t) {
	TransData *td = NULL;
	EditMesh *em = G.editMesh;
	EditEdge *eed;
	float mtx[3][3], smtx[3][3];
	int count=0, countsel=0;
	int propmode = t->flag & T_PROP_EDIT;

	for(eed= em->edges.first; eed; eed= eed->next) {
		if(eed->h==0) {
			if (eed->f & SELECT) countsel++;
			if (propmode) count++;
		}
	}

	if (countsel == 0)
		return;

	if(propmode) {
		t->total = count;
	}
	else {
		t->total = countsel;
	}

	td= t->data= MEM_callocN(t->total * sizeof(TransData), "TransCrease");

	Mat3CpyMat4(mtx, G.obedit->obmat);
	Mat3Inv(smtx, mtx);

	for(eed= em->edges.first; eed; eed= eed->next) {
		if(eed->h==0 && (eed->f & SELECT || propmode)) {
			/* need to set center for center calculations */
			VecAddf(td->center, eed->v1->co, eed->v2->co);
			VecMulf(td->center, 0.5f);

			td->loc= NULL;
			if (eed->f & SELECT)
				td->flag= TD_SELECTED;
			else 
				td->flag= 0;


			Mat3CpyMat3(td->smtx, smtx);
			Mat3CpyMat3(td->mtx, mtx);

			td->ext = NULL;
			td->tdi = NULL;
			td->val = &(eed->crease);
			td->ival = eed->crease;

			td++;
		}
	}
}

/* ********************* pose mode ************* */

static bKinematicConstraint *has_targetless_ik(bPoseChannel *pchan)
{
	bConstraint *con= pchan->constraints.first;
	
	for(;con; con= con->next) {
		if(con->type==CONSTRAINT_TYPE_KINEMATIC) {
			bKinematicConstraint *data= con->data;
			
			if(data->tar==NULL) 
				return data;
			if(data->tar->type==OB_ARMATURE && data->subtarget[0]==0) 
				return data;
		}
	}
	return NULL;
}

static short apply_targetless_ik(Object *ob)
{
	bPoseChannel *pchan, *parchan, *chanlist[256];
	bKinematicConstraint *data;
	int segcount, apply= 0;
	
	/* now we got a difficult situation... we have to find the
	   target-less IK pchans, and apply transformation to the all 
	   pchans that were in the chain */
	
	for (pchan=ob->pose->chanbase.first; pchan; pchan=pchan->next) {
		data= has_targetless_ik(pchan);
		if(data && (data->flag & CONSTRAINT_IK_AUTO)) {
			
			/* fill the array with the bones of the chain (armature.c does same, keep it synced) */
			segcount= 0;
			
			/* exclude tip from chain? */
			if(!(data->flag & CONSTRAINT_IK_TIP))
				parchan= pchan->parent;
			else
				parchan= pchan;
			
			/* Find the chain's root & count the segments needed */
			for (; parchan; parchan=parchan->parent){
				chanlist[segcount]= parchan;
				segcount++;
				
				if(segcount==data->rootbone || segcount>255) break; // 255 is weak
			}
			for(;segcount;segcount--) {
				Bone *bone;
				float rmat[4][4], tmat[4][4], imat[4][4];
				
				/* pose_mat(b) = pose_mat(b-1) * offs_bone * channel * constraint * IK  */
				/* we put in channel the entire result of rmat= (channel * constraint * IK) */
				/* pose_mat(b) = pose_mat(b-1) * offs_bone * rmat  */
				/* rmat = pose_mat(b) * inv( pose_mat(b-1) * offs_bone ) */
				
				parchan= chanlist[segcount-1];
				bone= parchan->bone;
				bone->flag |= BONE_TRANSFORM;	/* ensures it gets an auto key inserted */
				
				if(parchan->parent) {
					Bone *parbone= parchan->parent->bone;
					float offs_bone[4][4];
					
					/* offs_bone =  yoffs(b-1) + root(b) + bonemat(b) */
					Mat4CpyMat3(offs_bone, bone->bone_mat);
					
					/* The bone's root offset (is in the parent's coordinate system) */
					VECCOPY(offs_bone[3], bone->head);
					
					/* Get the length translation of parent (length along y axis) */
					offs_bone[3][1]+= parbone->length;
					
					/* pose_mat(b-1) * offs_bone */
					if(parchan->bone->flag & BONE_HINGE) {
						/* the rotation of the parent restposition */
						Mat4CpyMat4(rmat, parbone->arm_mat);	/* rmat used as temp */
						
						/* the location of actual parent transform */
						VECCOPY(rmat[3], offs_bone[3]);
						offs_bone[3][0]= offs_bone[3][1]= offs_bone[3][2]= 0.0f;
						Mat4MulVecfl(parchan->parent->pose_mat, rmat[3]);
						
						Mat4MulMat4(tmat, offs_bone, rmat);
					}
					else
						Mat4MulMat4(tmat, offs_bone, parchan->parent->pose_mat);
					
					Mat4Invert(imat, tmat);
				}
				else {
					Mat4CpyMat3(tmat, bone->bone_mat);

					VECCOPY(tmat[3], bone->head);
					Mat4Invert(imat, tmat);
				}
				/* result matrix */
				Mat4MulMat4(rmat, parchan->pose_mat, imat);
				
				/* apply and decompose, doesn't work for constraints or non-uniform scale well */
				{
					float rmat3[3][3], qmat[3][3], imat[3][3], smat[3][3];
					
					Mat3CpyMat4(rmat3, rmat);
					
					/* quaternion */
					Mat3ToQuat(rmat3, parchan->quat);
					
					/* for size, remove rotation */
					QuatToMat3(parchan->quat, qmat);
					Mat3Inv(imat, qmat);
					Mat3MulMat3(smat, rmat3, imat);
					Mat3ToSize(smat, parchan->size);
					
					VECCOPY(parchan->loc, rmat[3]);
				}
				
			}
			
			apply= 1;
			data->flag &= ~CONSTRAINT_IK_AUTO;
		}
	}		
	
	return apply;
}

static void add_pose_transdata(TransInfo *t, bPoseChannel *pchan, Object *ob, TransData *td)
{
	Bone *bone= pchan->bone;
	float pmat[3][3], omat[3][3];
	float cmat[3][3], tmat[3][3];
	float vec[3];

	VECCOPY(vec, pchan->pose_mat[3]);
	VECCOPY(td->center, vec);
	
	td->ob = ob;
	td->flag= TD_SELECTED|TD_USEQUAT;
	td->protectflag= pchan->protectflag;
	
	td->loc = pchan->loc;
	VECCOPY(td->iloc, pchan->loc);
	
	td->ext->rot= NULL;
	td->ext->quat= pchan->quat;
	td->ext->size= pchan->size;

	QUATCOPY(td->ext->iquat, pchan->quat);
	VECCOPY(td->ext->isize, pchan->size);

	/* proper way to get parent transform + own transform + constraints transform */
	Mat3CpyMat4(omat, ob->obmat);
	
	if(pchan->parent) { 	 
		if(pchan->bone->flag & BONE_HINGE) 	 
			Mat3CpyMat4(pmat, pchan->parent->bone->arm_mat); 	 
		else 	 
			Mat3CpyMat4(pmat, pchan->parent->pose_mat);
		
		if (constraints_list_needinv(&pchan->constraints)) {
			Mat3CpyMat4(tmat, pchan->constinv);
			Mat3Inv(cmat, tmat);
			Mat3MulSerie(td->mtx, pchan->bone->bone_mat, pmat, cmat, omat, 0,0,0,0);    // dang mulserie swaps args
		}
		else
			Mat3MulSerie(td->mtx, pchan->bone->bone_mat, pmat, omat, 0,0,0,0,0);    // dang mulserie swaps args
	}
	else {
		if (constraints_list_needinv(&pchan->constraints)) {
			Mat3CpyMat4(tmat, pchan->constinv);
			Mat3Inv(cmat, tmat);
			Mat3MulSerie(td->mtx, pchan->bone->bone_mat, cmat, omat, 0, 0,0,0,0);    // dang mulserie swaps args
		}
		else 
			Mat3MulMat3(td->mtx, omat, pchan->bone->bone_mat);  // Mat3MulMat3 has swapped args! 
	}
	
	Mat3Inv(td->smtx, td->mtx);
	
	/* for axismat we use bone's own transform */
	Mat3CpyMat4(pmat, pchan->pose_mat);
	Mat3MulMat3(td->axismtx, omat, pmat);
	Mat3Ortho(td->axismtx);
	
	if(t->mode==TFM_BONESIZE) {
		bArmature *arm= t->poseobj->data;
		
		if(arm->drawtype==ARM_ENVELOPE) {
			td->loc= NULL;
			td->val= &bone->dist;
			td->ival= bone->dist;
		}
		else {
			// abusive storage of scale in the loc pointer :)
			td->loc= &bone->xwidth;
			VECCOPY (td->iloc, td->loc);
			td->val= NULL;
		}
	}
	
	/* in this case we can do target-less IK grabbing */
	if(t->mode==TFM_TRANSLATION) {
		bKinematicConstraint *data= has_targetless_ik(pchan);
		if(data) {
			if(data->flag & CONSTRAINT_IK_TIP) {
				VECCOPY(data->grabtarget, pchan->pose_tail);
			}
			else {
				VECCOPY(data->grabtarget, pchan->pose_head);
			}
			td->loc = data->grabtarget;
			VECCOPY(td->iloc, td->loc);
			data->flag |= CONSTRAINT_IK_AUTO;
			
			/* only object matrix correction */
			Mat3CpyMat3 (td->mtx, omat);
			Mat3Inv (td->smtx, td->mtx);
		}
	}
}

static void bone_children_clear_transflag(ListBase *lb)
{
	Bone *bone= lb->first;
	
	for(;bone;bone= bone->next) {
		bone->flag &= ~BONE_TRANSFORM;
		bone_children_clear_transflag(&bone->childbase);
	}
}

/* sets transform flags in the bones, returns total */
static void set_pose_transflags(TransInfo *t, Object *ob)
{
	bArmature *arm= ob->data;
	bPoseChannel *pchan;
	Bone *bone;
	
	t->total= 0;
	
	for(pchan= ob->pose->chanbase.first; pchan; pchan= pchan->next) {
		bone= pchan->bone;
		if(bone->layer & arm->layer) {
			if(bone->flag & BONE_SELECTED)
				bone->flag |= BONE_TRANSFORM;
			else
				bone->flag &= ~BONE_TRANSFORM;
		}
	}
	
	/* make sure no bone can be transformed when a parent is transformed */
	/* since pchans are depsgraph sorted, the parents are in beginning of list */
	if(t->mode!=TFM_BONESIZE) {
		for(pchan= ob->pose->chanbase.first; pchan; pchan= pchan->next) {
			bone= pchan->bone;
			if(bone->flag & BONE_TRANSFORM)
				bone_children_clear_transflag(&bone->childbase);
		}
	}	
	/* now count, and check if we have autoIK or have to switch from translate to rotate */
	for(pchan= ob->pose->chanbase.first; pchan; pchan= pchan->next) {
		bone= pchan->bone;
		if(bone->flag & BONE_TRANSFORM) {
			t->total++;
			
			if(t->mode==TFM_TRANSLATION) {
				if( has_targetless_ik(pchan)==NULL ) {
					if(pchan->parent && (pchan->bone->flag & BONE_CONNECTED))
						t->mode= TFM_ROTATION;
					else if((pchan->protectflag & OB_LOCK_LOC)==OB_LOCK_LOC)
						t->mode= TFM_ROTATION;
				}
			}
		}
	}
}

/* frees temporal IKs */
static void pose_grab_with_ik_clear(Object *ob)
{
	bKinematicConstraint *data;
	bPoseChannel *pchan;
	bConstraint *con;
	
	for(pchan= ob->pose->chanbase.first; pchan; pchan= pchan->next) {
		for(con= pchan->constraints.first; con; con= con->next) {
			if(con->type==CONSTRAINT_TYPE_KINEMATIC) {
				data= con->data;
				if(data->flag & CONSTRAINT_IK_TEMP) {
					BLI_remlink(&pchan->constraints, con);
					MEM_freeN(con->data);
					MEM_freeN(con);
					pchan->constflag &= ~(PCHAN_HAS_IK|PCHAN_HAS_TARGET);
					break;
				}
			}
		}
	}
}

/* adds the IK to pchan */
static void pose_grab_with_ik_add(bPoseChannel *pchan)
{
	bKinematicConstraint *data;
	bConstraint *con;
	
	if (pchan == NULL) { // Sanity check
		return;
	}
	
	/* rule: not if there's already an IK on this channel */
	for(con= pchan->constraints.first; con; con= con->next)
		if(con->type==CONSTRAINT_TYPE_KINEMATIC)
			break;
	
	if(con) {
		/* but, if this is a targetless IK, we make it auto anyway (for the children loop) */
		data= has_targetless_ik(pchan);
		if(data)
			data->flag |= CONSTRAINT_IK_AUTO;
		return;
	}
	
	con = add_new_constraint(CONSTRAINT_TYPE_KINEMATIC);
	BLI_addtail(&pchan->constraints, con);
	pchan->constflag |= (PCHAN_HAS_IK|PCHAN_HAS_TARGET);	/* for draw, but also for detecting while pose solving */
	data= con->data;
	data->flag= CONSTRAINT_IK_TIP|CONSTRAINT_IK_TEMP|CONSTRAINT_IK_AUTO;
	VECCOPY(data->grabtarget, pchan->pose_tail);
	data->rootbone= 1;
	
	/* we include only a connected chain */
	while(pchan && (pchan->bone->flag & BONE_CONNECTED)) {
		data->rootbone++;
		pchan= pchan->parent;
	}
}

/* bone is a canditate to get IK, but we don't do it if it has children connected */
static void pose_grab_with_ik_children(bPose *pose, Bone *bone)
{
	Bone *bonec;
	int wentdeeper= 0;

	/* go deeper if children & children are connected */
	for(bonec= bone->childbase.first; bonec; bonec= bonec->next) {
		if(bonec->flag & BONE_CONNECTED) {
			wentdeeper= 1;
			pose_grab_with_ik_children(pose, bonec);
		}
	}
	if(wentdeeper==0) {
		bPoseChannel *pchan= get_pose_channel(pose, bone->name);
		if(pchan)
			pose_grab_with_ik_add(pchan);
	}
}

/* main call which adds temporal IK chains */
static void pose_grab_with_ik(Object *ob)
{
	bArmature *arm;
	bPoseChannel *pchan, *pchansel= NULL;
	
	if(ob==NULL || ob->pose==NULL || (ob->flag & OB_POSEMODE)==0)
		return;
		
	arm = ob->data;
	
	/* rule: only one Bone */
	for(pchan= ob->pose->chanbase.first; pchan; pchan= pchan->next) {
		if(pchan->bone->layer & arm->layer) {
			if(pchan->bone->flag & BONE_SELECTED) {
				if(pchansel)
					break;
				pchansel= pchan;
			}
		}
	}
	if(pchan || pchansel==NULL) return;
	
	/* rule: if selected Bone is not a root bone, it gets a temporal IK */
	if(pchansel->parent) {
		/* only adds if there's no IK yet */
		pose_grab_with_ik_add(pchansel);
	}
	else {
		/* rule: go over the children and add IK to the tips */
		pose_grab_with_ik_children(ob->pose, pchansel->bone);
	}
}	



/* only called with pose mode active object now */
static void createTransPose(TransInfo *t, Object *ob)
{
	bArmature *arm;
	bPoseChannel *pchan;
	TransData *td;
	TransDataExtension *tdx;
	int i;
	
	t->total= 0;
	
	/* check validity of state */
	arm=get_armature (ob);
	if (arm==NULL || ob->pose==NULL) return;
	
	if (arm->flag & ARM_RESTPOS) {
		if(t->mode!=TFM_BONESIZE) {
			notice ("Pose edit not possible while Rest Position is enabled");
			return;
		}
	}
	if (!(ob->lay & G.vd->lay)) return;

	/* do we need to add temporal IK chains? */
	if((arm->flag & ARM_AUTO_IK) && t->mode==TFM_TRANSLATION)
		pose_grab_with_ik(ob);
	
	/* set flags and count total (warning, can change transform to rotate) */
	set_pose_transflags(t, ob);
	
	if(t->total==0) return;

	t->flag |= T_POSE;
	t->poseobj= ob;	/* we also allow non-active objects to be transformed, in weightpaint */
	
	/* make sure the lock is set OK, unlock can be accidentally saved? */
	ob->pose->flag |= POSE_LOCKED;
	ob->pose->flag &= ~POSE_DO_UNLOCK;

	/* init trans data */
    td = t->data = MEM_callocN(t->total*sizeof(TransData), "TransPoseBone");
    tdx = t->ext = MEM_callocN(t->total*sizeof(TransDataExtension), "TransPoseBoneExt");
	for(i=0; i<t->total; i++, td++, tdx++) {
		td->ext= tdx;
		td->tdi = NULL;
		td->val = NULL;
	}	
	
	/* use pose channels to fill trans data */
	td= t->data;
	for(pchan= ob->pose->chanbase.first; pchan; pchan= pchan->next) {
		if(pchan->bone->flag & BONE_TRANSFORM) {
			add_pose_transdata(t, pchan, ob, td);
			td++;
		}
	}
	
	if(td != (t->data+t->total)) printf("Bone selection count error\n");
	
}

/* ********************* armature ************** */

static void createTransArmatureVerts(TransInfo *t)
{
	EditBone *ebo;
	bArmature *arm= G.obedit->data;
	TransData *td;
	float mtx[3][3], smtx[3][3], delta[3], bonemat[3][3];

	t->total = 0;
	for (ebo=G.edbo.first;ebo;ebo=ebo->next) {
		if(ebo->layer & arm->layer) {
			if (t->mode==TFM_BONESIZE) {
				if (ebo->flag & BONE_SELECTED)
					t->total++;
			}
			else if (t->mode==TFM_BONE_ROLL) {
				if (ebo->flag & BONE_SELECTED)
					t->total++;
			}
			else {
				if (ebo->flag & BONE_TIPSEL)
					t->total++;
				if (ebo->flag & BONE_ROOTSEL)
					t->total++;
			}
		}
	}

    if (!t->total) return;
	
	Mat3CpyMat4(mtx, G.obedit->obmat);
	Mat3Inv(smtx, mtx);

    td = t->data = MEM_callocN(t->total*sizeof(TransData), "TransEditBone");
	
	for (ebo=G.edbo.first;ebo;ebo=ebo->next){
		ebo->oldlength= ebo->length;	// length==0.0 on extrude, used for scaling radius of bone points
		
		if(ebo->layer & arm->layer) {
			if (t->mode==TFM_BONE_ENVELOPE) {
				
				if (ebo->flag & BONE_ROOTSEL){
					td->val= &ebo->rad_head;
					td->ival= *td->val;
					
					VECCOPY (td->center, ebo->head);
					td->flag= TD_SELECTED;
					
					Mat3CpyMat3(td->smtx, smtx);
					Mat3CpyMat3(td->mtx, mtx);
					
					td->loc = NULL;
					td->ext = NULL;
					td->tdi = NULL;
					
					td++;
				}
				if (ebo->flag & BONE_TIPSEL){
					td->val= &ebo->rad_tail;
					td->ival= *td->val;
					VECCOPY (td->center, ebo->tail);
					td->flag= TD_SELECTED;
					
					Mat3CpyMat3(td->smtx, smtx);
					Mat3CpyMat3(td->mtx, mtx);
					
					td->loc = NULL;
					td->ext = NULL;
					td->tdi = NULL;
					
					td++;
				}
				
			}
			else if (t->mode==TFM_BONESIZE) {
				if (ebo->flag & BONE_SELECTED) {
					if(arm->drawtype==ARM_ENVELOPE) {
						td->loc= NULL;
						td->val= &ebo->dist;
						td->ival= ebo->dist;
					}
					else {
						// abusive storage of scale in the loc pointer :)
						td->loc= &ebo->xwidth;
						VECCOPY (td->iloc, td->loc);
						td->val= NULL;
					}
					VECCOPY (td->center, ebo->head);
					td->flag= TD_SELECTED;
					
					/* use local bone matrix */
					VecSubf(delta, ebo->tail, ebo->head);	
					vec_roll_to_mat3(delta, ebo->roll, bonemat);
					Mat3MulMat3(td->mtx, mtx, bonemat);
					Mat3Inv(td->smtx, td->mtx);
					
					Mat3CpyMat3(td->axismtx, td->mtx);
					Mat3Ortho(td->axismtx);

					td->ext = NULL;
					td->tdi = NULL;
					
					td++;
				}
			}
			else if (t->mode==TFM_BONE_ROLL) {
				if (ebo->flag & BONE_SELECTED) {
					td->loc= NULL;
					td->val= &(ebo->roll);
					td->ival= ebo->roll;
					
					VECCOPY (td->center, ebo->head);
					td->flag= TD_SELECTED;

					td->ext = NULL;
					td->tdi = NULL;
					
					td++;
				}
			}
			else {
				if (ebo->flag & BONE_TIPSEL){
					VECCOPY (td->iloc, ebo->tail);
					VECCOPY (td->center, td->iloc);
					td->loc= ebo->tail;
					td->flag= TD_SELECTED;

					Mat3CpyMat3(td->smtx, smtx);
					Mat3CpyMat3(td->mtx, mtx);

					td->ext = NULL;
					td->tdi = NULL;
					td->val = NULL;

					td++;
				}
				if (ebo->flag & BONE_ROOTSEL){
					VECCOPY (td->iloc, ebo->head);
					VECCOPY (td->center, td->iloc);
					td->loc= ebo->head;
					td->flag= TD_SELECTED;

					Mat3CpyMat3(td->smtx, smtx);
					Mat3CpyMat3(td->mtx, mtx);

					td->ext = NULL;
					td->tdi = NULL;
					td->val = NULL;

					td++;
				}
			}
		}
	}
}

/* ********************* meta elements ********* */

static void createTransMBallVerts(TransInfo *t)
{
 	MetaElem *ml;
	TransData *td;
	TransDataExtension *tx;
	float mtx[3][3], smtx[3][3];
	int count=0, countsel=0;
	int propmode = t->flag & T_PROP_EDIT;

	/* count totals */
	for(ml= editelems.first; ml; ml= ml->next) {
		if(ml->flag & SELECT) countsel++;
		if(propmode) count++;
	}

	/* note: in prop mode we need at least 1 selected */
	if (countsel==0) return;
	
	if(propmode) t->total = count; 
	else t->total = countsel;
	
	td = t->data= MEM_callocN(t->total*sizeof(TransData), "TransObData(MBall EditMode)");
	tx = t->ext = MEM_callocN(t->total*sizeof(TransDataExtension), "MetaElement_TransExtension");

	Mat3CpyMat4(mtx, G.obedit->obmat);
	Mat3Inv(smtx, mtx);
    
	for(ml= editelems.first; ml; ml= ml->next) {
		if(propmode || (ml->flag & SELECT)) {
			td->loc= &ml->x;
			VECCOPY(td->iloc, td->loc);
			VECCOPY(td->center, td->loc);

			if(ml->flag & SELECT) td->flag= TD_SELECTED | TD_USEQUAT | TD_SINGLESIZE;
			else td->flag= TD_USEQUAT;

			Mat3CpyMat3(td->smtx, smtx);
			Mat3CpyMat3(td->mtx, mtx);

			td->ext = tx;
			td->tdi = NULL;

			/* Radius of MetaElem (mass of MetaElem influence) */
			if(ml->flag & MB_SCALE_RAD){
				td->val = &ml->rad;
				td->ival = ml->rad;
			}
			else{
				td->val = &ml->s;
				td->ival = ml->s;
			}

			/* expx/expy/expz determine "shape" of some MetaElem types */
			tx->size = &ml->expx;
			tx->isize[0] = ml->expx;
			tx->isize[1] = ml->expy;
			tx->isize[2] = ml->expz;

			/* quat is used for rotation of MetaElem */
			tx->quat = ml->quat;
			QUATCOPY(tx->iquat, ml->quat);

			tx->rot = NULL;

			td++;
			tx++;
		}
	}
} 

/* ********************* curve/surface ********* */

static void calc_distanceCurveVerts(TransData *head, TransData *tail) {
	TransData *td, *td_near = NULL;
	for (td = head; td<=tail; td++) {
		if (td->flag & TD_SELECTED) {
			td_near = td;
			td->dist = 0.0f;
		}
		else if(td_near) {
			float dist;
			dist = VecLenf(td_near->center, td->center);
			if (dist < (td-1)->dist) {
				td->dist = (td-1)->dist;
			}
			else {
				td->dist = dist;
			}
		}
		else {
			td->dist = MAXFLOAT;
			td->flag |= TD_NOTCONNECTED;
		}
	}
	td_near = NULL;
	for (td = tail; td>=head; td--) {
		if (td->flag & TD_SELECTED) {
			td_near = td;
			td->dist = 0.0f;
		}
		else if(td_near) {
			float dist;
			dist = VecLenf(td_near->center, td->center);
			if (td->flag & TD_NOTCONNECTED || dist < td->dist || (td+1)->dist < td->dist) {
				td->flag &= ~TD_NOTCONNECTED;
				if (dist < (td+1)->dist) {
					td->dist = (td+1)->dist;
				}
				else {
					td->dist = dist;
				}
			}
		}
	}
}

static void createTransCurveVerts(TransInfo *t)
{
	TransData *td = NULL;
  	Nurb *nu;
	BezTriple *bezt;
	BPoint *bp;
	float mtx[3][3], smtx[3][3];
	int a;
	int count=0, countsel=0;
	int propmode = t->flag & T_PROP_EDIT;

	/* count total of vertices, check identical as in 2nd loop for making transdata! */
	for(nu= editNurb.first; nu; nu= nu->next) {
		if((nu->type & 7)==CU_BEZIER) {
			for(a=0, bezt= nu->bezt; a<nu->pntsu; a++, bezt++) {
				if(bezt->hide==0) {
					if(bezt->f1 & 1) countsel++;
					if(bezt->f2 & 1) countsel++;
					if(bezt->f3 & 1) countsel++;
					if(propmode) count+= 3;
				}
			}
		}
		else {
			for(a= nu->pntsu*nu->pntsv, bp= nu->bp; a>0; a--, bp++) {
				if(bp->hide==0) {
					if(propmode) count++;
					if(bp->f1 & 1) countsel++;
				}
			}
		}
	}
	/* note: in prop mode we need at least 1 selected */
	if (countsel==0) return;
	
	if(propmode) t->total = count; 
	else t->total = countsel;
	t->data= MEM_callocN(t->total*sizeof(TransData), "TransObData(Curve EditMode)");

	Mat3CpyMat4(mtx, G.obedit->obmat);
	Mat3Inv(smtx, mtx);

    td = t->data;
	for(nu= editNurb.first; nu; nu= nu->next) {
		if((nu->type & 7)==CU_BEZIER) {
			TransData *head, *tail;
			head = tail = td;
			for(a=0, bezt= nu->bezt; a<nu->pntsu; a++, bezt++) {
				if(bezt->hide==0) {
					if(propmode || (bezt->f1 & 1)) {
						VECCOPY(td->iloc, bezt->vec[0]);
						td->loc= bezt->vec[0];
						VECCOPY(td->center, bezt->vec[1]);
						if(bezt->f1 & 1) td->flag= TD_SELECTED;
						else td->flag= 0;
						td->ext = NULL;
						td->tdi = NULL;
						td->val = NULL;

						Mat3CpyMat3(td->smtx, smtx);
						Mat3CpyMat3(td->mtx, mtx);

						td++;
						count++;
						tail++;
					}
					/* THIS IS THE CV, the other two are handles */
					if(propmode || (bezt->f2 & 1)) {
						VECCOPY(td->iloc, bezt->vec[1]);
						td->loc= bezt->vec[1];
						VECCOPY(td->center, td->loc);
						if(bezt->f2 & 1) td->flag= TD_SELECTED;
						else td->flag= 0;
						td->ext = NULL;
						td->tdi = NULL;
						
						if (t->mode==TFM_CURVE_SHRINKFATTEN) {
							td->val = &(bezt->radius);
							td->ival = bezt->radius;
						} else {
							td->val = &(bezt->alfa);
							td->ival = bezt->alfa;
						}

						Mat3CpyMat3(td->smtx, smtx);
						Mat3CpyMat3(td->mtx, mtx);

						td++;
						count++;
						tail++;
					}
					if(propmode || (bezt->f3 & 1)) {
						VECCOPY(td->iloc, bezt->vec[2]);
						td->loc= bezt->vec[2];
						VECCOPY(td->center, bezt->vec[1]);
						if(bezt->f3 & 1) td->flag= TD_SELECTED;
						else td->flag= 0;
						td->ext = NULL;
						td->tdi = NULL;
						td->val = NULL;

						Mat3CpyMat3(td->smtx, smtx);
						Mat3CpyMat3(td->mtx, mtx);

						td++;
						count++;
						tail++;
					}
				}
				else if (propmode && head != tail) {
					calc_distanceCurveVerts(head, tail-1);
					head = tail;
				}
			}
			if (propmode && head != tail)
				calc_distanceCurveVerts(head, tail-1);
		}
		else {
			TransData *head, *tail;
			head = tail = td;
			for(a= nu->pntsu*nu->pntsv, bp= nu->bp; a>0; a--, bp++) {
				if(bp->hide==0) {
					if(propmode || (bp->f1 & 1)) {
						VECCOPY(td->iloc, bp->vec);
						td->loc= bp->vec;
						VECCOPY(td->center, td->loc);
						if(bp->f1 & 1) td->flag= TD_SELECTED;
						else td->flag= 0;
						td->ext = NULL;
						td->tdi = NULL;
						
						if (t->mode==TFM_CURVE_SHRINKFATTEN) {
							td->val = &(bp->radius);
							td->ival = bp->radius;
						} else {
							td->val = &(bp->alfa);
							td->ival = bp->alfa;
						}

						Mat3CpyMat3(td->smtx, smtx);
						Mat3CpyMat3(td->mtx, mtx);

						td++;
						count++;
						tail++;
					}
				}
				else if (propmode && head != tail) {
					calc_distanceCurveVerts(head, tail-1);
					head = tail;
				}
			}
			if (propmode && head != tail)
				calc_distanceCurveVerts(head, tail-1);
		}
	}
}

/* ********************* lattice *************** */

static void createTransLatticeVerts(TransInfo *t)
{
	TransData *td = NULL;
	BPoint *bp;
	float mtx[3][3], smtx[3][3];
	int a;
	int count=0, countsel=0;
	int propmode = t->flag & T_PROP_EDIT;

	bp= editLatt->def;
	a= editLatt->pntsu*editLatt->pntsv*editLatt->pntsw;
	while(a--) {
		if(bp->hide==0) {
			if(bp->f1 & 1) countsel++;
			if(propmode) count++;
		}
		bp++;
	}
	
 	/* note: in prop mode we need at least 1 selected */
	if (countsel==0) return;
	
	if(propmode) t->total = count; 
	else t->total = countsel;
	t->data= MEM_callocN(t->total*sizeof(TransData), "TransObData(Lattice EditMode)");
	
	Mat3CpyMat4(mtx, G.obedit->obmat);
	Mat3Inv(smtx, mtx);

	td = t->data;
	bp= editLatt->def;
	a= editLatt->pntsu*editLatt->pntsv*editLatt->pntsw;
	while(a--) {
		if(propmode || (bp->f1 & 1)) {
			if(bp->hide==0) {
				VECCOPY(td->iloc, bp->vec);
				td->loc= bp->vec;
				VECCOPY(td->center, td->loc);
				if(bp->f1 & 1) td->flag= TD_SELECTED;
				else td->flag= 0;
				Mat3CpyMat3(td->smtx, smtx);
				Mat3CpyMat3(td->mtx, mtx);

				td->ext = NULL;
				td->tdi = NULL;
				td->val = NULL;

				td++;
				count++;
			}
		}
		bp++;
	}
} 

/* ********************* mesh ****************** */

/* proportional distance based on connectivity  */
#define E_VEC(a)	(vectors + (3 * (a)->tmp.l))
#define E_NEAR(a)	(nears[((a)->tmp.l)])
#define THRESHOLD	0.0001f
static void editmesh_set_connectivity_distance(int total, float *vectors, EditVert **nears)
{
	EditMesh *em = G.editMesh;
	EditVert *eve;
	EditEdge *eed;
	int i= 0, done= 1;

	/* f2 flag is used for 'selection' */
	/* tmp.l is offset on scratch array   */
	for(eve= em->verts.first; eve; eve= eve->next) {
		if(eve->h==0) {
			eve->tmp.l = i++;

			if(eve->f & SELECT) {
				eve->f2= 2;
				E_NEAR(eve) = eve;
				E_VEC(eve)[0] = 0.0f;
				E_VEC(eve)[1] = 0.0f;
				E_VEC(eve)[2] = 0.0f;
			}
			else {
				eve->f2 = 0;
			}
		}
	}


	/* Floodfill routine */
	/*
	At worst this is n*n of complexity where n is number of edges 
	Best case would be n if the list is ordered perfectly.
	Estimate is n log n in average (so not too bad)
	*/
	while(done) {
		done= 0;
		
		for(eed= em->edges.first; eed; eed= eed->next) {
			if(eed->h==0) {
				EditVert *v1= eed->v1, *v2= eed->v2;
				float *vec2 = E_VEC(v2);
				float *vec1 = E_VEC(v1);

				if (v1->f2 + v2->f2 == 4)
					continue;

				if (v1->f2) {
					if (v2->f2) {
						float nvec[3];
						float len1 = VecLength(vec1);
						float len2 = VecLength(vec2);
						float lenn;
						/* for v2 if not selected */
						if (v2->f2 != 2) {
							VecSubf(nvec, v2->co, E_NEAR(v1)->co);
							lenn = VecLength(nvec);
							/* 1 < n < 2 */
							if (lenn - len1 > THRESHOLD && len2 - lenn > THRESHOLD) {
								VECCOPY(vec2, nvec);
								E_NEAR(v2) = E_NEAR(v1);
								done = 1;
							}
							/* n < 1 < 2 */
							else if (len2 - len1 > THRESHOLD && len1 - lenn > THRESHOLD) {
								VECCOPY(vec2, vec1);
								E_NEAR(v2) = E_NEAR(v1);
								done = 1;
							}
						}
						/* for v1 if not selected */
						if (v1->f2 != 2) {
							VecSubf(nvec, v1->co, E_NEAR(v2)->co);
							lenn = VecLength(nvec);
							/* 2 < n < 1 */
							if (lenn - len2 > THRESHOLD && len1 - lenn > THRESHOLD) {
								VECCOPY(vec1, nvec);
								E_NEAR(v1) = E_NEAR(v2);
								done = 1;
							}
							/* n < 2 < 1 */
							else if (len1 - len2 > THRESHOLD && len2 - lenn > THRESHOLD) {
								VECCOPY(vec1, vec2);
								E_NEAR(v1) = E_NEAR(v2);
								done = 1;
							}
						}
					}
					else {
						v2->f2 = 1;
						VecSubf(vec2, v2->co, E_NEAR(v1)->co);
						/* 2 < 1 */
						if (VecLength(vec1) - VecLength(vec2) > THRESHOLD) {
							VECCOPY(vec2, vec1);
						}
						E_NEAR(v2) = E_NEAR(v1);
						done = 1;
					}
				}
				else if (v2->f2) {
					v1->f2 = 1;
					VecSubf(vec1, v1->co, E_NEAR(v2)->co);
					/* 2 < 1 */
					if (VecLength(vec2) - VecLength(vec1) > THRESHOLD) {
						VECCOPY(vec1, vec2);
					}
					E_NEAR(v1) = E_NEAR(v2);
					done = 1;
				}
			}
		}
	}
}

/* loop-in-a-loop I know, but we need it! (ton) */
static void get_face_center(float *cent, EditVert *eve)
{
	EditMesh *em = G.editMesh;
	EditFace *efa;
	
	for(efa= em->faces.first; efa; efa= efa->next)
		if(efa->f & SELECT)
			if(efa->v1==eve || efa->v2==eve || efa->v3==eve || efa->v4==eve)
				break;
	if(efa) {
		VECCOPY(cent, efa->cent);
	}
}

static void VertsToTransData(TransData *td, EditVert *eve)
{
	td->flag = 0;
	td->loc = eve->co;
	
	VECCOPY(td->center, td->loc);
	if(G.vd->around==V3D_LOCAL && (G.scene->selectmode & SCE_SELECT_FACE))
		get_face_center(td->center, eve);
	VECCOPY(td->iloc, td->loc);

	// Setting normals
	VECCOPY(td->axismtx[2], eve->no);
	td->axismtx[0][0]		=
		td->axismtx[0][1]	=
		td->axismtx[0][2]	=
		td->axismtx[1][0]	=
		td->axismtx[1][1]	=
		td->axismtx[1][2]	= 0.0f;

	td->ext = NULL;
	td->tdi = NULL;
	td->val = NULL;
	td->tdmir= NULL;

#ifdef WITH_VERSE
	if(eve->vvert) {
		td->verse = (void*)eve->vvert;
		td->flag |= TD_VERSE_VERT;
	}
	else
		td->flag &= ~TD_VERSE_VERT;
#endif
}

/* *********************** CrazySpace correction. Now without doing subsurf optimal ****************** */

static void make_vertexcos__mapFunc(void *userData, int index, float *co, float *no_f, short *no_s)
{
	float *vec = userData;
	
	vec+= 3*index;
	VECCOPY(vec, co);
}

/* hurmf, copy from buttons_editing.c, i have to sort this out what it means... */
static void modifiers_setOnCage(void *ob_v, void *md_v)
{
	Object *ob = ob_v;
	ModifierData *md;
	
	int i, cageIndex = modifiers_getCageIndex(ob, NULL );
	
	for( i = 0, md=ob->modifiers.first; md; ++i, md=md->next )
		if( md == md_v ) {
			if( i >= cageIndex )
				md->mode ^= eModifierMode_OnCage;
			break;
		}
}


/* disable subsurf temporal, get mapped cos, and enable it */
static float *get_crazy_mapped_editverts(void)
{
	DerivedMesh *dm;
	ModifierData *md;
	float *vertexcos;
	int i;
	
	for( i = 0, md=G.obedit->modifiers.first; md; ++i, md=md->next ) {
		if(md->type==eModifierType_Subsurf)
			if(md->mode & eModifierMode_OnCage)
				break;
	}
	if(md) {
		/* this call disables subsurf and enables the underlying modifier to deform, apparently */
		modifiers_setOnCage(G.obedit, md);
		/* make it all over */
		makeDerivedMesh(G.obedit, CD_MASK_BAREMESH);
	}
	
	/* now get the cage */
	dm= editmesh_get_derived_cage(CD_MASK_BAREMESH);

	vertexcos= MEM_mallocN(3*sizeof(float)*G.totvert, "vertexcos map");
	dm->foreachMappedVert(dm, make_vertexcos__mapFunc, vertexcos);
	
	dm->release(dm);
	
	if(md) {
		/* set back the flag, no new cage needs to be built, transform does it */
		modifiers_setOnCage(G.obedit, md);
	}
	
	return vertexcos;
}

#define TAN_MAKE_VEC(a, b, c)	a[0]= b[0] + 0.2f*(b[0]-c[0]); a[1]= b[1] + 0.2f*(b[1]-c[1]); a[2]= b[2] + 0.2f*(b[2]-c[2])
static void set_crazy_vertex_quat(float *quat, float *v1, float *v2, float *v3, float *def1, float *def2, float *def3)
{
	float vecu[3], vecv[3];
	float q1[4], q2[4];
	
	TAN_MAKE_VEC(vecu, v1, v2);
	TAN_MAKE_VEC(vecv, v1, v3);
	triatoquat(v1, vecu, vecv, q1);
	
	TAN_MAKE_VEC(vecu, def1, def2);
	TAN_MAKE_VEC(vecv, def1, def3);
	triatoquat(def1, vecu, vecv, q2);
	
	QuatSub(quat, q2, q1);
}
#undef TAN_MAKE_VEC

static void set_crazyspace_quats(float *mappedcos, float *quats)
{
	EditMesh *em = G.editMesh;
	EditVert *eve, *prev;
	EditFace *efa;
	float *v1, *v2, *v3, *v4;
	long index= 0;
	
	/* two abused locations in vertices */
	for(eve= em->verts.first; eve; eve= eve->next, index++) {
		eve->tmp.fp = NULL;
		eve->prev= (EditVert *)index;
	}
	
	/* first store two sets of tangent vectors in vertices, we derive it just from the face-edges */
	for(efa= em->faces.first; efa; efa= efa->next) {
		
		/* retrieve mapped coordinates */
		v1= mappedcos + 3*( (long)(efa->v1->prev) );
		v2= mappedcos + 3*( (long)(efa->v2->prev) );
		v3= mappedcos + 3*( (long)(efa->v3->prev) );
		
		if(efa->v2->tmp.fp==NULL && efa->v2->f1) {
			set_crazy_vertex_quat(quats, efa->v2->co, efa->v3->co, efa->v1->co, v2, v3, v1);
			efa->v2->tmp.fp= quats;
			quats+= 4;
		}
		
		if(efa->v4) {
			v4= mappedcos + 3*( (long)(efa->v4->prev) );
			
			if(efa->v1->tmp.fp==NULL && efa->v1->f1) {
				set_crazy_vertex_quat(quats, efa->v1->co, efa->v2->co, efa->v4->co, v1, v2, v4);
				efa->v1->tmp.fp= quats;
				quats+= 4;
			}
			if(efa->v3->tmp.fp==NULL && efa->v3->f1) {
				set_crazy_vertex_quat(quats, efa->v3->co, efa->v4->co, efa->v2->co, v3, v4, v2);
				efa->v3->tmp.fp= quats;
				quats+= 4;
			}
			if(efa->v4->tmp.fp==NULL && efa->v4->f1) {
				set_crazy_vertex_quat(quats, efa->v4->co, efa->v1->co, efa->v3->co, v4, v1, v3);
				efa->v4->tmp.fp= quats;
				quats+= 4;
			}
		}
		else {
			if(efa->v1->tmp.fp==NULL && efa->v1->f1) {
				set_crazy_vertex_quat(quats, efa->v1->co, efa->v2->co, efa->v3->co, v1, v2, v3);
				efa->v1->tmp.fp= quats;
				quats+= 4;
			}
			if(efa->v3->tmp.fp==NULL && efa->v3->f1) {
				set_crazy_vertex_quat(quats, efa->v3->co, efa->v1->co, efa->v2->co, v3, v1, v2);
				efa->v3->tmp.fp= quats;
				quats+= 4;
			}
		}
	}

	/* restore abused prev pointer */
	for(prev= NULL, eve= em->verts.first; eve; prev= eve, eve= eve->next)
		eve->prev= prev;

}

static void createTransEditVerts(TransInfo *t)
{
	TransData *tob = NULL;
	EditMesh *em = G.editMesh;
	EditVert *eve;
	EditVert **nears = NULL;
	float *vectors = NULL, *mappedcos = NULL, *quats= NULL;
	float mtx[3][3], smtx[3][3];
	int count=0, countsel=0;
	int propmode = t->flag & T_PROP_EDIT;
	int mirror= (G.scene->toolsettings->editbutflag & B_MESH_X_MIRROR);

	// transform now requires awareness for select mode, so we tag the f1 flags in verts
	if(G.scene->selectmode & SCE_SELECT_VERTEX) {
		for(eve= em->verts.first; eve; eve= eve->next) {
			if(eve->h==0 && (eve->f & SELECT)) 
				eve->f1= SELECT;
			else
				eve->f1= 0;
		}
	}
	else if(G.scene->selectmode & SCE_SELECT_EDGE) {
		EditEdge *eed;
		for(eve= em->verts.first; eve; eve= eve->next) eve->f1= 0;
		for(eed= em->edges.first; eed; eed= eed->next) {
			if(eed->h==0 && (eed->f & SELECT))
				eed->v1->f1= eed->v2->f1= SELECT;
		}
	}
	else {
		EditFace *efa;
		for(eve= em->verts.first; eve; eve= eve->next) eve->f1= 0;
		for(efa= em->faces.first; efa; efa= efa->next) {
			if(efa->h==0 && (efa->f & SELECT)) {
				efa->v1->f1= efa->v2->f1= efa->v3->f1= SELECT;
				if(efa->v4) efa->v4->f1= SELECT;
			}
		}
	}
	
	/* now we can count */
	for(eve= em->verts.first; eve; eve= eve->next) {
		if(eve->h==0) {
			if(eve->f1) countsel++;
			if(propmode) count++;
		}
	}
	
 	/* note: in prop mode we need at least 1 selected */
	if (countsel==0) return;
	
	if(propmode) {
		t->total = count; 
	
		/* allocating scratch arrays */
		vectors = (float *)MEM_mallocN(t->total * 3 * sizeof(float), "scratch vectors");
		nears = (EditVert**)MEM_mallocN(t->total * sizeof(EditVert*), "scratch nears");
	}
	else t->total = countsel;
	tob= t->data= MEM_callocN(t->total*sizeof(TransData), "TransObData(Mesh EditMode)");
	
	Mat3CpyMat4(mtx, G.obedit->obmat);
	Mat3Inv(smtx, mtx);

	if(propmode) editmesh_set_connectivity_distance(t->total, vectors, nears);
	
	/* detect CrazySpace [tm] */
	if(propmode==0) {
		if(modifiers_getCageIndex(G.obedit, NULL)>=0) {
			if(modifiers_isDeformed(G.obedit)) {
				/* disable subsurf temporal, get mapped cos, and enable it */
				mappedcos= get_crazy_mapped_editverts();
				quats= MEM_mallocN( (t->total)*sizeof(float)*4, "crazy quats");
				set_crazyspace_quats(mappedcos, quats);
			}
		}
	}
	
	/* find out which half we do */
	if(mirror) {
		for (eve=em->verts.first; eve; eve=eve->next) {
			if(eve->h==0 && eve->f1 && eve->co[0]!=0.0f) {
				if(eve->co[0]<0.0f)
					mirror = -1;
				break;
			}
		}
	}
	
	for (eve=em->verts.first; eve; eve=eve->next) {
		if(eve->h==0) {
			if(propmode || eve->f1) {
				VertsToTransData(tob, eve);

				if(eve->f1) tob->flag |= TD_SELECTED;
				if(propmode) {
					if (eve->f2) {
						float vec[3];
						VECCOPY(vec, E_VEC(eve));
						Mat3MulVecfl(mtx, vec);
						tob->dist= VecLength(vec);
					}
					else {
						tob->flag |= TD_NOTCONNECTED;
						tob->dist = MAXFLOAT;
					}
				}
				
				/* CrazySpace */
				if(quats && eve->tmp.fp) {
					float mat[3][3], imat[3][3], qmat[3][3];
					
					QuatToMat3(eve->tmp.fp, qmat);
					Mat3MulMat3(mat, mtx, qmat);
					Mat3Inv(imat, mat);
					
					Mat3CpyMat3(tob->smtx, imat);
					Mat3CpyMat3(tob->mtx, mat);
				}
				else {
					Mat3CpyMat3(tob->smtx, smtx);
					Mat3CpyMat3(tob->mtx, mtx);
				}
				
				/* Mirror? */
				if( (mirror>0 && tob->iloc[0]>0.0f) || (mirror<0 && tob->iloc[0]<0.0f)) {
					EditVert *vmir= editmesh_get_x_mirror_vert(G.obedit, tob->iloc);	/* initializes octree on first call */
					if(vmir!=eve) tob->tdmir= vmir;
				}
				tob++;
			}
		}	
	}
	if (propmode) {
		MEM_freeN(vectors);
		MEM_freeN(nears);
	}
	/* crazy space free */
	if(mappedcos)
		MEM_freeN(mappedcos);
	if(quats)
		MEM_freeN(quats);
}

/* ********************* UV ****************** */

static void UVsToTransData(TransData *td, TransData2D *td2d, float *uv, int selected)
{
	float aspx, aspy;

	transform_aspect_ratio_tface_uv(&aspx, &aspy);

	/* uv coords are scaled by aspects. this is needed for rotations and
	   proportional editing to be consistent with the stretchted uv coords
	   that are displayed. this also means that for display and numinput,
	   and when the the uv coords are flushed, these are converted each time */
	td2d->loc[0] = uv[0]*aspx;
	td2d->loc[1] = uv[1]*aspy;
	td2d->loc[2] = 0.0f;
	td2d->loc2d = uv;

	td->flag = 0;
	td->loc = td2d->loc;
	VECCOPY(td->center, td->loc);
	VECCOPY(td->iloc, td->loc);

	memset(td->axismtx, 0, sizeof(td->axismtx));
	td->axismtx[2][2] = 1.0f;

	td->ext= NULL; td->tdi= NULL; td->val= NULL;

	if(selected) {
		td->flag |= TD_SELECTED;
		td->dist= 0.0;
	}
	else
		td->dist= MAXFLOAT;
	
	Mat3One(td->mtx);
	Mat3One(td->smtx);
}

static void createTransUVs(TransInfo *t)
{
	TransData *td = NULL;
	TransData2D *td2d = NULL;
	Mesh *me;
	MFace *mf;
	MTFace *tf;
	int a, count=0, countsel=0;
	int propmode = t->flag & T_PROP_EDIT;
	
	if(is_uv_tface_editing_allowed()==0) return;
	me= get_mesh(OBACT);

	/* count */
	tf= me->mtface;
	mf= me->mface;
	for(a=me->totface; a>0; a--, tf++, mf++) {
		if(mf->v3 && mf->flag & ME_FACE_SEL) {
			if(tf->flag & TF_SEL1) countsel++;
			if(tf->flag & TF_SEL2) countsel++;
			if(tf->flag & TF_SEL3) countsel++;
			if(mf->v4 && (tf->flag & TF_SEL4)) countsel++;
			if(propmode)
				count += (mf->v4)? 4: 3;
		}
	}

 	/* note: in prop mode we need at least 1 selected */
	if (countsel==0) return;
	
	t->total= (propmode)? count: countsel;
	t->data= MEM_callocN(t->total*sizeof(TransData), "TransObData(UV Editing)");
	/* for each 2d uv coord a 3d vector is allocated, so that they can be
	   treated just as if they were 3d verts */
	t->data2d= MEM_mallocN(t->total*sizeof(TransData2D), "TransObData2D(UV Editing)");

	if(G.sima->flag & SI_CLIP_UV)
		t->flag |= T_CLIP_UV;

	td= t->data;
	td2d= t->data2d;
	tf= me->mtface;
	mf= me->mface;
	for(a=me->totface; a>0; a--, tf++, mf++) {
		if(mf->v3 && mf->flag & ME_FACE_SEL) {
			if(tf->flag & TF_SEL1 || propmode)
				UVsToTransData(td++, td2d++, tf->uv[0], (tf->flag & TF_SEL1));
			if(tf->flag & TF_SEL2 || propmode)
				UVsToTransData(td++, td2d++, tf->uv[1], (tf->flag & TF_SEL2));
			if(tf->flag & TF_SEL3 || propmode)
				UVsToTransData(td++, td2d++, tf->uv[2], (tf->flag & TF_SEL3));

			if(mf->v4 && (tf->flag & TF_SEL4 || propmode))
				UVsToTransData(td++, td2d++, tf->uv[3], (tf->flag & TF_SEL4));
		}
	}

	if (G.sima->flag & SI_LIVE_UNWRAP)
		unwrap_lscm_live_begin();
}

void flushTransUVs(TransInfo *t)
{
	TransData2D *td;
	int a, width, height;
	Object *ob= OBACT;
	Mesh *me= get_mesh(ob);
	float aspx, aspy, invx, invy;

	transform_aspect_ratio_tface_uv(&aspx, &aspy);
	transform_width_height_tface_uv(&width, &height);
	invx= 1.0f/aspx;
	invy= 1.0f/aspy;

	/* flush to 2d vector from internally used 3d vector */
	for(a=0, td= t->data2d; a<t->total; a++, td++) {
		td->loc2d[0]= td->loc[0]*invx;
		td->loc2d[1]= td->loc[1]*invy;
		
		if((G.sima->flag & SI_PIXELSNAP) && (t->state != TRANS_CANCEL)) {
			td->loc2d[0]= floor(width*td->loc2d[0] + 0.5f)/width;
			td->loc2d[1]= floor(height*td->loc2d[1] + 0.5f)/height;
		}
	}

	/* always call this, also for cancel (it transforms non-selected vertices...) */
	if((G.sima->flag & SI_BE_SQUARE))
		be_square_tface_uv(me);

	/* this is overkill if G.sima->lock is not set, but still needed */
	object_uvs_changed(ob);
}

int clipUVTransform(TransInfo *t, float *vec, int resize)
{
	TransData *td;
	int a, clipx=1, clipy=1;
	float aspx, aspy, min[2], max[2];

	transform_aspect_ratio_tface_uv(&aspx, &aspy);
	min[0]= min[1]= 0.0f;
	max[0]= aspx; max[1]= aspy;

	for(a=0, td= t->data; a<t->total; a++, td++) {
		DO_MINMAX2(td->loc, min, max);
	}

	if(resize) {
		if(min[0] < 0.0f && t->center[0] > 0.0f && t->center[0] < aspx*0.5f)
			vec[0] *= t->center[0]/(t->center[0] - min[0]);
		else if(max[0] > aspx && t->center[0] < aspx)
			vec[0] *= (t->center[0] - aspx)/(t->center[0] - max[0]);
		else
			clipx= 0;

		if(min[1] < 0.0f && t->center[1] > 0.0f && t->center[1] < aspy*0.5f)
			vec[1] *= t->center[1]/(t->center[1] - min[1]);
		else if(max[1] > aspy && t->center[1] < aspy)
			vec[1] *= (t->center[1] - aspy)/(t->center[1] - max[1]);
		else
			clipy= 0;
	}
	else {
		if(min[0] < 0.0f)
			vec[0] -= min[0];
		else if(max[0] > aspx)
			vec[0] -= max[0]-aspx;
		else
			clipx= 0;

		if(min[1] < 0.0f)
			vec[1] -= min[1];
		else if(max[1] > aspy)
			vec[1] -= max[1]-aspy;
		else
			clipy= 0;
	}	

	return (clipx || clipy);
}

/* **************** IpoKey stuff, for Object TransData ********** */

/* storage of bezier triple. thats why -3 and +3! */
static void set_tdi_old(float *old, float *poin)
{
	old[0]= *(poin);
	old[3]= *(poin-3);
	old[6]= *(poin+3);
}

/* while transforming */
void add_tdi_poin(float *poin, float *old, float delta)
{
	if(poin) {
		poin[0]= old[0]+delta;
		poin[-3]= old[3]+delta;
		poin[3]= old[6]+delta;
	}
}

/* fill ipokey transdata with old vals and pointers */
static void ipokey_to_transdata(IpoKey *ik, TransData *td)
{
	extern int ob_ar[];		// blenkernel ipo.c
	TransDataIpokey *tdi= td->tdi;
	BezTriple *bezt;
	int a, delta= 0;
	
	td->val= NULL;	// is read on ESC
	
	for(a=0; a<OB_TOTIPO; a++) {
		if(ik->data[a]) {
			bezt= ik->data[a];
			
			switch( ob_ar[a] ) {
				case OB_LOC_X:
				case OB_DLOC_X:
					tdi->locx= &(bezt->vec[1][1]); break;
				case OB_LOC_Y:
				case OB_DLOC_Y:
					tdi->locy= &(bezt->vec[1][1]); break;
				case OB_LOC_Z:
				case OB_DLOC_Z:
					tdi->locz= &(bezt->vec[1][1]); break;
					
				case OB_DROT_X:
					delta= 1;
				case OB_ROT_X:
					tdi->rotx= &(bezt->vec[1][1]); break;
				case OB_DROT_Y:
					delta= 1;
				case OB_ROT_Y:
					tdi->roty= &(bezt->vec[1][1]); break;
				case OB_DROT_Z:
					delta= 1;
				case OB_ROT_Z:
					tdi->rotz= &(bezt->vec[1][1]); break;
					
				case OB_SIZE_X:
				case OB_DSIZE_X:
					tdi->sizex= &(bezt->vec[1][1]); break;
				case OB_SIZE_Y:
				case OB_DSIZE_Y:
					tdi->sizey= &(bezt->vec[1][1]); break;
				case OB_SIZE_Z:
				case OB_DSIZE_Z:
					tdi->sizez= &(bezt->vec[1][1]); break;		
			}	
		}
	}
	
	/* oldvals for e.g. undo */
	if(tdi->locx) set_tdi_old(tdi->oldloc, tdi->locx);
	if(tdi->locy) set_tdi_old(tdi->oldloc+1, tdi->locy);
	if(tdi->locz) set_tdi_old(tdi->oldloc+2, tdi->locz);
	
	/* remember, for mapping curves ('1'=10 degrees)  */
	if(tdi->rotx) set_tdi_old(tdi->oldrot, tdi->rotx);
	if(tdi->roty) set_tdi_old(tdi->oldrot+1, tdi->roty);
	if(tdi->rotz) set_tdi_old(tdi->oldrot+2, tdi->rotz);
	
	/* this is not allowed to be dsize! */
	if(tdi->sizex) set_tdi_old(tdi->oldsize, tdi->sizex);
	if(tdi->sizey) set_tdi_old(tdi->oldsize+1, tdi->sizey);
	if(tdi->sizez) set_tdi_old(tdi->oldsize+2, tdi->sizez);
	
	tdi->flag= TOB_IPO;
	if(delta) tdi->flag |= TOB_IPODROT;
}


/* *************************** Object Transform data ******************* */

/* Little helper function for ObjectToTransData used to give certain
 * constraints (ChildOf, FollowPath, and others that may be added)
 * inverse corrections for transform, so that they aren't in CrazySpace.
 * These particular constraints benefit from this, but others don't, hence
 * this semi-hack ;-)    - Aligorith
 */
static short constraints_list_needinv(ListBase *list)
{
	bConstraint *con;
	
	/* loop through constraints, checking if there's one of the mentioned 
	 * constraints needing special crazyspace corrections
	 */
	if (list) {
		for (con= list->first; con; con=con->next) {
			/* only consider constraint if it is enabled, and has influence on result */
			if ((con->flag & CONSTRAINT_DISABLE)==0 && (con->enforce!=0.0)) {
				/* (affirmative) returns for specific constraints here... */
				if (con->type == CONSTRAINT_TYPE_CHILDOF) return 1;
				if (con->type == CONSTRAINT_TYPE_FOLLOWPATH) return 1;
				if (con->type == CONSTRAINT_TYPE_CLAMPTO) return 1;
			}
		}
	}
	
	/* no appropriate candidates found */
	return 0;
}

/* transcribe given object into TransData for Transforming */
static void ObjectToTransData(TransData *td, Object *ob) 
{
	Object *track;
	ListBase fakecons = {NULL, NULL};
	float obmtx[3][3];
	short constinv;

	/* axismtx has the real orientation */
	Mat3CpyMat4(td->axismtx, ob->obmat);
	Mat3Ortho(td->axismtx);

	/* hack: tempolarily disable tracking and/or constraints when getting 
	 *		object matrix, if tracking is on, or if constraints don't need
	 * 		inverse correction to stop it from screwing up space conversion
	 *		matrix later
	 */
	constinv= constraints_list_needinv(&ob->constraints);
	if (ob->track || constinv==0) {
		track= ob->track;
		ob->track= NULL;
		
		if (constinv == 0) {
			fakecons.first = ob->constraints.first;
			fakecons.last = ob->constraints.last;
			ob->constraints.first = ob->constraints.last = NULL;
		}
		
		where_is_object(ob);
		
		if (constinv == 0) {
			ob->constraints.first = fakecons.first;
			ob->constraints.last = fakecons.last;
		}
		
		ob->track= track;
	}
	else
		where_is_object(ob);

	td->ob = ob;

	td->loc = ob->loc;
	VECCOPY(td->iloc, td->loc);
	
	td->ext->rot = ob->rot;
	VECCOPY(td->ext->irot, ob->rot);
	VECCOPY(td->ext->drot, ob->drot);
	
	td->ext->size = ob->size;
	VECCOPY(td->ext->isize, ob->size);
	VECCOPY(td->ext->dsize, ob->dsize);

	VECCOPY(td->center, ob->obmat[3]);

	/* is there a need to set the global<->data space conversion matrices? */
	if (ob->parent || constinv) {
		float totmat[3][3], obinv[3][3];
		
		/* Get the effect of parenting, and/or certain constraints.
		 * NOTE: some Constraints, and also Tracking should never get this
		 *		done, as it doesn't work well.
		 */
		object_to_mat3(ob, obmtx);
		Mat3CpyMat4(totmat, ob->obmat);
		Mat3Inv(obinv, totmat);
		Mat3MulMat3(td->smtx, obmtx, obinv);
		Mat3Inv(td->mtx, td->smtx);
	}
	else {
		/* no conversion to/from dataspace */
		Mat3One(td->smtx);
		Mat3One(td->mtx);
	}
#ifdef WITH_VERSE
	if(ob->vnode) {
		td->verse = (void*)ob;
		td->flag |= TD_VERSE_OBJECT;
	}
	else
		td->flag &= ~TD_VERSE_OBJECT;
#endif
}


/* sets flags in Bases to define whether they take part in transform */
/* it deselects Bases, so we have to call the clear function always after */
static void set_trans_object_base_flags(TransInfo *t)
{
	/*
	 if Base selected and has parent selected:
	 base->flag= BA_WAS_SEL
	 */
	Base *base;
	
	/* makes sure base flags and object flags are identical */
	copy_baseflags();
	
	/* handle pending update events, otherwise they got copied below */
	for (base= FIRSTBASE; base; base= base->next) {
		if(base->object->recalc) 
			object_handle_update(base->object);
	}
	
	for (base= FIRSTBASE; base; base= base->next) {
		base->flag &= ~BA_WAS_SEL;
		
		if(TESTBASELIB(base)) {
			Object *ob= base->object;
			Object *parsel= ob->parent;
			
			/* if parent selected, deselect */
			while(parsel) {
				if(parsel->flag & SELECT) break;
				parsel= parsel->parent;
			}
			
			if(parsel) {
				base->flag &= ~SELECT;
				base->flag |= BA_WAS_SEL;
			}
			/* used for flush, depgraph will change recalcs if needed :) */
			ob->recalc |= OB_RECALC_OB;
		}
	}
	/* all recalc flags get flushed to all layers, so a layer flip later on works fine */
	DAG_scene_flush_update(G.scene, -1);
	
	/* and we store them temporal in base (only used for transform code) */
	/* this because after doing updates, the object->recalc is cleared */
	for (base= FIRSTBASE; base; base= base->next) {
		if(base->object->recalc & OB_RECALC_OB)
			base->flag |= BA_HAS_RECALC_OB;
		if(base->object->recalc & OB_RECALC_DATA)
			base->flag |= BA_HAS_RECALC_DATA;
	}
}

static void clear_trans_object_base_flags(void)
{
	Base *base;
	
	base= FIRSTBASE;
	while(base) {
		if(base->flag & BA_WAS_SEL) base->flag |= SELECT;
		base->flag &= ~(BA_WAS_SEL|BA_HAS_RECALC_OB|BA_HAS_RECALC_DATA|BA_DO_IPO);
		
		base = base->next;
	}
}

/* auto-keyframing feature - for objects 
 * 	tmode: should be a transform mode 
 */
void autokeyframe_ob_cb_func(Object *ob, int tmode)
{
	IpoCurve *icu;
	char *actname="";
	
	if (G.flags & G_RECORDKEYS) {
		if(ob->ipoflag & OB_ACTION_OB)
			actname= "Object";

		if(U.uiflag & USER_KEYINSERTAVAI) {
			if(ob->ipo || ob->action) {
				ID *id= (ID *)(ob);
				
				if (ob->ipo) {
					icu= ob->ipo->curve.first;
				}
				else {
					bActionChannel *achan;
					achan= get_action_channel(ob->action, actname);
					
					if (achan && achan->ipo)
						icu= achan->ipo->curve.first;
					else
						icu= NULL;
				}
				
				while(icu) {
					icu->flag &= ~IPO_SELECT;
					if (U.uiflag & USER_KEYINSERTNEED)
						insertkey_smarter(id, ID_OB, actname, NULL, icu->adrcode);
					else
						insertkey(id, ID_OB, actname, NULL, icu->adrcode);
					icu= icu->next;
				}
			}
		}
		else if (U.uiflag & USER_KEYINSERTNEED) {
			if (tmode==TFM_RESIZE) {
				insertkey_smarter(&ob->id, ID_OB, actname, NULL, OB_SIZE_X);
				insertkey_smarter(&ob->id, ID_OB, actname, NULL, OB_SIZE_Y);
				insertkey_smarter(&ob->id, ID_OB, actname, NULL, OB_SIZE_Z);
			}
			else if (tmode==TFM_ROTATION) {
				insertkey_smarter(&ob->id, ID_OB, actname, NULL, OB_ROT_X);
				insertkey_smarter(&ob->id, ID_OB, actname, NULL, OB_ROT_Y);
				insertkey_smarter(&ob->id, ID_OB, actname, NULL, OB_ROT_Z);
			}
			else if (tmode==TFM_TRANSLATION) {
				insertkey_smarter(&ob->id, ID_OB, actname, NULL, OB_LOC_X);
				insertkey_smarter(&ob->id, ID_OB, actname, NULL, OB_LOC_Y);
				insertkey_smarter(&ob->id, ID_OB, actname, NULL, OB_LOC_Z);
			}
		}
		else {
			insertkey(&ob->id, ID_OB, actname, NULL, OB_ROT_X);
			insertkey(&ob->id, ID_OB, actname, NULL, OB_ROT_Y);
			insertkey(&ob->id, ID_OB, actname, NULL, OB_ROT_Z);

			insertkey(&ob->id, ID_OB, actname, NULL, OB_LOC_X);
			insertkey(&ob->id, ID_OB, actname, NULL, OB_LOC_Y);
			insertkey(&ob->id, ID_OB, actname, NULL, OB_LOC_Z);

			insertkey(&ob->id, ID_OB, actname, NULL, OB_SIZE_X);
			insertkey(&ob->id, ID_OB, actname, NULL, OB_SIZE_Y);
			insertkey(&ob->id, ID_OB, actname, NULL, OB_SIZE_Z);
		}

		remake_object_ipos(ob);
		allqueue(REDRAWIPO, 0);
		allspace(REMAKEIPO, 0);
		allqueue(REDRAWVIEW3D, 0);
		allqueue(REDRAWNLA, 0);
		allqueue(REDRAWTIME, 0);
	}
}

/* auto-keyframing feature - for poses/pose-channels 
 * 	tmode: should be a transform mode 
 *	targetless_ik: has targetless ik been done on any channels?
 */
void autokeyframe_pose_cb_func(Object *ob, int tmode, short targetless_ik)
{
	bAction	*act;
	bPose	*pose;
	bPoseChannel *pchan;
	IpoCurve *icu;
	
	pose= ob->pose;
	act= ob->action;
	
	if (G.flags & G_RECORDKEYS) {
		if (!act)
			act= ob->action= add_empty_action("Action");
		
		for (pchan=pose->chanbase.first; pchan; pchan=pchan->next){
			if (pchan->bone->flag & BONE_TRANSFORM){

				if(U.uiflag & USER_KEYINSERTAVAI) {
					bActionChannel *achan; 

					for (achan = act->chanbase.first; achan; achan=achan->next){
						if (achan->ipo && !strcmp (achan->name, pchan->name)){
							for (icu = achan->ipo->curve.first; icu; icu=icu->next){
								if (U.uiflag & USER_KEYINSERTNEED)
									insertkey_smarter(&ob->id, ID_PO, pchan->name, NULL, icu->adrcode);
								else
									insertkey(&ob->id, ID_PO, pchan->name, NULL, icu->adrcode);
							}
							break;
						}
					}
				}
				else if (U.uiflag & USER_KEYINSERTNEED) {
					if ((tmode==TFM_TRANSLATION) && (targetless_ik==0)) {
						insertkey_smarter(&ob->id, ID_PO, pchan->name, NULL, AC_LOC_X);
						insertkey_smarter(&ob->id, ID_PO, pchan->name, NULL, AC_LOC_Y);
						insertkey_smarter(&ob->id, ID_PO, pchan->name, NULL, AC_LOC_Z);
					}
					if ((tmode==TFM_ROTATION) || ((tmode==TFM_TRANSLATION) && targetless_ik)) {
						insertkey_smarter(&ob->id, ID_PO, pchan->name, NULL, AC_QUAT_W);
						insertkey_smarter(&ob->id, ID_PO, pchan->name, NULL, AC_QUAT_X);
						insertkey_smarter(&ob->id, ID_PO, pchan->name, NULL, AC_QUAT_Y);
						insertkey_smarter(&ob->id, ID_PO, pchan->name, NULL, AC_QUAT_Z);
					}
					if (tmode==TFM_RESIZE) {
						insertkey_smarter(&ob->id, ID_PO, pchan->name, NULL, AC_SIZE_X);
						insertkey_smarter(&ob->id, ID_PO, pchan->name, NULL, AC_SIZE_Y);
						insertkey_smarter(&ob->id, ID_PO, pchan->name, NULL, AC_SIZE_Z);
					}
				}
				else {
					insertkey(&ob->id, ID_PO, pchan->name, NULL, AC_SIZE_X);
					insertkey(&ob->id, ID_PO, pchan->name, NULL, AC_SIZE_Y);
					insertkey(&ob->id, ID_PO, pchan->name, NULL, AC_SIZE_Z);

					insertkey(&ob->id, ID_PO, pchan->name, NULL, AC_QUAT_W);
					insertkey(&ob->id, ID_PO, pchan->name, NULL, AC_QUAT_X);
					insertkey(&ob->id, ID_PO, pchan->name, NULL, AC_QUAT_Y);
					insertkey(&ob->id, ID_PO, pchan->name, NULL, AC_QUAT_Z);

					insertkey(&ob->id, ID_PO, pchan->name, NULL, AC_LOC_X);
					insertkey(&ob->id, ID_PO, pchan->name, NULL, AC_LOC_Y);
					insertkey(&ob->id, ID_PO, pchan->name, NULL, AC_LOC_Z);
				}
			}
		}
		
		remake_action_ipos (act);
		allspace(REMAKEIPO, 0);
		allqueue(REDRAWACTION, 0);
		allqueue(REDRAWIPO, 0);
		allqueue(REDRAWNLA, 0);
		allqueue(REDRAWTIME, 0);
	}
}

/* inserting keys, refresh ipo-keys, softbody, redraw events... (ton) */
/* note; transdata has been freed already! */
void special_aftertrans_update(TransInfo *t)
{
	Object *ob;
	Base *base;
	int redrawipo=0;
	int cancelled= (t->state == TRANS_CANCEL);
		
	if(G.obedit) {
		if(t->mode==TFM_BONESIZE || t->mode==TFM_BONE_ENVELOPE)
			allqueue(REDRAWBUTSEDIT, 0);
		
		/* table needs to be created for each edit command, since vertices can move etc */
		mesh_octree_table(G.obedit, NULL, 'e');
	}
	else if( (t->flag & T_POSE) && t->poseobj) {
		bArmature *arm;
		bPose	*pose;
		bPoseChannel *pchan;
		short targetless_ik= 0;

		ob= t->poseobj;
		arm= ob->data;
		pose= ob->pose;
		
		/* this signal does one recalc on pose, then unlocks, so ESC or edit will work */
		pose->flag |= POSE_DO_UNLOCK;

		/* if target-less IK grabbing, we calculate the pchan transforms and clear flag */
		if(!cancelled && t->mode==TFM_TRANSLATION)
			targetless_ik= apply_targetless_ik(ob);
		else {
			/* not forget to clear the auto flag */
			for (pchan=ob->pose->chanbase.first; pchan; pchan=pchan->next) {
				bKinematicConstraint *data= has_targetless_ik(pchan);
				if(data) data->flag &= ~CONSTRAINT_IK_AUTO;
			}
		}
		
		if(t->mode==TFM_TRANSLATION)
			pose_grab_with_ik_clear(ob);
		
		/* automatic inserting of keys */
		if(!cancelled) {
			autokeyframe_pose_cb_func(ob, t->mode, targetless_ik);
			DAG_object_flush_update(G.scene, ob, OB_RECALC_DATA);
		}
		else if(arm->flag & ARM_DELAYDEFORM) {
			/* old optimize trick... this enforces to bypass the depgraph */
			DAG_object_flush_update(G.scene, ob, OB_RECALC_DATA);
			ob->recalc= 0;	// is set on OK position already by recalcData()
		}
		else 
			DAG_object_flush_update(G.scene, ob, OB_RECALC_DATA);
		
		if(t->mode==TFM_BONESIZE || t->mode==TFM_BONE_ENVELOPE)
			allqueue(REDRAWBUTSEDIT, 0);

	}
	else {
		base= FIRSTBASE;
		while(base) {	
			
			if(base->flag & BA_DO_IPO) redrawipo= 1;
			
			ob= base->object;

			if(modifiers_isSoftbodyEnabled(ob)) ob->softflag |= OB_SB_REDO;
			
			/* Set autokey if necessary */
			if ((!cancelled) && (base->flag & SELECT)){
				autokeyframe_ob_cb_func(ob, t->mode);
			}
			
			base= base->next;
		}
		
	}
	
	clear_trans_object_base_flags();
	
	if(redrawipo) {
		allqueue(REDRAWNLA, 0);
		allqueue(REDRAWACTION, 0);
		allqueue(REDRAWIPO, 0);
	}
	
	reset_slowparents();
	
	/* note; should actually only be done for all objects when a lamp is moved... (ton) */
	if(t->spacetype==SPACE_VIEW3D && G.vd->drawtype == OB_SHADED)
		reshadeall_displist();
}

static void createTransObject(TransInfo *t)
{
	TransData *td = NULL;
	TransDataExtension *tx;
	Object *ob;
	Base *base;
	IpoKey *ik;
	ListBase elems;
	
	set_trans_object_base_flags(t);

	/* count */	
	for(base= FIRSTBASE; base; base= base->next) {
		if TESTBASELIB(base) {
			ob= base->object;
			
			/* store ipo keys? */
			if(ob->ipo && ob->ipo->showkey && (ob->ipoflag & OB_DRAWKEY)) {
				elems.first= elems.last= NULL;
				make_ipokey_transform(ob, &elems, 1); /* '1' only selected keys */
				
				pushdata(&elems, sizeof(ListBase));
				
				for(ik= elems.first; ik; ik= ik->next) t->total++;

				if(elems.first==NULL) t->total++;
			}
			else {
				t->total++;
			}
		}
	}

	if(!t->total) {
		/* clear here, main transform function escapes too */
		clear_trans_object_base_flags();
		return;
	}
	
	td = t->data = MEM_callocN(t->total*sizeof(TransData), "TransOb");
	tx = t->ext = MEM_callocN(t->total*sizeof(TransDataExtension), "TransObExtension");

	for(base= FIRSTBASE; base; base= base->next) {
		if TESTBASELIB(base) {
			ob= base->object;
			
			td->flag= TD_SELECTED;
			td->protectflag= ob->protectflag;
			td->ext = tx;

			/* store ipo keys? */
			if(ob->ipo && ob->ipo->showkey && (ob->ipoflag & OB_DRAWKEY)) {
				
				popfirst(&elems);	// bring back pushed listbase
				
				if(elems.first) {
					int cfraont;
					int ipoflag;
					
					base->flag |= BA_DO_IPO+BA_WAS_SEL;
					base->flag &= ~SELECT;
					
					cfraont= CFRA;
					set_no_parent_ipo(1);
					ipoflag= ob->ipoflag;
					ob->ipoflag &= ~OB_OFFS_OB;
					
					/*
					 * This is really EVIL code that pushes down Object values
					 * (loc, dloc, orig, size, dsize, rot, drot)
					 * */
					 
					pushdata((void*)ob->loc, 7 * 3 * sizeof(float)); // tsk! tsk!
					
					for(ik= elems.first; ik; ik= ik->next) {
						
						/* weak... this doesn't correct for floating values, giving small errors */
						CFRA= (int)(ik->val/G.scene->r.framelen);
						
						do_ob_ipo(ob);
						ObjectToTransData(td, ob);	// does where_is_object()
						
						td->flag= TD_SELECTED;
						
						td->tdi= MEM_callocN(sizeof(TransDataIpokey), "TransDataIpokey");
						/* also does tdi->flag and oldvals, needs to be after ob_to_transob()! */
						ipokey_to_transdata(ik, td);
						
						td++;
						tx++;
						if(ik->next) td->ext= tx;	// prevent corrupting mem!
					}
					free_ipokey(&elems);
					
					poplast(ob->loc);
					set_no_parent_ipo(0);
					
					CFRA= cfraont;
					ob->ipoflag= ipoflag;
					
					where_is_object(ob);	// restore 
				}
				else {
					ObjectToTransData(td, ob);
					td->tdi = NULL;
					td->val = NULL;
					td++;
					tx++;
				}
			}
			else {
				ObjectToTransData(td, ob);
				td->tdi = NULL;
				td->val = NULL;
				td++;
				tx++;
			}
		}
	}
}

void createTransData(TransInfo *t) 
{
	Object *ob= OBACT;
	
	if (t->context == CTX_TEXTURE) {
		t->flag |= T_TEXTURE;
		createTransTexspace(t);
	}
	else if (t->context == CTX_EDGE) {
		t->ext = NULL;
		t->flag |= T_EDIT;
		createTransEdge(t);
		if(t->data && t->flag & T_PROP_EDIT) {
			sort_trans_data(t);	// makes selected become first in array
			set_prop_dist(t, 1);
			sort_trans_data_dist(t);
		}
	}
	else if (t->spacetype == SPACE_IMAGE) {
		t->flag |= T_POINTS|T_2D_EDIT;
		createTransUVs(t);
		if(t->data && (t->flag & T_PROP_EDIT)) {
			sort_trans_data(t);	// makes selected become first in array
			set_prop_dist(t, 1);
			sort_trans_data_dist(t);
		}
	}
	else if (G.obedit) {
		t->ext = NULL;
		if (G.obedit->type == OB_MESH) {
			createTransEditVerts(t);	
   		}
		else if ELEM(G.obedit->type, OB_CURVE, OB_SURF) {
			createTransCurveVerts(t);
		}
		else if (G.obedit->type==OB_LATTICE) {
			createTransLatticeVerts(t);
		}
		else if (G.obedit->type==OB_MBALL) {
			createTransMBallVerts(t);
		}
		else if (G.obedit->type==OB_ARMATURE) {
			t->flag &= ~T_PROP_EDIT;
			createTransArmatureVerts(t);
  		}					  		
		else {
			printf("not done yet! only have mesh surface curve\n");
		}

		if(t->data && t->flag & T_PROP_EDIT) {
			if (ELEM(G.obedit->type, OB_CURVE, OB_MESH)) {
				sort_trans_data(t);	// makes selected become first in array
				set_prop_dist(t, 0);
				sort_trans_data_dist(t);
			}
			else {
				sort_trans_data(t);	// makes selected become first in array
				set_prop_dist(t, 1);
				sort_trans_data_dist(t);
			}
		}

		t->flag |= T_EDIT|T_POINTS;
		
		/* exception... hackish, we want bonesize to use bone orientation matrix (ton) */
		if(t->mode==TFM_BONESIZE) {
			t->flag &= ~(T_EDIT|T_POINTS);
			t->flag |= T_POSE;
			t->poseobj= ob;	/* <- tsk tsk, this is going to give issues one day */
		}
	}
	else if (ob && (ob->flag & OB_POSEMODE)) {
		createTransPose(t, OBACT);
	}
	else if (G.f & G_WEIGHTPAINT) {
		/* exception, we look for the one selected armature */
		Base *base;
		for(base=FIRSTBASE; base; base= base->next) {
			if(TESTBASELIB(base)) {
				if(base->object->type==OB_ARMATURE)
					if(base->object->flag & OB_POSEMODE)
						break;
			}
		}
		if(base) {
			createTransPose(t, base->object);
		}
	}
	else {
		createTransObject(t);
		t->flag |= T_OBJECT;
	}

	if((t->flag & T_OBJECT) && G.vd->camera==OBACT && G.vd->persp>1) {
		t->flag |= T_CAMERA;
	}
	
	/* temporal...? */
	G.scene->recalc |= SCE_PRV_CHANGED;	/* test for 3d preview */
}

