/*
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
* along with this program; if not, write to the Free Software  Foundation,
* Inc., 51 Franklin Street, Fifth Floor, Boston, MA 02110-1301, USA.
*
* The Original Code is Copyright (C) 2005 by the Blender Foundation.
* All rights reserved.
*
* Contributor(s): Daniel Dunbar
*                 Ton Roosendaal,
*                 Ben Batt,
*                 Brecht Van Lommel,
*                 Campbell Barton
*
* ***** END GPL LICENSE BLOCK *****
*
*/

#include "stddef.h"
#include "string.h"
#include "stdarg.h"
#include "math.h"
#include "float.h"

#include "BLI_kdtree.h"
#include "BLI_rand.h"
#include "BLI_math.h"
#include "BLI_edgehash.h"

#include "MEM_guardedalloc.h"

#include "DNA_meshdata_types.h"
#include "DNA_scene_types.h"
#include "DNA_curve_types.h"
#include "DNA_key_types.h"
#include "DNA_material_types.h"
#include "DNA_object_fluidsim.h"


#include "BKE_action.h"
#include "BKE_cdderivedmesh.h"
#include "BKE_displist.h"
#include "BKE_fluidsim.h"
#include "BKE_multires.h"
#include "BKE_key.h"
#include "BKE_lattice.h"
#include "BKE_material.h"
#include "BKE_mesh.h"
#include "BKE_modifier.h"
#include "BKE_object.h"
#include "BKE_paint.h"
#include "BKE_particle.h"
#include "BKE_pointcache.h"
#include "BKE_scene.h"
#include "BKE_smoke.h"
#include "BKE_softbody.h"
#include "BKE_subsurf.h"
#include "BKE_texture.h"
#include "BKE_utildefines.h"

#include "depsgraph_private.h"
#include "BKE_deform.h"
#include "BKE_shrinkwrap.h"

#include "LOD_decimation.h"

#include "CCGSubSurf.h"

#include "RE_shader_ext.h"

#include "MOD_modifiertypes.h"


static void initData(ModifierData *md)
{
	ExplodeModifierData *emd= (ExplodeModifierData*) md;

	emd->facepa=0;
	emd->flag |= eExplodeFlag_Unborn+eExplodeFlag_Alive+eExplodeFlag_Dead;
}
static void freeData(ModifierData *md)
{
	ExplodeModifierData *emd= (ExplodeModifierData*) md;
	
	if(emd->facepa) MEM_freeN(emd->facepa);
}
static void copyData(ModifierData *md, ModifierData *target)
{
	ExplodeModifierData *emd= (ExplodeModifierData*) md;
	ExplodeModifierData *temd= (ExplodeModifierData*) target;

	temd->facepa = 0;
	temd->flag = emd->flag;
	temd->protect = emd->protect;
	temd->vgroup = emd->vgroup;
}
static int dependsOnTime(ModifierData *md) 
{
	return 1;
}
static CustomDataMask requiredDataMask(Object *ob, ModifierData *md)
{
	ExplodeModifierData *emd= (ExplodeModifierData*) md;
	CustomDataMask dataMask = 0;

	if(emd->vgroup)
		dataMask |= (1 << CD_MDEFORMVERT);

	return dataMask;
}

static void createFacepa(ExplodeModifierData *emd,
					 ParticleSystemModifierData *psmd,
	  Object *ob, DerivedMesh *dm)
{
	ParticleSystem *psys=psmd->psys;
	MFace *fa=0, *mface=0;
	MVert *mvert = 0;
	ParticleData *pa;
	KDTree *tree;
	float center[3], co[3];
	int *facepa=0,*vertpa=0,totvert=0,totface=0,totpart=0;
	int i,p,v1,v2,v3,v4=0;

	mvert = dm->getVertArray(dm);
	mface = dm->getFaceArray(dm);
	totface= dm->getNumFaces(dm);
	totvert= dm->getNumVerts(dm);
	totpart= psmd->psys->totpart;

	BLI_srandom(psys->seed);

	if(emd->facepa)
		MEM_freeN(emd->facepa);

	facepa = emd->facepa = MEM_callocN(sizeof(int)*totface, "explode_facepa");

	vertpa = MEM_callocN(sizeof(int)*totvert, "explode_vertpa");

	/* initialize all faces & verts to no particle */
	for(i=0; i<totface; i++)
		facepa[i]=totpart;

	for (i=0; i<totvert; i++)
		vertpa[i]=totpart;

	/* set protected verts */
	if(emd->vgroup){
		MDeformVert *dvert = dm->getVertDataArray(dm, CD_MDEFORMVERT);
		float val;
		if(dvert){
			int defgrp_index= emd->vgroup-1;
			for(i=0; i<totvert; i++, dvert++){
				val = BLI_frand();
				val = (1.0f-emd->protect)*val + emd->protect*0.5f;
				if(val < defvert_find_weight(dvert, defgrp_index))
					vertpa[i] = -1;
			}
		}
	}

	/* make tree of emitter locations */
	tree=BLI_kdtree_new(totpart);
	for(p=0,pa=psys->particles; p<totpart; p++,pa++){
		psys_particle_on_dm(psmd->dm,psys->part->from,pa->num,pa->num_dmcache,pa->fuv,pa->foffset,co,0,0,0,0,0);
		BLI_kdtree_insert(tree, p, co, NULL);
	}
	BLI_kdtree_balance(tree);

	/* set face-particle-indexes to nearest particle to face center */
	for(i=0,fa=mface; i<totface; i++,fa++){
		add_v3_v3v3(center,mvert[fa->v1].co,mvert[fa->v2].co);
		add_v3_v3v3(center,center,mvert[fa->v3].co);
		if(fa->v4){
			add_v3_v3v3(center,center,mvert[fa->v4].co);
			mul_v3_fl(center,0.25);
		}
		else
			mul_v3_fl(center,0.3333f);

		p= BLI_kdtree_find_nearest(tree,center,NULL,NULL);

		v1=vertpa[fa->v1];
		v2=vertpa[fa->v2];
		v3=vertpa[fa->v3];
		if(fa->v4)
			v4=vertpa[fa->v4];

		if(v1>=0 && v2>=0 && v3>=0 && (fa->v4==0 || v4>=0))
			facepa[i]=p;

		if(v1>=0) vertpa[fa->v1]=p;
		if(v2>=0) vertpa[fa->v2]=p;
		if(v3>=0) vertpa[fa->v3]=p;
		if(fa->v4 && v4>=0) vertpa[fa->v4]=p;
	}

	if(vertpa) MEM_freeN(vertpa);
	BLI_kdtree_free(tree);
}

static int edgesplit_get(EdgeHash *edgehash, int v1, int v2)
{
	return GET_INT_FROM_POINTER(BLI_edgehash_lookup(edgehash, v1, v2));
}

static DerivedMesh * splitEdges(ExplodeModifierData *emd, DerivedMesh *dm){
	DerivedMesh *splitdm;
	MFace *mf=0,*df1=0,*df2=0,*df3=0;
	MFace *mface=CDDM_get_faces(dm);
	MVert *dupve, *mv;
	EdgeHash *edgehash;
	EdgeHashIterator *ehi;
	int totvert=dm->getNumVerts(dm);
	int totface=dm->getNumFaces(dm);

	int *facesplit = MEM_callocN(sizeof(int)*totface,"explode_facesplit");
	int *vertpa = MEM_callocN(sizeof(int)*totvert,"explode_vertpa2");
	int *facepa = emd->facepa;
	int *fs, totesplit=0,totfsplit=0,totin=0,curdupvert=0,curdupface=0,curdupin=0;
	int i,j,v1,v2,v3,v4,esplit;

	edgehash= BLI_edgehash_new();

	/* recreate vertpa from facepa calculation */
	for (i=0,mf=mface; i<totface; i++,mf++) {
		vertpa[mf->v1]=facepa[i];
		vertpa[mf->v2]=facepa[i];
		vertpa[mf->v3]=facepa[i];
		if(mf->v4)
			vertpa[mf->v4]=facepa[i];
	}

	/* mark edges for splitting and how to split faces */
	for (i=0,mf=mface,fs=facesplit; i<totface; i++,mf++,fs++) {
		if(mf->v4){
			v1=vertpa[mf->v1];
			v2=vertpa[mf->v2];
			v3=vertpa[mf->v3];
			v4=vertpa[mf->v4];

			if(v1!=v2){
				BLI_edgehash_insert(edgehash, mf->v1, mf->v2, NULL);
				(*fs)++;
			}

			if(v2!=v3){
				BLI_edgehash_insert(edgehash, mf->v2, mf->v3, NULL);
				(*fs)++;
			}

			if(v3!=v4){
				BLI_edgehash_insert(edgehash, mf->v3, mf->v4, NULL);
				(*fs)++;
			}

			if(v1!=v4){
				BLI_edgehash_insert(edgehash, mf->v1, mf->v4, NULL);
				(*fs)++;
			}

			if(*fs==2){
				if((v1==v2 && v3==v4) || (v1==v4 && v2==v3))
					*fs=1;
				else if(v1!=v2){
					if(v1!=v4)
						BLI_edgehash_insert(edgehash, mf->v2, mf->v3, NULL);
					else
						BLI_edgehash_insert(edgehash, mf->v3, mf->v4, NULL);
				}
				else{ 
					if(v1!=v4)
						BLI_edgehash_insert(edgehash, mf->v1, mf->v2, NULL);
					else
						BLI_edgehash_insert(edgehash, mf->v1, mf->v4, NULL);
				}
			}
		}
	}

	/* count splits & reindex */
	ehi= BLI_edgehashIterator_new(edgehash);
	totesplit=totvert;
	for(; !BLI_edgehashIterator_isDone(ehi); BLI_edgehashIterator_step(ehi)) {
		BLI_edgehashIterator_setValue(ehi, SET_INT_IN_POINTER(totesplit));
		totesplit++;
	}
	BLI_edgehashIterator_free(ehi);

	/* count new faces due to splitting */
	for(i=0,fs=facesplit; i<totface; i++,fs++){
		if(*fs==1)
			totfsplit+=1;
		else if(*fs==2)
			totfsplit+=2;
		else if(*fs==3)
			totfsplit+=3;
		else if(*fs==4){
			totfsplit+=3;

			mf=dm->getFaceData(dm,i,CD_MFACE);//CDDM_get_face(dm,i);

			if(vertpa[mf->v1]!=vertpa[mf->v2] && vertpa[mf->v2]!=vertpa[mf->v3])
				totin++;
		}
	}
	
	splitdm= CDDM_from_template(dm, totesplit+totin, dm->getNumEdges(dm),totface+totfsplit);

	/* copy new faces & verts (is it really this painful with custom data??) */
	for(i=0; i<totvert; i++){
		MVert source;
		MVert *dest;
		dm->getVert(dm, i, &source);
		dest = CDDM_get_vert(splitdm, i);

		DM_copy_vert_data(dm, splitdm, i, i, 1);
		*dest = source;
	}
	for(i=0; i<totface; i++){
		MFace source;
		MFace *dest;
		dm->getFace(dm, i, &source);
		dest = CDDM_get_face(splitdm, i);

		DM_copy_face_data(dm, splitdm, i, i, 1);
		*dest = source;
	}

	/* override original facepa (original pointer is saved in caller function) */
	facepa= MEM_callocN(sizeof(int)*(totface+totfsplit),"explode_facepa");
	memcpy(facepa,emd->facepa,totface*sizeof(int));
	emd->facepa=facepa;

	/* create new verts */
	curdupvert=totvert;
	ehi= BLI_edgehashIterator_new(edgehash);
	for(; !BLI_edgehashIterator_isDone(ehi); BLI_edgehashIterator_step(ehi)) {
		BLI_edgehashIterator_getKey(ehi, &i, &j);
		esplit= GET_INT_FROM_POINTER(BLI_edgehashIterator_getValue(ehi));
		mv=CDDM_get_vert(splitdm,j);
		dupve=CDDM_get_vert(splitdm,esplit);

		DM_copy_vert_data(splitdm,splitdm,j,esplit,1);

		*dupve=*mv;

		mv=CDDM_get_vert(splitdm,i);

		add_v3_v3(dupve->co, mv->co);
		mul_v3_fl(dupve->co, 0.5f);
	}
	BLI_edgehashIterator_free(ehi);

	/* create new faces */
	curdupface=totface;
	curdupin=totesplit;
	for(i=0,fs=facesplit; i<totface; i++,fs++){
		if(*fs){
			mf=CDDM_get_face(splitdm,i);

			v1=vertpa[mf->v1];
			v2=vertpa[mf->v2];
			v3=vertpa[mf->v3];
			v4=vertpa[mf->v4];
			/* ouch! creating new faces & remapping them to new verts is no fun */
			if(*fs==1){
				df1=CDDM_get_face(splitdm,curdupface);
				DM_copy_face_data(splitdm,splitdm,i,curdupface,1);
				*df1=*mf;
				curdupface++;
				
				if(v1==v2){
					df1->v1=edgesplit_get(edgehash, mf->v1, mf->v4);
					df1->v2=edgesplit_get(edgehash, mf->v2, mf->v3);
					mf->v3=df1->v2;
					mf->v4=df1->v1;
				}
				else{
					df1->v1=edgesplit_get(edgehash, mf->v1, mf->v2);
					df1->v4=edgesplit_get(edgehash, mf->v3, mf->v4);
					mf->v2=df1->v1;
					mf->v3=df1->v4;
				}

				facepa[i]=v1;
				facepa[curdupface-1]=v3;

				test_index_face(df1, &splitdm->faceData, curdupface, (df1->v4 ? 4 : 3));
			}
			if(*fs==2){
				df1=CDDM_get_face(splitdm,curdupface);
				DM_copy_face_data(splitdm,splitdm,i,curdupface,1);
				*df1=*mf;
				curdupface++;

				df2=CDDM_get_face(splitdm,curdupface);
				DM_copy_face_data(splitdm,splitdm,i,curdupface,1);
				*df2=*mf;
				curdupface++;

				if(v1!=v2){
					if(v1!=v4){
						df1->v1=edgesplit_get(edgehash, mf->v1, mf->v4);
						df1->v2=edgesplit_get(edgehash, mf->v1, mf->v2);
						df2->v1=df1->v3=mf->v2;
						df2->v3=df1->v4=mf->v4;
						df2->v2=mf->v3;

						mf->v2=df1->v2;
						mf->v3=df1->v1;

						df2->v4=mf->v4=0;

						facepa[i]=v1;
					}
					else{
						df1->v2=edgesplit_get(edgehash, mf->v1, mf->v2);
						df1->v3=edgesplit_get(edgehash, mf->v2, mf->v3);
						df1->v4=mf->v3;
						df2->v2=mf->v3;
						df2->v3=mf->v4;

						mf->v1=df1->v2;
						mf->v3=df1->v3;

						df2->v4=mf->v4=0;

						facepa[i]=v2;
					}
					facepa[curdupface-1]=facepa[curdupface-2]=v3;
				}
				else{
					if(v1!=v4){
						df1->v3=edgesplit_get(edgehash, mf->v3, mf->v4);
						df1->v4=edgesplit_get(edgehash, mf->v1, mf->v4);
						df1->v2=mf->v3;

						mf->v1=df1->v4;
						mf->v2=df1->v3;
						mf->v3=mf->v4;

						df2->v4=mf->v4=0;

						facepa[i]=v4;
					}
					else{
						df1->v3=edgesplit_get(edgehash, mf->v2, mf->v3);
						df1->v4=edgesplit_get(edgehash, mf->v3, mf->v4);
						df1->v1=mf->v4;
						df1->v2=mf->v2;
						df2->v3=mf->v4;

						mf->v1=df1->v4;
						mf->v2=df1->v3;

						df2->v4=mf->v4=0;

						facepa[i]=v3;
					}

					facepa[curdupface-1]=facepa[curdupface-2]=v1;
				}

				test_index_face(df1, &splitdm->faceData, curdupface-2, (df1->v4 ? 4 : 3));
				test_index_face(df1, &splitdm->faceData, curdupface-1, (df1->v4 ? 4 : 3));
			}
			else if(*fs==3){
				df1=CDDM_get_face(splitdm,curdupface);
				DM_copy_face_data(splitdm,splitdm,i,curdupface,1);
				*df1=*mf;
				curdupface++;

				df2=CDDM_get_face(splitdm,curdupface);
				DM_copy_face_data(splitdm,splitdm,i,curdupface,1);
				*df2=*mf;
				curdupface++;

				df3=CDDM_get_face(splitdm,curdupface);
				DM_copy_face_data(splitdm,splitdm,i,curdupface,1);
				*df3=*mf;
				curdupface++;

				if(v1==v2){
					df2->v1=df1->v1=edgesplit_get(edgehash, mf->v1, mf->v4);
					df3->v1=df1->v2=edgesplit_get(edgehash, mf->v2, mf->v3);
					df3->v3=df2->v2=df1->v3=edgesplit_get(edgehash, mf->v3, mf->v4);
					df3->v2=mf->v3;
					df2->v3=mf->v4;
					df1->v4=df2->v4=df3->v4=0;

					mf->v3=df1->v2;
					mf->v4=df1->v1;

					facepa[i]=facepa[curdupface-3]=v1;
					facepa[curdupface-1]=v3;
					facepa[curdupface-2]=v4;
				}
				else if(v2==v3){
					df3->v1=df2->v3=df1->v1=edgesplit_get(edgehash, mf->v1, mf->v4);
					df2->v2=df1->v2=edgesplit_get(edgehash, mf->v1, mf->v2);
					df3->v2=df1->v3=edgesplit_get(edgehash, mf->v3, mf->v4);

					df3->v3=mf->v4;
					df2->v1=mf->v1;
					df1->v4=df2->v4=df3->v4=0;

					mf->v1=df1->v2;
					mf->v4=df1->v3;

					facepa[i]=facepa[curdupface-3]=v2;
					facepa[curdupface-1]=v4;
					facepa[curdupface-2]=v1;
				}
				else if(v3==v4){
					df3->v2=df2->v1=df1->v1=edgesplit_get(edgehash, mf->v1, mf->v2);
					df2->v3=df1->v2=edgesplit_get(edgehash, mf->v2, mf->v3);
					df3->v3=df1->v3=edgesplit_get(edgehash, mf->v1, mf->v4);

					df3->v1=mf->v1;
					df2->v2=mf->v2;
					df1->v4=df2->v4=df3->v4=0;

					mf->v1=df1->v3;
					mf->v2=df1->v2;

					facepa[i]=facepa[curdupface-3]=v3;
					facepa[curdupface-1]=v1;
					facepa[curdupface-2]=v2;
				}
				else{
					df3->v1=df1->v1=edgesplit_get(edgehash, mf->v1, mf->v2);
					df3->v3=df2->v1=df1->v2=edgesplit_get(edgehash, mf->v2, mf->v3);
					df2->v3=df1->v3=edgesplit_get(edgehash, mf->v3, mf->v4);

					df3->v2=mf->v2;
					df2->v2=mf->v3;
					df1->v4=df2->v4=df3->v4=0;

					mf->v2=df1->v1;
					mf->v3=df1->v3;

					facepa[i]=facepa[curdupface-3]=v1;
					facepa[curdupface-1]=v2;
					facepa[curdupface-2]=v3;
				}

				test_index_face(df1, &splitdm->faceData, curdupface-3, (df1->v4 ? 4 : 3));
				test_index_face(df1, &splitdm->faceData, curdupface-2, (df1->v4 ? 4 : 3));
				test_index_face(df1, &splitdm->faceData, curdupface-1, (df1->v4 ? 4 : 3));
			}
			else if(*fs==4){
				if(v1!=v2 && v2!=v3){

					/* set new vert to face center */
					mv=CDDM_get_vert(splitdm,mf->v1);
					dupve=CDDM_get_vert(splitdm,curdupin);
					DM_copy_vert_data(splitdm,splitdm,mf->v1,curdupin,1);
					*dupve=*mv;

					mv=CDDM_get_vert(splitdm,mf->v2);
					VECADD(dupve->co,dupve->co,mv->co);
					mv=CDDM_get_vert(splitdm,mf->v3);
					VECADD(dupve->co,dupve->co,mv->co);
					mv=CDDM_get_vert(splitdm,mf->v4);
					VECADD(dupve->co,dupve->co,mv->co);
					mul_v3_fl(dupve->co,0.25);


					df1=CDDM_get_face(splitdm,curdupface);
					DM_copy_face_data(splitdm,splitdm,i,curdupface,1);
					*df1=*mf;
					curdupface++;

					df2=CDDM_get_face(splitdm,curdupface);
					DM_copy_face_data(splitdm,splitdm,i,curdupface,1);
					*df2=*mf;
					curdupface++;

					df3=CDDM_get_face(splitdm,curdupface);
					DM_copy_face_data(splitdm,splitdm,i,curdupface,1);
					*df3=*mf;
					curdupface++;

					df1->v1=edgesplit_get(edgehash, mf->v1, mf->v2);
					df3->v2=df1->v3=edgesplit_get(edgehash, mf->v2, mf->v3);

					df2->v1=edgesplit_get(edgehash, mf->v1, mf->v4);
					df3->v4=df2->v3=edgesplit_get(edgehash, mf->v3, mf->v4);

					df3->v1=df2->v2=df1->v4=curdupin;

					mf->v2=df1->v1;
					mf->v3=curdupin;
					mf->v4=df2->v1;

					curdupin++;

					facepa[i]=v1;
					facepa[curdupface-3]=v2;
					facepa[curdupface-2]=v3;
					facepa[curdupface-1]=v4;

					test_index_face(df1, &splitdm->faceData, curdupface-3, (df1->v4 ? 4 : 3));

					test_index_face(df1, &splitdm->faceData, curdupface-2, (df1->v4 ? 4 : 3));
					test_index_face(df1, &splitdm->faceData, curdupface-1, (df1->v4 ? 4 : 3));
				}
				else{
					df1=CDDM_get_face(splitdm,curdupface);
					DM_copy_face_data(splitdm,splitdm,i,curdupface,1);
					*df1=*mf;
					curdupface++;

					df2=CDDM_get_face(splitdm,curdupface);
					DM_copy_face_data(splitdm,splitdm,i,curdupface,1);
					*df2=*mf;
					curdupface++;

					df3=CDDM_get_face(splitdm,curdupface);
					DM_copy_face_data(splitdm,splitdm,i,curdupface,1);
					*df3=*mf;
					curdupface++;

					if(v2==v3){
						df1->v1=edgesplit_get(edgehash, mf->v1, mf->v2);
						df3->v1=df1->v2=df1->v3=edgesplit_get(edgehash, mf->v2, mf->v3);
						df2->v1=df1->v4=edgesplit_get(edgehash, mf->v1, mf->v4);

						df3->v3=df2->v3=edgesplit_get(edgehash, mf->v3, mf->v4);

						df3->v2=mf->v3;
						df3->v4=0;

						mf->v2=df1->v1;
						mf->v3=df1->v4;
						mf->v4=0;

						facepa[i]=v1;
						facepa[curdupface-3]=facepa[curdupface-2]=v2;
						facepa[curdupface-1]=v3;
					}
					else{
						df3->v1=df2->v1=df1->v2=edgesplit_get(edgehash, mf->v1, mf->v2);
						df2->v4=df1->v3=edgesplit_get(edgehash, mf->v3, mf->v4);
						df1->v4=edgesplit_get(edgehash, mf->v1, mf->v4);

						df3->v3=df2->v2=edgesplit_get(edgehash, mf->v2, mf->v3);

						df3->v4=0;

						mf->v1=df1->v4;
						mf->v2=df1->v3;
						mf->v3=mf->v4;
						mf->v4=0;

						facepa[i]=v4;
						facepa[curdupface-3]=facepa[curdupface-2]=v1;
						facepa[curdupface-1]=v2;
					}

					test_index_face(df1, &splitdm->faceData, curdupface-3, (df1->v4 ? 4 : 3));
					test_index_face(df1, &splitdm->faceData, curdupface-2, (df1->v4 ? 4 : 3));
					test_index_face(df1, &splitdm->faceData, curdupface-1, (df1->v4 ? 4 : 3));
				}
			}

			test_index_face(df1, &splitdm->faceData, i, (df1->v4 ? 4 : 3));
		}
	}

	BLI_edgehash_free(edgehash, NULL);
	MEM_freeN(facesplit);
	MEM_freeN(vertpa);

	return splitdm;

}
static DerivedMesh * explodeMesh(ExplodeModifierData *emd, 
		ParticleSystemModifierData *psmd, Scene *scene, Object *ob, 
  DerivedMesh *to_explode)
{
	DerivedMesh *explode, *dm=to_explode;
	MFace *mf=0, *mface;
	ParticleSettings *part=psmd->psys->part;
	ParticleSimulationData sim = {scene, ob, psmd->psys, psmd};
	ParticleData *pa=NULL, *pars=psmd->psys->particles;
	ParticleKey state;
	EdgeHash *vertpahash;
	EdgeHashIterator *ehi;
	float *vertco=0, imat[4][4];
	float loc0[3], nor[3];
	float timestep, cfra;
	int *facepa=emd->facepa;
	int totdup=0,totvert=0,totface=0,totpart=0;
	int i, j, v, mindex=0;

	totface= dm->getNumFaces(dm);
	totvert= dm->getNumVerts(dm);
	mface= dm->getFaceArray(dm);
	totpart= psmd->psys->totpart;

	timestep= psys_get_timestep(&sim);

	//if(part->flag & PART_GLOB_TIME)
		cfra=bsystem_time(scene, 0,(float)scene->r.cfra,0.0);
	//else
	//	cfra=bsystem_time(scene, ob,(float)scene->r.cfra,0.0);

	/* hash table for vertice <-> particle relations */
	vertpahash= BLI_edgehash_new();

	for (i=0; i<totface; i++) {
		/* do mindex + totvert to ensure the vertex index to be the first
		 * with BLI_edgehashIterator_getKey */
		if(facepa[i]==totpart || cfra <= (pars+facepa[i])->time)
			mindex = totvert+totpart;
		else 
			mindex = totvert+facepa[i];

		mf= &mface[i];

		/* set face vertices to exist in particle group */
		BLI_edgehash_insert(vertpahash, mf->v1, mindex, NULL);
		BLI_edgehash_insert(vertpahash, mf->v2, mindex, NULL);
		BLI_edgehash_insert(vertpahash, mf->v3, mindex, NULL);
		if(mf->v4)
			BLI_edgehash_insert(vertpahash, mf->v4, mindex, NULL);
	}

	/* make new vertice indexes & count total vertices after duplication */
	ehi= BLI_edgehashIterator_new(vertpahash);
	for(; !BLI_edgehashIterator_isDone(ehi); BLI_edgehashIterator_step(ehi)) {
		BLI_edgehashIterator_setValue(ehi, SET_INT_IN_POINTER(totdup));
		totdup++;
	}
	BLI_edgehashIterator_free(ehi);

	/* the final duplicated vertices */
	explode= CDDM_from_template(dm, totdup, 0,totface);
	/*dupvert= CDDM_get_verts(explode);*/

	/* getting back to object space */
	invert_m4_m4(imat,ob->obmat);

	psmd->psys->lattice = psys_get_lattice(&sim);

	/* duplicate & displace vertices */
	ehi= BLI_edgehashIterator_new(vertpahash);
	for(; !BLI_edgehashIterator_isDone(ehi); BLI_edgehashIterator_step(ehi)) {
		MVert source;
		MVert *dest;

		/* get particle + vertex from hash */
		BLI_edgehashIterator_getKey(ehi, &j, &i);
		i -= totvert;
		v= GET_INT_FROM_POINTER(BLI_edgehashIterator_getValue(ehi));

		dm->getVert(dm, j, &source);
		dest = CDDM_get_vert(explode,v);

		DM_copy_vert_data(dm,explode,j,v,1);
		*dest = source;

		if(i!=totpart) {
			/* get particle */
			pa= pars+i;

			/* get particle state */
			psys_particle_on_emitter(psmd,part->from,pa->num,pa->num_dmcache,pa->fuv,pa->foffset,loc0,nor,0,0,0,0);
			mul_m4_v3(ob->obmat,loc0);

			state.time=cfra;
			psys_get_particle_state(&sim, i, &state, 1);

			vertco=CDDM_get_vert(explode,v)->co;
			
			mul_m4_v3(ob->obmat,vertco);

			VECSUB(vertco,vertco,loc0);

			/* apply rotation, size & location */
			mul_qt_v3(state.rot,vertco);
			if(emd->flag & eExplodeFlag_PaSize)
				mul_v3_fl(vertco,pa->size);
			VECADD(vertco,vertco,state.co);

			mul_m4_v3(imat,vertco);
		}
	}
	BLI_edgehashIterator_free(ehi);

	/*map new vertices to faces*/
	for (i=0; i<totface; i++) {
		MFace source;
		int orig_v4;

		if(facepa[i]!=totpart)
		{
			pa=pars+facepa[i];

			if(pa->alive==PARS_UNBORN && (emd->flag&eExplodeFlag_Unborn)==0) continue;
			if(pa->alive==PARS_ALIVE && (emd->flag&eExplodeFlag_Alive)==0) continue;
			if(pa->alive==PARS_DEAD && (emd->flag&eExplodeFlag_Dead)==0) continue;
		}

		dm->getFace(dm,i,&source);
		mf=CDDM_get_face(explode,i);
		
		orig_v4 = source.v4;

		if(facepa[i]!=totpart && cfra <= pa->time)
			mindex = totvert+totpart;
		else 
			mindex = totvert+facepa[i];

		source.v1 = edgesplit_get(vertpahash, source.v1, mindex);
		source.v2 = edgesplit_get(vertpahash, source.v2, mindex);
		source.v3 = edgesplit_get(vertpahash, source.v3, mindex);
		if(source.v4)
			source.v4 = edgesplit_get(vertpahash, source.v4, mindex);

		DM_copy_face_data(dm,explode,i,i,1);

		*mf = source;

		test_index_face(mf, &explode->faceData, i, (orig_v4 ? 4 : 3));
	}

	/* cleanup */
	BLI_edgehash_free(vertpahash, NULL);

	/* finalization */
	CDDM_calc_edges(explode);
	CDDM_calc_normals(explode);

	if(psmd->psys->lattice){
		end_latt_deform(psmd->psys->lattice);
		psmd->psys->lattice= NULL;
	}

	return explode;
}

static ParticleSystemModifierData * findPrecedingParticlesystem(Object *ob, ModifierData *emd)
{
	ModifierData *md;
	ParticleSystemModifierData *psmd=0;

	for (md=ob->modifiers.first; emd!=md; md=md->next){
		if(md->type==eModifierType_ParticleSystem)
			psmd= (ParticleSystemModifierData*) md;
	}
	return psmd;
}
static DerivedMesh * applyModifier(
		ModifierData *md, Object *ob, DerivedMesh *derivedData,
  int useRenderParams, int isFinalCalc)
{
	DerivedMesh *dm = derivedData;
	ExplodeModifierData *emd= (ExplodeModifierData*) md;
	ParticleSystemModifierData *psmd=findPrecedingParticlesystem(ob,md);

	if(psmd){
		ParticleSystem * psys=psmd->psys;

		if(psys==0 || psys->totpart==0) return derivedData;
		if(psys->part==0 || psys->particles==0) return derivedData;
		if(psmd->dm==0) return derivedData;

		/* 1. find faces to be exploded if needed */
		if(emd->facepa==0
				 || psmd->flag&eParticleSystemFlag_Pars
				 || emd->flag&eExplodeFlag_CalcFaces
				 || MEM_allocN_len(emd->facepa)/sizeof(int) != dm->getNumFaces(dm)){
			if(psmd->flag & eParticleSystemFlag_Pars)
				psmd->flag &= ~eParticleSystemFlag_Pars;
			
			if(emd->flag & eExplodeFlag_CalcFaces)
				emd->flag &= ~eExplodeFlag_CalcFaces;

			createFacepa(emd,psmd,ob,derivedData);
				 }

				 /* 2. create new mesh */
				 if(emd->flag & eExplodeFlag_EdgeSplit){
					 int *facepa = emd->facepa;
					 DerivedMesh *splitdm=splitEdges(emd,dm);
					 DerivedMesh *explode=explodeMesh(emd, psmd, md->scene, ob, splitdm);

					 MEM_freeN(emd->facepa);
					 emd->facepa=facepa;
					 splitdm->release(splitdm);
					 return explode;
				 }
				 else
					 return explodeMesh(emd, psmd, md->scene, ob, derivedData);
	}
	return derivedData;
}


ModifierTypeInfo modifierType_Explode = {
	/* name */              "Explode",
	/* structName */        "ExplodeModifierData",
	/* structSize */        sizeof(ExplodeModifierData),
	/* type */              eModifierTypeType_Nonconstructive,
	/* flags */             eModifierTypeFlag_AcceptsMesh,
	/* copyData */          copyData,
	/* deformVerts */       0,
	/* deformVertsEM */     0,
	/* deformMatricesEM */  0,
	/* applyModifier */     applyModifier,
	/* applyModifierEM */   0,
	/* initData */          initData,
	/* requiredDataMask */  requiredDataMask,
	/* freeData */          freeData,
	/* isDisabled */        0,
	/* updateDepgraph */    0,
	/* dependsOnTime */     dependsOnTime,
	/* foreachObjectLink */ 0,
	/* foreachIDLink */     0,
};
