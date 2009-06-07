/* 
 * $Id$
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
 * Inc., 59 Temple Place - Suite 330, Boston, MA  02111-1307, USA.
 *
 * The Original Code is Copyright (C) Blender Foundation.
 * All rights reserved.
 *
 * The Original Code is: all of this file.
 *
 * Contributor(s): none yet.
 *
 * ***** END GPL LICENSE BLOCK *****
 */

/* global includes */

#include <stdlib.h>
#include <math.h>
#include <string.h>

#ifndef WIN32
#include <unistd.h>
#else
#include <io.h>
#endif   
#include "MEM_guardedalloc.h"

#include "BLO_readfile.h" 

#include "BLI_arithb.h"
#include "BLI_blenlib.h"
#include "BLI_threads.h"

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

#include "BKE_context.h"
#include "BKE_depsgraph.h"
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

#include "BIF_gl.h"
#include "BIF_glutil.h"

#include "PIL_time.h"

#include "RE_pipeline.h"

#include "GPU_material.h"

#include "WM_api.h"
#include "WM_types.h"

#include "ED_anim_api.h"
#include "ED_previewrender.h"
#include "ED_view3d.h"

#include "UI_interface.h"

#include "previewrender_intern.h"

#define PR_XMIN		10
#define PR_YMIN		5
#define PR_XMAX		200
#define PR_YMAX		195

/* XXX */
static int qtest() {return 0;}
/* XXX */

typedef struct ShaderPreview {
	/* from wmJob */
	void *owner;
	short *stop, *do_update;
	
	Scene *scene;
	ID *id;
	
	int sizex, sizey;
	int pr_method;
	
} ShaderPreview;


static void set_previewrect(ScrArea *sa, RenderInfo *ri)
{
	ARegion *ar= NULL; // XXX
	rctf viewplane;
	
	BLI_init_rctf(&viewplane, PR_XMIN, PR_XMAX, PR_YMIN, PR_YMAX);

//	ui_graphics_to_window_rct(ar->win, &viewplane, &ri->disprect);
	
	/* correction for gla draw */
	BLI_translate_rcti(&ri->disprect, -ar->winrct.xmin, -ar->winrct.ymin);
	
	glMatrixMode(GL_PROJECTION);
	glPushMatrix();
	glMatrixMode(GL_MODELVIEW);
	glPushMatrix();
	
	glaDefine2DArea(&ar->winrct);

	ri->pr_rectx= (ri->disprect.xmax-ri->disprect.xmin);
	ri->pr_recty= (ri->disprect.ymax-ri->disprect.ymin);
}

static void end_previewrect(ARegion *ar)
{
	glMatrixMode(GL_PROJECTION);
	glPopMatrix();
	glMatrixMode(GL_MODELVIEW);
	glPopMatrix();
	
	// restore viewport / scissor which was set by glaDefine2DArea
	glViewport(ar->winrct.xmin, ar->winrct.ymin, ar->winx, ar->winy);
	glScissor(ar->winrct.xmin, ar->winrct.ymin, ar->winx, ar->winy);

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
#if 0	
	ScrArea *sa;
	
	for(sa= G.curscreen->areabase.first; sa; sa= sa->next) {
		if(sa->spacetype==SPACE_BUTS) {
			SpaceButs *sbuts= sa->spacedata.first;
			if(sbuts->mainb==CONTEXT_SHADING) {
				int tab= sbuts->tab[CONTEXT_SHADING];
				if(tab==TAB_SHADING_MAT && (id_code==ID_MA || id_code==ID_TE)) {
					if (sbuts->ri) sbuts->ri->curtile= 0;
					addafterqueue(sa->win, RENDERPREVIEW, 1);
				}
				else if(tab==TAB_SHADING_TEX && (id_code==ID_TE || id_code==-1)) {
					if (sbuts->ri) sbuts->ri->curtile= 0;
					addafterqueue(sa->win, RENDERPREVIEW, 1);
				}
				else if(tab==TAB_SHADING_LAMP && (id_code==ID_LA || id_code==ID_TE)) {
					if (sbuts->ri) sbuts->ri->curtile= 0;
					addafterqueue(sa->win, RENDERPREVIEW, 1);
				}
				else if(tab==TAB_SHADING_WORLD && (id_code==ID_WO || id_code==ID_TE)) {
					if (sbuts->ri) sbuts->ri->curtile= 0;
					addafterqueue(sa->win, RENDERPREVIEW, 1);
				}
			}
			else if (sbuts->ri) 
				sbuts->ri->curtile= 0;	/* ensure changes always result in re-render when context is restored */
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
				vd->ri->curtile= 0;
				addafterqueue(sa->win, RENDERPREVIEW, 1);
			}
		}
	}

	if(ELEM4(id_code, ID_MA, ID_TE, ID_LA, ID_WO)) {
		Object *ob;
		Material *ma;

		if(id_code == ID_WO) {
			for(ma=G.main->mat.first; ma; ma=ma->id.next) {
				if(ma->gpumaterial.first) {
					GPU_material_free(ma);
				}
			}
		}
		else if(id_code == ID_LA) {
			for(ob=G.main->object.first; ob; ob=ob->id.next) {
				if(ob->gpulamp.first) {
					GPU_lamp_free(ob);
				}
			}
		} else if(OBACT) {
			Object *ob = OBACT;

			ma= give_current_material(ob, ob->actcol);
			if(ma && ma->gpumaterial.first) {
				GPU_material_free(ma);
			}
		}
	}
#endif
}

/* *************************** Preview for buttons *********************** */

static Main *pr_main= NULL;

void ED_preview_init_dbase(void)
{
	BlendFileData *bfd;
	extern int datatoc_preview_blend_size;
	extern char datatoc_preview_blend[];
	
	G.fileflags |= G_FILE_NO_UI;
	bfd= BLO_read_from_memory(datatoc_preview_blend, datatoc_preview_blend_size, NULL);
	if (bfd) {
		pr_main= bfd->main;
		
		MEM_freeN(bfd);
	}
	G.fileflags &= ~G_FILE_NO_UI;
}

void ED_preview_free_dbase(void)
{
	if(pr_main)
		free_main(pr_main);
}

static Object *find_object(ListBase *lb, const char *name)
{
	Object *ob;
	for(ob= lb->first; ob; ob= ob->id.next)
		if(strcmp(ob->id.name+2, name)==0)
			break;
	return ob;
}

/* call this with a pointer to initialize preview scene */
/* call this with NULL to restore assigned ID pointers in preview scene */
static Scene *preview_prepare_scene(Scene *scene, int id_type, ShaderPreview *sp)
{
	Scene *sce;
	Base *base;
	ID *id= sp?sp->id:NULL;
	
	if(pr_main==NULL) return NULL;
	
	sce= pr_main->scene.first;
	if(sce) {
		/* this flag tells render to not execute depsgraph or ipos etc */
		sce->r.scemode |= R_PREVIEWBUTS;
		/* set world always back, is used now */
		sce->world= pr_main->world.first;
		/* now: exposure copy */
		if(scene->world) {
			sce->world->exp= scene->world->exp;
			sce->world->range= scene->world->range;
		}
		
		sce->r.cfra= scene->r.cfra;
		
		if(id_type==ID_MA) {
			Material *mat= (Material *)id;
			
			if(id) {
				init_render_material(mat, 0, NULL);		/* call that retrieves mode_l */
				end_render_material(mat);
				
				/* turn on raytracing if needed */
				if(mat->mode_l & (MA_RAYTRANSP|MA_RAYMIRROR))
					sce->r.mode |= R_RAYTRACE;
				if(mat->sss_flag & MA_DIFF_SSS)
					sce->r.mode |= R_SSS;
				
				/* turn off fake shadows if needed */
				/* this only works in a specific case where the preview.blend contains
				 * an object starting with 'c' which has a material linked to it (not the obdata)
				 * and that material has a fake shadow texture in the active texture slot */
				for(base= sce->base.first; base; base= base->next) {
					if(base->object->id.name[2]=='c') {
						Material *shadmat= give_current_material(base->object, base->object->actcol);
						if(shadmat) {
							if (mat->mode & MA_SHADBUF) shadmat->septex = 0;
							else shadmat->septex |= 1;
						}
					}
				}

				
				if(sp->pr_method==PR_ICON_RENDER) {
					if (mat->material_type == MA_TYPE_HALO) {
						sce->lay= 1<<MA_FLAT;
					} 
					else {
						sce->lay= 1<<MA_SPHERE_A;
					}
				}
				else {
					sce->lay= 1<<mat->pr_type;
					if(mat->nodetree)
						ntreeInitPreview(mat->nodetree, sp->sizex, sp->sizey);
				}
			}
			else {
				sce->r.mode &= ~(R_OSA|R_RAYTRACE|R_SSS);
			}
			
			for(base= sce->base.first; base; base= base->next) {
				if(base->object->id.name[2]=='p') {
					if(ELEM4(base->object->type, OB_MESH, OB_CURVE, OB_SURF, OB_MBALL))
						assign_material(base->object, mat, base->object->actcol);
				}
			}
		}
		else if(id_type==ID_TE) {
			Tex *tex= (Tex *)id;
			
			sce->lay= 1<<MA_TEXTURE;
			
			for(base= sce->base.first; base; base= base->next) {
				if(base->object->id.name[2]=='t') {
					Material *mat= give_current_material(base->object, base->object->actcol);
					if(mat && mat->mtex[0]) {
						mat->mtex[0]->tex= tex;
						/* show alpha in this case */
						if(tex==NULL || (tex->flag & TEX_PRV_ALPHA)) {
							mat->mtex[0]->mapto |= MAP_ALPHA;
							mat->alpha= 0.0f;
						}
						else {
							mat->mtex[0]->mapto &= ~MAP_ALPHA;
							mat->alpha= 1.0f;
						}
					}
				}
			}
		}
		else if(id_type==ID_LA) {
			Lamp *la= (Lamp *)id;
			
			if(la && la->type==LA_SUN && (la->sun_effect_type & LA_SUN_EFFECT_SKY)) {
				sce->lay= 1<<MA_ATMOS;
				sce->world= scene->world;
				sce->camera= (Object *)find_object(&pr_main->object, "CameraAtmo");
			}
			else {
				sce->lay= 1<<MA_LAMP;
				sce->world= NULL;
				sce->camera= (Object *)find_object(&pr_main->object, "Camera");
			}
			sce->r.mode &= ~R_SHADOW;
			
			for(base= sce->base.first; base; base= base->next) {
				if(base->object->id.name[2]=='p') {
					if(base->object->type==OB_LAMP)
						base->object->data= la;
				}
			}
		}
		else if(id_type==ID_WO) {
			sce->lay= 1<<MA_SKY;
			sce->world= (World *)id;
		}
		
		return sce;
	}
	
	return NULL;
}

void previewrender_progress(void *handle, RenderResult *rr, volatile rcti *renrect)
{
	SpaceButs *sbuts= NULL; // XXX
	RenderLayer *rl;
	RenderInfo *ri= sbuts->ri;
	float ofsx, ofsy;
	
	if(renrect) return;
	
	rl= rr->layers.first;
	
	ofsx= ri->disprect.xmin + rr->tilerect.xmin;
	ofsy= ri->disprect.ymin + rr->tilerect.ymin;
	
	glDrawBuffer(GL_FRONT);
	glaDrawPixelsSafe_to32(ofsx, ofsy, rr->rectx, rr->recty, rr->rectx, rl->rectf);
	bglFlush();
	glDrawBuffer(GL_BACK);
}


/* called by interface_icons.c, or by BIF_previewrender_buts or by nodes... */
void BIF_previewrender(Scene *scene, struct ID *id, struct RenderInfo *ri, struct ScrArea *area, int pr_method)
{
	SpaceButs *sbuts= NULL; // XXX
	Render *re;
	RenderStats *rstats;
	Scene *sce;
	int oldx= ri->pr_rectx, oldy= ri->pr_recty;
	char name [32];
	
	if(ri->tottile && ri->curtile>=ri->tottile) return;
	
	/* check for return with a new event */
	if(pr_method!=PR_ICON_RENDER && qtest()) {
//		if(area)
//			addafterqueue(area->win, RENDERPREVIEW, 1);
		return;
	}
	
	/* get the stuff from the builtin preview dbase */
//	sce= preview_prepare_scene(scene, ri, GS(id->name), id, pr_method);
	if(sce==NULL) return;
	
	/* set drawing conditions OK */
	if(area) {
		sbuts= area->spacedata.first;	/* needed for flag */
		
		set_previewrect(area, ri); // uses UImat
		
		/* because preview render size can differs */
		if(ri->rect && (oldx!=ri->pr_rectx || oldy!=ri->pr_recty)) {
			MEM_freeN(ri->rect);
			ri->rect= NULL;
			ri->curtile= 0;
		}
	}
	
// XXX	sprintf(name, "ButsPreview %d", area?area->win:0);
	re= RE_GetRender(name);
	
	/* full refreshed render from first tile */
	if(re==NULL || ri->curtile==0) {
		
		re= RE_NewRender(name);
		
		/* handle cases */
		if(pr_method==PR_DRAW_RENDER) {
//			RE_display_draw_cb(re, previewrender_progress);
//			RE_test_break_cb(re, qtest);
			sce->r.scemode |= R_NODE_PREVIEW;
			if(sbuts->flag & SB_PRV_OSA)
				sce->r.mode |= R_OSA;
			sce->r.scemode &= ~R_NO_IMAGE_LOAD;
		}
		else if(pr_method==PR_DO_RENDER) {
//			RE_test_break_cb(re, qtest);
			sce->r.scemode |= R_NODE_PREVIEW;
			sce->r.scemode &= ~R_NO_IMAGE_LOAD;
		}
		else {	/* PR_ICON_RENDER */
			sce->r.scemode &= ~R_NODE_PREVIEW;
			sce->r.scemode |= R_NO_IMAGE_LOAD;
		}
		
		/* allocates render result */
		RE_InitState(re, NULL, &sce->r, ri->pr_rectx, ri->pr_recty, NULL);
		
		/* enforce preview image clear */
		if(GS(id->name)==ID_MA) {
			Material *ma= (Material *)id;
			ntreeClearPreview(ma->nodetree);
		}
	}
	/* entire cycle for render engine */
	RE_SetCamera(re, sce->camera);
	RE_Database_FromScene(re, sce, 1);
	RE_TileProcessor(re, ri->curtile, 0);	// actual render engine
	RE_Database_Free(re);
	
	/* handle results */
	if(pr_method==PR_ICON_RENDER) {
		if(ri->rect==NULL)
			ri->rect= MEM_mallocN(sizeof(int)*ri->pr_rectx*ri->pr_recty, "BIF_previewrender");
		RE_ResultGet32(re, ri->rect);
	}
	else {
		rstats= RE_GetStats(re);
		
		if(rstats->partsdone!=ri->curtile) {
			if(ri->rect==NULL)
				ri->rect= MEM_mallocN(sizeof(int)*ri->pr_rectx*ri->pr_recty, "BIF_previewrender");
			RE_ResultGet32(re, ri->rect);
		}
		
		if(rstats->totpart==rstats->partsdone && rstats->partsdone) {
			// allqueues
		}
		else {
//			if(pr_method==PR_DRAW_RENDER && qtest())
//				addafterqueue(area->win, RENDERPREVIEW, 1);
		}
		
		ri->curtile= rstats->partsdone;
		ri->tottile= rstats->totpart;
	}

	/* unassign the pointers, reset vars */
//	preview_prepare_scene(scene, ri, GS(id->name), NULL, 0);
	
}


/* afterqueue call */
void BIF_previewrender_buts(Scene *scene, SpaceButs *sbuts)
{
//	ScrArea *sa= NULL; // XXX
	ARegion *ar= NULL; // XXX
	uiBlock *block;
	struct ID* id = 0;
//	struct ID* idfrom = 0;
	struct ID* idshow = 0;
	Object *ob;
	
	if (!sbuts->ri) return;
	
	
//	block= uiFindOpenPanelBlockName(&sa->uiblocks, "Preview");
	if(block==NULL) return;
	
	ob= ((scene->basact)? (scene->basact)->object: 0);
	
	/* we cant trust this global lockpoin.. for example with headerless window */
//	buttons_active_id(&id, &idfrom);
	sbuts->lockpoin= id;
	
	if(sbuts->mainb==CONTEXT_SHADING) {
		int tab= TAB_SHADING_MAT; // XXX sbuts->tab[CONTEXT_SHADING];
		
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
//		uiPanelPush(block);
//		BIF_previewrender(scene, idshow, sbuts->ri, sbuts->area, PR_DRAW_RENDER);
//		uiPanelPop(block);
		end_previewrect(ar);
	}
	else {
		/* no active block to draw. But we do draw black if possible */
		if(sbuts->ri->rect) {
			memset(sbuts->ri->rect, 0, sizeof(int)*sbuts->ri->pr_rectx*sbuts->ri->pr_recty);
			sbuts->ri->tottile= 10000;
//			addqueue(sa->win, REDRAW, 1);
		}
		return;
	}
}

/* new UI convention: draw is in pixel space already. */
/* uses ROUNDBOX button in block to get the rect */
void ED_preview_draw(const bContext *C, void *idp, rcti *rect)
{
	ScrArea *sa= CTX_wm_area(C);
	SpaceButs *sbuts= sa->spacedata.first;
	RenderResult rres;
	int newx= rect->xmax-rect->xmin, newy= rect->ymax-rect->ymin;
	int ok= 0;
	char name[32];
	
	sprintf(name, "Preview %p", sa);
	BLI_lock_malloc_thread();
	RE_GetResultImage(RE_GetRender(name), &rres);

	if(rres.rectf) {
		if(rres.rectx==newx && rres.recty==newy) {
			glBlendFunc(GL_DST_ALPHA, GL_ONE_MINUS_DST_ALPHA);
			glEnable(GL_BLEND);
			
			glaDrawPixelsSafe(rect->xmin, rect->ymin, rres.rectx, rres.recty, rres.rectx, GL_RGBA, GL_FLOAT, rres.rectf);
			ok= 1;
			
			glDisable(GL_BLEND);
			glBlendFunc(GL_SRC_ALPHA, GL_ONE_MINUS_SRC_ALPHA);
		}
	}
	BLI_unlock_malloc_thread();

	/* check for spacetype... */
	if(sbuts->spacetype==SPACE_BUTS && sbuts->preview) {
		sbuts->preview= 0;
		ok= 0;
	}
	
	if(ok==0) {
		printf("added shader job\n");
		ED_preview_shader_job(C, sa, idp, newx, newy);
	}
	
}

/* *************************** Preview for 3d window *********************** */

void view3d_previewrender_progress(RenderResult *rr, volatile rcti *renrect)
{
//	ScrArea *sa= NULL; // XXX
//	View3D *v3d= NULL; // XXX
	RenderLayer *rl;
	int ofsx, ofsy;
	
	if(renrect) return;
	
	rl= rr->layers.first;
	
	/* this case is when we render envmaps... */
//	if(rr->rectx > v3d->ri->pr_rectx || rr->recty > v3d->ri->pr_recty)
//		return;
	
//	ofsx= v3d->ri->disprect.xmin + rr->tilerect.xmin;
//	ofsy= v3d->ri->disprect.ymin + rr->tilerect.ymin;
	
	glDrawBuffer(GL_FRONT);
//	glaDefine2DArea(&sa->winrct);
	glaDrawPixelsSafe_to32(ofsx, ofsy, rr->rectx, rr->recty, rr->rectx, rl->rectf);
	bglFlush();
	glDrawBuffer(GL_BACK);

}

void BIF_view3d_previewrender_signal(ScrArea *sa, short signal)
{
#if 0
	View3D *v3d= sa->spacedata.first;
	
	/* this can be called from other window... solve! */
	if(sa->spacetype!=SPACE_VIEW3D)
		return; // XXX
	   
	if(v3d && v3d->ri) {
		RenderInfo *ri= v3d->ri;
		ri->status &= ~signal;
		ri->curtile= 0;
		//printf("preview signal %d\n", signal);
		if(ri->re && (signal & PR_DBASE))
			RE_Database_Free(ri->re);

//		addafterqueue(sa->win, RENDERPREVIEW, 1);
	}
#endif
}

void BIF_view3d_previewrender_free(View3D *v3d)
{
#if 0
	if(v3d->ri) {
		RenderInfo *ri= v3d->ri;
		if(ri->re) {
//			printf("free render\n");
			RE_Database_Free(ri->re);
			RE_FreeRender(ri->re);
			ri->re= NULL;
		}
		if (v3d->ri->rect) MEM_freeN(v3d->ri->rect);
		MEM_freeN(v3d->ri);
		v3d->ri= NULL;
	}
#endif
}

/* returns 1 if OK, do not call while in panel space!  */
static int view3d_previewrender_get_rects(ScrArea *sa, rctf *viewplane, RenderInfo *ri, float *clipsta, float *clipend, int *ortho, float *pixsize)
{
	View3D *v3d= NULL; // XXX
	RegionView3D *rv3d= NULL; // XXX
	int rectx, recty;
//	uiBlock *block;
	
//	block= uiFindOpenPanelBlockName(&sa->uiblocks, "Preview");
//	if(block==NULL) return 0;
	
	/* calculate preview rect size */
//	BLI_init_rctf(viewplane, 15.0f, (block->maxx - block->minx)-15.0f, 15.0f, (block->maxy - block->miny)-15.0f);
//	uiPanelPush(block);
//	ui_graphics_to_window_rct(sa->win, viewplane, &ri->disprect);
//	uiPanelPop(block);
	
	/* correction for gla draw */
//	BLI_translate_rcti(&ri->disprect, -sa->winrct.xmin, -sa->winrct.ymin);
	
	*ortho= get_view3d_viewplane(v3d, rv3d, sa->winx, sa->winy, viewplane, clipsta, clipend, pixsize);

	rectx= ri->disprect.xmax - ri->disprect.xmin;
	recty= ri->disprect.ymax - ri->disprect.ymin;
	
	if(rectx<4 || recty<4) return 0;
	
	if(ri->rect && (rectx!=ri->pr_rectx || recty!=ri->pr_recty)) {
		MEM_freeN(ri->rect);
		ri->rect= NULL;
		ri->curtile= 0;
		printf("changed size\n");
	}
	ri->pr_rectx= rectx;
	ri->pr_recty= recty;
	
	return 1;
}

/* called before a panel gets moved/scaled, makes sure we can see through */
void BIF_view3d_previewrender_clear(ScrArea *sa)
{
#if 0
	View3D *v3d= sa->spacedata.first;

	if(v3d->ri) {
		RenderInfo *ri= v3d->ri;
		ri->curtile= 0;
		if(ri->rect)
			MEM_freeN(ri->rect);
		ri->rect= NULL;
	}
#endif
}

/* afterqueue call */
void BIF_view3d_previewrender(Scene *scene, ScrArea *sa)
{
	bContext *C= NULL;
	View3D *v3d= sa->spacedata.first;
	RegionView3D *rv3d= NULL; // XXX
	Render *re;
	RenderInfo *ri;	/* preview struct! */
	RenderStats *rstats;
	RenderData rdata;
	rctf viewplane;
	float clipsta, clipend, pixsize;
	int orth;
	
	/* first get the render info right */
//	if (!v3d->ri) {
//		ri= v3d->ri= MEM_callocN(sizeof(RenderInfo), "butsrenderinfo");
//		ri->tottile= 10000;
//	}
//	ri= v3d->ri;
	
	if(0==view3d_previewrender_get_rects(sa, &viewplane, ri, &clipsta, &clipend, &orth, &pixsize))
		return;
	
	/* render is finished, so return */
	if(ri->tottile && ri->curtile>=ri->tottile) return;

	/* or return with a new event */
	if(qtest()) {
//		addafterqueue(sa->win, RENDERPREVIEW, 1);
		return;
	}
	//printf("Enter previewrender\n");
	/* ok, are we rendering all over? */
	if(ri->re==NULL) {
		char name[32];
		
		ri->status= 0;
		
		sprintf(name, "View3dPreview %p", sa);
		re= ri->re= RE_NewRender(name);
		//RE_display_draw_cb(re, view3d_previewrender_progress);
		//RE_stats_draw_cb(re, view3d_previewrender_stats);
		//RE_test_break_cb(re, qtest);
		
		/* no osa, blur, seq, layers, etc for preview render */
		rdata= scene->r;
		rdata.mode &= ~(R_OSA|R_MBLUR);
		rdata.scemode &= ~(R_DOSEQ|R_DOCOMP|R_FREE_IMAGE);
		rdata.layers.first= rdata.layers.last= NULL;
		rdata.renderer= R_INTERN;
		 
		RE_InitState(re, NULL, &rdata, sa->winx, sa->winy, &ri->disprect);
	
		if(orth)
			RE_SetOrtho(re, &viewplane, clipsta, clipend);
		else
			RE_SetWindow(re, &viewplane, clipsta, clipend);
		RE_SetPixelSize(re, pixsize);
		
		/* until here are no escapes */
		ri->status |= PR_DISPRECT;
		ri->curtile= 0;
		//printf("new render\n");
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
			RE_SetPixelSize(re, pixsize);
			ri->status |= PR_DISPRECT;
			ri->curtile= 0;
			//printf("disprect update\n");
		}
		if((ri->status & PR_DBASE)==0) {
			unsigned int lay= scene->lay;
			
			RE_SetView(re, rv3d->viewmat);
			
			/* allow localview render for objects with lights in normal layers */
			if(v3d->lay & 0xFF000000)
				scene->lay |= v3d->lay;
			else scene->lay= v3d->lay;
			
			RE_Database_FromScene(re, scene, 0);		// 0= dont use camera view
			scene->lay= lay;
			
			rstats= RE_GetStats(re);
			if(rstats->convertdone) 
				ri->status |= PR_DBASE|PR_PROJECTED|PR_ROTATED;
			ri->curtile= 0;
			
			/* database can have created render-resol data... */
			if(rstats->convertdone) 
				ED_anim_dag_flush_update(C); // <--- only current scene XXX
			
			//printf("dbase update\n");
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
			ri->curtile= 0;
			//printf("project update\n");
		}
	
		/* OK, can we enter render code? */
		if(ri->status==(PR_DISPRECT|PR_DBASE|PR_PROJECTED|PR_ROTATED)) {
			//printf("curtile %d tottile %d\n", ri->curtile, ri->tottile);
			RE_TileProcessor(ri->re, ri->curtile, 0);
	
			if(ri->rect==NULL)
				ri->rect= MEM_mallocN(sizeof(int)*ri->pr_rectx*ri->pr_recty, "preview view3d rect");
			
			RE_ResultGet32(ri->re, ri->rect);
		}
		
		rstats= RE_GetStats(ri->re);
//		if(rstats->totpart==rstats->partsdone && rstats->partsdone)
//			addqueue(sa->win, REDRAW, 1);
//		else
//			addafterqueue(sa->win, RENDERPREVIEW, 1);
		
		ri->curtile= rstats->partsdone;
		ri->tottile= rstats->totpart;
	}
	else {
//		addafterqueue(sa->win, RENDERPREVIEW, 1);
	}
	
	//printf("\n");
}

/* in panel space! */
static void view3d_previewdraw_rect(ScrArea *sa, uiBlock *block, RenderInfo *ri)
{
//	rctf dispf;
	
	if(ri->rect==NULL)
		return;
	
//	BLI_init_rctf(&dispf, 15.0f, (block->maxx - block->minx)-15.0f, 15.0f, (block->maxy - block->miny)-15.0f);
//	ui_graphics_to_window_rct(sa->win, &dispf, &ri->disprect);

	/* correction for gla draw */
//	BLI_translate_rcti(&ri->disprect, -sa->winrct.xmin, -sa->winrct.ymin);
	
	/* when panel scale changed, free rect */
	if(ri->disprect.xmax-ri->disprect.xmin != ri->pr_rectx ||
	   ri->disprect.ymax-ri->disprect.ymin != ri->pr_recty) {
		MEM_freeN(ri->rect);
		ri->rect= NULL;
	}
	else {
//		glaDefine2DArea(&sa->winrct);
		glaDrawPixelsSafe(ri->disprect.xmin, ri->disprect.ymin, ri->pr_rectx, ri->pr_recty, ri->pr_rectx, GL_RGBA, GL_UNSIGNED_BYTE, ri->rect);
	}	
}

/* is panel callback, supposed to be called with correct panel offset matrix */
void BIF_view3d_previewdraw(struct ScrArea *sa, struct uiBlock *block)
{
	RegionView3D *rv3d= NULL;

//	if (v3d->ri==NULL || v3d->ri->rect==NULL) 
//		addafterqueue(sa->win, RENDERPREVIEW, 1);
//	else {
		view3d_previewdraw_rect(sa, block, rv3d->ri);
//		if(v3d->ri->curtile==0) 
//			addafterqueue(sa->win, RENDERPREVIEW, 1);
//	}
}


/* **************************** New preview system ****************** */

/* inside thread, called by renderer, sets job update value */
static void shader_preview_draw(void *spv, RenderResult *rr, volatile struct rcti *rect)
{
	ShaderPreview *sp= spv;
	
	*(sp->do_update)= 1;
}

/* called by renderer, checks job value */
static int shader_preview_break(void *spv)
{
	ShaderPreview *sp= spv;
	
	return *(sp->stop);
}

/* outside thread, called before redraw notifiers, it moves finished preview over */
static void shader_preview_updatejob(void *spv)
{
//	ShaderPreview *sp= spv;
	
}

/* runs inside thread */
static void shader_preview_startjob(void *customdata, short *stop, short *do_update)
{
	ShaderPreview *sp= customdata;
	Render *re;
	Scene *sce;
	float oldlens;
	char name [32];

	sp->stop= stop;
	sp->do_update= do_update;
	
	/* get the stuff from the builtin preview dbase */
	sce= preview_prepare_scene(sp->scene, GS(sp->id->name), sp);
	if(sce==NULL) return;
	
	sprintf(name, "Preview %p", sp->owner);
	re= RE_GetRender(name);
	
	/* full refreshed render from first tile */
	if(re==NULL)
		re= RE_NewRender(name);
		
	/* sce->r gets copied in RE_InitState! */
	if(sp->pr_method==PR_DO_RENDER) {
		sce->r.scemode |= R_NODE_PREVIEW;
		sce->r.scemode &= ~R_NO_IMAGE_LOAD;
		sce->r.mode |= R_OSA;
	}
	else {	/* PR_ICON_RENDER */
		sce->r.scemode &= ~R_NODE_PREVIEW;
		sce->r.scemode |= R_NO_IMAGE_LOAD;
	}

	/* allocates or re-uses render result */
	RE_InitState(re, NULL, &sce->r, sp->sizex, sp->sizey, NULL);

	/* callbacs are cleared on GetRender() */
	if(sp->pr_method==PR_DO_RENDER) {
		RE_display_draw_cb(re, sp, shader_preview_draw);
		RE_test_break_cb(re, sp, shader_preview_break);
	}
	/* lens adjust */
	oldlens= ((Camera *)sce->camera->data)->lens;
	((Camera *)sce->camera->data)->lens *= (float)sp->sizey/(float)sp->sizex;

	/* entire cycle for render engine */
	RE_SetCamera(re, sce->camera);
	RE_Database_FromScene(re, sce, 1);
	RE_TileProcessor(re, 0, 1);	// actual render engine
	RE_Database_Free(re);

	((Camera *)sce->camera->data)->lens= oldlens;
	*do_update= 1;

	/* handle results */
	if(sp->pr_method==PR_ICON_RENDER) {
		//if(ri->rect==NULL)
		//	ri->rect= MEM_mallocN(sizeof(int)*ri->pr_rectx*ri->pr_recty, "BIF_previewrender");
		//RE_ResultGet32(re, ri->rect);
	}
	else {
		/* validate owner */
		//if(ri->rect==NULL)
		//	ri->rect= MEM_mallocN(sizeof(int)*ri->pr_rectx*ri->pr_recty, "BIF_previewrender");
		//RE_ResultGet32(re, ri->rect);
	}

	/* unassign the pointers, reset vars */
	preview_prepare_scene(sp->scene, GS(sp->id->name), NULL);

}

static void shader_preview_free(void *customdata)
{
	ShaderPreview *sp= customdata;
	
	MEM_freeN(sp);
}

void ED_preview_shader_job(const bContext *C, void *owner, ID *id, int sizex, int sizey)
{
	wmJob *steve= WM_jobs_get(CTX_wm_manager(C), CTX_wm_window(C), owner);
	ShaderPreview *sp= MEM_callocN(sizeof(ShaderPreview), "shader preview");

	/* customdata for preview thread */
	sp->scene= CTX_data_scene(C);
	sp->owner= owner;
	sp->sizex= sizex;
	sp->sizey= sizey;
	sp->pr_method= PR_DO_RENDER;
	sp->id = id;
	
	/* setup job */
	WM_jobs_customdata(steve, sp, shader_preview_free);
	WM_jobs_timer(steve, 0.1, NC_MATERIAL, NC_MATERIAL);
	WM_jobs_callbacks(steve, shader_preview_startjob, NULL, shader_preview_updatejob);
	
	WM_jobs_start(CTX_wm_manager(C), steve);
}


