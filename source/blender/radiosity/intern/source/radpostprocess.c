/* ***************************************
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



    radpostprocess.c	nov/dec 1992
						may 1999

    - faces
    - filtering and node-limit
    - apply to meshes
    $Id$

 *************************************** */

#include <stdlib.h>
#include <math.h>

#ifdef HAVE_CONFIG_H
#include <config.h>
#endif

#ifdef WIN32
#include "BLI_winstuff.h"
#endif

#include "MEM_guardedalloc.h"

#include "BLI_blenlib.h"
#include "BLI_arithb.h"
#include "BIF_toolbox.h"  // notice()

#include "DNA_material_types.h"
#include "DNA_mesh_types.h"
#include "DNA_meshdata_types.h"
#include "DNA_object_types.h"
#include "DNA_radio_types.h"

#include "BKE_utildefines.h"
#include "BKE_global.h"
#include "BKE_main.h"
#include "BKE_mesh.h"
#include "BKE_object.h"
#include "BKE_material.h"

#include "BIF_screen.h"		/* waitcursor */
#include "BIF_editview.h"	/* deselectall */

#include "BDR_editobject.h"	/* delete_obj */

#include "radio.h"

/* locals? not. done in radio.h...  */
/*  void rad_addmesh(void); */
/*  void rad_replacemesh(void); */

void addaccu(register char *z, register char *t)
{
	register int div, mul;

	mul= *t;
	div= mul+1;
	(*t)++;

	t[1]= (mul*t[1]+z[1])/div;
	t[2]= (mul*t[2]+z[2])/div;
	t[3]= (mul*t[3]+z[3])/div;

}

void addaccuweight(register char *z, register char *t, int w)
{
	register int div, mul;
	
	if(w==0) w= 1;
	
	mul= *t;
	div= mul+w;
	if(div>255) return;
	(*t)= div;

	t[1]= (mul*t[1]+w*z[1])/div;
	t[2]= (mul*t[2]+w*z[2])/div;
	t[3]= (mul*t[3]+w*z[3])/div;

}

void triaweight(Face *face, int *w1, int *w2, int *w3)
{
	float n1[3], n2[3], n3[3], temp;

	n1[0]= face->v2[0]-face->v1[0];
	n1[1]= face->v2[1]-face->v1[1];
	n1[2]= face->v2[2]-face->v1[2];
	n2[0]= face->v3[0]-face->v2[0];
	n2[1]= face->v3[1]-face->v2[1];
	n2[2]= face->v3[2]-face->v2[2];
	n3[0]= face->v1[0]-face->v3[0];
	n3[1]= face->v1[1]-face->v3[1];
	n3[2]= face->v1[2]-face->v3[2];
	Normalise(n1);
	Normalise(n2);
	Normalise(n3);
	temp= 32.0/(PI);
	*w1= 0.5+temp*acos(-n1[0]*n3[0]-n1[1]*n3[1]-n1[2]*n3[2]);
	*w2= 0.5+temp*acos(-n1[0]*n2[0]-n1[1]*n2[1]-n1[2]*n2[2]);
	*w3= 0.5+temp*acos(-n2[0]*n3[0]-n2[1]*n3[1]-n2[2]*n3[2]);
	
}



void init_face_tab()
{
	int a= 0;

	if(RG.facebase==0) {
		RG.facebase= MEM_callocN(sizeof(void *)*RAD_MAXFACETAB, "init_face_tab");
	}
	for(a=0; a<RAD_MAXFACETAB; a++) {
		if(RG.facebase[a]==0) break;
		MEM_freeN(RG.facebase[a]);
		RG.facebase[a]= 0;
	}
	RG.totface= 0;
}

Face *addface()
{
	Face *face;
	int a;

	if(RG.totface<0 || RG.totface>RAD_MAXFACETAB*1024 ) {
		printf("error in addface: %d\n", RG.totface);
		return 0;
	}
	a= RG.totface>>10;
	face= RG.facebase[a];
	if(face==0) {
		face= MEM_callocN(1024*sizeof(Face),"addface");
		RG.facebase[a]= face;
	}
	face+= (RG.totface & 1023);
	
	RG.totface++;
	
	return face;

}

void makeface(float *v1, float *v2, float *v3, float *v4, RNode *rn)
{
	Face *face;
	
	face= addface();
	face->v1= v1;
	face->v2= v2;
	face->v3= v3;
	face->v4= v4;
	face->col= rn->col;
	face->matindex= rn->par->matindex;
}


void anchorQuadface(RNode *rn, float *v1, float *v2, float *v3, float *v4, int flag)
{

	switch(flag) {
		case 1:
			makeface(rn->v1, v1, rn->v4, 0, rn);
			makeface(v1, rn->v3, rn->v4, 0, rn);
			makeface(v1, rn->v2, rn->v3, 0, rn);
			break;
		case 2:
			makeface(rn->v2, v2, rn->v1, 0, rn);
			makeface(v2, rn->v4, rn->v1, 0, rn);
			makeface(v2, rn->v3, rn->v4, 0, rn);
			break;
		case 4:
			makeface(rn->v3, v3, rn->v2, 0, rn);
			makeface(v3, rn->v1, rn->v2, 0, rn);
			makeface(v3, rn->v4, rn->v1, 0, rn);
			break;
		case 8:
			makeface(rn->v4, v4, rn->v3, 0, rn);
			makeface(v4, rn->v2, rn->v3, 0, rn);
			makeface(v4, rn->v1, rn->v2, 0, rn);
			break;
			
		case 3:
			makeface(rn->v1, v1, rn->v4, 0, rn);
			makeface(v1, v2, rn->v4, 0, rn);
			makeface(v1, rn->v2, v2, 0, rn);
			makeface(v2, rn->v3, rn->v4, 0, rn);
			break;
		case 6:
			makeface(rn->v2, v2, rn->v1, 0, rn);
			makeface(v2, v3, rn->v1, 0, rn);
			makeface(v2, rn->v3, v3, 0, rn);
			makeface(v3, rn->v4, rn->v1, 0, rn);
			break;
		case 12:
			makeface(rn->v3, v3, rn->v2, 0, rn);
			makeface(v3, v4, rn->v2, 0, rn);
			makeface(v3, rn->v4, v4, 0, rn);
			makeface(v4, rn->v1, rn->v2, 0, rn);
			break;
		case 9:
			makeface(rn->v4, v4, rn->v3, 0, rn);
			makeface(v4, v1, rn->v3, 0, rn);
			makeface(v4, rn->v1, v1, 0, rn);
			makeface(v1, rn->v2, rn->v3, 0, rn);
			break;
			
		case 5:
			makeface(rn->v1, v1, v3, rn->v4, rn);
			makeface(v1, rn->v2, rn->v3, v3, rn);
			break;
		case 10:
			makeface(rn->v2, v2, v4, rn->v1, rn);
			makeface(v2, rn->v3, rn->v4, v4, rn);
			break;
			
		case 7:
			makeface(rn->v1, v1, v3, rn->v4, rn);
			makeface(v1, v2, v3, 0, rn);
			makeface(v1, rn->v2, v2, 0, rn);
			makeface(v2, rn->v3, v3, 0, rn);
			break;
		case 14:
			makeface(rn->v2, v2, v4, rn->v1, rn);
			makeface(v2, v3, v4, 0, rn);
			makeface(v2, rn->v3, v3, 0, rn);
			makeface(v3, rn->v4, v4, 0, rn);
			break;
		case 13:
			makeface(rn->v3, v3, v1, rn->v2, rn);
			makeface(v3, v4, v1, 0, rn);
			makeface(v3, rn->v4, v4, 0, rn);
			makeface(v4, rn->v1, v1, 0, rn);
			break;
		case 11:
			makeface(rn->v4, v4, v2, rn->v3, rn);
			makeface(v4, v1, v2, 0, rn);
			makeface(v4, rn->v1, v1, 0, rn);
			makeface(v1, rn->v2, v2, 0, rn);
			break;
		
		case 15:
			makeface(v1, v2, v3, v4, rn);
			makeface(v1, rn->v2, v2, 0, rn);
			makeface(v2, rn->v3, v3, 0, rn);
			makeface(v3, rn->v4, v4, 0, rn);
			makeface(v4, rn->v1, v1, 0, rn);
			break;
	}
}

void anchorTriface(RNode *rn, float *v1, float *v2, float *v3, int flag)
{
	switch(flag) {
		case 1:
			makeface(rn->v1, v1, rn->v3, 0, rn);
			makeface(v1, rn->v2, rn->v3, 0, rn);
			break;
		case 2:
			makeface(rn->v2, v2, rn->v1, 0, rn);
			makeface(v2, rn->v3, rn->v1, 0, rn);
			break;
		case 4:
			makeface(rn->v3, v3, rn->v2, 0, rn);
			makeface(v3, rn->v1, rn->v2, 0, rn);
			break;
			
		case 3:
			makeface(rn->v1, v2, rn->v3, 0, rn);
			makeface(rn->v1, v1, v2, 0, rn);
			makeface(v1, rn->v2, v2, 0, rn);
			break;
		case 6:
			makeface(rn->v2, v3, rn->v1, 0, rn);
			makeface(rn->v2, v2, v3, 0, rn);
			makeface(v2, rn->v3, v3, 0, rn);
			break;
		case 5:
			makeface(rn->v3, v1, rn->v2, 0, rn);
			makeface(rn->v3, v3, v1, 0, rn);
			makeface(v3, rn->v1, v1, 0, rn);
			break;
			
		case 7:
			makeface(v1, v2, v3, 0, rn);
			makeface(rn->v1, v1, v3, 0, rn);
			makeface(rn->v2, v2, v1, 0, rn);
			makeface(rn->v3, v3, v2, 0, rn);
			break;
	}	
}


float *findmiddlevertex(RNode *node, RNode *nb, float *v1, float *v2)
{
	int test= 0;

	if(nb==0) return 0;
	
	if(nb->ed1==node) {
		if(nb->v1==v1 || nb->v1==v2) test++;
		if(nb->v2==v1 || nb->v2==v2) test+=2;
		if(test==1) return nb->v2;
		else if(test==2) return nb->v1;
	}
	else if(nb->ed2==node) {
		if(nb->v2==v1 || nb->v2==v2) test++;
		if(nb->v3==v1 || nb->v3==v2) test+=2;
		if(test==1) return nb->v3;
		else if(test==2) return nb->v2;
	}
	else if(nb->ed3==node) {
		if(nb->type==4) {
			if(nb->v3==v1 || nb->v3==v2) test++;
			if(nb->v4==v1 || nb->v4==v2) test+=2;
			if(test==1) return nb->v4;
			else if(test==2) return nb->v3;
			
		}
		else {
			if(nb->v3==v1 || nb->v3==v2) test++;
			if(nb->v1==v1 || nb->v1==v2) test+=2;
			if(test==1) return nb->v1;
			else if(test==2) return nb->v3;
		}
	}
	else if(nb->ed4==node) {
		if(nb->v4==v1 || nb->v4==v2) test++;
		if(nb->v1==v1 || nb->v1==v2) test+=2;
		if(test==1) return nb->v1;
		else if(test==2) return nb->v4;
	}
	return 0;
}

void make_face_tab()	/* takes care of anchoring */
{
	RNode *rn, **el;
	Face *face = NULL;
	float *v1, *v2, *v3, *v4;
	int a, flag, w1, w2, w3;
	char *charcol;

	if(RG.totelem==0) return;
	
	init_face_tab();
	
	RG.igamma= 1.0/RG.gamma;
	RG.radfactor= RG.radfac*pow(64*64, RG.igamma);

	/* convert face colors */
	el= RG.elem;
	for(a=RG.totelem; a>0; a--, el++) {
		rn= *el;
		charcol= (char *)&( rn->col );

		charcol[3]= calculatecolor(rn->totrad[0]);
		charcol[2]= calculatecolor(rn->totrad[1]);
		charcol[1]= calculatecolor(rn->totrad[2]);
	}
	
	/* check nodes and make faces */
	el= RG.elem;
	for(a=RG.totelem; a>0; a--, el++) {

		rn= *el;
		
		rn->v1[3]= 0.0;
		rn->v2[3]= 0.0;
		rn->v3[3]= 0.0;
		if(rn->v4) rn->v4[3]= 0.0;
		
		/* test edges for subdivide */
		flag= 0;
		v1= v2= v3= v4= 0;
		if(rn->ed1) {
			v1= findmiddlevertex(rn, rn->ed1->down1, rn->v1, rn->v2);
			if(v1) flag |= 1;
		}
		if(rn->ed2) {
			v2= findmiddlevertex(rn, rn->ed2->down1, rn->v2, rn->v3);
			if(v2) flag |= 2;
		}
		if(rn->ed3) {
			if(rn->type==4)
				v3= findmiddlevertex(rn, rn->ed3->down1, rn->v3, rn->v4);
			else
				v3= findmiddlevertex(rn, rn->ed3->down1, rn->v3, rn->v1);
			if(v3) flag |= 4;
		}
		if(rn->ed4) {
			v4= findmiddlevertex(rn, rn->ed4->down1, rn->v4, rn->v1);
			if(v4) flag |= 8;
		}
		
		/* using flag and vertexpointers now Faces can be made */
		
		if(flag==0) {
			makeface(rn->v1, rn->v2, rn->v3, rn->v4, rn);
		}
		else if(rn->type==4) anchorQuadface(rn, v1, v2, v3, v4, flag);
		else anchorTriface(rn, v1, v2, v3, flag);
	}
	
	/* add */
	for(a=0; a<RG.totface; a++) {
		
		RAD_NEXTFACE(a);
		
		if(face->v4) {
			addaccuweight( (char *)&(face->col), (char *)(face->v1+3), 16 );
			addaccuweight( (char *)&(face->col), (char *)(face->v2+3), 16 );
			addaccuweight( (char *)&(face->col), (char *)(face->v3+3), 16 );
			addaccuweight( (char *)&(face->col), (char *)(face->v4+3), 16 );
		}
		else {
			triaweight(face, &w1, &w2, &w3);
			addaccuweight( (char *)&(face->col), (char *)(face->v1+3), w1 );
			addaccuweight( (char *)&(face->col), (char *)(face->v2+3), w2 );
			addaccuweight( (char *)&(face->col), (char *)(face->v3+3), w3 );
		}
	}

}

void filterFaces()
{
	/* put vertex colors in faces, and put them back */
	
	Face *face = NULL;
	int a, w1, w2, w3;

	if(RG.totface==0) return;

	/* clear */
	for(a=0; a<RG.totface; a++) {
		RAD_NEXTFACE(a);
		face->col= 0;
	}
	
	/* add: vertices with faces */
	for(a=0; a<RG.totface; a++) {
		RAD_NEXTFACE(a);
		
		if(face->v4) {
			addaccuweight( (char *)(face->v1+3), (char *)&(face->col), 16 );
			addaccuweight( (char *)(face->v2+3), (char *)&(face->col), 16 );
			addaccuweight( (char *)(face->v3+3), (char *)&(face->col), 16 );
			addaccuweight( (char *)(face->v4+3), (char *)&(face->col), 16 );
		}
		else {
			triaweight(face, &w1, &w2, &w3);
			addaccuweight( (char *)(face->v1+3), (char *)&(face->col), w1 );
			addaccuweight( (char *)(face->v2+3), (char *)&(face->col), w2 );
			addaccuweight( (char *)(face->v3+3), (char *)&(face->col), w3 );
		}
	}
	
	/* clear */
	for(a=0; a<RG.totface; a++) {
		RAD_NEXTFACE(a);
		face->v1[3]= 0.0;
		face->v2[3]= 0.0;
		face->v3[3]= 0.0;
		if(face->v4) face->v4[3]= 0.0;
	}


	/* add: faces with vertices */
	for(a=0; a<RG.totface; a++) {
		
		RAD_NEXTFACE(a);
		
		if(face->v4) {
			addaccuweight( (char *)&(face->col), (char *)(face->v1+3), 16 );
			addaccuweight( (char *)&(face->col), (char *)(face->v2+3), 16 );
			addaccuweight( (char *)&(face->col), (char *)(face->v3+3), 16 );
			addaccuweight( (char *)&(face->col), (char *)(face->v4+3), 16 );
		}
		else {
			triaweight(face, &w1, &w2, &w3);
			addaccuweight( (char *)&(face->col), (char *)(face->v1+3), w1 );
			addaccuweight( (char *)&(face->col), (char *)(face->v2+3), w2 );
			addaccuweight( (char *)&(face->col), (char *)(face->v3+3), w3 );
		}
	}
}

void calcfiltrad(RNode *rn, float *cd)
{
	float area;

	cd[0]= 2.0*rn->totrad[0];
	cd[1]= 2.0*rn->totrad[1];
	cd[2]= 2.0*rn->totrad[2];
	area= 2.0;
	
	if(rn->ed1) {
		cd[0]+= rn->ed1->totrad[0];
		cd[1]+= rn->ed1->totrad[1];
		cd[2]+= rn->ed1->totrad[2];
		area+= 1.0;
	}
	if(rn->ed2) {
		cd[0]+= rn->ed2->totrad[0];
		cd[1]+= rn->ed2->totrad[1];
		cd[2]+= rn->ed2->totrad[2];
		area+= 1.0;
	}
	if(rn->ed3) {
		cd[0]+= rn->ed3->totrad[0];
		cd[1]+= rn->ed3->totrad[1];
		cd[2]+= rn->ed3->totrad[2];
		area+= 1.0;
	}
	if(rn->ed4) {
		cd[0]+= rn->ed4->totrad[0];
		cd[1]+= rn->ed4->totrad[1];
		cd[2]+= rn->ed4->totrad[2];
		area+= 1.0;
	}
	cd[0]/= area;
	cd[1]/= area;
	cd[2]/= area;
	
}

void filterNodes()
{
	/* colors from nodes in tempblock and back */
	
	RNode *rn, **el;
	float *coldata, *cd;
	int a;

	if(RG.totelem==0) return;
	/* the up-nodes need a color */
	el= RG.elem;
	for(a=0; a<RG.totelem; a++, el++) {
		rn= *el;
		if(rn->up) {
			rn->up->totrad[0]= 0.0;
			rn->up->totrad[1]= 0.0;
			rn->up->totrad[2]= 0.0;
			if(rn->up->up) {
				rn->up->up->totrad[0]= 0.0;
				rn->up->up->totrad[1]= 0.0;
				rn->up->up->totrad[2]= 0.0;
			}
		}
	}
	el= RG.elem;
	for(a=0; a<RG.totelem; a++, el++) {
		rn= *el;
		if(rn->up) {
			rn->up->totrad[0]+= 0.5*rn->totrad[0];
			rn->up->totrad[1]+= 0.5*rn->totrad[1];
			rn->up->totrad[2]+= 0.5*rn->totrad[2];
			if(rn->up->up) {
				rn->up->up->totrad[0]+= 0.25*rn->totrad[0];
				rn->up->up->totrad[1]+= 0.25*rn->totrad[1];
				rn->up->up->totrad[2]+= 0.25*rn->totrad[2];
			}
		}
	}
	
	/* add using area */
	cd= coldata= MEM_mallocN(3*4*RG.totelem, "filterNodes");
	el= RG.elem;
	for(a=0; a<RG.totelem; a++, el++) {
		calcfiltrad(*el, cd);
		cd+= 3;
	}
	
	cd= coldata;
	el= RG.elem;
	for(a=0; a<RG.totelem; a++, el++) {
		rn= *el;
		VECCOPY(rn->totrad, cd);
		cd+= 3;
	}
	MEM_freeN(coldata);
}

void removeEqualNodes(short limit)
{
	/* nodes with equal colors: remove */
	RNode **el, *rn, *rn1;
	float thresh, f1, f2;
	int a, foundone=1, ok;
	int c1, c2;
	
	if(limit==0) return;
	
	thresh= 1.0/(256.0*RG.radfactor);
	thresh= 3.0*pow(thresh, RG.gamma);
	
	waitcursor(1);
		
	while(foundone) {
		foundone= 0;
		
		el= RG.elem;
		for(a=RG.totelem; a>1; a--, el++) {
			rn= *el;
			rn1= *(el+1);
			
			if(rn!=rn->par->first && rn1!=rn1->par->first) {
				if(rn->up && rn->up==rn1->up) {
					f1= rn->totrad[0]+ rn->totrad[1]+ rn->totrad[2];
					f2= rn1->totrad[0]+ rn1->totrad[1]+ rn1->totrad[2];
					
					ok= 0;
					if(f1<thresh && f2<thresh) ok= 1;
					else {
						c1= calculatecolor(rn->totrad[0]);
						c2= calculatecolor(rn1->totrad[0]);
						
						if( abs(c1-c2)<=limit ) {
							c1= calculatecolor(rn->totrad[1]);
							c2= calculatecolor(rn1->totrad[1]);
							
							if( abs(c1-c2)<=limit ) {
								c1= calculatecolor(rn->totrad[2]);
								c2= calculatecolor(rn1->totrad[2]);
								
								if( abs(c1-c2)<=limit ) {
									ok= 1;
								}
							}
						}
					}
					
					if(ok) {
						rn->up->totrad[0]= 0.5*(rn->totrad[0]+rn1->totrad[0]);
						rn->up->totrad[1]= 0.5*(rn->totrad[1]+rn1->totrad[1]);
						rn->up->totrad[2]= 0.5*(rn->totrad[2]+rn1->totrad[2]);
						rn1= rn->up;
						deleteNodes(rn1);
						if(rn1->down1) ;
						else {
							foundone++;
							a--; el++;
						}
					}
				}
			}
		}
		if(foundone) {
			makeGlobalElemArray();
		}
	}
	waitcursor(0);
}

#define BLSIZE	32000

void rad_addmesh(void)
{
	Face *face = NULL;
	Object *ob;
	Mesh *me;
	MVert *mvert;
	MFace *mface;
	Material *ma=0;
	unsigned int *md, *coldata, *cd;
	float **fpp, **poindata;
	float cent[3], min[3], max[3];
	int a, vcount, vlnr, startf, endf;
	
	if(RG.totface==0) return;
	
	if(RG.totmat==MAXMAT) notice("warning: cannot assign more than 16 materials to 1 mesh");
	
	/* make sure there's alpha in the color, to distinguish */
	for(a=0; a<RG.totface; a++) {
		RAD_NEXTFACE(a);
		*((unsigned int *)face->v1+3) |= 0x1000000; 
		*((unsigned int *)face->v2+3) |= 0x1000000;
		*((unsigned int *)face->v3+3) |= 0x1000000;
		if(face->v4) {
			*((unsigned int *)face->v4+3) |= 0x1000000;
		}
	}
	
	/* we write in blocks of BLSIZE vertices max */
	coldata= MEM_mallocN(4*BLSIZE, "writefaces");
	poindata= MEM_mallocN(sizeof(void *)*BLSIZE, "writefaces1");
	
	vlnr= 0; 
	endf= 0;
	while(endf<RG.totface) {
		
		cd= coldata;
		fpp= poindata;
		startf= endf;
		vcount= 0;
	
		face= RG.facebase[(startf-1)>>10]+((startf-1) & 1023);
		for(vlnr=startf; vlnr<RG.totface; vlnr++) {
			RAD_NEXTFACE(vlnr);

			if( *((unsigned int *)face->v1+3) & 0x1000000 ) { /* is a color */
				*cd= *((unsigned int *)face->v1+3);
				*((unsigned int *)face->v1+3) = vcount;
				*fpp= face->v1;
				fpp++; vcount++; cd++;
			}
			if( *((unsigned int *)face->v2+3) & 0x1000000 ) {
				*cd= *((unsigned int *)face->v2+3);
				*((unsigned int *)face->v2+3) = vcount;
				*fpp= face->v2;
				fpp++; vcount++; cd++;
			}
			if( *((unsigned int *)face->v3+3) & 0x1000000 ) {
				*cd= *((unsigned int *)face->v3+3);
				*((unsigned int *)face->v3+3) = vcount;
				*fpp= face->v3;
				fpp++; vcount++; cd++;
			}
			if(face->v4 && ( *((unsigned int *)face->v4+3) & 0x1000000 ) ) {
				*cd= *((unsigned int *)face->v4+3);
				*((unsigned int *)face->v4+3) = vcount;
				*fpp= face->v4;
				fpp++; vcount++; cd++;
			}

			if(vcount>=BLSIZE-4) {
				vlnr++;
				break;
			}
		}

		/* we now make the Mesh */
		endf= vlnr;

		ob= add_object(OB_MESH);
		
		me= ob->data;
		me->totvert= vcount;
		me->totface= endf-startf;
		me->flag= 0;
		me->mvert= MEM_callocN(me->totvert*sizeof(MVert), "mverts");
		me->mcol= MEM_callocN(4*me->totface*sizeof(MCol), "mverts");
		me->mface= MEM_callocN(me->totface*sizeof(MFace), "mface");


		/* materials, and set VCOL flag */
		for(a=0; a<RG.totmat; a++) {
			assign_material(ob, RG.matar[a], a+1);
			ma= RG.matar[a];
			if(ma) ma->mode |= MA_VERTEXCOL;
		}

		/* verts */
		mvert= me->mvert;
		fpp= poindata;
		for(a=0; a<me->totvert; a++, mvert++, fpp++) {
			VECCOPY(mvert->co, *fpp);
		}

		/* faces and mcol */
		mface= me->mface;
		md= (unsigned int *)me->mcol;

		face= RG.facebase[(startf-1)>>10]+((startf-1) & 1023);
		for(a=startf; a<endf; a++, md+=4, mface++) {
			RAD_NEXTFACE(a);
			mface->v1= *((unsigned int *)face->v1+3);
			mface->v2= *((unsigned int *)face->v2+3);
			mface->v3= *((unsigned int *)face->v3+3);
			if(face->v4) mface->v4= *((unsigned int *)face->v4+3);

			mface->edcode= 3;
			test_index_mface(mface, face->v4 ? 4 : 3);
			mface->mat_nr= face->matindex;
			
			md[0]= coldata[mface->v1];
			md[1]= coldata[mface->v2];
			md[2]= coldata[mface->v3];
			md[3]= coldata[mface->v4];
		}
		
		/* boundbox and centrenew */
		
		INIT_MINMAX(min, max);

		mvert= me->mvert;
		for(a=0; a<me->totvert; a++, mvert++) {
			DO_MINMAX(mvert->co, min, max);
		}

		cent[0]= (min[0]+max[0])/2.0;
		cent[1]= (min[1]+max[1])/2.0;
		cent[2]= (min[2]+max[2])/2.0;

		mvert= me->mvert;
		for(a=0; a<me->totvert; a++, mvert++) {
			VecSubf(mvert->co, mvert->co, cent);
		}
		
		VECCOPY(ob->loc, cent);
		tex_space_mesh(me);
		
		/* restore colors */
		face= RG.facebase[(startf-1)>>10]+((startf-1) & 1023);
		for(a=startf; a<endf; a++) {
			RAD_NEXTFACE(a);

			cd= ((unsigned int *)face->v1+3);
			if( *cd < 0x1000000 ) *cd= coldata[*cd];
			cd= ((unsigned int *)face->v2+3);
			if( *cd < 0x1000000 ) *cd= coldata[*cd];
			cd= ((unsigned int *)face->v3+3);
			if( *cd < 0x1000000 ) *cd= coldata[*cd];
			if(face->v4) {
				cd= ((unsigned int *)face->v4+3);
				if( *cd < 0x1000000 ) *cd= coldata[*cd];
			}
		}
	}
		
	MEM_freeN(coldata);
	MEM_freeN(poindata);
	
}

void rad_replacemesh(void)
{
	RPatch *rp;
	
	deselectall();
	
	rp= RG.patchbase.first;
	while(rp) {
		if( exist_object(rp->from)) rp->from->flag |= SELECT;
		rp= rp->next;
	}
	
	copy_objectflags();
	delete_obj(1);
	
	rad_addmesh();
}

