/*
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
 * Inc., 51 Franklin Street, Fifth Floor, Boston, MA 02110-1301, USA.
 *
 * The Original Code is Copyright (C) 2001-2002 by NaN Holding BV.
 * All rights reserved.
 *
 * Contributors: Hos, RPW
 *               2004-2006 Blender Foundation, full recode
 *
 * ***** END GPL LICENSE BLOCK *****
 */

/** \file blender/render/intern/source/zbuf.c
 *  \ingroup render
 */



/*---------------------------------------------------------------------------*/
/* Common includes                                                           */
/*---------------------------------------------------------------------------*/

#include <math.h>
#include <float.h>
#include <stdlib.h>
#include <limits.h>
#include <string.h>

#include "BLI_math.h"
#include "BLI_blenlib.h"
#include "BLI_jitter.h"
#include "BLI_threads.h"
#include "BLI_utildefines.h"

#include "MEM_guardedalloc.h"

#include "DNA_lamp_types.h"
#include "DNA_mesh_types.h"
#include "DNA_node_types.h"
#include "DNA_meshdata_types.h"
#include "DNA_material_types.h"

#include "BKE_global.h"
#include "BKE_material.h"


#include "RE_render_ext.h"

/* local includes */
#include "gammaCorrectionTables.h"
#include "pixelblending.h"
#include "render_result.h"
#include "render_types.h"
#include "renderpipeline.h"
#include "renderdatabase.h"
#include "rendercore.h"
#include "shadbuf.h"
#include "shading.h"
#include "sss.h"
#include "strand.h"

/* own includes */
#include "zbuf.h"

/* ~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~ */
/* defined in pipeline.c, is hardcopy of active dynamic allocated Render */
/* only to be used here in this file, it's for speed */
extern struct Render R;
/* ~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~ */


/* ****************** Spans ******************************* */

/* each zbuffer has coordinates transformed to local rect coordinates, so we can simply clip */
void zbuf_alloc_span(ZSpan *zspan, int rectx, int recty, float clipcrop)
{
	memset(zspan, 0, sizeof(ZSpan));
	
	zspan->rectx= rectx;
	zspan->recty= recty;
	
	zspan->span1= MEM_mallocN(recty*sizeof(float), "zspan");
	zspan->span2= MEM_mallocN(recty*sizeof(float), "zspan");

	zspan->clipcrop= clipcrop;
}

void zbuf_free_span(ZSpan *zspan)
{
	if (zspan) {
		if (zspan->span1) MEM_freeN(zspan->span1);
		if (zspan->span2) MEM_freeN(zspan->span2);
		zspan->span1= zspan->span2= NULL;
	}
}

/* reset range for clipping */
static void zbuf_init_span(ZSpan *zspan)
{
	zspan->miny1= zspan->miny2= zspan->recty+1;
	zspan->maxy1= zspan->maxy2= -1;
	zspan->minp1= zspan->maxp1= zspan->minp2= zspan->maxp2= NULL;
}

static void zbuf_add_to_span(ZSpan *zspan, const float *v1, const float *v2)
{
	const float *minv, *maxv;
	float *span;
	float xx1, dx0, xs0;
	int y, my0, my2;
	
	if (v1[1]<v2[1]) {
		minv= v1; maxv= v2;
	}
	else {
		minv= v2; maxv= v1;
	}
	
	my0= ceil(minv[1]);
	my2= floor(maxv[1]);
	
	if (my2<0 || my0>= zspan->recty) return;
	
	/* clip top */
	if (my2>=zspan->recty) my2= zspan->recty-1;
	/* clip bottom */
	if (my0<0) my0= 0;
	
	if (my0>my2) return;
	/* if (my0>my2) should still fill in, that way we get spans that skip nicely */
	
	xx1= maxv[1]-minv[1];
	if (xx1>FLT_EPSILON) {
		dx0= (minv[0]-maxv[0])/xx1;
		xs0= dx0*(minv[1]-my2) + minv[0];
	}
	else {
		dx0= 0.0f;
		xs0= MIN2(minv[0], maxv[0]);
	}
	
	/* empty span */
	if (zspan->maxp1 == NULL) {
		span= zspan->span1;
	}
	else {	/* does it complete left span? */
		if ( maxv == zspan->minp1 || minv==zspan->maxp1) {
			span= zspan->span1;
		}
		else {
			span= zspan->span2;
		}
	}

	if (span==zspan->span1) {
//		printf("left span my0 %d my2 %d\n", my0, my2);
		if (zspan->minp1==NULL || zspan->minp1[1] > minv[1] ) {
			zspan->minp1= minv;
		}
		if (zspan->maxp1==NULL || zspan->maxp1[1] < maxv[1] ) {
			zspan->maxp1= maxv;
		}
		if (my0<zspan->miny1) zspan->miny1= my0;
		if (my2>zspan->maxy1) zspan->maxy1= my2;
	}
	else {
//		printf("right span my0 %d my2 %d\n", my0, my2);
		if (zspan->minp2==NULL || zspan->minp2[1] > minv[1] ) {
			zspan->minp2= minv;
		}
		if (zspan->maxp2==NULL || zspan->maxp2[1] < maxv[1] ) {
			zspan->maxp2= maxv;
		}
		if (my0<zspan->miny2) zspan->miny2= my0;
		if (my2>zspan->maxy2) zspan->maxy2= my2;
	}

	for (y=my2; y>=my0; y--, xs0+= dx0) {
		/* xs0 is the xcoord! */
		span[y]= xs0;
	}
}

/*-----------------------------------------------------------*/ 
/* Functions                                                 */
/*-----------------------------------------------------------*/ 

void fillrect(int *rect, int x, int y, int val)
{
	int len, *drect;

	len= x*y;
	drect= rect;
	while (len>0) {
		len--;
		*drect= val;
		drect++;
	}
}

/* based on Liang&Barsky, for clipping of pyramidical volume */
static short cliptestf(float a, float b, float c, float d, float *u1, float *u2)
{
	float p= a + b, q= c + d, r;
	
	if (p<0.0f) {
		if (q<p) return 0;
		else if (q<0.0f) {
			r= q/p;
			if (r>*u2) return 0;
			else if (r>*u1) *u1=r;
		}
	}
	else {
		if (p>0.0f) {
			if (q<0.0f) return 0;
			else if (q<p) {
				r= q/p;
				if (r<*u1) return 0;
				else if (r<*u2) *u2=r;
			}
		}
		else if (q<0.0f) return 0;
	}
	return 1;
}

int testclip(const float v[4])
{
	float abs4;	/* WATCH IT: this function should do the same as cliptestf, otherwise troubles in zbufclip()*/
	short c=0;
	
	/* if we set clip flags, the clipping should be at least larger than epsilon. 
	 * prevents issues with vertices lying exact on borders */
	abs4= fabsf(v[3]) + FLT_EPSILON;
	
	if ( v[0] < -abs4) c+=1;
	else if ( v[0] > abs4) c+=2;
	
	if ( v[1] > abs4) c+=4;
	else if ( v[1] < -abs4) c+=8;
	
	if (v[2] < -abs4) c+=16;			/* this used to be " if (v[2]<0) ", see clippz() */
	else if (v[2]> abs4) c+= 32;
	
	return c;
}



/* *************  ACCUMULATION ZBUF ************ */


static APixstr *addpsmainA(ListBase *lb)
{
	APixstrMain *psm;

	psm= MEM_mallocN(sizeof(APixstrMain), "addpsmainA");
	BLI_addtail(lb, psm);
	psm->ps= MEM_callocN(4096*sizeof(APixstr), "pixstr");

	return psm->ps;
}

void freepsA(ListBase *lb)
{
	APixstrMain *psm, *psmnext;

	for (psm= lb->first; psm; psm= psmnext) {
		psmnext= psm->next;
		if (psm->ps)
			MEM_freeN(psm->ps);
		MEM_freeN(psm);
	}
}

static APixstr *addpsA(ZSpan *zspan)
{
	/* make new PS */
	if (zspan->apsmcounter==0) {
		zspan->curpstr= addpsmainA(zspan->apsmbase);
		zspan->apsmcounter= 4095;
	}
	else {
		zspan->curpstr++;
		zspan->apsmcounter--;
	}
	return zspan->curpstr;
}

static void zbuffillAc4(ZSpan *zspan, int obi, int zvlnr,
                        const float *v1, const float *v2, const float *v3, const float *v4)
{
	APixstr *ap, *apofs, *apn;
	double zxd, zyd, zy0, zverg;
	float x0, y0, z0;
	float x1, y1, z1, x2, y2, z2, xx1;
	float *span1, *span2;
	int *rz, *rm, x, y;
	int sn1, sn2, rectx, *rectzofs, *rectmaskofs, my0, my2, mask;
	
	/* init */
	zbuf_init_span(zspan);
	
	/* set spans */
	zbuf_add_to_span(zspan, v1, v2);
	zbuf_add_to_span(zspan, v2, v3);
	if (v4) {
		zbuf_add_to_span(zspan, v3, v4);
		zbuf_add_to_span(zspan, v4, v1);
	}
	else
		zbuf_add_to_span(zspan, v3, v1);
	
	/* clipped */
	if (zspan->minp2==NULL || zspan->maxp2==NULL) return;

	if (zspan->miny1 < zspan->miny2) my0= zspan->miny2; else my0= zspan->miny1;
	if (zspan->maxy1 > zspan->maxy2) my2= zspan->maxy2; else my2= zspan->maxy1;
	
	if (my2<my0) return;
	
	/* ZBUF DX DY, in floats still */
	x1= v1[0]- v2[0];
	x2= v2[0]- v3[0];
	y1= v1[1]- v2[1];
	y2= v2[1]- v3[1];
	z1= v1[2]- v2[2];
	z2= v2[2]- v3[2];
	x0= y1*z2-z1*y2;
	y0= z1*x2-x1*z2;
	z0= x1*y2-y1*x2;
	
	if (z0==0.0f) return;
	
	xx1= (x0*v1[0] + y0*v1[1])/z0 + v1[2];
	
	zxd= -(double)x0/(double)z0;
	zyd= -(double)y0/(double)z0;
	zy0= ((double)my2)*zyd + (double)xx1;
	
	/* start-offset in rect */
	rectx= zspan->rectx;
	rectzofs= (int *)(zspan->arectz+rectx*(my2));
	rectmaskofs= (int *)(zspan->rectmask+rectx*(my2));
	apofs= (zspan->apixbuf+ rectx*(my2));
	mask= zspan->mask;

	/* correct span */
	sn1= (my0 + my2)/2;
	if (zspan->span1[sn1] < zspan->span2[sn1]) {
		span1= zspan->span1+my2;
		span2= zspan->span2+my2;
	}
	else {
		span1= zspan->span2+my2;
		span2= zspan->span1+my2;
	}
	
	for (y=my2; y>=my0; y--, span1--, span2--) {
		
		sn1= floor(*span1);
		sn2= floor(*span2);
		sn1++; 
		
		if (sn2>=rectx) sn2= rectx-1;
		if (sn1<0) sn1= 0;
		
		if (sn2>=sn1) {
			int intzverg;
			
			zverg= (double)sn1*zxd + zy0;
			rz= rectzofs+sn1;
			rm= rectmaskofs+sn1;
			ap= apofs+sn1;
			x= sn2-sn1;
			
			zverg-= zspan->polygon_offset;
			
			while (x>=0) {
				intzverg= (int)CLAMPIS(zverg, INT_MIN, INT_MAX);

				if ( intzverg < *rz) {
					if (!zspan->rectmask || intzverg > *rm) {
						
						apn= ap;
						while (apn) {
							if (apn->p[0]==0) {apn->obi[0]= obi; apn->p[0]= zvlnr; apn->z[0]= intzverg; apn->mask[0]= mask; break; }
							if (apn->p[0]==zvlnr && apn->obi[0]==obi) {apn->mask[0]|= mask; break; }
							if (apn->p[1]==0) {apn->obi[1]= obi; apn->p[1]= zvlnr; apn->z[1]= intzverg; apn->mask[1]= mask; break; }
							if (apn->p[1]==zvlnr && apn->obi[1]==obi) {apn->mask[1]|= mask; break; }
							if (apn->p[2]==0) {apn->obi[2]= obi; apn->p[2]= zvlnr; apn->z[2]= intzverg; apn->mask[2]= mask; break; }
							if (apn->p[2]==zvlnr && apn->obi[2]==obi) {apn->mask[2]|= mask; break; }
							if (apn->p[3]==0) {apn->obi[3]= obi; apn->p[3]= zvlnr; apn->z[3]= intzverg; apn->mask[3]= mask; break; }
							if (apn->p[3]==zvlnr && apn->obi[3]==obi) {apn->mask[3]|= mask; break; }
							if (apn->next==NULL) apn->next= addpsA(zspan);
							apn= apn->next;
						}				
					}
				}
				zverg+= zxd;
				rz++; 
				rm++;
				ap++; 
				x--;
			}
		}
		
		zy0-=zyd;
		rectzofs-= rectx;
		rectmaskofs-= rectx;
		apofs-= rectx;
	}
}



static void zbuflineAc(ZSpan *zspan, int obi, int zvlnr, const float vec1[3], const float vec2[3])
{
	APixstr *ap, *apn;
	int *rectz, *rectmask;
	int start, end, x, y, oldx, oldy, ofs;
	int dz, vergz, mask, maxtest=0;
	float dx, dy;
	float v1[3], v2[3];
	
	dx= vec2[0]-vec1[0];
	dy= vec2[1]-vec1[1];
	
	mask= zspan->mask;
	
	if (fabs(dx) > fabs(dy)) {

		/* all lines from left to right */
		if (vec1[0]<vec2[0]) {
			copy_v3_v3(v1, vec1);
			copy_v3_v3(v2, vec2);
		}
		else {
			copy_v3_v3(v2, vec1);
			copy_v3_v3(v1, vec2);
			dx= -dx; dy= -dy;
		}

		start= floor(v1[0]);
		end= start+floor(dx);
		if (end>=zspan->rectx) end= zspan->rectx-1;
		
		oldy= floor(v1[1]);
		dy/= dx;
		
		vergz= v1[2];
		vergz-= zspan->polygon_offset;
		dz= (v2[2]-v1[2])/dx;
		if (vergz>0x50000000 && dz>0) maxtest= 1;  /* prevent overflow */
		
		rectz= (int *)(zspan->arectz+zspan->rectx*(oldy) +start);
		rectmask= (int *)(zspan->rectmask+zspan->rectx*(oldy) +start);
		ap= (zspan->apixbuf+ zspan->rectx*(oldy) +start);

		if (dy<0) ofs= -zspan->rectx;
		else ofs= zspan->rectx;
		
		for (x= start; x<=end; x++, rectz++, rectmask++, ap++) {
			
			y= floor(v1[1]);
			if (y!=oldy) {
				oldy= y;
				rectz+= ofs;
				rectmask+= ofs;
				ap+= ofs;
			}
			
			if (x>=0 && y>=0 && y<zspan->recty) {
				if (vergz<*rectz) {
					if (!zspan->rectmask || vergz>*rectmask) {
					
						apn= ap;
						while (apn) {	/* loop unrolled */
							if (apn->p[0]==0) {apn->obi[0]= obi; apn->p[0]= zvlnr; apn->z[0]= vergz; apn->mask[0]= mask; break; }
							if (apn->p[0]==zvlnr && apn->obi[0]==obi) {apn->mask[0]|= mask; break; }
							if (apn->p[1]==0) {apn->obi[1]= obi; apn->p[1]= zvlnr; apn->z[1]= vergz; apn->mask[1]= mask; break; }
							if (apn->p[1]==zvlnr && apn->obi[1]==obi) {apn->mask[1]|= mask; break; }
							if (apn->p[2]==0) {apn->obi[2]= obi; apn->p[2]= zvlnr; apn->z[2]= vergz; apn->mask[2]= mask; break; }
							if (apn->p[2]==zvlnr && apn->obi[2]==obi) {apn->mask[2]|= mask; break; }
							if (apn->p[3]==0) {apn->obi[3]= obi; apn->p[3]= zvlnr; apn->z[3]= vergz; apn->mask[3]= mask; break; }
							if (apn->p[3]==zvlnr && apn->obi[3]==obi) {apn->mask[3]|= mask; break; }
							if (apn->next==0) apn->next= addpsA(zspan);
							apn= apn->next;
						}				
					}
				}
			}
			
			v1[1]+= dy;
			if (maxtest && (vergz > 0x7FFFFFF0 - dz)) vergz= 0x7FFFFFF0;
			else vergz+= dz;
		}
	}
	else {
	
		/* all lines from top to bottom */
		if (vec1[1]<vec2[1]) {
			copy_v3_v3(v1, vec1);
			copy_v3_v3(v2, vec2);
		}
		else {
			copy_v3_v3(v2, vec1);
			copy_v3_v3(v1, vec2);
			dx= -dx; dy= -dy;
		}

		start= floor(v1[1]);
		end= start+floor(dy);
		
		if (start>=zspan->recty || end<0) return;
		
		if (end>=zspan->recty) end= zspan->recty-1;
		
		oldx= floor(v1[0]);
		dx/= dy;
		
		vergz= v1[2];
		vergz-= zspan->polygon_offset;
		dz= (v2[2]-v1[2])/dy;
		if (vergz>0x50000000 && dz>0) maxtest= 1;  /* prevent overflow */

		rectz= (int *)( zspan->arectz+ (start)*zspan->rectx+ oldx );
		rectmask= (int *)( zspan->rectmask+ (start)*zspan->rectx+ oldx );
		ap= (zspan->apixbuf+ zspan->rectx*(start) +oldx);
				
		if (dx<0) ofs= -1;
		else ofs= 1;

		for (y= start; y<=end; y++, rectz+=zspan->rectx, rectmask+=zspan->rectx, ap+=zspan->rectx) {
			
			x= floor(v1[0]);
			if (x!=oldx) {
				oldx= x;
				rectz+= ofs;
				rectmask+= ofs;
				ap+= ofs;
			}
			
			if (x>=0 && y>=0 && x<zspan->rectx) {
				if (vergz<*rectz) {
					if (!zspan->rectmask || vergz>*rectmask) {
						
						apn= ap;
						while (apn) {	/* loop unrolled */
							if (apn->p[0]==0) {apn->obi[0]= obi; apn->p[0]= zvlnr; apn->z[0]= vergz; apn->mask[0]= mask; break; }
							if (apn->p[0]==zvlnr) {apn->mask[0]|= mask; break; }
							if (apn->p[1]==0) {apn->obi[1]= obi; apn->p[1]= zvlnr; apn->z[1]= vergz; apn->mask[1]= mask; break; }
							if (apn->p[1]==zvlnr) {apn->mask[1]|= mask; break; }
							if (apn->p[2]==0) {apn->obi[2]= obi; apn->p[2]= zvlnr; apn->z[2]= vergz; apn->mask[2]= mask; break; }
							if (apn->p[2]==zvlnr) {apn->mask[2]|= mask; break; }
							if (apn->p[3]==0) {apn->obi[3]= obi; apn->p[3]= zvlnr; apn->z[3]= vergz; apn->mask[3]= mask; break; }
							if (apn->p[3]==zvlnr) {apn->mask[3]|= mask; break; }
							if (apn->next==0) apn->next= addpsA(zspan);
							apn= apn->next;
						}	
					}
				}
			}
			
			v1[0]+= dx;
			if (maxtest && (vergz > 0x7FFFFFF0 - dz)) vergz= 0x7FFFFFF0;
			else vergz+= dz;
		}
	}
}

/* *************  NORMAL ZBUFFER ************ */

static void zbufline(ZSpan *zspan, int obi, int zvlnr, const float vec1[3], const float vec2[3])
{
	int *rectz, *rectp, *recto, *rectmask;
	int start, end, x, y, oldx, oldy, ofs;
	int dz, vergz, maxtest= 0;
	float dx, dy;
	float v1[3], v2[3];
	
	dx= vec2[0]-vec1[0];
	dy= vec2[1]-vec1[1];
	
	if (fabs(dx) > fabs(dy)) {

		/* all lines from left to right */
		if (vec1[0]<vec2[0]) {
			copy_v3_v3(v1, vec1);
			copy_v3_v3(v2, vec2);
		}
		else {
			copy_v3_v3(v2, vec1);
			copy_v3_v3(v1, vec2);
			dx= -dx; dy= -dy;
		}

		start= floor(v1[0]);
		end= start+floor(dx);
		if (end>=zspan->rectx) end= zspan->rectx-1;
		
		oldy= floor(v1[1]);
		dy/= dx;
		
		vergz= floor(v1[2]);
		dz= floor((v2[2]-v1[2])/dx);
		if (vergz>0x50000000 && dz>0) maxtest= 1;  /* prevent overflow */
		
		rectz= zspan->rectz + oldy*zspan->rectx+ start;
		rectp= zspan->rectp + oldy*zspan->rectx+ start;
		recto= zspan->recto + oldy*zspan->rectx+ start;
		rectmask= zspan->rectmask + oldy*zspan->rectx+ start;
		
		if (dy<0) ofs= -zspan->rectx;
		else ofs= zspan->rectx;
		
		for (x= start; x<=end; x++, rectz++, rectp++, recto++, rectmask++) {
			
			y= floor(v1[1]);
			if (y!=oldy) {
				oldy= y;
				rectz+= ofs;
				rectp+= ofs;
				recto+= ofs;
				rectmask+= ofs;
			}
			
			if (x>=0 && y>=0 && y<zspan->recty) {
				if (vergz<*rectz) {
					if (!zspan->rectmask || vergz>*rectmask) {
						*recto= obi;
						*rectz= vergz;
						*rectp= zvlnr;
					}
				}
			}
			
			v1[1]+= dy;
			
			if (maxtest && (vergz > 0x7FFFFFF0 - dz)) vergz= 0x7FFFFFF0;
			else vergz+= dz;
		}
	}
	else {
		/* all lines from top to bottom */
		if (vec1[1]<vec2[1]) {
			copy_v3_v3(v1, vec1);
			copy_v3_v3(v2, vec2);
		}
		else {
			copy_v3_v3(v2, vec1);
			copy_v3_v3(v1, vec2);
			dx= -dx; dy= -dy;
		}

		start= floor(v1[1]);
		end= start+floor(dy);
		
		if (end>=zspan->recty) end= zspan->recty-1;
		
		oldx= floor(v1[0]);
		dx/= dy;
		
		vergz= floor(v1[2]);
		dz= floor((v2[2]-v1[2])/dy);
		if (vergz>0x50000000 && dz>0) maxtest= 1;  /* prevent overflow */
		
		rectz= zspan->rectz + start*zspan->rectx+ oldx;
		rectp= zspan->rectp + start*zspan->rectx+ oldx;
		recto= zspan->recto + start*zspan->rectx+ oldx;
		rectmask= zspan->rectmask + start*zspan->rectx+ oldx;
		
		if (dx<0) ofs= -1;
		else ofs= 1;

		for (y= start; y<=end; y++, rectz+=zspan->rectx, rectp+=zspan->rectx, recto+=zspan->rectx, rectmask+=zspan->rectx) {
			
			x= floor(v1[0]);
			if (x!=oldx) {
				oldx= x;
				rectz+= ofs;
				rectp+= ofs;
				recto+= ofs;
				rectmask+= ofs;
			}
			
			if (x>=0 && y>=0 && x<zspan->rectx) {
				if (vergz<*rectz) {
					if (!zspan->rectmask || vergz>*rectmask) {
						*rectz= vergz;
						*rectp= zvlnr;
						*recto= obi;
					}
				}
			}
			
			v1[0]+= dx;
			if (maxtest && (vergz > 0x7FFFFFF0 - dz)) vergz= 0x7FFFFFF0;
			else vergz+= dz;
		}
	}
}

static void zbufline_onlyZ(ZSpan *zspan, int UNUSED(obi), int UNUSED(zvlnr), const float vec1[3], const float vec2[3])
{
	int *rectz, *rectz1= NULL;
	int start, end, x, y, oldx, oldy, ofs;
	int dz, vergz, maxtest= 0;
	float dx, dy;
	float v1[3], v2[3];
	
	dx= vec2[0]-vec1[0];
	dy= vec2[1]-vec1[1];
	
	if (fabs(dx) > fabs(dy)) {
		
		/* all lines from left to right */
		if (vec1[0]<vec2[0]) {
			copy_v3_v3(v1, vec1);
			copy_v3_v3(v2, vec2);
		}
		else {
			copy_v3_v3(v2, vec1);
			copy_v3_v3(v1, vec2);
			dx= -dx; dy= -dy;
		}
		
		start= floor(v1[0]);
		end= start+floor(dx);
		if (end>=zspan->rectx) end= zspan->rectx-1;
		
		oldy= floor(v1[1]);
		dy/= dx;
		
		vergz= floor(v1[2]);
		dz= floor((v2[2]-v1[2])/dx);
		if (vergz>0x50000000 && dz>0) maxtest= 1;  /* prevent overflow */
		
		rectz= zspan->rectz + oldy*zspan->rectx+ start;
		if (zspan->rectz1)
			rectz1= zspan->rectz1 + oldy*zspan->rectx+ start;
		
		if (dy<0) ofs= -zspan->rectx;
		else ofs= zspan->rectx;
		
		for (x= start; x<=end; x++, rectz++) {
			
			y= floor(v1[1]);
			if (y!=oldy) {
				oldy= y;
				rectz+= ofs;
				if (rectz1) rectz1+= ofs;
			}
			
			if (x>=0 && y>=0 && y<zspan->recty) {
				if (vergz < *rectz) {
					if (rectz1) *rectz1= *rectz;
					*rectz= vergz;
				}
				else if (rectz1 && vergz < *rectz1)
					*rectz1= vergz;
			}
			
			v1[1]+= dy;
			
			if (maxtest && (vergz > 0x7FFFFFF0 - dz)) vergz= 0x7FFFFFF0;
			else vergz+= dz;
			
			if (rectz1) rectz1++;
		}
	}
	else {
		/* all lines from top to bottom */
		if (vec1[1]<vec2[1]) {
			copy_v3_v3(v1, vec1);
			copy_v3_v3(v2, vec2);
		}
		else {
			copy_v3_v3(v2, vec1);
			copy_v3_v3(v1, vec2);
			dx= -dx; dy= -dy;
		}
		
		start= floor(v1[1]);
		end= start+floor(dy);
		
		if (end>=zspan->recty) end= zspan->recty-1;
		
		oldx= floor(v1[0]);
		dx/= dy;
		
		vergz= floor(v1[2]);
		dz= floor((v2[2]-v1[2])/dy);
		if (vergz>0x50000000 && dz>0) maxtest= 1;  /* prevent overflow */

		rectz= zspan->rectz + start*zspan->rectx+ oldx;
		if (zspan->rectz1)
			rectz1= zspan->rectz1 + start*zspan->rectx+ oldx;
		
		if (dx<0) ofs= -1;
		else ofs= 1;
		
		for (y= start; y<=end; y++, rectz+=zspan->rectx) {
			
			x= floor(v1[0]);
			if (x!=oldx) {
				oldx= x;
				rectz+= ofs;
				if (rectz1) rectz1+= ofs;
			}
			
			if (x>=0 && y>=0 && x<zspan->rectx) {
				if (vergz < *rectz) {
					if (rectz1) *rectz1= *rectz;
					*rectz= vergz;
				}
				else if (rectz1 && vergz < *rectz1)
					*rectz1= vergz;
			}
			
			v1[0]+= dx;
			if (maxtest && (vergz > 0x7FFFFFF0 - dz)) vergz= 0x7FFFFFF0;
			else vergz+= dz;
			
			if (rectz1)
				rectz1+=zspan->rectx;
		}
	}
}


static int clipline(float v1[4], float v2[4])	/* return 0: do not draw */
{
	float dz, dw, u1=0.0, u2=1.0;
	float dx, dy, v13;
	
	dz= v2[2]-v1[2];
	dw= v2[3]-v1[3];
	
	/* this 1.01 is for clipping x and y just a tinsy larger. that way it is
	 * filled in with zbufwire correctly when rendering in parts. otherwise
	 * you see line endings at edges... */
	
	if (cliptestf(-dz, -dw, v1[3], v1[2], &u1, &u2)) {
		if (cliptestf(dz, -dw, v1[3], -v1[2], &u1, &u2)) {
			
			dx= v2[0]-v1[0];
			dz= 1.01f*(v2[3]-v1[3]);
			v13= 1.01f*v1[3];
			
			if (cliptestf(-dx, -dz, v1[0], v13, &u1, &u2)) {
				if (cliptestf(dx, -dz, v13, -v1[0], &u1, &u2)) {
					
					dy= v2[1]-v1[1];
					
					if (cliptestf(-dy, -dz, v1[1], v13, &u1, &u2)) {
						if (cliptestf(dy, -dz, v13, -v1[1], &u1, &u2)) {
							
							if (u2<1.0f) {
								v2[0]= v1[0]+u2*dx;
								v2[1]= v1[1]+u2*dy;
								v2[2]= v1[2]+u2*dz;
								v2[3]= v1[3]+u2*dw;
							}
							if (u1>0.0f) {
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

void hoco_to_zco(ZSpan *zspan, float zco[3], const float hoco[4])
{
	float div;
	
	div= 1.0f/hoco[3];
	zco[0]= zspan->zmulx*(1.0f+hoco[0]*div) + zspan->zofsx;
	zco[1]= zspan->zmuly*(1.0f+hoco[1]*div) + zspan->zofsy;
	zco[2]= 0x7FFFFFFF *(hoco[2]*div);
}

void zbufclipwire(ZSpan *zspan, int obi, int zvlnr, int ec, float *ho1, float *ho2, float *ho3, float *ho4, int c1, int c2, int c3, int c4)
{
	float vez[20];
	int and, or;

	/* edgecode: 1= draw */
	if (ec==0) return;

	if (ho4) {
		and= (c1 & c2 & c3 & c4);
		or= (c1 | c2 | c3 | c4);
	}
	else {
		and= (c1 & c2 & c3);
		or= (c1 | c2 | c3);
	}
	
	if (or) {	/* not in the middle */
		if (and) {	/* out completely */
			return;
		}
		else {	/* clipping */

			if (ec & ME_V1V2) {
				copy_v4_v4(vez, ho1);
				copy_v4_v4(vez+4, ho2);
				if ( clipline(vez, vez+4)) {
					hoco_to_zco(zspan, vez, vez);
					hoco_to_zco(zspan, vez+4, vez+4);
					zspan->zbuflinefunc(zspan, obi, zvlnr, vez, vez+4);
				}
			}
			if (ec & ME_V2V3) {
				copy_v4_v4(vez, ho2);
				copy_v4_v4(vez+4, ho3);
				if ( clipline(vez, vez+4)) {
					hoco_to_zco(zspan, vez, vez);
					hoco_to_zco(zspan, vez+4, vez+4);
					zspan->zbuflinefunc(zspan, obi, zvlnr, vez, vez+4);
				}
			}
			if (ho4) {
				if (ec & ME_V3V4) {
					copy_v4_v4(vez, ho3);
					copy_v4_v4(vez+4, ho4);
					if ( clipline(vez, vez+4)) {
						hoco_to_zco(zspan, vez, vez);
						hoco_to_zco(zspan, vez+4, vez+4);
						zspan->zbuflinefunc(zspan, obi, zvlnr, vez, vez+4);
					}
				}
				if (ec & ME_V4V1) {
					copy_v4_v4(vez, ho4);
					copy_v4_v4(vez+4, ho1);
					if ( clipline(vez, vez+4)) {
						hoco_to_zco(zspan, vez, vez);
						hoco_to_zco(zspan, vez+4, vez+4);
						zspan->zbuflinefunc(zspan, obi, zvlnr, vez, vez+4);
					}
				}
			}
			else {
				if (ec & ME_V3V1) {
					copy_v4_v4(vez, ho3);
					copy_v4_v4(vez+4, ho1);
					if ( clipline(vez, vez+4)) {
						hoco_to_zco(zspan, vez, vez);
						hoco_to_zco(zspan, vez+4, vez+4);
						zspan->zbuflinefunc(zspan, obi, zvlnr, vez, vez+4);
					}
				}
			}
			
			return;
		}
	}

	hoco_to_zco(zspan, vez, ho1);
	hoco_to_zco(zspan, vez+4, ho2);
	hoco_to_zco(zspan, vez+8, ho3);
	if (ho4) {
		hoco_to_zco(zspan, vez+12, ho4);

		if (ec & ME_V3V4)  zspan->zbuflinefunc(zspan, obi, zvlnr, vez+8, vez+12);
		if (ec & ME_V4V1)  zspan->zbuflinefunc(zspan, obi, zvlnr, vez+12, vez);
	}
	else {
		if (ec & ME_V3V1)  zspan->zbuflinefunc(zspan, obi, zvlnr, vez+8, vez);
	}

	if (ec & ME_V1V2)  zspan->zbuflinefunc(zspan, obi, zvlnr, vez, vez+4);
	if (ec & ME_V2V3)  zspan->zbuflinefunc(zspan, obi, zvlnr, vez+4, vez+8);

}

void zbufsinglewire(ZSpan *zspan, int obi, int zvlnr, const float ho1[4], const float ho2[4])
{
	float f1[4], f2[4];
	int c1, c2;

	c1= testclip(ho1);
	c2= testclip(ho2);

	if (c1 | c2) {	/* not in the middle */
		if (!(c1 & c2)) {	/* not out completely */
			copy_v4_v4(f1, ho1);
			copy_v4_v4(f2, ho2);

			if (clipline(f1, f2)) {
				hoco_to_zco(zspan, f1, f1);
				hoco_to_zco(zspan, f2, f2);
				zspan->zbuflinefunc(zspan, obi, zvlnr, f1, f2);
			}
		}
	}
	else {
		hoco_to_zco(zspan, f1, ho1);
		hoco_to_zco(zspan, f2, ho2);

		zspan->zbuflinefunc(zspan, obi, zvlnr, f1, f2);
	}
}

/**
 * Fill the z buffer, but invert z order, and add the face index to
 * the corresponing face buffer.
 *
 * This is one of the z buffer fill functions called in zbufclip() and
 * zbufwireclip(). 
 *
 * \param v1 [4 floats, world coordinates] first vertex
 * \param v2 [4 floats, world coordinates] second vertex
 * \param v3 [4 floats, world coordinates] third vertex
 */

/* WATCH IT: zbuffillGLinv4 and zbuffillGL4 are identical except for a 2 lines,
 * commented below */
static void zbuffillGLinv4(ZSpan *zspan, int obi, int zvlnr,
                           const float *v1, const float *v2, const float *v3, const float *v4)
{
	double zxd, zyd, zy0, zverg;
	float x0, y0, z0;
	float x1, y1, z1, x2, y2, z2, xx1;
	float *span1, *span2;
	int *rectoofs, *ro;
	int *rectpofs, *rp;
	int *rectmaskofs, *rm;
	int *rz, x, y;
	int sn1, sn2, rectx, *rectzofs, my0, my2;

	/* init */
	zbuf_init_span(zspan);

	/* set spans */
	zbuf_add_to_span(zspan, v1, v2);
	zbuf_add_to_span(zspan, v2, v3);
	if (v4) {
		zbuf_add_to_span(zspan, v3, v4);
		zbuf_add_to_span(zspan, v4, v1);
	}
	else
		zbuf_add_to_span(zspan, v3, v1);

	/* clipped */
	if (zspan->minp2==NULL || zspan->maxp2==NULL) return;

	if (zspan->miny1 < zspan->miny2) my0= zspan->miny2; else my0= zspan->miny1;
	if (zspan->maxy1 > zspan->maxy2) my2= zspan->maxy2; else my2= zspan->maxy1;

	//	printf("my %d %d\n", my0, my2);
	if (my2<my0) return;


	/* ZBUF DX DY, in floats still */
	x1= v1[0]- v2[0];
	x2= v2[0]- v3[0];
	y1= v1[1]- v2[1];
	y2= v2[1]- v3[1];
	z1= v1[2]- v2[2];
	z2= v2[2]- v3[2];
	x0= y1*z2-z1*y2;
	y0= z1*x2-x1*z2;
	z0= x1*y2-y1*x2;

	if (z0==0.0f) return;

	xx1= (x0*v1[0] + y0*v1[1])/z0 + v1[2];

	zxd= -(double)x0/(double)z0;
	zyd= -(double)y0/(double)z0;
	zy0= ((double)my2)*zyd + (double)xx1;

	/* start-offset in rect */
	rectx= zspan->rectx;
	rectzofs= (zspan->rectz+rectx*my2);
	rectpofs= (zspan->rectp+rectx*my2);
	rectoofs= (zspan->recto+rectx*my2);
	rectmaskofs= (zspan->rectmask+rectx*my2);

	/* correct span */
	sn1= (my0 + my2)/2;
	if (zspan->span1[sn1] < zspan->span2[sn1]) {
		span1= zspan->span1+my2;
		span2= zspan->span2+my2;
	}
	else {
		span1= zspan->span2+my2;
		span2= zspan->span1+my2;
	}

	for (y=my2; y>=my0; y--, span1--, span2--) {

		sn1= floor(*span1);
		sn2= floor(*span2);
		sn1++;

		if (sn2>=rectx) sn2= rectx-1;
		if (sn1<0) sn1= 0;

		if (sn2>=sn1) {
			int intzverg;

			zverg= (double)sn1*zxd + zy0;
			rz= rectzofs+sn1;
			rp= rectpofs+sn1;
			ro= rectoofs+sn1;
			rm= rectmaskofs+sn1;
			x= sn2-sn1;

			while (x>=0) {
				intzverg= (int)CLAMPIS(zverg, INT_MIN, INT_MAX);

				if ( intzverg > *rz || *rz==0x7FFFFFFF) { /* UNIQUE LINE: see comment above */
					if (!zspan->rectmask || intzverg > *rm) {
						*ro= obi; /* UNIQUE LINE: see comment above (order differs) */
						*rz= intzverg;
						*rp= zvlnr;
					}
				}
				zverg+= zxd;
				rz++;
				rp++;
				ro++;
				rm++;
				x--;
			}
		}

		zy0-=zyd;
		rectzofs-= rectx;
		rectpofs-= rectx;
		rectoofs-= rectx;
		rectmaskofs-= rectx;
	}
}

/* uses spanbuffers */

/* WATCH IT: zbuffillGLinv4 and zbuffillGL4 are identical except for a 2 lines,
 * commented below */
static void zbuffillGL4(ZSpan *zspan, int obi, int zvlnr,
                        const float *v1, const float *v2, const float *v3, const float *v4)
{
	double zxd, zyd, zy0, zverg;
	float x0, y0, z0;
	float x1, y1, z1, x2, y2, z2, xx1;
	float *span1, *span2;
	int *rectoofs, *ro;
	int *rectpofs, *rp;
	int *rectmaskofs, *rm;
	int *rz, x, y;
	int sn1, sn2, rectx, *rectzofs, my0, my2;

	/* init */
	zbuf_init_span(zspan);

	/* set spans */
	zbuf_add_to_span(zspan, v1, v2);
	zbuf_add_to_span(zspan, v2, v3);
	if (v4) {
		zbuf_add_to_span(zspan, v3, v4);
		zbuf_add_to_span(zspan, v4, v1);
	}
	else
		zbuf_add_to_span(zspan, v3, v1);

	/* clipped */
	if (zspan->minp2==NULL || zspan->maxp2==NULL) return;

	if (zspan->miny1 < zspan->miny2) my0= zspan->miny2; else my0= zspan->miny1;
	if (zspan->maxy1 > zspan->maxy2) my2= zspan->maxy2; else my2= zspan->maxy1;

	//	printf("my %d %d\n", my0, my2);
	if (my2<my0) return;


	/* ZBUF DX DY, in floats still */
	x1= v1[0]- v2[0];
	x2= v2[0]- v3[0];
	y1= v1[1]- v2[1];
	y2= v2[1]- v3[1];
	z1= v1[2]- v2[2];
	z2= v2[2]- v3[2];
	x0= y1*z2-z1*y2;
	y0= z1*x2-x1*z2;
	z0= x1*y2-y1*x2;

	if (z0==0.0f) return;

	xx1= (x0*v1[0] + y0*v1[1])/z0 + v1[2];

	zxd= -(double)x0/(double)z0;
	zyd= -(double)y0/(double)z0;
	zy0= ((double)my2)*zyd + (double)xx1;

	/* start-offset in rect */
	rectx= zspan->rectx;
	rectzofs= (zspan->rectz+rectx*my2);
	rectpofs= (zspan->rectp+rectx*my2);
	rectoofs= (zspan->recto+rectx*my2);
	rectmaskofs= (zspan->rectmask+rectx*my2);

	/* correct span */
	sn1= (my0 + my2)/2;
	if (zspan->span1[sn1] < zspan->span2[sn1]) {
		span1= zspan->span1+my2;
		span2= zspan->span2+my2;
	}
	else {
		span1= zspan->span2+my2;
		span2= zspan->span1+my2;
	}

	for (y=my2; y>=my0; y--, span1--, span2--) {

		sn1= floor(*span1);
		sn2= floor(*span2);
		sn1++;

		if (sn2>=rectx) sn2= rectx-1;
		if (sn1<0) sn1= 0;

		if (sn2>=sn1) {
			int intzverg;

			zverg= (double)sn1*zxd + zy0;
			rz= rectzofs+sn1;
			rp= rectpofs+sn1;
			ro= rectoofs+sn1;
			rm= rectmaskofs+sn1;
			x= sn2-sn1;

			while (x>=0) {
				intzverg= (int)CLAMPIS(zverg, INT_MIN, INT_MAX);

				if (intzverg < *rz) { /* ONLY UNIQUE LINE: see comment above */
					if (!zspan->rectmask || intzverg > *rm) {
						*rz= intzverg;
						*rp= zvlnr;
						*ro= obi; /* UNIQUE LINE: see comment above (order differs) */
					}
				}
				zverg+= zxd;
				rz++;
				rp++;
				ro++;
				rm++;
				x--;
			}
		}

		zy0-=zyd;
		rectzofs-= rectx;
		rectpofs-= rectx;
		rectoofs-= rectx;
		rectmaskofs-= rectx;
	}
}

/**
 * Fill the z buffer. The face buffer is not operated on!
 *
 * This is one of the z buffer fill functions called in zbufclip() and
 * zbufwireclip(). 
 *
 * \param v1 [4 floats, world coordinates] first vertex
 * \param v2 [4 floats, world coordinates] second vertex
 * \param v3 [4 floats, world coordinates] third vertex
 */

/* now: filling two Z values, the closest and 2nd closest */
static void zbuffillGL_onlyZ(ZSpan *zspan, int UNUSED(obi), int UNUSED(zvlnr),
                             const float *v1, const float *v2, const float *v3, const float *v4)
{
	double zxd, zyd, zy0, zverg;
	float x0, y0, z0;
	float x1, y1, z1, x2, y2, z2, xx1;
	float *span1, *span2;
	int *rz, *rz1, x, y;
	int sn1, sn2, rectx, *rectzofs, *rectzofs1= NULL, my0, my2;
	
	/* init */
	zbuf_init_span(zspan);
	
	/* set spans */
	zbuf_add_to_span(zspan, v1, v2);
	zbuf_add_to_span(zspan, v2, v3);
	if (v4) {
		zbuf_add_to_span(zspan, v3, v4);
		zbuf_add_to_span(zspan, v4, v1);
	}
	else 
		zbuf_add_to_span(zspan, v3, v1);
	
	/* clipped */
	if (zspan->minp2==NULL || zspan->maxp2==NULL) return;
	
	if (zspan->miny1 < zspan->miny2) my0= zspan->miny2; else my0= zspan->miny1;
	if (zspan->maxy1 > zspan->maxy2) my2= zspan->maxy2; else my2= zspan->maxy1;
	
	//	printf("my %d %d\n", my0, my2);
	if (my2<my0) return;
	
	
	/* ZBUF DX DY, in floats still */
	x1= v1[0]- v2[0];
	x2= v2[0]- v3[0];
	y1= v1[1]- v2[1];
	y2= v2[1]- v3[1];
	z1= v1[2]- v2[2];
	z2= v2[2]- v3[2];
	x0= y1*z2-z1*y2;
	y0= z1*x2-x1*z2;
	z0= x1*y2-y1*x2;
	
	if (z0==0.0f) return;
	
	xx1= (x0*v1[0] + y0*v1[1])/z0 + v1[2];
	
	zxd= -(double)x0/(double)z0;
	zyd= -(double)y0/(double)z0;
	zy0= ((double)my2)*zyd + (double)xx1;
	
	/* start-offset in rect */
	rectx= zspan->rectx;
	rectzofs= (zspan->rectz+rectx*my2);
	if (zspan->rectz1)
		rectzofs1= (zspan->rectz1+rectx*my2);
	
	/* correct span */
	sn1= (my0 + my2)/2;
	if (zspan->span1[sn1] < zspan->span2[sn1]) {
		span1= zspan->span1+my2;
		span2= zspan->span2+my2;
	}
	else {
		span1= zspan->span2+my2;
		span2= zspan->span1+my2;
	}
	
	for (y=my2; y>=my0; y--, span1--, span2--) {
		
		sn1= floor(*span1);
		sn2= floor(*span2);
		sn1++; 
		
		if (sn2>=rectx) sn2= rectx-1;
		if (sn1<0) sn1= 0;
		
		if (sn2>=sn1) {
			zverg= (double)sn1*zxd + zy0;
			rz= rectzofs+sn1;
			rz1= rectzofs1+sn1;
			x= sn2-sn1;
			
			while (x>=0) {
				int zvergi= (int)CLAMPIS(zverg, INT_MIN, INT_MAX);

				/* option: maintain two depth values, closest and 2nd closest */
				if (zvergi < *rz) {
					if (rectzofs1) *rz1= *rz;
					*rz= zvergi;
				}
				else if (rectzofs1 && zvergi < *rz1)
					*rz1= zvergi;

				zverg+= zxd;
				
				rz++; 
				rz1++;
				x--;
			}
		}
		
		zy0-=zyd;
		rectzofs-= rectx;
		if (rectzofs1) rectzofs1-= rectx;
	}
}

/* 2d scanconvert for tria, calls func for each x, y coordinate and gives UV barycentrics */
void zspan_scanconvert_strand(ZSpan *zspan, void *handle, float *v1, float *v2, float *v3, void (*func)(void *, int, int, float, float, float) )
{
	float x0, y0, x1, y1, x2, y2, z0, z1, z2, z;
	float u, v, uxd, uyd, vxd, vyd, uy0, vy0, zxd, zyd, zy0, xx1;
	float *span1, *span2;
	int x, y, sn1, sn2, rectx= zspan->rectx, my0, my2;
	
	/* init */
	zbuf_init_span(zspan);
	
	/* set spans */
	zbuf_add_to_span(zspan, v1, v2);
	zbuf_add_to_span(zspan, v2, v3);
	zbuf_add_to_span(zspan, v3, v1);
	
	/* clipped */
	if (zspan->minp2==NULL || zspan->maxp2==NULL) return;
	
	if (zspan->miny1 < zspan->miny2) my0= zspan->miny2; else my0= zspan->miny1;
	if (zspan->maxy1 > zspan->maxy2) my2= zspan->maxy2; else my2= zspan->maxy1;
	
	//	printf("my %d %d\n", my0, my2);
	if (my2<my0) return;
	
	/* ZBUF DX DY, in floats still */
	x1= v1[0]- v2[0];
	x2= v2[0]- v3[0];
	y1= v1[1]- v2[1];
	y2= v2[1]- v3[1];
	z1= v1[2]- v2[2];
	z2= v2[2]- v3[2];

	x0= y1*z2-z1*y2;
	y0= z1*x2-x1*z2;
	z0= x1*y2-y1*x2;
	
	if (z0==0.0f) return;

	xx1= (x0*v1[0] + y0*v1[1])/z0 + v1[2];
	zxd= -(double)x0/(double)z0;
	zyd= -(double)y0/(double)z0;
	zy0= ((double)my2)*zyd + (double)xx1;

	z1= 1.0f; /* (u1 - u2) */
	z2= 0.0f; /* (u2 - u3) */

	x0= y1*z2-z1*y2;
	y0= z1*x2-x1*z2;

	xx1= (x0*v1[0] + y0*v1[1])/z0 + 1.0f;
	uxd= -(double)x0/(double)z0;
	uyd= -(double)y0/(double)z0;
	uy0= ((double)my2)*uyd + (double)xx1;

	z1= -1.0f; /* (v1 - v2) */
	z2= 1.0f;  /* (v2 - v3) */

	x0= y1*z2-z1*y2;
	y0= z1*x2-x1*z2;
	
	xx1= (x0*v1[0] + y0*v1[1])/z0;
	vxd= -(double)x0/(double)z0;
	vyd= -(double)y0/(double)z0;
	vy0= ((double)my2)*vyd + (double)xx1;
	
	/* correct span */
	sn1= (my0 + my2)/2;
	if (zspan->span1[sn1] < zspan->span2[sn1]) {
		span1= zspan->span1+my2;
		span2= zspan->span2+my2;
	}
	else {
		span1= zspan->span2+my2;
		span2= zspan->span1+my2;
	}
	
	for (y=my2; y>=my0; y--, span1--, span2--) {
		
		sn1= floor(*span1);
		sn2= floor(*span2);
		sn1++; 
		
		if (sn2>=rectx) sn2= rectx-1;
		if (sn1<0) sn1= 0;
		
		u= (double)sn1*uxd + uy0;
		v= (double)sn1*vxd + vy0;
		z= (double)sn1*zxd + zy0;
		
		for (x= sn1; x<=sn2; x++, u+=uxd, v+=vxd, z+=zxd)
			func(handle, x, y, u, v, z);
		
		uy0 -= uyd;
		vy0 -= vyd;
		zy0 -= zyd;
	}
}

/* scanconvert for strand triangles, calls func for each x, y coordinate and gives UV barycentrics and z */

void zspan_scanconvert(ZSpan *zspan, void *handle, float *v1, float *v2, float *v3, void (*func)(void *, int, int, float, float) )
{
	float x0, y0, x1, y1, x2, y2, z0, z1, z2;
	float u, v, uxd, uyd, vxd, vyd, uy0, vy0, xx1;
	float *span1, *span2;
	int x, y, sn1, sn2, rectx= zspan->rectx, my0, my2;
	
	/* init */
	zbuf_init_span(zspan);
	
	/* set spans */
	zbuf_add_to_span(zspan, v1, v2);
	zbuf_add_to_span(zspan, v2, v3);
	zbuf_add_to_span(zspan, v3, v1);
	
	/* clipped */
	if (zspan->minp2==NULL || zspan->maxp2==NULL) return;
	
	if (zspan->miny1 < zspan->miny2) my0= zspan->miny2; else my0= zspan->miny1;
	if (zspan->maxy1 > zspan->maxy2) my2= zspan->maxy2; else my2= zspan->maxy1;
	
	//	printf("my %d %d\n", my0, my2);
	if (my2<my0) return;
	
	/* ZBUF DX DY, in floats still */
	x1= v1[0]- v2[0];
	x2= v2[0]- v3[0];
	y1= v1[1]- v2[1];
	y2= v2[1]- v3[1];
	
	z1= 1.0f; /* (u1 - u2) */
	z2= 0.0f; /* (u2 - u3) */

	x0= y1*z2-z1*y2;
	y0= z1*x2-x1*z2;
	z0= x1*y2-y1*x2;

	if (z0==0.0f) return;

	xx1= (x0*v1[0] + y0*v1[1])/z0 + 1.0f;
	uxd= -(double)x0/(double)z0;
	uyd= -(double)y0/(double)z0;
	uy0= ((double)my2)*uyd + (double)xx1;

	z1= -1.0f; /* (v1 - v2) */
	z2= 1.0f;  /* (v2 - v3) */

	x0= y1*z2-z1*y2;
	y0= z1*x2-x1*z2;
	
	xx1= (x0*v1[0] + y0*v1[1])/z0;
	vxd= -(double)x0/(double)z0;
	vyd= -(double)y0/(double)z0;
	vy0= ((double)my2)*vyd + (double)xx1;
	
	/* correct span */
	sn1= (my0 + my2)/2;
	if (zspan->span1[sn1] < zspan->span2[sn1]) {
		span1= zspan->span1+my2;
		span2= zspan->span2+my2;
	}
	else {
		span1= zspan->span2+my2;
		span2= zspan->span1+my2;
	}
	
	for (y=my2; y>=my0; y--, span1--, span2--) {
		
		sn1= floor(*span1);
		sn2= floor(*span2);
		sn1++; 
		
		if (sn2>=rectx) sn2= rectx-1;
		if (sn1<0) sn1= 0;
		
		u= (double)sn1*uxd + uy0;
		v= (double)sn1*vxd + vy0;
		
		for (x= sn1; x<=sn2; x++, u+=uxd, v+=vxd)
			func(handle, x, y, u, v);
		
		uy0 -= uyd;
		vy0 -= vyd;
	}
}



/**
 * (clip pyramid)
 * Sets labda: flag, and parametrize the clipping of vertices in
 * viewspace coordinates. labda = -1 means no clipping, labda in [0, 1] means a clipping.
 * Note: uses globals.
 * \param v1 start coordinate s
 * \param v2 target coordinate t
 * \param b1 
 * \param b2 
 * \param b3
 * \param a index for coordinate (x, y, or z)
 */

static void clippyra(float *labda, float *v1, float *v2, int *b2, int *b3, int a, float clipcrop)
{
	float da, dw, u1=0.0, u2=1.0;
	float v13;
	
	labda[0]= -1.0;
	labda[1]= -1.0;

	da= v2[a]-v1[a];
	/* prob; we clip slightly larger, osa renders add 2 pixels on edges, should become variable? */
	/* or better; increase r.winx/y size, but thats quite a complex one. do it later */
	if (a==2) {
		dw= (v2[3]-v1[3]);
		v13= v1[3];
	}
	else {
		dw= clipcrop*(v2[3]-v1[3]);
		v13= clipcrop*v1[3];
	}	
	/* according the original article by Liang&Barsky, for clipping of
	 * homogeneous coordinates with viewplane, the value of "0" is used instead of "-w" .
	 * This differs from the other clipping cases (like left or top) and I considered
	 * it to be not so 'homogenic'. But later it has proven to be an error,
	 * who would have thought that of L&B!
	 */

	if (cliptestf(-da, -dw, v13, v1[a], &u1, &u2)) {
		if (cliptestf(da, -dw, v13, -v1[a], &u1, &u2)) {
			*b3=1;
			if (u2<1.0f) {
				labda[1]= u2;
				*b2=1;
			}
			else labda[1]=1.0;  /* u2 */
			if (u1>0.0f) {
				labda[0] = u1;
				*b2 = 1;
			}
			else {
				labda[0] = 0.0;
			}
		}
	}
}

/**
 * (make vertex pyramide clip)
 * Checks labda and uses this to make decision about clipping the line
 * segment from v1 to v2. labda is the factor by which the vector is
 * cut. ( calculate s + l * ( t - s )). The result is appended to the
 * vertex list of this face.
 * 
 * 
 * \param v1 start coordinate s
 * \param v2 target coordinate t
 * \param b1 
 * \param b2 
 * \param clve vertex vector.
 */

static void makevertpyra(float *vez, float *labda, float **trias, float *v1, float *v2, int *b1, int *clve)
{
	float l1, l2, *adr;

	l1= labda[0];
	l2= labda[1];

	if (l1!= -1.0f) {
		if (l1!= 0.0f) {
			adr= vez+4*(*clve);
			trias[*b1]=adr;
			(*clve)++;
			adr[0]= v1[0]+l1*(v2[0]-v1[0]);
			adr[1]= v1[1]+l1*(v2[1]-v1[1]);
			adr[2]= v1[2]+l1*(v2[2]-v1[2]);
			adr[3]= v1[3]+l1*(v2[3]-v1[3]);
		} 
		else trias[*b1]= v1;
		
		(*b1)++;
	}
	if (l2!= -1.0f) {
		if (l2!= 1.0f) {
			adr= vez+4*(*clve);
			trias[*b1]=adr;
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

void projectverto(const float v1[3], float winmat[][4], float adr[4])
{
	/* calcs homogenic coord of vertex v1 */
	float x, y, z;

	x = v1[0];
	y = v1[1];
	z = v1[2];
	adr[0] = x * winmat[0][0] + z * winmat[2][0] + winmat[3][0];
	adr[1] = y * winmat[1][1] + z * winmat[2][1] + winmat[3][1];
	adr[2] =                    z * winmat[2][2] + winmat[3][2];
	adr[3] =                    z * winmat[2][3] + winmat[3][3];

	//printf("hoco %f %f %f %f\n", adr[0], adr[1], adr[2], adr[3]);
}

/* ------------------------------------------------------------------------- */

void projectvert(const float v1[3], float winmat[][4], float adr[4])
{
	/* calcs homogenic coord of vertex v1 */
	float x, y, z;

	x = v1[0];
	y = v1[1];
	z = v1[2];
	adr[0] = x * winmat[0][0] + y * winmat[1][0] + z * winmat[2][0] + winmat[3][0];
	adr[1] = x * winmat[0][1] + y * winmat[1][1] + z * winmat[2][1] + winmat[3][1];
	adr[2] = x * winmat[0][2] + y * winmat[1][2] + z * winmat[2][2] + winmat[3][2];
	adr[3] = x * winmat[0][3] + y * winmat[1][3] + z * winmat[2][3] + winmat[3][3];
}

/* ------------------------------------------------------------------------- */

#define ZBUF_PROJECT_CACHE_SIZE 256

typedef struct ZbufProjectCache {
	int index, clip;
	float ho[4];
} ZbufProjectCache;

static void zbuf_project_cache_clear(ZbufProjectCache *cache, int size)
{
	int i;

	if (size > ZBUF_PROJECT_CACHE_SIZE)
		size= ZBUF_PROJECT_CACHE_SIZE;

	memset(cache, 0, sizeof(ZbufProjectCache)*size);
	for (i=0; i<size; i++)
		cache[i].index= -1;
}

static int zbuf_shadow_project(ZbufProjectCache *cache, int index, float winmat[][4], float *co, float *ho)
{
	int cindex= index & 255;

	if (cache[cindex].index == index) {
		copy_v4_v4(ho, cache[cindex].ho);
		return cache[cindex].clip;
	}
	else {
		int clipflag;
		projectvert(co, winmat, ho);
		clipflag= testclip(ho);

		copy_v4_v4(cache[cindex].ho, ho);
		cache[cindex].clip= clipflag;
		cache[cindex].index= index;

		return clipflag;
	}
}

static void zbuffer_part_bounds(int winx, int winy, RenderPart *pa, float *bounds)
{
	bounds[0]= (2*pa->disprect.xmin - winx-1)/(float)winx;
	bounds[1]= (2*pa->disprect.xmax - winx+1)/(float)winx;
	bounds[2]= (2*pa->disprect.ymin - winy-1)/(float)winy;
	bounds[3]= (2*pa->disprect.ymax - winy+1)/(float)winy;
}

static int zbuf_part_project(ZbufProjectCache *cache, int index, float winmat[][4], float *bounds, float *co, float *ho)
{
	float vec[3];
	int cindex= index & 255;

	if (cache[cindex].index == index) {
		copy_v4_v4(ho, cache[cindex].ho);
		return cache[cindex].clip;
	}
	else {
		float wco;
		int clipflag= 0;
		copy_v3_v3(vec, co);
		projectvert(co, winmat, ho);

		wco= ho[3];
		if (ho[0] < bounds[0]*wco) clipflag |= 1;
		else if (ho[0] > bounds[1]*wco) clipflag |= 2;
		if (ho[1] > bounds[3]*wco) clipflag |= 4;
		else if (ho[1] < bounds[2]*wco) clipflag |= 8;

		copy_v4_v4(cache[cindex].ho, ho);
		cache[cindex].clip= clipflag;
		cache[cindex].index= index;

		return clipflag;
	}
}

void zbuf_render_project(float winmat[][4], const float co[3], float ho[4])
{
	float vec[3];

	copy_v3_v3(vec, co);
	projectvert(vec, winmat, ho);
}

void zbuf_make_winmat(Render *re, float winmat[][4])
{
	if (re->r.mode & R_PANORAMA) {
		float panomat[4][4]= MAT4_UNITY;

		panomat[0][0]= re->panoco;
		panomat[0][2]= re->panosi;
		panomat[2][0]= -re->panosi;
		panomat[2][2]= re->panoco;

		mult_m4_m4m4(winmat, re->winmat, panomat);
	}
	else
		copy_m4_m4(winmat, re->winmat);
}

/* do zbuffering and clip, f1 f2 f3 are hocos, c1 c2 c3 are clipping flags */

void zbufclip(ZSpan *zspan, int obi, int zvlnr, float *f1, float *f2, float *f3, int c1, int c2, int c3)
{
	float *vlzp[32][3], labda[3][2];
	float vez[400], *trias[40];
	
	if (c1 | c2 | c3) {	/* not in middle */
		if (c1 & c2 & c3) {	/* completely out */
			return;
		}
		else {	/* clipping */
			int arg, v, b, clipflag[3], b1, b2, b3, c4, clve=3, clvlo, clvl=1;

			vez[0]= f1[0]; vez[1]= f1[1]; vez[2]= f1[2]; vez[3]= f1[3];
			vez[4]= f2[0]; vez[5]= f2[1]; vez[6]= f2[2]; vez[7]= f2[3];
			vez[8]= f3[0]; vez[9]= f3[1]; vez[10]= f3[2];vez[11]= f3[3];

			vlzp[0][0]= vez;
			vlzp[0][1]= vez+4;
			vlzp[0][2]= vez+8;

			clipflag[0]= ( (c1 & 48) | (c2 & 48) | (c3 & 48) );
			if (clipflag[0]==0) {	/* othwerwise it needs to be calculated again, after the first (z) clip */
				clipflag[1]= ( (c1 & 3) | (c2 & 3) | (c3 & 3) );
				clipflag[2]= ( (c1 & 12) | (c2 & 12) | (c3 & 12) );
			}
			else clipflag[1]=clipflag[2]= 0;
			
			for (b=0;b<3;b++) {
				
				if (clipflag[b]) {
				
					clvlo= clvl;
					
					for (v=0; v<clvlo; v++) {
					
						if (vlzp[v][0]!=NULL) {	/* face is still there */
							b2= b3 =0;	/* clip flags */

							if (b==0) arg= 2;
							else if (b==1) arg= 0;
							else arg= 1;
							
							clippyra(labda[0], vlzp[v][0], vlzp[v][1], &b2, &b3, arg, zspan->clipcrop);
							clippyra(labda[1], vlzp[v][1], vlzp[v][2], &b2, &b3, arg, zspan->clipcrop);
							clippyra(labda[2], vlzp[v][2], vlzp[v][0], &b2, &b3, arg, zspan->clipcrop);

							if (b2==0 && b3==1) {
								/* completely 'in', but we copy because of last for () loop in this section */;
								vlzp[clvl][0]= vlzp[v][0];
								vlzp[clvl][1]= vlzp[v][1];
								vlzp[clvl][2]= vlzp[v][2];
								vlzp[v][0]= NULL;
								clvl++;
							}
							else if (b3==0) {
								vlzp[v][0]= NULL;
								/* completely 'out' */;
							}
							else {
								b1=0;
								makevertpyra(vez, labda[0], trias, vlzp[v][0], vlzp[v][1], &b1, &clve);
								makevertpyra(vez, labda[1], trias, vlzp[v][1], vlzp[v][2], &b1, &clve);
								makevertpyra(vez, labda[2], trias, vlzp[v][2], vlzp[v][0], &b1, &clve);

								/* after front clip done: now set clip flags */
								if (b==0) {
									clipflag[1]= clipflag[2]= 0;
									f1= vez;
									for (b3=0; b3<clve; b3++) {
										c4= testclip(f1);
										clipflag[1] |= (c4 & 3);
										clipflag[2] |= (c4 & 12);
										f1+= 4;
									}
								}
								
								vlzp[v][0]= NULL;
								if (b1>2) {
									for (b3=3; b3<=b1; b3++) {
										vlzp[clvl][0]= trias[0];
										vlzp[clvl][1]= trias[b3-2];
										vlzp[clvl][2]= trias[b3-1];
										clvl++;
									}
								}
							}
						}
					}
				}
			}

			/* warning, this should never happen! */
			if (clve>38 || clvl>31) printf("clip overflow: clve clvl %d %d\n", clve, clvl);

			/* perspective division */
			f1=vez;
			for (c1=0;c1<clve;c1++) {
				hoco_to_zco(zspan, f1, f1);
				f1+=4;
			}
			for (b=1;b<clvl;b++) {
				if (vlzp[b][0]) {
					zspan->zbuffunc(zspan, obi, zvlnr, vlzp[b][0], vlzp[b][1], vlzp[b][2], NULL);
				}
			}
			return;
		}
	}

	/* perspective division: HCS to ZCS */
	hoco_to_zco(zspan, vez, f1);
	hoco_to_zco(zspan, vez+4, f2);
	hoco_to_zco(zspan, vez+8, f3);
	zspan->zbuffunc(zspan, obi, zvlnr, vez, vez+4, vez+8, NULL);
}

void zbufclip4(ZSpan *zspan, int obi, int zvlnr, float *f1, float *f2, float *f3, float *f4, int c1, int c2, int c3, int c4)
{
	float vez[16];
	
	if (c1 | c2 | c3 | c4) {	/* not in middle */
		if (c1 & c2 & c3 & c4) {	/* completely out */
			return;
		}
		else {	/* clipping */
			zbufclip(zspan, obi, zvlnr, f1, f2, f3, c1, c2, c3);
			zbufclip(zspan, obi, zvlnr, f1, f3, f4, c1, c3, c4);
		}
		return;
	}

	/* perspective division: HCS to ZCS */
	hoco_to_zco(zspan, vez, f1);
	hoco_to_zco(zspan, vez+4, f2);
	hoco_to_zco(zspan, vez+8, f3);
	hoco_to_zco(zspan, vez+12, f4);

	zspan->zbuffunc(zspan, obi, zvlnr, vez, vez+4, vez+8, vez+12);
}

/* ************** ZMASK ******************************** */

#define EXTEND_PIXEL(a)	if (temprectp[a]) { z += rectz[a]; tot++; } (void)0

/* changes the zbuffer to be ready for z-masking: applies an extend-filter, and then clears */
static void zmask_rect(int *rectz, int *rectp, int xs, int ys, int neg)
{
	int len=0, x, y;
	int *temprectp;
	int row1, row2, row3, *curp, *curz;
	
	temprectp= MEM_dupallocN(rectp);
	
	/* extend: if pixel is not filled in, we check surrounding pixels and average z value  */
	
	for (y=1; y<=ys; y++) {
		/* setup row indices */
		row1= (y-2)*xs;
		row2= row1 + xs;
		row3= row2 + xs;
		if (y==1)
			row1= row2;
		else if (y==ys)
			row3= row2;
		
		curp= rectp + (y-1)*xs;
		curz= rectz + (y-1)*xs;
		
		for (x=0; x<xs; x++, curp++, curz++) {
			if (curp[0]==0) {
				int tot= 0;
				float z= 0.0f;
				
				EXTEND_PIXEL(row1);
				EXTEND_PIXEL(row2);
				EXTEND_PIXEL(row3);
				EXTEND_PIXEL(row1 + 1);
				EXTEND_PIXEL(row3 + 1);
				if (x!=xs-1) {
					EXTEND_PIXEL(row1 + 2);
					EXTEND_PIXEL(row2 + 2);
					EXTEND_PIXEL(row3 + 2);
				}					
				if (tot) {
					len++;
					curz[0]= (int)(z/(float)tot);
					curp[0]= -1;	/* env */
				}
			}
			
			if (x!=0) {
				row1++; row2++; row3++;
			}
		}
	}

	MEM_freeN(temprectp);
	
	if (neg); /* z values for negative are already correct */
	else {
		/* clear not filled z values */
		for (len= xs*ys -1; len>=0; len--) {
			if (rectp[len]==0) {
				rectz[len] = -0x7FFFFFFF;
				rectp[len]= -1;	/* env code */
			}	
		}
	}
}




/* ***************** ZBUFFER MAIN ROUTINES **************** */

void zbuffer_solid(RenderPart *pa, RenderLayer *rl, void(*fillfunc)(RenderPart*, ZSpan*, int, void*), void *data)
{
	ZbufProjectCache cache[ZBUF_PROJECT_CACHE_SIZE];
	ZSpan zspans[16], *zspan; /* 16 = RE_MAX_OSA */
	VlakRen *vlr= NULL;
	VertRen *v1, *v2, *v3, *v4;
	Material *ma=0;
	ObjectInstanceRen *obi;
	ObjectRen *obr;
	float obwinmat[4][4], winmat[4][4], bounds[4];
	float ho1[4], ho2[4], ho3[4], ho4[4]={0};
	unsigned int lay= rl->lay, lay_zmask= rl->lay_zmask;
	int i, v, zvlnr, zsample, samples, c1, c2, c3, c4=0;
	short nofill=0, env=0, wire=0, zmaskpass=0;
	short all_z= (rl->layflag & SCE_LAY_ALL_Z) && !(rl->layflag & SCE_LAY_ZMASK);
	short neg_zmask= (rl->layflag & SCE_LAY_ZMASK) && (rl->layflag & SCE_LAY_NEG_ZMASK);

	zbuf_make_winmat(&R, winmat);
	
	samples= (R.osa? R.osa: 1);
	samples= MIN2(4, samples-pa->sample);

	for (zsample=0; zsample<samples; zsample++) {
		zspan= &zspans[zsample];

		zbuffer_part_bounds(R.winx, R.winy, pa, bounds);
		zbuf_alloc_span(zspan, pa->rectx, pa->recty, R.clipcrop);
		
		/* needed for transform from hoco to zbuffer co */
		zspan->zmulx= ((float)R.winx)/2.0f;
		zspan->zmuly= ((float)R.winy)/2.0f;
		
		if (R.osa) {
			zspan->zofsx= -pa->disprect.xmin - R.jit[pa->sample+zsample][0];
			zspan->zofsy= -pa->disprect.ymin - R.jit[pa->sample+zsample][1];
		}
		else if (R.i.curblur) {
			zspan->zofsx= -pa->disprect.xmin - R.mblur_jit[R.i.curblur-1][0];
			zspan->zofsy= -pa->disprect.ymin - R.mblur_jit[R.i.curblur-1][1];
		}
		else {
			zspan->zofsx= -pa->disprect.xmin;
			zspan->zofsy= -pa->disprect.ymin;
		}
		/* to center the sample position */
		zspan->zofsx -= 0.5f;
		zspan->zofsy -= 0.5f;
		
		/* the buffers */
		if (zsample == samples-1) {
			zspan->rectp= pa->rectp;
			zspan->recto= pa->recto;

			if (neg_zmask)
				zspan->rectz= pa->rectmask;
			else
				zspan->rectz= pa->rectz;
		}
		else {
			zspan->recto= MEM_mallocN(sizeof(int)*pa->rectx*pa->recty, "recto");
			zspan->rectp= MEM_mallocN(sizeof(int)*pa->rectx*pa->recty, "rectp");
			zspan->rectz= MEM_mallocN(sizeof(int)*pa->rectx*pa->recty, "rectz");
		}

		fillrect(zspan->rectz, pa->rectx, pa->recty, 0x7FFFFFFF);
		fillrect(zspan->rectp, pa->rectx, pa->recty, 0);
		fillrect(zspan->recto, pa->rectx, pa->recty, 0);
	}

	/* in case zmask we fill Z for objects in lay_zmask first, then clear Z, and then do normal zbuffering */
	if (rl->layflag & SCE_LAY_ZMASK)
		zmaskpass= 1;
	
	for (; zmaskpass >=0; zmaskpass--) {
		ma= NULL;

		/* filling methods */
		for (zsample=0; zsample<samples; zsample++) {
			zspan= &zspans[zsample];

			if (zmaskpass && neg_zmask)
				zspan->zbuffunc= zbuffillGLinv4;
			else
				zspan->zbuffunc= zbuffillGL4;
			zspan->zbuflinefunc= zbufline;
		}

		/* regular zbuffering loop, does all sample buffers */
		for (i=0, obi=R.instancetable.first; obi; i++, obi=obi->next) {
			obr= obi->obr;

			/* continue happens in 2 different ways... zmaskpass only does lay_zmask stuff */
			if (zmaskpass) {
				if ((obi->lay & lay_zmask)==0)
					continue;
			}
			else if (!all_z && !(obi->lay & (lay|lay_zmask)))
				continue;
			
			if (obi->flag & R_TRANSFORMED)
				mult_m4_m4m4(obwinmat, winmat, obi->mat);
			else
				copy_m4_m4(obwinmat, winmat);

			if (clip_render_object(obi->obr->boundbox, bounds, obwinmat))
				continue;

			zbuf_project_cache_clear(cache, obr->totvert);

			for (v=0; v<obr->totvlak; v++) {
				if ((v & 255)==0) vlr= obr->vlaknodes[v>>8].vlak;
				else vlr++;

				/* the cases: visible for render, only z values, zmask, nothing */
				if (obi->lay & lay) {
					if (vlr->mat!=ma) {
						ma= vlr->mat;
						nofill= (ma->mode & MA_ONLYCAST) || ((ma->mode & MA_TRANSP) && (ma->mode & MA_ZTRANSP));
						env= (ma->mode & MA_ENV);
						wire= (ma->material_type == MA_TYPE_WIRE);
						
						for (zsample=0; zsample<samples; zsample++) {
							if (ma->mode & MA_ZINV || (zmaskpass && neg_zmask))
								zspans[zsample].zbuffunc= zbuffillGLinv4;
							else
								zspans[zsample].zbuffunc= zbuffillGL4;
						}
					}
				}
				else if (all_z || (obi->lay & lay_zmask)) {
					env= 1;
					nofill= 0;
					ma= NULL; 
				}
				else {
					nofill= 1;
					ma= NULL;	/* otherwise nofill can hang */
				}

				if (!(vlr->flag & R_HIDDEN) && nofill==0) {
					unsigned short partclip;
					
					v1= vlr->v1;
					v2= vlr->v2;
					v3= vlr->v3;
					v4= vlr->v4;

					c1= zbuf_part_project(cache, v1->index, obwinmat, bounds, v1->co, ho1);
					c2= zbuf_part_project(cache, v2->index, obwinmat, bounds, v2->co, ho2);
					c3= zbuf_part_project(cache, v3->index, obwinmat, bounds, v3->co, ho3);

					/* partclipping doesn't need viewplane clipping */
					partclip= c1 & c2 & c3;
					if (v4) {
						c4= zbuf_part_project(cache, v4->index, obwinmat, bounds, v4->co, ho4);
						partclip &= c4;
					}

					if (partclip==0) {
						
						if (env) zvlnr= -1;
						else zvlnr= v+1;

						c1= testclip(ho1);
						c2= testclip(ho2);
						c3= testclip(ho3);
						if (v4)
							c4= testclip(ho4);

						for (zsample=0; zsample<samples; zsample++) {
							zspan= &zspans[zsample];

							if (wire) {
								if (v4)
									zbufclipwire(zspan, i, zvlnr, vlr->ec, ho1, ho2, ho3, ho4, c1, c2, c3, c4);
								else
									zbufclipwire(zspan, i, zvlnr, vlr->ec, ho1, ho2, ho3, 0, c1, c2, c3, 0);
							}
							else {
								/* strands allow to be filled in as quad */
								if (v4 && (vlr->flag & R_STRAND)) {
									zbufclip4(zspan, i, zvlnr, ho1, ho2, ho3, ho4, c1, c2, c3, c4);
								}
								else {
									zbufclip(zspan, i, zvlnr, ho1, ho2, ho3, c1, c2, c3);
									if (v4)
										zbufclip(zspan, i, (env)? zvlnr: zvlnr+RE_QUAD_OFFS, ho1, ho3, ho4, c1, c3, c4);
								}
							}
						}
					}
				}
			}
		}
		
		/* clear all z to close value, so it works as mask for next passes (ztra+strand) */
		if (zmaskpass) {
			for (zsample=0; zsample<samples; zsample++) {
				zspan= &zspans[zsample];

				if (neg_zmask) {
					zspan->rectmask= zspan->rectz;
					if (zsample == samples-1)
						zspan->rectz= pa->rectz;
					else
						zspan->rectz= MEM_mallocN(sizeof(int)*pa->rectx*pa->recty, "rectz");
					fillrect(zspan->rectz, pa->rectx, pa->recty, 0x7FFFFFFF);

					zmask_rect(zspan->rectmask, zspan->rectp, pa->rectx, pa->recty, 1);
				}
				else
					zmask_rect(zspan->rectz, zspan->rectp, pa->rectx, pa->recty, 0);
			}
		}
	}

	for (zsample=0; zsample<samples; zsample++) {
		zspan= &zspans[zsample];

		if (fillfunc)
			fillfunc(pa, zspan, pa->sample+zsample, data);

		if (zsample != samples-1) {
			MEM_freeN(zspan->rectz);
			MEM_freeN(zspan->rectp);
			MEM_freeN(zspan->recto);
			if (zspan->rectmask)
				MEM_freeN(zspan->rectmask);
		}

		zbuf_free_span(zspan);
	}
}

void zbuffer_shadow(Render *re, float winmat[][4], LampRen *lar, int *rectz, int size, float jitx, float jity)
{
	ZbufProjectCache cache[ZBUF_PROJECT_CACHE_SIZE];
	ZSpan zspan;
	ObjectInstanceRen *obi;
	ObjectRen *obr;
	VlakRen *vlr= NULL;
	Material *ma= NULL;
	StrandSegment sseg;
	StrandRen *strand= NULL;
	StrandVert *svert;
	StrandBound *sbound;
	float obwinmat[4][4], ho1[4], ho2[4], ho3[4], ho4[4];
	int a, b, c, i, c1, c2, c3, c4, ok=1, lay= -1;

	if (lar->mode & (LA_LAYER|LA_LAYER_SHADOW)) lay= lar->lay;

	/* 1.0f for clipping in clippyra()... bad stuff actually */
	zbuf_alloc_span(&zspan, size, size, 1.0f);
	zspan.zmulx=  ((float)size)/2.0f;
	zspan.zmuly=  ((float)size)/2.0f;
	/* -0.5f to center the sample position */
	zspan.zofsx= jitx - 0.5f;
	zspan.zofsy= jity - 0.5f;
	
	/* the buffers */
	zspan.rectz= rectz;
	fillrect(rectz, size, size, 0x7FFFFFFE);
	if (lar->buftype==LA_SHADBUF_HALFWAY) {
		zspan.rectz1= MEM_mallocN(size*size*sizeof(int), "seconday z buffer");
		fillrect(zspan.rectz1, size, size, 0x7FFFFFFE);
	}
	
	/* filling methods */
	zspan.zbuflinefunc= zbufline_onlyZ;
	zspan.zbuffunc= zbuffillGL_onlyZ;

	for (i=0, obi=re->instancetable.first; obi; i++, obi=obi->next) {
		obr= obi->obr;

		if (obr->ob==re->excludeob)
			continue;
		else if (!(obi->lay & lay))
			continue;

		if (obi->flag & R_TRANSFORMED)
			mult_m4_m4m4(obwinmat, winmat, obi->mat);
		else
			copy_m4_m4(obwinmat, winmat);

		if (clip_render_object(obi->obr->boundbox, NULL, obwinmat))
			continue;

		zbuf_project_cache_clear(cache, obr->totvert);

		/* faces */
		for (a=0; a<obr->totvlak; a++) {

			if ((a & 255)==0) vlr= obr->vlaknodes[a>>8].vlak;
			else vlr++;

			/* note, these conditions are copied in shadowbuf_autoclip() */
			if (vlr->mat!= ma) {
				ma= vlr->mat;
				ok= 1;
				if ((ma->mode & MA_SHADBUF)==0) ok= 0;
			}

			if (ok && (obi->lay & lay) && !(vlr->flag & R_HIDDEN)) {
				c1= zbuf_shadow_project(cache, vlr->v1->index, obwinmat, vlr->v1->co, ho1);
				c2= zbuf_shadow_project(cache, vlr->v2->index, obwinmat, vlr->v2->co, ho2);
				c3= zbuf_shadow_project(cache, vlr->v3->index, obwinmat, vlr->v3->co, ho3);

				if ((ma->material_type == MA_TYPE_WIRE) || (vlr->flag & R_STRAND)) {
					if (vlr->v4) {
						c4= zbuf_shadow_project(cache, vlr->v4->index, obwinmat, vlr->v4->co, ho4);
						zbufclipwire(&zspan, 0, a+1, vlr->ec, ho1, ho2, ho3, ho4, c1, c2, c3, c4);
					}
					else
						zbufclipwire(&zspan, 0, a+1, vlr->ec, ho1, ho2, ho3, 0, c1, c2, c3, 0);
				}
				else {
					if (vlr->v4) {
						c4= zbuf_shadow_project(cache, vlr->v4->index, obwinmat, vlr->v4->co, ho4);
						zbufclip4(&zspan, 0, 0, ho1, ho2, ho3, ho4, c1, c2, c3, c4);
					}
					else
						zbufclip(&zspan, 0, 0, ho1, ho2, ho3, c1, c2, c3);
				}
			}

			if ((a & 255)==255 && re->test_break(re->tbh)) 
				break;
		}

		/* strands */
		if (obr->strandbuf) {
			/* for each bounding box containing a number of strands */
			sbound= obr->strandbuf->bound;
			for (c=0; c<obr->strandbuf->totbound; c++, sbound++) {
				if (clip_render_object(sbound->boundbox, NULL, obwinmat))
					continue;

				/* for each strand in this bounding box */
				for (a=sbound->start; a<sbound->end; a++) {
					strand= RE_findOrAddStrand(obr, a);

					sseg.obi= obi;
					sseg.buffer= strand->buffer;
					sseg.sqadaptcos= sseg.buffer->adaptcos;
					sseg.sqadaptcos *= sseg.sqadaptcos;
					sseg.strand= strand;
					svert= strand->vert;

					/* note, these conditions are copied in shadowbuf_autoclip() */
					if (sseg.buffer->ma!= ma) {
						ma= sseg.buffer->ma;
						ok= 1;
						if ((ma->mode & MA_SHADBUF)==0) ok= 0;
					}

					if (ok && (sseg.buffer->lay & lay)) {
						zbuf_project_cache_clear(cache, strand->totvert);

						for (b=0; b<strand->totvert-1; b++, svert++) {
							sseg.v[0]= (b > 0)? (svert-1): svert;
							sseg.v[1]= svert;
							sseg.v[2]= svert+1;
							sseg.v[3]= (b < strand->totvert-2)? svert+2: svert+1;

							c1= zbuf_shadow_project(cache, sseg.v[0]-strand->vert, obwinmat, sseg.v[0]->co, ho1);
							c2= zbuf_shadow_project(cache, sseg.v[1]-strand->vert, obwinmat, sseg.v[1]->co, ho2);
							c3= zbuf_shadow_project(cache, sseg.v[2]-strand->vert, obwinmat, sseg.v[2]->co, ho3);
							c4= zbuf_shadow_project(cache, sseg.v[3]-strand->vert, obwinmat, sseg.v[3]->co, ho4);

							if (!(c1 & c2 & c3 & c4))
								render_strand_segment(re, winmat, NULL, &zspan, 1, &sseg);
						}
					}

					if ((a & 255)==255 && re->test_break(re->tbh)) 
						break;
				}
			}
		}

		if (re->test_break(re->tbh)) 
			break;
	}
	
	/* merge buffers */
	if (lar->buftype==LA_SHADBUF_HALFWAY) {
		for (a=size*size -1; a>=0; a--)
			rectz[a]= (rectz[a]>>1) + (zspan.rectz1[a]>>1);
		
		MEM_freeN(zspan.rectz1);
	}
	
	zbuf_free_span(&zspan);
}

static void zbuffill_sss(ZSpan *zspan, int obi, int zvlnr,
                         const float *v1, const float *v2, const float *v3, const float *v4)
{
	double zxd, zyd, zy0, z;
	float x0, y0, x1, y1, x2, y2, z0, z1, z2, xx1, *span1, *span2;
	int x, y, sn1, sn2, rectx= zspan->rectx, my0, my2;
	/* init */
	zbuf_init_span(zspan);
	
	/* set spans */
	zbuf_add_to_span(zspan, v1, v2);
	zbuf_add_to_span(zspan, v2, v3);
	if (v4) {
		zbuf_add_to_span(zspan, v3, v4);
		zbuf_add_to_span(zspan, v4, v1);
	}
	else 
		zbuf_add_to_span(zspan, v3, v1);
	
	/* clipped */
	if (zspan->minp2==NULL || zspan->maxp2==NULL) return;
	
	if (zspan->miny1 < zspan->miny2) my0= zspan->miny2; else my0= zspan->miny1;
	if (zspan->maxy1 > zspan->maxy2) my2= zspan->maxy2; else my2= zspan->maxy1;
	
	if (my2<my0) return;
	
	/* ZBUF DX DY, in floats still */
	x1= v1[0]- v2[0];
	x2= v2[0]- v3[0];
	y1= v1[1]- v2[1];
	y2= v2[1]- v3[1];
	z1= v1[2]- v2[2];
	z2= v2[2]- v3[2];
	
	x0= y1*z2-z1*y2;
	y0= z1*x2-x1*z2;
	z0= x1*y2-y1*x2;
	
	if (z0==0.0f) return;
	
	xx1= (x0*v1[0] + y0*v1[1])/z0 + v1[2];
	zxd= -(double)x0/(double)z0;
	zyd= -(double)y0/(double)z0;
	zy0= ((double)my2)*zyd + (double)xx1;
	
	/* correct span */
	sn1= (my0 + my2)/2;
	if (zspan->span1[sn1] < zspan->span2[sn1]) {
		span1= zspan->span1+my2;
		span2= zspan->span2+my2;
	}
	else {
		span1= zspan->span2+my2;
		span2= zspan->span1+my2;
	}
	
	for (y=my2; y>=my0; y--, span1--, span2--) {
		sn1= floor(*span1);
		sn2= floor(*span2);
		sn1++; 
		
		if (sn2>=rectx) sn2= rectx-1;
		if (sn1<0) sn1= 0;
		
		z= (double)sn1*zxd + zy0;
		
		for (x= sn1; x<=sn2; x++, z+=zxd)
			zspan->sss_func(zspan->sss_handle, obi, zvlnr, x, y, z);
		
		zy0 -= zyd;
	}
}

void zbuffer_sss(RenderPart *pa, unsigned int lay, void *handle, void (*func)(void*, int, int, int, int, int))
{
	ZbufProjectCache cache[ZBUF_PROJECT_CACHE_SIZE];
	ZSpan zspan;
	ObjectInstanceRen *obi;
	ObjectRen *obr;
	VlakRen *vlr= NULL;
	VertRen *v1, *v2, *v3, *v4;
	Material *ma=0, *sss_ma= R.sss_mat;
	float obwinmat[4][4], winmat[4][4], bounds[4];
	float ho1[4], ho2[4], ho3[4], ho4[4]={0};
	int i, v, zvlnr, c1, c2, c3, c4=0;
	short nofill=0, env=0, wire=0;
	
	zbuf_make_winmat(&R, winmat);
	zbuffer_part_bounds(R.winx, R.winy, pa, bounds);
	zbuf_alloc_span(&zspan, pa->rectx, pa->recty, R.clipcrop);

	zspan.sss_handle= handle;
	zspan.sss_func= func;
	
	/* needed for transform from hoco to zbuffer co */
	zspan.zmulx=  ((float)R.winx)/2.0f;
	zspan.zmuly=  ((float)R.winy)/2.0f;
	
	/* -0.5f to center the sample position */
	zspan.zofsx= -pa->disprect.xmin - 0.5f;
	zspan.zofsy= -pa->disprect.ymin - 0.5f;
	
	/* filling methods */
	zspan.zbuffunc= zbuffill_sss;

	/* fill front and back zbuffer */
	if (pa->rectz) {
		fillrect(pa->recto, pa->rectx, pa->recty, 0); 
		fillrect(pa->rectp, pa->rectx, pa->recty, 0); 
		fillrect(pa->rectz, pa->rectx, pa->recty, 0x7FFFFFFF);
	}
	if (pa->rectbackz) {
		fillrect(pa->rectbacko, pa->rectx, pa->recty, 0); 
		fillrect(pa->rectbackp, pa->rectx, pa->recty, 0); 
		fillrect(pa->rectbackz, pa->rectx, pa->recty, -0x7FFFFFFF);
	}

	for (i=0, obi=R.instancetable.first; obi; i++, obi=obi->next) {
		obr= obi->obr;

		if (!(obi->lay & lay))
			continue;

		if (obi->flag & R_TRANSFORMED)
			mult_m4_m4m4(obwinmat, winmat, obi->mat);
		else
			copy_m4_m4(obwinmat, winmat);

		if (clip_render_object(obi->obr->boundbox, bounds, obwinmat))
			continue;

		zbuf_project_cache_clear(cache, obr->totvert);

		for (v=0; v<obr->totvlak; v++) {
			if ((v & 255)==0) vlr= obr->vlaknodes[v>>8].vlak;
			else vlr++;
			
			if (material_in_material(vlr->mat, sss_ma)) {
				/* three cases, visible for render, only z values and nothing */
				if (obi->lay & lay) {
					if (vlr->mat!=ma) {
						ma= vlr->mat;
						nofill= ma->mode & MA_ONLYCAST;
						env= (ma->mode & MA_ENV);
						wire= (ma->material_type == MA_TYPE_WIRE);
					}
				}
				else {
					nofill= 1;
					ma= NULL;	/* otherwise nofill can hang */
				}
				
				if (nofill==0 && wire==0 && env==0) {
					unsigned short partclip;
					
					v1= vlr->v1;
					v2= vlr->v2;
					v3= vlr->v3;
					v4= vlr->v4;

					c1= zbuf_part_project(cache, v1->index, obwinmat, bounds, v1->co, ho1);
					c2= zbuf_part_project(cache, v2->index, obwinmat, bounds, v2->co, ho2);
					c3= zbuf_part_project(cache, v3->index, obwinmat, bounds, v3->co, ho3);

					/* partclipping doesn't need viewplane clipping */
					partclip= c1 & c2 & c3;
					if (v4) {
						c4= zbuf_part_project(cache, v4->index, obwinmat, bounds, v4->co, ho4);
						partclip &= c4;
					}

					if (partclip==0) {
						c1= testclip(ho1);
						c2= testclip(ho2);
						c3= testclip(ho3);

						zvlnr= v+1;
						zbufclip(&zspan, i, zvlnr, ho1, ho2, ho3, c1, c2, c3);
						if (v4) {
							c4= testclip(ho4);
							zbufclip(&zspan, i, zvlnr+RE_QUAD_OFFS, ho1, ho3, ho4, c1, c3, c4);
						}
					}
				}
			}
		}
	}
		
	zbuf_free_span(&zspan);
}

/* ******************** VECBLUR ACCUM BUF ************************* */

typedef struct DrawBufPixel {
	float *colpoin;
	float alpha;
} DrawBufPixel;


static void zbuf_fill_in_rgba(ZSpan *zspan, DrawBufPixel *col, float *v1, float *v2, float *v3, float *v4)
{
	DrawBufPixel *rectpofs, *rp;
	double zxd, zyd, zy0, zverg;
	float x0, y0, z0;
	float x1, y1, z1, x2, y2, z2, xx1;
	float *span1, *span2;
	float *rectzofs, *rz;
	int x, y;
	int sn1, sn2, rectx, my0, my2;
	
	/* init */
	zbuf_init_span(zspan);
	
	/* set spans */
	zbuf_add_to_span(zspan, v1, v2);
	zbuf_add_to_span(zspan, v2, v3);
	zbuf_add_to_span(zspan, v3, v4);
	zbuf_add_to_span(zspan, v4, v1);
	
	/* clipped */
	if (zspan->minp2==NULL || zspan->maxp2==NULL) return;
	
	if (zspan->miny1 < zspan->miny2) my0= zspan->miny2; else my0= zspan->miny1;
	if (zspan->maxy1 > zspan->maxy2) my2= zspan->maxy2; else my2= zspan->maxy1;
	
	//	printf("my %d %d\n", my0, my2);
	if (my2<my0) return;
	
	/* ZBUF DX DY, in floats still */
	x1= v1[0]- v2[0];
	x2= v2[0]- v3[0];
	y1= v1[1]- v2[1];
	y2= v2[1]- v3[1];
	z1= v1[2]- v2[2];
	z2= v2[2]- v3[2];
	x0= y1*z2-z1*y2;
	y0= z1*x2-x1*z2;
	z0= x1*y2-y1*x2;
	
	if (z0==0.0f) return;
	
	xx1= (x0*v1[0] + y0*v1[1])/z0 + v1[2];
	
	zxd= -(double)x0/(double)z0;
	zyd= -(double)y0/(double)z0;
	zy0= ((double)my2)*zyd + (double)xx1;
	
	/* start-offset in rect */
	rectx= zspan->rectx;
	rectzofs= (float *)(zspan->rectz + rectx*my2);
	rectpofs= ((DrawBufPixel *)zspan->rectp) + rectx*my2;
	
	/* correct span */
	sn1= (my0 + my2)/2;
	if (zspan->span1[sn1] < zspan->span2[sn1]) {
		span1= zspan->span1+my2;
		span2= zspan->span2+my2;
	}
	else {
		span1= zspan->span2+my2;
		span2= zspan->span1+my2;
	}
	
	for (y=my2; y>=my0; y--, span1--, span2--) {
		
		sn1= floor(*span1);
		sn2= floor(*span2);
		sn1++; 
		
		if (sn2>=rectx) sn2= rectx-1;
		if (sn1<0) sn1= 0;
		
		if (sn2>=sn1) {
			zverg= (double)sn1*zxd + zy0;
			rz= rectzofs+sn1;
			rp= rectpofs+sn1;
			x= sn2-sn1;
			
			while (x>=0) {
				if ( zverg < *rz) {
					*rz= zverg;
					*rp= *col;
				}
				zverg+= zxd;
				rz++; 
				rp++; 
				x--;
			}
		}
		
		zy0-=zyd;
		rectzofs-= rectx;
		rectpofs-= rectx;
	}
}

/* char value==255 is filled in, rest should be zero */
/* returns alpha values, but sets alpha to 1 for zero alpha pixels that have an alpha value as neighbor */
void antialias_tagbuf(int xsize, int ysize, char *rectmove)
{
	char *row1, *row2, *row3;
	char prev, next;
	int a, x, y, step;
	
	/* 1: tag pixels to be candidate for AA */
	for (y=2; y<ysize; y++) {
		/* setup rows */
		row1= rectmove + (y-2)*xsize;
		row2= row1 + xsize;
		row3= row2 + xsize;
		for (x=2; x<xsize; x++, row1++, row2++, row3++) {
			if (row2[1]) {
				if (row2[0]==0 || row2[2]==0 || row1[1]==0 || row3[1]==0)
					row2[1]= 128;
			}
		}
	}
	
	/* 2: evaluate horizontal scanlines and calculate alphas */
	row1= rectmove;
	for (y=0; y<ysize; y++) {
		row1++;
		for (x=1; x<xsize; x++, row1++) {
			if (row1[0]==128 && row1[1]==128) {
				/* find previous color and next color and amount of steps to blend */
				prev= row1[-1];
				step= 1;
				while (x+step<xsize && row1[step]==128)
					step++;
				
				if (x+step!=xsize) {
					/* now we can blend values */
					next= row1[step];

					/* note, prev value can be next value, but we do this loop to clear 128 then */
					for (a=0; a<step; a++) {
						int fac, mfac;
						
						fac= ((a+1)<<8)/(step+1);
						mfac= 255-fac;
						
						row1[a]= (prev*mfac + next*fac)>>8; 
					}
				}
			}
		}
	}
	
	/* 3: evaluate vertical scanlines and calculate alphas */
	/*    use for reading a copy of the original tagged buffer */
	for (x=0; x<xsize; x++) {
		row1= rectmove + x+xsize;
		
		for (y=1; y<ysize; y++, row1+=xsize) {
			if (row1[0]==128 && row1[xsize]==128) {
				/* find previous color and next color and amount of steps to blend */
				prev= row1[-xsize];
				step= 1;
				while (y+step<ysize && row1[step*xsize]==128)
					step++;
				
				if (y+step!=ysize) {
					/* now we can blend values */
					next= row1[step*xsize];
					/* note, prev value can be next value, but we do this loop to clear 128 then */
					for (a=0; a<step; a++) {
						int fac, mfac;
						
						fac= ((a+1)<<8)/(step+1);
						mfac= 255-fac;
						
						row1[a*xsize]= (prev*mfac + next*fac)>>8; 
					}
				}
			}
		}
	}
	
	/* last: pixels with 0 we fill in zbuffer, with 1 we skip for mask */
	for (y=2; y<ysize; y++) {
		/* setup rows */
		row1= rectmove + (y-2)*xsize;
		row2= row1 + xsize;
		row3= row2 + xsize;
		for (x=2; x<xsize; x++, row1++, row2++, row3++) {
			if (row2[1]==0) {
				if (row2[0]>1 || row2[2]>1 || row1[1]>1 || row3[1]>1)
					row2[1]= 1;
			}
		}
	}
}

/* in: two vectors, first vector points from origin back in time, 2nd vector points to future */
/* we make this into 3 points, center point is (0, 0) */
/* and offset the center point just enough to make curve go through midpoint */

static void quad_bezier_2d(float *result, float *v1, float *v2, float *ipodata)
{
	float p1[2], p2[2], p3[2];
	
	p3[0]= -v2[0];
	p3[1]= -v2[1];
	
	p1[0]= v1[0];
	p1[1]= v1[1];
	
	/* official formula 2*p2 - 0.5*p1 - 0.5*p3 */
	p2[0]= -0.5f*p1[0] - 0.5f*p3[0];
	p2[1]= -0.5f*p1[1] - 0.5f*p3[1];
	
	result[0]= ipodata[0]*p1[0] + ipodata[1]*p2[0] + ipodata[2]*p3[0];
	result[1]= ipodata[0]*p1[1] + ipodata[1]*p2[1] + ipodata[2]*p3[1];
}

static void set_quad_bezier_ipo(float fac, float *data)
{
	float mfac= (1.0f-fac);
	
	data[0]= mfac*mfac;
	data[1]= 2.0f*mfac*fac;
	data[2]= fac*fac;
}

void RE_zbuf_accumulate_vecblur(NodeBlurData *nbd, int xsize, int ysize, float *newrect, float *imgrect, float *vecbufrect, float *zbufrect)
{
	ZSpan zspan;
	DrawBufPixel *rectdraw, *dr;
	static float jit[256][2];
	float v1[3], v2[3], v3[3], v4[3], fx, fy;
	float *rectvz, *dvz, *dimg, *dvec1, *dvec2, *dz, *dz1, *dz2, *rectz;
	float *minvecbufrect= NULL, *rectweight, *rw, *rectmax, *rm, *ro;
	float maxspeedsq= (float)nbd->maxspeed*nbd->maxspeed;
	int y, x, step, maxspeed=nbd->maxspeed, samples= nbd->samples;
	int tsktsk= 0;
	static int firsttime= 1;
	char *rectmove, *dm;
	
	zbuf_alloc_span(&zspan, xsize, ysize, 1.0f);
	zspan.zmulx=  ((float)xsize)/2.0f;
	zspan.zmuly=  ((float)ysize)/2.0f;
	zspan.zofsx= 0.0f;
	zspan.zofsy= 0.0f;
	
	/* the buffers */
	rectz= MEM_mapallocN(sizeof(float)*xsize*ysize, "zbuf accum");
	zspan.rectz= (int *)rectz;
	
	rectmove= MEM_mapallocN(xsize*ysize, "rectmove");
	rectdraw= MEM_mapallocN(sizeof(DrawBufPixel)*xsize*ysize, "rect draw");
	zspan.rectp= (int *)rectdraw;

	rectweight= MEM_mapallocN(sizeof(float)*xsize*ysize, "rect weight");
	rectmax= MEM_mapallocN(sizeof(float)*xsize*ysize, "rect max");
	
	/* debug... check if PASS_VECTOR_MAX still is in buffers */
	dvec1= vecbufrect;
	for (x= 4*xsize*ysize; x>0; x--, dvec1++) {
		if (dvec1[0]==PASS_VECTOR_MAX) {
			dvec1[0]= 0.0f;
			tsktsk= 1;
		}
	}
	if (tsktsk) printf("Found uninitialized speed in vector buffer... fixed.\n");
	
	/* min speed? then copy speedbuffer to recalculate speed vectors */
	if (nbd->minspeed) {
		float minspeed= (float)nbd->minspeed;
		float minspeedsq= minspeed*minspeed;
		
		minvecbufrect= MEM_mapallocN(4*sizeof(float)*xsize*ysize, "minspeed buf");
		
		dvec1= vecbufrect;
		dvec2= minvecbufrect;
		for (x= 2*xsize*ysize; x>0; x--, dvec1+=2, dvec2+=2) {
			if (dvec1[0]==0.0f && dvec1[1]==0.0f) {
				dvec2[0]= dvec1[0];
				dvec2[1]= dvec1[1];
			}
			else {
				float speedsq= dvec1[0]*dvec1[0] + dvec1[1]*dvec1[1];
				if (speedsq <= minspeedsq) {
					dvec2[0]= 0.0f;
					dvec2[1]= 0.0f;
				}
				else {
					speedsq= 1.0f - minspeed/sqrt(speedsq);
					dvec2[0]= speedsq*dvec1[0];
					dvec2[1]= speedsq*dvec1[1];
				}
			}
		}
		SWAP(float *, minvecbufrect, vecbufrect);
	}
	
	/* make vertex buffer with averaged speed and zvalues */
	rectvz= MEM_mapallocN(4*sizeof(float)*(xsize+1)*(ysize+1), "vertices");
	dvz= rectvz;
	for (y=0; y<=ysize; y++) {
		
		if (y==0)
			dvec1= vecbufrect + 4*y*xsize;
		else
			dvec1= vecbufrect + 4*(y-1)*xsize;
		
		if (y==ysize)
			dvec2= vecbufrect + 4*(y-1)*xsize;
		else
			dvec2= vecbufrect + 4*y*xsize;
		
		for (x=0; x<=xsize; x++) {
			
			/* two vectors, so a step loop */
			for (step=0; step<2; step++, dvec1+=2, dvec2+=2, dvz+=2) {
				/* average on minimal speed */
				int div= 0;
				
				if (x!=0) {
					if (dvec1[-4]!=0.0f || dvec1[-3]!=0.0f) {
						dvz[0]= dvec1[-4];
						dvz[1]= dvec1[-3];
						div++;
					}
					if (dvec2[-4]!=0.0f || dvec2[-3]!=0.0f) {
						if (div==0) {
							dvz[0]= dvec2[-4];
							dvz[1]= dvec2[-3];
							div++;
						}
						else if ( (ABS(dvec2[-4]) + ABS(dvec2[-3]))< (ABS(dvz[0]) + ABS(dvz[1])) ) {
							dvz[0]= dvec2[-4];
							dvz[1]= dvec2[-3];
						}
					}
				}

				if (x!=xsize) {
					if (dvec1[0]!=0.0f || dvec1[1]!=0.0f) {
						if (div==0) {
							dvz[0]= dvec1[0];
							dvz[1]= dvec1[1];
							div++;
						}
						else if ( (ABS(dvec1[0]) + ABS(dvec1[1]))< (ABS(dvz[0]) + ABS(dvz[1])) ) {
							dvz[0]= dvec1[0];
							dvz[1]= dvec1[1];
						}
					}
					if (dvec2[0]!=0.0f || dvec2[1]!=0.0f) {
						if (div==0) {
							dvz[0]= dvec2[0];
							dvz[1]= dvec2[1];
						}
						else if ( (ABS(dvec2[0]) + ABS(dvec2[1]))< (ABS(dvz[0]) + ABS(dvz[1])) ) {
							dvz[0]= dvec2[0];
							dvz[1]= dvec2[1];
						}
					}
				}
				if (maxspeed) {
					float speedsq= dvz[0]*dvz[0] + dvz[1]*dvz[1];
					if (speedsq > maxspeedsq) {
						speedsq= (float)maxspeed/sqrt(speedsq);
						dvz[0]*= speedsq;
						dvz[1]*= speedsq;
					}
				}
			}
		}
	}
	
	/* set border speeds to keep border speeds on border */
	dz1= rectvz;
	dz2= rectvz+4*(ysize)*(xsize+1);
	for (x=0; x<=xsize; x++, dz1+=4, dz2+=4) {
		dz1[1]= 0.0f;
		dz2[1]= 0.0f;
		dz1[3]= 0.0f;
		dz2[3]= 0.0f;
	}
	dz1= rectvz;
	dz2= rectvz+4*(xsize);
	for (y=0; y<=ysize; y++, dz1+=4*(xsize+1), dz2+=4*(xsize+1)) {
		dz1[0]= 0.0f;
		dz2[0]= 0.0f;
		dz1[2]= 0.0f;
		dz2[2]= 0.0f;
	}
	
	/* tag moving pixels, only these faces we draw */
	dm= rectmove;
	dvec1= vecbufrect;
	for (x=xsize*ysize; x>0; x--, dm++, dvec1+=4) {
		if ((dvec1[0]!=0.0f || dvec1[1]!=0.0f || dvec1[2]!=0.0f || dvec1[3]!=0.0f))
			*dm= 255;
	}
	
	antialias_tagbuf(xsize, ysize, rectmove);
	
	/* has to become static, the init-jit calls a random-seed, screwing up texture noise node */
	if (firsttime) {
		firsttime= 0;
		BLI_jitter_init(jit[0], 256);
	}
	
	memset(newrect, 0, sizeof(float)*xsize*ysize*4);

	/* accumulate */
	samples/= 2;
	for (step= 1; step<=samples; step++) {
		float speedfac= 0.5f*nbd->fac*(float)step/(float)(samples+1);
		int side;
		
		for (side=0; side<2; side++) {
			float blendfac, ipodata[4];
			
			/* clear zbuf, if we draw future we fill in not moving pixels */
			if (0)
				for (x= xsize*ysize-1; x>=0; x--) rectz[x]= 10e16;
			else 
				for (x= xsize*ysize-1; x>=0; x--) {
					if (rectmove[x]==0)
						rectz[x]= zbufrect[x];
					else
						rectz[x]= 10e16;
				}
			
			/* clear drawing buffer */
			for (x= xsize*ysize-1; x>=0; x--) rectdraw[x].colpoin= NULL;
			
			dimg= imgrect;
			dm= rectmove;
			dz= zbufrect;
			dz1= rectvz;
			dz2= rectvz + 4*(xsize + 1);
			
			if (side) {
				if (nbd->curved==0) {
					dz1+= 2;
					dz2+= 2;
				}
				speedfac= -speedfac;
			}
			
			set_quad_bezier_ipo(0.5f + 0.5f*speedfac, ipodata);
			
			for (fy= -0.5f+jit[step & 255][0], y=0; y<ysize; y++, fy+=1.0f) {
				for (fx= -0.5f+jit[step & 255][1], x=0; x<xsize; x++, fx+=1.0f, dimg+=4, dz1+=4, dz2+=4, dm++, dz++) {
					if (*dm>1) {
						float jfx = fx + 0.5f;
						float jfy = fy + 0.5f;
						DrawBufPixel col;
						
						/* make vertices */
						if (nbd->curved) {	/* curved */
							quad_bezier_2d(v1, dz1, dz1+2, ipodata);
							v1[0]+= jfx; v1[1]+= jfy; v1[2]= *dz;

							quad_bezier_2d(v2, dz1+4, dz1+4+2, ipodata);
							v2[0]+= jfx+1.0f; v2[1]+= jfy; v2[2]= *dz;

							quad_bezier_2d(v3, dz2+4, dz2+4+2, ipodata);
							v3[0]+= jfx+1.0f; v3[1]+= jfy+1.0f; v3[2]= *dz;
							
							quad_bezier_2d(v4, dz2, dz2+2, ipodata);
							v4[0]+= jfx; v4[1]+= jfy+1.0f; v4[2]= *dz;
						}
						else {
							v1[0]= speedfac*dz1[0]+jfx;			v1[1]= speedfac*dz1[1]+jfy;			v1[2]= *dz;
							v2[0]= speedfac*dz1[4]+jfx+1.0f;		v2[1]= speedfac*dz1[5]+jfy;			v2[2]= *dz;
							v3[0]= speedfac*dz2[4]+jfx+1.0f;		v3[1]= speedfac*dz2[5]+jfy+1.0f;		v3[2]= *dz;
							v4[0]= speedfac*dz2[0]+jfx;			v4[1]= speedfac*dz2[1]+jfy+1.0f;		v4[2]= *dz;
						}
						if (*dm==255) col.alpha= 1.0f;
						else if (*dm<2) col.alpha= 0.0f;
						else col.alpha= ((float)*dm)/255.0f;
						col.colpoin= dimg;

						zbuf_fill_in_rgba(&zspan, &col, v1, v2, v3, v4);
					}
				}
				dz1+=4;
				dz2+=4;
			}

			/* blend with a falloff. this fixes the ugly effect you get with
			 * a fast moving object. then it looks like a solid object overlayed
			 * over a very transparent moving version of itself. in reality, the
			 * whole object should become transparent if it is moving fast, be
			 * we don't know what is behind it so we don't do that. this hack
			 * overestimates the contribution of foreground pixels but looks a
			 * bit better without a sudden cutoff. */
			blendfac= ((samples - step)/(float)samples);
			/* smoothstep to make it look a bit nicer as well */
			blendfac= 3.0f*pow(blendfac, 2.0f) - 2.0f*pow(blendfac, 3.0f);

			/* accum */
			rw= rectweight;
			rm= rectmax;
			for (dr= rectdraw, dz2=newrect, x= xsize*ysize-1; x>=0; x--, dr++, dz2+=4, rw++, rm++) {
				if (dr->colpoin) {
					float bfac= dr->alpha*blendfac;
					
					dz2[0] += bfac*dr->colpoin[0];
					dz2[1] += bfac*dr->colpoin[1];
					dz2[2] += bfac*dr->colpoin[2];
					dz2[3] += bfac*dr->colpoin[3];

					*rw += bfac;
					*rm= MAX2(*rm, bfac);
				}
			}
		}
	}
	
	/* blend between original images and accumulated image */
	rw= rectweight;
	rm= rectmax;
	ro= imgrect;
	dm= rectmove;
	for (dz2=newrect, x= xsize*ysize-1; x>=0; x--, dz2+=4, ro+=4, rw++, rm++, dm++) {
		float mfac = *rm;
		float fac = (*rw == 0.0f)? 0.0f: mfac/(*rw);
		float nfac = 1.0f - mfac;

		dz2[0]= fac*dz2[0] + nfac*ro[0];
		dz2[1]= fac*dz2[1] + nfac*ro[1];
		dz2[2]= fac*dz2[2] + nfac*ro[2];
		dz2[3]= fac*dz2[3] + nfac*ro[3];
	}

	MEM_freeN(rectz);
	MEM_freeN(rectmove);
	MEM_freeN(rectdraw);
	MEM_freeN(rectvz);
	MEM_freeN(rectweight);
	MEM_freeN(rectmax);
	if (minvecbufrect) MEM_freeN(vecbufrect);  /* rects were swapped! */
	zbuf_free_span(&zspan);
}

/* ******************** ABUF ************************* */

/**
 * Copy results from the solid face z buffering to the transparent
 * buffer.
 */
static void copyto_abufz(RenderPart *pa, int *arectz, int *rectmask, int sample)
{
	PixStr *ps;
	int x, y, *rza, *rma;
	intptr_t *rd;
	
	if (R.osa==0) {
		if (!pa->rectz)
			fillrect(arectz, pa->rectx, pa->recty, 0x7FFFFFFE);
		else
			memcpy(arectz, pa->rectz, sizeof(int)*pa->rectx*pa->recty);

		if (rectmask && pa->rectmask)
			memcpy(rectmask, pa->rectmask, sizeof(int)*pa->rectx*pa->recty);

		return;
	}
	else if (!pa->rectdaps) {
		fillrect(arectz, pa->rectx, pa->recty, 0x7FFFFFFE);
		return;
	}
	
	rza= arectz;
	rma= rectmask;
	rd= pa->rectdaps;

	sample= (1<<sample);
	
	for (y=0; y<pa->recty; y++) {
		for (x=0; x<pa->rectx; x++) {
			
			*rza= 0x7FFFFFFF;
			if (rectmask) *rma= 0x7FFFFFFF;
			if (*rd) {	
				/* when there's a sky pixstruct, fill in sky-Z, otherwise solid Z */
				for (ps= (PixStr *)(*rd); ps; ps= ps->next) {
					if (sample & ps->mask) {
						*rza= ps->z;
						if (rectmask) *rma= ps->maskz;
						break;
					}
				}
			}
			
			rd++; rza++, rma++;
		}
	}
}


/* ------------------------------------------------------------------------ */

/**
 * Do accumulation z buffering.
 */

static int zbuffer_abuf(Render *re, RenderPart *pa, APixstr *APixbuf, ListBase *apsmbase, unsigned int lay, int negzmask, float winmat[][4], int winx, int winy, int samples, float (*jit)[2], float UNUSED(clipcrop), int shadow)
{
	ZbufProjectCache cache[ZBUF_PROJECT_CACHE_SIZE];
	ZSpan zspans[16], *zspan;	/* MAX_OSA */
	Material *ma=NULL;
	ObjectInstanceRen *obi;
	ObjectRen *obr;
	VlakRen *vlr=NULL;
	VertRen *v1, *v2, *v3, *v4;
	float vec[3], hoco[4], mul, zval, fval;
	float obwinmat[4][4], bounds[4], ho1[4], ho2[4], ho3[4], ho4[4]={0};
	int i, v, zvlnr, c1, c2, c3, c4=0, dofill= 0;
	int zsample, polygon_offset;

	zbuffer_part_bounds(winx, winy, pa, bounds);

	for (zsample=0; zsample<samples; zsample++) {
		zspan= &zspans[zsample];

		zbuf_alloc_span(zspan, pa->rectx, pa->recty, re->clipcrop);
		
		/* needed for transform from hoco to zbuffer co */
		zspan->zmulx=  ((float)winx)/2.0f;
		zspan->zmuly=  ((float)winy)/2.0f;
		
		/* the buffers */
		zspan->arectz= MEM_mallocN(sizeof(int)*pa->rectx*pa->recty, "Arectz");
		zspan->apixbuf= APixbuf;
		zspan->apsmbase= apsmbase;
		
		if (negzmask)
			zspan->rectmask= MEM_mallocN(sizeof(int)*pa->rectx*pa->recty, "Arectmask");

		/* filling methods */
		zspan->zbuffunc= zbuffillAc4;
		zspan->zbuflinefunc= zbuflineAc;

		copyto_abufz(pa, zspan->arectz, zspan->rectmask, zsample);	/* init zbuffer */
		zspan->mask= 1<<zsample;

		if (jit) {
			zspan->zofsx= -pa->disprect.xmin - jit[zsample][0];
			zspan->zofsy= -pa->disprect.ymin - jit[zsample][1];
		}
		else {
			zspan->zofsx= -pa->disprect.xmin;
			zspan->zofsy= -pa->disprect.ymin;
		}

		/* to center the sample position */
		zspan->zofsx -= 0.5f;
		zspan->zofsy -= 0.5f;
	}
	
	/* we use this to test if nothing was filled in */
	zvlnr= 0;
		
	for (i=0, obi=re->instancetable.first; obi; i++, obi=obi->next) {
		obr= obi->obr;

		if (!(obi->lay & lay))
			continue;

		if (obi->flag & R_TRANSFORMED)
			mult_m4_m4m4(obwinmat, winmat, obi->mat);
		else
			copy_m4_m4(obwinmat, winmat);

		if (clip_render_object(obi->obr->boundbox, bounds, obwinmat))
			continue;

		zbuf_project_cache_clear(cache, obr->totvert);

		for (v=0; v<obr->totvlak; v++) {
			if ((v & 255)==0)
				vlr= obr->vlaknodes[v>>8].vlak;
			else vlr++;
			
			if (vlr->mat!=ma) {
				ma= vlr->mat;
				if (shadow)
					dofill= (ma->mode & MA_SHADBUF);
				else
					dofill= (((ma->mode & MA_TRANSP) && (ma->mode & MA_ZTRANSP)) && !(ma->mode & MA_ONLYCAST));
			}
			
			if (dofill) {
				if (!(vlr->flag & R_HIDDEN) && (obi->lay & lay)) {
					unsigned short partclip;
					
					v1= vlr->v1;
					v2= vlr->v2;
					v3= vlr->v3;
					v4= vlr->v4;

					c1= zbuf_part_project(cache, v1->index, obwinmat, bounds, v1->co, ho1);
					c2= zbuf_part_project(cache, v2->index, obwinmat, bounds, v2->co, ho2);
					c3= zbuf_part_project(cache, v3->index, obwinmat, bounds, v3->co, ho3);

					/* partclipping doesn't need viewplane clipping */
					partclip= c1 & c2 & c3;
					if (v4) {
						c4= zbuf_part_project(cache, v4->index, obwinmat, bounds, v4->co, ho4);
						partclip &= c4;
					}

					if (partclip==0) {
						/* a little advantage for transp rendering (a z offset) */
						if (!shadow && ma->zoffs != 0.0f) {
							mul= 0x7FFFFFFF;
							zval= mul*(1.0f+ho1[2]/ho1[3]);

							copy_v3_v3(vec, v1->co);
							/* z is negative, otherwise its being clipped */ 
							vec[2]-= ma->zoffs;
							projectverto(vec, obwinmat, hoco);
							fval= mul*(1.0f+hoco[2]/hoco[3]);

							polygon_offset= (int) fabs(zval - fval );
						}
						else polygon_offset= 0;
						
						zvlnr= v+1;

						c1= testclip(ho1);
						c2= testclip(ho2);
						c3= testclip(ho3);
						if (v4)
							c4= testclip(ho4);

						for (zsample=0; zsample<samples; zsample++) {
							zspan= &zspans[zsample];
							zspan->polygon_offset= polygon_offset;
				
							if (ma->material_type == MA_TYPE_WIRE) {
								if (v4)
									zbufclipwire(zspan, i, zvlnr, vlr->ec, ho1, ho2, ho3, ho4, c1, c2, c3, c4);
								else
									zbufclipwire(zspan, i, zvlnr, vlr->ec, ho1, ho2, ho3, 0, c1, c2, c3, 0);
							}
							else {
								if (v4 && (vlr->flag & R_STRAND)) {
									zbufclip4(zspan, i, zvlnr, ho1, ho2, ho3, ho4, c1, c2, c3, c4);
								}
								else {
									zbufclip(zspan, i, zvlnr, ho1, ho2, ho3, c1, c2, c3);
									if (v4)
										zbufclip(zspan, i, zvlnr+RE_QUAD_OFFS, ho1, ho3, ho4, c1, c3, c4);
								}
							}
						}
					}
					if ((v & 255)==255) 
						if (re->test_break(re->tbh)) 
							break; 
				}
			}
		}

		if (re->test_break(re->tbh)) break;
	}
	
	for (zsample=0; zsample<samples; zsample++) {
		zspan= &zspans[zsample];
		MEM_freeN(zspan->arectz);
		if (zspan->rectmask)
			MEM_freeN(zspan->rectmask);
		zbuf_free_span(zspan);
	}
	
	return zvlnr;
}

static int zbuffer_abuf_render(RenderPart *pa, APixstr *APixbuf, APixstrand *APixbufstrand, ListBase *apsmbase, RenderLayer *rl, StrandShadeCache *sscache)
{
	float winmat[4][4], (*jit)[2];
	int samples, negzmask, doztra= 0;

	samples= (R.osa)? R.osa: 1;
	negzmask= ((rl->layflag & SCE_LAY_ZMASK) && (rl->layflag & SCE_LAY_NEG_ZMASK));

	if (R.osa)
		jit= R.jit;
	else if (R.i.curblur)
		jit= &R.mblur_jit[R.i.curblur-1];
	else
		jit= NULL;
	
	zbuf_make_winmat(&R, winmat);

	if (rl->layflag & SCE_LAY_ZTRA)
		doztra+= zbuffer_abuf(&R, pa, APixbuf, apsmbase, rl->lay, negzmask, winmat, R.winx, R.winy, samples, jit, R.clipcrop, 0);
	if ((rl->layflag & SCE_LAY_STRAND) && APixbufstrand)
		doztra+= zbuffer_strands_abuf(&R, pa, APixbufstrand, apsmbase, rl->lay, negzmask, winmat, R.winx, R.winy, samples, jit, R.clipcrop, 0, sscache);

	return doztra;
}

void zbuffer_abuf_shadow(Render *re, LampRen *lar, float winmat[][4], APixstr *APixbuf, APixstrand *APixbufstrand, ListBase *apsmbase, int size, int samples, float (*jit)[2])
{
	RenderPart pa;
	int lay= -1;

	if (lar->mode & LA_LAYER) lay= lar->lay;

	memset(&pa, 0, sizeof(RenderPart));
	pa.rectx= size;
	pa.recty= size;
	pa.disprect.xmin = 0;
	pa.disprect.ymin = 0;
	pa.disprect.xmax = size;
	pa.disprect.ymax = size;

	zbuffer_abuf(re, &pa, APixbuf, apsmbase, lay, 0, winmat, size, size, samples, jit, 1.0f, 1);
	if (APixbufstrand)
		zbuffer_strands_abuf(re, &pa, APixbufstrand, apsmbase, lay, 0, winmat, size, size, samples, jit, 1.0f, 1, NULL);
}

/* different rules for speed in transparent pass...  */
/* speed pointer NULL = sky, we clear */
/* else if either alpha is full or no solid was filled in: copy speed */
/* else fill in minimum speed */
void add_transp_speed(RenderLayer *rl, int offset, float *speed, float alpha, intptr_t *rdrect)
{
	RenderPass *rpass;
	
	for (rpass= rl->passes.first; rpass; rpass= rpass->next) {
		if (rpass->passtype==SCE_PASS_VECTOR) {
			float *fp= rpass->rect + 4*offset;
			
			if (speed==NULL) {
				/* clear */
				if (fp[0]==PASS_VECTOR_MAX) fp[0]= 0.0f;
				if (fp[1]==PASS_VECTOR_MAX) fp[1]= 0.0f;
				if (fp[2]==PASS_VECTOR_MAX) fp[2]= 0.0f;
				if (fp[3]==PASS_VECTOR_MAX) fp[3]= 0.0f;
			}
			else if (rdrect==NULL || rdrect[offset]==0 || alpha>0.95f) {
				copy_v4_v4(fp, speed);
			}
			else {
				/* add minimum speed in pixel */
				if ( (ABS(speed[0]) + ABS(speed[1]))< (ABS(fp[0]) + ABS(fp[1])) ) {
					fp[0]= speed[0];
					fp[1]= speed[1];
				}
				if ( (ABS(speed[2]) + ABS(speed[3]))< (ABS(fp[2]) + ABS(fp[3])) ) {
					fp[2]= speed[2];
					fp[3]= speed[3];
				}
			}
			break;
		}
	}
}

static void add_transp_obindex(RenderLayer *rl, int offset, Object *ob)
{
	RenderPass *rpass;
	
	for (rpass= rl->passes.first; rpass; rpass= rpass->next) {
		if (rpass->passtype == SCE_PASS_INDEXOB||rpass->passtype == SCE_PASS_INDEXMA) {
			float *fp= rpass->rect + offset;
			*fp= (float)ob->index;
			break;
		}
	}
}

/* ONLY OSA! merge all shaderesult samples to one */
/* target should have been cleared */
void merge_transp_passes(RenderLayer *rl, ShadeResult *shr)
{
	RenderPass *rpass;
	float weight= 1.0f/((float)R.osa);
	int delta= sizeof(ShadeResult)/4;
	
	for (rpass= rl->passes.first; rpass; rpass= rpass->next) {
		float *col= NULL;
		int pixsize= 3;
		
		switch (rpass->passtype) {
			case SCE_PASS_RGBA:
				col= shr->col;
				pixsize= 4;
				break;
			case SCE_PASS_EMIT:
				col= shr->emit;
				break;
			case SCE_PASS_DIFFUSE:
				col= shr->diff;
				break;
			case SCE_PASS_SPEC:
				col= shr->spec;
				break;
			case SCE_PASS_SHADOW:
				col= shr->shad;
				break;
			case SCE_PASS_AO:
				col= shr->ao;
				break;
			case SCE_PASS_ENVIRONMENT:
				col= shr->env;
				break;
			case SCE_PASS_INDIRECT:
				col= shr->indirect;
				break;
			case SCE_PASS_REFLECT:
				col= shr->refl;
				break;
			case SCE_PASS_REFRACT:
				col= shr->refr;
				break;
			case SCE_PASS_NORMAL:
				col= shr->nor;
				break;
			case SCE_PASS_MIST:
				col= &shr->mist;
				pixsize= 1;
				break;
			case SCE_PASS_Z:
				col= &shr->z;
				pixsize= 1;
				break;
			case SCE_PASS_VECTOR:
				
				{
					ShadeResult *shr_t= shr+1;
					float *fp= shr->winspeed;	/* was initialized */
					int samp;
					
					/* add minimum speed in pixel */
					for (samp= 1; samp<R.osa; samp++, shr_t++) {
						
						if (shr_t->combined[3] > 0.0f) {
							float *speed= shr_t->winspeed;
							
							if ( (ABS(speed[0]) + ABS(speed[1]))< (ABS(fp[0]) + ABS(fp[1])) ) {
								fp[0]= speed[0];
								fp[1]= speed[1];
							}
							if ( (ABS(speed[2]) + ABS(speed[3]))< (ABS(fp[2]) + ABS(fp[3])) ) {
								fp[2]= speed[2];
								fp[3]= speed[3];
							}
						}
					}
				}
				break;
		}
		if (col) {
			float *fp= col+delta;
			int samp;
			
			for (samp= 1; samp<R.osa; samp++, fp+=delta) {
				col[0]+= fp[0];
				if (pixsize>1) {
					col[1]+= fp[1];
					col[2]+= fp[2];
					if (pixsize==4) col[3]+= fp[3];
				}
			}
			col[0]*= weight;
			if (pixsize>1) {
				col[1]*= weight;
				col[2]*= weight;
				if (pixsize==4) col[3]*= weight;
			}
		}
	}
				
}

void add_transp_passes(RenderLayer *rl, int offset, ShadeResult *shr, float alpha)
{
	RenderPass *rpass;
	
	for (rpass= rl->passes.first; rpass; rpass= rpass->next) {
		float *fp, *col= NULL;
		int pixsize= 3;
		
		switch (rpass->passtype) {
			case SCE_PASS_Z:
				fp= rpass->rect + offset;
				if (shr->z < *fp)
					*fp= shr->z;
				break;
			case SCE_PASS_RGBA:
				fp= rpass->rect + 4*offset;
				addAlphaOverFloat(fp, shr->col);
				break;
			case SCE_PASS_EMIT:
				col= shr->emit;
				break;
			case SCE_PASS_DIFFUSE:
				col= shr->diff;
				break;
			case SCE_PASS_SPEC:
				col= shr->spec;
				break;
			case SCE_PASS_SHADOW:
				col= shr->shad;
				break;
			case SCE_PASS_AO:
				col= shr->ao;
				break;
			case SCE_PASS_ENVIRONMENT:
				col= shr->env;
				break;
			case SCE_PASS_INDIRECT:
				col= shr->indirect;
				break;
			case SCE_PASS_REFLECT:
				col= shr->refl;
				break;
			case SCE_PASS_REFRACT:
				col= shr->refr;
				break;
			case SCE_PASS_NORMAL:
				col= shr->nor;
				break;
			case SCE_PASS_MIST:
				col= &shr->mist;
				pixsize= 1;
				break;
		}
		if (col) {

			fp= rpass->rect + pixsize*offset;
			fp[0]= col[0] + (1.0f-alpha)*fp[0];
			if (pixsize==3) {
				fp[1]= col[1] + (1.0f-alpha)*fp[1];
				fp[2]= col[2] + (1.0f-alpha)*fp[2];
			}
		}
	}
}

typedef struct ZTranspRow {
	int obi;
	int z;
	int p;
	int mask;
	int segment;
	float u, v;
} ZTranspRow;

static int vergzvlak(const void *a1, const void *a2)
{
	const ZTranspRow *r1 = a1, *r2 = a2;

	if (r1->z < r2->z) return 1;
	else if (r1->z > r2->z) return -1;
	return 0;
}

static void shade_strand_samples(StrandShadeCache *cache, ShadeSample *ssamp, int UNUSED(x), int UNUSED(y), ZTranspRow *row, int addpassflag)
{
	StrandSegment sseg;
	StrandVert *svert;
	ObjectInstanceRen *obi;
	ObjectRen *obr;

	obi= R.objectinstance + row->obi;
	obr= obi->obr;

	sseg.obi= obi;
	sseg.strand= RE_findOrAddStrand(obr, row->p-1);
	sseg.buffer= sseg.strand->buffer;

	svert= sseg.strand->vert + row->segment;
	sseg.v[0]= (row->segment > 0)? (svert-1): svert;
	sseg.v[1]= svert;
	sseg.v[2]= svert+1;
	sseg.v[3]= (row->segment < sseg.strand->totvert-2)? svert+2: svert+1;

	ssamp->tot= 1;
	strand_shade_segment(&R, cache, &sseg, ssamp, row->v, row->u, addpassflag);
	ssamp->shi[0].mask= row->mask;
}

static void unref_strand_samples(StrandShadeCache *cache, ZTranspRow *row, int totface)
{
	StrandVert *svert;
	ObjectInstanceRen *obi;
	ObjectRen *obr;
	StrandRen *strand;

	/* remove references to samples that are not being rendered, but we still
	 * need to remove them so that the reference count of strand vertex shade
	 * samples correctly drops to zero */
	while (totface > 0) {
		totface--;

		if (row[totface].segment != -1) {
			obi= R.objectinstance + row[totface].obi;
			obr= obi->obr;
			strand= RE_findOrAddStrand(obr, row[totface].p-1);
			svert= strand->vert + row[totface].segment;

			strand_shade_unref(cache, obi, svert);
			strand_shade_unref(cache, obi, svert+1);
		}
	}
}

static void shade_tra_samples_fill(ShadeSample *ssamp, int x, int y, int z, int obi, int facenr, int curmask)
{
	ShadeInput *shi= ssamp->shi;
	float xs, ys;
	
	ssamp->tot= 0;

	shade_input_set_triangle(shi, obi, facenr, 1);
		
	/* officially should always be true... we have no sky info */
	if (shi->vlr) {
		
		/* full osa is only set for OSA renders */
		if (shi->vlr->flag & R_FULL_OSA) {
			short shi_inc= 0, samp;
			
			for (samp=0; samp<R.osa; samp++) {
				if (curmask & (1<<samp)) {
					xs= (float)x + R.jit[samp][0] + 0.5f;	/* zbuffer has this inverse corrected, ensures (xs, ys) are inside pixel */
					ys= (float)y + R.jit[samp][1] + 0.5f;
					
					if (shi_inc) {
						shade_input_copy_triangle(shi+1, shi);
						shi++;
					}
					shi->mask= (1<<samp);
					shi->samplenr= R.shadowsamplenr[shi->thread]++;
					shade_input_set_viewco(shi, x, y, xs, ys, (float)z);
					shade_input_set_uv(shi);
					if (shi_inc==0)
						shade_input_set_normals(shi);
					else /* XXX shi->flippednor messes up otherwise */
						shade_input_set_vertex_normals(shi);
					
					shi_inc= 1;
				}
			}
		}
		else {
			if (R.osa) {
				short b= R.samples->centmask[curmask];
				xs= (float)x + R.samples->centLut[b & 15] + 0.5f;
				ys= (float)y + R.samples->centLut[b>>4] + 0.5f;
			}
			else {
				xs= (float)x + 0.5f;
				ys= (float)y + 0.5f;
			}
			shi->mask= curmask;
			shi->samplenr= R.shadowsamplenr[shi->thread]++;
			shade_input_set_viewco(shi, x, y, xs, ys, (float)z);
			shade_input_set_uv(shi);
			shade_input_set_normals(shi);
		}
		
		/* total sample amount, shi->sample is static set in initialize */
		ssamp->tot= shi->sample+1;
	}
}

static int shade_tra_samples(ShadeSample *ssamp, StrandShadeCache *cache, int x, int y, ZTranspRow *row, int addpassflag)
{
	if (row->segment != -1) {
		shade_strand_samples(cache, ssamp, x, y, row, addpassflag);
		return 1;
	}

	shade_tra_samples_fill(ssamp, x, y, row->z, row->obi, row->p, row->mask);
	
	if (ssamp->tot) {
		ShadeInput *shi= ssamp->shi;
		ShadeResult *shr= ssamp->shr;
		int samp;
		
		/* if AO? */
		shade_samples_do_AO(ssamp);
		
		/* if shade (all shadepinputs have same passflag) */
		if (shi->passflag & ~(SCE_PASS_Z|SCE_PASS_INDEXOB|SCE_PASS_INDEXMA)) {
			for (samp=0; samp<ssamp->tot; samp++, shi++, shr++) {
				shade_input_set_shade_texco(shi);
				shade_input_do_shade(shi, shr);
				
				/* include lamphalos for ztra, since halo layer was added already */
				if (R.flag & R_LAMPHALO)
					if (shi->layflag & SCE_LAY_HALO)
						renderspothalo(shi, shr->combined, shr->combined[3]);
			}
		}
		else if (shi->passflag & SCE_PASS_Z) {
			for (samp=0; samp<ssamp->tot; samp++, shi++, shr++)
				shr->z= -shi->co[2];
		}

		return 1;
	}
	return 0;
}

static int addtosamp_shr(ShadeResult *samp_shr, ShadeSample *ssamp, int addpassflag)
{
	int a, sample, osa = (R.osa? R.osa: 1), retval = osa;
	
	for (a=0; a < osa; a++, samp_shr++) {
		ShadeInput *shi= ssamp->shi;
		ShadeResult *shr= ssamp->shr;
		
		for (sample=0; sample<ssamp->tot; sample++, shi++, shr++) {
		
			if (shi->mask & (1<<a)) {
				float fac= (1.0f - samp_shr->combined[3])*shr->combined[3];
				
				addAlphaUnderFloat(samp_shr->combined, shr->combined);
				
				samp_shr->z= MIN2(samp_shr->z, shr->z);

				if (addpassflag & SCE_PASS_VECTOR) {
					copy_v4_v4(samp_shr->winspeed, shr->winspeed);
				}
				/* optim... */
				if (addpassflag & ~(SCE_PASS_VECTOR)) {
					
					if (addpassflag & SCE_PASS_RGBA)
						addAlphaUnderFloat(samp_shr->col, shr->col);
					
					if (addpassflag & SCE_PASS_NORMAL)
						madd_v3_v3fl(samp_shr->nor, shr->nor, fac);

					if (addpassflag & SCE_PASS_EMIT)
						madd_v3_v3fl(samp_shr->emit, shr->emit, fac);

					if (addpassflag & SCE_PASS_DIFFUSE)
						madd_v3_v3fl(samp_shr->diff, shr->diff, fac);
					
					if (addpassflag & SCE_PASS_SPEC)
						madd_v3_v3fl(samp_shr->spec, shr->spec, fac);

					if (addpassflag & SCE_PASS_SHADOW)
						madd_v3_v3fl(samp_shr->shad, shr->shad, fac);

					if (addpassflag & SCE_PASS_AO)
						madd_v3_v3fl(samp_shr->ao, shr->ao, fac);

					if (addpassflag & SCE_PASS_ENVIRONMENT)
						madd_v3_v3fl(samp_shr->env, shr->env, fac);

					if (addpassflag & SCE_PASS_INDIRECT)
						madd_v3_v3fl(samp_shr->indirect, shr->indirect, fac);

					if (addpassflag & SCE_PASS_REFLECT)
						madd_v3_v3fl(samp_shr->refl, shr->refl, fac);
					
					if (addpassflag & SCE_PASS_REFRACT)
						madd_v3_v3fl(samp_shr->refr, shr->refr, fac);
					
					if (addpassflag & SCE_PASS_MIST)
						samp_shr->mist= samp_shr->mist+fac*shr->mist;

				}
			}
		}
		
		if (samp_shr->combined[3]>0.999f) retval--;
	}
	return retval;
}

static void reset_sky_speedvectors(RenderPart *pa, RenderLayer *rl, float *rectf)
{
	/* speed vector exception... if solid render was done, sky pixels are set to zero already */
	/* for all pixels with alpha zero, we re-initialize speed again then */
	float *fp, *col;
	int a;
	
	fp= RE_RenderLayerGetPass(rl, SCE_PASS_VECTOR);
	if (fp==NULL) return;
	col= rectf+3;
	
	for (a= 4*pa->rectx*pa->recty -4; a>=0; a-=4) {
		if (col[a]==0.0f) {
			fp[a]= PASS_VECTOR_MAX;
			fp[a+1]= PASS_VECTOR_MAX;
			fp[a+2]= PASS_VECTOR_MAX;
			fp[a+3]= PASS_VECTOR_MAX;
		}
	}
}

#define MAX_ZROW	2000

/* main render call to do the z-transparent layer */
/* returns a mask, only if a) transp rendered and b) solid was rendered */
unsigned short *zbuffer_transp_shade(RenderPart *pa, RenderLayer *rl, float *pass, ListBase *UNUSED(psmlist))
{
	RenderResult *rr= pa->result;
	ShadeSample ssamp;
	APixstr *APixbuf;      /* Zbuffer: linked list of face samples */
	APixstrand *APixbufstrand = NULL;
	APixstr *ap, *aprect, *apn;
	APixstrand *apstrand, *aprectstrand, *apnstrand;
	ListBase apsmbase={NULL, NULL};
	ShadeResult samp_shr[16];		/* MAX_OSA */
	ZTranspRow zrow[MAX_ZROW];
	StrandShadeCache *sscache= NULL;
	RenderLayer *rlpp[RE_MAX_OSA];
	float sampalpha, alpha, *passrect= pass;
	intptr_t *rdrect;
	int x, y, crop=0, a, b, totface, totfullsample, totsample, doztra;
	int addpassflag, offs= 0, od, osa = (R.osa? R.osa: 1);
	unsigned short *ztramask= NULL, filled;

	/* looks nicer for calling code */
	if (R.test_break(R.tbh))
		return NULL;
	
	if (R.osa>16) { /* MAX_OSA */
		printf("zbuffer_transp_shade: osa too large\n");
		G.afbreek= 1;
		return NULL;
	}
	
	APixbuf= MEM_callocN(pa->rectx*pa->recty*sizeof(APixstr), "APixbuf");
	if (R.totstrand && (rl->layflag & SCE_LAY_STRAND)) {
		APixbufstrand= MEM_callocN(pa->rectx*pa->recty*sizeof(APixstrand), "APixbufstrand");
		sscache= strand_shade_cache_create();
	}

	/* general shader info, passes */
	shade_sample_initialize(&ssamp, pa, rl);
	addpassflag= rl->passflag & ~(SCE_PASS_COMBINED);
	
	if (R.osa)
		sampalpha= 1.0f/(float)R.osa;
	else
		sampalpha= 1.0f;
	
	/* fill the Apixbuf */
	doztra= zbuffer_abuf_render(pa, APixbuf, APixbufstrand, &apsmbase, rl, sscache);

	if (doztra == 0) {
		/* nothing filled in */
		MEM_freeN(APixbuf);
		if (APixbufstrand)
			MEM_freeN(APixbufstrand);
		if (sscache)
			strand_shade_cache_free(sscache);
		freepsA(&apsmbase);
		return NULL;
	}

	aprect= APixbuf;
	aprectstrand= APixbufstrand;
	rdrect= pa->rectdaps;

	/* needed for correct zbuf/index pass */
	totfullsample= get_sample_layers(pa, rl, rlpp);
	
	/* irregular shadowb buffer creation */
	if (R.r.mode & R_SHADOW)
		ISB_create(pa, APixbuf);

	/* masks, to have correct alpha combine */
	if (R.osa && (rl->layflag & SCE_LAY_SOLID) && pa->fullresult.first==NULL)
		ztramask= MEM_callocN(pa->rectx*pa->recty*sizeof(short), "ztramask");

	/* zero alpha pixels get speed vector max again */
	if (addpassflag & SCE_PASS_VECTOR)
		if (rl->layflag & SCE_LAY_SOLID)
			reset_sky_speedvectors(pa, rl, rl->acolrect?rl->acolrect:rl->rectf);	/* if acolrect is set we use it */

	/* filtered render, for now we assume only 1 filter size */
	if (pa->crop) {
		crop= 1;
		offs= pa->rectx + 1;
		passrect+= 4*offs;
		aprect+= offs;
		aprectstrand+= offs;
	}
	
	/* init scanline updates */
	rr->renrect.ymin = 0;
	rr->renrect.ymax = -pa->crop;
	rr->renlay= rl;
				
	/* render the tile */
	for (y=pa->disprect.ymin+crop; y<pa->disprect.ymax-crop; y++, rr->renrect.ymax++) {
		pass= passrect;
		ap= aprect;
		apstrand= aprectstrand;
		od= offs;
		
		if (R.test_break(R.tbh))
			break;
		
		for (x=pa->disprect.xmin+crop; x<pa->disprect.xmax-crop; x++, ap++, apstrand++, pass+=4, od++) {
			
			if (ap->p[0]==0 && (!APixbufstrand || apstrand->p[0]==0)) {
				if (addpassflag & SCE_PASS_VECTOR) 
					add_transp_speed(rl, od, NULL, 0.0f, rdrect);
			}
			else {
				/* sort in z */
				totface= 0;
				apn= ap;
				while (apn) {
					for (a=0; a<4; a++) {
						if (apn->p[a]) {
							zrow[totface].obi= apn->obi[a];
							zrow[totface].z= apn->z[a];
							zrow[totface].p= apn->p[a];
							zrow[totface].mask= apn->mask[a];
							zrow[totface].segment= -1;
							totface++;
							if (totface>=MAX_ZROW) totface= MAX_ZROW-1;
						}
						else break;
					}
					apn= apn->next;
				}

				apnstrand= (APixbufstrand)? apstrand: NULL;
				while (apnstrand) {
					for (a=0; a<4; a++) {
						if (apnstrand->p[a]) {
							zrow[totface].obi= apnstrand->obi[a];
							zrow[totface].z= apnstrand->z[a];
							zrow[totface].p= apnstrand->p[a];
							zrow[totface].mask= apnstrand->mask[a];
							zrow[totface].segment= apnstrand->seg[a];

							if (R.osa) {
								totsample= 0;
								for (b=0; b<R.osa; b++)
									if (zrow[totface].mask & (1<<b))
										totsample++;
							}
							else
								totsample= 1;

							zrow[totface].u= apnstrand->u[a]/totsample;
							zrow[totface].v= apnstrand->v[a]/totsample;
							totface++;
							if (totface>=MAX_ZROW) totface= MAX_ZROW-1;
						}
					}
					apnstrand= apnstrand->next;
				}

				if (totface==2) {
					if (zrow[0].z < zrow[1].z) {
						SWAP(ZTranspRow, zrow[0], zrow[1]);
					}
					
				}
				else if (totface>2) {
					qsort(zrow, totface, sizeof(ZTranspRow), vergzvlak);
				}
				
				/* front face does index pass for transparent, no AA or filters, but yes FSA */
				if (addpassflag & SCE_PASS_INDEXOB) {
					ObjectRen *obr= R.objectinstance[zrow[totface-1].obi].obr;
					if (obr->ob) {
						for (a= 0; a<totfullsample; a++)
							add_transp_obindex(rlpp[a], od, obr->ob);
					}
				}
				if (addpassflag & SCE_PASS_INDEXMA) {
					ObjectRen *obr= R.objectinstance[zrow[totface-1].obi].obr;
					if (obr->ob) {
						for (a= 0; a<totfullsample; a++)
							add_transp_obindex(rlpp[a], od, obr->ob);
					}
				}

				/* for each mask-sample we alpha-under colors. then in end it's added using filter */
				memset(samp_shr, 0, sizeof(ShadeResult)*osa);
				for (a=0; a<osa; a++) {
					samp_shr[a].z= 10e10f;
					if (addpassflag & SCE_PASS_VECTOR) {
						samp_shr[a].winspeed[0]= PASS_VECTOR_MAX;
						samp_shr[a].winspeed[1]= PASS_VECTOR_MAX;
						samp_shr[a].winspeed[2]= PASS_VECTOR_MAX;
						samp_shr[a].winspeed[3]= PASS_VECTOR_MAX;
					}
				}

				if (R.osa==0) {
					while (totface>0) {
						totface--;
						
						if (shade_tra_samples(&ssamp, sscache, x, y, &zrow[totface], addpassflag)) {
							filled= addtosamp_shr(samp_shr, &ssamp, addpassflag);
							addAlphaUnderFloat(pass, ssamp.shr[0].combined);
							
							if (filled == 0) {
								if (sscache)
									unref_strand_samples(sscache, zrow, totface);
								break;
							}
						}
					}

					alpha= samp_shr->combined[3];
					if (alpha!=0.0f) {
						add_transp_passes(rl, od, samp_shr, alpha);
						if (addpassflag & SCE_PASS_VECTOR)
							add_transp_speed(rl, od, samp_shr->winspeed, alpha, rdrect);
					}
				}
				else {
					short *sp= (short *)(ztramask+od);
					
					while (totface>0) {
						totface--;
						
						if (shade_tra_samples(&ssamp, sscache, x, y, &zrow[totface], addpassflag)) {
							filled= addtosamp_shr(samp_shr, &ssamp, addpassflag);
							
							if (ztramask)
								*sp |= zrow[totface].mask;
							if (filled==0) {
								if (sscache)
									unref_strand_samples(sscache, zrow, totface);
								break;
							}
						}
					}
					
					/* multisample buffers or filtered mask filling? */
					if (pa->fullresult.first) {
						for (a=0; a<R.osa; a++) {
							alpha= samp_shr[a].combined[3];
							if (alpha!=0.0f) {
								RenderLayer *rl= ssamp.rlpp[a];
								
								addAlphaOverFloat(rl->rectf + 4*od, samp_shr[a].combined);
				
								add_transp_passes(rl, od, &samp_shr[a], alpha);
								if (addpassflag & SCE_PASS_VECTOR)
									add_transp_speed(rl, od, samp_shr[a].winspeed, alpha, rdrect);
							}
						}
					}
					else {
						alpha= 0.0f;

						/* note; cannot use pass[3] for alpha due to filtermask */
						for (a=0; a<R.osa; a++) {
							add_filt_fmask(1<<a, samp_shr[a].combined, pass, rr->rectx);
							alpha+= samp_shr[a].combined[3];
						}
						
						if (addpassflag) {
							alpha*= sampalpha;
							
							/* merge all in one, and then add */
							merge_transp_passes(rl, samp_shr);
							add_transp_passes(rl, od, samp_shr, alpha);

							if (addpassflag & SCE_PASS_VECTOR)
								add_transp_speed(rl, od, samp_shr[0].winspeed, alpha, rdrect);
						}
					}
				}
			}
		}
		
		aprect+= pa->rectx;
		aprectstrand+= pa->rectx;
		passrect+= 4*pa->rectx;
		offs+= pa->rectx;
	}

	/* disable scanline updating */
	rr->renlay= NULL;

	MEM_freeN(APixbuf);
	if (APixbufstrand)
		MEM_freeN(APixbufstrand);
	if (sscache)
		strand_shade_cache_free(sscache);
	freepsA(&apsmbase);	

	if (R.r.mode & R_SHADOW)
		ISB_free(pa);

	return ztramask;
}


/* end of zbuf.c */
