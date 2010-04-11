/*
* $Id:
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
#include "BLI_uvproject.h"

#include "MEM_guardedalloc.h"

#include "DNA_armature_types.h"
#include "DNA_camera_types.h"
#include "DNA_curve_types.h"
#include "DNA_key_types.h"
#include "DNA_material_types.h"
#include "DNA_object_fluidsim.h"


#include "BKE_action.h"
#include "BKE_bmesh.h"
#include "BKE_cloth.h"
#include "BKE_cdderivedmesh.h"
#include "BKE_displist.h"
#include "BKE_fluidsim.h"
#include "BKE_global.h"
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

#include "depsgraph_private.h"
#include "BKE_deform.h"
#include "BKE_shrinkwrap.h"

#include "LOD_decimation.h"

#include "CCGSubSurf.h"

#include "RE_shader_ext.h"

#include "MOD_modifiertypes.h"
#include "MOD_util.h"


static void initData(ModifierData *md)
{
	MeshDeformModifierData *mmd = (MeshDeformModifierData*) md;

	mmd->gridsize= 5;
}

static void freeData(ModifierData *md)
{
	MeshDeformModifierData *mmd = (MeshDeformModifierData*) md;

	if(mmd->bindweights) MEM_freeN(mmd->bindweights);
	if(mmd->bindcos) MEM_freeN(mmd->bindcos);
	if(mmd->dyngrid) MEM_freeN(mmd->dyngrid);
	if(mmd->dyninfluences) MEM_freeN(mmd->dyninfluences);
	if(mmd->dynverts) MEM_freeN(mmd->dynverts);
}

static void copyData(ModifierData *md, ModifierData *target)
{
	MeshDeformModifierData *mmd = (MeshDeformModifierData*) md;
	MeshDeformModifierData *tmmd = (MeshDeformModifierData*) target;

	tmmd->gridsize = mmd->gridsize;
	tmmd->object = mmd->object;
}

static CustomDataMask requiredDataMask(Object *ob, ModifierData *md)
{	
	MeshDeformModifierData *mmd = (MeshDeformModifierData *)md;
	CustomDataMask dataMask = 0;

	/* ask for vertexgroups if we need them */
	if(mmd->defgrp_name[0]) dataMask |= (1 << CD_MDEFORMVERT);

	return dataMask;
}

static int isDisabled(ModifierData *md, int useRenderParams)
{
	MeshDeformModifierData *mmd = (MeshDeformModifierData*) md;

	return !mmd->object;
}

static void foreachObjectLink(
		ModifierData *md, Object *ob,
  void (*walk)(void *userData, Object *ob, Object **obpoin),
	 void *userData)
{
	MeshDeformModifierData *mmd = (MeshDeformModifierData*) md;

	walk(userData, ob, &mmd->object);
}

static void updateDepgraph(
						  ModifierData *md, DagForest *forest, Scene *scene, Object *ob,
	   DagNode *obNode)
{
	MeshDeformModifierData *mmd = (MeshDeformModifierData*) md;

	if (mmd->object) {
		DagNode *curNode = dag_get_node(forest, mmd->object);

		dag_add_relation(forest, curNode, obNode,
				 DAG_RL_DATA_DATA|DAG_RL_OB_DATA|DAG_RL_DATA_OB|DAG_RL_OB_OB,
				 "Mesh Deform Modifier");
	}
}

static float meshdeform_dynamic_bind(MeshDeformModifierData *mmd, float (*dco)[3], float *vec)
{
	MDefCell *cell;
	MDefInfluence *inf;
	float gridvec[3], dvec[3], ivec[3], co[3], wx, wy, wz;
	float weight, cageweight, totweight, *cageco;
	int i, j, a, x, y, z, size;

	co[0]= co[1]= co[2]= 0.0f;
	totweight= 0.0f;
	size= mmd->dyngridsize;

	for(i=0; i<3; i++) {
		gridvec[i]= (vec[i] - mmd->dyncellmin[i] - mmd->dyncellwidth*0.5f)/mmd->dyncellwidth;
		ivec[i]= (int)gridvec[i];
		dvec[i]= gridvec[i] - ivec[i];
	}

	for(i=0; i<8; i++) {
		if(i & 1) { x= ivec[0]+1; wx= dvec[0]; }
		else { x= ivec[0]; wx= 1.0f-dvec[0]; } 

		if(i & 2) { y= ivec[1]+1; wy= dvec[1]; }
		else { y= ivec[1]; wy= 1.0f-dvec[1]; } 

		if(i & 4) { z= ivec[2]+1; wz= dvec[2]; }
		else { z= ivec[2]; wz= 1.0f-dvec[2]; } 

		CLAMP(x, 0, size-1);
		CLAMP(y, 0, size-1);
		CLAMP(z, 0, size-1);

		a= x + y*size + z*size*size;
		weight= wx*wy*wz;

		cell= &mmd->dyngrid[a];
		inf= mmd->dyninfluences + cell->offset;
		for(j=0; j<cell->totinfluence; j++, inf++) {
			cageco= dco[inf->vertex];
			cageweight= weight*inf->weight;
			co[0] += cageweight*cageco[0];
			co[1] += cageweight*cageco[1];
			co[2] += cageweight*cageco[2];
			totweight += cageweight;
		}
	}

	VECCOPY(vec, co);

	return totweight;
}

static void meshdeformModifier_do(
				  ModifierData *md, Object *ob, DerivedMesh *dm,
	  float (*vertexCos)[3], int numVerts)
{
	MeshDeformModifierData *mmd = (MeshDeformModifierData*) md;
	Mesh *me= (mmd->object)? mmd->object->data: NULL;
	EditMesh *em = (me)? BKE_mesh_get_editmesh(me): NULL;
	DerivedMesh *tmpdm, *cagedm;
	MDeformVert *dvert = NULL;
	MDeformWeight *dw;
	MVert *cagemvert;
	float imat[4][4], cagemat[4][4], iobmat[4][4], icagemat[3][3], cmat[4][4];
	float weight, totweight, fac, co[3], *weights, (*dco)[3], (*bindcos)[3];
	int a, b, totvert, totcagevert, defgrp_index;
	
	if(!mmd->object || (!mmd->bindcos && !mmd->bindfunc))
		return;
	
	/* get cage derivedmesh */
	if(em) {
		tmpdm= editmesh_get_derived_cage_and_final(md->scene, ob, em, &cagedm, 0);
		if(tmpdm)
			tmpdm->release(tmpdm);
		BKE_mesh_end_editmesh(me, em);
	}
	else
		cagedm= mmd->object->derivedFinal;

	/* if we don't have one computed, use derivedmesh from data
	 * without any modifiers */
	if(!cagedm) {
		cagedm= get_dm(md->scene, mmd->object, NULL, NULL, NULL, 0);
		if(cagedm)
			cagedm->needsFree= 1;
	}
	
	if(!cagedm)
		return;

	/* compute matrices to go in and out of cage object space */
	invert_m4_m4(imat, mmd->object->obmat);
	mul_m4_m4m4(cagemat, ob->obmat, imat);
	mul_m4_m4m4(cmat, cagemat, mmd->bindmat);
	invert_m4_m4(iobmat, cmat);
	copy_m3_m4(icagemat, iobmat);

	/* bind weights if needed */
	if(!mmd->bindcos) {
		static int recursive = 0;

		/* progress bar redraw can make this recursive .. */
		if(!recursive) {
			recursive = 1;
			mmd->bindfunc(md->scene, dm, mmd, (float*)vertexCos, numVerts, cagemat);
			recursive = 0;
		}
	}

	/* verify we have compatible weights */
	totvert= numVerts;
	totcagevert= cagedm->getNumVerts(cagedm);

	if(mmd->totvert!=totvert || mmd->totcagevert!=totcagevert || !mmd->bindcos) {
		cagedm->release(cagedm);
		return;
	}
	
	/* setup deformation data */
	cagemvert= cagedm->getVertArray(cagedm);
	weights= mmd->bindweights;
	bindcos= (float(*)[3])mmd->bindcos;

	dco= MEM_callocN(sizeof(*dco)*totcagevert, "MDefDco");
	for(a=0; a<totcagevert; a++) {
		/* get cage vertex in world space with binding transform */
		VECCOPY(co, cagemvert[a].co);

		if(G.rt != 527) {
			mul_m4_v3(mmd->bindmat, co);
			/* compute difference with world space bind coord */
			VECSUB(dco[a], co, bindcos[a]);
		}
		else
			VECCOPY(dco[a], co)
	}

	defgrp_index = defgroup_name_index(ob, mmd->defgrp_name);

	if (defgrp_index >= 0)
		dvert= dm->getVertDataArray(dm, CD_MDEFORMVERT);

	/* do deformation */
	fac= 1.0f;

	for(b=0; b<totvert; b++) {
		if(mmd->flag & MOD_MDEF_DYNAMIC_BIND)
			if(!mmd->dynverts[b])
				continue;

		if(dvert) {
			for(dw=NULL, a=0; a<dvert[b].totweight; a++) {
				if(dvert[b].dw[a].def_nr == defgrp_index) {
					dw = &dvert[b].dw[a];
					break;
				}
			}

			if(mmd->flag & MOD_MDEF_INVERT_VGROUP) {
				if(!dw) fac= 1.0f;
				else if(dw->weight == 1.0f) continue;
				else fac=1.0f-dw->weight;
			}
			else {
				if(!dw) continue;
				else fac= dw->weight;
			}
		}

		if(mmd->flag & MOD_MDEF_DYNAMIC_BIND) {
			/* transform coordinate into cage's local space */
			VECCOPY(co, vertexCos[b]);
			mul_m4_v3(cagemat, co);
			totweight= meshdeform_dynamic_bind(mmd, dco, co);
		}
		else {
			totweight= 0.0f;
			co[0]= co[1]= co[2]= 0.0f;

			for(a=0; a<totcagevert; a++) {
				weight= weights[a + b*totcagevert];
				co[0]+= weight*dco[a][0];
				co[1]+= weight*dco[a][1];
				co[2]+= weight*dco[a][2];
				totweight += weight;
			}
		}

		if(totweight > 0.0f) {
			mul_v3_fl(co, fac/totweight);
			mul_m3_v3(icagemat, co);
			if(G.rt != 527)
				VECADD(vertexCos[b], vertexCos[b], co)
						else
						VECCOPY(vertexCos[b], co)
		}
	}

	/* release cage derivedmesh */
	MEM_freeN(dco);
	cagedm->release(cagedm);
}

static void deformVerts(
					   ModifierData *md, Object *ob, DerivedMesh *derivedData,
	float (*vertexCos)[3], int numVerts, int useRenderParams, int isFinalCalc)
{
	DerivedMesh *dm= get_dm(md->scene, ob, NULL, derivedData, NULL, 0);;

	if(!dm)
		return;

	modifier_vgroup_cache(md, vertexCos); /* if next modifier needs original vertices */
	
	meshdeformModifier_do(md, ob, dm, vertexCos, numVerts);

	if(dm != derivedData)
		dm->release(dm);
}

static void deformVertsEM(
						 ModifierData *md, Object *ob, EditMesh *editData,
	  DerivedMesh *derivedData, float (*vertexCos)[3], int numVerts)
{
	DerivedMesh *dm;

	if(!derivedData && ob->type == OB_MESH)
		dm = CDDM_from_editmesh(editData, ob->data);
	else
		dm = derivedData;

	meshdeformModifier_do(md, ob, dm, vertexCos, numVerts);

	if(dm != derivedData)
		dm->release(dm);
}


ModifierTypeInfo modifierType_MeshDeform = {
	/* name */              "MeshDeform",
	/* structName */        "MeshDeformModifierData",
	/* structSize */        sizeof(MeshDeformModifierData),
	/* type */              eModifierTypeType_OnlyDeform,
	/* flags */             eModifierTypeFlag_AcceptsCVs
							| eModifierTypeFlag_SupportsEditmode,

	/* copyData */          copyData,
	/* deformVerts */       deformVerts,
	/* deformVertsEM */     deformVertsEM,
	/* deformMatricesEM */  0,
	/* applyModifier */     0,
	/* applyModifierEM */   0,
	/* initData */          initData,
	/* requiredDataMask */  requiredDataMask,
	/* freeData */          freeData,
	/* isDisabled */        isDisabled,
	/* updateDepgraph */    updateDepgraph,
	/* dependsOnTime */     0,
	/* foreachObjectLink */ foreachObjectLink,
	/* foreachIDLink */     0,
};
