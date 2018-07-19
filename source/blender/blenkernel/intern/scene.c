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
 * The Original Code is: all of this file.
 *
 * Contributor(s): none yet.
 *
 * ***** END GPL LICENSE BLOCK *****
 */

/** \file blender/blenkernel/intern/scene.c
 *  \ingroup bke
 */


#include <stddef.h>
#include <stdio.h>
#include <string.h>

#include "MEM_guardedalloc.h"

#include "DNA_anim_types.h"
#include "DNA_group_types.h"
#include "DNA_linestyle_types.h"
#include "DNA_mesh_types.h"
#include "DNA_node_types.h"
#include "DNA_object_types.h"
#include "DNA_rigidbody_types.h"
#include "DNA_scene_types.h"
#include "DNA_screen_types.h"
#include "DNA_sequence_types.h"
#include "DNA_space_types.h"
#include "DNA_view3d_types.h"
#include "DNA_windowmanager_types.h"
#include "DNA_workspace_types.h"
#include "DNA_gpencil_types.h"

#include "BLI_math.h"
#include "BLI_blenlib.h"
#include "BLI_utildefines.h"
#include "BLI_callbacks.h"
#include "BLI_string.h"
#include "BLI_string_utils.h"
#include "BLI_threads.h"
#include "BLI_task.h"

#include "BLT_translation.h"

#include "BKE_anim.h"
#include "BKE_animsys.h"
#include "BKE_action.h"
#include "BKE_armature.h"
#include "BKE_cachefile.h"
#include "BKE_collection.h"
#include "BKE_colortools.h"
#include "BKE_editmesh.h"
#include "BKE_fcurve.h"
#include "BKE_freestyle.h"
#include "BKE_global.h"
#include "BKE_gpencil.h"
#include "BKE_icons.h"
#include "BKE_idprop.h"
#include "BKE_image.h"
#include "BKE_layer.h"
#include "BKE_library.h"
#include "BKE_library_remap.h"
#include "BKE_linestyle.h"
#include "BKE_main.h"
#include "BKE_mask.h"
#include "BKE_node.h"
#include "BKE_object.h"
#include "BKE_paint.h"
#include "BKE_rigidbody.h"
#include "BKE_scene.h"
#include "BKE_screen.h"
#include "BKE_sequencer.h"
#include "BKE_sound.h"
#include "BKE_unit.h"
#include "BKE_workspace.h"
#include "BKE_world.h"

#include "DEG_depsgraph.h"
#include "DEG_depsgraph_build.h"
#include "DEG_depsgraph_debug.h"
#include "DEG_depsgraph_query.h"

#include "RE_engine.h"

#include "engines/eevee/eevee_lightcache.h"

#include "PIL_time.h"

#include "IMB_colormanagement.h"
#include "IMB_imbuf.h"

#include "bmesh.h"

const char *RE_engine_id_BLENDER_EEVEE = "BLENDER_EEVEE";
const char *RE_engine_id_BLENDER_OPENGL = "BLENDER_OPENGL";
const char *RE_engine_id_CYCLES = "CYCLES";

void free_avicodecdata(AviCodecData *acd)
{
	if (acd) {
		if (acd->lpFormat) {
			MEM_freeN(acd->lpFormat);
			acd->lpFormat = NULL;
			acd->cbFormat = 0;
		}
		if (acd->lpParms) {
			MEM_freeN(acd->lpParms);
			acd->lpParms = NULL;
			acd->cbParms = 0;
		}
	}
}

static void remove_sequencer_fcurves(Scene *sce)
{
	AnimData *adt = BKE_animdata_from_id(&sce->id);

	if (adt && adt->action) {
		FCurve *fcu, *nextfcu;

		for (fcu = adt->action->curves.first; fcu; fcu = nextfcu) {
			nextfcu = fcu->next;

			if ((fcu->rna_path) && strstr(fcu->rna_path, "sequences_all")) {
				action_groups_remove_channel(adt->action, fcu);
				free_fcurve(fcu);
			}
		}
	}
}

/* flag -- copying options (see BKE_library.h's LIB_ID_COPY_... flags for more). */
ToolSettings *BKE_toolsettings_copy(ToolSettings *toolsettings, const int flag)
{
	if (toolsettings == NULL) {
		return NULL;
	}
	ToolSettings *ts = MEM_dupallocN(toolsettings);
	if (ts->vpaint) {
		ts->vpaint = MEM_dupallocN(ts->vpaint);
		BKE_paint_copy(&ts->vpaint->paint, &ts->vpaint->paint, flag);
	}
	if (ts->wpaint) {
		ts->wpaint = MEM_dupallocN(ts->wpaint);
		BKE_paint_copy(&ts->wpaint->paint, &ts->wpaint->paint, flag);
	}
	if (ts->sculpt) {
		ts->sculpt = MEM_dupallocN(ts->sculpt);
		BKE_paint_copy(&ts->sculpt->paint, &ts->sculpt->paint, flag);
	}
	if (ts->uvsculpt) {
		ts->uvsculpt = MEM_dupallocN(ts->uvsculpt);
		BKE_paint_copy(&ts->uvsculpt->paint, &ts->uvsculpt->paint, flag);
	}

	BKE_paint_copy(&ts->imapaint.paint, &ts->imapaint.paint, flag);
	ts->imapaint.paintcursor = NULL;
	ts->particle.paintcursor = NULL;
	ts->particle.scene = NULL;
	ts->particle.object = NULL;

	/* duplicate Grease Pencil Drawing Brushes */
	BLI_listbase_clear(&ts->gp_brushes);
	for (bGPDbrush *brush = toolsettings->gp_brushes.first; brush; brush = brush->next) {
		bGPDbrush *newbrush = BKE_gpencil_brush_duplicate(brush);
		BLI_addtail(&ts->gp_brushes, newbrush);
	}

	/* duplicate Grease Pencil interpolation curve */
	ts->gp_interpolate.custom_ipo = curvemapping_copy(ts->gp_interpolate.custom_ipo);
	return ts;
}

void BKE_toolsettings_free(ToolSettings *toolsettings)
{
	if (toolsettings == NULL) {
		return;
	}
	if (toolsettings->vpaint) {
		BKE_paint_free(&toolsettings->vpaint->paint);
		MEM_freeN(toolsettings->vpaint);
	}
	if (toolsettings->wpaint) {
		BKE_paint_free(&toolsettings->wpaint->paint);
		MEM_freeN(toolsettings->wpaint);
	}
	if (toolsettings->sculpt) {
		BKE_paint_free(&toolsettings->sculpt->paint);
		MEM_freeN(toolsettings->sculpt);
	}
	if (toolsettings->uvsculpt) {
		BKE_paint_free(&toolsettings->uvsculpt->paint);
		MEM_freeN(toolsettings->uvsculpt);
	}
	BKE_paint_free(&toolsettings->imapaint.paint);

	/* free Grease Pencil Drawing Brushes */
	BKE_gpencil_free_brushes(&toolsettings->gp_brushes);
	BLI_freelistN(&toolsettings->gp_brushes);

	/* free Grease Pencil interpolation curve */
	if (toolsettings->gp_interpolate.custom_ipo) {
		curvemapping_free(toolsettings->gp_interpolate.custom_ipo);
	}

	MEM_freeN(toolsettings);
}

/**
 * Only copy internal data of Scene ID from source to already allocated/initialized destination.
 * You probably nerver want to use that directly, use id_copy or BKE_id_copy_ex for typical needs.
 *
 * WARNING! This function will not handle ID user count!
 *
 * \param flag  Copying options (see BKE_library.h's LIB_ID_COPY_... flags for more).
 */
void BKE_scene_copy_data(Main *bmain, Scene *sce_dst, const Scene *sce_src, const int flag)
{
	/* We never handle usercount here for own data. */
	const int flag_subdata = flag | LIB_ID_CREATE_NO_USER_REFCOUNT;

	sce_dst->ed = NULL;
	sce_dst->depsgraph_hash = NULL;
	sce_dst->fps_info = NULL;

	/* Master Collection */
	if (sce_src->master_collection) {
		sce_dst->master_collection = BKE_collection_copy_master(bmain, sce_src->master_collection, flag);
	}

	/* View Layers */
	BLI_duplicatelist(&sce_dst->view_layers, &sce_src->view_layers);
	for (ViewLayer *view_layer_src = sce_src->view_layers.first, *view_layer_dst = sce_dst->view_layers.first;
	     view_layer_src;
	     view_layer_src = view_layer_src->next, view_layer_dst = view_layer_dst->next)
	{
		BKE_view_layer_copy_data(sce_dst, sce_src, view_layer_dst, view_layer_src, flag_subdata);
	}

	BLI_duplicatelist(&(sce_dst->markers), &(sce_src->markers));
	BLI_duplicatelist(&(sce_dst->transform_spaces), &(sce_src->transform_spaces));
	BLI_duplicatelist(&(sce_dst->r.views), &(sce_src->r.views));
	BKE_keyingsets_copy(&(sce_dst->keyingsets), &(sce_src->keyingsets));

	if (sce_src->nodetree) {
		/* Note: nodetree is *not* in bmain, however this specific case is handled at lower level
		 *       (see BKE_libblock_copy_ex()). */
		BKE_id_copy_ex(bmain, (ID *)sce_src->nodetree, (ID **)&sce_dst->nodetree, flag, false);
		BKE_libblock_relink_ex(bmain, sce_dst->nodetree, (void *)(&sce_src->id), &sce_dst->id, false);
	}

	if (sce_src->rigidbody_world) {
		sce_dst->rigidbody_world = BKE_rigidbody_world_copy(sce_src->rigidbody_world, flag_subdata);
	}

	/* copy color management settings */
	BKE_color_managed_display_settings_copy(&sce_dst->display_settings, &sce_src->display_settings);
	BKE_color_managed_view_settings_copy(&sce_dst->view_settings, &sce_src->view_settings);
	BKE_color_managed_colorspace_settings_copy(&sce_dst->sequencer_colorspace_settings, &sce_src->sequencer_colorspace_settings);

	BKE_color_managed_display_settings_copy(&sce_dst->r.im_format.display_settings, &sce_src->r.im_format.display_settings);
	BKE_color_managed_view_settings_copy(&sce_dst->r.im_format.view_settings, &sce_src->r.im_format.view_settings);

	BKE_color_managed_display_settings_copy(&sce_dst->r.bake.im_format.display_settings, &sce_src->r.bake.im_format.display_settings);
	BKE_color_managed_view_settings_copy(&sce_dst->r.bake.im_format.view_settings, &sce_src->r.bake.im_format.view_settings);

	curvemapping_copy_data(&sce_dst->r.mblur_shutter_curve, &sce_src->r.mblur_shutter_curve);

	/* tool settings */
	sce_dst->toolsettings = BKE_toolsettings_copy(sce_dst->toolsettings, flag_subdata);

	/* make a private copy of the avicodecdata */
	if (sce_src->r.avicodecdata) {
		sce_dst->r.avicodecdata = MEM_dupallocN(sce_src->r.avicodecdata);
		sce_dst->r.avicodecdata->lpFormat = MEM_dupallocN(sce_dst->r.avicodecdata->lpFormat);
		sce_dst->r.avicodecdata->lpParms = MEM_dupallocN(sce_dst->r.avicodecdata->lpParms);
	}

	if (sce_src->r.ffcodecdata.properties) { /* intentionally check sce_dst not sce_src. */  /* XXX ??? comment outdated... */
		sce_dst->r.ffcodecdata.properties = IDP_CopyProperty_ex(sce_src->r.ffcodecdata.properties, flag_subdata);
	}

	/* before scene copy */
	BKE_sound_create_scene(sce_dst);

	/* Copy sequencer, this is local data! */
	if (sce_src->ed) {
		sce_dst->ed = MEM_callocN(sizeof(*sce_dst->ed), __func__);
		sce_dst->ed->seqbasep = &sce_dst->ed->seqbase;
		BKE_sequence_base_dupli_recursive(
		            sce_src, sce_dst, &sce_dst->ed->seqbase, &sce_src->ed->seqbase, SEQ_DUPE_ALL, flag_subdata);
	}

	if ((flag & LIB_ID_COPY_NO_PREVIEW) == 0) {
		BKE_previewimg_id_copy(&sce_dst->id, &sce_src->id);
	}
	else {
		sce_dst->preview = NULL;
	}

	sce_dst->eevee.light_cache = NULL;
	/* TODO Copy the cache. */
}

Scene *BKE_scene_copy(Main *bmain, Scene *sce, int type)
{
	Scene *sce_copy;

	/* TODO this should/could most likely be replaced by call to more generic code at some point...
	 * But for now, let's keep it well isolated here. */
	if (type == SCE_COPY_EMPTY) {
		ListBase rv;

		sce_copy = BKE_scene_add(bmain, sce->id.name + 2);

		rv = sce_copy->r.views;
		curvemapping_free_data(&sce_copy->r.mblur_shutter_curve);
		sce_copy->r = sce->r;
		sce_copy->r.views = rv;
		sce_copy->unit = sce->unit;
		sce_copy->physics_settings = sce->physics_settings;
		sce_copy->audio = sce->audio;

		if (sce->id.properties)
			sce_copy->id.properties = IDP_CopyProperty(sce->id.properties);

		MEM_freeN(sce_copy->toolsettings);
		BKE_sound_destroy_scene(sce_copy);

		/* copy color management settings */
		BKE_color_managed_display_settings_copy(&sce_copy->display_settings, &sce->display_settings);
		BKE_color_managed_view_settings_copy(&sce_copy->view_settings, &sce->view_settings);
		BKE_color_managed_colorspace_settings_copy(&sce_copy->sequencer_colorspace_settings, &sce->sequencer_colorspace_settings);

		BKE_color_managed_display_settings_copy(&sce_copy->r.im_format.display_settings, &sce->r.im_format.display_settings);
		BKE_color_managed_view_settings_copy(&sce_copy->r.im_format.view_settings, &sce->r.im_format.view_settings);

		BKE_color_managed_display_settings_copy(&sce_copy->r.bake.im_format.display_settings, &sce->r.bake.im_format.display_settings);
		BKE_color_managed_view_settings_copy(&sce_copy->r.bake.im_format.view_settings, &sce->r.bake.im_format.view_settings);

		curvemapping_copy_data(&sce_copy->r.mblur_shutter_curve, &sce->r.mblur_shutter_curve);

		/* viewport display settings */
		sce_copy->display = sce->display;

		/* tool settings */
		sce_copy->toolsettings = BKE_toolsettings_copy(sce->toolsettings, 0);

		/* make a private copy of the avicodecdata */
		if (sce->r.avicodecdata) {
			sce_copy->r.avicodecdata = MEM_dupallocN(sce->r.avicodecdata);
			sce_copy->r.avicodecdata->lpFormat = MEM_dupallocN(sce_copy->r.avicodecdata->lpFormat);
			sce_copy->r.avicodecdata->lpParms = MEM_dupallocN(sce_copy->r.avicodecdata->lpParms);
		}

		if (sce->r.ffcodecdata.properties) { /* intentionally check scen not sce. */
			sce_copy->r.ffcodecdata.properties = IDP_CopyProperty(sce->r.ffcodecdata.properties);
		}

		/* before scene copy */
		BKE_sound_create_scene(sce_copy);

		/* grease pencil */
		sce_copy->gpd = NULL;

		sce_copy->preview = NULL;

		return sce_copy;
	}
	else {
		BKE_id_copy_ex(bmain, (ID *)sce, (ID **)&sce_copy, LIB_ID_COPY_ACTIONS, false);
		id_us_min(&sce_copy->id);
		id_us_ensure_real(&sce_copy->id);

		/* Extra actions, most notably SCE_FULL_COPY also duplicates several 'children' datablocks... */

		if (type == SCE_COPY_FULL) {
			/* Copy Freestyle LineStyle datablocks. */
			for (ViewLayer *view_layer_dst = sce_copy->view_layers.first; view_layer_dst; view_layer_dst = view_layer_dst->next) {
				for (FreestyleLineSet *lineset = view_layer_dst->freestyle_config.linesets.first; lineset; lineset = lineset->next) {
					if (lineset->linestyle) {
						/* XXX Not copying anim/actions here? */
						BKE_id_copy_ex(bmain, (ID *)lineset->linestyle, (ID **)&lineset->linestyle, 0, false);
					}
				}
			}

			/* Full copy of world (included animations) */
			if (sce_copy->world) {
				BKE_id_copy_ex(bmain, (ID *)sce_copy->world, (ID **)&sce_copy->world, LIB_ID_COPY_ACTIONS, false);
			}

			/* Collections */
			BKE_collection_copy_full(bmain, sce_copy->master_collection);

			/* Full copy of GreasePencil. */
			/* XXX Not copying anim/actions here? */
			if (sce_copy->gpd) {
				BKE_id_copy_ex(bmain, (ID *)sce_copy->gpd, (ID **)&sce_copy->gpd, 0, false);
			}
		}
		else {
			/* Remove sequencer if not full copy */
			/* XXX Why in Hell? :/ */
			remove_sequencer_fcurves(sce_copy);
			BKE_sequencer_editing_free(sce_copy, true);
		}

		/* NOTE: part of SCE_COPY_LINK_DATA and SCE_COPY_FULL operations
		 * are done outside of blenkernel with ED_objects_single_users! */

		/*  camera */
		if (ELEM(type, SCE_COPY_LINK_DATA, SCE_COPY_FULL)) {
			ID_NEW_REMAP(sce_copy->camera);
		}

		return sce_copy;
	}
}

void BKE_scene_groups_relink(Scene *sce)
{
	if (sce->rigidbody_world)
		BKE_rigidbody_world_groups_relink(sce->rigidbody_world);
}

void BKE_scene_make_local(Main *bmain, Scene *sce, const bool lib_local)
{
	/* For now should work, may need more work though to support all possible corner cases
	 * (also scene_copy probably needs some love). */
	BKE_id_make_local_generic(bmain, &sce->id, true, lib_local);
}

/** Free (or release) any data used by this scene (does not free the scene itself). */
void BKE_scene_free_ex(Scene *sce, const bool do_id_user)
{
	BKE_animdata_free((ID *)sce, false);

	BKE_sequencer_editing_free(sce, do_id_user);

	BKE_keyingsets_free(&sce->keyingsets);

	/* is no lib link block, but scene extension */
	if (sce->nodetree) {
		ntreeFreeTree(sce->nodetree);
		MEM_freeN(sce->nodetree);
		sce->nodetree = NULL;
	}

	if (sce->rigidbody_world) {
		BKE_rigidbody_free_world(sce);
	}

	if (sce->r.avicodecdata) {
		free_avicodecdata(sce->r.avicodecdata);
		MEM_freeN(sce->r.avicodecdata);
		sce->r.avicodecdata = NULL;
	}
	if (sce->r.ffcodecdata.properties) {
		IDP_FreeProperty(sce->r.ffcodecdata.properties);
		MEM_freeN(sce->r.ffcodecdata.properties);
		sce->r.ffcodecdata.properties = NULL;
	}

	BLI_freelistN(&sce->markers);
	BLI_freelistN(&sce->transform_spaces);
	BLI_freelistN(&sce->r.views);

	BKE_toolsettings_free(sce->toolsettings);
	sce->toolsettings = NULL;

	BKE_scene_free_depsgraph_hash(sce);

	MEM_SAFE_FREE(sce->fps_info);

	BKE_sound_destroy_scene(sce);

	BKE_color_managed_view_settings_free(&sce->view_settings);

	BKE_previewimg_free(&sce->preview);
	curvemapping_free_data(&sce->r.mblur_shutter_curve);

	for (ViewLayer *view_layer = sce->view_layers.first, *view_layer_next; view_layer; view_layer = view_layer_next) {
		view_layer_next = view_layer->next;

		BLI_remlink(&sce->view_layers, view_layer);
		BKE_view_layer_free_ex(view_layer, do_id_user);
	}

	/* Master Collection */
	// TODO: what to do with do_id_user? it's also true when just
	// closing the file which seems wrong? should decrement users
	// for objects directly in the master collection? then other
	// collections in the scene need to do it too?
	if (sce->master_collection) {
		BKE_collection_free(sce->master_collection);
		MEM_freeN(sce->master_collection);
		sce->master_collection = NULL;
	}

	if (sce->eevee.light_cache) {
		EEVEE_lightcache_free(sce->eevee.light_cache);
		sce->eevee.light_cache = NULL;
	}

	/* These are freed on doversion. */
	BLI_assert(sce->layer_properties == NULL);
}

void BKE_scene_free(Scene *sce)
{
	BKE_scene_free_ex(sce, true);
}

void BKE_scene_init(Scene *sce)
{
	ParticleEditSettings *pset;
	int a;
	const char *colorspace_name;
	SceneRenderView *srv;
	CurveMapping *mblur_shutter_curve;

	BLI_assert(MEMCMP_STRUCT_OFS_IS_ZERO(sce, id));

	sce->lay = sce->layact = 1;

	sce->r.mode = R_OSA;
	sce->r.cfra = 1;
	sce->r.sfra = 1;
	sce->r.efra = 250;
	sce->r.frame_step = 1;
	sce->r.xsch = 1920;
	sce->r.ysch = 1080;
	sce->r.xasp = 1;
	sce->r.yasp = 1;
	sce->r.tilex = 256;
	sce->r.tiley = 256;
	sce->r.size = 100;

	sce->r.im_format.planes = R_IMF_PLANES_RGBA;
	sce->r.im_format.imtype = R_IMF_IMTYPE_PNG;
	sce->r.im_format.depth = R_IMF_CHAN_DEPTH_8;
	sce->r.im_format.quality = 90;
	sce->r.im_format.compress = 15;

	sce->r.displaymode = R_OUTPUT_WINDOW;
	sce->r.framapto = 100;
	sce->r.images = 100;
	sce->r.framelen = 1.0;
	sce->r.blurfac = 0.5;
	sce->r.frs_sec = 24;
	sce->r.frs_sec_base = 1;

	/* OCIO_TODO: for forwards compatibility only, so if no tonecurve are used,
	 *            images would look in the same way as in current blender
	 *
	 *            perhaps at some point should be completely deprecated?
	 */
	sce->r.color_mgt_flag |= R_COLOR_MANAGEMENT;

	sce->r.dither_intensity = 1.0f;

	sce->r.bake_mode = 0;
	sce->r.bake_filter = 16;
	sce->r.bake_flag = R_BAKE_CLEAR;
	sce->r.bake_samples = 256;
	sce->r.bake_biasdist = 0.001;

	sce->r.bake.flag = R_BAKE_CLEAR;
	sce->r.bake.pass_filter = R_BAKE_PASS_FILTER_ALL;
	sce->r.bake.width = 512;
	sce->r.bake.height = 512;
	sce->r.bake.margin = 16;
	sce->r.bake.normal_space = R_BAKE_SPACE_TANGENT;
	sce->r.bake.normal_swizzle[0] = R_BAKE_POSX;
	sce->r.bake.normal_swizzle[1] = R_BAKE_POSY;
	sce->r.bake.normal_swizzle[2] = R_BAKE_POSZ;
	BLI_strncpy(sce->r.bake.filepath, U.renderdir, sizeof(sce->r.bake.filepath));

	sce->r.bake.im_format.planes = R_IMF_PLANES_RGBA;
	sce->r.bake.im_format.imtype = R_IMF_IMTYPE_PNG;
	sce->r.bake.im_format.depth = R_IMF_CHAN_DEPTH_8;
	sce->r.bake.im_format.quality = 90;
	sce->r.bake.im_format.compress = 15;

	sce->r.scemode = R_DOCOMP | R_DOSEQ | R_EXTENSION;
	sce->r.stamp = R_STAMP_TIME | R_STAMP_FRAME | R_STAMP_DATE | R_STAMP_CAMERA | R_STAMP_SCENE | R_STAMP_FILENAME | R_STAMP_RENDERTIME | R_STAMP_MEMORY;
	sce->r.stamp_font_id = 12;
	sce->r.fg_stamp[0] = sce->r.fg_stamp[1] = sce->r.fg_stamp[2] = 0.8f;
	sce->r.fg_stamp[3] = 1.0f;
	sce->r.bg_stamp[0] = sce->r.bg_stamp[1] = sce->r.bg_stamp[2] = 0.0f;
	sce->r.bg_stamp[3] = 0.25f;

	sce->r.seq_prev_type = OB_SOLID;
	sce->r.seq_rend_type = OB_SOLID;
	sce->r.seq_flag = 0;

	sce->r.threads = 1;

	sce->r.simplify_subsurf = 6;
	sce->r.simplify_particles = 1.0f;

	sce->r.border.xmin = 0.0f;
	sce->r.border.ymin = 0.0f;
	sce->r.border.xmax = 1.0f;
	sce->r.border.ymax = 1.0f;

	sce->r.preview_start_resolution = 64;

	sce->r.line_thickness_mode = R_LINE_THICKNESS_ABSOLUTE;
	sce->r.unit_line_thickness = 1.0f;

	mblur_shutter_curve = &sce->r.mblur_shutter_curve;
	curvemapping_set_defaults(mblur_shutter_curve, 1, 0.0f, 0.0f, 1.0f, 1.0f);
	curvemapping_initialize(mblur_shutter_curve);
	curvemap_reset(mblur_shutter_curve->cm,
	               &mblur_shutter_curve->clipr,
	               CURVE_PRESET_MAX,
	               CURVEMAP_SLOPE_POS_NEG);

	sce->toolsettings = MEM_callocN(sizeof(struct ToolSettings), "Tool Settings Struct");

	sce->toolsettings->object_flag |= SCE_OBJECT_MODE_LOCK;
	sce->toolsettings->doublimit = 0.001;
	sce->toolsettings->vgroup_weight = 1.0f;
	sce->toolsettings->uvcalc_margin = 0.001f;
	sce->toolsettings->uvcalc_flag = UVCALC_TRANSFORM_CORRECT;
	sce->toolsettings->unwrapper = 1;
	sce->toolsettings->select_thresh = 0.01f;
	sce->toolsettings->gizmo_flag = SCE_MANIP_TRANSLATE | SCE_MANIP_ROTATE | SCE_MANIP_SCALE;

	sce->toolsettings->selectmode = SCE_SELECT_VERTEX;
	sce->toolsettings->uv_selectmode = UV_SELECT_VERTEX;
	sce->toolsettings->autokey_mode = U.autokey_mode;


	sce->toolsettings->transform_pivot_point = V3D_AROUND_CENTER_MEAN;
	sce->toolsettings->snap_mode = SCE_SNAP_MODE_INCREMENT;
	sce->toolsettings->snap_node_mode = SCE_SNAP_MODE_GRID;
	sce->toolsettings->snap_uv_mode = SCE_SNAP_MODE_INCREMENT;

	sce->toolsettings->curve_paint_settings.curve_type = CU_BEZIER;
	sce->toolsettings->curve_paint_settings.flag |= CURVE_PAINT_FLAG_CORNERS_DETECT;
	sce->toolsettings->curve_paint_settings.error_threshold = 8;
	sce->toolsettings->curve_paint_settings.radius_max = 1.0f;
	sce->toolsettings->curve_paint_settings.corner_angle = DEG2RADF(70.0f);

	sce->toolsettings->statvis.overhang_axis = OB_NEGZ;
	sce->toolsettings->statvis.overhang_min = 0;
	sce->toolsettings->statvis.overhang_max = DEG2RADF(45.0f);
	sce->toolsettings->statvis.thickness_max = 0.1f;
	sce->toolsettings->statvis.thickness_samples = 1;
	sce->toolsettings->statvis.distort_min = DEG2RADF(5.0f);
	sce->toolsettings->statvis.distort_max = DEG2RADF(45.0f);

	sce->toolsettings->statvis.sharp_min = DEG2RADF(90.0f);
	sce->toolsettings->statvis.sharp_max = DEG2RADF(180.0f);

	sce->toolsettings->proportional_size = 1.0f;

	sce->toolsettings->imapaint.paint.flags |= PAINT_SHOW_BRUSH;
	sce->toolsettings->imapaint.normal_angle = 80;
	sce->toolsettings->imapaint.seam_bleed = 2;

	sce->physics_settings.gravity[0] = 0.0f;
	sce->physics_settings.gravity[1] = 0.0f;
	sce->physics_settings.gravity[2] = -9.81f;
	sce->physics_settings.flag = PHYS_GLOBAL_GRAVITY;

	sce->unit.system = USER_UNIT_METRIC;
	sce->unit.scale_length = 1.0f;

	pset = &sce->toolsettings->particle;
	pset->flag = PE_KEEP_LENGTHS | PE_LOCK_FIRST | PE_DEFLECT_EMITTER | PE_AUTO_VELOCITY;
	pset->emitterdist = 0.25f;
	pset->totrekey = 5;
	pset->totaddkey = 5;
	pset->brushtype = PE_BRUSH_NONE;
	pset->draw_step = 2;
	pset->fade_frames = 2;
	pset->selectmode = SCE_SELECT_PATH;

	for (a = 0; a < ARRAY_SIZE(pset->brush); a++) {
		pset->brush[a].strength = 0.5f;
		pset->brush[a].size = 50;
		pset->brush[a].step = 10;
		pset->brush[a].count = 10;
	}
	pset->brush[PE_BRUSH_CUT].strength = 1.0f;

	sce->r.ffcodecdata.audio_mixrate = 48000;
	sce->r.ffcodecdata.audio_volume = 1.0f;
	sce->r.ffcodecdata.audio_bitrate = 192;
	sce->r.ffcodecdata.audio_channels = 2;

	BLI_strncpy(sce->r.engine, RE_engine_id_BLENDER_EEVEE, sizeof(sce->r.engine));

	sce->audio.distance_model = 2.0f;
	sce->audio.doppler_factor = 1.0f;
	sce->audio.speed_of_sound = 343.3f;
	sce->audio.volume = 1.0f;
	sce->audio.flag = AUDIO_SYNC;

	BLI_strncpy(sce->r.pic, U.renderdir, sizeof(sce->r.pic));

	BLI_rctf_init(&sce->r.safety, 0.1f, 0.9f, 0.1f, 0.9f);
	sce->r.osa = 8;

	/* note; in header_info.c the scene copy happens..., if you add more to renderdata it has to be checked there */

	/* multiview - stereo */
	BKE_scene_add_render_view(sce, STEREO_LEFT_NAME);
	srv = sce->r.views.first;
	BLI_strncpy(srv->suffix, STEREO_LEFT_SUFFIX, sizeof(srv->suffix));

	BKE_scene_add_render_view(sce, STEREO_RIGHT_NAME);
	srv = sce->r.views.last;
	BLI_strncpy(srv->suffix, STEREO_RIGHT_SUFFIX, sizeof(srv->suffix));

	BKE_sound_create_scene(sce);

	/* color management */
	colorspace_name = IMB_colormanagement_role_colorspace_name_get(COLOR_ROLE_DEFAULT_SEQUENCER);

	BKE_color_managed_display_settings_init(&sce->display_settings);
	BKE_color_managed_view_settings_init(&sce->view_settings);
	BLI_strncpy(sce->sequencer_colorspace_settings.name, colorspace_name,
	            sizeof(sce->sequencer_colorspace_settings.name));

	/* Safe Areas */
	copy_v2_fl2(sce->safe_areas.title, 3.5f / 100.0f, 3.5f / 100.0f);
	copy_v2_fl2(sce->safe_areas.action, 10.0f / 100.0f, 5.0f / 100.0f);
	copy_v2_fl2(sce->safe_areas.title_center, 17.5f / 100.0f, 5.0f / 100.0f);
	copy_v2_fl2(sce->safe_areas.action_center, 15.0f / 100.0f, 5.0f / 100.0f);

	sce->preview = NULL;

	/* GP Sculpt brushes */
	{
		GP_BrushEdit_Settings *gset = &sce->toolsettings->gp_sculpt;
		GP_EditBrush_Data *gp_brush;

		gp_brush = &gset->brush[GP_EDITBRUSH_TYPE_SMOOTH];
		gp_brush->size = 25;
		gp_brush->strength = 0.3f;
		gp_brush->flag = GP_EDITBRUSH_FLAG_USE_FALLOFF | GP_EDITBRUSH_FLAG_SMOOTH_PRESSURE;

		gp_brush = &gset->brush[GP_EDITBRUSH_TYPE_THICKNESS];
		gp_brush->size = 25;
		gp_brush->strength = 0.5f;
		gp_brush->flag = GP_EDITBRUSH_FLAG_USE_FALLOFF;

		gp_brush = &gset->brush[GP_EDITBRUSH_TYPE_STRENGTH];
		gp_brush->size = 25;
		gp_brush->strength = 0.5f;
		gp_brush->flag = GP_EDITBRUSH_FLAG_USE_FALLOFF;

		gp_brush = &gset->brush[GP_EDITBRUSH_TYPE_GRAB];
		gp_brush->size = 50;
		gp_brush->strength = 0.3f;
		gp_brush->flag = GP_EDITBRUSH_FLAG_USE_FALLOFF;

		gp_brush = &gset->brush[GP_EDITBRUSH_TYPE_PUSH];
		gp_brush->size = 25;
		gp_brush->strength = 0.3f;
		gp_brush->flag = GP_EDITBRUSH_FLAG_USE_FALLOFF;

		gp_brush = &gset->brush[GP_EDITBRUSH_TYPE_TWIST];
		gp_brush->size = 50;
		gp_brush->strength = 0.3f; // XXX?
		gp_brush->flag = GP_EDITBRUSH_FLAG_USE_FALLOFF;

		gp_brush = &gset->brush[GP_EDITBRUSH_TYPE_PINCH];
		gp_brush->size = 50;
		gp_brush->strength = 0.5f; // XXX?
		gp_brush->flag = GP_EDITBRUSH_FLAG_USE_FALLOFF;

		gp_brush = &gset->brush[GP_EDITBRUSH_TYPE_RANDOMIZE];
		gp_brush->size = 25;
		gp_brush->strength = 0.5f;
		gp_brush->flag = GP_EDITBRUSH_FLAG_USE_FALLOFF;
	}

	/* GP Stroke Placement */
	sce->toolsettings->gpencil_v3d_align = GP_PROJECT_VIEWSPACE;
	sce->toolsettings->gpencil_v2d_align = GP_PROJECT_VIEWSPACE;
	sce->toolsettings->gpencil_seq_align = GP_PROJECT_VIEWSPACE;
	sce->toolsettings->gpencil_ima_align = GP_PROJECT_VIEWSPACE;

	sce->orientation_index_custom = -1;

	/* Master Collection */
	sce->master_collection = BKE_collection_master_add();

	BKE_view_layer_add(sce, "View Layer");

	/* SceneDisplay */
	copy_v3_v3(sce->display.light_direction, (float[3]){-M_SQRT1_3, -M_SQRT1_3, M_SQRT1_3});
	sce->display.shadow_shift = 0.1;

	sce->display.matcap_ssao_distance = 0.2f;
	sce->display.matcap_ssao_attenuation = 1.0f;
	sce->display.matcap_ssao_samples = 16;

	/* OpenGL Render. */
	BKE_screen_view3d_shading_init(&sce->display.shading);

	/* SceneEEVEE */
	sce->eevee.gi_diffuse_bounces = 3;
	sce->eevee.gi_cubemap_resolution = 512;
	sce->eevee.gi_visibility_resolution = 32;
	sce->eevee.gi_cubemap_draw_size = 0.3f;
	sce->eevee.gi_irradiance_draw_size = 0.1f;

	sce->eevee.taa_samples = 16;
	sce->eevee.taa_render_samples = 64;

	sce->eevee.sss_samples = 7;
	sce->eevee.sss_jitter_threshold = 0.3f;

	sce->eevee.ssr_quality = 0.25f;
	sce->eevee.ssr_max_roughness = 0.5f;
	sce->eevee.ssr_thickness = 0.2f;
	sce->eevee.ssr_border_fade = 0.075f;
	sce->eevee.ssr_firefly_fac = 10.0f;

	sce->eevee.volumetric_start = 0.1f;
	sce->eevee.volumetric_end = 100.0f;
	sce->eevee.volumetric_tile_size = 8;
	sce->eevee.volumetric_samples = 64;
	sce->eevee.volumetric_sample_distribution = 0.8f;
	sce->eevee.volumetric_light_clamp = 0.0f;
	sce->eevee.volumetric_shadow_samples = 16;

	sce->eevee.gtao_distance = 0.2f;
	sce->eevee.gtao_factor = 1.0f;
	sce->eevee.gtao_quality = 0.25f;

	sce->eevee.bokeh_max_size = 100.0f;
	sce->eevee.bokeh_threshold = 1.0f;

	copy_v3_fl(sce->eevee.bloom_color, 1.0f);
	sce->eevee.bloom_threshold = 0.8f;
	sce->eevee.bloom_knee = 0.5f;
	sce->eevee.bloom_intensity = 0.8f;
	sce->eevee.bloom_radius = 6.5f;
	sce->eevee.bloom_clamp = 1.0f;

	sce->eevee.motion_blur_samples = 8;
	sce->eevee.motion_blur_shutter = 1.0f;

	sce->eevee.shadow_method = SHADOW_ESM;
	sce->eevee.shadow_cube_size = 512;
	sce->eevee.shadow_cascade_size = 1024;

	sce->eevee.light_cache = NULL;

	sce->eevee.flag =
	        SCE_EEVEE_VOLUMETRIC_LIGHTS |
	        SCE_EEVEE_VOLUMETRIC_COLORED |
	        SCE_EEVEE_GTAO_BENT_NORMALS |
	        SCE_EEVEE_GTAO_BOUNCE |
	        SCE_EEVEE_TAA_REPROJECTION |
	        SCE_EEVEE_SSR_HALF_RESOLUTION;
}

Scene *BKE_scene_add(Main *bmain, const char *name)
{
	Scene *sce;

	sce = BKE_libblock_alloc(bmain, ID_SCE, name, 0);
	id_us_min(&sce->id);
	id_us_ensure_real(&sce->id);

	BKE_scene_init(sce);

	return sce;
}

/**
 * Check if there is any intance of the object in the scene
 */
bool BKE_scene_object_find(Scene *scene, Object *ob)
{
	for (ViewLayer *view_layer = scene->view_layers.first; view_layer; view_layer = view_layer->next) {
		if (BLI_findptr(&view_layer->object_bases, ob, offsetof(Base, object))) {
		    return true;
		}
	}
	return false;
}

Object *BKE_scene_object_find_by_name(Scene *scene, const char *name)
{
	for (ViewLayer *view_layer = scene->view_layers.first; view_layer; view_layer = view_layer->next) {
		for (Base *base = view_layer->object_bases.first; base; base = base->next) {
			if (STREQ(base->object->id.name + 2, name)) {
				return base->object;
			}
		}
	}
	return NULL;
}

/**
 * Sets the active scene, mainly used when running in background mode (``--scene`` command line argument).
 * This is also called to set the scene directly, bypassing windowing code.
 * Otherwise #WM_window_set_active_scene is used when changing scenes by the user.
 */
void BKE_scene_set_background(Main *bmain, Scene *scene)
{
	Object *ob;

	/* check for cyclic sets, for reading old files but also for definite security (py?) */
	BKE_scene_validate_setscene(bmain, scene);

	/* deselect objects (for dataselect) */
	for (ob = bmain->object.first; ob; ob = ob->id.next)
		ob->flag &= ~SELECT;

	/* copy layers and flags from bases to objects */
	for (ViewLayer *view_layer = scene->view_layers.first; view_layer; view_layer = view_layer->next) {
		for (Base *base = view_layer->object_bases.first; base; base = base->next) {
			ob = base->object;
			/* collection patch... */
			BKE_scene_object_base_flag_sync_from_base(base);
		}
	}
	/* no full animation update, this to enable render code to work (render code calls own animation updates) */
}

/* called from creator_args.c */
Scene *BKE_scene_set_name(Main *bmain, const char *name)
{
	Scene *sce = (Scene *)BKE_libblock_find_name(bmain, ID_SCE, name);
	if (sce) {
		BKE_scene_set_background(bmain, sce);
		printf("Scene switch for render: '%s' in file: '%s'\n", name, BKE_main_blendfile_path(bmain));
		return sce;
	}

	printf("Can't find scene: '%s' in file: '%s'\n", name, BKE_main_blendfile_path(bmain));
	return NULL;
}

/* Used by metaballs, return *all* objects (including duplis) existing in the scene (including scene's sets) */
int BKE_scene_base_iter_next(Depsgraph *depsgraph, SceneBaseIter *iter,
        Scene **scene, int val, Base **base, Object **ob)
{
	bool run_again = true;

	/* init */
	if (val == 0) {
		iter->phase = F_START;
		iter->dupob = NULL;
		iter->duplilist = NULL;
		iter->dupli_refob = NULL;
	}
	else {
		/* run_again is set when a duplilist has been ended */
		while (run_again) {
			run_again = false;

			/* the first base */
			if (iter->phase == F_START) {
				ViewLayer *view_layer = (depsgraph) ?
					DEG_get_evaluated_view_layer(depsgraph) :
					BKE_view_layer_context_active_PLACEHOLDER(*scene);
				*base = view_layer->object_bases.first;
				if (*base) {
					*ob = (*base)->object;
					iter->phase = F_SCENE;
				}
				else {
					/* exception: empty scene layer */
					while ((*scene)->set) {
						(*scene) = (*scene)->set;
						ViewLayer *view_layer_set = BKE_view_layer_default_render((*scene));
						if (view_layer_set->object_bases.first) {
							*base = view_layer_set->object_bases.first;
							*ob = (*base)->object;
							iter->phase = F_SCENE;
							break;
						}
					}
				}
			}
			else {
				if (*base && iter->phase != F_DUPLI) {
					*base = (*base)->next;
					if (*base) {
						*ob = (*base)->object;
					}
					else {
						if (iter->phase == F_SCENE) {
							/* (*scene) is finished, now do the set */
							while ((*scene)->set) {
								(*scene) = (*scene)->set;
								ViewLayer *view_layer_set = BKE_view_layer_default_render((*scene));
								if (view_layer_set->object_bases.first) {
									*base = view_layer_set->object_bases.first;
									*ob = (*base)->object;
									break;
								}
							}
						}
					}
				}
			}

			if (*base == NULL) {
				iter->phase = F_START;
			}
			else {
				if (iter->phase != F_DUPLI) {
					if (depsgraph && (*base)->object->transflag & OB_DUPLI) {
						/* collections cannot be duplicated for mballs yet,
						 * this enters eternal loop because of
						 * makeDispListMBall getting called inside of collection_duplilist */
						if ((*base)->object->dup_group == NULL) {
							iter->duplilist = object_duplilist(depsgraph, (*scene), (*base)->object);

							iter->dupob = iter->duplilist->first;

							if (!iter->dupob) {
								free_object_duplilist(iter->duplilist);
								iter->duplilist = NULL;
							}
							iter->dupli_refob = NULL;
						}
					}
				}
				/* handle dupli's */
				if (iter->dupob) {
					(*base)->flag_legacy |= OB_FROMDUPLI;
					*ob = iter->dupob->ob;
					iter->phase = F_DUPLI;

					if (iter->dupli_refob != *ob) {
						if (iter->dupli_refob) {
							/* Restore previous object's real matrix. */
							copy_m4_m4(iter->dupli_refob->obmat, iter->omat);
						}
						/* Backup new object's real matrix. */
						iter->dupli_refob = *ob;
						copy_m4_m4(iter->omat, iter->dupli_refob->obmat);
					}
					copy_m4_m4((*ob)->obmat, iter->dupob->mat);

					iter->dupob = iter->dupob->next;
				}
				else if (iter->phase == F_DUPLI) {
					iter->phase = F_SCENE;
					(*base)->flag_legacy &= ~OB_FROMDUPLI;

					if (iter->dupli_refob) {
						/* Restore last object's real matrix. */
						copy_m4_m4(iter->dupli_refob->obmat, iter->omat);
						iter->dupli_refob = NULL;
					}

					free_object_duplilist(iter->duplilist);
					iter->duplilist = NULL;
					run_again = true;
				}
			}
		}
	}

#if 0
	if (ob && *ob) {
		printf("Scene: '%s', '%s'\n", (*scene)->id.name + 2, (*ob)->id.name + 2);
	}
#endif

	return iter->phase;
}

Scene *BKE_scene_find_from_collection(const Main *bmain, const Collection *collection)
{
	for (Scene *scene = bmain->scene.first; scene; scene = scene->id.next) {
		for (ViewLayer *layer = scene->view_layers.first; layer; layer = layer->next) {
			if (BKE_view_layer_has_collection(layer, collection)) {
				return scene;
			}
		}
	}

	return NULL;
}

#ifdef DURIAN_CAMERA_SWITCH
Object *BKE_scene_camera_switch_find(Scene *scene)
{
	if (scene->r.mode & R_NO_CAMERA_SWITCH) {
		return NULL;
	}

	TimeMarker *m;
	int cfra = scene->r.cfra;
	int frame = -(MAXFRAME + 1);
	int min_frame = MAXFRAME + 1;
	Object *camera = NULL;
	Object *first_camera = NULL;

	for (m = scene->markers.first; m; m = m->next) {
		if (m->camera && (m->camera->restrictflag & OB_RESTRICT_RENDER) == 0) {
			if ((m->frame <= cfra) && (m->frame > frame)) {
				camera = m->camera;
				frame = m->frame;

				if (frame == cfra)
					break;
			}

			if (m->frame < min_frame) {
				first_camera = m->camera;
				min_frame = m->frame;
			}
		}
	}

	if (camera == NULL) {
		/* If there's no marker to the left of current frame,
		 * use camera from left-most marker to solve all sort
		 * of Schrodinger uncertainties.
		 */
		return first_camera;
	}

	return camera;
}
#endif

int BKE_scene_camera_switch_update(Scene *scene)
{
#ifdef DURIAN_CAMERA_SWITCH
	Object *camera = BKE_scene_camera_switch_find(scene);
	if (camera) {
		scene->camera = camera;
		DEG_id_tag_update(&scene->id, DEG_TAG_COPY_ON_WRITE);
		return 1;
	}
#else
	(void)scene;
#endif
	return 0;
}

char *BKE_scene_find_marker_name(Scene *scene, int frame)
{
	ListBase *markers = &scene->markers;
	TimeMarker *m1, *m2;

	/* search through markers for match */
	for (m1 = markers->first, m2 = markers->last; m1 && m2; m1 = m1->next, m2 = m2->prev) {
		if (m1->frame == frame)
			return m1->name;

		if (m1 == m2)
			break;

		if (m2->frame == frame)
			return m2->name;
	}

	return NULL;
}

/* return the current marker for this frame,
 * we can have more than 1 marker per frame, this just returns the first :/ */
char *BKE_scene_find_last_marker_name(Scene *scene, int frame)
{
	TimeMarker *marker, *best_marker = NULL;
	int best_frame = -MAXFRAME * 2;
	for (marker = scene->markers.first; marker; marker = marker->next) {
		if (marker->frame == frame) {
			return marker->name;
		}

		if (marker->frame > best_frame && marker->frame < frame) {
			best_marker = marker;
			best_frame = marker->frame;
		}
	}

	return best_marker ? best_marker->name : NULL;
}

void BKE_scene_remove_rigidbody_object(struct Main *bmain, Scene *scene, Object *ob)
{
	/* remove rigid body constraint from world before removing object */
	if (ob->rigidbody_constraint)
		BKE_rigidbody_remove_constraint(scene, ob);
	/* remove rigid body object from world before removing object */
	if (ob->rigidbody_object)
		BKE_rigidbody_remove_object(bmain, scene, ob);
}

/* checks for cycle, returns 1 if it's all OK */
bool BKE_scene_validate_setscene(Main *bmain, Scene *sce)
{
	Scene *sce_iter;
	int a, totscene;

	if (sce->set == NULL) return true;
	totscene = BLI_listbase_count(&bmain->scene);

	for (a = 0, sce_iter = sce; sce_iter->set; sce_iter = sce_iter->set, a++) {
		/* more iterations than scenes means we have a cycle */
		if (a > totscene) {
			/* the tested scene gets zero'ed, that's typically current scene */
			sce->set = NULL;
			return false;
		}
	}

	return true;
}

/* This function is needed to cope with fractional frames - including two Blender rendering features
 * mblur (motion blur that renders 'subframes' and blurs them together), and fields rendering.
 */
float BKE_scene_frame_get(const Scene *scene)
{
	return BKE_scene_frame_get_from_ctime(scene, scene->r.cfra);
}

/* This function is used to obtain arbitrary fractional frames */
float BKE_scene_frame_get_from_ctime(const Scene *scene, const float frame)
{
	float ctime = frame;
	ctime += scene->r.subframe;
	ctime *= scene->r.framelen;

	return ctime;
}

/**
 * Sets the frame int/float components.
 */
void BKE_scene_frame_set(struct Scene *scene, double cfra)
{
	double intpart;
	scene->r.subframe = modf(cfra, &intpart);
	scene->r.cfra = (int)intpart;
}

/* That's like really a bummer, because currently animation data for armatures
 * might want to use pose, and pose might be missing on the object.
 * This happens when changing visible layers, which leads to situations when
 * pose is missing or marked for recalc, animation will change it and then
 * object update will restore the pose.
 *
 * This could be solved by the new dependency graph, but for until then we'll
 * do an extra pass on the objects to ensure it's all fine.
 */
#define POSE_ANIMATION_WORKAROUND

#ifdef POSE_ANIMATION_WORKAROUND
static void scene_armature_depsgraph_workaround(Main *bmain, Depsgraph *depsgraph)
{
	Object *ob;
	if (BLI_listbase_is_empty(&bmain->armature) || !DEG_id_type_updated(depsgraph, ID_OB)) {
		return;
	}
	for (ob = bmain->object.first; ob; ob = ob->id.next) {
		if (ob->type == OB_ARMATURE && ob->adt && ob->adt->recalc & ADT_RECALC_ANIM) {
			if (ob->pose == NULL || (ob->pose->flag & POSE_RECALC)) {
				BKE_pose_rebuild(bmain, ob, ob->data);
			}
		}
	}
}
#endif

static bool check_rendered_viewport_visible(Main *bmain)
{
	wmWindowManager *wm = bmain->wm.first;
	wmWindow *window;
	for (window = wm->windows.first; window != NULL; window = window->next) {
		const bScreen *screen = BKE_workspace_active_screen_get(window->workspace_hook);
		Scene *scene = window->scene;
		RenderEngineType *type = RE_engines_find(scene->r.engine);

		if (type->draw_engine || !type->render) {
			continue;
		}

		for (ScrArea *area = screen->areabase.first; area != NULL; area = area->next) {
			View3D *v3d = area->spacedata.first;
			if (area->spacetype != SPACE_VIEW3D) {
				continue;
			}
			if (v3d->shading.type == OB_RENDER) {
				return true;
			}
		}
	}
	return false;
}

/* TODO(campbell): shouldn't we be able to use 'DEG_get_view_layer' here?
 * Currently this is NULL on load, so don't. */
static void prepare_mesh_for_viewport_render(
        Main *bmain, const ViewLayer *view_layer)
{
	/* This is needed to prepare mesh to be used by the render
	 * engine from the viewport rendering. We do loading here
	 * so all the objects which shares the same mesh datablock
	 * are nicely tagged for update and updated.
	 *
	 * This makes it so viewport render engine doesn't need to
	 * call loading of the edit data for the mesh objects.
	 */

	Object *obedit = OBEDIT_FROM_VIEW_LAYER(view_layer);
	if (obedit) {
		Mesh *mesh = obedit->data;
		if ((obedit->type == OB_MESH) &&
		    ((obedit->id.recalc & ID_RECALC_ALL) ||
		     (mesh->id.recalc & ID_RECALC_ALL)))
		{
			if (check_rendered_viewport_visible(bmain)) {
				BMesh *bm = mesh->edit_btmesh->bm;
				BM_mesh_bm_to_me(
				        bmain, bm, mesh,
				        (&(struct BMeshToMeshParams){
				            .calc_object_remap = true,
				        }));
				DEG_id_tag_update(&mesh->id, 0);
			}
		}
	}
}

/* TODO(sergey): This actually should become view_layer_graph or so.
 * Same applies to update_for_newframe.
 */
void BKE_scene_graph_update_tagged(Depsgraph *depsgraph,
                                   Main *bmain)
{
	Scene *scene = DEG_get_input_scene(depsgraph);
	ViewLayer *view_layer = DEG_get_input_view_layer(depsgraph);

	/* TODO(sergey): Some functions here are changing global state,
	 * for example, clearing update tags from bmain.
	 */
	/* (Re-)build dependency graph if needed. */
	DEG_graph_relations_update(depsgraph, bmain, scene, view_layer);
	/* Uncomment this to check if graph was properly tagged for update. */
	// DEG_debug_graph_relations_validate(depsgraph, bmain, scene);
	/* Flush editing data if needed. */
	prepare_mesh_for_viewport_render(bmain, view_layer);
	/* Flush recalc flags to dependencies. */
	DEG_graph_flush_update(bmain, depsgraph);
	/* Update all objects: drivers, matrices, displists, etc. flags set
	 * by depgraph or manual, no layer check here, gets correct flushed.
	 */
	DEG_evaluate_on_refresh(depsgraph);
	/* Update sound system animation (TODO, move to depsgraph). */
	BKE_sound_update_scene(bmain, scene);
	/* Inform editors about possible changes. */
	DEG_ids_check_recalc(bmain, depsgraph, scene, view_layer, false);
	/* Clear recalc flags. */
	DEG_ids_clear_recalc(bmain, depsgraph);
}

/* applies changes right away, does all sets too */
void BKE_scene_graph_update_for_newframe(Depsgraph *depsgraph,
                                         Main *bmain)
{
	Scene *scene = DEG_get_input_scene(depsgraph);
	ViewLayer *view_layer = DEG_get_input_view_layer(depsgraph);

	/* TODO(sergey): Some functions here are changing global state,
	 * for example, clearing update tags from bmain.
	 */
	const float ctime = BKE_scene_frame_get(scene);
	/* Keep this first. */
	BLI_callback_exec(bmain, &scene->id, BLI_CB_EVT_FRAME_CHANGE_PRE);
	/* Update animated image textures for particles, modifiers, gpu, etc,
	 * call this at the start so modifiers with textures don't lag 1 frame.
	 */
	BKE_image_update_frame(bmain, scene->r.cfra);
	BKE_sound_set_cfra(scene->r.cfra);
	DEG_graph_relations_update(depsgraph, bmain, scene, view_layer);
	/* Update animated cache files for modifiers.
	 *
	 * TODO(sergey): Make this a depsgraph node?
	 */
	BKE_cachefile_update_frame(bmain, depsgraph, scene, ctime,
	                           (((double)scene->r.frs_sec) / (double)scene->r.frs_sec_base));
#ifdef POSE_ANIMATION_WORKAROUND
	scene_armature_depsgraph_workaround(bmain, depsgraph);
#endif
	/* Update all objects: drivers, matrices, displists, etc. flags set
	 * by depgraph or manual, no layer check here, gets correct flushed.
	 */
	DEG_evaluate_on_framechange(bmain, depsgraph, ctime);
	/* Update sound system animation (TODO, move to depsgraph). */
	BKE_sound_update_scene(bmain, scene);
	/* Notify editors and python about recalc. */
	BLI_callback_exec(bmain, &scene->id, BLI_CB_EVT_FRAME_CHANGE_POST);
	/* Inform editors about possible changes. */
	DEG_ids_check_recalc(bmain, depsgraph, scene, view_layer, true);
	/* clear recalc flags */
	DEG_ids_clear_recalc(bmain, depsgraph);
}

/* return default view */
SceneRenderView *BKE_scene_add_render_view(Scene *sce, const char *name)
{
	SceneRenderView *srv;

	if (!name)
		name = DATA_("RenderView");

	srv = MEM_callocN(sizeof(SceneRenderView), "new render view");
	BLI_strncpy(srv->name, name, sizeof(srv->name));
	BLI_uniquename(&sce->r.views, srv, DATA_("RenderView"), '.', offsetof(SceneRenderView, name), sizeof(srv->name));
	BLI_addtail(&sce->r.views, srv);

	return srv;
}

bool BKE_scene_remove_render_view(Scene *scene, SceneRenderView *srv)
{
	const int act = BLI_findindex(&scene->r.views, srv);

	if (act == -1) {
		return false;
	}
	else if (scene->r.views.first == scene->r.views.last) {
		/* ensure 1 view is kept */
		return false;
	}

	BLI_remlink(&scene->r.views, srv);
	MEM_freeN(srv);

	scene->r.actview = 0;

	return true;
}

/* render simplification */

int get_render_subsurf_level(const RenderData *r, int lvl, bool for_render)
{
	if (r->mode & R_SIMPLIFY) {
		if (for_render)
			return min_ii(r->simplify_subsurf_render, lvl);
		else
			return min_ii(r->simplify_subsurf, lvl);
	}
	else {
		return lvl;
	}
}

int get_render_child_particle_number(const RenderData *r, int num, bool for_render)
{
	if (r->mode & R_SIMPLIFY) {
		if (for_render)
			return (int)(r->simplify_particles_render * num);
		else
			return (int)(r->simplify_particles * num);
	}
	else {
		return num;
	}
}

/**
  * Helper function for the SETLOOPER and SETLOOPER_VIEW_LAYER macros
  *
  * It iterates over the bases of the active layer and then the bases
  * of the active layer of the background (set) scenes recursively.
  */
Base *_setlooper_base_step(Scene **sce_iter, ViewLayer *view_layer, Base *base)
{
	if (base && base->next) {
		/* Common case, step to the next. */
		return base->next;
	}
	else if ((base == NULL) && (view_layer != NULL)) {
		/* First time looping, return the scenes first base. */
		/* For the first loop we should get the layer from workspace when available. */
		if (view_layer->object_bases.first) {
			return (Base *)view_layer->object_bases.first;
		}
		/* No base on this scene layer. */
		goto next_set;
	}
	else {
next_set:
		/* Reached the end, get the next base in the set. */
		while ((*sce_iter = (*sce_iter)->set)) {
			ViewLayer *view_layer_set = BKE_view_layer_default_render((*sce_iter));
			base = (Base *)view_layer_set->object_bases.first;

			if (base) {
				return base;
			}
		}
	}

	return NULL;
}

bool BKE_scene_use_shading_nodes_custom(Scene *scene)
{
	RenderEngineType *type = RE_engines_find(scene->r.engine);
	return (type && type->flag & RE_USE_SHADING_NODES_CUSTOM);
}

bool BKE_scene_use_spherical_stereo(Scene *scene)
{
	RenderEngineType *type = RE_engines_find(scene->r.engine);
	return (type && type->flag & RE_USE_SPHERICAL_STEREO);
}

bool BKE_scene_uses_blender_eevee(const Scene *scene)
{
	return STREQ(scene->r.engine, RE_engine_id_BLENDER_EEVEE);
}

bool BKE_scene_uses_blender_opengl(const Scene *scene)
{
	return STREQ(scene->r.engine, RE_engine_id_BLENDER_OPENGL);
}

bool BKE_scene_uses_cycles(const Scene *scene)
{
	return STREQ(scene->r.engine, RE_engine_id_CYCLES);
}

void BKE_scene_base_flag_to_objects(ViewLayer *view_layer)
{
	Base *base = view_layer->object_bases.first;

	while (base) {
		BKE_scene_object_base_flag_sync_from_base(base);
		base = base->next;
	}
}

void BKE_scene_object_base_flag_sync_from_base(Base *base)
{
	Object *ob = base->object;

	ob->flag = base->flag;

	if ((base->flag & BASE_SELECTED) != 0) {
		ob->flag |= SELECT;
	}
	else {
		ob->flag &= ~SELECT;
	}
}

void BKE_scene_object_base_flag_sync_from_object(Base *base)
{
	Object *ob = base->object;
	base->flag = ob->flag;

	if ((ob->flag & SELECT) != 0) {
		base->flag |= BASE_SELECTED;
		BLI_assert((base->flag & BASE_SELECTABLE) != 0);
	}
	else {
		base->flag &= ~BASE_SELECTED;
	}
}

void BKE_scene_disable_color_management(Scene *scene)
{
	ColorManagedDisplaySettings *display_settings = &scene->display_settings;
	ColorManagedViewSettings *view_settings = &scene->view_settings;
	const char *view;
	const char *none_display_name;

	none_display_name = IMB_colormanagement_display_get_none_name();

	BLI_strncpy(display_settings->display_device, none_display_name, sizeof(display_settings->display_device));

	view = IMB_colormanagement_view_get_default_name(display_settings->display_device);

	if (view) {
		BLI_strncpy(view_settings->view_transform, view, sizeof(view_settings->view_transform));
	}
}

bool BKE_scene_check_color_management_enabled(const Scene *scene)
{
	return !STREQ(scene->display_settings.display_device, "None");
}

bool BKE_scene_check_rigidbody_active(const Scene *scene)
{
	return scene && scene->rigidbody_world && scene->rigidbody_world->group && !(scene->rigidbody_world->flag & RBW_FLAG_MUTED);
}

int BKE_render_num_threads(const RenderData *rd)
{
	int threads;

	/* override set from command line? */
	threads = BLI_system_num_threads_override_get();

	if (threads > 0)
		return threads;

	/* fixed number of threads specified in scene? */
	if (rd->mode & R_FIXED_THREADS)
		threads = rd->threads;
	else
		threads = BLI_system_thread_count();

	return max_ii(threads, 1);
}

int BKE_scene_num_threads(const Scene *scene)
{
	return BKE_render_num_threads(&scene->r);
}

int BKE_render_preview_pixel_size(const RenderData *r)
{
	if (r->preview_pixel_size == 0) {
		return (U.pixelsize > 1.5f) ? 2 : 1;
	}
	return r->preview_pixel_size;
}

/* Apply the needed correction factor to value, based on unit_type (only length-related are affected currently)
 * and unit->scale_length.
 */
double BKE_scene_unit_scale(const UnitSettings *unit, const int unit_type, double value)
{
	if (unit->system == USER_UNIT_NONE) {
		/* Never apply scale_length when not using a unit setting! */
		return value;
	}

	switch (unit_type) {
		case B_UNIT_LENGTH:
			return value * (double)unit->scale_length;
		case B_UNIT_AREA:
			return value * pow(unit->scale_length, 2);
		case B_UNIT_VOLUME:
			return value * pow(unit->scale_length, 3);
		case B_UNIT_MASS:
			return value * pow(unit->scale_length, 3);
		case B_UNIT_CAMERA:  /* *Do not* use scene's unit scale for camera focal lens! See T42026. */
		default:
			return value;
	}
}

/******************** multiview *************************/

int BKE_scene_multiview_num_views_get(const RenderData *rd)
{
	SceneRenderView *srv;
	int totviews = 0;

	if ((rd->scemode & R_MULTIVIEW) == 0)
		return 1;

	if (rd->views_format == SCE_VIEWS_FORMAT_STEREO_3D) {
		srv = BLI_findstring(&rd->views, STEREO_LEFT_NAME, offsetof(SceneRenderView, name));
		if ((srv && srv->viewflag & SCE_VIEW_DISABLE) == 0) {
			totviews++;
		}

		srv = BLI_findstring(&rd->views, STEREO_RIGHT_NAME, offsetof(SceneRenderView, name));
		if ((srv && srv->viewflag & SCE_VIEW_DISABLE) == 0) {
			totviews++;
		}
	}
	else {
		for (srv = rd->views.first; srv; srv = srv->next) {
			if ((srv->viewflag & SCE_VIEW_DISABLE) == 0) {
				totviews++;
			}
		}
	}
	return totviews;
}

bool BKE_scene_multiview_is_stereo3d(const RenderData *rd)
{
	SceneRenderView *srv[2];

	if ((rd->scemode & R_MULTIVIEW) == 0)
		return false;

	srv[0] = (SceneRenderView *)BLI_findstring(&rd->views, STEREO_LEFT_NAME, offsetof(SceneRenderView, name));
	srv[1] = (SceneRenderView *)BLI_findstring(&rd->views, STEREO_RIGHT_NAME, offsetof(SceneRenderView, name));

	return (srv[0] && ((srv[0]->viewflag & SCE_VIEW_DISABLE) == 0) &&
	        srv[1] && ((srv[1]->viewflag & SCE_VIEW_DISABLE) == 0));
}

/* return whether to render this SceneRenderView */
bool BKE_scene_multiview_is_render_view_active(const RenderData *rd, const SceneRenderView *srv)
{
	if (srv == NULL)
		return false;

	if ((rd->scemode & R_MULTIVIEW) == 0)
		return false;

	if ((srv->viewflag & SCE_VIEW_DISABLE))
		return false;

	if (rd->views_format == SCE_VIEWS_FORMAT_MULTIVIEW)
		return true;

	/* SCE_VIEWS_SETUP_BASIC */
	if (STREQ(srv->name, STEREO_LEFT_NAME) ||
	    STREQ(srv->name, STEREO_RIGHT_NAME))
	{
		return true;
	}

	return false;
}

/* return true if viewname is the first or if the name is NULL or not found */
bool BKE_scene_multiview_is_render_view_first(const RenderData *rd, const char *viewname)
{
	SceneRenderView *srv;

	if ((rd->scemode & R_MULTIVIEW) == 0)
		return true;

	if ((!viewname) || (!viewname[0]))
		return true;

	for (srv = rd->views.first; srv; srv = srv->next) {
		if (BKE_scene_multiview_is_render_view_active(rd, srv)) {
			return STREQ(viewname, srv->name);
		}
	}

	return true;
}

/* return true if viewname is the last or if the name is NULL or not found */
bool BKE_scene_multiview_is_render_view_last(const RenderData *rd, const char *viewname)
{
	SceneRenderView *srv;

	if ((rd->scemode & R_MULTIVIEW) == 0)
		return true;

	if ((!viewname) || (!viewname[0]))
		return true;

	for (srv = rd->views.last; srv; srv = srv->prev) {
		if (BKE_scene_multiview_is_render_view_active(rd, srv)) {
			return STREQ(viewname, srv->name);
		}
	}

	return true;
}

SceneRenderView *BKE_scene_multiview_render_view_findindex(const RenderData *rd, const int view_id)
{
	SceneRenderView *srv;
	size_t nr;

	if ((rd->scemode & R_MULTIVIEW) == 0)
		return NULL;

	for (srv = rd->views.first, nr = 0; srv; srv = srv->next) {
		if (BKE_scene_multiview_is_render_view_active(rd, srv)) {
			if (nr++ == view_id)
				return srv;
		}
	}
	return srv;
}

const char *BKE_scene_multiview_render_view_name_get(const RenderData *rd, const int view_id)
{
	SceneRenderView *srv = BKE_scene_multiview_render_view_findindex(rd, view_id);

	if (srv)
		return srv->name;
	else
		return "";
}

int BKE_scene_multiview_view_id_get(const RenderData *rd, const char *viewname)
{
	SceneRenderView *srv;
	size_t nr;

	if ((!rd) || ((rd->scemode & R_MULTIVIEW) == 0))
		return 0;

	if ((!viewname) || (!viewname[0]))
		return 0;

	for (srv = rd->views.first, nr = 0; srv; srv = srv->next) {
		if (BKE_scene_multiview_is_render_view_active(rd, srv)) {
			if (STREQ(viewname, srv->name)) {
				return nr;
			}
			else {
				nr += 1;
			}
		}
	}

	return 0;
}

void BKE_scene_multiview_filepath_get(
        SceneRenderView *srv, const char *filepath,
        char *r_filepath)
{
	BLI_strncpy(r_filepath, filepath, FILE_MAX);
	BLI_path_suffix(r_filepath, FILE_MAX, srv->suffix, "");
}

/**
 * When multiview is not used the filepath is as usual (e.g., ``Image.jpg``).
 * When multiview is on, even if only one view is enabled the view is incorporated
 * into the file name (e.g., ``Image_L.jpg``). That allows for the user to re-render
 * individual views.
 */
void BKE_scene_multiview_view_filepath_get(
        const RenderData *rd, const char *filepath, const char *viewname,
        char *r_filepath)
{
	SceneRenderView *srv;
	char suffix[FILE_MAX];

	srv = BLI_findstring(&rd->views, viewname, offsetof(SceneRenderView, name));
	if (srv)
		BLI_strncpy(suffix, srv->suffix, sizeof(suffix));
	else
		BLI_strncpy(suffix, viewname, sizeof(suffix));

	BLI_strncpy(r_filepath, filepath, FILE_MAX);
	BLI_path_suffix(r_filepath, FILE_MAX, suffix, "");
}

const char *BKE_scene_multiview_view_suffix_get(const RenderData *rd, const char *viewname)
{
	SceneRenderView *srv;

	if ((viewname == NULL) || (viewname[0] == '\0'))
		return viewname;

	srv = BLI_findstring(&rd->views, viewname, offsetof(SceneRenderView, name));
	if (srv)
		return srv->suffix;
	else
		return viewname;
}

const char *BKE_scene_multiview_view_id_suffix_get(const RenderData *rd, const int view_id)
{
	if ((rd->scemode & R_MULTIVIEW) == 0) {
		return "";
	}
	else {
		const char *viewname = BKE_scene_multiview_render_view_name_get(rd, view_id);
		return BKE_scene_multiview_view_suffix_get(rd, viewname);
	}
}

void BKE_scene_multiview_view_prefix_get(Scene *scene, const char *name, char *rprefix, const char **rext)
{
	SceneRenderView *srv;
	size_t index_act;
	const char *suf_act;
	const char delims[] = {'.', '\0'};

	rprefix[0] = '\0';

	/* begin of extension */
	index_act = BLI_str_rpartition(name, delims, rext, &suf_act);
	if (*rext == NULL)
		return;
	BLI_assert(index_act > 0);
	UNUSED_VARS_NDEBUG(index_act);

	for (srv = scene->r.views.first; srv; srv = srv->next) {
		if (BKE_scene_multiview_is_render_view_active(&scene->r, srv)) {
			size_t len = strlen(srv->suffix);
			if (strlen(*rext) >= len && STREQLEN(*rext - len, srv->suffix, len)) {
				BLI_strncpy(rprefix, name, strlen(name) - strlen(*rext) - len + 1);
				break;
			}
		}
	}
}

void BKE_scene_multiview_videos_dimensions_get(
        const RenderData *rd, const size_t width, const size_t height,
        size_t *r_width, size_t *r_height)
{
	if ((rd->scemode & R_MULTIVIEW) &&
	    rd->im_format.views_format == R_IMF_VIEWS_STEREO_3D)
	{
		IMB_stereo3d_write_dimensions(
		        rd->im_format.stereo3d_format.display_mode,
		        (rd->im_format.stereo3d_format.flag & S3D_SQUEEZED_FRAME) != 0,
		        width, height,
		        r_width, r_height);
	}
	else {
		*r_width = width;
		*r_height = height;
	}
}

int BKE_scene_multiview_num_videos_get(const RenderData *rd)
{
	if (BKE_imtype_is_movie(rd->im_format.imtype) == false)
		return 0;

	if ((rd->scemode & R_MULTIVIEW) == 0)
		return 1;

	if (rd->im_format.views_format == R_IMF_VIEWS_STEREO_3D) {
		return 1;
	}
	else {
		/* R_IMF_VIEWS_INDIVIDUAL */
		return BKE_scene_multiview_num_views_get(rd);
	}
}

/* Manipulation of depsgraph storage. */

/* This is a key which identifies depsgraph. */
typedef struct DepsgraphKey {
	ViewLayer *view_layer;
	/* TODO(sergey): Need to include window somehow (same layer might be in a
	 * different states in different windows).
	 */
} DepsgraphKey;

static unsigned int depsgraph_key_hash(const void *key_v)
{
	const DepsgraphKey *key = key_v;
	unsigned int hash = BLI_ghashutil_ptrhash(key->view_layer);
	/* TODO(sergey): Include hash from other fields in the key. */
	return hash;
}

static bool depsgraph_key_compare(const void *key_a_v, const void *key_b_v)
{
	const DepsgraphKey *key_a = key_a_v;
	const DepsgraphKey *key_b = key_b_v;
	/* TODO(sergey): Compare rest of  */
	return !(key_a->view_layer == key_b->view_layer);
}

static void depsgraph_key_free(void *key_v)
{
	DepsgraphKey *key = key_v;
	MEM_freeN(key);
}

static void depsgraph_key_value_free(void *value)
{
	Depsgraph *depsgraph = value;
	DEG_graph_free(depsgraph);
}

void BKE_scene_allocate_depsgraph_hash(Scene *scene)
{
	scene->depsgraph_hash = BLI_ghash_new(depsgraph_key_hash,
	                                      depsgraph_key_compare,
	                                      "Scene Depsgraph Hash");
}

void BKE_scene_ensure_depsgraph_hash(Scene *scene)
{
	if (scene->depsgraph_hash == NULL) {
		BKE_scene_allocate_depsgraph_hash(scene);
	}
}

void BKE_scene_free_depsgraph_hash(Scene *scene)
{
	if (scene->depsgraph_hash == NULL) {
		return;
	}
	BLI_ghash_free(scene->depsgraph_hash,
	               depsgraph_key_free,
	               depsgraph_key_value_free);
}

/* Query depsgraph for a specific contexts. */

Depsgraph *BKE_scene_get_depsgraph(Scene *scene,
                                   ViewLayer *view_layer,
                                   bool allocate)
{
	BLI_assert(scene != NULL);
	BLI_assert(view_layer != NULL);
	/* Make sure hash itself exists. */
	if (allocate) {
		BKE_scene_ensure_depsgraph_hash(scene);
	}
	if (scene->depsgraph_hash == NULL) {
		return NULL;
	}
	/* Either ensure item is in the hash or simply return NULL if it's not,
	 * depending on whether caller wants us to create depsgraph or not.
	 */
	DepsgraphKey key;
	key.view_layer = view_layer;
	Depsgraph *depsgraph;
	if (allocate) {
		DepsgraphKey **key_ptr;
		Depsgraph **depsgraph_ptr;
		if (!BLI_ghash_ensure_p_ex(scene->depsgraph_hash,
		                           &key,
		                           (void ***)&key_ptr,
		                           (void ***)&depsgraph_ptr))
		{
			*key_ptr = MEM_mallocN(sizeof(DepsgraphKey), __func__);
			**key_ptr = key;
			*depsgraph_ptr = DEG_graph_new(scene, view_layer, DAG_EVAL_VIEWPORT);
			/* TODO(sergey): Would be cool to avoid string format print,
			 * but is a bit tricky because we can't know in advance  whether
			 * we will ever enable debug messages for this depsgraph.
			 */
			char name[1024];
			BLI_snprintf(name, sizeof(name), "%s :: %s", scene->id.name, view_layer->name);
			DEG_debug_name_set(*depsgraph_ptr, name);
		}
		depsgraph = *depsgraph_ptr;
	}
	else {
		depsgraph = BLI_ghash_lookup(scene->depsgraph_hash, &key);
	}
	return depsgraph;
}

/* -------------------------------------------------------------------- */
/** \name Scene Orientation
 * \{ */

void BKE_scene_transform_orientation_remove(
        Scene *scene, TransformOrientation *orientation)
{
	const int orientation_index = BKE_scene_transform_orientation_get_index(scene, orientation);
	if (scene->orientation_index_custom == orientation_index) {
		/* could also use orientation_index-- */
		scene->orientation_type = V3D_MANIP_GLOBAL;
		scene->orientation_index_custom = -1;
	}
	BLI_freelinkN(&scene->transform_spaces, orientation);
}

TransformOrientation *BKE_scene_transform_orientation_find(
        const Scene *scene, const int index)
{
	return BLI_findlink(&scene->transform_spaces, index);
}

/**
 * \return the index that \a orientation has within \a scene's transform-orientation list or -1 if not found.
 */
int BKE_scene_transform_orientation_get_index(
        const Scene *scene, const TransformOrientation *orientation)
{
	return BLI_findindex(&scene->transform_spaces, orientation);
}

/** \} */
