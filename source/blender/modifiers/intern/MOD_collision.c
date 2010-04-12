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

#include "DNA_scene_types.h"

#include "BLI_math.h"

#include "BKE_collision.h"
#include "BKE_cdderivedmesh.h"
#include "BKE_global.h"
#include "BKE_modifier.h"
#include "BKE_object.h"
#include "BKE_pointcache.h"


static void initData(ModifierData *md) 
{
	CollisionModifierData *collmd = (CollisionModifierData*) md;
	
	collmd->x = NULL;
	collmd->xnew = NULL;
	collmd->current_x = NULL;
	collmd->current_xnew = NULL;
	collmd->current_v = NULL;
	collmd->time = -1000;
	collmd->numverts = 0;
	collmd->bvhtree = NULL;
}

static void freeData(ModifierData *md)
{
	CollisionModifierData *collmd = (CollisionModifierData*) md;
	
	if (collmd) 
	{
		if(collmd->bvhtree)
			BLI_bvhtree_free(collmd->bvhtree);
		if(collmd->x)
			MEM_freeN(collmd->x);
		if(collmd->xnew)
			MEM_freeN(collmd->xnew);
		if(collmd->current_x)
			MEM_freeN(collmd->current_x);
		if(collmd->current_xnew)
			MEM_freeN(collmd->current_xnew);
		if(collmd->current_v)
			MEM_freeN(collmd->current_v);
		if(collmd->mfaces)
			MEM_freeN(collmd->mfaces);
		
		collmd->x = NULL;
		collmd->xnew = NULL;
		collmd->current_x = NULL;
		collmd->current_xnew = NULL;
		collmd->current_v = NULL;
		collmd->time = -1000;
		collmd->numverts = 0;
		collmd->bvhtree = NULL;
		collmd->mfaces = NULL;
	}
}

static int dependsOnTime(ModifierData *md)
{
	return 1;
}

static void deformVerts(
					  ModifierData *md, Object *ob, DerivedMesh *derivedData,
	   float (*vertexCos)[3], int numVerts, int useRenderParams, int isFinalCalc)
{
	CollisionModifierData *collmd = (CollisionModifierData*) md;
	DerivedMesh *dm = NULL;
	float current_time = 0;
	unsigned int numverts = 0, i = 0;
	MVert *tempVert = NULL;
	
	/* if possible use/create DerivedMesh */
	if(derivedData) dm = CDDM_copy(derivedData);
	else if(ob->type==OB_MESH) dm = CDDM_from_mesh(ob->data, ob);
	
	if(!ob->pd)
	{
		printf("CollisionModifier deformVerts: Should not happen!\n");
		return;
	}
	
	if(dm)
	{
		CDDM_apply_vert_coords(dm, vertexCos);
		CDDM_calc_normals(dm);
		
		current_time = bsystem_time (md->scene,  ob, ( float ) md->scene->r.cfra, 0.0 );
		
		if(G.rt > 0)
			printf("current_time %f, collmd->time %f\n", current_time, collmd->time);
		
		numverts = dm->getNumVerts ( dm );
		
		if((current_time > collmd->time)|| (BKE_ptcache_get_continue_physics()))
		{	
			// check if mesh has changed
			if(collmd->x && (numverts != collmd->numverts))
				freeData((ModifierData *)collmd);
			
			if(collmd->time == -1000) // first time
			{
				collmd->x = dm->dupVertArray(dm); // frame start position
				
				for ( i = 0; i < numverts; i++ )
				{
					// we save global positions
					mul_m4_v3( ob->obmat, collmd->x[i].co );
				}
				
				collmd->xnew = MEM_dupallocN(collmd->x); // frame end position
				collmd->current_x = MEM_dupallocN(collmd->x); // inter-frame
				collmd->current_xnew = MEM_dupallocN(collmd->x); // inter-frame
				collmd->current_v = MEM_dupallocN(collmd->x); // inter-frame

				collmd->numverts = numverts;
				
				collmd->mfaces = dm->dupFaceArray(dm);
				collmd->numfaces = dm->getNumFaces(dm);
				
				// create bounding box hierarchy
				collmd->bvhtree = bvhtree_build_from_mvert(collmd->mfaces, collmd->numfaces, collmd->x, numverts, ob->pd->pdef_sboft);
				
				collmd->time = current_time;
			}
			else if(numverts == collmd->numverts)
			{
				// put positions to old positions
				tempVert = collmd->x;
				collmd->x = collmd->xnew;
				collmd->xnew = tempVert;
				
				memcpy(collmd->xnew, dm->getVertArray(dm), numverts*sizeof(MVert));
				
				for ( i = 0; i < numverts; i++ )
				{
					// we save global positions
					mul_m4_v3( ob->obmat, collmd->xnew[i].co );
				}
				
				memcpy(collmd->current_xnew, collmd->x, numverts*sizeof(MVert));
				memcpy(collmd->current_x, collmd->x, numverts*sizeof(MVert));
				
				/* check if GUI setting has changed for bvh */
				if(collmd->bvhtree) 
				{
					if(ob->pd->pdef_sboft != BLI_bvhtree_getepsilon(collmd->bvhtree))
					{
						BLI_bvhtree_free(collmd->bvhtree);
						collmd->bvhtree = bvhtree_build_from_mvert(collmd->mfaces, collmd->numfaces, collmd->current_x, numverts, ob->pd->pdef_sboft);
					}
			
				}
				
				/* happens on file load (ONLY when i decomment changes in readfile.c) */
				if(!collmd->bvhtree)
				{
					collmd->bvhtree = bvhtree_build_from_mvert(collmd->mfaces, collmd->numfaces, collmd->current_x, numverts, ob->pd->pdef_sboft);
				}
				else
				{
					// recalc static bounding boxes
					bvhtree_update_from_mvert ( collmd->bvhtree, collmd->mfaces, collmd->numfaces, collmd->current_x, collmd->current_xnew, collmd->numverts, 1 );
				}
				
				collmd->time = current_time;
			}
			else if(numverts != collmd->numverts)
			{
				freeData((ModifierData *)collmd);
			}
			
		}
		else if(current_time < collmd->time)
		{	
			freeData((ModifierData *)collmd);
		}
		else
		{
			if(numverts != collmd->numverts)
			{
				freeData((ModifierData *)collmd);
			}
		}
	}
	
	if(dm)
		dm->release(dm);
}


ModifierTypeInfo modifierType_Collision = {
	/* name */              "Collision",
	/* structName */        "CollisionModifierData",
	/* structSize */        sizeof(CollisionModifierData),
	/* type */              eModifierTypeType_OnlyDeform,
	/* flags */             eModifierTypeFlag_AcceptsMesh
							| eModifierTypeFlag_Single,

	/* copyData */          0,
	/* deformVerts */       deformVerts,
	/* deformVertsEM */     0,
	/* deformMatricesEM */  0,
	/* applyModifier */     0,
	/* applyModifierEM */   0,
	/* initData */          initData,
	/* requiredDataMask */  0,
	/* freeData */          freeData,
	/* isDisabled */        0,
	/* updateDepgraph */    0,
	/* dependsOnTime */     dependsOnTime,
	/* foreachObjectLink */ 0,
	/* foreachIDLink */     0,
};
