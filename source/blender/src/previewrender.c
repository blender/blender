/* 
 * $Id$
 *
 * ***** BEGIN GPL/BL DUAL LICENSE BLOCK *****
 *
 * This program is free software; you can redistribute it and/or
 * modify it under the terms of the GNU General Public License
 * as published by the Free Software Foundation; either version 2
 * of the License, or (at your option) any later version. The Blender
 * Foundation also sells licenses for use in proprietary software under
 * the Blender License.  See http://www.blender.org/BL/ for information
 * about this.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with this program; if not, write to the Free Software Foundation,
 * Inc., 59 Temple Place - Suite 330, Boston, MA  02111-1307, USA.
 *
 * The Original Code is Copyright (C) 2001-2002 by NaN Holding BV.
 * All rights reserved.
 *
 * The Original Code is: all of this file.
 *
 * Contributor(s): none yet.
 *
 * ***** END GPL/BL DUAL LICENSE BLOCK *****
 */

/* global includes */

#include <stdlib.h>
#include <math.h>
#include <string.h>

#ifdef HAVE_CONFIG_H
#include <config.h>
#endif

#ifndef WIN32
#include <unistd.h>
#else
#include <io.h>
#endif   
#include "MEM_guardedalloc.h"

#include "BLI_arithb.h"
#include "BLI_blenlib.h"

#include "MTC_matrixops.h"

#include "DNA_texture_types.h"
#include "DNA_world_types.h"
#include "DNA_camera_types.h"
#include "DNA_image_types.h"
#include "DNA_material_types.h"
#include "DNA_node_types.h"
#include "DNA_object_types.h"
#include "DNA_lamp_types.h"
#include "DNA_space_types.h"
#include "DNA_view3d_types.h"
#include "DNA_scene_types.h"
#include "DNA_screen_types.h"

#include "BKE_global.h"
#include "BKE_image.h"
#include "BKE_icons.h"
#include "BKE_library.h"
#include "BKE_main.h"
#include "BKE_texture.h"
#include "BKE_material.h"
#include "BKE_node.h"
#include "BKE_world.h"
#include "BKE_texture.h"
#include "BKE_utildefines.h"

#include "IMB_imbuf.h"
#include "IMB_imbuf_types.h"

#include "BSE_headerbuttons.h"
#include "BSE_node.h"
#include "BSE_view.h"

#include "BIF_gl.h"
#include "BIF_screen.h"
#include "BIF_space.h"		/* allqueue */
#include "BIF_butspace.h"	
#include "BIF_mywindow.h"
#include "BIF_interface.h"
#include "BIF_glutil.h"

#include "BIF_previewrender.h"  /* include ourself for prototypes */

#include "PIL_time.h"

#include "RE_pipeline.h"
#include "BLO_readfile.h" 

#include "blendef.h"	/* CLAMP */
#include "interface.h"	/* ui_graphics_to_window(),  SOLVE! (ton) */
#include "mydevice.h"


#define PR_XMIN		10
#define PR_YMIN		5
#define PR_XMAX		200
#define PR_YMAX		195


void set_previewrect(RenderInfo *ri, int win)
{
	rctf viewplane;
	
	BLI_init_rctf(&viewplane, PR_XMIN, PR_XMAX, PR_YMIN, PR_YMAX);

	ui_graphics_to_window_rct(win, &viewplane, &ri->disprect);
	
	/* correction for gla draw */
	BLI_translate_rcti(&ri->disprect, -curarea->winrct.xmin, -curarea->winrct.ymin);
	
	glMatrixMode(GL_PROJECTION);
	glPushMatrix();
	glMatrixMode(GL_MODELVIEW);
	glPushMatrix();
	
	glaDefine2DArea(&curarea->winrct);

	ri->pr_rectx= (ri->disprect.xmax-ri->disprect.xmin);
	ri->pr_recty= (ri->disprect.ymax-ri->disprect.ymin);
}

static void end_previewrect(void)
{
	glMatrixMode(GL_PROJECTION);
	glPopMatrix();
	glMatrixMode(GL_MODELVIEW);
	glPopMatrix();
	
	// restore viewport / scissor which was set by glaDefine2DArea
	glViewport(curarea->winrct.xmin, curarea->winrct.ymin, curarea->winx, curarea->winy);
	glScissor(curarea->winrct.xmin, curarea->winrct.ymin, curarea->winx, curarea->winy);

}

/* unused now */
void draw_tex_crop(Tex *tex)
{
	rcti rct;
	int ret= 0;
	
	if(tex==0) return;
	
	if(tex->type==TEX_IMAGE) {
		if(tex->cropxmin==0.0f) ret++;
		if(tex->cropymin==0.0f) ret++;
		if(tex->cropxmax==1.0f) ret++;
		if(tex->cropymax==1.0f) ret++;
		if(ret==4) return;
		
		rct.xmin= PR_XMIN+2+tex->cropxmin*(PR_XMAX-PR_XMIN-4);
		rct.xmax= PR_XMIN+2+tex->cropxmax*(PR_XMAX-PR_XMIN-4);
		rct.ymin= PR_YMIN+2+tex->cropymin*(PR_YMAX-PR_YMIN-4);
		rct.ymax= PR_YMIN+2+tex->cropymax*(PR_YMAX-PR_YMIN-4);

		glPolygonMode(GL_FRONT_AND_BACK, GL_LINE); 

		glColor3ub(0, 0, 0);
		glRecti(rct.xmin+1,  rct.ymin-1,  rct.xmax+1,  rct.ymax-1); 

		glColor3ub(255, 255, 255);
		glRecti(rct.xmin,  rct.ymin,  rct.xmax,  rct.ymax);

		glPolygonMode(GL_FRONT_AND_BACK, GL_FILL);			
	}
	
}

/* temporal abuse; if id_code is -1 it only does texture.... solve! */
void BIF_preview_changed(short id_code)
{
	ScrArea *sa;
	
	for(sa= G.curscreen->areabase.first; sa; sa= sa->next) {
		if(sa->spacetype==SPACE_BUTS) {
			SpaceButs *sbuts= sa->spacedata.first;
			if(sbuts->mainb==CONTEXT_SHADING) {
				int tab= sbuts->tab[CONTEXT_SHADING];
				if(tab==TAB_SHADING_MAT && (id_code==ID_MA || id_code==ID_TE)) {
					if (sbuts->ri) sbuts->ri->cury= 0;
					addafterqueue(sa->win, RENDERPREVIEW, 1);
				}
				else if(tab==TAB_SHADING_TEX && (id_code==ID_TE || id_code==-1)) {
					if (sbuts->ri) sbuts->ri->cury= 0;
					addafterqueue(sa->win, RENDERPREVIEW, 1);
				}
				else if(tab==TAB_SHADING_LAMP && (id_code==ID_LA || id_code==ID_TE)) {
					if (sbuts->ri) sbuts->ri->cury= 0;
					addafterqueue(sa->win, RENDERPREVIEW, 1);
				}
				else if(tab==TAB_SHADING_WORLD && (id_code==ID_WO || id_code==ID_TE)) {
					if (sbuts->ri) sbuts->ri->cury= 0;
					addafterqueue(sa->win, RENDERPREVIEW, 1);
				}
			}
		}
		else if(sa->spacetype==SPACE_NODE) {
			SpaceNode *snode= sa->spacedata.first;
			if(snode->treetype==NTREE_SHADER && (id_code==ID_MA || id_code==ID_TE)) {
				snode_tag_dirty(snode);
			}
		}
		else if(sa->spacetype==SPACE_VIEW3D) {
			View3D *vd= sa->spacedata.first;
			/* if is has a renderinfo, we consider that reason for signalling */
			if (vd->ri) {
				vd->ri->cury= 0;
				addafterqueue(sa->win, RENDERPREVIEW, 1);
			}
		}
	}
}

/* *************************** Preview for buttons *********************** */

static Main *pr_main= NULL;

void BIF_preview_init_dbase(void)
{
	BlendReadError bre;
	BlendFileData *bfd;
	extern int datatoc_preview_blend_size;
	extern char datatoc_preview_blend[];
	
	G.fileflags |= G_FILE_NO_UI;
	bfd= BLO_read_from_memory(datatoc_preview_blend, datatoc_preview_blend_size, &bre);
	if (bfd) {
		pr_main= bfd->main;
		
		MEM_freeN(bfd);
	}
	G.fileflags &= ~G_FILE_NO_UI;
}

void BIF_preview_free_dbase(void)
{
	if(pr_main)
		free_main(pr_main);
}

static Scene *preview_prepare_scene(RenderInfo *ri, ID *id, int pr_method)
{
	Scene *sce;
	Base *base;
	
	if(pr_main==NULL) return NULL;
	
	sce= pr_main->scene.first;
	if(sce) {
		if(GS(id->name)==ID_MA) {
			Material *mat= (Material *)id;
			
			if(pr_method==PR_ICON_RENDER) {
				sce->lay= 1<<MA_SPHERE_A;
			}
			else {
				sce->lay= 1<<mat->pr_type;
				if(mat->nodetree)
					ntreeInitPreview(mat->nodetree, ri->pr_rectx, ri->pr_recty);
			}
			
			for(base= sce->base.first; base; base= base->next) {
				if(base->object->id.name[2]=='p') {
					if(ELEM4(base->object->type, OB_MESH, OB_CURVE, OB_SURF, OB_MBALL))
						assign_material(base->object, mat, base->object->actcol);
				}
			}
		}
		return sce;
	}
	
	return NULL;
}

/* prevent pointer from being 'hanging' in preview dbase */
static void preview_exit_scene(RenderInfo *ri, ID *id)
{
	Scene *sce;
	Base *base;
	
	if(pr_main==NULL) return;
	
	sce= pr_main->scene.first;
	if(sce) {
		if(GS(id->name)==ID_MA) {
			for(base= sce->base.first; base; base= base->next) {
				if(base->object->id.name[2]=='p') {
					if(ELEM4(base->object->type, OB_MESH, OB_CURVE, OB_SURF, OB_MBALL))
						assign_material(base->object, NULL, base->object->actcol);
				}
			}
		}
	}
}

static void previewrender_progress(RenderResult *rr, rcti *unused)
{
	RenderLayer *rl;
	RenderInfo *ri= G.buts->ri;
	float ofsx, ofsy;
	
	rl= rr->layers.first;
	
	ofsx= ri->disprect.xmin + rr->tilerect.xmin;
	ofsy= ri->disprect.ymin + rr->tilerect.ymin;
	
	glDrawBuffer(GL_FRONT);
	glaDrawPixelsSafe(ofsx, ofsy, rr->rectx, rr->recty, rr->rectx, GL_RGBA, GL_FLOAT, rl->rectf);
	glFlush();
	glDrawBuffer(GL_BACK);
}


/* called by interface_icons.c, or by BIF_previewrender_buts or by nodes... */
void BIF_previewrender(struct ID *id, struct RenderInfo *ri, struct ScrArea *area, int pr_method)
{
	Render *re;
	RenderStats *rstats;
	Scene *sce;
	char name [32];
	
	if(ri->cury>=ri->pr_recty) return;
	
	if(ri->rect==NULL) {
		ri->rect= MEM_callocN(sizeof(int)*ri->pr_rectx*ri->pr_recty, "butsrect");
	}
	
	/* check for return with a new event */
	if(pr_method!=PR_ICON_RENDER && qtest()) {
		if(area)
			addafterqueue(area->win, RENDERPREVIEW, 1);
		return;
	}
	
	/* get the stuff from the builtin preview dbase */
	sce= preview_prepare_scene(ri, id, pr_method);
	if(sce==NULL) return;
	
	/* just create new render always now */
	sprintf(name, "ButsPreview %d", area?area->win:0);
	re= RE_NewRender(name);
	
	/* handle cases */
	if(pr_method==PR_DRAW_RENDER) {
		RE_display_draw_cb(re, previewrender_progress);
		RE_test_break_cb(re, qtest);
		sce->r.scemode |= R_NODE_PREVIEW;
	}
	else if(pr_method==PR_DO_RENDER) {
		RE_test_break_cb(re, qtest);
		sce->r.scemode |= R_NODE_PREVIEW;
	}
	else {	/* PR_ICON_RENDER */
		sce->r.scemode &= ~R_NODE_PREVIEW;
	}

	/* entire cycle for render engine */
	RE_InitState(re, &sce->r, ri->pr_rectx, ri->pr_recty, NULL);
	RE_SetCamera(re, sce->camera);
	RE_Database_FromScene(re, sce, 1);
	RE_TileProcessor(re);	// actual render engine
	RE_Database_Free(re);

	/* handle results */
	if(pr_method==PR_ICON_RENDER) {
		RE_ResultGet32(re, ri->rect);
	}
	else {
		rstats= RE_GetStats(re);
		if(rstats->totpart==rstats->partsdone && rstats->partsdone) {
			ri->cury= ri->pr_recty;
			RE_ResultGet32(re, ri->rect);
			if(GS(id->name)==ID_MA && ((Material *)id)->use_nodes)
				allqueue(REDRAWNODE, 0);
		}
		else {
			if(pr_method==PR_DRAW_RENDER && qtest()) {
				addafterqueue(area->win, RENDERPREVIEW, 1);
			}
		}
	}
	preview_exit_scene(ri, id);
	RE_FreeRender(re);
}


/* afterqueue call */
void BIF_previewrender_buts(SpaceButs *sbuts)
{
	uiBlock *block;
	struct ID* id = 0;
	struct ID* idfrom = 0;
	struct ID* idshow = 0;
	Object *ob;
	
	if (!sbuts->ri) return;
	
	/* we safely assume curarea has panel "preview" */
	/* quick hack for now, later on preview should become uiBlock itself */
	
	block= uiFindOpenPanelBlockName(&curarea->uiblocks, "Preview");
	if(block==NULL) return;
	
	ob= ((G.scene->basact)? (G.scene->basact)->object: 0);
	
	/* we cant trust this global lockpoin.. for example with headerless window */
	buttons_active_id(&id, &idfrom);
	G.buts->lockpoin= id;
	
	if(sbuts->mainb==CONTEXT_SHADING) {
		int tab= sbuts->tab[CONTEXT_SHADING];
		
		if(tab==TAB_SHADING_MAT) 
			idshow = sbuts->lockpoin;
		else if(tab==TAB_SHADING_TEX) 
			idshow = sbuts->lockpoin;
		else if(tab==TAB_SHADING_LAMP) {
			if(ob && ob->type==OB_LAMP) idshow= ob->data;
		}
		else if(tab==TAB_SHADING_WORLD)
			idshow = sbuts->lockpoin;
	}
	else if(sbuts->mainb==CONTEXT_OBJECT) {
		if(ob && ob->type==OB_LAMP) idshow = ob->data;
	}
	
	if (idshow) {
		BKE_icon_changed(BKE_icon_getid(idshow));
		uiPanelPush(block);
		set_previewrect(sbuts->ri, sbuts->area->win); // uses UImat
		BIF_previewrender(idshow, sbuts->ri, sbuts->area, PR_DRAW_RENDER);
		uiPanelPop(block);
		end_previewrect();
	}
	else {
		/* no active block to draw. But we do draw black if possible */
		if(sbuts->ri->rect) {
			memset(sbuts->ri->rect, 0, sizeof(int)*sbuts->ri->pr_rectx*sbuts->ri->pr_recty);
			sbuts->ri->cury= sbuts->ri->pr_recty;
			addqueue(curarea->win, REDRAW, 1);
		}
		return;
	}
}


/* is panel callback, supposed to be called with correct panel offset matrix */
void BIF_previewdraw(ScrArea *sa, uiBlock *block)
{
	SpaceButs *sbuts= sa->spacedata.first;
	short id_code= 0;
	
	if(sbuts->lockpoin) {
		ID *id= sbuts->lockpoin;
		id_code= GS(id->name);
	}
	
	if (!sbuts->ri) {
		sbuts->ri= MEM_callocN(sizeof(RenderInfo), "butsrenderinfo");
		sbuts->ri->cury = 0;
		sbuts->ri->rect = NULL;
	}
	
	if (sbuts->ri->rect==NULL) BIF_preview_changed(id_code);
	else {
		RenderInfo *ri= sbuts->ri;
		int oldx= ri->pr_rectx, oldy= ri->pr_recty;
		
		/* we now do scalable previews! */
		set_previewrect(ri, sa->win);
		if(oldx==ri->pr_rectx && oldy==ri->pr_recty) 
			glaDrawPixelsSafe(ri->disprect.xmin, ri->disprect.ymin, ri->pr_rectx, ri->pr_recty, ri->pr_rectx, GL_RGBA, GL_UNSIGNED_BYTE, ri->rect);
		else {
			MEM_freeN(ri->rect);
			ri->rect= NULL;
			sbuts->ri->cury= 0;
		}
		end_previewrect();
	}
	if(sbuts->ri->cury==0) BIF_preview_changed(id_code);
	
}

/* *************************** Preview for 3d window *********************** */
static void view3d_previewrender_stats(RenderStats *rs)
{
	printf("rendered %.3f\n", rs->lastframetime);
}

static void view3d_previewrender_progress(RenderResult *rr, rcti *unused)
{
	RenderLayer *rl;
	int ofsx, ofsy;
	
	rl= rr->layers.first;
	
	/* this case is when we render envmaps... */
	if(rr->rectx>G.vd->ri->pr_rectx || rr->recty>G.vd->ri->pr_recty)
		return;
	
	ofsx= G.vd->ri->disprect.xmin + rr->tilerect.xmin;
	ofsy= G.vd->ri->disprect.ymin + rr->tilerect.ymin;
	
	glDrawBuffer(GL_FRONT);
	glaDefine2DArea(&curarea->winrct);
	glaDrawPixelsSafe(ofsx, ofsy, rr->rectx, rr->recty, rr->rectx, GL_RGBA, GL_FLOAT, rl->rectf);
	glFlush();
	glDrawBuffer(GL_BACK);

}

void BIF_view3d_previewrender_signal(ScrArea *sa, short signal)
{
	View3D *v3d= sa->spacedata.first;
	
	/* this can be called from other window... solve! */
	if(sa->spacetype!=SPACE_VIEW3D)
		v3d= G.vd;
	   
	if(v3d && v3d->ri) {
		RenderInfo *ri= v3d->ri;
		ri->status &= ~signal;
		ri->cury= 0;
		if(ri->re && (signal & PR_DBASE))
			RE_Database_Free(ri->re);

		addafterqueue(sa->win, RENDERPREVIEW, 1);
	}
}

void BIF_view3d_previewrender_free(ScrArea *sa)
{
	View3D *v3d= sa->spacedata.first;

	if(v3d->ri) {
		RenderInfo *ri= v3d->ri;
		if(ri->re) {
//			printf("free render\n");
			RE_Database_Free(ri->re);
			RE_FreeRender(ri->re);
			ri->re= NULL;
		}
		ri->status= 0;
		ri->cury= 0;
	}
}

/* returns 1 if OK, do not call while in panel space!  */
static int view3d_previewrender_get_rects(ScrArea *sa, rctf *viewplane, RenderInfo *ri, float *clipsta, float *clipend, int *ortho)
{
	int rectx, recty;
	uiBlock *block;
	
	block= uiFindOpenPanelBlockName(&curarea->uiblocks, "Preview");
	if(block==NULL) return 0;
	
	/* calculate preview rect size */
	BLI_init_rctf(viewplane, 15.0f, (block->maxx - block->minx)-15.0f, 15.0f, (block->maxy - block->miny)-15.0f);
	uiPanelPush(block);
	ui_graphics_to_window_rct(sa->win, viewplane, &ri->disprect);
	uiPanelPop(block);
	
	/* correction for gla draw */
	BLI_translate_rcti(&ri->disprect, -sa->winrct.xmin, -sa->winrct.ymin);
	
	*ortho= get_view3d_viewplane(sa->winx, sa->winy, viewplane, clipsta, clipend);

	rectx= ri->disprect.xmax - ri->disprect.xmin;
	recty= ri->disprect.ymax - ri->disprect.ymin;
	
	if(rectx<4 || recty<4) return 0;
	
	if(ri->rect && (rectx!=ri->pr_rectx || recty!=ri->pr_recty)) {
		MEM_freeN(ri->rect);
		ri->rect= NULL;
	}
	ri->pr_rectx= rectx;
	ri->pr_recty= recty;
	
	return 1;
}

/* called before a panel gets moved/scaled, makes sure we can see through */
void BIF_view3d_previewrender_clear(ScrArea *sa)
{
	View3D *v3d= sa->spacedata.first;

	if(v3d->ri) {
		RenderInfo *ri= v3d->ri;
		ri->cury= 0;
		if(ri->rect)
			MEM_freeN(ri->rect);
		ri->rect= NULL;
	}
}

/* afterqueue call */
void BIF_view3d_previewrender(ScrArea *sa)
{
	View3D *v3d= sa->spacedata.first;
	Render *re;
	RenderInfo *ri;	/* preview struct! */
	RenderStats *rstats;
	RenderData rdata;
	rctf viewplane;
	float clipsta, clipend;
	int orth;
	
	/* first get the render info right */
	if (!v3d->ri)
		ri= v3d->ri= MEM_callocN(sizeof(RenderInfo), "butsrenderinfo");
	ri= v3d->ri;
	
	if(0==view3d_previewrender_get_rects(sa, &viewplane, ri, &clipsta, &clipend, &orth))
		return;
	
	/* render is finished, so return */
	if(ri->cury>=ri->pr_rectx) return;

	/* or return with a new event */
	if(qtest()) {
		addafterqueue(curarea->win, RENDERPREVIEW, 1);
		return;
	}
	
	/* ok, are we rendering all over? */
	if(ri->re==NULL) {
		char name[32];
		
		ri->status= 0;
		
		sprintf(name, "View3dPreview %d", sa->win);
		re= ri->re= RE_NewRender(name);
		RE_display_draw_cb(re, view3d_previewrender_progress);
		RE_stats_draw_cb(re, view3d_previewrender_stats);
		RE_test_break_cb(re, qtest);
		
		/* no osa, blur, seq, for preview render */
		rdata= G.scene->r;
		rdata.mode &= ~(R_OSA|R_MBLUR|R_DOSEQ);
		rdata.layers.first= rdata.layers.last= NULL;
	
		RE_InitState(re, &rdata, sa->winx, sa->winy, &ri->disprect);
	
		if(orth)
			RE_SetOrtho(re, &viewplane, clipsta, clipend);
		else
			RE_SetWindow(re, &viewplane, clipsta, clipend);
		
		/* until here are no escapes */
		ri->status |= PR_DISPRECT;
	}

	re= ri->re;
	
	PIL_sleep_ms(100);	/* wait 0.1 second if theres really no event... */
	if(qtest()==0)	{
		
		/* check status */
		if((ri->status & PR_DISPRECT)==0) {
			RE_SetDispRect(ri->re, &ri->disprect);
			if(orth)
				RE_SetOrtho(ri->re, &viewplane, clipsta, clipend);
			else
				RE_SetWindow(ri->re, &viewplane, clipsta, clipend);
			ri->status |= PR_DISPRECT;
		}
		if((ri->status & PR_DBASE)==0) {
			unsigned int lay= G.scene->lay;
			
			RE_SetView(re, G.vd->viewmat);
			
			/* allow localview render for objects with lights in normal layers */
			if(v3d->lay & 0xFF000000)
				G.scene->lay |= v3d->lay;
			else G.scene->lay= v3d->lay;
			
			RE_Database_FromScene(re, G.scene, 0);		// 0= dont use camera view
			G.scene->lay= lay;
			
			rstats= RE_GetStats(re);
			if(rstats->convertdone) 
				ri->status |= PR_DBASE|PR_PROJECTED|PR_ROTATED;
		}
		if((ri->status & PR_PROJECTED)==0) {
			if(ri->status & PR_DBASE) {
				if(orth)
					RE_SetOrtho(ri->re, &viewplane, clipsta, clipend);
				else
					RE_SetWindow(ri->re, &viewplane, clipsta, clipend);
				RE_DataBase_ApplyWindow(re);
				ri->status |= PR_PROJECTED;
			}
		}
	
		/* OK, can we enter render code? */
		if(ri->status==(PR_DISPRECT|PR_DBASE|PR_PROJECTED|PR_ROTATED)) {
			RE_TileProcessor(ri->re);
	
			if(ri->rect==NULL)
				ri->rect= MEM_callocN(sizeof(int)*ri->pr_rectx*ri->pr_recty, "preview view3d rect");
			
			RE_ResultGet32(ri->re, ri->rect);
		}
		
		rstats= RE_GetStats(ri->re);
		if(rstats->totpart==rstats->partsdone && rstats->partsdone) {
			ri->cury= 12000;	/* arbitrary... */
			addqueue(sa->win, REDRAW, 1);
		}
		else {
			addafterqueue(curarea->win, RENDERPREVIEW, 1);
			ri->cury= 0;
		}
	}
	else {
		addafterqueue(curarea->win, RENDERPREVIEW, 1);
		ri->cury= 0;
	}
}

/* in panel space! */
static void view3d_previewdraw_rect(ScrArea *sa, uiBlock *block, RenderInfo *ri)
{
	rctf dispf;
	
	if(ri->rect==NULL)
		return;
	
	BLI_init_rctf(&dispf, 15.0f, (block->maxx - block->minx)-15.0f, 15.0f, (block->maxy - block->miny)-15.0f);
	ui_graphics_to_window_rct(sa->win, &dispf, &ri->disprect);

	/* correction for gla draw */
	BLI_translate_rcti(&ri->disprect, -curarea->winrct.xmin, -curarea->winrct.ymin);
	
	/* when panel scale changed, free rect */
	if(ri->disprect.xmax-ri->disprect.xmin != ri->pr_rectx ||
	   ri->disprect.ymax-ri->disprect.ymin != ri->pr_recty) {
		MEM_freeN(ri->rect);
		ri->rect= NULL;
	}
	else {
		glaDefine2DArea(&sa->winrct);
		glaDrawPixelsSafe(ri->disprect.xmin, ri->disprect.ymin, ri->pr_rectx, ri->pr_recty, ri->pr_rectx, GL_RGBA, GL_UNSIGNED_BYTE, ri->rect);
	}	
}

/* is panel callback, supposed to be called with correct panel offset matrix */
void BIF_view3d_previewdraw(struct ScrArea *sa, struct uiBlock *block)
{
	View3D *v3d= sa->spacedata.first;

	if (v3d->ri==NULL || v3d->ri->rect==NULL) 
		addafterqueue(sa->win, RENDERPREVIEW, 1);
	else {
		view3d_previewdraw_rect(sa, block, v3d->ri);
		if(v3d->ri->cury==0) 
			addafterqueue(sa->win, RENDERPREVIEW, 1);
	}
}

