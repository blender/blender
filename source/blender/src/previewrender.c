/* previewrender.c  		GRAPHICS
 * 
 * maart 95
 * 
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
#include "BKE_utildefines.h"

#include "MTC_matrixops.h"

#include "render.h"
#include "mydevice.h"

#include "DNA_texture_types.h"
#include "DNA_world_types.h"
#include "DNA_camera_types.h"
#include "DNA_image_types.h"
#include "DNA_object_types.h"
#include "DNA_lamp_types.h"
#include "DNA_space_types.h"
#include "DNA_scene_types.h"
#include "DNA_screen_types.h"

#include "BKE_global.h"
#include "BKE_image.h"
#include "BKE_texture.h"
#include "BKE_material.h"
#include "BKE_world.h"
#include "BKE_texture.h"

#include "BSE_headerbuttons.h"

#include "BIF_gl.h"
#include "BIF_screen.h"
#include "BIF_space.h"		/* allqueue */
#include "BIF_butspace.h"	
#include "BIF_drawimage.h"	/* rectwrite_part */
#include "BIF_mywindow.h"
#include "BIF_interface.h"
#include "BIF_glutil.h"

#include "PIL_time.h"

#include "RE_renderconverter.h"

#define PR_RECTX	141
#define PR_RECTY	141
#define PR_XMIN		10
#define PR_YMIN		5
#define PR_XMAX		200
#define PR_YMAX		195

#define PR_FACY		(PR_YMAX-PR_YMIN-4)/(PR_RECTY)

static rcti prerect;
static int pr_sizex, pr_sizey;
static float pr_facx, pr_facy;


/* implementation */

static short snijpunt(float *v1,   float *v2,  float *v3,  float *rtlabda, float *ray1, float *ray2)
{
	float x0,x1,x2,t00,t01,t02,t10,t11,t12,t20,t21,t22;
	float m0,m1,m2,deeldet,det1,det2,det3;
	float rtu, rtv;
	
	t00= v3[0]-v1[0];
	t01= v3[1]-v1[1];
	t02= v3[2]-v1[2];
	t10= v3[0]-v2[0];
	t11= v3[1]-v2[1];
	t12= v3[2]-v2[2];
	t20= ray1[0]-ray2[0];
	t21= ray1[1]-ray2[1];
	t22= ray1[2]-ray2[2];
	
	x0= t11*t22-t12*t21;
	x1= t12*t20-t10*t22;
	x2= t10*t21-t11*t20;

	deeldet= t00*x0+t01*x1+t02*x2;
	if(deeldet!=0.0) {
		m0= ray1[0]-v3[0];
		m1= ray1[1]-v3[1];
		m2= ray1[2]-v3[2];
		det1= m0*x0+m1*x1+m2*x2;
		rtu= det1/deeldet;
		if(rtu<=0.0) {
			det2= t00*(m1*t22-m2*t21);
			det2+= t01*(m2*t20-m0*t22);
			det2+= t02*(m0*t21-m1*t20);
			rtv= det2/deeldet;
			if(rtv<=0.0) {
				if(rtu+rtv>= -1.0) {
					
					det3=  m0*(t12*t01-t11*t02);
					det3+= m1*(t10*t02-t12*t00);
					det3+= m2*(t11*t00-t10*t01);
					*rtlabda= det3/deeldet;
					
					if(*rtlabda>=0.0 && *rtlabda<=1.0) {
						return 1;
					}
				}
			}
		}
	}
	return 0;
}

static float rcubev[7][3]= {
	{-0.002055,  6.627364, -3.369742}, 
	{-6.031684, -3.750204, -1.992980}, 
	{-6.049086,  3.817431,  1.969788}, 
	{ 6.031685,  3.833064,  1.992979}, 
	{ 6.049086, -3.734571, -1.969787}, 
	{ 0.002054, -6.544502,  3.369744}, 
	{-0.015348,  1.023131,  7.332510} };

static int rcubi[3][4]= {
	{3,  6,  5,  4},
	{1,  5,  6,  2},  
	{3,  0,  2,  6} };


static int ray_previewrender(int x,  int y,  float *vec, float *vn)
{
	float scalef= 10.0/100.0;
	float ray1[3], ray2[3];
	float minlabda, labda;
	int totface= 3, hitface= -1;
	int a;

	ray1[0]= ray2[0]= x*scalef;
	ray1[1]= ray2[1]= y*scalef;
	ray1[2]= -10.0;
	ray2[2]= 10.0;
	
	minlabda= 1.0;
	for(a=0; a<totface; a++) {
		if(snijpunt( rcubev[rcubi[a][0]], rcubev[rcubi[a][1]], rcubev[rcubi[a][2]], &labda, ray1, ray2)) {
			if( labda < minlabda) {
				minlabda= labda;
				hitface= a;
			}
		}
		if(snijpunt( rcubev[rcubi[a][0]], rcubev[rcubi[a][2]], rcubev[rcubi[a][3]], &labda, ray1, ray2)) {
			if( labda < minlabda) {
				minlabda= labda;
				hitface= a;
			}
		}
	}
	
	if(hitface > -1) {
		
		CalcNormFloat(rcubev[rcubi[hitface][0]], rcubev[rcubi[hitface][1]], rcubev[rcubi[hitface][2]], vn);
		
		vec[0]= (minlabda*(ray1[0]-ray2[0])+ray2[0])/3.7;
		vec[1]= (minlabda*(ray1[1]-ray2[1])+ray2[1])/3.7;
		vec[2]= (minlabda*(ray1[2]-ray2[2])+ray2[2])/3.7;
		
		return 1;
	}
	return 0;
}


static unsigned int previewback(int type, int x, int y)
{
	
	/* checkerboard, for later
	x+= PR_RECTX/2;
	y+= PR_RECTX/2;
	if( ((x/24) + (y/24)) & 1) return 0x40404040;
	else return 0xa0a0a0a0;
	*/
	
	if(type & MA_DARK) {
		if(abs(x)>abs(y)) return 0;
		else return 0x40404040;
	}
	else {
		if(abs(x)>abs(y)) return 0x40404040;
		else return 0xa0a0a0a0;
	}
}

static void view2d_to_window(int win, int *x_r, int *y_r)
{
	int x= *x_r, y= *y_r;
	int size[2], origin[2];
	float winmat[4][4];

	bwin_getsinglematrix(win, winmat);
	bwin_getsize(win, &size[0], &size[1]);
	bwin_getsuborigin(win, &origin[0], &origin[1]);
	
	*x_r= origin[0] + (size[0]*(0.5 + 0.5*(x*winmat[0][0] + y*winmat[1][0] + winmat[3][0])));
	*y_r= origin[1] + (size[1]*(0.5 + 0.5*(x*winmat[0][1] + y*winmat[1][1] + winmat[3][1])));
}

static void set_previewrect(int win, int xmin, int ymin, int xmax, int ymax)
{
	prerect.xmin= xmin;
	prerect.ymin= ymin;
	prerect.xmax= xmax;
	prerect.ymax= ymax;

	view2d_to_window(win, &prerect.xmin, &prerect.ymin);
	view2d_to_window(win, &prerect.xmax, &prerect.ymax);
	
	pr_sizex= (prerect.xmax-prerect.xmin);
	pr_sizey= (prerect.ymax-prerect.ymin);

	pr_facx= ( (float)pr_sizex-1)/PR_RECTX;
	pr_facy= ( (float)pr_sizey-1)/PR_RECTY;
}

static void display_pr_scanline(unsigned int *rect, int recty)
{
	static double lasttime= 0;
	/* we display 3 new scanlines, one old, the overlap is for wacky 3d cards that cant handle zoom proper */

	if(recty % 2) return;
	if(recty<2) return;
	
	rect+= (recty-2)*PR_RECTX;

	/* enlarge a bit in the y direction, to avoid GL/mesa bug */
	glPixelZoom(pr_facx, pr_facy);

	glRasterPos2f( (float)PR_XMIN+0.5, 1.0+(float)PR_YMIN + (recty*PR_FACY) );
	glDrawPixels(PR_RECTX, 3, GL_RGBA, GL_UNSIGNED_BYTE,  rect);

	//glaDrawPixelsTex((float)PR_XMIN, (float)PR_YMIN + (recty*PR_FACY), PR_RECTX, 3, rect);

	glPixelZoom(1.0, 1.0);
	
	/* flush opengl for cards with frontbuffer slowness */
	if(recty==PR_RECTY-1 || (PIL_check_seconds_timer() - lasttime > 0.05)) {
		lasttime= PIL_check_seconds_timer();
		glFinish();
	}
}

static void draw_tex_crop(Tex *tex)
{
	rcti rct;
	int ret= 0;
	
	if(tex==0) return;
	
	if(tex->type==TEX_IMAGE) {
		if(tex->cropxmin==0.0) ret++;
		if(tex->cropymin==0.0) ret++;
		if(tex->cropxmax==1.0) ret++;
		if(tex->cropymax==1.0) ret++;
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

void BIF_all_preview_changed(void)
{
	ScrArea *sa;
	SpaceButs *sbuts;
	
	sa= G.curscreen->areabase.first;
	while(sa) {
		if(sa->spacetype==SPACE_BUTS) {
			sbuts= sa->spacedata.first;
			sbuts->cury= 0;
			addafterqueue(sa->win, RENDERPREVIEW, 1);
		}
		sa= sa->next;
	}
}


void BIF_preview_changed(SpaceButs *sbuts)
{
	/* can be called when no buttonswindow visible */
	if(sbuts) {
		sbuts->cury= 0;
		addafterqueue(sbuts->area->win, RENDERPREVIEW, 1);
	}
}

/* is supposed to be called with correct panel offset matrix */
void BIF_previewdraw(void)
{
	SpaceButs *sbuts= curarea->spacedata.first;
	
	set_previewrect(sbuts->area->win, PR_XMIN, PR_YMIN, PR_XMAX, PR_YMAX);

	if (sbuts->rect==0) BIF_preview_changed(sbuts);
	else {
		int y;

		for (y=0; y<PR_RECTY; y++) {
			display_pr_scanline(sbuts->rect, y);
		}

		if (sbuts->mainb==CONTEXT_SHADING && sbuts->tab[CONTEXT_SHADING]==TAB_SHADING_TEX) {
			draw_tex_crop(sbuts->lockpoin);
		}
	}
	if(sbuts->cury==0) BIF_preview_changed(sbuts);
}

static void sky_preview_pixel(float lens, int x, int y, char *rect)
{
	float view[3];
	
	if(R.wrld.skytype & WO_SKYPAPER) {
		view[0]= (2*x)/(float)PR_RECTX;
		view[1]= (2*y)/(float)PR_RECTY;
		view[2]= 0.0;
	}
	else {
		view[0]= x;
		view[1]= y;
		view[2]= -lens*PR_RECTX/32.0;
		Normalise(view);
	}
	RE_sky(view, rect);
}

static void lamp_preview_pixel(ShadeInput *shi, LampRen *la, int x, int y, char *rect)
{
	float inpr, i, t, dist, distkw, vec[3];
	int col;
	
	shi->co[0]= (float)x/(PR_RECTX/4);
	shi->co[1]= (float)y/(PR_RECTX/4);
	shi->co[2]= 0;
	
	vec[0]= 0.02*x;
	vec[1]= 0.02*y;
	vec[2]= 0.005*PR_RECTX;
	VECCOPY(shi->view, vec);
	dist= Normalise(shi->view);

	if(la->mode & LA_TEXTURE) do_lamp_tex(la, vec, shi);

	if(la->type==LA_SUN || la->type==LA_HEMI) {
		dist= 1.0;
	}
	else {
		
		if(la->mode & LA_QUAD) {
			
			t= 1.0;
			if(la->ld1>0.0)
				t= la->dist/(la->dist+la->ld1*dist);
			if(la->ld2>0.0) {
				distkw= la->dist*la->dist;
				t= t*distkw/(t*distkw+la->ld2*dist*dist);
			}
			dist= t;
		}
		else {
			dist= (la->dist/(la->dist+dist));
		}
	}

	if(la->type==LA_SPOT) {

		
		if(la->mode & LA_SQUARE) {
			/* slightly smaller... */
			inpr= 1.7*cos(MAX2(fabs(shi->view[0]/shi->view[2]) , fabs(shi->view[1]/shi->view[2]) ));
		}
		else {
			inpr= shi->view[2];
		}
		
		t= la->spotsi;
		if(inpr<t) dist= 0.0;
		else {
			t= inpr-t;
			if(t<la->spotbl && la->spotbl!=0.0) {
				/* soft area */
				i= t/la->spotbl;
				t= i*i;
				i= t*i;
				inpr*=(3.0*t-2.0*i);
			}
		}
		dist*=inpr;
	}
	else if ELEM(la->type, LA_LOCAL, LA_AREA) dist*= shi->view[2];
	
	col= 255.0*dist*la->r;
	if(col<=0) rect[0]= 0; else if(col>=255) rect[0]= 255; else rect[0]= col;

	col= 255.0*dist*la->g;
	if(col<=0) rect[1]= 0; else if(col>=255) rect[1]= 255; else rect[1]= col;

	col= 255.0*dist*la->b;
	if(col<=0) rect[2]= 0; else if(col>=255) rect[2]= 255; else rect[2]= col;
}

static void init_previewhalo(HaloRen *har, Material *mat)
{
	
	har->type= 0;
	if(mat->mode & MA_HALO_XALPHA) har->type |= HA_XALPHA;
	har->mat= mat;
	har->hard= mat->har;
	har->rad= PR_RECTX/2.0;
	har->radsq= PR_RECTX*PR_RECTX/4.0;
	har->alfa= mat->alpha;
	har->add= 255.0*mat->add;
	har->r= 255.0*mat->r;
	har->g= 255.0*mat->g; 
	har->b= 255.0*mat->b;
	har->xs= PR_RECTX/2.0;
	har->ys= PR_RECTX/2.0;
	har->zs= har->zd= 0;
	har->seed= (mat->seed1 % 256);
	
	if( (mat->mode & MA_HALOTEX) && mat->mtex[0] ) har->tex= 1; else har->tex=0;

	if(mat->mode & MA_STAR) har->starpoints= mat->starc; else har->starpoints= 0;
	if(mat->mode & MA_HALO_LINES) har->linec= mat->linec; else har->linec= 0;
	if(mat->mode & MA_HALO_RINGS) har->ringc= mat->ringc; else har->ringc= 0;
	if(mat->mode & MA_HALO_FLARE) har->flarec= mat->flarec; else har->flarec= 0;
	
	if(har->flarec) {
		har->xs-= PR_RECTX/3;
		har->ys+= PR_RECTX/3;
		
		har->rad*= 0.3;
		har->radsq= har->rad*har->rad;
		
		har->pixels= har->rad*har->rad*har->rad;
	}
}	

static void halo_preview_pixel(HaloRen *har, int startx, int endx, int y, char *rect)
{
	float dist, xn, yn, xsq, ysq;
	int x;
	char front[4];
	
	if(har->flarec) yn= y-PR_RECTX/3;
	else yn= y;
	ysq= yn*yn;
	
	for(x=startx; x<endx; x++) {
		
		if(har->flarec) xn= x+PR_RECTX/3;
		else xn= x;
		
		xsq= xn*xn;
		dist= xsq+ysq;

		
		
		if(dist<har->radsq) {
			RE_shadehalo(har, front, 0, dist, xn, yn, har->flarec);
			RE_addalphaAddfac(rect, front, har->add);
		}
		rect+= 4;
	}
}

static void previewflare(SpaceButs *sbuts, HaloRen *har, unsigned int *rect)
{
	uiBlock *block;
	float ycor;
	unsigned int *rectot;
	int afmx, afmy, rectx, recty;
	
	block= uiFindOpenPanelBlockName(&curarea->uiblocks, "Preview");
	if(block==NULL) return;

	/* temps */
	ycor= R.ycor;
	rectx= R.rectx;
	recty= R.recty;
	afmx= R.afmx;
	afmy= R.afmy;
	rectot= R.rectot;

	R.ycor= 1.0;
	R.rectx= PR_RECTX;	
	R.recty= PR_RECTY;
	R.afmx= PR_RECTX/2;
	R.afmy= PR_RECTY/2;
	R.rectot= rect;

	waitcursor(1);
	RE_renderflare(har);
	waitcursor(0);
	// not sure why, either waitcursor or renderflare screws up
	areawinset(curarea->win);
	
	uiPanelPush(block);
	BIF_previewdraw();
	uiPanelPop(block);
	
	
	/* temps */
	R.ycor= ycor;
	R.rectx= rectx;
	R.recty= recty;
	R.afmx= afmx;
	R.afmy= afmy;
	R.rectot= rectot;
}

extern float Tin, Tr, Tg, Tb, Ta; /* texture.c */
static void texture_preview_pixel(Tex *tex, int x, int y, char *rect)
{
	float i, v1, xsq, ysq, texvec[3], dummy[3];
	int rgbnor, tracol, skip=0;
		
	if(tex->type==TEX_IMAGE) {
		v1= 1.0/PR_RECTX;
		
		texvec[0]= 0.5+v1*x;
		texvec[1]= 0.5+v1*y;
		
		/* no coordinate mapping, exception: repeat */
		if(tex->xrepeat>1) {
			texvec[0] *= tex->xrepeat;
			if(texvec[0]>1.0) texvec[0] -= (int)(texvec[0]);
		}
		if(tex->yrepeat>1) {
			texvec[1] *= tex->yrepeat;
			if(texvec[1]>1.0) texvec[1] -= (int)(texvec[1]);
		}

	}
	else if(tex->type==TEX_ENVMAP) {
		if(tex->env) {
			ysq= y*y;
			xsq= x*x;
			if(xsq+ysq < (PR_RECTX/2)*(PR_RECTY/2)) {
				texvec[2]= sqrt( (float)((PR_RECTX/2)*(PR_RECTY/2)-xsq-ysq) );
				texvec[0]= -x;
				texvec[1]= -y;
				Normalise(texvec);

				i= 2.0*(texvec[2]);
				texvec[0]= (i*texvec[0]);
				texvec[1]= (i*texvec[1]);
				texvec[2]= (-1.0+i*texvec[2]);

			}
			else {
				skip= 1;
				Ta= 0.0;
			}
		}
		else {
			skip= 1;
			Ta= 0.0;
		}
	}
	else {
		v1= 2.0/PR_RECTX;
	
		texvec[0]= v1*x;
		texvec[1]= v1*y;
		texvec[2]= 0.0;
	}
	
	/* does not return Tin */
	if(tex->type==TEX_STUCCI) {
		tex->nor= dummy;
		dummy[0]= 1.0;
		dummy[1]= dummy[2]= 0.0;
	}
	
	if(skip==0) rgbnor= multitex(tex, texvec, NULL, NULL, 0);
	else rgbnor= 1;
	
	if(rgbnor & 1) {
		
		rect[0]= 255.0*Tr;
		rect[1]= 255.0*Tg;
		rect[2]= 255.0*Tb;
		
		if(Ta!=1.0) {
			tracol=  64+100*(abs(x)>abs(y));
			tracol= (1.0-Ta)*tracol;
			
			rect[0]= tracol+ (rect[0]*Ta) ;
			rect[1]= tracol+ (rect[1]*Ta) ;
			rect[2]= tracol+ (rect[2]*Ta) ;
					
		}
	}
	else {
	
		if(tex->type==TEX_STUCCI) {
			Tin= 0.5 + 0.7*tex->nor[0];
			CLAMP(Tin, 0.0, 1.0);
		}
		rect[0]= 255.0*Tin;
		rect[1]= 255.0*Tin;
		rect[2]= 255.0*Tin;
	}
}

static float pr1_lamp[3]= {2.3, -2.4, -4.6};
static float pr2_lamp[3]= {-8.8, -5.6, -1.5};
static float pr1_col[3]= {0.8, 0.8, 0.8};
static float pr2_col[3]= {0.5, 0.6, 0.7};

static void refraction_prv(int *x, int *y, float *n, float index)
{
	float dot, fac, view[3], len;

	index= 1.0/index;
	
	view[0]= index*(float)*x;
	view[1]= ((float)*y)/index;
	view[2]= 20.0;
	len= Normalise(view);
	
	dot= view[0]*n[0] + view[1]*n[1] + view[2]*n[2];

	if(dot>0.0) {
		fac= 1.0 - (1.0 - dot*dot)*index*index;
		if(fac<= 0.0) return;
		fac= -dot*index + sqrt(fac);
	}
	else {
		index = 1.0/index;
		fac= 1.0 - (1.0 - dot*dot)*index*index;
		if(fac<= 0.0) return;
		fac= -dot*index - sqrt(fac);
	}

	*x= (int)(len*(index*view[0] + fac*n[0]));
	*y= (int)(len*(index*view[1] + fac*n[1]));
}


static void shade_preview_pixel(ShadeInput *shi, float *vec, int x, int y,char *rect, int smooth)
{
	extern float fresnel_fac(float *view, float *vn, float ior, float fac);
	Material *mat;
	float v1,inp, inprspec=0, isr=0.0, isb=0.0, isg=0.0;
	float ir=0.0, ib=0.0, ig=0.0;
	float view[3], lv[3], *la, alpha;
	float eul[3], tmat[3][3], imat[3][3];
	int temp, a;
	char tracol;
		
	mat= shi->matren;

	v1= 1.0/PR_RECTX;
	view[0]= v1*x;
	view[1]= v1*y;
	view[2]= 1.0;
	Normalise(view);
	
	shi->refcol[0]= shi->refcol[1]= shi->refcol[2]= shi->refcol[3]= 0.0;

	/* texture handling */
	if(mat->texco) {
		
		VECCOPY(shi->lo, vec);
		
		if(mat->pr_type==MA_CUBE) {
			
			eul[0]= (297)*M_PI/180.0;
			eul[1]= 0.0;
			eul[2]= (45)*M_PI/180.0;
			EulToMat3(eul, tmat);

			MTC_Mat3MulVecfl(tmat, shi->lo);
			MTC_Mat3MulVecfl(tmat, shi->vn);
			/* hack for cubemap, why!!! */
			SWAP(float, shi->vn[0], shi->vn[1]);
		}
		/* textures otherwise upside down */
		if(mat->pr_type==MA_CUBE || mat->pr_type==MA_SPHERE) 
			shi->lo[2]= -shi->lo[2];

		if(mat->texco & TEXCO_GLOB) {
			VECCOPY(shi->gl, shi->lo);
		}
		if(mat->texco & TEXCO_WINDOW) {
			VECCOPY(shi->winco, shi->lo);
		}
		if(mat->texco & TEXCO_STICKY) {
			VECCOPY(shi->sticky, shi->lo);
		}
		if(mat->texco & TEXCO_UV) {
			VECCOPY(shi->uv, shi->lo);
		}
		if(mat->texco & TEXCO_OBJECT) {
			VECCOPY(shi->co, shi->lo);
		}
		if(mat->texco & TEXCO_NORM) {
			shi->orn[0]= shi->vn[0];
			shi->orn[1]= shi->vn[1];
			shi->orn[2]= shi->vn[2];
		}
		if(mat->texco & TEXCO_REFL) {
			/* for bump texture */
			VECCOPY(shi->view, view);
			
			inp= -2.0*(shi->vn[0]*view[0]+shi->vn[1]*view[1]+shi->vn[2]*view[2]);
			shi->ref[0]= (view[0]+inp*shi->vn[0]);
			shi->ref[1]= (view[1]+inp*shi->vn[1]);
			shi->ref[2]= (view[2]+inp*shi->vn[2]);
		}

		/* Clear displase vec for preview */
		shi->displace[0]= shi->displace[1]= shi->displace[2]= 0.0;
		
		/* normals flipped in render... */
		if(mat->mapto & MAP_NORM) VecMulf(shi->vn, -1.0);
		
		do_material_tex(shi);

		/* normals flipped in render... */
		if(mat->mapto & MAP_NORM) VecMulf(shi->vn, -1.0);
	
		if(mat->texco & TEXCO_REFL) {
			/* normals in render are pointing different... rhm */
			if(smooth) shi->ref[1]= -shi->ref[1];
		}

		if(mat->pr_type==MA_CUBE) {
			/* rotate normal back for normals texture */
			SWAP(float, shi->vn[0], shi->vn[1]);
			MTC_Mat3Inv(imat, tmat);
			MTC_Mat3MulVecfl(imat, shi->vn);
		}
		
	}
	/* set it here, because ray_mirror will affect it */
	alpha= mat->alpha;

	if(mat->mapto & MAP_DISPLACE) { /* Quick hack of fake displacement preview */
		shi->vn[0]-=2.0*shi->displace[2];
		shi->vn[1]-=2.0*shi->displace[0];
		shi->vn[2]+=2.0*shi->displace[1];
		Normalise(shi->vn);
	}
		
	if(mat->mode & (MA_ZTRA|MA_RAYTRANSP)) 
		if(mat->fresnel_tra!=0.0) 
			alpha*= fresnel_fac(view, shi->vn, mat->fresnel_tra_i, mat->fresnel_tra);

	if(mat->mode & MA_SHLESS) {
		temp= 255.0*(mat->r);
		if(temp>255) rect[0]= 255; else if(temp<0) rect[0]= 0; else rect[0]= temp;

		temp= 255.0*(mat->g);
		if(temp>255) rect[1]= 255; else if(temp<0) rect[1]= 0; else rect[1]= temp;

		temp= 255.0*(mat->b);
		if(temp>255) rect[2]= 255; else if(temp<0) rect[2]= 0; else rect[2]= temp;
	}
	else {
		
		for(a=0; a<2; a++) {
			
			if(a==0) la= pr1_lamp;
			else la= pr2_lamp;
			
			lv[0]= vec[0]-la[0];
			lv[1]= vec[1]-la[1];
			lv[2]= vec[2]-la[2];
			Normalise(lv);
			
			inp= shi->vn[0]*lv[0]+shi->vn[1]*lv[1]+shi->vn[2]*lv[2];
			if(inp<0.0) inp= 0.0;
			
			if(mat->spec)  {
				
				if(inp>0.0) {
					/* specular shaders */
					float specfac;
					
					if(mat->spec_shader==MA_SPEC_PHONG) 
						specfac= Phong_Spec(shi->vn, lv, view, mat->har);
					else if(mat->spec_shader==MA_SPEC_COOKTORR) 
						specfac= CookTorr_Spec(shi->vn, lv, view, mat->har);
					else if(mat->spec_shader==MA_SPEC_BLINN) 
						specfac= Blinn_Spec(shi->vn, lv, view, mat->refrac, (float)mat->har);
					else 
						specfac= Toon_Spec(shi->vn, lv, view, mat->param[2], mat->param[3]);
				
					inprspec= specfac*mat->spec;
					
					isr+= inprspec*mat->specr;
					isg+= inprspec*mat->specg;
					isb+= inprspec*mat->specb;

				}
			}
			/* diffuse shaders */
			if(mat->diff_shader==MA_DIFF_ORENNAYAR) inp= OrenNayar_Diff(shi->vn, lv, view, mat->roughness);
			else if(mat->diff_shader==MA_DIFF_TOON) inp= Toon_Diff(shi->vn, lv, view, mat->param[0], mat->param[1]);
			// else Lambert

			inp= (mat->ref*inp + mat->emit);
			
			if(a==0) la= pr1_col;
			else la= pr2_col;

			ir+= inp*la[0];
			ig+= inp*la[1];
			ib+= inp*la[2];
		}
		
		/* drawing checkerboard and sky */
		if(mat->mode & MA_RAYMIRROR) {
			float col, div, y, z;
			int fac;
			
			/* rotate a bit in x */
			y= shi->ref[1]; z= shi->ref[2];
			shi->ref[1]= 0.98*y - 0.17*z;
			shi->ref[2]= 0.17*y + 0.98*z;
			
			/* scale */
			div= (0.85*shi->ref[1]);
			
			shi->refcol[0]= mat->ray_mirror*fresnel_fac(view, shi->vn, mat->fresnel_mir_i, mat->fresnel_mir);
			/* not real 'alpha', but mirror overriding transparency */
			if(mat->mode & MA_RAYTRANSP) {
				float fac= sqrt(shi->refcol[0]);
				alpha= alpha*(1.0-fac) + fac;
			}
			else alpha= alpha*(1.0-shi->refcol[0]) + shi->refcol[0];
			
			if(div<0.0) {
				/* minus 0.5 prevents too many small tiles in distance */
				fac= (int)(shi->ref[0]/(div-0.1) ) + (int)(shi->ref[2]/(div-0.1) );
				if(fac & 1) col= 0.8;
				else col= 0.3;

				shi->refcol[1]= shi->refcol[0]*col;
				shi->refcol[2]= shi->refcol[1];
				shi->refcol[3]= shi->refcol[2];
			}
			else {
				shi->refcol[1]= 0.0;
				shi->refcol[2]= shi->refcol[0]*0.3*div;
				shi->refcol[3]= shi->refcol[0]*0.8*div;
			}
		}

		if(shi->refcol[0]==0.0) {
			a= 255.0*( mat->r*ir +mat->ambr +isr);
			if(a>255) a=255; else if(a<0) a= 0;
			rect[0]= a;
			a= 255.0*(mat->g*ig +mat->ambg +isg);
			if(a>255) a=255; else if(a<0) a= 0;
			rect[1]= a;
			a= 255*(mat->b*ib +mat->ambb +isb);
			if(a>255) a=255; else if(a<0) a= 0;
			rect[2]= a;
		}
		else {
			a= 255.0*( mat->mirr*shi->refcol[1] + (1.0 - mat->mirr*shi->refcol[0])*(mat->r*ir +mat->ambr) +isr);
			if(a>255) a=255; else if(a<0) a= 0;
			rect[0]= a;
			a= 255.0*( mat->mirg*shi->refcol[2] + (1.0 - mat->mirg*shi->refcol[0])*(mat->g*ig +mat->ambg) +isg);
			if(a>255) a=255; else if(a<0) a= 0;
			rect[1]= a;
			a= 255.0*( mat->mirb*shi->refcol[3] + (1.0 - mat->mirb*shi->refcol[0])*(mat->b*ib +mat->ambb) +isb);
			if(a>255) a=255; else if(a<0) a= 0;
			rect[2]= a;
		}
	}

		/* ztra shade */
	if(mat->spectra!=0.0) {
		inp = MAX3(isr, isg, isb);
		inp *= mat->spectra;
		if(inp>1.0) inp= 1.0;
		alpha= (1.0-inp)*alpha+inp;
	}

	if(alpha!=1.0) {
		if(mat->mode & MA_RAYTRANSP) {
			refraction_prv(&x, &y, shi->vn, mat->ang);
		}
		
		tracol=  previewback(mat->pr_back, x, y) & 255;
		
		tracol= (1.0-alpha)*tracol;
		
		rect[0]= tracol+ (rect[0]*alpha) ;
		rect[1]= tracol+ (rect[1]*alpha) ;
		rect[2]= tracol+ (rect[2]*alpha) ;
	}
}


void BIF_previewrender(SpaceButs *sbuts)
{
	ID *id, *idfrom;
	Material *mat= NULL;
	Tex *tex= NULL;
	Lamp *la= NULL;
	World *wrld= NULL;
	LampRen *lar= NULL;
	Image *ima;
	HaloRen har;
	Object *ob;
	uiBlock *block;
	ShadeInput shi;
	float lens = 0.0, vec[3];
	int x, y, starty, startx, endy, endx, radsq, xsq, ysq, last = 0;
	unsigned int *rect;

	if(sbuts->cury>=PR_RECTY) return;
	
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
			mat= sbuts->lockpoin;
		else if(tab==TAB_SHADING_TEX) 
			tex= sbuts->lockpoin;
		else if(tab==TAB_SHADING_LAMP) {
			if(ob && ob->type==OB_LAMP) la= ob->data;
		}
		else if(tab==TAB_SHADING_WORLD)
			wrld= sbuts->lockpoin;
	}
	else if(sbuts->mainb==CONTEXT_OBJECT) {
		if(ob && ob->type==OB_LAMP) la= ob->data;
	}
	
	if(mat==NULL && tex==NULL && la==NULL && wrld==NULL) return;
	
	har.flarec= 0;	/* below is a test for postrender flare */
	
	if(qtest()) {
		addafterqueue(curarea->win, RENDERPREVIEW, 1);
		return;
	}

	MTC_Mat4One(R.viewmat);
	MTC_Mat4One(R.viewinv);
	
	shi.osatex= 0;
	
	if(mat) {
		/* rendervars */
		init_render_world();
		init_render_material(mat);
		
		/* clear imats */
		for(x=0; x<8; x++) {
			if(mat->mtex[x]) {
				if(mat->mtex[x]->tex) {
					init_render_texture(mat->mtex[x]->tex);
					
					if(mat->mtex[x]->tex->env && mat->mtex[x]->tex->env->object) 
						MTC_Mat4One(mat->mtex[x]->tex->env->object->imat);
				}
				if(mat->mtex[x]->object) MTC_Mat4One(mat->mtex[x]->object->imat);
				if(mat->mtex[x]->object) MTC_Mat4One(mat->mtex[x]->object->imat);
			}
		}
		shi.vlr= 0;
		shi.mat= mat;
		shi.matren= mat->ren;
		
		if(mat->mode & MA_HALO) init_previewhalo(&har, mat);
	}
	else if(tex) {

		ima= tex->ima;
		if(ima) last= ima->lastframe;
		init_render_texture(tex);
		free_unused_animimages();
		if(tex->ima) {
			if(tex->ima!=ima) allqueue(REDRAWBUTSSHADING, 0);
			else if(last!=ima->lastframe) allqueue(REDRAWBUTSSHADING, 0);
		}
		if(tex->env && tex->env->object) 
			MTC_Mat4Invert(tex->env->object->imat, tex->env->object->obmat);
	}
	else if(la) {

		init_render_world();
		init_render_textures();	/* do not do it twice!! (brightness) */
		R.totlamp= 0;
		RE_add_render_lamp(ob, 0);	/* 0=no shadbuf */
		lar= R.la[0];
		
		/* exceptions: */
		lar->spottexfac= 1.0;
		lar->spotsi= cos( M_PI/3.0 );
		lar->spotbl= (1.0-lar->spotsi)*la->spotblend;
		
		MTC_Mat3One(lar->imat);
	}
	else if(wrld) {
		
		lens= 35.0;
		if(G.scene->camera) {
			lens= ( (Camera *)G.scene->camera->data)->lens;
		}
		
		init_render_world();
		init_render_textures();	/* dont do it twice!! (brightness) */
	}

	set_previewrect(sbuts->area->win, PR_XMIN, PR_YMIN, PR_XMAX, PR_YMAX);

	if(sbuts->rect==0) {
		sbuts->rect= MEM_callocN(sizeof(int)*PR_RECTX*PR_RECTY, "butsrect");
		
		/* built in emboss */
		rect= sbuts->rect;
		for(y=0; y<PR_RECTY; y++, rect++) *rect= 0xFFFFFFFF;
		
		rect= sbuts->rect + PR_RECTX-1;
		for(y=0; y<PR_RECTY; y++, rect+=PR_RECTX) *rect= 0xFFFFFFFF;
	}
	
	starty= -PR_RECTY/2;
	endy= starty+PR_RECTY;
	starty+= sbuts->cury;
	
	/* offset +1 for emboss */
	startx= -PR_RECTX/2 +1;
	endx= startx+PR_RECTX -2;

	radsq= (PR_RECTX/2)*(PR_RECTY/2);
	
	if(mat) {
		if(mat->pr_type==MA_SPHERE) {
			pr1_lamp[0]= 2.3; pr1_lamp[1]= -2.4; pr1_lamp[2]= -4.6;
			pr2_lamp[0]= -8.8; pr2_lamp[1]= -5.6; pr2_lamp[2]= -1.5;
		}
		else {
			pr1_lamp[0]= 1.9; pr1_lamp[1]= 3.1; pr1_lamp[2]= -8.5;
			pr2_lamp[0]= 1.2; pr2_lamp[1]= -18; pr2_lamp[2]= 3.2;
		}
	}

	/* here it starts! */
	glDrawBuffer(GL_FRONT);
	uiPanelPush(block);

	for(y=starty; y<endy; y++) {
		
		rect= sbuts->rect + 1 + PR_RECTX*sbuts->cury;
		
		if(y== -PR_RECTY/2 || y==endy-1);		/* emboss */
		else if(mat) {
			
			if(mat->mode & MA_HALO) {
				for(x=startx; x<endx; x++, rect++) {
					rect[0]= previewback(mat->pr_back, x, y);
				}

				if(har.flarec) {
					if(y==endy-2) previewflare(sbuts, &har, sbuts->rect);
				}
				else {
					halo_preview_pixel(&har, startx, endx, y, (char *) (rect-PR_RECTX));
				}
			}
			else {
				ysq= y*y;
				for(x=startx; x<endx; x++, rect++) {
					xsq= x*x;
					if(mat->pr_type==MA_SPHERE) {
					
						if(xsq+ysq <= radsq) {
							shi.vn[0]= x;
							shi.vn[1]= y;
							shi.vn[2]= sqrt( (float)(radsq-xsq-ysq) );
							Normalise(shi.vn);
							
							vec[0]= shi.vn[0];
							vec[1]= shi.vn[2];
							vec[2]= -shi.vn[1];
							
							shade_preview_pixel(&shi, vec, x, y, (char *)rect, 1);
						}
						else {
							rect[0]= previewback(mat->pr_back, x, y);
						}
					}
					else if(mat->pr_type==MA_CUBE) {
						if( ray_previewrender(x, y, vec, shi.vn) ) {
							
							shade_preview_pixel(&shi, vec, x, y, (char *)rect, 0);
						}
						else {
							rect[0]= previewback(mat->pr_back, x, y);
						}
					}
					else {
						vec[0]= x*(2.0/PR_RECTX);
						vec[1]= y*(2.0/PR_RECTX);
						vec[2]= 0.0;
						
						shi.vn[0]= shi.vn[1]= 0.0;
						shi.vn[2]= 1.0;
						
						shade_preview_pixel(&shi, vec, x, y, (char *)rect, 0);
					}
				}
			}
		}
		else if(tex) {
			for(x=startx; x<endx; x++, rect++) {
				texture_preview_pixel(tex, x, y, (char *)rect);
			}
		}
		else if(la) {
			for(x=startx; x<endx; x++, rect++) {
				lamp_preview_pixel(&shi, lar, x, y, (char *)rect);
			}
		}
		else  {
			for(x=startx; x<endx; x++, rect++) {				
				sky_preview_pixel(lens, x, y, (char *)rect);
			}
		}
		
		if(y<endy-2) {

			if(qtest()) {
				addafterqueue(curarea->win, RENDERPREVIEW, 1);
				break;
			}
		}

		display_pr_scanline(sbuts->rect, sbuts->cury);
		
		sbuts->cury++;
	}

	if(sbuts->cury>=PR_RECTY && tex) 
		if (sbuts->tab[CONTEXT_SHADING]==TAB_SHADING_TEX) 
			draw_tex_crop(sbuts->lockpoin);
	
	glDrawBuffer(GL_BACK);
	/* draw again for clean swapbufers */
	BIF_previewdraw();

	uiPanelPop(block);
	
	if(mat) {
		end_render_material(mat);
		for(x=0; x<8; x++) {
			if(mat->mtex[x] && mat->mtex[x]->tex) end_render_texture(mat->mtex[x]->tex);
		}	
	}
	else if(tex) {
		end_render_texture(tex);
	}
	else if(la) {
		if(R.totlamp) {
			if(R.la[0]->org) MEM_freeN(R.la[0]->org);
			MEM_freeN(R.la[0]);
		}
		R.totlamp= 0;
		end_render_textures();
	}
	else if(wrld) {
		end_render_textures();
	}
}

