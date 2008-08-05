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
 * Contributor(s): none yet.
 *
 * ***** END GPL LICENSE BLOCK *****
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
#include "DNA_nla_types.h"
#include "DNA_object_types.h"
#include "DNA_object_force.h"
#include "DNA_particle_types.h"
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
#include "DNA_gpencil_types.h"

#include "BKE_action.h"
#include "BKE_armature.h"
#include "BKE_blender.h"
#include "BKE_cloth.h"
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
#include "BKE_key.h"
#include "BKE_main.h"
#include "BKE_mball.h"
#include "BKE_mesh.h"
#include "BKE_modifier.h"
#include "BKE_object.h"
#include "BKE_particle.h"
#include "BKE_pointcache.h"
#include "BKE_softbody.h"
#include "BKE_utildefines.h"
#include "BKE_bmesh.h"

#include "BIF_editaction.h"
#include "BIF_editview.h"
#include "BIF_editlattice.h"
#include "BIF_editconstraint.h"
#include "BIF_editarmature.h"
#include "BIF_editmesh.h"
#include "BIF_editnla.h"
#include "BIF_editsima.h"
#include "BIF_editparticle.h"
#include "BIF_gl.h"
#include "BIF_poseobject.h"
#include "BIF_meshtools.h"
#include "BIF_mywindow.h"
#include "BIF_resources.h"
#include "BIF_retopo.h"
#include "BIF_screen.h"
#include "BIF_space.h"
#include "BIF_toolbox.h"

#include "BSE_view.h"
#include "BSE_drawipo.h"
#include "BSE_edit.h"
#include "BSE_editipo.h"
#include "BSE_editipo_types.h"
#include "BSE_editaction_types.h"

#include "BDR_drawaction.h"		// list of keyframes in action
#include "BDR_editobject.h"		// reset_slowparents()
#include "BDR_gpencil.h"
#include "BDR_unwrapper.h"

#include "BLI_arithb.h"
#include "BLI_blenlib.h"
#include "BLI_editVert.h"

#include "editmesh.h"

#include "blendef.h"

#include "mydevice.h"

extern ListBase editNurb;
extern ListBase editelems;

#include "transform.h"

/* local function prototype - for Object/Bone Constraints */
static short constraints_list_needinv(TransInfo *t, ListBase *list);
/* local function prototype - for finding number of keyframes that are selected for editing */
static int count_ipo_keys(Ipo *ipo, char side, float cfra);

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
	int *texflag;
	
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
	
	if (give_obdata_texspace(ob, &texflag, &td->loc, &td->ext->size, &td->ext->rot)) {
		*texflag &= ~AUTOSPACE;
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
			if (t->mode == TFM_BWEIGHT) {
				td->val = &(eed->bweight);
				td->ival = eed->bweight;
			}
			else {
				td->val = &(eed->crease);
				td->ival = eed->crease;
			}

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
					else if(parchan->bone->flag & BONE_NO_SCALE) {
						Mat4MulMat4(tmat, offs_bone, parchan->parent->pose_mat);
						Mat4Ortho(tmat);
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
	if(bone->flag & BONE_HINGE_CHILD_TRANSFORM)
		td->flag |= TD_NOCENTER;
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
		
		if (constraints_list_needinv(t, &pchan->constraints)) {
			Mat3CpyMat4(tmat, pchan->constinv);
			Mat3Inv(cmat, tmat);
			Mat3MulSerie(td->mtx, pchan->bone->bone_mat, pmat, omat, cmat, 0,0,0,0);    // dang mulserie swaps args
		}
		else
			Mat3MulSerie(td->mtx, pchan->bone->bone_mat, pmat, omat, 0,0,0,0,0);    // dang mulserie swaps args
	}
	else {
		if (constraints_list_needinv(t, &pchan->constraints)) {
			Mat3CpyMat4(tmat, pchan->constinv);
			Mat3Inv(cmat, tmat);
			Mat3MulSerie(td->mtx, pchan->bone->bone_mat, omat, cmat, 0,0,0,0,0);    // dang mulserie swaps args
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
	
	/* store reference to first constraint */
	td->con= pchan->constraints.first;
}

static void bone_children_clear_transflag(ListBase *lb)
{
	Bone *bone= lb->first;
	
	for(;bone;bone= bone->next) {
		if((bone->flag & BONE_HINGE) && (bone->flag & BONE_CONNECTED))
			bone->flag |= BONE_HINGE_CHILD_TRANSFORM;
		else
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
	int hastranslation;
	
	t->total= 0;
	
	for(pchan= ob->pose->chanbase.first; pchan; pchan= pchan->next) {
		bone= pchan->bone;
		if(bone->layer & arm->layer) {
			if(bone->flag & BONE_SELECTED)
				bone->flag |= BONE_TRANSFORM;
			else
				bone->flag &= ~BONE_TRANSFORM;

			bone->flag &= ~BONE_HINGE_CHILD_TRANSFORM;
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
	hastranslation= 0;

	for(pchan= ob->pose->chanbase.first; pchan; pchan= pchan->next) {
		bone= pchan->bone;
		if(bone->flag & BONE_TRANSFORM) {

			t->total++;
			
			if(t->mode==TFM_TRANSLATION) {
				if( has_targetless_ik(pchan)==NULL ) {
					if(pchan->parent && (pchan->bone->flag & BONE_CONNECTED)) {
						if(pchan->bone->flag & BONE_HINGE_CHILD_TRANSFORM)
							hastranslation= 1;
					}
					else if((pchan->protectflag & OB_LOCK_LOC)!=OB_LOCK_LOC)
						hastranslation= 1;
				}
				else
					hastranslation= 1;
			}
		}
	}

	/* if there are no translatable bones, do rotation */
	if(t->mode==TFM_TRANSLATION && !hastranslation)
		t->mode= TFM_ROTATION;
}


/* -------- Auto-IK ---------- */

/* adjust pose-channel's auto-ik chainlen */
static void pchan_autoik_adjust (bPoseChannel *pchan, short chainlen)
{
	bConstraint *con;
	
	/* don't bother to search if no valid constraints */
	if ((pchan->constflag & (PCHAN_HAS_IK|PCHAN_HAS_TARGET))==0)
		return;
	
	/* check if pchan has ik-constraint */
	for (con= pchan->constraints.first; con; con= con->next) {
		if (con->type == CONSTRAINT_TYPE_KINEMATIC) {
			bKinematicConstraint *data= con->data;
			
			/* only accept if a temporary one (for auto-ik) */
			if (data->flag & CONSTRAINT_IK_TEMP) {
				/* chainlen is new chainlen, but is limited by maximum chainlen */
				if ((chainlen==0) || (chainlen > data->max_rootbone))
					data->rootbone= data->max_rootbone;
				else
					data->rootbone= chainlen;
			}
		}
	}
}

/* change the chain-length of auto-ik */
void transform_autoik_update (TransInfo *t, short mode)
{
	short *chainlen= &G.scene->toolsettings->autoik_chainlen;
	bPoseChannel *pchan;
	
	/* mode determines what change to apply to chainlen */
	if (mode == 1) {
		/* mode=1 is from WHEELMOUSEDOWN... increases len */
		(*chainlen)++;
	}
	else if (mode == -1) {
		/* mode==-1 is from WHEELMOUSEUP... decreases len */
		if (*chainlen > 0) (*chainlen)--;
	}
	
	/* sanity checks (don't assume t->poseobj is set, or that it is an armature) */
	if (ELEM(NULL, t->poseobj, t->poseobj->pose))
		return;
	
	/* apply to all pose-channels */
	for (pchan=t->poseobj->pose->chanbase.first; pchan; pchan=pchan->next) {
		pchan_autoik_adjust(pchan, *chainlen);
	}	
}

/* frees temporal IKs */
static void pose_grab_with_ik_clear(Object *ob)
{
	bKinematicConstraint *data;
	bPoseChannel *pchan;
	bConstraint *con;
	
	for (pchan= ob->pose->chanbase.first; pchan; pchan= pchan->next) {
		/* clear all temporary lock flags */
		pchan->ikflag &= ~(BONE_IK_NO_XDOF_TEMP|BONE_IK_NO_YDOF_TEMP|BONE_IK_NO_ZDOF_TEMP);
		
		/* remove all temporary IK-constraints added */
		for (con= pchan->constraints.first; con; con= con->next) {
			if (con->type==CONSTRAINT_TYPE_KINEMATIC) {
				data= con->data;
				if (data->flag & CONSTRAINT_IK_TEMP) {
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

/* adds the IK to pchan - returns if added */
static short pose_grab_with_ik_add(bPoseChannel *pchan)
{
	bKinematicConstraint *data;
	bConstraint *con;
	
	/* Sanity check */
	if (pchan == NULL) 
		return 0;
	
	/* Rule: not if there's already an IK on this channel */
	for (con= pchan->constraints.first; con; con= con->next) {
		if (con->type==CONSTRAINT_TYPE_KINEMATIC)
			break;
	}
	
	if (con) {
		/* but, if this is a targetless IK, we make it auto anyway (for the children loop) */
		data= has_targetless_ik(pchan);
		if (data)
			data->flag |= CONSTRAINT_IK_AUTO;
		return 0;
	}
	
	con = add_new_constraint(CONSTRAINT_TYPE_KINEMATIC);
	BLI_addtail(&pchan->constraints, con);
	pchan->constflag |= (PCHAN_HAS_IK|PCHAN_HAS_TARGET);	/* for draw, but also for detecting while pose solving */
	data= con->data;
	data->flag= CONSTRAINT_IK_TIP|CONSTRAINT_IK_TEMP|CONSTRAINT_IK_AUTO;
	VECCOPY(data->grabtarget, pchan->pose_tail);
	data->rootbone= 1;
	
	/* we include only a connected chain */
	while ((pchan) && (pchan->bone->flag & BONE_CONNECTED)) {
		/* here, we set ik-settings for bone from pchan->protectflag */
		if (pchan->protectflag & OB_LOCK_ROTX) pchan->ikflag |= BONE_IK_NO_XDOF_TEMP;
		if (pchan->protectflag & OB_LOCK_ROTY) pchan->ikflag |= BONE_IK_NO_YDOF_TEMP;
		if (pchan->protectflag & OB_LOCK_ROTZ) pchan->ikflag |= BONE_IK_NO_ZDOF_TEMP;
		
		/* now we count this pchan as being included */
		data->rootbone++;
		pchan= pchan->parent;
	}
	
	/* make a copy of maximum chain-length */
	data->max_rootbone= data->rootbone;
	
	return 1;
}

/* bone is a candidate to get IK, but we don't do it if it has children connected */
static short pose_grab_with_ik_children(bPose *pose, Bone *bone)
{
	Bone *bonec;
	short wentdeeper=0, added=0;

	/* go deeper if children & children are connected */
	for (bonec= bone->childbase.first; bonec; bonec= bonec->next) {
		if (bonec->flag & BONE_CONNECTED) {
			wentdeeper= 1;
			added+= pose_grab_with_ik_children(pose, bonec);
		}
	}
	if (wentdeeper==0) {
		bPoseChannel *pchan= get_pose_channel(pose, bone->name);
		if (pchan)
			added+= pose_grab_with_ik_add(pchan);
	}
	
	return added;
}

/* main call which adds temporal IK chains */
static short pose_grab_with_ik(Object *ob)
{
	bArmature *arm;
	bPoseChannel *pchan, *parent;
	Bone *bonec;
	short tot_ik= 0;
	
	if ((ob==NULL) || (ob->pose==NULL) || (ob->flag & OB_POSEMODE)==0)
		return 0;
		
	arm = ob->data;
	
	/* Rule: allow multiple Bones (but they must be selected, and only one ik-solver per chain should get added) */
	for (pchan= ob->pose->chanbase.first; pchan; pchan= pchan->next) {
		if (pchan->bone->layer & arm->layer) {
			if (pchan->bone->flag & BONE_SELECTED) {
				/* Rule: no IK for solitatry (unconnected) bones */
				for (bonec=pchan->bone->childbase.first; bonec; bonec=bonec->next) {
					if (bonec->flag & BONE_CONNECTED) {
						break;
					}
				}
				if ((pchan->bone->flag & BONE_CONNECTED)==0 && (bonec == NULL))
					continue;
				
				/* rule: if selected Bone is not a root bone, it gets a temporal IK */
				if (pchan->parent) {
					/* only adds if there's no IK yet (and no parent bone was selected) */
					for (parent= pchan->parent; parent; parent= parent->parent) {
						if (parent->bone->flag & BONE_SELECTED)
							break;
					}
					if (parent == NULL)
						tot_ik += pose_grab_with_ik_add(pchan);
				}
				else {
					/* rule: go over the children and add IK to the tips */
					tot_ik += pose_grab_with_ik_children(ob->pose, pchan->bone);
				}
			}
		}
	}
	
	return (tot_ik) ? 1 : 0;
}	


/* only called with pose mode active object now */
static void createTransPose(TransInfo *t, Object *ob)
{
	bArmature *arm;
	bPoseChannel *pchan;
	TransData *td;
	TransDataExtension *tdx;
	short ik_on= 0;
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
	if ((arm->flag & ARM_AUTO_IK) && t->mode==TFM_TRANSLATION) {
		ik_on= pose_grab_with_ik(ob);
		if (ik_on) t->flag |= T_AUTOIK;
	}
	
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
	
	/* initialise initial auto=ik chainlen's? */
	if (ik_on) transform_autoik_update(t, 0);
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
					if (ebo->flag & BONE_EDITMODE_LOCKED)
						td->protectflag = OB_LOCK_LOC|OB_LOCK_ROT|OB_LOCK_SCALE;

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
					if (ebo->flag & BONE_EDITMODE_LOCKED)
						td->protectflag = OB_LOCK_LOC|OB_LOCK_ROT|OB_LOCK_SCALE;

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

/* Utility function for getting the handle data from bezier's */
TransDataCurveHandleFlags *initTransDataCurveHandes(TransData *td, struct BezTriple *bezt) {
	TransDataCurveHandleFlags *hdata;
	td->flag |= TD_BEZTRIPLE;
	hdata = td->hdata = MEM_mallocN(sizeof(TransDataCurveHandleFlags), "CuHandle Data");
	hdata->ih1 = bezt->h1;
	hdata->h1 = &bezt->h1;
	hdata->ih2 = bezt->h2; /* incase the second is not selected */
	hdata->h2 = &bezt->h2;
	return hdata;
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
					if (G.f & G_HIDDENHANDLES) {
						if(bezt->f2 & SELECT) countsel+=3;
						if(propmode) count+= 3;
					} else {
						if(bezt->f1 & SELECT) countsel++;
						if(bezt->f2 & SELECT) countsel++;
						if(bezt->f3 & SELECT) countsel++;
						if(propmode) count+= 3;
					}
				}
			}
		}
		else {
			for(a= nu->pntsu*nu->pntsv, bp= nu->bp; a>0; a--, bp++) {
				if(bp->hide==0) {
					if(propmode) count++;
					if(bp->f1 & SELECT) countsel++;
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
					TransDataCurveHandleFlags *hdata = NULL;
					
					if(		propmode ||
							((bezt->f2 & SELECT) && (G.f & G_HIDDENHANDLES)) ||
							((bezt->f1 & SELECT) && (G.f & G_HIDDENHANDLES)==0)
					  ) {
						VECCOPY(td->iloc, bezt->vec[0]);
						td->loc= bezt->vec[0];
						VECCOPY(td->center, bezt->vec[1]);
						if (G.f & G_HIDDENHANDLES) {
							if(bezt->f2 & SELECT) td->flag= TD_SELECTED;
							else td->flag= 0;
						} else {
							if(bezt->f1 & SELECT) td->flag= TD_SELECTED;
							else td->flag= 0;
						}
						td->ext = NULL;
						td->tdi = NULL;
						td->val = NULL;
						
						hdata = initTransDataCurveHandes(td, bezt);

						Mat3CpyMat3(td->smtx, smtx);
						Mat3CpyMat3(td->mtx, mtx);

						td++;
						count++;
						tail++;
					}
					
					/* This is the Curve Point, the other two are handles */
					if(propmode || (bezt->f2 & SELECT)) {
						VECCOPY(td->iloc, bezt->vec[1]);
						td->loc= bezt->vec[1];
						VECCOPY(td->center, td->loc);
						if(bezt->f2 & SELECT) td->flag= TD_SELECTED;
						else td->flag= 0;
						td->ext = NULL;
						td->tdi = NULL;
						
						if (t->mode==TFM_CURVE_SHRINKFATTEN) { /* || t->mode==TFM_RESIZE) {*/ /* TODO - make points scale */
							td->val = &(bezt->radius);
							td->ival = bezt->radius;
						} else if (t->mode==TFM_TILT) {
							td->val = &(bezt->alfa);
							td->ival = bezt->alfa;
						} else {
							td->val = NULL;
						}

						Mat3CpyMat3(td->smtx, smtx);
						Mat3CpyMat3(td->mtx, mtx);
						
						if ((bezt->f1&SELECT)==0 && (bezt->f3&SELECT)==0)
						/* If the middle is selected but the sides arnt, this is needed */
						if (hdata==NULL) { /* if the handle was not saved by the previous handle */
							hdata = initTransDataCurveHandes(td, bezt);
						}
						
						td++;
						count++;
						tail++;
					}
					if(		propmode ||
							((bezt->f2 & SELECT) && (G.f & G_HIDDENHANDLES)) ||
							((bezt->f3 & SELECT) && (G.f & G_HIDDENHANDLES)==0)
					  ) {
						VECCOPY(td->iloc, bezt->vec[2]);
						td->loc= bezt->vec[2];
						VECCOPY(td->center, bezt->vec[1]);
						if (G.f & G_HIDDENHANDLES) {
							if(bezt->f2 & SELECT) td->flag= TD_SELECTED;
							else td->flag= 0;
						} else {
							if(bezt->f3 & SELECT) td->flag= TD_SELECTED;
							else td->flag= 0;
						}
						td->ext = NULL;
						td->tdi = NULL;
						td->val = NULL;

						if (hdata==NULL) { /* if the handle was not saved by the previous handle */
							hdata = initTransDataCurveHandes(td, bezt);
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
			
			testhandlesNurb(nu); /* sets the handles based on their selection, do this after the data is copied to the TransData */
		}
		else {
			TransData *head, *tail;
			head = tail = td;
			for(a= nu->pntsu*nu->pntsv, bp= nu->bp; a>0; a--, bp++) {
				if(bp->hide==0) {
					if(propmode || (bp->f1 & SELECT)) {
						VECCOPY(td->iloc, bp->vec);
						td->loc= bp->vec;
						VECCOPY(td->center, td->loc);
						if(bp->f1 & SELECT) td->flag= TD_SELECTED;
						else td->flag= 0;
						td->ext = NULL;
						td->tdi = NULL;
						
						if (t->mode==TFM_CURVE_SHRINKFATTEN || t->mode==TFM_RESIZE) {
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
			if(bp->f1 & SELECT) countsel++;
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
		if(propmode || (bp->f1 & SELECT)) {
			if(bp->hide==0) {
				VECCOPY(td->iloc, bp->vec);
				td->loc= bp->vec;
				VECCOPY(td->center, td->loc);
				if(bp->f1 & SELECT) td->flag= TD_SELECTED;
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

/* ******************* particle edit **************** */
static void createTransParticleVerts(TransInfo *t)
{
	TransData *td = NULL;
	TransDataExtension *tx;
	Base *base = BASACT;
	Object *ob = OBACT;
	ParticleSystem *psys = PE_get_current(ob);
	ParticleSystemModifierData *psmd = NULL;
	ParticleEditSettings *pset = PE_settings();
	ParticleData *pa = NULL;
	ParticleEdit *edit;
	ParticleEditKey *key;
	float mat[4][4];
	int i,k, totpart, transformparticle;
	int count = 0, hasselected = 0;
	int propmode = t->flag & T_PROP_EDIT;

	if(psys==NULL || G.scene->selectmode==SCE_SELECT_PATH) return;

	psmd = psys_get_modifier(ob,psys);

	edit = psys->edit;
	totpart = psys->totpart;
	base->flag |= BA_HAS_RECALC_DATA;

	for(i=0, pa=psys->particles; i<totpart; i++, pa++) {
		pa->flag &= ~PARS_TRANSFORM;
		transformparticle= 0;

		if((pa->flag & PARS_HIDE)==0) {
			for(k=0, key=edit->keys[i]; k<pa->totkey; k++, key++) {
				if((key->flag&PEK_HIDE)==0) {
					if(key->flag&PEK_SELECT) {
						hasselected= 1;
						transformparticle= 1;
					}
					else if(propmode)
						transformparticle= 1;
				}
			}
		}

		if(transformparticle) {
			count += pa->totkey;
			pa->flag |= PARS_TRANSFORM;
		}
	}
	
 	/* note: in prop mode we need at least 1 selected */
	if (hasselected==0) return;
	
	t->total = count;
	td = t->data = MEM_callocN(t->total * sizeof(TransData), "TransObData(Particle Mode)");

	if(t->mode == TFM_BAKE_TIME)
		tx = t->ext = MEM_callocN(t->total * sizeof(TransDataExtension), "Particle_TransExtension");
	else
		tx = t->ext = NULL;

	Mat4One(mat);

	Mat4Invert(ob->imat,ob->obmat);

	for(i=0, pa=psys->particles; i<totpart; i++, pa++) {
		TransData *head, *tail;
		head = tail = td;

		if(!(pa->flag & PARS_TRANSFORM)) continue;

		psys_mat_hair_to_global(ob, psmd->dm, psys->part->from, pa, mat);

		for(k=0, key=edit->keys[i]; k<pa->totkey; k++, key++) {
			VECCOPY(key->world_co, key->co);
			Mat4MulVecfl(mat, key->world_co);
			td->loc = key->world_co;

			VECCOPY(td->iloc, td->loc);
			VECCOPY(td->center, td->loc);

			if(key->flag & PEK_SELECT)
				td->flag |= TD_SELECTED;
			else if(!propmode)
				td->flag |= TD_SKIP;

			Mat3One(td->mtx);
			Mat3One(td->smtx);

			/* don't allow moving roots */
			if(k==0 && pset->flag & PE_LOCK_FIRST)
				td->protectflag |= OB_LOCK_LOC;

			td->ob = ob;
			td->ext = tx;
			td->tdi = NULL;
			if(t->mode == TFM_BAKE_TIME) {
				td->val = key->time;
				td->ival = *(key->time);
				/* abuse size and quat for min/max values */
				td->flag |= TD_NO_EXT;
				if(k==0) tx->size = 0;
				else tx->size = (key - 1)->time;

				if(k == pa->totkey - 1) tx->quat = 0;
				else tx->quat = (key + 1)->time;
			}

			td++;
			if(tx)
				tx++;
			tail++;
		}
		if (propmode && head != tail)
			calc_distanceCurveVerts(head, tail - 1);
	}
}

void flushTransParticles(TransInfo *t)
{
	Object *ob = OBACT;
	ParticleSystem *psys = PE_get_current(ob);
	ParticleSystemModifierData *psmd;
	ParticleData *pa;
	ParticleEditKey *key;
	TransData *td;
	float mat[4][4], imat[4][4], co[3];
	int i, k, propmode = t->flag & T_PROP_EDIT;

	psmd = psys_get_modifier(ob, psys);

	/* we do transform in world space, so flush world space position
	 * back to particle local space */
	td= t->data;
	for(i=0, pa=psys->particles; i<psys->totpart; i++, pa++, td++) {
		if(!(pa->flag & PARS_TRANSFORM)) continue;

		psys_mat_hair_to_global(ob, psmd->dm, psys->part->from, pa, mat);
		Mat4Invert(imat,mat);

		for(k=0, key=psys->edit->keys[i]; k<pa->totkey; k++, key++) {
			VECCOPY(co, key->world_co);
			Mat4MulVecfl(imat, co);

			/* optimization for proportional edit */
			if(!propmode || !FloatCompare(key->co, co, 0.0001f)) {
				VECCOPY(key->co, co);
				pa->flag |= PARS_EDIT_RECALC;
			}
		}
	}

	PE_update_object(OBACT, 1);
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

//way to overwrite what data is edited with transform
//static void VertsToTransData(TransData *td, EditVert *eve, BakeKey *key)
static void VertsToTransData(TransData *td, EditVert *eve)
{
	td->flag = 0;
	//if(key)
	//	td->loc = key->co;
	//else
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
	td->tdmir = NULL;
	if (BIF_GetTransInfo()->mode == TFM_BWEIGHT) {
		td->val = &(eve->bweight);
		td->ival = eve->bweight;
	}

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

static int modifiers_disable_subsurf_temporary(Object *ob)
{
	ModifierData *md;
	int disabled = 0;
	
	for(md=ob->modifiers.first; md; md=md->next)
		if(md->type==eModifierType_Subsurf)
			if(md->mode & eModifierMode_OnCage) {
				md->mode ^= eModifierMode_DisableTemporary;
				disabled= 1;
			}
	
	return disabled;
}

/* disable subsurf temporal, get mapped cos, and enable it */
static float *get_crazy_mapped_editverts(void)
{
	DerivedMesh *dm;
	float *vertexcos;

	/* disable subsurf temporal, get mapped cos, and enable it */
	if(modifiers_disable_subsurf_temporary(G.obedit)) {
		/* need to make new derivemesh */
		makeDerivedMesh(G.obedit, CD_MASK_BAREMESH);
	}

	/* now get the cage */
	dm= editmesh_get_derived_cage(CD_MASK_BAREMESH);

	vertexcos= MEM_mallocN(3*sizeof(float)*G.totvert, "vertexcos map");
	dm->foreachMappedVert(dm, make_vertexcos__mapFunc, vertexcos);
	
	dm->release(dm);
	
	/* set back the flag, no new cage needs to be built, transform does it */
	modifiers_disable_subsurf_temporary(G.obedit);
	
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

static void set_crazyspace_quats(float *origcos, float *mappedcos, float *quats)
{
	EditMesh *em = G.editMesh;
	EditVert *eve, *prev;
	EditFace *efa;
	float *v1, *v2, *v3, *v4, *co1, *co2, *co3, *co4;
	long index= 0;
	
	/* two abused locations in vertices */
	for(eve= em->verts.first; eve; eve= eve->next, index++) {
		eve->tmp.p = NULL;
		eve->prev= (EditVert *)index;
	}
	
	/* first store two sets of tangent vectors in vertices, we derive it just from the face-edges */
	for(efa= em->faces.first; efa; efa= efa->next) {
		
		/* retrieve mapped coordinates */
		v1= mappedcos + 3*(long)(efa->v1->prev);
		v2= mappedcos + 3*(long)(efa->v2->prev);
		v3= mappedcos + 3*(long)(efa->v3->prev);

		co1= (origcos)? origcos + 3*(long)(efa->v1->prev): efa->v1->co;
		co2= (origcos)? origcos + 3*(long)(efa->v2->prev): efa->v2->co;
		co3= (origcos)? origcos + 3*(long)(efa->v3->prev): efa->v3->co;

		if(efa->v2->tmp.p==NULL && efa->v2->f1) {
			set_crazy_vertex_quat(quats, co2, co3, co1, v2, v3, v1);
			efa->v2->tmp.p= (void*)quats;
			quats+= 4;
		}
		
		if(efa->v4) {
			v4= mappedcos + 3*(long)(efa->v4->prev);
			co4= (origcos)? origcos + 3*(long)(efa->v4->prev): efa->v4->co;

			if(efa->v1->tmp.p==NULL && efa->v1->f1) {
				set_crazy_vertex_quat(quats, co1, co2, co4, v1, v2, v4);
				efa->v1->tmp.p= (void*)quats;
				quats+= 4;
			}
			if(efa->v3->tmp.p==NULL && efa->v3->f1) {
				set_crazy_vertex_quat(quats, co3, co4, co2, v3, v4, v2);
				efa->v3->tmp.p= (void*)quats;
				quats+= 4;
			}
			if(efa->v4->tmp.p==NULL && efa->v4->f1) {
				set_crazy_vertex_quat(quats, co4, co1, co3, v4, v1, v3);
				efa->v4->tmp.p= (void*)quats;
				quats+= 4;
			}
		}
		else {
			if(efa->v1->tmp.p==NULL && efa->v1->f1) {
				set_crazy_vertex_quat(quats, co1, co2, co3, v1, v2, v3);
				efa->v1->tmp.p= (void*)quats;
				quats+= 4;
			}
			if(efa->v3->tmp.p==NULL && efa->v3->f1) {
				set_crazy_vertex_quat(quats, co3, co1, co2, v3, v1, v2);
				efa->v3->tmp.p= (void*)quats;
				quats+= 4;
			}
		}
	}

	/* restore abused prev pointer */
	for(prev= NULL, eve= em->verts.first; eve; prev= eve, eve= eve->next)
		eve->prev= prev;

}

void createTransBMeshVerts(TransInfo *t, BME_Mesh *bm, BME_TransData_Head *td) {
	BME_Vert *v;
	BME_TransData *vtd;
	TransData *tob;
	int i;

	tob = t->data = MEM_callocN(td->len*sizeof(TransData), "TransObData(Bevel tool)");

	for (i=0,v=bm->verts.first;v;v=v->next) {
		if ( (vtd = BME_get_transdata(td,v)) ) {
			tob->loc = vtd->loc;
			tob->val = &vtd->factor;
			VECCOPY(tob->iloc,vtd->co);
			VECCOPY(tob->center,vtd->org);
			VECCOPY(tob->axismtx[0],vtd->vec);
			tob->axismtx[1][0] = vtd->max ? *vtd->max : 0;
			tob++;
			i++;
		}
	}
	/* since td is a memarena, it can hold more transdata than actual elements
	 * (i.e. we can't depend on td->len to determine the number of actual elements) */
	t->total = i;
}

static void createTransEditVerts(TransInfo *t)
{
	TransData *tob = NULL;
	EditMesh *em = G.editMesh;
	EditVert *eve;
	EditVert **nears = NULL;
	EditVert *eve_act = NULL;
	float *vectors = NULL, *mappedcos = NULL, *quats= NULL;
	float mtx[3][3], smtx[3][3], (*defmats)[3][3] = NULL, (*defcos)[3] = NULL;
	int count=0, countsel=0, a, totleft;
	int propmode = t->flag & T_PROP_EDIT;
	int mirror = 0;
	
	if ((t->context & CTX_NO_MIRROR) == 0 && (G.scene->toolsettings->editbutflag & B_MESH_X_MIRROR))
	{
		mirror = 1;
	}

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
	
	/* check active */
	if (G.editMesh->selected.last) {
		EditSelection *ese = G.editMesh->selected.last;
		if ( ese->type == EDITVERT ) {
			eve_act = (EditVert *)ese->data;
		}
	}

	
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
				/* check if we can use deform matrices for modifier from the
				   start up to stack, they are more accurate than quats */
				totleft= editmesh_get_first_deform_matrices(&defmats, &defcos);

				/* if we still have more modifiers, also do crazyspace
				   correction with quats, relative to the coordinates after
				   the modifiers that support deform matrices (defcos) */
				if(totleft > 0) {
					mappedcos= get_crazy_mapped_editverts();
					quats= MEM_mallocN( (t->total)*sizeof(float)*4, "crazy quats");
					set_crazyspace_quats((float*)defcos, mappedcos, quats);
					if(mappedcos)
						MEM_freeN(mappedcos);
				}

				if(defcos)
					MEM_freeN(defcos);
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
	
	for (a=0, eve=em->verts.first; eve; eve=eve->next, a++) {
		if(eve->h==0) {
			if(propmode || eve->f1) {
				VertsToTransData(tob, eve);
				
				/* selected */
				if(eve->f1) tob->flag |= TD_SELECTED;
				
				/* active */
				if(eve == eve_act) tob->flag |= TD_ACTIVE;
				
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
				if(defmats || (quats && eve->tmp.p)) {
					float mat[3][3], imat[3][3], qmat[3][3];
					
					/* use both or either quat and defmat correction */
					if(quats && eve->tmp.f) {
						QuatToMat3(eve->tmp.p, qmat);

						if(defmats)
							Mat3MulSerie(mat, mtx, qmat, defmats[a],
								NULL, NULL, NULL, NULL, NULL);
						else
							Mat3MulMat3(mat, mtx, qmat);
					}
					else
						Mat3MulMat3(mat, mtx, defmats[a]);

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
					if(vmir != eve) tob->tdmir = vmir;
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
	if(quats)
		MEM_freeN(quats);
	if(defmats)
		MEM_freeN(defmats);
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
	else {
		td->dist= MAXFLOAT;
	}
	Mat3One(td->mtx);
	Mat3One(td->smtx);
}

static void createTransUVs(TransInfo *t)
{
	TransData *td = NULL;
	TransData2D *td2d = NULL;
	MTFace *tf;
	int count=0, countsel=0;
	int propmode = t->flag & T_PROP_EDIT;
	int efa_s1,efa_s2,efa_s3,efa_s4;

	EditMesh *em = G.editMesh;
	EditFace *efa;
	
	if(is_uv_tface_editing_allowed()==0) return;

	/* count */
	if (G.sima->flag & SI_BE_SQUARE && !propmode) {
		for (efa= em->faces.first; efa; efa= efa->next) {
			/* store face pointer for second loop, prevent second lookup */
			tf= CustomData_em_get(&em->fdata, efa->data, CD_MTFACE);
			if (simaFaceDraw_Check(efa, tf)) {
				efa->tmp.p = tf;
				
				efa_s1 = simaUVSel_Check(efa, tf, 0);
				efa_s2 = simaUVSel_Check(efa, tf, 1);
				efa_s3 = simaUVSel_Check(efa, tf, 2);
				if (efa->v4) {
					efa_s4 = simaUVSel_Check(efa, tf, 3);
					if ( efa_s1 || efa_s2 || efa_s3 || efa_s4 ) {
						countsel += 4; /* all corners of this quad need their edges moved. so we must store TD for each */
					}
				} else {
					/* tri's are delt with normally when SI_BE_SQUARE's enabled */
					if (efa_s1) countsel++; 
					if (efa_s2) countsel++; 
					if (efa_s3) countsel++;
				}
			} else {
				efa->tmp.p = NULL;
			}
		}
	} else {
		for (efa= em->faces.first; efa; efa= efa->next) {
			tf= CustomData_em_get(&em->fdata, efa->data, CD_MTFACE);
			if (simaFaceDraw_Check(efa, tf)) {
				efa->tmp.p = tf;
				
				if (simaUVSel_Check(efa, tf, 0)) countsel++; 
				if (simaUVSel_Check(efa, tf, 1)) countsel++; 
				if (simaUVSel_Check(efa, tf, 2)) countsel++; 
				if (efa->v4 && simaUVSel_Check(efa, tf, 3)) countsel++;
				if(propmode)
					count += (efa->v4)? 4: 3;
			} else {
				efa->tmp.p = NULL;
			}
		}
	}
	
 	/* note: in prop mode we need at least 1 selected */
	if (countsel==0) return;
	
	t->total= (propmode)? count: countsel;
	t->data= MEM_callocN(t->total*sizeof(TransData), "TransObData(UV Editing)");
	/* for each 2d uv coord a 3d vector is allocated, so that they can be
	   treated just as if they were 3d verts */
	t->data2d= MEM_callocN(t->total*sizeof(TransData2D), "TransObData2D(UV Editing)");

	if(G.sima->flag & SI_CLIP_UV)
		t->flag |= T_CLIP_UV;

	td= t->data;
	td2d= t->data2d;
	
	if (G.sima->flag & SI_BE_SQUARE && !propmode) {
		for (efa= em->faces.first; efa; efa= efa->next) {
			tf=(MTFace *)efa->tmp.p;
			if (tf) {
				efa_s1 = simaUVSel_Check(efa, tf, 0);
				efa_s2 = simaUVSel_Check(efa, tf, 1);
				efa_s3 = simaUVSel_Check(efa, tf, 2);
				
				if (efa->v4) {
					efa_s4 = simaUVSel_Check(efa, tf, 3);
					
					if ( efa_s1 || efa_s2 || efa_s3 || efa_s4 ) {
						/* all corners of this quad need their edges moved. so we must store TD for each */

						UVsToTransData(td, td2d, tf->uv[0], efa_s1);
						if (!efa_s1)	td->flag |= TD_SKIP;
						td++; td2d++;

						UVsToTransData(td, td2d, tf->uv[1], efa_s2);
						if (!efa_s2)	td->flag |= TD_SKIP;
						td++; td2d++;

						UVsToTransData(td, td2d, tf->uv[2], efa_s3);
						if (!efa_s3)	td->flag |= TD_SKIP;
						td++; td2d++;

						UVsToTransData(td, td2d, tf->uv[3], efa_s4);
						if (!efa_s4)	td->flag |= TD_SKIP;
						td++; td2d++;
					}
				} else {
					/* tri's are delt with normally when SI_BE_SQUARE's enabled */
					if (efa_s1) UVsToTransData(td++, td2d++, tf->uv[0], 1); 
					if (efa_s2) UVsToTransData(td++, td2d++, tf->uv[1], 1); 
					if (efa_s3) UVsToTransData(td++, td2d++, tf->uv[2], 1);
				}
			}
		}
	} else {
		for (efa= em->faces.first; efa; efa= efa->next) {
			/*tf= CustomData_em_get(&em->fdata, efa->data, CD_MTFACE);
			if (simaFaceDraw_Check(efa, tf)) {*/
			if ((tf=(MTFace *)efa->tmp.p)) {
				if (propmode) {
					UVsToTransData(td++, td2d++, tf->uv[0], simaUVSel_Check(efa, tf, 0));
					UVsToTransData(td++, td2d++, tf->uv[1], simaUVSel_Check(efa, tf, 1));
					UVsToTransData(td++, td2d++, tf->uv[2], simaUVSel_Check(efa, tf, 2));
					if(efa->v4)
						UVsToTransData(td++, td2d++, tf->uv[3], simaUVSel_Check(efa, tf, 3));
				} else {
					if(simaUVSel_Check(efa, tf, 0))				UVsToTransData(td++, td2d++, tf->uv[0], 1);
					if(simaUVSel_Check(efa, tf, 1))				UVsToTransData(td++, td2d++, tf->uv[1], 1);
					if(simaUVSel_Check(efa, tf, 2))				UVsToTransData(td++, td2d++, tf->uv[2], 1);
					if(efa->v4 && simaUVSel_Check(efa, tf, 3))	UVsToTransData(td++, td2d++, tf->uv[3], 1);
				}
			}
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
	EditMesh *em = G.editMesh;
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
	
	if((G.sima->flag & SI_BE_SQUARE) && (t->flag & T_PROP_EDIT)==0 && (t->state != TRANS_CANCEL)) 
		be_square_tface_uv(em);

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

/* ********************* IPO EDITOR ************************* */

/* for IPO Editor transform - but actual creation of transform structures is not performed here
 * due to bad globals that would need to be imported specially for this
 */
static void createTransIpoData(TransInfo *t)
{
	/* in editipo.c due to some globals that are defined in that file... */
	make_ipo_transdata(t);
}

/* this function is called on recalcData to apply the transforms applied
 * to the transdata on to the actual keyframe data 
 */
void flushTransIpoData(TransInfo *t)
{
	TransData2D *td;
	int a;
	
	/* flush to 2d vector from internally used 3d vector */
	for (a=0, td= t->data2d; a<t->total; a++, td++) {
		/* we need to unapply the nla-scaling from the time in some situations */
		if (NLA_IPO_SCALED)
			td->loc2d[0]= get_action_frame(OBACT, td->loc[0]);
		else
			td->loc2d[0]= td->loc[0];
		
		/* when the icu that point comes from is a bitflag holder, don't allow adjusting values */
		if ((t->data[a].flag & TD_TIMEONLY)==0)
			td->loc2d[1]= td->loc[1];
	}
}

/* ********************* ACTION/NLA EDITOR ****************** */

/* Called by special_aftertrans_update to make sure selected gp-frames replace
 * any other gp-frames which may reside on that frame (that are not selected).
 * It also makes sure gp-frames are still stored in chronological order after
 * transform.
 */
static void posttrans_gpd_clean (bGPdata *gpd)
{
	bGPDlayer *gpl;
	
	for (gpl= gpd->layers.first; gpl; gpl= gpl->next) {
		ListBase sel_buffer = {NULL, NULL};
		bGPDframe *gpf, *gpfn;
		bGPDframe *gfs, *gfsn;
		
		/* loop 1: loop through and isolate selected gp-frames to buffer 
		 * (these need to be sorted as they are isolated)
		 */
		for (gpf= gpl->frames.first; gpf; gpf= gpfn) {
			gpfn= gpf->next;
			
			if (gpf->flag & GP_FRAME_SELECT) {
				BLI_remlink(&gpl->frames, gpf);
				
				/* find place to add them in buffer
				 * - go backwards as most frames will still be in order,
				 *   so doing it this way will be faster 
				 */
				for (gfs= sel_buffer.last; gfs; gfs= gfs->prev) {
					/* if current (gpf) occurs after this one in buffer, add! */
					if (gfs->framenum < gpf->framenum) {
						BLI_insertlinkafter(&sel_buffer, gfs, gpf);
						break;
					}
				}
				if (gfs == NULL)
					BLI_addhead(&sel_buffer, gpf);
			}
		}
		
		/* error checking: it is unlikely, but may be possible to have none selected */
		if (sel_buffer.first == NULL)
			continue;
		
		/* if all were selected (i.e. gpl->frames is empty), then just transfer sel-buf over */
		if (gpl->frames.first == NULL) {
			gpl->frames.first= sel_buffer.first;
			gpl->frames.last= sel_buffer.last;
			
			continue;
		}
		
		/* loop 2: remove duplicates of frames in buffers */
		//gfs= sel_buffer.first;
		//gfsn= gfs->next;
		
		for (gpf= gpl->frames.first; gpf && sel_buffer.first; gpf= gpfn) {
			gpfn= gpf->next;
			 
			/* loop through sel_buffer, emptying stuff from front of buffer if ok */
			for (gfs= sel_buffer.first; gfs && gpf; gfs= gfsn) {
				gfsn= gfs->next;
				
				/* if this buffer frame needs to go before current, add it! */
				if (gfs->framenum < gpf->framenum) {
					/* transfer buffer frame to frames list (before current) */
					BLI_remlink(&sel_buffer, gfs);
					BLI_insertlinkbefore(&gpl->frames, gpf, gfs);
				}
				/* if this buffer frame is on same frame, replace current with it and stop */
				else if (gfs->framenum == gpf->framenum) {
					/* transfer buffer frame to frames list (before current) */
					BLI_remlink(&sel_buffer, gfs);
					BLI_insertlinkbefore(&gpl->frames, gpf, gfs);
					
					/* get rid of current frame */
					gpencil_layer_delframe(gpl, gpf);
				}
			}
		}
		
		/* if anything is still in buffer, append to end */
		for (gfs= sel_buffer.first; gfs; gfs= gfsn) {
			gfsn= gfs->next;
			
			BLI_remlink(&sel_buffer, gfs);
			BLI_addtail(&gpl->frames, gfs);
		}
	}
}

/* Called by special_aftertrans_update to make sure selected keyframes replace
 * any other keyframes which may reside on that frame (that is not selected).
 */
static void posttrans_ipo_clean (Ipo *ipo)
{
	IpoCurve *icu;
	int i;
	
	/* delete any keyframes that occur on same frame as selected keyframe, but is not selected */
	for (icu= ipo->curve.first; icu; icu= icu->next) {
		float *selcache;	/* cache for frame numbers of selected frames (icu->totvert*sizeof(float)) */
		int len, index;		/* number of frames in cache, item index */
		
		/* allocate memory for the cache */
		// TODO: investigate using GHash for this instead?
		if (icu->totvert == 0) 
			continue;
		selcache= MEM_callocN(sizeof(float)*icu->totvert, "IcuSelFrameNums");
		len= 0;
		index= 0;
		
		/* We do 2 loops, 1 for marking keyframes for deletion, one for deleting 
		 * as there is no guarantee what order the keyframes are exactly, even though 
		 * they have been sorted by time.
		 */
		 
		/*	Loop 1: find selected keyframes   */
		for (i = 0; i < icu->totvert; i++) {
			BezTriple *bezt= &icu->bezt[i];
			
			if (BEZSELECTED(bezt)) {
				selcache[index]= bezt->vec[1][0];
				index++;
				len++;
			}
		}
		
		/* Loop 2: delete unselected keyframes on the same frames (if any keyframes were found) */
		if (len) {
			for (i = 0; i < icu->totvert; i++) {
				BezTriple *bezt= &icu->bezt[i];
				
				if (BEZSELECTED(bezt) == 0) {
					/* check beztriple should be removed according to cache */
					for (index= 0; index < len; index++) {
						if (IS_EQ(bezt->vec[1][0], selcache[index])) {
							delete_icu_key(icu, i, 0);
							break;
						}
						else if (bezt->vec[1][0] > selcache[index])
							break;
					}
				}
			}
			
			testhandles_ipocurve(icu);
		}
		
		/* free cache */
		MEM_freeN(selcache);
	}
}

/* Called by special_aftertrans_update to make sure selected keyframes replace
 * any other keyframes which may reside on that frame (that is not selected).
 * remake_action_ipos should have already been called 
 */
static void posttrans_action_clean (bAction *act)
{
	ListBase act_data = {NULL, NULL};
	bActListElem *ale;
	int filter;
	
	/* filter data */
	filter= (ACTFILTER_VISIBLE | ACTFILTER_FOREDIT | ACTFILTER_IPOKEYS);
	actdata_filter(&act_data, filter, act, ACTCONT_ACTION);
	
	/* loop through relevant data, removing keyframes from the ipo-blocks that were attached 
	 *  	- all keyframes are converted in/out of global time 
	 */
	for (ale= act_data.first; ale; ale= ale->next) {
		if (NLA_ACTION_SCALED) {
			actstrip_map_ipo_keys(OBACT, ale->key_data, 0, 1); 
			posttrans_ipo_clean(ale->key_data);
			actstrip_map_ipo_keys(OBACT, ale->key_data, 1, 1);
		}
		else 
			posttrans_ipo_clean(ale->key_data);
	}
	
	/* free temp data */
	BLI_freelistN(&act_data);
}

/* Called by special_aftertrans_update to make sure selected keyframes replace
 * any other keyframes which may reside on that frame (that is not selected).
 * remake_all_ipos should have already been called 
 */
static void posttrans_nla_clean (TransInfo *t)
{
	Base *base;
	Object *ob;
	bActionStrip *strip;
	bActionChannel *achan;
	bConstraintChannel *conchan;
	float cfra;
	char side;
	int i;
	
	/* which side of the current frame should be allowed */
	if (t->mode == TFM_TIME_EXTEND) {
		/* only side on which mouse is gets transformed */
		float xmouse, ymouse;
		
		areamouseco_to_ipoco(G.v2d, t->imval, &xmouse, &ymouse);
		side = (xmouse > CFRA) ? 'R' : 'L';
	}
	else {
		/* normal transform - both sides of current frame are considered */
		side = 'B';
	}
	
	/* only affect keyframes */
	for (base=G.scene->base.first; base; base=base->next) {
		ob= base->object;
		
		/* Check object ipos */
		i= count_ipo_keys(ob->ipo, side, CFRA);
		if (i) posttrans_ipo_clean(ob->ipo);
		
		/* Check object constraint ipos */
		for (conchan=ob->constraintChannels.first; conchan; conchan=conchan->next) {
			i= count_ipo_keys(conchan->ipo, side, CFRA);	
			if (i) posttrans_ipo_clean(ob->ipo);
		}
		
		/* skip actions and nlastrips if object is collapsed */
		if (ob->nlaflag & OB_NLA_COLLAPSED)
			continue;
		
		/* Check action ipos */
		if (ob->action) {
			/* exclude if strip is selected too */
			for (strip=ob->nlastrips.first; strip; strip=strip->next) {
				if (strip->flag & ACTSTRIP_SELECT) {
					if (strip->act == ob->action)
						break;
				}
			}
			if (strip==NULL) {
				cfra = get_action_frame(ob, CFRA);
				
				for (achan=ob->action->chanbase.first; achan; achan=achan->next) {
					if (EDITABLE_ACHAN(achan)) {
						i= count_ipo_keys(achan->ipo, side, cfra);
						if (i) {
							actstrip_map_ipo_keys(ob, achan->ipo, 0, 1); 
							posttrans_ipo_clean(achan->ipo);
							actstrip_map_ipo_keys(ob, achan->ipo, 1, 1);
						}
						
						/* Check action constraint ipos */
						if (EXPANDED_ACHAN(achan) && FILTER_CON_ACHAN(achan)) {
							for (conchan=achan->constraintChannels.first; conchan; conchan=conchan->next) {
								if (EDITABLE_CONCHAN(conchan)) {
									i = count_ipo_keys(conchan->ipo, side, cfra);
									if (i) {
										actstrip_map_ipo_keys(ob, conchan->ipo, 0, 1); 
										posttrans_ipo_clean(conchan->ipo);
										actstrip_map_ipo_keys(ob, conchan->ipo, 1, 1);
									}
								}
							}
						}
					}
				}
			}		
		}
	}
}

/* ----------------------------- */

/* This function tests if a point is on the "mouse" side of the cursor/frame-marking */
static short FrameOnMouseSide(char side, float frame, float cframe)
{
	/* both sides, so it doesn't matter */
	if (side == 'B') return 1;
	
	/* only on the named side */
	if (side == 'R')
		return (frame >= cframe) ? 1 : 0;
	else
		return (frame <= cframe) ? 1 : 0;
}

/* fully select selected beztriples, but only include if it's on the right side of cfra */
static int count_ipo_keys(Ipo *ipo, char side, float cfra)
{
	IpoCurve *icu;
	BezTriple *bezt;
	int i, count = 0;
	
	if (ipo == NULL)
		return count;
	
	/* only include points that occur on the right side of cfra */
	for (icu= ipo->curve.first; icu; icu= icu->next) {
		for (i=0, bezt=icu->bezt; i < icu->totvert; i++, bezt++) {
			if (bezt->f2 & SELECT) {
				/* fully select the other two keys */
				bezt->f1 |= SELECT;
				bezt->f3 |= SELECT;
				
				/* increment by 3, as there are 3 points (3 * x-coordinates) that need transform */
				if (FrameOnMouseSide(side, bezt->vec[1][0], cfra))
					count += 3;
			}
		}
	}
	
	return count;
}

/* fully select selected beztriples, but only include if it's on the right side of cfra */
static int count_gplayer_frames(bGPDlayer *gpl, char side, float cfra)
{
	bGPDframe *gpf;
	int count = 0;
	
	if (gpl == NULL)
		return count;
	
	/* only include points that occur on the right side of cfra */
	for (gpf= gpl->frames.first; gpf; gpf= gpf->next) {
		if (gpf->flag & GP_FRAME_SELECT) {
			if (FrameOnMouseSide(side, gpf->framenum, cfra))
				count++;
		}
	}
	
	return count;
}

/* This function assigns the information to transdata */
static void TimeToTransData(TransData *td, float *time, Object *ob)
{
	/* memory is calloc'ed, so that should zero everything nicely for us */
	td->val = time;
	td->ival = *(time);
	
	/* store the Object where this keyframe exists as a keyframe of the 
	 * active action as td->ob. Usually, this member is only used for constraints 
	 * drawing
	 */
	td->ob= ob;
}

/* This function advances the address to which td points to, so it must return
 * the new address so that the next time new transform data is added, it doesn't
 * overwrite the existing ones...  i.e.   td = IpoToTransData(td, ipo, ob, side, cfra);
 *
 * The 'side' argument is needed for the extend mode. 'B' = both sides, 'R'/'L' mean only data
 * on the named side are used. 
 */
static TransData *IpoToTransData(TransData *td, Ipo *ipo, Object *ob, char side, float cfra)
{
	IpoCurve *icu;
	BezTriple *bezt;
	int i;
	
	if (ipo == NULL)
		return td;
	
	for (icu= ipo->curve.first; icu; icu= icu->next) {
		for (i=0, bezt=icu->bezt; i < icu->totvert; i++, bezt++) {
			/* only add selected keyframes (for now, proportional edit is not enabled) */
			if (BEZSELECTED(bezt)) {
				/* only add if on the right 'side' of the current frame */
				if (FrameOnMouseSide(side, bezt->vec[1][0], cfra)) {
					/* each control point needs to be added separetely */
					TimeToTransData(td, bezt->vec[0], ob);
					td++;
					
					TimeToTransData(td, bezt->vec[1], ob);
					td++;
					
					TimeToTransData(td, bezt->vec[2], ob);
					td++;
				}
			}	
		}
	}
	
	return td;
}

/* helper struct for gp-frame transforms (only used here) */
typedef struct tGPFtransdata {
	float val;			/* where transdata writes transform */
	int *sdata;			/* pointer to gpf->framenum */
} tGPFtransdata;

/* This function helps flush transdata written to tempdata into the gp-frames  */
void flushTransGPactionData (TransInfo *t)
{
	tGPFtransdata *tfd;
	int i;
	
	/* find the first one to start from */
	if (t->mode == TFM_TIME_SLIDE)
		tfd= (tGPFtransdata *)( (float *)(t->customData) + 2 );
	else
		tfd= (tGPFtransdata *)(t->customData);
		
	/* flush data! */
	for (i = 0; i < t->total; i++, tfd++) {
		*(tfd->sdata)= (int)floor(tfd->val + 0.5);
	}	
}

/* This function advances the address to which td points to, so it must return
 * the new address so that the next time new transform data is added, it doesn't
 * overwrite the existing ones...  i.e.   td = GPLayerToTransData(td, ipo, ob, side, cfra);
 *
 * The 'side' argument is needed for the extend mode. 'B' = both sides, 'R'/'L' mean only data
 * on the named side are used. 
 */
static int GPLayerToTransData (TransData *td, tGPFtransdata *tfd, bGPDlayer *gpl, short side, float cfra)
{
	bGPDframe *gpf;
	int count= 0;
	
	/* check for select frames on right side of current frame */
	for (gpf= gpl->frames.first; gpf; gpf= gpf->next) {
		if (gpf->flag & GP_FRAME_SELECT) {
			if (FrameOnMouseSide(side, gpf->framenum, cfra)) {
				/* memory is calloc'ed, so that should zero everything nicely for us */
				td->val= &tfd->val;
				td->ival= gpf->framenum;
				
				tfd->val= gpf->framenum;
				tfd->sdata= &gpf->framenum;
				
				/* advance td now */
				td++;
				tfd++;
				count++;
			}
		}
	}
	
	return count;
}

static void createTransActionData(TransInfo *t)
{
	TransData *td = NULL;
	tGPFtransdata *tfd = NULL;
	Object *ob= NULL;
	
	ListBase act_data = {NULL, NULL};
	bActListElem *ale;
	void *data;
	short datatype;
	int filter;
	
	int count=0;
	float cfra;
	char side;
	
	/* determine what type of data we are operating on */
	data = get_action_context(&datatype);
	if (data == NULL) return;
	
	/* filter data */
	if (datatype == ACTCONT_GPENCIL)
		filter= (ACTFILTER_VISIBLE | ACTFILTER_FOREDIT);
	else
		filter= (ACTFILTER_VISIBLE | ACTFILTER_FOREDIT | ACTFILTER_IPOKEYS);
	actdata_filter(&act_data, filter, data, datatype);
	
	/* is the action scaled? if so, the it should belong to the active object */
	if (NLA_ACTION_SCALED)
		ob= OBACT;
		
	/* which side of the current frame should be allowed */
	if (t->mode == TFM_TIME_EXTEND) {
		/* only side on which mouse is gets transformed */
		float xmouse, ymouse;
		
		areamouseco_to_ipoco(G.v2d, t->imval, &xmouse, &ymouse);
		side = (xmouse > CFRA) ? 'R' : 'L';
	}
	else {
		/* normal transform - both sides of current frame are considered */
		side = 'B';
	}
	
	/* convert current-frame to action-time (slightly less accurate, espcially under
	 * higher scaling ratios, but is faster than converting all points) 
	 */
	if (ob) 
		cfra = get_action_frame(ob, CFRA);
	else
		cfra = CFRA;
	
	/* loop 1: fully select ipo-keys and count how many BezTriples are selected */
	for (ale= act_data.first; ale; ale= ale->next) {
		if (ale->type == ACTTYPE_GPLAYER)
			count += count_gplayer_frames(ale->data, side, cfra);
		else
			count += count_ipo_keys(ale->key_data, side, cfra);
	}
	
	/* stop if trying to build list if nothing selected */
	if (count == 0) {
		/* cleanup temp list */
		BLI_freelistN(&act_data);
		return;
	}
	
	/* allocate memory for data */
	t->total= count;
	
	t->data= MEM_callocN(t->total*sizeof(TransData), "TransData(Action Editor)");
	td= t->data;
	
	if (datatype == ACTCONT_GPENCIL) {
		if (t->mode == TFM_TIME_SLIDE) {
			t->customData= MEM_callocN((sizeof(float)*2)+(sizeof(tGPFtransdata)*count), "TimeSlide + tGPFtransdata");
			tfd= (tGPFtransdata *)( (float *)(t->customData) + 2 );
		}
		else {
			t->customData= MEM_callocN(sizeof(tGPFtransdata)*count, "tGPFtransdata");
			tfd= (tGPFtransdata *)(t->customData);
		}
	}
	else if (t->mode == TFM_TIME_SLIDE)
		t->customData= MEM_callocN(sizeof(float)*2, "TimeSlide Min/Max");
	
	/* loop 2: build transdata array */
	for (ale= act_data.first; ale; ale= ale->next) {
		if (ale->type == ACTTYPE_GPLAYER) {
			bGPDlayer *gpl= (bGPDlayer *)ale->data;
			int i;
			
			i = GPLayerToTransData(td, tfd, gpl, side, cfra);
			td += i;
			tfd += i;
		}
		else {
			Ipo *ipo= (Ipo *)ale->key_data;
			
			td= IpoToTransData(td, ipo, ob, side, cfra);
		}
	}
	
	/* check if we're supposed to be setting minx/maxx for TimeSlide */
	if (t->mode == TFM_TIME_SLIDE) {
		float min=999999999.0f, max=-999999999.0;
		int i;
		
		td= (t->data + 1);
		for (i=1; i < count; i+=3, td+=3) {
			if (min > *(td->val)) min= *(td->val);
			if (max < *(td->val)) max= *(td->val);
		}
		
		/* minx/maxx values used by TimeSlide are stored as a 
		 * calloced 2-float array in t->customData. This gets freed
		 * in postTrans (T_FREE_CUSTOMDATA). 
		 */
		*((float *)(t->customData)) = min;
		*((float *)(t->customData) + 1) = max;
	}
	
	/* cleanup temp list */
	BLI_freelistN(&act_data);
}

static void createTransNlaData(TransInfo *t)
{
	Base *base;
	bActionStrip *strip;
	bActionChannel *achan;
	bConstraintChannel *conchan;
	
	TransData *td = NULL;
	int count=0, i;
	float cfra;
	char side;
	
	/* which side of the current frame should be allowed */
	if (t->mode == TFM_TIME_EXTEND) {
		/* only side on which mouse is gets transformed */
		float xmouse, ymouse;
		
		areamouseco_to_ipoco(G.v2d, t->imval, &xmouse, &ymouse);
		side = (xmouse > CFRA) ? 'R' : 'L';
	}
	else {
		/* normal transform - both sides of current frame are considered */
		side = 'B';
	}
	
	/* Ensure that partial selections result in beztriple selections */
	for (base=G.scene->base.first; base; base=base->next) {
		/* Check object ipos */
		i= count_ipo_keys(base->object->ipo, side, CFRA);
		if (i) base->flag |= BA_HAS_RECALC_OB;
		count += i;
		
		/* Check object constraint ipos */
		for (conchan=base->object->constraintChannels.first; conchan; conchan=conchan->next)
			count += count_ipo_keys(conchan->ipo, side, CFRA);			
		
		/* skip actions and nlastrips if object is collapsed */
		if (base->object->nlaflag & OB_NLA_COLLAPSED)
			continue;
		
		/* Check action ipos */
		if (base->object->action) {
			/* exclude if strip is selected too */
			for (strip=base->object->nlastrips.first; strip; strip=strip->next) {
				if (strip->flag & ACTSTRIP_SELECT) {
					if (strip->act == base->object->action)
						break;
				}
			}
			if (strip==NULL) {
				cfra = get_action_frame(base->object, CFRA);
				
				for (achan=base->object->action->chanbase.first; achan; achan=achan->next) {
					if (EDITABLE_ACHAN(achan)) {
						i= count_ipo_keys(achan->ipo, side, cfra);
						if (i) base->flag |= BA_HAS_RECALC_OB|BA_HAS_RECALC_DATA;
						count += i;
						
						/* Check action constraint ipos */
						if (EXPANDED_ACHAN(achan) && FILTER_CON_ACHAN(achan)) {
							for (conchan=achan->constraintChannels.first; conchan; conchan=conchan->next) {
								if (EDITABLE_CONCHAN(conchan))
									count += count_ipo_keys(conchan->ipo, side, cfra);
							}
						}
					}
				}
			}		
		}
		
		/* Check nlastrips */
		for (strip=base->object->nlastrips.first; strip; strip=strip->next) {
			if (strip->flag & ACTSTRIP_SELECT) {
				base->flag |= BA_HAS_RECALC_OB|BA_HAS_RECALC_DATA;
				
				if (FrameOnMouseSide(side, strip->start, CFRA)) count++;
				if (FrameOnMouseSide(side, strip->end, CFRA)) count++;
			}
		}
	}
	
	/* If nothing is selected, bail out */
	if (count == 0)
		return;
	
	/* allocate memory for data */
	t->total= count;
	t->data= MEM_callocN(t->total*sizeof(TransData), "TransData (NLA Editor)");
	
	/* build the transdata structure */
	td= t->data;
	for (base=G.scene->base.first; base; base=base->next) {
		/* Manipulate object ipos */
		/* 	- no scaling of keyframe times is allowed here  */
		td= IpoToTransData(td, base->object->ipo, NULL, side, CFRA);
		
		/* Manipulate object constraint ipos */
		/* 	- no scaling of keyframe times is allowed here  */
		for (conchan=base->object->constraintChannels.first; conchan; conchan=conchan->next)
			td= IpoToTransData(td, conchan->ipo, NULL, side, CFRA);
		
		/* skip actions and nlastrips if object collapsed */
		if (base->object->nlaflag & OB_NLA_COLLAPSED)
			continue;
			
		/* Manipulate action ipos */
		if (base->object->action) {
			/* exclude if strip that active action belongs to is selected too */
			for (strip=base->object->nlastrips.first; strip; strip=strip->next) {
				if (strip->flag & ACTSTRIP_SELECT) {
					if (strip->act == base->object->action)
						break;
				}
			}
			
			/* can include if no strip found */
			if (strip==NULL) {
				cfra = get_action_frame(base->object, CFRA);
				
				for (achan=base->object->action->chanbase.first; achan; achan=achan->next) {
					if (EDITABLE_ACHAN(achan)) {
						td= IpoToTransData(td, achan->ipo, base->object, side, cfra);
						
						/* Manipulate action constraint ipos */
						if (EXPANDED_ACHAN(achan) && FILTER_CON_ACHAN(achan)) {
							for (conchan=achan->constraintChannels.first; conchan; conchan=conchan->next) {
								if (EDITABLE_CONCHAN(conchan))
									td= IpoToTransData(td, conchan->ipo, base->object, side, cfra);
							}
						}
					}
				}
			}
		}
		
		/* Manipulate nlastrips */
		for (strip=base->object->nlastrips.first; strip; strip=strip->next) {
			if (strip->flag & ACTSTRIP_SELECT) {
				/* first TransData is the start, second is the end */
				if (FrameOnMouseSide(side, strip->start, CFRA)) {
					td->val = &strip->start;
					td->ival = strip->start;
					td++;
				}
				if (FrameOnMouseSide(side, strip->end, CFRA)) {
					td->val = &strip->end;
					td->ival = strip->end;
					td++;
				}
			}
		}
	}
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
static short constraints_list_needinv(TransInfo *t, ListBase *list)
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
					/* constraints that require this regardless  */
				if (con->type == CONSTRAINT_TYPE_CHILDOF) return 1;
				if (con->type == CONSTRAINT_TYPE_FOLLOWPATH) return 1;
				if (con->type == CONSTRAINT_TYPE_CLAMPTO) return 1;
				
					/* constraints that require this only under special conditions */
				if (con->type == CONSTRAINT_TYPE_ROTLIKE) {
					/* CopyRot constraint only does this when rotating, and offset is on */
					bRotateLikeConstraint *data = (bRotateLikeConstraint *)con->data;
					
					if ((data->flag & ROTLIKE_OFFSET) && (t->mode == TFM_ROTATION))
						return 1;
				}
			}
		}
	}
	
	/* no appropriate candidates found */
	return 0;
}

/* transcribe given object into TransData for Transforming */
static void ObjectToTransData(TransInfo *t, TransData *td, Object *ob) 
{
	Object *track;
	ListBase fakecons = {NULL, NULL};
	float obmtx[3][3];
	short constinv;
	short skip_invert = 0;

	/* axismtx has the real orientation */
	Mat3CpyMat4(td->axismtx, ob->obmat);
	Mat3Ortho(td->axismtx);

	td->con= ob->constraints.first;
	
	/* hack: tempolarily disable tracking and/or constraints when getting 
	 *		object matrix, if tracking is on, or if constraints don't need
	 * 		inverse correction to stop it from screwing up space conversion
	 *		matrix later
	 */
	constinv = constraints_list_needinv(t, &ob->constraints);
	
	/* disable constraints inversion for dummy pass */
	if (t->mode == TFM_DUMMY)
		skip_invert = 1;
		
	if (skip_invert == 0 && (ob->track || constinv==0)) {
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
	
	Mat4CpyMat4(td->ext->obmat, ob->obmat);

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
	
	/* set active flag */
	if (BASACT && BASACT->object == ob)
	{
		td->flag |= TD_ACTIVE;
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
	
	/* don't do it if we're not actually going to recalculate anything */
	if(t->mode == TFM_DUMMY)
		return;

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
	DAG_scene_flush_update(G.scene, -1, 0);
	
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

/* auto-keyframing feature - checks for whether anything should be done for the current frame */
short autokeyframe_cfra_can_key(Object *ob)
{
	ListBase keys = {NULL, NULL};
	ActKeyColumn *ak;
	float cfra;
	short found= 0;
	
	/* only filter if auto-key mode requires this */
	if (IS_AUTOKEY_ON == 0)
		return 0;
	else if (IS_AUTOKEY_MODE(NORMAL)) 
		return 1;
	
	/* sanity check */
	if (ob == NULL) 
		return 0;
	
	/* get keyframes that object has (bone anim is stored on ob too) */
	if (ob->action)
		action_to_keylist(ob->action, &keys, NULL, NULL);
	else if (ob->ipo)
		ipo_to_keylist(ob->ipo, &keys, NULL, NULL);
	else
		return 0;
		
	/* get current frame (will apply nla-scaling as necessary) */
	// ack... this is messy...
	cfra= frame_to_float(CFRA);
	cfra= get_action_frame(ob, cfra);
	
	/* check if a keyframe occurs on current frame */
	for (ak= keys.first; ak; ak= ak->next) {
		if (IS_EQ(cfra, ak->cfra)) {
			found= 1;
			break;
		}
	}
	
	/* free temp list */
	BLI_freelistN(&keys);
	
	return found;
}

/* auto-keyframing feature - for objects 
 * 	tmode: should be a transform mode 
 */
void autokeyframe_ob_cb_func(Object *ob, int tmode)
{
	IpoCurve *icu;
	
	if (autokeyframe_cfra_can_key(ob)) {
		char *actname = NULL;
		
		if (ob->ipoflag & OB_ACTION_OB)
			actname= "Object";
		
		if (IS_AUTOKEY_FLAG(INSERTAVAIL)) {
			if ((ob->ipo) || (ob->action)) {
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
				
				while (icu) {
					icu->flag &= ~IPO_SELECT;
					if (IS_AUTOKEY_FLAG(INSERTNEEDED))
						insertkey_smarter(id, ID_OB, actname, NULL, icu->adrcode);
					else
						insertkey(id, ID_OB, actname, NULL, icu->adrcode, 0);
					icu= icu->next;
				}
			}
		}
		else if (IS_AUTOKEY_FLAG(INSERTNEEDED)) {
			ID *id= (ID *)(ob);
			short doLoc=0, doRot=0, doScale=0;
			
			/* filter the conditions when this happens (assume that curarea->spacetype==SPACE_VIE3D) */
			if (tmode == TFM_TRANSLATION) {
				doLoc = 1;
			}
			else if (tmode == TFM_ROTATION) {
				if (G.vd->around == V3D_ACTIVE) {
					if (ob != OBACT)
						doLoc = 1;
				}
				else if (G.vd->around == V3D_CURSOR)
					doLoc = 1;	
				
				if ((G.vd->flag & V3D_ALIGN)==0) 
					doRot = 1;
			}
			else if (tmode == TFM_RESIZE) {
				if (G.vd->around == V3D_ACTIVE) {
					if (ob != OBACT)
						doLoc = 1;
				}
				else if (G.vd->around == V3D_CURSOR)
					doLoc = 1;	
				
				if ((G.vd->flag & V3D_ALIGN)==0)
					doScale = 1;
			}
			
			if (doLoc) {
				insertkey_smarter(id, ID_OB, actname, NULL, OB_LOC_X);
				insertkey_smarter(id, ID_OB, actname, NULL, OB_LOC_Y);
				insertkey_smarter(id, ID_OB, actname, NULL, OB_LOC_Z);
			}
			if (doRot) {
				insertkey_smarter(id, ID_OB, actname, NULL, OB_ROT_X);
				insertkey_smarter(id, ID_OB, actname, NULL, OB_ROT_Y);
				insertkey_smarter(id, ID_OB, actname, NULL, OB_ROT_Z);
			}
			if (doScale) {
				insertkey_smarter(id, ID_OB, actname, NULL, OB_SIZE_X);
				insertkey_smarter(id, ID_OB, actname, NULL, OB_SIZE_Y);
				insertkey_smarter(id, ID_OB, actname, NULL, OB_SIZE_Z);
			}
		}
		else {
			ID *id= (ID *)(ob);
			
			insertkey(id, ID_OB, actname, NULL, OB_LOC_X, 0);
			insertkey(id, ID_OB, actname, NULL, OB_LOC_Y, 0);
			insertkey(id, ID_OB, actname, NULL, OB_LOC_Z, 0);
			
			insertkey(id, ID_OB, actname, NULL, OB_ROT_X, 0);
			insertkey(id, ID_OB, actname, NULL, OB_ROT_Y, 0);
			insertkey(id, ID_OB, actname, NULL, OB_ROT_Z, 0);
			
			insertkey(id, ID_OB, actname, NULL, OB_SIZE_X, 0);
			insertkey(id, ID_OB, actname, NULL, OB_SIZE_Y, 0);
			insertkey(id, ID_OB, actname, NULL, OB_SIZE_Z, 0);
		}
		
		remake_object_ipos(ob);
		allqueue(REDRAWMARKER, 0);
		allqueue(REDRAWOOPS, 0);
	}
}

/* auto-keyframing feature - for poses/pose-channels 
 * 	tmode: should be a transform mode 
 *	targetless_ik: has targetless ik been done on any channels?
 */
void autokeyframe_pose_cb_func(Object *ob, int tmode, short targetless_ik)
{
	ID *id= (ID *)(ob);
	bArmature *arm= ob->data;
	bAction	*act;
	bPose	*pose;
	bPoseChannel *pchan;
	IpoCurve *icu;
	
	pose= ob->pose;
	act= ob->action;
	
	if (autokeyframe_cfra_can_key(ob)) {
		if (act == NULL)
			act= ob->action= add_empty_action("Action");
		
		for (pchan=pose->chanbase.first; pchan; pchan=pchan->next) {
			if (pchan->bone->flag & BONE_TRANSFORM) {
				/* clear any 'unkeyed' flag it may have */
				pchan->bone->flag &= ~BONE_UNKEYED;
				
				/* only insert into available channels? */
				if (IS_AUTOKEY_FLAG(INSERTAVAIL)) {
					bActionChannel *achan; 
					
					for (achan = act->chanbase.first; achan; achan=achan->next) {
						if ((achan->ipo) && !strcmp(achan->name, pchan->name)) {
							for (icu = achan->ipo->curve.first; icu; icu=icu->next) {
								/* only insert keyframe if needed? */
								if (IS_AUTOKEY_FLAG(INSERTNEEDED))
									insertkey_smarter(&ob->id, ID_PO, pchan->name, NULL, icu->adrcode);
								else
									insertkey(&ob->id, ID_PO, pchan->name, NULL, icu->adrcode, 0);
							}
							break;
						}
					}
				}
				/* only insert keyframe if needed? */
				else if (IS_AUTOKEY_FLAG(INSERTNEEDED)) {
					short doLoc=0, doRot=0, doScale=0;
					
					/* filter the conditions when this happens (assume that curarea->spacetype==SPACE_VIE3D) */
					if (tmode == TFM_TRANSLATION) {
						if (targetless_ik) 
							doRot= 1;
						else 
							doLoc = 1;
					}
					else if (tmode == TFM_ROTATION) {
						if (ELEM(G.vd->around, V3D_CURSOR, V3D_ACTIVE))
							doLoc = 1;
							
						if ((G.vd->flag & V3D_ALIGN)==0) 
							doRot = 1;
					}
					else if (tmode == TFM_RESIZE) {
						if (ELEM(G.vd->around, V3D_CURSOR, V3D_ACTIVE))
							doLoc = 1;
						
						if ((G.vd->flag & V3D_ALIGN)==0)
							doScale = 1;
					}
					
					if (doLoc) {
						insertkey_smarter(id, ID_PO, pchan->name, NULL, AC_LOC_X);
						insertkey_smarter(id, ID_PO, pchan->name, NULL, AC_LOC_Y);
						insertkey_smarter(id, ID_PO, pchan->name, NULL, AC_LOC_Z);
					}
					if (doRot) {
						insertkey_smarter(id, ID_PO, pchan->name, NULL, AC_QUAT_W);
						insertkey_smarter(id, ID_PO, pchan->name, NULL, AC_QUAT_X);
						insertkey_smarter(id, ID_PO, pchan->name, NULL, AC_QUAT_Y);
						insertkey_smarter(id, ID_PO, pchan->name, NULL, AC_QUAT_Z);
					}
					if (doScale) {
						insertkey_smarter(id, ID_PO, pchan->name, NULL, AC_SIZE_X);
						insertkey_smarter(id, ID_PO, pchan->name, NULL, AC_SIZE_Y);
						insertkey_smarter(id, ID_PO, pchan->name, NULL, AC_SIZE_Z);
					}
				}
				/* insert keyframe in any channel that's appropriate */
				else {
					insertkey(id, ID_PO, pchan->name, NULL, AC_SIZE_X, 0);
					insertkey(id, ID_PO, pchan->name, NULL, AC_SIZE_Y, 0);
					insertkey(id, ID_PO, pchan->name, NULL, AC_SIZE_Z, 0);
					
					insertkey(id, ID_PO, pchan->name, NULL, AC_QUAT_W, 0);
					insertkey(id, ID_PO, pchan->name, NULL, AC_QUAT_X, 0);
					insertkey(id, ID_PO, pchan->name, NULL, AC_QUAT_Y, 0);
					insertkey(id, ID_PO, pchan->name, NULL, AC_QUAT_Z, 0);
					
					insertkey(id, ID_PO, pchan->name, NULL, AC_LOC_X, 0);
					insertkey(id, ID_PO, pchan->name, NULL, AC_LOC_Y, 0);
					insertkey(id, ID_PO, pchan->name, NULL, AC_LOC_Z, 0);
				}
			}
		}
		
		remake_action_ipos(act);
		allqueue(REDRAWMARKER, 0);
		allqueue(REDRAWOOPS, 0);
		
		/* locking can be disabled */
		ob->pose->flag &= ~(POSE_DO_UNLOCK|POSE_LOCKED);
		
		/* do the bone paths */
		if (arm->pathflag & ARM_PATH_ACFRA) {
			//pose_clear_paths(ob);
			pose_recalculate_paths(ob);
		}	
	}
	else {
		/* tag channels that should have unkeyed data */
		for (pchan=pose->chanbase.first; pchan; pchan=pchan->next) {
			if (pchan->bone->flag & BONE_TRANSFORM) {
				/* tag this channel */
				pchan->bone->flag |= BONE_UNKEYED;
			}
		}
	}
}

/* very bad call!!! - copied from editnla.c!  */
static void recalc_all_ipos(void)
{
	Ipo *ipo;
	IpoCurve *icu;
	
	/* Go to each ipo */
	for (ipo=G.main->ipo.first; ipo; ipo=ipo->id.next){
		for (icu = ipo->curve.first; icu; icu=icu->next){
			sort_time_ipocurve(icu);
			testhandles_ipocurve(icu);
		}
	}
}

/* inserting keys, refresh ipo-keys, pointcache, redraw events... (ton) */
/* note: transdata has been freed already! */
void special_aftertrans_update(TransInfo *t)
{
	Object *ob;
	Base *base;
	short redrawipo=0, resetslowpar=1;
	int cancelled= (t->state == TRANS_CANCEL);
	short duplicate= (t->undostr && strstr(t->undostr, "Duplicate")) ? 1 : 0;
	
	if (t->spacetype==SPACE_VIEW3D) {
		if (G.obedit) {
			if (cancelled==0) {
				EM_automerge(1);
				/* when snapping, delay retopo until after automerge */
				if (G.qual & LR_CTRLKEY) {
					retopo_do_all();
				}
			}
		}
	}
	if (t->spacetype == SPACE_ACTION) {
		void *data;
		short datatype;
		
		/* determine what type of data we are operating on */
		data = get_action_context(&datatype);
		if (data == NULL) return;
		ob = OBACT;
		
		if (datatype == ACTCONT_ACTION) {
			/* Depending on the lock status, draw necessary views */
			if (ob) {
				ob->ctime= -1234567.0f;
				
				if(ob->pose || ob_get_key(ob))
					DAG_object_flush_update(G.scene, ob, OB_RECALC);
				else
					DAG_object_flush_update(G.scene, ob, OB_RECALC_OB);
			}
			
			/* Do curve cleanups? */
			if ( (G.saction->flag & SACTION_NOTRANSKEYCULL)==0 && 
			     ((cancelled == 0) || (duplicate)) )
			{
				posttrans_action_clean((bAction *)data);
			}
			
			/* Do curve updates */
			remake_action_ipos((bAction *)data);
		}
		else if (datatype == ACTCONT_SHAPEKEY) {
			/* fix up the Ipocurves and redraw stuff */
			Key *key= (Key *)data;
			if (key->ipo) {
				IpoCurve *icu;
				
				if ( (G.saction->flag & SACTION_NOTRANSKEYCULL)==0 && 
				     ((cancelled == 0) || (duplicate)) )
				{
					posttrans_ipo_clean(key->ipo);
				}
				
				for (icu = key->ipo->curve.first; icu; icu=icu->next) {
					sort_time_ipocurve(icu);
					testhandles_ipocurve(icu);
				}
			}
			
			DAG_object_flush_update(G.scene, OBACT, OB_RECALC_DATA);
		}
		else if (datatype == ACTCONT_GPENCIL) {
			/* remove duplicate frames and also make sure points are in order! */
			if ((cancelled == 0) || (duplicate))
			{
				posttrans_gpd_clean(data);
			}
		}
		
		G.saction->flag &= ~SACTION_MOVING;
	}
	else if (t->spacetype == SPACE_NLA) {
		recalc_all_ipos();	// bad
		synchronize_action_strips();
		
		/* cleanup */
		for (base=G.scene->base.first; base; base=base->next)
			base->flag &= ~(BA_HAS_RECALC_OB|BA_HAS_RECALC_DATA);
		
		/* after transform, remove duplicate keyframes on a frame that resulted from transform */
		if ( (G.snla->flag & SNLA_NOTRANSKEYCULL)==0 && 
			 ((cancelled == 0) || (duplicate)) )
		{
			posttrans_nla_clean(t);
		}
	}
	else if (t->spacetype == SPACE_IPO) {
		// FIXME! is there any code from the old transform_ipo that needs to be added back? 
		
		/* after transform, remove duplicate keyframes on a frame that resulted from transform */
		if (G.sipo->ipo) 
		{
			if ( (G.sipo->flag & SIPO_NOTRANSKEYCULL)==0 && 
				 (cancelled == 0) )
			{
				if (NLA_IPO_SCALED) {
					actstrip_map_ipo_keys(OBACT, G.sipo->ipo, 0, 1); 
					posttrans_ipo_clean(G.sipo->ipo);
					actstrip_map_ipo_keys(OBACT, G.sipo->ipo, 1, 1);
				}
				else 
					posttrans_ipo_clean(G.sipo->ipo);
			}
		}
		
		/* resetting slow-parents isn't really necessary when editing sequence ipo's */
		if (G.sipo->blocktype==ID_SEQ)
			resetslowpar= 0;
	}
	else if (G.obedit) {
		if (t->mode==TFM_BONESIZE || t->mode==TFM_BONE_ENVELOPE)
			allqueue(REDRAWBUTSEDIT, 0);
		
		/* table needs to be created for each edit command, since vertices can move etc */
		mesh_octree_table(G.obedit, NULL, 'e');
	}
	else if ((t->flag & T_POSE) && (t->poseobj)) {
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
		if (!cancelled && t->mode==TFM_TRANSLATION)
			targetless_ik= apply_targetless_ik(ob);
		else {
			/* not forget to clear the auto flag */
			for (pchan=ob->pose->chanbase.first; pchan; pchan=pchan->next) {
				bKinematicConstraint *data= has_targetless_ik(pchan);
				if(data) data->flag &= ~CONSTRAINT_IK_AUTO;
			}
		}
		
		if (t->mode==TFM_TRANSLATION)
			pose_grab_with_ik_clear(ob);
			
		/* automatic inserting of keys and unkeyed tagging - only if transform wasn't cancelled (or TFM_DUMMY) */
		if (!cancelled && (t->mode != TFM_DUMMY)) {
			autokeyframe_pose_cb_func(ob, t->mode, targetless_ik);
			DAG_object_flush_update(G.scene, ob, OB_RECALC_DATA);
		}
		else if (arm->flag & ARM_DELAYDEFORM) {
			/* old optimize trick... this enforces to bypass the depgraph */
			DAG_object_flush_update(G.scene, ob, OB_RECALC_DATA);
			ob->recalc= 0;	// is set on OK position already by recalcData()
		}
		else 
			DAG_object_flush_update(G.scene, ob, OB_RECALC_DATA);
		
		if (t->mode==TFM_BONESIZE || t->mode==TFM_BONE_ENVELOPE)
			allqueue(REDRAWBUTSEDIT, 0);
		
	}
	else if(G.f & G_PARTICLEEDIT) {
		;
	}
	else {
		base= FIRSTBASE;

		while (base) {			

			if(base->flag & BA_DO_IPO) redrawipo= 1;
			
			ob= base->object;
			
			if(base->flag & SELECT && (t->mode != TFM_DUMMY)) {
				if(BKE_ptcache_object_reset(ob, PTCACHE_RESET_DEPSGRAPH))
					ob->recalc |= OB_RECALC_DATA;
				
				/* Set autokey if necessary */
				if (!cancelled)
					autokeyframe_ob_cb_func(ob, t->mode);
			}
			
			base= base->next;
		}
		
	}
	
	clear_trans_object_base_flags();
	
	if (redrawipo) {
		allqueue(REDRAWNLA, 0);
		allqueue(REDRAWACTION, 0);
		allqueue(REDRAWIPO, 0);
	}
	
	if(resetslowpar)
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
		if TESTBASE(base) {
			ob= base->object;
			
			/* store ipo keys? */
			if (ob->id.lib == 0 && ob->ipo && ob->ipo->showkey && (ob->ipoflag & OB_DRAWKEY)) {
				elems.first= elems.last= NULL;
				make_ipokey_transform(ob, &elems, 1); /* '1' only selected keys */
				
				pushdata(&elems, sizeof(ListBase));
				
				for(ik= elems.first; ik; ik= ik->next)
					t->total++;

				if(elems.first==NULL)
					t->total++;
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
		if TESTBASE(base) {
			ob= base->object;
			
			td->flag = TD_SELECTED;
			td->protectflag= ob->protectflag;
			td->ext = tx;
			
			/* select linked objects, but skip them later */
			if (ob->id.lib != 0) {
				td->flag |= TD_SKIP;
			}

			/* store ipo keys? */
			if(ob->id.lib == 0 && ob->ipo && ob->ipo->showkey && (ob->ipoflag & OB_DRAWKEY)) {
				
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
						ObjectToTransData(t, td, ob);	// does where_is_object()
						
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
					ObjectToTransData(t, td, ob);
					td->tdi = NULL;
					td->val = NULL;
					td++;
					tx++;
				}
			}
			else {
				ObjectToTransData(t, td, ob);
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
	else if (t->context == CTX_BMESH) {
		createTransBMeshVerts(t, G.editBMesh->bm, G.editBMesh->td);
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
	else if (t->spacetype == SPACE_ACTION) {
		t->flag |= T_POINTS|T_2D_EDIT;
		createTransActionData(t);
	}
	else if (t->spacetype == SPACE_NLA) {
		t->flag |= T_POINTS|T_2D_EDIT;
		createTransNlaData(t);
	}
	else if (t->spacetype == SPACE_IPO) {
		t->flag |= T_POINTS|T_2D_EDIT;
		createTransIpoData(t); 
		if (t->data && (t->flag & T_PROP_EDIT)) {
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
			printf("not done yet! only have mesh surface curve lattice mball armature\n");
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
	else if (G.f & G_PARTICLEEDIT && PE_can_edit(PE_get_current(ob))) {
		createTransParticleVerts(t);

		if(t->data && t->flag & T_PROP_EDIT) {
			sort_trans_data(t);	// makes selected become first in array
			set_prop_dist(t, 1);
			sort_trans_data_dist(t);
		}

		t->flag |= T_POINTS;
	}
	else {
		t->flag &= ~T_PROP_EDIT; /* no proportional edit in object mode */
		createTransObject(t);
		t->flag |= T_OBJECT;
	}

	if((t->flag & T_OBJECT) && G.vd->camera==OBACT && G.vd->persp==V3D_CAMOB) {
		t->flag |= T_CAMERA;
	}
	
	/* temporal...? */
	G.scene->recalc |= SCE_PRV_CHANGED;	/* test for 3d preview */
}



