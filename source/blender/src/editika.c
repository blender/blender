/**
 * ------------------------------------------------------------
 * This is the old 'Ika' system of Blender (until 2.14)
 * it should be removed entirely from the tree, so I'll also leave comments
 * not translated... (ton)
 * ------------------------------------------------------------
 *
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

#ifdef WIN32
#include "BLI_winstuff.h"
#endif
#include "MEM_guardedalloc.h"

#include "BMF_Api.h"

#include "BLI_blenlib.h"
#include "BLI_arithb.h"
#include "BLI_editVert.h"

#include "DNA_object_types.h"
#include "DNA_screen_types.h"
#include "DNA_scene_types.h"
#include "DNA_ika_types.h"
#include "DNA_view3d_types.h"

#include "BKE_utildefines.h"
#include "BKE_object.h"
#include "BKE_ika.h"
#include "BKE_global.h"
#include "BKE_displist.h"

#include "BIF_gl.h"
#include "BIF_toolbox.h"
#include "BIF_mywindow.h"
#include "BIF_screen.h"
#include "BIF_space.h"
#include "BIF_editika.h"

#include "BSE_view.h"
#include "BSE_drawview.h"

#include "mydevice.h"
#include "blendef.h"

static void draw_limb(Limb *li, float small)
{
	float vec[2];
	
	glRotatef(( li->alpha*180.0/M_PI), 0.0, 0.0, 1.0);
	
	{ GLUquadricObj *qobj = gluNewQuadric(); 
		gluQuadricDrawStyle(qobj, GLU_SILHOUETTE); 
		gluPartialDisk( qobj, small,  small, 32, 1, 180.0, 180.0); 
		gluDeleteQuadric(qobj); 
	};
	
	vec[0]= 0.0; vec[1]= small;
	glBegin(GL_LINE_STRIP);
	glVertex2fv(vec);
	vec[0]= li->len; vec[1]= 0.0;
	glVertex2fv(vec);
	vec[0]= 0.0; vec[1]= -small;
	glVertex2fv(vec);
	glEnd();
	
	small*= 0.25;
	
	if(li->next) circf(li->len, 0.0, small);
	else circf(li->len, 0.0, small);
	
	glTranslatef(li->len,  0.0,  0.0);
}

void draw_ika(Object *ob, int sel)
{
	Ika *ika;
	Limb *li;
	float col[4];
	float small= 0.15;
	
	ika= ob->data;
	li= ika->limbbase.first;
	if(li==0) return;
	
	/* we zijn al in objectspace */
	glPushMatrix();
	
	glGetFloatv(GL_CURRENT_COLOR, col);

	if((ika->flag & IK_GRABEFF)==0) {
		if(sel) cpack(0xFFFF);
		circf(0.0, 0.0, 0.05*li->len);
		
		glColor3f(col[0],  col[1],  col[2]);
	}

	while(li) {
		small= 0.10*li->len;
		draw_limb(li, small);
		li= li->next;
	}	
	
	if(ika->flag & IK_GRABEFF) {
		if(sel) {
			if(ika->def) cpack(0xFFFF00);
			else cpack(0xFFFF);
		}
		circf(0.0, 0.0, 0.25*small);
		glColor3f(col[0],  col[1],  col[2]);
	}
	
	glPopMatrix();
}

/* type 0: verts, type 1: limbs */
void draw_ika_nrs(Object *ob, int type)
{
	Ika *ika;
	Limb *li;
	int nr=0;
	char str[12];
	
	if(curarea->spacetype!=SPACE_VIEW3D) return;
	mywinset(curarea->win);
	
	glDrawBuffer(GL_FRONT);
	myloadmatrix(G.vd->viewmat);
	mymultmatrix(ob->obmat);
	
	ika= ob->data;
	li= ika->limbbase.first;
	
	/* we zijn al in objectspace */
	glPushMatrix();
	cpack(0xFFFFFF);
	
	if(type==0) {
		sprintf(str, " %d", nr++);
		glRasterPos3f(0.0,  0.0,  0.0);
		BMF_DrawString(G.font, str);

		while(li) {

			glRotatef(( li->alpha*180.0/M_PI), 0.0, 0.0, 1.0);
			glTranslatef(li->len,  0.0,  0.0);
			sprintf(str, " %d", nr++);
			glRasterPos3f(0.0,  0.0,  0.0);
			BMF_DrawString(G.font, str);
			
			li= li->next;
		}	
	}
	else {
		while(li) {

			glRotatef(( li->alpha*180.0/M_PI), 0.0, 0.0, 1.0);
			glTranslatef( 0.7*li->len,  0.0,  0.0);
			sprintf(str, " %d", nr++);
			glRasterPos3f(0.0,  0.0,  0.0);
			BMF_DrawString(G.font, str);
			glTranslatef( 0.3*li->len,  0.0,  0.0);
			
			li= li->next;
		}	
		
	}
	
	glDrawBuffer(GL_BACK);
	glPopMatrix();
}



int extrude_ika(Object *ob, int add)
{
	Ika *ika;
	Limb *li;
	float dvec[3], dvecp[3], oldeul[3], mat[3][3], imat[3][3];
	int firsttime= 1;
	unsigned short event = 0;
	short val, afbreek=0, mval[2], xo, yo;
	
	/* init */
	VECCOPY(oldeul, ob->rot);
	initgrabz(ob->obmat[3][0], ob->obmat[3][1], ob->obmat[3][2]);

	Mat3CpyMat4(mat, ob->obmat);
	Mat3Inv(imat, mat);
	
	getmouseco_areawin(mval);
	xo= mval[0];
	yo= mval[1];

	/* het laatste punt van de ika */
	ika= ob->data;
	
	if(add) {
		/* een erbij: */
		li= MEM_callocN(sizeof(Limb), "limb");
		BLI_addtail(&ika->limbbase, li);
		if(li->prev) {
			li->eff[0]= li->prev->eff[0];
			li->eff[1]= li->prev->eff[1];
		}
		li->eff[0]+= 0.5;
	}
	li= ika->limbbase.last;
	if(li==0) return 0;
	
	while(TRUE) {

		getmouseco_areawin(mval);
		if(mval[0]!=xo || mval[1]!=yo || firsttime) {
			firsttime= 0;

			window_to_3d(dvec, mval[0]-xo, mval[1]-yo);
			VECCOPY(dvecp, dvec);
			
			/* apply */
			Mat3MulVecfl(imat, dvecp);
			li->eff[0]+= dvecp[0];
			li->eff[1]+= dvecp[1];

			calc_limb(li);
			
			if(li->prev==0) {
				VECCOPY(ob->rot, oldeul);
				euler_rot(ob->rot, li->alpha, 'z');
				li->alpha= li->alphao= 0.0;
			}
			
			xo= mval[0];
			yo= mval[1];
			
			force_draw();
		}
		
		while(qtest()) {
			event= extern_qread(&val);
			if(val) {
				switch(event) {
				case ESCKEY:
				case LEFTMOUSE:
				case MIDDLEMOUSE:
				case SPACEKEY:
				case RETKEY:
					afbreek= 1;
					break;
				}
			}
			if(afbreek) break;
		}
		
		if(afbreek) break;
	}
	
	if(event==ESCKEY) {
		if(ika->limbbase.first!=ika->limbbase.last) {
			li= ika->limbbase.last;
			BLI_remlink(&ika->limbbase, li);
			MEM_freeN(li);
		}
	}
	else if(add) init_defstate_ika(ob);
	
	allqueue(REDRAWVIEW3D, 0);
	
	if(event==LEFTMOUSE) return 0;
	return 1; 
}

void delete_skeleton(void)
{
	Object *ob;
	Ika *ika;

	ob= OBACT;
	if(ob==0 || ob->type!=OB_IKA || (ob->flag & SELECT)==0) return;
	
	ika= ob->data;

	if(!ika->def) return;
	if(!okee("Delete Skeleton")) return;

	if(ika->def) MEM_freeN(ika->def);
	ika->def= 0;
	ika->totdef= 0;

	allqueue(REDRAWVIEW3D, 0);
}

static void copy_deform(int tot, Deform *defbase, Deform *def)
{
	/* defbase is the old one, *def is new. When they match,
           the deform data is copied */

	while(tot--) {
		if(defbase->ob==def->ob && defbase->par1==def->par1) {
			def->fac= defbase->fac;
			def->dist= defbase->dist;
			return;
		}
		defbase++;
	}
}

void make_skeleton(void)
{
	Object *ob;
	Base *base;
	Ika *ika;
	Deform *def, *defbase;
	Limb *li;
	int a, totdef=0;
	
	ob= OBACT;
	if(ob==0 || ob->type!=OB_IKA || (ob->flag & SELECT)==0) return;
	
	if(!okee("Make Skeleton")) return;
	
	ika= ob->data;
	
	/* per selected ob, per limb, de obmat en imat berekenen */
	
	base= FIRSTBASE;
	while(base) {
		if TESTBASE(base) {
			if(base->object->type==OB_IKA) totdef+= count_limbs(base->object);
			else totdef++;
		}
		base= base->next;
	}
	
	if(totdef==0) {
		error("Nothing selected");
		return;
	}
	
	def=defbase= MEM_callocN(totdef*sizeof(Deform), "deform");
	
	base= FIRSTBASE;
	while(base) {
		if TESTBASE(base) {
			
			if(base->object->type==OB_IKA) {
				
				li= ( (Ika *)(base->object->data) )->limbbase.first;
				a= 0;
				while(li) {
					what_does_parent1(base->object, PARLIMB, a, 0, 0);
					def->ob= base->object;
					def->partype= PARLIMB;
					def->par1= a;
					
					Mat4Invert(def->imat, workob.obmat);
					def->vec[0]= li->len;
					def->fac= 1.0;

					copy_deform(ika->totdef, ika->def, def);

					def++;
					a++;
					li= li->next;
				}
			}
			else {
				what_does_parent1(base->object, PAROBJECT, 0, 0, 0);
				def->ob= base->object;
				def->partype= PAROBJECT;
				
				def->vec[0]= 0.0;
				def->fac= 1.0;
				def->dist= 0.0;
				
				copy_deform(ika->totdef, ika->def, def);

				Mat4Invert(def->imat, workob.obmat);
				def++;
			}
		}
		base= base->next;
	}

	if(ika->def) MEM_freeN(ika->def);
	ika->def= defbase;
	ika->totdef= totdef;
	
		/* Recalculate the deformation on any object
		 * that was parented to the old skeleton.
		 */
	for (base= FIRSTBASE; base; base= base->next)
		if (base->object->parent==ob)
			makeDispList(base->object);
	
	allqueue(REDRAWVIEW3D, 0);
	allqueue(REDRAWBUTSEDIT, 0);
}
