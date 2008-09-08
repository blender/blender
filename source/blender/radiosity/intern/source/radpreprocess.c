	/* *************************************** 
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
 * The Original Code is: all of this file.
 *
 * Contributor(s): none yet.
 *
 * ***** END GPL LICENSE BLOCK *****



    preproces.c	nov/dec 1992
				may 1999
	
	- collect from meshes
	- countglobaldata()
	- makeGlobalElemArray()
	
   $Id$

  *************************************** */

#include <stdlib.h>
#include <stdio.h>
#include <string.h>
#include <math.h>

#include "MEM_guardedalloc.h"

#include "BLI_blenlib.h"
#include "BLI_arithb.h"

#include "DNA_mesh_types.h"
#include "DNA_meshdata_types.h"
#include "DNA_object_types.h"
#include "DNA_scene_types.h"
#include "DNA_view3d_types.h"

#include "BKE_customdata.h"
#include "BKE_global.h"
#include "BKE_main.h"
#include "BKE_material.h"
#include "BKE_mesh.h"
#include "BKE_object.h" /* during_script() */
#include "BKE_utildefines.h"

#include "BIF_toolbox.h"

#include "BDR_editface.h"

#include "radio.h"

#include "BLO_sys_types.h" // for intptr_t support

#ifdef HAVE_CONFIG_H
#include <config.h>
#endif

void setparelem(RNode *rn, RPatch *par);

void splitconnected()
{
	/* Since input meshes can have faces with sharing vertices, the geometry is being tested here.
	 * Using normals and colors, faces are split separately. we do this by storing for each
	 * vertex a normal and a color
	 */
	RPatch *rp;
	RNode *rn;
	VeNoCo *vnc, *next, *vnc1;
	int a;
	
	/* test if we need a split */
	
	rp= RG.patchbase.first;
	while(rp) {
		rn= rp->first;
		if((rp->f1 & RAD_NO_SPLIT)==0) {
			for(a=0; a<rp->type; a++) {

				if(a==0) vnc= (VeNoCo *)rn->v1;
				else if(a==1) vnc= (VeNoCo *)rn->v2;
				else if(a==2) vnc= (VeNoCo *)rn->v3;
				else vnc= (VeNoCo *)rn->v4;

				if(vnc->flag==0) {
					vnc->n= (float *)rp->norm;
					vnc->col= (float *)rp->ref;
					vnc->flag= 1;
				}
				else {	/* is face from this vertex allowed for gouraud? */
					vnc1= vnc;
					while(vnc1) {
						if(VecCompare(vnc1->n, rp->norm, 0.01f)) {
							if(VecCompare(vnc1->col, rp->ref, 0.01f)) {
								break;
							}
						}
						vnc= vnc1;
						vnc1= vnc1->next;
					}
					if(vnc1==0) {
						vnc1= MEM_mallocN(sizeof(VeNoCo), "splitconn");
						vnc1->next= 0;
						vnc1->v= mallocVert();
						vnc->next= vnc1;
						VECCOPY(vnc1->v, vnc->v);
						vnc1->n= (float *)rp->norm;
						vnc1->col= (float *)rp->ref;
					}
					if(a==0) rn->v1= (float *)vnc1;
					else if(a==1) rn->v2= (float *)vnc1;
					else if(a==2) rn->v3= (float *)vnc1;
					else rn->v4= (float *)vnc1;
				}
			}
		}
		rp= rp->next;
	}
		/* adapt vertexpointers from nodes */
	
	rp= RG.patchbase.first;
	while(rp) {
		rn= rp->first;
		rn->v1= ((VeNoCo *)(rn->v1))->v;
		rn->v2= ((VeNoCo *)(rn->v2))->v;
		rn->v3= ((VeNoCo *)(rn->v3))->v;
		if(rp->type==4) rn->v4= ((VeNoCo *)(rn->v4))->v;

		rp= rp->next;
	}
	
	
	/* free all */
	vnc= RG.verts;
	for(a=0; a<RG.totvert; a++) {
		vnc1= vnc->next;
		while(vnc1) {
			next= vnc1->next;
			MEM_freeN(vnc1);
			vnc1= next;
		}
		vnc++;
	}
	MEM_freeN(RG.verts);
	RG.verts= 0;
}

int vergedge(const void *v1,const void *v2)
{
	int *e1, *e2;
	
	e1= (int *)v1;
	e2= (int *)v2;

	if( e1[0] > e2[0] ) return 1;
	else if( e1[0] < e2[0] ) return -1;
	else if( e1[1] > e2[1] ) return 1;
	else if( e1[1] < e2[1] ) return -1;

	return 0;
}


void addedge(float *v1, float *v2, EdSort *es)
{
	if( ((intptr_t)v1)<((intptr_t)v2) ) {
		es->v1= v1;
		es->v2= v2;
	}
	else {
		es->v2= v1;
		es->v1= v2;
	}
}

static void setedge(RNode *node, RNode *nb, int nr, int nrb)
{
	switch(nr) {
		case 1:
			node->ed1= nb;
			break;
		case 2:
			node->ed2= nb;
			break;
		case 3:
			node->ed3= nb;
			break;
		case 4:
			node->ed4= nb;
			break;
	}
	switch(nrb) {
		case 1:
			nb->ed1= node;
			break;
		case 2:
			nb->ed2= node;
			break;
		case 3:
			nb->ed3= node;
			break;
		case 4:
			nb->ed4= node;
			break;
	}
}

void setedgepointers()
{
	/* make edge-array and sort it */
	/* pairs of edges are put together: fill in pointers in nodes */
	EdSort *es, *esblock;
	RPatch *rp;
	RNode *rn;
	int tot= 0;
	
	rp= RG.patchbase.first;
	while(rp) {
		tot+= rp->type;
		rp= rp->next;
	}
	
	if(tot==0) return;
	
	es=esblock= MEM_mallocN(tot*sizeof(EdSort), "setedgepointers");
	rp= RG.patchbase.first;
	while(rp) {
		rn= rp->first;
		addedge(rn->v1, rn->v2, es);
		es->nr= 1;
		es->node= rn;
		es++;		
		addedge(rn->v2, rn->v3, es);
		es->nr= 2;
		es->node= rn;
		es++;		
		if(rp->type==3) {
			addedge(rn->v3, rn->v1, es);
			es->nr= 3;
			es->node= rn;
			es++;
		}
		else {
			addedge(rn->v3, rn->v4, es);
			es->nr= 3;
			es->node= rn;
			es++;					
			addedge(rn->v4, rn->v1, es);
			es->nr= 4;
			es->node= rn;
			es++;
		}
		rp= rp->next;
	}
	
	qsort(esblock,tot,sizeof(EdSort),vergedge);

	es= esblock;
	while(tot>0) {
		if( es->v1== (es+1)->v1 ) {
			if( es->v2== (es+1)->v2 ) {
				setedge(es->node, (es+1)->node, es->nr, (es+1)->nr);
				tot--;
				es++;
			}
		}
		es++;
		tot--;
	}

	MEM_freeN(esblock);
}

int materialIndex(Material *ma)
{
	int i = 0;
	for(i=0;i< RG.totmat; i++)
	{
		if (RG.matar[i] == ma) {
			return i;
		}
	}
	return -1;
}

void rad_collect_meshes()
{
	extern Material defmaterial;
	Base *base;
	Object *ob;
	Mesh *me;
	MVert *mvert;
	MFace *mface;
	MTFace *tf, *tface;
	Material *ma = NULL, *noma= NULL;
	RPatch *rp;
	RNode *rn;
	VeNoCo *vnc, **nodevert;
	float *vd, *v1, *v2, *v3, *v4 = NULL;
	int a, b, offs, index, mfdatatot;
	
	if (G.vd==NULL) {
		printf("Error, trying to collect radiosity meshes with no 3d view\n");
		return;
	}
	
	if(G.obedit) {
		if (!during_script()) error("Unable to perform function in EditMode");
		return;
	}

	set_radglobal();

	freeAllRad();

	start_fastmalloc("Radiosity");
					
	/* count the number of verts */
	RG.totvert= 0;
	RG.totface= 0;
	base= (G.scene->base.first);
	while(base) {
		if(((base)->flag & SELECT) && ((base)->lay & G.vd->lay) ) {
			if(base->object->type==OB_MESH) {
				base->flag |= OB_RADIO;
				me= base->object->data;
				RG.totvert+= me->totvert;
			}
		}
		base= base->next;
	}
	if(RG.totvert==0) {
		if (!during_script()) error("No vertices");
		return;
	}
	vnc= RG.verts= MEM_callocN(RG.totvert*sizeof(VeNoCo), "radioverts");

	RG.min[0]= RG.min[1]= RG.min[2]= 1.0e20f;
	RG.max[0]= RG.max[1]= RG.max[2]= -1.0e20f;

	mfdatatot= 0;
	
	/* min-max and material array */
	base= (G.scene->base.first);
	while(base) {
		if( ((base)->flag & SELECT) && ((base)->lay & G.vd->lay) ) {
			if(base->object->type==OB_MESH) {
				me= base->object->data;
				mvert= me->mvert;
				for(a=0; a<me->totvert; a++, mvert++) {
					vd= mallocVert();
					VECCOPY(vd, mvert->co);
					/* Should make MTC its own module... */
					Mat4MulVecfl(base->object->obmat, vd);
					
					vnc->v= vd;
					for(b=0; b<3; b++) {
						RG.min[b]= MIN2(RG.min[b], vd[b]);
						RG.max[b]= MAX2(RG.max[b], vd[b]);
					}
					vnc++;
				}
				
				if(base->object->totcol==0) {
					if(RG.totmat<MAXMAT) {
						if(noma==NULL) {
							noma= add_material("RadioMat");
							RG.matar[RG.totmat]= noma;
							RG.totmat++;
						}
					}
				}
				else {
					for(a=0; a<base->object->totcol; a++) {
						if(RG.totmat >= MAXMAT) break;

						ma = give_current_material(base->object, a+1);

						if (materialIndex(ma)!=-1) break;

						RG.matar[RG.totmat]= ma;
						RG.totmat++;
					}
				}

				mfdatatot += me->totface;
			}
		}
		base= base->next;
	}

	RG.cent[0]= (RG.min[0]+ RG.max[0])/2;
	RG.cent[1]= (RG.min[1]+ RG.max[1])/2;
	RG.cent[2]= (RG.min[2]+ RG.max[2])/2;
	RG.size[0]= (RG.max[0]- RG.min[0]);
	RG.size[1]= (RG.max[1]- RG.min[1]);
	RG.size[2]= (RG.max[2]- RG.min[2]);
	RG.maxsize= MAX3(RG.size[0],RG.size[1],RG.size[2]);

	RG.mfdata= MEM_callocN(sizeof(CustomData), "radiomfdata");
	RG.mfdatanodes= MEM_mallocN(sizeof(RNode*)*mfdatatot, "radiomfdatanodes");
	RG.mfdatatot= mfdatatot;

	/* make patches */

	RG.totelem= 0;
	RG.totpatch= 0;
	RG.totlamp= 0;
	offs= 0;
	
	base= (G.scene->base.first);
	while(base) {
		if( ((base)->flag & SELECT) && ((base)->lay & G.vd->lay) )  {
			if(base->object->type==OB_MESH) {
				ob= base->object;
				me= ob->data;
				mface= me->mface;
				tface= me->mtface;
				
				index= -1;

				CustomData_merge(&me->fdata, RG.mfdata, CD_MASK_DERIVEDMESH,
					CD_DEFAULT, mfdatatot);
				
				for(a=0; a<me->totface; a++, mface++) {
					tf= tface? tface+a: NULL;
					
					if (tf && (tf->mode & TF_INVISIBLE))
						continue;

					rp= callocPatch();
					BLI_addtail(&(RG.patchbase), rp);
					rp->from= ob;
					
					if(mface->v4) rp->type= 4;
					else rp->type= 3;
					
					rp->first= rn= callocNode();
					
					if(mface->flag & ME_SMOOTH) rp->f1= RAD_NO_SPLIT;
					
					/* temporal: we store the venoco in the node */
					rn->v1= (float *)(RG.verts+mface->v1+offs);
					v1= (RG.verts+mface->v1+offs)->v;
					rn->v2= (float *)(RG.verts+mface->v2+offs);
					v2= (RG.verts+mface->v2+offs)->v;
					rn->v3= (float *)(RG.verts+mface->v3+offs);
					v3= (RG.verts+mface->v3+offs)->v;

					if(mface->v4) {
						rn->v4= (float *)(RG.verts+mface->v4+offs);
						v4= (RG.verts+mface->v4+offs)->v;
					}			
					rn->par= rp;
					rn->f= RAD_PATCH;	/* this node is a Patch */
					rn->type= rp->type;

					if(rn->type==4) {
						rp->area= AreaQ3Dfl(v1, v2, v3, v4);
						CalcNormFloat4(v1, v2, v3, v4, rp->norm);
					}
					else {
						rp->area= AreaT3Dfl(v1, v2, v3);
						CalcNormFloat(v1, v2, v3, rp->norm);
					}

					rn->area= rp->area;

					/* color and emit */
					if(mface->mat_nr != index) {
						index= mface->mat_nr;
						ma= give_current_material(ob, index+1);
						if(ma==0) ma= &defmaterial;
					}
					rp->ref[0]= ma->r;
					rp->ref[1]= ma->g;
					rp->ref[2]= ma->b;

					if(ma->emit) RG.totlamp++;

					rp->emit[0]= rp->emit[1]= rp->emit[2]= ma->emit;
					rp->emit[0]*= rp->ref[0];
					rp->emit[1]*= rp->ref[1];
					rp->emit[2]*= rp->ref[2];

// uncommented, this is not satisfying, but i leave it in code for now (ton)						
//						if(ma->translucency!=0.0) rn->f |= RAD_TWOSIDED;

					nodevert= (VeNoCo **)&(rn->v1);
					for(b=0; b<rp->type; b++) {
						rp->cent[0]+= (*nodevert)->v[0];
						rp->cent[1]+= (*nodevert)->v[1];
						rp->cent[2]+= (*nodevert)->v[2];
						nodevert++;
					}
					rp->cent[0]/= (float)rp->type;
					rp->cent[1]/= (float)rp->type;
					rp->cent[2]/= (float)rp->type;
					
					/* for reconstruction materials */
					rp->matindex= materialIndex(ma);
					if(rp->matindex==-1) rp->matindex= 1;

					/* these RNode's are stored now for later use in rad_addmesh
					   they should not get deleted before that */
					rn->orig= RG.totelem;
					RG.mfdatanodes[RG.totelem]= rn;

					CustomData_copy_data(&me->fdata, RG.mfdata, a, RG.totelem, 1);
					
					RG.totelem++;
					RG.totpatch++;
				}

				offs+= me->totvert;
			}
		}
		base= base->next;
	}
	
	splitconnected();
	setedgepointers();

	makeGlobalElemArray();
	pseudoAmb();
	rad_setlimits();
}

void setparelem(RNode *rn, RPatch *par)
{
	
	if(rn->down1) {
		setparelem(rn->down1, par);
		setparelem(rn->down2, par);
	}
	else {
		rn->par= par;
	}
}

void countelem(RNode *rn)
{

	if(rn->down1) {
		countelem(rn->down1);
		countelem(rn->down2);
	}
	else RG.totelem++;
}

void countglobaldata()
{
	/* counts elements and patches*/
	RPatch *rp;

	RG.totelem= RG.totpatch= 0;

	rp= RG.patchbase.first;
	while(rp) {
		RG.totpatch++;
		countelem(rp->first);
		rp= rp->next;
	}
}

void addelem(RNode ***el, RNode *rn, RPatch *rp)
{
	if(rn->down1) {
		addelem(el, rn->down1, rp);
		addelem(el, rn->down2, rp);
	}
	else {
		rn->par= rp;
		**el= rn;
		(*el)++;
	}
}

void makeGlobalElemArray()
{
	/* always called when # of elements change */
	RPatch *rp;
	RNode **el;

	countglobaldata();

	if(RG.elem) MEM_freeN(RG.elem);
	if(RG.totelem) {
		el= RG.elem= MEM_mallocN(sizeof(void *)*RG.totelem, "makeGlobalElemArray");
	}
	else {
		RG.elem= 0;
		return;
	}

	/* recursive adding elements */
	rp= RG.patchbase.first;
	while(rp) {
		addelem(&el, rp->first, rp);
		rp= rp->next;
	}

	/* formfactor array */
	if(RG.formfactors) MEM_freeN(RG.formfactors);
	if(RG.totelem)
		RG.formfactors= MEM_mallocN(sizeof(float)*RG.totelem, "formfactors");
	else
		RG.formfactors= 0;
}

void splitpatch(RPatch *old)		/* in case of overflow during shoot */
{
	RNode *rn;
	float **fpp;
	RPatch *rp;
	int a;
	
	rn= old->first;
	if(rn->down1==0) return;
	rn= rn->down1;

	old->unshot[0]/=2.0;
	old->unshot[1]/=2.0;
	old->unshot[2]/=2.0;
	setnodeflags(old->first, 2, 0);

	rp= mallocPatch();
	*rp= *old;
	BLI_addhead(&RG.patchbase, rp);
	rp->first= rn;
	rp->area= rn->area;
	rp->cent[0]= rp->cent[1]= rp->cent[2]= 0.0;
	fpp= &(rn->v1);
	for(a=0; a<rp->type; a++) {
		rp->cent[0]+= (*fpp)[0];
		rp->cent[1]+= (*fpp)[1];
		rp->cent[2]+= (*fpp)[2];
		fpp++;
	}
	rp->cent[0]/=(float)rp->type;
	rp->cent[1]/=(float)rp->type;
	rp->cent[2]/=(float)rp->type;
		
	setparelem(rn, rp);

	rn= old->first->down2;

	rp= mallocPatch();
	*rp= *old;
	BLI_addhead(&RG.patchbase, rp);
	rp->first= rn;
	rp->area= rn->area;
	rp->cent[0]= rp->cent[1]= rp->cent[2]= 0.0;
	fpp= &(rn->v1);
	for(a=0; a<rp->type; a++) {
		rp->cent[0]+= (*fpp)[0];
		rp->cent[1]+= (*fpp)[1];
		rp->cent[2]+= (*fpp)[2];
		fpp++;
	}
	rp->cent[0]/=(float)rp->type;
	rp->cent[1]/=(float)rp->type;
	rp->cent[2]/=(float)rp->type;
	
	setparelem(rn, rp);

	BLI_remlink(&RG.patchbase, old);
	freePatch(old);
}


void addpatch(RPatch *old, RNode *rn)
{
	float **fpp;
	RPatch *rp;
	int a;
	
	if(rn->down1) {
		addpatch(old, rn->down1);
		addpatch(old, rn->down2);
	}
	else {
		rp= mallocPatch();
		*rp= *old;
		BLI_addhead(&RG.patchbase, rp);
		rp->first= rn;
		
		rp->area= rn->area;
		rp->cent[0]= rp->cent[1]= rp->cent[2]= 0.0;
		fpp= &(rn->v1);
		for(a=0; a<rp->type; a++) {
			rp->cent[0]+= (*fpp)[0];
			rp->cent[1]+= (*fpp)[1];
			rp->cent[2]+= (*fpp)[2];
			fpp++;
		}
		rp->cent[0]/=(float)rp->type;
		rp->cent[1]/=(float)rp->type;
		rp->cent[2]/=(float)rp->type;
		
		rn->par= rp;
	}
}

void converttopatches()
{
	/* chacks patches list, if node subdivided: new patch */
	RPatch *rp, *next;
	
	rp= RG.patchbase.first;
	while(rp) {
		next= rp->next;
		if(rp->first->down1) {
			addpatch(rp, rp->first);
			BLI_remlink(&RG.patchbase, rp);
			freePatch(rp);
		}
		rp= next;
	}
	
}

void subdiv_elements()
{
	RNode **el, *rn;
	int a, toobig= 1;

	rad_init_energy();
	
	/* first maxsize elements */
	
	while(toobig) {
		toobig= 0;
		
		el= RG.elem;
		for(a=RG.totelem; a>0; a--, el++) {
			rn= *el;
			if( rn->totrad[0]==0.0 && rn->totrad[1]==0.0 && rn->totrad[2]==0.0) {
				if(rn->area>RG.elemmin) {
					subdivideNode(rn, 0);
					if(rn->down1 ) {
						toobig= 1;
						if(rn->down1->area>RG.elemmin)
							subdivideNode( rn->down1, 0);
						if(rn->down2->area>RG.elemmin)
							subdivideNode( rn->down2, 0);
					}
				}
			}
		}
		if(toobig) makeGlobalElemArray();
	}

	el= RG.elem;
	for(a=RG.totelem; a>0; a--, el++) {
		rn= *el;
		if( rn->totrad[0]==0.0 && rn->totrad[1]==0.0 && rn->totrad[2]==0.0) {
			subdivideNode(rn, 0);
			if( rn->down1 ) {
				subdivideNode( rn->down1, 0);
				subdivideNode( rn->down2, 0);
			}
		}
	}
	makeGlobalElemArray();
}

void subdividelamps()
{
	RPatch *rp, *next;
	
	rp= RG.patchbase.first;
	while(rp) {
		next= rp->next;
		if(rp->emit[0]!=0.0 || rp->emit[1]!=0.0 || rp->emit[2]!=0.0) {
			subdivideNode( rp->first, 0);
			if(rp->first->down1) {
				subdivideNode(rp->first->down1, 0);
				subdivideNode(rp->first->down2, 0);
			}
			
			addpatch(rp, rp->first);
			BLI_remlink(&RG.patchbase, rp);
			freePatch(rp);
		}
		rp= next;
	}
	
}

void maxsizePatches()
{
	RPatch *rp;
	int toobig= 1;
	
	while(toobig) {
		toobig= 0;
		rp= RG.patchbase.first;
		while(rp) {
			if(rp->area>RG.patchmax) {
				subdivideNode( rp->first, 0);
				if(rp->first->down1) toobig= 1;
			}
			rp= rp->next;
		}
		
		if(toobig) converttopatches();
	}
	
	/* count lamps */
	rp= RG.patchbase.first;
	RG.totlamp= 0;
	while(rp) {
		if(rp->emit[0]!=0.0 || rp->emit[1]!=0.0 || rp->emit[2]!=0.0) {
			RG.totlamp++;
		}
		rp= rp->next;
	}
	makeGlobalElemArray();
}



