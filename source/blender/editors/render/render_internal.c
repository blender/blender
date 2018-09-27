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
 * The Original Code is Copyright (C) 2008 Blender Foundation.
 * All rights reserved.
 *
 *
 * ***** END GPL LICENSE BLOCK *****
 */

/** \file blender/editors/render/render_internal.c
 *  \ingroup edrend
 */


#include <math.h>
#include <string.h>
#include <stddef.h>

#include "MEM_guardedalloc.h"

#include "BLI_listbase.h"
#include "BLI_rect.h"
#include "BLI_timecode.h"
#include "BLI_math.h"
#include "BLI_threads.h"
#include "BLI_utildefines.h"

#include "PIL_time.h"

#include "BLT_translation.h"

#include "DNA_object_types.h"
#include "DNA_scene_types.h"
#include "DNA_view3d_types.h"
#include "DNA_userdef_types.h"

#include "BKE_blender_undo.h"
#include "BKE_blender_version.h"
#include "BKE_camera.h"
#include "BKE_context.h"
#include "BKE_colortools.h"
#include "BKE_depsgraph.h"
#include "BKE_global.h"
#include "BKE_image.h"
#include "BKE_library.h"
#include "BKE_main.h"
#include "BKE_node.h"
#include "BKE_object.h"
#include "BKE_report.h"
#include "BKE_sequencer.h"
#include "BKE_screen.h"
#include "BKE_scene.h"
#include "BKE_undo_system.h"

#include "WM_api.h"
#include "WM_types.h"

#include "ED_object.h"
#include "ED_render.h"
#include "ED_screen.h"
#include "ED_util.h"
#include "ED_undo.h"
#include "ED_view3d.h"

#include "RE_pipeline.h"
#include "RE_engine.h"

#include "IMB_colormanagement.h"
#include "IMB_imbuf_types.h"


#include "BIF_gl.h"
#include "BIF_glutil.h"

#include "RNA_access.h"
#include "RNA_define.h"

#include "BLO_undofile.h"

#include "render_intern.h"

/* Render Callbacks */
static int render_break(void *rjv);

typedef struct RenderJob {
	Main *main;
	Scene *scene;
	Scene *current_scene;
	Render *re;
	SceneRenderLayer *srl;
	struct Object *camera_override;
	int lay_override;
	bool v3d_override;
	bool anim, write_still;
	Image *image;
	ImageUser iuser;
	bool image_outdated;
	short *stop;
	short *do_update;
	float *progress;
	ReportList *reports;
	int orig_layer;
	int last_layer;
	ScrArea *sa;
	ColorManagedViewSettings view_settings;
	ColorManagedDisplaySettings display_settings;
	bool supports_glsl_draw;
	bool interface_locked;
} RenderJob;

/* called inside thread! */
static void image_buffer_rect_update(RenderJob *rj, RenderResult *rr, ImBuf *ibuf, ImageUser *iuser, volatile rcti *renrect, const char *viewname)
{
	Scene *scene = rj->scene;
	const float *rectf = NULL;
	int ymin, ymax, xmin, xmax;
	int rymin, rxmin;
	int linear_stride, linear_offset_x, linear_offset_y;
	ColorManagedViewSettings *view_settings;
	ColorManagedDisplaySettings *display_settings;

	/* Exception for exr tiles -- display buffer conversion happens here,
	 * NOT in the color management pipeline.
	 */
	if (ibuf->userflags & IB_DISPLAY_BUFFER_INVALID &&
	    rr->do_exr_tile == false)
	{
		/* The whole image buffer it so be color managed again anyway. */
		return;
	}

	/* if renrect argument, we only refresh scanlines */
	if (renrect) {
		/* if (ymax == recty), rendering of layer is ready, we should not draw, other things happen... */
		if (rr->renlay == NULL || renrect->ymax >= rr->recty)
			return;

		/* xmin here is first subrect x coord, xmax defines subrect width */
		xmin = renrect->xmin + rr->crop;
		xmax = renrect->xmax - xmin + rr->crop;
		if (xmax < 2)
			return;

		ymin = renrect->ymin + rr->crop;
		ymax = renrect->ymax - ymin + rr->crop;
		if (ymax < 2)
			return;
		renrect->ymin = renrect->ymax;

	}
	else {
		xmin = ymin = rr->crop;
		xmax = rr->rectx - 2 * rr->crop;
		ymax = rr->recty - 2 * rr->crop;
	}

	/* xmin ymin is in tile coords. transform to ibuf */
	rxmin = rr->tilerect.xmin + xmin;
	if (rxmin >= ibuf->x) return;
	rymin = rr->tilerect.ymin + ymin;
	if (rymin >= ibuf->y) return;

	if (rxmin + xmax > ibuf->x)
		xmax = ibuf->x - rxmin;
	if (rymin + ymax > ibuf->y)
		ymax = ibuf->y - rymin;

	if (xmax < 1 || ymax < 1) return;

	/* The thing here is, the logic below (which was default behavior
	 * of how rectf is acquiring since forever) gives float buffer for
	 * composite output only. This buffer can not be used for other
	 * passes obviously.
	 *
	 * We might try finding corresponding for pass buffer in render result
	 * (which is actually missing when rendering with Cycles, who only
	 * writes all the passes when the tile is finished) or use float
	 * buffer from image buffer as reference, which is easier to use and
	 * contains all the data we need anyway.
	 *                                              - sergey -
	 */
	/* TODO(sergey): Need to check has_combined here? */
	if (iuser->pass == 0) {
		RenderView *rv;
		const int view_id = BKE_scene_multiview_view_id_get(&scene->r, viewname);
		rv = RE_RenderViewGetById(rr, view_id);

		/* find current float rect for display, first case is after composite... still weak */
		if (rv->rectf)
			rectf = rv->rectf;
		else {
			if (rv->rect32) {
				/* special case, currently only happens with sequencer rendering,
				 * which updates the whole frame, so we can only mark display buffer
				 * as invalid here (sergey)
				 */
				ibuf->userflags |= IB_DISPLAY_BUFFER_INVALID;
				return;
			}
			else {
				if (rr->renlay == NULL) return;
				rectf = RE_RenderLayerGetPass(rr->renlay, RE_PASSNAME_COMBINED, viewname);
			}
		}
		if (rectf == NULL) return;

		rectf += 4 * (rr->rectx * ymin + xmin);
		linear_stride = rr->rectx;
		linear_offset_x = rxmin;
		linear_offset_y = rymin;
	}
	else {
		rectf = ibuf->rect_float;
		linear_stride = ibuf->x;
		linear_offset_x = 0;
		linear_offset_y = 0;
	}

	if (rr->do_exr_tile) {
		/* We don't support changing color management settings during rendering
		 * when using Save Buffers option.
		 */
		view_settings = &rj->view_settings;
		display_settings = &rj->display_settings;
	}
	else {
		view_settings = &scene->view_settings;
		display_settings = &scene->display_settings;
	}

	IMB_partial_display_buffer_update(ibuf, rectf, NULL,
	                                  linear_stride, linear_offset_x, linear_offset_y,
	                                  view_settings, display_settings,
	                                  rxmin, rymin, rxmin + xmax, rymin + ymax,
	                                  rr->do_exr_tile);
}

/* ****************************** render invoking ***************** */

/* set callbacks, exported to sequence render too.
 * Only call in foreground (UI) renders. */

static void screen_render_scene_layer_set(wmOperator *op, Main *mainp, Scene **scene, SceneRenderLayer **srl)
{
	/* single layer re-render */
	if (RNA_struct_property_is_set(op->ptr, "scene")) {
		Scene *scn;
		char scene_name[MAX_ID_NAME - 2];

		RNA_string_get(op->ptr, "scene", scene_name);
		scn = (Scene *)BLI_findstring(&mainp->scene, scene_name, offsetof(ID, name) + 2);

		if (scn) {
			/* camera switch wont have updated */
			scn->r.cfra = (*scene)->r.cfra;
			BKE_scene_camera_switch_update(scn);

			*scene = scn;
		}
	}

	if (RNA_struct_property_is_set(op->ptr, "layer")) {
		SceneRenderLayer *rl;
		char rl_name[RE_MAXNAME];

		RNA_string_get(op->ptr, "layer", rl_name);
		rl = (SceneRenderLayer *)BLI_findstring(&(*scene)->r.layers, rl_name, offsetof(SceneRenderLayer, name));

		if (rl)
			*srl = rl;
	}
}

/* executes blocking render */
static int screen_render_exec(bContext *C, wmOperator *op)
{
	Scene *scene = CTX_data_scene(C);
	SceneRenderLayer *srl = NULL;
	Render *re;
	Image *ima;
	View3D *v3d = CTX_wm_view3d(C);
	Main *mainp = CTX_data_main(C);
	unsigned int lay_override;
	const bool is_animation = RNA_boolean_get(op->ptr, "animation");
	const bool is_write_still = RNA_boolean_get(op->ptr, "write_still");
	struct Object *camera_override = v3d ? V3D_CAMERA_LOCAL(v3d) : NULL;

	/* custom scene and single layer re-render */
	screen_render_scene_layer_set(op, mainp, &scene, &srl);

	if (!is_animation && is_write_still && BKE_imtype_is_movie(scene->r.im_format.imtype)) {
		BKE_report(op->reports, RPT_ERROR, "Cannot write a single file with an animation format selected");
		return OPERATOR_CANCELLED;
	}

	re = RE_NewSceneRender(scene);
	lay_override = (v3d && v3d->lay != scene->lay) ? v3d->lay : 0;

	G.is_break = false;
	RE_test_break_cb(re, NULL, render_break);

	ima = BKE_image_verify_viewer(mainp, IMA_TYPE_R_RESULT, "Render Result");
	BKE_image_signal(mainp, ima, NULL, IMA_SIGNAL_FREE);
	BKE_image_backup_render(scene, ima, true);

	/* cleanup sequencer caches before starting user triggered render.
	 * otherwise, invalidated cache entries can make their way into
	 * the output rendering. We can't put that into RE_BlenderFrame,
	 * since sequence rendering can call that recursively... (peter) */
	BKE_sequencer_cache_cleanup();

	RE_SetReports(re, op->reports);

	BLI_threaded_malloc_begin();
	if (is_animation)
		RE_BlenderAnim(re, mainp, scene, camera_override, lay_override, scene->r.sfra, scene->r.efra, scene->r.frame_step);
	else
		RE_BlenderFrame(re, mainp, scene, srl, camera_override, lay_override, scene->r.cfra, is_write_still);
	BLI_threaded_malloc_end();

	RE_SetReports(re, NULL);

	// no redraw needed, we leave state as we entered it
	ED_update_for_newframe(mainp, scene, 1);

	WM_event_add_notifier(C, NC_SCENE | ND_RENDER_RESULT, scene);

	return OPERATOR_FINISHED;
}

static void render_freejob(void *rjv)
{
	RenderJob *rj = rjv;

	BKE_color_managed_view_settings_free(&rj->view_settings);
	MEM_freeN(rj);
}

/* str is IMA_MAX_RENDER_TEXT in size */
static void make_renderinfo_string(const RenderStats *rs,
                                   const Scene *scene,
                                   const bool v3d_override,
                                   const char *error,
                                   char *str)
{
	char info_time_str[32]; // used to be extern to header_info.c
	uintptr_t mem_in_use, mmap_in_use, peak_memory;
	float megs_used_memory, mmap_used_memory, megs_peak_memory;
	char *spos = str;

	mem_in_use = MEM_get_memory_in_use();
	mmap_in_use = MEM_get_mapped_memory_in_use();
	peak_memory = MEM_get_peak_memory();

	megs_used_memory = (mem_in_use - mmap_in_use) / (1024.0 * 1024.0);
	mmap_used_memory = (mmap_in_use) / (1024.0 * 1024.0);
	megs_peak_memory = (peak_memory) / (1024.0 * 1024.0);

	/* local view */
	if (rs->localview)
		spos += sprintf(spos, "%s | ", IFACE_("3D Local View"));
	else if (v3d_override)
		spos += sprintf(spos, "%s | ", IFACE_("3D View"));

	/* frame number */
	spos += sprintf(spos, IFACE_("Frame:%d "), (scene->r.cfra));

	/* previous and elapsed time */
	BLI_timecode_string_from_time_simple(info_time_str, sizeof(info_time_str), rs->lastframetime);

	if (rs->infostr && rs->infostr[0]) {
		if (rs->lastframetime != 0.0)
			spos += sprintf(spos, IFACE_("| Last:%s "), info_time_str);
		else
			spos += sprintf(spos, "| ");

		BLI_timecode_string_from_time_simple(info_time_str, sizeof(info_time_str), PIL_check_seconds_timer() - rs->starttime);
	}
	else
		spos += sprintf(spos, "| ");

	spos += sprintf(spos, IFACE_("Time:%s "), info_time_str);

	/* statistics */
	if (rs->statstr) {
		if (rs->statstr[0]) {
			spos += sprintf(spos, "| %s ", rs->statstr);
		}
	}
	else {
		if (rs->totvert || rs->totface || rs->tothalo || rs->totstrand || rs->totlamp)
			spos += sprintf(spos, "| ");

		if (rs->totvert) spos += sprintf(spos, IFACE_("Ve:%d "), rs->totvert);
		if (rs->totface) spos += sprintf(spos, IFACE_("Fa:%d "), rs->totface);
		if (rs->tothalo) spos += sprintf(spos, IFACE_("Ha:%d "), rs->tothalo);
		if (rs->totstrand) spos += sprintf(spos, IFACE_("St:%d "), rs->totstrand);
		if (rs->totlamp) spos += sprintf(spos, IFACE_("La:%d "), rs->totlamp);

		if (rs->mem_peak == 0.0f)
			spos += sprintf(spos, IFACE_("| Mem:%.2fM (%.2fM, Peak %.2fM) "),
			                megs_used_memory, mmap_used_memory, megs_peak_memory);
		else
			spos += sprintf(spos, IFACE_("| Mem:%.2fM, Peak: %.2fM "), rs->mem_used, rs->mem_peak);

		if (rs->curfield)
			spos += sprintf(spos, IFACE_("Field %d "), rs->curfield);
		if (rs->curblur)
			spos += sprintf(spos, IFACE_("Blur %d "), rs->curblur);
	}

	/* full sample */
	if (rs->curfsa)
		spos += sprintf(spos, IFACE_("| Full Sample %d "), rs->curfsa);

	/* extra info */
	if (rs->infostr && rs->infostr[0]) {
		spos += sprintf(spos, "| %s ", rs->infostr);
	}
	else if (error && error[0]) {
		spos += sprintf(spos, "| %s ", error);
	}

	/* very weak... but 512 characters is quite safe */
	if (spos >= str + IMA_MAX_RENDER_TEXT)
		if (G.debug & G_DEBUG)
			printf("WARNING! renderwin text beyond limit\n");

}

static void image_renderinfo_cb(void *rjv, RenderStats *rs)
{
	RenderJob *rj = rjv;
	RenderResult *rr;

	rr = RE_AcquireResultRead(rj->re);

	if (rr) {
		/* malloc OK here, stats_draw is not in tile threads */
		if (rr->text == NULL)
			rr->text = MEM_callocN(IMA_MAX_RENDER_TEXT, "rendertext");

		make_renderinfo_string(rs, rj->scene, rj->v3d_override,
		                       rr->error, rr->text);
	}

	RE_ReleaseResult(rj->re);

	/* make jobs timer to send notifier */
	*(rj->do_update) = true;

}

static void render_progress_update(void *rjv, float progress)
{
	RenderJob *rj = rjv;

	if (rj->progress && *rj->progress != progress) {
		*rj->progress = progress;

		/* make jobs timer to send notifier */
		*(rj->do_update) = true;
	}
}

/* Not totally reliable, but works fine in most of cases and
 * in worst case would just make it so extra color management
 * for the whole render result is applied (which was already
 * happening already).
 */
static void render_image_update_pass_and_layer(RenderJob *rj, RenderResult *rr, ImageUser *iuser)
{
	wmWindowManager *wm;
	ScrArea *first_sa = NULL, *matched_sa = NULL;

	/* image window, compo node users */
	for (wm = rj->main->wm.first; wm && matched_sa == NULL; wm = wm->id.next) { /* only 1 wm */
		wmWindow *win;
		for (win = wm->windows.first; win && matched_sa == NULL; win = win->next) {
			ScrArea *sa;
			for (sa = win->screen->areabase.first; sa; sa = sa->next) {
				if (sa->spacetype == SPACE_IMAGE) {
					SpaceImage *sima = sa->spacedata.first;
					// sa->spacedata might be empty when toggling fullscreen mode.
					if (sima != NULL && sima->image == rj->image) {
						if (first_sa == NULL) {
							first_sa = sa;
						}
						if (sa == rj->sa) {
							matched_sa = sa;
							break;
						}
					}
				}
			}
		}
	}

	if (matched_sa == NULL) {
		matched_sa = first_sa;
	}

	if (matched_sa) {
		SpaceImage *sima = matched_sa->spacedata.first;
		RenderResult *main_rr = RE_AcquireResultRead(rj->re);

		/* TODO(sergey): is there faster way to get the layer index? */
		if (rr->renlay) {
			int layer = BLI_findstringindex(&main_rr->layers,
			                                (char *)rr->renlay->name,
			                                offsetof(RenderLayer, name));
			sima->iuser.layer = layer;
			rj->last_layer = layer;
		}

		iuser->pass = sima->iuser.pass;
		iuser->layer = sima->iuser.layer;

		RE_ReleaseResult(rj->re);
	}
}

static void image_rect_update(void *rjv, RenderResult *rr, volatile rcti *renrect)
{
	RenderJob *rj = rjv;
	Image *ima = rj->image;
	ImBuf *ibuf;
	void *lock;
	const char *viewname = RE_GetActiveRenderView(rj->re);

	/* only update if we are displaying the slot being rendered */
	if (ima->render_slot != ima->last_render_slot) {
		rj->image_outdated = true;
		return;
	}
	else if (rj->image_outdated) {
		/* update entire render */
		rj->image_outdated = false;
		BKE_image_signal(rj->main, ima, NULL, IMA_SIGNAL_COLORMANAGE);
		*(rj->do_update) = true;
		return;
	}

	if (rr == NULL)
		return;

	/* update part of render */
	render_image_update_pass_and_layer(rj, rr, &rj->iuser);
	ibuf = BKE_image_acquire_ibuf(ima, &rj->iuser, &lock);
	if (ibuf) {
		/* Don't waste time on CPU side color management if
		 * image will be displayed using GLSL.
		 *
		 * Need to update rect if Save Buffers enabled because in
		 * this case GLSL doesn't have original float buffer to
		 * operate with.
		 */
		if (rr->do_exr_tile ||
		    !rj->supports_glsl_draw ||
		    ibuf->channels == 1 ||
		    U.image_draw_method != IMAGE_DRAW_METHOD_GLSL)
		{
			image_buffer_rect_update(rj, rr, ibuf, &rj->iuser, renrect, viewname);
		}

		/* make jobs timer to send notifier */
		*(rj->do_update) = true;
	}
	BKE_image_release_ibuf(ima, ibuf, lock);
}

static void current_scene_update(void *rjv, Scene *scene)
{
	RenderJob *rj = rjv;
	rj->current_scene = scene;
	rj->iuser.scene = scene;
}

static void render_startjob(void *rjv, short *stop, short *do_update, float *progress)
{
	RenderJob *rj = rjv;

	rj->stop = stop;
	rj->do_update = do_update;
	rj->progress = progress;

	RE_SetReports(rj->re, rj->reports);

	if (rj->anim)
		RE_BlenderAnim(rj->re, rj->main, rj->scene, rj->camera_override, rj->lay_override, rj->scene->r.sfra, rj->scene->r.efra, rj->scene->r.frame_step);
	else
		RE_BlenderFrame(rj->re, rj->main, rj->scene, rj->srl, rj->camera_override, rj->lay_override, rj->scene->r.cfra, rj->write_still);

	RE_SetReports(rj->re, NULL);
}

static void render_image_restore_layer(RenderJob *rj)
{
	wmWindowManager *wm;

	/* image window, compo node users */
	for (wm = rj->main->wm.first; wm; wm = wm->id.next) { /* only 1 wm */
		wmWindow *win;
		for (win = wm->windows.first; win; win = win->next) {
			ScrArea *sa;
			for (sa = win->screen->areabase.first; sa; sa = sa->next) {
				if (sa == rj->sa) {
					if (sa->spacetype == SPACE_IMAGE) {
						SpaceImage *sima = sa->spacedata.first;

						if (RE_HasSingleLayer(rj->re)) {
							/* For single layer renders keep the active layer
							 * visible, or show the compositing result. */
							RenderResult *rr = RE_AcquireResultRead(rj->re);
							if (RE_HasCombinedLayer(rr)) {
								sima->iuser.layer = 0;
							}
							RE_ReleaseResult(rj->re);
						}
						else {
							/* For multiple layer render, set back the layer
							 * that was set at the start of rendering. */
							sima->iuser.layer = rj->orig_layer;
						}
					}
					return;
				}
			}
		}
	}
}

static void render_endjob(void *rjv)
{
	RenderJob *rj = rjv;

	/* this render may be used again by the sequencer without the active 'Render' where the callbacks
	 * would be re-assigned. assign dummy callbacks to avoid referencing freed renderjobs bug [#24508] */
	RE_InitRenderCB(rj->re);

	if (rj->main != G.main)
		BKE_main_free(rj->main);

	/* else the frame will not update for the original value */
	if (rj->anim && !(rj->scene->r.scemode & R_NO_FRAME_UPDATE)) {
		/* possible this fails of loading new file while rendering */
		if (G.main->wm.first) {
			ED_update_for_newframe(G.main, rj->scene, 1);
		}
	}

	/* XXX above function sets all tags in nodes */
	ntreeCompositClearTags(rj->scene->nodetree);

	/* potentially set by caller */
	rj->scene->r.scemode &= ~R_NO_FRAME_UPDATE;

	if (rj->srl) {
		nodeUpdateID(rj->scene->nodetree, &rj->scene->id);
		WM_main_add_notifier(NC_NODE | NA_EDITED, rj->scene);
	}

	if (rj->sa) {
		render_image_restore_layer(rj);
	}

	/* XXX render stability hack */
	G.is_rendering = false;
	WM_main_add_notifier(NC_SCENE | ND_RENDER_RESULT, NULL);

	/* Partial render result will always update display buffer
	 * for first render layer only. This is nice because you'll
	 * see render progress during rendering, but it ends up in
	 * wrong display buffer shown after rendering.
	 *
	 * The code below will mark display buffer as invalid after
	 * rendering in case multiple layers were rendered, which
	 * ensures display buffer matches render layer after
	 * rendering.
	 *
	 * Perhaps proper way would be to toggle active render
	 * layer in image editor and job, so we always display
	 * layer being currently rendered. But this is not so much
	 * trivial at this moment, especially because of external
	 * engine API, so lets use simple and robust way for now
	 *                                          - sergey -
	 */
	if (rj->scene->r.layers.first != rj->scene->r.layers.last ||
	    rj->image_outdated)
	{
		void *lock;
		Image *ima = rj->image;
		ImBuf *ibuf = BKE_image_acquire_ibuf(ima, &rj->iuser, &lock);

		if (ibuf)
			ibuf->userflags |= IB_DISPLAY_BUFFER_INVALID;

		BKE_image_release_ibuf(ima, ibuf, lock);
	}

	/* Finally unlock the user interface (if it was locked). */
	if (rj->interface_locked) {
		Scene *scene;

		/* Interface was locked, so window manager couldn't have been changed
		 * and using one from Global will unlock exactly the same manager as
		 * was locked before running the job.
		 */
		WM_set_locked_interface(G.main->wm.first, false);

		/* We've freed all the derived caches before rendering, which is
		 * effectively the same as if we re-loaded the file.
		 *
		 * So let's not try being smart here and just reset all updated
		 * scene layers and use generic DAG_on_visible_update.
		 */
		for (scene = G.main->scene.first; scene; scene = scene->id.next) {
			scene->lay_updated = 0;
		}

		DAG_on_visible_update(G.main, false);
	}
}

/* called by render, check job 'stop' value or the global */
static int render_breakjob(void *rjv)
{
	RenderJob *rj = rjv;

	if (G.is_break)
		return 1;
	if (rj->stop && *(rj->stop))
		return 1;
	return 0;
}

/* for exec() when there is no render job
 * note: this wont check for the escape key being pressed, but doing so isnt threadsafe */
static int render_break(void *UNUSED(rjv))
{
	if (G.is_break)
		return 1;
	return 0;
}

/* runs in thread, no cursor setting here works. careful with notifiers too (malloc conflicts) */
/* maybe need a way to get job send notifier? */
static void render_drawlock(void *rjv, int lock)
{
	RenderJob *rj = rjv;

	/* If interface is locked, renderer callback shall do nothing. */
	if (!rj->interface_locked) {
		BKE_spacedata_draw_locks(lock);
	}
}

/* catch esc */
static int screen_render_modal(bContext *C, wmOperator *op, const wmEvent *event)
{
	Scene *scene = (Scene *) op->customdata;

	/* no running blender, remove handler and pass through */
	if (0 == WM_jobs_test(CTX_wm_manager(C), scene, WM_JOB_TYPE_RENDER)) {
		return OPERATOR_FINISHED | OPERATOR_PASS_THROUGH;
	}

	/* running render */
	switch (event->type) {
		case ESCKEY:
			return OPERATOR_RUNNING_MODAL;
	}
	return OPERATOR_PASS_THROUGH;
}

static void screen_render_cancel(bContext *C, wmOperator *op)
{
	wmWindowManager *wm = CTX_wm_manager(C);
	Scene *scene = (Scene *) op->customdata;

	/* kill on cancel, because job is using op->reports */
	WM_jobs_kill_type(wm, scene, WM_JOB_TYPE_RENDER);
}

static void clean_viewport_memory(Main *bmain, Scene *scene, int renderlay)
{
	Object *object;
	Scene *sce_iter;
	Base *base;

	for (object = bmain->object.first; object; object = object->id.next) {
		object->id.tag |= LIB_TAG_DOIT;
	}

	for (SETLOOPER(scene, sce_iter, base)) {
		if ((base->lay & renderlay) == 0) {
			continue;
		}
		if (RE_allow_render_generic_object(base->object)) {
			base->object->id.tag &= ~LIB_TAG_DOIT;
		}
	}

	for (SETLOOPER(scene, sce_iter, base)) {
		object = base->object;
		if ((object->id.tag & LIB_TAG_DOIT) == 0) {
			continue;
		}
		object->id.tag &= ~LIB_TAG_DOIT;

		BKE_object_free_derived_caches(object);
	}
}

/* using context, starts job */
static int screen_render_invoke(bContext *C, wmOperator *op, const wmEvent *event)
{
	/* new render clears all callbacks */
	Main *mainp;
	Scene *scene = CTX_data_scene(C);
	SceneRenderLayer *srl = NULL;
	Render *re;
	wmJob *wm_job;
	RenderJob *rj;
	Image *ima;
	int jobflag;
	const bool is_animation = RNA_boolean_get(op->ptr, "animation");
	const bool is_write_still = RNA_boolean_get(op->ptr, "write_still");
	const bool use_viewport = RNA_boolean_get(op->ptr, "use_viewport");
	View3D *v3d = use_viewport ? CTX_wm_view3d(C) : NULL;
	struct Object *camera_override = v3d ? V3D_CAMERA_LOCAL(v3d) : NULL;
	const char *name;
	ScrArea *sa;

	/* only one render job at a time */
	if (WM_jobs_test(CTX_wm_manager(C), scene, WM_JOB_TYPE_RENDER))
		return OPERATOR_CANCELLED;

	if (RE_force_single_renderlayer(scene))
		WM_event_add_notifier(C, NC_SCENE | ND_RENDER_OPTIONS, NULL);

	if (!RE_is_rendering_allowed(scene, camera_override, op->reports)) {
		return OPERATOR_CANCELLED;
	}

	if (!is_animation && is_write_still && BKE_imtype_is_movie(scene->r.im_format.imtype)) {
		BKE_report(op->reports, RPT_ERROR, "Cannot write a single file with an animation format selected");
		return OPERATOR_CANCELLED;
	}

	/* stop all running jobs, except screen one. currently previews frustrate Render */
	WM_jobs_kill_all_except(CTX_wm_manager(C), CTX_wm_screen(C));

	/* get main */
	if (G.debug_value == 101) {
		/* thread-safety experiment, copy main from the undo buffer */
		struct MemFile *memfile = ED_undosys_stack_memfile_get_active(CTX_wm_manager(C)->undo_stack);
		mainp = BLO_memfile_main_get(memfile, CTX_data_main(C), &scene);
	}
	else
		mainp = CTX_data_main(C);

	/* cancel animation playback */
	if (ED_screen_animation_playing(CTX_wm_manager(C)))
		ED_screen_animation_play(C, 0, 0);

	/* handle UI stuff */
	WM_cursor_wait(1);

	/* flush sculpt and editmode changes */
	ED_editors_flush_edits(C, true);

	/* cleanup sequencer caches before starting user triggered render.
	 * otherwise, invalidated cache entries can make their way into
	 * the output rendering. We can't put that into RE_BlenderFrame,
	 * since sequence rendering can call that recursively... (peter) */
	BKE_sequencer_cache_cleanup();

	// store spare
	// get view3d layer, local layer, make this nice api call to render
	// store spare

	/* ensure at least 1 area shows result */
	sa = render_view_open(C, event->x, event->y, op->reports);

	jobflag = WM_JOB_EXCL_RENDER | WM_JOB_PRIORITY | WM_JOB_PROGRESS;

	/* custom scene and single layer re-render */
	screen_render_scene_layer_set(op, mainp, &scene, &srl);

	if (RNA_struct_property_is_set(op->ptr, "layer"))
		jobflag |= WM_JOB_SUSPEND;

	/* job custom data */
	rj = MEM_callocN(sizeof(RenderJob), "render job");
	rj->main = mainp;
	rj->scene = scene;
	rj->current_scene = rj->scene;
	rj->srl = srl;
	rj->camera_override = camera_override;
	rj->lay_override = 0;
	rj->anim = is_animation;
	rj->write_still = is_write_still && !is_animation;
	rj->iuser.scene = scene;
	rj->iuser.ok = 1;
	rj->reports = op->reports;
	rj->orig_layer = 0;
	rj->last_layer = 0;
	rj->sa = sa;
	rj->supports_glsl_draw = IMB_colormanagement_support_glsl_draw(&scene->view_settings);

	BKE_color_managed_display_settings_copy(&rj->display_settings, &scene->display_settings);
	BKE_color_managed_view_settings_copy(&rj->view_settings, &scene->view_settings);

	if (sa) {
		SpaceImage *sima = sa->spacedata.first;
		rj->orig_layer = sima->iuser.layer;
	}

	if (v3d) {
		if (scene->lay != v3d->lay) {
			rj->lay_override = v3d->lay;
			rj->v3d_override = true;
		}
		else if (camera_override && camera_override != scene->camera)
			rj->v3d_override = true;

		if (v3d->localvd)
			rj->lay_override |= v3d->localvd->lay;
	}

	/* Lock the user interface depending on render settings. */
	if (scene->r.use_lock_interface) {
		int renderlay = rj->lay_override ? rj->lay_override : scene->lay;

		WM_set_locked_interface(CTX_wm_manager(C), true);

		/* Set flag interface need to be unlocked.
		 *
		 * This is so because we don't have copy of render settings
		 * accessible from render job and copy is needed in case
		 * of non-locked rendering, so we wouldn't try to unlock
		 * anything if option was initially unset but then was
		 * enabled during rendering.
		 */
		rj->interface_locked = true;

		/* Clean memory used by viewport? */
		clean_viewport_memory(rj->main, scene, renderlay);
	}

	/* setup job */
	if (RE_seq_render_active(scene, &scene->r)) name = "Sequence Render";
	else name = "Render";

	wm_job = WM_jobs_get(CTX_wm_manager(C), CTX_wm_window(C), scene, name, jobflag, WM_JOB_TYPE_RENDER);
	WM_jobs_customdata_set(wm_job, rj, render_freejob);
	WM_jobs_timer(wm_job, 0.2, NC_SCENE | ND_RENDER_RESULT, 0);
	WM_jobs_callbacks(wm_job, render_startjob, NULL, NULL, render_endjob);

	/* get a render result image, and make sure it is empty */
	ima = BKE_image_verify_viewer(mainp, IMA_TYPE_R_RESULT, "Render Result");
	BKE_image_signal(rj->main, ima, NULL, IMA_SIGNAL_FREE);
	BKE_image_backup_render(rj->scene, ima, true);
	rj->image = ima;

	/* setup new render */
	re = RE_NewSceneRender(scene);
	RE_test_break_cb(re, rj, render_breakjob);
	RE_draw_lock_cb(re, rj, render_drawlock);
	RE_display_update_cb(re, rj, image_rect_update);
	RE_current_scene_update_cb(re, rj, current_scene_update);
	RE_stats_draw_cb(re, rj, image_renderinfo_cb);
	RE_progress_cb(re, rj, render_progress_update);

	rj->re = re;
	G.is_break = false;

	/* store actual owner of job, so modal operator could check for it,
	 * the reason of this is that active scene could change when rendering
	 * several layers from compositor [#31800]
	 */
	op->customdata = scene;

	WM_jobs_start(CTX_wm_manager(C), wm_job);

	WM_cursor_wait(0);
	WM_event_add_notifier(C, NC_SCENE | ND_RENDER_RESULT, scene);

	/* we set G.is_rendering here already instead of only in the job, this ensure
	 * main loop or other scene updates are disabled in time, since they may
	 * have started before the job thread */
	G.is_rendering = true;

	/* add modal handler for ESC */
	WM_event_add_modal_handler(C, op);

	return OPERATOR_RUNNING_MODAL;
}

/* contextual render, using current scene, view3d? */
void RENDER_OT_render(wmOperatorType *ot)
{
	PropertyRNA *prop;

	/* identifiers */
	ot->name = "Render";
	ot->description = "Render active scene";
	ot->idname = "RENDER_OT_render";

	/* api callbacks */
	ot->invoke = screen_render_invoke;
	ot->modal = screen_render_modal;
	ot->cancel = screen_render_cancel;
	ot->exec = screen_render_exec;

	/*ot->poll = ED_operator_screenactive;*/ /* this isn't needed, causes failer in background mode */

	prop = RNA_def_boolean(ot->srna, "animation", 0, "Animation", "Render files from the animation range of this scene");
	RNA_def_property_flag(prop, PROP_SKIP_SAVE);
	RNA_def_boolean(ot->srna, "write_still", 0, "Write Image", "Save rendered the image to the output path (used only when animation is disabled)");
	prop = RNA_def_boolean(ot->srna, "use_viewport", 0, "Use 3D Viewport", "When inside a 3D viewport, use layers and camera of the viewport");
	RNA_def_property_flag(prop, PROP_SKIP_SAVE);
	prop = RNA_def_string(ot->srna, "layer", NULL, RE_MAXNAME, "Render Layer", "Single render layer to re-render (used only when animation is disabled)");
	RNA_def_property_flag(prop, PROP_SKIP_SAVE);
	prop = RNA_def_string(ot->srna, "scene", NULL, MAX_ID_NAME - 2, "Scene", "Scene to render, current scene if not specified");
	RNA_def_property_flag(prop, PROP_SKIP_SAVE);
}


/* ************** preview for 3d viewport ***************** */

#define PR_UPDATE_VIEW				1
#define PR_UPDATE_RENDERSIZE		2
#define PR_UPDATE_MATERIAL			4
#define PR_UPDATE_DATABASE			8

typedef struct RenderPreview {
	/* from wmJob */
	void *owner;
	short *stop, *do_update;
	wmJob *job;

	Scene *scene;
	ScrArea *sa;
	ARegion *ar;
	View3D *v3d;
	RegionView3D *rv3d;
	Main *bmain;
	RenderEngine *engine;

	float viewmat[4][4];

	int start_resolution_divider;
	int resolution_divider;
	bool has_freestyle;
} RenderPreview;

static int render_view3d_disprect(Scene *scene, ARegion *ar, View3D *v3d, RegionView3D *rv3d, rcti *disprect)
{
	/* copied code from view3d_draw.c */
	rctf viewborder;
	int draw_border;

	if (rv3d->persp == RV3D_CAMOB)
		draw_border = (scene->r.mode & R_BORDER) != 0;
	else
		draw_border = (v3d->flag2 & V3D_RENDER_BORDER) != 0;

	if (draw_border) {
		if (rv3d->persp == RV3D_CAMOB) {
			ED_view3d_calc_camera_border(scene, ar, v3d, rv3d, &viewborder, false);

			disprect->xmin = viewborder.xmin + scene->r.border.xmin * BLI_rctf_size_x(&viewborder);
			disprect->ymin = viewborder.ymin + scene->r.border.ymin * BLI_rctf_size_y(&viewborder);
			disprect->xmax = viewborder.xmin + scene->r.border.xmax * BLI_rctf_size_x(&viewborder);
			disprect->ymax = viewborder.ymin + scene->r.border.ymax * BLI_rctf_size_y(&viewborder);
		}
		else {
			disprect->xmin = v3d->render_border.xmin * ar->winx;
			disprect->xmax = v3d->render_border.xmax * ar->winx;
			disprect->ymin = v3d->render_border.ymin * ar->winy;
			disprect->ymax = v3d->render_border.ymax * ar->winy;
		}

		return 1;
	}

	BLI_rcti_init(disprect, 0, 0, 0, 0);
	return 0;
}

/* returns true if OK  */
static bool render_view3d_get_rects(ARegion *ar, View3D *v3d, RegionView3D *rv3d, rctf *viewplane, RenderEngine *engine,
                                    float *r_clipsta, float *r_clipend, float *r_pixsize, bool *r_ortho)
{

	if (ar->winx < 4 || ar->winy < 4) return false;

	*r_ortho = ED_view3d_viewplane_get(v3d, rv3d, ar->winx, ar->winy, viewplane, r_clipsta, r_clipend, r_pixsize);

	engine->resolution_x = ar->winx;
	engine->resolution_y = ar->winy;

	return true;
}

static bool render_view3d_is_valid(RenderPreview *rp)
{
	return (rp->rv3d->render_engine != NULL);
}

/* called by renderer, checks job value */
static int render_view3d_break(void *rpv)
{
	RenderPreview *rp = rpv;

	if (G.is_break)
		return 1;

	/* during render, rv3d->engine can get freed */
	if (render_view3d_is_valid(rp) == false) {
		*rp->stop = 1;
	}

	return *(rp->stop);
}

static void render_view3d_display_update(void *rpv, RenderResult *UNUSED(rr), volatile struct rcti *UNUSED(rect))
{
	RenderPreview *rp = rpv;

	*(rp->do_update) = true;
}

static void render_view3d_renderinfo_cb(void *rjp, RenderStats *rs)
{
	RenderPreview *rp = rjp;

	/* during render, rv3d->engine can get freed */
	if (rp->rv3d->render_engine == NULL) {
		*rp->stop = 1;
	}
	else {
		make_renderinfo_string(rs, rp->scene, false, NULL, rp->engine->text);

		/* make jobs timer to send notifier */
		*(rp->do_update) = true;
	}
}

BLI_INLINE void rcti_scale_coords(rcti *scaled_rect, const rcti *rect,
                                  const float scale)
{
	scaled_rect->xmin = rect->xmin * scale;
	scaled_rect->ymin = rect->ymin * scale;
	scaled_rect->xmax = rect->xmax * scale;
	scaled_rect->ymax = rect->ymax * scale;
}

static void render_update_resolution(Render *re, const RenderPreview *rp,
                                     bool use_border, const rcti *clip_rect)
{
	int winx = rp->ar->winx / rp->resolution_divider;
	int winy = rp->ar->winy / rp->resolution_divider;
	if (use_border) {
		rcti scaled_cliprct;
		rcti_scale_coords(&scaled_cliprct, clip_rect,
		                  1.0f / rp->resolution_divider);
		RE_ChangeResolution(re, winx, winy, &scaled_cliprct);
	}
	else {
		RE_ChangeResolution(re, winx, winy, NULL);
	}

	if (rp->has_freestyle) {
		if (rp->resolution_divider == BKE_render_preview_pixel_size(&rp->scene->r)) {
			RE_ChangeModeFlag(re, R_EDGE_FRS, false);
		}
		else {
			RE_ChangeModeFlag(re, R_EDGE_FRS, true);
		}
	}
}

static void render_view3d_startjob(void *customdata, short *stop, short *do_update, float *UNUSED(progress))
{
	RenderPreview *rp = customdata;
	Render *re;
	RenderStats *rstats;
	rctf viewplane;
	rcti cliprct;
	float clipsta, clipend, pixsize;
	bool orth, restore = 0;
	char name[32];
	int update_flag;
	bool use_border;
	int ob_inst_update_flag = 0;

	update_flag = rp->engine->job_update_flag;
	rp->engine->job_update_flag = 0;

	//printf("ma %d res %d view %d db %d\n", update_flag & PR_UPDATE_MATERIAL, update_flag & PR_UPDATE_RENDERSIZE, update_flag & PR_UPDATE_VIEW, update_flag & PR_UPDATE_DATABASE);

	G.is_break = false;

	if (false == render_view3d_get_rects(rp->ar, rp->v3d, rp->rv3d, &viewplane, rp->engine, &clipsta, &clipend, &pixsize, &orth))
		return;

	rp->stop = stop;
	rp->do_update = do_update;

	// printf("Enter previewrender\n");

	/* ok, are we rendering all over? */
	sprintf(name, "View3dPreview %p", (void *)rp->ar);
	re = rp->engine->re = RE_GetRender(name);

	/* set this always, rp is different for each job */
	RE_test_break_cb(re, rp, render_view3d_break);
	RE_display_update_cb(re, rp, render_view3d_display_update);
	RE_stats_draw_cb(re, rp, render_view3d_renderinfo_cb);

	rstats = RE_GetStats(re);

	if (update_flag & PR_UPDATE_VIEW) {
		Object *object;
		rp->resolution_divider = rp->start_resolution_divider;

		/* Same as database_init_objects(), loop over all objects.
		 * We might consider de-duplicating the code between this two cases.
		 */
		for (object = rp->bmain->object.first; object; object = object->id.next) {
			float mat[4][4];
			mul_m4_m4m4(mat, rp->viewmat, object->obmat);
			invert_m4_m4(object->imat_ren, mat);
		}
	}

	use_border = render_view3d_disprect(rp->scene, rp->ar, rp->v3d,
	                                    rp->rv3d, &cliprct);

	if ((update_flag & (PR_UPDATE_RENDERSIZE | PR_UPDATE_DATABASE | PR_UPDATE_VIEW)) || rstats->convertdone == 0) {
		RenderData rdata;

		/* no osa, blur, seq, layers, savebuffer etc for preview render */
		rdata = rp->scene->r;
		rdata.mode &= ~(R_OSA | R_MBLUR | R_BORDER | R_PANORAMA);
		rdata.scemode &= ~(R_DOSEQ | R_DOCOMP | R_FREE_IMAGE | R_EXR_TILE_FILE | R_FULL_SAMPLE);
		rdata.scemode |= R_VIEWPORT_PREVIEW;

		/* we do use layers, but only active */
		rdata.scemode |= R_SINGLE_LAYER;

		/* initalize always */
		if (use_border) {
			rdata.mode |= R_BORDER;
			RE_InitState(re, NULL, &rdata, NULL, rp->ar->winx, rp->ar->winy, &cliprct);
		}
		else
			RE_InitState(re, NULL, &rdata, NULL, rp->ar->winx, rp->ar->winy, NULL);
	}

	if (orth)
		RE_SetOrtho(re, &viewplane, clipsta, clipend);
	else
		RE_SetWindow(re, &viewplane, clipsta, clipend);

	RE_SetPixelSize(re, pixsize);

	if ((update_flag & PR_UPDATE_DATABASE) || rstats->convertdone == 0) {
		unsigned int lay = rp->scene->lay;

		/* allow localview render for objects with lights in normal layers */
		if (rp->v3d->lay & 0xFF000000)
			lay |= rp->v3d->lay;
		else lay = rp->v3d->lay;

		RE_SetView(re, rp->viewmat);

		/* copying blender data while main thread is locked, to avoid crashes */
		WM_job_main_thread_lock_acquire(rp->job);
		RE_Database_Free(re);
		RE_Database_FromScene(re, rp->bmain, rp->scene, lay, 0);		// 0= dont use camera view
		WM_job_main_thread_lock_release(rp->job);

		/* do preprocessing like building raytree, shadows, volumes, SSS */
		RE_Database_Preprocess(re);

		/* conversion not completed, need to do it again */
		if (!rstats->convertdone) {
			if (render_view3d_is_valid(rp)) {
				rp->engine->job_update_flag |= PR_UPDATE_DATABASE;
			}
		}

		// printf("dbase update\n");
	}
	else {
		// printf("dbase rotate\n");
		RE_DataBase_IncrementalView(re, rp->viewmat, 0);
		restore = 1;
	}

	RE_DataBase_ApplyWindow(re);

	/* OK, can we enter render code? */
	if (rstats->convertdone) {
		bool first_time = true;

		if (update_flag & PR_UPDATE_VIEW) {
			ob_inst_update_flag |= RE_OBJECT_INSTANCES_UPDATE_VIEW;
		}

		RE_updateRenderInstances(re, ob_inst_update_flag);

		for (;;) {
			int pixel_size = BKE_render_preview_pixel_size(&rp->scene->r);
			if (first_time == false) {
				if (restore)
					RE_DataBase_IncrementalView(re, rp->viewmat, 1);

				rp->resolution_divider = MAX2(rp->resolution_divider / 2, pixel_size);
				*do_update = 1;

				render_update_resolution(re, rp, use_border, &cliprct);

				RE_DataBase_IncrementalView(re, rp->viewmat, 0);
				RE_DataBase_ApplyWindow(re);
				restore = 1;
			}
			else {
				render_update_resolution(re, rp, use_border, &cliprct);
			}

			RE_TileProcessor(re);

			first_time = false;

			if (*stop || rp->resolution_divider == pixel_size) {
				break;
			}
		}

		/* always rotate back */
		if (restore)
			RE_DataBase_IncrementalView(re, rp->viewmat, 1);
	}
}

static void render_view3d_free(void *customdata)
{
	RenderPreview *rp = customdata;

	MEM_freeN(rp);
}

static bool render_view3d_flag_changed(RenderEngine *engine, const bContext *C)
{
	Main *bmain = CTX_data_main(C);
	RegionView3D *rv3d = CTX_wm_region_view3d(C);
	View3D *v3d = CTX_wm_view3d(C);
	ARegion *ar = CTX_wm_region(C);
	Scene *scene = CTX_data_scene(C);
	Render *re;
	rctf viewplane;
	rcti disprect;
	float clipsta, clipend;
	bool orth;
	int job_update_flag = 0;
	char name[32];

	/* ensure render engine exists */
	re = engine->re;

	if (!re) {
		sprintf(name, "View3dPreview %p", (void *)ar);
		re = engine->re = RE_GetRender(name);
		if (!re)
			re = engine->re = RE_NewRender(name);

		engine->update_flag |= RE_ENGINE_UPDATE_DATABASE;
	}

	/* check update_flag */
	if (engine->update_flag & RE_ENGINE_UPDATE_MA)
		job_update_flag |= PR_UPDATE_MATERIAL;

	if (engine->update_flag & RE_ENGINE_UPDATE_OTHER)
		job_update_flag |= PR_UPDATE_MATERIAL;

	if (engine->update_flag & RE_ENGINE_UPDATE_DATABASE) {
		job_update_flag |= PR_UPDATE_DATABASE;

		/* load editmesh */
		if (scene->obedit)
			ED_object_editmode_load(bmain, scene->obedit);
	}

	engine->update_flag = 0;

	/* check if viewport changed */
	if (engine->last_winx != ar->winx || engine->last_winy != ar->winy) {
		engine->last_winx = ar->winx;
		engine->last_winy = ar->winy;
		job_update_flag |= PR_UPDATE_RENDERSIZE;
	}

	if (compare_m4m4(engine->last_viewmat, rv3d->viewmat, 0.00001f) == 0) {
		copy_m4_m4(engine->last_viewmat, rv3d->viewmat);
		job_update_flag |= PR_UPDATE_VIEW;
	}

	render_view3d_get_rects(ar, v3d, rv3d, &viewplane, engine, &clipsta, &clipend, NULL, &orth);

	if (BLI_rctf_compare(&viewplane, &engine->last_viewplane, 0.00001f) == 0) {
		engine->last_viewplane = viewplane;
		job_update_flag |= PR_UPDATE_VIEW;
	}

	render_view3d_disprect(scene, ar, v3d, rv3d, &disprect);
	if (BLI_rcti_compare(&disprect, &engine->last_disprect) == 0) {
		engine->last_disprect = disprect;
		job_update_flag |= PR_UPDATE_RENDERSIZE;
	}

	/* any changes? go ahead and rerender */
	if (job_update_flag) {
		engine->job_update_flag |= job_update_flag;
		return true;
	}

	return false;
}

static void render_view3d_do(RenderEngine *engine, const bContext *C)
{
	wmJob *wm_job;
	RenderPreview *rp;
	Scene *scene = CTX_data_scene(C);
	ARegion *ar = CTX_wm_region(C);
	int width = ar->winx, height = ar->winy;
	int divider = BKE_render_preview_pixel_size(&scene->r);
	int resolution_threshold = scene->r.preview_start_resolution *
	                           scene->r.preview_start_resolution;

	if (CTX_wm_window(C) == NULL)
		return;
	if (!render_view3d_flag_changed(engine, C))
		return;

	wm_job = WM_jobs_get(CTX_wm_manager(C), CTX_wm_window(C), CTX_wm_region(C), "Render Preview",
	                     WM_JOB_EXCL_RENDER, WM_JOB_TYPE_RENDER_PREVIEW);
	rp = MEM_callocN(sizeof(RenderPreview), "render preview");
	rp->job = wm_job;

	while (width * height > resolution_threshold) {
		width = max_ii(1, width / 2);
		height = max_ii(1, height / 2);
		divider *= 2;
	}

	/* customdata for preview thread */
	rp->scene = scene;
	rp->engine = engine;
	rp->sa = CTX_wm_area(C);
	rp->ar = CTX_wm_region(C);
	rp->v3d = rp->sa->spacedata.first;
	rp->rv3d = CTX_wm_region_view3d(C);
	rp->bmain = CTX_data_main(C);
	rp->resolution_divider = divider;
	rp->start_resolution_divider = divider;
	rp->has_freestyle = (scene->r.mode & R_EDGE_FRS) != 0;
	copy_m4_m4(rp->viewmat, rp->rv3d->viewmat);

	/* clear info text */
	engine->text[0] = '\0';

	/* setup job */
	WM_jobs_customdata_set(wm_job, rp, render_view3d_free);
	WM_jobs_timer(wm_job, 0.1, NC_SPACE | ND_SPACE_VIEW3D, NC_SPACE | ND_SPACE_VIEW3D);
	WM_jobs_callbacks(wm_job, render_view3d_startjob, NULL, NULL, NULL);

	WM_jobs_start(CTX_wm_manager(C), wm_job);

	engine->flag &= ~RE_ENGINE_DO_UPDATE;
}

/* callback for render engine, on changes */
void render_view3d_update(RenderEngine *engine, const bContext *C)
{
	/* this shouldn't be needed and causes too many database rebuilds, but we
	 * aren't actually tracking updates for all relevant datablocks so this is
	 * a catch-all for updates */
	engine->update_flag |= RE_ENGINE_UPDATE_DATABASE;

	render_view3d_do(engine, C);
}

void render_view3d_draw(RenderEngine *engine, const bContext *C)
{
	Render *re = engine->re;
	RenderResult rres;
	char name[32];

	render_view3d_do(engine, C);

	if (re == NULL) {
		sprintf(name, "View3dPreview %p", (void *)CTX_wm_region(C));
		re = RE_GetRender(name);

		if (re == NULL) return;
	}

	/* Viewport render preview doesn't support multiview, view hardcoded to 0 */
	RE_AcquireResultImage(re, &rres, 0);

	if (rres.rectf) {
		RegionView3D *rv3d = CTX_wm_region_view3d(C);
		View3D *v3d = CTX_wm_view3d(C);
		Scene *scene = CTX_data_scene(C);
		ARegion *ar = CTX_wm_region(C);
		bool force_fallback = false;
		bool need_fallback = true;
		float dither = scene->r.dither_intensity;
		float scale_x, scale_y;
		rcti clip_rect;
		int xof, yof;

		if (render_view3d_disprect(scene, ar, v3d, rv3d, &clip_rect)) {
			scale_x = (float) BLI_rcti_size_x(&clip_rect) / rres.rectx;
			scale_y = (float) BLI_rcti_size_y(&clip_rect) / rres.recty;
			xof = clip_rect.xmin;
			yof = clip_rect.ymin;
		}
		else {
			scale_x = (float) ar->winx / rres.rectx;
			scale_y = (float) ar->winy / rres.recty;
			xof = rres.xof;
			yof = rres.yof;
		}

		/* If user decided not to use GLSL, fallback to glaDrawPixelsAuto */
		force_fallback |= (U.image_draw_method != IMAGE_DRAW_METHOD_GLSL);

		/* Try using GLSL display transform. */
		if (force_fallback == false) {
			if (IMB_colormanagement_setup_glsl_draw(&scene->view_settings, &scene->display_settings, dither, true)) {
				glEnable(GL_BLEND);
				glColor4f(1.0f, 1.0f, 1.0f, 1.0f);
				glPixelZoom(scale_x, scale_y);
				glaDrawPixelsTex(xof, yof, rres.rectx, rres.recty,
				                 GL_RGBA, GL_FLOAT, GL_NEAREST, rres.rectf);
				glPixelZoom(1.0f, 1.0f);
				glDisable(GL_BLEND);

				IMB_colormanagement_finish_glsl_draw();
				need_fallback = false;
			}
		}

		/* If GLSL failed, use old-school CPU-based transform. */
		if (need_fallback) {
			unsigned char *display_buffer = MEM_mallocN(4 * rres.rectx * rres.recty * sizeof(char),
			                                            "render_view3d_draw");

			IMB_colormanagement_buffer_make_display_space(rres.rectf, display_buffer, rres.rectx, rres.recty,
			                                              4, dither, &scene->view_settings, &scene->display_settings);

			glEnable(GL_BLEND);
			glColor4f(1.0f, 1.0f, 1.0f, 1.0f);
			glPixelZoom(scale_x, scale_y);
			glaDrawPixelsAuto(xof, yof, rres.rectx, rres.recty,
			                  GL_RGBA, GL_UNSIGNED_BYTE,
			                  GL_NEAREST, display_buffer);
			glPixelZoom(1.0f, 1.0f);
			glDisable(GL_BLEND);

			MEM_freeN(display_buffer);
		}
	}

	RE_ReleaseResultImage(re);
}

void ED_viewport_render_kill_jobs(wmWindowManager *wm,
                                  Main *bmain,
                                  bool free_database)
{
	bScreen *sc;
	ScrArea *sa;
	ARegion *ar;

	if (!wm)
		return;

	/* kill all actively running jobs */
	WM_jobs_kill(wm, NULL, render_view3d_startjob);

	/* loop over 3D view render engines */
	for (sc = bmain->screen.first; sc; sc = sc->id.next) {
		for (sa = sc->areabase.first; sa; sa = sa->next) {
			if (sa->spacetype != SPACE_VIEW3D)
				continue;

			for (ar = sa->regionbase.first; ar; ar = ar->next) {
				RegionView3D *rv3d;

				if (ar->regiontype != RGN_TYPE_WINDOW)
					continue;

				rv3d = ar->regiondata;

				if (rv3d->render_engine) {
					/* free render database now before we change data, because
					 * RE_Database_Free will also loop over blender data */
					if (free_database) {
						char name[32];
						Render *re;

						sprintf(name, "View3dPreview %p", (void *)ar);
						re = RE_GetRender(name);

						if (re)
							RE_Database_Free(re);

						/* tag render engine to update entire database */
						rv3d->render_engine->update_flag |= RE_ENGINE_UPDATE_DATABASE;
					}
					else {
						/* quick shader update */
						rv3d->render_engine->update_flag |= RE_ENGINE_UPDATE_MA;
					}
				}
			}
		}
	}
}

Scene *ED_render_job_get_scene(const bContext *C)
{
	wmWindowManager *wm = CTX_wm_manager(C);
	RenderJob *rj = (RenderJob *)WM_jobs_customdata_from_type(wm, WM_JOB_TYPE_RENDER);

	if (rj)
		return rj->scene;

	return NULL;
}

Scene *ED_render_job_get_current_scene(const bContext *C)
{
	wmWindowManager *wm = CTX_wm_manager(C);
	RenderJob *rj = (RenderJob *)WM_jobs_customdata_from_type(wm, WM_JOB_TYPE_RENDER);
	if (rj) {
		return rj->current_scene;
	}
	return NULL;
}

/* Motion blur curve preset */

static int render_shutter_curve_preset_exec(bContext *C, wmOperator *op)
{
	Scene *scene = CTX_data_scene(C);
	CurveMapping *mblur_shutter_curve = &scene->r.mblur_shutter_curve;
	CurveMap *cm = mblur_shutter_curve->cm;
	int preset = RNA_enum_get(op->ptr, "shape");

	cm->flag &= ~CUMA_EXTEND_EXTRAPOLATE;
	mblur_shutter_curve->preset = preset;
	curvemap_reset(cm,
	               &mblur_shutter_curve->clipr,
	               mblur_shutter_curve->preset,
	               CURVEMAP_SLOPE_POS_NEG);
	curvemapping_changed(mblur_shutter_curve, false);

	return OPERATOR_FINISHED;
}

void RENDER_OT_shutter_curve_preset(wmOperatorType *ot)
{
	PropertyRNA *prop;
	static const EnumPropertyItem prop_shape_items[] = {
		{CURVE_PRESET_SHARP, "SHARP", 0, "Sharp", ""},
		{CURVE_PRESET_SMOOTH, "SMOOTH", 0, "Smooth", ""},
		{CURVE_PRESET_MAX, "MAX", 0, "Max", ""},
		{CURVE_PRESET_LINE, "LINE", 0, "Line", ""},
		{CURVE_PRESET_ROUND, "ROUND", 0, "Round", ""},
		{CURVE_PRESET_ROOT, "ROOT", 0, "Root", ""},
		{0, NULL, 0, NULL, NULL}};

	ot->name = "Shutter Curve Preset";
	ot->description = "Set shutter curve";
	ot->idname = "RENDER_OT_shutter_curve_preset";

	ot->exec = render_shutter_curve_preset_exec;

	prop = RNA_def_enum(ot->srna, "shape", prop_shape_items, CURVE_PRESET_SMOOTH, "Mode", "");
	RNA_def_property_translation_context(prop, BLT_I18NCONTEXT_ID_CURVE); /* Abusing id_curve :/ */
}
