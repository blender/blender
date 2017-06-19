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

#include "BKE_object.h"

#include "eevee_engine.h"
#include "eevee_private.h"

/* Theses are the structs stored inside Objects.
 * It works with even if the object is in multiple layers
 * because we don't get the same "Object *" for each layer. */
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

typedef struct ShadowCaster {
	struct ShadowCaster *next, *prev;
	void *ob;
	bool prune;
} ShadowCaster;

static struct {
	struct GPUShader *shadow_sh;
	struct GPUShader *shadow_store_sh;
} e_data = {NULL}; /* Engine data */

extern char datatoc_shadow_vert_glsl[];
extern char datatoc_shadow_geom_glsl[];
extern char datatoc_shadow_frag_glsl[];
extern char datatoc_shadow_store_vert_glsl[];
extern char datatoc_shadow_store_geom_glsl[];
extern char datatoc_shadow_store_frag_glsl[];

/* *********** FUNCTIONS *********** */

void EEVEE_lights_init(EEVEE_SceneLayerData *sldata)
{
	const unsigned int shadow_ubo_size = sizeof(EEVEE_ShadowCube) * MAX_SHADOW_CUBE +
	                                     sizeof(EEVEE_ShadowMap) * MAX_SHADOW_MAP +
	                                     sizeof(EEVEE_ShadowCascade) * MAX_SHADOW_CASCADE;

	if (!e_data.shadow_sh) {
		e_data.shadow_sh = DRW_shader_create(
		        datatoc_shadow_vert_glsl, datatoc_shadow_geom_glsl, datatoc_shadow_frag_glsl, NULL);

		e_data.shadow_store_sh = DRW_shader_create(
		        datatoc_shadow_store_vert_glsl, datatoc_shadow_store_geom_glsl, datatoc_shadow_store_frag_glsl, NULL);
	}

	if (!sldata->lamps) {
		sldata->lamps              = MEM_callocN(sizeof(EEVEE_LampsInfo), "EEVEE_LampsInfo");
		sldata->light_ubo          = DRW_uniformbuffer_create(sizeof(EEVEE_Light) * MAX_LIGHT, NULL);
		sldata->shadow_ubo         = DRW_uniformbuffer_create(shadow_ubo_size, NULL);
		sldata->shadow_render_ubo  = DRW_uniformbuffer_create(sizeof(EEVEE_ShadowRender), NULL);
	}
}

void EEVEE_lights_cache_init(EEVEE_SceneLayerData *sldata, EEVEE_PassList *psl)
{
	EEVEE_LampsInfo *linfo = sldata->lamps;

	linfo->num_light = linfo->num_cube = linfo->num_map = linfo->num_cascade = 0;
	memset(linfo->light_ref, 0, sizeof(linfo->light_ref));
	memset(linfo->shadow_cube_ref, 0, sizeof(linfo->shadow_cube_ref));
	memset(linfo->shadow_map_ref, 0, sizeof(linfo->shadow_map_ref));
	memset(linfo->shadow_cascade_ref, 0, sizeof(linfo->shadow_cascade_ref));

	{
		psl->shadow_cube_store_pass = DRW_pass_create("Shadow Storage Pass", DRW_STATE_WRITE_COLOR);

		DRWShadingGroup *grp = DRW_shgroup_create(e_data.shadow_store_sh, psl->shadow_cube_store_pass);
		DRW_shgroup_uniform_buffer(grp, "shadowCube", &sldata->shadow_color_cube_target);
		DRW_shgroup_uniform_block(grp, "shadow_render_block", sldata->shadow_render_ubo);
		DRW_shgroup_call_add(grp, DRW_cache_fullscreen_quad_get(), NULL);
	}

	{
		psl->shadow_cube_pass = DRW_pass_create("Shadow Cube Pass", DRW_STATE_WRITE_COLOR | DRW_STATE_WRITE_DEPTH | DRW_STATE_DEPTH_LESS);
	}

	{
		psl->shadow_cascade_pass = DRW_pass_create("Shadow Cascade Pass", DRW_STATE_WRITE_DEPTH | DRW_STATE_DEPTH_LESS);
	}

	/* Reset shadow casters list */
	BLI_freelistN(&sldata->shadow_casters);
}

void EEVEE_lights_cache_add(EEVEE_SceneLayerData *sldata, Object *ob)
{
	EEVEE_LampsInfo *linfo = sldata->lamps;

	/* Step 1 find all lamps in the scene and setup them */
	if (linfo->num_light > MAX_LIGHT) {
		printf("Too much lamps in the scene !!!\n");
		linfo->num_light = MAX_LIGHT;
	}
	else {
		Lamp *la = (Lamp *)ob->data;
		EEVEE_LampEngineData *led = EEVEE_lamp_data_get(ob);

		if ((ob->deg_update_flag & DEG_RUNTIME_DATA_UPDATE) != 0) {
			led->need_update = true;
		}

		MEM_SAFE_FREE(led->storage);

#if 1 /* TODO Waiting for notified refresh. only on scene change. Else too much perf cost. */
		if (la->mode & (LA_SHAD_BUF | LA_SHAD_RAY)) {
			if (la->type == LA_SUN && linfo->num_cascade < MAX_SHADOW_CASCADE) {
#if 0 /* TODO filter cascaded shadow map */
				led->storage = MEM_mallocN(sizeof(EEVEE_ShadowCascadeData), "EEVEE_ShadowCascadeData");
				((EEVEE_ShadowCascadeData *)led->storage)->shadow_id = linfo->num_cascade;
				linfo->shadow_cascade_ref[linfo->num_cascade] = ob;
				linfo->num_cascade++;
#endif
			}
			else if ((la->type == LA_SPOT || la->type == LA_LOCAL || la->type == LA_AREA)
			          && linfo->num_cube < MAX_SHADOW_CUBE) {
				led->storage = MEM_mallocN(sizeof(EEVEE_ShadowCubeData), "EEVEE_ShadowCubeData");
				((EEVEE_ShadowCubeData *)led->storage)->shadow_id = linfo->num_cube;
				linfo->shadow_cube_ref[linfo->num_cube] = ob;
				linfo->num_cube++;
			}

		}
#else
		UNUSED_VARS(la);
#endif
		/* Default light without shadows */
		if (!led->storage) {
			led->storage = MEM_mallocN(sizeof(EEVEE_LightData), "EEVEE_LightData");
			((EEVEE_LightData *)led->storage)->shadow_id = -1;
		}

		((EEVEE_LightData *)led->storage)->light_id = linfo->num_light;
		linfo->light_ref[linfo->num_light] = ob;
		linfo->num_light++;
	}
}

/* Add a shadow caster to the shadowpasses */
void EEVEE_lights_cache_shcaster_add(EEVEE_SceneLayerData *sldata, EEVEE_PassList *psl, struct Gwn_Batch *geom, float (*obmat)[4])
{
	DRWShadingGroup *grp = DRW_shgroup_instance_create(e_data.shadow_sh, psl->shadow_cube_pass, geom);
	DRW_shgroup_uniform_block(grp, "shadow_render_block", sldata->shadow_render_ubo);
	DRW_shgroup_uniform_mat4(grp, "ShadowModelMatrix", (float *)obmat);

	for (int i = 0; i < 6; ++i)
		DRW_shgroup_call_dynamic_add_empty(grp);

	grp = DRW_shgroup_instance_create(e_data.shadow_sh, psl->shadow_cascade_pass, geom);
	DRW_shgroup_uniform_block(grp, "shadow_render_block", sldata->shadow_render_ubo);
	DRW_shgroup_uniform_mat4(grp, "ShadowModelMatrix", (float *)obmat);

	for (int i = 0; i < MAX_CASCADE_NUM; ++i)
		DRW_shgroup_call_dynamic_add_empty(grp);
}

void EEVEE_lights_cache_finish(EEVEE_SceneLayerData *sldata)
{
	EEVEE_LampsInfo *linfo = sldata->lamps;

	/* Setup enough layers. */
	/* Free textures if number mismatch. */
	if (linfo->num_cube != linfo->cache_num_cube) {
		DRW_TEXTURE_FREE_SAFE(sldata->shadow_depth_cube_pool);
		linfo->cache_num_cube = linfo->num_cube;
		linfo->update_flag |= LIGHT_UPDATE_SHADOW_CUBE;
	}
	if (linfo->num_map != linfo->cache_num_map) {
		DRW_TEXTURE_FREE_SAFE(sldata->shadow_depth_map_pool);
		linfo->cache_num_map = linfo->num_map;
	}
	if (linfo->num_cascade != linfo->cache_num_cascade) {
		DRW_TEXTURE_FREE_SAFE(sldata->shadow_depth_cascade_pool);
		linfo->cache_num_cascade = linfo->num_cascade;
	}

	/* Initialize Textures Arrays first so DRW_framebuffer_init just bind them. */
	if (!sldata->shadow_depth_cube_target) {
		/* Render Cubemap */
		sldata->shadow_depth_cube_target = DRW_texture_create_cube(512, DRW_TEX_DEPTH_24, 0, NULL);
		sldata->shadow_color_cube_target = DRW_texture_create_cube(512, DRW_TEX_R_32, DRW_TEX_FILTER, NULL);
		if (sldata->shadow_cube_fb) {
			DRW_framebuffer_texture_attach(sldata->shadow_cube_fb, sldata->shadow_depth_cube_target, 0, 0);
			DRW_framebuffer_texture_attach(sldata->shadow_cube_fb, sldata->shadow_color_cube_target, 0, 0);
		}
	}
	if (!sldata->shadow_depth_cube_pool) {
		/* Cubemap / octahedral map pool */
		/* TODO Cubemap array */
		sldata->shadow_depth_cube_pool = DRW_texture_create_2D_array(
		        512, 512, max_ff(1, linfo->num_cube), DRW_TEX_R_16,
		        DRW_TEX_FILTER | DRW_TEX_COMPARE, NULL);
		if (sldata->shadow_cube_fb) {
			DRW_framebuffer_texture_attach(sldata->shadow_cube_fb, sldata->shadow_depth_cube_pool, 0, 0);
		}
	}
	if (!sldata->shadow_depth_map_pool) {
		sldata->shadow_depth_map_pool = DRW_texture_create_2D_array(
		        512, 512, max_ff(1, linfo->num_map), DRW_TEX_DEPTH_24,
		        DRW_TEX_FILTER | DRW_TEX_COMPARE, NULL);
		if (sldata->shadow_map_fb) {
			DRW_framebuffer_texture_attach(sldata->shadow_map_fb, sldata->shadow_depth_map_pool, 0, 0);
		}
	}
	if (!sldata->shadow_depth_cascade_pool) {
		sldata->shadow_depth_cascade_pool = DRW_texture_create_2D_array(
		        512, 512, max_ff(1, linfo->num_cascade * MAX_CASCADE_NUM), DRW_TEX_DEPTH_24,
		        DRW_TEX_FILTER | DRW_TEX_COMPARE, NULL);
		if (sldata->shadow_cascade_fb) {
			DRW_framebuffer_texture_attach(sldata->shadow_cascade_fb, sldata->shadow_depth_map_pool, 0, 0);
		}
	}

	DRWFboTexture tex_cube_target[2] = {
	        {&sldata->shadow_depth_cube_target, DRW_TEX_DEPTH_24, 0},
	        {&sldata->shadow_color_cube_target, DRW_TEX_R_16, DRW_TEX_FILTER}};
	DRW_framebuffer_init(&sldata->shadow_cube_target_fb, &draw_engine_eevee_type, 512, 512, tex_cube_target, 2);

	DRWFboTexture tex_cube = {&sldata->shadow_depth_cube_pool, DRW_TEX_R_16, DRW_TEX_FILTER};
	DRW_framebuffer_init(&sldata->shadow_cube_fb, &draw_engine_eevee_type, 512, 512, &tex_cube, 1);

	DRWFboTexture tex_cascade = {&sldata->shadow_depth_cascade_pool, DRW_TEX_DEPTH_24, DRW_TEX_FILTER | DRW_TEX_COMPARE};
	DRW_framebuffer_init(&sldata->shadow_cascade_fb, &draw_engine_eevee_type, 512, 512, &tex_cascade, 1);

	/* Update Lamps UBOs. */
	EEVEE_lights_update(sldata);
}

/* Update buffer with lamp data */
static void eevee_light_setup(Object *ob, EEVEE_LampsInfo *linfo, EEVEE_LampEngineData *led)
{
	/* TODO only update if data changes */
	EEVEE_LightData *evld = led->storage;
	EEVEE_Light *evli = linfo->light_data + evld->light_id;
	Lamp *la = (Lamp *)ob->data;
	float mat[4][4], scale[3], power;

	/* Position */
	copy_v3_v3(evli->position, ob->obmat[3]);

	/* Color */
	copy_v3_v3(evli->color, &la->r);

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
		evli->radius = max_ff(0.001f, la->area_size);
	}
	else if (la->type == LA_AREA) {
		evli->sizex = max_ff(0.0001f, la->area_size * scale[0] * 0.5f);
		if (la->area_shape == LA_AREA_RECT) {
			evli->sizey = max_ff(0.0001f, la->area_sizey * scale[1] * 0.5f);
		}
		else {
			evli->sizey = max_ff(0.0001f, la->area_size * scale[1] * 0.5f);
		}
	}
	else {
		evli->radius = max_ff(0.001f, la->area_size);
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
	mul_v3_fl(evli->color, power * la->energy);

	/* Lamp Type */
	evli->lamptype = (float)la->type;

	/* No shadow by default */
	evli->shadowid = -1.0f;
}

static void eevee_shadow_cube_setup(Object *ob, EEVEE_LampsInfo *linfo, EEVEE_LampEngineData *led)
{
	float projmat[4][4];

	EEVEE_ShadowCubeData *evsmp = (EEVEE_ShadowCubeData *)led->storage;
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
	evsh->exp = la->bleedexp;

	evli->shadowid = (float)(evsmp->shadow_id);
}

static void eevee_shadow_map_setup(Object *ob, EEVEE_LampsInfo *linfo, EEVEE_LampEngineData *led)
{
	float viewmat[4][4], projmat[4][4];

	EEVEE_ShadowMapData *evsmp = (EEVEE_ShadowMapData *)led->storage;
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

#define LERP(t, a, b) ((a) + (t) * ((b) - (a)))

static void frustum_min_bounding_sphere(const float corners[8][4], float r_center[3], float *r_radius)
{
#if 0 /* Simple solution but waist too much space. */
	float minvec[3], maxvec[3];

	/* compute the bounding box */
	INIT_MINMAX(minvec, maxvec);
	for (int i = 0; i < 8; ++i)	{
		minmax_v3v3_v3(minvec, maxvec, corners[i]);
	}

	/* compute the bounding sphere of this box */
	r_radius = len_v3v3(minvec, maxvec) * 0.5f;
	add_v3_v3v3(r_center, minvec, maxvec);
	mul_v3_fl(r_center, 0.5f);
#else
	/* Make the bouding sphere always centered on the front diagonal */
	add_v3_v3v3(r_center, corners[4], corners[7]);
	mul_v3_fl(r_center, 0.5f);
	*r_radius = len_v3v3(corners[0], r_center);

	/* Search the largest distance between the sphere center
	 * and the front plane corners. */
	for (int i = 0; i < 4; ++i) {
		float rad = len_v3v3(corners[4+i], r_center);
		if (rad > *r_radius) {
			*r_radius = rad;
		}
	}
#endif
}

static void eevee_shadow_cascade_setup(Object *ob, EEVEE_LampsInfo *linfo, EEVEE_LampEngineData *led)
{
	/* Camera Matrices */
	float persmat[4][4], persinv[4][4];
	float viewprojmat[4][4], projinv[4][4];
	float near, far;
	float near_v[4] = {0.0f, 0.0f, -1.0f, 1.0f};
	float far_v[4] = {0.0f, 0.0f,  1.0f, 1.0f};
	bool is_persp = DRW_viewport_is_persp_get();
	DRW_viewport_matrix_get(persmat, DRW_MAT_PERS);
	invert_m4_m4(persinv, persmat);
	/* FIXME : Get near / far from Draw manager? */
	DRW_viewport_matrix_get(viewprojmat, DRW_MAT_WIN);
	invert_m4_m4(projinv, viewprojmat);
	mul_m4_v4(projinv, near_v);
	mul_m4_v4(projinv, far_v);
	near = near_v[2];
	far = far_v[2]; /* TODO: Should be a shadow parameter */
	if (is_persp) {
		near /= near_v[3];
		far /= far_v[3];
	}

	/* Lamps Matrices */
	float viewmat[4][4], projmat[4][4];
	int cascade_ct = MAX_CASCADE_NUM;
	float shadow_res = 512.0f; /* TODO parameter */

	EEVEE_ShadowCascadeData *evscp = (EEVEE_ShadowCascadeData *)led->storage;
	EEVEE_Light *evli = linfo->light_data + evscp->light_id;
	EEVEE_ShadowCascade *evsh = linfo->shadow_cascade_data + evscp->shadow_id;
	Lamp *la = (Lamp *)ob->data;

	/* The technique consists into splitting
	 * the view frustum into several sub-frustum
	 * that are individually receiving one shadow map */

	/* init near/far */
	for (int c = 0; c < MAX_CASCADE_NUM; ++c) {
		evsh->split[c] = far;
	}

	/* Compute split planes */
	float splits_ndc[MAX_CASCADE_NUM + 1];
	splits_ndc[0] = -1.0f;
	splits_ndc[cascade_ct] = 1.0f;
	for (int c = 1; c < cascade_ct; ++c) {
		const float lambda = 0.8f; /* TODO : Parameter */

		/* View Space */
		float linear_split = LERP(((float)(c) / (float)cascade_ct), near, far);
		float exp_split = near * powf(far / near, (float)(c) / (float)cascade_ct);

		if (is_persp) {
			evsh->split[c-1] = LERP(lambda, linear_split, exp_split);
		}
		else {
			evsh->split[c-1] = linear_split;
		}

		/* NDC Space */
		float p[4] = {1.0f, 1.0f, evsh->split[c-1], 1.0f};
		mul_m4_v4(viewprojmat, p);
		splits_ndc[c] = p[2];

		if (is_persp) {
			splits_ndc[c] /= p[3];
		}
	}

	/* For each cascade */
	for (int c = 0; c < cascade_ct; ++c) {
		/* Given 8 frustrum corners */
		float corners[8][4] = {
			/* Near Cap */
			{-1.0f, -1.0f, splits_ndc[c], 1.0f},
			{ 1.0f, -1.0f, splits_ndc[c], 1.0f},
			{-1.0f,  1.0f, splits_ndc[c], 1.0f},
			{ 1.0f,  1.0f, splits_ndc[c], 1.0f},
			/* Far Cap */
			{-1.0f, -1.0f, splits_ndc[c+1], 1.0f},
			{ 1.0f, -1.0f, splits_ndc[c+1], 1.0f},
			{-1.0f,  1.0f, splits_ndc[c+1], 1.0f},
			{ 1.0f,  1.0f, splits_ndc[c+1], 1.0f}
		};

		/* Transform them into world space */
		for (int i = 0; i < 8; ++i)	{
			mul_m4_v4(persinv, corners[i]);
			mul_v3_fl(corners[i], 1.0f / corners[i][3]);
			corners[i][3] = 1.0f;
		}

		/* Project them into light space */
		invert_m4_m4(viewmat, ob->obmat);
		normalize_v3(viewmat[0]);
		normalize_v3(viewmat[1]);
		normalize_v3(viewmat[2]);

		for (int i = 0; i < 8; ++i)	{
			mul_m4_v4(viewmat, corners[i]);
		}

		float center[3], radius;
		frustum_min_bounding_sphere(corners, center, &radius);

		/* Snap projection center to nearest texel to cancel shimering. */
		float shadow_origin[2], shadow_texco[2];
		mul_v2_v2fl(shadow_origin, center, shadow_res / (2.0f * radius)); /* Light to texture space. */

		/* Find the nearest texel. */
		shadow_texco[0] = round(shadow_origin[0]);
		shadow_texco[1] = round(shadow_origin[1]);

		/* Compute offset. */
		sub_v2_v2(shadow_texco, shadow_origin);
		mul_v2_fl(shadow_texco, (2.0f * radius) / shadow_res); /* Texture to light space. */

		/* Apply offset. */
		add_v2_v2(center, shadow_texco);

		/* Expand the projection to cover frustum range */
		orthographic_m4(projmat,
		                center[0] - radius,
		                center[0] + radius,
		                center[1] - radius,
		                center[1] + radius,
		                la->clipsta, la->clipend);

		mul_m4_m4m4(evscp->viewprojmat[c], projmat, viewmat);
		mul_m4_m4m4(evsh->shadowmat[c], texcomat, evscp->viewprojmat[c]);

		/* TODO modify bias depending on the cascade radius */
		evsh->bias[c] = 0.005f * la->bias;
	}

	evli->shadowid = (float)(MAX_SHADOW_CUBE + MAX_SHADOW_MAP + evscp->shadow_id);
}

/* Used for checking if object is inside the shadow volume. */
static bool cube_bbox_intersect(const float cube_center[3], float cube_half_dim, const BoundBox *bb, float (*obmat)[4])
{
	float min[3], max[4], tmp[4][4];
	unit_m4(tmp);
	translate_m4(tmp, -cube_center[0], -cube_center[1], -cube_center[2]);
	mul_m4_m4m4(tmp, tmp, obmat);

	/* Just simple AABB intersection test in world space. */
	INIT_MINMAX(min, max);
	for (int i = 0; i < 8; ++i) {
		float vec[3];
		copy_v3_v3(vec, bb->vec[i]);
		mul_m4_v3(tmp, vec);
		minmax_v3v3_v3(min, max, vec);
	}

    if (MAX3(max[0], max[1], max[2]) < -cube_half_dim) return false;
    if (MIN3(min[0], min[1], min[2]) >  cube_half_dim) return false;

	return true;
}

static ShadowCaster *search_object_in_list(ListBase *list, Object *ob)
{
	for (ShadowCaster *ldata = list->first; ldata; ldata = ldata->next) {
		if (ldata->ob == ob)
			return ldata;
	}

	return NULL;
}

static void delete_pruned_shadowcaster(EEVEE_LampEngineData *led)
{
	ShadowCaster *next;
	for (ShadowCaster *ldata = led->shadow_caster_list.first; ldata; ldata = next) {
		next = ldata->next;
		if (ldata->prune == true) {
			led->need_update = true;
			BLI_freelinkN(&led->shadow_caster_list, ldata);
		}
	}
}

static void light_tag_shadow_update(Object *lamp, Object *ob)
{
	Lamp *la = lamp->data;
	EEVEE_LampEngineData *led = EEVEE_lamp_data_get(lamp);

	bool is_inside_range = cube_bbox_intersect(lamp->obmat[3], la->clipend, BKE_object_boundbox_get(ob), ob->obmat);
	ShadowCaster *ldata = search_object_in_list(&led->shadow_caster_list, ob);

	if (is_inside_range) {
		if (ldata == NULL) {
			/* Object was not a shadow caster previously but is now. Add it. */
			ldata = MEM_callocN(sizeof(ShadowCaster), "ShadowCaster");
			ldata->ob = ob;
			BLI_addtail(&led->shadow_caster_list, ldata);
			led->need_update = true;
		}
		else {
			EEVEE_ObjectEngineData *oedata = EEVEE_object_data_get(ob);
			if (oedata->need_update) {
				led->need_update = true;
			}
		}
		ldata->prune = false;
	}
	else if (ldata != NULL) {
		/* Object was a shadow caster previously and is not anymore. Remove it. */
		led->need_update = true;
		BLI_freelinkN(&led->shadow_caster_list, ldata);
	}
}

static void eevee_lights_shcaster_updated(EEVEE_SceneLayerData *sldata, Object *ob)
{
	Object *lamp;
	EEVEE_LampsInfo *linfo = sldata->lamps;

	/* Iterate over all shadow casting lamps to see if
	 * each of them needs update because of this object */
	for (int i = 0; (lamp = linfo->shadow_cube_ref[i]) && (i < MAX_SHADOW_CUBE); i++) {
		light_tag_shadow_update(lamp, ob);
	}
}

void EEVEE_lights_update(EEVEE_SceneLayerData *sldata)
{
	EEVEE_LampsInfo *linfo = sldata->lamps;
	Object *ob;
	int i;

	/* Prune shadow casters to remove if object does not exists anymore (unprune them if object exists) */
	Object *lamp;
	for (i = 0; (lamp = linfo->shadow_cube_ref[i]) && (i < MAX_SHADOW_CUBE); i++) {
		EEVEE_LampEngineData *led = EEVEE_lamp_data_get(lamp);

		if ((linfo->update_flag & LIGHT_UPDATE_SHADOW_CUBE) != 0) {
			led->need_update = true;
		}

		for (ShadowCaster *ldata = led->shadow_caster_list.first; ldata; ldata = ldata->next) {
			ldata->prune = true;
		}
	}

	for (LinkData *ldata = sldata->shadow_casters.first; ldata; ldata = ldata->next) {
		eevee_lights_shcaster_updated(sldata, ldata->data);
	}

	for (i = 0; (ob = linfo->light_ref[i]) && (i < MAX_LIGHT); i++) {
		EEVEE_LampEngineData *led = EEVEE_lamp_data_get(ob);
		eevee_light_setup(ob, linfo, led);
	}

	for (i = 0; (ob = linfo->shadow_cube_ref[i]) && (i < MAX_SHADOW_CUBE); i++) {
		EEVEE_LampEngineData *led = EEVEE_lamp_data_get(ob);
		eevee_shadow_cube_setup(ob, linfo, led);
		delete_pruned_shadowcaster(led);
	}

	for (i = 0; (ob = linfo->shadow_map_ref[i]) && (i < MAX_SHADOW_MAP); i++) {
		EEVEE_LampEngineData *led = EEVEE_lamp_data_get(ob);
		eevee_shadow_map_setup(ob, linfo, led);
	}

	for (i = 0; (ob = linfo->shadow_cascade_ref[i]) && (i < MAX_SHADOW_CASCADE); i++) {
		EEVEE_LampEngineData *led = EEVEE_lamp_data_get(ob);
		eevee_shadow_cascade_setup(ob, linfo, led);
	}

	DRW_uniformbuffer_update(sldata->light_ubo, &linfo->light_data);
	DRW_uniformbuffer_update(sldata->shadow_ubo, &linfo->shadow_cube_data); /* Update all data at once */
}

/* this refresh lamps shadow buffers */
void EEVEE_draw_shadows(EEVEE_SceneLayerData *sldata, EEVEE_PassList *psl)
{
	EEVEE_LampsInfo *linfo = sldata->lamps;
	Object *ob;
	int i;
	float clear_color[4] = {FLT_MAX, FLT_MAX, FLT_MAX, 0.0f};

	/* Cube Shadow Maps */
	/* Render each shadow to one layer of the array */
	for (i = 0; (ob = linfo->shadow_cube_ref[i]) && (i < MAX_SHADOW_CUBE); i++) {
		EEVEE_LampEngineData *led = EEVEE_lamp_data_get(ob);
		Lamp *la = (Lamp *)ob->data;

		if (led->need_update) {
			EEVEE_ShadowCubeData *evscd = (EEVEE_ShadowCubeData *)led->storage;
			EEVEE_ShadowRender *srd = &linfo->shadow_render_data;

			srd->layer = i;
			srd->exponent = la->bleedexp;
			copy_v3_v3(srd->position, ob->obmat[3]);
			for (int j = 0; j < 6; ++j) {
				copy_m4_m4(srd->shadowmat[j], evscd->viewprojmat[j]);
			}
			DRW_uniformbuffer_update(sldata->shadow_render_ubo, &linfo->shadow_render_data);

			DRW_framebuffer_bind(sldata->shadow_cube_target_fb);
			DRW_framebuffer_clear(true, true, false, clear_color, 1.0);
			/* Render shadow cube */
			DRW_draw_pass(psl->shadow_cube_pass);

			/* Push it to shadowmap array */
			DRW_framebuffer_bind(sldata->shadow_cube_fb);
			DRW_draw_pass(psl->shadow_cube_store_pass);

			led->need_update = false;
		}
	}
	linfo->update_flag &= ~LIGHT_UPDATE_SHADOW_CUBE;

#if 0
	/* Standard Shadow Maps */
	DRW_framebuffer_bind(fbl->shadow_map_fb);
	DRW_framebuffer_clear(false, true, false, NULL, 1.0);

	/* Render each shadow to one layer of the array */
	for (i = 0; (ob = linfo->shadow_map_ref[i]) && (i < MAX_SHADOW_MAP); i++) {
		EEVEE_LampEngineData *led = EEVEE_lamp_data_get(ob);
		EEVEE_ShadowMapData *evsmd = (EEVEE_ShadowMapData *)led->storage;

		linfo->layer = i;
		copy_m4_m4(linfo->shadowmat, evsmd->viewprojmat);
		DRW_draw_pass(vedata->psl->shadow_pass);
	}
#endif

	/* Cascaded Shadow Maps */
// 	DRW_framebuffer_bind(fbl->shadow_cascade_fb);
// 	DRW_framebuffer_clear(false, true, false, NULL, 1.0);

// 	/* Render each shadow to one layer of the array */
// 	for (i = 0; (ob = linfo->shadow_cascade_ref[i]) && (i < MAX_SHADOW_CASCADE); i++) {
// 		EEVEE_LampEngineData *led = EEVEE_lamp_data_get(ob);
// 		EEVEE_ShadowCascadeData *evscd = (EEVEE_ShadowCascadeData *)led->storage;
// 		EEVEE_ShadowRender *srd = &linfo->shadow_render_data;

// 		srd->layer = i;
// 		for (int j = 0; j < MAX_CASCADE_NUM; ++j) {
// 			copy_m4_m4(srd->shadowmat[j], evscd->viewprojmat[j]);
// 		}
// 		DRW_uniformbuffer_update(sldata->shadow_render_ubo, &linfo->shadow_render_data);

// 		DRW_draw_pass(psl->shadow_cascade_pass);
// 	}
}

void EEVEE_lights_free(void)
{
	DRW_SHADER_FREE_SAFE(e_data.shadow_sh);
	DRW_SHADER_FREE_SAFE(e_data.shadow_store_sh);
}