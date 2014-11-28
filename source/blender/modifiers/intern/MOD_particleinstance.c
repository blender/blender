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

/** \file blender/modifiers/intern/MOD_particleinstance.c
 *  \ingroup modifiers
 */


#include "DNA_meshdata_types.h"

#include "MEM_guardedalloc.h"

#include "BLI_math.h"
#include "BLI_listbase.h"
#include "BLI_rand.h"
#include "BLI_utildefines.h"

#include "BKE_cdderivedmesh.h"
#include "BKE_lattice.h"
#include "BKE_modifier.h"
#include "BKE_particle.h"
#include "BKE_pointcache.h"

#include "depsgraph_private.h"


static void initData(ModifierData *md)
{
	ParticleInstanceModifierData *pimd = (ParticleInstanceModifierData *) md;

	pimd->flag = eParticleInstanceFlag_Parents | eParticleInstanceFlag_Unborn |
	             eParticleInstanceFlag_Alive | eParticleInstanceFlag_Dead;
	pimd->psys = 1;
	pimd->position = 1.0f;
	pimd->axis = 2;

}
static void copyData(ModifierData *md, ModifierData *target)
{
#if 0
	ParticleInstanceModifierData *pimd = (ParticleInstanceModifierData *) md;
	ParticleInstanceModifierData *tpimd = (ParticleInstanceModifierData *) target;
#endif
	modifier_copyData_generic(md, target);
}

static bool isDisabled(ModifierData *md, int useRenderParams)
{
	ParticleInstanceModifierData *pimd = (ParticleInstanceModifierData *)md;
	ParticleSystem *psys;
	ModifierData *ob_md;
	
	if (!pimd->ob)
		return true;
	
	psys = BLI_findlink(&pimd->ob->particlesystem, pimd->psys - 1);
	if (psys == NULL)
		return true;
	
	/* If the psys modifier is disabled we cannot use its data.
	 * First look up the psys modifier from the object, then check if it is enabled.
	 */
	for (ob_md = pimd->ob->modifiers.first; ob_md; ob_md = ob_md->next) {
		if (ob_md->type == eModifierType_ParticleSystem) {
			ParticleSystemModifierData *psmd = (ParticleSystemModifierData *)ob_md;
			if (psmd->psys == psys) {
				int required_mode;
				
				if (useRenderParams) required_mode = eModifierMode_Render;
				else required_mode = eModifierMode_Realtime;
				
				if (!modifier_isEnabled(md->scene, ob_md, required_mode))
					return true;
				
				break;
			}
		}
	}
	
	return false;
}


static void updateDepgraph(ModifierData *md, DagForest *forest,
                           struct Scene *UNUSED(scene),
                           Object *UNUSED(ob),
                           DagNode *obNode)
{
	ParticleInstanceModifierData *pimd = (ParticleInstanceModifierData *) md;

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
	ParticleInstanceModifierData *pimd = (ParticleInstanceModifierData *) md;

	walk(userData, ob, &pimd->ob);
}

static int particle_skip(ParticleInstanceModifierData *pimd, ParticleSystem *psys, int p)
{
	ParticleData *pa;

	if (pimd->flag & eParticleInstanceFlag_Parents) {
		if (p >= psys->totpart) {
			if (psys->part->childtype == PART_CHILD_PARTICLES) {
				pa = psys->particles + (psys->child + p - psys->totpart)->parent;
			}
			else {
				pa = NULL;
			}
		}
		else {
			pa = psys->particles + p;
		}
	}
	else {
		if (psys->part->childtype == PART_CHILD_PARTICLES) {
			pa = psys->particles + (psys->child + p)->parent;
		}
		else {
			pa = NULL;
		}
	}

	if (pa) {
		if (pa->alive == PARS_UNBORN && (pimd->flag & eParticleInstanceFlag_Unborn) == 0) return 1;
		if (pa->alive == PARS_ALIVE && (pimd->flag & eParticleInstanceFlag_Alive) == 0) return 1;
		if (pa->alive == PARS_DEAD && (pimd->flag & eParticleInstanceFlag_Dead) == 0) return 1;
	}
	
	return 0;
}

static DerivedMesh *applyModifier(ModifierData *md, Object *ob,
                                  DerivedMesh *derivedData,
                                  ModifierApplyFlag UNUSED(flag))
{
	DerivedMesh *dm = derivedData, *result;
	ParticleInstanceModifierData *pimd = (ParticleInstanceModifierData *) md;
	ParticleSimulationData sim;
	ParticleSystem *psys = NULL;
	ParticleData *pa = NULL;
	MPoly *mpoly, *orig_mpoly;
	MLoop *mloop, *orig_mloop;
	MVert *mvert, *orig_mvert;
	int totvert, totpoly, totloop /* , totedge */;
	int maxvert, maxpoly, maxloop, totpart = 0, first_particle = 0;
	int k, p, p_skip;
	short track = ob->trackflag % 3, trackneg, axis = pimd->axis;
	float max_co = 0.0, min_co = 0.0, temp_co[3], cross[3];
	float *size = NULL;

	trackneg = ((ob->trackflag > 2) ? 1 : 0);

	if (pimd->ob == ob) {
		pimd->ob = NULL;
		return derivedData;
	}

	if (pimd->ob) {
		psys = BLI_findlink(&pimd->ob->particlesystem, pimd->psys - 1);
		if (psys == NULL || psys->totpart == 0)
			return derivedData;
	}
	else {
		return derivedData;
	}

	if (pimd->flag & eParticleInstanceFlag_Parents)
		totpart += psys->totpart;
	if (pimd->flag & eParticleInstanceFlag_Children) {
		if (totpart == 0)
			first_particle = psys->totpart;
		totpart += psys->totchild;
	}

	if (totpart == 0)
		return derivedData;

	sim.scene = md->scene;
	sim.ob = pimd->ob;
	sim.psys = psys;
	sim.psmd = psys_get_modifier(pimd->ob, psys);

	if (pimd->flag & eParticleInstanceFlag_UseSize) {
		float *si;
		si = size = MEM_callocN(totpart * sizeof(float), "particle size array");

		if (pimd->flag & eParticleInstanceFlag_Parents) {
			for (p = 0, pa = psys->particles; p < psys->totpart; p++, pa++, si++)
				*si = pa->size;
		}

		if (pimd->flag & eParticleInstanceFlag_Children) {
			ChildParticle *cpa = psys->child;

			for (p = 0; p < psys->totchild; p++, cpa++, si++) {
				*si = psys_get_child_size(psys, cpa, 0.0f, NULL);
			}
		}
	}

	totvert = dm->getNumVerts(dm);
	totpoly = dm->getNumPolys(dm);
	totloop = dm->getNumLoops(dm);
	/* totedge = dm->getNumEdges(dm); */ /* UNUSED */

	/* count particles */
	maxvert = 0;
	maxpoly = 0;
	maxloop = 0;

	for (p = 0; p < totpart; p++) {
		if (particle_skip(pimd, psys, p))
			continue;

		maxvert += totvert;
		maxpoly += totpoly;
		maxloop += totloop;
	}

	psys->lattice_deform_data = psys_create_lattice_deform_data(&sim);

	if (psys->flag & (PSYS_HAIR_DONE | PSYS_KEYED) || psys->pointcache->flag & PTCACHE_BAKED) {
		float min[3], max[3];
		INIT_MINMAX(min, max);
		dm->getMinMax(dm, min, max);
		min_co = min[track];
		max_co = max[track];
	}

	result = CDDM_from_template(dm, maxvert, 0, 0, maxloop, maxpoly);

	mvert = result->getVertArray(result);
	orig_mvert = dm->getVertArray(dm);

	mpoly = result->getPolyArray(result);
	orig_mpoly = dm->getPolyArray(dm);
	mloop = result->getLoopArray(result);
	orig_mloop = dm->getLoopArray(dm);

	for (p = 0, p_skip = 0; p < totpart; p++) {
		/* skip particle? */
		if (particle_skip(pimd, psys, p))
			continue;

		/* set vertices coordinates */
		for (k = 0; k < totvert; k++) {
			ParticleKey state;
			MVert *inMV;
			MVert *mv = mvert + p_skip * totvert + k;

			inMV = orig_mvert + k;
			DM_copy_vert_data(dm, result, k, p_skip * totvert + k, 1);
			*mv = *inMV;

			/*change orientation based on object trackflag*/
			copy_v3_v3(temp_co, mv->co);
			mv->co[axis] = temp_co[track];
			mv->co[(axis + 1) % 3] = temp_co[(track + 1) % 3];
			mv->co[(axis + 2) % 3] = temp_co[(track + 2) % 3];

			/* get particle state */
			if ((psys->flag & (PSYS_HAIR_DONE | PSYS_KEYED) || psys->pointcache->flag & PTCACHE_BAKED) &&
			    (pimd->flag & eParticleInstanceFlag_Path))
			{
				float ran = 0.0f;
				if (pimd->random_position != 0.0f) {
					ran = pimd->random_position * BLI_hash_frand(psys->seed + p);
				}

				if (pimd->flag & eParticleInstanceFlag_KeepShape) {
					state.time = pimd->position * (1.0f - ran);
				}
				else {
					state.time = (mv->co[axis] - min_co) / (max_co - min_co) * pimd->position * (1.0f - ran);

					if (trackneg)
						state.time = 1.0f - state.time;

					mv->co[axis] = 0.0;
				}

				psys_get_particle_on_path(&sim, first_particle + p, &state, 1);

				normalize_v3(state.vel);

				/* TODO: incremental rotations somehow */
				if (state.vel[axis] < -0.9999f || state.vel[axis] > 0.9999f) {
					unit_qt(state.rot);
				}
				else {
					float temp[3] = {0.0f, 0.0f, 0.0f};
					temp[axis] = 1.0f;

					cross_v3_v3v3(cross, temp, state.vel);

					/* state.vel[axis] is the only component surviving from a dot product with the axis */
					axis_angle_to_quat(state.rot, cross, saacos(state.vel[axis]));
				}
			}
			else {
				state.time = -1.0;
				psys_get_particle_state(&sim, first_particle + p, &state, 1);
			}

			mul_qt_v3(state.rot, mv->co);
			if (pimd->flag & eParticleInstanceFlag_UseSize)
				mul_v3_fl(mv->co, size[p]);
			add_v3_v3(mv->co, state.co);
		}

		/* create polys and loops */
		for (k = 0; k < totpoly; k++) {
			MPoly *inMP = orig_mpoly + k;
			MPoly *mp = mpoly + p_skip * totpoly + k;

			DM_copy_poly_data(dm, result, k, p_skip * totpoly + k, 1);
			*mp = *inMP;
			mp->loopstart += p_skip * totloop;

			{
				MLoop *inML = orig_mloop + inMP->loopstart;
				MLoop *ml = mloop + mp->loopstart;
				int j = mp->totloop;

				DM_copy_loop_data(dm, result, inMP->loopstart, mp->loopstart, j);
				for (; j; j--, ml++, inML++) {
					ml->v = inML->v + (p_skip * totvert);
				}
			}
		}

		p_skip++;
	}

	CDDM_calc_edges(result);

	if (psys->lattice_deform_data) {
		end_latt_deform(psys->lattice_deform_data);
		psys->lattice_deform_data = NULL;
	}

	if (size)
		MEM_freeN(size);

	result->dirty |= DM_DIRTY_NORMALS;

	return result;
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
	/* deformVerts */       NULL,
	/* deformMatrices */    NULL,
	/* deformVertsEM */     NULL,
	/* deformMatricesEM */  NULL,
	/* applyModifier */     applyModifier,
	/* applyModifierEM */   NULL,
	/* initData */          initData,
	/* requiredDataMask */  NULL,
	/* freeData */          NULL,
	/* isDisabled */        isDisabled,
	/* updateDepgraph */    updateDepgraph,
	/* dependsOnTime */     NULL,
	/* dependsOnNormals */  NULL,
	/* foreachObjectLink */ foreachObjectLink,
	/* foreachIDLink */     NULL,
	/* foreachTexLink */    NULL,
};
