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

#include "MTC_matrixops.h"

#include "render.h"
#include "mydevice.h"

#include "DNA_group_types.h"
#include "DNA_texture_types.h"
#include "DNA_world_types.h"
#include "DNA_camera_types.h"
#include "DNA_image_types.h"
#include "DNA_material_types.h"
#include "DNA_node_types.h"
#include "DNA_object_types.h"
#include "DNA_lamp_types.h"
#include "DNA_space_types.h"
#include "DNA_scene_types.h"
#include "DNA_screen_types.h"

#include "BKE_global.h"
#include "BKE_image.h"
#include "BKE_icons.h"
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

#include "BIF_gl.h"
#include "BIF_screen.h"
#include "BIF_space.h"		/* allqueue */
#include "BIF_butspace.h"	
#include "BIF_mywindow.h"
#include "BIF_interface.h"
#include "BIF_glutil.h"

#include "BIF_previewrender.h"  /* include ourself for prototypes */

#include "PIL_time.h"

#include "RE_renderconverter.h"

#include "blendef.h"	/* CLAMP */
#include "interface.h"	/* ui_graphics_to_window() SOLVE! (ton) */

#define PR_XMIN		10
#define PR_YMIN		5
#define PR_XMAX		200
#define PR_YMAX		195

#define PR_FACY		(PR_YMAX-PR_YMIN-4)/(PR_RECTY)

static rctf prerect;
static float pr_facx, pr_facy;


/* implementation */

static short intersect(float *v1,   float *v2,  float *v3,  float *rtlabda, float *ray1, float *ray2)
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
	if(deeldet!=0.0f) {
		m0= ray1[0]-v3[0];
		m1= ray1[1]-v3[1];
		m2= ray1[2]-v3[2];
		det1= m0*x0+m1*x1+m2*x2;
		rtu= det1/deeldet;
		if(rtu<=0.0f) {
			det2= t00*(m1*t22-m2*t21);
			det2+= t01*(m2*t20-m0*t22);
			det2+= t02*(m0*t21-m1*t20);
			rtv= det2/deeldet;
			if(rtv<=0.0f) {
				if(rtu+rtv>= -1.0f) {
					
					det3=  m0*(t12*t01-t11*t02);
					det3+= m1*(t10*t02-t12*t00);
					det3+= m2*(t11*t00-t10*t01);
					*rtlabda= det3/deeldet;
					
					if(*rtlabda>=0.0f && *rtlabda<=1.0f) {
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


static int ray_previewrender(int x,  int y,  float *vec, float *vn, short pr_rectx, short pr_recty)
{
	/* float scalef= 10.0/100.0; - not fixed any more because of different render sizes */
 	float scalef= ( 64.0f / (float)pr_rectx ) * 0.25f; 
	float ray1[3], ray2[3];
	float minlabda, labda;
	int totface= 3, hitface= -1;
	int a;

	ray1[0]= ray2[0]= x*scalef;
	ray1[1]= ray2[1]= y*scalef;
	ray1[2]= -10.0f;
	ray2[2]= 10.0f;
	
	minlabda= 1.0f;
	for(a=0; a<totface; a++) {
		if(intersect( rcubev[rcubi[a][0]], rcubev[rcubi[a][1]], rcubev[rcubi[a][2]], &labda, ray1, ray2)) {
			if( labda < minlabda) {
				minlabda= labda;
				hitface= a;
			}
		}
		if(intersect( rcubev[rcubi[a][0]], rcubev[rcubi[a][2]], rcubev[rcubi[a][3]], &labda, ray1, ray2)) {
			if( labda < minlabda) {
				minlabda= labda;
				hitface= a;
			}
		}
	}
	
	if(hitface > -1) {
		
		CalcNormFloat(rcubev[rcubi[hitface][2]], rcubev[rcubi[hitface][1]], rcubev[rcubi[hitface][0]], vn);
		
		vec[0]= (minlabda*(ray1[0]-ray2[0])+ray2[0])/4.1;
		vec[1]= (minlabda*(ray1[1]-ray2[1])+ray2[1])/4.1;
		vec[2]= (minlabda*(ray1[2]-ray2[2])+ray2[2])/4.1;
		
		return 1;
	}
	return 0;
}

static unsigned int previewback(int type, int x, int y)
{
	unsigned int col;
	char* pcol;
	
	/* checkerboard, for later
		x+= PR_RECTX/2;
	y+= PR_RECTX/2;
	if( ((x/24) + (y/24)) & 1) return 0x40404040;
	else return 0xa0a0a0a0;
	*/
	
	if(type & MA_DARK) {
		if(abs(x)>abs(y)) col= 0;
		else col= 0x40404040;
	}
	else {
		if(abs(x)>abs(y)) col= 0x40404040;
		else col= 0xa0a0a0a0;
	}
	pcol = (char*) &col;
	pcol[3] = 0; /* set alpha to zero - endianess!*/
	
	return col;
}

static float previewbackf(int type, int x, int y)
{
	float col;
	
	if(type & MA_DARK) {
		if(abs(x)>abs(y)) col= 0.0f;
		else col= 0.25f;
	}
	else {
		if(abs(x)>abs(y)) col= 0.25f;
		else col= 0.625f;
	}
	return col;
}

void set_previewrect(int win, int xmin, int ymin, int xmax, int ymax, short pr_rectx, short pr_recty)
{
	float pr_sizex, pr_sizey;
	
	prerect.xmin= xmin;
	prerect.ymin= ymin;
	prerect.xmax= xmax;
	prerect.ymax= ymax;

	ui_graphics_to_window(win, &prerect.xmin, &prerect.ymin);
	ui_graphics_to_window(win, &prerect.xmax, &prerect.ymax);
	
	pr_sizex= (prerect.xmax-prerect.xmin);
	pr_sizey= (prerect.ymax-prerect.ymin);

	pr_facx= ( pr_sizex-1.0f)/pr_rectx;
	pr_facy= ( pr_sizey-1.0f)/pr_recty;

	/* correction for gla draw */
	prerect.xmin-= curarea->winrct.xmin;
	prerect.ymin-= curarea->winrct.ymin;
	
	glMatrixMode(GL_PROJECTION);
	glPushMatrix();
	glMatrixMode(GL_MODELVIEW);
	glPushMatrix();
	
	glaDefine2DArea(&curarea->winrct);

	glPixelZoom(pr_facx, pr_facy);
	
}

static void end_previewrect(void)
{
	glMatrixMode(GL_PROJECTION);
	glPopMatrix();
	glMatrixMode(GL_MODELVIEW);
	glPopMatrix();
	
	glPixelZoom(1.0f, 1.0f);
	
	// restore viewport / scissor which was set by glaDefine2DArea
	glViewport(curarea->winrct.xmin, curarea->winrct.ymin, curarea->winx, curarea->winy);
	glScissor(curarea->winrct.xmin, curarea->winrct.ymin, curarea->winx, curarea->winy);

}

static void display_pr_scanline(unsigned int *rect, int recty, short pr_rectx)
{
	
	/* we do steps of 4 scanlines. but draw 5, because of errors in some gfx cards (nvidia geforce, ati...) */
	if( (recty & 3)==3) {
		
		if(recty == 3) {
			glaDrawPixelsSafe(prerect.xmin, prerect.ymin, pr_rectx, 4, GL_RGBA, GL_UNSIGNED_BYTE, rect);
		}
		else {
			rect+= (recty-4)*pr_rectx;
			glaDrawPixelsSafe(prerect.xmin, prerect.ymin + (((float)recty-4.0)*pr_facy), pr_rectx, 5, GL_RGBA, GL_UNSIGNED_BYTE, rect);
		}
	}
}

static void draw_tex_crop(Tex *tex)
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
	}
}

static void previewdraw_render(struct RenderInfo* ri, ScrArea* area)
{
	int y;

	if (!ri) {
		return;
	}
	
	for (y=0; y<ri->pr_recty; y++) {
		display_pr_scanline(ri->rect, y, ri->pr_rectx);
	}	
}


static void sky_preview_pixel(float lens, int x, int y, char *rect, short pr_rectx, short pr_recty)
{
	float view[3];
	
	if(R.wrld.skytype & WO_SKYPAPER) {
		view[0]= (2*x)/(float)pr_rectx;
		view[1]= (2*y)/(float)pr_recty;
		view[2]= 0.0f;
	}
	else {
		view[0]= x;
		view[1]= y;
		view[2]= -lens*pr_rectx/32.0;
		Normalise(view);
	}
	RE_sky_char(view, rect);
	rect[3] = 0xFF;
}

 static void init_preview_world(World* wrld)
 {
 	int a;
 	char *cp;
 	
 	if(wrld) {
 		R.wrld= *(wrld);
 		
 		cp= (char *)&R.wrld.fastcol;
 		
 		cp[0]= 255.0*R.wrld.horr;
 		cp[1]= 255.0*R.wrld.horg;
 		cp[2]= 255.0*R.wrld.horb;
 		cp[3]= 1;
 		
 		VECCOPY(R.grvec, R.viewmat[2]);
 		Normalise(R.grvec);
 		Mat3CpyMat4(R.imat, R.viewinv);
 		
 		for(a=0; a<MAX_MTEX; a++) 
 			if(R.wrld.mtex[a] && R.wrld.mtex[a]->tex) R.wrld.skytype |= WO_SKYTEX;
 		
 		while(R.wrld.aosamp*R.wrld.aosamp < R.osa) R.wrld.aosamp++;
 	}
 	else {
 		memset(&R.wrld, 0, sizeof(World));
 		R.wrld.exp= 0.0;
 		R.wrld.range= 1.0;
 	}
  	
 	R.wrld.linfac= 1.0 + pow((2.0*R.wrld.exp + 0.5), -10);
 	R.wrld.logfac= log( (R.wrld.linfac-1.0)/R.wrld.linfac )/R.wrld.range;
}

 /* This function carefully copies over the struct members
    from the struct Lamp to a new struct LampRen.
    It only copies the struct members that are needed 
    in the lamp_preview_pixel function.
    Replacement for the RE_add_render_lamp function in
    the preview, because this only works for the 
    current selected lamp.
 */
static LampRen* create_preview_render_lamp(Lamp* la)
{
 	LampRen *lar;
 	int c;
	
 	lar= (LampRen *)MEM_callocN(sizeof(LampRen),"lampren");
	
 	MTC_Mat3One(lar->mat);
 	MTC_Mat3One(lar->imat);
	
 	lar->type= la->type;
 	lar->mode= la->mode;
 	lar->energy= la->energy;
 	if(la->mode & LA_NEG) lar->energy= -lar->energy;
 	lar->r= lar->energy*la->r;
 	lar->g= lar->energy*la->g;
 	lar->b= lar->energy*la->b;
 	lar->k= la->k;
 	lar->dist= la->dist;
 	lar->ld1= la->att1;
 	lar->ld2= la->att2;
	
 	/* exceptions: */
 	lar->spottexfac= 1.0;
 	lar->spotsi= cos( M_PI/3.0 );
 	lar->spotbl= (1.0-lar->spotsi)*la->spotblend;
 	
 	MTC_Mat3One(lar->imat);
	
 	if(lar->type==LA_SPOT) {
 		if(lar->mode & LA_ONLYSHADOW) {
 			if((lar->mode & (LA_SHAD|LA_SHAD_RAY))==0) lar->mode -= LA_ONLYSHADOW;
 		}
 	}
 	memcpy(lar->mtex, la->mtex, MAX_MTEX*sizeof(void *));
	
 	for(c=0; c<MAX_MTEX; c++) {
 		if(la->mtex[c] && la->mtex[c]->tex) {
 			lar->mode |= LA_TEXTURE;
			
 			if(R.flag & R_RENDERING) {
 				if(R.osa) {
 					if(la->mtex[c]->tex->type==TEX_IMAGE) lar->mode |= LA_OSATEX;
 				}
 			}
 		}
	}
	
 	return lar;
}

static void lamp_preview_pixel(ShadeInput *shi, LampRen *la, int x, int y, char *rect, short pr_rectx, short pr_recty)
{
	float inpr, i, t, dist, distkw, vec[3], lacol[3];
	int col;
	
	shi->co[0]= (float)x/(pr_rectx/4);
	shi->co[1]= (float)y/(pr_rectx/4);
	shi->co[2]= 0;
	
	vec[0]= 0.02f*x;
	vec[1]= 0.02f*y;
	vec[2]= 0.005f*pr_rectx;
	VECCOPY(shi->view, vec);
	dist= Normalise(shi->view);

	lacol[0]= la->r;
	lacol[1]= la->g;
	lacol[2]= la->b;
	
	if(la->mode & LA_TEXTURE) do_lamp_tex(la, vec, shi, lacol);

	if(la->type==LA_SUN || la->type==LA_HEMI) {
		dist= 1.0f;
	}
	else {
		
		if(la->mode & LA_QUAD) {
			
			t= 1.0f;
			if(la->ld1>0.0f)
				t= la->dist/(la->dist+la->ld1*dist);
			if(la->ld2>0.0f) {
				distkw= la->dist*la->dist;
				t= t*distkw/(t*distkw+la->ld2*dist*dist);
			}
			dist= t;
		}
		else {
			dist= (la->dist/(la->dist+dist));
		}
	}

	/* yafray: preview shade as spot, sufficient */
	if ((la->type==LA_SPOT) || (la->type==LA_YF_PHOTON)) {

		
		if(la->mode & LA_SQUARE) {
			/* slightly smaller... */
			inpr= 1.7*cos(MAX2(fabs(shi->view[0]/shi->view[2]) , fabs(shi->view[1]/shi->view[2]) ));
		}
		else {
			inpr= shi->view[2];
		}
		
		t= la->spotsi;
		if(inpr<t) dist= 0.0f;
		else {
			t= inpr-t;
			if(t<la->spotbl && la->spotbl!=0.0f) {
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
	
	col= 255.0*dist*lacol[0];
	if(col<=0) rect[0]= 0; else if(col>=255) rect[0]= 255; else rect[0]= col;

	col= 255.0*dist*lacol[1];
	if(col<=0) rect[1]= 0; else if(col>=255) rect[1]= 255; else rect[1]= col;

	col= 255.0*dist*lacol[2];
	if(col<=0) rect[2]= 0; else if(col>=255) rect[2]= 255; else rect[2]= col;

	rect[3] = 0xFF;
}

static void init_previewhalo(HaloRen *har, Material *mat, short pr_rectx, short pr_recty)
{
	
	har->type= 0;
	if(mat->mode & MA_HALO_XALPHA) har->type |= HA_XALPHA;
	har->mat= mat;
	har->hard= mat->har;
	har->rad= pr_rectx/2.0;
	har->radsq= pr_rectx*pr_rectx/4.0;
	har->alfa= mat->alpha;
	har->add= 255.0*mat->add;
	har->r= mat->r;
	har->g= mat->g; 
	har->b= mat->b;
	har->xs= pr_rectx/2.0;
	har->ys= pr_rectx/2.0;
	har->zs= har->zd= 0;
	har->seed= (mat->seed1 % 256);
	
	if( (mat->mode & MA_HALOTEX) && mat->mtex[0] ) har->tex= 1; else har->tex=0;

	if(mat->mode & MA_STAR) har->starpoints= mat->starc; else har->starpoints= 0;
	if(mat->mode & MA_HALO_LINES) har->linec= mat->linec; else har->linec= 0;
	if(mat->mode & MA_HALO_RINGS) har->ringc= mat->ringc; else har->ringc= 0;
	if(mat->mode & MA_HALO_FLARE) har->flarec= mat->flarec; else har->flarec= 0;
	
	if(har->flarec) {
		har->xs-= pr_rectx/3;
		har->ys+= pr_rectx/3;
		
		har->rad*= 0.3;
		har->radsq= har->rad*har->rad;
		
		har->pixels= har->rad*har->rad*har->rad;
	}
}	

static void halo_preview_pixel(HaloRen *har, int startx, int endx, int y, char *rect, short pr_rectx)
{
	float dist, xn, yn, xsq, ysq, colf[4];
	int x;
	char front[4];
	
	if(har->flarec) yn= y-pr_rectx/3;
	else yn= y;
	ysq= yn*yn;
	
	for(x=startx; x<endx; x++) {
		
		if(har->flarec) xn= x+pr_rectx/3;
		else xn= x;
		
		xsq= xn*xn;
		dist= xsq+ysq;

		if(dist<har->radsq) {
			RE_shadehalo(har, front, colf, 0, dist, xn, yn, har->flarec);
			RE_addalphaAddfac(rect, front, har->add);
			rect[3] = 0xFF;		/* makes icon display all pixels */
		}
		rect+= 4;
	}
}

static void previewflare(RenderInfo *ri, HaloRen *har, short pr_rectx, short pr_recty, int pr_method)
{
	float ycor;
	unsigned int *rectot;
	int afmx, afmy, rectx, recty, y;
	
	/* check for "Preview" block already in calling function BIF_previewrender! - elubie */

	/* temps */
	ycor= R.ycor;
	rectx= R.rectx;
	recty= R.recty;
	afmx= R.afmx;
	afmy= R.afmy;
	rectot= R.rectot;

	R.r.postmul= R.r.postgamma= R.r.postsat= 1.0f;
	R.r.posthue= R.r.postadd= 0.0f;
	R.ycor= 1.0f;
	R.rectx= pr_rectx;	
	R.recty= pr_recty;
	R.afmx= pr_rectx/2;
	R.afmy= pr_recty/2;
	R.rectot= ri->rect;

	waitcursor(1);
	RE_renderflare(har);
	waitcursor(0);
	// not sure why, either waitcursor or renderflare screws up (disabled then)
	//areawinset(curarea->win);
	
	/* draw can just be called this way, all settings are OK */
	if (pr_method==PR_DRAW_RENDER) {
		for (y=0; y<pr_recty; y++) {
			display_pr_scanline(ri->rect, y, pr_rectx);
		}
	}
	
	/* temps */
	R.ycor= ycor;
	R.rectx= rectx;
	R.recty= recty;
	R.afmx= afmx;
	R.afmy= afmy;
	R.rectot= rectot;
}

static void texture_preview_pixel(Tex *tex, int x, int y, char *rect, short pr_rectx, short pr_recty)
{
	float i, v1, xsq, ysq, texvec[3];
	float tin=1.0f, tr, tg, tb, ta;
	int rgbnor, tracol, skip=0;
	
	if(tex->type==TEX_IMAGE) {
		v1= 1.0f/pr_rectx;
		
		texvec[0]= 0.5+v1*x;
		texvec[1]= 0.5+v1*y;
		
		/* no coordinate mapping, exception: repeat */
		if(tex->extend==TEX_REPEAT) {
			if(tex->xrepeat>1) {
				texvec[0] *= tex->xrepeat;
				if(texvec[0]>1.0f) texvec[0] -= (int)(texvec[0]);
			}
			if(tex->yrepeat>1) {
				texvec[1] *= tex->yrepeat;
				if(texvec[1]>1.0f) texvec[1] -= (int)(texvec[1]);
			}
		}
		else if(tex->extend==TEX_CHECKER) {
			texvec[0]= 0.5+1.6*v1*x;
			texvec[1]= 0.5+1.6*v1*y;
		}
	}
	else if(tex->type==TEX_ENVMAP) {
		if(tex->env) {
			ysq= y*y;
			xsq= x*x;
			if(xsq+ysq < (pr_rectx/2)*(pr_recty/2)) {
				texvec[2]= sqrt( (float)((pr_rectx/2)*(pr_recty/2)-xsq-ysq) );
				texvec[0]= -x;
				texvec[1]= -y;
				Normalise(texvec);

				i= 2.0*(texvec[2]);
				texvec[0]= (i*texvec[0]);
				texvec[1]= (i*texvec[1]);
				texvec[2]= (-1.0f+i*texvec[2]);

			}
			else {
				skip= 1;
				tr= tg= tb= ta= 0.0f;
			}
		}
		else {
			skip= 1;
			tr= tg= tb= ta= 0.0f;
		}
	}
	else {
		v1= 2.0/pr_rectx;
	
		texvec[0]= v1*x;
		texvec[1]= v1*y;
		texvec[2]= 0.0f;
	}
	
	if(skip==0) rgbnor= multitex_ext(tex, texvec, &tin, &tr, &tg, &tb, &ta);
	else rgbnor= 1;
	
	if(rgbnor & 1) {
		
		v1= 255.0*tr;
		rect[0]= CLAMPIS(v1, 0, 255);
		v1= 255.0*tg;
		rect[1]= CLAMPIS(v1, 0, 255);
		v1= 255.0*tb;
		rect[2]= CLAMPIS(v1, 0, 255);
		
		if(ta!=1.0f) {
			tracol=  64+100*(abs(x)>abs(y));
			tracol= (1.0f-ta)*tracol;
			
			rect[0]= tracol+ (rect[0]*ta) ;
			rect[1]= tracol+ (rect[1]*ta) ;
			rect[2]= tracol+ (rect[2]*ta) ;
					
		}

		rect[3] = 0xFF;
	}
	else {
		rect[0]= 255.0*tin;
		rect[1]= 255.0*tin;
		rect[2]= 255.0*tin;
		rect[3] = 0xFF;
	}
}

static float pr1_lamp[3]= {2.3, -2.4, -4.6};	/* note; is not used! */
static float pr2_lamp[3]= {-8.8, -5.6, -1.5};
static float pr1_col[3]= {0.8, 0.8, 0.8};
static float pr2_col[3]= {0.5, 0.6, 0.7};

static void refraction_prv(int *x, int *y, float *n, float index)
{
	float dot, fac, view[3], len;

	index= 1.0f/index;
	
	view[0]= index*(float)*x;
	view[1]= ((float)*y)/index;
	view[2]= 20.0f;
	len= Normalise(view);
	
	dot= view[0]*n[0] + view[1]*n[1] + view[2]*n[2];

	if(dot>0.0f) {
		fac= 1.0f - (1.0f - dot*dot)*index*index;
		if(fac<= 0.0f) return;
		fac= -dot*index + sqrt(fac);
	}
	else {
		index = 1.0f/index;
		fac= 1.0f - (1.0f - dot*dot)*index*index;
		if(fac<= 0.0f) return;
		fac= -dot*index - sqrt(fac);
	}

	*x= (int)(len*(index*view[0] + fac*n[0]));
	*y= (int)(len*(index*view[1] + fac*n[1]));
}

static void shade_lamp_loop_preview(ShadeInput *shi, ShadeResult *shr)
{
	extern float fresnel_fac(float *view, float *vn, float ior, float fac);
	Material *mat= shi->mat;
	float inp, is, inprspec=0;
	float lv[3], *la, *vn, vnor[3];
	int a;
	
	if((mat->mode & MA_RAYMIRROR)==0) shi->ray_mirror= 0.0f;
	memset(shr, 0, sizeof(ShadeResult));
	
	do_material_tex(shi);
	
	shr->alpha= shi->alpha;
	
	if(mat->mode & (MA_ZTRA|MA_RAYTRANSP)) 
		if(mat->fresnel_tra!=0.0f) 
			shr->alpha*= fresnel_fac(shi->view, shi->vn, mat->fresnel_tra_i, mat->fresnel_tra);
	
	if(mat->mode & MA_SHLESS) {
		shr->diff[0]= shi->r;
		shr->diff[1]= shi->g;
		shr->diff[2]= shi->b;
		
	}
	else {
		
		if(mat->texco & TEXCO_REFL) {
			inp= -2.0*(shi->vn[0]*shi->view[0]+shi->vn[1]*shi->view[1]+shi->vn[2]*shi->view[2]);
			shi->ref[0]= (shi->view[0]+inp*shi->vn[0]);
			shi->ref[1]= (shi->view[1]+inp*shi->vn[1]);
			shi->ref[2]= (shi->view[2]+inp*shi->vn[2]);
			/* normals in render are pointing different... rhm */
//			if(shi->pr_type==MA_SPHERE)
//				shi->ref[1]= -shi->ref[1];
		}
		
		for(a=0; a<2; a++) {
			
			if((mat->pr_lamp & (1<<a))==0) continue;
			
			if(a==0) la= pr1_lamp;
			else la= pr2_lamp;
			
			lv[0]= shi->co[0]-la[0];
			lv[1]= shi->co[1]-la[1];
			lv[2]= shi->co[2]-la[2];
			Normalise(lv);
			
			if(shi->spec>0.0f)  {
				/* specular shaders */
				float specfac;
				
				if(mat->mode & MA_TANGENT_V) vn= shi->tang;
				else vn= shi->vn;
				
				if(mat->spec_shader==MA_SPEC_PHONG) 
					specfac= Phong_Spec(vn, lv, shi->view, shi->har, mat->mode & MA_TANGENT_V);
				else if(mat->spec_shader==MA_SPEC_COOKTORR) 
					specfac= CookTorr_Spec(vn, lv, shi->view, shi->har, mat->mode & MA_TANGENT_V);
				else if(mat->spec_shader==MA_SPEC_BLINN) 
					specfac= Blinn_Spec(vn, lv, shi->view, mat->refrac, (float)shi->har, mat->mode & MA_TANGENT_V);
				else if(mat->spec_shader==MA_SPEC_WARDISO)
					specfac= WardIso_Spec(vn, lv, shi->view, mat->rms, mat->mode & MA_TANGENT_V);
				else 
					specfac= Toon_Spec(vn, lv, shi->view, mat->param[2], mat->param[3], mat->mode & MA_TANGENT_V);
				
				inprspec= specfac*shi->spec;
				
				if(mat->mode & MA_RAMP_SPEC) {
					float spec[3];
					do_specular_ramp(shi, specfac, inprspec, spec);
					shr->spec[0]+= inprspec*spec[0];
					shr->spec[1]+= inprspec*spec[1];
					shr->spec[2]+= inprspec*spec[2];
				}
				else {	
					shr->spec[0]+= inprspec*shi->specr;
					shr->spec[1]+= inprspec*shi->specg;
					shr->spec[2]+= inprspec*shi->specb;
				}
			}
			
			if(mat->mode & MA_TANGENT_V) {
				float cross[3];
				Crossf(cross, lv, shi->tang);
				Crossf(vnor, cross, shi->tang);
				vnor[0]= -vnor[0];vnor[1]= -vnor[1];vnor[2]= -vnor[2];
				vn= vnor;
			}
			else vn= shi->vn;
			
			is= vn[0]*lv[0]+vn[1]*lv[1]+vn[2]*lv[2];
			if(is<0.0f) is= 0.0f;
			
			/* diffuse shaders */
			if(mat->diff_shader==MA_DIFF_ORENNAYAR) is= OrenNayar_Diff(vn, lv, shi->view, mat->roughness);
			else if(mat->diff_shader==MA_DIFF_TOON) is= Toon_Diff(vn, lv, shi->view, mat->param[0], mat->param[1]);
			else if(mat->diff_shader==MA_DIFF_MINNAERT) is= Minnaert_Diff(is, vn, shi->view, mat->darkness);
			else if(mat->diff_shader==MA_DIFF_FRESNEL) is= Fresnel_Diff(vn, lv, shi->view, mat->param[0], mat->param[1]);
			// else Lambert
			
			inp= (shi->refl*is + shi->emit);
			
			if(a==0) la= pr1_col;
			else la= pr2_col;
			
			add_to_diffuse(shr->diff, shi, is, inp*la[0], inp*la[1], inp*la[2]);
		}
		/* end lamp loop */
		
		/* drawing checkerboard and sky */
		if(mat->mode & MA_RAYMIRROR) {
			float col, div, y, z;
			int fac;
			
			/* rotate a bit in x */
			y= shi->ref[1]; z= shi->ref[2];
			shi->ref[1]= 0.98*y - 0.17*z;
			shi->ref[2]= 0.17*y + 0.98*z;
			
			/* scale */
			div= (0.85f*shi->ref[1]);
			
			shi->refcol[0]= shi->ray_mirror*fresnel_fac(shi->view, shi->vn, mat->fresnel_mir_i, mat->fresnel_mir);
			/* not real 'alpha', but mirror overriding transparency */
			if(mat->mode & MA_RAYTRANSP) {
				float fac= sqrt(shi->refcol[0]);
				shr->alpha= shr->alpha*(1.0f-fac) + fac;
			}
			else shr->alpha= shr->alpha*(1.0f-shi->refcol[0]) + shi->refcol[0];
			
			if(div<0.0f) {
				/* minus 0.5 prevents too many small tiles in distance */
				fac= (int)(shi->ref[0]/(div-0.1f) ) + (int)(shi->ref[2]/(div-0.1f) );
				if(fac & 1) col= 0.8f;
				else col= 0.3f;
				
				shi->refcol[1]= shi->refcol[0]*col;
				shi->refcol[2]= shi->refcol[1];
				shi->refcol[3]= shi->refcol[2];
			}
			else {
				shi->refcol[1]= 0.0f;
				shi->refcol[2]= shi->refcol[0]*0.3f*div;
				shi->refcol[3]= shi->refcol[0]*0.8f*div;
			}
		}
		else 
			shi->refcol[0]= 0.0f;
		
		shr->diff[0]+= shi->ambr;
		shr->diff[1]+= shi->ambg;
		shr->diff[2]+= shi->ambb;
		
		if(mat->mode & MA_RAMP_COL) ramp_diffuse_result(shr->diff, shi);
		if(mat->mode & MA_RAMP_SPEC) ramp_spec_result(shr->spec, shr->spec+1, shr->spec+2, shi);
		
		/* refcol */
		if(shi->refcol[0]!=0.0f) {
			shr->diff[0]= shi->mirr*shi->refcol[1] + (1.0f - shi->mirr*shi->refcol[0])*shr->diff[0];
			shr->diff[1]= shi->mirg*shi->refcol[2] + (1.0f - shi->mirg*shi->refcol[0])*shr->diff[1];
			shr->diff[2]= shi->mirb*shi->refcol[3] + (1.0f - shi->mirb*shi->refcol[0])*shr->diff[2];
		}
	
		/* ztra shade */
		if(shi->spectra!=0.0f) {
			inp = MAX3(shr->spec[0], shr->spec[1], shr->spec[2]);
			inp *= shi->spectra;
			if(inp>1.0f) inp= 1.0f;
			shr->alpha= (1.0f-inp)*shr->alpha+inp;
		}
	}
}

static void shade_preview_pixel(ShadeInput *shi, float *vec, int x, int y, char *rect, short pr_rectx, short pr_recty)
{
	Material *mat;
	ShadeResult shr;
	float v1;
	float eul[3], tmat[3][3], imat[3][3], col[4];
		
	mat= shi->mat;

	v1= 0.5/pr_rectx;
	shi->view[0]= v1*x;
	shi->view[1]= v1*y;
	shi->view[2]= -1.0f;
	Normalise(shi->view);
	
	shi->xs= x + pr_rectx/2;
	shi->ys= y + pr_recty/2;
	
	shi->refcol[0]= shi->refcol[1]= shi->refcol[2]= shi->refcol[3]= 0.0f;
	VECCOPY(shi->co, vec);
	
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
		if(mat->texco & TEXCO_STRAND) {
			shi->strand= shi->lo[0];
		}
		if(mat->texco & TEXCO_OBJECT) {
			/* nothing */
		}
		if(mat->texco & (TEXCO_NORM)) {
			//shi->orn[0]= shi->vn[0];
			//shi->orn[1]= shi->vn[1];
			//shi->orn[2]= shi->vn[2];
		}

		/* Clear displase vec for preview */
		shi->displace[0]= shi->displace[1]= shi->displace[2]= 0.0;
			
		if(mat->pr_type==MA_CUBE) {
			/* rotate normal back for normals texture */
			SWAP(float, shi->vn[0], shi->vn[1]);
			MTC_Mat3Inv(imat, tmat);
			MTC_Mat3MulVecfl(imat, shi->vn);
		}
		
	}

	if(mat->mapto & MAP_DISPLACE) { /* Quick hack of fake displacement preview */
//		shi->vn[0]-=2.0*shi->displace[2];
//		shi->vn[1]-=2.0*shi->displace[0];
//		shi->vn[2]+=2.0*shi->displace[1];
//		Normalise(shi->vn);
	}

	VECCOPY(shi->vno, shi->vn);
	if(mat->nodetree && mat->use_nodes) {
		ntreeShaderExecTree(mat->nodetree, shi, &shr);
	}
	else {
		/* copy all relevant material vars, note, keep this synced with render_types.h */
		memcpy(&shi->r, &mat->r, 23*sizeof(float));
		shi->har= mat->har;
		
		shade_lamp_loop_preview(shi, &shr);
	}

	shi->mat= mat;	/* restore, shade input is re-used! */

	/* after shading and composit layers */
	if(shr.spec[0]<0.0f) shr.spec[0]= 0.0f;
	if(shr.spec[1]<0.0f) shr.spec[1]= 0.0f;
	if(shr.spec[2]<0.0f) shr.spec[2]= 0.0f;

	if(shr.diff[0]<0.0f) shr.diff[0]= 0.0f;
	if(shr.diff[1]<0.0f) shr.diff[1]= 0.0f;
	if(shr.diff[2]<0.0f) shr.diff[2]= 0.0f;

	VECADD(col, shr.diff, shr.spec);
	col[3]= shr.alpha;

	/* handle backdrop now */

	if(col[3]!=1.0f) {
		float back, backm;
		
		/* distorts x and y */
		if(mat->mode & MA_RAYTRANSP) {
			refraction_prv(&x, &y, shi->vn, shi->ang);
		}
		
		back= previewbackf(mat->pr_back, x, y);
		backm= (1.0f-shr.alpha)*back;
		
		if((mat->mode & MA_RAYTRANSP) && mat->filter!=0.0) {
			float fr= 1.0f+ mat->filter*(shr.diff[0]-1.0f);
			col[0]= fr*backm+ (col[3]*col[0]);
			fr= 1.0f+ mat->filter*(shr.diff[1]-1.0f);
			col[1]= fr*backm+ (col[3]*col[1]);
			fr= 1.0f+ mat->filter*(shr.diff[2]-1.0f);
			col[2]= fr*backm+ (col[3]*col[2]);
		}
		else {
			col[0]= backm + (col[3]*col[0]);
			col[1]= backm + (col[3]*col[1]);
			col[2]= backm + (col[3]*col[2]);
		}
	}

	if(col[0]<=0.0f) rect[0]= 0; else if(col[0]>=1.0f) rect[0]= 255; else rect[0]= (char)(255.0f*col[0]);
	if(col[1]<=0.0f) rect[1]= 0; else if(col[1]>=1.0f) rect[1]= 255; else rect[1]= (char)(255.0f*col[1]);
	if(col[2]<=0.0f) rect[2]= 0; else if(col[2]>=1.0f) rect[2]= 255; else rect[2]= (char)(255.0f*col[2]);
	if(col[3]<=0.0f) rect[3]= 0; else if(col[3]>=1.0f) rect[3]= 255; else rect[3]= (char)(255.0f*col[3]);
}

static void preview_init_render_textures(MTex **mtex)
{
	int x;
	
	for(x=0; x<MAX_MTEX; x++) {
		if(mtex[x]) {
			if(mtex[x]->tex) {
				init_render_texture(mtex[x]->tex);
				
				if(mtex[x]->tex->env && mtex[x]->tex->env->object) 
					MTC_Mat4One(mtex[x]->tex->env->object->imat);
				
			}
			if(mtex[x]->object) MTC_Mat4One(mtex[x]->object->imat);
			if(mtex[x]->object) MTC_Mat4One(mtex[x]->object->imat);
		}
	}
	
}

/* main previewrender loop */
void BIF_previewrender(struct ID *id, struct RenderInfo *ri, struct ScrArea *area, int pr_method)
{
	static double lasttime= 0;
	Material *mat= NULL;
	Tex *tex= NULL;
	Lamp *la= NULL;
	World *wrld= NULL;
	LampRen *lar= NULL;
	Image *ima;
	HaloRen har;
	Object *ob;
	ShadeInput shi;
	float lens = 0.0, vec[3];
	int x, y, starty, startx, endy, endx, radsq, xsq, ysq, last = 0;
	unsigned int *rect;

	if(ri->cury>=ri->pr_rectx) return;
	
	ob= ((G.scene->basact)? (G.scene->basact)->object: 0);

	switch(GS(id->name)) 
	{
	case ID_MA:
		mat = (Material*)id; break;
	case ID_TE:
		tex = (Tex*)id; break;
	case ID_LA:
		la = (Lamp*)id; break;
	case ID_WO:
		wrld = (World*)id; break;
	default:
		return;
	}	
	
	har.flarec= 0;	/* below is a test for postrender flare */

	/* no event escape for icon render */
	if(pr_method!=PR_ICON_RENDER && qtest()) {
		addafterqueue(curarea->win, RENDERPREVIEW, 1);
		return;
	}

	MTC_Mat4One(R.viewmat);
	MTC_Mat4One(R.viewinv);
	
	shi.osatex= 0;
	
	if(mat) {
		
		/* rendervars */
		init_render_world();
		init_render_material(mat);	/* does nodes too */
		
		/* also clears imats */
		preview_init_render_textures(mat->mtex);
		
		/* do the textures for nodes */
		if(mat->nodetree && mat->use_nodes) {
			bNode *node;
			for(node=mat->nodetree->nodes.first; node; node= node->next) {
				if(node->id && GS(node->id->name)==ID_MA) {
					Material *ma= (Material *)node->id;
					preview_init_render_textures(ma->mtex);
				}
			}
			/* signal to node editor to store previews or not */
			if(pr_method==PR_ICON_RENDER) {
				shi.do_preview= 0;
			}
			else {
				ntreeInitPreview(mat->nodetree, ri->pr_rectx, ri->pr_recty);
				shi.do_preview= 1;
			}
		}
		shi.vlr= NULL;		
		shi.mat= mat;
		shi.pr_type= mat->pr_type;

		if(mat->mode & MA_HALO) init_previewhalo(&har, mat, ri->pr_rectx, ri->pr_recty);
		
		set_node_shader_lamp_loop(shade_lamp_loop_preview);
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
		preview_init_render_textures(la->mtex);
		
		/* lar= ((GroupObject *)R.lights.first)->lampren;
		RE_add_render_lamp(ob, 0);	*/ /* 0=no shadbuf or tables */
		
		/* elubie: not nice, but ob contains current object, not usable if you 
		need to render lamp that's not active object :( */
		lar = create_preview_render_lamp(la);
		
		/* exceptions: */
		lar->spottexfac= 1.0f;
		lar->spotsi= cos( M_PI/3.0f );
		lar->spotbl= (1.0f-lar->spotsi)*la->spotblend;
		
		MTC_Mat3One(lar->imat);
	}
	else if(wrld) {
		
		lens= 35.0;
		if(G.scene->camera) {
			lens= ( (Camera *)G.scene->camera->data)->lens;

			/* needed for init_render_world */
			MTC_Mat4CpyMat4(R.viewinv, G.scene->camera->obmat);
			MTC_Mat4Ortho(R.viewinv);
			MTC_Mat4Invert(R.viewmat, R.viewinv);
		}
		init_preview_world(wrld);
		preview_init_render_textures(wrld->mtex);	
	}

	if(ri->rect==NULL) {
		ri->rect= MEM_callocN(sizeof(int)*ri->pr_rectx*ri->pr_recty, "butsrect");
	}
	
	starty= -ri->pr_recty/2;
	endy= starty+ri->pr_recty;
	starty+= ri->cury;
	
	startx= -ri->pr_rectx/2;
	endx= startx+ri->pr_rectx;

	radsq= (ri->pr_rectx/2)*(ri->pr_recty/2); 
	
	if(mat) {
		pr1_lamp[0]= -2.3; pr1_lamp[1]= 2.4; pr1_lamp[2]= 4.6;
		pr2_lamp[0]= 8.8; pr2_lamp[1]= 5.6; pr2_lamp[2]= 1.5;
		
	}

	if (pr_method==PR_DRAW_RENDER) 
		glDrawBuffer(GL_FRONT);

	/* here it starts! */
	for(y=starty; y<endy; y++) {
		
		rect= ri->rect + ri->pr_rectx*ri->cury;
		
		if(mat) {
			
			if(mat->mode & MA_HALO) {
				for(x=startx; x<endx; x++, rect++) {
					rect[0]= previewback(mat->pr_back, x, y);
				}

				if(har.flarec) {
					if(y==endy-2) previewflare(ri, &har, ri->pr_rectx, ri->pr_recty, pr_method);
				}
				else {
					halo_preview_pixel(&har, startx, endx, y, (char *) (rect-ri->pr_rectx), ri->pr_rectx);
				}
			}
			else {
				ysq= y*y;
				for(x=startx; x<endx; x++, rect++) {
					xsq= x*x;
					if(mat->pr_type==MA_SPHERE) {
					
						if(xsq+ysq <= radsq) {
							shi.vn[0]= -x;
							shi.vn[1]= -y;
							shi.vn[2]= -sqrt( (float)(radsq-xsq-ysq) );
							Normalise(shi.vn);
							
							vec[0]= shi.vn[0];
							vec[1]= shi.vn[1];
							vec[2]= -shi.vn[2];
							
							if(mat->mode & MA_TANGENT_V) {
								float tmp[3];
								tmp[0]=tmp[2]= 0.0f;
								tmp[1]= 1.0f;
								Crossf(shi.tang, tmp, shi.vn);
								Normalise(shi.tang);
							}
							
							shade_preview_pixel(&shi, vec, x, y, (char *)rect, ri->pr_rectx, ri->pr_recty);
						}
						else {
							rect[0]= previewback(mat->pr_back, x, y);
							
							if(pr_method!=PR_ICON_RENDER && mat->nodetree && mat->use_nodes) 
								ntreeClearPixelTree(mat->nodetree, x+ri->pr_rectx/2, y+ri->pr_recty/2);
						}
					}
					else if(mat->pr_type==MA_CUBE) {
						if( ray_previewrender(x, y, vec, shi.vn, ri->pr_rectx, ri->pr_recty) ) {
							
							shade_preview_pixel(&shi, vec, x, y, (char *)rect, ri->pr_rectx, ri->pr_recty);
						}
						else {
							rect[0]= previewback(mat->pr_back, x, y);
							
							if(pr_method!=PR_ICON_RENDER && mat->nodetree && mat->use_nodes) 
								ntreeClearPixelTree(mat->nodetree, x+ri->pr_rectx/2, y+ri->pr_recty/2);
						}
					}
					else {
						vec[0]= x*(2.0f/ri->pr_rectx);
						vec[1]= y*(2.0f/ri->pr_recty);
						vec[2]= 0.0;
						
						shi.vn[0]= shi.vn[1]= 0.0f;
						shi.vn[2]= -1.0f;
						
						shade_preview_pixel(&shi, vec, x, y, (char *)rect, ri->pr_rectx, ri->pr_recty);
					}
				}
			}
		}
		else if(tex) {
			for(x=startx; x<endx; x++, rect++) {
				texture_preview_pixel(tex, x, y, (char *)rect, ri->pr_rectx, ri->pr_recty);
			}
		}
		else if(la) {
			for(x=startx; x<endx; x++, rect++) {
				lamp_preview_pixel(&shi, lar, x, y, (char *)rect, ri->pr_rectx, ri->pr_recty);
			}
		}
		else  {
			for(x=startx; x<endx; x++, rect++) {				
				sky_preview_pixel(lens, x, y, (char *)rect, ri->pr_rectx, ri->pr_recty);
			}
		}
		
		if (pr_method!=PR_ICON_RENDER) {
			if(y<endy-2) {
				if(qtest()) {
					addafterqueue(curarea->win, RENDERPREVIEW, 1);
					break;
				}
			}
			display_pr_scanline(ri->rect, ri->cury, ri->pr_rectx);	

			/* flush opengl for cards with frontbuffer slowness */
			if(ri->cury==ri->pr_recty-1 || (PIL_check_seconds_timer() - lasttime > 0.05)) {
				lasttime= PIL_check_seconds_timer();
				glFlush();
			}
		}
		ri->cury++;
	}

	if (pr_method==PR_DRAW_RENDER) {
		if(ri->cury>=ri->pr_recty && tex) 
			draw_tex_crop((Tex*)id);
	
		glDrawBuffer(GL_BACK);
		/* draw again for clean swapbufers */
		previewdraw_render(ri, area);
	}
	
	if(lar) {
		MEM_freeN(lar);
		/*
		MEM_freeN(R.lights.first);
		R.lights.first= R.lights.last= NULL;
		*/
	}
	
	if(mat) {
		end_render_material(mat);
		
		if(mat->nodetree && mat->use_nodes)
			if(ri->cury>=ri->pr_recty)
				allqueue(REDRAWNODE, 0);
	}
}

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
		set_previewrect(sbuts->area->win, PR_XMIN, PR_YMIN, PR_XMAX, PR_YMAX, sbuts->ri->pr_rectx, sbuts->ri->pr_recty); // uses UImat
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
void BIF_previewdraw(void)
{
	SpaceButs *sbuts= curarea->spacedata.first;
	short id_code= 0;
	
	if(sbuts->lockpoin) {
		ID *id= sbuts->lockpoin;
		id_code= GS(id->name);
	}
	
	if (!sbuts->ri) {
		sbuts->ri= MEM_callocN(sizeof(RenderInfo), "butsrenderinfo");
		sbuts->ri->cury = 0;
		sbuts->ri->rect = NULL;
		sbuts->ri->pr_rectx = PREVIEW_RENDERSIZE;
		sbuts->ri->pr_recty = PREVIEW_RENDERSIZE;
	}
	
	if (sbuts->ri->rect==NULL) BIF_preview_changed(id_code);
	else {
		set_previewrect(sbuts->area->win, PR_XMIN, PR_YMIN, PR_XMAX, PR_YMAX, sbuts->ri->pr_rectx, sbuts->ri->pr_recty);
		previewdraw_render(sbuts->ri, sbuts->area);
		end_previewrect();
	}
	if(sbuts->ri->cury==0) BIF_preview_changed(id_code);
	
}


