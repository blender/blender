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

#include "BMF_Api.h"

#include "DNA_armature_types.h"
#include "DNA_curve_types.h"
#include "DNA_lattice_types.h"
#include "DNA_meta_types.h"
#include "DNA_mesh_types.h"
#include "DNA_object_types.h"
#include "DNA_screen_types.h"
#include "DNA_scene_types.h"
#include "DNA_space_types.h"
#include "DNA_view3d_types.h"

#include "BLI_blenlib.h"
#include "BLI_arithb.h"
#include "BLI_editVert.h"

#include "BKE_utildefines.h"
#include "BKE_anim.h"
#include "BKE_object.h"
#include "BKE_displist.h"
#include "BKE_global.h"
#include "BKE_lattice.h"
#include "BKE_mesh.h"

#include "BIF_gl.h"
#include "BIF_mywindow.h"
#include "BIF_screen.h"
#include "BIF_interface.h"
#include "BIF_space.h"
#include "BIF_editview.h"
#include "BIF_glutil.h"
#include "BIF_toolbox.h"
#include "BIF_editmesh.h"

#include "BSE_view.h"
#include "BSE_edit.h"
#include "BSE_trans_types.h"
#include "BSE_drawipo.h"
#include "BSE_drawview.h"

#include "BDR_editobject.h"
#include "BDR_editmball.h"
#include "BDR_editcurve.h"

/* old stuff */
#include "blendef.h"
#include "mydevice.h"

/*#include "armature.h"*/
/*  #include "edit.h" */
#include "nla.h"

#ifdef __NLA
#include "BIF_editarmature.h"
#endif

/* editmball.c */
extern ListBase editelems;  /* go away ! */


/* from editobject */
extern void make_trans_verts(float *min, float *max, int mode);     

/* circle selection callback */
typedef void (*select_CBfunc)(short selecting, Object *editobj, short *mval, float rad);

extern void obedit_selectionCB(short selecting, Object *editobj, 
                               short *mval, float rad);
extern void uvedit_selectionCB(short selecting, Object *editobj, 
                               short *mval, float rad);

void circle_selectCB(select_CBfunc func);

int get_border(rcti *rect, short col)
{
	float dvec[4], fac1, fac2;
	int retval=1;
	unsigned short event;
	short mval[2], mvalo[4], val, x1, y1;
	char str[64];

	mywinset(G.curscreen->mainwin);
	
	/* slightly larger, 1 pixel at the edge */
	glReadBuffer(GL_FRONT);
	glDrawBuffer(GL_FRONT);

	/* removed my_get_frontbuffer, this crashes when it gets a part outside the screen */
	/* solved it with just a redraw! */

	mywinset(curarea->win);
	
	glDrawBuffer(GL_FRONT);
	persp(PERSP_WIN);
	initgrabz(0.0, 0.0, 0.0);
	
	getmouseco_areawin(mvalo);

	/* draws the selection initial cross */
	sdrawXORline4(0, 0,  mvalo[1],  curarea->winx,  mvalo[1]);
	sdrawXORline4(1, mvalo[0],  0,  mvalo[0],  curarea->winy); 

	while(TRUE) {
	
		/* for when a renderwindow is open, and a mouse cursor activates it */
		persp(PERSP_VIEW);
		mywinset(curarea->win);
		persp(PERSP_WIN);
		
		/* selection loop while mouse pressed */
		getmouseco_areawin(mval);

		if(mvalo[0]!=mval[0] || mvalo[1]!=mval[1]) {

			/* aiming cross */
  			sdrawXORline4(0, 0,  mval[1],  curarea->winx,  mval[1]);
  			sdrawXORline4(1, mval[0],  0,  mval[0],  curarea->winy);
			glFlush();

			mvalo[0]= mval[0];
			mvalo[1]= mval[1];
		}
		event= extern_qread(&val);

		if(event && val) {
			if(event==ESCKEY) {
				retval= 0;
				break;
			}
			else if(event==BKEY) {
				/* b has been pressed twice: proceed with circle select */
				retval= 0;
				break;
			}
			else if(event==LEFTMOUSE) break;
			else if(event==MIDDLEMOUSE) break;
			else if(event==RIGHTMOUSE) break;
		}
	} /* end while (TRUE) */

	/* erase XORed lines */
	sdrawXORline4(-1, 0, 0, 0, 0);
	
	if(retval) {
		/* box select */
		x1= mval[0];
		y1= mval[1];
		
		getmouseco_areawin(mvalo);

		sdrawXORline4(0, x1, y1, x1, mvalo[1]); 
		sdrawXORline4(1, x1, mvalo[1], mvalo[0], mvalo[1]); 
		sdrawXORline4(2, mvalo[0], mvalo[1], mvalo[0], y1); 
		sdrawXORline4(3,  mvalo[0], y1, x1, y1); 
		glFlush();
			
		while(TRUE) {
			getmouseco_areawin(mval);
			if(mvalo[0]!=mval[0] || mvalo[1]!=mval[1]) {

				sdrawXORline4(0, x1, y1, x1, mval[1]); 
				sdrawXORline4(1, x1, mval[1], mval[0], mval[1]); 
				sdrawXORline4(2, mval[0], mval[1], mval[0], y1); 
				sdrawXORline4(3,  mval[0], y1, x1, y1); 
				glFlush();

				mvalo[0]= mval[0];
				mvalo[1]= mval[1];
			}
			
			event= extern_qread(&val);
			
			/* still because of the renderwindow... */
			persp(PERSP_VIEW);
			mywinset(curarea->win);
			persp(PERSP_WIN);
				
			if(val==0) {
				if(event==ESCKEY) {
					retval= 0;
					break;
				}
				else if(event==LEFTMOUSE) break;
				else if(event==MIDDLEMOUSE) break;
				else if(event==RIGHTMOUSE) break;
			}
			
			if(curarea->spacetype==SPACE_VIEW3D) {
				glColor3f(0.4375, 0.4375, 0.4375); 
				glRecti(0, 10, 250, 20);
				glColor3f(0.0, 0.0, 0.0); 

				if(G.vd->persp==0) {
					window_to_3d(dvec, mvalo[0]-x1, mvalo[1]-y1);

					glRasterPos2i(10,  10);
					sprintf(str, "X %.4f  Y %.4f  Z %.4f  Dia %.4f", dvec[0], dvec[1], dvec[2], sqrt(dvec[0]*dvec[0]+dvec[1]*dvec[1]+dvec[2]*dvec[2]));
					BMF_DrawString(G.fonts, str);
				}
				else if(G.vd->persp==2) {
					rcti vb;

					calc_viewborder(G.vd, &vb);

					fac1= (mvalo[0]-x1)/( (float) (vb.xmax-vb.xmin) );
					fac1*= 0.01*G.scene->r.size*G.scene->r.xsch;
					
					fac2= (mvalo[1]-y1)/( (float) (vb.ymax-vb.ymin) );
					fac2*= 0.01*G.scene->r.size*G.scene->r.ysch;
					
					glRasterPos2i(10,  10);
					sprintf(str, "X %.1f  Y %.1f  Dia %.1f", fabs(fac1), fabs(fac2), sqrt(fac1*fac1 + fac2*fac2) );
					BMF_DrawString(G.fonts, str);
				}
			}
			else if(curarea->spacetype==SPACE_IPO) {
				SpaceIpo *sipo= curarea->spacedata.first;

				glColor3f(.40625, .40625, .40625);
				glRecti(20, 30, 170, 40);
				glColor3f(0.0, 0.0, 0.0); 
							
				mvalo[2]= x1;
				mvalo[3]= y1;
				areamouseco_to_ipoco(&sipo->v2d, mval, dvec, dvec+1);
				areamouseco_to_ipoco(&sipo->v2d, mvalo+2, dvec+2, dvec+3);

				glRasterPos2i(30,  30);
				sprintf(str, "Time: %.4f  Y %.4f", dvec[0]-dvec[2], dvec[1]-dvec[3]);
				BMF_DrawString(G.fonts, str);
			}
		} /* end while (TRUE) */
		sdrawXORline4(-1, 0, 0, 0, 0);
		
		if(retval) {
			rect->xmin= x1;
			rect->ymin= y1;
			rect->xmax= mval[0];
			rect->ymax= mval[1];
			retval= event;

			/* normalise */
			if(rect->xmin>rect->xmax) SWAP(int, rect->xmin, rect->xmax);
			if(rect->ymin>rect->ymax) SWAP(int, rect->ymin, rect->ymax);
			
			if(rect->xmin==rect->xmax) retval= 0;
			if(rect->ymin==rect->ymax) retval= 0;
		}
	}


	/* clear */
	if(event!=BKEY) {
		if ELEM(curarea->spacetype, SPACE_VIEW3D, SPACE_IPO) {
			scrarea_queue_winredraw(curarea);
		}
	}
	
	glFlush();
	glReadBuffer(GL_BACK);
	glDrawBuffer(GL_BACK);

	persp(PERSP_VIEW);
	
	/* pressed B again ? -> brush select */
	if(event==BKEY) {
		switch (curarea->spacetype) {
		case SPACE_VIEW3D:
			if (G.obedit)
				circle_selectCB(&obedit_selectionCB);
			return 0;
		case SPACE_IMAGE: // brush select in UV editor
			circle_selectCB(&uvedit_selectionCB);
			// this is a hack; we return 0 that the caller from get_border
			// doesn't execute the selection code for border select..
			return 0;
		}
	}
	return retval;
}

void draw_sel_circle(short *mval, short *mvalo, float rad, float rado, int selecting)
{
	static short no_mvalo=0;

	if(mval==0 && mvalo==0) {	/* signal */
		no_mvalo= 1;
		return;
	}

	persp(PERSP_WIN);
	glDrawBuffer(GL_FRONT);

	/* draw circle */

	if(mvalo && no_mvalo==0) {
		sdrawXORcirc(mvalo[0], mvalo[1], rado);
	}
	
	if(mval) {
		sdrawXORcirc(mval[0], mval[1], rad);
	}

	glFlush();
	persp(PERSP_VIEW);
	glDrawBuffer(GL_BACK);
	
	no_mvalo= 0;
}

/** This function does the same as editview.c:circle_select(),
  * but the selection actions are defined by a callback, making
  * it (hopefully) reusable for other windows than the 3D view.
  */

void circle_selectCB(select_CBfunc callback)
{
	static float rad= 40.0;
	float rado;
	int firsttime=1;
	unsigned short event;
	short mvalo[2], mval[2], val;
	short selecting=0;
	Object *obj;
	
	if(G.obedit) obj = G.obedit;
	else obj = OBACT;

	
	getmouseco_areawin(mvalo);
	draw_sel_circle(mvalo, 0, rad, 0.0, selecting);
	
	rado= rad;
	
	while(TRUE) {
		
		/* for when another window is open and a mouse cursor activates it */

		mywinset(curarea->win);

		getmouseco_areawin(mval);
		
		if(mval[0]!=mvalo[0] || mval[1]!=mvalo[1] || rado!=rad || firsttime) {
			firsttime= 0;
			
			draw_sel_circle(mval, mvalo, rad, rado, selecting);
		
			mvalo[0]= mval[0];
			mvalo[1]= mval[1];
			rado= rad;

			if(selecting) {
				callback(selecting, obj, mval, rad);
			}
		}
		event= extern_qread(&val);
		if (event) {
			int afbreek= 0;
			
			switch(event) {
			case LEFTMOUSE:
			case MIDDLEMOUSE:
				if(val) selecting= event;
				else selecting= 0;
				firsttime= 1;
				
				break;
			case WHEELDOWNMOUSE:
			case PADPLUSKEY:
				if(val) if(rad<200.0) rad*= 1.2;
				break;
			case WHEELUPMOUSE:
			case PADMINUS:
				if(val) if(rad>5.0) rad/= 1.2;
				break;
			
			case ESCKEY: case SPACEKEY: case RIGHTMOUSE:
			case GKEY: case SKEY: case RKEY: case XKEY: case EKEY: case TABKEY:
				afbreek= 1;
				break;

			}
			
			if(afbreek) break;
		}
	}
	
	/* clear circle */
	draw_sel_circle(0, mvalo, 0, rad, 1);

	countall();
	allqueue(REDRAWINFO, 0);
}

void count_object(Object *ob, int sel)
{
	Mesh *me;
	Curve *cu;
	int tot=0, totf=0;
	
	switch(ob->type) {
	case OB_MESH:
		G.totmesh++;
		me= get_mesh(ob);
		if(me) {
			int totvert, totface;
				/* hack, should be getting displistmesh from a central function */
			if (mesh_uses_displist(me) && me->disp.first && ((DispList*)me->disp.first)->type==DL_MESH) {
				DispListMesh *dlm= ((DispList*)me->disp.first)->mesh;
				totvert= dlm->totvert;
				totface= dlm->totface;
			} else {
				totvert= me->totvert;
				totface= me->totface;
			}
			
			G.totvert+= totvert;
			G.totface+= totface;
			if(sel) {
				G.totvertsel+= totvert;
				G.totfacesel+= totface;
			}
		}
		break;

	case OB_LAMP:
		G.totlamp++;
		break;
	case OB_SURF:
	case OB_CURVE:
	case OB_FONT:
		G.totcurve++;
		tot=totf= 0;
		cu= ob->data;
		if(cu->disp.first==0) makeDispList(ob);
		count_displist( &cu->disp, &tot, &totf);
		G.totvert+= tot;
		G.totface+= totf;
		if(sel) {
			G.totvertsel+= tot;
			G.totfacesel+= totf;
		}
		break;
	case OB_MBALL:
		count_displist( &ob->disp, &tot, &totf);
		G.totvert+= tot;
		G.totface+= totf;
		if(sel) {
			G.totvertsel+= tot;
			G.totfacesel+= totf;
		}
		
		break;
	}
	
}

void countall()
{
/*  	extern Lattice *editLatt; in BKE_lattice.h*/
	extern ListBase editNurb;
	/* extern ListBase bpbase; */
	Base *base;
	Object *ob;
	Mesh *me;
	Nurb *nu;
	BezTriple *bezt;
	BPoint *bp;
	MetaElem *ml;
	/* struct BodyPoint *bop; */
	struct EditVert *eve;
	struct EditVlak *evl;
#ifdef __NLA
	struct EditBone *ebo;
#endif
	int a;

	G.totvert= G.totvertsel= G.totfacesel= G.totface= G.totobj= 
	    G.totmesh= G.totlamp= G.totcurve= G.totobjsel=  0;

	if(G.obedit) {
		
		if(G.obedit->type==OB_MESH) {
			eve= G.edve.first;
			while(eve) {
				G.totvert++;
				if(eve->f & 1) G.totvertsel++;
				eve= eve->next;
			}
			evl= G.edvl.first;
			while(evl) {
				G.totface++;
				if(evl->v1->f & 1) {
					if(evl->v2->f & 1) {
						if(evl->v3->f & 1) {
							if(evl->v4)  {
								if(evl->v4->f & 1) G.totfacesel++;
							}
							else {
								G.totfacesel++;
							}
						}
					}
				}
				evl= evl->next;
			}
		}
#ifdef __NLA
		else if (G.obedit->type==OB_ARMATURE){
			for (ebo=G.edbo.first;ebo;ebo=ebo->next){
				
				/* Sync selection to parent for ik children */
				if ((ebo->flag & BONE_IK_TOPARENT) && ebo->parent){
					G.totvert--;
					if (ebo->parent->flag & BONE_TIPSEL)
						ebo->flag |= BONE_ROOTSEL;
					else
						ebo->flag &= ~BONE_ROOTSEL;
				}
				
				if (ebo->flag & BONE_TIPSEL)
					G.totvertsel++;
				if (ebo->flag & BONE_ROOTSEL)
					G.totvertsel++;
				
				if ((ebo->flag & BONE_TIPSEL) && (ebo->flag & BONE_ROOTSEL))
					ebo->flag |= BONE_SELECTED;
				else
					ebo->flag &= ~BONE_SELECTED;
				
				//	If this is an IK child and it's parent is being moved, remove our root
				if ((ebo->flag & BONE_IK_TOPARENT)&& (ebo->flag & BONE_ROOTSEL) && ebo->parent && (ebo->parent->flag & BONE_TIPSEL)){
					G.totvertsel--;
				}

				G.totvert+=2;
				G.totface++;
				

			}
		}
#endif
		else if ELEM3(G.obedit->type, OB_CURVE, OB_SURF, OB_FONT) {
			nu= editNurb.first;
			while(nu) {
				if((nu->type & 7)==CU_BEZIER) {
					bezt= nu->bezt;
					a= nu->pntsu;
					while(a--) {
					G.totvert+=3;
					if(bezt->f1) G.totvertsel++;
					if(bezt->f2) G.totvertsel++;
					if(bezt->f3) G.totvertsel++;
					bezt++;
				}
			}
			else {
				bp= nu->bp;
				a= nu->pntsu*nu->pntsv;
				while(a--) {
					G.totvert++;
					if(bp->f1 & 1) G.totvertsel++;
					bp++;
				}
			}
			nu= nu->next;
		}
	}
	else if(G.obedit->type==OB_MBALL) {
		ml= editelems.first;
		while(ml) {
			G.totvert++;
			if(ml->flag & SELECT) G.totvertsel++;
			ml= ml->next;
		}
	}
	else if(G.obedit->type==OB_LATTICE) {
		bp= editLatt->def;
		
		a= editLatt->pntsu*editLatt->pntsv*editLatt->pntsw;
		while(a--) {
			G.totvert++;
			if(bp->f1 & 1) G.totvertsel++;
			bp++;
		}
	}
	
	allqueue(REDRAWINFO, 1);	/* 1, because header->win==0! */
		return;
	}
	else if(G.f & (G_FACESELECT + G_VERTEXPAINT + G_TEXTUREPAINT +G_WEIGHTPAINT)) {
		me= get_mesh((G.scene->basact) ? (G.scene->basact->object) : 0);
		if(me) {
			G.totface= me->totface;
			G.totvert= me->totvert;
		}
		allqueue(REDRAWINFO, 1);	/* 1, because header->win==0! */
		return;
	}

	if(G.vd==0) return;
	if(G.scene==0) return;

	base= (G.scene->base.first);
	while(base) {
		if(G.vd->lay & base->lay) {
			
			G.totobj++;
			if(base->flag & SELECT) G.totobjsel++;
			
			count_object(base->object, base->flag & SELECT);
			
			if(base->object->transflag & OB_DUPLI) {
				extern ListBase duplilist;

				make_duplilist(G.scene, base->object);
				ob= duplilist.first;
				while(ob) {
					G.totobj++;
					count_object(ob, base->flag & SELECT);
					ob= ob->id.next;
				}
				free_duplilist();
			}
		}
		base= base->next;
	}
	allqueue(REDRAWINFO, 1); 	/* 1, because header->win==0! */
}


void snap_sel_to_grid()
{
	extern TransVert *transvmain;
	extern int tottrans;
	extern float originmat[3][3];	/* object.c */
	TransVert *tv;
	Base *base;
	Object *ob;
	float gridf, imat[3][3], bmat[3][3], vec[3];
	int a;

	gridf= G.vd->grid;


		if(G.obedit) {
#ifdef __NLA
			if ELEM5(G.obedit->type, OB_ARMATURE, OB_LATTICE, OB_MESH, OB_SURF, OB_CURVE) make_trans_verts(bmat[0], bmat[1], 0);
#else
			if ELEM4(G.obedit->type, OB_LATTICE, OB_MESH, OB_SURF, OB_CURVE) make_trans_verts(bmat[0], bmat[1], 0);
#endif
			if(tottrans==0) return;

			Mat3CpyMat4(bmat, G.obedit->obmat);
			Mat3Inv(imat, bmat);

			tv= transvmain;
			for(a=0; a<tottrans; a++, tv++) {

			VECCOPY(vec, tv->loc);
			Mat3MulVecfl(bmat, vec);
			VecAddf(vec, vec, G.obedit->obmat[3]);
			vec[0]= G.vd->grid*floor(.5+ vec[0]/gridf);
			vec[1]= G.vd->grid*floor(.5+ vec[1]/gridf);
			vec[2]= G.vd->grid*floor(.5+ vec[2]/gridf);
			VecSubf(vec, vec, G.obedit->obmat[3]);

			Mat3MulVecfl(imat, vec);
			VECCOPY(tv->loc, vec);

			}

			MEM_freeN(transvmain);
			transvmain= 0;

			if ELEM(G.obedit->type, OB_SURF, OB_CURVE) makeDispList(G.obedit);

			if (G.obedit->type == OB_ARMATURE)
				special_trans_update(0);

			allqueue(REDRAWVIEW3D, 0);
			return;
		}
#ifdef __NLA
		if (G.obpose){
			allqueue(REDRAWVIEW3D, 0);
			return;
		}
#endif
		base= (G.scene->base.first);
		while(base) {
			if( ( ((base)->flag & SELECT) && ((base)->lay & G.vd->lay) && ((base)->object->id.lib==0))) {
				ob= base->object;

				vec[0]= -ob->obmat[3][0]+G.vd->grid*floor(.5+ ob->obmat[3][0]/gridf);
				vec[1]= -ob->obmat[3][1]+G.vd->grid*floor(.5+ ob->obmat[3][1]/gridf);
				vec[2]= -ob->obmat[3][2]+G.vd->grid*floor(.5+ ob->obmat[3][2]/gridf);

				if(ob->parent) {
					where_is_object(ob);

					Mat3Inv(imat, originmat);
					Mat3MulVecfl(imat, vec);
					ob->loc[0]+= vec[0];
					ob->loc[1]+= vec[1];
					ob->loc[2]+= vec[2];
				}
				else {
					ob->loc[0]+= vec[0];
					ob->loc[1]+= vec[1];
					ob->loc[2]+= vec[2];
				}
			}

			base= base->next;
		}
		allqueue(REDRAWVIEW3D, 0);
}

void snap_sel_to_curs()
{
	extern TransVert *transvmain;
	extern int tottrans;
	extern float originmat[3][3];	/* object.c */
	TransVert *tv;
	Base *base;
	Object *ob;
	float *curs, imat[3][3], bmat[3][3], vec[3];
	int a;

	curs= give_cursor();

		if(G.obedit) {
#ifdef __NLA
			if ELEM5(G.obedit->type, OB_ARMATURE, OB_LATTICE, OB_MESH, OB_SURF, OB_CURVE) make_trans_verts(bmat[0], bmat[1], 0);
#else
			if ELEM4(G.obedit->type, OB_LATTICE, OB_MESH, OB_SURF, OB_CURVE) make_trans_verts(bmat[0], bmat[1], 0);
#endif
			if(tottrans==0) return;

			Mat3CpyMat4(bmat, G.obedit->obmat);
			Mat3Inv(imat, bmat);

			tv= transvmain;
			for(a=0; a<tottrans; a++, tv++) {


				vec[0]= curs[0]-G.obedit->obmat[3][0];
				vec[1]= curs[1]-G.obedit->obmat[3][1];
				vec[2]= curs[2]-G.obedit->obmat[3][2];


				Mat3MulVecfl(imat, vec);
				VECCOPY(tv->loc, vec);

			}
			MEM_freeN(transvmain);
			transvmain= 0;

			if ELEM(G.obedit->type, OB_SURF, OB_CURVE) makeDispList(G.obedit);

			if (G.obedit->type == OB_ARMATURE)
				special_trans_update(0);

			allqueue(REDRAWVIEW3D, 0);
			return;
		}
#ifdef __NLA
		if (G.obpose){
			allqueue(REDRAWVIEW3D, 0);
			return;
		}
#endif
		base= (G.scene->base.first);
		while(base) {
			if( ( ((base)->flag & SELECT) && ((base)->lay & G.vd->lay) && ((base)->object->id.lib==0))) {
				ob= base->object;

				vec[0]= -ob->obmat[3][0] + curs[0];
				vec[1]= -ob->obmat[3][1] + curs[1];
				vec[2]= -ob->obmat[3][2] + curs[2];


				if(ob->parent) {
					where_is_object(ob);

					Mat3Inv(imat, originmat);
					Mat3MulVecfl(imat, vec);
					ob->loc[0]+= vec[0];
					ob->loc[1]+= vec[1];
					ob->loc[2]+= vec[2];
				}
				else {
					ob->loc[0]+= vec[0];
					ob->loc[1]+= vec[1];
					ob->loc[2]+= vec[2];
				}
			}

			base= base->next;
		}
		allqueue(REDRAWVIEW3D, 0);
}

void snap_curs_to_grid()
{
	float gridf, *curs;

	gridf= G.vd->grid;
	curs= give_cursor();

	curs[0]= G.vd->grid*floor(.5+curs[0]/gridf);
	curs[1]= G.vd->grid*floor(.5+curs[1]/gridf);
	curs[2]= G.vd->grid*floor(.5+curs[2]/gridf);

	allqueue(REDRAWVIEW3D, 0);

}

void snap_curs_to_sel()
{
	extern TransVert *transvmain;
	extern int tottrans;
	TransVert *tv;
	Base *base;
	float *curs, bmat[3][3], vec[3], min[3], max[3], centroid[3];
	int count, a;

	curs= give_cursor();

	count= 0;
	INIT_MINMAX(min, max);
	centroid[0]= centroid[1]= centroid[2]= 0.0;

		if(G.obedit) {
			tottrans=0;
#ifdef __NLA
			if ELEM5(G.obedit->type, OB_ARMATURE, OB_LATTICE, OB_MESH, OB_SURF, OB_CURVE) make_trans_verts(bmat[0], bmat[1], 0);
#else
			if ELEM4(G.obedit->type, OB_LATTICE, OB_MESH, OB_SURF, OB_CURVE) make_trans_verts(bmat[0], bmat[1], 0);
#endif
			if(tottrans==0) return;

			Mat3CpyMat4(bmat, G.obedit->obmat);

			tv= transvmain;
			for(a=0; a<tottrans; a++, tv++) {
				VECCOPY(vec, tv->loc);
				Mat3MulVecfl(bmat, vec);
				VecAddf(vec, vec, G.obedit->obmat[3]);
				VecAddf(centroid, centroid, vec);
				DO_MINMAX(vec, min, max);
			}

			if(G.vd->around==V3D_CENTROID) {
				VecMulf(centroid, 1.0/(float)tottrans);
				VECCOPY(curs, centroid);
			}
			else {
				curs[0]= (min[0]+max[0])/2;
				curs[1]= (min[1]+max[1])/2;
				curs[2]= (min[2]+max[2])/2;
			}
			MEM_freeN(transvmain);
			transvmain= 0;
		}
		else {
			base= (G.scene->base.first);
			while(base) {
				if(((base)->flag & SELECT) && ((base)->lay & G.vd->lay) ) {
					VECCOPY(vec, base->object->obmat[3]);
					VecAddf(centroid, centroid, vec);
					DO_MINMAX(vec, min, max);
					count++;
				}
				base= base->next;
			}
			if(count) {
				if(G.vd->around==V3D_CENTROID) {
					VecMulf(centroid, 1.0/(float)count);
					VECCOPY(curs, centroid);
				}
				else {
					curs[0]= (min[0]+max[0])/2;
					curs[1]= (min[1]+max[1])/2;
					curs[2]= (min[2]+max[2])/2;
				}
			}
		}
		allqueue(REDRAWVIEW3D, 0);
}

void snap_curs_to_firstsel()
{
	extern TransVert *transvmain;
	extern int tottrans;
	TransVert *tv;
	Base *base;
	float *curs, bmat[3][3], vec[3], min[3], max[3], centroid[3];
	int count;

	curs= give_cursor();

	count= 0;
	INIT_MINMAX(min, max);
	centroid[0]= centroid[1]= centroid[2]= 0.0;

		if(G.obedit) {
			tottrans=0;
#ifdef __NLA
			if ELEM5(G.obedit->type, OB_ARMATURE, OB_LATTICE, OB_MESH, OB_SURF, OB_CURVE) make_trans_verts(bmat[0], bmat[1], 0);
#else
			if ELEM4(G.obedit->type, OB_LATTICE, OB_MESH, OB_SURF, OB_CURVE) make_trans_verts(bmat[0], bmat[1], 0);
#endif
			if(tottrans==0) return;

			Mat3CpyMat4(bmat, G.obedit->obmat);

			tv= transvmain;
			VECCOPY(vec, tv->loc);
				/*Mat3MulVecfl(bmat, vec);
				VecAddf(vec, vec, G.obedit->obmat[3]);
				VecAddf(centroid, centroid, vec);
				DO_MINMAX(vec, min, max);*/

			if(G.vd->around==V3D_CENTROID) {
				VecMulf(vec, 1.0/(float)tottrans);
				VECCOPY(curs, vec);
			}
			else {
				curs[0]= vec[0];
				curs[1]= vec[1];
				curs[2]= vec[2];
			}
			MEM_freeN(transvmain);
			transvmain= 0;
		}
		else {
			base= (G.scene->base.first);
			while(base) {
				if(((base)->flag & SELECT) && ((base)->lay & G.vd->lay) ) {
					VECCOPY(vec, base->object->obmat[3]);
					VecAddf(centroid, centroid, vec);
					DO_MINMAX(vec, min, max);
					count++;
				}
				base= base->next;
			}
			if(count) {
				if(G.vd->around==V3D_CENTROID) {
					VecMulf(centroid, 1.0/(float)count);
					VECCOPY(curs, centroid);
				}
				else {
					curs[0]= (min[0]+max[0])/2;
					curs[1]= (min[1]+max[1])/2;
					curs[2]= (min[2]+max[2])/2;
				}
			}
		}
		allqueue(REDRAWVIEW3D, 0);
}

void snap_to_center()
{
	extern TransVert *transvmain;
	extern int tottrans;
	extern float originmat[3][3];
	TransVert *tv;
	Base *base;
	Object *ob;
	float snaploc[3], imat[3][3], bmat[3][3], vec[3], min[3], max[3], centroid[3];
	int count, a;


/*calculate the snaplocation (centerpoint) */
	count= 0;
	INIT_MINMAX(min, max);
	centroid[0]= centroid[1]= centroid[2]= 0.0;

		if(G.obedit) {
			/*tottrans=0;*/
#ifdef __NLA
			if ELEM5(G.obedit->type, OB_ARMATURE, OB_LATTICE, OB_MESH, OB_SURF, OB_CURVE) make_trans_verts(bmat[0], bmat[1], 0);
#else
			if ELEM4(G.obedit->type, OB_LATTICE, OB_MESH, OB_SURF, OB_CURVE) make_trans_verts(bmat[0], bmat[1], 0);
#endif
			if(tottrans==0) return;

			Mat3CpyMat4(bmat, G.obedit->obmat);
			Mat3Inv(imat, bmat);

			tv= transvmain;
			for(a=0; a<tottrans; a++, tv++) {
				VECCOPY(vec, tv->loc);
				Mat3MulVecfl(bmat, vec);
				VecAddf(vec, vec, G.obedit->obmat[3]);
				VecAddf(centroid, centroid, vec);
				DO_MINMAX(vec, min, max);
			}

			if(G.vd->around==V3D_CENTROID) {
				VecMulf(centroid, 1.0/(float)tottrans);
				VECCOPY(snaploc, centroid);
			}
			else {
				snaploc[0]= (min[0]+max[0])/2;
				snaploc[1]= (min[1]+max[1])/2;
				snaploc[2]= (min[2]+max[2])/2;
			}
			
			MEM_freeN(transvmain);
			transvmain= 0;

		}
		else {
			base= (G.scene->base.first);
			while(base) {
				if(((base)->flag & SELECT) && ((base)->lay & G.vd->lay) ) {
					VECCOPY(vec, base->object->obmat[3]);
					VecAddf(centroid, centroid, vec);
					DO_MINMAX(vec, min, max);
					count++;
				}
				base= base->next;
			}
			if(count) {
				if(G.vd->around==V3D_CENTROID) {
					VecMulf(centroid, 1.0/(float)count);
					VECCOPY(snaploc, centroid);
				}
				else {
					snaploc[0]= (min[0]+max[0])/2;
					snaploc[1]= (min[1]+max[1])/2;
					snaploc[2]= (min[2]+max[2])/2;
				}
			}
		}


/* Snap the selection to the snaplocation (duh!) */

		if(G.obedit) {
#ifdef __NLA
			if ELEM5(G.obedit->type, OB_ARMATURE, OB_LATTICE, OB_MESH, OB_SURF, OB_CURVE) make_trans_verts(bmat[0], bmat[1], 0);
#else
			if ELEM4(G.obedit->type, OB_LATTICE, OB_MESH, OB_SURF, OB_CURVE) make_trans_verts(bmat[0], bmat[1], 0);
#endif
			if(tottrans==0) return;

			Mat3CpyMat4(bmat, G.obedit->obmat);
			Mat3Inv(imat, bmat);

			tv= transvmain;
			for(a=0; a<tottrans; a++, tv++) {


				vec[0]= snaploc[0]-G.obedit->obmat[3][0];
				vec[1]= snaploc[1]-G.obedit->obmat[3][1];
				vec[2]= snaploc[2]-G.obedit->obmat[3][2];


				Mat3MulVecfl(imat, vec);
				VECCOPY(tv->loc, vec);

			}
			MEM_freeN(transvmain);
			transvmain= 0;

			if ELEM(G.obedit->type, OB_SURF, OB_CURVE) makeDispList(G.obedit);

			if (G.obedit->type == OB_ARMATURE)
				special_trans_update(0);

			allqueue(REDRAWVIEW3D, 0);
			return;
		}
#ifdef __NLA
		if (G.obpose){
			allqueue(REDRAWVIEW3D, 0);
			return;
		}
#endif
		base= (G.scene->base.first);
		while(base) {
			if( ( ((base)->flag & SELECT) && ((base)->lay & G.vd->lay) && ((base)->object->id.lib==0))) {
				ob= base->object;

				vec[0]= -ob->obmat[3][0] + snaploc[0];
				vec[1]= -ob->obmat[3][1] + snaploc[1];
				vec[2]= -ob->obmat[3][2] + snaploc[2];


				if(ob->parent) {
					where_is_object(ob);

					Mat3Inv(imat, originmat);
					Mat3MulVecfl(imat, vec);
					ob->loc[0]+= vec[0];
					ob->loc[1]+= vec[1];
					ob->loc[2]+= vec[2];
				}
				else {
					ob->loc[0]+= vec[0];
					ob->loc[1]+= vec[1];
					ob->loc[2]+= vec[2];
				}
			}

			base= base->next;
		}

		allqueue(REDRAWVIEW3D, 0);
}


void snapmenu()
{
	short event;

	event = pupmenu("Snap %t|Selection -> Grid%x1|Selection -> Cursor%x2|Cursor-> Grid%x3|Cursor-> Selection%x4|Selection-> Center%x5");

	switch (event) {
		case 1: /*Selection to grid*/
		    snap_sel_to_grid();
		    break;
		case 2: /*Selection to cursor*/
		    snap_sel_to_curs();
		    break;	    
		case 3: /*Cursor to grid*/
		    snap_curs_to_grid();
		    break;
		case 4: /*Cursor to selection*/
		    snap_curs_to_sel();
		    break;
		case 5: /*Selection to center of selection*/
		    snap_to_center();
		    break;
	}
}


void mergemenu(void)
{
	extern float doublimit;
	short event;

	event = pupmenu("MERGE %t|At Center%x1|At Cursor%x2");

	if (event==-1) return; /* Return if the menu is closed without any choices */

	undo_push_mesh("Merge"); /* The action has been confirmed, push the mesh down the undo pipe */

	if (event==1) 
		snap_to_center(); /*Merge at Center*/
	else
		snap_sel_to_curs(); /*Merge at Cursor*/

	notice("Removed: %d", removedoublesflag(1, doublimit));
	allqueue(REDRAWVIEW3D, 0);
	countall();
}

void delete_context_selected(void) {
	if(G.obedit) {
		if(G.obedit->type==OB_MESH) delete_mesh();
		else if ELEM(G.obedit->type, OB_CURVE, OB_SURF) delNurb();
		else if(G.obedit->type==OB_MBALL) delete_mball();
		else if (G.obedit->type==OB_ARMATURE) delete_armature();
	}
	else delete_obj(0);
}

void duplicate_context_selected(void) {
	if(G.obedit) {
		if(G.obedit->type==OB_MESH) adduplicate_mesh();
		else if(G.obedit->type==OB_ARMATURE) adduplicate_armature();
		else if(G.obedit->type==OB_MBALL) adduplicate_mball();
		else if ELEM(G.obedit->type, OB_CURVE, OB_SURF) adduplicate_nurb();
	}
	else if(G.obpose){
		error ("Duplicate not possible in posemode.");
	}
	else adduplicate(0);
}

void toggle_shading(void) {
	if(G.qual & LR_CTRLKEY) {
		reshadeall_displist();
		G.vd->drawtype= OB_SHADED;
	}
	else if(G.qual & LR_SHIFTKEY) {
		if(G.vd->drawtype== OB_SHADED) G.vd->drawtype= OB_WIRE;
		else G.vd->drawtype= OB_SHADED;
	}
	else if(G.qual & LR_ALTKEY) {
		if(G.vd->drawtype== OB_TEXTURE) G.vd->drawtype= OB_SOLID;
		else G.vd->drawtype= OB_TEXTURE;
	}
	else {
		if(G.vd->drawtype==OB_SOLID || G.vd->drawtype==OB_SHADED) G.vd->drawtype= OB_WIRE;
		else G.vd->drawtype= OB_SOLID;
	}
}

void minmax_verts(float *min, float *max)
{
	extern TransVert *transvmain;
	extern int tottrans;
	TransVert *tv;
	float centroid[3], vec[3], bmat[3][3];
	int a;

	tottrans=0;
#ifdef __NLA
	if ELEM5(G.obedit->type, OB_ARMATURE, OB_LATTICE, OB_MESH, OB_SURF, OB_CURVE) make_trans_verts(bmat[0], bmat[1], 0);
#else
	if ELEM4(G.obedit->type, OB_LATTICE, OB_MESH, OB_SURF, OB_CURVE) make_trans_verts(bmat[0], bmat[1], 0);
#endif
	if(tottrans==0) return;

	Mat3CpyMat4(bmat, G.obedit->obmat);
	
	tv= transvmain;
	for(a=0; a<tottrans; a++, tv++) {		
		VECCOPY(vec, tv->loc);
		Mat3MulVecfl(bmat, vec);
		VecAddf(vec, vec, G.obedit->obmat[3]);
		VecAddf(centroid, centroid, vec);
		DO_MINMAX(vec, min, max);		
	}
	
	MEM_freeN(transvmain);
	transvmain= 0;
}