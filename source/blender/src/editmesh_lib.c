/**
 * $Id: 
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
 * The Original Code is Copyright (C) 2004 by NaN Holding BV.
 * All rights reserved.
 *
 * The Original Code is: all of this file.
 *
 * Contributor(s): none yet.
 *
 * ***** END GPL/BL DUAL LICENSE BLOCK *****
 */

/*

editmesh_lib: generic (no UI, no menus) operations/evaluators for editmesh data

*/

#include <stdlib.h>
#include <string.h>
#include <math.h>

#ifdef HAVE_CONFIG_H
#include <config.h>
#endif

#ifdef WIN32
#include "BLI_winstuff.h"
#endif
#include "MEM_guardedalloc.h"


#include "DNA_mesh_types.h"
#include "DNA_meshdata_types.h"
#include "DNA_object_types.h"

#include "BLI_blenlib.h"
#include "BLI_arithb.h"
#include "BLI_editVert.h"

#include "BKE_global.h"
#include "BKE_mesh.h"
#include "BKE_utildefines.h"

#include "BIF_editmesh.h"

#include "editmesh.h"


/* ********************* */

int editmesh_nfaces_selected(void)
{
	EditMesh *em = G.editMesh;
	EditFace *efa;
	int count= 0;

	for (efa= em->faces.first; efa; efa= efa->next)
		if (faceselectedAND(efa, SELECT))
			count++;

	return count;
}

int editmesh_nvertices_selected(void)
{
	EditMesh *em = G.editMesh;
	EditVert *eve;
	int count= 0;

	for (eve= em->verts.first; eve; eve= eve->next)
		if (eve->f & SELECT)
			count++;

	return count;
}

/* ***************** */

short extrudeflag(short flag,short type)
{
	/* when type=1 old extrusion faces are removed (for spin etc) */
	/* all verts with (flag & 'flag'): extrude */
	/* from old verts, 'flag' is cleared, in new ones it is set */
	EditMesh *em = G.editMesh;
	EditVert *eve, *v1, *v2, *v3, *v4, *nextve;
	EditEdge *eed, *e1, *e2, *e3, *e4, *nexted;
	EditFace *efa, *efa2, *nextvl;
	short sel=0, deloud= 0, smooth= 0;

	if(G.obedit==0 || get_mesh(G.obedit)==0) return 0;

	/* clear vert flag f1, we use this to detext a loose selected vertice */
	eve= em->verts.first;
	while(eve) {
		if(eve->f & flag) eve->f1= 1;
		else eve->f1= 0;
		eve= eve->next;
	}
	/* clear edges counter flag, if selected we set it at 1 */
	eed= em->edges.first;
	while(eed) {
		if( (eed->v1->f & flag) && (eed->v2->f & flag) ) {
			eed->f= 1;
			eed->v1->f1= 0;
			eed->v2->f1= 0;
		}
		else eed->f= 0;
		
		eed->f1= 1;		/* this indicates it is an 'old' edge (in this routine we make new ones) */
		
		eed= eed->next;
	}

	/* we set a flag in all selected faces, and increase the associated edge counters */

	efa= em->faces.first;
	while(efa) {
		efa->f= 0;

		if (efa->flag & ME_SMOOTH) {
			if (faceselectedOR(efa, 1)) smooth= 1;
		}
		
		if(faceselectedAND(efa, flag)) {
			e1= efa->e1;
			e2= efa->e2;
			e3= efa->e3;
			e4= efa->e4;

			if(e1->f < 3) e1->f++;
			if(e2->f < 3) e2->f++;
			if(e3->f < 3) e3->f++;
			if(e4 && e4->f < 3) e4->f++;
			efa->f= 1;
		}
		else if(faceselectedOR(efa, flag)) {
			e1= efa->e1;
			e2= efa->e2;
			e3= efa->e3;
			e4= efa->e4;
			
			if( (e1->v1->f & flag) && (e1->v2->f & flag) ) e1->f1= 2;
			if( (e2->v1->f & flag) && (e2->v2->f & flag) ) e2->f1= 2;
			if( (e3->v1->f & flag) && (e3->v2->f & flag) ) e3->f1= 2;
			if( e4 && (e4->v1->f & flag) && (e4->v2->f & flag) ) e4->f1= 2;
		}
		
		efa= efa->next;
	}

	/* set direction of edges */
	efa= em->faces.first;
	while(efa) {
		if(efa->f== 0) {
			if(efa->e1->f==2) {
				if(efa->e1->v1 == efa->v1) efa->e1->dir= 0;
				else efa->e1->dir= 1;
			}
			if(efa->e2->f==2) {
				if(efa->e2->v1 == efa->v2) efa->e2->dir= 0;
				else efa->e2->dir= 1;
			}
			if(efa->e3->f==2) {
				if(efa->e3->v1 == efa->v3) efa->e3->dir= 0;
				else efa->e3->dir= 1;
			}
			if(efa->e4 && efa->e4->f==2) {
				if(efa->e4->v1 == efa->v4) efa->e4->dir= 0;
				else efa->e4->dir= 1;
			}
		}
		efa= efa->next;
	}	


	/* the current state now is:
		eve->f1==1: loose selected vertex 

		eed->f==0 : edge is not selected, no extrude
		eed->f==1 : edge selected, is not part of a face, extrude
		eed->f==2 : edge selected, is part of 1 face, extrude
		eed->f==3 : edge selected, is part of more faces, no extrude
		
		eed->f1==0: new edge
		eed->f1==1: edge selected, is part of selected face, when eed->f==3: remove
		eed->f1==2: edge selected, is not part of a selected face
					
		efa->f==1 : duplicate this face
	*/

	/* copy all selected vertices, */
	/* write pointer to new vert in old struct at eve->vn */
	eve= em->verts.last;
	while(eve) {
		eve->f&= ~128;  /* clear, for later test for loose verts */
		if(eve->f & flag) {
			sel= 1;
			v1= addvertlist(0);
			
			VECCOPY(v1->co, eve->co);
			v1->f= eve->f;
			eve->f-= flag;
			eve->vn= v1;
		}
		else eve->vn= 0;
		eve= eve->prev;
	}

	if(sel==0) return 0;

	/* all edges with eed->f==1 or eed->f==2 become faces */
	/* if deloud==1 then edges with eed->f>2 are removed */
	eed= em->edges.last;
	while(eed) {
		nexted= eed->prev;
		if( eed->f<3) {
			eed->v1->f|=128;  /* = no loose vert! */
			eed->v2->f|=128;
		}
		if( (eed->f==1 || eed->f==2) ) {
			if(eed->f1==2) deloud=1;
			
			if(eed->dir==1) efa2= addfacelist(eed->v1, eed->v2, eed->v2->vn, eed->v1->vn, NULL);
			else efa2= addfacelist(eed->v2, eed->v1, eed->v1->vn, eed->v2->vn, NULL);
			if (smooth) efa2->flag |= ME_SMOOTH;

			/* Needs smarter adaption of existing creases.
			 * If addedgelist is used, make sure seams are set to 0 on these
			 * new edges, since we do not want to add any seams on extrusion.
			 */
			efa2->e1->crease= eed->crease;
			efa2->e2->crease= eed->crease;
			efa2->e3->crease= eed->crease;
			if(efa2->e4) efa2->e4->crease= eed->crease;
		}

		eed= nexted;
	}
	if(deloud) {
		eed= em->edges.first;
		while(eed) {
			nexted= eed->next;
			if(eed->f==3 && eed->f1==1) {
				remedge(eed);
				free_editedge(eed);
			}
			eed= nexted;
		}
	}
	/* duplicate faces, if necessart remove old ones  */
	efa= em->faces.first;
	while(efa) {
		nextvl= efa->next;
		if(efa->f & 1) {
		
			v1= efa->v1->vn;
			v2= efa->v2->vn;
			v3= efa->v3->vn;
			if(efa->v4) v4= efa->v4->vn; else v4= 0;
			
			efa2= addfacelist(v1, v2, v3, v4, efa);
			
			if(deloud) {
				BLI_remlink(&em->faces, efa);
				free_editface(efa);
			}
			if (smooth) efa2->flag |= ME_SMOOTH;			
		}
		efa= nextvl;
	}
	/* for all vertices with eve->vn!=0 
		if eve->f1==1: make edge
		if flag!=128 : if deloud==1: remove
	*/
	eve= em->verts.last;
	while(eve) {
		nextve= eve->prev;
		if(eve->vn) {
			if(eve->f1==1) addedgelist(eve, eve->vn, NULL);
			else if( (eve->f & 128)==0) {
				if(deloud) {
					BLI_remlink(&em->verts,eve);
					free_editvert(eve);
					eve= NULL;
				}
			}
		}
		if(eve) eve->f&= ~128;
		
		eve= nextve;
	}

	return 1;
}

void rotateflag(short flag, float *cent, float rotmat[][3])
{
	/* all verts with (flag & 'flag') rotate */
	EditMesh *em = G.editMesh;
	EditVert *eve;

	eve= em->verts.first;
	while(eve) {
		if(eve->f & flag) {
			eve->co[0]-=cent[0];
			eve->co[1]-=cent[1];
			eve->co[2]-=cent[2];
			Mat3MulVecfl(rotmat,eve->co);
			eve->co[0]+=cent[0];
			eve->co[1]+=cent[1];
			eve->co[2]+=cent[2];
		}
		eve= eve->next;
	}
}

void translateflag(short flag, float *vec)
{
	/* all verts with (flag & 'flag') translate */
	EditMesh *em = G.editMesh;
	EditVert *eve;

	eve= em->verts.first;
	while(eve) {
		if(eve->f & flag) {
			eve->co[0]+=vec[0];
			eve->co[1]+=vec[1];
			eve->co[2]+=vec[2];
		}
		eve= eve->next;
	}
}


void delfaceflag(int flag)
{
	EditMesh *em = G.editMesh;
	/* delete all faces with 'flag', including edges and loose vertices */
	/* in vertices the 'flag' is cleared */
	EditVert *eve,*nextve;
	EditEdge *eed, *nexted;
	EditFace *efa,*nextvl;

	eed= em->edges.first;
	while(eed) {
		eed->f= 0;
		eed= eed->next;
	}

	efa= em->faces.first;
	while(efa) {
		nextvl= efa->next;
		if(faceselectedAND(efa, flag)) {
			
			efa->e1->f= 1;
			efa->e2->f= 1;
			efa->e3->f= 1;
			if(efa->e4) {
				efa->e4->f= 1;
			}
								
			BLI_remlink(&em->faces, efa);
			free_editface(efa);
		}
		efa= nextvl;
	}
	/* all faces with 1, 2 (3) vertices selected: make sure we keep the edges */
	efa= em->faces.first;
	while(efa) {
		efa->e1->f= 0;
		efa->e2->f= 0;
		efa->e3->f= 0;
		if(efa->e4) {
			efa->e4->f= 0;
		}

		efa= efa->next;
	}
	
	/* test all edges for vertices with 'flag', and clear */
	eed= em->edges.first;
	while(eed) {
		nexted= eed->next;
		if(eed->f==1) {
			remedge(eed);
			free_editedge(eed);
		}
		else if( (eed->v1->f & flag) || (eed->v2->f & flag) ) {
			eed->v1->f&= ~flag;
			eed->v2->f&= ~flag;
		}
		eed= nexted;
	}
	/* vertices with 'flag' now are the loose ones, and will be removed */
	eve= em->verts.first;
	while(eve) {
		nextve= eve->next;
		if(eve->f & flag) {
			BLI_remlink(&em->verts, eve);
			free_editvert(eve);
		}
		eve= nextve;
	}

}

/* ********************* */

static int contrpuntnorm(float *n, float *puno)  /* dutch: check vertex normal */
{
	float inp;

	inp= n[0]*puno[0]+n[1]*puno[1]+n[2]*puno[2];

	/* angles 90 degrees: dont flip */
	if(inp> -0.000001) return 0;

	return 1;
}


void vertexnormals(int testflip)
{
	EditMesh *em = G.editMesh;
	Mesh *me;
	EditVert *eve;
	EditFace *efa;	
	float n1[3], n2[3], n3[3], n4[3], co[4], fac1, fac2, fac3, fac4, *temp;
	float *f1, *f2, *f3, *f4, xn, yn, zn;
	float len;
	
	if(G.obedit && G.obedit->type==OB_MESH) {
		me= G.obedit->data;
		if((me->flag & ME_TWOSIDED)==0) testflip= 0;
	}

	if(G.totvert==0) return;

	if(G.totface==0) {
		/* fake vertex normals for 'halo puno'! */
		eve= em->verts.first;
		while(eve) {
			VECCOPY(eve->no, eve->co);
			Normalise( (float *)eve->no);
			eve= eve->next;
		}
		return;
	}

	/* clear normals */	
	eve= em->verts.first;
	while(eve) {
		eve->no[0]= eve->no[1]= eve->no[2]= 0.0;
		eve= eve->next;
	}
	
	/* calculate cosine angles and add to vertex normal */
	efa= em->faces.first;
	while(efa) {
		VecSubf(n1, efa->v2->co, efa->v1->co);
		VecSubf(n2, efa->v3->co, efa->v2->co);
		Normalise(n1);
		Normalise(n2);

		if(efa->v4==0) {
			VecSubf(n3, efa->v1->co, efa->v3->co);
			Normalise(n3);
			
			co[0]= saacos(-n3[0]*n1[0]-n3[1]*n1[1]-n3[2]*n1[2]);
			co[1]= saacos(-n1[0]*n2[0]-n1[1]*n2[1]-n1[2]*n2[2]);
			co[2]= saacos(-n2[0]*n3[0]-n2[1]*n3[1]-n2[2]*n3[2]);
			
		}
		else {
			VecSubf(n3, efa->v4->co, efa->v3->co);
			VecSubf(n4, efa->v1->co, efa->v4->co);
			Normalise(n3);
			Normalise(n4);
			
			co[0]= saacos(-n4[0]*n1[0]-n4[1]*n1[1]-n4[2]*n1[2]);
			co[1]= saacos(-n1[0]*n2[0]-n1[1]*n2[1]-n1[2]*n2[2]);
			co[2]= saacos(-n2[0]*n3[0]-n2[1]*n3[1]-n2[2]*n3[2]);
			co[3]= saacos(-n3[0]*n4[0]-n3[1]*n4[1]-n3[2]*n4[2]);
		}
		
		temp= efa->v1->no;
		if(testflip && contrpuntnorm(efa->n, temp) ) co[0]= -co[0];
		temp[0]+= co[0]*efa->n[0];
		temp[1]+= co[0]*efa->n[1];
		temp[2]+= co[0]*efa->n[2];
		
		temp= efa->v2->no;
		if(testflip && contrpuntnorm(efa->n, temp) ) co[1]= -co[1];
		temp[0]+= co[1]*efa->n[0];
		temp[1]+= co[1]*efa->n[1];
		temp[2]+= co[1]*efa->n[2];
		
		temp= efa->v3->no;
		if(testflip && contrpuntnorm(efa->n, temp) ) co[2]= -co[2];
		temp[0]+= co[2]*efa->n[0];
		temp[1]+= co[2]*efa->n[1];
		temp[2]+= co[2]*efa->n[2];
		
		if(efa->v4) {
			temp= efa->v4->no;
			if(testflip && contrpuntnorm(efa->n, temp) ) co[3]= -co[3];
			temp[0]+= co[3]*efa->n[0];
			temp[1]+= co[3]*efa->n[1];
			temp[2]+= co[3]*efa->n[2];
		}
		
		efa= efa->next;
	}

	/* normalise vertex normals */
	eve= em->verts.first;
	while(eve) {
		len= Normalise(eve->no);
		if(len==0.0) {
			VECCOPY(eve->no, eve->co);
			Normalise( eve->no);
		}
		eve= eve->next;
	}
	
	/* vertex normal flip-flags for shade (render) */
	efa= em->faces.first;
	while(efa) {
		efa->f=0;			

		if(testflip) {
			f1= efa->v1->no;
			f2= efa->v2->no;
			f3= efa->v3->no;
			
			fac1= efa->n[0]*f1[0] + efa->n[1]*f1[1] + efa->n[2]*f1[2];
			if(fac1<0.0) {
				efa->f = ME_FLIPV1;
			}
			fac2= efa->n[0]*f2[0] + efa->n[1]*f2[1] + efa->n[2]*f2[2];
			if(fac2<0.0) {
				efa->f += ME_FLIPV2;
			}
			fac3= efa->n[0]*f3[0] + efa->n[1]*f3[1] + efa->n[2]*f3[2];
			if(fac3<0.0) {
				efa->f += ME_FLIPV3;
			}
			if(efa->v4) {
				f4= efa->v4->no;
				fac4= efa->n[0]*f4[0] + efa->n[1]*f4[1] + efa->n[2]*f4[2];
				if(fac4<0.0) {
					efa->f += ME_FLIPV4;
				}
			}
		}
		/* projection for cubemap! */
		xn= fabs(efa->n[0]);
		yn= fabs(efa->n[1]);
		zn= fabs(efa->n[2]);
		
		if(zn>xn && zn>yn) efa->f += ME_PROJXY;
		else if(yn>xn && yn>zn) efa->f += ME_PROJXZ;
		else efa->f += ME_PROJYZ;
		
		efa= efa->next;
	}
}

void flipface(EditFace *efa)
{
	if(efa->v4) {
		SWAP(EditVert *, efa->v2, efa->v4);
		SWAP(EditEdge *, efa->e1, efa->e4);
		SWAP(EditEdge *, efa->e2, efa->e3);
		SWAP(unsigned int, efa->tf.col[1], efa->tf.col[3]);
		SWAP(float, efa->tf.uv[1][0], efa->tf.uv[3][0]);
		SWAP(float, efa->tf.uv[1][1], efa->tf.uv[3][1]);
	}
	else {
		SWAP(EditVert *, efa->v2, efa->v3);
		SWAP(EditEdge *, efa->e1, efa->e3);
		SWAP(unsigned int, efa->tf.col[1], efa->tf.col[2]);
		efa->e2->dir= 1-efa->e2->dir;
		SWAP(float, efa->tf.uv[1][0], efa->tf.uv[2][0]);
		SWAP(float, efa->tf.uv[1][1], efa->tf.uv[2][1]);
	}
	if(efa->v4) CalcNormFloat4(efa->v1->co, efa->v2->co, efa->v3->co, efa->v4->co, efa->n);
	else CalcNormFloat(efa->v1->co, efa->v2->co, efa->v3->co, efa->n);
}


void flip_editnormals(void)
{
	EditMesh *em = G.editMesh;
	EditFace *efa;
	
	efa= em->faces.first;
	while(efa) {
		if( faceselectedAND(efa, 1) ) {
			flipface(efa);
		}
		efa= efa->next;
	}
}


void recalc_editnormals(void)
{
	EditMesh *em = G.editMesh;
	EditFace *efa;

	efa= em->faces.first;
	while(efa) {
		if(efa->v4) CalcNormFloat4(efa->v1->co, efa->v2->co, efa->v3->co, efa->v4->co, efa->n);
		else CalcNormFloat(efa->v1->co, efa->v2->co, efa->v3->co, efa->n);
		efa= efa->next;
	}
}


int faceselectedOR(EditFace *efa, int flag)
{
	
	if(efa->v1->f & flag) return 1;
	if(efa->v2->f & flag) return 1;
	if(efa->v3->f & flag) return 1;
	if(efa->v4 && (efa->v4->f & 1)) return 1;
	return 0;
}

int faceselectedAND(EditFace *efa, int flag)
{
	if(efa->v1->f & flag) {
		if(efa->v2->f & flag) {
			if(efa->v3->f & flag) {
				if(efa->v4) {
					if(efa->v4->f & flag) return 1;
				}
				else return 1;
			}
		}
	}
	return 0;
}

int compareface(EditFace *vl1, EditFace *vl2)
{
	EditVert *v1, *v2, *v3, *v4;
	
	if(vl1->v4 && vl2->v4) {
		v1= vl2->v1;
		v2= vl2->v2;
		v3= vl2->v3;
		v4= vl2->v4;
		
		if(vl1->v1==v1 || vl1->v2==v1 || vl1->v3==v1 || vl1->v4==v1) {
			if(vl1->v1==v2 || vl1->v2==v2 || vl1->v3==v2 || vl1->v4==v2) {
				if(vl1->v1==v3 || vl1->v2==v3 || vl1->v3==v3 || vl1->v4==v3) {
					if(vl1->v1==v4 || vl1->v2==v4 || vl1->v3==v4 || vl1->v4==v4) {
						return 1;
					}
				}
			}
		}
	}
	else if(vl1->v4==0 && vl2->v4==0) {
		v1= vl2->v1;
		v2= vl2->v2;
		v3= vl2->v3;

		if(vl1->v1==v1 || vl1->v2==v1 || vl1->v3==v1) {
			if(vl1->v1==v2 || vl1->v2==v2 || vl1->v3==v2) {
				if(vl1->v1==v3 || vl1->v2==v3 || vl1->v3==v3) {
					return 1;
				}
			}
		}
	}

	return 0;
}

int exist_face(EditVert *v1, EditVert *v2, EditVert *v3, EditVert *v4)
{
	EditMesh *em = G.editMesh;
	EditFace *efa, efatest;
	
	efatest.v1= v1;
	efatest.v2= v2;
	efatest.v3= v3;
	efatest.v4= v4;
	
	efa= em->faces.first;
	while(efa) {
		if(compareface(&efatest, efa)) return 1;
		efa= efa->next;
	}
	return 0;
}


float convex(float *v1, float *v2, float *v3, float *v4)
{
	float cross[3], test[3];
	float inpr;
	
	CalcNormFloat(v1, v2, v3, cross);
	CalcNormFloat(v1, v3, v4, test);

	inpr= cross[0]*test[0]+cross[1]*test[1]+cross[2]*test[2];

	return inpr;
}


