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
 * ***** END GPL/BL DUAL LICENSE BLOCK *****
 */

/** \file blender/render/intern/source/imagetexture.c
 *  \ingroup render
 */

#include <stdio.h>
#include <string.h>
#include <fcntl.h>
#include <math.h>
#include <float.h>
#ifndef WIN32 
#include <unistd.h>
#else
#include <io.h>
#endif

#include "MEM_guardedalloc.h"

#include "IMB_imbuf_types.h"
#include "IMB_imbuf.h"

#include "DNA_image_types.h"
#include "DNA_scene_types.h"
#include "DNA_texture_types.h"

#include "BLI_math.h"
#include "BLI_blenlib.h"
#include "BLI_threads.h"
#include "BLI_utildefines.h"

#include "BKE_global.h"
#include "BKE_main.h"
#include "BKE_image.h"
#include "BKE_texture.h"
#include "BKE_library.h"

#include "RE_render_ext.h"

#include "renderpipeline.h"
#include "render_types.h"
#include "texture.h"

/* ~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~ */
/* defined in pipeline.c, is hardcopy of active dynamic allocated Render */
/* only to be used here in this file, it's for speed */
extern struct Render R;
/* ~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~ */

static void boxsample(ImBuf *ibuf, float minx, float miny, float maxx, float maxy, TexResult *texres, const short imaprepeat, const short imapextend);

/* *********** IMAGEWRAPPING ****************** */


/* x and y have to be checked for image size */
static void ibuf_get_color(float col[4], struct ImBuf *ibuf, int x, int y)
{
	int ofs = y * ibuf->x + x;
	
	if (ibuf->rect_float) {
		if (ibuf->channels==4) {
			float *fp= ibuf->rect_float + 4*ofs;
			copy_v4_v4(col, fp);
		}
		else if (ibuf->channels==3) {
			float *fp= ibuf->rect_float + 3*ofs;
			copy_v3_v3(col, fp);
			col[3]= 1.0f;
		}
		else {
			float *fp= ibuf->rect_float + ofs;
			col[0]= col[1]= col[2]= col[3]= *fp;
		}
	}
	else {
		char *rect = (char *)( ibuf->rect+ ofs);

		col[0] = ((float)rect[0])*(1.0f/255.0f);
		col[1] = ((float)rect[1])*(1.0f/255.0f);
		col[2] = ((float)rect[2])*(1.0f/255.0f);
		col[3] = ((float)rect[3])*(1.0f/255.0f);
	}	
}

int imagewrap(Tex *tex, Image *ima, ImBuf *ibuf, const float texvec[3], TexResult *texres)
{
	float fx, fy, val1, val2, val3;
	int x, y, retval;
	int xi, yi; /* original values */

	texres->tin= texres->ta= texres->tr= texres->tg= texres->tb= 0.0f;
	
	/* we need to set retval OK, otherwise texture code generates normals itself... */
	retval= texres->nor?3:1;
	
	/* quick tests */
	if (ibuf==NULL && ima==NULL)
		return retval;
	if (ima) {
		
		/* hack for icon render */
		if (ima->ibufs.first==NULL && (R.r.scemode & R_NO_IMAGE_LOAD))
			return retval;
		
		ibuf= BKE_image_get_ibuf(ima, &tex->iuser);

		ima->flag|= IMA_USED_FOR_RENDER;
	}
	if (ibuf==NULL || (ibuf->rect==NULL && ibuf->rect_float==NULL))
		return retval;
	
	/* setup mapping */
	if (tex->imaflag & TEX_IMAROT) {
		fy= texvec[0];
		fx= texvec[1];
	}
	else {
		fx= texvec[0];
		fy= texvec[1];
	}
	
	if (tex->extend == TEX_CHECKER) {
		int xs, ys;
		
		xs= (int)floor(fx);
		ys= (int)floor(fy);
		fx-= xs;
		fy-= ys;

		if ( (tex->flag & TEX_CHECKER_ODD)==0) {
			if ((xs+ys) & 1);else return retval;
		}
		if ( (tex->flag & TEX_CHECKER_EVEN)==0) {
			if ((xs+ys) & 1) return retval;
		}
		/* scale around center, (0.5, 0.5) */
		if (tex->checkerdist<1.0f) {
			fx= (fx-0.5f)/(1.0f-tex->checkerdist) +0.5f;
			fy= (fy-0.5f)/(1.0f-tex->checkerdist) +0.5f;
		}
	}

	x= xi= (int)floorf(fx*ibuf->x);
	y= yi= (int)floorf(fy*ibuf->y);

	if (tex->extend == TEX_CLIPCUBE) {
		if (x<0 || y<0 || x>=ibuf->x || y>=ibuf->y || texvec[2]<-1.0f || texvec[2]>1.0f) {
			return retval;
		}
	}
	else if ( tex->extend==TEX_CLIP || tex->extend==TEX_CHECKER) {
		if (x<0 || y<0 || x>=ibuf->x || y>=ibuf->y) {
			return retval;
		}
	}
	else {
		if (tex->extend==TEX_EXTEND) {
			if (x>=ibuf->x) x = ibuf->x-1;
			else if (x<0) x= 0;
		}
		else {
			x= x % ibuf->x;
			if (x<0) x+= ibuf->x;
		}
		if (tex->extend==TEX_EXTEND) {
			if (y>=ibuf->y) y = ibuf->y-1;
			else if (y<0) y= 0;
		}
		else {
			y= y % ibuf->y;
			if (y<0) y+= ibuf->y;
		}
	}
	
	/* warning, no return before setting back! */
	if ( (R.flag & R_SEC_FIELD) && (ibuf->flags & IB_fields) ) {
		ibuf->rect+= (ibuf->x*ibuf->y);
	}

	/* keep this before interpolation [#29761] */
	if (tex->imaflag & TEX_USEALPHA) {
		if ((tex->imaflag & TEX_CALCALPHA) == 0) {
			texres->talpha = TRUE;
		} 
	}

	/* interpolate */
	if (tex->imaflag & TEX_INTERPOL) {
		float filterx, filtery;
		filterx = (0.5f * tex->filtersize) / ibuf->x;
		filtery = (0.5f * tex->filtersize) / ibuf->y;

		/* important that this value is wrapped [#27782]
		 * this applies the modifications made by the checks above,
		 * back to the floating point values */
		fx -= (float)(xi - x) / (float)ibuf->x;
		fy -= (float)(yi - y) / (float)ibuf->y;

		boxsample(ibuf, fx-filterx, fy-filtery, fx+filterx, fy+filtery, texres, (tex->extend==TEX_REPEAT), (tex->extend==TEX_EXTEND));
	}
	else { /* no filtering */
		ibuf_get_color(&texres->tr, ibuf, x, y);
	}
	
	if ( (R.flag & R_SEC_FIELD) && (ibuf->flags & IB_fields) ) {
		ibuf->rect-= (ibuf->x*ibuf->y);
	}

	if (texres->nor) {
		if (tex->imaflag & TEX_NORMALMAP) {
			// qdn: normal from color
			// The invert of the red channel is to make
			// the normal map compliant with the outside world.
			// It needs to be done because in Blender
			// the normal used in the renderer points inward. It is generated
			// this way in calc_vertexnormals(). Should this ever change
			// this negate must be removed.
			texres->nor[0] = -2.f*(texres->tr - 0.5f);
			texres->nor[1] = 2.f*(texres->tg - 0.5f);
			texres->nor[2] = 2.f*(texres->tb - 0.5f);
		}
		else {
			/* bump: take three samples */
			val1= texres->tr+texres->tg+texres->tb;

			if (x<ibuf->x-1) {
				float col[4];
				ibuf_get_color(col, ibuf, x+1, y);
				val2= (col[0]+col[1]+col[2]);
			}
			else val2= val1;

			if (y<ibuf->y-1) {
				float col[4];
				ibuf_get_color(col, ibuf, x, y+1);
				val3= (col[0]+col[1]+col[2]);
			}
			else val3= val1;

			/* do not mix up x and y here! */
			texres->nor[0]= (val1-val2);
			texres->nor[1]= (val1-val3);
		}
	}

	if (texres->talpha) texres->tin= texres->ta;
	else if (tex->imaflag & TEX_CALCALPHA) {
		texres->ta= texres->tin= MAX3(texres->tr, texres->tg, texres->tb);
	}
	else texres->ta= texres->tin= 1.0;
	
	if (tex->flag & TEX_NEGALPHA) texres->ta= 1.0f-texres->ta;

	/* de-premul, this is being premulled in shade_input_do_shade() */
	if (texres->ta!=1.0f && texres->ta>1e-4f) {
		fx= 1.0f/texres->ta;
		texres->tr*= fx;
		texres->tg*= fx;
		texres->tb*= fx;
	}
	
	BRICONTRGB;
	
	return retval;
}

static void clipx_rctf_swap(rctf *stack, short *count, float x1, float x2)
{
	rctf *rf, *newrct;
	short a;

	a= *count;
	rf= stack;
	for (;a>0;a--) {
		if (rf->xmin<x1) {
			if (rf->xmax<x1) {
				rf->xmin+= (x2-x1);
				rf->xmax+= (x2-x1);
			}
			else {
				if (rf->xmax>x2) rf->xmax = x2;
				newrct= stack+ *count;
				(*count)++;

				newrct->xmax = x2;
				newrct->xmin = rf->xmin+(x2-x1);
				newrct->ymin = rf->ymin;
				newrct->ymax = rf->ymax;
				
				if (newrct->xmin ==newrct->xmax) (*count)--;
				
				rf->xmin = x1;
			}
		}
		else if (rf->xmax>x2) {
			if (rf->xmin>x2) {
				rf->xmin-= (x2-x1);
				rf->xmax-= (x2-x1);
			}
			else {
				if (rf->xmin<x1) rf->xmin = x1;
				newrct= stack+ *count;
				(*count)++;

				newrct->xmin = x1;
				newrct->xmax = rf->xmax-(x2-x1);
				newrct->ymin = rf->ymin;
				newrct->ymax = rf->ymax;

				if (newrct->xmin ==newrct->xmax) (*count)--;

				rf->xmax = x2;
			}
		}
		rf++;
	}

}

static void clipy_rctf_swap(rctf *stack, short *count, float y1, float y2)
{
	rctf *rf, *newrct;
	short a;

	a= *count;
	rf= stack;
	for (;a>0;a--) {
		if (rf->ymin<y1) {
			if (rf->ymax<y1) {
				rf->ymin+= (y2-y1);
				rf->ymax+= (y2-y1);
			}
			else {
				if (rf->ymax>y2) rf->ymax = y2;
				newrct= stack+ *count;
				(*count)++;

				newrct->ymax = y2;
				newrct->ymin = rf->ymin+(y2-y1);
				newrct->xmin = rf->xmin;
				newrct->xmax = rf->xmax;

				if (newrct->ymin==newrct->ymax) (*count)--;

				rf->ymin = y1;
			}
		}
		else if (rf->ymax>y2) {
			if (rf->ymin>y2) {
				rf->ymin-= (y2-y1);
				rf->ymax-= (y2-y1);
			}
			else {
				if (rf->ymin<y1) rf->ymin = y1;
				newrct= stack+ *count;
				(*count)++;

				newrct->ymin = y1;
				newrct->ymax = rf->ymax-(y2-y1);
				newrct->xmin = rf->xmin;
				newrct->xmax = rf->xmax;

				if (newrct->ymin==newrct->ymax) (*count)--;

				rf->ymax = y2;
			}
		}
		rf++;
	}
}

static float square_rctf(rctf *rf)
{
	float x, y;

	x= rf->xmax- rf->xmin;
	y= rf->ymax- rf->ymin;
	return (x*y);
}

static float clipx_rctf(rctf *rf, float x1, float x2)
{
	float size;

	size= rf->xmax - rf->xmin;

	if (rf->xmin<x1) {
		rf->xmin = x1;
	}
	if (rf->xmax>x2) {
		rf->xmax = x2;
	}
	if (rf->xmin > rf->xmax) {
		rf->xmin = rf->xmax;
		return 0.0;
	}
	else if (size!=0.0f) {
		return (rf->xmax - rf->xmin)/size;
	}
	return 1.0;
}

static float clipy_rctf(rctf *rf, float y1, float y2)
{
	float size;

	size= rf->ymax - rf->ymin;

	if (rf->ymin<y1) {
		rf->ymin = y1;
	}
	if (rf->ymax>y2) {
		rf->ymax = y2;
	}

	if (rf->ymin > rf->ymax) {
		rf->ymin = rf->ymax;
		return 0.0;
	}
	else if (size!=0.0f) {
		return (rf->ymax - rf->ymin)/size;
	}
	return 1.0;

}

static void boxsampleclip(struct ImBuf *ibuf, rctf *rf, TexResult *texres)
{
	/* sample box, is clipped already, and minx etc. have been set at ibuf size.
	 * Enlarge with antialiased edges of the pixels */

	float muly, mulx, div, col[4];
	int x, y, startx, endx, starty, endy;

	startx= (int)floor(rf->xmin);
	endx= (int)floor(rf->xmax);
	starty= (int)floor(rf->ymin);
	endy= (int)floor(rf->ymax);

	if (startx < 0) startx= 0;
	if (starty < 0) starty= 0;
	if (endx>=ibuf->x) endx= ibuf->x-1;
	if (endy>=ibuf->y) endy= ibuf->y-1;

	if (starty==endy && startx==endx) {
		ibuf_get_color(&texres->tr, ibuf, startx, starty);
	}
	else {
		div= texres->tr= texres->tg= texres->tb= texres->ta= 0.0;
		for (y=starty; y<=endy; y++) {
			
			muly= 1.0;

			if (starty==endy);
			else {
				if (y==starty) muly= 1.0f-(rf->ymin - y);
				if (y==endy) muly= (rf->ymax - y);
			}
			
			if (startx==endx) {
				mulx= muly;
				
				ibuf_get_color(col, ibuf, startx, y);

				texres->ta+= mulx*col[3];
				texres->tr+= mulx*col[0];
				texres->tg+= mulx*col[1];
				texres->tb+= mulx*col[2];
				div+= mulx;
			}
			else {
				for (x=startx; x<=endx; x++) {
					mulx= muly;
					if (x==startx) mulx*= 1.0f-(rf->xmin - x);
					if (x==endx) mulx*= (rf->xmax - x);

					ibuf_get_color(col, ibuf, x, y);
					
					if (mulx==1.0f) {
						texres->ta+= col[3];
						texres->tr+= col[0];
						texres->tg+= col[1];
						texres->tb+= col[2];
						div+= 1.0f;
					}
					else {
						texres->ta+= mulx*col[3];
						texres->tr+= mulx*col[0];
						texres->tg+= mulx*col[1];
						texres->tb+= mulx*col[2];
						div+= mulx;
					}
				}
			}
		}

		if (div!=0.0f) {
			div= 1.0f/div;
			texres->tb*= div;
			texres->tg*= div;
			texres->tr*= div;
			texres->ta*= div;
		}
		else {
			texres->tr= texres->tg= texres->tb= texres->ta= 0.0f;
		}
	}
}

static void boxsample(ImBuf *ibuf, float minx, float miny, float maxx, float maxy, TexResult *texres, const short imaprepeat, const short imapextend)
{
	/* Sample box, performs clip. minx etc are in range 0.0 - 1.0 .
	 * Enlarge with antialiased edges of pixels.
	 * If variable 'imaprepeat' has been set, the
	 * clipped-away parts are sampled as well.
	 */
	/* note: actually minx etc isn't in the proper range... this due to filter size and offset vectors for bump */
	/* note: talpha must be initialized */
	/* note: even when 'imaprepeat' is set, this can only repeate once in any direction.
	 * the point which min/max is derived from is assumed to be wrapped */
	TexResult texr;
	rctf *rf, stack[8];
	float opp, tot, alphaclip= 1.0;
	short count=1;

	rf= stack;
	rf->xmin = minx*(ibuf->x);
	rf->xmax = maxx*(ibuf->x);
	rf->ymin = miny*(ibuf->y);
	rf->ymax = maxy*(ibuf->y);

	texr.talpha= texres->talpha;	/* is read by boxsample_clip */
	
	if (imapextend) {
		CLAMP(rf->xmin, 0.0f, ibuf->x-1);
		CLAMP(rf->xmax, 0.0f, ibuf->x-1);
	}
	else if (imaprepeat)
		clipx_rctf_swap(stack, &count, 0.0, (float)(ibuf->x));
	else {
		alphaclip= clipx_rctf(rf, 0.0, (float)(ibuf->x));

		if (alphaclip<=0.0f) {
			texres->tr= texres->tb= texres->tg= texres->ta= 0.0;
			return;
		}
	}

	if (imapextend) {
		CLAMP(rf->ymin, 0.0f, ibuf->y-1);
		CLAMP(rf->ymax, 0.0f, ibuf->y-1);
	}
	else if (imaprepeat)
		clipy_rctf_swap(stack, &count, 0.0, (float)(ibuf->y));
	else {
		alphaclip*= clipy_rctf(rf, 0.0, (float)(ibuf->y));

		if (alphaclip<=0.0f) {
			texres->tr= texres->tb= texres->tg= texres->ta= 0.0;
			return;
		}
	}

	if (count>1) {
		tot= texres->tr= texres->tb= texres->tg= texres->ta= 0.0;
		while (count--) {
			boxsampleclip(ibuf, rf, &texr);
			
			opp= square_rctf(rf);
			tot+= opp;

			texres->tr+= opp*texr.tr;
			texres->tg+= opp*texr.tg;
			texres->tb+= opp*texr.tb;
			if (texres->talpha) texres->ta+= opp*texr.ta;
			rf++;
		}
		if (tot!= 0.0f) {
			texres->tr/= tot;
			texres->tg/= tot;
			texres->tb/= tot;
			if (texres->talpha) texres->ta/= tot;
		}
	}
	else
		boxsampleclip(ibuf, rf, texres);

	if (texres->talpha==0) texres->ta= 1.0;
	
	if (alphaclip!=1.0f) {
		/* premul it all */
		texres->tr*= alphaclip;
		texres->tg*= alphaclip;
		texres->tb*= alphaclip;
		texres->ta*= alphaclip;
	}
}	

//-----------------------------------------------------------------------------------------------------------------
// from here, some functions only used for the new filtering

// anisotropic filters, data struct used instead of long line of (possibly unused) func args
typedef struct afdata_t {
	float dxt[2], dyt[2];
	int intpol, extflag;
	// feline only
	float majrad, minrad, theta;
	int iProbes;
	float dusc, dvsc;
} afdata_t;

// this only used here to make it easier to pass extend flags as single int
enum {TXC_XMIR=1, TXC_YMIR, TXC_REPT, TXC_EXTD};

// similar to ibuf_get_color() but clips/wraps coords according to repeat/extend flags
// returns true if out of range in clipmode
static int ibuf_get_color_clip(float col[4], ImBuf *ibuf, int x, int y, int extflag)
{
	int clip = 0;
	switch (extflag) {
		case TXC_XMIR:	// y rep
			x %= 2*ibuf->x;
			x += x < 0 ? 2*ibuf->x : 0;
			x = x >= ibuf->x ? 2*ibuf->x - x - 1 : x;
			y %= ibuf->y;
			y += y < 0 ? ibuf->y : 0;
			break;
		case TXC_YMIR:	// x rep
			x %= ibuf->x;
			x += x < 0 ? ibuf->x : 0;
			y %= 2*ibuf->y;
			y += y < 0 ? 2*ibuf->y : 0;
			y = y >= ibuf->y ? 2*ibuf->y - y - 1 : y;
			break;
		case TXC_EXTD:
			x = (x < 0) ? 0 : ((x >= ibuf->x) ? (ibuf->x - 1) : x);
			y = (y < 0) ? 0 : ((y >= ibuf->y) ? (ibuf->y - 1) : y);
			break;
		case TXC_REPT:
			x %= ibuf->x;
			x += (x < 0) ? ibuf->x : 0;
			y %= ibuf->y;
			y += (y < 0) ? ibuf->y : 0;
			break;
		default:	{	// as extend, if clipped, set alpha to 0.0
			if (x < 0) { x = 0;  } // TXF alpha: clip = 1; }
			if (x >= ibuf->x) { x = ibuf->x - 1; } // TXF alpha:  clip = 1; }
			if (y < 0) { y = 0; } // TXF alpha:  clip = 1; }
			if (y >= ibuf->y) { y = ibuf->y - 1; } // TXF alpha:  clip = 1; }
		}
	}

	if (ibuf->rect_float) {
		const float* fp = ibuf->rect_float + (x + y*ibuf->x)*ibuf->channels;
		if (ibuf->channels == 1)
			col[0] = col[1] = col[2] = col[3] = *fp;
		else {
			col[0] = fp[0];
			col[1] = fp[1];
			col[2] = fp[2];
			col[3] = clip ? 0.f : (ibuf->channels == 4 ? fp[3] : 1.f);
		}
	}
	else {
		char* rect = (char*)(ibuf->rect + x + y*ibuf->x);
		col[0] = rect[0]*(1.f/255.f);
		col[1] = rect[1]*(1.f/255.f);
		col[2] = rect[2]*(1.f/255.f);
		col[3] = clip ? 0.f : rect[3]*(1.f/255.f);
	}
	return clip;
}

// as above + bilerp
static int ibuf_get_color_clip_bilerp(float col[4], ImBuf *ibuf, float u, float v, int intpol, int extflag)
{
	if (intpol) {
		float c00[4], c01[4], c10[4], c11[4];
		const float ufl = floorf(u -= 0.5f), vfl = floorf(v -= 0.5f);
		const float uf = u - ufl, vf = v - vfl;
		const float w00=(1.f-uf)*(1.f-vf), w10=uf*(1.f-vf), w01=(1.f-uf)*vf, w11=uf*vf;
		const int x1 = (int)ufl, y1 = (int)vfl, x2 = x1 + 1, y2 = y1 + 1;
		int clip = ibuf_get_color_clip(c00, ibuf, x1, y1, extflag);
		clip |= ibuf_get_color_clip(c10, ibuf, x2, y1, extflag);
		clip |= ibuf_get_color_clip(c01, ibuf, x1, y2, extflag);
		clip |= ibuf_get_color_clip(c11, ibuf, x2, y2, extflag);
		col[0] = w00*c00[0] + w10*c10[0] + w01*c01[0] + w11*c11[0];
		col[1] = w00*c00[1] + w10*c10[1] + w01*c01[1] + w11*c11[1];
		col[2] = w00*c00[2] + w10*c10[2] + w01*c01[2] + w11*c11[2];
		col[3] = clip ? 0.f : w00*c00[3] + w10*c10[3] + w01*c01[3] + w11*c11[3];
		return clip;
	}
	return ibuf_get_color_clip(col, ibuf, (int)u, (int)v, extflag);
}

static void area_sample(TexResult* texr, ImBuf* ibuf, float fx, float fy, afdata_t* AFD)
{
	int xs, ys, clip = 0;
	float tc[4], xsd, ysd, cw = 0.f;
	const float ux = ibuf->x*AFD->dxt[0], uy = ibuf->y*AFD->dxt[1];
	const float vx = ibuf->x*AFD->dyt[0], vy = ibuf->y*AFD->dyt[1];
	int xsam = (int)(0.5f*sqrtf(ux*ux + uy*uy) + 0.5f);
	int ysam = (int)(0.5f*sqrtf(vx*vx + vy*vy) + 0.5f);
	const int minsam = AFD->intpol ? 2 : 4;
	xsam = CLAMPIS(xsam, minsam, ibuf->x*2);
	ysam = CLAMPIS(ysam, minsam, ibuf->y*2);
	xsd = 1.f / xsam;
	ysd = 1.f / ysam;
	texr->tr = texr->tg = texr->tb = texr->ta = 0.f;
	for (ys=0; ys<ysam; ++ys) {
		for (xs=0; xs<xsam; ++xs) {
			const float su = (xs + ((ys & 1) + 0.5f)*0.5f)*xsd - 0.5f;
			const float sv = (ys + ((xs & 1) + 0.5f)*0.5f)*ysd - 0.5f;
			const float pu = fx + su*AFD->dxt[0] + sv*AFD->dyt[0];
			const float pv = fy + su*AFD->dxt[1] + sv*AFD->dyt[1];
			const int out = ibuf_get_color_clip_bilerp(tc, ibuf, pu*ibuf->x, pv*ibuf->y, AFD->intpol, AFD->extflag);
			clip |= out;
			cw += out ? 0.f : 1.f;
			texr->tr += tc[0];
			texr->tg += tc[1];
			texr->tb += tc[2];
			texr->ta += texr->talpha ? tc[3] : 0.f;
		}
	}
	xsd *= ysd;
	texr->tr *= xsd;
	texr->tg *= xsd;
	texr->tb *= xsd;
	// clipping can be ignored if alpha used, texr->ta already includes filtered edge
	texr->ta = texr->talpha ? texr->ta*xsd : (clip ? cw*xsd : 1.f);
}

// table of (exp(ar) - exp(a)) / (1 - exp(a)) for r in range [0, 1] and a = -2
// used instead of actual gaussian, otherwise at high texture magnifications circular artifacts are visible
#define EWA_MAXIDX 255
static float EWA_WTS[EWA_MAXIDX + 1] =
{ 1.f, 0.990965f, 0.982f, 0.973105f, 0.96428f, 0.955524f, 0.946836f, 0.938216f, 0.929664f,
 0.921178f, 0.912759f, 0.904405f, 0.896117f, 0.887893f, 0.879734f, 0.871638f, 0.863605f,
 0.855636f, 0.847728f, 0.839883f, 0.832098f, 0.824375f, 0.816712f, 0.809108f, 0.801564f,
 0.794079f, 0.786653f, 0.779284f, 0.771974f, 0.76472f, 0.757523f, 0.750382f, 0.743297f,
 0.736267f, 0.729292f, 0.722372f, 0.715505f, 0.708693f, 0.701933f, 0.695227f, 0.688572f,
 0.68197f, 0.67542f, 0.66892f, 0.662471f, 0.656073f, 0.649725f, 0.643426f, 0.637176f,
 0.630976f, 0.624824f, 0.618719f, 0.612663f, 0.606654f, 0.600691f, 0.594776f, 0.588906f,
 0.583083f, 0.577305f, 0.571572f, 0.565883f, 0.56024f, 0.55464f, 0.549084f, 0.543572f,
 0.538102f, 0.532676f, 0.527291f, 0.521949f, 0.516649f, 0.511389f, 0.506171f, 0.500994f,
 0.495857f, 0.490761f, 0.485704f, 0.480687f, 0.475709f, 0.470769f, 0.465869f, 0.461006f,
 0.456182f, 0.451395f, 0.446646f, 0.441934f, 0.437258f, 0.432619f, 0.428017f, 0.42345f,
 0.418919f, 0.414424f, 0.409963f, 0.405538f, 0.401147f, 0.39679f, 0.392467f, 0.388178f,
 0.383923f, 0.379701f, 0.375511f, 0.371355f, 0.367231f, 0.363139f, 0.359079f, 0.355051f,
 0.351055f, 0.347089f, 0.343155f, 0.339251f, 0.335378f, 0.331535f, 0.327722f, 0.323939f,
 0.320186f, 0.316461f, 0.312766f, 0.3091f, 0.305462f, 0.301853f, 0.298272f, 0.294719f,
 0.291194f, 0.287696f, 0.284226f, 0.280782f, 0.277366f, 0.273976f, 0.270613f, 0.267276f,
 0.263965f, 0.26068f, 0.257421f, 0.254187f, 0.250979f, 0.247795f, 0.244636f, 0.241502f,
 0.238393f, 0.235308f, 0.232246f, 0.229209f, 0.226196f, 0.223206f, 0.220239f, 0.217296f,
 0.214375f, 0.211478f, 0.208603f, 0.20575f, 0.20292f, 0.200112f, 0.197326f, 0.194562f,
 0.191819f, 0.189097f, 0.186397f, 0.183718f, 0.18106f, 0.178423f, 0.175806f, 0.17321f,
 0.170634f, 0.168078f, 0.165542f, 0.163026f, 0.16053f, 0.158053f, 0.155595f, 0.153157f,
 0.150738f, 0.148337f, 0.145955f, 0.143592f, 0.141248f, 0.138921f, 0.136613f, 0.134323f,
 0.132051f, 0.129797f, 0.12756f, 0.125341f, 0.123139f, 0.120954f, 0.118786f, 0.116635f,
 0.114501f, 0.112384f, 0.110283f, 0.108199f, 0.106131f, 0.104079f, 0.102043f, 0.100023f,
 0.0980186f, 0.09603f, 0.094057f, 0.0920994f, 0.0901571f, 0.08823f, 0.0863179f, 0.0844208f,
 0.0825384f, 0.0806708f, 0.0788178f, 0.0769792f, 0.0751551f, 0.0733451f, 0.0715493f, 0.0697676f,
 0.0679997f, 0.0662457f, 0.0645054f, 0.0627786f, 0.0610654f, 0.0593655f, 0.0576789f, 0.0560055f,
 0.0543452f, 0.0526979f, 0.0510634f, 0.0494416f, 0.0478326f, 0.0462361f, 0.0446521f, 0.0430805f,
 0.0415211f, 0.039974f, 0.0384389f, 0.0369158f, 0.0354046f, 0.0339052f, 0.0324175f, 0.0309415f,
 0.029477f, 0.0280239f, 0.0265822f, 0.0251517f, 0.0237324f, 0.0223242f, 0.020927f, 0.0195408f,
 0.0181653f, 0.0168006f, 0.0154466f, 0.0141031f, 0.0127701f, 0.0114476f, 0.0101354f, 0.00883339f,
 0.00754159f, 0.00625989f, 0.00498819f, 0.00372644f, 0.00247454f, 0.00123242f, 0.f
};

// test if a float value is 'nan'
// there is a C99 function for this: isnan(), but blender seems to use C90 (according to gcc warns),
// and may not be supported by other compilers either
#ifndef ISNAN
#define ISNAN(x) ((x) != (x))
#endif
//static int ISNAN(float x) { return (x != x); }

static void radangle2imp(float a2, float b2, float th, float* A, float* B, float* C, float* F)
{
	float ct2 = cosf(th);
	const float st2 = 1.f - ct2*ct2;	// <- sin(th)^2
	ct2 *= ct2;
	*A = a2*st2 + b2*ct2;
	*B = (b2 - a2)*sinf(2.f*th);
	*C = a2*ct2 + b2*st2;
	*F = a2*b2;
}

// all tests here are done to make sure possible overflows are hopefully minimized
static void imp2radangle(float A, float B, float C, float F, float* a, float* b, float* th, float* ecc)
{
	if (F <= 1e-5f) {	// use arbitrary major radius, zero minor, infinite eccentricity
		*a = sqrtf(A > C ? A : C);
		*b = 0.f;
		*ecc = 1e10f;
		*th = 0.5f*(atan2f(B, A - C) + (float)M_PI);
	}
	else {
		const float AmC = A - C, ApC = A + C, F2 = F*2.f;
		const float r = sqrtf(AmC*AmC + B*B);
		float d = ApC - r;
		*a = (d <= 0.f) ? sqrtf(A > C ? A : C) : sqrtf(F2 / d);
		d = ApC + r;
		if (d <= 0.f) {
			*b = 0.f;
			*ecc = 1e10f;
		}
		else {
			*b = sqrtf(F2 / d);
			*ecc = *a / *b;
		}
		// incr theta by 0.5*pi (angle of major axis)
		*th = 0.5f*(atan2f(B, AmC) + (float)M_PI);
	}
}

static void ewa_eval(TexResult* texr, ImBuf* ibuf, float fx, float fy, afdata_t* AFD)
{
	// scaling dxt/dyt by full resolution can cause overflow because of huge A/B/C and esp. F values,
	// scaling by aspect ratio alone does the opposite, so try something in between instead...
	const float ff2 = ibuf->x, ff = sqrtf(ff2), q = ibuf->y / ff;
	const float Ux = AFD->dxt[0]*ff, Vx = AFD->dxt[1]*q, Uy = AFD->dyt[0]*ff, Vy = AFD->dyt[1]*q;
	float A = Vx*Vx + Vy*Vy;
	float B = -2.f*(Ux*Vx + Uy*Vy);
	float C = Ux*Ux + Uy*Uy;
	float F = A*C - B*B*0.25f;
	float a, b, th, ecc, a2, b2, ue, ve, U0, V0, DDQ, U, ac1, ac2, BU, d; // TXF alpha: cw = 0.f;
	int u, v, u1, u2, v1, v2; // TXF alpha: clip = 0;

	// The so-called 'high' quality ewa method simply adds a constant of 1 to both A & C,
	// so the ellipse always covers at least some texels. But since the filter is now always larger,
	// it also means that everywhere else it's also more blurry then ideally should be the case.
	// So instead here the ellipse radii are modified instead whenever either is too low.
	// Use a different radius based on interpolation switch, just enough to anti-alias when interpolation is off,
	// and slightly larger to make result a bit smoother than bilinear interpolation when interpolation is on
	// (minimum values: const float rmin = intpol ? 1.f : 0.5f;)
	const float rmin = (AFD->intpol ? 1.5625f : 0.765625f)/ff2;
	imp2radangle(A, B, C, F, &a, &b, &th, &ecc);
	if ((b2 = b*b) < rmin) {
		if ((a2 = a*a) < rmin) {
			B = 0.f;
			A = C = rmin;
			F = A*C;
		}
		else {
			b2 = rmin;
			radangle2imp(a2, b2, th, &A, &B, &C, &F);
		}
	}

	ue = ff*sqrtf(C);
	ve = ff*sqrtf(A);
	d = (float)(EWA_MAXIDX + 1) / (F*ff2);
	A *= d;
	B *= d;
	C *= d;

	U0 = fx*ibuf->x;
	V0 = fy*ibuf->y;
	u1 = (int)(floorf(U0 - ue));
	u2 = (int)(ceilf(U0 + ue));
	v1 = (int)(floorf(V0 - ve));
	v2 = (int)(ceilf(V0 + ve));
	U0 -= 0.5f;
	V0 -= 0.5f;
	DDQ = 2.f*A;
	U = u1 - U0;
	ac1 = A*(2.f*U + 1.f);
	ac2 = A*U*U;
	BU = B*U;

	d = texr->tr = texr->tb = texr->tg = texr->ta = 0.f;
	for (v=v1; v<=v2; ++v) {
		const float V = v - V0;
		float DQ = ac1 + B*V;
		float Q = (C*V + BU)*V + ac2;
		for (u=u1; u<=u2; ++u) {
			if (Q < (float)(EWA_MAXIDX + 1)) {
				float tc[4];
				const float wt = EWA_WTS[(Q < 0.f) ? 0 : (unsigned int)Q];
				/*const int out =*/ ibuf_get_color_clip(tc, ibuf, u, v, AFD->extflag);
				// TXF alpha: clip |= out;
				// TXF alpha: cw += out ? 0.f : wt;
				texr->tr += tc[0]*wt;
				texr->tg += tc[1]*wt;
				texr->tb += tc[2]*wt;
				texr->ta += texr->talpha ? tc[3]*wt : 0.f;
				d += wt;
			}
			Q += DQ;
			DQ += DDQ;
		}
	}

	// d should hopefully never be zero anymore
	d = 1.f/d;
	texr->tr *= d;
	texr->tg *= d;
	texr->tb *= d;
	// clipping can be ignored if alpha used, texr->ta already includes filtered edge
	texr->ta = texr->talpha ? texr->ta*d : 1.f; // TXF alpha (clip ? cw*d : 1.f);
}

static void feline_eval(TexResult* texr, ImBuf* ibuf, float fx, float fy, afdata_t* AFD)
{
	const int maxn = AFD->iProbes - 1;
	const float ll = ((AFD->majrad == AFD->minrad) ? 2.f*AFD->majrad : 2.f*(AFD->majrad - AFD->minrad)) / (maxn ? (float)maxn : 1.f);
	float du = maxn ? cosf(AFD->theta)*ll : 0.f;
	float dv = maxn ? sinf(AFD->theta)*ll : 0.f;
	//const float D = -0.5f*(du*du + dv*dv) / (AFD->majrad*AFD->majrad);
	const float D = (EWA_MAXIDX + 1)*0.25f*(du*du + dv*dv) / (AFD->majrad*AFD->majrad);
	float d; // TXF alpha: cw = 0.f;
	int n; // TXF alpha: clip = 0;
	// have to use same scaling for du/dv here as for Ux/Vx/Uy/Vy (*after* D calc.)
	du *= AFD->dusc;
	dv *= AFD->dvsc;
	d = texr->tr = texr->tb = texr->tg = texr->ta = 0.f;
	for (n=-maxn; n<=maxn; n+=2) {
		float tc[4];
		const float hn = n*0.5f;
		const float u = fx + hn*du, v = fy + hn*dv;
		//const float wt = expf(n*n*D);
		// can use ewa table here too
		const float wt = EWA_WTS[(int)(n*n*D)];
		/*const int out =*/ ibuf_get_color_clip_bilerp(tc, ibuf, ibuf->x*u, ibuf->y*v, AFD->intpol, AFD->extflag);
		// TXF alpha: clip |= out;
		// TXF alpha: cw += out ? 0.f : wt;
		texr->tr += tc[0]*wt;
		texr->tg += tc[1]*wt;
		texr->tb += tc[2]*wt;
		texr->ta += texr->talpha ? tc[3]*wt : 0.f;
		d += wt;
	}

	d = 1.f/d;
	texr->tr *= d;
	texr->tg *= d;
	texr->tb *= d;
	// clipping can be ignored if alpha used, texr->ta already includes filtered edge
	texr->ta = texr->talpha ? texr->ta*d : 1.f; // TXF alpha: (clip ? cw*d : 1.f);
}
#undef EWA_MAXIDX

static void alpha_clip_aniso(ImBuf *ibuf, float minx, float miny, float maxx, float maxy, int extflag, TexResult *texres)
{
	float alphaclip;
	rctf rf;

	// TXF apha: we're doing the same alphaclip here as boxsample, but i'm doubting
	// if this is actually correct for the all the filtering algorithms ..

	if (!(extflag == TXC_REPT || extflag == TXC_EXTD)) {
		rf.xmin = minx*(ibuf->x);
		rf.xmax = maxx*(ibuf->x);
		rf.ymin = miny*(ibuf->y);
		rf.ymax = maxy*(ibuf->y);

		alphaclip = clipx_rctf(&rf, 0.0, (float)(ibuf->x));
		alphaclip*= clipy_rctf(&rf, 0.0, (float)(ibuf->y));
		alphaclip= MAX2(alphaclip, 0.0f);

		if (alphaclip!=1.0f) {
			/* premul it all */
			texres->tr*= alphaclip;
			texres->tg*= alphaclip;
			texres->tb*= alphaclip;
			texres->ta*= alphaclip;
		}
	}
}

static void image_mipmap_test(Tex *tex, ImBuf *ibuf)
{
	if (tex->imaflag & TEX_MIPMAP) {
		if ((ibuf->flags & IB_fields) == 0) {
			
			if (ibuf->mipmap[0] && (ibuf->userflags & IB_MIPMAP_INVALID)) {
				BLI_lock_thread(LOCK_IMAGE);
				if (ibuf->userflags & IB_MIPMAP_INVALID) {
					IMB_remakemipmap(ibuf, tex->imaflag & TEX_GAUSS_MIP);
					ibuf->userflags &= ~IB_MIPMAP_INVALID;
				}				
				BLI_unlock_thread(LOCK_IMAGE);
			}
			if (ibuf->mipmap[0] == NULL) {
				BLI_lock_thread(LOCK_IMAGE);
				if (ibuf->mipmap[0] == NULL) 
					IMB_makemipmap(ibuf, tex->imaflag & TEX_GAUSS_MIP);
				BLI_unlock_thread(LOCK_IMAGE);
			}
		}
	}
	
}

static int imagewraposa_aniso(Tex *tex, Image *ima, ImBuf *ibuf, const float texvec[3], float dxt[3], float dyt[3], TexResult *texres)
{
	TexResult texr;
	float fx, fy, minx, maxx, miny, maxy;
	float maxd, val1, val2, val3;
	int curmap, retval, intpol, extflag = 0;
	afdata_t AFD;

	void (*filterfunc)(TexResult*, ImBuf*, float, float, afdata_t*);
	switch (tex->texfilter) {
		case TXF_EWA:
			filterfunc = ewa_eval;
			break;
		case TXF_FELINE:
			filterfunc = feline_eval;
			break;
		case TXF_AREA:
		default:
			filterfunc = area_sample;
	}

	texres->tin = texres->ta = texres->tr = texres->tg = texres->tb = 0.f;

	// we need to set retval OK, otherwise texture code generates normals itself...
	retval = texres->nor ? 3 : 1;

	// quick tests
	if (ibuf==NULL && ima==NULL) return retval;

	if (ima) {	// hack for icon render
		if ((ima->ibufs.first == NULL) && (R.r.scemode & R_NO_IMAGE_LOAD)) return retval;
		ibuf = BKE_image_get_ibuf(ima, &tex->iuser); 
	}

	if ((ibuf == NULL) || ((ibuf->rect == NULL) && (ibuf->rect_float == NULL))) return retval;

	/* mipmap test */
	image_mipmap_test(tex, ibuf);
	
	if ((tex->imaflag & TEX_USEALPHA) && ((tex->imaflag & TEX_CALCALPHA) == 0)) texres->talpha = 1;
	texr.talpha = texres->talpha;

	if (tex->imaflag & TEX_IMAROT) {
		fy = texvec[0];
		fx = texvec[1];
	}
	else {
		fx = texvec[0];
		fy = texvec[1];
	}

	if (ibuf->flags & IB_fields) {
		if (R.r.mode & R_FIELDS) {			/* field render */
			if (R.flag & R_SEC_FIELD) {		/* correction for 2nd field */
				/* fac1= 0.5/( (float)ibuf->y ); */
				/* fy-= fac1; */
			}
			else 	/* first field */
				fy += 0.5f/( (float)ibuf->y );
		}
	}

	// pixel coordinates
	minx = MIN3(dxt[0], dyt[0], dxt[0] + dyt[0]);
	maxx = MAX3(dxt[0], dyt[0], dxt[0] + dyt[0]);
	miny = MIN3(dxt[1], dyt[1], dxt[1] + dyt[1]);
	maxy = MAX3(dxt[1], dyt[1], dxt[1] + dyt[1]);

	// tex_sharper has been removed
	minx = (maxx - minx)*0.5f;
	miny = (maxy - miny)*0.5f;

	if (tex->imaflag & TEX_FILTER_MIN) {
		// make sure the filtersize is minimal in pixels (normal, ref map can have miniature pixel dx/dy)
		const float addval = (0.5f * tex->filtersize) / (float)MIN2(ibuf->x, ibuf->y);
		if (addval > minx) minx = addval;
		if (addval > miny) miny = addval;
	}
	else if (tex->filtersize != 1.f) {
		minx *= tex->filtersize;
		miny *= tex->filtersize;
		dxt[0] *= tex->filtersize;
		dxt[1] *= tex->filtersize;
		dyt[0] *= tex->filtersize;
		dyt[1] *= tex->filtersize;
	}

	if (tex->imaflag & TEX_IMAROT) {
		float t;
		SWAP(float, minx, miny);
		// must rotate dxt/dyt 90 deg
		// yet another blender problem is that swapping X/Y axes (or any tex proj switches) should do something similar,
		// but it doesn't, it only swaps coords, so filter area will be incorrect in those cases.
		t = dxt[0];
		dxt[0] = dxt[1];
		dxt[1] = -t;
		t = dyt[0];
		dyt[0] = dyt[1];
		dyt[1] = -t;
	}

	// side faces of unit-cube
	minx = (minx > 0.25f) ? 0.25f : ((minx < 1e-5f) ? 1e-5f : minx);
	miny = (miny > 0.25f) ? 0.25f : ((miny < 1e-5f) ? 1e-5f : miny);

	// repeat and clip

	if (tex->extend == TEX_REPEAT) {
		if ((tex->flag & (TEX_REPEAT_XMIR | TEX_REPEAT_YMIR)) == (TEX_REPEAT_XMIR | TEX_REPEAT_YMIR))
			extflag = TXC_EXTD;
		else if (tex->flag & TEX_REPEAT_XMIR)
			extflag = TXC_XMIR;
		else if (tex->flag & TEX_REPEAT_YMIR)
			extflag = TXC_YMIR;
		else
			extflag = TXC_REPT;
	}
	else if (tex->extend == TEX_EXTEND)
		extflag = TXC_EXTD;

	if (tex->extend == TEX_CHECKER) {
		int xs = (int)floorf(fx), ys = (int)floorf(fy);
		// both checkers available, no boundary exceptions, checkerdist will eat aliasing
		if ((tex->flag & TEX_CHECKER_ODD) && (tex->flag & TEX_CHECKER_EVEN)) {
			fx -= xs;
			fy -= ys;
		}
		else {
			int xs1 = (int)floorf(fx - minx);
			int ys1 = (int)floorf(fy - miny);
			int xs2 = (int)floorf(fx + minx);
			int ys2 = (int)floorf(fy + miny);
			if ((xs1 != xs2) || (ys1 != ys2)) {
				if (tex->flag & TEX_CHECKER_ODD) {
					fx -= ((xs1 + ys) & 1) ? xs2 : xs1;
					fy -= ((ys1 + xs) & 1) ? ys2 : ys1;
				}
				if (tex->flag & TEX_CHECKER_EVEN) {
					fx -= ((xs1 + ys) & 1) ? xs1 : xs2;
					fy -= ((ys1 + xs) & 1) ? ys1 : ys2;
				}
			}
			else {
				if ((tex->flag & TEX_CHECKER_ODD) == 0 && ((xs + ys) & 1) == 0) return retval;
				if ((tex->flag & TEX_CHECKER_EVEN) == 0 && (xs + ys) & 1) return retval;
				fx -= xs;
				fy -= ys;
			}
		}
		// scale around center, (0.5, 0.5)
		if (tex->checkerdist < 1.f) {
			const float omcd = 1.f / (1.f - tex->checkerdist);
			fx = (fx - 0.5f)*omcd + 0.5f;
			fy = (fy - 0.5f)*omcd + 0.5f;
			minx *= omcd;
			miny *= omcd;
		}
	}

	if (tex->extend == TEX_CLIPCUBE) {
		if ((fx + minx) < 0.f || (fy + miny) < 0.f || (fx - minx) > 1.f || (fy - miny) > 1.f || texvec[2] < -1.f || texvec[2] > 1.f) return retval;
	}
	else if (tex->extend == TEX_CLIP || tex->extend == TEX_CHECKER) {
		if ((fx + minx) < 0.f || (fy + miny) < 0.f || (fx - minx) > 1.f || (fy - miny) > 1.f) return retval;
	}
	else {
		if (tex->extend == TEX_EXTEND) {
			fx = (fx > 1.f) ? 1.f : ((fx < 0.f) ? 0.f : fx);
			fy = (fy > 1.f) ? 1.f : ((fy < 0.f) ? 0.f : fy);
		}
		else {
			fx -= floorf(fx);
			fy -= floorf(fy);
		}
	}

	intpol = tex->imaflag & TEX_INTERPOL;

	// warning no return!
	if ((R.flag & R_SEC_FIELD) && (ibuf->flags & IB_fields))
		ibuf->rect += ibuf->x*ibuf->y;

	// struct common data
	copy_v2_v2(AFD.dxt, dxt);
	copy_v2_v2(AFD.dyt, dyt);
	AFD.intpol = intpol;
	AFD.extflag = extflag;

	// brecht: added stupid clamping here, large dx/dy can give very large
	// filter sizes which take ages to render, it may be better to do this
	// more intelligently later in the code .. probably it's not noticeable
	if (AFD.dxt[0]*AFD.dxt[0] + AFD.dxt[1]*AFD.dxt[1] > 2.0f*2.0f)
		mul_v2_fl(AFD.dxt, 2.0f/len_v2(AFD.dxt));
	if (AFD.dyt[0]*AFD.dyt[0] + AFD.dyt[1]*AFD.dyt[1] > 2.0f*2.0f)
		mul_v2_fl(AFD.dyt, 2.0f/len_v2(AFD.dyt));

	// choice:
	if (tex->imaflag & TEX_MIPMAP) {
		ImBuf *previbuf, *curibuf;
		float levf;
		int maxlev;
		ImBuf* mipmaps[IB_MIPMAP_LEVELS + 1];

		// modify ellipse minor axis if too eccentric, use for area sampling as well
		// scaling dxt/dyt as done in pbrt is not the same
		// (as in ewa_eval(), scale by sqrt(ibuf->x) to maximize precision)
		const float ff = sqrtf(ibuf->x), q = ibuf->y/ff;
		const float Ux = dxt[0]*ff, Vx = dxt[1]*q, Uy = dyt[0]*ff, Vy = dyt[1]*q;
		const float A = Vx*Vx + Vy*Vy;
		const float B = -2.f*(Ux*Vx + Uy*Vy);
		const float C = Ux*Ux + Uy*Uy;
		const float F = A*C - B*B*0.25f;
		float a, b, th, ecc;
		imp2radangle(A, B, C, F, &a, &b, &th, &ecc);
		if (tex->texfilter == TXF_FELINE) {
			float fProbes;
			a *= ff;
			b *= ff;
			a = MAX2(a, 1.f);
			b = MAX2(b, 1.f);
			fProbes = 2.f*(a / b) - 1.f;
			AFD.iProbes = (int)floorf(fProbes + 0.5f);
			AFD.iProbes = MIN2(AFD.iProbes, tex->afmax);
			if (AFD.iProbes < fProbes)
				b = 2.f*a / (float)(AFD.iProbes + 1);
			AFD.majrad = a/ff;
			AFD.minrad = b/ff;
			AFD.theta = th;
			AFD.dusc = 1.f/ff;
			AFD.dvsc = ff / (float)ibuf->y;
		}
		else {	// EWA & area
			if (ecc > (float)tex->afmax) b = a / (float)tex->afmax;
			b *= ff;
		}
		maxd = MAX2(b, 1e-8f);
		levf = ((float)M_LOG2E)*logf(maxd);

		curmap = 0;
		maxlev = 1;
		mipmaps[0] = ibuf;
		while (curmap < IB_MIPMAP_LEVELS) {
			mipmaps[curmap + 1] = ibuf->mipmap[curmap];
			if (ibuf->mipmap[curmap]) maxlev++;
			curmap++;
		}

		// mipmap level
		if (levf < 0.f) {	// original image only
			previbuf = curibuf = mipmaps[0];
			levf = 0.f;
		}
		else if (levf >= maxlev - 1) {
			previbuf = curibuf = mipmaps[maxlev - 1];
			levf = 0.f;
			if (tex->texfilter == TXF_FELINE) AFD.iProbes = 1;
		}
		else {
			const int lev = ISNAN(levf) ? 0 : (int)levf;
			curibuf = mipmaps[lev];
			previbuf = mipmaps[lev + 1];
			levf -= floorf(levf);
		}

		// filter functions take care of interpolation themselves, no need to modify dxt/dyt here

		if (texres->nor && ((tex->imaflag & TEX_NORMALMAP) == 0)) {
			// color & normal
			filterfunc(texres, curibuf, fx, fy, &AFD);
			val1 = texres->tr + texres->tg + texres->tb;
			filterfunc(&texr, curibuf, fx + dxt[0], fy + dxt[1], &AFD);
			val2 = texr.tr + texr.tg + texr.tb;
			filterfunc(&texr, curibuf, fx + dyt[0], fy + dyt[1], &AFD);
			val3 = texr.tr + texr.tg + texr.tb;
			// don't switch x or y!
			texres->nor[0] = val1 - val2;
			texres->nor[1] = val1 - val3;
			if (previbuf != curibuf) {  // interpolate
				filterfunc(&texr, previbuf, fx, fy, &AFD);
				// rgb
				texres->tr += levf*(texr.tr - texres->tr);
				texres->tg += levf*(texr.tg - texres->tg);
				texres->tb += levf*(texr.tb - texres->tb);
				texres->ta += levf*(texr.ta - texres->ta);
				// normal
				val1 += levf*((texr.tr + texr.tg + texr.tb) - val1);
				filterfunc(&texr, previbuf, fx + dxt[0], fy + dxt[1], &AFD);
				val2 += levf*((texr.tr + texr.tg + texr.tb) - val2);
				filterfunc(&texr, previbuf, fx + dyt[0], fy + dyt[1], &AFD);
				val3 += levf*((texr.tr + texr.tg + texr.tb) - val3);
				texres->nor[0] = val1 - val2;	// vals have been interpolated above!
				texres->nor[1] = val1 - val3;
			}
		}
		else {	// color
			filterfunc(texres, curibuf, fx, fy, &AFD);
			if (previbuf != curibuf) {  // interpolate
				filterfunc(&texr, previbuf, fx, fy, &AFD);
				texres->tr += levf*(texr.tr - texres->tr);
				texres->tg += levf*(texr.tg - texres->tg);
				texres->tb += levf*(texr.tb - texres->tb);
				texres->ta += levf*(texr.ta - texres->ta);
			}

			alpha_clip_aniso(ibuf, fx-minx, fy-miny, fx+minx, fy+miny, extflag, texres);
		}
	}
	else {	// no mipmap
		// filter functions take care of interpolation themselves, no need to modify dxt/dyt here
		if (tex->texfilter == TXF_FELINE) {
			const float ff = sqrtf(ibuf->x), q = ibuf->y/ff;
			const float Ux = dxt[0]*ff, Vx = dxt[1]*q, Uy = dyt[0]*ff, Vy = dyt[1]*q;
			const float A = Vx*Vx + Vy*Vy;
			const float B = -2.f*(Ux*Vx + Uy*Vy);
			const float C = Ux*Ux + Uy*Uy;
			const float F = A*C - B*B*0.25f;
			float a, b, th, ecc, fProbes;
			imp2radangle(A, B, C, F, &a, &b, &th, &ecc);
			a *= ff;
			b *= ff;
			a = MAX2(a, 1.f);
			b = MAX2(b, 1.f);
			fProbes = 2.f*(a / b) - 1.f;
			// no limit to number of Probes here
			AFD.iProbes = (int)floorf(fProbes + 0.5f);
			if (AFD.iProbes < fProbes) b = 2.f*a / (float)(AFD.iProbes + 1);
			AFD.majrad = a/ff;
			AFD.minrad = b/ff;
			AFD.theta = th;
			AFD.dusc = 1.f/ff;
			AFD.dvsc = ff / (float)ibuf->y;
		}
		if (texres->nor && ((tex->imaflag & TEX_NORMALMAP) == 0)) {
			// color & normal
			filterfunc(texres, ibuf, fx, fy, &AFD);
			val1 = texres->tr + texres->tg + texres->tb;
			filterfunc(&texr, ibuf, fx + dxt[0], fy + dxt[1], &AFD);
			val2 = texr.tr + texr.tg + texr.tb;
			filterfunc(&texr, ibuf, fx + dyt[0], fy + dyt[1], &AFD);
			val3 = texr.tr + texr.tg + texr.tb;
			// don't switch x or y!
			texres->nor[0] = val1 - val2;
			texres->nor[1] = val1 - val3;
		}
		else {
			filterfunc(texres, ibuf, fx, fy, &AFD);
			alpha_clip_aniso(ibuf, fx-minx, fy-miny, fx+minx, fy+miny, extflag, texres);
		}
	}

	if (tex->imaflag & TEX_CALCALPHA)
		texres->ta = texres->tin = texres->ta * MAX3(texres->tr, texres->tg, texres->tb);
	else
		texres->tin = texres->ta;
	if (tex->flag & TEX_NEGALPHA) texres->ta = 1.f - texres->ta;
	
	if ((R.flag & R_SEC_FIELD) && (ibuf->flags & IB_fields))
		ibuf->rect -= ibuf->x*ibuf->y;

	if (texres->nor && (tex->imaflag & TEX_NORMALMAP)) {	// normal from color
		// The invert of the red channel is to make
		// the normal map compliant with the outside world.
		// It needs to be done because in Blender
		// the normal used in the renderer points inward. It is generated
		// this way in calc_vertexnormals(). Should this ever change
		// this negate must be removed.
		texres->nor[0] = -2.f*(texres->tr - 0.5f);
		texres->nor[1] = 2.f*(texres->tg - 0.5f);
		texres->nor[2] = 2.f*(texres->tb - 0.5f);
	}
	
	// de-premul, this is being premulled in shade_input_do_shade()
	// TXF: this currently does not (yet?) work properly, destroys edge AA in clip/checker mode, so for now commented out
	// also disabled in imagewraposa() to be able to compare results with blender's default texture filtering

	// brecht: tried to fix this, see "TXF alpha" comments

	if (texres->ta != 1.f && (texres->ta > 1e-4f)) {
		fx = 1.f/texres->ta;
		texres->tr *= fx;
		texres->tg *= fx;
		texres->tb *= fx;
	}

	BRICONTRGB;
	
	return retval;
}


int imagewraposa(Tex *tex, Image *ima, ImBuf *ibuf, const float texvec[3], const float DXT[3], const float DYT[3], TexResult *texres)
{
	TexResult texr;
	float fx, fy, minx, maxx, miny, maxy, dx, dy, dxt[3], dyt[3];
	float maxd, pixsize, val1, val2, val3;
	int curmap, retval, imaprepeat, imapextend;

	// TXF: since dxt/dyt might be modified here and since they might be needed after imagewraposa() call,
	// make a local copy here so that original vecs remain untouched
	copy_v3_v3(dxt, DXT);
	copy_v3_v3(dyt, DYT);

	// anisotropic filtering
	if (tex->texfilter != TXF_BOX)
		return imagewraposa_aniso(tex, ima, ibuf, texvec, dxt, dyt, texres);

	texres->tin= texres->ta= texres->tr= texres->tg= texres->tb= 0.0f;
	
	/* we need to set retval OK, otherwise texture code generates normals itself... */
	retval= texres->nor?3:1;
	
	/* quick tests */
	if (ibuf==NULL && ima==NULL)
		return retval;
	if (ima) {

		/* hack for icon render */
		if (ima->ibufs.first==NULL && (R.r.scemode & R_NO_IMAGE_LOAD))
			return retval;
		
		ibuf= BKE_image_get_ibuf(ima, &tex->iuser); 

		ima->flag|= IMA_USED_FOR_RENDER;
	}
	if (ibuf==NULL || (ibuf->rect==NULL && ibuf->rect_float==NULL))
		return retval;
	
	/* mipmap test */
	image_mipmap_test(tex, ibuf);

	if (tex->imaflag & TEX_USEALPHA) {
		if (tex->imaflag & TEX_CALCALPHA);
		else texres->talpha= 1;
	}
	
	texr.talpha= texres->talpha;
	
	if (tex->imaflag & TEX_IMAROT) {
		fy= texvec[0];
		fx= texvec[1];
	}
	else {
		fx= texvec[0];
		fy= texvec[1];
	}
	
	if (ibuf->flags & IB_fields) {
		if (R.r.mode & R_FIELDS) {			/* field render */
			if (R.flag & R_SEC_FIELD) {		/* correction for 2nd field */
				/* fac1= 0.5/( (float)ibuf->y ); */
				/* fy-= fac1; */
			}
			else {				/* first field */
				fy+= 0.5f/( (float)ibuf->y );
			}
		}
	}
	
	/* pixel coordinates */

	minx = MIN3(dxt[0], dyt[0], dxt[0] + dyt[0]);
	maxx = MAX3(dxt[0], dyt[0], dxt[0] + dyt[0]);
	miny = MIN3(dxt[1], dyt[1], dxt[1] + dyt[1]);
	maxy = MAX3(dxt[1], dyt[1], dxt[1] + dyt[1]);

	/* tex_sharper has been removed */
	minx= (maxx-minx)/2.0f;
	miny= (maxy-miny)/2.0f;
	
	if (tex->imaflag & TEX_FILTER_MIN) {
		/* make sure the filtersize is minimal in pixels (normal, ref map can have miniature pixel dx/dy) */
		float addval= (0.5f * tex->filtersize) / (float) MIN2(ibuf->x, ibuf->y);

		if (addval > minx)
			minx= addval;
		if (addval > miny)
			miny= addval;
	}
	else if (tex->filtersize!=1.0f) {
		minx*= tex->filtersize;
		miny*= tex->filtersize;
		
		dxt[0]*= tex->filtersize;
		dxt[1]*= tex->filtersize;
		dyt[0]*= tex->filtersize;
		dyt[1]*= tex->filtersize;
	}

	if (tex->imaflag & TEX_IMAROT) SWAP(float, minx, miny);
	
	if (minx>0.25f) minx= 0.25f;
	else if (minx<0.00001f) minx= 0.00001f;	/* side faces of unit-cube */
	if (miny>0.25f) miny= 0.25f;
	else if (miny<0.00001f) miny= 0.00001f;

	
	/* repeat and clip */
	imaprepeat= (tex->extend==TEX_REPEAT);
	imapextend= (tex->extend==TEX_EXTEND);

	if (tex->extend == TEX_REPEAT) {
		if (tex->flag & (TEX_REPEAT_XMIR|TEX_REPEAT_YMIR)) {
			imaprepeat= 0;
			imapextend= 1;
		}
	}

	if (tex->extend == TEX_CHECKER) {
		int xs, ys, xs1, ys1, xs2, ys2, boundary;
		
		xs= (int)floor(fx);
		ys= (int)floor(fy);
		
		// both checkers available, no boundary exceptions, checkerdist will eat aliasing
		if ( (tex->flag & TEX_CHECKER_ODD) && (tex->flag & TEX_CHECKER_EVEN) ) {
			fx-= xs;
			fy-= ys;
		}
		else {
			
			xs1= (int)floor(fx-minx);
			ys1= (int)floor(fy-miny);
			xs2= (int)floor(fx+minx);
			ys2= (int)floor(fy+miny);
			boundary= (xs1!=xs2) || (ys1!=ys2);

			if (boundary==0) {
				if ( (tex->flag & TEX_CHECKER_ODD)==0) {
					if ((xs+ys) & 1);
					else return retval;
				}
				if ( (tex->flag & TEX_CHECKER_EVEN)==0) {
					if ((xs+ys) & 1) return retval;
				}
				fx-= xs;
				fy-= ys;
			}
			else {
				if (tex->flag & TEX_CHECKER_ODD) {
					if ((xs1+ys) & 1) fx-= xs2;
					else fx-= xs1;
					
					if ((ys1+xs) & 1) fy-= ys2;
					else fy-= ys1;
				}
				if (tex->flag & TEX_CHECKER_EVEN) {
					if ((xs1+ys) & 1) fx-= xs1;
					else fx-= xs2;
					
					if ((ys1+xs) & 1) fy-= ys1;
					else fy-= ys2;
				}
			}
		}

		/* scale around center, (0.5, 0.5) */
		if (tex->checkerdist<1.0f) {
			fx= (fx-0.5f)/(1.0f-tex->checkerdist) +0.5f;
			fy= (fy-0.5f)/(1.0f-tex->checkerdist) +0.5f;
			minx/= (1.0f-tex->checkerdist);
			miny/= (1.0f-tex->checkerdist);
		}
	}

	if (tex->extend == TEX_CLIPCUBE) {
		if (fx+minx<0.0f || fy+miny<0.0f || fx-minx>1.0f || fy-miny>1.0f || texvec[2]<-1.0f || texvec[2]>1.0f) {
			return retval;
		}
	}
	else if (tex->extend==TEX_CLIP || tex->extend==TEX_CHECKER) {
		if (fx+minx<0.0f || fy+miny<0.0f || fx-minx>1.0f || fy-miny>1.0f) {
			return retval;
		}
	}
	else {
		if (imapextend) {
			if (fx>1.0f) fx = 1.0f;
			else if (fx<0.0f) fx= 0.0f;
		}
		else {
			if (fx>1.0f) fx -= (int)(fx);
			else if (fx<0.0f) fx+= 1-(int)(fx);
		}
		
		if (imapextend) {
			if (fy>1.0f) fy = 1.0f;
			else if (fy<0.0f) fy= 0.0f;
		}
		else {
			if (fy>1.0f) fy -= (int)(fy);
			else if (fy<0.0f) fy+= 1-(int)(fy);
		}
	}

	/* warning no return! */
	if ( (R.flag & R_SEC_FIELD) && (ibuf->flags & IB_fields) ) {
		ibuf->rect+= (ibuf->x*ibuf->y);
	}

	/* choice:  */
	if (tex->imaflag & TEX_MIPMAP) {
		ImBuf *previbuf, *curibuf;
		float bumpscale;
		
		dx= minx;
		dy= miny;
		maxd= MAX2(dx, dy);
		if (maxd>0.5f) maxd= 0.5f;

		pixsize = 1.0f/ (float) MIN2(ibuf->x, ibuf->y);
		
		bumpscale= pixsize/maxd;
		if (bumpscale>1.0f) bumpscale= 1.0f;
		else bumpscale*=bumpscale;
		
		curmap= 0;
		previbuf= curibuf= ibuf;
		while (curmap<IB_MIPMAP_LEVELS && ibuf->mipmap[curmap]) {
			if (maxd < pixsize) break;
			previbuf= curibuf;
			curibuf= ibuf->mipmap[curmap];
			pixsize= 1.0f / (float)MIN2(curibuf->x, curibuf->y);
			curmap++;
		}

		if (previbuf!=curibuf || (tex->imaflag & TEX_INTERPOL)) {
			/* sample at least 1 pixel */
			if (minx < 0.5f / ibuf->x) minx = 0.5f / ibuf->x;
			if (miny < 0.5f / ibuf->y) miny = 0.5f / ibuf->y;
		}
		
		if (texres->nor && (tex->imaflag & TEX_NORMALMAP)==0) {
			/* a bit extra filter */
			//minx*= 1.35f;
			//miny*= 1.35f;
			
			boxsample(curibuf, fx-minx, fy-miny, fx+minx, fy+miny, texres, imaprepeat, imapextend);
			val1= texres->tr+texres->tg+texres->tb;
			boxsample(curibuf, fx-minx+dxt[0], fy-miny+dxt[1], fx+minx+dxt[0], fy+miny+dxt[1], &texr, imaprepeat, imapextend);
			val2= texr.tr + texr.tg + texr.tb;
			boxsample(curibuf, fx-minx+dyt[0], fy-miny+dyt[1], fx+minx+dyt[0], fy+miny+dyt[1], &texr, imaprepeat, imapextend);
			val3= texr.tr + texr.tg + texr.tb;

			/* don't switch x or y! */
			texres->nor[0]= (val1-val2);
			texres->nor[1]= (val1-val3);
			
			if (previbuf!=curibuf) {  /* interpolate */
				
				boxsample(previbuf, fx-minx, fy-miny, fx+minx, fy+miny, &texr, imaprepeat, imapextend);
				
				/* calc rgb */
				dx= 2.0f*(pixsize-maxd)/pixsize;
				if (dx>=1.0f) {
					texres->ta= texr.ta; texres->tb= texr.tb;
					texres->tg= texr.tg; texres->tr= texr.tr;
				}
				else {
					dy= 1.0f-dx;
					texres->tb= dy*texres->tb+ dx*texr.tb;
					texres->tg= dy*texres->tg+ dx*texr.tg;
					texres->tr= dy*texres->tr+ dx*texr.tr;
					texres->ta= dy*texres->ta+ dx*texr.ta;
				}
				
				val1= dy*val1+ dx*(texr.tr + texr.tg + texr.tb);
				boxsample(previbuf, fx-minx+dxt[0], fy-miny+dxt[1], fx+minx+dxt[0], fy+miny+dxt[1], &texr, imaprepeat, imapextend);
				val2= dy*val2+ dx*(texr.tr + texr.tg + texr.tb);
				boxsample(previbuf, fx-minx+dyt[0], fy-miny+dyt[1], fx+minx+dyt[0], fy+miny+dyt[1], &texr, imaprepeat, imapextend);
				val3= dy*val3+ dx*(texr.tr + texr.tg + texr.tb);
				
				texres->nor[0]= (val1-val2);	/* vals have been interpolated above! */
				texres->nor[1]= (val1-val3);
				
				if (dx<1.0f) {
					dy= 1.0f-dx;
					texres->tb= dy*texres->tb+ dx*texr.tb;
					texres->tg= dy*texres->tg+ dx*texr.tg;
					texres->tr= dy*texres->tr+ dx*texr.tr;
					texres->ta= dy*texres->ta+ dx*texr.ta;
				}
			}
			texres->nor[0]*= bumpscale;
			texres->nor[1]*= bumpscale;
		}
		else {
			maxx= fx+minx;
			minx= fx-minx;
			maxy= fy+miny;
			miny= fy-miny;

			boxsample(curibuf, minx, miny, maxx, maxy, texres, imaprepeat, imapextend);

			if (previbuf!=curibuf) {  /* interpolate */
				boxsample(previbuf, minx, miny, maxx, maxy, &texr, imaprepeat, imapextend);
				
				fx= 2.0f*(pixsize-maxd)/pixsize;
				
				if (fx>=1.0f) {
					texres->ta= texr.ta; texres->tb= texr.tb;
					texres->tg= texr.tg; texres->tr= texr.tr;
				}
				else {
					fy= 1.0f-fx;
					texres->tb= fy*texres->tb+ fx*texr.tb;
					texres->tg= fy*texres->tg+ fx*texr.tg;
					texres->tr= fy*texres->tr+ fx*texr.tr;
					texres->ta= fy*texres->ta+ fx*texr.ta;
				}
			}
		}
	}
	else {
		const int intpol = tex->imaflag & TEX_INTERPOL;
		if (intpol) {
			/* sample 1 pixel minimum */
			if (minx < 0.5f / ibuf->x) minx = 0.5f / ibuf->x;
			if (miny < 0.5f / ibuf->y) miny = 0.5f / ibuf->y;
		}

		if (texres->nor && (tex->imaflag & TEX_NORMALMAP)==0) {
			boxsample(ibuf, fx-minx, fy-miny, fx+minx, fy+miny, texres, imaprepeat, imapextend);
			val1= texres->tr+texres->tg+texres->tb;
			boxsample(ibuf, fx-minx+dxt[0], fy-miny+dxt[1], fx+minx+dxt[0], fy+miny+dxt[1], &texr, imaprepeat, imapextend);
			val2= texr.tr + texr.tg + texr.tb;
			boxsample(ibuf, fx-minx+dyt[0], fy-miny+dyt[1], fx+minx+dyt[0], fy+miny+dyt[1], &texr, imaprepeat, imapextend);
			val3= texr.tr + texr.tg + texr.tb;

			/* don't switch x or y! */
			texres->nor[0]= (val1-val2);
			texres->nor[1]= (val1-val3);
		}
		else
			boxsample(ibuf, fx-minx, fy-miny, fx+minx, fy+miny, texres, imaprepeat, imapextend);
	}
	
	if (tex->imaflag & TEX_CALCALPHA) {
		texres->ta= texres->tin= texres->ta*MAX3(texres->tr, texres->tg, texres->tb);
	}
	else texres->tin= texres->ta;

	if (tex->flag & TEX_NEGALPHA) texres->ta= 1.0f-texres->ta;
	
	if ( (R.flag & R_SEC_FIELD) && (ibuf->flags & IB_fields) ) {
		ibuf->rect-= (ibuf->x*ibuf->y);
	}

	if (texres->nor && (tex->imaflag & TEX_NORMALMAP)) {
		// qdn: normal from color
		// The invert of the red channel is to make
		// the normal map compliant with the outside world.
		// It needs to be done because in Blender
		// the normal used in the renderer points inward. It is generated
		// this way in calc_vertexnormals(). Should this ever change
		// this negate must be removed.
		texres->nor[0] = -2.f*(texres->tr - 0.5f);
		texres->nor[1] = 2.f*(texres->tg - 0.5f);
		texres->nor[2] = 2.f*(texres->tb - 0.5f);
	}
	
	/* de-premul, this is being premulled in shade_input_do_shade() */
	if (texres->ta!=1.0f && texres->ta>1e-4f) {
		fx= 1.0f/texres->ta;
		texres->tr*= fx;
		texres->tg*= fx;
		texres->tb*= fx;
	}

	BRICONTRGB;
	
	return retval;
}

void image_sample(Image *ima, float fx, float fy, float dx, float dy, float *result)
{
	TexResult texres;
	ImBuf *ibuf= BKE_image_get_ibuf(ima, NULL);
	
	if (ibuf==NULL) {
		result[0]= result[1]= result[2]= result[3]= 0.0f;
		return;
	}
	
	if ( (R.flag & R_SEC_FIELD) && (ibuf->flags & IB_fields) )
		ibuf->rect+= (ibuf->x*ibuf->y);

	texres.talpha= 1; /* boxsample expects to be initialized */
	boxsample(ibuf, fx, fy, fx+dx, fy+dy, &texres, 0, 1);
	result[0]= texres.tr;
	result[1]= texres.tg;
	result[2]= texres.tb;
	result[3]= texres.ta;
	
	if ( (R.flag & R_SEC_FIELD) && (ibuf->flags & IB_fields) )
		ibuf->rect-= (ibuf->x*ibuf->y);

	ima->flag|= IMA_USED_FOR_RENDER;
}

void ibuf_sample(ImBuf *ibuf, float fx, float fy, float dx, float dy, float *result)
{
	TexResult texres;
	afdata_t AFD;
	
	if (ibuf==NULL) {
		return;
	}
	
	AFD.dxt[0] = dx; AFD.dxt[1] = dx;
	AFD.dyt[0] = dy; AFD.dyt[1] = dy;
	//copy_v2_v2(AFD.dxt, dx);
	//copy_v2_v2(AFD.dyt, dy);
	
	AFD.intpol = 1;
	AFD.extflag = TXC_EXTD;
	
	memset(&texres, 0, sizeof(texres));
	ewa_eval(&texres, ibuf, fx, fy, &AFD);
	
	
	result[0]= texres.tr;
	result[1]= texres.tg;
	result[2]= texres.tb;
	result[3]= texres.ta;
}
