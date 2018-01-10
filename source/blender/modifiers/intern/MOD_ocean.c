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

/** \file blender/modifiers/intern/MOD_ocean.c
 *  \ingroup modifiers
 */

#include "DNA_customdata_types.h"
#include "DNA_object_types.h"
#include "DNA_meshdata_types.h"
#include "DNA_modifier_types.h"
#include "DNA_scene_types.h"

#include "BLI_math.h"
#include "BLI_math_inline.h"
#include "BLI_task.h"
#include "BLI_utildefines.h"

#include "BKE_cdderivedmesh.h"
#include "BKE_modifier.h"
#include "BKE_ocean.h"

#include "MOD_modifiertypes.h"

#ifdef WITH_OCEANSIM
static void init_cache_data(Object *ob, struct OceanModifierData *omd)
{
	const char *relbase = modifier_path_relbase(ob);

	omd->oceancache = BKE_ocean_init_cache(omd->cachepath, relbase,
	                                       omd->bakestart, omd->bakeend, omd->wave_scale,
	                                       omd->chop_amount, omd->foam_coverage, omd->foam_fade, omd->resolution);
}

static void clear_cache_data(struct OceanModifierData *omd)
{
	BKE_ocean_free_cache(omd->oceancache);
	omd->oceancache = NULL;
	omd->cached = false;
}

/* keep in sync with init_ocean_modifier_bake(), object_modifier.c */
static void init_ocean_modifier(struct OceanModifierData *omd)
{
	int do_heightfield, do_chop, do_normals, do_jacobian;

	if (!omd || !omd->ocean) return;

	do_heightfield = true;
	do_chop = (omd->chop_amount > 0);
	do_normals = (omd->flag & MOD_OCEAN_GENERATE_NORMALS);
	do_jacobian = (omd->flag & MOD_OCEAN_GENERATE_FOAM);

	BKE_ocean_free_data(omd->ocean);
	BKE_ocean_init(omd->ocean, omd->resolution * omd->resolution, omd->resolution * omd->resolution,
	               omd->spatial_size, omd->spatial_size,
	               omd->wind_velocity, omd->smallest_wave, 1.0, omd->wave_direction, omd->damp, omd->wave_alignment,
	               omd->depth, omd->time,
	               do_heightfield, do_chop, do_normals, do_jacobian,
	               omd->seed);
}

static void simulate_ocean_modifier(struct OceanModifierData *omd)
{
	if (!omd || !omd->ocean) return;

	BKE_ocean_simulate(omd->ocean, omd->time, omd->wave_scale, omd->chop_amount);
}
#endif /* WITH_OCEANSIM */



/* Modifier Code */

static void initData(ModifierData *md)
{
#ifdef WITH_OCEANSIM
	OceanModifierData *omd = (OceanModifierData *) md;

	omd->resolution = 7;
	omd->spatial_size = 50;

	omd->wave_alignment = 0.0;
	omd->wind_velocity = 30.0;

	omd->damp = 0.5;
	omd->smallest_wave = 0.01;
	omd->wave_direction = 0.0;
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
	omd->foamlayername[0] = '\0';   /* layer name empty by default */

	omd->ocean = BKE_ocean_add();
	init_ocean_modifier(omd);
	simulate_ocean_modifier(omd);
#else  /* WITH_OCEANSIM */
	   /* unused */
	(void)md;
#endif /* WITH_OCEANSIM */
}

static void freeData(ModifierData *md)
{
#ifdef WITH_OCEANSIM
	OceanModifierData *omd = (OceanModifierData *) md;

	BKE_ocean_free(omd->ocean);
	if (omd->oceancache)
		BKE_ocean_free_cache(omd->oceancache);
#else /* WITH_OCEANSIM */
	/* unused */
	(void)md;
#endif /* WITH_OCEANSIM */
}

static void copyData(ModifierData *md, ModifierData *target)
{
#ifdef WITH_OCEANSIM
#if 0
	OceanModifierData *omd = (OceanModifierData *) md;
#endif
	OceanModifierData *tomd = (OceanModifierData *) target;

	freeData(target);

	modifier_copyData_generic(md, target);

	tomd->refresh = 0;

	/* XXX todo: copy cache runtime too */
	tomd->cached = 0;
	tomd->oceancache = NULL;

	tomd->ocean = BKE_ocean_add();
	init_ocean_modifier(tomd);
	simulate_ocean_modifier(tomd);
#else /* WITH_OCEANSIM */
	/* unused */
	(void)md;
	(void)target;
#endif /* WITH_OCEANSIM */
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
#else /* WITH_OCEANSIM */
static CustomDataMask requiredDataMask(Object *UNUSED(ob), ModifierData *md)
{
	/* unused */
	(void)md;
	return 0;
}
#endif /* WITH_OCEANSIM */

static bool dependsOnNormals(ModifierData *md)
{
	OceanModifierData *omd = (OceanModifierData *)md;
	return (omd->geometry_mode != MOD_OCEAN_GEOM_GENERATE);
}

#if 0
static void dm_get_bounds(DerivedMesh *dm, float *sx, float *sy, float *ox, float *oy)
{
	/* get bounding box of underlying dm */
	int v, totvert = dm->getNumVerts(dm);
	float min[3], max[3], delta[3];

	MVert *mvert = dm->getVertDataArray(dm, 0);

	copy_v3_v3(min, mvert->co);
	copy_v3_v3(max, mvert->co);

	for (v = 1; v < totvert; v++, mvert++) {
		min[0] = min_ff(min[0], mvert->co[0]);
		min[1] = min_ff(min[1], mvert->co[1]);
		min[2] = min_ff(min[2], mvert->co[2]);

		max[0] = max_ff(max[0], mvert->co[0]);
		max[1] = max_ff(max[1], mvert->co[1]);
		max[2] = max_ff(max[2], mvert->co[2]);
	}

	sub_v3_v3v3(delta, max, min);

	*sx = delta[0];
	*sy = delta[1];

	*ox = min[0];
	*oy = min[1];
}
#endif

#ifdef WITH_OCEANSIM

typedef struct GenerateOceanGeometryData {
	MVert *mverts;
	MPoly *mpolys;
	MLoop *mloops;
	int *origindex;
	MLoopUV *mloopuvs;

	int res_x, res_y;
	int rx, ry;
	float ox, oy;
	float sx, sy;
	float ix, iy;
} GenerateOceanGeometryData;

static void generate_ocean_geometry_vertices(
        void *__restrict userdata,
        const int y,
        const ParallelRangeTLS *__restrict UNUSED(tls))
{
	GenerateOceanGeometryData *gogd = userdata;
	int x;

	for (x = 0; x <= gogd->res_x; x++) {
		const int i = y * (gogd->res_x + 1) + x;
		float *co = gogd->mverts[i].co;
		co[0] = gogd->ox + (x * gogd->sx);
		co[1] = gogd->oy + (y * gogd->sy);
		co[2] = 0.0f;
	}
}

static void generate_ocean_geometry_polygons(
        void *__restrict userdata,
        const int y,
        const ParallelRangeTLS *__restrict UNUSED(tls))
{
	GenerateOceanGeometryData *gogd = userdata;
	int x;

	for (x = 0; x < gogd->res_x; x++) {
		const int fi = y * gogd->res_x + x;
		const int vi = y * (gogd->res_x + 1) + x;
		MPoly *mp = &gogd->mpolys[fi];
		MLoop *ml = &gogd->mloops[fi * 4];

		ml->v = vi;
		ml++;
		ml->v = vi + 1;
		ml++;
		ml->v = vi + 1 + gogd->res_x + 1;
		ml++;
		ml->v = vi + gogd->res_x + 1;
		ml++;

		mp->loopstart = fi * 4;
		mp->totloop = 4;

		mp->flag |= ME_SMOOTH;

		/* generated geometry does not map to original faces */
		gogd->origindex[fi] = ORIGINDEX_NONE;
	}
}

static void generate_ocean_geometry_uvs(
        void *__restrict userdata,
        const int y,
        const ParallelRangeTLS *__restrict UNUSED(tls))
{
	GenerateOceanGeometryData *gogd = userdata;
	int x;

	for (x = 0; x < gogd->res_x; x++) {
		const int i = y * gogd->res_x + x;
		MLoopUV *luv = &gogd->mloopuvs[i * 4];

		luv->uv[0] = x * gogd->ix;
		luv->uv[1] = y * gogd->iy;
		luv++;

		luv->uv[0] = (x + 1) * gogd->ix;
		luv->uv[1] = y * gogd->iy;
		luv++;

		luv->uv[0] = (x + 1) * gogd->ix;
		luv->uv[1] = (y + 1) * gogd->iy;
		luv++;

		luv->uv[0] = x * gogd->ix;
		luv->uv[1] = (y + 1) * gogd->iy;
		luv++;
	}
}

static DerivedMesh *generate_ocean_geometry(OceanModifierData *omd)
{
	DerivedMesh *result;

	GenerateOceanGeometryData gogd;

	int num_verts;
	int num_polys;

	const bool use_threading = omd->resolution > 4;

	gogd.rx = omd->resolution * omd->resolution;
	gogd.ry = omd->resolution * omd->resolution;
	gogd.res_x = gogd.rx * omd->repeat_x;
	gogd.res_y = gogd.ry * omd->repeat_y;

	num_verts = (gogd.res_x + 1) * (gogd.res_y + 1);
	num_polys = gogd.res_x * gogd.res_y;

	gogd.sx = omd->size * omd->spatial_size;
	gogd.sy = omd->size * omd->spatial_size;
	gogd.ox = -gogd.sx / 2.0f;
	gogd.oy = -gogd.sy / 2.0f;

	gogd.sx /= gogd.rx;
	gogd.sy /= gogd.ry;

	result = CDDM_new(num_verts, 0, 0, num_polys * 4, num_polys);

	gogd.mverts = CDDM_get_verts(result);
	gogd.mpolys = CDDM_get_polys(result);
	gogd.mloops = CDDM_get_loops(result);

	gogd.origindex = CustomData_get_layer(&result->polyData, CD_ORIGINDEX);

	ParallelRangeSettings settings;
	BLI_parallel_range_settings_defaults(&settings);
	settings.use_threading = use_threading;

	/* create vertices */
	BLI_task_parallel_range(0, gogd.res_y + 1, &gogd, generate_ocean_geometry_vertices, &settings);

	/* create faces */
	BLI_task_parallel_range(0, gogd.res_y, &gogd, generate_ocean_geometry_polygons, &settings);

	CDDM_calc_edges(result);

	/* add uvs */
	if (CustomData_number_of_layers(&result->loopData, CD_MLOOPUV) < MAX_MTFACE) {
		gogd.mloopuvs = CustomData_add_layer(&result->loopData, CD_MLOOPUV, CD_CALLOC, NULL, num_polys * 4);
		CustomData_add_layer(&result->polyData, CD_MTEXPOLY, CD_CALLOC, NULL, num_polys);

		if (gogd.mloopuvs) { /* unlikely to fail */
			gogd.ix = 1.0 / gogd.rx;
			gogd.iy = 1.0 / gogd.ry;

			BLI_task_parallel_range(0, gogd.res_y, &gogd, generate_ocean_geometry_uvs, &settings);
		}
	}

	result->dirty |= DM_DIRTY_NORMALS;

	return result;
}

static DerivedMesh *doOcean(ModifierData *md, Object *ob,
                            DerivedMesh *derivedData,
                            int UNUSED(useRenderParams))
{
	OceanModifierData *omd = (OceanModifierData *) md;

	DerivedMesh *dm = NULL;
	OceanResult ocr;

	MVert *mverts;

	int cfra;
	int i, j;

	/* use cached & inverted value for speed
	 * expanded this would read...
	 *
	 * (axis / (omd->size * omd->spatial_size)) + 0.5f) */
#define OCEAN_CO(_size_co_inv, _v) ((_v * _size_co_inv) + 0.5f)

	const float size_co_inv = 1.0f / (omd->size * omd->spatial_size);

	/* can happen in when size is small, avoid bad array lookups later and quit now */
	if (!isfinite(size_co_inv)) {
		return derivedData;
	}

	/* update modifier */
	if (omd->refresh & MOD_OCEAN_REFRESH_ADD) {
		omd->ocean = BKE_ocean_add();
	}
	if (omd->refresh & MOD_OCEAN_REFRESH_RESET) {
		init_ocean_modifier(omd);
	}
	if (omd->refresh & MOD_OCEAN_REFRESH_CLEAR_CACHE) {
		clear_cache_data(omd);
	}
	omd->refresh = 0;

	/* do ocean simulation */
	if (omd->cached == true) {
		if (!omd->oceancache) {
			init_cache_data(ob, omd);
		}
		BKE_ocean_simulate_cache(omd->oceancache, md->scene->r.cfra);
	}
	else {
		simulate_ocean_modifier(omd);
	}

	if (omd->geometry_mode == MOD_OCEAN_GEOM_GENERATE) {
		dm = generate_ocean_geometry(omd);
		DM_ensure_normals(dm);
	}
	else if (omd->geometry_mode == MOD_OCEAN_GEOM_DISPLACE) {
		dm = CDDM_copy(derivedData);
	}

	cfra = md->scene->r.cfra;
	CLAMP(cfra, omd->bakestart, omd->bakeend);
	cfra -= omd->bakestart; /* shift to 0 based */

	mverts = dm->getVertArray(dm);

	/* add vcols before displacement - allows lookup based on position */

	if (omd->flag & MOD_OCEAN_GENERATE_FOAM) {
		if (CustomData_number_of_layers(&dm->loopData, CD_MLOOPCOL) < MAX_MCOL) {
			const int num_polys = dm->getNumPolys(dm);
			const int num_loops = dm->getNumLoops(dm);
			MLoop *mloops = dm->getLoopArray(dm);
			MLoopCol *mloopcols = CustomData_add_layer_named(
			                          &dm->loopData, CD_MLOOPCOL, CD_CALLOC, NULL, num_loops, omd->foamlayername);

			if (mloopcols) { /* unlikely to fail */
				MPoly *mpolys = dm->getPolyArray(dm);
				MPoly *mp;

				for (i = 0, mp = mpolys; i < num_polys; i++, mp++) {
					MLoop *ml = &mloops[mp->loopstart];
					MLoopCol *mlcol = &mloopcols[mp->loopstart];

					for (j = mp->totloop; j--; ml++, mlcol++) {
						const float *vco = mverts[ml->v].co;
						const float u = OCEAN_CO(size_co_inv, vco[0]);
						const float v = OCEAN_CO(size_co_inv, vco[1]);
						float foam;

						if (omd->oceancache && omd->cached == true) {
							BKE_ocean_cache_eval_uv(omd->oceancache, &ocr, cfra, u, v);
							foam = ocr.foam;
							CLAMP(foam, 0.0f, 1.0f);
						}
						else {
							BKE_ocean_eval_uv(omd->ocean, &ocr, u, v);
							foam = BKE_ocean_jminus_to_foam(ocr.Jminus, omd->foam_coverage);
						}

						mlcol->r = mlcol->g = mlcol->b = (char)(foam * 255);
						/* This needs to be set (render engine uses) */
						mlcol->a = 255;
					}
				}
			}
		}
	}


	/* displace the geometry */

	/* Note: tried to parallelized that one and previous foam loop, but gives 20% slower results... odd. */
	{
		const int num_verts = dm->getNumVerts(dm);

		for (i = 0; i < num_verts; i++) {
			float *vco = mverts[i].co;
			const float u = OCEAN_CO(size_co_inv, vco[0]);
			const float v = OCEAN_CO(size_co_inv, vco[1]);

			if (omd->oceancache && omd->cached == true) {
				BKE_ocean_cache_eval_uv(omd->oceancache, &ocr, cfra, u, v);
			}
			else {
				BKE_ocean_eval_uv(omd->ocean, &ocr, u, v);
			}

			vco[2] += ocr.disp[1];

			if (omd->chop_amount > 0.0f) {
				vco[0] += ocr.disp[0];
				vco[1] += ocr.disp[2];
			}
		}
	}

#undef OCEAN_CO

	return dm;
}
#else  /* WITH_OCEANSIM */
static DerivedMesh *doOcean(ModifierData *md, Object *UNUSED(ob),
                            DerivedMesh *derivedData,
                            int UNUSED(useRenderParams))
{
	/* unused */
	(void)md;
	return derivedData;
}
#endif /* WITH_OCEANSIM */

static DerivedMesh *applyModifier(ModifierData *md, Object *ob,
                                  DerivedMesh *derivedData,
                                  ModifierApplyFlag UNUSED(flag))
{
	DerivedMesh *result;

	result = doOcean(md, ob, derivedData, 0);

	if (result != derivedData)
		result->dirty |= DM_DIRTY_NORMALS;

	return result;
}


ModifierTypeInfo modifierType_Ocean = {
	/* name */              "Ocean",
	/* structName */        "OceanModifierData",
	/* structSize */        sizeof(OceanModifierData),
	/* type */              eModifierTypeType_Constructive,
	/* flags */             eModifierTypeFlag_AcceptsMesh |
	                        eModifierTypeFlag_SupportsEditmode |
	                        eModifierTypeFlag_EnableInEditmode,

	/* copyData */          copyData,
	/* deformMatrices */    NULL,
	/* deformVerts */       NULL,
	/* deformVertsEM */     NULL,
	/* deformMatricesEM */  NULL,
	/* applyModifier */     applyModifier,
	/* applyModifierEM */   NULL,
	/* initData */          initData,
	/* requiredDataMask */  requiredDataMask,
	/* freeData */          freeData,
	/* isDisabled */        NULL,
	/* updateDepgraph */    NULL,
	/* updateDepsgraph */   NULL,
	/* dependsOnTime */     NULL,
	/* dependsOnNormals */	dependsOnNormals,
	/* foreachObjectLink */ NULL,
	/* foreachIDLink */     NULL,
	/* foreachTexLink */    NULL,
};
