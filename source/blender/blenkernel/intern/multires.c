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
 * Inc., 59 Temple Place - Suite 330, Boston, MA  02111-1307, USA.
 *
 * The Original Code is Copyright (C) 2007 by Nicholas Bishop
 * All rights reserved.
 *
 * The Original Code is: all of this file.
 *
 * Contributor(s): none yet.
 *
 * ***** END GPL LICENSE BLOCK *****
 */

#include "MEM_guardedalloc.h"

#include "DNA_key_types.h"
#include "DNA_mesh_types.h"
#include "DNA_meshdata_types.h"
#include "DNA_modifier_types.h"
#include "DNA_object_types.h"
#include "DNA_scene_types.h"
#include "DNA_view3d_types.h"

#include "BLI_math.h"
#include "BLI_blenlib.h"

#include "BKE_btex.h"
#include "BKE_cdderivedmesh.h"
#include "BKE_customdata.h"
#include "BKE_depsgraph.h"
#include "BKE_DerivedMesh.h"
#include "BKE_global.h"
#include "BKE_mesh.h"
#include "BKE_modifier.h"
#include "BKE_multires.h"
#include "BKE_object.h"
#include "BKE_subsurf.h"
#include "BKE_utildefines.h"

#include "CCGSubSurf.h"

#include <math.h>
#include <string.h>

/* MULTIRES MODIFIER */
static const int multires_max_levels = 13;
static const int multires_grid_tot[] = {1, 4, 9, 25, 81, 289, 1089, 4225, 16641, 66049, 263169, 1050625, 4198401, 16785409};
static const int multires_side_tot[] = {1, 2, 3, 5,  9,  17,  33,   65,   129,   257,   513,    1025,    2049,    4097};

static void multiresModifier_disp_run(DerivedMesh *dm, Mesh *me, int invert, int add, DMGridData **oldGridData, int totlvl);

MultiresModifierData *find_multires_modifier(Object *ob)
{
	ModifierData *md;
	MultiresModifierData *mmd = NULL;

	for(md = ob->modifiers.first; md; md = md->next) {
		if(md->type == eModifierType_Multires) {
			mmd = (MultiresModifierData*)md;
			break;
		}
	}

	return mmd;
}

static int multires_get_level(Object *ob, MultiresModifierData *mmd, int render)
{
	if(render)
		return mmd->renderlvl;
	else if(ob->mode == OB_MODE_SCULPT)
		return mmd->sculptlvl;
	else
		return mmd->lvl;
}

static void multires_set_tot_level(Object *ob, MultiresModifierData *mmd, int lvl)
{
	mmd->totlvl = lvl;

	if(ob->mode != OB_MODE_SCULPT) {
		mmd->lvl = MAX2(mmd->lvl, lvl);
		CLAMP(mmd->lvl, 0, mmd->totlvl);
	}

	mmd->sculptlvl = MAX2(mmd->sculptlvl, lvl);
	CLAMP(mmd->sculptlvl, 0, mmd->totlvl);

	mmd->renderlvl = MAX2(mmd->renderlvl, lvl);
	CLAMP(mmd->renderlvl, 0, mmd->totlvl);
}

/* XXX */
#if 0
void multiresModifier_join(Object *ob)
{
	Base *base = NULL;
	int highest_lvl = 0;

	/* First find the highest level of subdivision */
	base = FIRSTBASE;
	while(base) {
		if(TESTBASELIB_BGMODE(v3d, scene, base) && base->object->type==OB_MESH) {
			ModifierData *md;
			for(md = base->object->modifiers.first; md; md = md->next) {
				if(md->type == eModifierType_Multires) {
					int totlvl = ((MultiresModifierData*)md)->totlvl;
					if(totlvl > highest_lvl)
						highest_lvl = totlvl;

					/* Ensure that all updates are processed */
					multires_force_update(base->object);
				}
			}
		}
		base = base->next;
	}

	/* No multires meshes selected */
	if(highest_lvl == 0)
		return;

	/* Subdivide all the displacements to the highest level */
	base = FIRSTBASE;
	while(base) {
		if(TESTBASELIB_BGMODE(v3d, scene, base) && base->object->type==OB_MESH) {
			ModifierData *md = NULL;
			MultiresModifierData *mmd = NULL;

			for(md = base->object->modifiers.first; md; md = md->next) {
				if(md->type == eModifierType_Multires)
					mmd = (MultiresModifierData*)md;
			}

			/* If the object didn't have multires enabled, give it a new modifier */
			if(!mmd) {
				md = base->object->modifiers.first;
				
				while(md && modifierType_getInfo(md->type)->type == eModifierTypeType_OnlyDeform)
					md = md->next;
				
				mmd = (MultiresModifierData*)modifier_new(eModifierType_Multires);
				BLI_insertlinkbefore(&base->object->modifiers, md, mmd);
				modifier_unique_name(&base->object->modifiers, mmd);
			}

			if(mmd)
				multiresModifier_subdivide(mmd, base->object, highest_lvl - mmd->totlvl, 0, 0);
		}
		base = base->next;
	}
}
#endif

/* Returns 0 on success, 1 if the src's totvert doesn't match */
int multiresModifier_reshape(MultiresModifierData *mmd, Object *dst, Object *src)
{
	/* XXX */
#if 0
	Mesh *src_me = get_mesh(src);
	DerivedMesh *mrdm = dst->derivedFinal;

	if(mrdm && mrdm->getNumVerts(mrdm) == src_me->totvert) {
		MVert *mvert = CDDM_get_verts(mrdm);
		int i;

		for(i = 0; i < src_me->totvert; ++i)
			copy_v3_v3(mvert[i].co, src_me->mvert[i].co);
		mrdm->needsFree = 1;
		MultiresDM_mark_as_modified(mrdm);
		mrdm->release(mrdm);
		dst->derivedFinal = NULL;

		return 0;
	}
#endif

	return 1;
}

static void column_vectors_to_mat3(float mat[][3], float v1[3], float v2[3], float v3[3])
{
	copy_v3_v3(mat[0], v1);
	copy_v3_v3(mat[1], v2);
	copy_v3_v3(mat[2], v3);
}

static void multires_copy_grid(float (*gridA)[3], float (*gridB)[3], int sizeA, int sizeB)
{
	int x, y, j, skip;

	if(sizeA > sizeB) {
		skip = (sizeA-1)/(sizeB-1);

		for(j = 0, y = 0; y < sizeB; y++)
			for(x = 0; x < sizeB; x++, j++)
				copy_v3_v3(gridA[y*skip*sizeA + x*skip], gridB[j]);
	}
	else {
		skip = (sizeB-1)/(sizeA-1);

		for(j = 0, y = 0; y < sizeA; y++)
			for(x = 0; x < sizeA; x++, j++)
				copy_v3_v3(gridA[j], gridB[y*skip*sizeB + x*skip]);
	}
}

static void multires_copy_dm_grid(DMGridData *gridA, DMGridData *gridB, int sizeA, int sizeB)
{
	int x, y, j, skip;

	if(sizeA > sizeB) {
		skip = (sizeA-1)/(sizeB-1);

		for(j = 0, y = 0; y < sizeB; y++)
			for(x = 0; x < sizeB; x++, j++)
				copy_v3_v3(gridA[y*skip*sizeA + x*skip].co, gridB[j].co);
	}
	else {
		skip = (sizeB-1)/(sizeA-1);

		for(j = 0, y = 0; y < sizeA; y++)
			for(x = 0; x < sizeA; x++, j++)
				copy_v3_v3(gridA[j].co, gridB[y*skip*sizeB + x*skip].co);
	}
}

/* direction=1 for delete higher, direction=0 for lower (not implemented yet) */
void multiresModifier_del_levels(struct MultiresModifierData *mmd, struct Object *ob, int direction)
{
	Mesh *me = get_mesh(ob);
	int lvl = multires_get_level(ob, mmd, 0);
	int levels = mmd->totlvl - lvl;
	MDisps *mdisps;
	
	CustomData_external_read(&me->fdata, CD_MASK_MDISPS, me->totface);
	mdisps= CustomData_get_layer(&me->fdata, CD_MDISPS);

	multires_force_update(ob);

	if(mdisps && levels > 0 && direction == 1) {
		int nsize = multires_side_tot[lvl];
		int hsize = multires_side_tot[mmd->totlvl];
		int i;

		for(i = 0; i < me->totface; ++i) {
			MDisps *mdisp= &mdisps[i];
			float (*disps)[3], (*ndisps)[3], (*hdisps)[3];
			int nvert = (me->mface[i].v4)? 4: 3;
			int totdisp = multires_grid_tot[lvl]*nvert;
			int S;

			disps = MEM_callocN(sizeof(float) * 3 * totdisp, "multires disps");

			ndisps = disps;
			hdisps = mdisp->disps;

			for(S = 0; S < nvert; S++) {
				multires_copy_grid(ndisps, hdisps, nsize, hsize);

				ndisps += nsize*nsize;
				hdisps += hsize*hsize;
			}

			MEM_freeN(mdisp->disps);
			mdisp->disps = disps;
			mdisp->totdisp = totdisp;
		}
	}

	multires_set_tot_level(ob, mmd, lvl);
}

static DerivedMesh *multires_dm_create_local(Object *ob, DerivedMesh *dm, int lvl, int totlvl, int simple)
{
	MultiresModifierData mmd;

	memset(&mmd, 0, sizeof(MultiresModifierData));
	mmd.lvl = lvl;
	mmd.sculptlvl = lvl;
	mmd.renderlvl = lvl;
	mmd.totlvl = totlvl;
	mmd.simple = simple;

	return multires_dm_create_from_derived(&mmd, 1, dm, ob, 0, 0);
}

static DerivedMesh *subsurf_dm_create_local(Object *ob, DerivedMesh *dm, int lvl, int simple)
{
	SubsurfModifierData smd;

	memset(&smd, 0, sizeof(SubsurfModifierData));
	smd.levels = smd.renderLevels = lvl;
	smd.flags |= eSubsurfModifierFlag_SubsurfUv;
	if(simple)
		smd.subdivType = ME_SIMPLE_SUBSURF;

	return subsurf_make_derived_from_derived(dm, &smd, 0, NULL, 0, 0);
}

static void multires_reallocate_mdisps(Mesh *me, MDisps *mdisps, int lvl)
{
	int i;

	/* reallocate displacements to be filled in */
	for(i = 0; i < me->totface; ++i) {
		int nvert = (me->mface[i].v4)? 4: 3;
		int totdisp = multires_grid_tot[lvl]*nvert;
		float (*disps)[3] = MEM_callocN(sizeof(float) * 3 * totdisp, "multires disps");

		if(mdisps[i].disps)
			MEM_freeN(mdisps[i].disps);

		mdisps[i].disps = disps;
		mdisps[i].totdisp = totdisp;
	}
}

void multiresModifier_subdivide(MultiresModifierData *mmd, Object *ob, int updateblock, int simple)
{
	Mesh *me = ob->data;
	MDisps *mdisps;
	int lvl= mmd->totlvl;
	int totlvl= mmd->totlvl+1;

	if(totlvl > multires_max_levels)
		return;

	multires_force_update(ob);

	mdisps = CustomData_get_layer(&me->fdata, CD_MDISPS);
	if(!mdisps)
		mdisps = CustomData_add_layer(&me->fdata, CD_MDISPS, CD_DEFAULT, NULL, me->totface);

	if(mdisps->disps && !updateblock && totlvl > 1) {
		/* upsample */
		DerivedMesh *lowdm, *cddm, *highdm;
		DMGridData **highGridData, **lowGridData, **subGridData;
		CCGSubSurf *ss;
		int i, numGrids, highGridSize, lowGridSize;

		/* create subsurf DM from original mesh at high level */
		cddm = CDDM_from_mesh(me, NULL);
		highdm = subsurf_dm_create_local(ob, cddm, totlvl, simple);

		/* create multires DM from original mesh at low level */
		lowdm = multires_dm_create_local(ob, cddm, lvl, lvl, simple);
		cddm->release(cddm);

		/* copy subsurf grids and replace them with low displaced grids */
		numGrids = highdm->getNumGrids(highdm);
		highGridSize = highdm->getGridSize(highdm);
		highGridData = highdm->getGridData(highdm);
		lowGridSize = lowdm->getGridSize(lowdm);
		lowGridData = lowdm->getGridData(lowdm);

		subGridData = MEM_callocN(sizeof(float*)*numGrids, "subGridData*");

		for(i = 0; i < numGrids; ++i) {
			/* backup subsurf grids */
			subGridData[i] = MEM_callocN(sizeof(DMGridData)*highGridSize*highGridSize, "subGridData");
			memcpy(subGridData[i], highGridData[i], sizeof(DMGridData)*highGridSize*highGridSize);

			/* overwrite with current displaced grids */
			multires_copy_dm_grid(highGridData[i], lowGridData[i], highGridSize, lowGridSize);
		}

		/* low lower level dm no longer needed at this point */
		lowdm->release(lowdm);

		/* subsurf higher levels again with displaced data */
		ss= ((CCGDerivedMesh*)highdm)->ss;
		ccgSubSurf_updateFromFaces(ss, lvl, NULL, 0);
		ccgSubSurf_updateLevels(ss, lvl, NULL, 0);

		/* reallocate displacements */
		multires_reallocate_mdisps(me, mdisps, totlvl); 

		/* compute displacements */
		multiresModifier_disp_run(highdm, me, 1, 0, subGridData, totlvl);

		/* free */
		highdm->release(highdm);
		for(i = 0; i < numGrids; ++i)
			MEM_freeN(subGridData[i]);
		MEM_freeN(subGridData);
	}
	else {
		/* only reallocate, nothing to upsample */
		multires_reallocate_mdisps(me, mdisps, totlvl); 
	}

	multires_set_tot_level(ob, mmd, totlvl);
}

static void grid_adjacent_rotate(int rotation, int gridSize, int *x, int *y)
{
	/* we rotate (rotation * 90°) counterclockwise around center */
	int nx, ny;

	switch(rotation) {
		case 0: nx = *x; ny = *y; break;
		case 1: nx = *y; ny = *x; break;
		case 2: nx = *x; ny = *y; break; //gridSize - 1 - *x; ny = gridSize - 1 - *y; break;
		case 3: nx = *y; ny = *x; break;
	}

	*x = nx;
	*y = ny;
}

static void grid_adjacent_jump(DMGridAdjacency *adj, int gridSize, int *index, int *x, int *y)
{
	if(*x < 0) {
		if(adj->index[3] == -1) {
			/* no adjacent grid, clamp */
			*x = 0;
		}
		else {
			/* jump to adjacent grid */
			*index = adj->index[3];
			*x += gridSize;
			grid_adjacent_rotate(adj->rotation[3], gridSize, x, y);
		}
	}
	else if(*x >= gridSize) {
		if(adj->index[1] == -1) {
			/* no adjacent grid, take a step back */
			*x = gridSize - 1;
		}
		else {
			/* jump to adjacent grid */
			*index = adj->index[1];
			*x -= gridSize;
			grid_adjacent_rotate(adj->rotation[1], gridSize, x, y);
		}
	}
	else if(*y < 0) {
		if(adj->index[0] == -1) {
			/* no adjacent grid, clamp */
			*y = 0;
		}
		else {
			/* jump to adjacent grid */
			*index = adj->index[0];
			*y += gridSize;
			grid_adjacent_rotate(adj->rotation[0], gridSize, x, y);
		}
	}
	else if(*y >= gridSize) {
		if(adj->index[2] == -1) {
			/* no adjacent grid, take a step back */
			*y = gridSize - 1;
		}
		else {
			/* jump to adjacent grid */
			*index = adj->index[2];
			*y -= gridSize;
			grid_adjacent_rotate(adj->rotation[2], gridSize, x, y);
		}
	}
}

static void grid_tangent(DMGridAdjacency *adj, int gridSize, int index, int x, int y, int axis, DMGridData **gridData, float t[3])
{
	int jindex = index, jx = x, jy = y;

	if(axis == 0) {
		if(adj->index[1] == -1 && x == gridSize - 1) {
			if(adj->index[2] == -1 && y == gridSize - 1)
				sub_v3_v3v3(t, gridData[index][x + gridSize*(y - 1)].co, gridData[index][x - 1 + gridSize*(y - 1)].co);
			else
				sub_v3_v3v3(t, gridData[index][x + gridSize*y].co, gridData[index][x - 1 + gridSize*y].co);
		}
		else {
			jx += 1;
			grid_adjacent_jump(adj, gridSize, &jindex, &jx, &jy);
			sub_v3_v3v3(t, gridData[jindex][jx + gridSize*jy].co, gridData[index][x + gridSize*y].co);
		}
	}
	else if(axis == 1) {
		if(adj->index[2] == -1 && y == gridSize - 1) {
			if(adj->index[1] == -1 && x == gridSize - 1) {
				sub_v3_v3v3(t, gridData[index][x - 1 + gridSize*y].co, gridData[index][x - 1 + gridSize*(y - 1)].co);
			}
			else {
				sub_v3_v3v3(t, gridData[index][x + gridSize*y].co, gridData[index][x + gridSize*(y - 1)].co);
			}
		}
		else {
			jy += 1;
			grid_adjacent_jump(adj, gridSize, &jindex, &jx, &jy);
			sub_v3_v3v3(t, gridData[jindex][jx + gridSize*jy].co, gridData[index][x + gridSize*y].co);
		}
	}
}

static void multiresModifier_disp_run(DerivedMesh *dm, Mesh *me, int invert, int add, DMGridData **oldGridData, int totlvl)
{
	CCGDerivedMesh *ccgdm = (CCGDerivedMesh*)dm;
	DMGridData **gridData, **subGridData;
	DMGridAdjacency *gridAdjacency;
	MFace *mface = me->mface;
	MDisps *mdisps = CustomData_get_layer(&me->fdata, CD_MDISPS);
	int i, S, x, y, numGrids, gIndex, gridSize, dGridSize, dSkip;

	numGrids = dm->getNumGrids(dm);
	gridSize = dm->getGridSize(dm);
	gridData = dm->getGridData(dm);
	gridAdjacency = dm->getGridAdjacency(dm);
	subGridData = (oldGridData)? oldGridData: gridData;

	dGridSize = multires_side_tot[totlvl];
	dSkip = (dGridSize-1)/(gridSize-1);

	for(gIndex = 0, i = 0; i < me->totface; ++i) {
		const int numVerts = mface[i].v4 ? 4 : 3;
		MDisps *mdisp = &mdisps[i];

		for(S = 0; S < numVerts; ++S, ++gIndex) {
			DMGridData *grid = gridData[gIndex];
			DMGridData *subgrid = subGridData[gIndex];
			DMGridAdjacency *adj = &gridAdjacency[gIndex];
			float (*dispgrid)[3] = &mdisp->disps[S*dGridSize*dGridSize];

			for(y = 0; y < gridSize; y++) {
				for(x = 0; x < gridSize; x++) {
					float *co = grid[x + y*gridSize].co;
					float *sco = subgrid[x + y*gridSize].co;
					float *no = subgrid[x + y*gridSize].no;
					float *data = dispgrid[dGridSize*y*dSkip + x*dSkip];
					float mat[3][3], tx[3], ty[3], disp[3], d[3];

					/* construct tangent space matrix */
					grid_tangent(adj, gridSize, gIndex, x, y, 0, subGridData, tx);
					normalize_v3(tx);

					grid_tangent(adj, gridSize, gIndex, x, y, 1, subGridData, ty);
					normalize_v3(ty);

					column_vectors_to_mat3(mat, tx, ty, no);

					if(!invert) {
						/* convert to object space and add */
						mul_v3_m3v3(disp, mat, data);
						add_v3_v3v3(co, sco, disp);
					}
					else if(!add) {
						/* convert difference to tangent space */
						sub_v3_v3v3(disp, co, sco);
						invert_m3(mat);
						mul_v3_m3v3(data, mat, disp);
					}
					else {
						/* convert difference to tangent space */
						invert_m3(mat);
						mul_v3_m3v3(d, mat, co);
						add_v3_v3(data, d);
					}
				}
			}
		}
	}

	if(!invert) {
		ccgSubSurf_stitchFaces(ccgdm->ss, 0, NULL, 0);
		ccgSubSurf_updateNormals(ccgdm->ss, NULL, 0);
	}
}

static void multiresModifier_update(DerivedMesh *dm)
{
	CCGDerivedMesh *ccgdm= (CCGDerivedMesh*)dm;
	Object *ob;
	Mesh *me;
	MDisps *mdisps;
	MultiresModifierData *mmd;

	ob = ccgdm->multires.ob;
	me = ccgdm->multires.ob->data;
	mmd = ccgdm->multires.mmd;
	CustomData_external_read(&me->fdata, CD_MASK_MDISPS, me->totface);
	mdisps = CustomData_get_layer(&me->fdata, CD_MDISPS);

	if(mdisps) {
		int lvl = ccgdm->multires.lvl;
		int totlvl = ccgdm->multires.totlvl;
		
		if(lvl < totlvl) {
			Mesh *me = ob->data;
			DerivedMesh *lowdm, *cddm, *highdm;
			DMGridData **highGridData, **lowGridData, **subGridData, **gridData, *diffGrid;
			CCGSubSurf *ss;
			int i, j, numGrids, highGridSize, lowGridSize;

			/* create subsurf DM from original mesh at high level */
			cddm = CDDM_from_mesh(me, NULL);
			highdm = subsurf_dm_create_local(ob, cddm, totlvl, mmd->simple);

			/* create multires DM from original mesh and displacements */
			lowdm = multires_dm_create_local(ob, cddm, lvl, totlvl, mmd->simple);
			cddm->release(cddm);

			/* gather grid data */
			numGrids = highdm->getNumGrids(highdm);
			highGridSize = highdm->getGridSize(highdm);
			highGridData = highdm->getGridData(highdm);
			lowGridSize = lowdm->getGridSize(lowdm);
			lowGridData = lowdm->getGridData(lowdm);
			gridData = dm->getGridData(dm);

			subGridData = MEM_callocN(sizeof(DMGridData*)*numGrids, "subGridData*");
			diffGrid = MEM_callocN(sizeof(DMGridData)*lowGridSize*lowGridSize, "diff");

			for(i = 0; i < numGrids; ++i) {
				/* backup subsurf grids */
				subGridData[i] = MEM_callocN(sizeof(DMGridData)*highGridSize*highGridSize, "subGridData");
				memcpy(subGridData[i], highGridData[i], sizeof(DMGridData)*highGridSize*highGridSize);

				/* write difference of subsurf and displaced low level into high subsurf */
				for(j = 0; j < lowGridSize*lowGridSize; ++j)
					sub_v3_v3v3(diffGrid[j].co, gridData[i][j].co, lowGridData[i][j].co);

				multires_copy_dm_grid(highGridData[i], diffGrid, highGridSize, lowGridSize);
			}

			/* lower level dm no longer needed at this point */
			MEM_freeN(diffGrid);
			lowdm->release(lowdm);

			/* subsurf higher levels again with difference of coordinates */
			ss= ((CCGDerivedMesh*)highdm)->ss;
			ccgSubSurf_updateFromFaces(ss, lvl, NULL, 0);
			ccgSubSurf_updateLevels(ss, lvl, NULL, 0);

			/* add to displacements */
			multiresModifier_disp_run(highdm, me, 1, 1, subGridData, mmd->totlvl);

			/* free */
			highdm->release(highdm);
			for(i = 0; i < numGrids; ++i)
				MEM_freeN(subGridData[i]);
			MEM_freeN(subGridData);
		}
		else {
			DerivedMesh *cddm, *subdm;

			cddm = CDDM_from_mesh(me, NULL);
			subdm = subsurf_dm_create_local(ob, cddm, mmd->totlvl, mmd->simple);
			cddm->release(cddm);

			multiresModifier_disp_run(dm, me, 1, 0, subdm->getGridData(subdm), mmd->totlvl);

			subdm->release(subdm);
		}
	}
}

void multires_mark_as_modified(struct Object *ob)
{
	if(ob && ob->derivedFinal) {
		CCGDerivedMesh *ccgdm = (CCGDerivedMesh*)ob->derivedFinal;
		ccgdm->multires.modified = 1;
	}
}

void multires_force_update(Object *ob)
{
	if(ob && ob->derivedFinal) {
		ob->derivedFinal->needsFree =1;
		ob->derivedFinal->release(ob->derivedFinal);
		ob->derivedFinal = NULL;
	}
}

struct DerivedMesh *multires_dm_create_from_derived(MultiresModifierData *mmd, int local_mmd, DerivedMesh *dm, Object *ob,
						    int useRenderParams, int isFinalCalc)
{
	Mesh *me= ob->data;
	DerivedMesh *result;
	CCGDerivedMesh *ccgdm;
	DMGridData **gridData, **subGridData;
	int lvl= multires_get_level(ob, mmd, useRenderParams);
	int i, gridSize, numGrids;

	if(lvl == 0)
		return dm;

	result = subsurf_dm_create_local(ob, dm, lvl, 0);

	if(!local_mmd) {
		ccgdm = (CCGDerivedMesh*)result;

		ccgdm->multires.ob = ob;
		ccgdm->multires.mmd = mmd;
		ccgdm->multires.local_mmd = local_mmd;
		ccgdm->multires.lvl = lvl;
		ccgdm->multires.totlvl = mmd->totlvl;
		ccgdm->multires.modified = 0;
		ccgdm->multires.update = multiresModifier_update;
	}

	numGrids = result->getNumGrids(result);
	gridSize = result->getGridSize(result);
	gridData = result->getGridData(result);

	subGridData = MEM_callocN(sizeof(DMGridData*)*numGrids, "subGridData*");

	for(i = 0; i < numGrids; i++) {
		subGridData[i] = MEM_callocN(sizeof(DMGridData)*gridSize*gridSize, "subGridData");
		memcpy(subGridData[i], gridData[i], sizeof(DMGridData)*gridSize*gridSize);
	}

	CustomData_external_read(&me->fdata, CD_MASK_MDISPS, me->totface);
	multiresModifier_disp_run(result, ob->data, 0, 0, subGridData, mmd->totlvl);

	for(i = 0; i < numGrids; i++)
		MEM_freeN(subGridData[i]);
	MEM_freeN(subGridData);

	return result;
}

/**** Old Multires code ****
***************************/

/* Does not actually free lvl itself */
static void multires_free_level(MultiresLevel *lvl)
{
	if(lvl) {
		if(lvl->faces) MEM_freeN(lvl->faces);
		if(lvl->edges) MEM_freeN(lvl->edges);
		if(lvl->colfaces) MEM_freeN(lvl->colfaces);
	}
}

void multires_free(Multires *mr)
{
	if(mr) {
		MultiresLevel* lvl= mr->levels.first;

		/* Free the first-level data */
		if(lvl) {
			CustomData_free(&mr->vdata, lvl->totvert);
			CustomData_free(&mr->fdata, lvl->totface);
			if(mr->edge_flags)
				MEM_freeN(mr->edge_flags);
			if(mr->edge_creases)
				MEM_freeN(mr->edge_creases);
		}

		while(lvl) {
			multires_free_level(lvl);			
			lvl= lvl->next;
		}

		MEM_freeN(mr->verts);

		BLI_freelistN(&mr->levels);

		MEM_freeN(mr);
	}
}

static void create_old_vert_face_map(ListBase **map, IndexNode **mem, const MultiresFace *mface,
				     const int totvert, const int totface)
{
	int i,j;
	IndexNode *node = NULL;
	
	(*map) = MEM_callocN(sizeof(ListBase) * totvert, "vert face map");
	(*mem) = MEM_callocN(sizeof(IndexNode) * totface*4, "vert face map mem");
	node = *mem;
	
	/* Find the users */
	for(i = 0; i < totface; ++i){
		for(j = 0; j < (mface[i].v[3]?4:3); ++j, ++node) {
			node->index = i;
			BLI_addtail(&(*map)[mface[i].v[j]], node);
		}
	}
}

static void create_old_vert_edge_map(ListBase **map, IndexNode **mem, const MultiresEdge *medge,
				     const int totvert, const int totedge)
{
	int i,j;
	IndexNode *node = NULL;
	
	(*map) = MEM_callocN(sizeof(ListBase) * totvert, "vert edge map");
	(*mem) = MEM_callocN(sizeof(IndexNode) * totedge*2, "vert edge map mem");
	node = *mem;
	
	/* Find the users */
	for(i = 0; i < totedge; ++i){
		for(j = 0; j < 2; ++j, ++node) {
			node->index = i;
			BLI_addtail(&(*map)[medge[i].v[j]], node);
		}
	}
}

static MultiresFace *find_old_face(ListBase *map, MultiresFace *faces, int v1, int v2, int v3, int v4)
{
	IndexNode *n1;
	int v[4] = {v1, v2, v3, v4}, i, j;

	for(n1 = map[v1].first; n1; n1 = n1->next) {
		int fnd[4] = {0, 0, 0, 0};

		for(i = 0; i < 4; ++i) {
			for(j = 0; j < 4; ++j) {
				if(v[i] == faces[n1->index].v[j])
					fnd[i] = 1;
			}
		}

		if(fnd[0] && fnd[1] && fnd[2] && fnd[3])
			return &faces[n1->index];
	}

	return NULL;
}

static MultiresEdge *find_old_edge(ListBase *map, MultiresEdge *edges, int v1, int v2)
{
	IndexNode *n1, *n2;

	for(n1 = map[v1].first; n1; n1 = n1->next) {
		for(n2 = map[v2].first; n2; n2 = n2->next) {
			if(n1->index == n2->index)
				return &edges[n1->index];
		}
	}

	return NULL;
}

static void multires_load_old_edges(ListBase **emap, MultiresLevel *lvl, int *vvmap, int dst, int v1, int v2, int mov)
{
	int emid = find_old_edge(emap[2], lvl->edges, v1, v2)->mid;
	vvmap[dst + mov] = emid;

	if(lvl->next->next) {
		multires_load_old_edges(emap + 1, lvl->next, vvmap, dst + mov, v1, emid, mov / 2);
		multires_load_old_edges(emap + 1, lvl->next, vvmap, dst + mov, v2, emid, -mov / 2);
	}
}

static void multires_load_old_faces(ListBase **fmap, ListBase **emap, MultiresLevel *lvl, int *vvmap, int dst,
				    int v1, int v2, int v3, int v4, int st2, int st3)
{
	int fmid;
	int emid13, emid14, emid23, emid24;

	if(lvl && lvl->next) {
		fmid = find_old_face(fmap[1], lvl->faces, v1, v2, v3, v4)->mid;
		vvmap[dst] = fmid;

		emid13 = find_old_edge(emap[1], lvl->edges, v1, v3)->mid;
		emid14 = find_old_edge(emap[1], lvl->edges, v1, v4)->mid;
		emid23 = find_old_edge(emap[1], lvl->edges, v2, v3)->mid;
		emid24 = find_old_edge(emap[1], lvl->edges, v2, v4)->mid;


		multires_load_old_faces(fmap + 1, emap + 1, lvl->next, vvmap, dst + st2 * st3 + st3,
					fmid, v2, emid23, emid24, st2, st3 / 2);

		multires_load_old_faces(fmap + 1, emap + 1, lvl->next, vvmap, dst - st2 * st3 + st3,
					emid14, emid24, fmid, v4, st2, st3 / 2);

		multires_load_old_faces(fmap + 1, emap + 1, lvl->next, vvmap, dst + st2 * st3 - st3,
					emid13, emid23, v3, fmid, st2, st3 / 2);

		multires_load_old_faces(fmap + 1, emap + 1, lvl->next, vvmap, dst - st2 * st3 - st3,
					v1, fmid, emid13, emid14, st2, st3 / 2);

		if(lvl->next->next) {
			multires_load_old_edges(emap, lvl->next, vvmap, dst, emid24, fmid, st3);
			multires_load_old_edges(emap, lvl->next, vvmap, dst, emid13, fmid, -st3);
			multires_load_old_edges(emap, lvl->next, vvmap, dst, emid14, fmid, -st2 * st3);
			multires_load_old_edges(emap, lvl->next, vvmap, dst, emid23, fmid, st2 * st3);
		}
	}
}

/* Loads a multires object stored in the old Multires struct into the new format */
void multires_load_old(DerivedMesh *dm, Multires *mr)
{
	MultiresLevel *lvl, *lvl1;
	MVert *vsrc, *vdst;
	int src, dst;
	int totlvl = 2; // XXX MultiresDM_get_totlvl(dm);
	int st = multires_side_tot[totlvl - 1] - 1;
	int extedgelen = multires_side_tot[totlvl] - 2;
	int *vvmap; // inorder for dst, map to src
	int crossedgelen;
	int i, j, s, x, totvert, tottri, totquad;

	src = 0;
	dst = 0;
	vsrc = mr->verts;
	vdst = CDDM_get_verts(dm);
	totvert = dm->getNumVerts(dm);
	vvmap = MEM_callocN(sizeof(int) * totvert, "multires vvmap");

	lvl1 = mr->levels.first;
	/* Load base verts */
	for(i = 0; i < lvl1->totvert; ++i) {
		vvmap[totvert - lvl1->totvert + i] = src;
		++src;
	}

	/* Original edges */
	dst = totvert - lvl1->totvert - extedgelen * lvl1->totedge;
	for(i = 0; i < lvl1->totedge; ++i) {
		int ldst = dst + extedgelen * i;
		int lsrc = src;
		lvl = lvl1->next;

		for(j = 2; j <= mr->level_count; ++j) {
			int base = multires_side_tot[totlvl - j + 1] - 2;
			int skip = multires_side_tot[totlvl - j + 2] - 1;
			int st = multires_side_tot[j - 1] - 1;

			for(x = 0; x < st; ++x)
				vvmap[ldst + base + x * skip] = lsrc + st * i + x;

			lsrc += lvl->totvert - lvl->prev->totvert;
			lvl = lvl->next;
		}
	}

	/* Center points */
	dst = 0;
	for(i = 0; i < lvl1->totface; ++i) {
		int sides = lvl1->faces[i].v[3] ? 4 : 3;

		vvmap[dst] = src + lvl1->totedge + i;
		dst += 1 + sides * (st - 1) * st;
	}


	/* The rest is only for level 3 and up */
	if(lvl1->next && lvl1->next->next) {
		ListBase **fmap, **emap;
		IndexNode **fmem, **emem;

		/* Face edge cross */
		tottri = totquad = 0;
		crossedgelen = multires_side_tot[totlvl - 1] - 2;
		dst = 0;
		for(i = 0; i < lvl1->totface; ++i) {
			int sides = lvl1->faces[i].v[3] ? 4 : 3;

			lvl = lvl1->next->next;
			++dst;

			for(j = 3; j <= mr->level_count; ++j) {
				int base = multires_side_tot[totlvl - j + 1] - 2;
				int skip = multires_side_tot[totlvl - j + 2] - 1;
				int st = pow(2, j - 2);
				int st2 = pow(2, j - 3);
				int lsrc = lvl->prev->totvert;

				/* Skip exterior edge verts */
				lsrc += lvl1->totedge * st;

				/* Skip earlier face edge crosses */
				lsrc += st2 * (tottri * 3 + totquad * 4);

				for(s = 0; s < sides; ++s) {
					for(x = 0; x < st2; ++x) {
						vvmap[dst + crossedgelen * (s + 1) - base - x * skip - 1] = lsrc;
						++lsrc;
					}
				}

				lvl = lvl->next;
			}

			dst += sides * (st - 1) * st;

			if(sides == 4) ++totquad;
			else ++tottri;

		}

		/* calculate vert to edge/face maps for each level (except the last) */
		fmap = MEM_callocN(sizeof(ListBase*) * (mr->level_count-1), "multires fmap");
		emap = MEM_callocN(sizeof(ListBase*) * (mr->level_count-1), "multires emap");
		fmem = MEM_callocN(sizeof(IndexNode*) * (mr->level_count-1), "multires fmem");
		emem = MEM_callocN(sizeof(IndexNode*) * (mr->level_count-1), "multires emem");
		lvl = lvl1;
		for(i = 0; i < mr->level_count - 1; ++i) {
			create_old_vert_face_map(fmap + i, fmem + i, lvl->faces, lvl->totvert, lvl->totface);
			create_old_vert_edge_map(emap + i, emem + i, lvl->edges, lvl->totvert, lvl->totedge);
			lvl = lvl->next;
		}

		/* Interior face verts */
		lvl = lvl1->next->next;
		dst = 0;
		for(j = 0; j < lvl1->totface; ++j) {
			int sides = lvl1->faces[j].v[3] ? 4 : 3;
			int ldst = dst + 1 + sides * (st - 1);

			for(s = 0; s < sides; ++s) {
				int st2 = multires_side_tot[totlvl - 1] - 2;
				int st3 = multires_side_tot[totlvl - 2] - 2;
				int st4 = st3 == 0 ? 1 : (st3 + 1) / 2;
				int mid = ldst + st2 * st3 + st3;
				int cv = lvl1->faces[j].v[s];
				int nv = lvl1->faces[j].v[s == sides - 1 ? 0 : s + 1];
				int pv = lvl1->faces[j].v[s == 0 ? sides - 1 : s - 1];

				multires_load_old_faces(fmap, emap, lvl1->next, vvmap, mid,
							vvmap[dst], cv,
							find_old_edge(emap[0], lvl1->edges, pv, cv)->mid,
							find_old_edge(emap[0], lvl1->edges, cv, nv)->mid,
							st2, st4);

				ldst += (st - 1) * (st - 1);
			}


			dst = ldst;
		}

		lvl = lvl->next;

		for(i = 0; i < mr->level_count - 1; ++i) {
			MEM_freeN(fmap[i]);
			MEM_freeN(fmem[i]);
			MEM_freeN(emap[i]);
			MEM_freeN(emem[i]);
		}

		MEM_freeN(fmap);
		MEM_freeN(emap);
		MEM_freeN(fmem);
		MEM_freeN(emem);
	}

	/* Transfer verts */
	for(i = 0; i < totvert; ++i)
		copy_v3_v3(vdst[i].co, vsrc[vvmap[i]].co);

	MEM_freeN(vvmap);
}

