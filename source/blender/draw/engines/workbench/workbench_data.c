#include "workbench_private.h"

#include "UI_resources.h"

void workbench_private_data_init(WORKBENCH_PrivateData *wpd)
{
	const DRWContextState *draw_ctx = DRW_context_state_get();
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
	UI_GetThemeColor3fv(UI_GetThemeValue(TH_SHOW_BACK_GRAD) ? TH_LOW_GRAD : TH_HIGH_GRAD, wd->background_color_low);
	UI_GetThemeColor3fv(TH_HIGH_GRAD, wd->background_color_high);

	/* XXX: Really quick conversion to avoid washed out background.
	 * Needs to be adressed properly (color managed using ocio). */
	srgb_to_linearrgb_v3_v3(wd->background_color_high, wd->background_color_high);
	srgb_to_linearrgb_v3_v3(wd->background_color_low, wd->background_color_low);

	studiolight_update_world(wpd->studio_light, wd);

	copy_v3_v3(wd->object_outline_color, wpd->shading.object_outline_color);
	wd->object_outline_color[3] = 1.0f;

	wpd->world_ubo = DRW_uniformbuffer_create(sizeof(WORKBENCH_UBO_World), &wpd->world_data);
}

void workbench_private_data_free(WORKBENCH_PrivateData *wpd)
{
	BLI_ghash_free(wpd->material_hash, NULL, MEM_freeN);
	DRW_UBO_FREE_SAFE(wpd->world_ubo);
}
