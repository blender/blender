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

#include <stdlib.h>
#include <string.h>
#include <math.h>

#ifdef HAVE_CONFIG_H
#include <config.h>
#endif

#ifndef WIN32
#include <unistd.h>
#else
#include <io.h>
#include "BLI_winstuff.h"
#endif

#include "MEM_guardedalloc.h"

#include "DNA_action_types.h"
#include "DNA_armature_types.h"
#include "DNA_camera_types.h"
#include "DNA_curve_types.h"
#include "DNA_effect_types.h"
#include "DNA_ika_types.h"
#include "DNA_image_types.h"
#include "DNA_ipo_types.h"
#include "DNA_key_types.h"
#include "DNA_lamp_types.h"
#include "DNA_lattice_types.h"
#include "DNA_mesh_types.h"
#include "DNA_meshdata_types.h"
#include "DNA_meta_types.h"
#include "DNA_object_types.h"
#include "DNA_scene_types.h"
#include "DNA_screen_types.h"
#include "DNA_texture_types.h"
#include "DNA_view3d_types.h"
#include "DNA_world_types.h"
#include "DNA_userdef_types.h"
#include "DNA_property_types.h"
#include "DNA_vfont_types.h"
#include "DNA_constraint_types.h"

#include "BIF_editview.h"
#include "BIF_resources.h"
#include "BIF_mywindow.h"
#include "BIF_gl.h"
#include "BIF_editlattice.h"
#include "BIF_editarmature.h"
#include "BIF_editmesh.h"
#include "BIF_screen.h"
#include "BIF_space.h"
#include "BIF_toolbox.h"

#include "BKE_action.h"
#include "BKE_armature.h"
#include "BKE_blender.h"
#include "BKE_curve.h"
#include "BKE_displist.h"
#include "BKE_effect.h"
#include "BKE_global.h"
#include "BKE_ipo.h"
#include "BKE_lattice.h"
#include "BKE_mball.h"
#include "BKE_object.h"
#include "BKE_utildefines.h"

#include "BSE_view.h"
#include "BSE_edit.h"
#include "BSE_editipo.h"
#include "BSE_editipo_types.h"
#include "BDR_editobject.h"		// reset_slowparents()

#include "BLI_arithb.h"
#include "BLI_editVert.h"
#include "BLI_ghash.h"

#include "PIL_time.h"

#include "blendef.h"

#include "mydevice.h"

extern ListBase editNurb;
extern ListBase editelems;

extern void helpline(float *vec);


#include "transform.h"
#include "transform_generics.h"
#include "transform_constraints.h"
#include "transform_numinput.h"

/* GLOBAL VARIABLE THAT SHOULD MOVED TO SCREEN MEMBER OR SOMETHING  */
TransInfo Trans;
int	LastMode = TFM_TRANSLATION;

/* ************************** Functions *************************** */

/* ************************** CONVERSIONS ************************* */

static int allocTransData(void)
{
	int count, mode=0;
	countall();
	
 	if(mode) count= G.totvert;
	else count= G.totvertsel;
	printf("count: %d\n", count);
	if(G.totvertsel==0) {
		count= 0;
		return count;
	}
	
	Trans.total = count;
	Trans.data= MEM_mallocN(Trans.total*sizeof(TransData), "TransObData(EditMode)");
	return count;
}

void createTransTexspace(void)
{
	TransData *td;
	Object *ob;
	ID *id;
	
	ob= OBACT;
	Trans.total = 1;
	td= Trans.data= MEM_callocN(sizeof(TransData), "TransTexspace");
	td->ext= MEM_callocN(sizeof(TransDataExtension), "TransTexspace");
	
	td->flag= TD_SELECTED;
	VECCOPY(td->center, ob->obmat[3]);
	td->ob = ob;
	
	Mat3CpyMat4(td->mtx, ob->obmat);
	Mat3Inv(td->smtx, td->mtx);
	
	id= ob->data;
	if(id==0);
	else if( GS(id->name)==ID_ME) {
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


/* ********************* pose mode ************* */

/* callback, make sure it's identical structured as next one */
static void count_bone_select(ListBase *lb, int *counter) 
{
	Bone *bone;
	
	for(bone= lb->first; bone; bone= bone->next) {
		if (bone->flag & BONE_SELECTED) {
			/* We don't let IK children get "grabbed" */
			/* ALERT! abusive global Trans here */
			if ( (Trans.mode!=TFM_TRANSLATION) || (bone->flag & BONE_IK_TOPARENT)==0 ) {
				(*counter)++;
				return;	// no transform on children if one parent bone is selected
			}
		}
		count_bone_select( &bone->childbase, counter);
	}
}

/* callback */
static void add_pose_transdata(ListBase *lb, Object *ob, TransData **tdp)
{
	Bone *bone;
	TransData *td= *tdp;
	float	parmat[4][4], tempmat[4][4];
	float tempobmat[4][4];
	float vec[3];
	
	for(bone= lb->first; bone; bone= bone->next) {
		if (bone->flag & BONE_SELECTED) {
			/* We don't let IK children get "grabbed" */
			/* ALERT! abusive global Trans here */
			if ( (Trans.mode!=TFM_TRANSLATION) || (bone->flag & BONE_IK_TOPARENT)==0 ) {
				
				get_bone_root_pos (bone, vec, 1);
				
				//VecAddf (centroid, centroid, vec);
				VECCOPY(td->center, vec);
				
				td->ob = ob;
				td->dist= 0.0f;
				td->flag= TD_SELECTED|TD_USEQUAT;
				td->loc = bone->loc;
				VECCOPY(td->iloc, bone->loc);
				
				td->ext->rot= NULL;
				td->ext->quat= bone->quat;
				td->ext->size= bone->size;
				td->ext->bone= bone; //	FIXME: Dangerous

				QUATCOPY(td->ext->iquat, bone->quat);
				VECCOPY(td->ext->isize, bone->size);
				
				/* Get the matrix of this bone minus the usertransform */
				Mat4CpyMat4 (tempobmat, bone->obmat);
				Mat4One (bone->obmat);
				get_objectspace_bone_matrix(bone, tempmat, 1, 1);
				Mat4CpyMat4 (bone->obmat, tempobmat);

				Mat4MulMat4 (parmat, tempmat, ob->obmat);	/* Original */
				
				Mat3CpyMat4 (td->mtx, parmat);
				Mat3Inv (td->smtx, td->mtx);
				
				(*tdp)++;
				return;	// see above function
			}
		}
		add_pose_transdata(&bone->childbase, ob, tdp);
	}
}

static void createTransPose(void)
{
	bArmature *arm;
	TransData *td;
	TransDataExtension *tdx;
	int i;
	
	Trans.total= 0;	// to be able to return
	
	/* check validity of state */
	arm=get_armature (G.obpose);
	if (arm==NULL) return;
	
	if (arm->flag & ARM_RESTPOS){
		notice ("Transformation not possible while Rest Position is enabled");
		return;
	}
	if (!(G.obpose->lay & G.vd->lay)) return;

	/* copied from old code, no idea. we let linker solve it for now */
	{
		extern void figure_bone_nocalc(Object *ob);
		extern void figure_pose_updating(void);

		/* figure out which bones need calculating */
		figure_bone_nocalc(G.obpose);
		figure_pose_updating();
	}
	
	/* copied from old code, no idea... (ton) */
	apply_pose_armature(arm, G.obpose->pose, 0);
	where_is_armature (G.obpose);
	
	/* count total */
	count_bone_select(&arm->bonebase, &Trans.total);
	
	if(Trans.total==0 && Trans.mode==TFM_TRANSLATION) {
		Trans.mode= TFM_ROTATION;
		count_bone_select(&arm->bonebase, &Trans.total);
	}		
	if(Trans.total==0) return;
	
	/* init trans data */
    td = Trans.data = MEM_mallocN(Trans.total*sizeof(TransData), "TransPoseBone");
    tdx = MEM_mallocN(Trans.total*sizeof(TransDataExtension), "TransPoseBoneExt");
	for(i=0; i<Trans.total; i++, td++, tdx++) {
		td->ext= tdx;
		td->tdi = NULL;
	}	
	/* recursive fill trans data */
	td= Trans.data;
	add_pose_transdata(&arm->bonebase, G.obpose, &td);
	
}

static void createTransArmatureVerts(void)
{
	EditBone *ebo;
	TransData *td;
	float mtx[3][3], smtx[3][3];

	Trans.total = 0;
	for (ebo=G.edbo.first;ebo;ebo=ebo->next){
		if (ebo->flag & BONE_TIPSEL){
			Trans.total++;
		}
		if (ebo->flag & BONE_ROOTSEL){
			Trans.total++;
		}
	}

    if (!Trans.total) return;
	
	Mat3CpyMat4(mtx, G.obedit->obmat);
	Mat3Inv(smtx, mtx);

    td = Trans.data = MEM_mallocN(Trans.total*sizeof(TransData), "TransEditBone");
	
	for (ebo=G.edbo.first;ebo;ebo=ebo->next){
		if (ebo->flag & BONE_TIPSEL){
			VECCOPY (td->iloc, ebo->tail);
			td->loc= ebo->tail;
			td->flag= TD_SELECTED;

			Mat3CpyMat3(td->smtx, smtx);
			Mat3CpyMat3(td->mtx, mtx);

			td->ext = NULL;
			td->tdi = NULL;

			td->dist = 0.0f;
			
			td++;
		}
		if (ebo->flag & BONE_ROOTSEL){
			VECCOPY (td->iloc, ebo->head);
			td->loc= ebo->head;
			td->flag= TD_SELECTED;

			Mat3CpyMat3(td->smtx, smtx);
			Mat3CpyMat3(td->mtx, mtx);

			td->ext = NULL;
			td->tdi = NULL;

			td->dist = 0.0f;
		
			td++;
		}
			
	}
}

static void createTransMBallVerts(void)
{
 	MetaElem *ml;
	TransData *td;
	TransDataExtension *tx;
	float mtx[3][3], smtx[3][3];
	int count;

	count = allocTransData();
	if (!count) return;

	tx = MEM_mallocN(Trans.total*sizeof(TransDataExtension), "MetaElement_TransExtension");

	Mat3CpyMat4(mtx, G.obedit->obmat);
	Mat3Inv(smtx, mtx);
    
	td = Trans.data;
    ml= editelems.first;
	while(ml) {
		if(ml->flag & SELECT) {
			td->loc= &ml->x;
			VECCOPY(td->iloc, td->loc);
			VECCOPY(td->center, td->loc);
			td->flag= TD_SELECTED;

			Mat3CpyMat3(td->smtx, smtx);
			Mat3CpyMat3(td->mtx, mtx);

			td->ext = tx;
			td->tdi = NULL;

			tx->size = &ml->expx;
			tx->isize[0] = ml->expx;
			tx->isize[1] = ml->expy;
			tx->isize[2] = ml->expz;

			tx->rot = NULL;

			td->dist = 0.0f;

			td++;
			tx++;
		}
		ml= ml->next;
	}
} 

static void createTransCurveVerts(void)
{
	TransData *td = NULL;
  	Nurb *nu;
	BezTriple *bezt;
	BPoint *bp;
	float mtx[3][3], smtx[3][3];
	int a;
	int count=0;
	int mode = 0; /*This used for. . .what?*/
	//int proptrans= 0;

	count = allocTransData();
	if (!count) return;

	Mat3CpyMat4(mtx, G.obedit->obmat);
	Mat3Inv(smtx, mtx);

    td = Trans.data;
	nu= editNurb.first;
	while(nu) {
		if((nu->type & 7)==CU_BEZIER) {
			a= nu->pntsu;
			bezt= nu->bezt;
			while(a--) {
				if(bezt->hide==0) {
					if(mode==1 || (bezt->f1 & 1)) {
						VECCOPY(td->iloc, bezt->vec[0]);
						td->loc= bezt->vec[0];
						VECCOPY(td->center, td->loc);
						td->flag= TD_SELECTED;
						td->ext = NULL;
						td->tdi = NULL;

						Mat3CpyMat3(td->smtx, smtx);
						Mat3CpyMat3(td->mtx, mtx);

						td->dist = 0.0f;
			
						td++;
						count++;
					}
					if(mode==1 || (bezt->f2 & 1)) {
						VECCOPY(td->iloc, bezt->vec[1]);
						td->loc= bezt->vec[1];
						VECCOPY(td->center, td->loc);
						td->flag= TD_SELECTED;
						td->ext = NULL;
						td->tdi = NULL;

						Mat3CpyMat3(td->smtx, smtx);
						Mat3CpyMat3(td->mtx, mtx);

						td->dist = 0.0f;
			
						td++;
						count++;
					}
					if(mode==1 || (bezt->f3 & 1)) {
						VECCOPY(td->iloc, bezt->vec[2]);
						td->loc= bezt->vec[2];
						VECCOPY(td->center, td->loc);
						td->flag= TD_SELECTED;
						td->ext = NULL;
						td->tdi = NULL;

						Mat3CpyMat3(td->smtx, smtx);
						Mat3CpyMat3(td->mtx, mtx);

						td->dist = 0.0f;
			
						td++;
						count++;
					}
				}
				bezt++;
			}
		}
		else {
			a= nu->pntsu*nu->pntsv;
			bp= nu->bp;
			while(a--) {
				if(bp->hide==0) {
					if(mode==1 || (bp->f1 & 1)) {
						VECCOPY(td->iloc, bp->vec);
						td->loc= bp->vec;
						VECCOPY(td->center, td->loc);
						td->flag= TD_SELECTED;
						td->ext = NULL;
						td->tdi = NULL;

						Mat3CpyMat3(td->smtx, smtx);
						Mat3CpyMat3(td->mtx, mtx);

						td->dist = 0.0f;
			
						td++;
						count++;
					}
				}
				bp++;
			}
		}
		nu= nu->next;
	}
}

static void createTransLatticeVerts(void)
{
	TransData *td = NULL;
	int count = 0;
	BPoint *bp;
	float mtx[3][3], smtx[3][3];
	int mode = 0; /*This used for proportional editing*/
	/*should find a function that does this. . . what else is this used for? I DONT KNOW!*/
	int a;
	//int proptrans= 0;

	bp= editLatt->def;
	
	
 	count = allocTransData();
	
	if (!count) return;
	
	Mat3CpyMat4(mtx, G.obedit->obmat);
	Mat3Inv(smtx, mtx);

	td = Trans.data;
	bp= editLatt->def;
	a= editLatt->pntsu*editLatt->pntsv*editLatt->pntsw;
	while(a--) {
		if(mode==1 || (bp->f1 & 1)) {
			if(bp->hide==0) {
				VECCOPY(td->iloc, bp->vec);
				td->loc= bp->vec;
				VECCOPY(td->center, td->loc);
				td->flag= TD_SELECTED;

				Mat3CpyMat3(td->smtx, smtx);
				Mat3CpyMat3(td->mtx, mtx);

				td->ext = NULL;
				td->tdi = NULL;

				td->dist = 0.0f;

				td++;
				count++;
			}
		}
		bp++;
	}
} 

static void VertsToTransData(TransData *td, EditVert *eve)
{
	td->flag = 0;
	td->loc = eve->co;
	VECCOPY(td->center, td->loc);
	VECCOPY(td->iloc, td->loc);
	td->ext = NULL;
	td->tdi = NULL;
}

static void createTransEditVerts(void)
{
	TransData *tob = NULL;
	int totsel = 0;
	EditMesh *em = G.editMesh;
	EditVert *eve;
	float mtx[3][3], smtx[3][3];
	/*should find a function that does this. . .*/
	// int proptrans= 0;
		
	// transform now requires awareness for select mode, so we tag the f1 flags in verts
	if(G.scene->selectmode & SCE_SELECT_VERTEX) {
		for(eve= em->verts.first; eve; eve= eve->next) {
			if(eve->h==0 && (eve->f & SELECT)) {
				eve->f1= SELECT;
				Trans.total++;
			}
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
		for(eve= em->verts.first; eve; eve= eve->next)
			if(eve->f1)
				Trans.total++;
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
		for(eve= em->verts.first; eve; eve= eve->next)
			if(eve->f1)
				Trans.total++;
	}
	
	totsel = Trans.total;
	/* proportional edit exception... */
	if((G.f & G_PROPORTIONAL) && Trans.total) {
		for(eve= em->verts.first; eve; eve= eve->next) {
			if(eve->h==0 && (!(eve->f1 & SELECT))) {
				eve->f1 = 2;
				Trans.total++;
			}
		}
	}
	
	/* and now make transverts */
    if (!Trans.total) return;
	
	Mat3CpyMat4(mtx, G.obedit->obmat);
	Mat3Inv(smtx, mtx);

    tob = Trans.data = MEM_mallocN(Trans.total*sizeof(TransData), "TransEditVert");

	for (eve=em->verts.first; eve; eve=eve->next)
	{
		if (eve->f1 == SELECT) {
			VertsToTransData(tob, eve);

			tob->flag |= TD_SELECTED;

			Mat3CpyMat3(tob->smtx, smtx);
			Mat3CpyMat3(tob->mtx, mtx);

			tob->dist = 0.0f;

			tob++;
		}	
	}	

	/* PROPORTIONAL*/
	if (G.f & G_PROPORTIONAL) {
		for (eve=em->verts.first; eve; eve=eve->next)
		{
			TransData *td;
			int i;
			float dist, vec[3];
			if (eve->f1 == 2) {

				VertsToTransData(tob, eve);

				Mat3CpyMat3(tob->smtx, smtx);
				Mat3CpyMat3(tob->mtx, mtx);
			
				tob->dist = -1;

				td = Trans.data;
				for (i = 0; i < totsel; i++, td++) {
					VecSubf(vec, tob->center, td->center);
					Mat3MulVecfl(mtx, vec);
					dist = Normalise(vec);
					if (tob->dist == -1) {
						tob->dist = dist;
					}
					else if (dist < tob->dist) {
						tob->dist = dist;
					}
				}

				tob++;
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
static void add_tdi_poin(float *poin, float *old, float delta)
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

static void ObjectToTransData(TransData *td, Object *ob) 
{
	float obmtx[3][3];
	Object *tr;
	void *cfirst, *clast;

	cfirst = ob->constraints.first;
	clast = ob->constraints.last;
	ob->constraints.first=ob->constraints.last=NULL;

	tr= ob->track;
	ob->track= NULL;

	where_is_object(ob);

	ob->track= tr;

	ob->constraints.first = cfirst;
	ob->constraints.last = clast;

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

	Mat3CpyMat4(td->mtx, ob->obmat);

	if (ob->parent)
	{
		float totmat[3][3], obinv[3][3];
		
		/* we calculate smtx without obmat: so a parmat */
		object_to_mat3(ob, obmtx);
		Mat3CpyMat4(totmat, ob->obmat);
		Mat3Inv(obinv, totmat);
		Mat3MulMat3(td->smtx, obmtx, obinv);
		Mat3Inv(td->mtx, td->smtx);
	}
	else
	{
		Mat3One(td->smtx);
		Mat3One(td->mtx);
	}
}

/* only used in function below, stuff to be removed */
static Object *is_a_parent_selected_int(Object *startob, Object *ob, GHash *done_hash) 
{
	if (ob!=startob && TESTBASE(ob))
		return ob;
	
	if (BLI_ghash_haskey(done_hash, ob))
		return NULL;
	else
		BLI_ghash_insert(done_hash, ob, NULL);
	
	if (ob->parent) {
		Object *par= is_a_parent_selected_int(startob, ob->parent, done_hash);
		if (par)
			return par;
	}
	return NULL;
}

/* only used in function below, stuff to be removed */
static Object *is_a_parent_selected(Object *ob) 
{
	GHash *gh= BLI_ghash_new(BLI_ghashutil_ptrhash, BLI_ghashutil_ptrcmp);
	Object *res= is_a_parent_selected_int(ob, ob, gh);
	BLI_ghash_free(gh, NULL, NULL);
	
	return res;
}



/* sets flags in Bases to define whether they take part in transform */
/* it deselects Bases, so we have to call the clear function always after */
static void set_trans_object_base_flags(TransInfo *t)
{
	/*
	 if Base selected and has parent selected:
	 base->flag= BA_WASSEL+BA_PARSEL
	 if base not selected and parent selected:
	 base->flag= BA_PARSEL
	 */
	GHash *object_to_base_hash= NULL; 
	Base *base;
	
	/* moved to start of function, it is needed for hooks now too */
	if (!object_to_base_hash) {
		Base *b;
		object_to_base_hash= BLI_ghash_new(BLI_ghashutil_ptrhash, BLI_ghashutil_ptrcmp);
		
		for (b= FIRSTBASE; b; b= b->next)
			BLI_ghash_insert(object_to_base_hash, b->object, b);
	}
	
	/* makes sure base flags and object flags are identical */
	copy_baseflags();
	
	for (base= FIRSTBASE; base; base= base->next) {
		base->flag &= ~(BA_PARSEL+BA_WASSEL);
		
		if( (base->lay & G.vd->lay) && base->object->id.lib==0) {
			Object *ob= base->object;
			Object *parsel= is_a_parent_selected(ob);
			
			/* parentkey here? */
			
			if(parsel) {
				if(base->flag & SELECT) {
					base->flag &= ~SELECT;
					base->flag |= (BA_PARSEL+BA_WASSEL);
				}
				else base->flag |= BA_PARSEL;
			}
			
			if(t->mode==TFM_TRANSLATION)  {
				if(ob->track && TESTBASE(ob->track) && (base->flag & SELECT)==0)  
					base->flag |= BA_PARSEL;
			}
			
			/* updates? */
			if(ob->hooks.first) {
				Base *b;
				ObHook *hook= ob->hooks.first;
				
				while(hook) {
					if(hook->parent) {
						Object *parsel= is_a_parent_selected(hook->parent);
						
						b= BLI_ghash_lookup(object_to_base_hash, hook->parent);
						if(parsel || ((base->flag | b->flag) & (SELECT | BA_PARSEL)) ) {
							base->flag |= BA_DISP_UPDATE;
						}
					}
					hook= hook->next;
				}
			}
			
			if(ob->parent && ob->parent->type==OB_LATTICE)
				if(ob->parent->hooks.first) base->flag |= BA_DISP_UPDATE;
			
			if(base->flag & (SELECT | BA_PARSEL)) {
				
				base->flag |= BA_WHERE_UPDATE;
				
				if(ob->parent) {
					if(ob->parent->type==OB_LATTICE) base->flag |= BA_DISP_UPDATE;
					else if(ob->partype==PARSKEL) {
						if ELEM3(ob->parent->type, OB_IKA, OB_CURVE, OB_ARMATURE) 
							base->flag |= BA_DISP_UPDATE;
					}
				}
				if(ob->track) {
					;
				}
				
				if( give_parteff(ob) ) base->flag |= BA_DISP_UPDATE;
				
				if(ob->type==OB_MBALL) {
					Base *b;
					
					b= BLI_ghash_lookup(object_to_base_hash, find_basis_mball(ob));
					b->flag |= BA_DISP_UPDATE;
				}
			}
		}
	}
	
	if (object_to_base_hash)
		BLI_ghash_free(object_to_base_hash, NULL, NULL);
	
}

static void clear_trans_object_base_flags(void)
{
	Base *base;
	
	base= FIRSTBASE;
	while(base) {
		if(base->flag & BA_WASSEL) base->flag |= SELECT;
		base->flag &= ~(BA_PARSEL+BA_WASSEL);
		
		base->flag &= ~(BA_DISP_UPDATE+BA_WHERE_UPDATE+BA_DO_IPO);
		
		/* pose here? */
		if (base->object->pose) {
			Object *ob= base->object;
			bPoseChannel *chan;
			for (chan = ob->pose->chanbase.first; chan; chan=chan->next) {
				chan->flag &= ~PCHAN_TRANS_UPDATE;
			}
		}
		
		base= base->next;
	}
	copy_baseflags();
}


static void createTransObject(void)
{
	TransData *td = NULL;
	TransDataExtension *tx;
	Object *ob;
	Base *base;
	IpoKey *ik;
	ListBase elems;
	
	/* hackish... but we have to do it somewhere */
	reset_slowparents();
	
	set_trans_object_base_flags(&Trans);
	
	/* count */	
	for(base= FIRSTBASE; base; base= base->next) {
		if TESTBASELIB(base) {
			ob= base->object;
			
			/* store ipo keys? */
			if(ob->ipo && ob->ipo->showkey && (ob->ipoflag & OB_DRAWKEY)) {
				elems.first= elems.last= NULL;
				make_ipokey_transform(ob, &elems, 1); /* '1' only selected keys */
				
				pushdata(&elems, sizeof(ListBase));
				
				for(ik= elems.first; ik; ik= ik->next) Trans.total++;

				if(elems.first==NULL) Trans.total++;
			}
			else {
				Trans.total++;
			}
		}
	}

	if(!Trans.total) {
		/* clear here, main transform function escapes too */
		clear_trans_object_base_flags();
		return;
	}
	
	td = Trans.data = MEM_mallocN(Trans.total*sizeof(TransData), "TransOb");
	tx = MEM_mallocN(Trans.total*sizeof(TransDataExtension), "TransObExtension");

	for(base= FIRSTBASE; base; base= base->next) {
		if TESTBASELIB(base) {
			ob= base->object;
			
			td->flag= TD_SELECTED|TD_OBJECT;
			td->ext = tx;
			td->dist = 0.0f;

			/* store ipo keys? */
			if(ob->ipo && ob->ipo->showkey && (ob->ipoflag & OB_DRAWKEY)) {
				
				popfirst(&elems);	// bring back pushed listbase
				
				if(elems.first) {
					float cfraont;
					int ipoflag;
					
					base->flag |= BA_DO_IPO+BA_WASSEL;
					base->flag &= ~SELECT;
					
					cfraont= CFRA;
					set_no_parent_ipo(1);
					ipoflag= ob->ipoflag;
					ob->ipoflag &= ~OB_OFFS_OB;
					
					pushdata(ob->loc, 7*3*4); // tsk! tsk!
					
					for(ik= elems.first; ik; ik= ik->next) {
						
						/* weak... this doesn't correct for floating values, giving small errors */
						CFRA= ik->val/G.scene->r.framelen;
						
						do_ob_ipo(ob);
						ObjectToTransData(td, ob);	// does where_is_object()
						
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
				}
				else {
					ObjectToTransData(td, ob);
					td->tdi= NULL;
				}
			}
			else {
				ObjectToTransData(td, ob);
				td->tdi= NULL;
			}
			td++;
			tx++;
		}
	}
}

static void createTransData(TransInfo *t) 
{
	if( t->mode & TFM_TEX) {
		createTransTexspace();
		t->mode &= ~TFM_TEX;	// now becoming normal grab/rot/scale
	}
	else if (G.obpose) {
		createTransPose();
	}
	else if (G.obedit) {
		if (G.obedit->type == OB_MESH) {
			createTransEditVerts();	
   		}
		else if ELEM(G.obedit->type, OB_CURVE, OB_SURF) {
			createTransCurveVerts();
		}
		else if (G.obedit->type==OB_LATTICE) {
			createTransLatticeVerts();
		}
		else if (G.obedit->type==OB_MBALL) {
			createTransMBallVerts();
		}
		else if (G.obedit->type==OB_ARMATURE) {
			createTransArmatureVerts();
  		}					  		
		else {
			printf("not done yet! only have mesh surface curve\n");
		}
	}
	else {
		createTransObject();
	}
}

#define TRANS_CANCEL	2
#define TRANS_CONFIRM	1

/* ************************** TRANSFORMATIONS **************************** */

void Transform(int mode) 
{
	int ret_val = 0;
	short pmval[2] = {0, 0}, mval[2], val;
	float mati[3][3];
	unsigned short event;

	/*joeedh -> hopefully may be what makes the old transform() constant*/
	/* ton: I doubt, but it doesnt harm for now. shouldnt be needed though */
	areawinset(curarea->win);

	Mat3One(mati);

	/* stupid PET initialisation code */
	/* START */
	if (Trans.propsize == 0.0f) {
		Trans.propsize = 1.0;
	}
	/* END */

	if (mode == TFM_REPEAT) {
		mode = LastMode;
	}
	else {
		LastMode = mode;
	}
	
	initTransModeFlags(&Trans, mode);	// modal settings in struct Trans

	initTrans(&Trans);					// internal data, mouse, vectors

	createTransData(&Trans);			// make TransData structs from selection

	if (Trans.total == 0)
		return;

	/* EVIL! posemode code can switch translation to rotate when 1 bone is selected. will be removed (ton) */
	/* EVIL2: we gave as argument also texture space context bit... was cleared */
	mode= Trans.mode;
	
	calculatePropRatio(&Trans);
	calculateCenter(&Trans);

	switch (mode) {
	case TFM_TRANSLATION:
		initTranslation(&Trans);
		break;
	case TFM_ROTATION:
		initRotation(&Trans);
		break;
	case TFM_RESIZE:
		initResize(&Trans);
		break;
	case TFM_TOSPHERE:
		initToSphere(&Trans);
		break;
	case TFM_SHEAR:
		initShear(&Trans);
		break;
	}

	// Emptying event queue
	while( qtest() ) {
		event= extern_qread(&val);
	}

	Trans.redraw = 1;

	while (ret_val == 0) {
		
		getmouseco_areawin(mval);
		
		if (mval[0] != pmval[0] || mval[1] != pmval[1]) {
			Trans.redraw = 1;
		}
		if (Trans.redraw) {
			pmval[0] = mval[0];
			pmval[1] = mval[1];

			if (Trans.transform) {
				Trans.transform(&Trans, mval);
			}
			Trans.redraw = 0;
		}
		
		/* essential for idling subloop */
		if( qtest()==0) PIL_sleep_ms(2);

		while( qtest() ) {
			event= extern_qread(&val);

			if(val) {
				switch (event){
				case MIDDLEMOUSE:
					selectConstraint(&Trans);
					Trans.redraw = 1;
					break;
				case ESCKEY:
				case RIGHTMOUSE:
					ret_val = TRANS_CANCEL;
					break;
				case LEFTMOUSE:
				case SPACEKEY:
				case PADENTER:
				case RETKEY:
					ret_val = TRANS_CONFIRM;
					break;
				case GKEY:
				case SKEY:
				case RKEY:
					if (G.qual == LR_CTRLKEY)
						applyTransObjects(&Trans);
					else
						restoreTransObjects(&Trans);
					break;
				case XKEY:
					if (G.qual == 0)
						setConstraint(&Trans, mati, (APPLYCON|CONAXIS0));
					else if (G.qual == LR_CTRLKEY)
						setConstraint(&Trans, mati, (APPLYCON|CONAXIS1|CONAXIS2));
					break;
				case YKEY:
					if (G.qual == 0)
						setConstraint(&Trans, mati, (APPLYCON|CONAXIS1));
					else if (G.qual == LR_CTRLKEY)
						setConstraint(&Trans, mati, (APPLYCON|CONAXIS0|CONAXIS2));
					break;
				case ZKEY:
					if (G.qual == 0)
						setConstraint(&Trans, mati, (APPLYCON|CONAXIS2));
					else if (G.qual == LR_CTRLKEY)
						setConstraint(&Trans, mati, (APPLYCON|CONAXIS0|CONAXIS1));
					break;
				case OKEY:
					if (G.qual==LR_SHIFTKEY) {
						extern int prop_mode;
						prop_mode = (prop_mode+1)%5;
						calculatePropRatio(&Trans);
						Trans.redraw= 1;
					}
					break;
				case WHEELDOWNMOUSE:
				case PADPLUSKEY:
					if(G.f & G_PROPORTIONAL) {
						Trans.propsize*= 1.1f;
						calculatePropRatio(&Trans);
						Trans.redraw= 1;
					}
					break;
				case WHEELUPMOUSE:
				case PADMINUS:
					if(G.f & G_PROPORTIONAL) {
						Trans.propsize*= 0.90909090f;
						calculatePropRatio(&Trans);
						Trans.redraw= 1;
					}
					break;
				}
				Trans.redraw |= handleNumInput(&(Trans.num), event);
				arrows_move_cursor(event);
			}
			else {
				switch (event){
				case MIDDLEMOUSE:
					chooseConstraint(&Trans);
					Trans.redraw = 1;
					break;
				case LEFTMOUSE:
				case RIGHTMOUSE:
					/* commented out, doesn't work for actions started with menu */
					// ret_val = TRANS_CONFIRM;
					break;
				}
			}
		}
	}
	
	
	if(ret_val == TRANS_CANCEL) {
		restoreTransObjects(&Trans);
	}
	else {
		BIF_undo_push("Transform");
	}
	
	/* free data, reset vars */
	postTrans(&Trans);
	
	/* mess from old transform, just for now (ton) */
	{
		char cmode='g';
		
		if(mode==TFM_RESIZE) cmode= 's';
		else if(mode==TFM_ROTATION) cmode= 'r';
		/* aftertrans does displists, ipos and action channels */
		special_aftertrans_update(cmode, 0, ret_val == TRANS_CANCEL, 0 /*keyflags*/);
		
		if(G.obedit==NULL && G.obpose==NULL)
			clear_trans_object_base_flags();
	}
	
	
	/* send events out for redraws */
	allqueue(REDRAWVIEW3D, 0);
	allqueue(REDRAWBUTSOBJECT, 0);
	scrarea_queue_headredraw(curarea);
}

/* ************************** WRAP *************************** */

void initWrap(TransInfo *t) 
{
	float min[3], max[3], loc[3];
	int i;
	calculateCenterCursor(t);
	t->num.idx_max = 0;
	t->transform = Wrap;

	for(i = 0; i < t->total; i++) {
		VECCOPY(loc, t->data[i].iloc);
		if (G.obedit) {
			Mat4MulVecfl(G.obedit->obmat, loc);
		}
		Mat4MulVecfl(G.vd->viewmat, loc);
		if (i) {
			MinMax3(min, max, loc);
		}
		else {
			VECCOPY(max, loc);
			VECCOPY(min, loc);
		}
	}


	t->fac = (float)(t->center2d[0] - t->imval[0]);
}


int Wrap(TransInfo *t, short mval[2])
{
	return 1;
}

/* ************************** SHEAR *************************** */

void initShear(TransInfo *t) 
{
	t->num.idx_max = 0;
	t->transform = Shear;
	t->fac = (float)(t->center2d[0] - t->imval[0]);
}

int Shear(TransInfo *t, short mval[2]) 
{
	float vec[3];
	float smat[3][3], tmat[3][3], totmat[3][3], persmat[3][3], persinv[3][3];
	float value;
	int i;
	char str[50];
	TransData *td = t->data;

	Mat3CpyMat4(persmat, G.vd->viewmat);
	Mat3Inv(persinv, persmat);

	value = -0.005f * ((float)(t->center2d[0] - mval[0]) - t->fac);

	apply_grid1(&value, t->num.idx_max, 0.1f);

	applyNumInput(&t->num, &value);

	/* header print for NumInput */
	if (hasNumInput(&t->num)) {
		char c[20];

		outputNumInput(&(t->num), c);

		sprintf(str, "Shear: %s %s", c, t->proptext);
	}
	else {
		/* default header print */
		sprintf(str, "Shear: %.3f %s", value, t->proptext);
	}
	
	Mat3One(smat);
	smat[1][0] = value;
	Mat3MulMat3(tmat, smat, persmat);
	Mat3MulMat3(totmat, persinv, tmat);
	
	for(i = 0 ; i < t->total; i++, td++) {
		if (td->flag & TD_NOACTION)
			continue;
		if (G.obedit) {
			float mat3[3][3];
			Mat3MulMat3(mat3, totmat, td->mtx);
			Mat3MulMat3(tmat, td->smtx, mat3);
		}
		else {
			Mat3CpyMat3(tmat, totmat);
		}
		VecSubf(vec, td->center, t->center);

		Mat3MulVecfl(tmat, vec);

		VecAddf(vec, vec, t->center);
		VecSubf(vec, vec, td->center);

		VecMulf(vec, td->factor);

		VecAddf(td->loc, td->iloc, vec);
	}

	recalcData(t);

	headerprint(str);

	force_draw(0);

	helpline (t->center);

	return 1;
}

/* ************************** RESIZE *************************** */

void initResize(TransInfo *t) 
{
	Trans.fac = (float)sqrt( (float)
		(
			(Trans.center2d[1] - Trans.imval[1])*(Trans.center2d[1] - Trans.imval[1])
		+
			(Trans.center2d[0] - Trans.imval[0])*(Trans.center2d[0] - Trans.imval[0])
		) );

	t->num.idx_max = 2;
	t->transform = Resize;
}

int Resize(TransInfo *t, short mval[2]) 
{
	TransData *td = t->data;
	float vec[3];
	float size[3], tsize[3], mat[3][3], tmat[3][3];
	float ratio;
	int i;
	char str[50];

	ratio = (float)sqrt( (float)
		(
			(t->center2d[1] - mval[1])*(t->center2d[1] - mval[1])
		+
			(t->center2d[0] - mval[0])*(t->center2d[0] - mval[0])
		) ) / t->fac;

	size[0] = size[1] = size[2] = ratio;

	apply_grid1(size, t->num.idx_max, 0.1f);

	if (t->con.applyVec) {
		t->con.applyVec(t, NULL, size, tsize);
		VECCOPY(size, tsize);
	}

	applyNumInput(&t->num, size);

	/* header print for NumInput */
	if (hasNumInput(&t->num)) {
		char c[60];

		outputNumInput(&(t->num), c);

		sprintf(str, "Size X: %s Y: %s Z: %s %s", &c[0], &c[20], &c[40], t->proptext);
	}
	else {
		/* default header print */
		sprintf(str, "Size X: %.3f Y: %.3f Z: %.3f %s", size[0], size[1], size[2], t->proptext);
	}
	
	SizeToMat3(size, mat);
	for(i = 0 ; i < t->total; i++, td++) {
		float smat[3][3];
		if (td->flag & TD_NOACTION)
			continue;

		if (!(td->flag & TD_OBJECT)) {
			Mat3MulMat3(smat, mat, td->smtx);
			Mat3MulMat3(tmat, td->mtx, smat);
		}
		else {
			Mat3CpyMat3(tmat, mat);
		}

		if (td->ext) {
			float fsize[3];

			if (td->flag & TD_OBJECT) {
				float obsizemat[3][3];
				Mat3MulMat3(obsizemat, td->smtx, tmat);
				Mat3ToSize(obsizemat, fsize);
			}
			else {
				Mat3ToSize(tmat, fsize);
			}
			
			/* handle ipokeys? */
			if(td->tdi) {
				TransDataIpokey *tdi= td->tdi;
				/* calculate delta size (equal for size and dsize) */
				
				// commented out for now
				vec[0]= (tdi->oldsize[0])*(fsize[0] -1.0f) * td->factor;
				vec[1]= (tdi->oldsize[1])*(fsize[1] -1.0f) * td->factor;
				vec[2]= (tdi->oldsize[2])*(fsize[2] -1.0f) * td->factor;
				
				add_tdi_poin(tdi->sizex, tdi->oldsize,   vec[0]);
				add_tdi_poin(tdi->sizey, tdi->oldsize+1, vec[1]);
				add_tdi_poin(tdi->sizez, tdi->oldsize+2, vec[2]);
				
			}
			else {
				// TEMPORARY NAIVE CODE
				td->ext->size[0] = td->ext->isize[0] + td->ext->isize[0] * (fsize[0] - 1.0f) * td->factor;
				td->ext->size[1] = td->ext->isize[1] + td->ext->isize[1] * (fsize[1] - 1.0f) * td->factor;
				td->ext->size[2] = td->ext->isize[2] + td->ext->isize[2] * (fsize[2] - 1.0f) * td->factor;
			}
		}
		VecSubf(vec, td->center, t->center);

		Mat3MulVecfl(tmat, vec);

		VecAddf(vec, vec, t->center);
		VecSubf(vec, vec, td->center);

		VecMulf(vec, td->factor);

		if (td->flag & TD_OBJECT) {
			Mat3MulVecfl(td->smtx, vec);
		}

		VecAddf(td->loc, td->iloc, vec);
	}

	recalcData(t);

	headerprint(str);

	force_draw(0);

	helpline (t->center);

	return 1;
}

/* ************************** TOSPHERE *************************** */

void initToSphere(TransInfo *t) 
{
	TransData *td = t->data;
	int i;

	// Calculate average radius
	for(i = 0 ; i < t->total; i++, td++) {
		t->val += VecLenf(t->center, td->iloc);
	}

	t->val /= (float)t->total;

	Trans.fac = (float)sqrt( (float)
		(
			(Trans.center2d[1] - Trans.imval[1])*(Trans.center2d[1] - Trans.imval[1])
		+
			(Trans.center2d[0] - Trans.imval[0])*(Trans.center2d[0] - Trans.imval[0])
		) );

	t->num.idx_max = 0;
	t->transform = ToSphere;
}



int ToSphere(TransInfo *t, short mval[2]) 
{
	float vec[3];
	float ratio, radius;
	int i;
	char str[50];
	TransData *td = t->data;

	ratio = (float)sqrt( (float)
		(
			(t->center2d[1] - mval[1])*(t->center2d[1] - mval[1])
		+
			(t->center2d[0] - mval[0])*(t->center2d[0] - mval[0])
		) ) / t->fac;

	apply_grid1(&ratio, t->num.idx_max, 0.1f);

	applyNumInput(&t->num, &ratio);

	if (ratio > 1.0f)
		ratio = 1.0f;

	/* header print for NumInput */
	if (hasNumInput(&t->num)) {
		char c[20];

		outputNumInput(&(t->num), c);

		sprintf(str, "To Sphere: %s %s", c, t->proptext);
	}
	else {
		/* default header print */
		sprintf(str, "To Sphere: %.4f %s", ratio, t->proptext);
	}
	
	
	for(i = 0 ; i < t->total; i++, td++) {
		float tratio;
		if (td->flag & TD_NOACTION)
			continue;
		VecSubf(vec, td->iloc, t->center);

		radius = Normalise(vec);

		tratio = 1.0f - ((1.0f - ratio) * td->factor);

		VecMulf(vec, radius * tratio + t->val * (1.0f - tratio));

		VecAddf(td->loc, t->center, vec);
	}

	recalcData(t);

	headerprint(str);

	force_draw(0);

	helpline (t->center);

	return 1;
}

/* ************************** ROTATION *************************** */

void initRotation(TransInfo *t) 
{
	t->num.idx_max = 0;
	t->fac = 0;
	t->transform = Rotation;
}

int Rotation(TransInfo *t, short mval[2]) 
{
	TransData *td = t->data;
	int i;
	char str[50];

	float final;

	int dx2 = t->center2d[0] - mval[0];
	int dy2 = t->center2d[1] - mval[1];
	float B = (float)sqrt(dx2*dx2+dy2*dy2);

	int dx1 = t->center2d[0] - t->imval[0];
	int dy1 = t->center2d[1] - t->imval[1];
	float A = (float)sqrt(dx1*dx1+dy1*dy1);

	int dx3 = mval[0] - t->imval[0];
	int dy3 = mval[1] - t->imval[1];

	float deler= ((dx1*dx1+dy1*dy1)+(dx2*dx2+dy2*dy2)-(dx3*dx3+dy3*dy3))
		/ (2 * A * B);

	float dphi;

	float vec[3], axis[3];
	float mat[3][3], totmat[3][3], omat[3][3], smat[3][3];

	if (G.obedit) {
		Mat3CpyMat4(omat, G.obedit->obmat);
	}

	VECCOPY(axis, G.vd->persinv[2]);
	Normalise(axis);

	dphi = saacos(deler);
	if( (dx1*dy2-dx2*dy1)>0.0 ) dphi= -dphi;

	if(G.qual & LR_SHIFTKEY) t->fac += dphi/30.0f;
	else t->fac += dphi;

	final = t->fac;

	apply_grid2(&final, t->num.idx_max, (float)((5.0/180)*M_PI), 0.2f);

	t->imval[0] = mval[0];
	t->imval[1] = mval[1];

	if (t->con.applyRot) {
		t->con.applyRot(t, NULL, axis);
	}

	if (hasNumInput(&t->num)) {
		char c[20];

		applyNumInput(&t->num, &final);

		outputNumInput(&(t->num), c);

		sprintf(str, "Rot: %s %s", &c[0], t->proptext);

		final *= (float)(M_PI / 180.0);
	}
	else {
		sprintf(str, "Rot: %.2f %s", 180.0*final/M_PI, t->proptext);
	}

	//printf("Axis %f %f %f\n", axis[0], axis[1], axis[2]);
	VecRotToMat3(axis, final * td->factor, mat);

	for(i = 0 ; i < t->total; i++, td++) {
		if (td->flag & TD_NOACTION)
			continue;

		if (t->con.applyRot) {
			t->con.applyRot(t, td, axis);
			VecRotToMat3(axis, final * td->factor, mat);
		}
		else if (G.f & G_PROPORTIONAL) {
			VecRotToMat3(axis, final * td->factor, mat);
		}

		if (G.obedit) {
			Mat3MulMat3(totmat, mat, omat);
			Mat3MulMat3(smat, td->smtx, totmat);

			VecSubf(vec, td->iloc, t->center);
			Mat3MulVecfl(smat, vec);

			VecAddf(td->loc, vec, t->center);
		}
		else {
			float eul[3], fmat[3][3];

			/* translation */
			VecSubf(vec, td->center, t->center);
			Mat3MulVecfl(mat, vec);
			VecAddf(vec, vec, t->center);
			/* vec now is the location where the object has to be */
			VecSubf(vec, vec, td->center);
			Mat3MulVecfl(td->smtx, vec);

			VecAddf(td->loc, td->iloc, vec);
			
			if(td->flag & TD_USEQUAT) {
				float quat[4];
				
				Mat3MulSerie(fmat, td->mtx, mat, td->smtx, 0, 0, 0, 0, 0);
				
				Mat3ToQuat(fmat, quat);	// Actual transform
				
				QuatMul(td->ext->quat, quat, td->ext->iquat);
			}
			else {
				float obmat[3][3];
				
				/* are there ipo keys? */
				if(td->tdi) {
					TransDataIpokey *tdi= td->tdi;
					float rot[3];
					
					/* calculate the total rotatation in eulers */
					VecAddf(eul, td->ext->irot, td->ext->drot);
					EulToMat3(eul, obmat);
					/* mat = transform, obmat = object rotation */
					Mat3MulMat3(fmat, mat, obmat);
					Mat3ToEul(fmat, eul);
					compatible_eul(eul, td->ext->irot);
					
					/* correct back for delta rot */
					if(tdi->flag & TOB_IPODROT) {
						VecSubf(rot, eul, td->ext->irot);
					}
					else {
						VecSubf(rot, eul, td->ext->drot);
					}
					
					VecMulf(rot, 9.0/M_PI_2);
					VecSubf(rot, rot, tdi->oldrot);
					
					add_tdi_poin(tdi->rotx, tdi->oldrot, rot[0]);
					add_tdi_poin(tdi->roty, tdi->oldrot+1, rot[1]);
					add_tdi_poin(tdi->rotz, tdi->oldrot+2, rot[2]);
				}
				else {
					
					/* calculate the total rotatation in eulers */
					VecAddf(eul, td->ext->irot, td->ext->drot); /* we have to correct for delta rot */
					EulToMat3(eul, obmat);
					/* mat = transform, obmat = object rotation */
					Mat3MulMat3(fmat, mat, obmat);
					Mat3ToEul(fmat, eul);
					compatible_eul(eul, td->ext->irot);
					
					/* correct back for delta rot */
					VecSubf(eul, eul, td->ext->drot);
					/* and apply */
					VECCOPY(td->ext->rot, eul);
				}
			}
		}
	}

	recalcData(t);

	headerprint(str);

	force_draw(0);

	helpline (t->center);

	return 1;
}

/* ************************** TRANSLATION *************************** */
	
void initTranslation(TransInfo *t) 
{
	
	t->num.idx_max = 2;
	t->transform = Translation;
	
	/* initgrabz() defines a factor for perspective depth correction, used in window_to_3d() */
	if (G.obedit) {
		float vec[3];
		
		VECCOPY(vec, t->center);
		Mat4MulVecfl(G.obedit->obmat, vec);
		initgrabz(vec[0], vec[1], vec[2]);
	}
	else initgrabz(t->center[0], t->center[1], t->center[2]); 
}

int Translation(TransInfo *t, short mval[2]) 
{
	float vec[3], tvec[3];
	int i;
	char str[70];
	TransData *td = t->data;

	window_to_3d(vec, (short)(mval[0] - t->imval[0]), (short)(mval[1] - t->imval[1]));

	if (t->con.applyVec) {
		t->con.applyVec(t, NULL, vec, tvec);
		VECCOPY(vec, tvec);
	}

	apply_grid1(vec, t->num.idx_max, 1.0f);

	applyNumInput(&t->num, vec);

	/* header print for NumInput */
	if (hasNumInput(&t->num)) {
		char c[60];

		outputNumInput(&(t->num), c);

		sprintf(str, "Dx: %s   Dy: %s  Dz: %s %s", &c[0], &c[20], &c[40], t->proptext);
	}
	else {
		/* default header print */
		sprintf(str, "Dx: %.4f   Dy: %.4f  Dz: %.4f %s", vec[0], vec[1], vec[2], t->proptext);
	}


	for(i = 0 ; i < t->total; i++, td++) {
		if (td->flag & TD_NOACTION)
			continue;

		if (t->con.applyVec) {
			t->con.applyVec(t, td, vec, tvec);
		}
		else {
			VECCOPY(tvec, vec);
		}

		Mat3MulVecfl(td->smtx, tvec);
		VecMulf(tvec, td->factor);
		
		/* transdata ipokey */
		if(td->tdi) {
			TransDataIpokey *tdi= td->tdi;
			add_tdi_poin(tdi->locx, tdi->oldloc, tvec[0]);
			add_tdi_poin(tdi->locy, tdi->oldloc+1, tvec[1]);
			add_tdi_poin(tdi->locz, tdi->oldloc+2, tvec[2]);
		}
		else VecAddf(td->loc, td->iloc, tvec);
	}


	recalcData(t);

	headerprint(str);

	force_draw(0);

	return 1;
}
