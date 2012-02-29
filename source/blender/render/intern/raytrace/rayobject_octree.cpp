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
 * The Original Code is Copyright (C) 1990-1998 NeoGeo BV.
 * All rights reserved.
 *
 * Contributors: 2004/2005 Blender Foundation, full recode
 *
 * ***** END GPL LICENSE BLOCK *****
 */

/** \file blender/render/intern/raytrace/rayobject_octree.cpp
 *  \ingroup render
 */


/* IMPORTANT NOTE: this code must be independent of any other render code
   to use it outside the renderer! */

#include <math.h>
#include <string.h>
#include <stdlib.h>
#include <float.h>
#include <assert.h>

#include "MEM_guardedalloc.h"

#include "DNA_material_types.h"

#include "BLI_math.h"
#include "BLI_utildefines.h"

#include "rayintersection.h"
#include "rayobject.h"

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
	struct OcVal ov[8];
	struct Node *next;
} Node;

typedef struct Octree {
	RayObject rayobj;

	struct Branch **adrbranch;
	struct Node **adrnode;
	float ocsize;	/* ocsize: mult factor,  max size octree */
	float ocfacx,ocfacy,ocfacz;
	float min[3], max[3];
	int ocres;
	int branchcount, nodecount;

	/* during building only */
	char *ocface; 
	
	RayFace **ro_nodes;
	int ro_nodes_size, ro_nodes_used;
	
} Octree;

static int  RE_rayobject_octree_intersect(RayObject *o, Isect *isec);
static void RE_rayobject_octree_add(RayObject *o, RayObject *ob);
static void RE_rayobject_octree_done(RayObject *o);
static void RE_rayobject_octree_free(RayObject *o);
static void RE_rayobject_octree_bb(RayObject *o, float *min, float *max);

/*
 * This function is not expected to be called by current code state.
 */
static float RE_rayobject_octree_cost(RayObject *UNUSED(o))
{
	return 1.0;
}

static void RE_rayobject_octree_hint_bb(RayObject *UNUSED(o), RayHint *UNUSED(hint),
                                        float *UNUSED(min), float *UNUSED(max))
{
	return;
}

static RayObjectAPI octree_api =
{
	RE_rayobject_octree_intersect,
	RE_rayobject_octree_add,
	RE_rayobject_octree_done,
	RE_rayobject_octree_free,
	RE_rayobject_octree_bb,
	RE_rayobject_octree_cost,
	RE_rayobject_octree_hint_bb
};

/* **************** ocval method ******************* */
/* within one octree node, a set of 3x15 bits defines a 'boundbox' to OR with */

#define OCVALRES	15
#define BROW16(min, max)      (((max)>=OCVALRES? 0xFFFF: (1<<(max+1))-1) - ((min>0)? ((1<<(min))-1):0) )

static void calc_ocval_face(float *v1, float *v2, float *v3, float *v4, short x, short y, short z, OcVal *ov)
{
	float min[3], max[3];
	int ocmin, ocmax;
	
	copy_v3_v3(min, v1);
	copy_v3_v3(max, v1);
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
		oc->adrbranch[index]= (Branch*)MEM_callocN(4096*sizeof(Branch), "new oc branch");

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
		oc->adrnode[index]= (Node*)MEM_callocN(4096*sizeof(Node),"addnode");

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
		normal_tri_v3( nor,rtf[0], rtf[1], rtf[2]);
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

static void ocwrite(Octree *oc, RayFace *face, int quad, short x, short y, short z, float rtf[4][3])
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
	
	no->v[a]= (RayFace*) RE_rayobject_align(face);
	
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

static void filltriangle(Octree *oc, short c1, short c2, char *ocface, short *ocmin, short *ocmax)
{
	int a, x, y, y1, y2;

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

static void RE_rayobject_octree_free(RayObject *tree)
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


RayObject *RE_rayobject_octree_create(int ocres, int size)
{
	Octree *oc= (Octree*)MEM_callocN(sizeof(Octree), "Octree");
	assert( RE_rayobject_isAligned(oc) ); /* RayObject API assumes real data to be 4-byte aligned */	
	
	oc->rayobj.api = &octree_api;
	
	oc->ocres = ocres;
	
	oc->ro_nodes = (RayFace**)MEM_callocN(sizeof(RayFace*)*size, "octree rayobject nodes");
	oc->ro_nodes_size = size;
	oc->ro_nodes_used = 0;

	
	return RE_rayobject_unalignRayAPI((RayObject*) oc);
}


static void RE_rayobject_octree_add(RayObject *tree, RayObject *node)
{
	Octree *oc = (Octree*)tree;

	assert( RE_rayobject_isRayFace(node) );
	assert( oc->ro_nodes_used < oc->ro_nodes_size );
	oc->ro_nodes[ oc->ro_nodes_used++ ] = (RayFace*)RE_rayobject_align(node);
}

static void octree_fill_rayface(Octree *oc, RayFace *face)
{
	float ocfac[3], rtf[4][3];
	float co1[3], co2[3], co3[3], co4[3];
	short rts[4][3];
	short ocmin[3], ocmax[3];
	char *ocface= oc->ocface;	// front, top, size view of face, to fill in
	int a, b, c, oc1, oc2, oc3, oc4, x, y, z, ocres2;

	ocfac[0]= oc->ocfacx;
	ocfac[1]= oc->ocfacy;
	ocfac[2]= oc->ocfacz;

	ocres2= oc->ocres*oc->ocres;

	copy_v3_v3(co1, face->v1);
	copy_v3_v3(co2, face->v2);
	copy_v3_v3(co3, face->v3);
	if(face->v4)
		copy_v3_v3(co4, face->v4);

	for(c=0;c<3;c++) {
		rtf[0][c] = (co1[c] - oc->min[c]) * ocfac[c];
		rts[0][c] = (short)rtf[0][c];
		rtf[1][c] = (co2[c] - oc->min[c]) * ocfac[c];
		rts[1][c] = (short)rtf[1][c];
		rtf[2][c] = (co3[c] - oc->min[c]) * ocfac[c];
		rts[2][c] = (short)rtf[2][c];
		if(RE_rayface_isQuad(face)) {
			rtf[3][c] = (co4[c] - oc->min[c]) * ocfac[c];
			rts[3][c] = (short)rtf[3][c];
		}
	}
	
	for(c=0;c<3;c++) {
		oc1= rts[0][c];
		oc2= rts[1][c];
		oc3= rts[2][c];
		if(!RE_rayface_isQuad(face)) {
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
		ocwrite(oc, face, RE_rayface_isQuad(face), ocmin[0], ocmin[1], ocmin[2], rtf);
	}
	else {
		
		d2dda(oc, 0,1,0,1,ocface+ocres2,rts,rtf);
		d2dda(oc, 0,1,0,2,ocface,rts,rtf);
		d2dda(oc, 0,1,1,2,ocface+2*ocres2,rts,rtf);
		d2dda(oc, 1,2,0,1,ocface+ocres2,rts,rtf);
		d2dda(oc, 1,2,0,2,ocface,rts,rtf);
		d2dda(oc, 1,2,1,2,ocface+2*ocres2,rts,rtf);
		if(!RE_rayface_isQuad(face)) {
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
		filltriangle(oc, 0,1,ocface+ocres2,ocmin,ocmax);
		filltriangle(oc, 0,2,ocface,ocmin,ocmax);
		filltriangle(oc, 1,2,ocface+2*ocres2,ocmin,ocmax);
		
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
								ocwrite(oc, face, RE_rayface_isQuad(face), x,y,z, rtf);
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

static void RE_rayobject_octree_done(RayObject *tree)
{
	Octree *oc = (Octree*)tree;
	int c;
	float t00, t01, t02;
	int ocres2 = oc->ocres*oc->ocres;
	
	INIT_MINMAX(oc->min, oc->max);
	
	/* Calculate Bounding Box */
	for(c=0; c<oc->ro_nodes_used; c++)
		RE_rayobject_merge_bb( RE_rayobject_unalignRayFace(oc->ro_nodes[c]), oc->min, oc->max);
		
	/* Alloc memory */
	oc->adrbranch= (Branch**)MEM_callocN(sizeof(void *)*BRANCH_ARRAY, "octree branches");
	oc->adrnode= (Node**)MEM_callocN(sizeof(void *)*NODE_ARRAY, "octree nodes");
	
	oc->adrbranch[0]=(Branch *)MEM_callocN(4096*sizeof(Branch), "makeoctree");
	
	/* the lookup table, per face, for which nodes to fill in */
	oc->ocface= (char*)MEM_callocN( 3*ocres2 + 8, "ocface");
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

	for(c=0; c<oc->ro_nodes_used; c++)
	{
		octree_fill_rayface(oc, oc->ro_nodes[c]);
	}

	MEM_freeN(oc->ocface);
	oc->ocface = NULL;
	MEM_freeN(oc->ro_nodes);
	oc->ro_nodes = NULL;
	
	printf("%f %f - %f\n", oc->min[0], oc->max[0], oc->ocfacx );
	printf("%f %f - %f\n", oc->min[1], oc->max[1], oc->ocfacy );
	printf("%f %f - %f\n", oc->min[2], oc->max[2], oc->ocfacz );
}

static void RE_rayobject_octree_bb(RayObject *tree, float *min, float *max)
{
	Octree *oc = (Octree*)tree;
	DO_MINMAX(oc->min, min, max);
	DO_MINMAX(oc->max, min, max);
}

/* check all faces in this node */
static int testnode(Octree *UNUSED(oc), Isect *is, Node *no, OcVal ocval)
{
	short nr=0;

	/* return on any first hit */
	if(is->mode==RE_RAY_SHADOW) {
	
		for(; no; no = no->next)
		for(nr=0; nr<8; nr++)
		{
			RayFace *face = no->v[nr];
			OcVal 		*ov = no->ov+nr;
			
			if(!face) break;
			
			if( (ov->ocx & ocval.ocx) && (ov->ocy & ocval.ocy) && (ov->ocz & ocval.ocz) )
			{
				if( RE_rayobject_intersect( RE_rayobject_unalignRayFace(face),is) )
					return 1;
			}
		}
	}
	else
	{			/* else mirror or glass or shadowtra, return closest face  */
		int found= 0;
		
		for(; no; no = no->next)
		for(nr=0; nr<8; nr++)
		{
			RayFace *face = no->v[nr];
			OcVal 		*ov = no->ov+nr;
			
			if(!face) break;
			
			if( (ov->ocx & ocval.ocx) && (ov->ocy & ocval.ocy) && (ov->ocz & ocval.ocz) )
			{ 
				if( RE_rayobject_intersect( RE_rayobject_unalignRayFace(face),is) )
					found= 1;
			}
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

/* return 1: found valid intersection */
/* starts with is->orig.face */
static int RE_rayobject_octree_intersect(RayObject *tree, Isect *is)
{
	Octree *oc= (Octree*)tree;
	Node *no;
	OcVal ocval;
	float vec1[3], vec2[3], start[3], end[3];
	float u1,u2,ox1,ox2,oy1,oy2,oz1,oz2;
	float labdao,labdax,ldx,labday,ldy,labdaz,ldz, ddalabda;
	float olabda = 0;
	int dx,dy,dz;	
	int xo,yo,zo,c1=0;
	int ocx1,ocx2,ocy1, ocy2,ocz1,ocz2;
	
	/* clip with octree */
	if(oc->branchcount==0) return 0;
	
	/* do this before intersect calls */
#if 0
	is->facecontr= NULL;				/* to check shared edge */
	is->obcontr= 0;
	is->faceisect= is->isect= 0;		/* shared edge, quad half flag */
	is->userdata= oc->userdata;
#endif

	copy_v3_v3( start, is->start );
	madd_v3_v3v3fl( end, is->start, is->dir, is->dist );
	ldx= is->dir[0]*is->dist;
	olabda = is->dist;
	u1= 0.0f;
	u2= 1.0f;
	
	/* clip with octree cube */
	if(cliptest(-ldx, start[0]-oc->min[0], &u1,&u2)) {
		if(cliptest(ldx, oc->max[0]-start[0], &u1,&u2)) {
			ldy= is->dir[1]*is->dist;
			if(cliptest(-ldy, start[1]-oc->min[1], &u1,&u2)) {
				if(cliptest(ldy, oc->max[1]-start[1], &u1,&u2)) {
					ldz = is->dir[2]*is->dist;
					if(cliptest(-ldz, start[2]-oc->min[2], &u1,&u2)) {
						if(cliptest(ldz, oc->max[2]-start[2], &u1,&u2)) {
							c1=1;
							if(u2<1.0f) {
								end[0] = start[0]+u2*ldx;
								end[1] = start[1]+u2*ldy;
								end[2] = start[2]+u2*ldz;
							}

							if(u1>0.0f) {
								start[0] += u1*ldx;
								start[1] += u1*ldy;
								start[2] += u1*ldz;
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
	ox1= (start[0]-oc->min[0])*oc->ocfacx;
	oy1= (start[1]-oc->min[1])*oc->ocfacy;
	oz1= (start[2]-oc->min[2])*oc->ocfacz;
	ox2= (end[0]-oc->min[0])*oc->ocfacx;
	oy2= (end[1]-oc->min[1])*oc->ocfacy;
	oz2= (end[2]-oc->min[2])*oc->ocfacz;

	ocx1= (int)ox1;
	ocy1= (int)oy1;
	ocz1= (int)oz1;
	ocx2= (int)ox2;
	ocy2= (int)oy2;
	ocz2= (int)oz2;
	
	if(ocx1==ocx2 && ocy1==ocy2 && ocz1==ocz2) {
		no= ocread(oc, ocx1, ocy1, ocz1);
		if(no) {
			/* exact intersection with node */
			vec1[0]= ox1; vec1[1]= oy1; vec1[2]= oz1;
			vec2[0]= ox2; vec2[1]= oy2; vec2[2]= oz2;
			calc_ocval_ray(&ocval, (float)ocx1, (float)ocy1, (float)ocz1, vec1, vec2);
			if( testnode(oc, is, no, ocval) ) return 1;
		}
	}
	else {
		int found = 0;
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
		ddalabda= MIN3(labdax,labday,labdaz);
		
		vec2[0]= ox1;
		vec2[1]= oy1;
		vec2[2]= oz1;
		
		/* this loop has been constructed to make sure the first and last node of ray
		   are always included, even when ddalabda==1.0f or larger */

		while(TRUE) {

			no= ocread(oc, xo, yo, zo);
			if(no) {
				
				/* calculate ray intersection with octree node */
				copy_v3_v3(vec1, vec2);
				// dox,y,z is negative
				vec2[0]= ox1-ddalabda*dox;
				vec2[1]= oy1-ddalabda*doy;
				vec2[2]= oz1-ddalabda*doz;
				calc_ocval_ray(&ocval, (float)xo, (float)yo, (float)zo, vec1, vec2);

				//is->dist = (u1+ddalabda*(u2-u1))*olabda;
				if( testnode(oc, is, no, ocval) )
					found = 1;

				if(is->dist < (u1+ddalabda*(u2-u1))*olabda)
					return found;
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
	return 0;
}	



