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
 * Trackball math (in calctrackballvec())  Copyright (C) Silicon Graphics, Inc.
 *
 * The Original Code is: all of this file.
 *
 * Contributor(s): none yet.
 *
 * ***** END GPL LICENSE BLOCK *****
 */

#include <math.h>
#include <string.h>

#ifdef HAVE_CONFIG_H
#include <config.h>
#endif

#ifdef WIN32
#include <io.h>
#else
#include <unistd.h>
#endif   

#include "MEM_guardedalloc.h"

#include "BLI_blenlib.h"
#include "BLI_arithb.h"

#include "DNA_action_types.h"
#include "DNA_armature_types.h"
#include "DNA_camera_types.h"
#include "DNA_lamp_types.h"
#include "DNA_object_types.h"
#include "DNA_screen_types.h"
#include "DNA_scene_types.h"
#include "DNA_space_types.h"
#include "DNA_userdef_types.h"
#include "DNA_view3d_types.h"

#include "BKE_action.h"
#include "BKE_anim.h"
#include "BKE_global.h"
#include "BKE_main.h"
#include "BKE_object.h"
#include "BKE_sculpt.h"
#include "BKE_utildefines.h"

#include "BIF_transform.h"
#include "BIF_editparticle.h"
#include "BIF_gl.h"
#include "BIF_previewrender.h"
#include "BIF_mywindow.h"
#include "BIF_retopo.h"
#include "BIF_space.h"
#include "BIF_screen.h"
#include "BIF_toolbox.h"

#include "BSE_view.h"
#include "BSE_edit.h"		/* For countall */
#include "BSE_drawview.h"	/* For inner_play_anim_loop */

#include "BDR_drawobject.h"	/* For draw_object */
#include "BDR_editface.h"	/* For minmax_tface */
#include "BDR_sculptmode.h"

#include "mydevice.h"
#include "blendef.h"
#include "transform.h"

#include "PIL_time.h" /* smoothview */
#include <float.h>

#define TRACKBALLSIZE  (1.1)
#define BL_NEAR_CLIP 0.001


/* local prototypes ----------*/
void setcameratoview3d(void); /* windows.c & toets.c */

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

/* create intersection ray in view Z direction at mouse coordinates */
void viewray(short mval[2], float ray_start[3], float ray_normal[3])
{
	float ray_end[3];
	viewline(mval, ray_start, ray_end);
	VecSubf(ray_normal, ray_end, ray_start);
	Normalize(ray_normal);
}

/* create intersection coordinates in view Z direction at mouse coordinates */
void viewline(short mval[2], float ray_start[3], float ray_end[3])
{
	float vec[4];
	
	if(G.vd->persp != V3D_ORTHO){
		vec[0]= 2.0f * mval[0] / curarea->winx - 1;
		vec[1]= 2.0f * mval[1] / curarea->winy - 1;
		vec[2]= -1.0f;
		vec[3]= 1.0f;

		Mat4MulVec4fl(G.vd->persinv, vec);
		VecMulf(vec, 1.0f / vec[3]);

		VECCOPY(ray_start, G.vd->viewinv[3]);
		VECSUB(vec, vec, ray_start);
		Normalize(vec);

		VECADDFAC(ray_start, G.vd->viewinv[3], vec, G.vd->near);
		VECADDFAC(ray_end, G.vd->viewinv[3], vec, G.vd->far);
	}
	else {
		vec[0] = 2.0f * mval[0] / curarea->winx - 1;
		vec[1] = 2.0f * mval[1] / curarea->winy - 1;
		vec[2] = 0.0f;
		vec[3] = 1.0f;

		Mat4MulVec4fl(G.vd->persinv, vec);

		VECADDFAC(ray_start, vec, G.vd->viewinv[2],  1000.0f);
		VECADDFAC(ray_end, vec, G.vd->viewinv[2], -1000.0f);
	}
}

void initgrabz(float x, float y, float z)
{
	if(G.vd==NULL) return;
	G.vd->zfac= G.vd->persmat[0][3]*x+ G.vd->persmat[1][3]*y+ G.vd->persmat[2][3]*z+ G.vd->persmat[3][3];

	/* if x,y,z is exactly the viewport offset, zfac is 0 and we don't want that 
	 * (accounting for near zero values)
	 * */
	if (G.vd->zfac < 1.e-6f && G.vd->zfac > -1.e-6f) G.vd->zfac = 1.0f;
	
	/* Negative zfac means x, y, z was behind the camera (in perspective).
	 * This gives flipped directions, so revert back to ok default case.
	 */
	if (G.vd->zfac < 0.0f) G.vd->zfac = 1.0f;
}

void window_to_3d(float *vec, short mx, short my)
{
	/* always call initgrabz */
	float dx, dy;
	
	dx= 2.0f*mx*G.vd->zfac/curarea->winx;
	dy= 2.0f*my*G.vd->zfac/curarea->winy;
	
	vec[0]= (G.vd->persinv[0][0]*dx + G.vd->persinv[1][0]*dy);
	vec[1]= (G.vd->persinv[0][1]*dx + G.vd->persinv[1][1]*dy);
	vec[2]= (G.vd->persinv[0][2]*dx + G.vd->persinv[1][2]*dy);
}

void project_short(float *vec, short *adr)	/* clips */
{
	float fx, fy, vec4[4];

	adr[0]= IS_CLIPPED;
	
	if(G.vd->flag & V3D_CLIPPING) {
		if(view3d_test_clipping(G.vd, vec))
			return;
	}

	VECCOPY(vec4, vec);
	vec4[3]= 1.0;
	Mat4MulVec4fl(G.vd->persmat, vec4);
	
	if( vec4[3]>BL_NEAR_CLIP ) {	/* 0.001 is the NEAR clipping cutoff for picking */
		fx= (curarea->winx/2)*(1 + vec4[0]/vec4[3]);
		
		if( fx>0 && fx<curarea->winx) {
		
			fy= (curarea->winy/2)*(1 + vec4[1]/vec4[3]);
			
			if(fy>0.0 && fy< (float)curarea->winy) {
				adr[0]= floor(fx); 
				adr[1]= floor(fy);
			}
		}
	}
}

void project_int(float *vec, int *adr)
{
	float fx, fy, vec4[4];

	adr[0]= 2140000000.0f;
	VECCOPY(vec4, vec);
	vec4[3]= 1.0;
	
	Mat4MulVec4fl(G.vd->persmat, vec4);

	if( vec4[3]>BL_NEAR_CLIP ) {	/* 0.001 is the NEAR clipping cutoff for picking */
		fx= (curarea->winx/2)*(1 + vec4[0]/vec4[3]);
		
		if( fx>-2140000000.0f && fx<2140000000.0f) {
			fy= (curarea->winy/2)*(1 + vec4[1]/vec4[3]);
			
			if(fy>-2140000000.0f && fy<2140000000.0f) {
				adr[0]= floor(fx); 
				adr[1]= floor(fy);
			}
		}
	}
}

void project_int_noclip(float *vec, int *adr)
{
	float fx, fy, vec4[4];

	VECCOPY(vec4, vec);
	vec4[3]= 1.0;
	
	Mat4MulVec4fl(G.vd->persmat, vec4);

	if( fabs(vec4[3]) > BL_NEAR_CLIP ) {
		fx = (curarea->winx/2)*(1 + vec4[0]/vec4[3]);
		fy = (curarea->winy/2)*(1 + vec4[1]/vec4[3]);
			
		adr[0] = floor(fx); 
		adr[1] = floor(fy);
	}
	else
	{
		adr[0] = curarea->winx / 2;
		adr[1] = curarea->winy / 2;
	}
}

void project_short_noclip(float *vec, short *adr)
{
	float fx, fy, vec4[4];

	adr[0]= IS_CLIPPED;
	VECCOPY(vec4, vec);
	vec4[3]= 1.0;
	
	Mat4MulVec4fl(G.vd->persmat, vec4);

	if( vec4[3]>BL_NEAR_CLIP ) {	/* 0.001 is the NEAR clipping cutoff for picking */
		fx= (curarea->winx/2)*(1 + vec4[0]/vec4[3]);
		
		if( fx>-32700 && fx<32700) {
		
			fy= (curarea->winy/2)*(1 + vec4[1]/vec4[3]);
			
			if(fy>-32700.0 && fy<32700.0) {
				adr[0]= floor(fx); 
				adr[1]= floor(fy);
			}
		}
	}
}

void project_float(float *vec, float *adr)
{
	float vec4[4];
	
	adr[0]= IS_CLIPPED;
	VECCOPY(vec4, vec);
	vec4[3]= 1.0;
	
	Mat4MulVec4fl(G.vd->persmat, vec4);

	if( vec4[3]>BL_NEAR_CLIP ) {
		adr[0] = (curarea->winx/2.0)+(curarea->winx/2.0)*vec4[0]/vec4[3];	
		adr[1] = (curarea->winy/2.0)+(curarea->winy/2.0)*vec4[1]/vec4[3];
	}
}

void project_float_noclip(float *vec, float *adr)
{
	float vec4[4];

	VECCOPY(vec4, vec);
	vec4[3]= 1.0;
	
	Mat4MulVec4fl(G.vd->persmat, vec4);

	if( fabs(vec4[3]) > BL_NEAR_CLIP ) {
		adr[0] = (curarea->winx/2.0)+(curarea->winx/2.0)*vec4[0]/vec4[3];	
		adr[1] = (curarea->winy/2.0)+(curarea->winy/2.0)*vec4[1]/vec4[3];
	}
	else
	{
		adr[0] = curarea->winx / 2.0f;
		adr[1] = curarea->winy / 2.0f;
	}
}

void view3d_get_object_project_mat(ScrArea *area, Object *ob, float pmat[4][4], float vmat[4][4])
{
	if (area->spacetype!=SPACE_VIEW3D || !area->spacedata.first) {
		Mat4One(pmat);
		Mat4One(vmat);
	} else {
		View3D *vd = area->spacedata.first;

		Mat4MulMat4(vmat, ob->obmat, vd->viewmat);
		Mat4MulMat4(pmat, vmat, vd->winmat1);
		Mat4CpyMat4(vmat, ob->obmat);
	}
}

/* projectmat brings it to window coords, wmat to rotated world space */
void view3d_project_short_clip(ScrArea *area, float *vec, short *adr, float projmat[4][4], float wmat[4][4])
{
	View3D *v3d= area->spacedata.first;
	float fx, fy, vec4[4];

	adr[0]= IS_CLIPPED;
	
	/* clipplanes in eye space */
	if(v3d->flag & V3D_CLIPPING) {
		VECCOPY(vec4, vec);
		Mat4MulVecfl(wmat, vec4);
		if(view3d_test_clipping(v3d, vec4))
			return;
	}
	
	VECCOPY(vec4, vec);
	vec4[3]= 1.0;
	
	Mat4MulVec4fl(projmat, vec4);
	
	/* clipplanes in window space */
	if( vec4[3]>BL_NEAR_CLIP ) {	/* 0.001 is the NEAR clipping cutoff for picking */
		fx= (area->winx/2)*(1 + vec4[0]/vec4[3]);
		
		if( fx>0 && fx<area->winx) {
		
			fy= (area->winy/2)*(1 + vec4[1]/vec4[3]);
			
			if(fy>0.0 && fy< (float)area->winy) {
				adr[0]= floor(fx); 
				adr[1]= floor(fy);
			}
		}
	}
}

void view3d_project_short_noclip(ScrArea *area, float *vec, short *adr, float mat[4][4])
{
	float fx, fy, vec4[4];

	adr[0]= IS_CLIPPED;
	
	VECCOPY(vec4, vec);
	vec4[3]= 1.0;
	
	Mat4MulVec4fl(mat, vec4);

	if( vec4[3]>BL_NEAR_CLIP ) {	/* 0.001 is the NEAR clipping cutoff for picking */
		fx= (area->winx/2)*(1 + vec4[0]/vec4[3]);
		
		if( fx>-32700 && fx<32700) {
		
			fy= (area->winy/2)*(1 + vec4[1]/vec4[3]);
			
			if(fy>-32700.0 && fy<32700.0) {
				adr[0]= floor(fx); 
				adr[1]= floor(fy);
			}
		}
	}
}

void view3d_project_float(ScrArea *area, float *vec, float *adr, float mat[4][4])
{
	float vec4[4];
	
	adr[0]= IS_CLIPPED;
	VECCOPY(vec4, vec);
	vec4[3]= 1.0;
	
	Mat4MulVec4fl(mat, vec4);

	if( vec4[3]>FLT_EPSILON ) {
		adr[0] = (area->winx/2.0)+(area->winx/2.0)*vec4[0]/vec4[3];	
		adr[1] = (area->winy/2.0)+(area->winy/2.0)*vec4[1]/vec4[3];
	} else {
		adr[0] = adr[1] = 0.0;
	}
}

int boundbox_clip(float obmat[][4], BoundBox *bb)
{
	/* return 1: draw */
	
	float mat[4][4];
	float vec[4], min, max;
	int a, flag= -1, fl;
	
	if(bb==NULL) return 1;
	if(bb->flag & OB_BB_DISABLED) return 1;
	
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
	
	/* normalize x and y */
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
	
	/* x en y normalizeren */
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


// ndof scaling will be moved to user setting.
// In the mean time this is just a place holder.

// Note: scaling in the plugin and ghostwinlay.c
// should be removed. With driver default setting,
// each axis returns approx. +-200 max deflection.

// The values I selected are based on the older
// polling i/f. With event i/f, the sensistivity
// can be increased for improved response from
// small deflections of the device input.


// lukep notes : i disagree on the range.
// the normal 3Dconnection driver give +/-400
// on defaut range in other applications
// and up to +/- 1000 if set to maximum
// because i remove the scaling by delta,
// which was a bad idea as it depend of the system
// speed and os, i changed the scaling values, but 
// those are still not ok


float ndof_axis_scale[6] = {
	+0.01,	// Tx
	+0.01,	// Tz
	+0.01,	// Ty
	+0.0015,	// Rx
	+0.0015,	// Rz
	+0.0015	// Ry
};

// statics for controlling G.vd->dist corrections.
// viewmoveNDOF zeros and adjusts G.vd->ofs.
// viewmove restores based on dz_flag state.

int dz_flag = 0;
float m_dist;

void viewmoveNDOFfly(int mode)
{
    int i;
    float phi;
    float dval[7];
	// static fval[6] for low pass filter; device input vector is dval[6]
	static float fval[6];
    float tvec[3],rvec[3];
    float q1[4];
	float mat[3][3];
	float upvec[3];


    /*----------------------------------------------------
	 * sometimes this routine is called from headerbuttons
     * viewmove needs to refresh the screen
     */
	areawinset(curarea->win);


	// fetch the current state of the ndof device
	getndof(dval);

	if (G.vd->ndoffilter)
		filterNDOFvalues(fval);

//	for(i=0;i<7;i++) printf("%f ",dval[i]);
//		printf("\n");


	// Scale input values

//	if(dval[6] == 0) return; // guard against divide by zero

	for(i=0;i<6;i++) {

		// user scaling
		dval[i] = dval[i] * ndof_axis_scale[i];

		// non-linear scaling
		if(dval[i]<0.0f)
			dval[i] = -1.0f * dval[i] * dval[i];
		else
			dval[i] = dval[i] * dval[i];
	}


	// low pass filter with zero crossing reset

	for(i=0;i<6;i++) {
		if((dval[i] * fval[i]) >= 0)
			dval[i] = (fval[i] * 15 + dval[i]) / 16;
		else
			fval[i] = 0;
	}


	// force perspective mode. This is a hack and is
	// incomplete. It doesn't actually effect the view
	// until the first draw and doesn't update the menu
	// to reflect persp mode.

	G.vd->persp = V3D_PERSP;


	// Correct the distance jump if G.vd->dist != 0

	// This is due to a side effect of the original
	// mouse view rotation code. The rotation point is
	// set a distance in front of the viewport to
	// make rotating with the mouse look better.
	// The distance effect is written at a low level
	// in the view management instead of the mouse
	// view function. This means that all other view
	// movement devices must subtract this from their
	// view transformations.

	if(G.vd->dist != 0.0) {
		dz_flag = 1;
		m_dist = G.vd->dist;
		upvec[0] = upvec[1] = 0;
		upvec[2] = G.vd->dist;
		Mat3CpyMat4(mat, G.vd->viewinv);
		Mat3MulVecfl(mat, upvec);
		VecSubf(G.vd->ofs, G.vd->ofs, upvec);
		G.vd->dist = 0.0;
	}


	// Apply rotation
	// Rotations feel relatively faster than translations only in fly mode, so
	// we have no choice but to fix that here (not in the plugins)
	rvec[0] = -0.5 * dval[3];
	rvec[1] = -0.5 * dval[4];
	rvec[2] = -0.5 * dval[5];

	// rotate device x and y by view z

	Mat3CpyMat4(mat, G.vd->viewinv);
	mat[2][2] = 0.0f;
	Mat3MulVecfl(mat, rvec);

	// rotate the view

	phi = Normalize(rvec);
	if(phi != 0) {
		VecRotToQuat(rvec,phi,q1);
		QuatMul(G.vd->viewquat, G.vd->viewquat, q1);
	}


	// Apply translation

	tvec[0] = dval[0];
	tvec[1] = dval[1];
	tvec[2] = -dval[2];

	// the next three lines rotate the x and y translation coordinates
	// by the current z axis angle

	Mat3CpyMat4(mat, G.vd->viewinv);
	mat[2][2] = 0.0f;
	Mat3MulVecfl(mat, tvec);

	// translate the view

	VecSubf(G.vd->ofs, G.vd->ofs, tvec);


	/*----------------------------------------------------
     * refresh the screen
     */
    scrarea_do_windraw(curarea);
    screen_swapbuffers();

	// update render preview window

	BIF_view3d_previewrender_signal(curarea, PR_DBASE|PR_DISPRECT);
}

void viewmove(int mode)
{
	static float lastofs[3] = {0,0,0};
	Object *ob = OBACT;
	float firstvec[3], newvec[3], dvec[3];
	float reverse, oldquat[4], q1[4], si, phi, dist0;
	float ofs[3], obofs[3]= {0.0f, 0.0f, 0.0f};
	int firsttime=1;
	short mvalball[2], mval[2], mvalo[2], mval_area[2], mvali[2];
	short use_sel = 0;
	short preview3d_event= 1;
	
	// locals for dist correction
	float mat[3][3];
	float upvec[3];

		/* 3D window may not be defined */
	if( !G.vd ) {
		fprintf( stderr, "G.vd == NULL in viewmove()\n" );
		return;
	}
	
	// dist correction from other movement devices	
	if((dz_flag)||G.vd->dist==0) {
		dz_flag = 0;
		G.vd->dist = m_dist;
		upvec[0] = upvec[1] = 0;
		upvec[2] = G.vd->dist;
		Mat3CpyMat4(mat, G.vd->viewinv);
		Mat3MulVecfl(mat, upvec);
		VecAddf(G.vd->ofs, G.vd->ofs, upvec);
	}


		
	/* sometimes this routine is called from headerbuttons */

	areawinset(curarea->win);
	
	initgrabz(-G.vd->ofs[0], -G.vd->ofs[1], -G.vd->ofs[2]);
	
	QUATCOPY(oldquat, G.vd->viewquat);
	
	getmouseco_areawin(mval_area);	/* for zoom to mouse loc */
	getmouseco_sc(mvali);		/* work with screen coordinates because of trackball function */
	mvalball[0]= mvalo[0] = mvali[0];			/* needed for turntable to work */
	mvalball[1]= mvalo[1] = mvali[1];
	dist0= G.vd->dist;
	
	calctrackballvec(&curarea->winrct, mvalo, firstvec);

	/* cumultime(0); */

	if(!G.obedit && (G.f & G_SCULPTMODE) && ob && G.vd->pivot_last) {
		use_sel= 1;
		VecCopyf(ofs, G.vd->ofs);

		VecCopyf(obofs, sculpt_data()->pivot);
		Mat4MulVecfl(ob->obmat, obofs);
		obofs[0]= -obofs[0];
		obofs[1]= -obofs[1];
		obofs[2]= -obofs[2];
	}
	else if (U.uiflag & USER_ORBIT_SELECTION) {
		use_sel = 1;
		
		VECCOPY(ofs, G.vd->ofs);
		
		/* If there's no selection, lastofs is unmodified and last value since static */
		calculateTransformCenter(V3D_CENTROID, lastofs);
		
		VECCOPY(obofs, lastofs);
		VecMulf(obofs, -1.0f);
	}
	else
		ofs[0] = ofs[1] = ofs[2] = 0.0f;

	reverse= 1.0f;
	if (G.vd->persmat[2][1] < 0.0f)
		reverse= -1.0f;

	while(TRUE) {
		getmouseco_sc(mval);
		
		// if playanim = alt+A, screenhandlers are for animated UI, python, etc
		if(		(mode==2 && U.viewzoom==USER_ZOOM_CONT) || /* continues zoom always update */
				mval[0]!=mvalo[0] || mval[1]!=mvalo[1] || /* mouse moved, so update */
				(G.f & G_PLAYANIM) || do_screenhandlers(G.curscreen)
			) {
			
			if(firsttime) {
				
				firsttime= 0;
				/* are we translating, rotating or zooming? */
				if(mode==0) {
					if(G.vd->view!=0) scrarea_queue_headredraw(curarea);	/*for button */
					G.vd->view= 0;
				}
				if(G.vd->persp==V3D_CAMOB && mode!=1 && G.vd->camera) {
					G.vd->persp= V3D_PERSP;
					scrarea_do_windraw(curarea);
					scrarea_queue_headredraw(curarea);
				}
			}

			if(mode==0) {	/* view rotate */
				if (U.uiflag & USER_AUTOPERSP) G.vd->persp= V3D_PERSP;

				if (U.flag & USER_TRACKBALL) mvalball[0]= mval[0];
				mvalball[1]= mval[1];
				
				calctrackballvec(&curarea->winrct, mvalball, newvec);
				
				VecSubf(dvec, newvec, firstvec);
				
				si= sqrt(dvec[0]*dvec[0]+ dvec[1]*dvec[1]+ dvec[2]*dvec[2]);
				si/= (2.0*TRACKBALLSIZE);
			
				if (U.flag & USER_TRACKBALL) {
					Crossf(q1+1, firstvec, newvec);
	
					Normalize(q1+1);
	
					/* Allow for rotation beyond the interval
					 * [-pi, pi] */
					while (si > 1.0)
						si -= 2.0;
		
					/* This relation is used instead of
					 * phi = asin(si) so that the angle
					 * of rotation is linearly proportional
					 * to the distance that the mouse is
					 * dragged. */
					phi = si * M_PI / 2.0;
		
					si= sin(phi);
					q1[0]= cos(phi);
					q1[1]*= si;
					q1[2]*= si;
					q1[3]*= si;	
					QuatMul(G.vd->viewquat, q1, oldquat);

					if (use_sel) {
						/* compute the post multiplication quat, to rotate the offset correctly */
						QUATCOPY(q1, oldquat);
						QuatConj(q1);
						QuatMul(q1, q1, G.vd->viewquat);

						QuatConj(q1); /* conj == inv for unit quat */
						VECCOPY(G.vd->ofs, ofs);
						VecSubf(G.vd->ofs, G.vd->ofs, obofs);
						QuatMulVecf(q1, G.vd->ofs);
						VecAddf(G.vd->ofs, G.vd->ofs, obofs);
					}
				} else {
					/* New turntable view code by John Aughey */

					float m[3][3];
					float m_inv[3][3];
					float xvec[3] = {1,0,0};
					/* Sensitivity will control how fast the viewport rotates.  0.0035 was
					   obtained experimentally by looking at viewport rotation sensitivities
					   on other modeling programs. */
					/* Perhaps this should be a configurable user parameter. */
					const float sensitivity = 0.0035;

					/* Get the 3x3 matrix and its inverse from the quaternion */
					QuatToMat3(G.vd->viewquat, m);
					Mat3Inv(m_inv,m);

					/* Determine the direction of the x vector (for rotating up and down) */
					/* This can likely be compuated directly from the quaternion. */
					Mat3MulVecfl(m_inv,xvec);

					/* Perform the up/down rotation */
					phi = sensitivity * -(mval[1] - mvalo[1]);
					si = sin(phi);
					q1[0] = cos(phi);
					q1[1] = si * xvec[0];
					q1[2] = si * xvec[1];
					q1[3] = si * xvec[2];
					QuatMul(G.vd->viewquat, G.vd->viewquat, q1);

					if (use_sel) {
						QuatConj(q1); /* conj == inv for unit quat */
						VecSubf(G.vd->ofs, G.vd->ofs, obofs);
						QuatMulVecf(q1, G.vd->ofs);
						VecAddf(G.vd->ofs, G.vd->ofs, obofs);
					}

					/* Perform the orbital rotation */
					phi = sensitivity * reverse * (mval[0] - mvalo[0]);
					q1[0] = cos(phi);
					q1[1] = q1[2] = 0.0;
					q1[3] = sin(phi);
					QuatMul(G.vd->viewquat, G.vd->viewquat, q1);

					if (use_sel) {
						QuatConj(q1);
						VecSubf(G.vd->ofs, G.vd->ofs, obofs);
						QuatMulVecf(q1, G.vd->ofs);
						VecAddf(G.vd->ofs, G.vd->ofs, obofs);
					}
				}
			}
			else if(mode==1) {	/* translate */
				if(G.vd->persp==V3D_CAMOB) {
					float max= (float)MAX2(curarea->winx, curarea->winy);

					G.vd->camdx += (mvalo[0]-mval[0])/(max);
					G.vd->camdy += (mvalo[1]-mval[1])/(max);
					CLAMP(G.vd->camdx, -1.0f, 1.0f);
					CLAMP(G.vd->camdy, -1.0f, 1.0f);
					preview3d_event= 0;
				}
				else {
					window_to_3d(dvec, mval[0]-mvalo[0], mval[1]-mvalo[1]);
					VecAddf(G.vd->ofs, G.vd->ofs, dvec);
				}
			}
			else if(mode==2) {
				float zfac=1.0;

				/* use initial value (do not use mvalo (that is used to detect mouse moviments)) */
				mvalo[0] = mvali[0];
				mvalo[1] = mvali[1];
				
				if(U.viewzoom==USER_ZOOM_CONT) {
					// oldstyle zoom
					zfac = 1.0+(float)(mvalo[0]-mval[0]+mvalo[1]-mval[1])/1000.0;
				}
				else if(U.viewzoom==USER_ZOOM_SCALE) {
					int ctr[2], len1, len2;
					// method which zooms based on how far you move the mouse
					
					ctr[0] = (curarea->winrct.xmax + curarea->winrct.xmin)/2;
					ctr[1] = (curarea->winrct.ymax + curarea->winrct.ymin)/2;
					
					len1 = (int)sqrt((ctr[0] - mval[0])*(ctr[0] - mval[0]) + (ctr[1] - mval[1])*(ctr[1] - mval[1])) + 5;
					len2 = (int)sqrt((ctr[0] - mvalo[0])*(ctr[0] - mvalo[0]) + (ctr[1] - mvalo[1])*(ctr[1] - mvalo[1])) + 5;
					
					zfac = dist0 * ((float)len2/len1) / G.vd->dist;
				}
				else {	/* USER_ZOOM_DOLLY */
					float len1 = (curarea->winrct.ymax - mval[1]) + 5;
					float len2 = (curarea->winrct.ymax - mvalo[1]) + 5;
					zfac = dist0 * (2.0*((len2/len1)-1.0) + 1.0) / G.vd->dist;
				}

				if(zfac != 1.0 && zfac*G.vd->dist > 0.001*G.vd->grid && 
					zfac*G.vd->dist < 10.0*G.vd->far)
					view_zoom_mouseloc(zfac, mval_area);
				
				/* these limits are in toets.c too */
				if(G.vd->dist<0.001*G.vd->grid) G.vd->dist= 0.001*G.vd->grid;
				if(G.vd->dist>10.0*G.vd->far) G.vd->dist=10.0*G.vd->far;
				
				if(G.vd->persp==V3D_ORTHO || G.vd->persp==V3D_CAMOB) preview3d_event= 0;
			}
			
			
			
			mvalo[0]= mval[0];
			mvalo[1]= mval[1];

			if(G.f & G_PLAYANIM) inner_play_anim_loop(0, 0);
			if(G.f & G_SIMULATION) break;

			/* If in retopo paint mode, update lines */
			if(retopo_mesh_paint_check() && G.vd->retopo_view_data) {
				G.vd->retopo_view_data->queue_matrix_update= 1;
				retopo_paint_view_update(G.vd);
			}

			scrarea_do_windraw(curarea);
			screen_swapbuffers();
		}
		else {
			short val;
			unsigned short event;
			/* we need to empty the queue... when you do this very long it overflows */
			while(qtest()) event= extern_qread(&val);
			
			BIF_wait_for_statechange();
		}
		
		/* this in the end, otherwise get_mbut does not work on a PC... */
		if( !(get_mbut() & (L_MOUSE|M_MOUSE))) break;
	}

	if(G.vd->depths) G.vd->depths->damaged= 1;
	retopo_queue_updates(G.vd);
	allqueue(REDRAWVIEW3D, 0);

	if(preview3d_event) 
		BIF_view3d_previewrender_signal(curarea, PR_DBASE|PR_DISPRECT);
	else
		BIF_view3d_previewrender_signal(curarea, PR_PROJECTED);

}
 
void view_zoom_mouseloc(float dfac, short *mouseloc)
{
	if(U.uiflag & USER_ZOOM_TO_MOUSEPOS) {
		short vb[2];
		float dvec[3];
		float tvec[3];
		float tpos[3];
		float new_dist;

		/* find the current window width and height */
		vb[0] = G.vd->area->winx;
		vb[1] = G.vd->area->winy;
		
		tpos[0] = -G.vd->ofs[0];
		tpos[1] = -G.vd->ofs[1];
		tpos[2] = -G.vd->ofs[2];

		/* Project cursor position into 3D space */
		initgrabz(tpos[0], tpos[1], tpos[2]);
		window_to_3d(dvec, mouseloc[0]-vb[0]/2, mouseloc[1]-vb[1]/2);

		/* Calculate view target position for dolly */
		tvec[0] = -(tpos[0] + dvec[0]);
		tvec[1] = -(tpos[1] + dvec[1]);
		tvec[2] = -(tpos[2] + dvec[2]);

		/* Offset to target position and dolly */
		new_dist = G.vd->dist * dfac;
		
		VECCOPY(G.vd->ofs, tvec);
		G.vd->dist = new_dist;
		
		/* Calculate final offset */
		dvec[0] = tvec[0] + dvec[0] * dfac;
		dvec[1] = tvec[1] + dvec[1] * dfac;
		dvec[2] = tvec[2] + dvec[2] * dfac;
		
		VECCOPY(G.vd->ofs, dvec);
	} else {
		G.vd->dist *= dfac;
	}
}

void viewmoveNDOF(int mode)
{
    float fval[7];
    float dvec[3];
    float sbadjust = 1.0f;
    float len;
	short use_sel = 0;
	Object *ob = OBACT;
    float m[3][3];
    float m_inv[3][3];
    float xvec[3] = {1,0,0};
    float yvec[3] = {0,-1,0};
    float zvec[3] = {0,0,1};
	float phi, si;
    float q1[4];
    float obofs[3];
    float reverse;
    float diff[4];
    float d, curareaX, curareaY;
    float mat[3][3];
    float upvec[3];

    /* Sensitivity will control how fast the view rotates.  The value was
     * obtained experimentally by tweaking until the author didn't get dizzy watching.
     * Perhaps this should be a configurable user parameter. 
     */
    float psens = 0.005f * (float) U.ndof_pan;   /* pan sensitivity */
    float rsens = 0.005f * (float) U.ndof_rotate;  /* rotate sensitivity */
    float zsens = 0.3f;   /* zoom sensitivity */

    const float minZoom = -30.0f;
    const float maxZoom = 300.0f;

	//reset view type
	G.vd->view = 0;
//printf("passing here \n");
//
	if (G.obedit==NULL && ob && !(ob->flag & OB_POSEMODE)) {
		use_sel = 1;
	}

    if((dz_flag)||G.vd->dist==0) {
		dz_flag = 0;
		G.vd->dist = m_dist;
		upvec[0] = upvec[1] = 0;
		upvec[2] = G.vd->dist;
		Mat3CpyMat4(mat, G.vd->viewinv);
		Mat3MulVecfl(mat, upvec);
		VecAddf(G.vd->ofs, G.vd->ofs, upvec);
	}

    /*----------------------------------------------------
	 * sometimes this routine is called from headerbuttons
     * viewmove needs to refresh the screen
     */
	areawinset(curarea->win);

    /*----------------------------------------------------
     * record how much time has passed. clamp at 10 Hz
     * pretend the previous frame occured at the clamped time 
     */
//    now = PIL_check_seconds_timer();
 //   frametime = (now - prevTime);
 //   if (frametime > 0.1f){        /* if more than 1/10s */
 //       frametime = 1.0f/60.0;      /* clamp at 1/60s so no jumps when starting to move */
//    }
//    prevTime = now;
 //   sbadjust *= 60 * frametime;             /* normalize ndof device adjustments to 100Hz for framerate independence */

    /* fetch the current state of the ndof device */
    getndof(fval);
 //           printf(" motion command %f %f %f %f %f %f %f \n", fval[0], fval[1], fval[2],
 //           							 fval[3], fval[4], fval[5], fval[6]);
			if (G.vd->ndoffilter)
				filterNDOFvalues(fval);
	
	
    // put scaling back here, was previously in ghostwinlay
    fval[0] = fval[0] * (1.0f/1200.0f);
    fval[1] = fval[1] * (1.0f/1200.0f);
    fval[2] = fval[2] * (1.0f/1200.0f);
    fval[3] = fval[3] * 0.00005f;
    fval[4] =-fval[4] * 0.00005f;
    fval[5] = fval[5] * 0.00005f;
    fval[6] = fval[6] / 1000000.0f;
			
    // scale more if not in perspective mode
    if (G.vd->persp == V3D_ORTHO) {
        fval[0] = fval[0] * 0.05f;
        fval[1] = fval[1] * 0.05f;
        fval[2] = fval[2] * 0.05f;
        fval[3] = fval[3] * 0.9f;
        fval[4] = fval[4] * 0.9f;
        fval[5] = fval[5] * 0.9f;
        zsens *= 8;
    }
			
	
    /* set object offset */
	if (ob) {
		obofs[0] = -ob->obmat[3][0];
		obofs[1] = -ob->obmat[3][1];
		obofs[2] = -ob->obmat[3][2];
	}
	else {
		VECCOPY(obofs, G.vd->ofs);
	}

    /* calc an adjustment based on distance from camera */
    if (ob) {
        VecSubf(diff, obofs, G.vd->ofs);
        d = VecLength(diff);
    }
    else {
        d = 1.0f;
    }
    reverse = (G.vd->persmat[2][1] < 0.0f) ? -1.0f : 1.0f;

    /*----------------------------------------------------
     * ndof device pan 
     */
    psens *= 1.0f + d;
    curareaX = sbadjust * psens * fval[0];
    curareaY = sbadjust * psens * fval[1];
    dvec[0] = curareaX * G.vd->persinv[0][0] + curareaY * G.vd->persinv[1][0];
    dvec[1] = curareaX * G.vd->persinv[0][1] + curareaY * G.vd->persinv[1][1];
    dvec[2] = curareaX * G.vd->persinv[0][2] + curareaY * G.vd->persinv[1][2];
    VecAddf(G.vd->ofs, G.vd->ofs, dvec);

    /*----------------------------------------------------
     * ndof device dolly 
     */
    len = zsens * sbadjust * fval[2];

    if (G.vd->persp==V3D_CAMOB) {
        if(G.vd->persp==V3D_CAMOB) { /* This is stupid, please fix - TODO */
            G.vd->camzoom+= 10.0f * -len;
        }
        if (G.vd->camzoom < minZoom) G.vd->camzoom = minZoom;
        else if (G.vd->camzoom > maxZoom) G.vd->camzoom = maxZoom;
    }
    else if ((G.vd->dist> 0.001*G.vd->grid) && (G.vd->dist<10.0*G.vd->far)) {
        G.vd->dist*=(1.0 + len);
    }


    /*----------------------------------------------------
     * ndof device turntable
     * derived from the turntable code in viewmove
     */

    /* Get the 3x3 matrix and its inverse from the quaternion */
    QuatToMat3(G.vd->viewquat, m);
    Mat3Inv(m_inv,m);

    /* Determine the direction of the x vector (for rotating up and down) */
    /* This can likely be compuated directly from the quaternion. */
    Mat3MulVecfl(m_inv,xvec);
    Mat3MulVecfl(m_inv,yvec);
    Mat3MulVecfl(m_inv,zvec);

    /* Perform the up/down rotation */
    phi = sbadjust * rsens * /*0.5f * */ fval[3]; /* spin vertically half as fast as horizontally */
    si = sin(phi);
    q1[0] = cos(phi);
    q1[1] = si * xvec[0];
    q1[2] = si * xvec[1];
    q1[3] = si * xvec[2];
    QuatMul(G.vd->viewquat, G.vd->viewquat, q1);

    if (use_sel) {
        QuatConj(q1); /* conj == inv for unit quat */
        VecSubf(G.vd->ofs, G.vd->ofs, obofs);
        QuatMulVecf(q1, G.vd->ofs);
        VecAddf(G.vd->ofs, G.vd->ofs, obofs);
    }

    /* Perform the orbital rotation */
    /* Perform the orbital rotation 
       If the seen Up axis is parallel to the zoom axis, rotation should be
       achieved with a pure Roll motion (no Spin) on the device. When you start 
       to tilt, moving from Top to Side view, Spinning will increasingly become 
       more relevant while the Roll component will decrease. When a full 
       Side view is reached, rotations around the world's Up axis are achieved
       with a pure Spin-only motion.  In other words the control of the spinning
       around the world's Up axis should move from the device's Spin axis to the
       device's Roll axis depending on the orientation of the world's Up axis 
       relative to the screen. */
    //phi = sbadjust * rsens * reverse * fval[4];  /* spin the knob, y axis */
    phi = sbadjust * rsens * (yvec[2] * fval[4] + zvec[2] * fval[5]);
    q1[0] = cos(phi);
    q1[1] = q1[2] = 0.0;
    q1[3] = sin(phi);
    QuatMul(G.vd->viewquat, G.vd->viewquat, q1);

    if (use_sel) {
        QuatConj(q1);
        VecSubf(G.vd->ofs, G.vd->ofs, obofs);
        QuatMulVecf(q1, G.vd->ofs);
        VecAddf(G.vd->ofs, G.vd->ofs, obofs);
    }

    /*----------------------------------------------------
     * refresh the screen
     */
    scrarea_do_windraw(curarea);
    screen_swapbuffers();
}


/* Gets the lens and clipping values from a camera of lamp type object */
void object_view_settings(Object *ob, float *lens, float *clipsta, float *clipend)
{	
	if (!ob) return;
	
	if(ob->type==OB_LAMP ) {
		Lamp *la = ob->data;
		if (lens) {
			float x1, fac;
			fac= cos( M_PI*la->spotsize/360.0);
			x1= saacos(fac);
			*lens= 16.0*fac/sin(x1);
		}
		if (clipsta)	*clipsta= la->clipsta;
		if (clipend)	*clipend= la->clipend;
	}
	else if(ob->type==OB_CAMERA) {
		Camera *cam= ob->data;
		if (lens)		*lens= cam->lens;
		if (clipsta)	*clipsta= cam->clipsta;
		if (clipend)	*clipend= cam->clipend;
	}
}


int get_view3d_viewplane(int winxi, int winyi, rctf *viewplane, float *clipsta, float *clipend, float *pixsize)
{
	Camera *cam=NULL;
	float lens, fac, x1, y1, x2, y2;
	float winx= (float)winxi, winy= (float)winyi;
	int orth= 0;
	
	lens= G.vd->lens;	
	
	*clipsta= G.vd->near;
	*clipend= G.vd->far;

/*	
 * Cant use this since we need the fac and x1 values set
 * if(G.vd->persp==V3D_CAMOB)
		object_view_settings(G.vd->camera, &lens, &(*clipsta), &(*clipend));*/
	
	if(G.vd->persp==V3D_CAMOB) {
		if(G.vd->camera) {
			if(G.vd->camera->type==OB_LAMP ) {
				Lamp *la;
				
				la= G.vd->camera->data;
				fac= cos( M_PI*la->spotsize/360.0);
				
				x1= saacos(fac);
				lens= 16.0*fac/sin(x1);
		
				*clipsta= la->clipsta;
				*clipend= la->clipend;
			}
			else if(G.vd->camera->type==OB_CAMERA) {
				cam= G.vd->camera->data;
				lens= cam->lens;
				*clipsta= cam->clipsta;
				*clipend= cam->clipend;
			}
		}
	}
	
	if(G.vd->persp==V3D_ORTHO) {
		if(winx>winy) x1= -G.vd->dist;
		else x1= -winx*G.vd->dist/winy;
		x2= -x1;

		if(winx>winy) y1= -winy*G.vd->dist/winx;
		else y1= -G.vd->dist;
		y2= -y1;
		
		*clipend *= 0.5;	// otherwise too extreme low zbuffer quality
		*clipsta= - *clipend;
		orth= 1;
	}
	else {
		/* fac for zoom, also used for camdx */
		if(G.vd->persp==V3D_CAMOB) {
			fac= (1.41421+( (float)G.vd->camzoom )/50.0);
			fac*= fac;
		}
		else fac= 2.0;
		
		/* viewplane size depends... */
		if(cam && cam->type==CAM_ORTHO) {
			/* ortho_scale == 1 means exact 1 to 1 mapping */
			float dfac= 2.0*cam->ortho_scale/fac;
			
			if(winx>winy) x1= -dfac;
			else x1= -winx*dfac/winy;
			x2= -x1;
			
			if(winx>winy) y1= -winy*dfac/winx;
			else y1= -dfac;
			y2= -y1;
			orth= 1;
		}
		else {
			float dfac;
			
			if(winx>winy) dfac= 64.0/(fac*winx*lens);
			else dfac= 64.0/(fac*winy*lens);
			
			x1= - *clipsta * winx*dfac;
			x2= -x1;
			y1= - *clipsta * winy*dfac;
			y2= -y1;
			orth= 0;
		}
		/* cam view offset */
		if(cam) {
			float dx= 0.5*fac*G.vd->camdx*(x2-x1);
			float dy= 0.5*fac*G.vd->camdy*(y2-y1);
			x1+= dx;
			x2+= dx;
			y1+= dy;
			y2+= dy;
		}
	}

	if(pixsize) {
		float viewfac;

		if(orth) {
			viewfac= (winx >= winy)? winx: winy;
			*pixsize= 1.0f/viewfac;
		}
		else {
			viewfac= (((winx >= winy)? winx: winy)*lens)/32.0;
			*pixsize= *clipsta/viewfac;
		}
	}
	
	viewplane->xmin= x1;
	viewplane->ymin= y1;
	viewplane->xmax= x2;
	viewplane->ymax= y2;
	
	return orth;
}

/* important to not set windows active in here, can be renderwin for example */
void setwinmatrixview3d(int winx, int winy, rctf *rect)		/* rect: for picking */
{
	rctf viewplane;
	float clipsta, clipend, x1, y1, x2, y2;
	int orth;
	
	orth= get_view3d_viewplane(winx, winy, &viewplane, &clipsta, &clipend, NULL);
//	printf("%d %d %f %f %f %f %f %f\n", winx, winy, viewplane.xmin, viewplane.ymin, viewplane.xmax, viewplane.ymax, clipsta, clipend);
	x1= viewplane.xmin;
	y1= viewplane.ymin;
	x2= viewplane.xmax;
	y2= viewplane.ymax;
	
	if(rect) {		/* picking */
		rect->xmin/= (float)curarea->winx;
		rect->xmin= x1+rect->xmin*(x2-x1);
		rect->ymin/= (float)curarea->winy;
		rect->ymin= y1+rect->ymin*(y2-y1);
		rect->xmax/= (float)curarea->winx;
		rect->xmax= x1+rect->xmax*(x2-x1);
		rect->ymax/= (float)curarea->winy;
		rect->ymax= y1+rect->ymax*(y2-y1);
		
		if(orth) myortho(rect->xmin, rect->xmax, rect->ymin, rect->ymax, -clipend, clipend);
		else mywindow(rect->xmin, rect->xmax, rect->ymin, rect->ymax, clipsta, clipend);
		
	}
	else {
		if(orth) myortho(x1, x2, y1, y2, clipsta, clipend);
		else mywindow(x1, x2, y1, y2, clipsta, clipend);
	}

	/* not sure what this was for? (ton) */
	glMatrixMode(GL_PROJECTION);
	mygetmatrix(curarea->winmat);
	glMatrixMode(GL_MODELVIEW);
}

void obmat_to_viewmat(Object *ob, short smooth)
{
	float bmat[4][4];
	float tmat[3][3];

	G.vd->view= 0; /* dont show the grid */

	Mat4CpyMat4(bmat, ob->obmat);
	Mat4Ortho(bmat);
	Mat4Invert(G.vd->viewmat, bmat);
	
	/* view quat calculation, needed for add object */
	Mat3CpyMat4(tmat, G.vd->viewmat);
	if (smooth) {
		float new_quat[4];
		if (G.vd->persp==V3D_CAMOB && G.vd->camera) {
			/* were from a camera view */
			
			float orig_ofs[3];
			float orig_dist= G.vd->dist;
			float orig_lens= G.vd->lens;
			VECCOPY(orig_ofs, G.vd->ofs);
			
			/* Switch from camera view */
			Mat3ToQuat(tmat, new_quat);
			
			G.vd->persp=V3D_PERSP;
			G.vd->dist= 0.0;
			
			view_settings_from_ob(G.vd->camera, G.vd->ofs, NULL, NULL, &G.vd->lens);
			smooth_view(G.vd, orig_ofs, new_quat, &orig_dist, &orig_lens);
			
			G.vd->persp=V3D_CAMOB; /* just to be polite, not needed */
			
		} else {
			Mat3ToQuat(tmat, new_quat);
			smooth_view(G.vd, NULL, new_quat, NULL, NULL);
		}
	} else {
		Mat3ToQuat(tmat, G.vd->viewquat);
	}
}

/* dont set windows active in in here, is used by renderwin too */
void setviewmatrixview3d()
{
	if(G.vd->persp==V3D_CAMOB) {	    /* obs/camera */
		if(G.vd->camera) {
			where_is_object(G.vd->camera);	
			obmat_to_viewmat(G.vd->camera, 0);
		}
		else {
			QuatToMat4(G.vd->viewquat, G.vd->viewmat);
			G.vd->viewmat[3][2]-= G.vd->dist;
		}
	}
	else {
		
		QuatToMat4(G.vd->viewquat, G.vd->viewmat);
		if(G.vd->persp==V3D_PERSP) G.vd->viewmat[3][2]-= G.vd->dist;
		if(G.vd->ob_centre) {
			Object *ob= G.vd->ob_centre;
			float vec[3];
			
			VECCOPY(vec, ob->obmat[3]);
			if(ob->type==OB_ARMATURE && G.vd->ob_centre_bone[0]) {
				bPoseChannel *pchan= get_pose_channel(ob->pose, G.vd->ob_centre_bone);
				if(pchan) {
					VECCOPY(vec, pchan->pose_mat[3]);
					Mat4MulVecfl(ob->obmat, vec);
				}
			}
			i_translate(-vec[0], -vec[1], -vec[2], G.vd->viewmat);
		}
		else i_translate(G.vd->ofs[0], G.vd->ofs[1], G.vd->ofs[2], G.vd->viewmat);
	}
}

void setcameratoview3d(void)
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
	/*  */
	/*if (ob->transflag & OB_QUAT) {
		QUATCOPY(ob->quat, G.vd->viewquat);
	} else {*/
		QuatToEul(G.vd->viewquat, ob->rot);
	/*}*/
	G.vd->viewquat[0]= -G.vd->viewquat[0];
}

/* IGLuint-> GLuint*/
/* Warning: be sure to account for a negative return value
 *   This is an error, "Too many objects in select buffer"
 *   and no action should be taken (can crash blender) if this happens
 */
short  view3d_opengl_select(unsigned int *buffer, unsigned int bufsize, short x1, short y1, short x2, short y2)
{
	rctf rect;
	short mval[2], code, hits;

	G.f |= G_PICKSEL;
	
	if(x1==0 && x2==0 && y1==0 && y2==0) {
		getmouseco_areawin(mval);
		rect.xmin= mval[0]-12;	// seems to be default value for bones only now
		rect.xmax= mval[0]+12;
		rect.ymin= mval[1]-12;
		rect.ymax= mval[1]+12;
	}
	else {
		rect.xmin= x1;
		rect.xmax= x2;
		rect.ymin= y1;
		rect.ymax= y2;
	}
	/* get rid of overlay button matrix */
	persp(PERSP_VIEW);
	setwinmatrixview3d(curarea->winx, curarea->winy, &rect);
	Mat4MulMat4(G.vd->persmat, G.vd->viewmat, curarea->winmat);
	
	if(G.vd->drawtype > OB_WIRE) {
		G.vd->zbuf= TRUE;
		glEnable(GL_DEPTH_TEST);
	}

	if(G.vd->flag & V3D_CLIPPING)
		view3d_set_clipping(G.vd);
	
	glSelectBuffer( bufsize, (GLuint *)buffer);
	glRenderMode(GL_SELECT);
	glInitNames();	/* these two calls whatfor? It doesnt work otherwise */
	glPushName(-1);
	code= 1;
	
	if(G.obedit && G.obedit->type==OB_MBALL) {
		draw_object(BASACT, DRAW_PICKING|DRAW_CONSTCOLOR);
	}
	else if ((G.obedit && G.obedit->type==OB_ARMATURE)) {
		draw_object(BASACT, DRAW_PICKING|DRAW_CONSTCOLOR);
	}
	else {
		Base *base;
		
		G.vd->xray= TRUE;	// otherwise it postpones drawing
		for(base= G.scene->base.first; base; base= base->next) {
			if(base->lay & G.vd->lay) {
				
				if (base->object->restrictflag & OB_RESTRICT_SELECT)
					base->selcol= 0;
				else {
					base->selcol= code;
					glLoadName(code);
					draw_object(base, DRAW_PICKING|DRAW_CONSTCOLOR);
					
					/* we draw group-duplicators for selection too */
					if((base->object->transflag & OB_DUPLI) && base->object->dup_group) {
						ListBase *lb;
						DupliObject *dob;
						Base tbase;
						
						tbase.flag= OB_FROMDUPLI;
						lb= object_duplilist(G.scene, base->object);
						
						for(dob= lb->first; dob; dob= dob->next) {
							tbase.object= dob->ob;
							Mat4CpyMat4(dob->ob->obmat, dob->mat);
							
							draw_object(&tbase, DRAW_PICKING|DRAW_CONSTCOLOR);
							
							Mat4CpyMat4(dob->ob->obmat, dob->omat);
						}
						free_object_duplilist(lb);
					}
					code++;
				}				
			}
		}
		G.vd->xray= FALSE;	// restore
	}
	
	glPopName();	/* see above (pushname) */
	hits= glRenderMode(GL_RENDER);

	G.f &= ~G_PICKSEL;
	setwinmatrixview3d(curarea->winx, curarea->winy, NULL);
	Mat4MulMat4(G.vd->persmat, G.vd->viewmat, curarea->winmat);
	
	if(G.vd->drawtype > OB_WIRE) {
		G.vd->zbuf= 0;
		glDisable(GL_DEPTH_TEST);
	}
	persp(PERSP_WIN);
	
	if(G.vd->flag & V3D_CLIPPING)
		view3d_clr_clipping();
	
	if(hits<0) error("Too many objects in select buffer");
	
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

	INIT_MINMAX(min, max);

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
		size= 0.7*MAX3(afm[0], afm[1], afm[2]);
		if(size<=0.01) size= 0.01;
	}
	
	if(ok) {
		G.vd->localvd= MEM_mallocN(sizeof(View3D), "localview");
		memcpy(G.vd->localvd, G.vd, sizeof(View3D));

		G.vd->ofs[0]= -(min[0]+max[0])/2.0;
		G.vd->ofs[1]= -(min[1]+max[1])/2.0;
		G.vd->ofs[2]= -(min[2]+max[2])/2.0;

		G.vd->dist= size;

		// correction for window aspect ratio
		if(curarea->winy>2 && curarea->winx>2) {
			size= (float)curarea->winx/(float)curarea->winy;
			if(size<1.0) size= 1.0/size;
			G.vd->dist*= size;
		}
		
		if (G.vd->persp==V3D_CAMOB) G.vd->persp= V3D_PERSP;
		if (G.vd->near> 0.1) G.vd->near= 0.1;
		
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
	BIF_view3d_previewrender_signal(curarea, PR_DBASE|PR_DISPRECT);
}

void centerview()	/* like a localview without local! */
{
	Object *ob= OBACT;
	float size, min[3], max[3], afm[3];
	int ok=0;
	
	/* SMOOTHVIEW */
	float new_ofs[3];
	float new_dist;
	
	INIT_MINMAX(min, max);

	if (G.f & G_WEIGHTPAINT) {
		/* hardcoded exception, we look for the one selected armature */
		/* this is weak code this way, we should make a generic active/selection callback interface once... */
		Base *base;
		for(base=FIRSTBASE; base; base= base->next) {
			if(TESTBASELIB(base)) {
				if(base->object->type==OB_ARMATURE)
					if(base->object->flag & OB_POSEMODE)
						break;
			}
		}
		if(base)
			ob= base->object;
	}
	
	
	if(G.obedit) {
		ok = minmax_verts(min, max);	/* only selected */
	}
	else if(ob && (ob->flag & OB_POSEMODE)) {
		if(ob->pose) {
			bArmature *arm= ob->data;
			bPoseChannel *pchan;
			float vec[3];
			
			for(pchan= ob->pose->chanbase.first; pchan; pchan= pchan->next) {
				if(pchan->bone->flag & BONE_SELECTED) {
					if(pchan->bone->layer & arm->layer) {
						ok= 1;
						VECCOPY(vec, pchan->pose_head);
						Mat4MulVecfl(ob->obmat, vec);
						DO_MINMAX(vec, min, max);
						VECCOPY(vec, pchan->pose_tail);
						Mat4MulVecfl(ob->obmat, vec);
						DO_MINMAX(vec, min, max);
					}
				}
			}
		}
	}
	else if (FACESEL_PAINT_TEST) {
		ok= minmax_tface(min, max);
	}
	else if (G.f & G_PARTICLEEDIT) {
		ok= PE_minmax(min, max);
	}
	else {
		Base *base= FIRSTBASE;
		while(base) {
			if TESTBASE(base)  {
				minmax_object(base->object, min, max);
				/* account for duplis */
				minmax_object_duplis(base->object, min, max);
				
				ok= 1;
			}
			base= base->next;
		}
	}
	
	if(ok==0) return;
	
	afm[0]= (max[0]-min[0]);
	afm[1]= (max[1]-min[1]);
	afm[2]= (max[2]-min[2]);
	size= 0.7*MAX3(afm[0], afm[1], afm[2]);
	
	if(size <= G.vd->near*1.5) size= G.vd->near*1.5;
	
	new_ofs[0]= -(min[0]+max[0])/2.0;
	new_ofs[1]= -(min[1]+max[1])/2.0;
	new_ofs[2]= -(min[2]+max[2])/2.0;
	
	new_dist = size;

	/* correction for window aspect ratio */
	if(curarea->winy>2 && curarea->winx>2) {
		size= (float)curarea->winx/(float)curarea->winy;
		if(size<1.0) size= 1.0/size;
		new_dist*= size;
	}
	
	G.vd->cursor[0]= -new_ofs[0];
	G.vd->cursor[1]= -new_ofs[1];
	G.vd->cursor[2]= -new_ofs[2];
	
	if (G.vd->persp==V3D_CAMOB && G.vd->camera) {
		float orig_lens= G.vd->lens;
		
		G.vd->persp=V3D_PERSP;
		G.vd->dist= 0.0;
		view_settings_from_ob(G.vd->camera, G.vd->ofs, NULL, NULL, &G.vd->lens);
		smooth_view(G.vd, new_ofs, NULL, &new_dist, &orig_lens);
	} else {
		if(G.vd->persp==V3D_CAMOB)
			G.vd->persp= V3D_PERSP;
		
		smooth_view(G.vd, new_ofs, NULL, &new_dist, NULL);
	}
	scrarea_queue_winredraw(curarea);
	BIF_view3d_previewrender_signal(curarea, PR_DBASE|PR_DISPRECT);

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
				if(base->object != G.obedit) {
					base->flag |= SELECT;
					base->object->flag |= SELECT;
				}
				base->object->lay= base->lay;
			}
			base= base->next;
		}

		countall();
		allqueue(REDRAWVIEW3D, 0);	/* because of select */
		allqueue(REDRAWOOPS, 0);	/* because of select */
		BIF_view3d_previewrender_signal(curarea, PR_DBASE|PR_DISPRECT);
	} 
}

void view3d_home(int center)
{
	Base *base;
	float size, min[3], max[3], afm[3];
	int ok= 1, onedone=0;

	if(center) {
		min[0]= min[1]= min[2]= 0.0;
		max[0]= max[1]= max[2]= 0.0;
	}
	else {
		INIT_MINMAX(min, max);
	}
	
	for(base= FIRSTBASE; base; base= base->next) {
		if(base->lay & G.vd->lay) {
			onedone= 1;
			minmax_object(base->object, min, max);
		}
	}
	if(!onedone) return;
	
	afm[0]= (max[0]-min[0]);
	afm[1]= (max[1]-min[1]);
	afm[2]= (max[2]-min[2]);
	size= 0.7*MAX3(afm[0], afm[1], afm[2]);
	if(size==0.0) ok= 0;
		
	if(ok) {
		float new_dist;
		float new_ofs[3];
		
		new_dist = size;
		new_ofs[0]= -(min[0]+max[0])/2.0;
		new_ofs[1]= -(min[1]+max[1])/2.0;
		new_ofs[2]= -(min[2]+max[2])/2.0;
		
		// correction for window aspect ratio
		if(curarea->winy>2 && curarea->winx>2) {
			size= (float)curarea->winx/(float)curarea->winy;
			if(size<1.0) size= 1.0/size;
			new_dist*= size;
		}
		
		if (G.vd->persp==V3D_CAMOB && G.vd->camera) {
			/* switch out of camera view */
			float orig_lens= G.vd->lens;
			
			G.vd->persp= V3D_PERSP;
			G.vd->dist= 0.0;
			view_settings_from_ob(G.vd->camera, G.vd->ofs, NULL, NULL, &G.vd->lens);
			smooth_view(G.vd, new_ofs, NULL, &new_dist, &orig_lens);
			
		} else {
			if(G.vd->persp==V3D_CAMOB) G.vd->persp= V3D_PERSP;
			smooth_view(G.vd, new_ofs, NULL, &new_dist, NULL);
		}
		scrarea_queue_winredraw(curarea);
	}
	BIF_view3d_previewrender_signal(curarea, PR_DBASE|PR_DISPRECT);

}


void view3d_align_axis_to_vector(View3D *v3d, int axisidx, float vec[3])
{
	float alignaxis[3] = {0.0, 0.0, 0.0};
	float norm[3], axis[3], angle, new_quat[4];

	if(axisidx > 0) alignaxis[axisidx-1]= 1.0;
	else alignaxis[-axisidx-1]= -1.0;

	VECCOPY(norm, vec);
	Normalize(norm);

	angle= acos(Inpf(alignaxis, norm));
	Crossf(axis, alignaxis, norm);
	VecRotToQuat(axis, -angle, new_quat);

	v3d->view= 0;
	
	if (v3d->persp==V3D_CAMOB && v3d->camera) {
		/* switch out of camera view */
		float orig_ofs[3];
		float orig_dist= v3d->dist;
		float orig_lens= v3d->lens;

		VECCOPY(orig_ofs, v3d->ofs);
		G.vd->persp= V3D_PERSP;
		G.vd->dist= 0.0;
		view_settings_from_ob(v3d->camera, v3d->ofs, NULL, NULL, &v3d->lens);
		smooth_view(G.vd, orig_ofs, new_quat, &orig_dist, &orig_lens);
	} else {
		if (v3d->persp==V3D_CAMOB) v3d->persp= V3D_PERSP; /* switch out of camera mode */
		smooth_view(v3d, NULL, new_quat, NULL, NULL);
	}
}



/* SMOOTHVIEW */
void smooth_view(View3D *v3d, float *ofs, float *quat, float *dist, float *lens)
{
	/* View Animation enabled */
	if (U.smooth_viewtx) {
		int i;
		char changed = 0;
		float step = 0.0, step_inv;
		float orig_dist;
		float orig_lens;
		float orig_quat[4];
		float orig_ofs[3];
		
		double time_allowed, time_current, time_start;
		
		/* if there is no difference, return */
		changed = 0; /* zero means no difference */
		if (dist) {
			if ((*dist) != v3d->dist)
				changed = 1;
		}
		
		if (lens) {
			if ((*lens) != v3d->lens)
				changed = 1;
		}
		
		if (!changed && ofs) {
			if ((ofs[0]!=v3d->ofs[0]) ||
				(ofs[1]!=v3d->ofs[1]) ||
				(ofs[2]!=v3d->ofs[2]) )
				changed = 1;
		}
		
		if (!changed && quat ) {
			if ((quat[0]!=v3d->viewquat[0]) ||
				(quat[1]!=v3d->viewquat[1]) ||
				(quat[2]!=v3d->viewquat[2]) ||
				(quat[3]!=v3d->viewquat[3]) )
				changed = 1;
		}
		
		/* The new view is different from the old one
		 * so animate the view */
		if (changed) {
			
			/* store original values */
			VECCOPY(orig_ofs,	v3d->ofs);
			QUATCOPY(orig_quat,	v3d->viewquat);
			orig_dist =			v3d->dist;
			orig_lens =			v3d->lens;
			
			time_allowed= (float)U.smooth_viewtx / 1000.0;
			time_current = time_start = PIL_check_seconds_timer();
			
			/* if this is view rotation only
			 * we can decrease the time allowed by
			 * the angle between quats 
			 * this means small rotations wont lag */
			 if (quat && !ofs && !dist) {
			 	float vec1[3], vec2[3];
			 	VECCOPY(vec1, quat);
			 	VECCOPY(vec2, v3d->viewquat);
			 	Normalize(vec1);
			 	Normalize(vec2);
			 	/* scale the time allowed by the rotation */
			 	time_allowed *= NormalizedVecAngle2(vec1, vec2)/(M_PI/2); 
			 }
			
			while (time_start + time_allowed > time_current) {
				
				step =  (float)((time_current-time_start) / time_allowed);
				
				/* ease in/out */
				if (step < 0.5)	step = pow(step*2, 2)/2;
				else			step = 1-(pow(2*(1-step) ,2)/2);
				
				step_inv = 1-step;
				
				if (ofs)
					for (i=0; i<3; i++)
						v3d->ofs[i] = ofs[i]*step + orig_ofs[i]*step_inv;
				
				
				if (quat)
					QuatInterpol(v3d->viewquat, orig_quat, quat, step);
					
				if (dist)
					v3d->dist = ((*dist)*step) + (orig_dist*step_inv);
				
				if (lens)
					v3d->lens = ((*lens)*step) + (orig_lens*step_inv);
				
				/*redraw the view*/
				scrarea_do_windraw(curarea);
				screen_swapbuffers();
				
				time_current= PIL_check_seconds_timer();
			}
		}
	}
	
	/* set these values even if animation is enabled because flaot
	 * error will make then not quite accurate */
	if (ofs)
		VECCOPY(v3d->ofs, ofs);
	if (quat)
		QUATCOPY(v3d->viewquat, quat);
	if (dist)
		v3d->dist = *dist;
	if (lens)
		v3d->lens = *lens;
	
}



/* Gets the view trasnformation from a camera
 * currently dosnt take camzoom into account
 * 
 * The dist is not modified for this function, if NULL its assimed zero
 * */
void view_settings_from_ob(Object *ob, float *ofs, float *quat, float *dist, float *lens)
{	
	float bmat[4][4];
	float imat[4][4];
	float tmat[3][3];
	
	if (!ob) return;
	
	/* Offset */
	if (ofs) {
		where_is_object(ob);
		VECCOPY(ofs, ob->obmat[3]);
		VecMulf(ofs, -1.0f); /*flip the vector*/
	}
	
	/* Quat */
	if (quat) {
		Mat4CpyMat4(bmat, ob->obmat);
		Mat4Ortho(bmat);
		Mat4Invert(imat, bmat);
		Mat3CpyMat4(tmat, imat);
		Mat3ToQuat(tmat, quat);
	}
	
	if (dist) {
		float vec[3];
		Mat3CpyMat4(tmat, ob->obmat);
		
		vec[0]= vec[1] = 0.0;
		vec[2]= -(*dist);
		Mat3MulVecfl(tmat, vec);
		VecSubf(ofs, ofs, vec);
	}
	
	/* Lens */
	if (lens)
		object_view_settings(ob, lens, NULL, NULL);
}

/* For use with smooth view
 * 
 * the current view is unchanged, blend between the current view and the
 * camera view
 * */
void smooth_view_to_camera(View3D *v3d)
{
	if (!U.smooth_viewtx || !v3d->camera || G.vd->persp != V3D_CAMOB) {
		return;
	} else {
		Object *ob = v3d->camera;
		
		float orig_ofs[3];
		float orig_dist=v3d->dist;
		float orig_lens=v3d->lens;
		float new_dist=0.0;
		float new_lens=35.0;
		float new_quat[4];
		float new_ofs[3];
		
		VECCOPY(orig_ofs, v3d->ofs);
		
		view_settings_from_ob(ob, new_ofs, new_quat, NULL, &new_lens);
		
		G.vd->persp= V3D_PERSP;
		smooth_view(v3d, new_ofs, new_quat, &new_dist, &new_lens);
		VECCOPY(v3d->ofs, orig_ofs);
		v3d->lens= orig_lens;
		v3d->dist = orig_dist; /* restore the dist */
		
		v3d->camera = ob;
		v3d->persp= V3D_CAMOB;
	}
}
