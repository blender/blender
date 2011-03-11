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
 * Inc., 51 Franklin Street, Fifth Floor, Boston, MA 02110-1301, USA.
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

/** \file blender/editors/render/render_preview.c
 *  \ingroup edrend
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

#include "BLI_math.h"
#include "BLI_blenlib.h"
#include "BLI_threads.h"
#include "BLI_utildefines.h"

#include "DNA_world_types.h"
#include "DNA_camera_types.h"
#include "DNA_material_types.h"
#include "DNA_node_types.h"
#include "DNA_object_types.h"
#include "DNA_lamp_types.h"
#include "DNA_space_types.h"
#include "DNA_view3d_types.h"
#include "DNA_scene_types.h"
#include "DNA_brush_types.h"
#include "DNA_screen_types.h"

#include "BKE_context.h"
#include "BKE_depsgraph.h"
#include "BKE_global.h"
#include "BKE_idprop.h"
#include "BKE_image.h"
#include "BKE_icons.h"
#include "BKE_library.h"
#include "BKE_main.h"
#include "BKE_material.h"
#include "BKE_node.h"
#include "BKE_texture.h"

#include "IMB_imbuf.h"
#include "IMB_imbuf_types.h"

#include "BIF_gl.h"
#include "BIF_glutil.h"

#include "PIL_time.h"

#include "RE_pipeline.h"


#include "WM_api.h"
#include "WM_types.h"

#include "ED_render.h"
#include "ED_view3d.h"

#include "UI_interface.h"

#include "render_intern.h"

#define PR_XMIN		10
#define PR_YMIN		5
#define PR_XMAX		200
#define PR_YMAX		195

/* XXX */
static int qtest(void) {return 0;}
/* XXX */

ImBuf* get_brush_icon(Brush *brush)
{
	static const int flags = IB_rect|IB_multilayer|IB_metadata;

	char path[240];
	char *folder;

	if (!(brush->icon_imbuf)) {
		if (brush->flag & BRUSH_CUSTOM_ICON) {

			if (brush->icon_filepath[0]) {
				// first use the path directly to try and load the file

				BLI_strncpy(path, brush->icon_filepath, sizeof(brush->icon_filepath));
				BLI_path_abs(path, G.main->name);

				brush->icon_imbuf= IMB_loadiffname(path, flags);

				// otherwise lets try to find it in other directories
				if (!(brush->icon_imbuf)) {
					folder= BLI_get_folder(BLENDER_DATAFILES, "brushicons");

					path[0]= 0;

					BLI_make_file_string(G.main->name, path, folder, brush->icon_filepath);

					if (path[0])
						brush->icon_imbuf= IMB_loadiffname(path, flags);
				}
			}
		}
	}

	if (!(brush->icon_imbuf))
		brush->id.icon_id = 0;
	else
		BKE_icon_changed(BKE_icon_getid(&(brush->id)));

	return brush->icon_imbuf;
}

typedef struct ShaderPreview {
	/* from wmJob */
	void *owner;
	short *stop, *do_update;
	
	Scene *scene;
	ID *id;
	ID *parent;
	MTex *slot;
	
	/* node materials/texture need full copy during preview render, glsl uses it too */
	Material *matcopy;
	Tex *texcopy;
	
	float col[4];		/* active object color */
	
	int sizex, sizey;
	unsigned int *pr_rect;
	int pr_method;
	
} ShaderPreview;



/* unused now */
void draw_tex_crop(Tex *tex)
{
	rcti rct;
	int ret= 0;
	
	if(tex==NULL) return;
	
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
void BIF_preview_changed(short UNUSED(id_code))
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

			for(ma=G.main->mat.first; ma; ma=ma->id.next) {
				if(ma->gpumaterial.first) {
					GPU_material_free(ma);
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
	const int fileflags= G.fileflags;
	
	G.fileflags |= G_FILE_NO_UI;
	bfd= BLO_read_from_memory(datatoc_preview_blend, datatoc_preview_blend_size, NULL);
	if (bfd) {
		pr_main= bfd->main;
		
		MEM_freeN(bfd);
	}
	G.fileflags= fileflags;
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

static int preview_mat_has_sss(Material *mat, bNodeTree *ntree)
{
	if(mat) {
		if(mat->sss_flag & MA_DIFF_SSS)
			return 1;
		if(mat->nodetree)
			if( preview_mat_has_sss(NULL, mat->nodetree))
				return 1;
	}
	else if(ntree) {
		bNode *node;
		for(node= ntree->nodes.first; node; node= node->next) {
			if(node->type==NODE_GROUP && node->id) {
				if( preview_mat_has_sss(NULL, (bNodeTree *)node->id))
					return 1;
			}
			else if(node->id && ELEM(node->type, SH_NODE_MATERIAL, SH_NODE_MATERIAL_EXT)) {
				mat= (Material *)node->id;
				if(mat->sss_flag & MA_DIFF_SSS)
					return 1;
			}
		}
	}
	return 0;
}

/* call this with a pointer to initialize preview scene */
/* call this with NULL to restore assigned ID pointers in preview scene */
static Scene *preview_prepare_scene(Scene *scene, ID *id, int id_type, ShaderPreview *sp)
{
	Scene *sce;
	Base *base;
	
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
		
		sce->r.color_mgt_flag = scene->r.color_mgt_flag;
		
		/* prevent overhead for small renders and icons (32) */
		if(id && sp->sizex < 40)
			sce->r.xparts= sce->r.yparts= 1;
		else
			sce->r.xparts= sce->r.yparts= 4;
		
		/* exception: don't color manage texture previews or icons */
		if((id && sp->pr_method==PR_ICON_RENDER) || id_type == ID_TE)
			sce->r.color_mgt_flag &= ~R_COLOR_MANAGEMENT;
		
		if((id && sp->pr_method==PR_ICON_RENDER) && id_type != ID_WO)
			sce->r.alphamode= R_ALPHAPREMUL;
		else
			sce->r.alphamode= R_ADDSKY;

		sce->r.cfra= scene->r.cfra;
		strcpy(sce->r.engine, scene->r.engine);
		
		if(id_type==ID_MA) {
			Material *mat= NULL, *origmat= (Material *)id;
			
			if(id) {
				/* work on a copy */
				mat= localize_material(origmat);
				sp->matcopy= mat;
				BLI_addtail(&pr_main->mat, mat);
				
				init_render_material(mat, 0, NULL);		/* call that retrieves mode_l */
				end_render_material(mat);
				
				/* un-useful option */
				if(sp->pr_method==PR_ICON_RENDER)
					mat->shade_flag &= ~MA_OBCOLOR;

				/* turn on raytracing if needed */
				if(mat->mode_l & MA_RAYMIRROR)
					sce->r.mode |= R_RAYTRACE;
				if(mat->material_type == MA_TYPE_VOLUME)
					sce->r.mode |= R_RAYTRACE;
				if((mat->mode_l & MA_RAYTRANSP) && (mat->mode_l & MA_TRANSP))
					sce->r.mode |= R_RAYTRACE;
				if(preview_mat_has_sss(mat, NULL))
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
				
				/* turn off bounce lights for volume, 
				 * doesn't make much visual difference and slows it down too */
				if(mat->material_type == MA_TYPE_VOLUME) {
					for(base= sce->base.first; base; base= base->next) {
						if(base->object->type == OB_LAMP) {
							/* if doesn't match 'Lamp.002' --> main key light */
							if( strcmp(base->object->id.name+2, "Lamp.002") != 0 ) {
								base->object->restrictflag |= OB_RESTRICT_RENDER;
							}
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
					if(mat->nodetree && sp->pr_method==PR_NODE_RENDER) {
						/* two previews, they get copied by wmJob */
						ntreeInitPreview(mat->nodetree, sp->sizex, sp->sizey);
						ntreeInitPreview(origmat->nodetree, sp->sizex, sp->sizey);
					}
				}
			}
			else {
				sce->r.mode &= ~(R_OSA|R_RAYTRACE|R_SSS);
				
			}
			
			for(base= sce->base.first; base; base= base->next) {
				if(base->object->id.name[2]=='p') {
					/* copy over object color, in case material uses it */
					copy_v4_v4(base->object->col, sp->col);
					
					if(ELEM4(base->object->type, OB_MESH, OB_CURVE, OB_SURF, OB_MBALL)) {
						/* don't use assign_material, it changed mat->id.us, which shows in the UI */
						Material ***matar= give_matarar(base->object);
						int actcol= MAX2(base->object->actcol > 0, 1) - 1;

						if(matar && actcol < base->object->totcol)
							(*matar)[actcol]= mat;
					} else if (base->object->type == OB_LAMP) {
						base->object->restrictflag &= ~OB_RESTRICT_RENDER;
					}
				}
			}
		}
		else if(id_type==ID_TE) {
			Tex *tex= NULL, *origtex= (Tex *)id;
			
			if(origtex) {
				tex= localize_texture(origtex);
				sp->texcopy= tex;
				BLI_addtail(&pr_main->tex, tex);
			}			
			sce->lay= 1<<MA_TEXTURE;
			
			for(base= sce->base.first; base; base= base->next) {
				if(base->object->id.name[2]=='t') {
					Material *mat= give_current_material(base->object, base->object->actcol);
					if(mat && mat->mtex[0]) {
						mat->mtex[0]->tex= tex;
						
						if(tex && sp->slot)
							mat->mtex[0]->which_output = sp->slot->which_output;
						
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

			if(tex && tex->nodetree && sp->pr_method==PR_NODE_RENDER) {
				/* two previews, they get copied by wmJob */
				ntreeInitPreview(origtex->nodetree, sp->sizex, sp->sizey);
				ntreeInitPreview(tex->nodetree, sp->sizex, sp->sizey);
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

/* new UI convention: draw is in pixel space already. */
/* uses ROUNDBOX button in block to get the rect */
static int ed_preview_draw_rect(ScrArea *sa, Scene *sce, ID *id, int split, int first, rcti *rect, rcti *newrect)
{
	Render *re;
	RenderResult rres;
	char name[32];
	int do_gamma_correct=0;
	int offx=0, newx= rect->xmax-rect->xmin, newy= rect->ymax-rect->ymin;

	if (id && GS(id->name) != ID_TE) {
		/* exception: don't color manage texture previews - show the raw values */
		if (sce) do_gamma_correct = sce->r.color_mgt_flag & R_COLOR_MANAGEMENT;
	}

	if(!split || first) sprintf(name, "Preview %p", (void *)sa);
	else sprintf(name, "SecondPreview %p", (void *)sa);

	if(split) {
		if(first) {
			offx= 0;
			newx= newx/2;
		}
		else {
			offx= newx/2;
			newx= newx - newx/2;
		}
	}

	re= RE_GetRender(name);
	RE_AcquireResultImage(re, &rres);

	if(rres.rectf) {
		
		if(ABS(rres.rectx-newx)<2 && ABS(rres.recty-newy)<2) {
			newrect->xmax= MAX2(newrect->xmax, rect->xmin + rres.rectx + offx);
			newrect->ymax= MAX2(newrect->ymax, rect->ymin + rres.recty);

			glaDrawPixelsSafe_to32(rect->xmin+offx, rect->ymin, rres.rectx, rres.recty, rres.rectx, rres.rectf, do_gamma_correct);

			RE_ReleaseResultImage(re);
			return 1;
		}
	}

	RE_ReleaseResultImage(re);
	return 0;
}

void ED_preview_draw(const bContext *C, void *idp, void *parentp, void *slotp, rcti *rect)
{
	if(idp) {
		ScrArea *sa= CTX_wm_area(C);
		Scene *sce = CTX_data_scene(C);
		ID *id = (ID *)idp;
		ID *parent= (ID *)parentp;
		MTex *slot= (MTex *)slotp;
		SpaceButs *sbuts= sa->spacedata.first;
		rcti newrect;
		int ok;
		int newx= rect->xmax-rect->xmin, newy= rect->ymax-rect->ymin;

		newrect.xmin= rect->xmin;
		newrect.xmax= rect->xmin;
		newrect.ymin= rect->ymin;
		newrect.ymax= rect->ymin;

		if(parent) {
			ok = ed_preview_draw_rect(sa, sce, id, 1, 1, rect, &newrect);
			ok &= ed_preview_draw_rect(sa, sce, parent, 1, 0, rect, &newrect);
		}
		else
			ok = ed_preview_draw_rect(sa, sce, id, 0, 0, rect, &newrect);

		if(ok)
			*rect= newrect;

		/* check for spacetype... */
		if(sbuts->spacetype==SPACE_BUTS && sbuts->preview) {
			sbuts->preview= 0;
			ok= 0;
		}
	
		if(ok==0) {
			ED_preview_shader_job(C, sa, id, parent, slot, newx, newy, PR_BUTS_RENDER);
		}
	}	
}

/* ******************************** Icon Preview **************************** */

void ED_preview_icon_draw(const bContext *UNUSED(C), void *UNUSED(idp), void *UNUSED(arg1), void *UNUSED(arg2), rcti *UNUSED(rect))
{
}

/* *************************** Preview for 3d window *********************** */

void view3d_previewrender_progress(RenderResult *rr, volatile rcti *renrect)
{
//	ScrArea *sa= NULL; // XXX
//	View3D *v3d= NULL; // XXX
	RenderLayer *rl;
	int ofsx=0, ofsy=0;
	
	if(renrect) return;
	
	rl= rr->layers.first;
	
	/* this case is when we render envmaps... */
//	if(rr->rectx > v3d->ri->pr_rectx || rr->recty > v3d->ri->pr_recty)
//		return;
	
//	ofsx= v3d->ri->disprect.xmin + rr->tilerect.xmin;
//	ofsy= v3d->ri->disprect.ymin + rr->tilerect.ymin;
	
	glDrawBuffer(GL_FRONT);
//	glaDefine2DArea(&sa->winrct);
	glaDrawPixelsSafe_to32(ofsx, ofsy, rr->rectx, rr->recty, rr->rectx, rl->rectf, 0);
	bglFlush();
	glDrawBuffer(GL_BACK);

}

void BIF_view3d_previewrender_signal(ScrArea *UNUSED(sa), short UNUSED(signal))
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

void BIF_view3d_previewrender_free(View3D *UNUSED(v3d))
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
void BIF_view3d_previewrender_clear(ScrArea *UNUSED(sa))
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
void BIF_view3d_previewrender(Main *bmain, Scene *scene, ScrArea *sa)
{
	View3D *v3d= sa->spacedata.first;
	RegionView3D *rv3d= NULL; // XXX
	Render *re;
	RenderInfo *ri=NULL;	/* preview struct! */
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
		
		sprintf(name, "View3dPreview %p", (void *)sa);
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
		 
		RE_InitState(re, NULL, &rdata, NULL, sa->winx, sa->winy, &ri->disprect);
	
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
				lay |= v3d->lay;
			else lay= v3d->lay;
			
			RE_Database_FromScene(re, bmain, scene, lay, 0);		// 0= dont use camera view
			
			rstats= RE_GetStats(re);
			if(rstats->convertdone) 
				ri->status |= PR_DBASE|PR_PROJECTED|PR_ROTATED;
			ri->curtile= 0;
			
			/* database can have created render-resol data... */
			if(rstats->convertdone) 
				DAG_scene_flush_update(bmain, scene, scene->lay, 0);
			
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
			RE_TileProcessor(ri->re); //, ri->curtile, 0);
	
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
static void view3d_previewdraw_rect(ScrArea *UNUSED(sa), uiBlock *UNUSED(block), RenderInfo *ri)
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


/* **************************** new shader preview system ****************** */

/* inside thread, called by renderer, sets job update value */
static void shader_preview_draw(void *spv, RenderResult *UNUSED(rr), volatile struct rcti *UNUSED(rect))
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
	ShaderPreview *sp= spv;
	
	if(sp->id) {
		if(sp->pr_method==PR_NODE_RENDER) {
			if( GS(sp->id->name) == ID_MA) {
				Material *mat= (Material *)sp->id;
				
				if(sp->matcopy && mat->nodetree && sp->matcopy->nodetree)
					ntreeLocalSync(sp->matcopy->nodetree, mat->nodetree);
			}
			else if( GS(sp->id->name) == ID_TE) {
				Tex *tex= (Tex *)sp->id;
				
				if(sp->texcopy && tex->nodetree && sp->texcopy->nodetree)
					ntreeLocalSync(sp->texcopy->nodetree, tex->nodetree);
			}
		}		
	}
}

static void shader_preview_render(ShaderPreview *sp, ID *id, int split, int first)
{
	Render *re;
	Scene *sce;
	float oldlens;
	short idtype= GS(id->name);
	char name[32];
	int sizex;
	
	/* get the stuff from the builtin preview dbase */
	sce= preview_prepare_scene(sp->scene, id, idtype, sp); // XXX sizex
	if(sce==NULL) return;
	
	if(!split || first) sprintf(name, "Preview %p", sp->owner);
	else sprintf(name, "SecondPreview %p", sp->owner);
	re= RE_GetRender(name);
	
	/* full refreshed render from first tile */
	if(re==NULL)
		re= RE_NewRender(name);
		
	/* sce->r gets copied in RE_InitState! */
	sce->r.scemode &= ~(R_MATNODE_PREVIEW|R_TEXNODE_PREVIEW);
	sce->r.scemode &= ~R_NO_IMAGE_LOAD;

	if(sp->pr_method==PR_ICON_RENDER) {
		sce->r.scemode |= R_NO_IMAGE_LOAD;
		sce->r.mode |= R_OSA;
	}
	else if(sp->pr_method==PR_NODE_RENDER) {
		if(idtype == ID_MA) sce->r.scemode |= R_MATNODE_PREVIEW;
		else if(idtype == ID_TE) sce->r.scemode |= R_TEXNODE_PREVIEW;
		sce->r.mode &= ~R_OSA;
	}
	else {	/* PR_BUTS_RENDER */
		sce->r.mode |= R_OSA;
	}

	/* in case of split preview, use border render */
	if(split) {
		if(first) sizex= sp->sizex/2;
		else sizex= sp->sizex - sp->sizex/2;
	}
	else sizex= sp->sizex;

	/* allocates or re-uses render result */
	sce->r.xsch= sizex;
	sce->r.ysch= sp->sizey;
	sce->r.size= 100;

	/* callbacs are cleared on GetRender() */
	if(ELEM(sp->pr_method, PR_BUTS_RENDER, PR_NODE_RENDER)) {
		RE_display_draw_cb(re, sp, shader_preview_draw);
	}
	/* set this for all previews, default is react to G.afbreek still */
	RE_test_break_cb(re, sp, shader_preview_break);
	
	/* lens adjust */
	oldlens= ((Camera *)sce->camera->data)->lens;
	if(sizex > sp->sizey)
		((Camera *)sce->camera->data)->lens *= (float)sp->sizey/(float)sizex;

	/* entire cycle for render engine */
	RE_PreviewRender(re, pr_main, sce);

	((Camera *)sce->camera->data)->lens= oldlens;

	/* handle results */
	if(sp->pr_method==PR_ICON_RENDER) {
		// char *rct= (char *)(sp->pr_rect + 32*16 + 16);
		
		if(sp->pr_rect)
			RE_ResultGet32(re, sp->pr_rect);
	}
	else {
		/* validate owner */
		//if(ri->rect==NULL)
		//	ri->rect= MEM_mallocN(sizeof(int)*ri->pr_rectx*ri->pr_recty, "BIF_previewrender");
		//RE_ResultGet32(re, ri->rect);
	}

	/* unassign the pointers, reset vars */
	preview_prepare_scene(sp->scene, NULL, GS(id->name), sp);
	
	/* XXX bad exception, end-exec is not being called in render, because it uses local main */
//	if(idtype == ID_TE) {
//		Tex *tex= (Tex *)id;
//		if(tex->use_nodes && tex->nodetree)
//			ntreeEndExecTree(tex->nodetree);
//	}

}

/* runs inside thread for material and icons */
static void shader_preview_startjob(void *customdata, short *stop, short *do_update)
{
	ShaderPreview *sp= customdata;

	sp->stop= stop;
	sp->do_update= do_update;

	if(sp->parent) {
		shader_preview_render(sp, sp->id, 1, 1);
		shader_preview_render(sp, sp->parent, 1, 0);
	}
	else
		shader_preview_render(sp, sp->id, 0, 0);

	*do_update= 1;
}

static void shader_preview_free(void *customdata)
{
	ShaderPreview *sp= customdata;
	
	if(sp->matcopy) {
		struct IDProperty *properties;
		/* node previews */
		shader_preview_updatejob(sp);
		
		/* get rid of copied material */
		BLI_remlink(&pr_main->mat, sp->matcopy);
		free_material(sp->matcopy);

		properties= IDP_GetProperties((ID *)sp->matcopy, FALSE);
		if (properties) {
			IDP_FreeProperty(properties);
			MEM_freeN(properties);
		}
		MEM_freeN(sp->matcopy);
	}
	if(sp->texcopy) {
		struct IDProperty *properties;
		/* node previews */
		shader_preview_updatejob(sp);
		
		/* get rid of copied texture */
		BLI_remlink(&pr_main->tex, sp->texcopy);
		free_texture(sp->texcopy);
		
		properties= IDP_GetProperties((ID *)sp->texcopy, FALSE);
		if (properties) {
			IDP_FreeProperty(properties);
			MEM_freeN(properties);
		}
		MEM_freeN(sp->texcopy);
	}
	
	MEM_freeN(sp);
}

/* ************************* icon preview ********************** */

static void icon_copy_rect(ImBuf *ibuf, unsigned int w, unsigned int h, unsigned int *rect)
{
	struct ImBuf *ima;
	unsigned int *drect, *srect;
	float scaledx, scaledy;
	short ex, ey, dx, dy;

	/* paranoia test */
	if(ibuf==NULL || (ibuf->rect==NULL && ibuf->rect_float==NULL))
		return;
	
	/* waste of cpu cyles... but the imbuf API has no other way to scale fast (ton) */
	ima = IMB_dupImBuf(ibuf);
	
	if (!ima) 
		return;
	
	if (ima->x > ima->y) {
		scaledx = (float)w;
		scaledy =  ( (float)ima->y/(float)ima->x )*(float)w;
	}
	else {			
		scaledx =  ( (float)ima->x/(float)ima->y )*(float)h;
		scaledy = (float)h;
	}
	
	ex = (short)scaledx;
	ey = (short)scaledy;
	
	dx = (w - ex) / 2;
	dy = (h - ey) / 2;
	
	IMB_scalefastImBuf(ima, ex, ey);
	
	/* if needed, convert to 32 bits */
	if(ima->rect==NULL)
		IMB_rect_from_float(ima);

	srect = ima->rect;
	drect = rect;

	drect+= dy*w+dx;
	for (;ey > 0; ey--){		
		memcpy(drect,srect, ex * sizeof(int));
		drect += w;
		srect += ima->x;
	}

	IMB_freeImBuf(ima);
}

static void set_alpha(char *cp, int sizex, int sizey, char alpha) 
{
	int a, size= sizex*sizey;

	for(a=0; a<size; a++, cp+=4)
		cp[3]= alpha;
}

static void icon_preview_startjob(void *customdata, short *stop, short *do_update)
{
	ShaderPreview *sp= customdata;
	ID *id= sp->id;
	short idtype= GS(id->name);

	if(idtype == ID_IM) {
		Image *ima= (Image*)id;
		ImBuf *ibuf= NULL;
		ImageUser iuser= {NULL};

		/* ima->ok is zero when Image cannot load */
		if(ima==NULL || ima->ok==0)
			return;

		/* setup dummy image user */
		iuser.ok= iuser.framenr= 1;
		iuser.scene= sp->scene;
		
		/* elubie: this needs to be changed: here image is always loaded if not
		   already there. Very expensive for large images. Need to find a way to 
		   only get existing ibuf */
		ibuf = BKE_image_get_ibuf(ima, &iuser);
		if(ibuf==NULL || ibuf->rect==NULL)
			return;
		
		icon_copy_rect(ibuf, sp->sizex, sp->sizey, sp->pr_rect);

		*do_update= 1;
	}
	else if(idtype == ID_BR) {
		Brush *br= (Brush*)id;

		br->icon_imbuf= get_brush_icon(br);

		if(!(br->icon_imbuf) || !(br->icon_imbuf->rect))
			return;

		memset(sp->pr_rect, 0x888888, sp->sizex*sp->sizey*sizeof(unsigned int));
		icon_copy_rect(br->icon_imbuf, sp->sizex, sp->sizey, sp->pr_rect);

		*do_update= 1;
	}
	else {
		/* re-use shader job */
		shader_preview_startjob(customdata, stop, do_update);

		/* world is rendered with alpha=0, so it wasn't displayed 
		   this could be render option for sky to, for later */
		if(idtype == ID_WO) {
			set_alpha((char*)sp->pr_rect, sp->sizex, sp->sizey, 255);
		}
		else if(idtype == ID_MA) {
			Material* ma = (Material*)id;

			if(ma->material_type == MA_TYPE_HALO)
				set_alpha((char*)sp->pr_rect, sp->sizex, sp->sizey, 255);
		}
	}
}

/* use same function for icon & shader, so the job manager
   does not run two of them at the same time. */

static void common_preview_startjob(void *customdata, short *stop, short *do_update, float *UNUSED(progress))
{
	ShaderPreview *sp= customdata;

	if(sp->pr_method == PR_ICON_RENDER)
		icon_preview_startjob(customdata, stop, do_update);
	else
		shader_preview_startjob(customdata, stop, do_update);
}

static void common_preview_endjob(void *customdata)
{
	ShaderPreview *sp= customdata;

	if(sp->id && GS(sp->id->name) == ID_BR)
		WM_main_add_notifier(NC_BRUSH|NA_EDITED, sp->id);
}

/* exported functions */

void ED_preview_icon_job(const bContext *C, void *owner, ID *id, unsigned int *rect, int sizex, int sizey)
{
	wmJob *steve;
	ShaderPreview *sp;
	
	/* suspended start means it starts after 1 timer step, see WM_jobs_timer below */
	steve= WM_jobs_get(CTX_wm_manager(C), CTX_wm_window(C), owner, "Icon Preview", WM_JOB_EXCL_RENDER|WM_JOB_SUSPEND);
	sp= MEM_callocN(sizeof(ShaderPreview), "shader preview");

	/* customdata for preview thread */
	sp->scene= CTX_data_scene(C);
	sp->owner= id;
	sp->sizex= sizex;
	sp->sizey= sizey;
	sp->pr_method= PR_ICON_RENDER;
	sp->pr_rect= rect;
	sp->id = id;

	/* setup job */
	WM_jobs_customdata(steve, sp, shader_preview_free);
	WM_jobs_timer(steve, 0.25, NC_MATERIAL, NC_MATERIAL);
	WM_jobs_callbacks(steve, common_preview_startjob, NULL, NULL, common_preview_endjob);

	WM_jobs_start(CTX_wm_manager(C), steve);
}

void ED_preview_shader_job(const bContext *C, void *owner, ID *id, ID *parent, MTex *slot, int sizex, int sizey, int method)
{
	Object *ob= CTX_data_active_object(C);
	wmJob *steve;
	ShaderPreview *sp;

	steve= WM_jobs_get(CTX_wm_manager(C), CTX_wm_window(C), owner, "Shader Preview", WM_JOB_EXCL_RENDER);
	sp= MEM_callocN(sizeof(ShaderPreview), "shader preview");

	/* customdata for preview thread */
	sp->scene= CTX_data_scene(C);
	sp->owner= owner;
	sp->sizex= sizex;
	sp->sizey= sizey;
	sp->pr_method= method;
	sp->id = id;
	sp->parent= parent;
	sp->slot= slot;
	if(ob && ob->totcol) copy_v4_v4(sp->col, ob->col);
	else sp->col[0]= sp->col[1]= sp->col[2]= sp->col[3]= 1.0f;
	
	/* setup job */
	WM_jobs_customdata(steve, sp, shader_preview_free);
	WM_jobs_timer(steve, 0.1, NC_MATERIAL, NC_MATERIAL);
	WM_jobs_callbacks(steve, common_preview_startjob, NULL, shader_preview_updatejob, NULL);
	
	WM_jobs_start(CTX_wm_manager(C), steve);
}

void ED_preview_kill_jobs(const struct bContext *C)
{
	wmWindowManager *wm= CTX_wm_manager(C);
	if(wm)
		WM_jobs_kill(wm, NULL, common_preview_startjob);
}

