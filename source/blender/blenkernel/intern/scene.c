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
#include "BKE_colortools.h"
#include "BKE_depsgraph.h"
#include "BKE_editmesh.h"
#include "BKE_fcurve.h"
#include "BKE_freestyle.h"
#include "BKE_global.h"
#include "BKE_gpencil.h"
#include "BKE_group.h"
#include "BKE_icons.h"
#include "BKE_idprop.h"
#include "BKE_image.h"
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
#include "BKE_world.h"

#include "DEG_depsgraph.h"

#include "RE_engine.h"

#include "PIL_time.h"

#include "IMB_colormanagement.h"
#include "IMB_imbuf.h"

#include "bmesh.h"

const char *RE_engine_id_BLENDER_RENDER = "BLENDER_RENDER";
const char *RE_engine_id_BLENDER_GAME = "BLENDER_GAME";
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
	sce_dst->theDag = NULL;
	sce_dst->depsgraph = NULL;
	sce_dst->obedit = NULL;
	sce_dst->stats = NULL;
	sce_dst->fps_info = NULL;

	BLI_duplicatelist(&(sce_dst->base), &(sce_src->base));
	for (Base *base_dst = sce_dst->base.first, *base_src = sce_src->base.first;
	     base_dst;
	     base_dst = base_dst->next, base_src = base_src->next)
	{
		if (base_src == sce_src->basact) {
			sce_dst->basact = base_dst;
		}
	}

	BLI_duplicatelist(&(sce_dst->markers), &(sce_src->markers));
	BLI_duplicatelist(&(sce_dst->transform_spaces), &(sce_src->transform_spaces));
	BLI_duplicatelist(&(sce_dst->r.layers), &(sce_src->r.layers));
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

	/* copy Freestyle settings */
	for (SceneRenderLayer *srl_dst = sce_dst->r.layers.first, *srl_src = sce_src->r.layers.first;
	     srl_src;
	     srl_dst = srl_dst->next, srl_src = srl_src->next)
	{
		if (srl_dst->prop != NULL) {
			srl_dst->prop = IDP_CopyProperty_ex(srl_dst->prop, flag_subdata);
		}
		BKE_freestyle_config_copy(&srl_dst->freestyleConfig, &srl_src->freestyleConfig, flag_subdata);
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
}

Scene *BKE_scene_copy(Main *bmain, Scene *sce, int type)
{
	Scene *sce_copy;

	/* TODO this should/could most likely be replaced by call to more generic code at some point...
	 * But for now, let's keep it well isolated here. */
	if (type == SCE_COPY_EMPTY) {
		ListBase rl, rv;

		sce_copy = BKE_scene_add(bmain, sce->id.name + 2);

		rl = sce_copy->r.layers;
		rv = sce_copy->r.views;
		curvemapping_free_data(&sce_copy->r.mblur_shutter_curve);
		sce_copy->r = sce->r;
		sce_copy->r.layers = rl;
		sce_copy->r.actlay = 0;
		sce_copy->r.views = rv;
		sce_copy->unit = sce->unit;
		sce_copy->physics_settings = sce->physics_settings;
		sce_copy->gm = sce->gm;
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
			for (SceneRenderLayer *srl_dst = sce_copy->r.layers.first; srl_dst; srl_dst = srl_dst->next) {
				for (FreestyleLineSet *lineset = srl_dst->freestyleConfig.linesets.first; lineset; lineset = lineset->next) {
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
void BKE_scene_free(Scene *sce)
{
	SceneRenderLayer *srl;

	BKE_animdata_free((ID *)sce, false);

	sce->basact = NULL;
	BLI_freelistN(&sce->base);
	BKE_sequencer_editing_free(sce, false);

	BKE_keyingsets_free(&sce->keyingsets);

	/* is no lib link block, but scene extension */
	if (sce->nodetree) {
		ntreeFreeTree(sce->nodetree);
		MEM_freeN(sce->nodetree);
		sce->nodetree = NULL;
	}

	if (sce->rigidbody_world) {
		BKE_rigidbody_free_world(sce->rigidbody_world);
		sce->rigidbody_world = NULL;
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

	for (srl = sce->r.layers.first; srl; srl = srl->next) {
		if (srl->prop != NULL) {
			IDP_FreeProperty(srl->prop);
			MEM_freeN(srl->prop);
		}
		BKE_freestyle_config_free(&srl->freestyleConfig);
	}

	BLI_freelistN(&sce->markers);
	BLI_freelistN(&sce->transform_spaces);
	BLI_freelistN(&sce->r.layers);
	BLI_freelistN(&sce->r.views);

	BKE_toolsettings_free(sce->toolsettings);
	sce->toolsettings = NULL;

	DAG_scene_free(sce);
	if (sce->depsgraph)
		DEG_graph_free(sce->depsgraph);

	MEM_SAFE_FREE(sce->stats);
	MEM_SAFE_FREE(sce->fps_info);

	BKE_sound_destroy_scene(sce);

	BKE_color_managed_view_settings_free(&sce->view_settings);

	BKE_previewimg_free(&sce->preview);
	curvemapping_free_data(&sce->r.mblur_shutter_curve);
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

	sce->r.mode = R_GAMMA | R_OSA | R_SHADOW | R_SSS | R_ENVMAP | R_RAYTRACE;
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
	sce->r.mblur_samples = 1;
	sce->r.filtertype = R_FILTER_MITCH;
	sce->r.size = 50;

	sce->r.im_format.planes = R_IMF_PLANES_RGBA;
	sce->r.im_format.imtype = R_IMF_IMTYPE_PNG;
	sce->r.im_format.depth = R_IMF_CHAN_DEPTH_8;
	sce->r.im_format.quality = 90;
	sce->r.im_format.compress = 15;

	sce->r.displaymode = R_OUTPUT_AREA;
	sce->r.framapto = 100;
	sce->r.images = 100;
	sce->r.framelen = 1.0;
	sce->r.blurfac = 0.5;
	sce->r.frs_sec = 24;
	sce->r.frs_sec_base = 1;
	sce->r.edgeint = 10;
	sce->r.ocres = 128;

	/* OCIO_TODO: for forwards compatibility only, so if no tonecurve are used,
	 *            images would look in the same way as in current blender
	 *
	 *            perhaps at some point should be completely deprecated?
	 */
	sce->r.color_mgt_flag |= R_COLOR_MANAGEMENT;

	sce->r.gauss = 1.0;

	/* deprecated but keep for upwards compat */
	sce->r.postgamma = 1.0;
	sce->r.posthue = 0.0;
	sce->r.postsat = 1.0;

	sce->r.bake_mode = 1;    /* prevent to include render stuff here */
	sce->r.bake_filter = 16;
	sce->r.bake_osa = 5;
	sce->r.bake_flag = R_BAKE_CLEAR;
	sce->r.bake_normal_space = R_BAKE_SPACE_TANGENT;
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
	sce->r.raytrace_options = R_RAYTRACE_USE_INSTANCES;

	sce->r.seq_prev_type = OB_SOLID;
	sce->r.seq_rend_type = OB_SOLID;
	sce->r.seq_flag = 0;

	sce->r.threads = 1;

	sce->r.simplify_subsurf = 6;
	sce->r.simplify_particles = 1.0f;
	sce->r.simplify_shadowsamples = 16;
	sce->r.simplify_aosss = 1.0f;

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
	sce->toolsettings->doublimit = 0.001;
	sce->toolsettings->vgroup_weight = 1.0f;
	sce->toolsettings->uvcalc_margin = 0.001f;
	sce->toolsettings->unwrapper = 1;
	sce->toolsettings->select_thresh = 0.01f;

	sce->toolsettings->selectmode = SCE_SELECT_VERTEX;
	sce->toolsettings->uv_selectmode = UV_SELECT_VERTEX;
	sce->toolsettings->normalsize = 0.1;
	sce->toolsettings->autokey_mode = U.autokey_mode;

	sce->toolsettings->snap_node_mode = SCE_SNAP_MODE_GRID;

	sce->toolsettings->skgen_resolution = 100;
	sce->toolsettings->skgen_threshold_internal     = 0.01f;
	sce->toolsettings->skgen_threshold_external     = 0.01f;
	sce->toolsettings->skgen_angle_limit            = 45.0f;
	sce->toolsettings->skgen_length_ratio           = 1.3f;
	sce->toolsettings->skgen_length_limit           = 1.5f;
	sce->toolsettings->skgen_correlation_limit      = 0.98f;
	sce->toolsettings->skgen_symmetry_limit         = 0.1f;
	sce->toolsettings->skgen_postpro = SKGEN_SMOOTH;
	sce->toolsettings->skgen_postpro_passes = 1;
	sce->toolsettings->skgen_options = SKGEN_FILTER_INTERNAL | SKGEN_FILTER_EXTERNAL | SKGEN_FILTER_SMART | SKGEN_HARMONIC | SKGEN_SUB_CORRELATION | SKGEN_STICK_TO_EMBEDDING;
	sce->toolsettings->skgen_subdivisions[0] = SKGEN_SUB_CORRELATION;
	sce->toolsettings->skgen_subdivisions[1] = SKGEN_SUB_LENGTH;
	sce->toolsettings->skgen_subdivisions[2] = SKGEN_SUB_ANGLE;

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

	BLI_strncpy(sce->r.engine, RE_engine_id_BLENDER_RENDER, sizeof(sce->r.engine));

	sce->audio.distance_model = 2.0f;
	sce->audio.doppler_factor = 1.0f;
	sce->audio.speed_of_sound = 343.3f;
	sce->audio.volume = 1.0f;

	BLI_strncpy(sce->r.pic, U.renderdir, sizeof(sce->r.pic));

	BLI_rctf_init(&sce->r.safety, 0.1f, 0.9f, 0.1f, 0.9f);
	sce->r.osa = 8;

	/* note; in header_info.c the scene copy happens..., if you add more to renderdata it has to be checked there */
	BKE_scene_add_render_layer(sce, NULL);

	/* multiview - stereo */
	BKE_scene_add_render_view(sce, STEREO_LEFT_NAME);
	srv = sce->r.views.first;
	BLI_strncpy(srv->suffix, STEREO_LEFT_SUFFIX, sizeof(srv->suffix));

	BKE_scene_add_render_view(sce, STEREO_RIGHT_NAME);
	srv = sce->r.views.last;
	BLI_strncpy(srv->suffix, STEREO_RIGHT_SUFFIX, sizeof(srv->suffix));

	/* game data */
	sce->gm.stereoflag = STEREO_NOSTEREO;
	sce->gm.stereomode = STEREO_ANAGLYPH;
	sce->gm.eyeseparation = 0.10;

	sce->gm.dome.angle = 180;
	sce->gm.dome.mode = DOME_FISHEYE;
	sce->gm.dome.res = 4;
	sce->gm.dome.resbuf = 1.0f;
	sce->gm.dome.tilt = 0;

	sce->gm.xplay = 640;
	sce->gm.yplay = 480;
	sce->gm.freqplay = 60;
	sce->gm.depth = 32;

	sce->gm.gravity = 9.8f;
	sce->gm.physicsEngine = WOPHY_BULLET;
	sce->gm.mode = 32; //XXX ugly harcoding, still not sure we should drop mode. 32 == 1 << 5 == use_occlusion_culling
	sce->gm.occlusionRes = 128;
	sce->gm.ticrate = 60;
	sce->gm.maxlogicstep = 5;
	sce->gm.physubstep = 1;
	sce->gm.maxphystep = 5;
	sce->gm.lineardeactthreshold = 0.8f;
	sce->gm.angulardeactthreshold = 1.0f;
	sce->gm.deactivationtime = 0.0f;

	sce->gm.flag = GAME_DISPLAY_LISTS;
	sce->gm.matmode = GAME_MAT_MULTITEX;

	sce->gm.obstacleSimulation = OBSTSIMULATION_NONE;
	sce->gm.levelHeight = 2.f;

	sce->gm.recastData.cellsize = 0.3f;
	sce->gm.recastData.cellheight = 0.2f;
	sce->gm.recastData.agentmaxslope = M_PI_4;
	sce->gm.recastData.agentmaxclimb = 0.9f;
	sce->gm.recastData.agentheight = 2.0f;
	sce->gm.recastData.agentradius = 0.6f;
	sce->gm.recastData.edgemaxlen = 12.0f;
	sce->gm.recastData.edgemaxerror = 1.3f;
	sce->gm.recastData.regionminsize = 8.f;
	sce->gm.recastData.regionmergesize = 20.f;
	sce->gm.recastData.vertsperpoly = 6;
	sce->gm.recastData.detailsampledist = 6.0f;
	sce->gm.recastData.detailsamplemaxerror = 1.0f;

	sce->gm.lodflag = SCE_LOD_USE_HYST;
	sce->gm.scehysteresis = 10;

	sce->gm.exitkey = 218; // Blender key code for ESC

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

Base *BKE_scene_base_find_by_name(struct Scene *scene, const char *name)
{
	Base *base;

	for (base = scene->base.first; base; base = base->next) {
		if (STREQ(base->object->id.name + 2, name)) {
			break;
		}
	}

	return base;
}

Base *BKE_scene_base_find(Scene *scene, Object *ob)
{
	return BLI_findptr(&scene->base, ob, offsetof(Base, object));
}

/**
 * Sets the active scene, mainly used when running in background mode (``--scene`` command line argument).
 * This is also called to set the scene directly, bypassing windowing code.
 * Otherwise #ED_screen_set_scene is used when changing scenes by the user.
 */
void BKE_scene_set_background(Main *bmain, Scene *scene)
{
	Scene *sce;
	Base *base;
	Object *ob;
	Group *group;
	GroupObject *go;
	int flag;

	/* check for cyclic sets, for reading old files but also for definite security (py?) */
	BKE_scene_validate_setscene(bmain, scene);

	/* can happen when switching modes in other scenes */
	if (scene->obedit && !(scene->obedit->mode & OB_MODE_EDIT))
		scene->obedit = NULL;

	/* deselect objects (for dataselect) */
	for (ob = bmain->object.first; ob; ob = ob->id.next)
		ob->flag &= ~(SELECT | OB_FROMGROUP);

	/* group flags again */
	for (group = bmain->group.first; group; group = group->id.next) {
		for (go = group->gobject.first; go; go = go->next) {
			if (go->ob) {
				go->ob->flag |= OB_FROMGROUP;
			}
		}
	}

	/* sort baselist for scene and sets */
	for (sce = scene; sce; sce = sce->set)
		DAG_scene_relations_rebuild(bmain, sce);

	/* copy layers and flags from bases to objects */
	for (base = scene->base.first; base; base = base->next) {
		ob = base->object;
		ob->lay = base->lay;

		/* group patch... */
		base->flag &= ~(OB_FROMGROUP);
		flag = ob->flag & (OB_FROMGROUP);
		base->flag |= flag;

		/* not too nice... for recovering objects with lost data */
		//if (ob->pose == NULL) base->flag &= ~OB_POSEMODE;
		ob->flag = base->flag;
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
int BKE_scene_base_iter_next(Main *bmain, EvaluationContext *eval_ctx, SceneBaseIter *iter,
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
				*base = (*scene)->base.first;
				if (*base) {
					*ob = (*base)->object;
					iter->phase = F_SCENE;
				}
				else {
					/* exception: empty scene */
					while ((*scene)->set) {
						(*scene) = (*scene)->set;
						if ((*scene)->base.first) {
							*base = (*scene)->base.first;
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
								if ((*scene)->base.first) {
									*base = (*scene)->base.first;
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
					if ( (*base)->object->transflag & OB_DUPLI) {
						/* groups cannot be duplicated for mballs yet,
						 * this enters eternal loop because of
						 * makeDispListMBall getting called inside of group_duplilist */
						if ((*base)->object->dup_group == NULL) {
							iter->duplilist = object_duplilist_ex(bmain, eval_ctx, (*scene), (*base)->object, false);

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
					(*base)->flag |= OB_FROMDUPLI;
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
					(*base)->flag &= ~OB_FROMDUPLI;

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

Object *BKE_scene_camera_find(Scene *sc)
{
	Base *base;

	for (base = sc->base.first; base; base = base->next)
		if (base->object->type == OB_CAMERA)
			return base->object;

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

int BKE_scene_frame_snap_by_seconds(Scene *scene, double interval_in_seconds, int cfra)
{
	const int fps = round_db_to_int(FPS * interval_in_seconds);
	const int second_prev = cfra - mod_i(cfra, fps);
	const int second_next = second_prev + fps;
	const int delta_prev = cfra - second_prev;
	const int delta_next = second_next - cfra;
	return (delta_prev < delta_next) ? second_prev : second_next;
}


Base *BKE_scene_base_add(Scene *sce, Object *ob)
{
	Base *b = MEM_callocN(sizeof(*b), __func__);
	BLI_addhead(&sce->base, b);

	b->object = ob;
	b->flag = ob->flag;
	b->lay = ob->lay;

	return b;
}

void BKE_scene_base_unlink(Scene *sce, Base *base)
{
	/* remove rigid body constraint from world before removing object */
	if (base->object->rigidbody_constraint)
		BKE_rigidbody_remove_constraint(sce, base->object);
	/* remove rigid body object from world before removing object */
	if (base->object->rigidbody_object)
		BKE_rigidbody_remove_object(sce, base->object);

	BLI_remlink(&sce->base, base);
	if (sce->basact == base)
		sce->basact = NULL;
}

void BKE_scene_base_deselect_all(Scene *sce)
{
	Base *b;

	for (b = sce->base.first; b; b = b->next) {
		b->flag &= ~SELECT;
		b->object->flag = b->flag;
	}
}

void BKE_scene_base_select(Scene *sce, Base *selbase)
{
	selbase->flag |= SELECT;
	selbase->object->flag = selbase->flag;

	sce->basact = selbase;
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

#ifdef WITH_LEGACY_DEPSGRAPH
/* drivers support/hacks
 *  - this method is called from scene_update_tagged_recursive(), so gets included in viewport + render
 *	- these are always run since the depsgraph can't handle non-object data
 *	- these happen after objects are all done so that we can read in their final transform values,
 *	  though this means that objects can't refer to scene info for guidance...
 */
static void scene_update_drivers(Main *UNUSED(bmain), Scene *scene)
{
	SceneRenderLayer *srl;
	float ctime = BKE_scene_frame_get(scene);

	/* scene itself */
	if (scene->adt && scene->adt->drivers.first) {
		BKE_animsys_evaluate_animdata(scene, &scene->id, scene->adt, ctime, ADT_RECALC_DRIVERS);
	}

	/* world */
	/* TODO: what about world textures? but then those have nodes too... */
	if (scene->world) {
		ID *wid = (ID *)scene->world;
		AnimData *adt = BKE_animdata_from_id(wid);

		if (adt && adt->drivers.first)
			BKE_animsys_evaluate_animdata(scene, wid, adt, ctime, ADT_RECALC_DRIVERS);
	}

	/* nodes */
	if (scene->nodetree) {
		ID *nid = (ID *)scene->nodetree;
		AnimData *adt = BKE_animdata_from_id(nid);

		if (adt && adt->drivers.first)
			BKE_animsys_evaluate_animdata(scene, nid, adt, ctime, ADT_RECALC_DRIVERS);
	}

	/* world nodes */
	if (scene->world && scene->world->nodetree) {
		ID *nid = (ID *)scene->world->nodetree;
		AnimData *adt = BKE_animdata_from_id(nid);

		if (adt && adt->drivers.first)
			BKE_animsys_evaluate_animdata(scene, nid, adt, ctime, ADT_RECALC_DRIVERS);
	}

	/* freestyle */
	for (srl = scene->r.layers.first; srl; srl = srl->next) {
		FreestyleConfig *config = &srl->freestyleConfig;
		FreestyleLineSet *lineset;

		for (lineset = config->linesets.first; lineset; lineset = lineset->next) {
			if (lineset->linestyle) {
				ID *lid = &lineset->linestyle->id;
				AnimData *adt = BKE_animdata_from_id(lid);

				if (adt && adt->drivers.first)
					BKE_animsys_evaluate_animdata(scene, lid, adt, ctime, ADT_RECALC_DRIVERS);
			}
		}
	}
}

/* deps hack - do extra recalcs at end */
static void scene_depsgraph_hack(Main *bmain, EvaluationContext *eval_ctx, Scene *scene, Scene *scene_parent)
{
	Base *base;

	scene->customdata_mask = scene_parent->customdata_mask;

	/* sets first, we allow per definition current scene to have
	 * dependencies on sets, but not the other way around. */
	if (scene->set)
		scene_depsgraph_hack(bmain, eval_ctx, scene->set, scene_parent);

	for (base = scene->base.first; base; base = base->next) {
		Object *ob = base->object;

		if (ob->depsflag) {
			int recalc = 0;
			// printf("depshack %s\n", ob->id.name + 2);

			if (ob->depsflag & OB_DEPS_EXTRA_OB_RECALC)
				recalc |= OB_RECALC_OB;
			if (ob->depsflag & OB_DEPS_EXTRA_DATA_RECALC)
				recalc |= OB_RECALC_DATA;

			ob->recalc |= recalc;
			BKE_object_handle_update(bmain, eval_ctx, scene_parent, ob);

			if (ob->dup_group && (ob->transflag & OB_DUPLIGROUP)) {
				GroupObject *go;

				for (go = ob->dup_group->gobject.first; go; go = go->next) {
					if (go->ob)
						go->ob->recalc |= recalc;
				}
				BKE_group_handle_recalc_and_update(bmain, eval_ctx, scene_parent, ob, ob->dup_group);
			}
		}
	}
}
#endif  /* WITH_LEGACY_DEPSGRAPH */

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
static void scene_armature_depsgraph_workaround(Main *bmain)
{
	Object *ob;
	if (BLI_listbase_is_empty(&bmain->armature) || !DAG_id_type_tagged(bmain, ID_OB)) {
		return;
	}
	for (ob = bmain->object.first; ob; ob = ob->id.next) {
		if (ob->type == OB_ARMATURE && ob->adt && ob->adt->recalc & ADT_RECALC_ANIM) {
			if (ob->pose == NULL || (ob->pose->flag & POSE_RECALC)) {
				BKE_pose_rebuild(ob, ob->data);
			}
		}
	}
}
#endif

#ifdef WITH_LEGACY_DEPSGRAPH
static void scene_rebuild_rbw_recursive(Scene *scene, float ctime)
{
	if (scene->set)
		scene_rebuild_rbw_recursive(scene->set, ctime);

	if (BKE_scene_check_rigidbody_active(scene))
		BKE_rigidbody_rebuild_world(scene, ctime);
}

static void scene_do_rb_simulation_recursive(Scene *scene, float ctime)
{
	if (scene->set)
		scene_do_rb_simulation_recursive(scene->set, ctime);

	if (BKE_scene_check_rigidbody_active(scene))
		BKE_rigidbody_do_simulation(scene, ctime);
}
#endif

/* Used to visualize CPU threads activity during threaded object update,
 * would pollute STDERR with whole bunch of timing information which then
 * could be parsed and nicely visualized.
 */
#ifdef WITH_LEGACY_DEPSGRAPH
#  undef DETAILED_ANALYSIS_OUTPUT
#else
/* ALWAYS KEEY DISABLED! */
#  undef DETAILED_ANALYSIS_OUTPUT
#endif

/* Mballs evaluation uses BKE_scene_base_iter_next which calls
 * duplilist for all objects in the scene. This leads to conflict
 * accessing and writing same data from multiple threads.
 *
 * Ideally Mballs shouldn't do such an iteration and use DAG
 * queries instead. For the time being we've got new DAG
 * let's keep it simple and update mballs in a single thread.
 */
#define MBALL_SINGLETHREAD_HACK

#ifdef WITH_LEGACY_DEPSGRAPH
typedef struct StatisicsEntry {
	struct StatisicsEntry *next, *prev;
	Object *object;
	double start_time;
	double duration;
} StatisicsEntry;

typedef struct ThreadedObjectUpdateState {
	/* TODO(sergey): We might want this to be per-thread object. */
	EvaluationContext *eval_ctx;
	Main *bmain;
	Scene *scene;
	Scene *scene_parent;
	double base_time;

#ifdef MBALL_SINGLETHREAD_HACK
	bool has_mballs;
#endif

	int num_threads;

	/* Execution statistics */
	bool has_updated_objects;
	ListBase *statistics;
} ThreadedObjectUpdateState;

static void scene_update_object_add_task(void *node, void *user_data);

static void scene_update_all_bases(Main *bmain, EvaluationContext *eval_ctx, Scene *scene, Scene *scene_parent)
{
	Base *base;

	for (base = scene->base.first; base; base = base->next) {
		Object *object = base->object;

		BKE_object_handle_update_ex(bmain, eval_ctx, scene_parent, object, scene->rigidbody_world, true);

		if (object->dup_group && (object->transflag & OB_DUPLIGROUP))
			BKE_group_handle_recalc_and_update(bmain, eval_ctx, scene_parent, object, object->dup_group);

		/* always update layer, so that animating layers works (joshua july 2010) */
		/* XXX commented out, this has depsgraph issues anyway - and this breaks setting scenes
		 * (on scene-set, the base-lay is copied to ob-lay (ton nov 2012) */
		// base->lay = ob->lay;
	}
}

static void scene_update_object_func(TaskPool * __restrict pool, void *taskdata, int threadid)
{
/* Disable print for now in favor of summary statistics at the end of update. */
#define PRINT if (false) printf

	ThreadedObjectUpdateState *state = (ThreadedObjectUpdateState *) BLI_task_pool_userdata(pool);
	void *node = taskdata;
	Object *object = DAG_get_node_object(node);
	EvaluationContext *eval_ctx = state->eval_ctx;
	Main *bmain = state->bmain;
	Scene *scene = state->scene;
	Scene *scene_parent = state->scene_parent;

#ifdef MBALL_SINGLETHREAD_HACK
	if (object && object->type == OB_MBALL) {
		state->has_mballs = true;
	}
	else
#endif
	if (object) {
		double start_time = 0.0;
		bool add_to_stats = false;

		if (G.debug & G_DEBUG_DEPSGRAPH_EVAL) {
			if (object->recalc & OB_RECALC_ALL) {
				printf("Thread %d: update object %s\n", threadid, object->id.name);
			}

			start_time = PIL_check_seconds_timer();

			if (object->recalc & OB_RECALC_ALL) {
				state->has_updated_objects = true;
				add_to_stats = true;
			}
		}

		/* We only update object itself here, dupli-group will be updated
		 * separately from main thread because of we've got no idea about
		 * dependencies inside the group.
		 */
		BKE_object_handle_update_ex(bmain, eval_ctx, scene_parent, object, scene->rigidbody_world, false);

		/* Calculate statistics. */
		if (add_to_stats) {
			StatisicsEntry *entry;

			entry = MEM_mallocN(sizeof(StatisicsEntry), "update thread statistics");
			entry->object = object;
			entry->start_time = start_time;
			entry->duration = PIL_check_seconds_timer() - start_time;

			BLI_addtail(&state->statistics[threadid], entry);
		}
	}
	else {
		PRINT("Threda %d: update node %s\n", threadid,
		      DAG_get_node_name(scene, node));
	}

	/* Update will decrease child's valency and schedule child with zero valency. */
	DAG_threaded_update_handle_node_updated(node, scene_update_object_add_task, pool);

#undef PRINT
}

static void scene_update_object_add_task(void *node, void *user_data)
{
	TaskPool *task_pool = user_data;

	BLI_task_pool_push(task_pool, scene_update_object_func, node, false, TASK_PRIORITY_LOW);
}

static void print_threads_statistics(ThreadedObjectUpdateState *state)
{
	double finish_time;

	if ((G.debug & G_DEBUG_DEPSGRAPH_EVAL) == 0) {
		return;
	}

#ifdef DETAILED_ANALYSIS_OUTPUT
	if (state->has_updated_objects) {
		tot_thread = BLI_system_thread_count();

		fprintf(stderr, "objects update base time %f\n", state->base_time);

		for (i = 0; i < tot_thread; i++) {
			StatisicsEntry *entry;
			for (entry = state->statistics[i].first;
			     entry;
			     entry = entry->next)
			{
				fprintf(stderr, "thread %d object %s start_time %f duration %f\n",
				        i, entry->object->id.name + 2,
				        entry->start_time, entry->duration);
			}
			BLI_freelistN(&state->statistics[i]);
		}
	}
#else
	finish_time = PIL_check_seconds_timer();
	int total_objects = 0;

	for (int i = 0; i < state->num_threads; i++) {
		int thread_total_objects = 0;
		double thread_total_time = 0.0;
		StatisicsEntry *entry;

		if (state->has_updated_objects) {
			/* Don't pollute output if no objects were updated. */
			for (entry = state->statistics[i].first;
			     entry;
			     entry = entry->next)
			{
				thread_total_objects++;
				thread_total_time += entry->duration;
			}

			printf("Thread %d: total %d objects in %f sec.\n",
			       i,
			       thread_total_objects,
			       thread_total_time);

			for (entry = state->statistics[i].first;
			     entry;
			     entry = entry->next)
			{
				printf("  %s in %f sec\n", entry->object->id.name + 2, entry->duration);
			}

			total_objects += thread_total_objects;
		}

		BLI_freelistN(&state->statistics[i]);
	}
	if (state->has_updated_objects) {
		printf("Scene updated %d objects in %f sec\n",
		       total_objects,
		       finish_time - state->base_time);
	}
#endif
}

static bool scene_need_update_objects(Main *bmain)
{
	return
		/* Object datablocks themselves (for OB_RECALC_OB) */
		DAG_id_type_tagged(bmain, ID_OB) ||

		/* Objects data datablocks (for OB_RECALC_DATA) */
		DAG_id_type_tagged(bmain, ID_ME)  ||  /* Mesh */
		DAG_id_type_tagged(bmain, ID_CU)  ||  /* Curve */
		DAG_id_type_tagged(bmain, ID_MB)  ||  /* MetaBall */
		DAG_id_type_tagged(bmain, ID_LA)  ||  /* Lamp */
		DAG_id_type_tagged(bmain, ID_LT)  ||  /* Lattice */
		DAG_id_type_tagged(bmain, ID_CA)  ||  /* Camera */
		DAG_id_type_tagged(bmain, ID_KE)  ||  /* KE */
		DAG_id_type_tagged(bmain, ID_SPK) ||  /* Speaker */
		DAG_id_type_tagged(bmain, ID_AR);     /* Armature */
}

static void scene_update_objects(EvaluationContext *eval_ctx, Main *bmain, Scene *scene, Scene *scene_parent)
{
	TaskScheduler *task_scheduler;
	TaskPool *task_pool;
	ThreadedObjectUpdateState state;
	bool need_singlethread_pass;
	bool need_free_scheduler;

	/* Early check for whether we need to invoke all the task-based
	 * things (spawn new ppol, traverse dependency graph and so on).
	 *
	 * Basically if there's no ID datablocks tagged for update which
	 * corresponds to object->recalc flags (which are checked in
	 * BKE_object_handle_update() then we do nothing here.
	 */
	if (!scene_need_update_objects(bmain)) {
		return;
	}

	state.eval_ctx = eval_ctx;
	state.bmain = bmain;
	state.scene = scene;
	state.scene_parent = scene_parent;

	if (G.debug & G_DEBUG_DEPSGRAPH_NO_THREADS) {
		task_scheduler = BLI_task_scheduler_create(1);
		need_free_scheduler = true;
	}
	else {
		task_scheduler = BLI_task_scheduler_get();
		need_free_scheduler = false;
	}

	/* Those are only needed when blender is run with --debug argument. */
	if (G.debug & G_DEBUG_DEPSGRAPH_EVAL) {
		const int tot_thread = BLI_task_scheduler_num_threads(task_scheduler);
		state.statistics = MEM_callocN(tot_thread * sizeof(*state.statistics),
		                               "scene update objects stats");
		state.has_updated_objects = false;
		state.base_time = PIL_check_seconds_timer();
		state.num_threads = tot_thread;
	}

#ifdef MBALL_SINGLETHREAD_HACK
	state.has_mballs = false;
#endif

	task_pool = BLI_task_pool_create(task_scheduler, &state);

	DAG_threaded_update_begin(scene, scene_update_object_add_task, task_pool);
	BLI_task_pool_work_and_wait(task_pool);
	BLI_task_pool_free(task_pool);

	if (G.debug & G_DEBUG_DEPSGRAPH_EVAL) {
		print_threads_statistics(&state);
		MEM_freeN(state.statistics);
	}

	/* We do single thread pass to update all the objects which are in cyclic dependency.
	 * Such objects can not be handled by a generic DAG traverse and it's really tricky
	 * to detect whether cycle could be solved or not.
	 *
	 * In this situation we simply update all remaining objects in a single thread and
	 * it'll happen in the same exact order as it was in single-threaded DAG.
	 *
	 * We couldn't use threaded update for objects which are in cycle because they might
	 * access data of each other which is being re-evaluated.
	 *
	 * Also, as was explained above, for now we also update all the mballs in single thread.
	 *
	 *                                                                   - sergey -
	 */
	need_singlethread_pass = DAG_is_acyclic(scene) == false;
#ifdef MBALL_SINGLETHREAD_HACK
	need_singlethread_pass |= state.has_mballs;
#endif

	if (need_singlethread_pass) {
		scene_update_all_bases(bmain, eval_ctx, scene, scene_parent);
	}

	if (need_free_scheduler) {
		BLI_task_scheduler_free(task_scheduler);
	}
}

static void scene_update_tagged_recursive(EvaluationContext *eval_ctx, Main *bmain, Scene *scene, Scene *scene_parent)
{
	scene->customdata_mask = scene_parent->customdata_mask;

	/* sets first, we allow per definition current scene to have
	 * dependencies on sets, but not the other way around. */
	if (scene->set)
		scene_update_tagged_recursive(eval_ctx, bmain, scene->set, scene_parent);

	/* scene objects */
	scene_update_objects(eval_ctx, bmain, scene, scene_parent);

	/* scene drivers... */
	scene_update_drivers(bmain, scene);

	/* update masking curves */
	BKE_mask_update_scene(bmain, scene);

}
#endif  /* WITH_LEGACY_DEPSGRAPH */

static bool check_rendered_viewport_visible(Main *bmain)
{
	wmWindowManager *wm = bmain->wm.first;
	wmWindow *window;
	for (window = wm->windows.first; window != NULL; window = window->next) {
		bScreen *screen = window->screen;
		ScrArea *area;
		for (area = screen->areabase.first; area != NULL; area = area->next) {
			View3D *v3d = area->spacedata.first;
			if (area->spacetype != SPACE_VIEW3D) {
				continue;
			}
			if (v3d->drawtype == OB_RENDER) {
				return true;
			}
		}
	}
	return false;
}

static void prepare_mesh_for_viewport_render(Main *bmain, Scene *scene)
{
	/* This is needed to prepare mesh to be used by the render
	 * engine from the viewport rendering. We do loading here
	 * so all the objects which shares the same mesh datablock
	 * are nicely tagged for update and updated.
	 *
	 * This makes it so viewport render engine doesn't need to
	 * call loading of the edit data for the mesh objects.
	 */

	Object *obedit = scene->obedit;
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
				DAG_id_tag_update(&mesh->id, 0);
			}
		}
	}
}

void BKE_scene_update_tagged(EvaluationContext *eval_ctx, Main *bmain, Scene *scene)
{
	Scene *sce_iter;
#ifdef WITH_LEGACY_DEPSGRAPH
	bool use_new_eval = !DEG_depsgraph_use_legacy();
#endif

	/* keep this first */
	BLI_callback_exec(bmain, &scene->id, BLI_CB_EVT_SCENE_UPDATE_PRE);

	/* (re-)build dependency graph if needed */
	for (sce_iter = scene; sce_iter; sce_iter = sce_iter->set) {
		DAG_scene_relations_update(bmain, sce_iter);
		/* Uncomment this to check if dependency graph was properly tagged for update. */
#if 0
#ifdef WITH_LEGACY_DEPSGRAPH
		if (use_new_eval)
#endif
		{
			DAG_scene_relations_validate(bmain, sce_iter);
		}
#endif
	}

	/* flush editing data if needed */
	prepare_mesh_for_viewport_render(bmain, scene);

	/* flush recalc flags to dependencies */
	DAG_ids_flush_tagged(bmain);

	/* removed calls to quick_cache, see pointcache.c */

	/* clear "LIB_TAG_DOIT" flag from all materials, to prevent infinite recursion problems later
	 * when trying to find materials with drivers that need evaluating [#32017]
	 */
	BKE_main_id_tag_idcode(bmain, ID_MA, LIB_TAG_DOIT, false);
	BKE_main_id_tag_idcode(bmain, ID_LA, LIB_TAG_DOIT, false);

	/* update all objects: drivers, matrices, displists, etc. flags set
	 * by depgraph or manual, no layer check here, gets correct flushed
	 *
	 * in the future this should handle updates for all datablocks, not
	 * only objects and scenes. - brecht */
#ifdef WITH_LEGACY_DEPSGRAPH
	if (!use_new_eval) {
		scene_update_tagged_recursive(eval_ctx, bmain, scene, scene);
	}
	else
#endif
	{
		DEG_evaluate_on_refresh(eval_ctx, scene->depsgraph, scene);
	}

	/* update sound system animation (TODO, move to depsgraph) */
	BKE_sound_update_scene(bmain, scene);

	/* extra call here to recalc scene animation (for sequencer) */
	{
		AnimData *adt = BKE_animdata_from_id(&scene->id);
		float ctime = BKE_scene_frame_get(scene);

		if (adt && (adt->recalc & ADT_RECALC_ANIM))
			BKE_animsys_evaluate_animdata(scene, &scene->id, adt, ctime, 0);
	}

	/* Extra call here to recalc material animation.
	 *
	 * Need to do this so changing material settings from the graph/dopesheet
	 * will update stuff in the viewport.
	 */
#ifdef WITH_LEGACY_DEPSGRAPH
	if (!use_new_eval && DAG_id_type_tagged(bmain, ID_MA)) {
		Material *material;
		float ctime = BKE_scene_frame_get(scene);

		for (material = bmain->mat.first;
		     material;
		     material = material->id.next)
		{
			AnimData *adt = BKE_animdata_from_id(&material->id);
			if (adt && (adt->recalc & ADT_RECALC_ANIM))
				BKE_animsys_evaluate_animdata(scene, &material->id, adt, ctime, 0);
		}
	}

	/* Also do the same for node trees. */
	if (!use_new_eval && DAG_id_type_tagged(bmain, ID_NT)) {
		float ctime = BKE_scene_frame_get(scene);

		FOREACH_NODETREE(bmain, ntree, id)
		{
			AnimData *adt = BKE_animdata_from_id(&ntree->id);
			if (adt && (adt->recalc & ADT_RECALC_ANIM))
				BKE_animsys_evaluate_animdata(scene, &ntree->id, adt, ctime, 0);
		}
		FOREACH_NODETREE_END
	}
#endif

	/* notify editors and python about recalc */
	BLI_callback_exec(bmain, &scene->id, BLI_CB_EVT_SCENE_UPDATE_POST);

	/* Inform editors about possible changes. */
	DAG_ids_check_recalc(bmain, scene, false);

	/* clear recalc flags */
	DAG_ids_clear_recalc(bmain);
}

/* applies changes right away, does all sets too */
void BKE_scene_update_for_newframe(EvaluationContext *eval_ctx, Main *bmain, Scene *sce, unsigned int lay)
{
	BKE_scene_update_for_newframe_ex(eval_ctx, bmain, sce, lay, false);
}

void BKE_scene_update_for_newframe_ex(EvaluationContext *eval_ctx, Main *bmain, Scene *sce, unsigned int lay, bool do_invisible_flush)
{
	float ctime = BKE_scene_frame_get(sce);
	Scene *sce_iter;
#ifdef DETAILED_ANALYSIS_OUTPUT
	double start_time = PIL_check_seconds_timer();
#endif
#ifdef WITH_LEGACY_DEPSGRAPH
	bool use_new_eval = !DEG_depsgraph_use_legacy();
#else
	/* TODO(sergey): Pass to evaluation routines instead of storing layer in the dependency graph? */
	(void) do_invisible_flush;
#endif

	DAG_editors_update_pre(bmain, sce, true);

	/* keep this first */
	BLI_callback_exec(bmain, &sce->id, BLI_CB_EVT_FRAME_CHANGE_PRE);
	BLI_callback_exec(bmain, &sce->id, BLI_CB_EVT_SCENE_UPDATE_PRE);

	/* update animated image textures for particles, modifiers, gpu, etc,
	 * call this at the start so modifiers with textures don't lag 1 frame */
	BKE_image_update_frame(bmain, sce->r.cfra);

#ifdef WITH_LEGACY_DEPSGRAPH
	/* rebuild rigid body worlds before doing the actual frame update
	 * this needs to be done on start frame but animation playback usually starts one frame later
	 * we need to do it here to avoid rebuilding the world on every simulation change, which can be very expensive
	 */
	if (!use_new_eval) {
		scene_rebuild_rbw_recursive(sce, ctime);
	}
#endif

	BKE_sound_set_cfra(sce->r.cfra);

	/* clear animation overrides */
	/* XXX TODO... */

	for (sce_iter = sce; sce_iter; sce_iter = sce_iter->set)
		DAG_scene_relations_update(bmain, sce_iter);

#ifdef WITH_LEGACY_DEPSGRAPH
	if (!use_new_eval) {
		/* flush recalc flags to dependencies, if we were only changing a frame
		 * this would not be necessary, but if a user or a script has modified
		 * some datablock before BKE_scene_update_tagged was called, we need the flush */
		DAG_ids_flush_tagged(bmain);

		/* Following 2 functions are recursive
		 * so don't call within 'scene_update_tagged_recursive' */
		DAG_scene_update_flags(bmain, sce, lay, true, do_invisible_flush);   // only stuff that moves or needs display still
		BKE_mask_evaluate_all_masks(bmain, ctime, true);
	}
#endif

	/* Update animated cache files for modifiers. */
	BKE_cachefile_update_frame(bmain, sce, ctime, (((double)sce->r.frs_sec) / (double)sce->r.frs_sec_base));

#ifdef POSE_ANIMATION_WORKAROUND
	scene_armature_depsgraph_workaround(bmain);
#endif

	/* All 'standard' (i.e. without any dependencies) animation is handled here,
	 * with an 'local' to 'macro' order of evaluation. This should ensure that
	 * settings stored nestled within a hierarchy (i.e. settings in a Texture block
	 * can be overridden by settings from Scene, which owns the Texture through a hierarchy
	 * such as Scene->World->MTex/Texture) can still get correctly overridden.
	 */
#ifdef WITH_LEGACY_DEPSGRAPH
	if (!use_new_eval) {
		BKE_animsys_evaluate_all_animation(bmain, sce, ctime);
		/*...done with recursive funcs */
	}
#endif

	/* clear "LIB_TAG_DOIT" flag from all materials, to prevent infinite recursion problems later
	 * when trying to find materials with drivers that need evaluating [#32017]
	 */
	BKE_main_id_tag_idcode(bmain, ID_MA, LIB_TAG_DOIT, false);
	BKE_main_id_tag_idcode(bmain, ID_LA, LIB_TAG_DOIT, false);

	/* run rigidbody sim */
	/* NOTE: current position is so that rigidbody sim affects other objects, might change in the future */
#ifdef WITH_LEGACY_DEPSGRAPH
	if (!use_new_eval) {
		scene_do_rb_simulation_recursive(sce, ctime);
	}
#endif

	/* BKE_object_handle_update() on all objects, groups and sets */
#ifdef WITH_LEGACY_DEPSGRAPH
	if (use_new_eval) {
		DEG_evaluate_on_framechange(eval_ctx, bmain, sce->depsgraph, ctime, lay);
	}
	else {
		scene_update_tagged_recursive(eval_ctx, bmain, sce, sce);
	}
#else
	DEG_evaluate_on_framechange(eval_ctx, bmain, sce->depsgraph, ctime, lay);
#endif

	/* update sound system animation (TODO, move to depsgraph) */
	BKE_sound_update_scene(bmain, sce);

#ifdef WITH_LEGACY_DEPSGRAPH
	if (!use_new_eval) {
		scene_depsgraph_hack(bmain, eval_ctx, sce, sce);
	}
#endif

	/* notify editors and python about recalc */
	BLI_callback_exec(bmain, &sce->id, BLI_CB_EVT_SCENE_UPDATE_POST);
	BLI_callback_exec(bmain, &sce->id, BLI_CB_EVT_FRAME_CHANGE_POST);

	/* Inform editors about possible changes. */
	DAG_ids_check_recalc(bmain, sce, true);

	/* clear recalc flags */
	DAG_ids_clear_recalc(bmain);

#ifdef DETAILED_ANALYSIS_OUTPUT
	fprintf(stderr, "frame update start_time %f duration %f\n", start_time, PIL_check_seconds_timer() - start_time);
#endif
}

/* return default layer, also used to patch old files */
SceneRenderLayer *BKE_scene_add_render_layer(Scene *sce, const char *name)
{
	SceneRenderLayer *srl;

	if (!name)
		name = DATA_("RenderLayer");

	srl = MEM_callocN(sizeof(SceneRenderLayer), "new render layer");
	BLI_strncpy(srl->name, name, sizeof(srl->name));
	BLI_uniquename(&sce->r.layers, srl, DATA_("RenderLayer"), '.', offsetof(SceneRenderLayer, name), sizeof(srl->name));
	BLI_addtail(&sce->r.layers, srl);

	/* note, this is also in render, pipeline.c, to make layer when scenedata doesnt have it */
	srl->lay = (1 << 20) - 1;
	srl->layflag = 0x7FFF;   /* solid ztra halo edge strand */
	srl->passflag = SCE_PASS_COMBINED | SCE_PASS_Z;
	srl->pass_alpha_threshold = 0.5f;
	BKE_freestyle_config_init(&srl->freestyleConfig);

	return srl;
}

bool BKE_scene_remove_render_layer(Main *bmain, Scene *scene, SceneRenderLayer *srl)
{
	const int act = BLI_findindex(&scene->r.layers, srl);
	Scene *sce;

	if (act == -1) {
		return false;
	}
	else if ( (scene->r.layers.first == scene->r.layers.last) &&
	          (scene->r.layers.first == srl))
	{
		/* ensure 1 layer is kept */
		return false;
	}

	BKE_freestyle_config_free(&srl->freestyleConfig);

	if (srl->prop) {
		IDP_FreeProperty(srl->prop);
		MEM_freeN(srl->prop);
	}

	BLI_remlink(&scene->r.layers, srl);
	MEM_freeN(srl);

	scene->r.actlay = 0;

	for (sce = bmain->scene.first; sce; sce = sce->id.next) {
		if (sce->nodetree) {
			bNode *node;
			for (node = sce->nodetree->nodes.first; node; node = node->next) {
				if (node->type == CMP_NODE_R_LAYERS && (Scene *)node->id == scene) {
					if (node->custom1 == act)
						node->custom1 = 0;
					else if (node->custom1 > act)
						node->custom1--;
				}
			}
		}
	}

	return true;
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

int get_render_shadow_samples(const RenderData *r, int samples)
{
	if ((r->mode & R_SIMPLIFY) && samples > 0)
		return min_ii(r->simplify_shadowsamples, samples);
	else
		return samples;
}

float get_render_aosss_error(const RenderData *r, float error)
{
	if (r->mode & R_SIMPLIFY)
		return ((1.0f - r->simplify_aosss) * 10.0f + 1.0f) * error;
	else
		return error;
}

/* helper function for the SETLOOPER macro */
Base *_setlooper_base_step(Scene **sce_iter, Base *base)
{
	if (base && base->next) {
		/* common case, step to the next */
		return base->next;
	}
	else if (base == NULL && (*sce_iter)->base.first) {
		/* first time looping, return the scenes first base */
		return (Base *)(*sce_iter)->base.first;
	}
	else {
		/* reached the end, get the next base in the set */
		while ((*sce_iter = (*sce_iter)->set)) {
			base = (Base *)(*sce_iter)->base.first;
			if (base) {
				return base;
			}
		}
	}

	return NULL;
}

bool BKE_scene_use_new_shading_nodes(const Scene *scene)
{
	const RenderEngineType *type = RE_engines_find(scene->r.engine);
	return (type && type->flag & RE_USE_SHADING_NODES);
}

bool BKE_scene_use_shading_nodes_custom(Scene *scene)
{
	RenderEngineType *type = RE_engines_find(scene->r.engine);
	return (type && type->flag & RE_USE_SHADING_NODES_CUSTOM);
}

bool BKE_scene_use_world_space_shading(Scene *scene)
{
	const RenderEngineType *type = RE_engines_find(scene->r.engine);
	return ((scene->r.mode & R_USE_WS_SHADING) ||
	        (type && (type->flag & RE_USE_SHADING_NODES)));
}

bool BKE_scene_use_spherical_stereo(Scene *scene)
{
	RenderEngineType *type = RE_engines_find(scene->r.engine);
	return (type && type->flag & RE_USE_SPHERICAL_STEREO);
}

bool BKE_scene_uses_blender_internal(const  Scene *scene)
{
	return STREQ(scene->r.engine, RE_engine_id_BLENDER_RENDER);
}

bool BKE_scene_uses_blender_game(const Scene *scene)
{
	return STREQ(scene->r.engine, RE_engine_id_BLENDER_GAME);
}

void BKE_scene_base_flag_to_objects(struct Scene *scene)
{
	Base *base = scene->base.first;

	while (base) {
		base->object->flag = base->flag;
		base = base->next;
	}
}

void BKE_scene_base_flag_from_objects(struct Scene *scene)
{
	Base *base = scene->base.first;

	while (base) {
		base->flag = base->object->flag;
		base = base->next;
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
