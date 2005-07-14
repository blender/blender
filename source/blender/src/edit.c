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
#include <string.h>

#ifdef HAVE_CONFIG_H
#include <config.h>
#endif

#ifndef WIN32
#include <unistd.h>
#else
#include <io.h>
#endif   

#include "MEM_guardedalloc.h"

#include "BMF_Api.h"

#include "PIL_time.h"

#include "DNA_action_types.h"
#include "DNA_armature_types.h"
#include "DNA_curve_types.h"
#include "DNA_ipo_types.h"
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

#include "BKE_action.h"
#include "BKE_armature.h"
#include "BKE_anim.h"
#include "BKE_curve.h"
#include "BKE_depsgraph.h"
#include "BKE_displist.h"
#include "BKE_global.h"
#include "BKE_ipo.h"
#include "BKE_lattice.h"
#include "BKE_mesh.h"
#include "BKE_object.h"
#include "BKE_utildefines.h"

#include "BIF_editmesh.h"
#include "BIF_editview.h"
#include "BIF_editarmature.h"
#include "BIF_gl.h"
#include "BIF_glutil.h"
#include "BIF_interface.h"
#include "BIF_mywindow.h"
#include "BIF_resources.h"
#include "BIF_space.h"
#include "BIF_screen.h"
#include "BIF_toolbox.h"

#include "BSE_edit.h"
#include "BSE_drawipo.h"
#include "BSE_drawview.h"
#include "BSE_trans_types.h"
#include "BSE_view.h"

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


/* circle selection callback */
typedef void (*select_CBfunc)(short selecting, Object *editobj, short *mval, float rad);

extern void obedit_selectionCB(short selecting, Object *editobj, 
                               short *mval, float rad);
extern void uvedit_selectionCB(short selecting, Object *editobj, 
                               short *mval, float rad);

void circle_selectCB(select_CBfunc func);

/* local protos ---------------*/
void snap_curs_to_firstsel(void);


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
	glFlush();
	
	while(TRUE) {
	
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

			/* for when a renderwindow is open, and a mouse cursor activates it */
			persp(PERSP_VIEW);
			mywinset(curarea->win);
			persp(PERSP_WIN);
			
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
		else PIL_sleep_ms(10);
		
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
				
				/* draw size information in corner */
				if(curarea->spacetype==SPACE_VIEW3D) {
					BIF_ThemeColor(TH_BACK);
					glRecti(10, 25, 250, 40);
	
					if(G.vd->persp==0) {
						window_to_3d(dvec, mvalo[0]-x1, mvalo[1]-y1);
	
						sprintf(str, "X %.4f  Y %.4f  Z %.4f  Dia %.4f", dvec[0], dvec[1], dvec[2], sqrt(dvec[0]*dvec[0]+dvec[1]*dvec[1]+dvec[2]*dvec[2]));
						glColor3f(0.0, 0.0, 0.0); 
						glRasterPos2i(15,  27);
						BMF_DrawString(G.fonts, str);
						glColor3f(0.7, 0.7, 0.7); 
						glRasterPos2i(16,  28);
						BMF_DrawString(G.fonts, str);
					}
					else if(G.vd->persp==2) {
						rcti vb;
	
						calc_viewborder(G.vd, &vb);
	
						fac1= (mvalo[0]-x1)/( (float) (vb.xmax-vb.xmin) );
						fac1*= 0.01*G.scene->r.size*G.scene->r.xsch;
						
						fac2= (mvalo[1]-y1)/( (float) (vb.ymax-vb.ymin) );
						fac2*= 0.01*G.scene->r.size*G.scene->r.ysch;
						
						sprintf(str, "X %.1f  Y %.1f  Dia %.1f", fabs(fac1), fabs(fac2), sqrt(fac1*fac1 + fac2*fac2) );
						glColor3f(0.0, 0.0, 0.0); 
						glRasterPos2i(15,  27);
						BMF_DrawString(G.fonts, str);
						glColor3f(0.7, 0.7, 0.7); 
						glRasterPos2i(16,  28);
						BMF_DrawString(G.fonts, str);
					}
				}
				else if(curarea->spacetype==SPACE_IPO) {
					SpaceIpo *sipo= curarea->spacedata.first;
	
					BIF_ThemeColor(TH_BACK);
					glRecti(20, 30, 170, 40);
								
					mvalo[2]= x1;
					mvalo[3]= y1;
					areamouseco_to_ipoco(&sipo->v2d, mval, dvec, dvec+1);
					areamouseco_to_ipoco(&sipo->v2d, mvalo+2, dvec+2, dvec+3);

					sprintf(str, "Time: %.4f  Y %.4f", dvec[0]-dvec[2], dvec[1]-dvec[3]);
					glRasterPos2i(30,  30);
					glColor3f(0.0, 0.0, 0.0); 
					BMF_DrawString(G.fonts, str);
					glRasterPos2i(31,  31);
					glColor3f(0.9, 0.9, 0.9); 
					BMF_DrawString(G.fonts, str);
				}

				glFlush();

				mvalo[0]= mval[0];
				mvalo[1]= mval[1];
			}
			
			event= extern_qread(&val);
			
			if(event && val==0) {
				/* still because of the renderwindow... */
				persp(PERSP_VIEW);
				mywinset(curarea->win);
				persp(PERSP_WIN);
				
				if(event==ESCKEY) {
					retval= 0;
					break;
				}
				else if(event==LEFTMOUSE) break;
				else if(event==MIDDLEMOUSE) break;
				else if(event==RIGHTMOUSE) break;
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
		setlinestyle(0);
		switch (curarea->spacetype) {
		case SPACE_VIEW3D:
			if (G.obedit) {
				if ELEM4(G.obedit->type, OB_MESH, OB_CURVE, OB_SURF, OB_LATTICE) {
					circle_selectCB(&obedit_selectionCB);
					return 0;
				}
			}
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

	if(mval==NULL && mvalo==NULL) {	/* signal */
		no_mvalo= 1;
		return;
	}

	persp(PERSP_WIN);
	glReadBuffer(GL_FRONT);
	glDrawBuffer(GL_FRONT);
	//setlinestyle(2);

	/* draw circle */
	if(mvalo && no_mvalo==0) {
		fdrawXORcirc(mvalo[0], mvalo[1], rado);
	}
	
	if(mval) {
		fdrawXORcirc(mval[0], mval[1], rad);
	}
	//setlinestyle(0);

	glFlush();
	persp(PERSP_VIEW);
	glDrawBuffer(GL_BACK);
	glReadBuffer(GL_BACK);

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
	int escape= 0;
	unsigned short event;
	short mvalo[2], mval[2], val;
	short selecting=0;
	Object *obj;
	
	if(G.obedit) obj = G.obedit;
	else obj = OBACT;

	mywinset(curarea->win);
	
	getmouseco_areawin(mvalo);
	mval[0]= mvalo[0]; mval[1]= mvalo[1];

	draw_sel_circle(mval, NULL, rad, 0.0, selecting); // draws frontbuffer, but sets backbuf again
	
	rado= rad;
	
	while(TRUE) {
		
		if(mval[0]!=mvalo[0] || mval[1]!=mvalo[1] || rado!=rad || firsttime) {
			firsttime= 0;
			
			if(selecting) {
				callback(selecting, obj, mval, rad);
			}

			draw_sel_circle(mval, mvalo, rad, rado, selecting);
		
			mvalo[0]= mval[0];
			mvalo[1]= mval[1];
			rado= rad;

		}
		
		while(qtest()) {
			event= extern_qread(&val);
			if (event) {

				/* for when another window is open and a mouse cursor activates it */
				if(event!=MOUSEY && event!=MOUSEX) mywinset(curarea->win);
				
				getmouseco_areawin(mval);	// important to do here, trust events!
				
				switch(event) {
			
				case LEFTMOUSE:
				case MIDDLEMOUSE:
					if(val) selecting= event;
					else selecting= 0;
					firsttime= 1;
					
					break;
				case WHEELDOWNMOUSE:
				case PADPLUSKEY:
				case EQUALKEY:
					if(val) if(rad<200.0) rad*= 1.2;
					break;
				case WHEELUPMOUSE:
				case PADMINUS:
				case MINUSKEY:
					if(val) if(rad>5.0) rad/= 1.2;
					break;
				
				case ESCKEY: case SPACEKEY: case RIGHTMOUSE: case INPUTCHANGE: 
				case GKEY: case SKEY: case RKEY: case XKEY: case EKEY: case TABKEY:
					escape= 1;
					break;

				}
				
				if(escape) break;
			}
		}
		PIL_sleep_ms(10);
		
		if(escape) break;
	}
	
	/* clear circle */
	draw_sel_circle(NULL, mvalo, 0, rad, 1);
	BIF_undo_push("Circle Select");
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
			
			/* note; do not make disp or get derived mesh here. spoils dependency order */
			if (me->flag & ME_SUBSURF) {
					/* approximate counts, about right for a mesh entirely made 
					 * of quads and that is a closed 2-manifold.
					 */
				totvert= totface= me->totface*1<<(me->subdiv*2);
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
		if(cu->disp.first)
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

/* countall does statistics */
/* is called on most actions, like select/add/delete/layermove */
void countall()
{
	extern ListBase editNurb;
	Base *base;
	Object *ob;
	Mesh *me;
	Nurb *nu;
	BezTriple *bezt;
	BPoint *bp;
	MetaElem *ml;
	struct EditBone *ebo;
	int a;

	G.totvert= G.totvertsel= G.totedge= G.totedgesel= G.totfacesel= G.totface= G.totobj= 
	    G.totmesh= G.totlamp= G.totcurve= G.totobjsel= G.totbone= G.totbonesel=  0;

	if(G.obedit) {
		
		if(G.obedit->type==OB_MESH) {
			EditMesh *em = G.editMesh;
			EditVert *eve;
			EditEdge *eed;
			EditFace *efa;
			
			for(eve= em->verts.first; eve; eve= eve->next) {
				G.totvert++;
				if(eve->f & SELECT) G.totvertsel++;
			}
			for(eed= em->edges.first; eed; eed= eed->next) {
				G.totedge++;
				if(eed->f & SELECT) G.totedgesel++;
			}
			for(efa= em->faces.first; efa; efa= efa->next) {
				G.totface++;
				if(efa->f & SELECT) G.totfacesel++;
			}
		}
		else if (G.obedit->type==OB_ARMATURE){
			for (ebo=G.edbo.first;ebo;ebo=ebo->next){
				G.totbone++;
				
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
				
				if(ebo->flag & BONE_SELECTED) G.totbonesel++;

				//	If this is an IK child and it's parent is being moved, remove our root
				if ((ebo->flag & BONE_IK_TOPARENT)&& (ebo->flag & BONE_ROOTSEL) && ebo->parent && (ebo->parent->flag & BONE_TIPSEL)){
					G.totvertsel--;
				}

				G.totvert+=2;
			}
		}
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
			/* editmball.c */
			extern ListBase editelems;  /* go away ! */
			
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
	else if(G.obpose) {
		if(G.obpose->pose) {
			bPoseChannel *pchan;
			for(pchan= G.obpose->pose->chanbase.first; pchan; pchan= pchan->next) {
				G.totbone++;
				if(pchan->bone && (pchan->bone->flag & BONE_SELECTED)) G.totbonesel++;
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

	if(G.scene==NULL) return;

	base= (G.scene->base.first);
	while(base) {
		if(G.scene->lay & base->lay) {
			
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

/* ************************************************** */
/* ********************* old transform stuff ******** */
/* ************************************************** */

static TransVert *transvmain=NULL;
static int tottrans= 0;

/* copied from editobject.c, now uses (almost) proper depgraph */
static void special_transvert_update(void)
{
	
	if(G.obedit) {
		
		DAG_object_flush_update(G.scene, G.obedit, OB_RECALC_DATA);
		
		if(G.obedit->type==OB_MESH) {
			recalc_editnormals();	// does face centers too
		}
		else if ELEM(G.obedit->type, OB_CURVE, OB_SURF) {
			extern ListBase editNurb;
			Nurb *nu= editNurb.first;
			while(nu) {
				test2DNurb(nu);
				testhandlesNurb(nu); /* test for bezier too */
				nu= nu->next;
			}
		}
		else if(G.obedit->type==OB_ARMATURE){
			EditBone *ebo;
			
			/* Ensure all bones are correctly adjusted */
			for (ebo=G.edbo.first; ebo; ebo=ebo->next){
				
				if ((ebo->flag & BONE_IK_TOPARENT) && ebo->parent){
					/* If this bone has a parent tip that has been moved */
					if (ebo->parent->flag & BONE_TIPSEL){
						VECCOPY (ebo->head, ebo->parent->tail);
					}
					/* If this bone has a parent tip that has NOT been moved */
					else{
						VECCOPY (ebo->parent->tail, ebo->head);
					}
				}
			}
		}
		else if(G.obedit->type==OB_LATTICE) {
			if(editLatt->flag & LT_OUTSIDE) outside_lattice(editLatt);
		}
	}
}

/* copied from editobject.c, needs to be replaced with new transform code still */
/* mode: 1 = proportional */
static void make_trans_verts(float *min, float *max, int mode)	
{
	extern ListBase editNurb;
	EditMesh *em = G.editMesh;
	Nurb *nu;
	BezTriple *bezt;
	BPoint *bp;
	TransVert *tv=NULL;
	MetaElem *ml;
	EditVert *eve;
	EditBone	*ebo;
	float total, centre[3], centroid[3];
	int a;

	tottrans= 0; // global!
	
	INIT_MINMAX(min, max);
	centroid[0]=centroid[1]=centroid[2]= 0.0;
	
	/* note for transform refactor: dont rely on countall anymore... its ancient */
	/* I skip it for editmesh now (ton) */
	if(G.obedit->type!=OB_MESH) {
		countall();
		if(mode) tottrans= G.totvert;
		else tottrans= G.totvertsel;

		if(G.totvertsel==0) {
			tottrans= 0;
			return;
		}
		tv=transvmain= MEM_callocN(tottrans*sizeof(TransVert), "maketransverts");
	}
	
	/* we count again because of hide (old, not for mesh!) */
	tottrans= 0;
	
	if(G.obedit->type==OB_MESH) {
		int proptrans= 0;
		
		// transform now requires awareness for select mode, so we tag the f1 flags in verts
		tottrans= 0;
		if(G.scene->selectmode & SCE_SELECT_VERTEX) {
			for(eve= em->verts.first; eve; eve= eve->next) {
				if(eve->h==0 && (eve->f & SELECT)) {
					eve->f1= SELECT;
					tottrans++;
				}
				else eve->f1= 0;
			}
		}
		else if(G.scene->selectmode & SCE_SELECT_EDGE) {
			EditEdge *eed;
			for(eve= em->verts.first; eve; eve= eve->next) eve->f1= 0;
			for(eed= em->edges.first; eed; eed= eed->next) {
				if(eed->h==0 && (eed->f & SELECT)) eed->v1->f1= eed->v2->f1= SELECT;
			}
			for(eve= em->verts.first; eve; eve= eve->next) if(eve->f1) tottrans++;
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
			for(eve= em->verts.first; eve; eve= eve->next) if(eve->f1) tottrans++;
		}
		
		/* proportional edit exception... */
		if(mode==1 && tottrans) {
			for(eve= em->verts.first; eve; eve= eve->next) {
				if(eve->h==0) {
					eve->f1 |= 2;
					proptrans++;
				}
			}
			if(proptrans>tottrans) tottrans= proptrans;
		}
		
		/* and now make transverts */
		if(tottrans) {
			tv=transvmain= MEM_callocN(tottrans*sizeof(TransVert), "maketransverts");

			for(eve= em->verts.first; eve; eve= eve->next) {
				if(eve->f1) {
					VECCOPY(tv->oldloc, eve->co);
					tv->loc= eve->co;
					if(eve->no[0]!=0.0 || eve->no[1]!=0.0 ||eve->no[2]!=0.0)
						tv->nor= eve->no; // note this is a hackish signal (ton)
					tv->flag= eve->f1 & SELECT;
					tv++;
				}
			}
		}
	}
	else if (G.obedit->type==OB_ARMATURE){
		for (ebo=G.edbo.first;ebo;ebo=ebo->next){
			if (ebo->flag & BONE_TIPSEL){
				VECCOPY (tv->oldloc, ebo->tail);
				tv->loc= ebo->tail;
				tv->nor= NULL;
				tv->flag= 1;
				tv++;
				tottrans++;
			}

			/*  Only add the root if there is no selected IK parent */
			if (ebo->flag & BONE_ROOTSEL){
				if (!(ebo->parent && (ebo->flag & BONE_IK_TOPARENT) && ebo->parent->flag & BONE_TIPSEL)){
					VECCOPY (tv->oldloc, ebo->head);
					tv->loc= ebo->head;
					tv->nor= NULL;
					tv->flag= 1;
					tv++;
					tottrans++;
				}		
			}
			
		}
	}
	else if ELEM(G.obedit->type, OB_CURVE, OB_SURF) {
		nu= editNurb.first;
		while(nu) {
			if((nu->type & 7)==CU_BEZIER) {
				a= nu->pntsu;
				bezt= nu->bezt;
				while(a--) {
					if(bezt->hide==0) {
						if(mode==1 || (bezt->f1 & 1)) {
							VECCOPY(tv->oldloc, bezt->vec[0]);
							tv->loc= bezt->vec[0];
							tv->flag= bezt->f1 & 1;
							tv++;
							tottrans++;
						}
						if(mode==1 || (bezt->f2 & 1)) {
							VECCOPY(tv->oldloc, bezt->vec[1]);
							tv->loc= bezt->vec[1];
							tv->val= &(bezt->alfa);
							tv->oldval= bezt->alfa;
							tv->flag= bezt->f2 & 1;
							tv++;
							tottrans++;
						}
						if(mode==1 || (bezt->f3 & 1)) {
							VECCOPY(tv->oldloc, bezt->vec[2]);
							tv->loc= bezt->vec[2];
							tv->flag= bezt->f3 & 1;
							tv++;
							tottrans++;
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
							VECCOPY(tv->oldloc, bp->vec);
							tv->loc= bp->vec;
							tv->val= &(bp->alfa);
							tv->oldval= bp->alfa;
							tv->flag= bp->f1 & 1;
							tv++;
							tottrans++;
						}
					}
					bp++;
				}
			}
			nu= nu->next;
		}
	}
	else if(G.obedit->type==OB_MBALL) {
		extern ListBase editelems;  /* go away ! */
		ml= editelems.first;
		while(ml) {
			if(ml->flag & SELECT) {
				tv->loc= &ml->x;
				VECCOPY(tv->oldloc, tv->loc);
				tv->val= &(ml->rad);
				tv->oldval= ml->rad;
				tv->flag= 1;
				tv++;
				tottrans++;
			}
			ml= ml->next;
		}
	}
	else if(G.obedit->type==OB_LATTICE) {
		bp= editLatt->def;
		
		a= editLatt->pntsu*editLatt->pntsv*editLatt->pntsw;
		
		while(a--) {
			if(mode==1 || (bp->f1 & 1)) {
				if(bp->hide==0) {
					VECCOPY(tv->oldloc, bp->vec);
					tv->loc= bp->vec;
					tv->flag= bp->f1 & 1;
					tv++;
					tottrans++;
				}
			}
			bp++;
		}
	}
	
	/* cent etc */
	tv= transvmain;
	total= 0.0;
	for(a=0; a<tottrans; a++, tv++) {
		if(tv->flag & SELECT) {
			centroid[0]+= tv->oldloc[0];
			centroid[1]+= tv->oldloc[1];
			centroid[2]+= tv->oldloc[2];
			total+= 1.0;
			DO_MINMAX(tv->oldloc, min, max);
		}
	}
	if(total!=0.0) {
		centroid[0]/= total;
		centroid[1]/= total;
		centroid[2]/= total;
	}

	centre[0]= (min[0]+max[0])/2.0;
	centre[1]= (min[1]+max[1])/2.0;
	centre[2]= (min[2]+max[2])/2.0;
	
}

void snap_sel_to_grid()
{
	extern float originmat[3][3];	/* object.c */
	TransVert *tv;
	Base *base;
	Object *ob;
	float gridf, imat[3][3], bmat[3][3], vec[3];
	int a;

	gridf= G.vd->gridview;


	if(G.obedit) {
		tottrans= 0;
		
		if ELEM6(G.obedit->type, OB_ARMATURE, OB_LATTICE, OB_MESH, OB_SURF, OB_CURVE, OB_MBALL) 
			make_trans_verts(bmat[0], bmat[1], 0);
		if(tottrans==0) return;

		Mat3CpyMat4(bmat, G.obedit->obmat);
		Mat3Inv(imat, bmat);

		tv= transvmain;
		for(a=0; a<tottrans; a++, tv++) {

			VECCOPY(vec, tv->loc);
			Mat3MulVecfl(bmat, vec);
			VecAddf(vec, vec, G.obedit->obmat[3]);
			vec[0]= G.vd->gridview*floor(.5+ vec[0]/gridf);
			vec[1]= G.vd->gridview*floor(.5+ vec[1]/gridf);
			vec[2]= G.vd->gridview*floor(.5+ vec[2]/gridf);
			VecSubf(vec, vec, G.obedit->obmat[3]);

			Mat3MulVecfl(imat, vec);
			VECCOPY(tv->loc, vec);

		}

		MEM_freeN(transvmain);
		transvmain= 0;
		
		special_transvert_update();

		allqueue(REDRAWVIEW3D, 0);
		return;
	}

	if (G.obpose){
		allqueue(REDRAWVIEW3D, 0);
		return;
	}

	base= (G.scene->base.first);
	while(base) {
		if( ( ((base)->flag & SELECT) && ((base)->lay & G.vd->lay) && ((base)->object->id.lib==0))) {
			ob= base->object;
			ob->recalc |= OB_RECALC_OB;
			
			vec[0]= -ob->obmat[3][0]+G.vd->gridview*floor(.5+ ob->obmat[3][0]/gridf);
			vec[1]= -ob->obmat[3][1]+G.vd->gridview*floor(.5+ ob->obmat[3][1]/gridf);
			vec[2]= -ob->obmat[3][2]+G.vd->gridview*floor(.5+ ob->obmat[3][2]/gridf);

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
	DAG_scene_flush_update(G.scene);
	allqueue(REDRAWVIEW3D, 0);
}

void snap_sel_to_curs()
{
	extern float originmat[3][3];	/* object.c */
	TransVert *tv;
	Base *base;
	Object *ob;
	float *curs, imat[3][3], bmat[3][3], vec[3];
	int a;

	curs= give_cursor();

	if(G.obedit) {
		tottrans= 0;

		if ELEM6(G.obedit->type, OB_ARMATURE, OB_LATTICE, OB_MESH, OB_SURF, OB_CURVE, OB_MBALL) 
			make_trans_verts(bmat[0], bmat[1], 0);
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

		special_transvert_update();

		allqueue(REDRAWVIEW3D, 0);
		return;
	}

	if (G.obpose){
		allqueue(REDRAWVIEW3D, 0);
		return;
	}

	base= (G.scene->base.first);
	while(base) {
		if( ( ((base)->flag & SELECT) && ((base)->lay & G.vd->lay) && ((base)->object->id.lib==0))) {
			ob= base->object;
			ob->recalc |= OB_RECALC_OB;
			
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
	DAG_scene_flush_update(G.scene);
	allqueue(REDRAWVIEW3D, 0);
}

void snap_curs_to_grid()
{
	float gridf, *curs;

	gridf= G.vd->gridview;
	curs= give_cursor();

	curs[0]= G.vd->gridview*floor(.5+curs[0]/gridf);
	curs[1]= G.vd->gridview*floor(.5+curs[1]/gridf);
	curs[2]= G.vd->gridview*floor(.5+curs[2]/gridf);

	allqueue(REDRAWVIEW3D, 0);

}

void snap_curs_to_sel()
{
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

		if ELEM6(G.obedit->type, OB_ARMATURE, OB_LATTICE, OB_MESH, OB_SURF, OB_CURVE, OB_MBALL) 
			make_trans_verts(bmat[0], bmat[1], 0);
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

		if ELEM6(G.obedit->type, OB_ARMATURE, OB_LATTICE, OB_MESH, OB_SURF, OB_CURVE, OB_MBALL) 
			make_trans_verts(bmat[0], bmat[1], 0);
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
		tottrans= 0;

		if ELEM6(G.obedit->type, OB_ARMATURE, OB_LATTICE, OB_MESH, OB_SURF, OB_CURVE, OB_MBALL) 
			make_trans_verts(bmat[0], bmat[1], 0);
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
		tottrans= 0;

		if ELEM6(G.obedit->type, OB_ARMATURE, OB_LATTICE, OB_MESH, OB_SURF, OB_CURVE, OB_MBALL) 
			make_trans_verts(bmat[0], bmat[1], 0);
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

		special_transvert_update();

		allqueue(REDRAWVIEW3D, 0);
		return;
	}

	if (G.obpose){
		allqueue(REDRAWVIEW3D, 0);
		return;
	}

	base= (G.scene->base.first);
	while(base) {
		if( ( ((base)->flag & SELECT) && ((base)->lay & G.vd->lay) && ((base)->object->id.lib==0))) {
			ob= base->object;
			ob->recalc |= OB_RECALC_OB;
			
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
	DAG_scene_flush_update(G.scene);
	allqueue(REDRAWVIEW3D, 0);
}


void snapmenu()
{
	short event;

	event = pupmenu("Snap %t|Selection -> Grid%x1|Selection -> Cursor%x2|Cursor-> Grid%x3|Cursor-> Selection%x4|Selection-> Center%x5");

	switch (event) {
		case 1: /*Selection to grid*/
		    snap_sel_to_grid();
			BIF_undo_push("Snap selection to grid");
		    break;
		case 2: /*Selection to cursor*/
		    snap_sel_to_curs();
			BIF_undo_push("Snap selection to cursor");
		    break;	    
		case 3: /*Cursor to grid*/
		    snap_curs_to_grid();
		    break;
		case 4: /*Cursor to selection*/
		    snap_curs_to_sel();
		    break;
		case 5: /*Selection to center of selection*/
		    snap_to_center();
			BIF_undo_push("Snap selection to center");
		    break;
	}
}


void mergemenu(void)
{
	extern float doublimit;
	short event;

	event = pupmenu("Merge %t|At Center%x1|At Cursor%x2");

	if (event==-1) return; /* Return if the menu is closed without any choices */

	if (event==1) 
		snap_to_center(); /*Merge at Center*/
	else
		snap_sel_to_curs(); /*Merge at Cursor*/

	notice("Removed %d Vertices", removedoublesflag(1, doublimit));
	allqueue(REDRAWVIEW3D, 0);
	countall();
	BIF_undo_push("Merge"); /* push the mesh down the undo pipe */

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
	else if(!(G.obpose)){
		adduplicate(0);
	}
}

void toggle_shading(void) 
{
	if(G.qual & LR_SHIFTKEY) {
		if(G.qual & LR_ALTKEY) {
			reshadeall_displist();
			G.vd->drawtype= OB_SHADED;
		}
		else {
			if(G.vd->drawtype== OB_SHADED) G.vd->drawtype= OB_WIRE;
			else G.vd->drawtype= OB_SHADED;
		}
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
	TransVert *tv;
	float centroid[3], vec[3], bmat[3][3];
	int a;

	tottrans=0;
	if ELEM5(G.obedit->type, OB_ARMATURE, OB_LATTICE, OB_MESH, OB_SURF, OB_CURVE) 
		make_trans_verts(bmat[0], bmat[1], 0);
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

