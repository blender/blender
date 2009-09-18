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
 * The Original Code is Copyright (C) 1990-1998 NeoGeo BV.
 * All rights reserved.
 *
 * Contributors: 2004/2005 Blender Foundation, full recode
 *
 * ***** END GPL LICENSE BLOCK *****
 */

/* IMPORTANT NOTE: this code must be independent of any other render code
   to use it outside the renderer! */

#include <math.h>
#include <string.h>
#include <stdlib.h>
#include <float.h>

#include "MEM_guardedalloc.h"

#include "DNA_material_types.h"

#include "BKE_utildefines.h"

#include "BLI_arithb.h"

#include "RE_raytrace.h"

/* ********** structs *************** */

#define BRANCH_ARRAY 1024
#define NODE_ARRAY 4096

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
	struct RayFace *v[8];
	int ob[8];
	struct OcVal ov[8];
	struct Node *next;
} Node;

typedef struct Octree {
	struct Branch **adrbranch;
	struct Node **adrnode;
	float ocsize;	/* ocsize: mult factor,  max size octree */
	float ocfacx,ocfacy,ocfacz;
	float min[3], max[3];
	int ocres;
	int branchcount, nodecount;
	char *ocface; /* during building only */
	RayCoordsFunc coordsfunc;
	RayCheckFunc checkfunc;
	RayObjectTransformFunc transformfunc;
	void *userdata;
} Octree;

/* ******** globals ***************** */

/* just for statistics */
static int raycount;
static int accepted, rejected, coherent_ray;


/* **************** ocval method ******************* */
/* within one octree node, a set of 3x15 bits defines a 'boundbox' to OR with */

#define OCVALRES	15
#define BROW16(min, max)      (((max)>=OCVALRES? 0xFFFF: (1<<(max+1))-1) - ((min>0)? ((1<<(min))-1):0) )

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
	ov->ocx= BROW16(ocmin, ocmax);
	
	ocmin= OCVALRES*(min[1]-y); 
	ocmax= OCVALRES*(max[1]-y);
	ov->ocy= BROW16(ocmin, ocmax);
	
	ocmin= OCVALRES*(min[2]-z); 
	ocmax= OCVALRES*(max[2]-z);
	ov->ocz= BROW16(ocmin, ocmax);

}

static void calc_ocval_ray(OcVal *ov, float xo, float yo, float zo, float *vec1, float *vec2)
{
	int ocmin, ocmax;
	
	if(vec1[0]<vec2[0]) {
		ocmin= OCVALRES*(vec1[0] - xo);
		ocmax= OCVALRES*(vec2[0] - xo);
	} else {
		ocmin= OCVALRES*(vec2[0] - xo);
		ocmax= OCVALRES*(vec1[0] - xo);
	}
	ov->ocx= BROW16(ocmin, ocmax);

	if(vec1[1]<vec2[1]) {
		ocmin= OCVALRES*(vec1[1] - yo);
		ocmax= OCVALRES*(vec2[1] - yo);
	} else {
		ocmin= OCVALRES*(vec2[1] - yo);
		ocmax= OCVALRES*(vec1[1] - yo);
	}
	ov->ocy= BROW16(ocmin, ocmax);

	if(vec1[2]<vec2[2]) {
		ocmin= OCVALRES*(vec1[2] - zo);
		ocmax= OCVALRES*(vec2[2] - zo);
	} else {
		ocmin= OCVALRES*(vec2[2] - zo);
		ocmax= OCVALRES*(vec1[2] - zo);
	}
	ov->ocz= BROW16(ocmin, ocmax);
}

/* ************* octree ************** */

static Branch *addbranch(Octree *oc, Branch *br, short ocb)
{
	int index;
	
	if(br->b[ocb]) return br->b[ocb];
	
	oc->branchcount++;
	index= oc->branchcount>>12;
	
	if(oc->adrbranch[index]==NULL)
		oc->adrbranch[index]= MEM_callocN(4096*sizeof(Branch), "new oc branch");

	if(oc->branchcount>= BRANCH_ARRAY*4096) {
		printf("error; octree branches full\n");
		oc->branchcount=0;
	}
	
	return br->b[ocb]= oc->adrbranch[index]+(oc->branchcount & 4095);
}

static Node *addnode(Octree *oc)
{
	int index;
	
	oc->nodecount++;
	index= oc->nodecount>>12;
	
	if(oc->adrnode[index]==NULL)
		oc->adrnode[index]= MEM_callocN(4096*sizeof(Node),"addnode");

	if(oc->nodecount> NODE_ARRAY*NODE_ARRAY) {
		printf("error; octree nodes full\n");
		oc->nodecount=0;
	}
	
	return oc->adrnode[index]+(oc->nodecount & 4095);
}

static int face_in_node(RayFace *face, short x, short y, short z, float rtf[][3])
{
	static float nor[3], d;
	float fx, fy, fz;
	
	// init static vars 
	if(face) {
		CalcNormFloat(rtf[0], rtf[1], rtf[2], nor);
		d= -nor[0]*rtf[0][0] - nor[1]*rtf[0][1] - nor[2]*rtf[0][2];
		return 0;
	}
	
	fx= x;
	fy= y;
	fz= z;
	
	if((fx)*nor[0] + (fy)*nor[1] + (fz)*nor[2] + d > 0.0f) {
		if((fx+1)*nor[0] + (fy  )*nor[1] + (fz  )*nor[2] + d < 0.0f) return 1;
		if((fx  )*nor[0] + (fy+1)*nor[1] + (fz  )*nor[2] + d < 0.0f) return 1;
		if((fx+1)*nor[0] + (fy+1)*nor[1] + (fz  )*nor[2] + d < 0.0f) return 1;
	
		if((fx  )*nor[0] + (fy  )*nor[1] + (fz+1)*nor[2] + d < 0.0f) return 1;
		if((fx+1)*nor[0] + (fy  )*nor[1] + (fz+1)*nor[2] + d < 0.0f) return 1;
		if((fx  )*nor[0] + (fy+1)*nor[1] + (fz+1)*nor[2] + d < 0.0f) return 1;
		if((fx+1)*nor[0] + (fy+1)*nor[1] + (fz+1)*nor[2] + d < 0.0f) return 1;
	}
	else {
		if((fx+1)*nor[0] + (fy  )*nor[1] + (fz  )*nor[2] + d > 0.0f) return 1;
		if((fx  )*nor[0] + (fy+1)*nor[1] + (fz  )*nor[2] + d > 0.0f) return 1;
		if((fx+1)*nor[0] + (fy+1)*nor[1] + (fz  )*nor[2] + d > 0.0f) return 1;
	
		if((fx  )*nor[0] + (fy  )*nor[1] + (fz+1)*nor[2] + d > 0.0f) return 1;
		if((fx+1)*nor[0] + (fy  )*nor[1] + (fz+1)*nor[2] + d > 0.0f) return 1;
		if((fx  )*nor[0] + (fy+1)*nor[1] + (fz+1)*nor[2] + d > 0.0f) return 1;
		if((fx+1)*nor[0] + (fy+1)*nor[1] + (fz+1)*nor[2] + d > 0.0f) return 1;
	}
	
	return 0;
}

static void ocwrite(Octree *oc, int ob, RayFace *face, int quad, short x, short y, short z, float rtf[][3])
{
	Branch *br;
	Node *no;
	short a, oc0, oc1, oc2, oc3, oc4, oc5;

	x<<=2;
	y<<=1;

	br= oc->adrbranch[0];

	if(oc->ocres==512) {
		oc0= ((x & 1024)+(y & 512)+(z & 256))>>8;
		br= addbranch(oc, br, oc0);
	}
	if(oc->ocres>=256) {
		oc0= ((x & 512)+(y & 256)+(z & 128))>>7;
		br= addbranch(oc, br, oc0);
	}
	if(oc->ocres>=128) {
		oc0= ((x & 256)+(y & 128)+(z & 64))>>6;
		br= addbranch(oc, br, oc0);
	}

	oc0= ((x & 128)+(y & 64)+(z & 32))>>5;
	oc1= ((x & 64)+(y & 32)+(z & 16))>>4;
	oc2= ((x & 32)+(y & 16)+(z & 8))>>3;
	oc3= ((x & 16)+(y & 8)+(z & 4))>>2;
	oc4= ((x & 8)+(y & 4)+(z & 2))>>1;
	oc5= ((x & 4)+(y & 2)+(z & 1));

	br= addbranch(oc, br,oc0);
	br= addbranch(oc, br,oc1);
	br= addbranch(oc, br,oc2);
	br= addbranch(oc, br,oc3);
	br= addbranch(oc, br,oc4);
	no= (Node *)br->b[oc5];
	if(no==NULL) br->b[oc5]= (Branch *)(no= addnode(oc));

	while(no->next) no= no->next;

	a= 0;
	if(no->v[7]) {		/* node full */
		no->next= addnode(oc);
		no= no->next;
	}
	else {
		while(no->v[a]!=NULL) a++;
	}
	
	no->v[a]= face;
	no->ob[a]= ob;
	
	if(quad)
		calc_ocval_face(rtf[0], rtf[1], rtf[2], rtf[3], x>>2, y>>1, z, &no->ov[a]);
	else
		calc_ocval_face(rtf[0], rtf[1], rtf[2], NULL, x>>2, y>>1, z, &no->ov[a]);
}

static void d2dda(Octree *oc, short b1, short b2, short c1, short c2, char *ocface, short rts[][3], float rtf[][3])
{
	int ocx1,ocx2,ocy1,ocy2;
	int x,y,dx=0,dy=0;
	float ox1,ox2,oy1,oy2;
	float labda,labdao,labdax,labday,ldx,ldy;

	ocx1= rts[b1][c1];
	ocy1= rts[b1][c2];
	ocx2= rts[b2][c1];
	ocy2= rts[b2][c2];

	if(ocx1==ocx2 && ocy1==ocy2) {
		ocface[oc->ocres*ocx1+ocy1]= 1;
		return;
	}

	ox1= rtf[b1][c1];
	oy1= rtf[b1][c2];
	ox2= rtf[b2][c1];
	oy2= rtf[b2][c2];

	if(ox1!=ox2) {
		if(ox2-ox1>0.0f) {
			labdax= (ox1-ocx1-1.0f)/(ox1-ox2);
			ldx= -1.0f/(ox1-ox2);
			dx= 1;
		} else {
			labdax= (ox1-ocx1)/(ox1-ox2);
			ldx= 1.0f/(ox1-ox2);
			dx= -1;
		}
	} else {
		labdax=1.0f;
		ldx=0;
	}

	if(oy1!=oy2) {
		if(oy2-oy1>0.0f) {
			labday= (oy1-ocy1-1.0f)/(oy1-oy2);
			ldy= -1.0f/(oy1-oy2);
			dy= 1;
		} else {
			labday= (oy1-ocy1)/(oy1-oy2);
			ldy= 1.0f/(oy1-oy2);
			dy= -1;
		}
	} else {
		labday=1.0f;
		ldy=0;
	}
	
	x=ocx1; y=ocy1;
	labda= MIN2(labdax, labday);
	
	while(TRUE) {
		
		if(x<0 || y<0 || x>=oc->ocres || y>=oc->ocres);
		else ocface[oc->ocres*x+y]= 1;
		
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
		if(labda>=1.0f) break;
	}
	ocface[oc->ocres*ocx2+ocy2]=1;
}

static void filltriangle(Octree *oc, short c1, short c2, char *ocface, short *ocmin)
{
	short *ocmax;
	int a, x, y, y1, y2;

	ocmax=ocmin+3;

	for(x=ocmin[c1];x<=ocmax[c1];x++) {
		a= oc->ocres*x;
		for(y=ocmin[c2];y<=ocmax[c2];y++) {
			if(ocface[a+y]) {
				y++;
				while(ocface[a+y] && y!=ocmax[c2]) y++;
				for(y1=ocmax[c2];y1>y;y1--) {
					if(ocface[a+y1]) {
						for(y2=y;y2<=y1;y2++) ocface[a+y2]=1;
						y1=0;
					}
				}
				y=ocmax[c2];
			}
		}
	}
}

void RE_ray_tree_free(RayTree *tree)
{
	Octree *oc= (Octree*)tree;

#if 0
	printf("branches %d nodes %d\n", oc->branchcount, oc->nodecount);
	printf("raycount %d \n", raycount);	
	printf("ray coherent %d \n", coherent_ray);
	printf("accepted %d rejected %d\n", accepted, rejected);
#endif
	if(oc->ocface)
		MEM_freeN(oc->ocface);

	if(oc->adrbranch) {
		int a= 0;
		while(oc->adrbranch[a]) {
			MEM_freeN(oc->adrbranch[a]);
			oc->adrbranch[a]= NULL;
			a++;
		}
		MEM_freeN(oc->adrbranch);
		oc->adrbranch= NULL;
	}
	oc->branchcount= 0;
	
	if(oc->adrnode) {
		int a= 0;
		while(oc->adrnode[a]) {
			MEM_freeN(oc->adrnode[a]);
			oc->adrnode[a]= NULL;
			a++;
		}
		MEM_freeN(oc->adrnode);
		oc->adrnode= NULL;
	}
	oc->nodecount= 0;

	MEM_freeN(oc);
}

RayTree *RE_ray_tree_create(int ocres, int totface, float *min, float *max, RayCoordsFunc coordsfunc, RayCheckFunc checkfunc, RayObjectTransformFunc transformfunc, void *userdata)
{
	Octree *oc;
	float t00, t01, t02;
	int c, ocres2;
	
	oc= MEM_callocN(sizeof(Octree), "Octree");
	oc->adrbranch= MEM_callocN(sizeof(void *)*BRANCH_ARRAY, "octree branches");
	oc->adrnode= MEM_callocN(sizeof(void *)*NODE_ARRAY, "octree nodes");

	oc->coordsfunc= coordsfunc;
	oc->checkfunc= checkfunc;
	oc->transformfunc= transformfunc;
	oc->userdata= userdata;

	/* only for debug info */
	raycount=0;
	accepted= 0;
	rejected= 0;
	coherent_ray= 0;
	
	/* fill main octree struct */
	oc->ocres= ocres;
	ocres2= oc->ocres*oc->ocres;
	
	VECCOPY(oc->min, min);
	VECCOPY(oc->max, max);

	oc->adrbranch[0]=(Branch *)MEM_callocN(4096*sizeof(Branch), "makeoctree");
	
	/* the lookup table, per face, for which nodes to fill in */
	oc->ocface= MEM_callocN( 3*ocres2 + 8, "ocface");
	memset(oc->ocface, 0, 3*ocres2);

	for(c=0;c<3;c++) {	/* octree enlarge, still needed? */
		oc->min[c]-= 0.01f;
		oc->max[c]+= 0.01f;
	}

	t00= oc->max[0]-oc->min[0];
	t01= oc->max[1]-oc->min[1];
	t02= oc->max[2]-oc->min[2];
	
	/* this minus 0.1 is old safety... seems to be needed? */
	oc->ocfacx= (oc->ocres-0.1)/t00;
	oc->ocfacy= (oc->ocres-0.1)/t01;
	oc->ocfacz= (oc->ocres-0.1)/t02;
	
	oc->ocsize= sqrt(t00*t00+t01*t01+t02*t02);	/* global, max size octree */

	return (RayTree*)oc;
}

void RE_ray_tree_add_face(RayTree *tree, int ob, RayFace *face)
{
	Octree *oc = (Octree*)tree;
	float *v1, *v2, *v3, *v4, ocfac[3], rtf[4][3];
	float co1[3], co2[3], co3[3], co4[3];
	short rts[4][3], ocmin[6], *ocmax;
	char *ocface= oc->ocface;	// front, top, size view of face, to fill in
	int a, b, c, oc1, oc2, oc3, oc4, x, y, z, ocres2;

	ocfac[0]= oc->ocfacx;
	ocfac[1]= oc->ocfacy;
	ocfac[2]= oc->ocfacz;

	ocres2= oc->ocres*oc->ocres;

	ocmax= ocmin+3;

	oc->coordsfunc(face, &v1, &v2, &v3, &v4);

	VECCOPY(co1, v1);
	VECCOPY(co2, v2);
	VECCOPY(co3, v3);
	if(v4)
		VECCOPY(co4, v4);

	if(ob >= RE_RAY_TRANSFORM_OFFS) {
		float (*mat)[4]= (float(*)[4])oc->transformfunc(oc->userdata, ob);

		if(mat) {
			Mat4MulVecfl(mat, co1);
			Mat4MulVecfl(mat, co2);
			Mat4MulVecfl(mat, co3);
			if(v4)
				Mat4MulVecfl(mat, co4);
		}
	}
	
	for(c=0;c<3;c++) {
		rtf[0][c]= (co1[c]-oc->min[c])*ocfac[c] ;
		rts[0][c]= (short)rtf[0][c];
		rtf[1][c]= (co2[c]-oc->min[c])*ocfac[c] ;
		rts[1][c]= (short)rtf[1][c];
		rtf[2][c]= (co3[c]-oc->min[c])*ocfac[c] ;
		rts[2][c]= (short)rtf[2][c];
		if(v4) {
			rtf[3][c]= (co4[c]-oc->min[c])*ocfac[c] ;
			rts[3][c]= (short)rtf[3][c];
		}
	}
	
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
		if(ocmax[c]>oc->ocres-1) ocmax[c]=oc->ocres-1;
		if(ocmin[c]<0) ocmin[c]=0;
	}
	
	if(ocmin[0]==ocmax[0] && ocmin[1]==ocmax[1] && ocmin[2]==ocmax[2]) {
		ocwrite(oc, ob, face, (v4 != NULL), ocmin[0], ocmin[1], ocmin[2], rtf);
	}
	else {
		
		d2dda(oc, 0,1,0,1,ocface+ocres2,rts,rtf);
		d2dda(oc, 0,1,0,2,ocface,rts,rtf);
		d2dda(oc, 0,1,1,2,ocface+2*ocres2,rts,rtf);
		d2dda(oc, 1,2,0,1,ocface+ocres2,rts,rtf);
		d2dda(oc, 1,2,0,2,ocface,rts,rtf);
		d2dda(oc, 1,2,1,2,ocface+2*ocres2,rts,rtf);
		if(v4==NULL) {
			d2dda(oc, 2,0,0,1,ocface+ocres2,rts,rtf);
			d2dda(oc, 2,0,0,2,ocface,rts,rtf);
			d2dda(oc, 2,0,1,2,ocface+2*ocres2,rts,rtf);
		}
		else {
			d2dda(oc, 2,3,0,1,ocface+ocres2,rts,rtf);
			d2dda(oc, 2,3,0,2,ocface,rts,rtf);
			d2dda(oc, 2,3,1,2,ocface+2*ocres2,rts,rtf);
			d2dda(oc, 3,0,0,1,ocface+ocres2,rts,rtf);
			d2dda(oc, 3,0,0,2,ocface,rts,rtf);
			d2dda(oc, 3,0,1,2,ocface+2*ocres2,rts,rtf);
		}
		/* nothing todo with triangle..., just fills :) */
		filltriangle(oc, 0,1,ocface+ocres2,ocmin);
		filltriangle(oc, 0,2,ocface,ocmin);
		filltriangle(oc, 1,2,ocface+2*ocres2,ocmin);
		
		/* init static vars here */
		face_in_node(face, 0,0,0, rtf);
		
		for(x=ocmin[0];x<=ocmax[0];x++) {
			a= oc->ocres*x;
			for(y=ocmin[1];y<=ocmax[1];y++) {
				if(ocface[a+y+ocres2]) {
					b= oc->ocres*y+2*ocres2;
					for(z=ocmin[2];z<=ocmax[2];z++) {
						if(ocface[b+z] && ocface[a+z]) {
							if(face_in_node(NULL, x, y, z, rtf))
								ocwrite(oc, ob, face, (v4 != NULL), x,y,z, rtf);
						}
					}
				}
			}
		}
		
		/* same loops to clear octree, doubt it can be done smarter */
		for(x=ocmin[0];x<=ocmax[0];x++) {
			a= oc->ocres*x;
			for(y=ocmin[1];y<=ocmax[1];y++) {
				/* x-y */
				ocface[a+y+ocres2]= 0;

				b= oc->ocres*y + 2*ocres2;
				for(z=ocmin[2];z<=ocmax[2];z++) {
					/* y-z */
					ocface[b+z]= 0;
					/* x-z */
					ocface[a+z]= 0;
				}
			}
		}
	}
}

void RE_ray_tree_done(RayTree *tree)
{
	Octree *oc= (Octree*)tree;

	MEM_freeN(oc->ocface);
	oc->ocface= NULL;
}

/* ************ raytracer **************** */

#define ISECT_EPSILON ((float)FLT_EPSILON)

/* only for self-intersecting test with current render face (where ray left) */
static int intersection2(RayFace *face, int ob, RayObjectTransformFunc transformfunc, RayCoordsFunc coordsfunc, void *userdata, float r0, float r1, float r2, float rx1, float ry1, float rz1)
{
	float *v1, *v2, *v3, *v4, co1[3], co2[3], co3[3], co4[3];
	float x0,x1,x2,t00,t01,t02,t10,t11,t12,t20,t21,t22;
	float m0, m1, m2, divdet, det, det1;
	float u1, v, u2;

	coordsfunc(face, &v1, &v2, &v3, &v4);

	/* happens for baking with non existing face */
	if(v1==NULL)
		return 1;

	if(v4) {
		SWAP(float*, v3, v4);
	}

	VECCOPY(co1, v1);
	VECCOPY(co2, v2);
	VECCOPY(co3, v3);
	if(v4)
		VECCOPY(co4, v4);

	if(ob >= RE_RAY_TRANSFORM_OFFS) {
		float (*mat)[4]= (float(*)[4])transformfunc(userdata, ob);

		if(mat) {
			Mat4MulVecfl(mat, co1);
			Mat4MulVecfl(mat, co2);
			Mat4MulVecfl(mat, co3);
			if(v4)
				Mat4MulVecfl(mat, co4);
		}
	}
	
	t00= co3[0]-co1[0];
	t01= co3[1]-co1[1];
	t02= co3[2]-co1[2];
	t10= co3[0]-co2[0];
	t11= co3[1]-co2[1];
	t12= co3[2]-co2[2];
	
	x0= t11*r2-t12*r1;
	x1= t12*r0-t10*r2;
	x2= t10*r1-t11*r0;

	divdet= t00*x0+t01*x1+t02*x2;

	m0= rx1-co3[0];
	m1= ry1-co3[1];
	m2= rz1-co3[2];
	det1= m0*x0+m1*x1+m2*x2;
	
	if(divdet!=0.0f) {
		u1= det1/divdet;

		if(u1<ISECT_EPSILON) {
			det= t00*(m1*r2-m2*r1);
			det+= t01*(m2*r0-m0*r2);
			det+= t02*(m0*r1-m1*r0);
			v= det/divdet;

			if(v<ISECT_EPSILON && (u1 + v) > -(1.0f+ISECT_EPSILON)) {
				return 1;
			}
		}
	}

	if(v4) {

		t20= co3[0]-co4[0];
		t21= co3[1]-co4[1];
		t22= co3[2]-co4[2];

		divdet= t20*x0+t21*x1+t22*x2;
		if(divdet!=0.0f) {
			u2= det1/divdet;
		
			if(u2<ISECT_EPSILON) {
				det= t20*(m1*r2-m2*r1);
				det+= t21*(m2*r0-m0*r2);
				det+= t22*(m0*r1-m1*r0);
				v= det/divdet;
	
				if(v<ISECT_EPSILON && (u2 + v) >= -(1.0f+ISECT_EPSILON)) {
					return 2;
				}
			}
		}
	}
	return 0;
}

#if 0
/* ray - line intersection */
/* disabled until i got real & fast cylinder checking, this code doesnt work proper
for faster strands */

static int intersection_strand(Isect *is)
{
	float v1[3], v2[3];		/* length of strand */
	float axis[3], rc[3], nor[3], radline, dist, len;
	
	/* radius strand */
	radline= 0.5f*VecLenf(is->vlr->v1->co, is->vlr->v2->co);
	
	VecMidf(v1, is->vlr->v1->co, is->vlr->v2->co);
	VecMidf(v2, is->vlr->v3->co, is->vlr->v4->co);
	
	VECSUB(rc, v1, is->start);	/* vector from base ray to base cylinder */
	VECSUB(axis, v2, v1);		/* cylinder axis */
	
	CROSS(nor, is->vec, axis);
	len= VecLength(nor);
	
	if(len<FLT_EPSILON)
		return 0;

	dist= INPR(rc, nor)/len;	/* distance between ray and axis cylinder */
	
	if(dist<radline && dist>-radline) {
		float dot1, dot2, dot3, rlen, alen, div;
		float labda;
		
		/* calculating the intersection point of shortest distance */
		dot1 = INPR(rc, is->vec);
		dot2 = INPR(is->vec, axis);
		dot3 = INPR(rc, axis);
		rlen = INPR(is->vec, is->vec);
		alen = INPR(axis, axis);
		
		div = alen * rlen - dot2 * dot2;
		if (ABS(div) < FLT_EPSILON)
			return 0;
		
		labda = (dot1*dot2 - dot3*rlen)/div;
		
		radline/= sqrt(alen);
		
		/* labda: where on axis do we have closest intersection? */
		if(labda >= -radline && labda <= 1.0f+radline) {
			VlakRen *vlr= is->faceorig;
			VertRen *v1= is->vlr->v1, *v2= is->vlr->v2, *v3= is->vlr->v3, *v4= is->vlr->v4;
				/* but we dont do shadows from faces sharing edge */
			
			if(v1==vlr->v1 || v2==vlr->v1 || v3==vlr->v1 || v4==vlr->v1) return 0;
			if(v1==vlr->v2 || v2==vlr->v2 || v3==vlr->v2 || v4==vlr->v2) return 0;
			if(v1==vlr->v3 || v2==vlr->v3 || v3==vlr->v3 || v4==vlr->v3) return 0;
			if(vlr->v4) {
				if(v1==vlr->v4 || v2==vlr->v4 || v3==vlr->v4 || v4==vlr->v4) return 0;
			}
			return 1;
		}
	}
	return 0;
}
#endif

/* ray - triangle or quad intersection */
int RE_ray_face_intersection(Isect *is, RayObjectTransformFunc transformfunc, RayCoordsFunc coordsfunc)
{
	RayFace *face= is->face;
	int ob= is->ob;
	float *v1,*v2,*v3,*v4,co1[3],co2[3],co3[3],co4[3];
	float x0,x1,x2,t00,t01,t02,t10,t11,t12,t20,t21,t22,r0,r1,r2;
	float m0, m1, m2, divdet, det1;
	short ok=0;

	/* disabled until i got real & fast cylinder checking, this code doesnt work proper
	   for faster strands */
//	if(is->mode==RE_RAY_SHADOW && is->vlr->flag & R_STRAND) 
//		return intersection_strand(is);
	
	coordsfunc(face, &v1, &v2, &v3, &v4);

	if(v4) {
		SWAP(float*, v3, v4);
	}

	VECCOPY(co1, v1);
	VECCOPY(co2, v2);
	VECCOPY(co3, v3);
	if(v4)
		VECCOPY(co4, v4);

	if(ob) {
		float (*mat)[4]= (float(*)[4])transformfunc(is->userdata, ob);

		if(mat) {
			Mat4MulVecfl(mat, co1);
			Mat4MulVecfl(mat, co2);
			Mat4MulVecfl(mat, co3);
			if(v4)
				Mat4MulVecfl(mat, co4);
		}
	}

	t00= co3[0]-co1[0];
	t01= co3[1]-co1[1];
	t02= co3[2]-co1[2];
	t10= co3[0]-co2[0];
	t11= co3[1]-co2[1];
	t12= co3[2]-co2[2];
	
	r0= is->vec[0];
	r1= is->vec[1];
	r2= is->vec[2];
	
	x0= t12*r1-t11*r2;
	x1= t10*r2-t12*r0;
	x2= t11*r0-t10*r1;

	divdet= t00*x0+t01*x1+t02*x2;

	m0= is->start[0]-co3[0];
	m1= is->start[1]-co3[1];
	m2= is->start[2]-co3[2];
	det1= m0*x0+m1*x1+m2*x2;
	
	if(divdet!=0.0f) {
		float u;

		divdet= 1.0f/divdet;
		u= det1*divdet;
		if(u<ISECT_EPSILON && u>-(1.0f+ISECT_EPSILON)) {
			float v, cros0, cros1, cros2;
			
			cros0= m1*t02-m2*t01;
			cros1= m2*t00-m0*t02;
			cros2= m0*t01-m1*t00;
			v= divdet*(cros0*r0 + cros1*r1 + cros2*r2);

			if(v<ISECT_EPSILON && (u + v) > -(1.0f+ISECT_EPSILON)) {
				float labda;
				labda= divdet*(cros0*t10 + cros1*t11 + cros2*t12);

				if(labda>-ISECT_EPSILON && labda<1.0f+ISECT_EPSILON) {
					is->labda= labda;
					is->u= u; is->v= v;
					ok= 1;
				}
			}
		}
	}

	if(ok==0 && v4) {

		t20= co3[0]-co4[0];
		t21= co3[1]-co4[1];
		t22= co3[2]-co4[2];

		divdet= t20*x0+t21*x1+t22*x2;
		if(divdet!=0.0f) {
			float u;
			divdet= 1.0f/divdet;
			u = det1*divdet;
			
			if(u<ISECT_EPSILON && u>-(1.0f+ISECT_EPSILON)) {
				float v, cros0, cros1, cros2;
				cros0= m1*t22-m2*t21;
				cros1= m2*t20-m0*t22;
				cros2= m0*t21-m1*t20;
				v= divdet*(cros0*r0 + cros1*r1 + cros2*r2);
	
				if(v<ISECT_EPSILON && (u + v) >-(1.0f+ISECT_EPSILON)) {
					float labda;
					labda= divdet*(cros0*t10 + cros1*t11 + cros2*t12);
					
					if(labda>-ISECT_EPSILON && labda<1.0f+ISECT_EPSILON) {
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
		
		if(is->mode!=RE_RAY_SHADOW) {
			/* for mirror & tra-shadow: large faces can be filled in too often, this prevents
			   a face being detected too soon... */
			if(is->labda > is->ddalabda) {
				return 0;
			}
		}
		
		/* when a shadow ray leaves a face, it can be little outside the edges of it, causing
		intersection to be detected in its neighbour face */
		
		if(is->facecontr && is->faceisect);	// optimizing, the tests below are not needed
		else if(is->labda< .1 && is->faceorig) {
			RayFace *face= is->faceorig;
			float *origv1, *origv2, *origv3, *origv4;
			short de= 0;

			coordsfunc(face, &origv1, &origv2, &origv3, &origv4);
			
			if(ob == is->oborig) {
				if(v1==origv1 || v2==origv1 || v3==origv1 || v4==origv1) de++;
				if(v1==origv2 || v2==origv2 || v3==origv2 || v4==origv2) de++;
				if(v1==origv3 || v2==origv3 || v3==origv3 || v4==origv3) de++;
				if(origv4) {
					if(v1==origv4 || v2==origv4 || v3==origv4 || v4==origv4) de++;
				}
			}
			if(de) {
				/* so there's a shared edge or vertex, let's intersect ray with face
				itself, if that's true we can safely return 1, otherwise we assume
				the intersection is invalid, 0 */
				
				if(is->facecontr==NULL) {
					is->obcontr= is->oborig;
					is->facecontr= face;
					is->faceisect= intersection2(face, is->oborig, transformfunc, coordsfunc, is->userdata, -r0, -r1, -r2, is->start[0], is->start[1], is->start[2]);
				}

				if(is->faceisect) return 1;
				return 0;
			}
		}
		
		return 1;
	}

	return 0;
}

/* check all faces in this node */
static int testnode(Octree *oc, Isect *is, Node *no, OcVal ocval, RayCheckFunc checkfunc)
{
	RayFace *face;
	int ob;
	short nr=0;
	OcVal *ov;

	/* return on any first hit */
	if(is->mode==RE_RAY_SHADOW) {
		
		face= no->v[0];
		ob= no->ob[0];
		while(face) {
		
			if(!(is->faceorig == face && is->oborig == ob)) {

				if(checkfunc(is, ob, face)) {
					
					ov= no->ov+nr;
					if( (ov->ocx & ocval.ocx) && (ov->ocy & ocval.ocy) && (ov->ocz & ocval.ocz) ) { 
						//accepted++;
						is->ob= ob;
						is->face= face;
	
						if(RE_ray_face_intersection(is, oc->transformfunc, oc->coordsfunc)) {
							is->ob_last= ob;
							is->face_last= face;
							return 1;
						}
					}
					//else rejected++;
				}
			}
			
			nr++;
			if(nr==8) {
				no= no->next;
				if(no==0) return 0;
				nr=0;
			}
			face= no->v[nr];
			ob= no->ob[nr];
		}
	}
	else {			/* else mirror or glass or shadowtra, return closest face  */
		Isect isect;
		int found= 0;
		
		is->labda= 1.0f;	/* needed? */
		isect= *is;		/* copy for sorting */
		
		face= no->v[0];
		ob= no->ob[0];
		while(face) {

			if(!(is->faceorig == face && is->oborig == ob)) {
				if(checkfunc(is, ob, face)) {
					ov= no->ov+nr;
					if( (ov->ocx & ocval.ocx) && (ov->ocy & ocval.ocy) && (ov->ocz & ocval.ocz) ) { 
						//accepted++;

						isect.ob= ob;
						isect.face= face;
						if(RE_ray_face_intersection(&isect, oc->transformfunc, oc->coordsfunc)) {
							if(isect.labda<is->labda) {
								*is= isect;
								found= 1;
							}
							
						}
					}
					//else rejected++;
				}
			}
			
			nr++;
			if(nr==8) {
				no= no->next;
				if(no==NULL) break;
				nr=0;
			}
			face= no->v[nr];
			ob= no->ob[nr];
		}
		
		return found;
	}

	return 0;
}

/* find the Node for the octree coord x y z */
static Node *ocread(Octree *oc, int x, int y, int z)
{
	Branch *br;
	int oc1;
	
	x<<=2;
	y<<=1;
	
	br= oc->adrbranch[0];
	
	if(oc->ocres==512) {
		oc1= ((x & 1024)+(y & 512)+(z & 256))>>8;
		br= br->b[oc1];
		if(br==NULL) {
			return NULL;
		}
	}
	if(oc->ocres>=256) {
		oc1= ((x & 512)+(y & 256)+(z & 128))>>7;
		br= br->b[oc1];
		if(br==NULL) {
			return NULL;
		}
	}
	if(oc->ocres>=128) {
		oc1= ((x & 256)+(y & 128)+(z & 64))>>6;
		br= br->b[oc1];
		if(br==NULL) {
			return NULL;
		}
	}
	
	oc1= ((x & 128)+(y & 64)+(z & 32))>>5;
	br= br->b[oc1];
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
						oc1= ((x & 4)+(y & 2)+(z & 1));
						return (Node *)br->b[oc1];
					}
				}
			}
		}
	}
	
	return NULL;
}

static int cliptest(float p, float q, float *u1, float *u2)
{
	float r;

	if(p<0.0f) {
		if(q<p) return 0;
		else if(q<0.0f) {
			r= q/p;
			if(r>*u2) return 0;
			else if(r>*u1) *u1=r;
		}
	}
	else {
		if(p>0.0f) {
			if(q<0.0f) return 0;
			else if(q<p) {
				r= q/p;
				if(r<*u1) return 0;
				else if(r<*u2) *u2=r;
			}
		}
		else if(q<0.0f) return 0;
	}
	return 1;
}

/* extensive coherence checks/storage cancels out the benefit of it, and gives errors... we
   need better methods, sample code commented out below (ton) */
 
/*

in top: static int coh_nodes[16*16*16][6];
in makeoctree: memset(coh_nodes, 0, sizeof(coh_nodes));
 
static void add_coherence_test(int ocx1, int ocx2, int ocy1, int ocy2, int ocz1, int ocz2)
{
	short *sp;
	
	sp= coh_nodes[ (ocx2 & 15) + 16*(ocy2 & 15) + 256*(ocz2 & 15) ];
	sp[0]= ocx1; sp[1]= ocy1; sp[2]= ocz1;
	sp[3]= ocx2; sp[4]= ocy2; sp[5]= ocz2;
	
}

static int do_coherence_test(int ocx1, int ocx2, int ocy1, int ocy2, int ocz1, int ocz2)
{
	short *sp;
	
	sp= coh_nodes[ (ocx2 & 15) + 16*(ocy2 & 15) + 256*(ocz2 & 15) ];
	if(sp[0]==ocx1 && sp[1]==ocy1 && sp[2]==ocz1 &&
	   sp[3]==ocx2 && sp[4]==ocy2 && sp[5]==ocz2) return 1;
	return 0;
}

*/

int RE_ray_tree_intersect(RayTree *tree, Isect *is)
{
	Octree *oc= (Octree*)tree;

	return RE_ray_tree_intersect_check(tree, is, oc->checkfunc);
}

/* return 1: found valid intersection */
/* starts with is->faceorig */
int RE_ray_tree_intersect_check(RayTree *tree, Isect *is, RayCheckFunc checkfunc)
{
	Octree *oc= (Octree*)tree;
	Node *no;
	OcVal ocval;
	float vec1[3], vec2[3];
	float u1,u2,ox1,ox2,oy1,oy2,oz1,oz2;
	float labdao,labdax,ldx,labday,ldy,labdaz,ldz, ddalabda;
	int dx,dy,dz;	
	int xo,yo,zo,c1=0;
	int ocx1,ocx2,ocy1, ocy2,ocz1,ocz2;
	
	/* clip with octree */
	if(oc->branchcount==0) return 0;
	
	/* do this before intersect calls */
	is->facecontr= NULL;				/* to check shared edge */
	is->obcontr= 0;
	is->faceisect= is->isect= 0;		/* shared edge, quad half flag */
	is->userdata= oc->userdata;

	/* only for shadow! */
	if(is->mode==RE_RAY_SHADOW) {
	
		/* check with last intersected shadow face */
		if(is->face_last!=NULL && !(is->face_last==is->faceorig && is->ob_last==is->oborig)) {
			if(checkfunc(is, is->ob_last, is->face_last)) {
				is->ob= is->ob_last;
				is->face= is->face_last;
				VECSUB(is->vec, is->end, is->start);
				if(RE_ray_face_intersection(is, oc->transformfunc, oc->coordsfunc)) return 1;
			}
		}
	}
	
	ldx= is->end[0] - is->start[0];
	u1= 0.0f;
	u2= 1.0f;

	/* clip with octree cube */
	if(cliptest(-ldx, is->start[0]-oc->min[0], &u1,&u2)) {
		if(cliptest(ldx, oc->max[0]-is->start[0], &u1,&u2)) {
			ldy= is->end[1] - is->start[1];
			if(cliptest(-ldy, is->start[1]-oc->min[1], &u1,&u2)) {
				if(cliptest(ldy, oc->max[1]-is->start[1], &u1,&u2)) {
					ldz= is->end[2] - is->start[2];
					if(cliptest(-ldz, is->start[2]-oc->min[2], &u1,&u2)) {
						if(cliptest(ldz, oc->max[2]-is->start[2], &u1,&u2)) {
							c1=1;
							if(u2<1.0f) {
								is->end[0]= is->start[0]+u2*ldx;
								is->end[1]= is->start[1]+u2*ldy;
								is->end[2]= is->start[2]+u2*ldz;
							}
							if(u1>0.0f) {
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
	//ocread(oc, oc->ocres, 0, 0);

	/* setup 3dda to traverse octree */
	ox1= (is->start[0]-oc->min[0])*oc->ocfacx;
	oy1= (is->start[1]-oc->min[1])*oc->ocfacy;
	oz1= (is->start[2]-oc->min[2])*oc->ocfacz;
	ox2= (is->end[0]-oc->min[0])*oc->ocfacx;
	oy2= (is->end[1]-oc->min[1])*oc->ocfacy;
	oz2= (is->end[2]-oc->min[2])*oc->ocfacz;

	ocx1= (int)ox1;
	ocy1= (int)oy1;
	ocz1= (int)oz1;
	ocx2= (int)ox2;
	ocy2= (int)oy2;
	ocz2= (int)oz2;

	/* for intersection */
	VECSUB(is->vec, is->end, is->start);

	if(ocx1==ocx2 && ocy1==ocy2 && ocz1==ocz2) {
		no= ocread(oc, ocx1, ocy1, ocz1);
		if(no) {
			/* exact intersection with node */
			vec1[0]= ox1; vec1[1]= oy1; vec1[2]= oz1;
			vec2[0]= ox2; vec2[1]= oy2; vec2[2]= oz2;
			calc_ocval_ray(&ocval, (float)ocx1, (float)ocy1, (float)ocz1, vec1, vec2);
			is->ddalabda= 1.0f;
			if( testnode(oc, is, no, ocval, checkfunc) ) return 1;
		}
	}
	else {
		//static int coh_ocx1,coh_ocx2,coh_ocy1, coh_ocy2,coh_ocz1,coh_ocz2;
		float dox, doy, doz;
		int eqval;
		
		/* calc labda en ld */
		dox= ox1-ox2;
		doy= oy1-oy2;
		doz= oz1-oz2;

		if(dox<-FLT_EPSILON) {
			ldx= -1.0f/dox;
			labdax= (ocx1-ox1+1.0f)*ldx;
			dx= 1;
		} else if(dox>FLT_EPSILON) {
			ldx= 1.0f/dox;
			labdax= (ox1-ocx1)*ldx;
			dx= -1;
		} else {
			labdax=1.0f;
			ldx=0;
			dx= 0;
		}

		if(doy<-FLT_EPSILON) {
			ldy= -1.0f/doy;
			labday= (ocy1-oy1+1.0f)*ldy;
			dy= 1;
		} else if(doy>FLT_EPSILON) {
			ldy= 1.0f/doy;
			labday= (oy1-ocy1)*ldy;
			dy= -1;
		} else {
			labday=1.0f;
			ldy=0;
			dy= 0;
		}

		if(doz<-FLT_EPSILON) {
			ldz= -1.0f/doz;
			labdaz= (ocz1-oz1+1.0f)*ldz;
			dz= 1;
		} else if(doz>FLT_EPSILON) {
			ldz= 1.0f/doz;
			labdaz= (oz1-ocz1)*ldz;
			dz= -1;
		} else {
			labdaz=1.0f;
			ldz=0;
			dz= 0;
		}
		
		xo=ocx1; yo=ocy1; zo=ocz1;
		labdao= ddalabda= MIN3(labdax,labday,labdaz);
		
		vec2[0]= ox1;
		vec2[1]= oy1;
		vec2[2]= oz1;
		
		/* this loop has been constructed to make sure the first and last node of ray
		   are always included, even when ddalabda==1.0f or larger */

		while(TRUE) {

			no= ocread(oc, xo, yo, zo);
			if(no) {
				
				/* calculate ray intersection with octree node */
				VECCOPY(vec1, vec2);
				// dox,y,z is negative
				vec2[0]= ox1-ddalabda*dox;
				vec2[1]= oy1-ddalabda*doy;
				vec2[2]= oz1-ddalabda*doz;
				calc_ocval_ray(&ocval, (float)xo, (float)yo, (float)zo, vec1, vec2);
							   
				is->ddalabda= ddalabda;
				if( testnode(oc, is, no, ocval, checkfunc) ) return 1;
			}

			labdao= ddalabda;
			
			/* traversing ocree nodes need careful detection of smallest values, with proper
			   exceptions for equal labdas */
			eqval= (labdax==labday);
			if(labday==labdaz) eqval += 2;
			if(labdax==labdaz) eqval += 4;
			
			if(eqval) {	// only 4 cases exist!
				if(eqval==7) {	// x=y=z
					xo+=dx; labdax+=ldx;
					yo+=dy; labday+=ldy;
					zo+=dz; labdaz+=ldz;
				}
				else if(eqval==1) { // x=y 
					if(labday < labdaz) {
						xo+=dx; labdax+=ldx;
						yo+=dy; labday+=ldy;
					}
					else {
						zo+=dz; labdaz+=ldz;
					}
				}
				else if(eqval==2) { // y=z
					if(labdax < labday) {
						xo+=dx; labdax+=ldx;
					}
					else {
						yo+=dy; labday+=ldy;
						zo+=dz; labdaz+=ldz;
					}
				}
				else { // x=z
					if(labday < labdax) {
						yo+=dy; labday+=ldy;
					}
					else {
						xo+=dx; labdax+=ldx;
						zo+=dz; labdaz+=ldz;
					}
				}
			}
			else {	// all three different, just three cases exist
				eqval= (labdax<labday);
				if(labday<labdaz) eqval += 2;
				if(labdax<labdaz) eqval += 4;
				
				if(eqval==7 || eqval==5) { // x smallest
					xo+=dx; labdax+=ldx;
				}
				else if(eqval==2 || eqval==6) { // y smallest
					yo+=dy; labday+=ldy;
				}
				else { // z smallest
					zo+=dz; labdaz+=ldz;
				}
				
			}

			ddalabda=MIN3(labdax,labday,labdaz);
			if(ddalabda==labdao) break;
			/* to make sure the last node is always checked */
			if(labdao>=1.0f) break;
		}
	}
	
	/* reached end, no intersections found */
	is->ob_last= 0;
	is->face_last= NULL;
	return 0;
}	

float RE_ray_tree_max_size(RayTree *tree)
{
	return ((Octree*)tree)->ocsize;
}

