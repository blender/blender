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

#include "BLI_blenlib.h"
#include "BLI_arithb.h"

#include "IMB_imbuf_types.h"

#include "DNA_mesh_types.h"
#include "DNA_scene_types.h"
#include "DNA_screen_types.h"
#include "DNA_userdef_types.h"
#include "DNA_space_types.h"
#include "DNA_image_types.h"
#include "DNA_object_types.h" // only for uvedit_selectionCB() (struct Object)

#include "BKE_global.h"
#include "BKE_mesh.h"

#include "BIF_gl.h"
#include "BIF_interface.h"
#include "BIF_screen.h"
#include "BIF_drawimage.h"
#include "BIF_editview.h"
#include "BIF_space.h"
#include "BIF_editsima.h"
#include "BIF_toolbox.h"
#include "BIF_mywindow.h"

#include "BSE_drawipo.h"
#include "BSE_edit.h"
#include "BSE_trans_types.h"

#include "BDR_editobject.h"

#include "blendef.h"
#include "mydevice.h"

static int is_uv_tface_editing_allowed(void)
{
	Mesh *me;

	if(G.obedit) {error("Unable to perform function in EditMode"); return 0;}
	if(G.sima->mode!=SI_TEXTURE) return 0;
	if(!(G.f & G_FACESELECT)) return 0;  
	me= get_mesh(OBACT);
	if(me==0 || me->tface==0) return 0;
	
	return 1;
}

void clever_numbuts_sima(void)
{
	float ocent[2], cent[2]= {0.0, 0.0};
	int imx, imy;
	int i, nactive= 0;
	Mesh *me;
	
	if( is_uv_tface_editing_allowed()==0 ) return;
	me= get_mesh(OBACT);
	
	if (G.sima->image && G.sima->image->ibuf) {
		imx= G.sima->image->ibuf->x;
		imy= G.sima->image->ibuf->y;
	} else
		imx= imy= 256;
	
	for (i=0; i<me->totface; i++) {
		MFace *mf= &((MFace*) me->mface)[i];
		TFace *tf= &((TFace*) me->tface)[i];
		
		if (!mf->v3 || !(tf->flag & TF_SELECT))
			continue;
		
		if (tf->flag & TF_SEL1) {
			cent[0]+= tf->uv[0][0];
			cent[1]+= tf->uv[0][1];
			nactive++;
		}
		if (tf->flag & TF_SEL2) {
			cent[0]+= tf->uv[1][0];
			cent[1]+= tf->uv[1][1];
			nactive++;
		}
		if (tf->flag & TF_SEL3) {
			cent[0]+= tf->uv[2][0];
			cent[1]+= tf->uv[2][1];
			nactive++;
		}
		if (mf->v4 && (tf->flag & TF_SEL4)) {
			cent[0]+= tf->uv[3][0];
			cent[1]+= tf->uv[3][1];
			nactive++;
		}
	}
	
	if (nactive) {
		cent[0]= (cent[0]*imx)/nactive;
		cent[1]= (cent[1]*imy)/nactive;

		add_numbut(0, NUM|FLO, "LocX:", -imx*20, imx*20, &cent[0], NULL);
		add_numbut(1, NUM|FLO, "LocY:", -imy*20, imy*20, &cent[1], NULL);
		
		ocent[0]= cent[0];
		ocent[1]= cent[1];
		if (do_clever_numbuts((nactive==1)?"Active Vertex":"Selected Center", 2, REDRAW)) {
			float delta[2];
			
			delta[0]= (cent[0]-ocent[0])/imx;
			delta[1]= (cent[1]-ocent[1])/imy;

			for (i=0; i<me->totface; i++) {
				MFace *mf= &((MFace*) me->mface)[i];
				TFace *tf= &((TFace*) me->tface)[i];
			
				if (!mf->v3 || !(tf->flag & TF_SELECT))
					continue;
			
				if (tf->flag & TF_SEL1) {
					tf->uv[0][0]+= delta[0];
					tf->uv[0][1]+= delta[1];
				}
				if (tf->flag & TF_SEL2) {
					tf->uv[1][0]+= delta[0];
					tf->uv[1][1]+= delta[1];
				}
				if (tf->flag & TF_SEL3) {
					tf->uv[2][0]+= delta[0];
					tf->uv[2][1]+= delta[1];
				}
				if (mf->v4 && (tf->flag & TF_SEL4)) {
					tf->uv[3][0]+= delta[0];
					tf->uv[3][1]+= delta[1];
				}
			}
			
			allqueue(REDRAWVIEW3D, 0);
		}
	}
}

static void sima_pixelgrid(float *loc, float sx, float sy)
{
	float y;
	float x;
	
	if(G.sima->image && G.sima->image->ibuf) {
		x= G.sima->image->ibuf->x;
		y= G.sima->image->ibuf->y;
	
		sx= floor(x*sx)/x;
		if(G.sima->flag & SI_CLIP_UV) {
			CLAMP(sx, 0, 1.0);
		}
		loc[0]= sx;
		
		sy= floor(y*sy)/y;
		if(G.sima->flag & SI_CLIP_UV) {
			CLAMP(sy, 0, 1.0);
		}
		loc[1]= sy;
	}
	else {
		loc[0]= sx;
		loc[1]= sy;
	}
}


static void be_square_tface_uv(Mesh *me)
{
	TFace *tface;
	MFace *mface;
	int a;
	
	/* if 1 vertex selected: doit (with the selected vertex) */
	for(a=me->totface, mface= me->mface, tface= me->tface; a>0; a--, tface++, mface++) {
		if(mface->v4) {
			if(tface->flag & TF_SELECT) {
				if(tface->flag & TF_SEL1) {
					if( tface->uv[1][0] == tface->uv[2][0] ) {
						tface->uv[1][1]= tface->uv[0][1];
						tface->uv[3][0]= tface->uv[0][0];
					}
					else {	
						tface->uv[1][0]= tface->uv[0][0];
						tface->uv[3][1]= tface->uv[0][1];
					}
					
				}
				if(tface->flag & TF_SEL2) {
					if( tface->uv[2][1] == tface->uv[3][1] ) {
						tface->uv[2][0]= tface->uv[1][0];
						tface->uv[0][1]= tface->uv[1][1];
					}
					else {
						tface->uv[2][1]= tface->uv[1][1];
						tface->uv[0][0]= tface->uv[1][0];
					}

				}
				if(tface->flag & TF_SEL3) {
					if( tface->uv[3][0] == tface->uv[0][0] ) {
						tface->uv[3][1]= tface->uv[2][1];
						tface->uv[1][0]= tface->uv[2][0];
					}
					else {
						tface->uv[3][0]= tface->uv[2][0];
						tface->uv[1][1]= tface->uv[2][1];
					}
				}
				if(tface->flag & TF_SEL4) {
					if( tface->uv[0][1] == tface->uv[1][1] ) {
						tface->uv[0][0]= tface->uv[3][0];
						tface->uv[2][1]= tface->uv[3][1];
					}
					else  {
						tface->uv[0][1]= tface->uv[3][1];
						tface->uv[2][0]= tface->uv[3][0];
					}

				}
			}
		}
	}

}

void tface_do_clip(void)
{
	Mesh *me;
	TFace *tface;
	int a, b;
	
	if( is_uv_tface_editing_allowed()==0 ) return;
	me= get_mesh(OBACT);
	tface= me->tface;
	
	for(a=0; a<me->totface; a++, tface++) {
		if(tface->flag & TF_SELECT) {
			for(b=0; b<4; b++) {
				CLAMP(tface->uv[b][0], 0.0, 1.0);
				CLAMP(tface->uv[b][1], 0.0, 1.0);
			}
		}
	}
	
}

void transform_tface_uv(int mode)
{
	MFace *mface;
	TFace *tface;
	Mesh *me;
	TransVert *transmain, *tv;
	float asp, dx1, dx2, dy1, dy2, phi, dphi, co, si;
	float xref=1.0, yref=1.0, size[2], sizefac;
	float dx, dy, dvec2[2], dvec[2], div, cent[2];
	float x, y, min[2], max[2], vec[2], xtra[2], ivec[2];
	int xim, yim, tot=0, a, b, firsttime=1, afbreek=0, midtog= 0, proj = 0;
	unsigned short event = 0;
	short mval[2], val, xo, yo, xn, yn, xc, yc;
	char str[32];
	
	if( is_uv_tface_editing_allowed()==0 ) return;
	me= get_mesh(OBACT);
	
	min[0]= min[1]= 10000.0;
	max[0]= max[1]= -10000.0;
	
	calc_image_view(G.sima, 'f');
	
	if(G.sima->image && G.sima->image->ibuf) {
		xim= G.sima->image->ibuf->x;
		yim= G.sima->image->ibuf->y;
	}
	else {
		xim= yim= 256;
	}
	/* which vertices are involved */
	
	for(a=me->totface, tface= me->tface, mface= me->mface; a>0; a--, tface++, mface++) {
		if((tface->flag & TF_SELECT) && mface->v3) {
			if(tface->flag & TF_SEL1) tot++;
			if(tface->flag & TF_SEL2) tot++;
			if(tface->flag & TF_SEL3) tot++;
			if(tface->flag & TF_SEL4) tot++; 
		}
	}
	if(tot==0) return;
	
	tv=transmain= MEM_callocN(tot*sizeof(TransVert), "transmain");

	for(a=me->totface, tface= me->tface, mface= me->mface; a>0; a--, tface++, mface++) {
		if((tface->flag & TF_SELECT) && mface->v3) {
			if(tface->flag & TF_SEL1) {
				tv->loc= tface->uv[0];
				tv++;
			}
			if(tface->flag & TF_SEL2) {
				tv->loc= tface->uv[1];
				tv++;
			}
			if(tface->flag & TF_SEL3) {
				tv->loc= tface->uv[2];
				tv++;
			}
			if(tface->flag & TF_SEL4) {
				tv->loc= tface->uv[3];
				tv++;
			}
		}
	}
	
	a= tot;
	tv= transmain;
	while(a--) {
		tv->oldloc[0]= tv->loc[0];
		tv->oldloc[1]= tv->loc[1];
		DO_MINMAX2(tv->loc, min, max);
		tv++;
	}
	
	cent[0]= (min[0]+max[0])/2.0;
	cent[1]= (min[1]+max[1])/2.0;

	ipoco_to_areaco_noclip(G.v2d, cent, mval);
	xc= mval[0];
	yc= mval[1];
	
	getmouseco_areawin(mval);
	xo= xn= mval[0];
	yo= yn= mval[1];
	dvec[0]= dvec[1]= 0.0;
	dx1= xc-xn; 
	dy1= yc-yn;
	phi= 0.0;
	
	
	sizefac= sqrt( (float)((yc-yn)*(yc-yn)+(xn-xc)*(xn-xc)) );
	if(sizefac<2.0) sizefac= 2.0;

	while(afbreek==0) {
		getmouseco_areawin(mval);
		if(mval[0]!=xo || mval[1]!=yo || firsttime) {
			
			if(mode=='g') {
			
				dx= mval[0]- xo;
				dy= mval[1]- yo;
	
				div= G.v2d->mask.xmax-G.v2d->mask.xmin;
				dvec[0]+= (G.v2d->cur.xmax-G.v2d->cur.xmin)*(dx)/div;
	
				div= G.v2d->mask.ymax-G.v2d->mask.ymin;
				dvec[1]+= (G.v2d->cur.ymax-G.v2d->cur.ymin)*(dy)/div;
				
				if(midtog) dvec[proj]= 0.0;
				
				dvec2[0]= dvec[0];
				dvec2[1]= dvec[1];
				apply_keyb_grid(dvec2, 0.0, 1.0/8.0, 1.0/16.0, U.flag & USER_AUTOGRABGRID);
				apply_keyb_grid(dvec2+1, 0.0, 1.0/8.0, 1.0/16.0, U.flag & USER_AUTOGRABGRID);

				vec[0]= dvec2[0];
				vec[1]= dvec2[1];
				
				if(G.sima->flag & SI_CLIP_UV) {
					if(vec[0]< -min[0]) vec[0]= -min[0];
					if(vec[1]< -min[1]) vec[1]= -min[1];
					if(vec[0]> 1.0-max[0]) vec[0]= 1.0-max[0];
					if(vec[1]> 1.0-max[1]) vec[1]= 1.0-max[1];
				}
				tv= transmain;
				for(a=0; a<tot; a++, tv++) {
					
					x= tv->oldloc[0]+vec[0];
					y= tv->oldloc[1]+vec[1];
						
					sima_pixelgrid(tv->loc, x, y);
				}
				ivec[0]= (vec[0]*xim);
				ivec[1]= (vec[1]*yim);

				if(G.sima->flag & SI_BE_SQUARE) be_square_tface_uv(me);
			
				sprintf(str, "X: %.4f   Y: %.4f  ", ivec[0], ivec[1]);
				headerprint(str);
			}
			else if(mode=='r') {

				dx2= xc-mval[0];
				dy2= yc-mval[1];
				
				div= sqrt( (dx1*dx1+dy1*dy1)*(dx2*dx2+dy2*dy2));
				if(div>1.0) {
				
					dphi= (dx1*dx2+dy1*dy2)/div;
					dphi= saacos(dphi);
					if( (dx1*dy2-dx2*dy1)<0.0 ) dphi= -dphi;
					
					if(G.qual & LR_SHIFTKEY) phi+= dphi/30.0;
					else phi+= dphi;
					
					apply_keyb_grid(&phi, 0.0, (5.0/180)*M_PI, (1.0/180)*M_PI, U.flag & USER_AUTOROTGRID);
					
					dx1= dx2; 
					dy1= dy2;
					
					co= cos(phi);
					si= sin(phi);
					asp= (float)yim/(float)xim;

					
					tv= transmain;
					for(a=0; a<tot; a++, tv++) {
						
						x= ( co*( tv->oldloc[0]-cent[0]) - si*asp*(tv->oldloc[1]-cent[1]) ) +cent[0];
						y= ( si*( tv->oldloc[0]-cent[0])/asp + co*(tv->oldloc[1]-cent[1]) ) +cent[1];
						sima_pixelgrid(tv->loc, x, y);
						
						if(G.sima->flag & SI_CLIP_UV) {
							if(tv->loc[0]<0.0) tv->loc[0]= 0.0;
							else if(tv->loc[0]>1.0) tv->loc[0]= 1.0;
							if(tv->loc[1]<0.0) tv->loc[1]= 0.0;
							else if(tv->loc[1]>1.0) tv->loc[1]= 1.0;
						}
					}					
					
					
					sprintf(str, "Rot: %.3f  ", phi*180.0/M_PI);
					headerprint(str);
				}
			}
			else if(mode=='s') {
				
				size[0]=size[1]= (sqrt( (float)((yc-mval[1])*(yc-mval[1])+(mval[0]-xc)*(mval[0]-xc)) ))/sizefac;
				
				if(midtog) size[proj]= 1.0;
				
				apply_keyb_grid(size, 0.0, 0.1, 0.01, U.flag & USER_AUTOSIZEGRID);
				apply_keyb_grid(size+1, 0.0, 0.1, 0.01, U.flag & USER_AUTOSIZEGRID);

				size[0]*= xref;
				size[1]*= yref;
				
				xtra[0]= xtra[1]= 0;

				if(G.sima->flag & SI_CLIP_UV) {
					/* boundbox limit: four step plan: XTRA X */
	
					a=b= 0;
					if(size[0]*(min[0]-cent[0]) + cent[0] + xtra[0] < 0) 
						a= -size[0]*(min[0]-cent[0]) - cent[0];
					if(size[0]*(max[0]-cent[0]) + cent[0] + xtra[0] > 1.0) 
						b= 1.0 - size[0]*(max[0]-cent[0]) - cent[0];
					xtra[0]= (a+b)/2;
					
					/* SIZE X */
					if(size[0]*(min[0]-cent[0]) + cent[0] + xtra[0] < 0) 
						size[0]= (-cent[0]-xtra[0])/(min[0]-cent[0]);
					if(size[0]*(max[0]-cent[0]) + cent[0] +xtra[0] > 1.0) 
						size[0]= (1.0-cent[0]-xtra[0])/(max[0]-cent[0]);
						
					/* XTRA Y */
					a=b= 0;
					if(size[1]*(min[1]-cent[1]) + cent[1] + xtra[1] < 0) 
						a= -size[1]*(min[1]-cent[1]) - cent[1];
					if(size[1]*(max[1]-cent[1]) + cent[1] + xtra[1] > 1.0) 
						b= 1.0 - size[1]*(max[1]-cent[1]) - cent[1];
					xtra[1]= (a+b)/2;
					
					/* SIZE Y */
					if(size[1]*(min[1]-cent[1]) + cent[1] +xtra[1] < 0) 
						size[1]= (-cent[1]-xtra[1])/(min[1]-cent[1]);
					if(size[1]*(max[1]-cent[1]) + cent[1] + xtra[1]> 1.0) 
						size[1]= (1.0-cent[1]-xtra[1])/(max[1]-cent[1]);
				}

				/* if(midtog==0) { */
				/* 	if(size[1]>size[0]) size[1]= size[0]; */
				/* 	else if(size[0]>size[1]) size[0]= size[1]; */
				/* } */

				tv= transmain;
				for(a=0; a<tot; a++, tv++) {
					
					x= size[0]*(tv->oldloc[0]-cent[0])+ cent[0] + xtra[0];
					y= size[1]*(tv->oldloc[1]-cent[1])+ cent[1] + xtra[1];
					sima_pixelgrid(tv->loc, x, y);
				}
				
				sprintf(str, "sizeX: %.3f   sizeY: %.3f  ", size[0], size[1]);
				headerprint(str);
				
			}
			
			xo= mval[0];
			yo= mval[1];
			
			if(G.sima->lock) force_draw_plus(SPACE_VIEW3D);
			else force_draw();
			
			firsttime= 0;
			
		}
		else BIF_wait_for_statechange();
		
		while(qtest()) {
			event= extern_qread(&val);
			if(val) {
				switch(event) {
				case ESCKEY:
				case RIGHTMOUSE:
				case LEFTMOUSE:
				case SPACEKEY:
				case RETKEY:
					afbreek= 1;
					break;
				case MIDDLEMOUSE:
					
					midtog= ~midtog;
					if(midtog) {
						if( abs(mval[0]-xn) > abs(mval[1]-yn)) proj= 1;
						else proj= 0;
						firsttime= 1;
					}
				
					break;
				case XKEY:
				case YKEY:
					if(event==XKEY) xref= -xref;
					else yref= -yref;
					
					firsttime= 1;
					break;
				default:
					arrows_move_cursor(event);
				}
			}
			if(afbreek) break;
		}
	}
	
	if(event==ESCKEY || event == RIGHTMOUSE) {
		tv= transmain;
		for(a=0; a<tot; a++, tv++) {
			tv->loc[0]= tv->oldloc[0];
			tv->loc[1]= tv->oldloc[1];
		}
	}
	MEM_freeN(transmain);
	
	if(mode=='g') if(G.sima->flag & SI_BE_SQUARE) be_square_tface_uv(me);
			
	makeDispList(OBACT);
	allqueue(REDRAWVIEW3D, 0);
	scrarea_queue_headredraw(curarea);
	scrarea_queue_winredraw(curarea);
}

void select_swap_tface_uv(void)
{
	Mesh *me;
	TFace *tface;
	MFace *mface;
	int a, sel=0;
	
	if( is_uv_tface_editing_allowed()==0 ) return;
	me= get_mesh(OBACT);

	for(a=me->totface, tface= me->tface; a>0; a--, tface++) {
		if(tface->flag & TF_SELECT) {	
			if(tface->flag & (TF_SEL1+TF_SEL2+TF_SEL3+TF_SEL4)) {
				sel= 1;
				break;
			}
		}
	}
	
	mface= me->mface;
	for(a=me->totface, tface= me->tface; a>0; a--, tface++, mface++) {
		if(tface->flag & TF_SELECT) {
			if(mface->v4) {
				if(sel) tface->flag &= ~(TF_SEL1+TF_SEL2+TF_SEL3+TF_SEL4);
				else tface->flag |= (TF_SEL1+TF_SEL2+TF_SEL3+TF_SEL4);
			}
			else if(mface->v3) {
				if(sel) tface->flag &= ~(TF_SEL1+TF_SEL2+TF_SEL3+TF_SEL4);
				else tface->flag |= (TF_SEL1+TF_SEL2+TF_SEL3);
			}
		}
	}
	
	allqueue(REDRAWIMAGE, 0);
}

void mouse_select_sima(void)
{
	Mesh *me;
	TFace *tface;
	MFace *mface;
	int temp, dist=100;
	int a;
	short mval[2], uval[2], val = 0;
	char *flagpoin =0;
	
	if( is_uv_tface_editing_allowed()==0 ) return;
	me= get_mesh(OBACT);
	
	getmouseco_areawin(mval);	

	mface= me->mface;
	for(a=me->totface, tface= me->tface; a>0; a--, tface++, mface++) {
		
		if(tface->flag & TF_SELECT) {
		
			uvco_to_areaco_noclip(tface->uv[0], uval);
			temp= abs(mval[0]- uval[0])+ abs(mval[1]- uval[1]);
			if( tface->flag & TF_SEL1) temp+=5;
			if(temp<dist) { 
				flagpoin= &tface->flag;
				dist= temp; 
				val= TF_SEL1;
			}
	
			uvco_to_areaco_noclip(tface->uv[1], uval);
			temp= abs(mval[0]- uval[0])+ abs(mval[1]- uval[1]);
			if( tface->flag & TF_SEL2) temp+=5;
			if(temp<dist) { 
				flagpoin= &tface->flag;
				dist= temp; 
				val= TF_SEL2;
			}
	
			uvco_to_areaco_noclip(tface->uv[2], uval);
			temp= abs(mval[0]- uval[0])+ abs(mval[1]- uval[1]);
			if( tface->flag & TF_SEL3) temp+=5;
			if(temp<dist) { 
				flagpoin= &tface->flag;
				dist= temp; 
				val= TF_SEL3;
			}
	
			if(mface->v4) {
				uvco_to_areaco_noclip(tface->uv[3], uval);
				temp= abs(mval[0]- uval[0])+ abs(mval[1]- uval[1]);
				if( tface->flag & TF_SEL4) temp+=5;
				if(temp<dist) { 
					flagpoin= &tface->flag;
					dist= temp; 
					val= TF_SEL4;
				}
			}
			
		}
	}
	
	if(flagpoin) {
		if(G.qual & LR_SHIFTKEY) {
			if(*flagpoin & val) *flagpoin &= ~val;
			else *flagpoin |= val;
		}
		else {
			for(a=me->totface, tface= me->tface; a>0; a--, tface++) {
				if(tface->flag & TF_SELECT) {
					tface->flag &= ~(TF_SEL1+TF_SEL2+TF_SEL3+TF_SEL4);
				}
			}
			
			*flagpoin |= val;
		}
		
	
		glDrawBuffer(GL_FRONT);
		draw_tfaces();
		glFlush(); // at OSX, a flush pops up the "frontbuffer" (it does a swap, doh!)
		glDrawBuffer(GL_BACK);
		
		std_rmouse_transform(transform_tface_uv);
	}
}

void borderselect_sima(void)
{
	Mesh *me;
	TFace *tface;
	MFace *mface;
	rcti rect;
	rctf rectf;
	int a, val;
	short mval[2];

	if( is_uv_tface_editing_allowed()==0 ) return;
	me= get_mesh(OBACT);

	val= get_border(&rect, 3);

	if(val) {
		mval[0]= rect.xmin;
		mval[1]= rect.ymin;
		areamouseco_to_ipoco(G.v2d, mval, &rectf.xmin, &rectf.ymin);
		mval[0]= rect.xmax;
		mval[1]= rect.ymax;
		areamouseco_to_ipoco(G.v2d, mval, &rectf.xmax, &rectf.ymax);

		mface= me->mface;
		for(a=me->totface, tface= me->tface; a>0; a--, tface++, mface++) {
		
			if(tface->flag & TF_SELECT) {
				
				if(BLI_in_rctf(&rectf, (float)tface->uv[0][0], (float)tface->uv[0][1])) {
					if(val==LEFTMOUSE) tface->flag |= TF_SEL1;
					else tface->flag &= ~TF_SEL1;
				}
				if(BLI_in_rctf(&rectf, (float)tface->uv[1][0], (float)tface->uv[1][1])) {
					if(val==LEFTMOUSE) tface->flag |= TF_SEL2;
					else tface->flag &= ~TF_SEL2;
				}
				if(BLI_in_rctf(&rectf, (float)tface->uv[2][0], (float)tface->uv[2][1])) {
					if(val==LEFTMOUSE) tface->flag |= TF_SEL3;
					else tface->flag &= ~TF_SEL3;
				}
				if(mface->v4 && BLI_in_rctf(&rectf, (float)tface->uv[3][0], (float)tface->uv[3][1])) {
					if(val==LEFTMOUSE) tface->flag |= TF_SEL4;
					else tface->flag &= ~TF_SEL4;
				}
			}
							
		}
		scrarea_queue_winredraw(curarea);
	}
}

/** This is an ugly function to set the Tface selection flags depending
  * on whether its UV coordinates are inside the normalized 
  * area with radius rad and offset offset. These coordinates must be
  * normalized to 1.0 
  * Just for readability...
  */

void sel_uvco_inside_radius(short sel, TFace *tface, int index, float *offset, float *ell, short select_mask)
{
	// normalized ellipse: ell[0] = scaleX,
	//                        [1] = scaleY

	float *uv = tface->uv[index];
	float x, y, r2;

	x = (uv[0] - offset[0]) * ell[0];
	y = (uv[1] - offset[1]) * ell[1];

	r2 = x * x + y * y;
	if (r2 < 1.0) {
		if (sel == LEFTMOUSE) tface->flag |= select_mask;
		else tface->flag &= ~select_mask;
	}
}

// see below:
/** gets image dimensions of the 2D view 'v' */
static void getSpaceImageDimension(SpaceImage *sima, float *xy)
{
	Image *img = sima->image;
	float z;

	z = sima->zoom;

	if (img) {
		xy[0] = img->ibuf->x * z;
		xy[1] = img->ibuf->y * z;
	} else {
		xy[0] = 256 * z;
		xy[1] = 256 * z;
	}
}

/** Callback function called by circle_selectCB to enable 
  * brush select in UV editor.
  */

void uvedit_selectionCB(short selecting, Object *editobj, short *mval, float rad) 
{
	float offset[2];
	Mesh *me;
	MFace *mface;
	TFace *tface;
	int i;

	float ellipse[2]; // we need to deal with ellipses, as
	                  // non square textures require for circle
					  // selection. this ellipse is normalized; r = 1.0
	
	me = get_mesh(editobj);

	getSpaceImageDimension(curarea->spacedata.first, ellipse);
	ellipse[0] /= rad;
	ellipse[1] /= rad;

	areamouseco_to_ipoco(G.v2d, mval, &offset[0], &offset[1]);

	mface= me->mface;
	tface= me->tface;

	if (selecting) {
		for(i = 0; i < me->totface; i++) {
			sel_uvco_inside_radius(selecting, tface, 0, offset, ellipse, TF_SEL1);
			sel_uvco_inside_radius(selecting, tface, 1, offset, ellipse, TF_SEL2);
			sel_uvco_inside_radius(selecting, tface, 2, offset, ellipse, TF_SEL3);
			if (mface->v4)
				sel_uvco_inside_radius(selecting, tface, 3, offset, ellipse, TF_SEL4);
			
			tface++; mface++;

		}
	// force_draw() is no good here...
		glDrawBuffer(GL_FRONT);
		draw_tfaces();
		glDrawBuffer(GL_BACK);
	}	
}


void mouseco_to_curtile(void)
{
	float fx, fy;
	short mval[2];
	
	if( is_uv_tface_editing_allowed()==0) return;

	if(G.sima->image && G.sima->image->tpageflag & IMA_TILES) {
		
		G.sima->flag |= SI_EDITTILE;
		
		while(get_mbut()&L_MOUSE) {
			
			calc_image_view(G.sima, 'f');
			
			getmouseco_areawin(mval);
			areamouseco_to_ipoco(G.v2d, mval, &fx, &fy);

			if(fx>=0.0 && fy>=0.0 && fx<1.0 && fy<1.0) {
			
				fx= (fx)*G.sima->image->xrep;
				fy= (fy)*G.sima->image->yrep;
				
				mval[0]= fx;
				mval[1]= fy;
				
				G.sima->curtile= mval[1]*G.sima->image->xrep + mval[0];
			}

			scrarea_do_windraw(curarea);
			screen_swapbuffers();
		
		}
		
		G.sima->flag &= ~SI_EDITTILE;

		image_changed(G.sima, 1);

		allqueue(REDRAWVIEW3D, 0);
		scrarea_queue_winredraw(curarea);
	}
}
