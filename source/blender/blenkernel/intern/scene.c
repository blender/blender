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

#include "BLI_math.h"
#include "BLI_blenlib.h"
#include "BLI_utildefines.h"
#include "BLI_callbacks.h"
#include "BLI_string.h"
#include "BLI_threads.h"
#include "BLI_task.h"

#include "BLF_translation.h"

#include "BKE_anim.h"
#include "BKE_animsys.h"
#include "BKE_action.h"
#include "BKE_colortools.h"
#include "BKE_depsgraph.h"
#include "BKE_editmesh.h"
#include "BKE_fcurve.h"
#include "BKE_freestyle.h"
#include "BKE_global.h"
#include "BKE_group.h"
#include "BKE_idprop.h"
#include "BKE_image.h"
#include "BKE_library.h"
#include "BKE_main.h"
#include "BKE_mask.h"
#include "BKE_node.h"
#include "BKE_object.h"
#include "BKE_paint.h"
#include "BKE_rigidbody.h"
#include "BKE_scene.h"
#include "BKE_sequencer.h"
#include "BKE_sound.h"
#include "BKE_unit.h"
#include "BKE_world.h"

#include "RE_engine.h"

#include "PIL_time.h"

#include "IMB_colormanagement.h"

#include "bmesh.h"

//XXX #include "BIF_previewrender.h"
//XXX #include "BIF_editseq.h"

#ifdef WIN32
#else
#  include <sys/time.h>
#endif

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

Scene *BKE_scene_copy(Scene *sce, int type)
{
	Scene *scen;
	SceneRenderLayer *srl, *new_srl;
	ToolSettings *ts;
	Base *base, *obase;
	
	if (type == SCE_COPY_EMPTY) {
		ListBase lb;
		/* XXX. main should become an arg */
		scen = BKE_scene_add(G.main, sce->id.name + 2);
		
		lb = scen->r.layers;
		scen->r = sce->r;
		scen->r.layers = lb;
		scen->r.actlay = 0;
		scen->unit = sce->unit;
		scen->physics_settings = sce->physics_settings;
		scen->gm = sce->gm;
		scen->audio = sce->audio;

		if (sce->id.properties)
			scen->id.properties = IDP_CopyProperty(sce->id.properties);

		MEM_freeN(scen->toolsettings);
	}
	else {
		scen = BKE_libblock_copy(&sce->id);
		BLI_duplicatelist(&(scen->base), &(sce->base));
		
		BKE_main_id_clear_newpoins(G.main);
		
		id_us_plus((ID *)scen->world);
		id_us_plus((ID *)scen->set);
		id_us_plus((ID *)scen->gm.dome.warptext);

		scen->ed = NULL;
		scen->theDag = NULL;
		scen->obedit = NULL;
		scen->stats = NULL;
		scen->fps_info = NULL;

		if (sce->rigidbody_world)
			scen->rigidbody_world = BKE_rigidbody_world_copy(sce->rigidbody_world);

		BLI_duplicatelist(&(scen->markers), &(sce->markers));
		BLI_duplicatelist(&(scen->transform_spaces), &(sce->transform_spaces));
		BLI_duplicatelist(&(scen->r.layers), &(sce->r.layers));
		BKE_keyingsets_copy(&(scen->keyingsets), &(sce->keyingsets));

		if (sce->nodetree) {
			/* ID's are managed on both copy and switch */
			scen->nodetree = ntreeCopyTree(sce->nodetree);
			ntreeSwitchID(scen->nodetree, &sce->id, &scen->id);
		}

		obase = sce->base.first;
		base = scen->base.first;
		while (base) {
			id_us_plus(&base->object->id);
			if (obase == sce->basact) scen->basact = base;
	
			obase = obase->next;
			base = base->next;
		}

		/* copy color management settings */
		BKE_color_managed_display_settings_copy(&scen->display_settings, &sce->display_settings);
		BKE_color_managed_view_settings_copy(&scen->view_settings, &sce->view_settings);
		BKE_color_managed_view_settings_copy(&scen->r.im_format.view_settings, &sce->r.im_format.view_settings);

		BLI_strncpy(scen->sequencer_colorspace_settings.name, sce->sequencer_colorspace_settings.name,
		            sizeof(scen->sequencer_colorspace_settings.name));

		/* copy action and remove animation used by sequencer */
		BKE_copy_animdata_id_action(&scen->id);

		if (type != SCE_COPY_FULL)
			remove_sequencer_fcurves(scen);

		/* copy Freestyle settings */
		new_srl = scen->r.layers.first;
		for (srl = sce->r.layers.first; srl; srl = srl->next) {
			BKE_freestyle_config_copy(&new_srl->freestyleConfig, &srl->freestyleConfig);
			new_srl = new_srl->next;
		}
	}

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
		ID_NEW(scen->camera);
	}
	
	/* before scene copy */
	sound_create_scene(scen);

	/* world */
	if (type == SCE_COPY_FULL) {
		if (scen->world) {
			id_us_plus((ID *)scen->world);
			scen->world = BKE_world_copy(scen->world);
			BKE_copy_animdata_id_action((ID *)scen->world);
		}

		if (sce->ed) {
			scen->ed = MEM_callocN(sizeof(Editing), "addseq");
			scen->ed->seqbasep = &scen->ed->seqbase;
			BKE_sequence_base_dupli_recursive(sce, scen, &scen->ed->seqbase, &sce->ed->seqbase, SEQ_DUPE_ALL);
		}
	}

	return scen;
}

void BKE_scene_groups_relink(Scene *sce)
{
	if (sce->rigidbody_world)
		BKE_rigidbody_world_groups_relink(sce->rigidbody_world);
}

/* do not free scene itself */
void BKE_scene_free(Scene *sce)
{
	Base *base;
	SceneRenderLayer *srl;

	/* check all sequences */
	BKE_sequencer_clear_scene_in_allseqs(G.main, sce);

	base = sce->base.first;
	while (base) {
		base->object->id.us--;
		base = base->next;
	}
	/* do not free objects! */
	
	if (sce->gpd) {
#if 0   /* removed since this can be invalid memory when freeing everything */
		/* since the grease pencil data is freed before the scene.
		 * since grease pencil data is not (yet?), shared between objects
		 * its probably safe not to do this, some save and reload will free this. */
		sce->gpd->id.us--;
#endif
		sce->gpd = NULL;
	}

	BLI_freelistN(&sce->base);
	BKE_sequencer_editing_free(sce);

	BKE_free_animdata((ID *)sce);
	BKE_keyingsets_free(&sce->keyingsets);
	
	if (sce->rigidbody_world)
		BKE_rigidbody_free_world(sce->rigidbody_world);
	
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

		MEM_freeN(sce->toolsettings);
		sce->toolsettings = NULL;
	}
	
	DAG_scene_free(sce);
	
	if (sce->nodetree) {
		ntreeFreeTree(sce->nodetree);
		MEM_freeN(sce->nodetree);
	}

	if (sce->stats)
		MEM_freeN(sce->stats);
	if (sce->fps_info)
		MEM_freeN(sce->fps_info);

	sound_destroy_scene(sce);

	BKE_color_managed_view_settings_free(&sce->view_settings);
}

Scene *BKE_scene_add(Main *bmain, const char *name)
{
	Scene *sce;
	ParticleEditSettings *pset;
	int a;
	const char *colorspace_name;

	sce = BKE_libblock_alloc(bmain, ID_SCE, name);
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
	sce->r.stamp = R_STAMP_TIME | R_STAMP_FRAME | R_STAMP_DATE | R_STAMP_CAMERA | R_STAMP_SCENE | R_STAMP_FILENAME | R_STAMP_RENDERTIME;
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

	sce->toolsettings = MEM_callocN(sizeof(struct ToolSettings), "Tool Settings Struct");
	sce->toolsettings->doublimit = 0.001;
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
		pset->brush[a].strength = 0.5;
		pset->brush[a].size = 50;
		pset->brush[a].step = 10;
		pset->brush[a].count = 10;
	}
	pset->brush[PE_BRUSH_CUT].strength = 100;

	sce->r.ffcodecdata.audio_mixrate = 44100;
	sce->r.ffcodecdata.audio_volume = 1.0f;
	sce->r.ffcodecdata.audio_bitrate = 192;
	sce->r.ffcodecdata.audio_channels = 2;

	BLI_strncpy(sce->r.engine, "BLENDER_RENDER", sizeof(sce->r.engine));

	sce->audio.distance_model = 2.0f;
	sce->audio.doppler_factor = 1.0f;
	sce->audio.speed_of_sound = 343.3f;
	sce->audio.volume = 1.0f;

	BLI_strncpy(sce->r.pic, U.renderdir, sizeof(sce->r.pic));

	BLI_rctf_init(&sce->r.safety, 0.1f, 0.9f, 0.1f, 0.9f);
	sce->r.osa = 8;

	/* note; in header_info.c the scene copy happens..., if you add more to renderdata it has to be checked there */
	BKE_scene_add_render_layer(sce, NULL);
	
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
	sce->gm.recastData.agentmaxslope = M_PI / 2;
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

	sce->gm.exitkey = 218; // Blender key code for ESC

	sound_create_scene(sce);

	/* color management */
	colorspace_name = IMB_colormanagement_role_colorspace_name_get(COLOR_ROLE_DEFAULT_SEQUENCER);

	BKE_color_managed_display_settings_init(&sce->display_settings);
	BKE_color_managed_view_settings_init(&sce->view_settings);
	BLI_strncpy(sce->sequencer_colorspace_settings.name, colorspace_name,
	            sizeof(sce->sequencer_colorspace_settings.name));

	return sce;
}

Base *BKE_scene_base_find(Scene *scene, Object *ob)
{
	return BLI_findptr(&scene->base, ob, offsetof(Base, object));
}

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

/* called from creator.c */
Scene *BKE_scene_set_name(Main *bmain, const char *name)
{
	Scene *sce = (Scene *)BKE_libblock_find_name_ex(bmain, ID_SCE, name);
	if (sce) {
		BKE_scene_set_background(bmain, sce);
		printf("Scene switch: '%s' in file: '%s'\n", name, bmain->name);
		return sce;
	}

	printf("Can't find scene: '%s' in file: '%s'\n", name, bmain->name);
	return NULL;
}

static void scene_unlink_space_node(SpaceNode *snode, Scene *sce)
{
	if (snode->id == &sce->id) {
		/* nasty DNA logic for SpaceNode:
		 * ideally should be handled by editor code, but would be bad level call
		 */
		bNodeTreePath *path, *path_next;
		for (path = snode->treepath.first; path; path = path_next) {
			path_next = path->next;
			MEM_freeN(path);
		}
		BLI_listbase_clear(&snode->treepath);
		
		snode->id = NULL;
		snode->from = NULL;
		snode->nodetree = NULL;
		snode->edittree = NULL;
	}
}

static void scene_unlink_space_buts(SpaceButs *sbuts, Scene *sce)
{
	if (sbuts->pinid == &sce->id) {
		sbuts->pinid = NULL;
	}
}

void BKE_scene_unlink(Main *bmain, Scene *sce, Scene *newsce)
{
	Scene *sce1;
	bScreen *screen;

	/* check all sets */
	for (sce1 = bmain->scene.first; sce1; sce1 = sce1->id.next)
		if (sce1->set == sce)
			sce1->set = NULL;
	
	for (sce1 = bmain->scene.first; sce1; sce1 = sce1->id.next) {
		bNode *node;
		
		if (sce1 == sce || !sce1->nodetree)
			continue;
		
		for (node = sce1->nodetree->nodes.first; node; node = node->next) {
			if (node->id == &sce->id)
				node->id = NULL;
		}
	}
	
	/* all screens */
	for (screen = bmain->screen.first; screen; screen = screen->id.next) {
		ScrArea *area;
		
		if (screen->scene == sce)
			screen->scene = newsce;
		
		for (area = screen->areabase.first; area; area = area->next) {
			SpaceLink *space_link;
			for (space_link = area->spacedata.first; space_link; space_link = space_link->next) {
				switch (space_link->spacetype) {
					case SPACE_NODE:
						scene_unlink_space_node((SpaceNode *)space_link, sce);
						break;
					case SPACE_BUTS:
						scene_unlink_space_buts((SpaceButs *)space_link, sce);
						break;
				}
			}
		}
	}

	BKE_libblock_free(bmain, sce);
}

/* Used by metaballs, return *all* objects (including duplis) existing in the scene (including scene's sets) */
int BKE_scene_base_iter_next(EvaluationContext *eval_ctx, SceneBaseIter *iter,
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


Base *BKE_scene_base_add(Scene *sce, Object *ob)
{
	Base *b = MEM_callocN(sizeof(*b), "BKE_scene_base_add");
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
int BKE_scene_validate_setscene(Main *bmain, Scene *sce)
{
	Scene *scene;
	int a, totscene;
	
	if (sce->set == NULL) return 1;
	
	totscene = 0;
	for (scene = bmain->scene.first; scene; scene = scene->id.next)
		totscene++;
	
	for (a = 0, scene = sce; scene->set; scene = scene->set, a++) {
		/* more iterations than scenes means we have a cycle */
		if (a > totscene) {
			/* the tested scene gets zero'ed, that's typically current scene */
			sce->set = NULL;
			return 0;
		}
	}

	return 1;
}

/* This function is needed to cope with fractional frames - including two Blender rendering features
 * mblur (motion blur that renders 'subframes' and blurs them together), and fields rendering. 
 */
float BKE_scene_frame_get(Scene *scene)
{
	return BKE_scene_frame_get_from_ctime(scene, scene->r.cfra);
}

/* This function is used to obtain arbitrary fractional frames */
float BKE_scene_frame_get_from_ctime(Scene *scene, const float frame)
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
static void scene_depsgraph_hack(EvaluationContext *eval_ctx, Scene *scene, Scene *scene_parent)
{
	Base *base;
		
	scene->customdata_mask = scene_parent->customdata_mask;
	
	/* sets first, we allow per definition current scene to have
	 * dependencies on sets, but not the other way around. */
	if (scene->set)
		scene_depsgraph_hack(eval_ctx, scene->set, scene_parent);
	
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
			BKE_object_handle_update(eval_ctx, scene_parent, ob);
			
			if (ob->dup_group && (ob->transflag & OB_DUPLIGROUP)) {
				GroupObject *go;
				
				for (go = ob->dup_group->gobject.first; go; go = go->next) {
					if (go->ob)
						go->ob->recalc |= recalc;
				}
				BKE_group_handle_recalc_and_update(eval_ctx, scene_parent, ob, ob->dup_group);
			}
		}
	}

}

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

/* Used to visualize CPU threads activity during threaded object update,
 * would pollute STDERR with whole bunch of timing information which then
 * could be parsed and nicely visualized.
 */
#undef DETAILED_ANALYSIS_OUTPUT

/* Mballs evaluation uses BKE_scene_base_iter_next which calls
 * duplilist for all objects in the scene. This leads to conflict
 * accessing and writing same data from multiple threads.
 *
 * Ideally Mballs shouldn't do such an iteration and use DAG
 * queries instead. For the time being we've got new DAG
 * let's keep it simple and update mballs in a ingle thread.
 */
#define MBALL_SINGLETHREAD_HACK

typedef struct StatisicsEntry {
	struct StatisicsEntry *next, *prev;
	Object *object;
	double start_time;
	double duration;
} StatisicsEntry;

typedef struct ThreadedObjectUpdateState {
	/* TODO(sergey): We might want this to be per-thread object. */
	EvaluationContext *eval_ctx;
	Scene *scene;
	Scene *scene_parent;
	double base_time;

	/* Execution statistics */
	ListBase statistics[BLENDER_MAX_THREADS];
	bool has_updated_objects;

#ifdef MBALL_SINGLETHREAD_HACK
	bool has_mballs;
#endif
} ThreadedObjectUpdateState;

static void scene_update_object_add_task(void *node, void *user_data);

static void scene_update_all_bases(EvaluationContext *eval_ctx, Scene *scene, Scene *scene_parent)
{
	Base *base;

	for (base = scene->base.first; base; base = base->next) {
		Object *object = base->object;

		BKE_object_handle_update_ex(eval_ctx, scene_parent, object, scene->rigidbody_world, true);

		if (object->dup_group && (object->transflag & OB_DUPLIGROUP))
			BKE_group_handle_recalc_and_update(eval_ctx, scene_parent, object, object->dup_group);

		/* always update layer, so that animating layers works (joshua july 2010) */
		/* XXX commented out, this has depsgraph issues anyway - and this breaks setting scenes
		 * (on scene-set, the base-lay is copied to ob-lay (ton nov 2012) */
		// base->lay = ob->lay;
	}
}

static void scene_update_object_func(TaskPool *pool, void *taskdata, int threadid)
{
/* Disable print for now in favor of summary statistics at the end of update. */
#define PRINT if (false) printf

	ThreadedObjectUpdateState *state = (ThreadedObjectUpdateState *) BLI_task_pool_userdata(pool);
	void *node = taskdata;
	Object *object = DAG_get_node_object(node);
	EvaluationContext *eval_ctx = state->eval_ctx;
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

		if (G.debug & G_DEBUG_DEPSGRAPH) {
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
		BKE_object_handle_update_ex(eval_ctx, scene_parent, object, scene->rigidbody_world, false);

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
	int i, tot_thread;

	if ((G.debug & G_DEBUG_DEPSGRAPH) == 0) {
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
	tot_thread = BLI_system_thread_count();

	for (i = 0; i < tot_thread; i++) {
		int total_objects = 0;
		double total_time = 0.0;
		StatisicsEntry *entry;

		if (state->has_updated_objects) {
			/* Don't pollute output if no objects were updated. */
			for (entry = state->statistics[i].first;
			     entry;
			     entry = entry->next)
			{
				total_objects++;
				total_time += entry->duration;
			}

			printf("Thread %d: total %d objects in %f sec.\n", i, total_objects, total_time);

			for (entry = state->statistics[i].first;
			     entry;
			     entry = entry->next)
			{
				printf("  %s in %f sec\n", entry->object->id.name + 2, entry->duration);
			}
		}

		BLI_freelistN(&state->statistics[i]);
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
	TaskScheduler *task_scheduler = BLI_task_scheduler_get();
	TaskPool *task_pool;
	ThreadedObjectUpdateState state;
	bool need_singlethread_pass;

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
	state.scene = scene;
	state.scene_parent = scene_parent;

	/* Those are only needed when blender is run with --debug argument. */
	if (G.debug & G_DEBUG_DEPSGRAPH) {
		memset(state.statistics, 0, sizeof(state.statistics));
		state.has_updated_objects = false;
		state.base_time = PIL_check_seconds_timer();
	}

#ifdef MBALL_SINGLETHREAD_HACK
	state.has_mballs = false;
#endif

	task_pool = BLI_task_pool_create(task_scheduler, &state);

	DAG_threaded_update_begin(scene, scene_update_object_add_task, task_pool);
	BLI_task_pool_work_and_wait(task_pool);
	BLI_task_pool_free(task_pool);

	if (G.debug & G_DEBUG_DEPSGRAPH) {
		print_threads_statistics(&state);
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
		scene_update_all_bases(eval_ctx, scene, scene_parent);
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
		/* TODO(sergey): Check object recalc flags as well? */
		if ((obedit->type == OB_MESH) &&
		    (mesh->id.flag & (LIB_ID_RECALC | LIB_ID_RECALC_DATA)))
		{
			if (check_rendered_viewport_visible(bmain)) {
				BMesh *bm = mesh->edit_btmesh->bm;
				BM_mesh_bm_to_me(bm, mesh, false);
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
	for (sce_iter = scene; sce_iter; sce_iter = sce_iter->set)
		DAG_scene_relations_update(bmain, sce_iter);

	/* flush editing data if needed */
	prepare_mesh_for_viewport_render(bmain, scene);

	/* flush recalc flags to dependencies */
	DAG_ids_flush_tagged(bmain);

	/* removed calls to quick_cache, see pointcache.c */
	
	/* clear "LIB_DOIT" flag from all materials, to prevent infinite recursion problems later 
	 * when trying to find materials with drivers that need evaluating [#32017] 
	 */
	BKE_main_id_tag_idcode(bmain, ID_MA, false);
	BKE_main_id_tag_idcode(bmain, ID_LA, false);

	/* update all objects: drivers, matrices, displists, etc. flags set
	 * by depgraph or manual, no layer check here, gets correct flushed
	 *
	 * in the future this should handle updates for all datablocks, not
	 * only objects and scenes. - brecht */
	scene_update_tagged_recursive(eval_ctx, bmain, scene, scene);
	/* update sound system animation (TODO, move to depsgraph) */
	sound_update_scene(bmain, scene);

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
	if (DAG_id_type_tagged(bmain, ID_MA)) {
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
	if (DAG_id_type_tagged(bmain, ID_NT)) {
		float ctime = BKE_scene_frame_get(scene);

		FOREACH_NODETREE(bmain, ntree, id)
		{
			AnimData *adt = BKE_animdata_from_id(&ntree->id);
			if (adt && (adt->recalc & ADT_RECALC_ANIM))
				BKE_animsys_evaluate_animdata(scene, &ntree->id, adt, ctime, 0);
		}
		FOREACH_NODETREE_END
	}

	/* notify editors and python about recalc */
	BLI_callback_exec(bmain, &scene->id, BLI_CB_EVT_SCENE_UPDATE_POST);
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

	/* keep this first */
	BLI_callback_exec(bmain, &sce->id, BLI_CB_EVT_FRAME_CHANGE_PRE);
	BLI_callback_exec(bmain, &sce->id, BLI_CB_EVT_SCENE_UPDATE_PRE);

	/* update animated image textures for particles, modifiers, gpu, etc,
	 * call this at the start so modifiers with textures don't lag 1 frame */
	BKE_image_update_frame(bmain, sce->r.cfra);
	
	/* rebuild rigid body worlds before doing the actual frame update
	 * this needs to be done on start frame but animation playback usually starts one frame later
	 * we need to do it here to avoid rebuilding the world on every simulation change, which can be very expensive
	 */
	scene_rebuild_rbw_recursive(sce, ctime);

	sound_set_cfra(sce->r.cfra);
	
	/* clear animation overrides */
	/* XXX TODO... */

	for (sce_iter = sce; sce_iter; sce_iter = sce_iter->set)
		DAG_scene_relations_update(bmain, sce_iter);

	/* flush recalc flags to dependencies, if we were only changing a frame
	 * this would not be necessary, but if a user or a script has modified
	 * some datablock before BKE_scene_update_tagged was called, we need the flush */
	DAG_ids_flush_tagged(bmain);

	/* Following 2 functions are recursive
	 * so don't call within 'scene_update_tagged_recursive' */
	DAG_scene_update_flags(bmain, sce, lay, true, do_invisible_flush);   // only stuff that moves or needs display still

	BKE_mask_evaluate_all_masks(bmain, ctime, true);

	/* All 'standard' (i.e. without any dependencies) animation is handled here,
	 * with an 'local' to 'macro' order of evaluation. This should ensure that
	 * settings stored nestled within a hierarchy (i.e. settings in a Texture block
	 * can be overridden by settings from Scene, which owns the Texture through a hierarchy
	 * such as Scene->World->MTex/Texture) can still get correctly overridden.
	 */
	BKE_animsys_evaluate_all_animation(bmain, sce, ctime);
	/*...done with recursive funcs */

	/* clear "LIB_DOIT" flag from all materials, to prevent infinite recursion problems later 
	 * when trying to find materials with drivers that need evaluating [#32017] 
	 */
	BKE_main_id_tag_idcode(bmain, ID_MA, false);
	BKE_main_id_tag_idcode(bmain, ID_LA, false);

	/* run rigidbody sim */
	/* NOTE: current position is so that rigidbody sim affects other objects, might change in the future */
	scene_do_rb_simulation_recursive(sce, ctime);

	/* BKE_object_handle_update() on all objects, groups and sets */
	scene_update_tagged_recursive(eval_ctx, bmain, sce, sce);
	/* update sound system animation (TODO, move to depsgraph) */
	sound_update_scene(bmain, sce);

	scene_depsgraph_hack(eval_ctx, sce, sce);

	/* notify editors and python about recalc */
	BLI_callback_exec(bmain, &sce->id, BLI_CB_EVT_SCENE_UPDATE_POST);
	BLI_callback_exec(bmain, &sce->id, BLI_CB_EVT_FRAME_CHANGE_POST);

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
		return 0;
	}
	else if ( (scene->r.layers.first == scene->r.layers.last) &&
	          (scene->r.layers.first == srl))
	{
		/* ensure 1 layer is kept */
		return 0;
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

	return 1;
}

/* render simplification */

int get_render_subsurf_level(RenderData *r, int lvl)
{
	if (r->mode & R_SIMPLIFY)
		return min_ii(r->simplify_subsurf, lvl);
	else
		return lvl;
}

int get_render_child_particle_number(RenderData *r, int num)
{
	if (r->mode & R_SIMPLIFY)
		return (int)(r->simplify_particles * num);
	else
		return num;
}

int get_render_shadow_samples(RenderData *r, int samples)
{
	if ((r->mode & R_SIMPLIFY) && samples > 0)
		return min_ii(r->simplify_shadowsamples, samples);
	else
		return samples;
}

float get_render_aosss_error(RenderData *r, float error)
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

bool BKE_scene_use_new_shading_nodes(Scene *scene)
{
	RenderEngineType *type = RE_engines_find(scene->r.engine);
	return (type && type->flag & RE_USE_SHADING_NODES);
}

bool BKE_scene_uses_blender_internal(struct Scene *scene)
{
	return strcmp("BLENDER_RENDER", scene->r.engine) == 0;
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
	return strcmp(scene->display_settings.display_device, "None") != 0;
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
		case B_UNIT_CAMERA:
			return value * (double)unit->scale_length;
		case B_UNIT_AREA:
			return value * pow(unit->scale_length, 2);
		case B_UNIT_VOLUME:
			return value * pow(unit->scale_length, 3);
		case B_UNIT_MASS:
			return value * pow(unit->scale_length, 3);
		default:
			return value;
	}
}
