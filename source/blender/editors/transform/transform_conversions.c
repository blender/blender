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

#ifndef WIN32
#include <unistd.h>
#else
#include <io.h>
#endif
#include <string.h>
#include <math.h>

#include "MEM_guardedalloc.h"

#include "DNA_anim_types.h"
#include "DNA_armature_types.h"
#include "DNA_lattice_types.h"
#include "DNA_meta_types.h"
#include "DNA_node_types.h"
#include "DNA_screen_types.h"
#include "DNA_space_types.h"
#include "DNA_sequence_types.h"
#include "DNA_view3d_types.h"
#include "DNA_constraint_types.h"

#include "BKE_action.h"
#include "BKE_armature.h"
#include "BKE_blender.h"
#include "BKE_cloth.h"
#include "BKE_context.h"
#include "BKE_curve.h"
#include "BKE_constraint.h"
#include "BKE_depsgraph.h"
#include "BKE_displist.h"
#include "BKE_DerivedMesh.h"
#include "BKE_effect.h"
#include "BKE_font.h"
#include "BKE_fcurve.h"
#include "BKE_global.h"
#include "BKE_lattice.h"
#include "BKE_key.h"
#include "BKE_main.h"
#include "BKE_mball.h"
#include "BKE_mesh.h"
#include "BKE_modifier.h"
#include "BKE_nla.h"
#include "BKE_object.h"
#include "BKE_particle.h"
#include "BKE_sequencer.h"
#include "BKE_pointcache.h"
#include "BKE_softbody.h"
#include "BKE_utildefines.h"
#include "BKE_bmesh.h"
#include "BKE_context.h"
#include "BKE_report.h"
#include "BKE_scene.h"

#include "BIF_gl.h"

#include "ED_anim_api.h"
#include "ED_armature.h"
#include "ED_particle.h"
#include "ED_image.h"
#include "ED_keyframing.h"
#include "ED_keyframes_edit.h"
#include "ED_object.h"
#include "ED_markers.h"
#include "ED_mesh.h"
#include "ED_types.h"
#include "ED_uvedit.h"

#include "UI_view2d.h"

#include "BLI_math.h"
#include "BLI_blenlib.h"
#include "BLI_editVert.h"

#include "RNA_access.h"

extern ListBase editelems;

#include "transform.h"

#include "BLO_sys_types.h" // for intptr_t support

/* local function prototype - for Object/Bone Constraints */
static short constraints_list_needinv(TransInfo *t, ListBase *list);

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
					sub_v3_v3v3(vec, tob->center, td->center);
					mul_m3_v3(tob->mtx, vec);
					dist = normalize_v3(vec);
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

static void createTransTexspace(bContext *C, TransInfo *t)
{
	Scene *scene = t->scene;
	TransData *td;
	Object *ob;
	ID *id;
	short *texflag;

	ob = OBACT;

	if (ob == NULL) { // Shouldn't logically happen, but still...
		t->total = 0;
		return;
	}

	id = ob->data;
	if(id == NULL || !ELEM3( GS(id->name), ID_ME, ID_CU, ID_MB )) {
		t->total = 0;
		return;
	}

	t->total = 1;
	td= t->data= MEM_callocN(sizeof(TransData), "TransTexspace");
	td->ext= t->ext= MEM_callocN(sizeof(TransDataExtension), "TransTexspace");

	td->flag= TD_SELECTED;
	VECCOPY(td->center, ob->obmat[3]);
	td->ob = ob;

	copy_m3_m4(td->mtx, ob->obmat);
	copy_m3_m4(td->axismtx, ob->obmat);
	normalize_m3(td->axismtx);
	invert_m3_m3(td->smtx, td->mtx);

	if (give_obdata_texspace(ob, &texflag, &td->loc, &td->ext->size, &td->ext->rot)) {
		*texflag &= ~AUTOSPACE;
	}

	VECCOPY(td->iloc, td->loc);
	VECCOPY(td->ext->irot, td->ext->rot);
	VECCOPY(td->ext->isize, td->ext->size);
}

/* ********************* edge (for crease) ***** */

static void createTransEdge(bContext *C, TransInfo *t) {
	EditMesh *em = ((Mesh *)t->obedit->data)->edit_mesh;
	TransData *td = NULL;
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

	copy_m3_m4(mtx, t->obedit->obmat);
	invert_m3_m3(smtx, mtx);

	for(eed= em->edges.first; eed; eed= eed->next) {
		if(eed->h==0 && (eed->f & SELECT || propmode)) {
			/* need to set center for center calculations */
			add_v3_v3v3(td->center, eed->v1->co, eed->v2->co);
			mul_v3_fl(td->center, 0.5f);

			td->loc= NULL;
			if (eed->f & SELECT)
				td->flag= TD_SELECTED;
			else
				td->flag= 0;


			copy_m3_m3(td->smtx, smtx);
			copy_m3_m3(td->mtx, mtx);

			td->ext = NULL;
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
		if(con->type==CONSTRAINT_TYPE_KINEMATIC && (con->enforce!=0.0)) {
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
					copy_m4_m3(offs_bone, bone->bone_mat);

					/* The bone's root offset (is in the parent's coordinate system) */
					VECCOPY(offs_bone[3], bone->head);

					/* Get the length translation of parent (length along y axis) */
					offs_bone[3][1]+= parbone->length;

					/* pose_mat(b-1) * offs_bone */
					if(parchan->bone->flag & BONE_HINGE) {
						/* the rotation of the parent restposition */
						copy_m4_m4(rmat, parbone->arm_mat);	/* rmat used as temp */

						/* the location of actual parent transform */
						VECCOPY(rmat[3], offs_bone[3]);
						offs_bone[3][0]= offs_bone[3][1]= offs_bone[3][2]= 0.0f;
						mul_m4_v3(parchan->parent->pose_mat, rmat[3]);

						mul_m4_m4m4(tmat, offs_bone, rmat);
					}
					else if(parchan->bone->flag & BONE_NO_SCALE) {
						mul_m4_m4m4(tmat, offs_bone, parchan->parent->pose_mat);
						normalize_m4(tmat);
					}
					else
						mul_m4_m4m4(tmat, offs_bone, parchan->parent->pose_mat);

					invert_m4_m4(imat, tmat);
				}
				else {
					copy_m4_m3(tmat, bone->bone_mat);

					VECCOPY(tmat[3], bone->head);
					invert_m4_m4(imat, tmat);
				}
				/* result matrix */
				mul_m4_m4m4(rmat, parchan->pose_mat, imat);

				/* apply and decompose, doesn't work for constraints or non-uniform scale well */
				{
					float rmat3[3][3], qrmat[3][3], imat[3][3], smat[3][3];

					copy_m3_m4(rmat3, rmat);
					
					/* rotation */
					if (parchan->rotmode > 0) 
						mat3_to_eulO( parchan->eul, parchan->rotmode,rmat3);
					else if (parchan->rotmode == ROT_MODE_AXISANGLE)
						mat3_to_axis_angle( parchan->rotAxis, &pchan->rotAngle,rmat3);
					else
						mat3_to_quat( parchan->quat,rmat3);
					
					/* for size, remove rotation */
					/* causes problems with some constraints (so apply only if needed) */
					if (data->flag & CONSTRAINT_IK_STRETCH) {
						if (parchan->rotmode > 0)
							eulO_to_mat3( qrmat,parchan->eul, parchan->rotmode);
						else if (parchan->rotmode == ROT_MODE_AXISANGLE)
							axis_angle_to_mat3( qrmat,parchan->rotAxis, parchan->rotAngle);
						else
							quat_to_mat3( qrmat,parchan->quat);
						
						invert_m3_m3(imat, qrmat);
						mul_m3_m3m3(smat, rmat3, imat);
						mat3_to_size( parchan->size,smat);
					}
					
					/* causes problems with some constraints (e.g. childof), so disable this */
					/* as it is IK shouldn't affect location directly */
					/* VECCOPY(parchan->loc, rmat[3]); */
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
	float pmat[3][3], omat[3][3], bmat[3][3];
	float cmat[3][3], tmat[3][3];
	float vec[3];

	VECCOPY(vec, pchan->pose_mat[3]);
	VECCOPY(td->center, vec);

	td->ob = ob;
	td->flag = TD_SELECTED;
	if (bone->flag & BONE_HINGE_CHILD_TRANSFORM)
	{
		td->flag |= TD_NOCENTER;
	}

	if (bone->flag & BONE_TRANSFORM_CHILD)
	{
		td->flag |= TD_NOCENTER;
		td->flag |= TD_NO_LOC;
	}

	td->protectflag= pchan->protectflag;

	td->loc = pchan->loc;
	VECCOPY(td->iloc, pchan->loc);

	td->ext->size= pchan->size;
	VECCOPY(td->ext->isize, pchan->size);

	if (pchan->rotmode > 0) {
		td->ext->rot= pchan->eul;
		td->ext->rotAxis= NULL;
		td->ext->rotAngle= NULL;
		td->ext->quat= NULL;
		
		VECCOPY(td->ext->irot, pchan->eul);
	}
	else if (pchan->rotmode == ROT_MODE_AXISANGLE) {
		td->ext->rot= NULL;
		td->ext->rotAxis= pchan->rotAxis;
		td->ext->rotAngle= &pchan->rotAngle;
		td->ext->quat= NULL;
		
		td->ext->irotAngle= pchan->rotAngle;
		VECCOPY(td->ext->irotAxis, pchan->rotAxis);
	}
	else {
		td->ext->rot= NULL;
		td->ext->rotAxis= NULL;
		td->ext->rotAngle= NULL;
		td->ext->quat= pchan->quat;
		
		QUATCOPY(td->ext->iquat, pchan->quat);
	}
	td->rotOrder= pchan->rotmode;
	

	/* proper way to get parent transform + own transform + constraints transform */
	copy_m3_m4(omat, ob->obmat);

	if (t->mode==TFM_TRANSLATION && (pchan->bone->flag & BONE_NO_LOCAL_LOCATION))
		unit_m3(bmat);
	else
		copy_m3_m3(bmat, pchan->bone->bone_mat);

	if (pchan->parent) {
		if(pchan->bone->flag & BONE_HINGE)
			copy_m3_m4(pmat, pchan->parent->bone->arm_mat);
		else
			copy_m3_m4(pmat, pchan->parent->pose_mat);

		if (constraints_list_needinv(t, &pchan->constraints)) {
			copy_m3_m4(tmat, pchan->constinv);
			invert_m3_m3(cmat, tmat);
			mul_serie_m3(td->mtx, bmat, pmat, omat, cmat, 0,0,0,0);    // dang mulserie swaps args
		}
		else
			mul_serie_m3(td->mtx, bmat, pmat, omat, 0,0,0,0,0);    // dang mulserie swaps args
	}
	else {
		if (constraints_list_needinv(t, &pchan->constraints)) {
			copy_m3_m4(tmat, pchan->constinv);
			invert_m3_m3(cmat, tmat);
			mul_serie_m3(td->mtx, bmat, omat, cmat, 0,0,0,0,0);    // dang mulserie swaps args
		}
		else
			mul_m3_m3m3(td->mtx, omat, bmat);  // Mat3MulMat3 has swapped args!
	}

	invert_m3_m3(td->smtx, td->mtx);

	/* for axismat we use bone's own transform */
	copy_m3_m4(pmat, pchan->pose_mat);
	mul_m3_m3m3(td->axismtx, omat, pmat);
	normalize_m3(td->axismtx);

	if (t->mode==TFM_BONESIZE) {
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
	if (t->mode==TFM_TRANSLATION) {
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
			copy_m3_m3(td->mtx, omat);
			invert_m3_m3(td->smtx, td->mtx);
		}
	}

	/* store reference to first constraint */
	td->con= pchan->constraints.first;
}

static void bone_children_clear_transflag(int mode, short around, ListBase *lb)
{
	Bone *bone= lb->first;

	for(;bone;bone= bone->next) {
		if((bone->flag & BONE_HINGE) && (bone->flag & BONE_CONNECTED))
		{
			bone->flag |= BONE_HINGE_CHILD_TRANSFORM;
		}
		else if (bone->flag & BONE_TRANSFORM && (mode == TFM_ROTATION || mode == TFM_TRACKBALL) && around == V3D_LOCAL)
		{
			bone->flag |= BONE_TRANSFORM_CHILD;
		}
		else
		{
			bone->flag &= ~BONE_TRANSFORM;
		}

		bone_children_clear_transflag(mode, around, &bone->childbase);
	}
}

/* sets transform flags in the bones, returns total */
int count_set_pose_transflags(int *out_mode, short around, Object *ob)
{
	bArmature *arm= ob->data;
	bPoseChannel *pchan;
	Bone *bone;
	int mode = *out_mode;
	int hastranslation = 0;
	int total = 0;

	for (pchan = ob->pose->chanbase.first; pchan; pchan = pchan->next) {
		bone = pchan->bone;
		if ((bone->layer & arm->layer) && !(pchan->bone->flag & BONE_HIDDEN_P)) {
			if ((bone->flag & BONE_SELECTED) && !(ob->proxy && pchan->bone->layer & arm->layer_protected))
				bone->flag |= BONE_TRANSFORM;
			else
				bone->flag &= ~BONE_TRANSFORM;
			
			bone->flag &= ~BONE_HINGE_CHILD_TRANSFORM;
			bone->flag &= ~BONE_TRANSFORM_CHILD;
		}
		else
			bone->flag &= ~BONE_TRANSFORM;
	}

	/* make sure no bone can be transformed when a parent is transformed */
	/* since pchans are depsgraph sorted, the parents are in beginning of list */
	if(mode != TFM_BONESIZE) {
		for(pchan = ob->pose->chanbase.first; pchan; pchan = pchan->next) {
			bone = pchan->bone;
			if(bone->flag & BONE_TRANSFORM)
				bone_children_clear_transflag(mode, around, &bone->childbase);
		}
	}
	/* now count, and check if we have autoIK or have to switch from translate to rotate */
	hastranslation = 0;

	for(pchan = ob->pose->chanbase.first; pchan; pchan = pchan->next) {
		bone = pchan->bone;
		if(bone->flag & BONE_TRANSFORM) {

			total++;

			if(mode == TFM_TRANSLATION) {
				if( has_targetless_ik(pchan)==NULL ) {
					if(pchan->parent && (pchan->bone->flag & BONE_CONNECTED)) {
						if(pchan->bone->flag & BONE_HINGE_CHILD_TRANSFORM)
							hastranslation = 1;
					}
					else if((pchan->protectflag & OB_LOCK_LOC)!=OB_LOCK_LOC)
						hastranslation = 1;
				}
				else
					hastranslation = 1;
			}
		}
	}

	/* if there are no translatable bones, do rotation */
	if(mode == TFM_TRANSLATION && !hastranslation)
	{
		*out_mode = TFM_ROTATION;
	}

	return total;
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
		if (con->type == CONSTRAINT_TYPE_KINEMATIC && (con->enforce!=0.0)) {
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
	short *chainlen= &t->settings->autoik_chainlen;
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
	bConstraint *con, *next;

	for (pchan= ob->pose->chanbase.first; pchan; pchan= pchan->next) {
		/* clear all temporary lock flags */
		pchan->ikflag &= ~(BONE_IK_NO_XDOF_TEMP|BONE_IK_NO_YDOF_TEMP|BONE_IK_NO_ZDOF_TEMP);

		pchan->constflag &= ~(PCHAN_HAS_IK|PCHAN_HAS_TARGET);
		/* remove all temporary IK-constraints added */
		for (con= pchan->constraints.first; con; con= next) {
			next= con->next;
			if (con->type==CONSTRAINT_TYPE_KINEMATIC) {
				data= con->data;
				if (data->flag & CONSTRAINT_IK_TEMP) {
					BLI_remlink(&pchan->constraints, con);
					MEM_freeN(con->data);
					MEM_freeN(con);
					continue;
				}
				pchan->constflag |= PCHAN_HAS_IK;
				if(data->tar==NULL || (data->tar->type==OB_ARMATURE && data->subtarget[0]==0))
					pchan->constflag |= PCHAN_HAS_TARGET;
			}
		}
	}
}

/* adds the IK to pchan - returns if added */
static short pose_grab_with_ik_add(bPoseChannel *pchan)
{
	bKinematicConstraint *data;
	bConstraint *con;
	bConstraint *targetless = 0;

	/* Sanity check */
	if (pchan == NULL)
		return 0;

	/* Rule: not if there's already an IK on this channel */
	for (con= pchan->constraints.first; con; con= con->next) {
		if (con->type==CONSTRAINT_TYPE_KINEMATIC) {
			bKinematicConstraint *data= con->data;
			if(data->tar==NULL || (data->tar->type==OB_ARMATURE && data->subtarget[0]==0)) {
				targetless = con;
				/* but, if this is a targetless IK, we make it auto anyway (for the children loop) */
				if (con->enforce!=0.0f) {
					targetless->flag |= CONSTRAINT_IK_AUTO;
					return 0;
				}
			}
			if ((con->flag & CONSTRAINT_DISABLE)==0 && (con->enforce!=0.0f))
				return 0;
		}
	}

	con = add_pose_constraint(NULL, pchan, "TempConstraint", CONSTRAINT_TYPE_KINEMATIC);
	pchan->constflag |= (PCHAN_HAS_IK|PCHAN_HAS_TARGET);	/* for draw, but also for detecting while pose solving */
	data= con->data;
	if (targetless) { /* if exists use values from last targetless IK-constraint as base */
		*data = *((bKinematicConstraint*)targetless->data);
	}
	else
		data->flag= CONSTRAINT_IK_TIP;
	data->flag |= CONSTRAINT_IK_TEMP|CONSTRAINT_IK_AUTO;
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

	if ((ob==NULL) || (ob->pose==NULL) || (ob->mode & OB_MODE_POSE)==0)
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
static void createTransPose(bContext *C, TransInfo *t, Object *ob)
{
	bArmature *arm;
	bPoseChannel *pchan;
	TransData *td;
	TransDataExtension *tdx;
	short ik_on= 0;
	int i;

	t->total= 0;

	/* check validity of state */
	arm= get_armature(ob);
	if ((arm==NULL) || (ob->pose==NULL)) return;

	if (arm->flag & ARM_RESTPOS) {
		if (ELEM(t->mode, TFM_DUMMY, TFM_BONESIZE)==0) {
			// XXX use transform operator reports
			// BKE_report(op->reports, RPT_ERROR, "Can't select linked when sync selection is enabled.");
			return;
		}
	}

	/* do we need to add temporal IK chains? */
	if ((arm->flag & ARM_AUTO_IK) && t->mode==TFM_TRANSLATION) {
		ik_on= pose_grab_with_ik(ob);
		if (ik_on) t->flag |= T_AUTOIK;
	}

	/* set flags and count total (warning, can change transform to rotate) */
	t->total = count_set_pose_transflags(&t->mode, t->around, ob);

	if(t->total == 0) return;

	t->flag |= T_POSE;
	t->poseobj= ob;	/* we also allow non-active objects to be transformed, in weightpaint */

	/* init trans data */
	td = t->data = MEM_callocN(t->total*sizeof(TransData), "TransPoseBone");
	tdx = t->ext = MEM_callocN(t->total*sizeof(TransDataExtension), "TransPoseBoneExt");
	for(i=0; i<t->total; i++, td++, tdx++) {
		td->ext= tdx;
		td->val = NULL;
	}

	/* use pose channels to fill trans data */
	td= t->data;
	for (pchan= ob->pose->chanbase.first; pchan; pchan= pchan->next) {
		if (pchan->bone->flag & BONE_TRANSFORM) {
			add_pose_transdata(t, pchan, ob, td);
			td++;
		}
	}

	if(td != (t->data+t->total)) {
		// XXX use transform operator reports
		// BKE_report(op->reports, RPT_DEBUG, "Bone selection count error.");
	}

	/* initialise initial auto=ik chainlen's? */
	if (ik_on) transform_autoik_update(t, 0);
}

/* ********************* armature ************** */

static void createTransArmatureVerts(bContext *C, TransInfo *t)
{
	EditBone *ebo;
	bArmature *arm= t->obedit->data;
	ListBase *edbo = arm->edbo;
	TransData *td;
	float mtx[3][3], smtx[3][3], delta[3], bonemat[3][3];
	
	/* special hack for envelope drawmode and scaling:
	 * 	to allow scaling the size of the envelope around single points,
	 *	mode should become TFM_BONE_ENVELOPE in this case
	 */
	// TODO: maybe we need a separate hotkey for it, but this is consistent with 2.4x for now
	if ((t->mode == TFM_RESIZE) && (arm->drawtype==ARM_ENVELOPE))
		t->mode= TFM_BONE_ENVELOPE;
	
	t->total = 0;
	for (ebo = edbo->first; ebo; ebo = ebo->next)
	{
		if (EBONE_VISIBLE(arm, ebo) && !(ebo->flag & BONE_EDITMODE_LOCKED)) 
		{
			if (t->mode==TFM_BONESIZE)
			{
				if (ebo->flag & BONE_SELECTED)
					t->total++;
			}
			else if (t->mode==TFM_BONE_ROLL)
			{
				if (ebo->flag & BONE_SELECTED)
					t->total++;
			}
			else
			{
				if (ebo->flag & BONE_TIPSEL)
					t->total++;
				if (ebo->flag & BONE_ROOTSEL)
					t->total++;
			}
		}
	}

	if (!t->total) return;

	copy_m3_m4(mtx, t->obedit->obmat);
	invert_m3_m3(smtx, mtx);

	td = t->data = MEM_callocN(t->total*sizeof(TransData), "TransEditBone");

	for (ebo = edbo->first; ebo; ebo = ebo->next)
	{
		ebo->oldlength = ebo->length;	// length==0.0 on extrude, used for scaling radius of bone points

		if (EBONE_VISIBLE(arm, ebo) && !(ebo->flag & BONE_EDITMODE_LOCKED)) 
		{
			if (t->mode==TFM_BONE_ENVELOPE)
			{
				if (ebo->flag & BONE_ROOTSEL)
				{
					td->val= &ebo->rad_head;
					td->ival= *td->val;

					VECCOPY (td->center, ebo->head);
					td->flag= TD_SELECTED;

					copy_m3_m3(td->smtx, smtx);
					copy_m3_m3(td->mtx, mtx);

					td->loc = NULL;
					td->ext = NULL;
					td->ob = t->obedit;

					td++;
				}
				if (ebo->flag & BONE_TIPSEL)
				{
					td->val= &ebo->rad_tail;
					td->ival= *td->val;
					VECCOPY (td->center, ebo->tail);
					td->flag= TD_SELECTED;

					copy_m3_m3(td->smtx, smtx);
					copy_m3_m3(td->mtx, mtx);

					td->loc = NULL;
					td->ext = NULL;
					td->ob = t->obedit;

					td++;
				}

			}
			else if (t->mode==TFM_BONESIZE)
			{
				if (ebo->flag & BONE_SELECTED) {
					if(arm->drawtype==ARM_ENVELOPE)
					{
						td->loc= NULL;
						td->val= &ebo->dist;
						td->ival= ebo->dist;
					}
					else
					{
						// abusive storage of scale in the loc pointer :)
						td->loc= &ebo->xwidth;
						VECCOPY (td->iloc, td->loc);
						td->val= NULL;
					}
					VECCOPY (td->center, ebo->head);
					td->flag= TD_SELECTED;

					/* use local bone matrix */
					sub_v3_v3v3(delta, ebo->tail, ebo->head);
					vec_roll_to_mat3(delta, ebo->roll, bonemat);
					mul_m3_m3m3(td->mtx, mtx, bonemat);
					invert_m3_m3(td->smtx, td->mtx);

					copy_m3_m3(td->axismtx, td->mtx);
					normalize_m3(td->axismtx);

					td->ext = NULL;
					td->ob = t->obedit;

					td++;
				}
			}
			else if (t->mode==TFM_BONE_ROLL)
			{
				if (ebo->flag & BONE_SELECTED)
				{
					td->loc= NULL;
					td->val= &(ebo->roll);
					td->ival= ebo->roll;

					VECCOPY (td->center, ebo->head);
					td->flag= TD_SELECTED;

					td->ext = NULL;
					td->ob = t->obedit;

					td++;
				}
			}
			else
			{
				if (ebo->flag & BONE_TIPSEL)
				{
					VECCOPY (td->iloc, ebo->tail);
					VECCOPY (td->center, td->iloc);
					td->loc= ebo->tail;
					td->flag= TD_SELECTED;
					if (ebo->flag & BONE_EDITMODE_LOCKED)
						td->protectflag = OB_LOCK_LOC|OB_LOCK_ROT|OB_LOCK_SCALE;

					copy_m3_m3(td->smtx, smtx);
					copy_m3_m3(td->mtx, mtx);

					sub_v3_v3v3(delta, ebo->tail, ebo->head);
					vec_roll_to_mat3(delta, ebo->roll, td->axismtx);

					if ((ebo->flag & BONE_ROOTSEL) == 0)
					{
						td->extra = ebo;
					}

					td->ext = NULL;
					td->val = NULL;
					td->ob = t->obedit;

					td++;
				}
				if (ebo->flag & BONE_ROOTSEL)
				{
					VECCOPY (td->iloc, ebo->head);
					VECCOPY (td->center, td->iloc);
					td->loc= ebo->head;
					td->flag= TD_SELECTED;
					if (ebo->flag & BONE_EDITMODE_LOCKED)
						td->protectflag = OB_LOCK_LOC|OB_LOCK_ROT|OB_LOCK_SCALE;

					copy_m3_m3(td->smtx, smtx);
					copy_m3_m3(td->mtx, mtx);

					sub_v3_v3v3(delta, ebo->tail, ebo->head);
					vec_roll_to_mat3(delta, ebo->roll, td->axismtx);

					td->extra = ebo; /* to fix roll */

					td->ext = NULL;
					td->val = NULL;
					td->ob = t->obedit;

					td++;
				}
			}
		}
	}
}

/* ********************* meta elements ********* */

static void createTransMBallVerts(bContext *C, TransInfo *t)
{
	MetaBall *mb = (MetaBall*)t->obedit->data;
	 MetaElem *ml;
	TransData *td;
	TransDataExtension *tx;
	float mtx[3][3], smtx[3][3];
	int count=0, countsel=0;
	int propmode = t->flag & T_PROP_EDIT;

	/* count totals */
	for(ml= mb->editelems->first; ml; ml= ml->next) {
		if(ml->flag & SELECT) countsel++;
		if(propmode) count++;
	}

	/* note: in prop mode we need at least 1 selected */
	if (countsel==0) return;

	if(propmode) t->total = count;
	else t->total = countsel;

	td = t->data= MEM_callocN(t->total*sizeof(TransData), "TransObData(MBall EditMode)");
	tx = t->ext = MEM_callocN(t->total*sizeof(TransDataExtension), "MetaElement_TransExtension");

	copy_m3_m4(mtx, t->obedit->obmat);
	invert_m3_m3(smtx, mtx);

	for(ml= mb->editelems->first; ml; ml= ml->next) {
		if(propmode || (ml->flag & SELECT)) {
			td->loc= &ml->x;
			VECCOPY(td->iloc, td->loc);
			VECCOPY(td->center, td->loc);

			if(ml->flag & SELECT) td->flag= TD_SELECTED | TD_USEQUAT | TD_SINGLESIZE;
			else td->flag= TD_USEQUAT;

			copy_m3_m3(td->smtx, smtx);
			copy_m3_m3(td->mtx, mtx);

			td->ext = tx;

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
			dist = len_v3v3(td_near->center, td->center);
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
			dist = len_v3v3(td_near->center, td->center);
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
TransDataCurveHandleFlags *initTransDataCurveHandles(TransData *td, struct BezTriple *bezt) {
	TransDataCurveHandleFlags *hdata;
	td->flag |= TD_BEZTRIPLE;
	hdata = td->hdata = MEM_mallocN(sizeof(TransDataCurveHandleFlags), "CuHandle Data");
	hdata->ih1 = bezt->h1;
	hdata->h1 = &bezt->h1;
	hdata->ih2 = bezt->h2; /* incase the second is not selected */
	hdata->h2 = &bezt->h2;
	return hdata;
}

static void createTransCurveVerts(bContext *C, TransInfo *t)
{
	Object *obedit= CTX_data_edit_object(C);
	Curve *cu= obedit->data;
	TransData *td = NULL;
	  Nurb *nu;
	BezTriple *bezt;
	BPoint *bp;
	float mtx[3][3], smtx[3][3];
	int a;
	int count=0, countsel=0;
	int propmode = t->flag & T_PROP_EDIT;
	short hide_handles = (cu->drawflag & CU_HIDE_HANDLES);
	
	/* to be sure */
	if(cu->editnurb==NULL) return;

	/* count total of vertices, check identical as in 2nd loop for making transdata! */
	for(nu= cu->editnurb->first; nu; nu= nu->next) {
		if(nu->type == CU_BEZIER) {
			for(a=0, bezt= nu->bezt; a<nu->pntsu; a++, bezt++) {
				if(bezt->hide==0) {
					if (hide_handles) {
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

	copy_m3_m4(mtx, t->obedit->obmat);
	invert_m3_m3(smtx, mtx);

	td = t->data;
	for(nu= cu->editnurb->first; nu; nu= nu->next) {
		if(nu->type == CU_BEZIER) {
			TransData *head, *tail;
			head = tail = td;
			for(a=0, bezt= nu->bezt; a<nu->pntsu; a++, bezt++) {
				if(bezt->hide==0) {
					TransDataCurveHandleFlags *hdata = NULL;

					if(		propmode ||
							((bezt->f2 & SELECT) && hide_handles) ||
							((bezt->f1 & SELECT) && hide_handles == 0)
					  ) {
						VECCOPY(td->iloc, bezt->vec[0]);
						td->loc= bezt->vec[0];
						VECCOPY(td->center, bezt->vec[1]);
						if (hide_handles) {
							if(bezt->f2 & SELECT) td->flag= TD_SELECTED;
							else td->flag= 0;
						} else {
							if(bezt->f1 & SELECT) td->flag= TD_SELECTED;
							else td->flag= 0;
						}
						td->ext = NULL;
						td->val = NULL;

						hdata = initTransDataCurveHandles(td, bezt);

						copy_m3_m3(td->smtx, smtx);
						copy_m3_m3(td->mtx, mtx);

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

						if (t->mode==TFM_CURVE_SHRINKFATTEN) { /* || t->mode==TFM_RESIZE) {*/ /* TODO - make points scale */
							td->val = &(bezt->radius);
							td->ival = bezt->radius;
						} else if (t->mode==TFM_TILT) {
							td->val = &(bezt->alfa);
							td->ival = bezt->alfa;
						} else {
							td->val = NULL;
						}

						copy_m3_m3(td->smtx, smtx);
						copy_m3_m3(td->mtx, mtx);

						if ((bezt->f1&SELECT)==0 && (bezt->f3&SELECT)==0)
						/* If the middle is selected but the sides arnt, this is needed */
						if (hdata==NULL) { /* if the handle was not saved by the previous handle */
							hdata = initTransDataCurveHandles(td, bezt);
						}

						td++;
						count++;
						tail++;
					}
					if(		propmode ||
							((bezt->f2 & SELECT) && hide_handles) ||
							((bezt->f3 & SELECT) && hide_handles == 0)
					  ) {
						VECCOPY(td->iloc, bezt->vec[2]);
						td->loc= bezt->vec[2];
						VECCOPY(td->center, bezt->vec[1]);
						if (hide_handles) {
							if(bezt->f2 & SELECT) td->flag= TD_SELECTED;
							else td->flag= 0;
						} else {
							if(bezt->f3 & SELECT) td->flag= TD_SELECTED;
							else td->flag= 0;
						}
						td->ext = NULL;
						td->val = NULL;

						if (hdata==NULL) { /* if the handle was not saved by the previous handle */
							hdata = initTransDataCurveHandles(td, bezt);
						}

						copy_m3_m3(td->smtx, smtx);
						copy_m3_m3(td->mtx, mtx);

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

			/* TODO - in the case of tilt and radius we can also avoid allocating the initTransDataCurveHandles
			 * but for now just dont change handle types */
			if (ELEM(t->mode, TFM_CURVE_SHRINKFATTEN, TFM_TILT) == 0)
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

						if (t->mode==TFM_CURVE_SHRINKFATTEN || t->mode==TFM_RESIZE) {
							td->val = &(bp->radius);
							td->ival = bp->radius;
						} else {
							td->val = &(bp->alfa);
							td->ival = bp->alfa;
						}

						copy_m3_m3(td->smtx, smtx);
						copy_m3_m3(td->mtx, mtx);

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

static void createTransLatticeVerts(bContext *C, TransInfo *t)
{
	Lattice *latt = ((Lattice*)t->obedit->data)->editlatt;
	TransData *td = NULL;
	BPoint *bp;
	float mtx[3][3], smtx[3][3];
	int a;
	int count=0, countsel=0;
	int propmode = t->flag & T_PROP_EDIT;

	bp = latt->def;
	a  = latt->pntsu * latt->pntsv * latt->pntsw;
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

	copy_m3_m4(mtx, t->obedit->obmat);
	invert_m3_m3(smtx, mtx);

	td = t->data;
	bp = latt->def;
	a  = latt->pntsu * latt->pntsv * latt->pntsw;
	while(a--) {
		if(propmode || (bp->f1 & SELECT)) {
			if(bp->hide==0) {
				VECCOPY(td->iloc, bp->vec);
				td->loc= bp->vec;
				VECCOPY(td->center, td->loc);
				if(bp->f1 & SELECT) td->flag= TD_SELECTED;
				else td->flag= 0;
				copy_m3_m3(td->smtx, smtx);
				copy_m3_m3(td->mtx, mtx);

				td->ext = NULL;
				td->val = NULL;

				td++;
				count++;
			}
		}
		bp++;
	}
}

/* ******************* particle edit **************** */
static void createTransParticleVerts(bContext *C, TransInfo *t)
{
	TransData *td = NULL;
	TransDataExtension *tx;
	Base *base = CTX_data_active_base(C);
	Object *ob = CTX_data_active_object(C);
	ParticleEditSettings *pset = PE_settings(t->scene);
	PTCacheEdit *edit = PE_get_current(t->scene, ob);
	ParticleSystem *psys = NULL;
	ParticleSystemModifierData *psmd = NULL;
	PTCacheEditPoint *point;
	PTCacheEditKey *key;
	float mat[4][4];
	int i,k, transformparticle;
	int count = 0, hasselected = 0;
	int propmode = t->flag & T_PROP_EDIT;

	if(edit==NULL || t->settings->particle.selectmode==SCE_SELECT_PATH) return;

	psys = edit->psys;

	if(psys)
		psmd = psys_get_modifier(ob,psys);

	base->flag |= BA_HAS_RECALC_DATA;

	for(i=0, point=edit->points; i<edit->totpoint; i++, point++) {
		point->flag &= ~PEP_TRANSFORM;
		transformparticle= 0;

		if((point->flag & PEP_HIDE)==0) {
			for(k=0, key=point->keys; k<point->totkey; k++, key++) {
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
			count += point->totkey;
			point->flag |= PEP_TRANSFORM;
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

	unit_m4(mat);

	invert_m4_m4(ob->imat,ob->obmat);

	for(i=0, point=edit->points; i<edit->totpoint; i++, point++) {
		TransData *head, *tail;
		head = tail = td;

		if(!(point->flag & PEP_TRANSFORM)) continue;

		if(psys && !(psys->flag & PSYS_GLOBAL_HAIR))
			psys_mat_hair_to_global(ob, psmd->dm, psys->part->from, psys->particles + i, mat);

		for(k=0, key=point->keys; k<point->totkey; k++, key++) {
			if(key->flag & PEK_USE_WCO) {
				VECCOPY(key->world_co, key->co);
				mul_m4_v3(mat, key->world_co);
				td->loc = key->world_co;
			}
			else
				td->loc = key->co;

			VECCOPY(td->iloc, td->loc);
			VECCOPY(td->center, td->loc);

			if(key->flag & PEK_SELECT)
				td->flag |= TD_SELECTED;
			else if(!propmode)
				td->flag |= TD_SKIP;

			unit_m3(td->mtx);
			unit_m3(td->smtx);

			/* don't allow moving roots */
			if(k==0 && pset->flag & PE_LOCK_FIRST && (!psys || !(psys->flag & PSYS_GLOBAL_HAIR)))
				td->protectflag |= OB_LOCK_LOC;

			td->ob = ob;
			td->ext = tx;
			if(t->mode == TFM_BAKE_TIME) {
				td->val = key->time;
				td->ival = *(key->time);
				/* abuse size and quat for min/max values */
				td->flag |= TD_NO_EXT;
				if(k==0) tx->size = 0;
				else tx->size = (key - 1)->time;

				if(k == point->totkey - 1) tx->quat = 0;
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
	Scene *scene = t->scene;
	Object *ob = OBACT;
	PTCacheEdit *edit = PE_get_current(scene, ob);
	ParticleSystem *psys = edit->psys;
	ParticleSystemModifierData *psmd = NULL;
	PTCacheEditPoint *point;
	PTCacheEditKey *key;
	TransData *td;
	float mat[4][4], imat[4][4], co[3];
	int i, k, propmode = t->flag & T_PROP_EDIT;

	if(psys)
		psmd = psys_get_modifier(ob, psys);

	/* we do transform in world space, so flush world space position
	 * back to particle local space (only for hair particles) */
	td= t->data;
	for(i=0, point=edit->points; i<edit->totpoint; i++, point++, td++) {
		if(!(point->flag & PEP_TRANSFORM)) continue;

		if(psys && !(psys->flag & PSYS_GLOBAL_HAIR)) {
			psys_mat_hair_to_global(ob, psmd->dm, psys->part->from, psys->particles + i, mat);
			invert_m4_m4(imat,mat);

			for(k=0, key=point->keys; k<point->totkey; k++, key++) {
				VECCOPY(co, key->world_co);
				mul_m4_v3(imat, co);


				/* optimization for proportional edit */
				if(!propmode || !compare_v3v3(key->co, co, 0.0001f)) {
					VECCOPY(key->co, co);
					point->flag |= PEP_EDIT_RECALC;
				}
			}
		}
		else
			point->flag |= PEP_EDIT_RECALC;
	}

	PE_update_object(scene, OBACT, 1);
}

/* ********************* mesh ****************** */

/* proportional distance based on connectivity  */
#define THRESHOLD	0.0001f

static int connectivity_edge(float mtx[][3], EditVert *v1, EditVert *v2)
{
	float edge_vec[3];
	float edge_len;
	int done = 0;

	sub_v3_v3v3(edge_vec, v1->co, v2->co);
	mul_m3_v3(mtx, edge_vec);

	edge_len = len_v3(edge_vec);

	if (v1->f2 + v2->f2 == 4)
		return 0;

	if (v1->f2) {
		if (v2->f2) {
			if (v2->tmp.fp + edge_len + THRESHOLD < v1->tmp.fp) {
				v1->tmp.fp = v2->tmp.fp + edge_len;
				done = 1;
			} else if (v1->tmp.fp + edge_len + THRESHOLD < v2->tmp.fp) {
				v2->tmp.fp = v1->tmp.fp + edge_len;
				done = 1;
			}
		}
		else {
			v2->f2 = 1;
			v2->tmp.fp = v1->tmp.fp + edge_len;
			done = 1;
		}
	}
	else if (v2->f2) {
		v1->f2 = 1;
		v1->tmp.fp = v2->tmp.fp + edge_len;
		done = 1;
	}

	return done;
}

static void editmesh_set_connectivity_distance(EditMesh *em, float mtx[][3])
{
	EditVert *eve;
	EditEdge *eed;
	EditFace *efa;
	int done= 1;

	/* f2 flag is used for 'selection' */
	/* tmp.l is offset on scratch array   */
	for(eve= em->verts.first; eve; eve= eve->next) {
		if(eve->h==0) {
			eve->tmp.fp = 0;

			if(eve->f & SELECT) {
				eve->f2= 2;
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
				done |= connectivity_edge(mtx, eed->v1, eed->v2);
			}
		}

		/* do internal edges for quads */
		for(efa= em->faces.first; efa; efa= efa->next) {
			if (efa->v4) {
				done |= connectivity_edge(mtx, efa->v1, efa->v3);
				done |= connectivity_edge(mtx, efa->v2, efa->v4);
			}
		}
	}
}

/* loop-in-a-loop I know, but we need it! (ton) */
static void get_face_center(float *cent, EditMesh *em, EditVert *eve)
{
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
static void VertsToTransData(TransInfo *t, TransData *td, EditMesh *em, EditVert *eve)
{
	td->flag = 0;
	//if(key)
	//	td->loc = key->co;
	//else
	td->loc = eve->co;

	VECCOPY(td->center, td->loc);
	if(t->around==V3D_LOCAL && (em->selectmode & SCE_SELECT_FACE))
		get_face_center(td->center, em, eve);
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
	td->val = NULL;
	td->extra = NULL;
	if (t->mode == TFM_BWEIGHT) {
		td->val = &(eve->bweight);
		td->ival = eve->bweight;
	}
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
static float *get_crazy_mapped_editverts(TransInfo *t)
{
	Mesh *me= t->obedit->data;
	DerivedMesh *dm;
	float *vertexcos;

	/* disable subsurf temporal, get mapped cos, and enable it */
	if(modifiers_disable_subsurf_temporary(t->obedit)) {
		/* need to make new derivemesh */
		makeDerivedMesh(t->scene, t->obedit, me->edit_mesh, CD_MASK_BAREMESH);
	}

	/* now get the cage */
	dm= editmesh_get_derived_cage(t->scene, t->obedit, me->edit_mesh, CD_MASK_BAREMESH);

	vertexcos= MEM_mallocN(3*sizeof(float)*me->edit_mesh->totvert, "vertexcos map");
	dm->foreachMappedVert(dm, make_vertexcos__mapFunc, vertexcos);

	dm->release(dm);

	/* set back the flag, no new cage needs to be built, transform does it */
	modifiers_disable_subsurf_temporary(t->obedit);

	return vertexcos;
}

#define TAN_MAKE_VEC(a, b, c)	a[0]= b[0] + 0.2f*(b[0]-c[0]); a[1]= b[1] + 0.2f*(b[1]-c[1]); a[2]= b[2] + 0.2f*(b[2]-c[2])
static void set_crazy_vertex_quat(float *quat, float *v1, float *v2, float *v3, float *def1, float *def2, float *def3)
{
	float vecu[3], vecv[3];
	float q1[4], q2[4];

	TAN_MAKE_VEC(vecu, v1, v2);
	TAN_MAKE_VEC(vecv, v1, v3);
	tri_to_quat( q1,v1, vecu, vecv);

	TAN_MAKE_VEC(vecu, def1, def2);
	TAN_MAKE_VEC(vecv, def1, def3);
	tri_to_quat( q2,def1, vecu, vecv);

	sub_qt_qtqt(quat, q2, q1);
}
#undef TAN_MAKE_VEC

static void set_crazyspace_quats(EditMesh *em, float *origcos, float *mappedcos, float *quats)
{
	EditVert *eve, *prev;
	EditFace *efa;
	float *v1, *v2, *v3, *v4, *co1, *co2, *co3, *co4;
	intptr_t index= 0;

	/* two abused locations in vertices */
	for(eve= em->verts.first; eve; eve= eve->next, index++) {
		eve->tmp.p = NULL;
		eve->prev= (EditVert *)index;
	}

	/* first store two sets of tangent vectors in vertices, we derive it just from the face-edges */
	for(efa= em->faces.first; efa; efa= efa->next) {

		/* retrieve mapped coordinates */
		v1= mappedcos + 3*(intptr_t)(efa->v1->prev);
		v2= mappedcos + 3*(intptr_t)(efa->v2->prev);
		v3= mappedcos + 3*(intptr_t)(efa->v3->prev);

		co1= (origcos)? origcos + 3*(intptr_t)(efa->v1->prev): efa->v1->co;
		co2= (origcos)? origcos + 3*(intptr_t)(efa->v2->prev): efa->v2->co;
		co3= (origcos)? origcos + 3*(intptr_t)(efa->v3->prev): efa->v3->co;

		if(efa->v2->tmp.p==NULL && efa->v2->f1) {
			set_crazy_vertex_quat(quats, co2, co3, co1, v2, v3, v1);
			efa->v2->tmp.p= (void*)quats;
			quats+= 4;
		}

		if(efa->v4) {
			v4= mappedcos + 3*(intptr_t)(efa->v4->prev);
			co4= (origcos)? origcos + 3*(intptr_t)(efa->v4->prev): efa->v4->co;

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

static void createTransEditVerts(bContext *C, TransInfo *t)
{
	ToolSettings *ts = CTX_data_tool_settings(C);
	TransData *tob = NULL;
	EditMesh *em = ((Mesh *)t->obedit->data)->edit_mesh;
	EditVert *eve;
	EditVert *eve_act = NULL;
	float *mappedcos = NULL, *quats= NULL;
	float mtx[3][3], smtx[3][3], (*defmats)[3][3] = NULL, (*defcos)[3] = NULL;
	int count=0, countsel=0, a, totleft;
	int propmode = t->flag & T_PROP_EDIT;
	int mirror = 0;
	short selectmode = ts->selectmode;

	if (t->flag & T_MIRROR)
	{
		mirror = 1;
	}

	/* edge slide forces edge select */
	if (t->mode == TFM_EDGE_SLIDE) {
		selectmode = SCE_SELECT_EDGE;
	}

	// transform now requires awareness for select mode, so we tag the f1 flags in verts
	if(selectmode & SCE_SELECT_VERTEX) {
		for(eve= em->verts.first; eve; eve= eve->next) {
			if(eve->h==0 && (eve->f & SELECT))
				eve->f1= SELECT;
			else
				eve->f1= 0;
		}
	}
	else if(selectmode & SCE_SELECT_EDGE) {
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
	if (em->selected.last) {
		EditSelection *ese = em->selected.last;
		if ( ese->type == EDITVERT ) {
			eve_act = (EditVert *)ese->data;
		}
	}


	if(propmode) t->total = count;
	else t->total = countsel;

	tob= t->data= MEM_callocN(t->total*sizeof(TransData), "TransObData(Mesh EditMode)");

	copy_m3_m4(mtx, t->obedit->obmat);
	invert_m3_m3(smtx, mtx);

	if(propmode) editmesh_set_connectivity_distance(em, mtx);

	/* detect CrazySpace [tm] */
	if(propmode==0) {
		if(modifiers_getCageIndex(t->scene, t->obedit, NULL, 1)>=0) {
			if(modifiers_isCorrectableDeformed(t->scene, t->obedit)) {
				/* check if we can use deform matrices for modifier from the
				   start up to stack, they are more accurate than quats */
				totleft= editmesh_get_first_deform_matrices(t->scene, t->obedit, em, &defmats, &defcos);

				/* if we still have more modifiers, also do crazyspace
				   correction with quats, relative to the coordinates after
				   the modifiers that support deform matrices (defcos) */
				if(totleft > 0) {
					mappedcos= get_crazy_mapped_editverts(t);
					quats= MEM_mallocN( (t->total)*sizeof(float)*4, "crazy quats");
					set_crazyspace_quats(em, (float*)defcos, mappedcos, quats);
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
				{
					t->mirror = -1;
					mirror = -1;
				}
				break;
			}
		}
	}

	for (a=0, eve=em->verts.first; eve; eve=eve->next, a++) {
		if(eve->h==0) {
			if(propmode || eve->f1) {
				VertsToTransData(t, tob, em, eve);

				/* selected */
				if(eve->f1) tob->flag |= TD_SELECTED;

				/* active */
				if(eve == eve_act) tob->flag |= TD_ACTIVE;

				if(propmode) {
					if (eve->f2) {
						tob->dist= eve->tmp.fp;
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
						quat_to_mat3( qmat,eve->tmp.p);

						if(defmats)
							mul_serie_m3(mat, mtx, qmat, defmats[a],
								NULL, NULL, NULL, NULL, NULL);
						else
							mul_m3_m3m3(mat, mtx, qmat);
					}
					else
						mul_m3_m3m3(mat, mtx, defmats[a]);

					invert_m3_m3(imat, mat);

					copy_m3_m3(tob->smtx, imat);
					copy_m3_m3(tob->mtx, mat);
				}
				else {
					copy_m3_m3(tob->smtx, smtx);
					copy_m3_m3(tob->mtx, mtx);
				}

				/* Mirror? */
				if( (mirror>0 && tob->iloc[0]>0.0f) || (mirror<0 && tob->iloc[0]<0.0f)) {
					EditVert *vmir= editmesh_get_x_mirror_vert(t->obedit, em, eve, tob->iloc, a);	/* initializes octree on first call */
					if(vmir != eve) {
						tob->extra = vmir;
					}
				}
				tob++;
			}
		}
	}
	
	if (mirror != 0)
	{
		tob = t->data;
		for( a = 0; a < t->total; a++, tob++ )
		{
			if (ABS(tob->loc[0]) <= 0.00001f)
			{
				tob->flag |= TD_MIRROR_EDGE;
			}
		}
	}
	
	/* crazy space free */
	if(quats)
		MEM_freeN(quats);
	if(defmats)
		MEM_freeN(defmats);
}

/* *** NODE EDITOR *** */
void flushTransNodes(TransInfo *t)
{
	int a;
	TransData2D *td;

	/* flush to 2d vector from internally used 3d vector */
	for(a=0, td= t->data2d; a<t->total; a++, td++) {
		td->loc2d[0]= td->loc[0];
		td->loc2d[1]= td->loc[1];
	}
}

/* *** SEQUENCE EDITOR *** */
#define XXX_DURIAN_ANIM_TX_HACK
void flushTransSeq(TransInfo *t)
{
	ListBase *seqbasep= seq_give_editing(t->scene, FALSE)->seqbasep; /* Editing null check alredy done */
	int a, new_frame;
	TransData *td= NULL;
	TransData2D *td2d= NULL;
	TransDataSeq *tdsq= NULL;
	Sequence *seq;



	/* prevent updating the same seq twice
	 * if the transdata order is changed this will mess up
	 * but so will TransDataSeq */
	Sequence *seq_prev= NULL;

	/* flush to 2d vector from internally used 3d vector */
	for(a=0, td= t->data, td2d= t->data2d; a<t->total; a++, td++, td2d++) {
		tdsq= (TransDataSeq *)td->extra;
		seq= tdsq->seq;
		new_frame= (int)floor(td2d->loc[0] + 0.5f);

		switch (tdsq->sel_flag) {
		case SELECT:
#ifdef XXX_DURIAN_ANIM_TX_HACK
			if (seq != seq_prev) {
				int ofs = (new_frame - tdsq->start_offset) - seq->start; // breaks for single strips - color/image
				seq_offset_animdata(t->scene, seq, ofs);
			}
#endif
			if (seq->type != SEQ_META && (seq->depth != 0 || seq_tx_test(seq))) /* for meta's, their children move */
				seq->start= new_frame - tdsq->start_offset;

			if (seq->depth==0) {
				seq->machine= (int)floor(td2d->loc[1] + 0.5f);
				CLAMP(seq->machine, 1, MAXSEQ);
			}
			break;
		case SEQ_LEFTSEL: /* no vertical transform  */
			seq_tx_set_final_left(seq, new_frame);
			seq_tx_handle_xlimits(seq, tdsq->flag&SEQ_LEFTSEL, tdsq->flag&SEQ_RIGHTSEL);
			seq_single_fix(seq); /* todo - move this into aftertrans update? - old seq tx needed it anyway */
			break;
		case SEQ_RIGHTSEL: /* no vertical transform  */
			seq_tx_set_final_right(seq, new_frame);
			seq_tx_handle_xlimits(seq, tdsq->flag&SEQ_LEFTSEL, tdsq->flag&SEQ_RIGHTSEL);
			seq_single_fix(seq); /* todo - move this into aftertrans update? - old seq tx needed it anyway */
			break;
		}

		if (seq != seq_prev) {
			if(seq->depth==0) {
				/* Calculate this strip and all nested strips
				 * children are ALWAYS transformed first
				 * so we dont need to do this in another loop. */
				calc_sequence(t->scene, seq);
			}
			else {
				calc_sequence_disp(t->scene, seq);
			}
		}
		seq_prev= seq;
	}

	/* need to do the overlap check in a new loop otherwise adjacent strips
	 * will not be updated and we'll get false positives */
	seq_prev= NULL;
	for(a=0, td= t->data, td2d= t->data2d; a<t->total; a++, td++, td2d++) {

		tdsq= (TransDataSeq *)td->extra;
		seq= tdsq->seq;

		if (seq != seq_prev) {
			if(seq->depth==0) {
				/* test overlap, displayes red outline */
				seq->flag &= ~SEQ_OVERLAP;
				if( seq_test_overlap(seqbasep, seq) ) {
					seq->flag |= SEQ_OVERLAP;
				}
			}
		}
		seq_prev= seq;
	}

	if (t->mode == TFM_TIME_TRANSLATE) { /* originally TFM_TIME_EXTEND, transform changes */
		/* Special annoying case here, need to calc metas with TFM_TIME_EXTEND only */
		seq= seqbasep->first;

		while(seq) {
			if (seq->type == SEQ_META && seq->flag & SELECT)
				calc_sequence(t->scene, seq);
			seq= seq->next;
		}
	}
}

/* ********************* UV ****************** */

static void UVsToTransData(SpaceImage *sima, TransData *td, TransData2D *td2d, float *uv, int selected)
{
	float aspx, aspy;

	ED_space_image_uv_aspect(sima, &aspx, &aspy);

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

	td->ext= NULL; td->val= NULL;

	if(selected) {
		td->flag |= TD_SELECTED;
		td->dist= 0.0;
	}
	else {
		td->dist= MAXFLOAT;
	}
	unit_m3(td->mtx);
	unit_m3(td->smtx);
}

static void createTransUVs(bContext *C, TransInfo *t)
{
	SpaceImage *sima = CTX_wm_space_image(C);
	Image *ima = CTX_data_edit_image(C);
	Scene *scene = t->scene;
	TransData *td = NULL;
	TransData2D *td2d = NULL;
	MTFace *tf;
	int count=0, countsel=0;
	int propmode = t->flag & T_PROP_EDIT;

	EditMesh *em = ((Mesh *)t->obedit->data)->edit_mesh;
	EditFace *efa;

	if(!ED_uvedit_test(t->obedit)) return;

	/* count */
	for (efa= em->faces.first; efa; efa= efa->next) {
		tf= CustomData_em_get(&em->fdata, efa->data, CD_MTFACE);

		if(uvedit_face_visible(scene, ima, efa, tf)) {
			efa->tmp.p = tf;

			if (uvedit_uv_selected(scene, efa, tf, 0)) countsel++;
			if (uvedit_uv_selected(scene, efa, tf, 1)) countsel++;
			if (uvedit_uv_selected(scene, efa, tf, 2)) countsel++;
			if (efa->v4 && uvedit_uv_selected(scene, efa, tf, 3)) countsel++;
			if(propmode)
				count += (efa->v4)? 4: 3;
		} else {
			efa->tmp.p = NULL;
		}
	}

	 /* note: in prop mode we need at least 1 selected */
	if (countsel==0) return;

	t->total= (propmode)? count: countsel;
	t->data= MEM_callocN(t->total*sizeof(TransData), "TransObData(UV Editing)");
	/* for each 2d uv coord a 3d vector is allocated, so that they can be
	   treated just as if they were 3d verts */
	t->data2d= MEM_callocN(t->total*sizeof(TransData2D), "TransObData2D(UV Editing)");

	if(sima->flag & SI_CLIP_UV)
		t->flag |= T_CLIP_UV;

	td= t->data;
	td2d= t->data2d;

	for (efa= em->faces.first; efa; efa= efa->next) {
		if ((tf=(MTFace *)efa->tmp.p)) {
			if (propmode) {
				UVsToTransData(sima, td++, td2d++, tf->uv[0], uvedit_uv_selected(scene, efa, tf, 0));
				UVsToTransData(sima, td++, td2d++, tf->uv[1], uvedit_uv_selected(scene, efa, tf, 1));
				UVsToTransData(sima, td++, td2d++, tf->uv[2], uvedit_uv_selected(scene, efa, tf, 2));
				if(efa->v4)
					UVsToTransData(sima, td++, td2d++, tf->uv[3], uvedit_uv_selected(scene, efa, tf, 3));
			} else {
				if(uvedit_uv_selected(scene, efa, tf, 0))				UVsToTransData(sima, td++, td2d++, tf->uv[0], 1);
				if(uvedit_uv_selected(scene, efa, tf, 1))				UVsToTransData(sima, td++, td2d++, tf->uv[1], 1);
				if(uvedit_uv_selected(scene, efa, tf, 2))				UVsToTransData(sima, td++, td2d++, tf->uv[2], 1);
				if(efa->v4 && uvedit_uv_selected(scene, efa, tf, 3))	UVsToTransData(sima, td++, td2d++, tf->uv[3], 1);
			}
		}
	}

	if (sima->flag & SI_LIVE_UNWRAP)
		ED_uvedit_live_unwrap_begin(t->scene, t->obedit);
}

void flushTransUVs(TransInfo *t)
{
	SpaceImage *sima = t->sa->spacedata.first;
	TransData2D *td;
	int a, width, height;
	float aspx, aspy, invx, invy;

	ED_space_image_uv_aspect(sima, &aspx, &aspy);
	ED_space_image_size(sima, &width, &height);
	invx= 1.0f/aspx;
	invy= 1.0f/aspy;

	/* flush to 2d vector from internally used 3d vector */
	for(a=0, td= t->data2d; a<t->total; a++, td++) {
		td->loc2d[0]= td->loc[0]*invx;
		td->loc2d[1]= td->loc[1]*invy;

		if((sima->flag & SI_PIXELSNAP) && (t->state != TRANS_CANCEL)) {
			td->loc2d[0]= (float)floor(width*td->loc2d[0] + 0.5f)/width;
			td->loc2d[1]= (float)floor(height*td->loc2d[1] + 0.5f)/height;
		}
	}
}

int clipUVTransform(TransInfo *t, float *vec, int resize)
{
	TransData *td;
	int a, clipx=1, clipy=1;
	float aspx, aspy, min[2], max[2];

	ED_space_image_uv_aspect(t->sa->spacedata.first, &aspx, &aspy);
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

/* ********************* ANIMATION EDITORS (GENERAL) ************************* */

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

/* ********************* NLA EDITOR ************************* */

static void createTransNlaData(bContext *C, TransInfo *t)
{
	Scene *scene= t->scene;
	TransData *td = NULL;
	TransDataNla *tdn = NULL;
	
	bAnimContext ac;
	ListBase anim_data = {NULL, NULL};
	bAnimListElem *ale;
	int filter;
	
	int count=0;
	
	/* determine what type of data we are operating on */
	if (ANIM_animdata_get_context(C, &ac) == 0)
		return;
	
	/* filter data */
	filter= (ANIMFILTER_VISIBLE | ANIMFILTER_NLATRACKS | ANIMFILTER_FOREDIT);
	ANIM_animdata_filter(&ac, &anim_data, filter, ac.data, ac.datatype);
	
	/* which side of the current frame should be allowed */
	if (t->mode == TFM_TIME_EXTEND) {
		/* only side on which mouse is gets transformed */
		float xmouse, ymouse;
		
		UI_view2d_region_to_view(&ac.ar->v2d, t->imval[0], t->imval[1], &xmouse, &ymouse);
		t->frame_side= (xmouse > CFRA) ? 'R' : 'L';
	}
	else {
		/* normal transform - both sides of current frame are considered */
		t->frame_side = 'B';
	}
	
	/* loop 1: count how many strips are selected (consider each strip as 2 points) */
	for (ale= anim_data.first; ale; ale= ale->next) {
		NlaTrack *nlt= (NlaTrack *)ale->data;
		NlaStrip *strip;
		
		/* make some meta-strips for chains of selected strips */
		BKE_nlastrips_make_metas(&nlt->strips, 1);
		
		/* only consider selected strips */
		for (strip= nlt->strips.first; strip; strip= strip->next) {
			// TODO: we can make strips have handles later on...
			/* transition strips can't get directly transformed */
			if (strip->type != NLASTRIP_TYPE_TRANSITION) {
				if (strip->flag & NLASTRIP_FLAG_SELECT) {
					if (FrameOnMouseSide(t->frame_side, strip->start, (float)CFRA)) count++;
					if (FrameOnMouseSide(t->frame_side, strip->end, (float)CFRA)) count++;
				}
			}
		}
	}
	
	/* stop if trying to build list if nothing selected */
	if (count == 0) {
		/* cleanup temp list */
		BLI_freelistN(&anim_data);
		return;
	}
	
	/* allocate memory for data */
	t->total= count;
	
	t->data= MEM_callocN(t->total*sizeof(TransData), "TransData(NLA Editor)");
	td= t->data;
	t->customData= MEM_callocN(t->total*sizeof(TransDataNla), "TransDataNla (NLA Editor)");
	tdn= t->customData;
	
	/* loop 2: build transdata array */
	for (ale= anim_data.first; ale; ale= ale->next) {
		/* only if a real NLA-track */
		if (ale->type == ANIMTYPE_NLATRACK) {
			NlaTrack *nlt= (NlaTrack *)ale->data;
			NlaStrip *strip;
			
			/* only consider selected strips */
			for (strip= nlt->strips.first; strip; strip= strip->next) {
				// TODO: we can make strips have handles later on...
				/* transition strips can't get directly transformed */
				if (strip->type != NLASTRIP_TYPE_TRANSITION) {
					if (strip->flag & NLASTRIP_FLAG_SELECT) {
						/* our transform data is constructed as follows:
						 *	- only the handles on the right side of the current-frame get included
						 *	- td structs are transform-elements operated on by the transform system
						 *	  and represent a single handle. The storage/pointer used (val or loc) depends on
						 *	  whether we're scaling or transforming. Ultimately though, the handles
						 * 	  the td writes to will simply be a dummy in tdn
						 *	- for each strip being transformed, a single tdn struct is used, so in some
						 *	  cases, there will need to be 1 of these tdn elements in the array skipped...
						 */
						float center[3], yval;
						
						/* firstly, init tdn settings */
						tdn->id= ale->id;
						tdn->oldTrack= tdn->nlt= nlt;
						tdn->strip= strip;
						tdn->trackIndex= BLI_findindex(&nlt->strips, strip);
						
						yval= (float)(tdn->trackIndex * NLACHANNEL_STEP);
						
						tdn->h1[0]= strip->start;
						tdn->h1[1]= yval;
						tdn->h2[0]= strip->end;
						tdn->h2[1]= yval;
						
						center[0]= (float)CFRA;
						center[1]= yval;
						center[2]= 0.0f;
						
						/* set td's based on which handles are applicable */
						if (FrameOnMouseSide(t->frame_side, strip->start, (float)CFRA))
						{
							/* just set tdn to assume that it only has one handle for now */
							tdn->handle= -1;
							
							/* now, link the transform data up to this data */
							if (ELEM(t->mode, TFM_TRANSLATION, TFM_TIME_EXTEND)) {
								td->loc= tdn->h1;
								VECCOPY(td->iloc, tdn->h1);
								
								/* store all the other gunk that is required by transform */
								VECCOPY(td->center, center);
								memset(td->axismtx, 0, sizeof(td->axismtx));
								td->axismtx[2][2] = 1.0f;
								
								td->ext= NULL; td->val= NULL;
								
								td->flag |= TD_SELECTED;
								td->dist= 0.0f;
								
								unit_m3(td->mtx);
								unit_m3(td->smtx);
							}
							else {
								td->val= &tdn->h1[0];
								td->ival= tdn->h1[0];
							}
							
							td->extra= tdn;
							td++;
						}
						if (FrameOnMouseSide(t->frame_side, strip->end, (float)CFRA))
						{
							/* if tdn is already holding the start handle, then we're doing both, otherwise, only end */
							tdn->handle= (tdn->handle) ? 2 : 1;
							
							/* now, link the transform data up to this data */
							if (ELEM(t->mode, TFM_TRANSLATION, TFM_TIME_EXTEND)) {
								td->loc= tdn->h2;
								VECCOPY(td->iloc, tdn->h2);
								
								/* store all the other gunk that is required by transform */
								VECCOPY(td->center, center);
								memset(td->axismtx, 0, sizeof(td->axismtx));
								td->axismtx[2][2] = 1.0f;
								
								td->ext= NULL; td->val= NULL;
								
								td->flag |= TD_SELECTED;
								td->dist= 0.0f;
								
								unit_m3(td->mtx);
								unit_m3(td->smtx);
							}
							else {
								td->val= &tdn->h2[0];
								td->ival= tdn->h2[0];
							}
							
							td->extra= tdn;
							td++;
						}
						
						/* if both handles were used, skip the next tdn (i.e. leave it blank) since the counting code is dumb...
						 * otherwise, just advance to the next one...
						 */
						if (tdn->handle == 2)
							tdn += 2;
						else
							tdn++;
					}
				}
			}
		}
	}

	/* cleanup temp list */
	BLI_freelistN(&anim_data);
}

/* ********************* ACTION EDITOR ****************** */

/* Called by special_aftertrans_update to make sure selected gp-frames replace
 * any other gp-frames which may reside on that frame (that are not selected).
 * It also makes sure gp-frames are still stored in chronological order after
 * transform.
 */
#if 0
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
			short added= 0;
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
						added= 1;
						break;
					}
				}
				if (added == 0)
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
					// TRANSFORM_FIX_ME
					//gpencil_layer_delframe(gpl, gpf);
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
#endif

/* Called during special_aftertrans_update to make sure selected keyframes replace
 * any other keyframes which may reside on that frame (that is not selected).
 */
static void posttrans_fcurve_clean (FCurve *fcu)
{
	float *selcache;	/* cache for frame numbers of selected frames (fcu->totvert*sizeof(float)) */
	int len, index, i;	/* number of frames in cache, item index */

	/* allocate memory for the cache */
	// TODO: investigate using BezTriple columns instead?
	if (fcu->totvert == 0 || fcu->bezt==NULL)
		return;
	selcache= MEM_callocN(sizeof(float)*fcu->totvert, "FCurveSelFrameNums");
	len= 0;
	index= 0;

	/* We do 2 loops, 1 for marking keyframes for deletion, one for deleting
	 * as there is no guarantee what order the keyframes are exactly, even though
	 * they have been sorted by time.
	 */

	/*	Loop 1: find selected keyframes   */
	for (i = 0; i < fcu->totvert; i++) {
		BezTriple *bezt= &fcu->bezt[i];
		
		if (BEZSELECTED(bezt)) {
			selcache[index]= bezt->vec[1][0];
			index++;
			len++;
		}
	}

	/* Loop 2: delete unselected keyframes on the same frames 
	 * (if any keyframes were found, or the whole curve wasn't affected) 
	 */
	if ((len) && (len != fcu->totvert)) {
		for (i = 0; i < fcu->totvert; i++) {
			BezTriple *bezt= &fcu->bezt[i];
			
			if (BEZSELECTED(bezt) == 0) {
				/* check beztriple should be removed according to cache */
				for (index= 0; index < len; index++) {
					if (IS_EQ(bezt->vec[1][0], selcache[index])) {
						delete_fcurve_key(fcu, i, 0);
						break;
					}
					else if (bezt->vec[1][0] > selcache[index])
						break;
				}
			}
		}
		
		testhandles_fcurve(fcu);
	}

	/* free cache */
	MEM_freeN(selcache);
}



/* Called by special_aftertrans_update to make sure selected keyframes replace
 * any other keyframes which may reside on that frame (that is not selected).
 * remake_action_ipos should have already been called
 */
static void posttrans_action_clean (bAnimContext *ac, bAction *act)
{
	ListBase anim_data = {NULL, NULL};
	bAnimListElem *ale;
	int filter;

	/* filter data */
	filter= (ANIMFILTER_VISIBLE | ANIMFILTER_FOREDIT | ANIMFILTER_CURVESONLY);
	ANIM_animdata_filter(ac, &anim_data, filter, act, ANIMCONT_ACTION);

	/* loop through relevant data, removing keyframes as appropriate
	 *  	- all keyframes are converted in/out of global time
	 */
	for (ale= anim_data.first; ale; ale= ale->next) {
		AnimData *adt= ANIM_nla_mapping_get(ac, ale);

		if (adt) {
			ANIM_nla_mapping_apply_fcurve(adt, ale->key_data, 0, 1);
			posttrans_fcurve_clean(ale->key_data);
			ANIM_nla_mapping_apply_fcurve(adt, ale->key_data, 1, 1);
		}
		else
			posttrans_fcurve_clean(ale->key_data);
	}

	/* free temp data */
	BLI_freelistN(&anim_data);
}

/* ----------------------------- */

/* fully select selected beztriples, but only include if it's on the right side of cfra */
static int count_fcurve_keys(FCurve *fcu, char side, float cfra)
{
	BezTriple *bezt;
	int i, count = 0;

	if (ELEM(NULL, fcu, fcu->bezt))
		return count;

	/* only include points that occur on the right side of cfra */
	for (i=0, bezt=fcu->bezt; i < fcu->totvert; i++, bezt++) {
		if (bezt->f2 & SELECT) {
			/* fully select the other two keys */
			bezt->f1 |= SELECT;
			bezt->f3 |= SELECT;

			/* increment by 3, as there are 3 points (3 * x-coordinates) that need transform */
			if (FrameOnMouseSide(side, bezt->vec[1][0], cfra))
				count += 3;
		}
	}

	return count;
}

/* fully select selected beztriples, but only include if it's on the right side of cfra */
#if 0
static int count_gplayer_frames(bGPDlayer *gpl, char side, float cfra)
{
	bGPDframe *gpf;
	int count = 0;

	if (gpl == NULL)
		return count;

	/* only include points that occur on the right side of cfra */
	for (gpf= gpl->frames.first; gpf; gpf= gpf->next) {
		if (gpf->flag & GP_FRAME_SELECT) {
			if (FrameOnMouseSide(side, (float)gpf->framenum, cfra))
				count++;
		}
	}

	return count;
}
#endif

/* This function assigns the information to transdata */
static void TimeToTransData(TransData *td, float *time, AnimData *adt)
{
	/* memory is calloc'ed, so that should zero everything nicely for us */
	td->val = time;
	td->ival = *(time);

	/* store the AnimData where this keyframe exists as a keyframe of the
	 * active action as td->extra.
	 */
	td->extra= adt;
}

/* This function advances the address to which td points to, so it must return
 * the new address so that the next time new transform data is added, it doesn't
 * overwrite the existing ones...  i.e.   td = IcuToTransData(td, icu, ob, side, cfra);
 *
 * The 'side' argument is needed for the extend mode. 'B' = both sides, 'R'/'L' mean only data
 * on the named side are used.
 */
static TransData *FCurveToTransData(TransData *td, FCurve *fcu, AnimData *adt, char side, float cfra)
{
	BezTriple *bezt;
	int i;

	if (fcu == NULL)
		return td;

	for (i=0, bezt=fcu->bezt; i < fcu->totvert; i++, bezt++) {
		/* only add selected keyframes (for now, proportional edit is not enabled) */
		if (BEZSELECTED(bezt)) {
			/* only add if on the right 'side' of the current frame */
			if (FrameOnMouseSide(side, bezt->vec[1][0], cfra)) {
				/* each control point needs to be added separetely */
				TimeToTransData(td, bezt->vec[0], adt);
				td++;

				TimeToTransData(td, bezt->vec[1], adt);
				td++;

				TimeToTransData(td, bezt->vec[2], adt);
				td++;
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
#if 0
static int GPLayerToTransData (TransData *td, tGPFtransdata *tfd, bGPDlayer *gpl, char side, float cfra)
{
	bGPDframe *gpf;
	int count= 0;

	/* check for select frames on right side of current frame */
	for (gpf= gpl->frames.first; gpf; gpf= gpf->next) {
		if (gpf->flag & GP_FRAME_SELECT) {
			if (FrameOnMouseSide(side, (float)gpf->framenum, cfra)) {
				/* memory is calloc'ed, so that should zero everything nicely for us */
				td->val= &tfd->val;
				td->ival= (float)gpf->framenum;

				tfd->val= (float)gpf->framenum;
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
#endif

static void createTransActionData(bContext *C, TransInfo *t)
{
	Scene *scene= t->scene;
	TransData *td = NULL;
	tGPFtransdata *tfd = NULL;
	
	bAnimContext ac;
	ListBase anim_data = {NULL, NULL};
	bAnimListElem *ale;
	int filter;
	
	int count=0;
	float cfra;
	
	/* determine what type of data we are operating on */
	if (ANIM_animdata_get_context(C, &ac) == 0)
		return;
	
	/* filter data */
	if (ac.datatype == ANIMCONT_GPENCIL)
		filter= (ANIMFILTER_VISIBLE | ANIMFILTER_FOREDIT);
	else
		filter= (ANIMFILTER_VISIBLE | ANIMFILTER_FOREDIT | ANIMFILTER_CURVESONLY);
	ANIM_animdata_filter(&ac, &anim_data, filter, ac.data, ac.datatype);
	
	/* which side of the current frame should be allowed */
	if (t->mode == TFM_TIME_EXTEND) {
		/* only side on which mouse is gets transformed */
		float xmouse, ymouse;
		
		UI_view2d_region_to_view(&ac.ar->v2d, t->imval[0], t->imval[1], &xmouse, &ymouse);
		t->frame_side = (xmouse > CFRA) ? 'R' : 'L'; // XXX use t->frame_side
	}
	else {
		/* normal transform - both sides of current frame are considered */
		t->frame_side = 'B';
	}
	
	/* loop 1: fully select ipo-keys and count how many BezTriples are selected */
	for (ale= anim_data.first; ale; ale= ale->next) {
		AnimData *adt= ANIM_nla_mapping_get(&ac, ale);
		
		/* convert current-frame to action-time (slightly less accurate, espcially under
		 * higher scaling ratios, but is faster than converting all points)
		 */
		if (adt)
			cfra = BKE_nla_tweakedit_remap(adt, (float)CFRA, NLATIME_CONVERT_UNMAP);
		else
			cfra = (float)CFRA;
		
		//if (ale->type == ANIMTYPE_GPLAYER)
		//	count += count_gplayer_frames(ale->data, t->frame_side, cfra);
		//else
			count += count_fcurve_keys(ale->key_data, t->frame_side, cfra);
	}
	
	/* stop if trying to build list if nothing selected */
	if (count == 0) {
		/* cleanup temp list */
		BLI_freelistN(&anim_data);
		return;
	}
	
	/* allocate memory for data */
	t->total= count;
	
	t->data= MEM_callocN(t->total*sizeof(TransData), "TransData(Action Editor)");
	td= t->data;
	
	if (ac.datatype == ANIMCONT_GPENCIL) {
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
	for (ale= anim_data.first; ale; ale= ale->next) {
		//if (ale->type == ANIMTYPE_GPLAYER) {
		//	bGPDlayer *gpl= (bGPDlayer *)ale->data;
		//	int i;
		//
		//	i = GPLayerToTransData(td, tfd, gpl, t->frame_side, cfra);
		//	td += i;
		//	tfd += i;
		//}
		//else {
			AnimData *adt= ANIM_nla_mapping_get(&ac, ale);
			FCurve *fcu= (FCurve *)ale->key_data;
			
			/* convert current-frame to action-time (slightly less accurate, espcially under
			 * higher scaling ratios, but is faster than converting all points)
			 */
			if (adt)
				cfra = BKE_nla_tweakedit_remap(adt, (float)CFRA, NLATIME_CONVERT_UNMAP);
			else
				cfra = (float)CFRA;
			
			td= FCurveToTransData(td, fcu, adt, t->frame_side, cfra);
		//}
	}
	
	/* check if we're supposed to be setting minx/maxx for TimeSlide */
	if (t->mode == TFM_TIME_SLIDE) {
		float min=999999999.0f, max=-999999999.0f;
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
	BLI_freelistN(&anim_data);
}

/* ********************* GRAPH EDITOR ************************* */

/* Helper function for createTransGraphEditData, which is reponsible for associating
 * source data with transform data
 */
static void bezt_to_transdata (TransData *td, TransData2D *td2d, AnimData *adt, float *loc, float *cent, short selected, short ishandle, short intvals)
{
	/* New location from td gets dumped onto the old-location of td2d, which then
	 * gets copied to the actual data at td2d->loc2d (bezt->vec[n])
	 *
	 * Due to NLA mapping, we apply NLA mapping to some of the verts here,
	 * and then that mapping will be undone after transform is done.
	 */
	
	if (adt) {
		td2d->loc[0] = BKE_nla_tweakedit_remap(adt, loc[0], NLATIME_CONVERT_MAP);
		td2d->loc[1] = loc[1];
		td2d->loc[2] = 0.0f;
		td2d->loc2d = loc;
		
		td->loc = td2d->loc;
		td->center[0] = BKE_nla_tweakedit_remap(adt, cent[0], NLATIME_CONVERT_MAP);
		td->center[1] = cent[1];
		td->center[2] = 0.0f;
		
		VECCOPY(td->iloc, td->loc);
	}
	else {
		td2d->loc[0] = loc[0];
		td2d->loc[1] = loc[1];
		td2d->loc[2] = 0.0f;
		td2d->loc2d = loc;
		
		td->loc = td2d->loc;
		VECCOPY(td->center, cent);
		VECCOPY(td->iloc, td->loc);
	}
	
	memset(td->axismtx, 0, sizeof(td->axismtx));
	td->axismtx[2][2] = 1.0f;
	
	td->ext= NULL; td->val= NULL;
	
	/* store AnimData info in td->extra, for applying mapping when flushing */
	td->extra= adt;
	
	if (selected) {
		td->flag |= TD_SELECTED;
		td->dist= 0.0f;
	}
	else
		td->dist= MAXFLOAT;
	
	if (ishandle)
		td->flag |= TD_NOTIMESNAP;
	if (intvals)
		td->flag |= TD_INTVALUES;
	
	unit_m3(td->mtx);
	unit_m3(td->smtx);
}

static void createTransGraphEditData(bContext *C, TransInfo *t)
{
	SpaceIpo *sipo= CTX_wm_space_graph(C);
	Scene *scene= t->scene;
	ARegion *ar= t->ar;
	View2D *v2d= &ar->v2d;
	
	TransData *td = NULL;
	TransData2D *td2d = NULL;
	
	bAnimContext ac;
	ListBase anim_data = {NULL, NULL};
	bAnimListElem *ale;
	int filter;
	
	BezTriple *bezt;
	int count=0, i;
	float cfra;
	
	/* determine what type of data we are operating on */
	if (ANIM_animdata_get_context(C, &ac) == 0)
		return;
	
	/* filter data */
	filter= (ANIMFILTER_VISIBLE | ANIMFILTER_FOREDIT | ANIMFILTER_CURVESONLY | ANIMFILTER_CURVEVISIBLE);
	ANIM_animdata_filter(&ac, &anim_data, filter, ac.data, ac.datatype);
	
	/* which side of the current frame should be allowed */
		// XXX we still want this mode, but how to get this using standard transform too?
	if (t->mode == TFM_TIME_EXTEND) {
		/* only side on which mouse is gets transformed */
		float xmouse, ymouse;
		
		UI_view2d_region_to_view(v2d, t->imval[0], t->imval[1], &xmouse, &ymouse);
		t->frame_side = (xmouse > CFRA) ? 'R' : 'L'; // XXX use t->frame_side
	}
	else {
		/* normal transform - both sides of current frame are considered */
		t->frame_side = 'B';
	}
	
	/* loop 1: count how many BezTriples (specifically their verts) are selected (or should be edited) */
	for (ale= anim_data.first; ale; ale= ale->next) {
		AnimData *adt= ANIM_nla_mapping_get(&ac, ale);
		FCurve *fcu= (FCurve *)ale->key_data;
		
		/* convert current-frame to action-time (slightly less accurate, espcially under
		 * higher scaling ratios, but is faster than converting all points)
		 */
		if (adt)
			cfra = BKE_nla_tweakedit_remap(adt, (float)CFRA, NLATIME_CONVERT_UNMAP);
		else
			cfra = (float)CFRA;
		
		/* F-Curve may not have any keyframes */
		if (fcu->bezt == NULL)
			continue;
		
		/* only include BezTriples whose 'keyframe' occurs on the same side of the current frame as mouse */
		for (i=0, bezt=fcu->bezt; i < fcu->totvert; i++, bezt++) {
			if (FrameOnMouseSide(t->frame_side, bezt->vec[1][0], cfra)) {
				if (sipo->around == V3D_LOCAL) {
					/* for local-pivot we only need to count the number of selected handles only, so that centerpoints don't
					 * don't get moved wrong
					 */
					if (bezt->ipo == BEZT_IPO_BEZ) {
						if (bezt->f1 & SELECT) count++;
						if (bezt->f3 & SELECT) count++;
					}
					else if (bezt->f2 & SELECT) count++; // TODO: could this cause problems?
				}
				else {
					/* for 'normal' pivots - just include anything that is selected */
					if (bezt->f1 & SELECT) count++;
					if (bezt->f2 & SELECT) count++;
					if (bezt->f3 & SELECT) count++;
				}
			}
		}
	}
	
	/* stop if trying to build list if nothing selected */
	if (count == 0) {
		/* cleanup temp list */
		BLI_freelistN(&anim_data);
		return;
	}
	
	/* allocate memory for data */
	t->total= count;
	
	t->data= MEM_callocN(t->total*sizeof(TransData), "TransData (Graph Editor)");
		/* for each 2d vert a 3d vector is allocated, so that they can be treated just as if they were 3d verts */
	t->data2d= MEM_callocN(t->total*sizeof(TransData2D), "TransData2D (Graph Editor)");
	
	td= t->data;
	td2d= t->data2d;
	
	/* loop 2: build transdata arrays */
	for (ale= anim_data.first; ale; ale= ale->next) {
		AnimData *adt= ANIM_nla_mapping_get(&ac, ale);
		FCurve *fcu= (FCurve *)ale->key_data;
		short intvals= (fcu->flag & FCURVE_INT_VALUES);

		/* convert current-frame to action-time (slightly less accurate, espcially under
		 * higher scaling ratios, but is faster than converting all points)
		 */
		if (adt)
			cfra = BKE_nla_tweakedit_remap(adt, (float)CFRA, NLATIME_CONVERT_UNMAP);
		else
			cfra = (float)CFRA;
			
		/* F-Curve may not have any keyframes */
		if (fcu->bezt == NULL)
			continue;
		
		ANIM_unit_mapping_apply_fcurve(ac.scene, ale->id, ale->key_data, ANIM_UNITCONV_ONLYSEL|ANIM_UNITCONV_SELVERTS);
		
		/* only include BezTriples whose 'keyframe' occurs on the same side of the current frame as mouse (if applicable) */
		for (i=0, bezt= fcu->bezt; i < fcu->totvert; i++, bezt++) {
			if (FrameOnMouseSide(t->frame_side, bezt->vec[1][0], cfra)) {
				TransDataCurveHandleFlags *hdata = NULL;
				short h1=1, h2=1;
				
				/* only include handles if selected, irrespective of the interpolation modes */
				if (bezt->f1 & SELECT) {
					hdata = initTransDataCurveHandles(td, bezt);
					bezt_to_transdata(td++, td2d++, adt, bezt->vec[0], bezt->vec[1], 1, 1, intvals);
				}
				else
					h1= 0;
				if (bezt->f3 & SELECT) {
					if (hdata==NULL)
						hdata = initTransDataCurveHandles(td, bezt);
					bezt_to_transdata(td++, td2d++, adt, bezt->vec[2], bezt->vec[1], 1, 1, intvals);
				}
				else
					h2= 0;
				
				/* only include main vert if selected */
				if (bezt->f2 & SELECT) {
					/* if scaling around individuals centers, do not include keyframes */
					if (sipo->around != V3D_LOCAL) {
						/* if handles were not selected, store their selection status */
						if (!(bezt->f1 & SELECT) && !(bezt->f3 & SELECT)) {
							if (hdata == NULL)
								hdata = initTransDataCurveHandles(td, bezt);
						}
						
						bezt_to_transdata(td++, td2d++, adt, bezt->vec[1], bezt->vec[1], 1, 0, intvals);
					}
					
					/* special hack (must be done after initTransDataCurveHandles(), as that stores handle settings to restore...):
					 *	- Check if we've got entire BezTriple selected and we're scaling/rotating that point,
					 *	  then check if we're using auto-handles.
					 *	- If so, change them auto-handles to aligned handles so that handles get affected too
					 */
					if ((bezt->h1 == HD_AUTO) && (bezt->h2 == HD_AUTO) && ELEM(t->mode, TFM_ROTATION, TFM_RESIZE)) {
						if (h1 && h2) {
							bezt->h1= HD_ALIGN;
							bezt->h2= HD_ALIGN;
						}
					}
				}
			}
		}
		
		/* Sets handles based on the selection */
		testhandles_fcurve(fcu);
	}
	
	/* cleanup temp list */
	BLI_freelistN(&anim_data);
}


/* ------------------------ */

/* struct for use in re-sorting BezTriples during IPO transform */
typedef struct BeztMap {
	BezTriple *bezt;
	int oldIndex; 		/* index of bezt in icu->bezt array before sorting */
	int newIndex;		/* index of bezt in icu->bezt array after sorting */
	short swapHs; 		/* swap order of handles (-1=clear; 0=not checked, 1=swap) */
	char pipo, cipo;	/* interpolation of current and next segments */
} BeztMap;


/* This function converts an FCurve's BezTriple array to a BeztMap array
 * NOTE: this allocates memory that will need to get freed later
 */
static BeztMap *bezt_to_beztmaps (BezTriple *bezts, int totvert)
{
	BezTriple *bezt= bezts;
	BezTriple *prevbezt= NULL;
	BeztMap *bezm, *bezms;
	int i;
	
	/* allocate memory for this array */
	if (totvert==0 || bezts==NULL)
		return NULL;
	bezm= bezms= MEM_callocN(sizeof(BeztMap)*totvert, "BeztMaps");
	
	/* assign beztriples to beztmaps */
	for (i=0; i < totvert; i++, bezm++, prevbezt=bezt, bezt++) {
		bezm->bezt= bezt;
		
		bezm->oldIndex= i;
		bezm->newIndex= i;
		
		bezm->pipo= (prevbezt) ? prevbezt->ipo : bezt->ipo;
		bezm->cipo= bezt->ipo;
	}

	return bezms;
}

/* This function copies the code of sort_time_ipocurve, but acts on BeztMap structs instead */
static void sort_time_beztmaps (BeztMap *bezms, int totvert)
{
	BeztMap *bezm;
	int i, ok= 1;
	
	/* keep repeating the process until nothing is out of place anymore */
	while (ok) {
		ok= 0;
		
		bezm= bezms;
		i= totvert;
		while (i--) {
			/* is current bezm out of order (i.e. occurs later than next)? */
			if (i > 0) {
				if (bezm->bezt->vec[1][0] > (bezm+1)->bezt->vec[1][0]) {
					bezm->newIndex++;
					(bezm+1)->newIndex--;
					
					SWAP(BeztMap, *bezm, *(bezm+1));
					
					ok= 1;
				}
			}
			
			/* do we need to check if the handles need to be swapped?
			 * optimisation: this only needs to be performed in the first loop
			 */
			if (bezm->swapHs == 0) {
				if ( (bezm->bezt->vec[0][0] > bezm->bezt->vec[1][0]) &&
					 (bezm->bezt->vec[2][0] < bezm->bezt->vec[1][0]) )
				{
					/* handles need to be swapped */
					bezm->swapHs = 1;
				}
				else {
					/* handles need to be cleared */
					bezm->swapHs = -1;
				}
			}
			
			bezm++;
		}
	}
}

/* This function firstly adjusts the pointers that the transdata has to each BezTriple */
static void beztmap_to_data (TransInfo *t, FCurve *fcu, BeztMap *bezms, int totvert)
{
	BezTriple *bezts = fcu->bezt;
	BeztMap *bezm;
	TransData2D *td;
	int i, j;
	char *adjusted;
	
	/* dynamically allocate an array of chars to mark whether an TransData's
	 * pointers have been fixed already, so that we don't override ones that are
	 * already done
	  */
	adjusted= MEM_callocN(t->total, "beztmap_adjusted_map");
	
	/* for each beztmap item, find if it is used anywhere */
	bezm= bezms;
	for (i= 0; i < totvert; i++, bezm++) {
		/* loop through transdata, testing if we have a hit
		 * for the handles (vec[0]/vec[2]), we must also check if they need to be swapped...
		 */
		td= t->data2d;
		for (j= 0; j < t->total; j++, td++) {
			/* skip item if already marked */
			if (adjusted[j] != 0) continue;
			
			/* only selected verts */
			if (bezm->pipo == BEZT_IPO_BEZ) {
				if (bezm->bezt->f1 & SELECT) {
					if (td->loc2d == bezm->bezt->vec[0]) {
						if (bezm->swapHs == 1)
							td->loc2d= (bezts + bezm->newIndex)->vec[2];
						else
							td->loc2d= (bezts + bezm->newIndex)->vec[0];
						adjusted[j] = 1;
					}
				}
			}
			if (bezm->cipo == BEZT_IPO_BEZ) {
				if (bezm->bezt->f3 & SELECT) {
					if (td->loc2d == bezm->bezt->vec[2]) {
						if (bezm->swapHs == 1)
							td->loc2d= (bezts + bezm->newIndex)->vec[0];
						else
							td->loc2d= (bezts + bezm->newIndex)->vec[2];
						adjusted[j] = 1;
					}
				}
			}
			if (bezm->bezt->f2 & SELECT) {
				if (td->loc2d == bezm->bezt->vec[1]) {
					td->loc2d= (bezts + bezm->newIndex)->vec[1];
					adjusted[j] = 1;
				}
			}
		}
		
	}
	
	/* free temp memory used for 'adjusted' array */
	MEM_freeN(adjusted);
}

/* This function is called by recalcData during the Transform loop to recalculate
 * the handles of curves and sort the keyframes so that the curves draw correctly.
 * It is only called if some keyframes have moved out of order.
 *
 * anim_data is the list of channels (F-Curves) retrieved already containing the
 * channels to work on. It should not be freed here as it may still need to be used.
 */
void remake_graph_transdata (TransInfo *t, ListBase *anim_data)
{
	bAnimListElem *ale;
	
	/* sort and reassign verts */
	for (ale= anim_data->first; ale; ale= ale->next) {
		FCurve *fcu= (FCurve *)ale->key_data;
		
		if (fcu->bezt) {
			BeztMap *bezm;
			
			/* adjust transform-data pointers */
			bezm= bezt_to_beztmaps(fcu->bezt, fcu->totvert);
			sort_time_beztmaps(bezm, fcu->totvert);
			beztmap_to_data(t, fcu, bezm, fcu->totvert);
			
			/* free mapping stuff */
			MEM_freeN(bezm);
			
			/* re-sort actual beztriples (perhaps this could be done using the beztmaps to save time?) */
			sort_time_fcurve(fcu);
			
			/* make sure handles are all set correctly */
			testhandles_fcurve(fcu);
		}
	}
}

/* this function is called on recalcData to apply the transforms applied
 * to the transdata on to the actual keyframe data
 */
void flushTransGraphData(TransInfo *t)
{
	SpaceIpo *sipo = (SpaceIpo *)t->sa->spacedata.first;
	TransData *td;
	TransData2D *td2d;
	Scene *scene= t->scene;
	double secf= FPS;
	int a;

	/* flush to 2d vector from internally used 3d vector */
	for (a=0, td= t->data, td2d=t->data2d; a<t->total; a++, td++, td2d++) {
		AnimData *adt= (AnimData *)td->extra; /* pointers to relevant AnimData blocks are stored in the td->extra pointers */
		
		/* handle snapping for time values
		 *	- we should still be in NLA-mapping timespace
		 *	- only apply to keyframes (but never to handles)
		 */
		if ((td->flag & TD_NOTIMESNAP)==0) {
			switch (sipo->autosnap) {
				case SACTSNAP_FRAME: /* snap to nearest frame (or second if drawing seconds) */
					if (sipo->flag & SIPO_DRAWTIME)
						td2d->loc[0]= (float)( floor((td2d->loc[0]/secf) + 0.5f) * secf );
					else
						td2d->loc[0]= (float)( floor(td2d->loc[0]+0.5f) );
					break;
				
				case SACTSNAP_MARKER: /* snap to nearest marker */
					td2d->loc[0]= (float)ED_markers_find_nearest_marker_time(&t->scene->markers, td2d->loc[0]);
					break;
			}
		}
		
		/* we need to unapply the nla-mapping from the time in some situations */
		if (adt)
			td2d->loc2d[0]= BKE_nla_tweakedit_remap(adt, td2d->loc[0], NLATIME_CONVERT_UNMAP);
		else
			td2d->loc2d[0]= td2d->loc[0];
		
		/* if int-values only, truncate to integers */
		if (td->flag & TD_INTVALUES)
			td2d->loc2d[1]= (float)((int)td2d->loc[1]);
		else
			td2d->loc2d[1]= td2d->loc[1];
	}
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


/* This function applies the rules for transforming a strip so duplicate
 * checks dont need to be added in multiple places.
 *
 * recursive, count and flag MUST be set.
 *
 * seq->depth must be set before running this function so we know if the strips
 * are root level or not
 */
static void SeqTransInfo(TransInfo *t, Sequence *seq, int *recursive, int *count, int *flag)
{
 
#ifdef XXX_DURIAN_ANIM_TX_HACK
	/* hack */
	if((seq->flag & SELECT)==0 && seq->type & SEQ_EFFECT) {
		Sequence *seq_t[3] = {seq->seq1, seq->seq2, seq->seq3};
		int i;
		for(i=0; i<3; i++) {
			if (seq_t[i] && ((seq_t[i])->flag & SELECT) && !(seq_t[i]->flag & SEQ_LOCK) && !(seq_t[i]->flag & (SEQ_LEFTSEL|SEQ_RIGHTSEL)))
				seq->flag |= SELECT;
		}
	}
#endif
    
	/* for extend we need to do some tricks */
	if (t->mode == TFM_TIME_EXTEND) {

		/* *** Extend Transform *** */

		Scene * scene= t->scene;
		int cfra= CFRA;
		int left= seq_tx_get_final_left(seq, 0);
		int right= seq_tx_get_final_right(seq, 0);

		if (seq->depth == 0 && ((seq->flag & SELECT) == 0 || (seq->flag & SEQ_LOCK))) {
			*recursive= 0;
			*count= 0;
			*flag= 0;
		}
		else if (seq->type ==SEQ_META) {

			/* for meta's we only ever need to extend their children, no matter what depth
			 * just check the meta's are in the bounds */
			if (t->frame_side=='R' && right <= cfra)		*recursive= 0;
			else if (t->frame_side=='L' && left >= cfra)	*recursive= 0;
			else											*recursive= 1;

			*count= 0;
			*flag= 0;
		}
		else {

			*recursive= 0;	/* not a meta, so no thinking here */
			*count= 1;		/* unless its set to 0, extend will never set 2 handles at once */
			*flag= (seq->flag | SELECT) & ~(SEQ_LEFTSEL|SEQ_RIGHTSEL);

			if (t->frame_side=='R') {
				if (right <= cfra)		*count= *flag= 0;	/* ignore */
				else if (left > cfra)	;	/* keep the selection */
				else					*flag |= SEQ_RIGHTSEL;
			}
			else {
				if (left >= cfra)		*count= *flag= 0;	/* ignore */
				else if (right < cfra)	;	/* keep the selection */
				else					*flag |= SEQ_LEFTSEL;
			}
		}
	} else {

		t->frame_side= 'B';

		/* *** Normal Transform *** */

		if (seq->depth == 0) {

			/* Count */

			/* Non nested strips (resect selection and handles) */
			if ((seq->flag & SELECT) == 0 || (seq->flag & SEQ_LOCK)) {
				*recursive= 0;
				*count= 0;
				*flag= 0;
			}
			else {
				if ((seq->flag & (SEQ_LEFTSEL|SEQ_RIGHTSEL)) == (SEQ_LEFTSEL|SEQ_RIGHTSEL)) {
					*flag= seq->flag;
					*count= 2; /* we need 2 transdata's */
				} else {
					*flag= seq->flag;
					*count= 1; /* selected or with a handle selected */
				}

				/* Recursive */

				if ((seq->type == SEQ_META) && ((seq->flag & (SEQ_LEFTSEL|SEQ_RIGHTSEL)) == 0)) {
					/* if any handles are selected, dont recurse */
					*recursive = 1;
				}
				else {
					*recursive = 0;
				}
			}
		}
		else {
			/* Nested, different rules apply */

			if (seq->type == SEQ_META) {
				/* Meta's can only directly be moved between channels since they
				 * dont have their start and length set directly (children affect that)
				 * since this Meta is nested we dont need any of its data infact.
				 * calc_sequence() will update its settings when run on the toplevel meta */
				*flag= 0;
				*count= 0;
				*recursive = 1;
			}
			else {
				*flag= (seq->flag | SELECT) & ~(SEQ_LEFTSEL|SEQ_RIGHTSEL);
				*count= 1; /* ignore the selection for nested */
				*recursive = 0;
			}
		}
	}
}



static int SeqTransCount(TransInfo *t, ListBase *seqbase, int depth)
{
	Sequence *seq;
	int tot= 0, recursive, count, flag;

	for (seq= seqbase->first; seq; seq= seq->next) {
		seq->depth= depth;

		SeqTransInfo(t, seq, &recursive, &count, &flag); /* ignore the flag */
		tot += count;

		if (recursive) {
			tot += SeqTransCount(t, &seq->seqbase, depth+1);
		}
	}

	return tot;
}


static TransData *SeqToTransData(TransInfo *t, TransData *td, TransData2D *td2d, TransDataSeq *tdsq, Sequence *seq, int flag, int sel_flag)
{
	int start_left;

	switch(sel_flag) {
	case SELECT:
		/* Use seq_tx_get_final_left() and an offset here
		 * so transform has the left hand location of the strip.
		 * tdsq->start_offset is used when flushing the tx data back */
		start_left= seq_tx_get_final_left(seq, 0);
		td2d->loc[0]= start_left;
		tdsq->start_offset= start_left - seq->start; /* use to apply the original location */
		break;
	case SEQ_LEFTSEL:
		start_left= seq_tx_get_final_left(seq, 0);
		td2d->loc[0] = start_left;
		break;
	case SEQ_RIGHTSEL:
		td2d->loc[0] = seq_tx_get_final_right(seq, 0);
		break;
	}

	td2d->loc[1] = seq->machine; /* channel - Y location */
	td2d->loc[2] = 0.0f;
	td2d->loc2d = NULL;


	tdsq->seq= seq;

	/* Use instead of seq->flag for nested strips and other
	 * cases where the selection may need to be modified */
	tdsq->flag= flag;
	tdsq->sel_flag= sel_flag;


	td->extra= (void *)tdsq; /* allow us to update the strip from here */

	td->flag = 0;
	td->loc = td2d->loc;
	VECCOPY(td->center, td->loc);
	VECCOPY(td->iloc, td->loc);

	memset(td->axismtx, 0, sizeof(td->axismtx));
	td->axismtx[2][2] = 1.0f;

	td->ext= NULL; td->val= NULL;

	td->flag |= TD_SELECTED;
	td->dist= 0.0;

	unit_m3(td->mtx);
	unit_m3(td->smtx);

	/* Time Transform (extend) */
	td->val= td2d->loc;
	td->ival= td2d->loc[0];

	return td;
}

static int SeqToTransData_Recursive(TransInfo *t, ListBase *seqbase, TransData *td, TransData2D *td2d, TransDataSeq *tdsq)
{
	Sequence *seq;
	int recursive, count, flag;
	int tot= 0;

	for (seq= seqbase->first; seq; seq= seq->next) {

		SeqTransInfo(t, seq, &recursive, &count, &flag);

		/* add children first so recalculating metastrips does nested strips first */
		if (recursive) {
			int tot_children= SeqToTransData_Recursive(t, &seq->seqbase, td, td2d, tdsq);

			td=		td +	tot_children;
			td2d=	td2d +	tot_children;
			tdsq=	tdsq +	tot_children;

			tot += tot_children;
		}

		/* use 'flag' which is derived from seq->flag but modified for special cases */
		if (flag & SELECT) {
			if (flag & (SEQ_LEFTSEL|SEQ_RIGHTSEL)) {
				if (flag & SEQ_LEFTSEL) {
					SeqToTransData(t, td++, td2d++, tdsq++, seq, flag, SEQ_LEFTSEL);
					tot++;
				}
				if (flag & SEQ_RIGHTSEL) {
					SeqToTransData(t, td++, td2d++, tdsq++, seq, flag, SEQ_RIGHTSEL);
					tot++;
				}
			}
			else {
				SeqToTransData(t, td++, td2d++, tdsq++, seq, flag, SELECT);
				tot++;
			}
		}
	}

	return tot;
}

static void freeSeqData(TransInfo *t)
{
	Editing *ed= seq_give_editing(t->scene, FALSE);

	if(ed != NULL) {
		ListBase *seqbasep= ed->seqbasep;
		TransData *td= t->data;
		int a;

		/* prevent updating the same seq twice
		 * if the transdata order is changed this will mess up
		 * but so will TransDataSeq */
		Sequence *seq_prev= NULL;
		Sequence *seq;


		if (!(t->state == TRANS_CANCEL)) {

#if 0		// default 2.4 behavior

			/* flush to 2d vector from internally used 3d vector */
			for(a=0; a<t->total; a++, td++) {
				if ((seq != seq_prev) && (seq->depth==0) && (seq->flag & SEQ_OVERLAP)) {
				seq= ((TransDataSeq *)td->extra)->seq;
					shuffle_seq(seqbasep, seq);
				}

				seq_prev= seq;
			}

#else		// durian hack
			{
				int overlap= 0;

				for(a=0; a<t->total; a++, td++) {
					seq_prev= NULL;
					seq= ((TransDataSeq *)td->extra)->seq;
					if ((seq != seq_prev) && (seq->depth==0) && (seq->flag & SEQ_OVERLAP)) {
						overlap= 1;
						break;
					}
					seq_prev= seq;
				}

				if(overlap) {
					for(seq= seqbasep->first; seq; seq= seq->next)
						seq->tmp= NULL;

					td= t->data;
					seq_prev= NULL;
					for(a=0; a<t->total; a++, td++) {
						seq= ((TransDataSeq *)td->extra)->seq;
						if ((seq != seq_prev)) {
							/* Tag seq with a non zero value, used by shuffle_seq_time to identify the ones to shuffle */
							seq->tmp= (void*)1;
						}
					}

					shuffle_seq_time(seqbasep, t->scene);
				}
			}
#endif

			for(seq= seqbasep->first; seq; seq= seq->next) {
				/* We might want to build a list of effects that need to be updated during transform */
				if(seq->type & SEQ_EFFECT) {
					if		(seq->seq1 && seq->seq1->flag & SELECT) calc_sequence(t->scene, seq);
					else if	(seq->seq2 && seq->seq2->flag & SELECT) calc_sequence(t->scene, seq);
					else if	(seq->seq3 && seq->seq3->flag & SELECT) calc_sequence(t->scene, seq);
				}
			}

			sort_seq(t->scene);
		}
		else {
			/* Cancelled, need to update the strips display */
			for(a=0; a<t->total; a++, td++) {
				seq= ((TransDataSeq *)td->extra)->seq;
				if ((seq != seq_prev) && (seq->depth==0)) {
					calc_sequence_disp(t->scene, seq);
				}
				seq_prev= seq;
			}
		}
	}

	if (t->customData) {
		MEM_freeN(t->customData);
		t->customData= NULL;
	}
	if (t->data) {
		MEM_freeN(t->data); // XXX postTrans usually does this
		t->data= NULL;
	}
}

static void createTransSeqData(bContext *C, TransInfo *t)
{

	View2D *v2d= UI_view2d_fromcontext(C);
	Scene *scene= t->scene;
	Editing *ed= seq_give_editing(t->scene, FALSE);
	TransData *td = NULL;
	TransData2D *td2d= NULL;
	TransDataSeq *tdsq= NULL;

	int count=0;

	if (ed==NULL) {
		t->total= 0;
		return;
	}

	t->customFree= freeSeqData;

	/* which side of the current frame should be allowed */
	if (t->mode == TFM_TIME_EXTEND) {
		/* only side on which mouse is gets transformed */
		float xmouse, ymouse;

		UI_view2d_region_to_view(v2d, t->imval[0], t->imval[1], &xmouse, &ymouse);
		t->frame_side = (xmouse > CFRA) ? 'R' : 'L';
	}
	else {
		/* normal transform - both sides of current frame are considered */
		t->frame_side = 'B';
	}


	count = SeqTransCount(t, ed->seqbasep, 0);

	/* allocate memory for data */
	t->total= count;

	/* stop if trying to build list if nothing selected */
	if (count == 0) {
		return;
	}

	td = t->data = MEM_callocN(t->total*sizeof(TransData), "TransSeq TransData");
	td2d = t->data2d = MEM_callocN(t->total*sizeof(TransData2D), "TransSeq TransData2D");
	tdsq = t->customData= MEM_callocN(t->total*sizeof(TransDataSeq), "TransSeq TransDataSeq");



	/* loop 2: build transdata array */
	SeqToTransData_Recursive(t, ed->seqbasep, td, td2d, tdsq);
}


/* transcribe given object into TransData for Transforming */
static void ObjectToTransData(bContext *C, TransInfo *t, TransData *td, Object *ob)
{
	Scene *scene = t->scene;
	float obmtx[3][3];
	short constinv;
	short skip_invert = 0;

	/* axismtx has the real orientation */
	copy_m3_m4(td->axismtx, ob->obmat);
	normalize_m3(td->axismtx);

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

	if (skip_invert == 0 && constinv == 0) {
		if (constinv == 0)
			ob->transflag |= OB_NO_CONSTRAINTS; /* where_is_object_time checks this */
		
		where_is_object(t->scene, ob);
		
		if (constinv == 0)
			ob->transflag &= ~OB_NO_CONSTRAINTS;
	}
	else
		where_is_object(t->scene, ob);

	td->ob = ob;

	td->loc = ob->loc;
	VECCOPY(td->iloc, td->loc);
	
	if (ob->rotmode > 0) {
		td->ext->rot= ob->rot;
		td->ext->rotAxis= NULL;
		td->ext->rotAngle= NULL;
		td->ext->quat= NULL;
		
		VECCOPY(td->ext->irot, ob->rot);
		VECCOPY(td->ext->drot, ob->drot);
	}
	else if (ob->rotmode == ROT_MODE_AXISANGLE) {
		td->ext->rot= NULL;
		td->ext->rotAxis= ob->rotAxis;
		td->ext->rotAngle= &ob->rotAngle;
		td->ext->quat= NULL;
		
		td->ext->irotAngle= ob->rotAngle;
		VECCOPY(td->ext->irotAxis, ob->rotAxis);
		td->ext->drotAngle= ob->drotAngle;
		VECCOPY(td->ext->drotAxis, ob->drotAxis);
	}
	else {
		td->ext->rot= NULL;
		td->ext->rotAxis= NULL;
		td->ext->rotAngle= NULL;
		td->ext->quat= ob->quat;
		
		QUATCOPY(td->ext->iquat, ob->quat);
		QUATCOPY(td->ext->dquat, ob->dquat);
	}
	td->rotOrder=ob->rotmode;

	td->ext->size = ob->size;
	VECCOPY(td->ext->isize, ob->size);
	VECCOPY(td->ext->dsize, ob->dsize);

	VECCOPY(td->center, ob->obmat[3]);

	copy_m4_m4(td->ext->obmat, ob->obmat);

	/* is there a need to set the global<->data space conversion matrices? */
	if (ob->parent || constinv) {
		float totmat[3][3], obinv[3][3];

		/* Get the effect of parenting, and/or certain constraints.
		 * NOTE: some Constraints, and also Tracking should never get this
		 *		done, as it doesn't work well.
		 */
		object_to_mat3(ob, obmtx);
		copy_m3_m4(totmat, ob->obmat);
		invert_m3_m3(obinv, totmat);
		mul_m3_m3m3(td->smtx, obmtx, obinv);
		invert_m3_m3(td->mtx, td->smtx);
	}
	else {
		/* no conversion to/from dataspace */
		unit_m3(td->smtx);
		unit_m3(td->mtx);
	}

	/* set active flag */
	if (ob == OBACT)
	{
		td->flag |= TD_ACTIVE;
	}
}


/* sets flags in Bases to define whether they take part in transform */
/* it deselects Bases, so we have to call the clear function always after */
static void set_trans_object_base_flags(bContext *C, TransInfo *t)
{
	Scene *scene = t->scene;
	View3D *v3d = t->view;

	/*
	 if Base selected and has parent selected:
	 base->flag= BA_WAS_SEL
	 */
	Base *base;

	/* don't do it if we're not actually going to recalculate anything */
	if(t->mode == TFM_DUMMY)
		return;

	/* makes sure base flags and object flags are identical */
	copy_baseflags(t->scene);

	/* handle pending update events, otherwise they got copied below */
	for (base= scene->base.first; base; base= base->next) {
		if(base->object->recalc)
			object_handle_update(t->scene, base->object);
	}

	for (base= scene->base.first; base; base= base->next) {
		base->flag &= ~BA_WAS_SEL;

		if(TESTBASELIB_BGMODE(v3d, scene, base)) {
			Object *ob= base->object;
			Object *parsel= ob->parent;

			/* if parent selected, deselect */
			while(parsel) {
				if(parsel->flag & SELECT) break;
				parsel= parsel->parent;
			}

			if(parsel)
			{
				/* rotation around local centers are allowed to propagate */
				if ((t->mode == TFM_ROTATION || t->mode == TFM_TRACKBALL)  && t->around == V3D_LOCAL) {
					base->flag |= BA_TRANSFORM_CHILD;
				} else {
					base->flag &= ~SELECT;
					base->flag |= BA_WAS_SEL;
				}
			}
			/* used for flush, depgraph will change recalcs if needed :) */
			ob->recalc |= OB_RECALC_OB;
		}
	}

	/* all recalc flags get flushed to all layers, so a layer flip later on works fine */
	DAG_scene_flush_update(t->scene, -1, 0);

	/* and we store them temporal in base (only used for transform code) */
	/* this because after doing updates, the object->recalc is cleared */
	for (base= scene->base.first; base; base= base->next) {
		if(base->object->recalc & OB_RECALC_OB)
			base->flag |= BA_HAS_RECALC_OB;
		if(base->object->recalc & OB_RECALC_DATA)
			base->flag |= BA_HAS_RECALC_DATA;
	}
}

static int mark_children(Object *ob)
{
	if (ob->flag & (SELECT|BA_TRANSFORM_CHILD))
		return 1;

	if (ob->parent)
	{
		if (mark_children(ob->parent))
		{
			ob->flag |= BA_TRANSFORM_CHILD;
			return 1;
		}
	}
	
	return 0;
}

static int count_proportional_objects(TransInfo *t)
{
	int total = 0;
	Scene *scene = t->scene;
	View3D *v3d = t->view;
	Base *base;

	/* rotations around local centers are allowed to propagate, so we take all objects */
	if (!((t->mode == TFM_ROTATION || t->mode == TFM_TRACKBALL)  && t->around == V3D_LOCAL))
	{
		/* mark all parents */
		for (base= scene->base.first; base; base= base->next) {
			if(TESTBASELIB_BGMODE(v3d, scene, base)) {
				Object *parent = base->object->parent;
	
				/* flag all parents */
				while(parent) {
					parent->flag |= BA_TRANSFORM_PARENT;
					parent = parent->parent;
				}
			}
		}

		/* mark all children */
		for (base= scene->base.first; base; base= base->next) {
			/* all base not already selected or marked that is editable */
			if ((base->object->flag & (SELECT|BA_TRANSFORM_CHILD|BA_TRANSFORM_PARENT)) == 0 && BASE_EDITABLE_BGMODE(v3d, scene, base))
			{
				mark_children(base->object);
			}
		}
	}
	
	for (base= scene->base.first; base; base= base->next) {
		Object *ob= base->object;

		/* if base is not selected, not a parent of selection or not a child of selection and it is editable */
		if ((ob->flag & (SELECT|BA_TRANSFORM_CHILD|BA_TRANSFORM_PARENT)) == 0 && BASE_EDITABLE_BGMODE(v3d, scene, base))
		{

			/* used for flush, depgraph will change recalcs if needed :) */
			ob->recalc |= OB_RECALC_OB;

			total += 1;
		}
	}
	

	/* all recalc flags get flushed to all layers, so a layer flip later on works fine */
	DAG_scene_flush_update(t->scene, -1, 0);

	/* and we store them temporal in base (only used for transform code) */
	/* this because after doing updates, the object->recalc is cleared */
	for (base= scene->base.first; base; base= base->next) {
		if(base->object->recalc & OB_RECALC_OB)
			base->flag |= BA_HAS_RECALC_OB;
		if(base->object->recalc & OB_RECALC_DATA)
			base->flag |= BA_HAS_RECALC_DATA;
	}

	return total;
}

static void clear_trans_object_base_flags(TransInfo *t)
{
	Scene *sce = t->scene;
	Base *base;

	for (base= sce->base.first; base; base = base->next)
	{
		if(base->flag & BA_WAS_SEL)
			base->flag |= SELECT;

		base->flag &= ~(BA_WAS_SEL|BA_HAS_RECALC_OB|BA_HAS_RECALC_DATA|BA_DO_IPO|BA_TRANSFORM_CHILD|BA_TRANSFORM_PARENT);
	}
}

/* auto-keyframing feature - for objects
 * 	tmode: should be a transform mode
 */
// NOTE: context may not always be available, so must check before using it as it's a luxury for a few cases
void autokeyframe_ob_cb_func(bContext *C, Scene *scene, View3D *v3d, Object *ob, int tmode)
{
	ID *id= &ob->id;
	FCurve *fcu;
	
	// TODO: this should probably be done per channel instead...
	if (autokeyframe_cfra_can_key(scene, id)) {
		KeyingSet *active_ks = ANIM_scene_get_active_keyingset(scene);
		ListBase dsources = {NULL, NULL};
		float cfra= (float)CFRA; // xxx this will do for now
		short flag = 0;
		
		/* get flags used for inserting keyframes */
		flag = ANIM_get_keyframing_flags(scene, 1);
		
		/* add datasource override for the camera object */
		ANIM_relative_keyingset_add_source(&dsources, id, NULL, NULL); 
		
		if (IS_AUTOKEY_FLAG(ONLYKEYINGSET) && (active_ks)) {
			/* only insert into active keyingset 
			 * NOTE: we assume here that the active Keying Set does not need to have its iterator overridden spe
			 */
			ANIM_apply_keyingset(C, NULL, NULL, active_ks, MODIFYKEY_MODE_INSERT, cfra);
		}
		else if (IS_AUTOKEY_FLAG(INSERTAVAIL)) {
			AnimData *adt= ob->adt;
			
			/* only key on available channels */
			if (adt && adt->action) {
				for (fcu= adt->action->curves.first; fcu; fcu= fcu->next) {
					fcu->flag &= ~FCURVE_SELECTED;
					insert_keyframe(id, adt->action, ((fcu->grp)?(fcu->grp->name):(NULL)), fcu->rna_path, fcu->array_index, cfra, flag);
				}
			}
		}
		else if (IS_AUTOKEY_FLAG(INSERTNEEDED)) {
			short doLoc=0, doRot=0, doScale=0;
			
			/* filter the conditions when this happens (assume that curarea->spacetype==SPACE_VIE3D) */
			if (tmode == TFM_TRANSLATION) {
				doLoc = 1;
			}
			else if (tmode == TFM_ROTATION) {
				if (v3d->around == V3D_ACTIVE) {
					if (ob != OBACT)
						doLoc = 1;
				}
				else if (v3d->around == V3D_CURSOR)
					doLoc = 1;
				
				if ((v3d->flag & V3D_ALIGN)==0)
					doRot = 1;
			}
			else if (tmode == TFM_RESIZE) {
				if (v3d->around == V3D_ACTIVE) {
					if (ob != OBACT)
						doLoc = 1;
				}
				else if (v3d->around == V3D_CURSOR)
					doLoc = 1;
				
				if ((v3d->flag & V3D_ALIGN)==0)
					doScale = 1;
			}
			
			/* insert keyframes for the affected sets of channels using the builtin KeyingSets found */
			if (doLoc) {
				KeyingSet *ks= ANIM_builtin_keyingset_get_named(NULL, "Location");
				ANIM_apply_keyingset(C, &dsources, NULL, ks, MODIFYKEY_MODE_INSERT, cfra);
			}
			if (doRot) {
				KeyingSet *ks= ANIM_builtin_keyingset_get_named(NULL, "Rotation");
				ANIM_apply_keyingset(C, &dsources, NULL, ks, MODIFYKEY_MODE_INSERT, cfra);
			}
			if (doScale) {
				KeyingSet *ks= ANIM_builtin_keyingset_get_named(NULL, "Scale");
				ANIM_apply_keyingset(C, &dsources, NULL, ks, MODIFYKEY_MODE_INSERT, cfra);
			}
		}
		/* insert keyframe in all (transform) channels */
		else {
			KeyingSet *ks= ANIM_builtin_keyingset_get_named(NULL, "LocRotScale");
			ANIM_apply_keyingset(C, &dsources, NULL, ks, MODIFYKEY_MODE_INSERT, cfra);
		}
		
		/* free temp info */
		BLI_freelistN(&dsources);
	}
}

/* auto-keyframing feature - for poses/pose-channels
 * 	tmode: should be a transform mode
 *	targetless_ik: has targetless ik been done on any channels?
 */
// NOTE: context may not always be available, so must check before using it as it's a luxury for a few cases
void autokeyframe_pose_cb_func(bContext *C, Scene *scene, View3D *v3d, Object *ob, int tmode, short targetless_ik)
{
	ID *id= &ob->id;
	AnimData *adt= ob->adt;
	bAction	*act= (adt) ? adt->action : NULL;
	bPose	*pose= ob->pose;
	bPoseChannel *pchan;
	FCurve *fcu;
	
	// TODO: this should probably be done per channel instead...
	if (autokeyframe_cfra_can_key(scene, id)) {
		KeyingSet *active_ks = ANIM_scene_get_active_keyingset(scene);
		float cfra= (float)CFRA;
		short flag= 0;
		
		/* flag is initialised from UserPref keyframing settings
		 *	- special exception for targetless IK - INSERTKEY_MATRIX keyframes should get
		 * 	  visual keyframes even if flag not set, as it's not that useful otherwise
		 *	  (for quick animation recording)
		 */
		flag = ANIM_get_keyframing_flags(scene, 1);
		
		if (targetless_ik) 
			flag |= INSERTKEY_MATRIX;
		
		for (pchan=pose->chanbase.first; pchan; pchan=pchan->next) {
			if (pchan->bone->flag & BONE_TRANSFORM) {
				ListBase dsources = {NULL, NULL};
				
				/* clear any 'unkeyed' flag it may have */
				pchan->bone->flag &= ~BONE_UNKEYED;
				
				/* add datasource override for the camera object */
				ANIM_relative_keyingset_add_source(&dsources, id, &RNA_PoseBone, pchan); 
				
				/* only insert into active keyingset? */
				// TODO: move this first case out of the loop
				if (IS_AUTOKEY_FLAG(ONLYKEYINGSET) && (active_ks)) {
					ANIM_apply_keyingset(C, NULL, NULL, active_ks, MODIFYKEY_MODE_INSERT, cfra);
				}
				/* only insert into available channels? */
				else if (IS_AUTOKEY_FLAG(INSERTAVAIL)) {
					if (act) {
						for (fcu= act->curves.first; fcu; fcu= fcu->next) {
							/* only insert keyframes for this F-Curve if it affects the current bone */
							if (strstr(fcu->rna_path, "bones")) {
								char *pchanName= BLI_getQuotedStr(fcu->rna_path, "bones[");
								
								/* only if bone name matches too... 
								 * NOTE: this will do constraints too, but those are ok to do here too?
								 */
								if (pchanName && strcmp(pchanName, pchan->name) == 0) 
									insert_keyframe(id, act, ((fcu->grp)?(fcu->grp->name):(NULL)), fcu->rna_path, fcu->array_index, cfra, flag);
									
								if (pchanName) MEM_freeN(pchanName);
							}
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
						if (ELEM(v3d->around, V3D_CURSOR, V3D_ACTIVE))
							doLoc = 1;
							
						if ((v3d->flag & V3D_ALIGN)==0)
							doRot = 1;
					}
					else if (tmode == TFM_RESIZE) {
						if (ELEM(v3d->around, V3D_CURSOR, V3D_ACTIVE))
							doLoc = 1;
							
						if ((v3d->flag & V3D_ALIGN)==0)
							doScale = 1;
					}
					
					if (doLoc) {
						KeyingSet *ks= ANIM_builtin_keyingset_get_named(NULL, "Location");
						ANIM_apply_keyingset(C, &dsources, NULL, ks, MODIFYKEY_MODE_INSERT, cfra);
					}
					if (doRot) {
						KeyingSet *ks= ANIM_builtin_keyingset_get_named(NULL, "Rotation");
						ANIM_apply_keyingset(C, &dsources, NULL, ks, MODIFYKEY_MODE_INSERT, cfra);
					}
					if (doScale) {
						KeyingSet *ks= ANIM_builtin_keyingset_get_named(NULL, "Scale");
						ANIM_apply_keyingset(C, &dsources, NULL, ks, MODIFYKEY_MODE_INSERT, cfra);
					}
				}
				/* insert keyframe in all (transform) channels */
				else {
					KeyingSet *ks= ANIM_builtin_keyingset_get_named(NULL, "LocRotScale");
					ANIM_apply_keyingset(C, &dsources, NULL, ks, MODIFYKEY_MODE_INSERT, cfra);
				}
				
				/* free temp info */
				BLI_freelistN(&dsources);
			}
		}
		
		/* do the bone paths 
		 * 	- only do this when there is context info, since we need that to resolve
		 *	  how to do the updates and so on...
		 *	- do not calculate unless there are paths already to update...
		 */
		if (C && (ob->pose->avs.path_bakeflag & MOTIONPATH_BAKE_HAS_PATHS)) {
			//ED_pose_clear_paths(C, ob); // XXX for now, don't need to clear
			ED_pose_recalculate_paths(C, scene, ob);
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


/* inserting keys, pointcache, redraw events... */
/* 
 * note: sequencer freeing has its own function now because of a conflict with transform's order of freeing (campbell)
 * 		 Order changed, the sequencer stuff should go back in here
 * */
void special_aftertrans_update(bContext *C, TransInfo *t)
{
	Object *ob;
//	short redrawipo=0, resetslowpar=1;
	int cancelled= (t->state == TRANS_CANCEL);
	short duplicate= (t->undostr && strstr(t->undostr, "Duplicate")) ? 1 : 0; /* see bugreport #21229 for reasons for this data */
	
	/* early out when nothing happened */
	if (t->total == 0 || t->mode == TFM_DUMMY)
		return;
	
	if (t->spacetype==SPACE_VIEW3D) {
		if (t->obedit) {
			if (cancelled==0) {
				EM_automerge(t->scene, t->obedit, 1);
			}
		}
	}
	
	if (t->spacetype == SPACE_SEQ) {
		/* freeSeqData in transform_conversions.c does this
		 * keep here so the else at the end wont run... */

		SpaceSeq *sseq= (SpaceSeq *)t->sa->spacedata.first;

		/* marker transform, not especially nice but we may want to move markers
		 * at the same time as keyframes in the dope sheet. */
		if ((sseq->flag & SEQ_MARKER_TRANS) && (cancelled == 0)) {
			/* cant use , TFM_TIME_EXTEND
			 * for some reason EXTEND is changed into TRANSLATE, so use frame_side instead */

			if(t->mode == TFM_SEQ_SLIDE) {
				if(t->frame_side == 'B')
					scene_marker_tfm_translate(t->scene, floor(t->values[0] + 0.5f), SELECT);
			}
			else if (ELEM(t->frame_side, 'L', 'R')) {
				scene_marker_tfm_extend(t->scene, floor(t->vec[0] + 0.5f), SELECT, t->scene->r.cfra, t->frame_side);
			}
		}

	}
	else if (t->spacetype == SPACE_NODE) {
		/* pass */
	}
	else if (t->spacetype == SPACE_ACTION) {
		SpaceAction *saction= (SpaceAction *)t->sa->spacedata.first;
		bAnimContext ac;
		
		/* initialise relevant anim-context 'context' data */
		if (ANIM_animdata_get_context(C, &ac) == 0)
			return;
			
		ob = ac.obact;
		
		if (ELEM(ac.datatype, ANIMCONT_DOPESHEET, ANIMCONT_SHAPEKEY)) {
			ListBase anim_data = {NULL, NULL};
			bAnimListElem *ale;
			short filter= (ANIMFILTER_VISIBLE | ANIMFILTER_FOREDIT | ANIMFILTER_CURVESONLY);
			
			/* get channels to work on */
			ANIM_animdata_filter(&ac, &anim_data, filter, ac.data, ac.datatype);
			
			/* these should all be F-Curves */
			for (ale= anim_data.first; ale; ale= ale->next) {
				AnimData *adt= ANIM_nla_mapping_get(&ac, ale);
				FCurve *fcu= (FCurve *)ale->key_data;
				
				if ( (saction->flag & SACTION_NOTRANSKEYCULL)==0 &&
					 ((cancelled == 0) || (duplicate)) )
				{
					if (adt) {
						ANIM_nla_mapping_apply_fcurve(adt, fcu, 0, 1);
						posttrans_fcurve_clean(fcu);
						ANIM_nla_mapping_apply_fcurve(adt, fcu, 1, 1);
					}
					else
						posttrans_fcurve_clean(fcu);
				}
			}
			
			/* free temp memory */
			BLI_freelistN(&anim_data);
		}
		else if (ac.datatype == ANIMCONT_ACTION) { // TODO: just integrate into the above...
			/* Depending on the lock status, draw necessary views */
			// fixme... some of this stuff is not good
			if (ob) {
				if (ob->pose || ob_get_key(ob))
					DAG_id_flush_update(&ob->id, OB_RECALC);
				else
					DAG_id_flush_update(&ob->id, OB_RECALC_OB);
			}
			
			/* Do curve cleanups? */
			if ( (saction->flag & SACTION_NOTRANSKEYCULL)==0 &&
				 ((cancelled == 0) || (duplicate)) )
			{
				posttrans_action_clean(&ac, (bAction *)ac.data);
			}
		}

		/* marker transform, not especially nice but we may want to move markers
		 * at the same time as keyframes in the dope sheet. */
		if ((saction->flag & SACTION_MARKERS_MOVE) && (cancelled == 0)) {
			/* cant use , TFM_TIME_EXTEND
			 * for some reason EXTEND is changed into TRANSLATE, so use frame_side instead */

			if(t->mode == TFM_TIME_TRANSLATE) {
				if(t->frame_side == 'B')
					scene_marker_tfm_translate(t->scene, floor(t->vec[0] + 0.5f), SELECT);
				else if (ELEM(t->frame_side, 'L', 'R'))
					scene_marker_tfm_extend(t->scene, floor(t->vec[0] + 0.5f), SELECT, t->scene->r.cfra, t->frame_side);
			}
		}

#if 0 // XXX future of this is still not clear
		else if (ac.datatype == ANIMCONT_GPENCIL) {
			/* remove duplicate frames and also make sure points are in order! */
			if ((cancelled == 0) || (duplicate))
			{
				bScreen *sc= (bScreen *)ac.data;
				ScrArea *sa;
				
				/* BAD... we need to loop over all screen areas for current screen...
				 * 	- sync this with actdata_filter_gpencil() in editaction.c
				 */
				for (sa= sc->areabase.first; sa; sa= sa->next) {
					bGPdata *gpd= gpencil_data_get_active(sa);
					
					if (gpd)
						posttrans_gpd_clean(gpd);
				}
			}
		}
#endif // XXX future of this is still not clear
		
		/* make sure all F-Curves are set correctly */
		ANIM_editkeyframes_refresh(&ac);
		
		/* clear flag that was set for time-slide drawing */
		saction->flag &= ~SACTION_MOVING;
	}
	else if (t->spacetype == SPACE_IPO) {
		SpaceIpo *sipo= (SpaceIpo *)t->sa->spacedata.first;
		bAnimContext ac;
		
		/* initialise relevant anim-context 'context' data */
		if (ANIM_animdata_get_context(C, &ac) == 0)
			return;
		
		if (ac.datatype)
		{
			ListBase anim_data = {NULL, NULL};
			bAnimListElem *ale;
			short filter= (ANIMFILTER_VISIBLE | ANIMFILTER_FOREDIT | ANIMFILTER_CURVESONLY | ANIMFILTER_CURVEVISIBLE);
			
			/* get channels to work on */
			ANIM_animdata_filter(&ac, &anim_data, filter, ac.data, ac.datatype);
			
			for (ale= anim_data.first; ale; ale= ale->next) {
				AnimData *adt= ANIM_nla_mapping_get(&ac, ale);
				FCurve *fcu= (FCurve *)ale->key_data;
				
				if ( (sipo->flag & SIPO_NOTRANSKEYCULL)==0 &&
					 ((cancelled == 0) || (duplicate)) )
				{
					if (adt) {
						ANIM_nla_mapping_apply_fcurve(adt, fcu, 0, 1);
						posttrans_fcurve_clean(fcu);
						ANIM_nla_mapping_apply_fcurve(adt, fcu, 1, 1);
					}
					else
						posttrans_fcurve_clean(fcu);
				}
			}
			
			/* free temp memory */
			BLI_freelistN(&anim_data);
		}
		
		/* make sure all F-Curves are set correctly */
		ANIM_editkeyframes_refresh(&ac);
	}
	else if (t->spacetype == SPACE_NLA) {
		bAnimContext ac;
		
		/* initialise relevant anim-context 'context' data */
		if (ANIM_animdata_get_context(C, &ac) == 0)
			return;
			
		if (ac.datatype)
		{
			ListBase anim_data = {NULL, NULL};
			bAnimListElem *ale;
			short filter= (ANIMFILTER_VISIBLE | ANIMFILTER_FOREDIT | ANIMFILTER_NLATRACKS);
			
			/* get channels to work on */
			ANIM_animdata_filter(&ac, &anim_data, filter, ac.data, ac.datatype);
			
			for (ale= anim_data.first; ale; ale= ale->next) {
				NlaTrack *nlt= (NlaTrack *)ale->data;
				
				/* make sure strips are in order again */
				BKE_nlatrack_sort_strips(nlt);
				
				/* remove the temp metas */
				BKE_nlastrips_clear_metas(&nlt->strips, 0, 1);
			}
			
			/* free temp memory */
			BLI_freelistN(&anim_data);
			
			/* perform after-transfrom validation */
			ED_nla_postop_refresh(&ac);
		}
	}
	else if (t->obedit) {
		if (t->obedit->type == OB_MESH)
		{
			EditMesh *em = ((Mesh *)t->obedit->data)->edit_mesh;
			/* table needs to be created for each edit command, since vertices can move etc */
			mesh_octree_table(t->obedit, em, NULL, 'e');
		}
	}
	else if ((t->flag & T_POSE) && (t->poseobj)) {
		bArmature *arm;
		bPose	*pose;
		bPoseChannel *pchan;
		short targetless_ik= 0;

		ob= t->poseobj;
		arm= ob->data;
		pose= ob->pose;

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
			autokeyframe_pose_cb_func(C, t->scene, (View3D *)t->view, ob, t->mode, targetless_ik);
			DAG_id_flush_update(&ob->id, OB_RECALC_DATA);
		}
		else if (arm->flag & ARM_DELAYDEFORM) {
			/* old optimize trick... this enforces to bypass the depgraph */
			DAG_id_flush_update(&ob->id, OB_RECALC_DATA);
			ob->recalc= 0;	// is set on OK position already by recalcData()
		}
		else
			DAG_id_flush_update(&ob->id, OB_RECALC_DATA);

	}
	else if(t->scene->basact && (ob = t->scene->basact->object) && (ob->mode & OB_MODE_PARTICLE_EDIT) && PE_get_current(t->scene, ob)) {
		;
	}
	else { /* Objects */
		int i, recalcObPaths=0;

		for (i = 0; i < t->total; i++) {
			TransData *td = t->data + i;
			Object *ob = td->ob;
			ListBase pidlist;
			PTCacheID *pid;
			
			if (td->flag & TD_NOACTION)
				break;
			
			if (td->flag & TD_SKIP)
				continue;

			/* flag object caches as outdated */
			BKE_ptcache_ids_from_object(&pidlist, ob);
			for(pid=pidlist.first; pid; pid=pid->next) {
				if(pid->type != PTCACHE_TYPE_PARTICLES) /* particles don't need reset on geometry change */
					pid->cache->flag |= PTCACHE_OUTDATED;
			}
			BLI_freelistN(&pidlist);

			/* pointcache refresh */
			if (BKE_ptcache_object_reset(t->scene, ob, PTCACHE_RESET_OUTDATED))
				ob->recalc |= OB_RECALC_DATA;

			/* Needed for proper updating of "quick cached" dynamics. */
			/* Creates troubles for moving animated objects without */
			/* autokey though, probably needed is an anim sys override? */
			/* Please remove if some other solution is found. -jahka */
			DAG_id_flush_update(&ob->id, OB_RECALC_OB);

			/* Set autokey if necessary */
			if (!cancelled) {
				autokeyframe_ob_cb_func(C, t->scene, (View3D *)t->view, ob, t->mode);
				
				/* only calculate paths if there are paths to be recalculated */
				if (ob->avs.path_bakeflag & MOTIONPATH_BAKE_HAS_PATHS)
					recalcObPaths= 1;
			}
		}
		
		/* recalculate motion paths for objects (if necessary) 
		 * NOTE: only do this when there is context info
		 */
		if (C && recalcObPaths) {
			//ED_objects_clear_paths(C); // XXX for now, don't need to clear
			ED_objects_recalculate_paths(C, t->scene);
		}
	}

	clear_trans_object_base_flags(t);

	if(t->spacetype == SPACE_VIEW3D)
	{
		View3D *v3d = t->view;

		/* restore manipulator */
		if (t->flag & T_MODAL) {
			v3d->twtype = t->twtype;
		}
	}


#if 0 // TRANSFORM_FIX_ME
	if(resetslowpar)
		reset_slowparents();

	/* note; should actually only be done for all objects when a lamp is moved... (ton) */
	if(t->spacetype==SPACE_VIEW3D && G.vd->drawtype == OB_SHADED)
		reshadeall_displist();
#endif
}

static void createTransObject(bContext *C, TransInfo *t)
{
	TransData *td = NULL;
	TransDataExtension *tx;
	int propmode = t->flag & T_PROP_EDIT;

	set_trans_object_base_flags(C, t);

	/* count */
	t->total= CTX_DATA_COUNT(C, selected_objects);
	
	if(!t->total) {
		/* clear here, main transform function escapes too */
		clear_trans_object_base_flags(t);
		return;
	}
	
	if (propmode)
	{
		t->total += count_proportional_objects(t);
	}

	td = t->data = MEM_callocN(t->total*sizeof(TransData), "TransOb");
	tx = t->ext = MEM_callocN(t->total*sizeof(TransDataExtension), "TransObExtension");

	CTX_DATA_BEGIN(C, Base*, base, selected_bases)
	{
		Object *ob= base->object;
		
		td->flag = TD_SELECTED;
		td->protectflag= ob->protectflag;
		td->ext = tx;
		td->rotOrder= ob->rotmode;
		
		if (base->flag & BA_TRANSFORM_CHILD)
		{
			td->flag |= TD_NOCENTER;
			td->flag |= TD_NO_LOC;
		}
		
		/* select linked objects, but skip them later */
		if (ob->id.lib != 0) {
			td->flag |= TD_SKIP;
		}
		
		ObjectToTransData(C, t, td, ob);
		td->val = NULL;
		td++;
		tx++;
	}
	CTX_DATA_END;
	
	if (propmode)
	{
		Scene *scene = t->scene;
		View3D *v3d = t->view;
		Base *base;

		for (base= scene->base.first; base; base= base->next) {
			Object *ob= base->object;

			/* if base is not selected, not a parent of selection or not a child of selection and it is editable */
			if ((ob->flag & (SELECT|BA_TRANSFORM_CHILD|BA_TRANSFORM_PARENT)) == 0 && BASE_EDITABLE_BGMODE(v3d, scene, base))
			{
				td->protectflag= ob->protectflag;
				td->ext = tx;
				td->rotOrder= ob->rotmode;
				
				ObjectToTransData(C, t, td, ob);
				td->val = NULL;
				td++;
				tx++;
			}
		}
	}
}

/* transcribe given node into TransData2D for Transforming */
static void NodeToTransData(TransData *td, TransData2D *td2d, bNode *node)
// static void NodeToTransData(bContext *C, TransInfo *t, TransData2D *td, bNode *node)
{
	td2d->loc[0] = node->locx; /* hold original location */
	td2d->loc[1] = node->locy;
	td2d->loc[2] = 0.0f;
	td2d->loc2d = &node->locx; /* current location */

	td->flag = 0;
	td->loc = td2d->loc;
	VECCOPY(td->center, td->loc);
	VECCOPY(td->iloc, td->loc);

	memset(td->axismtx, 0, sizeof(td->axismtx));
	td->axismtx[2][2] = 1.0f;

	td->ext= NULL; td->val= NULL;

	td->flag |= TD_SELECTED;
	td->dist= 0.0;

	unit_m3(td->mtx);
	unit_m3(td->smtx);
}

void createTransNodeData(bContext *C, TransInfo *t)
{
	TransData *td;
	TransData2D *td2d;

	t->total= CTX_DATA_COUNT(C, selected_nodes);

	td = t->data = MEM_callocN(t->total*sizeof(TransData), "TransNode TransData");
	td2d = t->data2d = MEM_callocN(t->total*sizeof(TransData2D), "TransNode TransData2D");

	CTX_DATA_BEGIN(C, bNode *, selnode, selected_nodes)
		NodeToTransData(td++, td2d++, selnode);
	CTX_DATA_END
}

void createTransData(bContext *C, TransInfo *t)
{
	Scene *scene = t->scene;
	Object *ob = OBACT;

	if (t->options & CTX_TEXTURE) {
		t->flag |= T_TEXTURE;
		createTransTexspace(C, t);
	}
	else if (t->options & CTX_EDGE) {
		t->ext = NULL;
		t->flag |= T_EDIT;
		createTransEdge(C, t);
		if(t->data && t->flag & T_PROP_EDIT) {
			sort_trans_data(t);	// makes selected become first in array
			set_prop_dist(t, 1);
			sort_trans_data_dist(t);
		}
	}
	else if (t->options == CTX_BMESH) {
		// TRANSFORM_FIX_ME
		//createTransBMeshVerts(t, G.editBMesh->bm, G.editBMesh->td);
	}
	else if (t->spacetype == SPACE_IMAGE) {
		t->flag |= T_POINTS|T_2D_EDIT;
		createTransUVs(C, t);
		if(t->data && (t->flag & T_PROP_EDIT)) {
			sort_trans_data(t);	// makes selected become first in array
			set_prop_dist(t, 1);
			sort_trans_data_dist(t);
		}
	}
	else if (t->spacetype == SPACE_ACTION) {
		t->flag |= T_POINTS|T_2D_EDIT;
		createTransActionData(C, t);
	}
	else if (t->spacetype == SPACE_NLA) {
		t->flag |= T_POINTS|T_2D_EDIT;
		createTransNlaData(C, t);
	}
	else if (t->spacetype == SPACE_SEQ) {
		t->flag |= T_POINTS|T_2D_EDIT;
		t->num.flag |= NUM_NO_FRACTION; /* sequencer has no use for floating point transformations */
		createTransSeqData(C, t);
	}
	else if (t->spacetype == SPACE_IPO) {
		t->flag |= T_POINTS|T_2D_EDIT;
		createTransGraphEditData(C, t);
#if 0
		if (t->data && (t->flag & T_PROP_EDIT)) {
			sort_trans_data(t);	// makes selected become first in array
			set_prop_dist(t, 1);
			sort_trans_data_dist(t);
		}
#endif
	}
	else if(t->spacetype == SPACE_NODE) {
		t->flag |= T_2D_EDIT|T_POINTS;
		createTransNodeData(C, t);
		if (t->data && (t->flag & T_PROP_EDIT)) {
			sort_trans_data(t);	// makes selected become first in array
			set_prop_dist(t, 1);
			sort_trans_data_dist(t);
		}
	}
	else if (t->obedit) {
		t->ext = NULL;
		if (t->obedit->type == OB_MESH) {
			createTransEditVerts(C, t);
		   }
		else if ELEM(t->obedit->type, OB_CURVE, OB_SURF) {
			createTransCurveVerts(C, t);
		}
		else if (t->obedit->type==OB_LATTICE) {
			createTransLatticeVerts(C, t);
		}
		else if (t->obedit->type==OB_MBALL) {
			createTransMBallVerts(C, t);
		}
		else if (t->obedit->type==OB_ARMATURE) {
			t->flag &= ~T_PROP_EDIT;
			createTransArmatureVerts(C, t);
		  }
		else {
			printf("edit type not implemented!\n");
		}

		t->flag |= T_EDIT|T_POINTS;

		if(t->data && t->flag & T_PROP_EDIT) {
			if (ELEM(t->obedit->type, OB_CURVE, OB_MESH)) {
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

		/* exception... hackish, we want bonesize to use bone orientation matrix (ton) */
		if(t->mode==TFM_BONESIZE) {
			t->flag &= ~(T_EDIT|T_POINTS);
			t->flag |= T_POSE;
			t->poseobj = ob;	/* <- tsk tsk, this is going to give issues one day */
		}
	}
	else if (ob && (ob->mode & OB_MODE_POSE)) {
		// XXX this is currently limited to active armature only...
		// XXX active-layer checking isn't done as that should probably be checked through context instead
		createTransPose(C, t, ob);
	}
	else if (ob && (ob->mode & OB_MODE_WEIGHT_PAINT)) {
		/* exception, we look for the one selected armature */
		CTX_DATA_BEGIN(C, Object*, ob_armature, selected_objects)
		{
			if(ob_armature->type==OB_ARMATURE)
			{
				if((ob_armature->mode & OB_MODE_POSE) && ob_armature == modifiers_isDeformedByArmature(ob))
				{
					createTransPose(C, t, ob_armature);
					break;
				}
			}
		}
		CTX_DATA_END;
	}
	else if (ob && (ob->mode & OB_MODE_PARTICLE_EDIT) 
		&& PE_start_edit(PE_get_current(scene, ob))) {
		createTransParticleVerts(C, t);
		t->flag |= T_POINTS;

		if(t->data && t->flag & T_PROP_EDIT) {
			sort_trans_data(t);	// makes selected become first in array
			set_prop_dist(t, 1);
			sort_trans_data_dist(t);
		}
	}
	else {
		createTransObject(C, t);
		t->flag |= T_OBJECT;

		if(t->data && t->flag & T_PROP_EDIT) {
			// selected objects are already first, no need to presort
			set_prop_dist(t, 1);
			sort_trans_data_dist(t);
		}

		if (t->ar->regiontype == RGN_TYPE_WINDOW)
		{
			View3D *v3d = t->view;
			RegionView3D *rv3d = CTX_wm_region_view3d(C);
			if(rv3d && (t->flag & T_OBJECT) && v3d->camera == OBACT && rv3d->persp==RV3D_CAMOB)
			{
				t->flag |= T_CAMERA;
			}
		}
	}

// TRANSFORM_FIX_ME
//	/* temporal...? */
//	t->scene->recalc |= SCE_PRV_CHANGED;	/* test for 3d preview */
}




