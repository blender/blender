/*
 * Copyright 2016, Blender Foundation.
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
 * Contributor(s): Blender Institute
 *
 */

/** \file eevee_lights.c
 *  \ingroup DNA
 */

#include "DRW_render.h"

#include "eevee.h"
#include "eevee_private.h"

typedef struct EEVEE_LightData {
	short light_id, shadow_id;
} EEVEE_LightData;

typedef struct EEVEE_ShadowCubeData {
	short light_id, shadow_id;
	float viewprojmat[6][4][4];
} EEVEE_ShadowCubeData;

typedef struct EEVEE_ShadowMapData {
	short light_id, shadow_id;
	float viewprojmat[4][4]; /* World->Lamp->NDC : used for rendering the shadow map. */
} EEVEE_ShadowMapData;

typedef struct EEVEE_ShadowCascadeData {
	short light_id, shadow_id;
	float viewprojmat[MAX_CASCADE_NUM][4][4]; /* World->Lamp->NDC : used for rendering the shadow map. */
} EEVEE_ShadowCascadeData;

/* *********** FUNCTIONS *********** */

void EEVEE_lights_init(EEVEE_StorageList *stl)
{
	const unsigned int shadow_ubo_size = sizeof(EEVEE_ShadowCube) * MAX_SHADOW_CUBE +
	                                     sizeof(EEVEE_ShadowMap) * MAX_SHADOW_MAP +
	                                     sizeof(EEVEE_ShadowCascade) * MAX_SHADOW_CASCADE;

	if (!stl->lamps) {
		stl->lamps  = MEM_callocN(sizeof(EEVEE_LampsInfo), "EEVEE_LampsInfo");
		stl->light_ubo   = DRW_uniformbuffer_create(sizeof(EEVEE_Light) * MAX_LIGHT, NULL);
		stl->shadow_ubo  = DRW_uniformbuffer_create(shadow_ubo_size, NULL);
	}
}

void EEVEE_lights_cache_init(EEVEE_StorageList *stl)
{
	EEVEE_LampsInfo *linfo = stl->lamps;

	linfo->num_light = linfo->num_cube = linfo->num_map = linfo->num_cascade = 0;
	memset(linfo->light_ref, 0, sizeof(linfo->light_ref));
	memset(linfo->shadow_cube_ref, 0, sizeof(linfo->shadow_cube_ref));
	memset(linfo->shadow_map_ref, 0, sizeof(linfo->shadow_map_ref));
	memset(linfo->shadow_cascade_ref, 0, sizeof(linfo->shadow_cascade_ref));
}

void EEVEE_lights_cache_add(EEVEE_StorageList *stl, Object *ob)
{
	EEVEE_LampsInfo *linfo = stl->lamps;

	/* Step 1 find all lamps in the scene and setup them */
	if (linfo->num_light > MAX_LIGHT) {
		printf("Too much lamps in the scene !!!\n");
		linfo->num_light = MAX_LIGHT;
	}
	else {
		Lamp *la = (Lamp *)ob->data;
		EEVEE_LampEngineData *led = (EEVEE_LampEngineData *)DRW_lamp_engine_data_get(ob, &viewport_eevee_type);

		DRW_lamp_engine_data_free((void *)led);

		if (la->mode & (LA_SHAD_BUF | LA_SHAD_RAY)) {
			if (la->type == LA_SUN && linfo->num_map < MAX_SHADOW_MAP) {
				led->sto = MEM_mallocN(sizeof(EEVEE_ShadowMapData), "EEVEE_ShadowMapData");
				((EEVEE_ShadowMapData *)led->sto)->shadow_id = linfo->num_map;
				linfo->shadow_map_ref[linfo->num_map] = ob;
				linfo->num_map++;
			}
			else if ((la->type == LA_SPOT || la->type == LA_LOCAL || la->type == LA_AREA)
			          && linfo->num_cube < MAX_SHADOW_CUBE) {
				led->sto = MEM_mallocN(sizeof(EEVEE_ShadowCubeData), "EEVEE_ShadowCubeData");
				((EEVEE_ShadowCubeData *)led->sto)->shadow_id = linfo->num_cube;
				linfo->shadow_cube_ref[linfo->num_cube] = ob;
				linfo->num_cube++;
			}
		}

		if (!led->sto) {
			led->sto = MEM_mallocN(sizeof(EEVEE_LightData), "EEVEE_LightData");
			((EEVEE_LightData *)led->sto)->shadow_id = -1;
		}

		((EEVEE_LightData *)led->sto)->light_id = linfo->num_light;
		linfo->light_ref[linfo->num_light] = ob;
		linfo->num_light++;
	}
}

void EEVEE_lights_cache_finish(EEVEE_StorageList *stl, EEVEE_TextureList *txl, EEVEE_FramebufferList *fbl)
{
	EEVEE_LampsInfo *linfo = stl->lamps;

	/* Step 4 Update Lamp UBOs */
	EEVEE_lights_update(stl);

	/* Step 5 Setup enough layers */
	/* Free textures if number mismatch */
	if (linfo->num_cube != linfo->cache_num_cube) {
		if (txl->shadow_depth_cube_pool) {
			DRW_texture_free(txl->shadow_depth_cube_pool);
			txl->shadow_depth_cube_pool = NULL;
		}
		linfo->cache_num_cube = linfo->num_cube;
	}
	if (linfo->num_map != linfo->cache_num_map) {
		if (txl->shadow_depth_map_pool) {
			DRW_texture_free(txl->shadow_depth_map_pool);
			txl->shadow_depth_map_pool = NULL;
		}
		linfo->cache_num_map = linfo->num_map;
	}
	if (linfo->num_cascade != linfo->cache_num_cascade) {
		if (txl->shadow_depth_cascade_pool) {
			DRW_texture_free(txl->shadow_depth_cascade_pool);
			txl->shadow_depth_cascade_pool = NULL;
		}
		linfo->cache_num_cascade = linfo->num_cascade;
	}

	/* Initialize Textures Arrays first so DRW_framebuffer_init just bind them */
	if (!txl->shadow_depth_cube_pool) {
		txl->shadow_depth_cube_pool = DRW_texture_create_2D_array(512, 512, MAX2(1, linfo->num_cube * 6), DRW_TEX_DEPTH_24, DRW_TEX_FILTER | DRW_TEX_COMPARE, NULL);
		if (fbl->shadow_cube_fb)
			DRW_framebuffer_texture_attach(fbl->shadow_cube_fb, txl->shadow_depth_cube_pool, 0);
	}
	if (!txl->shadow_depth_map_pool) {
		txl->shadow_depth_map_pool = DRW_texture_create_2D_array(512, 512, MAX2(1, linfo->num_map), DRW_TEX_DEPTH_24, DRW_TEX_FILTER | DRW_TEX_COMPARE, NULL);
		if (fbl->shadow_map_fb)
			DRW_framebuffer_texture_attach(fbl->shadow_map_fb, txl->shadow_depth_map_pool, 0);
	}
	if (!txl->shadow_depth_cascade_pool) {
		txl->shadow_depth_cascade_pool = DRW_texture_create_2D_array(512, 512, MAX2(1, linfo->num_cascade), DRW_TEX_DEPTH_24, DRW_TEX_FILTER | DRW_TEX_COMPARE, NULL);
		if (fbl->shadow_cascade_fb)
			DRW_framebuffer_texture_attach(fbl->shadow_cascade_fb, txl->shadow_depth_map_pool, 0);
	}

	DRWFboTexture tex_cube = {&txl->shadow_depth_cube_pool, DRW_BUF_DEPTH_24, DRW_TEX_FILTER | DRW_TEX_COMPARE};
	DRW_framebuffer_init(&fbl->shadow_cube_fb, 512, 512, &tex_cube, 1);

	DRWFboTexture tex_map = {&txl->shadow_depth_map_pool, DRW_BUF_DEPTH_24, DRW_TEX_FILTER | DRW_TEX_COMPARE};
	DRW_framebuffer_init(&fbl->shadow_map_fb, 512, 512, &tex_map, 1);

	DRWFboTexture tex_cascade = {&txl->shadow_depth_cascade_pool, DRW_BUF_DEPTH_24, DRW_TEX_FILTER | DRW_TEX_COMPARE};
	DRW_framebuffer_init(&fbl->shadow_cascade_fb, 512, 512, &tex_cascade, 1);
}

/* Update buffer with lamp data */
static void eevee_light_setup(Object *ob, EEVEE_LampsInfo *linfo, EEVEE_LampEngineData *led)
{
	/* TODO only update if data changes */
	EEVEE_LightData *evld = led->sto;
	EEVEE_Light *evli = linfo->light_data + evld->light_id;
	Lamp *la = (Lamp *)ob->data;
	float mat[4][4], scale[3], power;

	/* Position */
	copy_v3_v3(evli->position, ob->obmat[3]);

	/* Color */
	evli->color[0] = la->r * la->energy;
	evli->color[1] = la->g * la->energy;
	evli->color[2] = la->b * la->energy;

	/* Influence Radius */
	evli->dist = la->dist;

	/* Vectors */
	normalize_m4_m4_ex(mat, ob->obmat, scale);
	copy_v3_v3(evli->forwardvec, mat[2]);
	normalize_v3(evli->forwardvec);
	negate_v3(evli->forwardvec);

	copy_v3_v3(evli->rightvec, mat[0]);
	normalize_v3(evli->rightvec);

	copy_v3_v3(evli->upvec, mat[1]);
	normalize_v3(evli->upvec);

	/* Spot size & blend */
	if (la->type == LA_SPOT) {
		evli->sizex = scale[0] / scale[2];
		evli->sizey = scale[1] / scale[2];
		evli->spotsize = cosf(la->spotsize * 0.5f);
		evli->spotblend = (1.0f - evli->spotsize) * la->spotblend;
		evli->radius = MAX2(0.001f, la->area_size);
	}
	else if (la->type == LA_AREA) {
		evli->sizex = MAX2(0.0001f, la->area_size * scale[0] * 0.5f);
		if (la->area_shape == LA_AREA_RECT) {
			evli->sizey = MAX2(0.0001f, la->area_sizey * scale[1] * 0.5f);
		}
		else {
			evli->sizey = evli->sizex;
		}
	}
	else {
		evli->radius = MAX2(0.001f, la->area_size);
	}

	/* Make illumination power constant */
	if (la->type == LA_AREA) {
		power = 1.0f / (evli->sizex * evli->sizey * 4.0f * M_PI) /* 1/(w*h*Pi) */
		        * 80.0f; /* XXX : Empirical, Fit cycles power */
	}
	else if (la->type == LA_SPOT || la->type == LA_LOCAL) {
		power = 1.0f / (4.0f * evli->radius * evli->radius * M_PI * M_PI) /* 1/(4*r²*Pi²) */
		        * M_PI * M_PI * M_PI * 10.0; /* XXX : Empirical, Fit cycles power */

		/* for point lights (a.k.a radius == 0.0) */
		// power = M_PI * M_PI * 0.78; /* XXX : Empirical, Fit cycles power */
	}
	else {
		power = 1.0f;
	}
	mul_v3_fl(evli->color, power);

	/* Lamp Type */
	evli->lamptype = (float)la->type;

	/* No shadow by default */
	evli->shadowid = -1.0f;
}

static float texcomat[4][4] = { /* From NDC to TexCo */
	{0.5, 0.0, 0.0, 0.0},
	{0.0, 0.5, 0.0, 0.0},
	{0.0, 0.0, 0.5, 0.0},
	{0.5, 0.5, 0.5, 1.0}
};

static float cubefacemat[6][4][4] = {
	/* Pos X */
	{{0.0, 0.0, -1.0, 0.0},
	 {0.0, -1.0, 0.0, 0.0},
	 {-1.0, 0.0, 0.0, 0.0},
	 {0.0, 0.0, 0.0, 1.0}},
	/* Neg X */
	{{0.0, 0.0, 1.0, 0.0},
	 {0.0, -1.0, 0.0, 0.0},
	 {1.0, 0.0, 0.0, 0.0},
	 {0.0, 0.0, 0.0, 1.0}},
	/* Pos Y */
	{{1.0, 0.0, 0.0, 0.0},
	 {0.0, 0.0, 1.0, 0.0},
	 {0.0, -1.0, 0.0, 0.0},
	 {0.0, 0.0, 0.0, 1.0}},
	/* Neg Y */
	{{1.0, 0.0, 0.0, 0.0},
	 {0.0, 0.0, -1.0, 0.0},
	 {0.0, 1.0, 0.0, 0.0},
	 {0.0, 0.0, 0.0, 1.0}},
	/* Pos Z */
	{{1.0, 0.0, 0.0, 0.0},
	 {0.0, -1.0, 0.0, 0.0},
	 {0.0, 0.0, -1.0, 0.0},
	 {0.0, 0.0, 0.0, 1.0}},
	/* Neg Z */
	{{-1.0, 0.0, 0.0, 0.0},
	 {0.0, -1.0, 0.0, 0.0},
	 {0.0, 0.0, 1.0, 0.0},
	 {0.0, 0.0, 0.0, 1.0}},
};

static void eevee_shadow_cube_setup(Object *ob, EEVEE_LampsInfo *linfo, EEVEE_LampEngineData *led)
{
	float projmat[4][4];

	EEVEE_ShadowCubeData *evsmp = (EEVEE_ShadowCubeData *)led->sto;
	EEVEE_Light *evli = linfo->light_data + evsmp->light_id;
	EEVEE_ShadowCube *evsh = linfo->shadow_cube_data + evsmp->shadow_id;
	Lamp *la = (Lamp *)ob->data;

	perspective_m4(projmat, -la->clipsta, la->clipsta, -la->clipsta, la->clipsta, la->clipsta, la->clipend);

	for (int i = 0; i < 6; ++i) {
		float tmp[4][4];
		unit_m4(tmp);
		negate_v3_v3(tmp[3], ob->obmat[3]);
		mul_m4_m4m4(tmp, cubefacemat[i], tmp);
		mul_m4_m4m4(evsmp->viewprojmat[i], projmat, tmp);
	}

	evsh->bias = 0.05f * la->bias;
	evsh->near = la->clipsta;
	evsh->far = la->clipend;

	evli->shadowid = (float)(evsmp->shadow_id);
}

static void eevee_shadow_map_setup(Object *ob, EEVEE_LampsInfo *linfo, EEVEE_LampEngineData *led)
{
	float viewmat[4][4], projmat[4][4];

	EEVEE_ShadowMapData *evsmp = (EEVEE_ShadowMapData *)led->sto;
	EEVEE_Light *evli = linfo->light_data + evsmp->light_id;
	EEVEE_ShadowMap *evsh = linfo->shadow_map_data + evsmp->shadow_id;
	Lamp *la = (Lamp *)ob->data;

	invert_m4_m4(viewmat, ob->obmat);
	normalize_v3(viewmat[0]);
	normalize_v3(viewmat[1]);
	normalize_v3(viewmat[2]);

	float wsize = la->shadow_frustum_size;
	orthographic_m4(projmat, -wsize, wsize, -wsize, wsize, la->clipsta, la->clipend);

	mul_m4_m4m4(evsmp->viewprojmat, projmat, viewmat);
	mul_m4_m4m4(evsh->shadowmat, texcomat, evsmp->viewprojmat);

	evsh->bias = 0.005f * la->bias;

	evli->shadowid = (float)(MAX_SHADOW_CUBE + evsmp->shadow_id);
}

void EEVEE_lights_update(EEVEE_StorageList *stl)
{
	EEVEE_LampsInfo *linfo = stl->lamps;
	Object *ob;
	int i;

	for (i = 0; (ob = linfo->light_ref[i]) && (i < MAX_LIGHT); i++) {
		EEVEE_LampEngineData *led = (EEVEE_LampEngineData *)DRW_lamp_engine_data_get(ob, &viewport_eevee_type);
		eevee_light_setup(ob, linfo, led);
	}

	for (i = 0; (ob = linfo->shadow_cube_ref[i]) && (i < MAX_SHADOW_CUBE); i++) {
		EEVEE_LampEngineData *led = (EEVEE_LampEngineData *)DRW_lamp_engine_data_get(ob, &viewport_eevee_type);
		eevee_shadow_cube_setup(ob, linfo, led);
	}

	for (i = 0; (ob = linfo->shadow_map_ref[i]) && (i < MAX_SHADOW_MAP); i++) {
		EEVEE_LampEngineData *led = (EEVEE_LampEngineData *)DRW_lamp_engine_data_get(ob, &viewport_eevee_type);
		eevee_shadow_map_setup(ob, linfo, led);
	}

	// for (i = 0; (ob = linfo->shadow_cascade_ref[i]) && (i < MAX_SHADOW_CASCADE); i++) {
	// 	EEVEE_LampEngineData *led = (EEVEE_LampEngineData *)DRW_lamp_engine_data_get(ob, &viewport_eevee_type);
	// 	eevee_shadow_map_setup(ob, linfo, led);
	// }

	DRW_uniformbuffer_update(stl->light_ubo, &linfo->light_data);
	DRW_uniformbuffer_update(stl->shadow_ubo, &linfo->shadow_cube_data); /* Update all data at once */
}

/* this refresh lamps shadow buffers */
void EEVEE_draw_shadows(EEVEE_Data *vedata)
{
	EEVEE_StorageList *stl = vedata->stl;
	EEVEE_FramebufferList *fbl = vedata->fbl;
	EEVEE_LampsInfo *linfo = stl->lamps;
	Object *ob;
	int i;

	/* Cube Shadow Maps */
	/* For old hardware support, we render each face of the shadow map
	 * onto 6 layer of a big 2D texture array and sample manualy the right layer
	 * in the fragment shader. */
	DRW_framebuffer_bind(fbl->shadow_cube_fb);
	DRW_framebuffer_clear(false, true, false, NULL, 1.0);

	/* Render each shadow to one layer of the array */
	for (i = 0; (ob = linfo->shadow_cube_ref[i]) && (i < MAX_SHADOW_CUBE); i++) {
		EEVEE_LampEngineData *led = (EEVEE_LampEngineData *)DRW_lamp_engine_data_get(ob, &viewport_eevee_type);
		EEVEE_ShadowCubeData *evscd = (EEVEE_ShadowCubeData *)led->sto;

		for (int j = 0; j < 6; ++j) {
			linfo->layer = i * 6 + j;
			copy_m4_m4(linfo->shadowmat, evscd->viewprojmat[j]);
			DRW_draw_pass(vedata->psl->shadow_pass);
		}
	}

	/* Standard Shadow Maps */
	DRW_framebuffer_bind(fbl->shadow_map_fb);
	DRW_framebuffer_clear(false, true, false, NULL, 1.0);

	/* Render each shadow to one layer of the array */
	for (i = 0; (ob = linfo->shadow_map_ref[i]) && (i < MAX_SHADOW_MAP); i++) {
		EEVEE_LampEngineData *led = (EEVEE_LampEngineData *)DRW_lamp_engine_data_get(ob, &viewport_eevee_type);
		EEVEE_ShadowMapData *evsmd = (EEVEE_ShadowMapData *)led->sto;

		linfo->layer = i;
		copy_m4_m4(linfo->shadowmat, evsmd->viewprojmat);
		DRW_draw_pass(vedata->psl->shadow_pass);
	}

	// DRW_framebuffer_bind(e_data.shadow_cascade_fb);
}
