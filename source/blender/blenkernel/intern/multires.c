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
 * The Original Code is Copyright (C) 2007 by Nicholas Bishop
 * All rights reserved.
 *
 * The Original Code is: all of this file.
 *
 * Contributor(s): none yet.
 *
 * ***** END GPL LICENSE BLOCK *****
 */

/** \file blender/blenkernel/intern/multires.c
 *  \ingroup bke
 */


#include "MEM_guardedalloc.h"

/* for reading old multires */
#define DNA_DEPRECATED_ALLOW

#include "DNA_mesh_types.h"
#include "DNA_meshdata_types.h"
#include "DNA_object_types.h"
#include "DNA_scene_types.h"

#include "BLI_bitmap.h"
#include "BLI_blenlib.h"
#include "BLI_math.h"
#include "BLI_utildefines.h"

#include "BKE_pbvh.h"
#include "BKE_ccg.h"
#include "BKE_cdderivedmesh.h"
#include "BKE_mesh.h"
#include "BKE_mesh_mapping.h"
#include "BKE_modifier.h"
#include "BKE_multires.h"
#include "BKE_paint.h"
#include "BKE_scene.h"
#include "BKE_subsurf.h"
#include "BKE_editmesh.h"

#include "BKE_object.h"

#include "CCGSubSurf.h"

#include <math.h>
#include <string.h>

/* MULTIRES MODIFIER */
static const int multires_max_levels = 13;
static const int multires_grid_tot[] = {0, 4, 9, 25, 81, 289, 1089, 4225, 16641, 66049, 263169, 1050625, 4198401, 16785409};
static const int multires_side_tot[] = {0, 2, 3, 5,  9,  17,  33,   65,   129,   257,   513,    1025,    2049,    4097};

/* See multiresModifier_disp_run for description of each operation */
typedef enum {
	APPLY_DISPLACEMENTS,
	CALC_DISPLACEMENTS,
	ADD_DISPLACEMENTS,
} DispOp;

static void multires_mvert_to_ss(DerivedMesh *dm, MVert *mvert);
static void multiresModifier_disp_run(DerivedMesh *dm, Mesh *me, DerivedMesh *dm2, DispOp op, CCGElem **oldGridData, int totlvl);

/** Customdata **/

void multires_customdata_delete(Mesh *me)
{
	if (me->edit_btmesh) {
		BMEditMesh *em = me->edit_btmesh;
		/* CustomData_external_remove is used here only to mark layer
		 * as non-external for further free-ing, so zero element count
		 * looks safer than em->totface */
		CustomData_external_remove(&em->bm->ldata, &me->id,
		                           CD_MDISPS, 0);
		BM_data_layer_free(em->bm, &em->bm->ldata, CD_MDISPS);

		BM_data_layer_free(em->bm, &em->bm->ldata, CD_GRID_PAINT_MASK);
	}
	else {
		CustomData_external_remove(&me->ldata, &me->id,
		                           CD_MDISPS, me->totloop);
		CustomData_free_layer_active(&me->ldata, CD_MDISPS,
		                             me->totloop);

		CustomData_free_layer_active(&me->ldata, CD_GRID_PAINT_MASK,
		                             me->totloop);
	}
}

/** Grid hiding **/
static BLI_bitmap *multires_mdisps_upsample_hidden(BLI_bitmap *lo_hidden,
                                                   int lo_level,
                                                   int hi_level,

                                                   /* assumed to be at hi_level (or
                                                    *  null) */
                                                   BLI_bitmap *prev_hidden)
{
	BLI_bitmap *subd;
	int hi_gridsize = BKE_ccg_gridsize(hi_level);
	int lo_gridsize = BKE_ccg_gridsize(lo_level);
	int yh, xh, xl, yl, xo, yo, hi_ndx;
	int offset, factor;

	BLI_assert(lo_level <= hi_level);

	/* fast case */
	if (lo_level == hi_level)
		return MEM_dupallocN(lo_hidden);

	subd = BLI_BITMAP_NEW(hi_gridsize * hi_gridsize, "MDisps.hidden upsample");

	factor = BKE_ccg_factor(lo_level, hi_level);
	offset = 1 << (hi_level - lo_level - 1);

	/* low-res blocks */
	for (yl = 0; yl < lo_gridsize; yl++) {
		for (xl = 0; xl < lo_gridsize; xl++) {
			int lo_val = BLI_BITMAP_GET(lo_hidden, yl * lo_gridsize + xl);

			/* high-res blocks */
			for (yo = -offset; yo <= offset; yo++) {
				yh = yl * factor + yo;
				if (yh < 0 || yh >= hi_gridsize)
					continue;

				for (xo = -offset; xo <= offset; xo++) {
					xh = xl * factor + xo;
					if (xh < 0 || xh >= hi_gridsize)
						continue;

					hi_ndx = yh * hi_gridsize + xh;

					if (prev_hidden) {
						/* If prev_hidden is available, copy it to
						 * subd, except when the equivalent element in
						 * lo_hidden is different */
						if (lo_val != prev_hidden[hi_ndx])
							BLI_BITMAP_MODIFY(subd, hi_ndx, lo_val);
						else
							BLI_BITMAP_MODIFY(subd, hi_ndx, prev_hidden[hi_ndx]);
					}
					else {
						BLI_BITMAP_MODIFY(subd, hi_ndx, lo_val);
					}
				}
			}
		}
	}

	return subd;
}

static BLI_bitmap *multires_mdisps_downsample_hidden(BLI_bitmap *old_hidden,
                                                     int old_level,
                                                     int new_level)
{
	BLI_bitmap *new_hidden;
	int new_gridsize = BKE_ccg_gridsize(new_level);
	int old_gridsize = BKE_ccg_gridsize(old_level);
	int x, y, factor, old_value;

	BLI_assert(new_level <= old_level);
	factor = BKE_ccg_factor(new_level, old_level);
	new_hidden = BLI_BITMAP_NEW(new_gridsize * new_gridsize,
	                            "downsample hidden");



	for (y = 0; y < new_gridsize; y++) {
		for (x = 0; x < new_gridsize; x++) {
			old_value = BLI_BITMAP_GET(old_hidden,
			                           factor * y * old_gridsize + x * factor);
			
			BLI_BITMAP_MODIFY(new_hidden, y * new_gridsize + x, old_value);
		}
	}

	return new_hidden;
}

static void multires_output_hidden_to_ccgdm(CCGDerivedMesh *ccgdm,
                                            Mesh *me, int level)
{
	const MDisps *mdisps = CustomData_get_layer(&me->ldata, CD_MDISPS);
	BLI_bitmap **grid_hidden = ccgdm->gridHidden;
	int *gridOffset;
	int i, j;
	
	gridOffset = ccgdm->dm.getGridOffset(&ccgdm->dm);

	for (i = 0; i < me->totpoly; i++) {
		for (j = 0; j < me->mpoly[i].totloop; j++) {
			int g = gridOffset[i] + j;
			const MDisps *md = &mdisps[g];
			BLI_bitmap *gh = md->hidden;
			
			if (gh) {
				grid_hidden[g] =
				        multires_mdisps_downsample_hidden(gh, md->level, level);
			}
		}
	}
}

/* subdivide mdisps.hidden if needed (assumes that md.level reflects
 * the current level of md.hidden) */
static void multires_mdisps_subdivide_hidden(MDisps *md, int new_level)
{
	BLI_bitmap *subd;
	
	BLI_assert(md->hidden);

	/* nothing to do if already subdivided enough */
	if (md->level >= new_level)
		return;

	subd = multires_mdisps_upsample_hidden(md->hidden,
	                                       md->level,
	                                       new_level,
	                                       NULL);
	
	/* swap in the subdivided data */
	MEM_freeN(md->hidden);
	md->hidden = subd;
}

static MDisps *multires_mdisps_initialize_hidden(Mesh *me, int level)
{
	MDisps *mdisps = CustomData_add_layer(&me->ldata, CD_MDISPS,
	                                      CD_CALLOC, NULL, me->totloop);
	int gridsize = BKE_ccg_gridsize(level);
	int gridarea = gridsize * gridsize;
	int i, j, k;
	
	for (i = 0; i < me->totpoly; i++) {
		int hide = 0;

		for (j = 0; j < me->mpoly[i].totloop; j++) {
			if (me->mvert[me->mloop[me->mpoly[i].loopstart + j].v].flag & ME_HIDE) {
				hide = 1;
				break;
			}
		}

		if (!hide)
			continue;

		for (j = 0; j < me->mpoly[i].totloop; j++) {
			MDisps *md = &mdisps[me->mpoly[i].loopstart + j];

			BLI_assert(!md->hidden);

			md->hidden = BLI_BITMAP_NEW(gridarea, "MDisps.hidden initialize");

			for (k = 0; k < gridarea; k++)
				BLI_BITMAP_SET(md->hidden, k);
		}
	}

	return mdisps;
}

DerivedMesh *get_multires_dm(Scene *scene, MultiresModifierData *mmd, Object *ob)
{
	ModifierData *md = (ModifierData *)mmd;
	ModifierTypeInfo *mti = modifierType_getInfo(md->type);
	DerivedMesh *tdm = mesh_get_derived_deform(scene, ob, CD_MASK_BAREMESH);
	DerivedMesh *dm;

	dm = mti->applyModifier(md, ob, tdm, MOD_APPLY_USECACHE);
	if (dm == tdm) {
		dm = CDDM_copy(tdm);
	}

	return dm;
}

MultiresModifierData *find_multires_modifier_before(Scene *scene, ModifierData *lastmd)
{
	ModifierData *md;

	for (md = lastmd; md; md = md->prev) {
		if (md->type == eModifierType_Multires) {
			if (modifier_isEnabled(scene, md, eModifierMode_Realtime))
				return (MultiresModifierData *)md;
		}
	}

	return NULL;
}

/* used for applying scale on mdisps layer and syncing subdivide levels when joining objects
 * use_first - return first multires modifier if all multires'es are disabled
 */
MultiresModifierData *get_multires_modifier(Scene *scene, Object *ob, bool use_first)
{
	ModifierData *md;
	MultiresModifierData *mmd = NULL, *firstmmd = NULL;

	/* find first active multires modifier */
	for (md = ob->modifiers.first; md; md = md->next) {
		if (md->type == eModifierType_Multires) {
			if (!firstmmd)
				firstmmd = (MultiresModifierData *)md;

			if (modifier_isEnabled(scene, md, eModifierMode_Realtime)) {
				mmd = (MultiresModifierData *)md;
				break;
			}
		}
	}

	if (!mmd && use_first) {
		/* active multires have not been found
		 * try to use first one */
		return firstmmd;
	}

	return mmd;
}

static int multires_get_level(Object *ob, MultiresModifierData *mmd, int render)
{
	if (render)
		return (mmd->modifier.scene) ? get_render_subsurf_level(&mmd->modifier.scene->r, mmd->renderlvl) : mmd->renderlvl;
	else if (ob->mode == OB_MODE_SCULPT)
		return mmd->sculptlvl;
	else
		return (mmd->modifier.scene) ? get_render_subsurf_level(&mmd->modifier.scene->r, mmd->lvl) : mmd->lvl;
}

void multires_set_tot_level(Object *ob, MultiresModifierData *mmd, int lvl)
{
	mmd->totlvl = lvl;

	if (ob->mode != OB_MODE_SCULPT)
		mmd->lvl = CLAMPIS(MAX2(mmd->lvl, lvl), 0, mmd->totlvl);

	mmd->sculptlvl = CLAMPIS(MAX2(mmd->sculptlvl, lvl), 0, mmd->totlvl);
	mmd->renderlvl = CLAMPIS(MAX2(mmd->renderlvl, lvl), 0, mmd->totlvl);
}

static void multires_dm_mark_as_modified(DerivedMesh *dm, MultiresModifiedFlags flags)
{
	CCGDerivedMesh *ccgdm = (CCGDerivedMesh *)dm;
	ccgdm->multires.modified_flags |= flags;
}

void multires_mark_as_modified(Object *ob, MultiresModifiedFlags flags)
{
	if (ob && ob->derivedFinal)
		multires_dm_mark_as_modified(ob->derivedFinal, flags);
}

void multires_force_update(Object *ob)
{
	if (ob) {
		BKE_object_free_derived_caches(ob);

		if (ob->sculpt && ob->sculpt->pbvh) {
			BKE_pbvh_free(ob->sculpt->pbvh);
			ob->sculpt->pbvh = NULL;
		}
	}
}

void multires_force_external_reload(Object *ob)
{
	Mesh *me = BKE_mesh_from_object(ob);

	CustomData_external_reload(&me->ldata, &me->id, CD_MASK_MDISPS, me->totloop);
	multires_force_update(ob);
}

void multires_force_render_update(Object *ob)
{
	if (ob && (ob->mode & OB_MODE_SCULPT) && modifiers_findByType(ob, eModifierType_Multires))
		multires_force_update(ob);
}

int multiresModifier_reshapeFromDM(Scene *scene, MultiresModifierData *mmd,
                                   Object *ob, DerivedMesh *srcdm)
{
	DerivedMesh *mrdm = get_multires_dm(scene, mmd, ob);

	if (mrdm && srcdm && mrdm->getNumVerts(mrdm) == srcdm->getNumVerts(srcdm)) {
		multires_mvert_to_ss(mrdm, srcdm->getVertArray(srcdm));

		multires_dm_mark_as_modified(mrdm, MULTIRES_COORDS_MODIFIED);
		multires_force_update(ob);

		mrdm->release(mrdm);

		return 1;
	}

	if (mrdm) mrdm->release(mrdm);

	return 0;
}

/* Returns 1 on success, 0 if the src's totvert doesn't match */
int multiresModifier_reshape(Scene *scene, MultiresModifierData *mmd, Object *dst, Object *src)
{
	DerivedMesh *srcdm = mesh_get_derived_final(scene, src, CD_MASK_BAREMESH);
	return multiresModifier_reshapeFromDM(scene, mmd, dst, srcdm);
}

int multiresModifier_reshapeFromDeformMod(Scene *scene, MultiresModifierData *mmd,
                                          Object *ob, ModifierData *md)
{
	ModifierTypeInfo *mti = modifierType_getInfo(md->type);
	DerivedMesh *dm, *ndm;
	int numVerts, result;
	float (*deformedVerts)[3];

	if (multires_get_level(ob, mmd, 0) == 0)
		return 0;

	/* Create DerivedMesh for deformation modifier */
	dm = get_multires_dm(scene, mmd, ob);
	numVerts = dm->getNumVerts(dm);
	deformedVerts = MEM_mallocN(sizeof(float[3]) * numVerts, "multiresReshape_deformVerts");

	dm->getVertCos(dm, deformedVerts);
	mti->deformVerts(md, ob, dm, deformedVerts, numVerts, 0);

	ndm = CDDM_copy(dm);
	CDDM_apply_vert_coords(ndm, deformedVerts);

	MEM_freeN(deformedVerts);
	dm->release(dm);

	/* Reshaping */
	result = multiresModifier_reshapeFromDM(scene, mmd, ob, ndm);

	/* Cleanup */
	ndm->release(ndm);

	return result;
}

/* reset the multires levels to match the number of mdisps */
static int get_levels_from_disps(Object *ob)
{
	Mesh *me = ob->data;
	MDisps *mdisp, *md;
	int i, j, totlvl = 0;

	mdisp = CustomData_get_layer(&me->ldata, CD_MDISPS);

	for (i = 0; i < me->totpoly; ++i) {
		md = mdisp + me->mpoly[i].loopstart;

		for (j = 0; j < me->mpoly[i].totloop; j++, md++) {
			if (md->totdisp == 0) continue;
	
			while (1) {
				int side = (1 << (totlvl - 1)) + 1;
				int lvl_totdisp = side * side;
				if (md->totdisp == lvl_totdisp)
					break;
				else if (md->totdisp < lvl_totdisp)
					totlvl--;
				else
					totlvl++;
	
			}
			
			break;
		}
	}

	return totlvl;
}

/* reset the multires levels to match the number of mdisps */
void multiresModifier_set_levels_from_disps(MultiresModifierData *mmd, Object *ob)
{
	Mesh *me = ob->data;
	MDisps *mdisp;

	if (me->edit_btmesh)
		mdisp = CustomData_get_layer(&me->edit_btmesh->bm->ldata, CD_MDISPS);
	else
		mdisp = CustomData_get_layer(&me->ldata, CD_MDISPS);

	if (mdisp) {
		mmd->totlvl = get_levels_from_disps(ob);
		mmd->lvl = MIN2(mmd->sculptlvl, mmd->totlvl);
		mmd->sculptlvl = MIN2(mmd->sculptlvl, mmd->totlvl);
		mmd->renderlvl = MIN2(mmd->renderlvl, mmd->totlvl);
	}
}

static void multires_set_tot_mdisps(Mesh *me, int lvl)
{
	MDisps *mdisps = CustomData_get_layer(&me->ldata, CD_MDISPS);
	int i;

	if (mdisps) {
		for (i = 0; i < me->totloop; i++, mdisps++) {
			mdisps->totdisp = multires_grid_tot[lvl];
			mdisps->level = lvl;
		}
	}
}

static void multires_reallocate_mdisps(int totloop, MDisps *mdisps, int lvl)
{
	int i;

	/* reallocate displacements to be filled in */
	for (i = 0; i < totloop; ++i) {
		int totdisp = multires_grid_tot[lvl];
		float (*disps)[3] = MEM_callocN(sizeof(float) * 3 * totdisp, "multires disps");

		if (mdisps[i].disps)
			MEM_freeN(mdisps[i].disps);
		
		if (mdisps[i].level && mdisps[i].hidden)
			multires_mdisps_subdivide_hidden(&mdisps[i], lvl);

		mdisps[i].disps = disps;
		mdisps[i].totdisp = totdisp;
		mdisps[i].level = lvl;
	}
}

static void multires_copy_grid(float (*gridA)[3], float (*gridB)[3], int sizeA, int sizeB)
{
	int x, y, j, skip;

	if (sizeA > sizeB) {
		skip = (sizeA - 1) / (sizeB - 1);

		for (j = 0, y = 0; y < sizeB; y++)
			for (x = 0; x < sizeB; x++, j++)
				copy_v3_v3(gridA[y * skip * sizeA + x * skip], gridB[j]);
	}
	else {
		skip = (sizeB - 1) / (sizeA - 1);

		for (j = 0, y = 0; y < sizeA; y++)
			for (x = 0; x < sizeA; x++, j++)
				copy_v3_v3(gridA[j], gridB[y * skip * sizeB + x * skip]);
	}
}

static void multires_copy_dm_grid(CCGElem *gridA, CCGElem *gridB, CCGKey *keyA, CCGKey *keyB)
{
	int x, y, j, skip;

	if (keyA->grid_size > keyB->grid_size) {
		skip = (keyA->grid_size - 1) / (keyB->grid_size - 1);

		for (j = 0, y = 0; y < keyB->grid_size; y++)
			for (x = 0; x < keyB->grid_size; x++, j++)
				memcpy(CCG_elem_offset_co(keyA, gridA, y * skip * keyA->grid_size + x * skip),
				       CCG_elem_offset_co(keyB, gridB, j),
				       sizeof(float) * keyA->num_layers);
	}
	else {
		skip = (keyB->grid_size - 1) / (keyA->grid_size - 1);

		for (j = 0, y = 0; y < keyA->grid_size; y++)
			for (x = 0; x < keyA->grid_size; x++, j++)
				memcpy(CCG_elem_offset_co(keyA, gridA, j),
				       CCG_elem_offset_co(keyB, gridB, y * skip * keyB->grid_size + x * skip),
				       sizeof(float) * keyA->num_layers);
	}
}

/* Reallocate gpm->data at a lower resolution and copy values over
 * from the original high-resolution data */
static void multires_grid_paint_mask_downsample(GridPaintMask *gpm, int level)
{
	if (level < gpm->level) {
		int gridsize = BKE_ccg_gridsize(level);
		float *data = MEM_callocN(sizeof(float) * gridsize * gridsize,
		                          "multires_grid_paint_mask_downsample");
		int x, y;

		for (y = 0; y < gridsize; y++) {
			for (x = 0; x < gridsize; x++) {
				data[y * gridsize + x] =
				    paint_grid_paint_mask(gpm, level, x, y);
			}
		}

		MEM_freeN(gpm->data);
		gpm->data = data;
		gpm->level = level;
	}
}

static void multires_del_higher(MultiresModifierData *mmd, Object *ob, int lvl)
{
	Mesh *me = (Mesh *)ob->data;
	int levels = mmd->totlvl - lvl;
	MDisps *mdisps;
	GridPaintMask *gpm;

	multires_set_tot_mdisps(me, mmd->totlvl);
	CustomData_external_read(&me->ldata, &me->id, CD_MASK_MDISPS, me->totloop);
	mdisps = CustomData_get_layer(&me->ldata, CD_MDISPS);
	gpm = CustomData_get_layer(&me->ldata, CD_GRID_PAINT_MASK);

	multires_force_update(ob);

	if (mdisps && levels > 0) {
		if (lvl > 0) {
			/* MLoop *ml = me->mloop; */ /*UNUSED*/
			int nsize = multires_side_tot[lvl];
			int hsize = multires_side_tot[mmd->totlvl];
			int i, j;

			for (i = 0; i < me->totpoly; ++i) {
				for (j = 0; j < me->mpoly[i].totloop; j++) {
					int g = me->mpoly[i].loopstart + j;
					MDisps *mdisp = &mdisps[g];
					float (*disps)[3], (*ndisps)[3], (*hdisps)[3];
					int totdisp = multires_grid_tot[lvl];

					disps = MEM_callocN(sizeof(float) * 3 * totdisp, "multires disps");

					ndisps = disps;
					hdisps = mdisp->disps;

					multires_copy_grid(ndisps, hdisps, nsize, hsize);
					if (mdisp->hidden) {
						BLI_bitmap *gh =
						    multires_mdisps_downsample_hidden(mdisp->hidden,
						                                      mdisp->level,
						                                      lvl);
						MEM_freeN(mdisp->hidden);
						mdisp->hidden = gh;
					}

					ndisps += nsize * nsize;
					hdisps += hsize * hsize;

					MEM_freeN(mdisp->disps);
					mdisp->disps = disps;
					mdisp->totdisp = totdisp;
					mdisp->level = lvl;

					if (gpm) {
						multires_grid_paint_mask_downsample(&gpm[g], lvl);
					}
				}
			}
		}
		else {
			multires_customdata_delete(me);
		}
	}

	multires_set_tot_level(ob, mmd, lvl);
}

/* (direction = 1) for delete higher, (direction = 0) for lower (not implemented yet) */
void multiresModifier_del_levels(MultiresModifierData *mmd, Object *ob, int direction)
{
	Mesh *me = BKE_mesh_from_object(ob);
	int lvl = multires_get_level(ob, mmd, 0);
	int levels = mmd->totlvl - lvl;
	MDisps *mdisps;

	multires_set_tot_mdisps(me, mmd->totlvl);
	CustomData_external_read(&me->ldata, &me->id, CD_MASK_MDISPS, me->totloop);
	mdisps = CustomData_get_layer(&me->ldata, CD_MDISPS);

	multires_force_update(ob);

	if (mdisps && levels > 0 && direction == 1) {
		multires_del_higher(mmd, ob, lvl);
	}

	multires_set_tot_level(ob, mmd, lvl);
}

static DerivedMesh *multires_dm_create_local(Object *ob, DerivedMesh *dm, int lvl, int totlvl, int simple, bool alloc_paint_mask)
{
	MultiresModifierData mmd = {{NULL}};
	MultiresFlags flags = MULTIRES_USE_LOCAL_MMD;

	mmd.lvl = lvl;
	mmd.sculptlvl = lvl;
	mmd.renderlvl = lvl;
	mmd.totlvl = totlvl;
	mmd.simple = simple;

	if (alloc_paint_mask)
		flags |= MULTIRES_ALLOC_PAINT_MASK;

	return multires_make_derived_from_derived(dm, &mmd, ob, flags);
}

static DerivedMesh *subsurf_dm_create_local(Object *ob, DerivedMesh *dm, int lvl, int simple, int optimal, int plain_uv, int alloc_paint_mask)
{
	SubsurfModifierData smd = {{NULL}};
	SubsurfFlags flags = 0;

	smd.levels = smd.renderLevels = lvl;
	if (!plain_uv)
		smd.flags |= eSubsurfModifierFlag_SubsurfUv;
	if (simple)
		smd.subdivType = ME_SIMPLE_SUBSURF;
	if (optimal)
		smd.flags |= eSubsurfModifierFlag_ControlEdges;

	if (ob->mode & OB_MODE_EDIT)
		flags |= SUBSURF_IN_EDIT_MODE;

	if (alloc_paint_mask)
		flags |= SUBSURF_ALLOC_PAINT_MASK;
	
	return subsurf_make_derived_from_derived(dm, &smd, NULL, flags);
}



/* assumes no is normalized; return value's sign is negative if v is on
 * the other side of the plane */
static float v3_dist_from_plane(float v[3], float center[3], float no[3])
{
	float s[3];
	sub_v3_v3v3(s, v, center);
	return dot_v3v3(s, no);
}

void multiresModifier_base_apply(MultiresModifierData *mmd, Object *ob)
{
	DerivedMesh *cddm, *dispdm, *origdm;
	Mesh *me;
	const MeshElemMap *pmap;
	float (*origco)[3];
	int i, j, k, offset, totlvl;

	multires_force_update(ob);

	me = BKE_mesh_from_object(ob);
	totlvl = mmd->totlvl;

	/* nothing to do */
	if (!totlvl)
		return;

	/* XXX - probably not necessary to regenerate the cddm so much? */

	/* generate highest level with displacements */
	cddm = CDDM_from_mesh(me);
	DM_set_only_copy(cddm, CD_MASK_BAREMESH);
	dispdm = multires_dm_create_local(ob, cddm, totlvl, totlvl, 0, 0);
	cddm->release(cddm);

	/* copy the new locations of the base verts into the mesh */
	offset = dispdm->getNumVerts(dispdm) - me->totvert;
	for (i = 0; i < me->totvert; ++i) {
		dispdm->getVertCo(dispdm, offset + i, me->mvert[i].co);
	}

	/* heuristic to produce a better-fitting base mesh */

	cddm = CDDM_from_mesh(me);
	pmap = cddm->getPolyMap(ob, cddm);
	origco = MEM_callocN(sizeof(float) * 3 * me->totvert, "multires apply base origco");
	for (i = 0; i < me->totvert; ++i)
		copy_v3_v3(origco[i], me->mvert[i].co);

	for (i = 0; i < me->totvert; ++i) {
		float avg_no[3] = {0, 0, 0}, center[3] = {0, 0, 0}, push[3];
		float dist;
		int tot = 0;

		/* don't adjust verts not used by at least one poly */
		if (!pmap[i].count)
			continue;

		/* find center */
		for (j = 0; j < pmap[i].count; j++) {
			const MPoly *p = &me->mpoly[pmap[i].indices[j]];
			
			/* this double counts, not sure if that's bad or good */
			for (k = 0; k < p->totloop; ++k) {
				int vndx = me->mloop[p->loopstart + k].v;
				if (vndx != i) {
					add_v3_v3(center, origco[vndx]);
					tot++;
				}
			}
		}
		mul_v3_fl(center, 1.0f / tot);

		/* find normal */
		for (j = 0; j < pmap[i].count; j++) {
			const MPoly *p = &me->mpoly[pmap[i].indices[j]];
			MPoly fake_poly;
			MLoop *fake_loops;
			float (*fake_co)[3];
			float no[3];

			/* set up poly, loops, and coords in order to call
			 * BKE_mesh_calc_poly_normal_coords() */
			fake_poly.totloop = p->totloop;
			fake_poly.loopstart = 0;
			fake_loops = MEM_mallocN(sizeof(MLoop) * p->totloop, "fake_loops");
			fake_co = MEM_mallocN(sizeof(float) * 3 * p->totloop, "fake_co");
			
			for (k = 0; k < p->totloop; ++k) {
				int vndx = me->mloop[p->loopstart + k].v;
				
				fake_loops[k].v = k;
				
				if (vndx == i)
					copy_v3_v3(fake_co[k], center);
				else
					copy_v3_v3(fake_co[k], origco[vndx]);
			}
			
			BKE_mesh_calc_poly_normal_coords(&fake_poly, fake_loops,
			                                 (const float(*)[3])fake_co, no);
			MEM_freeN(fake_loops);
			MEM_freeN(fake_co);

			add_v3_v3(avg_no, no);
		}
		normalize_v3(avg_no);

		/* push vertex away from the plane */
		dist = v3_dist_from_plane(me->mvert[i].co, center, avg_no);
		copy_v3_v3(push, avg_no);
		mul_v3_fl(push, dist);
		add_v3_v3(me->mvert[i].co, push);
		
	}

	MEM_freeN(origco);
	cddm->release(cddm);

	/* Vertices were moved around, need to update normals after all the vertices are updated
	 * Probably this is possible to do in the loop above, but this is rather tricky because
	 * we don't know all needed vertices' coordinates there yet.
	 */
	BKE_mesh_calc_normals(me);

	/* subdivide the mesh to highest level without displacements */
	cddm = CDDM_from_mesh(me);
	DM_set_only_copy(cddm, CD_MASK_BAREMESH);
	origdm = subsurf_dm_create_local(ob, cddm, totlvl, 0, 0, mmd->flags & eMultiresModifierFlag_PlainUv, 0);
	cddm->release(cddm);

	/* calc disps */
	multiresModifier_disp_run(dispdm, me, NULL, CALC_DISPLACEMENTS, origdm->getGridData(origdm), totlvl);

	origdm->release(origdm);
	dispdm->release(dispdm);
}

static void multires_subdivide(MultiresModifierData *mmd, Object *ob, int totlvl, int updateblock, int simple)
{
	Mesh *me = ob->data;
	MDisps *mdisps;
	int lvl = mmd->totlvl;

	if ((totlvl > multires_max_levels) || (me->totpoly == 0))
		return;

	multires_force_update(ob);

	mdisps = CustomData_get_layer(&me->ldata, CD_MDISPS);
	if (!mdisps)
		mdisps = multires_mdisps_initialize_hidden(me, totlvl);

	if (mdisps->disps && !updateblock && totlvl > 1) {
		/* upsample */
		DerivedMesh *lowdm, *cddm, *highdm;
		CCGElem **highGridData, **lowGridData, **subGridData;
		CCGKey highGridKey, lowGridKey;
		CCGSubSurf *ss;
		int i, numGrids, highGridSize;
		const bool has_mask = CustomData_has_layer(&me->ldata, CD_GRID_PAINT_MASK);

		/* create subsurf DM from original mesh at high level */
		cddm = CDDM_from_mesh(me);
		DM_set_only_copy(cddm, CD_MASK_BAREMESH);
		highdm = subsurf_dm_create_local(ob, cddm, totlvl, simple, 0, mmd->flags & eMultiresModifierFlag_PlainUv, has_mask);
		ss = ((CCGDerivedMesh *)highdm)->ss;

		/* create multires DM from original mesh at low level */
		lowdm = multires_dm_create_local(ob, cddm, lvl, lvl, simple, has_mask);
		cddm->release(cddm);

		/* copy subsurf grids and replace them with low displaced grids */
		numGrids = highdm->getNumGrids(highdm);
		highGridSize = highdm->getGridSize(highdm);
		highGridData = highdm->getGridData(highdm);
		highdm->getGridKey(highdm, &highGridKey);
		lowGridData = lowdm->getGridData(lowdm);
		lowdm->getGridKey(lowdm, &lowGridKey);

		subGridData = MEM_callocN(sizeof(float *) * numGrids, "subGridData*");

		for (i = 0; i < numGrids; ++i) {
			/* backup subsurf grids */
			subGridData[i] = MEM_callocN(highGridKey.elem_size * highGridSize * highGridSize, "subGridData");
			memcpy(subGridData[i], highGridData[i], highGridKey.elem_size * highGridSize * highGridSize);

			/* overwrite with current displaced grids */
			multires_copy_dm_grid(highGridData[i], lowGridData[i], &highGridKey, &lowGridKey);
		}

		/* low lower level dm no longer needed at this point */
		lowdm->release(lowdm);

		/* subsurf higher levels again with displaced data */
		ccgSubSurf_updateFromFaces(ss, lvl, NULL, 0);
		ccgSubSurf_updateLevels(ss, lvl, NULL, 0);

		/* reallocate displacements */
		multires_reallocate_mdisps(me->totloop, mdisps, totlvl); 

		/* compute displacements */
		multiresModifier_disp_run(highdm, me, NULL, CALC_DISPLACEMENTS, subGridData, totlvl);

		/* free */
		highdm->release(highdm);
		for (i = 0; i < numGrids; ++i)
			MEM_freeN(subGridData[i]);
		MEM_freeN(subGridData);
	}
	else {
		/* only reallocate, nothing to upsample */
		multires_reallocate_mdisps(me->totloop, mdisps, totlvl); 
	}

	multires_set_tot_level(ob, mmd, totlvl);
}

void multiresModifier_subdivide(MultiresModifierData *mmd, Object *ob, int updateblock, int simple)
{
	multires_subdivide(mmd, ob, mmd->totlvl + 1, updateblock, simple);
}

static void grid_tangent(const CCGKey *key, int x, int y, int axis, CCGElem *grid, float t[3])
{
	if (axis == 0) {
		if (x == key->grid_size - 1) {
			if (y == key->grid_size - 1)
				sub_v3_v3v3(t, CCG_grid_elem_co(key, grid, x, y - 1), CCG_grid_elem_co(key, grid, x - 1, y - 1));
			else
				sub_v3_v3v3(t, CCG_grid_elem_co(key, grid, x, y), CCG_grid_elem_co(key, grid, x - 1, y));
		}
		else
			sub_v3_v3v3(t, CCG_grid_elem_co(key, grid, x + 1, y), CCG_grid_elem_co(key, grid, x, y));
	}
	else if (axis == 1) {
		if (y == key->grid_size - 1) {
			if (x == key->grid_size - 1)
				sub_v3_v3v3(t, CCG_grid_elem_co(key, grid, x - 1, y), CCG_grid_elem_co(key, grid, x - 1, (y - 1)));
			else
				sub_v3_v3v3(t, CCG_grid_elem_co(key, grid, x, y), CCG_grid_elem_co(key, grid, x, (y - 1)));
		}
		else
			sub_v3_v3v3(t, CCG_grid_elem_co(key, grid, x, (y + 1)), CCG_grid_elem_co(key, grid, x, y));
	}
}

/* Construct 3x3 tangent-space matrix in 'mat' */
static void grid_tangent_matrix(float mat[3][3], const CCGKey *key,
                                int x, int y, CCGElem *grid)
{
	grid_tangent(key, x, y, 0, grid, mat[0]);
	normalize_v3(mat[0]);

	grid_tangent(key, x, y, 1, grid, mat[1]);
	normalize_v3(mat[1]);

	copy_v3_v3(mat[2], CCG_grid_elem_no(key, grid, x, y));
}

/* XXX WARNING: subsurf elements from dm and oldGridData *must* be of the same format (size),
 *              because this code uses CCGKey's info from dm to access oldGridData's normals
 *              (through the call to grid_tangent_matrix())! */
static void multiresModifier_disp_run(DerivedMesh *dm, Mesh *me, DerivedMesh *dm2, DispOp op, CCGElem **oldGridData, int totlvl)
{
	CCGDerivedMesh *ccgdm = (CCGDerivedMesh *)dm;
	CCGElem **gridData, **subGridData;
	CCGKey key;
	MPoly *mpoly = me->mpoly;
	MDisps *mdisps = CustomData_get_layer(&me->ldata, CD_MDISPS);
	GridPaintMask *grid_paint_mask = NULL;
	int *gridOffset;
	int i, k, /*numGrids, */ gridSize, dGridSize, dSkip;
	int totloop, totpoly;
	
	/* this happens in the dm made by bmesh_mdisps_space_set */
	if (dm2 && CustomData_has_layer(&dm2->loopData, CD_MDISPS)) {
		mpoly = CustomData_get_layer(&dm2->polyData, CD_MPOLY);
		mdisps = CustomData_get_layer(&dm2->loopData, CD_MDISPS);
		totloop = dm2->numLoopData;
		totpoly = dm2->numPolyData;
	}
	else {
		totloop = me->totloop;
		totpoly = me->totpoly;
	}
	
	if (!mdisps) {
		if (op == CALC_DISPLACEMENTS)
			mdisps = CustomData_add_layer(&me->ldata, CD_MDISPS, CD_DEFAULT, NULL, me->totloop);
		else
			return;
	}

	/*numGrids = dm->getNumGrids(dm);*/ /*UNUSED*/
	gridSize = dm->getGridSize(dm);
	gridData = dm->getGridData(dm);
	gridOffset = dm->getGridOffset(dm);
	dm->getGridKey(dm, &key);
	subGridData = (oldGridData) ? oldGridData : gridData;

	dGridSize = multires_side_tot[totlvl];
	dSkip = (dGridSize - 1) / (gridSize - 1);

	/* multires paint masks */
	if (key.has_mask)
		grid_paint_mask = CustomData_get_layer(&me->ldata, CD_GRID_PAINT_MASK);

	k = 0; /*current loop/mdisp index within the mloop array*/

#pragma omp parallel for private(i) if (totloop * gridSize * gridSize >= CCG_OMP_LIMIT)

	for (i = 0; i < totpoly; ++i) {
		const int numVerts = mpoly[i].totloop;
		int S, x, y, gIndex = gridOffset[i];

		for (S = 0; S < numVerts; ++S, ++gIndex, ++k) {
			GridPaintMask *gpm = grid_paint_mask ? &grid_paint_mask[gIndex] : NULL;
			MDisps *mdisp = &mdisps[mpoly[i].loopstart + S];
			CCGElem *grid = gridData[gIndex];
			CCGElem *subgrid = subGridData[gIndex];
			float (*dispgrid)[3] = NULL;

			/* when adding new faces in edit mode, need to allocate disps */
			if (!mdisp->disps)
#pragma omp critical
			{
				multires_reallocate_mdisps(totloop, mdisps, totlvl);
			}

			dispgrid = mdisp->disps;

			/* if needed, reallocate multires paint mask */
			if (gpm && gpm->level < key.level) {
				gpm->level = key.level;
#pragma omp critical
				{
					if (gpm->data)
						MEM_freeN(gpm->data);
					gpm->data = MEM_callocN(sizeof(float) * key.grid_area, "gpm.data");
				}
			}

			for (y = 0; y < gridSize; y++) {
				for (x = 0; x < gridSize; x++) {
					float *co = CCG_grid_elem_co(&key, grid, x, y);
					float *sco = CCG_grid_elem_co(&key, subgrid, x, y);
					float *data = dispgrid[dGridSize * y * dSkip + x * dSkip];
					float mat[3][3], disp[3], d[3], mask;

					/* construct tangent space matrix */
					grid_tangent_matrix(mat, &key, x, y, subgrid);

					switch (op) {
						case APPLY_DISPLACEMENTS:
							/* Convert displacement to object space
							 * and add to grid points */
							mul_v3_m3v3(disp, mat, data);
							add_v3_v3v3(co, sco, disp);
							break;
						case CALC_DISPLACEMENTS:
							/* Calculate displacement between new and old
							 * grid points and convert to tangent space */
							sub_v3_v3v3(disp, co, sco);
							invert_m3(mat);
							mul_v3_m3v3(data, mat, disp);
							break;
						case ADD_DISPLACEMENTS:
							/* Convert subdivided displacements to tangent
							 * space and add to the original displacements */
							invert_m3(mat);
							mul_v3_m3v3(d, mat, co);
							add_v3_v3(data, d);
							break;
					}

					if (gpm) {
						switch (op) {
							case APPLY_DISPLACEMENTS:
								/* Copy mask from gpm to DM */
								*CCG_grid_elem_mask(&key, grid, x, y) =
								    paint_grid_paint_mask(gpm, key.level, x, y);
								break;
							case CALC_DISPLACEMENTS:
								/* Copy mask from DM to gpm */
								mask = *CCG_grid_elem_mask(&key, grid, x, y);
								gpm->data[y * gridSize + x] = CLAMPIS(mask, 0, 1);
								break;
							case ADD_DISPLACEMENTS:
								/* Add mask displacement to gpm */
								gpm->data[y * gridSize + x] +=
								    *CCG_grid_elem_mask(&key, grid, x, y);
								break;
						}
					}
				}
			}
		}
	}
	
	if (op == APPLY_DISPLACEMENTS) {
		ccgSubSurf_stitchFaces(ccgdm->ss, 0, NULL, 0);
		ccgSubSurf_updateNormals(ccgdm->ss, NULL, 0);
	}
}

void multires_modifier_update_mdisps(struct DerivedMesh *dm)
{
	CCGDerivedMesh *ccgdm = (CCGDerivedMesh *)dm;
	Object *ob;
	Mesh *me;
	MDisps *mdisps;
	MultiresModifierData *mmd;

	ob = ccgdm->multires.ob;
	me = ccgdm->multires.ob->data;
	mmd = ccgdm->multires.mmd;
	multires_set_tot_mdisps(me, mmd->totlvl);
	CustomData_external_read(&me->ldata, &me->id, CD_MASK_MDISPS, me->totloop);
	mdisps = CustomData_get_layer(&me->ldata, CD_MDISPS);

	if (mdisps) {
		int lvl = ccgdm->multires.lvl;
		int totlvl = ccgdm->multires.totlvl;
		
		if (lvl < totlvl) {
			DerivedMesh *lowdm, *cddm, *highdm;
			CCGElem **highGridData, **lowGridData, **subGridData, **gridData, *diffGrid;
			CCGKey highGridKey, lowGridKey;
			CCGSubSurf *ss;
			int i, j, numGrids, highGridSize, lowGridSize;
			const bool has_mask = CustomData_has_layer(&me->ldata, CD_GRID_PAINT_MASK);

			/* create subsurf DM from original mesh at high level */
			if (ob->derivedDeform) cddm = CDDM_copy(ob->derivedDeform);
			else cddm = CDDM_from_mesh(me);
			DM_set_only_copy(cddm, CD_MASK_BAREMESH);

			highdm = subsurf_dm_create_local(ob, cddm, totlvl, mmd->simple, 0, mmd->flags & eMultiresModifierFlag_PlainUv, has_mask);
			ss = ((CCGDerivedMesh *)highdm)->ss;

			/* create multires DM from original mesh and displacements */
			lowdm = multires_dm_create_local(ob, cddm, lvl, totlvl, mmd->simple, has_mask);
			cddm->release(cddm);

			/* gather grid data */
			numGrids = highdm->getNumGrids(highdm);
			highGridSize = highdm->getGridSize(highdm);
			highGridData = highdm->getGridData(highdm);
			highdm->getGridKey(highdm, &highGridKey);
			lowGridSize = lowdm->getGridSize(lowdm);
			lowGridData = lowdm->getGridData(lowdm);
			lowdm->getGridKey(lowdm, &lowGridKey);
			gridData = dm->getGridData(dm);

			BLI_assert(highGridKey.elem_size == lowGridKey.elem_size);

			subGridData = MEM_callocN(sizeof(CCGElem *) * numGrids, "subGridData*");
			diffGrid = MEM_callocN(lowGridKey.elem_size * lowGridSize * lowGridSize, "diff");

			for (i = 0; i < numGrids; ++i) {
				/* backup subsurf grids */
				subGridData[i] = MEM_callocN(highGridKey.elem_size * highGridSize * highGridSize, "subGridData");
				memcpy(subGridData[i], highGridData[i], highGridKey.elem_size * highGridSize * highGridSize);

				/* write difference of subsurf and displaced low level into high subsurf */
				for (j = 0; j < lowGridSize * lowGridSize; ++j) {
					sub_v4_v4v4(CCG_elem_offset_co(&lowGridKey, diffGrid, j),
					            CCG_elem_offset_co(&lowGridKey, gridData[i], j),
					            CCG_elem_offset_co(&lowGridKey, lowGridData[i], j));
				}

				multires_copy_dm_grid(highGridData[i], diffGrid, &highGridKey, &lowGridKey);
			}

			/* lower level dm no longer needed at this point */
			MEM_freeN(diffGrid);
			lowdm->release(lowdm);

			/* subsurf higher levels again with difference of coordinates */
			ccgSubSurf_updateFromFaces(ss, lvl, NULL, 0);
			ccgSubSurf_updateLevels(ss, lvl, NULL, 0);

			/* add to displacements */
			multiresModifier_disp_run(highdm, me, NULL, ADD_DISPLACEMENTS, subGridData, mmd->totlvl);

			/* free */
			highdm->release(highdm);
			for (i = 0; i < numGrids; ++i)
				MEM_freeN(subGridData[i]);
			MEM_freeN(subGridData);
		}
		else {
			DerivedMesh *cddm, *subdm;
			const bool has_mask = CustomData_has_layer(&me->ldata, CD_GRID_PAINT_MASK);

			if (ob->derivedDeform) cddm = CDDM_copy(ob->derivedDeform);
			else cddm = CDDM_from_mesh(me);
			DM_set_only_copy(cddm, CD_MASK_BAREMESH);

			subdm = subsurf_dm_create_local(ob, cddm, mmd->totlvl, mmd->simple, 0, mmd->flags & eMultiresModifierFlag_PlainUv, has_mask);
			cddm->release(cddm);

			multiresModifier_disp_run(dm, me, NULL, CALC_DISPLACEMENTS, subdm->getGridData(subdm), mmd->totlvl);

			subdm->release(subdm);
		}
	}
}

void multires_modifier_update_hidden(DerivedMesh *dm)
{
	CCGDerivedMesh *ccgdm = (CCGDerivedMesh *)dm;
	BLI_bitmap **grid_hidden = ccgdm->gridHidden;
	Mesh *me = ccgdm->multires.ob->data;
	MDisps *mdisps = CustomData_get_layer(&me->ldata, CD_MDISPS);
	int totlvl = ccgdm->multires.totlvl;
	int lvl = ccgdm->multires.lvl;

	if (mdisps) {
		int i;
		
		for (i = 0; i < me->totloop; i++) {
			MDisps *md = &mdisps[i];
			BLI_bitmap *gh = grid_hidden[i];

			if (!gh && md->hidden) {
				MEM_freeN(md->hidden);
				md->hidden = NULL;
			}
			else if (gh) {
				gh = multires_mdisps_upsample_hidden(gh, lvl, totlvl,
				                                     md->hidden);
				if (md->hidden)
					MEM_freeN(md->hidden);
				
				md->hidden = gh;
			}
		}
	}
}

void multires_set_space(DerivedMesh *dm, Object *ob, int from, int to)
{
	DerivedMesh *ccgdm = NULL, *subsurf = NULL;
	CCGElem **gridData, **subGridData = NULL;
	CCGKey key;
	MPoly *mpoly = CustomData_get_layer(&dm->polyData, CD_MPOLY);
	MDisps *mdisps;
	MultiresModifierData *mmd = get_multires_modifier(NULL, ob, 1);
	int *gridOffset, totlvl;
	int i, k, numGrids, gridSize, dGridSize, dSkip;
	
	if (!mmd)
		return;
	
	mdisps = CustomData_get_layer(&dm->loopData, CD_MDISPS);

	if (!mdisps) {
		goto cleanup;
	}

	totlvl = mmd->totlvl;
	ccgdm = multires_dm_create_local(ob, dm, totlvl, totlvl, mmd->simple, false);
	
	subsurf = subsurf_dm_create_local(ob, dm, totlvl,
	                                  mmd->simple, mmd->flags & eMultiresModifierFlag_ControlEdges, mmd->flags & eMultiresModifierFlag_PlainUv, 0);

	numGrids = subsurf->getNumGrids(subsurf);
	gridSize = subsurf->getGridSize(subsurf);
	gridData = subsurf->getGridData(subsurf);
	subsurf->getGridKey(subsurf, &key);

	subGridData = MEM_callocN(sizeof(CCGElem *) * numGrids, "subGridData*");

	for (i = 0; i < numGrids; i++) {
		subGridData[i] = MEM_callocN(key.elem_size * gridSize * gridSize, "subGridData");
		memcpy(subGridData[i], gridData[i], key.elem_size * gridSize * gridSize);
	}
	
	/* numGrids = ccgdm->dm->getNumGrids((DerivedMesh *)ccgdm); */ /*UNUSED*/
	gridSize = ccgdm->getGridSize((DerivedMesh *)ccgdm);
	gridData = ccgdm->getGridData((DerivedMesh *)ccgdm);
	gridOffset = ccgdm->getGridOffset((DerivedMesh *)ccgdm);

	dGridSize = multires_side_tot[totlvl];
	dSkip = (dGridSize - 1) / (gridSize - 1);

	k = 0; /*current loop/mdisp index within the mloop array*/

	//#pragma omp parallel for private(i) if (dm->numLoopData * gridSize * gridSize >= CCG_OMP_LIMIT)

	for (i = 0; i < dm->numPolyData; ++i) {
		const int numVerts = mpoly[i].totloop;
		int S, x, y, gIndex = gridOffset[i];
						
		for (S = 0; S < numVerts; ++S, ++gIndex, ++k) {
			MDisps *mdisp = &mdisps[mpoly[i].loopstart + S];
			/* CCGElem *grid = gridData[gIndex]; */ /* UNUSED */
			CCGElem *subgrid = subGridData[gIndex];
			float (*dispgrid)[3] = NULL;

			/* when adding new faces in edit mode, need to allocate disps */
			if (!mdisp->disps) {
				mdisp->totdisp = gridSize * gridSize;
				mdisp->level = totlvl;
				mdisp->disps = MEM_callocN(sizeof(float) * 3 * mdisp->totdisp, "disp in multires_set_space");
			}

			dispgrid = mdisp->disps;

			for (y = 0; y < gridSize; y++) {
				for (x = 0; x < gridSize; x++) {
					float *data = dispgrid[dGridSize * y * dSkip + x * dSkip];
					float *co = CCG_grid_elem_co(&key, subgrid, x, y);
					float mat[3][3], dco[3];
					
					/* construct tangent space matrix */
					grid_tangent_matrix(mat, &key, x, y, subgrid);

					/* convert to absolute coordinates in space */
					if (from == MULTIRES_SPACE_TANGENT) {
						mul_v3_m3v3(dco, mat, data);
						add_v3_v3(dco, co);
					}
					else if (from == MULTIRES_SPACE_OBJECT) {
						add_v3_v3v3(dco, co, data);
					}
					else if (from == MULTIRES_SPACE_ABSOLUTE) {
						copy_v3_v3(dco, data);
					}
					
					/*now, convert to desired displacement type*/
					if (to == MULTIRES_SPACE_TANGENT) {
						invert_m3(mat);

						sub_v3_v3(dco, co);
						mul_v3_m3v3(data, mat, dco);
					}
					else if (to == MULTIRES_SPACE_OBJECT) {
						sub_v3_v3(dco, co);
						mul_v3_m3v3(data, mat, dco);
					}
					else if (to == MULTIRES_SPACE_ABSOLUTE) {
						copy_v3_v3(data, dco);
					}
				}
			}
		}
	}

cleanup:
	if (subsurf) {
		subsurf->needsFree = 1;
		subsurf->release(subsurf);
	}

	if (ccgdm) {
		ccgdm->needsFree = 1;
		ccgdm->release(ccgdm);
	}
}

void multires_stitch_grids(Object *ob)
{
	/* utility for smooth brush */
	if (ob && ob->derivedFinal) {
		CCGDerivedMesh *ccgdm = (CCGDerivedMesh *)ob->derivedFinal;
		CCGFace **faces;
		int totface;

		if (ccgdm->pbvh) {
			BKE_pbvh_get_grid_updates(ccgdm->pbvh, 0, (void ***)&faces, &totface);

			if (totface) {
				ccgSubSurf_stitchFaces(ccgdm->ss, 0, faces, totface);
				MEM_freeN(faces);
			}
		}
	}
}

DerivedMesh *multires_make_derived_from_derived(DerivedMesh *dm,
                                                MultiresModifierData *mmd,
                                                Object *ob,
                                                MultiresFlags flags)
{
	Mesh *me = ob->data;
	DerivedMesh *result;
	CCGDerivedMesh *ccgdm = NULL;
	CCGElem **gridData, **subGridData;
	CCGKey key;
	int lvl = multires_get_level(ob, mmd, (flags & MULTIRES_USE_RENDER_PARAMS));
	int i, gridSize, numGrids;

	if (lvl == 0)
		return dm;

	result = subsurf_dm_create_local(ob, dm, lvl,
	                                 mmd->simple, mmd->flags & eMultiresModifierFlag_ControlEdges,
	                                 mmd->flags & eMultiresModifierFlag_PlainUv,
	                                 flags & MULTIRES_ALLOC_PAINT_MASK);

	if (!(flags & MULTIRES_USE_LOCAL_MMD)) {
		ccgdm = (CCGDerivedMesh *)result;

		ccgdm->multires.ob = ob;
		ccgdm->multires.mmd = mmd;
		ccgdm->multires.local_mmd = 0;
		ccgdm->multires.lvl = lvl;
		ccgdm->multires.totlvl = mmd->totlvl;
		ccgdm->multires.modified_flags = 0;
	}

	numGrids = result->getNumGrids(result);
	gridSize = result->getGridSize(result);
	gridData = result->getGridData(result);
	result->getGridKey(result, &key);

	subGridData = MEM_callocN(sizeof(CCGElem *) * numGrids, "subGridData*");

	for (i = 0; i < numGrids; i++) {
		subGridData[i] = MEM_callocN(key.elem_size * gridSize * gridSize, "subGridData");
		memcpy(subGridData[i], gridData[i], key.elem_size * gridSize * gridSize);
	}

	multires_set_tot_mdisps(me, mmd->totlvl);
	CustomData_external_read(&me->ldata, &me->id, CD_MASK_MDISPS, me->totloop);

	/*run displacement*/
	multiresModifier_disp_run(result, ob->data, dm, APPLY_DISPLACEMENTS, subGridData, mmd->totlvl);

	/* copy hidden elements for this level */
	if (ccgdm)
		multires_output_hidden_to_ccgdm(ccgdm, me, lvl);

	for (i = 0; i < numGrids; i++)
		MEM_freeN(subGridData[i]);
	MEM_freeN(subGridData);

	return result;
}

/**** Old Multires code ****
 ***************************/

/* Adapted from sculptmode.c */
void old_mdisps_bilinear(float out[3], float (*disps)[3], const int st, float u, float v)
{
	int x, y, x2, y2;
	const int st_max = st - 1;
	float urat, vrat, uopp;
	float d[4][3], d2[2][3];
	
	if (!disps || isnan(u) || isnan(v))
		return;
			
	if (u < 0)
		u = 0;
	else if (u >= st)
		u = st_max;
	if (v < 0)
		v = 0;
	else if (v >= st)
		v = st_max;

	x = floor(u);
	y = floor(v);
	x2 = x + 1;
	y2 = y + 1;

	if (x2 >= st) x2 = st_max;
	if (y2 >= st) y2 = st_max;
	
	urat = u - x;
	vrat = v - y;
	uopp = 1 - urat;

	mul_v3_v3fl(d[0], disps[y * st + x], uopp);
	mul_v3_v3fl(d[1], disps[y * st + x2], urat);
	mul_v3_v3fl(d[2], disps[y2 * st + x], uopp);
	mul_v3_v3fl(d[3], disps[y2 * st + x2], urat);

	add_v3_v3v3(d2[0], d[0], d[1]);
	add_v3_v3v3(d2[1], d[2], d[3]);
	mul_v3_fl(d2[0], 1 - vrat);
	mul_v3_fl(d2[1], vrat);

	add_v3_v3v3(out, d2[0], d2[1]);
}

static void old_mdisps_rotate(int S, int UNUSED(newside), int oldside, int x, int y, float *u, float *v)
{
	float offset = oldside * 0.5f - 0.5f;

	if (S == 1) { *u = offset + x; *v = offset - y; }
	if (S == 2) { *u = offset + y; *v = offset + x; }
	if (S == 3) { *u = offset - x; *v = offset + y; }
	if (S == 0) { *u = offset - y; *v = offset - x; }
}

static void old_mdisps_convert(MFace *mface, MDisps *mdisp)
{
	int newlvl = log(sqrt(mdisp->totdisp) - 1) / M_LN2;
	int oldlvl = newlvl + 1;
	int oldside = multires_side_tot[oldlvl];
	int newside = multires_side_tot[newlvl];
	int nvert = (mface->v4) ? 4 : 3;
	int newtotdisp = multires_grid_tot[newlvl] * nvert;
	int x, y, S;
	float (*disps)[3], (*out)[3], u = 0.0f, v = 0.0f; /* Quite gcc barking. */

	disps = MEM_callocN(sizeof(float) * 3 * newtotdisp, "multires disps");

	out = disps;
	for (S = 0; S < nvert; S++) {
		for (y = 0; y < newside; ++y) {
			for (x = 0; x < newside; ++x, ++out) {
				old_mdisps_rotate(S, newside, oldside, x, y, &u, &v);
				old_mdisps_bilinear(*out, mdisp->disps, oldside, u, v);

				if (S == 1) { (*out)[1] = -(*out)[1]; }
				else if (S == 2) { SWAP(float, (*out)[0], (*out)[1]); }
				else if (S == 3) { (*out)[0] = -(*out)[0]; }
				else if (S == 0) { SWAP(float, (*out)[0], (*out)[1]); (*out)[0] = -(*out)[0]; (*out)[1] = -(*out)[1]; }
			}
		}
	}

	MEM_freeN(mdisp->disps);

	mdisp->totdisp = newtotdisp;
	mdisp->level = newlvl;
	mdisp->disps = disps;
}

void multires_load_old_250(Mesh *me)
{
	MDisps *mdisps, *mdisps2;
	MFace *mf;
	int i, j, k;

	mdisps = CustomData_get_layer(&me->fdata, CD_MDISPS);

	if (mdisps) {
		for (i = 0; i < me->totface; i++)
			if (mdisps[i].totdisp)
				old_mdisps_convert(&me->mface[i], &mdisps[i]);
		
		CustomData_add_layer(&me->ldata, CD_MDISPS, CD_CALLOC, NULL, me->totloop);
		mdisps2 = CustomData_get_layer(&me->ldata, CD_MDISPS);

		k = 0;
		mf = me->mface;
		for (i = 0; i < me->totface; i++, mf++) {
			int nvert = mf->v4 ? 4 : 3;
			int totdisp = mdisps[i].totdisp / nvert;
			
			for (j = 0; j < mf->v4 ? 4 : 3; j++, k++) {
				mdisps2[k].disps = MEM_callocN(sizeof(float) * 3 * totdisp, "multires disp in conversion");
				mdisps2[k].totdisp = totdisp;
				mdisps2[k].level = mdisps[i].level;
				memcpy(mdisps2[k].disps, mdisps[i].disps + totdisp * j, totdisp);
			}

		}
	}
}

/* Does not actually free lvl itself */
static void multires_free_level(MultiresLevel *lvl)
{
	if (lvl) {
		if (lvl->faces) MEM_freeN(lvl->faces);
		if (lvl->edges) MEM_freeN(lvl->edges);
		if (lvl->colfaces) MEM_freeN(lvl->colfaces);
	}
}

void multires_free(Multires *mr)
{
	if (mr) {
		MultiresLevel *lvl = mr->levels.first;

		/* Free the first-level data */
		if (lvl) {
			CustomData_free(&mr->vdata, lvl->totvert);
			CustomData_free(&mr->fdata, lvl->totface);
			if (mr->edge_flags)
				MEM_freeN(mr->edge_flags);
			if (mr->edge_creases)
				MEM_freeN(mr->edge_creases);
		}

		while (lvl) {
			multires_free_level(lvl);
			lvl = lvl->next;
		}

		MEM_freeN(mr->verts);

		BLI_freelistN(&mr->levels);

		MEM_freeN(mr);
	}
}

typedef struct IndexNode {
	struct IndexNode *next, *prev;
	int index;
} IndexNode;

static void create_old_vert_face_map(ListBase **map, IndexNode **mem, const MultiresFace *mface,
                                     const int totvert, const int totface)
{
	int i, j;
	IndexNode *node = NULL;
	
	(*map) = MEM_callocN(sizeof(ListBase) * totvert, "vert face map");
	(*mem) = MEM_callocN(sizeof(IndexNode) * totface * 4, "vert face map mem");
	node = *mem;
	
	/* Find the users */
	for (i = 0; i < totface; ++i) {
		for (j = 0; j < (mface[i].v[3] ? 4 : 3); ++j, ++node) {
			node->index = i;
			BLI_addtail(&(*map)[mface[i].v[j]], node);
		}
	}
}

static void create_old_vert_edge_map(ListBase **map, IndexNode **mem, const MultiresEdge *medge,
                                     const int totvert, const int totedge)
{
	int i, j;
	IndexNode *node = NULL;
	
	(*map) = MEM_callocN(sizeof(ListBase) * totvert, "vert edge map");
	(*mem) = MEM_callocN(sizeof(IndexNode) * totedge * 2, "vert edge map mem");
	node = *mem;
	
	/* Find the users */
	for (i = 0; i < totedge; ++i) {
		for (j = 0; j < 2; ++j, ++node) {
			node->index = i;
			BLI_addtail(&(*map)[medge[i].v[j]], node);
		}
	}
}

static MultiresFace *find_old_face(ListBase *map, MultiresFace *faces, int v1, int v2, int v3, int v4)
{
	IndexNode *n1;
	int v[4], i, j;

	v[0] = v1;
	v[1] = v2;
	v[2] = v3;
	v[3] = v4;

	for (n1 = map[v1].first; n1; n1 = n1->next) {
		int fnd[4] = {0, 0, 0, 0};

		for (i = 0; i < 4; ++i) {
			for (j = 0; j < 4; ++j) {
				if (v[i] == faces[n1->index].v[j])
					fnd[i] = 1;
			}
		}

		if (fnd[0] && fnd[1] && fnd[2] && fnd[3])
			return &faces[n1->index];
	}

	return NULL;
}

static MultiresEdge *find_old_edge(ListBase *map, MultiresEdge *edges, int v1, int v2)
{
	IndexNode *n1, *n2;

	for (n1 = map[v1].first; n1; n1 = n1->next) {
		for (n2 = map[v2].first; n2; n2 = n2->next) {
			if (n1->index == n2->index)
				return &edges[n1->index];
		}
	}

	return NULL;
}

static void multires_load_old_edges(ListBase **emap, MultiresLevel *lvl, int *vvmap, int dst, int v1, int v2, int mov)
{
	int emid = find_old_edge(emap[2], lvl->edges, v1, v2)->mid;
	vvmap[dst + mov] = emid;

	if (lvl->next->next) {
		multires_load_old_edges(emap + 1, lvl->next, vvmap, dst + mov, v1, emid, mov / 2);
		multires_load_old_edges(emap + 1, lvl->next, vvmap, dst + mov, v2, emid, -mov / 2);
	}
}

static void multires_load_old_faces(ListBase **fmap, ListBase **emap, MultiresLevel *lvl, int *vvmap, int dst,
                                    int v1, int v2, int v3, int v4, int st2, int st3)
{
	int fmid;
	int emid13, emid14, emid23, emid24;

	if (lvl && lvl->next) {
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

		if (lvl->next->next) {
			multires_load_old_edges(emap, lvl->next, vvmap, dst, emid24, fmid, st3);
			multires_load_old_edges(emap, lvl->next, vvmap, dst, emid13, fmid, -st3);
			multires_load_old_edges(emap, lvl->next, vvmap, dst, emid14, fmid, -st2 * st3);
			multires_load_old_edges(emap, lvl->next, vvmap, dst, emid23, fmid, st2 * st3);
		}
	}
}

static void multires_mvert_to_ss(DerivedMesh *dm, MVert *mvert)
{
	CCGDerivedMesh *ccgdm = (CCGDerivedMesh *) dm;
	CCGSubSurf *ss = ccgdm->ss;
	CCGElem *vd;
	CCGKey key;
	int index;
	int totvert, totedge, totface;
	int gridSize = ccgSubSurf_getGridSize(ss);
	int edgeSize = ccgSubSurf_getEdgeSize(ss);
	int i = 0;

	dm->getGridKey(dm, &key);

	totface = ccgSubSurf_getNumFaces(ss);
	for (index = 0; index < totface; index++) {
		CCGFace *f = ccgdm->faceMap[index].face;
		int x, y, S, numVerts = ccgSubSurf_getFaceNumVerts(f);

		vd = ccgSubSurf_getFaceCenterData(f);
		copy_v3_v3(CCG_elem_co(&key, vd), mvert[i].co);
		i++;
		
		for (S = 0; S < numVerts; S++) {
			for (x = 1; x < gridSize - 1; x++, i++) {
				vd = ccgSubSurf_getFaceGridEdgeData(ss, f, S, x);
				copy_v3_v3(CCG_elem_co(&key, vd), mvert[i].co);
			}
		}

		for (S = 0; S < numVerts; S++) {
			for (y = 1; y < gridSize - 1; y++) {
				for (x = 1; x < gridSize - 1; x++, i++) {
					vd = ccgSubSurf_getFaceGridData(ss, f, S, x, y);
					copy_v3_v3(CCG_elem_co(&key, vd), mvert[i].co);
				}
			}
		}
	}

	totedge = ccgSubSurf_getNumEdges(ss);
	for (index = 0; index < totedge; index++) {
		CCGEdge *e = ccgdm->edgeMap[index].edge;
		int x;

		for (x = 1; x < edgeSize - 1; x++, i++) {
			vd = ccgSubSurf_getEdgeData(ss, e, x);
			copy_v3_v3(CCG_elem_co(&key, vd), mvert[i].co);
		}
	}

	totvert = ccgSubSurf_getNumVerts(ss);
	for (index = 0; index < totvert; index++) {
		CCGVert *v = ccgdm->vertMap[index].vert;

		vd = ccgSubSurf_getVertData(ss, v);
		copy_v3_v3(CCG_elem_co(&key, vd), mvert[i].co);
		i++;
	}

	ccgSubSurf_updateToFaces(ss, 0, NULL, 0);
}

/* Loads a multires object stored in the old Multires struct into the new format */
static void multires_load_old_dm(DerivedMesh *dm, Mesh *me, int totlvl)
{
	MultiresLevel *lvl, *lvl1;
	Multires *mr = me->mr;
	MVert *vsrc, *vdst;
	unsigned int src, dst;
	int st = multires_side_tot[totlvl - 1] - 1;
	int extedgelen = multires_side_tot[totlvl] - 2;
	int *vvmap; // inorder for dst, map to src
	int crossedgelen;
	int s, x, tottri, totquad;
	unsigned int i, j, totvert;

	src = 0;
	vsrc = mr->verts;
	vdst = dm->getVertArray(dm);
	totvert = (unsigned int)dm->getNumVerts(dm);
	vvmap = MEM_callocN(sizeof(int) * totvert, "multires vvmap");

	lvl1 = mr->levels.first;
	/* Load base verts */
	for (i = 0; i < lvl1->totvert; ++i) {
		vvmap[totvert - lvl1->totvert + i] = src;
		src++;
	}

	/* Original edges */
	dst = totvert - lvl1->totvert - extedgelen * lvl1->totedge;
	for (i = 0; i < lvl1->totedge; ++i) {
		int ldst = dst + extedgelen * i;
		int lsrc = src;
		lvl = lvl1->next;

		for (j = 2; j <= mr->level_count; ++j) {
			int base = multires_side_tot[totlvl - j + 1] - 2;
			int skip = multires_side_tot[totlvl - j + 2] - 1;
			int st = multires_side_tot[j - 1] - 1;

			for (x = 0; x < st; ++x)
				vvmap[ldst + base + x * skip] = lsrc + st * i + x;

			lsrc += lvl->totvert - lvl->prev->totvert;
			lvl = lvl->next;
		}
	}

	/* Center points */
	dst = 0;
	for (i = 0; i < lvl1->totface; ++i) {
		int sides = lvl1->faces[i].v[3] ? 4 : 3;

		vvmap[dst] = src + lvl1->totedge + i;
		dst += 1 + sides * (st - 1) * st;
	}


	/* The rest is only for level 3 and up */
	if (lvl1->next && lvl1->next->next) {
		ListBase **fmap, **emap;
		IndexNode **fmem, **emem;

		/* Face edge cross */
		tottri = totquad = 0;
		crossedgelen = multires_side_tot[totlvl - 1] - 2;
		dst = 0;
		for (i = 0; i < lvl1->totface; ++i) {
			int sides = lvl1->faces[i].v[3] ? 4 : 3;

			lvl = lvl1->next->next;
			dst++;

			for (j = 3; j <= mr->level_count; ++j) {
				int base = multires_side_tot[totlvl - j + 1] - 2;
				int skip = multires_side_tot[totlvl - j + 2] - 1;
				int st = pow(2, j - 2);
				int st2 = pow(2, j - 3);
				int lsrc = lvl->prev->totvert;

				/* Skip exterior edge verts */
				lsrc += lvl1->totedge * st;

				/* Skip earlier face edge crosses */
				lsrc += st2 * (tottri * 3 + totquad * 4);

				for (s = 0; s < sides; ++s) {
					for (x = 0; x < st2; ++x) {
						vvmap[dst + crossedgelen * (s + 1) - base - x * skip - 1] = lsrc;
						lsrc++;
					}
				}

				lvl = lvl->next;
			}

			dst += sides * (st - 1) * st;

			if (sides == 4) ++totquad;
			else ++tottri;

		}

		/* calculate vert to edge/face maps for each level (except the last) */
		fmap = MEM_callocN(sizeof(ListBase *) * (mr->level_count - 1), "multires fmap");
		emap = MEM_callocN(sizeof(ListBase *) * (mr->level_count - 1), "multires emap");
		fmem = MEM_callocN(sizeof(IndexNode *) * (mr->level_count - 1), "multires fmem");
		emem = MEM_callocN(sizeof(IndexNode *) * (mr->level_count - 1), "multires emem");
		lvl = lvl1;
		for (i = 0; i < (unsigned int)mr->level_count - 1; ++i) {
			create_old_vert_face_map(fmap + i, fmem + i, lvl->faces, lvl->totvert, lvl->totface);
			create_old_vert_edge_map(emap + i, emem + i, lvl->edges, lvl->totvert, lvl->totedge);
			lvl = lvl->next;
		}

		/* Interior face verts */
		/* lvl = lvl1->next->next; */ /* UNUSED */
		dst = 0;
		for (j = 0; j < lvl1->totface; ++j) {
			int sides = lvl1->faces[j].v[3] ? 4 : 3;
			int ldst = dst + 1 + sides * (st - 1);

			for (s = 0; s < sides; ++s) {
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

		/*lvl = lvl->next;*/ /*UNUSED*/

		for (i = 0; i < (unsigned int)(mr->level_count - 1); ++i) {
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
	for (i = 0; i < totvert; ++i)
		copy_v3_v3(vdst[i].co, vsrc[vvmap[i]].co);

	MEM_freeN(vvmap);

	multires_mvert_to_ss(dm, vdst);
}

/* Copy the first-level vcol data to the mesh, if it exists */
/* Warning: higher-level vcol data will be lost */
static void multires_load_old_vcols(Mesh *me)
{
	MultiresLevel *lvl;
	MultiresColFace *colface;
	MCol *mcol;
	int i, j;

	if (!(lvl = me->mr->levels.first))
		return;

	if (!(colface = lvl->colfaces))
		return;

	/* older multires format never supported multiple vcol layers,
	 * so we can assume the active vcol layer is the correct one */
	if (!(mcol = CustomData_get_layer(&me->fdata, CD_MCOL)))
		return;
	
	for (i = 0; i < me->totface; ++i) {
		for (j = 0; j < 4; ++j) {
			mcol[i * 4 + j].a = colface[i].col[j].a;
			mcol[i * 4 + j].r = colface[i].col[j].r;
			mcol[i * 4 + j].g = colface[i].col[j].g;
			mcol[i * 4 + j].b = colface[i].col[j].b;
		}
	}
}

/* Copy the first-level face-flag data to the mesh */
static void multires_load_old_face_flags(Mesh *me)
{
	MultiresLevel *lvl;
	MultiresFace *faces;
	int i;

	if (!(lvl = me->mr->levels.first))
		return;

	if (!(faces = lvl->faces))
		return;

	for (i = 0; i < me->totface; ++i)
		me->mface[i].flag = faces[i].flag;
}

void multires_load_old(Object *ob, Mesh *me)
{
	MultiresLevel *lvl;
	ModifierData *md;
	MultiresModifierData *mmd;
	DerivedMesh *dm, *orig;
	CustomDataLayer *l;
	int i;

	/* Load original level into the mesh */
	lvl = me->mr->levels.first;
	CustomData_free_layers(&me->vdata, CD_MVERT, lvl->totvert);
	CustomData_free_layers(&me->edata, CD_MEDGE, lvl->totedge);
	CustomData_free_layers(&me->fdata, CD_MFACE, lvl->totface);
	me->totvert = lvl->totvert;
	me->totedge = lvl->totedge;
	me->totface = lvl->totface;
	me->mvert = CustomData_add_layer(&me->vdata, CD_MVERT, CD_CALLOC, NULL, me->totvert);
	me->medge = CustomData_add_layer(&me->edata, CD_MEDGE, CD_CALLOC, NULL, me->totedge);
	me->mface = CustomData_add_layer(&me->fdata, CD_MFACE, CD_CALLOC, NULL, me->totface);
	memcpy(me->mvert, me->mr->verts, sizeof(MVert) * me->totvert);
	for (i = 0; i < me->totedge; ++i) {
		me->medge[i].v1 = lvl->edges[i].v[0];
		me->medge[i].v2 = lvl->edges[i].v[1];
	}
	for (i = 0; i < me->totface; ++i) {
		me->mface[i].v1 = lvl->faces[i].v[0];
		me->mface[i].v2 = lvl->faces[i].v[1];
		me->mface[i].v3 = lvl->faces[i].v[2];
		me->mface[i].v4 = lvl->faces[i].v[3];
		me->mface[i].mat_nr = lvl->faces[i].mat_nr;
	}

	/* Copy the first-level data to the mesh */
	/* XXX We must do this before converting tessfaces to polys/lopps! */
	for (i = 0, l = me->mr->vdata.layers; i < me->mr->vdata.totlayer; ++i, ++l)
		CustomData_add_layer(&me->vdata, l->type, CD_REFERENCE, l->data, me->totvert);
	for (i = 0, l = me->mr->fdata.layers; i < me->mr->fdata.totlayer; ++i, ++l)
		CustomData_add_layer(&me->fdata, l->type, CD_REFERENCE, l->data, me->totface);
	CustomData_reset(&me->mr->vdata);
	CustomData_reset(&me->mr->fdata);

	multires_load_old_vcols(me);
	multires_load_old_face_flags(me);

	/* multiresModifier_subdivide (actually, multires_subdivide) expects polys, not tessfaces! */
	BKE_mesh_convert_mfaces_to_mpolys(me);

	/* Add a multires modifier to the object */
	md = ob->modifiers.first;
	while (md && modifierType_getInfo(md->type)->type == eModifierTypeType_OnlyDeform)
		md = md->next;                          
	mmd = (MultiresModifierData *)modifier_new(eModifierType_Multires);
	BLI_insertlinkbefore(&ob->modifiers, md, mmd);

	for (i = 0; i < me->mr->level_count - 1; ++i)
		multiresModifier_subdivide(mmd, ob, 1, 0);

	mmd->lvl = mmd->totlvl;
	orig = CDDM_from_mesh(me);
	/* XXX We *must* alloc paint mask here, else we have some kind of mismatch in
	 *     multires_modifier_update_mdisps() (called by dm->release(dm)), which always creates the
	 *     reference subsurfed dm with this option, before calling multiresModifier_disp_run(),
	 *     which implicitly expects both subsurfs from its first dm and oldGridData parameters to
	 *     be of the same "format"! */
	dm = multires_make_derived_from_derived(orig, mmd, ob, 0);

	multires_load_old_dm(dm, me, mmd->totlvl + 1);

	multires_dm_mark_as_modified(dm, MULTIRES_COORDS_MODIFIED);
	dm->release(dm);
	orig->release(orig);

	/* Remove the old multires */
	multires_free(me->mr);
	me->mr = NULL;
}

/* If 'ob' and 'to_ob' both have multires modifiers, synchronize them
 * such that 'ob' has the same total number of levels as 'to_ob'. */
static void multires_sync_levels(Scene *scene, Object *ob, Object *to_ob)
{
	MultiresModifierData *mmd = get_multires_modifier(scene, ob, 1);
	MultiresModifierData *to_mmd = get_multires_modifier(scene, to_ob, 1);

	if (!mmd) {
		/* object could have MDISP even when there is no multires modifier
		 * this could lead to troubles due to i've got no idea how mdisp could be
		 * upsampled correct without modifier data.
		 * just remove mdisps if no multires present (nazgul) */

		multires_customdata_delete(ob->data);
	}

	if (mmd && to_mmd) {
		if (mmd->totlvl > to_mmd->totlvl)
			multires_del_higher(mmd, ob, to_mmd->totlvl);
		else
			multires_subdivide(mmd, ob, to_mmd->totlvl, 0, mmd->simple);
	}
}

static void multires_apply_smat(Scene *scene, Object *ob, float smat[3][3])
{
	DerivedMesh *dm = NULL, *cddm = NULL, *subdm = NULL;
	CCGElem **gridData, **subGridData;
	CCGKey dm_key, subdm_key;
	Mesh *me = (Mesh *)ob->data;
	MPoly *mpoly = me->mpoly;
	/* MLoop *mloop = me->mloop; */ /* UNUSED */
	MDisps *mdisps;
	int *gridOffset;
	int i, /*numGrids, */ gridSize, dGridSize, dSkip, totvert;
	float (*vertCos)[3] = NULL;
	MultiresModifierData *mmd = get_multires_modifier(scene, ob, 1);
	MultiresModifierData high_mmd;

	CustomData_external_read(&me->ldata, &me->id, CD_MASK_MDISPS, me->totloop);
	mdisps = CustomData_get_layer(&me->ldata, CD_MDISPS);

	if (!mdisps || !mmd || !mmd->totlvl) return;

	/* we need derived mesh created from highest resolution */
	high_mmd = *mmd;
	high_mmd.lvl = high_mmd.totlvl;

	/* unscaled multires with applied displacement */
	subdm = get_multires_dm(scene, &high_mmd, ob);

	/* prepare scaled CDDM to create ccgDN */
	cddm = mesh_get_derived_deform(scene, ob, CD_MASK_BAREMESH);

	totvert = cddm->getNumVerts(cddm);
	vertCos = MEM_mallocN(sizeof(*vertCos) * totvert, "multiresScale vertCos");
	cddm->getVertCos(cddm, vertCos);
	for (i = 0; i < totvert; i++)
		mul_m3_v3(smat, vertCos[i]);
	CDDM_apply_vert_coords(cddm, vertCos);
	MEM_freeN(vertCos);

	/* scaled ccgDM for tangent space of object with applied scale */
	dm = subsurf_dm_create_local(ob, cddm, high_mmd.totlvl, high_mmd.simple, 0, mmd->flags & eMultiresModifierFlag_PlainUv, 0);
	cddm->release(cddm);

	gridSize = dm->getGridSize(dm);
	gridData = dm->getGridData(dm);
	gridOffset = dm->getGridOffset(dm);
	dm->getGridKey(dm, &dm_key);
	subGridData = subdm->getGridData(subdm);
	subdm->getGridKey(subdm, &subdm_key);

	dGridSize = multires_side_tot[high_mmd.totlvl];
	dSkip = (dGridSize - 1) / (gridSize - 1);

#pragma omp parallel for private(i) if (me->totface * gridSize * gridSize * 4 >= CCG_OMP_LIMIT)
	for (i = 0; i < me->totpoly; ++i) {
		const int numVerts = mpoly[i].totloop;
		MDisps *mdisp = &mdisps[mpoly[i].loopstart];
		int S, x, y, gIndex = gridOffset[i];

		for (S = 0; S < numVerts; ++S, ++gIndex, mdisp++) {
			CCGElem *grid = gridData[gIndex];
			CCGElem *subgrid = subGridData[gIndex];
			float (*dispgrid)[3] = mdisp->disps;

			for (y = 0; y < gridSize; y++) {
				for (x = 0; x < gridSize; x++) {
					float *co = CCG_grid_elem_co(&dm_key, grid, x, y);
					float *sco = CCG_grid_elem_co(&subdm_key, subgrid, x, y);
					float *data = dispgrid[dGridSize * y * dSkip + x * dSkip];
					float mat[3][3], disp[3];

					/* construct tangent space matrix */
					grid_tangent_matrix(mat, &dm_key, x, y, grid);

					/* scale subgrid coord and calculate displacement */
					mul_m3_v3(smat, sco);
					sub_v3_v3v3(disp, sco, co);

					/* convert difference to tangent space */
					invert_m3(mat);
					mul_v3_m3v3(data, mat, disp);
				}
			}
		}
	}

	dm->release(dm);
	subdm->release(subdm);
}

int multires_mdisp_corners(MDisps *s)
{
	int lvl = 13;

	while (lvl > 0) {
		int side = (1 << (lvl - 1)) + 1;
		if ((s->totdisp % (side * side)) == 0) return s->totdisp / (side * side);
		lvl--;
	}

	return 0;
}

void multiresModifier_scale_disp(Scene *scene, Object *ob)
{
	float smat[3][3];

	/* object's scale matrix */
	BKE_object_scale_to_mat3(ob, smat);

	multires_apply_smat(scene, ob, smat);
}

void multiresModifier_prepare_join(Scene *scene, Object *ob, Object *to_ob)
{
	float smat[3][3], tmat[3][3], mat[3][3];
	multires_sync_levels(scene, ob, to_ob);

	/* construct scale matrix for displacement */
	BKE_object_scale_to_mat3(to_ob, tmat);
	invert_m3(tmat);
	BKE_object_scale_to_mat3(ob, smat);
	mul_m3_m3m3(mat, smat, tmat);

	multires_apply_smat(scene, ob, mat);
}

/* update multires data after topology changing */
void multires_topology_changed(Mesh *me)
{
	MDisps *mdisp = NULL, *cur = NULL;
	int i, grid = 0;

	CustomData_external_read(&me->ldata, &me->id, CD_MASK_MDISPS, me->totloop);
	mdisp = CustomData_get_layer(&me->ldata, CD_MDISPS);

	if (!mdisp)
		return;

	cur = mdisp;
	for (i = 0; i < me->totloop; i++, cur++) {
		if (cur->totdisp) {
			grid = mdisp->totdisp;

			break;
		}
	}

	for (i = 0; i < me->totloop; i++, mdisp++) {
		/* allocate memory for mdisp, the whole disp layer would be erased otherwise */
		if (!mdisp->totdisp || !mdisp->disps) {
			if (grid) {
				mdisp->totdisp = grid;
				mdisp->disps = MEM_callocN(3 * mdisp->totdisp * sizeof(float), "mdisp topology");
			}

			continue;
		}
	}
}

/***************** Multires interpolation stuff *****************/

/* Find per-corner coordinate with given per-face UV coord */
int mdisp_rot_face_to_crn(const int corners, const int face_side, const float u, const float v, float *x, float *y)
{
	const float offset = face_side * 0.5f - 0.5f;
	int S = 0;

	if (corners == 4) {
		if (u <= offset && v <= offset) S = 0;
		else if (u > offset  && v <= offset) S = 1;
		else if (u > offset  && v > offset) S = 2;
		else if (u <= offset && v >= offset) S = 3;

		if (S == 0) {
			*y = offset - u;
			*x = offset - v;
		}
		else if (S == 1) {
			*x = u - offset;
			*y = offset - v;
		}
		else if (S == 2) {
			*y = u - offset;
			*x = v - offset;
		}
		else if (S == 3) {
			*x = offset - u;
			*y = v - offset;
		}
	}
	else {
		int grid_size = offset;
		float w = (face_side - 1) - u - v;
		float W1, W2;

		if (u >= v && u >= w) {S = 0; W1 = w; W2 = v; }
		else if (v >= u && v >= w) {S = 1; W1 = u; W2 = w; }
		else {S = 2; W1 = v; W2 = u; }

		W1 /= (face_side - 1);
		W2 /= (face_side - 1);

		*x = (1 - (2 * W1) / (1 - W2)) * grid_size;
		*y = (1 - (2 * W2) / (1 - W1)) * grid_size;
	}

	return S;
}
