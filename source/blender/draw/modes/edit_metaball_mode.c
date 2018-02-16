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

/** \file blender/draw/modes/edit_metaball_mode.c
 *  \ingroup draw
 */

#include "DRW_engine.h"
#include "DRW_render.h"

#include "DNA_meta_types.h"

#include "BKE_mball.h"

/* If builtin shaders are needed */
#include "GPU_shader.h"
#include "GPU_select.h"

#include "draw_common.h"

#include "draw_mode_engines.h"

/* If needed, contains all global/Theme colors
 * Add needed theme colors / values to DRW_globals_update() and update UBO
 * Not needed for constant color. */
extern struct GPUUniformBuffer *globals_ubo; /* draw_common.c */
extern struct GlobalsUboStorage ts; /* draw_common.c */

/* *********** LISTS *********** */
/* All lists are per viewport specific datas.
 * They are all free when viewport changes engines
 * or is free itself. Use EDIT_METABALL_engine_init() to
 * initialize most of them and EDIT_METABALL_cache_init()
 * for EDIT_METABALL_PassList */

typedef struct EDIT_METABALL_PassList {
	/* Declare all passes here and init them in
	 * EDIT_METABALL_cache_init().
	 * Only contains (DRWPass *) */
	struct DRWPass *pass;
} EDIT_METABALL_PassList;

typedef struct EDIT_METABALL_FramebufferList {
	/* Contains all framebuffer objects needed by this engine.
	 * Only contains (GPUFrameBuffer *) */
	struct GPUFrameBuffer *fb;
} EDIT_METABALL_FramebufferList;

typedef struct EDIT_METABALL_TextureList {
	/* Contains all framebuffer textures / utility textures
	 * needed by this engine. Only viewport specific textures
	 * (not per object). Only contains (GPUTexture *) */
	struct GPUTexture *texture;
} EDIT_METABALL_TextureList;

typedef struct EDIT_METABALL_StorageList {
	/* Contains any other memory block that the engine needs.
	 * Only directly MEM_(m/c)allocN'ed blocks because they are
	 * free with MEM_freeN() when viewport is freed.
	 * (not per object) */
	// struct CustomStruct *block;
	struct EDIT_METABALL_PrivateData *g_data;
} EDIT_METABALL_StorageList;

typedef struct EDIT_METABALL_Data {
	/* Struct returned by DRW_viewport_engine_data_ensure.
	 * If you don't use one of these, just make it a (void *) */
	// void *fbl;
	void *engine_type; /* Required */
	EDIT_METABALL_FramebufferList *fbl;
	EDIT_METABALL_TextureList *txl;
	EDIT_METABALL_PassList *psl;
	EDIT_METABALL_StorageList *stl;
} EDIT_METABALL_Data;

/* *********** STATIC *********** */

typedef struct EDIT_METABALL_PrivateData {
	/* This keeps the references of the shading groups for
	 * easy access in EDIT_METABALL_cache_populate() */
	DRWShadingGroup *group;
} EDIT_METABALL_PrivateData; /* Transient data */

/* *********** FUNCTIONS *********** */

/* Here init all passes and shading groups
 * Assume that all Passes are NULL */
static void EDIT_METABALL_cache_init(void *vedata)
{
	EDIT_METABALL_PassList *psl = ((EDIT_METABALL_Data *)vedata)->psl;
	EDIT_METABALL_StorageList *stl = ((EDIT_METABALL_Data *)vedata)->stl;

	if (!stl->g_data) {
		/* Alloc transient pointers */
		stl->g_data = MEM_mallocN(sizeof(*stl->g_data), __func__);
	}

	{
		/* Create a pass */
		DRWState state = DRW_STATE_WRITE_COLOR | DRW_STATE_WRITE_DEPTH | DRW_STATE_DEPTH_LESS | DRW_STATE_BLEND | DRW_STATE_WIRE;
		psl->pass = DRW_pass_create("My Pass", state);

		/* Create a shadingGroup using a function in draw_common.c or custom one */
		stl->g_data->group = shgroup_instance_mball_handles(psl->pass, DRW_cache_screenspace_circle_get());
	}
}

static void EDIT_METABALL_cache_populate_radius(
        DRWShadingGroup *group, MetaElem *ml, const float scale_xform[3][4],
        const float *radius, const int selection_id)
{
	const float *color;
	static const float col_radius[3] =        {0.63, 0.19, 0.19}; /* 0x3030A0 */
	static const float col_radius_select[3] = {0.94, 0.63, 0.63}; /* 0xA0A0F0 */

	if ((ml->flag & SELECT) && (ml->flag & MB_SCALE_RAD)) color = col_radius_select;
	else color = col_radius;

	if (selection_id != -1) {
		ml->selcol1 = selection_id;
		DRW_select_load_id(selection_id);
	}

	DRW_shgroup_call_dynamic_add(group, scale_xform, radius, color);
}

static void EDIT_METABALL_cache_populate_stiffness(
        DRWShadingGroup *group, MetaElem *ml, const float scale_xform[3][4],
        const float *radius, const int selection_id)
{
	const float *color;
	static const float col_stiffness[3] =        {0.19, 0.63, 0.19}; /* 0x30A030 */
	static const float col_stiffness_select[3] = {0.63, 0.94, 0.63}; /* 0xA0F0A0 */

	if ((ml->flag & SELECT) && !(ml->flag & MB_SCALE_RAD)) color = col_stiffness_select;
	else color = col_stiffness;

	if (selection_id != -1) {
		ml->selcol2 = selection_id;
		DRW_select_load_id(selection_id);
	}

	DRW_shgroup_call_dynamic_add(group, scale_xform, radius, color);
}

/* Add geometry to shadingGroups. Execute for each objects */
static void EDIT_METABALL_cache_populate(void *vedata, Object *ob)
{
	//EDIT_METABALL_PassList *psl = ((EDIT_METABALL_Data *)vedata)->psl;
	EDIT_METABALL_StorageList *stl = ((EDIT_METABALL_Data *)vedata)->stl;

	if (ob->type == OB_MBALL) {
		const DRWContextState *draw_ctx = DRW_context_state_get();
		DRWShadingGroup *group = stl->g_data->group;

		if (ob == draw_ctx->object_edit) {
			MetaBall *mb = ob->data;

			const bool is_select = DRW_state_is_select();

			int selection_id = 0;

			for (MetaElem *ml = mb->editelems->first; ml != NULL; ml = ml->next) {
				BKE_mball_element_calc_scale_xform(ml->draw_scale_xform, ob->obmat, &ml->x);
				ml->draw_stiffness_radius = ml->rad * atanf(ml->s) / (float)M_PI_2;

				EDIT_METABALL_cache_populate_radius(
				        group, ml, ml->draw_scale_xform, &ml->rad, is_select ? ++selection_id : -1);

				EDIT_METABALL_cache_populate_stiffness(
				        group, ml, ml->draw_scale_xform, &ml->draw_stiffness_radius, is_select ? ++selection_id : -1);
			}
		}
	}
}

/* Draw time ! Control rendering pipeline from here */
static void EDIT_METABALL_draw_scene(void *vedata)
{
	EDIT_METABALL_PassList *psl = ((EDIT_METABALL_Data *)vedata)->psl;
	/* render passes on default framebuffer. */
	DRW_draw_pass(psl->pass);

	/* If you changed framebuffer, double check you rebind
	 * the default one with its textures attached before finishing */
}

/* Cleanup when destroying the engine.
 * This is not per viewport ! only when quitting blender.
 * Mostly used for freeing shaders */
static void EDIT_METABALL_engine_free(void)
{
	// DRW_SHADER_FREE_SAFE(custom_shader);
}

/* Create collection settings here.
 *
 * Be sure to add this function there :
 * source/blender/draw/DRW_engine.h
 * source/blender/blenkernel/intern/layer.c
 * source/blenderplayer/bad_level_call_stubs/stubs.c
 *
 * And relevant collection settings to :
 * source/blender/makesrna/intern/rna_scene.c
 * source/blender/blenkernel/intern/layer.c
 */
#if 0
void EDIT_METABALL_collection_settings_create(CollectionEngineSettings *ces)
{
	BLI_assert(ces);
	// BKE_collection_engine_property_add_int(ces, "my_bool_prop", false);
	// BKE_collection_engine_property_add_int(ces, "my_int_prop", 0);
	// BKE_collection_engine_property_add_float(ces, "my_float_prop", 0.0f);
}
#endif

static const DrawEngineDataSize EDIT_METABALL_data_size = DRW_VIEWPORT_DATA_SIZE(EDIT_METABALL_Data);

DrawEngineType draw_engine_edit_metaball_type = {
	NULL, NULL,
	N_("EditMetaballMode"),
	&EDIT_METABALL_data_size,
	NULL,
	&EDIT_METABALL_engine_free,
	&EDIT_METABALL_cache_init,
	&EDIT_METABALL_cache_populate,
	NULL,
	NULL, /* draw_background but not needed by mode engines */
	&EDIT_METABALL_draw_scene,
	NULL,
	NULL,
};
