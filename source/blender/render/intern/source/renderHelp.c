/**  
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
 * Some helpful conversions/functions.
 *
 * $Id$
 */

#include <math.h>
#include <limits.h>
#include <stdlib.h>

#include "DNA_mesh_types.h"
#include "BLI_arithb.h"

#include "render.h" 
#include "render_intern.h"
#include "renderHelp.h"
#include "zbuf.h"

#ifdef HAVE_CONFIG_H
#include <config.h>
#endif

static float panovco, panovsi;
static float panophi=0.0;
static float tempPanoPhi;

static int panotestclip(float *v);

void pushTempPanoPhi(float p) {
	tempPanoPhi = panophi;
	panophi = p;
}

void popTempPanoPhi() {
	panophi = tempPanoPhi;
}

float getPanoPhi(){
	return panophi;
}
float getPanovCo(){
	return panovco;
}
float getPanovSi(){
	return panovsi;
}

void setPanoRot(int part)
{
/*  	extern float panovco, panovsi; */
	static float alpha= 1.0;

	/* part==0 init all */

	if(part==0) {

		alpha= ((float)R.r.xsch)/R.viewfac;
		alpha= 2.0*atan(alpha/2.0);
	}


	/* rotate it all around the y-as with phi degrees */

	panophi= -0.5*(R.r.xparts-1)*alpha + part*alpha;

	panovsi= sin(-panophi);
	panovco= cos(-panophi);

}




static int panotestclip(float *v)
{
	/* to be used for halos en infos */
	float abs4;
	short c=0;

	if((R.r.mode & R_PANORAMA)==0) return RE_testclip(v);

	abs4= fabs(v[3]);

	if(v[2]< -abs4) c=16;		/* this used to be " if(v[2]<0) ", see clippz() */
	else if(v[2]> abs4) c+= 32;

	if( v[1]>abs4) c+=4;
	else if( v[1]< -abs4) c+=8;

	abs4*= R.r.xparts;
	if( v[0]>abs4) c+=2;
	else if( v[0]< -abs4) c+=1;

	return c;
}

/*

  This adds the hcs coordinates to vertices. It iterates over all
  vertices, halos and faces. After the conversion, we clip in hcs.

  Elsewhere, all primites are converted to vertices. 
  Called in 
  - envmapping (envmap.c)
  - shadow buffering (shadbuf.c)
  - preparation for rendering (renderPreAndPost.c) 

*/

/* move to renderer */
void setzbufvlaggen( void (*projectfunc)(float *, float *) )
/* homocos too */
{
	VlakRen *vlr = NULL;
	VertRen *ver = NULL;
	HaloRen *har = NULL;
	float zn, vec[3], si, co, hoco[4];
	int a;
   	float panophi = 0.0;
	
	panophi = getPanoPhi();
	si= sin(panophi);
	co= cos(panophi);

   /* calculate view coordinates (and zbuffer value) */
	for(a=0; a< R.totvert;a++) {
		if((a & 255)==0) ver= R.blove[a>>8];
		else ver++;

		if(R.r.mode & R_PANORAMA) {
			vec[0]= co*ver->co[0] + si*ver->co[2];
			vec[1]= ver->co[1];
			vec[2]= -si*ver->co[0] + co*ver->co[2];
		}
		else {
			VECCOPY(vec, ver->co);
		}
		/* Go from wcs to hcs ... */
		projectfunc(vec, ver->ho);
		/* ... and clip in that system. */
		ver->clip = RE_testclip(ver->ho);
		/* 
		   Because all other ops are performed in other systems, this is 
		   the only thing that has to be done.
		*/
	}

   /* calculate view coordinates (and zbuffer value) */
	for(a=0; a<R.tothalo; a++) {
		if((a & 255)==0) har= R.bloha[a>>8];
		else har++;

		if(R.r.mode & R_PANORAMA) {
			vec[0]= co*har->co[0] + si*har->co[2];
			vec[1]= har->co[1];
			vec[2]= -si*har->co[0] + co*har->co[2];
		}
		else {
			VECCOPY(vec, har->co);
		}

		projectfunc(vec, hoco);

		hoco[3]*= 2.0;

		if( panotestclip(hoco) ) {
			har->miny= har->maxy= -10000;	/* that way render clips it */
		}
		else if(hoco[3]<0.0) {
			har->miny= har->maxy= -10000;	/* render clips it */
		}
		else /* do the projection...*/
		{
			zn= hoco[3]/2.0;
			har->xs= 0.5*R.rectx*(1.0+hoco[0]/zn); /* the 0.5 negates the previous 2...*/
			har->ys= 0.5*R.recty*(1.0+hoco[1]/zn);
		
			/* this should be the zbuffer coordinate */
			har->zs= 0x7FFFFF*(1.0+hoco[2]/zn);
			/* taking this from the face clip functions? seems ok... */
			har->zBufDist = 0x7FFFFFFF*(hoco[2]/zn);
			vec[0]+= har->hasize;
			projectfunc(vec, hoco);
			vec[0]-= har->hasize;
			zn= hoco[3];
			har->rad= fabs(har->xs- 0.5*R.rectx*(1.0+hoco[0]/zn));
		
			/* this clip is not really OK, to prevent stars to become too large */
			if(har->type & HA_ONLYSKY) {
				if(har->rad>3.0) har->rad= 3.0;
			}
		
			har->radsq= har->rad*har->rad;
		
			har->miny= har->ys - har->rad/R.ycor;
			har->maxy= har->ys + har->rad/R.ycor;
		
			/* the Zd value is still not really correct for pano */
		
			vec[2]-= har->hasize;	/* z negative, otherwise it's clipped */
			projectfunc(vec, hoco);
			zn= hoco[3];
			zn= fabs(har->zs - 0x7FFFFF*(1.0+hoco[2]/zn));
			har->zd= CLAMPIS(zn, 0, INT_MAX);
		
			/* if( har->zs < 2*har->zd) { */
			/* PRINT2(d, d, har->zs, har->zd); */
			/* har->alfa= har->mat->alpha * ((float)(har->zs))/(float)(2*har->zd); */
			/* } */
		
		}
		
	}

	/* set flags at 0 if clipped away */
	for(a=0; a<R.totvlak; a++) {
		if((a & 255)==0) vlr= R.blovl[a>>8];
		else vlr++;

			vlr->flag |= R_VISIBLE;
			if(vlr->v4) {
				if(vlr->v1->clip & vlr->v2->clip & vlr->v3->clip & vlr->v4->clip) vlr->flag &= ~R_VISIBLE;
			}
			else if(vlr->v1->clip & vlr->v2->clip & vlr->v3->clip) vlr->flag &= ~R_VISIBLE;

		}

}

/* ------------------------------------------------------------------------- */
/* move to renderer */

void set_normalflags(void)
{
	VlakRen *vlr = NULL;
	float vec[3], xn, yn, zn;
	int a1;
	
	/* switch normal 'snproj' values (define which axis is the optimal one for calculations) */
	for(a1=0; a1<R.totvlak; a1++) {
		if((a1 & 255)==0) vlr= R.blovl[a1>>8];
		else vlr++;
		
		vec[0]= vlr->v1->co[0];
		vec[1]= vlr->v1->co[1];
		vec[2]= vlr->v1->co[2];

		if( (vec[0]*vlr->n[0] +vec[1]*vlr->n[1] +vec[2]*vlr->n[2])<0.0 ) {
			vlr->puno= vlr->puno ^ 15;
			vlr->n[0]= -vlr->n[0];
			vlr->n[1]= -vlr->n[1];
			vlr->n[2]= -vlr->n[2];
		}

		/* recalculate puno. Displace & flipped matrices can screw up */
		vlr->puno= 0;
		if( Inpf(vlr->n, vlr->v1->n) < 0.0 ) vlr->puno |= ME_FLIPV1;
		if( Inpf(vlr->n, vlr->v2->n) < 0.0 ) vlr->puno |= ME_FLIPV2;
		if( Inpf(vlr->n, vlr->v3->n) < 0.0 ) vlr->puno |= ME_FLIPV3;
		if(vlr->v4 && Inpf(vlr->n, vlr->v4->n) < 0.0 ) vlr->puno |= ME_FLIPV4;
				
		xn= fabs(vlr->n[0]);
		yn= fabs(vlr->n[1]);
		zn= fabs(vlr->n[2]);
		if(zn>=xn && zn>=yn) vlr->snproj= 0;
		else if(yn>=xn && yn>=zn) vlr->snproj= 1;
		else vlr->snproj= 2;

	}
}
