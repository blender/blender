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
 * The Original Code is Copyright (C) 2006 Blender Foundation.
 * All rights reserved.
 *
 * The Original Code is: all of this file.
 *
 * Contributor(s): none yet.
 *
 * ***** END GPL LICENSE BLOCK *****
 */

/** \file blender/render/intern/source/render_result.c
 *  \ingroup render
 */

#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "MEM_guardedalloc.h"

#include "BKE_appdir.h"
#include "BLI_utildefines.h"
#include "BLI_listbase.h"
#include "BLI_hash_md5.h"
#include "BLI_path_util.h"
#include "BLI_rect.h"
#include "BLI_string.h"
#include "BLI_threads.h"

#include "BKE_image.h"
#include "BKE_global.h"
#include "BKE_main.h"
#include "BKE_report.h"
#include "BKE_camera.h"
#include "BKE_scene.h"

#include "IMB_imbuf.h"
#include "IMB_imbuf_types.h"
#include "IMB_colormanagement.h"

#include "intern/openexr/openexr_multi.h"

#include "render_result.h"
#include "render_types.h"

/********************************** Free *************************************/

static void render_result_views_free(RenderResult *res)
{
	while (res->views.first) {
		RenderView *rv = res->views.first;
		BLI_remlink(&res->views, rv);

		if (rv->rect32)
			MEM_freeN(rv->rect32);

		if (rv->rectz)
			MEM_freeN(rv->rectz);

		if (rv->rectf)
			MEM_freeN(rv->rectf);

		MEM_freeN(rv);
	}
}

void render_result_free(RenderResult *res)
{
	if (res == NULL) return;

	while (res->layers.first) {
		RenderLayer *rl = res->layers.first;

		/* acolrect and scolrect are optionally allocated in shade_tile, only free here since it can be used for drawing */
		if (rl->acolrect) MEM_freeN(rl->acolrect);
		if (rl->scolrect) MEM_freeN(rl->scolrect);
		if (rl->display_buffer) MEM_freeN(rl->display_buffer);
		
		while (rl->passes.first) {
			RenderPass *rpass = rl->passes.first;
			if (rpass->rect) MEM_freeN(rpass->rect);
			BLI_remlink(&rl->passes, rpass);
			MEM_freeN(rpass);
		}
		BLI_remlink(&res->layers, rl);
		MEM_freeN(rl);
	}

	render_result_views_free(res);

	if (res->rect32)
		MEM_freeN(res->rect32);
	if (res->rectz)
		MEM_freeN(res->rectz);
	if (res->rectf)
		MEM_freeN(res->rectf);
	if (res->text)
		MEM_freeN(res->text);
	if (res->error)
		MEM_freeN(res->error);
	if (res->stamp_data)
		MEM_freeN(res->stamp_data);

	MEM_freeN(res);
}

/* version that's compatible with fullsample buffers */
void render_result_free_list(ListBase *lb, RenderResult *rr)
{
	RenderResult *rrnext;
	
	for (; rr; rr = rrnext) {
		rrnext = rr->next;
		
		if (lb && lb->first)
			BLI_remlink(lb, rr);
		
		render_result_free(rr);
	}
}

/********************************* multiview *************************************/

/* create a new views Listbase in rr without duplicating the memory pointers */
void render_result_views_shallowcopy(RenderResult *dst, RenderResult *src)
{
	RenderView *rview;

	if (dst == NULL || src == NULL)
		return;

	for (rview = src->views.first; rview; rview = rview->next) {
		RenderView *rv;

		rv = MEM_mallocN(sizeof(RenderView), "new render view");
		BLI_addtail(&dst->views, rv);

		BLI_strncpy(rv->name, rview->name, sizeof(rv->name));
		rv->rectf = rview->rectf;
		rv->rectz = rview->rectz;
		rv->rect32 = rview->rect32;
	}
}

/* free the views created temporarily */
void render_result_views_shallowdelete(RenderResult *rr)
{
	if (rr == NULL)
		return;

	while (rr->views.first) {
		RenderView *rv = rr->views.first;
		BLI_remlink(&rr->views, rv);
		MEM_freeN(rv);
	}
}

static const char *name_from_passtype(int passtype, int channel)
{
	if (passtype == SCE_PASS_COMBINED) {
		if (channel == -1) return "Combined";
		if (channel == 0) return "Combined.R";
		if (channel == 1) return "Combined.G";
		if (channel == 2) return "Combined.B";
		return "Combined.A";
	}
	if (passtype == SCE_PASS_Z) {
		if (channel == -1) return "Depth";
		return "Depth.Z";
	}
	if (passtype == SCE_PASS_VECTOR) {
		if (channel == -1) return "Vector";
		if (channel == 0) return "Vector.X";
		if (channel == 1) return "Vector.Y";
		if (channel == 2) return "Vector.Z";
		return "Vector.W";
	}
	if (passtype == SCE_PASS_NORMAL) {
		if (channel == -1) return "Normal";
		if (channel == 0) return "Normal.X";
		if (channel == 1) return "Normal.Y";
		return "Normal.Z";
	}
	if (passtype == SCE_PASS_UV) {
		if (channel == -1) return "UV";
		if (channel == 0) return "UV.U";
		if (channel == 1) return "UV.V";
		return "UV.A";
	}
	if (passtype == SCE_PASS_RGBA) {
		if (channel == -1) return "Color";
		if (channel == 0) return "Color.R";
		if (channel == 1) return "Color.G";
		if (channel == 2) return "Color.B";
		return "Color.A";
	}
	if (passtype == SCE_PASS_EMIT) {
		if (channel == -1) return "Emit";
		if (channel == 0) return "Emit.R";
		if (channel == 1) return "Emit.G";
		return "Emit.B";
	}
	if (passtype == SCE_PASS_DIFFUSE) {
		if (channel == -1) return "Diffuse";
		if (channel == 0) return "Diffuse.R";
		if (channel == 1) return "Diffuse.G";
		return "Diffuse.B";
	}
	if (passtype == SCE_PASS_SPEC) {
		if (channel == -1) return "Spec";
		if (channel == 0) return "Spec.R";
		if (channel == 1) return "Spec.G";
		return "Spec.B";
	}
	if (passtype == SCE_PASS_SHADOW) {
		if (channel == -1) return "Shadow";
		if (channel == 0) return "Shadow.R";
		if (channel == 1) return "Shadow.G";
		return "Shadow.B";
	}
	if (passtype == SCE_PASS_AO) {
		if (channel == -1) return "AO";
		if (channel == 0) return "AO.R";
		if (channel == 1) return "AO.G";
		return "AO.B";
	}
	if (passtype == SCE_PASS_ENVIRONMENT) {
		if (channel == -1) return "Env";
		if (channel == 0) return "Env.R";
		if (channel == 1) return "Env.G";
		return "Env.B";
	}
	if (passtype == SCE_PASS_INDIRECT) {
		if (channel == -1) return "Indirect";
		if (channel == 0) return "Indirect.R";
		if (channel == 1) return "Indirect.G";
		return "Indirect.B";
	}
	if (passtype == SCE_PASS_REFLECT) {
		if (channel == -1) return "Reflect";
		if (channel == 0) return "Reflect.R";
		if (channel == 1) return "Reflect.G";
		return "Reflect.B";
	}
	if (passtype == SCE_PASS_REFRACT) {
		if (channel == -1) return "Refract";
		if (channel == 0) return "Refract.R";
		if (channel == 1) return "Refract.G";
		return "Refract.B";
	}
	if (passtype == SCE_PASS_INDEXOB) {
		if (channel == -1) return "IndexOB";
		return "IndexOB.X";
	}
	if (passtype == SCE_PASS_INDEXMA) {
		if (channel == -1) return "IndexMA";
		return "IndexMA.X";
	}
	if (passtype == SCE_PASS_MIST) {
		if (channel == -1) return "Mist";
		return "Mist.Z";
	}
	if (passtype == SCE_PASS_RAYHITS) {
		if (channel == -1) return "Rayhits";
		if (channel == 0) return "Rayhits.R";
		if (channel == 1) return "Rayhits.G";
		return "Rayhits.B";
	}
	if (passtype == SCE_PASS_DIFFUSE_DIRECT) {
		if (channel == -1) return "DiffDir";
		if (channel == 0) return "DiffDir.R";
		if (channel == 1) return "DiffDir.G";
		return "DiffDir.B";
	}
	if (passtype == SCE_PASS_DIFFUSE_INDIRECT) {
		if (channel == -1) return "DiffInd";
		if (channel == 0) return "DiffInd.R";
		if (channel == 1) return "DiffInd.G";
		return "DiffInd.B";
	}
	if (passtype == SCE_PASS_DIFFUSE_COLOR) {
		if (channel == -1) return "DiffCol";
		if (channel == 0) return "DiffCol.R";
		if (channel == 1) return "DiffCol.G";
		return "DiffCol.B";
	}
	if (passtype == SCE_PASS_GLOSSY_DIRECT) {
		if (channel == -1) return "GlossDir";
		if (channel == 0) return "GlossDir.R";
		if (channel == 1) return "GlossDir.G";
		return "GlossDir.B";
	}
	if (passtype == SCE_PASS_GLOSSY_INDIRECT) {
		if (channel == -1) return "GlossInd";
		if (channel == 0) return "GlossInd.R";
		if (channel == 1) return "GlossInd.G";
		return "GlossInd.B";
	}
	if (passtype == SCE_PASS_GLOSSY_COLOR) {
		if (channel == -1) return "GlossCol";
		if (channel == 0) return "GlossCol.R";
		if (channel == 1) return "GlossCol.G";
		return "GlossCol.B";
	}
	if (passtype == SCE_PASS_TRANSM_DIRECT) {
		if (channel == -1) return "TransDir";
		if (channel == 0) return "TransDir.R";
		if (channel == 1) return "TransDir.G";
		return "TransDir.B";
	}
	if (passtype == SCE_PASS_TRANSM_INDIRECT) {
		if (channel == -1) return "TransInd";
		if (channel == 0) return "TransInd.R";
		if (channel == 1) return "TransInd.G";
		return "TransInd.B";
	}
	if (passtype == SCE_PASS_TRANSM_COLOR) {
		if (channel == -1) return "TransCol";
		if (channel == 0) return "TransCol.R";
		if (channel == 1) return "TransCol.G";
		return "TransCol.B";
	}
	if (passtype == SCE_PASS_SUBSURFACE_DIRECT) {
		if (channel == -1) return "SubsurfaceDir";
		if (channel == 0) return "SubsurfaceDir.R";
		if (channel == 1) return "SubsurfaceDir.G";
		return "SubsurfaceDir.B";
	}
	if (passtype == SCE_PASS_SUBSURFACE_INDIRECT) {
		if (channel == -1) return "SubsurfaceInd";
		if (channel == 0) return "SubsurfaceInd.R";
		if (channel == 1) return "SubsurfaceInd.G";
		return "SubsurfaceInd.B";
	}
	if (passtype == SCE_PASS_SUBSURFACE_COLOR) {
		if (channel == -1) return "SubsurfaceCol";
		if (channel == 0) return "SubsurfaceCol.R";
		if (channel == 1) return "SubsurfaceCol.G";
		return "SubsurfaceCol.B";
	}
	return "Unknown";
}

static int passtype_from_name(const char *str)
{
	if (STRPREFIX(str, "Combined"))
		return SCE_PASS_COMBINED;

	if (STRPREFIX(str, "Depth"))
		return SCE_PASS_Z;

	if (STRPREFIX(str, "Vector"))
		return SCE_PASS_VECTOR;

	if (STRPREFIX(str, "Normal"))
		return SCE_PASS_NORMAL;

	if (STRPREFIX(str, "UV"))
		return SCE_PASS_UV;

	if (STRPREFIX(str, "Color"))
		return SCE_PASS_RGBA;

	if (STRPREFIX(str, "Emit"))
		return SCE_PASS_EMIT;

	if (STRPREFIX(str, "Diffuse"))
		return SCE_PASS_DIFFUSE;

	if (STRPREFIX(str, "Spec"))
		return SCE_PASS_SPEC;

	if (STRPREFIX(str, "Shadow"))
		return SCE_PASS_SHADOW;
	
	if (STRPREFIX(str, "AO"))
		return SCE_PASS_AO;

	if (STRPREFIX(str, "Env"))
		return SCE_PASS_ENVIRONMENT;

	if (STRPREFIX(str, "Indirect"))
		return SCE_PASS_INDIRECT;

	if (STRPREFIX(str, "Reflect"))
		return SCE_PASS_REFLECT;

	if (STRPREFIX(str, "Refract"))
		return SCE_PASS_REFRACT;

	if (STRPREFIX(str, "IndexOB"))
		return SCE_PASS_INDEXOB;

	if (STRPREFIX(str, "IndexMA"))
		return SCE_PASS_INDEXMA;

	if (STRPREFIX(str, "Mist"))
		return SCE_PASS_MIST;
	
	if (STRPREFIX(str, "RayHits"))
		return SCE_PASS_RAYHITS;

	if (STRPREFIX(str, "DiffDir"))
		return SCE_PASS_DIFFUSE_DIRECT;

	if (STRPREFIX(str, "DiffInd"))
		return SCE_PASS_DIFFUSE_INDIRECT;

	if (STRPREFIX(str, "DiffCol"))
		return SCE_PASS_DIFFUSE_COLOR;

	if (STRPREFIX(str, "GlossDir"))
		return SCE_PASS_GLOSSY_DIRECT;

	if (STRPREFIX(str, "GlossInd"))
		return SCE_PASS_GLOSSY_INDIRECT;

	if (STRPREFIX(str, "GlossCol"))
		return SCE_PASS_GLOSSY_COLOR;

	if (STRPREFIX(str, "TransDir"))
		return SCE_PASS_TRANSM_DIRECT;

	if (STRPREFIX(str, "TransInd"))
		return SCE_PASS_TRANSM_INDIRECT;

	if (STRPREFIX(str, "TransCol"))
		return SCE_PASS_TRANSM_COLOR;
		
	if (STRPREFIX(str, "SubsurfaceDir"))
		return SCE_PASS_SUBSURFACE_DIRECT;

	if (STRPREFIX(str, "SubsurfaceInd"))
		return SCE_PASS_SUBSURFACE_INDIRECT;

	if (STRPREFIX(str, "SubsurfaceCol"))
		return SCE_PASS_SUBSURFACE_COLOR;

	return 0;
}


static void set_pass_name(char *passname, int passtype, int channel, const char *view)
{
	const char *end;
	const char *token;
	int len;

	const char *passtype_name = name_from_passtype(passtype, channel);

	if (view == NULL || view[0] == '\0') {
		BLI_strncpy(passname, passtype_name, EXR_PASS_MAXNAME);
		return;
	}

	end = passtype_name + strlen(passtype_name);
	len = IMB_exr_split_token(passtype_name, end, &token);

	if (len == strlen(passtype_name))
		sprintf(passname, "%s.%s", passtype_name, view);
	else
		sprintf(passname, "%.*s%s.%s", (int)(end-passtype_name) - len, passtype_name, view, token);
}

/********************************** New **************************************/

static RenderPass *render_layer_add_pass(RenderResult *rr, RenderLayer *rl, int channels, int passtype, const char *viewname)
{
	const size_t view_id = BLI_findstringindex(&rr->views, viewname, offsetof(RenderView, name));
	const char *typestr = name_from_passtype(passtype, -1);
	RenderPass *rpass = MEM_callocN(sizeof(RenderPass), typestr);
	int rectsize = rr->rectx * rr->recty * channels;
	
	BLI_addtail(&rl->passes, rpass);
	rpass->passtype = passtype;
	rpass->channels = channels;
	rpass->rectx = rl->rectx;
	rpass->recty = rl->recty;
	rpass->view_id = view_id;

	set_pass_name(rpass->name, rpass->passtype, -1, viewname);
	BLI_strncpy(rpass->internal_name, typestr, sizeof(rpass->internal_name));
	BLI_strncpy(rpass->view, viewname, sizeof(rpass->view));
	
	if (rl->exrhandle) {
		int a;
		for (a = 0; a < channels; a++)
			IMB_exr_add_channel(rl->exrhandle, rl->name, name_from_passtype(passtype, a), viewname, 0, 0, NULL);
	}
	else {
		float *rect;
		int x;
		
		rpass->rect = MEM_mapallocN(sizeof(float) * rectsize, typestr);
		
		if (passtype == SCE_PASS_VECTOR) {
			/* initialize to max speed */
			rect = rpass->rect;
			for (x = rectsize - 1; x >= 0; x--)
				rect[x] = PASS_VECTOR_MAX;
		}
		else if (passtype == SCE_PASS_Z) {
			rect = rpass->rect;
			for (x = rectsize - 1; x >= 0; x--)
				rect[x] = 10e10;
		}
	}
	return rpass;
}

#ifdef WITH_CYCLES_DEBUG
static const char *debug_pass_type_name_get(int debug_type)
{
	switch (debug_type) {
		case RENDER_PASS_DEBUG_BVH_TRAVERSAL_STEPS:
			return "BVH Traversal Steps";
	}
	return "Unknown";
}

static RenderPass *render_layer_add_debug_pass(RenderResult *rr,
                                               RenderLayer *rl,
                                               int channels,
                                               int pass_type,
                                               int debug_type,
                                               const char *view)
{
	RenderPass *rpass = render_layer_add_pass(rr, rl, channels, pass_type, view);
	rpass->debug_type = debug_type;
	BLI_strncpy(rpass->name,
	            debug_pass_type_name_get(debug_type),
	            sizeof(rpass->name));
	return rpass;
}
#endif

/* called by main render as well for parts */
/* will read info from Render *re to define layers */
/* called in threads */
/* re->winx,winy is coordinate space of entire image, partrct the part within */
RenderResult *render_result_new(Render *re, rcti *partrct, int crop, int savebuffers, const char *layername, const char *viewname)
{
	RenderResult *rr;
	RenderLayer *rl;
	RenderView *rv;
	SceneRenderLayer *srl;
	int rectx, recty;
	int nr, i;
	
	rectx = BLI_rcti_size_x(partrct);
	recty = BLI_rcti_size_y(partrct);
	
	if (rectx <= 0 || recty <= 0)
		return NULL;
	
	rr = MEM_callocN(sizeof(RenderResult), "new render result");
	rr->rectx = rectx;
	rr->recty = recty;
	rr->renrect.xmin = 0; rr->renrect.xmax = rectx - 2 * crop;
	/* crop is one or two extra pixels rendered for filtering, is used for merging and display too */
	rr->crop = crop;

	/* tilerect is relative coordinates within render disprect. do not subtract crop yet */
	rr->tilerect.xmin = partrct->xmin - re->disprect.xmin;
	rr->tilerect.xmax = partrct->xmax - re->disprect.xmin;
	rr->tilerect.ymin = partrct->ymin - re->disprect.ymin;
	rr->tilerect.ymax = partrct->ymax - re->disprect.ymin;
	
	if (savebuffers) {
		rr->do_exr_tile = true;
	}

	render_result_views_new(rr, &re->r);

	/* check renderdata for amount of layers */
	for (nr = 0, srl = re->r.layers.first; srl; srl = srl->next, nr++) {

		if (layername && layername[0])
			if (!STREQ(srl->name, layername))
				continue;

		if (re->r.scemode & R_SINGLE_LAYER) {
			if (nr != re->r.actlay)
				continue;
		}
		else {
			if (srl->layflag & SCE_LAY_DISABLE)
				continue;
		}
		
		rl = MEM_callocN(sizeof(RenderLayer), "new render layer");
		BLI_addtail(&rr->layers, rl);
		
		BLI_strncpy(rl->name, srl->name, sizeof(rl->name));
		rl->lay = srl->lay;
		rl->lay_zmask = srl->lay_zmask;
		rl->lay_exclude = srl->lay_exclude;
		rl->layflag = srl->layflag;
		rl->passflag = srl->passflag; /* for debugging: srl->passflag | SCE_PASS_RAYHITS; */
		rl->pass_xor = srl->pass_xor;
		rl->light_override = srl->light_override;
		rl->mat_override = srl->mat_override;
		rl->rectx = rectx;
		rl->recty = recty;
		
		if (rr->do_exr_tile) {
			rl->display_buffer = MEM_mapallocN(rectx * recty * sizeof(unsigned int), "Combined display space rgba");
			rl->exrhandle = IMB_exr_get_handle();
		}

		for (rv = rr->views.first; rv; rv = rv->next) {
			const char *view = rv->name;

			if (viewname && viewname[0])
				if (!STREQ(view, viewname))
					continue;

			if (rr->do_exr_tile)
				IMB_exr_add_view(rl->exrhandle, view);

			/* a renderlayer should always have a Combined pass*/
			render_layer_add_pass(rr, rl, 4, SCE_PASS_COMBINED, view);

			if (srl->passflag  & SCE_PASS_Z)
				render_layer_add_pass(rr, rl, 1, SCE_PASS_Z, view);
			if (srl->passflag  & SCE_PASS_VECTOR)
				render_layer_add_pass(rr, rl, 4, SCE_PASS_VECTOR, view);
			if (srl->passflag  & SCE_PASS_NORMAL)
				render_layer_add_pass(rr, rl, 3, SCE_PASS_NORMAL, view);
			if (srl->passflag  & SCE_PASS_UV)
				render_layer_add_pass(rr, rl, 3, SCE_PASS_UV, view);
			if (srl->passflag  & SCE_PASS_RGBA)
				render_layer_add_pass(rr, rl, 4, SCE_PASS_RGBA, view);
			if (srl->passflag  & SCE_PASS_EMIT)
				render_layer_add_pass(rr, rl, 3, SCE_PASS_EMIT, view);
			if (srl->passflag  & SCE_PASS_DIFFUSE)
				render_layer_add_pass(rr, rl, 3, SCE_PASS_DIFFUSE, view);
			if (srl->passflag  & SCE_PASS_SPEC)
				render_layer_add_pass(rr, rl, 3, SCE_PASS_SPEC, view);
			if (srl->passflag  & SCE_PASS_AO)
				render_layer_add_pass(rr, rl, 3, SCE_PASS_AO, view);
			if (srl->passflag  & SCE_PASS_ENVIRONMENT)
				render_layer_add_pass(rr, rl, 3, SCE_PASS_ENVIRONMENT, view);
			if (srl->passflag  & SCE_PASS_INDIRECT)
				render_layer_add_pass(rr, rl, 3, SCE_PASS_INDIRECT, view);
			if (srl->passflag  & SCE_PASS_SHADOW)
				render_layer_add_pass(rr, rl, 3, SCE_PASS_SHADOW, view);
			if (srl->passflag  & SCE_PASS_REFLECT)
				render_layer_add_pass(rr, rl, 3, SCE_PASS_REFLECT, view);
			if (srl->passflag  & SCE_PASS_REFRACT)
				render_layer_add_pass(rr, rl, 3, SCE_PASS_REFRACT, view);
			if (srl->passflag  & SCE_PASS_INDEXOB)
				render_layer_add_pass(rr, rl, 1, SCE_PASS_INDEXOB, view);
			if (srl->passflag  & SCE_PASS_INDEXMA)
				render_layer_add_pass(rr, rl, 1, SCE_PASS_INDEXMA, view);
			if (srl->passflag  & SCE_PASS_MIST)
				render_layer_add_pass(rr, rl, 1, SCE_PASS_MIST, view);
			if (rl->passflag & SCE_PASS_RAYHITS)
				render_layer_add_pass(rr, rl, 4, SCE_PASS_RAYHITS, view);
			if (srl->passflag  & SCE_PASS_DIFFUSE_DIRECT)
				render_layer_add_pass(rr, rl, 3, SCE_PASS_DIFFUSE_DIRECT, view);
			if (srl->passflag  & SCE_PASS_DIFFUSE_INDIRECT)
				render_layer_add_pass(rr, rl, 3, SCE_PASS_DIFFUSE_INDIRECT, view);
			if (srl->passflag  & SCE_PASS_DIFFUSE_COLOR)
				render_layer_add_pass(rr, rl, 3, SCE_PASS_DIFFUSE_COLOR, view);
			if (srl->passflag  & SCE_PASS_GLOSSY_DIRECT)
				render_layer_add_pass(rr, rl, 3, SCE_PASS_GLOSSY_DIRECT, view);
			if (srl->passflag  & SCE_PASS_GLOSSY_INDIRECT)
				render_layer_add_pass(rr, rl, 3, SCE_PASS_GLOSSY_INDIRECT, view);
			if (srl->passflag  & SCE_PASS_GLOSSY_COLOR)
				render_layer_add_pass(rr, rl, 3, SCE_PASS_GLOSSY_COLOR, view);
			if (srl->passflag  & SCE_PASS_TRANSM_DIRECT)
				render_layer_add_pass(rr, rl, 3, SCE_PASS_TRANSM_DIRECT, view);
			if (srl->passflag  & SCE_PASS_TRANSM_INDIRECT)
				render_layer_add_pass(rr, rl, 3, SCE_PASS_TRANSM_INDIRECT, view);
			if (srl->passflag  & SCE_PASS_TRANSM_COLOR)
				render_layer_add_pass(rr, rl, 3, SCE_PASS_TRANSM_COLOR, view);
			if (srl->passflag  & SCE_PASS_SUBSURFACE_DIRECT)
				render_layer_add_pass(rr, rl, 3, SCE_PASS_SUBSURFACE_DIRECT, view);
			if (srl->passflag  & SCE_PASS_SUBSURFACE_INDIRECT)
				render_layer_add_pass(rr, rl, 3, SCE_PASS_SUBSURFACE_INDIRECT, view);
			if (srl->passflag  & SCE_PASS_SUBSURFACE_COLOR)
				render_layer_add_pass(rr, rl, 3, SCE_PASS_SUBSURFACE_COLOR, view);

#ifdef WITH_CYCLES_DEBUG
			if (BKE_scene_use_new_shading_nodes(re->scene)) {
				render_layer_add_debug_pass(rr, rl, 1, SCE_PASS_DEBUG,
				        RENDER_PASS_DEBUG_BVH_TRAVERSAL_STEPS, view);
			}
#endif
		}
	}
	/* sss, previewrender and envmap don't do layers, so we make a default one */
	if (BLI_listbase_is_empty(&rr->layers) && !(layername && layername[0])) {
		rl = MEM_callocN(sizeof(RenderLayer), "new render layer");
		BLI_addtail(&rr->layers, rl);
		
		rl->rectx = rectx;
		rl->recty = recty;

		/* duplicate code... */
		if (rr->do_exr_tile) {
			rl->display_buffer = MEM_mapallocN(rectx * recty * sizeof(unsigned int), "Combined display space rgba");
			rl->exrhandle = IMB_exr_get_handle();
		}

		for (rv = rr->views.first; rv; rv = rv->next) {
			const char *view = rv->name;

			if (viewname && viewname[0])
				if (strcmp(view, viewname) != 0)
					continue;

			if (rr->do_exr_tile) {
				IMB_exr_add_view(rl->exrhandle, view);

				for (i=0; i < 4; i++)
					IMB_exr_add_channel(rl->exrhandle, rl->name, name_from_passtype(SCE_PASS_COMBINED, i), view, 0, 0, NULL);
			}
			else {
				render_layer_add_pass(rr, rl, 4, SCE_PASS_COMBINED, view);
			}
		}

		/* note, this has to be in sync with scene.c */
		rl->lay = (1 << 20) - 1;
		rl->layflag = 0x7FFF;    /* solid ztra halo strand */
		rl->passflag = SCE_PASS_COMBINED;
		
		re->r.actlay = 0;
	}
	
	/* border render; calculate offset for use in compositor. compo is centralized coords */
	/* XXX obsolete? I now use it for drawing border render offset (ton) */
	rr->xof = re->disprect.xmin + BLI_rcti_cent_x(&re->disprect) - (re->winx / 2);
	rr->yof = re->disprect.ymin + BLI_rcti_cent_y(&re->disprect) - (re->winy / 2);
	
	return rr;
}

/* allocate osa new results for samples */
RenderResult *render_result_new_full_sample(Render *re, ListBase *lb, rcti *partrct, int crop, int savebuffers, const char *viewname)
{
	int a;
	
	if (re->osa == 0)
		return render_result_new(re, partrct, crop, savebuffers, RR_ALL_LAYERS, viewname);
	
	for (a = 0; a < re->osa; a++) {
		RenderResult *rr = render_result_new(re, partrct, crop, savebuffers, RR_ALL_LAYERS, viewname);
		BLI_addtail(lb, rr);
		rr->sample_nr = a;
	}
	
	return lb->first;
}

/* callbacks for render_result_new_from_exr */
static void *ml_addlayer_cb(void *base, const char *str)
{
	RenderResult *rr = base;
	RenderLayer *rl;
	
	rl = MEM_callocN(sizeof(RenderLayer), "new render layer");
	BLI_addtail(&rr->layers, rl);
	
	BLI_strncpy(rl->name, str, EXR_LAY_MAXNAME);
	return rl;
}

static void ml_addpass_cb(void *base, void *lay, const char *str, float *rect, int totchan, const char *chan_id, const char *view)
{
	RenderResult *rr = base;
	RenderLayer *rl = lay;
	RenderPass *rpass = MEM_callocN(sizeof(RenderPass), "loaded pass");
	int a;
	
	BLI_addtail(&rl->passes, rpass);
	rpass->channels = totchan;
	rpass->passtype = passtype_from_name(str);
	if (rpass->passtype == 0) printf("unknown pass %s\n", str);
	rl->passflag |= rpass->passtype;
	
	/* channel id chars */
	for (a = 0; a < totchan; a++)
		rpass->chan_id[a] = chan_id[a];

	rpass->rect = rect;
	if (view[0] != '\0') {
		BLI_snprintf(rpass->name, sizeof(rpass->name), "%s.%s", str, view);
		rpass->view_id = BLI_findstringindex(&rr->views, view, offsetof(RenderView, name));
	}
	else {
		BLI_strncpy(rpass->name,  str, sizeof(rpass->name));
		rpass->view_id = 0;
	}

	BLI_strncpy(rpass->view, view, sizeof(rpass->view));
	BLI_strncpy(rpass->internal_name, str, sizeof(rpass->internal_name));
}

static void *ml_addview_cb(void *base, const char *str)
{
	RenderResult *rr = base;
	RenderView *rv;

	rv = MEM_callocN(sizeof(RenderView), "new render view");
	BLI_strncpy(rv->name, str, EXR_VIEW_MAXNAME);

	/* For stereo drawing we need to ensure:
	 * STEREO_LEFT_NAME  == STEREO_LEFT_ID and
	 * STEREO_RIGHT_NAME == STEREO_RIGHT_ID */

	if (STREQ(str, STEREO_LEFT_NAME)) {
		BLI_addhead(&rr->views, rv);
	}
	else if (STREQ(str, STEREO_RIGHT_NAME)) {
		RenderView *left_rv = BLI_findstring(&rr->views, STEREO_LEFT_NAME, offsetof(RenderView, name));

		if (left_rv == NULL) {
			BLI_addhead(&rr->views, rv);
		}
		else {
			BLI_insertlinkafter(&rr->views, left_rv, rv);
		}
	}
	else {
		BLI_addtail(&rr->views, rv);
	}

	return rv;
}

static int order_render_passes(const void *a, const void *b)
{
	// 1 if a is after b
	RenderPass *rpa = (RenderPass *) a;
	RenderPass *rpb = (RenderPass *) b;

	if (rpa->passtype > rpb->passtype)
		return 1;
	else if (rpa->passtype < rpb->passtype)
		return 0;

	/* they have the same type */
	/* left first */
	if (STREQ(rpa->view, STEREO_LEFT_NAME))
		return 0;
	else if (STREQ(rpb->view, STEREO_LEFT_NAME))
		return 1;

	/* right second */
	if (STREQ(rpa->view, STEREO_RIGHT_NAME))
		return 0;
	else if (STREQ(rpb->view, STEREO_RIGHT_NAME))
		return 1;

	/* remaining in ascending id order */
	return (rpa->view_id < rpb->view_id);
}

/* from imbuf, if a handle was returned and it's not a singlelayer multiview we convert this to render result */
RenderResult *render_result_new_from_exr(void *exrhandle, const char *colorspace, bool predivide, int rectx, int recty)
{
	RenderResult *rr = MEM_callocN(sizeof(RenderResult), __func__);
	RenderLayer *rl;
	RenderPass *rpass;
	const char *to_colorspace = IMB_colormanagement_role_colorspace_name_get(COLOR_ROLE_SCENE_LINEAR);

	rr->rectx = rectx;
	rr->recty = recty;
	
	IMB_exr_multilayer_convert(exrhandle, rr, ml_addview_cb, ml_addlayer_cb, ml_addpass_cb);

	for (rl = rr->layers.first; rl; rl = rl->next) {
		int c=0;
		rl->rectx = rectx;
		rl->recty = recty;

		BLI_listbase_sort(&rl->passes, order_render_passes);

		for (rpass = rl->passes.first; rpass; rpass = rpass->next) {
			printf("%d: %s\n", c++, rpass->name);
			rpass->rectx = rectx;
			rpass->recty = recty;

			if (rpass->channels >= 3) {
				IMB_colormanagement_transform(rpass->rect, rpass->rectx, rpass->recty, rpass->channels,
				                              colorspace, to_colorspace, predivide);
			}
		}
	}
	
	return rr;
}

void render_result_views_new(RenderResult *rr, RenderData *rd)
{
	SceneRenderView *srv;
	RenderView *rv;

	/* clear previously existing views - for sequencer */
	render_result_views_free(rr);

	/* check renderdata for amount of views */
	if ((rd->scemode & R_MULTIVIEW)) {
		for (srv = rd->views.first; srv; srv = srv->next) {
			if (BKE_scene_multiview_is_render_view_active(rd, srv) == false) continue;

			rv = MEM_callocN(sizeof(RenderView), "new render view");
			BLI_addtail(&rr->views, rv);

			BLI_strncpy(rv->name, srv->name, sizeof(rv->name));
		}
	}

	/* we always need at least one view */
	if (BLI_listbase_count_ex(&rr->views, 1) == 0) {
		rv = MEM_callocN(sizeof(RenderView), "new render view");
		BLI_addtail(&rr->views, rv);
	}
}

/*********************************** Merge ***********************************/

static void do_merge_tile(RenderResult *rr, RenderResult *rrpart, float *target, float *tile, int pixsize)
{
	int y, ofs, copylen, tilex, tiley;
	
	copylen = tilex = rrpart->rectx;
	tiley = rrpart->recty;
	
	if (rrpart->crop) { /* filters add pixel extra */
		tile += pixsize * (rrpart->crop + rrpart->crop * tilex);
		
		copylen = tilex - 2 * rrpart->crop;
		tiley -= 2 * rrpart->crop;
		
		ofs = (rrpart->tilerect.ymin + rrpart->crop) * rr->rectx + (rrpart->tilerect.xmin + rrpart->crop);
		target += pixsize * ofs;
	}
	else {
		ofs = (rrpart->tilerect.ymin * rr->rectx + rrpart->tilerect.xmin);
		target += pixsize * ofs;
	}

	copylen *= sizeof(float) * pixsize;
	tilex *= pixsize;
	ofs = pixsize * rr->rectx;

	for (y = 0; y < tiley; y++) {
		memcpy(target, tile, copylen);
		target += ofs;
		tile += tilex;
	}
}

/* used when rendering to a full buffer, or when reading the exr part-layer-pass file */
/* no test happens here if it fits... we also assume layers are in sync */
/* is used within threads */
void render_result_merge(RenderResult *rr, RenderResult *rrpart)
{
	RenderLayer *rl, *rlp;
	RenderPass *rpass, *rpassp;
	
	for (rl = rr->layers.first; rl; rl = rl->next) {
		rlp = RE_GetRenderLayer(rrpart, rl->name);
		if (rlp) {
			/* passes are allocated in sync */
			for (rpass = rl->passes.first, rpassp = rlp->passes.first;
			     rpass && rpassp;
			     rpass = rpass->next)
			{
				/* renderresult have all passes, renderpart only the active view's passes */
				if (strcmp(rpassp->name, rpass->name) != 0)
					continue;

				do_merge_tile(rr, rrpart, rpass->rect, rpassp->rect, rpass->channels);

				/* manually get next render pass */
				rpassp = rpassp->next;
			}
		}
	}
}

/* for passes read from files, these have names stored */
static char *make_pass_name(RenderPass *rpass, int chan)
{
	static char name[EXR_PASS_MAXNAME];
	int len;
	
	BLI_strncpy(name, rpass->name, EXR_PASS_MAXNAME);
	len = strlen(name);
	name[len] = '.';
	name[len + 1] = rpass->chan_id[chan];
	name[len + 2] = 0;

	return name;
}

/* called from within UI and render pipeline, saves both rendered result as a file-read result
 * if multiview is true saves all views in a multiview exr
 * else if view is not NULL saves single view
 * else saves stereo3d
 */
bool RE_WriteRenderResult(ReportList *reports, RenderResult *rr, const char *filename, ImageFormatData *imf, const bool multiview, const char *view)
{
	RenderLayer *rl;
	RenderPass *rpass;
	RenderView *rview;
	void *exrhandle = IMB_exr_get_handle();
	bool success;
	int a, nr;
	const char *chan_view = NULL;
	int compress = (imf ? imf->exr_codec : 0);
	size_t width, height;

	const bool is_mono = view && !multiview;

	width = rr->rectx;
	height = rr->recty;

	if (imf && imf->imtype == R_IMF_IMTYPE_OPENEXR && multiview) {
		/* single layer OpenEXR */
		const char *RGBAZ[] = {"R", "G", "B", "A", "Z"};
		for (nr = 0, rview = rr->views.first; rview; rview = rview->next, nr++) {
			IMB_exr_add_view(exrhandle, rview->name);

			if (rview->rectf) {
				for (a = 0; a < 4; a++)
					IMB_exr_add_channel(exrhandle, "", RGBAZ[a],
					                    rview->name, 4, 4 * width, rview->rectf + a);
				if (rview->rectz)
					IMB_exr_add_channel(exrhandle, "", RGBAZ[4],
					                    rview->name, 1, width, rview->rectz);
			}
		}
	}
	else {
		for (nr = 0, rview = rr->views.first; rview; rview = rview->next, nr++) {
			if (is_mono) {
				if (!STREQ(view, rview->name)) {
					continue;
				}
				chan_view = "";
			}
			else {
				/* if rendered only one view, we treat as a a non-view render */
				chan_view = rview->name;
			}

			IMB_exr_add_view(exrhandle, rview->name);

			if (rview->rectf) {
				for (a = 0; a < 4; a++)
					IMB_exr_add_channel(exrhandle, "Composite", name_from_passtype(SCE_PASS_COMBINED, a),
					                    chan_view, 4, 4 * width, rview->rectf + a);
			}
		}

		/* add layers/passes and assign channels */
		for (rl = rr->layers.first; rl; rl = rl->next) {

			/* passes are allocated in sync */
			for (rpass = rl->passes.first; rpass; rpass = rpass->next) {
				const int xstride = rpass->channels;

				if (is_mono) {
					if (!STREQ(view, rpass->view)) {
						continue;
					}
					chan_view = "";
				}
				else {
					/* if rendered only one view, we treat as a a non-view render */
					chan_view = (nr > 1 ? rpass->view :"");
				}

				for (a = 0; a < xstride; a++) {

					if (rpass->passtype) {
						IMB_exr_add_channel(exrhandle, rl->name, name_from_passtype(rpass->passtype, a), chan_view,
						                    xstride, xstride * width, rpass->rect + a);
					}
					else {
						IMB_exr_add_channel(exrhandle, rl->name, make_pass_name(rpass, a), chan_view,
						                    xstride, xstride * width, rpass->rect + a);
					}
				}
			}
		}
	}

	BLI_make_existing_file(filename);

	if (IMB_exr_begin_write(exrhandle, filename, width, height, compress)) {
		IMB_exr_write_channels(exrhandle);
		success = true;
	}
	else {
		/* TODO, get the error from openexr's exception */
		BKE_report(reports, RPT_ERROR, "Error writing render result (see console)");
		success = false;
	}

	IMB_exr_close(exrhandle);
	return success;
}

/**************************** Single Layer Rendering *************************/

void render_result_single_layer_begin(Render *re)
{
	/* all layers except the active one get temporally pushed away */

	/* officially pushed result should be NULL... error can happen with do_seq */
	RE_FreeRenderResult(re->pushedresult);
	
	re->pushedresult = re->result;
	re->result = NULL;
}

/* if scemode is R_SINGLE_LAYER, at end of rendering, merge the both render results */
void render_result_single_layer_end(Render *re)
{
	SceneRenderLayer *srl;
	RenderLayer *rlpush;
	RenderLayer *rl;
	int nr;

	if (re->result == NULL) {
		printf("pop render result error; no current result!\n");
		return;
	}

	if (!re->pushedresult)
		return;

	if (re->pushedresult->rectx == re->result->rectx && re->pushedresult->recty == re->result->recty) {
		/* find which layer in re->pushedresult should be replaced */
		rl = re->result->layers.first;
		
		/* render result should be empty after this */
		BLI_remlink(&re->result->layers, rl);
		
		/* reconstruct render result layers */
		for (nr = 0, srl = re->r.layers.first; srl; srl = srl->next, nr++) {
			if (nr == re->r.actlay) {
				BLI_addtail(&re->result->layers, rl);
			}
			else {
				rlpush = RE_GetRenderLayer(re->pushedresult, srl->name);
				if (rlpush) {
					BLI_remlink(&re->pushedresult->layers, rlpush);
					BLI_addtail(&re->result->layers, rlpush);
				}
			}
		}
	}

	RE_FreeRenderResult(re->pushedresult);
	re->pushedresult = NULL;
}

/************************* EXR Tile File Rendering ***************************/

static void save_render_result_tile(RenderResult *rr, RenderResult *rrpart, const char *viewname)
{
	RenderLayer *rlp, *rl;
	RenderPass *rpassp;
	int offs, partx, party;
	
	BLI_lock_thread(LOCK_IMAGE);
	
	for (rlp = rrpart->layers.first; rlp; rlp = rlp->next) {
		rl = RE_GetRenderLayer(rr, rlp->name);

		/* should never happen but prevents crash if it does */
		BLI_assert(rl);
		if (UNLIKELY(rl == NULL)) {
			continue;
		}

		if (rrpart->crop) { /* filters add pixel extra */
			offs = (rrpart->crop + rrpart->crop * rrpart->rectx);
		}
		else {
			offs = 0;
		}

		/* passes are allocated in sync */
		for (rpassp = rlp->passes.first; rpassp; rpassp = rpassp->next) {
			const int xstride = rpassp->channels;
			int a;
			char passname[EXR_PASS_MAXNAME];

			for (a = 0; a < xstride; a++) {
				set_pass_name(passname, rpassp->passtype, a, rpassp->view);

				IMB_exr_set_channel(rl->exrhandle, rlp->name, passname,
				                    xstride, xstride * rrpart->rectx, rpassp->rect + a + xstride * offs);
			}
		}
		
	}

	party = rrpart->tilerect.ymin + rrpart->crop;
	partx = rrpart->tilerect.xmin + rrpart->crop;

	for (rlp = rrpart->layers.first; rlp; rlp = rlp->next) {
		rl = RE_GetRenderLayer(rr, rlp->name);

		/* should never happen but prevents crash if it does */
		BLI_assert(rl);
		if (UNLIKELY(rl == NULL)) {
			continue;
		}

		IMB_exrtile_write_channels(rl->exrhandle, partx, party, 0, viewname);
	}

	BLI_unlock_thread(LOCK_IMAGE);
}

static void save_empty_result_tiles(Render *re)
{
	RenderPart *pa;
	RenderResult *rr;
	RenderLayer *rl;
	
	for (rr = re->result; rr; rr = rr->next) {
		for (rl = rr->layers.first; rl; rl = rl->next) {
			IMB_exr_clear_channels(rl->exrhandle);
		
			for (pa = re->parts.first; pa; pa = pa->next) {
				if (pa->status != PART_STATUS_READY) {
					int party = pa->disprect.ymin - re->disprect.ymin + pa->crop;
					int partx = pa->disprect.xmin - re->disprect.xmin + pa->crop;
					IMB_exrtile_write_channels(rl->exrhandle, partx, party, 0, re->viewname);
				}
			}
		}
	}
}

/* begin write of exr tile file */
void render_result_exr_file_begin(Render *re)
{
	RenderResult *rr;
	RenderLayer *rl;
	char str[FILE_MAX];

	for (rr = re->result; rr; rr = rr->next) {
		for (rl = rr->layers.first; rl; rl = rl->next) {
			render_result_exr_file_path(re->scene, rl->name, rr->sample_nr, str);
			printf("write exr tmp file, %dx%d, %s\n", rr->rectx, rr->recty, str);
			IMB_exrtile_begin_write(rl->exrhandle, str, 0, rr->rectx, rr->recty, re->partx, re->party);
		}
	}
}

/* end write of exr tile file, read back first sample */
void render_result_exr_file_end(Render *re)
{
	RenderResult *rr;
	RenderLayer *rl;

	save_empty_result_tiles(re);
	
	for (rr = re->result; rr; rr = rr->next) {
		for (rl = rr->layers.first; rl; rl = rl->next) {
			IMB_exr_close(rl->exrhandle);
			rl->exrhandle = NULL;
		}

		rr->do_exr_tile = false;
	}
	
	render_result_free_list(&re->fullresult, re->result);
	re->result = NULL;

	render_result_exr_file_read_sample(re, 0);
}

/* save part into exr file */
void render_result_exr_file_merge(RenderResult *rr, RenderResult *rrpart, const char *viewname)
{
	for (; rr && rrpart; rr = rr->next, rrpart = rrpart->next)
		save_render_result_tile(rr, rrpart, viewname);
}

/* path to temporary exr file */
void render_result_exr_file_path(Scene *scene, const char *layname, int sample, char *filepath)
{
	char name[FILE_MAXFILE + MAX_ID_NAME + MAX_ID_NAME + 100], fi[FILE_MAXFILE];
	
	BLI_split_file_part(G.main->name, fi, sizeof(fi));
	if (sample == 0) {
		BLI_snprintf(name, sizeof(name), "%s_%s_%s.exr", fi, scene->id.name + 2, layname);
	}
	else {
		BLI_snprintf(name, sizeof(name), "%s_%s_%s%d.exr", fi, scene->id.name + 2, layname, sample);
	}

	/* Make name safe for paths, see T43275. */
	BLI_filename_make_safe(name);

	BLI_make_file_string("/", filepath, BKE_tempdir_session(), name);
}

/* only for temp buffer, makes exact copy of render result */
int render_result_exr_file_read_sample(Render *re, int sample)
{
	RenderLayer *rl;
	char str[FILE_MAXFILE + MAX_ID_NAME + MAX_ID_NAME + 100] = "";
	bool success = true;

	RE_FreeRenderResult(re->result);
	re->result = render_result_new(re, &re->disprect, 0, RR_USE_MEM, RR_ALL_LAYERS, RR_ALL_VIEWS);

	for (rl = re->result->layers.first; rl; rl = rl->next) {
		render_result_exr_file_path(re->scene, rl->name, sample, str);
		printf("read exr tmp file: %s\n", str);

		if (!render_result_exr_file_read_path(re->result, rl, str)) {
			printf("cannot read: %s\n", str);
			success = false;
		}
	}

	return success;
}

/* called for reading temp files, and for external engines */
int render_result_exr_file_read_path(RenderResult *rr, RenderLayer *rl_single, const char *filepath)
{
	RenderLayer *rl;
	RenderPass *rpass;
	void *exrhandle = IMB_exr_get_handle();
	int rectx, recty;

	if (IMB_exr_begin_read(exrhandle, filepath, &rectx, &recty) == 0) {
		printf("failed being read %s\n", filepath);
		IMB_exr_close(exrhandle);
		return 0;
	}

	if (rr == NULL || rectx != rr->rectx || recty != rr->recty) {
		if (rr)
			printf("error in reading render result: dimensions don't match\n");
		else
			printf("error in reading render result: NULL result pointer\n");
		IMB_exr_close(exrhandle);
		return 0;
	}

	for (rl = rr->layers.first; rl; rl = rl->next) {
		if (rl_single && rl_single != rl)
			continue;
		
		/* passes are allocated in sync */
		for (rpass = rl->passes.first; rpass; rpass = rpass->next) {
			const int xstride = rpass->channels;
			int a;
			char passname[EXR_PASS_MAXNAME];

			for (a = 0; a < xstride; a++) {
				set_pass_name(passname, rpass->passtype, a, rpass->view);
				IMB_exr_set_channel(exrhandle, rl->name, passname,
				                    xstride, xstride * rectx, rpass->rect + a);
			}

			set_pass_name(rpass->name, rpass->passtype, -1, rpass->view);
		}
	}

	IMB_exr_read_channels(exrhandle);
	IMB_exr_close(exrhandle);

	return 1;
}

static void render_result_exr_file_cache_path(Scene *sce, const char *root, char *r_path)
{
	char filename_full[FILE_MAX + MAX_ID_NAME + 100], filename[FILE_MAXFILE], dirname[FILE_MAXDIR];
	char path_digest[16] = {0};
	char path_hexdigest[33];

	/* If root is relative, use either current .blend file dir, or temp one if not saved. */
	if (G.main->name[0]) {
		BLI_split_dirfile(G.main->name, dirname, filename, sizeof(dirname), sizeof(filename));
		BLI_replace_extension(filename, sizeof(filename), "");  /* strip '.blend' */
		BLI_hash_md5_buffer(G.main->name, strlen(G.main->name), path_digest);
	}
	else {
		BLI_strncpy(dirname, BKE_tempdir_base(), sizeof(dirname));
		BLI_strncpy(filename, "UNSAVED", sizeof(filename));
	}
	BLI_hash_md5_to_hexdigest(path_digest, path_hexdigest);

	/* Default to *non-volatile* tmp dir. */
	if (*root == '\0') {
		root = BKE_tempdir_base();
	}

	BLI_snprintf(filename_full, sizeof(filename_full), "cached_RR_%s_%s_%s.exr",
	             filename, sce->id.name + 2, path_hexdigest);
	BLI_make_file_string(dirname, r_path, root, filename_full);
}

void render_result_exr_file_cache_write(Render *re)
{
	RenderResult *rr = re->result;
	char str[FILE_MAXFILE + FILE_MAXFILE + MAX_ID_NAME + 100];
	char *root = U.render_cachedir;

	render_result_exr_file_cache_path(re->scene, root, str);
	printf("Caching exr file, %dx%d, %s\n", rr->rectx, rr->recty, str);

	RE_WriteRenderResult(NULL, rr, str, NULL, true, NULL);
}

/* For cache, makes exact copy of render result */
bool render_result_exr_file_cache_read(Render *re)
{
	char str[FILE_MAXFILE + MAX_ID_NAME + MAX_ID_NAME + 100] = "";
	char *root = U.render_cachedir;

	RE_FreeRenderResult(re->result);
	re->result = render_result_new(re, &re->disprect, 0, RR_USE_MEM, RR_ALL_LAYERS, RR_ALL_VIEWS);

	/* First try cache. */
	render_result_exr_file_cache_path(re->scene, root, str);

	printf("read exr cache file: %s\n", str);
	if (!render_result_exr_file_read_path(re->result, NULL, str)) {
		printf("cannot read: %s\n", str);
		return false;
	}
	return true;
}

/*************************** Combined Pixel Rect *****************************/

ImBuf *render_result_rect_to_ibuf(RenderResult *rr, RenderData *rd, const int view_id)
{
	ImBuf *ibuf = IMB_allocImBuf(rr->rectx, rr->recty, rd->im_format.planes, 0);
	
	/* if not exists, BKE_imbuf_write makes one */
	ibuf->rect = (unsigned int *) RE_RenderViewGetRect32(rr, view_id);
	ibuf->rect_float = RE_RenderViewGetRectf(rr, view_id);
	ibuf->zbuf_float = RE_RenderViewGetRectz(rr, view_id);
	
	/* float factor for random dither, imbuf takes care of it */
	ibuf->dither = rd->dither_intensity;
	
	/* prepare to gamma correct to sRGB color space
	 * note that sequence editor can generate 8bpc render buffers
	 */
	if (ibuf->rect) {
		if (BKE_imtype_valid_depths(rd->im_format.imtype) & (R_IMF_CHAN_DEPTH_12 | R_IMF_CHAN_DEPTH_16 | R_IMF_CHAN_DEPTH_24 | R_IMF_CHAN_DEPTH_32)) {
			if (rd->im_format.depth == R_IMF_CHAN_DEPTH_8) {
				/* Higher depth bits are supported but not needed for current file output. */
				ibuf->rect_float = NULL;
			}
			else {
				IMB_float_from_rect(ibuf);
			}
		}
		else {
			/* ensure no float buffer remained from previous frame */
			ibuf->rect_float = NULL;
		}
	}

	/* color -> grayscale */
	/* editing directly would alter the render view */
	if (rd->im_format.planes == R_IMF_PLANES_BW) {
		ImBuf *ibuf_bw = IMB_dupImBuf(ibuf);
		IMB_color_to_bw(ibuf_bw);
		IMB_freeImBuf(ibuf);
		ibuf = ibuf_bw;
	}

	return ibuf;
}

void render_result_rect_from_ibuf(RenderResult *rr, RenderData *UNUSED(rd), ImBuf *ibuf, const int view_id)
{
	RenderView *rv = BLI_findlink(&rr->views, view_id);

	if (ibuf->rect_float) {
		if (!rv->rectf)
			rv->rectf = MEM_mallocN(4 * sizeof(float) * rr->rectx * rr->recty, "render_seq rectf");
		
		memcpy(rv->rectf, ibuf->rect_float, 4 * sizeof(float) * rr->rectx * rr->recty);

		/* TSK! Since sequence render doesn't free the *rr render result, the old rect32
		 * can hang around when sequence render has rendered a 32 bits one before */
		MEM_SAFE_FREE(rv->rect32);
	}
	else if (ibuf->rect) {
		if (!rv->rect32)
			rv->rect32 = MEM_mallocN(sizeof(int) * rr->rectx * rr->recty, "render_seq rect");

		memcpy(rv->rect32, ibuf->rect, 4 * rr->rectx * rr->recty);

		/* Same things as above, old rectf can hang around from previous render. */
		MEM_SAFE_FREE(rv->rectf);
	}

	/* clean up non-view buffers */
	MEM_SAFE_FREE(rr->rect32);
	MEM_SAFE_FREE(rr->rectf);
}

void render_result_rect_fill_zero(RenderResult *rr, const int view_id)
{
	RenderView *rv = BLI_findlink(&rr->views, view_id);

	if (rv->rectf)
		memset(rv->rectf, 0, 4 * sizeof(float) * rr->rectx * rr->recty);
	else if (rv->rect32)
		memset(rv->rect32, 0, 4 * rr->rectx * rr->recty);
	else
		rv->rect32 = MEM_callocN(sizeof(int) * rr->rectx * rr->recty, "render_seq rect");
}

void render_result_rect_get_pixels(RenderResult *rr, unsigned int *rect, int rectx, int recty,
                                   const ColorManagedViewSettings *view_settings, const ColorManagedDisplaySettings *display_settings,
                                   const int view_id)
{
	if (rr->rect32) {
		int *rect32 = RE_RenderViewGetRect32(rr, view_id);
		memcpy(rect, (rect32 ? rect32 : rr->rect32), sizeof(int) * rr->rectx * rr->recty);
	}
	else if (rr->rectf) {
		float *rectf = RE_RenderViewGetRectf(rr, view_id);
		IMB_display_buffer_transform_apply((unsigned char *) rect, (rectf ? rectf : rr->rectf), rr->rectx, rr->recty, 4,
		                                   view_settings, display_settings, true);
	}
	else
		/* else fill with black */
		memset(rect, 0, sizeof(int) * rectx * recty);
}


/*************************** multiview functions *****************************/

bool RE_HasFakeLayer(RenderResult *res)
{
	RenderView *rv;

	if (res == NULL)
		return false;

	rv = res->views.first;
	if (rv == NULL)
		return false;

	return (rv->rect32 || rv->rectf);
}

bool RE_RenderResult_is_stereo(RenderResult *res)
{
	if (! BLI_findstring(&res->views, STEREO_LEFT_NAME, offsetof(RenderView, name)))
		return false;

	if (! BLI_findstring(&res->views, STEREO_RIGHT_NAME, offsetof(RenderView, name)))
		return false;

	return true;
}

void RE_RenderViewSetRectf(RenderResult *res, const int view_id, float *rect)
{
	RenderView *rv = BLI_findlink(&res->views, view_id);
	if (rv) {
		rv->rectf = rect;
	}
}

void RE_RenderViewSetRectz(RenderResult *res, const int view_id, float *rect)
{
	RenderView *rv = BLI_findlink(&res->views, view_id);
	if (rv) {
		rv->rectz = rect;
	}
}

float *RE_RenderViewGetRectz(RenderResult *res, const int view_id)
{
	RenderView *rv = BLI_findlink(&res->views, view_id);
	if (rv) {
		return rv->rectz;
	}
	return res->rectz;
}

float *RE_RenderViewGetRectf(RenderResult *res, const int view_id)
{
	RenderView *rv = BLI_findlink(&res->views, view_id);
	if (rv) {
		return rv->rectf;
	}
	return res->rectf;
}

int *RE_RenderViewGetRect32(RenderResult *res, const int view_id)
{
	RenderView *rv = BLI_findlink(&res->views, view_id);
	if (rv) {
		return rv->rect32;
	}
	return res->rect32;
}


