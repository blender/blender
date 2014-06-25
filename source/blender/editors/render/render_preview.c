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

#include "BLI_math.h"
#include "BLI_blenlib.h"
#include "BLI_utildefines.h"

#include "BLO_readfile.h"

#include "DNA_world_types.h"
#include "DNA_camera_types.h"
#include "DNA_material_types.h"
#include "DNA_node_types.h"
#include "DNA_object_types.h"
#include "DNA_lamp_types.h"
#include "DNA_space_types.h"
#include "DNA_scene_types.h"
#include "DNA_brush_types.h"
#include "DNA_screen_types.h"

#include "BKE_brush.h"
#include "BKE_context.h"
#include "BKE_colortools.h"
#include "BKE_global.h"
#include "BKE_idprop.h"
#include "BKE_image.h"
#include "BKE_icons.h"
#include "BKE_lamp.h"
#include "BKE_library.h"
#include "BKE_main.h"
#include "BKE_material.h"
#include "BKE_node.h"
#include "BKE_scene.h"
#include "BKE_texture.h"
#include "BKE_world.h"

#include "IMB_imbuf.h"
#include "IMB_imbuf_types.h"
#include "IMB_colormanagement.h"

#include "GPU_extensions.h"

#include "BIF_gl.h"
#include "BIF_glutil.h"

#include "PIL_time.h"

#include "RE_pipeline.h"

#include "WM_api.h"
#include "WM_types.h"

#include "ED_datafiles.h"
#include "ED_render.h"

#include "UI_interface.h"

#include "render_intern.h"

ImBuf *get_brush_icon(Brush *brush)
{
	static const int flags = IB_rect | IB_multilayer | IB_metadata;

	char path[FILE_MAX];
	const char *folder;

	if (!(brush->icon_imbuf)) {
		if (brush->flag & BRUSH_CUSTOM_ICON) {

			if (brush->icon_filepath[0]) {
				// first use the path directly to try and load the file

				BLI_strncpy(path, brush->icon_filepath, sizeof(brush->icon_filepath));
				BLI_path_abs(path, G.main->name);

				/* use default colorspaces for brushes */
				brush->icon_imbuf = IMB_loadiffname(path, flags, NULL);

				// otherwise lets try to find it in other directories
				if (!(brush->icon_imbuf)) {
					folder = BLI_get_folder(BLENDER_DATAFILES, "brushicons");

					BLI_make_file_string(G.main->name, path, folder, brush->icon_filepath);

					if (path[0]) {
						/* use fefault color spaces */
						brush->icon_imbuf = IMB_loadiffname(path, flags, NULL);
					}
				}

				if (brush->icon_imbuf)
					BKE_icon_changed(BKE_icon_getid(&brush->id));
			}
		}
	}

	if (!(brush->icon_imbuf))
		brush->id.icon_id = 0;

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
	
	/* datablocks with nodes need full copy during preview render, glsl uses it too */
	Material *matcopy;
	Tex *texcopy;
	Lamp *lampcopy;
	World *worldcopy;
	
	float col[4];       /* active object color */
	
	int sizex, sizey;
	unsigned int *pr_rect;
	int pr_method;

	Main *pr_main;
} ShaderPreview;

typedef struct IconPreviewSize {
	struct IconPreviewSize *next, *prev;
	int sizex, sizey;
	unsigned int *rect;
} IconPreviewSize;

typedef struct IconPreview {
	Scene *scene;
	void *owner;
	ID *id;
	ListBase sizes;
} IconPreview;

/* *************************** Preview for buttons *********************** */

static Main *G_pr_main = NULL;
static Main *G_pr_main_cycles = NULL;

#ifndef WITH_HEADLESS
static Main *load_main_from_memory(const void *blend, int blend_size)
{
	const int fileflags = G.fileflags;
	Main *bmain = NULL;
	BlendFileData *bfd;

	G.fileflags |= G_FILE_NO_UI;
	bfd = BLO_read_from_memory(blend, blend_size, NULL);
	if (bfd) {
		bmain = bfd->main;

		MEM_freeN(bfd);
	}
	G.fileflags = fileflags;

	return bmain;
}
#endif

void ED_preview_init_dbase(void)
{
#ifndef WITH_HEADLESS
	G_pr_main = load_main_from_memory(datatoc_preview_blend, datatoc_preview_blend_size);
	G_pr_main_cycles = load_main_from_memory(datatoc_preview_cycles_blend, datatoc_preview_cycles_blend_size);
#endif
}

void ED_preview_free_dbase(void)
{
	if (G_pr_main)
		BKE_main_free(G_pr_main);

	if (G_pr_main_cycles)
		BKE_main_free(G_pr_main_cycles);
}

static int preview_mat_has_sss(Material *mat, bNodeTree *ntree)
{
	if (mat) {
		if (mat->sss_flag & MA_DIFF_SSS)
			return 1;
		if (mat->nodetree)
			if (preview_mat_has_sss(NULL, mat->nodetree))
				return 1;
	}
	else if (ntree) {
		bNode *node;
		for (node = ntree->nodes.first; node; node = node->next) {
			if (node->type == NODE_GROUP && node->id) {
				if (preview_mat_has_sss(NULL, (bNodeTree *)node->id))
					return 1;
			}
			else if (node->id && ELEM(node->type, SH_NODE_MATERIAL, SH_NODE_MATERIAL_EXT)) {
				mat = (Material *)node->id;
				if (mat->sss_flag & MA_DIFF_SSS)
					return 1;
			}
		}
	}
	return 0;
}

static Scene *preview_get_scene(Main *pr_main)
{
	if (pr_main == NULL) return NULL;
	
	return pr_main->scene.first;
}


/* call this with a pointer to initialize preview scene */
/* call this with NULL to restore assigned ID pointers in preview scene */
static Scene *preview_prepare_scene(Scene *scene, ID *id, int id_type, ShaderPreview *sp)
{
	Scene *sce;
	Base *base;
	Main *pr_main = sp->pr_main;
	
	sce = preview_get_scene(pr_main);
	if (sce) {
		
		/* this flag tells render to not execute depsgraph or ipos etc */
		sce->r.scemode |= R_BUTS_PREVIEW;
		/* set world always back, is used now */
		sce->world = pr_main->world.first;
		/* now: exposure copy */
		if (scene->world) {
			sce->world->exp = scene->world->exp;
			sce->world->range = scene->world->range;
		}
		
		sce->r.color_mgt_flag = scene->r.color_mgt_flag;
		BKE_color_managed_display_settings_copy(&sce->display_settings, &scene->display_settings);

		BKE_color_managed_view_settings_free(&sce->view_settings);
		BKE_color_managed_view_settings_copy(&sce->view_settings, &scene->view_settings);
		
		/* prevent overhead for small renders and icons (32) */
		if (id && sp->sizex < 40) {
			sce->r.tilex = sce->r.tiley = 64;
		}
		else {
			sce->r.tilex = sce->r.xsch / 4;
			sce->r.tiley = sce->r.ysch / 4;
		}
		
		if ((id && sp->pr_method == PR_ICON_RENDER) && id_type != ID_WO)
			sce->r.alphamode = R_ALPHAPREMUL;
		else
			sce->r.alphamode = R_ADDSKY;

		sce->r.cfra = scene->r.cfra;

		if (id_type == ID_TE) {
			/* Force blender internal for texture icons and nodes render,
			 * seems commonly used render engines does not support
			 * such kind of rendering.
			 */
			BLI_strncpy(sce->r.engine, "BLENDER_RENDER", sizeof(sce->r.engine));
		}
		else {
			BLI_strncpy(sce->r.engine, scene->r.engine, sizeof(sce->r.engine));
		}
		
		if (id_type == ID_MA) {
			Material *mat = NULL, *origmat = (Material *)id;
			
			if (origmat) {
				/* work on a copy */
				mat = localize_material(origmat);
				sp->matcopy = mat;
				BLI_addtail(&pr_main->mat, mat);
				
				if (!BKE_scene_use_new_shading_nodes(scene)) {
					init_render_material(mat, 0, NULL);     /* call that retrieves mode_l */
					end_render_material(mat);
					
					/* un-useful option */
					if (sp->pr_method == PR_ICON_RENDER)
						mat->shade_flag &= ~MA_OBCOLOR;

					/* turn on raytracing if needed */
					if (mat->mode_l & MA_RAYMIRROR)
						sce->r.mode |= R_RAYTRACE;
					if (mat->material_type == MA_TYPE_VOLUME)
						sce->r.mode |= R_RAYTRACE;
					if ((mat->mode_l & MA_RAYTRANSP) && (mat->mode_l & MA_TRANSP))
						sce->r.mode |= R_RAYTRACE;
					if (preview_mat_has_sss(mat, NULL))
						sce->r.mode |= R_SSS;
					
					/* turn off fake shadows if needed */
					/* this only works in a specific case where the preview.blend contains
					 * an object starting with 'c' which has a material linked to it (not the obdata)
					 * and that material has a fake shadow texture in the active texture slot */
					for (base = sce->base.first; base; base = base->next) {
						if (base->object->id.name[2] == 'c') {
							Material *shadmat = give_current_material(base->object, base->object->actcol);
							if (shadmat) {
								if (mat->mode2 & MA_CASTSHADOW) shadmat->septex = 0;
								else shadmat->septex |= 1;
							}
						}
					}
					
					/* turn off bounce lights for volume, 
					 * doesn't make much visual difference and slows it down too */
					for (base = sce->base.first; base; base = base->next) {
						if (base->object->type == OB_LAMP) {
							/* if doesn't match 'Lamp.002' --> main key light */
							if (strcmp(base->object->id.name + 2, "Lamp.002") != 0) {
								if (mat->material_type == MA_TYPE_VOLUME)
									base->object->restrictflag |= OB_RESTRICT_RENDER;
								else
									base->object->restrictflag &= ~OB_RESTRICT_RENDER;
							}
						}
					}
				}
				else {
					/* use current scene world to light sphere */
					if (mat->pr_type == MA_SPHERE_A)
						sce->world = scene->world;
				}
				
				if (sp->pr_method == PR_ICON_RENDER) {
					if (mat->material_type == MA_TYPE_HALO) {
						sce->lay = 1 << MA_FLAT;
					}
					else {
						sce->lay = 1 << MA_SPHERE_A;

						/* same as above, use current scene world to light sphere */
						if (BKE_scene_use_new_shading_nodes(scene))
							sce->world = scene->world;
					}
				}
				else {
					sce->lay = 1 << mat->pr_type;
					if (mat->nodetree && sp->pr_method == PR_NODE_RENDER) {
						/* two previews, they get copied by wmJob */
						BKE_node_preview_init_tree(mat->nodetree, sp->sizex, sp->sizey, true);
						BKE_node_preview_init_tree(origmat->nodetree, sp->sizex, sp->sizey, true);
					}
				}
			}
			else {
				sce->r.mode &= ~(R_OSA | R_RAYTRACE | R_SSS);
				
			}
			
			for (base = sce->base.first; base; base = base->next) {
				if (base->object->id.name[2] == 'p') {
					/* copy over object color, in case material uses it */
					copy_v4_v4(base->object->col, sp->col);
					
					if (OB_TYPE_SUPPORT_MATERIAL(base->object->type)) {
						/* don't use assign_material, it changed mat->id.us, which shows in the UI */
						Material ***matar = give_matarar(base->object);
						int actcol = max_ii(base->object->actcol - 1, 0);

						if (matar && actcol < base->object->totcol)
							(*matar)[actcol] = mat;
					}
					else if (base->object->type == OB_LAMP) {
						base->object->restrictflag &= ~OB_RESTRICT_RENDER;
					}
				}
			}
		}
		else if (id_type == ID_TE) {
			Tex *tex = NULL, *origtex = (Tex *)id;
			
			if (origtex) {
				tex = localize_texture(origtex);
				sp->texcopy = tex;
				BLI_addtail(&pr_main->tex, tex);
			}
			sce->lay = 1 << MA_TEXTURE;
			
			for (base = sce->base.first; base; base = base->next) {
				if (base->object->id.name[2] == 't') {
					Material *mat = give_current_material(base->object, base->object->actcol);
					if (mat && mat->mtex[0]) {
						mat->mtex[0]->tex = tex;
						
						if (tex && sp->slot)
							mat->mtex[0]->which_output = sp->slot->which_output;

						mat->mtex[0]->mapto &= ~MAP_ALPHA;
						mat->alpha = 1.0f;

						/* show alpha in this case */
						if (tex == NULL || (tex->flag & TEX_PRV_ALPHA)) {
							if (!(tex && tex->type == TEX_IMAGE && (tex->imaflag & (TEX_USEALPHA | TEX_CALCALPHA)) == 0)) {
								mat->mtex[0]->mapto |= MAP_ALPHA;
								mat->alpha = 0.0f;
							}
						}
					}
				}
			}

			if (tex && tex->nodetree && sp->pr_method == PR_NODE_RENDER) {
				/* two previews, they get copied by wmJob */
				BKE_node_preview_init_tree(origtex->nodetree, sp->sizex, sp->sizey, true);
				BKE_node_preview_init_tree(tex->nodetree, sp->sizex, sp->sizey, true);
			}
		}
		else if (id_type == ID_LA) {
			Lamp *la = NULL, *origla = (Lamp *)id;

			/* work on a copy */
			if (origla) {
				la = localize_lamp(origla);
				sp->lampcopy = la;
				BLI_addtail(&pr_main->lamp, la);
			}

			sce->lay = 1 << MA_LAMP;

			if (!BKE_scene_use_new_shading_nodes(scene)) {
				if (la && la->type == LA_SUN && (la->sun_effect_type & LA_SUN_EFFECT_SKY)) {
					sce->lay = 1 << MA_ATMOS;
					sce->world = scene->world;
					sce->camera = (Object *)BLI_findstring(&pr_main->object, "CameraAtmo", offsetof(ID, name) + 2);
				}
				else {
					sce->world = NULL;
					sce->camera = (Object *)BLI_findstring(&pr_main->object, "Camera", offsetof(ID, name) + 2);
				}
			}
				
			for (base = sce->base.first; base; base = base->next) {
				if (base->object->id.name[2] == 'p') {
					if (base->object->type == OB_LAMP)
						base->object->data = la;
				}
			}

			if (la && la->nodetree && sp->pr_method == PR_NODE_RENDER) {
				/* two previews, they get copied by wmJob */
				BKE_node_preview_init_tree(origla->nodetree, sp->sizex, sp->sizey, true);
				BKE_node_preview_init_tree(la->nodetree, sp->sizex, sp->sizey, true);
			}
		}
		else if (id_type == ID_WO) {
			World *wrld = NULL, *origwrld = (World *)id;

			if (origwrld) {
				wrld = localize_world(origwrld);
				sp->worldcopy = wrld;
				BLI_addtail(&pr_main->world, wrld);
			}

			sce->lay = 1 << MA_SKY;
			sce->world = wrld;

			if (wrld && wrld->nodetree && sp->pr_method == PR_NODE_RENDER) {
				/* two previews, they get copied by wmJob */
				BKE_node_preview_init_tree(wrld->nodetree, sp->sizex, sp->sizey, true);
				BKE_node_preview_init_tree(origwrld->nodetree, sp->sizex, sp->sizey, true);
			}
		}
		
		return sce;
	}
	
	return NULL;
}

/* new UI convention: draw is in pixel space already. */
/* uses ROUNDBOX button in block to get the rect */
static bool ed_preview_draw_rect(ScrArea *sa, int split, int first, rcti *rect, rcti *newrect)
{
	Render *re;
	RenderResult rres;
	char name[32];
	int offx = 0;
	int newx = BLI_rcti_size_x(rect);
	int newy = BLI_rcti_size_y(rect);
	bool ok = false;

	if (!split || first) sprintf(name, "Preview %p", (void *)sa);
	else sprintf(name, "SecondPreview %p", (void *)sa);

	if (split) {
		if (first) {
			offx = 0;
			newx = newx / 2;
		}
		else {
			offx = newx / 2;
			newx = newx - newx / 2;
		}
	}

	/* test if something rendered ok */
	re = RE_GetRender(name);
	RE_AcquireResultImage(re, &rres);

	if (rres.rectf) {
		
		if (ABS(rres.rectx - newx) < 2 && ABS(rres.recty - newy) < 2) {

			newrect->xmax = max_ii(newrect->xmax, rect->xmin + rres.rectx + offx);
			newrect->ymax = max_ii(newrect->ymax, rect->ymin + rres.recty);

			if (rres.rectx && rres.recty) {
				unsigned char *rect_byte = MEM_mallocN(rres.rectx * rres.recty * sizeof(int), "ed_preview_draw_rect");
				float fx = rect->xmin + offx;
				float fy = rect->ymin;
				
				RE_AcquiredResultGet32(re, &rres, (unsigned int *)rect_byte);
				glaDrawPixelsSafe(fx, fy, rres.rectx, rres.recty, rres.rectx, GL_RGBA, GL_UNSIGNED_BYTE, rect_byte);
				
				MEM_freeN(rect_byte);
				
				ok = 1;
			}
		}
	}

	RE_ReleaseResultImage(re);

	return ok;
}

void ED_preview_draw(const bContext *C, void *idp, void *parentp, void *slotp, rcti *rect)
{
	if (idp) {
		wmWindowManager *wm = CTX_wm_manager(C);
		ScrArea *sa = CTX_wm_area(C);
		ID *id = (ID *)idp;
		ID *parent = (ID *)parentp;
		MTex *slot = (MTex *)slotp;
		SpaceButs *sbuts = sa->spacedata.first;
		ShaderPreview *sp = WM_jobs_customdata(wm, sa);
		rcti newrect;
		int ok;
		int newx = BLI_rcti_size_x(rect);
		int newy = BLI_rcti_size_y(rect);

		newrect.xmin = rect->xmin;
		newrect.xmax = rect->xmin;
		newrect.ymin = rect->ymin;
		newrect.ymax = rect->ymin;

		if (parent) {
			ok = ed_preview_draw_rect(sa, 1, 1, rect, &newrect);
			ok &= ed_preview_draw_rect(sa, 1, 0, rect, &newrect);
		}
		else
			ok = ed_preview_draw_rect(sa, 0, 0, rect, &newrect);

		if (ok)
			*rect = newrect;

		/* start a new preview render job if signalled through sbuts->preview,
		 * if no render result was found and no preview render job is running,
		 * or if the job is running and the size of preview changed */
		if ((sbuts->spacetype == SPACE_BUTS && sbuts->preview) ||
		    (!ok && !WM_jobs_test(wm, sa, WM_JOB_TYPE_RENDER_PREVIEW)) ||
		    (sp && (ABS(sp->sizex - newx) >= 2 || ABS(sp->sizey - newy) > 2)))
		{
			sbuts->preview = 0;
			ED_preview_shader_job(C, sa, id, parent, slot, newx, newy, PR_BUTS_RENDER);
		}
	}
}

/* **************************** new shader preview system ****************** */

/* inside thread, called by renderer, sets job update value */
static void shader_preview_update(void *spv, RenderResult *UNUSED(rr), volatile struct rcti *UNUSED(rect))
{
	ShaderPreview *sp = spv;
	
	*(sp->do_update) = true;
}

/* called by renderer, checks job value */
static int shader_preview_break(void *spv)
{
	ShaderPreview *sp = spv;

	return *(sp->stop);
}

/* outside thread, called before redraw notifiers, it moves finished preview over */
static void shader_preview_updatejob(void *spv)
{
	ShaderPreview *sp = spv;
	
	if (sp->id) {
		if (sp->pr_method == PR_NODE_RENDER) {
			if (GS(sp->id->name) == ID_MA) {
				Material *mat = (Material *)sp->id;
				
				if (sp->matcopy && mat->nodetree && sp->matcopy->nodetree)
					ntreeLocalSync(sp->matcopy->nodetree, mat->nodetree);
			}
			else if (GS(sp->id->name) == ID_TE) {
				Tex *tex = (Tex *)sp->id;
				
				if (sp->texcopy && tex->nodetree && sp->texcopy->nodetree)
					ntreeLocalSync(sp->texcopy->nodetree, tex->nodetree);
			}
			else if (GS(sp->id->name) == ID_WO) {
				World *wrld = (World *)sp->id;
				
				if (sp->worldcopy && wrld->nodetree && sp->worldcopy->nodetree)
					ntreeLocalSync(sp->worldcopy->nodetree, wrld->nodetree);
			}
			else if (GS(sp->id->name) == ID_LA) {
				Lamp *la = (Lamp *)sp->id;
				
				if (sp->lampcopy && la->nodetree && sp->lampcopy->nodetree)
					ntreeLocalSync(sp->lampcopy->nodetree, la->nodetree);
			}
		}
	}
}

static void shader_preview_render(ShaderPreview *sp, ID *id, int split, int first)
{
	Render *re;
	Scene *sce;
	float oldlens;
	short idtype = GS(id->name);
	char name[32];
	int sizex;
	Main *pr_main = sp->pr_main;
	
	/* in case of split preview, use border render */
	if (split) {
		if (first) sizex = sp->sizex / 2;
		else sizex = sp->sizex - sp->sizex / 2;
	}
	else {
		sizex = sp->sizex;
	}

	/* we have to set preview variables first */
	sce = preview_get_scene(pr_main);
	if (sce) {
		sce->r.xsch = sizex;
		sce->r.ysch = sp->sizey;
		sce->r.size = 100;
	}
	
	/* get the stuff from the builtin preview dbase */
	sce = preview_prepare_scene(sp->scene, id, idtype, sp);
	if (sce == NULL) return;
	
	if (!split || first) sprintf(name, "Preview %p", sp->owner);
	else sprintf(name, "SecondPreview %p", sp->owner);
	re = RE_GetRender(name);
	
	/* full refreshed render from first tile */
	if (re == NULL)
		re = RE_NewRender(name);
		
	/* sce->r gets copied in RE_InitState! */
	sce->r.scemode &= ~(R_MATNODE_PREVIEW | R_TEXNODE_PREVIEW);
	sce->r.scemode &= ~R_NO_IMAGE_LOAD;

	if (sp->pr_method == PR_ICON_RENDER) {
		sce->r.scemode |= R_NO_IMAGE_LOAD;
		sce->r.mode |= R_OSA;
	}
	else if (sp->pr_method == PR_NODE_RENDER) {
		if (idtype == ID_MA) sce->r.scemode |= R_MATNODE_PREVIEW;
		else if (idtype == ID_TE) sce->r.scemode |= R_TEXNODE_PREVIEW;
		sce->r.mode &= ~R_OSA;
	}
	else {  /* PR_BUTS_RENDER */
		sce->r.mode |= R_OSA;
	}


	/* callbacs are cleared on GetRender() */
	if (ELEM(sp->pr_method, PR_BUTS_RENDER, PR_NODE_RENDER)) {
		RE_display_update_cb(re, sp, shader_preview_update);
	}
	/* set this for all previews, default is react to G.is_break still */
	RE_test_break_cb(re, sp, shader_preview_break);
	
	/* lens adjust */
	oldlens = ((Camera *)sce->camera->data)->lens;
	if (sizex > sp->sizey)
		((Camera *)sce->camera->data)->lens *= (float)sp->sizey / (float)sizex;

	/* entire cycle for render engine */
	RE_PreviewRender(re, pr_main, sce);

	((Camera *)sce->camera->data)->lens = oldlens;

	/* handle results */
	if (sp->pr_method == PR_ICON_RENDER) {
		// char *rct= (char *)(sp->pr_rect + 32*16 + 16);
		
		if (sp->pr_rect)
			RE_ResultGet32(re, sp->pr_rect);
	}
	else {
		/* validate owner */
		//if (ri->rect == NULL)
		//	ri->rect= MEM_mallocN(sizeof(int) * ri->pr_rectx*ri->pr_recty, "BIF_previewrender");
		//RE_ResultGet32(re, ri->rect);
	}

	/* unassign the pointers, reset vars */
	preview_prepare_scene(sp->scene, NULL, GS(id->name), sp);
	
	/* XXX bad exception, end-exec is not being called in render, because it uses local main */
//	if (idtype == ID_TE) {
//		Tex *tex= (Tex *)id;
//		if (tex->use_nodes && tex->nodetree)
//			ntreeEndExecTree(tex->nodetree);
//	}

}

/* runs inside thread for material and icons */
static void shader_preview_startjob(void *customdata, short *stop, short *do_update)
{
	ShaderPreview *sp = customdata;

	sp->stop = stop;
	sp->do_update = do_update;

	if (sp->parent) {
		shader_preview_render(sp, sp->id, 1, 1);
		shader_preview_render(sp, sp->parent, 1, 0);
	}
	else
		shader_preview_render(sp, sp->id, 0, 0);

	*do_update = true;
}

static void shader_preview_free(void *customdata)
{
	ShaderPreview *sp = customdata;
	Main *pr_main = sp->pr_main;
	
	if (sp->matcopy) {
		struct IDProperty *properties;
		
		/* node previews */
		shader_preview_updatejob(sp);
		
		/* get rid of copied material */
		BLI_remlink(&pr_main->mat, sp->matcopy);
		
		BKE_material_free_ex(sp->matcopy, false);

		properties = IDP_GetProperties((ID *)sp->matcopy, false);
		if (properties) {
			IDP_FreeProperty(properties);
			MEM_freeN(properties);
		}
		MEM_freeN(sp->matcopy);
	}
	if (sp->texcopy) {
		struct IDProperty *properties;
		/* node previews */
		shader_preview_updatejob(sp);
		
		/* get rid of copied texture */
		BLI_remlink(&pr_main->tex, sp->texcopy);
		BKE_texture_free(sp->texcopy);
		
		properties = IDP_GetProperties((ID *)sp->texcopy, false);
		if (properties) {
			IDP_FreeProperty(properties);
			MEM_freeN(properties);
		}
		MEM_freeN(sp->texcopy);
	}
	if (sp->worldcopy) {
		struct IDProperty *properties;
		/* node previews */
		shader_preview_updatejob(sp);
		
		/* get rid of copied world */
		BLI_remlink(&pr_main->world, sp->worldcopy);
		BKE_world_free_ex(sp->worldcopy, true); /* [#32865] - we need to unlink the texture copies, unlike for materials */
		
		properties = IDP_GetProperties((ID *)sp->worldcopy, false);
		if (properties) {
			IDP_FreeProperty(properties);
			MEM_freeN(properties);
		}
		MEM_freeN(sp->worldcopy);
	}
	if (sp->lampcopy) {
		struct IDProperty *properties;
		/* node previews */
		shader_preview_updatejob(sp);
		
		/* get rid of copied lamp */
		BLI_remlink(&pr_main->lamp, sp->lampcopy);
		BKE_lamp_free(sp->lampcopy);
		
		properties = IDP_GetProperties((ID *)sp->lampcopy, false);
		if (properties) {
			IDP_FreeProperty(properties);
			MEM_freeN(properties);
		}
		MEM_freeN(sp->lampcopy);
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
	if (ibuf == NULL || (ibuf->rect == NULL && ibuf->rect_float == NULL))
		return;
	
	/* waste of cpu cyles... but the imbuf API has no other way to scale fast (ton) */
	ima = IMB_dupImBuf(ibuf);
	
	if (!ima) 
		return;
	
	if (ima->x > ima->y) {
		scaledx = (float)w;
		scaledy =  ( (float)ima->y / (float)ima->x) * (float)w;
	}
	else {
		scaledx =  ( (float)ima->x / (float)ima->y) * (float)h;
		scaledy = (float)h;
	}
	
	ex = (short)scaledx;
	ey = (short)scaledy;
	
	dx = (w - ex) / 2;
	dy = (h - ey) / 2;
	
	IMB_scalefastImBuf(ima, ex, ey);
	
	/* if needed, convert to 32 bits */
	if (ima->rect == NULL)
		IMB_rect_from_float(ima);

	srect = ima->rect;
	drect = rect;

	drect += dy * w + dx;
	for (; ey > 0; ey--) {
		memcpy(drect, srect, ex * sizeof(int));
		drect += w;
		srect += ima->x;
	}

	IMB_freeImBuf(ima);
}

static void set_alpha(char *cp, int sizex, int sizey, char alpha) 
{
	int a, size = sizex * sizey;

	for (a = 0; a < size; a++, cp += 4)
		cp[3] = alpha;
}

static void icon_preview_startjob(void *customdata, short *stop, short *do_update)
{
	ShaderPreview *sp = customdata;
	ID *id = sp->id;
	short idtype = GS(id->name);
	
	if (idtype == ID_IM) {
		Image *ima = (Image *)id;
		ImBuf *ibuf = NULL;
		ImageUser iuser = {NULL};

		/* ima->ok is zero when Image cannot load */
		if (ima == NULL || ima->ok == 0)
			return;

		/* setup dummy image user */
		iuser.ok = iuser.framenr = 1;
		iuser.scene = sp->scene;
		
		/* elubie: this needs to be changed: here image is always loaded if not
		 * already there. Very expensive for large images. Need to find a way to 
		 * only get existing ibuf */
		ibuf = BKE_image_acquire_ibuf(ima, &iuser, NULL);
		if (ibuf == NULL || ibuf->rect == NULL) {
			BKE_image_release_ibuf(ima, ibuf, NULL);
			return;
		}
		
		icon_copy_rect(ibuf, sp->sizex, sp->sizey, sp->pr_rect);

		*do_update = true;

		BKE_image_release_ibuf(ima, ibuf, NULL);
	}
	else if (idtype == ID_BR) {
		Brush *br = (Brush *)id;

		br->icon_imbuf = get_brush_icon(br);

		memset(sp->pr_rect, 0x88, sp->sizex * sp->sizey * sizeof(unsigned int));

		if (!(br->icon_imbuf) || !(br->icon_imbuf->rect))
			return;

		icon_copy_rect(br->icon_imbuf, sp->sizex, sp->sizey, sp->pr_rect);

		*do_update = true;
	}
	else {
		/* re-use shader job */
		shader_preview_startjob(customdata, stop, do_update);

		/* world is rendered with alpha=0, so it wasn't displayed 
		 * this could be render option for sky to, for later */
		if (idtype == ID_WO) {
			set_alpha((char *)sp->pr_rect, sp->sizex, sp->sizey, 255);
		}
		else if (idtype == ID_MA) {
			Material *ma = (Material *)id;

			if (ma->material_type == MA_TYPE_HALO)
				set_alpha((char *)sp->pr_rect, sp->sizex, sp->sizey, 255);
		}
	}
}

/* use same function for icon & shader, so the job manager
 * does not run two of them at the same time. */

static void common_preview_startjob(void *customdata, short *stop, short *do_update, float *UNUSED(progress))
{
	ShaderPreview *sp = customdata;

	if (sp->pr_method == PR_ICON_RENDER)
		icon_preview_startjob(customdata, stop, do_update);
	else
		shader_preview_startjob(customdata, stop, do_update);
}

/* exported functions */

static void icon_preview_add_size(IconPreview *ip, unsigned int *rect, int sizex, int sizey)
{
	IconPreviewSize *cur_size = ip->sizes.first, *new_size;

	while (cur_size) {
		if (cur_size->sizex == sizex && cur_size->sizey == sizey) {
			/* requested size is already in list, no need to add it again */
			return;
		}

		cur_size = cur_size->next;
	}

	new_size = MEM_callocN(sizeof(IconPreviewSize), "IconPreviewSize");
	new_size->sizex = sizex;
	new_size->sizey = sizey;
	new_size->rect = rect;

	BLI_addtail(&ip->sizes, new_size);
}

static void icon_preview_startjob_all_sizes(void *customdata, short *stop, short *do_update, float *progress)
{
	IconPreview *ip = (IconPreview *)customdata;
	IconPreviewSize *cur_size = ip->sizes.first;
	const bool use_new_shading = BKE_scene_use_new_shading_nodes(ip->scene);

	while (cur_size) {
		ShaderPreview *sp = MEM_callocN(sizeof(ShaderPreview), "Icon ShaderPreview");

		/* construct shader preview from image size and previewcustomdata */
		sp->scene = ip->scene;
		sp->owner = ip->owner;
		sp->sizex = cur_size->sizex;
		sp->sizey = cur_size->sizey;
		sp->pr_method = PR_ICON_RENDER;
		sp->pr_rect = cur_size->rect;
		sp->id = ip->id;

		if (use_new_shading) {
			/* texture icon rendering is hardcoded to use BI,
			 * so don't even think of using cycle's bmain for
			 * texture icons
			 */
			if (GS(ip->id->name) != ID_TE)
				sp->pr_main = G_pr_main_cycles;
			else
				sp->pr_main = G_pr_main;
		}
		else {
			sp->pr_main = G_pr_main;
		}

		common_preview_startjob(sp, stop, do_update, progress);
		shader_preview_free(sp);

		cur_size = cur_size->next;
	}
}

static void icon_preview_endjob(void *customdata)
{
	IconPreview *ip = customdata;

	if (ip->id) {

		if (GS(ip->id->name) == ID_BR)
			WM_main_add_notifier(NC_BRUSH | NA_EDITED, ip->id);
#if 0		
		if (GS(ip->id->name) == ID_MA) {
			Material *ma = (Material *)ip->id;
			PreviewImage *prv_img = ma->preview;
			int i;

			/* signal to gpu texture */
			for (i = 0; i < NUM_ICON_SIZES; ++i) {
				if (prv_img->gputexture[i]) {
					GPU_texture_free(prv_img->gputexture[i]);
					prv_img->gputexture[i] = NULL;
					WM_main_add_notifier(NC_MATERIAL|ND_SHADING_DRAW, ip->id);
				}
			}
		}
#endif
	}
}

static void icon_preview_free(void *customdata)
{
	IconPreview *ip = (IconPreview *)customdata;

	BLI_freelistN(&ip->sizes);
	MEM_freeN(ip);
}

void ED_preview_icon_job(const bContext *C, void *owner, ID *id, unsigned int *rect, int sizex, int sizey)
{
	wmJob *wm_job;
	IconPreview *ip, *old_ip;
	
	/* suspended start means it starts after 1 timer step, see WM_jobs_timer below */
	wm_job = WM_jobs_get(CTX_wm_manager(C), CTX_wm_window(C), owner, "Icon Preview",
	                     WM_JOB_EXCL_RENDER | WM_JOB_SUSPEND, WM_JOB_TYPE_RENDER_PREVIEW);

	ip = MEM_callocN(sizeof(IconPreview), "icon preview");

	/* render all resolutions from suspended job too */
	old_ip = WM_jobs_customdata_get(wm_job);
	if (old_ip)
		BLI_movelisttolist(&ip->sizes, &old_ip->sizes);

	/* customdata for preview thread */
	ip->scene = CTX_data_scene(C);
	ip->owner = id;
	ip->id = id;

	icon_preview_add_size(ip, rect, sizex, sizey);

	/* setup job */
	WM_jobs_customdata_set(wm_job, ip, icon_preview_free);
	WM_jobs_timer(wm_job, 0.1, NC_MATERIAL, NC_MATERIAL);
	WM_jobs_callbacks(wm_job, icon_preview_startjob_all_sizes, NULL, NULL, icon_preview_endjob);

	WM_jobs_start(CTX_wm_manager(C), wm_job);
}

void ED_preview_shader_job(const bContext *C, void *owner, ID *id, ID *parent, MTex *slot, int sizex, int sizey, int method)
{
	Object *ob = CTX_data_active_object(C);
	wmJob *wm_job;
	ShaderPreview *sp;
	Scene *scene = CTX_data_scene(C);
	short id_type = GS(id->name);
	bool use_new_shading = BKE_scene_use_new_shading_nodes(scene);

	/* Only texture node preview is supported with Cycles. */
	if (use_new_shading && method == PR_NODE_RENDER && id_type != ID_TE) {
		return;
	}

	wm_job = WM_jobs_get(CTX_wm_manager(C), CTX_wm_window(C), owner, "Shader Preview",
	                    WM_JOB_EXCL_RENDER, WM_JOB_TYPE_RENDER_PREVIEW);
	sp = MEM_callocN(sizeof(ShaderPreview), "shader preview");

	/* customdata for preview thread */
	sp->scene = scene;
	sp->owner = owner;
	sp->sizex = sizex;
	sp->sizey = sizey;
	sp->pr_method = method;
	sp->id = id;
	sp->parent = parent;
	sp->slot = slot;

	/* hardcoded preview .blend for cycles/internal, this should be solved
	 * once with custom preview .blend path for external engines */
	if ((method != PR_NODE_RENDER) && id_type != ID_TE && use_new_shading) {
		sp->pr_main = G_pr_main_cycles;
	}
	else {
		sp->pr_main = G_pr_main;
	}

	if (ob && ob->totcol) copy_v4_v4(sp->col, ob->col);
	else sp->col[0] = sp->col[1] = sp->col[2] = sp->col[3] = 1.0f;
	
	/* setup job */
	WM_jobs_customdata_set(wm_job, sp, shader_preview_free);
	WM_jobs_timer(wm_job, 0.1, NC_MATERIAL, NC_MATERIAL);
	WM_jobs_callbacks(wm_job, common_preview_startjob, NULL, shader_preview_updatejob, NULL);
	
	WM_jobs_start(CTX_wm_manager(C), wm_job);
}

void ED_preview_kill_jobs(const struct bContext *C)
{
	wmWindowManager *wm = CTX_wm_manager(C);
	if (wm)
		WM_jobs_kill(wm, NULL, common_preview_startjob);
	
	ED_viewport_render_kill_jobs(C, false);
}

