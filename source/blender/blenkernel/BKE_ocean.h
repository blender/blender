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
 * The Original Code is Copyright (C) 2001-2002 by NaN Holding BV.
 * All rights reserved.
 *
 * Contributors: Matt Ebb
 *
 * ***** END GPL LICENSE BLOCK *****
 */

#ifndef __BKE_OCEAN_H__
#define __BKE_OCEAN_H__

#ifdef __cplusplus
extern "C" {
#endif

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


#define OCEAN_NOT_CACHED    0
#define OCEAN_CACHING       1
#define OCEAN_CACHED        2

struct Ocean *BKE_add_ocean(void);
void BKE_free_ocean_data(struct Ocean *oc);
void BKE_free_ocean(struct Ocean *oc);

void BKE_init_ocean(struct Ocean *o, int M, int N, float Lx, float Lz, float V, float l, float A, float w, float damp,
                    float alignment, float depth, float time, short do_height_field, short do_chop, short do_normals, short do_jacobian, int seed);
void BKE_simulate_ocean(struct Ocean *o, float t, float scale, float chop_amount);

/* sampling the ocean surface */
float BKE_ocean_jminus_to_foam(float jminus, float coverage);
void  BKE_ocean_eval_uv(struct Ocean *oc, struct OceanResult *ocr, float u, float v);
void  BKE_ocean_eval_uv_catrom(struct Ocean *oc, struct OceanResult *ocr, float u, float v);
void  BKE_ocean_eval_xz(struct Ocean *oc, struct OceanResult *ocr, float x, float z);
void  BKE_ocean_eval_xz_catrom(struct Ocean *oc, struct OceanResult *ocr, float x, float z);
void  BKE_ocean_eval_ij(struct Ocean *oc, struct OceanResult *ocr, int i, int j);


/* ocean cache handling */
struct OceanCache *BKE_init_ocean_cache(const char *bakepath, const char *relbase,
                                        int start, int end, float wave_scale,
                                        float chop_amount, float foam_coverage, float foam_fade, int resolution);
void BKE_simulate_ocean_cache(struct OceanCache *och, int frame);
	
void BKE_bake_ocean(struct Ocean *o, struct OceanCache *och, void (*update_cb)(void *, float progress, int *cancel), void *update_cb_data);
void BKE_ocean_cache_eval_uv(struct OceanCache *och, struct OceanResult *ocr, int f, float u, float v);
void BKE_ocean_cache_eval_ij(struct OceanCache *och, struct OceanResult *ocr, int f, int i, int j);

void BKE_free_ocean_cache(struct OceanCache *och);
#ifdef __cplusplus
}
#endif

#endif
