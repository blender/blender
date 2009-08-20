/**
 * smokehighres.c
 *
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
 * The Original Code is Copyright (C) Blender Foundation.
 * All rights reserved.
 *
 * The Original Code is: all of this file.
 *
 * Contributor(s): Daniel Genrich
 *
 * ***** END GPL LICENSE BLOCK *****
 */

/* Part of the code copied from elbeem fluid library, copyright by Nils Thuerey */

#include "DNA_scene_types.h"
#include "DNA_listBase.h"
#include "DNA_modifier_types.h"
#include "DNA_object_types.h"
#include "DNA_smoke_types.h"

#include "BKE_modifier.h"
#include "BKE_smoke.h"
#include "BKE_pointcache.h"

#include "smoke_API.h"

// we need different handling for the high-res feature
/*
if(bigdensity)
{
	// init all surrounding cells according to amplification, too
	int i, j, k;

	smoke_turbulence_get_res(smd->domain->wt, bigres);

	for(i = 0; i < smd->domain->amplify + 1; i++)
		for(j = 0; j < smd->domain->amplify + 1; j++)
			for(k = 0; k < smd->domain->amplify + 1; k++)
			{
				index = smoke_get_index((smd->domain->amplify + 1)* cell[0] + i, bigres[0], (smd->domain->amplify + 1)* cell[1] + j, bigres[1], (smd->domain->amplify + 1)* cell[2] + k);
				bigdensity[index] = sfs->density;
			}
}
*/

static void smokeHRinit(SmokeHRModifierData *shrmd, SmokeDomainSettings *sds)
{
	if(!shrmd->wt)
	{
		shrmd->wt = smoke_turbulence_init(sds->res,  shrmd->amplify + 1, shrmd->noise);
		smoke_turbulence_initBlenderRNA(shrmd->wt, &shrmd->strength);
	}
}

void smokeHRModifier_free(SmokeHRModifierData *shrmd)
{
	if(shrmd->wt)
		smoke_turbulence_free(shrmd->wt);

	BKE_ptcache_free_list(&shrmd->ptcaches);
	shrmd->point_cache = NULL;
}

void smokeHRModifier_do(SmokeHRModifierData *shrmd, Scene *scene, Object *ob, int useRenderParams, int isFinalCalc)
{
	ModifierData *md = NULL;
	SmokeModifierData *smd = NULL;
	SmokeDomainSettings *sds = NULL;

	// find underlaying smoke domain
	smd = (SmokeModifierData *)modifiers_findByType(ob, eModifierType_Smoke);
	if(!(smd && smd->type == MOD_SMOKE_TYPE_DOMAIN))
		return;

	sds = smd->domain;

	smokeHRinit(shrmd, sds);

	// smoke_turbulence_dissolve(shrmd->wt, sds->diss_speed, sds->flags & MOD_SMOKE_DISSOLVE_LOG);

	// smoke_turbulence_step(shrmd->wt, sds->fluid);
}


// update necessary information for 3dview ("high res" option)
void smoke_prepare_bigView(SmokeHRModifierData *shrmd, float *light)
{
	float *density = NULL;
	size_t i = 0;
	int bigres[3];
/*
	smoke_turbulence_get_res(shrmd->wt, bigres);

	if(!smd->domain->traybig)
	{
		// TRay is for self shadowing
		smd->domain->traybig = MEM_callocN(sizeof(float)*bigres[0]*bigres[1]*bigres[2], "Smoke_tRayBig");
	}
	if(!smd->domain->tvoxbig)
	{
		// TVox is for tranaparency
		smd->domain->tvoxbig = MEM_callocN(sizeof(float)*bigres[0]*bigres[1]*bigres[2], "Smoke_tVoxBig");
	}

	density = smoke_turbulence_get_density(smd->domain->wt);
	for (i = 0; i < bigres[0] * bigres[1] * bigres[2]; i++)
	{
		// Transparency computation
		// formula taken from "Visual Simulation of Smoke" / Fedkiw et al. pg. 4
		// T_vox = exp(-C_ext * h)
		// C_ext/sigma_t = density * C_ext
		smoke_set_bigtvox(smd, i, exp(-density[i] * 7.0 * smd->domain->dx / (smd->domain->amplify + 1)) );
	}
	smoke_calc_transparency(smd, light, 1);
	*/
}


