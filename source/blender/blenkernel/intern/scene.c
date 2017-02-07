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
#include "BKE_collection.h"
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

void free_qtcodecdata(QuicktimeCodecData *qcd)
{
	if (qcd) {
		if (qcd->cdParms) {
			MEM_freeN(qcd->cdParms);
			qcd->cdParms = NULL;
			qcd->cdSize = 0;
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

/* copy SceneCollection tree but keep pointing to the same objects */
static void scene_collection_copy(SceneCollection *scn, SceneCollection *sc)
{
	BLI_duplicatelist(&scn->objects, &sc->objects);
	for (LinkData *link = scn->objects.first; link; link = link->next) {
		id_us_plus(link->data);
	}

	BLI_duplicatelist(&scn->filter_objects, &sc->filter_objects);
	for (LinkData *link = scn->filter_objects.first; link; link = link->next) {
		id_us_plus(link->data);
	}

	BLI_duplicatelist(&scn->scene_collections, &sc->scene_collections);
	SceneCollection *nscn = scn->scene_collections.first; /* nested SceneCollection new */
	for (SceneCollection *nsc = sc->scene_collections.first; nsc; nsc = nsc->next) {
		scene_collection_copy(nscn, nsc);
		nscn = nscn->next;
	}
}

/* Find the equivalent SceneCollection in the new tree */
static SceneCollection *scene_collection_from_new_tree(SceneCollection *sc_reference, SceneCollection *scn, SceneCollection *sc)
{
	if (sc == sc_reference) {
		return scn;
	}

	SceneCollection *nscn = scn->scene_collections.first; /* nested master collection new */
	for (SceneCollection *nsc = sc->scene_collections.first; nsc; nsc = nsc->next) {

		SceneCollection *found = scene_collection_from_new_tree(sc_reference, nscn, nsc);
		if (found) {
			return found;
		}
		nscn = nscn->next;
	}
	return NULL;
}

/* recreate the LayerCollection tree */
static void layer_collections_recreate(SceneLayer *sl, ListBase *lb, SceneCollection *mcn, SceneCollection *mc)
{
	for (LayerCollection *lc = lb->first; lc; lc = lc->next) {

		SceneCollection *sc = scene_collection_from_new_tree(lc->scene_collection, mcn, mc);
		BLI_assert(sc);

		/* instead of syncronizing both trees we simply re-create it */
		BKE_collection_link(sl, sc);
	}
}

Scene *BKE_scene_copy(Main *bmain, Scene *sce, int type)
{
	Scene *scen;
	SceneRenderLayer *srl, *new_srl;
	FreestyleLineSet *lineset;
	ToolSettings *ts;
	BaseLegacy *legacy_base, *olegacy_base;
	
	if (type == SCE_COPY_EMPTY) {
		ListBase rl, rv;
		scen = BKE_scene_add(bmain, sce->id.name + 2);
		
		rl = scen->r.layers;
		rv = scen->r.views;
		curvemapping_free_data(&scen->r.mblur_shutter_curve);
		scen->r = sce->r;
		scen->r.layers = rl;
		scen->r.actlay = 0;
		scen->r.views = rv;
		scen->unit = sce->unit;
		scen->physics_settings = sce->physics_settings;
		scen->gm = sce->gm;
		scen->audio = sce->audio;

		if (sce->id.properties)
			scen->id.properties = IDP_CopyProperty(sce->id.properties);

		MEM_freeN(scen->toolsettings);
		BKE_sound_destroy_scene(scen);
	}
	else {
		scen = BKE_libblock_copy(bmain, &sce->id);
		BLI_duplicatelist(&(scen->base), &(sce->base));
		
		id_us_plus((ID *)scen->world);
		id_us_plus((ID *)scen->set);
		/* id_us_plus((ID *)scen->gm.dome.warptext); */  /* XXX Not refcounted? see readfile.c */

		scen->ed = NULL;
		scen->theDag = NULL;
		scen->depsgraph = NULL;
		scen->obedit = NULL;
		scen->stats = NULL;
		scen->fps_info = NULL;

		if (sce->rigidbody_world)
			scen->rigidbody_world = BKE_rigidbody_world_copy(sce->rigidbody_world);

		BLI_duplicatelist(&(scen->markers), &(sce->markers));
		BLI_duplicatelist(&(scen->transform_spaces), &(sce->transform_spaces));
		BLI_duplicatelist(&(scen->r.layers), &(sce->r.layers));
		BLI_duplicatelist(&(scen->r.views), &(sce->r.views));
		BKE_keyingsets_copy(&(scen->keyingsets), &(sce->keyingsets));

		if (sce->nodetree) {
			/* ID's are managed on both copy and switch */
			scen->nodetree = ntreeCopyTree(bmain, sce->nodetree);
			BKE_libblock_relink_ex(bmain, scen->nodetree, &sce->id, &scen->id, false);
		}

		olegacy_base = sce->base.first;
		legacy_base = scen->base.first;
		while (legacy_base) {
			id_us_plus(&legacy_base->object->id);
			if (olegacy_base == sce->basact) scen->basact = legacy_base;
	
			olegacy_base = olegacy_base->next;
			legacy_base = legacy_base->next;
		}

		/* copy action and remove animation used by sequencer */
		BKE_animdata_copy_id_action(&scen->id, false);

		if (type != SCE_COPY_FULL)
			remove_sequencer_fcurves(scen);

		/* copy Freestyle settings */
		new_srl = scen->r.layers.first;
		for (srl = sce->r.layers.first; srl; srl = srl->next) {
			BKE_freestyle_config_copy(&new_srl->freestyleConfig, &srl->freestyleConfig);
			if (type == SCE_COPY_FULL) {
				for (lineset = new_srl->freestyleConfig.linesets.first; lineset; lineset = lineset->next) {
					if (lineset->linestyle) {
						id_us_plus((ID *)lineset->linestyle);
						lineset->linestyle = BKE_linestyle_copy(bmain, lineset->linestyle);
					}
				}
			}
			new_srl = new_srl->next;
		}

		/* layers and collections */
		scen->collection = MEM_dupallocN(sce->collection);
		SceneCollection *mcn = BKE_collection_master(scen);
		SceneCollection *mc = BKE_collection_master(sce);

		/* recursively creates a new SceneCollection tree */
		scene_collection_copy(mcn, mc);

		BLI_duplicatelist(&scen->render_layers, &sce->render_layers);
		SceneLayer *new_sl = scen->render_layers.first;
		for (SceneLayer *sl = sce->render_layers.first; sl; sl = sl->next) {

			/* we start fresh with no overrides and no visibility flags set
			 * instead of syncing both trees we simply unlink and relink the scene collection */
			BLI_listbase_clear(&new_sl->layer_collections);
			BLI_listbase_clear(&new_sl->object_bases);
			layer_collections_recreate(new_sl, &sl->layer_collections, mcn, mc);

			if (sl->basact) {
				Object *active_ob = sl->basact->object;
				for (Base *base = new_sl->object_bases.first; base; base = base->next) {
					if (base->object == active_ob) {
						new_sl->basact = base;
						break;
					}
				}
			}
			new_sl = new_sl->next;
		}
	}

	/* copy color management settings */
	BKE_color_managed_display_settings_copy(&scen->display_settings, &sce->display_settings);
	BKE_color_managed_view_settings_copy(&scen->view_settings, &sce->view_settings);
	BKE_color_managed_colorspace_settings_copy(&scen->sequencer_colorspace_settings, &sce->sequencer_colorspace_settings);

	BKE_color_managed_display_settings_copy(&scen->r.im_format.display_settings, &sce->r.im_format.display_settings);
	BKE_color_managed_view_settings_copy(&scen->r.im_format.view_settings, &sce->r.im_format.view_settings);

	BKE_color_managed_display_settings_copy(&scen->r.bake.im_format.display_settings, &sce->r.bake.im_format.display_settings);
	BKE_color_managed_view_settings_copy(&scen->r.bake.im_format.view_settings, &sce->r.bake.im_format.view_settings);

	curvemapping_copy_data(&scen->r.mblur_shutter_curve, &sce->r.mblur_shutter_curve);

	/* tool settings */
	scen->toolsettings = MEM_dupallocN(sce->toolsettings);

	ts = scen->toolsettings;
	if (ts) {
		if (ts->vpaint) {
			ts->vpaint = MEM_dupallocN(ts->vpaint);
			ts->vpaint->paintcursor = NULL;
			ts->vpaint->vpaint_prev = NULL;
			ts->vpaint->wpaint_prev = NULL;
			BKE_paint_copy(&ts->vpaint->paint, &ts->vpaint->paint);
		}
		if (ts->wpaint) {
			ts->wpaint = MEM_dupallocN(ts->wpaint);
			ts->wpaint->paintcursor = NULL;
			ts->wpaint->vpaint_prev = NULL;
			ts->wpaint->wpaint_prev = NULL;
			BKE_paint_copy(&ts->wpaint->paint, &ts->wpaint->paint);
		}
		if (ts->sculpt) {
			ts->sculpt = MEM_dupallocN(ts->sculpt);
			BKE_paint_copy(&ts->sculpt->paint, &ts->sculpt->paint);
		}

		BKE_paint_copy(&ts->imapaint.paint, &ts->imapaint.paint);
		ts->imapaint.paintcursor = NULL;
		id_us_plus((ID *)ts->imapaint.stencil);
		ts->particle.paintcursor = NULL;
		
		/* duplicate Grease Pencil Drawing Brushes */
		BLI_listbase_clear(&ts->gp_brushes);
		for (bGPDbrush *brush = sce->toolsettings->gp_brushes.first; brush; brush = brush->next) {
			bGPDbrush *newbrush = BKE_gpencil_brush_duplicate(brush);
			BLI_addtail(&ts->gp_brushes, newbrush);
		}
		
		/* duplicate Grease Pencil interpolation curve */
		ts->gp_interpolate.custom_ipo = curvemapping_copy(ts->gp_interpolate.custom_ipo);
	}
	
	/* make a private copy of the avicodecdata */
	if (sce->r.avicodecdata) {
		scen->r.avicodecdata = MEM_dupallocN(sce->r.avicodecdata);
		scen->r.avicodecdata->lpFormat = MEM_dupallocN(scen->r.avicodecdata->lpFormat);
		scen->r.avicodecdata->lpParms = MEM_dupallocN(scen->r.avicodecdata->lpParms);
	}
	
	/* make a private copy of the qtcodecdata */
	if (sce->r.qtcodecdata) {
		scen->r.qtcodecdata = MEM_dupallocN(sce->r.qtcodecdata);
		scen->r.qtcodecdata->cdParms = MEM_dupallocN(scen->r.qtcodecdata->cdParms);
	}
	
	if (sce->r.ffcodecdata.properties) { /* intentionally check scen not sce. */
		scen->r.ffcodecdata.properties = IDP_CopyProperty(sce->r.ffcodecdata.properties);
	}

	/* NOTE: part of SCE_COPY_LINK_DATA and SCE_COPY_FULL operations
	 * are done outside of blenkernel with ED_objects_single_users! */

	/*  camera */
	if (type == SCE_COPY_LINK_DATA || type == SCE_COPY_FULL) {
		ID_NEW_REMAP(scen->camera);
	}
	
	/* before scene copy */
	BKE_sound_create_scene(scen);

	/* world */
	if (type == SCE_COPY_FULL) {
		if (scen->world) {
			id_us_plus((ID *)scen->world);
			scen->world = BKE_world_copy(bmain, scen->world);
			BKE_animdata_copy_id_action((ID *)scen->world, false);
		}

		if (sce->ed) {
			scen->ed = MEM_callocN(sizeof(Editing), "addseq");
			scen->ed->seqbasep = &scen->ed->seqbase;
			BKE_sequence_base_dupli_recursive(sce, scen, &scen->ed->seqbase, &sce->ed->seqbase, SEQ_DUPE_ALL);
		}
	}
	
	/* grease pencil */
	if (scen->gpd) {
		if (type == SCE_COPY_FULL) {
			scen->gpd = BKE_gpencil_data_duplicate(bmain, scen->gpd, false);
		}
		else if (type == SCE_COPY_EMPTY) {
			scen->gpd = NULL;
		}
		else {
			id_us_plus((ID *)scen->gpd);
		}
	}

	BKE_previewimg_id_copy(&scen->id, &sce->id);

	return scen;
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

	/* check all sequences */
	BKE_sequencer_clear_scene_in_allseqs(G.main, sce);

	sce->basact = NULL;
	BLI_freelistN(&sce->base);
	BKE_sequencer_editing_free(sce);

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
	if (sce->r.qtcodecdata) {
		free_qtcodecdata(sce->r.qtcodecdata);
		MEM_freeN(sce->r.qtcodecdata);
		sce->r.qtcodecdata = NULL;
	}
	if (sce->r.ffcodecdata.properties) {
		IDP_FreeProperty(sce->r.ffcodecdata.properties);
		MEM_freeN(sce->r.ffcodecdata.properties);
		sce->r.ffcodecdata.properties = NULL;
	}
	
	for (srl = sce->r.layers.first; srl; srl = srl->next) {
		BKE_freestyle_config_free(&srl->freestyleConfig);
	}
	
	BLI_freelistN(&sce->markers);
	BLI_freelistN(&sce->transform_spaces);
	BLI_freelistN(&sce->r.layers);
	BLI_freelistN(&sce->r.views);
	
	if (sce->toolsettings) {
		if (sce->toolsettings->vpaint) {
			BKE_paint_free(&sce->toolsettings->vpaint->paint);
			MEM_freeN(sce->toolsettings->vpaint);
		}
		if (sce->toolsettings->wpaint) {
			BKE_paint_free(&sce->toolsettings->wpaint->paint);
			MEM_freeN(sce->toolsettings->wpaint);
		}
		if (sce->toolsettings->sculpt) {
			BKE_paint_free(&sce->toolsettings->sculpt->paint);
			MEM_freeN(sce->toolsettings->sculpt);
		}
		if (sce->toolsettings->uvsculpt) {
			BKE_paint_free(&sce->toolsettings->uvsculpt->paint);
			MEM_freeN(sce->toolsettings->uvsculpt);
		}
		BKE_paint_free(&sce->toolsettings->imapaint.paint);
		
		/* free Grease Pencil Drawing Brushes */
		BKE_gpencil_free_brushes(&sce->toolsettings->gp_brushes);
		BLI_freelistN(&sce->toolsettings->gp_brushes);
		
		/* free Grease Pencil interpolation curve */
		if (sce->toolsettings->gp_interpolate.custom_ipo) {
			curvemapping_free(sce->toolsettings->gp_interpolate.custom_ipo);
		}
		
		MEM_freeN(sce->toolsettings);
		sce->toolsettings = NULL;
	}
	
	DAG_scene_free(sce);
	if (sce->depsgraph)
		DEG_graph_free(sce->depsgraph);
	
	MEM_SAFE_FREE(sce->stats);
	MEM_SAFE_FREE(sce->fps_info);

	BKE_sound_destroy_scene(sce);

	BKE_color_managed_view_settings_free(&sce->view_settings);

	BKE_previewimg_free(&sce->preview);
	curvemapping_free_data(&sce->r.mblur_shutter_curve);

	for (SceneLayer *sl = sce->render_layers.first; sl; sl = sl->next) {
		BKE_scene_layer_free(sl);
	}
	BLI_freelistN(&sce->render_layers);

	/* Master Collection */
	BKE_collection_master_free(sce);
	MEM_freeN(sce->collection);
	sce->collection = NULL;
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
	sce->r.seq_flag = R_SEQ_GL_PREV;

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
	for (a = 0; a < PE_TOT_BRUSH; a++) {
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

	/* Master Collection */
	sce->collection = MEM_callocN(sizeof(SceneCollection), "Master Collection");
	BLI_strncpy(sce->collection->name, "Master Collection", sizeof(sce->collection->name));

	BKE_scene_layer_add(sce, "Render Layer");
}

Scene *BKE_scene_add(Main *bmain, const char *name)
{
	Scene *sce;

	sce = BKE_libblock_alloc(bmain, ID_SCE, name);
	id_us_min(&sce->id);
	id_us_ensure_real(&sce->id);

	BKE_scene_init(sce);

	return sce;
}

BaseLegacy *BKE_scene_base_find_by_name(struct Scene *scene, const char *name)
{
	BaseLegacy *base;

	for (base = scene->base.first; base; base = base->next) {
		if (STREQ(base->object->id.name + 2, name)) {
			break;
		}
	}

	return base;
}

BaseLegacy *BKE_scene_base_find(Scene *scene, Object *ob)
{
	return BLI_findptr(&scene->base, ob, offsetof(BaseLegacy, object));
}

/**
 * Sets the active scene, mainly used when running in background mode (``--scene`` command line argument).
 * This is also called to set the scene directly, bypassing windowing code.
 * Otherwise #ED_screen_set_scene is used when changing scenes by the user.
 */
void BKE_scene_set_background(Main *bmain, Scene *scene)
{
	Scene *sce;
	BaseLegacy *base;
	Object *ob;
	Group *group;
	GroupObject *go;
	
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
		BKE_scene_base_flag_sync_from_base(base);
	}
	/* no full animation update, this to enable render code to work (render code calls own animation updates) */
}

/* called from creator_args.c */
Scene *BKE_scene_set_name(Main *bmain, const char *name)
{
	Scene *sce = (Scene *)BKE_libblock_find_name_ex(bmain, ID_SCE, name);
	if (sce) {
		BKE_scene_set_background(bmain, sce);
		printf("Scene switch for render: '%s' in file: '%s'\n", name, bmain->name);
		return sce;
	}

	printf("Can't find scene: '%s' in file: '%s'\n", name, bmain->name);
	return NULL;
}

/* Used by metaballs, return *all* objects (including duplis) existing in the scene (including scene's sets) */
int BKE_scene_base_iter_next(EvaluationContext *eval_ctx, SceneBaseIter *iter,
                             Scene **scene, int val, BaseLegacy **base, Object **ob)
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
							iter->duplilist = object_duplilist_ex(eval_ctx, (*scene), (*base)->object, false);
							
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

Object *BKE_scene_camera_find(Scene *sc)
{
	BaseLegacy *base;
	
	for (base = sc->base.first; base; base = base->next)
		if (base->object->type == OB_CAMERA)
			return base->object;

	return NULL;
}

#ifdef DURIAN_CAMERA_SWITCH
Object *BKE_scene_camera_switch_find(Scene *scene)
{
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

void BKE_scene_remove_rigidbody_object(Scene *scene, Object *ob)
{
	/* remove rigid body constraint from world before removing object */
	if (ob->rigidbody_constraint)
		BKE_rigidbody_remove_constraint(scene, ob);
	/* remove rigid body object from world before removing object */
	if (ob->rigidbody_object)
		BKE_rigidbody_remove_object(scene, ob);
}

BaseLegacy *BKE_scene_base_add(Scene *sce, Object *ob)
{
	BaseLegacy *b = MEM_callocN(sizeof(*b), __func__);
	BLI_addhead(&sce->base, b);

	b->object = ob;
	b->flag_legacy = ob->flag;
	b->lay = ob->lay;

	return b;
}

void BKE_scene_base_unlink(Scene *sce, BaseLegacy *base)
{
	BKE_scene_remove_rigidbody_object(sce, base->object);

	BLI_remlink(&sce->base, base);
	if (sce->basact == base)
		sce->basact = NULL;
}

void BKE_scene_base_deselect_all(Scene *sce)
{
	BaseLegacy *b;

	for (b = sce->base.first; b; b = b->next) {
		b->flag_legacy &= ~SELECT;
		int flag = b->object->flag & (OB_FROMGROUP);
		b->object->flag = b->flag_legacy;
		b->object->flag |= flag;
	}
}

void BKE_scene_base_select(Scene *sce, BaseLegacy *selbase)
{
	selbase->flag_legacy |= SELECT;
	selbase->object->flag = selbase->flag_legacy;

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
		    ((obedit->id.tag & LIB_TAG_ID_RECALC_ALL) ||
		     (mesh->id.tag & LIB_TAG_ID_RECALC_ALL)))
		{
			if (check_rendered_viewport_visible(bmain)) {
				BMesh *bm = mesh->edit_btmesh->bm;
				BM_mesh_bm_to_me(bm, mesh, (&(struct BMeshToMeshParams){0}));
				DAG_id_tag_update(&mesh->id, 0);
			}
		}
	}
}

void BKE_scene_update_tagged(EvaluationContext *eval_ctx, Main *bmain, Scene *scene)
{
	Scene *sce_iter;

	/* keep this first */
	BLI_callback_exec(bmain, &scene->id, BLI_CB_EVT_SCENE_UPDATE_PRE);

	/* (re-)build dependency graph if needed */
	for (sce_iter = scene; sce_iter; sce_iter = sce_iter->set) {
		DAG_scene_relations_update(bmain, sce_iter);
		/* Uncomment this to check if graph was properly tagged for update. */
#if 0
		DAG_scene_relations_validate(bmain, sce_iter);
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
	DEG_evaluate_on_refresh(eval_ctx, scene->depsgraph, scene);
	/* TODO(sergey): This is to beocme a node in new depsgraph. */
	BKE_mask_update_scene(bmain, scene);

	/* update sound system animation (TODO, move to depsgraph) */
	BKE_sound_update_scene(bmain, scene);

	/* extra call here to recalc scene animation (for sequencer) */
	{
		AnimData *adt = BKE_animdata_from_id(&scene->id);
		float ctime = BKE_scene_frame_get(scene);
		
		if (adt && (adt->recalc & ADT_RECALC_ANIM))
			BKE_animsys_evaluate_animdata(scene, &scene->id, adt, ctime, 0);
	}

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

void BKE_scene_update_for_newframe_ex(EvaluationContext *eval_ctx, Main *bmain, Scene *sce, unsigned int lay, bool UNUSED(do_invisible_flush))
{
	float ctime = BKE_scene_frame_get(sce);
	Scene *sce_iter;

	DAG_editors_update_pre(bmain, sce, true);

	/* keep this first */
	BLI_callback_exec(bmain, &sce->id, BLI_CB_EVT_FRAME_CHANGE_PRE);
	BLI_callback_exec(bmain, &sce->id, BLI_CB_EVT_SCENE_UPDATE_PRE);

	/* update animated image textures for particles, modifiers, gpu, etc,
	 * call this at the start so modifiers with textures don't lag 1 frame */
	BKE_image_update_frame(bmain, sce->r.cfra);

	BKE_sound_set_cfra(sce->r.cfra);

	/* clear animation overrides */
	/* XXX TODO... */

	for (sce_iter = sce; sce_iter; sce_iter = sce_iter->set)
		DAG_scene_relations_update(bmain, sce_iter);

	BKE_mask_evaluate_all_masks(bmain, ctime, true);

	/* Update animated cache files for modifiers. */
	BKE_cachefile_update_frame(bmain, sce, ctime, (((double)sce->r.frs_sec) / (double)sce->r.frs_sec_base));

#ifdef POSE_ANIMATION_WORKAROUND
	scene_armature_depsgraph_workaround(bmain);
#endif

	/* clear "LIB_TAG_DOIT" flag from all materials, to prevent infinite recursion problems later
	 * when trying to find materials with drivers that need evaluating [#32017] 
	 */
	BKE_main_id_tag_idcode(bmain, ID_MA, LIB_TAG_DOIT, false);
	BKE_main_id_tag_idcode(bmain, ID_LA, LIB_TAG_DOIT, false);

	/* BKE_object_handle_update() on all objects, groups and sets */
	DEG_evaluate_on_framechange(eval_ctx, bmain, sce->depsgraph, ctime, lay);

	/* update sound system animation (TODO, move to depsgraph) */
	BKE_sound_update_scene(bmain, sce);

	/* notify editors and python about recalc */
	BLI_callback_exec(bmain, &sce->id, BLI_CB_EVT_SCENE_UPDATE_POST);
	BLI_callback_exec(bmain, &sce->id, BLI_CB_EVT_FRAME_CHANGE_POST);

	/* Inform editors about possible changes. */
	DAG_ids_check_recalc(bmain, sce, true);

	/* clear recalc flags */
	DAG_ids_clear_recalc(bmain);
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

	BLI_remlink(&scene->r.layers, srl);
	MEM_freeN(srl);

	scene->r.actlay = 0;

	for (sce = bmain->scene.first; sce; sce = sce->id.next) {
		if (sce->nodetree) {
			BKE_nodetree_remove_layer_n(sce->nodetree, scene, act);
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
	if (r->mode & R_SIMPLIFY)  {
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
BaseLegacy *_setlooper_base_step(Scene **sce_iter, BaseLegacy *base)
{
	if (base && base->next) {
		/* common case, step to the next */
		return base->next;
	}
	else if (base == NULL && (*sce_iter)->base.first) {
		/* first time looping, return the scenes first base */
		return (BaseLegacy *)(*sce_iter)->base.first;
	}
	else {
		/* reached the end, get the next base in the set */
		while ((*sce_iter = (*sce_iter)->set)) {
			base = (BaseLegacy *)(*sce_iter)->base.first;
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
	BaseLegacy *base = scene->base.first;

	while (base) {
		BKE_scene_base_flag_sync_from_base(base);
		base = base->next;
	}
}

void BKE_scene_base_flag_from_objects(struct Scene *scene)
{
	BaseLegacy *base = scene->base.first;

	while (base) {
		BKE_scene_base_flag_sync_from_object(base);
		base = base->next;
	}
}

void BKE_scene_base_flag_sync_from_base(BaseLegacy *base)
{
	Object *ob = base->object;

	/* keep the object only flags untouched */
	int flag = ob->flag & OB_FROMGROUP;

	ob->flag = base->flag_legacy;
	ob->flag |= flag;
}

void BKE_scene_base_flag_sync_from_object(BaseLegacy *base)
{
	base->flag_legacy = base->object->flag;
}

void BKE_scene_object_base_flag_sync_from_base(Base *base)
{
	Object *ob = base->object;

	/* keep the object only flags untouched */
	int flag = ob->flag & OB_FROMGROUP;

	ob->flag = base->flag;
	ob->flag |= flag;
}

void BKE_scene_object_base_flag_sync_from_object(Base *base)
{
	base->flag = base->object->flag;
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
