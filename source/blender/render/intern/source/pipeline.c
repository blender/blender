/*  
 *
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

/** \file blender/render/intern/source/pipeline.c
 *  \ingroup render
 */


#include <math.h>
#include <limits.h>
#include <string.h>
#include <stdlib.h>
#include <stddef.h>

#include "DNA_group_types.h"
#include "DNA_image_types.h"
#include "DNA_node_types.h"
#include "DNA_object_types.h"
#include "DNA_scene_types.h"
#include "DNA_sequence_types.h"
#include "DNA_userdef_types.h"

#include "MEM_guardedalloc.h"

#include "BKE_animsys.h"	/* <------ should this be here?, needed for sequencer update */
#include "BKE_camera.h"
#include "BKE_global.h"
#include "BKE_image.h"
#include "BKE_main.h"
#include "BKE_node.h"
#include "BKE_object.h"
#include "BKE_pointcache.h"
#include "BKE_report.h"
#include "BKE_scene.h"
#include "BKE_sequencer.h"
#include "BKE_utildefines.h"
#include "BKE_writeavi.h"	/* <------ should be replaced once with generic movie module */

#include "BLI_math.h"
#include "BLI_blenlib.h"
#include "BLI_rand.h"
#include "BLI_threads.h"
#include "BLI_callbacks.h"
#include "BLI_utildefines.h"

#include "PIL_time.h"
#include "IMB_imbuf.h"
#include "IMB_imbuf_types.h"

#include "intern/openexr/openexr_multi.h"

#include "RE_engine.h"
#include "RE_pipeline.h"

/* internal */
#include "render_types.h"
#include "renderpipeline.h"
#include "renderdatabase.h"
#include "rendercore.h"
#include "envmap.h"
#include "initrender.h"
#include "shadbuf.h"
#include "pixelblending.h"
#include "zbuf.h"


/* render flow

1) Initialize state
- state data, tables
- movie/image file init
- everything that doesn't change during animation

2) Initialize data
- camera, world, matrices
- make render verts, faces, halos, strands
- everything can change per frame/field

3) Render Processor
- multiple layers
- tiles, rect, baking
- layers/tiles optionally to disk or directly in Render Result

4) Composite Render Result
- also read external files etc

5) Image Files
- save file or append in movie

*/


/* ********* globals ******** */

/* here we store all renders */
static struct {
	ListBase renderlist;

	/* commandline thread override */
	int threads;
} RenderGlobal = {{NULL, NULL}, -1}; 

/* hardcopy of current render, used while rendering for speed */
Render R;

/* ********* alloc and free ******** */

static int do_write_image_or_movie(Render *re, Main *bmain, Scene *scene, bMovieHandle *mh, const char *name_override);

static volatile int g_break= 0;
static int thread_break(void *UNUSED(arg))
{
	return g_break;
}

/* default callbacks, set in each new render */
static void result_nothing(void *UNUSED(arg), RenderResult *UNUSED(rr)) {}
static void result_rcti_nothing(void *UNUSED(arg), RenderResult *UNUSED(rr), volatile struct rcti *UNUSED(rect)) {}
static void stats_nothing(void *UNUSED(arg), RenderStats *UNUSED(rs)) {}
static void float_nothing(void *UNUSED(arg), float UNUSED(val)) {}
static int default_break(void *UNUSED(arg)) {return G.afbreek == 1;}

static void stats_background(void *UNUSED(arg), RenderStats *rs)
{
	uintptr_t mem_in_use, mmap_in_use, peak_memory;
	float megs_used_memory, mmap_used_memory, megs_peak_memory;

	mem_in_use= MEM_get_memory_in_use();
	mmap_in_use= MEM_get_mapped_memory_in_use();
	peak_memory = MEM_get_peak_memory();

	megs_used_memory= (mem_in_use-mmap_in_use)/(1024.0*1024.0);
	mmap_used_memory= (mmap_in_use)/(1024.0*1024.0);
	megs_peak_memory = (peak_memory)/(1024.0*1024.0);

	fprintf(stdout, "Fra:%d Mem:%.2fM (%.2fM, peak %.2fM) ", rs->cfra,
	        megs_used_memory, mmap_used_memory, megs_peak_memory);

	if(rs->curfield)
		fprintf(stdout, "Field %d ", rs->curfield);
	if(rs->curblur)
		fprintf(stdout, "Blur %d ", rs->curblur);

	if(rs->infostr) {
		fprintf(stdout, "| %s", rs->infostr);
	}
	else {
		if(rs->tothalo)
			fprintf(stdout, "Sce: %s Ve:%d Fa:%d Ha:%d La:%d", rs->scenename, rs->totvert, rs->totface, rs->tothalo, rs->totlamp);
		else
			fprintf(stdout, "Sce: %s Ve:%d Fa:%d La:%d", rs->scenename, rs->totvert, rs->totface, rs->totlamp);
	}

	BLI_exec_cb(G.main, NULL, BLI_CB_EVT_RENDER_STATS);

	fputc('\n', stdout);
	fflush(stdout);
}

void RE_FreeRenderResult(RenderResult *res)
{
	if(res==NULL) return;

	while(res->layers.first) {
		RenderLayer *rl= res->layers.first;
		
		if(rl->rectf) MEM_freeN(rl->rectf);
		/* acolrect and scolrect are optionally allocated in shade_tile, only free here since it can be used for drawing */
		if(rl->acolrect) MEM_freeN(rl->acolrect);
		if(rl->scolrect) MEM_freeN(rl->scolrect);
		
		while(rl->passes.first) {
			RenderPass *rpass= rl->passes.first;
			if(rpass->rect) MEM_freeN(rpass->rect);
			BLI_remlink(&rl->passes, rpass);
			MEM_freeN(rpass);
		}
		BLI_remlink(&res->layers, rl);
		MEM_freeN(rl);
	}
	
	if(res->rect32)
		MEM_freeN(res->rect32);
	if(res->rectz)
		MEM_freeN(res->rectz);
	if(res->rectf)
		MEM_freeN(res->rectf);
	if(res->text)
		MEM_freeN(res->text);
	
	MEM_freeN(res);
}

/* version that's compatible with fullsample buffers */
void free_render_result(ListBase *lb, RenderResult *rr)
{
	RenderResult *rrnext;
	
	for(; rr; rr= rrnext) {
		rrnext= rr->next;
		
		if(lb && lb->first)
			BLI_remlink(lb, rr);
		
		RE_FreeRenderResult(rr);
	}
}


/* all layers except the active one get temporally pushed away */
static void push_render_result(Render *re)
{
	BLI_rw_mutex_lock(&re->resultmutex, THREAD_LOCK_WRITE);

	/* officially pushed result should be NULL... error can happen with do_seq */
	RE_FreeRenderResult(re->pushedresult);
	
	re->pushedresult= re->result;
	re->result= NULL;

	BLI_rw_mutex_unlock(&re->resultmutex);
}

/* if scemode is R_SINGLE_LAYER, at end of rendering, merge the both render results */
static void pop_render_result(Render *re)
{
	if(re->result==NULL) {
		printf("pop render result error; no current result!\n");
		return;
	}

	if(re->pushedresult) {
		BLI_rw_mutex_lock(&re->resultmutex, THREAD_LOCK_WRITE);

		if(re->pushedresult->rectx==re->result->rectx && re->pushedresult->recty==re->result->recty) {
			/* find which layer in pushedresult should be replaced */
			SceneRenderLayer *srl;
			RenderLayer *rlpush;
			RenderLayer *rl= re->result->layers.first;
			int nr;
			
			/* render result should be empty after this */
			BLI_remlink(&re->result->layers, rl);
			
			/* reconstruct render result layers */
			for(nr=0, srl= re->scene->r.layers.first; srl; srl= srl->next, nr++) {
				if(nr==re->r.actlay)
					BLI_addtail(&re->result->layers, rl);
				else {
					rlpush= RE_GetRenderLayer(re->pushedresult, srl->name);
					if(rlpush) {
						BLI_remlink(&re->pushedresult->layers, rlpush);
						BLI_addtail(&re->result->layers, rlpush);
					}
				}
			}
		}
		
		RE_FreeRenderResult(re->pushedresult);
		re->pushedresult= NULL;

		BLI_rw_mutex_unlock(&re->resultmutex);
	}
}

/* NOTE: OpenEXR only supports 32 chars for layer+pass names
   In blender we now use max 10 chars for pass, max 20 for layer */
static const char *get_pass_name(int passtype, int channel)
{
	
	if(passtype == SCE_PASS_COMBINED) {
		if(channel==-1) return "Combined";
		if(channel==0) return "Combined.R";
		if(channel==1) return "Combined.G";
		if(channel==2) return "Combined.B";
		return "Combined.A";
	}
	if(passtype == SCE_PASS_Z) {
		if(channel==-1) return "Depth";
		return "Depth.Z";
	}
	if(passtype == SCE_PASS_VECTOR) {
		if(channel==-1) return "Vector";
		if(channel==0) return "Vector.X";
		if(channel==1) return "Vector.Y";
		if(channel==2) return "Vector.Z";
		return "Vector.W";
	}
	if(passtype == SCE_PASS_NORMAL) {
		if(channel==-1) return "Normal";
		if(channel==0) return "Normal.X";
		if(channel==1) return "Normal.Y";
		return "Normal.Z";
	}
	if(passtype == SCE_PASS_UV) {
		if(channel==-1) return "UV";
		if(channel==0) return "UV.U";
		if(channel==1) return "UV.V";
		return "UV.A";
	}
	if(passtype == SCE_PASS_RGBA) {
		if(channel==-1) return "Color";
		if(channel==0) return "Color.R";
		if(channel==1) return "Color.G";
		if(channel==2) return "Color.B";
		return "Color.A";
	}
	if(passtype == SCE_PASS_EMIT) {
		if(channel==-1) return "Emit";
		if(channel==0) return "Emit.R";
		if(channel==1) return "Emit.G";
		return "Emit.B";
	}
	if(passtype == SCE_PASS_DIFFUSE) {
		if(channel==-1) return "Diffuse";
		if(channel==0) return "Diffuse.R";
		if(channel==1) return "Diffuse.G";
		return "Diffuse.B";
	}
	if(passtype == SCE_PASS_SPEC) {
		if(channel==-1) return "Spec";
		if(channel==0) return "Spec.R";
		if(channel==1) return "Spec.G";
		return "Spec.B";
	}
	if(passtype == SCE_PASS_SHADOW) {
		if(channel==-1) return "Shadow";
		if(channel==0) return "Shadow.R";
		if(channel==1) return "Shadow.G";
		return "Shadow.B";
	}
	if(passtype == SCE_PASS_AO) {
		if(channel==-1) return "AO";
		if(channel==0) return "AO.R";
		if(channel==1) return "AO.G";
		return "AO.B";
	}
	if(passtype == SCE_PASS_ENVIRONMENT) {
		if(channel==-1) return "Env";
		if(channel==0) return "Env.R";
		if(channel==1) return "Env.G";
		return "Env.B";
	}
	if(passtype == SCE_PASS_INDIRECT) {
		if(channel==-1) return "Indirect";
		if(channel==0) return "Indirect.R";
		if(channel==1) return "Indirect.G";
		return "Indirect.B";
	}
	if(passtype == SCE_PASS_REFLECT) {
		if(channel==-1) return "Reflect";
		if(channel==0) return "Reflect.R";
		if(channel==1) return "Reflect.G";
		return "Reflect.B";
	}
	if(passtype == SCE_PASS_REFRACT) {
		if(channel==-1) return "Refract";
		if(channel==0) return "Refract.R";
		if(channel==1) return "Refract.G";
		return "Refract.B";
	}
	if(passtype == SCE_PASS_INDEXOB) {
		if(channel==-1) return "IndexOB";
		return "IndexOB.X";
	}
	if(passtype == SCE_PASS_INDEXMA) {
		if(channel==-1) return "IndexMA";
		return "IndexMA.X";
	}
	if(passtype == SCE_PASS_MIST) {
		if(channel==-1) return "Mist";
		return "Mist.Z";
	}
	if(passtype == SCE_PASS_RAYHITS)
	{
		if(channel==-1) return "Rayhits";
		if(channel==0) return "Rayhits.R";
		if(channel==1) return "Rayhits.G";
		return "Rayhits.B";
	}
	return "Unknown";
}

static int passtype_from_name(const char *str)
{
	
	if(strcmp(str, "Combined")==0)
		return SCE_PASS_COMBINED;

	if(strcmp(str, "Depth")==0)
		return SCE_PASS_Z;

	if(strcmp(str, "Vector")==0)
		return SCE_PASS_VECTOR;

	if(strcmp(str, "Normal")==0)
		return SCE_PASS_NORMAL;

	if(strcmp(str, "UV")==0)
		return SCE_PASS_UV;

	if(strcmp(str, "Color")==0)
		return SCE_PASS_RGBA;

	if(strcmp(str, "Emit")==0)
		return SCE_PASS_EMIT;

	if(strcmp(str, "Diffuse")==0)
		return SCE_PASS_DIFFUSE;

	if(strcmp(str, "Spec")==0)
		return SCE_PASS_SPEC;

	if(strcmp(str, "Shadow")==0)
		return SCE_PASS_SHADOW;
	
	if(strcmp(str, "AO")==0)
		return SCE_PASS_AO;

	if(strcmp(str, "Env")==0)
		return SCE_PASS_ENVIRONMENT;

	if(strcmp(str, "Indirect")==0)
		return SCE_PASS_INDIRECT;

	if(strcmp(str, "Reflect")==0)
		return SCE_PASS_REFLECT;

	if(strcmp(str, "Refract")==0)
		return SCE_PASS_REFRACT;

	if(strcmp(str, "IndexOB")==0)
		return SCE_PASS_INDEXOB;

	if(strcmp(str, "IndexMA")==0)
		return SCE_PASS_INDEXMA;

	if(strcmp(str, "Mist")==0)
		return SCE_PASS_MIST;
	
	if(strcmp(str, "RayHits")==0)
		return SCE_PASS_RAYHITS;
	return 0;
}

static void scene_unique_exr_name(Scene *scene, char *str, int sample)
{
	char di[FILE_MAX], name[FILE_MAXFILE+MAX_ID_NAME+100], fi[FILE_MAXFILE];
	
	BLI_strncpy(di, G.main->name, FILE_MAX);
	BLI_splitdirstring(di, fi);
	
	if(sample==0)
		BLI_snprintf(name, sizeof(name), "%s_%s.exr", fi, scene->id.name+2);
	else
		BLI_snprintf(name, sizeof(name), "%s_%s%d.exr", fi, scene->id.name+2, sample);

	BLI_make_file_string("/", str, BLI_temporary_dir(), name);
}

static void render_unique_exr_name(Render *re, char *str, int sample)
{
	scene_unique_exr_name(re->scene, str, sample);
}

static void render_layer_add_pass(RenderResult *rr, RenderLayer *rl, int channels, int passtype)
{
	const char *typestr= get_pass_name(passtype, 0);
	RenderPass *rpass= MEM_callocN(sizeof(RenderPass), typestr);
	int rectsize= rr->rectx*rr->recty*channels;
	
	BLI_addtail(&rl->passes, rpass);
	rpass->passtype= passtype;
	rpass->channels= channels;
	rpass->rectx= rl->rectx;
	rpass->recty= rl->recty;
	
	if(rr->exrhandle) {
		int a;
		for(a=0; a<channels; a++)
			IMB_exr_add_channel(rr->exrhandle, rl->name, get_pass_name(passtype, a), 0, 0, NULL);
	}
	else {
		float *rect;
		int x;
		
		rpass->rect= MEM_mapallocN(sizeof(float)*rectsize, typestr);
		
		if(passtype==SCE_PASS_VECTOR) {
			/* initialize to max speed */
			rect= rpass->rect;
			for(x= rectsize-1; x>=0; x--)
				rect[x]= PASS_VECTOR_MAX;
		}
		else if(passtype==SCE_PASS_Z) {
			rect= rpass->rect;
			for(x= rectsize-1; x>=0; x--)
				rect[x]= 10e10;
		}
	}
}

float *RE_RenderLayerGetPass(RenderLayer *rl, int passtype)
{
	RenderPass *rpass;
	
	for(rpass=rl->passes.first; rpass; rpass= rpass->next)
		if(rpass->passtype== passtype)
			return rpass->rect;
	return NULL;
}

RenderLayer *RE_GetRenderLayer(RenderResult *rr, const char *name)
{
	RenderLayer *rl;
	
	if(rr==NULL) return NULL;
	
	for(rl= rr->layers.first; rl; rl= rl->next)
		if(strncmp(rl->name, name, RE_MAXNAME)==0)
			return rl;
	return NULL;
}

/* called by main render as well for parts */
/* will read info from Render *re to define layers */
/* called in threads */
/* re->winx,winy is coordinate space of entire image, partrct the part within */
RenderResult *new_render_result(Render *re, rcti *partrct, int crop, int savebuffers)
{
	RenderResult *rr;
	RenderLayer *rl;
	SceneRenderLayer *srl;
	int rectx, recty, nr;
	
	rectx= partrct->xmax - partrct->xmin;
	recty= partrct->ymax - partrct->ymin;
	
	if(rectx<=0 || recty<=0)
		return NULL;
	
	rr= MEM_callocN(sizeof(RenderResult), "new render result");
	rr->rectx= rectx;
	rr->recty= recty;
	rr->renrect.xmin= 0; rr->renrect.xmax= rectx-2*crop;
	/* crop is one or two extra pixels rendered for filtering, is used for merging and display too */
	rr->crop= crop;
	
	/* tilerect is relative coordinates within render disprect. do not subtract crop yet */
	rr->tilerect.xmin= partrct->xmin - re->disprect.xmin;
	rr->tilerect.xmax= partrct->xmax - re->disprect.xmax;
	rr->tilerect.ymin= partrct->ymin - re->disprect.ymin;
	rr->tilerect.ymax= partrct->ymax - re->disprect.ymax;
	
	if(savebuffers) {
		rr->exrhandle= IMB_exr_get_handle();
	}
	
	/* check renderdata for amount of layers */
	for(nr=0, srl= re->r.layers.first; srl; srl= srl->next, nr++) {
		
		if((re->r.scemode & R_SINGLE_LAYER) && nr!=re->r.actlay)
			continue;
		if(srl->layflag & SCE_LAY_DISABLE)
			continue;
		
		rl= MEM_callocN(sizeof(RenderLayer), "new render layer");
		BLI_addtail(&rr->layers, rl);
		
		BLI_strncpy(rl->name, srl->name, sizeof(rl->name));
		rl->lay= srl->lay;
		rl->lay_zmask= srl->lay_zmask;
		rl->layflag= srl->layflag;
		rl->passflag= srl->passflag; // for debugging: srl->passflag|SCE_PASS_RAYHITS;
		rl->pass_xor= srl->pass_xor;
		rl->light_override= srl->light_override;
		rl->mat_override= srl->mat_override;
		rl->rectx= rectx;
		rl->recty= recty;
		
		if(rr->exrhandle) {
			IMB_exr_add_channel(rr->exrhandle, rl->name, "Combined.R", 0, 0, NULL);
			IMB_exr_add_channel(rr->exrhandle, rl->name, "Combined.G", 0, 0, NULL);
			IMB_exr_add_channel(rr->exrhandle, rl->name, "Combined.B", 0, 0, NULL);
			IMB_exr_add_channel(rr->exrhandle, rl->name, "Combined.A", 0, 0, NULL);
		}
		else
			rl->rectf= MEM_mapallocN(rectx*recty*sizeof(float)*4, "Combined rgba");
		
		if(srl->passflag  & SCE_PASS_Z)
			render_layer_add_pass(rr, rl, 1, SCE_PASS_Z);
		if(srl->passflag  & SCE_PASS_VECTOR)
			render_layer_add_pass(rr, rl, 4, SCE_PASS_VECTOR);
		if(srl->passflag  & SCE_PASS_NORMAL)
			render_layer_add_pass(rr, rl, 3, SCE_PASS_NORMAL);
		if(srl->passflag  & SCE_PASS_UV) 
			render_layer_add_pass(rr, rl, 3, SCE_PASS_UV);
		if(srl->passflag  & SCE_PASS_RGBA)
			render_layer_add_pass(rr, rl, 4, SCE_PASS_RGBA);
		if(srl->passflag  & SCE_PASS_EMIT)
			render_layer_add_pass(rr, rl, 3, SCE_PASS_EMIT);
		if(srl->passflag  & SCE_PASS_DIFFUSE)
			render_layer_add_pass(rr, rl, 3, SCE_PASS_DIFFUSE);
		if(srl->passflag  & SCE_PASS_SPEC)
			render_layer_add_pass(rr, rl, 3, SCE_PASS_SPEC);
		if(srl->passflag  & SCE_PASS_AO)
			render_layer_add_pass(rr, rl, 3, SCE_PASS_AO);
		if(srl->passflag  & SCE_PASS_ENVIRONMENT)
			render_layer_add_pass(rr, rl, 3, SCE_PASS_ENVIRONMENT);
		if(srl->passflag  & SCE_PASS_INDIRECT)
			render_layer_add_pass(rr, rl, 3, SCE_PASS_INDIRECT);
		if(srl->passflag  & SCE_PASS_SHADOW)
			render_layer_add_pass(rr, rl, 3, SCE_PASS_SHADOW);
		if(srl->passflag  & SCE_PASS_REFLECT)
			render_layer_add_pass(rr, rl, 3, SCE_PASS_REFLECT);
		if(srl->passflag  & SCE_PASS_REFRACT)
			render_layer_add_pass(rr, rl, 3, SCE_PASS_REFRACT);
		if(srl->passflag  & SCE_PASS_INDEXOB)
			render_layer_add_pass(rr, rl, 1, SCE_PASS_INDEXOB);
		if(srl->passflag  & SCE_PASS_INDEXMA)
			render_layer_add_pass(rr, rl, 1, SCE_PASS_INDEXMA);
		if(srl->passflag  & SCE_PASS_MIST)
			render_layer_add_pass(rr, rl, 1, SCE_PASS_MIST);
		if(rl->passflag & SCE_PASS_RAYHITS)
			render_layer_add_pass(rr, rl, 4, SCE_PASS_RAYHITS);
		
	}
	/* sss, previewrender and envmap don't do layers, so we make a default one */
	if(rr->layers.first==NULL) {
		rl= MEM_callocN(sizeof(RenderLayer), "new render layer");
		BLI_addtail(&rr->layers, rl);
		
		rl->rectx= rectx;
		rl->recty= recty;

		/* duplicate code... */
		if(rr->exrhandle) {
			IMB_exr_add_channel(rr->exrhandle, rl->name, "Combined.R", 0, 0, NULL);
			IMB_exr_add_channel(rr->exrhandle, rl->name, "Combined.G", 0, 0, NULL);
			IMB_exr_add_channel(rr->exrhandle, rl->name, "Combined.B", 0, 0, NULL);
			IMB_exr_add_channel(rr->exrhandle, rl->name, "Combined.A", 0, 0, NULL);
		}
		else
			rl->rectf= MEM_mapallocN(rectx*recty*sizeof(float)*4, "Combined rgba");
		
		/* note, this has to be in sync with scene.c */
		rl->lay= (1<<20) -1;
		rl->layflag= 0x7FFF;	/* solid ztra halo strand */
		rl->passflag= SCE_PASS_COMBINED;
		
		re->r.actlay= 0;
	}
	
	/* border render; calculate offset for use in compositor. compo is centralized coords */
	rr->xof= re->disprect.xmin + (re->disprect.xmax - re->disprect.xmin)/2 - re->winx/2;
	rr->yof= re->disprect.ymin + (re->disprect.ymax - re->disprect.ymin)/2 - re->winy/2;
	
	return rr;
}

static int render_scene_needs_vector(Render *re)
{
	SceneRenderLayer *srl;
	
	for(srl= re->scene->r.layers.first; srl; srl= srl->next)
		if(!(srl->layflag & SCE_LAY_DISABLE))
			if(srl->passflag & SCE_PASS_VECTOR)
				return 1;

	return 0;
}

static void do_merge_tile(RenderResult *rr, RenderResult *rrpart, float *target, float *tile, int pixsize)
{
	int y, ofs, copylen, tilex, tiley;
	
	copylen= tilex= rrpart->rectx;
	tiley= rrpart->recty;
	
	if(rrpart->crop) {	/* filters add pixel extra */
		tile+= pixsize*(rrpart->crop + rrpart->crop*tilex);
		
		copylen= tilex - 2*rrpart->crop;
		tiley -= 2*rrpart->crop;
		
		ofs= (rrpart->tilerect.ymin + rrpart->crop)*rr->rectx + (rrpart->tilerect.xmin+rrpart->crop);
		target+= pixsize*ofs;
	}
	else {
		ofs= (rrpart->tilerect.ymin*rr->rectx + rrpart->tilerect.xmin);
		target+= pixsize*ofs;
	}

	copylen *= sizeof(float)*pixsize;
	tilex *= pixsize;
	ofs= pixsize*rr->rectx;

	for(y=0; y<tiley; y++) {
		memcpy(target, tile, copylen);
		target+= ofs;
		tile+= tilex;
	}
}

/* used when rendering to a full buffer, or when reading the exr part-layer-pass file */
/* no test happens here if it fits... we also assume layers are in sync */
/* is used within threads */
void merge_render_result(RenderResult *rr, RenderResult *rrpart)
{
	RenderLayer *rl, *rlp;
	RenderPass *rpass, *rpassp;
	
	for(rl= rr->layers.first, rlp= rrpart->layers.first; rl && rlp; rl= rl->next, rlp= rlp->next) {
		
		/* combined */
		if(rl->rectf && rlp->rectf)
			do_merge_tile(rr, rrpart, rl->rectf, rlp->rectf, 4);
		
		/* passes are allocated in sync */
		for(rpass= rl->passes.first, rpassp= rlp->passes.first; rpass && rpassp; rpass= rpass->next, rpassp= rpassp->next) {
			do_merge_tile(rr, rrpart, rpass->rect, rpassp->rect, rpass->channels);
		}
	}
}


static void save_render_result_tile(RenderResult *rr, RenderResult *rrpart)
{
	RenderLayer *rlp;
	RenderPass *rpassp;
	int offs, partx, party;
	
	BLI_lock_thread(LOCK_IMAGE);
	
	for(rlp= rrpart->layers.first; rlp; rlp= rlp->next) {
		
		if(rrpart->crop) {	/* filters add pixel extra */
			offs= (rrpart->crop + rrpart->crop*rrpart->rectx);
		}
		else {
			offs= 0;
		}
		
		/* combined */
		if(rlp->rectf) {
			int a, xstride= 4;
			for(a=0; a<xstride; a++)
				IMB_exr_set_channel(rr->exrhandle, rlp->name, get_pass_name(SCE_PASS_COMBINED, a), 
								xstride, xstride*rrpart->rectx, rlp->rectf+a + xstride*offs);
		}
		
		/* passes are allocated in sync */
		for(rpassp= rlp->passes.first; rpassp; rpassp= rpassp->next) {
			int a, xstride= rpassp->channels;
			for(a=0; a<xstride; a++)
				IMB_exr_set_channel(rr->exrhandle, rlp->name, get_pass_name(rpassp->passtype, a), 
									xstride, xstride*rrpart->rectx, rpassp->rect+a + xstride*offs);
		}
		
	}

	party= rrpart->tilerect.ymin + rrpart->crop;
	partx= rrpart->tilerect.xmin + rrpart->crop;
	IMB_exrtile_write_channels(rr->exrhandle, partx, party, 0);

	BLI_unlock_thread(LOCK_IMAGE);

}

static void save_empty_result_tiles(Render *re)
{
	RenderPart *pa;
	RenderResult *rr;
	
	for(rr= re->result; rr; rr= rr->next) {
		IMB_exrtile_clear_channels(rr->exrhandle);
		
		for(pa= re->parts.first; pa; pa= pa->next) {
			if(pa->ready==0) {
				int party= pa->disprect.ymin - re->disprect.ymin + pa->crop;
				int partx= pa->disprect.xmin - re->disprect.xmin + pa->crop;
				IMB_exrtile_write_channels(rr->exrhandle, partx, party, 0);
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
	len= strlen(name);
	name[len]= '.';
	name[len+1]= rpass->chan_id[chan];
	name[len+2]= 0;

	return name;
}

/* filename already made absolute */
/* called from within UI, saves both rendered result as a file-read result */
int RE_WriteRenderResult(ReportList *reports, RenderResult *rr, const char *filename, int compress)
{
	RenderLayer *rl;
	RenderPass *rpass;
	void *exrhandle= IMB_exr_get_handle();
	int success;

	BLI_make_existing_file(filename);
	
	/* composite result */
	if(rr->rectf) {
		IMB_exr_add_channel(exrhandle, "Composite", "Combined.R", 4, 4*rr->rectx, rr->rectf);
		IMB_exr_add_channel(exrhandle, "Composite", "Combined.G", 4, 4*rr->rectx, rr->rectf+1);
		IMB_exr_add_channel(exrhandle, "Composite", "Combined.B", 4, 4*rr->rectx, rr->rectf+2);
		IMB_exr_add_channel(exrhandle, "Composite", "Combined.A", 4, 4*rr->rectx, rr->rectf+3);
	}
	
	/* add layers/passes and assign channels */
	for(rl= rr->layers.first; rl; rl= rl->next) {
		
		/* combined */
		if(rl->rectf) {
			int a, xstride= 4;
			for(a=0; a<xstride; a++)
				IMB_exr_add_channel(exrhandle, rl->name, get_pass_name(SCE_PASS_COMBINED, a), 
									xstride, xstride*rr->rectx, rl->rectf+a);
		}
		
		/* passes are allocated in sync */
		for(rpass= rl->passes.first; rpass; rpass= rpass->next) {
			int a, xstride= rpass->channels;
			for(a=0; a<xstride; a++) {
				if(rpass->passtype)
					IMB_exr_add_channel(exrhandle, rl->name, get_pass_name(rpass->passtype, a), 
										xstride, xstride*rr->rectx, rpass->rect+a);
				else
					IMB_exr_add_channel(exrhandle, rl->name, make_pass_name(rpass, a), 
										xstride, xstride*rr->rectx, rpass->rect+a);
			}
		}
	}

	/* when the filename has no permissions, this can fail */
	if(IMB_exr_begin_write(exrhandle, filename, rr->rectx, rr->recty, compress)) {
		IMB_exr_write_channels(exrhandle);
		success= TRUE;
	}
	else {
		/* TODO, get the error from openexr's exception */
		BKE_report(reports, RPT_ERROR, "Error Writing Render Result, see console");
		success= FALSE;
	}
	IMB_exr_close(exrhandle);

	return success;
}

/* callbacks for RE_MultilayerConvert */
static void *ml_addlayer_cb(void *base, char *str)
{
	RenderResult *rr= base;
	RenderLayer *rl;
	
	rl= MEM_callocN(sizeof(RenderLayer), "new render layer");
	BLI_addtail(&rr->layers, rl);
	
	BLI_strncpy(rl->name, str, EXR_LAY_MAXNAME);
	return rl;
}
static void ml_addpass_cb(void *UNUSED(base), void *lay, char *str, float *rect, int totchan, char *chan_id)
{
	RenderLayer *rl= lay;	
	RenderPass *rpass= MEM_callocN(sizeof(RenderPass), "loaded pass");
	int a;
	
	BLI_addtail(&rl->passes, rpass);
	rpass->channels= totchan;

	rpass->passtype= passtype_from_name(str);
	if(rpass->passtype==0) printf("unknown pass %s\n", str);
	rl->passflag |= rpass->passtype;
	
	BLI_strncpy(rpass->name, str, EXR_PASS_MAXNAME);
	/* channel id chars */
	for(a=0; a<totchan; a++)
		rpass->chan_id[a]= chan_id[a];
	
	rpass->rect= rect;
}

/* from imbuf, if a handle was returned we convert this to render result */
RenderResult *RE_MultilayerConvert(void *exrhandle, int rectx, int recty)
{
	RenderResult *rr= MEM_callocN(sizeof(RenderResult), "loaded render result");
	RenderLayer *rl;
	RenderPass *rpass;
	
	rr->rectx= rectx;
	rr->recty= recty;
	
	IMB_exr_multilayer_convert(exrhandle, rr, ml_addlayer_cb, ml_addpass_cb);

	for(rl=rr->layers.first; rl; rl=rl->next) {
		rl->rectx= rectx;
		rl->recty= recty;

		for(rpass=rl->passes.first; rpass; rpass=rpass->next) {
			rpass->rectx= rectx;
			rpass->recty= recty;
		}
	}
	
	return rr;
}

/* called in end of render, to add names to passes... for UI only */
static void renderresult_add_names(RenderResult *rr)
{
	RenderLayer *rl;
	RenderPass *rpass;
	
	for(rl= rr->layers.first; rl; rl= rl->next)
		for(rpass= rl->passes.first; rpass; rpass= rpass->next)
			BLI_strncpy(rpass->name, get_pass_name(rpass->passtype, -1), sizeof(rpass->name));
}

/* called for reading temp files, and for external engines */
static int read_render_result_from_file(const char *filename, RenderResult *rr)
{
	RenderLayer *rl;
	RenderPass *rpass;
	void *exrhandle= IMB_exr_get_handle();
	int rectx, recty;

	if(IMB_exr_begin_read(exrhandle, filename, &rectx, &recty)==0) {
		printf("failed being read %s\n", filename);
		IMB_exr_close(exrhandle);
		return 0;
	}

	if(rr == NULL || rectx!=rr->rectx || recty!=rr->recty) {
		if(rr)
			printf("error in reading render result: dimensions don't match\n");
		else
			printf("error in reading render result: NULL result pointer\n");
		IMB_exr_close(exrhandle);
		return 0;
	}
	else {
		for(rl= rr->layers.first; rl; rl= rl->next) {
			
			/* combined */
			if(rl->rectf) {
				int a, xstride= 4;
				for(a=0; a<xstride; a++)
					IMB_exr_set_channel(exrhandle, rl->name, get_pass_name(SCE_PASS_COMBINED, a), 
										xstride, xstride*rectx, rl->rectf+a);
			}
			
			/* passes are allocated in sync */
			for(rpass= rl->passes.first; rpass; rpass= rpass->next) {
				int a, xstride= rpass->channels;
				for(a=0; a<xstride; a++)
					IMB_exr_set_channel(exrhandle, rl->name, get_pass_name(rpass->passtype, a), 
										xstride, xstride*rectx, rpass->rect+a);
			}
			
		}
		IMB_exr_read_channels(exrhandle);
		renderresult_add_names(rr);
	}

	IMB_exr_close(exrhandle);

	return 1;
}

/* only for temp buffer files, makes exact copy of render result */
static int read_render_result(Render *re, int sample)
{
	char str[FILE_MAX];
	int success;

	BLI_rw_mutex_lock(&re->resultmutex, THREAD_LOCK_WRITE);

	RE_FreeRenderResult(re->result);
	re->result= new_render_result(re, &re->disprect, 0, RR_USEMEM);

	render_unique_exr_name(re, str, sample);
	printf("read exr tmp file: %s\n", str);

	if(read_render_result_from_file(str, re->result)) {
		success= TRUE;
	}
	else {
		printf("cannot read: %s\n", str);
		success= FALSE;

	}

	BLI_rw_mutex_unlock(&re->resultmutex);

	return success;
}

/* *************************************************** */

Render *RE_GetRender(const char *name)
{
	Render *re;

	/* search for existing renders */
	for(re= RenderGlobal.renderlist.first; re; re= re->next)
		if(strncmp(re->name, name, RE_MAXNAME)==0)
			break;

	return re;
}

/* if you want to know exactly what has been done */
RenderResult *RE_AcquireResultRead(Render *re)
{
	if(re) {
		BLI_rw_mutex_lock(&re->resultmutex, THREAD_LOCK_READ);
		return re->result;
	}

	return NULL;
}

RenderResult *RE_AcquireResultWrite(Render *re)
{
	if(re) {
		BLI_rw_mutex_lock(&re->resultmutex, THREAD_LOCK_WRITE);
		return re->result;
	}

	return NULL;
}

void RE_SwapResult(Render *re, RenderResult **rr)
{
	/* for keeping render buffers */
	if(re) {
		SWAP(RenderResult*, re->result, *rr);
	}
}


void RE_ReleaseResult(Render *re)
{
	if(re)
		BLI_rw_mutex_unlock(&re->resultmutex);
}

/* displist.c util.... */
Scene *RE_GetScene(Render *re)
{
	if(re)
		return re->scene;
	return NULL;
}

RenderLayer *render_get_active_layer(Render *re, RenderResult *rr)
{
	RenderLayer *rl= BLI_findlink(&rr->layers, re->r.actlay);
	
	if(rl) 
		return rl;
	else 
		return rr->layers.first;
}


/* fill provided result struct with what's currently active or done */
void RE_AcquireResultImage(Render *re, RenderResult *rr)
{
	memset(rr, 0, sizeof(RenderResult));

	if(re) {
		BLI_rw_mutex_lock(&re->resultmutex, THREAD_LOCK_READ);

		if(re->result) {
			RenderLayer *rl;
			
			rr->rectx= re->result->rectx;
			rr->recty= re->result->recty;
			
			rr->rectf= re->result->rectf;
			rr->rectz= re->result->rectz;
			rr->rect32= re->result->rect32;
			
			/* active layer */
			rl= render_get_active_layer(re, re->result);

			if(rl) {
				if(rr->rectf==NULL)
					rr->rectf= rl->rectf;
				if(rr->rectz==NULL)
					rr->rectz= RE_RenderLayerGetPass(rl, SCE_PASS_Z);	
			}

			rr->have_combined= (re->result->rectf != NULL);
			rr->layers= re->result->layers;
		}
	}
}

void RE_ReleaseResultImage(Render *re)
{
	if(re)
		BLI_rw_mutex_unlock(&re->resultmutex);
}

/* caller is responsible for allocating rect in correct size! */
void RE_ResultGet32(Render *re, unsigned int *rect)
{
	RenderResult rres;
	
	RE_AcquireResultImage(re, &rres);

	if(rres.rect32) {
		memcpy(rect, rres.rect32, sizeof(int)*rres.rectx*rres.recty);
	}
	else if(rres.rectf) {
		int profile_from= (re->r.color_mgt_flag & R_COLOR_MANAGEMENT)? IB_PROFILE_LINEAR_RGB: IB_PROFILE_SRGB;
		int predivide= (re->r.color_mgt_flag & R_COLOR_MANAGEMENT_PREDIVIDE);
		int dither= 0;

		IMB_buffer_byte_from_float((unsigned char*)rect, rres.rectf,
			4, dither, IB_PROFILE_SRGB, profile_from, predivide,
			rres.rectx, rres.recty, rres.rectx, rres.rectx);
	}
	else
		/* else fill with black */
		memset(rect, 0, sizeof(int)*re->rectx*re->recty);

	RE_ReleaseResultImage(re);
}

RenderStats *RE_GetStats(Render *re)
{
	return &re->i;
}

Render *RE_NewRender(const char *name)
{
	Render *re;

	/* only one render per name exists */
	re= RE_GetRender(name);
	if(re==NULL) {
		
		/* new render data struct */
		re= MEM_callocN(sizeof(Render), "new render");
		BLI_addtail(&RenderGlobal.renderlist, re);
		BLI_strncpy(re->name, name, RE_MAXNAME);
		BLI_rw_mutex_init(&re->resultmutex);
	}
	
	RE_InitRenderCB(re);

	/* init some variables */
	re->ycor= 1.0f;
	
	return re;
}

/* called for new renders and when finishing rendering so
 * we calways have valid callbacks on a render */
void RE_InitRenderCB(Render *re)
{
	/* set default empty callbacks */
	re->display_init= result_nothing;
	re->display_clear= result_nothing;
	re->display_draw= result_rcti_nothing;
	re->progress= float_nothing;
	re->test_break= default_break;
	if(G.background)
		re->stats_draw= stats_background;
	else
		re->stats_draw= stats_nothing;
	/* clear callback handles */
	re->dih= re->dch= re->ddh= re->sdh= re->prh= re->tbh= NULL;
}

/* only call this while you know it will remove the link too */
void RE_FreeRender(Render *re)
{
	BLI_rw_mutex_end(&re->resultmutex);
	
	free_renderdata_tables(re);
	free_sample_tables(re);
	
	RE_FreeRenderResult(re->result);
	RE_FreeRenderResult(re->pushedresult);
	
	BLI_remlink(&RenderGlobal.renderlist, re);
	MEM_freeN(re);
}

/* exit blender */
void RE_FreeAllRender(void)
{
	while(RenderGlobal.renderlist.first) {
		RE_FreeRender(RenderGlobal.renderlist.first);
	}
}

/* ********* initialize state ******** */


/* what doesn't change during entire render sequence */
/* disprect is optional, if NULL it assumes full window render */
void RE_InitState(Render *re, Render *source, RenderData *rd, SceneRenderLayer *srl, int winx, int winy, rcti *disprect)
{
	re->ok= TRUE;	/* maybe flag */
	
	re->i.starttime= PIL_check_seconds_timer();
	re->r= *rd;		/* hardcopy */
	
	re->winx= winx;
	re->winy= winy;
	if(disprect) {
		re->disprect= *disprect;
		re->rectx= disprect->xmax-disprect->xmin;
		re->recty= disprect->ymax-disprect->ymin;
	}
	else {
		re->disprect.xmin= re->disprect.ymin= 0;
		re->disprect.xmax= winx;
		re->disprect.ymax= winy;
		re->rectx= winx;
		re->recty= winy;
	}
	
	if(re->rectx < 2 || re->recty < 2 || (BKE_imtype_is_movie(rd->im_format.imtype) &&
										  (re->rectx < 16 || re->recty < 16) )) {
		BKE_report(re->reports, RPT_ERROR, "Image too small");
		re->ok= 0;
		return;
	}

	if((re->r.mode & (R_OSA))==0)
		re->r.scemode &= ~R_FULL_SAMPLE;

#ifdef WITH_OPENEXR
	if(re->r.scemode & R_FULL_SAMPLE)
		re->r.scemode |= R_EXR_TILE_FILE;	/* enable automatic */

	/* Until use_border is made compatible with save_buffers/full_sample, render without the later instead of not rendering at all.*/
	if(re->r.mode & R_BORDER) 
	{
		re->r.scemode &= ~(R_EXR_TILE_FILE|R_FULL_SAMPLE);
	}

#else
	/* can't do this without openexr support */
	re->r.scemode &= ~(R_EXR_TILE_FILE|R_FULL_SAMPLE);
#endif
	
	/* fullsample wants uniform osa levels */
	if(source && (re->r.scemode & R_FULL_SAMPLE)) {
		/* but, if source has no full sample we disable it */
		if((source->r.scemode & R_FULL_SAMPLE)==0)
			re->r.scemode &= ~R_FULL_SAMPLE;
		else
			re->r.osa= re->osa= source->osa;
	}
	else {
		/* check state variables, osa? */
		if(re->r.mode & (R_OSA)) {
			re->osa= re->r.osa;
			if(re->osa>16) re->osa= 16;
		}
		else re->osa= 0;
	}
	
	if (srl) {
		int index = BLI_findindex(&re->r.layers, srl);
		if (index != -1) {
			re->r.actlay = index;
			re->r.scemode |= R_SINGLE_LAYER;
		}
	}
		
	/* always call, checks for gamma, gamma tables and jitter too */
	make_sample_tables(re);	
	
	/* if preview render, we try to keep old result */
	BLI_rw_mutex_lock(&re->resultmutex, THREAD_LOCK_WRITE);

	if(re->r.scemode & R_PREVIEWBUTS) {
		if(re->result && re->result->rectx==re->rectx && re->result->recty==re->recty);
		else {
			RE_FreeRenderResult(re->result);
			re->result= NULL;
		}
	}
	else {
		
		/* make empty render result, so display callbacks can initialize */
		RE_FreeRenderResult(re->result);
		re->result= MEM_callocN(sizeof(RenderResult), "new render result");
		re->result->rectx= re->rectx;
		re->result->recty= re->recty;
	}

	BLI_rw_mutex_unlock(&re->resultmutex);
	
	/* we clip faces with a minimum of 2 pixel boundary outside of image border. see zbuf.c */
	re->clipcrop= 1.0f + 2.0f/(float)(re->winx>re->winy?re->winy:re->winx);
	
	re->mblur_offs = re->field_offs = 0.f;
	
	RE_init_threadcount(re);
}

/* part of external api, not called for regular render pipeline */
void RE_SetDispRect (struct Render *re, rcti *disprect)
{
	re->disprect= *disprect;
	re->rectx= disprect->xmax-disprect->xmin;
	re->recty= disprect->ymax-disprect->ymin;
	
	/* initialize render result */
	BLI_rw_mutex_lock(&re->resultmutex, THREAD_LOCK_WRITE);

	RE_FreeRenderResult(re->result);
	re->result= new_render_result(re, &re->disprect, 0, RR_USEMEM);

	BLI_rw_mutex_unlock(&re->resultmutex);
}

void RE_SetWindow(Render *re, rctf *viewplane, float clipsta, float clipend)
{
	/* re->ok flag? */
	
	re->viewplane= *viewplane;
	re->clipsta= clipsta;
	re->clipend= clipend;
	re->r.mode &= ~R_ORTHO;

	perspective_m4( re->winmat,re->viewplane.xmin, re->viewplane.xmax, re->viewplane.ymin, re->viewplane.ymax, re->clipsta, re->clipend);
	
}

void RE_SetOrtho(Render *re, rctf *viewplane, float clipsta, float clipend)
{
	/* re->ok flag? */
	
	re->viewplane= *viewplane;
	re->clipsta= clipsta;
	re->clipend= clipend;
	re->r.mode |= R_ORTHO;

	orthographic_m4( re->winmat,re->viewplane.xmin, re->viewplane.xmax, re->viewplane.ymin, re->viewplane.ymax, re->clipsta, re->clipend);
}

void RE_SetView(Render *re, float mat[][4])
{
	/* re->ok flag? */
	copy_m4_m4(re->viewmat, mat);
	invert_m4_m4(re->viewinv, re->viewmat);
}

/* image and movie output has to move to either imbuf or kernel */
void RE_display_init_cb(Render *re, void *handle, void (*f)(void *handle, RenderResult *rr))
{
	re->display_init= f;
	re->dih= handle;
}
void RE_display_clear_cb(Render *re, void *handle, void (*f)(void *handle, RenderResult *rr))
{
	re->display_clear= f;
	re->dch= handle;
}
void RE_display_draw_cb(Render *re, void *handle, void (*f)(void *handle, RenderResult *rr, volatile rcti *rect))
{
	re->display_draw= f;
	re->ddh= handle;
}
void RE_stats_draw_cb(Render *re, void *handle, void (*f)(void *handle, RenderStats *rs))
{
	re->stats_draw= f;
	re->sdh= handle;
}
void RE_progress_cb(Render *re, void *handle, void (*f)(void *handle, float))
{
	re->progress= f;
	re->prh= handle;
}

void RE_draw_lock_cb(Render *re, void *handle, void (*f)(void *handle, int i))
{
	re->draw_lock= f;
	re->tbh= handle;
}

void RE_test_break_cb(Render *re, void *handle, int (*f)(void *handle))
{
	re->test_break= f;
	re->tbh= handle;
}


/* ********* add object data (later) ******** */

/* object is considered fully prepared on correct time etc */
/* includes lights */
#if 0
void RE_AddObject(Render *UNUSED(re), Object *UNUSED(ob))
{
	
}
#endif

/* *************************************** */

static int render_display_draw_enabled(Render *re)
{
	/* don't show preprocess for previewrender sss */
	if(re->sss_points)
		return !(re->r.scemode & R_PREVIEWBUTS);
	else
		return 1;
}

/* allocate osa new results for samples */
static RenderResult *new_full_sample_buffers(Render *re, ListBase *lb, rcti *partrct, int crop)
{
	int a;
	
	if(re->osa==0)
		return new_render_result(re, partrct, crop, RR_USEMEM);
	
	for(a=0; a<re->osa; a++) {
		RenderResult *rr= new_render_result(re, partrct, crop, RR_USEMEM);
		BLI_addtail(lb, rr);
		rr->sample_nr= a;
	}
	
	return lb->first;
}


/* the main thread call, renders an entire part */
static void *do_part_thread(void *pa_v)
{
	RenderPart *pa= pa_v;
	
	/* need to return nicely all parts on esc */
	if(R.test_break(R.tbh)==0) {
		
		if(!R.sss_points && (R.r.scemode & R_FULL_SAMPLE))
			pa->result= new_full_sample_buffers(&R, &pa->fullresult, &pa->disprect, pa->crop);
		else
			pa->result= new_render_result(&R, &pa->disprect, pa->crop, RR_USEMEM);

		if(R.sss_points)
			zbufshade_sss_tile(pa);
		else if(R.osa)
			zbufshadeDA_tile(pa);
		else
			zbufshade_tile(pa);
		
		/* merge too on break! */
		if(R.result->exrhandle) {
			RenderResult *rr, *rrpart;
			
			for(rr= R.result, rrpart= pa->result; rr && rrpart; rr= rr->next, rrpart= rrpart->next)
				save_render_result_tile(rr, rrpart);
			
		}
		else if(render_display_draw_enabled(&R)) {
			/* on break, don't merge in result for preview renders, looks nicer */
			if(R.test_break(R.tbh) && (R.r.scemode & R_PREVIEWBUTS));
			else merge_render_result(R.result, pa->result);
		}
	}
	
	pa->ready= 1;
	
	return NULL;
}

/* calculus for how much 1 pixel rendered should rotate the 3d geometry */
/* is not that simple, needs to be corrected for errors of larger viewplane sizes */
/* called in initrender.c, initparts() and convertblender.c, for speedvectors */
float panorama_pixel_rot(Render *re)
{
	float psize, phi, xfac;
	float borderfac= (float)(re->disprect.xmax - re->disprect.xmin) / (float)re->winx;
	
	/* size of 1 pixel mapped to viewplane coords */
	psize= (re->viewplane.xmax-re->viewplane.xmin)/(float)(re->winx);
	/* angle of a pixel */
	phi= atan(psize/re->clipsta);
	
	/* correction factor for viewplane shifting, first calculate how much the viewplane angle is */
	xfac= borderfac*((re->viewplane.xmax-re->viewplane.xmin))/(float)re->xparts;
	xfac= atan(0.5f*xfac/re->clipsta); 
	/* and how much the same viewplane angle is wrapped */
	psize= 0.5f*phi*((float)re->partx);
	
	/* the ratio applied to final per-pixel angle */
	phi*= xfac/psize;
	
	return phi;
}

/* call when all parts stopped rendering, to find the next Y slice */
/* if slice found, it rotates the dbase */
static RenderPart *find_next_pano_slice(Render *re, int *minx, rctf *viewplane)
{
	RenderPart *pa, *best= NULL;
	
	*minx= re->winx;
	
	/* most left part of the non-rendering parts */
	for(pa= re->parts.first; pa; pa= pa->next) {
		if(pa->ready==0 && pa->nr==0) {
			if(pa->disprect.xmin < *minx) {
				best= pa;
				*minx= pa->disprect.xmin;
			}
		}
	}
			
	if(best) {
		float phi= panorama_pixel_rot(re);

		R.panodxp= (re->winx - (best->disprect.xmin + best->disprect.xmax) )/2;
		R.panodxv= ((viewplane->xmax-viewplane->xmin)*R.panodxp)/(float)(re->winx);

		/* shift viewplane */
		R.viewplane.xmin = viewplane->xmin + R.panodxv;
		R.viewplane.xmax = viewplane->xmax + R.panodxv;
		RE_SetWindow(re, &R.viewplane, R.clipsta, R.clipend);
		copy_m4_m4(R.winmat, re->winmat);
		
		/* rotate database according to part coordinates */
		project_renderdata(re, projectverto, 1, -R.panodxp*phi, 1);
		R.panosi= sin(R.panodxp*phi);
		R.panoco= cos(R.panodxp*phi);
	}
	return best;
}

static RenderPart *find_next_part(Render *re, int minx)
{
	RenderPart *pa, *best= NULL;

	/* long long int's needed because of overflow [#24414] */
	long long int centx=re->winx/2, centy=re->winy/2, tot=1;
	long long int mindist= (long long int)re->winx * (long long int)re->winy;
	
	/* find center of rendered parts, image center counts for 1 too */
	for(pa= re->parts.first; pa; pa= pa->next) {
		if(pa->ready) {
			centx+= (pa->disprect.xmin+pa->disprect.xmax)/2;
			centy+= (pa->disprect.ymin+pa->disprect.ymax)/2;
			tot++;
		}
	}
	centx/=tot;
	centy/=tot;
	
	/* closest of the non-rendering parts */
	for(pa= re->parts.first; pa; pa= pa->next) {
		if(pa->ready==0 && pa->nr==0) {
			long long int distx= centx - (pa->disprect.xmin+pa->disprect.xmax)/2;
			long long int disty= centy - (pa->disprect.ymin+pa->disprect.ymax)/2;
			distx= (long long int)sqrt(distx*distx + disty*disty);
			if(distx<mindist) {
				if(re->r.mode & R_PANORAMA) {
					if(pa->disprect.xmin==minx) {
						best= pa;
						mindist= distx;
					}
				}
				else {
					best= pa;
					mindist= distx;
				}
			}
		}
	}
	return best;
}

static void print_part_stats(Render *re, RenderPart *pa)
{
	char str[64];
	
	BLI_snprintf(str, sizeof(str), "%s, Part %d-%d", re->scene->id.name+2, pa->nr, re->i.totpart);
	re->i.infostr= str;
	re->stats_draw(re->sdh, &re->i);
	re->i.infostr= NULL;
}

/* make osa new results for samples */
static RenderResult *new_full_sample_buffers_exr(Render *re)
{
	int a;
	
	for(a=0; a<re->osa; a++) {
		RenderResult *rr= new_render_result(re, &re->disprect, 0, 1);
		BLI_addtail(&re->fullresult, rr);
		rr->sample_nr= a;
	}
	
	return re->fullresult.first;
}

static void threaded_tile_processor(Render *re)
{
	ListBase threads;
	RenderPart *pa, *nextpa;
	rctf viewplane= re->viewplane;
	int rendering=1, counter= 1, drawtimer=0, hasdrawn, minx=0;
	
	BLI_rw_mutex_lock(&re->resultmutex, THREAD_LOCK_WRITE);

	/* first step; free the entire render result, make new, and/or prepare exr buffer saving */
	if(re->result==NULL || !(re->r.scemode & R_PREVIEWBUTS)) {
		RE_FreeRenderResult(re->result);
	
		if(re->sss_points && render_display_draw_enabled(re))
			re->result= new_render_result(re, &re->disprect, 0, 0);
		else if(re->r.scemode & R_FULL_SAMPLE)
			re->result= new_full_sample_buffers_exr(re);
		else
			re->result= new_render_result(re, &re->disprect, 0, re->r.scemode & (R_EXR_TILE_FILE|R_FULL_SAMPLE));
	}

	BLI_rw_mutex_unlock(&re->resultmutex);
	
	if(re->result==NULL)
		return;
	
	/* warning; no return here without closing exr file */
	
	initparts(re);

	if(re->result->exrhandle) {
		RenderResult *rr;
		char str[FILE_MAX];
		
		for(rr= re->result; rr; rr= rr->next) {
			render_unique_exr_name(re, str, rr->sample_nr);
		
			printf("write exr tmp file, %dx%d, %s\n", rr->rectx, rr->recty, str);
			IMB_exrtile_begin_write(rr->exrhandle, str, 0, rr->rectx, rr->recty, re->partx, re->party);
		}
	}
	
	BLI_init_threads(&threads, do_part_thread, re->r.threads);
	
	/* assuming no new data gets added to dbase... */
	R= *re;
	
	/* set threadsafe break */
	R.test_break= thread_break;
	
	/* timer loop demands to sleep when no parts are left, so we enter loop with a part */
	if(re->r.mode & R_PANORAMA)
		nextpa= find_next_pano_slice(re, &minx, &viewplane);
	else
		nextpa= find_next_part(re, 0);
	
	while(rendering) {
		
		if(re->test_break(re->tbh))
			PIL_sleep_ms(50);
		else if(nextpa && BLI_available_threads(&threads)) {
			drawtimer= 0;
			nextpa->nr= counter++;	/* for nicest part, and for stats */
			nextpa->thread= BLI_available_thread_index(&threads);	/* sample index */
			BLI_insert_thread(&threads, nextpa);

			nextpa= find_next_part(re, minx);
		}
		else if(re->r.mode & R_PANORAMA) {
			if(nextpa==NULL && BLI_available_threads(&threads)==re->r.threads)
				nextpa= find_next_pano_slice(re, &minx, &viewplane);
			else {
				PIL_sleep_ms(50);
				drawtimer++;
			}
		}
		else {
			PIL_sleep_ms(50);
			drawtimer++;
		}
		
		/* check for ready ones to display, and if we need to continue */
		rendering= 0;
		hasdrawn= 0;
		for(pa= re->parts.first; pa; pa= pa->next) {
			if(pa->ready) {
				
				BLI_remove_thread(&threads, pa);
				
				if(pa->result) {
					if(render_display_draw_enabled(re))
						re->display_draw(re->ddh, pa->result, NULL);
					print_part_stats(re, pa);
					
					free_render_result(&pa->fullresult, pa->result);
					pa->result= NULL;
					re->i.partsdone++;
					re->progress(re->prh, re->i.partsdone / (float)re->i.totpart);
					hasdrawn= 1;
				}
			}
			else {
				rendering= 1;
				if(pa->nr && pa->result && drawtimer>20) {
					if(render_display_draw_enabled(re))
						re->display_draw(re->ddh, pa->result, &pa->result->renrect);
					hasdrawn= 1;
				}
			}
		}
		if(hasdrawn)
			drawtimer= 0;

		/* on break, wait for all slots to get freed */
		if( (g_break=re->test_break(re->tbh)) && BLI_available_threads(&threads)==re->r.threads)
			rendering= 0;
		
	}
	
	if(re->result->exrhandle) {
		RenderResult *rr;

		BLI_rw_mutex_lock(&re->resultmutex, THREAD_LOCK_WRITE);
		save_empty_result_tiles(re);
		
		for(rr= re->result; rr; rr= rr->next) {
			IMB_exr_close(rr->exrhandle);
			rr->exrhandle= NULL;
		}
		
		free_render_result(&re->fullresult, re->result);
		re->result= NULL;

		BLI_rw_mutex_unlock(&re->resultmutex);
		
		read_render_result(re, 0);
	}
	
	/* unset threadsafety */
	g_break= 0;
	
	BLI_end_threads(&threads);
	freeparts(re);
	re->viewplane= viewplane; /* restore viewplane, modified by pano render */
}

/* currently only called by preview renders and envmap */
void RE_TileProcessor(Render *re)
{
	threaded_tile_processor(re);
}

/* ************  This part uses API, for rendering Blender scenes ********** */

static void do_render_3d(Render *re)
{
	/* try external */
	if(RE_engine_render(re, 0))
		return;

	/* internal */
	
//	re->cfra= cfra;	/* <- unused! */
	re->scene->r.subframe = re->mblur_offs + re->field_offs;
	
	/* lock drawing in UI during data phase */
	if(re->draw_lock)
		re->draw_lock(re->dlh, 1);
	
	/* make render verts/faces/halos/lamps */
	if(render_scene_needs_vector(re))
		RE_Database_FromScene_Vectors(re, re->main, re->scene, re->lay);
	else
		RE_Database_FromScene(re, re->main, re->scene, re->lay, 1);
	
	/* clear UI drawing locks */
	if(re->draw_lock)
		re->draw_lock(re->dlh, 0);
	
	threaded_tile_processor(re);
	
	/* do left-over 3d post effects (flares) */
	if(re->flag & R_HALO)
		if(!re->test_break(re->tbh))
			add_halo_flare(re);
	
	/* free all render verts etc */
	RE_Database_Free(re);
	
	re->scene->r.subframe = 0.f;
}

/* called by blur loop, accumulate RGBA key alpha */
static void addblur_rect_key(RenderResult *rr, float *rectf, float *rectf1, float blurfac)
{
	float mfac= 1.0f - blurfac;
	int a, b, stride= 4*rr->rectx;
	int len= stride*sizeof(float);
	
	for(a=0; a<rr->recty; a++) {
		if(blurfac==1.0f) {
			memcpy(rectf, rectf1, len);
		}
		else {
			float *rf= rectf, *rf1= rectf1;
			
			for( b= rr->rectx; b>0; b--, rf+=4, rf1+=4) {
				if(rf1[3]<0.01f)
					rf[3]= mfac*rf[3];
				else if(rf[3]<0.01f) {
					rf[0]= rf1[0];
					rf[1]= rf1[1];
					rf[2]= rf1[2];
					rf[3]= blurfac*rf1[3];
				}
				else {
					rf[0]= mfac*rf[0] + blurfac*rf1[0];
					rf[1]= mfac*rf[1] + blurfac*rf1[1];
					rf[2]= mfac*rf[2] + blurfac*rf1[2];
					rf[3]= mfac*rf[3] + blurfac*rf1[3];
				}				
			}
		}
		rectf+= stride;
		rectf1+= stride;
	}
}

/* called by blur loop, accumulate renderlayers */
static void addblur_rect(RenderResult *rr, float *rectf, float *rectf1, float blurfac, int channels)
{
	float mfac= 1.0f - blurfac;
	int a, b, stride= channels*rr->rectx;
	int len= stride*sizeof(float);
	
	for(a=0; a<rr->recty; a++) {
		if(blurfac==1.0f) {
			memcpy(rectf, rectf1, len);
		}
		else {
			float *rf= rectf, *rf1= rectf1;
			
			for( b= rr->rectx*channels; b>0; b--, rf++, rf1++) {
				rf[0]= mfac*rf[0] + blurfac*rf1[0];
			}
		}
		rectf+= stride;
		rectf1+= stride;
	}
}


/* called by blur loop, accumulate renderlayers */
static void merge_renderresult_blur(RenderResult *rr, RenderResult *brr, float blurfac, int key_alpha)
{
	RenderLayer *rl, *rl1;
	RenderPass *rpass, *rpass1;
	
	rl1= brr->layers.first;
	for(rl= rr->layers.first; rl && rl1; rl= rl->next, rl1= rl1->next) {
		
		/* combined */
		if(rl->rectf && rl1->rectf) {
			if(key_alpha)
				addblur_rect_key(rr, rl->rectf, rl1->rectf, blurfac);
			else
				addblur_rect(rr, rl->rectf, rl1->rectf, blurfac, 4);
		}
		
		/* passes are allocated in sync */
		rpass1= rl1->passes.first;
		for(rpass= rl->passes.first; rpass && rpass1; rpass= rpass->next, rpass1= rpass1->next) {
			addblur_rect(rr, rpass->rect, rpass1->rect, blurfac, rpass->channels);
		}
	}
}

/* main blur loop, can be called by fields too */
static void do_render_blur_3d(Render *re)
{
	RenderResult *rres;
	float blurfac;
	int blur= re->r.mblur_samples;
	
	/* create accumulation render result */
	rres= new_render_result(re, &re->disprect, 0, RR_USEMEM);
	
	/* do the blur steps */
	while(blur--) {
		re->mblur_offs = re->r.blurfac*((float)(re->r.mblur_samples-blur))/(float)re->r.mblur_samples;
		
		re->i.curblur= re->r.mblur_samples-blur;	/* stats */
		
		do_render_3d(re);
		
		blurfac= 1.0f/(float)(re->r.mblur_samples-blur);
		
		merge_renderresult_blur(rres, re->result, blurfac, re->r.alphamode & R_ALPHAKEY);
		if(re->test_break(re->tbh)) break;
	}
	
	/* swap results */
	BLI_rw_mutex_lock(&re->resultmutex, THREAD_LOCK_WRITE);
	RE_FreeRenderResult(re->result);
	re->result= rres;
	BLI_rw_mutex_unlock(&re->resultmutex);
	
	re->mblur_offs = 0.0f;
	re->i.curblur= 0;	/* stats */
	
	/* weak... the display callback wants an active renderlayer pointer... */
	re->result->renlay= render_get_active_layer(re, re->result);
	re->display_draw(re->ddh, re->result, NULL);	
}


/* function assumes rectf1 and rectf2 to be half size of rectf */
static void interleave_rect(RenderResult *rr, float *rectf, float *rectf1, float *rectf2, int channels)
{
	int a, stride= channels*rr->rectx;
	int len= stride*sizeof(float);
	
	for(a=0; a<rr->recty; a+=2) {
		memcpy(rectf, rectf1, len);
		rectf+= stride;
		rectf1+= stride;
		memcpy(rectf, rectf2, len);
		rectf+= stride;
		rectf2+= stride;
	}
}

/* merge render results of 2 fields */
static void merge_renderresult_fields(RenderResult *rr, RenderResult *rr1, RenderResult *rr2)
{
	RenderLayer *rl, *rl1, *rl2;
	RenderPass *rpass, *rpass1, *rpass2;
	
	rl1= rr1->layers.first;
	rl2= rr2->layers.first;
	for(rl= rr->layers.first; rl && rl1 && rl2; rl= rl->next, rl1= rl1->next, rl2= rl2->next) {
		
		/* combined */
		if(rl->rectf && rl1->rectf && rl2->rectf)
			interleave_rect(rr, rl->rectf, rl1->rectf, rl2->rectf, 4);
		
		/* passes are allocated in sync */
		rpass1= rl1->passes.first;
		rpass2= rl2->passes.first;
		for(rpass= rl->passes.first; rpass && rpass1 && rpass2; rpass= rpass->next, rpass1= rpass1->next, rpass2= rpass2->next) {
			interleave_rect(rr, rpass->rect, rpass1->rect, rpass2->rect, rpass->channels);
		}
	}
}


/* interleaves 2 frames */
static void do_render_fields_3d(Render *re)
{
	Object *camera= RE_GetCamera(re);
	RenderResult *rr1, *rr2= NULL;
	
	/* no render result was created, we can safely halve render y */
	re->winy /= 2;
	re->recty /= 2;
	re->disprect.ymin /= 2;
	re->disprect.ymax /= 2;
	
	re->i.curfield= 1;	/* stats */
	
	/* first field, we have to call camera routine for correct aspect and subpixel offset */
	RE_SetCamera(re, camera);
	if(re->r.mode & R_MBLUR && (re->r.scemode & R_FULL_SAMPLE)==0)
		do_render_blur_3d(re);
	else
		do_render_3d(re);

	BLI_rw_mutex_lock(&re->resultmutex, THREAD_LOCK_WRITE);
	rr1= re->result;
	re->result= NULL;
	BLI_rw_mutex_unlock(&re->resultmutex);
	
	/* second field */
	if(!re->test_break(re->tbh)) {
		
		re->i.curfield= 2;	/* stats */
		
		re->flag |= R_SEC_FIELD;
		if((re->r.mode & R_FIELDSTILL)==0) {
			re->field_offs = 0.5f;
		}
		RE_SetCamera(re, camera);
		if(re->r.mode & R_MBLUR && (re->r.scemode & R_FULL_SAMPLE)==0)
			do_render_blur_3d(re);
		else
			do_render_3d(re);
		re->flag &= ~R_SEC_FIELD;
		
		re->field_offs = 0.0f;
		
		rr2= re->result;
	}
	
	/* allocate original height new buffers */
	re->winy *= 2;
	re->recty *= 2;
	re->disprect.ymin *= 2;
	re->disprect.ymax *= 2;

	BLI_rw_mutex_lock(&re->resultmutex, THREAD_LOCK_WRITE);
	re->result= new_render_result(re, &re->disprect, 0, RR_USEMEM);

	if(rr2) {
		if(re->r.mode & R_ODDFIELD)
			merge_renderresult_fields(re->result, rr2, rr1);
		else
			merge_renderresult_fields(re->result, rr1, rr2);
		
		RE_FreeRenderResult(rr2);
	}

	RE_FreeRenderResult(rr1);
	
	re->i.curfield= 0;	/* stats */
	
	/* weak... the display callback wants an active renderlayer pointer... */
	re->result->renlay= render_get_active_layer(re, re->result);

	BLI_rw_mutex_unlock(&re->resultmutex);

	re->display_draw(re->ddh, re->result, NULL);
}

/* main render routine, no compositing */
static void do_render_fields_blur_3d(Render *re)
{
	Object *camera= RE_GetCamera(re);
	/* also check for camera here */
	if(camera == NULL) {
		printf("ERROR: Cannot render, no camera\n");
		G.afbreek= 1;
		return;
	}

	/* now use renderdata and camera to set viewplane */
	RE_SetCamera(re, camera);
	
	if(re->r.mode & R_FIELDS)
		do_render_fields_3d(re);
	else if(re->r.mode & R_MBLUR && (re->r.scemode & R_FULL_SAMPLE)==0)
		do_render_blur_3d(re);
	else
		do_render_3d(re);
	
	/* when border render, check if we have to insert it in black */
	if(re->result) {
		if(re->r.mode & R_BORDER) {
			if((re->r.mode & R_CROP)==0) {
				RenderResult *rres;
				
				BLI_rw_mutex_lock(&re->resultmutex, THREAD_LOCK_WRITE);

				/* sub-rect for merge call later on */
				re->result->tilerect= re->disprect;
				
				/* this copying sequence could become function? */
				/* weak is: it chances disprect from border */
				re->disprect.xmin= re->disprect.ymin= 0;
				re->disprect.xmax= re->winx;
				re->disprect.ymax= re->winy;
				re->rectx= re->winx;
				re->recty= re->winy;
				
				rres= new_render_result(re, &re->disprect, 0, RR_USEMEM);
				
				merge_render_result(rres, re->result);
				RE_FreeRenderResult(re->result);
				re->result= rres;
				
				/* weak... the display callback wants an active renderlayer pointer... */
				re->result->renlay= render_get_active_layer(re, re->result);
				
				BLI_rw_mutex_unlock(&re->resultmutex);
		
				re->display_init(re->dih, re->result);
				re->display_draw(re->ddh, re->result, NULL);
			}
			else {
				/* set offset (again) for use in compositor, disprect was manipulated. */
				re->result->xof= 0;
				re->result->yof= 0;
			}
		}
	}
}


/* within context of current Render *re, render another scene.
   it uses current render image size and disprect, but doesn't execute composite
*/
static void render_scene(Render *re, Scene *sce, int cfra)
{
	Render *resc= RE_NewRender(sce->id.name);
	int winx= re->winx, winy= re->winy;
	
	sce->r.cfra= cfra;

	scene_camera_switch_update(sce);

	/* exception: scene uses own size (unfinished code) */
	if(0) {
		winx= (sce->r.size*sce->r.xsch)/100;
		winy= (sce->r.size*sce->r.ysch)/100;
	}
	
	/* initial setup */
	RE_InitState(resc, re, &sce->r, NULL, winx, winy, &re->disprect);
	
	/* still unsure entity this... */
	resc->main= re->main;
	resc->scene= sce;
	resc->lay= sce->lay;
	
	/* ensure scene has depsgraph, base flags etc OK */
	set_scene_bg(re->main, sce);

	/* copy callbacks */
	resc->display_draw= re->display_draw;
	resc->ddh= re->ddh;
	resc->test_break= re->test_break;
	resc->tbh= re->tbh;
	resc->stats_draw= re->stats_draw;
	resc->sdh= re->sdh;
	
	do_render_fields_blur_3d(resc);
}

/* helper call to detect if this scene needs a render, or if there's a any render layer to render */
static int composite_needs_render(Scene *sce, int this_scene)
{
	bNodeTree *ntree= sce->nodetree;
	bNode *node;
	
	if(ntree==NULL) return 1;
	if(sce->use_nodes==0) return 1;
	if((sce->r.scemode & R_DOCOMP)==0) return 1;
	
	for(node= ntree->nodes.first; node; node= node->next) {
		if(node->type==CMP_NODE_R_LAYERS)
			if(this_scene==0 || node->id==NULL || node->id==&sce->id)
				return 1;
	}
	return 0;
}

static void tag_scenes_for_render(Render *re)
{
	bNode *node;
	Scene *sce;
	
	for(sce= re->main->scene.first; sce; sce= sce->id.next)
		sce->id.flag &= ~LIB_DOIT;
	
	if(RE_GetCamera(re) && composite_needs_render(re->scene, 1))
		re->scene->id.flag |= LIB_DOIT;
	
	if(re->scene->nodetree==NULL) return;
	
	/* check for render-layers nodes using other scenes, we tag them LIB_DOIT */
	for(node= re->scene->nodetree->nodes.first; node; node= node->next) {
		if(node->type==CMP_NODE_R_LAYERS) {
			if(node->id) {
				if(node->id != (ID *)re->scene)
					node->id->flag |= LIB_DOIT;
			}
		}
	}
	
}

static void ntree_render_scenes(Render *re)
{
	bNode *node;
	int cfra= re->scene->r.cfra;
	int restore_scene= 0;
	
	if(re->scene->nodetree==NULL) return;
	
	tag_scenes_for_render(re);
	
	/* now foreach render-result node tagged we do a full render */
	/* results are stored in a way compisitor will find it */
	for(node= re->scene->nodetree->nodes.first; node; node= node->next) {
		if(node->type==CMP_NODE_R_LAYERS) {
			if(node->id && node->id != (ID *)re->scene) {
				if(node->id->flag & LIB_DOIT) {
					Scene *scene = (Scene*)node->id;

					render_scene(re, scene, cfra);
					restore_scene= (scene != re->scene);
					node->id->flag &= ~LIB_DOIT;
					
					nodeUpdate(re->scene->nodetree, node);
				}
			}
		}
	}

	/* restore scene if we rendered another last */
	if(restore_scene)
		set_scene_bg(re->main, re->scene);
}

/* bad call... need to think over proper method still */
static void render_composit_stats(void *UNUSED(arg), char *str)
{
	R.i.infostr= str;
	R.stats_draw(R.sdh, &R.i);
	R.i.infostr= NULL;
}


/* reads all buffers, calls optional composite, merges in first result->rectf */
static void do_merge_fullsample(Render *re, bNodeTree *ntree)
{
	float *rectf, filt[3][3];
	int sample;
	
	/* interaction callbacks */
	if(ntree) {
		ntree->stats_draw= render_composit_stats;
		ntree->test_break= re->test_break;
		ntree->progress= re->progress;
		ntree->sdh= re->sdh;
		ntree->tbh= re->tbh;
		ntree->prh= re->prh;
	}
	
	/* filtmask needs it */
	R= *re;
	
	/* we accumulate in here */
	rectf= MEM_mapallocN(re->rectx*re->recty*sizeof(float)*4, "fullsample rgba");
	
	for(sample=0; sample<re->r.osa; sample++) {
		Render *re1;
		RenderResult rres;
		int x, y, mask;
		
		/* enable full sample print */
		R.i.curfsa= sample+1;
		
		/* set all involved renders on the samplebuffers (first was done by render itself, but needs tagged) */
		/* also function below assumes this */
			
		tag_scenes_for_render(re);
		for(re1= RenderGlobal.renderlist.first; re1; re1= re1->next) {
			if(re1->scene->id.flag & LIB_DOIT) {
				if(re1->r.scemode & R_FULL_SAMPLE) {
					if(sample)
						read_render_result(re1, sample);
					ntreeCompositTagRender(re1->scene); /* ensure node gets exec to put buffers on stack */
				}
			}
		}
		
		/* composite */
		if(ntree) {
			ntreeCompositTagRender(re->scene);
			ntreeCompositTagAnimated(ntree);
			
			ntreeCompositExecTree(ntree, &re->r, G.background==0);
		}
		
		/* ensure we get either composited result or the active layer */
		RE_AcquireResultImage(re, &rres);
		
		/* accumulate with filter, and clip */
		mask= (1<<sample);
		mask_array(mask, filt);

		for(y=0; y<re->recty; y++) {
			float *rf= rectf + 4*y*re->rectx;
			float *col= rres.rectf + 4*y*re->rectx;
				
			for(x=0; x<re->rectx; x++, rf+=4, col+=4) {
				/* clamping to 1.0 is needed for correct AA */
				if(col[0]<0.0f) col[0]=0.0f; else if(col[0] > 1.0f) col[0]= 1.0f;
				if(col[1]<0.0f) col[1]=0.0f; else if(col[1] > 1.0f) col[1]= 1.0f;
				if(col[2]<0.0f) col[2]=0.0f; else if(col[2] > 1.0f) col[2]= 1.0f;
				
				add_filt_fmask_coord(filt, col, rf, re->rectx, re->recty, x, y);
			}
		}
		
		RE_ReleaseResultImage(re);

		/* show stuff */
		if(sample!=re->osa-1) {
			/* weak... the display callback wants an active renderlayer pointer... */
			re->result->renlay= render_get_active_layer(re, re->result);
			re->display_draw(re->ddh, re->result, NULL);
		}
		
		if(re->test_break(re->tbh))
			break;
	}
	
	/* clear interaction callbacks */
	if(ntree) {
		ntree->stats_draw= NULL;
		ntree->test_break= NULL;
		ntree->progress= NULL;
		ntree->tbh= ntree->sdh= ntree->prh= NULL;
	}
	
	/* disable full sample print */
	R.i.curfsa= 0;
	
	BLI_rw_mutex_lock(&re->resultmutex, THREAD_LOCK_WRITE);
	if(re->result->rectf) 
		MEM_freeN(re->result->rectf);
	re->result->rectf= rectf;
	BLI_rw_mutex_unlock(&re->resultmutex);
}

/* called externally, via compositor */
void RE_MergeFullSample(Render *re, Main *bmain, Scene *sce, bNodeTree *ntree)
{
	Scene *scene;
	bNode *node;

	/* default start situation */
	G.afbreek= 0;
	
	re->main= bmain;
	re->scene= sce;
	
	/* first call RE_ReadRenderResult on every renderlayer scene. this creates Render structs */
	
	/* tag scenes unread */
	for(scene= re->main->scene.first; scene; scene= scene->id.next) 
		scene->id.flag |= LIB_DOIT;
	
	for(node= ntree->nodes.first; node; node= node->next) {
		if(node->type==CMP_NODE_R_LAYERS) {
			Scene *nodescene= (Scene *)node->id;
			
			if(nodescene==NULL) nodescene= sce;
			if(nodescene->id.flag & LIB_DOIT) {
				nodescene->r.mode |= R_OSA;	/* render struct needs tables */
				RE_ReadRenderResult(sce, nodescene);
				nodescene->id.flag &= ~LIB_DOIT;
			}
		}
	}
	
	/* own render result should be read/allocated */
	if(re->scene->id.flag & LIB_DOIT) {
		RE_ReadRenderResult(re->scene, re->scene);
		re->scene->id.flag &= ~LIB_DOIT;
	}
	
	/* and now we can draw (result is there) */
	re->display_init(re->dih, re->result);
	re->display_clear(re->dch, re->result);
	
	do_merge_fullsample(re, ntree);
}

/* returns fully composited render-result on given time step (in RenderData) */
static void do_render_composite_fields_blur_3d(Render *re)
{
	bNodeTree *ntree= re->scene->nodetree;
	int update_newframe=0;
	
	/* INIT seeding, compositor can use random texture */
	BLI_srandom(re->r.cfra);
	
	if(composite_needs_render(re->scene, 1)) {
		/* save memory... free all cached images */
		ntreeFreeCache(ntree);
		
		do_render_fields_blur_3d(re);
	} 
	else {
		/* ensure new result gets added, like for regular renders */
		BLI_rw_mutex_lock(&re->resultmutex, THREAD_LOCK_WRITE);
		
		RE_FreeRenderResult(re->result);
		re->result= new_render_result(re, &re->disprect, 0, RR_USEMEM);

		BLI_rw_mutex_unlock(&re->resultmutex);
		
		/* scene render process already updates animsys */
		update_newframe = 1;
	}
	
	/* swap render result */
	if(re->r.scemode & R_SINGLE_LAYER)
		pop_render_result(re);
	
	if(!re->test_break(re->tbh)) {
		
		if(ntree) {
			ntreeCompositTagRender(re->scene);
			ntreeCompositTagAnimated(ntree);
		}
		
		if(ntree && re->scene->use_nodes && re->r.scemode & R_DOCOMP) {
			/* checks if there are render-result nodes that need scene */
			if((re->r.scemode & R_SINGLE_LAYER)==0)
				ntree_render_scenes(re);
			
			if(!re->test_break(re->tbh)) {
				ntree->stats_draw= render_composit_stats;
				ntree->test_break= re->test_break;
				ntree->progress= re->progress;
				ntree->sdh= re->sdh;
				ntree->tbh= re->tbh;
				ntree->prh= re->prh;
				
				/* in case it was never initialized */
				R.sdh= re->sdh;
				R.stats_draw= re->stats_draw;
				
				if (update_newframe)
					scene_update_for_newframe(re->main, re->scene, re->lay);
				
				if(re->r.scemode & R_FULL_SAMPLE) 
					do_merge_fullsample(re, ntree);
				else {
					ntreeCompositExecTree(ntree, &re->r, G.background==0);
				}
				
				ntree->stats_draw= NULL;
				ntree->test_break= NULL;
				ntree->progress= NULL;
				ntree->tbh= ntree->sdh= ntree->prh= NULL;
			}
		}
		else if(re->r.scemode & R_FULL_SAMPLE)
			do_merge_fullsample(re, NULL);
	}

	/* weak... the display callback wants an active renderlayer pointer... */
	re->result->renlay= render_get_active_layer(re, re->result);
	re->display_draw(re->ddh, re->result, NULL);
}

static void renderresult_stampinfo(Render *re)
{
	RenderResult rres;

	/* this is the basic trick to get the displayed float or char rect from render result */
	RE_AcquireResultImage(re, &rres);
	BKE_stamp_buf(re->scene, RE_GetCamera(re), (unsigned char *)rres.rect32, rres.rectf, rres.rectx, rres.recty, 4);
	RE_ReleaseResultImage(re);
}

int RE_seq_render_active(Scene *scene, RenderData *rd)
{
	Editing *ed;
	Sequence *seq;

	ed = scene->ed;
	
	if (!(rd->scemode & R_DOSEQ) || !ed || !ed->seqbase.first)
		return 0;
	
	for (seq= ed->seqbase.first; seq; seq= seq->next) {
		if (seq->type != SEQ_SOUND)
			return 1;
	}
	
	return 0;
}

static void do_render_seq(Render * re)
{
	static int recurs_depth = 0;
	struct ImBuf *ibuf;
	RenderResult *rr; /* don't assign re->result here as it might change during give_ibuf_seq */
	int cfra = re->r.cfra;
	SeqRenderData context;

	re->i.cfra= cfra;

	if(recurs_depth==0) {
		/* otherwise sequencer animation isnt updated */
		BKE_animsys_evaluate_all_animation(re->main, re->scene, (float)cfra); // XXX, was BKE_curframe(re->scene)
	}

	recurs_depth++;

	context = seq_new_render_data(re->main, re->scene,
				      re->result->rectx, re->result->recty, 
				      100);

	ibuf = give_ibuf_seq(context, cfra, 0);

	recurs_depth--;

	rr = re->result;
	
	BLI_rw_mutex_lock(&re->resultmutex, THREAD_LOCK_WRITE);

	if(ibuf) {
		if(ibuf->rect_float) {
			/* color management: when off ensure rectf is non-lin, since thats what the internal
			 * render engine delivers */
			int profile_to= (re->r.color_mgt_flag & R_COLOR_MANAGEMENT)? IB_PROFILE_LINEAR_RGB: IB_PROFILE_SRGB;
			int profile_from= (ibuf->profile == IB_PROFILE_LINEAR_RGB)? IB_PROFILE_LINEAR_RGB: IB_PROFILE_SRGB;
			int predivide= (re->r.color_mgt_flag & R_COLOR_MANAGEMENT_PREDIVIDE);

			if (!rr->rectf)
				rr->rectf= MEM_mallocN(4*sizeof(float)*rr->rectx*rr->recty, "render_seq rectf");
			
			IMB_buffer_float_from_float(rr->rectf, ibuf->rect_float,
				4, profile_to, profile_from, predivide,
				rr->rectx, rr->recty, rr->rectx, rr->rectx);
			
			/* TSK! Since sequence render doesn't free the *rr render result, the old rect32
			   can hang around when sequence render has rendered a 32 bits one before */
			if(rr->rect32) {
				MEM_freeN(rr->rect32);
				rr->rect32= NULL;
			}
		}
		else if(ibuf->rect) {
			if (!rr->rect32)
				rr->rect32= MEM_mallocN(sizeof(int)*rr->rectx*rr->recty, "render_seq rect");

			memcpy(rr->rect32, ibuf->rect, 4*rr->rectx*rr->recty);

			/* if (ibuf->zbuf) { */
			/* 	if (R.rectz) freeN(R.rectz); */
			/* 	R.rectz = BLI_dupallocN(ibuf->zbuf); */
			/* } */

			/* Same things as above, old rectf can hang around from previous render. */
			if(rr->rectf) {
				MEM_freeN(rr->rectf);
				rr->rectf= NULL;
			}
		}
		
		if (recurs_depth == 0) { /* with nested scenes, only free on toplevel... */
			Editing * ed = re->scene->ed;
			if (ed) {
				free_imbuf_seq(re->scene, &ed->seqbase, TRUE, TRUE);
			}
		}
		IMB_freeImBuf(ibuf);
	}
	else {
		/* render result is delivered empty in most cases, nevertheless we handle all cases */
		if (rr->rectf)
			memset(rr->rectf, 0, 4*sizeof(float)*rr->rectx*rr->recty);
		else if (rr->rect32)
			memset(rr->rect32, 0, 4*rr->rectx*rr->recty);
		else
			rr->rect32= MEM_callocN(sizeof(int)*rr->rectx*rr->recty, "render_seq rect");
	}

	BLI_rw_mutex_unlock(&re->resultmutex);

	/* just in case this flag went missing at some point */
	re->r.scemode |= R_DOSEQ;

	/* set overall progress of sequence rendering */
	if(re->r.efra!=re->r.sfra)
		re->progress(re->prh, (float)(cfra-re->r.sfra) / (re->r.efra-re->r.sfra));
	else
		re->progress(re->prh, 1.0f);
}

/* ~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~ */

/* main loop: doing sequence + fields + blur + 3d render + compositing */
static void do_render_all_options(Render *re)
{
	scene_camera_switch_update(re->scene);

	re->i.starttime= PIL_check_seconds_timer();

	/* ensure no images are in memory from previous animated sequences */
	BKE_image_all_free_anim_ibufs(re->r.cfra);

	if(RE_engine_render(re, 1)) {
		/* in this case external render overrides all */
	}
	else if(RE_seq_render_active(re->scene, &re->r)) {
		/* note: do_render_seq() frees rect32 when sequencer returns float images */
		if(!re->test_break(re->tbh)) 
			do_render_seq(re);
		
		re->stats_draw(re->sdh, &re->i);
		re->display_draw(re->ddh, re->result, NULL);
	}
	else {
		do_render_composite_fields_blur_3d(re);
	}
	
	/* for UI only */
	BLI_rw_mutex_lock(&re->resultmutex, THREAD_LOCK_WRITE);
	renderresult_add_names(re->result);
	BLI_rw_mutex_unlock(&re->resultmutex);
	
	re->i.lastframetime= PIL_check_seconds_timer()- re->i.starttime;
	
	re->stats_draw(re->sdh, &re->i);
	
	/* stamp image info here */
	if((re->r.stamp & R_STAMP_ALL) && (re->r.stamp & R_STAMP_DRAW)) {
		renderresult_stampinfo(re);
		re->display_draw(re->ddh, re->result, NULL);
	}
}

static int check_valid_camera(Scene *scene, Object *camera_override)
{
	int check_comp= 1;

	if (camera_override == NULL && scene->camera == NULL)
		scene->camera= scene_find_camera(scene);

	if(scene->r.scemode&R_DOSEQ) {
		if(scene->ed) {
			Sequence *seq= scene->ed->seqbase.first;

			check_comp= 0;

			while(seq) {
				if(seq->type == SEQ_SCENE) {
					if(!seq->scene_camera) {
						if(!seq->scene->camera && !scene_find_camera(seq->scene)) {
							if(seq->scene == scene) {
								/* for current scene camera could be unneeded due to compisite nodes */
								check_comp= 1;
							} else {
								/* for other scenes camera is necessary */
								return 0;
							}
						}
					}
				}

				seq= seq->next;
			}
		}
	}

	if(check_comp) { /* no sequencer or sequencer depends on compositor */
		if(scene->r.scemode&R_DOCOMP && scene->use_nodes) {
			bNode *node= scene->nodetree->nodes.first;

			while(node) {
				if(node->type == CMP_NODE_R_LAYERS) {
					Scene *sce= node->id ? (Scene*)node->id : scene;

					if(!sce->camera && !scene_find_camera(sce)) {
						/* all render layers nodes need camera */
						return 0;
					}
				}

				node= node->next;
			}
		} else {
			return (camera_override != NULL || scene->camera != NULL);
		}
	}

	return 1;
}

int RE_is_rendering_allowed(Scene *scene, Object *camera_override, ReportList *reports)
{
	SceneRenderLayer *srl;
	
	if(scene->r.mode & R_BORDER) {
		if(scene->r.border.xmax <= scene->r.border.xmin ||
		   scene->r.border.ymax <= scene->r.border.ymin) {
			BKE_report(reports, RPT_ERROR, "No border area selected.");
			return 0;
		}
	}
	
	if(scene->r.scemode & (R_EXR_TILE_FILE|R_FULL_SAMPLE)) {
		char str[FILE_MAX];
		
		scene_unique_exr_name(scene, str, 0);
		
		if (BLI_file_is_writable(str)==0) {
			BKE_report(reports, RPT_ERROR, "Can not save render buffers, check the temp default path");
			return 0;
		}
		
		/* no fullsample and edge */
		if((scene->r.scemode & R_FULL_SAMPLE) && (scene->r.mode & R_EDGE)) {
			BKE_report(reports, RPT_ERROR, "Full Sample doesn't support Edge Enhance");
			return 0;
		}
		
	}
	else
		scene->r.scemode &= ~R_FULL_SAMPLE;	/* clear to be sure */
	
	if(scene->r.scemode & R_DOCOMP) {
		if(scene->use_nodes) {
			bNodeTree *ntree= scene->nodetree;
			bNode *node;
		
			if(ntree==NULL) {
				BKE_report(reports, RPT_ERROR, "No Nodetree in Scene");
				return 0;
			}
			
			for(node= ntree->nodes.first; node; node= node->next)
				if(node->type==CMP_NODE_COMPOSITE)
					break;
			
			if(node==NULL) {
				BKE_report(reports, RPT_ERROR, "No Render Output Node in Scene");
				return 0;
			}
			
			if(scene->r.scemode & R_FULL_SAMPLE) {
				if(composite_needs_render(scene, 0)==0) {
					BKE_report(reports, RPT_ERROR, "Full Sample AA not supported without 3d rendering");
					return 0;
				}
			}
		}
	}
	
	 /* check valid camera, without camera render is OK (compo, seq) */
	if(!check_valid_camera(scene, camera_override)) {
		BKE_report(reports, RPT_ERROR, "No camera");
		return 0;
	}
	
	/* get panorama & ortho, only after camera is set */
	object_camera_mode(&scene->r, camera_override ? camera_override : scene->camera);

	/* forbidden combinations */
	if(scene->r.mode & R_PANORAMA) {
		if(scene->r.mode & R_ORTHO) {
			BKE_report(reports, RPT_ERROR, "No Ortho render possible for Panorama");
			return 0;
		}
	}

	/* layer flag tests */
	if(scene->r.scemode & R_SINGLE_LAYER) {
		srl= BLI_findlink(&scene->r.layers, scene->r.actlay);
		/* force layer to be enabled */
		srl->layflag &= ~SCE_LAY_DISABLE;
	}
	
	for(srl= scene->r.layers.first; srl; srl= srl->next)
		if(!(srl->layflag & SCE_LAY_DISABLE))
			break;
	if(srl==NULL) {
		BKE_report(reports, RPT_ERROR, "All RenderLayers are disabled");
		return 0;
	}

	return 1;
}

static void validate_render_settings(Render *re)
{
	if(re->r.scemode & (R_EXR_TILE_FILE|R_FULL_SAMPLE)) {
		/* no osa + fullsample won't work... */
		if(re->r.osa==0)
			re->r.scemode &= ~R_FULL_SAMPLE;
	} else re->r.scemode &= ~R_FULL_SAMPLE;	/* clear to be sure */

	if(RE_engine_is_external(re)) {
		/* not supported yet */
		re->r.scemode &= ~(R_EXR_TILE_FILE|R_FULL_SAMPLE);
	}
}

static void update_physics_cache(Render *re, Scene *scene, int UNUSED(anim_init))
{
	PTCacheBaker baker;

	baker.main = re->main;
	baker.scene = scene;
	baker.pid = NULL;
	baker.bake = 0;
	baker.render = 1;
	baker.anim_init = 1;
	baker.quick_step = 1;
	baker.break_test = re->test_break;
	baker.break_data = re->tbh;
	baker.progressbar = NULL;

	BKE_ptcache_bake(&baker);
}
/* evaluating scene options for general Blender render */
static int render_initialize_from_main(Render *re, Main *bmain, Scene *scene, SceneRenderLayer *srl, Object *camera_override, unsigned int lay, int anim, int anim_init)
{
	int winx, winy;
	rcti disprect;
	
	/* r.xsch and r.ysch has the actual view window size
		r.border is the clipping rect */
	
	/* calculate actual render result and display size */
	winx= (scene->r.size*scene->r.xsch)/100;
	winy= (scene->r.size*scene->r.ysch)/100;
	
	/* we always render smaller part, inserting it in larger image is compositor bizz, it uses disprect for it */
	if(scene->r.mode & R_BORDER) {
		disprect.xmin= scene->r.border.xmin*winx;
		disprect.xmax= scene->r.border.xmax*winx;
		
		disprect.ymin= scene->r.border.ymin*winy;
		disprect.ymax= scene->r.border.ymax*winy;
	}
	else {
		disprect.xmin= disprect.ymin= 0;
		disprect.xmax= winx;
		disprect.ymax= winy;
	}
	
	re->main= bmain;
	re->scene= scene;
	re->camera_override= camera_override;
	re->lay= lay;
	
	/* not too nice, but it survives anim-border render */
	if(anim) {
		re->disprect= disprect;
		return 1;
	}
	
	/* check all scenes involved */
	tag_scenes_for_render(re);

	/*
	 * Disabled completely for now,
	 * can be later set as render profile option
	 * and default for background render.
	*/
	if(0) {
		/* make sure dynamics are up to date */
		update_physics_cache(re, scene, anim_init);
	}
	
	if(srl || scene->r.scemode & R_SINGLE_LAYER)
		push_render_result(re);
	
	RE_InitState(re, NULL, &scene->r, srl, winx, winy, &disprect);
	if(!re->ok)  /* if an error was printed, abort */
		return 0;
	
	/* initstate makes new result, have to send changed tags around */
	ntreeCompositTagRender(re->scene);

	validate_render_settings(re);

	re->display_init(re->dih, re->result);
	re->display_clear(re->dch, re->result);
	
	return 1;
}

void RE_SetReports(Render *re, ReportList *reports)
{
	re->reports= reports;
}

/* general Blender frame render call */
void RE_BlenderFrame(Render *re, Main *bmain, Scene *scene, SceneRenderLayer *srl, Object *camera_override, unsigned int lay, int frame, const short write_still)
{
	/* ugly global still... is to prevent preview events and signal subsurfs etc to make full resol */
	G.rendering= 1;
	
	scene->r.cfra= frame;
	
	if(render_initialize_from_main(re, bmain, scene, srl, camera_override, lay, 0, 0)) {
		MEM_reset_peak_memory();

		BLI_exec_cb(re->main, (ID *)scene, BLI_CB_EVT_RENDER_PRE);

		do_render_all_options(re);

		if(write_still && !G.afbreek) {
			if(BKE_imtype_is_movie(scene->r.im_format.imtype)) {
				/* operator checks this but incase its called from elsewhere */
				printf("Error: cant write single images with a movie format!\n");
			}
			else {
				char name[FILE_MAX];
				BKE_makepicstring(name, scene->r.pic, bmain->name, scene->r.cfra, scene->r.im_format.imtype, scene->r.scemode & R_EXTENSION, FALSE);

				/* reports only used for Movie */
				do_write_image_or_movie(re, bmain, scene, NULL, name);
			}
		}

		BLI_exec_cb(re->main, (ID *)scene, BLI_CB_EVT_RENDER_POST); /* keep after file save */
	}

	/* UGLY WARNING */
	G.rendering= 0;
}

static int do_write_image_or_movie(Render *re, Main *bmain, Scene *scene, bMovieHandle *mh, const char *name_override)
{
	char name[FILE_MAX];
	RenderResult rres;
	Object *camera= RE_GetCamera(re);
	int ok= 1;
	
	RE_AcquireResultImage(re, &rres);

	/* write movie or image */
	if(BKE_imtype_is_movie(scene->r.im_format.imtype)) {
		int dofree = 0;
		unsigned int *rect32 = (unsigned int *)rres.rect32;
		/* note; the way it gets 32 bits rects is weak... */
		if(rres.rect32 == NULL) {
			rect32 = MEM_mapallocN(sizeof(int)*rres.rectx*rres.recty, "temp 32 bits rect");
			RE_ResultGet32(re, rect32);
			dofree = 1;
		}

		ok= mh->append_movie(&re->r, scene->r.cfra, (int *)rect32, rres.rectx, rres.recty, re->reports);
		if(dofree) {
			MEM_freeN(rect32);
		}
		printf("Append frame %d", scene->r.cfra);
	} 
	else {
		if(name_override)
			BLI_strncpy(name, name_override, sizeof(name));
		else
			BKE_makepicstring(name, scene->r.pic, bmain->name, scene->r.cfra, scene->r.im_format.imtype, scene->r.scemode & R_EXTENSION, TRUE);
		
		if(re->r.im_format.imtype==R_IMF_IMTYPE_MULTILAYER) {
			if(re->result) {
				RE_WriteRenderResult(re->reports, re->result, name, scene->r.im_format.compress);
				printf("Saved: %s", name);
			}
		}
		else {
			int flags = (scene->r.color_mgt_flag & R_COLOR_MANAGEMENT_PREDIVIDE)? IB_cm_predivide: 0;
			ImBuf *ibuf= IMB_allocImBuf(rres.rectx, rres.recty, scene->r.im_format.planes, flags);
			
			/* if not exists, BKE_write_ibuf makes one */
			ibuf->rect= (unsigned int *)rres.rect32;    
			ibuf->rect_float= rres.rectf;
			ibuf->zbuf_float= rres.rectz;
			
			/* float factor for random dither, imbuf takes care of it */
			ibuf->dither= scene->r.dither_intensity;
			
			/* prepare to gamma correct to sRGB color space */
			if (scene->r.color_mgt_flag & R_COLOR_MANAGEMENT) {
				/* sequence editor can generate 8bpc render buffers */
				if (ibuf->rect) {
					ibuf->profile = IB_PROFILE_SRGB;
					if (BKE_imtype_valid_depths(scene->r.im_format.imtype) & (R_IMF_CHAN_DEPTH_12|R_IMF_CHAN_DEPTH_16|R_IMF_CHAN_DEPTH_24|R_IMF_CHAN_DEPTH_32))
						IMB_float_from_rect(ibuf);
				} else {				
					ibuf->profile = IB_PROFILE_LINEAR_RGB;
				}
			}

			/* color -> greyscale */
			/* editing directly would alter the render view */
			if(scene->r.im_format.planes == R_IMF_PLANES_BW) {
				ImBuf *ibuf_bw= IMB_dupImBuf(ibuf);
				IMB_color_to_bw(ibuf_bw);
				IMB_freeImBuf(ibuf);
				ibuf= ibuf_bw;
			}

			ok= BKE_write_ibuf_stamp(scene, camera, ibuf, name, &scene->r.im_format);
			
			if(ok==0) {
				printf("Render error: cannot save %s\n", name);
			}
			else printf("Saved: %s", name);
			
			/* optional preview images for exr */
			if(ok && scene->r.im_format.imtype==R_IMF_IMTYPE_OPENEXR && (scene->r.im_format.flag & R_IMF_FLAG_PREVIEW_JPG)) {
				ImageFormatData imf= scene->r.im_format;
				imf.imtype= R_IMF_IMTYPE_JPEG90;

				if(BLI_testextensie(name, ".exr")) 
					name[strlen(name)-4]= 0;
				BKE_add_image_extension(name, R_IMF_IMTYPE_JPEG90);
				ibuf->planes= 24;
				BKE_write_ibuf_stamp(scene, camera, ibuf, name, &imf);
				printf("\nSaved: %s", name);
			}
			
					/* imbuf knows which rects are not part of ibuf */
			IMB_freeImBuf(ibuf);
		}
	}
	
	RE_ReleaseResultImage(re);

	BLI_timestr(re->i.lastframetime, name);
	printf(" Time: %s\n", name);
	fflush(stdout); /* needed for renderd !! (not anymore... (ton)) */

	return ok;
}

/* saves images to disk */
void RE_BlenderAnim(Render *re, Main *bmain, Scene *scene, Object *camera_override, unsigned int lay, int sfra, int efra, int tfra)
{
	bMovieHandle *mh= BKE_get_movie_handle(scene->r.im_format.imtype);
	int cfrao= scene->r.cfra;
	int nfra, totrendered= 0, totskipped= 0;
	
	/* do not fully call for each frame, it initializes & pops output window */
	if(!render_initialize_from_main(re, bmain, scene, NULL, camera_override, lay, 0, 1))
		return;
	
	/* ugly global still... is to prevent renderwin events and signal subsurfs etc to make full resol */
	/* is also set by caller renderwin.c */
	G.rendering= 1;

	re->flag |= R_ANIMATION;

	if(BKE_imtype_is_movie(scene->r.im_format.imtype))
		if(!mh->start_movie(scene, &re->r, re->rectx, re->recty, re->reports))
			G.afbreek= 1;

	if (mh->get_next_frame) {
		while (!(G.afbreek == 1)) {
			int nf = mh->get_next_frame(&re->r, re->reports);
			if (nf >= 0 && nf >= scene->r.sfra && nf <= scene->r.efra) {
				scene->r.cfra = re->r.cfra = nf;

				BLI_exec_cb(re->main, (ID *)scene, BLI_CB_EVT_RENDER_PRE);

				do_render_all_options(re);
				totrendered++;

				if(re->test_break(re->tbh) == 0) {
					if(!do_write_image_or_movie(re, bmain, scene, mh, NULL))
						G.afbreek= 1;
				}

				if(G.afbreek == 0) {
					BLI_exec_cb(re->main, (ID *)scene, BLI_CB_EVT_RENDER_POST); /* keep after file save */
				}
			}
			else {
				if(re->test_break(re->tbh))
					G.afbreek= 1;
			}
		}
	} else {
		for(nfra= sfra, scene->r.cfra= sfra; scene->r.cfra<=efra; scene->r.cfra++) {
			char name[FILE_MAX];
			
			/* only border now, todo: camera lens. (ton) */
			render_initialize_from_main(re, bmain, scene, NULL, camera_override, lay, 1, 0);

			if(nfra!=scene->r.cfra) {
				/*
				 * Skip this frame, but update for physics and particles system.
				 * From convertblender.c:
				 * in localview, lamps are using normal layers, objects only local bits.
				 */
				unsigned int updatelay;

				if(re->lay & 0xFF000000)
					updatelay= re->lay & 0xFF000000;
				else
					updatelay= re->lay;

				scene_update_for_newframe(bmain, scene, updatelay);
				continue;
			}
			else
				nfra+= tfra;

			/* Touch/NoOverwrite options are only valid for image's */
			if(BKE_imtype_is_movie(scene->r.im_format.imtype) == 0) {
				if(scene->r.mode & (R_NO_OVERWRITE | R_TOUCH))
					BKE_makepicstring(name, scene->r.pic, bmain->name, scene->r.cfra, scene->r.im_format.imtype, scene->r.scemode & R_EXTENSION, TRUE);

				if(scene->r.mode & R_NO_OVERWRITE && BLI_exists(name)) {
					printf("skipping existing frame \"%s\"\n", name);
					totskipped++;
					continue;
				}
				if(scene->r.mode & R_TOUCH && !BLI_exists(name)) {
					BLI_make_existing_file(name); /* makes the dir if its not there */
					BLI_file_touch(name);
				}
			}

			re->r.cfra= scene->r.cfra;	   /* weak.... */

			/* run callbacs before rendering, before the scene is updated */
			BLI_exec_cb(re->main, (ID *)scene, BLI_CB_EVT_RENDER_PRE);

			
			do_render_all_options(re);
			totrendered++;
			
			if(re->test_break(re->tbh) == 0) {
				if(!G.afbreek)
					if(!do_write_image_or_movie(re, bmain, scene, mh, NULL))
						G.afbreek= 1;
			}
			else
				G.afbreek= 1;
		
			if(G.afbreek==1) {
				/* remove touched file */
				if(BKE_imtype_is_movie(scene->r.im_format.imtype) == 0) {
					if (scene->r.mode & R_TOUCH && BLI_exists(name) && BLI_file_size(name) == 0) {
						BLI_delete(name, 0, 0);
					}
				}
				
				break;
			}

			if(G.afbreek==0) {
				BLI_exec_cb(re->main, (ID *)scene, BLI_CB_EVT_RENDER_POST); /* keep after file save */
			}
		}
	}
	
	/* end movie */
	if(BKE_imtype_is_movie(scene->r.im_format.imtype))
		mh->end_movie();
	
	if(totskipped && totrendered == 0)
		BKE_report(re->reports, RPT_INFO, "No frames rendered, skipped to not overwrite");

	scene->r.cfra= cfrao;

	re->flag &= ~R_ANIMATION;

	/* UGLY WARNING */
	G.rendering= 0;
}

void RE_PreviewRender(Render *re, Main *bmain, Scene *sce)
{
	Object *camera;
	int winx, winy;

	winx= (sce->r.size*sce->r.xsch)/100;
	winy= (sce->r.size*sce->r.ysch)/100;

	RE_InitState(re, NULL, &sce->r, NULL, winx, winy, NULL);

	re->main = bmain;
	re->scene = sce;
	re->lay = sce->lay;

	camera = RE_GetCamera(re);
	RE_SetCamera(re, camera);

	do_render_3d(re);
}

/* note; repeated win/disprect calc... solve that nicer, also in compo */

/* only the temp file! */
int RE_ReadRenderResult(Scene *scene, Scene *scenode)
{
	Render *re;
	int winx, winy;
	rcti disprect;
	
	/* calculate actual render result and display size */
	winx= (scene->r.size*scene->r.xsch)/100;
	winy= (scene->r.size*scene->r.ysch)/100;
	
	/* only in movie case we render smaller part */
	if(scene->r.mode & R_BORDER) {
		disprect.xmin= scene->r.border.xmin*winx;
		disprect.xmax= scene->r.border.xmax*winx;
		
		disprect.ymin= scene->r.border.ymin*winy;
		disprect.ymax= scene->r.border.ymax*winy;
	}
	else {
		disprect.xmin= disprect.ymin= 0;
		disprect.xmax= winx;
		disprect.ymax= winy;
	}
	
	if(scenode)
		scene= scenode;
	
	/* get render: it can be called from UI with draw callbacks */
	re= RE_GetRender(scene->id.name);
	if(re==NULL)
		re= RE_NewRender(scene->id.name);
	RE_InitState(re, NULL, &scene->r, NULL, winx, winy, &disprect);
	re->scene= scene;
	
	return read_render_result(re, 0);
}

void RE_set_max_threads(int threads)
{
	if (threads==0) {
		RenderGlobal.threads = BLI_system_thread_count();
	} else if(threads>=1 && threads<=BLENDER_MAX_THREADS) {
		RenderGlobal.threads= threads;
	} else {
		printf("Error, threads has to be in range 0-%d\n", BLENDER_MAX_THREADS);
	}
}

void RE_init_threadcount(Render *re) 
{
	if(RenderGlobal.threads >= 1) { /* only set as an arg in background mode */
		re->r.threads= MIN2(RenderGlobal.threads, BLENDER_MAX_THREADS);
	} else if ((re->r.mode & R_FIXED_THREADS)==0 || RenderGlobal.threads == 0) { /* Automatic threads */
		re->r.threads = BLI_system_thread_count();
	}
}

/* loads in image into a result, size must match
 * x/y offsets are only used on a partial copy when dimensions dont match */
void RE_layer_load_from_file(RenderLayer *layer, ReportList *reports, const char *filename, int x, int y)
{
	ImBuf *ibuf = IMB_loadiffname(filename, IB_rect);

	if(ibuf  && (ibuf->rect || ibuf->rect_float)) {
		if (ibuf->x == layer->rectx && ibuf->y == layer->recty) {
			if(ibuf->rect_float==NULL)
				IMB_float_from_rect(ibuf);

			memcpy(layer->rectf, ibuf->rect_float, sizeof(float)*4*layer->rectx*layer->recty);
		} else {
			if ((ibuf->x - x >= layer->rectx) && (ibuf->y - y >= layer->recty)) {
				ImBuf *ibuf_clip;

				if(ibuf->rect_float==NULL)
					IMB_float_from_rect(ibuf);

				ibuf_clip = IMB_allocImBuf(layer->rectx, layer->recty, 32, IB_rectfloat);
				if(ibuf_clip) {
					IMB_rectcpy(ibuf_clip, ibuf, 0,0, x,y, layer->rectx, layer->recty);

					memcpy(layer->rectf, ibuf_clip->rect_float, sizeof(float)*4*layer->rectx*layer->recty);
					IMB_freeImBuf(ibuf_clip);
				}
				else {
					BKE_reportf(reports, RPT_ERROR, "RE_result_rect_from_file: failed to allocate clip buffer '%s'\n", filename);
				}
			}
			else {
				BKE_reportf(reports, RPT_ERROR, "RE_result_rect_from_file: incorrect dimensions for partial copy '%s'\n", filename);
			}
		}

		IMB_freeImBuf(ibuf);
	}
	else {
		BKE_reportf(reports, RPT_ERROR, "RE_result_rect_from_file: failed to load '%s'\n", filename);
	}
}

void RE_result_load_from_file(RenderResult *result, ReportList *reports, const char *filename)
{
	if(!read_render_result_from_file(filename, result)) {
		BKE_reportf(reports, RPT_ERROR, "RE_result_rect_from_file: failed to load '%s'\n", filename);
		return;
	}
}

const float default_envmap_layout[] = { 0,0, 1,0, 2,0, 0,1, 1,1, 2,1 };

int RE_WriteEnvmapResult(struct ReportList *reports, Scene *scene, EnvMap *env, const char *relpath, const char imtype, float layout[12])
{
	ImageFormatData imf;
	ImBuf *ibuf=NULL;
	int ok;
	int dx;
	int maxX=0,maxY=0,i=0;
	char filepath[FILE_MAX];

	if(env->cube[1]==NULL) {
		BKE_report(reports, RPT_ERROR, "There is no generated environment map available to save");
		return 0;
	}

	imf= scene->r.im_format;
	imf.imtype= imtype;

	dx= env->cube[1]->x;

	if (env->type == ENV_CUBE) {
		for (i=0; i < 12; i+=2) {
			maxX = MAX2(maxX,layout[i] + 1);
			maxY = MAX2(maxY,layout[i+1] + 1);
		}

		ibuf = IMB_allocImBuf(maxX*dx, maxY*dx, 24, IB_rectfloat);

		for (i=0; i < 12; i+=2)
			if (layout[i] > -1 && layout[i+1] > -1)
				IMB_rectcpy(ibuf, env->cube[i/2], layout[i]*dx, layout[i+1]*dx, 0, 0, dx, dx);
	}
	else if (env->type == ENV_PLANE) {
		ibuf = IMB_allocImBuf(dx, dx, 24, IB_rectfloat);
		IMB_rectcpy(ibuf, env->cube[1], 0, 0, 0, 0, dx, dx);
	}
	else {
		BKE_report(reports, RPT_ERROR, "Invalid environment map type");
		return 0;
	}

	if (scene->r.color_mgt_flag & R_COLOR_MANAGEMENT)
		ibuf->profile = IB_PROFILE_LINEAR_RGB;

	/* to save, we first get absolute path */
	BLI_strncpy(filepath, relpath, sizeof(filepath));
	BLI_path_abs(filepath, G.main->name);

	ok= BKE_write_ibuf(ibuf, filepath, &imf);

	IMB_freeImBuf(ibuf);

	if(ok) {
		return TRUE;
	}
	else {
		BKE_report(reports, RPT_ERROR, "Error writing environment map.");
		return FALSE;
	}
}

