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
 * Trackball math (in calctrackballvec())  Copyright (C) Silicon Graphics, Inc.
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

#ifdef WIN32
#include <io.h>
#include "BLI_winstuff.h"
#else
#include <unistd.h>
#endif   

#include "MEM_guardedalloc.h"

#include "BLI_blenlib.h"
#include "BLI_arithb.h"

#include "DNA_object_types.h"
#include "DNA_screen_types.h"
#include "DNA_scene_types.h"
#include "DNA_space_types.h"
#include "DNA_view3d_types.h"
#include "DNA_camera_types.h"
#include "DNA_lamp_types.h"
#include "DNA_userdef_types.h"

#include "BKE_utildefines.h"
#include "BKE_object.h"
#include "BKE_global.h"
#include "BKE_main.h"

#include "BIF_gl.h"
#include "BIF_space.h"
#include "BIF_mywindow.h"
#include "BIF_screen.h"
#include "BIF_toolbox.h"

#include "BSE_view.h"
#include "BSE_edit.h"		/* For countall */
#include "BSE_drawview.h"	/* For inner_play_anim_loop */

#include "BDR_drawobject.h"	/* For draw_object */

#include "mydevice.h"
#include "blendef.h"

/* Modules used */
#include "render.h"

#define TRACKBALLSIZE  (1.1)

void persp_general(int a)
{
	/* for all window types, not 3D */
	
	if(a== 0) {
		glPushMatrix();
		glMatrixMode(GL_PROJECTION);
		glPushMatrix();
		glMatrixMode(GL_MODELVIEW);

		myortho2(-0.375, ((float)(curarea->winx))-0.375, -0.375, ((float)(curarea->winy))-0.375);
		glLoadIdentity();
	}
	else if(a== 1) {
		glMatrixMode(GL_PROJECTION);
		glPopMatrix();
		glMatrixMode(GL_MODELVIEW);
		glPopMatrix();
	}
}

void persp(int a)
{
	/* only 3D windows */

	if(curarea->spacetype!=SPACE_VIEW3D) persp_general(a);
	else if(a == PERSP_STORE) {		// only store
		glMatrixMode(GL_PROJECTION);
		mygetmatrix(G.vd->winmat1);	
		glMatrixMode(GL_MODELVIEW);
		mygetmatrix(G.vd->viewmat1); 
	}
	else if(a== PERSP_WIN) {		// only set
		myortho2(-0.375, (float)(curarea->winx)-0.375, -0.375, (float)(curarea->winy)-0.375);
		glLoadIdentity();
	}
	else if(a== PERSP_VIEW) {
		glMatrixMode(GL_PROJECTION);
		myloadmatrix(G.vd->winmat1); // put back
		Mat4CpyMat4(curarea->winmat, G.vd->winmat1); // to be sure? 
		glMatrixMode(GL_MODELVIEW); 
		myloadmatrix(G.vd->viewmat); // put back
		
	}
}


float zfac=1.0;

void initgrabz(float x, float y, float z)
{
	if(G.vd==0) return;
	zfac= G.vd->persmat[0][3]*x+ G.vd->persmat[1][3]*y+ G.vd->persmat[2][3]*z+ G.vd->persmat[3][3];
}

void window_to_3d(float *vec, short mx, short my)
{
	/* always call initzgrab */
	float dx, dy;
	float fmx, fmy, winx, winy;
	
	/* stupid! */
	winx= curarea->winx;
	winy= curarea->winy;
	fmx= mx;
	fmy= my;
	
	dx= (2.0*fmx)/winx;
	dx*= zfac;
	dy= (2.0*fmy)/winy;
	dy*= zfac;

	vec[0]= (G.vd->persinv[0][0]*dx + G.vd->persinv[1][0]*dy);
	vec[1]= (G.vd->persinv[0][1]*dx + G.vd->persinv[1][1]*dy);
	vec[2]= (G.vd->persinv[0][2]*dx + G.vd->persinv[1][2]*dy);
}

void project_short(float *vec, short *adr)	/* clips */
{
	float fx, fy, vec4[4];

	adr[0]= 3200;
	VECCOPY(vec4, vec);
	vec4[3]= 1.0;
	
	Mat4MulVec4fl(G.vd->persmat, vec4);

	if( vec4[3]>0.1 ) {
		fx= (curarea->winx/2)+(curarea->winx/2)*vec4[0]/vec4[3];
		
		if( fx>0 && fx<curarea->winx) {
		
			fy= (curarea->winy/2)+(curarea->winy/2)*vec4[1]/vec4[3];
			
			if(fy>0.0 && fy< (float)curarea->winy) {
				adr[0]= floor(fx+0.5); 
				adr[1]= floor(fy+0.5);
			}
		}
	}
}

void project_short_noclip(float *vec, short *adr)
{
	float fx, fy, vec4[4];

	adr[0]= 3200;
	VECCOPY(vec4, vec);
	vec4[3]= 1.0;
	
	Mat4MulVec4fl(G.vd->persmat, vec4);

	if( vec4[3]>0.1 ) {
		fx= (curarea->winx/2)+(curarea->winx/2)*vec4[0]/vec4[3];
		
		if( fx>-32700 && fx<32700) {
		
			fy= (curarea->winy/2)+(curarea->winy/2)*vec4[1]/vec4[3];
			
			if(fy>-32700.0 && fy<32700.0) {
				adr[0]= floor(fx+0.5); 
				adr[1]= floor(fy+0.5);
			}
		}
	}
}

void project_float(float *vec, float *adr)
{
	float vec4[4];
	
	adr[0]= 3200.0;
	VECCOPY(vec4, vec);
	vec4[3]= 1.0;
	
	Mat4MulVec4fl(G.vd->persmat, vec4);

	if( vec4[3]>0.1 ) {
		adr[0]= (curarea->winx/2.0)+(curarea->winx/2.0)*vec4[0]/vec4[3];	
		adr[1]= (curarea->winy/2.0)+(curarea->winy/2.0)*vec4[1]/vec4[3];
	}
}

int boundbox_clip(float obmat[][4], BoundBox *bb)
{
	/* return 1: draw */
	
	float mat[4][4];
	float vec[4], min, max;
	int a, flag= -1, fl;
	
	if(bb==0) return 1;
	
	Mat4MulMat4(mat, obmat, G.vd->persmat);

	for(a=0; a<8; a++) {
		VECCOPY(vec, bb->vec[a]);
		vec[3]= 1.0;
		Mat4MulVec4fl(mat, vec);
		max= vec[3];
		min= -vec[3];

		fl= 0;
		if(vec[0] < min) fl+= 1;
		if(vec[0] > max) fl+= 2;
		if(vec[1] < min) fl+= 4;
		if(vec[1] > max) fl+= 8;
		if(vec[2] < min) fl+= 16;
		if(vec[2] > max) fl+= 32;
		
		flag &= fl;
		if(flag==0) return 1;
	}

	return 0;

}

void fdrawline(float x1, float y1, float x2, float y2)
{
	float v[2];

	glBegin(GL_LINE_STRIP);
	v[0] = x1; v[1] = y1;
	glVertex2fv(v);
	v[0] = x2; v[1] = y2;
	glVertex2fv(v);
	glEnd();
}

void fdrawbox(float x1, float y1, float x2, float y2)
{
	float v[2];

	glBegin(GL_LINE_STRIP);

	v[0] = x1; v[1] = y1;
	glVertex2fv(v);
	v[0] = x1; v[1] = y2;
	glVertex2fv(v);
	v[0] = x2; v[1] = y2;
	glVertex2fv(v);
	v[0] = x2; v[1] = y1;
	glVertex2fv(v);
	v[0] = x1; v[1] = y1;
	glVertex2fv(v);

	glEnd();
}

void sdrawline(short x1, short y1, short x2, short y2)
{
	short v[2];

	glBegin(GL_LINE_STRIP);
	v[0] = x1; v[1] = y1;
	glVertex2sv(v);
	v[0] = x2; v[1] = y2;
	glVertex2sv(v);
	glEnd();
}

void sdrawbox(short x1, short y1, short x2, short y2)
{
	short v[2];

	glBegin(GL_LINE_STRIP);

	v[0] = x1; v[1] = y1;
	glVertex2sv(v);
	v[0] = x1; v[1] = y2;
	glVertex2sv(v);
	v[0] = x2; v[1] = y2;
	glVertex2sv(v);
	v[0] = x2; v[1] = y1;
	glVertex2sv(v);
	v[0] = x1; v[1] = y1;
	glVertex2sv(v);

	glEnd();
}

/* the central math in this function was copied from trackball.cpp, sample code from the 
   Developers Toolbox series by SGI. */

/* trackball: better one than a full spherical solution */

void calctrackballvecfirst(rcti *area, short *mval, float *vec)
{
	float x, y, radius, d, z, t;
	
	radius= TRACKBALLSIZE;
	
	/* normalise x and y */
	x= (area->xmax + area->xmin)/2 -mval[0];
	x/= (float)((area->xmax - area->xmin)/2);
	y= (area->ymax + area->ymin)/2 -mval[1];
	y/= (float)((area->ymax - area->ymin)/2);
	
	d = sqrt(x*x + y*y);
	if (d < radius*M_SQRT1_2)  	/* Inside sphere */
		z = sqrt(radius*radius - d*d);
	else
	{ 			/* On hyperbola */
		t = radius / M_SQRT2;
		z = t*t / d;
	}

	vec[0]= x;
	vec[1]= y;
	vec[2]= -z;		/* yah yah! */

	if( fabs(vec[2])>fabs(vec[1]) && fabs(vec[2])>fabs(vec[0]) ) {
		vec[0]= 0.0;
		vec[1]= 0.0;
		if(vec[2]>0.0) vec[2]= 1.0; else vec[2]= -1.0;
	}
	else if( fabs(vec[1])>fabs(vec[0]) && fabs(vec[1])>fabs(vec[2]) ) {
		vec[0]= 0.0;
		vec[2]= 0.0;
		if(vec[1]>0.0) vec[1]= 1.0; else vec[1]= -1.0;
	}
	else  {
		vec[1]= 0.0;
		vec[2]= 0.0;
		if(vec[0]>0.0) vec[0]= 1.0; else vec[0]= -1.0;
	}
}

void calctrackballvec(rcti *area, short *mval, float *vec)
{
	float x, y, radius, d, z, t;
	
	radius= TRACKBALLSIZE;
	
	/* x en y normaliseren */
	x= (area->xmax + area->xmin)/2 -mval[0];
	x/= (float)((area->xmax - area->xmin)/4);
	y= (area->ymax + area->ymin)/2 -mval[1];
	y/= (float)((area->ymax - area->ymin)/2);
	
	d = sqrt(x*x + y*y);
	if (d < radius*M_SQRT1_2)  	/* Inside sphere */
		z = sqrt(radius*radius - d*d);
	else
	{ 			/* On hyperbola */
		t = radius / M_SQRT2;
		z = t*t / d;
	}

	vec[0]= x;
	vec[1]= y;
	vec[2]= -z;		/* yah yah! */

}

void viewmove(int mode)
{
	float firstvec[3], newvec[3], dvec[3];
	float oldquat[4], q1[4], si, phi;
	int firsttime=1;
	short mvalball[2], mval[2], mvalo[2];
	
	/* sometimes this routine is called from headerbuttons */
	areawinset(curarea->win);
	curarea->head_swap= 0;
	
	initgrabz(-G.vd->ofs[0], -G.vd->ofs[1], -G.vd->ofs[2]);
	
	QUATCOPY(oldquat, G.vd->viewquat);
	
	getmouseco_sc(mvalo);		/* work with screen coordinates because of trackball function */
	mvalball[0]= mvalo[0];			/* needed for turntable to work */
	mvalball[1]= mvalo[1];
	
	calctrackballvec(&curarea->winrct, mvalo, firstvec);

	/* cumultime(0); */

	while(TRUE) {
		getmouseco_sc(mval);
		
		if(mval[0]!=mvalo[0] || mval[1]!=mvalo[1] || (G.f & G_PLAYANIM)) {
			
			if(firsttime) {
				firsttime= 0;
				/* are we translating, rotating or zooming? */
				if(mode==0) {
					if(G.vd->view!=0) scrarea_queue_headredraw(curarea);	/*for button */
					G.vd->view= 0;
				}
						
				if(G.vd->persp==2 || (G.vd->persp==3 && mode!=1)) {
					G.vd->persp= 1;
					scrarea_do_windraw(curarea);
					scrarea_queue_headredraw(curarea);
				}
			}


			if(mode==0) {	/* view rotate */
			
				/* if turntable method, we don't change mvalball[0] */
			
				if(U.flag & TRACKBALL) mvalball[0]= mval[0];
				mvalball[1]= mval[1];
				
				calctrackballvec(&curarea->winrct, mvalball, newvec);
				
				VecSubf(dvec, newvec, firstvec);
				
				si= sqrt(dvec[0]*dvec[0]+ dvec[1]*dvec[1]+ dvec[2]*dvec[2]);
				si/= (2.0*TRACKBALLSIZE);
				
				/* is there an acceptable solution? (180 degrees limitor) */
				if(si<1.0) {
					Crossf(q1+1, firstvec, newvec);

					Normalise(q1+1);
		
					phi= asin(si);
	
					si= sin(phi);
					q1[0]= cos(phi);
					q1[1]*= si;
					q1[2]*= si;
					q1[3]*= si;
					
					QuatMul(G.vd->viewquat, q1, oldquat);

					if( (U.flag & TRACKBALL)==0 ) {
					
						/* rotate around z-axis (mouse x moves)  */
						
						phi= 2*(mval[0]-mvalball[0]);
						phi/= (float)curarea->winx;
						si= sin(phi);
						q1[0]= cos(phi);
						q1[1]= q1[2]= 0.0;
						q1[3]= si;
						
						QuatMul(G.vd->viewquat, G.vd->viewquat, q1);
					}
				}
			}
			else if(mode==1) {	/* translate */
				if(G.vd->persp==3) {
					/* zoom= 0.5+0.5*(float)(2<<G.vd->rt1); */
					/* dx-= (mval[0]-mvalo[0])/zoom; */
					/* dy-= (mval[1]-mvalo[1])/zoom; */
					/* G.vd->rt2= dx; */
					/* G.vd->rt3= dy; */
					/* if(G.vd->rt2<-320) G.vd->rt2= -320; */
					/* if(G.vd->rt2> 320) G.vd->rt2=  320; */
					/* if(G.vd->rt3<-250) G.vd->rt3= -250; */
					/* if(G.vd->rt3> 250) G.vd->rt3=  250; */
				}
				else {
					window_to_3d(dvec, mval[0]-mvalo[0], mval[1]-mvalo[1]);
					VecAddf(G.vd->ofs, G.vd->ofs, dvec);
				}
			}
			else if(mode==2) {
				G.vd->dist*= 1.0+(float)(mvalo[0]-mval[0]+mvalo[1]-mval[1])/1000.0;
				
				/* these limits are in toets.c too */
				if(G.vd->dist<0.001*G.vd->grid) G.vd->dist= 0.001*G.vd->grid;
				if(G.vd->dist>10.0*G.vd->far) G.vd->dist=10.0*G.vd->far;
				
				mval[1]= mvalo[1]; /* keeps zooming that way */
				mval[0]= mvalo[0];
			}
			
			mvalo[0]= mval[0];
			mvalo[1]= mval[1];

			if(G.f & G_PLAYANIM) inner_play_anim_loop(0, 0);
			if(G.f & G_SIMULATION) break;

			scrarea_do_windraw(curarea);
			screen_swapbuffers();
		}
		else {
			BIF_wait_for_statechange();
		}
		
		/* this in the end, otherwise get_mbut does not work on a PC... */
		if( !(get_mbut() & (L_MOUSE|M_MOUSE))) break;
	}

	curarea->head_swap= WIN_FRONT_OK;
}

short v3d_windowmode=0;

void setwinmatrixview3d(rctf *rect)		/* rect: for picking */
{
	Camera *cam=0;
	float d, near, far, winx = 0.0, winy = 0.0;
	float lens, dfac, tfac, fac, x1, y1, x2, y2;
	short orth;
	
	lens= G.vd->lens;
	near= G.vd->near;
	far= G.vd->far;
	
	if(G.vd->persp==2) {
		if(G.vd->camera) {
			if(G.vd->camera->type==OB_LAMP ) {
				Lamp *la;
				
				la= G.vd->camera->data;
				fac= cos( M_PI*la->spotsize/360.0);
				
				x1= saacos(fac);
				lens= 16.0*fac/sin(x1);
		
				near= la->clipsta;
				far= la->clipend;
			}
			else if(G.vd->camera->type==OB_CAMERA) {
				cam= G.vd->camera->data;
				lens= cam->lens;
				near= cam->clipsta;
				far= cam->clipend;
				
				if(cam->type==CAM_ORTHO) {
					lens*= 100.0;
					near= (near+1.0)*100.0;	/* otherwise zbuffer troubles. a Patch! */
					far*= 100.0;
				}
			}
		}
	}
	
	if(v3d_windowmode) {
		winx= R.rectx;
		winy= R.recty;
	}
	else {
		winx= curarea->winx;
		winy= curarea->winy;
	}
	
	if(winx>winy) d= 0.015625*winx*lens;
	else d= 0.015625*winy*lens;
	
	dfac= near/d;

	/* if(G.vd->persp==1 && G.vd->dproj>1.0) far= G.vd->dproj*far; */

	if(G.vd->persp==0) {
		/* x1= -winx*G.vd->dist/1000.0; */
		x1= -G.vd->dist;
		x2= -x1;
		y1= -winy*G.vd->dist/winx;
		y2= -y1;
		orth= 1;
	}
	else {
		if(G.vd->persp==2) {
			fac= (1.41421+( (float)G.vd->camzoom )/50.0);
			fac*= fac;
		}
		else fac= 2.0;
		
		x1= -dfac*(winx/fac);
		x2= -x1;
		y1= -dfac*(winy/fac);
		y2= -y1;
		
		if(G.vd->persp==2 && (G.special1 & G_HOLO)) {
			if(cam && (cam->flag & CAM_HOLO2)) {
				tfac= fac/4.0;	/* the fac is 1280/640 corrected for obszoom */

				if(cam->netend==0.0) cam->netend= EFRA;
				fac= (G.scene->r.cfra-1.0)/(cam->netend)-0.5;
				
				fac*= tfac*(x2-x1);
				fac*= ( cam->hololen1 );
				x1-= fac;
				x2-= fac;
			}
		}
		
		orth= 0;
	}

	if(rect) {		/* picking */
		rect->xmin/= winx;
		rect->xmin= x1+rect->xmin*(x2-x1);
		rect->ymin/= winy;
		rect->ymin= y1+rect->ymin*(y2-y1);
		rect->xmax/= winx;
		rect->xmax= x1+rect->xmax*(x2-x1);
		rect->ymax/= winy;
		rect->ymax= y1+rect->ymax*(y2-y1);

		if(orth) myortho(rect->xmin, rect->xmax, rect->ymin, rect->ymax, -far, far);
		else mywindow(rect->xmin, rect->xmax, rect->ymin, rect->ymax, near, far);

	}
	else {
		if(v3d_windowmode) {
			if(orth) i_ortho(x1, x2, y1, y2, -far, far, R.winmat);
			else {
				if(cam && cam->type==CAM_ORTHO) i_window(x1, x2, y1, y2, near, far, R.winmat);
				else i_window(x1, x2, y1, y2, near, far, R.winmat);
			}
		}
		else {
			if(orth) myortho(x1, x2, y1, y2, -far, far);
			else {
				if(cam && cam->type==CAM_ORTHO) mywindow(x1, x2, y1, y2, near, far);
				else mywindow(x1, x2, y1, y2, near, far);
			}
		}
	}

	if(v3d_windowmode==0) {
		glMatrixMode(GL_PROJECTION);
		mygetmatrix(curarea->winmat);
		glMatrixMode(GL_MODELVIEW);
	}
}


void obmat_to_viewmat(Object *ob)
{
	float bmat[4][4];
	float tmat[3][3];

	Mat4CpyMat4(bmat, ob->obmat);
	Mat4Ortho(bmat);
	Mat4Invert(G.vd->viewmat, bmat);
	
	/* view quat calculation, needed for add object */
	Mat3CpyMat4(tmat, G.vd->viewmat);
	Mat3ToQuat(tmat, G.vd->viewquat);
}


void setviewmatrixview3d()
{
	Camera *cam;

	if(G.special1 & G_HOLO) RE_holoview();

	if(G.vd->persp>=2) {	    /* obs/camera */
		if(G.vd->camera) {
			
			where_is_object(G.vd->camera);	
			obmat_to_viewmat(G.vd->camera);
			
			if(G.vd->camera->type==OB_CAMERA) {
				cam= G.vd->camera->data;
				if(cam->type==CAM_ORTHO) G.vd->viewmat[3][2]*= 100.0;
			}
		}
		else {
			QuatToMat4(G.vd->viewquat, G.vd->viewmat);
			G.vd->viewmat[3][2]-= G.vd->dist;
		}
	}
	else {
		
		QuatToMat4(G.vd->viewquat, G.vd->viewmat);
		if(G.vd->persp==1) G.vd->viewmat[3][2]-= G.vd->dist;
		i_translate(G.vd->ofs[0], G.vd->ofs[1], G.vd->ofs[2], G.vd->viewmat);
	}
}

void setcameratoview3d()
{
	Object *ob;
	float dvec[3];

	ob= G.vd->camera;
	dvec[0]= G.vd->dist*G.vd->viewinv[2][0];
	dvec[1]= G.vd->dist*G.vd->viewinv[2][1];
	dvec[2]= G.vd->dist*G.vd->viewinv[2][2];					
	VECCOPY(ob->loc, dvec);
	VecSubf(ob->loc, ob->loc, G.vd->ofs);
	G.vd->viewquat[0]= -G.vd->viewquat[0];
	if (ob->transflag & OB_QUAT) {
		QUATCOPY(ob->quat, G.vd->viewquat);
	} else {
		QuatToEul(G.vd->viewquat, ob->rot);
	}
	G.vd->viewquat[0]= -G.vd->viewquat[0];
}

/* IGLuint-> GLuint*/
short selectprojektie(unsigned int *buffer, short x1, short y1, short x2, short y2)
{
	rctf rect;
	Base *base;
	short mval[2], code, hits;

	G.f |= G_PICKSEL;
	
	if(x1==0 && x2==0 && y1==0 && y2==0) {
		getmouseco_areawin(mval);
		rect.xmin= mval[0]-7;
		rect.xmax= mval[0]+7;
		rect.ymin= mval[1]-7;
		rect.ymax= mval[1]+7;
	}
	else {
		rect.xmin= x1;
		rect.xmax= x2;
		rect.ymin= y1;
		rect.ymax= y2;
	}
	/* get rid of overlay button matrix */
	persp(PERSP_VIEW);
	setwinmatrixview3d(&rect);
	Mat4MulMat4(G.vd->persmat, G.vd->viewmat, curarea->winmat);
	
	if(G.vd->drawtype > OB_WIRE) {
		G.zbuf= TRUE;
		glEnable(GL_DEPTH_TEST);
	}

	glSelectBuffer( MAXPICKBUF, (GLuint *)buffer);
	glRenderMode(GL_SELECT);
	glInitNames();	/* these two calls whatfor? It doesnt work otherwise */
	glPushName(-1);
	code= 1;
	
	if(G.obedit && G.obedit->type==OB_MBALL) {
		draw_object(BASACT);
	}
	else if ((G.obedit && G.obedit->type==OB_ARMATURE)||(G.obpose && G.obpose->type==OB_ARMATURE)) {
		draw_object(BASACT);
	}
	else {
		base= G.scene->base.first;
		while(base) {
			if(base->lay & G.vd->lay) {
				base->selcol= code;
				glLoadName(code);
				draw_object(base);
				code++;
			}
			base= base->next;
		}
	}
	glPopName();	/* see above (pushname) */
	hits= glRenderMode(GL_RENDER);
	if(hits<0) error("Too many objects in selectbuf");

	G.f &= ~G_PICKSEL;
	setwinmatrixview3d(0);
	Mat4MulMat4(G.vd->persmat, G.vd->viewmat, curarea->winmat);
	
	if(G.vd->drawtype > OB_WIRE) {
		G.zbuf= 0;
		glDisable(GL_DEPTH_TEST);
	}
	persp(PERSP_WIN);

	return hits;
}

float *give_cursor()
{
	if(G.vd && G.vd->localview) return G.vd->cursor;
	else return G.scene->cursor;
}

unsigned int free_localbit()
{
	unsigned int lay;
	ScrArea *sa;
	bScreen *sc;
	
	lay= 0;
	
	/* sometimes we loose a localview: when an area is closed */
	/* check all areas: which localviews are in use? */
	sc= G.main->screen.first;
	while(sc) {
		sa= sc->areabase.first;
		while(sa) {
			SpaceLink *sl= sa->spacedata.first;
			while(sl) {
				if(sl->spacetype==SPACE_VIEW3D) {
					View3D *v3d= (View3D*) sl;
					lay |= v3d->lay;
				}
				sl= sl->next;
			}
			sa= sa->next;
		}
		sc= sc->id.next;
	}
	
	if( (lay & 0x01000000)==0) return 0x01000000;
	if( (lay & 0x02000000)==0) return 0x02000000;
	if( (lay & 0x04000000)==0) return 0x04000000;
	if( (lay & 0x08000000)==0) return 0x08000000;
	if( (lay & 0x10000000)==0) return 0x10000000;
	if( (lay & 0x20000000)==0) return 0x20000000;
	if( (lay & 0x40000000)==0) return 0x40000000;
	if( (lay & 0x80000000)==0) return 0x80000000;
	
	return 0;
}


void initlocalview()
{
	Base *base;
	float size = 0.0, min[3], max[3], afm[3];
	unsigned int locallay;
	int ok=0;

	if(G.vd->localvd) return;

	min[0]= min[1]= min[2]= 1.0e10;
	max[0]= max[1]= max[2]= -1.0e10;

	locallay= free_localbit();

	if(locallay==0) {
		error("Sorry,  no more than 8 localviews");
		ok= 0;
	}
	else {
		if(G.obedit) {
			minmax_object(G.obedit, min, max);
			
			ok= 1;
		
			BASACT->lay |= locallay;
			G.obedit->lay= BASACT->lay;
		}
		else {
			base= FIRSTBASE;
			while(base) {
				if TESTBASE(base)  {
					minmax_object(base->object, min, max);
					base->lay |= locallay;
					base->object->lay= base->lay;
					ok= 1;
				}
				base= base->next;
			}
		}
		
		afm[0]= (max[0]-min[0]);
		afm[1]= (max[1]-min[1]);
		afm[2]= (max[2]-min[2]);
		size= MAX3(afm[0], afm[1], afm[2]);
		if(size<=0.01) size= 0.01;
	}
	
	if(ok) {
		G.vd->localvd= MEM_mallocN(sizeof(View3D), "localview");
		memcpy(G.vd->localvd, G.vd, sizeof(View3D));

		G.vd->ofs[0]= -(min[0]+max[0])/2.0;
		G.vd->ofs[1]= -(min[1]+max[1])/2.0;
		G.vd->ofs[2]= -(min[2]+max[2])/2.0;

		G.vd->dist= size;

		if(G.vd->persp>1) {
			G.vd->persp= 1;
			
		}
		G.vd->near= 0.1;
		G.vd->cursor[0]= -G.vd->ofs[0];
		G.vd->cursor[1]= -G.vd->ofs[1];
		G.vd->cursor[2]= -G.vd->ofs[2];

		G.vd->lay= locallay;
		
		countall();
		scrarea_queue_winredraw(curarea);
	}
	else {
		/* clear flags */
		base= FIRSTBASE;
		while(base) {
			if( base->lay & locallay ) {
				base->lay-= locallay;
				if(base->lay==0) base->lay= G.vd->layact;
				if(base->object != G.obedit) base->flag |= SELECT;
				base->object->lay= base->lay;
			}
			base= base->next;
		}
		scrarea_queue_headredraw(curarea);
		
		G.vd->localview= 0;
	}
}

void centreview()	/* like a localview without local! */
{
	Base *base;
	float size, min[3], max[3], afm[3];
	int ok=0;

	min[0]= min[1]= min[2]= 1.0e10;
	max[0]= max[1]= max[2]= -1.0e10;

	if(G.obedit) {
		minmax_object(G.obedit, min, max);
		
		ok= 1;
	}
	else {
		base= FIRSTBASE;
		while(base) {
			if TESTBASE(base)  {
				minmax_object(base->object, min, max);
				ok= 1;
			}
			base= base->next;
		}
	}
	
	if(ok==0) return;
	
	afm[0]= (max[0]-min[0]);
	afm[1]= (max[1]-min[1]);
	afm[2]= (max[2]-min[2]);
	size= MAX3(afm[0], afm[1], afm[2]);
	
	if(size<=0.01) size= 0.01;
	
	

	G.vd->ofs[0]= -(min[0]+max[0])/2.0;
	G.vd->ofs[1]= -(min[1]+max[1])/2.0;
	G.vd->ofs[2]= -(min[2]+max[2])/2.0;

	G.vd->dist= size;

	if(G.vd->persp>1) {
		G.vd->persp= 1;
		
	}
	G.vd->near= 0.1;
	G.vd->cursor[0]= -G.vd->ofs[0];
	G.vd->cursor[1]= -G.vd->ofs[1];
	G.vd->cursor[2]= -G.vd->ofs[2];

	scrarea_queue_winredraw(curarea);

}


void restore_localviewdata(View3D *vd)
{
	if(vd->localvd==0) return;
	
	VECCOPY(vd->ofs, vd->localvd->ofs);
	vd->dist= vd->localvd->dist;
	vd->persp= vd->localvd->persp;
	vd->view= vd->localvd->view;
	vd->near= vd->localvd->near;
	vd->far= vd->localvd->far;
	vd->lay= vd->localvd->lay;
	vd->layact= vd->localvd->layact;
	vd->drawtype= vd->localvd->drawtype;
	vd->camera= vd->localvd->camera;
	QUATCOPY(vd->viewquat, vd->localvd->viewquat);
	
}

void endlocalview(ScrArea *sa)
{
	View3D *v3d;
	struct Base *base;
	unsigned int locallay;
	
	if(sa->spacetype!=SPACE_VIEW3D) return;
	v3d= sa->spacedata.first;
	
	if(v3d->localvd) {
		
		locallay= v3d->lay & 0xFF000000;
		
		restore_localviewdata(v3d);
		
		MEM_freeN(v3d->localvd);
		v3d->localvd= 0;
		v3d->localview= 0;

		/* for when in other window the layers have changed */
		if(v3d->scenelock) v3d->lay= G.scene->lay;
		
		base= FIRSTBASE;
		while(base) {
			if( base->lay & locallay ) {
				base->lay-= locallay;
				if(base->lay==0) base->lay= v3d->layact;
				if(base->object != G.obedit) base->flag |= SELECT;
				base->object->lay= base->lay;
			}
			base= base->next;
		}

		countall();
		allqueue(REDRAWVIEW3D, 0);	/* because of select */
		
	}
}

void view3d_home(int centre)
{
	Base *base;
	float size, min[3], max[3], afm[3];
	int ok= 1, onedone=0;

	if(centre) {
		min[0]= min[1]= min[2]= 0.0;
		max[0]= max[1]= max[2]= 0.0;
	}
	else {
		min[0]= min[1]= min[2]= 1.0e10;
		max[0]= max[1]= max[2]= -1.0e10;
	}
	
	base= FIRSTBASE;
	if(base==0) return;
	while(base) {
		if(base->lay & G.vd->lay) {
			onedone= 1;
			minmax_object(base->object, min, max);
		}
		base= base->next;
	}
	if(!onedone) return;
	
	afm[0]= (max[0]-min[0]);
	afm[1]= (max[1]-min[1]);
	afm[2]= (max[2]-min[2]);
	size= MAX3(afm[0], afm[1], afm[2]);
	if(size==0.0) ok= 0;
		
	if(ok) {

		G.vd->ofs[0]= -(min[0]+max[0])/2.0;
		G.vd->ofs[1]= -(min[1]+max[1])/2.0;
		G.vd->ofs[2]= -(min[2]+max[2])/2.0;

		G.vd->dist= size;
		
		if(G.vd->persp==2) G.vd->persp= 1;
		
		scrarea_queue_winredraw(curarea);
	}
}


void view3d_align_axis_to_vector(View3D *v3d, int axisidx, float vec[3])
{
	float alignaxis[3];
	float norm[3], axis[3], angle;

	alignaxis[0]= alignaxis[1]= alignaxis[2]= 0.0;
	alignaxis[axisidx]= 1.0;

	norm[0]= vec[0], norm[1]= vec[1], norm[2]= vec[2];
	Normalise(norm);

	angle= acos(Inpf(alignaxis, norm));
	Crossf(axis, alignaxis, norm);
	VecRotToQuat(axis, -angle, v3d->viewquat);

	v3d->view= 0;
	if (v3d->persp>=2) v3d->persp= 0; /* switch out of camera mode */
}

