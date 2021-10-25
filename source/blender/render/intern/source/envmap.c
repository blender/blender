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
 * Contributors: 2004/2005/2006 Blender Foundation, full recode
 *
 * ***** END GPL LICENSE BLOCK *****
 */

/** \file blender/render/intern/source/envmap.c
 *  \ingroup render
 */

#include <math.h>
#include <string.h>

/* external modules: */

#include "BLI_math.h"
#include "BLI_blenlib.h"
#include "BLI_threads.h"
#include "BLI_utildefines.h"

#include "BLT_translation.h"

#include "IMB_imbuf_types.h"
#include "IMB_imbuf.h"        /* for rectcpy */

#include "DNA_group_types.h"
#include "DNA_image_types.h"
#include "DNA_lamp_types.h"
#include "DNA_object_types.h"
#include "DNA_scene_types.h"
#include "DNA_texture_types.h"

#include "BKE_main.h"
#include "BKE_image.h"   /* BKE_imbuf_write */
#include "BKE_texture.h"
#include "BKE_scene.h"

/* this module */
#include "render_types.h"
#include "envmap.h"
#include "renderdatabase.h"
#include "renderpipeline.h"
#include "texture.h"
#include "zbuf.h"
#include "render_result.h"

/* ------------------------------------------------------------------------- */

static void envmap_split_ima(EnvMap *env, ImBuf *ibuf)
{
	int dx, part;
	
	/* after lock we test cube[1], if set the other thread has done it fine */
	BLI_lock_thread(LOCK_IMAGE);
	if (env->cube[1] == NULL) {

		BKE_texture_envmap_free_data(env);
	
		dx = ibuf->y;
		dx /= 2;
		if (3 * dx == ibuf->x) {
			env->type = ENV_CUBE;
			env->ok = ENV_OSA;
		}
		else if (ibuf->x == ibuf->y) {
			env->type = ENV_PLANE;
			env->ok = ENV_OSA;
		}
		else {
			printf("Incorrect envmap size\n");
			env->ok = 0;
			env->ima->ok = 0;
		}
		
		if (env->ok) {
			if (env->type == ENV_CUBE) {
				for (part = 0; part < 6; part++) {
					env->cube[part] = IMB_allocImBuf(dx, dx, 24, IB_rect | IB_rectfloat);
				}
				IMB_float_from_rect(ibuf);
				
				IMB_rectcpy(env->cube[0], ibuf, 
				            0, 0, 0, 0, dx, dx);
				IMB_rectcpy(env->cube[1], ibuf, 
				            0, 0, dx, 0, dx, dx);
				IMB_rectcpy(env->cube[2], ibuf, 
				            0, 0, 2 * dx, 0, dx, dx);
				IMB_rectcpy(env->cube[3], ibuf, 
				            0, 0, 0, dx, dx, dx);
				IMB_rectcpy(env->cube[4], ibuf, 
				            0, 0, dx, dx, dx, dx);
				IMB_rectcpy(env->cube[5], ibuf, 
				            0, 0, 2 * dx, dx, dx, dx);
				
			}
			else { /* ENV_PLANE */
				env->cube[1] = IMB_dupImBuf(ibuf);
				IMB_float_from_rect(env->cube[1]);
			}
		}
	}
	BLI_unlock_thread(LOCK_IMAGE);
}

/* ------------------------------------------------------------------------- */
/* ****************** RENDER ********************** */

/* copy current render */
static Render *envmap_render_copy(Render *re, EnvMap *env)
{
	Render *envre;
	float viewscale;
	int cuberes;
	
	envre = RE_NewRender("Envmap");
	
	env->lastsize = re->r.size;
	cuberes = (env->cuberes * re->r.size) / 100;
	cuberes &= 0xFFFC;
	
	/* this flag has R_ZTRA in it for example */
	envre->flag = re->flag;
	
	/* set up renderdata */
	render_copy_renderdata(&envre->r, &re->r);
	envre->r.mode &= ~(R_BORDER | R_PANORAMA | R_ORTHO | R_MBLUR);
	BLI_freelistN(&envre->r.layers);
	BLI_freelistN(&envre->r.views);
	envre->r.filtertype = 0;
	envre->r.tilex = envre->r.xsch / 2;
	envre->r.tiley = envre->r.ysch / 2;
	envre->r.size = 100;
	envre->r.yasp = envre->r.xasp = 1;
	
	RE_InitState(envre, NULL, &envre->r, NULL, cuberes, cuberes, NULL);
	envre->main = re->main;
	envre->scene = re->scene;    /* unsure about this... */
	envre->scene_color_manage = re->scene_color_manage;
	envre->lay = re->lay;

	/* view stuff in env render */
	viewscale = (env->type == ENV_PLANE) ? env->viewscale : 1.0f;
	RE_SetEnvmapCamera(envre, env->object, viewscale, env->clipsta, env->clipend);
	copy_m4_m4(envre->viewmat_orig, re->viewmat_orig);
	
	/* callbacks */
	envre->display_update = re->display_update;
	envre->duh = re->duh;
	envre->test_break = re->test_break;
	envre->tbh = re->tbh;
	envre->current_scene_update = re->current_scene_update;
	envre->suh = re->suh;
	
	/* and for the evil stuff; copy the database... */
	envre->totvlak = re->totvlak;
	envre->totvert = re->totvert;
	envre->tothalo = re->tothalo;
	envre->totstrand = re->totstrand;
	envre->totlamp = re->totlamp;
	envre->sortedhalos = re->sortedhalos;
	envre->lights = re->lights;
	envre->objecttable = re->objecttable;
	envre->customdata_names = re->customdata_names;
	envre->raytree = re->raytree;
	envre->totinstance = re->totinstance;
	envre->instancetable = re->instancetable;
	envre->objectinstance = re->objectinstance;
	envre->qmcsamplers = re->qmcsamplers;
	
	return envre;
}

static void envmap_free_render_copy(Render *envre)
{

	envre->totvlak = 0;
	envre->totvert = 0;
	envre->tothalo = 0;
	envre->totstrand = 0;
	envre->totlamp = 0;
	envre->totinstance = 0;
	envre->sortedhalos = NULL;
	BLI_listbase_clear(&envre->lights);
	BLI_listbase_clear(&envre->objecttable);
	BLI_listbase_clear(&envre->customdata_names);
	envre->raytree = NULL;
	BLI_listbase_clear(&envre->instancetable);
	envre->objectinstance = NULL;
	envre->qmcsamplers = NULL;
	
	RE_FreeRender(envre);
}

/* ------------------------------------------------------------------------- */

static void envmap_transmatrix(float mat[4][4], int part)
{
	float tmat[4][4], eul[3], rotmat[4][4];
	
	eul[0] = eul[1] = eul[2] = 0.0;
	
	if (part == 0) {          /* neg z */
		/* pass */
	}
	else if (part == 1) { /* pos z */
		eul[0] = M_PI;
	}
	else if (part == 2) { /* pos y */
		eul[0] = M_PI / 2.0;
	}
	else if (part == 3) { /* neg x */
		eul[0] = M_PI / 2.0;
		eul[2] = M_PI / 2.0;
	}
	else if (part == 4) { /* neg y */
		eul[0] = M_PI / 2.0;
		eul[2] = M_PI;
	}
	else {              /* pos x */
		eul[0] = M_PI / 2.0;
		eul[2] = -M_PI / 2.0;
	}
	
	copy_m4_m4(tmat, mat);
	eul_to_mat4(rotmat, eul);
	mul_m4_m4m4(mat, tmat, rotmat);
}
/* ------------------------------------------------------------------------- */

static void env_set_imats(Render *re)
{
	Base *base;
	float mat[4][4];
	
	base = re->scene->base.first;
	while (base) {
		mul_m4_m4m4(mat, re->viewmat, base->object->obmat);
		invert_m4_m4(base->object->imat, mat);
		
		base = base->next;
	}
	
}

/* ------------------------------------------------------------------------- */

void env_rotate_scene(Render *re, float mat[4][4], int do_rotate)
{
	ObjectRen *obr;
	ObjectInstanceRen *obi;
	LampRen *lar = NULL;
	HaloRen *har = NULL;
	float imat[3][3], mat_inverse[4][4], smat[4][4], tmat[4][4], cmat[3][3], tmpmat[4][4];
	int a;
	
	if (do_rotate == 0) {
		invert_m4_m4(tmat, mat);
		copy_m3_m4(imat, tmat);
		
		copy_m4_m4(mat_inverse, mat);
	}
	else {
		copy_m4_m4(tmat, mat);
		copy_m3_m4(imat, mat);
		
		invert_m4_m4(mat_inverse, tmat);
	}

	for (obi = re->instancetable.first; obi; obi = obi->next) {
		/* append or set matrix depending on dupli */
		if (obi->flag & R_DUPLI_TRANSFORMED) {
			copy_m4_m4(tmpmat, obi->mat);
			mul_m4_m4m4(obi->mat, tmat, tmpmat);
		}
		else if (do_rotate == 1)
			copy_m4_m4(obi->mat, tmat);
		else
			unit_m4(obi->mat);

		copy_m3_m4(cmat, obi->mat);
		invert_m3_m3(obi->nmat, cmat);
		transpose_m3(obi->nmat);

		/* indicate the renderer has to use transform matrices */
		if (do_rotate == 0)
			obi->flag &= ~R_ENV_TRANSFORMED;
		else {
			obi->flag |= R_ENV_TRANSFORMED;
			copy_m4_m4(obi->imat, mat_inverse);
		}
	}
	

	for (obr = re->objecttable.first; obr; obr = obr->next) {
		for (a = 0; a < obr->tothalo; a++) {
			if ((a & 255) == 0) har = obr->bloha[a >> 8];
			else har++;
		
			mul_m4_v3(tmat, har->co);
		}

		/* imat_ren is needed for correct texture coordinates */
		mul_m4_m4m4(obr->ob->imat_ren, re->viewmat, obr->ob->obmat);
		invert_m4(obr->ob->imat_ren);
	}
	
	for (lar = re->lampren.first; lar; lar = lar->next) {
		float lamp_imat[4][4];

		/* copy from add_render_lamp */
		if (do_rotate == 1)
			mul_m4_m4m4(tmpmat, re->viewmat, lar->lampmat);
		else
			mul_m4_m4m4(tmpmat, re->viewmat_orig, lar->lampmat);

		invert_m4_m4(lamp_imat, tmpmat);
		copy_m3_m4(lar->mat, tmpmat);
		copy_m3_m4(lar->imat, lamp_imat);

		lar->vec[0]= -tmpmat[2][0];
		lar->vec[1]= -tmpmat[2][1];
		lar->vec[2]= -tmpmat[2][2];
		normalize_v3(lar->vec);
		lar->co[0]= tmpmat[3][0];
		lar->co[1]= tmpmat[3][1];
		lar->co[2]= tmpmat[3][2];

		if (lar->type == LA_AREA) {
			area_lamp_vectors(lar);
		}
		else if (lar->type == LA_SPOT) {
			normalize_v3(lar->imat[0]);
			normalize_v3(lar->imat[1]);
			normalize_v3(lar->imat[2]);
		
			lar->sh_invcampos[0] = -lar->co[0];
			lar->sh_invcampos[1] = -lar->co[1];
			lar->sh_invcampos[2] = -lar->co[2];
			mul_m3_v3(lar->imat, lar->sh_invcampos);
			lar->sh_invcampos[2] *= lar->sh_zfac;
		
			if (lar->shb) {
				if (do_rotate == 1) {
					mul_m4_m4m4(smat, lar->shb->viewmat, mat_inverse);
					mul_m4_m4m4(lar->shb->persmat, lar->shb->winmat, smat);
				}
				else mul_m4_m4m4(lar->shb->persmat, lar->shb->winmat, lar->shb->viewmat);
			}
		}
	}
	
	if (do_rotate) {
		init_render_world(re);
		env_set_imats(re);
	}
}

/* ------------------------------------------------------------------------- */

static void env_layerflags(Render *re, unsigned int notlay)
{
	ObjectRen *obr;
	VlakRen *vlr = NULL;
	int a;
	
	/* invert notlay, so if face is in multiple layers it will still be visible,
	 * unless all 'notlay' bits match the face bits.
	 * face: 0110
	 * not:  0100
	 * ~not: 1011
	 * now (face & ~not) is true
	 */
	
	notlay = ~notlay;
	
	for (obr = re->objecttable.first; obr; obr = obr->next) {
		if ((obr->lay & notlay) == 0) {
			for (a = 0; a < obr->totvlak; a++) {
				if ((a & 255) == 0) vlr = obr->vlaknodes[a >> 8].vlak;
				else vlr++;

				vlr->flag |= R_HIDDEN;
			}
		}
	}
}

static void env_hideobject(Render *re, Object *ob)
{
	ObjectRen *obr;
	VlakRen *vlr = NULL;
	int a;
	
	for (obr = re->objecttable.first; obr; obr = obr->next) {
		for (a = 0; a < obr->totvlak; a++) {
			if ((a & 255) == 0) vlr = obr->vlaknodes[a >> 8].vlak;
			else vlr++;

			if (obr->ob == ob)
				vlr->flag |= R_HIDDEN;
		}
	}
}

static void env_showobjects(Render *re)
{
	ObjectRen *obr;
	VlakRen *vlr = NULL;
	int a;
	
	for (obr = re->objecttable.first; obr; obr = obr->next) {
		for (a = 0; a < obr->totvlak; a++) {
			if ((a & 255) == 0) vlr = obr->vlaknodes[a >> 8].vlak;
			else vlr++;

			vlr->flag &= ~R_HIDDEN;
		}
	}
}

/* ------------------------------------------------------------------------- */

static void render_envmap(Render *re, EnvMap *env)
{
	/* only the cubemap and planar map is implemented */
	Render *envre;
	ImBuf *ibuf;
	float orthmat[4][4];
	float oldviewinv[4][4], mat[4][4], tmat[4][4];
	short part;
	
	/* need a recalc: ortho-render has no correct viewinv */
	invert_m4_m4(oldviewinv, re->viewmat);

	envre = envmap_render_copy(re, env);
	
	/* precalc orthmat for object */
	copy_m4_m4(orthmat, env->object->obmat);
	normalize_m4(orthmat);
	
	/* need imat later for texture imat */
	mul_m4_m4m4(mat, re->viewmat, orthmat);
	invert_m4_m4(tmat, mat);
	copy_m3_m4(env->obimat, tmat);

	for (part = 0; part < 6; part++) {
		if (env->type == ENV_PLANE && part != 1)
			continue;
		
		re->display_clear(re->dch, envre->result);
		
		copy_m4_m4(tmat, orthmat);
		envmap_transmatrix(tmat, part);
		invert_m4_m4(mat, tmat);
		/* mat now is the camera 'viewmat' */

		copy_m4_m4(envre->viewmat, mat);
		copy_m4_m4(envre->viewinv, tmat);
		
		/* we have to correct for the already rotated vertexcoords */
		mul_m4_m4m4(tmat, envre->viewmat, oldviewinv);
		invert_m4_m4(env->imat, tmat);
		
		env_rotate_scene(envre, tmat, 1);
		project_renderdata(envre, projectverto, 0, 0, 1);
		env_layerflags(envre, env->notlay);
		env_hideobject(envre, env->object);
				
		if (re->test_break(re->tbh) == 0) {
			RE_TileProcessor(envre);
		}
		
		/* rotate back */
		env_showobjects(envre);
		env_rotate_scene(envre, tmat, 0);

		if (re->test_break(re->tbh) == 0) {
			int y;
			float *alpha;
			float *rect;

			if (envre->result->do_exr_tile) {
				BLI_rw_mutex_lock(&envre->resultmutex, THREAD_LOCK_WRITE);
				render_result_exr_file_end(envre);
				BLI_rw_mutex_unlock(&envre->resultmutex);
			}

			RenderLayer *rl = envre->result->layers.first;

			/* envmap is rendered independently of multiview  */
			rect = RE_RenderLayerGetPass(rl, RE_PASSNAME_COMBINED, "");
			ibuf = IMB_allocImBuf(envre->rectx, envre->recty, 24, IB_rect | IB_rectfloat);
			memcpy(ibuf->rect_float, rect, ibuf->channels * ibuf->x * ibuf->y * sizeof(float));
			
			/* envmap renders without alpha */
			alpha = ibuf->rect_float + 3;
			for (y = ibuf->x * ibuf->y - 1; y >= 0; y--, alpha += 4)
				*alpha = 1.0;
			
			env->cube[part] = ibuf;
		}
		
		if (re->test_break(re->tbh)) break;

	}
	
	if (re->test_break(re->tbh)) BKE_texture_envmap_free_data(env);
	else {
		if (envre->r.mode & R_OSA) env->ok = ENV_OSA;
		else env->ok = ENV_NORMAL;
		env->lastframe = re->scene->r.cfra;
	}
	
	/* restore */
	envmap_free_render_copy(envre);
	env_set_imats(re);

}

/* ------------------------------------------------------------------------- */

void make_envmaps(Render *re)
{
	Tex *tex;
	bool do_init = false;
	int depth = 0, trace;
	
	if (!(re->r.mode & R_ENVMAP)) return;
	
	/* we don't raytrace, disabling the flag will cause ray_transp render solid */
	trace = (re->r.mode & R_RAYTRACE);
	re->r.mode &= ~R_RAYTRACE;

	re->i.infostr = IFACE_("Creating Environment maps");
	re->stats_draw(re->sdh, &re->i);
	
	/* 5 = hardcoded max recursion level */
	while (depth < 5) {
		tex = re->main->tex.first;
		while (tex) {
			if (tex->id.us && tex->type == TEX_ENVMAP) {
				if (tex->env && tex->env->object) {
					EnvMap *env = tex->env;
					
					if (env->object->lay & re->lay) {
						if (env->stype == ENV_LOAD) {
							float orthmat[4][4], mat[4][4], tmat[4][4];
							
							/* precalc orthmat for object */
							copy_m4_m4(orthmat, env->object->obmat);
							normalize_m4(orthmat);
							
							/* need imat later for texture imat */
							mul_m4_m4m4(mat, re->viewmat, orthmat);
							invert_m4_m4(tmat, mat);
							copy_m3_m4(env->obimat, tmat);
						}
						else {
							
							/* decide if to render an envmap (again) */
							if (env->depth >= depth) {
								
								/* set 'recalc' to make sure it does an entire loop of recalcs */
								
								if (env->ok) {
									/* free when OSA, and old one isn't OSA */
									if ((re->r.mode & R_OSA) && env->ok == ENV_NORMAL)
										BKE_texture_envmap_free_data(env);
									/* free when size larger */
									else if (env->lastsize < re->r.size)
										BKE_texture_envmap_free_data(env);
									/* free when env is in recalcmode */
									else if (env->recalc)
										BKE_texture_envmap_free_data(env);
								}
								
								if (env->ok == 0 && depth == 0) env->recalc = 1;
								
								if (env->ok == 0) {
									do_init = true;
									render_envmap(re, env);
									
									if (depth == env->depth) env->recalc = 0;
								}
							}
						}
					}
				}
			}
			tex = tex->id.next;
		}
		depth++;
	}

	if (do_init) {
		re->display_init(re->dih, re->result);
		re->display_clear(re->dch, re->result);
		// re->flag |= R_REDRAW_PRV;
	}
	/* restore */
	re->r.mode |= trace;

}

/* ------------------------------------------------------------------------- */

static int envcube_isect(EnvMap *env, const float vec[3], float answ[2])
{
	float lambda;
	int face;
	
	if (env->type == ENV_PLANE) {
		face = 1;
		
		lambda = 1.0f / vec[2];
		answ[0] = env->viewscale * lambda * vec[0];
		answ[1] = -env->viewscale * lambda * vec[1];
	}
	else {
		/* which face */
		if (vec[2] <= -fabsf(vec[0]) && vec[2] <= -fabsf(vec[1]) ) {
			face = 0;
			lambda = -1.0f / vec[2];
			answ[0] = lambda * vec[0];
			answ[1] = lambda * vec[1];
		}
		else if (vec[2] >= fabsf(vec[0]) && vec[2] >= fabsf(vec[1])) {
			face = 1;
			lambda = 1.0f / vec[2];
			answ[0] = lambda * vec[0];
			answ[1] = -lambda * vec[1];
		}
		else if (vec[1] >= fabsf(vec[0])) {
			face = 2;
			lambda = 1.0f / vec[1];
			answ[0] = lambda * vec[0];
			answ[1] = lambda * vec[2];
		}
		else if (vec[0] <= -fabsf(vec[1])) {
			face = 3;
			lambda = -1.0f / vec[0];
			answ[0] = lambda * vec[1];
			answ[1] = lambda * vec[2];
		}
		else if (vec[1] <= -fabsf(vec[0])) {
			face = 4;
			lambda = -1.0f / vec[1];
			answ[0] = -lambda * vec[0];
			answ[1] = lambda * vec[2];
		}
		else {
			face = 5;
			lambda = 1.0f / vec[0];
			answ[0] = -lambda * vec[1];
			answ[1] = lambda * vec[2];
		}
	}
	
	answ[0] = 0.5f + 0.5f * answ[0];
	answ[1] = 0.5f + 0.5f * answ[1];
	return face;
}

/* ------------------------------------------------------------------------- */

static void set_dxtdyt(float r_dxt[3], float r_dyt[3], const float dxt[3], const float dyt[3], int face)
{
	if (face == 2 || face == 4) {
		r_dxt[0] = dxt[0];
		r_dyt[0] = dyt[0];
		r_dxt[1] = dxt[2];
		r_dyt[1] = dyt[2];
	}
	else if (face == 3 || face == 5) {
		r_dxt[0] = dxt[1];
		r_dxt[1] = dxt[2];
		r_dyt[0] = dyt[1];
		r_dyt[1] = dyt[2];
	}
	else {
		r_dxt[0] = dxt[0];
		r_dyt[0] = dyt[0];
		r_dxt[1] = dxt[1];
		r_dyt[1] = dyt[1];
	}
}

/* ------------------------------------------------------------------------- */

int envmaptex(Tex *tex, const float texvec[3], float dxt[3], float dyt[3], int osatex, TexResult *texres, struct ImagePool *pool, const bool skip_load_image)
{
	extern Render R;                /* only in this call */
	/* texvec should be the already reflected normal */
	EnvMap *env;
	ImBuf *ibuf;
	float fac, vec[3], sco[3], dxts[3], dyts[3];
	int face, face1;
	
	env = tex->env;
	if (env == NULL || (env->stype != ENV_LOAD && env->object == NULL)) {
		texres->tin = 0.0;
		return 0;
	}
	
	if (env->stype == ENV_LOAD) {
		env->ima = tex->ima;
		if (env->ima && env->ima->ok) {
			if (env->cube[1] == NULL) {
				ImBuf *ibuf_ima = BKE_image_pool_acquire_ibuf(env->ima, NULL, pool);
				if (ibuf_ima)
					envmap_split_ima(env, ibuf_ima);
				else
					env->ok = 0;

				if (env->type == ENV_PLANE)
					tex->extend = TEX_EXTEND;

				BKE_image_pool_release_ibuf(env->ima, ibuf_ima, pool);
			}
		}
	}

	if (env->ok == 0) {
		texres->tin = 0.0;
		return 0;
	}
	
	/* rotate to envmap space, if object is set */
	copy_v3_v3(vec, texvec);
	if (env->object) {
		mul_m3_v3(env->obimat, vec);
		if (osatex) {
			mul_m3_v3(env->obimat, dxt);
			mul_m3_v3(env->obimat, dyt);
		}
	}
	else {
		if (!BKE_scene_use_world_space_shading(R.scene)) {
			// texvec is in view space
			mul_mat3_m4_v3(R.viewinv, vec);
			if (osatex) {
				mul_mat3_m4_v3(R.viewinv, dxt);
				mul_mat3_m4_v3(R.viewinv, dyt);
			}
		}
	}
	
	face = envcube_isect(env, vec, sco);
	ibuf = env->cube[face];
	
	if (osatex) {
		set_dxtdyt(dxts, dyts, dxt, dyt, face);
		imagewraposa(tex, NULL, ibuf, sco, dxts, dyts, texres, pool, skip_load_image);
		
		/* edges? */
		
		if (texres->ta < 1.0f) {
			TexResult texr1, texr2;
	
			texr1.nor = texr2.nor = NULL;
			texr1.talpha = texr2.talpha = texres->talpha; /* boxclip expects this initialized */

			add_v3_v3(vec, dxt);
			face1 = envcube_isect(env, vec, sco);
			sub_v3_v3(vec, dxt);
			
			if (face != face1) {
				ibuf = env->cube[face1];
				set_dxtdyt(dxts, dyts, dxt, dyt, face1);
				imagewraposa(tex, NULL, ibuf, sco, dxts, dyts, &texr1, pool, skip_load_image);
			}
			else texr1.tr = texr1.tg = texr1.tb = texr1.ta = 0.0;
			
			/* here was the nasty bug! results were not zero-ed. FPE! */
			
			add_v3_v3(vec, dyt);
			face1 = envcube_isect(env, vec, sco);
			sub_v3_v3(vec, dyt);
			
			if (face != face1) {
				ibuf = env->cube[face1];
				set_dxtdyt(dxts, dyts, dxt, dyt, face1);
				imagewraposa(tex, NULL, ibuf, sco, dxts, dyts, &texr2, pool, skip_load_image);
			}
			else texr2.tr = texr2.tg = texr2.tb = texr2.ta = 0.0;
			
			fac = (texres->ta + texr1.ta + texr2.ta);
			if (fac != 0.0f) {
				fac = 1.0f / fac;

				texres->tr = fac * (texres->ta * texres->tr + texr1.ta * texr1.tr + texr2.ta * texr2.tr);
				texres->tg = fac * (texres->ta * texres->tg + texr1.ta * texr1.tg + texr2.ta * texr2.tg);
				texres->tb = fac * (texres->ta * texres->tb + texr1.ta * texr1.tb + texr2.ta * texr2.tb);
			}
			texres->ta = 1.0;
		}
	}
	else {
		imagewrap(tex, NULL, ibuf, sco, texres, pool, skip_load_image);
	}
	
	return 1;
}
