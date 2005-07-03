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

meshtools.c: no editmode, tools operating on meshes

void join_mesh(void);

void fasterdraw(void);
void slowerdraw(void);

void vertexnormals_mesh(Mesh *me, float *extverts);
void sort_faces(void);

*/

#include <stdlib.h>
#include <string.h>
#include <math.h>

#ifdef HAVE_CONFIG_H
#include <config.h>
#endif

#include "MEM_guardedalloc.h"

#include "DNA_mesh_types.h"
#include "DNA_meshdata_types.h"
#include "DNA_object_types.h"
#include "DNA_material_types.h"
#include "DNA_scene_types.h"
#include "DNA_screen_types.h"
#include "DNA_view3d_types.h"

#include "BLI_blenlib.h"
#include "BLI_arithb.h"

#include "BKE_depsgraph.h"
#include "BKE_global.h"
#include "BKE_library.h"
#include "BKE_main.h"
#include "BKE_mesh.h"
#include "BKE_material.h"
#include "BKE_object.h"
#include "BKE_utildefines.h"

#include "BIF_editmesh.h"
#include "BIF_graphics.h"
#include "BIF_mywindow.h"
#include "BIF_screen.h"
#include "BIF_space.h"
#include "BIF_toolbox.h"
#include "BIF_editconstraint.h"

#include "BDR_editobject.h" 
#include "BDR_editface.h" 

#include "mydevice.h"
#include "blendef.h"

#include "BIF_meshtools.h" /* include ourself for prototypes */


/* * ********************** no editmode!!! *********** */


/** tests whether selected mesh objects have tfaces */
static int testSelected_TfaceMesh(void)
{
	Base *base;
	Mesh *me;

	base = FIRSTBASE;
	while (base) {
		if TESTBASE(base) {
			if(base->object->type==OB_MESH) {
				me= base->object->data;
				if (me->tface) 
					return 1;
			}		
		}			
		base= base->next;
	}	
	return 0;
}	

void join_mesh(void)
{
	Base *base, *nextb;
	Object *ob;
	Material **matar, *ma;
	Mesh *me;
	MVert *mvert, *mvertmain;
	MEdge *medge = NULL, *medgemain;
	MFace *mface = NULL, *mfacemain;
	TFace *tface = NULL, *tfacemain;
	unsigned int *mcol=NULL, *mcolmain;
	float imat[4][4], cmat[4][4];
	int a, b, totcol, totedge=0, totvert=0, totface=0, ok=0, vertofs, map[MAXMAT];
	int hasedges=0;
	int	i, j, index, haskey=0, hasdefgroup=0;
	bDeformGroup *dg, *odg;
	MDeformVert *dvert, *dvertmain;
	
	if(G.obedit) return;
	
	ob= OBACT;
	if(!ob || ob->type!=OB_MESH) return;
	
	/* count */
	base= FIRSTBASE;
	while(base) {
		if TESTBASELIB(base) {
			if(base->object->type==OB_MESH) {
				me= base->object->data;
				totvert+= me->totvert;
				totface+= me->totface;
				if(me->medge) hasedges= 1;

				if(base->object == ob) ok= 1;

				if(me->key) {
					haskey= 1;
					break;
				}
			}
		}
		base= base->next;
	}
	
	if(haskey) {
		error("Can't join meshes with vertex keys");
		return;
	}
	/* that way the active object is always selected */ 
	if(ok==0) return;
	
	if(totvert==0 || totvert>MESH_MAX_VERTS) return;
	
	if(okee("Join selected meshes")==0) return;


	/* if needed add edges to other meshes */
	if(hasedges) {
		for(base= FIRSTBASE; base; base= base->next) {
			if TESTBASELIB(base) {
				if(base->object->type==OB_MESH) {
					me= base->object->data;
					if(me->medge==NULL) make_edges(me);
					totedge += me->totedge;
				}
			}
		}
	}
	
	/* new material indices and material array */
	matar= MEM_callocN(sizeof(void *)*MAXMAT, "join_mesh");
	totcol= ob->totcol;
	
	/* obact materials in new main array, is nicer start! */
	for(a=1; a<=ob->totcol; a++) {
		matar[a-1]= give_current_material(ob, a);
		id_us_plus((ID *)matar[a-1]);
		/* increase id->us : will be lowered later */
	}
	
	base= FIRSTBASE;
	while(base) {
		if TESTBASELIB(base) {
			if(ob!=base->object && base->object->type==OB_MESH) {
				me= base->object->data;

				// Join this object's vertex groups to the base one's
				for (dg=base->object->defbase.first; dg; dg=dg->next){
					hasdefgroup= 1;
					
					/* See if this group exists in the object */
					for (odg=ob->defbase.first; odg; odg=odg->next){
						if (!strcmp(odg->name, dg->name)){
							break;
						}
					}
					if (!odg){
						odg = MEM_callocN (sizeof(bDeformGroup), "deformGroup");
						memcpy (odg, dg, sizeof(bDeformGroup));
						BLI_addtail(&ob->defbase, odg);
					}

				}
				if (ob->defbase.first && ob->actdef==0)
					ob->actdef=1;

				if(me->totvert) {
					for(a=1; a<=base->object->totcol; a++) {
						ma= give_current_material(base->object, a);
						if(ma) {
							for(b=0; b<totcol; b++) {
								if(ma == matar[b]) break;
							}
							if(b==totcol) {
								matar[b]= ma;
								ma->id.us++;
								totcol++;
							}
							if(totcol>=MAXMAT-1) break;
						}
					}
				}
			}
			if(totcol>=MAXMAT-1) break;
		}
		base= base->next;
	}

	me= ob->data;
	mvert= mvertmain= MEM_mallocN(totvert*sizeof(MVert), "joinmesh vert");

	if(totedge) medge= medgemain= MEM_callocN(totedge*sizeof(MEdge), "joinmesh edge");
	else medgemain= NULL;
	
	if (totface) mface= mfacemain= MEM_mallocN(totface*sizeof(MFace), "joinmesh face");
	else mfacemain= NULL;

	if(me->mcol) mcol= mcolmain= MEM_callocN(totface*4*sizeof(int), "joinmesh mcol");
	else mcolmain= NULL;

	/* if active object doesn't have Tfaces, but one in the selection does,
	   make TFaces for active, so we don't lose texture information in the
	   join process */
	if(me->tface || testSelected_TfaceMesh()) tface= tfacemain= MEM_callocN(totface*4*sizeof(TFace), "joinmesh4");
	else tfacemain= NULL;

	if(me->dvert || hasdefgroup)
		dvert= dvertmain= MEM_callocN(totvert*sizeof(MDeformVert), "joinmesh5");
	else dvert=dvertmain= NULL;

	vertofs= 0;
	
	/* inverse transorm all selected meshes in this object */
	Mat4Invert(imat, ob->obmat);
	
	base= FIRSTBASE;
	while(base) {
		nextb= base->next;
		if TESTBASELIB(base) {
			if(base->object->type==OB_MESH) {
				
				me= base->object->data;
				
				if(me->totvert) {
					
					memcpy(mvert, me->mvert, me->totvert*sizeof(MVert));
					
					copy_dverts(dvert, me->dvert, me->totvert);

					/* NEW VERSION */
					if (dvertmain){
						for (i=0; i<me->totvert; i++){
							for (j=0; j<dvert[i].totweight; j++){
								//	Find the old vertex group
								odg = BLI_findlink (&base->object->defbase, dvert[i].dw[j].def_nr);
								if(odg) {
									//	Search for a match in the new object
									for (dg=ob->defbase.first, index=0; dg; dg=dg->next, index++){
										if (!strcmp(dg->name, odg->name)){
											dvert[i].dw[j].def_nr = index;
											break;
										}
									}
								}
							}
						}
						dvert+=me->totvert;
					}

					if(base->object != ob) {
						/* watch this: switch matmul order really goes wrong */
						Mat4MulMat4(cmat, base->object->obmat, imat);
						
						a= me->totvert;
						while(a--) {
							Mat4MulVecfl(cmat, mvert->co);
							mvert++;
						}
					}
					else mvert+= me->totvert;
					
					if(mcolmain) {
						if(me->mcol) memcpy(mcol, me->mcol, me->totface*4*4);
						mcol+= 4*me->totface;
					}
				}
				if(me->totface) {
				
					/* make mapping for materials */
					memset(map, 0, 4*MAXMAT);
					for(a=1; a<=base->object->totcol; a++) {
						ma= give_current_material(base->object, a);
						if(ma) {
							for(b=0; b<totcol; b++) {
								if(ma == matar[b]) {
									map[a-1]= b;
									break;
								}
							}
						}
					}

					memcpy(mface, me->mface, me->totface*sizeof(MFace));
					
					a= me->totface;
					while(a--) {
						mface->v1+= vertofs;
						mface->v2+= vertofs;
						if(mface->v3) mface->v3+= vertofs;
						if(mface->v4) mface->v4+= vertofs;
						
						mface->mat_nr= map[(int)mface->mat_nr];
						
						mface++;
					}
					
					if(tfacemain) {
						if(me->tface) {
							memcpy(tface, me->tface, me->totface*sizeof(TFace));
							tface+= me->totface;
						}
						else {
							for(a=0; a<me->totface; a++, tface++) {
								default_tface(tface);
							}
						}
					}
					
				}
				
				if(me->totedge) {
					memcpy(medge, me->medge, me->totedge*sizeof(MEdge));
					
					a= me->totedge;
					while(a--) {
						medge->v1+= vertofs;
						medge->v2+= vertofs;
						medge++;
					}
				}
				
				vertofs+= me->totvert;
				
				if(base->object!=ob) {
					free_and_unlink_base(base);
				}
			}
		}
		base= nextb;
	}
	
	me= ob->data;
	
	if(me->mvert) MEM_freeN(me->mvert);
	me->mvert= mvertmain;

	if(me->medge) MEM_freeN(me->medge);
	me->medge= medgemain;

	if(me->mface) MEM_freeN(me->mface);
	me->mface= mfacemain;

	if(me->dvert) free_dverts(me->dvert, me->totvert);
	me->dvert = dvertmain;

	if(me->mcol) MEM_freeN(me->mcol);
	me->mcol= (MCol *)mcolmain;
	
	if(me->tface) MEM_freeN(me->tface);
	me->tface= tfacemain;
	
	me->totvert= totvert;
	me->totedge= totedge;
	me->totface= totface;
	
	/* old material array */
	for(a=1; a<=ob->totcol; a++) {
		ma= ob->mat[a-1];
		if(ma) ma->id.us--;
	}
	for(a=1; a<=me->totcol; a++) {
		ma= me->mat[a-1];
		if(ma) ma->id.us--;
	}
	if(ob->mat) MEM_freeN(ob->mat);
	if(me->mat) MEM_freeN(me->mat);
	ob->mat= me->mat= 0;
	
	if(totcol) {
		me->mat= matar;
		ob->mat= MEM_callocN(sizeof(void *)*totcol, "join obmatar");
	}
	else MEM_freeN(matar);
	
	ob->totcol= me->totcol= totcol;
	ob->colbits= 0;
	
	/* other mesh users */
	test_object_materials((ID *)me);
	
	enter_editmode();
	exit_editmode(1);	// freedata, but no undo
	
	allqueue(REDRAWVIEW3D, 0);
	allqueue(REDRAWBUTSSHADING, 0);
	DAG_scene_sort(G.scene);
	DAG_object_flush_update(G.scene, ob, OB_RECALC_DATA);

	BIF_undo_push("Join Mesh");
}


void fasterdraw(void)
{
	Base *base;
	Mesh *me;
	MFace *mface;
	int toggle, a;

	if(G.obedit) return;

	/* reset flags */
	me= G.main->mesh.first;
	while(me) {
		me->flag &= ~ME_ISDONE;
		me= me->id.next;
	}

	base= FIRSTBASE;
	while(base) {
		if( TESTBASELIB(base) && (base->object->type==OB_MESH)) {
			me= base->object->data;
			if(me->id.lib==0 && (me->flag & ME_ISDONE)==0) {
				me->flag |= ME_ISDONE;
				mface= me->mface;
				toggle= 0;
				for(a=0; a<me->totface; a++) {
					if( (mface->edcode & ME_V1V2) && ( (toggle++) & 1) ) {
						mface->edcode-= ME_V1V2;
					}
					if( (mface->edcode & ME_V2V3) && ( (toggle++) & 1)) {
						mface->edcode-= ME_V2V3;
					}
					if( (mface->edcode & ME_V3V1) && ( (toggle++) & 1)) {
						mface->edcode-= ME_V3V1;
					}
					if( (mface->edcode & ME_V4V1) && ( (toggle++) & 1)) {
						mface->edcode-= ME_V4V1;
					}
					if( (mface->edcode & ME_V3V4) && ( (toggle++) & 1)) {
						mface->edcode-= ME_V3V4;
					}
					mface++;
				}
			}
		}
		base= base->next;
	}

	/* important?: reset flags again */
	me= G.main->mesh.first;
	while(me) {
		me->flag &= ~ME_ISDONE;
		me= me->id.next;
	}

	allqueue(REDRAWVIEW3D, 0);
}

void slowerdraw(void)		/* reset fasterdraw */
{
	Base *base;
	Mesh *me;
	MFace *mface;
	int a;

	if(G.obedit) return;

	base= FIRSTBASE;
	while(base) {
		if( TESTBASELIB(base) && (base->object->type==OB_MESH)) {
			me= base->object->data;
			if(me->id.lib==0) {
				
				mface= me->mface;
				
				for(a=0; a<me->totface; a++) {
				
					mface->edcode |= ME_V1V2|ME_V2V3;
					mface++;
				}
			}
		}
		base= base->next;
	}

	allqueue(REDRAWVIEW3D, 0);
}

/* ***************** */

/* this one for NOT in editmode 

(only used by external modules, that is, until now by the 
python NMesh module) 

TODO: Probably it's better to convert the mesh into a EditMesh, call
vertexnormals() and convert it back to a Mesh again.

*/

static int contrpuntnorm(float *n, float *puno)  /* dutch: check vertex normal */
{
	float inp;

	inp= n[0]*puno[0]+n[1]*puno[1]+n[2]*puno[2];

	/* angles 90 degrees: dont flip */
	if(inp> -0.000001) return 0;

	return 1;
}

void vertexnormals_mesh(Mesh *me, float *extverts)
{
	MVert *mvert;
	MFace *mface;
	float n1[3], n2[3], n3[3], n4[3], co[4], fac1, fac2, fac3, fac4, *temp;
	float *f1, *f2, *f3, *f4, xn, yn, zn, *normals;
	float *v1, *v2, *v3, *v4, len, vnor[3];
	int a, testflip;

	if(me->totvert==0) return;

	testflip= (me->flag & ME_NOPUNOFLIP)==0;
	if((me->flag & ME_TWOSIDED)==0) testflip= 0;	/* large angles */
	
	if(me->totface==0) {
		/* fake vertex normals for 'halopuno' (render option) */
		mvert= me->mvert;
		for(a=0; a<me->totvert; a++, mvert++) {
			VECCOPY(n1, mvert->co);
			Normalise(n1);
			mvert->no[0]= 32767.0*n1[0];
			mvert->no[1]= 32767.0*n1[1];
			mvert->no[2]= 32767.0*n1[2];
		}
		return;
	}

	normals= MEM_callocN(me->totvert*3*sizeof(float), "normals");
	
	/* calculate cosine angles, and add to vertex normal */
	mface= me->mface;
	mvert= me->mvert;
	for(a=0; a<me->totface; a++, mface++) {
		
		if(mface->v3==0) continue;
		
		if(extverts) {
			v1= extverts+3*mface->v1;
			v2= extverts+3*mface->v2;
			v3= extverts+3*mface->v3;
			v4= extverts+3*mface->v4;
		}
		else {		
			v1= (mvert+mface->v1)->co;
			v2= (mvert+mface->v2)->co;
			v3= (mvert+mface->v3)->co;
			v4= (mvert+mface->v4)->co;
		}
		
		VecSubf(n1, v2, v1);
		VecSubf(n2, v3, v2);
		Normalise(n1);
		Normalise(n2);

		if(mface->v4==0) {
			VecSubf(n3, v1, v3);
			Normalise(n3);
			
			co[0]= saacos(-n3[0]*n1[0]-n3[1]*n1[1]-n3[2]*n1[2]);
			co[1]= saacos(-n1[0]*n2[0]-n1[1]*n2[1]-n1[2]*n2[2]);
			co[2]= saacos(-n2[0]*n3[0]-n2[1]*n3[1]-n2[2]*n3[2]);
			
		}
		else {
			VecSubf(n3, v4, v3);
			VecSubf(n4, v1, v4);
			Normalise(n3);
			Normalise(n4);

			co[0]= saacos(-n4[0]*n1[0]-n4[1]*n1[1]-n4[2]*n1[2]);
			co[1]= saacos(-n1[0]*n2[0]-n1[1]*n2[1]-n1[2]*n2[2]);
			co[2]= saacos(-n2[0]*n3[0]-n2[1]*n3[1]-n2[2]*n3[2]);
			co[3]= saacos(-n3[0]*n4[0]-n3[1]*n4[1]-n3[2]*n4[2]);
		}
		
		CalcNormFloat(v1, v2, v3, vnor);
		
		temp= normals+3*mface->v1;
		if(testflip && contrpuntnorm(vnor, temp) ) co[0]= -co[0];
		temp[0]+= co[0]*vnor[0];
		temp[1]+= co[0]*vnor[1];
		temp[2]+= co[0]*vnor[2];
		
		temp= normals+3*mface->v2;
		if(testflip && contrpuntnorm(vnor, temp) ) co[1]= -co[1];
		temp[0]+= co[1]*vnor[0];
		temp[1]+= co[1]*vnor[1];
		temp[2]+= co[1]*vnor[2];
		
		temp= normals+3*mface->v3;
		if(testflip && contrpuntnorm(vnor, temp) ) co[2]= -co[2];
		temp[0]+= co[2]*vnor[0];
		temp[1]+= co[2]*vnor[1];
		temp[2]+= co[2]*vnor[2];
		
		if(mface->v4) {
			temp= normals+3*mface->v4;
			if(testflip && contrpuntnorm(vnor, temp) ) co[3]= -co[3];
			temp[0]+= co[3]*vnor[0];
			temp[1]+= co[3]*vnor[1];
			temp[2]+= co[3]*vnor[2];
		}
	}

	/* normalize vertex normals */
	mvert= me->mvert;
	for(a=0; a<me->totvert; a++, mvert++) {
		len= Normalise(normals+3*a);
		if(len!=0.0) {
			VECCOPY(n1, normals+3*a);
			Normalise(n1);

			mvert->no[0]= 32767.0*n1[0];
			mvert->no[1]= 32767.0*n1[1];
			mvert->no[2]= 32767.0*n1[2];
		}
	}
	
	/* vertex normal flipping flags, for during render */
	mface= me->mface;
	mvert= me->mvert;
	for(a=0; a<me->totface; a++, mface++) {
		mface->puno=0;			
		
		if(mface->v3==0) continue;
		
		if(extverts) {
			v1= extverts+3*mface->v1;
			v2= extverts+3*mface->v2;
			v3= extverts+3*mface->v3;
		}
		else {		
			v1= (mvert+mface->v1)->co;
			v2= (mvert+mface->v2)->co;
			v3= (mvert+mface->v3)->co;
		}

		CalcNormFloat(v1, v2, v3, vnor);

		if(testflip) {
			f1= normals + 3*mface->v1;
			f2= normals + 3*mface->v2;
			f3= normals + 3*mface->v3;

			fac1= vnor[0]*f1[0] + vnor[1]*f1[1] + vnor[2]*f1[2];
			if(fac1<0.0) {
				mface->puno = ME_FLIPV1;
			}
			fac2= vnor[0]*f2[0] + vnor[1]*f2[1] + vnor[2]*f2[2];
			if(fac2<0.0) {
				mface->puno += ME_FLIPV2;
			}
			fac3= vnor[0]*f3[0] + vnor[1]*f3[1] + vnor[2]*f3[2];
			if(fac3<0.0) {
				mface->puno += ME_FLIPV3;
			}
			if(mface->v4) {
				f4= normals + 3*mface->v4;
				fac4= vnor[0]*f4[0] + vnor[1]*f4[1] + vnor[2]*f4[2];
				if(fac4<0.0) {
					mface->puno += ME_FLIPV4;
				}
			}
		}
		/* proj for cubemap! */
		xn= fabs(vnor[0]);
		yn= fabs(vnor[1]);
		zn= fabs(vnor[2]);
		
		if(zn>xn && zn>yn) mface->puno += ME_PROJXY;
		else if(yn>xn && yn>zn) mface->puno += ME_PROJXZ;
		else mface->puno += ME_PROJYZ;
		
	}
	
	MEM_freeN(normals);
}



/* ********************** SORT FACES ******************* */

static void permutate(void *list, int num, int size, int *index)
{
	void *buf;
	int len;
	int i;

	len = num * size;

	buf = MEM_mallocN(len, "permutate");
	memcpy(buf, list, len);
	
	for (i = 0; i < num; i++) {
		memcpy((char *)list + (i * size), (char *)buf + (index[i] * size), size);
	}
	MEM_freeN(buf);
}

static MVert *mvertbase;
static MFace *mfacebase;

static int verg_mface(const void *v1, const void *v2)
{
	MFace *x1, *x2;

	MVert *ve1, *ve2;
	int i1, i2;

	i1 = ((int *) v1)[0];
	i2 = ((int *) v2)[0];
	
	x1 = mfacebase + i1;
	x2 = mfacebase + i2;

	ve1= mvertbase+x1->v1;
	ve2= mvertbase+x2->v1;
	
	if( ve1->co[2] > ve2->co[2] ) return 1;
	else if( ve1->co[2] < ve2->co[2]) return -1;
	return 0;
}


void sort_faces(void)
{
	Object *ob= OBACT;
	Mesh *me;
	
	int i, *index;
	
	if(ob==0) return;
	if(G.obedit) return;
	if(ob->type!=OB_MESH) return;
	
	if(okee("Sort faces in Z axis")==0) return;
	me= ob->data;
	if(me->totface==0) return;

/*	create index list */
	index = (int *) MEM_mallocN(sizeof(int) * me->totface, "sort faces");
	for (i = 0; i < me->totface; i++) {
		index[i] = i;
	}
	mvertbase= me->mvert;
	mfacebase = me->mface;

/* sort index list instead of faces itself 
   and apply this permutation to the face list plus
   to the texture faces */
	qsort(index, me->totface, sizeof(int), verg_mface);

	permutate(mfacebase, me->totface, sizeof(MFace), index);
	if (me->tface) 
		permutate(me->tface, me->totface, sizeof(TFace), index);

	MEM_freeN(index);

	allqueue(REDRAWVIEW3D, 0);
	DAG_object_flush_update(G.scene, ob, OB_RECALC_DATA);
}


