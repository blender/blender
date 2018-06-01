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
		wpd->studio_light = BKE_studiolight_find(wpd->shading.studio_light, 0);
	}
	else {
		memset(&wpd->shading, 0, sizeof(wpd->shading));
		wpd->shading.light = V3D_LIGHTING_STUDIO;
		wpd->shading.shadow_intensity = 0.5;
		copy_v3_fl(wpd->shading.single_color, 0.8f);
		wpd->drawtype = OB_SOLID;
		wpd->studio_light = BKE_studiolight_findindex(0);
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
	}

	/* XXX: Really quick conversion to avoid washed out background.
	 * Needs to be adressed properly (color managed using ocio). */
	srgb_to_linearrgb_v3_v3(wd->background_color_high, wd->background_color_high);
	srgb_to_linearrgb_v3_v3(wd->background_color_low, wd->background_color_low);

	studiolight_update_world(wpd->studio_light, wd);

	copy_v3_v3(wd->object_outline_color, wpd->shading.object_outline_color);
	wd->object_outline_color[3] = 1.0f;

	wpd->world_ubo = DRW_uniformbuffer_create(sizeof(WORKBENCH_UBO_World), &wpd->world_data);
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

	if (STUDIOLIGHT_ORIENTATION_CAMERA_ENABLED(wpd)) {
		int light_index = 0;
		for (int index = 0 ; index < 3; index++)
		{
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

static void workbench_private_material_free(void* data)
{
	WORKBENCH_MaterialData *material_data = (WORKBENCH_MaterialData*)data;
	DRW_UBO_FREE_SAFE(material_data->material_ubo);
	MEM_freeN(material_data);
}

void workbench_private_data_free(WORKBENCH_PrivateData *wpd)
{
	
	BLI_ghash_free(wpd->material_hash, NULL, workbench_private_material_free);
	DRW_UBO_FREE_SAFE(wpd->world_ubo);
}
