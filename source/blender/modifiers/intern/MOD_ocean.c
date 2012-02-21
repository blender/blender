/**
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
 * Inc., 51 Franklin Street, Fifth Floor, Boston, MA 02110-1301, USA.
 *
 * The Original Code is Copyright (C) Blender Foundation
 * All rights reserved.
 *
 * The Original Code is: all of this file.
 *
 * Contributor(s): Matt Ebb
 *
 * ***** END GPL LICENSE BLOCK *****
 */

#include "MEM_guardedalloc.h"

#include "DNA_customdata_types.h"
#include "DNA_object_types.h"
#include "DNA_meshdata_types.h"
#include "DNA_modifier_types.h"
#include "DNA_scene_types.h"

#include "BKE_cdderivedmesh.h"
#include "BKE_global.h"
#include "BKE_modifier.h"
#include "BKE_ocean.h"
#include "BKE_utildefines.h"

#include "BLI_blenlib.h"
#include "BLI_math.h"
#include "BLI_math_inline.h"
#include "BLI_utildefines.h"
#include "BLI_string.h"

#include "MOD_util.h"

#ifdef WITH_OCEANSIM
static void init_cache_data(Object *ob, struct OceanModifierData *omd)
{
	const char *relbase= modifier_path_relbase(ob);

	omd->oceancache = BKE_init_ocean_cache(omd->cachepath, relbase,
	                                       omd->bakestart, omd->bakeend, omd->wave_scale,
	                                       omd->chop_amount, omd->foam_coverage, omd->foam_fade, omd->resolution);
}

static void clear_cache_data(struct OceanModifierData *omd)
{
	BKE_free_ocean_cache(omd->oceancache);
	omd->oceancache = NULL;
	omd->cached = FALSE;
}

/* keep in sync with init_ocean_modifier_bake(), object_modifier.c */
static void init_ocean_modifier(struct OceanModifierData *omd)
{
	int do_heightfield, do_chop, do_normals, do_jacobian;

	if (!omd || !omd->ocean) return;

	do_heightfield = TRUE;
	do_chop = (omd->chop_amount > 0);
	do_normals = (omd->flag & MOD_OCEAN_GENERATE_NORMALS);
	do_jacobian = (omd->flag & MOD_OCEAN_GENERATE_FOAM);

	BKE_free_ocean_data(omd->ocean);
	BKE_init_ocean(omd->ocean, omd->resolution*omd->resolution, omd->resolution*omd->resolution, omd->spatial_size, omd->spatial_size,
	               omd->wind_velocity, omd->smallest_wave, 1.0, omd->wave_direction, omd->damp, omd->wave_alignment,
	               omd->depth, omd->time,
	               do_heightfield, do_chop, do_normals, do_jacobian,
	               omd->seed);
}

static void simulate_ocean_modifier(struct OceanModifierData *omd)
{
	if (!omd || !omd->ocean) return;

	BKE_simulate_ocean(omd->ocean, omd->time, omd->wave_scale, omd->chop_amount);
}
#endif /* WITH_OCEANSIM */



/* Modifier Code */

static void initData(ModifierData *md)
{
#ifdef WITH_OCEANSIM
	OceanModifierData *omd = (OceanModifierData*) md;

	omd->resolution = 7;
	omd->spatial_size = 50;

	omd->wave_alignment = 0.0;
	omd->wind_velocity = 30.0;

	omd->damp = 0.5;
	omd->smallest_wave = 0.01;
	omd->wave_direction= 0.0;
	omd->depth = 200.0;

	omd->wave_scale = 1.0;

	omd->chop_amount = 1.0;

	omd->foam_coverage = 0.0;

	omd->seed = 0;
	omd->time = 1.0;

	omd->refresh = 0;

	omd->size = 1.0;
	omd->repeat_x = 1;
	omd->repeat_y = 1;

	modifier_path_init(omd->cachepath, sizeof(omd->cachepath), "cache_ocean");

	omd->cached = 0;
	omd->bakestart = 1;
	omd->bakeend = 250;
	omd->oceancache = NULL;
	omd->foam_fade = 0.98;
	omd->foamlayername[0] = '\0';	/* layer name empty by default */

	omd->ocean = BKE_add_ocean();
	init_ocean_modifier(omd);
	simulate_ocean_modifier(omd);
#else  // WITH_OCEANSIM
	/* unused */
	(void)md;
#endif // WITH_OCEANSIM
}

static void freeData(ModifierData *md)
{
#ifdef WITH_OCEANSIM
	OceanModifierData *omd = (OceanModifierData*) md;

	BKE_free_ocean(omd->ocean);
	if (omd->oceancache)
		BKE_free_ocean_cache(omd->oceancache);
#else // WITH_OCEANSIM
	/* unused */
	(void)md;
#endif // WITH_OCEANSIM
}

static void copyData(ModifierData *md, ModifierData *target)
{
#ifdef WITH_OCEANSIM
	OceanModifierData *omd = (OceanModifierData*) md;
	OceanModifierData *tomd = (OceanModifierData*) target;

	tomd->resolution = omd->resolution;
	tomd->spatial_size = omd->spatial_size;

	tomd->wind_velocity = omd->wind_velocity;

	tomd->damp = omd->damp;
	tomd->smallest_wave = omd->smallest_wave;
	tomd->depth = omd->depth;

	tomd->wave_alignment = omd->wave_alignment;
	tomd->wave_direction = omd->wave_direction;
	tomd->wave_scale = omd->wave_scale;

	tomd->chop_amount = omd->chop_amount;
	tomd->foam_coverage = omd->foam_coverage;
	tomd->time = omd->time;

	tomd->seed = omd->seed;
	tomd->flag = omd->flag;

	tomd->refresh = 0;


	tomd->size = omd->size;
	tomd->repeat_x = omd->repeat_x;
	tomd->repeat_y = omd->repeat_y;

	/* XXX todo: copy cache runtime too */
	tomd->cached = 0;
	tomd->bakestart = omd->bakestart;
	tomd->bakeend = omd->bakeend;
	tomd->oceancache = NULL;

	tomd->ocean = BKE_add_ocean();
	init_ocean_modifier(tomd);
	simulate_ocean_modifier(tomd);
#else // WITH_OCEANSIM
	/* unused */
	(void)md;
	(void)target;
#endif // WITH_OCEANSIM
}

#ifdef WITH_OCEANSIM
static CustomDataMask requiredDataMask(Object *UNUSED(ob), ModifierData *md)
{
	OceanModifierData *omd = (OceanModifierData *)md;
	CustomDataMask dataMask = 0;

	if (omd->flag & MOD_OCEAN_GENERATE_FOAM)
		dataMask |= CD_MASK_MCOL;

	return dataMask;
}
#else // WITH_OCEANSIM
static CustomDataMask requiredDataMask(Object *UNUSED(ob), ModifierData *md)
{
	/* unused */
	(void)md;
	return 0;
}
#endif // WITH_OCEANSIM

#if 0
static void dm_get_bounds(DerivedMesh *dm, float *sx, float *sy, float *ox, float *oy)
{
	/* get bounding box of underlying dm */
	int v, totvert=dm->getNumVerts(dm);
	float min[3], max[3], delta[3];

	MVert *mvert = dm->getVertDataArray(dm,0);

	copy_v3_v3(min, mvert->co);
	copy_v3_v3(max, mvert->co);

	for(v=1; v<totvert; v++, mvert++) {
		min[0]=MIN2(min[0],mvert->co[0]);
		min[1]=MIN2(min[1],mvert->co[1]);
		min[2]=MIN2(min[2],mvert->co[2]);

		max[0]=MAX2(max[0],mvert->co[0]);
		max[1]=MAX2(max[1],mvert->co[1]);
		max[2]=MAX2(max[2],mvert->co[2]);
	}

	sub_v3_v3v3(delta, max, min);

	*sx = delta[0];
	*sy = delta[1];

	*ox = min[0];
	*oy = min[1];
}
#endif

#ifdef WITH_OCEANSIM


#define OMP_MIN_RES	18
static DerivedMesh *generate_ocean_geometry(OceanModifierData *omd)
{
	DerivedMesh *result;

	MVert *mverts;
	MPoly *mpolys;
	MLoop *mloops;
	int *origindex;

	int cdlayer;

	const int rx = omd->resolution*omd->resolution;
	const int ry = omd->resolution*omd->resolution;
	const int res_x = rx * omd->repeat_x;
	const int res_y = ry * omd->repeat_y;

	const int num_verts = (res_x + 1) * (res_y + 1);
	/* const int num_edges = (res_x * res_y * 2) + res_x + res_y; */ /* UNUSED BMESH */
	const int num_faces = res_x * res_y;

	float sx = omd->size * omd->spatial_size;
	float sy = omd->size * omd->spatial_size;
	const float ox = -sx / 2.0f;
	const float oy = -sy / 2.0f;

	float ix, iy;

	int x, y;

	sx /= rx;
	sy /= ry;

	result = CDDM_new(num_verts, 0, 0, num_faces * 4, num_faces);

	mverts = CDDM_get_verts(result);
	mpolys = CDDM_get_polys(result);
	mloops = CDDM_get_loops(result);

#if 0 // trunk
	origindex= result->getFaceDataArray(result, CD_ORIGINDEX);
#else // bmesh
	origindex= CustomData_get_layer(&result->polyData, CD_ORIGINDEX);
#endif

	/* create vertices */
	#pragma omp parallel for private(x, y) if (rx > OMP_MIN_RES)
	for (y=0; y < res_y+1; y++) {
		for (x=0; x < res_x+1; x++) {
			const int i = y*(res_x+1) + x;
			float *co= mverts[i].co;
			co[0] = ox + (x * sx);
			co[1] = oy + (y * sy);
			co[2] = 0;
		}
	}

	/* create faces */
	#pragma omp parallel for private(x, y) if (rx > OMP_MIN_RES)
	for (y=0; y < res_y; y++) {
		for (x=0; x < res_x; x++) {
			const int fi = y*res_x + x;
			const int vi = y*(res_x+1) + x;
			MPoly *mp= &mpolys[fi];
			MLoop *ml= &mloops[fi * 4];

			ml->v = vi;
			ml++;
			ml->v = vi + 1;
			ml++;
			ml->v = vi + 1 + res_x + 1;
			ml++;
			ml->v = vi + res_x + 1;
			ml++;

			mp->loopstart= fi * 4;
			mp->totloop= 4;

			mp->flag |= ME_SMOOTH;

			/* generated geometry does not map to original faces */
			origindex[fi] = ORIGINDEX_NONE;
		}
	}

	CDDM_calc_edges(result);

	/* add uvs */
	cdlayer= CustomData_number_of_layers(&result->loopData, CD_MLOOPUV);
	if(cdlayer < MAX_MTFACE) {
		MLoopUV *mloopuvs= CustomData_add_layer(&result->loopData, CD_MLOOPUV, CD_CALLOC, NULL, num_faces * 4);

		if (mloopuvs) { /* unlikely to fail */
			ix = 1.0 / rx;
			iy = 1.0 / ry;
			#pragma omp parallel for private(x, y) if (rx > OMP_MIN_RES)
			for (y=0; y < res_y; y++) {
				for (x=0; x < res_x; x++) {
					const int i = y*res_x + x;
					MLoopUV *luv= &mloopuvs[i * 4];

					luv->uv[0] = x * ix;
					luv->uv[1] = y * iy;
					luv++;

					luv->uv[0] = (x+1) * ix;
					luv->uv[1] = y * iy;
					luv++;

					luv->uv[0] = (x+1) * ix;
					luv->uv[1] = (y+1) * iy;
					luv++;

					luv->uv[0] = x * ix;
					luv->uv[1] = (y+1) * iy;
					luv++;

				}
			}
		}
	}

	return result;
}

static DerivedMesh *doOcean(ModifierData *md, Object *ob,
                            DerivedMesh *derivedData,
                            int UNUSED(useRenderParams))
{
	OceanModifierData *omd = (OceanModifierData*) md;

	DerivedMesh *dm=NULL;
	OceanResult ocr;

	MVert *mverts, *mv;
	MLoop *mloops;

	int i, j;

	int num_verts;
	int num_faces;

	int cfra;

	/* use cached & inverted value for speed
	 * expanded this would read...
	 *
	 * (axis / (omd->size * omd->spatial_size)) + 0.5f) */
#define OCEAN_CO(_size_co_inv, _v) ((_v * _size_co_inv) + 0.5f)

	const float size_co_inv= 1.0f / (omd->size * omd->spatial_size);

	/* update modifier */
	if (omd->refresh & MOD_OCEAN_REFRESH_ADD)
		omd->ocean = BKE_add_ocean();
	if (omd->refresh & MOD_OCEAN_REFRESH_RESET)
		init_ocean_modifier(omd);
	if (omd->refresh & MOD_OCEAN_REFRESH_CLEAR_CACHE)
		clear_cache_data(omd);

	omd->refresh = 0;

	/* do ocean simulation */
	if (omd->cached == TRUE) {
		if (!omd->oceancache) init_cache_data(ob, omd);
		BKE_simulate_ocean_cache(omd->oceancache, md->scene->r.cfra);
	}
	else {
		simulate_ocean_modifier(omd);
	}

	if (omd->geometry_mode == MOD_OCEAN_GEOM_GENERATE)
		dm = generate_ocean_geometry(omd);
	else if (omd->geometry_mode == MOD_OCEAN_GEOM_DISPLACE) {
		dm = CDDM_copy(derivedData);
	}

	cfra = md->scene->r.cfra;
	CLAMP(cfra, omd->bakestart, omd->bakeend);
	cfra -= omd->bakestart;	// shift to 0 based

	num_verts = dm->getNumVerts(dm);
	num_faces = dm->getNumPolys(dm);

	mverts = dm->getVertArray(dm);
	mloops = dm->getLoopArray(dm);

	/* add vcols before displacement - allows lookup based on position */

	if (omd->flag & MOD_OCEAN_GENERATE_FOAM) {
		int cdlayer= CustomData_number_of_layers(&dm->loopData, CD_MLOOPCOL);

		if(cdlayer < MAX_MCOL) {
			MLoopCol *mloopcols= CustomData_add_layer_named(&dm->loopData, CD_MLOOPCOL, CD_CALLOC, NULL, num_faces * 4, omd->foamlayername);

			if (mloopcols) { /* unlikely to fail */
				MLoopCol *mlcol;
				MPoly *mpolys= dm->getPolyArray(dm);
				MPoly *mp;

				float foam;

				for (i = 0, mp= mpolys; i < num_faces; i++, mp++) {
					j= mp->totloop - 1;

					/* highly unlikely */
					if (j <= 0) continue;

					do {
						const float *co= mverts[mloops[mp->loopstart + j].v].co;
						const float u = OCEAN_CO(size_co_inv, co[0]);
						const float v = OCEAN_CO(size_co_inv, co[1]);

						if (omd->oceancache && omd->cached==TRUE) {
							BKE_ocean_cache_eval_uv(omd->oceancache, &ocr, cfra, u, v);
							foam = ocr.foam;
							CLAMP(foam, 0.0f, 1.0f);
						}
						else {
							BKE_ocean_eval_uv(omd->ocean, &ocr, u, v);
							foam = BKE_ocean_jminus_to_foam(ocr.Jminus, omd->foam_coverage);
						}

						mlcol= &mloopcols[mp->loopstart + j];
						mlcol->r = mlcol->g = mlcol->b = (char)(foam * 255);
						/* mc->a = 255; */ /* no need to set */
					} while (j--);
				}
			}
		}
	}


	/* displace the geometry */

	//#pragma omp parallel for private(i, ocr) if (omd->resolution > OMP_MIN_RES)
	for (i=0, mv= mverts; i< num_verts; i++, mv++) {
		const float u = OCEAN_CO(size_co_inv, mv->co[0]);
		const float v = OCEAN_CO(size_co_inv, mv->co[1]);

		if (omd->oceancache && omd->cached==TRUE)
			BKE_ocean_cache_eval_uv(omd->oceancache, &ocr, cfra, u, v);
		else
			BKE_ocean_eval_uv(omd->ocean, &ocr, u, v);

		mv->co[2] += ocr.disp[1];

		if (omd->chop_amount > 0.0f) {
			mv->co[0] += ocr.disp[0];
			mv->co[1] += ocr.disp[2];
		}
	}

	#undef OCEAN_CO

	return dm;
}
#else  // WITH_OCEANSIM
static DerivedMesh *doOcean(ModifierData *md, Object *UNUSED(ob),
							  DerivedMesh *derivedData,
							  int UNUSED(useRenderParams))
{
	/* unused */
	(void)md;
	return derivedData;
}
#endif // WITH_OCEANSIM

static DerivedMesh *applyModifier(ModifierData *md, Object *ob,
								  DerivedMesh *derivedData,
								  int UNUSED(useRenderParams),
								  int UNUSED(isFinalCalc))
{
	DerivedMesh *result;

	result = doOcean(md, ob, derivedData, 0);

	if(result != derivedData)
		CDDM_calc_normals(result);

	return result;
}

static DerivedMesh *applyModifierEM(ModifierData *md, Object *ob,
									struct BMEditMesh *UNUSED(editData),
									DerivedMesh *derivedData)
{
	return applyModifier(md, ob, derivedData, 0, 1);
}



ModifierTypeInfo modifierType_Ocean = {
	/* name */              "Ocean",
	/* structName */        "OceanModifierData",
	/* structSize */        sizeof(OceanModifierData),
	/* type */              eModifierTypeType_Constructive,
	/* flags */             eModifierTypeFlag_AcceptsMesh
							| eModifierTypeFlag_SupportsEditmode
							| eModifierTypeFlag_EnableInEditmode,

	/* copyData */          copyData,
	/* deformMatrices */    NULL,
	/* deformVerts */       NULL,
	/* deformVertsEM */     NULL,
	/* deformMatricesEM */  NULL,
	/* applyModifier */     applyModifier,
	/* applyModifierEM */   applyModifierEM,
	/* initData */          initData,
	/* requiredDataMask */  requiredDataMask,
	/* freeData */          freeData,
	/* isDisabled */        NULL,
	/* updateDepgraph */    NULL,
	/* dependsOnTime */     NULL,
	/* dependsOnNormals */	NULL,
	/* foreachObjectLink */ NULL,
	/* foreachIDLink */     NULL,
};
