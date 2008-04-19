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

#include <string.h>
#include <math.h>

#include "MEM_guardedalloc.h"

#include "DNA_action_types.h"
#include "DNA_armature_types.h"
#include "DNA_constraint_types.h"
#include "DNA_curve_types.h"
#include "DNA_lattice_types.h"
#include "DNA_mesh_types.h"
#include "DNA_modifier_types.h"
#include "DNA_object_types.h"
#include "DNA_object_force.h"
#include "DNA_particle_types.h"
#include "DNA_screen_types.h"
#include "DNA_space_types.h"
#include "DNA_scene_types.h"
#include "DNA_userdef_types.h"
#include "DNA_view3d_types.h"

#include "BIF_screen.h"
#include "BIF_resources.h"
#include "BIF_mywindow.h"
#include "BIF_gl.h"
#include "BIF_editaction.h"
#include "BIF_editarmature.h"
#include "BIF_editmesh.h"
#include "BIF_editnla.h"
#include "BIF_editsima.h"
#include "BIF_editparticle.h"
#include "BIF_meshtools.h"
#include "BIF_retopo.h"

#include "BSE_editipo.h"
#include "BSE_editipo_types.h"

#ifdef WITH_VERSE
#include "BIF_verse.h"
#endif

#include "BKE_action.h"
#include "BKE_anim.h"
#include "BKE_armature.h"
#include "BKE_cloth.h"
#include "BKE_curve.h"
#include "BKE_depsgraph.h"
#include "BKE_displist.h"
#include "BKE_depsgraph.h"
#include "BKE_global.h"
#include "BKE_group.h"
#include "BKE_ipo.h"
#include "BKE_lattice.h"
#include "BKE_key.h"
#include "BKE_mesh.h"
#include "BKE_modifier.h"
#include "BKE_object.h"
#include "BKE_utildefines.h"

#ifdef WITH_VERSE
#include "BKE_verse.h"
#endif

#include "BSE_view.h"
#include "BSE_editaction_types.h"
#include "BDR_unwrapper.h"

#include "BLI_arithb.h"
#include "BLI_blenlib.h"
#include "BLI_editVert.h"
#include "BLI_rand.h"

#include "blendef.h"

#include "mydevice.h"

#include "transform.h"

extern ListBase editNurb;
extern ListBase editelems;

extern TransInfo Trans;	/* From transform.c */

/* ************************** Functions *************************** */


void getViewVector(float coord[3], float vec[3])
{
	TransInfo *t = BIF_GetTransInfo();

	if (t->persp != V3D_ORTHO)
	{
		float p1[4], p2[4];

		VECCOPY(p1, coord);
		p1[3] = 1.0f;
		VECCOPY(p2, p1);
		p2[3] = 1.0f;
		Mat4MulVec4fl(t->viewmat, p2);

		p2[0] = 2.0f * p2[0];
		p2[1] = 2.0f * p2[1];
		p2[2] = 2.0f * p2[2];

		Mat4MulVec4fl(t->viewinv, p2);

		VecSubf(vec, p1, p2);
	}
	else {
		VECCOPY(vec, t->viewinv[2]);
	}
	Normalize(vec);
}

/* ************************** GENERICS **************************** */

static void clipMirrorModifier(TransInfo *t, Object *ob)
{
	ModifierData *md= ob->modifiers.first;
	float tolerance[3] = {0.0f, 0.0f, 0.0f};
	int axis = 0;

	for (; md; md=md->next) {
		if (md->type==eModifierType_Mirror) {
			MirrorModifierData *mmd = (MirrorModifierData*) md;	
		
			if(mmd->flag & MOD_MIR_CLIPPING) {
				axis = 0;
				if(mmd->flag & MOD_MIR_AXIS_X) {
					axis |= 1;
					tolerance[0] = mmd->tolerance;
				}
				if(mmd->flag & MOD_MIR_AXIS_Y) {
					axis |= 2;
					tolerance[1] = mmd->tolerance;
				}
				if(mmd->flag & MOD_MIR_AXIS_Z) {
					axis |= 4;
					tolerance[2] = mmd->tolerance;
				}
				if (axis) {
					float mtx[4][4], imtx[4][4];
					int i;
					TransData *td = t->data;
		
					if (mmd->mirror_ob) {
						float obinv[4][4];

						Mat4Invert(obinv, mmd->mirror_ob->obmat);
						Mat4MulMat4(mtx, ob->obmat, obinv);
						Mat4Invert(imtx, mtx);
					}

					for(i = 0 ; i < t->total; i++, td++) {
						int clip;
						float loc[3], iloc[3];

						if (td->flag & TD_NOACTION)
							break;
						if (td->loc==NULL)
							break;
							
						if (td->flag & TD_SKIP)
							continue;
			
						VecCopyf(loc,  td->loc);
						VecCopyf(iloc, td->iloc);

						if (mmd->mirror_ob) {
							VecMat4MulVecfl(loc, mtx, loc);
							VecMat4MulVecfl(iloc, mtx, iloc);
						}

						clip = 0;
						if(axis & 1) {
							if(fabs(iloc[0])<=tolerance[0] || 
							   loc[0]*iloc[0]<0.0f) {
								loc[0]= 0.0f;
								clip = 1;
							}
						}
			
						if(axis & 2) {
							if(fabs(iloc[1])<=tolerance[1] || 
							   loc[1]*iloc[1]<0.0f) {
								loc[1]= 0.0f;
								clip = 1;
							}
						}
						if(axis & 4) {
							if(fabs(iloc[2])<=tolerance[2] || 
							   loc[2]*iloc[2]<0.0f) {
								loc[2]= 0.0f;
								clip = 1;
							}
						}
						if (clip) {
							if (mmd->mirror_ob) {
								VecMat4MulVecfl(loc, imtx, loc);
							}
							VecCopyf(td->loc, loc);
						}
					}
				}

			}
		}
	}
}

/* assumes G.obedit set to mesh object */
static void editmesh_apply_to_mirror(TransInfo *t)
{
	TransData *td = t->data;
	EditVert *eve;
	int i;
	
	for(i = 0 ; i < t->total; i++, td++) {
		if (td->flag & TD_NOACTION)
			break;
		if (td->loc==NULL)
			break;
		if (td->flag & TD_SKIP)
			continue;
		
		eve = td->tdmir;
		if(eve) {
			eve->co[0]= -td->loc[0];
			eve->co[1]= td->loc[1];
			eve->co[2]= td->loc[2];
		}		
	}		
}

/* called for updating while transform acts, once per redraw */
void recalcData(TransInfo *t)
{
	Base *base;
#ifdef WITH_VERSE
	struct TransData *td;
#endif
	
	if (t->spacetype == SPACE_ACTION) {
		Object *ob= OBACT;
		void *data;
		short context;
		
		/* determine what type of data we are operating on */
		data = get_action_context(&context);
		if (data == NULL) return;
		
		if (G.saction->lock) {
			if (context == ACTCONT_ACTION) {
				if(ob) {
					ob->ctime= -1234567.0f;
					if(ob->pose || ob_get_key(ob))
						DAG_object_flush_update(G.scene, ob, OB_RECALC);
					else
						DAG_object_flush_update(G.scene, ob, OB_RECALC_OB);
				}
			}
			else if (context == ACTCONT_SHAPEKEY) {
				DAG_object_flush_update(G.scene, OBACT, OB_RECALC_OB|OB_RECALC_DATA);
			}
		}
	}	
	else if (t->spacetype == SPACE_NLA) {
		if (G.snla->lock) {
			for (base=G.scene->base.first; base; base=base->next) {
				if (base->flag & BA_HAS_RECALC_OB)
					base->object->recalc |= OB_RECALC_OB;
				if (base->flag & BA_HAS_RECALC_DATA)
					base->object->recalc |= OB_RECALC_DATA;
				
				if (base->object->recalc) 
					base->object->ctime= -1234567.0f;	// eveil! 
			}
			
			DAG_scene_flush_update(G.scene, screen_view3d_layers(), 0);
		}
	}
	else if (t->spacetype == SPACE_IPO) {
		EditIpo *ei;
		int dosort = 0;
		int a;
		
		/* do the flush first */
		flushTransIpoData(t);
		
		/* now test if there is a need to re-sort */
		ei= G.sipo->editipo;
		for (a=0; a<G.sipo->totipo; a++, ei++) {
			if (ISPOIN(ei, flag & IPO_VISIBLE, icu)) {
				
				/* watch it: if the time is wrong: do not correct handles */
				if (test_time_ipocurve(ei->icu)) {
					dosort++;
				} else {
					calchandles_ipocurve(ei->icu);
				}
			}
		}
		
		/* do resort and other updates? */
		if (dosort) remake_ipo_transdata(t);
		if (G.sipo->showkey) update_ipokey_val();
		
		calc_ipo(G.sipo->ipo, (float)CFRA);
		
		/* update realtime - not working? */
		if (G.sipo->lock) {
			if (G.sipo->blocktype==ID_MA || G.sipo->blocktype==ID_TE) {
				do_ipo(G.sipo->ipo);
			}
			else if(G.sipo->blocktype==ID_CA) {
				do_ipo(G.sipo->ipo);
			}
			else if(G.sipo->blocktype==ID_KE) {
				Object *ob= OBACT;
				if(ob) {
					ob->shapeflag &= ~OB_SHAPE_TEMPLOCK;
					DAG_object_flush_update(G.scene, ob, OB_RECALC_DATA);
				}
			}
			else if(G.sipo->blocktype==ID_PO) {
				Object *ob= OBACT;
				if(ob && ob->pose) {
					DAG_object_flush_update(G.scene, ob, OB_RECALC_DATA);
				}
			}
			else if(G.sipo->blocktype==ID_OB) {
				Base *base= FIRSTBASE;
				
				while(base) {
					if(base->object->ipo==G.sipo->ipo) {
						do_ob_ipo(base->object);
						base->object->recalc |= OB_RECALC_OB;
					}
					base= base->next;
				}
				DAG_scene_flush_update(G.scene, screen_view3d_layers(), 0);
			}
		}
	}
	else if (G.obedit) {
		if (G.obedit->type == OB_MESH) {
			if(t->spacetype==SPACE_IMAGE) {
				flushTransUVs(t);
				if (G.sima->flag & SI_LIVE_UNWRAP)
					unwrap_lscm_live_re_solve();
			} else {
				/* mirror modifier clipping? */
				if(t->state != TRANS_CANCEL) {
					if ((G.qual & LR_CTRLKEY)==0) {
						/* Only retopo if not snapping, Note, this is the only case of G.qual being used, but we have no T_SHIFT_MOD - Campbell */
						retopo_do_all();
					}
					clipMirrorModifier(t, G.obedit);
				}
				if((t->context & CTX_NO_MIRROR) == 0 && (G.scene->toolsettings->editbutflag & B_MESH_X_MIRROR))
					editmesh_apply_to_mirror(t);
				
				DAG_object_flush_update(G.scene, G.obedit, OB_RECALC_DATA);  /* sets recalc flags */
				
				recalc_editnormals();
			}
		}
		else if ELEM(G.obedit->type, OB_CURVE, OB_SURF) {
			Nurb *nu= editNurb.first;
			DAG_object_flush_update(G.scene, G.obedit, OB_RECALC_DATA);  /* sets recalc flags */
			
			if (t->state == TRANS_CANCEL) {
				while(nu) {
					calchandlesNurb(nu); /* Cant do testhandlesNurb here, it messes up the h1 and h2 flags */
					nu= nu->next;
				}
			} else {
				/* Normal updating */
				while(nu) {
					test2DNurb(nu);
					calchandlesNurb(nu);
					nu= nu->next;
				}
				retopo_do_all();
			}
		}
		else if(G.obedit->type==OB_ARMATURE){   /* no recalc flag, does pose */
			bArmature *arm= G.obedit->data;
			EditBone *ebo;
			
			/* Ensure all bones are correctly adjusted */
			for (ebo=G.edbo.first; ebo; ebo=ebo->next){
				
				if ((ebo->flag & BONE_CONNECTED) && ebo->parent){
					/* If this bone has a parent tip that has been moved */
					if (ebo->parent->flag & BONE_TIPSEL){
						VECCOPY (ebo->head, ebo->parent->tail);
						if(t->mode==TFM_BONE_ENVELOPE) ebo->rad_head= ebo->parent->rad_tail;
					}
					/* If this bone has a parent tip that has NOT been moved */
					else{
						VECCOPY (ebo->parent->tail, ebo->head);
						if(t->mode==TFM_BONE_ENVELOPE) ebo->parent->rad_tail= ebo->rad_head;
					}
				}
				
				/* on extrude bones, oldlength==0.0f, so we scale radius of points */
				ebo->length= VecLenf(ebo->head, ebo->tail);
				if(ebo->oldlength==0.0f) {
					ebo->rad_head= 0.25f*ebo->length;
					ebo->rad_tail= 0.10f*ebo->length;
					ebo->dist= 0.25f*ebo->length;
					if(ebo->parent) {
						if(ebo->rad_head > ebo->parent->rad_tail)
							ebo->rad_head= ebo->parent->rad_tail;
					}
				}
				else if(t->mode!=TFM_BONE_ENVELOPE) {
					/* if bones change length, lets do that for the deform distance as well */
					ebo->dist*= ebo->length/ebo->oldlength;
					ebo->rad_head*= ebo->length/ebo->oldlength;
					ebo->rad_tail*= ebo->length/ebo->oldlength;
					ebo->oldlength= ebo->length;
				}
			}
			if(arm->flag & ARM_MIRROR_EDIT) 
				transform_armature_mirror_update();
			
		}
		else if(G.obedit->type==OB_LATTICE) {
			DAG_object_flush_update(G.scene, G.obedit, OB_RECALC_DATA);  /* sets recalc flags */
			
			if(editLatt->flag & LT_OUTSIDE) outside_lattice(editLatt);
		}
		else {
			DAG_object_flush_update(G.scene, G.obedit, OB_RECALC_DATA);  /* sets recalc flags */
		}
	}
	else if( (t->flag & T_POSE) && t->poseobj) {
		Object *ob= t->poseobj;
		bArmature *arm= ob->data;
		
		/* old optimize trick... this enforces to bypass the depgraph */
		if (!(arm->flag & ARM_DELAYDEFORM)) {
			DAG_object_flush_update(G.scene, ob, OB_RECALC_DATA);  /* sets recalc flags */
		}
		else
			where_is_pose(ob);
	}
	else if(G.f & G_PARTICLEEDIT) {
		flushTransParticles(t);
	}
	else {
		for(base= FIRSTBASE; base; base= base->next) {
			Object *ob= base->object;
			
			/* this flag is from depgraph, was stored in initialize phase, handled in drawview.c */
			if(base->flag & BA_HAS_RECALC_OB)
				ob->recalc |= OB_RECALC_OB;
			if(base->flag & BA_HAS_RECALC_DATA)
				ob->recalc |= OB_RECALC_DATA;

			/* thanks to ob->ctime usage, ipos are not called in where_is_object,
			   unless we edit ipokeys */
			if(base->flag & BA_DO_IPO) {
				if(ob->ipo) {
					IpoCurve *icu;
					
					ob->ctime= -1234567.0;
					
					icu= ob->ipo->curve.first;
					while(icu) {
						calchandles_ipocurve(icu);
						icu= icu->next;
					}
				}				
			}
			
			/* proxy exception */
			if(ob->proxy)
				ob->proxy->recalc |= ob->recalc;
			if(ob->proxy_group)
				group_tag_recalc(ob->proxy_group->dup_group);
		} 
	}

#ifdef WITH_VERSE
	for (td = t->data; td < t->data + t->total; td++) {
		if(td->flag & TD_VERSE_VERT) {
			if(td->verse)
				send_versevert_pos((VerseVert*)td->verse);
		}
		else if(td->flag & TD_VERSE_OBJECT)
			if(td->verse) b_verse_send_transformation((Object*)td->verse);
	}
#endif
	
	/* update shaded drawmode while transform */
	if(t->spacetype==SPACE_VIEW3D && G.vd->drawtype == OB_SHADED)
		reshadeall_displist();
}

void initTransModeFlags(TransInfo *t, int mode) 
{
	t->mode = mode;
	t->num.flag = 0;

	/* REMOVING RESTRICTIONS FLAGS */
	t->flag &= ~T_ALL_RESTRICTIONS;
	
	switch (mode) {
	case TFM_RESIZE:
		t->flag |= T_NULL_ONE;
		t->num.flag |= NUM_NULL_ONE;
		t->num.flag |= NUM_AFFECT_ALL;
		if (!G.obedit) {
			t->flag |= T_NO_ZERO;
			t->num.flag |= NUM_NO_ZERO;
		}
		break;
	case TFM_TOSPHERE:
		t->num.flag |= NUM_NULL_ONE;
		t->num.flag |= NUM_NO_NEGATIVE;
		t->flag |= T_NO_CONSTRAINT;
		break;
	case TFM_SHEAR:
	case TFM_CREASE:
	case TFM_BONE_ENVELOPE:
	case TFM_CURVE_SHRINKFATTEN:
	case TFM_BONE_ROLL:
		t->flag |= T_NO_CONSTRAINT;
		break;
	}
}

void drawLine(float *center, float *dir, char axis, short options)
{
	extern void make_axis_color(char *col, char *col2, char axis);	// drawview.c
	float v1[3], v2[3], v3[3];
	char col[3], col2[3];
	
	//if(G.obedit) mymultmatrix(G.obedit->obmat);	// sets opengl viewing

	VecCopyf(v3, dir);
	VecMulf(v3, G.vd->far);
	
	VecSubf(v2, center, v3);
	VecAddf(v1, center, v3);

	if (options & DRAWLIGHT) {
		col[0] = col[1] = col[2] = 220;
	}
	else {
		BIF_GetThemeColor3ubv(TH_GRID, col);
	}
	make_axis_color(col, col2, axis);
	glColor3ubv((GLubyte *)col2);

	setlinestyle(0);
	glBegin(GL_LINE_STRIP); 
		glVertex3fv(v1); 
		glVertex3fv(v2); 
	glEnd();
	
	myloadmatrix(G.vd->viewmat);
}

void initTrans (TransInfo *t)
{
	
	/* moving: is shown in drawobject() (transform color) */
	if(G.obedit || (t->flag & T_POSE) ) G.moving= G_TRANSFORM_EDIT;
	else if(G.f & G_PARTICLEEDIT) G.moving= G_TRANSFORM_PARTICLE;
	else G.moving= G_TRANSFORM_OBJ;

	t->data = NULL;
	t->ext = NULL;

	t->flag = 0;

	/* setting PET flag */
	if ((t->context & CTX_NO_PET) == 0 && (G.scene->proportional)) {
		t->flag |= T_PROP_EDIT;
		if(G.scene->proportional==2) t->flag |= T_PROP_CONNECTED;	// yes i know, has to become define
	}

	getmouseco_areawin(t->imval);
	t->con.imval[0] = t->imval[0];
	t->con.imval[1] = t->imval[1];

	t->transform		= NULL;
	t->handleEvent		= NULL;

	t->total			=
		t->num.idx		=
		t->num.idx_max	=
		t->num.ctrl[0]	= 
		t->num.ctrl[1]	= 
		t->num.ctrl[2]	= 0;

	t->val = 0.0f;

	t->num.val[0]		= 
		t->num.val[1]	= 
		t->num.val[2]	= 0.0f;

	t->vec[0]			=
		t->vec[1]		=
		t->vec[2]		= 0.0f;
	
	t->center[0]		=
		t->center[1]	=
		t->center[2]	= 0.0f;
	
	Mat3One(t->mat);
	
	t->spacetype = curarea->spacetype;
	if(t->spacetype==SPACE_VIEW3D) {
		if(G.vd->flag & V3D_ALIGN) t->flag |= T_V3D_ALIGN;
		t->around = G.vd->around;
	} else if(t->spacetype==SPACE_IMAGE) {
		t->around = G.v2d->around;
	}
	else
		t->around = V3D_CENTER;

	setTransformViewMatrices(t);
	initNDofInput(&(t->ndof));
}

/* Here I would suggest only TransInfo related issues, like free data & reset vars. Not redraws */
void postTrans (TransInfo *t) 
{
	TransData *td;

	G.moving = 0; // Set moving flag off (display as usual)
#ifdef WITH_VERSE

	for (td = t->data; td < t->data + t->total; td++) {
		if(td->flag & TD_VERSE_VERT) {
			if(td->verse) send_versevert_pos((VerseVert*)td->verse);
		}
		else if(td->flag & TD_VERSE_OBJECT) {
			if(td->verse) {
				struct VNode *vnode;
				vnode = (VNode*)((Object*)td->verse)->vnode;
				((VObjectData*)vnode->data)->flag |= POS_SEND_READY;
				((VObjectData*)vnode->data)->flag |= ROT_SEND_READY;
				((VObjectData*)vnode->data)->flag |= SCALE_SEND_READY;
				b_verse_send_transformation((Object*)td->verse);
			}
		}
	}
#endif

	stopConstraint(t);
	
	/* postTrans can be called when nothing is selected, so data is NULL already */
	if (t->data) {
		int a;

		/* since ipokeys are optional on objects, we mallocced them per trans-data */
		for(a=0, td= t->data; a<t->total; a++, td++) {
			if(td->tdi) MEM_freeN(td->tdi);
			if (td->flag & TD_BEZTRIPLE) MEM_freeN(td->hdata); 
		}
		MEM_freeN(t->data);
	}

	if (t->ext) MEM_freeN(t->ext);
	if (t->data2d) {
		MEM_freeN(t->data2d);
		t->data2d= NULL;
	}

	if(t->spacetype==SPACE_IMAGE) {
		if (G.sima->flag & SI_LIVE_UNWRAP)
			unwrap_lscm_live_end(t->state == TRANS_CANCEL);
	}
}

void applyTransObjects(TransInfo *t)
{
	TransData *td;
	
	for (td = t->data; td < t->data + t->total; td++) {
		VECCOPY(td->iloc, td->loc);
		if (td->ext->rot) {
			VECCOPY(td->ext->irot, td->ext->rot);
		}
		if (td->ext->size) {
			VECCOPY(td->ext->isize, td->ext->size);
		}
	}	
	recalcData(t);
} 

/* helper for below */
static void restore_ipokey(float *poin, float *old)
{
	if(poin) {
		poin[0]= old[0];
		poin[-3]= old[3];
		poin[3]= old[6];
	}
}

static void restoreElement(TransData *td) {
	/* TransData for crease has no loc */
	if (td->loc) {
		VECCOPY(td->loc, td->iloc);
	}
	if (td->val) {
		*td->val = td->ival;
	}
	if (td->ext && (td->flag&TD_NO_EXT)==0) {
		if (td->ext->rot) {
			VECCOPY(td->ext->rot, td->ext->irot);
		}
		if (td->ext->size) {
			VECCOPY(td->ext->size, td->ext->isize);
		}
		if(td->flag & TD_USEQUAT) {
			if (td->ext->quat) {
				QUATCOPY(td->ext->quat, td->ext->iquat);
			}
		}
	}
	
	if (td->flag & TD_BEZTRIPLE) {
		*(td->hdata->h1) = td->hdata->ih1;
		*(td->hdata->h2) = td->hdata->ih2;
	}
	
	if(td->tdi) {
		TransDataIpokey *tdi= td->tdi;
		
		restore_ipokey(tdi->locx, tdi->oldloc);
		restore_ipokey(tdi->locy, tdi->oldloc+1);
		restore_ipokey(tdi->locz, tdi->oldloc+2);

		restore_ipokey(tdi->rotx, tdi->oldrot);
		restore_ipokey(tdi->roty, tdi->oldrot+1);
		restore_ipokey(tdi->rotz, tdi->oldrot+2);
		
		restore_ipokey(tdi->sizex, tdi->oldsize);
		restore_ipokey(tdi->sizey, tdi->oldsize+1);
		restore_ipokey(tdi->sizez, tdi->oldsize+2);
	}
}

void restoreTransObjects(TransInfo *t)
{
	TransData *td;
	
	for (td = t->data; td < t->data + t->total; td++) {
		restoreElement(td);
#ifdef WITH_VERSE
		/* position of vertexes and object transformation matrix is sent
		 * extra, becuase blender uses synchronous sending of vertexes
		 * position as well object trans. matrix and it isn't possible to
		 * send it in recalcData sometimes */
		if(td->flag & TD_VERSE_VERT) {
			if(td->verse) {
				((VerseVert*)td->verse)->flag |= VERT_POS_OBSOLETE;
			}
		}
		else if(td->flag & TD_VERSE_OBJECT)
			if(td->verse) {
				struct VNode *vnode;
				vnode = (VNode*)((Object*)td->verse)->vnode;
				((VObjectData*)vnode->data)->flag |= POS_SEND_READY;
				((VObjectData*)vnode->data)->flag |= ROT_SEND_READY;
				((VObjectData*)vnode->data)->flag |= SCALE_SEND_READY;
			}
#endif
	}	
	recalcData(t);
}

void calculateCenter2D(TransInfo *t)
{
	if (t->flag & (T_EDIT|T_POSE)) {
		Object *ob= G.obedit?G.obedit:t->poseobj;
		float vec[3];
		
		VECCOPY(vec, t->center);
		Mat4MulVecfl(ob->obmat, vec);
		projectIntView(t, vec, t->center2d);
	}
	else {
		projectIntView(t, t->center, t->center2d);
	}
}

void calculateCenterCursor(TransInfo *t)
{
	float *cursor;

	cursor = give_cursor();
	VECCOPY(t->center, cursor);

	/* If edit or pose mode, move cursor in local space */
	if (t->flag & (T_EDIT|T_POSE)) {
		Object *ob = G.obedit?G.obedit:t->poseobj;
		float mat[3][3], imat[3][3];
		
		VecSubf(t->center, t->center, ob->obmat[3]);
		Mat3CpyMat4(mat, ob->obmat);
		Mat3Inv(imat, mat);
		Mat3MulVecfl(imat, t->center);
	}
	
	calculateCenter2D(t);
}

void calculateCenterCursor2D(TransInfo *t)
{
	float aspx=1.0, aspy=1.0;
	
	if(t->spacetype==SPACE_IMAGE) /* only space supported right now but may change */
		transform_aspect_ratio_tface_uv(&aspx, &aspy);
	if (G.v2d) {
		t->center[0] = G.v2d->cursor[0] * aspx; 
		t->center[1] = G.v2d->cursor[1] * aspy; 
	}
	calculateCenter2D(t);
}

void calculateCenterMedian(TransInfo *t)
{
	float partial[3] = {0.0f, 0.0f, 0.0f};
	int i;
	
	for(i = 0; i < t->total; i++) {
		if (t->data[i].flag & TD_SELECTED) {
			if (!(t->data[i].flag & TD_NOCENTER))
				VecAddf(partial, partial, t->data[i].center);
		}
		else {
			/* 
			   All the selected elements are at the head of the array 
			   which means we can stop when it finds unselected data
			*/
			break;
		}
	}
	if(i)
		VecMulf(partial, 1.0f / i);
	VECCOPY(t->center, partial);

	calculateCenter2D(t);
}

void calculateCenterBound(TransInfo *t)
{
	float max[3];
	float min[3];
	int i;
	for(i = 0; i < t->total; i++) {
		if (i) {
			if (t->data[i].flag & TD_SELECTED) {
				if (!(t->data[i].flag & TD_NOCENTER))
					MinMax3(min, max, t->data[i].center);
			}
			else {
				/* 
				   All the selected elements are at the head of the array 
				   which means we can stop when it finds unselected data
				*/
				break;
			}
		}
		else {
			VECCOPY(max, t->data[i].center);
			VECCOPY(min, t->data[i].center);
		}
	}
	VecAddf(t->center, min, max);
	VecMulf(t->center, 0.5);

	calculateCenter2D(t);
}

void calculateCenter(TransInfo *t) 
{
	switch(t->around) {
	case V3D_CENTER:
		calculateCenterBound(t);
		break;
	case V3D_CENTROID:
		calculateCenterMedian(t);
		break;
	case V3D_CURSOR:
		if(t->spacetype==SPACE_IMAGE)
			calculateCenterCursor2D(t);
		else
			calculateCenterCursor(t);
		break;
	case V3D_LOCAL:
		/* Individual element center uses median center for helpline and such */
		calculateCenterMedian(t);
		break;
	case V3D_ACTIVE:
		/* set median, and if if if... do object center */
		
		/* EDIT MODE ACTIVE EDITMODE ELEMENT */
		if (G.obedit && G.obedit->type == OB_MESH && G.editMesh->selected.last) {
			EM_editselection_center(t->center, G.editMesh->selected.last);
			calculateCenter2D(t);
			break;
		} /* END EDIT MODE ACTIVE ELEMENT */
		
		calculateCenterMedian(t);
		if((t->flag & (T_EDIT|T_POSE))==0) {
			Object *ob= OBACT;
			if(ob) {
				VECCOPY(t->center, ob->obmat[3]);
				projectIntView(t, t->center, t->center2d);
			}
		}
	}

	/* setting constraint center */
	VECCOPY(t->con.center, t->center);
	if(t->flag & (T_EDIT|T_POSE)) {
		Object *ob= G.obedit?G.obedit:t->poseobj;
		Mat4MulVecfl(ob->obmat, t->con.center);
	}

	/* voor panning from cameraview */
	if(t->flag & T_OBJECT) {
		if( G.vd->camera==OBACT && G.vd->persp==V3D_CAMOB) {
			float axis[3];
			/* persinv is nasty, use viewinv instead, always right */
			VECCOPY(axis, t->viewinv[2]);
			Normalize(axis);

			/* 6.0 = 6 grid units */
			axis[0]= t->center[0]- 6.0f*axis[0];
			axis[1]= t->center[1]- 6.0f*axis[1];
			axis[2]= t->center[2]- 6.0f*axis[2];
			
			projectIntView(t, axis, t->center2d);
			
			/* rotate only needs correct 2d center, grab needs initgrabz() value */
			if(t->mode==TFM_TRANSLATION) {
				VECCOPY(t->center, axis);
				VECCOPY(t->con.center, t->center);
			}
		}
	}	

	if(t->spacetype==SPACE_VIEW3D)
		initgrabz(t->center[0], t->center[1], t->center[2]);
}

void calculatePropRatio(TransInfo *t)
{
	TransData *td = t->data;
	int i;
	float dist;
	short connected = t->flag & T_PROP_CONNECTED;

	if (t->flag & T_PROP_EDIT) {
		for(i = 0 ; i < t->total; i++, td++) {
			if (td->flag & TD_SELECTED) {
				td->factor = 1.0f;
			}
			else if	((connected && 
						(td->flag & TD_NOTCONNECTED || td->dist > t->propsize))
				||
					(connected == 0 &&
						td->rdist > t->propsize)) {
				/* 
				   The elements are sorted according to their dist member in the array,
				   that means we can stop when it finds one element outside of the propsize.
				*/
				td->flag |= TD_NOACTION;
				td->factor = 0.0f;
				restoreElement(td);
			}
			else {
				/* Use rdist for falloff calculations, it is the real distance */
				td->flag &= ~TD_NOACTION;
				dist= (t->propsize-td->rdist)/t->propsize;
				
				/*
				 * Clamp to positive numbers.
				 * Certain corner cases with connectivity and individual centers
				 * can give values of rdist larger than propsize.
				 */
				if (dist < 0.0f)
					dist = 0.0f;
				
				switch(G.scene->prop_mode) {
				case PROP_SHARP:
					td->factor= dist*dist;
					break;
				case PROP_SMOOTH:
					td->factor= 3.0f*dist*dist - 2.0f*dist*dist*dist;
					break;
				case PROP_ROOT:
					td->factor = (float)sqrt(dist);
					break;
				case PROP_LIN:
					td->factor = dist;
					break;
				case PROP_CONST:
					td->factor = 1.0f;
					break;
				case PROP_SPHERE:
					td->factor = (float)sqrt(2*dist - dist * dist);
					break;
				case PROP_RANDOM:
					BLI_srand( BLI_rand() ); /* random seed */
					td->factor = BLI_frand()*dist;
					break;
				default:
					td->factor = 1;
				}
			}
		}
		switch(G.scene->prop_mode) {
		case PROP_SHARP:
			strcpy(t->proptext, "(Sharp)");
			break;
		case PROP_SMOOTH:
			strcpy(t->proptext, "(Smooth)");
			break;
		case PROP_ROOT:
			strcpy(t->proptext, "(Root)");
			break;
		case PROP_LIN:
			strcpy(t->proptext, "(Linear)");
			break;
		case PROP_CONST:
			strcpy(t->proptext, "(Constant)");
			break;
		case PROP_SPHERE:
			strcpy(t->proptext, "(Sphere)");
			break;
		case PROP_RANDOM:
			strcpy(t->proptext, "(Random)");
			break;
		default:
			strcpy(t->proptext, "");
		}
	}
	else {
		for(i = 0 ; i < t->total; i++, td++) {
			td->factor = 1.0;
		}
		strcpy(t->proptext, "");
	}
}

TransInfo * BIF_GetTransInfo() {
	return &Trans;
}

