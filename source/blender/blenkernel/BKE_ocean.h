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
 * Contributors: Matt Ebb
 *
 * ***** END GPL LICENSE BLOCK *****
 */

#ifndef __BKE_OCEAN_H__
#define __BKE_OCEAN_H__

/** \file BKE_ocean.h
 *  \ingroup bli
 */

#ifdef __cplusplus
extern "C" {
#endif

struct OceanModifierData;

typedef struct OceanResult {
	float disp[3];
	float normal[3];
	float foam;

	/* raw eigenvalues/vectors */
	float Jminus;
	float Jplus;
	float Eminus[3];
	float Eplus[3];
} OceanResult;

typedef struct OceanCache {
	struct ImBuf **ibufs_disp;
	struct ImBuf **ibufs_foam;
	struct ImBuf **ibufs_norm;

	const char *bakepath;
	const char *relbase;

	/* precalculated for time range */
	float *time;

	/* constant for time range */
	float wave_scale;
	float chop_amount;
	float foam_coverage;
	float foam_fade;

	int start;
	int end;
	int duration;
	int resolution_x;
	int resolution_y;

	int baked;
} OceanCache;

struct Ocean *BKE_ocean_add(void);
void BKE_ocean_free_data(struct Ocean *oc);
void BKE_ocean_free(struct Ocean *oc);
bool BKE_ocean_ensure(struct OceanModifierData *omd);
void BKE_ocean_init_from_modifier(struct Ocean *ocean, struct OceanModifierData const *omd);

void BKE_ocean_init(
        struct Ocean *o, int M, int N, float Lx, float Lz, float V, float l, float A, float w, float damp,
        float alignment, float depth, float time, short do_height_field, short do_chop, short do_normals, short do_jacobian, int seed);
void BKE_ocean_simulate(struct Ocean *o, float t, float scale, float chop_amount);

/* sampling the ocean surface */
float BKE_ocean_jminus_to_foam(float jminus, float coverage);
void  BKE_ocean_eval_uv(struct Ocean *oc, struct OceanResult *ocr, float u, float v);
void  BKE_ocean_eval_uv_catrom(struct Ocean *oc, struct OceanResult *ocr, float u, float v);
void  BKE_ocean_eval_xz(struct Ocean *oc, struct OceanResult *ocr, float x, float z);
void  BKE_ocean_eval_xz_catrom(struct Ocean *oc, struct OceanResult *ocr, float x, float z);
void  BKE_ocean_eval_ij(struct Ocean *oc, struct OceanResult *ocr, int i, int j);


/* ocean cache handling */
struct OceanCache *BKE_ocean_init_cache(
        const char *bakepath, const char *relbase,
        int start, int end, float wave_scale,
        float chop_amount, float foam_coverage, float foam_fade, int resolution);
void BKE_ocean_simulate_cache(struct OceanCache *och, int frame);

void BKE_ocean_bake(struct Ocean *o, struct OceanCache *och, void (*update_cb)(void *, float progress, int *cancel), void *update_cb_data);
void BKE_ocean_cache_eval_uv(struct OceanCache *och, struct OceanResult *ocr, int f, float u, float v);
void BKE_ocean_cache_eval_ij(struct OceanCache *och, struct OceanResult *ocr, int f, int i, int j);

void BKE_ocean_free_cache(struct OceanCache *och);
void BKE_ocean_free_modifier_cache(struct OceanModifierData *omd);

#ifdef __cplusplus
}
#endif

#endif
