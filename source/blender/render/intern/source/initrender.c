/**
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


/* Global includes */

#include <math.h>
#include <stdlib.h>
#include <string.h>
#include <stdio.h>

#ifdef HAVE_CONFIG_H
#include <config.h>
#endif

#ifdef WIN32
#include "BLI_winstuff.h"
#endif

#include "blendef.h"
#include "MEM_guardedalloc.h"

#include "PIL_time.h"

#include "BLI_arithb.h"
#include "BLI_blenlib.h"
#include "BLI_rand.h"

#include "MTC_matrixops.h"

#include "DNA_image_types.h"
#include "DNA_camera_types.h"
#include "DNA_lamp_types.h"
#include "DNA_scene_types.h"
#include "DNA_object_types.h"

#include "BKE_utildefines.h"
#include "BKE_global.h"
#include "BKE_material.h"
#include "BKE_object.h"
#include "BKE_image.h"
#include "BKE_ipo.h"
#include "BKE_key.h"
#include "BKE_ika.h"
#include "BKE_action.h"
#include "BKE_writeavi.h"
#include "BKE_scene.h"

#include "BIF_toolbox.h"
#include "BIF_writeavicodec.h"
#include "BIF_writemovie.h"		/* start_movie(), append_movie(), end_movie() */

#include "BSE_drawview.h"
#include "BSE_sequence.h"

#include "IMB_imbuf_types.h"
#include "IMB_imbuf.h"

#ifdef WITH_QUICKTIME
#include "quicktime_export.h"
#endif

#include "SDL_thread.h"

/* this module */
#include "render.h"

#include "RE_callbacks.h"
#include "zbuf.h"
#include "rendercore.h" /* part handler for the old renderer, shading functions */
#include "pixelshading.h"
#include "renderPreAndPost.h"
#include "vanillaRenderPipe.h"
#include "renderHelp.h"
#include "jitter.h"
#include "gammaCorrectionTables.h"

/* Own includes */
#include "initrender.h"

/* yafray: include for yafray export/render */
#include "YafRay_Api.h"


float centLut[16], *fmask1[9], *fmask2[9];
unsigned short *gamtab, *igamtab2, *igamtab1;
char cmask[256], *centmask;

Material defmaterial;

/* ****************** GAMMA, MASKS and LUTS **************** */

static float calc_weight(float *weight, int i, int j)
{
	float x, y, dist, totw= 0.0, fac;
	int a;

	fac= R.r.gauss*R.r.gauss;
	fac*= fac;

	for(a=0; a<R.osa; a++) {
		x= jit[a][0] + i;
		y= jit[a][1] + j;
		dist= sqrt(x*x+y*y);

		weight[a]= 0.0;

		/* gaussian weighting */
		if(R.r.mode & R_GAUSS) {
			if(dist<R.r.gauss) {
				x = dist*R.r.gauss;
				weight[a]= (1.0/exp(x*x) - 1.0/exp(fac));
			}
		}
		else {
			if(i==0 && j==0) weight[a]= 1.0;
		}

		totw+= weight[a];

	}
	return totw;
}

static void init_filt_mask(void)
{
	static int firsttime=1;
	static int lastosa=0;
	static int lastgauss=0;
	static float lastgamma= 0.0;
	float gamma, igamma, flweight[32], fmask[256];
	float weight[32], totw, val, *fpx1, *fpx2, *fpy1, *fpy2, *m3, *m4;
	int i, j, a;

	if(firsttime) {
		firsttime= 0;
		
		for(a=0; a<9;a++) {
			fmask1[a]= MEM_mallocN(256*sizeof(float), "initfilt");
			fmask2[a]= MEM_mallocN(256*sizeof(float), "initfilt");
		}
		for(a=0; a<256; a++) {
			cmask[a]= 0;
			if(a &   1) cmask[a]++;
			if(a &   2) cmask[a]++;
			if(a &   4) cmask[a]++;
			if(a &   8) cmask[a]++;
			if(a &  16) cmask[a]++;
			if(a &  32) cmask[a]++;
			if(a &  64) cmask[a]++;
			if(a & 128) cmask[a]++;
		}
		centmask= MEM_mallocN(65536, "Initfilt3");
		for(a=0; a<16; a++) {
			centLut[a]= -0.45+((float)a)/16.0;
		}

		gamtab= MEM_mallocN(65536*sizeof(short), "initGaus2");
		igamtab1= MEM_mallocN(256*sizeof(short), "initGaus2");
		igamtab2= MEM_mallocN(65536*sizeof(short), "initGaus2");
		
		return;	// this case is called on startup
	}

	if(R.r.alphamode==R_ALPHAKEY) gamma= 1.0;	/* gamma correction of alpha is nasty */

	if(R.r.mode & R_GAMMA) gamma= 2.0;
	else gamma= 1.0;
	igamma= 1.0/gamma;

	if(gamma!= lastgamma) {
		lastgamma= gamma;

		/* gamtab: in short, out short */
		for(a=0; a<65536; a++) {
			val= a;
			val/= 65535.0;

			if(gamma==2.0) val= sqrt(val);
			else if(gamma!=1.0) val= pow(val, igamma);

			gamtab[a]= (65535.99*val);
		}
		/* inverse gamtab1 : in byte, out short */
		for(a=1; a<=256; a++) {
			if(gamma==2.0) igamtab1[a-1]= a*a-1;
			else if(gamma==1.0) igamtab1[a-1]= 256*a-1;
			else {
				val= a/256.0;
				igamtab1[a-1]= (65535.0*pow(val, gamma)) -1 ;
			}
		}

		/* inverse gamtab2 : in short, out short */
		for(a=0; a<65536; a++) {
			val= a;
			val/= 65535.0;
			if(gamma==2.0) val= val*val;
			else val= pow(val, gamma);

			igamtab2[a]= 65535.0*val;
		}
	}

	if(R.osa && (lastosa!=R.osa || lastgauss != (R.r.mode & R_GAUSS)) ) {
		lastosa= R.osa;
		lastgauss= R.r.mode & R_GAUSS;
			
		val= 1.0/((float)R.osa);
		for(a=0; a<256; a++) {
			fmask[a]= ((float)cmask[a])*val;
		}

		for(a=0; a<9;a++) {
			memset(fmask1[a], 0, 256*sizeof(float));
			memset(fmask2[a], 0, 256*sizeof(float));
		}

		/* calculate totw */
		totw= 0.0;
		for(j= -1; j<2; j++) {
			for(i= -1; i<2; i++) {
				totw+= calc_weight(weight, i, j);
			}
		}

		for(j= -1; j<2; j++) {
			for(i= -1; i<2; i++) {
				/* calculate using jit, with offset the weights */

				memset(weight, 0, sizeof(weight));
				calc_weight(weight, i, j);

				for(a=0; a<16; a++) flweight[a]= weight[a]*(1.0/totw);

				m3= fmask1[ 3*(j+1)+i+1 ];
				m4= fmask2[ 3*(j+1)+i+1 ];

				for(a=0; a<256; a++) {
					if(a &   1) {
						m3[a]+= flweight[0];
						m4[a]+= flweight[8];
					}
					if(a &   2) {
						m3[a]+= flweight[1];
						m4[a]+= flweight[9];
					}
					if(a &   4) {
						m3[a]+= flweight[2];
						m4[a]+= flweight[10];
					}
					if(a &   8) {
						m3[a]+= flweight[3];
						m4[a]+= flweight[11];
					}
					if(a &  16) {
						m3[a]+= flweight[4];
						m4[a]+= flweight[12];
					}
					if(a &  32) {
						m3[a]+= flweight[5];
						m4[a]+= flweight[13];
					}
					if(a &  64) {
						m3[a]+= flweight[6];
						m4[a]+= flweight[14];
					}
					if(a & 128) {
						m3[a]+= flweight[7];
						m4[a]+= flweight[15];
					}
				}
			}
		}

		/* centmask: the correct subpixel offset per mask */

		fpx1= MEM_mallocN(256*sizeof(float), "initgauss4");
		fpx2= MEM_mallocN(256*sizeof(float), "initgauss4");
		fpy1= MEM_mallocN(256*sizeof(float), "initgauss4");
		fpy2= MEM_mallocN(256*sizeof(float), "initgauss4");
		for(a=0; a<256; a++) {
			fpx1[a]= fpx2[a]= 0.0;
			fpy1[a]= fpy2[a]= 0.0;
			if(a & 1) {
				fpx1[a]+= jit[0][0];
				fpy1[a]+= jit[0][1];
				fpx2[a]+= jit[8][0];
				fpy2[a]+= jit[8][1];
			}
			if(a & 2) {
				fpx1[a]+= jit[1][0];
				fpy1[a]+= jit[1][1];
				fpx2[a]+= jit[9][0];
				fpy2[a]+= jit[9][1];
			}
			if(a & 4) {
				fpx1[a]+= jit[2][0];
				fpy1[a]+= jit[2][1];
				fpx2[a]+= jit[10][0];
				fpy2[a]+= jit[10][1];
			}
			if(a & 8) {
				fpx1[a]+= jit[3][0];
				fpy1[a]+= jit[3][1];
				fpx2[a]+= jit[11][0];
				fpy2[a]+= jit[11][1];
			}
			if(a & 16) {
				fpx1[a]+= jit[4][0];
				fpy1[a]+= jit[4][1];
				fpx2[a]+= jit[12][0];
				fpy2[a]+= jit[12][1];
			}
			if(a & 32) {
				fpx1[a]+= jit[5][0];
				fpy1[a]+= jit[5][1];
				fpx2[a]+= jit[13][0];
				fpy2[a]+= jit[13][1];
			}
			if(a & 64) {
				fpx1[a]+= jit[6][0];
				fpy1[a]+= jit[6][1];
				fpx2[a]+= jit[14][0];
				fpy2[a]+= jit[14][1];
			}
			if(a & 128) {
				fpx1[a]+= jit[7][0];
				fpy1[a]+= jit[7][1];
				fpx2[a]+= jit[15][0];
				fpy2[a]+= jit[15][1];
			}
		}

		for(a= (1<<R.osa)-1; a>0; a--) {
			val= count_mask(a);
			i= 8+(15.9*(fpy1[a & 255]+fpy2[a>>8])/val);
			CLAMP(i, 0, 15);
			j= 8+(15.9*(fpx1[a & 255]+fpx2[a>>8])/val);
			CLAMP(j, 0, 15);
			i= j + (i<<4);
			centmask[a]= i;
		}

		MEM_freeN(fpx1);
		MEM_freeN(fpx2);
		MEM_freeN(fpy1);
		MEM_freeN(fpy2);
	}
}

static void free_filt_mask()
{
	int a;

	for(a=0; a<9; a++) {
		MEM_freeN(fmask1[a]);
		MEM_freeN(fmask2[a]);
	}
	MEM_freeN(gamtab);
	MEM_freeN(igamtab1);
	MEM_freeN(igamtab2);

	MEM_freeN(centmask);
}

/* unused */

#if 0
void defaultlamp()
{
	LampRen *lar;

	lar= (LampRen *)MEM_callocN(sizeof(LampRen),"lampren");
	R.la[R.totlamp++]=lar;

	lar->type= LA_SUN;
	lar->vec[0]= -R.viewmat[2][0];
	lar->vec[1]= -R.viewmat[2][1];
	lar->vec[2]= -R.viewmat[2][2];
	Normalise(lar->vec);
	lar->r= 1.0;
	lar->g= 1.0;
	lar->b= 1.0;
	lar->lay= 65535;
}
#endif



/* ********************* init calls *********************** */


static void init_def_material(void)
{
	Material *ma;
	
	ma= &defmaterial;
	
	init_material(&defmaterial);
	
	init_render_material(ma);
}

void RE_init_render_data(void)
{
	memset(&R, 0, sizeof(RE_Render));
	
	R.blove= (VertRen **)MEM_callocN(sizeof(void *)*(TABLEINITSIZE),"Blove");
	R.blovl= (VlakRen **)MEM_callocN(sizeof(void *)*(TABLEINITSIZE),"Blovl");
	R.bloha= (HaloRen **)MEM_callocN(sizeof(void *)*(TABLEINITSIZE),"Bloha");
	R.la= (LampRen **)MEM_mallocN(LAMPINITSIZE*sizeof(void *),"renderlamparray");
	
	init_def_material();
	init_filt_mask();
}

void RE_free_render_data()
{
	MEM_freeN(R.blove);
	R.blove= NULL;
	MEM_freeN(R.blovl);
	R.blovl= NULL;
	MEM_freeN(R.bloha);
	R.bloha= NULL;
	MEM_freeN(R.la);
	R.la= NULL;
	if(R.rectot) MEM_freeN(R.rectot);
	if(R.rectftot) MEM_freeN(R.rectftot);
	if(R.rectz) MEM_freeN(R.rectz);
	if(R.rectspare) MEM_freeN(R.rectspare);
	R.rectot= NULL;
	R.rectftot= NULL;
	R.rectz= NULL;
	R.rectspare= NULL;
	
	end_render_material(&defmaterial);
	free_filt_mask();
}

/* ~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~ */

void RE_make_existing_file(char *name)
{
	char di[FILE_MAXDIR], fi[FILE_MAXFILE];

	strcpy(di, name);
	BLI_splitdirstring(di, fi);

	/* test exist */
	if (BLI_exists(di) == 0) {
		BLI_recurdir_fileops(di);
	}
}


/* ~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~ */

void RE_setwindowclip(int mode, int jmode)
{
	extern float bluroffsx, bluroffsy;	// rendercore.c... hackish (ton)
	Camera *cam=NULL;
	float lens, minx, miny, maxx, maxy;
	float xd, yd, afmx, afmy;

	if(G.scene->camera==NULL) return;

	afmx= R.afmx;
	afmy= R.afmy;

	if(mode) {

		if(G.scene->camera->type==OB_LAMP) {
			/* fac= cos( PI*((float)(256- la->spsi))/512.0 ); */

			/* phi= acos(fac); */
			/* lens= 16.0*fac/sin(phi); */
			lens= 35.0;
			R.near= 0.1;
			R.far= 1000.0;
		}
		else if(G.scene->camera->type==OB_CAMERA) {
			cam= G.scene->camera->data;

			lens= cam->lens;
			R.near= cam->clipsta;
			R.far= cam->clipend;
		}
		else {
			lens= 16.0;
		}

		if(R.r.mode & R_ORTHO) {
			if( (R.r.xasp*afmx) >= (R.r.yasp*afmy) ) {
				R.viewfac= 2.0*afmx;
			}
			else {
				R.viewfac= 2.0*R.ycor*afmy;
			}
			/* ortho_scale == 1.0 means exact 1 to 1 mapping */
			R.pixsize= cam->ortho_scale/R.viewfac;
		}
		else {
			if( (R.r.xasp*afmx) >= (R.r.yasp*afmy) ) {
				R.viewfac= (afmx*lens)/16.0;
			}
			else {
				R.viewfac= R.ycor*(afmy*lens)/16.0;
			}
			
			R.pixsize= R.near/R.viewfac;
		}
		
		/* pixsize is not a real global... get rid of it! (ton) */
	}

	/* revision / simplification of subpixel offsets:
	   - the matrix will go without offset from start (e.g. -100) to end (e.g. +99).
	   - filling in with zbuffer will set offset of 0.5. to make sure clipped faces fill in too
	   - in shadepixel() again that 0.5 offset is corrected
	*/
  	minx= R.xstart; 
  	miny= R.ycor*(R.ystart); 
  	maxx= R.xend; 
  	maxy= R.ycor*(R.yend); 
	   
	if(R.flag & R_SEC_FIELD) {
		if(R.r.mode & R_ODDFIELD) {
			miny-= .5*R.ycor;
			maxy-= .5*R.ycor;
		}
		else {
			miny+= .5*R.ycor;
			maxy+= .5*R.ycor;
		}
	}

	xd= yd= 0.0;
	if(jmode!= -1) {
		bluroffsx= xd= jit[jmode % R.osa][0];
		bluroffsy= yd= R.ycor*jit[jmode % R.osa][1];
	}
	else bluroffsx=bluroffsy= 0.0;

	minx= R.pixsize*(minx+xd);
	maxx= R.pixsize*(maxx+xd);
	miny= R.pixsize*(miny+yd);
	maxy= R.pixsize*(maxy+yd);

	if(R.r.mode & R_ORTHO)
		i_ortho(minx, maxx, miny, maxy, R.near, R.far, R.winmat);
	else 
		i_window(minx, maxx, miny, maxy, R.near, R.far, R.winmat);

//	printmatrix4("win", R.winmat);
}


/* ~~~~~~~~~~~~~~~~ PARTS ~~~~~~~~~~~~~~~~~~~~~~ */

/** 
* Part as in part-rendering. An image rendered in parts is rendered
* to a list of parts, with x,y size, and a pointer to the render
* output stored per part. Internal!
*/
typedef struct Part
{
	struct Part *next, *prev;
	unsigned int *rect;		// color 4x8 bits
	float *rectf;			// color 4x32 bits
	unsigned int *rectz;	// zbuffer

	short minx, miny, maxx, maxy, x, y;
} Part;

static void freeparts(void)
{
	Part *part= R.parts.first;
	while(part) {
		if(part->rect) MEM_freeN(part->rect);
		if(part->rectz) MEM_freeN(part->rectz);
		if(part->rectf) MEM_freeN(part->rectf);
		part= part->next;
	}
	BLI_freelistN(&R.parts);
}

static void initparts(void)
{
	Part *pa;
	short nr, xd, yd, xpart, ypart, xparts, yparts;
	short a, xminb, xmaxb, yminb, ymaxb;

	freeparts();
	
	if(R.r.mode & R_BORDER) {
		xminb= R.r.border.xmin*R.rectx;
		xmaxb= R.r.border.xmax*R.rectx;

		yminb= R.r.border.ymin*R.recty;
		ymaxb= R.r.border.ymax*R.recty;

		if(xminb<0) xminb= 0;
		if(xmaxb>R.rectx) xmaxb= R.rectx;
		if(yminb<0) yminb= 0;
		if(ymaxb>R.recty) ymaxb= R.recty;
	}
	else {
		xminb=yminb= 0;
		xmaxb= R.rectx;
		ymaxb= R.recty;
	}

	xparts= R.r.xparts;	/* for border */
	yparts= R.r.yparts;

	xpart= R.rectx/xparts;
	ypart= R.recty/yparts;

	/* if border: test if amount of parts can be fewer */
	if(R.r.mode & R_BORDER) {
		a= (xmaxb-xminb-1)/xpart+1; /* amount of parts in border */
		if(a<xparts) xparts= a;
		a= (ymaxb-yminb-1)/ypart+1; /* amount of parts in border */
		if(a<yparts) yparts= a;

		xpart= (xmaxb-xminb)/xparts;
		ypart= (ymaxb-yminb)/yparts;
	}
	
	for(nr=0; nr<xparts*yparts; nr++) {
		pa= MEM_callocN(sizeof(Part), "new part");
		
		if(R.r.mode & R_PANORAMA) {
			pa->minx= pa->miny= 0;
			pa->maxx= pa->x= R.rectx;
			pa->maxy= pa->y= R.recty;
		}
		else {
			xd= (nr % xparts);
			yd= (nr-xd)/xparts;

			pa->minx= xminb+ xd*xpart;
			pa->miny= yminb+ yd*ypart;
			if(xd<R.r.xparts-1) pa->maxx= pa->minx+xpart;
			else pa->maxx= xmaxb;
			if(yd<R.r.yparts-1) pa->maxy= pa->miny+ypart;
			else pa->maxy= ymaxb;

			pa->x= pa->maxx-pa->minx;
			pa->y= pa->maxy-pa->miny;
		}
		
		if(pa->x>0 && pa->y>0) {
			/* Gauss needs 1 pixel extra to work */
			if(xparts*yparts>1 && (R.r.mode & R_GAUSS)) {
				pa->minx-= 1;
				pa->miny-= 1;
				pa->maxx+= 1;
				pa->maxy+= 1;
				pa->x+= 2;
				pa->y+= 2;
			}
			BLI_addtail(&R.parts, pa);
		}
		else MEM_freeN(pa);
	}
	
}

/* ~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~ */
static void setpart(Part *pa)
{

	R.xstart= pa->minx-R.afmx;
	R.ystart= pa->miny-R.afmy;
	R.xend= pa->maxx-R.afmx;
	R.yend= pa->maxy-R.afmy;
	R.rectx= pa->x;
	R.recty= pa->y;
}

/* ~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~ */
static void addparttorect(Part *pa)
{
	float *rf, *rfp;
	unsigned int *rt, *rtp, *rz, *rzp;
	int y, heigth, len, copylen;

	/* calc the right offset in rects, zbuffer cannot exist... */

	if(pa->rect==NULL) return;
	
	rtp= pa->rect;
	rzp= pa->rectz;
	rfp= pa->rectf;
	
	copylen=len= pa->x;
	heigth= pa->y;
	
	if(R.r.mode & R_GAUSS) {
		
		rtp+= 1+len;
		if(rzp) rzp+= 1+len;
		if(rfp) rfp+= 4*(1+len);
		
		copylen= len-2;
		heigth -= 2;
		rt= R.rectot+ (pa->miny + 1)*R.rectx+ (pa->minx+1);
		rz= R.rectz+ (pa->miny + 1)*R.rectx+ (pa->minx+1);
		rf= R.rectftot+ 4*( (pa->miny + 1)*R.rectx + (pa->minx+1) );
	}
	else {
		rt= R.rectot+ pa->miny*R.rectx+ pa->minx;
		rz= R.rectz+ pa->miny*R.rectx+ pa->minx;
		rf= R.rectftot+ 4*(pa->miny*R.rectx+ pa->minx);
	}

	for(y=0; y<heigth; y++) {
		memcpy(rt, rtp, 4*copylen);
		rt+= R.rectx;
		rtp+= len;
		
		if(rzp) {
			memcpy(rz, rzp, 4*copylen);
			rz+= R.rectx;
			rzp+= len;
		}
		if(rfp) {
			memcpy(rf, rfp, 16*copylen);
			rf+= 4*R.rectx;
			rfp+= 4*len;
		}
	}
}


/* ~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~ */

 
static void add_to_blurbuf(int blur)
{
	static unsigned int *blurrect= 0;
	int tot, gamval;
	short facr, facb;
	char *rtr, *rtb;

	if(blur<0) {
		if(blurrect) {
			if(R.rectot) MEM_freeN(R.rectot);
			R.rectot= blurrect;
			blurrect= 0;
		}
	}
	else if(blur==R.osa-1) {
		/* first time */
		blurrect= MEM_mallocN(R.rectx*R.recty*sizeof(int), "rectblur");
		if(R.rectot) memcpy(blurrect, R.rectot, R.rectx*R.recty*4);
	}
	else if(blurrect) {
		/* accumulate */

		facr= 256/(R.osa-blur);
		facb= 256-facr;

		if(R.rectot) {
			rtr= (char *)R.rectot;
			rtb= (char *)blurrect;
			tot= R.rectx*R.recty;
			while(tot--) {
				if( *((unsigned int *)rtb) != *((unsigned int *)rtr) ) {

					if(R.r.mode & R_GAMMA) {
						gamval= (facr* igamtab2[ rtr[0]<<8 ] + facb* igamtab2[ rtb[0]<<8 ])>>8;
						rtb[0]= gamtab[ gamval ]>>8;
						gamval= (facr* igamtab2[ rtr[1]<<8 ] + facb* igamtab2[ rtb[1]<<8 ])>>8;
						rtb[1]= gamtab[ gamval ]>>8;
						gamval= (facr* igamtab2[ rtr[2]<<8 ] + facb* igamtab2[ rtb[2]<<8 ])>>8;
						rtb[2]= gamtab[ gamval ]>>8;
						gamval= (facr* igamtab2[ rtr[3]<<8 ] + facb* igamtab2[ rtb[3]<<8 ])>>8;
						rtb[3]= gamtab[ gamval ]>>8;
					}
					else {
						rtb[0]= (facr*rtr[0] + facb*rtb[0])>>8;
						rtb[1]= (facr*rtr[1] + facb*rtb[1])>>8;
						rtb[2]= (facr*rtr[2] + facb*rtb[2])>>8;
						rtb[3]= (facr*rtr[3] + facb*rtb[3])>>8;
					}
				}
				rtr+= 4;
				rtb+= 4;
			}
		}
		if(blur==0) {
			/* last time */
			if(R.rectot) MEM_freeN(R.rectot);
			R.rectot= blurrect;
			blurrect= 0;
		}
	}
}


/* yafray: main yafray render/export call */
void yafrayRender()
{
	R.flag |= R_RENDERING;	/* !!! */

	/* all allocs moved here, out of export code */
	/* display rgba buf */
	if (R.rectot) MEM_freeN(R.rectot);
	R.rectot = MEM_callocN(sizeof(int)*R.rectx*R.recty, "rectot");
	/* zbuf */
	if (R.rectz) MEM_freeN(R.rectz);
	R.rectz = (unsigned int *)MEM_mallocN(sizeof(int)*R.rectx*R.recty, "rectz");
	/* float rgba buf */
	if (R.rectftot) MEM_freeN(R.rectftot);
	if (R.r.mode & R_FBUF) R.rectftot= MEM_callocN(4*sizeof(float)*R.rectx*R.recty, "rectftot");

	// switch must be done before prepareScene()
	if (!R.r.YFexportxml)
		YAF_switchFile();
	else
		YAF_switchPlugin();

	printf("Starting scene conversion.\n");
	prepareScene();
	printf("Scene conversion done.\n");

	YAF_exportScene();
	finalizeScene();

	// show postpro effects if floatbuffer used (plugin only)
	if (R.r.YFexportxml) {
		if ((R.r.mode & R_FBUF) && R.rectftot)
			RE_floatbuffer_to_output();
	}
}


/* ~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~ */

// exported to other files, belongs in include... later
SDL_mutex *render_abuf_lock=NULL, *load_ibuf_lock=NULL;


static void renderloop_setblending(void)
{
	
	/* this value should only be set here. do_gamma is for gammablended adding of subpixels */
	do_gamma= 0;
	if(R.r.mode & R_GAMMA) {
		if((R.r.mode & R_OSA)) do_gamma= 1;
	}
	
	/* always call, it does gamma tables used by alphaunder, but call after R.osa and jit was set */
	init_filt_mask();
	
	switch (R.r.alphamode) {
		case R_ALPHAKEY:	
			setSkyBlendingMode(RE_ALPHA_KEY);
			break;
		case R_ALPHAPREMUL:	
			setSkyBlendingMode(RE_ALPHA_PREMUL);
			break;
		default:
			setSkyBlendingMode(RE_ALPHA_SKY);		
	}
	
	/* SHould use slider when the gamma button is pressed. */
	if (do_gamma) {		
		makeGammaTables(2.0);
	} else {
		makeGammaTables(1.0);
	}
	
}

static void mainRenderLoop(void)  /* here the PART and FIELD loops */
{
	Part *pa;
	int blur, fields, fi, totparts, nr;

	/* create mutexes for threaded render */
	render_abuf_lock = SDL_CreateMutex();
	load_ibuf_lock = SDL_CreateMutex();

	if(R.rectz) MEM_freeN(R.rectz);
	R.rectz = NULL;
	if(R.rectftot) MEM_freeN(R.rectftot);
	R.rectftot = NULL;
	
	/* FIELD LOOP */
	totparts= R.r.xparts*R.r.yparts;
	fields= 1;

	if(R.r.mode & R_FIELDS) {
		fields= 2;
		R.rectf1= R.rectf2= NULL;	/* field rects */
		R.r.ysch/= 2;
		R.afmy/= 2;
		R.r.yasp*= 2;
		R.ycor= ( (float)R.r.yasp)/( (float)R.r.xasp);

	}
	
	for(fi=0; fi<fields; fi++) {

		/* INIT */
		BLI_srand( 2*(G.scene->r.cfra)+fi);
			
		R.flag|= R_RENDERING;
		if(fi==1) R.flag |= R_SEC_FIELD;
	
		/* MOTIONBLUR loop */
		if(R.r.mode & R_MBLUR) blur= R.osa;
		else blur= 1;
		while(blur--) {

			/* WINDOW */
			R.rectx= R.r.xsch;
			R.recty= R.r.ysch;
			R.xstart= -R.afmx;
			R.ystart= -R.afmy;
			R.xend= R.xstart+R.rectx-1;
			R.yend= R.ystart+R.recty-1;


			if(R.r.mode & R_MBLUR) set_mblur_offs(R.osa-blur);

			initparts(); /* always do, because of border */
			setpart(R.parts.first);

			RE_local_init_render_display();
			RE_local_clear_render_display(R.win);
			RE_local_timecursor((G.scene->r.cfra));

			prepareScene();

			/* PARTS LOOP */
			nr= 0;
			for(pa= R.parts.first; pa; pa= pa->next, nr++) {

				if(RE_local_test_break()) break;

				setpart(pa);

				if(R.r.mode & R_MBLUR) RE_setwindowclip(0, blur);
				else RE_setwindowclip(0,-1);

				if(R.r.mode & R_PANORAMA) setPanoRot(nr);

				/* HOMOGENIC COORDINATES AND ZBUF AND CLIP OPTIMISATION (per part) */
				/* There may be some interference with z-coordinate    */
				/* calculation here?                                   */

				doClipping(RE_projectverto);
				if(RE_local_test_break()) break;
				
				/* rectot is for result and integer face indices */
				if(R.rectot) MEM_freeN(R.rectot);
				R.rectot= MEM_callocN(sizeof(int)*R.rectx*R.recty, "rectot");
				
				if(R.rectftot) MEM_freeN(R.rectftot);
				if(R.r.mode & R_FBUF) R.rectftot= MEM_callocN(4*sizeof(float)*R.rectx*R.recty, "rectftot");
				
				if(R.r.mode & R_MBLUR) {
					RE_local_printrenderinfo(0.0, R.osa - blur);
					if(G.background && blur<R.osa) printf("\n"); // newline for percentage print
				}
				else RE_local_printrenderinfo(0.0, -1);

				if(R.r.mode & R_UNIFIED) {
					zBufShadeAdvanced();
				}
				else {
					if(R.rectz) MEM_freeN(R.rectz);
					R.rectz =  (unsigned int *)MEM_mallocN(sizeof(int)*R.rectx*R.recty, "rectz");

					if(R.r.mode & R_OSA) zbufshadeDA();
					else zbufshade();
				}
				
				/* exception */
				if( (R.r.mode & R_BORDER) && (R.r.mode & R_MOVIECROP));
				else {
					/* HANDLE PART OR BORDER */
					if(totparts>1 || (R.r.mode & R_BORDER)) {

						pa->rect= R.rectot;
						R.rectot= NULL;
						pa->rectf= R.rectftot;
						R.rectftot= NULL;
						pa->rectz= R.rectz;
						R.rectz= NULL;
					}
				}

				if(RE_local_test_break()) break;
			}

			/* JOIN PARTS OR INSERT BORDER */

			/* exception: crop */
			if( (R.r.mode & R_BORDER) && (R.r.mode & R_MOVIECROP)) ;
			else {
				R.rectx= R.r.xsch;
				R.recty= R.r.ysch;

				if(R.r.mode & R_PANORAMA) R.rectx*= R.r.xparts;

				if(totparts>1 || (R.r.mode & R_BORDER)) {
					int a;
					
					if(R.rectot) MEM_freeN(R.rectot);
					if(R.rectftot) MEM_freeN(R.rectftot);
					if(R.rectz) MEM_freeN(R.rectz);
					
					R.rectot= MEM_callocN(sizeof(int)*R.rectx*R.recty, "rectot");
					R.rectz= MEM_callocN(sizeof(int)*R.rectx*R.recty, "rectz");
					if(R.r.mode & R_FBUF) R.rectftot= MEM_callocN(4*sizeof(float)*R.rectx*R.recty, "rectftot");
					else R.rectftot= NULL;
					
					for(a=0, pa= R.parts.first; pa; pa= pa->next, a++) {
						
						if(R.r.mode & R_PANORAMA) {		// pano is fake parts...
							pa->minx += a*R.r.xsch;
							pa->maxx += a*R.r.xsch;
						}
						addparttorect(pa);
					}
				}
			}
			
			freeparts();
			
			if( (R.flag & R_HALO)) {
				if(RE_local_test_break()==0) add_halo_flare();
			}

			if(R.r.mode & R_MBLUR) {
				add_to_blurbuf(blur);
			}

			/* END (blur loop) */
			finalizeScene();

			if(RE_local_test_break()) break;
		}

		/* definite free */
		add_to_blurbuf(-1);

		/* HANDLE FIELD */
		if(R.r.mode & R_FIELDS) {
			if(R.flag & R_SEC_FIELD) R.rectf2= R.rectot;
			else R.rectf1= R.rectot;
			R.rectot= NULL;
		}

		if(RE_local_test_break()) break;
	}

	/* JOIN FIELDS */
	if(R.r.mode & R_FIELDS) {
		R.r.ysch*= 2;
		R.afmy*= 2;
		R.recty*= 2;
		R.r.yasp/=2;

		if(R.rectot) MEM_freeN(R.rectot);	/* happens when a render has been stopped */
		R.rectot=(unsigned int *)MEM_callocN(sizeof(int)*R.rectx*R.recty, "rectot");
		
		if(RE_local_test_break()==0) {
			unsigned int *rt, *rt1, *rt2;
			int len, a;
			
			rt= R.rectot;

			if(R.r.mode & R_ODDFIELD) {
				rt2= R.rectf1;
				rt1= R.rectf2;
			}
			else {
				rt1= R.rectf1;
				rt2= R.rectf2;
			}

			len= 4*R.rectx;

			for(a=0; a<R.recty; a+=2) {
				memcpy(rt, rt1, len);
				rt+= R.rectx;
				rt1+= R.rectx;
				memcpy(rt, rt2, len);
				rt+= R.rectx;
				rt2+= R.rectx;
			}
		}
		
		if(R.rectf1) MEM_freeN(R.rectf1);
		R.rectf1= NULL;
		if(R.rectf2) MEM_freeN(R.rectf2);
		R.rectf2= NULL;
		/* fbuf and zbuf free, image size differs now */
		if(R.rectftot) MEM_freeN(R.rectftot);
		R.rectftot= NULL;
		if(R.rectz) MEM_freeN(R.rectz);
		R.rectz= NULL;
		
	}

	/* if border: still do skybuf */
	if(R.r.mode & R_BORDER) {
		if( (R.r.mode & R_MOVIECROP)==0) {
			if(R.r.bufflag & 1) {
				unsigned int *rt;
				int x, y;
				
				R.xstart= -R.afmx;
				R.ystart= -R.afmy;
				rt= R.rectot;
				for(y=0; y<R.recty; y++) {
					for(x=0; x<R.rectx; x++, rt++) {
						if(*rt==0) fillBackgroundImageChar((char *)rt, x, y);
					}
				}
			}
		}
	}

	set_mblur_offs(0);

	/* mutexes free */
	SDL_DestroyMutex(load_ibuf_lock);
	SDL_DestroyMutex(render_abuf_lock);
	load_ibuf_lock= NULL;
	render_abuf_lock= NULL;
}

void render() {
	/* yafray: render, see above */
	if (R.r.renderer==R_YAFRAY)
		yafrayRender();
	else
		mainRenderLoop();
}


/* ~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~ */

void RE_initrender(struct View3D *ogl_render_view3d)
{
	double start_time;
	Image *bima;
	char name[256];
	
	/* scene data to R */
	R.r= G.scene->r;
	R.r.postigamma= 1.0/R.r.postgamma;
	
	/* WINDOW size (sch='scherm' dutch for screen...) */
	R.r.xsch= (R.r.size*R.r.xsch)/100;
	R.r.ysch= (R.r.size*R.r.ysch)/100;
	
	R.afmx= R.r.xsch/2;
	R.afmy= R.r.ysch/2;
	
	/* to be sure: when a premature return (rectx can differ from xsch) */
	R.rectx= R.r.xsch;
	R.recty= R.r.ysch;
	
	/* IS RENDERING ALLOWED? */
	
	/* forbidden combination */
	if(R.r.mode & R_PANORAMA) {
		if(R.r.mode & R_BORDER) {
			error("No border supported for Panorama");
			G.afbreek= 1;
		}
		if(R.r.yparts>1) {
			error("No Y-Parts supported for Panorama");
			G.afbreek= 1;
		}
		if(R.r.mode & R_ORTHO) {
			error("No Ortho render possible for Panorama");
			G.afbreek= 1;
		}
	}

	if(R.r.mode & R_BORDER) {
		if(R.r.border.xmax <= R.r.border.xmin || 
			R.r.border.ymax <= R.r.border.ymin) {
			error("No border area selected.");
			G.afbreek= 1;
		}
	}
	
	if(R.r.xparts*R.r.yparts>=2 && (R.r.mode & R_MOVIECROP) && (R.r.mode & R_BORDER)) {
		error("Combination of border, crop and parts not allowed");
		G.afbreek= 1;
		return;
	}
	
	if(R.r.xparts*R.r.yparts>64) {
		error("No more than 64 parts supported");
		G.afbreek= 1;
		return;
	}
	
	if(R.r.yparts>1 && (R.r.mode & R_PANORAMA)) {
		error("No Y-Parts supported for Panorama");
		G.afbreek= 1;
		return;
	}
	
	if(G.afbreek==1) return;
	
	/* TEST BACKBUF */
	/* If an image is specified for use as backdrop, that image is loaded    */
	/* here.                                                                 */
	if((R.r.bufflag & 1) && (G.scene->r.scemode & R_OGL)==0) {
		if(R.r.alphamode == R_ADDSKY) {
			strcpy(name, R.r.backbuf);
			BLI_convertstringcode(name, G.sce, G.scene->r.cfra);
			
			if(R.backbuf) {
				R.backbuf->id.us--;
				bima= R.backbuf;
			}
			else bima= NULL;
			
			R.backbuf= add_image(name);
			
			if(bima && bima->id.us<1) {
				free_image_buffers(bima);
			}

			if(R.backbuf && R.backbuf->ibuf==NULL) {
				R.backbuf->ibuf= IMB_loadiffname(R.backbuf->name, IB_rect);
				if(R.backbuf->ibuf==NULL) R.backbuf->ok= 0;
				else R.backbuf->ok= 1;
			}
			if(R.backbuf==NULL || R.backbuf->ok==0) {
				// error() doesnt work with render window open
				//error("No backbuf there!");
				printf("Error: No backbuf %s\n", name);
			}
		}
	}
	
	if(R.r.mode & (R_OSA|R_MBLUR)) {
		R.osa= R.r.osa;
		if(R.osa>16) R.osa= 16;
		
		init_render_jit(R.osa);
		
	}
	else R.osa= 0;
	
	renderloop_setblending();	// alpha, sky, gamma
	
	/* when rendered without camera object */
	/* it has to done here because of envmaps */
	R.near= 0.1;
	R.far= 1000.0;
	
	
	if(R.afmx<1 || R.afmy<1) {
		error("Image too small");
		return;
	}
	R.ycor= ( (float)R.r.yasp)/( (float)R.r.xasp);
	
	start_time= PIL_check_seconds_timer();
	
	if(R.r.scemode & R_DOSEQ) {
		R.rectx= R.r.xsch;
		R.recty= R.r.ysch;
		if(R.r.mode & R_PANORAMA) {
			R.rectx*= R.r.xparts;
		}
		
		if(R.rectot) MEM_freeN(R.rectot);
		R.rectot= (unsigned int *)MEM_callocN(sizeof(int)*R.rectx*R.recty, "rectot");
		
		if(R.rectftot) MEM_freeN(R.rectftot);
		R.rectftot= NULL;
		
		RE_local_timecursor((G.scene->r.cfra));
		
		if(RE_local_test_break()==0) do_render_seq();
		
		/* display */
		if(R.rectot) RE_local_render_display(0, R.recty-1, R.rectx, R.recty,R.rectot);
	}
	else if(R.r.scemode & R_OGL) {
		R.rectx= R.r.xsch;
		R.recty= R.r.ysch;
		
		if(R.rectot) MEM_freeN(R.rectot);
		R.rectot= (unsigned int *)MEM_callocN(sizeof(int)*R.rectx*R.recty, "rectot");
		
		if(R.rectftot) MEM_freeN(R.rectftot);
		R.rectftot= NULL;
		
		RE_local_init_render_display();
		drawview3d_render(ogl_render_view3d);
	}
	else {
		if(G.scene->camera==0) {
			G.scene->camera= scene_find_camera(G.scene);
		}
		
		if(G.scene->camera==0) {
			error("No camera");
			/* needed because R.rectx and R.recty can be unmatching R.rectot */
			
			if(R.rectot) MEM_freeN(R.rectot);
			R.rectot= NULL;
			if(R.rectftot) MEM_freeN(R.rectftot);
			R.rectftot= NULL;
			
			G.afbreek=1;
			return;
		}
		else {
			
			if(G.scene->camera->type==OB_CAMERA) {
				Camera *cam= G.scene->camera->data;
				if(cam->type==CAM_ORTHO) R.r.mode |= R_ORTHO;
			}
			
			render(); /* returns with complete rect xsch-ysch */
		}
	}
	
	/* display again: fields/seq/parts/pano etc */
	if(R.rectot) {
		RE_local_init_render_display();
		RE_local_render_display(0, R.recty-1, R.rectx, R.recty, R.rectot);
	}
	else RE_local_clear_render_display(R.win);
	
	if ((G.scene->r.scemode & R_OGL)==0) /* header gets scrabled if renderwindow holds OGL context */	
		RE_local_printrenderinfo((PIL_check_seconds_timer() - start_time), -1);
	
	/* grms... this is a nasty global */
	do_gamma= 0;
	
	/* these flags remain on, until reset in caller to render (renderwin.c) */
	R.flag &= (R_RENDERING|R_ANIMRENDER|R_REDRAW_PRV);
}

void RE_animrender(struct View3D *ogl_render_view3d)
{
	int cfrao;
	char name[256];

	if(G.scene==NULL) return;
	if(G.scene->r.sfra > G.scene->r.efra) {
		error("Startframe larger than Endframe");
		return;
	}
	
	/* scenedata to R: (for backbuf, R.rectx etc) */
	R.r= G.scene->r;

	/* START ANIMLOOP, everywhere NOT the cfra from R.r is gebruikt: because of rest blender */
	cfrao= (G.scene->r.cfra);

	if(G.scene->r.scemode & R_OGL) R.r.mode &= ~R_PANORAMA;
	
	// these calculations apply for all movie formats
	R.rectx= (R.r.size*R.r.xsch)/100;
	R.recty= (R.r.size*R.r.ysch)/100;
	if(R.r.mode & R_PANORAMA) {
		R.rectx*= R.r.xparts;
		R.recty*= R.r.yparts;
	}
	if(R.r.mode & R_MOVIECROP) {
		initparts();
		setpart(R.parts.first);		// this will adjust r.rectx
	}

	if (0) {
#ifdef __sgi
	} else if (R.r.imtype==R_MOVIE) {
		start_movie();
#endif
#if defined(_WIN32) && !defined(FREE_WINDOWS)
	} else if (R.r.imtype == R_AVICODEC) {
		start_avi_codec();
#endif
#if WITH_QUICKTIME
	} else if (R.r.imtype == R_QUICKTIME) {
		start_qt();
#endif
	} else if ELEM4(R.r.imtype, R_AVIRAW, R_AVIJPEG, R_MOVIE, R_AVICODEC) {
		if ELEM(R.r.imtype, R_MOVIE, R_AVICODEC) {
			printf("Selected movie format not supported on this platform,\nusing RAW AVI instead\n");
		}
		start_avi();
	}
// set initial conditions for softbodies here
// ******************************************
	for((G.scene->r.cfra)=(G.scene->r.sfra); (G.scene->r.cfra)<=(G.scene->r.efra); (G.scene->r.cfra)++) {
		double starttime= PIL_check_seconds_timer();

		R.flag |= R_ANIMRENDER; // unused now (ton)

		RE_initrender(ogl_render_view3d);
		
		/* WRITE IMAGE */
		if(RE_local_test_break()==0) {

			if (0) {
#ifdef __sgi
			} else if (R.r.imtype == R_MOVIE) {
				append_movie((G.scene->r.cfra));
#endif
#if defined(_WIN32) && !defined(FREE_WINDOWS)
			} else if (R.r.imtype == R_AVICODEC) {
				append_avi_codec((G.scene->r.cfra));
#endif
#ifdef WITH_QUICKTIME
			} else if (R.r.imtype == R_QUICKTIME) {
				append_qt((G.scene->r.cfra));
#endif
			} else if ELEM4(R.r.imtype, R_AVIRAW, R_AVIJPEG, R_MOVIE, R_AVICODEC) {
				append_avi((G.scene->r.cfra));
			} else {
				makepicstring(name, (G.scene->r.cfra));
				schrijfplaatje(name);
				if(RE_local_test_break()==0) printf("Saved: %s", name);
			}

			timestr(PIL_check_seconds_timer()-starttime, name);
			printf(" Time: %s\n", name);
			fflush(stdout); /* needed for renderd !! */
		}

		if(G.afbreek==1) break;

	}

	G.scene->r.cfra= cfrao;

	/* restore time */
	if(R.r.mode & (R_FIELDS|R_MBLUR)) {
		do_all_ipos();
		do_all_keys();
		do_all_actions();
		do_all_ikas();
	}

	if (0) {
#ifdef __sgi	
	} else if (R.r.imtype==R_MOVIE) {
		end_movie();
#endif
#if defined(_WIN32) && !defined(FREE_WINDOWS)
	} else if (R.r.imtype == R_AVICODEC) {
		end_avi_codec();
#endif
#ifdef WITH_QUICKTIME
	} else if (R.r.imtype == R_QUICKTIME) {
		end_qt();
#endif
	} else if ELEM4(R.r.imtype, R_AVIRAW, R_AVIJPEG, R_MOVIE, R_AVICODEC) {
		end_avi();
	}
}

/* *************************************************** */
/* ******************* Screendumps ******************** */
/* moved to the windowControl thing */
