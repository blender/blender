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

#include "BKE_utildefines.h"
#include "BKE_global.h"

#include "BLI_arithb.h"
#include <BLI_rand.h>

#include "render.h"
#include "render_intern.h"
#include "rendercore.h"
#include "pixelblending.h"
#include "jitter.h"
#include "texture.h"

#define OCRES	64

#define DDA_SHADOW 0
#define DDA_MIRROR 1
#define DDA_SHADOW_TRA 2

#define DEPTH_SHADOW_TRA  10


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
	float start[3], vec[3], end[3];		/* start+vec = end, in d3dda */
	float labda, u, v;
	struct VlakRen *vlr, *vlrcontr, *vlrorig;
	short isect, mode;	/* isect: which half of quad, mode: DDA_SHADOW, DDA_MIRROR, DDA_SHADOW_TRA */
	float ddalabda;
	float col[4];		/* RGBA for shadow_tra */
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
static int coh_test=0;	/* coherence optimize */

/* just for statistics */
static int raycount, branchcount, nodecount;
static int accepted, rejected, coherent_ray;


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

static int face_in_node(VlakRen *vlr, short x, short y, short z, float rtf[][3])
{
	static float nor[3], d;
	float fx, fy, fz;
	
	// init static vars 
	if(vlr) {
		CalcNormFloat(rtf[0], rtf[1], rtf[2], nor);
		d= -nor[0]*rtf[0][0] - nor[1]*rtf[0][1] - nor[2]*rtf[0][2];
		return 0;
	}
	
	fx= x;
	fy= y;
	fz= z;
	
	if((x+0)*nor[0] + (y+0)*nor[1] + (z+0)*nor[2] + d > 0.0) {
		if((x+1)*nor[0] + (y+0)*nor[1] + (z+0)*nor[2] + d < 0.0) return 1;
		if((x+0)*nor[0] + (y+1)*nor[1] + (z+0)*nor[2] + d < 0.0) return 1;
		if((x+1)*nor[0] + (y+1)*nor[1] + (z+0)*nor[2] + d < 0.0) return 1;
	
		if((x+0)*nor[0] + (y+0)*nor[1] + (z+1)*nor[2] + d < 0.0) return 1;
		if((x+1)*nor[0] + (y+0)*nor[1] + (z+1)*nor[2] + d < 0.0) return 1;
		if((x+0)*nor[0] + (y+1)*nor[1] + (z+1)*nor[2] + d < 0.0) return 1;
		if((x+1)*nor[0] + (y+1)*nor[1] + (z+1)*nor[2] + d < 0.0) return 1;
	}
	else {
		if((x+1)*nor[0] + (y+0)*nor[1] + (z+0)*nor[2] + d > 0.0) return 1;
		if((x+0)*nor[0] + (y+1)*nor[1] + (z+0)*nor[2] + d > 0.0) return 1;
		if((x+1)*nor[0] + (y+1)*nor[1] + (z+0)*nor[2] + d > 0.0) return 1;
	
		if((x+0)*nor[0] + (y+0)*nor[1] + (z+1)*nor[2] + d > 0.0) return 1;
		if((x+1)*nor[0] + (y+0)*nor[1] + (z+1)*nor[2] + d > 0.0) return 1;
		if((x+0)*nor[0] + (y+1)*nor[1] + (z+1)*nor[2] + d > 0.0) return 1;
		if((x+1)*nor[0] + (y+1)*nor[1] + (z+1)*nor[2] + d > 0.0) return 1;
	}

	return 0;
}

static void ocwrite(VlakRen *vlr, short x, short y, short z, float rtf[][3])
{
	Branch *br;
	Node *no;
	short a, oc0, oc1, oc2, oc3, oc4, oc5;

	if(face_in_node(NULL, x,y,z, rtf)==0) return;

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
	

//	printf("branches %d nodes %d\n", branchcount, nodecount);
	printf("raycount %d \n", raycount);	
	printf("ray coherent %d \n", coherent_ray);
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
	coherent_ray= 0;
	
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
			
			/* init static vars here */
			face_in_node(vlr, 0,0,0, rtf);
			
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
static short intersection2(VlakRen *vlr, float r0, float r1, float r2, float rx1, float ry1, float rz1)
{
	VertRen *v1,*v2,*v3,*v4=NULL;
	float x0,x1,x2,t00,t01,t02,t10,t11,t12,t20,t21,t22;
	float m0, m1, m2, divdet, det, det1;
	float u1, v, u2;

	v1= vlr->v1; 
	v2= vlr->v2; 
	if(vlr->v4) {
		v3= vlr->v4;
		v4= vlr->v3;
	}
	else v3= vlr->v3;	

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
	float m0, m1, m2, divdet, det1;
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
	
	r0= is->vec[0];
	r1= is->vec[1];
	r2= is->vec[2];
	
	x0= t12*r1-t11*r2;
	x1= t10*r2-t12*r0;
	x2= t11*r0-t10*r1;

	divdet= t00*x0+t01*x1+t02*x2;

	m0= is->start[0]-v3->co[0];
	m1= is->start[1]-v3->co[1];
	m2= is->start[2]-v3->co[2];
	det1= m0*x0+m1*x1+m2*x2;
	
	if(divdet!=0.0) {
		float u;

		divdet= 1.0/divdet;
		u= det1*divdet;
		if(u<0.0 && u>-1.0) {
			float v, cros0, cros1, cros2;
			
			cros0= m1*t02-m2*t01;
			cros1= m2*t00-m0*t02;
			cros2= m0*t01-m1*t00;
			v= divdet*(cros0*r0 + cros1*r1 + cros2*r2);

			if(v<0.0 && (u + v) > -1.0) {
				float labda;
				labda= divdet*(cros0*t10 + cros1*t11 + cros2*t12);

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
			float u;
			divdet= 1.0/divdet;
			u = det1*divdet;
			
			if(u<0.0 && u>-1.0) {
				float v, cros0, cros1, cros2;
				cros0= m1*t22-m2*t21;
				cros1= m2*t20-m0*t22;
				cros2= m0*t21-m1*t20;
				v= divdet*(cros0*r0 + cros1*r1 + cros2*r2);
	
				if(v<0.0 && (u + v) > -1.0) {
					float labda;
					labda= divdet*(cros0*t10 + cros1*t11 + cros2*t12);
					
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
			VlakRen *vlr= is->vlrorig;
			short de= 0;
			
			if(v1==vlr->v1 || v2==vlr->v1 || v3==vlr->v1 || v4==vlr->v1) de++;
			if(v1==vlr->v2 || v2==vlr->v2 || v3==vlr->v2 || v4==vlr->v2) de++;
			if(v1==vlr->v3 || v2==vlr->v3 || v3==vlr->v3 || v4==vlr->v3) de++;
			if(vlr->v4) {
				if(v1==vlr->v4 || v2==vlr->v4 || v3==vlr->v4 || v4==vlr->v4) de++;
			}
			if(de) {
				
				/* so there's a shared edge or vertex, let's intersect ray with vlr
				itself, if that's true we can safely return 1, otherwise we assume
				the intersection is invalid, 0 */
				
				if(is->vlrcontr==NULL) {
					is->vlrcontr= vlr;
					vlrisect= intersection2(vlr, -r0, -r1, -r2, is->start[0], is->start[1], is->start[2]);
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
/* starts with is->vlrorig */
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
	is->vlrorig->raycount= raycount;
	is->vlrcontr= NULL;	/*  to check shared edge */


	/* only for shadow! */
	if(is->mode==DDA_SHADOW) {
	
		/* check with last intersected shadow face */
		if(g_oc.vlr_last!=NULL && g_oc.vlr_last!=is->vlrorig) {
			is->vlr= g_oc.vlr_last;
			VECSUB(is->vec, is->end, is->start);
			if(intersection(is)) return 1;
		}
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

	/* for intersection */
	VECSUB(is->vec, is->end, is->start);

	if(ocx1==ocx2 && ocy1==ocy2 && ocz1==ocz2) {
		no= ocread(ocx1, ocy1, ocz1);
		if(no) {
			/* no calc, this is store */
			calc_ocval_ray(NULL, ox1, oy1, oz1, ox2, oy2, oz2);
			is->ddalabda= 1.0;
			if( testnode(is, no, ocx1, ocy1, ocz1) ) return 1;
		}
	}
	else {
		static int coh_ocx1,coh_ocx2,coh_ocy1, coh_ocy2,coh_ocz1,coh_ocz2;
		float dox, doy, doz;
		int coherent=1, nodecount=0;
		
		/* check coherence; 
			coh_test: 0=don't, 1=check 
			coherent: for current ray 
		*/
		if(coh_test) {
			if(coh_ocx1==ocx1 && coh_ocy1==ocy1 && coh_ocz1==ocz1
			   && coh_ocx2==ocx2 && coh_ocy2==ocy2 && coh_ocz2==ocz2);
			else coh_test= 0;
		}
		
		/* calc labda en ld */
		dox= ox1-ox2;
		doy= oy1-oy2;
		doz= oz1-oz2;
		
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
		
		vec2[0]= ox1;
		vec2[1]= oy1;
		vec2[2]= oz1;
		
		/* this loop has been constructed to make sure the first and last node of ray
		   are always included, even when ddalabda==1.0 or larger */

		while(TRUE) {

			no= ocread(xo, yo, zo);
			nodecount++;
			if(no) {
				if(nodecount>3) coherent= 0;
				
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
			else if(coh_test) {
				coherent_ray++;
				return 0;
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
		if(coherent) {
			coh_test= 1;
			coh_ocx1= ocx1; coh_ocy1= ocy1;coh_ocz1= ocz1;
			coh_ocx2= ocx2; coh_ocy2= ocy2;coh_ocz2= ocz2;
		}
		else coh_test= 0;
	}
	
	/* reached end, no intersections found */
	g_oc.vlr_last= NULL;
	return 0;
}		


static void shade_ray(Isect *is, ShadeInput *shi, ShadeResult *shr, int mask)
{
	VlakRen *vlr= is->vlr;
	int flip= 0;
	
	/* set up view vector */
	VECCOPY(shi->view, is->vec);

	/* render co */
	shi->co[0]= is->start[0]+is->labda*(shi->view[0]);
	shi->co[1]= is->start[1]+is->labda*(shi->view[1]);
	shi->co[2]= is->start[2]+is->labda*(shi->view[2]);
	
	Normalise(shi->view);

	shi->vlr= vlr;
	shi->mat= vlr->mat;
	shi->matren= shi->mat->ren;
	
	/* face normal, check for flip */
	if((shi->matren->mode & MA_RAYTRANSP)==0) {
		float l= vlr->n[0]*shi->view[0]+vlr->n[1]*shi->view[1]+vlr->n[2]*shi->view[2];
		if(l<0.0) {	
			flip= 1;
			vlr->n[0]= -vlr->n[0];
			vlr->n[1]= -vlr->n[1];
			vlr->n[2]= -vlr->n[2];
			// only flip lower 4 bits
			vlr->puno= vlr->puno ^ 15;
		}
	}
	
	// Osa structs we leave unchanged now
	shi->osatex= 0;	
	
	// but, set O structs zero where it can confuse texture code
	if(shi->matren->texco & (TEXCO_NORM|TEXCO_REFL) ) {
		O.dxno[0]= O.dxno[1]= O.dxno[2]= 0.0;
		O.dyno[0]= O.dyno[1]= O.dyno[2]= 0.0;
	}

	if(vlr->v4) {
		if(is->isect==2) 
			shade_input_set_coords(shi, is->u, is->v, 2, 1, 3);
		else
			shade_input_set_coords(shi, is->u, is->v, 0, 1, 3);
	}
	else {
		shade_input_set_coords(shi, is->u, is->v, 0, 1, 2);
	}
	
	shi->osatex= (shi->matren->texco & TEXCO_OSA);

	if(is->mode==DDA_SHADOW_TRA) shade_color(shi, shr);
	else {

		shade_lamp_loop(shi, shr, mask);	

		if(shi->matren->translucency!=0.0) {
			ShadeResult shr_t;
			VecMulf(shi->vn, -1.0);
			VecMulf(vlr->n, -1.0);
			shade_lamp_loop(shi, &shr_t, mask);
			shr->diff[0]+= shi->matren->translucency*shr_t.diff[0];
			shr->diff[1]+= shi->matren->translucency*shr_t.diff[1];
			shr->diff[2]+= shi->matren->translucency*shr_t.diff[2];
			VecMulf(shi->vn, -1.0);
			VecMulf(vlr->n, -1.0);
		}
	}
	
	if(flip) {	
		vlr->n[0]= -vlr->n[0];
		vlr->n[1]= -vlr->n[1];
		vlr->n[2]= -vlr->n[2];
		// only flip lower 4 bits
		vlr->puno= vlr->puno ^ 15;
	}
}

static void refraction(float *refract, float *n, float *view, float index)
{
	float dot, fac;

	VECCOPY(refract, view);
	index= 1.0/index;
	
	dot= view[0]*n[0] + view[1]*n[1] + view[2]*n[2];

	if(dot>0.0) {
		fac= 1.0 - (1.0 - dot*dot)*index*index;
		if(fac<= 0.0) return;
		fac= -dot*index + sqrt(fac);
	}
	else {
		index = 1.0/index;
		fac= 1.0 - (1.0 - dot*dot)*index*index;
		if(fac<= 0.0) return;
		fac= -dot*index - sqrt(fac);
	}

	refract[0]= index*view[0] + fac*n[0];
	refract[1]= index*view[1] + fac*n[1];
	refract[2]= index*view[2] + fac*n[2];
}

static void calc_dx_dy_refract(float *ref, float *n, float *view, float index, int smooth)
{
	float dref[3], dview[3], dnor[3];
	
	refraction(ref, n, view, index);
	
	dview[0]= view[0]+ O.dxview;
	dview[1]= view[1];
	dview[2]= view[2];

	if(smooth) {
		VECADD(dnor, n, O.dxno);
		refraction(dref, dnor, dview, index);
	}
	else {
		refraction(dref, n, dview, index);
	}
	VECSUB(O.dxrefract, ref, dref);
	
	dview[0]= view[0];
	dview[1]= view[1]+ O.dyview;

	if(smooth) {
		VECADD(dnor, n, O.dyno);
		refraction(dref, dnor, dview, index);
	}
	else {
		refraction(dref, n, dview, index);
	}
	VECSUB(O.dyrefract, ref, dref);
	
}


/* orn = original face normal */
static void reflection(float *ref, float *n, float *view, float *orn)
{
	float f1;
	
	f1= -2.0*(n[0]*view[0]+ n[1]*view[1]+ n[2]*view[2]);
	
	if(orn==NULL) {
		// heuristic, should test this! is to prevent normal going to the back
		if(f1> -0.2) f1= -0.2;
	}
	
	ref[0]= (view[0]+f1*n[0]);
	ref[1]= (view[1]+f1*n[1]);
	ref[2]= (view[2]+f1*n[2]);

	if(orn) {
		/* test phong normals, then we should prevent vector going to the back */
		f1= ref[0]*orn[0]+ ref[1]*orn[1]+ ref[2]*orn[2];
		if(f1>0.0) {
			f1+= .01;
			ref[0]-= f1*orn[0];
			ref[1]-= f1*orn[1];
			ref[2]-= f1*orn[2];
		}
	}
}

static void color_combine(float *result, float fac1, float fac2, float *col1, float *col2)
{
	float col1t[3], col2t[3];
	
	col1t[0]= sqrt(col1[0]);
	col1t[1]= sqrt(col1[1]);
	col1t[2]= sqrt(col1[2]);
	col2t[0]= sqrt(col2[0]);
	col2t[1]= sqrt(col2[1]);
	col2t[2]= sqrt(col2[2]);

	result[0]= (fac1*col1t[0] + fac2*col2t[0]);
	result[0]*= result[0];
	result[1]= (fac1*col1t[1] + fac2*col2t[1]);
	result[1]*= result[1];
	result[2]= (fac1*col1t[2] + fac2*col2t[2]);
	result[2]*= result[2];
}

/* the main recursive tracer itself */
static void traceray(short depth, float *start, float *vec, float *col, VlakRen *vlr, int mask)
{
	ShadeInput shi;
	ShadeResult shr;
	Isect isec;
	float f, f1, fr, fg, fb;
	float ref[3];
	
	VECCOPY(isec.start, start);
	isec.end[0]= start[0]+g_oc.ocsize*vec[0];
	isec.end[1]= start[1]+g_oc.ocsize*vec[1];
	isec.end[2]= start[2]+g_oc.ocsize*vec[2];
	isec.mode= DDA_MIRROR;
	isec.vlrorig= vlr;

	if( d3dda(&isec) ) {
	
		shade_ray(&isec, &shi, &shr, mask);
		
		if(depth>0) {

			if(shi.matren->mode & MA_RAYTRANSP && shr.alpha!=1.0) {
				float f, f1, refract[3], tracol[3];
				
				refraction(refract, shi.vn, shi.view, shi.matren->ang);
				traceray(depth-1, shi.co, refract, tracol, shi.vlr, mask);
				
				f= shr.alpha; f1= 1.0-f;
				shr.diff[0]= f*shr.diff[0] + f1*tracol[0];
				shr.diff[1]= f*shr.diff[1] + f1*tracol[1];
				shr.diff[2]= f*shr.diff[2] + f1*tracol[2];
				shr.alpha= 1.0;
			}

			if(shi.matren->mode & MA_RAYMIRROR) {
				f= shi.matren->ray_mirror;
				if(f!=0.0) f*= fresnel_fac(shi.view, shi.vn, shi.matren->fresnel_mir_i, shi.matren->fresnel_mir);
			}
			else f= 0.0;
			
			if(f!=0.0) {
			
				reflection(ref, shi.vn, shi.view, NULL);			
				traceray(depth-1, shi.co, ref, col, shi.vlr, mask);
			
				f1= 1.0-f;

				/* combine */
				//color_combine(col, f*fr*(1.0-shr.spec[0]), f1, col, shr.diff);
				//col[0]+= shr.spec[0];
				//col[1]+= shr.spec[1];
				//col[2]+= shr.spec[2];
				
				fr= shi.matren->mirr;
				fg= shi.matren->mirg;
				fb= shi.matren->mirb;
		
				col[0]= f*fr*(1.0-shr.spec[0])*col[0] + f1*shr.diff[0] + shr.spec[0];
				col[1]= f*fg*(1.0-shr.spec[1])*col[1] + f1*shr.diff[1] + shr.spec[1];
				col[2]= f*fb*(1.0-shr.spec[2])*col[2] + f1*shr.diff[2] + shr.spec[2];
			}
			else {
				col[0]= shr.diff[0] + shr.spec[0];
				col[1]= shr.diff[1] + shr.spec[1];
				col[2]= shr.diff[2] + shr.spec[2];
			}
		}
		else {
			col[0]= shr.diff[0] + shr.spec[0];
			col[1]= shr.diff[1] + shr.spec[1];
			col[2]= shr.diff[2] + shr.spec[2];
		}
		
	}
	else {	/* sky */
		char skycol[4];
		
		VECCOPY(shi.view, vec);
		Normalise(shi.view);

		RE_sky(shi.view, skycol);	

		col[0]= skycol[0]/255.0;
		col[1]= skycol[1]/255.0;
		col[2]= skycol[2]/255.0;

	}
}

/* **************** jitter blocks ********** */

/* calc distributed planar energy */

static void DP_energy(float *table, float *vec, int tot, float xsize, float ysize)
{
	int x, y, a;
	float *fp, force[3], result[3];
	float dx, dy, dist, min;
	
	min= MIN2(xsize, ysize);
	min*= min;
	result[0]= result[1]= 0.0;
	
	for(y= -1; y<2; y++) {
		dy= ysize*y;
		for(x= -1; x<2; x++) {
			dx= xsize*x;
			fp= table;
			for(a=0; a<tot; a++, fp+= 2) {
				force[0]= vec[0] - fp[0]-dx;
				force[1]= vec[1] - fp[1]-dy;
				dist= force[0]*force[0] + force[1]*force[1];
				if(dist < min && dist>0.0) {
					result[0]+= force[0]/dist;
					result[1]+= force[1]/dist;
				}
			}
		}
	}
	vec[0] += 0.1*min*result[0]/(float)tot;
	vec[1] += 0.1*min*result[1]/(float)tot;
	// cyclic clamping
	vec[0]= vec[0] - xsize*floor(vec[0]/xsize + 0.5);
	vec[1]= vec[1] - ysize*floor(vec[1]/ysize + 0.5);
}


float *test_jitter(int resol, int iter, float xsize, float ysize)
{
	static float jitter[2*256];
	float *fp;
	int x;
	
	/* fill table with random locations, area_size large */
	fp= jitter;
	for(x=0; x<resol*resol; x++, fp+=2) {
		fp[0]= (BLI_frand()-0.5)*xsize;
		fp[1]= (BLI_frand()-0.5)*ysize;
	}
	
	while(iter--) {
		fp= jitter;
		for(x=0; x<resol*resol; x++, fp+=2) {
			DP_energy(jitter, fp, resol*resol, xsize, ysize);
		}
	}
	return jitter;
}

// random offset of 1 in 2
static void jitter_plane_offset(float *jitter1, float *jitter2, int tot, float sizex, float sizey, float ofsx, float ofsy)
{
	float dsizex= sizex*ofsx;
	float dsizey= sizey*ofsy;
	float hsizex= 0.5*sizex, hsizey= 0.5*sizey;
	int x;
	
	for(x=tot; x>0; x--, jitter1+=2, jitter2+=2) {
		jitter2[0]= jitter1[0] + dsizex;
		jitter2[1]= jitter1[1] + dsizey;
		if(jitter2[0] > hsizex) jitter2[0]-= sizex;
		if(jitter2[1] > hsizey) jitter2[1]-= sizey;
	}
}

/* table around origin, -0.5*size to 0.5*size */
static float *jitter_plane(LampRen *lar, int xs, int ys)
{
	float *fp;
	int tot, x, iter=12;
	
	tot= lar->ray_totsamp;
	
	if(lar->jitter==NULL) {
	
		fp=lar->jitter= MEM_mallocN(4*tot*2*sizeof(float), "lamp jitter tab");
		
		/* fill table with random locations, area_size large */
		for(x=0; x<tot; x++, fp+=2) {
			fp[0]= (BLI_frand()-0.5)*lar->area_size;
			fp[1]= (BLI_frand()-0.5)*lar->area_sizey;
		}
		
		while(iter--) {
			fp= lar->jitter;
			for(x=tot; x>0; x--, fp+=2) {
				DP_energy(lar->jitter, fp, tot, lar->area_size, lar->area_sizey);
			}
		}
		
		jitter_plane_offset(lar->jitter, lar->jitter+2*tot, tot, lar->area_size, lar->area_sizey, 0.5, 0.0);
		jitter_plane_offset(lar->jitter, lar->jitter+4*tot, tot, lar->area_size, lar->area_sizey, 0.5, 0.5);
		jitter_plane_offset(lar->jitter, lar->jitter+6*tot, tot, lar->area_size, lar->area_sizey, 0.0, 0.5);
	}
		
	if(lar->ray_samp_type & LA_SAMP_JITTER) {
		static float jitter[2*256];
		jitter_plane_offset(lar->jitter, jitter, tot, lar->area_size, lar->area_sizey, BLI_frand(), BLI_frand());
		return jitter;
	}
	if(lar->ray_samp_type & LA_SAMP_DITHER) {
		return lar->jitter + 2*tot*((xs & 1)+2*(ys & 1));
	}
	
	return lar->jitter;
}


/* ***************** main calls ************** */


/* extern call from render loop */
void ray_trace(ShadeInput *shi, ShadeResult *shr, int mask)
{
	VlakRen *vlr;
	float i, f, f1, fr, fg, fb, vec[3], mircol[3], tracol[3];
	int do_tra, do_mir;
	
	do_tra= ((shi->matren->mode & MA_RAYTRANSP) && shr->alpha!=1.0);
	do_mir= ((shi->matren->mode & MA_RAYMIRROR) && shi->matren->ray_mirror!=0.0);
	vlr= shi->vlr;
	
	coh_test= 0;		// reset coherence optimize

	if(R.r.mode & R_OSA) {
		float accum[3], rco[3], ref[3];
		float accur[3], refract[3], divr=0.0, div= 0.0;
		int j;
		
		if(do_tra) calc_dx_dy_refract(refract, shi->vn, shi->view, shi->matren->ang, vlr->flag & R_SMOOTH);
		if(do_mir) {
			if(vlr->flag & R_SMOOTH) 
				reflection(ref, shi->vn, shi->view, vlr->n);
			else
				reflection(ref, shi->vn, shi->view, NULL);
		}

		accum[0]= accum[1]= accum[2]= 0.0;
		accur[0]= accur[1]= accur[2]= 0.0;
		
		for(j=0; j<R.osa; j++) {
			if(mask & 1<<j) {
				
				rco[0]= shi->co[0] + (jit[j][0]-0.5)*O.dxco[0] + (jit[j][1]-0.5)*O.dyco[0];
				rco[1]= shi->co[1] + (jit[j][0]-0.5)*O.dxco[1] + (jit[j][1]-0.5)*O.dyco[1];
				rco[2]= shi->co[2] + (jit[j][0]-0.5)*O.dxco[2] + (jit[j][1]-0.5)*O.dyco[2];
				
				if(do_tra) {
					vec[0]= refract[0] + (jit[j][0]-0.5)*O.dxrefract[0] + (jit[j][1]-0.5)*O.dyrefract[0] ;
					vec[1]= refract[1] + (jit[j][0]-0.5)*O.dxrefract[1] + (jit[j][1]-0.5)*O.dyrefract[1] ;
					vec[2]= refract[2] + (jit[j][0]-0.5)*O.dxrefract[2] + (jit[j][1]-0.5)*O.dyrefract[2] ;

					traceray(shi->matren->ray_depth_tra, rco, vec, tracol, shi->vlr, mask);
					
					VECADD(accur, accur, tracol);
					divr+= 1.0;
				}

				if(do_mir) {
					vec[0]= ref[0] + 2.0*(jit[j][0]-0.5)*O.dxref[0] + 2.0*(jit[j][1]-0.5)*O.dyref[0] ;
					vec[1]= ref[1] + 2.0*(jit[j][0]-0.5)*O.dxref[1] + 2.0*(jit[j][1]-0.5)*O.dyref[1] ;
					vec[2]= ref[2] + 2.0*(jit[j][0]-0.5)*O.dxref[2] + 2.0*(jit[j][1]-0.5)*O.dyref[2] ;
					
					/* prevent normal go to backside */
					i= vec[0]*vlr->n[0]+ vec[1]*vlr->n[1]+ vec[2]*vlr->n[2];
					if(i>0.0) {
						i+= .01;
						vec[0]-= i*vlr->n[0];
						vec[1]-= i*vlr->n[1];
						vec[2]-= i*vlr->n[2];
					}
					
					/* we use a new mask here, only shadow uses it */
					/* result in accum, this is copied to shade_lamp_loop */
					traceray(shi->matren->ray_depth, rco, vec, mircol, shi->vlr, 1<<j);
					
					VECADD(accum, accum, mircol);
					div+= 1.0;
				}
			}
		}
		
		if(divr!=0.0) {
			f= shr->alpha; f1= 1.0-f; f1/= divr;
			shr->diff[0]= f*shr->diff[0] + f1*accur[0];
			shr->diff[1]= f*shr->diff[1] + f1*accur[1];
			shr->diff[2]= f*shr->diff[2] + f1*accur[2];
			shr->alpha= 1.0;
		}
		
		if(div!=0.0) {
			i= shi->matren->ray_mirror*fresnel_fac(shi->view, shi->vn, shi->matren->fresnel_mir_i, shi->matren->fresnel_mir);
			fr= shi->matren->mirr;
			fg= shi->matren->mirg;
			fb= shi->matren->mirb;
	
			/* result */
			f= i*fr*(1.0-shr->spec[0]);	f1= 1.0-i; f/= div;
			shr->diff[0]= f*accum[0] + f1*shr->diff[0];
			
			f= i*fg*(1.0-shr->spec[1]);	f1= 1.0-i; f/= div;
			shr->diff[1]= f*accum[1] + f1*shr->diff[1];
			
			f= i*fb*(1.0-shr->spec[2]);	f1= 1.0-i; f/= div;
			shr->diff[2]= f*accum[2] + f1*shr->diff[2];
		}
	}
	else {
		
		if(do_tra) {
			float refract[3];
			
			refraction(refract, shi->vn, shi->view, shi->matren->ang);
			traceray(shi->matren->ray_depth_tra, shi->co, refract, tracol, shi->vlr, mask);
			
			f= shr->alpha; f1= 1.0-f;
			shr->diff[0]= f*shr->diff[0] + f1*tracol[0];
			shr->diff[1]= f*shr->diff[1] + f1*tracol[1];
			shr->diff[2]= f*shr->diff[2] + f1*tracol[2];
			shr->alpha= 1.0;
		}
		
		if(do_mir) {
		
			i= shi->matren->ray_mirror*fresnel_fac(shi->view, shi->vn, shi->matren->fresnel_mir_i, shi->matren->fresnel_mir);
			if(i!=0.0) {
				fr= shi->matren->mirr;
				fg= shi->matren->mirg;
				fb= shi->matren->mirb;
	
				if(vlr->flag & R_SMOOTH) 
					reflection(vec, shi->vn, shi->view, vlr->n);
				else
					reflection(vec, shi->vn, shi->view, NULL);
		
				traceray(shi->matren->ray_depth, shi->co, vec, mircol, shi->vlr, mask);
				
				f= i*fr*(1.0-shr->spec[0]);	f1= 1.0-i;
				shr->diff[0]= f*mircol[0] + f1*shr->diff[0];
				
				f= i*fg*(1.0-shr->spec[1]);	f1= 1.0-i;
				shr->diff[1]= f*mircol[1] + f1*shr->diff[1];
				
				f= i*fb*(1.0-shr->spec[2]);	f1= 1.0-i;
				shr->diff[2]= f*mircol[2] + f1*shr->diff[2];
			}
		}
	}
}

/* no premul here! */
static void addAlphaLight(float *old, float *over)
{
	float div= old[3]+over[3];

	if(div > 0.0001) {
		old[0]= (over[3]*over[0] + old[3]*old[0])/div;
		old[1]= (over[3]*over[1] + old[3]*old[1])/div;
		old[2]= (over[3]*over[2] + old[3]*old[2])/div;
	}
	old[3]= over[3] + (1-over[3])*old[3];

}

static void ray_trace_shadow_tra(Isect *is, int depth)
{
	/* ray to lamp, find first face that intersects, check alpha properties,
	   if it has alpha<1  continue. exit when alpha is full */
	ShadeInput shi;
	ShadeResult shr;

	if( d3dda(is)) {
		float col[4];
		/* we got a face */
		
		shade_ray(is, &shi, &shr, 0);	// mask not needed
		
		/* add color */
		VECCOPY(col, shr.diff);
		col[3]= shr.alpha;
		addAlphaLight(is->col, col);

		if(depth>0 && is->col[3]<1.0) {
			
			/* adapt isect struct */
			VECCOPY(is->start, shi.co);
			is->vlrorig= shi.vlr;

			ray_trace_shadow_tra(is, depth-1);
		}
		else if(is->col[3]>1.0) is->col[3]= 1.0;

	}
}

/* not used, test function for ambient occlusion (yaf: pathlight) */
/* main problem; has to be called within shading loop, giving unwanted recursion */
int ray_trace_shadow_rad(ShadeInput *ship, ShadeResult *shr)
{
	static int counter=0, only_one= 0;
	extern float hashvectf[];
	Isect isec;
	ShadeInput shi;
	ShadeResult shr_t;
	float vec[3], accum[3], div= 0.0;
	int a;
	
	if(only_one) {
		return 0;
	}
	only_one= 1;
	
	accum[0]= accum[1]= accum[2]= 0.0;
	isec.mode= DDA_MIRROR;
	isec.vlrorig= ship->vlr;
	
	for(a=0; a<8*8; a++) {
		
		counter+=3;
		counter %= 768;
		VECCOPY(vec, hashvectf+counter);
		if(ship->vn[0]*vec[0]+ship->vn[1]*vec[1]+ship->vn[2]*vec[2]>0.0) {
			vec[0]-= vec[0];
			vec[1]-= vec[1];
			vec[2]-= vec[2];
		}
		VECCOPY(isec.start, ship->co);
		isec.end[0]= isec.start[0] + g_oc.ocsize*vec[0];
		isec.end[1]= isec.start[1] + g_oc.ocsize*vec[1];
		isec.end[2]= isec.start[2] + g_oc.ocsize*vec[2];
		
		if( d3dda(&isec)) {
			float fac;
			shade_ray(&isec, &shi, &shr_t, 0);	// mask not needed
			fac= isec.labda*isec.labda;
			fac= 1.0;
			accum[0]+= fac*(shr_t.diff[0]+shr_t.spec[0]);
			accum[1]+= fac*(shr_t.diff[1]+shr_t.spec[1]);
			accum[2]+= fac*(shr_t.diff[2]+shr_t.spec[2]);
			div+= fac;
		}
		else div+= 1.0;
	}
	
	if(div!=0.0) {
		shr->diff[0]+= accum[0]/div;
		shr->diff[1]+= accum[1]/div;
		shr->diff[2]+= accum[2]/div;
	}
	shr->alpha= 1.0;
	
	only_one= 0;
	return 1;
}

/* aolight: function to create random unit sphere vectors for total random sampling */
void RandomSpherical(float *v)
{
	float r;
	v[2] = 2.f*BLI_frand()-1.f;
	if ((r = 1.f - v[2]*v[2])>0.f) {
		float a = 6.283185307f*BLI_frand();
		r = sqrt(r);
		v[0] = r * cos(a);
		v[1] = r * sin(a);
	}
	else v[2] = 1.f;
}

/* calc distributed spherical energy */
static void DS_energy(float *sphere, int tot, float *vec)
{
	float *fp, fac, force[3], res[3];
	int a;
	
	res[0]= res[1]= res[2]= 0.0;
	
	for(a=0, fp=sphere; a<tot; a++, fp+=3) {
		VecSubf(force, vec, fp);
		fac= force[0]*force[0] + force[1]*force[1] + force[2]*force[2];
		if(fac!=0.0) {
			fac= 1.0/fac;
			res[0]+= fac*force[0];
			res[1]+= fac*force[1];
			res[2]+= fac*force[2];
		}
	}

	VecMulf(res, 0.5);
	VecAddf(vec, vec, res);
	Normalise(vec);
	
}

static void DistributedSpherical(float *sphere, int tot, int iter)
{
	float *fp;
	int a;

	/* init */
	fp= sphere;
	for(a=0; a<tot; a++, fp+= 3) {
		RandomSpherical(fp);
	}
	
	while(iter--) {
		for(a=0, fp= sphere; a<tot; a++, fp+= 3) {
			DS_energy(sphere, tot, fp);
		}
	}
}

static float *sphere_sampler(int type, int resol, float *nrm)
{
	static float sphere[2*3*256], sphere1[2*3*256];
	static int last_distr= 0, tot;
	float *vec;
	
	if(resol>16) return sphere;
	
	tot= 2*resol*resol;

	if (type & WO_AORNDSMP) {
		int a;
		
		/* total random sampling */
		vec= sphere;
		for (a=0; a<tot; a++, vec+=3) {
			RandomSpherical(vec);
		}
	} 
	else {
		float cosf, sinf, cost, sint;
		float ang, *vec1;
		int a;
		
		if(last_distr!=resol) DistributedSpherical(sphere, tot, 16);
		last_distr= resol;
		
		// random rotation
		ang= BLI_frand();
		sinf= sin(ang); cosf= cos(ang);
		ang= BLI_frand();
		sint= sin(ang); cost= cos(ang);
		
		vec= sphere;
		vec1= sphere1;
		for (a=0; a<tot; a++, vec+=3, vec1+=3) {
			vec1[0]= cost*cosf*vec[0] - sinf*vec[1] + sint*cosf*vec[2];
			vec1[1]= cost*sinf*vec[0] + cosf*vec[1] + sint*sinf*vec[2];
			vec1[2]= -sint*vec[0] + cost*vec[2];			
		}
		return sphere1;
	}
#if 0
	{	/* stratified uniform sampling */
		float gdiv = 1.0/resol;
		float gdiv2p = gdiv*2.0*M_PI;
		float d, z1, z2, sqz1, sz2, cz2;
		float ru[3], rv[3];
		int x, y;

		last_distr= 0;
		

		/* calculate the two perpendicular vectors */
		if ((nrm[0]==0.0) && (nrm[1]==0.0)) {
			if (nrm[2]<0) ru[0]=-1; else ru[0]=1;
			ru[1] = ru[2] = 0;
			rv[0] = rv[2] = 0;
			rv[1] = 1;
		}
		else {
			ru[0] = nrm[1];
			ru[1] = -nrm[0];
			ru[2] = 0.0;
			d = ru[0]*ru[0] + ru[1]*ru[1];
			if (d!=0) {
				d = 1.0/sqrt(d);
				ru[0] *= d;
				ru[1] *= d;
			}
			Crossf(rv, nrm, ru);
		}

		vec= sphere;
		for (x=0; x<resol; x++) {
			for (y=0; y<resol; y++, vec+=3) {
				z1 = (x + BLI_frand()) * gdiv;
				z2 = (y + BLI_frand()) * gdiv2p;
				if ((sqz1 = 1.0-z1*z1)<0) sqz1=0; else sqz1=sqrt(sqz1);
				sz2 = sin(z2);
				cz2 = cos(z2);
				vec[0] = sqz1*(cz2*ru[0] + sz2*rv[0]) + nrm[0]*z1;
				vec[1] = sqz1*(cz2*ru[1] + sz2*rv[1]) + nrm[1]*z1;
				vec[2] = sqz1*(cz2*ru[2] + sz2*rv[2]) + nrm[2]*z1;
			}
		}
	}
#endif	
	return sphere;
}


/* extern call from shade_lamp_loop, ambient occlusion calculus */
void ray_ao(ShadeInput *shi, World *wrld, float *shadfac)
{
	Isect isec;
	float *vec, *nrm, div, sh=0;
	float maxdist = wrld->aodist;
	int tot, actual;

	VECCOPY(isec.start, shi->co);
	isec.vlrorig= shi->vlr;
	isec.mode= DDA_SHADOW;
	coh_test= 0;		// reset coherence optimize

	shadfac[0]= shadfac[1]= shadfac[2]= 0.0;

	/* if sky texture used, these values have to be reset back to original */
	if(wrld->aocolor==WO_AOSKYCOL && G.scene->world) {
		R.wrld.horr= G.scene->world->horr;
		R.wrld.horg= G.scene->world->horg;
		R.wrld.horb= G.scene->world->horb;
		R.wrld.zenr= G.scene->world->zenr;
		R.wrld.zeng= G.scene->world->zeng;
		R.wrld.zenb= G.scene->world->zenb;
	}
	
	nrm= shi->vlr->n;
	vec= sphere_sampler(wrld->aomode, wrld->aosamp, nrm);
	
	// warning: since we use full sphere now, and dotproduct is below, we do twice as much
	tot= 2*wrld->aosamp*wrld->aosamp;
	actual= 0;
	
	while(tot--) {
		
		if ((vec[0]*nrm[0] + vec[1]*nrm[1] + vec[2]*nrm[2]) > 0.0) {
			
			actual++;
			
			isec.end[0] = shi->co[0] - maxdist*vec[0];
			isec.end[1] = shi->co[1] - maxdist*vec[1];
			isec.end[2] = shi->co[2] - maxdist*vec[2];
			
			/* do the trace */
			if (d3dda(&isec)) {
				if (wrld->aomode & WO_AODIST) sh+= exp(-isec.labda*wrld->aodistfac); 
				else sh+= 1.0;
			}
			else if(wrld->aocolor!=WO_AOPLAIN) {
				char skycol[4];
				float fac, view[3];
				
				view[0]= -vec[0];
				view[1]= -vec[1];
				view[2]= -vec[2];
				Normalise(view);
				
				if(wrld->aocolor==WO_AOSKYCOL) {
					fac= 0.5*(1.0+view[0]*R.grvec[0]+ view[1]*R.grvec[1]+ view[2]*R.grvec[2]);
					shadfac[0]+= (1.0-fac)*R.wrld.horr + fac*R.wrld.zenr;
					shadfac[1]+= (1.0-fac)*R.wrld.horg + fac*R.wrld.zeng;
					shadfac[2]+= (1.0-fac)*R.wrld.horb + fac*R.wrld.zenb;
				}
				else {
					RE_sky(view, skycol);
					shadfac[0]+= skycol[0]/255.0;
					shadfac[1]+= skycol[1]/255.0;
					shadfac[2]+= skycol[2]/255.0;
				}
			}
		}
		// samples
		vec+= 3;
	}
	
	div= 1.0/(float)(actual);
	shadfac[3] = 1.0 - (sh*div);
	
	if(wrld->aocolor!=WO_AOPLAIN) {
		shadfac[0] *= div;
		shadfac[1] *= div;
		shadfac[2] *= div;
	}
}



/* extern call from shade_lamp_loop */
void ray_shadow(ShadeInput *shi, LampRen *lar, float *shadfac, int mask)
{
	Isect isec;
	Material stored;
	float fac, div=0.0, lampco[3];

	if(shi->matren->mode & MA_SHADOW_TRA) {
		isec.mode= DDA_SHADOW_TRA;
		/* needed to prevent shade_ray changing matren (textures) */
		stored= *(shi->matren);
	}
	else isec.mode= DDA_SHADOW;
	
	shadfac[3]= 1.0;	// 1=full light
	coh_test= 0;		// reset coherence optimize
	
	if(lar->type==LA_SUN || lar->type==LA_HEMI) {
		lampco[0]= shi->co[0] - g_oc.ocsize*lar->vec[0];
		lampco[1]= shi->co[1] - g_oc.ocsize*lar->vec[1];
		lampco[2]= shi->co[2] - g_oc.ocsize*lar->vec[2];
	}
	else {
		VECCOPY(lampco, lar->co);
	}
	
	/* shadow and soft not implemented yet */
	if(lar->ray_totsamp<2 ||  isec.mode == DDA_SHADOW_TRA) {
		if(R.r.mode & R_OSA) {
			float accum[4]={0.0, 0.0, 0.0, 0.0};
			int j;
			
			fac= 0.0;
			
			for(j=0; j<R.osa; j++) {
				if(mask & 1<<j) {
					/* set up isec */
					isec.start[0]= shi->co[0] + (jit[j][0]-0.5)*O.dxco[0] + (jit[j][1]-0.5)*O.dyco[0] ;
					isec.start[1]= shi->co[1] + (jit[j][0]-0.5)*O.dxco[1] + (jit[j][1]-0.5)*O.dyco[1] ;
					isec.start[2]= shi->co[2] + (jit[j][0]-0.5)*O.dxco[2] + (jit[j][1]-0.5)*O.dyco[2] ;
					VECCOPY(isec.end, lampco);
					isec.vlrorig= shi->vlr;
					
					if(isec.mode==DDA_SHADOW_TRA) {
						isec.col[0]= isec.col[1]= isec.col[2]=  1.0;
						isec.col[3]= 0.0;	//alpha

						ray_trace_shadow_tra(&isec, DEPTH_SHADOW_TRA);
						
						accum[0]+= isec.col[0]; accum[1]+= isec.col[1];
						accum[2]+= isec.col[2]; accum[3]+= isec.col[3];
					}
					else if( d3dda(&isec) ) fac+= 1.0;
					div+= 1.0;
				}
			}
			if(isec.mode==DDA_SHADOW_TRA) {
					// alpha to 'light'
				accum[3]/= div;
				shadfac[3]= 1.0-accum[3];
				shadfac[0]= shadfac[3]+accum[0]*accum[3]/div;
				shadfac[1]= shadfac[3]+accum[1]*accum[3]/div;
				shadfac[2]= shadfac[3]+accum[2]*accum[3]/div;
			}
			else shadfac[3]= 1.0-fac/div;
		}
		else {
			/* set up isec */
			VECCOPY(isec.start, shi->co);
			VECCOPY(isec.end, lampco);
			isec.vlrorig= shi->vlr;

			if(isec.mode==DDA_SHADOW_TRA) {
				isec.col[0]= isec.col[1]= isec.col[2]=  1.0;
				isec.col[3]= 0.0;	//alpha

				ray_trace_shadow_tra(&isec, DEPTH_SHADOW_TRA);

				VECCOPY(shadfac, isec.col);
					// alpha to 'light'
				shadfac[3]= 1.0-isec.col[3];
				shadfac[0]= shadfac[3]+shadfac[0]*isec.col[3];
				shadfac[1]= shadfac[3]+shadfac[1]*isec.col[3];
				shadfac[2]= shadfac[3]+shadfac[2]*isec.col[3];
			}
			else if( d3dda(&isec)) shadfac[3]= 0.0;
		}
	}
	else {
		/* area soft shadow */
		float *jitlamp;
		float vec[3];
		int a, j=0;
		
		VECCOPY(isec.start, shi->co);
		isec.vlrorig= shi->vlr;

		fac= 0.0;
		jitlamp= jitter_plane(lar, floor(shi->xs), floor(shi->ys));

		a= lar->ray_totsamp;
		
		while(a--) {
			
			vec[0]= jitlamp[0];
			vec[1]= jitlamp[1];
			vec[2]= 0.0;
			Mat3MulVecfl(lar->mat, vec);
			
			isec.end[0]= lampco[0]+vec[0];
			isec.end[1]= lampco[1]+vec[1];
			isec.end[2]= lampco[2]+vec[2];
			
			if(R.r.mode & R_OSA) {
				isec.start[0]= shi->co[0] + (jit[j][0]-0.5)*O.dxco[0] + (jit[j][1]-0.5)*O.dyco[0] ;
				isec.start[1]= shi->co[1] + (jit[j][0]-0.5)*O.dxco[1] + (jit[j][1]-0.5)*O.dyco[1] ;
				isec.start[2]= shi->co[2] + (jit[j][0]-0.5)*O.dxco[2] + (jit[j][1]-0.5)*O.dyco[2] ;
				j++;
				if(j>=R.osa) j= 0;
			}
			
			if( d3dda(&isec) ) fac+= 1.0;
			
			jitlamp+= 2;
		}
		// sqrt makes nice umbra effect
		if(lar->ray_samp_type & LA_SAMP_UMBRA)
			shadfac[3]= sqrt(1.0-fac/((float)lar->ray_totsamp));
		else
			shadfac[3]= 1.0-fac/((float)lar->ray_totsamp);
	}

	if(shi->matren->mode & MA_SHADOW_TRA) {
		/* needed to prevent shade_ray changing matren (textures) */
		*(shi->matren)= stored;
	}
	
	
}

