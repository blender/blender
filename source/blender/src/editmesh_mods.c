/**
 * $Id: 
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
 * The Original Code is Copyright (C) 2004 by NaN Holding BV.
 * All rights reserved.
 *
 * The Original Code is: all of this file.
 *
 * Contributor(s): none yet.
 *
 * ***** END GPL/BL DUAL LICENSE BLOCK *****
 */

/*

editmesh_mods.c, UI level access, no geometry changes 

*/

#include <stdlib.h>
#include <string.h>
#include <math.h>

#ifdef HAVE_CONFIG_H
#include <config.h>
#endif

#ifdef WIN32
#include "BLI_winstuff.h"
#endif
#include "MEM_guardedalloc.h"


#include "DNA_mesh_types.h"
#include "DNA_material_types.h"
#include "DNA_meshdata_types.h"
#include "DNA_object_types.h"
#include "DNA_texture_types.h"
#include "DNA_scene_types.h"
#include "DNA_screen_types.h"
#include "DNA_view3d_types.h"

#include "BLI_blenlib.h"
#include "BLI_arithb.h"
#include "BLI_editVert.h"
#include "BLI_rand.h"

#include "BKE_displist.h"
#include "BKE_global.h"
#include "BKE_mesh.h"
#include "BKE_material.h"
#include "BKE_texture.h"
#include "BKE_utildefines.h"

#include "BIF_editmesh.h"
#include "BIF_resources.h"
#include "BIF_gl.h"
#include "BIF_graphics.h"
#include "BIF_interface.h"
#include "BIF_mywindow.h"
#include "BIF_screen.h"
#include "BIF_space.h"
#include "BIF_toolbox.h"

#include "BDR_drawobject.h"
#include "BDR_editobject.h"

#include "BSE_view.h"
#include "BSE_edit.h"

#include "mydevice.h"
#include "blendef.h"
#include "render.h"  // externtex, badlevel call (ton)

#include "editmesh.h"

/* **************************** MODS ************************* */

void selectswap_mesh(void)
{
	EditMesh *em = G.editMesh;
	EditVert *eve;

	eve= em->verts.first;
	while(eve) {
		if(eve->h==0) {
			if(eve->f & 1) eve->f&= ~1;
			else eve->f|= 1;
		}
		eve= eve->next;
	}
	countall();
	allqueue(REDRAWVIEW3D, 0);

}




void deselectall_mesh(void)	/* toggle */
{
	EditMesh *em = G.editMesh;
	EditVert *eve;
	int a;
	
	if(G.obedit->lay & G.vd->lay) {
		a= 0;
		eve= em->verts.first;
		while(eve) {
			if(eve->f & 1) {
				a= 1;
				break;
			}
			eve= eve->next;
		}
		
		if (a) undo_push_mesh("Deselect All");
		else undo_push_mesh("Select All");
		
		eve= em->verts.first;
		while(eve) {
			if(eve->h==0) {
				if(a) eve->f&= -2;
				else eve->f|= 1;
			}
			eve= eve->next;
		}
	}
	countall();
	allqueue(REDRAWVIEW3D, 0);
}


void righthandfaces(int select)	/* makes faces righthand turning */
{
	EditMesh *em = G.editMesh;
	EditEdge *eed, *ed1, *ed2, *ed3, *ed4;
	EditFace *efa, *startvl;
	float maxx, nor[3], cent[3];
	int totsel, found, foundone, direct, turn, tria_nr;

   /* based at a select-connected to witness loose objects */

	/* count per edge the amount of faces */

	/* find the ultimate left, front, upper face (not manhattan dist!!) */
	/* also evaluate both triangle cases in quad, since these can be non-flat */

	/* put normal to the outside, and set the first direction flags in edges */

	/* then check the object, and set directions / direction-flags: but only for edges with 1 or 2 faces */
	/* this is in fact the 'select connected' */
	
	/* in case (selected) faces were not done: start over with 'find the ultimate ...' */

	waitcursor(1);
	
	eed= em->edges.first;
	while(eed) {
		eed->f= 0;
		eed->f1= 0;
		eed= eed->next;
	}

	/* count faces and edges */
	totsel= 0;
	efa= em->faces.first;
	while(efa) {
		if(select==0 || faceselectedAND(efa, 1) ) {
			efa->f= 1;
			totsel++;
			efa->e1->f1++;
			efa->e2->f1++;
			efa->e3->f1++;
			if(efa->v4) efa->e4->f1++;
		}
		else efa->f= 0;

		efa= efa->next;
	}

	while(totsel>0) {
		/* from the outside to the inside */

		efa= em->faces.first;
		startvl= NULL;
		maxx= -1.0e10;
		tria_nr= 0;

		while(efa) {
			if(efa->f) {
				CalcCent3f(cent, efa->v1->co, efa->v2->co, efa->v3->co);
				cent[0]= cent[0]*cent[0] + cent[1]*cent[1] + cent[2]*cent[2];
				
				if(cent[0]>maxx) {
					maxx= cent[0];
					startvl= efa;
					tria_nr= 0;
				}
				if(efa->v4) {
					CalcCent3f(cent, efa->v1->co, efa->v3->co, efa->v4->co);
					cent[0]= cent[0]*cent[0] + cent[1]*cent[1] + cent[2]*cent[2];
					
					if(cent[0]>maxx) {
						maxx= cent[0];
						startvl= efa;
						tria_nr= 1;
					}
				}
			}
			efa= efa->next;
		}
		
		/* set first face correct: calc normal */
		
		if(tria_nr==1) {
			CalcNormFloat(startvl->v1->co, startvl->v3->co, startvl->v4->co, nor);
			CalcCent3f(cent, startvl->v1->co, startvl->v3->co, startvl->v4->co);
		} else {
			CalcNormFloat(startvl->v1->co, startvl->v2->co, startvl->v3->co, nor);
			CalcCent3f(cent, startvl->v1->co, startvl->v2->co, startvl->v3->co);
		}
		/* first normal is oriented this way or the other */
		if(select) {
			if(select==2) {
				if(cent[0]*nor[0]+cent[1]*nor[1]+cent[2]*nor[2] > 0.0) flipface(startvl);
			}
			else {
				if(cent[0]*nor[0]+cent[1]*nor[1]+cent[2]*nor[2] < 0.0) flipface(startvl);
			}
		}
		else if(cent[0]*nor[0]+cent[1]*nor[1]+cent[2]*nor[2] < 0.0) flipface(startvl);


		eed= startvl->e1;
		if(eed->v1==startvl->v1) eed->f= 1; 
		else eed->f= 2;
		
		eed= startvl->e2;
		if(eed->v1==startvl->v2) eed->f= 1; 
		else eed->f= 2;
		
		eed= startvl->e3;
		if(eed->v1==startvl->v3) eed->f= 1; 
		else eed->f= 2;
		
		eed= startvl->e4;
		if(eed) {
			if(eed->v1==startvl->v4) eed->f= 1; 
			else eed->f= 2;
		}
		
		startvl->f= 0;
		totsel--;

		/* test normals */
		found= 1;
		direct= 1;
		while(found) {
			found= 0;
			if(direct) efa= em->faces.first;
			else efa= em->faces.last;
			while(efa) {
				if(efa->f) {
					turn= 0;
					foundone= 0;

					ed1= efa->e1;
					ed2= efa->e2;
					ed3= efa->e3;
					ed4= efa->e4;

					if(ed1->f) {
						if(ed1->v1==efa->v1 && ed1->f==1) turn= 1;
						if(ed1->v2==efa->v1 && ed1->f==2) turn= 1;
						foundone= 1;
					}
					else if(ed2->f) {
						if(ed2->v1==efa->v2 && ed2->f==1) turn= 1;
						if(ed2->v2==efa->v2 && ed2->f==2) turn= 1;
						foundone= 1;
					}
					else if(ed3->f) {
						if(ed3->v1==efa->v3 && ed3->f==1) turn= 1;
						if(ed3->v2==efa->v3 && ed3->f==2) turn= 1;
						foundone= 1;
					}
					else if(ed4 && ed4->f) {
						if(ed4->v1==efa->v4 && ed4->f==1) turn= 1;
						if(ed4->v2==efa->v4 && ed4->f==2) turn= 1;
						foundone= 1;
					}

					if(foundone) {
						found= 1;
						totsel--;
						efa->f= 0;

						if(turn) {
							if(ed1->v1==efa->v1) ed1->f= 2; 
							else ed1->f= 1;
							if(ed2->v1==efa->v2) ed2->f= 2; 
							else ed2->f= 1;
							if(ed3->v1==efa->v3) ed3->f= 2; 
							else ed3->f= 1;
							if(ed4) {
								if(ed4->v1==efa->v4) ed4->f= 2; 
								else ed4->f= 1;
							}

							flipface(efa);

						}
						else {
							if(ed1->v1== efa->v1) ed1->f= 1; 
							else ed1->f= 2;
							if(ed2->v1==efa->v2) ed2->f= 1; 
							else ed2->f= 2;
							if(ed3->v1==efa->v3) ed3->f= 1; 
							else ed3->f= 2;
							if(ed4) {
								if(ed4->v1==efa->v4) ed4->f= 1; 
								else ed4->f= 2;
							}
						}
					}
				}
				if(direct) efa= efa->next;
				else efa= efa->prev;
			}
			direct= 1-direct;
		}
	}

	recalc_editnormals();
	
	makeDispList(G.obedit);
	
	waitcursor(0);
}

static EditVert *findnearestvert(short sel)
{
	EditMesh *em = G.editMesh;
	/* if sel==1 the vertices with flag==1 get a disadvantage */
	EditVert *eve,*act=0;
	static EditVert *acto=0;
	short dist=100,temp,mval[2];

	if(em->verts.first==0) return 0;

	/* do projection */
	calc_meshverts_ext();	/* drawobject.c */
	
	/* we count from acto->next to last, and from first to acto */
	/* does acto exist? */
	eve= em->verts.first;
	while(eve) {
		if(eve==acto) break;
		eve= eve->next;
	}
	if(eve==0) acto= em->verts.first;

	if(acto==0) return 0;

	/* is there an indicated vertex? part 1 */
	getmouseco_areawin(mval);
	eve= acto->next;
	while(eve) {
		if(eve->h==0) {
			temp= abs(mval[0]- eve->xs)+ abs(mval[1]- eve->ys);
			if( (eve->f & 1)==sel ) temp+=5;
			if(temp<dist) {
				act= eve;
				dist= temp;
				if(dist<4) break;
			}
		}
		eve= eve->next;
	}
	/* is there an indicated vertex? part 2 */
	if(dist>3) {
		eve= em->verts.first;
		while(eve) {
			if(eve->h==0) {
				temp= abs(mval[0]- eve->xs)+ abs(mval[1]- eve->ys);
				if( (eve->f & 1)==sel ) temp+=5;
				if(temp<dist) {
					act= eve;
					if(temp<4) break;
					dist= temp;
				}
				if(eve== acto) break;
			}
			eve= eve->next;
		}
	}

	acto= act;
	return act;
}


EditEdge *findnearestedge()
{
	EditMesh *em = G.editMesh;
	EditEdge *closest, *eed;
	EditVert *eve;
	short found=0, mval[2];
	float distance[2], v1[2], v2[2], mval2[2];
	
	if(em->edges.first==0) return NULL;
	else eed=em->edges.first;	
	
	/* reset flags */	
	for(eve=em->verts.first; eve; eve=eve->next){
		eve->f &= ~2;
	}	
		
	calc_meshverts_ext_f2();     	/*sets (eve->f & 2) for vertices that aren't visible*/
	getmouseco_areawin(mval);
	closest=NULL;
	
	mval2[0] = (float)mval[0];
	mval2[1] = (float)mval[1];
	
	eed=em->edges.first;
	/*compare the distance to the rest of the edges and find the closest one*/
	while(eed) {
		/* Are both vertices of the edge ofscreen or either of them hidden? then don't select the edge*/
		if( !((eed->v1->f & 2) && (eed->v2->f & 2)) && (eed->v1->h==0 && eed->v2->h==0)){
			v1[0] = eed->v1->xs;
			v1[1] = eed->v1->ys;
			v2[0] = eed->v2->xs;
			v2[1] = eed->v2->ys;
			
			distance[1] = PdistVL2Dfl(mval2, v1, v2);
			
			if(distance[1]<50){
				/*do we have to compare it to other distances? */    			
				if(found) {
					if (distance[1]<distance[0]){
						distance[0]=distance[1];
						/*save the current closest edge*/
						closest=eed;  	
					}
				} else {
					distance[0]=distance[1];
					closest=eed;
					found=1;
				}
			}
		}
		eed= eed->next;
	}
	
	/* reset flags */	
	for(eve=em->verts.first; eve; eve=eve->next){
		eve->f &= ~2;
	}
	
	if(found) return closest;
	else return 0;
}

static void edge_select(void)
{
	EditMesh *em = G.editMesh;
	EditEdge *closest=0;
	
	closest=findnearestedge();	
	
	if(closest){         /* Did we find anything that is selectable?*/

		if( (G.qual & LR_SHIFTKEY)==0) {
			EditVert *eve;			
			
			undo_push_mesh("Select Edge");
			/* deselectall */
			for(eve= em->verts.first; eve; eve= eve->next) eve->f&= ~1;

			/* select edge */
			closest->v1->f |= 1;
			closest->v2->f |= 1;
		}
		else {
			/*  both of the vertices are selected: deselect both*/
			if((closest->v1->f & 1) && (closest->v2->f & 1) ){  
				closest->v1->f &= ~1;
				closest->v2->f &= ~1;
			}
			else { 
				/* select both */
				closest->v1->f |= 1;
				closest->v2->f |= 1;
			}
		}
		countall();
		allqueue(REDRAWVIEW3D, 0);
	}
}

static void draw_vertices_special(int mode, EditVert *act) /* teken = draw */
{
	/* (only this view, no other windows) */
	/* hackish routine for visual speed:
	 * mode 0: deselect the selected ones, draw them, except act
	 * mode 1: only draw act
	 */
	EditMesh *em = G.editMesh;
	EditVert *eve;
	float size= BIF_GetThemeValuef(TH_VERTEX_SIZE);
	char col[3];
	
	glPointSize(size);

	persp(PERSP_VIEW);
	glPushMatrix();
	mymultmatrix(G.obedit->obmat);

	if(mode==0) {
		BIF_ThemeColor(TH_VERTEX);
		
		/* set zbuffer on, its default off outside main drawloops */
		if(G.vd->drawtype > OB_WIRE) {
			G.zbuf= 1;
			glEnable(GL_DEPTH_TEST);
		}

		glBegin(GL_POINTS);
		eve= (EditVert *)em->verts.first;
		while(eve) {
			if(eve->h==0) {
				if(eve!=act && (eve->f & 1)) {
					eve->f -= 1;
					glVertex3fv(eve->co);
				}
			}
			eve= eve->next;
		}
		glEnd();
		
		glDisable(GL_DEPTH_TEST);
		G.zbuf= 0;
	}
	
	/* draw active vertex */
	if(act->f & 1) BIF_GetThemeColor3ubv(TH_VERTEX_SELECT, col);
	else BIF_GetThemeColor3ubv(TH_VERTEX, col);
	
	glColor3ub(col[0], col[1], col[2]);

	glBegin(GL_POINTS);
	glVertex3fv(act->co);
	glEnd();
	
	glPointSize(1.0);
	glPopMatrix();

	
}

void mouse_mesh(void)
{
	EditVert *act=0;

	if(G.qual & LR_ALTKEY) {
		if (G.qual & LR_CTRLKEY) edge_select();
	}
	else {
	
		act= findnearestvert(1);
		if(act) {
			
			glDrawBuffer(GL_FRONT);

			undo_push_mesh("Select Vertex");

			if( (act->f & 1)==0) act->f+= 1;
			else if(G.qual & LR_SHIFTKEY) act->f-= 1;

			if((G.qual & LR_SHIFTKEY)==0) {
				draw_vertices_special(0, act);
			}
			else draw_vertices_special(1, act);

			countall();

			glFlush();
			glDrawBuffer(GL_BACK);
			
			/* signal that frontbuf differs from back */
			curarea->win_swap= WIN_FRONT_OK;
			
			if(G.f & (G_FACESELECT|G_DRAWFACES|G_DRAWEDGES)) {
				/* update full view later on */
				allqueue(REDRAWVIEW3D, 0);
			}
			else allqueue(REDRAWVIEW3D, curarea->win);	// all windows except this one
		}
	
		rightmouse_transform();
	}
}




static void selectconnectedAll(void)
{
	EditMesh *em = G.editMesh;
	EditVert *v1,*v2;
	EditEdge *eed;
	short flag=1,toggle=0;

	if(em->edges.first==0) return;
	
	undo_push_mesh("Select Connected (All)");

	while(flag==1) {
		flag= 0;
		toggle++;
		if(toggle & 1) eed= em->edges.first;
		else eed= em->edges.last;
		while(eed) {
			v1= eed->v1;
			v2= eed->v2;
			if(eed->h==0) {
				if(v1->f & 1) {
					if( (v2->f & 1)==0 ) {
						v2->f |= 1;
						flag= 1;
					}
				}
				else if(v2->f & 1) {
					if( (v1->f & 1)==0 ) {
						v1->f |= 1;
						flag= 1;
					}
				}
			}
			if(toggle & 1) eed= eed->next;
			else eed= eed->prev;
		}
	}
	countall();

	allqueue(REDRAWVIEW3D, 0);

}

void selectconnected_mesh(int qual)
{
	EditMesh *em = G.editMesh;
	EditVert *eve,*v1,*v2,*act= 0;
	EditEdge *eed;
	short flag=1,sel,toggle=0;

	if(em->edges.first==0) return;

	if(qual & LR_CTRLKEY) {
		selectconnectedAll();
		return;
	}

	sel= 3;
	if(qual & LR_SHIFTKEY) sel=2;
	
	act= findnearestvert(sel-2);
	if(act==0) {
		error("Nothing indicated ");
		return;
	}
	
	undo_push_mesh("Select Linked");
	/* clear test flags */
	eve= em->verts.first;
	while(eve) {
		eve->f&= ~2;
		eve= eve->next;
	}
	act->f= (act->f & ~3) | sel;

	while(flag==1) {
		flag= 0;
		toggle++;
		if(toggle & 1) eed= em->edges.first;
		else eed= em->edges.last;
		while(eed) {
			v1= eed->v1;
			v2= eed->v2;
			if(eed->h==0) {
				if(v1->f & 2) {
					if( (v2->f & 2)==0 ) {
						v2->f= (v2->f & ~3) | sel;
						flag= 1;
					}
				}
				else if(v2->f & 2) {
					if( (v1->f & 2)==0 ) {
						v1->f= (v1->f & ~3) | sel;
						flag= 1;
					}
				}
			}
			if(toggle & 1) eed= eed->next;
			else eed= eed->prev;
		}
	}
	countall();
	
	allqueue(REDRAWVIEW3D, 0);
}


void vertexsmooth(void)
{
	EditMesh *em = G.editMesh;
	EditVert *eve;
	EditEdge *eed;
	float *adror, *adr, fac;
	float fvec[3];
	int teller=0;

	if(G.obedit==0) return;

	/* count */
	eve= em->verts.first;
	while(eve) {
		if(eve->f & 1) teller++;
		eve= eve->next;
	}
	if(teller==0) return;
	
	undo_push_mesh("Smooth");

	adr=adror= (float *)MEM_callocN(3*sizeof(float *)*teller, "vertsmooth");
	eve= em->verts.first;
	while(eve) {
		if(eve->f & 1) {
			eve->vn= (EditVert *)adr;
			eve->f1= 0;
			adr+= 3;
		}
		eve= eve->next;
	}
	
	eed= em->edges.first;
	while(eed) {
		if( (eed->v1->f & 1) || (eed->v2->f & 1) ) {
			fvec[0]= (eed->v1->co[0]+eed->v2->co[0])/2.0;
			fvec[1]= (eed->v1->co[1]+eed->v2->co[1])/2.0;
			fvec[2]= (eed->v1->co[2]+eed->v2->co[2])/2.0;
			
			if((eed->v1->f & 1) && eed->v1->f1<255) {
				eed->v1->f1++;
				VecAddf((float *)eed->v1->vn, (float *)eed->v1->vn, fvec);
			}
			if((eed->v2->f & 1) && eed->v2->f1<255) {
				eed->v2->f1++;
				VecAddf((float *)eed->v2->vn, (float *)eed->v2->vn, fvec);
			}
		}
		eed= eed->next;
	}

	eve= em->verts.first;
	while(eve) {
		if(eve->f & 1) {
			if(eve->f1) {
				adr= (float *)eve->vn;
				fac= 0.5/(float)eve->f1;
				
				eve->co[0]= 0.5*eve->co[0]+fac*adr[0];
				eve->co[1]= 0.5*eve->co[1]+fac*adr[1];
				eve->co[2]= 0.5*eve->co[2]+fac*adr[2];
			}
			eve->vn= 0;
		}
		eve= eve->next;
	}
	MEM_freeN(adror);

	allqueue(REDRAWVIEW3D, 0);
	makeDispList(G.obedit);
}

void vertexnoise(void)
{
	EditMesh *em = G.editMesh;
	extern float Tin;
	Material *ma;
	Tex *tex;
	EditVert *eve;
	float b2, ofs, vec[3];

	if(G.obedit==0) return;
	
	undo_push_mesh("Noise");
	
	ma= give_current_material(G.obedit, G.obedit->actcol);
	if(ma==0 || ma->mtex[0]==0 || ma->mtex[0]->tex==0) {
		return;
	}
	tex= ma->mtex[0]->tex;
	
	ofs= tex->turbul/200.0;
	
	eve= (struct EditVert *)em->verts.first;
	while(eve) {
		if(eve->f & 1) {
			
			if(tex->type==TEX_STUCCI) {
				
				b2= BLI_hnoise(tex->noisesize, eve->co[0], eve->co[1], eve->co[2]);
				if(tex->stype) ofs*=(b2*b2);
				vec[0]= 0.2*(b2-BLI_hnoise(tex->noisesize, eve->co[0]+ofs, eve->co[1], eve->co[2]));
				vec[1]= 0.2*(b2-BLI_hnoise(tex->noisesize, eve->co[0], eve->co[1]+ofs, eve->co[2]));
				vec[2]= 0.2*(b2-BLI_hnoise(tex->noisesize, eve->co[0], eve->co[1], eve->co[2]+ofs));
				
				VecAddf(eve->co, eve->co, vec);
			}
			else {
				
				externtex(ma->mtex[0], eve->co);
			
				eve->co[2]+= 0.05*Tin;
			}
		}
		eve= eve->next;
	}

	allqueue(REDRAWVIEW3D, 0);
	makeDispList(G.obedit);
}

void hide_mesh(int swap)
{
	EditMesh *em = G.editMesh;
	EditVert *eve;
	EditEdge *eed;

	if(G.obedit==0) return;

	if(swap) {
		eve= em->verts.first;
		while(eve) {
			if((eve->f & 1)==0) {
				eve->xs= 3200;
				eve->h= 1;
			}
			eve= eve->next;
		}
	}
	else {
		eve= em->verts.first;
		while(eve) {
			if(eve->f & 1) {
				eve->f-=1;
				eve->xs= 3200;
				eve->h= 1;
			}
			eve= eve->next;
		}
	}
	eed= em->edges.first;
	while(eed) {
		if(eed->v1->h || eed->v2->h) eed->h= 1;
		else eed->h= 0;
		eed= eed->next;
	}

	allqueue(REDRAWVIEW3D, 0);
	makeDispList(G.obedit);
}


void reveal_mesh(void)
{
	EditMesh *em = G.editMesh;
	EditVert *eve;
	EditEdge *eed;

	if(G.obedit==0) return;

	eve= em->verts.first;
	while(eve) {
		if(eve->h) {
			eve->h= 0;
			eve->f|=1;
		}
		eve= eve->next;
	}

	eed= em->edges.first;
	while(eed) {
		eed->h= 0;
		eed= eed->next;
	}

	allqueue(REDRAWVIEW3D, 0);
	makeDispList(G.obedit);
}


void vertices_to_sphere(void)
{
	EditMesh *em = G.editMesh;
	EditVert *eve;
	Object *ob= OBACT;
	float *curs, len, vec[3], cent[3], fac, facm, imat[3][3], bmat[3][3];
	int tot;
	short perc=100;
	
	if(ob==0) return;
	TEST_EDITMESH
	
	if(button(&perc, 1, 100, "Percentage:")==0) return;
	
	undo_push_mesh("To Sphere");
	
	fac= perc/100.0;
	facm= 1.0-fac;
	
	Mat3CpyMat4(bmat, ob->obmat);
	Mat3Inv(imat, bmat);

	/* centre */
	curs= give_cursor();
	cent[0]= curs[0]-ob->obmat[3][0];
	cent[1]= curs[1]-ob->obmat[3][1];
	cent[2]= curs[2]-ob->obmat[3][2];
	Mat3MulVecfl(imat, cent);

	len= 0.0;
	tot= 0;
	eve= em->verts.first;
	while(eve) {
		if(eve->f & 1) {
			tot++;
			len+= VecLenf(cent, eve->co);
		}
		eve= eve->next;
	}
	len/=tot;
	
	if(len==0.0) len= 10.0;
	
	eve= em->verts.first;
	while(eve) {
		if(eve->f & 1) {
			vec[0]= eve->co[0]-cent[0];
			vec[1]= eve->co[1]-cent[1];
			vec[2]= eve->co[2]-cent[2];
			
			Normalise(vec);
			
			eve->co[0]= fac*(cent[0]+vec[0]*len) + facm*eve->co[0];
			eve->co[1]= fac*(cent[1]+vec[1]*len) + facm*eve->co[1];
			eve->co[2]= fac*(cent[2]+vec[2]*len) + facm*eve->co[2];
			
		}
		eve= eve->next;
	}
	
	allqueue(REDRAWVIEW3D, 0);
	makeDispList(G.obedit);
}


/* ********** ALIGN WITH VIEW **************** */


static void editmesh_calc_selvert_center(float cent_r[3])
{
	EditMesh *em = G.editMesh;
	EditVert *eve;
	int nsel= 0;

	cent_r[0]= cent_r[1]= cent_r[0]= 0.0;

	for (eve= em->verts.first; eve; eve= eve->next) {
		if (eve->f & SELECT) {
			cent_r[0]+= eve->co[0];
			cent_r[1]+= eve->co[1];
			cent_r[2]+= eve->co[2];
			nsel++;
		}
	}

	if (nsel) {
		cent_r[0]/= nsel;
		cent_r[1]/= nsel;
		cent_r[2]/= nsel;
	}
}

static int tface_is_selected(TFace *tf)
{
	return (!(tf->flag & TF_HIDE) && (tf->flag & TF_SELECT));
}

static int faceselect_nfaces_selected(Mesh *me)
{
	int i, count= 0;

	for (i=0; i<me->totface; i++) {
		MFace *mf= ((MFace*) me->mface) + i;
		TFace *tf= ((TFace*) me->tface) + i;

		if (mf->v3 && tface_is_selected(tf))
			count++;
	}

	return count;
}

	/* XXX, code for both these functions should be abstract,
	 * then unified, then written for other things (like objects,
	 * which would use same as vertices method), then added
	 * to interface! Hoera! - zr
	 */
void faceselect_align_view_to_selected(View3D *v3d, Mesh *me, int axis)
{
	if (!faceselect_nfaces_selected(me)) {
		error("No faces selected.");
	} else {
		float norm[3];
		int i;

		norm[0]= norm[1]= norm[2]= 0.0;
		for (i=0; i<me->totface; i++) {
			MFace *mf= ((MFace*) me->mface) + i;
			TFace *tf= ((TFace*) me->tface) + i;
	
			if (mf->v3 && tface_is_selected(tf)) {
				float *v1, *v2, *v3, fno[3];

				v1= me->mvert[mf->v1].co;
				v2= me->mvert[mf->v2].co;
				v3= me->mvert[mf->v3].co;
				if (mf->v4) {
					float *v4= me->mvert[mf->v4].co;
					CalcNormFloat4(v1, v2, v3, v4, fno);
				} else {
					CalcNormFloat(v1, v2, v3, fno);
				}

				norm[0]+= fno[0];
				norm[1]+= fno[1];
				norm[2]+= fno[2];
			}
		}

		view3d_align_axis_to_vector(v3d, axis, norm);
	}
}

void editmesh_align_view_to_selected(View3D *v3d, int axis)
{
	EditMesh *em = G.editMesh;
	int nselverts= editmesh_nvertices_selected();

	if (nselverts<3) {
		if (nselverts==0) {
			error("No faces or vertices selected.");
		} else {
			error("At least one face or three vertices must be selected.");
		}
	} else if (editmesh_nfaces_selected()) {
		float norm[3];
		EditFace *efa;

		norm[0]= norm[1]= norm[2]= 0.0;
		for (efa= em->faces.first; efa; efa= efa->next) {
			if (faceselectedAND(efa, SELECT)) {
				float fno[3];
				if (efa->v4) CalcNormFloat4(efa->v1->co, efa->v2->co, efa->v3->co, efa->v4->co, fno);
				else CalcNormFloat(efa->v1->co, efa->v2->co, efa->v3->co, fno);
						/* XXX, fixme, should be flipped intp a 
						 * consistent direction. -zr
						 */
				norm[0]+= fno[0];
				norm[1]+= fno[1];
				norm[2]+= fno[2];
			}
		}

		Mat4Mul3Vecfl(G.obedit->obmat, norm);
		view3d_align_axis_to_vector(v3d, axis, norm);
	} else {
		float cent[3], norm[3];
		EditVert *eve, *leve= NULL;

		norm[0]= norm[1]= norm[2]= 0.0;
		editmesh_calc_selvert_center(cent);
		for (eve= em->verts.first; eve; eve= eve->next) {
			if (eve->f & SELECT) {
				if (leve) {
					float tno[3];
					CalcNormFloat(cent, leve->co, eve->co, tno);
					
						/* XXX, fixme, should be flipped intp a 
						 * consistent direction. -zr
						 */
					norm[0]+= tno[0];
					norm[1]+= tno[1];
					norm[2]+= tno[2];
				}
				leve= eve;
			}
		}

		Mat4Mul3Vecfl(G.obedit->obmat, norm);
		view3d_align_axis_to_vector(v3d, axis, norm);
	}
}


void select_non_manifold(void)
{
	EditMesh *em = G.editMesh;
	EditVert *eve;
	EditEdge *eed;
	EditFace *efa;

	/* Selects isolated verts, and edges that do not have 2 neighboring
	 * faces
	 */


	eve= em->verts.first;
	while(eve) {
		/* this will count how many edges are connected
		 * to this vert */
		eve->f1= 0;
		eve= eve->next;
	}

	eed= em->edges.first;
	while(eed) {
		/* this will count how many faces are connected to
		 * this edge */
		eed->f1= 0;
		/* increase edge count for verts */
		++eed->v1->f1;
		++eed->v2->f1;
		eed= eed->next;
	}

	efa= em->faces.first;
	while(efa) {
		/* increase face count for edges */
		++efa->e1->f1;
		++efa->e2->f1;
		++efa->e3->f1;
		if (efa->e4)
			++efa->e4->f1;			
		efa= efa->next;
	}

	/* select verts that are attached to an edge that does not
	 * have 2 neighboring faces */
	eed= em->edges.first;
	while(eed) {
		if (eed->f1 != 2) {
			if (!eed->v1->h) eed->v1->f |= 1;
			if (!eed->v2->h) eed->v2->f |= 1;
		}
		eed= eed->next;
	}

	/* select isolated verts */
	eve= em->verts.first;
	while(eve) {
		if (eve->f1 == 0) {
			if (!eve->h) eve->f |= 1;
		}
		eve= eve->next;
	}

	countall();
	addqueue(curarea->win,  REDRAW, 0);

}

void select_more(void)
{
	EditMesh *em = G.editMesh;
	EditVert *eve;
	EditEdge *eed;

	eve= em->verts.first;
	while(eve) {
		eve->f1 = 0;
		eve= eve->next;
	}

	eed= em->edges.first;
	while(eed) {
		if (eed->v1->f & 1)
			eed->v2->f1 = 1;
		if (eed->v2->f & 1)
			eed->v1->f1 = 1;

		eed= eed->next;
	}

	eve= em->verts.first;
	while(eve) {
		if (eve->f1 == 1)
			if (!eve->h) eve->f |= 1;

		eve= eve->next;
	}

	countall();
	addqueue(curarea->win,  REDRAW, 0);
}

void select_less(void)
{
	EditMesh *em = G.editMesh;
	EditVert *eve;
	EditEdge *eed;
	EditFace *efa;

	/* eve->f1 & 1 => isolated   */
	/* eve->f1 & 2 => on an edge */
	/* eve->f1 & 4 => shares edge with a deselected vert */ 
	/* eve->f1 & 8 => at most one neighbor */ 

	eve= em->verts.first;
	while(eve) {
		/* assume vert is isolated unless proven otherwise, */
		/* assume at most one neighbor too */
		eve->f1 = 1 | 8;

		eve= eve->next;
	}

	eed= em->edges.first;
	while(eed) {
		/* this will count how many faces are connected to
		 * this edge */
		eed->f1= 0;

		/* if vert wasn't isolated, it now has more than one neighbor */
		if (~eed->v1->f1 & 1) eed->v1->f1 &= ~8;
		if (~eed->v2->f1 & 1) eed->v2->f1 &= ~8;

		/* verts on edge are clearly not isolated */
		eed->v1->f1 &= ~1;
		eed->v2->f1 &= ~1;

		/* if one of the verts on the edge is deselected, 
		 * deselect the other */
		if ( !(eed->v1->h) && (~eed->v1->f & 1) )
			eed->v2->f1 |= 4;
		if ( !(eed->v2->h) && (~eed->v2->f & 1) )
			eed->v1->f1 |= 4;

		eed= eed->next;
	}

	efa= em->faces.first;
	while(efa) {
		/* increase face count for edges */
		++efa->e1->f1;
		++efa->e2->f1;
		++efa->e3->f1;
		if (efa->e4)
			++efa->e4->f1;			

		efa= efa->next;
	}

	eed= em->edges.first;
	while(eed) {
		/* if the edge has only one neighboring face, then
		 * deselect attached verts */
		if (eed->f1 == 1) {
			eed->v1->f1 |= 2;
			eed->v2->f1 |= 2;
		}

		eed= eed->next;
	}

	/* deselect verts */
	eve= em->verts.first;
	while(eve) {
		if (eve->f1) {
			eve->f &= ~1;
		}

		eve= eve->next;
	}

	countall();
	allqueue(REDRAWVIEW3D, 0);
}


void selectrandom_mesh(void) /* randomly selects a user-set % of vertices */
{
	EditMesh *em = G.editMesh;
	EditVert *eve;
	int newsel = 0; /* to decide whether to redraw or not */
	short randfac = 50;

	if(G.obedit==0) return;

	/* Get the percentage of vertices to randomly select as 'randfac' */
	if(button(&randfac,0, 100,"Percentage:")==0) return;
		
	if(G.obedit->lay & G.vd->lay) {
		eve= em->verts.first;
		while(eve) {
			BLI_srand( BLI_rand() ); /* random seed */
			if ( (BLI_frand() * 100) < randfac) {
					eve->f |= SELECT;
					newsel = 1;
			} else {
				/* Deselect other vertices
				 *
				 * - Commenting this out makes it add to the selection, 
				 * rather than replace it.
				 * eve->f &= ~SELECT;
				 */
			}
			eve= eve->next;
		}
		countall();
		allqueue(REDRAWVIEW3D, 0);
	}
}



void editmesh_select_by_material(int index) 
{
	EditMesh *em = G.editMesh;
	EditFace *efa;
	
	for (efa=em->faces.first; efa; efa= efa->next) {
		if (efa->mat_nr==index) {
			if(efa->v1->h==0) efa->v1->f |= 1;
			if(efa->v2->h==0) efa->v2->f |= 1;
			if(efa->v3->h==0) efa->v3->f |= 1;
			if(efa->v4 && efa->v4->h==0) efa->v4->f |= 1;
		}
	}
}

void editmesh_deselect_by_material(int index) 
{
	EditMesh *em = G.editMesh;
	EditFace *efa;
	
	for (efa=em->faces.first; efa; efa= efa->next) {
		if (efa->mat_nr==index) {
			if(efa->v1->h==0) efa->v1->f &= ~1;
			if(efa->v2->h==0) efa->v2->f &= ~1;
			if(efa->v3->h==0) efa->v3->f &= ~1;
			if(efa->v4 && efa->v4->h==0) efa->v4->f &= ~1;
		}
	}
}

void editmesh_mark_seam(int clear)
{
	EditMesh *em= G.editMesh;
	EditEdge *eed;
	Mesh *me= G.obedit->data;

	/* auto-enable seams drawing */
	if(clear==0) {
		if(!(G.f & G_DRAWSEAMS)) {
			G.f |= G_DRAWSEAMS;
			allqueue(REDRAWBUTSEDIT, 0);
		}
		if(!me->medge)
			me->medge= MEM_callocN(sizeof(MEdge), "fake mesh edge");
	}

	if(clear) {
		eed= em->edges.first;
		while(eed) {
			if((eed->h==0) && (eed->v1->f & 1) && (eed->v2->f & 1)) {
				eed->seam = 0;
			}
			eed= eed->next;
		}
	}
	else {
		eed= em->edges.first;
		while(eed) {
			if((eed->h==0) && (eed->v1->f & 1) && (eed->v2->f & 1)) {
				eed->seam = 1;
			}
			eed= eed->next;
		}
	}

	allqueue(REDRAWVIEW3D, 0);
}

void Edge_Menu() {
	short ret;

	ret= pupmenu("Edge Specials%t|Mark Seam %x1|Clear Seam %x2|Rotate Edges %x3");

	switch(ret)
	{
	case 1:
		editmesh_mark_seam(0);
		break;
	case 2:
		editmesh_mark_seam(1);
		break;
	case 3:
		edge_rotate_selected();
		break;
	}
}


