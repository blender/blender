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
 * The Original Code is Copyright (C) 1990-1998 NeoGeo BV.
 * All rights reserved.
 *
 * The Original Code is: all of this file.
 *
 * ***** END GPL/BL DUAL LICENSE BLOCK *****
 */


#include <math.h>
#include <string.h>
#include <stdlib.h>

#include "MEM_guardedalloc.h"

#include "DNA_material_types.h"
#include "DNA_lamp_types.h"
#include "DNA_mesh_types.h"

#include "BKE_utildefines.h"
#include "BKE_texture.h"

#include "BLI_arithb.h"

#include "render.h"
#include "render_intern.h"
#include "jitter.h"

#define OCRES	64
#define DDA_SHADOW 0
#define DDA_MIRROR 1

/* ********** structs *************** */

typedef struct Octree {
	struct Branch *adrbranch[256];
	struct Node *adrnode[256];
	float ocsize;	/* ocsize: mult factor,  max size octree */
	float ocfacx,ocfacy,ocfacz;
	float min[3], max[3];
	/* for optimize, last intersected face */
	VlakRen *vlr_last;

} Octree;

typedef struct Isect {
	float start[3], end[3];
	float labda, u, v;
	struct VlakRen *vlr, *vlrcontr;
	short isect, mode;	/* mode: DDA_SHADOW or DDA_MIRROR */
	float ddalabda;
} Isect;

typedef struct Branch
{
	struct Branch *b[8];
} Branch;

typedef struct OcVal 
{
	short ocx, ocy, ocz;
} OcVal;

typedef struct Node
{
	struct VlakRen *v[8];
	struct OcVal ov[8];
	struct Node *next;
} Node;


/* ******** globals ***************** */

static Octree g_oc;	/* can be scene pointer or so later... */

/* just for statistics */
static int raycount, branchcount, nodecount;
static int accepted, rejected;


/* **************** ocval method ******************* */
/* within one octree node, a set of 3x15 bits defines a 'boundbox' to OR with */

#define OCVALRES	15
#define BROW(min, max)      (((max)>=OCVALRES? 0xFFFF: (1<<(max+1))-1) - ((min>0)? ((1<<(min))-1):0) )

static void calc_ocval_face(float *v1, float *v2, float *v3, float *v4, short x, short y, short z, OcVal *ov)
{
	float min[3], max[3];
	int ocmin, ocmax;
	
	VECCOPY(min, v1);
	VECCOPY(max, v1);
	DO_MINMAX(v2, min, max);
	DO_MINMAX(v3, min, max);
	if(v4) {
		DO_MINMAX(v4, min, max);
	}
	
	ocmin= OCVALRES*(min[0]-x); 
	ocmax= OCVALRES*(max[0]-x);
	ov->ocx= BROW(ocmin, ocmax);
	
	ocmin= OCVALRES*(min[1]-y); 
	ocmax= OCVALRES*(max[1]-y);
	ov->ocy= BROW(ocmin, ocmax);
	
	ocmin= OCVALRES*(min[2]-z); 
	ocmax= OCVALRES*(max[2]-z);
	ov->ocz= BROW(ocmin, ocmax);

}

static void calc_ocval_ray(OcVal *ov, float x1, float y1, float z1, 
											  float x2, float y2, float z2)
{
	static float ox1, ox2, oy1, oy2, oz1, oz2;
	
	if(ov==NULL) {
		ox1= x1; ox2= x2;
		oy1= y1; oy2= y2;
		oz1= z1; oz2= z2;
	}
	else {
		int ocmin, ocmax;
		
		if(ox1<ox2) {
			ocmin= OCVALRES*(ox1 - ((int)x1));
			ocmax= OCVALRES*(ox2 - ((int)x1));
		} else {
			ocmin= OCVALRES*(ox2 - ((int)x1));
			ocmax= OCVALRES*(ox1 - ((int)x1));
		}
		ov->ocx= BROW(ocmin, ocmax);

		if(oy1<oy2) {
			ocmin= OCVALRES*(oy1 - ((int)y1));
			ocmax= OCVALRES*(oy2 - ((int)y1));
		} else {
			ocmin= OCVALRES*(oy2 - ((int)y1));
			ocmax= OCVALRES*(oy1 - ((int)y1));
		}
		ov->ocy= BROW(ocmin, ocmax);

		if(oz1<oz2) {
			ocmin= OCVALRES*(oz1 - ((int)z1));
			ocmax= OCVALRES*(oz2 - ((int)z1));
		} else {
			ocmin= OCVALRES*(oz2 - ((int)z1));
			ocmax= OCVALRES*(oz1 - ((int)z1));
		}
		ov->ocz= BROW(ocmin, ocmax);
		
	}
}

/* ************* octree ************** */

static Branch *addbranch(Branch *br, short oc)
{
	
	if(br->b[oc]) return br->b[oc];
	
	branchcount++;
	if(g_oc.adrbranch[branchcount>>8]==0)
		g_oc.adrbranch[branchcount>>8]= MEM_callocN(256*sizeof(Branch),"addbranch");

	if(branchcount>= 256*256) {
		printf("error; octree branches full\n");
		branchcount=0;
	}
	
	return br->b[oc]=g_oc.adrbranch[branchcount>>8]+(branchcount & 255);
}

static Node *addnode(void)
{
	
	nodecount++;
	if(g_oc.adrnode[nodecount>>12]==0)
		g_oc.adrnode[nodecount>>12]= MEM_callocN(4096*sizeof(Node),"addnode");

	if(nodecount> 256*4096) {
		printf("error; octree nodes full\n");
		nodecount=0;
	}
	
	return g_oc.adrnode[nodecount>>12]+(nodecount & 4095);
}


static void ocwrite(VlakRen *vlr, short x, short y, short z, float rtf[][3])
{
	Branch *br;
	Node *no;
	short a, oc0, oc1, oc2, oc3, oc4, oc5;

	x<<=2;
	y<<=1;
	oc0= ((x & 128)+(y & 64)+(z & 32))>>5;
	oc1= ((x & 64)+(y & 32)+(z & 16))>>4;
	oc2= ((x & 32)+(y & 16)+(z & 8))>>3;
	oc3= ((x & 16)+(y & 8)+(z & 4))>>2;
	oc4= ((x & 8)+(y & 4)+(z & 2))>>1;
	oc5= ((x & 4)+(y & 2)+(z & 1));

	br= addbranch(g_oc.adrbranch[0],oc0);
	br= addbranch(br,oc1);
	br= addbranch(br,oc2);
	br= addbranch(br,oc3);
	br= addbranch(br,oc4);
	no= (Node *)br->b[oc5];
	if(no==NULL) br->b[oc5]= (Branch *)(no= addnode());

	while(no->next) no= no->next;

	a= 0;
	if(no->v[7]) {		/* node full */
		no->next= addnode();
		no= no->next;
	}
	else {
		while(no->v[a]!=NULL) a++;
	}
	
	no->v[a]= vlr;
	
	calc_ocval_face(rtf[0], rtf[1], rtf[2], rtf[3], x>>2, y>>1, z, &no->ov[a]);

}

static void d2dda(short b1, short b2, short c1, short c2, char *ocvlak, short rts[][3], float rtf[][3])
{
	short ocx1,ocx2,ocy1,ocy2;
	short x,y,dx=0,dy=0;
	float ox1,ox2,oy1,oy2;
	float labda,labdao,labdax,labday,ldx,ldy;

	ocx1= rts[b1][c1];
	ocy1= rts[b1][c2];
	ocx2= rts[b2][c1];
	ocy2= rts[b2][c2];

	if(ocx1==ocx2 && ocy1==ocy2) {
		ocvlak[OCRES*ocx1+ocy1]= 1;
		return;
	}

	ox1= rtf[b1][c1];
	oy1= rtf[b1][c2];
	ox2= rtf[b2][c1];
	oy2= rtf[b2][c2];

	if(ox1!=ox2) {
		if(ox2-ox1>0.0) {
			labdax= (ox1-ocx1-1.0)/(ox1-ox2);
			ldx= -1.0/(ox1-ox2);
			dx= 1;
		} else {
			labdax= (ox1-ocx1)/(ox1-ox2);
			ldx= 1.0/(ox1-ox2);
			dx= -1;
		}
	} else {
		labdax=1.0;
		ldx=0;
	}

	if(oy1!=oy2) {
		if(oy2-oy1>0.0) {
			labday= (oy1-ocy1-1.0)/(oy1-oy2);
			ldy= -1.0/(oy1-oy2);
			dy= 1;
		} else {
			labday= (oy1-ocy1)/(oy1-oy2);
			ldy= 1.0/(oy1-oy2);
			dy= -1;
		}
	} else {
		labday=1.0;
		ldy=0;
	}
	
	x=ocx1; y=ocy1;
	labda= MIN2(labdax, labday);
	
	while(TRUE) {
		
		if(x<0 || y<0 || x>=OCRES || y>=OCRES);
		else ocvlak[OCRES*x+y]= 1;
		
		labdao=labda;
		if(labdax==labday) {
			labdax+=ldx;
			x+=dx;
			labday+=ldy;
			y+=dy;
		} else {
			if(labdax<labday) {
				labdax+=ldx;
				x+=dx;
			} else {
				labday+=ldy;
				y+=dy;
			}
		}
		labda=MIN2(labdax,labday);
		if(labda==labdao) break;
		if(labda>=1.0) break;
	}
	ocvlak[OCRES*ocx2+ocy2]=1;
}

static void filltriangle(short c1, short c2, char *ocvlak, short *ocmin)
{
	short a,x,y,y1,y2,*ocmax;

	ocmax=ocmin+3;

	for(x=ocmin[c1];x<=ocmax[c1];x++) {
		a= OCRES*x;
		for(y=ocmin[c2];y<=ocmax[c2];y++) {
			if(ocvlak[a+y]) {
				y++;
				while(ocvlak[a+y] && y!=ocmax[c2]) y++;
				for(y1=ocmax[c2];y1>y;y1--) {
					if(ocvlak[a+y1]) {
						for(y2=y;y2<=y1;y2++) ocvlak[a+y2]=1;
						y1=0;
					}
				}
				y=ocmax[c2];
			}
		}
	}
}

void freeoctree(void)
{
	int a= 0;
	
 	while(g_oc.adrbranch[a]) {
		MEM_freeN(g_oc.adrbranch[a]);
		g_oc.adrbranch[a]= NULL;
		a++;
	}
	
	a= 0;
	while(g_oc.adrnode[a]) {
		MEM_freeN(g_oc.adrnode[a]);
		g_oc.adrnode[a]= NULL;
		a++;
	}
	
	printf("branches %d nodes %d\n", branchcount, nodecount);
	printf("raycount %d \n", raycount);
//	printf("accepted %d rejected %d\n", accepted, rejected);

	branchcount= 0;
	nodecount= 0;
}

void makeoctree()
{
	VlakRen *vlr=NULL;
	VertRen *v1, *v2, *v3, *v4;
	float ocfac[3], t00, t01, t02;
	float rtf[4][3];
	int v;
	short a,b,c, rts[4][3], oc1, oc2, oc3, oc4, ocmin[6], *ocmax, x, y, z;
	char ocvlak[3*OCRES*OCRES + 8];	// front, top, size view of face, to fill in

	ocmax= ocmin+3;

	memset(g_oc.adrnode, 0, sizeof(g_oc.adrnode));
	memset(g_oc.adrbranch, 0, sizeof(g_oc.adrbranch));

	branchcount=0;
	nodecount=0;
	raycount=0;
	accepted= 0;
	rejected= 0;

	g_oc.vlr_last= NULL;
	INIT_MINMAX(g_oc.min, g_oc.max);
	
	/* first min max octree space */
	for(v=0;v<R.totvlak;v++) {
		if((v & 255)==0) vlr= R.blovl[v>>8];	
		else vlr++;
		if(vlr->mat->mode & MA_TRACEBLE) {	
			
			DO_MINMAX(vlr->v1->co, g_oc.min, g_oc.max);
			DO_MINMAX(vlr->v2->co, g_oc.min, g_oc.max);
			DO_MINMAX(vlr->v3->co, g_oc.min, g_oc.max);
			if(vlr->v4) {
				DO_MINMAX(vlr->v4->co, g_oc.min, g_oc.max);
			}
		}
	}

	if(g_oc.min[0] > g_oc.max[0]) return;	/* empty octree */

	g_oc.adrbranch[0]=(Branch *)MEM_callocN(256*sizeof(Branch),"makeoctree");

	for(c=0;c<3;c++) {	/* octree enlarge, still needed? */
		g_oc.min[c]-= 0.01;
		g_oc.max[c]+= 0.01;
	}
	
	t00= g_oc.max[0]-g_oc.min[0];
	t01= g_oc.max[1]-g_oc.min[1];
	t02= g_oc.max[2]-g_oc.min[2];
	
	/* this minus 0.1 is old safety... seems to be needed? */
	g_oc.ocfacx=ocfac[0]= (OCRES-0.1)/t00;
	g_oc.ocfacy=ocfac[1]= (OCRES-0.1)/t01;
	g_oc.ocfacz=ocfac[2]= (OCRES-0.1)/t02;
	
	g_oc.ocsize= sqrt(t00*t00+t01*t01+t02*t02);	/* global, max size octree */

	for(v=0; v<R.totvlak; v++) {
		if((v & 255)==0) vlr= R.blovl[v>>8];	
		else vlr++;
		
		if(vlr->mat->mode & MA_TRACEBLE) {
			
			v1= vlr->v1;
			v2= vlr->v2;
			v3= vlr->v3;
			v4= vlr->v4;
			
			for(c=0;c<3;c++) {
				rtf[0][c]= (v1->co[c]-g_oc.min[c])*ocfac[c] ;
				rts[0][c]= (short)rtf[0][c];
				rtf[1][c]= (v2->co[c]-g_oc.min[c])*ocfac[c] ;
				rts[1][c]= (short)rtf[1][c];
				rtf[2][c]= (v3->co[c]-g_oc.min[c])*ocfac[c] ;
				rts[2][c]= (short)rtf[2][c];
				if(v4) {
					rtf[3][c]= (v4->co[c]-g_oc.min[c])*ocfac[c] ;
					rts[3][c]= (short)rtf[3][c];
				}
			}
			
			memset(ocvlak, 0, sizeof(ocvlak));
			
			for(c=0;c<3;c++) {
				oc1= rts[0][c];
				oc2= rts[1][c];
				oc3= rts[2][c];
				if(v4==NULL) {
					ocmin[c]= MIN3(oc1,oc2,oc3);
					ocmax[c]= MAX3(oc1,oc2,oc3);
				}
				else {
					oc4= rts[3][c];
					ocmin[c]= MIN4(oc1,oc2,oc3,oc4);
					ocmax[c]= MAX4(oc1,oc2,oc3,oc4);
				}
				if(ocmax[c]>OCRES-1) ocmax[c]=OCRES-1;
				if(ocmin[c]<0) ocmin[c]=0;
			}

			d2dda(0,1,0,1,ocvlak+OCRES*OCRES,rts,rtf);
			d2dda(0,1,0,2,ocvlak,rts,rtf);
			d2dda(0,1,1,2,ocvlak+2*OCRES*OCRES,rts,rtf);
			d2dda(1,2,0,1,ocvlak+OCRES*OCRES,rts,rtf);
			d2dda(1,2,0,2,ocvlak,rts,rtf);
			d2dda(1,2,1,2,ocvlak+2*OCRES*OCRES,rts,rtf);
			if(v4==NULL) {
				d2dda(2,0,0,1,ocvlak+OCRES*OCRES,rts,rtf);
				d2dda(2,0,0,2,ocvlak,rts,rtf);
				d2dda(2,0,1,2,ocvlak+2*OCRES*OCRES,rts,rtf);
			}
			else {
				d2dda(2,3,0,1,ocvlak+OCRES*OCRES,rts,rtf);
				d2dda(2,3,0,2,ocvlak,rts,rtf);
				d2dda(2,3,1,2,ocvlak+2*OCRES*OCRES,rts,rtf);
				d2dda(3,0,0,1,ocvlak+OCRES*OCRES,rts,rtf);
				d2dda(3,0,0,2,ocvlak,rts,rtf);
				d2dda(3,0,1,2,ocvlak+2*OCRES*OCRES,rts,rtf);
			}
			/* nothing todo with triangle..., just fills :) */
			filltriangle(0,1,ocvlak+OCRES*OCRES,ocmin);
			filltriangle(0,2,ocvlak,ocmin);
			filltriangle(1,2,ocvlak+2*OCRES*OCRES,ocmin);
			
			/* this is approximation here... should calculate for real
			   if a node intersects plane */
			
			for(x=ocmin[0];x<=ocmax[0];x++) {
				a= OCRES*x;
				for(y=ocmin[1];y<=ocmax[1];y++) {
					b= OCRES*y;
					if(ocvlak[a+y+OCRES*OCRES]) {
						for(z=ocmin[2];z<=ocmax[2];z++) {
							if(ocvlak[b+z+2*OCRES*OCRES] && ocvlak[a+z]) ocwrite(vlr, x,y,z, rtf);
						}
					}
				}
			}
		}
	}
}

/* ************ raytracer **************** */

/* only for self-intersecting test with current render face (where ray left) */
static short intersection2(float r0, float r1, float r2, float rx1, float ry1, float rz1)
{
	VertRen *v1,*v2,*v3,*v4=NULL;
	float x0,x1,x2,t00,t01,t02,t10,t11,t12,t20,t21,t22;
	float m0, m1, m2, divdet, det, det1;
	float u1, v, u2;

	v1= R.vlr->v1; 
	v2= R.vlr->v2; 
	if(R.vlr->v4) {
		v3= R.vlr->v4;
		v4= R.vlr->v3;
	}
	else v3= R.vlr->v3;	

	t00= v3->co[0]-v1->co[0];
	t01= v3->co[1]-v1->co[1];
	t02= v3->co[2]-v1->co[2];
	t10= v3->co[0]-v2->co[0];
	t11= v3->co[1]-v2->co[1];
	t12= v3->co[2]-v2->co[2];
	
	x0= t11*r2-t12*r1;
	x1= t12*r0-t10*r2;
	x2= t10*r1-t11*r0;

	divdet= t00*x0+t01*x1+t02*x2;

	m0= rx1-v3->co[0];
	m1= ry1-v3->co[1];
	m2= rz1-v3->co[2];
	det1= m0*x0+m1*x1+m2*x2;
	
	if(divdet!=0.0) {
		u1= det1/divdet;

		if(u1<=0.0) {
			det= t00*(m1*r2-m2*r1);
			det+= t01*(m2*r0-m0*r2);
			det+= t02*(m0*r1-m1*r0);
			v= det/divdet;

			if(v<=0.0 && (u1 + v) >= -1.0) {
				return 1;
			}
		}
	}

	if(v4) {

		t20= v3->co[0]-v4->co[0];
		t21= v3->co[1]-v4->co[1];
		t22= v3->co[2]-v4->co[2];

		divdet= t20*x0+t21*x1+t22*x2;
		if(divdet!=0.0) {
			u2= det1/divdet;
		
			if(u2<=0.0) {
				det= t20*(m1*r2-m2*r1);
				det+= t21*(m2*r0-m0*r2);
				det+= t22*(m0*r1-m1*r0);
				v= det/divdet;
	
				if(v<=0.0 && (u2 + v) >= -1.0) {
					return 2;
				}
			}
		}
	}
	return 0;
}

static short intersection(Isect *is)
{
	VertRen *v1,*v2,*v3,*v4=NULL;
	float x0,x1,x2,t00,t01,t02,t10,t11,t12,t20,t21,t22,r0,r1,r2;
	float m0, m1, m2, divdet, det, det1;
	static short vlrisect=0;
	short ok=0;

	is->vlr->raycount= raycount;

	v1= is->vlr->v1; 
	v2= is->vlr->v2; 
	if(is->vlr->v4) {
		v3= is->vlr->v4;
		v4= is->vlr->v3;
	}
	else v3= is->vlr->v3;	

	t00= v3->co[0]-v1->co[0];
	t01= v3->co[1]-v1->co[1];
	t02= v3->co[2]-v1->co[2];
	t10= v3->co[0]-v2->co[0];
	t11= v3->co[1]-v2->co[1];
	t12= v3->co[2]-v2->co[2];
	
	r0= is->start[0]-is->end[0];
	r1= is->start[1]-is->end[1];
	r2= is->start[2]-is->end[2];
	
	x0= t11*r2-t12*r1;
	x1= t12*r0-t10*r2;
	x2= t10*r1-t11*r0;

	divdet= t00*x0+t01*x1+t02*x2;

	m0= is->start[0]-v3->co[0];
	m1= is->start[1]-v3->co[1];
	m2= is->start[2]-v3->co[2];
	det1= m0*x0+m1*x1+m2*x2;
	
	if(divdet!=0.0) {
		float u= det1/divdet;

		if(u<0.0) {
			float v;
			det= t00*(m1*r2-m2*r1);
			det+= t01*(m2*r0-m0*r2);
			det+= t02*(m0*r1-m1*r0);
			v= det/divdet;

			if(v<0.0 && (u + v) > -1.0) {
				float labda;
				det=  m0*(t12*t01-t11*t02);
				det+= m1*(t10*t02-t12*t00);
				det+= m2*(t11*t00-t10*t01);
				labda= det/divdet;
					
				if(labda>0.0 && labda<1.0) {
					is->labda= labda;
					is->u= u; is->v= v;
					ok= 1;
				}
			}
		}
	}

	if(ok==0 && v4) {

		t20= v3->co[0]-v4->co[0];
		t21= v3->co[1]-v4->co[1];
		t22= v3->co[2]-v4->co[2];

		divdet= t20*x0+t21*x1+t22*x2;
		if(divdet!=0.0) {
			float u= det1/divdet;
		
			if(u<0.0) {
				float v;
				det= t20*(m1*r2-m2*r1);
				det+= t21*(m2*r0-m0*r2);
				det+= t22*(m0*r1-m1*r0);
				v= det/divdet;
	
				if(v<0.0 && (u + v) > -1.0) {
					float labda;
					det=  m0*(t12*t21-t11*t22);
					det+= m1*(t10*t22-t12*t20);
					det+= m2*(t11*t20-t10*t21);
					labda= det/divdet;
						
					if(labda>0.0 && labda<1.0) {
						ok= 2;
						is->labda= labda;
						is->u= u; is->v= v;
					}
				}
			}
		}
	}

	if(ok) {
		is->isect= ok;	// wich half of the quad
		
		if(is->mode==DDA_MIRROR) {
			/* for mirror: large faces can be filled in too often, this prevents
			   a face being detected too soon... */
			if(is->labda > is->ddalabda) {
				is->vlr->raycount= 0;
				return 0;
			}
		}
		
		/* when a shadow ray leaves a face, it can be little outside the edges of it, causing
		intersection to be detected in its neighbour face */
		
		if(is->vlrcontr && vlrisect);	// optimizing, the tests below are not needed
		else if(is->labda< .1) {
			short de= 0;
			
			if(v1==R.vlr->v1 || v2==R.vlr->v1 || v3==R.vlr->v1 || v4==R.vlr->v1) de++;
			if(v1==R.vlr->v2 || v2==R.vlr->v2 || v3==R.vlr->v2 || v4==R.vlr->v2) de++;
			if(v1==R.vlr->v3 || v2==R.vlr->v3 || v3==R.vlr->v3 || v4==R.vlr->v3) de++;
			if(R.vlr->v4) {
				if(v1==R.vlr->v4 || v2==R.vlr->v4 || v3==R.vlr->v4 || v4==R.vlr->v4) de++;
			}
			if(de) {
				
				/* so there's a shared edge or vertex, let's intersect ray with R.vlr
				itself, if that's true we can safely return 1, otherwise we assume
				the intersection is invalid, 0 */
				
				if(is->vlrcontr==NULL) {
					is->vlrcontr= R.vlr;
					vlrisect= intersection2(r0, r1, r2, is->start[0], is->start[1], is->start[2]);
				}

				if(vlrisect) return 1;
				return 0;
			}
		}
		
		return 1;
	}

	return 0;
}

/* check all faces in this node */
static int testnode(Isect *is, Node *no, int x, int y, int z)
{
	VlakRen *vlr;
	short nr=0, ocvaldone=0;
	OcVal ocval, *ov;
	
	if(is->mode==DDA_SHADOW) {
		
		vlr= no->v[0];
		while(vlr) {
		
			if(raycount != vlr->raycount) {
				
				if(ocvaldone==0) {
					calc_ocval_ray(&ocval, (float)x, (float)y, (float)z, 0.0, 0.0, 0.0);
					ocvaldone= 1;
				}
				
				ov= no->ov+nr;
				if( (ov->ocx & ocval.ocx) && (ov->ocy & ocval.ocy) && (ov->ocz & ocval.ocz) ) { 
					//accepted++;
					is->vlr= vlr;

					if(intersection(is)) {
						g_oc.vlr_last= vlr;
						return 1;
					}
				}
				//else rejected++;
			}
			
			nr++;
			if(nr==8) {
				no= no->next;
				if(no==0) return 0;
				nr=0;
			}
			vlr= no->v[nr];
		}
	}
	else {			/* else mirror and glass  */
		Isect isect;
		int found= 0;
		
		is->labda= 1.0;	/* needed? */
		isect= *is;		/* copy for sorting */
		
		vlr= no->v[0];
		while(vlr) {
			
			if(raycount != vlr->raycount) {
				
				if(ocvaldone==0) {
					calc_ocval_ray(&ocval, (float)x, (float)y, (float)z, 0.0, 0.0, 0.0);
					ocvaldone= 1;
				}
				
				ov= no->ov+nr;
				if( (ov->ocx & ocval.ocx) && (ov->ocy & ocval.ocy) && (ov->ocz & ocval.ocz) ) { 
					//accepted++;

					isect.vlr= vlr;
					if(intersection(&isect)) {
						if(isect.labda<is->labda) *is= isect;
						found= 1;
					}
				}
				//else rejected++;
			}
			
			nr++;
			if(nr==8) {
				no= no->next;
				if(no==NULL) break;
				nr=0;
			}
			vlr= no->v[nr];
		}
		
		return found;
	}

	return 0;
}

/* find the Node for the octree coord x y z */
static Node *ocread(int x, int y, int z)
{
	static int mdiff=0, xo=OCRES, yo=OCRES, zo=OCRES;
	Branch *br;
	int oc1, diff;

	/* outside of octree check, reset */
	if( (x & ~(OCRES-1)) ||  (y & ~(OCRES-1)) ||  (z & ~(OCRES-1)) ) {
		xo=OCRES; yo=OCRES; zo=OCRES;
		return NULL;
	}
	
	diff= (xo ^ x) | (yo ^ y) | (zo ^ z);

	if(diff>mdiff) {
		
		xo=x; yo=y; zo=z;
		x<<=2;
		y<<=1;
		
		oc1= ((x & 128)+(y & 64)+(z & 32))>>5;
		br= g_oc.adrbranch[0]->b[oc1];
		if(br) {

			oc1= ((x & 64)+(y & 32)+(z & 16))>>4;
			br= br->b[oc1];
			if(br) {
				oc1= ((x & 32)+(y & 16)+(z & 8))>>3;
				br= br->b[oc1];
				if(br) {
					oc1= ((x & 16)+(y & 8)+(z & 4))>>2;
					br= br->b[oc1];
					if(br) {
						oc1= ((x & 8)+(y & 4)+(z & 2))>>1;
						br= br->b[oc1];
						if(br) {
							mdiff=0;
							oc1= ((x & 4)+(y & 2)+(z & 1));
							return (Node *)br->b[oc1];
						}
						else mdiff=1;
					}
					else mdiff=3;
				}
				else mdiff=7;
			}
			else mdiff=15;
		}
		else mdiff=31;
	}
	return NULL;
}

static short cliptest(float p, float q, float *u1, float *u2)
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

/* return 1: found valid intersection */
/* starts with global R.vlr */
static int d3dda(Isect *is)	
{
	Node *no;
	float u1,u2,ox1,ox2,oy1,oy2,oz1,oz2;
	float labdao,labdax,ldx,labday,ldy,labdaz,ldz, ddalabda;
	float vec1[3], vec2[3];
	int dx,dy,dz;	
	int xo,yo,zo,c1=0;
	int ocx1,ocx2,ocy1, ocy2,ocz1,ocz2;
	
	/* clip with octree */
	if(branchcount==0) return NULL;
	
	/* do this before intersect calls */
	raycount++;
	R.vlr->raycount= raycount;
	is->vlrcontr= NULL;	/*  to check shared edge */

	/* only for shadow! */
	if(is->mode==DDA_SHADOW && g_oc.vlr_last!=NULL && g_oc.vlr_last!=R.vlr) {
		is->vlr= g_oc.vlr_last;
		if(intersection(is)) return 1;
	}
	
	ldx= is->end[0] - is->start[0];
	u1= 0.0;
	u2= 1.0;
	
	/* clip with octree cube */
	if(cliptest(-ldx, is->start[0]-g_oc.min[0], &u1,&u2)) {
		if(cliptest(ldx, g_oc.max[0]-is->start[0], &u1,&u2)) {
			ldy= is->end[1] - is->start[1];
			if(cliptest(-ldy, is->start[1]-g_oc.min[1], &u1,&u2)) {
				if(cliptest(ldy, g_oc.max[1]-is->start[1], &u1,&u2)) {
					ldz= is->end[2] - is->start[2];
					if(cliptest(-ldz, is->start[2]-g_oc.min[2], &u1,&u2)) {
						if(cliptest(ldz, g_oc.max[2]-is->start[2], &u1,&u2)) {
							c1=1;
							if(u2<1.0) {
								is->end[0]= is->start[0]+u2*ldx;
								is->end[1]= is->start[1]+u2*ldy;
								is->end[2]= is->start[2]+u2*ldz;
							}
							if(u1>0.0) {
								is->start[0]+=u1*ldx;
								is->start[1]+=u1*ldy;
								is->start[2]+=u1*ldz;
							}
						}
					}
				}
			}
		}
	}

	if(c1==0) return 0;

	/* reset static variables in ocread */
	ocread(OCRES, OCRES, OCRES);

	/* setup 3dda to traverse octree */
	ox1= (is->start[0]-g_oc.min[0])*g_oc.ocfacx;
	oy1= (is->start[1]-g_oc.min[1])*g_oc.ocfacy;
	oz1= (is->start[2]-g_oc.min[2])*g_oc.ocfacz;
	ox2= (is->end[0]-g_oc.min[0])*g_oc.ocfacx;
	oy2= (is->end[1]-g_oc.min[1])*g_oc.ocfacy;
	oz2= (is->end[2]-g_oc.min[2])*g_oc.ocfacz;

	ocx1= (int)ox1;
	ocy1= (int)oy1;
	ocz1= (int)oz1;
	ocx2= (int)ox2;
	ocy2= (int)oy2;
	ocz2= (int)oz2;

	if(ocx1==ocx2 && ocy1==ocy2 && ocz1==ocz2) {
		/* no calc, this is store */
		calc_ocval_ray(NULL, ox1, oy1, oz1, ox2, oy2, oz2);

		no= ocread(ocx1, ocy1, ocz1);
		if(no) {
			is->ddalabda= 1.0;
			if( testnode(is, no, ocx1, ocy1, ocz1) ) return 1;
		}
	}
	else {
		float dox, doy, doz;
		
		dox= ox1-ox2;
		doy= oy1-oy2;
		doz= oz1-oz2;
		
		/* calc labda en ld */
		if(dox!=0.0) {
			if(dox<0.0) {
				labdax= (ox1-ocx1-1.0)/dox;
				ldx= -1.0/dox;
				dx= 1;
			} else {
				labdax= (ox1-ocx1)/dox;
				ldx= 1.0/dox;
				dx= -1;
			}
		} else {
			labdax=1.0;
			ldx=0;
			dx= 0;
		}

		if(doy!=0.0) {
			if(doy<0.0) {
				labday= (oy1-ocy1-1.0)/doy;
				ldy= -1.0/doy;
				dy= 1;
			} else {
				labday= (oy1-ocy1)/doy;
				ldy= 1.0/doy;
				dy= -1;
			}
		} else {
			labday=1.0;
			ldy=0;
			dy= 0;
		}

		if(doz!=0.0) {
			if(doz<0.0) {
				labdaz= (oz1-ocz1-1.0)/doz;
				ldz= -1.0/doz;
				dz= 1;
			} else {
				labdaz= (oz1-ocz1)/doz;
				ldz= 1.0/doz;
				dz= -1;
			}
		} else {
			labdaz=1.0;
			ldz=0;
			dz= 0;
		}
		
		xo=ocx1; yo=ocy1; zo=ocz1;
		ddalabda= MIN3(labdax,labday,labdaz);
		
		// dox,y,z is negative
		vec2[0]= ox1;
		vec2[1]= oy1;
		vec2[2]= oz1;
		
		/* this loop has been constructed to make sure the first and last node of ray
		   are always included, even when ddalabda==1.0 or larger */
		while(TRUE) {

			no= ocread(xo, yo, zo);
			if(no) {
				
				/* calculate ray intersection with octree node */
				VECCOPY(vec1, vec2);
					// dox,y,z is negative
				vec2[0]= ox1-ddalabda*dox;
				vec2[1]= oy1-ddalabda*doy;
				vec2[2]= oz1-ddalabda*doz;
				/* no calc, this is store */
				calc_ocval_ray(NULL, vec1[0], vec1[1], vec1[2], vec2[0], vec2[1], vec2[2]);
				
				is->ddalabda= ddalabda;
				if( testnode(is, no, xo,yo,zo) ) return 1;
			}

			labdao= ddalabda;

			if(labdax<labday) {
				if(labday<labdaz) {
					xo+=dx;
					labdax+=ldx;
				} else if(labdax<labdaz) {
					xo+=dx;
					labdax+=ldx;
				} else {
					zo+=dz;
					labdaz+=ldz;
					if(labdax==labdaz) {
						xo+=dx;
						labdax+=ldx;
					}
				}
			} else if(labdax<labdaz) {
				yo+=dy;
				labday+=ldy;
				if(labday==labdax) {
					xo+=dx;
					labdax+=ldx;
				}
			} else if(labday<labdaz) {
				yo+=dy;
				labday+=ldy;
			} else if(labday<labdax) {
				zo+=dz;
				labdaz+=ldz;
				if(labdaz==labday) {
					yo+=dy;
					labday+=ldy;
				}
			} else {
				xo+=dx;
				labdax+=ldx;
				yo+=dy;
				labday+=ldy;
				zo+=dz;
				labdaz+=ldz;
			}

			ddalabda=MIN3(labdax,labday,labdaz);
			if(ddalabda==labdao) break;
			/* to make sure the last node is always checked */
			if(labdao>=1.0) break;
		}
	}
	
	/* reached end, no intersections found */
	g_oc.vlr_last= NULL;
	return 0;
}		

/* for now; mostly a duplicate of shadepixel() itself... could be unified once */
/* R.view has been set */
static void shade_ray(Isect *is, int mask)
{
	extern void shade_lamp_loop(int );
	VertRen *v1, *v2, *v3;
	float n1[3], n2[3], n3[3];
	float *o1, *o2, *o3;
	float u, v, l;
	int flip= 0;
	char p1, p2, p3;
	
	R.co[0]= is->start[0]+is->labda*(is->end[0]-is->start[0]);
	R.co[1]= is->start[1]+is->labda*(is->end[1]-is->start[1]);
	R.co[2]= is->start[2]+is->labda*(is->end[2]-is->start[2]);
		
	R.vlaknr= -1; // signal to reset static variables in rendercore.c shadepixel

	R.vlr= is->vlr;
	R.mat= R.vlr->mat;
	R.matren= R.mat->ren;
	R.osatex= (R.matren->texco & TEXCO_OSA);
	
	/* face normal, check for flip */
	R.vno= R.vlr->n;
	l= R.vlr->n[0]*R.view[0]+R.vlr->n[1]*R.view[1]+R.vlr->n[2]*R.view[2];
	if(l<0.0) {	
		flip= 1;
		R.vlr->n[0]= -R.vlr->n[0];
		R.vlr->n[1]= -R.vlr->n[1];
		R.vlr->n[2]= -R.vlr->n[2];
		R.vlr->puno= ~(R.vlr->puno);
	}

	if(R.vlr->v4) {
		if(is->isect==2) {
			v1= R.vlr->v3;
			p1= ME_FLIPV3; 
		} else {
			v1= R.vlr->v1;
			p1= ME_FLIPV1; 
		}
		v2= R.vlr->v2;
		v3= R.vlr->v4;
		p2= ME_FLIPV2; p3= ME_FLIPV4;
	}
	else {
		v1= R.vlr->v1;
		v2= R.vlr->v2;
		v3= R.vlr->v3;
		p1= ME_FLIPV1; p2= ME_FLIPV2; p3= ME_FLIPV3;
	}
	
	if(R.vlr->flag & R_SMOOTH) { /* adjust punos (vertexnormals) */
		if(R.vlr->puno & p1) {
			n1[0]= -v1->n[0]; n1[1]= -v1->n[1]; n1[2]= -v1->n[2];
		} else {
			n1[0]= v1->n[0]; n1[1]= v1->n[1]; n1[2]= v1->n[2];
		}
		if(R.vlr->puno & p2) {
			n2[0]= -v2->n[0]; n2[1]= -v2->n[1]; n2[2]= -v2->n[2];
		} else {
			n2[0]= v2->n[0]; n2[1]= v2->n[1]; n2[2]= v2->n[2];
		}
		
		if(R.vlr->puno & p3) {
			n3[0]= -v3->n[0]; n3[1]= -v3->n[1]; n3[2]= -v3->n[2];
		} else {
			n3[0]= v3->n[0]; n3[1]= v3->n[1]; n3[2]= v3->n[2];
		}
	}
	
	u= is->u;
	v= is->v;
		
	// Osa structs we leave unchanged now


	/* UV and TEX*/
	if( (R.vlr->flag & R_SMOOTH) || (R.matren->texco & NEED_UV)) {

		l= 1.0+u+v;

		if(R.vlr->flag & R_SMOOTH) {
			R.vn[0]= l*n3[0]-u*n1[0]-v*n2[0];
			R.vn[1]= l*n3[1]-u*n1[1]-v*n2[1];
			R.vn[2]= l*n3[2]-u*n1[2]-v*n2[2];

			Normalise(R.vn);
		}
		else {
			VECCOPY(R.vn, R.vlr->n);
		}

		if(R.matren->texco & TEXCO_ORCO) {
			if(v2->orco) {
				o1= v1->orco;
				o2= v2->orco;
				o3= v3->orco;
				
				R.lo[0]= l*o3[0]-u*o1[0]-v*o2[0];
				R.lo[1]= l*o3[1]-u*o1[1]-v*o2[1];
				R.lo[2]= l*o3[2]-u*o1[2]-v*o2[2];

			}
		}
		
		if(R.matren->texco & TEXCO_GLOB) {
			VECCOPY(R.gl, R.co);
			Mat4MulVecfl(R.viewinv, R.gl);
		}
		if(R.matren->texco & TEXCO_NORM) {
			R.orn[0]= R.vn[0];
			R.orn[1]= -R.vn[1];
			R.orn[2]= R.vn[2];
		}

		if((R.matren->texco & TEXCO_UV) || (R.matren->mode & (MA_VERTEXCOL|MA_FACETEXTURE)))  {
			if(R.vlr->tface) {
				float *uv1, *uv2, *uv3;
				
				if( R.vlr->v4 || (R.vlr->flag & R_FACE_SPLIT) ) {
					if(is->isect==2) uv1= R.vlr->tface->uv[2];
					else uv1= R.vlr->tface->uv[0];
					uv2= R.vlr->tface->uv[1];
					uv3= R.vlr->tface->uv[3];
				}
				else {
					uv1= R.vlr->tface->uv[0];
					uv2= R.vlr->tface->uv[1];
					uv3= R.vlr->tface->uv[2];
				}
				
				R.uv[0]= -1.0 + 2.0*(l*uv3[0]-u*uv1[0]-v*uv2[0]);
				R.uv[1]= -1.0 + 2.0*(l*uv3[1]-u*uv1[1]-v*uv2[1]);				
			}
			else {
				R.uv[0]= 2.0*(u+.5);
				R.uv[1]= 2.0*(v+.5);
			}
		}

		if(R.matren->mode & MA_VERTEXCOL) {
			char *cp3, *cp2, *cp1= (char *)R.vlr->vcol;
			if(cp1) {
				if( R.vlr->v4 || (R.vlr->flag & R_FACE_SPLIT) ) {
					if(is->isect==2) cp1= (char *)(R.vlr->vcol+2);
					else cp1= (char *)(R.vlr->vcol+0);
					
					cp2= (char *)(R.vlr->vcol+1);
					cp3= (char *)(R.vlr->vcol+3);
				}
				else {
					cp1= (char *)(R.vlr->vcol+0);
					cp2= (char *)(R.vlr->vcol+1);
					cp3= (char *)(R.vlr->vcol+2);
				}
				R.vcol[0]= (l*cp3[3]-u*cp1[3]-v*cp2[3])/255.0;
				R.vcol[1]= (l*cp3[2]-u*cp1[2]-v*cp2[2])/255.0;
				R.vcol[2]= (l*cp3[1]-u*cp1[1]-v*cp2[1])/255.0;
				
			}
			else {
				R.vcol[0]= 0.0;
				R.vcol[1]= 0.0;
				R.vcol[2]= 0.0;
			}
		}
		if(R.matren->mode & MA_RADIO) {
			R.rad[0]= (l*v3->rad[0] - u*v1->rad[0] - v*v2->rad[0]);
			R.rad[1]= (l*v3->rad[1] - u*v1->rad[1] - v*v2->rad[1]);
			R.rad[2]= (l*v3->rad[2] - u*v1->rad[2] - v*v2->rad[2]);
		}
		else {
			R.rad[0]= R.rad[1]= R.rad[2]= 0.0;
		}
		if(R.matren->mode & MA_FACETEXTURE) {
			if((R.matren->mode & MA_VERTEXCOL)==0) {
				R.vcol[0]= 1.0;
				R.vcol[1]= 1.0;
				R.vcol[2]= 1.0;
			}
			if(R.vlr->tface) render_realtime_texture();
		}
		if(R.matren->texco & TEXCO_REFL) {
			/* R.vn dot R.view */
			float i= -2.0*(R.vn[0]*R.view[0]+R.vn[1]*R.view[1]+R.vn[2]*R.view[2]);
		
			R.ref[0]= (R.view[0]+i*R.vn[0]);
			R.ref[1]= (R.view[1]+i*R.vn[1]);
			R.ref[2]= (R.view[2]+i*R.vn[2]);
		}
	}
	else {
		VECCOPY(R.vn, R.vlr->n);
	}

	shade_lamp_loop(mask);	

	if(flip) {	
		R.vlr->n[0]= -R.vlr->n[0];
		R.vlr->n[1]= -R.vlr->n[1];
		R.vlr->n[2]= -R.vlr->n[2];
		R.vlr->puno= ~(R.vlr->puno);
	}
}

/* the main recursive tracer itself */
static void traceray(float f, short depth, float *start, float *vec, float *col, int mask)
{
	extern unsigned short shortcol[4];	// only for old render, which stores ushort
	Isect isec;
	float f1, fr, fg, fb;
	float ref[3];
	
	if(depth<0) return;

	fr= R.mat->mirr;
	fg= R.mat->mirg;
	fb= R.mat->mirb;

	VECCOPY(isec.start, start);
	isec.end[0]= start[0]+g_oc.ocsize*vec[0];
	isec.end[1]= start[1]+g_oc.ocsize*vec[1];
	isec.end[2]= start[2]+g_oc.ocsize*vec[2];
	isec.mode= DDA_MIRROR;
	
	if( d3dda(&isec) ) {
	
		/* set up view vector */
		VECCOPY(R.view, vec);
		Normalise(R.view);
		
		shade_ray(&isec, mask);	// returns shortcol
		
		f1= 1.0-f;

		col[0]= f*fr*(shortcol[0]/65535.0)+ f1*col[0];
		col[1]= f*fg*(shortcol[1]/65535.0)+ f1*col[1];
		col[2]= f*fb*(shortcol[2]/65535.0)+ f1*col[2];
		
		/* is already new material: */
		if(R.mat->ray_mirror>0.0) {
			f1= -2*(R.vn[0]*R.view[0]+R.vn[1]*R.view[1]+R.vn[2]*R.view[2]);
			if(f1> -0.2) f1= -0.2;
			
			ref[0]= (R.view[0]+f1*R.vn[0]);
			ref[1]= (R.view[1]+f1*R.vn[1]);
			ref[2]= (R.view[2]+f1*R.vn[2]);

			f*= R.mat->ray_mirror;
			traceray(f, depth-1, R.co, ref, col, mask);
		}
	}
	else {	/* sky */
		char skycol[4];
		
		VECCOPY(R.view, vec);
		Normalise(R.view);

		RE_sky(skycol);	
		
		f1= 1.0-f;

		f/= 255.0;
		col[0]= f*fr*skycol[0]+ f1*col[0];
		col[1]= f*fg*skycol[1]+ f1*col[1];
		col[2]= f*fb*skycol[2]+ f1*col[2];

	}
}

/* **************** jitter blocks ********** */

static float jit_plane2[2*2*3]={0.0};
static float jit_plane3[3*3*3]={0.0};
static float jit_plane4[4*4*3]={0.0};
static float jit_plane5[5*5*3]={0.0};
static float jit_plane6[5*5*3]={0.0};
static float jit_plane7[7*7*3]={0.0};
static float jit_plane8[8*8*3]={0.0};

static float jit_cube2[2*2*2*3]={0.0};
static float jit_cube3[3*3*3*3]={0.0};
static float jit_cube4[4*4*4*3]={0.0};
static float jit_cube5[5*5*5*3]={0.0};

/* table around origin, -.5 to 0.5 */
static float *jitter_plane(int resol)
{
	extern float hashvectf[];
	float dsize, *jit, *fp, *hv;
	int x, y;
	
	if(resol<2) resol= 2;
	if(resol>8) resol= 8;

	switch (resol) {
	case 2: jit= jit_plane2; break;
	case 3: jit= jit_plane3; break;
	case 4: jit= jit_plane4; break;
	case 5: jit= jit_plane5; break;
	case 6: jit= jit_plane6; break;
	case 7: jit= jit_plane7; break;
	default: jit= jit_plane8; break;
	}
	if(jit[0]!=0.0) return jit;

	dsize= 1.0/(resol-1.0);
	fp= jit;
	hv= hashvectf;
	for(x=0; x<resol; x++) {
		for(y=0; y<resol; y++, fp+= 3, hv+=3) {
			fp[0]= -0.5 + (x+0.25*hv[0])*dsize;
			fp[1]= -0.5 + (y+0.25*hv[1])*dsize;
			fp[2]= fp[0]*fp[0] + fp[1]*fp[1];
			if(resol>2)
				if(fp[2]>0.3) fp[2]= 0.0;
		}
	}
	
	return jit;
}

static void *jitter_cube(int resol)
{
	float dsize, *jit, *fp;
	int x, y, z;
	
	if(resol<2) resol= 2;
	if(resol>5) resol= 5;

	switch (resol) {
	case 2: jit= jit_cube2; break;
	case 3: jit= jit_cube3; break;
	case 4: jit= jit_cube4; break;
	default: jit= jit_cube5; break;
	}
	if(jit[0]!=0.0) return jit;

	dsize= 1.0/(resol-1.0);
	fp= jit;
	for(x=0; x<resol; x++) {
		for(y=0; y<resol; y++) {
			for(z=0; z<resol; z++, fp+= 3) {
				fp[0]= -0.5 + x*dsize;
				fp[1]= -0.5 + y*dsize;
				fp[2]= -0.5 + z*dsize;
			}
		}
	}
	
	return jit;

}

/* ***************** extern calls ************** */


/* extern call from render loop */
void ray_mirror(int mask)
{
	float i, vec[3];
	
	if(R.r.mode & R_OSA) {
		VlakRen *vlr;
		float accum[3], rco[3], rvno[3], col[3], ref[3], dxref[3], dyref[3];
		float div= 0.0;
		int j;
		
		accum[0]= accum[1]= accum[2]= 0.0;
		
		/* store variables which change during tracing */
		VECCOPY(rco, R.co);
		VECCOPY(rvno, R.vno);
		VECCOPY(ref, R.ref);
		VECCOPY(dxref, O.dxref);
		VECCOPY(dyref, O.dyref);
		vlr= R.vlr;

		for(j=0; j<R.osa; j++) {
			if(mask & 1<<j) {
				vec[0]= ref[0] + 1.0*(jit[j][0]-0.5)*dxref[0] + 1.0*(jit[j][1]-0.5)*dyref[0] ;
				vec[1]= ref[1] + 1.0*(jit[j][0]-0.5)*dxref[1] + 1.0*(jit[j][1]-0.5)*dyref[1] ;
				vec[2]= ref[2] + 1.0*(jit[j][0]-0.5)*dxref[2] + 1.0*(jit[j][1]-0.5)*dyref[2] ;
				
				/* prevent normal go to backside */
				i= vec[0]*rvno[0]+ vec[1]*rvno[1]+ vec[2]*rvno[2];
				if(i>0.0) {
					i+= .01;
					vec[0]-= i*rvno[0];
					vec[1]-= i*rvno[1];
					vec[2]-= i*rvno[2];
				}
				
				R.co[0]+= (jit[j][0]-0.5)*O.dxco[0] + (jit[j][1]-0.5)*O.dyco[0] ;
				R.co[1]+= (jit[j][0]-0.5)*O.dxco[1] + (jit[j][1]-0.5)*O.dyco[1] ;
				R.co[2]+= (jit[j][0]-0.5)*O.dxco[2] + (jit[j][1]-0.5)*O.dyco[2] ;
				
				/* we use a new mask here, only shadow uses it */
				/* result in accum, this is copied to R.refcol for shade_lamp_loop */
				traceray(1.0, R.mat->ray_depth, R.co, vec, col, 1<<j);
				
				VecAddf(accum, accum, col);
				div+= 1.0;

				/* restore */
				VECCOPY(R.co, rco);
				R.vlr= vlr;
				R.mat= vlr->mat;
				R.matren= R.mat->ren;
			}
		}
		R.refcol[0]= R.mat->ray_mirror;
		R.refcol[1]= R.mat->ray_mirror*accum[0]/div;
		R.refcol[2]= R.mat->ray_mirror*accum[1]/div;
		R.refcol[3]= R.mat->ray_mirror*accum[2]/div;
	}
	else {
		i= -2.0*(R.vn[0]*R.view[0]+R.vn[1]*R.view[1]+R.vn[2]*R.view[2]);
		
		vec[0]= (R.view[0]+i*R.vn[0]);
		vec[1]= (R.view[1]+i*R.vn[1]);
		vec[2]= (R.view[2]+i*R.vn[2]);
		
		/* test phong normals, then we should prevent vector going to the back */
		if(R.vlr->flag & R_SMOOTH) {
			i= vec[0]*R.vno[0]+ vec[1]*R.vno[1]+ vec[2]*R.vno[2];
			if(i>0.0) {
				i+= .01;
				vec[0]-= i*R.vno[0];
				vec[1]-= i*R.vno[1];
				vec[2]-= i*R.vno[2];
			}
		}
		
		/* result in r.refcol, this is added in shade_lamp_loop */
		i= R.mat->ray_mirror;
		traceray(1.0, R.mat->ray_depth, R.co, vec, R.refcol+1, mask);
		R.refcol[0]= i;
		R.refcol[1]*= i;
		R.refcol[2]*= i;
		R.refcol[3]*= i;
		
	}
}

/* extern call from shade_lamp_loop */
float ray_shadow(LampRen *lar, int mask)
{
	Isect isec;
	float fac, div=0.0, lampco[3];

	isec.mode= DDA_SHADOW;
	
	if(lar->type==LA_SUN || lar->type==LA_HEMI) {
		lampco[0]= R.co[0] - g_oc.ocsize*lar->vec[0];
		lampco[1]= R.co[1] - g_oc.ocsize*lar->vec[1];
		lampco[2]= R.co[2] - g_oc.ocsize*lar->vec[2];
	}
	else {
		VECCOPY(lampco, lar->co);
	}
	
	if(lar->ray_samp<2) {
		if(R.r.mode & R_OSA) {
			int j;
			fac= 0.0;
			for(j=0; j<R.osa; j++) {
				if(mask & 1<<j) {
					isec.start[0]= R.co[0] + (jit[j][0]-0.5)*O.dxco[0] + (jit[j][1]-0.5)*O.dyco[0] ;
					isec.start[1]= R.co[1] + (jit[j][0]-0.5)*O.dxco[1] + (jit[j][1]-0.5)*O.dyco[1] ;
					isec.start[2]= R.co[2] + (jit[j][0]-0.5)*O.dxco[2] + (jit[j][1]-0.5)*O.dyco[2] ;
					VECCOPY(isec.end, lampco);
					if( d3dda(&isec) ) fac+= 1.0;
					div+= 1.0;
				}
			}
			return fac/div;
		}
		else {
			VECCOPY(isec.start, R.co);
			VECCOPY(isec.end, lampco);
			if( d3dda(&isec)) return 1.0;
		}
	}
	else {
		float *jitlamp;
		float vec[3];
		int a, j=0;
		
		VECCOPY(isec.start, R.co);
		
		fac= 0.0;
		a= lar->ray_samp*lar->ray_samp;
		jitlamp= jitter_plane(lar->ray_samp);
		
		while(a--) {
			if(jitlamp[2]!=0.0) {
				vec[0]= lar->ray_soft*jitlamp[0];
				vec[1]= lar->ray_soft*jitlamp[1];
				vec[2]= 0.0;
				Mat3TransMulVecfl(lar->imat, vec);
				
				isec.end[0]= lampco[0]+vec[0];
				isec.end[1]= lampco[1]+vec[1];
				isec.end[2]= lampco[2]+vec[2];
				
				if(R.r.mode & R_OSA) {
					isec.start[0]= R.co[0] + (jit[j][0]-0.5)*O.dxco[0] + (jit[j][1]-0.5)*O.dyco[0] ;
					isec.start[1]= R.co[1] + (jit[j][0]-0.5)*O.dxco[1] + (jit[j][1]-0.5)*O.dyco[1] ;
					isec.start[2]= R.co[2] + (jit[j][0]-0.5)*O.dxco[2] + (jit[j][1]-0.5)*O.dyco[2] ;
					j++;
					if(j>=R.osa) j= 0;
				}
				
				if( d3dda(&isec) ) fac+= 1.0;
				div+= 1.0;
			}
			jitlamp+= 3;
		}
		return fac/div;
	}
	return 0.0;
}

