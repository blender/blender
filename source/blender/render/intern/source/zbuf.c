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
 * Contributor(s): Hos, RPW.
 *
 * ***** END GPL/BL DUAL LICENSE BLOCK *****
 */

/*---------------------------------------------------------------------------*/
/* Common includes                                                           */
/*---------------------------------------------------------------------------*/

#include <math.h>
#include <stdlib.h>
#include <limits.h>
#include <string.h>
#include "MTC_matrixops.h"
#include "MEM_guardedalloc.h"

#include "BKE_global.h"

#include "DNA_lamp_types.h"
#include "DNA_mesh_types.h"

#include "radio_types.h"
#include "radio.h"  /* needs RG, some root data for radiosity                */

#include "render.h"
#include "render_intern.h"
#include "RE_callbacks.h"
#include "old_zbuffer_types.h"
/* local includes */
/* can be removed when the old renderer disappears */
#include "rendercore.h"  /* shade_pixel and count_mask */
#include "pixelblending.h"
#include "jitter.h"

/* own includes */
#include "zbuf.h"
#include "zbuf_int.h"

#ifdef HAVE_CONFIG_H
#include <config.h>
#endif

/* crud */
#define MIN2(x,y)               ( (x)<(y) ? (x) : (y) )  
/*-----------------------------------------------------------*/ 
/* Globals for this file                                     */
/*-----------------------------------------------------------*/ 

extern float centLut[16];
extern char *centmask;

float *vlzp[32][3], labda[3][2], vez[400], *p[40];

float Zmulx; /* Half the screenwidth, in pixels. (used in render.c, */
             /* zbuf.c)                                             */
float Zmuly; /* Half the screenheight, in pixels. (used in render.c,*/
             /* zbuf.c)                                             */
float Zjitx; /* Jitter offset in x. When jitter is disabled, this   */
             /* should be 0.5. (used in render.c, zbuf.c)           */
float Zjity; /* Jitter offset in y. When jitter is disabled, this   */
             /* should be 0.5. (used in render.c, zbuf.c)           */

unsigned int Zvlnr, Zsample;
VlakRen *Zvlr;
void (*zbuffunc)(float *, float *, float *);
void (*zbuflinefunc)(float *, float *);

APixstr       *APixbuf;      /* Zbuffer: linked list of face indices       */
unsigned short        *Acolrow;      /* Zbuffer: colour buffer, one line high      */
int           *Arectz;       /* Zbuffer: distance buffer, almost obsolete  */
int            Aminy;        /* y value of first line in the accu buffer   */
int            Amaxy;        /* y value of last line in the accu buffer    */
int            Azvoordeel  = 0;
APixstrMain    apsmfirst;
short          apsmteller  = 0;

/*-----------------------------------------------------------*/ 
/* Functions                                                 */
/*-----------------------------------------------------------*/ 

void fillrect(unsigned int *rect, int x, unsigned int y, unsigned int val)
{
	unsigned int len,*drect;

	len= x*y;
	drect= rect;
	while(len>0) {
		len--;
		*drect= val;
		drect++;
	}
}

/* *************  ACCUMULATION ZBUF ************ */

/*-APixstr---------------------(antialised pixel struct)------------------------------*/ 

APixstr *addpsmainA()
{
	APixstrMain *psm;

	psm= &apsmfirst;

	while(psm->next) {
		psm= psm->next;
	}

	psm->next= MEM_mallocN(sizeof(APixstrMain), "addpsmainA");

	psm= psm->next;
	psm->next=0;
	psm->ps= MEM_callocN(4096*sizeof(APixstr),"pixstr");
	apsmteller= 0;

	return psm->ps;
}

void freepsA()
{
	APixstrMain *psm, *next;

	psm= &apsmfirst;

	while(psm) {
		next= psm->next;
		if(psm->ps) {
			MEM_freeN(psm->ps);
			psm->ps= 0;
		}
		if(psm!= &apsmfirst) MEM_freeN(psm);
		psm= next;
	}

	apsmfirst.next= 0;
	apsmfirst.ps= 0;
	apsmteller= 0;
}

APixstr *addpsA(void)
{
	static APixstr *prev;

	/* make first PS */
	if((apsmteller & 4095)==0) prev= addpsmainA();
	else prev++;
	apsmteller++;
	
	return prev;
}

/* fills in color, with window coordinate, from Aminy->Amaxy */
void zbufinvulAc(float *v1, float *v2, float *v3)  
{
	APixstr *ap, *apofs, *apn;
	double x0,y0,z0,x1,y1,z1,x2,y2,z2,xx1;
	double zxd,zyd,zy0, tmp;
	float *minv,*maxv,*midv;
	int *rz,zverg,x;
	int my0,my2,sn1,sn2,rectx,zd,*rectzofs;
	int y,omsl,xs0,xs1,xs2,xs3, dx0,dx1,dx2, mask;
	
	/* MIN MAX */
	if(v1[1]<v2[1]) {
		if(v2[1]<v3[1]) 	{
			minv=v1; midv=v2; maxv=v3;
		}
		else if(v1[1]<v3[1]) 	{
			minv=v1; midv=v3; maxv=v2;
		}
		else	{
			minv=v3; midv=v1; maxv=v2;
		}
	}
	else {
		if(v1[1]<v3[1]) 	{
			minv=v2; midv=v1; maxv=v3;
		}
		else if(v2[1]<v3[1]) 	{
			minv=v2; midv=v3; maxv=v1;
		}
		else	{
			minv=v3; midv=v2; maxv=v1;
		}
	}

	if(minv[1] == maxv[1]) return;	/* prevent 'zero' size faces */

	my0= ceil(minv[1]);
	my2= floor(maxv[1]);
	omsl= floor(midv[1]);

	if(my2<Aminy || my0> Amaxy) return;

	if(my0<Aminy) my0= Aminy;

	/* EDGES : LONGEST */
	xx1= maxv[1]-minv[1];
	if(xx1>2.0/65536.0) {
		z0= (maxv[0]-minv[0])/xx1;
		
		tmp= (-65536.0*z0);
		dx0= CLAMPIS(tmp, INT_MIN, INT_MAX);
		
		tmp= 65536.0*(z0*(my2-minv[1])+minv[0]);
		xs0= CLAMPIS(tmp, INT_MIN, INT_MAX);
	}
	else {
		dx0= 0;
		xs0= 65536.0*(MIN2(minv[0],maxv[0]));
	}
	/* EDGES : THE TOP ONE */
	xx1= maxv[1]-midv[1];
	if(xx1>2.0/65536.0) {
		z0= (maxv[0]-midv[0])/xx1;
		
		tmp= (-65536.0*z0);
		dx1= CLAMPIS(tmp, INT_MIN, INT_MAX);
		
		tmp= 65536.0*(z0*(my2-midv[1])+midv[0]);
		xs1= CLAMPIS(tmp, INT_MIN, INT_MAX);
	}
	else {
		dx1= 0;
		xs1= 65536.0*(MIN2(midv[0],maxv[0]));
	}
	/* EDGES : BOTTOM ONE */
	xx1= midv[1]-minv[1];
	if(xx1>2.0/65536.0) {
		z0= (midv[0]-minv[0])/xx1;
		
		tmp= (-65536.0*z0);
		dx2= CLAMPIS(tmp, INT_MIN, INT_MAX);
		
		tmp= 65536.0*(z0*(omsl-minv[1])+minv[0]);
		xs2= CLAMPIS(tmp, INT_MIN, INT_MAX);
	}
	else {
		dx2= 0;
		xs2= 65536.0*(MIN2(minv[0],midv[0]));
	}

	/* ZBUF DX DY */
	x1= v1[0]- v2[0];
	x2= v2[0]- v3[0];
	y1= v1[1]- v2[1];
	y2= v2[1]- v3[1];
	z1= v1[2]- v2[2];
	z2= v2[2]- v3[2];
	x0= y1*z2-z1*y2;
	y0= z1*x2-x1*z2;
	z0= x1*y2-y1*x2;
	if(z0==0.0) return;

	if(midv[1]==maxv[1]) omsl= my2;
	if(omsl<Aminy) omsl= Aminy-1;  /* to make sure it does the first loop completely */

	while (my2 > Amaxy) {  /* my2 can be larger */
		xs0+=dx0;
		if (my2<=omsl) {
			xs2+= dx2;
		}
		else{
			xs1+= dx1;
		}
		my2--;
	}

	xx1= (x0*v1[0]+y0*v1[1])/z0+v1[2];

	zxd= -x0/z0;
	zyd= -y0/z0;
	zy0= my2*zyd+xx1;
	zd= (int)CLAMPIS(zxd, INT_MIN, INT_MAX);

	/* start-offset in rect */
	rectx= R.rectx;
	rectzofs= (int *)(Arectz+rectx*(my2-Aminy));
	apofs= (APixbuf+ rectx*(my2-Aminy));
	mask= 1<<Zsample;

	xs3= 0;		/* flag */
	if(dx0>dx1) {
		xs3= xs0;
		xs0= xs1;
		xs1= xs3;
		xs3= dx0;
		dx0= dx1;
		dx1= xs3;
		xs3= 1;	/* flag */

	}

	for(y=my2;y>omsl;y--) {

		sn1= xs0>>16;
		xs0+= dx0;

		sn2= xs1>>16;
		xs1+= dx1;

		sn1++;

		if(sn2>=rectx) sn2= rectx-1;
		if(sn1<0) sn1= 0;
		zverg= (int) CLAMPIS((sn1*zxd+zy0), INT_MIN, INT_MAX);
		rz= rectzofs+sn1;
		ap= apofs+sn1;
		x= sn2-sn1;
		
		zverg-= Azvoordeel;
		
		while(x>=0) {
			if(zverg< *rz) {
				apn= ap;
				while(apn) {	/* loop unrolled */
					if(apn->p[0]==0) {apn->p[0]= Zvlnr; apn->z[0]= zverg; apn->mask[0]= mask; break; }
					if(apn->p[0]==Zvlnr) {apn->mask[0]|= mask; break; }
					if(apn->p[1]==0) {apn->p[1]= Zvlnr; apn->z[1]= zverg; apn->mask[1]= mask; break; }
					if(apn->p[1]==Zvlnr) {apn->mask[1]|= mask; break; }
					if(apn->p[2]==0) {apn->p[2]= Zvlnr; apn->z[2]= zverg; apn->mask[2]= mask; break; }
					if(apn->p[2]==Zvlnr) {apn->mask[2]|= mask; break; }
					if(apn->p[3]==0) {apn->p[3]= Zvlnr; apn->z[3]= zverg; apn->mask[3]= mask; break; }
					if(apn->p[3]==Zvlnr) {apn->mask[3]|= mask; break; }
					if(apn->next==0) apn->next= addpsA();
					apn= apn->next;
				}				
			}
			zverg+= zd;
			rz++; 
			ap++; 
			x--;
		}
		zy0-= zyd;
		rectzofs-= rectx;
		apofs-= rectx;
	}

	if(xs3) {
		xs0= xs1;
		dx0= dx1;
	}
	if(xs0>xs2) {
		xs3= xs0;
		xs0= xs2;
		xs2= xs3;
		xs3= dx0;
		dx0= dx2;
		dx2= xs3;
	}

	for(; y>=my0; y--) {

		sn1= xs0>>16;
		xs0+= dx0;

		sn2= xs2>>16;
		xs2+= dx2;

		sn1++;

		if(sn2>=rectx) sn2= rectx-1;
		if(sn1<0) sn1= 0;
		zverg= (int) CLAMPIS((sn1*zxd+zy0), INT_MIN, INT_MAX);
		rz= rectzofs+sn1;
		ap= apofs+sn1;
		x= sn2-sn1;

		zverg-= Azvoordeel;

		while(x>=0) {
			if(zverg< *rz) {
				apn= ap;
				while(apn) {	/* loop unrolled */
					if(apn->p[0]==0) {apn->p[0]= Zvlnr; apn->z[0]= zverg; apn->mask[0]= mask; break; }
					if(apn->p[0]==Zvlnr) {apn->mask[0]|= mask; break; }
					if(apn->p[1]==0) {apn->p[1]= Zvlnr; apn->z[1]= zverg; apn->mask[1]= mask; break; }
					if(apn->p[1]==Zvlnr) {apn->mask[1]|= mask; break; }
					if(apn->p[2]==0) {apn->p[2]= Zvlnr; apn->z[2]= zverg; apn->mask[2]= mask; break; }
					if(apn->p[2]==Zvlnr) {apn->mask[2]|= mask; break; }
					if(apn->p[3]==0) {apn->p[3]= Zvlnr; apn->z[3]= zverg; apn->mask[3]= mask; break; }
					if(apn->p[3]==Zvlnr) {apn->mask[3]|= mask; break; }
					if(apn->next==0) apn->next= addpsA();
					apn= apn->next;
				}
			}
			zverg+= zd;
			rz++; 
			ap++; 
			x--;
		}

		zy0-=zyd;
		rectzofs-= rectx;
		apofs-= rectx;
	}
}

void zbuflineAc(float *vec1, float *vec2)
{
	APixstr *ap, *apn;
	unsigned int *rectz;
	int start, end, x, y, oldx, oldy, ofs;
	int dz, vergz, mask;
	float dx, dy;
	float v1[3], v2[3];
	
	dx= vec2[0]-vec1[0];
	dy= vec2[1]-vec1[1];
	
	if(fabs(dx) > fabs(dy)) {

		/* all lines from left to right */
		if(vec1[0]<vec2[0]) {
			VECCOPY(v1, vec1);
			VECCOPY(v2, vec2);
		}
		else {
			VECCOPY(v2, vec1);
			VECCOPY(v1, vec2);
			dx= -dx; dy= -dy;
		}

		start= floor(v1[0]);
		end= start+floor(dx);
		if(end>=R.rectx) end= R.rectx-1;
		
		oldy= floor(v1[1]);
		dy/= dx;
		
		vergz= v1[2];
		vergz-= Azvoordeel;
		dz= (v2[2]-v1[2])/dx;
		
		rectz= (unsigned int *)(Arectz+R.rectx*(oldy-Aminy) +start);
		ap= (APixbuf+ R.rectx*(oldy-Aminy) +start);
		mask= 1<<Zsample;	
		
		if(dy<0) ofs= -R.rectx;
		else ofs= R.rectx;
		
		for(x= start; x<=end; x++, rectz++, ap++) {
			
			y= floor(v1[1]);
			if(y!=oldy) {
				oldy= y;
				rectz+= ofs;
				ap+= ofs;
			}
			
			if(x>=0 && y>=Aminy && y<=Amaxy) {
				if(vergz<*rectz) {
				
					apn= ap;
					while(apn) {	/* loop unrolled */
						if(apn->p[0]==0) {apn->p[0]= Zvlnr; apn->z[0]= vergz; apn->mask[0]= mask; break; }
						if(apn->p[0]==Zvlnr) {apn->mask[0]|= mask; break; }
						if(apn->p[1]==0) {apn->p[1]= Zvlnr; apn->z[1]= vergz; apn->mask[1]= mask; break; }
						if(apn->p[1]==Zvlnr) {apn->mask[1]|= mask; break; }
						if(apn->p[2]==0) {apn->p[2]= Zvlnr; apn->z[2]= vergz; apn->mask[2]= mask; break; }
						if(apn->p[2]==Zvlnr) {apn->mask[2]|= mask; break; }
						if(apn->p[3]==0) {apn->p[3]= Zvlnr; apn->z[3]= vergz; apn->mask[3]= mask; break; }
						if(apn->p[3]==Zvlnr) {apn->mask[3]|= mask; break; }
						if(apn->next==0) apn->next= addpsA();
						apn= apn->next;
					}				
					
				}
			}
			
			v1[1]+= dy;
			vergz+= dz;
		}
	}
	else {
	
		/* all lines from top to bottom */
		if(vec1[1]<vec2[1]) {
			VECCOPY(v1, vec1);
			VECCOPY(v2, vec2);
		}
		else {
			VECCOPY(v2, vec1);
			VECCOPY(v1, vec2);
			dx= -dx; dy= -dy;
		}

		start= floor(v1[1]);
		end= start+floor(dy);
		
		if(start>Amaxy || end<Aminy) return;
		
		if(end>Amaxy) end= Amaxy;
		
		oldx= floor(v1[0]);
		dx/= dy;
		
		vergz= v1[2];
		vergz-= Azvoordeel;
		dz= (v2[2]-v1[2])/dy;

		rectz= (unsigned int *)( Arectz+ (start-Aminy)*R.rectx+ oldx );
		ap= (APixbuf+ R.rectx*(start-Aminy) +oldx);
		mask= 1<<Zsample;
				
		if(dx<0) ofs= -1;
		else ofs= 1;

		for(y= start; y<=end; y++, rectz+=R.rectx, ap+=R.rectx) {
			
			x= floor(v1[0]);
			if(x!=oldx) {
				oldx= x;
				rectz+= ofs;
				ap+= ofs;
			}
			
			if(x>=0 && y>=Aminy && x<R.rectx) {
				if(vergz<*rectz) {
					
					apn= ap;
					while(apn) {	/* loop unrolled */
						if(apn->p[0]==0) {apn->p[0]= Zvlnr; apn->z[0]= vergz; apn->mask[0]= mask; break; }
						if(apn->p[0]==Zvlnr) {apn->mask[0]|= mask; break; }
						if(apn->p[1]==0) {apn->p[1]= Zvlnr; apn->z[1]= vergz; apn->mask[1]= mask; break; }
						if(apn->p[1]==Zvlnr) {apn->mask[1]|= mask; break; }
						if(apn->p[2]==0) {apn->p[2]= Zvlnr; apn->z[2]= vergz; apn->mask[2]= mask; break; }
						if(apn->p[2]==Zvlnr) {apn->mask[2]|= mask; break; }
						if(apn->p[3]==0) {apn->p[3]= Zvlnr; apn->z[3]= vergz; apn->mask[3]= mask; break; }
						if(apn->p[3]==Zvlnr) {apn->mask[3]|= mask; break; }
						if(apn->next==0) apn->next= addpsA();
						apn= apn->next;
					}	
					
				}
			}
			
			v1[0]+= dx;
			vergz+= dz;
		}
	}
}



/* *************  NORMAL ZBUFFER ************ */

void hoco_to_zco(float *zco, float *hoco)
{
	float deler;

	deler= hoco[3];
	zco[0]= Zmulx*(1.0+hoco[0]/deler)+ Zjitx;
	zco[1]= Zmuly*(1.0+hoco[1]/deler)+ Zjity;
	zco[2]= 0x7FFFFFFF *(hoco[2]/deler);
}

void zbufline(vec1, vec2)
float *vec1, *vec2;
{
	unsigned int *rectz, *rectp;
	int start, end, x, y, oldx, oldy, ofs;
	int dz, vergz;
	float dx, dy;
	float v1[3], v2[3];
	
	dx= vec2[0]-vec1[0];
	dy= vec2[1]-vec1[1];
	
	if(fabs(dx) > fabs(dy)) {

		/* all lines from left to right */
		if(vec1[0]<vec2[0]) {
			VECCOPY(v1, vec1);
			VECCOPY(v2, vec2);
		}
		else {
			VECCOPY(v2, vec1);
			VECCOPY(v1, vec2);
			dx= -dx; dy= -dy;
		}

		start= floor(v1[0]);
		end= start+floor(dx);
		if(end>=R.rectx) end= R.rectx-1;
		
		oldy= floor(v1[1]);
		dy/= dx;
		
		vergz= v1[2];
		dz= (v2[2]-v1[2])/dx;
		
		rectz= R.rectz+ oldy*R.rectx+ start;
		rectp= R.rectot+ oldy*R.rectx+ start;
		
		if(dy<0) ofs= -R.rectx;
		else ofs= R.rectx;
		
		for(x= start; x<=end; x++, rectz++, rectp++) {
			
			y= floor(v1[1]);
			if(y!=oldy) {
				oldy= y;
				rectz+= ofs;
				rectp+= ofs;
			}
			
			if(x>=0 && y>=0 && y<R.recty) {
				if(vergz<*rectz) {
					*rectz= vergz;
					*rectp= Zvlnr;
				}
			}
			
			v1[1]+= dy;
			vergz+= dz;
		}
	}
	else {
		/* all lines from top to bottom */
		if(vec1[1]<vec2[1]) {
			VECCOPY(v1, vec1);
			VECCOPY(v2, vec2);
		}
		else {
			VECCOPY(v2, vec1);
			VECCOPY(v1, vec2);
			dx= -dx; dy= -dy;
		}

		start= floor(v1[1]);
		end= start+floor(dy);
		
		if(end>=R.recty) end= R.recty-1;
		
		oldx= floor(v1[0]);
		dx/= dy;
		
		vergz= v1[2];
		dz= (v2[2]-v1[2])/dy;

		rectz= R.rectz+ start*R.rectx+ oldx;
		rectp= R.rectot+ start*R.rectx+ oldx;
		
		if(dx<0) ofs= -1;
		else ofs= 1;

		for(y= start; y<=end; y++, rectz+=R.rectx, rectp+=R.rectx) {
			
			x= floor(v1[0]);
			if(x!=oldx) {
				oldx= x;
				rectz+= ofs;
				rectp+= ofs;
			}
			
			if(x>=0 && y>=0 && x<R.rectx) {
				if(vergz<*rectz) {
					*rectz= vergz;
					*rectp= Zvlnr;
				}
			}
			
			v1[0]+= dx;
			vergz+= dz;
		}
	}
}


void zbufclipwire(VlakRen *vlr)
{
	float *f1, *f2, *f3, *f4= 0,  deler;
	int c1, c2, c3, c4, ec, and, or;

	/* edgecode: 1= draw */
	ec = vlr->ec;
	if(ec==0) return;

	c1= vlr->v1->clip;
	c2= vlr->v2->clip;
	c3= vlr->v3->clip;
	f1= vlr->v1->ho;
	f2= vlr->v2->ho;
	f3= vlr->v3->ho;
	
	if(vlr->v4) {
		f4= vlr->v4->ho;
		c4= vlr->v4->clip;
		
		and= (c1 & c2 & c3 & c4);
		or= (c1 | c2 | c3 | c4);
	}
	else {
		and= (c1 & c2 & c3);
		or= (c1 | c2 | c3);
	}
	
	if(or) {	/* not in the middle */
		if(and) {	/* out completely */
			return;
		}
		else {	/* clipping */

			if(ec & ME_V1V2) {
				QUATCOPY(vez, f1);
				QUATCOPY(vez+4, f2);
				if( clipline(vez, vez+4)) {
					hoco_to_zco(vez, vez);
					hoco_to_zco(vez+4, vez+4);
					zbuflinefunc(vez, vez+4);
				}
			}
			if(ec & ME_V2V3) {
				QUATCOPY(vez, f2);
				QUATCOPY(vez+4, f3);
				if( clipline(vez, vez+4)) {
					hoco_to_zco(vez, vez);
					hoco_to_zco(vez+4, vez+4);
					zbuflinefunc(vez, vez+4);
				}
			}
			if(vlr->v4) {
				if(ec & ME_V3V4) {
					QUATCOPY(vez, f3);
					QUATCOPY(vez+4, f4);
					if( clipline(vez, vez+4)) {
						hoco_to_zco(vez, vez);
						hoco_to_zco(vez+4, vez+4);
						zbuflinefunc(vez, vez+4);
					}
				}
				if(ec & ME_V4V1) {
					QUATCOPY(vez, f4);
					QUATCOPY(vez+4, f1);
					if( clipline(vez, vez+4)) {
						hoco_to_zco(vez, vez);
						hoco_to_zco(vez+4, vez+4);
						zbuflinefunc(vez, vez+4);
					}
				}
			}
			else {
				if(ec & ME_V3V1) {
					QUATCOPY(vez, f3);
					QUATCOPY(vez+4, f1);
					if( clipline(vez, vez+4)) {
						hoco_to_zco(vez, vez);
						hoco_to_zco(vez+4, vez+4);
						zbuflinefunc(vez, vez+4);
					}
				}
			}
			
			return;
		}
	}

	deler= f1[3];
	vez[0]= Zmulx*(1.0+f1[0]/deler)+ Zjitx;
	vez[1]= Zmuly*(1.0+f1[1]/deler)+ Zjity;
	vez[2]= 0x7FFFFFFF *(f1[2]/deler);

	deler= f2[3];
	vez[4]= Zmulx*(1.0+f2[0]/deler)+ Zjitx;
	vez[5]= Zmuly*(1.0+f2[1]/deler)+ Zjity;
	vez[6]= 0x7FFFFFFF *(f2[2]/deler);

	deler= f3[3];
	vez[8]= Zmulx*(1.0+f3[0]/deler)+ Zjitx;
	vez[9]= Zmuly*(1.0+f3[1]/deler)+ Zjity;
	vez[10]= 0x7FFFFFFF *(f3[2]/deler);
	
	if(vlr->v4) {
		deler= f4[3];
		vez[12]= Zmulx*(1.0+f4[0]/deler)+ Zjitx;
		vez[13]= Zmuly*(1.0+f4[1]/deler)+ Zjity;
		vez[14]= 0x7FFFFFFF *(f4[2]/deler);

		if(ec & ME_V3V4)  zbuflinefunc(vez+8, vez+12);
		if(ec & ME_V4V1)  zbuflinefunc(vez+12, vez);
	}
	else {
		if(ec & ME_V3V1)  zbuflinefunc(vez+8, vez);
	}

	if(ec & ME_V1V2)  zbuflinefunc(vez, vez+4);
	if(ec & ME_V2V3)  zbuflinefunc(vez+4, vez+8);
	


}

void zbufinvulGLinv(v1,v2,v3) 
float *v1,*v2,*v3;
/* fills in R.rectot the value of Zvlnr using R.rectz */
/* INVERSE Z COMPARISION: BACKSIDE GETS VISIBLE */
{
	double x0,y0,z0,x1,y1,z1,x2,y2,z2,xx1;
	double zxd,zyd,zy0,tmp;
	float *minv,*maxv,*midv;
	unsigned int *rectpofs,*rp;
	int *rz,zverg,zvlak,x;
	int my0,my2,sn1,sn2,rectx,zd,*rectzofs;
	int y,omsl,xs0,xs1,xs2,xs3, dx0,dx1,dx2;
	
	/* MIN MAX */
	if(v1[1]<v2[1]) {
		if(v2[1]<v3[1]) 	{
			minv=v1;  midv=v2;  maxv=v3;
		}
		else if(v1[1]<v3[1]) 	{
			minv=v1;  midv=v3;  maxv=v2;
		}
		else	{
			minv=v3;  midv=v1;  maxv=v2;
		}
	}
	else {
		if(v1[1]<v3[1]) 	{
			minv=v2;  midv=v1; maxv=v3;
		}
		else if(v2[1]<v3[1]) 	{
			minv=v2;  midv=v3;  maxv=v1;
		}
		else	{
			minv=v3;  midv=v2;  maxv=v1;
		}
	}

	my0= ceil(minv[1]);
	my2= floor(maxv[1]);
	omsl= floor(midv[1]);

	if(my2<0 || my0> R.recty) return;

	if(my0<0) my0=0;

	/* EDGES : LONGEST */
	xx1= maxv[1]-minv[1];
	if(xx1>2.0/65536.0) {
		z0= (maxv[0]-minv[0])/xx1;
		
		tmp= (-65536.0*z0);
		dx0= CLAMPIS(tmp, INT_MIN, INT_MAX);
		
		tmp= 65536.0*(z0*(my2-minv[1])+minv[0]);
		xs0= CLAMPIS(tmp, INT_MIN, INT_MAX);
	}
	else {
		dx0= 0;
		xs0= 65536.0*(MIN2(minv[0],maxv[0]));
	}
	/* EDGES : THE TOP ONE */
	xx1= maxv[1]-midv[1];
	if(xx1>2.0/65536.0) {
		z0= (maxv[0]-midv[0])/xx1;
		
		tmp= (-65536.0*z0);
		dx1= CLAMPIS(tmp, INT_MIN, INT_MAX);
		
		tmp= 65536.0*(z0*(my2-midv[1])+midv[0]);
		xs1= CLAMPIS(tmp, INT_MIN, INT_MAX);
	}
	else {
		dx1= 0;
		xs1= 65536.0*(MIN2(midv[0],maxv[0]));
	}
	/* EDGES : THE BOTTOM ONE */
	xx1= midv[1]-minv[1];
	if(xx1>2.0/65536.0) {
		z0= (midv[0]-minv[0])/xx1;
		
		tmp= (-65536.0*z0);
		dx2= CLAMPIS(tmp, INT_MIN, INT_MAX);
		
		tmp= 65536.0*(z0*(omsl-minv[1])+minv[0]);
		xs2= CLAMPIS(tmp, INT_MIN, INT_MAX);
	}
	else {
		dx2= 0;
		xs2= 65536.0*(MIN2(minv[0],midv[0]));
	}

	/* ZBUF DX DY */
	x1= v1[0]- v2[0];
	x2= v2[0]- v3[0];
	y1= v1[1]- v2[1];
	y2= v2[1]- v3[1];
	z1= v1[2]- v2[2];
	z2= v2[2]- v3[2];
	x0= y1*z2-z1*y2;
	y0= z1*x2-x1*z2;
	z0= x1*y2-y1*x2;

	if(z0==0.0) return;

	if(midv[1]==maxv[1]) omsl= my2;
	if(omsl<0) omsl= -1;  /* then it does the first loop entirely */

	while (my2 >= R.recty) {  /* my2 can be larger */
		xs0+=dx0;
		if (my2<=omsl) {
			xs2+= dx2;
		}
		else{
			xs1+= dx1;
		}
		my2--;
	}

	xx1= (x0*v1[0]+y0*v1[1])/z0+v1[2];

	zxd= -x0/z0;
	zyd= -y0/z0;
	zy0= my2*zyd+xx1;
	zd= (int)CLAMPIS(zxd, INT_MIN, INT_MAX);

	/* start-offset in rect */
	rectx= R.rectx;
	rectzofs= (int *)(R.rectz+rectx*my2);
	rectpofs= (R.rectot+rectx*my2);
	zvlak= Zvlnr;

	xs3= 0;		/* flag */
	if(dx0>dx1) {
		xs3= xs0;
		xs0= xs1;
		xs1= xs3;
		xs3= dx0;
		dx0= dx1;
		dx1= xs3;
		xs3= 1;	/* flag */

	}

	for(y=my2;y>omsl;y--) {

		sn1= xs0>>16;
		xs0+= dx0;

		sn2= xs1>>16;
		xs1+= dx1;

		sn1++;

		if(sn2>=rectx) sn2= rectx-1;
		if(sn1<0) sn1= 0;
		zverg= (int) CLAMPIS((sn1*zxd+zy0), INT_MIN, INT_MAX);
		rz= rectzofs+sn1;
		rp= rectpofs+sn1;
		x= sn2-sn1;
		while(x>=0) {
			if(zverg> *rz || *rz==0x7FFFFFFF) {
				*rz= zverg;
				*rp= zvlak;
			}
			zverg+= zd;
			rz++; 
			rp++; 
			x--;
		}
		zy0-=zyd;
		rectzofs-= rectx;
		rectpofs-= rectx;
	}

	if(xs3) {
		xs0= xs1;
		dx0= dx1;
	}
	if(xs0>xs2) {
		xs3= xs0;
		xs0= xs2;
		xs2= xs3;
		xs3= dx0;
		dx0= dx2;
		dx2= xs3;
	}

	for(;y>=my0;y--) {

		sn1= xs0>>16;
		xs0+= dx0;

		sn2= xs2>>16;
		xs2+= dx2;

		sn1++;

		if(sn2>=rectx) sn2= rectx-1;
		if(sn1<0) sn1= 0;
		zverg= (int) CLAMPIS((sn1*zxd+zy0), INT_MIN, INT_MAX);
		rz= rectzofs+sn1;
		rp= rectpofs+sn1;
		x= sn2-sn1;
		while(x>=0) {
			if(zverg> *rz || *rz==0x7FFFFFFF) {
				*rz= zverg;
				*rp= zvlak;
			}
			zverg+= zd;
			rz++; 
			rp++; 
			x--;
		}

		zy0-=zyd;
		rectzofs-= rectx;
		rectpofs-= rectx;
	}
}

void zbufinvulGL(float *v1, float *v2, float *v3)  /* fills in R.rectot the value Zvlnr using R.rectz */
{
	double x0,y0,z0;
	double x1,y1,z1,x2,y2,z2,xx1;
	double zxd,zyd,zy0,tmp;
	float *minv,*maxv,*midv;
	unsigned int *rectpofs,*rp;
	int *rz,zverg,zvlak,x;
	int my0,my2,sn1,sn2,rectx,zd,*rectzofs;
	int y,omsl,xs0,xs1,xs2,xs3, dx0,dx1,dx2;

	/* MIN MAX */
	if(v1[1]<v2[1]) {
		if(v2[1]<v3[1]) 	{
			minv=v1;  midv=v2;  maxv=v3;
		}
		else if(v1[1]<v3[1]) 	{
			minv=v1;  midv=v3;  maxv=v2;
		}
		else	{
			minv=v3;  midv=v1;  maxv=v2;
		}
	}
	else {
		if(v1[1]<v3[1]) 	{
			minv=v2;  midv=v1; maxv=v3;
		}
		else if(v2[1]<v3[1]) 	{
			minv=v2;  midv=v3;  maxv=v1;
		}
		else	{
			minv=v3;  midv=v2;  maxv=v1;
		}
	}

	if(minv[1] == maxv[1]) return;	/* no zero sized faces */

	my0= ceil(minv[1]);
	my2= floor(maxv[1]);
	omsl= floor(midv[1]);

	if(my2<0 || my0> R.recty) return;

	if(my0<0) my0= 0;

	/* EDGES : THE LONGEST */
	xx1= maxv[1]-minv[1];
	if(xx1!=0.0) {
		z0= (maxv[0]-minv[0])/xx1;
		
		tmp= -65536.0*z0;
		dx0= CLAMPIS(tmp, INT_MIN, INT_MAX);
		
		tmp= 65536.0*(z0*(my2-minv[1]) + minv[0]);
		xs0= CLAMPIS(tmp, INT_MIN, INT_MAX);
	}
	else {
		dx0= 0;
		xs0= 65536.0*(MIN2(minv[0],maxv[0]));
	}
	/* EDGES : THE TOP ONE */
	xx1= maxv[1]-midv[1];
	if(xx1!=0.0) {
		z0= (maxv[0]-midv[0])/xx1;
		
		tmp= -65536.0*z0;
		dx1= CLAMPIS(tmp, INT_MIN, INT_MAX);
		
		tmp= 65536.0*(z0*(my2-midv[1])+midv[0]);
		xs1= CLAMPIS(tmp, INT_MIN, INT_MAX);
	}
	else {
		dx1= 0;
		xs1= 65536.0*(MIN2(midv[0],maxv[0]));
	}
	/* EDGES : BOTTOM ONE */
	xx1= midv[1]-minv[1];
	if(xx1!=0.0) {
		z0= (midv[0]-minv[0])/xx1;
		
		tmp= -65536.0*z0;
		dx2= CLAMPIS(tmp, INT_MIN, INT_MAX);
		
		tmp= 65536.0*(z0*(omsl-minv[1])+minv[0]);
		xs2= CLAMPIS(tmp, INT_MIN, INT_MAX);
	}
	else {
		dx2= 0;
		xs2= 65536.0*(MIN2(minv[0],midv[0]));
	}

	/* ZBUF DX DY */
	x1= v1[0]- v2[0];
	x2= v2[0]- v3[0];
	y1= v1[1]- v2[1];
	y2= v2[1]- v3[1];
	z1= v1[2]- v2[2];
	z2= v2[2]- v3[2];
	x0= y1*z2-z1*y2;
	y0= z1*x2-x1*z2;
	z0= x1*y2-y1*x2;

	if(z0==0.0) return;

	if(midv[1]==maxv[1]) omsl= my2;
	if(omsl<0) omsl= -1;  /* then it takes the first loop entirely */

	while (my2 >= R.recty) {  /* my2 can be larger */
		xs0+=dx0;
		if (my2<=omsl) {
			xs2+= dx2;
		}
		else{
			xs1+= dx1;
		}
		my2--;
	}

	xx1= (x0*v1[0]+y0*v1[1])/z0+v1[2];

	zxd= -x0/z0;
	zyd= -y0/z0;
	zy0= my2*zyd+xx1;
	zd= (int)CLAMPIS(zxd, INT_MIN, INT_MAX);

	/* start-offset in rect */
	rectx= R.rectx;
	rectzofs= (int *)(R.rectz+rectx*my2);
	rectpofs= (R.rectot+rectx*my2);
	zvlak= Zvlnr;

	xs3= 0;		/* flag */
	if(dx0>dx1) {
		xs3= xs0;
		xs0= xs1;
		xs1= xs3;
		xs3= dx0;
		dx0= dx1;
		dx1= xs3;
		xs3= 1;	/* flag */

	}

	for(y=my2;y>omsl;y--) {
		
		/* endian insensitive */
		sn1= xs0>>16;
		xs0+= dx0;

		sn2= xs1>>16;
		xs1+= dx1;

		sn1++;

		if(sn2>=rectx) sn2= rectx-1;
		if(sn1<0) sn1= 0;
		zverg= (int) CLAMPIS((sn1*zxd+zy0), INT_MIN, INT_MAX);
		rz= rectzofs+sn1;
		rp= rectpofs+sn1;
		x= sn2-sn1;

		while(x>=0) {
			if(zverg< *rz) {
				*rz= zverg;
				*rp= zvlak;
			}
			zverg+= zd;
			rz++; 
			rp++; 
			x--;
		}
		zy0-=zyd;
		rectzofs-= rectx;
		rectpofs-= rectx;
	}

	if(xs3) {
		xs0= xs1;
		dx0= dx1;
	}
	if(xs0>xs2) {
		xs3= xs0;
		xs0= xs2;
		xs2= xs3;
		xs3= dx0;
		dx0= dx2;
		dx2= xs3;
	}

	for(;y>=my0;y--) {

		sn1= xs0>>16;
		xs0+= dx0;

		sn2= xs2>>16;
		xs2+= dx2;

		sn1++;

		if(sn2>=rectx) sn2= rectx-1;
		if(sn1<0) sn1= 0;
		zverg= (int) CLAMPIS((sn1*zxd+zy0), INT_MIN, INT_MAX);
		rz= rectzofs+sn1;
		rp= rectpofs+sn1;
		x= sn2-sn1;
		while(x>=0) {
			if(zverg< *rz) {
				*rz= zverg;
				*rp= zvlak;
			}
			zverg+= zd;
			rz++; 
			rp++; 
			x--;
		}

		zy0-=zyd;
		rectzofs-= rectx;
		rectpofs-= rectx;
	}
}


void zbufinvulGL_onlyZ(float *v1, float *v2, float *v3)   /* only fills in R.rectz */
{
	double x0,y0,z0,x1,y1,z1,x2,y2,z2,xx1;
	double zxd,zyd,zy0,tmp;
	float *minv,*maxv,*midv;
	int *rz,zverg,x;
	int my0,my2,sn1,sn2,rectx,zd,*rectzofs;
	int y,omsl,xs0,xs1,xs2,xs3, dx0,dx1,dx2;
	
	/* MIN MAX */
	if(v1[1]<v2[1]) {
		if(v2[1]<v3[1]) 	{
			minv=v1; 
			midv=v2; 
			maxv=v3;
		}
		else if(v1[1]<v3[1]) 	{
			minv=v1; 
			midv=v3; 
			maxv=v2;
		}
		else	{
			minv=v3; 
			midv=v1; 
			maxv=v2;
		}
	}
	else {
		if(v1[1]<v3[1]) 	{
			minv=v2; 
			midv=v1; 
			maxv=v3;
		}
		else if(v2[1]<v3[1]) 	{
			minv=v2; 
			midv=v3; 
			maxv=v1;
		}
		else	{
			minv=v3; 
			midv=v2; 
			maxv=v1;
		}
	}

	my0= ceil(minv[1]);
	my2= floor(maxv[1]);
	omsl= floor(midv[1]);

	if(my2<0 || my0> R.recty) return;

	if(my0<0) my0=0;

	/* EDGES : LONGEST */
	xx1= maxv[1]-minv[1];
	if(xx1!=0.0) {
		z0= (maxv[0]-minv[0])/xx1;
		
		tmp= (-65536.0*z0);
		dx0= CLAMPIS(tmp, INT_MIN, INT_MAX);
		
		tmp= 65536.0*(z0*(my2-minv[1])+minv[0]);
		xs0= CLAMPIS(tmp, INT_MIN, INT_MAX);
	}
	else {
		dx0= 0;
		xs0= 65536.0*(MIN2(minv[0],maxv[0]));
	}
	/* EDGES : TOP */
	xx1= maxv[1]-midv[1];
	if(xx1!=0.0) {
		z0= (maxv[0]-midv[0])/xx1;
		
		tmp= (-65536.0*z0);
		dx1= CLAMPIS(tmp, INT_MIN, INT_MAX);
		
		tmp= 65536.0*(z0*(my2-midv[1])+midv[0]);
		xs1= CLAMPIS(tmp, INT_MIN, INT_MAX);
	}
	else {
		dx1= 0;
		xs1= 65536.0*(MIN2(midv[0],maxv[0]));
	}
	/* EDGES : BOTTOM */
	xx1= midv[1]-minv[1];
	if(xx1!=0.0) {
		z0= (midv[0]-minv[0])/xx1;
		
		tmp= (-65536.0*z0);
		dx2= CLAMPIS(tmp, INT_MIN, INT_MAX);
		
		tmp= 65536.0*(z0*(omsl-minv[1])+minv[0]);
		xs2= CLAMPIS(tmp, INT_MIN, INT_MAX);
	}
	else {
		dx2= 0;
		xs2= 65536.0*(MIN2(minv[0],midv[0]));
	}

	/* ZBUF DX DY */
	x1= v1[0]- v2[0];
	x2= v2[0]- v3[0];
	y1= v1[1]- v2[1];
	y2= v2[1]- v3[1];
	z1= v1[2]- v2[2];
	z2= v2[2]- v3[2];
	x0= y1*z2-z1*y2;
	y0= z1*x2-x1*z2;
	z0= x1*y2-y1*x2;

	if(z0==0.0) return;

	if(midv[1]==maxv[1]) omsl= my2;
	if(omsl<0) omsl= -1;  /* then it takes first loop entirely */

	while (my2 >= R.recty) {  /* my2 can be larger */
		xs0+=dx0;
		if (my2<=omsl) {
			xs2+= dx2;
		}
		else{
			xs1+= dx1;
		}
		my2--;
	}

	xx1= (x0*v1[0]+y0*v1[1])/z0+v1[2];

	zxd= -x0/z0;
	zyd= -y0/z0;
	zy0= my2*zyd+xx1;
	zd= (int)CLAMPIS(zxd, INT_MIN, INT_MAX);

	/* start-offset in rect */
	rectx= R.rectx;
	rectzofs= (int *)(R.rectz+rectx*my2);

	xs3= 0;		/* flag */
	if(dx0>dx1) {
		xs3= xs0;
		xs0= xs1;
		xs1= xs3;
		xs3= dx0;
		dx0= dx1;
		dx1= xs3;
		xs3= 1;	/* flag */

	}

	for(y=my2;y>omsl;y--) {

		sn1= xs0>>16;
		xs0+= dx0;

		sn2= xs1>>16;
		xs1+= dx1;

		sn1++;

		if(sn2>=rectx) sn2= rectx-1;
		if(sn1<0) sn1= 0;
		zverg= (int) CLAMPIS((sn1*zxd+zy0), INT_MIN, INT_MAX);
		rz= rectzofs+sn1;

		x= sn2-sn1;
		while(x>=0) {
			if(zverg< *rz) {
				*rz= zverg;
			}
			zverg+= zd;
			rz++; 
			x--;
		}
		zy0-=zyd;
		rectzofs-= rectx;
	}

	if(xs3) {
		xs0= xs1;
		dx0= dx1;
	}
	if(xs0>xs2) {
		xs3= xs0;
		xs0= xs2;
		xs2= xs3;
		xs3= dx0;
		dx0= dx2;
		dx2= xs3;
	}

	for(;y>=my0;y--) {

		sn1= xs0>>16;
		xs0+= dx0;

		sn2= xs2>>16;
		xs2+= dx2;

		sn1++;

		if(sn2>=rectx) sn2= rectx-1;
		if(sn1<0) sn1= 0;
		zverg= (int) CLAMPIS((sn1*zxd+zy0), INT_MIN, INT_MAX);
		rz= rectzofs+sn1;

		x= sn2-sn1;
		while(x>=0) {
			if(zverg< *rz) {
				*rz= zverg;
			}
			zverg+= zd;
			rz++; 
			x--;
		}

		zy0-=zyd;
		rectzofs-= rectx;
	}
}

void print3floats(float *v1, float *v2, float *v3)
{
	printf("1  %f %f %f %f\n", v1[0], v1[1], v1[2], v1[3]);
	printf("2  %f %f %f %f\n", v2[0], v2[1], v2[2], v2[3]);
	printf("3  %f %f %f %f\n", v3[0], v3[1], v3[2], v3[3]);
}

static short cliptestf(float p, float q, float *u1, float *u2)
{
	float r;

	if(p<0.0) {
		if(q<p) return 0;
		else if(q<0.0) {
			r= q/p;
			if(r>*u2) return 0;
			else if(r>*u1) *u1=r;
		}
	}
	else {
		if(p>0.0) {
			if(q<0.0) return 0;
			else if(q<p) {
				r= q/p;
				if(r<*u1) return 0;
				else if(r<*u2) *u2=r;
			}
		}
		else if(q<0.0) return 0;
	}
	return 1;
}

int RE_testclip(float *v)
{
	float abs4;	/* WATCH IT: this function should do the same as cliptestf, otherwise troubles in zbufclip()*/
	short c=0;

	abs4= fabs(v[3]);

	if(v[2]< -abs4) c=16;			/* this used to be " if(v[2]<0) ", see clippz() */
	else if(v[2]> abs4) c+= 32;

	if( v[0]>abs4) c+=2;
	else if( v[0]< -abs4) c+=1;

	if( v[1]>abs4) c+=4;
	else if( v[1]< -abs4) c+=8;

	return c;
}


static void clipp(float *v1, float *v2, int b1, int *b2, int *b3, int a)
{
	float da,db,u1=0.0,u2=1.0;

	labda[b1][0]= -1.0;
	labda[b1][1]= -1.0;

	da= v2[a]-v1[a];
	db= v2[3]-v1[3];

	/* according the original article by Liang&Barsky, for clipping of
	 * homeginic coordinates with viewplane, the value of "0" is used instead of "-w" .
	 * This differs from the other clipping cases (like left or top) and I considered
	 * it to be not so 'homogenic'. But later it has proven to be an error,
	 * who would have thought that of L&B!
	 */

	if(cliptestf(-da-db, v1[3]+v1[a], &u1,&u2)) {
		if(cliptestf(da-db, v1[3]-v1[a], &u1,&u2)) {
			*b3=1;
			if(u2<1.0) {
				labda[b1][1]= u2;
				*b2=1;
			}
			else labda[b1][1]=1.0;  /* u2 */
			if(u1>0.0) {
				labda[b1][0]= u1;
				*b2=1;
			} else labda[b1][0]=0.0;
		}
	}
}

static int clipline(float *v1, float *v2)	/* return 0: do not draw */
{
	float dz,dw, u1=0.0, u2=1.0;
	float dx, dy;

	dz= v2[2]-v1[2];
	dw= v2[3]-v1[3];

	if(cliptestf(-dz-dw, v1[3]+v1[2], &u1,&u2)) {
		if(cliptestf(dz-dw, v1[3]-v1[2], &u1,&u2)) {

			dx= v2[0]-v1[0];
			dz= v2[3]-v1[3];
		
			if(cliptestf(-dx-dz, v1[0]+v1[3], &u1,&u2)) {
				if(cliptestf(dx-dz, v1[3]-v1[0], &u1,&u2)) {

					dy= v2[1]-v1[1];
				
					if(cliptestf(-dy-dz,v1[1]+v1[3],&u1,&u2)) {
						if(cliptestf(dy-dz,v1[3]-v1[1],&u1,&u2)) {
						
							if(u2<1.0) {
								v2[0]= v1[0]+u2*dx;
								v2[1]= v1[1]+u2*dy;
								v2[2]= v1[2]+u2*dz;
								v2[3]= v1[3]+u2*dw;
							}
							if(u1>0.0) {
								v1[0]= v1[0]+u1*dx;
								v1[1]= v1[1]+u1*dy;
								v1[2]= v1[2]+u1*dz;
								v1[3]= v1[3]+u1*dw;
							}
							return 1;
						}
					}
				}
			}
		}
	}
	
	return 0;
}


static void maakvertpira(float *v1, float *v2, int *b1, int b2, int *clve)
{
	float l1,l2,*adr;

	l1= labda[b2][0];
	l2= labda[b2][1];

	if(l1!= -1.0) {
		if(l1!= 0.0) {
			adr= vez+4*(*clve);
			p[*b1]=adr;
			(*clve)++;
			adr[0]= v1[0]+l1*(v2[0]-v1[0]);
			adr[1]= v1[1]+l1*(v2[1]-v1[1]);
			adr[2]= v1[2]+l1*(v2[2]-v1[2]);
			adr[3]= v1[3]+l1*(v2[3]-v1[3]);
		} else p[*b1]= v1;
		(*b1)++;
	}
	if(l2!= -1.0) {
		if(l2!= 1.0) {
			adr= vez+4*(*clve);
			p[*b1]=adr;
			(*clve)++;
			adr[0]= v1[0]+l2*(v2[0]-v1[0]);
			adr[1]= v1[1]+l2*(v2[1]-v1[1]);
			adr[2]= v1[2]+l2*(v2[2]-v1[2]);
			adr[3]= v1[3]+l2*(v2[3]-v1[3]);
			(*b1)++;
		}
	}
}

/* ------------------------------------------------------------------------- */

void RE_projectverto(float *v1, float *adr)
{
	/* calcs homogenic coord of vertex v1 */
	float x,y,z;

	x= v1[0]; 
	y= v1[1]; 
	z= v1[2];
	adr[0]= x*R.winmat[0][0]          + z*R.winmat[2][0];
	adr[1]= 		      y*R.winmat[1][1]+ z*R.winmat[2][1];
	adr[2]=                             z*R.winmat[2][2] + R.winmat[3][2];
	adr[3]=                             z*R.winmat[2][3] + R.winmat[3][3];

}

/* ------------------------------------------------------------------------- */

void projectvert(float *v1, float *adr)
{
	/* calcs homogenic coord of vertex v1 */
	float x,y,z;

	x= v1[0]; 
	y= v1[1]; 
	z= v1[2];
	adr[0]= x*R.winmat[0][0]+ y*R.winmat[1][0]+ z*R.winmat[2][0]+ R.winmat[3][0];
	adr[1]= x*R.winmat[0][1]+ y*R.winmat[1][1]+ z*R.winmat[2][1]+ R.winmat[3][1];
	adr[2]= x*R.winmat[0][2]+ y*R.winmat[1][2]+ z*R.winmat[2][2]+ R.winmat[3][2];
	adr[3]= x*R.winmat[0][3]+ y*R.winmat[1][3]+ z*R.winmat[2][3]+ R.winmat[3][3];
}


void zbufclip(float *f1, float *f2, float *f3, int c1, int c2, int c3)
{
	float deler;

	if(c1 | c2 | c3) {	/* not in middle */
		if(c1 & c2 & c3) {	/* completely out */
			return;
		} else {	/* clipping */
			int arg, v, b, clipflag[3], b1, b2, b3, c4, clve=3, clvlo, clvl=1;

			vez[0]= f1[0]; vez[1]= f1[1]; vez[2]= f1[2]; vez[3]= f1[3];
			vez[4]= f2[0]; vez[5]= f2[1]; vez[6]= f2[2]; vez[7]= f2[3];
			vez[8]= f3[0]; vez[9]= f3[1]; vez[10]= f3[2];vez[11]= f3[3];

			vlzp[0][0]= vez;
			vlzp[0][1]= vez+4;
			vlzp[0][2]= vez+8;

			clipflag[0]= ( (c1 & 48) | (c2 & 48) | (c3 & 48) );
			if(clipflag[0]==0) {	/* othwerwise it needs to be calculated again, after the first (z) clip */
				clipflag[1]= ( (c1 & 3) | (c2 & 3) | (c3 & 3) );
				clipflag[2]= ( (c1 & 12) | (c2 & 12) | (c3 & 12) );
			}
			
			for(b=0;b<3;b++) {
				
				if(clipflag[b]) {
				
					clvlo= clvl;
					
					for(v=0; v<clvlo; v++) {
					
						if(vlzp[v][0]!=0) {	/* face is still there */
							b2= b3 =0;	/* clip flags */

							if(b==0) arg= 2;
							else if (b==1) arg= 0;
							else arg= 1;
							
							clipp(vlzp[v][0],vlzp[v][1],0,&b2,&b3, arg);
							clipp(vlzp[v][1],vlzp[v][2],1,&b2,&b3, arg);
							clipp(vlzp[v][2],vlzp[v][0],2,&b2,&b3, arg);

							if(b2==0 && b3==1) {
								/* completely 'in' */;
							} else if(b3==0) {
								vlzp[v][0]=0;
								/* completely 'out' */;
							} else {
								b1=0;
								maakvertpira(vlzp[v][0],vlzp[v][1],&b1,0,&clve);
								maakvertpira(vlzp[v][1],vlzp[v][2],&b1,1,&clve);
								maakvertpira(vlzp[v][2],vlzp[v][0],&b1,2,&clve);

								/* after front clip done: now set clip flags */
								if(b==0) {
									clipflag[1]= clipflag[2]= 0;
									f1= vez;
									for(b3=0; b3<clve; b3++) {
										c4= RE_testclip(f1);
										clipflag[1] |= (c4 & 3);
										clipflag[2] |= (c4 & 12);
										f1+= 4;
									}
								}
								
								vlzp[v][0]=0;
								if(b1>2) {
									for(b3=3; b3<=b1; b3++) {
										vlzp[clvl][0]= p[0];
										vlzp[clvl][1]= p[b3-2];
										vlzp[clvl][2]= p[b3-1];
										clvl++;
									}
								}
							}
						}
					}
				}
			}

            /* warning, this should never happen! */
			if(clve>38) printf("clip overflow: clve clvl %d %d\n",clve,clvl);

            /* perspective division */
			f1=vez;
			for(c1=0;c1<clve;c1++) {
				deler= f1[3];
				f1[0]= Zmulx*(1.0+f1[0]/deler)+ Zjitx;
				f1[1]= Zmuly*(1.0+f1[1]/deler)+ Zjity;
				f1[2]= 0x7FFFFFFF *(f1[2]/deler);
				f1+=4;
			}
			for(b=1;b<clvl;b++) {
				if(vlzp[b][0]) {
					zbuffunc(vlzp[b][0],vlzp[b][1],vlzp[b][2]);
				}
			}
			return;
		}
	}

	/* perspective division: HCS to ZCS */
	
	deler= f1[3];
	vez[0]= Zmulx*(1.0+f1[0]/deler)+ Zjitx;
	vez[1]= Zmuly*(1.0+f1[1]/deler)+ Zjity;
	vez[2]= 0x7FFFFFFF *(f1[2]/deler);

	deler= f2[3];
	vez[4]= Zmulx*(1.0+f2[0]/deler)+ Zjitx;
	vez[5]= Zmuly*(1.0+f2[1]/deler)+ Zjity;
	vez[6]= 0x7FFFFFFF *(f2[2]/deler);

	deler= f3[3];
	vez[8]= Zmulx*(1.0+f3[0]/deler)+ Zjitx;
	vez[9]= Zmuly*(1.0+f3[1]/deler)+ Zjity;
	vez[10]= 0x7FFFFFFF *(f3[2]/deler);

	zbuffunc(vez,vez+4,vez+8);
}

/* ***************** ZBUFFER MAIN ROUTINES **************** */


void zbufferall(void)
{
	VlakRen *vlr= NULL;
	Material *ma=0;
	int v;
	short transp=0, env=0, wire=0;

	Zmulx= ((float)R.rectx)/2.0;
	Zmuly= ((float)R.recty)/2.0;

	fillrect(R.rectz, R.rectx, R.recty, 0x7FFFFFFF);

	Zvlnr= 0;

	zbuffunc= zbufinvulGL;
	zbuflinefunc= zbufline;

	for(v=0;v<R.totvlak;v++) {

		if((v & 255)==0) vlr= R.blovl[v>>8];
		else vlr++;
		
		if(vlr->flag & R_VISIBLE) {
			if(vlr->mat!=ma) {
				ma= vlr->mat;
				transp= ma->mode & MA_ZTRA;
				env= (ma->mode & MA_ENV);
				wire= (ma->mode & MA_WIRE);
				
				if(ma->mode & MA_ZINV) zbuffunc= zbufinvulGLinv;
				else zbuffunc= zbufinvulGL;
			}
			
			if(transp==0) {
				if(env) Zvlnr= 0;
				else Zvlnr= v+1;
				
				if(wire) zbufclipwire(vlr);
				else {
					zbufclip(vlr->v1->ho, vlr->v2->ho, vlr->v3->ho, vlr->v1->clip, vlr->v2->clip, vlr->v3->clip);
					if(vlr->v4) {
						if(Zvlnr) Zvlnr+= 0x800000;
						zbufclip(vlr->v1->ho, vlr->v3->ho, vlr->v4->ho, vlr->v1->clip, vlr->v3->clip, vlr->v4->clip);
					}
				}
			}
		}
	}
}

static int hashlist_projectvert(float *v1, float *hoco)
{
	static VertBucket bucket[256], *buck;
	
	if(v1==0) {
		memset(bucket, 0, 256*sizeof(VertBucket));
		return 0;
	}
	
	buck= &bucket[ (((long)v1)/16) & 255 ];
	if(buck->vert==v1) {
		COPY_16(hoco, buck->hoco);
		return buck->clip;
	}
	
	projectvert(v1, hoco);
	buck->clip = RE_testclip(hoco);
	buck->vert= v1;
	COPY_16(buck->hoco, hoco);
	return buck->clip;
}


void RE_zbufferall_radio(struct RadView *vw, RNode **rg_elem, int rg_totelem)
{
	RNode **re, *rn;
	float hoco[4][4];
	int a;
	int c1, c2, c3, c4= 0;
	unsigned int *rectoto, *rectzo;
	int rectxo, rectyo;

	if(rg_totelem==0) return;

	hashlist_projectvert(0, 0);
	
	rectxo= R.rectx;
	rectyo= R.recty;
	rectoto= R.rectot;
	rectzo= R.rectz;
	
	R.rectx= vw->rectx;
	R.recty= vw->recty;
	R.rectot= vw->rect;
	R.rectz= vw->rectz;
	
	Zmulx= ((float)R.rectx)/2.0;
	Zmuly= ((float)R.recty)/2.0;

	/* needed for projectvert */
	MTC_Mat4MulMat4(R.winmat, vw->viewmat, vw->winmat);

	fillrect(R.rectz, R.rectx, R.recty, 0x7FFFFFFF);
	fillrect(R.rectot, R.rectx, R.recty, 0xFFFFFF);

	zbuffunc= zbufinvulGL;

	re= rg_elem;
	re+= (rg_totelem-1);
	for(a= rg_totelem-1; a>=0; a--, re--) {
		rn= *re;
		if( (rn->f & RAD_SHOOT)==0 ) {    /* no shootelement */
			
			if( rn->f & RAD_BACKFACE) Zvlnr= 0xFFFFFF;	
			else Zvlnr= a;
			
			c1= hashlist_projectvert(rn->v1, hoco[0]);
			c2= hashlist_projectvert(rn->v2, hoco[1]);
			c3= hashlist_projectvert(rn->v3, hoco[2]);
			
			if(rn->v4) {
				c4= hashlist_projectvert(rn->v4, hoco[3]);
			}

			zbufclip(hoco[0], hoco[1], hoco[2], c1, c2, c3);
			if(rn->v4) {
				zbufclip(hoco[0], hoco[2], hoco[3], c1, c3, c4);
			}
		}
	}

	/* restore */
	R.rectx= rectxo;
	R.recty= rectyo;
	R.rectot= rectoto;
	R.rectz= rectzo;

}

void zbuffershad(LampRen *lar)
{
	VlakRen *vlr= NULL;
	Material *ma=0;
	int a, ok=1, lay= -1;

	if(lar->mode & LA_LAYER) lay= lar->lay;

	Zmulx= ((float)R.rectx)/2.0;
	Zmuly= ((float)R.recty)/2.0;
	Zjitx= Zjity= -.5;

	fillrect(R.rectz,R.rectx,R.recty,0x7FFFFFFE);

	zbuffunc= zbufinvulGL_onlyZ;

	for(a=0;a<R.totvlak;a++) {

		if((a & 255)==0) vlr= R.blovl[a>>8];
		else vlr++;

		if(vlr->mat!= ma) {
			ma= vlr->mat;
			ok= 1;
			if((ma->mode & MA_TRACEBLE)==0) ok= 0;
		}
		
		if(ok && (vlr->flag & R_VISIBLE) && (vlr->lay & lay)) {
			zbufclip(vlr->v1->ho, vlr->v2->ho, vlr->v3->ho, vlr->v1->clip, vlr->v2->clip, vlr->v3->clip);
			if(vlr->v4) zbufclip(vlr->v1->ho, vlr->v3->ho, vlr->v4->ho, vlr->v1->clip, vlr->v3->clip, vlr->v4->clip);
		}
	}
}



/* ******************** ABUF ************************* */


void bgnaccumbuf(void)
{
	
	Acolrow= MEM_mallocN(4*sizeof(short)*R.rectx, "Acolrow");
	Arectz= MEM_mallocN(sizeof(int)*ABUFPART*R.rectx, "Arectz");
	APixbuf= MEM_mallocN(ABUFPART*R.rectx*sizeof(APixstr), "APixbuf");

	Aminy= -1000;
	Amaxy= -1000;
	
	apsmteller= 0;
	apsmfirst.next= 0;
	apsmfirst.ps= 0;
} 
/* ------------------------------------------------------------------------ */

void endaccumbuf(void)
{
	
	MEM_freeN(Acolrow);
	MEM_freeN(Arectz);
	MEM_freeN(APixbuf);
	freepsA();
}

/* ------------------------------------------------------------------------ */

void copyto_abufz(int sample)
{
	PixStr *ps;
	int x, y, *rza;
	long *rd;
	
	memcpy(Arectz, R.rectz+ R.rectx*Aminy, 4*R.rectx*(Amaxy-Aminy+1));

	if( (R.r.mode & R_OSA)==0 || sample==0) return;
		
	rza= Arectz;
	rd= (R.rectdaps+ R.rectx*Aminy);

	sample= (1<<sample);
	
	for(y=Aminy; y<=Amaxy; y++) {
		for(x=0; x<R.rectx; x++) {
			
			if( IS_A_POINTER_CODE(*rd)) {	
				ps= (PixStr *) POINTER_FROM_CODE(*rd);

				while(ps) {
					if(sample & ps->mask) {
						*rza= ps->z;
						break;
					}
					ps= ps->next;
				}
			}
			
			rd++; rza++;
		}
	}
}


/* ------------------------------------------------------------------------ */

void zbuffer_abuf()
{
	float vec[3], hoco[4], mul, zval, fval;
	Material *ma=0;
	int v, len;
	
	Zjitx= Zjity= -.5;
	Zmulx= ((float)R.rectx)/2.0;
	Zmuly= ((float)R.recty)/2.0;

	/* clear APixstructs */
	len= sizeof(APixstr)*R.rectx*ABUFPART;
	memset(APixbuf, 0, len);
	
	zbuffunc= zbufinvulAc;
	zbuflinefunc= zbuflineAc;

	for(Zsample=0; Zsample<R.osa || R.osa==0; Zsample++) {
		
		copyto_abufz(Zsample);	/* init zbuffer */

		if(R.r.mode & R_OSA) {
			Zjitx= -jit[Zsample][0];
			Zjity= -jit[Zsample][1];
		}
		
		for(v=0; v<R.totvlak; v++) {
			if((v & 255)==0) {
				Zvlr= R.blovl[v>>8];
			}
			else Zvlr++;
			
			ma= Zvlr->mat;

			if(ma->mode & (MA_ZTRA)) {

						/* a little advantage for transp rendering (a z offset) */
				if( ma->zoffs != 0.0) {
					mul= 0x7FFFFFFF;
					zval= mul*(1.0+Zvlr->v1->ho[2]/Zvlr->v1->ho[3]);

					VECCOPY(vec, Zvlr->v1->co);
					/* z is negative, otherwise its being clipped */ 
					vec[2]-= ma->zoffs;
					RE_projectverto(vec, hoco);
					fval= mul*(1.0+hoco[2]/hoco[3]);

					Azvoordeel= (int) fabs(zval - fval );
				}
				else Azvoordeel= 0;
				
				Zvlnr= v+1;
		
				if(Zvlr->flag & R_VISIBLE) {
					
					if(ma->mode & (MA_WIRE)) zbufclipwire(Zvlr);
					else {
						zbufclip(Zvlr->v1->ho, Zvlr->v2->ho, Zvlr->v3->ho, Zvlr->v1->clip, Zvlr->v2->clip, Zvlr->v3->clip);
						if(Zvlr->v4) {
							Zvlnr+= 0x800000;
							zbufclip(Zvlr->v1->ho, Zvlr->v3->ho, Zvlr->v4->ho, Zvlr->v1->clip, Zvlr->v3->clip, Zvlr->v4->clip);
						}
					}
				}
			}
			if(RE_local_test_break()) break; 
		}
		
		if((R.r.mode & R_OSA)==0) break;
		if(RE_local_test_break()) break;
	}
	
}

int vergzvlak(const void *a1, const void *a2)
{
	const int *x1=a1, *x2=a2;

	if( x1[0] < x2[0] ) return 1;
	else if( x1[0] > x2[0]) return -1;
	return 0;
}

void shadetrapixel(float x, float y, int vlak)
{
	if( (vlak & 0x7FFFFF) > R.totvlak) {
		printf("error in shadetrapixel nr: %d\n", (vlak & 0x7FFFFF));
		return;
	}

	shadepixel(x, y, vlak);
}

extern unsigned short usegamtab;
extern unsigned short shortcol[4];
void abufsetrow(int y)
{
	APixstr *ap, *apn;
	float xs, ys;
	int x, part, a, b, zrow[100][3], totvlak, alpha[32], tempgam, nr, intcol[4];
	int sval, tempRf;
	unsigned short *col, tempcol[4], sampcol[16*4], *scol;
	
	if(y<0) return;
	if(R.osa>16) {
		printf("abufsetrow: osa too large\n");
		G.afbreek= 1;
		return;
	}

   tempRf= R.flag;
   R.flag &= ~R_LAMPHALO;

	/* alpha LUT */
	if(R.r.mode & R_OSA ) {
		x= (65536/R.osa);
		for(a=0; a<=R.osa; a++) {
			alpha[a]= a*x;
		}
	}
	/* does a pixbuf has to be created? */
	if(y<Aminy || y>Amaxy) {
		part= (y/ABUFPART);
		Aminy= part*ABUFPART;
		Amaxy= Aminy+ABUFPART-1;
		if(Amaxy>=R.recty) Amaxy= R.recty-1;
		freepsA();
		zbuffer_abuf();
	}
	
	/* render row */
	col= Acolrow;
	memset(col, 0, 2*4*R.rectx);
	ap= APixbuf+R.rectx*(y-Aminy);
	ys= y;
	tempgam= usegamtab;
	usegamtab= 0;
	
	for(x=0; x<R.rectx; x++, col+=4, ap++) {
		if(ap->p[0]) {
			/* sort in z */
			totvlak= 0;
			apn= ap;
			while(apn) {
				for(a=0; a<4; a++) {
					if(apn->p[a]) {
						zrow[totvlak][0]= apn->z[a];
						zrow[totvlak][1]= apn->p[a];
						zrow[totvlak][2]= apn->mask[a];
						totvlak++;
						if(totvlak>99) totvlak= 99;
					}
					else break;
				}
				apn= apn->next;
			}
			if(totvlak==1) {
				
				if(R.r.mode & R_OSA ) {
					b= centmask[ ap->mask[0] ];
					xs= (float)x+centLut[b & 15];
					ys= (float)y+centLut[b>>4];
				}
				else {
					xs= x; ys= y;
				}
				shadetrapixel(xs, ys, ap->p[0]);
	
				nr= count_mask(ap->mask[0]);
				if( (R.r.mode & R_OSA) && nr<R.osa) {
					a= alpha[ nr ];
					col[0]= (shortcol[0]*a)>>16;
					col[1]= (shortcol[1]*a)>>16;
					col[2]= (shortcol[2]*a)>>16;
					col[3]= (shortcol[3]*a)>>16;
				}
				else {
					col[0]= shortcol[0];
					col[1]= shortcol[1];
					col[2]= shortcol[2];
					col[3]= shortcol[3];
				}
			}
			else {

				if(totvlak==2) {
					if(zrow[0][0] < zrow[1][0]) {
						a= zrow[0][0]; zrow[0][0]= zrow[1][0]; zrow[1][0]= a;
						a= zrow[0][1]; zrow[0][1]= zrow[1][1]; zrow[1][1]= a;
						a= zrow[0][2]; zrow[0][2]= zrow[1][2]; zrow[1][2]= a;
					}

				}
				else {	/* totvlak>2 */
					qsort(zrow, totvlak, sizeof(int)*3, vergzvlak);
				}
				
				/* join when pixels are adjacent */
				
				while(totvlak>0) {
					totvlak--;
					
					if(R.r.mode & R_OSA) {
						b= centmask[ zrow[totvlak][2] ];
						xs= (float)x+centLut[b & 15];
						ys= (float)y+centLut[b>>4];
					}
					else {
						xs= x; ys= y;
					}
					shadetrapixel(xs, ys, zrow[totvlak][1]);
					
					a= count_mask(zrow[totvlak][2]);
					if( (R.r.mode & R_OSA ) && a<R.osa) {
						if(totvlak>0) {
							memset(sampcol, 0, 4*2*R.osa);
							sval= addtosampcol(sampcol, shortcol, zrow[totvlak][2]);

                     /* sval==0: alpha completely full */
                     while( (sval != 0) && (totvlak>0) ) {
                       a= count_mask(zrow[totvlak-1][2]);
                       if(a==R.osa) break;
                       totvlak--;
                       
                       b= centmask[ zrow[totvlak][2] ];
                       
                       xs= (float)x+centLut[b & 15];
                       ys= (float)y+centLut[b>>4];
                       
                       shadetrapixel(xs, ys, zrow[totvlak][1]);
                       sval= addtosampcol(sampcol, shortcol, zrow[totvlak][2]);
                     }
							scol= sampcol;
							intcol[0]= scol[0]; intcol[1]= scol[1];
							intcol[2]= scol[2]; intcol[3]= scol[3];
							scol+= 4;
							for(a=1; a<R.osa; a++, scol+=4) {
								intcol[0]+= scol[0]; intcol[1]+= scol[1];
								intcol[2]+= scol[2]; intcol[3]+= scol[3];
							}
							tempcol[0]= intcol[0]/R.osa;
							tempcol[1]= intcol[1]/R.osa;
							tempcol[2]= intcol[2]/R.osa;
							tempcol[3]= intcol[3]/R.osa;
							
							addAlphaUnderShort(col, tempcol);
							
						}
						else {
							a= alpha[a];
							shortcol[0]= (shortcol[0]*a)>>16;
							shortcol[1]= (shortcol[1]*a)>>16;
							shortcol[2]= (shortcol[2]*a)>>16;
							shortcol[3]= (shortcol[3]*a)>>16;
							addAlphaUnderShort(col, shortcol);
						}
					}	
					else addAlphaUnderShort(col, shortcol);
					
					if(col[3]>=0xFFF0) break;
				}
			}
		}
	}
	
	usegamtab= tempgam;
   R.flag= tempRf;
}

/* end of zbuf.c */




