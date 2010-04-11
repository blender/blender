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

#include "MOD_modifiertypes.h"


static void initData(ModifierData *md)
{
	ParticleInstanceModifierData *pimd= (ParticleInstanceModifierData*) md;

	pimd->flag = eParticleInstanceFlag_Parents|eParticleInstanceFlag_Unborn|
			eParticleInstanceFlag_Alive|eParticleInstanceFlag_Dead;
	pimd->psys = 1;
	pimd->position = 1.0f;
	pimd->axis = 2;

}
static void copyData(ModifierData *md, ModifierData *target)
{
	ParticleInstanceModifierData *pimd= (ParticleInstanceModifierData*) md;
	ParticleInstanceModifierData *tpimd= (ParticleInstanceModifierData*) target;

	tpimd->ob = pimd->ob;
	tpimd->psys = pimd->psys;
	tpimd->flag = pimd->flag;
	tpimd->axis = pimd->axis;
	tpimd->position = pimd->position;
	tpimd->random_position = pimd->random_position;
}

static int dependsOnTime(ModifierData *md)
{
	return 0;
}
static void updateDepgraph(ModifierData *md, DagForest *forest,
		 Scene *scene,Object *ob, DagNode *obNode)
{
	ParticleInstanceModifierData *pimd = (ParticleInstanceModifierData*) md;

	if (pimd->ob) {
		DagNode *curNode = dag_get_node(forest, pimd->ob);

		dag_add_relation(forest, curNode, obNode,
				 DAG_RL_DATA_DATA | DAG_RL_OB_DATA,
				 "Particle Instance Modifier");
	}
}

static void foreachObjectLink(ModifierData *md, Object *ob,
		ObjectWalkFunc walk, void *userData)
{
	ParticleInstanceModifierData *pimd = (ParticleInstanceModifierData*) md;

	walk(userData, ob, &pimd->ob);
}

static DerivedMesh * applyModifier(
		ModifierData *md, Object *ob, DerivedMesh *derivedData,
  int useRenderParams, int isFinalCalc)
{
	DerivedMesh *dm = derivedData, *result;
	ParticleInstanceModifierData *pimd= (ParticleInstanceModifierData*) md;
	ParticleSimulationData sim;
	ParticleSystem * psys=0;
	ParticleData *pa=0, *pars=0;
	MFace *mface, *orig_mface;
	MVert *mvert, *orig_mvert;
	int i,totvert, totpart=0, totface, maxvert, maxface, first_particle=0;
	short track=ob->trackflag%3, trackneg, axis = pimd->axis;
	float max_co=0.0, min_co=0.0, temp_co[3], cross[3];
	float *size=NULL;

	trackneg=((ob->trackflag>2)?1:0);

	if(pimd->ob==ob){
		pimd->ob=0;
		return derivedData;
	}

	if(pimd->ob){
		psys = BLI_findlink(&pimd->ob->particlesystem,pimd->psys-1);
		if(psys==0 || psys->totpart==0)
			return derivedData;
	}
	else return derivedData;

	if(pimd->flag & eParticleInstanceFlag_Parents)
		totpart+=psys->totpart;
	if(pimd->flag & eParticleInstanceFlag_Children){
		if(totpart==0)
			first_particle=psys->totpart;
		totpart+=psys->totchild;
	}

	if(totpart==0)
		return derivedData;

	sim.scene = md->scene;
	sim.ob = pimd->ob;
	sim.psys = psys;
	sim.psmd = psys_get_modifier(pimd->ob, psys);

	if(pimd->flag & eParticleInstanceFlag_UseSize) {
		int p;
		float *si;
		si = size = MEM_callocN(totpart * sizeof(float), "particle size array");

		if(pimd->flag & eParticleInstanceFlag_Parents) {
			for(p=0, pa= psys->particles; p<psys->totpart; p++, pa++, si++)
				*si = pa->size;
		}

		if(pimd->flag & eParticleInstanceFlag_Children) {
			ChildParticle *cpa = psys->child;

			for(p=0; p<psys->totchild; p++, cpa++, si++) {
				*si = psys_get_child_size(psys, cpa, 0.0f, NULL);
			}
		}
	}

	pars=psys->particles;

	totvert=dm->getNumVerts(dm);
	totface=dm->getNumFaces(dm);

	maxvert=totvert*totpart;
	maxface=totface*totpart;

	psys->lattice=psys_get_lattice(&sim);

	if(psys->flag & (PSYS_HAIR_DONE|PSYS_KEYED) || psys->pointcache->flag & PTCACHE_BAKED){

		float min_r[3], max_r[3];
		INIT_MINMAX(min_r, max_r);
		dm->getMinMax(dm, min_r, max_r);
		min_co=min_r[track];
		max_co=max_r[track];
	}

	result = CDDM_from_template(dm, maxvert,dm->getNumEdges(dm)*totpart,maxface);

	mvert=result->getVertArray(result);
	orig_mvert=dm->getVertArray(dm);

	for(i=0; i<maxvert; i++){
		MVert *inMV;
		MVert *mv = mvert + i;
		ParticleKey state;

		inMV = orig_mvert + i%totvert;
		DM_copy_vert_data(dm, result, i%totvert, i, 1);
		*mv = *inMV;

		/*change orientation based on object trackflag*/
		VECCOPY(temp_co,mv->co);
		mv->co[axis]=temp_co[track];
		mv->co[(axis+1)%3]=temp_co[(track+1)%3];
		mv->co[(axis+2)%3]=temp_co[(track+2)%3];

		if((psys->flag & (PSYS_HAIR_DONE|PSYS_KEYED) || psys->pointcache->flag & PTCACHE_BAKED) && pimd->flag & eParticleInstanceFlag_Path){
			float ran = 0.0f;
			if(pimd->random_position != 0.0f) {
				BLI_srandom(psys->seed + (i/totvert)%totpart);
				ran = pimd->random_position * BLI_frand();
			}

			if(pimd->flag & eParticleInstanceFlag_KeepShape) {
				state.time = pimd->position * (1.0f - ran);
			}
			else {
				state.time=(mv->co[axis]-min_co)/(max_co-min_co) * pimd->position * (1.0f - ran);

				if(trackneg)
					state.time=1.0f-state.time;

				mv->co[axis] = 0.0;
			}

			psys_get_particle_on_path(&sim, first_particle + i/totvert, &state,1);

			normalize_v3(state.vel);

			/* TODO: incremental rotations somehow */
			if(state.vel[axis] < -0.9999 || state.vel[axis] > 0.9999) {
				state.rot[0] = 1;
				state.rot[1] = state.rot[2] = state.rot[3] = 0.0f;
			}
			else {
				float temp[3] = {0.0f,0.0f,0.0f};
				temp[axis] = 1.0f;

				cross_v3_v3v3(cross, temp, state.vel);

				/* state.vel[axis] is the only component surviving from a dot product with the axis */
				axis_angle_to_quat(state.rot,cross,saacos(state.vel[axis]));
			}

		}
		else{
			state.time=-1.0;
			psys_get_particle_state(&sim, first_particle + i/totvert, &state,1);
		}

		mul_qt_v3(state.rot,mv->co);
		if(pimd->flag & eParticleInstanceFlag_UseSize)
			mul_v3_fl(mv->co, size[i/totvert]);
		VECADD(mv->co,mv->co,state.co);
	}

	mface=result->getFaceArray(result);
	orig_mface=dm->getFaceArray(dm);

	for(i=0; i<maxface; i++){
		MFace *inMF;
		MFace *mf = mface + i;

		if(pimd->flag & eParticleInstanceFlag_Parents){
			if(i/totface>=psys->totpart){
				if(psys->part->childtype==PART_CHILD_PARTICLES)
					pa=psys->particles+(psys->child+i/totface-psys->totpart)->parent;
				else
					pa=0;
			}
			else
				pa=pars+i/totface;
		}
		else{
			if(psys->part->childtype==PART_CHILD_PARTICLES)
				pa=psys->particles+(psys->child+i/totface)->parent;
			else
				pa=0;
		}

		if(pa){
			if(pa->alive==PARS_UNBORN && (pimd->flag&eParticleInstanceFlag_Unborn)==0) continue;
			if(pa->alive==PARS_ALIVE && (pimd->flag&eParticleInstanceFlag_Alive)==0) continue;
			if(pa->alive==PARS_DEAD && (pimd->flag&eParticleInstanceFlag_Dead)==0) continue;
		}

		inMF = orig_mface + i%totface;
		DM_copy_face_data(dm, result, i%totface, i, 1);
		*mf = *inMF;

		mf->v1+=(i/totface)*totvert;
		mf->v2+=(i/totface)*totvert;
		mf->v3+=(i/totface)*totvert;
		if(mf->v4)
			mf->v4+=(i/totface)*totvert;
	}

	CDDM_calc_edges(result);
	CDDM_calc_normals(result);

	if(psys->lattice){
		end_latt_deform(psys->lattice);
		psys->lattice= NULL;
	}

	if(size)
		MEM_freeN(size);

	return result;
}
static DerivedMesh *applyModifierEM(
		ModifierData *md, Object *ob, EditMesh *editData,
  DerivedMesh *derivedData)
{
	return applyModifier(md, ob, derivedData, 0, 1);
}


ModifierTypeInfo modifierType_ParticleInstance = {
	/* name */              "ParticleInstance",
	/* structName */        "ParticleInstanceModifierData",
	/* structSize */        sizeof(ParticleInstanceModifierData),
	/* type */              eModifierTypeType_Constructive,
	/* flags */             eModifierTypeFlag_AcceptsMesh |
							eModifierTypeFlag_SupportsMapping |
							eModifierTypeFlag_SupportsEditmode |
							eModifierTypeFlag_EnableInEditmode,

	/* copyData */          copyData,
	/* deformVerts */       0,
	/* deformVertsEM */     0,
	/* deformMatricesEM */  0,
	/* applyModifier */     applyModifier,
	/* applyModifierEM */   applyModifierEM,
	/* initData */          initData,
	/* requiredDataMask */  0,
	/* freeData */          0,
	/* isDisabled */        0,
	/* updateDepgraph */    updateDepgraph,
	/* dependsOnTime */     dependsOnTime,
	/* foreachObjectLink */ foreachObjectLink,
	/* foreachIDLink */     0,
};
