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

/** \file blender/draw/modes/object_mode.c
 *  \ingroup draw
 */

#include "DRW_engine.h"
#include "DRW_render.h"

#include "draw_mode_pass.h"

#include "object_mode.h"

/* keep it under MAX_PASSES */
typedef struct OBJECT_PassList {
	struct DRWPass *non_meshes_pass;
	struct DRWPass *ob_center_pass;
	struct DRWPass *wire_outline_pass;
	struct DRWPass *bone_solid_pass;
	struct DRWPass *bone_wire_pass;
} OBJECT_PassList;

void OBJECT_cache_init(void)
{
	OBJECT_PassList *psl = DRW_mode_pass_list_get();

	DRW_mode_passes_setup(NULL,
	                      NULL,
	                      &psl->wire_outline_pass,
	                      &psl->non_meshes_pass,
	                      &psl->ob_center_pass,
	                      &psl->bone_solid_pass,
	                      &psl->bone_wire_pass);
}

void OBJECT_cache_populate(Object *ob)
{
	CollectionEngineSettings *ces_mode_ob = BKE_object_collection_engine_get(ob, COLLECTION_MODE_OBJECT, "");

	bool do_wire = BKE_collection_engine_property_value_get_bool(ces_mode_ob, "show_wire");
	bool do_outlines = ((ob->base_flag & BASE_SELECTED) != 0) || do_wire;

	switch (ob->type) {
		case OB_MESH:
			DRW_shgroup_wire_outline(ob, do_wire, false, do_outlines);
			break;
		case OB_LAMP:
			DRW_shgroup_lamp(ob);
			break;
		case OB_CAMERA:
			DRW_shgroup_camera(ob);
			break;
		case OB_EMPTY:
			DRW_shgroup_empty(ob);
			break;
		case OB_SPEAKER:
			DRW_shgroup_speaker(ob);
			break;
		case OB_ARMATURE:
			DRW_shgroup_armature_object(ob);
			break;
		default:
			break;
	}

	DRW_shgroup_object_center(ob);
	DRW_shgroup_relationship_lines(ob);
}

void OBJECT_cache_finish(void)
{
	/* Do nothing */
}

void OBJECT_draw(void)
{
	OBJECT_PassList *psl = DRW_mode_pass_list_get();

	DRW_draw_pass(psl->bone_wire_pass);
	DRW_draw_pass(psl->bone_solid_pass);
	DRW_draw_pass(psl->wire_outline_pass);
	DRW_draw_pass(psl->non_meshes_pass);
	DRW_draw_pass(psl->ob_center_pass);
}

void OBJECT_collection_settings_create(CollectionEngineSettings *ces)
{
	BLI_assert(ces);
	BKE_collection_engine_property_add_int(ces, "show_wire", false);
	BKE_collection_engine_property_add_int(ces, "show_backface_culling", false);
}
