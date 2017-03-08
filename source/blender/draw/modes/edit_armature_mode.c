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

/** \file blender/draw/modes/edit_armature_mode.c
 *  \ingroup draw
 */

#include "DRW_engine.h"
#include "DRW_render.h"

#include "DNA_armature_types.h"

#include "draw_common.h"

#include "draw_mode_engines.h"

/* keep it under MAX_PASSES */
typedef struct EDIT_ARMATURE_PassList {
	struct DRWPass *bone_solid;
	struct DRWPass *bone_wire;
	struct DRWPass *relationship;
} EDIT_ARMATURE_PassList;

typedef struct EDIT_ARMATURE_Data {
	char engine_name[32];
	void *fbl;
	void *txl;
	EDIT_ARMATURE_PassList *psl;
	void *stl;
} EDIT_ARMATURE_Data;

static DRWShadingGroup *relationship_lines;

extern GlobalsUboStorage ts;

static EDIT_ARMATURE_Data *vedata;

static void EDIT_ARMATURE_cache_init(void)
{
	vedata = DRW_viewport_engine_data_get("EditArmatureMode");
	EDIT_ARMATURE_PassList *psl = vedata->psl;

	{
		/* Solid bones */
		DRWState state = DRW_STATE_WRITE_COLOR | DRW_STATE_WRITE_DEPTH | DRW_STATE_DEPTH_LESS;
		psl->bone_solid = DRW_pass_create("Bone Solid Pass", state);
	}

	{
		/* Wire bones */
		DRWState state = DRW_STATE_WRITE_COLOR | DRW_STATE_WRITE_DEPTH | DRW_STATE_DEPTH_LESS | DRW_STATE_BLEND;
		psl->bone_wire = DRW_pass_create("Bone Wire Pass", state);
	}

	{
		/* Non Meshes Pass (Camera, empties, lamps ...) */
		DRWState state = DRW_STATE_WRITE_COLOR | DRW_STATE_WRITE_DEPTH | DRW_STATE_DEPTH_LESS | DRW_STATE_BLEND | DRW_STATE_WIRE;
		psl->relationship = DRW_pass_create("Bone Relationship Pass", state);

		/* Relationship Lines */
		relationship_lines = shgroup_dynlines_uniform_color(psl->relationship, ts.colorWire);
		DRW_shgroup_state_set(relationship_lines, DRW_STATE_STIPPLE_3);
	}

}

static void EDIT_ARMATURE_cache_populate(Object *ob)
{
	bArmature *arm = ob->data;
	EDIT_ARMATURE_PassList *psl = vedata->psl;

	if (ob->type == OB_ARMATURE) {
		if (arm->edbo) {
			DRW_shgroup_armature_edit(ob, psl->bone_solid, psl->bone_wire, relationship_lines);
		}
	}
}

static void EDIT_ARMATURE_draw_scene(void)
{
	EDIT_ARMATURE_Data *ved = DRW_viewport_engine_data_get("EditArmatureMode");
	EDIT_ARMATURE_PassList *psl = ved->psl;

	DRW_draw_pass(psl->bone_solid);
	DRW_draw_pass(psl->bone_wire);
	DRW_draw_pass(psl->relationship);
}

void EDIT_ARMATURE_collection_settings_create(CollectionEngineSettings *ces)
{
	BLI_assert(ces);
	//BKE_collection_engine_property_add_int(ces, "show_occlude_wire", false);
}

DrawEngineType draw_engine_edit_armature_type = {
	NULL, NULL,
	N_("EditArmatureMode"),
	NULL,
	NULL,
	&EDIT_ARMATURE_cache_init,
	&EDIT_ARMATURE_cache_populate,
	NULL,
	NULL,
	&EDIT_ARMATURE_draw_scene
};
