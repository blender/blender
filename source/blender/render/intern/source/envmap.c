
/*  envmap.c        RENDER
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
 * 
 *  may 1999
 * 
 * $Id$
 */

#include <math.h>
#include <string.h>

#ifdef HAVE_CONFIG_H
#include <config.h>
#endif

/* external modules: */
#include "MEM_guardedalloc.h"
#include "BLI_arithb.h"
#include "BLI_blenlib.h"
#include "BKE_utildefines.h"

#include "IMB_imbuf_types.h"
#include "IMB_imbuf.h"        /* for rectcpy */

#include "DNA_texture_types.h"
#include "DNA_image_types.h"
#include "DNA_object_types.h"
#include "DNA_scene_types.h"
#include "DNA_texture_types.h"

#include "BKE_library.h"
#include "BKE_main.h"
#include "BKE_global.h"
#include "BKE_world.h"   // init_render_world
#include "BKE_image.h"   // BKE_write_ibuf 

#include "MTC_matrixops.h"

#include "SDL_thread.h"
#undef main 
#define main main /* stupid SDL_main redefines main as SDL_main */

/* this module */
#include "RE_callbacks.h"
#include "render.h"
#include "envmap.h"
#include "mydevice.h"
#include "rendercore.h" 
#include "renderHelp.h"
#include "texture.h"
#include "zbuf.h"


/* ------------------------------------------------------------------------- */

EnvMap *RE_add_envmap(void)
{
	EnvMap *env;
	
	env= MEM_callocN(sizeof(EnvMap), "envmap");
	env->type= ENV_CUBE;
	env->stype= ENV_STATIC;
	env->clipsta= 0.1;
	env->clipend= 100.0;
	env->cuberes= 100;
	
	return env;
} /* end of EnvMap *RE_add_envmap() */

/* ------------------------------------------------------------------------- */

EnvMap *RE_copy_envmap(EnvMap *env)
{
	EnvMap *envn;
	int a;
	
	envn= MEM_dupallocN(env);
	envn->ok= 0;
	for(a=0; a<6; a++) envn->cube[a]= 0;
	if(envn->ima) id_us_plus((ID *)envn->ima);
	
	return envn;
}

/* ------------------------------------------------------------------------- */

void RE_free_envmapdata(EnvMap *env)
{
	Image *ima;
	unsigned int a, part;
	
	for(part=0; part<6; part++) {
		ima= env->cube[part];
		if(ima) {
			if(ima->ibuf) IMB_freeImBuf(ima->ibuf);

			for(a=0; a<BLI_ARRAY_NELEMS(ima->mipmap); a++) {
				if(ima->mipmap[a]) IMB_freeImBuf(ima->mipmap[a]);
			}
			MEM_freeN(ima);
			env->cube[part]= 0;
		}
	}
	env->ok= 0;
}

/* ------------------------------------------------------------------------- */

void RE_free_envmap(EnvMap *env)
{
	
	RE_free_envmapdata(env);
	MEM_freeN(env);
	
}

/* ------------------------------------------------------------------------- */

static void envmap_split_ima(EnvMap *env)
{
	ImBuf *ibuf;
	Image *ima;
/*  	extern rectcpy(); */
	int dx, part;
	
	RE_free_envmapdata(env);	
	
	dx= env->ima->ibuf->y;
	dx/= 2;
	if(3*dx != env->ima->ibuf->x) {
		printf("Incorrect envmap size\n");
		env->ok= 0;
		env->ima->ok= 0;
	}
	else {
		for(part=0; part<6; part++) {
			ibuf= IMB_allocImBuf(dx, dx, 24, IB_rect, 0);
			ima= MEM_callocN(sizeof(Image), "image");
			ima->ibuf= ibuf;
			ima->ok= 1;
			env->cube[part]= ima;
		}
		IMB_rectop(env->cube[0]->ibuf, env->ima->ibuf, 
			0, 0, 0, 0, dx, dx, IMB_rectcpy, 0);
		IMB_rectop(env->cube[1]->ibuf, env->ima->ibuf, 
			0, 0, dx, 0, dx, dx, IMB_rectcpy, 0);
		IMB_rectop(env->cube[2]->ibuf, env->ima->ibuf, 
			0, 0, 2*dx, 0, dx, dx, IMB_rectcpy, 0);
		IMB_rectop(env->cube[3]->ibuf, env->ima->ibuf, 
			0, 0, 0, dx, dx, dx, IMB_rectcpy, 0);
		IMB_rectop(env->cube[4]->ibuf, env->ima->ibuf, 
			0, 0, dx, dx, dx, dx, IMB_rectcpy, 0);
		IMB_rectop(env->cube[5]->ibuf, env->ima->ibuf, 
			0, 0, 2*dx, dx, dx, dx, IMB_rectcpy, 0);
		env->ok= 2;
	}
}

/* ------------------------------------------------------------------------- */
/* ****************** RENDER ********************** */

static void envmap_renderdata(EnvMap *env)
{
	extern void init_filt_mask(void);
	static RE_Render envR;
	static Object *camera;
	int cuberes;
	
	if(env) {
		envR= R;
		camera= G.scene->camera;
		
		cuberes = (env->cuberes * R.r.size) / 100;
		cuberes &= 0xFFFC;
		env->lastsize= R.r.size;
		R.rectx= R.r.xsch= R.recty= R.r.ysch= cuberes;
		R.afmx= R.afmy= R.r.xsch/2;
		R.xstart= R.ystart= -R.afmx;
		R.xend= R.yend= R.xstart+R.rectx-1;

		R.r.mode &= ~(R_BORDER | R_PANORAMA | R_ORTHO | R_MBLUR | R_GAUSS);
		R.r.xparts= R.r.yparts= 1;
		R.r.bufflag= 0;
		R.r.size= 100;
		R.ycor= 1.0; 
		R.r.yasp= R.r.xasp= 1;
		
		R.near= env->clipsta;
		R.far= env->clipend;
		
		G.scene->camera= env->object;
		
	}
	else {
		/* this to make sure init_renderdisplay works */
		envR.winx= R.winx;
		envR.winy= R.winy;
		envR.winxof= R.winxof;
		envR.winyof= R.winyof;
		
		R= envR;
		G.scene->camera= camera;
	}
	
	/* gauss, gamma, etc */
	init_filt_mask();
}

/* ------------------------------------------------------------------------- */

static void envmap_transmatrix(float mat[][4], int part)
{
	float tmat[4][4], eul[3], rotmat[4][4];
	
	eul[0]= eul[1]= eul[2]= 0.0;
	
	if(part==0) {			/* neg z */
		;
	} else if(part==1) {	/* pos z */
		eul[0]= M_PI;
	} else if(part==2) {	/* pos y */
		eul[0]= M_PI/2.0;
	} else if(part==3) {	/* neg x */
		eul[0]= M_PI/2.0;
		eul[2]= M_PI/2.0;
	} else if(part==4) {	/* neg y */
		eul[0]= M_PI/2.0;
		eul[2]= M_PI;
	} else {				/* pos x */
		eul[0]= M_PI/2.0;
		eul[2]= -M_PI/2.0;
	}
	
	MTC_Mat4CpyMat4(tmat, mat);
	EulToMat4(eul, rotmat);
	MTC_Mat4MulSerie(mat, tmat, rotmat,
					 0,   0,    0,
					 0,   0,    0);
}

/* ------------------------------------------------------------------------- */

static void env_rotate_scene(float mat[][4], int mode)
{
	VlakRen *vlr = NULL;
	VertRen *ver = NULL;
	LampRen *lar = NULL;
	HaloRen *har = NULL;
	float xn, yn, zn, imat[3][3], pmat[4][4], smat[4][4], tmat[4][4], cmat[3][3];
	int a;
	
	if(mode==0) {
		MTC_Mat4Invert(tmat, mat);
		MTC_Mat3CpyMat4(imat, tmat);
	}
	else {
		MTC_Mat4CpyMat4(tmat, mat);
		MTC_Mat3CpyMat4(imat, mat);
	}
	
	for(a=0; a<R.totvert; a++) {
		if((a & 255)==0) ver= R.blove[a>>8];
		else ver++;
		
		MTC_Mat4MulVecfl(tmat, ver->co);
		
		xn= ver->n[0];
		yn= ver->n[1];
		zn= ver->n[2];
		/* no transpose ! */
		ver->n[0]= imat[0][0]*xn+imat[1][0]*yn+imat[2][0]*zn;
		ver->n[1]= imat[0][1]*xn+imat[1][1]*yn+imat[2][1]*zn;
		ver->n[2]= imat[0][2]*xn+imat[1][2]*yn+imat[2][2]*zn;
		Normalise(ver->n);
	}
	
	for(a=0; a<R.tothalo; a++) {
		if((a & 255)==0) har= R.bloha[a>>8];
		else har++;
	
		MTC_Mat4MulVecfl(tmat, har->co);
	}
		
	for(a=0; a<R.totvlak; a++) {
		if((a & 255)==0) vlr= R.blovl[a>>8];
		else vlr++;
		
		xn= vlr->n[0];
		yn= vlr->n[1];
		zn= vlr->n[2];
		/* no transpose ! */
		vlr->n[0]= imat[0][0]*xn+imat[1][0]*yn+imat[2][0]*zn;
		vlr->n[1]= imat[0][1]*xn+imat[1][1]*yn+imat[2][1]*zn;
		vlr->n[2]= imat[0][2]*xn+imat[1][2]*yn+imat[2][2]*zn;
		Normalise(vlr->n);
	}
	
	set_normalflags();
	
	for(a=0; a<R.totlamp; a++) {
		lar= R.la[a];
		
		/* removed here some horrible code of someone in NaN who tried to fix
		   prototypes... just solved by introducing a correct cmat[3][3] instead
		   of using smat. this works, check square spots in reflections  (ton) */
		Mat3CpyMat3(cmat, lar->imat); 
		Mat3MulMat3(lar->imat, cmat, imat); 

		MTC_Mat3MulVecfl(imat, lar->vec);
		MTC_Mat4MulVecfl(tmat, lar->co);

		lar->sh_invcampos[0]= -lar->co[0];
		lar->sh_invcampos[1]= -lar->co[1];
		lar->sh_invcampos[2]= -lar->co[2];
		MTC_Mat3MulVecfl(lar->imat, lar->sh_invcampos);
		lar->sh_invcampos[2]*= lar->sh_zfac;
		
		if(lar->shb) {
			if(mode==1) {
				MTC_Mat4Invert(pmat, mat);
				MTC_Mat4MulMat4(smat, pmat, lar->shb->viewmat);
				MTC_Mat4MulMat4(lar->shb->persmat, smat, lar->shb->winmat);
			}
			else MTC_Mat4MulMat4(lar->shb->persmat, lar->shb->viewmat, lar->shb->winmat);
		}
	}
	
}

/* ------------------------------------------------------------------------- */

static void env_layerflags(unsigned int notlay)
{
	VlakRen *vlr = NULL;
	int a;
	
	for(a=0; a<R.totvlak; a++) {
		if((a & 255)==0) vlr= R.blovl[a>>8];
		else vlr++;
		if(vlr->lay & notlay) vlr->flag &= ~R_VISIBLE;
	}
}

static void env_hideobject(Object *ob)
{
	VlakRen *vlr = NULL;
	int a;
	
	for(a=0; a<R.totvlak; a++) {
		if((a & 255)==0) vlr= R.blovl[a>>8];
		else vlr++;
		if(vlr->ob == ob) vlr->flag &= ~R_VISIBLE;
	}
}

/* ------------------------------------------------------------------------- */

static void env_set_imats()
{
	Base *base;
	float mat[4][4];
	
	base= G.scene->base.first;
	while(base) {
		MTC_Mat4MulMat4(mat, base->object->obmat, R.viewmat);
		MTC_Mat4Invert(base->object->imat, mat);
		
		base= base->next;
	}

}	

/* ------------------------------------------------------------------------- */

static void render_envmap(EnvMap *env)
{
	/* only the cubemap is implemented */
	ImBuf *ibuf;
	Image *ima;
	float oldviewinv[4][4], mat[4][4], tmat[4][4];
	short part;
	
	/* need a recalc: ortho-render has no correct viewinv */
	MTC_Mat4Invert(oldviewinv, R.viewmat);

	/* do first, envmap_renderdata copies entire R struct */
	if(R.rectz) MEM_freeN(R.rectz); R.rectz= NULL;
	if(R.rectot) MEM_freeN(R.rectot); R.rectot= NULL;
	if(R.rectftot) MEM_freeN(R.rectftot); R.rectftot= NULL;
	
	/* setup necessary globals */
	envmap_renderdata(env);
	
	RE_local_init_render_display();

	R.rectot= MEM_mallocN(sizeof(int)*R.rectx*R.recty, "rectot");
	R.rectz=  MEM_mallocN(sizeof(int)*R.rectx*R.recty, "rectz");

	for(part=0; part<6; part++) {

		RE_local_clear_render_display(R.win);
		fillrect(R.rectot, R.rectx, R.recty, 0);
		
		RE_setwindowclip(1,-1); /*  no jit:(-1) */
		
		MTC_Mat4CpyMat4(tmat, G.scene->camera->obmat);
		MTC_Mat4Ortho(tmat);
		envmap_transmatrix(tmat, part);
		MTC_Mat4Invert(mat, tmat);
		/* mat now is the camera 'viewmat' */

		MTC_Mat4CpyMat4(R.viewmat, mat);
		MTC_Mat4CpyMat4(R.viewinv, tmat);
		
		/* we have to correct for the already rotated vertexcoords */
		MTC_Mat4MulMat4(tmat, oldviewinv, R.viewmat);
		MTC_Mat4Invert(env->imat, tmat);
		
		env_rotate_scene(tmat, 1);
		init_render_world();
		setzbufvlaggen(RE_projectverto);
		env_layerflags(env->notlay);
		env_hideobject(env->object);
		env_set_imats();
				
		if(RE_local_test_break()==0) {

			RE_local_printrenderinfo(0.0, part);

			if(R.r.mode & R_OSA) zbufshadeDA();
			else zbufshade();

		}
		
		/* rotate back */
		env_rotate_scene(tmat, 0);

		if(RE_local_test_break()==0) {
			ibuf= IMB_allocImBuf(R.rectx, R.recty, 24, IB_rect, 0);
			ima= MEM_callocN(sizeof(Image), "image");
			memcpy(ibuf->rect, R.rectot, 4*ibuf->x*ibuf->y);
			ima->ibuf= ibuf;
			ima->ok= 1;
			env->cube[part]= ima;
		}
		
		if(RE_local_test_break()) break;

	}
	
	if(R.rectz) MEM_freeN(R.rectz); R.rectz= NULL;
	if(R.rectot) MEM_freeN(R.rectot); R.rectot= NULL;
	if(R.rectftot) MEM_freeN(R.rectftot); R.rectftot= NULL;
	
	if(RE_local_test_break()) RE_free_envmapdata(env);
	else {
		if(R.r.mode & R_OSA) env->ok= ENV_OSA;
		else env->ok= ENV_NORMAL;
		env->lastframe= G.scene->r.cfra;
	}
	
	/* restore */
	envmap_renderdata(0);
	env_set_imats();
	init_render_world();

}

/* ------------------------------------------------------------------------- */

void make_envmaps()
{
	Tex *tex;
	int do_init= 0, depth= 0, trace;
	
	if (!(R.r.mode & R_ENVMAP)) return;
	
	/* we dont raytrace, disabling the flag will cause ray_transp render solid */
	trace= (R.r.mode & R_RAYTRACE);
	R.r.mode &= ~R_RAYTRACE;

	/* 5 = hardcoded max recursion level */
	while(depth<5) {
		tex= G.main->tex.first;
		while(tex) {
			if(tex->id.us && tex->type==TEX_ENVMAP) {
				if(tex->env && tex->env->object) {
					if(tex->env->object->lay & G.scene->lay) {
						if(tex->env->stype!=ENV_LOAD) {
							
							/* decide if to render an envmap (again) */
							if(tex->env->depth >= depth) {
								
								/* set 'recalc' to make sure it does an entire loop of recalcs */
								
								if(tex->env->ok) {
										/* free when OSA, and old one isn't OSA */
									if((R.r.mode & R_OSA) && tex->env->ok==ENV_NORMAL) 
										RE_free_envmapdata(tex->env);
										/* free when size larger */
									else if(tex->env->lastsize < R.r.size) 
										RE_free_envmapdata(tex->env);
										/* free when env is in recalcmode */
									else if(tex->env->recalc)
										RE_free_envmapdata(tex->env);
								}
								
								if(tex->env->ok==0 && depth==0) tex->env->recalc= 1;
								
								if(tex->env->ok==0) {
									do_init= 1;
									render_envmap(tex->env);
									
									if(depth==tex->env->depth) tex->env->recalc= 0;
								}
							}
						}
					}
				}
			}
			tex= tex->id.next;
		}
		depth++;
	}

	if(do_init) {
		RE_local_init_render_display();
		RE_local_clear_render_display(R.win);
		R.flag |= R_REDRAW_PRV;
	}	
	// restore
	R.r.mode |= trace;

}

/* ------------------------------------------------------------------------- */

static int envcube_isect(float *vec, float *answ)
{
	float labda;
	int face;
	
	/* which face */
	if( vec[2]<=-fabs(vec[0]) && vec[2]<=-fabs(vec[1]) ) {
		face= 0;
		labda= -1.0/vec[2];
		answ[0]= labda*vec[0];
		answ[1]= labda*vec[1];
	}
	else if( vec[2]>=fabs(vec[0]) && vec[2]>=fabs(vec[1]) ) {
		face= 1;
		labda= 1.0/vec[2];
		answ[0]= labda*vec[0];
		answ[1]= -labda*vec[1];
	}
	else if( vec[1]>=fabs(vec[0]) ) {
		face= 2;
		labda= 1.0/vec[1];
		answ[0]= labda*vec[0];
		answ[1]= labda*vec[2];
	}
	else if( vec[0]<=-fabs(vec[1]) ) {
		face= 3;
		labda= -1.0/vec[0];
		answ[0]= labda*vec[1];
		answ[1]= labda*vec[2];
	}
	else if( vec[1]<=-fabs(vec[0]) ) {
		face= 4;
		labda= -1.0/vec[1];
		answ[0]= -labda*vec[0];
		answ[1]= labda*vec[2];
	}
	else {
		face= 5;
		labda= 1.0/vec[0];
		answ[0]= -labda*vec[1];
		answ[1]= labda*vec[2];
	}
	answ[0]= 0.5+0.5*answ[0];
	answ[1]= 0.5+0.5*answ[1];
	return face;
}

/* ------------------------------------------------------------------------- */

static void set_dxtdyt(float *dxts, float *dyts, float *dxt, float *dyt, int face)
{
	if(face==2 || face==4) {
		dxts[0]= dxt[0];
		dyts[0]= dyt[0];
		dxts[1]= dxt[2];
		dyts[1]= dyt[2];
	}
	else if(face==3 || face==5) {
		dxts[0]= dxt[1];
		dxts[1]= dxt[2];
		dyts[0]= dyt[1];
		dyts[1]= dyt[2];
	}
	else {
		dxts[0]= dxt[0];
		dyts[0]= dyt[0];
		dxts[1]= dxt[1];
		dyts[1]= dyt[1];
	}
}

/* ------------------------------------------------------------------------- */

int envmaptex(Tex *tex, float *texvec, float *dxt, float *dyt, int osatex, TexResult *texres)
{
	extern SDL_mutex *load_ibuf_lock; // initrender.c
	/* texvec should be the already reflected normal */
	EnvMap *env;
	Image *ima;
	float fac, vec[3], sco[3], dxts[3], dyts[3];
	int face, face1;
	
	env= tex->env;
	if(env==NULL || (env->stype!=ENV_LOAD && env->object==NULL)) {
		texres->tin= 0.0;
		return 0;
	}
	if(env->stype==ENV_LOAD) {
		env->ima= tex->ima;
		if(env->ima && env->ima->ok) {
			// Now thread safe
			if(load_ibuf_lock) SDL_mutexP(load_ibuf_lock);
			if(env->ima->ibuf==NULL) ima_ibuf_is_nul(tex, tex->ima);
			if(load_ibuf_lock) SDL_mutexV(load_ibuf_lock);
			if(env->ima->ok && env->ok==0) envmap_split_ima(env);
		}
	}

	if(env->ok==0) {
		
		texres->tin= 0.0;
		return 0;
	}
	
	/* rotate to envmap space, if object is set */
	VECCOPY(vec, texvec);
	if(env->object) MTC_Mat4Mul3Vecfl(env->object->imat, vec);
	else MTC_Mat4Mul3Vecfl(R.viewinv, vec);
	
	face= envcube_isect(vec, sco);
	ima= env->cube[face];
	
	if(osatex) {
		if(env->object) {
			MTC_Mat4Mul3Vecfl(env->object->imat, dxt);
			MTC_Mat4Mul3Vecfl(env->object->imat, dyt);
		}
		else {
			MTC_Mat4Mul3Vecfl(R.viewinv, dxt);
			MTC_Mat4Mul3Vecfl(R.viewinv, dyt);
		}
		set_dxtdyt(dxts, dyts, dxt, dyt, face);
		imagewraposa(tex, ima, sco, dxts, dyts, texres);
		
		/* edges? */
		
		if(texres->ta<1.0) {
			TexResult texr1, texr2;
	
			texr1.nor= texr2.nor= NULL;

			VecAddf(vec, vec, dxt);
			face1= envcube_isect(vec, sco);
			VecSubf(vec, vec, dxt);
			
			if(face!=face1) {
				ima= env->cube[face1];
				set_dxtdyt(dxts, dyts, dxt, dyt, face1);
				imagewraposa(tex, ima, sco, dxts, dyts, &texr1);
			}
			else texr1.tr= texr1.tg= texr1.tb= texr1.ta= 0.0;
			
			/* here was the nasty bug! results were not zero-ed. FPE! */
			
			VecAddf(vec, vec, dyt);
			face1= envcube_isect(vec, sco);
			VecSubf(vec, vec, dyt);
			
			if(face!=face1) {
				ima= env->cube[face1];
				set_dxtdyt(dxts, dyts, dxt, dyt, face1);
				imagewraposa(tex, ima, sco, dxts, dyts, &texr2);
			}
			else texr2.tr= texr2.tg= texr2.tb= texr2.ta= 0.0;
			
			fac= (texres->ta+texr1.ta+texr2.ta);
			if(fac!=0.0) {
				fac= 1.0/fac;
				
				texres->tr= fac*(texres->ta*texres->tr + texr1.ta*texr1.tr + texr2.ta*texr2.tr );
				texres->tg= fac*(texres->ta*texres->tg + texr1.ta*texr1.tg + texr2.ta*texr2.tg );
				texres->tb= fac*(texres->ta*texres->tb + texr1.ta*texr1.tb + texr2.ta*texr2.tb );
			}
			texres->ta= 1.0;
		}
	}
	else {
		imagewrap(tex, ima, sco, texres);
	}
	
	return 1;
}

/* ------------------------------------------------------------------------- */

/* eof */
