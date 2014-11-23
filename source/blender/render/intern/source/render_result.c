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
#include "BLI_system.h"
#include "BLI_threads.h"

#include "BKE_image.h"
#include "BKE_global.h"
#include "BKE_main.h"
#include "BKE_report.h"
#ifdef WITH_CYCLES_DEBUG
#  include "BKE_scene.h"
#endif

#include "IMB_imbuf.h"
#include "IMB_imbuf_types.h"
#include "IMB_colormanagement.h"

#include "intern/openexr/openexr_multi.h"

#include "render_result.h"
#include "render_types.h"

/********************************** Free *************************************/

void render_result_free(RenderResult *res)
{
	if (res == NULL) return;

	while (res->layers.first) {
		RenderLayer *rl = res->layers.first;
		
		if (rl->rectf) MEM_freeN(rl->rectf);
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
	
	if (res->rect32)
		MEM_freeN(res->rect32);
	if (res->rectz)
		MEM_freeN(res->rectz);
	if (res->rectf)
		MEM_freeN(res->rectf);
	if (res->text)
		MEM_freeN(res->text);
	
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

/********************************* Names *************************************/

/* NOTE: OpenEXR only supports 32 chars for layer+pass names
 * In blender we now use max 10 chars for pass, max 20 for layer */
static const char *get_pass_name(int passtype, int channel)
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
	
	if (strcmp(str, "Combined") == 0)
		return SCE_PASS_COMBINED;

	if (strcmp(str, "Depth") == 0)
		return SCE_PASS_Z;

	if (strcmp(str, "Vector") == 0)
		return SCE_PASS_VECTOR;

	if (strcmp(str, "Normal") == 0)
		return SCE_PASS_NORMAL;

	if (strcmp(str, "UV") == 0)
		return SCE_PASS_UV;

	if (strcmp(str, "Color") == 0)
		return SCE_PASS_RGBA;

	if (strcmp(str, "Emit") == 0)
		return SCE_PASS_EMIT;

	if (strcmp(str, "Diffuse") == 0)
		return SCE_PASS_DIFFUSE;

	if (strcmp(str, "Spec") == 0)
		return SCE_PASS_SPEC;

	if (strcmp(str, "Shadow") == 0)
		return SCE_PASS_SHADOW;
	
	if (strcmp(str, "AO") == 0)
		return SCE_PASS_AO;

	if (strcmp(str, "Env") == 0)
		return SCE_PASS_ENVIRONMENT;

	if (strcmp(str, "Indirect") == 0)
		return SCE_PASS_INDIRECT;

	if (strcmp(str, "Reflect") == 0)
		return SCE_PASS_REFLECT;

	if (strcmp(str, "Refract") == 0)
		return SCE_PASS_REFRACT;

	if (strcmp(str, "IndexOB") == 0)
		return SCE_PASS_INDEXOB;

	if (strcmp(str, "IndexMA") == 0)
		return SCE_PASS_INDEXMA;

	if (strcmp(str, "Mist") == 0)
		return SCE_PASS_MIST;
	
	if (strcmp(str, "RayHits") == 0)
		return SCE_PASS_RAYHITS;

	if (strcmp(str, "DiffDir") == 0)
		return SCE_PASS_DIFFUSE_DIRECT;

	if (strcmp(str, "DiffInd") == 0)
		return SCE_PASS_DIFFUSE_INDIRECT;

	if (strcmp(str, "DiffCol") == 0)
		return SCE_PASS_DIFFUSE_COLOR;

	if (strcmp(str, "GlossDir") == 0)
		return SCE_PASS_GLOSSY_DIRECT;

	if (strcmp(str, "GlossInd") == 0)
		return SCE_PASS_GLOSSY_INDIRECT;

	if (strcmp(str, "GlossCol") == 0)
		return SCE_PASS_GLOSSY_COLOR;

	if (strcmp(str, "TransDir") == 0)
		return SCE_PASS_TRANSM_DIRECT;

	if (strcmp(str, "TransInd") == 0)
		return SCE_PASS_TRANSM_INDIRECT;

	if (strcmp(str, "TransCol") == 0)
		return SCE_PASS_TRANSM_COLOR;
		
	if (strcmp(str, "SubsurfaceDir") == 0)
		return SCE_PASS_SUBSURFACE_DIRECT;

	if (strcmp(str, "SubsurfaceInd") == 0)
		return SCE_PASS_SUBSURFACE_INDIRECT;

	if (strcmp(str, "SubsurfaceCol") == 0)
		return SCE_PASS_SUBSURFACE_COLOR;

	return 0;
}

/********************************** New **************************************/

static RenderPass *render_layer_add_pass(RenderResult *rr, RenderLayer *rl, int channels, int passtype)
{
	const char *typestr = get_pass_name(passtype, 0);
	RenderPass *rpass = MEM_callocN(sizeof(RenderPass), typestr);
	int rectsize = rr->rectx * rr->recty * channels;
	
	BLI_addtail(&rl->passes, rpass);
	rpass->passtype = passtype;
	rpass->channels = channels;
	rpass->rectx = rl->rectx;
	rpass->recty = rl->recty;
	BLI_strncpy(rpass->name, get_pass_name(rpass->passtype, -1), sizeof(rpass->name));
	
	if (rl->exrhandle) {
		int a;
		for (a = 0; a < channels; a++)
			IMB_exr_add_channel(rl->exrhandle, rl->name, get_pass_name(passtype, a), 0, 0, NULL);
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
                                               int debug_type)
{
	RenderPass *rpass = render_layer_add_pass(rr, rl, channels, pass_type);
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
RenderResult *render_result_new(Render *re, rcti *partrct, int crop, int savebuffers, const char *layername)
{
	RenderResult *rr;
	RenderLayer *rl;
	SceneRenderLayer *srl;
	int rectx, recty, nr;
	
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

	/* check renderdata for amount of layers */
	for (nr = 0, srl = re->r.layers.first; srl; srl = srl->next, nr++) {

		if (layername && layername[0])
			if (strcmp(srl->name, layername) != 0)
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

			IMB_exr_add_channel(rl->exrhandle, rl->name, "Combined.R", 0, 0, NULL);
			IMB_exr_add_channel(rl->exrhandle, rl->name, "Combined.G", 0, 0, NULL);
			IMB_exr_add_channel(rl->exrhandle, rl->name, "Combined.B", 0, 0, NULL);
			IMB_exr_add_channel(rl->exrhandle, rl->name, "Combined.A", 0, 0, NULL);
		}
		else
			rl->rectf = MEM_mapallocN(rectx * recty * sizeof(float) * 4, "Combined rgba");
		
		if (srl->passflag  & SCE_PASS_Z)
			render_layer_add_pass(rr, rl, 1, SCE_PASS_Z);
		if (srl->passflag  & SCE_PASS_VECTOR)
			render_layer_add_pass(rr, rl, 4, SCE_PASS_VECTOR);
		if (srl->passflag  & SCE_PASS_NORMAL)
			render_layer_add_pass(rr, rl, 3, SCE_PASS_NORMAL);
		if (srl->passflag  & SCE_PASS_UV) 
			render_layer_add_pass(rr, rl, 3, SCE_PASS_UV);
		if (srl->passflag  & SCE_PASS_RGBA)
			render_layer_add_pass(rr, rl, 4, SCE_PASS_RGBA);
		if (srl->passflag  & SCE_PASS_EMIT)
			render_layer_add_pass(rr, rl, 3, SCE_PASS_EMIT);
		if (srl->passflag  & SCE_PASS_DIFFUSE)
			render_layer_add_pass(rr, rl, 3, SCE_PASS_DIFFUSE);
		if (srl->passflag  & SCE_PASS_SPEC)
			render_layer_add_pass(rr, rl, 3, SCE_PASS_SPEC);
		if (srl->passflag  & SCE_PASS_AO)
			render_layer_add_pass(rr, rl, 3, SCE_PASS_AO);
		if (srl->passflag  & SCE_PASS_ENVIRONMENT)
			render_layer_add_pass(rr, rl, 3, SCE_PASS_ENVIRONMENT);
		if (srl->passflag  & SCE_PASS_INDIRECT)
			render_layer_add_pass(rr, rl, 3, SCE_PASS_INDIRECT);
		if (srl->passflag  & SCE_PASS_SHADOW)
			render_layer_add_pass(rr, rl, 3, SCE_PASS_SHADOW);
		if (srl->passflag  & SCE_PASS_REFLECT)
			render_layer_add_pass(rr, rl, 3, SCE_PASS_REFLECT);
		if (srl->passflag  & SCE_PASS_REFRACT)
			render_layer_add_pass(rr, rl, 3, SCE_PASS_REFRACT);
		if (srl->passflag  & SCE_PASS_INDEXOB)
			render_layer_add_pass(rr, rl, 1, SCE_PASS_INDEXOB);
		if (srl->passflag  & SCE_PASS_INDEXMA)
			render_layer_add_pass(rr, rl, 1, SCE_PASS_INDEXMA);
		if (srl->passflag  & SCE_PASS_MIST)
			render_layer_add_pass(rr, rl, 1, SCE_PASS_MIST);
		if (rl->passflag & SCE_PASS_RAYHITS)
			render_layer_add_pass(rr, rl, 4, SCE_PASS_RAYHITS);
		if (srl->passflag  & SCE_PASS_DIFFUSE_DIRECT)
			render_layer_add_pass(rr, rl, 3, SCE_PASS_DIFFUSE_DIRECT);
		if (srl->passflag  & SCE_PASS_DIFFUSE_INDIRECT)
			render_layer_add_pass(rr, rl, 3, SCE_PASS_DIFFUSE_INDIRECT);
		if (srl->passflag  & SCE_PASS_DIFFUSE_COLOR)
			render_layer_add_pass(rr, rl, 3, SCE_PASS_DIFFUSE_COLOR);
		if (srl->passflag  & SCE_PASS_GLOSSY_DIRECT)
			render_layer_add_pass(rr, rl, 3, SCE_PASS_GLOSSY_DIRECT);
		if (srl->passflag  & SCE_PASS_GLOSSY_INDIRECT)
			render_layer_add_pass(rr, rl, 3, SCE_PASS_GLOSSY_INDIRECT);
		if (srl->passflag  & SCE_PASS_GLOSSY_COLOR)
			render_layer_add_pass(rr, rl, 3, SCE_PASS_GLOSSY_COLOR);
		if (srl->passflag  & SCE_PASS_TRANSM_DIRECT)
			render_layer_add_pass(rr, rl, 3, SCE_PASS_TRANSM_DIRECT);
		if (srl->passflag  & SCE_PASS_TRANSM_INDIRECT)
			render_layer_add_pass(rr, rl, 3, SCE_PASS_TRANSM_INDIRECT);
		if (srl->passflag  & SCE_PASS_TRANSM_COLOR)
			render_layer_add_pass(rr, rl, 3, SCE_PASS_TRANSM_COLOR);
		if (srl->passflag  & SCE_PASS_SUBSURFACE_DIRECT)
			render_layer_add_pass(rr, rl, 3, SCE_PASS_SUBSURFACE_DIRECT);
		if (srl->passflag  & SCE_PASS_SUBSURFACE_INDIRECT)
			render_layer_add_pass(rr, rl, 3, SCE_PASS_SUBSURFACE_INDIRECT);
		if (srl->passflag  & SCE_PASS_SUBSURFACE_COLOR)
			render_layer_add_pass(rr, rl, 3, SCE_PASS_SUBSURFACE_COLOR);

#ifdef WITH_CYCLES_DEBUG
		if(BKE_scene_use_new_shading_nodes(re->scene)) {
			render_layer_add_debug_pass(rr, rl, 1, SCE_PASS_DEBUG,
			        RENDER_PASS_DEBUG_BVH_TRAVERSAL_STEPS);
		}
#endif
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

			IMB_exr_add_channel(rl->exrhandle, rl->name, "Combined.R", 0, 0, NULL);
			IMB_exr_add_channel(rl->exrhandle, rl->name, "Combined.G", 0, 0, NULL);
			IMB_exr_add_channel(rl->exrhandle, rl->name, "Combined.B", 0, 0, NULL);
			IMB_exr_add_channel(rl->exrhandle, rl->name, "Combined.A", 0, 0, NULL);
		}
		else {
			rl->rectf = MEM_mapallocN(rectx * recty * sizeof(float) * 4, "Combined rgba");
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
RenderResult *render_result_new_full_sample(Render *re, ListBase *lb, rcti *partrct, int crop, int savebuffers)
{
	int a;
	
	if (re->osa == 0)
		return render_result_new(re, partrct, crop, savebuffers, RR_ALL_LAYERS);
	
	for (a = 0; a < re->osa; a++) {
		RenderResult *rr = render_result_new(re, partrct, crop, savebuffers, RR_ALL_LAYERS);
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

static void ml_addpass_cb(void *UNUSED(base), void *lay, const char *str, float *rect, int totchan, const char *chan_id)
{
	RenderLayer *rl = lay;
	RenderPass *rpass = MEM_callocN(sizeof(RenderPass), "loaded pass");
	int a;
	
	BLI_addtail(&rl->passes, rpass);
	rpass->channels = totchan;

	rpass->passtype = passtype_from_name(str);
	if (rpass->passtype == 0) printf("unknown pass %s\n", str);
	rl->passflag |= rpass->passtype;
	
	BLI_strncpy(rpass->name, str, EXR_PASS_MAXNAME);
	/* channel id chars */
	for (a = 0; a < totchan; a++)
		rpass->chan_id[a] = chan_id[a];
	
	rpass->rect = rect;
}

/* from imbuf, if a handle was returned we convert this to render result */
RenderResult *render_result_new_from_exr(void *exrhandle, const char *colorspace, bool predivide, int rectx, int recty)
{
	RenderResult *rr = MEM_callocN(sizeof(RenderResult), __func__);
	RenderLayer *rl;
	RenderPass *rpass;
	const char *to_colorspace = IMB_colormanagement_role_colorspace_name_get(COLOR_ROLE_SCENE_LINEAR);

	rr->rectx = rectx;
	rr->recty = recty;
	
	IMB_exr_multilayer_convert(exrhandle, rr, ml_addlayer_cb, ml_addpass_cb);

	for (rl = rr->layers.first; rl; rl = rl->next) {
		rl->rectx = rectx;
		rl->recty = recty;

		for (rpass = rl->passes.first; rpass; rpass = rpass->next) {
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
			/* combined */
			if (rl->rectf && rlp->rectf)
				do_merge_tile(rr, rrpart, rl->rectf, rlp->rectf, 4);

			/* passes are allocated in sync */
			for (rpass = rl->passes.first, rpassp = rlp->passes.first;
			     rpass && rpassp;
			     rpass = rpass->next, rpassp = rpassp->next)
			{
				do_merge_tile(rr, rrpart, rpass->rect, rpassp->rect, rpass->channels);
			}
		}
	}
}

/* for passes read from files, these have names stored */
static char *make_pass_name(RenderPass *rpass, int chan)
{
	static char name[16];
	int len;
	
	BLI_strncpy(name, rpass->name, EXR_PASS_MAXNAME);
	len = strlen(name);
	name[len] = '.';
	name[len + 1] = rpass->chan_id[chan];
	name[len + 2] = 0;

	return name;
}

/* filename already made absolute */
/* called from within UI, saves both rendered result as a file-read result */
bool RE_WriteRenderResult(ReportList *reports, RenderResult *rr, const char *filename, int compress)
{
	RenderLayer *rl;
	RenderPass *rpass;
	void *exrhandle = IMB_exr_get_handle();
	bool success;

	BLI_make_existing_file(filename);
	
	/* composite result */
	if (rr->rectf) {
		IMB_exr_add_channel(exrhandle, "Composite", "Combined.R", 4, 4 * rr->rectx, rr->rectf);
		IMB_exr_add_channel(exrhandle, "Composite", "Combined.G", 4, 4 * rr->rectx, rr->rectf + 1);
		IMB_exr_add_channel(exrhandle, "Composite", "Combined.B", 4, 4 * rr->rectx, rr->rectf + 2);
		IMB_exr_add_channel(exrhandle, "Composite", "Combined.A", 4, 4 * rr->rectx, rr->rectf + 3);
	}
	
	/* add layers/passes and assign channels */
	for (rl = rr->layers.first; rl; rl = rl->next) {
		
		/* combined */
		if (rl->rectf) {
			int a, xstride = 4;
			for (a = 0; a < xstride; a++) {
				IMB_exr_add_channel(exrhandle, rl->name, get_pass_name(SCE_PASS_COMBINED, a),
				                    xstride, xstride * rr->rectx, rl->rectf + a);
			}
		}
		
		/* passes are allocated in sync */
		for (rpass = rl->passes.first; rpass; rpass = rpass->next) {
			int a, xstride = rpass->channels;
			for (a = 0; a < xstride; a++) {
				if (rpass->passtype) {
					IMB_exr_add_channel(exrhandle, rl->name, get_pass_name(rpass->passtype, a),
					                    xstride, xstride * rr->rectx, rpass->rect + a);
				}
				else {
					IMB_exr_add_channel(exrhandle, rl->name, make_pass_name(rpass, a),
					                    xstride, xstride * rr->rectx, rpass->rect + a);
				}
			}
		}
	}

	/* when the filename has no permissions, this can fail */
	if (IMB_exr_begin_write(exrhandle, filename, rr->rectx, rr->recty, compress)) {
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

static void save_render_result_tile(RenderResult *rr, RenderResult *rrpart)
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
		
		/* combined */
		if (rlp->rectf) {
			int a, xstride = 4;
			for (a = 0; a < xstride; a++) {
				IMB_exr_set_channel(rl->exrhandle, rlp->name, get_pass_name(SCE_PASS_COMBINED, a), 
				                    xstride, xstride * rrpart->rectx, rlp->rectf + a + xstride * offs);
			}
		}
		
		/* passes are allocated in sync */
		for (rpassp = rlp->passes.first; rpassp; rpassp = rpassp->next) {
			int a, xstride = rpassp->channels;
			for (a = 0; a < xstride; a++) {
				IMB_exr_set_channel(rl->exrhandle, rlp->name, get_pass_name(rpassp->passtype, a), 
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

		IMB_exrtile_write_channels(rl->exrhandle, partx, party, 0);
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
			IMB_exrtile_clear_channels(rl->exrhandle);
		
			for (pa = re->parts.first; pa; pa = pa->next) {
				if (pa->status != PART_STATUS_READY) {
					int party = pa->disprect.ymin - re->disprect.ymin + pa->crop;
					int partx = pa->disprect.xmin - re->disprect.xmin + pa->crop;
					IMB_exrtile_write_channels(rl->exrhandle, partx, party, 0);
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
void render_result_exr_file_merge(RenderResult *rr, RenderResult *rrpart)
{
	for (; rr && rrpart; rr = rr->next, rrpart = rrpart->next)
		save_render_result_tile(rr, rrpart);
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

	BLI_make_file_string("/", filepath, BLI_temp_dir_session(), name);
}

/* only for temp buffer, makes exact copy of render result */
int render_result_exr_file_read_sample(Render *re, int sample)
{
	RenderLayer *rl;
	char str[FILE_MAXFILE + MAX_ID_NAME + MAX_ID_NAME + 100] = "";
	bool success = true;

	RE_FreeRenderResult(re->result);
	re->result = render_result_new(re, &re->disprect, 0, RR_USE_MEM, RR_ALL_LAYERS);

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

		/* combined */
		if (rl->rectf) {
			int a, xstride = 4;
			for (a = 0; a < xstride; a++)
				IMB_exr_set_channel(exrhandle, rl->name, get_pass_name(SCE_PASS_COMBINED, a), 
				                    xstride, xstride * rectx, rl->rectf + a);
		}
		
		/* passes are allocated in sync */
		for (rpass = rl->passes.first; rpass; rpass = rpass->next) {
			int a, xstride = rpass->channels;
			for (a = 0; a < xstride; a++)
				IMB_exr_set_channel(exrhandle, rl->name, get_pass_name(rpass->passtype, a), 
				                    xstride, xstride * rectx, rpass->rect + a);

			BLI_strncpy(rpass->name, get_pass_name(rpass->passtype, -1), sizeof(rpass->name));
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
		BLI_strncpy(dirname, BLI_temp_dir_base(), sizeof(dirname));
		BLI_strncpy(filename, "UNSAVED", sizeof(filename));
	}
	BLI_hash_md5_to_hexdigest(path_digest, path_hexdigest);

	/* Default to *non-volatile* tmp dir. */
	if (*root == '\0') {
		root = BLI_temp_dir_base();
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
	RE_WriteRenderResult(NULL, rr, str, 0);
}

/* For cache, makes exact copy of render result */
bool render_result_exr_file_cache_read(Render *re)
{
	char str[FILE_MAXFILE + MAX_ID_NAME + MAX_ID_NAME + 100] = "";
	char *root = U.render_cachedir;

	RE_FreeRenderResult(re->result);
	re->result = render_result_new(re, &re->disprect, 0, RR_USE_MEM, RR_ALL_LAYERS);

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

ImBuf *render_result_rect_to_ibuf(RenderResult *rr, RenderData *rd)
{
	ImBuf *ibuf = IMB_allocImBuf(rr->rectx, rr->recty, rd->im_format.planes, 0);
	
	/* if not exists, BKE_imbuf_write makes one */
	ibuf->rect = (unsigned int *)rr->rect32;
	ibuf->rect_float = rr->rectf;
	ibuf->zbuf_float = rr->rectz;
	
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

void render_result_rect_from_ibuf(RenderResult *rr, RenderData *UNUSED(rd), ImBuf *ibuf)
{
	if (ibuf->rect_float) {
		if (!rr->rectf)
			rr->rectf = MEM_mallocN(4 * sizeof(float) * rr->rectx * rr->recty, "render_seq rectf");
		
		memcpy(rr->rectf, ibuf->rect_float, 4 * sizeof(float) * rr->rectx * rr->recty);

		/* TSK! Since sequence render doesn't free the *rr render result, the old rect32
		 * can hang around when sequence render has rendered a 32 bits one before */
		if (rr->rect32) {
			MEM_freeN(rr->rect32);
			rr->rect32 = NULL;
		}
	}
	else if (ibuf->rect) {
		if (!rr->rect32)
			rr->rect32 = MEM_mallocN(sizeof(int) * rr->rectx * rr->recty, "render_seq rect");

		memcpy(rr->rect32, ibuf->rect, 4 * rr->rectx * rr->recty);

		/* Same things as above, old rectf can hang around from previous render. */
		if (rr->rectf) {
			MEM_freeN(rr->rectf);
			rr->rectf = NULL;
		}
	}
}

void render_result_rect_fill_zero(RenderResult *rr)
{
	if (rr->rectf)
		memset(rr->rectf, 0, 4 * sizeof(float) * rr->rectx * rr->recty);
	else if (rr->rect32)
		memset(rr->rect32, 0, 4 * rr->rectx * rr->recty);
	else
		rr->rect32 = MEM_callocN(sizeof(int) * rr->rectx * rr->recty, "render_seq rect");
}

void render_result_rect_get_pixels(RenderResult *rr, unsigned int *rect, int rectx, int recty,
                                   const ColorManagedViewSettings *view_settings, const ColorManagedDisplaySettings *display_settings)
{
	if (rr->rect32) {
		memcpy(rect, rr->rect32, sizeof(int) * rr->rectx * rr->recty);
	}
	else if (rr->rectf) {
		IMB_display_buffer_transform_apply((unsigned char *) rect, rr->rectf, rr->rectx, rr->recty, 4,
		                                   view_settings, display_settings, true);
	}
	else
		/* else fill with black */
		memset(rect, 0, sizeof(int) * rectx * recty);
}

