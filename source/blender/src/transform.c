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

#include "BIF_screen.h"
#include "BIF_space.h"
#include "BIF_editview.h"
#include "BIF_resources.h"
#include "BIF_mywindow.h"
#include "BIF_gl.h"
#include "BIF_editlattice.h"
#include "BIF_editarmature.h"
#include "BIF_editmesh.h"

#include "BKE_global.h"
#include "BKE_object.h"
#include "BKE_utildefines.h"
#include "BKE_lattice.h"
#include "BKE_armature.h"
#include "BKE_curve.h"
#include "BKE_displist.h"

#include "BSE_view.h"
#include "BSE_edit.h"

#include "BLI_arithb.h"
#include "BLI_editVert.h"

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
TransInfo trans;
int	LastMode = TRANSLATION;

/* ************************** Functions *************************** */

/* ************************** CONVERSIONS ************************* */

int allocTransData()
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
	
	trans.total = count;
	trans.data= MEM_mallocN(trans.total*sizeof(TransData), "TransObData(EditMode)");
	return count;
}

void createTransArmatureVerts()
{
	EditBone *ebo;
	TransData *tv;
	float mtx[3][3], smtx[3][3];

	trans.total = 0;
	for (ebo=G.edbo.first;ebo;ebo=ebo->next){
		if (ebo->flag & BONE_TIPSEL){
			trans.total++;
		}
		if (ebo->flag & BONE_ROOTSEL){
			trans.total++;
		}
	}

	if (!trans.total) return;
	
	Mat3CpyMat4(mtx, G.obedit->obmat);
	Mat3Inv(smtx, mtx);

	tv = trans.data = MEM_mallocN(trans.total*sizeof(TransData), "TransEditBone");
	
	Mat3CpyMat4(mtx, G.obedit->obmat);
	Mat3Inv(smtx, mtx);
	
	for (ebo=G.edbo.first;ebo;ebo=ebo->next){
		if (ebo->flag & BONE_TIPSEL){
			VECCOPY (tv->iloc, ebo->tail);
			tv->loc= ebo->tail;
			tv->flag= TD_SELECTED;

			Mat3CpyMat3(tv->smtx, smtx);
			Mat3CpyMat3(tv->mtx, mtx);

			tv->size = NULL;
			tv->rot = NULL;

			tv->dist = 0.0f;
			
			tv++;
		}
		if (ebo->flag & BONE_ROOTSEL){
			VECCOPY (tv->iloc, ebo->head);
			tv->loc= ebo->head;
			tv->flag= TD_SELECTED;

			Mat3CpyMat3(tv->smtx, smtx);
			Mat3CpyMat3(tv->mtx, mtx);

			tv->size = NULL;
			tv->rot = NULL;

			tv->dist = 0.0f;
		
			tv++;
		}
			
	}
}

void createTransMBallVerts()
{
 	MetaElem *ml;
	TransData *tv;
	int count;
	float mtx[3][3], smtx[3][3];

	count = allocTransData();
	if (!count) return;

	Mat3CpyMat4(mtx, G.obedit->obmat);
	Mat3Inv(smtx, mtx);
	
	tv = trans.data;
	ml= editelems.first;
	while(ml) {
		if(ml->flag & SELECT) {
			tv->loc= &ml->x;
			VECCOPY(tv->iloc, tv->loc);
			VECCOPY(tv->center, tv->loc);
			tv->flag= TD_SELECTED;

			Mat3CpyMat3(tv->smtx, smtx);
			Mat3CpyMat3(tv->mtx, mtx);

			tv->size = &ml->expx;
			tv->isize[0] = ml->expx;
			tv->isize[1] = ml->expy;
			tv->isize[2] = ml->expz;

			tv->rot = NULL;

			tv->dist = 0.0f;

			tv++;
		}
		ml= ml->next;
	}
} 

void createTransCurveVerts()
{
	TransData *tv = NULL;
	int count=0;
	int mode = 0; /*This used for. . .what?*/
  	Nurb *nu;
	BezTriple *bezt;
	BPoint *bp;
	int a;
	//int proptrans= 0;
	float mtx[3][3], smtx[3][3];

	count = allocTransData();
	if (!count) return;

	Mat3CpyMat4(mtx, G.obedit->obmat);
	Mat3Inv(smtx, mtx);

	tv = trans.data;
	nu= editNurb.first;
	while(nu) {
		if((nu->type & 7)==CU_BEZIER) {
			a= nu->pntsu;
			bezt= nu->bezt;
			while(a--) {
				if(bezt->hide==0) {
					if(mode==1 || (bezt->f1 & 1)) {
						VECCOPY(tv->iloc, bezt->vec[0]);
						tv->loc= bezt->vec[0];
						VECCOPY(tv->center, tv->loc);
						tv->flag= TD_SELECTED;
						tv->rot = NULL;
						tv->size = NULL;

						Mat3CpyMat3(tv->smtx, smtx);
						Mat3CpyMat3(tv->mtx, mtx);

						tv->dist = 0.0f;
			
						tv++;
						count++;
					}
					if(mode==1 || (bezt->f2 & 1)) {
						VECCOPY(tv->iloc, bezt->vec[1]);
						tv->loc= bezt->vec[1];
						VECCOPY(tv->center, tv->loc);
						tv->flag= TD_SELECTED;
						tv->rot = NULL;
						tv->size = NULL;

						Mat3CpyMat3(tv->smtx, smtx);
						Mat3CpyMat3(tv->mtx, mtx);

						tv->dist = 0.0f;
			
						tv++;
						count++;
					}
					if(mode==1 || (bezt->f3 & 1)) {
						VECCOPY(tv->iloc, bezt->vec[2]);
						tv->loc= bezt->vec[2];
						VECCOPY(tv->center, tv->loc);
						tv->flag= TD_SELECTED;
						tv->rot = NULL;
						tv->size = NULL;

						Mat3CpyMat3(tv->smtx, smtx);
						Mat3CpyMat3(tv->mtx, mtx);

						tv->dist = 0.0f;
			
						tv++;
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
						VECCOPY(tv->iloc, bp->vec);
						tv->loc= bp->vec;
						VECCOPY(tv->center, tv->loc);
						tv->flag= TD_SELECTED;
						tv->rot = NULL;
						tv->size = NULL;

						Mat3CpyMat3(tv->smtx, smtx);
						Mat3CpyMat3(tv->mtx, mtx);

						tv->dist = 0.0f;
			
						tv++;
						count++;
					}
				}
				bp++;
			}
		}
		nu= nu->next;
	}
}

void createTransLatticeVerts()
{
	TransData *tv = NULL;
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

	tv = trans.data;
	bp= editLatt->def;
	a= editLatt->pntsu*editLatt->pntsv*editLatt->pntsw;
	while(a--) {
		if(mode==1 || (bp->f1 & 1)) {
			if(bp->hide==0) {
				VECCOPY(tv->iloc, bp->vec);
				tv->loc= bp->vec;
				VECCOPY(tv->center, tv->loc);
				tv->flag= TD_SELECTED;

				Mat3CpyMat3(tv->smtx, smtx);
				Mat3CpyMat3(tv->mtx, mtx);

				tv->size = NULL;
				tv->rot = NULL;

				tv->dist = 0.0f;

				tv++;
				count++;
			}
		}
		bp++;
	}
} 

void VertsToTransData(TransData *tob, EditVert *eve)
{
	tob->flag = 0;
	tob->loc = eve->co;
	VECCOPY(tob->center, tob->loc);
	VECCOPY(tob->iloc, tob->loc);
	tob->rot = NULL; 
	tob->size = NULL;
	tob->quat = NULL;
}

void createTransEditVerts()
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
				trans.total++;
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
				trans.total++;
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
				trans.total++;
	}
	
	totsel = trans.total;
	/* proportional edit exception... */
	if((G.f & G_PROPORTIONAL) && trans.total) {
		for(eve= em->verts.first; eve; eve= eve->next) {
			if(eve->h==0 && (!(eve->f1 & SELECT))) {
				eve->f1 = 2;
				trans.total++;
			}
		}
	}
	
	/* and now make transverts */
	
	if (!trans.total) return;
	
	Mat3CpyMat4(mtx, G.obedit->obmat);
	Mat3Inv(smtx, mtx);

	tob = trans.data = MEM_mallocN(trans.total*sizeof(TransData), "TransEditVert");

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

				td = trans.data;
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

void ObjectToTransData(TransData *tob, Object *ob) {
	float totmat[3][3], obinv[3][3], obmtx[3][3];
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

	tob->ob = ob;

	tob->loc = ob->loc;
	VECCOPY(tob->iloc, tob->loc);
	
	tob->rot = ob->rot;
	VECCOPY(tob->irot, ob->rot);
	
	tob->size = ob->size;
	VECCOPY(tob->isize, ob->size);

	VECCOPY(tob->center, ob->obmat[3]);

	Mat3CpyMat4(tob->mtx, ob->obmat);

	object_to_mat3(ob, obmtx);

	Mat3CpyMat4(totmat, ob->obmat);
	Mat3Inv(obinv, totmat);
	Mat3MulMat3(tob->smtx, obmtx, obinv);
}

void createTransObject()
{
	TransData *tob = NULL;
	Object *ob;
	Base *base;
	int totsel = 0, i;

	/* count */	
	base= FIRSTBASE;
	while(base) {
		if TESTBASELIB(base) {
			trans.total++;
			totsel++;
		}
		else if (G.f & G_PROPORTIONAL) {
			ob= base->object;
			if (G.vd->lay & ob->lay) {
				trans.total++;
			}
		}
		base= base->next;
	}

	if(!trans.total)
		return;

	tob = trans.data = MEM_mallocN(trans.total*sizeof(TransData), "TransOb");

	base= FIRSTBASE;
	while(base) {
		if TESTBASELIB(base) {
			ob= base->object;
			
			tob->flag= TD_SELECTED;

			ObjectToTransData(tob, ob);

			tob->dist = 0.0f;

			tob++;
		}
		base= base->next;
	}


	/* PROPORTIONAL*/
	if (G.f & G_PROPORTIONAL) {
		base= FIRSTBASE;
		while(base) {
			if (!TESTBASELIB(base)) {
				TransData *td;
				int i;
				float dist;

				ob= base->object;
				
				if (G.vd->lay & ob->lay) {
					tob->flag = 0;

					tob->ob = ob;
				
					ObjectToTransData(tob, ob);

					tob->dist = -1;

					td = trans.data;
					for (i = 0; i < totsel; i++, td++) {
						dist = VecLenf(tob->center, td->center);
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
			base= base->next;
		}
	}

/*
	KICK OUT CHILDS OF OBJECTS THAT ARE BEING TRANSFORMED
	SINCE TRANSFORMATION IS ALREADY APPLIED ON PARENT

	THERE MUST BE A BETTER WAY TO DO THIS
*/

	tob = trans.data;
	for (i = 0; i < trans.total; i++, tob++) {
		ob = tob->ob->parent;
		while (ob) {
			TransData *td;
			int j, found = 0;
			td = trans.data;
			for (j = 0; j < trans.total; j++, td++) {
				if (ob == td->ob) {
					found = 1;
					tob->flag |= TD_NOACTION;
					break;
				}
			}

			if (found) {
				break;
			}

			ob = ob->parent;
		}
	}

}

void createTransData() {
	if (G.obedit) {
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

void Transform(int mode) {
	int ret_val = 0;
	short pmval[2] = {0, 0}, mval[2], val;
	float MatI[3][3];
	unsigned short event;

	/*joeedh -> hopefully may be what makes the old transform() constant*/	
	areawinset(curarea->win);


	Mat3One(MatI);

	/* stupid PET initialisation code */
	/* START */
	if (trans.propsize == 0.0f) {
		trans.propsize = 1.0;
	}
	/* END */

	if (mode == REPEAT) {
		mode = LastMode;
	}
	else {
		LastMode = mode;
	}
	
	initTransModeFlags(&trans, mode);

	initTrans(&trans);

	createTransData();

	if (trans.total == 0)
		return;

	calculatePropRatio(&trans);
	calculateCenter(&trans);

	switch (mode) {
	case TRANSLATION:
		initTranslation(&trans);
		break;
	case ROTATION:
		initRotation(&trans);
		break;
	case RESIZE:
		initResize(&trans);
		break;
	case TOSPHERE:
		initToSphere(&trans);
		break;
	case SHEAR:
		initShear(&trans);
		break;
	}

	// Emptying event queue
	while( qtest() ) {
		event= extern_qread(&val);
	}

	trans.redraw = 1;

	while (ret_val == 0) {
		getmouseco_areawin(mval);
		if (mval[0] != pmval[0] || mval[1] != pmval[1]) {
			trans.redraw = 1;
		}
		if (trans.redraw) {
			pmval[0] = mval[0];
			pmval[1] = mval[1];

			if (trans.transform) {
				trans.transform(&trans, mval);
			}
			trans.redraw = 0;
		}
		while( qtest() ) {
			event= extern_qread(&val);

			if(val) {
				switch (event){
				case MIDDLEMOUSE:
					selectConstraint(&trans);
					trans.redraw = 1;
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
						applyTransObjects(&trans);
					else
						restoreTransObjects(&trans);
					break;
				case XKEY:
					if (G.qual == 0)
						setConstraint(&trans, MatI, (APPLYCON|CONAXIS0));
					else if (G.qual == LR_CTRLKEY)
						setConstraint(&trans, MatI, (APPLYCON|CONAXIS1|CONAXIS2));
					break;
				case YKEY:
					if (G.qual == 0)
						setConstraint(&trans, MatI, (APPLYCON|CONAXIS1));
					else if (G.qual == LR_CTRLKEY)
						setConstraint(&trans, MatI, (APPLYCON|CONAXIS0|CONAXIS2));
					break;
				case ZKEY:
					if (G.qual == 0)
						setConstraint(&trans, MatI, (APPLYCON|CONAXIS2));
					else if (G.qual == LR_CTRLKEY)
						setConstraint(&trans, MatI, (APPLYCON|CONAXIS0|CONAXIS1));
					break;
				case OKEY:
					if (G.qual==LR_SHIFTKEY) {
						extern int prop_mode;
						prop_mode = (prop_mode+1)%5;
						calculatePropRatio(&trans);
						trans.redraw= 1;
					}
					break;
				case WHEELDOWNMOUSE:
				case PADPLUSKEY:
					if(G.f & G_PROPORTIONAL) {
						trans.propsize*= 1.1f;
						calculatePropRatio(&trans);
						trans.redraw= 1;
					}
					break;
				case WHEELUPMOUSE:
				case PADMINUS:
					if(G.f & G_PROPORTIONAL) {
						trans.propsize*= 0.90909090f;
						calculatePropRatio(&trans);
						trans.redraw= 1;
					}
					break;
				}
				trans.redraw |= handleNumInput(&(trans.num), event);
				arrows_move_cursor(event);
			}
			else {
				switch (event){
				case MIDDLEMOUSE:
					chooseConstraint(&trans);
					trans.redraw = 1;
					break;
				case LEFTMOUSE:
				case RIGHTMOUSE:
					ret_val = TRANS_CONFIRM;
					break;
				}
			}
		}
	}

	if(ret_val == TRANS_CANCEL) {
		restoreTransObjects(&trans);
	}
	else {
		BIF_undo_push("Transform");
	}

	postTrans(&trans);
}

/* ************************** WRAP *************************** */

void initWrap(TransInfo *t) {
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


int Wrap(TransInfo *t, short mval[2]) {
	return 1;
}

/* ************************** SHEAR *************************** */

void initShear(TransInfo *t) {
	t->num.idx_max = 0;
	t->transform = Shear;
	t->fac = (float)(t->center2d[0] - t->imval[0]);
}

int Shear(TransInfo *t, short mval[2]) {
	float vec[3];
	float smat[3][3], tmat[3][3], totmat[3][3], omat[3][3], persmat[3][3], persinv[3][3];
	float value;
	int i;
	char str[50];
	TransData *td = t->data;

	Mat3CpyMat4(persmat, G.vd->viewmat);
	Mat3Inv(persinv, persmat);

	if (G.obedit) {
		Mat3CpyMat4(omat, G.obedit->obmat);
	}

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
			Mat3MulMat3(mat3, totmat, omat);
			Mat3MulMat3(tmat, td->smtx, mat3);
		}
		else {
			Mat3CpyMat3(tmat, totmat);
		}
		VecSubf(vec, td->iloc, t->center);

		Mat3MulVecfl(tmat, vec);

		VecAddf(vec, vec, t->center);
		VecSubf(vec, vec, td->iloc);

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

void initResize(TransInfo *t) {
	trans.fac = (float)sqrt( (float)
		(
			(trans.center2d[1] - trans.imval[1])*(trans.center2d[1] - trans.imval[1])
		+
			(trans.center2d[0] - trans.imval[0])*(trans.center2d[0] - trans.imval[0])
		) );

	t->num.idx_max = 2;
	t->transform = Resize;
}

int Resize(TransInfo *t, short mval[2]) {
	float vec[3];
	float size[3], tsize[3], mat[3][3], tmat[3][3], omat[3][3];
	float ratio;
	int i;
	char str[50];
	TransData *td = t->data;

	if (G.obedit) {
		Mat3CpyMat4(omat, G.obedit->obmat);
	}

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
		if (td->flag & TD_NOACTION)
			continue;

		if (G.obedit) {
			float smat[3][3];
			Mat3MulMat3(smat, td->smtx, mat);
			Mat3MulMat3(tmat, smat, omat);
		}
		else {
			Mat3CpyMat3(tmat, mat);
		}

		if (td->size) {
			// TEMPORARY NAIVE CODE
			td->size[0] = td->isize[0] + td->isize[0] * (size[0] - 1.0f) * td->factor;
			td->size[1] = td->isize[1] + td->isize[1] * (size[1] - 1.0f) * td->factor;
			td->size[2] = td->isize[2] + td->isize[2] * (size[2] - 1.0f) * td->factor;
		}
		VecSubf(vec, td->iloc, t->center);

		Mat3MulVecfl(tmat, vec);

		VecAddf(vec, vec, t->center);
		VecSubf(vec, vec, td->iloc);

		VecMulf(vec, td->factor);

		VecAddf(td->loc, td->iloc, vec);
	}

	recalcData(t);

	headerprint(str);

	force_draw(0);

	helpline (t->center);

	return 1;
}

/* ************************** TOSPHERE *************************** */

void initToSphere(TransInfo *t) {
	TransData *td = t->data;
	int i;

	// Calculate average radius
	for(i = 0 ; i < t->total; i++, td++) {
		t->val += VecLenf(t->center, td->iloc);
	}

	t->val /= (float)t->total;

	trans.fac = (float)sqrt( (float)
		(
			(trans.center2d[1] - trans.imval[1])*(trans.center2d[1] - trans.imval[1])
		+
			(trans.center2d[0] - trans.imval[0])*(trans.center2d[0] - trans.imval[0])
		) );

	t->num.idx_max = 0;
	t->transform = ToSphere;
}



int ToSphere(TransInfo *t, short mval[2]) {
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

void initRotation(TransInfo *t) {
	t->num.idx_max = 0;
	t->fac = 0;
	t->transform = Rotation;
}

int Rotation(TransInfo *t, short mval[2]) {
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

			Mat3MulMat3(totmat, mat, td->mtx);
			Mat3MulMat3(fmat, td->smtx, totmat);
			Mat3ToEul(fmat, eul);
			VECCOPY(td->rot, eul);

		}
	}

	recalcData(t);

	headerprint(str);

	force_draw(0);

	helpline (t->center);

	return 1;
}

/* ************************** TRANSLATION *************************** */
	
void initTranslation(TransInfo *t) {
	// int x, y;
	t->num.idx_max = 2;
	t->transform = Translation;
	
	//x = G.vd->area->v1->vec.x;
	//y = G.vd->area->v1->vec.y + (G.vd->area->headwin?28:1);
	//warp_pointer(t->center2d[0] + x, t->center2d[1] + y);

	//t->imval[0] = t->center2d[0];
	//t->imval[1] = t->center2d[1];
}

int Translation(TransInfo *t, short mval[2]) {
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

		VecAddf(td->loc, td->iloc, tvec);
	}


	recalcData(t);

	headerprint(str);

	force_draw(0);

	return 1;
}