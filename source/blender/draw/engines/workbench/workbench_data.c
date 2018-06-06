#include "workbench_private.h"

#include "DNA_userdef_types.h"

#include "UI_resources.h"


void workbench_private_data_init(WORKBENCH_PrivateData *wpd)
{
	const DRWContextState *draw_ctx = DRW_context_state_get();
	const Scene *scene = draw_ctx->scene;
	wpd->material_hash = BLI_ghash_ptr_new(__func__);

	View3D *v3d = draw_ctx->v3d;
	if (v3d) {
		wpd->shading = v3d->shading;
		wpd->drawtype = v3d->drawtype;
		if (wpd->shading.light == V3D_LIGHTING_MATCAP) {
			wpd->studio_light = BKE_studiolight_find(
			        wpd->shading.matcap, STUDIOLIGHT_ORIENTATION_VIEWNORMAL);
		}
		else {
			wpd->studio_light = BKE_studiolight_find(
			        wpd->shading.studio_light, STUDIOLIGHT_ORIENTATION_CAMERA | STUDIOLIGHT_ORIENTATION_WORLD);
		}
	}
	else {
		memset(&wpd->shading, 0, sizeof(wpd->shading));
		wpd->shading.light = V3D_LIGHTING_STUDIO;
		wpd->shading.shadow_intensity = 0.5;
		copy_v3_fl(wpd->shading.single_color, 0.8f);
		wpd->drawtype = OB_SOLID;
		wpd->studio_light = BKE_studiolight_find_first(STUDIOLIGHT_INTERNAL);
	}
	wpd->shadow_multiplier = 1.0 - wpd->shading.shadow_intensity;

	WORKBENCH_UBO_World *wd = &wpd->world_data;

	if ((v3d->flag3 & V3D_SHOW_WORLD) &&
	    (scene->world != NULL))
	{
		copy_v3_v3(wd->background_color_low, &scene->world->horr);
		copy_v3_v3(wd->background_color_high, &scene->world->horr);
	}
	else {
		UI_GetThemeColor3fv(UI_GetThemeValue(TH_SHOW_BACK_GRAD) ? TH_LOW_GRAD : TH_HIGH_GRAD, wd->background_color_low);
		UI_GetThemeColor3fv(TH_HIGH_GRAD, wd->background_color_high);

		/* XXX: Really quick conversion to avoid washed out background.
		 * Needs to be adressed properly (color managed using ocio). */
		srgb_to_linearrgb_v3_v3(wd->background_color_high, wd->background_color_high);
		srgb_to_linearrgb_v3_v3(wd->background_color_low, wd->background_color_low);
	}

	studiolight_update_world(wpd->studio_light, wd);

	copy_v3_v3(wd->object_outline_color, wpd->shading.object_outline_color);
	wd->object_outline_color[3] = 1.0f;

	wpd->world_ubo = DRW_uniformbuffer_create(sizeof(WORKBENCH_UBO_World), &wpd->world_data);

	/* Cavity settings */
	{
		const int ssao_samples = scene->display.matcap_ssao_samples;

		float invproj[4][4];
		float dfdyfacs[2];
		const bool is_persp = DRW_viewport_is_persp_get();
		/* view vectors for the corners of the view frustum.
		 * Can be used to recreate the world space position easily */
		float viewvecs[3][4] = {
		    {-1.0f, -1.0f, -1.0f, 1.0f},
		    {1.0f, -1.0f, -1.0f, 1.0f},
		    {-1.0f, 1.0f, -1.0f, 1.0f}
		};
		int i;
		const float *size = DRW_viewport_size_get();

		DRW_state_dfdy_factors_get(dfdyfacs);

		wpd->ssao_params[0] = ssao_samples;
		wpd->ssao_params[1] = size[0] / 64.0;
		wpd->ssao_params[2] = size[1] / 64.0;
		wpd->ssao_params[3] = dfdyfacs[1]; /* dfdy sign for offscreen */

		/* distance, factor, factor, attenuation */
		copy_v4_fl4(wpd->ssao_settings, scene->display.matcap_ssao_distance, wpd->shading.cavity_valley_factor, wpd->shading.cavity_ridge_factor, scene->display.matcap_ssao_attenuation);

		/* invert the view matrix */
		DRW_viewport_matrix_get(wpd->winmat, DRW_MAT_WIN);
		invert_m4_m4(invproj, wpd->winmat);

		/* convert the view vectors to view space */
		for (i = 0; i < 3; i++) {
			mul_m4_v4(invproj, viewvecs[i]);
			/* normalized trick see:
			 * http://www.derschmale.com/2014/01/26/reconstructing-positions-from-the-depth-buffer */
			mul_v3_fl(viewvecs[i], 1.0f / viewvecs[i][3]);
			if (is_persp)
				mul_v3_fl(viewvecs[i], 1.0f / viewvecs[i][2]);
			viewvecs[i][3] = 1.0;

			copy_v4_v4(wpd->viewvecs[i], viewvecs[i]);
		}

		/* we need to store the differences */
		wpd->viewvecs[1][0] -= wpd->viewvecs[0][0];
		wpd->viewvecs[1][1] = wpd->viewvecs[2][1] - wpd->viewvecs[0][1];

		/* calculate a depth offset as well */
		if (!is_persp) {
			float vec_far[] = {-1.0f, -1.0f, 1.0f, 1.0f};
			mul_m4_v4(invproj, vec_far);
			mul_v3_fl(vec_far, 1.0f / vec_far[3]);
			wpd->viewvecs[1][2] = vec_far[2] - wpd->viewvecs[0][2];
		}
	}

}

void workbench_private_data_get_light_direction(WORKBENCH_PrivateData *wpd, float light_direction[3])
{
	const DRWContextState *draw_ctx = DRW_context_state_get();
	Scene *scene = draw_ctx->scene;
	WORKBENCH_UBO_World *wd = &wpd->world_data;
	float view_matrix[4][4];
	DRW_viewport_matrix_get(view_matrix, DRW_MAT_VIEW);

	{
		WORKBENCH_UBO_Light *light = &wd->lights[0];
		mul_v3_mat3_m4v3(light->light_direction_vs, view_matrix, light_direction);
		light->light_direction_vs[3] = 0.0f;
		copy_v3_fl(light->specular_color, 1.0f);
		light->energy = 1.0f;
		copy_v4_v4(wd->light_direction_vs, light->light_direction_vs);
		wd->num_lights = 1;
	}

	if (!STUDIOLIGHT_ORIENTATION_WORLD_ENABLED(wpd)) {
		int light_index = 0;
		for (int index = 0 ; index < 3; index++) {
			SolidLight *sl = &U.light[index];
			if (sl->flag) {
				WORKBENCH_UBO_Light *light = &wd->lights[light_index++];
				copy_v4_v4(light->light_direction_vs, sl->vec);
				negate_v3(light->light_direction_vs);
				copy_v4_v4(light->specular_color, sl->spec);
				light->energy = 1.0f;
			}
		}
		wd->num_lights = light_index;
	}

#if 0
	if (STUDIOLIGHT_ORIENTATION_WORLD_ENABLED(wpd)) {
		BKE_studiolight_ensure_flag(wpd->studio_light, STUDIOLIGHT_LIGHT_DIRECTION_CALCULATED);
		float rot_matrix[3][3];
		axis_angle_to_mat3_single(rot_matrix, 'Z', wpd->shading.studiolight_rot_z);
		mul_v3_m3v3(e_data.display.light_direction, rot_matrix, wpd->studio_light->light_direction);
	}
	else {
#else
	{
#endif
		copy_v3_v3(light_direction, scene->display.light_direction);
		negate_v3(light_direction);
	}

	DRW_uniformbuffer_update(wpd->world_ubo, &wpd->world_data);
}

static void workbench_private_material_free(void *data)
{
	WORKBENCH_MaterialData *material_data = data;
	DRW_UBO_FREE_SAFE(material_data->material_ubo);
	MEM_freeN(material_data);
}

void workbench_private_data_free(WORKBENCH_PrivateData *wpd)
{
	BLI_ghash_free(wpd->material_hash, NULL, workbench_private_material_free);
	DRW_UBO_FREE_SAFE(wpd->world_ubo);
}
